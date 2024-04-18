// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2009 Bruno Prémont
// SPDX-FileCopyrightText: Copyright (C) 2009,2010 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Bruno Prémont <bonbons at linux-vserver.org>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "config.h"

#if defined(STRPTIME_NEEDS_STANDARDS) || defined(HAVE_STRPTIME_STANDARDS)
#    ifndef _ISOC99_SOURCE
#        define _ISOC99_SOURCE 1
#    endif
#    ifndef _POSIX_C_SOURCE
#        define _POSIX_C_SOURCE 200809L
#    endif
#    ifndef _XOPEN_SOURCE
#        define _XOPEN_SOURCE 500
#    endif
#endif

#ifdef TIMEGM_NEEDS_DEFAULT
#    ifndef _DEFAULT_SOURCE
#        define _DEFAULT_SOURCE 1
#    endif
#elif defined(TIMEGM_NEEDS_BSD)
#    ifndef _BSD_SOURCE
#        define _BSD_SOURCE 1
#    endif
#endif

#include "plugin.h"
#include "libutils/common.h"

#include "plugins/bind/bind_fams.h"
#include "plugins/bind/bind_xml.h"
#include "plugins/bind/bind_json.h"

#include <time.h>

#include <curl/curl.h>

#ifndef BIND_DEFAULT_URL
#define BIND_DEFAULT_URL "http://localhost:8053/"
#endif


