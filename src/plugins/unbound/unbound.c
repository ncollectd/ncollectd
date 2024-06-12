// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/socket.h"

#include <netdb.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/bn.h>
#include <openssl/asn1.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#define UNBOUND_SERVER_CERT_FILE "/etc/unbound/unbound_server.pem"
#define UNBOUND_CONTROL_KEY_FILE "/etc/unbound/unbound_control.key"
#define UNBOUND_CONTROL_CERT_FILE "/etc/unbound/unbound_control.pem"
#define UNBOUND_CONTROL_PORT 8953

enum {
    FAM_UNBOUND_UP,
    FAM_UNBOUND_THREAD_QUERIES,
    FAM_UNBOUND_THREAD_QUERIES_IP_RATELIMITED,
    FAM_UNBOUND_THREAD_CACHE_HITS,
    FAM_UNBOUND_THREAD_CACHE_MISS,
    FAM_UNBOUND_THREAD_DNSCRYPT_CRYPTED,
    FAM_UNBOUND_THREAD_DNSCRYPT_CERT,
    FAM_UNBOUND_THREAD_DNSCRYPT_CLEARTEXT,
    FAM_UNBOUND_THREAD_DNSCRYPT_MALFORMED,
    FAM_UNBOUND_THREAD_PREFETCH,
    FAM_UNBOUND_THREAD_EXPIRED,
    FAM_UNBOUND_THREAD_RECURSIVE_REPLIES,
    FAM_UNBOUND_THREAD_REQUESTLIST_MAX,
    FAM_UNBOUND_THREAD_REQUESTLIST_OVERWRITTEN,
    FAM_UNBOUND_THREAD_REQUESTLIST_EXCEEDED,
    FAM_UNBOUND_THREAD_REQUESTLIST_CURRENT_ALL,
    FAM_UNBOUND_THREAD_REQUESTLIST_CURRENT_USER,
    FAM_UNBOUND_THREAD_RECURSION_TIME_AVG,
    FAM_UNBOUND_THREAD_RECURSION_TIME_MEDIAN,
    FAM_UNBOUND_THREAD_TCP_BUFFERS_USAGE,
    FAM_UNBOUND_QUERIES,
    FAM_UNBOUND_QUERIES_IP_RATELIMITED,
    FAM_UNBOUND_CACHE_HITS,
    FAM_UNBOUND_CACHE_MISS,
    FAM_UNBOUND_DNSCRYPT_CRYPTED,
    FAM_UNBOUND_DNSCRYPT_CERT,
    FAM_UNBOUND_DNSCRYPT_CLEARTEXT,
    FAM_UNBOUND_DNSCRYPT_MALFORMED,
    FAM_UNBOUND_PREFETCH,
    FAM_UNBOUND_EXPIRED,
    FAM_UNBOUND_RECURSIVE_REPLIES,
    FAM_UNBOUND_REQUESTLIST_MAX,
    FAM_UNBOUND_REQUESTLIST_OVERWRITTEN,
    FAM_UNBOUND_REQUESTLIST_EXCEEDED,
    FAM_UNBOUND_REQUESTLIST_CURRENT_ALL,
    FAM_UNBOUND_REQUESTLIST_CURRENT_USER,
    FAM_UNBOUND_RECURSION_TIME_AVG,
    FAM_UNBOUND_RECURSION_TIME_MEDIAN,
    FAM_UNBOUND_TCP_BUFFERS_USAGE,
    FAM_UNBOUND_UPTIME_SECONDS,
    FAM_UNBOUND_CACHE_RRSET_BYTES,
    FAM_UNBOUND_CACHE_MESSAGE_BYTES,
    FAM_UNBOUND_CACHE_DNSCRYPT_SHARED_SECRET_BYTES,
    FAM_UNBOUND_CACHE_DNSCRYPT_NONCE_BYTES,
    FAM_UNBOUND_MOD_ITERATOR_BYTES,
    FAM_UNBOUND_MOD_VALIDATOR_BYTES,
    FAM_UNBOUND_MOD_RESPIP_BYTES,
    FAM_UNBOUND_MOD_SUBNET_BYTES,
    FAM_UNBOUND_MOD_IPSEC_BYTES,
    FAM_UNBOUND_STREAM_WAIT_BYTES,
    FAM_UNBOUND_HTTP_QUERY_BUFFER_BYTES,
    FAM_UNBOUND_HTTP_RESPONSE_BUFFER_BYTES,
    FAM_UNBOUND_QUERY_TYPE,
    FAM_UNBOUND_QUERY_OPCODE,
    FAM_UNBOUND_QUERY_CLASS,
    FAM_UNBOUND_QUERY_TCP,
    FAM_UNBOUND_QUERY_TCP_OUT,
    FAM_UNBOUND_QUERY_UDP_OUT,
    FAM_UNBOUND_QUERY_TLS,
    FAM_UNBOUND_QUERY_TLS_RESUME,
    FAM_UNBOUND_QUERY_HTTPS,
    FAM_UNBOUND_QUERY_IPV6,
    FAM_UNBOUND_QUERY_FLAG,
    FAM_UNBOUND_QUERY_EDNS_PRESENT,
    FAM_UNBOUND_QUERY_EDNS_DO,
    FAM_UNBOUND_QUERY_RATELIMITED,
    FAM_UNBOUND_QUERY_DNSCRYPT_SHARED_SECRET_CACHE_MISS,
    FAM_UNBOUND_QUERY_DNSCRYPT_REPLAY,
    FAM_UNBOUND_ANSWER_RCODE,
    FAM_UNBOUND_ANSWER_SECURE,
    FAM_UNBOUND_ANSWER_BOGUS,
    FAM_UNBOUND_RRSET_BOGUS,
    FAM_UNBOUND_UNWANTED_QUERIES,
    FAM_UNBOUND_UNWANTED_REPLIES,
    FAM_UNBOUND_MESAGE_CACHE_SIZE,
    FAM_UNBOUND_RRSET_CACHE_SIZE,
    FAM_UNBOUND_INFRA_CACHE_SIZE,
    FAM_UNBOUND_KEY_CACHE_SIZE,
    FAM_UNBOUND_DNSCRYPT_SHARED_SECRET_CACHE_SIZE,
    FAM_UNBOUND_DNSCRYPT_NONCE_CACHE_SIZE,
    FAM_UNBOUND_QUERY_AUTHZONE_UP,
    FAM_UNBOUND_QUERY_AUTHZONE_DOWN,
    FAM_UNBOUND_QUERY_AGGRESSIVE_NOERROR,
    FAM_UNBOUND_QUERY_AGGRESSIVE_NXDOMAIN,
    FAM_UNBOUND_QUERY_SUBNET,
    FAM_UNBOUND_QUERY_SUBNET_CACHE,
    FAM_UNBOUND_RPZ_ACTION,
    FAM_UNBOUND_MAX,
};

