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

// #define SERVER_SOCKET LOCALSTATEDIR "/run/pdns.controlsocket"
#define SERVER_SOCKET LOCALSTATEDIR "/run/pdns/pdns.controlsocket"

enum {
    FAM_PDNS_UDP_QUERIES,
    FAM_PDNS_UDP_DO_QUERIES,
    FAM_PDNS_UDP_COOKIE_QUERIES,
    FAM_PDNS_UDP_ANSWERS,
    FAM_PDNS_UDP_ANSWERS_BYTES,
    FAM_PDNS_UDP4_ANSWERS_BYTES,
    FAM_PDNS_UDP6_ANSWERS_BYTES,
    FAM_PDNS_UDP4_ANSWERS,
    FAM_PDNS_UDP4_QUERIES,
    FAM_PDNS_UDP6_ANSWERS,
    FAM_PDNS_UDP6_QUERIES,
    FAM_PDNS_OVERLOAD_DROPS,
    FAM_PDNS_RD_QUERIES,
    FAM_PDNS_RECURSION_UNANSWERED,
    FAM_PDNS_RECURSING_ANSWERS,
    FAM_PDNS_RECURSING_QUESTIONS,
    FAM_PDNS_CORRUPT_PACKETS,
    FAM_PDNS_SIGNATURES,
    FAM_PDNS_TCP_QUERIES,
    FAM_PDNS_TCP_COOKIE_QUERIES,
    FAM_PDNS_TCP_ANSWERS,
    FAM_PDNS_TCP_ANSWERS_BYTES,
    FAM_PDNS_TCP4_ANSWERS_BYTES,
    FAM_PDNS_TCP6_ANSWERS_BYTES,
    FAM_PDNS_TCP4_QUERIES,
    FAM_PDNS_TCP4_ANSWERS,
    FAM_PDNS_TCP6_QUERIES,
    FAM_PDNS_TCP6_ANSWERS,
    FAM_PDNS_OPEN_TCP_CONNECTIONS,
    FAM_PDNS_QSIZE_Q,
    FAM_PDNS_DNSUPDATE_QUERIES,
    FAM_PDNS_DNSUPDATE_ANSWERS,
    FAM_PDNS_DNSUPDATE_REFUSED,
    FAM_PDNS_DNSUPDATE_CHANGES,
    FAM_PDNS_INCOMING_NOTIFICATIONS,
    FAM_PDNS_UPTIME_SECONDS,
    FAM_PDNS_REAL_MEMORY_USAGE_BYTES,
    FAM_PDNS_SPECIAL_MEMORY_USAGE_BYTES,
    FAM_PDNS_FD_USAGE,
    FAM_PDNS_UDP_RECVBUF_ERRORS,
    FAM_PDNS_UDP_SNDBUF_ERRORS,
    FAM_PDNS_UDP_NOPORT_ERRORS,
    FAM_PDNS_UDP_IN_ERRORS,
    FAM_PDNS_UDP_IN_CSUM_ERRORS,
    FAM_PDNS_UDP6_IN_ERRORS,
    FAM_PDNS_UDP6_RECVBUF_ERRORS,
    FAM_PDNS_UDP6_SNDBUF_ERRORS,
    FAM_PDNS_UDP6_NOPORT_ERRORS,
    FAM_PDNS_UDP6_IN_CSUM_ERRORS,
    FAM_PDNS_SYS_MSEC,
    FAM_PDNS_USER_MSEC,
    FAM_PDNS_CPU_IOWAIT,
    FAM_PDNS_CPU_STEAL,
    FAM_PDNS_META_CACHE_SIZE,
    FAM_PDNS_KEY_CACHE_SIZE,
    FAM_PDNS_SIGNATURE_CACHE_SIZE,
    FAM_PDNS_NXDOMAIN_PACKETS,
    FAM_PDNS_NOERROR_PACKETS,
    FAM_PDNS_SERVFAIL_PACKETS,
    FAM_PDNS_UNAUTH_PACKETS,
    FAM_PDNS_LATENCY,
    FAM_PDNS_RECEIVE_LATENCY,
    FAM_PDNS_CACHE_LATENCY,
    FAM_PDNS_BACKEND_LATENCY,
    FAM_PDNS_SEND_LATENCY,
    FAM_PDNS_TIMEDOUT_PACKETS,
    FAM_PDNS_SECURITY_STATUS,
    FAM_PDNS_XFR_QUEUE,
    FAM_PDNS_BACKEND_QUERIES,
    FAM_PDNS_QUERY_CACHE_HIT,
    FAM_PDNS_QUERY_CACHE_MISS,
    FAM_PDNS_QUERY_CACHE_SIZE,
    FAM_PDNS_DEFERRED_CACHE_INSERTS,
    FAM_PDNS_DEFERRED_CACHE_LOOKUP,
    FAM_PDNS_PACKETCACHE_HIT,
    FAM_PDNS_PACKETCACHE_MISS,
    FAM_PDNS_PACKETCACHE_SIZE,
    FAM_PDNS_DEFERRED_PACKETCACHE_INSERTS,
    FAM_PDNS_DEFERRED_PACKETCACHE_LOOKUP,
    FAM_PDNS_ZONE_CACHE_HIT,
    FAM_PDNS_ZONE_CACHE_MISS,
    FAM_PDNS_ZONE_CACHE_SIZE,
    FAM_PDNS_MAX
};

