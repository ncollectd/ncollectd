// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/socket.h"
#include "libutils/common.h" 
#include "libutils/random.h" 

#include "hmac.h"

/* AIX doesn't have MSG_DONTWAIT */
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT MSG_NONBLOCK
#endif

#define RADIUS_RANDOM_VECTOR_LEN 16

#define MD5_DIGEST_SIZE  16

#define RADIUS_ATTR_ID_VENDOR_SPECIFIC 26
#define RADIUS_ATTR_ID_MESSAGE_AUTHENTICATOR 80

#define RADIUS_CODE_ACCESS_ACCEPT 2
#define RADIUS_CODE_STATUS_SERVER 12
#define RADIUS_CODE_STATUS_CLIENT 13

#define RADIUS_ATTR_TYPE_INTEGER 1
#define RADIUS_ATTR_TYPE_DATE 3

#define FREERADIUS_ATTR_VENDOR_ID 11344
#define FREERADIUS_STATISTICS_TYPE 127
#define FREERADIUS_STATISTICS_ALL 0x1f

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t length;
    uint8_t data[];
} radius_avp_raw_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t length;
    uint32_t data;
} radius_avp_int_t;

typedef struct __attribute__((packed)) {
    uint8_t code;
    uint8_t id;
    uint16_t length;
    uint8_t vector[RADIUS_RANDOM_VECTOR_LEN];
} radius_hdr_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t length;
    uint32_t val;
} radius_vsa_int_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t length;
    uint32_t vendor_id;
    radius_vsa_int_t vsa;
} radius_avp_vsa_int_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t length;
    uint8_t data[MD5_DIGEST_SIZE];
} radius_avp_auth_t;

typedef struct __attribute__((packed)) {
    radius_hdr_t hdr;
    radius_avp_vsa_int_t vsa;
    radius_avp_auth_t auth;
} radius_status_server_t;

enum {
    FREERADIUS_TOTAL_ACCESS_REQUESTS                = 128, /* integer */
    FREERADIUS_TOTAL_ACCESS_ACCEPTS                 = 129, /* integer */
    FREERADIUS_TOTAL_ACCESS_REJECTS                 = 130, /* integer */
    FREERADIUS_TOTAL_ACCESS_CHALLENGES              = 131, /* integer */
    FREERADIUS_TOTAL_AUTH_RESPONSES                 = 132, /* integer */
    FREERADIUS_TOTAL_AUTH_DUPLICATE_REQUESTS        = 133, /* integer */
    FREERADIUS_TOTAL_AUTH_MALFORMED_REQUESTS        = 134, /* integer */
    FREERADIUS_TOTAL_AUTH_INVALID_REQUESTS          = 135, /* integer */
    FREERADIUS_TOTAL_AUTH_DROPPED_REQUESTS          = 136, /* integer */
    FREERADIUS_TOTAL_AUTH_UNKNOWN_TYPES             = 137, /* integer */
    FREERADIUS_TOTAL_PROXY_ACCESS_REQUESTS          = 138, /* integer */
    FREERADIUS_TOTAL_PROXY_ACCESS_ACCEPTS           = 139, /* integer */
    FREERADIUS_TOTAL_PROXY_ACCESS_REJECTS           = 140, /* integer */
    FREERADIUS_TOTAL_PROXY_ACCESS_CHALLENGES        = 141, /* integer */
    FREERADIUS_TOTAL_PROXY_AUTH_RESPONSES           = 142, /* integer */
    FREERADIUS_TOTAL_PROXY_AUTH_DUPLICATE_REQUESTS  = 143, /* integer */
    FREERADIUS_TOTAL_PROXY_AUTH_MALFORMED_REQUESTS  = 144, /* integer */
    FREERADIUS_TOTAL_PROXY_AUTH_INVALID_REQUESTS    = 145, /* integer */
    FREERADIUS_TOTAL_PROXY_AUTH_DROPPED_REQUESTS    = 146, /* integer */
    FREERADIUS_TOTAL_PROXY_AUTH_UNKNOWN_TYPES       = 147, /* integer */
    FREERADIUS_TOTAL_ACCOUNTING_REQUESTS            = 148, /* integer */
    FREERADIUS_TOTAL_ACCOUNTING_RESPONSES           = 149, /* integer */
    FREERADIUS_TOTAL_ACCT_DUPLICATE_REQUESTS        = 150, /* integer */
    FREERADIUS_TOTAL_ACCT_MALFORMED_REQUESTS        = 151, /* integer */
    FREERADIUS_TOTAL_ACCT_INVALID_REQUESTS          = 152, /* integer */
    FREERADIUS_TOTAL_ACCT_DROPPED_REQUESTS          = 153, /* integer */
    FREERADIUS_TOTAL_ACCT_UNKNOWN_TYPES             = 154, /* integer */
    FREERADIUS_TOTAL_PROXY_ACCOUNTING_REQUESTS      = 155, /* integer */
    FREERADIUS_TOTAL_PROXY_ACCOUNTING_RESPONSES     = 156, /* integer */
    FREERADIUS_TOTAL_PROXY_ACCT_DUPLICATE_REQUESTS  = 157, /* integer */
    FREERADIUS_TOTAL_PROXY_ACCT_MALFORMED_REQUESTS  = 158, /* integer */
    FREERADIUS_TOTAL_PROXY_ACCT_INVALID_REQUESTS    = 159, /* integer */
    FREERADIUS_TOTAL_PROXY_ACCT_DROPPED_REQUESTS    = 160, /* integer */
    FREERADIUS_TOTAL_PROXY_ACCT_UNKNOWN_TYPES       = 161, /* integer */
    FREERADIUS_QUEUE_LEN_INTERNAL                   = 162, /* integer */
    FREERADIUS_QUEUE_LEN_PROXY                      = 163, /* integer */
    FREERADIUS_QUEUE_LEN_AUTH                       = 164, /* integer */
    FREERADIUS_QUEUE_LEN_ACCT                       = 165, /* integer */
    FREERADIUS_QUEUE_LEN_DETAIL                     = 166, /* integer */
    FREERADIUS_STATS_CLIENT_IP_ADDRESS              = 167, /* ipaddr */
    FREERADIUS_STATS_CLIENT_NUMBER                  = 168, /* integer */
    FREERADIUS_STATS_CLIENT_NETMASK                 = 169, /* integer */
    FREERADIUS_STATS_SERVER_IP_ADDRESS              = 170, /* ipaddr */
    FREERADIUS_STATS_SERVER_PORT                    = 171, /* integer */
    FREERADIUS_STATS_SERVER_OUTSTANDING_REQUESTS    = 172, /* integer */
    FREERADIUS_STATS_SERVER_STATE                   = 173, /* integer: alive (0) zombie (1) dead (2) idle (3) */
    FREERADIUS_STATS_SERVER_TIME_OF_DEATH           = 174, /* date */
    FREERADIUS_STATS_SERVER_TIME_OF_LIFE            = 175, /* date */
    FREERADIUS_STATS_START_TIME                     = 176, /* date */
    FREERADIUS_STATS_HUP_TIME                       = 177, /* date */
    FREERADIUS_SERVER_EMA_WINDOW                    = 178, /* integer */
    FREERADIUS_SERVER_EMA_USEC_WINDOW_1             = 179, /* integer */
    FREERADIUS_SERVER_EMA_USEC_WINDOW_10            = 180, /* integer */
    FREERADIUS_QUEUE_PPS_IN                         = 181, /* integer */
    FREERADIUS_QUEUE_PPS_OUT                        = 182, /* integer */
    FREERADIUS_QUEUE_USE_PERCENTAGE                 = 183, /* integer */
    FREERADIUS_STATS_LAST_PACKET_RECV               = 184, /* date */
    FREERADIUS_STATS_LAST_PACKET_SENT               = 185, /* date */
};

