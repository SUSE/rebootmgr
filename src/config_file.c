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


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include <glib.h>

#include "calendarspec.h"
#include "parse-duration.h"

#include "config_file.h"
#include "log_msg.h"

static RM_RebootStrategy load_strategy(const char *str_strategy);
static const char* save_strategy(RM_RebootStrategy strategy);

#define DURATION_LEN 10

char* spec_to_string(CalendarSpec *spec)
{
  char *str_start;
  if (calendar_spec_to_string(spec, &str_start) > 0) {
    return 0;
  }
  return str_start;
}
char* duration_to_string(time_t duration)
{
  char buf[DURATION_LEN];
  char *p;
  if (strftime(buf, DURATION_LEN, "%Hh%Mm", gmtime(&duration)) == 0) {
    return 0;
  }
  p = malloc(DURATION_LEN);
  strncpy(p, buf, DURATION_LEN);

  return p;
}

char*
get_file_content(const char *fname)
{
  struct stat st;
  ssize_t size;
  char *buf = NULL;
  int fd = open(fname, O_RDONLY);

  if (fd < 0) {
    log_msg( LOG_INFO, "Failed to open %s: %s\n", fname, strerror(errno));
    goto fail;
  }

  if (fstat(fd, &st) < 0) {
    log_msg( LOG_INFO, "stat(%s) failed: %s\n", fname, strerror(errno));
    goto fail;
  }

  if (!(S_ISREG(st.st_mode))) {
    log_msg( LOG_INFO, "Invalid file %s\n", fname);
    goto fail;
  }

  buf = (char*)malloc(sizeof(char)*(size_t)st.st_size+1);

  if ((size = read(fd, buf, (size_t)st.st_size)) < 0) {
    log_msg( LOG_INFO, "read() failed: %s\n", strerror(errno));
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
#define RM_GROUP "rebootmgr"
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
    log_msg (LOG_ERR, "Cannot load '"CONFIG_FILE"': %s", error->message);
    g_error_free(error);
  }

  g_key_file_set_string(key_file, RM_GROUP, "strategy", save_strategy(ctx->reboot_strategy));
  char *p = spec_to_string(ctx->maint_window_start);
  g_key_file_set_string(key_file, RM_GROUP, "window-start", p);
  free(p);
  p = duration_to_string(ctx->maint_window_duration);
  g_key_file_set_string(key_file, RM_GROUP, "window-duration", p);
  free(p);

  error = NULL;
  if (!g_key_file_save_to_file(key_file, RM_CONFIG_FILE, &error))
  {
    log_msg (LOG_ERR, "Cannot save to '"RM_CONFIG_FILE"': %s", error->message);
    g_error_free(error);
  }

}

void
load_config (RM_CTX *ctx)
{
  GKeyFile *key_file;
  GError *error;
  gchar *str_start = NULL, *str_duration = NULL, *str_strategy = NULL;
  int ret;

  key_file = g_key_file_new ();
  error = NULL;

  if (!g_key_file_load_from_file (key_file, RM_CONFIG_FILE,
                                  G_KEY_FILE_KEEP_COMMENTS |
                                  G_KEY_FILE_KEEP_TRANSLATIONS,
                                  &error))
    {
      log_msg (LOG_ERR, "Cannot load '"CONFIG_FILE"': %s", error->message);
    }
  else
    {
      str_start = g_key_file_get_string (key_file, RM_GROUP, "window-start", NULL);
      str_duration = g_key_file_get_string (key_file, RM_GROUP,
                                            "window-duration", NULL);
      str_strategy = g_key_file_get_string(key_file, RM_GROUP, "strategy", NULL);
    }
  if (str_start == NULL)
    str_start = "03:30";
  if (str_duration == NULL)
    str_duration = "1h";
  ctx->reboot_strategy = load_strategy(str_strategy);
  if ((ret = calendar_spec_from_string (str_start, &ctx->maint_window_start)) < 0)
    log_msg (LOG_ERR, "ERROR: cannot parse window-start (%s): %s",
             str_start, strerror (-ret));
  if ((ctx->maint_window_duration = parse_duration (str_duration)) == BAD_TIME)
    log_msg (LOG_ERR, "ERROR: cannot parse window-duration '%s'",
             str_duration);
}

static RM_RebootStrategy
load_strategy(const char *str_strategy)
{
  if (!str_strategy)
    return RM_REBOOTSTRATEGY_BEST_EFFORT;

  if (strcasecmp (str_strategy, "best-effort") == 0)
    return RM_REBOOTSTRATEGY_BEST_EFFORT;
  else if (strcasecmp (str_strategy, "instantly") == 0)
    return RM_REBOOTSTRATEGY_INSTANTLY;
  else if (strcasecmp (str_strategy, "maint_window") == 0 ||
     strcasecmp (str_strategy, "maint-window") == 0)
    return RM_REBOOTSTRATEGY_MAINT_WINDOW;
  else if (strcasecmp (str_strategy, "etcd-lock") == 0)
    return RM_REBOOTSTRATEGY_ETCD_LOCK;
  else if (strcasecmp (str_strategy, "off") == 0)
    return RM_REBOOTSTRATEGY_OFF;

  // else
  log_msg (LOG_ERR, "ERROR: cannot parse strategy '%s'", str_strategy);
  return RM_REBOOTSTRATEGY_BEST_EFFORT;
}

static const char*
save_strategy(RM_RebootStrategy strategy)
{
  switch(strategy) {
  case RM_REBOOTSTRATEGY_INSTANTLY:
    return "instantly";
  case RM_REBOOTSTRATEGY_MAINT_WINDOW:
    return "maint_window";
  case RM_REBOOTSTRATEGY_ETCD_LOCK:
    return "etcd-lock";
  case RM_REBOOTSTRATEGY_OFF:
    return "off";
  case RM_REBOOTSTRATEGY_BEST_EFFORT:
      return "best-effort";
  case RM_REBOOTSTRATEGY_UNKNOWN:
    log_msg (LOG_ERR, "ERROR: can't save unknown strategy, defaulting to best-effort");
  }

  return "best-effort";
}
