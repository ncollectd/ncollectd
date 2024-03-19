// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/itoa.h"
#include "libxson/json_parse.h"

#include <time.h>

#include <curl/curl.h>

enum {
    FAM_ATS_CLIENT_REQUESTS_INVALID,
    FAM_ATS_CLIENT_REQUESTS_MISSING_HOST_HDR,
    FAM_ATS_CONNECT_FAILURES,
    FAM_ATS_CONNECTIONS,
    FAM_ATS_ERROR_CLIENT_ABORTS,
    FAM_ATS_ERROR_TRANSACTION_TIME_SECONDS,
    FAM_ATS_HIT_TRANSACTION_TIME_SECONDS,
    FAM_ATS_MISS_TRANSACTION_TIME_SECONDS,
    FAM_ATS_RAM_CACHE_TOTAL_BYTES,
    FAM_ATS_RAM_CACHE_MISSES,
    FAM_ATS_RAM_CACHE_USED_BYTES,
    FAM_ATS_RAM_CACHE_HITS,
    FAM_ATS_INCOMING_REQUESTS,
    FAM_ATS_OUTGOING_REQUESTS,
    FAM_ATS_REQUESTS,
    FAM_ATS_RESPONSE_CLASSES,
    FAM_ATS_RESPONSES,
    FAM_ATS_INCOMING_RESPONSES,
    FAM_ATS_RESTARTS,
    FAM_ATS_TRANSACTION_ERRORS,
    FAM_ATS_TRANSACTION_HITS,
    FAM_ATS_TRANSACTION_MISSES,
    FAM_ATS_TRANSACTION_OTHERS,
    FAM_ATS_TRANSACTIONS_TIME,
    FAM_ATS_TRANSACTIONS,
    FAM_ATS_REQUEST_SIZE_BYTES,
    FAM_ATS_RESPONSE_SIZE_BYTES,
    FAM_ATS_REQUEST_HEADER_SIZE_BYTES,
    FAM_ATS_REPONSE_HEADER_SIZE_BYTES,
    FAM_ATS_REQUEST_DOCUMENT_SIZE_BYTES,
    FAM_ATS_REPONSE_DOCUMENT_SIZE_BYTES,
    FAM_ATS_CACHE_VOLUME_USED_BYTES,
    FAM_ATS_CACHE_VOLUME_TOTAL_BYTES,
    FAM_ATS_CACHE_VOLUME_RAM_CACHE_USED_BYTES,
    FAM_ATS_CACHE_VOLUME_RAM_CACHE_TOTAL_BYTES,
    FAM_ATS_CACHE_VOLUME_RAM_CACHE_HITS,
    FAM_ATS_CACHE_VOLUME_RAM_CACHE_MISSES,
    FAM_ATS_CACHE_VOLUME_FULL_RATIO,
    FAM_ATS_CACHE_VOLUME_TOTAL_DIRENTRIES,
    FAM_ATS_CACHE_VOLUME_USED_DIRENTRIES,
    FAM_ATS_CACHE_VOLUME_OPERATIONS_ACTIVE,
    FAM_ATS_CACHE_VOLUME_OPERATIONS_SUCCESS,
    FAM_ATS_CACHE_VOLUME_OPERATIONS_FAILURE,
    FAM_ATS_MAX,
};