static metric_family_t fams_server[FAM_PDNS_MAX] = {
    [FAM_PDNS_UDP_QUERIES] = {
        .name = "pdns_udp_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of UDP queries received.",
    },
    [FAM_PDNS_UDP_DO_QUERIES] = {
        .name = "pdns_udp_do_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of UDP queries received with DO bit.",
    },
    [FAM_PDNS_UDP_COOKIE_QUERIES] = {
        .name = "pdns_udp_cookie_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of UDP queries received with the COOKIE EDNS option.",
    },
    [FAM_PDNS_UDP_ANSWERS] = {
        .name = "pdns_udp_answers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of answers sent out over UDP.",
    },
    [FAM_PDNS_UDP_ANSWERS_BYTES] = {
        .name = "pdns_udp_answers_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total size of answers sent out over UDP.",
    },
    [FAM_PDNS_UDP4_ANSWERS_BYTES] = {
        .name = "pdns_udp4_answers_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total size of answers sent out over UDPv4.",
    },
    [FAM_PDNS_UDP6_ANSWERS_BYTES] = {
        .name = "pdns_udp6_answers_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total size of answers sent out over UDPv6.",
    },
    [FAM_PDNS_UDP4_ANSWERS] = {
        .name = "pdns_udp4_answers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of IPv4 answers sent out over UDP.",
    },
    [FAM_PDNS_UDP4_QUERIES] = {
        .name = "pdns_udp4_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of IPv4 UDP queries received.",
    },
    [FAM_PDNS_UDP6_ANSWERS] = {
        .name = "pdns_udp6_answers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of IPv6 answers sent out over UDP.",
    },
    [FAM_PDNS_UDP6_QUERIES] = {
        .name = "pdns_udp6_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of IPv6 UDP queries received.",
    },
    [FAM_PDNS_OVERLOAD_DROPS] = {
        .name = "pdns_overload_drops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Queries dropped because backends overloaded.",
    },
    [FAM_PDNS_RD_QUERIES] = {
        .name = "pdns_rd_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of recursion desired questions.",
    },
    [FAM_PDNS_RECURSION_UNANSWERED] = {
        .name = "pdns_recursion_unanswered",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packets unanswered by configured recursor.",
    },
    [FAM_PDNS_RECURSING_ANSWERS] = {
        .name = "pdns_recursing_answers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of recursive answers sent out.",
    },
    [FAM_PDNS_RECURSING_QUESTIONS] = {
        .name = "pdns_recursing_questions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of questions sent to recursor.",
    },
    [FAM_PDNS_CORRUPT_PACKETS] = {
        .name = "pdns_corrupt_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of corrupt packets received.",
    },
    [FAM_PDNS_SIGNATURES] = {
        .name = "pdns_signatures",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of DNSSEC signatures made.",
    },
    [FAM_PDNS_TCP_QUERIES] = {
        .name = "pdns_tcp_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of TCP queries received.",
    },
    [FAM_PDNS_TCP_COOKIE_QUERIES] = {
        .name = "pdns_tcp_cookie_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of TCP queries received with the COOKIE option.",
    },
    [FAM_PDNS_TCP_ANSWERS] = {
        .name = "pdns_tcp_answers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of answers sent out over TCP.",
    },
    [FAM_PDNS_TCP_ANSWERS_BYTES] = {
        .name = "pdns_tcp_answers_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total size of answers sent out over TCP.",
    },
    [FAM_PDNS_TCP4_ANSWERS_BYTES] = {
        .name = "pdns_tcp4_answers_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total size of answers sent out over TCPv4.",
    },
    [FAM_PDNS_TCP6_ANSWERS_BYTES] = {
        .name = "pdns_tcp6_answers_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total size of answers sent out over TCPv6.",
    },
    [FAM_PDNS_TCP4_QUERIES] = {
        .name = "pdns_tcp4_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of IPv4 TCP queries received.",
    },
    [FAM_PDNS_TCP4_ANSWERS] = {
        .name = "pdns_tcp4_answers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of IPv4 answers sent out over TCP.",
    },
    [FAM_PDNS_TCP6_QUERIES] = {
        .name = "pdns_tcp6_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of IPv6 TCP queries received.",
    },
    [FAM_PDNS_TCP6_ANSWERS] = {
        .name = "pdns_tcp6_answers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of IPv6 answers sent out over TCP.",
    },
    [FAM_PDNS_OPEN_TCP_CONNECTIONS] = {
        .name = "pdns_open_tcp_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of currently open TCP connections.",
    },
    [FAM_PDNS_QSIZE_Q] = {
        .name = "pdns_qsize_q",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of questions waiting for database attention.",
    },
    [FAM_PDNS_DNSUPDATE_QUERIES] = {
        .name = "pdns_dnsupdate_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "DNS update packets received.",
    },
    [FAM_PDNS_DNSUPDATE_ANSWERS] = {
        .name = "pdns_dnsupdate_answers",
        .type = METRIC_TYPE_COUNTER,
        .help = "DNS update packets successfully answered.",
    },
    [FAM_PDNS_DNSUPDATE_REFUSED] = {
        .name = "pdns_dnsupdate_refused",
        .type = METRIC_TYPE_COUNTER,
        .help = "DNS update packets that are refused.",
    },
    [FAM_PDNS_DNSUPDATE_CHANGES] = {
        .name = "pdns_dnsupdate_changes",
        .type = METRIC_TYPE_COUNTER,
        .help = "DNS update changes to records in total.",
    },
    [FAM_PDNS_INCOMING_NOTIFICATIONS] =  {
        .name = "pdns_incoming_notifications",
        .type = METRIC_TYPE_COUNTER,
        .help = "NOTIFY packets received.",
    },
    [FAM_PDNS_UPTIME_SECONDS] = {
        .name = "pdns_uptime_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Uptime of process in seconds.",
    },
    [FAM_PDNS_REAL_MEMORY_USAGE_BYTES] = {
        .name = "pdns_real_memory_usage_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Actual unique use of memory in bytes (approx).",
    },
    [FAM_PDNS_SPECIAL_MEMORY_USAGE_BYTES] = {
        .name = "pdns_special_memory_usage_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Actual unique use of memory in bytes (approx).",
    },
    [FAM_PDNS_FD_USAGE] = {
        .name = "pdns_fd_usage",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of open filedescriptors.",
    },
    [FAM_PDNS_UDP_RECVBUF_ERRORS] = {
        .name = "pdns_udp_recvbuf_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "UDP 'recvbuf' errors.",
    },
    [FAM_PDNS_UDP_SNDBUF_ERRORS] = {
        .name = "pdns_udp_sndbuf_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "UDP 'sndbuf' errors.",
    },
    [FAM_PDNS_UDP_NOPORT_ERRORS] = {
        .name = "pdns_udp_noport_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "UDP 'noport' errors.",
    },
    [FAM_PDNS_UDP_IN_ERRORS] = {
        .name = "pdns_udp_in_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "UDP 'in' errors.",
    },
    [FAM_PDNS_UDP_IN_CSUM_ERRORS] = {
        .name = "pdns_udp_in_csum_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "UDP 'in checksum' errors.",
    },
    [FAM_PDNS_UDP6_IN_ERRORS] = {
        .name = "pdns_udp6_in_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "UDP 'in' errors over IPv6.",
    },
    [FAM_PDNS_UDP6_RECVBUF_ERRORS] = {
        .name = "pdns_udp6_recvbuf_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "UDP 'recvbuf' errors over IPv6.",
    },
    [FAM_PDNS_UDP6_SNDBUF_ERRORS] = {
        .name = "pdns_udp6_sndbuf_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "UDP 'sndbuf' errors over IPv6.",
    },
    [FAM_PDNS_UDP6_NOPORT_ERRORS] = {
        .name = "pdns_udp6_noport_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "UDP 'noport' errors over IPv6.",
    },
    [FAM_PDNS_UDP6_IN_CSUM_ERRORS] = {
        .name = "pdns_udp6_in_csum_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "UDP 'in checksum' errors over IPv6.",
    },
    [FAM_PDNS_SYS_MSEC] = {
        .name = "pdns_sys_msec",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of msec spent in system time.",
    },
    [FAM_PDNS_USER_MSEC] = {
        .name = "pdns_user_msec",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of msec spent in user time.",
    },
    [FAM_PDNS_CPU_IOWAIT] = {
        .name = "pdns_cpu_iowait",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time spent waiting for I/O to complete by the whole system, in units of USER_HZ.",
    },
    [FAM_PDNS_CPU_STEAL] = {
        .name = "pdns_cpu_steal",
        .type = METRIC_TYPE_COUNTER,
        .help = "Stolen time, which is the time spent by the whole system in other "
                "operating systems when running in a virtualized environment, in units of USER_HZ.",
    },
    [FAM_PDNS_META_CACHE_SIZE] = {
        .name = "pdns_meta_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of entries in the metadata cache.",
    },
    [FAM_PDNS_KEY_CACHE_SIZE] = {
        .name = "pdns_key_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of entries in the key cache.",
    },
    [FAM_PDNS_SIGNATURE_CACHE_SIZE] = {
        .name = "pdns_signature_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of entries in the signature cache.",
    },
    [FAM_PDNS_NXDOMAIN_PACKETS] = {
        .name = "pdns_nxdomain_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times an NXDOMAIN packet was sent out."
    },
    [FAM_PDNS_NOERROR_PACKETS] = {
        .name = "pdns_noerror_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a NOERROR packet was sent out."
    },
    [FAM_PDNS_SERVFAIL_PACKETS] = {
        .name = "pdns_servfail_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a server-failed packet was sent out."
    },
    [FAM_PDNS_UNAUTH_PACKETS] = {
        .name = "pdns_unauth_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a zone we are not auth for was queried."
    },
    [FAM_PDNS_LATENCY] = {
        .name = "pdns_latency",
        .type = METRIC_TYPE_GAUGE,
        .help = "Average number of microseconds needed to answer a question.",
    },
    [FAM_PDNS_RECEIVE_LATENCY] = {
        .name = "pdns_receive_latency",
        .type = METRIC_TYPE_GAUGE,
        .help = "Average number of microseconds needed to receive a query.",
    },
    [FAM_PDNS_CACHE_LATENCY] = {
        .name = "pdns_cache_latency",
        .type = METRIC_TYPE_GAUGE,
        .help = "Average number of microseconds needed for a packet cache lookup.",
    },
    [FAM_PDNS_BACKEND_LATENCY] = {
        .name = "pdns_backend_latency",
        .type = METRIC_TYPE_GAUGE,
        .help = "Average number of microseconds needed for a backend lookup.",
    },
    [FAM_PDNS_SEND_LATENCY] = {
        .name = "pdns_send_latency",
        .type = METRIC_TYPE_GAUGE,
        .help = "Average number of microseconds needed to send the answer.",
    },
    [FAM_PDNS_TIMEDOUT_PACKETS] = {
        .name = "pdns_timedout_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packets which weren't answered within timeout set.",
    },
    [FAM_PDNS_SECURITY_STATUS] = {
        .name = "pdns_security_status",
        .type = METRIC_TYPE_GAUGE,
        .help = "Security status based on regular polling.",
    },
    [FAM_PDNS_XFR_QUEUE] = {
        .name = "pdns_xfr_queue",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size of the queue of zones to be XFRd.",
    },
    [FAM_PDNS_BACKEND_QUERIES] = {
        .name = "pdns_backend_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Size of the queue of zones to be XFRd.",
    },
    [FAM_PDNS_QUERY_CACHE_HIT] = {
        .name = "pdns_query_cache_hit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of hits on the query cache.",
    },
    [FAM_PDNS_QUERY_CACHE_MISS] = {
        .name = "pdns_query_cache_miss",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of misses on the query cache.",
    },
    [FAM_PDNS_QUERY_CACHE_SIZE] = {
        .name = "pdns_query_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of entries in the query cache.",
    },
    [FAM_PDNS_DEFERRED_CACHE_INSERTS] = {
        .name = "pdns_deferred_cache_inserts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of cache inserts that were deferred because of maintenance.",
    },
    [FAM_PDNS_DEFERRED_CACHE_LOOKUP] = {
        .name = "pdns_deferred_cache_lookup",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of cache lookups that were deferred because of maintenance.",
    },
    [FAM_PDNS_PACKETCACHE_HIT] = {
        .name = "pdns_packetcache_hit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of hits on the packet cache.",
    },
    [FAM_PDNS_PACKETCACHE_MISS] = {
        .name = "pdns_packetcache_miss",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of misses on the packet cache.",
    },
    [FAM_PDNS_PACKETCACHE_SIZE] = {
        .name = "pdns_packetcache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of entries in the packet cache.",
    },
    [FAM_PDNS_DEFERRED_PACKETCACHE_INSERTS] = {
        .name = "pdns_deferred_packetcache_inserts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of packet cache inserts that were deferred because of maintenance.",
    },
    [FAM_PDNS_DEFERRED_PACKETCACHE_LOOKUP] = {
        .name = "pdns_deferred_packetcache_lookup",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of packet cache lookups that were deferred because of maintenance.",
    },
    [FAM_PDNS_ZONE_CACHE_HIT] = {
        .name = "pdns_zone_cache_hit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of zone cache hits.",
    },
    [FAM_PDNS_ZONE_CACHE_MISS] = {
        .name = "pdns_zone_cache_miss",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of zone cache misses.",
    },
    [FAM_PDNS_ZONE_CACHE_SIZE] = {
        .name = "pdns_zone_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of entries in the zone cache.",
    },
};

