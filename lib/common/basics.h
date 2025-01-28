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

#pragma once

#define _cleanup_(x) __attribute__((__cleanup__(x)))
#define _unused_(x) x __attribute__((unused))

#define mfree(memory)                           \
        ({                                      \
                free(memory);                   \
                (typeof(memory)) NULL;          \
        })

static inline void freep(void *p) {
        *(void**)p = mfree(*(void**) p);
}