enum {
    FAM_FREERADIUS_UP,
    FAM_FREERADIUS_ACCESS_REQUESTS,
    FAM_FREERADIUS_ACCESS_ACCEPTS,
    FAM_FREERADIUS_ACCESS_REJECTS,
    FAM_FREERADIUS_ACCESS_CHALLENGES,
    FAM_FREERADIUS_AUTH_RESPONSES,
    FAM_FREERADIUS_AUTH_DUPLICATE_REQUESTS,
    FAM_FREERADIUS_AUTH_MALFORMED_REQUESTS,
    FAM_FREERADIUS_AUTH_INVALID_REQUESTS,
    FAM_FREERADIUS_AUTH_DROPPED_REQUESTS,
    FAM_FREERADIUS_AUTH_UNKNOWN_TYPES,
    FAM_FREERADIUS_PROXY_ACCESS_REQUESTS,
    FAM_FREERADIUS_PROXY_ACCESS_ACCEPTS,
    FAM_FREERADIUS_PROXY_ACCESS_REJECTS,
    FAM_FREERADIUS_PROXY_ACCESS_CHALLENGES,
    FAM_FREERADIUS_PROXY_AUTH_RESPONSES,
    FAM_FREERADIUS_PROXY_AUTH_DUPLICATE_REQUESTS,
    FAM_FREERADIUS_PROXY_AUTH_MALFORMED_REQUESTS,
    FAM_FREERADIUS_PROXY_AUTH_INVALID_REQUESTS,
    FAM_FREERADIUS_PROXY_AUTH_DROPPED_REQUESTS,
    FAM_FREERADIUS_PROXY_AUTH_UNKNOWN_TYPES,
    FAM_FREERADIUS_ACCT_REQUESTS,
    FAM_FREERADIUS_ACCT_RESPONSES,
    FAM_FREERADIUS_ACCT_DUPLICATE_REQUESTS,
    FAM_FREERADIUS_ACCT_MALFORMED_REQUESTS,
    FAM_FREERADIUS_ACCT_INVALID_REQUESTS,
    FAM_FREERADIUS_ACCT_DROPPED_REQUESTS,
    FAM_FREERADIUS_ACCT_UNKNOWN_TYPES,
    FAM_FREERADIUS_PROXY_ACCT_REQUESTS,
    FAM_FREERADIUS_PROXY_ACCT_RESPONSES,
    FAM_FREERADIUS_PROXY_ACCT_DUPLICATE_REQUESTS,
    FAM_FREERADIUS_PROXY_ACCT_MALFORMED_REQUESTS,
    FAM_FREERADIUS_PROXY_ACCT_INVALID_REQUESTS,
    FAM_FREERADIUS_PROXY_ACCT_DROPPED_REQUESTS,
    FAM_FREERADIUS_PROXY_ACCT_UNKNOWN_TYPES,
    FAM_FREERADIUS_QUEUE_LEN_INTERNAL,
    FAM_FREERADIUS_QUEUE_LEN_PROXY,
    FAM_FREERADIUS_QUEUE_LEN_AUTH,
    FAM_FREERADIUS_QUEUE_LEN_ACCT,
    FAM_FREERADIUS_QUEUE_LEN_DETAIL,
    FAM_FREERADIUS_LAST_PACKET_RECV_SECONDS,
    FAM_FREERADIUS_LAST_PACKET_SENT_SECONDS,
    FAM_FREERADIUS_START_TIME_SECONDS,
    FAM_FREERADIUS_HUP_TIME_SECONDS,
    FAM_FREERADIUS_STATE,
    FAM_FREERADIUS_TIME_OF_DEATH_SECONDS,
    FAM_FREERADIUS_TIME_OF_LIFE_SECONDS,
    FAM_FREERADIUS_EMA_WINDOW,
    FAM_FREERADIUS_EMA_WINDOW1_SECONDS,
    FAM_FREERADIUS_EMA_WINDOW10_SECONDS,
    FAM_FREERADIUS_OUTSTANDING_REQUESTS,
    FAM_FREERADIUS_QUEUE_PPS_IN,
    FAM_FREERADIUS_QUEUE_PPS_OUT,
    FAM_FREERADIUS_QUEUE_USE_PERCENTAGE,
    FAM_FREERADIUS_MAX
};

