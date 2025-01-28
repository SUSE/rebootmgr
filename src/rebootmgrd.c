//SPDX-License-Identifier: GPL-2.0-or-later

/* Copyright (c) 2024, 2025 Thorsten Kukuk
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

#include <getopt.h>
#include <stdlib.h>
#include <stdbool.h>
#include <libintl.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-varlink.h>

#include "basics.h"
#include "common.h"
#include "parse-duration.h"

#include "varlink-org.openSUSE.rebootmgr.h"

static int verbose_flag = 0;

static int
vl_method_ping(sd_varlink *link, sd_json_variant *parameters,
               sd_varlink_method_flags_t _unused_(flags),
               void _unused_(*userdata))
{
  int r;

  if (verbose_flag)
    log_msg (LOG_INFO, "Varlink method \"Ping\" called...");

  r = sd_varlink_dispatch(link, parameters, NULL, NULL);
  if (r != 0)
    return r;

  return sd_varlink_replybo(link, SD_JSON_BUILD_PAIR_BOOLEAN("Alive", true));
}

static int
vl_method_set_log_level(sd_varlink *link, sd_json_variant *parameters,
                        sd_varlink_method_flags_t _unused_(flags),
                        void _unused_(*userdata))
{
  static const sd_json_dispatch_field dispatch_table[] = {
    { "Level", SD_JSON_VARIANT_INTEGER, sd_json_dispatch_int, 0, SD_JSON_MANDATORY },
    {}
  };

  int r, level;

  if (verbose_flag)
    log_msg(LOG_INFO, "Varlink method \"SetLogLevel\" called...");

  r = sd_varlink_dispatch(link, parameters, dispatch_table, &level);
  if (r != 0)
    return r;

  if (debug_flag)
    log_msg(LOG_DEBUG, "Log level %i requested", level);

  uid_t peer_uid;
  r = sd_varlink_get_peer_uid(link, &peer_uid);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to get peer UID: %s", strerror(-r));
      return r;
    }
  if (peer_uid != 0)
    {
      log_msg(LOG_WARNING, "SetLogLevel: peer UID %i denied", peer_uid);
      return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, parameters);
    }

  if (level >= LOG_DEBUG)
    debug_flag = 1;
  else
    debug_flag = 0;

  if (level >= LOG_INFO)
    verbose_flag = 1;
  else
    verbose_flag = 0;

  if (verbose_flag)
    log_msg (LOG_INFO, "New log settings: debug=%i, verbose=%i", debug_flag, verbose_flag);

  return sd_varlink_reply(link, NULL);
}

static int
vl_method_get_environment(sd_varlink *link, sd_json_variant *parameters,
                          sd_varlink_method_flags_t _unused_(flags),
                          void _unused_(*userdata))
{
  int r;

  if (verbose_flag)
    log_msg (LOG_INFO, "Varlink method \"GetEnvironment\" called...");

  r = sd_varlink_dispatch(link, parameters, NULL, NULL);
  if (r != 0)
    return r;

  uid_t peer_uid;
  r = sd_varlink_get_peer_uid(link, &peer_uid);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to get peer UID: %s", strerror(-r));
      return r;
    }
  if (peer_uid != 0)
    {
      log_msg(LOG_WARNING, "GetEnvironment: peer UID %i denied", peer_uid);
      return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, parameters);
    }

#if 0 /* XXX */
  for (char **e = environ; *e != 0; e++)
    {
      if (!env_assignment_is_valid(*e))
        goto invalid;
      if (!utf8_is_valid(*e))
        goto invalid;
    }
#endif

  return sd_varlink_replybo(link, SD_JSON_BUILD_PAIR_STRV("Environment", environ));

#if 0
 invalid:
  return sd_varlink_error(link, "io.systemd.service.InconsistentEnvironment", parameters);
#endif
}

