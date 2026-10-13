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
metrics_selinux_status_generate(const MetricFamily *mf, sd_varlink *link)
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
    {
      // XXX return error string with errno
      return metric_build_send_string(mf, link, "unknown");
    }
#else
  return 0;
#endif
}

static int
metrics_selinux_mode_configfile_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  int rc;
  int r = selinux_getenforcemode(&rc);
  if (r != 0)
    {
      // XXX return error string with errno
      return 0;
    }
  if (rc == 0)
    return metric_build_send_string(mf, link, "permissive");
  else if (rc == 1)
    return metric_build_send_string(mf, link, "enforcing");
  else if (rc == -1)
    return metric_build_send_string(mf, link, "disabled");
  else
    return metric_build_send_string(mf, link, "unknown");
#else
  return 0;
#endif
}

static int
metrics_selinux_root_dir_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  const char *root_dir = selinux_path();
  if (isempty(root_dir))
    return 0;
  // Remove trailing "/"
  return metric_build_send_string(mf, link, root_dir);
#else
  return 0;
#endif
}

static int
metrics_selinux_policy_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  const char *policy_path = selinux_policy_root();
  if (isempty(policy_path))
    return 0;
  return metric_build_send_string(mf, link, basename(policy_path));
#else
  return 0;
#endif
}

static int
metrics_selinux_policy_mls_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  int r = is_selinux_mls_enabled();
  if (r == 0)
    return metric_build_send_string(mf, link, "disabled");
  else if (r == 1)
    return metric_build_send_string(mf, link, "enabled");
  else
    {
      // XXX error string
      return metric_build_send_string(mf, link, "ERROR");
    }
#else
  return 0;
#endif
}

static int
metrics_selinux_policy_deny_unknown_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  int r = security_deny_unknown();
  if (r == 0)
    return metric_build_send_string(mf, link, "allowed");
  else if (r == 1)
    return metric_build_send_string(mf, link, "denied");
  else
    {
      // XXX return error string with errno
      return metric_build_send_string(mf, link, "ERROR");
    }
#else
  return 0;
#endif
}

static int
metrics_selinux_checkreqprot_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  int r = security_get_checkreqprot();
  if (r == 0)
    return metric_build_send_string(mf, link, "actual (secure)");
  else if (r == 1)
    return metric_build_send_string(mf, link, "requested (insecure)");
  else
    {
      // XXX return error string with errno
      return metric_build_send_string(mf, link, "ERROR");
    }
#else
  return 0;
#endif
}


static const MetricFamily selinux_metric_family_table[] = {
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "SELinux.Status",
    .description = "SELinux Status (enabled/disabled)",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_selinux_status_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "SELinux.Mode",
    .description = "SELinux mode: permissive or enforcing",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_selinux_mode_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "SELinux.ConfigfileMode",
    .description = "SELinux mode from config file",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_selinux_mode_configfile_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "SELinux.RootDir",
    .description = "SELinux root directory",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_selinux_root_dir_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "SELinux.Policy",
    .description = "Loaded SELinux policy name",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_selinux_policy_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "SELinux.PolicyMLS",
    .description = "SELinux Policy MLS status",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_selinux_policy_mls_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "SELinux.PolicyDenyUnknown",
    .description = "SELinux Policy deny_unknown status",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_selinux_policy_deny_unknown_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "SELinux.CheckReqProt",
    .description = "SELinux Memory protection checking",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_selinux_checkreqprot_generate,
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
