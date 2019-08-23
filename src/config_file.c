/* Copyright (c) 2016, 2017, 2019 Daniel Molkentin
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#if USE_ECONF
#include <libeconf.h>
#else
#include <glib.h>
#endif

#include "calendarspec.h"
#include "parse-duration.h"

#include "config_file.h"
#include "log_msg.h"
#include "util.h"

char*
get_file_content(const char *fname)
{
  struct stat st;
  ssize_t size;
  char *buf = NULL;
  int fd = open(fname, O_RDONLY);

  if (fd < 0) {
    log_msg( LOG_INFO, "Failed to open %s: %m\n", fname);
    goto fail;
  }

  if (fstat(fd, &st) < 0) {
    log_msg( LOG_INFO, "stat(%s) failed: %m", fname);
    goto fail;
  }

  if (!(S_ISREG(st.st_mode))) {
    log_msg( LOG_INFO, "Invalid file %s\n", fname);
    goto fail;
  }

  buf = (char*)malloc(sizeof(char)*(size_t)st.st_size+1);

  if ((size = read(fd, buf, (size_t)st.st_size)) < 0) {
    log_msg( LOG_INFO, "read() failed: %m");
    goto fail;
  }

  buf[size] = 0;
  close(fd);
  return buf;

fail:
  if (fd >= 0) {
    close(fd);
  }

  free(buf);

  return NULL;

}

#define RM_CONFIG_FILE SYSCONFDIR"/rebootmgr.conf"
#define RM_CONFIG2_FILE "/usr/"SYSCONFDIR"/rebootmgr.conf"
#define RM_GROUP "rebootmgr"

#if USE_ECONF
void
load_config (RM_CTX *ctx)
{
  Key_File *key_file = NULL, *key_file_1 = NULL, *key_file_2 = NULL,
    *key_file_m = NULL;
  econf_err error;

  key_file_1 = econf_get_key_file (DISTCONFDIR"/rebootmgr.conf", "=", '#', &error);
  if (key_file_1 == NULL)
    {
      log_msg (LOG_ERR, "Cannot load '"DISTCONFDIR"/rebootmgr.conf': %s",
	       econf_errString(error));
      return;
    }

  key_file_2 = econf_get_key_file (SYSCONFDIR"/rebootmgr.conf", "=", '#', &error);
  if (key_file_2 == NULL)
    {
      log_msg (LOG_ERR, "Cannot load '"SYSCONFDIR"rebootmgr.conf': %s",
	       econf_errString(error));
      return;
    }

  if (key_file_1 != NULL && key_file_2 != NULL)
    {
      key_file_m = econf_merge_key_files (key_file_1, key_file_2, &error);
      if (key_file_m == NULL)
	{
	  log_msg (LOG_ERR, "Cannot merge 'rebootmgr.conf': %s",
		   econf_errString(error));
	  return;
	}
      key_file = key_file_m;
    }
  else if  (key_file_2 != NULL)
    key_file = key_file_2;
  else if (key_file_1 != NULL)
    key_file = key_file_1;

  if (key_file == NULL) /* should not happen ... */
    {
      log_msg (LOG_ERR, "Cannot load 'rebootmgr.conf'");
    }
  else
    {
      char *str_start = NULL, *str_duration = NULL, *str_strategy = NULL, *lock_group = NULL;

      str_start = econf_getStringValue(key_file, RM_GROUP, "window-start");
      str_duration = econf_getStringValue (key_file, RM_GROUP, "window-duration");
      str_strategy = econf_getStringValue (key_file, RM_GROUP, "strategy");
      lock_group = econf_getStringValue (key_file, RM_GROUP, "lock-group");

      if (str_start == NULL && str_duration != NULL)
	str_duration = NULL;
      ctx->reboot_strategy = string_to_strategy(str_strategy, NULL);
      if (str_start != NULL)
	{
	  int ret;

	  if ((ret = calendar_spec_from_string (str_start,
						&ctx->maint_window_start)) < 0)
	    log_msg (LOG_ERR, "ERROR: cannot parse window-start (%s): %s",
		     str_start, strerror (-ret));
	  if ((ctx->maint_window_duration =
	       parse_duration (str_duration)) == BAD_TIME)
	    log_msg (LOG_ERR, "ERROR: cannot parse window-duration '%s'",
		     str_duration);
	}
      if (ctx->lock_group)
	free (ctx->lock_group);
      if (lock_group == NULL)
	ctx->lock_group = strdup ("default");
      else
	ctx->lock_group = strdup (lock_group);
      if (key_file_1)
	econf_destroy(key_file_1);
      if (key_file_2)
	econf_destroy(key_file_2);
      if (key_file_m)
	econf_destroy(key_file_m);
      if (str_start)
	free (str_start);
      if (str_duration)
	free (str_duration);
      if (str_strategy)
	free (str_strategy);
      if (lock_group)
	free (lock_group);
    }
}


