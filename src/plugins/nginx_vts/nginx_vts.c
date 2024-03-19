// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libxson/json_parse.h"

#include <curl/curl.h>

#define NGINX_VTS_DEFAULT_URL "http://localhost/status/format/json"

enum {
    FAM_NGINX_VTS_START_TIME_SECONDS,
    FAM_NGINX_VTS_CONNECTIONS,
    FAM_NGINX_VTS_SHM_USED_BYTES,
    FAM_NGINX_VTS_SHM_SIZE_BYTES,
    FAM_NGINX_VTS_SHM_NODES,
    FAM_NGINX_VTS_SERVER_IN_BYTES,
    FAM_NGINX_VTS_SERVER_OUT_BYTES,
    FAM_NGINX_VTS_SERVER_REQUESTS,
    FAM_NGINX_VTS_SERVER_RESPONSES,
    FAM_NGINX_VTS_SERVER_CACHE,
    FAM_NGINX_VTS_SERVER_REQUEST_SECONDS,
    FAM_NGINX_VTS_SERVER_REQUEST_DURATION_SECONDS,
    FAM_NGINX_VTS_FILTER_REQUESTS,
    FAM_NGINX_VTS_FILTER_IN_BYTES,
    FAM_NGINX_VTS_FILTER_OUT_BYTES,
    FAM_NGINX_VTS_FILTER_RESPONSES,
    FAM_NGINX_VTS_FILTER_CACHE,
    FAM_NGINX_VTS_FILTER_REQUEST_SECONDS,
    FAM_NGINX_VTS_FILTER_REQUEST_DURATION_SECONDS,
    FAM_NGINX_VTS_UPSTREAM_REQUESTS,
    FAM_NGINX_VTS_UPSTREAM_IN_BYTES,
    FAM_NGINX_VTS_UPSTREAM_OUT_BYTES,
    FAM_NGINX_VTS_UPSTREAM_RESPONSES,
    FAM_NGINX_VTS_UPSTREAM_REQUEST_SECONDS,
    FAM_NGINX_VTS_UPSTREAM_REQUEST_DURATION_SECONDS,
    FAM_NGINX_VTS_UPSTREAM_RESPONSE_SECONDS,
    FAM_NGINX_VTS_UPSTREAM_RESPONSE_DURATION_SECONDS,
    FAM_NGINX_VTS_CACHE_USED_BYTES,
    FAM_NGINX_VTS_CACHE_SIZE_BYTES,
    FAM_NGINX_VTS_CACHE_IN_BYTES,
    FAM_NGINX_VTS_CACHE_OUT_BYTES,
    FAM_NGINX_VTS_CACHE_RESPONSES,
    FAM_NGINX_VTS_MAX,
};

