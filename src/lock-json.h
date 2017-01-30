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

#ifndef _LOCK_JSON_H_
#define _LOCK_JSON_H_

int64_t get_max_locks (json_object *jobj);
int64_t get_curr_locks (json_object *jobj);
json_bool set_max_locks (json_object *jobj, int64_t max_locks);
json_bool add_id_to_holders (json_object *jobj, const char *id);
json_bool remove_id_from_holders (json_object *jobj, const char *id);
json_bool is_id_in_holders (json_object *jobj, const char *id);

#endif /* _LOCK_JSON_H_ */
