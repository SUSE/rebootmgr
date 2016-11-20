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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <libintl.h>
#include <getopt.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>

#include "log_msg.h"
#include "rebootmgr.h"
#include "calendarspec.h"

#ifndef _
#define _(String) gettext (String)
#endif

static RM_RebootStrategy reboot_strategy = RM_REBOOTSTRATEGY_BEST_EFFORD;
static int reboot_running = 0;
static guint reboot_timer_id = 0;
static CalendarSpec *maint_window_start = NULL;
/* static uint64_t maint_window_duration = 1; */

static void
print_help (void)
{
  fputs (_("rebootmgrd - reboot following a specified strategy\n\n"), stdout);

  fputs (_("  -d,--debug   Log debug output\n"),
         stdout);
  fputs (_("  -?, --help     Give this help list\n"), stdout);
  fputs (_("      --version  Print program version\n"), stdout);
}

static void
print_error (void)
{
  const char *program = "rebootmgrd";
  fprintf (stderr,
           _("Try `%s --help' for more information.\n"),
           program);
}

static void
reboot_now (void)
{
  if (reboot_running == 1)
    {
      log_msg (LOG_INFO, "rebootmgr: reboot triggered now!");

#if 0
      if (execl ("/usr/sbin/sytemctl", "systemctl", "reboot", NULL) == -1)
	log_msg (LOG_ERR, "Calling /usr/sbin/systemctl failed: %s\n",
		 strerror (errno));
#else
      log_msg (LOG_DEBUG, "systemctl reboot called!");
#endif
      reboot_running = 0;
    }
}

/* Called by g_timeout_add when maintenance window starts */
static gboolean
reboot_timer (gpointer user_data __attribute__((unused)))
{
  reboot_now ();
  return FALSE;
}

static void
initialize_timer (void)
{
  int r;
  usec_t curr = now(CLOCK_REALTIME);
  usec_t next;

  r = calendar_spec_next_usec (maint_window_start, curr, &next);
  if (r < 0)
    {
      log_msg (LOG_ERR, "Internal error converting the timer: %s",
	       strerror (-r));
      return;
    }

  if (debug_flag)
    {
      char buf[FORMAT_TIMESTAMP_MAX];
      int64_t in_secs = (next - curr) / USEC_PER_SEC;

      log_msg (LOG_DEBUG,
	       "Reboot in %i seconds at %s", in_secs,
	       format_timestamp(buf, sizeof(buf), next));
    }
  reboot_timer_id = g_timeout_add ((next-curr)/USEC_PER_MSEC, reboot_timer, NULL);
}

static int
is_etcd_running (void)
{
  /* XXX */
  return 0;
}

static void
do_reboot (RM_RebootOrder order)
{
  reboot_running = 1;

  if (order == RM_REBOOTORDER_FORCED)
    reboot_now ();

  switch (reboot_strategy)
    {
    case RM_REBOOTSTRATEGY_BEST_EFFORD:
      if (is_etcd_running ())
	{ /* XXX reboot with locks */ }
      else if (maint_window_start != NULL &&
	       order != RM_REBOOTORDER_FAST)
	initialize_timer ();
      else
	reboot_now ();
      break;
    case RM_REBOOTSTRATEGY_INSTANTLY:
      reboot_now ();
      break;
    case RM_REBOOTSTRATEGY_MAINT_WINDOW:
      if (order == RM_REBOOTORDER_FAST ||
	  maint_window_start == NULL)
	reboot_now ();
      initialize_timer ();
      break;
    case RM_REBOOTSTRATEGY_ETCD_LOCK:
      if (order == RM_REBOOTORDER_FAST)
	{ /* ignore maintenance window */ };
      /* XXX */
      break;
    case RM_REBOOTSTRATEGY_OFF:
      reboot_running = 0;
      /* Do nothing */
      break;
    default:
      reboot_running = 0;
      log_msg (LOG_ERR, "ERROR: unknown reboot strategy %i", reboot_strategy);
      break;
    }
}


static gboolean
dbus_reconnect (gpointer user_data __attribute__((unused)))
{
#if 0 /* XXX */
  gboolean status;

  status = dbus_init ();
  if (debug_flag)
    log_msg (LOG_DEBUG, "Reconnect %s",
	     status ? "successful" : "failed");
  return !status;
#endif
  return FALSE;
}