static int
vl_method_status (sd_varlink *link, sd_json_variant *parameters,
		  sd_varlink_method_flags_t _unused_(flags),
		  void *userdata)
{
  static const sd_json_dispatch_field dispatch_table[] = {
    {}
  };
  char buf[FORMAT_TIMESTAMP_MAX];
  RM_CTX *ctx = userdata;
  int r;

  if (verbose_flag)
    log_msg (LOG_INFO, "Varlink method \"Status\" called...");

  r = sd_varlink_dispatch (link, parameters, dispatch_table, /* userdata= */ NULL);
  if (r != 0)
    return r;

  _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;

  RM_RebootStatus tmp_status = ctx->reboot_status;
  if (ctx->temp_off)
    tmp_status = RM_REBOOTSTATUS_NOT_REQUESTED;

  r = sd_json_buildo(&v, SD_JSON_BUILD_PAIR("RebootStatus", SD_JSON_BUILD_INTEGER(tmp_status)));
  if (r == 0 && ctx->reboot_method != RM_REBOOTMETHOD_UNKNOWN)
    {
      r = sd_json_variant_merge_objectbo(&v,
	      SD_JSON_BUILD_PAIR("RequestedMethod", SD_JSON_BUILD_INTEGER(ctx->reboot_method)),
	      SD_JSON_BUILD_PAIR("RebootTime", SD_JSON_BUILD_STRING(format_timestamp(buf, sizeof(buf), ctx->reboot_time))));
    }
  if (r < 0)
    {
      log_msg (LOG_ERR, "Failed to build JSON data: %s", strerror (-r));
      return r;
    }

  return sd_varlink_reply (link, v);
}

static int
vl_method_fullstatus (sd_varlink *link, sd_json_variant *parameters,
		      sd_varlink_method_flags_t _unused_(flags),
		      void *userdata)
{
  static const sd_json_dispatch_field dispatch_table[] = {
    {}
  };
  RM_CTX *ctx = userdata;
  int r;

  if (verbose_flag)
    log_msg (LOG_INFO, "Varlink method \"FullStatus\" called...");

  r = sd_varlink_dispatch (link, parameters, dispatch_table, /* userdata= */ NULL);
  if (r != 0)
    return r;

  _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;

  RM_RebootStatus tmp_status = ctx->reboot_status;
  if (ctx->temp_off)
    tmp_status = RM_REBOOTSTATUS_NOT_REQUESTED;

  r = sd_json_buildo (&v,
		      SD_JSON_BUILD_PAIR("RebootStatus", SD_JSON_BUILD_INTEGER(tmp_status)),
		      SD_JSON_BUILD_PAIR("RebootStrategy", SD_JSON_BUILD_INTEGER(ctx->reboot_strategy)));

  if (r >= 0 && ctx->reboot_method != RM_REBOOTMETHOD_UNKNOWN)
    r = sd_json_variant_merge_objectbo(&v, SD_JSON_BUILD_PAIR("RequestedMethod", SD_JSON_BUILD_INTEGER(ctx->reboot_method)));
  if (r >= 0 && ctx->maint_window_start)
    {
      _cleanup_(freep) char *str = NULL;

      calendar_spec_to_string(ctx->maint_window_start, &str);
      r = sd_json_variant_merge_objectbo(&v, SD_JSON_BUILD_PAIR("MaintenanceWindowStart", SD_JSON_BUILD_STRING(str)));
    }
  if (r >= 0 && ctx->maint_window_duration != BAD_TIME)
    r = sd_json_variant_merge_objectbo(&v, SD_JSON_BUILD_PAIR("MaintenanceWindowDuration", SD_JSON_BUILD_INTEGER(ctx->maint_window_duration)));
  if (r >= 0 && ctx->reboot_time)
    {
      char buf[FORMAT_TIMESTAMP_MAX];
      r = sd_json_variant_merge_objectbo(&v, SD_JSON_BUILD_PAIR("RebootTime", SD_JSON_BUILD_STRING(format_timestamp(buf, sizeof(buf), ctx->reboot_time))));
    }

  if (r < 0)
    {
      log_msg (LOG_ERR, "Failed to build JSON data: %s", strerror (-r));
      return r;
    }

  return sd_varlink_reply (link, v);
}

