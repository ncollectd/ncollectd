// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2007-2008 C-Ware, Inc.
// SPDX-FileCopyrightText: Copyright (C) 2008 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Luke Heberling <lukeh at c-ware.com>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include <sys/un.h>
#include "libutils/common.h"

// #define RECURSOR_SOCKET LOCALSTATEDIR "/run/pdns_recursor.controlsocket"
#define RECURSOR_SOCKET LOCALSTATEDIR "/run/pdns-recursor/pdns_recursor.controlsocket"
#define PDNS_LOCAL_SOCKPATH LOCALSTATEDIR "/run/" PACKAGE_NAME "-recursor"

enum {
    FAM_RECURSOR_ALL_OUTQUERIES,
    FAM_RECURSOR_ANSWERS,
    FAM_RECURSOR_AUTH4_ANSWERS,
    FAM_RECURSOR_AUTH6_ANSWERS,
    FAM_RECURSOR_AUTH_ANSWERS,
    FAM_RECURSOR_AUTH_ZONE_QUERIES,
    FAM_RECURSOR_CACHE_BYTES,
    FAM_RECURSOR_CACHE_ENTRIES,
    FAM_RECURSOR_CACHE_HITS,
    FAM_RECURSOR_CACHE_MISSES,
    FAM_RECURSOR_CASE_MISMATCHES,
    FAM_RECURSOR_CHAIN_RESENDS,
    FAM_RECURSOR_CLIENT_PARSE_ERRORS,
    FAM_RECURSOR_CONCURRENT_QUERIES,
    FAM_RECURSOR_THREAD_CPU_MSEC,
    FAM_RECURSOR_ZONE_DISALLOWED_NOTIFY,
    FAM_RECURSOR_DNSSEC_AUTHENTIC_DATA_QUERIES,
    FAM_RECURSOR_DNSSEC_CHECK_DISABLED_QUERIES,
    FAM_RECURSOR_DNSSEC_QUERIES,
    FAM_RECURSOR_DNSSEC_RESULT_BOGUS,
    FAM_RECURSOR_DNSSEC_RESULT_BOGUS_REASON,
    FAM_RECURSOR_DNSSEC_RESULT_INDETERMINATE,
    FAM_RECURSOR_DNSSEC_RESULT_INSECURE,
    FAM_RECURSOR_DNSSEC_RESULT_NTA,
    FAM_RECURSOR_DNSSEC_RESULT_SECURE,
    FAM_RECURSOR_X_DNSSEC_RESULT_BOGUS,
    FAM_RECURSOR_X_DNSSEC_RESULT_BOGUS_REASON,
    FAM_RECURSOR_X_DNSSEC_RESULT_INDETERMINATE,
    FAM_RECURSOR_X_DNSSEC_RESULT_INSECURE,
    FAM_RECURSOR_X_DNSSEC_RESULT_NTA,
    FAM_RECURSOR_X_DNSSEC_RESULT_SECURE,
    FAM_RECURSOR_DNSSEC_VALIDATIONS,
    FAM_RECURSOR_DONT_OUTQUERIES,
    FAM_RECURSOR_QNAME_MIN_FALLBACK_SUCCESS,
    FAM_RECURSOR_ECS_QUERIES,
    FAM_RECURSOR_ECS_RESPONSES,
    FAM_RECURSOR_EDNS_PING_MATCHES,
    FAM_RECURSOR_EDNS_PING_MISMATCHES,
    FAM_RECURSOR_FAILED_HOST_ENTRIES,
    FAM_RECURSOR_NON_RESOLVING_NAMESERVER_ENTRIES,
    FAM_RECURSOR_IGNORED_PACKETS,
    FAM_RECURSOR_IPV6_OUTQUERIES,
    FAM_RECURSOR_IPV6_QUESTIONS,
    FAM_RECURSOR_MALLOC_BYTES,
    FAM_RECURSOR_MAX_CACHE_ENTRIES,
    FAM_RECURSOR_MAX_PACKETCACHE_ENTRIES,
    FAM_RECURSOR_MAX_MTHREAD_STACK,
    FAM_RECURSOR_NEGCACHE_ENTRIES,
    FAM_RECURSOR_NO_PACKET_ERROR,
    FAM_RECURSOR_NOD_LOOKUPS_DROPPED_OVERSIZE,
    FAM_RECURSOR_NOEDNS_OUTQUERIES,
    FAM_RECURSOR_NOERROR_ANSWERS,
    FAM_RECURSOR_NOPING_OUTQUERIES,
    FAM_RECURSOR_NSSET_INVALIDATIONS,
    FAM_RECURSOR_NSSPEEDS_ENTRIES,
    FAM_RECURSOR_NXDOMAIN_ANSWERS,
    FAM_RECURSOR_OUTGOING_TIMEOUTS,
    FAM_RECURSOR_OUTGOING4_TIMEOUTS,
    FAM_RECURSOR_OUTGOING6_TIMEOUTS,
    FAM_RECURSOR_OVER_CAPACITY_DROPS,
    FAM_RECURSOR_PACKETCACHE_BYTES,
    FAM_RECURSOR_PACKETCACHE_ENTRIES,
    FAM_RECURSOR_PACKETCACHE_HITS,
    FAM_RECURSOR_PACKETCACHE_MISSES,
    FAM_RECURSOR_POLICY_DROPS,
    FAM_RECURSOR_POLICY_RESULT,
    FAM_RECURSOR_QA_LATENCY,
    FAM_RECURSOR_QUERY_PIPE_FULL_DROPS,
    FAM_RECURSOR_QUESTIONS,
    FAM_RECURSOR_REBALANCED_QUERIES,
    FAM_RECURSOR_RESOURCE_LIMITS,
    FAM_RECURSOR_SECURITY_STATUS,
    FAM_RECURSOR_SERVER_PARSE_ERRORS,
    FAM_RECURSOR_SERVFAIL_ANSWERS,
    FAM_RECURSOR_SPOOF_PREVENTS,
    FAM_RECURSOR_SYS_MSEC,
    FAM_RECURSOR_TCP_CLIENT_OVERFLOW,
    FAM_RECURSOR_TCP_CLIENTS,
    FAM_RECURSOR_TCP_OUTQUERIES,
    FAM_RECURSOR_TCP_QUESTIONS,
    FAM_RECURSOR_THROTTLE_ENTRIES,
    FAM_RECURSOR_THROTTLED_OUT,
    FAM_RECURSOR_THROTTLED_OUTQUERIES,
    FAM_RECURSOR_TOO_OLD_DROPS,
    FAM_RECURSOR_TRUNCATED_DROPS,
    FAM_RECURSOR_EMPTY_QUERIES,
    FAM_RECURSOR_UNAUTHORIZED_TCP,
    FAM_RECURSOR_UNAUTHORIZED_UDP,
    FAM_RECURSOR_SOURCE_DISALLOWED_NOTIFY,
    FAM_RECURSOR_UNEXPECTED_PACKETS,
    FAM_RECURSOR_UNREACHABLES,
    FAM_RECURSOR_UPTIME,
    FAM_RECURSOR_USER_MSEC,
    FAM_RECURSOR_VARIABLE_RESPONSES,
    FAM_RECURSOR_X_OUR_LATENCY,
    FAM_RECURSOR_X_OUR_TIME,
    FAM_RECURSOR_FD_USAGE,
    FAM_RECURSOR_REAL_MEMORY_USAGE,
    FAM_RECURSOR_UDP_IN_ERRORS,
    FAM_RECURSOR_UDP_NOPORT_ERRORS,
    FAM_RECURSOR_UDP_RECVBUF_ERRORS,
    FAM_RECURSOR_UDP_SNDBUF_ERRORS,
    FAM_RECURSOR_UDP_IN_CSUM_ERRORS,
    FAM_RECURSOR_UDP6_IN_ERRORS,
    FAM_RECURSOR_UDP6_NOPORT_ERRORS,
    FAM_RECURSOR_UDP6_RECVBUF_ERRORS,
    FAM_RECURSOR_UDP6_SNDBUF_ERRORS,
    FAM_RECURSOR_UDP6_IN_CSUM_ERRORS,
    FAM_RECURSOR_CPU_IOWAIT,
    FAM_RECURSOR_CPU_STEAL,
    FAM_RECURSOR_PROXY_PROTOCOL_INVALID,
    FAM_RECURSOR_RECORD_CACHE_ACQUIRED,
    FAM_RECURSOR_RECORD_CACHE_CONTENDED,
    FAM_RECURSOR_PACKETCACHE_ACQUIRED,
    FAM_RECURSOR_PACKETCACHE_CONTENDED,
    FAM_RECURSOR_TASKQUEUE_EXPIRED,
    FAM_RECURSOR_TASKQUEUE_PUSHED,
    FAM_RECURSOR_TASKQUEUE_SIZE,
    FAM_RECURSOR_DOT_OUTQUERIES,
    FAM_RECURSOR_DNS64_PREFIX_ANSWERS,
    FAM_RECURSOR_AGGRESSIVE_NSEC_CACHE_ENTRIES,
    FAM_RECURSOR_AGGRESSIVE_NSEC_CACHE_NSEC_HITS,
    FAM_RECURSOR_AGGRESSIVE_NSEC_CACHE_NSEC_WC_HITS,
    FAM_RECURSOR_AGGRESSIVE_NSEC_CACHE_NSEC3_HITS,
    FAM_RECURSOR_AGGRESSIVE_NSEC_CACHE_NSEC3_WC_HITS,
    FAM_RECURSOR_ALMOST_EXPIRED_PUSHED,
    FAM_RECURSOR_ALMOST_EXPIRED_RUN,
    FAM_RECURSOR_ALMOST_EXPIRED_EXCEPTIONS,
    FAM_RECURSOR_IDLE_TCPOUT_CONNECTIONS,
    FAM_RECURSOR_MAINTENANCE_USEC,
    FAM_RECURSOR_MAINTENANCE_CALLS,
    FAM_RECURSOR_NOD_EVENTS,
    FAM_RECURSOR_UDR_EVENTS,
    FAM_RECURSOR_MAX,
};

