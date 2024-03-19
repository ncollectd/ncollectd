// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText:  Copyright (C) 2014 Pierre-Yves Ritschard
// SPDX-FileContributor: Pierre-Yves Ritschard <pyr at spootnik.org>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/random.h"
#include "libformat/format.h"

#include <librdkafka/rdkafka.h>
#include <stdint.h>
#include <errno.h>

typedef struct {
    char *name;
    char *topic_name;
    char *key;
    format_stream_metric_t format_metric;
    format_notification_t format_notification;
    strbuf_t buf;
    rd_kafka_t *kafka;
    rd_kafka_conf_t *kafka_conf;
    rd_kafka_topic_conf_t *conf;
    rd_kafka_topic_t *topic;
} kafka_topic_context_t;

/* Version 0.9.0 of librdkafka deprecates rd_kafka_set_logger() in favor of
 * rd_kafka_conf_set_log_cb(). This is to make sure we're not using the
 * deprecated function. */
#ifdef HAVE_LIBRDKAFKA_LOG_CB
#undef HAVE_LIBRDKAFKA_LOGGER
#endif

#if defined(HAVE_LIBRDKAFKA_LOGGER) || defined(HAVE_LIBRDKAFKA_LOG_CB)
static void kafka_log(const rd_kafka_t *, int, const char *, const char *);

static void kafka_log(__attribute__((unused)) const rd_kafka_t *rkt, int level,
                      __attribute__((unused)) const char *fac, const char *msg)
{
    plugin_log(level, NULL, 0, NULL, "%s", msg);
}
#endif

static rd_kafka_resp_err_t kafka_error(void)
{
#if RD_KAFKA_VERSION >= 0x000b00ff
    return rd_kafka_last_error();
#else
    return rd_kafka_errno2err(errno);
#endif
}

static uint32_t kafka_hash(const char *keydata, size_t keylen)
{
    uint32_t hash = 5381;
    for (; keylen > 0; keylen--)
        hash = ((hash << 5) + hash) + keydata[keylen - 1];
    return hash;
}

/* 31 bit -> 4 byte -> 8 byte hex string + null byte */
#define KAFKA_RANDOM_KEY_SIZE 9
#define KAFKA_RANDOM_KEY_BUFFER   (char[KAFKA_RANDOM_KEY_SIZE]) { "" }

static char *kafka_random_key(char buffer[static KAFKA_RANDOM_KEY_SIZE])
{
    ssnprintf(buffer, KAFKA_RANDOM_KEY_SIZE, "%08" PRIX32, cdrand_u());
    return buffer;
}

static int32_t kafka_partition(const rd_kafka_topic_t *rkt, const void *keydata,
                               size_t keylen, int32_t partition_cnt,
                               __attribute__((unused)) void *p,
                               __attribute__((unused)) void *m)
{
    uint32_t key = kafka_hash(keydata, keylen);
    uint32_t target = key % partition_cnt;
    int32_t i = partition_cnt;

    while (--i > 0 && !rd_kafka_topic_partition_available(rkt, target)) {
        target = (target + 1) % partition_cnt;
    }
    return target;
}