#include "plugins/pdns/pdns.h"

typedef struct {
    char *name;
    label_set_t labels;
    plugin_filter_t *filter;
    cdtime_t timeout;
    char *command;
    char *sockpath;
    struct sockaddr_un sockaddr;
    metric_family_t fams[FAM_PDNS_MAX];
} pdns_t;

static void pdns_free(void *arg)
{
    pdns_t *server = arg;
    if (server == NULL)
        return;

    free(server->name);
    label_set_reset(&server->labels);
    plugin_filter_free(server->filter);
    free(server->sockpath);
    free(server);
}

static int pdns_read(user_data_t *user_data)
{
    if ((user_data == NULL) || (user_data->data == NULL))
        return -1;

    pdns_t *server = user_data->data;

    int sd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sd < 0) {
        PLUGIN_ERROR("socket failed: %s", STRERRNO);
        return -1;
    }

    struct timeval timeout = CDTIME_T_TO_TIMEVAL(server->timeout);
    int status = setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (status != 0) {
        PLUGIN_ERROR("setockopt failed: %s", STRERRNO);
        close(sd);
        return -1;
    }

    status = connect(sd, (struct sockaddr *)&server->sockaddr, sizeof(server->sockaddr));
    if (status != 0) {
        PLUGIN_ERROR("Socket '%s' connect failed: %s", server->sockaddr.sun_path, STRERRNO);
        close(sd);
        return -1;
    }

    const char *command = "SHOW * \n";
    /* strlen + 1, because we need to send the terminating NULL byte, too. */
    status = send(sd, command, strlen(command) + 1, /* flags = */ 0);
    if (status < 0) {
        PLUGIN_ERROR("Socket '%s' send failed: %s", server->sockaddr.sun_path, STRERRNO);
        close(sd);
        return -1;
    }

    char *buffer = NULL;
    size_t buffer_size = 0;
    while (true) {
        char temp[4096];
        status = recv(sd, temp, sizeof(temp), /* flags = */ 0);
        if (status < 0) {
            PLUGIN_ERROR("Socket '%s' recv failed: %s", server->sockaddr.sun_path, STRERRNO);
            break;
        } else if (status == 0) {
            break;
        }

        char *buffer_new = realloc(buffer, buffer_size + status + 1);
        if (buffer_new == NULL) {
            PLUGIN_ERROR("realloc failed: %s", STRERRNO);
            status = ENOMEM;
            break;
        }
        buffer = buffer_new;

        memcpy(buffer + buffer_size, temp, status);
        buffer_size += status;
        buffer[buffer_size] = 0;
    }
    close(sd);

    if (status != 0) {
        free(buffer);
        return status;
    }

    char *dummy = buffer;
    char *saveptr = NULL;
    char *key;
    while ((key = strtok_r(dummy, ",", &saveptr)) != NULL) {
        dummy = NULL;

        char *value = strchr(key, '=');
        if (value == NULL)
            break;

        *value = '\0';
        value++;

        if (value[0] == '\0')
            continue;

        const struct pdns_metric *m = pdns_get_key(key, strlen(key));
        if (m == NULL)
            continue;

        if (m->fam < 0)
            continue;

        metric_family_t *fam = &server->fams[m->fam];

        value_t mvalue = {0};
        if (fam->type == METRIC_TYPE_COUNTER) {
            mvalue = VALUE_COUNTER(atoll(value));
        } else if (fam->type == METRIC_TYPE_GAUGE) {
            mvalue = VALUE_GAUGE(atof(value));
        } else {
            continue;
        }

        metric_family_append(fam, mvalue, &server->labels, NULL);
    }

    free(buffer);
    plugin_dispatch_metric_family_array_filtered(server->fams, FAM_PDNS_MAX, server->filter, 0);

    return 0;
}

