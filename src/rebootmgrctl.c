/* Copyright (c) 2016, 2017, 2018, 2019 Thorsten Kukuk
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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libintl.h>
#include <dbus/dbus.h>

#include "rebootmgr.h"
#include "util.h"
#include "parse-duration.h"

static void
usage (int exit_code)
{
  printf (_("Usage:\n"));
  printf (_("\trebootmgrctl --help|--version\n"));
  printf (_("\trebootmgrctl is-active [--quiet]\n"));
  printf (_("\trebootmgrctl reboot [fast|now]\n"));
  printf (_("\trebootmgrctl cancel\n"));
  printf (_("\trebootmgrctl status [--quiet]\n"));
  printf (_("\trebootmgrctl set-strategy best-effort|maint-window|instantly|off\n"));
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
  return dbus_bus_name_has_owner (connection, RM_DBUS_NAME, NULL);
}


static int
send_message_with_arg (DBusConnection *connection, const char *method,
		       uint32_t arg)
{
  DBusMessage *message;
  int retval = 0;
  message = dbus_message_new_method_call (RM_DBUS_NAME,
					  RM_DBUS_PATH,
					  RM_DBUS_INTERFACE,
					  method);
  if (message == NULL)
    {
      fprintf (stderr, _("Out of memory!\n"));
      return 1;
    }

  dbus_message_append_args (message, DBUS_TYPE_UINT32, &arg,
			    DBUS_TYPE_INVALID);

  /* Send the call */
  if (dbus_connection_send (connection, message, NULL) == FALSE)
    {
      fprintf (stderr, _("Out of memory!\n"));
      retval = 1;
    }
  dbus_message_unref (message);

  return retval;
}

static int
set_window (DBusConnection *connection, const char* start, const char* duration)
{
  DBusMessage *message;
  int retval = 0;
  message = dbus_message_new_method_call (RM_DBUS_NAME,
					  RM_DBUS_PATH,
					  RM_DBUS_INTERFACE,
            RM_DBUS_METHOD_SET_MAINTWINDOW);
  if (message == NULL)
    {
      fprintf (stderr, _("Out of memory!\n"));
      return 1;
    }

  dbus_message_append_args (message, DBUS_TYPE_STRING, &start,
			    DBUS_TYPE_STRING, &duration, DBUS_TYPE_INVALID);

  /* Send the call */
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
  if (strategy == RM_REBOOTSTRATEGY_OFF)
    return send_message_with_arg (connection,
				  RM_DBUS_METHOD_TEMPORARY_OFF, strategy);
  else
    return send_message_with_arg (connection,
				  RM_DBUS_METHOD_SET_STRATEGY, strategy);
 }

static int
trigger_reboot (DBusConnection *connection, RM_RebootOrder order)
{
  return send_message_with_arg (connection,
				RM_DBUS_METHOD_REBOOT, order);
}

static int
cancel_reboot (DBusConnection *connection)
{
  DBusMessage *message;
  int retval = 0;

  message = dbus_message_new_method_call(RM_DBUS_NAME,
 				  	 RM_DBUS_PATH,
				    	 RM_DBUS_INTERFACE,
			                 RM_DBUS_METHOD_CANCEL);
  if (message == NULL)
    {
      fprintf (stderr, _("Out of memory!\n"));
      return 1;
    }
  /* Send the  */
  if (dbus_connection_send (connection, message, NULL) == FALSE)
    {
      fprintf (stderr, _("Out of memory!\n"));
      retval = 1;
    }
  dbus_message_unref (message);

  return retval;
}