static metric_family_t fams_recursor[FAM_RECURSOR_MAX] = {
    [FAM_RECURSOR_ALL_OUTQUERIES] = {
        .name = "recursor_all_outqueries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of outgoing UDP queries since starting.",
    },
    [FAM_RECURSOR_ANSWERS] = {
        .name = "recursor_answers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries answered by response time.",
    },
    [FAM_RECURSOR_AUTH4_ANSWERS] = {
        .name = "recursor_auth4_answers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries answered by authoritatives over IPv4 by response time.",
    },
    [FAM_RECURSOR_AUTH6_ANSWERS] = {
        .name = "recursor_auth6_answers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries answered by authoritatives over IPv6 by response time.",
    },
    [FAM_RECURSOR_AUTH_ANSWERS] = {
        .name = "recursor_auth_answers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of RCodes returned by authoritative servers",
    },
    [FAM_RECURSOR_AUTH_ZONE_QUERIES] = {
        .name = "recursor_auth-zone-queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries to locally hosted authoritative zones "
                "('setting-auth-zones') since starting.",
    },
    [FAM_RECURSOR_CACHE_BYTES] = {
        .name = "recursor_cache_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size of the cache in bytes.",
    },
    [FAM_RECURSOR_CACHE_ENTRIES] = {
        .name = "recursor_cache_entries",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of entries in the cache.",
    },
    [FAM_RECURSOR_CACHE_HITS] = {
        .name = "recursor_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of of cache hits since starting, "
                "this does **not** include hits that got answered from the packet-cache.",
    },
    [FAM_RECURSOR_CACHE_MISSES] = {
        .name = "recursor_cache_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of cache misses since starting.",
    },
    [FAM_RECURSOR_CASE_MISMATCHES] = {
        .name = "recursor_case_mismatches",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of mismatches in character case since starting.",
    },
    [FAM_RECURSOR_CHAIN_RESENDS] = {
        .name = "recursor_chain_resends",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries chained to existing outstanding.",
    },
    [FAM_RECURSOR_CLIENT_PARSE_ERRORS] = {
        .name = "recursor_client_parse_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of client packets that could not be parsed.",
    },
    [FAM_RECURSOR_CONCURRENT_QUERIES] = {
        .name = "recursor_concurrent_queries",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of MThreads currently running.",
    },
    [FAM_RECURSOR_THREAD_CPU_MSEC] = {
        .name = "recursor_thread_cpu_msec",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of milliseconds spent in thread.",
    },
    [FAM_RECURSOR_ZONE_DISALLOWED_NOTIFY] = {
        .name = "recursor_zone_disallowed_notify",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of NOTIFY operations denied because of allow-notify-for restrictions.",
    },
    [FAM_RECURSOR_DNSSEC_AUTHENTIC_DATA_QUERIES] = {
        .name = "recursor_dnssec_authentic_data_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries received with the AD bit set.",
    },
    [FAM_RECURSOR_DNSSEC_CHECK_DISABLED_QUERIES] = {
        .name = "recursor_dnssec_check_disabled_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries received with the CD bit set.",
    },
    [FAM_RECURSOR_DNSSEC_QUERIES] = {
        .name = "recursor_dnssec_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries received with the DO bit set.",
    },
    [FAM_RECURSOR_DNSSEC_RESULT_BOGUS] = {
        .name = "recursor_dnssec_result_bogus",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses sent, packet-cache hits excluded, "
                "that were in the Bogus state.",
    },
    [FAM_RECURSOR_DNSSEC_RESULT_BOGUS_REASON] = {
        .name = "recursor_dnssec_result_bogus_reason",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses sent, packet-cache hits excluded, "
                "that were in the Bogus state by reason.",
    },
    [FAM_RECURSOR_DNSSEC_RESULT_INDETERMINATE] = {
        .name = "recursor_dnssec_result_indeterminate",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses sent, packet-cache hits excluded, "
                "that were in the Indeterminate state.",
    },
    [FAM_RECURSOR_DNSSEC_RESULT_INSECURE] = {
        .name = "recursor_dnssec_result_insecure",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses sent, packet-cache hits excluded, "
                "that were in the Insecure state.",
    },
    [FAM_RECURSOR_DNSSEC_RESULT_NTA] = {
        .name = "recursor_dnssec_result_nta",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses sent, packet-cache hits excluded, "
                "that were in the (negative trust anchor) state.",
    },
    [FAM_RECURSOR_DNSSEC_RESULT_SECURE] = {
        .name = "recursor_dnssec_result_secure",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses sent, packet-cache hits excluded, "
                "that were in the Secure state.",
    },
    [FAM_RECURSOR_X_DNSSEC_RESULT_BOGUS] = {
        .name = "recursor_x_dnssec_result_bogus",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses sent, packet-cache hits excluded, "
                "that were in the Bogus state.",
    },
    [FAM_RECURSOR_X_DNSSEC_RESULT_BOGUS_REASON] = {
        .name = "recursor_x_dnssec_result_bogus_reason",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses sent, packet-cache hits excluded, "
                "that were in the Bogus state by reason.",
    },
    [FAM_RECURSOR_X_DNSSEC_RESULT_INDETERMINATE] = {
        .name = "recursor_x_dnssec_result_indeterminate",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses sent, packet-cache hits excluded, "
                "that were in the Indeterminate state.",
    },
    [FAM_RECURSOR_X_DNSSEC_RESULT_INSECURE] = {
        .name = "recursor_x_dnssec_result_insecure",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses sent, packet-cache hits excluded, "
                "that were in the Insecure state.",
    },
    [FAM_RECURSOR_X_DNSSEC_RESULT_NTA] = {
        .name = "recursor_x_dnssec_result_nta",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses sent, packet-cache hits excluded, "
                "that were in the (negative trust anchor) state.",
    },
    [FAM_RECURSOR_X_DNSSEC_RESULT_SECURE] = {
        .name = "recursor_x_dnssec_result_secure",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses sent, packet-cache hits excluded, "
                "that were in the Secure state.",
    },
    [FAM_RECURSOR_DNSSEC_VALIDATIONS] = {
        .name = "recursor_dnssec_validations",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses sent, packet-cache hits excluded, "
                "for which a DNSSEC validation was requested by "
                "either the client or the configuration.",
    },
    [FAM_RECURSOR_DONT_OUTQUERIES] = {
        .name = "recursor_dont_outqueries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of outgoing queries dropped because of 'setting-dont-query' setting.",
    },
    [FAM_RECURSOR_QNAME_MIN_FALLBACK_SUCCESS] = {
        .name = "recursor_qname_min_fallback_success",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of successful queries due to fallback mechanism "
                "within 'qname-minimization' setting.",
    },
    [FAM_RECURSOR_ECS_QUERIES] = {
        .name = "recursor_ecs_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of outgoing queries adorned with an EDNS Client Subnet option.",
    },
    [FAM_RECURSOR_ECS_RESPONSES] = {
        .name = "recursor_ecs_responses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses received from authoritative servers "
                "with an EDNS Client Subnet option we used.",
    },
    [FAM_RECURSOR_EDNS_PING_MATCHES] = {
        .name = "recursor_edns_ping_matches",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of servers that sent a valid EDNS PING response.",
    },
    [FAM_RECURSOR_EDNS_PING_MISMATCHES] = {
        .name = "recursor_edns_ping_mismatches",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of servers that sent an invalid EDNS PING response.",
    },
    [FAM_RECURSOR_FAILED_HOST_ENTRIES] = {
        .name = "recursor_failed_host_entries",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of entries in the failed NS cache.",
    },
    [FAM_RECURSOR_NON_RESOLVING_NAMESERVER_ENTRIES] = {
        .name = "recursor_non_resolving_nameserver_entries",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of entries in the non-resolving NS name cache.",
    },
    [FAM_RECURSOR_IGNORED_PACKETS] = {
        .name = "recursor_ignored_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of non-query packets received on server sockets "
                "that should only get query packets.",
    },
    [FAM_RECURSOR_IPV6_OUTQUERIES] = {
        .name = "recursor_ipv6_outqueries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of outgoing queries over IPv6.",
    },
    [FAM_RECURSOR_IPV6_QUESTIONS] = {
        .name = "recursor_ipv6_questions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of end-user initiated queries with the RD bit set, "
                "received over IPv6 UDP.",
    },
    [FAM_RECURSOR_MALLOC_BYTES] = {
        .name = "recursor_malloc_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes allocated by the process (broken, always returns 0).",
    },
    [FAM_RECURSOR_MAX_CACHE_ENTRIES] = {
        .name = "recursor_max_cache_entries",
        .type = METRIC_TYPE_GAUGE,
        .help = "Currently configured maximum number of cache entries.",
    },
    [FAM_RECURSOR_MAX_PACKETCACHE_ENTRIES] = {
        .name = "recursor_max_packetcache_entries",
        .type = METRIC_TYPE_GAUGE,
        .help = "Currently configured maximum number of packet cache entries.",
    },
    [FAM_RECURSOR_MAX_MTHREAD_STACK] = {
        .name = "recursor_max_mthread_stack",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum amount of thread stack ever used.",
    },
    [FAM_RECURSOR_NEGCACHE_ENTRIES] = {
        .name = "recursor_negcache_entries",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of entries in the negative answer cache.",
    },
    [FAM_RECURSOR_NO_PACKET_ERROR] = {
        .name = "recursor_no_packet_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of erroneous received packets.",
    },
    [FAM_RECURSOR_NOD_LOOKUPS_DROPPED_OVERSIZE] = {
        .name = "recursor_nod_lookups_dropped_oversize",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of NOD lookups dropped because they would exceed the maximum name length.",
    },
    [FAM_RECURSOR_NOEDNS_OUTQUERIES] = {
        .name = "recursor_noedns_outqueries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries sent out without EDNS.",
    },
    [FAM_RECURSOR_NOERROR_ANSWERS] = {
        .name = "recursor_noerror_answers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of NOERROR answers since starting.",
    },
    [FAM_RECURSOR_NOPING_OUTQUERIES] = {
        .name = "recursor_noping_outqueries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries sent out without ENDS PING.",
    },
    [FAM_RECURSOR_NSSET_INVALIDATIONS] = {
        .name = "recursor_nsset_invalidations",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times an nsset was dropped because it no longer worked.",
    },
    [FAM_RECURSOR_NSSPEEDS_ENTRIES] = {
        .name = "recursor_nsspeeds_entries",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of entries in the NS speeds map.",
    },
    [FAM_RECURSOR_NXDOMAIN_ANSWERS] = {
        .name = "recursor_nxdomain_answers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of NXDOMAIN answers since starting.",
    },
    [FAM_RECURSOR_OUTGOING_TIMEOUTS] = {
        .name = "recursor_outgoing_timeouts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of timeouts on outgoing UDP queries since starting.",
    },
    [FAM_RECURSOR_OUTGOING4_TIMEOUTS] = {
        .name = "recursor_outgoing4_timeouts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of timeouts on outgoing UDP IPv4 queries since starting.",
    },
    [FAM_RECURSOR_OUTGOING6_TIMEOUTS] = {
        .name = "recursor_outgoing6_timeouts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of timeouts on outgoing UDP IPv6 queries since starting.",
    },
    [FAM_RECURSOR_OVER_CAPACITY_DROPS] = {
        .name = "recursor_over_capacity_drops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of questions dropped because over maximum concurrent query limit.",
    },
    [FAM_RECURSOR_PACKETCACHE_BYTES] = {
        .name = "recursor_packetcache_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size of the packet cache in bytes.",
    },
    [FAM_RECURSOR_PACKETCACHE_ENTRIES] = {
        .name = "recursor_packetcache_entries",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of packet cache entries.",
    },
    [FAM_RECURSOR_PACKETCACHE_HITS] = {
        .name = "recursor_packetcache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packet cache hits.",
    },
    [FAM_RECURSOR_PACKETCACHE_MISSES] = {
        .name = "recursor_packetcache_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packet cache misses.",
    },
    [FAM_RECURSOR_POLICY_DROPS] = {
        .name = "recursor_policy_drops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packets dropped because of (Lua) policy decision.",
    },
    [FAM_RECURSOR_POLICY_RESULT] = {
        .name = "recursor_policy_result",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packets by the result of the RPZ/filter engine.",
    },
    [FAM_RECURSOR_QA_LATENCY] = {
        .name = "recursor_qa_latency",
        .type = METRIC_TYPE_GAUGE,
        .help = "Shows the current latency average, in microseconds, "
                "exponentially weighted over past 'latency-statistic-size' packets.",
    },
    [FAM_RECURSOR_QUERY_PIPE_FULL_DROPS] = {
        .name = "recursor_query_pipe_full_drops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of questions dropped because the query distribution pipe was full.",
    },
    [FAM_RECURSOR_QUESTIONS] = {
        .name = "recursor_questions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Counts all end-user initiated queries with the RD bit set.",
    },
    [FAM_RECURSOR_REBALANCED_QUERIES] = {
        .name = "recursor_rebalanced_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries balanced to a different worker thread because "
                "the first selected one was above the target load configured "
                "with 'distribution-load-factor'.",
    },
    [FAM_RECURSOR_RESOURCE_LIMITS] = {
        .name = "recursor_resource_limits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that could not be performed because of resource limits.",
    },
    [FAM_RECURSOR_SECURITY_STATUS] = {
        .name = "recursor_security_status",
        .type = METRIC_TYPE_GAUGE,
        .help = "security status based on 'securitypolling'.",
    },
    [FAM_RECURSOR_SERVER_PARSE_ERRORS] = {
        .name = "recursor_server_parse_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of server replied packets that could not be parsed.",
    },
    [FAM_RECURSOR_SERVFAIL_ANSWERS] = {
        .name = "recursor_servfail_answers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of SERVFAIL answers since starting.",
    },
    [FAM_RECURSOR_SPOOF_PREVENTS] = {
        .name = "recursor_spoof_prevents",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times PowerDNS considered itself spoofed, and dropped the data.",
    },
    [FAM_RECURSOR_SYS_MSEC] = {
        .name = "recursor_sys_msec",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of CPU milliseconds spent in 'system' mode.",
    },
    [FAM_RECURSOR_TCP_CLIENT_OVERFLOW] = {
        .name = "recursor_tcp_client_overflow",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times an IP address was denied TCP access because "
                "it already had too many connections.",
    },
    [FAM_RECURSOR_TCP_CLIENTS] = {
        .name = "recursor_tcp_clients",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of currently active TCP/IP clients.",
    },
    [FAM_RECURSOR_TCP_OUTQUERIES] = {
        .name = "recursor_tcp_outqueries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of outgoing TCP queries since starting.",
    },
    [FAM_RECURSOR_TCP_QUESTIONS] = {
        .name = "recursor_tcp_questions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of all incoming TCP queries since starting.",
    },
    [FAM_RECURSOR_THROTTLE_ENTRIES] = {
        .name = "recursor_throttle_entries",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of of entries in the throttle map.",
    },
    [FAM_RECURSOR_THROTTLED_OUT] = {
        .name = "recursor_throttled_out",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of throttled outgoing UDP queries since starting.",
    },
    [FAM_RECURSOR_THROTTLED_OUTQUERIES] = {
        .name = "recursor_throttled_outqueries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of throttled outgoing UDP queries since starting.",
    },
    [FAM_RECURSOR_TOO_OLD_DROPS] = {
        .name = "recursor_too_old_drops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of questions dropped that were too old.",
    },
    [FAM_RECURSOR_TRUNCATED_DROPS] = {
        .name = "recursor_truncated_drops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of questions dropped because they were larger than 512 bytes.",
    },
    [FAM_RECURSOR_EMPTY_QUERIES] = {
        .name = "recursor_empty_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Questions dropped because they had a QD count of 0.",
    },
    [FAM_RECURSOR_UNAUTHORIZED_TCP] = {
        .name = "recursor_unauthorized_tcp",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of TCP questions denied because of allow-from restrictions.",
    },
    [FAM_RECURSOR_UNAUTHORIZED_UDP] = {
        .name = "recursor_unauthorized_udp",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of UDP questions denied because of allow-from restrictions.",
    },
    [FAM_RECURSOR_SOURCE_DISALLOWED_NOTIFY] = {
        .name = "recursor_source_disallowed_notify",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of NOTIFY operations denied because of allow-notify-from restrictions.",
    },
    [FAM_RECURSOR_UNEXPECTED_PACKETS] = {
        .name = "recursor_unexpected_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of answers from remote servers that were unexpected.",
    },
    [FAM_RECURSOR_UNREACHABLES] = {
        .name = "recursor_unreachables",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times nameservers were unreachable since starting.",
    },
    [FAM_RECURSOR_UPTIME] = {
        .name = "recursor_uptime",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of seconds process has been running.",
    },
    [FAM_RECURSOR_USER_MSEC] = {
        .name = "recursor_user_msec",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of CPU milliseconds spent in 'user' mode.",
    },
    [FAM_RECURSOR_VARIABLE_RESPONSES] = {
        .name = "recursor_variable_responses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses that were marked as 'variable'.",
    },
    [FAM_RECURSOR_X_OUR_LATENCY] = {
        .name = "recursor_x_our_latency",
        .type = METRIC_TYPE_GAUGE,
        .help = "Shows the averaged time spent within PowerDNS, in microseconds, "
                "exponentially weighted over past 'latency-statistic-size' packets.",
    },
    [FAM_RECURSOR_X_OUR_TIME] = {
        .name = "recursor_x_our_time",
        .type = METRIC_TYPE_COUNTER,
        .help = "Counts responses by the response time spent within the Recursor.",
    },
    [FAM_RECURSOR_FD_USAGE] = {
        .name = "recursor_fd_usage",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of open file descriptors.",
    },
    [FAM_RECURSOR_REAL_MEMORY_USAGE] = {
        .name = "recursor_real_memory_usage",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes real process memory usage.",
    },
    [FAM_RECURSOR_UDP_IN_ERRORS] = {
        .name = "recursor_udp_in_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "From /proc/net/snmp InErrors.",
    },
    [FAM_RECURSOR_UDP_NOPORT_ERRORS] = {
        .name = "recursor_udp_noport_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "From /proc/net/snmp NoPorts.",
    },
    [FAM_RECURSOR_UDP_RECVBUF_ERRORS] = {
        .name = "recursor_udp_recvbuf_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "From /proc/net/snmp RcvbufErrors.",
    },
    [FAM_RECURSOR_UDP_SNDBUF_ERRORS] = {
        .name = "recursor_udp_sndbuf_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "From /proc/net/snmp SndbufErrors.",
    },
    [FAM_RECURSOR_UDP_IN_CSUM_ERRORS] = {
        .name = "recursor_udp_in_csum_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "From /proc/net/snmp InCsumErrors.",
    },
    [FAM_RECURSOR_UDP6_IN_ERRORS] = {
        .name = "recursor_udp6_in_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "From /proc/net/snmp6 InErrors.",
    },
    [FAM_RECURSOR_UDP6_NOPORT_ERRORS] = {
        .name = "recursor_udp6_noport_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "From /proc/net/snmp6 NoPorts.",
    },
    [FAM_RECURSOR_UDP6_RECVBUF_ERRORS] = {
        .name = "recursor_udp6_recvbuf_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "From /proc/net/snmp6 RcvbufErrors.",
    },
    [FAM_RECURSOR_UDP6_SNDBUF_ERRORS] = {
        .name = "recursor_udp6_sndbuf_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "From /proc/net/snmp6 SndbufErrors.",
    },
    [FAM_RECURSOR_UDP6_IN_CSUM_ERRORS] = {
        .name = "recursor_udp6_in_csum_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "From /proc/net/snmp6 InCsumErrors.",
    },
    [FAM_RECURSOR_CPU_IOWAIT] = {
        .name = "recursor_cpu_iowait",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time spent waiting for I/O to complete by the whole system, in units of USER_HZ.",
    },
    [FAM_RECURSOR_CPU_STEAL] = {
        .name = "recursor_cpu_steal",
        .type = METRIC_TYPE_COUNTER,
        .help = "Stolen time, which is the time spent by the whole system "
                "in other operating systems when running in a virtualized environment, "
                "in units of USER_HZ.",
    },
    [FAM_RECURSOR_PROXY_PROTOCOL_INVALID] = {
        .name = "recursor_proxy-protocol_invalid",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of invalid proxy-protocol headers received.",
    },
    [FAM_RECURSOR_RECORD_CACHE_ACQUIRED] = {
        .name = "recursor_record_cache_acquired",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of record cache lock acquisitions.",
    },
    [FAM_RECURSOR_RECORD_CACHE_CONTENDED] = {
        .name = "recursor_record_cache_contended",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of contended record cache lock acquisitions.",
    },
    [FAM_RECURSOR_PACKETCACHE_ACQUIRED] = {
        .name = "recursor_packetcache_acquired",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packet cache lock acquisitions.",
    },
    [FAM_RECURSOR_PACKETCACHE_CONTENDED] = {
        .name = "recursor_packetcache_contended",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of contended packet cache lock acquisitions.",
    },
    [FAM_RECURSOR_TASKQUEUE_EXPIRED] = {
        .name = "recursor_taskqueue_expired",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of tasks expired before they could be run.",
    },
    [FAM_RECURSOR_TASKQUEUE_PUSHED] = {
        .name = "recursor_taskqueue_pushed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of tasks pushed to the taskqueues.",
    },
    [FAM_RECURSOR_TASKQUEUE_SIZE] = {
        .name = "recursor_taskqueue_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of tasks currently in the taskqueue.",
    },
    [FAM_RECURSOR_DOT_OUTQUERIES] = {
        .name = "recursor_dot_outqueries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of outgoing DoT queries since starting.",
    },
    [FAM_RECURSOR_DNS64_PREFIX_ANSWERS] = {
        .name = "recursor_dns64_prefix_answers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of AAAA and PTR generated by a matching dns64-prefix.",
    },
    [FAM_RECURSOR_AGGRESSIVE_NSEC_CACHE_ENTRIES] = {
        .name = "recursor_aggressive_nsec_cache_entries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of entries in the aggressive NSEC cache.",
    },
    [FAM_RECURSOR_AGGRESSIVE_NSEC_CACHE_NSEC_HITS] = {
        .name = "recursor_aggressive_nsec_cache_nsec_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of NSEC-related hits from the aggressive NSEC cache.",
    },
    [FAM_RECURSOR_AGGRESSIVE_NSEC_CACHE_NSEC_WC_HITS] = {
        .name = "recursor_aggressive_nsec_cache_nsec_wc_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of answers synthesized from the NSEC aggressive cache.",
    },
    [FAM_RECURSOR_AGGRESSIVE_NSEC_CACHE_NSEC3_HITS] = {
        .name = "recursor_aggressive_nsec_cache_nsec3_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of NSEC3-related hits from the aggressive NSEC cache.",
    },
    [FAM_RECURSOR_AGGRESSIVE_NSEC_CACHE_NSEC3_WC_HITS] = {
        .name = "recursor_aggressive_nsec_cache_nsec3_wc_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of answers synthesized from the NSEC3 aggressive cache.",
    },
    [FAM_RECURSOR_ALMOST_EXPIRED_PUSHED] = {
        .name = "recursor_almost_expired_pushed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of almost-expired tasks pushed.",
    },
    [FAM_RECURSOR_ALMOST_EXPIRED_RUN] = {
        .name = "recursor_almost_expired_run",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of almost-expired tasks run to completion.",
    },
    [FAM_RECURSOR_ALMOST_EXPIRED_EXCEPTIONS] = {
        .name = "recursor_almost_expired_exceptions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of almost-expired tasks that caused an exception.",
    },

    [FAM_RECURSOR_IDLE_TCPOUT_CONNECTIONS] = {
        .name = "recursor_idle_tcpout_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of connections in the TCP idle outgoing connections pool.",
    },
    [FAM_RECURSOR_MAINTENANCE_USEC] = {
        .name = "recursor_maintenance_usec",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time spent doing internal maintenance, including Lua maintenance.",
    },
    [FAM_RECURSOR_MAINTENANCE_CALLS] = {
        .name = "recursor_maintenance_calls",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times internal maintenance has been called, including Lua maintenance.",
    },
    [FAM_RECURSOR_NOD_EVENTS] = {
        .name = "recursor_nod_events",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of NOD events.",
    },
    [FAM_RECURSOR_UDR_EVENTS] = {
        .name = "recursor_udr_events",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of UDR events.",
    }
};

