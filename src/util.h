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

#include "rebootmgr.h"
#include "calendarspec.h"

#ifndef UTIL_H
#define UTIL_H

char* spec_to_string(CalendarSpec *spec);
char* duration_to_string(time_t duration);
RM_RebootStrategy string_to_strategy(const char *str_strategy, int *error);
const char* strategy_to_string(RM_RebootStrategy strategy, int *error);

#endif // UTIL_H
