/* Parse a time duration and return a seconds count
   Copyright (C) 2008-2014 Free Software Foundation, Inc.
   Written by Bruce Korb <bkorb@gnu.org>, 2008.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/*
   ==== The term may be followed by any of these formats:
   hhmmss
   hh:mm:ss
   hh H mm M ss S
*/

#ifndef _PARSE_DURATION_H
#define _PARSE_DURATION_H

#include <time.h>

/* Return value when a duration cannot be parsed. */
#define BAD_TIME	((time_t)~0)

time_t parse_duration (const char *);

#endif /* _PARSE_DURATION_H */
