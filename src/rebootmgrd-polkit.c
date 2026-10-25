//SPDX-License-Identifier: GPL-2.0-or-later

/* Copyright (c) 2026 Thorsten Kukuk
   Author: Thorsten Kukuk <kukuk@suse.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, see <http://www.gnu.org/licenses/>. */

#include "config.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-varlink.h>

#include "basics.h"
#include "common.h"
#include "rebootmgrd-polkit.h"

#define POLKIT_DESTINATION "org.freedesktop.PolicyKit1"
#define POLKIT_PATH        "/org/freedesktop/PolicyKit1/Authority"
#define POLKIT_INTERFACE   "org.freedesktop.PolicyKit1.Authority"

/* Tracks a single outstanding (or just completed, not yet consumed)
   polkit check for one Varlink connection. */
struct RMPolkitQuery {
  RM_CTX *ctx;
  sd_varlink *link;   /* ref'd for as long as this node is alive */
  sd_bus_slot *slot;  /* non-NULL while the CheckAuthorization call is in flight */
  char *action;       /* action id, kept for diagnostics only */
  bool done;
  bool authorized;
  bool challenge;      /* only meaningful if done && !authorized */
  int hard_error;       /* only meaningful if done && !authorized && !challenge;
                            -ENOENT means "polkit was unreachable" */
  RMPolkitQuery *next;
};

static RMPolkitQuery *
polkit_query_find(RM_CTX *ctx, sd_varlink *link)
{
  for (RMPolkitQuery *q = ctx->polkit_queries; q; q = q->next)
    if (q->link == link)
      return q;

  return NULL;
}

static void
polkit_query_unlink_free(RMPolkitQuery *q)
{
  RMPolkitQuery **pp = &q->ctx->polkit_queries;

  while (*pp && *pp != q)
    pp = &(*pp)->next;
  if (*pp)
    *pp = q->next;

  if (q->slot)
    q->slot = sd_bus_slot_unref(q->slot);
  if (q->link)
    q->link = sd_varlink_unref(q->link);
  q->action = mfree(q->action);
  free(q);
}

static int
ensure_polkit_bus(RM_CTX *ctx)
{
  int r;

  if (ctx->polkit_bus && sd_bus_is_open(ctx->polkit_bus) > 0)
    return 0;

  if (ctx->polkit_bus)
    ctx->polkit_bus = sd_bus_unref(ctx->polkit_bus);

  r = sd_bus_open_system(&ctx->polkit_bus);
  if (r < 0)
    return r;

  r = sd_bus_attach_event(ctx->polkit_bus, ctx->loop, SD_EVENT_PRIORITY_NORMAL);
  if (r < 0)
    {
      ctx->polkit_bus = sd_bus_unref(ctx->polkit_bus);
      return r;
    }

  return 0;
}

static bool
polkit_service_absent(const sd_bus_error *e)
{
  return sd_bus_error_has_name(e, SD_BUS_ERROR_SERVICE_UNKNOWN) ||
    sd_bus_error_has_name(e, SD_BUS_ERROR_NAME_HAS_NO_OWNER);
}

static int
rm_polkit_async_callback(sd_bus_message *reply, void *userdata, sd_bus_error _unused_(*reterr_error))
{
  RMPolkitQuery *q = userdata;
  int r;

  q->slot = sd_bus_slot_unref(q->slot);

  if (sd_bus_message_is_method_error(reply, NULL))
    {
      const sd_bus_error *e = sd_bus_message_get_error(reply);

      q->authorized = false;
      q->challenge = false;

      if (polkit_service_absent(e))
        q->hard_error = -ENOENT;
      else if (sd_bus_error_has_name(e, "org.freedesktop.PolicyKit1.Error.Failed") ||
               sd_bus_error_has_name(e, "org.freedesktop.PolicyKit1.Error.Cancelled") ||
               sd_bus_error_has_name(e, "org.freedesktop.PolicyKit1.Error.NotAuthorized"))
        q->hard_error = 0;
      else
        {
          log_msg(LOG_ERR, "polkit CheckAuthorization for '%s' failed: %s", q->action, e->message);
          q->hard_error = -EIO;
        }
    }
  else
    {
      int authorized = 0, challenge = 0;

      r = sd_bus_message_enter_container(reply, 'r', "bba{ss}");
      if (r >= 0)
        r = sd_bus_message_read(reply, "bb", &authorized, &challenge);
      if (r >= 0)
        r = sd_bus_message_skip(reply, "a{ss}");
      if (r >= 0)
        r = sd_bus_message_exit_container(reply);

      if (r < 0)
        {
          log_msg(LOG_ERR, "Failed to parse polkit reply for '%s': %s", q->action, strerror(-r));
          q->authorized = false;
          q->challenge = false;
          q->hard_error = -EIO;
        }
      else
        {
          q->authorized = authorized;
          q->challenge = !authorized && challenge;
          q->hard_error = 0;
        }
    }

  q->done = true;

  r = sd_varlink_dispatch_again(q->link);
  if (r < 0)
    log_msg(LOG_ERR, "Failed to re-dispatch Varlink call after polkit check: %s", strerror(-r));

  return 0;
}

