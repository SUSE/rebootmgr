/* Copyright (c) 2016 Daniel Molkentin
   Author: Daniel Molkentin <dmolkentin@suse.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation in version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, see <http://www.gnu.org/licenses/>. */

#include <stdio.h>
#include <string.h>
#include <libintl.h>
#include <dbus/dbus.h>

#include "etcd_handler.h"
#include "log_msg.h"
#include "rebootmgr.h"

#define SD_DBUS_NAME            "org.freedesktop.systemd1"
#define SD_DBUS_PATH            "/org/freedesktop/systemd1"
#define SD_DBUS_INTERFACE       "org.freedesktop.systemd1.Manager"
#define SD_DBUS_METHOD_SUBSCRIBE        "Subscribe"
#define SD_DBUS_SIGNAL_UNITNEW          "UnitNew"
#define SD_DBUS_SIGNAL_UNITREMOVED      "UnitRemoved"
#define SD_DBUS_METHOD_GETUNIT          "GetUnit"
#define SD_DBUS_ERROR_NOSUCHUNIT  "org.freedesktop.systemd1.NoSuchUnit"

#define ETCD_UNIT_NAME    "etcd.service"

static int etcd_running = 0;

int etcd_is_running()
{
  return etcd_running;
}

int
etcd_handler_init (DBusConnection *connection)
{

  DBusMessage *message, *reply;
  DBusError error;
  dbus_error_init (&error);

  dbus_bus_add_match (connection, "type='signal',interface='"SD_DBUS_INTERFACE"'", &error);
  if (dbus_error_is_set (&error))
  {
    log_msg (LOG_ERR, "Error adding match for systemd interface, %s: %s",
        error.name, error.message);
    dbus_error_free (&error);
    connection = NULL;
    return FALSE;
  }

  message = dbus_message_new_method_call (SD_DBUS_NAME,
      SD_DBUS_PATH,
      SD_DBUS_INTERFACE,
      SD_DBUS_METHOD_GETUNIT);

  const char *unit = ETCD_UNIT_NAME;
  if (!dbus_message_append_args(message, DBUS_TYPE_STRING, &unit, DBUS_TYPE_INVALID))
  {
    fprintf (stderr, _("Failed to pass argument to DBUS message!\n"));
    return FALSE;
  }

  if (message == NULL)
  {
    fprintf (stderr, _("Out of memory!\n"));
    return FALSE;
  }

  /* send message and get a handle for a reply */
  if ((reply = dbus_connection_send_with_reply_and_block (connection, message,
          -1, &error)) == NULL)
  {
    if (dbus_error_is_set (&error))
    {
      if (dbus_error_has_name (&error, SD_DBUS_ERROR_NOSUCHUNIT))
      {
        etcd_running = 0;
      } else {
        fprintf (stderr, _("Error: %s\n"), error.message);
      }
      dbus_message_unref (message);
      dbus_error_free (&error);
    }
    else
    {
      fprintf (stderr, _("Out of memory!\n"));
      dbus_message_unref (message);
    }

    if (reply) {
      dbus_message_unref (reply);
    }
  }

  return TRUE;
}

DBusHandlerResult
etcd_handler_filter(DBusMessage* message)
{
  DBusHandlerResult handled = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  if (dbus_message_is_signal (message, SD_DBUS_INTERFACE,
        SD_DBUS_SIGNAL_UNITNEW))
  {
    char *unit;
    if (dbus_message_get_args (message, NULL, DBUS_TYPE_STRING,
          &unit, DBUS_TYPE_INVALID) &&
        (strncmp(unit, "etcd.service", 11) == 0))
    {
      if (debug_flag)
        log_msg (LOG_DEBUG, "etcd started");
      etcd_running = 1;
      handled = DBUS_HANDLER_RESULT_HANDLED;
    }
  }
  else if (dbus_message_is_signal (message, SD_DBUS_INTERFACE,
        SD_DBUS_SIGNAL_UNITREMOVED))
  {
    char *unit;
    if (dbus_message_get_args (message, NULL, DBUS_TYPE_STRING,
          &unit, DBUS_TYPE_INVALID) &&
        (strncmp(unit, "etc.service", 11) == 0))
    {
      if (debug_flag)
        log_msg (LOG_DEBUG, "etcd stopped");
      etcd_running = 0;
      handled = DBUS_HANDLER_RESULT_HANDLED;
    }
  }
  return handled;
}
