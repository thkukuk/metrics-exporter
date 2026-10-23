// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-json.h>
#include <systemd/sd-varlink.h>
#ifdef WITH_LIBRPM
#include <rpm/rpmlib.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>
#endif

#include "basics.h"
#include "log_msg.h"
#include "metrics-varlink.h"
#include "packages-metrics.h"

#ifdef WITH_LIBRPM
static int
rpm_logger(rpmlogRec rec, rpmlogCallbackData _unused_(data))
{
  int priority = rpmlogRecPriority(rec);
  const char *msg = rpmlogRecMessage(rec);

  if (priority <= RPMLOG_ERR)
    log_msg(LOG_ERR, "%s", msg);
  else if (priority == RPMLOG_WARNING)
    log_msg(LOG_WARNING, "%s", msg);
  else if (priority == RPMLOG_INFO)
    log_msg(LOG_INFO, "%s", msg);
  else
    log_msg(LOG_DEBUG, "%s", msg);

  return 0;
}
#endif

static int
metrics_packages_list_generate(const MetricFamily _unused_(*mf), sd_varlink _unused_(*link),
                               void _unused_(*userdata))
{
#ifdef WITH_LIBRPM
  Header h;
  rpmts ts;
  rpmdbMatchIterator iter;
  int r;

  rpmlogSetCallback(rpm_logger, NULL);

  if (rpmReadConfigFiles(NULL, NULL) != 0)
    {
      log_msg(LOG_ERR, "Failed to read RPM configuration");
      return -1; // XXX better error code
    }

  ts = rpmtsCreate();
  iter = rpmtsInitIterator(ts, RPMDBI_PACKAGES, NULL, 0);
  if (iter == NULL)
    {
      log_msg(LOG_ERR, "Failed to initialize RPM database iterator");
      rpmtsFree(ts);
      return -1; // XXX better error code
    }

  r = 0;
  while ((h = rpmdbNextIterator(iter)) != NULL)
    {
      struct rpmtd_s td;
      const char *name, *version, *release, *arch;
      uint32_t epoch = 0;

      name = headerGetString(h, RPMTAG_NAME);
      version = headerGetString(h, RPMTAG_VERSION);
      release = headerGetString(h, RPMTAG_RELEASE);
      arch = headerGetString(h, RPMTAG_ARCH);

      // Extract epoch (which is a number and may not exist on all packages)
      if (headerGet(h, RPMTAG_EPOCH, &td, HEADERGET_MINMEM))
	{
	  epoch = rpmtdGetNumber(&td);
	  rpmtdFreeData(&td);
        }

      r = metric_build_send_package(mf, link, name, version, release, arch, epoch);
      if (r < 0)
	break;
    }

  rpmdbFreeIterator(iter);
  rpmtsFree(ts);

  return r;
#else
  return 0;
#endif
}

static const MetricFamily packages_metric_family_table[] = {
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "Packages",
    .description = "Installed Package",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_packages_list_generate,
  },
  {}
};

int
packages_metrics_describe(sd_varlink *link)
{
  int r;

  for (const MetricFamily *mf = packages_metric_family_table; mf->name; mf++)
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
packages_metrics_list(sd_varlink *link)
{
  int r;

  for (const MetricFamily *mf = packages_metric_family_table; mf->name; mf++)
    {
      r = mf->generate(mf, link, NULL);
      if (r < 0)
	{
	  log_msg(LOG_DEBUG, "Failed to list metric family '%s': %s", mf->name, strerror(-r));
	  return r;
	}
    }

  return 0;
}
