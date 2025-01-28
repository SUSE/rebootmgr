//SPDX-License-Identifier: GPL-2.0-or-later

/* Copyright (c) 2024 Thorsten Kukuk
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libintl.h>
#include <systemd/sd-varlink.h>
#include <libeconf.h>
#include <libeconf_ext.h>

#include "basics.h"
#include "common.h"
#include "parse-duration.h"

#ifndef _
#define _(String) gettext(String)
#endif

/* Takes inspiration from Rust's Option::take() method: reads and returns a pointer, but at the same time
 * resets it to NULL. See: https://doc.rust-lang.org/std/option/enum.Option.html#method.take */
#define TAKE_GENERIC(var, type, nullvalue)                       \
        ({                                                       \
                type *_pvar_ = &(var);                           \
                type _var_ = *_pvar_;                            \
                type _nullvalue_ = nullvalue;                    \
                *_pvar_ = _nullvalue_;                           \
                _var_;                                           \
        })
#define TAKE_PTR_TYPE(ptr, type) TAKE_GENERIC(ptr, type, NULL)
#define TAKE_PTR(ptr) TAKE_PTR_TYPE(ptr, typeof(ptr))

static int
connect_to_rebootmgr(sd_varlink **ret)
{
  _cleanup_(sd_varlink_unrefp) sd_varlink *link = NULL;
  int r;

  r = sd_varlink_connect_address(&link, RM_VARLINK_SOCKET);
  if (r < 0)
    {
      fprintf(stderr, "Failed to connect to " RM_VARLINK_SOCKET ": %s\n",
	       strerror(-r));
      return r;
    }

  *ret = TAKE_PTR(link);
  return 0;
}

static int
trigger_reboot(RM_RebootMethod method, bool forced)
{
  struct p {
    int reboot_method;
    char *reboot_time;
  } p = {
    .reboot_method = 0,
    .reboot_time = NULL
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "Method", SD_JSON_VARIANT_INTEGER, sd_json_dispatch_int, offsetof(struct p, reboot_method), 0 },
    { "Scheduled", SD_JSON_VARIANT_STRING, sd_json_dispatch_string, offsetof(struct p, reboot_time), 0 },
      {}
  };
  _cleanup_(sd_varlink_unrefp) sd_varlink *link = NULL;
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *params = NULL;
  sd_json_variant *result;
  int r;

  r = connect_to_rebootmgr(&link);
  if (r < 0)
    return r;

  r = sd_json_buildo(&params,
		     SD_JSON_BUILD_PAIR("Reboot", SD_JSON_BUILD_INTEGER(method)),
		     SD_JSON_BUILD_PAIR("Force", SD_JSON_BUILD_BOOLEAN(forced)));
  if (r < 0)
    {
      fprintf(stderr, "Failed to build JSON data: %s\n", strerror(-r));
      return r;
    }

  const char *error_id;
  r = sd_varlink_call(link, "org.openSUSE.rebootmgr.Reboot", params, &result, &error_id);
  if (r < 0)
    {
      fprintf(stderr, "Failed to call reboot method: %s\n", strerror(-r));
      return r;
    }

  /* dispatch before checking error_id, we may need the result for the error
     message */
  r = sd_json_dispatch(result, dispatch_table, SD_JSON_ALLOW_EXTENSIONS, &p);
  if (r < 0)
    {
      fprintf(stderr, _("Failed to parse JSON answer: %s\n"), strerror(-r));
      return r;
    }

  const char *method_str = NULL;
  r = rm_method_to_str(p.reboot_method, &method_str);
  if (r < 0)
    method_str = _("unknown reboot");

  if (error_id && strlen(error_id) > 0)
    {
      if (strcmp(error_id, "org.openSUSE.rebootmgr.AlreadyInProgress") == 0)
	printf(_("A %s is already scheduled for %s, ignoring new request\n"),
		method_str, p.reboot_time);
      else
	fprintf(stderr, _("Calling rebootmgrd failed: %s\n"), error_id);

      free(p.reboot_time);
      return -1;
    }

  printf(_("The %s got scheduled for %s\n"),  method_str, p.reboot_time);

  free(p.reboot_time);
  return 0;
}

