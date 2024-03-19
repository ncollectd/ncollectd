// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2019  Marco van Tol
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Marco van Tol <marco at tols.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"

#include <sys/sysctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <netinet6/ip6_var.h>

#ifndef KERNEL_FREEBSD
    #error "No applicable input method."
#endif

enum {
    FAM_IP_PACKETS,
    FAM_IP_BADSUM_PACKETS,
    FAM_IP_TOOSHORT_PACKETS,
    FAM_IP_TOOSMALL_PACKETS,
    FAM_IP_BADHLEN_PACKETS,
    FAM_IP_BADLEN_PACKETS,
    FAM_IP_FRAGMENTS_PACKETS,
    FAM_IP_FRAGDROPPED_PACKETS,
    FAM_IP_FRAGTIMEOUT_PACKETS,
    FAM_IP_FORWARD_PACKETS,
    FAM_IP_FASTFORWARD_PACKETS,
    FAM_IP_CANTFORWARD_PACKETS,
    FAM_IP_REDIRECTSENT_PACKETS,
    FAM_IP_NOPROTO_PACKETS,
    FAM_IP_DELIVERED_PACKETS,
    FAM_IP_LOCALOUT_PACKETS,
    FAM_IP_ODROPPED_PACKETS,
    FAM_IP_REASSEMBLED_PACKETS,
    FAM_IP_FRAGMENTED_PACKETS,
    FAM_IP_OFRAGMENTS_PACKETS,
    FAM_IP_CANTFRAG_PACKETS,
    FAM_IP_BADOPTIONS_PACKETS,
    FAM_IP_NOROUTE_PACKETS,
    FAM_IP_BADVERS_PACKETS,
    FAM_IP_RAWOUT_PACKETS,
    FAM_IP_TOOLONG_PACKETS,
    FAM_IP_NOTMEMBER_PACKETS,
    FAM_IP_NOGIF_PACKETS,
    FAM_IP_BADADDR_PACKETS,
    FAM_IP6_PACKETS,
    FAM_IP6_TOOSHORT_PACKETS,
    FAM_IP6_TOOSMALL_PACKETS,
    FAM_IP6_FRAGMENTS_PACKETS,
    FAM_IP6_FRAGDROPPED_PACKETS,
    FAM_IP6_FRAGTIMEOUT_PACKETS,
    FAM_IP6_FRAGOVERFLOW_PACKETS,
    FAM_IP6_FORWARD_PACKETS,
    FAM_IP6_CANTFORWARD_PACKETS,
    FAM_IP6_REDIRECTSENT_PACKETS,
    FAM_IP6_DELIVERED_PACKETS,
    FAM_IP6_LOCALOUT_PACKETS,
    FAM_IP6_ODROPPED_PACKETS,
    FAM_IP6_REASSEMBLED_PACKETS,
    FAM_IP6_FRAGMENTED_PACKETS,
    FAM_IP6_OFRAGMENTS_PACKETS,
    FAM_IP6_CANTFRAG_PACKETS,
    FAM_IP6_BADOPTIONS_PACKETS,
    FAM_IP6_NOROUTE_PACKETS,
    FAM_IP6_BADVERS_PACKETS,
    FAM_IP6_RAWOUT_PACKETS,
    FAM_IP6_BADSCOPE_PACKETS,
    FAM_FAMIP6_NOTMEMBER_PACKETS,
    FAM_IP6_NOGIF_PACKETS,
    FAM_IP6_TOOMANYHDR_PACKETS,
    FAM_IP_MAX
};