static metric_family_t fams_bind[FAM_BIND_MAX] = {
    [FAM_BIND_UP] = {
        .name = "bind_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the bind server be reached."
    },
    [FAM_BIND_BOOT_TIME_SECONDS] = {
        .name = "bind_boot_time_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Start time of the BIND process since unix epoch in seconds.",
    },
    [FAM_BIND_CONFIG_TIME_SECONDS] = {
        .name = "bind_config_time_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Time of the last reconfiguration since unix epoch in seconds.",
    },
    [FAM_BIND_INCOMING_QUERIES] = {
        .name = "bind_incoming_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of incoming DNS queries.",
    },
    [FAM_BIND_INCOMING_REQUESTS] = {
        .name = "bind_incoming_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of incoming DNS requests.",
    },
    [FAM_BIND_RESPONSE_RCODES] = {
        .name = "bind_response_rcodes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses sent per RCODE.",
    },
    [FAM_BIND_INCOMING_QUERIES_UDP] = {
        .name = "bind_incoming_queries_udp",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of incoming UDP queries.",
    },
    [FAM_BIND_INCOMING_QUERIES_TCP] = {
        .name = "bind_incoming_queries_tcp",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of incoming TCP queries.",
    },
    [FAM_BIND_INCOMING_REQUESTS_TCP] = {
        .name = "bind_incoming_requests_tcp",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of incoming TCP requests.",
    },
    [FAM_BIND_QUERY_DUPLICATES] = {
        .name = "bind_query_duplicates",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of duplicated queries received.",
    },
    [FAM_BIND_QUERY_RECURSIONS] = {
        .name = "bind_query_recursions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries causing recursion.",
    },
    [FAM_BIND_QUERY_ERRORS] = {
        .name = "bind_query_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of query failures",
    },
    [FAM_BIND_RECURSIVE_CLIENTS] = {
        .name = "bind_recursive_clients",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of current recursive clients."
    },
    [FAM_BIND_RESPONSES] = {
        .name = "bind_responses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of responses sent",
    },
    [FAM_BIND_TASKS_RUNNING] = {
        .name = "bind_tasks_running",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of running tasks."
    },
    [FAM_BIND_WORKER_THREADS] = {
        .name = "bind_worker_threads",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of available worker threads."
    },
    [FAM_BIND_ZONE_TRANSFER_FAILURE] = {
        .name = "bind_zone_transfer_failure",
        .type = METRIC_TYPE_COUNTER,
        .help =  "Number of failed zone transfers.",
    },
    [FAM_BIND_ZONE_TRANSFER_REJECTED] = {
        .name = "bind_zone_transfer_rejected",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rejected zone transfers.",
    },
    [FAM_BIND_ZONE_TRANSFER_SUCCESS] = {
        .name = "bind_zone_transfer_success",
        .type = METRIC_TYPE_COUNTER,
        .help =  "Number of successful zone transfers."
    },
    [FAM_BIND_RESOLVER_QUERIES] = {  // resolver qtype "type", "x"
        .name = "bind_resolver_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of failed resolver queries.",
    },
    [FAM_BIND_RESOLVER_QUERY_ERRORS] = {
        .name = "bind_resolver_query_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of outgoing DNS queries.",
    },
    [FAM_BIND_RESOLVER_RESPONSE_ERRORS] = {
        .name = "bind_resolver_resolver_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of resolver response errors received.",
    },
    [FAM_BIND_RESOLVER_QUERY_EDNS0_ERRORS] = {
        .name = "bind_resolver_query_edns0_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of EDNS(0) query errors.",
    },
    [FAM_BIND_RESOLVER_DNSSEC_VALIDATION_SUCCESS] = {
        .name = "bind_resolver_dnssec_validation_success",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of successful DNSSEC validation attempts.",
    },
    [FAM_BIND_RESOLVER_DNSSEC_VALIDATION_ERRORS] = {
        .name = "bind_resolver_dnssec_validation_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of DNSSEC validation attempt errors.",
    },
    [FAM_BIND_RESOLVER_RESPONSE_MISMATCH] = {
        .name = "bind_resolver_response_mismatch",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of mismatch responses received.",
    },
    [FAM_BIND_RESOLVER_RESPONSE_TRUNCATED] = {
        .name = "bind_resolver_response_truncated",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of truncated responses received.",
    },
    [FAM_BIND_RESOLVER_RESPONSE_LAME] = {
        .name = "bind_resolver_response_lame",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of lame delegation responses received.",
    },
    [FAM_BIND_RESOLVER_QUERY_RETRIES] = {
        .name = "bind_resolver_query_retries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of resolver query retries.",
    },
    [FAM_BIND_RESOLVER_CACHE_RRSETS] = {
        .name = "bind_resolver_cache_rrsets",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of RRsets in cache database.",
    },
    [FAM_BIND_RESOLVER_CACHE_HITS] = {
        .name = "bind_resolver_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of cache hits.",
    },
    [FAM_BIND_RESOLVER_CACHE_MISSES] = {
        .name = "bind_resolver_cache_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of cache misses.",
    },
    [FAM_BIND_RESOLVER_CACHE_QUERY_HITS] = {
        .name = "bind_resolver_cache_query_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of queries that were answered from cache.",
    },
    [FAM_BIND_RESOLVER_CACHE_QUERY_MISSES] = {
        .name = "bind_resolver_cache_query_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of queries that were not in cache.",
    },
    [FAM_BIND_RESOLVER_QUERY_DURATION_SECONDS] = {
        .name = "bind_resolver_query_duration_seconds",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "Resolver query round-trip time in seconds."
    },
    [FAM_BIND_TRAFFIC_INCOMING_REQUESTS_UDP4_SIZE] = {
        .name = "bind_traffic_incoming_requests_udp4_size",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "Size of DNS requests (UDP/IPv4).",
    },
    [FAM_BIND_TRAFFIC_INCOMING_REQUESTS_UDP6_SIZE] = {
        .name = "bind_traffic_incoming_requests_udp6_size",
        .type = METRIC_TYPE_HISTOGRAM,
        .help =  "Size of DNS requests (UDP/IPv6).",
    },
    [FAM_BIND_TRAFFIC_INCOMING_REQUESTS_TCP4_SIZE] = {
        .name = "bind_traffic_incoming_requests_tcp4_size",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "Size of DNS requests (TCP/IPv4).",
    },
    [FAM_BIND_TRAFFIC_INCOMING_REQUESTS_TCP6_SIZE] = {
        .name = "bind_traffic_incoming_requests_tcp6_size",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "Size of DNS requests (TCP/IPv6).",
    },
    [FAM_BIND_TRAFFIC_INCOMING_REQUESTS_TOTAL_SIZE] = {
        .name = "bind_traffic_incoming_requests_total_size",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "Size of DNS requests (any transport).",
    },
    [FAM_BIND_TRAFFIC_RESPONSES_UDP4_SIZE] = {
        .name = "bind_traffic_responses_udp4_size",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "Size of DNS responses (UDP/IPv4).",
    },
    [FAM_BIND_TRAFFIC_RESPONSES_UDP6_SIZE] = {
        .name = "bind_traffic_responses_udp6_size",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "Size of DNS responses (UDP/IPv6).",
    },
    [FAM_BIND_TRAFFIC_RESPONSES_TCP4_SIZE] = {
        .name = "bind_traffic_responses_tcp4_size",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "Size of DNS responses (TCP/IPv4).",
    },
    [FAM_BIND_TRAFFIC_RESPONSES_TCP6_SIZE] = {
        .name = "bind_traffic_responses_tcp6_size",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "Size of DNS responses (TCP/IPv6).",
    },
    [FAM_BIND_TRAFFIC_RESPONSES_TOTAL_SIZE] = {
        .name = "bind_traffic_responses_total_size",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "Size of DNS responses (any transport).",
    },
    [FAM_BIND_MEMORY_TOTAL_USE_BYTES] = {
        .name = "bind_memory_total_use_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_BIND_MEMORY_IN_USE_BYTES] = {
        .name = "bind_memory_in_use_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_BIND_MEMORY_MALLOCED_BYTES] = {
        .name = "bind_memory_malloced_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_BIND_MEMORY_CONTEXT_SIZE_BYTES] = {
        .name = "bind_memory_context_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_BIND_MEMORY_LOST_BYTES] = {
        .name = "bind_memory_lost_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_BIND_SOCKET_ACCEPT] = {
        .name = "bind_socket_accept",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of sockets opened successfully.",
    },
    [FAM_BIND_SOCKET_ACCEPT_FAIL] = {
        .name = "bind_socket_accept_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of failures to accept incoming connection requests.",
    },
    [FAM_BIND_SOCKET_ACTIVE] = {
        .name = "bind_socket_active",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total sockets active.",
    },
    [FAM_BIND_SOCKET_BIND_FAIL] = {
        .name = "bind_socket_bind_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of failures to bind sockets.",
    },
    [FAM_BIND_SOCKET_CLOSE] = {
        .name = "bind_socket_close",
        .type = METRIC_TYPE_COUNTER,
        .help = "This indicates the number of closed sockets.",
    },
    [FAM_BIND_SOCKET_CONNECT] = {
        .name = "bind_socket_connect",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of connections established successfully.",
    },
    [FAM_BIND_SOCKET_CONNECT_FAIL] = {
        .name = "bind_socket_connect_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of failures to connect sockets.",
    },
    [FAM_BIND_SOCKET_OPEN] = {
        .name = "bind_socket_open",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of sockets opened successfully.",
    },
    [FAM_BIND_SOCKET_OPEN_FAIL] = {
        .name = "bind_socket_open_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of failures to open sockets.",
    },
    [FAM_BIND_SOCKET_RECV_ERROR] = {
        .name = "bind_socket_recv_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of errors in socket receive operations, including errors of "
                "send operations on a connected UDP socket, notified by an ICMP error message.",
    },
    [FAM_BIND_SOCKET_SEND_ERROR] = {
        .name = "bind_socket_send_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "This indicates the number of errors in socket send operations.",
    },
};