static metric_family_t fams_nginx_vts[FAM_NGINX_VTS_MAX] = {
    [FAM_NGINX_VTS_START_TIME_SECONDS] = {
        .name = "nginx_vts_start_time_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Nginx start time in seconds.",
    },
    [FAM_NGINX_VTS_CONNECTIONS] = {
        .name = "nginx_vts_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Nginx connections.",
    },
    [FAM_NGINX_VTS_SHM_USED_BYTES] = {
        .name = "nginx_vts_shm_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Nginx shared memory in use in bytes.",
    },
    [FAM_NGINX_VTS_SHM_SIZE_BYTES] = {
        .name = "nginx_vts_shm_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Nginx total shared memory in bytes.",
    },
    [FAM_NGINX_VTS_SHM_NODES] = {
        .name = "nginx_vts_shm_nodes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Nginx shared memory nodes.",
    },
    [FAM_NGINX_VTS_SERVER_IN_BYTES] = {
        .name = "nginx_vts_server_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of request bytes in seconds.",
    },
    [FAM_NGINX_VTS_SERVER_OUT_BYTES] = {
        .name = "nginx_vts_server_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of reponse bytes in seconds.",
    },
    [FAM_NGINX_VTS_SERVER_REQUESTS] = {
        .name = "nginx_vts_server_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of requests.",
    },
    [FAM_NGINX_VTS_SERVER_REQUEST_SECONDS] = {
        .name = "nginx_vts_server_request_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total request processing time in seconds.",
    },
    [FAM_NGINX_VTS_SERVER_REQUEST_DURATION_SECONDS] = {
        .name = "nginx_vts_server_request_duration_seconds",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "The histogram of request processing time.",
    },
    [FAM_NGINX_VTS_SERVER_RESPONSES] = {
        .name = "nginx_vts_server_responses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of reponses by code.",
    },
    [FAM_NGINX_VTS_SERVER_CACHE] = {
        .name = "nginx_vts_server_cache",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of  requests cache counter",
    },

    [FAM_NGINX_VTS_FILTER_REQUESTS] = {
        .name = "nginx_vts_filter_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of requests.",
    },
    [FAM_NGINX_VTS_FILTER_IN_BYTES] = {
        .name = "nginx_vts_filter_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NGINX_VTS_FILTER_OUT_BYTES] = {
        .name = "nginx_vts_filter_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NGINX_VTS_FILTER_RESPONSES] = {
        .name = "nginx_vts_filter_responses",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NGINX_VTS_FILTER_CACHE] = {
        .name = "nginx_vts_filter_cache",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NGINX_VTS_FILTER_REQUEST_SECONDS] = {
        .name = "nginx_vts_filter_request_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NGINX_VTS_FILTER_REQUEST_DURATION_SECONDS] = {
        .name = "nginx_vts_filter_request_duration_seconds",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = NULL,
    },
    [FAM_NGINX_VTS_UPSTREAM_REQUESTS] = {
        .name = "nginx_vts_upstream_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NGINX_VTS_UPSTREAM_IN_BYTES] = {
        .name = "nginx_vts_upstream_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NGINX_VTS_UPSTREAM_OUT_BYTES] = {
        .name = "nginx_vts_upstream_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NGINX_VTS_UPSTREAM_RESPONSES] = {
        .name = "nginx_vts_upstream_responses",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NGINX_VTS_UPSTREAM_REQUEST_SECONDS] = {
        .name = "nginx_vts_upstream_request_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NGINX_VTS_UPSTREAM_REQUEST_DURATION_SECONDS] = {
        .name = "nginx_vts_upstream_request_duration_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NGINX_VTS_UPSTREAM_RESPONSE_SECONDS] = {
        .name = "nginx_vts_upstream_response_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NGINX_VTS_UPSTREAM_RESPONSE_DURATION_SECONDS] = {
        .name = "nginx_vts_upstream_response_duration_seconds",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = NULL,
    },
    [FAM_NGINX_VTS_CACHE_USED_BYTES] = {
        .name = "nginx_vts_cache_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_NGINX_VTS_CACHE_SIZE_BYTES] = {
        .name = "nginx_vts_cache_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_NGINX_VTS_CACHE_IN_BYTES] = {
        .name = "nginx_vts_cache_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NGINX_VTS_CACHE_OUT_BYTES] = {
        .name = "nginx_vts_cache_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_NGINX_VTS_CACHE_RESPONSES] = {
        .name = "nginx_vts_cache_reponses",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
};

typedef enum {
    NGINX_VTS_JSON_NONE,
    NGINX_VTS_JSON_LOAD,
    NGINX_VTS_JSON_CONNECTIONS,
    NGINX_VTS_JSON_SHARED_ZONES,
    NGINX_VTS_JSON_SHARED_ZONE_MAX_SIZE,
    NGINX_VTS_JSON_SHARED_ZONE_USED_SIZE,
    NGINX_VTS_JSON_SHARED_ZONE_USED_MODE,
    NGINX_VTS_JSON_SERVER_ZONES,
    NGINX_VTS_JSON_SERVER_ZONE_IN_BYTES,
    NGINX_VTS_JSON_SERVER_ZONE_OUT_BYTES,
    NGINX_VTS_JSON_SERVER_ZONE_REQUESTS,
    NGINX_VTS_JSON_SERVER_ZONE_RESPONSES,
    NGINX_VTS_JSON_FILTER_ZONES,
    NGINX_VTS_JSON_FILTER_ZONE_IN_BYTES,
    NGINX_VTS_JSON_FILTER_ZONE_OUT_BYTES,
    NGINX_VTS_JSON_FILTER_ZONE_REQUESTS,
    NGINX_VTS_JSON_FILTER_ZONE_RESPONSES,
    NGINX_VTS_JSON_UPSTREAM_ZONES,
    NGINX_VTS_JSON_UPSTREAM_ZONE_SERVER,
    NGINX_VTS_JSON_UPSTREAM_ZONE_IN_BYTES,
    NGINX_VTS_JSON_UPSTREAM_ZONE_OUT_BYTES,
    NGINX_VTS_JSON_UPSTREAM_ZONE_REQUESTS,
    NGINX_VTS_JSON_UPSTREAM_ZONE_RESPONSES,
    NGINX_VTS_JSON_CACHE_ZONES,
    NGINX_VTS_JSON_CACHE_ZONE_USED_BYTES,
    NGINX_VTS_JSON_CACHE_ZONE_SIZE_BYTES,
    NGINX_VTS_JSON_CACHE_ZONE_IN_BYTES,
    NGINX_VTS_JSON_CACHE_ZONE_OUT_BYTES,
    NGINX_VTS_JSON_CACHE_ZONE_RESPONSES,
} nginx_vts_json_key_t;

typedef struct {
    char value1[256];
    char value2[256];
    char value3[256];
    nginx_vts_json_key_t stack[JSON_MAX_DEPTH];
    size_t depth;
    metric_family_t *fams;
    label_set_t *labels;
} nginx_vts_json_ctx_t;

typedef struct {
    char *instance;
    char *url;
    int timeout;
    label_set_t labels;
    CURL *curl;
    char curl_error[CURL_ERROR_SIZE];
    metric_family_t fams[FAM_NGINX_VTS_MAX];
    json_parser_t handle;
} nginx_vts_instance_t;

static bool nginx_vts_json_string(void *ctx, const char *string, size_t string_len)
{
    nginx_vts_json_ctx_t *sctx = ctx;

    if ((sctx->depth == 3) && (sctx->stack[2] == NGINX_VTS_JSON_UPSTREAM_ZONE_SERVER))
        sstrnncpy(sctx->value2, sizeof(sctx->value2), (const char *)string, string_len);

    return true;
}

static bool nginx_vts_json_number(void *ctx, const char *number_val, size_t number_len)
{
    nginx_vts_json_ctx_t *sctx = ctx;

    char number[64];
    sstrnncpy(number, sizeof(number), number_val, number_len);

    switch (sctx->depth) {
    case 1:
        if (sctx->stack[0] == NGINX_VTS_JSON_LOAD)
            metric_family_append(&sctx->fams[FAM_NGINX_VTS_START_TIME_SECONDS],
                                 VALUE_GAUGE((double)atoll(number) / 1000.0), sctx->labels, NULL);
        break;
    case 2:
        switch (sctx->stack[0]) {
        case NGINX_VTS_JSON_CONNECTIONS:
            metric_family_append(&sctx->fams[FAM_NGINX_VTS_CONNECTIONS],
                                 VALUE_GAUGE(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="status", .value=sctx->value1}, NULL);
            break;
        case NGINX_VTS_JSON_SHARED_ZONES:
            switch (sctx->stack[1]) {
            case NGINX_VTS_JSON_SHARED_ZONE_MAX_SIZE:
                metric_family_append(&sctx->fams[FAM_NGINX_VTS_SHM_SIZE_BYTES],
                                     VALUE_GAUGE(atol(number)), sctx->labels, NULL);
                break;
            case NGINX_VTS_JSON_SHARED_ZONE_USED_SIZE:
                metric_family_append(&sctx->fams[FAM_NGINX_VTS_SHM_USED_BYTES],
                                     VALUE_GAUGE(atol(number)), sctx->labels, NULL);
                break;
            case NGINX_VTS_JSON_SHARED_ZONE_USED_MODE:
                metric_family_append(&sctx->fams[FAM_NGINX_VTS_SHM_NODES],
                                     VALUE_GAUGE(atol(number)), sctx->labels, NULL);
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
        break;
    case 3:
        switch (sctx->stack[2]) {
        case NGINX_VTS_JSON_SERVER_ZONE_IN_BYTES:
            metric_family_append(&sctx->fams[FAM_NGINX_VTS_SERVER_IN_BYTES],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="host", .value=sctx->value1}, NULL);
            break;
        case NGINX_VTS_JSON_SERVER_ZONE_OUT_BYTES:
            metric_family_append(&sctx->fams[FAM_NGINX_VTS_SERVER_OUT_BYTES],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="host", .value=sctx->value1}, NULL);
            break;
        case NGINX_VTS_JSON_SERVER_ZONE_REQUESTS:
            metric_family_append(&sctx->fams[FAM_NGINX_VTS_SERVER_REQUESTS],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="host", .value=sctx->value1}, NULL);
            break;
        case NGINX_VTS_JSON_UPSTREAM_ZONE_IN_BYTES:
            metric_family_append(&sctx->fams[FAM_NGINX_VTS_UPSTREAM_IN_BYTES],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="host", .value=sctx->value1}, NULL);
            break;
        case NGINX_VTS_JSON_UPSTREAM_ZONE_OUT_BYTES:
            metric_family_append(&sctx->fams[FAM_NGINX_VTS_UPSTREAM_OUT_BYTES],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="upstream", .value=sctx->value1},
                                 &(label_pair_const_t){.name="backend", .value=sctx->value2},
                                 NULL);
            break;
        case NGINX_VTS_JSON_UPSTREAM_ZONE_REQUESTS:
            metric_family_append(&sctx->fams[FAM_NGINX_VTS_UPSTREAM_REQUESTS],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="upstream", .value=sctx->value1},
                                 &(label_pair_const_t){.name="backend", .value=sctx->value2},
                                 NULL);
            break;
        case NGINX_VTS_JSON_CACHE_ZONE_USED_BYTES:
            metric_family_append(&sctx->fams[FAM_NGINX_VTS_CACHE_USED_BYTES],
                                 VALUE_GAUGE(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="cache_zone", .value=sctx->value1},
                                 NULL);
            break;
        case NGINX_VTS_JSON_CACHE_ZONE_SIZE_BYTES:
            metric_family_append(&sctx->fams[FAM_NGINX_VTS_CACHE_SIZE_BYTES],
                                 VALUE_GAUGE(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="cache_zone", .value=sctx->value1},
                                 NULL);
            break;
        case NGINX_VTS_JSON_CACHE_ZONE_IN_BYTES:
            metric_family_append(&sctx->fams[FAM_NGINX_VTS_CACHE_IN_BYTES],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="cache_zone", .value=sctx->value1},
                                 NULL);
            break;
        case NGINX_VTS_JSON_CACHE_ZONE_OUT_BYTES:
            metric_family_append(&sctx->fams[FAM_NGINX_VTS_CACHE_IN_BYTES],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="cache_zone", .value=sctx->value1},
                                 NULL);
            break;
        default:
            break;
        }
        break;
    case 4:
        switch (sctx->stack[2]) {
        case NGINX_VTS_JSON_SERVER_ZONE_RESPONSES:
            if ((strlen(sctx->value2) == 3) &&
                (sctx->value2[1] == 'x') && (sctx->value2[2] == 'x')) {
                metric_family_append(&sctx->fams[FAM_NGINX_VTS_SERVER_RESPONSES],
                                     VALUE_COUNTER(atol(number)), sctx->labels,
                                     &(label_pair_const_t){.name="host", .value=sctx->value1},
                                     &(label_pair_const_t){.name="code", .value=sctx->value2},
                                     NULL);
            } else {
                metric_family_append(&sctx->fams[FAM_NGINX_VTS_SERVER_CACHE],
                                     VALUE_COUNTER(atol(number)), sctx->labels,
                                     &(label_pair_const_t){.name="host", .value=sctx->value1},
                                     &(label_pair_const_t){.name="status", .value=sctx->value2},
                                     NULL);
            }
            break;
        case NGINX_VTS_JSON_UPSTREAM_ZONE_RESPONSES:
            metric_family_append(&sctx->fams[FAM_NGINX_VTS_UPSTREAM_RESPONSES],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="upstream", .value=sctx->value1},
                                 &(label_pair_const_t){.name="backend", .value=sctx->value2},
                                 &(label_pair_const_t){.name="code", .value=sctx->value3},
                                 NULL);
            break;
        case NGINX_VTS_JSON_CACHE_ZONE_RESPONSES:
            metric_family_append(&sctx->fams[NGINX_VTS_JSON_CACHE_ZONE_RESPONSES],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="cache_zone", .value=sctx->value1},
                                 &(label_pair_const_t){.name="status", .value=sctx->value2},
                                 NULL);
            break;
        case NGINX_VTS_JSON_FILTER_ZONES:
            switch(sctx->stack[3]) {
            case NGINX_VTS_JSON_FILTER_ZONE_IN_BYTES:
                metric_family_append(&sctx->fams[FAM_NGINX_VTS_FILTER_IN_BYTES],
                                     VALUE_COUNTER(atol(number)), sctx->labels,
                                     &(label_pair_const_t){.name="filter", .value=sctx->value1},
                                     &(label_pair_const_t){.name="filter_name", .value=sctx->value2},
                                     NULL);
                break;
            case NGINX_VTS_JSON_FILTER_ZONE_OUT_BYTES:
                metric_family_append(&sctx->fams[FAM_NGINX_VTS_FILTER_OUT_BYTES],
                                     VALUE_COUNTER(atol(number)), sctx->labels,
                                     &(label_pair_const_t){.name="filter", .value=sctx->value1},
                                     &(label_pair_const_t){.name="filter_name", .value=sctx->value2},
                                     NULL);
                break;
            case NGINX_VTS_JSON_FILTER_ZONE_REQUESTS:
                metric_family_append(&sctx->fams[FAM_NGINX_VTS_FILTER_REQUESTS],
                                     VALUE_COUNTER(atol(number)), sctx->labels,
                                     &(label_pair_const_t){.name="filter", .value=sctx->value1},
                                     &(label_pair_const_t){.name="filter_name", .value=sctx->value2},
                                     NULL);
                break;
            default:
                break;
            }
        default:
            break;
        }
        break;
    case 5:
        switch (sctx->stack[3]) {
        case NGINX_VTS_JSON_FILTER_ZONE_RESPONSES:
            if ((strlen(sctx->value2) == 3) &&
                (sctx->value2[1] == 'x') && (sctx->value2[2] == 'x')) {
                metric_family_append(&sctx->fams[FAM_NGINX_VTS_FILTER_RESPONSES],
                                     VALUE_COUNTER(atol(number)), sctx->labels,
                                     &(label_pair_const_t){.name="filter", .value=sctx->value1},
                                     &(label_pair_const_t){.name="filter_name", .value=sctx->value2},
                                     &(label_pair_const_t){.name="code", .value=sctx->value3},
                                     NULL);
            } else {
                metric_family_append(&sctx->fams[FAM_NGINX_VTS_FILTER_CACHE],
                                     VALUE_COUNTER(atol(number)), sctx->labels,
                                     &(label_pair_const_t){.name="filter", .value=sctx->value1},
                                     &(label_pair_const_t){.name="filter_name", .value=sctx->value2},
                                     &(label_pair_const_t){.name="status", .value=sctx->value3},
                                     NULL);
            }
            break;
        default:
            break;
        }
        break;
    }

    return true;
}

