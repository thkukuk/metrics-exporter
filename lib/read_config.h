// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <libeconf.h>

typedef struct config_t {
  bool debug;
  bool verbose;
  bool network;
  bool packages;
  bool selinux;
  bool selinux_booleans;
} config_t;

extern struct config_t *struct_config_free(config_t *cfg);
extern econf_err read_config(const char *name, config_t *cfg);