#include "plugins/recursor/recursor.h"

typedef enum {
    RECURSOR_PROTOCOL_V1,
    RECURSOR_PROTOCOL_V2,
    RECURSOR_PROTOCOL_V3,
} recursor_protocol_t;

typedef struct {
    char *name;
    recursor_protocol_t version;
    label_set_t labels;
    cdtime_t timeout;
    char command[24];
    size_t command_size;
    char *local_sockpath;
    struct sockaddr_un local_sockaddr;
    char *sockpath;
    struct sockaddr_un sockaddr;
    metric_family_t fams[FAM_RECURSOR_MAX];
} recursor_t;

#if 0
// For cumulative histogram, state the xxx_count name where xxx matches the name in rec_channel_rec
cumul-clientanswers-count,
histogram,
"histogram of our answer times to clients",
// For cumulative histogram, state the xxx_count name where xxx matches the name in rec_channel_rec
cumul-authanswers-count4,
histogram,
"histogram of answer times of authoritative servers",
// For multicounters, state the first
policy_hits,
multicounter,
"Number of filter or RPZ policy hits",
// For multicounters, state the first
proxy_mapping_total_n_0,
multicounter,
"Number of queries matching proxyMappings",
// For multicounters, state the first
remote_logger_count_o_0,
multicounter,
"Number of remote logging events",
#endif