static bool nginx_vts_json_start_map(void *ctx)
{
    nginx_vts_json_ctx_t *sctx = ctx;
    sctx->depth++;
    sctx->stack[sctx->depth] = NGINX_VTS_JSON_NONE;

    return true;
}

static bool nginx_vts_json_map_key(void * ctx, const char * key, size_t string_len)
{
    nginx_vts_json_ctx_t *sctx = ctx;

    switch (sctx->depth) {
    case 1:
        switch(string_len) {
        case 8: // loadMsec
            if (strncmp("loadMsec", (const char *)key, string_len) == 0) {
                sctx->stack[0] = NGINX_VTS_JSON_LOAD;
                return true;
            }
            break;
        case 10: // cacheZones
            if (strncmp("cacheZones", (const char *)key, string_len) == 0) {
                sctx->stack[0] = NGINX_VTS_JSON_CACHE_ZONES;
                return true;
            }
            break;
        case 11: // filterZones connections sharedZones serverZones
            if (strncmp("filterZones", (const char *)key, string_len) == 0) {
                sctx->stack[0] = NGINX_VTS_JSON_FILTER_ZONES;
                return true;
            } else if (strncmp("connections", (const char *)key, string_len) == 0) {
                sctx->stack[0] = NGINX_VTS_JSON_CONNECTIONS;
                return true;
            } else if (strncmp("sharedZones", (const char *)key, string_len) == 0) {
                sctx->stack[0] = NGINX_VTS_JSON_SHARED_ZONES;
                return true;
            } else if (strncmp("serverZones", (const char *)key, string_len) == 0) {
                sctx->stack[0] = NGINX_VTS_JSON_SERVER_ZONES;
                return true;
            }
            break;
        case 13: // upstreamZones
            if (strncmp("upstreamZones", (const char *)key, string_len) == 0) {
                sctx->stack[0] = NGINX_VTS_JSON_UPSTREAM_ZONES;
                return true;
            }
            break;
        default:
            break;
        }
        sctx->stack[0] = NGINX_VTS_JSON_NONE;
        break;
    case 2:
        switch (sctx->stack[0]) {
        case NGINX_VTS_JSON_CONNECTIONS:
        case NGINX_VTS_JSON_SERVER_ZONES:
        case NGINX_VTS_JSON_FILTER_ZONES:
        case NGINX_VTS_JSON_UPSTREAM_ZONES:
        case NGINX_VTS_JSON_CACHE_ZONES:
            sstrnncpy(sctx->value1, sizeof(sctx->value1), (const char *)key, string_len);
            break;
        case NGINX_VTS_JSON_SHARED_ZONES:
            switch(string_len) {
            case 7:
                if (strncmp("maxSize", (const char *)key, string_len) == 0) {
                    sctx->stack[1] = NGINX_VTS_JSON_SHARED_ZONE_MAX_SIZE;
                    return true;
                }
                break;
            case 8:
                if (strncmp("usedSize", (const char *)key, string_len) == 0) {
                    sctx->stack[1] = NGINX_VTS_JSON_SHARED_ZONE_USED_SIZE;
                    return true;
                } else if (strncmp("usedNode", (const char *)key, string_len) == 0) {
                    sctx->stack[1] = NGINX_VTS_JSON_SHARED_ZONE_USED_MODE;
                    return true;
                }
                break;
            }
            sctx->stack[1] = NGINX_VTS_JSON_NONE;
            break;
        default:
            break;
        }
        break;
    case 3:
        switch (sctx->stack[0]) {
        case NGINX_VTS_JSON_SERVER_ZONES:
            switch(string_len) {
            case 7:
                if (strncmp("inBytes", (const char *)key, string_len) == 0) {
                    sctx->stack[2] = NGINX_VTS_JSON_SERVER_ZONE_IN_BYTES;
                    return true;
                }
                break;
            case 8:
                if (strncmp("outBytes", (const char *)key, string_len) == 0) {
                    sctx->stack[2] = NGINX_VTS_JSON_SERVER_ZONE_OUT_BYTES;
                    return true;
                }
                break;
            case 9:
                if (strncmp("responses", (const char *)key, string_len) == 0) {
                    sctx->stack[2] = NGINX_VTS_JSON_SERVER_ZONE_RESPONSES;
                    return true;
                }
                break;
            case 14:
                if (strncmp("requestCounter", (const char *)key, string_len) == 0) {
                    sctx->stack[2] = NGINX_VTS_JSON_SERVER_ZONE_REQUESTS;
                    return true;
                }
                break;
            default:
                break;
            }
            sctx->stack[2] = NGINX_VTS_JSON_NONE;
            break;
        case NGINX_VTS_JSON_FILTER_ZONES:
            sstrnncpy(sctx->value2, sizeof(sctx->value2), (const char *)key, string_len);
            break;
        case NGINX_VTS_JSON_UPSTREAM_ZONES:
            switch(string_len) {
            case 6:
                if (strncmp("server", (const char *)key, string_len) == 0) {
                    sctx->stack[2] = NGINX_VTS_JSON_UPSTREAM_ZONE_SERVER;
                    return true;
                }
                break;
            case 7:
                if (strncmp("inBytes", (const char *)key, string_len) == 0) {
                    sctx->stack[2] = NGINX_VTS_JSON_UPSTREAM_ZONE_IN_BYTES;
                    return true;
                }
                break;
            case 8:
                if (strncmp("outBytes", (const char *)key, string_len) == 0) {
                    sctx->stack[2] = NGINX_VTS_JSON_UPSTREAM_ZONE_OUT_BYTES;
                    return true;
                }
                break;
            case 9:
                if (strncmp("responses", (const char *)key, string_len) == 0) {
                    sctx->stack[2] = NGINX_VTS_JSON_UPSTREAM_ZONE_RESPONSES;
                    return true;
                }
                break;
            case 14:
                if (strncmp("requestCounter", (const char *)key, string_len) == 0) {
                    sctx->stack[2] = NGINX_VTS_JSON_UPSTREAM_ZONE_REQUESTS;
                    return true;
                }
                break;
            default:
                break;
            }
            sctx->stack[2] = NGINX_VTS_JSON_NONE;
            break;
        case NGINX_VTS_JSON_CACHE_ZONES:
            switch(string_len) {
            case 7:
                if (strncmp("inBytes", (const char *)key, string_len) == 0) {
                    sctx->stack[2] = NGINX_VTS_JSON_CACHE_ZONE_IN_BYTES;
                    return true;
                } else if (strncmp("maxSize", (const char *)key, string_len) == 0) {
                    sctx->stack[2] = NGINX_VTS_JSON_CACHE_ZONE_SIZE_BYTES;
                    return true;
                }
                break;
            case 8:
                if (strncmp("outBytes", (const char *)key, string_len) == 0) {
                    sctx->stack[2] = NGINX_VTS_JSON_CACHE_ZONE_OUT_BYTES;
                    return true;
                } else if (strncmp("usedSize", (const char *)key, string_len) == 0) {
                    sctx->stack[2] = NGINX_VTS_JSON_CACHE_ZONE_USED_BYTES;
                    return true;
                }
                break;
            case 9:
                if (strncmp("responses", (const char *)key, string_len) == 0) {
                    sctx->stack[2] = NGINX_VTS_JSON_CACHE_ZONE_RESPONSES;
                    return true;
                }
                break;
            default:
                break;
            }
            sctx->stack[2] = NGINX_VTS_JSON_NONE;
            break;
            break;
        default:
            break;
        }
        break;
    case 4:
        if (sctx->stack[0] == NGINX_VTS_JSON_FILTER_ZONES) {
            switch(string_len) {
            case 7:
                if (strncmp("inBytes", (const char *)key, string_len) == 0) {
                    sctx->stack[3] = NGINX_VTS_JSON_FILTER_ZONE_IN_BYTES;
                    return true;
                }
                break;
            case 8:
                if (strncmp("outBytes", (const char *)key, string_len) == 0) {
                    sctx->stack[3] = NGINX_VTS_JSON_FILTER_ZONE_OUT_BYTES;
                    return true;
                }
                break;
            case 9:
                if (strncmp("responses", (const char *)key, string_len) == 0) {
                    sctx->stack[3] = NGINX_VTS_JSON_FILTER_ZONE_RESPONSES;
                    return true;
                }
                break;
            case 14:
                if (strncmp("requestCounter", (const char *)key, string_len) == 0) {
                    sctx->stack[3] = NGINX_VTS_JSON_FILTER_ZONE_REQUESTS;
                    return true;
                }
                break;
            default:
                break;
            }
            sctx->stack[3] = NGINX_VTS_JSON_NONE;
            break;
        } else if (sctx->stack[2] == NGINX_VTS_JSON_SERVER_ZONE_RESPONSES) {
            sstrnncpy(sctx->value2, sizeof(sctx->value2), (const char *)key, string_len);
        } else if (sctx->stack[2] == NGINX_VTS_JSON_CACHE_ZONE_RESPONSES) {
            sstrnncpy(sctx->value2, sizeof(sctx->value2), (const char *)key, string_len);
        } else if (sctx->stack[2] == NGINX_VTS_JSON_UPSTREAM_ZONE_RESPONSES) {
            sstrnncpy(sctx->value3, sizeof(sctx->value3), (const char *)key, string_len);
        }
        break;
    case 5:
        if (sctx->stack[3] == NGINX_VTS_JSON_FILTER_ZONE_RESPONSES) {
            sstrnncpy(sctx->value3, sizeof(sctx->value3), (const char *)key, string_len);
        }
        break;
    default:
        break;
    }

    return true;
}

