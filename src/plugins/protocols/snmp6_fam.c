// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"
#include "plugins/protocols/snmp6_fam.h"
#include "plugins/protocols/flags.h"

static metric_family_t fams[FAM_SNMP6_MAX] = {
    [FAM_IP6_IN_RECEIVES] = {
        .name = "system_ip6_in_receives",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of input IPv6 datagrams received from interfaces, "
                "including those received in error.",
    },
    [FAM_IP6_IN_HEADER_ERRORS] = {
        .name = "system_ip6_in_header_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of input IPv6 datagrams discarded due to errors in their IP headers, "
                "including version number mismatch, other format errors, hop count exceeded, "
                "errors discovered in processing their IP options, etc.",
    },
    [FAM_IP6_IN_TOO_BIG_ERRORS] = {
        .name = "system_ip6_in_too_big_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of input IPv6 datagrams that could not be forwarded because "
                "their size exceeded the link MTU of outgoing interface.",
    },
    [FAM_IP6_IN_NO_ROUTES] = {
        .name = "system_ip6_in_no_routes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of input IPv6 datagrams discarded because no route could be found "
                "to transmit them to their destination.",
    },
    [FAM_IP6_IN_ADDRESS_ERRORS] = {
        .name = "system_ip6_in_address_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of input datagrams discarded because the IPv6 address "
                "in their IPv6 header's destination field was not a valid address to be received.",
    },
    [FAM_IP6_IN_UNKNOWN_PROTOCOL] = {
        .name = "system_ip6_in_unknown_protocol",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of locally-addressed datagrams received successfully but discarded "
                "because of an unknown or unsupported protocol.",
    },
    [FAM_IP6_IN_TRUNCATE_PACKETS] = {
        .name = "system_ip6_in_truncate_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of input datagrams discarded because datagram frame "
                "didn't carry enough data.",
    },
    [FAM_IP6_IN_DISCARDS] = {
        .name = "system_ip6_in_discards",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of input IPv6 datagrams for which no problems were encountered "
                "to prevent their continued processing, but which were discarded.",
    },
    [FAM_IP6_IN_DELIVERS] = {
        .name = "system_ip6_in_delivers",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of datagrams successfully delivered to IPv6 user-protocols "
                "(including ICMP).",
    },
    [FAM_IP6_OUT_FORWARDED_DATAGRAMS] = {
        .name = "system_ip6_out_forwarded_datagrams",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of output datagrams which this entity received and "
                "forwarded to their final destinations.",
    },
    [FAM_IP6_OUT_REQUESTS] = {
        .name = "system_ip6_out_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of IPv6 datagrams which local IPv6 user-protocols "
                "(including ICMP) supplied to IPv6 in requests for transmission.",
    },
    [FAM_IP6_OUT_DISCARDS] = {
        .name = "system_ip6_out_discards",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of output IPv6 datagrams for which no problem was encountered "
                "to prevent their transmission to their destination, but which were discarded.",
    },
    [FAM_IP6_OUT_NO_ROUTES]= {
        .name = "system_ip6_out_no_routes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of IPv6 datagrams discarded because no route could be found "
                "to transmit them to their destination."
    },
    [FAM_IP6_REASSEMBLY_TIMEOUT] = {
        .name = "system_ip6_reassembly_timeout",
        .type = METRIC_TYPE_COUNTER,
        .help = "The maximum number of seconds that received fragments are held "
                "while they are awaiting reassembly.",
    },
    [FAM_IP6_REASSEMBLY_REQUIRED] = {
        .name = "system_ip6_reassembly_required",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of IPv6 fragments received which needed to be reassembled.",
    },
    [FAM_IP6_REASSEMBLY_OK] = {
        .name = "system_ip6_reassembly_ok",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of IPv6 datagrams successfully reassembled."
    },
    [FAM_IP6_REASSEMBLY_FAILS] = {
        .name = "system_ip6_reassembly_fails",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of failures detected by the IPv6 re-assembly algorithm.",
    },
    [FAM_IP6_FRAGMENTED_OK] = {
        .name = "system_ip6_fragmented_ok",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of IPv6 datagrams that have been successfully fragmented.",
    },
    [FAM_IP6_FRAGMENTED_FAILS] = {
        .name = "system_ip6_fragmented_fails",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of IPv6 datagrams that have been discarded because they "
                "needed to be fragmented but could not be.",
    },
    [FAM_IP6_FRAGMENTED_CREATES] = {
        .name = "system_ip6_fragmented_creates",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of output datagram fragments that have been generated "
                "as a result of fragmentation.",
    },
    [FAM_IP6_IN_MULTICAST_PACKETS] = {
        .name = "system_ip6_in_multicast_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of multicast IPv6 packets received.",
    },
    [FAM_IP6_OUT_MULTICAST_PACKETS] = {
        .name = "system_ip6_out_multicast_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of multicast IPv6 packets transmitted.",
    },
    [FAM_IP6_IN_BYTES] = {
        .name = "system_ip6_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes received in input IPv6 datagrams "
                "including those received in error.",
    },
    [FAM_IP6_OUT_BYTES] = {
        .name = "system_ip6_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes in IPv6 datagrams delivered to the "
                "lower layers for transmission.",
    },
    [FAM_IP6_IN_MULTICAST_BYTES] = {
        .name = "system_ip6_in_multicast_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes received in IPv6 multicast datagrams.",
    },
    [FAM_IP6_OUT_MULTICAST_BYTES] = {
        .name = "system_ip6_out_multicast_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes transmitted in IPv6 multicast datagrams.",
    },
    [FAM_IP6_IN_BROADCAST_BYTES] = {
        .name = "system_ip6_in_broadcast_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes received in IPv6 broadcast datagrams.",
    },
    [FAM_IP6_OUT_BROADCAST_BYTES] = {
        .name = "system_ip6_out_broadcast_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes transmitted in IPv6 broadcast datagrams.",
    },
    [FAM_IP6_IN_NOECT_PACKETS] = {
        .name = "system_ip6_in_noect_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets received with not ECN-Capable Transport.",
    },
    [FAM_IP6_IN_ECT1_PACKETS] = {
        .name = "system_ip6_in_ect1_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets received with ECN Capable Transport(1).",
    },
    [FAM_IP6_IN_ECT0_PACKETS] = {
        .name = "system_ip6_in_ect0_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets received with ECN Capable Transport(0).",
    },
    [FAM_IP6_IN_CE_PACKETS] =  {
        .name = "system_ip6_in_ce_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets received with Congestion Experienced.",
    },
    [FAM_IP6_OUT_TRANSMITS] = {
        .name = "system_ip6_out_transmits",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of IPv6 datagrams that this entity supplied to the lower layers "
                "for transmission. This includes datagrams generated locally and those forwarded "
                "by this entity.",
    },
    [FAM_ICMP6_IN_MESSAGES] = {
        .name = "system_icmp6_in_messages",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of ICMPv6 messages which the entity received.",
    },
    [FAM_ICMP6_IN_ERRORS] = {
        .name = "system_icmp6_in_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 messages which the entity received "
                "but determined as having ICMP-specific errors.",
    },
    [FAM_ICMP6_OUT_MESSAGES] = {
        .name = "system_icmp6_out_messages",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of ICMPv6 messages which this entity attempted to send.",
    },
    [FAM_ICMP6_OUT_ERRORS] = {
        .name = "system_icmp6_out_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 messages which this entity did not send "
                "due to problems discovered within ICMP such as a lack of buffers.",
    },
    [FAM_ICMP6_IN_CSUM_ERROR] = {
        .name = "system_icmp6_in_csum_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 messages which the checksum of the ICMP packet is wrong.",
    },
    [FAM_ICMP6_IN_DESTINATION_UNREACHABLE] = {
        .name = "system_icmp6_in_destination_unreachable",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Destination Unreachable messages received.",
    },
    [FAM_ICMP6_IN_PACKET_TOO_BIG] = {
        .name = "system_icmp6_in_packet_too_big",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Packet too big messages received.",
    },
    [FAM_ICMP6_IN_TIME_EXCEEDED] = {
        .name = "system_icmp6_in_time_exceeded",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Time Exceeded messages received.",
    },
    [FAM_ICMP6_IN_PARAMETER_PROBLEM] = {
        .name = "system_icmp6_in_parameter_problem",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Parameter Problem messages received.",
    },
    [FAM_ICMP6_IN_ECHO_REQUEST] = {
        .name = "system_icmp6_in_echo_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Echo (request) messages received.",
    },
    [FAM_ICMP6_IN_ECHO_REPLY] = {
        .name = "system_icmp6_in_echo_reply",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Echo Reply messages received.",
    },
    [FAM_ICMP6_IN_MULTICAST_LISTENER_QUERY] = {
        .name = "system_icmp6_in_multicast_listener_query",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Multicast Listener Query messages received.",
    },
    [FAM_ICMP6_IN_MULTICAST_LISTENER_REPORT] = {
        .name = "system_icmp6_in_multicast_listener_report",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Multicast Listener Report messages received.",
    },
    [FAM_ICMP6_IN_MULTICAST_LISTENER_DONE] = {
        .name = "system_icmp6_in_multicast_listener_done",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Multicast Listener Done messages received.",
    },
    [FAM_ICMP6_IN_ROUTER_SOLICITATION] = {
        .name = "system_icmp6_in_router_solicitation",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Router Solicitation messages received.",
    },
    [FAM_ICMP6_IN_ROUTER_ADVERTISEMENT] = {
        .name = "system_icmp6_in_router_advertisement",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Router Advertisement messages received.",
    },
    [FAM_ICMP6_IN_NEIGHBOR_SOLICITATION] = {
        .name = "system_icmp6_in_neighbor_solicitation",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Neighbor Solicitation messages received.",
    },
    [FAM_ICMP6_IN_NEIGHBOR_ADVERTISEMENT] = {
        .name = "system_icmp6_in_neighbor_advertisement",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Neighbor Advertisement messages received.",
    },
    [FAM_ICMP6_IN_REDIRECT] = {
        .name = "system_icmp6_in_redirect",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Redirect messages received.",
    },
    [FAM_ICMP6_IN_MULTICAST_LISTENER_DISCOVERY_REPORTS] = {
        .name = "system_icmp6_in_multicast_listener_discovery_reports",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Multicast Listener Discovery Reports messages received.",
    },
    [FAM_ICMP6_OUT_DESTINATION_UNREACHABLE] = {
        .name = "system_icmp6_out_destination_unreachable",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Destination Unreachable messages sent."
    },
    [FAM_ICMP6_OUT_PACKET_TOO_BIG] = {
        .name = "system_icmp6_out_packet_too_big",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Packet too big messages sent.",
    },
    [FAM_ICMP6_OUT_TIME_EXCEEDED] = {
        .name = "system_icmp6_out_time_exceeded",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Time Exceeded messages sent.",
    },
    [FAM_ICMP6_OUT_PARAMETER_PROBLEM] = {
        .name = "system_icmp6_out_parameter_problem",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Parameter Problem messages sent.",
    },
    [FAM_ICMP6_OUT_ECHO_REQUEST] = {
        .name = "system_icmp6_out_echo_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Echo (request) messages sent.",
    },
    [FAM_ICMP6_OUT_ECHO_REPLY] = {
        .name = "system_icmp6_out_echo_reply",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Echo Reply messages sent.",
    },
    [FAM_ICMP6_OUT_MULTICAST_LISTENER_QUERY] = {
        .name = "system_icmp6_out_multicast_listener_query",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Multicast Listener Query messages sent.",
    },
    [FAM_ICMP6_OUT_MULTICAST_LISTENER_REPORT] = {
        .name = "system_icmp6_out_multicast_listener_report",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Multicast Listener Report messages sent.",
    },
    [FAM_ICMP6_OUT_MULTICAST_LISTENER_DONE] = {
        .name = "system_icmp6_out_multicast_listener_done",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Multicast Listener Done messages sent.",
    },
    [FAM_ICMP6_OUT_ROUTER_SOLICITATION] = {
        .name = "system_icmp6_out_router_solicitation",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Router Solicitation messages sent.",
    },
    [FAM_ICMP6_OUT_ROUTER_ADVERTISEMENT] = {
        .name = "system_icmp6_out_router_advertisement",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Router Advertisement messages sent.",
    },
    [FAM_ICMP6_OUT_NEIGHBOR_SOLICITATION] = {
        .name = "system_icmp6_out_neighbor_solicitation",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Neighbor Solicitation messages sent.",
    },
    [FAM_ICMP6_OUT_NEIGHBOR_ADVERTISEMENT] = {
        .name = "system_icmp6_out_neighbor_advertisement",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Neighbor Advertisement messages sent.",
    },
    [FAM_ICMP6_OUT_OUT_REDIRECT] = {
        .name = "system_icmp6_out_out_redirect",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv2 Redirect messages sent.",
    },
    [FAM_ICMP6_OUT_MULTICAST_LISTENER_DISCOVERY_REPORTS] = {
        .name = "system_icmp6_out_multicast_listener_discovery_reports",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 Multicast Listener Discovery Reports messages sent.",
    },
    [FAM_ICMP6_IN_TYPE] = {
        .name = "system_icmp6_in_type",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 messages received by type.",
    },
    [FAM_ICMP6_OUT_TYPE] = {
        .name = "system_icmp6_out_type",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMPv6 messages sent by type.",
    },
    [FAM_UDP6_IN_DATAGRAMS] = {
        .name = "system_udp6_in_datagrams",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of UDPv6 datagrams delivered to UDP users.",
    },
    [FAM_UDP6_NO_PORTS] = {
        .name = "system_udp6_no_ports",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of received UDPv6 datagrams for which "
                "there was no application at the destination port.",
    },
    [FAM_UDP6_IN_ERRORS] = {
        .name = "system_udp6_in_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of received UDPv6 datagrams that could not be delivered "
                "for reasons other than the lack of an application at the destination port.",
    },
    [FAM_UDP6_OUT_DATAGRAMS] = {
        .name = "system_udp6_out_datagrams",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of UDPv6 datagrams sent from this entity.",
    },
    [FAM_UDP6_RECV_BUFFER_ERRORS] = {
        .name = "system_udp6_ recv_buffer_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased when memory cannot be allocated to process an incoming UDPv6 packet.",
    },
    [FAM_UDP6_SEND_BUFFER_ERRORS] = {
        .name = "system_udp6_send_buffer_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased when memory cannot be allocated to send an UDPv6 packet.",
    },
    [FAM_UDP6_IN_CSUM_ERRORS] = {
        .name = "system_udp6_in_csum_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased when a received UDPv6 packet has an invalid checksum."
    },
    [FAM_UDP6_IGNORED_MULTI] = {
        .name = "system_udp6_ignored_multi",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP6_MEMORY_ERRORS] = {
        .name = "system_udp6_memory_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDPLITE6_IN_DATAGRAMS] = {
        .name = "system_udplite6_in_datagrams",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of UDP-Litev6 datagrams that were delivered to UDP-Lite users.",
    },
    [FAM_UDPLITE6_NO_PORTS] = {
        .name = "system_udplite6_no_ports",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of received UDP-Litev6 datagrams for which "
                "there was no listener at the destination port.",
    },
    [FAM_UDPLITE6_IN_ERRORS] = {
        .name = "system_udplite6_in_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of received UDP-Litev6 datagrams that could not be delivered "
                "for reasons other than the lack of an application at the destination port.",
    },
    [FAM_UDPLITE6_OUT_DATAGRAMS] = {
        .name = "system_udplite6_out_datagrams",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of UDP-Litev6 datagrams sent from this entity.",
    },
    [FAM_UDPLITE6_RECV_BUFFER_ERRORS] = {
        .name = "system_udplite6_recv_buffer_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased when memory cannot be allocated to process an incoming UDP-Lite packet.",
    },
    [FAM_UDPLITE6_SEND_BUFFER_ERRORS] = {
        .name = "system_udplite6_send_buffer_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased when memory cannot be allocated to send an UDP-Litev6 packet."
    },
    [FAM_UDPLITE6_IN_CSUM_ERRORS] = {
        .name = "system_udplite6_in_csum_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased when a received UDP-Litev6 packet has an invalid checksum.",
    },
    [FAM_UDPLITE6_MEMORY_ERRORS] = {
        .name = "system_udplite6_memory_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL
    },
};


