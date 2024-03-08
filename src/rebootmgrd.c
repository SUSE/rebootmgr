
/* Copyright (c) 2016, 2017, 2018, 2019, 2020, 2023 Thorsten Kukuk
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

#include <pthread.h>
#include <errno.h>
#include <getopt.h>
#include <libintl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include <dbus/dbus.h>

#include "config_file.h"
#include "log_msg.h"
#include "rebootmgr.h"
#include "parse-duration.h"
#include "util.h"

#define PROPERTIES_METHOD_GETALL "GetAll"
#define PROPERTIES_METHOD_GET    "Get"
#define PROPERTIES_METHOD_SET    "Set"

static int verbose_flag = 0;
static int log_type = LOG_DEBUG;

static RM_CTX *ctx;
static pthread_mutex_t mutex_ctx = PTHREAD_MUTEX_INITIALIZER;

static int
create_context (void)
{
  pthread_mutex_lock (&mutex_ctx);
  if ((ctx = calloc(1, sizeof(RM_CTX))) == NULL)
    return 0;

  *ctx = (RM_CTX) {RM_REBOOTSTRATEGY_BEST_EFFORT, 0,
		   RM_REBOOTORDER_STANDARD, 0, 0, NULL, 3600, NULL};
  pthread_mutex_unlock (&mutex_ctx);

  return 1;
}

static int
destroy_context (void)
{
  if (ctx == NULL)
    {
      errno = EBADF;
      return 0;
    }

  pthread_mutex_lock (&mutex_ctx);
  if (ctx->connection && dbus_connection_get_is_connected (ctx->connection))
    {
      DBusError error;

      dbus_bus_release_name (ctx->connection, RM_DBUS_NAME, &error);
      dbus_connection_unref (ctx->connection);
    }
  if (ctx->maint_window_start != NULL)
    calendar_spec_free (ctx->maint_window_start);
  free (ctx);
  pthread_mutex_unlock (&mutex_ctx);

  return 1;
}

static void
print_help (void)
{
  fputs (_("rebootmgrd - reboot following a specified strategy\n\n"), stdout);

  fputs (_("  -d,--debug     Debug mode, no reboot done\n"),
         stdout);
  fputs (_("  -v,--verbose   Verbose logging\n"),
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
reboot_now (int soft_reboot)
{
  pthread_mutex_lock (&mutex_ctx);
  if (ctx->temp_off)
    {
      pthread_mutex_unlock (&mutex_ctx);
      return;
    }

  if (ctx->reboot_status > 0)
    {
      if (!debug_flag)
	{
	  pthread_mutex_unlock (&mutex_ctx);
	  if (soft_reboot)
	    log_msg (LOG_INFO, "rebootmgr: soft-reboot triggered now!");
	  else
	    log_msg (LOG_INFO, "rebootmgr: reboot triggered now!");
	  pid_t pid = fork();

	  if (pid < 0)
	    {
	      log_msg (LOG_ERR, "Calling /usr/bin/systemctl failed: %m");
	    }
	  else if (pid == 0)
	    {
	      if (soft_reboot)
		{
		  if (execl ("/usr/bin/systemctl", "systemctl", "soft-reboot",
			     NULL) == -1)
		    {
		      log_msg (LOG_ERR, "Calling /usr/bin/systemctl soft-reboot failed: %m");
		      exit (1);
		    }
		}
	      else
		{
		  if (execl ("/usr/bin/systemctl", "systemctl", "reboot",
			     NULL) == -1)
		    {
		      log_msg (LOG_ERR, "Calling /usr/bin/systemctl reboot failed: %m");
		      exit (1);
		    }
		}
	      exit (0);
	    }
	}
      else
	{
	  if (soft_reboot)
	    log_msg (LOG_DEBUG, "systemctl soft-reboot called!");
	  else
	    log_msg (LOG_DEBUG, "systemctl reboot called!");
	}

      ctx->reboot_status = RM_REBOOTSTATUS_NOT_REQUESTED;
    }
    pthread_mutex_unlock (&mutex_ctx);
}

/* Check which reboot method and forward to that function */
/* Called by timer_create as new thread */
static void
reboot_timer (sigval_t soft_reboot)
{
  if (debug_flag)
    log_msg (LOG_DEBUG, "reboot_timer called");

    reboot_now (soft_reboot.sival_int);
}