static bool nginx_vts_json_end_map(void *ctx)
{
    nginx_vts_json_ctx_t *sctx = ctx;

//fprintf(stderr, "end map %d\n", (int)sctx->depth);
    sctx->depth--;
    if (sctx->depth > 0) {
        sctx->stack[sctx->depth-1] = NGINX_VTS_JSON_NONE;
    } else {
        sctx->depth = 0;
    }

    return true;
}

static json_callbacks_t nginx_vts_json_callbacks = {
    .json_null        = NULL,
    .json_boolean     = NULL,
    .json_integer     = NULL,
    .json_double      = NULL,
    .json_number      = nginx_vts_json_number,
    .json_string      = nginx_vts_json_string,
    .json_start_map   = nginx_vts_json_start_map,
    .json_map_key     = nginx_vts_json_map_key,
    .json_end_map     = nginx_vts_json_end_map,
    .json_start_array = NULL,
    .json_end_array   = NULL,
};

static size_t nginx_vts_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    nginx_vts_instance_t *ngx = user_data;
    if (ngx == NULL) {
        PLUGIN_ERROR("user_data pointer is NULL.");
        return 0;
    }

    size_t len = size * nmemb;
    if (len == 0)
        return 0;

    json_status_t status = json_parser_parse(&ngx->handle, buf, len);
    if (status != JSON_STATUS_OK)
        return 0;

    return len;
}

