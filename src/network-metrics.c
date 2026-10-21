// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <systemd/sd-json.h>
#include <systemd/sd-varlink.h>
#ifdef WITH_NETWORK_MANAGER
#include <NetworkManager.h>
#endif

#include "basics.h"
#include "log_msg.h"
#include "exec_cmd.h"
#include "metrics-varlink.h"
#include "network-metrics.h"

static unsigned int
ipv4_netmask_to_prefixlen(const struct sockaddr_in *netmask)
{
  if (netmask == NULL)
    return 32;

  return (unsigned int)__builtin_popcount(ntohl(netmask->sin_addr.s_addr));
}

static unsigned int
ipv6_netmask_to_prefixlen(const struct sockaddr_in6 *netmask)
{
  unsigned int prefixlen = 0;

  if (netmask == NULL)
    return 128;

  for (size_t i = 0; i < sizeof(netmask->sin6_addr.s6_addr); i++)
    prefixlen += (unsigned int)__builtin_popcount(netmask->sin6_addr.s6_addr[i]);

  return prefixlen;
}

static inline void
freeifaddrsp(struct ifaddrs **ifaddr)
{
  if (*ifaddr)
    freeifaddrs(*ifaddr);
  *ifaddr = NULL;
}

static inline void
freeifnameindexp(struct if_nameindex **if_ni)
{
  if (*if_ni)
    if_freenameindex(*if_ni);
  *if_ni = NULL;
}

static inline void
freeNMClientp(NMClient **client)
{
  if (*client)
    g_object_unref(*client);
  *client = NULL;
}

static int
metrics_network_address_generate(const MetricFamily *mf, sd_varlink *link)
{
  _cleanup_(freeifaddrsp) struct ifaddrs *ifaddr = NULL;
  struct ifaddrs *ifa;
  char addr_str[INET6_ADDRSTRLEN];
  unsigned int prefixlen;
  int r;

  // Retrieve the linked list of network interfaces
  if (getifaddrs(&ifaddr) == -1)
    {
      r = -errno;
      log_msg(LOG_ERR, "getifaddrs failed: %s", strerror(-r));
      return r;
    }

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
      if (ifa->ifa_addr == NULL)
	continue;

      int family = ifa->ifa_addr->sa_family;

      // We only care about IPv4 and IPv6 interfaces
      if (family == AF_INET || family == AF_INET6)
	{
	  _cleanup_(sd_json_variant_unrefp) sd_json_variant *fields = NULL;
	  _cleanup_free_ char *cidr_str = NULL;
	  const char *family_str = (family == AF_INET) ? "ipv4" : "ipv6";
	  const char *scope_str = "global";

	  if (family == AF_INET)
	    {
	      struct sockaddr_in *sa = (struct sockaddr_in *)(void *)ifa->ifa_addr;
	      uint32_t addr = ntohl(sa->sin_addr.s_addr);

	      // 127.0.0.0/8 is host (loopback) scope
	      if ((addr & 0xFF000000) == 0x7F000000)
		continue; // scope_str = "host"
	      // 169.254.0.0/16 is link-local scope
	      else if ((addr & 0xFFFF0000) == 0xA9FE0000)
		scope_str = "link";

	      // Convert IP to string
	      inet_ntop(AF_INET, &(sa->sin_addr), addr_str, sizeof(addr_str));
	      prefixlen = ipv4_netmask_to_prefixlen((struct sockaddr_in *)(void *)ifa->ifa_netmask);
	    }
	  else
	    {
	      struct sockaddr_in6 *sa = (struct sockaddr_in6 *)(void *)ifa->ifa_addr;

	      // Use built-in netinet macros to determine IPv6 scope
	      if (IN6_IS_ADDR_LOOPBACK(&sa->sin6_addr))
		continue; // scope_str = "host"
	      else if (IN6_IS_ADDR_LINKLOCAL(&sa->sin6_addr))
		scope_str = "link";
	      else if (IN6_IS_ADDR_SITELOCAL(&sa->sin6_addr))
		scope_str = "site";


	      // Convert IP to string
	      inet_ntop(AF_INET6, &(sa->sin6_addr), addr_str, sizeof(addr_str));
	      prefixlen = ipv6_netmask_to_prefixlen((struct sockaddr_in6 *)(void *)ifa->ifa_netmask);
            }

	  if (asprintf(&cidr_str, "%s/%u", addr_str, prefixlen) < 0)
	    return -ENOMEM;

	  r = sd_json_buildo(&fields,
			     SD_JSON_BUILD_PAIR_STRING("family", family_str),
			     SD_JSON_BUILD_PAIR_STRING("scope", scope_str));
	  if (r < 0)
	    {
	      log_msg(LOG_ERR, "Failed to build metric fields: %s", strerror(-r));
	      return r;
	    }

	  r = metric_build_send_string(mf, link, ifa->ifa_name, cidr_str, fields);
	  if (r < 0)
	    return r;
        }
    }

  return 0;
}