static metric_family_t fams_ats[FAM_ATS_MAX] = {
    [FAM_ATS_CLIENT_REQUESTS_INVALID] = {
        .name = "ats_client_requests_invalid",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of requests received by the Traffic Server instance, "
                "which did not include a valid HTTP method.",
    },
    [FAM_ATS_CLIENT_REQUESTS_MISSING_HOST_HDR] = {
        .name = "ats_client_requests_missing_host_hdr",
        .type = METRIC_TYPE_COUNTER,
        .help = "Client requests missing host header.",
    },
    [FAM_ATS_CONNECT_FAILURES] = {
        .name = "ats_connect_failures",
        .type = METRIC_TYPE_COUNTER,
        .help = "Connect failures.",
    },
    [FAM_ATS_CONNECTIONS] = {
        .name = "ats_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Connection count.",
    },
    [FAM_ATS_ERROR_CLIENT_ABORTS] = {
        .name = "ats_error_client_aborts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Client aborts.",
    },
    [FAM_ATS_ERROR_TRANSACTION_TIME_SECONDS] = {
        .name = "ats_error_transaction_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total cache error transaction time in seconds.",
    },
    [FAM_ATS_HIT_TRANSACTION_TIME_SECONDS] = {
        .name = "ats_hit_transaction_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total cache hit transaction time in seconds.",
    },
    [FAM_ATS_MISS_TRANSACTION_TIME_SECONDS] = {
        .name = "ats_miss_transaction_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total cache miss transaction time in seconds.",
    },
    [FAM_ATS_RAM_CACHE_TOTAL_BYTES] = {
        .name = "ats_ram_cache_total_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total RAM cache in bytes."
    },
    [FAM_ATS_RAM_CACHE_MISSES] = {
        .name = "ats_ram_cache_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "RAM cache miss count.",
    },
    [FAM_ATS_RAM_CACHE_USED_BYTES] = {
        .name = "ats_ram_cache_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "RAM cache used in bytes.",
    },
    [FAM_ATS_RAM_CACHE_HITS] = {
        .name = "ats_ram_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "RAM cache hits  count.",
    },
    [FAM_ATS_INCOMING_REQUESTS] = {
        .name = "ats_incoming_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of client requests serviced by Traffic Server."
    },
    [FAM_ATS_OUTGOING_REQUESTS] = {
        .name = "ats_outgoing_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of requests made by Traffic Server to origin servers."
    },
    [FAM_ATS_REQUESTS] = {
        .name = "ats_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Request count.",
    },
    [FAM_ATS_RESPONSE_CLASSES] = {
        .name = "ats_response_classes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Response count by class, i.e. 2xx, 3xx.",
    },
    [FAM_ATS_RESPONSES] = {
        .name = "ats_responses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of responses.",
    },
    [FAM_ATS_INCOMING_RESPONSES] = {
        .name = "ats_incoming_responses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incoming responses."
    },
    [FAM_ATS_RESTARTS] = {
        .name = "ats_restarts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of Traffic Server restarts.",
    },
    [FAM_ATS_TRANSACTION_ERRORS] = {
        .name = "ats_transaction_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Transaction error counts.",
    },
    [FAM_ATS_TRANSACTION_HITS] = {
        .name = "ats_transaction_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Transaction hit counts.",
    },
    [FAM_ATS_TRANSACTION_MISSES] = {
        .name = "ats_transaction_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Transaction miss counts.",
    },
    [FAM_ATS_TRANSACTION_OTHERS] = {
        .name = "ats_transaction_others",
        .type = METRIC_TYPE_COUNTER,
        .help = "Transaction other/unclassified counts",
    },
    [FAM_ATS_TRANSACTIONS_TIME] = {
        .name = "ats_transactions_time",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total transaction time in seconds",
    },
    [FAM_ATS_TRANSACTIONS] = {
        .name = "ats_transactions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total transactions",
    },
    [FAM_ATS_REQUEST_SIZE_BYTES] = {
        .name = "ats_request_size_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ATS_RESPONSE_SIZE_BYTES] = {
        .name = "ats_response_size_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ATS_REQUEST_HEADER_SIZE_BYTES] = {
        .name = "ats_request_header_size_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ATS_REPONSE_HEADER_SIZE_BYTES] = {
        .name = "ats_reponse_header_size_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ATS_REQUEST_DOCUMENT_SIZE_BYTES] = {
        .name = "ats_request_document_size_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ATS_REPONSE_DOCUMENT_SIZE_BYTES] = {
        .name = "ats_reponse_document_size_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ATS_CACHE_VOLUME_USED_BYTES] = {
        .name = "ats_cache_volume_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ATS_CACHE_VOLUME_TOTAL_BYTES] = {
        .name = "ats_cache_volume_total_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ATS_CACHE_VOLUME_RAM_CACHE_USED_BYTES] = {
        .name = "ats_cache_volume_ram_cache_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ATS_CACHE_VOLUME_RAM_CACHE_TOTAL_BYTES] = {
        .name = "ats_cache_volume_ram_cache_total_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ATS_CACHE_VOLUME_RAM_CACHE_HITS] = {
        .name = "ats_cache_volume_ram_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ATS_CACHE_VOLUME_RAM_CACHE_MISSES] = {
        .name = "ats_cache_volume_ram_cache_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ATS_CACHE_VOLUME_FULL_RATIO] = {
        .name = "ats_cache_volume_full_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ATS_CACHE_VOLUME_TOTAL_DIRENTRIES] = {
        .name = "ats_cache_volume_total_direntries",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ATS_CACHE_VOLUME_USED_DIRENTRIES] = {
        .name = "ats_cache_volume_used_direntries",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ATS_CACHE_VOLUME_OPERATIONS_ACTIVE] = {
        .name = "ats_cache_volume_operations_active",
        .type = METRIC_TYPE_GAUGE,
        .help = "Active cache operations."
    },
    [FAM_ATS_CACHE_VOLUME_OPERATIONS_SUCCESS] = {
        .name = "ats_cache_volume_operations_success",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total success cache operations.",
    },
    [FAM_ATS_CACHE_VOLUME_OPERATIONS_FAILURE] = {
        .name = "ats_cache_volume_operations_failure",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total failed cache operations.",
    },
};

