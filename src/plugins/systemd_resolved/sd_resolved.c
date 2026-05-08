// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/socket.h"
#include "libxson/json_parse.h"

enum {
    FAM_SYSTEMD_RESOLVED_UP,
    FAM_SYSTEMD_RESOLVED_CURRENT_TRANSACTIONS,
    FAM_SYSTEMD_RESOLVED_TRANSACTIONS,
    FAM_SYSTEMD_RESOLVED_TIMEOUTS,
    FAM_SYSTEMD_RESOLVED_TIMEOUTS_SERVED_STALE,
    FAM_SYSTEMD_RESOLVED_FAILURE_RESPONSES,
    FAM_SYSTEMD_RESOLVED_FAILURE_RESPONSES_SERVED_STALE,
    FAM_SYSTEMD_RESOLVED_CACHE_SIZE,
    FAM_SYSTEMD_RESOLVED_CACHE_HIT,
    FAM_SYSTEMD_RESOLVED_CACHE_MISS,
    FAM_SYSTEMD_RESOLVED_DNSSEC_VERDICT,
    FAM_SYSTEMD_RESOLVED_MAX
};

static metric_family_t fams[FAM_SYSTEMD_RESOLVED_MAX] = {
    [FAM_SYSTEMD_RESOLVED_UP] = {
        .name = "systemd_resolved_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the systemd_resolved server be reached.",
    },
    [FAM_SYSTEMD_RESOLVED_CURRENT_TRANSACTIONS] = {
        .name = "systemd_resolved_current_transactions",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current transactions.",
    },
    [FAM_SYSTEMD_RESOLVED_TRANSACTIONS] = {
        .name = "systemd_resolved_transactions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of transactions.",
    },
    [FAM_SYSTEMD_RESOLVED_TIMEOUTS] = {
        .name = "systemd_resolved_timeouts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total Timeouts",
    },
    [FAM_SYSTEMD_RESOLVED_TIMEOUTS_SERVED_STALE] = {
        .name = "systemd_resolved_timeouts_served_stale",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total Timeouts (Stale Data Served)",
    },
    [FAM_SYSTEMD_RESOLVED_FAILURE_RESPONSES] = {
        .name = "systemd_resolved_failure_responses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total Failure Responses",
    },
    [FAM_SYSTEMD_RESOLVED_FAILURE_RESPONSES_SERVED_STALE] = {
        .name = "systemd_resolved_failure_responses_served_stale",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total Failure Responses (Stale Data Served)",
    },
    [FAM_SYSTEMD_RESOLVED_CACHE_SIZE] = {
        .name = "systemd_resolved_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current Cache Size."
    },
    [FAM_SYSTEMD_RESOLVED_CACHE_HIT] = {
        .name = "systemd_resolved_cache_hit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of cache hits.",
    },
    [FAM_SYSTEMD_RESOLVED_CACHE_MISS] = {
        .name = "systemd_resolved_cache_miss",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of cache misses.",
    },
    [FAM_SYSTEMD_RESOLVED_DNSSEC_VERDICT] = {
        .name = "systemd_resolved_dnssec_verdict",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total DNSSEC verdicts by type",
    },
};

#include "plugins/systemd_resolved/sd_resolved.h"

static char *sd_resolved_path = "/run/systemd/resolve/io.systemd.Resolve.Monitor";

typedef enum {
    SD_RESOLVED_JSON_NONE,
    SD_RESOLVED_JSON_ERROR,
    SD_RESOLVED_JSON_PARAMETERS
} sd_resolved_json_key_t;

typedef struct {
    sd_resolved_json_key_t stack[JSON_MAX_DEPTH];
    size_t depth;
    char error[256];
    char kbuf[256];
    strbuf_t key;
    size_t key1_len;
} sd_resolved_json_ctx_t;

static bool sd_resolved_json_string(void *ctx, const char *str, size_t str_len)
{
    sd_resolved_json_ctx_t *sctx = ctx;

    if ((sctx->depth == 1) && (sctx->stack[0] == SD_RESOLVED_JSON_ERROR))
        sstrnncpy(sctx->error, sizeof(sctx->error), (const char *)str, str_len);

    return true;
}

static bool sd_resolved_json_number(void *ctx, const char *str, size_t str_len)
{
    sd_resolved_json_ctx_t *sctx = ctx;

    if ((sctx->depth == 3) && (sctx->stack[0] == SD_RESOLVED_JSON_PARAMETERS)) {
        char number[256];
        sstrnncpy(number, sizeof(number), (const char *)str, str_len);

        const struct sd_resolved_metric *rm = sd_resolved_get_key(sctx->key.ptr,
                                                                  strbuf_len(&sctx->key));
        if (rm == NULL)
            return true;

        metric_family_t *fam = &fams[rm->fam];

        value_t value = {0};
        if (fam->type == METRIC_TYPE_COUNTER) {
            value = VALUE_COUNTER(atol(number));
        } else if (fam->type == METRIC_TYPE_GAUGE) {
            value = VALUE_GAUGE(atof(number));
        } else {
            return true;
        }

        if ((rm->lkey != NULL) && (rm->lvalue != NULL))
            metric_family_append(fam, value, NULL,
                                 &(label_pair_t){.name=rm->lkey, .value=rm->lvalue}, NULL);
        else
            metric_family_append(fam, value, NULL, NULL);
    }

    return true;
}

