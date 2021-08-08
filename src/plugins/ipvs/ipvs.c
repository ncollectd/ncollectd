/**
 * collectd - src/ipvs.c (based on ipvsadm and libipvs)
 * Copyright (C) 1997  Steven Clarke <steven@monmouth.demon.co.uk>
 * Copyright (C) 1998-2004  Wensong Zhang <wensong@linuxvirtualserver.org>
 * Copyright (C) 2003-2004  Peter Kese <peter.kese@ijs.si>
 * Copyright (C) 2007  Sebastian Harl
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors:
 *   Sebastian Harl <sh at tokkee.org>
 **/

/*
 * This plugin collects statistics about IPVS connections. It requires Linux
 * kernels >= 2.6.
 *
 * See http://www.linuxvirtualserver.org/software/index.html for more
 * information about IPVS.
 */

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif /* HAVE_ARPA_INET_H */
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#include <linux/ip_vs.h>

static int sockfd = -1;

enum {
  FAM_HOST_IPVS_SERVICE_CONNECTIONS_TOTAL,
  FAM_HOST_IPVS_SERVICE_IN_BYTES_TOTAL,
  FAM_HOST_IPVS_SERVICE_OUT_BYTES_TOTAL,
  FAM_HOST_IPVS_SERVICE_IN_PACKETS_TOTAL,
  FAM_HOST_IPVS_SERVICE_OUT_PACKETS_TOTAL,
  FAM_HOST_IPVS_DESTINATION_ACTIVE_CONNECTIONS,
  FAM_HOST_IPVS_DESTINATION_INACTIVE_CONNECTIONS,
  FAM_HOST_IPVS_DESTINATION_PERSISTENT_CONNECTIONS,
  FAM_HOST_IPVS_DESTINATION_CONNECTIONS_TOTAL,
  FAM_HOST_IPVS_DESTINATION_IN_BYTES_TOTAL,
  FAM_HOST_IPVS_DESTINATION_OUT_BYTES_TOTAL,
  FAM_HOST_IPVS_DESTINATION_IN_PACKETS_TOTAL,
  FAM_HOST_IPVS_DESTINATION_OUT_PACKETS_TOTAL,
  FAM_HOST_IPVS_MAX,
};

static metric_family_t fams[FAM_HOST_IPVS_MAX] = {
  [FAM_HOST_IPVS_SERVICE_CONNECTIONS_TOTAL] = {
    .name = "host_ipvs_service_connections_total",
    .type = METRIC_TYPE_COUNTER,
	.help = "Total number of connections scheduled in the ipvs service",
  },
  [FAM_HOST_IPVS_SERVICE_IN_BYTES_TOTAL] = {
    .name = "host_ipvs_service_in_bytes_total",
    .type = METRIC_TYPE_COUNTER,
	.help = "Total number of ingress bytes in the ipvs service",
  },
  [FAM_HOST_IPVS_SERVICE_OUT_BYTES_TOTAL] = {
    .name = "host_ipvs_service_out_bytes_total",
    .type = METRIC_TYPE_COUNTER,
	.help = "Total number of egress bytes in the ipvs service",
  },
  [FAM_HOST_IPVS_SERVICE_IN_PACKETS_TOTAL] = {
    .name = "host_ipvs_service_in_packets_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total number of ingress packets in the ipvs service",
  },
  [FAM_HOST_IPVS_SERVICE_OUT_PACKETS_TOTAL] = {
    .name = "host_ipvs_service_out_packets_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total number of egress packets in the ipvs service",
  },
  [FAM_HOST_IPVS_DESTINATION_ACTIVE_CONNECTIONS] = {
    .name = "host_ipvs_destination_active_connections",
    .type = METRIC_TYPE_GAUGE,
	.help = "Number of active connections in the ipvs destination",
  },
  [FAM_HOST_IPVS_DESTINATION_INACTIVE_CONNECTIONS] = {
    .name = "host_ipvs_destination_inactive_connections",
    .type = METRIC_TYPE_GAUGE,
	.help = "Number of inactive connections in the ipvs destination",
  },
  [FAM_HOST_IPVS_DESTINATION_PERSISTENT_CONNECTIONS] = {
    .name = "host_ipvs_destination_persistent_connections",
    .type = METRIC_TYPE_GAUGE,
	.help = "Number of persistent connections in the ipvs destination",
  },
  [FAM_HOST_IPVS_DESTINATION_CONNECTIONS_TOTAL] = {
    .name = "host_ipvs_destination_connections_total",
    .type = METRIC_TYPE_COUNTER,
	.help = "Total number of connections scheduled in the ipvs destination",
  },
  [FAM_HOST_IPVS_DESTINATION_IN_BYTES_TOTAL] = {
    .name = "host_ipvs_destination_in_bytes_total",
    .type = METRIC_TYPE_COUNTER,
	.help = "Total number of ingress bytes in the ipvs destination",
  },
  [FAM_HOST_IPVS_DESTINATION_OUT_BYTES_TOTAL] = {
    .name = "host_ipvs_destination_out_bytes_total",
    .type = METRIC_TYPE_COUNTER,
	.help = "Total number of egress bytes in the ipvs destination",
  },
  [FAM_HOST_IPVS_DESTINATION_IN_PACKETS_TOTAL] = {
    .name = "host_ipvs_destination_in_packets_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total number of ingress packets in the ipvs destination",
  },
  [FAM_HOST_IPVS_DESTINATION_OUT_PACKETS_TOTAL] = {
    .name = "host_ipvs_destination_out_packets_total",
    .type = METRIC_TYPE_COUNTER,
    .help = "Total number of egress packets in the ipvs destination",
  },
};