static int
cancel_reboot(void)
{
  struct p {
    bool success;
  } p = {
    .success = false
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "Success", SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct p, success), 0 },
      {}
  };
  _cleanup_(sd_varlink_unrefp) sd_varlink *link = NULL;
  sd_json_variant *result;
  int r;

  r = connect_to_rebootmgr(&link);
  if (r < 0)
    return r;

  const char *error_id;
  r = sd_varlink_call(link, "org.openSUSE.rebootmgr.Cancel", NULL, &result, &error_id);
  if (r < 0)
    {
      fprintf(stderr, _("Failed to call cancel method: %s\n"), strerror(-r));
      return r;
    }
  if (error_id && strlen(error_id) > 0)
    {
      if (strcmp(error_id, "org.openSUSE.rebootmgr.NoRebootScheduled") == 0)
	printf(_("There is no reboot scheduled which can be canceld\n"));
      else
	fprintf(stderr, _("Calling rebootmgrd failed: %s\n"), error_id);
      return -1;
    }

  r = sd_json_dispatch(result, dispatch_table, SD_JSON_ALLOW_EXTENSIONS, &p);
  if (r < 0)
    {
      fprintf(stderr, _("Failed to parse JSON answer: %s\n"), strerror(-r));
      return r;
    }

  if (p.success == true)
    printf(_("Request to cancel reboot was successful\n"));
  else
    printf(_("Request to cancel reboot failed\n"));

  return 0;
}

static int
set_strategy(RM_RebootStrategy strategy)
{
  struct p {
    bool success;
  } p = {
    .success = false
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "Success", SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct p, success), 0 },
      {}
  };
  _cleanup_(sd_varlink_unrefp) sd_varlink *link = NULL;
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *params = NULL;
  sd_json_variant *result;
  int r;

  r = connect_to_rebootmgr(&link);
  if (r < 0)
    return r;

  r = sd_json_buildo(&params,
		     SD_JSON_BUILD_PAIR("Strategy", SD_JSON_BUILD_INTEGER(strategy)));
  if (r < 0)
    {
      fprintf(stderr, _("Failed to build JSON data: %s\n"), strerror(-r));
      return r;
    }

  const char *error_id;
  r = sd_varlink_call(link, "org.openSUSE.rebootmgr.SetStrategy", params, &result, &error_id);
  if (r < 0)
    {
      fprintf(stderr, _("Failed to call SetStrategy method: %s\n"), strerror(-r));
      return r;
    }

  if (error_id && strlen(error_id) > 0)
    {
      if (strcmp(error_id, "org.openSUSE.rebootmgr.InvalidParameter") == 0)
	printf(_("Strategy '%i' got rejected as invalid\n"), strategy);
      else if (strcmp(error_id, "org.openSUSE.rebootmgr.ErrorWritingConfig") == 0)
	printf(_("Updating configuration file failed\n"));
      else
	fprintf(stderr, _("Calling rebootmgrd failed: %s\n"), error_id);
      return -1;
    }

  r = sd_json_dispatch(result, dispatch_table, SD_JSON_ALLOW_EXTENSIONS, &p);
  if (r < 0)
    {
      fprintf(stderr, _("Failed to parse JSON answer: %s\n"), strerror(-r));
      return r;
    }

  if (p.success == true)
    printf(_("Request to set new strategy was successful\n"));
  else
    printf(_("Request to set new strategy failed\n"));

  return 0;
}