static bool sd_resolved_json_start_map(void *ctx)
{
    sd_resolved_json_ctx_t *sctx = ctx;
    sctx->depth++;
    sctx->stack[sctx->depth] = SD_RESOLVED_JSON_NONE;
    return true;
}

static bool sd_resolved_json_map_key(void * ctx, const char *ukey, size_t key_len)
{
    const char *key = (const char *)ukey;
    sd_resolved_json_ctx_t *sctx = ctx;

    switch (sctx->depth) {
    case 1:
        if (strncmp("error", key, key_len) == 0) {
            sctx->stack[0] = SD_RESOLVED_JSON_ERROR;
        } else if (strncmp("parameters", key, key_len) == 0) {
            sctx->stack[0] = SD_RESOLVED_JSON_PARAMETERS;
        } else {
            sctx->stack[0] = SD_RESOLVED_JSON_NONE;
            return false;
        }
        break;
    case 2:
        if (sctx->stack[0] == SD_RESOLVED_JSON_PARAMETERS) {
            strbuf_reset(&sctx->key);
            strbuf_putstrn(&sctx->key, ukey, key_len);
            sctx->key1_len = strbuf_offset(&sctx->key);
        } else {
            sctx->stack[0] = SD_RESOLVED_JSON_NONE;
            return false;
        }
        break;
    case 3:
        if (sctx->stack[0] == SD_RESOLVED_JSON_PARAMETERS) {
            strbuf_resetto(&sctx->key, sctx->key1_len);
            strbuf_putchar(&sctx->key, '.');
            strbuf_putstrn(&sctx->key, ukey, key_len);
        } else {
            sctx->stack[0] = SD_RESOLVED_JSON_NONE;
            return false;
        }
        break;
    default:
        return false;
        break;
    }

    return true;
}

static bool sd_resolved_json_end_map(void *ctx)
{
    sd_resolved_json_ctx_t *sctx = ctx;

    sctx->depth--;
    if (sctx->depth > 0) {
        sctx->stack[sctx->depth-1] = SD_RESOLVED_JSON_NONE;
    } else {
        sctx->depth = 0;
    }

    return true;
}

static xson_callbacks_t sd_resolved_json_callbacks = {
    .xson_null        = NULL,
    .xson_boolean     = NULL,
    .xson_integer     = NULL,
    .xson_double      = NULL,
    .xson_number      = sd_resolved_json_number,
    .xson_string      = sd_resolved_json_string,
    .xson_start_map   = sd_resolved_json_start_map,
    .xson_map_key     = sd_resolved_json_map_key,
    .xson_end_map     = sd_resolved_json_end_map,
    .xson_start_array = NULL,
    .xson_end_array   = NULL,
};

static int sd_resolved_read_varlink(void)
{
    const char *method = "{\"method\":\"io.systemd.Resolve.Monitor.DumpStatistics\","
                         "\"parameters\":{\"allowInteractiveAuthentication\":false}}";

    int fd = socket_connect_unix_stream(sd_resolved_path, plugin_get_interval());
    if (fd < 0)
        return -1;

    size_t method_size = strlen(method);

    ssize_t wstatus = swrite(fd, method, method_size + 1);
    if (wstatus != 0) {
        close(fd);
        return -1;
    }

    sd_resolved_json_ctx_t ctx = {0};
    ctx.key = STRBUF_CREATE_STATIC(ctx.kbuf);

    json_parser_t handle;
    json_parser_init(&handle, 0, &sd_resolved_json_callbacks, &ctx);

    char buffer[4096];
    ssize_t len = 0;
    bool done = false;
    while ((len = read(fd, buffer, sizeof(buffer))) > 0) {
        if (buffer[len-1] == '\0') {
            if (len > 0)
                len--;
            done = true;
        }
        json_status_t status = json_parser_parse(&handle, (unsigned char *)buffer, len);
        if (status != JSON_STATUS_OK)
            break;

        if (done)
            break;
    }

    close(fd);

    json_status_t status = json_parser_complete(&handle);
    if (status != JSON_STATUS_OK) {
        unsigned char *errmsg = json_parser_get_error(&handle, 0, NULL, 0);
        PLUGIN_ERROR("json_parse_complete failed: %s", (const char *)errmsg);
        json_parser_free_error(errmsg);
        json_parser_free(&handle);
        return -1;
    }

    json_parser_free(&handle);

    if (ctx.error[0] != '\0') {
        PLUGIN_ERROR("Error calling io.systemd.Resolve.Monitor.DumpStatistics: %s.", ctx.error);
        return -1;
    }

    return 0;
}

static int sd_resolved_read(void)
{
    int status = sd_resolved_read_varlink();
    if (status != 0) {
        metric_family_append(&fams[FAM_SYSTEMD_RESOLVED_UP], VALUE_GAUGE(0), NULL, NULL);
        plugin_dispatch_metric_family_array(fams, FAM_SYSTEMD_RESOLVED_MAX, 0);
        return 0;
    }

    metric_family_append(&fams[FAM_SYSTEMD_RESOLVED_UP], VALUE_GAUGE(1), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_SYSTEMD_RESOLVED_MAX, 0);

    return 0;
}

void module_register(void)
{
    plugin_register_read("systemd_resolved", sd_resolved_read);
}
