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

#include <libeconf.h>

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

  if(buf == NULL){
    log_msg( LOG_INFO, "buf malloc failed in :%s\n",__func__);
    goto fail;
  }

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

  if(buf){
    free(buf);
  }
  return NULL;

}

#define RM_CONFIG_FILE SYSCONFDIR"/rebootmgr.conf"
#define RM_GROUP "rebootmgr"

void
load_config (RM_CTX *ctx)
{
  econf_file *file = NULL, *file_1 = NULL, *file_2 = NULL,
    *file_m = NULL;
  econf_err error;

  error = econf_readFile (&file_1, DISTCONFDIR"/rebootmgr.conf", "=", "#");
  if (error && error != ECONF_NOFILE)
    {
      log_msg (LOG_ERR, "Cannot load '"DISTCONFDIR"/rebootmgr.conf': %s",
	       econf_errString(error));
      return;
    }

  error = econf_readFile (&file_2, SYSCONFDIR"/rebootmgr.conf", "=", "#");
  if (error && error != ECONF_NOFILE)
    {
      log_msg (LOG_ERR, "Cannot load '"SYSCONFDIR"/rebootmgr.conf': %s",
	       econf_errString(error));
      return;
    }

  if (file_1 != NULL && file_2 != NULL)
    {
      if ((error = econf_mergeFiles (&file_m, file_1, file_2)))
	{
	  log_msg (LOG_ERR, "Cannot merge 'rebootmgr.conf': %s",
		   econf_errString(error));
	  return;
	}
      file = file_m;
    }
  else if  (file_2 != NULL)
    file = file_2;
  else if (file_1 != NULL)
    file = file_1;

  if (file == NULL) /* happens if no config file exists */
      log_msg (LOG_ERR, "Cannot load 'rebootmgr.conf'");
  else
    {
      char *str_start = NULL, *str_duration = NULL, *str_strategy = NULL;

      error = econf_getStringValue(file, RM_GROUP, "window-start", &str_start);
      if (error && error != ECONF_NOKEY)
	{
	  log_msg (LOG_ERR, "ERROR (econf): cannot get key 'window-start': %s",
		   econf_errString(error));
	  goto out;
	}
      error = econf_getStringValue (file, RM_GROUP, "window-duration", &str_duration);
      if (error && error != ECONF_NOKEY)
	{
	  log_msg (LOG_ERR, "ERROR (econf): cannot get key 'window-duration': %s",
		   econf_errString(error));
	  goto out;
	}
      error = econf_getStringValue (file, RM_GROUP, "strategy", &str_strategy);
      if (error && error != ECONF_NOKEY)
	{
	  log_msg (LOG_ERR, "ERROR (econf): cannot get key 'strategy': %s",
		   econf_errString(error));
	  goto out;
	}

      if (str_start == NULL && str_duration != NULL)
	{
	  free (str_duration);
	  str_duration = NULL;
	}
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
    out:
      if (file_1)
	econf_free(file_1);
      if (file_2)
	econf_free(file_2);
      if (file_m)
	econf_free(file_m);
      if (str_start)
	free (str_start);
      if (str_duration)
	free (str_duration);
      if (str_strategy)
	free (str_strategy);
    }
}


void
save_config (RM_CTX *ctx, int field)
{
  econf_file *file = NULL;
  econf_err error;

  error = econf_readFile (&file, SYSCONFDIR"/rebootmgr.conf", "=", "#");
  if (error)
    {
      if (error != ECONF_NOFILE)
	{
	  log_msg (LOG_ERR, "Cannot load '"SYSCONFDIR"rebootmgr.conf': %s",
		   econf_errString(error));
	  return;
	}
      else
	{
	  if ((error = econf_newKeyFile (&file, '=', '#')))
	    {
	      log_msg (LOG_ERR, "Cannot create new config file: %s",
		       econf_errString(error));
	      return;
	    }
	}
    }

  switch (field)
    {
      char *p;
    case SET_STRATEGY:
      error = econf_setStringValue (file, RM_GROUP, "strategy",
				    strategy_to_string(ctx->reboot_strategy, NULL));
      break;
    case SET_MAINT_WINDOW:
      p = spec_to_string(ctx->maint_window_start);
      error = econf_setStringValue (file, RM_GROUP, "window-start", p);
      free (p);
      if (!error)
	{
	  p = duration_to_string(ctx->maint_window_duration);
	  error = econf_setStringValue (file, RM_GROUP, "window-duration", p);
	  free (p);
	}
      break;
    default:
      log_msg (LOG_ERR, "Error writing config file, unknown field %i", field);
      econf_free (file);
      return;
    }

  if (error)
    {
      log_msg (LOG_ERR, "Error setting variable: %s\n", econf_errString(error));
      econf_free (file);
      return;
    }

  if ((error = econf_writeFile(file, SYSCONFDIR"/", "rebootmgr.conf")))
    log_msg (LOG_ERR, "Error writing "SYSCONFDIR"/rebootmgr.conf: %s",
	     econf_errString(error));

  econf_free (file);
}
