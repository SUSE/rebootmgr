/* Copyright (c) 2016 Daniel Molkentin
   Author: Daniel Molkentin <dmolkentin@suse.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation in version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, see <http://www.gnu.org/licenses/>. */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "calendarspec.h"
#include "parse-duration.h"

#include "util.h"

#define DURATION_LEN 10

char* spec_to_string(CalendarSpec *spec)
{
  char *str_start;
  if (calendar_spec_to_string(spec, &str_start) > 0) {
    return 0;
  }
  return str_start;
}
char* duration_to_string(time_t duration)
{
  char buf[DURATION_LEN];
  char *p;
  if (strftime(buf, DURATION_LEN, "%Hh%Mm", gmtime(&duration)) == 0) {
    return 0;
  }
  p = malloc(DURATION_LEN);
  strncpy(p, buf, DURATION_LEN);

  return p;
}

RM_RebootStrategy
string_to_strategy(const char *str_strategy, int *error)
{
  if (error) {
    *error=0;
  }
  if (!str_strategy) {
    if (error) {
      *error=1;
    }
    return RM_REBOOTSTRATEGY_BEST_EFFORT;
  }

  if (strcasecmp (str_strategy, "best-effort") == 0 ||
      strcasecmp (str_strategy, "best_effort") == 0)
    return RM_REBOOTSTRATEGY_BEST_EFFORT;
  else if (strcasecmp (str_strategy, "instantly") == 0)
    return RM_REBOOTSTRATEGY_INSTANTLY;
  else if (strcasecmp (str_strategy, "maint_window") == 0 ||
     strcasecmp (str_strategy, "maint-window") == 0)
    return RM_REBOOTSTRATEGY_MAINT_WINDOW;
  else if (strcasecmp (str_strategy, "off") == 0)
    return RM_REBOOTSTRATEGY_OFF;

  if (error) {
    *error=1;
  }
  return RM_REBOOTSTRATEGY_BEST_EFFORT;
}

const char*
status_to_string(RM_RebootStatus status, int *error)
{
  if (error)
    *error=0;

  switch(status)
    {
    case RM_REBOOTSTATUS_NOT_REQUESTED:
      return "Reboot not requested";
    case RM_REBOOTSTATUS_REQUESTED:
      return "Reboot requested";
    case RM_REBOOTSTATUS_WAITING_WINDOW:
      return "Reboot requested, waiting for maintenance window";
    default:
      if (error)
	*error=1;
    }

  return "Unknown";
}

const char*
strategy_to_string(RM_RebootStrategy strategy, int *error)
{
  if (error) {
    *error=0;
  }
  switch(strategy) {
  case RM_REBOOTSTRATEGY_INSTANTLY:
    return "instantly";
  case RM_REBOOTSTRATEGY_MAINT_WINDOW:
    return "maint_window";
  case RM_REBOOTSTRATEGY_OFF:
    return "off";
  case RM_REBOOTSTRATEGY_UNKNOWN:
    // fall through, default to best-effort
  case RM_REBOOTSTRATEGY_BEST_EFFORT:
    return "best-effort";
  }

  if (error) {
    *error=1;
  }
  return "best-effort";
}
