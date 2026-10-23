// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "basics.h"

/* The variadic argument list must be NULL-terminated.
   "extra_env" is an optional NULL-terminated array of "NAME=VALUE"
   strings which are added to the environment of the executed command;
   it may be NULL if no extra environment variables are needed. */
extern int exec_cmd(char **output, char *const *extra_env, const char *cmd, ...) _sentinel_;
