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
#ifdef WITH_LIBRPM
#include <rpm/rpmlib.h>
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>
#include <rpm/header.h>
#else
#include "exec_cmd.h"
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
    return metric_build_send_string(mf, link, NULL, "enabled", NULL);
  else
    return metric_build_send_string(mf, link, NULL, "disabled", NULL);
#else
  return 0;
#endif
}

static int
metrics_selinux_mode_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  int r;

  if (!is_selinux_enabled())
    return 0;

  r = security_getenforce();
  if (r == 0)
    return metric_build_send_string(mf, link, NULL, "permissive", NULL);
  else if (r == 1)
    return metric_build_send_string(mf, link, NULL, "enforcing", NULL);
  else
    {
      r = -errno;
      log_msg(LOG_ERR, "security_getenforce() failed: %s", strerror(-r));
      return r;
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
  int r;

  if (!is_selinux_enabled())
    return 0;

  r = selinux_getenforcemode(&rc);
  if (r != 0)
    {
      r = -errno;
      log_msg(LOG_ERR, "selinux_getenforcemode() failed: %s", strerror(-r));
      return r;
    }

  if (rc == 0)
    return metric_build_send_string(mf, link, NULL, "permissive", NULL);
  else if (rc == 1)
    return metric_build_send_string(mf, link, NULL, "enforcing", NULL);
  else if (rc == -1)
    return metric_build_send_string(mf, link, NULL, "disabled", NULL);
  else
    return metric_build_send_string(mf, link, NULL, "unknown", NULL);
#else
  return 0;
#endif
}

static int
metrics_selinux_root_dir_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  _cleanup_free_ char *cp = NULL;
  const char *root_dir;
  size_t len;

  root_dir = selinux_path();
  if (isempty(root_dir))
    return 0;

  cp = strdup(root_dir);
  if (cp == NULL)
    {
      log_msg(LOG_ERR, "%s", strerror(ENOMEM));
      return -ENOMEM;
    }

  len = strlen(cp);
  if (cp[len-1] == '/')
    cp[len-1] = '\0';

  return metric_build_send_string(mf, link, NULL, cp, NULL);
#else
  return 0;
#endif
}

static int
metrics_selinux_policy_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  const char *policy_path;

  if (!is_selinux_enabled())
    return 0;

  policy_path = selinux_policy_root();
  if (isempty(policy_path))
    return 0;

  return metric_build_send_string(mf, link, NULL, basename(policy_path), NULL);
#else
  return 0;
#endif
}

static int
metrics_selinux_policy_mls_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  int r;

  if (!is_selinux_enabled())
    return 0;

  r = is_selinux_mls_enabled();
  if (r == 0)
    return metric_build_send_string(mf, link, NULL, "disabled", NULL);
  else if (r == 1)
    return metric_build_send_string(mf, link, NULL, "enabled", NULL);
  else
    {
      r = -errno;
      log_msg(LOG_ERR, "is_selinux_mls_enabled() failed: %s", strerror(-r));
      return r;
    }
#else
  return 0;
#endif
}

static int
metrics_selinux_policy_deny_unknown_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  int r;

  if (!is_selinux_enabled())
    return 0;

  r = security_deny_unknown();
  if (r == 0)
    return metric_build_send_string(mf, link, NULL, "allowed", NULL);
  else if (r == 1)
    return metric_build_send_string(mf, link, NULL, "denied", NULL);
  else
    {
      r = -errno;
      log_msg(LOG_ERR, "security_deny_unknown() failed: %s", strerror(-r));
      return r;
    }
#else
  return 0;
#endif
}

static int
metrics_selinux_checkreqprot_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  int r;

  if (!is_selinux_enabled())
    return 0;

  r = security_get_checkreqprot();
  if (r == 0)
    return metric_build_send_string(mf, link, NULL, "actual (secure)", NULL);
  else if (r == 1)
    return metric_build_send_string(mf, link, NULL, "requested (insecure)", NULL);
  else
    {
      r = -errno;
      log_msg(LOG_ERR, "security_get_checkreqprot() failed: %s", strerror(-r));
      return r;
    }
#else
  return 0;
#endif
}

static int
metrics_selinux_policy_version_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  int vers;
  int r;

  if (!is_selinux_enabled())
    return 0;

  vers = security_policyvers();
  if (vers < 0)
    {
      r = -errno;
      log_msg(LOG_ERR, "security_policyvers() failed: %s", strerror(-r));
      return r;
    }

  return metric_build_send_unsigned(mf, link, NULL, vers, NULL);
#else
  return 0;
#endif
}

static int
metrics_selinux_policy_rpm_version_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  const char *pkg_name = "selinux-policy";
  int r;

  if (!is_selinux_enabled())
    return 0;