static int
calc_reboot_time (RM_CTX *ctx, usec_t *ret)
{
  usec_t next;
  usec_t curr = now (CLOCK_REALTIME);
  usec_t duration = ctx->maint_window_duration * USEC_PER_SEC;

  /* Check, if we are inside the maintenance window. If yes, reboot now. */
  int r = calendar_spec_next_usec (ctx->maint_window_start, curr - duration, &next);
  if (r < 0)
    {
      log_msg (LOG_ERR, "ERROR: Internal error converting the timer: %s",
               strerror (-r));
      return r;
    }
  if (curr > next && curr < next + duration)
    {
      /* We are inside the maintenance window. */
      next = curr;
    }
  else
    {
      /* we are not inside a maintenance window, set timer for next one */
      r = calendar_spec_next_usec (ctx->maint_window_start, curr, &next);
      if (r < 0)
	{
	  log_msg (LOG_ERR, "ERROR: Internal error converting the timer: %s",
		   strerror (-r));
	  return r;
	}

      /* Add a random delay between 0 and duration to not reboot
	 everything at the beginning of the maintenance window */
      next = next + ((usec_t)rand() * USEC_PER_SEC) % duration;
    }

  if (debug_flag || verbose_flag)
    {
      char buf[FORMAT_TIMESTAMP_MAX];
      int64_t in_secs = (next - curr) / USEC_PER_SEC;

      log_msg (LOG_NOTICE, "Reboot in %i seconds at %s", in_secs,
               format_timestamp(buf, sizeof(buf), next));
    }

  *ret = next;

  return 0;
}

static void
reset_timer(RM_CTX *ctx)
{
  ctx->reboot_status = RM_REBOOTSTATUS_NOT_REQUESTED;
  ctx->reboot_method = RM_REBOOTMETHOD_UNKNOWN;
  ctx->timer = sd_event_source_unref (ctx->timer);
}

static int
time_handler (sd_event_source _unused_(*s), uint64_t _unused_(usec), void *userdata)
{
  RM_CTX *ctx = userdata;

  if (debug_flag)
    log_msg (LOG_DEBUG, "Time handler for reboot called");

  if (ctx->temp_off)
    {
      if (debug_flag)
	log_msg (LOG_DEBUG, "Reboot temporary disabled, ignoring timer");
      return 0;
    }

  if (ctx->reboot_status > 0)
    {
      switch (ctx->reboot_method)
	{
	case RM_REBOOTMETHOD_HARD:
	  log_msg (LOG_INFO, "rebootmgr: reboot triggered now!");
	  break;
	case RM_REBOOTMETHOD_SOFT:
	  log_msg (LOG_INFO, "rebootmgr: soft-reboot triggered now!");
	  break;
	default:
	  log_msg (LOG_ERR, "rebootmgr: internal error, reboot method is invalid: %i",
		   ctx->reboot_method);
	  return -EINVAL;
	}

      if (debug_flag)
	{
	  switch (ctx->reboot_method)
	    {
	    case RM_REBOOTMETHOD_HARD:
	      log_msg (LOG_DEBUG, "systemctl reboot called!");
	      break;
	    case RM_REBOOTMETHOD_SOFT:
	      log_msg (LOG_DEBUG, "systemctl soft-reboot called!");
	      break;
	    default:
	      /* cannot happen */
	      break;
	    }
	}
      else
        {
          pid_t pid = fork();

          if (pid < 0)
            {
              log_msg (LOG_ERR, "Calling /usr/bin/systemctl failed: %m");
            }
          else if (pid == 0)
            {
	      int r;

	      switch (ctx->reboot_method)
		{
		case RM_REBOOTMETHOD_HARD:
                  char envar1[] = "SYSTEMCTL_SKIP_AUTO_SOFT_REBOOT=1";
                  char *env[] = {envar1, NULL};

                  r = execle ("/usr/bin/systemctl", "systemctl", "reboot",
			      NULL, env);

		  break;
		case RM_REBOOTMETHOD_SOFT:
                  r = execl ("/usr/bin/systemctl", "systemctl", "soft-reboot",
                             NULL);
		  break;
		default:
		  /* cannot happen */
		  break;
		}
	      if (r < 0)
		{
		  log_msg (LOG_ERR, "Calling /usr/bin/systemctl %s failed: %m",
			   (ctx->reboot_method == RM_REBOOTMETHOD_HARD)?"reboot":"soft-reboot");
		  exit (1);
                }
              exit (0);
            }
        }

      reset_timer(ctx);
    }

  return 0;
}

