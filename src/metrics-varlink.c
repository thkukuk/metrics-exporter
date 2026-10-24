// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-json.h>
#include <systemd/sd-varlink.h>

#include "basics.h"
#include "log_msg.h"
#include "read_config.h"
#include "metrics-varlink.h"
#include "network-metrics.h"
#include "packages-metrics.h"
#include "selinux-metrics.h"
#include "upower-metrics.h"
#include "varlink-io.systemd.Metrics.h"


const char *
metric_family_type_to_str(MetricFamilyType type)
{
  switch (type)
    {
    case METRIC_FAMILY_TYPE_COUNTER:
      return "counter";
    case METRIC_FAMILY_TYPE_GAUGE:
      return "gauge";
    case METRIC_FAMILY_TYPE_STRING:
      return "string";
    case METRIC_FAMILY_TYPE_OBJECT:
      return "object";
    default:
      return NULL;
    }
}

int
metric_build_send(const MetricFamily *mf, sd_varlink *link,
		  const char *object, sd_json_variant *value,
		  sd_json_variant *fields)
{
  return sd_varlink_replybo(link,
			    SD_JSON_BUILD_PAIR_STRING("name", mf->name),
			    SD_JSON_BUILD_PAIR_CONDITION(!isempty(object), "object",
							 SD_JSON_BUILD_STRING(object)),
			    SD_JSON_BUILD_PAIR_VARIANT("value", value),
			    SD_JSON_BUILD_PAIR_CONDITION(fields != NULL, "fields",
							 SD_JSON_BUILD_VARIANT(fields)));
}

int
metric_build_send_string(const MetricFamily *mf, sd_varlink *link,
			 const char *object, const char *value,
			 sd_json_variant *fields)
{
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;
  int r;

  r = sd_json_variant_new_string(&v, value);
  if (r < 0)
    return r;

  return metric_build_send(mf, link, object, v, fields);
}

int
metric_build_send_unsigned(const MetricFamily *mf, sd_varlink *link,
			   const char *object, uint64_t value,
			   sd_json_variant *fields)
{
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;
  int r;

  r = sd_json_variant_new_unsigned(&v, value);
  if (r < 0)
    return r;

  return metric_build_send(mf, link, object, v, fields);
}

int
metric_build_send_double(const MetricFamily *mf, sd_varlink *link,
			 const char *object, double value,
			 sd_json_variant *fields)
{
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;
  int r;

  r = sd_json_variant_new_real(&v, value);
  if (r < 0)
    return r;

  return metric_build_send(mf, link, object, v, fields);
}

int
metric_build_send_package(const MetricFamily *mf, sd_varlink *link,
			  const char *name, const char *version,
			  const char *release, const char *arch,
			  uint32_t epoch)
{
  _cleanup_free_ char *value_str = NULL;
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *value = NULL;
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *fields = NULL;
  int r;

  if (asprintf(&value_str, "%u:%s-%s.%s", epoch, version, release, arch) < 0)
    return -ENOMEM;

  r = sd_json_variant_new_string(&value, value_str);
  if (r < 0)
    return r;

  r = sd_json_buildo(&fields,
		     SD_JSON_BUILD_PAIR_STRING("version", version),
		     SD_JSON_BUILD_PAIR_STRING("release", release),
		     SD_JSON_BUILD_PAIR_STRING("arch", arch),
		     SD_JSON_BUILD_PAIR_CONDITION(epoch!=0, "epoch",
					  SD_JSON_BUILD_UNSIGNED(epoch)));
  if (r < 0)
    return r;

  return metric_build_send(mf, link, name, value, fields);
}

static int
metrics_version_generate(const MetricFamily *mf, sd_varlink *link,
                         void _unused_(*userdata))
{
  return metric_build_send_string(mf, link, NULL, VERSION, NULL);
}

static const MetricFamily main_metric_family_table[] = {
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "Version",
    .description = "Version of metrics-exporter",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_version_generate,
  },
  {}
};

int
vl_method_metrics_describe(sd_varlink *link, sd_json_variant *parameters,
                           sd_varlink_method_flags_t _unused_(flags),
			   void _unused_(*userdata))
{
  int r;

  r = sd_varlink_dispatch(link, parameters, /* dispatch_table= */ NULL, /* userdata= */ NULL);
  if (r != 0)
    return r;

  r = sd_varlink_set_sentinel(link, "io.systemd.Metrics.NoSuchMetric");
  if (r < 0)
    return r;

  for (const MetricFamily *mf = main_metric_family_table; mf->name; mf++)
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

  r = network_metrics_describe(link);
  if (r < 0)
    return r;

  r = packages_metrics_describe(link);
  if (r < 0)
    return r;

  r = selinux_metrics_describe(link);
  if (r < 0)
    return r;

  r = upower_metrics_describe(link);
  if (r < 0)
    return r;

  return 0;
}

int
vl_method_metrics_list(sd_varlink *link, sd_json_variant *parameters,
                       sd_varlink_method_flags_t _unused_(flags),
		       void *userdata)
{
  config_t *cfg = userdata;
  int r;

  r = sd_varlink_dispatch(link, parameters, /* dispatch_table= */ NULL, /* userdata= */ NULL);
  if (r != 0)
    return r;

  r = sd_varlink_set_sentinel(link, "io.systemd.Metrics.NoSuchMetric");
  if (r < 0)
    return r;

  for (const MetricFamily *mf = main_metric_family_table; mf->name; mf++)
    {
      r = mf->generate(mf, link, NULL);
      if (r < 0)
	{
	  log_msg(LOG_DEBUG, "Failed to list metric family '%s': %s", mf->name, strerror(-r));
	  return r;
	}
    }

  if (cfg->network)
    {
      r = network_metrics_list(link);
      if (r < 0)
	return r;
    }

  if (cfg->packages)
    {
      r = packages_metrics_list(link);
      if (r < 0)
	return r;
    }

  if (cfg->selinux)
    {
      r = selinux_metrics_list(link, cfg->selinux_booleans);
      if (r < 0)
	return r;
    }

  if (cfg->upower)
    {
      r = upower_metrics_list(link);
      if (r < 0)
	return r;
    }

  return 0;
}