int pdns_config_instance(config_item_t *ci)
{
    pdns_t *server = calloc(1, sizeof(*server));
    if (server == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    memcpy(server->fams, fams_server, sizeof(server->fams[0])*FAM_PDNS_MAX);

    int status = cf_util_get_string(ci, &server->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing server name.");
        free(server);
        return status;
    }
    assert(server->name != NULL);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &server->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &server->timeout);
        } else if (strcasecmp("socket", child->key) == 0) {
            status = cf_util_get_string(child, &server->sockpath);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &server->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }


    if (status != 0) {
        pdns_free(server);
        return -1;
    }

    if (server->sockpath == NULL) {
        server->sockpath = strdup(SERVER_SOCKET);
        if (server->sockpath == NULL) {
           PLUGIN_ERROR("strdup failed.");
           pdns_free(server);
           return -1;
        }
    }

    if (server->timeout == 0) {
        if (interval == 0) {
            server->timeout = plugin_get_interval() / 2;
        } else {
            server->timeout = interval / 2;
        }
    }

    server->sockaddr.sun_family = AF_UNIX;
    sstrncpy(server->sockaddr.sun_path, server->sockpath, sizeof(server->sockaddr.sun_path));

    label_set_add(&server->labels, true, "instance", server->name);

    return plugin_register_complex_read("pdns", server->name, pdns_read, interval,
                                        &(user_data_t){ .data = server,
                                                        .free_func = pdns_free});
}
static int pdns_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = pdns_config_instance(child);
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
    plugin_register_config("pdns", pdns_config);
}