static metric_family_t fams[FAM_UNBOUND_MAX] = {
    [FAM_UNBOUND_UP] = {
        .name = "unbound_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the unbound server be reached.",
    },
    [FAM_UNBOUND_THREAD_QUERIES] = {
        .name = "unbound_thread_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries received by thread.",
    },
    [FAM_UNBOUND_THREAD_QUERIES_IP_RATELIMITED] = {
        .name = "unbound_thread_queries_ip_ratelimited",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries rate limited by thread.",
    },
    [FAM_UNBOUND_THREAD_CACHE_HITS] = {
        .name = "unbound_thread_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that were successfully answered using a cache lookup.",
    },
    [FAM_UNBOUND_THREAD_CACHE_MISS] = {
        .name = "unbound_thread_cache_miss",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that needed recursive processing.",
    },
    [FAM_UNBOUND_THREAD_DNSCRYPT_CRYPTED] = {
        .name = "unbound_thread_dnscrypt_crypted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that were encrypted and successfully decapsulated by dnscrypt.",
    },
    [FAM_UNBOUND_THREAD_DNSCRYPT_CERT] = {
        .name = "unbound_thread_dnscrypt_cert",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that were requesting dnscrypt certificates.",
    },
    [FAM_UNBOUND_THREAD_DNSCRYPT_CLEARTEXT] = {
        .name = "unbound_thread_dnscrypt_cleartext",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries received on dnscrypt port that were cleartext "
                "and not a request for certificates.",
    },
    [FAM_UNBOUND_THREAD_DNSCRYPT_MALFORMED] = {
        .name = "unbound_thread_dnscrypt_malformed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of request that were neither cleartext, not valid dnscrypt messages.",
    },
    [FAM_UNBOUND_THREAD_PREFETCH] = {
        .name = "unbound_thread_prefetch",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of cache prefetches performed.",
    },
    [FAM_UNBOUND_THREAD_EXPIRED] = {
        .name = "unbound_thread_expired",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of replies that served an expired cache entry.",
    },
    [FAM_UNBOUND_THREAD_RECURSIVE_REPLIES] = {
        .name = "unbound_thread_recursive_replies",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of replies sent to queries that needed recursive processing.",
    },
    [FAM_UNBOUND_THREAD_REQUESTLIST_MAX] = {
        .name = "unbound_thread_requestlist_max",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum size attained by the internal recursive processing request list.",
    },
    [FAM_UNBOUND_THREAD_REQUESTLIST_OVERWRITTEN] = {
        .name = "unbound_thread_requestlist_overwritten",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of requests in the request list that were overwritten by newer entries.",
    },
    [FAM_UNBOUND_THREAD_REQUESTLIST_EXCEEDED] = {
        .name = "unbound_thread_requestlist_exceeded",
        .type = METRIC_TYPE_COUNTER,
        .help = "Queries that were dropped because the request list was full.",
    },
    [FAM_UNBOUND_THREAD_REQUESTLIST_CURRENT_ALL] = {
        .name = "unbound_thread_requestlist_current_all",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current size of the request list, includes internally generated queries "
                "(such as priming queries and glue lookups).",
    },
    [FAM_UNBOUND_THREAD_REQUESTLIST_CURRENT_USER] = {
        .name = "unbound_thread_requestlist_current_user",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current size of the request list, only the requests from client queries.",
    },
    [FAM_UNBOUND_THREAD_RECURSION_TIME_AVG] = {
        .name = "unbound_thread_recursion_time_avg",
        .type = METRIC_TYPE_GAUGE,
        .help = "Average time it took to answer queries that needed recursive processing.",
    },
    [FAM_UNBOUND_THREAD_RECURSION_TIME_MEDIAN] = {
        .name = "unbound_thread_recursion_time_median",
        .type = METRIC_TYPE_GAUGE,
        .help = "The median of the time it took to answer queries that needed recursive processing.",
    },
    [FAM_UNBOUND_THREAD_TCP_BUFFERS_USAGE] = {
        .name = "unbound_thread_tcp_buffers_usage",
        .type = METRIC_TYPE_GAUGE,
        .help = "The currently held tcp buffers for incoming connections.",
    },
    [FAM_UNBOUND_QUERIES] = {
        .name = "unbound_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries received.",
    },
    [FAM_UNBOUND_QUERIES_IP_RATELIMITED] = {
        .name = "unbound_queries_ip_ratelimited",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries rate limited.",
    },
    [FAM_UNBOUND_CACHE_HITS] = {
        .name = "unbound_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that were successfully answered using a cache lookup.",
    },
    [FAM_UNBOUND_CACHE_MISS] = {
        .name = "unbound_cache_miss",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that needed recursive processing.",
    },
    [FAM_UNBOUND_DNSCRYPT_CRYPTED] = {
        .name = "unbound_dnscrypt_crypted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that were encrypted and successfully decapsulated by dnscrypt.",
    },
    [FAM_UNBOUND_DNSCRYPT_CERT] = {
        .name = "unbound_dnscrypt_cert",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that were requesting dnscrypt certificates.",
    },
    [FAM_UNBOUND_DNSCRYPT_CLEARTEXT] = {
        .name = "unbound_dnscrypt_cleartext",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries received on dnscrypt port that were cleartext and "
                "not a request for certificates.",
    },
    [FAM_UNBOUND_DNSCRYPT_MALFORMED] = {
        .name = "unbound_dnscrypt_malformed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of request that were neither cleartext, not valid dnscrypt messages.",
    },
    [FAM_UNBOUND_PREFETCH] = {
        .name = "unbound_prefetch",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of cache prefetches performed.",
    },
    [FAM_UNBOUND_EXPIRED] = {
        .name = "unbound_expired",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of replies that served an expired cache entry.",
    },
    [FAM_UNBOUND_RECURSIVE_REPLIES] = {
        .name = "unbound_recursive_replies",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of replies sent to queries that needed recursive processing.",
    },
    [FAM_UNBOUND_REQUESTLIST_MAX] = {
        .name = "unbound_requestlist_max",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum size attained by the internal recursive processing request list.",
    },
    [FAM_UNBOUND_REQUESTLIST_OVERWRITTEN] = {
        .name = "unbound_requestlist_overwritten",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of requests in the request list that were overwritten by newer entries.",
    },
    [FAM_UNBOUND_REQUESTLIST_EXCEEDED] = {
        .name = "unbound_requestlist_exceeded",
        .type = METRIC_TYPE_COUNTER,
        .help = "Queries that were dropped because the request list was full.",
    },
    [FAM_UNBOUND_REQUESTLIST_CURRENT_ALL] = {
        .name = "unbound_requestlist_current_all",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current size of the request list, includes internally generated queries "
                "(such as priming queries and glue lookups).",
    },
    [FAM_UNBOUND_REQUESTLIST_CURRENT_USER] = {
        .name = "unbound_requestlist_current_user",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current size of the request list, only the requests from client queries.",
    },
    [FAM_UNBOUND_RECURSION_TIME_AVG] = {
        .name = "unbound_recursion_time_avg",
        .type = METRIC_TYPE_GAUGE,
        .help = "Average time it took to answer queries that needed recursive processing.",
    },
    [FAM_UNBOUND_RECURSION_TIME_MEDIAN] = {
        .name = "unbound_recursion_time_median",
        .type = METRIC_TYPE_GAUGE,
        .help = "The median of the time it took to answer queries that needed recursive processing.",
    },
    [FAM_UNBOUND_TCP_BUFFERS_USAGE] = {
        .name = "unbound_tcp_buffers_usage",
        .type = METRIC_TYPE_GAUGE,
        .help = "The currently held tcp buffers for incoming connections.",
    },
    [FAM_UNBOUND_UPTIME_SECONDS] = {
        .name = "unbound_uptime_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Uptime since server boot in seconds.",
    },
    [FAM_UNBOUND_CACHE_RRSET_BYTES] = {
        .name = "unbound_cache_rrset_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory in bytes in use by the RRset cache.",
    },
    [FAM_UNBOUND_CACHE_MESSAGE_BYTES] = {
        .name = "unbound_cache_message_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory in bytes in use by the message cache.",
    },
    [FAM_UNBOUND_CACHE_DNSCRYPT_SHARED_SECRET_BYTES] = {
        .name = "unbound_cache_dnscrypt_shared_secret_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory in bytes in use by the dnscrypt shared secrets cache.",
    },
    [FAM_UNBOUND_CACHE_DNSCRYPT_NONCE_BYTES] = {
        .name = "unbound_cache_dnscrypt_nonce_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory in bytes in use by the dnscrypt nonce cache.",
    },
    [FAM_UNBOUND_MOD_ITERATOR_BYTES] = {
        .name = "unbound_mod_iterator_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory in bytes in use by the iterator module.",
    },
    [FAM_UNBOUND_MOD_VALIDATOR_BYTES] = {
        .name = "unbound_mod_validator_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory in bytes in use by the validator module. "
                "Includes the key cache and negative cache.",
    },
    [FAM_UNBOUND_MOD_RESPIP_BYTES] = {
        .name = "unbound_mod_respip_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory in bytes in use by the respip module.",
    },
    [FAM_UNBOUND_MOD_SUBNET_BYTES] = {
        .name = "unbound_mod_subnet_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory in bytes in use by the subnet module.",
    },
    [FAM_UNBOUND_MOD_IPSEC_BYTES] = {
        .name = "unbound_mod_ipsec_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory in bytes in use by the ipsec module.",
    },
    [FAM_UNBOUND_STREAM_WAIT_BYTES] = {
        .name = "unbound_stream_wait_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory in bytes in used by the TCP and TLS stream wait buffers. "
                "These are answers waiting to be written back to the clients.",
    },
    [FAM_UNBOUND_HTTP_QUERY_BUFFER_BYTES] = {
        .name = "unbound_http_query_buffer_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory in bytes used by the HTTP/2 query buffers. Containing (partial) DNS "
                "queries waiting for request stream completion.",
    },
    [FAM_UNBOUND_HTTP_RESPONSE_BUFFER_BYTES] = {
        .name = "unbound_http_response_buffer_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory in bytes used by the HTTP/2 response buffers. Containing DNS responses "
                "waiting to be written back to the clients.",
    },
    [FAM_UNBOUND_QUERY_TYPE] = {
        .name = "unbound_query_type",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries with this query type.",
    },
    [FAM_UNBOUND_QUERY_OPCODE] = {
        .name = "unbound_query_opcode",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries with this opcode.",
    },
    [FAM_UNBOUND_QUERY_CLASS] = {
        .name = "unbound_query_class",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries with this query class.",
    },
    [FAM_UNBOUND_QUERY_TCP] = {
        .name = "unbound_query_tcp",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that were made using TCP towards the Unbound server.",
    },
    [FAM_UNBOUND_QUERY_TCP_OUT] = {
        .name = "unbound_query_tcp_out",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that the Unbound server made using TCP "
                "outgoing towards other servers.",
    },
    [FAM_UNBOUND_QUERY_UDP_OUT] = {
        .name = "unbound_query_udp_out",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that the Unbound server made using UDP "
                "outgoing towards other servers.",
    },
    [FAM_UNBOUND_QUERY_TLS] = {
        .name = "unbound_query_tls",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that were made using TLS towards the Unbound server.",
    },
    [FAM_UNBOUND_QUERY_TLS_RESUME] = {
        .name = "unbound_query_tls_resume",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of TLS session resumptions, these are queries over TLS towards "
                "the Unbound server where the client negotiated a TLS session resumption key.",
    },
    [FAM_UNBOUND_QUERY_HTTPS] = {
        .name = "unbound_query_https",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that were made using HTTPS towards the Unbound server.",
    },
    [FAM_UNBOUND_QUERY_IPV6] = {
        .name = "unbound_query_ipv6",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that were made using IPv6 towards the Unbound server.",
    },
    [FAM_UNBOUND_QUERY_FLAG] = {
        .name = "unbound_query_flag",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of queries that had the RD flag set in the header.",
    },
    [FAM_UNBOUND_QUERY_EDNS_PRESENT] = {
        .name = "unbound_query_edns_present",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that had an EDNS OPT record present.",
    },
    [FAM_UNBOUND_QUERY_EDNS_DO] = {
        .name = "unbound_query_edns_do",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that had an EDNS OPT record with the DO (DNSSEC OK) bit set.",
    },
    [FAM_UNBOUND_QUERY_RATELIMITED] = {
        .name = "unbound_query_ratelimited",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of queries that are turned away from being send to nameserver "
                "due to ratelimiting.",
    },
    [FAM_UNBOUND_QUERY_DNSCRYPT_SHARED_SECRET_CACHE_MISS] = {
        .name = "unbound_query_dnscrypt_shared_secret_cache_miss",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of dnscrypt queries that did not find a shared secret in the cache.",
    },
    [FAM_UNBOUND_QUERY_DNSCRYPT_REPLAY] = {
        .name = "unbound_query_dnscrypt_replay",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of dnscrypt queries that found a nonce hit in the nonce cache and "
                "hence are considered a query replay.",
    },
    [FAM_UNBOUND_ANSWER_RCODE] = {
        .name = "unbound_answer_rcode",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of answers to queries, from cache or from recursion, "
                "that had this return code",
    },
    [FAM_UNBOUND_ANSWER_SECURE] = {
        .name = "unbound_answer_secure",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of answers that were secure. The answer validated correctly.",
    },
    [FAM_UNBOUND_ANSWER_BOGUS] = {
        .name = "unbound_answer_bogus",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of answers that were bogus.",
    },
    [FAM_UNBOUND_RRSET_BOGUS] = {
        .name = "unbound_rrset_bogus",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rrsets marked bogus by the validator.",
    },
    [FAM_UNBOUND_UNWANTED_QUERIES] = {
        .name = "unbound_unwanted_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that were refused or dropped because they failed "
                "the access control settings.",
    },
    [FAM_UNBOUND_UNWANTED_REPLIES] = {
        .name = "unbound_unwanted_replies",
        .type = METRIC_TYPE_COUNTER,
        .help = "Replies that were unwanted or unsolicited.",
    },
    [FAM_UNBOUND_MESAGE_CACHE_SIZE] = {
        .name = "unbound_mesage_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of items (DNS replies) in the message cache.",
    },
    [FAM_UNBOUND_RRSET_CACHE_SIZE] = {
        .name = "unbound_rrset_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of RRsets in the rrset cache.",
    },
    [FAM_UNBOUND_INFRA_CACHE_SIZE] = {
        .name = "unbound_infra_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of items in the infra cache.",
    },
    [FAM_UNBOUND_KEY_CACHE_SIZE] = {
        .name = "unbound_key_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of items in the key cache.",
    },
    [FAM_UNBOUND_DNSCRYPT_SHARED_SECRET_CACHE_SIZE] = {
        .name = "unbound_dnscrypt_shared_secret_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "The  number of items in the shared secret cache.",
    },
    [FAM_UNBOUND_DNSCRYPT_NONCE_CACHE_SIZE] = {
        .name = "unbound_dnscrypt_nonce_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of items in the client nonce cache.",
    },
    [FAM_UNBOUND_QUERY_AUTHZONE_UP] = {
        .name = "unbound_query_authzone_up",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of queries answered from auth-zone data, upstream queries.",
    },
    [FAM_UNBOUND_QUERY_AUTHZONE_DOWN] = {
        .name = "unbound_query_authzone_down",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of queries for downstream answered from auth-zone data.",
    },
    [FAM_UNBOUND_QUERY_AGGRESSIVE_NOERROR] = {
        .name = "unbound_query_aggressive_noerror",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of queries answered using cached NSEC records with NODATA RCODE.",
    },
    [FAM_UNBOUND_QUERY_AGGRESSIVE_NXDOMAIN] = {
        .name = "unbound_query_aggressive_nxdomain",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of queries answered using cached NSEC records with NXDOMAIN RCODE.",
    },
    [FAM_UNBOUND_QUERY_SUBNET] = {
        .name = "unbound_query_subnet",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that got an answer that contained EDNS client subnet data.",
    },
    [FAM_UNBOUND_QUERY_SUBNET_CACHE] = {
        .name = "unbound_query_subnet_cache",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries answered from the edns client subnet cache.",
    },
    [FAM_UNBOUND_RPZ_ACTION] = {
        .name = "unbound_rpz_action",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries answered using configured RPZ policy, per RPZ action type.",
    },
};