static metric_family_t fams[FAM_IP_MAX] = {
    [FAM_IP_PACKETS] = {
        .name = "system_ip_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_BADSUM_PACKETS] = {
        .name = "system_ip_badsum_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_TOOSHORT_PACKETS] = {
        .name = "system_ip_tooshort_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_TOOSMALL_PACKETS] = {
        .name = "system_ip_toosmall_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_BADHLEN_PACKETS] = {
        .name = "system_ip_badhlen_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_BADLEN_PACKETS] = {
        .name = "system_ip_badlen_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_FRAGMENTS_PACKETS] = {
        .name = "system_ip_fragments_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_FRAGDROPPED_PACKETS] = {
        .name = "system_ip_fragdropped_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_FRAGTIMEOUT_PACKETS] = {
        .name = "system_ip_fragtimeout_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_FORWARD_PACKETS] = {
        .name = "system_ip_forward_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_FASTFORWARD_PACKETS] = {
        .name = "system_ip_fastforward_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_CANTFORWARD_PACKETS] = {
        .name = "system_ip_cantforward_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_REDIRECTSENT_PACKETS] = {
        .name = "system_ip_redirectsent_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_NOPROTO_PACKETS] = {
        .name = "system_ip_noproto_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_DELIVERED_PACKETS] = {
        .name = "system_ip_delivered_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_LOCALOUT_PACKETS] = {
        .name = "system_ip_localout_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_ODROPPED_PACKETS] = {
        .name = "system_ip_odropped_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_REASSEMBLED_PACKETS] = {
        .name = "system_ip_reassembled_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_FRAGMENTED_PACKETS] = {
        .name = "system_ip_fragmented_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_OFRAGMENTS_PACKETS] = {
        .name = "system_ip_ofragments_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_CANTFRAG_PACKETS] = {
        .name = "system_ip_cantfrag_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_BADOPTIONS_PACKETS] = {
        .name = "system_ip_badoptions_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_NOROUTE_PACKETS] = {
        .name = "system_ip_noroute_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_BADVERS_PACKETS] = {
        .name = "system_ip_badvers_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_RAWOUT_PACKETS] = {
        .name = "system_ip_rawout_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_TOOLONG_PACKETS] = {
        .name = "system_ip_toolong_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_NOTMEMBER_PACKETS] = {
        .name = "system_ip_notmember_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_NOGIF_PACKETS] = {
        .name = "system_ip_nogif_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP_BADADDR_PACKETS] = {
        .name = "system_ip_badaddr_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_PACKETS] = {
        .name = "system_ip6_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_TOOSHORT_PACKETS] = {
        .name = "system_ip6_tooshort_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_TOOSMALL_PACKETS] = {
        .name = "system_ip6_toosmall_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_FRAGMENTS_PACKETS] = {
        .name = "system_ip6_fragments_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_FRAGDROPPED_PACKETS] = {
        .name = "system_ip6_fragdropped_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_FRAGTIMEOUT_PACKETS] = {
        .name = "system_ip6_fragtimeout_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_FRAGOVERFLOW_PACKETS] = {
        .name = "system_ip6_fragoverflow_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_FORWARD_PACKETS] = {
        .name = "system_ip6_forward_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_CANTFORWARD_PACKETS] = {
        .name = "system_ip6_cantforward_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_REDIRECTSENT_PACKETS] = {
        .name = "system_ip6_redirectsent_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_DELIVERED_PACKETS] = {
        .name = "system_ip6_delivered_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_LOCALOUT_PACKETS] = {
        .name = "system_ip6_localout_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_ODROPPED_PACKETS] = {
        .name = "system_ip6_odropped_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_REASSEMBLED_PACKETS] = {
        .name = "system_ip6_reassembled_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_FRAGMENTED_PACKETS] = {
        .name = "system_ip6_fragmented_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_OFRAGMENTS_PACKETS] = {
        .name = "system_ip6_ofragments_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_CANTFRAG_PACKETS] = {
        .name = "system_ip6_cantfrag_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_BADOPTIONS_PACKETS] = {
        .name = "system_ip6_badoptions_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_NOROUTE_PACKETS] = {
        .name = "system_ip6_noroute_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_BADVERS_PACKETS] = {
        .name = "system_ip6_badvers_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_RAWOUT_PACKETS] = {
        .name = "system_ip6_rawout_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_BADSCOPE_PACKETS] = {
        .name = "system_ip6_badscope_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_FAMIP6_NOTMEMBER_PACKETS] = {
        .name = "system_ip6_notmember_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_NOGIF_PACKETS] = {
        .name = "system_ip6_nogif_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_IP6_TOOMANYHDR_PACKETS] = {
        .name = "system_ip6_toomanyhdr_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    }
};

static int ipstats_read(void)
{
    struct ipstat ipstat;
    size_t ipslen = sizeof(ipstat);
    char mib[] = "net.inet.ip.stats";

    if (sysctlbyname(mib, &ipstat, &ipslen, NULL, 0) != 0) {
        PLUGIN_WARNING("ipstats plugin: sysctl \"%s\" failed.", mib);
    } else {
        metric_family_append(&fams[FAM_IP_PACKETS],
                             VALUE_COUNTER(ipstat.ips_total), NULL, NULL);
        metric_family_append(&fams[FAM_IP_BADSUM_PACKETS],
                             VALUE_COUNTER(ipstat.ips_badsum), NULL, NULL);
        metric_family_append(&fams[FAM_IP_TOOSHORT_PACKETS],
                             VALUE_COUNTER(ipstat.ips_tooshort), NULL, NULL);
        metric_family_append(&fams[FAM_IP_TOOSMALL_PACKETS],
                             VALUE_COUNTER(ipstat.ips_toosmall), NULL, NULL);
        metric_family_append(&fams[FAM_IP_BADHLEN_PACKETS],
                             VALUE_COUNTER(ipstat.ips_badhlen), NULL, NULL);
        metric_family_append(&fams[FAM_IP_BADLEN_PACKETS],
                             VALUE_COUNTER(ipstat.ips_badlen), NULL, NULL);
        metric_family_append(&fams[FAM_IP_FRAGMENTS_PACKETS],
                             VALUE_COUNTER(ipstat.ips_fragments), NULL, NULL);
        metric_family_append(&fams[FAM_IP_FRAGDROPPED_PACKETS],
                             VALUE_COUNTER(ipstat.ips_fragdropped), NULL, NULL);
        metric_family_append(&fams[FAM_IP_FRAGTIMEOUT_PACKETS],
                             VALUE_COUNTER(ipstat.ips_fragtimeout), NULL, NULL);
        metric_family_append(&fams[FAM_IP_FORWARD_PACKETS],
                             VALUE_COUNTER(ipstat.ips_forward), NULL, NULL);
        metric_family_append(&fams[FAM_IP_FASTFORWARD_PACKETS],
                             VALUE_COUNTER(ipstat.ips_fastforward), NULL, NULL);
        metric_family_append(&fams[FAM_IP_CANTFORWARD_PACKETS],
                             VALUE_COUNTER(ipstat.ips_cantforward), NULL, NULL);
        metric_family_append(&fams[FAM_IP_REDIRECTSENT_PACKETS],
                             VALUE_COUNTER(ipstat.ips_redirectsent), NULL, NULL);
        metric_family_append(&fams[FAM_IP_NOPROTO_PACKETS],
                             VALUE_COUNTER(ipstat.ips_noproto), NULL, NULL);
        metric_family_append(&fams[FAM_IP_DELIVERED_PACKETS],
                             VALUE_COUNTER(ipstat.ips_delivered), NULL, NULL);
        metric_family_append(&fams[FAM_IP_LOCALOUT_PACKETS],
                             VALUE_COUNTER(ipstat.ips_localout), NULL, NULL);
        metric_family_append(&fams[FAM_IP_ODROPPED_PACKETS],
                             VALUE_COUNTER(ipstat.ips_odropped), NULL, NULL);
        metric_family_append(&fams[FAM_IP_REASSEMBLED_PACKETS],
                             VALUE_COUNTER(ipstat.ips_reassembled), NULL, NULL);
        metric_family_append(&fams[FAM_IP_FRAGMENTED_PACKETS],
                             VALUE_COUNTER(ipstat.ips_fragmented), NULL, NULL);
        metric_family_append(&fams[FAM_IP_OFRAGMENTS_PACKETS],
                             VALUE_COUNTER(ipstat.ips_ofragments), NULL, NULL);
        metric_family_append(&fams[FAM_IP_CANTFRAG_PACKETS],
                             VALUE_COUNTER(ipstat.ips_cantfrag), NULL, NULL);
        metric_family_append(&fams[FAM_IP_BADOPTIONS_PACKETS],
                             VALUE_COUNTER(ipstat.ips_badoptions), NULL, NULL);
        metric_family_append(&fams[FAM_IP_NOROUTE_PACKETS],
                             VALUE_COUNTER(ipstat.ips_noroute), NULL, NULL);
        metric_family_append(&fams[FAM_IP_BADVERS_PACKETS],
                             VALUE_COUNTER(ipstat.ips_badvers), NULL, NULL);
        metric_family_append(&fams[FAM_IP_RAWOUT_PACKETS],
                             VALUE_COUNTER(ipstat.ips_rawout), NULL, NULL);
        metric_family_append(&fams[FAM_IP_TOOLONG_PACKETS],
                             VALUE_COUNTER(ipstat.ips_toolong), NULL, NULL);
        metric_family_append(&fams[FAM_IP_NOTMEMBER_PACKETS],
                             VALUE_COUNTER(ipstat.ips_notmember), NULL, NULL);
        metric_family_append(&fams[FAM_IP_NOGIF_PACKETS],
                             VALUE_COUNTER(ipstat.ips_nogif), NULL, NULL);
        metric_family_append(&fams[FAM_IP_BADADDR_PACKETS],
                             VALUE_COUNTER(ipstat.ips_badaddr), NULL, NULL);
    }

    struct ip6stat ip6stat;
    size_t ip6slen = sizeof(ip6stat);
    char mib6[] = "net.inet6.ip6.stats";

    if (sysctlbyname(mib6, &ip6stat, &ip6slen, NULL, 0) != 0) {
        PLUGIN_WARNING("ipstats plugin: sysctl \"%s\" failed.", mib6);
    } else {
        metric_family_append(&fams[FAM_IP6_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_total), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_TOOSHORT_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_tooshort), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_TOOSMALL_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_toosmall), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_FRAGMENTS_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_fragments), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_FRAGDROPPED_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_fragdropped), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_FRAGTIMEOUT_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_fragtimeout), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_FRAGOVERFLOW_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_fragoverflow), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_FORWARD_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_forward), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_CANTFORWARD_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_cantforward), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_REDIRECTSENT_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_redirectsent), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_DELIVERED_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_delivered), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_LOCALOUT_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_localout), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_ODROPPED_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_odropped), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_REASSEMBLED_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_reassembled), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_FRAGMENTED_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_fragmented), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_OFRAGMENTS_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_ofragments), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_CANTFRAG_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_cantfrag), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_BADOPTIONS_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_badoptions), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_NOROUTE_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_noroute), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_BADVERS_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_badvers), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_RAWOUT_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_rawout), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_BADSCOPE_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_badscope), NULL, NULL);
        metric_family_append(&fams[FAM_FAMIP6_NOTMEMBER_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_notmember), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_NOGIF_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_nogif), NULL, NULL);
        metric_family_append(&fams[FAM_IP6_TOOMANYHDR_PACKETS],
                             VALUE_COUNTER(ip6stat.ip6s_toomanyhdr), NULL, NULL);
    }

    plugin_dispatch_metric_family_array(fams, FAM_IP_MAX, 0);
    return 0;
}

void module_register(void)
{
    plugin_register_read("ipstats", ipstats_read);
}