static struct ip_vs_get_services *ipvs_get_services(void)
{
  struct ip_vs_getinfo ipvs_info;
  struct ip_vs_get_services *services;

  socklen_t len = sizeof(ipvs_info);

  if (getsockopt(sockfd, IPPROTO_IP, IP_VS_SO_GET_INFO, &ipvs_info, &len) == -1) {
    ERROR("ipvs plugin: ip_vs_get_services: getsockopt() failed: %s", STRERRNO);
    return NULL;
  }

  len = sizeof(*services) +
        sizeof(struct ip_vs_service_entry) * ipvs_info.num_services;

  services = malloc(len);
  if (services == NULL) {
    ERROR("ipvs plugin: ipvs_get_services: Out of memory.");
    return NULL;
  }

  services->num_services = ipvs_info.num_services;

  if (getsockopt(sockfd, IPPROTO_IP, IP_VS_SO_GET_SERVICES, services, &len) == -1) {
    ERROR("ipvs plugin: ipvs_get_services: getsockopt failed: %s", STRERRNO);
    free(services);
    return NULL;
  }

  return services;
}

static struct ip_vs_get_dests *ipvs_get_dests(struct ip_vs_service_entry *se)
{
  struct ip_vs_get_dests *dests;

  socklen_t len = sizeof(*dests) + sizeof(struct ip_vs_dest_entry) * se->num_dests;

  dests = malloc(len);
  if (dests == NULL) {
    ERROR("ipvs plugin: ipvs_get_dests: Out of memory.");
    return NULL;
  }

  dests->fwmark = se->fwmark;
  dests->protocol = se->protocol;
  dests->addr = se->addr;
  dests->port = se->port;
  dests->num_dests = se->num_dests;

  if (getsockopt(sockfd, IPPROTO_IP, IP_VS_SO_GET_DESTS, dests, &len) == -1) {
    ERROR("ipvs plugin: ipvs_get_dests: getsockopt() failed: %s", STRERRNO);
    free(dests);
    return NULL;
  }

  return dests;
}