/* Create a new timer thread, which calls '_function' after
   specified seconds */
static timer_t
create_timer (time_t seconds, int soft_reboot, void (*_function) (sigval_t))
{
  timer_t timer_id;

  /* Create timer */
  struct sigevent se;
  se.sigev_notify = SIGEV_THREAD;
  se.sigev_value.sival_int = soft_reboot;
  se.sigev_notify_function = _function;
  se.sigev_notify_attributes = NULL;
  if (timer_create(CLOCK_REALTIME, &se, &timer_id) == -1)
    {
      log_msg (LOG_ERR, "ERROR: Could not create timer: %m");
      return 0;
    }

  /* activate timer */
  struct itimerspec its;
  its.it_value.tv_sec = seconds;
  its.it_value.tv_nsec = 0;
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_nsec = 0;
  if (timer_settime (timer_id, 0, &its, NULL) == -1)
    {
      log_msg (LOG_ERR, "ERROR: setting the timer failed: %m");
      timer_delete (timer_id);
      return 0;
    }

  return timer_id;
}

static void
initialize_timer (int soft_reboot)
{
  int r;
  usec_t curr = now(CLOCK_REALTIME);
  usec_t next;
  usec_t duration;

  pthread_mutex_lock (&mutex_ctx);

  duration = ctx->maint_window_duration * USEC_PER_SEC;

  /* Check, if we are inside the maintenance window. If yes, reboot now */
  r = calendar_spec_next_usec (ctx->maint_window_start, curr - duration, &next);
  if (r < 0)
    {
      log_msg (LOG_ERR, "ERROR: Internal error converting the timer: %s",
	       strerror (-r));
      pthread_mutex_unlock (&mutex_ctx);
      return;
    }
  if (curr > next && curr < next + duration)
    {
      /* we are inside the maintenance window, reboot */
      pthread_mutex_unlock (&mutex_ctx);
      reboot_timer ((sigval_t) soft_reboot);
      return;
    }

  /* we are not inside a maintenance window, set timer for next one */
  r = calendar_spec_next_usec (ctx->maint_window_start, curr, &next);
  if (r < 0)
    {
      log_msg (LOG_ERR, "ERROR: Internal error converting the timer: %s",
	       strerror (-r));
      pthread_mutex_unlock (&mutex_ctx);
      return;
    }

  /* Add a random delay between 0 and duration to not reboot
     everything at the beginning of the maintenance window */
  next = next + ((usec_t)rand() * USEC_PER_SEC) % duration;

  if (debug_flag || verbose_flag)
    {
      char buf[FORMAT_TIMESTAMP_MAX];
      int64_t in_secs = (next - curr) / USEC_PER_SEC;

      log_msg (log_type,
	       "Reboot in %i seconds at %s", in_secs,
	       format_timestamp(buf, sizeof(buf), next));
    }

  ctx->reboot_status = RM_REBOOTSTATUS_WAITING_WINDOW;
  if (ctx->reboot_timer_id != 0)
    timer_delete(ctx->reboot_timer_id);

  ctx->reboot_timer_id =
    create_timer ((next - curr) / USEC_PER_SEC, soft_reboot, reboot_timer);

  pthread_mutex_unlock (&mutex_ctx);
}