#ifdef WITH_LIBRPM
  if (rpmReadConfigFiles(NULL, NULL) != 0)
    {
      log_msg(LOG_ERR, "Failed to read RPM configuration");
      return 0; // XXX error code?
    }

  rpmts ts = rpmtsCreate();
  rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMDBI_NAME, pkg_name, 0);

  if (iter == NULL)
    {
      log_msg(LOG_ERR, "Failed to initialize RPM database iterator");
      rpmtsFree(ts);
      return 0; // XXX error code?
    }

  Header h;
  int found = 0;

  // Iterate over all installed instances of the package
  while ((h = rpmdbNextIterator(iter)) != NULL)
    {
      const char *version, *release, *arch, *epoch;
      found = 1;

      version = headerGetString(h, RPMTAG_VERSION);
      release = headerGetString(h, RPMTAG_RELEASE);
      arch = headerGetString(h, RPMTAG_ARCH);
      epoch = headerGetString(h, RPMTAG_EPOCH);

      r = metric_build_send_rpm(mf, link, pkg_name, strna(version), strempty(release), strempty(arch), epoch);
      if (r < 0)
	break;
    }
  rpmdbFreeIterator(iter);
  rpmtsFree(ts);

  if (r == 0 && !found)
    {
      log_msg(LOG_ERR, "Package '%s' is not installed", pkg_name);
      return -ENOENT;
    }

  return r;

#else
  /*
    XXX two problems:
    1. selinux-policy could be installed several times with different versions
    2. epoch field is ignored
  */
  char *version = NULL, *release = NULL, *arch = NULL;
  _cleanup_free_ char *rpm_info = NULL;

  r = exec_cmd(&rpm_info, "rpm", "rpm", "-q", "--qf", "%{VERSION} %{RELEASE} %{ARCH}", pkg_name, NULL);
  if (r < 0)
    {
      log_msg(LOG_ERR, "exec_cmd(rpm -q --qf...) failed: %s", strerror(-r));
      return 0;
    }

  log_msg(LOG_DEBUG, "exec_cmd(rpm): %s", strna(rpm_info));

  if (isempty(rpm_info))
    return 0;

  version = strtok(rpm_info, " ");
  release = strtok(NULL, " ");
  arch    = strtok(NULL, " ");

  return metric_build_send_rpm(mf, link, pkg_name, strna(version), strempty(release), strempty(arch), NULL);
#endif
#else
  return 0;
#endif
}

static int
metrics_selinux_policy_booleans_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_SELINUX
  char **bools = NULL;
  int nbool = 0;
  int r;

  if (!is_selinux_enabled())
    return 0;

  if (security_get_boolean_names(&bools, &nbool) >= 0)
    {
      for (int i = 0; i < nbool; i++)
	{
	  _cleanup_(sd_json_variant_unrefp) sd_json_variant *fields = NULL;
	  const char *state;

	  r = security_get_boolean_active(bools[i]);
	  if (r < 0)
	    {
	      log_msg(LOG_ERR, "security_get_boolean_active() failed: %s", strerror(errno));
	      return 0;
	    }
	  else if (r == 0)
	    state = "off";
	  else if (r == 1)
	    state = "on";
	  else
	    state = "unknown";

	  int pending = security_get_boolean_pending(bools[i]);
	  if (r != pending)
	    {

	      r = sd_json_buildo(
				 &fields,
				 SD_JSON_BUILD_PAIR_STRING("pending", pending?"activate":"inactivate"));
	      if (r < 0)
		{
		  log_msg(LOG_ERR, "Failed to build metric fields: %s", strerror(-r));
		  return 0;
		}
	    }
	  r = metric_build_send_string(mf, link, bools[i], state, NULL);
	  if (r < 0)
	    return r;
	}
    }
#endif
  return 0;
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
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "SELinux.MaxKernelPolicyVersion",
    .description = "Max kernel policy version",
    .type = METRIC_FAMILY_TYPE_GAUGE,
    .generate = metrics_selinux_policy_version_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "SELinux.PolicyRPMVersion",
    .description = "Version of selinux-policy RPM",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_selinux_policy_rpm_version_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "SELinux.PolicyBooleans",
    .description = "Report the current state of booleans",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_selinux_policy_booleans_generate,
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
selinux_metrics_list(sd_varlink *link, bool booleans)
{
  int r;

  for (const MetricFamily *mf = selinux_metric_family_table; mf->name; mf++)
    {
      if (!booleans && streq(mf->name, METRIC_ORG_OPENSUSE_METRICS_PREFIX "SELinux.PolicyBooleans"))
	continue;
      r = mf->generate(mf, link);
      if (r < 0)
	{
	  log_msg(LOG_DEBUG, "Failed to list metric family '%s': %s", mf->name, strerror(-r));
	  return r;
	}
    }

  return 0;
}