static struct {
    int fam;
    double scale;
} freeradius_stats[] = {
    [FREERADIUS_TOTAL_ACCESS_REQUESTS]               = { FAM_FREERADIUS_ACCESS_REQUESTS,               1.0  },
    [FREERADIUS_TOTAL_ACCESS_ACCEPTS]                = { FAM_FREERADIUS_ACCESS_ACCEPTS,                1.0  },
    [FREERADIUS_TOTAL_ACCESS_REJECTS]                = { FAM_FREERADIUS_ACCESS_REJECTS,                1.0  },
    [FREERADIUS_TOTAL_ACCESS_CHALLENGES]             = { FAM_FREERADIUS_ACCESS_CHALLENGES,             1.0  },
    [FREERADIUS_TOTAL_AUTH_RESPONSES]                = { FAM_FREERADIUS_AUTH_RESPONSES,                1.0  },
    [FREERADIUS_TOTAL_AUTH_DUPLICATE_REQUESTS]       = { FAM_FREERADIUS_AUTH_DUPLICATE_REQUESTS,       1.0  },
    [FREERADIUS_TOTAL_AUTH_MALFORMED_REQUESTS]       = { FAM_FREERADIUS_AUTH_MALFORMED_REQUESTS,       1.0  },
    [FREERADIUS_TOTAL_AUTH_INVALID_REQUESTS]         = { FAM_FREERADIUS_AUTH_INVALID_REQUESTS,         1.0  },
    [FREERADIUS_TOTAL_AUTH_DROPPED_REQUESTS]         = { FAM_FREERADIUS_AUTH_DROPPED_REQUESTS,         1.0  },
    [FREERADIUS_TOTAL_AUTH_UNKNOWN_TYPES]            = { FAM_FREERADIUS_AUTH_UNKNOWN_TYPES,            1.0  },
    [FREERADIUS_TOTAL_PROXY_ACCESS_REQUESTS]         = { FAM_FREERADIUS_PROXY_ACCESS_REQUESTS,         1.0  },
    [FREERADIUS_TOTAL_PROXY_ACCESS_ACCEPTS]          = { FAM_FREERADIUS_PROXY_ACCESS_ACCEPTS,          1.0  },
    [FREERADIUS_TOTAL_PROXY_ACCESS_REJECTS]          = { FAM_FREERADIUS_PROXY_ACCESS_REJECTS,          1.0  },
    [FREERADIUS_TOTAL_PROXY_ACCESS_CHALLENGES]       = { FAM_FREERADIUS_PROXY_ACCESS_CHALLENGES,       1.0  },
    [FREERADIUS_TOTAL_PROXY_AUTH_RESPONSES]          = { FAM_FREERADIUS_PROXY_AUTH_RESPONSES,          1.0  },
    [FREERADIUS_TOTAL_PROXY_AUTH_DUPLICATE_REQUESTS] = { FAM_FREERADIUS_PROXY_AUTH_DUPLICATE_REQUESTS, 1.0  },
    [FREERADIUS_TOTAL_PROXY_AUTH_MALFORMED_REQUESTS] = { FAM_FREERADIUS_PROXY_AUTH_MALFORMED_REQUESTS, 1.0  },
    [FREERADIUS_TOTAL_PROXY_AUTH_INVALID_REQUESTS]   = { FAM_FREERADIUS_PROXY_AUTH_INVALID_REQUESTS,   1.0  },
    [FREERADIUS_TOTAL_PROXY_AUTH_DROPPED_REQUESTS]   = { FAM_FREERADIUS_PROXY_AUTH_DROPPED_REQUESTS,   1.0  },
    [FREERADIUS_TOTAL_PROXY_AUTH_UNKNOWN_TYPES]      = { FAM_FREERADIUS_PROXY_AUTH_UNKNOWN_TYPES,      1.0  },
    [FREERADIUS_TOTAL_ACCOUNTING_REQUESTS]           = { FAM_FREERADIUS_ACCT_REQUESTS,                 1.0  },
    [FREERADIUS_TOTAL_ACCOUNTING_RESPONSES]          = { FAM_FREERADIUS_ACCT_RESPONSES,                1.0  },
    [FREERADIUS_TOTAL_ACCT_DUPLICATE_REQUESTS]       = { FAM_FREERADIUS_ACCT_DUPLICATE_REQUESTS,       1.0  },
    [FREERADIUS_TOTAL_ACCT_MALFORMED_REQUESTS]       = { FAM_FREERADIUS_ACCT_MALFORMED_REQUESTS,       1.0  },
    [FREERADIUS_TOTAL_ACCT_INVALID_REQUESTS]         = { FAM_FREERADIUS_ACCT_INVALID_REQUESTS,         1.0  },
    [FREERADIUS_TOTAL_ACCT_DROPPED_REQUESTS]         = { FAM_FREERADIUS_ACCT_DROPPED_REQUESTS,         1.0  },
    [FREERADIUS_TOTAL_ACCT_UNKNOWN_TYPES]            = { FAM_FREERADIUS_ACCT_UNKNOWN_TYPES,            1.0  },
    [FREERADIUS_TOTAL_PROXY_ACCOUNTING_REQUESTS]     = { FAM_FREERADIUS_PROXY_ACCT_REQUESTS,           1.0  },
    [FREERADIUS_TOTAL_PROXY_ACCOUNTING_RESPONSES]    = { FAM_FREERADIUS_PROXY_ACCT_RESPONSES,          1.0  },
    [FREERADIUS_TOTAL_PROXY_ACCT_DUPLICATE_REQUESTS] = { FAM_FREERADIUS_PROXY_ACCT_DUPLICATE_REQUESTS, 1.0  },
    [FREERADIUS_TOTAL_PROXY_ACCT_MALFORMED_REQUESTS] = { FAM_FREERADIUS_PROXY_ACCT_MALFORMED_REQUESTS, 1.0  },
    [FREERADIUS_TOTAL_PROXY_ACCT_INVALID_REQUESTS]   = { FAM_FREERADIUS_PROXY_ACCT_INVALID_REQUESTS,   1.0  },
    [FREERADIUS_TOTAL_PROXY_ACCT_DROPPED_REQUESTS]   = { FAM_FREERADIUS_PROXY_ACCT_DROPPED_REQUESTS,   1.0  },
    [FREERADIUS_TOTAL_PROXY_ACCT_UNKNOWN_TYPES]      = { FAM_FREERADIUS_PROXY_ACCT_UNKNOWN_TYPES,      1.0  },
    [FREERADIUS_QUEUE_LEN_INTERNAL]                  = { FAM_FREERADIUS_QUEUE_LEN_INTERNAL,            1.0  },
    [FREERADIUS_QUEUE_LEN_PROXY]                     = { FAM_FREERADIUS_QUEUE_LEN_PROXY,               1.0  },
    [FREERADIUS_QUEUE_LEN_AUTH]                      = { FAM_FREERADIUS_QUEUE_LEN_AUTH,                1.0  },
    [FREERADIUS_QUEUE_LEN_ACCT]                      = { FAM_FREERADIUS_QUEUE_LEN_ACCT,                1.0  },
    [FREERADIUS_QUEUE_LEN_DETAIL]                    = { FAM_FREERADIUS_QUEUE_LEN_DETAIL,              1.0  },
    [FREERADIUS_STATS_LAST_PACKET_RECV]              = { FAM_FREERADIUS_LAST_PACKET_RECV_SECONDS,      1.0  },
    [FREERADIUS_STATS_LAST_PACKET_SENT]              = { FAM_FREERADIUS_LAST_PACKET_SENT_SECONDS,      1.0  },
    [FREERADIUS_STATS_START_TIME]                    = { FAM_FREERADIUS_START_TIME_SECONDS,            1.0  },
    [FREERADIUS_STATS_HUP_TIME]                      = { FAM_FREERADIUS_HUP_TIME_SECONDS,              1.0  },
    [FREERADIUS_STATS_SERVER_STATE]                  = { FAM_FREERADIUS_STATE,                         1.0  },
    [FREERADIUS_STATS_SERVER_TIME_OF_DEATH]          = { FAM_FREERADIUS_TIME_OF_DEATH_SECONDS,         1.0  },
    [FREERADIUS_STATS_SERVER_TIME_OF_LIFE]           = { FAM_FREERADIUS_TIME_OF_LIFE_SECONDS,          1.0  },
    [FREERADIUS_SERVER_EMA_WINDOW]                   = { FAM_FREERADIUS_EMA_WINDOW,                    1.0  },
    [FREERADIUS_SERVER_EMA_USEC_WINDOW_1]            = { FAM_FREERADIUS_EMA_WINDOW1_SECONDS,           1e-6 },
    [FREERADIUS_SERVER_EMA_USEC_WINDOW_10]           = { FAM_FREERADIUS_EMA_WINDOW10_SECONDS,          1e-6 },
    [FREERADIUS_STATS_SERVER_OUTSTANDING_REQUESTS]   = { FAM_FREERADIUS_OUTSTANDING_REQUESTS,          1.0  },
    [FREERADIUS_QUEUE_PPS_IN]                        = { FAM_FREERADIUS_QUEUE_PPS_IN,                  1.0  },
    [FREERADIUS_QUEUE_PPS_OUT]                       = { FAM_FREERADIUS_QUEUE_PPS_OUT,                 1.0  },
    [FREERADIUS_QUEUE_USE_PERCENTAGE]                = { FAM_FREERADIUS_QUEUE_USE_PERCENTAGE,          1.0  },
};
static size_t freeradius_stats_size = STATIC_ARRAY_SIZE(freeradius_stats);

