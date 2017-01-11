/* Copyright (c) 2016, 2017 Thorsten Kukuk
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

#include <errno.h>
#include <getopt.h>
#include <libintl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "config_file.h"
#include "log_msg.h"
#include "rebootmgr.h"
#include "parse-duration.h"
#include "util.h"
#include "lock-etcd.h"

#define PROPERTIES_METHOD_GETALL "GetAll"
#define PROPERTIES_METHOD_GET    "Get"
#define PROPERTIES_METHOD_SET    "Set"

static int
create_context (RM_CTX **ctx)
{
  if ((*ctx = calloc(1, sizeof(RM_CTX))) == NULL)
    return 0;

  **ctx = (RM_CTX) {RM_REBOOTSTRATEGY_BEST_EFFORT, 0,
		     RM_REBOOTORDER_STANDARD, 0, NULL, 3600, NULL};
  (*ctx)->lock_group = strdup ("default");

  return 1;
}

static int
destroy_context (RM_CTX *ctx)
{
  if (ctx == NULL)
    {
      errno = EBADF;
      return 0;
    }

  if (ctx->maint_window_start != NULL)
    calendar_spec_free (ctx->maint_window_start);
  if (ctx->lock_group != NULL)
    free (ctx->lock_group);
  free (ctx);
  return 1;
}

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
reboot_now (RM_CTX *ctx)
{
  if (ctx->reboot_running == 1)
    {
      if (!debug_flag)
	{
	  log_msg (LOG_INFO, "rebootmgr: reboot triggered now!");
	  if (execl ("/usr/bin/systemctl", "systemctl", "reboot", NULL) == -1)
	    log_msg (LOG_ERR, "Calling /usr/bin/systemctl failed: %s",
		     g_strerror (errno));
	}
      else
	log_msg (LOG_DEBUG, "systemctl reboot called!");

      ctx->reboot_running = 0;
    }
}

/* Called by g_timeout_add when maintenance window starts */
static gboolean
reboot_timer (gpointer user_data)
{
  RM_CTX *ctx = user_data;

  if (((ctx->reboot_strategy == RM_REBOOTSTRATEGY_BEST_EFFORT &&
	etcd_is_running()) ||
       ctx->reboot_strategy == RM_REBOOTSTRATEGY_ETCD_LOCK))
    {
      if (etcd_get_lock (ctx->lock_group) != 0)
	{
	  log_msg (LOG_ERR, "ERROR: etcd_get_lock failed, abort reboot");
	  ctx->reboot_running = 0;
	  return FALSE;
	}
      reboot_now (ctx);
      /* if we end here, reboot was canceld */
      if (etcd_release_lock (ctx->lock_group) != 0)
	{
	  log_msg (LOG_ERR, "ERROR: cannot remove old reboot lock from etcd!");
	}
    }
  else
    reboot_now (ctx);
  return FALSE;
}

