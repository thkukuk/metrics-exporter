// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define METRIC_ORG_OPENSUSE_METRICS_PREFIX "org.openSUSE.Metrics."

typedef enum MetricFamilyType {
  METRIC_FAMILY_TYPE_COUNTER,
  METRIC_FAMILY_TYPE_GAUGE,
  METRIC_FAMILY_TYPE_STRING,
  METRIC_FAMILY_TYPE_OBJECT,
} MetricFamilyType;

typedef struct MetricFamily MetricFamily;

typedef int (*metric_family_generate_func_t)(const MetricFamily *mf, sd_varlink *link);

typedef struct MetricFamily {
  const char *name;
  const char *description;
  MetricFamilyType type;
  metric_family_generate_func_t generate;
} MetricFamily;

extern const char *metric_family_type_to_str(MetricFamilyType type);

extern int metric_build_send(const MetricFamily *mf, sd_varlink *link,
			     sd_json_variant *value);
extern int metric_build_send_string(const MetricFamily *mf, sd_varlink *link,
				    const char *value);
extern int metric_build_send_unsigned(const MetricFamily *mf,
				      sd_varlink *link, uint64_t value);

extern int metric_build_send_error(const MetricFamily *mf,
				   sd_varlink *link, int r);

extern int vl_method_metrics_describe(sd_varlink *link,
	       			      sd_json_variant *parameters,
		                      sd_varlink_method_flags_t flags,
				      void *userdata);
extern int vl_method_metrics_list(sd_varlink *link,
		                  sd_json_variant *parameters,
                                  sd_varlink_method_flags_t flags,
				  void *userdata);