static int
set_window(const char *start, const char *duration)
{
  struct p {
    char *variable;
    bool success;
  } p = {
    .variable = NULL,
    .success = false
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "Variable", SD_JSON_VARIANT_STRING,  sd_json_dispatch_string,  offsetof(struct p, variable), 0 },
    { "Success",  SD_JSON_VARIANT_BOOLEAN, sd_json_dispatch_stdbool, offsetof(struct p, success), 0 },
      {}
  };
  _cleanup_(sd_varlink_unrefp) sd_varlink *link = NULL;
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *params = NULL;
  sd_json_variant *result;
  int r;

  r = connect_to_rebootmgr(&link);
  if (r < 0)
    return r;

  r = sd_json_buildo(&params,
		     SD_JSON_BUILD_PAIR("Start", SD_JSON_BUILD_STRING(start)),
		     SD_JSON_BUILD_PAIR("Duration", SD_JSON_BUILD_STRING(duration)));
  if (r < 0)
    {
      fprintf(stderr, "Failed to build JSON data: %s\n", strerror(-r));
      return r;
    }

  const char *error_id;
  r = sd_varlink_call(link, "org.openSUSE.rebootmgr.SetWindow", params, &result, &error_id);
  if (r < 0)
    {
      fprintf(stderr, "Failed to call SetWindow method: %s\n", strerror(-r));
      return r;
    }

  /* dispatch before checking error_id, we may need the result for the error
     message */
  r = sd_json_dispatch(result, dispatch_table, SD_JSON_ALLOW_EXTENSIONS, &p);
  if (r < 0)
    {
      fprintf(stderr, _("Failed to parse JSON answer: %s\n"), strerror(-r));
      return r;
    }

  if (error_id && strlen(error_id) > 0)
    {
      if (strcmp(error_id, "org.openSUSE.rebootmgr.InvalidParameter") == 0)
	printf(_("New maintenance window got rejected as invalid (%s)\n"),
		p.variable);
      else if (strcmp(error_id, "org.openSUSE.rebootmgr.ErrorWritingConfig") == 0)
	printf(_("Updating configuration file failed\n"));
      else
	fprintf(stderr, _("Calling rebootmgrd failed: %s\n"), error_id);
      free(p.variable);
      return -1;
    }

  if (p.success == true)
    printf(_("Request to set new maintenance window was successful\n"));
  else
    printf(_("Request to set new maintenance window failed\n"));

  free(p.variable);

  return 0;
}

static int
get_status(RM_RebootStatus *status, RM_RebootMethod *method, char **reboot_time)
{
  struct p {
    RM_RebootStatus status;
    RM_RebootMethod method;
    char *reboot_time;
  } p = {
    .status = RM_REBOOTSTATUS_NOT_REQUESTED,
    .method = RM_REBOOTMETHOD_UNKNOWN,
    .reboot_time = NULL
  };
  static const sd_json_dispatch_field dispatch_table[] = {
    { "RebootStatus",    SD_JSON_VARIANT_INTEGER, sd_json_dispatch_int,    offsetof(struct p, status),      SD_JSON_MANDATORY },
    { "RebootTime",      SD_JSON_VARIANT_STRING,  sd_json_dispatch_string, offsetof(struct p, reboot_time), 0                 },
    { "RequestedMethod", SD_JSON_VARIANT_INTEGER, sd_json_dispatch_int,    offsetof(struct p, method),      0                 },
    {}
  };
  _cleanup_(sd_varlink_unrefp) sd_varlink *link = NULL;
  sd_json_variant *result;
  int r;

  r = connect_to_rebootmgr (&link);
  if (r < 0)
    return r;

  const char *error_id;
  r = sd_varlink_call(link, "org.openSUSE.rebootmgr.Status", NULL, &result, &error_id);
  if (r < 0)
    {
      fprintf(stderr, "Failed to call status method: %s\n", strerror(-r));
      return r;
    }
  if (error_id && strlen(error_id) > 0)
    {
      fprintf(stderr, _("Calling rebootmgrd failed: %s\n"), error_id);
      return -1;
    }

  r = sd_json_dispatch(result, dispatch_table, SD_JSON_ALLOW_EXTENSIONS, &p);
  if (r < 0)
    {
      fprintf(stderr, _("Failed to parse JSON answer: %s\n"), strerror(-r));
      return r;
    }

  *status = p.status;
  *method = p.method;
  if (reboot_time)
    *reboot_time = p.reboot_time;

  return 0;
}