#include "plugins/ats/ats.h"

typedef enum {
    ATS_JSON_NONE,
    ATS_JSON_GLOBAL,
    ATS_JSON_GLOBAL_STAT,
} ats_json_key_t;

typedef struct {
    ats_json_key_t stack[JSON_MAX_DEPTH];
    size_t depth;
    metric_family_t *fams;
    label_set_t *labels;
    int nfam;
    char *lkey1;
    char *lvalue1;
    char *lkey2;
    char *lvalue2;
    int volume;
} ats_json_ctx_t;

typedef struct {
    char *instance;
    char *url;
    int timeout;
    label_set_t labels;
    CURL *curl;
    char ats_curl_error[CURL_ERROR_SIZE];
    metric_family_t fams[FAM_ATS_MAX];
} ats_instance_t;

static bool ats_json_string(void *ctx, const char *number_val, size_t number_len)
{
    ats_json_ctx_t *sctx = ctx;

    if ((sctx->depth == 2) && (sctx->stack[1] == ATS_JSON_GLOBAL_STAT)  && (sctx->nfam > 0)) {
        metric_family_t *fam = &sctx->fams[sctx->nfam];

        char number[256];
        sstrnncpy(number, sizeof(number), (const char *)number_val, number_len);

        value_t value = {0};
        if (fam->type == METRIC_TYPE_COUNTER) {
            value = VALUE_COUNTER(atol(number));
        } else if (fam->type == METRIC_TYPE_GAUGE) {
            value = VALUE_GAUGE(atof(number));
        } else {
            return true;
        }

        label_pair_t labels[3] = {0};
        label_pair_t *label0 = NULL;
        label_pair_t *label1 = NULL;
        label_pair_t *label2 = NULL;
        char volume[ITOA_MAX] = {0};
        size_t n = 0;

        if ((sctx->lkey1 != NULL) && (sctx->lvalue1 != NULL)) {
            labels[n].name = sctx->lkey1;
            labels[n].value = sctx->lvalue1;
            n++;
        }

        if ((sctx->lkey2 != NULL) && (sctx->lvalue2 != NULL)) {
            labels[n].name = sctx->lkey2;
            labels[n].value = sctx->lvalue2;
            n++;
        }

        if (sctx->volume >= 0) {
            labels[n].name = "volume";
            itoa(sctx->volume, volume);
            labels[n].value = volume;
            n++;
        }

        if (n > 0)
            label0 = &labels[0];
        if (n > 1)
            label1 = &labels[1];
        if (n > 2)
            label2 = &labels[2];

        metric_family_append(fam, value, sctx->labels, label0, label1, label2, NULL);
    }

    return true;
}

