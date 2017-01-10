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
#include <string.h>
#include <stdarg.h>
#include <cetcd.h>
#include <json-c/json.h>

#include "lock-etcd.h"
#include "lock-json.h"

#define INIT_LOCK_DATA "{\n    \"max\": 1,\n    \"holders\": []\n}"

static int debug_flag = 1;

static void
debug (const char *fmt,...)
{
  char string[400];
  va_list ap;

  if (!debug_flag)
    return;

  va_start (ap, fmt);
  vsnprintf (string, sizeof (string), fmt, ap);
  va_end (ap);

  fprintf (stderr, "%s\n", string);
}

static const char *
get_machine_id (void)
{
  static const char *machine_id = NULL;

  if (machine_id == NULL)
    {
      char *buffer = NULL;
      FILE *fp = fopen ("/etc/machine-id", "rb");

      if (fp)
	{
	  /*read text until newline */
	  fscanf (fp,"%m[^\n]", &buffer);
	  fclose (fp);
	  machine_id = buffer;
	}
    }

  return machine_id;
}

/* return value: 0 success, 1 error */
static int
watch_key (cetcd_client *cli, const char *group, const char *name, uint64_t index)
{
  cetcd_response *resp;
  char *key = NULL;
  int retval = 0;

  debug ("watch key '%s' of group '%s' (index %lu)", name, group, index);

  if (asprintf (&key, "%s/%s/%s", ETCD_LOCKS, group, name) == -1)
    return 1;

  resp = cetcd_watch (cli, key, index);
  if (resp->err)
    {
      fprintf (stderr, "error: %d, %s (%s)\n", resp->err->ecode,
	       resp->err->message, resp->err->cause);
      retval = 1;
    }
  free (key);
  cetcd_response_release (resp);

  return retval;
}

static char *
get_value (cetcd_client *cli, const char *group, const char *name)
{
  char *retval = NULL;
  cetcd_response *resp;
  char *key = NULL;

  if (asprintf (&key, "%s/%s/%s", ETCD_LOCKS, group, name) == -1)
    return NULL;
  resp = cetcd_get (cli, key);
  if (resp->err)
    fprintf (stderr, "Error getting value (%s):%d, %s (%s)\n", key,
	     resp->err->ecode, resp->err->message, resp->err->cause);
  else
    retval = strdup (resp->node->value);

  cetcd_response_release (resp);
  free (key);

  return retval;
}

static int
set_lock_key (cetcd_client *cli, const char *group,
	      const char *name, const char *value)
{
  cetcd_response *resp;
  char *key = NULL;

  if (asprintf (&key, "%s/%s/%s", ETCD_LOCKS, group, name) == -1)
    return 1;
  resp = cetcd_set (cli, key, value, 0);
  if (resp->err)
    fprintf (stderr, "Error creating key (%s):%d, %s (%s)\n", key,
	     resp->err->ecode, resp->err->message, resp->err->cause);
  cetcd_response_release (resp);
  free (key);

  return 0;
}

static int
create_lock_dir (cetcd_client *cli, const char *group)
{
  debug ("create_lock_dir for group '%s' called", group);

  if (set_lock_key (cli, group, "mutex", "0") != 0)
    return 1;
  if (set_lock_key (cli, group, "data", INIT_LOCK_DATA) != 0)
    return 1;

  return 0;
}

static int
get_mutex (cetcd_client *cli, const char *group)
{
  cetcd_response *resp;
  char *path = NULL;
  uint64_t index = 0;

  debug ("get_mutex for group '%s' called", group);

  if (asprintf (&path, "%s/%s/mutex", ETCD_LOCKS, group) == -1)
    return 1;

  resp = cetcd_cmp_and_swap(cli, path, "1", "0", 0);
  index = resp->etcd_index+1;
  if (resp->err)
    {
      debug ("cmp_and_swap failed: %d, %s (%s)", resp->err->ecode,
	     resp->err->message, resp->err->cause);
      if (resp->err->ecode != 101)
	fprintf (stderr, "error: %d, %s (%s)\n", resp->err->ecode,
		 resp->err->message, resp->err->cause);
      cetcd_response_release (resp);

      /* Wait that the mutex key changes */
      watch_key (cli, group, "mutex", index);
      return 1;
    }
  else
    {
      char *val;

      cetcd_response_release (resp);

      /* check if the mutex is really set, not really needed */
      val = get_value (cli, group, "mutex");
      if (strcmp (val, "1") != 0)
	{
	  fprintf (stderr, "cetcd_cmp_and_swap succeeded, but no lock set?\n");
	  return 1;
	}

      debug ("got mutex for group '%s'", group);

    }
  return 0;
}

static int
release_mutex (cetcd_client *cli, const char *group)
{
  if (set_lock_key (cli, group, "mutex", "0") != 0)
    return 1;
  return 0;
}

