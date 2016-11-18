/*  Copyright (C) 2016 Thorsten Kukuk
    Author: Thorsten Kukuk <kukuk@suse.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */


#ifndef _LOG_MSG_H
#define _LOG_MSG_H 1

#include <syslog.h>

extern int debug_flag;
extern int logfile_flag;

extern void log_msg (int type, const char *, ...);
extern void close_logfile (void);
extern void log2file (const char *);

#endif
