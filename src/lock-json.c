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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <json-c/json.h>

#include "log_msg.h"
#include "lock-json.h"

/* returns number of allowed locks or -1 in error case */
int64_t
get_max_locks (json_object *jobj)
{
  int64_t max_locks;
  json_object *maxobj = NULL;

  if (json_object_object_get_ex (jobj, "max", &maxobj) != TRUE)
    {
      log_msg (LOG_ERR, "json: key \"max\" not found");
      return -1;
    }

  errno = 0;
  max_locks = json_object_get_int64 (maxobj);
  if (errno != 0)
    {
      log_msg (LOG_ERR, "json: couldn't get max locks value: %m");
      return -1;
    }
  return max_locks;
}

/* returns number of current locks or -1 in error case */
int64_t
get_curr_locks (json_object *jobj)
{
   json_object *jarray = NULL;

  if (json_object_object_get_ex (jobj, "holders", &jarray) != TRUE)
    {
      log_msg (LOG_ERR, "json: key \"holders\" not found");
      return -1;
    }

  return json_object_array_length (jarray);
}

json_bool
add_id_to_holders (json_object *jobj, const char *id)
{
  json_object *jid;
  json_object *jarray = NULL;

  if (json_object_object_get_ex (jobj, "holders", &jarray) != TRUE)
    {
      log_msg (LOG_ERR, "json: Key \"holders\" not found");
      return FALSE;
    }

  jid = json_object_new_string(id);

  json_object_array_add(jarray,jid);

  return TRUE;
}

json_bool
remove_id_from_holders (json_object *jobj, const char *id)
{
  json_object *jarray = NULL;
  int64_t idx;

  if (json_object_object_get_ex (jobj, "holders", &jarray) != TRUE)
    {
      log_msg (LOG_ERR, "json: key \"holders\" not found");
      return FALSE;
    }

  /*Creating a json array*/
  json_object *jnew = json_object_new_array();

  for (idx = 0 ; idx < json_object_array_length (jarray) ; idx++ )
    {
      json_object *entry = json_object_array_get_idx (jarray , idx );
      const char *val = json_object_get_string (entry);

      if (strcmp (val, id) !=0)
	{
	  json_object_get (entry);
	  json_object_array_add (jnew, entry);
	}
    }

  /*Form the json object*/
  json_object_object_del (jobj, "holders");
  json_object_object_add (jobj,"holders", jnew);

  return TRUE;
}

json_bool
is_id_in_holders (json_object *jobj, const char *id)
{
  json_object *jarray = NULL;
  int64_t idx;

  if (json_object_object_get_ex (jobj, "holders", &jarray) != TRUE)
    {
      log_msg (LOG_ERR, "json: key \"holders\" not found");
      return FALSE;
    }

  for (idx = 0 ; idx < json_object_array_length (jarray) ; idx++ )
    {
      json_object *entry = json_object_array_get_idx (jarray , idx );
      const char *val = json_object_get_string (entry);

      if (strcmp (val, id) ==0)
	return TRUE;
    }

  return FALSE;
}