static bool ats_json_start_map(void *ctx)
{
    ats_json_ctx_t *sctx = ctx;
    sctx->depth++;
    sctx->stack[sctx->depth] = ATS_JSON_NONE;
    return true;
}

static bool ats_json_map_key(void * ctx, const char *ukey, size_t key_len)
{
    const char *key = (const char *)ukey;
    ats_json_ctx_t *sctx = ctx;

    switch (sctx->depth) {
    case 1:
        if (strncmp("global", key, key_len) == 0) {
            sctx->stack[0] = ATS_JSON_GLOBAL;
        } else {
            sctx->stack[0] = ATS_JSON_NONE;
            sctx->nfam = -1;
        }
        break;
    case 2:
        if (sctx->stack[0] == ATS_JSON_GLOBAL) {
            char bkey[256];
            const char *pkey = (const char *)ukey;

            sctx->volume = -1;

            size_t vol_len = strlen("proxy.process.cache.volume_");
            if (key_len > vol_len) {
                if (strncmp((const char *)ukey, "proxy.process.cache.volume_", vol_len) == 0) {
                    sctx->volume = atoi((const char *)ukey + vol_len);
                    char *end = strchr((const char *)ukey + vol_len, '.');
                    if (end != NULL) {
                        vol_len--;
                        size_t end_len = key_len - (end - (const char *)ukey);
                        sstrncpy(bkey, "proxy.process.cache.volume", sizeof(bkey));
                        sstrnncpy(bkey + vol_len, sizeof(bkey) - vol_len, end, end_len);
                        pkey = bkey;
                        key_len = vol_len + end_len;
                    }
                }
            }

            const struct ats_metric *am = ats_get_key(pkey, key_len);
            if (am != NULL) {
                sctx->nfam = am->fam;
                sctx->lkey1 = am->lkey1;
                sctx->lvalue1 = am->lvalue1;
                sctx->lkey2 = am->lkey2;
                sctx->lvalue2 = am->lvalue2;
                sctx->stack[1] = ATS_JSON_GLOBAL_STAT;
            } else {
                sctx->stack[1] = ATS_JSON_NONE;
                sctx->nfam = -1;
// fprintf(stderr, "** [%d] <%.*s>\n", (int)sctx->depth, (int)key_len, pkey);
            }
        } else {
            sctx->stack[1] = ATS_JSON_NONE;
            sctx->nfam = -1;
        }
        break;
    default:
        break;
    }

    return true;
}

static bool ats_json_end_map(void *ctx)
{
    ats_json_ctx_t *sctx = ctx;

    sctx->depth--;
    if (sctx->depth > 0) {
        sctx->stack[sctx->depth-1] = ATS_JSON_NONE;
    } else {
        sctx->depth = 0;
    }

    return true;
}

static json_callbacks_t ats_json_callbacks = {
    .json_null        = NULL,
    .json_boolean     = NULL,
    .json_integer     = NULL,
    .json_double      = NULL,
    .json_number      = NULL,
    .json_string      = ats_json_string,
    .json_start_map   = ats_json_start_map,
    .json_map_key     = ats_json_map_key,
    .json_end_map     = ats_json_end_map,
    .json_start_array = NULL,
    .json_end_array   = NULL,
};

static size_t ats_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    json_parser_t *handle = user_data;
    if (handle == NULL) {
        PLUGIN_ERROR("user_data pointer is NULL.");
        return 0;
    }

    size_t len = size * nmemb;
    if (len == 0)
        return 0;

    json_status_t status = json_parser_parse(handle, buf, len);
    if (status != JSON_STATUS_OK)
        return -1;

    return len;
}