static void
initialize_timer (RM_CTX *ctx)
{
  int r;
  usec_t curr = now(CLOCK_REALTIME);
  usec_t next;
  usec_t duration = ctx->maint_window_duration * USEC_PER_SEC;

  /* Check, if we are inside the maintenance window. If yes, reboot now */
  r = calendar_spec_next_usec (ctx->maint_window_start, curr - duration, &next);
  if (r < 0)
    {
      log_msg (LOG_ERR, "Internal error converting the timer: %s",
	       g_strerror (-r));
      return;
    }
  if (curr > next && curr < next + duration)
    {
      /* we are inside the maintenance window, reboot */
      reboot_timer (ctx);
      return;
    }

  /* we are not inside a maintenance window, set timer for next one */
  r = calendar_spec_next_usec (ctx->maint_window_start, curr, &next);
  if (r < 0)
    {
      log_msg (LOG_ERR, "Internal error converting the timer: %s",
	       g_strerror (-r));
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
  ctx->reboot_timer_id =
    g_timeout_add ((next-curr)/USEC_PER_MSEC, reboot_timer, ctx);
}

static gpointer
do_reboot (gpointer user_data)
{
  RM_CTX *ctx = user_data;

  ctx->reboot_running = 1;

  if (ctx->reboot_order == RM_REBOOTORDER_FORCED)
    reboot_now (ctx);

  switch (ctx->reboot_strategy)
    {
    case RM_REBOOTSTRATEGY_BEST_EFFORT:
    case RM_REBOOTSTRATEGY_ETCD_LOCK:
      if (ctx->maint_window_start != NULL &&
	  ctx->reboot_order != RM_REBOOTORDER_FAST)
	initialize_timer(ctx);
      else
	{
	  if (ctx->reboot_strategy == RM_REBOOTSTRATEGY_ETCD_LOCK ||
	      etcd_is_running())
	    {
	      if (etcd_get_lock (ctx->lock_group) != 0)
		{
		  log_msg (LOG_ERR, "ERROR: etcd_get_lock failed, abort reboot");
		  ctx->reboot_running = 0;
		  return NULL;
		}
	    }
	  reboot_now (ctx);
	}
      break;
    case RM_REBOOTSTRATEGY_INSTANTLY:
      reboot_now (ctx);
      break;
    case RM_REBOOTSTRATEGY_MAINT_WINDOW:
      if (ctx->reboot_order == RM_REBOOTORDER_FAST ||
	  ctx->maint_window_start == NULL)
	reboot_now(ctx);
      initialize_timer(ctx);
      break;
    case RM_REBOOTSTRATEGY_OFF:
      ctx->reboot_running = 0;
      /* Do nothing */
      break;
    default:
      ctx->reboot_running = 0;
      log_msg (LOG_ERR, "ERROR: unknown reboot strategy %i",
	       ctx->reboot_strategy);
      break;
    }
  return NULL;
}

static DBusMessage *
handle_native_iface (RM_CTX *ctx, DBusMessage *message)
{
  DBusError err;
  DBusMessage *reply = 0;

  reply = dbus_message_new_method_return (message);

  dbus_error_init(&err);
  if (dbus_message_is_method_call (message, RM_DBUS_INTERFACE,
				   RM_DBUS_METHOD_REBOOT))
    {
      RM_RebootOrder order = RM_REBOOTORDER_UNKNOWN;

      if (dbus_message_get_args (message, NULL, DBUS_TYPE_UINT32,
				 &order, DBUS_TYPE_INVALID))
	{
	  if (order == RM_REBOOTORDER_STANDARD)
	    {
	      if (debug_flag)
		log_msg (LOG_DEBUG, "Reboot at next possible time");
	    }
	  else if (order == RM_REBOOTORDER_FAST)
	    {
	      if (debug_flag)
		log_msg (LOG_DEBUG, "Reboot as fast as possible");
	    }
	  else if (order == RM_REBOOTORDER_FORCED)
	    {
	      if (debug_flag)
		log_msg (LOG_DEBUG, "Reboot now");
	    }
	  else
	    {
	      log_msg (LOG_ERR, "Unknown reboot order (%i), ignore reboot command",
		       order);
	      return reply;
	    }
	  ctx->reboot_order = order;
	}
      if (ctx->reboot_running > 0)
	log_msg (LOG_INFO, "Reboot already in progress, ignored");
      else
	g_thread_new ("do reboot thread", do_reboot, ctx);
    }
  else if (dbus_message_is_method_call (message, RM_DBUS_INTERFACE,
					RM_DBUS_METHOD_CANCEL))
    {
      log_msg (LOG_INFO, "Reboot canceld");
      if (ctx->reboot_running > 0 && ctx->reboot_timer_id > 0)
	g_source_remove (ctx->reboot_timer_id);
      ctx->reboot_running = 0;
      ctx->reboot_timer_id = 0;
    }
  else if (dbus_message_is_method_call (message, RM_DBUS_INTERFACE,
					RM_DBUS_METHOD_SET_STRATEGY))
    {
      RM_RebootStrategy strategy = RM_REBOOTSTRATEGY_UNKNOWN;

      if (dbus_message_get_args (message, NULL, DBUS_TYPE_UINT32,
				 &strategy, DBUS_TYPE_INVALID))
	{
	  if (debug_flag)
	    log_msg (LOG_DEBUG, "set-strategy called");
	  if (strategy != RM_REBOOTSTRATEGY_UNKNOWN &&
	      ctx->reboot_strategy != strategy)
	    {
	      if (debug_flag)
		log_msg (LOG_DEBUG, "reboot_strategy changed");
	      ctx->reboot_strategy = strategy;
	    }
	  save_config (ctx);
	}
    }
  else if (dbus_message_is_method_call (message, RM_DBUS_INTERFACE,
					RM_DBUS_METHOD_GET_STRATEGY))
    {
      if (debug_flag)
	log_msg (LOG_DEBUG, "get-strategy called");

      /* create a reply from the message */
      dbus_message_append_args (reply, DBUS_TYPE_UINT32,
				&ctx->reboot_strategy, DBUS_TYPE_INVALID);
    }
  else if (dbus_message_is_method_call (message, RM_DBUS_INTERFACE,
					RM_DBUS_METHOD_STATUS))
    {
      if (debug_flag)
	log_msg (LOG_DEBUG, "Reboot status requested");

      /* create a reply from the message */
      dbus_message_append_args (reply, DBUS_TYPE_UINT32,
				&ctx->reboot_running, DBUS_TYPE_INVALID);
    }
  else if (dbus_message_is_method_call (message, RM_DBUS_INTERFACE,
					RM_DBUS_METHOD_GET_MAINTWINDOW))
    {
      char *str_start = spec_to_string(ctx->maint_window_start);
      char *str_duration = duration_to_string(ctx->maint_window_duration);

      log_msg (LOG_DEBUG, "str_start: '%s' str_duration: '%s'",
	       str_start, str_duration);
      /* create a reply from the message */
      dbus_message_append_args (reply, DBUS_TYPE_STRING, &str_start,
				DBUS_TYPE_STRING, &str_duration,
				DBUS_TYPE_INVALID);

      free (str_start);
      free (str_duration);
    }
  else if (dbus_message_is_method_call (message, RM_DBUS_INTERFACE,
					RM_DBUS_METHOD_SET_MAINTWINDOW))
    {
      const char *str_start;
      const char *str_duration;

      if (debug_flag)
	log_msg (LOG_DEBUG, "set-maintenancewindow called");

      if (dbus_message_get_args (message, NULL, DBUS_TYPE_STRING, &str_start,
                                 DBUS_TYPE_STRING, &str_duration,
                                 DBUS_TYPE_INVALID))
	{

	  if ((calendar_spec_from_string (str_start, &ctx->maint_window_start)) < 0)
	    return reply;

	  if ((ctx->maint_window_duration = parse_duration (str_duration)) ==
	      BAD_TIME)
	    return reply;
	  save_config(ctx);
	}
    }
  return reply;
}

static DBusMessage *
handle_introspect_request (DBusMessage *msg)
{
  DBusMessage *reply = dbus_message_new_method_return(msg);
  char *content =
    get_file_content(INTROSPECTIONDIR "/" RM_DBUS_INTERFACE ".xml");

  if (!content)
    content = get_file_content("../dbus/" RM_DBUS_INTERFACE ".xml");

  dbus_message_append_args(reply, DBUS_TYPE_STRING, &content,
			   DBUS_TYPE_INVALID);
  free(content);
  return reply;
}

/* This is just a stub implementation, and we don't announce it in the xml file
 * but without it, d-feet does fails when trying to query a method */
static DBusMessage *
handle_properties_iface (DBusMessage *msg)
{
  DBusMessage *reply = 0;
  const char* member = dbus_message_get_member(msg);
  if (strcmp(member, PROPERTIES_METHOD_GETALL) == 0 ||
      strcmp(member, PROPERTIES_METHOD_GET) == 0 ||
      strcmp(member, PROPERTIES_METHOD_SET) == 0)
    {
      reply = dbus_message_new_method_return(msg);
      dbus_message_append_args(reply, DBUS_TYPE_INVALID);
    }
  return reply;
}

/* vtable implementation: handles messages and calls respective C functions */
static DBusHandlerResult
handle_message (DBusConnection *connection, DBusMessage *message,
		void *user)
{
  RM_CTX *ctx = user;
  DBusMessage *reply = 0;
  const char* iface = dbus_message_get_interface(message);

  if (dbus_message_is_method_call(message, DBUS_INTERFACE_INTROSPECTABLE,
				  "Introspect"))
    {
      /* Handle Introspection request */
      reply = handle_introspect_request(message);
    }
  else if (strcmp(iface, DBUS_INTERFACE_PROPERTIES) == 0)
    {
      /* Stub implementation for property requests */
      reply = handle_properties_iface(message);
    }
  else if (strcmp(iface, RM_DBUS_INTERFACE) == 0)
    {
      /* Handle requests to our own interfaces  */
      reply = handle_native_iface(ctx, message);
    }

  if (reply)
    {
      /* send the reply && flush the connection */
      if (!dbus_connection_send (connection, reply, NULL))
	{
	  log_msg (LOG_ERR, "Out of memory!");
	  return DBUS_HANDLER_RESULT_NEED_MEMORY;
	}
      dbus_message_unref(reply);
    }
  return DBUS_HANDLER_RESULT_HANDLED;
}

static int dbus_init (RM_CTX*);

static gboolean
dbus_reconnect (gpointer user_data)
{
  RM_CTX* ctx = user_data;
  gboolean status;

  status = dbus_init (ctx);
  if (debug_flag)
    log_msg (LOG_DEBUG, "Reconnect %s",
	     status ? "successful" : "failed");
  return !status;
}

static DBusHandlerResult
dbus_filter (DBusConnection *connection, DBusMessage *message,
	     void *RM_UNUSED(user_data))
{
  DBusHandlerResult handled = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL,
			      "Disconnected"))
    {
      /* D-Bus system bus went away */
      log_msg (LOG_INFO, "Lost connection to D-Bus");
      dbus_connection_unref (connection);
      connection = NULL;
      /* g_timeout_add (1000, dbus_reconnect, NULL); */
      g_timeout_add_seconds (1, dbus_reconnect, NULL);
      handled = DBUS_HANDLER_RESULT_HANDLED;
    }

  return handled;
}

