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

#include <string.h>
#include <libintl.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>

#include "log_msg.h"
#include "rebootmgr.h"

#ifndef _
#define _(String) gettext (String)
#endif

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
dbus_filter (DBusConnection *connection,
	     DBusMessage *message, void *user_data  __attribute__((unused)))
{
  DBusHandlerResult handled = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL,
			      "Disconnected"))
    {
      /* D-Bus system bus went away */
      log_msg (LOG_INFO, "Lost connection to D-Bus\n");
      dbus_connection_unref (connection);
      connection = NULL;
      //g_timeout_add (1000, dbus_reconnect, NULL);
      g_timeout_add_seconds (1, dbus_reconnect, NULL);
      handled = DBUS_HANDLER_RESULT_HANDLED;
    }
  else if (dbus_message_is_signal (message, RM_DBUS_INTERFACE,
                                   RM_DBUS_SIGNAL_REBOOT))
    {
      RMRebootOrder order = RM_REBOOTORDER_UNKNOWN;

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
	  handled = DBUS_HANDLER_RESULT_HANDLED;
	}
    }
  else if (dbus_message_is_signal (message, RM_DBUS_INTERFACE,
				   RM_DBUS_SIGNAL_CANCEL))
    {
      if (debug_flag)
	log_msg (LOG_DEBUG, "Cancel reboot");
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

  debug_flag=1;

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

  dbus_bool_t ret = dbus_bus_name_has_owner (connection, RM_DBUS_SERVICE, &error);
  if (dbus_error_is_set (&error))
    {
      log_msg (LOG_ERR, "DBus Error: %s", error.message);
      dbus_error_free (&error);
      goto out;
    }

  if (ret == FALSE)
    {
      if (debug_flag)
	log_msg (LOG_INFO, "Bus name %s doesn't have an owner, reserving it...", RM_DBUS_SERVICE);

      int request_name_reply =
	dbus_bus_request_name (connection, RM_DBUS_SERVICE, DBUS_NAME_FLAG_DO_NOT_QUEUE,
			       &error);
      if (dbus_error_is_set (&error))
	{
	  log_msg (LOG_ERR, "Error requesting a bus name: %s", error.message);
	  dbus_error_free (&error);
	  return 0;
	}
      if ( request_name_reply == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER )
	{
	  if (debug_flag)
	    log_msg (LOG_DEBUG, "Bus name %s successfully reserved!", RM_DBUS_SERVICE);
	}
      else
	{
	  log_msg (LOG_ERR, "Failed to reserve name %s", RM_DBUS_SERVICE);
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
      log_msg (LOG_ERR, "%s is already reserved", RM_DBUS_SERVICE);
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
