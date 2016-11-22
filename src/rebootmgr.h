/* Copyright (c) 2016 Thorsten Kukuk
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

#ifndef _REBOOTMGR_H_
#define _REBOOTMGR_H_

#define RM_DBUS_NAME		 "org.opensuse.rebootmgr"
#define RM_DBUS_INTERFACE	 "org.opensuse.rebootmgr"
#define RM_DBUS_PATH_SERVER	 "/"
#define RM_DBUS_PATH      	"/org/opensuse/rebootmgr"
#define RM_DBUS_SIGNAL_REBOOT 		"Reboot"
#define RM_DBUS_SIGNAL_STATUS 		"Status"
#define RM_DBUS_SIGNAL_CANCEL 		"Cancel"
#define RM_DBUS_SIGNAL_SET_STRATEGY	"SetStrategy"
#define RM_DBUS_SIGNAL_GET_STRATEGY	"GetStrategy"

typedef enum RM_RebootOrder {
  RM_REBOOTORDER_UNKNOWN = 0,
  RM_REBOOTORDER_STANDARD, /* Follow normal reboot strategy */
  RM_REBOOTORDER_FAST,     /* Ignore maintenance window, but
			      get locks first */
  RM_REBOOTORDER_FORCED    /* Reboot immeaditly, without
                              waiting for maintenance window */
} RM_RebootOrder;

typedef enum RM_RebootStrategy {
  RM_REBOOTSTRATEGY_UNKNOWN = 0,
  RM_REBOOTSTRATEGY_BEST_EFFORD, /* etcd-lock if available, else 
			            maintenance window, else instantly */
  RM_REBOOTSTRATEGY_INSTANTLY,	 /* reboot instantly */
  RM_REBOOTSTRATEGY_MAINT_WINDOW,/* reboot only during maintenance window */
  RM_REBOOTSTRATEGY_ETCD_LOCK,   /* acquire etcd lock before reboot */
  RM_REBOOTSTRATEGY_OFF          /* don't reboot */
} RM_RebootStrategy;

#endif /* _REBOOTMGR_H_ */