/*
  init dbus
  return value:
  1: success
  0: error
*/
static int
dbus_init (RM_CTX *ctx)
{
  DBusConnection *connection = NULL;
  DBusError error;

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
      log_msg (LOG_ERR, "Error adding match for dbus interface, %s: %s",
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
      log_msg (LOG_ERR, "Error adding match for rebootmgrd interface, %s: %s",
	       error.name, error.message);
      dbus_error_free (&error);
      dbus_connection_unref (connection);
      connection = NULL;
      goto out;
    }

  dbus_connection_set_exit_on_disconnect (connection, FALSE);

  if (!dbus_connection_add_filter (connection, dbus_filter, NULL, NULL))
    goto out;

  DBusObjectPathVTable vtable;
  memset(&vtable, 0, sizeof(vtable));
  vtable.message_function = handle_message;

  if (!dbus_connection_register_object_path(connection, RM_DBUS_PATH,
					    &vtable, ctx))
    log_msg(LOG_ERR, "Failed to register object path");

  dbus_connection_setup_with_g_main (connection, NULL);

  return 1;

 out:
  if (connection)
    {
      dbus_bus_release_name (connection, RM_DBUS_NAME, &error);
      dbus_connection_unref (connection);
      return 0;
    }
  else
    {
      if (debug_flag)
	log_msg (LOG_DEBUG, "No connection possible");
      return 0;
    }
}

int
main (int argc, char **argv)
{
  GMainLoop *loop;
  RM_CTX *ctx;

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

  if (!create_context (&ctx))
    {
      log_msg (LOG_ERR, "Could not initialize context");
      return -1;
    }

  load_config (ctx);

  if (etcd_is_running () &&
      etcd_own_lock (ctx->lock_group))
    {
      if (etcd_release_lock (ctx->lock_group) != 0)
	log_msg (LOG_ERR, "ERROR: cannot remove old reboot lock from etcd!");
    }

  loop = g_main_loop_new (NULL, FALSE);

  if (dbus_init (ctx) != 1)
    return 1;

  g_main_loop_run (loop);

  if (!destroy_context(ctx))
    log_msg(LOG_ERR, "Could not destroy context");

  return 0;
}