static int
vl_method_reboot(sd_varlink *link, sd_json_variant *parameters,
		 sd_varlink_method_flags_t _unused_(flags),
		 void *userdata)
{
  struct p {
    int reboot_method;
    bool force;
  } p = {
    .reboot_method = RM_REBOOTMETHOD_UNKNOWN,
    .force = false,
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "Reboot",       SD_JSON_VARIANT_INTEGER, sd_json_dispatch_int,     offsetof(struct p, reboot_method), SD_JSON_MANDATORY },
    { "Force",        SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct p, force),         0 },
    {}
  };
  char time_str[FORMAT_TIMESTAMP_MAX];
  RM_CTX *ctx = userdata;
  int r;

  if (verbose_flag)
    log_msg (LOG_INFO, "Varlink method \"Reboot\" called...");

  r = sd_varlink_dispatch(link, parameters, dispatch_table, &p);
  if (r != 0)
    {
      log_msg(LOG_ERR, "Reboot request: varlik dispatch failed: %s", strerror (-r));
      return r;
    }

  if (debug_flag)
    {
      const char *str;

      rm_method_to_str(p.reboot_method, &str);
      log_msg(LOG_DEBUG, "Reboot request: %s (%i), force: %s", str, p.reboot_method, bool_to_str (p.force));
    }

  uid_t peer_uid;
  r = sd_varlink_get_peer_uid(link, &peer_uid);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to get peer UID: %s", strerror(-r));
      return r;
    }
  if (peer_uid != 0)
    {
      log_msg(LOG_WARNING, "Reboot: peer UID %i denied", peer_uid);
      return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, parameters);
    }

  if (p.reboot_method != RM_REBOOTMETHOD_HARD &&
      p.reboot_method != RM_REBOOTMETHOD_SOFT)
    return sd_varlink_error_invalid_parameter_name(link, "reboot");

  if (ctx->reboot_status != RM_REBOOTSTATUS_NOT_REQUESTED)
    return sd_varlink_errorbo(link, "org.openSUSE.rebootmgr.AlreadyInProgress",
			      SD_JSON_BUILD_PAIR_INTEGER("Method", ctx->reboot_method),
			      SD_JSON_BUILD_PAIR_STRING("Scheduled", format_timestamp (time_str, sizeof (time_str), ctx->reboot_time)));

  ctx->reboot_method = p.reboot_method;
  ctx->reboot_status = RM_REBOOTSTATUS_REQUESTED;

  usec_t reboot_time;
  if (p.force)
      reboot_time = now(CLOCK_REALTIME);
  else
    {
      r = calc_reboot_time(ctx, &reboot_time);
      if (r < 0)
	{
	  ctx->reboot_method = RM_REBOOTMETHOD_UNKNOWN;
	  ctx->reboot_status = RM_REBOOTSTATUS_NOT_REQUESTED;
	  log_msg(LOG_ERR, "Cannot calculate reboot timer: %s", strerror(-r));
	  return sd_varlink_error(link,"org.openSUSE.rebootmgr.InternalError", NULL);
	}
    }

  r = sd_event_add_time(ctx->loop, &ctx->timer, CLOCK_REALTIME,
			reboot_time, 0, time_handler, ctx);
  if (r < 0)
    {
      ctx->reboot_method = RM_REBOOTMETHOD_UNKNOWN;
      ctx->reboot_status = RM_REBOOTSTATUS_NOT_REQUESTED;
      log_msg(LOG_ERR, "Cannot add reboot timer to event loop: %s", strerror(-r));
      return sd_varlink_errorbo(link, "org.openSUSE.rebootmgr.InternalError", NULL);
    }
  ctx->reboot_status = RM_REBOOTSTATUS_WAITING_WINDOW;
  ctx->reboot_time = reboot_time;

  return sd_varlink_replybo(link,
			    SD_JSON_BUILD_PAIR_INTEGER("Method", ctx->reboot_method),
			    SD_JSON_BUILD_PAIR_STRING("Scheduled", format_timestamp (time_str, sizeof (time_str), ctx->reboot_time)));
}

