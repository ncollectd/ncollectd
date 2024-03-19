/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libxson/json_parse.h"

typedef enum {
    BIND_JSON_NONE,
    BIND_JSON_STATS_VERSION,
    BIND_JSON_BOOT_TIME,
    BIND_JSON_CONFIG_TIME,
    BIND_JSON_CURRENT_TIME,
    BIND_JSON_OPCODES,
    BIND_JSON_RCODES,
    BIND_JSON_QTYPES,
    BIND_JSON_NSSTATS,
    BIND_JSON_SOCKSTATS,
    BIND_JSON_ZONESTATS,
    BIND_JSON_MEMORY,
    BIND_JSON_VIEWS,
    BIND_JSON_VIEWS_RESOLVER,
    BIND_JSON_VIEWS_RESOLVER_STATS,
    BIND_JSON_VIEWS_RESOLVER_QTYPES,
    BIND_JSON_VIEWS_RESOLVER_CACHE,
    BIND_JSON_VIEWS_RESOLVER_CACHESTATS,
    BIND_JSON_VIEWS_RESOLVER_ADB,
    BIND_JSON_TRAFFIC,
    BIND_JSON_TRAFFIC_DNS_UDP_REQUESTS_SIZES_RECEIVED_IPV4,
    BIND_JSON_TRAFFIC_DNS_UDP_RESPONSES_SIZES_SENT_IPV4,
    BIND_JSON_TRAFFIC_DNS_TCP_REQUESTS_SIZES_RECEIVED_IPV4,
    BIND_JSON_TRAFFIC_DNS_TCP_RESPONSES_SIZES_SENT_IPV4,
    BIND_JSON_TRAFFIC_DNS_UDP_REQUESTS_SIZES_RECEIVED_IPV6,
    BIND_JSON_TRAFFIC_DNS_UDP_RESPONSES_SIZES_SENT_IPV6,
    BIND_JSON_TRAFFIC_DNS_TCP_REQUESTS_SIZES_RECEIVED_IPV6,
    BIND_JSON_TRAFFIC_DNS_TCP_RESPONSES_SIZES_SENT_IPV6,
} bind_json_key_t;

typedef struct {
    char value1[256];
    char value2[256];
    bind_json_key_t stack[JSON_MAX_DEPTH];
    size_t depth;
    json_parser_t handle;
    metric_family_t *fams;
    cdtime_t timestamp;
    label_set_t *labels;
    histogram_t *traffic[BIND_TRAFFIC_MAX];
} bind_json_ctx_t;

int bind_json_parse(bind_json_ctx_t *ctx);

int bind_json_parse_chunk(bind_json_ctx_t *ctx, const char unsigned *data, int size);

int bind_json_parse_end(bind_json_ctx_t *ctx);