static int
get_window (DBusConnection *connection)
{
  DBusError error;
  DBusMessage *message, *reply;
  const char *start, *duration;
  message = dbus_message_new_method_call (RM_DBUS_NAME,
					  RM_DBUS_PATH,
					  RM_DBUS_INTERFACE,
					  RM_DBUS_METHOD_GET_MAINTWINDOW);
  if (message == NULL)
    {
      fprintf (stderr, _("Out of memory!\n"));
      return 1;
    }

  dbus_error_init (&error);
  /* send message and get a handle for a reply */
  if ((reply = dbus_connection_send_with_reply_and_block (connection, message,
          -1, &error)) == NULL)
  {
    if (dbus_error_is_set (&error))
    {
      fprintf (stderr, _("Error: %s\n"), error.message);
      dbus_error_free (&error);
    }
    else
      fprintf (stderr, _("Out of memory!\n"));

    dbus_message_unref (message);

    return 1;
  }

  /* read the parameters */
  if (dbus_message_get_args (reply, &error, DBUS_TYPE_STRING, &start,
			     DBUS_TYPE_STRING, &duration, DBUS_TYPE_INVALID))
    {
      if (start != NULL && strlen (start) > 0)
	printf (_("Maintenance window is set to %s, lasting %s.\n"), start, duration);
      else
	printf (_("No maintenance window set.\n"));
    }
  else
  {
    if (dbus_error_is_set (&error))
    {
      fprintf (stderr, _("Error reading arguments: %s\n"),
          error.message);
      dbus_error_free (&error);
    }
    else
      fprintf (stderr, _("Unknown error reading arguments\n"));
  }

  /* free reply and close connection */
  dbus_message_unref (reply);

  return 0;
}

/*
  return values:
  -1 : error
  RM_RebootStatus otherwise.
 */
static int
get_strategy (DBusConnection *connection)
{
  DBusError error;
  DBusMessage *message, *reply;
  RM_RebootStrategy strategy = RM_REBOOTSTRATEGY_UNKNOWN;

  message = dbus_message_new_method_call (RM_DBUS_NAME,
					  RM_DBUS_PATH,
					  RM_DBUS_INTERFACE,
					  RM_DBUS_METHOD_GET_STRATEGY);
  if (message == NULL)
    {
      fprintf (stderr, _("Out of memory!\n"));
      return -1;
    }

  dbus_error_init (&error);
  /* send message and get a handle for a reply */
  if ((reply = dbus_connection_send_with_reply_and_block (connection, message,
          -1, &error)) == NULL)
  {
    if (dbus_error_is_set (&error))
    {
      fprintf (stderr, _("Error: %s\n"), error.message);
      dbus_error_free (&error);
    }
    else
      fprintf (stderr, _("Out of memory!\n"));

    dbus_message_unref (message);

    return -1;
  }

  /* read the parameters */
  if (!dbus_message_get_args (reply, &error, DBUS_TYPE_UINT32,
			      &strategy, DBUS_TYPE_INVALID))
    {
      if (dbus_error_is_set (&error))
	{
	  fprintf (stderr, _("Error reading arguments: %s\n"),
		   error.message);
	  dbus_error_free (&error);
	}
      else
	fprintf (stderr, _("Unknown error reading arguments\n"));

      /* free reply and close connection */
      dbus_message_unref (reply);

      return -1;
    }

  /* free reply and close connection */
  dbus_message_unref (reply);

  return strategy;
}