static int cipvs_read(void)
{
  if (sockfd < 0)
    return -1;

  struct ip_vs_get_services *services = ipvs_get_services();
  if (services == NULL)
    return -1;

  for (size_t i = 0; i < services->num_services; ++i) {
    struct ip_vs_service_entry *se = &services->entrytable[i];
    if (se == NULL)
      continue;

    metric_t m = {0};

    if (se->fwmark) {
      char buffer[24];
      ssnprintf(buffer, sizeof(buffer), "%d", se->fwmark);
      metric_label_set(&m, "fwmark", buffer);
    } else {
      struct in_addr saddr = {.s_addr = se->addr};
      metric_label_set(&m, "vip", inet_ntoa(saddr));
      char buffer[24];
      ssnprintf(buffer, sizeof(buffer), "%u", ntohs(se->port));
      metric_label_set(&m, "vport", buffer);
      metric_label_set(&m, "protocol",  (se->protocol == IPPROTO_TCP) ? "TCP" : "UDP");
    }

    struct ip_vs_stats_user sstats = se->stats;

    m.value.counter.uint64 = sstats.conns;
    metric_family_metric_append(&fams[FAM_HOST_IPVS_SERVICE_CONNECTIONS_TOTAL], m);
    m.value.counter.uint64 = sstats.inbytes;
    metric_family_metric_append(&fams[FAM_HOST_IPVS_SERVICE_IN_BYTES_TOTAL], m);
    m.value.counter.uint64 = sstats.outbytes;
    metric_family_metric_append(&fams[FAM_HOST_IPVS_SERVICE_OUT_BYTES_TOTAL], m);
    m.value.counter.uint64 = sstats.inpkts;
    metric_family_metric_append(&fams[FAM_HOST_IPVS_SERVICE_IN_PACKETS_TOTAL], m);
    m.value.counter.uint64 = sstats.outpkts;
    metric_family_metric_append(&fams[FAM_HOST_IPVS_SERVICE_OUT_PACKETS_TOTAL], m);

    struct ip_vs_get_dests *dests = ipvs_get_dests(se);
    if (dests == NULL)
      continue;

    for (size_t n = 0; n < dests->num_dests; ++n) {
      struct ip_vs_dest_entry *de = &dests->entrytable[n];
      if (de == NULL)
        continue;

      struct in_addr daddr = {.s_addr = de->addr};
      metric_label_set(&m, "rip", inet_ntoa(daddr));
      char buffer[24];
      ssnprintf(buffer, sizeof(buffer), "%u", ntohs(de->port));
      metric_label_set(&m, "rport", buffer);

      m.value.gauge.float64 = de->activeconns;
      metric_family_metric_append(&fams[FAM_HOST_IPVS_DESTINATION_ACTIVE_CONNECTIONS], m);
      m.value.gauge.float64 = de->inactconns;
      metric_family_metric_append(&fams[FAM_HOST_IPVS_DESTINATION_INACTIVE_CONNECTIONS], m);
      m.value.gauge.float64 = de->persistconns;
      metric_family_metric_append(&fams[FAM_HOST_IPVS_DESTINATION_PERSISTENT_CONNECTIONS], m);

      struct ip_vs_stats_user dstats = de->stats;

      m.value.counter.uint64 = dstats.conns;
      metric_family_metric_append(&fams[FAM_HOST_IPVS_DESTINATION_CONNECTIONS_TOTAL], m);
      m.value.counter.uint64 = dstats.inbytes;
      metric_family_metric_append(&fams[FAM_HOST_IPVS_DESTINATION_IN_BYTES_TOTAL], m);
      m.value.counter.uint64 = dstats.outbytes;
      metric_family_metric_append(&fams[FAM_HOST_IPVS_DESTINATION_OUT_BYTES_TOTAL], m);
      m.value.counter.uint64 = dstats.inpkts;
      metric_family_metric_append(&fams[FAM_HOST_IPVS_DESTINATION_IN_PACKETS_TOTAL], m);
      m.value.counter.uint64 = dstats.outpkts;
      metric_family_metric_append(&fams[FAM_HOST_IPVS_DESTINATION_OUT_PACKETS_TOTAL], m);
    }

    sfree(dests);
    metric_reset(&m);
  }
  sfree(services);

  for (size_t i = 0; i < FAM_HOST_IPVS_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("ipvs plugin: plugin_dispatch_metric_family failed: %s",
              STRERROR(status));
      }
      metric_family_metric_reset(&fams[i]);
    }
  }

  return 0;
}

static int cipvs_shutdown(void)
{
  if (sockfd >= 0)
    close(sockfd);
  sockfd = -1;

  return 0;
}

static int cipvs_init(void)
{
  struct ip_vs_getinfo ipvs_info;

  if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) == -1) {
    ERROR("ipvs plugin: cipvs_init: socket() failed: %s", STRERRNO);
    return -1;
  }

  socklen_t len = sizeof(ipvs_info);

  if (getsockopt(sockfd, IPPROTO_IP, IP_VS_SO_GET_INFO, &ipvs_info, &len) == -1) {
    ERROR("ipvs plugin: cipvs_init: getsockopt() failed: %s", STRERRNO);
    close(sockfd);
    sockfd = -1;
    return -1;
  }

  /* we need IPVS >= 1.1.4 */
  if (ipvs_info.version < ((1 << 16) + (1 << 8) + 4)) {
    ERROR("ipvs plugin: cipvs_init: IPVS version too old (%d.%d.%d < %d.%d.%d)",
           NVERSION(ipvs_info.version), 1, 1, 4);
    close(sockfd);
    sockfd = -1;
    return -1;
  } else {
    INFO("ipvs plugin: Successfully connected to IPVS %d.%d.%d",
          NVERSION(ipvs_info.version));
  }

  return 0;
}

void module_register(void)
{
  plugin_register_init("ipvs", cipvs_init);
  plugin_register_read("ipvs", cipvs_read);
  plugin_register_shutdown("ipvs", cipvs_shutdown);
}