/*
  return values:
  0: success
  1: error
*/
int
get_lock (const char *group)
{
  cetcd_client cli;
  cetcd_response *resp;
  cetcd_array addrs;
  char *path = NULL;
  int retval = 1;
  int have_lock = 0;

  cetcd_array_init (&addrs, 3);
  cetcd_array_append (&addrs, "http://127.0.0.1:2379");

  cetcd_client_init (&cli, &addrs);

  /* Check if the data structure for the locks exists, else create them */
  if (asprintf (&path, "%s/%s/mutex", ETCD_LOCKS, group) == -1)
    return 1;
  resp = cetcd_get (&cli, path);
  if (resp->err)
    {
      if (resp->err->ecode == 100)
	create_lock_dir (&cli, group);
      else
	{
	  fprintf (stderr, "error: %d, %s (%s)\n", resp->err->ecode,
		   resp->err->message, resp->err->cause);
	  return 1;
	}
    }
  cetcd_response_release (resp);

  /* try in a loop to get a mutex with current locks lower than max locks */
  while (!have_lock)
    {
      json_object *jobj;
      int64_t max_locks;
      int64_t curr_locks;
      char *val;

      val = get_value (&cli, group, "data");
      if (val == NULL)
	goto cleanup;

      jobj = json_tokener_parse (val);
      if (jobj == NULL)
	goto cleanup;
      max_locks = get_max_locks (jobj);
      curr_locks = get_curr_locks (jobj);
      json_object_put (jobj);

      if (curr_locks < max_locks)
	{
	  if (get_mutex (&cli, group) != 0)
	    {
	      /* either we got a lock but etcd data was not changed (what should never
		 happen), or the mutex was hold by somebody else and the watch reported
		 that this changed, so try again. */
	      debug ("get_lock: get_mutex for group '%s' failed, trying again.", group);
	      continue;
	    }

	  /* we have the mutex, check if we can set the lock */
	  val = get_value (&cli, group, "data");
	  if (val == NULL)
	    goto leave;

	  jobj = json_tokener_parse (val);
	  if (jobj == NULL)
	    goto leave;
	  max_locks = get_max_locks (jobj);
	  curr_locks = get_curr_locks (jobj);

	  if (curr_locks < max_locks)
	    {
	      add_id_to_holders (jobj, get_machine_id());
	      set_lock_key (&cli, group, "data",
			    json_object_to_json_string_ext (jobj,
							    JSON_C_TO_STRING_PRETTY));
	      have_lock = 1;
	      retval = 0;
	    }
	leave:
	  if (jobj != NULL)
	    json_object_put (jobj);
	  release_mutex (&cli, group);
	}
      else
	{
	  debug ("max locks reached for group '%s'", group);
	  if (watch_key (&cli, group, "count", 0 /* XXX */) != 0)
	    goto cleanup;
	}
    }

 cleanup:
  cetcd_array_destroy (&addrs);
  cetcd_client_destroy (&cli);

  return retval;
}

/*
  return values:
  0: success
  1: error
*/
int
release_lock (const char *group)
{
  cetcd_client cli;
  cetcd_array addrs;
  int retval = 1;
  int removed_lock = 0;

  if (!own_lock (group))
    return 0;

  cetcd_array_init (&addrs, 3);
  cetcd_array_append (&addrs, "http://127.0.0.1:2379");

  cetcd_client_init (&cli, &addrs);

  /* try in a loop to get a mutex */
  while (!removed_lock)
    {
      json_object *jobj;
      char *val;

      if (get_mutex (&cli, group) != 0)
	{
	  /* either we got a lock but etcd data was not changed (what should never
	     happen), or the mutex was hold by somebody else and the watch reported
	     that this changed, so try again. */
	  debug ("release_lock: get_mutex for group '%s' failed, trying again.", group);
	  continue;
	}

      val = get_value (&cli, group, "data");
      if (val == NULL)
	{
	  release_mutex (&cli, group);
	  goto cleanup;
	}

      jobj = json_tokener_parse (val);
      if (jobj == NULL)
	{
	  release_mutex (&cli, group);
	  goto cleanup;
	}

      remove_id_from_holders (jobj, get_machine_id());
      set_lock_key (&cli, group, "data",
		    json_object_to_json_string_ext (jobj,
						    JSON_C_TO_STRING_PRETTY));
      removed_lock = 1;
      retval = 0;
      release_mutex (&cli, group);
    }

 cleanup:
  cetcd_array_destroy (&addrs);
  cetcd_client_destroy (&cli);

  return retval;
}

/*
  return values:
  1: own a lock
  0: don't own a lock
*/
int
own_lock (const char *group)
{
  cetcd_client cli;
  cetcd_response *resp;
  cetcd_array addrs;
  char *path = NULL;
  int have_lock = 0;

  cetcd_array_init (&addrs, 3);
  cetcd_array_append (&addrs, "http://127.0.0.1:2379");

  cetcd_client_init (&cli, &addrs);

  /* Check if the data structure for the locks exists, else create them */
  if (asprintf (&path, "%s/%s/data", ETCD_LOCKS, group) == -1)
    return 1;
  resp = cetcd_get (&cli, path);
  free (path);
  if (resp->err)
    {
      if (resp->err->ecode == 100)
	create_lock_dir (&cli, group);
      else
	{
	  fprintf (stderr, "error: %d, %s (%s)\n", resp->err->ecode,
		   resp->err->message, resp->err->cause);
	}
    }
  else
    {
      json_object *jobj = json_tokener_parse (resp->node->value);

      have_lock = is_id_in_holders (jobj, get_machine_id ());

      json_object_put (jobj);
    }

  cetcd_response_release (resp);

  return have_lock;
}

/*
  check if etcd is running.
  return values:
  0: not running
  1: running
*/
int
etcd_is_running (void)
{
  int retval = 1;
  cetcd_client cli;
  cetcd_response *resp;
  cetcd_array addrs;

  cetcd_array_init (&addrs, 3);
  cetcd_array_append (&addrs, "http://127.0.0.1:2379");
  cetcd_client_init (&cli, &addrs);

  resp = cetcd_get (&cli, "/");
  if (resp->err && resp->err->ecode == 1002)
    retval = 0;
  cetcd_response_release (resp);

  cetcd_array_destroy (&addrs);
  cetcd_client_destroy (&cli);

  return retval;
}
