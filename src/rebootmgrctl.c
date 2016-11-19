/* Copyright (c) 2016 Thorsten Kukuk
   Author: Thorsten Kukuk <kukuk@suse.de>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation in version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libintl.h>
#include <dbus/dbus.h>

#include "rebootmgr.h"

#ifndef _
#define _(String) gettext (String)
#endif

int debug_flag=1;

static void
usage (int exit_code)
{
  printf (_("Usage:\n"));
  printf (_("\trebootmgrctl --help|--version\n"));
  printf (_("\trebootmgrctl is-active [--quiet]\n"));
  printf (_("\trebootmgrctl reboot [fast|now]\n"));
  printf (_("\trebootmgrctl cancel\n"));
  printf (_("\trebootmgrctl status\n"));
  printf (_("\trebootmgrctl set-strategy best-efford|etcd-lock|maint-window|\n"));
  printf (_("\t                   instantly|off\n"));
  printf (_("\trebootmgrctl get-strategy\n"));
  printf (_("\trebootmgrctl set-window <time> <duration>\n"));
  printf (_("\trebootmgrctl get-window\n"));
  exit (exit_code);
}

/*
  check if rebootmgr is running.
  return values:
  0: not running
  1: running
*/
static int
rebootmgr_is_running (DBusConnection *connection)
{
  return dbus_bus_name_has_owner (connection, RM_DBUS_SERVICE, NULL);
}


static int
send_message_with_arg (DBusConnection *connection, const char *signal,
		       uint32_t arg)
{
  DBusMessage *message;
  int retval = 0;

  message = dbus_message_new_signal (RM_DBUS_PATH, RM_DBUS_INTERFACE,
				     signal);
  if (message == NULL)
    {
      fprintf (stderr, _("Out of memory!\n"));
      return 1;
    }

  dbus_message_append_args (message, DBUS_TYPE_UINT32, &arg,
			    DBUS_TYPE_INVALID);

  /* Send the signal */
  if (dbus_connection_send (connection, message, NULL) == FALSE)
    {
      fprintf (stderr, _("Out of memory!\n"));
      retval = 1;
    }
  dbus_message_unref (message);

  return retval;
}

static int
set_strategy (DBusConnection *connection, RM_RebootStrategy strategy)
{
  return send_message_with_arg (connection,
				RM_DBUS_SIGNAL_SET_STRATEGY, strategy);
 }

static int
trigger_reboot (DBusConnection *connection, RM_RebootOrder order)
{
  return send_message_with_arg (connection,
				RM_DBUS_SIGNAL_REBOOT, order);
}

static int
cancel_reboot (DBusConnection *connection)
{
  DBusMessage *message;
  int retval = 0;

  message = dbus_message_new_signal (RM_DBUS_PATH, RM_DBUS_INTERFACE,
				     RM_DBUS_SIGNAL_CANCEL);
  if (message == NULL)
    {
      fprintf (stderr, _("Out of memory!\n"));
      return 1;
    }
  /* Send the signal */
  if (dbus_connection_send (connection, message, NULL) == FALSE)
    {
      fprintf (stderr, _("Out of memory!\n"));
      retval = 1;
    }
  dbus_message_unref (message);

  return retval;
}

int
main (int argc, char **argv)
{
  DBusConnection *connection = NULL;
  DBusError error;
  int retval = 0;

  setlocale (LC_MESSAGES, "");
  setlocale (LC_CTYPE, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  if (argc < 2)
    usage (1);
  else if (argc == 2)
    {
      /* Parse commandline for help or version */
      if (strcmp ("--version", argv[1]) == 0)
        {
          fprintf (stdout, _("rebootmgrctl (%s) %s\n"), PACKAGE, VERSION);
          exit (0);
        }
      else if (strcmp ("--help", argv[1]) == 0)
	usage (0);
    }

  dbus_error_init (&error);
  connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
  if (!connection)
    {
      fprintf (stderr, "Failed to connect to the D-BUS daemon: %s",
	       error.message);
      dbus_error_free (&error);
      return 1;
    }

  /* Continue parsing commandline. */
  if (strcasecmp ("is-active", argv[1]) == 0 ||
      strcasecmp ("is_active", argv[1]) == 0)
    {
      if (rebootmgr_is_running (connection))
	{
	  printf ("RebootMgr is active\n");
	  retval = 0;
	}
      else
	{
	  printf ("RebootMgr is dead\n");
	  retval = 1;
	}
    }
  else if (strcasecmp ("reboot", argv[1]) == 0)
    {
      RM_RebootOrder order = RM_REBOOTORDER_STANDARD;

      if (argc > 2)
	{
	  if (strcasecmp ("fast", argv[2]) == 0)
	    order = RM_REBOOTORDER_FAST;
	  else if (strcasecmp ("now", argv[2]) == 0)
	    order = RM_REBOOTORDER_FORCED;
	  else
	    usage (1);
	}
      retval = trigger_reboot (connection, order);
    }
  else if (strcasecmp ("set-strategy", argv[1]) == 0 ||
	   strcasecmp ("set_strategy", argv[1]) == 0)
    {
      RM_RebootStrategy strategy = RM_REBOOTSTRATEGY_BEST_EFFORD;

      if (argc > 2)
	{
	  if (strcasecmp ("best-efford", argv[2]) == 0 ||
	      strcasecmp ("best_efford", argv[2]) == 0)
	    strategy = RM_REBOOTSTRATEGY_BEST_EFFORD;
	  else if (strcasecmp ("etcd-lock", argv[2]) == 0 ||
		   strcasecmp ("etcd_lock", argv[2]) == 0)
	    strategy = RM_REBOOTSTRATEGY_ETCD_LOCK;
	  else if (strcasecmp ("instantly", argv[2]) == 0)
	    strategy = RM_REBOOTSTRATEGY_INSTANTLY;
	  else if (strcasecmp ("maint-window", argv[2]) == 0 ||
		   strcasecmp ("maint_window", argv[2]) == 0)
	    strategy = RM_REBOOTSTRATEGY_INSTANTLY;
	  else if (strcasecmp ("off", argv[2]) == 0)
	    strategy = RM_REBOOTSTRATEGY_OFF;

	  else
	    usage (1);
	}
      retval = set_strategy (connection, strategy);
    }
  else if (strcasecmp ("cancel", argv[1]) == 0)
    retval = cancel_reboot (connection);
  else
    usage (1);

  dbus_connection_unref (connection);
  return retval;
}