typedef struct {
    int fd;
    char *name;
    char *host;
    int port;
    char *secret;
    cdtime_t timeout;
    label_set_t labels;
    plugin_filter_t *filter;
    metric_family_t fams[FAM_FREERADIUS_MAX];
} freeradius_ctx_t;

static metric_family_t fams[FAM_FREERADIUS_MAX] = {
    [FAM_FREERADIUS_UP] = {
        .name = "freeradius_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the freeradius server be reached."
    },
    [FAM_FREERADIUS_ACCESS_REQUESTS] = {
        .name = "freeradius_access_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total access requests"
    },
    [FAM_FREERADIUS_ACCESS_ACCEPTS] = {
        .name = "freeradius_access_accepts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total access accepts"
    },
    [FAM_FREERADIUS_ACCESS_REJECTS] = {
        .name = "freeradius_access_rejects",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total access rejects"
    },
    [FAM_FREERADIUS_ACCESS_CHALLENGES] = {
        .name = "freeradius_access_challenges",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total access challenges"
    },
    [FAM_FREERADIUS_AUTH_RESPONSES] = {
        .name = "freeradius_auth_responses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total auth responses"
    },
    [FAM_FREERADIUS_AUTH_DUPLICATE_REQUESTS] = {
        .name = "freeradius_auth_duplicate_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total auth duplicate requests"
    },
    [FAM_FREERADIUS_AUTH_MALFORMED_REQUESTS] = {
        .name = "freeradius_auth_malformed_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total auth malformed requests"
    },
    [FAM_FREERADIUS_AUTH_INVALID_REQUESTS] = {
        .name = "freeradius_auth_invalid_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total auth invalid requests"
    },
    [FAM_FREERADIUS_AUTH_DROPPED_REQUESTS] = {
        .name = "freeradius_auth_dropped_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total auth dropped requests"
    },
    [FAM_FREERADIUS_AUTH_UNKNOWN_TYPES] = {
        .name = "freeradius_auth_unknown_types",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total auth unknown types"
    },
    [FAM_FREERADIUS_PROXY_ACCESS_REQUESTS] = {
        .name = "freeradius_proxy_access_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy access requests"
    },
    [FAM_FREERADIUS_PROXY_ACCESS_ACCEPTS] = {
        .name = "freeradius_proxy_access_accepts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy access accepts"
    },
    [FAM_FREERADIUS_PROXY_ACCESS_REJECTS] = {
        .name = "freeradius_proxy_access_rejects",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy access rejects"
    },
    [FAM_FREERADIUS_PROXY_ACCESS_CHALLENGES] = {
        .name = "freeradius_proxy_access_challenges",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy access challenges"
    },
    [FAM_FREERADIUS_PROXY_AUTH_RESPONSES] = {
        .name = "freeradius_proxy_auth_responses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy auth responses"
    },
    [FAM_FREERADIUS_PROXY_AUTH_DUPLICATE_REQUESTS] = {
        .name = "freeradius_proxy_auth_duplicate_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy auth duplicate requests"
    },
    [FAM_FREERADIUS_PROXY_AUTH_MALFORMED_REQUESTS] = {
        .name = "freeradius_proxy_auth_malformed_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy auth malformed requests"
    },
    [FAM_FREERADIUS_PROXY_AUTH_INVALID_REQUESTS] = {
        .name = "freeradius_proxy_auth_invalid_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy auth invalid requests"
    },
    [FAM_FREERADIUS_PROXY_AUTH_DROPPED_REQUESTS] = {
        .name = "freeradius_proxy_auth_dropped_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy auth dropped requests"
    },
    [FAM_FREERADIUS_PROXY_AUTH_UNKNOWN_TYPES] = {
        .name = "freeradius_proxy_auth_unknown_types",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy auth unknown types"
    },
    [FAM_FREERADIUS_ACCT_REQUESTS] = {
        .name = "freeradius_acct_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total acct requests"
    },
    [FAM_FREERADIUS_ACCT_RESPONSES] = {
        .name = "freeradius_acct_responses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total acct responses"
    },
    [FAM_FREERADIUS_ACCT_DUPLICATE_REQUESTS] = {
        .name = "freeradius_acct_duplicate_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total acct duplicate requests"
    },
    [FAM_FREERADIUS_ACCT_MALFORMED_REQUESTS] = {
        .name = "freeradius_acct_malformed_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total acct malformed requests"
    },
    [FAM_FREERADIUS_ACCT_INVALID_REQUESTS] = {
        .name = "freeradius_acct_invalid_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total acct invalid requests"
    },
    [FAM_FREERADIUS_ACCT_DROPPED_REQUESTS] = {
        .name = "freeradius_acct_dropped_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total acct dropped requests"
    },
    [FAM_FREERADIUS_ACCT_UNKNOWN_TYPES] = {
        .name = "freeradius_acct_unknown_types",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total acct unknown types"
    },
    [FAM_FREERADIUS_PROXY_ACCT_REQUESTS] = {
        .name = "freeradius_proxy_acct_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy acct requests"
    },
    [FAM_FREERADIUS_PROXY_ACCT_RESPONSES] = {
        .name = "freeradius_proxy_acct_responses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy acct responses"
    },
    [FAM_FREERADIUS_PROXY_ACCT_DUPLICATE_REQUESTS] = {
        .name = "freeradius_proxy_acct_duplicate_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy acct duplicate requests"
    },
    [FAM_FREERADIUS_PROXY_ACCT_MALFORMED_REQUESTS] = {
        .name = "freeradius_proxy_acct_malformed_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy acct malformed requests"
    },
    [FAM_FREERADIUS_PROXY_ACCT_INVALID_REQUESTS] = {
        .name = "freeradius_proxy_acct_invalid_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy acct invalid requests"
    },
    [FAM_FREERADIUS_PROXY_ACCT_DROPPED_REQUESTS] = {
        .name = "freeradius_proxy_acct_dropped_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy acct dropped requests"
    },
    [FAM_FREERADIUS_PROXY_ACCT_UNKNOWN_TYPES] = {
        .name = "freeradius_proxy_acct_unknown_types",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total proxy acct unknown types"
    },
    [FAM_FREERADIUS_QUEUE_LEN_INTERNAL] = {
        .name = "freeradius_queue_len_internal",
        .type = METRIC_TYPE_GAUGE,
        .help = "Interal queue length",
    },
    [FAM_FREERADIUS_QUEUE_LEN_PROXY] = {
        .name = "freeradius_queue_len_proxy",
        .type = METRIC_TYPE_GAUGE,
        .help = "Proxy queue length",
    },
    [FAM_FREERADIUS_QUEUE_LEN_AUTH] = {
        .name = "freeradius_queue_len_auth",
        .type = METRIC_TYPE_GAUGE,
        .help = "Auth queue length",
    },
    [FAM_FREERADIUS_QUEUE_LEN_ACCT] = {
        .name = "freeradius_queue_len_acct",
        .type = METRIC_TYPE_GAUGE,
        .help = "Acct queue length",
    },
    [FAM_FREERADIUS_QUEUE_LEN_DETAIL] = {
        .name = "freeradius_queue_len_detail",
        .type = METRIC_TYPE_GAUGE,
        .help = "Detail queue length",
    },
    [FAM_FREERADIUS_LAST_PACKET_RECV_SECONDS] = {
        .name = "freeradius_last_packet_recv_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Epoch timestamp when the last packet was received",
    },
    [FAM_FREERADIUS_LAST_PACKET_SENT_SECONDS] = {
        .name = "freeradius_last_packet_sent_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Epoch timestamp when the last packet was sent",
    },
    [FAM_FREERADIUS_START_TIME_SECONDS] = {
        .name = "freeradius_start_time_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Epoch timestamp when the server was started",
    },
    [FAM_FREERADIUS_HUP_TIME_SECONDS] = {
        .name = "freeradius_hup_time_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Epoch timestamp when the server hang up (If start == hup, it hasn't been hup'd yet)",
    },
    [FAM_FREERADIUS_STATE] = {
        .name = "freeradius_state",
        .type = METRIC_TYPE_GAUGE,
        .help = "State of the server. Alive = 0; Zombie = 1; Dead = 2; Idle = 3",
    },
    [FAM_FREERADIUS_TIME_OF_DEATH_SECONDS] = {
        .name = "freeradius_time_of_death_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Epoch timestamp when a home server is marked as 'dead'",
    },
    [FAM_FREERADIUS_TIME_OF_LIFE_SECONDS] = {
        .name = "freeradius_time_of_life_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Epoch timestamp when a home server is marked as 'alive'",
    },
    [FAM_FREERADIUS_EMA_WINDOW] = {
        .name = "freeradius_ema_window",
        .type = METRIC_TYPE_GAUGE,
        .help = "Exponential moving average of home server response time",
    },
    [FAM_FREERADIUS_EMA_WINDOW1_SECONDS] = {
        .name = "freeradius_ema_window1_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Window-1 is the average calculated over 'window' packets",
    },
    [FAM_FREERADIUS_EMA_WINDOW10_SECONDS] = {
        .name = "freeradius_ema_window10_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Window-10 is the average calculated over '10 * window' packets",
    },
    [FAM_FREERADIUS_OUTSTANDING_REQUESTS] = {
        .name = "freeradius_outstanding_requests",
        .type = METRIC_TYPE_GAUGE,
        .help = "Outstanding requests",
    },
    [FAM_FREERADIUS_QUEUE_PPS_IN] = {
        .name = "freeradius_queue_pps_in",
        .type = METRIC_TYPE_GAUGE,
        .help = "Queue PPS in",
    },
    [FAM_FREERADIUS_QUEUE_PPS_OUT] = {
        .name = "freeradius_queue_pps_out",
        .type = METRIC_TYPE_GAUGE,
        .help = "Queue PPS out",
    },
    [FAM_FREERADIUS_QUEUE_USE_PERCENTAGE] = {
        .name = "freeradius_queue_use_percentage",
        .type = METRIC_TYPE_GAUGE,
        .help = "Queue usage percentage",
    },
};