struct status {
  RM_RebootStatus status;
  RM_RebootMethod method;
  RM_RebootStrategy strategy;
  char *maint_window_start;
  time_t maint_window_duration;
  char *reboot_time;
};

static void
struct_status_free(struct status *p)
{
  p->maint_window_start = mfree(p->maint_window_start);
  p->reboot_time = mfree(p->reboot_time);
}

static int
get_full_status(struct status *p)
{
  static const sd_json_dispatch_field dispatch_table[] = {
    { "RebootStatus",              SD_JSON_VARIANT_INTEGER, sd_json_dispatch_int,    offsetof(struct status, status),                SD_JSON_MANDATORY },
    { "RequestedMethod",           SD_JSON_VARIANT_INTEGER, sd_json_dispatch_int,    offsetof(struct status, method),                0                 },
    { "RebootTime",                SD_JSON_VARIANT_STRING,  sd_json_dispatch_string, offsetof(struct status, reboot_time),           0                 },
    { "RebootStrategy",            SD_JSON_VARIANT_INTEGER, sd_json_dispatch_int,    offsetof(struct status, strategy),              SD_JSON_MANDATORY },
    { "MaintenanceWindowStart",    SD_JSON_VARIANT_STRING,  sd_json_dispatch_string, offsetof(struct status, maint_window_start),    SD_JSON_MANDATORY },
    { "MaintenanceWindowDuration", SD_JSON_VARIANT_INTEGER, sd_json_dispatch_int64,  offsetof(struct status, maint_window_duration), SD_JSON_MANDATORY },
    {}
  };
  _cleanup_(sd_varlink_unrefp) sd_varlink *link = NULL;
  sd_json_variant *result;
  int r;

  r = connect_to_rebootmgr(&link);
  if (r < 0)
    return r;

  const char *error_id;
  r = sd_varlink_call(link, "org.openSUSE.rebootmgr.FullStatus", NULL, &result, &error_id);
  if (r < 0)
    {
      fprintf(stderr, "Failed to call status method: %s\n", strerror(-r));
      return r;
    }
  if (error_id && strlen(error_id) > 0)
    {
      fprintf(stderr, _("Calling rebootmgrd failed: %s\n"), error_id);
      return -1;
    }

  r = sd_json_dispatch(result, dispatch_table, SD_JSON_ALLOW_EXTENSIONS, p);
  if (r < 0)
    {
      fprintf(stderr, _("Failed to parse JSON answer: %s\n"), strerror(-r));
      return r;
    }

  return 0;
}

static int
print_full_status(void)
{
  _cleanup_(struct_status_free) struct status status = {
    .status = RM_REBOOTSTATUS_NOT_REQUESTED,
    .method = RM_REBOOTMETHOD_UNKNOWN,
    .strategy = RM_REBOOTSTRATEGY_UNKNOWN,
    .maint_window_start = NULL,
    .maint_window_duration = 0,
    .reboot_time = NULL
  };
  const char *str = NULL;
  int r;

  r = get_full_status(&status);
  if (r < 0)
    return r;

  r = rm_status_to_str(status.status, status.method, &str);
  if (r < 0)
    {
      fprintf(stderr, "Converting status to string failed: %s\n", strerror(-r));
      return -1;
    }
  else
    printf("Status: %s\n", str);

  if (status.reboot_time && strlen(status.reboot_time) > 0)
    printf("Reboot at: %s\n", status.reboot_time);

  r = rm_strategy_to_str(status.strategy, &str);
  if (r < 0)
    {
      fprintf(stderr, "Converting strategy to string failed: %s\n", strerror(-r));
      return -1;
    }
  else
    printf("Strategy: %s\n", str);

  if (status.maint_window_start)
    {
      _cleanup_(freep) const char *duration_str;

      r = rm_duration_to_string(status.maint_window_duration, &duration_str);
      if (r < 0)
	{
	  fprintf(stderr, _("Error converting duration to string: %s\n"),
		   strerror(-r));
	  return r;
	}

      printf("Start of maintenance window: %s\n", status.maint_window_start);
      printf("Duration of maintenance window: %s\n", duration_str);
    }

  return 0;
}

