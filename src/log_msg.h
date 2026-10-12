//SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/* logging */
#include <syslog.h>
extern int debug_flag;
extern void log_init (void);
extern void log_msg (int priority, const char *fmt, ...);
