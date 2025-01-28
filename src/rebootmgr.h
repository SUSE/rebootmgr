/* Copyright (c) 2016, 2017 Thorsten Kukuk
   Author: Thorsten Kukuk <kukuk@suse.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation in version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, see <http://www.gnu.org/licenses/>. */

#pragma once

#include <systemd/sd-event.h>
#include "calendarspec.h"

#define RM_VARLINK_SOCKET_DIR   "/run/rebootmgr"
#define RM_VARLINK_SOCKET       RM_VARLINK_SOCKET_DIR"/rebootmgrd.socket"

typedef enum RM_RebootMethod {
  RM_REBOOTMETHOD_UNKNOWN = 0,
  RM_REBOOTMETHOD_HARD, /* Normal hard/full reboot */
  RM_REBOOTMETHOD_SOFT, /* systemd soft-reboot, only userland */
} RM_RebootMethod;

typedef enum RM_RebootStrategy {
  RM_REBOOTSTRATEGY_UNKNOWN = 0,
  RM_REBOOTSTRATEGY_BEST_EFFORT, /* maintenance window, else instantly */
  RM_REBOOTSTRATEGY_INSTANTLY,	 /* reboot instantly */
  RM_REBOOTSTRATEGY_MAINT_WINDOW,/* reboot only during maintenance window */
  RM_REBOOTSTRATEGY_OFF          /* don't reboot */
} RM_RebootStrategy;

typedef enum RM_RebootStatus {
  RM_REBOOTSTATUS_NOT_REQUESTED = 0,
  RM_REBOOTSTATUS_REQUESTED,
  RM_REBOOTSTATUS_WAITING_WINDOW,
} RM_RebootStatus;

typedef struct {
  RM_RebootStatus reboot_status;
  RM_RebootMethod reboot_method;
  RM_RebootStrategy reboot_strategy;
  CalendarSpec *maint_window_start;
  time_t maint_window_duration;
  int temp_off;
  sd_event *loop;
  sd_event_source *timer;
  usec_t reboot_time;
} RM_CTX;

