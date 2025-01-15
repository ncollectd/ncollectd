// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "plugins/protocols/snmp_fam.h"
#include "plugins/protocols/flags.h"

static metric_family_t fams[FAM_SNMP_MAX] = {
    [FAM_IP_FORWARDING] = {
        .name = "system_ip_forwarding",
        .type = METRIC_TYPE_GAUGE,
        .help = "The indication of whether this system is acting as an IP router "
                "in respect to the forwarding of datagrams received by, "
                "but not addressed to, this system.",
    },
    [FAM_IP_DEFAULT_TTL] = {
        .name = "system_ip_default_ttl",
        .type = METRIC_TYPE_GAUGE,
        .help = "The default value inserted into the Time-To-Live field "
                "of the IP header of datagrams originated at this entity, "
                "whenever a TTL value is not supplied by the transport layer.",
    },
    [FAM_IP_IN_RECEIVES] = {
        .name = "system_ip_in_receives",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of input datagrams received from interfaces, "
                "including those received in error.",
    },
    [FAM_IP_IN_HEADER_ERRORS] = {
        .name = "system_ip_in_header_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of input datagrams discarded due to errors in their IP headers, "
                "including bad checksums, version number mismatch, other format errors, "
                "time-to-live exceeded, errors discovered in processing their IP options, etc.",
    },
    [FAM_IP_IN_ADDRESS_ERRORS] = {
        .name = "system_ip_in_address_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of input datagrams discarded because "
                "the IP address in their IP header's destination field was "
                "not a valid address to be received at this entity.",
    },
    [FAM_IP_FORWARD_DATAGRAMS] = {
        .name = "system_ip_forward_datagrams",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of input datagrams for which this entity "
                "was not their final IP destination, as a result of which an attempt "
                "was made to find a route to forward them to that final destination.",
    },
    [FAM_IP_IN_UNKNOWN_PROTOCOL] = {
        .name = "system_ip_in_unknown_protocol",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of locally-addressed datagrams received successfully "
                "but discarded because of an unknown or unsupported protocol.",
    },
    [FAM_IP_IN_DISCARDS] = {
        .name = "system_ip_in_discards",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of input IP datagrams for which no problems were encountered "
                "to prevent their continued processing, but which were discarded.",
    },
    [FAM_IP_IN_DELIVERS] = {
        .name = "system_ip_in_delivers",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of input datagrams successfully delivered "
                "to IP user-protocols (including ICMP).",
    },
    [FAM_IP_OUT_REQUESTS] = {
        .name = "system_ip_out_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of IP datagrams which local IP user-protocols (including ICMP) "
                "supplied to IP in requests for transmission.",
    },
    [FAM_IP_OUT_DISCARDS] = {
        .name = "system_ip_out_discards",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of output IP datagrams for which no problem was encountered "
                "to prevent their transmission to their destination, but which were discarded.",
    },
    [FAM_IP_OUT_NO_ROUTES] = {
        .name = "system_ip_out_no_routes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of IP datagrams discarded because "
                "no route could be found to transmit them to their destination.",
    },
    [FAM_IP_REASSEMBLY_TIMEOUT] = {
        .name = "system_ip_reassembly_timeout",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum number of seconds which received fragments "
                "are held while they are awaiting reassembly at this entity.",
    },
    [FAM_IP_REASSEMBLY_REQUIRED] = {
        .name = "system_ip_reassembly_required",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of IP fragments received which "
                "needed to be reassembled at this entity.",
    },
    [FAM_IP_REASSEMBLY_OKS] = {
        .name = "system_ip_reassembly_oks",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of IP datagrams successfully re-assembled."
    },
    [FAM_IP_REASSEMBLY_FAILS] = {
        .name = "system_ip_reassembly_fails",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of failures detected by the IP re-assembly algorithm.",
    },
    [FAM_IP_FRAGMENTED_OKS] = {
        .name = "system_ip_fragmented_oks",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of IP datagrams that have been successfully fragmented at this entity.",
    },
    [FAM_IP_FRAGMENTED_FAILS] = {
        .name = "system_ip_fragmented_fails",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of IP datagrams that have been discarded because they needed "
                "to be fragmented at this entity but could not be.",
    },
    [FAM_IP_FRAGMENTED_CREATES] = {
        .name = "system_ip_fragmented_creates",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of IP datagram fragments that have been generated "
                "as a result of fragmentation at this entity.",
    },
    [FAM_IP_OUT_TRANSMITS] = {
        .name = "system_ip_out_transmits",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of IP datagrams that this entity supplied to the lower layers "
                "for transmission. This includes datagrams generated locally and those forwarded "
                "by this entity.",
    },
    [FAM_ICMP_IN_MESSAGES] = {
        .name = "system_icmp_in_messages",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of ICMP messages which the entity received.",
    },
    [FAM_ICMP_IN_ERRORS]  = {
        .name = "system_icmp_in_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP messages which the entity received "
                "but determined as having ICMP-specific errors.",
    },
    [FAM_ICMP_IN_CSUM_ERRORS] = {
        .name = "system_icmp_in_csum_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP messages which the checksum of the ICMP packet is wrong.",
    },
    [FAM_ICMP_IN_DESTINATION_UNREACHABLE] = {
        .name = "system_icmp_in_destination_unreachable",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Destination Unreachable messages received.",
    },
    [FAM_ICMP_OUT_MESSAGES] = {
        .name = "system_icmp_out_messages",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of ICMP messages which this entity attempted to send.",
    },
    [FAM_ICMP_OUT_ERRORS] = {
        .name = "system_icmp_out_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP messages which this entity did not send "
                "due to problems discovered within ICMP such as a lack of buffers.",
    },
    [FAM_ICMP_IN_TIME_EXCEEDED] = {
        .name = "system_icmp_in_time_exceeded",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Time Exceeded messages received."
    },
    [FAM_ICMP_IN_PARAMETER_PROBLEM] = {
        .name = "system_icmp_in_parameter_problem",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Parameter Problem messages received."
    },
    [FAM_ICMP_IN_SOURCE_QUENCH] = {
        .name = "system_icmp_in_source_quench",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Source Quench messages received."
    },
    [FAM_ICMP_IN_REDIRECT] = {
        .name = "system_icmp_in_redirect",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Redirect messages received."
    },
    [FAM_ICMP_IN_ECHO_REQUEST] = {
        .name = "system_icmp_in_echo_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Echo (request) messages received."
    },
    [FAM_ICMP_IN_ECHO_REPLY] = {
        .name = "system_icmp_in_echo_reply",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Echo Reply messages received."
    },
    [FAM_ICMP_IN_TIMESTAMP_REQUEST] = {
        .name = "system_icmp_in_timestamp_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Timestamp (request) messages received."
    },
    [FAM_ICMP_IN_TIMESTAMP_REPLY] = {
        .name = "system_icmp_in_timestamp_reply",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Timestamp Reply messages received."
    },
    [FAM_ICMP_IN_ADDRESS_MASK_REQUEST] = {
        .name = "system_icmp_in_address_mask_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Address Mask Request messages received."
    },
    [FAM_ICMP_IN_ADDRESS_MASK_REPLY] = {
        .name = "system_icmp_in_address_mask_reply",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Address Mask Reply messages received."
    },
    [FAM_ICMP_OUT_DESTINATION_UNREACHABLE] = {
        .name = "system_icmp_out_destination_unreachable",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Destination Unreachable messages sent."
    },
    [FAM_ICMP_OUT_TIME_EXCEEDED] = {
        .name = "system_icmp_out_time_exceeded",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Time Exceeded messages sent."
    },
    [FAM_ICMP_OUT_PARAMETER_PROBLEM] = {
        .name = "system_icmp_out_parameter_problem",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Parameter Problem messages sent."
    },
    [FAM_ICMP_OUT_SOURCE_QUENCH] = {
        .name = "system_icmp_out_source_quench",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Source Quench messages sent."
    },
    [FAM_ICMP_OUT_REDIRECT] = {
        .name = "system_icmp_out_redirect",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Redirect messages sent.",
    },
    [FAM_ICMP_OUT_ECHO_REQUEST] = {
        .name = "system_icmp_out_echo_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Echo (request) messages sent."
    },
    [FAM_ICMP_OUT_ECHO_REPLY] = {
        .name = "system_icmp_out_echo_reply",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Echo Reply messages sent."
    },
    [FAM_ICMP_OUT_TIMESTAMP_REQUEST] = {
        .name = "system_icmp_out_timestamp_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Timestamp (request) messages sent."
    },
    [FAM_ICMP_OUT_TIMESTAMP_REPLY] = {
        .name = "system_icmp_out_timestamp_reply",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Timestamp Reply messages sent."
    },
    [FAM_ICMP_OUT_ADDRESS_MASK_REQUEST] = {
        .name = "system_icmp_out_address_mask_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Address Mask Request messages sent."
    },
    [FAM_ICMP_OUT_ADDRESS_MASK_REPLY] = {
        .name = "system_icmp_out_address_mask_reply",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP Address Mask Reply messages sent."
    },
    [FAM_ICMP_IN_TYPE] = {
        .name = "system_icmp_in_type",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP messages received by type.",
    },
    [FAM_ICMP_OUT_TYPE] = {
        .name = "system_icmp_out_type",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ICMP messages sent by type.",
    },
    [FAM_TCP_RTO_ALGORITHM] = {
        .name = "system_tcp_rto_algorithm",
        .type = METRIC_TYPE_GAUGE,
        .help = "The algorithm used to determine the timeout value used for "
                "retransmitting unacknowledged octets.",
    },
    [FAM_TCP_RTO_MINIMUM] = { // seconds XXX
        .name = "system_tcp_rto_minimum",
        .type = METRIC_TYPE_GAUGE,
        .help = "The minimum value permitted by a TCP implementation for "
                "the retransmission timeout, measured in milliseconds."
    },
    [FAM_TCP_RTO_MAXIMUM] = { // seconds XXX
        .name = "system_tcp_rto_maximum",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum value permitted by a TCP implementation for "
                "the retransmission timeout, measured in milliseconds."
    },
    [FAM_TCP_MAXIMUM_CONNECTIONS] = {
        .name = "system_tcp_maximum_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "The limit on the total number of TCP connections the entity can support.",
    },
    [FAM_TCP_ACTIVE_OPENS] = {
        .name = "system_tcp_active_opens",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times TCP connections have made a direct transition "
                "to the SYN-SENT state from the CLOSED state.",
    },
    [FAM_TCP_PASSIVE_OPENS] = {
        .name = "system_tcp_passive_opens",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times TCP connections have made a direct transition "
                "to the SYN-RCVD state from the LISTEN state.",
    },
    [FAM_TCP_ATTEMPT_FAILS] = {
        .name = "system_tcp_attempt_fails",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times TCP connections have made a direct transition "
                "to the CLOSED state from either the SYN-SENT state or the SYN-RCVD state, "
                "plus the number of times TCP connections have made a direct transition "
                "to the LISTEN state from the SYN-RCVD state.",
    },
    [FAM_TCP_ESTABLISHED_RESETS] = {
        .name = "system_tcp_established_resets",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times TCP connections have made a direct transition "
                "to the CLOSED state from either the ESTABLISHED state or the CLOSE-WAIT state.",
    },
    [FAM_TCP_ESTABLISHED] = {
        .name = "system_tcp_established",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of TCP connections for which the current state is "
                "either ESTABLISHED or CLOSE- WAIT."
    },
    [FAM_TCP_IN_SEGMENTS] = {
        .name = "system_tcp_in_segments",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of segments received, including those received in error.",
    },
    [FAM_TCP_OUT_SEGMENTS] = {
        .name = "system_tcp_out_segments",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of segments sent, including those on current connections "
                 "but excluding those containing only retransmitted octets.",
    },
    [FAM_TCP_RETRANS_SEGMENTS] = {
        .name = "system_tcp_retrans_segments",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of segments retransmitted.",
    },
    [FAM_TCP_IN_ERRORS] = {
        .name = "system_tcp_in_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of segments received in error (e.g., bad TCP checksums).",
    },
    [FAM_TCP_OUT_RSTS] = {
        .name = "system_tcp_out_rsts",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of TCP segments sent containing the RST flag.",
    },
    [FAM_TCP_IN_CSUM_ERRORS] = {
        .name = "system_tcp_in_csum_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of TCP packets received with an incorrect checksum.",
    },
    [FAM_UDP_IN_DATAGRAMS] = {
        .name = "system_udp_in_datagrams",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of UDP datagrams delivered to UDP users.",
    },
    [FAM_UDP_NO_PORTS] = {
        .name = "system_udp_no_ports",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of received UDP datagrams for which "
                "there was no application at the destination port.",
    },
    [FAM_UDP_IN_ERRORS] = {
        .name = "system_udp_in_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of received UDP datagrams that could not be delivered "
                "for reasons other than the lack of an application at the destination port.",
    },
    [FAM_UDP_OUT_DATAGRAMS] = {
        .name = "system_udp_out_datagrams",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of UDP datagrams sent from this entity."
    },
    [FAM_UDP_RECV_BUFFER_ERRORS] = {
        .name = "system_udp_recv_buffer_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased when memory cannot be allocated to process an incoming UDP packet."
    },
    [FAM_UDP_SEND_BUFFER_ERRORS] = {
        .name = "system_udp_send_buffer_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased when memory cannot be allocated to send an UDP packet."
    },
    [FAM_UDP_IN_CSUM_ERRORS] = {
        .name = "system_udp_in_csum_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased when a received UDP packet has an invalid checksum."
    },
    [FAM_UDP_IGNORED_MULTI] = {
        .name = "system_udp_ignored_multi",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDP_MEMORY_ERRORS] = {
        .name = "system_udp_memory_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDPLITE_IN_DATAGRAMS] = {
        .name = "system_udplite_in_datagrams",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of UDP-Lite datagrams that were delivered to UDP-Lite users.",
    },
    [FAM_UDPLITE_NO_PORTS] = {
        .name = "system_udplite_no_ports",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of received UDP-Lite datagrams for which "
                "there was no listener at the destination port.",
    },
    [FAM_UDPLITE_IN_ERRORS] = {
        .name = "system_udplite_in_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of received UDP-Lite datagrams that could not be delivered "
                "for reasons other than the lack of an application at the destination port.",
    },
    [FAM_UDPLITE_OUT_DATAGRAMS] = {
        .name = "system_udplite_out_datagrams",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of UDP-Lite datagrams sent from this entity.",
    },
    [FAM_UDPLITE_RECV_BUFFER_ERRORS] = {
        .name = "system_udplite_recv_buffer_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased when memory cannot be allocated to process an incoming UDP-Lite packet.",
    },
    [FAM_UDPLITE_SEND_BUFFER_ERRORS] = {
        .name = "system_udplite_send_buffer_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased when memory cannot be allocated to send an UDP-Lite packet."
    },
    [FAM_UDPLITE_IN_CSUM_ERRORS] = {
        .name = "system_udplite_in_csum_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Increased when a received UDP-Lite packet has an invalid checksum."
    },
    [FAM_UDPLITE_IGNORED_MULTI] = {
        .name = "system_udplite_ignored_multi",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_UDPLITE_MEMORY_ERRORS] = {
        .name = "system_udplite_memory_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
};

struct snmp_metric {
    char *key;
    uint64_t flags;
    int fam;
};

const struct snmp_metric *snmp_get_key (register const char *str, register size_t len);

static char *path_proc_snmp;
static bool proc_snmp_found = false;

int snmp_init(void)
{
    path_proc_snmp = plugin_procpath("net/snmp");
    if (path_proc_snmp == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    if (access(path_proc_snmp, R_OK) != 0) {
        PLUGIN_ERROR("Cannot access %s: %s", path_proc_snmp, STRERRNO);
        return -1;
    }

    proc_snmp_found = true;

    return 0;
}

int snmp_shutdown(void)
{
    free(path_proc_snmp);
    return 0;
}

int snmp_read(uint64_t flags, plugin_filter_t *filter)
{
    if (!proc_snmp_found)
        return 0;

    if (!(flags & (COLLECT_ICMP | COLLECT_IP | COLLECT_TCP | COLLECT_UDP | COLLECT_UDPLITE)))
        return 0;

    FILE *fh = fopen(path_proc_snmp, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("fopen (%s) failed: %s.", path_proc_snmp, STRERRNO);
        return -1;
    }

    char key_buffer[4096];
    char value_buffer[4096];
    char *key_fields[256];
    char *value_fields[256];

    int status = -1;
    while (true) {
        clearerr(fh);
        char *key_ptr = fgets(key_buffer, sizeof(key_buffer), fh);
        if (key_ptr == NULL) {
            if (feof(fh) != 0) {
                status = 0;
                break;
            } else if (ferror(fh) != 0) {
                PLUGIN_ERROR("Reading from %s failed.", path_proc_snmp);
                break;
            } else {
                PLUGIN_ERROR("fgets failed for an unknown reason.");
                break;
            }
        }

        char *value_ptr = fgets(value_buffer, sizeof(value_buffer), fh);
        if (value_ptr == NULL) {
            PLUGIN_ERROR("read_file (%s): Could not read values line.", path_proc_snmp);
            break;
        }

        key_ptr = strchr(key_buffer, ':');
        if (key_ptr == NULL) {
            PLUGIN_ERROR("Could not find protocol name in keys line.");
            break;
        }
        *key_ptr = 0;
        key_ptr++;

        value_ptr = strchr(value_buffer, ':');
        if (value_ptr == NULL) {
            PLUGIN_ERROR("Could not find protocol name in values line.");
            break;
        }
        *value_ptr = 0;
        value_ptr++;

        if (strcmp(key_buffer, value_buffer) != 0) {
            PLUGIN_ERROR("Protocol names in keys and values lines don't match: `%s' vs. `%s'.",
                         key_buffer, value_buffer);
            break;
        }

        int key_fields_num = strsplit(key_ptr, key_fields, STATIC_ARRAY_SIZE(key_fields));
        int value_fields_num = strsplit(value_ptr, value_fields, STATIC_ARRAY_SIZE(value_fields));

        if (key_fields_num != value_fields_num) {
            PLUGIN_ERROR("Number of fields in keys and values lines don't match: %i vs %i.",
                         key_fields_num, value_fields_num);
            break;
        }

        char name[256];
        size_t name_key_len = strlen(key_buffer);

        if (name_key_len >= sizeof(name))
            continue;

        size_t name_len = sizeof(name) - name_key_len;
        sstrncpy(name, key_buffer, sizeof(name));

        for (int i = 0; i < key_fields_num; i++) {
            sstrncpy(name + name_key_len, key_fields[i], name_len);

            const struct snmp_metric *m = snmp_get_key (name, strlen(name));
            if (m == NULL) {
                if (strcmp("IcmpMsg", key_buffer) == 0) {
                    if (!(flags & COLLECT_ICMP))
                        continue;

                    if (strncmp("InType", key_fields[i], strlen("InType")) == 0) {
                        char *type = key_fields[i] + strlen("InType");
                        uint64_t value = atol(value_fields[i]);
                        metric_family_append(&fams[FAM_ICMP_IN_TYPE], VALUE_COUNTER(value), NULL,
                                             &(label_pair_const_t){.name="type", .value=type},
                                             NULL);
                    } else if (strncmp("OutType", key_fields[i], strlen("OutType")) == 0) {
                        char *type = key_fields[i] + strlen("OutType");
                        uint64_t value = atol(value_fields[i]);
                        metric_family_append(&fams[FAM_ICMP_OUT_TYPE], VALUE_COUNTER(value), NULL,
                                             &(label_pair_const_t){.name="type", .value=type},
                                             NULL);
                    }
                }

                continue;
            }

            if (!(m->flags & flags))
                continue;

            if (fams[m->fam].type == METRIC_TYPE_GAUGE) {
                double value = atof(value_fields[i]);
                if (!isfinite(value))
                    continue;
                metric_family_append(&fams[m->fam], VALUE_GAUGE(value), NULL, NULL);
            } else if (fams[m->fam].type == METRIC_TYPE_COUNTER) {
                uint64_t value = atol(value_fields[i]);
                metric_family_append(&fams[m->fam], VALUE_COUNTER(value), NULL, NULL);
            }
        }
    }

    fclose(fh);

    plugin_dispatch_metric_family_array_filtered(fams, FAM_SNMP_MAX, filter, 0);

    return status;
}
