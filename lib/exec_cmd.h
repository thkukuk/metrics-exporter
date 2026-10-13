// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "basics.h"

/* The variadic argument list must be NULL-terminated. */
extern int exec_cmd(char **output, const char *cmd, ...) _sentinel_;
