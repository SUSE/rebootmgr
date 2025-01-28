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

#pragma once

#include "rebootmgr.h"

/* generic functions */
extern int mkdir_p(const char *path, mode_t mode);

/* config file related functions */
#define RM_GROUP "rebootmgr"
extern int load_config(RM_CTX *ctx);
extern int save_config(RM_RebootStrategy reboot_strategy,
            	       CalendarSpec *maint_window_start,
                       time_t maint_window_duration);

/* logging */
#include <syslog.h>
extern int debug_flag;
extern void log_init (void);
extern void log_msg (int priority, const char *fmt, ...);

/* various functions to convert to/from strings */ 
const char *bool_to_str(bool var);
int rm_duration_to_string(time_t duration, const char **ret);
int rm_string_to_strategy(const char *str_strategy, RM_RebootStrategy *ret);
int rm_strategy_to_str(RM_RebootStrategy strategy, const char **ret);
int rm_status_to_str(RM_RebootStatus status, RM_RebootMethod method,
		     const char **ret);
int rm_method_to_str(RM_RebootMethod method, const char **ret);