/* system is requestion a reboot via dbus interface */
static void
do_reboot (void)
{
  pthread_mutex_lock (&mutex_ctx);

  ctx->reboot_status = RM_REBOOTSTATUS_REQUESTED;

  if (ctx->reboot_order == RM_REBOOTORDER_FORCED ||
      ctx->reboot_order == RM_REBOOTORDER_SOFT_FORCED)
    {
      if (debug_flag || verbose_flag)
	log_msg (log_type, "Forced reboot requested");
      pthread_mutex_unlock (&mutex_ctx);
      reboot_now (ctx->reboot_order == RM_REBOOTORDER_SOFT);
      return;
    }

  int soft_reboot = ctx->reboot_order == RM_REBOOTORDER_SOFT || ctx->reboot_order == RM_REBOOTORDER_SOFT_FAST || ctx->reboot_order == RM_REBOOTORDER_SOFT_FORCED;

  switch (ctx->reboot_strategy)
    {
    case RM_REBOOTSTRATEGY_BEST_EFFORT:
      if (ctx->maint_window_start != NULL &&
	  ctx->reboot_order != RM_REBOOTORDER_FAST &&
	  ctx->reboot_order != RM_REBOOTORDER_SOFT_FAST)
	{
	  pthread_mutex_unlock (&mutex_ctx);
	  initialize_timer(soft_reboot);
	  return;
	}
      else
	{
	  pthread_mutex_unlock (&mutex_ctx);
	  reboot_now (soft_reboot);
	  return;
	}
      break;
    case RM_REBOOTSTRATEGY_INSTANTLY:
      pthread_mutex_unlock (&mutex_ctx);
      reboot_now (soft_reboot);
      return;
      break;
    case RM_REBOOTSTRATEGY_MAINT_WINDOW:
      if (ctx->reboot_order == RM_REBOOTORDER_FAST ||
	  ctx->reboot_order == RM_REBOOTORDER_SOFT_FAST ||
	  ctx->maint_window_start == NULL)
	{
	  pthread_mutex_unlock (&mutex_ctx);
	  reboot_now (soft_reboot);
	  return;
	}
      pthread_mutex_unlock (&mutex_ctx);
      initialize_timer (soft_reboot);
      return;
      break;
    case RM_REBOOTSTRATEGY_OFF:
      ctx->reboot_status = RM_REBOOTSTATUS_NOT_REQUESTED;
      /* Do nothing */
      break;
    default:
      ctx->reboot_status = RM_REBOOTSTATUS_NOT_REQUESTED;
      log_msg (LOG_ERR, "ERROR: unknown reboot strategy %i",
	       ctx->reboot_strategy);
      break;
    }
  pthread_mutex_unlock (&mutex_ctx);
}