static int
get_status (DBusConnection *connection)
{
  DBusError error;
  DBusMessage *message, *reply;
  uint32_t status;
  message = dbus_message_new_method_call (RM_DBUS_NAME,
					  RM_DBUS_PATH,
					  RM_DBUS_INTERFACE,
					  RM_DBUS_METHOD_STATUS);
  if (message == NULL)
    {
      fprintf (stderr, _("Out of memory!\n"));
      return -1;
    }

  dbus_error_init (&error);
  /* send message and get a handle for a reply */
  if ((reply = dbus_connection_send_with_reply_and_block (connection, message,
							  -1, &error)) == NULL)
    {
      if (dbus_error_is_set (&error))
	{
	  fprintf (stderr, _("Error: %s\n"), error.message);
	  dbus_error_free (&error);
	}
      else
	fprintf (stderr, _("Out of memory!\n"));

      dbus_message_unref (message);

      return -1;
    }

  /* read the parameters */
  if (!dbus_message_get_args (reply, &error, DBUS_TYPE_UINT32, &status,
			     DBUS_TYPE_INVALID))
    {
      if (dbus_error_is_set (&error))
	{
	  fprintf (stderr, _("Error reading arguments: %s\n"),
		   error.message);
	  dbus_error_free (&error);
	}
      else
	fprintf (stderr, _("Unknown error reading arguments\n"));

      /* free reply and close connection */
      dbus_message_unref (reply);
      return -1;
  }

  /* free reply and close connection */
  dbus_message_unref (reply);

  return status;
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
      int quiet = 0;

      if (argc > 2)
	if (strcasecmp ("-q", argv[2]) == 0 ||
	    strcasecmp ("--quiet", argv[2]) == 0)
	  quiet = 1;

      if (rebootmgr_is_running (connection))
	{
	  if (!quiet)
	    printf ("RebootMgr is active\n");
	}
      else
	{
	  if (quiet)
	    retval = 1;
	  else
	    printf ("RebootMgr is dead\n");
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
  else if (strcasecmp ("soft-reboot", argv[1]) == 0)
    {
      RM_RebootOrder order = RM_REBOOTORDER_SOFT;

      if (argc > 2)
	{
	  if (strcasecmp ("fast", argv[2]) == 0)
	    order = RM_REBOOTORDER_SOFT_FAST;
	  else if (strcasecmp ("now", argv[2]) == 0)
	    order = RM_REBOOTORDER_SOFT_FORCED;
	  else
	    usage (1);
	}
      retval = trigger_reboot (connection, order);
    }
  else if (strcasecmp ("set-strategy", argv[1]) == 0 ||
	   strcasecmp ("set_strategy", argv[1]) == 0)
    {
      RM_RebootStrategy strategy = RM_REBOOTSTRATEGY_BEST_EFFORT;

      if (argc > 2)
    	{
        int err;
        strategy = string_to_strategy(argv[2], &err);
        if (err != 0) {
          usage(1);
        }
      } else {
        usage(1);
      }
      retval = set_strategy (connection, strategy);
    }
  else if (strcasecmp ("get-strategy", argv[1]) == 0 ||
           strcasecmp ("get_strategy", argv[1]) == 0)
    {
      int strategy;

      strategy = get_strategy (connection);
      if (strategy < 0)
	retval = 1;
      else
	{
	  int conv_error;
	  const char *str = strategy_to_string(strategy, &conv_error);
	  if (conv_error)
	    {
	      printf (_("Invalid strategy, defaulting to: %s\n"), str);
	      retval = 1;
	    }
	  else
	    printf (_("Reboot strategy: %s\n"), str);
      }
    }
  else if (strcasecmp ("get-window", argv[1]) == 0 ||
           strcasecmp ("get_window", argv[1]) == 0)
      retval = get_window (connection);
  else if (strcasecmp ("set-window", argv[1]) == 0 ||
           strcasecmp ("set_window", argv[1]) == 0)
    {
      if (argc > 3)
	{
	  const char *start = argv[2];
	  const char *duration = argv[3];
	  CalendarSpec *tmp = NULL;

	  if (strlen (argv[2]) > 0)
	    {

	      if ((calendar_spec_from_string (start, &tmp)) < 0)
		{
		  printf (_("Invalid time for maintenance window\n"));
		  retval = 1;
		}
	      else if (parse_duration (duration) ==  BAD_TIME)
		{
		  printf (_("Invalid duration format for maintenance window\n"));
		  retval = 1;
		}
	    }
	  if (retval == 0)
	    retval = set_window (connection, start, duration);
	}
      else
	{
	  usage(1);
	}
    }
  else if (strcasecmp ("cancel", argv[1]) == 0)
    retval = cancel_reboot (connection);
  else if (strcasecmp ("status", argv[1]) == 0)
    {
      int status;
      int quiet = 0;

      if (argc > 2)
	if (strcasecmp ("-q", argv[2]) == 0 ||
	    strcasecmp ("--quiet", argv[2]) == 0)
	  quiet = 1;

      status = get_status (connection);
      if (quiet)
	retval = status;
      else
	{
	  if (status >= 0)
	    printf ("Status: %s\n", status_to_string (status, NULL));
	  else
	    retval = 1;
	}
    }
  else
    usage (1);

  dbus_connection_unref (connection);
  return retval;
}
