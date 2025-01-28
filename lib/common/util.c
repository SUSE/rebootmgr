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

#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <libintl.h>

#include "common.h"

#ifndef _
#define _(String) gettext(String)
#endif

const char *
bool_to_str (bool var)
{
  return var?"true":"false";
}

int
rm_duration_to_string (time_t duration, const char **ret)
{
  char buf[18];
  struct tm tm;
  const char *fmt;

  if (gmtime_r (&duration, &tm) == NULL)
    return -errno;

  if (tm.tm_sec > 0)
    fmt = "%T";
  else
    fmt = "%R";

  size_t len = strftime (buf, sizeof (buf), fmt, &tm);
  if (len == 0)
    {
      *ret = NULL;
      return -ENOSPC;
    }

  char *p = malloc (len + 1);
  if (p == NULL)
    return -ENOMEM;

  strcpy(p, buf);

  *ret = p;

  return 0;
}

int
rm_string_to_strategy (const char *str_strategy, RM_RebootStrategy *ret)
{
  *ret = RM_REBOOTSTRATEGY_UNKNOWN;
  if (!str_strategy)
    return -EINVAL;

  if (strcasecmp (str_strategy, "best-effort") == 0)
    *ret = RM_REBOOTSTRATEGY_BEST_EFFORT;
  else if (strcasecmp (str_strategy, "instantly") == 0)
    *ret = RM_REBOOTSTRATEGY_INSTANTLY;
  else if (strcasecmp (str_strategy, "maint-window") == 0)
    *ret = RM_REBOOTSTRATEGY_MAINT_WINDOW;
  else if (strcasecmp (str_strategy, "off") == 0)
    *ret = RM_REBOOTSTRATEGY_OFF;

  return 0;
}

int
rm_status_to_str (RM_RebootStatus status, RM_RebootMethod method,
		  const char **ret)
{
  switch (status)
    {
    case RM_REBOOTSTATUS_NOT_REQUESTED:
      *ret = _("Reboot not requested");
      break;
    case RM_REBOOTSTATUS_REQUESTED:
      if (method == RM_REBOOTMETHOD_SOFT)
	*ret = _("Soft-reboot requested");
      else
	*ret = _("Reboot requested");
      break;
    case RM_REBOOTSTATUS_WAITING_WINDOW:
      if (method == RM_REBOOTMETHOD_SOFT)
	*ret = _("Soft-reboot requested, waiting for maintenance window");
      else
	*ret = _("Reboot requested, waiting for maintenance window");
      break;
    default:
      *ret = NULL;
      return -EINVAL;
    }
  return 0;
}

int
rm_strategy_to_str (RM_RebootStrategy strategy, const char **ret)
{
  switch (strategy) {
  case RM_REBOOTSTRATEGY_BEST_EFFORT:
    *ret = "best-effort";
    break;
  case RM_REBOOTSTRATEGY_INSTANTLY:
    *ret = "instantly";
    break;
  case RM_REBOOTSTRATEGY_MAINT_WINDOW:
    *ret = "maint-window";
    break;
  case RM_REBOOTSTRATEGY_OFF:
    *ret = "off";
    break;
  case RM_REBOOTSTRATEGY_UNKNOWN:
    /* fall through, not a valid entry, too */
  default:
    *ret = "unknown";
    return -EINVAL;
  }
  return 0;
}

int
rm_method_to_str (RM_RebootMethod method, const char **ret)
{
  switch (method) {
  case RM_REBOOTMETHOD_HARD:
    *ret = "reboot";
    break;
  case RM_REBOOTMETHOD_SOFT:
    *ret = "soft-reboot";
    break;
  case RM_REBOOTMETHOD_UNKNOWN:
  default:
    *ret = "unknown";
    return -EINVAL;
  }
  return 0;
}
