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

#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <systemd/sd-journal.h>

#include "common.h"

static int is_tty = 1;
int debug_flag = 0;

void
log_init (void)
{
  is_tty = isatty (STDOUT_FILENO);
}

void
log_msg (int priority, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);

  if (is_tty || debug_flag)
    {
      if (priority == LOG_ERR)
	{
	  vfprintf (stderr, fmt, ap);
	  fputc ('\n', stderr);
	}
      else
	{
	  vprintf (fmt, ap);
	  putchar ('\n');
	}
    }
  else
    sd_journal_printv (priority, fmt, ap);

  va_end (ap);
}
