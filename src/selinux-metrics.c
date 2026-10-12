// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-json.h>
#include <systemd/sd-varlink.h>
#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#endif

#include "basics.h"
#include "log_msg.h"
#include "metrics-varlink.h"
#include "selinux-metrics.h"

static int
metrics_selinux_enabled_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  if (is_selinux_enabled() > 0)
    return metric_build_send_string(mf, link, "enabled");
  else
    return metric_build_send_string(mf, link, "disabled");
#else
  return 0;
#endif
}

static int
metrics_selinux_mode_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  int r = security_getenforce();
  if (r == 0)
    return metric_build_send_string(mf, link, "permissive");
  else if (r == 1)
    return metric_build_send_string(mf, link, "enforcing");
  else
    return metric_build_send_string(mf, link, "unknown");
#else
  return 0;
#endif
}

static const MetricFamily selinux_metric_family_table[] = {
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "SELinux.Enabled",
    .description = "SELinux enabled/disabled",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_selinux_enabled_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "SELinux.Mode",
    .description = "SELinux mode: permissive or enforcing",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_selinux_mode_generate,
  },
  {}
};

int
selinux_metrics_describe(sd_varlink *link)
{
  int r;

  for (const MetricFamily *mf = selinux_metric_family_table; mf->name; mf++)
    {
      _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;

      r = sd_json_buildo(&v,
			 SD_JSON_BUILD_PAIR_STRING("name", mf->name),
			 SD_JSON_BUILD_PAIR_STRING("description", mf->description),
			 SD_JSON_BUILD_PAIR_STRING("type", metric_family_type_to_str(mf->type)));
      if (r < 0)
	return r;

      r = sd_varlink_reply(link, v);
      if (r < 0)
	return r;
    }

  return 0;
}

int
selinux_metrics_list(sd_varlink *link)
{
  int r;

  for (const MetricFamily *mf = selinux_metric_family_table; mf->name; mf++)
    {
      r = mf->generate(mf, link);
      if (r < 0)
	{
	  log_msg(LOG_DEBUG, "Failed to list metric family '%s': %s", mf->name, strerror(-r));
	  return r;
	}
    }

  return 0;
}