#include "plugins/unbound/unbound.h"

typedef struct {
    char *name;
    char *host;
    int port;
    char *socketpath;
    char *server_cert_file;
    char *control_key_file;
    char *control_cert_file;
    cdtime_t timeout;
    label_set_t labels;
    plugin_filter_t *filter;
    metric_family_t fams[FAM_UNBOUND_MAX];
} unbound_t;

static int unbound_parse_metric(unbound_t *unbound, char *line)
{
    char *value = strchr(line, '=');
    if (value == NULL)
        return -1;
    *value = '\0';
    value++;
    if (*value == '\0')
        return -1;

    char *key = line;
    size_t key_len = strlen(key);

    char buffer[64];
    char *lkey = NULL;
    char *lvalue = NULL;

    if (key[0] == 't') {
        if ((key_len > 6) && (memcmp("thread", key, 6) == 0)) {
            lkey = "thread";
            lvalue = key + 6;
            char *key1 = strchr(key, '.');
            if (key1 == NULL)
                return -1;
            *key1 = '\0';
            key1++;
            memcpy(buffer, "thread.", 7);
            sstrncpy(buffer + 7, key1, sizeof(buffer) - 7);
            key_len = strlen(buffer);
            key =  buffer;
        }
    } else if (key[0] == 'n') {
        if ((key_len > 14) && (memcmp("num.query.type", key, 14) == 0)) {
            key[14] = '\0';
            key_len = 14;
            lkey = "type";
            lvalue = key + 15;
        } else if ((key_len > 14) && (memcmp("num.rpz.action", key, 14) == 0)) {
            key[14] = '\0';
            key_len = 14;
            lkey = "action";
            lvalue = key + 15;
        } else if ((key_len > 15) && (memcmp("num.query.class", key, 15) == 0)) {
            key[15] = '\0';
            key_len = 15;
            lkey = "class";
            lvalue = key + 16;
        } else if ((key_len > 15) && (memcmp("num.query.flags", key, 15) == 0)) {
            key[15] = '\0';
            key_len = 15;
            lkey = "flag";
            lvalue = key + 16;
        } else if ((key_len > 16) && (memcmp("num.query.opcode", key, 16) == 0)) {
            key[16] = '\0';
            key_len = 16;
            lkey = "opcode";
            lvalue = key + 17;
        } else if ((key_len > 16) && (memcmp("num.answer.rcode", key, 16) == 0)) {
            key[16] = '\0';
            key_len = 16;
            lkey = "rcode";
            lvalue = key + 17;
        }
    }

#if 0
histogram.<sec>.<usec>.to.<sec>.<usec>
    Shows a histogram, summed over all threads. Every element counts the recursive queries whose reply time fit between the lower and upper bound.  Times larger or equal to the lowerbound, and smaller than the upper bound.  There are 40 buckets, with bucket sizes doubling.
#endif

    const struct unbound_metric *um = unbound_get_key(key, key_len);
    if (um == NULL)
        return 0;

    if (um->fam < 0)
        return 0;

    value_t mvalue = {0};

    if (unbound->fams[um->fam].type == METRIC_TYPE_COUNTER) {
        mvalue = VALUE_COUNTER(atoll(value));
    } else if (unbound->fams[um->fam].type == METRIC_TYPE_GAUGE) {
        mvalue = VALUE_GAUGE(atof(value));
    } else {
        return 0;
    }

    if ((lkey != NULL) && (lvalue != NULL)) {
        metric_family_append(&unbound->fams[um->fam], mvalue, &unbound->labels,
                             &(label_pair_const_t){.name=lkey, .value=lvalue}, NULL);
    } else {
        metric_family_append(&unbound->fams[um->fam], mvalue, &unbound->labels, NULL);
    }

    return 0;
}

