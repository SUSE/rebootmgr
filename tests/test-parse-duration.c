/* Copyright (c) 2016 Thorsten Kukuk
   Author: Thorsten Kukuk <kukuk@suse.com>

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

#include <assert.h>
#include <stdio.h>
#include "parse-duration.h"

int
main (void)
{
  assert (parse_duration ("1h30s") == 3630);
  assert (parse_duration ("1:00") == 3600);
  assert (parse_duration (" 1: 0") == 3600);
  assert (parse_duration ("  1:0  ") == 3600);
  assert (parse_duration ("01:0:30") == 3630);

  return 0;
}