static int
polkit_deny(sd_varlink *link, const char *action_id, uid_t peer_uid)
{
  log_msg(LOG_WARNING, "polkit denied action '%s' for uid %u", action_id, (unsigned int)peer_uid);
  return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, NULL);
}

int
rm_polkit_check_async(sd_varlink *link, RM_CTX *ctx, const char *action_id,
                       RMPolkitAbsentPolicy absent_policy)
{
  uid_t peer_uid;
  int r;

  r = sd_varlink_get_peer_uid(link, &peer_uid);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to get peer UID: %s", strerror(-r));
      return sd_varlink_error_errno(link, r);
    }

  /* Root always succeeds immediately, with zero polkit interaction. This
     keeps root working even if polkitd isn't installed or running. */
  if (peer_uid == 0)
    return 1;

  RMPolkitQuery *q = polkit_query_find(ctx, link);

  if (q)
    {
      if (!q->done)
        {
          /* sd-varlink only ever has one method call in flight per
             connection at a time, so this should be unreachable. */
          log_msg(LOG_ERR, "Internal error: polkit query for '%s' re-entered while pending", action_id);
          return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, NULL);
        }

      bool authorized = q->authorized;
      bool challenge = q->challenge;
      int hard_error = q->hard_error;

      polkit_query_unlink_free(q);

      if (authorized)
        return 1;

      if (hard_error == -ENOENT)
        {
          if (absent_policy == RM_POLKIT_ABSENT_ALLOW)
            return 1;
          log_msg(LOG_WARNING, "polkit unavailable, denying action '%s' for uid %u",
                  action_id, (unsigned int)peer_uid);
          return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, NULL);
        }
      if (hard_error < 0)
        return sd_varlink_error_errno(link, hard_error);
      if (challenge)
        return sd_varlink_error(link, SD_VARLINK_ERROR_INTERACTIVE_AUTHENTICATION_REQUIRED, NULL);

      return polkit_deny(link, action_id, peer_uid);
    }

  /* First time we see this call: issue the async CheckAuthorization request. */
  r = ensure_polkit_bus(ctx);
  if (r < 0)
    {
      if (absent_policy == RM_POLKIT_ABSENT_ALLOW)
        return 1;
      log_msg(LOG_WARNING, "Cannot reach system bus for polkit check (%s), denying action '%s'",
              strerror(-r), action_id);
      return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, NULL);
    }

  int pidfd = sd_varlink_get_peer_pidfd(link);
  if (pidfd < 0)
    return sd_varlink_error_errno(link, pidfd);

  _cleanup_(sd_bus_message_unrefp) sd_bus_message *m = NULL;
  r = sd_bus_message_new_method_call(ctx->polkit_bus, &m, POLKIT_DESTINATION, POLKIT_PATH,
                                      POLKIT_INTERFACE, "CheckAuthorization");
  if (r < 0)
    return sd_varlink_error_errno(link, r);

  r = sd_bus_message_append(m, "(sa{sv})s",
                             "unix-process", 2,
                               "pidfd", "h", (uint32_t)pidfd,
                               "uid",   "i", (int32_t)peer_uid,
                             action_id);
  if (r < 0)
    return sd_varlink_error_errno(link, r);

  r = sd_bus_message_append(m, "a{ss}", 0);
  if (r < 0)
    return sd_varlink_error_errno(link, r);

  r = sd_bus_message_append(m, "us", (uint32_t)0 /* flags: no interactive auth */, "");
  if (r < 0)
    return sd_varlink_error_errno(link, r);

  q = calloc(1, sizeof(RMPolkitQuery));
  if (!q)
    return sd_varlink_error_errno(link, -ENOMEM);
  q->ctx = ctx;
  q->link = sd_varlink_ref(link);
  q->action = strdup(action_id);

  r = sd_bus_call_async(ctx->polkit_bus, &q->slot, m, rm_polkit_async_callback, q, 0);
  if (r < 0)
    {
      sd_varlink_unref(q->link);
      free(q->action);
      free(q);
      if (absent_policy == RM_POLKIT_ABSENT_ALLOW)
        return 1;
      return sd_varlink_error_errno(link, r);
    }

  q->next = ctx->polkit_queries;
  ctx->polkit_queries = q;

  return 0;
}

void
rm_polkit_on_disconnect(sd_varlink_server _unused_(*server), sd_varlink *link, void *userdata)
{
  RM_CTX *ctx = userdata;
  RMPolkitQuery *q = polkit_query_find(ctx, link);

  if (q)
    polkit_query_unlink_free(q);
}

void
rm_polkit_context_done(RM_CTX *ctx)
{
  while (ctx->polkit_queries)
    polkit_query_unlink_free(ctx->polkit_queries);

  if (ctx->polkit_bus)
    ctx->polkit_bus = sd_bus_unref(ctx->polkit_bus);
}
