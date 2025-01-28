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

#include <errno.h>
#include <string.h>
#include <libeconf.h>

#include "basics.h"
#include "common.h"
#include "parse-duration.h"

static econf_err
open_config_file(econf_file **key_file)
{
  return econf_readConfig(key_file,
			  PACKAGE,
			  DATADIR,
			  "rebootmgr",
			  "conf", "=", "#");
}

int
load_config(RM_CTX *ctx)
{
  _cleanup_(econf_freeFilep) econf_file *key_file = NULL;
  econf_err error;
  int r;

  error = open_config_file(&key_file);
  if (error)
    {
      /* ignore if there is no configuration file at all */
      if (error == ECONF_NOFILE)
	return 0;

      log_msg(LOG_ERR, "econf_readConfig: %s\n",
	      econf_errString(error));
      return -1;
    }

  if (key_file == NULL) /* can this happen? */
      log_msg(LOG_ERR, "Cannot load 'rebootmgrd.conf'");
  else
    {
      _cleanup_(freep) char *str_start = NULL, *str_duration = NULL, *str_strategy = NULL;

      error = econf_getStringValue(key_file, RM_GROUP, "window-start", &str_start);
      if (error && error != ECONF_NOKEY)
	{
	  log_msg(LOG_ERR, "ERROR (econf): cannot get key 'window-start': %s",
		  econf_errString(error));
	  return -1;
	}
      error = econf_getStringValue(key_file, RM_GROUP, "window-duration", &str_duration);
      if (error && error != ECONF_NOKEY)
	{
	  log_msg(LOG_ERR, "ERROR (econf): cannot get key 'window-duration': %s",
		  econf_errString(error));
	  return -1;
	}

      error = econf_getStringValue(key_file, RM_GROUP, "strategy", &str_strategy);
      if (error && error != ECONF_NOKEY)
	{
	  log_msg(LOG_ERR, "ERROR (econf): cannot get key 'strategy': %s",
		  econf_errString(error));
	  return -1;
	}

      RM_RebootStrategy new_strategy = RM_REBOOTSTRATEGY_UNKNOWN;
      if (str_strategy != NULL)
	{
	  r = rm_string_to_strategy(str_strategy, &new_strategy);
	  if (r < 0)
	    {
	      log_msg(LOG_ERR, "ERROR: cannot parse strategy (%s): %s",
		      str_strategy, strerror(-r));
	      return -1;
	    }
	}

      CalendarSpec *new_start = NULL;
      if (str_start != NULL)
	{
	  r = calendar_spec_from_string(str_start, &new_start);
	  if (r < 0)
	    {
	      log_msg(LOG_ERR, "ERROR: cannot parse window-start (%s): %s",
		      str_start, strerror(-r));
	      return -1;
	    }
	}

      time_t new_duration = BAD_TIME;
      if (str_duration != NULL)
	{
	  if ((new_duration = parse_duration(str_duration)) == BAD_TIME)
	    {
	      log_msg(LOG_ERR, "ERROR: cannot parse window-duration (%s)",
		      str_duration);
	      return -1;
	    }
	}

      if (new_strategy != RM_REBOOTSTRATEGY_UNKNOWN)
	ctx->reboot_strategy = new_strategy;
      if (new_start != NULL)
	{
	  calendar_spec_free(ctx->maint_window_start);
	  ctx->maint_window_start = new_start;
	}
      if (new_duration != BAD_TIME)
	ctx->maint_window_duration = new_duration;
    }
  return 0;
}
