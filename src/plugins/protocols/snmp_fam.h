/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

enum {
    FAM_IP_FORWARDING,
    FAM_IP_DEFAULT_TTL,
    FAM_IP_IN_RECEIVES,
    FAM_IP_IN_HEADER_ERRORS,
    FAM_IP_IN_ADDRESS_ERRORS,
    FAM_IP_FORWARD_DATAGRAMS,
    FAM_IP_IN_UNKNOWN_PROTOCOL,
    FAM_IP_IN_DISCARDS,
    FAM_IP_IN_DELIVERS,
    FAM_IP_OUT_REQUESTS,
    FAM_IP_OUT_DISCARDS,
    FAM_IP_OUT_NO_ROUTES,
    FAM_IP_REASSEMBLY_TIMEOUT,
    FAM_IP_REASSEMBLY_REQUIRED,
    FAM_IP_REASSEMBLY_OKS,
    FAM_IP_REASSEMBLY_FAILS,
    FAM_IP_FRAGMENTED_OKS,
    FAM_IP_FRAGMENTED_FAILS,
    FAM_IP_FRAGMENTED_CREATES,
    FAM_ICMP_IN_MESSAGES,
    FAM_ICMP_IN_ERRORS,
    FAM_ICMP_IN_CSUM_ERRORS,
    FAM_ICMP_IN_DESTINATION_UNREACHABLE,
    FAM_ICMP_IN_TIME_EXCEEDED,
    FAM_ICMP_IN_PARAMETER_PROBLEM,
    FAM_ICMP_IN_SOURCE_QUENCH,
    FAM_ICMP_IN_REDIRECT,
    FAM_ICMP_IN_ECHO_REQUEST,
    FAM_ICMP_IN_ECHO_REPLY,
    FAM_ICMP_IN_TIMESTAMP_REQUEST,
    FAM_ICMP_IN_TIMESTAMP_REPLY,
    FAM_ICMP_IN_ADDRESS_MASK_REQUEST,
    FAM_ICMP_IN_ADDRESS_MASK_REPLY,
    FAM_ICMP_OUT_MESSAGES,
    FAM_ICMP_OUT_ERRORS,
    FAM_ICMP_OUT_DESTINATION_UNREACHABLE,
    FAM_ICMP_OUT_TIME_EXCEEDED,
    FAM_ICMP_OUT_PARAMETER_PROBLEM,
    FAM_ICMP_OUT_SOURCE_QUENCH,
    FAM_ICMP_OUT_REDIRECT,
    FAM_ICMP_OUT_ECHO_REQUEST,
    FAM_ICMP_OUT_ECHO_REPLY,
    FAM_ICMP_OUT_TIMESTAMP_REQUEST,
    FAM_ICMP_OUT_TIMESTAMP_REPLY,
    FAM_ICMP_OUT_ADDRESS_MASK_REQUEST,
    FAM_ICMP_OUT_ADDRESS_MASK_REPLY,
    FAM_ICMP_IN_TYPE,
    FAM_ICMP_OUT_TYPE,
    FAM_TCP_RTO_ALGORITHM,
    FAM_TCP_RTO_MINIMUM,
    FAM_TCP_RTO_MAXIMUM,
    FAM_TCP_MAXIMUM_CONNECTIONS,
    FAM_TCP_ACTIVE_OPENS,
    FAM_TCP_PASSIVE_OPENS,
    FAM_TCP_ATTEMPT_FAILS,
    FAM_TCP_ESTABLISHED_RESETS,
    FAM_TCP_ESTABLISHED,
    FAM_TCP_IN_SEGMENTS,
    FAM_TCP_OUT_SEGMENTS,
    FAM_TCP_RETRANS_SEGMENTS,
    FAM_TCP_IN_ERRORS,
    FAM_TCP_OUT_RSTS,
    FAM_TCP_IN_CSUM_ERRORS,
    FAM_UDP_IN_DATAGRAMS,
    FAM_UDP_NO_PORTS,
    FAM_UDP_IN_ERRORS,
    FAM_UDP_OUT_DATAGRAMS,
    FAM_UDP_RECV_BUFFER_ERRORS,
    FAM_UDP_SEND_BUFFER_ERRORS,
    FAM_UDP_IN_CSUM_ERRORS,
    FAM_UDP_IGNORED_MULTI,
    FAM_UDP_MEMORY_ERRORS,
    FAM_UDPLITE_IN_DATAGRAMS,
    FAM_UDPLITE_NO_PORTS,
    FAM_UDPLITE_IN_ERRORS,
    FAM_UDPLITE_OUT_DATAGRAMS,
    FAM_UDPLITE_RECV_BUFFER_ERRORS,
    FAM_UDPLITE_SEND_BUFFER_ERRORS,
    FAM_UDPLITE_IN_CSUM_ERRORS,
    FAM_UDPLITE_IGNORED_MULTI,
    FAM_UDPLITE_MEMORY_ERRORS,
    FAM_SNMP_MAX,
};