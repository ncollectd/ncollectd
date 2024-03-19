// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2010-2015  Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian Forster <ff at octo.it>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/itoa.h"
#include "libutils/dtoa.h"

#include <hiredis/hiredis.h>

#define REDIS_MAX_ARGS 256

typedef struct {
    char *instance;
    char *host;
    int port;
    char *socket;
    char *passwd;
    cdtime_t timeout;
    int database;
    cdtime_t retention;
    bool store_rates;
    redisContext *conn;
    strbuf_t buf_key;
    strbuf_t buf_metric;
} write_redis_t;

static int redis_cmd_argv(write_redis_t *node, int argc, const char **argv)
{
    redisReply *rr = redisCommandArgv(node->conn, argc, argv, NULL);
    if (rr == NULL) {
        if (node->socket)
            PLUGIN_ERROR("command %s fail on '%s'.", argv[0], node->socket);
        else
            PLUGIN_ERROR("command %s fail on '%s:%d'.", argv[0], node->host, node->port);

        redisFree(node->conn);
        node->conn = NULL;
        return -1;
    }

    if ((rr->type == REDIS_REPLY_STATUS) || (rr->type == REDIS_REPLY_INTEGER)) {
        freeReplyObject(rr);
        return 0;
    }

    if (node->socket)
        PLUGIN_ERROR("command %s unexpected reply %d on '%s'.", argv[0], rr->type, node->socket);
    else
        PLUGIN_ERROR("command %s unexpected reply %d on '%s:%d'.",
                     argv[0], rr->type, node->host, node->port);

    freeReplyObject(rr);
    redisFree(node->conn);
    node->conn = NULL;
    return -1;
}

static int redis_connect(write_redis_t *node)
{
    if (node->conn != NULL)
        return 0;

    struct timeval tv = CDTIME_T_TO_TIMEVAL(node->timeout);

    if (node->socket != NULL)
        node->conn = redisConnectUnixWithTimeout(node->socket, tv);
    else
        node->conn = redisConnectWithTimeout(node->host, node->port, tv);

    if (node->conn == NULL) {
        if (node->socket)
            PLUGIN_ERROR("unable to connect to '%s': Unknown reason.", node->socket);
        else
            PLUGIN_ERROR("unable to connect to '%s:%d': Unknown reason.", node->host, node->port);
        return -1;
    } else if (node->conn->err) {
        if (node->socket)
            PLUGIN_ERROR("unable to connect to '%s': %s.", node->socket, node->conn->errstr);
        else
            PLUGIN_ERROR("unable to connect to '%s:%d': %s.", node->host, node->port,
                                                              node->conn->errstr);
        redisFree(node->conn);
        node->conn = NULL;
        return -1;
    }

    if (node->passwd != NULL) {
        const char *argv[] = {"AUTH", node->passwd};
        int status = redis_cmd_argv(node, 2, argv);
        if (status != 0)
            return -1;
    }

    char database[ITOA_MAX];
    itoa(node->database, database);
    const char *argv[] = {"SELECT", database};
    int status = redis_cmd_argv(node, 2, argv);
    if (status != 0)
        return -1;

    return 0;
}

static int redis_cmd(write_redis_t *node, int argc, const char **argv)
{
    if (node->conn == NULL) {
        int status = redis_connect(node);
        if (status != 0)
            return -1;
    }

    return redis_cmd_argv(node, argc, argv);
}

