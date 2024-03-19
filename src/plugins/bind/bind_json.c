// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include "bind_fams.h"
#include "bind_json.h"

int bind_append_metric(metric_family_t *fams, label_set_t *labels,
                       const char *prefix, const char *name,
                       const char *lkey, const char *lvalue, const char *value);

time_t bind_get_timestamp(const char *str, size_t len);

int bind_traffic_histogram_append(histogram_t **rh, const char *maximum, const char *counter);

static bool bind_json_number(void *ctx, const char *number_val, size_t number_len)
{
    bind_json_ctx_t *sctx = ctx;

    char number[256];
    sstrnncpy(number, sizeof(number), number_val, number_len);

    switch (sctx->depth) {
    case 2:
        switch (sctx->stack[0]) {
        case BIND_JSON_OPCODES:
            metric_family_append(&sctx->fams[FAM_BIND_INCOMING_REQUESTS],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="opcode", .value=sctx->value1},
                                 NULL);
            break;
        case BIND_JSON_RCODES:
            metric_family_append(&sctx->fams[FAM_BIND_RESPONSE_RCODES],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="rcode", .value=sctx->value1},
                                 NULL);
            break;
        case BIND_JSON_QTYPES:
            metric_family_append(&sctx->fams[FAM_BIND_INCOMING_QUERIES],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="qtype", .value=sctx->value1},
                                 NULL);
            break;
        case BIND_JSON_NSSTATS:
            bind_append_metric(sctx->fams, sctx->labels, "nsstats:", sctx->value1,
                                           NULL,  NULL, number);
            break;
        case BIND_JSON_ZONESTATS:
            bind_append_metric(sctx->fams, sctx->labels, "zonestat:", sctx->value1,
                                           NULL,  NULL, number);
            break;
        case BIND_JSON_SOCKSTATS:
            bind_append_metric(sctx->fams, sctx->labels, "sockstat:", sctx->value1,
                                           NULL,  NULL, number);
            break;
        case BIND_JSON_MEMORY:
            bind_append_metric(sctx->fams, sctx->labels, "memory:", sctx->value1,
                                           NULL,  NULL, number);
            break;
        default:
            break;
        }
        break;
    case 3:
        switch (sctx->stack[1]) {
        case BIND_JSON_TRAFFIC_DNS_UDP_REQUESTS_SIZES_RECEIVED_IPV4:
            bind_traffic_histogram_append(&sctx->traffic[BIND_TRAFFIC_INCOMING_REQUESTS_UDP4_SIZE],
                                          sctx->value1, number);
            break;
        case BIND_JSON_TRAFFIC_DNS_UDP_RESPONSES_SIZES_SENT_IPV4:
            bind_traffic_histogram_append(&sctx->traffic[BIND_TRAFFIC_INCOMING_REQUESTS_UDP6_SIZE],
                                          sctx->value1, number);
            break;
        case BIND_JSON_TRAFFIC_DNS_TCP_REQUESTS_SIZES_RECEIVED_IPV4:
            bind_traffic_histogram_append(&sctx->traffic[BIND_TRAFFIC_INCOMING_REQUESTS_TCP4_SIZE],
                                          sctx->value1, number);
            break;
        case BIND_JSON_TRAFFIC_DNS_TCP_RESPONSES_SIZES_SENT_IPV4:
            bind_traffic_histogram_append(&sctx->traffic[BIND_TRAFFIC_INCOMING_REQUESTS_TCP6_SIZE],
                                          sctx->value1, number);
            break;
        case BIND_JSON_TRAFFIC_DNS_UDP_REQUESTS_SIZES_RECEIVED_IPV6:
            bind_traffic_histogram_append(&sctx->traffic[BIND_TRAFFIC_RESPONSES_UDP4_SIZE],
                                          sctx->value1, number);
            break;
        case BIND_JSON_TRAFFIC_DNS_UDP_RESPONSES_SIZES_SENT_IPV6:
            bind_traffic_histogram_append(&sctx->traffic[BIND_TRAFFIC_RESPONSES_UDP6_SIZE],
                                          sctx->value1, number);
            break;
        case BIND_JSON_TRAFFIC_DNS_TCP_REQUESTS_SIZES_RECEIVED_IPV6:
            bind_traffic_histogram_append(&sctx->traffic[BIND_TRAFFIC_RESPONSES_TCP4_SIZE],
                                          sctx->value1, number);
            break;
        case BIND_JSON_TRAFFIC_DNS_TCP_RESPONSES_SIZES_SENT_IPV6:
            bind_traffic_histogram_append(&sctx->traffic[BIND_TRAFFIC_RESPONSES_TCP6_SIZE],
                                          sctx->value1, number);
            break;
        default:
            break;
        }
        break;
    case 5:
        switch (sctx->stack[3]) {
        case BIND_JSON_VIEWS_RESOLVER_STATS:
            bind_append_metric(sctx->fams, sctx->labels, "resstat:", sctx->value2,
                                           "view", sctx->value1, number);
            break;
        case BIND_JSON_VIEWS_RESOLVER_QTYPES:
            metric_family_append(&sctx->fams[FAM_BIND_RESOLVER_QUERIES],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="view", .value=sctx->value1},
                                 &(label_pair_const_t){.name="type", .value=sctx->value2},
                                 NULL);
            break;
        case BIND_JSON_VIEWS_RESOLVER_CACHE:
            metric_family_append(&sctx->fams[FAM_BIND_RESOLVER_CACHE_RRSETS],
                                 VALUE_GAUGE(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="view", .value=sctx->value1},
                                 &(label_pair_const_t){.name="type", .value=sctx->value2},
                                 NULL);
            break;
        case BIND_JSON_VIEWS_RESOLVER_CACHESTATS:
            bind_append_metric(sctx->fams, sctx->labels, "cachestats:", sctx->value2,
                                           "view", sctx->value1, number);
            break;
        case BIND_JSON_VIEWS_RESOLVER_ADB:
            break;
        default:
            break;
        }
        break;
    }

    return true;
}

