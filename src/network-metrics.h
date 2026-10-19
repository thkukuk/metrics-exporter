// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>

extern int network_metrics_describe(sd_varlink *link);
extern int network_metrics_list(sd_varlink *link);