static int freeradius_read(user_data_t *user_data)
{
    freeradius_ctx_t *ctx = user_data->data;

    if (ctx == NULL)
        return -1;

    if (ctx->fd < 0) {
        ctx->fd = socket_connect_udp(ctx->host, ctx->port, 0);
        if (ctx->fd < 0)
            return -1;

        struct timeval tv = CDTIME_T_TO_TIMEVAL(ctx->timeout);
        int status = setsockopt (ctx->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof (tv));
        if (status != 0) {
            PLUGIN_ERROR("setsockopt setting SO_RCVTIMEO failed: %s", STRERRNO);
            return -1;
        }
    }

    radius_status_server_t pkt = {0};

    pkt.hdr.code = RADIUS_CODE_STATUS_SERVER;
    pkt.hdr.length = htons(sizeof(pkt));
    pkt.hdr.id = RADIUS_ATTR_ID_MESSAGE_AUTHENTICATOR;
    cdrand(pkt.hdr.vector, sizeof(pkt.hdr.vector));

    pkt.vsa.type = RADIUS_ATTR_ID_VENDOR_SPECIFIC;
    pkt.vsa.length = sizeof(pkt.vsa);
    pkt.vsa.vendor_id = htonl(FREERADIUS_ATTR_VENDOR_ID);
    pkt.vsa.vsa.type = FREERADIUS_STATISTICS_TYPE;
    pkt.vsa.vsa.length = sizeof(pkt.vsa.vsa);
    pkt.vsa.vsa.val = htonl(FREERADIUS_STATISTICS_ALL);

    pkt.auth.type = RADIUS_ATTR_ID_MESSAGE_AUTHENTICATOR;
    pkt.auth.length = sizeof(pkt.auth);

    size_t secret_len = strlen(ctx->secret);
    uint8_t digest[MD5_DIGEST_SIZE];
    hmac_md5((uint8_t *)&pkt, sizeof(pkt), (uint8_t *)ctx->secret, secret_len, digest);
    memcpy(pkt.auth.data, digest, MD5_DIGEST_SIZE);

    ssize_t ssize = send(ctx->fd, &pkt, sizeof(pkt), MSG_DONTWAIT);
    if (ssize != sizeof(pkt)) {
        if (ssize <= 0)
            PLUGIN_ERROR("Error sending packet: %s.", STRERRNO);
        else
            PLUGIN_ERROR("Failed to send packet.");
        goto error;
    }

    uint8_t resp[1452];
    ssize_t rsize = recv(ctx->fd, resp, sizeof(resp), 0);
    if (rsize <= 0) {
        PLUGIN_ERROR("Error receiving packet: %s.", STRERRNO);
        goto error;
    }

    cdtime_t submit = cdtime();

    radius_hdr_t *hdr = (radius_hdr_t *)resp;
    hdr->length = ntohs(hdr->length);
    if ((ssize_t)hdr->length != rsize) {
        PLUGIN_ERROR("Error in received packet: invalid packet size.");
        goto error;
    }

    if (hdr->code != RADIUS_CODE_ACCESS_ACCEPT) {
        PLUGIN_ERROR("Error in received packet: unexpected code in header.");
        goto error;
    }

    if (hdr->id != RADIUS_ATTR_ID_MESSAGE_AUTHENTICATOR) {
        PLUGIN_ERROR("Error in received packet: unexpected id in header.");
        goto error;
    }

    metric_family_append(&ctx->fams[FAM_FREERADIUS_UP], VALUE_GAUGE(1), &ctx->labels, NULL);

    size_t offset = sizeof(radius_hdr_t);
    while(true) {
        if (offset >= hdr->length)
            break;

        radius_avp_raw_t *avp = (radius_avp_raw_t *)(resp + offset);
        offset += avp->length;

        if (offset > hdr->length)
            break;

        if (avp->type != RADIUS_ATTR_ID_VENDOR_SPECIFIC)
            continue;

        if (avp->length != 12)
            continue;

        radius_avp_vsa_int_t *avp_vsa = (radius_avp_vsa_int_t *)avp;

        if (ntohl(avp_vsa->vendor_id) != FREERADIUS_ATTR_VENDOR_ID)
            continue;
        
        if (avp_vsa->vsa.length != 6)
            continue;

        uint32_t value = ntohl(avp_vsa->vsa.val);

        if (avp_vsa->vsa.type > freeradius_stats_size)
            continue;

        int fam = freeradius_stats[avp_vsa->vsa.type].fam;
        if (fam <= 0)
            continue;

        double scale = freeradius_stats[avp_vsa->vsa.type].scale;

        switch(ctx->fams[fam].type) {
        case METRIC_TYPE_GAUGE:
            metric_family_append(&ctx->fams[fam], VALUE_GAUGE((double)value * scale),
                                                  &ctx->labels, NULL);
            break;
        case METRIC_TYPE_COUNTER:
            metric_family_append(&ctx->fams[fam], VALUE_COUNTER(value), &ctx->labels, NULL);
            break;
        default:
            break;
        }

    }

    plugin_dispatch_metric_family_array_filtered(ctx->fams, FAM_FREERADIUS_MAX, ctx->filter, submit);

    return 0;

error:
    metric_family_append(&ctx->fams[FAM_FREERADIUS_UP], VALUE_GAUGE(0), &ctx->labels, NULL);
    plugin_dispatch_metric_family(&ctx->fams[FAM_FREERADIUS_UP], 0);

    return 0;
}