typedef enum {
    BIND_FORMAT_NONE,
    BIND_FORMAT_XML,
    BIND_FORMAT_JSON,
} bind_format_t;

typedef struct {
    char *instance;
    char *url;
    int timeout;
    label_set_t labels;

    CURL *curl;
    char bind_curl_error[CURL_ERROR_SIZE];

    metric_family_t fams[FAM_BIND_MAX];

    bind_format_t fmt;
    union {
        bind_xml_ctx_t ctx_xml;
        bind_json_ctx_t ctx_json;
    };
} bind_instance_t;

struct bind_metric { char *key; int fam; char *lkey; char *lvalue; };
const struct bind_metric *bind_get_key (register const char *str, register size_t len);

static int bind_traffic_histograms(metric_family_t *fams, label_set_t *labels, histogram_t **ah)
{
    if (ah == NULL)
        return -1;

    struct {
        size_t traffic;
        size_t fam;
    } tf[8] = {
        { BIND_TRAFFIC_INCOMING_REQUESTS_UDP4_SIZE, FAM_BIND_TRAFFIC_INCOMING_REQUESTS_UDP4_SIZE },
        { BIND_TRAFFIC_INCOMING_REQUESTS_UDP6_SIZE, FAM_BIND_TRAFFIC_INCOMING_REQUESTS_UDP6_SIZE },
        { BIND_TRAFFIC_INCOMING_REQUESTS_TCP4_SIZE, FAM_BIND_TRAFFIC_INCOMING_REQUESTS_TCP4_SIZE },
        { BIND_TRAFFIC_INCOMING_REQUESTS_TCP6_SIZE, FAM_BIND_TRAFFIC_INCOMING_REQUESTS_TCP6_SIZE },
        { BIND_TRAFFIC_RESPONSES_UDP4_SIZE, FAM_BIND_TRAFFIC_RESPONSES_UDP4_SIZE },
        { BIND_TRAFFIC_RESPONSES_UDP6_SIZE, FAM_BIND_TRAFFIC_RESPONSES_UDP6_SIZE },
        { BIND_TRAFFIC_RESPONSES_TCP4_SIZE, FAM_BIND_TRAFFIC_RESPONSES_TCP4_SIZE },
        { BIND_TRAFFIC_RESPONSES_TCP6_SIZE, FAM_BIND_TRAFFIC_RESPONSES_TCP6_SIZE },
    };

    for (size_t i = 0; i < 8; i++) {
        histogram_t *h = ah[tf[i].traffic];
        if (h == NULL)
            continue;

        h->sum = NAN;
        uint64_t total = 0;
        for (size_t j = 1; j < h->num; j++) {
            total += h->buckets[j].counter;
            h->buckets[j].counter = total;
        }
        total += h->buckets[0].counter;
        h->buckets[0].counter = total;

        metric_family_append(&fams[tf[i].fam], VALUE_HISTOGRAM(h), labels, NULL);
        histogram_destroy(ah[i]);
        ah[i] = NULL;
    }

    return 0;
}

