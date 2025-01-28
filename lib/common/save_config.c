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

#include <errno.h>
#include <string.h>
#include <libeconf.h>

#include "basics.h"
#include "common.h"
#include "rebootmgr.h"

#define RM_DROPIN_DIR "/etc/rebootmgr/rebootmgr.conf.d"

int
save_config(RM_RebootStrategy reboot_strategy,
	    CalendarSpec *maint_window_start,
	    time_t maint_window_duration)
{
  const char *dropin = NULL;
  _cleanup_(econf_freeFilep) econf_file *key_file = NULL;
  econf_err error;
  int r;

  r = mkdir_p(RM_DROPIN_DIR, 0755);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Cannot create '"RM_DROPIN_DIR"' directory: %s",
	      strerror(-r));
      return -1;
    }

  if ((error = econf_newKeyFile(&key_file, '=', '#')))
    {
      log_msg(LOG_ERR, "Cannot create new config file: %s",
	      econf_errString(error));
      return -1;
    }

  if (reboot_strategy != RM_REBOOTSTRATEGY_UNKNOWN)
    {
      const char *strategy_str = NULL;

      dropin = "50-strategy.conf";
      r = rm_strategy_to_str(reboot_strategy, &strategy_str);
      if (r < 0)
	{
	  log_msg(LOG_ERR, "Converting strategy to string failed: %s", strerror(-r));
	  return -1;
	}
      error = econf_setStringValue(key_file, RM_GROUP, "strategy", strategy_str);
      if (error)
	{
	  log_msg(LOG_ERR, "Error setting variable 'strategy': %s\n", econf_errString(error));
	  return -1;
	}
    }
  else if (maint_window_start != NULL)
    {
      _cleanup_(freep) char *start_str = NULL;
      _cleanup_(freep) const char *duration_str = NULL;

      dropin = "50-maintenance-window.conf";
      r = calendar_spec_to_string(maint_window_start, &start_str);
      if (r < 0)
	{
	  log_msg(LOG_ERR, "Converting calendar entry to string failed: %s", strerror(-r));
	  return -1;
	}

      error = econf_setStringValue(key_file, RM_GROUP, "window-start", start_str);
      if (error)
	{
	  log_msg(LOG_ERR, "Error setting variable 'window-start': %s\n", econf_errString(error));
	  return -1;
	}

      r = rm_duration_to_string(maint_window_duration, &duration_str);
      if (r < 0)
	{
	  log_msg(LOG_ERR, "Error converting duration to string: %s", strerror(-r));
	  return -1;
	}
      error = econf_setStringValue(key_file, RM_GROUP, "window-duration", duration_str);
      if (error)
	{
	  log_msg(LOG_ERR, "Error setting variable 'window-duration': %s\n", econf_errString(error));
	  return -1;
	}
    }

  if ((error = econf_writeFile(key_file, RM_DROPIN_DIR, dropin)))
    {
      log_msg(LOG_ERR, "Error writing '"RM_DROPIN_DIR"/%s': %s", dropin, econf_errString(error));
      return -1;
    }

  return 0;
}
