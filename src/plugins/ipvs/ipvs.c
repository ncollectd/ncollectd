// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 1997 Steven Clarke
// SPDX-FileCopyrightText: Copyright (C) 1998-2004 Wensong Zhang
// SPDX-FileCopyrightText: Copyright (C) 2003-2004 Peter Kese
// SPDX-FileCopyrightText: Copyright (C) 2007 Sebastian Harl
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Steven Clarke <steven at monmouth.demon.co.uk>
// SPDX-FileContributor: Wensong Zhang <wensong at linuxvirtualserver.org>
// SPDX-FileContributor: Peter Kese <peter.kese at ijs.si>
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>


/*
 * This plugin collects statistics about IPVS connections. It requires Linux
 * kernels >= 2.6.
 *
 * See http://www.linuxvirtualserver.org/software/index.html for more
 * information about IPVS.
 */

#include "plugin.h"
#include "libutils/common.h"

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <linux/ip_vs.h>

static int sockfd = -1;

enum {
    FAM_IPVS_SERVICE_CONNECTIONS,
    FAM_IPVS_SERVICE_IN_BYTES,
    FAM_IPVS_SERVICE_OUT_BYTES,
    FAM_IPVS_SERVICE_IN_PACKETS,
    FAM_IPVS_SERVICE_OUT_PACKETS,
    FAM_IPVS_DESTINATION_ACTIVE_CONNECTIONS,
    FAM_IPVS_DESTINATION_INACTIVE_CONNECTIONS,
    FAM_IPVS_DESTINATION_PERSISTENT_CONNECTIONS,
    FAM_IPVS_DESTINATION_CONNECTIONS,
    FAM_IPVS_DESTINATION_IN_BYTES,
    FAM_IPVS_DESTINATION_OUT_BYTES,
    FAM_IPVS_DESTINATION_IN_PACKETS,
    FAM_IPVS_DESTINATION_OUT_PACKETS,
    FAM_IPVS_MAX,
};