static int kafka_handle(kafka_topic_context_t *ctx)
{
    char errbuf[1024];
    rd_kafka_conf_t *conf;
    rd_kafka_topic_conf_t *topic_conf;

    if (ctx->kafka != NULL && ctx->topic != NULL)
        return 0;

    if (ctx->kafka == NULL) {
        if ((conf = rd_kafka_conf_dup(ctx->kafka_conf)) == NULL) {
            PLUGIN_ERROR("cannot duplicate kafka config");
            return 1;
        }

        if ((ctx->kafka = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errbuf, sizeof(errbuf))) == NULL) {
            PLUGIN_ERROR("cannot create kafka handle.");
            return 1;
        }

        rd_kafka_conf_destroy(ctx->kafka_conf);
        ctx->kafka_conf = NULL;

        PLUGIN_INFO("created KAFKA handle : %s", rd_kafka_name(ctx->kafka));

#if defined(HAVE_LIBRDKAFKA_LOGGER) && !defined(HAVE_LIBRDKAFKA_LOG_CB)
        rd_kafka_set_logger(ctx->kafka, kafka_log);
#endif
    }

    if (ctx->topic == NULL) {
        if ((topic_conf = rd_kafka_topic_conf_dup(ctx->conf)) == NULL) {
            PLUGIN_ERROR("cannot duplicate kafka topic config");
            return 1;
        }

        if ((ctx->topic = rd_kafka_topic_new(ctx->kafka, ctx->topic_name, topic_conf)) == NULL) {
            PLUGIN_ERROR("cannot create topic : %s\n", rd_kafka_err2str(kafka_error()));
            return errno;
        }

        rd_kafka_topic_conf_destroy(ctx->conf);
        ctx->conf = NULL;

        PLUGIN_INFO("handle created for topic : %s", rd_kafka_topic_name(ctx->topic));
    }

    return 0;

}

static int kafka_notif(notification_t const *n, user_data_t *user_data)
{
    if ((n == NULL) || (user_data == NULL))
        return EINVAL;

    kafka_topic_context_t *ctx = user_data->data;

    int status = kafka_handle(ctx);
    if (status != 0)
        return status;

    void *key = (ctx->key != NULL) ? ctx->key : kafka_random_key(KAFKA_RANDOM_KEY_BUFFER);
    size_t keylen = strlen(key);

    strbuf_reset(&ctx->buf);
    status = format_notification(ctx->format_notification, &ctx->buf, n);
    if (status != 0) {
        PLUGIN_ERROR("Failed to format notification.");
        return status;
    }

    size_t size = strbuf_len(&ctx->buf);
    rd_kafka_produce(ctx->topic, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY,
                                 ctx->buf.ptr, size, key, keylen, NULL);

    return 0;
}

static int kafka_write(metric_family_t const *fam, user_data_t *user_data)
{
    if ((fam == NULL) || (user_data == NULL))
        return EINVAL;

    kafka_topic_context_t *ctx = user_data->data;

    int status = kafka_handle(ctx);
    if (status != 0)
        return status;

    void *key = (ctx->key != NULL) ? ctx->key : kafka_random_key(KAFKA_RANDOM_KEY_BUFFER);
    size_t keylen = strlen(key);

    strbuf_reset(&ctx->buf);
    format_stream_metric_ctx_t fctx = {0};
    status = format_stream_metric_begin(&fctx, ctx->format_metric, &ctx->buf);
    status |= format_stream_metric_family(&fctx, fam);
    status |= format_stream_metric_end(&fctx);
    if (status != 0) {
        PLUGIN_ERROR("Failed to format metric.");
        return status;
    }

    size_t size = strbuf_len(&ctx->buf);
    rd_kafka_produce(ctx->topic, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY,
                                 ctx->buf.ptr, size, key, keylen, NULL);

    return 0;
}

static void kafka_topic_context_free(void *p)
{
    kafka_topic_context_t *ctx = p;

    if (ctx == NULL)
        return;

    free(ctx->name);
    free(ctx->topic_name);
    free(ctx->key);

    strbuf_destroy(&ctx->buf);

    if (ctx->topic != NULL)
        rd_kafka_topic_destroy(ctx->topic);
    if (ctx->conf != NULL)
        rd_kafka_topic_conf_destroy(ctx->conf);
    if (ctx->kafka_conf != NULL)
        rd_kafka_conf_destroy(ctx->kafka_conf);
    if (ctx->kafka != NULL)
        rd_kafka_destroy(ctx->kafka);

    free(ctx);
}

