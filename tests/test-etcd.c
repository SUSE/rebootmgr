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

#include <stdint.h>
#include <stdio.h>

#include "lock-etcd.h"

#define ETCD_LOCKS_DEFAULT_GROUP "rebootmgr-testsuite"

int
main (int argc, char *argv[])
{
  if (!etcd_is_running())
    {
      printf ("etcd not running\n");
      return 77;
    }

  printf ("etcd is running, try to get a lock\n");

  if (etcd_get_lock (ETCD_LOCKS_DEFAULT_GROUP, NULL) != 0)
    {
      fprintf (stderr, "etcd_get_lock() failed!\n");
      return 1;
    }

  printf ("got a lock, check if we own it\n");

  if (!etcd_own_lock (ETCD_LOCKS_DEFAULT_GROUP) != 0)
    {
      fprintf (stderr, "Don't own a lock!\n");
      return 1;
    }

  printf ("have a lock, release it\n");

  if (etcd_release_lock (ETCD_LOCKS_DEFAULT_GROUP, NULL) != 0)
    {
      fprintf (stderr, "etcd_release_lock() failed!\n");
      return 1;
    }

  printf ("have no lock, check that we don't own one\n");

  if (etcd_own_lock (ETCD_LOCKS_DEFAULT_GROUP) != 0)
    {
      fprintf (stderr, "Still own a lock!\n");
      return 1;
    }

  return 0;
}
