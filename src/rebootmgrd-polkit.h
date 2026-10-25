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

#pragma once

#include <systemd/sd-varlink.h>

#include "rebootmgr.h"

/* What to do if polkit itself cannot be reached (polkitd not running,
   no system bus, out of memory, ...). */
typedef enum RMPolkitAbsentPolicy {
  RM_POLKIT_ABSENT_DENY,   /* privileged actions: fail closed */
  RM_POLKIT_ABSENT_ALLOW,  /* legacy-open actions: fail open, preserves old behaviour */
} RMPolkitAbsentPolicy;

/*
 * Checks whether the peer of "link" is authorized for "action_id".
 *
 * Peer UID 0 (root) is always authorized immediately, without talking to
 * polkit at all, preserving the daemon's historic "root can do anything"
 * behaviour unconditionally, independent of local polkit rules and even
 * if polkitd isn't running.
 *
 * Return value:
 *   1   -> authorized. The caller should proceed with the privileged
 *          operation as normal.
 *   0   -> an asynchronous check was just started, or is already
 *          outstanding. The caller must `return 0;` immediately without
 *          calling sd_varlink_reply()/sd_varlink_error(); the Varlink
 *          call will be re-dispatched (the whole method handler run
 *          again) once the check completes, via sd_varlink_dispatch_again().
 *   <0  -> the check is finished and failed (denied, interactive
 *          authentication required, or an internal error); a Varlink
 *          error reply has already been sent. The caller must
 *          `return r;` immediately.
 */
int rm_polkit_check_async(sd_varlink *link, RM_CTX *ctx, const char *action_id,
                           RMPolkitAbsentPolicy absent_policy);

/* Must be bound as the Varlink server's disconnect callback (directly via
   sd_varlink_server_bind_disconnect()). Cancels and frees any polkit check
   outstanding for the departing connection. */
void rm_polkit_on_disconnect(sd_varlink_server *server, sd_varlink *link, void *userdata);

/* Called once from destroy_context() to release ctx->polkit_bus and any
   (normally none) leftover pending query nodes. */
void rm_polkit_context_done(RM_CTX *ctx);
