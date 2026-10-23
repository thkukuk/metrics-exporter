// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-json.h>
#include <systemd/sd-varlink.h>

#include "basics.h"
#include "log_msg.h"
#include "metrics-varlink.h"
#include "upower-metrics.h"

#ifdef WITH_UPOWER_GLIB
#include <upower.h>

struct battery_info {
  gchar *native_path;
  gchar *vendor;
  gchar *model;
  guint state;
  gdouble percentage;
  gdouble energy;
  gdouble energy_full;
  gdouble energy_design;
};

/* batteries is a calloc'd array terminated by a zeroed entry (native_path == NULL) */
static void
battery_info_freep(struct battery_info **array)
{
  struct battery_info *batteries = *array;

  if (!batteries)
    return;

  for (size_t i = 0; batteries[i].native_path; i++)
    {
      g_free(batteries[i].native_path);
      g_free(batteries[i].vendor);
      g_free(batteries[i].model);
    }

  free(batteries);
  *array = NULL;
}

static const char*
battery_state_to_str(guint state)
{
  switch (state)
    {
    case UP_DEVICE_STATE_CHARGING:          return "Charging";
    case UP_DEVICE_STATE_DISCHARGING:       return "Discharging";
    case UP_DEVICE_STATE_FULLY_CHARGED:     return "Fully Charged";
    case UP_DEVICE_STATE_EMPTY:             return "Empty";
    case UP_DEVICE_STATE_PENDING_CHARGE:    return "Pending Charge";
    case UP_DEVICE_STATE_PENDING_DISCHARGE: return "Pending Discharge";
    default:                                return "Unknown";
    }
}

#endif

static int
metrics_battery_count_generate(const MetricFamily _unused_(*mf),
			       sd_varlink _unused_(*link),
			       void _unused_(*userdata))
{
#ifdef WITH_UPOWER_GLIB
  struct battery_info *bi = userdata;
  size_t count;

  for (count = 0; bi[count].native_path; count++)
    ;

  return metric_build_send_unsigned(mf, link, NULL, count, NULL);
#else
  return 0;
#endif

}

static int
metrics_battery_vendor_generate(const MetricFamily _unused_(*mf),
				sd_varlink _unused_(*link),
				void _unused_(*userdata))
{
#ifdef WITH_UPOWER_GLIB
  struct battery_info *bi = userdata;
  int r;

  for (uint32_t i = 0; bi[i].native_path; i++)
    {
      if (isempty(bi[i].vendor))
	  continue;
      r = metric_build_send_string(mf, link, bi[i].native_path, bi[i].vendor, NULL);
      if (r < 0)
	return r;
    }
#endif
  return 0;
}

static int
metrics_battery_model_generate(const MetricFamily _unused_(*mf),
			       sd_varlink _unused_(*link),
			       void _unused_(*userdata))
{
#ifdef WITH_UPOWER_GLIB
  struct battery_info *bi = userdata;
  int r;

  for (uint32_t i = 0; bi[i].native_path; i++)
    {
      if (isempty(bi[i].model))
	  continue;
      r = metric_build_send_string(mf, link, bi[i].native_path, bi[i].model, NULL);
      if (r < 0)
	return r;
    }
#endif
  return 0;
 }

static int
metrics_battery_state_generate(const MetricFamily _unused_(*mf),
			       sd_varlink _unused_(*link),
			       void _unused_(*userdata))
{
#ifdef WITH_UPOWER_GLIB
  struct battery_info *bi = userdata;
  int r;

  for (uint32_t i = 0; bi[i].native_path; i++)
    {
      r = metric_build_send_string(mf, link, bi[i].native_path,
				   battery_state_to_str(bi[i].state), NULL);
      if (r < 0)
	return r;
    }
#endif
  return 0;
}

static int
metrics_battery_charge_generate(const MetricFamily _unused_(*mf),
				sd_varlink _unused_(*link),
				void _unused_(*userdata))
{
#ifdef WITH_UPOWER_GLIB
  struct battery_info *bi = userdata;
  int r;

  for (uint32_t i = 0; bi[i].native_path; i++)
    {
      _cleanup_free_ char *value = NULL;

      if (asprintf(&value, "%.1f%%", bi[i].percentage) < 0)
        return -ENOMEM;

      r = metric_build_send_string(mf, link, bi[i].native_path, value, NULL);
      if (r < 0)
	return r;
    }
#endif
  return 0;
}

static int
metrics_battery_energy_generate(const MetricFamily _unused_(*mf),
				sd_varlink _unused_(*link),
				void _unused_(*userdata))
{
#ifdef WITH_UPOWER_GLIB
  struct battery_info *bi = userdata;
  int r;

  for (uint32_t i = 0; bi[i].native_path; i++)
    {
      _cleanup_free_ char *value = NULL;

      if (asprintf(&value, "%.2f", bi[i].energy) < 0)
        return -ENOMEM;

      r = metric_build_send_string(mf, link, bi[i].native_path, value, NULL);
      if (r < 0)
	return r;
    }
#endif
  return 0;
}