void
save_config (RM_CTX *ctx)
{
#if 0
  GKeyFile *key_file;
  GError *error;

  key_file = g_key_file_new ();
  error = NULL;

  if (!g_key_file_load_from_file (key_file, RM_CONFIG_FILE,
                                  G_KEY_FILE_KEEP_COMMENTS |
                                  G_KEY_FILE_KEEP_TRANSLATIONS,
                                  &error))
  {
    log_msg (LOG_ERR, "Cannot load '"RM_CONFIG_FILE"': %s", error->message);
    g_error_free(error);
  }

  g_key_file_set_string(key_file, RM_GROUP, "strategy", strategy_to_string(ctx->reboot_strategy, NULL));
  if (ctx->maint_window_start != NULL)
    {
      char *p = spec_to_string(ctx->maint_window_start);
      g_key_file_set_string(key_file, RM_GROUP, "window-start", p);
      free(p);
      p = duration_to_string(ctx->maint_window_duration);
      g_key_file_set_string(key_file, RM_GROUP, "window-duration", p);
      free(p);
    }
  g_key_file_set_string (key_file, RM_GROUP, "lock-group", ctx->lock_group);

  error = NULL;
  if (!g_key_file_save_to_file(key_file, RM_CONFIG_FILE, &error))
  {
    log_msg (LOG_ERR, "Cannot save to '"RM_CONFIG_FILE"': %s", error->message);
    g_error_free(error);
  }

  g_key_file_free (key_file);
#endif
}

#else
void
save_config (RM_CTX *ctx)
{
  GKeyFile *key_file;
  GError *error;

  key_file = g_key_file_new ();
  error = NULL;

  if (!g_key_file_load_from_file (key_file, RM_CONFIG_FILE,
                                  G_KEY_FILE_KEEP_COMMENTS |
                                  G_KEY_FILE_KEEP_TRANSLATIONS,
                                  &error))
  {
    log_msg (LOG_ERR, "Cannot load '"RM_CONFIG_FILE"': %s", error->message);
    g_error_free(error);
  }

  g_key_file_set_string(key_file, RM_GROUP, "strategy", strategy_to_string(ctx->reboot_strategy, NULL));
  if (ctx->maint_window_start != NULL)
    {
      char *p = spec_to_string(ctx->maint_window_start);
      g_key_file_set_string(key_file, RM_GROUP, "window-start", p);
      free(p);
      p = duration_to_string(ctx->maint_window_duration);
      g_key_file_set_string(key_file, RM_GROUP, "window-duration", p);
      free(p);
    }
  g_key_file_set_string (key_file, RM_GROUP, "lock-group", ctx->lock_group);

  error = NULL;
  if (!g_key_file_save_to_file(key_file, RM_CONFIG_FILE, &error))
  {
    log_msg (LOG_ERR, "Cannot save to '"RM_CONFIG_FILE"': %s", error->message);
    g_error_free(error);
  }

  g_key_file_free (key_file);
}

void
load_config (RM_CTX *ctx)
{
  GKeyFile *key_file;
  GError *error;

  key_file = g_key_file_new ();
  error = NULL;

  if (!g_key_file_load_from_file (key_file, RM_CONFIG_FILE,
                                  G_KEY_FILE_KEEP_COMMENTS |
                                  G_KEY_FILE_KEEP_TRANSLATIONS,
                                  &error))
    {
      log_msg (LOG_ERR, "Cannot load '"RM_CONFIG_FILE"': %s", error->message);
    }
  else
    {
      gchar *str_start = NULL, *str_duration = NULL, *str_strategy = NULL, *lock_group = NULL;

      str_start = g_key_file_get_string (key_file, RM_GROUP, "window-start", NULL);
      str_duration = g_key_file_get_string (key_file, RM_GROUP,
                                            "window-duration", NULL);
      str_strategy = g_key_file_get_string (key_file, RM_GROUP, "strategy", NULL);
      lock_group = g_key_file_get_string (key_file, RM_GROUP, "lock-group", NULL);

      if (str_start == NULL && str_duration != NULL)
	str_duration = NULL;
      ctx->reboot_strategy = string_to_strategy(str_strategy, NULL);
      if (str_start != NULL)
	{
	  int ret;

	  if ((ret = calendar_spec_from_string (str_start,
						&ctx->maint_window_start)) < 0)
	    log_msg (LOG_ERR, "ERROR: cannot parse window-start (%s): %s",
		     str_start, strerror (-ret));
	  if ((ctx->maint_window_duration =
	       parse_duration (str_duration)) == BAD_TIME)
	    log_msg (LOG_ERR, "ERROR: cannot parse window-duration '%s'",
		     str_duration);
	}
      if (ctx->lock_group)
	free (ctx->lock_group);
      if (lock_group == NULL)
	ctx->lock_group = strdup ("default");
      else
	ctx->lock_group = strdup (lock_group);
      g_key_file_free (key_file);
      if (str_start)
	free (str_start);
      if (str_duration)
	free (str_duration);
      if (str_strategy)
	free (str_strategy);
      if (lock_group)
	free (lock_group);
    }
}
#endif