static int format_metric(write_redis_t *node, char *metric, char *metric_suffix,
                         const label_set_t *labels1, const label_set_t *labels2,
                         double value, cdtime_t time)
{
    const char *argv[REDIS_MAX_ARGS];

    strbuf_reset(&node->buf_metric);
    strbuf_t *buf_metric = &node->buf_metric;
    strbuf_reset(&node->buf_key);
    strbuf_t *buf_key = &node->buf_key;

    argv[0] = "TS.ADD";

    int status = strbuf_putstr(buf_metric, metric);
    if (metric_suffix != NULL)
        status |= strbuf_putstr(buf_metric, metric_suffix);

    status |= strbuf_putstrn(buf_key, buf_metric->ptr, strbuf_len(buf_metric));

    size_t size1 = labels1 == NULL ? 0 : labels1->num;
    size_t n1 = 0;
    size_t size2 = labels2 == NULL ? 0 : labels2->num;
    size_t n2 = 0;

    while ((n1 < size1) || (n2 < size2)) {
        label_pair_t *pair = NULL;
        if ((n1 < size1) && (n2 < size2)) {
            if (strcmp(labels1->ptr[n1].name, labels2->ptr[n2].name) <= 0) {
                pair = &labels1->ptr[n1++];
            } else {
                pair = &labels2->ptr[n2++];
            }
        } else if (n1 < size1) {
            pair = &labels1->ptr[n1++];
        } else if (n2 < size2) {
            pair = &labels2->ptr[n2++];
        }

        if (pair != NULL) {
            status |= strbuf_putchar(buf_key, ':');
            status |= strbuf_putstr(buf_key, pair->name);
            status |= strbuf_putchar(buf_key, '#');
            status |= strbuf_putstr(buf_key, pair->value);
        }
    }

    if (status != 0) {
        PLUGIN_ERROR("Failed to format metric name.");
        return -1;
    }

    argv[1] = buf_key->ptr;

    char str_timestamp[ITOA_MAX];
    itoa(CDTIME_T_TO_MS(time), str_timestamp);
    argv[2] = str_timestamp;

    char str_value[DTOA_MAX];
    dtoa(value, str_value, sizeof(str_value));
    argv[3] = str_value;

    size_t argc = 4;

    char str_retention[ITOA_MAX];
    if (node->retention > 0) {
        argv[argc++] = "RETENTION";
        itoa(CDTIME_T_TO_MS(node->retention), str_retention);
        argv[argc++] = str_retention;
    }

    argv[argc++] = "LABELS";

    argv[argc++] = "__name__";
    argv[argc++] = buf_metric->ptr;

    if (labels1 != NULL) {
        for (size_t i = 0; i < labels1->num; i++) {
            if ((argc + 2) >= REDIS_MAX_ARGS)
                break;
            argv[argc++] = labels1->ptr[i].name;
            argv[argc++] = labels1->ptr[i].value;
        }
    }

    if (labels2 != NULL) {
        for (size_t i = 0; i < labels2->num; i++) {
            if ((argc + 2) >= REDIS_MAX_ARGS)
                break;
            argv[argc++] = labels2->ptr[i].name;
            argv[argc++] = labels2->ptr[i].value;
        }
    }

    return redis_cmd(node, argc, argv);
}

static int write_redis_write(metric_family_t const *fam, user_data_t *ud)
{
    write_redis_t *node = ud->data;

    if (fam == NULL)
        return EINVAL;

    if (fam->metric.num == 0)
        return 0;

    int status = 0;

    for (size_t i = 0; i < fam->metric.num; i++) {
        metric_t *m = &fam->metric.ptr[i];

        switch(fam->type) {
        case METRIC_TYPE_UNKNOWN: {
            double value = m->value.unknown.type == UNKNOWN_FLOAT64 ? m->value.unknown.float64
                                                                    : m->value.unknown.int64;
            status |= format_metric(node, fam->name, NULL, &m->label, NULL, value, m->time);
        }   break;
        case METRIC_TYPE_GAUGE: {
            double value = m->value.gauge.type == GAUGE_FLOAT64 ? m->value.gauge.float64
                                                                : m->value.gauge.int64;
            status |= format_metric(node, fam->name, NULL, &m->label, NULL, value, m->time);
        }   break;
        case METRIC_TYPE_COUNTER: {
            double value = m->value.counter.type == COUNTER_UINT64 ? m->value.counter.uint64
                                                                   : m->value.counter.float64;
            status |= format_metric(node, fam->name, "_total", &m->label, NULL, value, m->time);
        }   break;
        case METRIC_TYPE_STATE_SET:
            for (size_t j = 0; j < m->value.state_set.num; j++) {
                label_pair_t label_pair = {.name = fam->name,
                                           .value = m->value.state_set.ptr[j].name};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                double value = m->value.state_set.ptr[j].enabled ? 1 : 0;

                status |= format_metric(node, fam->name, NULL, &m->label, &label_set,
                                              value, m->time);
            }
            break;
        case METRIC_TYPE_INFO:
            status |= format_metric(node, fam->name, "_info", &m->label, &m->value.info,
                                          1, m->time);
            break;
        case METRIC_TYPE_SUMMARY: {
            summary_t *s = m->value.summary;

            for (int j = s->num - 1; j >= 0; j--) {
                char quantile[DTOA_MAX];

                dtoa(s->quantiles[j].quantile, quantile, sizeof(quantile));

                label_pair_t label_pair = {.name = "quantile", .value = quantile};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                status |= format_metric(node, fam->name, NULL, &m->label, &label_set,
                                              s->quantiles[j].value, m->time);
            }

            status |= format_metric(node, fam->name, "_count", &m->label, NULL, s->count, m->time);
            status |= format_metric(node, fam->name, "_sum", &m->label, NULL, s->sum, m->time);
        }   break;
        case METRIC_TYPE_HISTOGRAM:
        case METRIC_TYPE_GAUGE_HISTOGRAM: {
            histogram_t *h = m->value.histogram;

            for (int j = h->num - 1; j >= 0; j--) {
                char le[DTOA_MAX];
                dtoa(h->buckets[j].maximum, le, sizeof(le));

                label_pair_t label_pair = {.name = "le", .value = le};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                status |= format_metric(node, fam->name, "_bucket", &m->label, &label_set,
                                              h->buckets[j].counter, m->time);
            }

            status |= format_metric(node, fam->name,
                                          fam->type == METRIC_TYPE_HISTOGRAM ? "_count" : "_gcount",
                                          &m->label, NULL, histogram_counter(h), m->time);
            status |= format_metric(node, fam->name,
                                          fam->type == METRIC_TYPE_HISTOGRAM ? "_sum" : "_gsum",
                                          &m->label, NULL, histogram_sum(h), m->time);
        }   break;
        }
        if (status != 0)
            return status;
    }

    return status;
}