static void recursor_free(void *arg)
{
    recursor_t *recursor = arg;
    if (recursor == NULL)
        return;

    free(recursor->name);
    label_set_reset(&recursor->labels);
    free(recursor->local_sockpath);
    free(recursor->sockpath);
    free(recursor);
}

static int recursor_read(user_data_t *user_data)
{
    if ((user_data == NULL) || (user_data->data == NULL))
        return -1;

    recursor_t *recursor = user_data->data;

    int sd = -1;

    switch (recursor->version) {
    case RECURSOR_PROTOCOL_V1:
    case RECURSOR_PROTOCOL_V2: {
        sd = socket(PF_UNIX, SOCK_DGRAM, 0);
        if (sd < 0) {
            PLUGIN_ERROR("socket failed: %s", STRERRNO);
            return -1;
        }

        int status = unlink(recursor->local_sockaddr.sun_path);
        if ((status != 0) && (errno != ENOENT)) {
            PLUGIN_ERROR("Socket '%s' unlink failed: %s",
                         recursor->local_sockaddr.sun_path, STRERRNO);
            close(sd);
            return -1;
        }

        /* We need to bind to a specific path, because this is a datagram socket
         * and otherwise the daemon cannot answer. */
        status = bind(sd, (struct sockaddr *)&recursor->local_sockaddr,
                          sizeof(recursor->local_sockaddr));
        if (status != 0) {
            PLUGIN_ERROR("Socket '%s' bind failed: %s",
                         recursor->local_sockaddr.sun_path, STRERRNO);
            close(sd);
            return -1;
        }

        /* Make the socket writeable by the daemon.. */
        status = chmod(recursor->local_sockaddr.sun_path, 0666);
        if (status != 0) {
            PLUGIN_ERROR("Socket '%s' chmod failed: %s",
                         recursor->local_sockaddr.sun_path, STRERRNO);
            close(sd);
            return -1;
        }

    }   break;
    case RECURSOR_PROTOCOL_V3: {
        sd = socket(PF_UNIX, SOCK_STREAM, 0);
        if (sd < 0) {
            PLUGIN_ERROR("socket failed: %s", STRERRNO);
            return -1;
        }
    }   break;
    }

    struct timeval timeout =  CDTIME_T_TO_TIMEVAL(recursor->timeout);
    int status = setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));
    if (status != 0) {
        PLUGIN_ERROR("Socket '%s' setsockopt failed: %s",
                     recursor->local_sockaddr.sun_path, STRERRNO);
        close(sd);
        return -1;
    }

    status = connect(sd, (struct sockaddr *)&recursor->sockaddr, sizeof(recursor->sockaddr));
    if (status != 0) {
        PLUGIN_ERROR("Socket '%s' connect failed: %s", recursor->sockaddr.sun_path, STRERRNO);
        close(sd);
        return -1;
    }

    status = send(sd, recursor->command, recursor->command_size, 0);
    if (status < 0) {
        PLUGIN_ERROR("Socket '%s' send failed: %s", recursor->sockaddr.sun_path, STRERRNO);
        close(sd);
        return -1;
    }

    char buffer[65536];
    status = recv(sd, buffer, sizeof(buffer) - 1, /* flags = */ 0);
    if (status < 0) {
        PLUGIN_ERROR("Socket '%s' recv failed: %s", recursor->sockaddr.sun_path, STRERRNO);
        close(sd);
        return -1;
    }

    size_t buffer_size = status;
    buffer[buffer_size] = '\0';

    close(sd);

    char *response = NULL;

    switch (recursor->version) {
    case RECURSOR_PROTOCOL_V1:
        unlink(recursor->local_sockaddr.sun_path);
        response = buffer;
        break;
    case RECURSOR_PROTOCOL_V2:
        unlink(recursor->local_sockaddr.sun_path);
        if (buffer_size <= 4) {
            PLUGIN_ERROR("Response too small.");
            return -1;
        }
        response = buffer + 4;
        break;
    case RECURSOR_PROTOCOL_V3: {
        if (buffer_size <= (4 + sizeof(size_t))) {
            PLUGIN_ERROR("Response too small.");
            return -1;
        }
        size_t response_size = 0;
        memcpy(&response_size, buffer + 4, sizeof(size_t));
        if (response_size != (buffer_size - 4 - sizeof(size_t))) {
            PLUGIN_ERROR("Invalid data size.");
            return -1;
        }
        response = buffer + 4 + sizeof(size_t);
    }   break;
    }

    if (response == NULL)
        return 0;

    /* Skip the `get' at the beginning ¿? */
    char *saveptr = NULL;
    char *key;
    while ((key = strtok_r(response, " \t\n\r", &saveptr)) != NULL) {
        response = NULL;

        char *value = strtok_r(NULL, " \t\n\r", &saveptr);
        if (value == NULL)
            break;


        const struct recursor_metric *m = recursor_get_key(key, strlen(key));
        if (m == NULL) {
            switch (key[0]) {
            case 'a': {
                if (strncmp(key, "auth-", strlen("auth-")) != 0)
                    continue;
                key += strlen("auth-");
                size_t key_len = strlen(key);
                if (key_len <= strlen("-answers"))
                    continue;
                key_len -= strlen("-answers");
                if (strncmp(key + key_len, "-answers", strlen("-answers")) != 0)
                    continue;
                key[key_len] = '\0';

                metric_family_append(&recursor->fams[FAM_RECURSOR_AUTH_ANSWERS],
                                     VALUE_COUNTER(atoll(value)), &recursor->labels,
                                     &(label_pair_const_t){.name="rcode", .value=key}, NULL);
                continue;
            }   break;
            case 'c': {
                if (strncmp(key, "cpu-msec-thread-", strlen("cpu-msec-thread-")) != 0)
                    continue;
                key += strlen("cpu-msec-thread-");

                metric_family_append(&recursor->fams[FAM_RECURSOR_THREAD_CPU_MSEC],
                                     VALUE_COUNTER(atoll(value)), &recursor->labels,
                                     &(label_pair_const_t){.name="thread", .value=key}, NULL);

                continue;
            }   break;
            }

            continue;
        }

        metric_family_t *fam = &recursor->fams[m->fam];

        value_t mvalue = {0};
        if (fam->type == METRIC_TYPE_COUNTER) {
            mvalue = VALUE_COUNTER(atoll(value));
        } else if (fam->type == METRIC_TYPE_GAUGE) {
            mvalue = VALUE_GAUGE(atof(value));
        } else {
            continue;
        }

        if ((m->lkey != NULL) && (m->lvalue != NULL))
            metric_family_append(fam, mvalue, &recursor->labels,
                                 &(label_pair_const_t){.name=m->lkey, .value=m->lvalue}, NULL);
        else
            metric_family_append(fam, mvalue, &recursor->labels, NULL);
    }

    plugin_dispatch_metric_family_array(recursor->fams, FAM_RECURSOR_MAX, 0);

    return 0;
}

