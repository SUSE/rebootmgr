//SPDX-License-Identifier: GPL-2.0-or-later

/* Copyright (c) 2024 Thorsten Kukuk
   Author: Thorsten Kukuk <kukuk@suse.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, see <http://www.gnu.org/licenses/>. */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>

#include "common.h"

int
mkdir_p(const char *path, mode_t mode)
{
  if (path == NULL)
    return -EINVAL;

  if (mkdir(path, mode) == 0)
    return 0;

  if (errno == EEXIST)
    {
      struct stat st;

      /* Check if the existing path is a directory */
      if (stat(path, &st) != 0)
	return -errno;

      /* If not, fail with ENOTDIR */
      if (!S_ISDIR(st.st_mode))
	return -ENOTDIR;

      /* if it is a directory, return */
      return 0;
    }

  /* If it fails for any reason but ENOENT, fail */
  if (errno != ENOENT)
    return -errno;

  char *buf = strdup(path);
  if (buf == NULL)
    return -ENOMEM;

  int r = mkdir_p(dirname(buf), mode);
  free(buf);
  /* if we couldn't create the parent, fail, too */
  if (r < 0)
    return r;

  return mkdir(path, mode);
}
