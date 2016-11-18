/* Copyright (C) 2016 Thorsten Kukuk
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

#ifndef _REBOOTMGR_H_
#define _REBOOTMGR_H_

#define RM_DBUS_INTERFACE "org.opensuse.rebootmgr"
#define RM_DBUS_SERVICE   "org.opensuse.rebootmgr"
#define RM_DBUS_PATH      "/org/opensuse/rebootmgr"
#define RM_DBUS_SIGNAL_REBOOT "Reboot"
#define RM_DBUS_SIGNAL_STATUS "Status"
#define RM_DBUS_SIGNAL_CANCEL "Cancel"

typedef enum RMRebootOrder {
  RM_REBOOTORDER_UNKNOWN = 0,
  RM_REBOOTORDER_STANDARD,               /* Reboot at next possible time */
  RM_REBOOTORDER_FAST,                       /* Reboot as fast as possible, but
						                  get locks first */
  RM_REBOOTORDER_FORCED                    /* Reboot immeaditly, without
                                                                  waiting for maintenance window */
} RMRebootOrder;


#endif /* _REBOOTMGR_H_ */