static void unbound_free(void *data)
{
    unbound_t *unbound = data;

    if (unbound == NULL)
        return;

    free(unbound->name);
    free(unbound->host);
    free(unbound->socketpath);
    free(unbound->server_cert_file);
    free(unbound->control_key_file);
    free(unbound->control_cert_file);
    label_set_reset(&unbound->labels);
    plugin_filter_free(unbound->filter);

    free(unbound);
}

static int unbound_read_ssl(unbound_t *unbound)
{
    int fd = -1;
    SSL *ssl = NULL;
    int status = 0;

    const SSL_METHOD *method = SSLv23_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (ctx == NULL) {
        PLUGIN_ERROR("Unable to create a new SSL context structure.");
        return -1;
    }

    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);

    status = SSL_CTX_use_certificate_file(ctx, unbound->control_cert_file, SSL_FILETYPE_PEM);
    if(status != 1) {
        PLUGIN_ERROR("Error setting up SSL_CTX client cert for '%s'.", unbound->control_cert_file);
        status = -1;
        goto error;
    }

    status = SSL_CTX_use_PrivateKey_file(ctx, unbound->control_key_file, SSL_FILETYPE_PEM);
    if(status != 1) {
        PLUGIN_ERROR("Error setting up SSL_CTX client key for '%s'", unbound->control_key_file);
        status = -1;
        goto error;
    }

    status = SSL_CTX_check_private_key(ctx);
    if(status != 1) {
        PLUGIN_ERROR("Error setting up SSL_CTX client key");
        status = -1;
        goto error;
    }

    status = SSL_CTX_load_verify_locations(ctx, unbound->server_cert_file, NULL);
    if (status != 1) {
        PLUGIN_ERROR("Error setting up SSL_CTX verify, server cert for '%s'", unbound->server_cert_file);
        goto error;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    ssl = SSL_new(ctx);
    if (ssl == NULL) {
        PLUGIN_ERROR("SSL_new failed.");
        status = -1;
        goto error;
    }
    SSL_set_connect_state(ssl);

    fd = socket_connect_tcp(unbound->host, unbound->port, 0, 0);
    if (fd < 0) {
        status = -1;
        goto error;
    }

    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
    SSL_set_fd(ssl, fd);
    status = SSL_do_handshake(ssl);
    if (status != 1) {
        PLUGIN_ERROR("SSL handshake failed.");
        unsigned long err = ERR_peek_error();
        PLUGIN_ERROR("could not SSL_do_handshake %d %s", fd, ERR_reason_error_string(err) );
        status = -1;
        goto error;
    }

    if (SSL_get_verify_result(ssl) != X509_V_OK) {
        PLUGIN_ERROR("SSL handshake failed.");
        status = -1;
        goto error;
    }

    X509 *cert = SSL_get_peer_certificate(ssl);
    if (cert == NULL) {
        PLUGIN_ERROR("SSL server presented no peer certificate");
        status = -1;
        goto error;
    }
    X509_free(cert);

    const char *command = "UBCT1 stats_noreset\n";
    int command_len = strlen(command);

    status = SSL_write(ssl, command, command_len);
    if (status <= 0) {
        unsigned long  err = ERR_peek_error();
        PLUGIN_ERROR("could not SSL_write %s", ERR_reason_error_string(err) );
        status = -1;
        goto error;
    }
#if 0
    BIO *rbio = SSL_get_rbio(ssl);
    if (rbio == NULL) {
        PLUGIN_ERROR("SSL_get_rbio failed");
        goto error;
    }

    BIO *bbio = BIO_new(BIO_f_buffer());
    if (bbio == NULL) {
        PLUGIN_ERROR("BIO_new failed");
        goto error;
    }

    BIO_push(bbio, rbio);

    while (true) {
        char buffer[256];
        status = BIO_gets(bbio, buffer, sizeof(buffer));
        if (status == 0)
            break;
        if (status < 0) {
            fprintf(stderr,  "*** %d\n", status);
            break;
        }
        fprintf(stderr, "[%s]\n", buffer);
    }
#endif

    char buffer[1024];
    while (true) {
        int buffer_size = sizeof(buffer);
        int rsize = SSL_read(ssl, buffer, buffer_size - 1);
        if (rsize == 0)
            break;
        if (rsize <= 0) {
            unsigned long err = ERR_peek_error();
            PLUGIN_ERROR("could not SSL_read %d %s", rsize,  ERR_reason_error_string(err) );
            break;
        }
        buffer[rsize] = '\0';

        unbound_parse_metric(unbound, buffer);
    }

    status = 0;

error:
    if (ssl != NULL)
        SSL_free(ssl);
    if (fd >= 0)
        close(fd);
    if (ctx != NULL)
        SSL_CTX_free(ctx);

    return status;
}