struct snmp6_metric {
    char *key;
    uint64_t flags;
    int fam;
};

const struct snmp6_metric *snmp6_get_key (register const char *str, register size_t len);


static char *path_proc_snmp6;
static bool proc_snmp6_found = false;

int snmp6_init(void)
{
    path_proc_snmp6 = plugin_procpath("net/snmp6");
    if (path_proc_snmp6 == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    if (access(path_proc_snmp6, R_OK) != 0) {
        PLUGIN_ERROR("Cannot access %s: %s", path_proc_snmp6, STRERRNO);
        return -1;
    }

    proc_snmp6_found = true;

    return 0;
}

int snmp6_shutdown(void)
{
    free(path_proc_snmp6);
    return 0;
}

int snmp6_read(uint64_t flags, exclist_t *excl_value)
{
    if (!proc_snmp6_found)
        return 0;

    if (!(flags & (COLLECT_ICMP6 | COLLECT_IP6 | COLLECT_UDP6 | COLLECT_UDPLITE6)))
        return 0;

    FILE *fh = fopen(path_proc_snmp6, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Open '%s' failed: %s", path_proc_snmp6, STRERRNO);
        return -1;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[2] = {0};

        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (fields_num < 2)
            continue;

        if (!exclist_match(excl_value, fields[0]))
            continue;

        const struct snmp6_metric *m = snmp6_get_key(fields[0], strlen(fields[0]));
        if (m == NULL) {
            if (strncmp("Icmp6InType", fields[0], strlen("Icmp6InType")) == 0) {
                if (!(flags & COLLECT_ICMP6))
                    continue;
                char *type = fields[0] + strlen("Icmp6InType");
                uint64_t value = atol(fields[1]);
                metric_family_append(&fams[FAM_ICMP6_IN_TYPE], VALUE_COUNTER(value),
                                     NULL, &(label_pair_const_t){.name="type", .value=type}, NULL);
            } else if (strncmp("Icmp6OutType", fields[0], strlen("Icmp6OutType")) == 0) {
                if (!(flags & COLLECT_ICMP6))
                    continue;
                char *type = fields[0] + strlen("Icmp6OutType");
                uint64_t value = atol(fields[1]);
                metric_family_append(&fams[FAM_ICMP6_OUT_TYPE], VALUE_COUNTER(value),
                                     NULL, &(label_pair_const_t){.name="type", .value=type}, NULL);
            }

            continue;
        }

        if (!(m->flags))
            continue;

        if (fams[m->fam].type == METRIC_TYPE_GAUGE) {
            double value = atof(fields[1]);
            if (!isfinite(value))
                continue;
            metric_family_append(&fams[m->fam], VALUE_GAUGE(value), NULL, NULL);
        } else if (fams[m->fam].type == METRIC_TYPE_COUNTER) {
            uint64_t value = atol(fields[1]);
            metric_family_append(&fams[m->fam], VALUE_COUNTER(value), NULL, NULL);
        }

    }

    fclose(fh);

    plugin_dispatch_metric_family_array(fams, FAM_SNMP6_MAX, 0);

    return 0;
}