static int ats_read(user_data_t *user_data)
{
    ats_instance_t *ats = user_data->data;

    if (ats == NULL) {
        PLUGIN_ERROR("ats instance is NULL.");
        return -1;
    }

    ats_json_ctx_t ctx = {0};
    ctx.fams = ats->fams;
    ctx.labels = &ats->labels;
    ctx.nfam = -1;

    json_parser_t handle;
    json_parser_init(&handle, 0, &ats_json_callbacks, &ctx);

    if (ats->curl == NULL) {
        ats->curl = curl_easy_init();
        if (ats->curl == NULL) {
            PLUGIN_ERROR("curl_easy_init failed.");
            return -1;
        }
    }

    CURLcode rcode = 0;

    rcode = curl_easy_setopt(ats->curl, CURLOPT_NOSIGNAL, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_NOSIGNAL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ats->curl, CURLOPT_WRITEFUNCTION, ats_curl_callback);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEFUNCTION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }
    /* coverity[BAD_SIZEOF] */
    rcode = curl_easy_setopt(ats->curl, CURLOPT_WRITEDATA, (void *)&handle);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEDATA failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ats->curl, CURLOPT_USERAGENT, NCOLLECTD_USERAGENT);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERAGENT failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ats->curl, CURLOPT_ERRORBUFFER, ats->ats_curl_error);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ats->curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_FOLLOWLOCATION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(ats->curl, CURLOPT_MAXREDIRS, 50L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_MAXREDIRS failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

#ifdef HAVE_CURLOPT_TIMEOUT_MS
    rcode = curl_easy_setopt(ats->curl, CURLOPT_TIMEOUT_MS,
                                (ats->timeout >= 0) ? (long)ats->timeout
                                : (long)CDTIME_T_TO_MS(plugin_get_interval()));
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

#endif

    rcode = curl_easy_setopt(ats->curl, CURLOPT_URL, ats->url);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_NOSIGNAL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }


    if (curl_easy_perform(ats->curl) != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_perform failed: %s", ats->ats_curl_error);
        return -1;
    }

    json_status_t status = json_parser_complete(&handle);
    if (status != JSON_STATUS_OK) {
        unsigned char *errmsg = json_parser_get_error(&handle, 0, NULL, 0);
        PLUGIN_ERROR("json_parse_complete failed: %s", (const char *)errmsg);
        json_parser_free_error(errmsg);
        json_parser_free (&handle);
        return -1;
    }
    json_parser_free(&handle);

    plugin_dispatch_metric_family_array(ats->fams, FAM_ATS_MAX, 0);

    return 0;
}

static void ats_instance_free(void *arg)
{
    ats_instance_t *ats = arg;

    if (ats == NULL)
        return;

    if (ats->curl != NULL) {
        curl_easy_cleanup(ats->curl);
        ats->curl = NULL;
    }

    free(ats->instance);
    free(ats->url);
    label_set_reset(&ats->labels);

    free(ats);
}

static int ats_config_instance(config_item_t *ci)
{
    ats_instance_t *ats = calloc(1, sizeof(*ats));
    if (ats == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    ats->timeout = -1;

    memcpy(ats->fams, fams_ats, FAM_ATS_MAX * sizeof(ats->fams[0]));

    int status = cf_util_get_string(ci, &ats->instance);
    if (status != 0) {
        free(ats);
        return status;
    }
    assert(ats->instance != NULL);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &ats->url);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_int(child, &ats->timeout);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ats->labels);
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
        ats_instance_free(ats);
        return -1;
    }

    if (ats->url == NULL) {
        PLUGIN_ERROR("Missing url in %s:%d", cf_get_file(ci), cf_get_lineno(ci));
        ats_instance_free(ats);
        return -1;
    }

    return plugin_register_complex_read("ats", ats->instance, ats_read, interval,
                    &(user_data_t){ .data = ats, .free_func = ats_instance_free});
}

static int ats_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = ats_config_instance(child);
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
    plugin_register_config("ats", ats_config);
}