static bool bind_json_string(void *ctx, const char *string_val, size_t string_len)
{
    bind_json_ctx_t *sctx = ctx;

    if (sctx->depth == 1) {
        switch (sctx->stack[0]) {
        case BIND_JSON_BOOT_TIME: {
            time_t t = bind_get_timestamp((const char *)string_val, string_len);
            if (t > 0)
                metric_family_append(&sctx->fams[FAM_BIND_BOOT_TIME_SECONDS],
                                     VALUE_GAUGE(t), sctx->labels, NULL);
        }   break;
        case BIND_JSON_CONFIG_TIME: {
            time_t t = bind_get_timestamp((const char *)string_val, string_len);
            if (t > 0)
                metric_family_append(&sctx->fams[FAM_BIND_CONFIG_TIME_SECONDS],
                                     VALUE_GAUGE(t), sctx->labels, NULL);
        }   break;
        case BIND_JSON_CURRENT_TIME: {
            time_t t = bind_get_timestamp((const char *)string_val, string_len);
            if (t > 0)
                sctx->timestamp = TIME_T_TO_CDTIME_T(t);
        }   break;
        default:
            break;
        }
    }

    return true;
}

static bool bind_json_start_map(void *ctx)
{
    bind_json_ctx_t *sctx = ctx;
    sctx->depth++;
    sctx->stack[sctx->depth] = BIND_JSON_NONE;
    return true;
}