static int
vl_method_set_strategy (sd_varlink *link, sd_json_variant *parameters,
			sd_varlink_method_flags_t _unused_(flags),
			void *userdata)
{
  struct p {
    RM_RebootStrategy strategy;
  } p = {
    .strategy = RM_REBOOTSTRATEGY_UNKNOWN,
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "Strategy", SD_JSON_VARIANT_INTEGER, sd_json_dispatch_int, offsetof(struct p, strategy), SD_JSON_MANDATORY },
    {}
  };
  RM_CTX *ctx = userdata;
  int r;

  if (verbose_flag)
    log_msg (LOG_INFO, "Varlink method \"SetStrategy\" called...");

  r = sd_varlink_dispatch (link, parameters, dispatch_table, &p);
  if (r != 0)
    {
      log_msg (LOG_ERR, "Set strategy request: varlik dispatch failed: %s", strerror (-r));
      return r;
    }

  uid_t peer_uid;
  r = sd_varlink_get_peer_uid(link, &peer_uid);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to get peer UID: %s", strerror(-r));
      return r;
    }
  if (peer_uid != 0)
    {
      log_msg(LOG_WARNING, "SetStrategy: peer UID %i denied", peer_uid);
      return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, parameters);
    }

  if (p.strategy > RM_REBOOTSTRATEGY_UNKNOWN &&
      p.strategy >= RM_REBOOTSTRATEGY_OFF &&
      ctx->reboot_strategy != p.strategy)
    {
      /* Don't save strategy "off" */
      if (p.strategy != RM_REBOOTSTRATEGY_OFF)
	{
	  r = save_config(p.strategy, NULL, 0);
	  if (r < 0)
	    {
	      log_msg(LOG_ERR, "Saving new reboot strategy failed");
	      return sd_varlink_errorbo(link, "org.openSUSE.rebootmgr.ErrorWritingConfig",
					SD_JSON_BUILD_PAIR_BOOLEAN("Success", false));
	    }
	}

      ctx->reboot_strategy = p.strategy;

      /* Informal log message */
      const char *str;
      rm_strategy_to_str(p.strategy, &str);
      log_msg(LOG_INFO, "Reboot strategy changed to '%s'", str);
    }
  else
    {
      log_msg(LOG_ERR, "Reboot strategy not changed, invalid value (%i)", p.strategy);
      return sd_varlink_errorbo(link, "org.openSUSE.rebootmgr.InvalidParameter",
				SD_JSON_BUILD_PAIR_BOOLEAN("Success", false));
    }

  return sd_varlink_replybo(link, SD_JSON_BUILD_PAIR_BOOLEAN("Success", true));
}


struct set_window {
  char *start;
  char *duration;
};

static void
set_window_free (struct set_window *var)
{
  var->start = mfree(var->start);
  var->duration = mfree(var->duration);
}

static int
vl_method_set_window (sd_varlink *link, sd_json_variant *parameters,
		      sd_varlink_method_flags_t _unused_(flags),
		      void *userdata)
{
  _cleanup_(set_window_free) struct set_window p = {
    .start = NULL,
    .duration = NULL
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "Start",    SD_JSON_VARIANT_STRING, sd_json_dispatch_string, offsetof(struct set_window, start), SD_JSON_MANDATORY },
    { "Duration", SD_JSON_VARIANT_STRING, sd_json_dispatch_string, offsetof(struct set_window, duration), SD_JSON_MANDATORY },
    {}
  };
  RM_CTX *ctx = userdata;
  int r;

  if (verbose_flag)
    log_msg (LOG_INFO, "Varlink method \"SetWindow\" called...");

  r = sd_varlink_dispatch (link, parameters, dispatch_table, &p);
  if (r != 0)
    {
      log_msg (LOG_ERR, "Set strategy request: varlik dispatch failed: %s", strerror (-r));
      return r;
    }

  uid_t peer_uid;
  r = sd_varlink_get_peer_uid(link, &peer_uid);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to get peer UID: %s", strerror(-r));
      return r;
    }
  if (peer_uid != 0)
    {
      log_msg(LOG_WARNING, "SetWindow: peer UID %i denied", peer_uid);
      return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, parameters);
    }

  CalendarSpec *new_start = NULL;
  if (p.start == NULL || strlen (p.start) == 0 ||
      calendar_spec_from_string (p.start, &new_start) < 0)
    {
      log_msg(LOG_ERR, "Reboot strategy not changed, invalid value for window start (%s)", p.start);
      return sd_varlink_errorbo(link, "org.openSUSE.rebootmgr.InvalidParameter",
				SD_JSON_BUILD_PAIR_STRING("Variable", "start time"),
				SD_JSON_BUILD_PAIR_BOOLEAN("Success", false));
    }

  time_t new_duration;
  if (p.duration == NULL || strlen(p.duration) == 0 ||
      (new_duration = parse_duration(p.duration)) == BAD_TIME)
    {
      calendar_spec_free(new_start);

      log_msg(LOG_ERR, "Reboot strategy not changed, invalid value for duration (%s)", p.duration);
      return sd_varlink_errorbo(link, "org.openSUSE.rebootmgr.InvalidParameter",
				SD_JSON_BUILD_PAIR_STRING("Variable", "duration"),
				SD_JSON_BUILD_PAIR_BOOLEAN("Success", false));
    }

  r = save_config(RM_REBOOTSTRATEGY_UNKNOWN, new_start, new_duration);
  if (r < 0)
    {
      calendar_spec_free(new_start);
      log_msg(LOG_ERR, "Maintenance window not changed, saving failed");
      return sd_varlink_errorbo(link, "org.openSUSE.rebootmgr.ErrorWritingConfig",
				SD_JSON_BUILD_PAIR_BOOLEAN("Success", false));
    }

  calendar_spec_free(ctx->maint_window_start);
  ctx->maint_window_start = new_start;
  ctx->maint_window_duration = new_duration;

  /* Informal log message */
  _cleanup_(freep) char *start_str = NULL;
  _cleanup_(freep) const char *duration_str = NULL;
  calendar_spec_to_string (ctx->maint_window_start, &start_str);
  r = rm_duration_to_string (ctx->maint_window_duration, &duration_str);
  if (r >= 0)
    log_msg (LOG_INFO, "Maintenance window changed to '%s', lasting %s",
	     start_str, duration_str);

  return sd_varlink_replybo (link, SD_JSON_BUILD_PAIR_BOOLEAN("Success", true));
}