static int nginx_vts_read(user_data_t *user_data)
{
    nginx_vts_instance_t *ngx = user_data->data;

    if (ngx == NULL) {
        PLUGIN_ERROR("nginx_vtx instance is NULL.");
        return -1;
    }

    if (ngx->curl == NULL) {
        ngx->curl = curl_easy_init();
        if (ngx->curl == NULL) {
            PLUGIN_ERROR("curl_easy_init failed.");
            return -1;
        }

        CURLcode rcode = 0;

        rcode = curl_easy_setopt(ngx->curl, CURLOPT_NOSIGNAL, 1L);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_NOSIGNAL failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(ngx->curl, CURLOPT_WRITEFUNCTION, nginx_vts_curl_callback);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEFUNCTION failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(ngx->curl, CURLOPT_WRITEDATA, ngx);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEDATA failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(ngx->curl, CURLOPT_USERAGENT, NCOLLECTD_USERAGENT);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERAGENT failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(ngx->curl, CURLOPT_ERRORBUFFER, ngx->curl_error);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(ngx->curl, CURLOPT_FOLLOWLOCATION, 1L);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_FOLLOWLOCATION failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(ngx->curl, CURLOPT_MAXREDIRS, 50L);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_MAXREDIRS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

#ifdef HAVE_CURLOPT_TIMEOUT_MS
        rcode = curl_easy_setopt(ngx->curl, CURLOPT_TIMEOUT_MS,
                                   (ngx->timeout >= 0) ? (long)ngx->timeout
                                   : (long)CDTIME_T_TO_MS(plugin_get_interval()));
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
#endif
    }

    nginx_vts_json_ctx_t ctx = {0};
    ctx.fams = ngx->fams;
    ctx.labels = &ngx->labels;

    json_parser_init(&ngx->handle, 0, &nginx_vts_json_callbacks, &ctx);

    CURLcode rcode = curl_easy_setopt(ngx->curl, CURLOPT_URL,
                                      (ngx->url != NULL) ? ngx->url : NGINX_VTS_DEFAULT_URL);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_URL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    if (curl_easy_perform(ngx->curl) != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_perform failed: %s", ngx->curl_error);
        json_parser_free(&ngx->handle);
        return -1;
    }

    json_status_t status = json_parser_complete(&ngx->handle);
    if (status != JSON_STATUS_OK) {
        unsigned char *errmsg = json_parser_get_error(&ngx->handle, 0, NULL, 0);
        PLUGIN_ERROR("json_parse_complete failed: %s", (const char *)errmsg);
        json_parser_free_error(errmsg);
        json_parser_free (&ngx->handle);
        return -1;
    }
    json_parser_free(&ngx->handle);

    plugin_dispatch_metric_family_array(ngx->fams, FAM_NGINX_VTS_MAX, 0);
    return 0;
}