static int
metrics_battery_energy_full_generate(const MetricFamily _unused_(*mf),
				     sd_varlink _unused_(*link),
				     void _unused_(*userdata))
{
#ifdef WITH_UPOWER_GLIB
  struct battery_info *bi = userdata;
  int r;

  for (uint32_t i = 0; bi[i].native_path; i++)
    {
      _cleanup_free_ char *value = NULL;

      if (asprintf(&value, "%.2f", bi[i].energy_full) < 0)
        return -ENOMEM;

      r = metric_build_send_string(mf, link, bi[i].native_path, value, NULL);
      if (r < 0)
	return r;
    }
#endif
  return 0;
}

static int
metrics_battery_energy_design_generate(const MetricFamily _unused_(*mf),
				       sd_varlink _unused_(*link),
				       void _unused_(*userdata))
{
#ifdef WITH_UPOWER_GLIB
  struct battery_info *bi = userdata;
  int r;

  for (uint32_t i = 0; bi[i].native_path; i++)
    {
      _cleanup_free_ char *value = NULL;

      if (asprintf(&value, "%.2f", bi[i].energy_design) < 0)
        return -ENOMEM;

      r = metric_build_send_string(mf, link, bi[i].native_path, value, NULL);
      if (r < 0)
	return r;
    }
#endif
  return 0;
}

static const MetricFamily upower_metric_family_table[] = {
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "UPower.Battery.Count",
    .description = "Number of batteries",
    .type = METRIC_FAMILY_TYPE_GAUGE,
    .generate = metrics_battery_count_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "UPower.Battery.Vendor",
    .description = "Battery Vendor",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_battery_vendor_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "UPower.Battery.Model",
    .description = "Battery Model",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_battery_model_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "UPower.Battery.State",
    .description = "Battery State",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_battery_state_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "UPower.Battery.Charge",
    .description = "Battery Charge Percentage",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_battery_charge_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "UPower.Battery.Energy",
    .description = "Battery Energy in Wh",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_battery_energy_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "UPower.Battery.EnergyFull",
    .description = "Full Battery Energy in Wh",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_battery_energy_full_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "UPower.Battery.EnergyDesign",
    .description = "Full Battery Energy (Design) in Wh",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_battery_energy_design_generate,
  },
  {}
};

int
upower_metrics_describe(sd_varlink *link)
{
  int r;

  for (const MetricFamily *mf = upower_metric_family_table; mf->name; mf++)
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
upower_metrics_list(sd_varlink _unused_(*link))
{
#ifdef WITH_UPOWER_GLIB
  _cleanup_(battery_info_freep) struct battery_info *batteries = NULL;
  uint32_t battery_count = 0;
  GError *error = NULL;
  int r;

  UpClient *client = up_client_new_full(NULL, &error);
  if (!client)
    {
      log_msg(LOG_ERR, "Failed to connect to the UPower daemon: %s", error->message);
      g_error_free(error);
      return 0; // No daemon, nothing to send.
    }

  // Get an array of all UPower devices
  GPtrArray *devices = up_client_get_devices2(client);
  if (!devices)
    {
      log_msg(LOG_ERR, "Failed to get devices.");
      g_object_unref(client);
      return 0; // No devices, nothing to send.
    }

  for (guint i = 0; i < devices->len; i++)
    {
      guint kind;
      UpDevice *device = g_ptr_array_index(devices, i);

      // Retrieve the kind of device first
      g_object_get(device, "kind", &kind, NULL);

      // Filter for batteries only
      if (kind == UP_DEVICE_KIND_BATTERY)
	battery_count++;
    }

  batteries = calloc(battery_count+1, sizeof(struct battery_info));
  if (!batteries)
    {
      g_ptr_array_unref(devices);
      g_object_unref(client);
      return -ENOMEM;
    }

  for (guint i = 0, bi = 0; i < devices->len; i++)
    {
      guint kind;
      UpDevice *device = g_ptr_array_index(devices, i);

      // Retrieve the kind of device first
      g_object_get(device, "kind", &kind, NULL);

      // Filter for batteries only
      if (kind != UP_DEVICE_KIND_BATTERY)
	continue;

	      // Get relevant properties
      g_object_get(device,
		   "native-path", &batteries[bi].native_path,
		   "vendor", &batteries[bi].vendor,
		   "model", &batteries[bi].model,
		   "state", &batteries[bi].state,
		   "percentage", &batteries[bi].percentage,
		   "energy", &batteries[bi].energy,
		   "energy-full", &batteries[bi].energy_full,
		   "energy-full-design", &batteries[bi].energy_design,
		   NULL);
      bi++;
    }

  g_ptr_array_unref(devices);
  g_object_unref(client);

  for (const MetricFamily *mf = upower_metric_family_table; mf->name; mf++)
    {
      r = mf->generate(mf, link, batteries);
      if (r < 0)
	{
	  log_msg(LOG_DEBUG, "Failed to list metric family '%s': %s", mf->name, strerror(-r));
	  return r;
	}
    }
#endif
  return 0;
}