int recursor_config_instance(config_item_t *ci)
{
    recursor_t *recursor = calloc(1, sizeof(*recursor));
    if (recursor == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    memcpy(recursor->fams, fams_recursor, sizeof(recursor->fams[0])*FAM_RECURSOR_MAX);

    int status = cf_util_get_string(ci, &recursor->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing recursor name.");
        free(recursor);
        return status;
    }

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &recursor->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &recursor->timeout);
        } else if (strcasecmp("local-socket", child->key) == 0) {
            status = cf_util_get_string(child, &recursor->local_sockpath);
        } else if (strcasecmp("socket", child->key) == 0) {
            status = cf_util_get_string(child, &recursor->sockpath);
        } else if (strcasecmp("protocol", child->key) == 0) {
            int version = 0;
            status = cf_util_get_int(child, &version);
            if (status == 0) {
                if ((version > 0) && (version < 4)) {
                    recursor->version = version - 1;
                } else {
                    PLUGIN_ERROR("Invalid protocol number '%d' in %s:%d,"
                                 " must be 1, 2 or 3.",
                                 version, cf_get_file(child), cf_get_lineno(child));
                    status = -1;
                }
            }
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        recursor_free(recursor);
        return -1;
    }

    uint32_t zero = 0;

    switch (recursor->version) {
    case RECURSOR_PROTOCOL_V1:
        sstrncpy(recursor->command, "get-all\n", sizeof(recursor->command));
        recursor->command_size = strlen(recursor->command);
        break;
    case RECURSOR_PROTOCOL_V2:
        memcpy(recursor->command, &zero, sizeof(uint32_t));
        sstrncpy(recursor->command + sizeof(uint32_t), "get-all", sizeof(recursor->command)-4);
        recursor->command_size = sizeof(uint32_t) + strlen("get-all");
        break;
    case RECURSOR_PROTOCOL_V3:
        memcpy(recursor->command, &zero, sizeof(uint32_t));
        recursor->command_size = strlen("get-all");
        memcpy(recursor->command + sizeof(uint32_t), &recursor->command_size, sizeof(size_t));
        sstrncpy(recursor->command + sizeof(uint32_t) + sizeof(size_t), "get-all",
                    sizeof(recursor->command) - sizeof(uint32_t) - sizeof(size_t));
        recursor->command_size += sizeof(uint32_t) + sizeof(size_t);
        break;
    }

    if (recursor->sockpath == NULL) {
        recursor->sockpath = strdup(RECURSOR_SOCKET);
        if (recursor->sockpath == NULL) {
           PLUGIN_ERROR("strdup failed.");
           recursor_free(recursor);
           return -1;
        }
    }

    recursor->sockaddr.sun_family = AF_UNIX;
    sstrncpy(recursor->sockaddr.sun_path, recursor->sockpath, sizeof(recursor->sockaddr.sun_path));

    if (recursor->local_sockpath == NULL) {
        recursor->local_sockpath = strdup(PDNS_LOCAL_SOCKPATH);
        if (recursor->local_sockpath == NULL) {
           PLUGIN_ERROR("strdup failed.");
           recursor_free(recursor);
           return -1;
        }
    }

    recursor->local_sockaddr.sun_family = AF_UNIX;
    sstrncpy(recursor->local_sockaddr.sun_path, recursor->local_sockpath,
                                                sizeof(recursor->local_sockaddr.sun_path));

    if (recursor->timeout == 0) {
        if (interval == 0) {
            recursor->timeout = plugin_get_interval() * 3 / 4;
        } else {
            recursor->timeout = interval * 3 / 4;
        }
    }

    label_set_add(&recursor->labels, true, "instance", recursor->name);

    return plugin_register_complex_read("recursor", recursor->name,
                                        recursor_read, interval,
                                        &(user_data_t){ .data = recursor,
                                                        .free_func = recursor_free});
}

static int recursor_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = recursor_config_instance(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("recursor", recursor_config);
}