static void nginx_vts_instance_free(void *arg)
{
    nginx_vts_instance_t *ngx = arg;

    if (ngx == NULL)
        return;

    if (ngx->curl != NULL) {
        curl_easy_cleanup(ngx->curl);
        ngx->curl = NULL;
    }

    free(ngx->instance);
    free(ngx->url);
    label_set_reset(&ngx->labels);

    free(ngx);
}

static int nginx_vts_config_instance(config_item_t *ci)
{
    nginx_vts_instance_t *ngx = calloc(1, sizeof(*ngx));
    if (ngx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    ngx->timeout = -1;

    memcpy(ngx->fams, fams_nginx_vts, FAM_NGINX_VTS_MAX * sizeof(ngx->fams[0]));

    int status = cf_util_get_string(ci, &ngx->instance);
    if (status != 0) {
        free(ngx);
        return status;
    }
    assert(ngx->instance != NULL);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &ngx->url);
        } else if (strcasecmp("labels", child->key) == 0) {
            status = cf_util_get_label(child, &ngx->labels);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_int(child, &ngx->timeout);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        nginx_vts_instance_free(ngx);
        return -1;
    }

    return plugin_register_complex_read("nginx_vts", ngx->instance, nginx_vts_read, interval,
                    &(user_data_t){.data = ngx, .free_func = nginx_vts_instance_free});
}

static int nginx_vts_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = nginx_vts_config_instance(child);
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
    plugin_register_config("nginx_vts", nginx_vts_config);
}