int bind_traffic_histogram_append(histogram_t **rh, const char *maximum, const char *counter)
{
    histogram_t *h = *rh;

    if (h == NULL) {
        h = histogram_new();
        *rh = h;
    }

    if (h == NULL)
        return -1;

    double max = 0.0;
    size_t maximum_len = strlen(maximum);
    if (maximum[maximum_len-1] == '+') {
        max = INFINITY;
    } else {
        char *c = strchr(maximum, '-');
        if (c == NULL)
            return -1;
        *c = '\0';
        max = atof(maximum);
    }

    uint64_t cnt = atoll(counter);
    h = histogram_bucket_append(h, max, cnt);
    if (h == NULL)
        return -1;

    *rh = h;

    return 0;
}

time_t bind_get_timestamp(const char *str, size_t len)
{
    char buf[64];

    sstrnncpy(buf, sizeof(buf), str, len);

    struct tm tm = {0};
    char *tmp = strptime(buf, "%Y-%m-%dT%T", &tm);
    if (tmp == NULL) {
        PLUGIN_ERROR("strptime failed.");
        return -1;
    }

    time_t t = 0;

#ifdef HAVE_TIMEGM
    t = timegm(&tm);
    if (t == ((time_t)-1)) {
        PLUGIN_ERROR("timegm() failed: %s", STRERRNO);
        return -1;
    }
#else
    t = mktime(&tm);
    if (t == ((time_t)-1)) {
        PLUGIN_ERROR("mktime() failed: %s", STRERRNO);
        return -1;
    }
    /* mktime assumes that tm is local time. Luckily, it also sets timezone to
     * the offset used for the conversion, and we undo the conversion to convert
     * back to UTC. */
    t = t - timezone;
#endif

    return t;
}

