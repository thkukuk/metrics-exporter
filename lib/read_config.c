// SPDX-License-Identifier: GPL-2.0-or-later

#include <pwd.h>
#include <ctype.h>
#include <errno.h>
#include <libeconf.h>

#include "basics.h"
#include "read_config.h"

config_t *
struct_config_free(config_t _unused_(*cfg))
{
  // nothing to do yet, maybe later
  return NULL;
}

/* we will read everything and only report the first error */
econf_err
read_config(const char *name, config_t *cfg)
{
  _cleanup_(econf_freeFilep) econf_file *key_file = NULL;
  econf_err error;

  /* This looks for metrics-exporter.conf{.d} in /usr/share/metrics-exporter/
     or /etc/account-utils/ */
  error = econf_readConfig(&key_file,
			   "metrics-exporter", /* project name */
			   "/usr/share",       /* directory below /usr */
			   name,               /* file name without extension */
			   "conf",             /* suffix */
			   "=",                /* delimiter */
			   "#");               /* comment */
  if (error != ECONF_SUCCESS)
    return error;

  error = econf_getBoolValue(key_file, "[Global]", "verbose", &(cfg->verbose));
  if (error != ECONF_SUCCESS && error != ECONF_NOGROUP)
    return error;
  error = econf_getBoolValue(key_file, "[Global]", "debug", &(cfg->debug));
  if (error)
    return error;
  // if debug is enabled, also print verbose messages
  if (cfg->debug)
    cfg->verbose = true;

  error = econf_getBoolValue(key_file, "[Metrics]", "selinux", &(cfg->selinux));
  if (error != ECONF_SUCCESS && error != ECONF_NOGROUP)
    return error;

  error = econf_getBoolValue(key_file, "[SELinux]", "booleans", &(cfg->selinux_booleans));
  if (error != ECONF_SUCCESS && error != ECONF_NOGROUP)
    return error;


  return ECONF_SUCCESS;
}