static int
dump_config(void)
{
  _cleanup_(freep) char *start_str = NULL;
  _cleanup_(freep) const char *duration_str = NULL;
  const char *strategy_str = NULL;
  RM_CTX ctx;
  int r;

  ctx.reboot_strategy = RM_REBOOTSTRATEGY_UNKNOWN;
  ctx.maint_window_duration = BAD_TIME;
  ctx.maint_window_start = NULL;

  log_init();

  r = load_config(&ctx);
  if (r < 0)
    return -r;

  if (ctx.reboot_strategy != RM_REBOOTSTRATEGY_UNKNOWN)
    {
      r = rm_strategy_to_str(ctx.reboot_strategy, &strategy_str);
      if (r < 0)
	{
	  fprintf(stderr, _("Converting strategy to string failed: %s\n"), strerror(-r));
	  return -1;
	}
    }
  else
    strategy_str = _("Not set");

  if (ctx.maint_window_start != NULL)
    {
      r = calendar_spec_to_string(ctx.maint_window_start, &start_str);
      if (r < 0)
	{
	  fprintf(stderr, _("Converting calendar entry to string failed: %s\n"), strerror(-r));
	  return -1;
	}
    }
  else
    start_str = strdup(_("Not set"));

  if (ctx.maint_window_duration != BAD_TIME)
    {
      r = rm_duration_to_string(ctx.maint_window_duration, &duration_str);
      if (r < 0)
	{
	  fprintf(stderr, _("Error converting duration to string: %s\n"), strerror(-r));
	  return -1;
	}
    }
  else
    duration_str = strdup(_("Not set"));

  printf ("strategy: %s\n", strategy_str);
  printf ("window-start: %s\n", start_str);
  printf ("window-duration: %s\n", duration_str);

  calendar_spec_free (ctx.maint_window_start);

  return 0;
}


static void
print_error(const econf_err error)
{
  char *filename = NULL;
  uint64_t line_nr = 0;

  econf_errLocation(&filename, &line_nr);
  fprintf(stderr, "%s (line %d): %s\n", filename, (int) line_nr,
          econf_errString(error));
  free(filename);
}


static inline void
free_groups(char ***groups)
{
  econf_free(*groups);
}

static econf_err
print_file_content(struct econf_file *key_file)
{
  _cleanup_(free_groups) char **groups = NULL;
  econf_ext_value *value = NULL;
  size_t groupCount = 0;
  econf_err econf_error;

  char *path = econf_getPath(key_file);
  if (strlen(path) > 0)
    fprintf(stderr, "%s:\n", path);
  free(path);

    /* show groups, keys and their value */
    econf_error = econf_getGroups(key_file, &groupCount, &groups);
    if (econf_error)
      {
        if (econf_error != ECONF_NOGROUP)
	  {
            print_error(econf_error);
            return econf_error;
	  }
	else
	  {
            /* no groups defined; generating an root entry */
            groups = calloc(1, sizeof(char*));
            groups[0] = NULL;
            groupCount = 1;
	  }
      }
    for (size_t g = 0; g < groupCount; g++)
      {
        char **keys = NULL;
        size_t key_count = 0;

        econf_error = econf_getKeys(key_file, groups[g], &key_count, &keys);
        if (econf_error)
	  {
            print_error(econf_error);
            econf_free(keys);
            return econf_error;
	  }

        if (groups[g] != NULL)
	  printf("[%s]\n", groups[g]);

        for (size_t k = 0; k < key_count; k++)
	  {
	    econf_error = econf_getExtValue(key_file, groups[g], keys[k], &value);
            if (econf_error)
	      {
		print_error(econf_error);
		econf_free(keys);
		return econf_error;
	      }
            if (value != NULL)
	      {
		size_t v = 0;
		printf("%s = ", keys[k]);
		while (value->values[v] != 0)
		  {
		    if (v==0)
		      printf("%s\n", value->values[v]);
		    else
		      printf("     %s\n", value->values[v]);
		    v++;
		  }
              }
	    econf_freeExtValue(value);
	  }
	printf("\n");
	econf_free(keys);
      }
    return ECONF_SUCCESS;
}