static DBusHandlerResult
dbus_filter (DBusConnection *connection, DBusMessage *message,
	     void *user_data  __attribute__((unused)))
{
  DBusHandlerResult handled = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL,
			      "Disconnected"))
    {
      /* D-Bus system bus went away */
      log_msg (LOG_INFO, "Lost connection to D-Bus\n");
      dbus_connection_unref (connection);
      connection = NULL;
      /* g_timeout_add (1000, dbus_reconnect, NULL); */
      g_timeout_add_seconds (1, dbus_reconnect, NULL);
      handled = DBUS_HANDLER_RESULT_HANDLED;
    }
  else if (dbus_message_is_signal (message, RM_DBUS_INTERFACE,
                                   RM_DBUS_SIGNAL_REBOOT))
    {
      RM_RebootOrder order = RM_REBOOTORDER_UNKNOWN;

      if (dbus_message_get_args (message, NULL, DBUS_TYPE_UINT32,
                                 &order, DBUS_TYPE_INVALID))
        {
          if (order == RM_REBOOTORDER_STANDARD) {
	    if (debug_flag)
	      log_msg (LOG_DEBUG, "Reboot at next possible time");
	  } else if (order == RM_REBOOTORDER_FAST) {
	    if (debug_flag)
	      log_msg (LOG_DEBUG, "Reboot as fast as possible");
	  } else if (order == RM_REBOOTORDER_FORCED) {
	    if (debug_flag)
	      log_msg (LOG_DEBUG, "Reboot now");
	  }
	}
      do_reboot (order);
      handled = DBUS_HANDLER_RESULT_HANDLED;
    }
  else if (dbus_message_is_signal (message, RM_DBUS_INTERFACE,
				   RM_DBUS_SIGNAL_CANCEL))
    {
      if (debug_flag)
	log_msg (LOG_DEBUG, "Cancel reboot");
      if (reboot_running > 0 && reboot_timer_id > 0)
	g_source_remove (reboot_timer_id);
      reboot_running = 0;
      reboot_timer_id = 0;
      handled = DBUS_HANDLER_RESULT_HANDLED;
    }
  else if (dbus_message_is_signal (message, RM_DBUS_INTERFACE,
                                   RM_DBUS_SIGNAL_SET_STRATEGY))
    {
      RM_RebootStrategy strategy = RM_REBOOTSTRATEGY_UNKNOWN;

      if (dbus_message_get_args (message, NULL, DBUS_TYPE_UINT32,
                                 &strategy, DBUS_TYPE_INVALID))
        {
	  if (debug_flag)
	    log_msg (LOG_DEBUG, "set-strategy called");
          if (strategy != RM_REBOOTSTRATEGY_UNKNOWN &&
	      reboot_strategy != strategy)
	    {
	      if (debug_flag)
		log_msg (LOG_DEBUG, "reboot_strategy changed");
	      reboot_strategy = strategy;
	    }
	  handled = DBUS_HANDLER_RESULT_HANDLED;
	}
    }
  else if (dbus_message_is_signal (message, RM_DBUS_INTERFACE,
				   RM_DBUS_SIGNAL_GET_STRATEGY))
    {
      DBusMessage* reply;

      if (debug_flag)
	log_msg (LOG_DEBUG, "get-strategy called");

      /* create a reply from the message */
      reply = dbus_message_new_method_return (message);
      dbus_message_append_args (reply, DBUS_TYPE_UINT32, &reboot_strategy,
				DBUS_TYPE_INVALID);
      /* send the reply && flush the connection */
      if (!dbus_connection_send (connection, reply, NULL))
	{
	  log_msg (LOG_ERR, "Out of memory!");
	  return handled;
	}
      /* free the reply */
      dbus_message_unref (reply);
      handled = DBUS_HANDLER_RESULT_HANDLED;
    }
  else if (dbus_message_is_signal (message, RM_DBUS_INTERFACE,
				   RM_DBUS_SIGNAL_STATUS))
    {
      if (debug_flag)
	log_msg (LOG_DEBUG, "Reboot status requested");
      handled = DBUS_HANDLER_RESULT_HANDLED;
    }
#if 0
  else if (debug_flag)
    {
      log_msg (LOG_DEBUG, "interface: %s, object path: %s, method: %s",
	       dbus_message_get_interface(message),
	       dbus_message_get_path (message),
	       dbus_message_get_member (message));
    }
#endif

  return handled;
}