static int
vl_method_cancel (sd_varlink *link, sd_json_variant *parameters,
		  sd_varlink_method_flags_t _unused_(flags),
		  void *userdata)
{
  static const sd_json_dispatch_field dispatch_table[] = {
    {}
  };
  RM_CTX *ctx = userdata;
  int r;

  if (verbose_flag)
    log_msg (LOG_INFO, "Varlink method \"Cancel\" called...");

  r = sd_varlink_dispatch (link, parameters, dispatch_table, /* userdata= */ NULL);
  if (r != 0)
    {
      log_msg (LOG_ERR, "Cancel request: varlik dispatch failed: %s", strerror (-r));
      return r;
    }

  uid_t peer_uid;
  r = sd_varlink_get_peer_uid(link, &peer_uid);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to get peer UID: %s", strerror(-r));
      return r;
    }
  if (peer_uid != 0)
    {
      log_msg(LOG_WARNING, "Cancel: peer UID %i denied", peer_uid);
      return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, parameters);
    }

  if (ctx->reboot_status == RM_REBOOTSTATUS_NOT_REQUESTED)
    return sd_varlink_error (link, "org.openSUSE.rebootmgr.NoRebootScheduled", NULL);

  r = sd_event_source_set_enabled (ctx->timer, SD_EVENT_OFF);
  if (r != 0)
    {
      log_msg (LOG_ERR, "Cancel request: disabling timer failed: %s", strerror (-r));
      return r;
    }

  reset_timer(ctx);

  return sd_varlink_replybo (link, SD_JSON_BUILD_PAIR_BOOLEAN("Success", true));
}

static int
vl_method_quit (sd_varlink *link, sd_json_variant *parameters,
		  sd_varlink_method_flags_t _unused_(flags),
		  void *userdata)
{
  struct p {
    int code;
  } p = {
    .code = 0
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "ExitCode", SD_JSON_VARIANT_INTEGER, sd_json_dispatch_int, offsetof(struct p, code), 0 },
    {}
  };
  RM_CTX *ctx = userdata;
  int r;

  if (verbose_flag)
    log_msg (LOG_INFO, "Varlink method \"Quit\" called...");

  r = sd_varlink_dispatch (link, parameters, dispatch_table, /* userdata= */ NULL);
  if (r != 0)
    {
      log_msg (LOG_ERR, "Quit request: varlik dispatch failed: %s", strerror (-r));
      return r;
    }

  uid_t peer_uid;
  r = sd_varlink_get_peer_uid(link, &peer_uid);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to get peer UID: %s", strerror(-r));
      return r;
    }
  if (peer_uid != 0)
    {
      log_msg(LOG_WARNING, "Quit: peer UID %i denied", peer_uid);
      return sd_varlink_error(link, SD_VARLINK_ERROR_PERMISSION_DENIED, parameters);
    }

  r = sd_event_exit (ctx->loop, p.code);
  if (r != 0)
    {
      log_msg (LOG_ERR, "Quit request: disabling event loop failed: %s",
	       strerror (-r));
      return sd_varlink_errorbo(link, "org.openSUSE.rebootmgr.InternalError",
				SD_JSON_BUILD_PAIR_BOOLEAN("Success", false));
    }

  ctx->timer = sd_event_source_unref (ctx->timer);
  ctx->reboot_status = RM_REBOOTSTATUS_NOT_REQUESTED;
  ctx->reboot_method = RM_REBOOTMETHOD_UNKNOWN;

  return sd_varlink_replybo (link, SD_JSON_BUILD_PAIR_BOOLEAN("Success", true));
}