static DBusMessage *
handle_native_iface (DBusMessage *message)
{
  DBusError err;
  DBusMessage *reply = 0;

  /* XXX lock ctx access */

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
	      if (debug_flag || verbose_flag)
		log_msg (log_type, "Reboot at next possible time requested");
	    }
	  else if (order == RM_REBOOTORDER_FAST)
	    {
	      if (debug_flag || verbose_flag)
		log_msg (log_type, "Fast reboot requested");
	    }
	  else if (order == RM_REBOOTORDER_FORCED)
	    {
	      if (debug_flag || verbose_flag)
		log_msg (log_type, "Immediate reboot requested");
	    }
	  else if (order == RM_REBOOTORDER_SOFT)
	    {
	      if (debug_flag || verbose_flag)
		log_msg (log_type, "Soft reboot at next possible time requested");
	    }
	  else if (order == RM_REBOOTORDER_SOFT_FAST)
	    {
	      if (debug_flag || verbose_flag)
		log_msg (log_type, "Fast soft reboot requested");
	    }
	  else if (order == RM_REBOOTORDER_SOFT_FORCED)
	    {
	      if (debug_flag || verbose_flag)
		log_msg (log_type, "Immediate soft reboot requested");
	    }
	  else
	    {
	      log_msg (LOG_ERR, "ERROR: Unknown reboot order (%i), ignore reboot command",
		       order);
	      return reply;
	    }
	  ctx->reboot_order = order;
	}
      if (ctx->reboot_status > 0)
	log_msg (LOG_INFO, "Reboot already in progress, ignored");
      else
	do_reboot ();
    }
  else if (dbus_message_is_method_call (message, RM_DBUS_INTERFACE,
					RM_DBUS_METHOD_CANCEL))
    {
      log_msg (LOG_INFO, "Reboot canceld");
      if (ctx->reboot_status > 0 && ctx->reboot_timer_id)
	{
	  /* delete timer */
	  if (timer_delete (ctx->reboot_timer_id) == -1)
	    log_msg (LOG_ERR, "ERROR: deleting timer failed: %m");
	}
      ctx->reboot_status = RM_REBOOTSTATUS_NOT_REQUESTED;
      ctx->reboot_timer_id = 0;
    }
  else if (dbus_message_is_method_call (message, RM_DBUS_INTERFACE,
					RM_DBUS_METHOD_SET_STRATEGY))
    {
      RM_RebootStrategy strategy = RM_REBOOTSTRATEGY_UNKNOWN;

      if (dbus_message_get_args (message, NULL, DBUS_TYPE_UINT32,
				 &strategy, DBUS_TYPE_INVALID))
	{
	  ctx->temp_off = 0;
	  if (debug_flag)
	    log_msg (LOG_DEBUG, "set-strategy called");
	  if (strategy != RM_REBOOTSTRATEGY_UNKNOWN &&
	      ctx->reboot_strategy != strategy)
	    {
	      if (debug_flag)
		log_msg (LOG_DEBUG, "reboot_strategy changed");
	      ctx->reboot_strategy = strategy;
	      save_config (ctx, SET_STRATEGY);
	    }
	}
    }
  else if (dbus_message_is_method_call (message, RM_DBUS_INTERFACE,
					RM_DBUS_METHOD_GET_STRATEGY))
    {
      if (debug_flag)
	log_msg (LOG_DEBUG, "get-strategy called");

      /* create a reply from the message */
      if (ctx->temp_off)
	{
	  RM_RebootStrategy strategy = RM_REBOOTSTRATEGY_OFF;

	  dbus_message_append_args (reply, DBUS_TYPE_UINT32,
				    &strategy, DBUS_TYPE_INVALID);
	}
      else
	dbus_message_append_args (reply, DBUS_TYPE_UINT32,
				  &ctx->reboot_strategy, DBUS_TYPE_INVALID);
    }
  else if (dbus_message_is_method_call (message, RM_DBUS_INTERFACE,
					RM_DBUS_METHOD_STATUS))
    {
      if (debug_flag)
	log_msg (LOG_DEBUG, "Reboot status requested");

      /* create a reply from the message */
      if (ctx->temp_off)
	{
	  RM_RebootStatus tmp = RM_REBOOTSTATUS_NOT_REQUESTED;

	  dbus_message_append_args (reply, DBUS_TYPE_UINT32,
				    &tmp, DBUS_TYPE_INVALID);
	}
      else
	dbus_message_append_args (reply, DBUS_TYPE_UINT32,
				  &ctx->reboot_status, DBUS_TYPE_INVALID);
    }
  else if (dbus_message_is_method_call (message, RM_DBUS_INTERFACE,
					RM_DBUS_METHOD_GET_MAINTWINDOW))
    {
      char *str_start;
      char *str_duration;

      if (ctx->maint_window_start != NULL)
	{
	  str_start = spec_to_string(ctx->maint_window_start);
	  str_duration = duration_to_string(ctx->maint_window_duration);
	}
      else
	{
	  str_start = strdup ("");
	  str_duration = strdup ("");
	}
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
	  if (str_start && strlen (str_start) > 0)
	    {
	      if ((calendar_spec_from_string (str_start, &ctx->maint_window_start)) < 0)
		return reply;

	      if ((ctx->maint_window_duration = parse_duration (str_duration)) ==
		  BAD_TIME)
		return reply;
	    }
	  else if (ctx->maint_window_start != NULL)
	    {
	      calendar_spec_free (ctx->maint_window_start);
	      ctx->maint_window_start = NULL;
	    }
	  save_config(ctx, SET_MAINT_WINDOW);
	}
    }
  else if (dbus_message_is_method_call (message, RM_DBUS_INTERFACE,
					RM_DBUS_METHOD_TEMPORARY_OFF))
    {
      log_msg (LOG_INFO, "Switched temporary off");
      ctx->temp_off = 1;
    }
  else if (dbus_message_is_method_call (message, RM_DBUS_INTERFACE,
					RM_DBUS_METHOD_TEMPORARY_ON))
    {
      log_msg (LOG_INFO, "Enabled rebootmgr again");
      ctx->temp_off = 1;
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
		void *RM_UNUSED(user))
{
  DBusMessage *reply = 0;
  const char* iface = dbus_message_get_interface(message);

  if (dbus_message_is_method_call (message, DBUS_INTERFACE_INTROSPECTABLE,
				   "Introspect"))
    {
      /* Handle Introspection request */
      reply = handle_introspect_request (message);
    }
  else if (strcmp(iface, DBUS_INTERFACE_PROPERTIES) == 0)
    {
      /* Stub implementation for property requests */
      reply = handle_properties_iface (message);
    }
  else if (strcmp(iface, RM_DBUS_INTERFACE) == 0)
    {
      /* Handle requests to our own interfaces  */
      reply = handle_native_iface (message);
    }

  if (reply)
    {
      /* send the reply && flush the connection */
      if (!dbus_connection_send (connection, reply, NULL))
	{
	  log_msg (LOG_ERR, "ERROR: Out of memory");
	  return DBUS_HANDLER_RESULT_NEED_MEMORY;
	}
      dbus_message_unref(reply);
    }
  return DBUS_HANDLER_RESULT_HANDLED;
}