int
main (int argc __attribute__((unused)),
      char **argv __attribute__ ((unused)))
{
  DBusConnection *connection = NULL;
  DBusError error;
  GMainLoop *loop;

  while (1)
    {
      int c;
      int option_index = 0;
      static struct option long_options[] =
	{
	  {"debug", no_argument, NULL, 'd'},
	  {"version", no_argument, NULL, 'v'},
	  {"usage", no_argument, NULL, '?'},
	  {"help", no_argument, NULL, 'h'},
	  {NULL, 0, NULL, '\0'}
	};


      c = getopt_long (argc, argv, "dvh?", long_options, &option_index);
      if (c == (-1))
        break;
      switch (c)
        {
        case 'd':
	  debug_flag = 1;
	  break;
	case '?':
	case 'h':
          print_help ();
          return 0;
	case 'v':
	  fprintf (stdout, "rebootmgrd (%s) %s\n", PACKAGE, VERSION);
          return 0;
        default:
          print_help ();
          return 1;
        }
    }

  argc -= optind;
  argv += optind;

  if (argc > 1)
    {
      print_error ();
      return 1;
    }

  calendar_spec_from_string("hourly", &maint_window_start);

  loop = g_main_loop_new (NULL, FALSE);

  dbus_error_init (&error);

  connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
  if (connection == NULL || dbus_error_is_set (&error))
    {
      log_msg (LOG_ERR, "Connection to D-BUS system message bus failed: %s.",
               error.message);
      dbus_error_free (&error);
      connection = NULL;
      goto out;
    }

  dbus_bool_t ret = dbus_bus_name_has_owner (connection,
					     RM_DBUS_NAME, &error);
  if (dbus_error_is_set (&error))
    {
      log_msg (LOG_ERR, "DBus Error: %s", error.message);
      dbus_error_free (&error);
      goto out;
    }

  if (ret == FALSE)
    {
      if (debug_flag)
	log_msg (LOG_INFO, "Bus name %s doesn't have an owner, reserving it...",
		 RM_DBUS_NAME);

      int request_name_reply =
	dbus_bus_request_name (connection, RM_DBUS_NAME,
			       DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
      if (dbus_error_is_set (&error))
	{
	  log_msg (LOG_ERR, "Error requesting a bus name: %s", error.message);
	  dbus_error_free (&error);
	  return 0;
	}
      if ( request_name_reply == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER )
	{
	  if (debug_flag)
	    log_msg (LOG_DEBUG, "Bus name %s successfully reserved!",
		     RM_DBUS_NAME);
	}
      else
	{
	  log_msg (LOG_ERR, "Failed to reserve name %s", RM_DBUS_NAME);
	  return 0;
	}
    }
  else
    {
      /*
	if ret of method dbus_bus_name_has_owner is TRUE, then this is useful for
	detecting if your application is already running and had reserved a bus name
	unless somebody stole this name from you, so better to choose a correct bus
	name
      */
      log_msg (LOG_ERR, "%s is already reserved", RM_DBUS_NAME);
      return 1;
    }

  dbus_bus_add_match (connection, "type='signal',"
		      "interface='" DBUS_INTERFACE_DBUS "',"
		      "sender='" DBUS_SERVICE_DBUS "'",
		      &error);
  if (dbus_error_is_set (&error))
    {
      log_msg (LOG_ERR, "Error adding match, %s: %s",
	       error.name, error.message);

      dbus_error_free (&error);
      dbus_connection_unref (connection);
      connection = NULL;
      goto out;
    }

  dbus_bus_add_match (connection,
    "type='signal',interface='"RM_DBUS_INTERFACE"'",
#if 0
    "sender='"RM_DBUS_SERVICE"',"
    "path='"RM_DBUS_PATH"'",
#endif
    &error);
  if (dbus_error_is_set (&error))
    {
      log_msg (LOG_ERR, "Error adding match, %s: %s",
	       error.name, error.message);
      dbus_error_free (&error);
      dbus_connection_unref (connection);
      connection = NULL;
      goto out;
    }

  dbus_connection_set_exit_on_disconnect (connection, FALSE);
  if (!dbus_connection_add_filter (connection, dbus_filter, loop, NULL))
    goto out;

  dbus_connection_setup_with_g_main (connection, NULL);

  g_main_loop_run (loop);

  return 0;

 out:
  if (connection)
    {
      return 1;
#if 0 /* XXX */
      dbus_bus_release_name (connection, RM_DBUS_SERVICE, &error);
      dbus_connection_unref (connection);
      return 0;
#endif
    }
  else
    {
      if (debug_flag)
	log_msg (LOG_DEBUG, "No connection possible, assume online mode");
      return 0;
    }
}