static int unbound_read_stream(unbound_t *unbound)
{
    int fd = -1;

    if (unbound->socketpath != NULL)
        fd = socket_connect_unix_stream(unbound->socketpath, unbound->timeout);
    else
        fd = socket_connect_tcp(unbound->host, unbound->port, 0, 0);

    if (fd < 0)
        return -1;

    const char *command = "UBCT1 stats_noreset\n";
    int command_len = strlen(command);
    if (send(fd, command, command_len, 0) < command_len) {
        PLUGIN_ERROR("unix socket send command failed");
        close(fd);
        return -1;
    }

    FILE *fh = fdopen(fd, "r");
    if (fh == NULL) {
        close(fd);
        return -1;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fh) != NULL)
        unbound_parse_metric(unbound, buffer);

    fclose(fh);

    return 0;
}

static int unbound_read(user_data_t *user_data)
{
    unbound_t *unbound = user_data->data;
    if (unbound == NULL)
        return  -1;

    int status = 0;
    if (unbound->socketpath != NULL)
        status = unbound_read_stream(unbound);
    else if (unbound->control_key_file == NULL)
        status = unbound_read_stream(unbound);
    else
        status = unbound_read_ssl(unbound);

    metric_family_append(&unbound->fams[FAM_UNBOUND_UP], VALUE_GAUGE(status == 0 ? 1 : 0),
                         &unbound->labels, NULL);

    plugin_dispatch_metric_family_array_filtered(unbound->fams, FAM_UNBOUND_MAX, unbound->filter, 0);

    return 0;
}