/* Send a messages to systemd daemon, that inicialization of daemon
   is finished and daemon is ready to accept connections. */
static void
announce_ready (void)
{
  int r = sd_notify (0, "READY=1\n"
		     "STATUS=Processing requests...");
  if (r < 0)
    log_msg (LOG_ERR, "sd_notify(READY) failed: %s", strerror(-r));
}

static void
announce_stopping (void)
{
  int r = sd_notify (0, "STOPPING=1\n"
                     "STATUS=Shutting down...");
  if (r < 0)
    log_msg (LOG_ERR, "sd_notify(STOPPING) failed: %s", strerror(-r));
}

static int
varlink_server_loop(sd_varlink_server *server, RM_CTX *ctx)
{
  int r;

  /* Runs a sd_varlink service event loop populated with a passed fd. */

  r = sd_event_new(&(ctx->loop));
  if (r < 0)
    return r;

  r = sd_varlink_server_set_exit_on_idle(server, false);
  if (r < 0)
    return r;

  r = sd_varlink_server_attach_event(server, ctx->loop, SD_EVENT_PRIORITY_NORMAL);
  if (r < 0)
    return r;

  r = sd_varlink_server_listen_auto(server);
  if (r < 0)
    return r;

  announce_ready();
  r = sd_event_loop (ctx->loop);
  announce_stopping();

  return r;
}

static int
run_varlink (RM_CTX *ctx)
{
  int r;
  _cleanup_(sd_varlink_server_unrefp) sd_varlink_server *varlink_server = NULL;

  r = sd_varlink_server_new (&varlink_server, SD_VARLINK_SERVER_ACCOUNT_UID|SD_VARLINK_SERVER_INHERIT_USERDATA);
  if (r < 0)
    {
      log_msg (LOG_ERR, "Failed to allocate varlink server: %s",
	       strerror (-r));
      return r;
    }

  r = sd_varlink_server_set_info(varlink_server, "openSUSE",
				 PACKAGE" (rebootmgrd)",
				 VERSION,
				 "https://github.com/thkukuk/rebootmgr3");
  if (r < 0)
    return r;

  r = sd_varlink_server_set_description (varlink_server, "Rebootmgr");
  if (r < 0)
    {
      log_msg (LOG_ERR, "Failed to set varlink server description: %s",
	       strerror (-r));
      return r;
    }

  sd_varlink_server_set_userdata (varlink_server, ctx);

  r = sd_varlink_server_add_interface (varlink_server, &vl_interface_org_openSUSE_rebootmgr);
  if (r < 0)
    {
      log_msg (LOG_ERR, "Failed to add Varlink interface: %s",
	       strerror (-r));
      return r;
    }

  r = sd_varlink_server_bind_method_many(varlink_server,
					 "org.openSUSE.rebootmgr.Cancel",         vl_method_cancel,
					 "org.openSUSE.rebootmgr.FullStatus",     vl_method_fullstatus,
					 "org.openSUSE.rebootmgr.GetEnvironment", vl_method_get_environment,
					 "org.openSUSE.rebootmgr.Ping",           vl_method_ping,
					 "org.openSUSE.rebootmgr.Quit",           vl_method_quit,
					 "org.openSUSE.rebootmgr.Reboot",         vl_method_reboot,
					 "org.openSUSE.rebootmgr.SetLogLevel",    vl_method_set_log_level,
					 "org.openSUSE.rebootmgr.SetStrategy",    vl_method_set_strategy,
					 "org.openSUSE.rebootmgr.SetWindow",      vl_method_set_window,
					 "org.openSUSE.rebootmgr.Status",         vl_method_status);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to bind Varlink methods: %s",
	      strerror(-r));
      return r;
    }

  r = mkdir_p(RM_VARLINK_SOCKET_DIR, 0755);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to create directory '"RM_VARLINK_SOCKET_DIR"' for Varlink socket: %s",
	      strerror(-r));
      return r;
    }
  r = sd_varlink_server_listen_address(varlink_server, RM_VARLINK_SOCKET, 0666);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to bind to Varlink socket: %s", strerror (-r));
      return r;
    }

  r = varlink_server_loop(varlink_server, ctx);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to run Varlink event loop: %s",
	      strerror(-r));
      return r;
    }

  return 0;
}