static int kafka_config_property(config_item_t *ci, rd_kafka_conf_t *conf)
{
    if (ci->values_num != 2) {
        PLUGIN_WARNING("kafka properties need both a key and a value.");
        return -1;
    }

    if ((ci->values[0].type != CONFIG_TYPE_STRING) || (ci->values[1].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("kafka properties needs string arguments.");
        return -1;
    }

    const char *key = ci->values[0].value.string;
    const char *val = ci->values[1].value.string;

    char errbuf[1024];
    rd_kafka_conf_res_t ret = rd_kafka_conf_set(conf, key, val, errbuf, sizeof(errbuf));
    if (ret != RD_KAFKA_CONF_OK) {
        PLUGIN_WARNING("cannot set kafka property %s to %s: %s.", key, val, errbuf);
        return 1;
    }

    return 0;
}

static int kafka_config_instance(config_item_t *ci)
{
    kafka_topic_context_t *tctx = calloc(1, sizeof(*tctx));
    if (tctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &tctx->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        kafka_topic_context_free(tctx);
        return status;
    }

    rd_kafka_conf_t *conf;
    if ((conf = rd_kafka_conf_new()) == NULL) {
        kafka_topic_context_free(tctx);
        WARNING("cannot allocate kafka configuration.");
        return -1;
    }

    tctx->format_metric = FORMAT_STREAM_METRIC_OPENMETRICS_TEXT;
    tctx->format_notification = FORMAT_NOTIFICATION_JSON;
    tctx->key = NULL;

    tctx->kafka_conf = rd_kafka_conf_dup(conf);
    if (tctx->kafka_conf == NULL) {
        kafka_topic_context_free(tctx);
        PLUGIN_ERROR("cannot allocate memory for kafka config");
        return -1;
    }

#ifdef HAVE_LIBRDKAFKA_LOG_CB
    rd_kafka_conf_set_log_cb(tctx->kafka_conf, kafka_log);
#endif

    tctx->conf = rd_kafka_topic_conf_new();
    if (tctx->conf == NULL) {
        kafka_topic_context_free(tctx);
        PLUGIN_ERROR("cannot create topic configuration.");
        return -1;
    }

    cf_send_t send = SEND_METRICS;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = &ci->children[i];
        status = 0;

        if  (strcasecmp("topic", child->key) == 0) {
            status = cf_util_get_string(child, &tctx->topic_name);
        } else if (strcasecmp("property", child->key) == 0) {
            status = kafka_config_property(ci, tctx->kafka_conf);
        } else if (strcasecmp("key", child->key) == 0) {
            if (cf_util_get_string(child, &tctx->key) != 0)
                continue;
            if (strcasecmp("random", tctx->key) == 0) {
                free(tctx->key);
                tctx->key = strdup(kafka_random_key(KAFKA_RANDOM_KEY_BUFFER));
            }
        } else if (strcasecmp("write", child->key) == 0) {
            status = cf_uti_get_send(child, &send);
        } else if (strcasecmp("format-metric", child->key) == 0) {
            status = config_format_stream_metric(child, &tctx->format_metric);
        } else if (strcasecmp("format-notification", child->key) == 0) {
            status = config_format_notification(child, &tctx->format_notification);
        } else {
            PLUGIN_WARNING("Invalid directive: %s.", child->key);
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        kafka_topic_context_free(tctx);
        return -1;
    }

    rd_kafka_topic_conf_set_partitioner_cb(tctx->conf, kafka_partition);
    rd_kafka_topic_conf_set_opaque(tctx->conf, tctx);

    user_data_t user_data = (user_data_t){.data = tctx, .free_func = kafka_topic_context_free};

    if (send == SEND_NOTIFICATIONS)
        return plugin_register_notification("write_kafka", tctx->topic_name, kafka_notif,
                                                           &user_data);

    return plugin_register_write("write_kafka", tctx->topic_name, kafka_write, NULL, 0, 0,
                                                &user_data);
}

static int kafka_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = kafka_config_instance(child);
        } else {
            PLUGIN_ERROR("Invalid configuration option: %s.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("write_kafka", kafka_config);
}