int bind_append_metric(metric_family_t *fams, label_set_t *labels,
                       const char *prefix, const char *name,
                       const char *lkey, const char *lvalue, const char *value)
{
    char key[256];

    size_t prefix_len = strlen(prefix);
    if (prefix_len >= sizeof(key))
        return 0;
    sstrnncpy(key, sizeof(key), prefix, prefix_len);
    sstrncpy(key + prefix_len, name, sizeof(key) - prefix_len);

    const struct bind_metric *bm = bind_get_key(key, strlen(key));
    if (bm == NULL)
        return 0;

    if (bm->fam < 0)
        return 0;

    value_t mvalue = {0};

    if (fams[bm->fam].type == METRIC_TYPE_COUNTER) {
        mvalue = VALUE_COUNTER(atoll(value));
    } else if (fams[bm->fam].type == METRIC_TYPE_GAUGE) {
        mvalue = VALUE_GAUGE(atof(value));
    } else {
        return 0;
    }

    if ((bm->lkey != NULL) && (bm->lvalue != NULL)) {
        if ((lkey != NULL) && (lvalue != NULL)) {
            metric_family_append(&fams[bm->fam], mvalue, labels,
                                 &(label_pair_const_t){.name=lkey, .value=lvalue},
                                 &(label_pair_const_t){.name=bm->lkey, .value=bm->lvalue}, NULL);
        } else {
            metric_family_append(&fams[bm->fam], mvalue, labels,
                                 &(label_pair_const_t){.name=bm->lkey, .value=bm->lvalue}, NULL);
        }
    } else {
        if ((lkey != NULL) && (lvalue != NULL)) {
            metric_family_append(&fams[bm->fam], mvalue, labels,
                                 &(label_pair_const_t){.name=lkey, .value=lvalue}, NULL);
        } else {
            metric_family_append(&fams[bm->fam], mvalue, labels, NULL);
        }
    }

    return 0;
}

size_t bind_header_callback(char *buffer, size_t size, size_t nitems, void *user_data)
{
    bind_instance_t *bi = user_data;
    if (bi == NULL) {
        PLUGIN_ERROR("user_data pointer is NULL.");
        return 0;
    }

    size_t len = size * nitems;
    if (len == 0)
        return 0;

    const char *content_type = "Content-Type: ";
    const size_t content_type_len = strlen(content_type);
    if (len <= content_type_len)
        return len;

    if (strncasecmp(buffer, content_type, content_type_len) == 0) {
        buffer += content_type_len;
        size_t rlen = len - content_type_len;

        if (rlen < strlen("text/xml"))
            return 0;
        if (strncasecmp(buffer, "text/xml", strlen("text/xml")) == 0) {
            bi->fmt = BIND_FORMAT_XML;
            memset(&bi->ctx_xml, 0, sizeof(bi->ctx_xml));
            bi->ctx_xml.fams = bi->fams;
            bi->ctx_xml.labels = &bi->labels;
            bind_xml_parse(&bi->ctx_xml);
            return len;
        }

        if (rlen < strlen("application/json"))
            return 0;
        if (strncasecmp(buffer, "application/json", strlen("application/json")) == 0) {
            bi->fmt = BIND_FORMAT_JSON;
            memset(&bi->ctx_json, 0, sizeof(bi->ctx_json));
            bi->ctx_json.fams = bi->fams;
            bi->ctx_json.labels = &bi->labels;
            bind_json_parse(&bi->ctx_json);
            return len;
        }
    }

    return len;
}

static size_t bind_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    bind_instance_t *bi = user_data;
    if (bi == NULL) {
        PLUGIN_ERROR("user_data pointer is NULL.");
        return 0;
    }

    size_t len = size * nmemb;
    if (len == 0)
        return 0;

    int status = 0;
    if (bi->fmt == BIND_FORMAT_XML) {
        status = bind_xml_parse_chunk(&bi->ctx_xml, buf, len);
    } else if (bi->fmt == BIND_FORMAT_JSON) {
        status = bind_json_parse_chunk(&bi->ctx_json, buf, len);
    }

    if (status != 0) {

    }

    return len;
}

