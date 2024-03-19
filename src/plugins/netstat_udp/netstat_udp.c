// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2015 Håvard Eidnes
// SPDX-FileContributor:  Håvard Eidnes <he at NetBSD.org>

#include "plugin.h"
#include "libutils/common.h"

#if !defined(KERNEL_NETBSD)
#error "No applicable input method."
#endif

#include <sys/cdefs.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet6/udp6_var.h>

enum {
    FAM_UDP_RECEIVED,
    FAM_UDP_BAD_HEADER,
    FAM_UDP_BAD_LENGTH,
    FAM_UDP_BAD_CHECKSUM,
    FAM_UDP_NO_PORT,
    FAM_UDP_NO_PORT_BROADCAST,
    FAM_UDP_FULL_SOCKET,
    FAM_UDP_DELIVERED,
    FAM_UDP6_RECEIVED,
    FAM_UDP6_BAD_HEADER,
    FAM_UDP6_BAD_LENGTH,
    FAM_UDP6_BAD_CHECKSUM,
    FAM_UDP6_NO_PORT,
    FAM_UDP6_NO_PORT_MULTICAST,
    FAM_UDP6_FULL_SOCKET,
    FAM_UDP6_DELIVERED,
    FAM_MAX,
};

static metric_family_t fams[FAM_MAX] = {
    [FAM_UDP_RECEIVED] = {
        .name = "system_udp_received_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP_BAD_HEADER] = {
        .name = "system_udp_bad_header_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP_BAD_LENGTH] = {
        .name = "system_udp_bad_length_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP_BAD_CHECKSUM] = {
        .name = "system_udp_bad_checksum_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP_NO_PORT] = {
        .name = "system_udp_no_port_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP_NO_PORT_BROADCAST] = {
        .name = "system_udp_no_port_broadcast_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP_FULL_SOCKET] = {
        .name = "system_udp_full_socket_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP_DELIVERED] = {
        .name = "system_udp_delivered_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP6_RECEIVED] = {
        .name = "system_udp6_received_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP6_BAD_HEADER] = {
        .name = "system_udp6_bad_header_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP6_BAD_LENGTH] = {
        .name = "system_udp6_bad_length_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP6_BAD_CHECKSUM] = {
        .name = "system_udp6_bad_checksum_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP6_NO_PORT] = {
        .name = "system_udp6_no_port_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP6_NO_PORT_MULTICAST] = {
        .name = "system_udp6_no_port_multicast_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP6_FULL_SOCKET] = {
        .name = "system_udp6_full_socket_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP6_DELIVERED] = {
        .name = "system_udp6_delivered_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
};

static int netstat_udp_read(void)
{
    uint64_t udpstat[UDP_NSTATS];
    uint64_t udp6stat[UDP6_NSTATS];

    size_t size = sizeof(udpstat);
    if (sysctlbyname("net.inet.udp.stats", udpstat, &size, NULL, 0) == -1) {
        PLUGIN_ERROR("could not get udp stats");
    } else {
        metric_family_append(&fams[FAM_UDP_RECEIVED],
                             VALUE_COUNTER(udpstat[UDP_STAT_IPACKETS]), NULL, NULL);
        metric_family_append(&fams[FAM_UDP_BAD_HEADER],
                             VALUE_COUNTER(udpstat[UDP_STAT_HDROPS]), NULL, NULL);
        metric_family_append(&fams[FAM_UDP_BAD_LENGTH],
                             VALUE_COUNTER(udpstat[UDP_STAT_BADLEN]), NULL, NULL);
        metric_family_append(&fams[FAM_UDP_BAD_CHECKSUM],
                             VALUE_COUNTER(udpstat[UDP_STAT_BADSUM]), NULL, NULL);
        metric_family_append(&fams[FAM_UDP_NO_PORT],
                             VALUE_COUNTER(udpstat[UDP_STAT_NOPORT]), NULL, NULL);
        metric_family_append(&fams[FAM_UDP_NO_PORT_BROADCAST],
                             VALUE_COUNTER(udpstat[UDP_STAT_NOPORTBCAST]), NULL, NULL);
        metric_family_append(&fams[FAM_UDP_FULL_SOCKET],
                             VALUE_COUNTER(udpstat[UDP_STAT_FULLSOCK]), NULL, NULL);
        metric_family_append(&fams[FAM_UDP_DELIVERED],
                             VALUE_COUNTER(udpstat[UDP_STAT_IPACKETS]
                              - udpstat[UDP_STAT_HDROPS] - udpstat[UDP_STAT_BADLEN]
                              - udpstat[UDP_STAT_BADSUM] - udpstat[UDP_STAT_NOPORT]
                              - udpstat[UDP_STAT_NOPORTBCAST] - udpstat[UDP_STAT_FULLSOCK]), NULL, NULL);
    }

    size = sizeof(udp6stat);
    if (sysctlbyname("net.inet6.udp6.stats", udp6stat, &size, NULL, 0) == -1) {
        PLUGIN_ERROR("could not get udp6 stats");
    } else {
        metric_family_append(&fams[FAM_UDP6_RECEIVED],
                             VALUE_COUNTER(udpstat[UDP6_STAT_IPACKETS]), NULL, NULL);
        metric_family_append(&fams[FAM_UDP6_BAD_HEADER],
                             VALUE_COUNTER(udpstat[UDP6_STAT_HDROPS]), NULL, NULL);
        metric_family_append(&fams[FAM_UDP6_BAD_LENGTH],
                             VALUE_COUNTER(udpstat[UDP6_STAT_BADLEN]), NULL, NULL);
        metric_family_append(&fams[FAM_UDP6_BAD_CHECKSUM],
                             VALUE_COUNTER(udpstat[UDP6_STAT_BADSUM]), NULL, NULL);
        metric_family_append(&fams[FAM_UDP6_NO_PORT],
                             VALUE_COUNTER(udpstat[UDP6_STAT_NOPORT]), NULL, NULL);
        metric_family_append(&fams[FAM_UDP6_NO_PORT_MULTICAST],
                             VALUE_COUNTER(udpstat[UDP6_STAT_NOPORTMCAST]), NULL, NULL);
        metric_family_append(&fams[FAM_UDP6_FULL_SOCKET],
                             VALUE_COUNTER(udpstat[UDP6_STAT_FULLSOCK]), NULL, NULL);
        metric_family_append(&fams[FAM_UDP6_DELIVERED],
                             VALUE_COUNTER(udpstat[UDP6_STAT_IPACKETS]
                              - udpstat[UDP6_STAT_HDROPS] - udpstat[UDP6_STAT_BADLEN]
                              - udpstat[UDP6_STAT_BADSUM] - udpstat[UDP6_STAT_NOPORT]
                              - udpstat[UDP6_STAT_NOPORTMCAST] - udpstat[UDP6_STAT_FULLSOCK]), NULL, NULL);
    }

    plugin_dispatch_metric_family_array(fams, FAM_MAX, 0);

    return 0;
}

void module_register(void)
{
    plugin_register_read("netstat_udp", netstat_udp_read);
}