static bool bind_json_map_key(void * ctx, const char *ukey, size_t key_len)
{
    const char *key = (const char *)ukey;
    bind_json_ctx_t *sctx = ctx;

    switch (sctx->depth) {
    case 1:
        switch(key_len) {
        case 5: // views
            if (strncmp("views", key, key_len) == 0) {
                sctx->stack[0] = BIND_JSON_VIEWS;
            } else {
                sctx->stack[0] = BIND_JSON_NONE;
            }
            break;
        case 6: // qtypes rcodes memory
            if (strncmp("qtypes", key, key_len) == 0) {
                sctx->stack[0] = BIND_JSON_QTYPES;
            } else if (strncmp("rcodes", key, key_len) == 0) {
                sctx->stack[0] = BIND_JSON_RCODES;
            } else if (strncmp("memory", key, key_len) == 0) {
                sctx->stack[0] = BIND_JSON_MEMORY;
            } else {
                sctx->stack[0] = BIND_JSON_NONE;
            }
            break;
        case 7: // opcodes  nsstats traffic
            if (strncmp("opcodes", key, key_len) == 0) {
                sctx->stack[0] = BIND_JSON_OPCODES;
            } else if (strncmp("nsstats", key, key_len) == 0) {
                sctx->stack[0] = BIND_JSON_NSSTATS;
            } else if (strncmp("traffic", key, key_len) == 0) {
                sctx->stack[0] = BIND_JSON_TRAFFIC;
            } else {
                sctx->stack[0] = BIND_JSON_NONE;
            }
            break;
        case 9: // sockstatsa boot-time
            if (strncmp("sockstats", key, key_len) == 0) {
                sctx->stack[0] = BIND_JSON_SOCKSTATS;
            } else if (strncmp("boot-time", key, key_len) == 0) {
                sctx->stack[0] = BIND_JSON_BOOT_TIME;
            } else {
                sctx->stack[0] = BIND_JSON_NONE;
            }
            break;
        case 11: // config-time
            if (strncmp("config-time", key, key_len) == 0) {
                sctx->stack[0] = BIND_JSON_CONFIG_TIME;
            } else {
                sctx->stack[0] = BIND_JSON_NONE;
            }
            break;
        case 12: // current-time
            if (strncmp("current-time", key, key_len) == 0) {
                sctx->stack[0] = BIND_JSON_CURRENT_TIME;
            } else {
                sctx->stack[0] = BIND_JSON_NONE;
            }
            break;
        default:
            sctx->stack[0] = BIND_JSON_NONE;
            break;
        }
        break;
    case 2:
        switch (sctx->stack[0]) {
        case BIND_JSON_OPCODES:
        case BIND_JSON_RCODES:
        case BIND_JSON_QTYPES:
        case BIND_JSON_NSSTATS:
        case BIND_JSON_SOCKSTATS:
        case BIND_JSON_MEMORY:
        case BIND_JSON_VIEWS:
            sstrnncpy(sctx->value1, sizeof(sctx->value1), key, key_len);
            break;
        case BIND_JSON_TRAFFIC:
            switch(key_len) {
            case 33:
                if (strncmp("dns-udp-responses-sizes-sent-ipv4", key, key_len) == 0) {
                    sctx->stack[1] = BIND_JSON_TRAFFIC_DNS_UDP_RESPONSES_SIZES_SENT_IPV4;
                } else if (strncmp("dns-tcp-responses-sizes-sent-ipv4", key, key_len) == 0) {
                    sctx->stack[1] = BIND_JSON_TRAFFIC_DNS_TCP_RESPONSES_SIZES_SENT_IPV4;
                } else if (strncmp("dns-udp-responses-sizes-sent-ipv6", key, key_len) == 0) {
                    sctx->stack[1] = BIND_JSON_TRAFFIC_DNS_UDP_RESPONSES_SIZES_SENT_IPV6;
                } else if (strncmp("dns-tcp-responses-sizes-sent-ipv6", key, key_len) == 0) {
                    sctx->stack[1] = BIND_JSON_TRAFFIC_DNS_TCP_RESPONSES_SIZES_SENT_IPV6;
                }
                break;
            case 36:
                if (strncmp("dns-udp-requests-sizes-received-ipv4", key, key_len) == 0) {
                    sctx->stack[1] = BIND_JSON_TRAFFIC_DNS_UDP_REQUESTS_SIZES_RECEIVED_IPV4;
                } else if (strncmp("dns-tcp-requests-sizes-received-ipv4", key, key_len) == 0) {
                    sctx->stack[1] = BIND_JSON_TRAFFIC_DNS_TCP_REQUESTS_SIZES_RECEIVED_IPV4;
                } else if (strncmp("dns-udp-requests-sizes-received-ipv6", key, key_len) == 0) {
                    sctx->stack[1] = BIND_JSON_TRAFFIC_DNS_UDP_REQUESTS_SIZES_RECEIVED_IPV6;
                } else if (strncmp("dns-tcp-requests-sizes-received-ipv6", key, key_len) == 0) {
                    sctx->stack[1] = BIND_JSON_TRAFFIC_DNS_TCP_REQUESTS_SIZES_RECEIVED_IPV6;
                }
                break;
            }
            break;
        default:
            break;
        }
        break;
    case 3:
        switch (sctx->stack[0]) {
        case BIND_JSON_VIEWS:
            if ((key_len == strlen("resolver")) && (strncmp("resolver", key, key_len) == 0)) {
                sctx->stack[2] = BIND_JSON_VIEWS_RESOLVER;
            }
            break;
        case BIND_JSON_TRAFFIC:
            switch (sctx->stack[1]) {
            case BIND_JSON_TRAFFIC_DNS_UDP_REQUESTS_SIZES_RECEIVED_IPV4:
            case BIND_JSON_TRAFFIC_DNS_UDP_RESPONSES_SIZES_SENT_IPV4:
            case BIND_JSON_TRAFFIC_DNS_TCP_REQUESTS_SIZES_RECEIVED_IPV4:
            case BIND_JSON_TRAFFIC_DNS_TCP_RESPONSES_SIZES_SENT_IPV4:
            case BIND_JSON_TRAFFIC_DNS_UDP_REQUESTS_SIZES_RECEIVED_IPV6:
            case BIND_JSON_TRAFFIC_DNS_UDP_RESPONSES_SIZES_SENT_IPV6:
            case BIND_JSON_TRAFFIC_DNS_TCP_REQUESTS_SIZES_RECEIVED_IPV6:
            case BIND_JSON_TRAFFIC_DNS_TCP_RESPONSES_SIZES_SENT_IPV6:
                sstrnncpy(sctx->value1, sizeof(sctx->value1), key, key_len);
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
        break;
    case 4:
        if (sctx->stack[2] == BIND_JSON_VIEWS_RESOLVER) {
            switch(key_len) {
            case 3:  // abd
                if (strncmp("adb", key, key_len) == 0) {
                    sctx->stack[3] = BIND_JSON_VIEWS_RESOLVER_ADB;
                } else {
                    sctx->stack[3] = BIND_JSON_NONE;
                }
                break;
            case 5:  // stats cache
                if (strncmp("stats", key, key_len) == 0) {
                    sctx->stack[3] = BIND_JSON_VIEWS_RESOLVER_STATS;
                } else if (strncmp("cache", key, key_len) == 0) {
                    sctx->stack[3] = BIND_JSON_VIEWS_RESOLVER_CACHE;
                } else {
                    sctx->stack[3] = BIND_JSON_NONE;
                }
                break;
            case 6:  // qtypes
                if (strncmp("qtypes", key, key_len) == 0) {
                    sctx->stack[3] = BIND_JSON_VIEWS_RESOLVER_QTYPES;
                } else {
                    sctx->stack[3] = BIND_JSON_NONE;
                }
                break;
            case 10: // cachestats
                if (strncmp("cachestats", key, key_len) == 0) {
                    sctx->stack[3] = BIND_JSON_VIEWS_RESOLVER_CACHESTATS;
                } else {
                    sctx->stack[3] = BIND_JSON_NONE;
                }
                break;
            default:
                sctx->stack[3] = BIND_JSON_NONE;
                break;
            }
        }
        break;
    case 5:
        switch (sctx->stack[3]) {
        case BIND_JSON_VIEWS_RESOLVER_QTYPES:
        case BIND_JSON_VIEWS_RESOLVER_CACHE:
        case BIND_JSON_VIEWS_RESOLVER_STATS:
        case BIND_JSON_VIEWS_RESOLVER_CACHESTATS:
        case BIND_JSON_VIEWS_RESOLVER_ADB:
            sstrnncpy(sctx->value2, sizeof(sctx->value2), key, key_len);
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    return true;
}

static bool bind_json_end_map(void *ctx)
{
    bind_json_ctx_t *sctx = ctx;

    sctx->depth--;
    if (sctx->depth > 0) {
        sctx->stack[sctx->depth-1] = BIND_JSON_NONE;
    } else {
        sctx->depth = 0;
    }

    return true;
}

static json_callbacks_t bind_json_callbacks = {
    .json_null        = NULL,
    .json_boolean     = NULL,
    .json_integer     = NULL,
    .json_double      = NULL,
    .json_number      = bind_json_number,
    .json_string      = bind_json_string,
    .json_start_map   = bind_json_start_map,
    .json_map_key     = bind_json_map_key,
    .json_end_map     = bind_json_end_map,
    .json_start_array = NULL,
    .json_end_array   = NULL,
};

int bind_json_parse(bind_json_ctx_t *ctx)
{
    json_parser_init(&ctx->handle, 0, &bind_json_callbacks, &ctx);
    return 0;
}

int bind_json_parse_chunk(bind_json_ctx_t *ctx, const char unsigned *data, int size)
{
    json_status_t status = json_parser_parse(&ctx->handle, data, size);
    if (status != JSON_STATUS_OK)
        return -1;
    return 0;
}

int bind_json_parse_end(bind_json_ctx_t *ctx)
{
    json_status_t status = json_parser_complete(&ctx->handle);
    if (status != JSON_STATUS_OK) {
        unsigned char *errmsg = json_parser_get_error(&ctx->handle, 0, NULL, 0);
        PLUGIN_ERROR("json_parse_complete failed: %s", (const char *)errmsg);
        json_parser_free_error(errmsg);
        json_parser_free(&ctx->handle);
        return -1;
    }

    json_parser_free(&ctx->handle);
    return 0;
}