static int
create_context (RM_CTX **ctx)
{
  if ((*ctx = malloc(sizeof(RM_CTX))) == NULL)
    {
      log_msg (LOG_ERR, "ERROR: Out of memory!");
      return -ENOMEM;
    }

  /*
   * default values if no config is provided:
   * RM_RebootStatus
   * RM_RebootMethod
   * RM_RebootStrategy
   * CalendarSpec
   * Maintenance Window Duration
   * temporary off
   */
  **ctx = (RM_CTX) {RM_REBOOTSTATUS_NOT_REQUESTED,
		   RM_REBOOTMETHOD_UNKNOWN,
		   RM_REBOOTSTRATEGY_BEST_EFFORT,
		   NULL, 3600, 0,
		   NULL, NULL, 0};
  calendar_spec_from_string("03:30", &(*ctx)->maint_window_start);

  return 0;
}

static int
destroy_context (RM_CTX *ctx)
{
  if (ctx == NULL)
    return -EBADF;

  calendar_spec_free (ctx->maint_window_start);
  sd_event_unrefp(&(ctx->loop));
  free (ctx);

  return 0;
}

static void
print_help (void)
{
  log_msg (LOG_INFO, "rebootmgrd - reboot following a specified strategy");

  log_msg (LOG_INFO, "  -d,--debug     Debug mode, no reboot done");
  log_msg (LOG_INFO, "  -v,--verbose   Verbose logging");
  log_msg (LOG_INFO, "  -?, --help     Give this help list");
  log_msg (LOG_INFO, "      --version  Print program version");
}

int
main (int argc, char **argv)
{
  RM_CTX *ctx = NULL;
  int r;

  log_init ();

  while (1)
    {
      int c;
      int option_index = 0;
      static struct option long_options[] =
        {
          {"debug", no_argument, NULL, 'd'},
          {"verbose", no_argument, NULL, 'v'},
          {"version", no_argument, NULL, '\255'},
          {"usage", no_argument, NULL, '?'},
          {"help", no_argument, NULL, 'h'},
          {NULL, 0, NULL, '\0'}
        };


      c = getopt_long (argc, argv, "dvh?", long_options, &option_index);
      if (c == (-1))
        break;
      switch (c)
        {
        case 'd':
          debug_flag = 1;
	  verbose_flag = 1;
          break;
        case '?':
        case 'h':
          print_help ();
          return 0;
        case 'v':
          verbose_flag = 1;
          break;
        case '\255':
          fprintf (stdout, "rebootmgrd (%s) %s\n", PACKAGE, VERSION);
          return 0;
        default:
          print_help ();
          return 1;
        }
    }

  argc -= optind;
  argv += optind;

  if (argc > 1)
    {
      fprintf (stderr, "Try `rebootmgrd --help' for more information.\n");
      return 1;
    }

  r = create_context (&ctx);
  if (r < 0)
    {
      log_msg (LOG_ERR, "ERROR: Could not initialize context: %s",
	       strerror (-r));
      return -r;
    }

  r = load_config (ctx);
  if (r < 0)
    {
      log_msg (LOG_ERR, "ERROR: Could not load configuration data: %s",
	       strerror (-r));
      return -r;
    }

  if (verbose_flag)
    log_msg (LOG_INFO, "Starting rebootmgrd (%s) %s...", PACKAGE, VERSION);

  r = run_varlink (ctx);
  if (r < 0)
    log_msg (LOG_ERR, "ERROR: varlink loop failed: %s", strerror (-r));

  r = destroy_context (ctx);
  if (r < 0)
    log_msg (LOG_ERR, "ERROR: Could not destroy context: %i", r);

  return -r;
}