static int
metrics_network_device_state_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_NETWORK_MANAGER
  _cleanup_(freeifnameindexp) struct if_nameindex *if_ni = NULL;
  _cleanup_(freeNMClientp) NMClient *client = NULL;
  GError *error = NULL;
  int r;

  client = nm_client_new(NULL, &error);
  if (!client)
    {
      log_msg(LOG_ERR, "Could not connect to NetworkManager: %s", error->message);
      g_error_free(error);
      return -ECONNREFUSED;
    }

  if_ni = if_nameindex();
  if (!if_ni)
    {
      r = -errno;
      log_msg(LOG_ERR, "if_nameindex failed: %s", strerror(-r));
      return r;
    }

  for (struct if_nameindex *ifni = if_ni; ifni->if_index != 0; ifni++)
    {
      _cleanup_(sd_json_variant_unrefp) sd_json_variant *fields = NULL;
      const char *state_str;

      // Interfaces NetworkManager doesn't know about are reported as unmanaged
      NMDevice *device = nm_client_get_device_by_iface(client, ifni->if_name);
      NMDeviceState state = device ? nm_device_get_state(device) : NM_DEVICE_STATE_UNMANAGED;

      switch (state)
	{
        case NM_DEVICE_STATE_UNKNOWN:
	  state_str = "Unknown";
	  break;
        case NM_DEVICE_STATE_UNMANAGED:
	  state_str = "Unmanaged";
	  break;
        case NM_DEVICE_STATE_UNAVAILABLE:
	  state_str = "Unavailable";
	  break;
        case NM_DEVICE_STATE_DISCONNECTED:
	  state_str = "Disconnected";
	  break;
        case NM_DEVICE_STATE_PREPARE:
	  state_str = "Preparing";
	  break;
        case NM_DEVICE_STATE_CONFIG:
	  state_str = "Configuring";
	  break;
        case NM_DEVICE_STATE_NEED_AUTH:
	  state_str = "Need Authentication";
	  break;
        case NM_DEVICE_STATE_IP_CONFIG:
	  state_str = "Requesting IP Configuration";
	  break;
        case NM_DEVICE_STATE_IP_CHECK:
	  state_str = "Checking IP Connectivity";
	  break;
        case NM_DEVICE_STATE_SECONDARIES:
	  state_str = "Waiting for Secondary Connections";
	  break;
        case NM_DEVICE_STATE_ACTIVATED:
	  state_str = "Activated (Connected)";
	  break;
        case NM_DEVICE_STATE_DEACTIVATING:
	  state_str = "Deactivating";
	  break;
        case NM_DEVICE_STATE_FAILED:
	  state_str = "Failed";
	  break;
        default:
	  state_str = "Unrecognized State";
	  break;
	}

      r = sd_json_buildo(&fields,
			 SD_JSON_BUILD_PAIR_INTEGER("state", state));
      if (r < 0)
	{
	  log_msg(LOG_ERR, "Failed to build metric fields: %s", strerror(-r));
	  return r;
	}

      r = metric_build_send_string(mf, link, ifni->if_name, state_str, fields);
      if (r < 0)
	return r;
    }
#endif

  return 0;
}

static int
metrics_network_managed_interfaces_generate(const MetricFamily *mf, sd_varlink *link)
{
#ifdef WITH_NETWORK_MANAGER
  _cleanup_(freeNMClientp) NMClient *client = NULL;
  const GPtrArray *devices;
  GError *error = NULL;
  unsigned int managed = 0;
  int r;

  client = nm_client_new(NULL, &error);
  if (!client)
    {
      log_msg(LOG_ERR, "Could not connect to NetworkManager: %s", error->message);
      g_error_free(error);
      return -ECONNREFUSED;
    }

  devices = nm_client_get_devices(client);
  if (devices)
    {
      for (guint i = 0; i < devices->len; i++)
	{
	  NMDevice *device = g_ptr_array_index(devices, i);

	  // Check if the device is currently managed by NM
	  if (nm_device_get_managed(device))
	    managed++;
        }
    }

  r = metric_build_send_unsigned(mf, link, NULL, managed, NULL);
  if (r < 0)
    return r;
#endif

  return 0;
}

static const MetricFamily network_metric_family_table[] = {
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "Network.Address",
    .description = "Per interface metric: configured IP address in CIDR notation",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_network_address_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "Network.DeviceState",
    .description = "Per interface metric: state of network device",
    .type = METRIC_FAMILY_TYPE_STRING,
    .generate = metrics_network_device_state_generate,
  },
  {
    .name = METRIC_ORG_OPENSUSE_METRICS_PREFIX "Network.ManagedInterfaces",
    .description = "Number of network interfaces managed by systemd-networkd",
    .type = METRIC_FAMILY_TYPE_GAUGE,
    .generate = metrics_network_managed_interfaces_generate,
  },
  {}
};

int
network_metrics_describe(sd_varlink *link)
{
  int r;

  for (const MetricFamily *mf = network_metric_family_table; mf->name; mf++)
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
network_metrics_list(sd_varlink *link)
{
  int r;

  for (const MetricFamily *mf = network_metric_family_table; mf->name; mf++)
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
