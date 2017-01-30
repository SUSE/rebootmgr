/* Copyright (c) 2017 Thorsten Kukuk
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

#ifndef _LOCK_ETCD_H_
#define _LOCK_ETCD_H_

#define ETCD_LOCKS "/opensuse.org/rebootmgr/locks"

int etcd_is_running (void);
int etcd_get_lock (const char *group, const char *machine_id);
int etcd_release_lock (const char *group, const char *machine_id);
int etcd_own_lock (const char *group);
int etcd_set_max_locks (const char *group, int64_t max_locks);
char *etcd_get_data_value (const char *group);

#endif /* _LOCK_ETCD_H_ */