static void freeradius_free(void *arg)
{
    freeradius_ctx_t *ctx = arg;

    if (ctx == NULL)
        return;

    if (ctx->fd > 0)
        close(ctx->fd);

    free(ctx->name);
    free(ctx->host);
    free(ctx->secret);

    label_set_reset(&ctx->labels);
    plugin_filter_free(ctx->filter);

    free(ctx);
}

static int freeradius_config_instance (config_item_t *ci)
{
    freeradius_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    ctx->fd = -1;
    memcpy(ctx->fams, fams, sizeof(ctx->fams[0])*FAM_FREERADIUS_MAX);

    int status = cf_util_get_string(ci, &ctx->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(ctx);
        return status;
    }
    assert(ctx->name != NULL);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "host") == 0) {
            status = cf_util_get_string(child, &ctx->host);
        } else if (strcasecmp(child->key, "port") == 0) {
            status = cf_util_get_port_number(child, &ctx->port);
        } else if (strcasecmp(child->key, "secret") == 0) {
            status = cf_util_get_string(child, &ctx->secret);
        } else if (strcasecmp(child->key, "timeout") == 0) {
            status = cf_util_get_cdtime(child, &ctx->timeout);
        } else if (strcasecmp(child->key, "label") == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp(child->key, "interval") == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = plugin_filter_configure(child, &ctx->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        freeradius_free(ctx);
        return -1;
    }

    if (ctx->secret == NULL) {
        PLUGIN_ERROR("Missing secret for freeradius.");
        freeradius_free(ctx);
        return -1;
    }

    if (ctx->host == NULL) {
        ctx->host = strdup("localhost");
        if (ctx->host == NULL) {
            PLUGIN_ERROR("strdup failed.");
            freeradius_free(ctx);
            return -1;
        }
    }

    if (ctx->port == 0)
        ctx->port = 18121;

    if (ctx->timeout == 0) {
        if (interval == 0)
            interval = plugin_get_interval();
        ctx->timeout = interval / 2.0;
    } else {
        if (interval == 0)
            interval = plugin_get_interval();
        if (ctx->timeout > interval) {
            PLUGIN_ERROR("Timeout: %.3f it's bigger than plugin interval: %.3f.",
                         CDTIME_T_TO_DOUBLE(ctx->timeout), CDTIME_T_TO_DOUBLE(interval));
            freeradius_free(ctx);
            return -1;
        }
    }

    label_set_add(&ctx->labels, true, "instance", ctx->name);

    return plugin_register_complex_read("freeradius", ctx->name, freeradius_read, interval,
                                        &(user_data_t){.data = ctx, .free_func = freeradius_free});
}

static int freeradius_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = freeradius_config_instance(child);
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
    plugin_register_config("freeradius", freeradius_config);
}