static metric_family_t fams[FAM_IPVS_MAX] = {
    [FAM_IPVS_SERVICE_CONNECTIONS] = {
        .name = "system_ipvs_service_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of connections scheduled in the ipvs service",
    },
    [FAM_IPVS_SERVICE_IN_BYTES] = {
        .name = "system_ipvs_service_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of ingress bytes in the ipvs service",
    },
    [FAM_IPVS_SERVICE_OUT_BYTES] = {
        .name = "system_ipvs_service_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of egress bytes in the ipvs service",
    },
    [FAM_IPVS_SERVICE_IN_PACKETS] = {
        .name = "system_ipvs_service_in_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of ingress packets in the ipvs service",
    },
    [FAM_IPVS_SERVICE_OUT_PACKETS] = {
        .name = "system_ipvs_service_out_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of egress packets in the ipvs service",
    },
    [FAM_IPVS_DESTINATION_ACTIVE_CONNECTIONS] = {
        .name = "system_ipvs_destination_active_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of active connections in the ipvs destination",
    },
    [FAM_IPVS_DESTINATION_INACTIVE_CONNECTIONS] = {
        .name = "system_ipvs_destination_inactive_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of inactive connections in the ipvs destination",
    },
    [FAM_IPVS_DESTINATION_PERSISTENT_CONNECTIONS] = {
        .name = "system_ipvs_destination_persistent_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of persistent connections in the ipvs destination",
    },
    [FAM_IPVS_DESTINATION_CONNECTIONS] = {
        .name = "system_ipvs_destination_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of connections scheduled in the ipvs destination",
    },
    [FAM_IPVS_DESTINATION_IN_BYTES] = {
        .name = "system_ipvs_destination_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of ingress bytes in the ipvs destination",
    },
    [FAM_IPVS_DESTINATION_OUT_BYTES] = {
        .name = "system_ipvs_destination_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of egress bytes in the ipvs destination",
    },
    [FAM_IPVS_DESTINATION_IN_PACKETS] = {
        .name = "system_ipvs_destination_in_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of ingress packets in the ipvs destination",
    },
    [FAM_IPVS_DESTINATION_OUT_PACKETS] = {
        .name = "system_ipvs_destination_out_packets",
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
        PLUGIN_ERROR("getsockopt() failed: %s", STRERRNO);
        return NULL;
    }

    len = sizeof(*services) +
                sizeof(struct ip_vs_service_entry) * ipvs_info.num_services;

    services = malloc(len);
    if (services == NULL) {
        PLUGIN_ERROR("Out of memory.");
        return NULL;
    }

    services->num_services = ipvs_info.num_services;

    if (getsockopt(sockfd, IPPROTO_IP, IP_VS_SO_GET_SERVICES, services, &len) == -1) {
        PLUGIN_ERROR("getsockopt failed: %s", STRERRNO);
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
        PLUGIN_ERROR("Out of memory.");
        return NULL;
    }

    dests->fwmark = se->fwmark;
    dests->protocol = se->protocol;
    dests->addr = se->addr;
    dests->port = se->port;
    dests->num_dests = se->num_dests;

    if (getsockopt(sockfd, IPPROTO_IP, IP_VS_SO_GET_DESTS, dests, &len) == -1) {
        PLUGIN_ERROR("getsockopt() failed: %s", STRERRNO);
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

        label_set_t labels = {0};

        if (se->fwmark) {
            char buffer[24];
            ssnprintf(buffer, sizeof(buffer), "%u", se->fwmark);
            label_set_add(&labels, true, "fwmark", buffer);
        } else {
            struct in_addr saddr = {.s_addr = se->addr};
            label_set_add(&labels, true, "vip", inet_ntoa(saddr));

            char buffer[24];
            ssnprintf(buffer, sizeof(buffer), "%u", ntohs(se->port));
            label_set_add(&labels, true, "vport", buffer);
            label_set_add(&labels, true, "protocol", (se->protocol == IPPROTO_TCP) ? "TCP" : "UDP");
        }

        struct ip_vs_stats_user sstats = se->stats;

        metric_family_append(&fams[FAM_IPVS_SERVICE_CONNECTIONS],
                             VALUE_COUNTER(sstats.conns), &labels, NULL);
        metric_family_append(&fams[FAM_IPVS_SERVICE_IN_BYTES],
                             VALUE_COUNTER(sstats.inbytes), &labels, NULL);
        metric_family_append(&fams[FAM_IPVS_SERVICE_OUT_BYTES],
                             VALUE_COUNTER(sstats.outbytes), &labels, NULL);
        metric_family_append(&fams[FAM_IPVS_SERVICE_IN_PACKETS],
                             VALUE_COUNTER(sstats.inpkts), &labels, NULL);
        metric_family_append(&fams[FAM_IPVS_SERVICE_OUT_PACKETS],
                             VALUE_COUNTER(sstats.outpkts), &labels, NULL);

        struct ip_vs_get_dests *dests = ipvs_get_dests(se);
        if (dests == NULL) {
            label_set_reset(&labels);
            continue;
        }

        for (size_t n = 0; n < dests->num_dests; ++n) {
            struct ip_vs_dest_entry *de = &dests->entrytable[n];
            if (de == NULL)
                continue;

            struct in_addr daddr = {.s_addr = de->addr};
            label_set_add(&labels, true, "rip", inet_ntoa(daddr));

            char buffer[24];
            ssnprintf(buffer, sizeof(buffer), "%u", ntohs(de->port));
            label_set_add(&labels, true, "rport", buffer);

            metric_family_append(&fams[FAM_IPVS_DESTINATION_ACTIVE_CONNECTIONS],
                                 VALUE_GAUGE(de->activeconns), &labels, NULL);
            metric_family_append(&fams[FAM_IPVS_DESTINATION_INACTIVE_CONNECTIONS],
                                 VALUE_GAUGE(de->inactconns), &labels, NULL);
            metric_family_append(&fams[FAM_IPVS_DESTINATION_PERSISTENT_CONNECTIONS],
                                 VALUE_GAUGE(de->persistconns), &labels, NULL);

            struct ip_vs_stats_user dstats = de->stats;

            metric_family_append(&fams[FAM_IPVS_DESTINATION_CONNECTIONS],
                                 VALUE_COUNTER(dstats.conns), &labels, NULL);
            metric_family_append(&fams[FAM_IPVS_DESTINATION_IN_BYTES],
                                 VALUE_COUNTER(dstats.inbytes), &labels, NULL);
            metric_family_append(&fams[FAM_IPVS_DESTINATION_OUT_BYTES],
                                 VALUE_COUNTER(dstats.outbytes), &labels, NULL);
            metric_family_append(&fams[FAM_IPVS_DESTINATION_IN_PACKETS],
                                 VALUE_COUNTER(dstats.inpkts), &labels, NULL);
            metric_family_append(&fams[FAM_IPVS_DESTINATION_OUT_PACKETS],
                                 VALUE_COUNTER(dstats.outpkts), &labels, NULL);
        }

        free(dests);

        label_set_reset(&labels);
    }

    free(services);

    plugin_dispatch_metric_family_array(fams, FAM_IPVS_MAX, 0);

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
        PLUGIN_ERROR("socket() failed: %s", STRERRNO);
        return -1;
    }

    socklen_t len = sizeof(ipvs_info);

    if (getsockopt(sockfd, IPPROTO_IP, IP_VS_SO_GET_INFO, &ipvs_info, &len) == -1) {
        PLUGIN_ERROR("getsockopt() failed: %s", STRERRNO);
        close(sockfd);
        sockfd = -1;
        return -1;
    }

    /* we need IPVS >= 1.1.4 */
    if (ipvs_info.version < ((1 << 16) + (1 << 8) + 4)) {
        PLUGIN_ERROR("IPVS version too old (%u.%u.%u < %d.%d.%d)",
                     NVERSION(ipvs_info.version), 1, 1, 4);
        close(sockfd);
        sockfd = -1;
        return -1;
    } else {
        PLUGIN_INFO("Successfully connected to IPVS %u.%u.%u", NVERSION(ipvs_info.version));
    }

    return 0;
}

void module_register(void)
{
    plugin_register_init("ipvs", cipvs_init);
    plugin_register_read("ipvs", cipvs_read);
    plugin_register_shutdown("ipvs", cipvs_shutdown);
}