static void write_redis_free(void *ptr)
{
    write_redis_t *node = ptr;

    if (node == NULL)
        return;

    if (node->conn != NULL) {
        redisFree(node->conn);
        node->conn = NULL;
    }

    free(node->host);
    free(node->socket);
    free(node->passwd);
    free(node->instance);
    strbuf_destroy(&node->buf_metric);
    strbuf_destroy(&node->buf_key);

    free(node);
}

static int write_redis_config_instance(config_item_t *ci)
{

    write_redis_t *node = calloc(1, sizeof(*node));
    if (node == NULL)
        return ENOMEM;

    node->host = NULL;
    node->port = 0;
    node->timeout = TIME_T_TO_CDTIME_T(1);
    node->conn = NULL;
    node->database = 0;
    node->store_rates = true;

    int status = cf_util_get_string(ci, &node->instance);
    if (status != 0) {
        write_redis_free(node);
        return status;
    }

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &node->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_port_number(child, &node->port);
        } else if (strcasecmp("socket", child->key) == 0) {
            status = cf_util_get_string(child, &node->socket);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &node->passwd);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &node->timeout);
        } else if (strcasecmp("database", child->key) == 0) {
            status = cf_util_get_int(child, &node->database);
        } else if (strcasecmp("retention", child->key) == 0) {
            status = cf_util_get_cdtime(child, &node->retention);
        } else if (strcasecmp("store-rates", child->key) == 0) {
            status = cf_util_get_boolean(child, &node->store_rates);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
          break;
    }

    if (status != 0) {
        write_redis_free(node);
        return -1;
    }

    if (node->socket == NULL) {
        if (node->host == NULL) {
           node->host = strdup("localhost");
           if (node->host == NULL) {
                PLUGIN_ERROR("strdup failed");
                write_redis_free(node);
                return -1;
           }
        }
        if (node->port == 0)
            node->port = 6379;
    }

    status = strbuf_resize(&node->buf_metric, 256);
    if (status != 0) {
        PLUGIN_ERROR("Failed to resize metrics buffer.");
        write_redis_free(node);
        return -1;
    }
        
    status = strbuf_resize(&node->buf_key, 1024);
    if (status != 0) {
        PLUGIN_ERROR("Failed to resize key buffer.");
        write_redis_free(node);
        return -1;
    }

    return plugin_register_write("write_redis", node->instance, write_redis_write, NULL, 0, 0,
                                &(user_data_t){ .data = node, .free_func = write_redis_free });
}

static int write_redis_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = write_redis_config_instance(child);
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
    plugin_register_config("write_redis", write_redis_config);
}