static int
dump_config_verbose(void)
{
  econf_file **key_files;
  econf_err econf_error;
  size_t size = 0;

  econf_error = econf_readDirsHistory(&key_files, &size,
				      CONFIGDIR, "/etc/rebootmgr", "rebootmgr",
				      "conf", "=", "#");
  if (econf_error)
    {
      print_error(econf_error);
      return -1;
    }

  for (size_t i=0; i < size; i++)
    {
      print_file_content(key_files[i]);
      econf_free(key_files[i]);
    }
  free(key_files);

  printf("Final result:\n");
  dump_config();

  return 0;
}


static void
usage(int exit_code)
{
  printf(_("Usage:\n"));
  printf(_("\trebootmgrctl --help|--version\n"));
  printf(_("\trebootmgrctl is-active [--quiet]\n"));
  printf(_("\trebootmgrctl reboot [now]\n"));
  printf(_("\trebootmgrctl soft-reboot [now]\n"));
  printf(_("\trebootmgrctl cancel\n"));
  printf(_("\trebootmgrctl status [--full|--quiet]\n"));
  printf(_("\trebootmgrctl set-strategy best-effort|maint-window|instantly|off\n"));
  printf(_("\trebootmgrctl get-strategy\n"));
  printf(_("\trebootmgrctl set-window <time> <duration>\n"));
  printf(_("\trebootmgrctl get-window\n"));
  printf(_("\trebootmgrctl dump-config\n"));
  exit(exit_code);
}