static int bind_read(user_data_t *user_data)
{
    bind_instance_t *bi = user_data->data;

    if (bi == NULL) {
        PLUGIN_ERROR("bind instance is NULL.");
        return -1;
    }

    if (bi->curl == NULL) {
        bi->curl = curl_easy_init();
        if (bi->curl == NULL) {
            PLUGIN_ERROR("curl_easy_init failed.");
            return -1;
        }

        CURLcode rcode = 0;

        rcode = curl_easy_setopt(bi->curl, CURLOPT_NOSIGNAL, 1L);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_NOSIGNAL failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(bi->curl, CURLOPT_WRITEFUNCTION, bind_curl_callback);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEFUNCTION failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(bi->curl, CURLOPT_WRITEDATA, bi);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEDATA failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(bi->curl, CURLOPT_HEADERFUNCTION, bind_header_callback);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_HEADERFUNCTION failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(bi->curl, CURLOPT_HEADERDATA, bi);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_HEADERDATA failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(bi->curl, CURLOPT_USERAGENT, NCOLLECTD_USERAGENT);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERAGENT failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(bi->curl, CURLOPT_ERRORBUFFER, bi->bind_curl_error);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(bi->curl, CURLOPT_FOLLOWLOCATION, 1L);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_FOLLOWLOCATION failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(bi->curl, CURLOPT_MAXREDIRS, 50L);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_MAXREDIRS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

#ifdef HAVE_CURLOPT_TIMEOUT_MS
        rcode = curl_easy_setopt(bi->curl, CURLOPT_TIMEOUT_MS,
                                 (bi->timeout >= 0) ? (long)bi->timeout
                                 : (long)CDTIME_T_TO_MS(plugin_get_interval()));
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
#endif
    }

    bi->fmt = BIND_FORMAT_NONE;

    CURLcode rcode = curl_easy_setopt(bi->curl, CURLOPT_URL,
                                                (bi->url != NULL) ? bi->url : BIND_DEFAULT_URL);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_URL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    if (curl_easy_perform(bi->curl) != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_perform failed: %s", bi->bind_curl_error);
        return -1;
    }

    histogram_t **traffic = NULL;
    if (bi->fmt == BIND_FORMAT_XML) {
        bind_xml_parse_end(&bi->ctx_xml);
        traffic = bi->ctx_xml.traffic;
    } else if (bi->fmt == BIND_FORMAT_JSON) {
        bind_json_parse_end(&bi->ctx_json);
        traffic = bi->ctx_json.traffic;
    }

    bind_traffic_histograms(bi->fams, &bi->labels, traffic);

    plugin_dispatch_metric_family_array(bi->fams, FAM_BIND_MAX, 0);

    return 0;
}

static void bind_instance_free(void *arg)
{
    bind_instance_t *bi = arg;

    if (bi == NULL)
        return;

    if (bi->curl != NULL) {
        curl_easy_cleanup(bi->curl);
        bi->curl = NULL;
    }

    free(bi->instance);
    free(bi->url);
    label_set_reset(&bi->labels);

    free(bi);
}

static int bind_config_instance(config_item_t *ci)
{
    bind_instance_t *bi = calloc(1, sizeof(*bi));
    if (bi == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    bi->timeout = -1;

    memcpy(bi->fams, fams_bind, FAM_BIND_MAX * sizeof(bi->fams[0]));

    int status = cf_util_get_string(ci, &bi->instance);
    if (status != 0) {
        free(bi);
        return status;
    }
    assert(bi->instance != NULL);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &bi->url);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_int(child, &bi->timeout);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &bi->labels);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        bind_instance_free(bi);
        return -1;
    }

    label_set_add(&bi->labels, true, "instance", bi->instance);

    return plugin_register_complex_read("bind", bi->instance, bind_read, interval,
                    &(user_data_t){ .data = bi, .free_func = bind_instance_free, });
}

static int bind_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = bind_config_instance(child);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
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
    plugin_register_config("bind", bind_config);
}