static int unbound_config_instance (config_item_t *ci)
{
    unbound_t *unbound = calloc(1, sizeof(*unbound));
    if (unbound == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    memcpy(unbound->fams, fams, sizeof(unbound->fams[0])*FAM_UNBOUND_MAX);

    int status = cf_util_get_string(ci, &unbound->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(unbound);
        return status;
    }

    unbound->port = UNBOUND_CONTROL_PORT;

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &unbound->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_port_number(child, &unbound->port);
        } else if (strcasecmp("socket-path", child->key) == 0) {
            status = cf_util_get_string(child, &unbound->socketpath);
        } else if (strcasecmp("server-cert", child->key) == 0) {
            status = cf_util_get_string(child, &unbound->server_cert_file);
        } else if (strcasecmp("control-key", child->key) == 0) {
            status = cf_util_get_string(child, &unbound->control_key_file);
        } else if (strcasecmp("control-cert", child->key) == 0) {
            status = cf_util_get_string(child, &unbound->control_cert_file);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &unbound->timeout);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &unbound->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &unbound->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        unbound_free(unbound);
        return -1;
    }

    if ((unbound->host == NULL) && (unbound->socketpath == NULL)) {
        PLUGIN_ERROR("Missing 'host' or 'socket-path' option.");
        unbound_free(unbound);
        return -1;
    }

    if ((unbound->host != NULL) && (strcmp(unbound->host, "::1") != 0) &&
        (strcmp(unbound->host, "127.0.0.1") != 0) && (strcmp(unbound->host, "localhost") != 0)) {

        if (unbound->server_cert_file == NULL) {
            PLUGIN_ERROR("Missing 'server-cert' option.");
            unbound_free(unbound);
            return -1;
        }

        if (unbound->control_key_file == NULL) {
            PLUGIN_ERROR("Missing 'control-key' option.");
            unbound_free(unbound);
            return -1;
        }

        if (unbound->control_cert_file == NULL) {
            PLUGIN_ERROR("Missing 'control-cert' option");
            unbound_free(unbound);
            return -1;
        }
    }

    label_set_add(&unbound->labels, true, "instance", unbound->name);

    return plugin_register_complex_read("unbound", unbound->name, unbound_read, interval,
                                        &(user_data_t){.data = unbound, .free_func = unbound_free});
}

static int unbound_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = unbound_config_instance(child);
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

static int unbound_init(void)
{
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
    SSL_load_error_strings();

    if (SSL_library_init() < 0) {
        PLUGIN_ERROR("Could not initialize the OpenSSL library.");
        return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("unbound", unbound_config);
    plugin_register_init("unbound", unbound_init);
}