static int dbus_init (void);

static void
dbus_reconnect (sigval_t RM_UNUSED(user_data))
{
  int status;

  status = dbus_init ();
  if (status < 0)
    log_msg (LOG_ERR, "ERROR: Reconnect to dbus failed");
  else
    log_msg (LOG_INFO, "Reconnect to dbus was successful");
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
      create_timer (1, 0, dbus_reconnect);
      handled = DBUS_HANDLER_RESULT_HANDLED;
    }

  return handled;
}

/*
  init dbus
  return value:
  0: success
  -1: error
*/
static int
dbus_init ()
{
  DBusConnection *connection = NULL;
  DBusError error;

  dbus_error_init (&error);

  connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
  if (connection == NULL || dbus_error_is_set (&error))
    {
      log_msg (LOG_ERR, "ERROR: Connection to D-BUS system message bus failed: %s",
               error.message);
      dbus_error_free (&error);
      connection = NULL;
      goto out;
    }

  dbus_bool_t ret = dbus_bus_name_has_owner (connection,
					     RM_DBUS_NAME, &error);
  if (dbus_error_is_set (&error))
    {
      log_msg (LOG_ERR, "ERROR: DBus failure: %s", error.message);
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
	  log_msg (LOG_ERR, "ERROR: DBus failure requesting a bus name: %s", error.message);
	  dbus_error_free (&error);
	  return -1;
	}
      if ( request_name_reply == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER )
	{
	  if (debug_flag)
	    log_msg (LOG_DEBUG, "Bus name %s successfully reserved!",
		     RM_DBUS_NAME);
	}
      else
	{
	  log_msg (LOG_ERR, "ERROR: Failed to reserve name %s", RM_DBUS_NAME);
	  return -1;
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
      log_msg (LOG_ERR, "ERROR: %s is already reserved", RM_DBUS_NAME);
      return -1;
    }

  dbus_bus_add_match (connection, "type='signal',"
		      "interface='" DBUS_INTERFACE_DBUS "',"
		      "sender='" DBUS_SERVICE_DBUS "'",
		      &error);
  if (dbus_error_is_set (&error))
    {
      log_msg (LOG_ERR, "ERROR: Failure adding match for dbus interface, %s: %s",
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
      log_msg (LOG_ERR, "ERROR: Failure adding match for rebootmgrd interface, %s: %s",
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
					    &vtable, NULL))
    log_msg(LOG_ERR, "ERROR: Failed to register object path");

  pthread_mutex_lock (&mutex_ctx);
  ctx->connection = connection;
  pthread_mutex_unlock (&mutex_ctx);

  return 0;

 out:
  if (connection)
    {
      dbus_bus_release_name (connection, RM_DBUS_NAME, &error);
      dbus_connection_unref (connection);
      return -1;
    }
  else
    {
      if (debug_flag)
	log_msg (LOG_DEBUG, "No connection possible");
      return -1;
    }
}

int
main (int argc, char **argv)
{
  while (1)
    {
      int c;
      int option_index = 0;
      static struct option long_options[] =
	{
	  {"debug", no_argument, NULL, 'd'},
	  {"verbose", no_argument, NULL, 'v'},
	  {"version", no_argument, NULL, '\255'},
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
	  log_type = LOG_DEBUG;
	  break;
	case '?':
	case 'h':
          print_help ();
          return 0;
	case 'v':
	  verbose_flag = 1;
	  log_type = LOG_INFO;
	  break;
	case '\255':
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

  if (!create_context ())
    {
      log_msg (LOG_ERR, "ERROR: Could not initialize context");
      return -1;
    }

  pthread_mutex_lock (&mutex_ctx);
  load_config (ctx);
  pthread_mutex_unlock (&mutex_ctx);

  if (dbus_init () != 0)
    return 1;

  while (true)
    {
      while (dbus_connection_read_write_dispatch (ctx->connection, -1))
	; // empty loop body

      if (dbus_connection_get_is_connected (ctx->connection))
	break;
      else
	{
	  if (dbus_init () != 0)
	    return 1;
	}
    }

  if (!destroy_context ())
    log_msg (LOG_ERR, "ERROR: Could not destroy context");

  return 0;
}