int
main(int argc, char **argv)
{
  int retval = 0;

  setlocale(LC_MESSAGES, "");
  setlocale(LC_CTYPE, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);

  if (argc < 2)
    usage(1);
  else if (argc == 2)
    {
      /* Parse commandline for help or version */
      if (strcmp("--version", argv[1]) == 0)
        {
          fprintf(stdout, _("rebootmgrctl (%s) %s\n"), PACKAGE, VERSION);
          exit(0);
        }
      else if (strcmp("--help", argv[1]) == 0)
	usage(0);
    }

  /* Continue parsing commandline. */
  if (strcasecmp("reboot", argv[1]) == 0)
    {
      RM_RebootMethod method = RM_REBOOTMETHOD_HARD;
      bool force = false;

      if (argc > 2)
	{
	  if (strcasecmp("now", argv[2]) == 0)
	    force = true;
	  else
	    usage(1);
	}
      retval = trigger_reboot(method, force);
    }
  else if (strcasecmp("soft-reboot", argv[1]) == 0)
    {
      RM_RebootMethod method = RM_REBOOTMETHOD_SOFT;
      bool force = false;

      if (argc > 2)
	{
	  if (strcasecmp("now", argv[2]) == 0)
	    force = true;
	  else
	    usage(1);
	}
      retval = trigger_reboot(method, force);
    }
  else if (strcasecmp("status", argv[1]) == 0)
    {
      int quiet = 0;
      int full = 0;
      RM_RebootStatus r_status = 0;
      RM_RebootMethod r_method = 0;
      _cleanup_(freep) char *r_time = NULL;

      if (argc == 3)
	{
	  if (strcasecmp("-q", argv[2]) == 0 ||
	      strcasecmp("--quiet", argv[2]) == 0)
	    quiet = 1;
	  if (strcasecmp("-f", argv[2]) == 0 ||
	      strcasecmp("--full", argv[2]) == 0)
	    full = 1;
	}
      else if (argc > 3)
	usage(1);

      if (full)
	{
	  int r = print_full_status();
	  if (r < 0)
	    retval = 1;
	}
      else
	{
	  int r = get_status(&r_status, &r_method, &r_time);
	  if (r < 0)
	    retval = 1;
	  else if (quiet)
	    retval = r_status;
	  else
	    {
	      if (r_status >= 0)
		{
		  const char *str;

		  r = rm_status_to_str(r_status, r_method, &str);
		  if (r < 0)
		    {
		      fprintf(stderr, _("Converting status to string failed: %s\n"),
			      strerror(-r));
		      retval = 1;
		    }
		  else
		    {
		      printf(_("Status: %s\n"), str);
		      if (r_time)
			printf(_("Scheduled for: %s\n"), r_time);
		    }
		}
	      else
		retval = 1;
	    }
	}
    }
  else if (strcasecmp("is-active", argv[1]) == 0)
    {
      int quiet = 0;
      RM_RebootStatus r_status = 0;
      RM_RebootMethod r_method = 0;

      if (argc > 2)
	if (strcasecmp("-q", argv[2]) == 0 ||
	    strcasecmp("--quiet", argv[2]) == 0)
	  quiet = 1;

      /* if we get an answer to a status request, rebootmgrd
	 must be active */
      int r = get_status(&r_status, &r_method, NULL);
      if (r == 0)
	{
	  if (quiet)
	    retval = 0;
	  else
	    printf("RebootMgr is active\n");
	}
      else
	{
	  if (quiet)
	    retval = 1;
	  else
	    printf("RebootMgr is not running\n");
	}
    }
  else if (strcasecmp("set-strategy", argv[1]) == 0)
    {
      RM_RebootStrategy strategy = RM_REBOOTSTRATEGY_BEST_EFFORT;

      if (argc > 2)
    	{
	  int r = rm_string_to_strategy(argv[2], &strategy);
	  if (r < 0)
	    usage(1);
	  retval = set_strategy(strategy);
	}
      else
	usage(1);
    }
  else if (strcasecmp("get-strategy", argv[1]) == 0)
    {
      struct status status = {
	.strategy = RM_REBOOTSTRATEGY_UNKNOWN,
      };
      int r;

      r = get_full_status(&status);
      if (r < 0)
	retval = 1;
      else
	{
	  const char *str = NULL;

	  r = rm_strategy_to_str(status.strategy, &str);
	  if (r < 0)
	    {
	      printf(_("Internal error, returned strategy is: %d\n"), status.strategy);
	      retval = 1;
	    }
	  else
	    printf(_("Reboot strategy: %s\n"), str);
	}
    }
  else if (strcasecmp("get-window", argv[1]) == 0)
    {
      struct status status = {
	.maint_window_start = NULL,
	.maint_window_duration = 0,
      };
      int r;

      r = get_full_status(&status);
      if (r < 0)
	retval = 1;
      else
	{
	  _cleanup_(freep) const char *duration_str = NULL;
	  r = rm_duration_to_string(status.maint_window_duration, &duration_str);
	  if (r < 0)
	    {
	      fprintf(stderr, _("Error converting duration to string: %s\n"),
		      strerror(-r));
	      retval = 1;
	    }
	  else
	    printf(_("Maintenance window is set to '%s', lasting %s.\n"),
		   status.maint_window_start, duration_str);
	}
    }
  else if (strcasecmp("set-window", argv[1]) == 0)
    {
      if (argc == 4)
	retval = set_window(argv[2], argv[3]);
      else
	usage(1);
    }
  else if (strcasecmp("cancel", argv[1]) == 0)
    retval = cancel_reboot();
  else if (strcasecmp("dump-config", argv[1]) == 0)
    {
      if (argc > 2 &&
	  (strcasecmp("-v", argv[2]) == 0 ||
	   strcasecmp("--verbose", argv[2]) == 0))
	retval = dump_config_verbose();
      else
	retval = dump_config();
    }
  else
    usage(1);

  return retval;
}
