// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/socket.h"
#include "libutils/random.h"
#include "libcompress/slz.h"
#include "libxson/render.h"

/* AIX doesn't have MSG_DONTWAIT */
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT MSG_NONBLOCK
#endif

typedef struct {
    char *instance;
    char *host;
    int port;
    int fd;
    bool compress;
    int log_level;
    unsigned int pckt_size;
    int rc;
} log_gelf_t;

static int log_gelf_send_chunked (log_gelf_t *ctx, uint8_t *msg, size_t msg_size)
{
    size_t chunks = msg_size / ctx->pckt_size;
    if ((msg_size % ctx->pckt_size) != 0)
        chunks++;

    if (chunks > 128) {
        PLUGIN_WARNING("message too big: %zu bytes, too many chunks", msg_size);
        return -1;
    }

    uint64_t messageid = ((uint64_t)(CDTIME_T_TO_NS(cdtime())) << 32) |
                          (uint64_t)cdrand_u();

    uint8_t header[12];
    header[0] = 0x1e;
    header[1] = 0x0f;
    memcpy (header+2, &messageid, 8);
    header[10] = chunks;

    struct iovec iov[2];
    iov[0].iov_base = header;
    iov[0].iov_len = 12;

    struct msghdr msghdr;
    memset(&msghdr, 0, sizeof(struct msghdr));
    msghdr.msg_iov = iov;
    msghdr.msg_iovlen = 2;

    size_t offset = 0;
    for (uint8_t n = 0; n < chunks; n++) {
        header[11] = n;

        iov[1].iov_base = msg + offset;
        if ((msg_size - offset) < ctx->pckt_size) {
            iov[1].iov_len = msg_size - offset;
        } else {
            iov[1].iov_len = ctx->pckt_size;
        }

        ssize_t send = sendmsg(ctx->fd, &msghdr, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (send < 0)
            PLUGIN_WARNING("Failed to send data.");
        offset += ctx->pckt_size;
    }

    return 0;
}

static int log_gelf_send(log_gelf_t *ctx, char *msg, size_t msg_size)
{
    if (ctx->compress) {
        size_t zdata_size = msg_size + 12;
        uint8_t *zdata = malloc(zdata_size);
        if (zdata == NULL) {
            PLUGIN_ERROR("malloc failed.");
            return -1;
        }

        struct slz_stream stream;
        memset(&stream, 0, sizeof(stream));
        slz_init(&stream, 1, SLZ_FMT_GZIP);

        int zsize = slz_encode(&stream, zdata, msg, msg_size, 0);
        if (zsize < 0) {
            free(zdata);
            return -1;
        }

        zsize += slz_finish(&stream, zdata+zsize);

        int status = 0;
        if (msg_size > ctx->pckt_size) {
            status = log_gelf_send_chunked(ctx, zdata, zsize);
        } else {
            status = send(ctx->fd, zdata, zsize, MSG_DONTWAIT | MSG_NOSIGNAL);
        }

        free(zdata);

        if (status < 0) {

        }

        return 0;
    }

    return send(ctx->fd, msg, msg_size, MSG_DONTWAIT | MSG_NOSIGNAL);
}

static int log_gelf_fmt_msg(strbuf_t *buf, const log_msg_t *msg)
{
    xson_render_t r = {0};
    xson_render_init(&r, buf, XSON_RENDER_TYPE_JSON, 0);

    int status = xson_render_map_open(&r);

    status |= xson_render_key_string(&r, "version");
    status |= xson_render_string(&r, "1.1");

    const char *hostname = plugin_get_hostname();
    if (hostname != NULL) {
        status |= xson_render_key_string(&r, "host");
        status |= xson_render_string(&r, hostname);
    }

    status |= xson_render_key_string(&r, "timestamp");
    status |= xson_render_integer(&r, CDTIME_T_TO_DOUBLE(msg->time));

    if (msg->plugin != NULL) {
        status |= xson_render_key_string(&r, "_plugin");
        status |= xson_render_string(&r, msg->plugin);
    }

    if (msg->file != NULL) {
        status |= xson_render_key_string(&r, "_file");
        status |= xson_render_string(&r, msg->file);
    }

    if (msg->line > 0) {
        status |= xson_render_key_string(&r, "_line");
        status |= xson_render_integer(&r, msg->line);
    }

    if (msg->func != NULL) {
        status |= xson_render_key_string(&r, "_function");
        status |= xson_render_string(&r, msg->func);
    }

    status |= xson_render_key_string(&r, "level");
    status |= xson_render_integer(&r, msg->severity);

    if (msg->msg != NULL) {
        status |= xson_render_key_string(&r, "short_message");
        status |= xson_render_string(&r, msg->msg);
    }

    status |= xson_render_map_close(&r);

    return status;
}

static void log_gelf_log(const log_msg_t *msg, user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL))
        return;

    if (msg == NULL)
        return;

    log_gelf_t *ctx = ud->data;

    if (msg->severity > ctx->log_level)
        return;

    strbuf_t buf = STRBUF_CREATE;

    int status = log_gelf_fmt_msg(&buf, msg);
    if (status != 0) {
        PLUGIN_ERROR("Failed to render message.");
        strbuf_destroy(&buf);
        return;
    }

    log_gelf_send(ctx, buf.ptr, strbuf_len(&buf));

    strbuf_destroy(&buf);

    return;
}

static int log_gelf_fmt_notification(strbuf_t *buf, const notification_t *n)
{
    xson_render_t r = {0};
    xson_render_init(&r, buf, XSON_RENDER_TYPE_JSON, 0);

    int status = xson_render_map_open(&r);

    status |= xson_render_key_string(&r, "version");
    status |= xson_render_string(&r, "1.1");

    const char *hostname = plugin_get_hostname();
    if (hostname != NULL) {
        status |= xson_render_key_string(&r, "host");
        status |= xson_render_string(&r, hostname);
    }

    status |= xson_render_key_string(&r, "timestamp");
    status |= xson_render_integer(&r, CDTIME_T_TO_DOUBLE(n->time));

    status |= xson_render_key_string(&r, "level");
    status |= xson_render_integer(&r, 6);

    status |= xson_render_key_string(&r, "severiry");
    switch (n->severity) {
    case NOTIF_FAILURE:
        status |= xson_render_string(&r, "failure");
        break;
    case NOTIF_WARNING:
        status |= xson_render_string(&r, "warning");
        break;
    case NOTIF_OKAY:
        status |= xson_render_string(&r, "okay");
        break;
    default:
        status |= xson_render_null(&r);
        break;
    }

    if (n->name != NULL) {
        status |= xson_render_key_string(&r, "_name");
        status |= xson_render_string(&r, n->name);
    }

    for (size_t i = 0; i < n->label.num ; n++) {
        label_pair_t *pair = &n->label.ptr[i];
        struct iovec iov[2] = {
            {.iov_base = "_label_",  .iov_len = strlen("_label_")  },
            {.iov_base = pair->name, .iov_len = strlen(pair->name) },
        };
        status |= xson_render_key_iov(&r, iov, 2);
        status |= xson_render_string(&r, pair->value);
    }

    for (size_t i = 0; i < n->annotation.num ; n++) {
        label_pair_t *pair = &n->annotation.ptr[i];
        struct iovec iov[2] = {
            {.iov_base = "_annotation_",  .iov_len = strlen("_annotation_") },
            {.iov_base = pair->name,      .iov_len = strlen(pair->name)     },
        };

        if (strcmp(pair->name, "summary") == 0) {
            status |= xson_render_key_string(&r, "short_message");
            status |= xson_render_string(&r, pair->value);
        }

        status |= xson_render_key_iov(&r, iov, 2);
        status |= xson_render_string(&r, pair->value);
    }

    status |= xson_render_map_close(&r);

    return status;
}

static int log_gelf_notification(const notification_t *n, user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL))
        return -1;

    log_gelf_t *ctx = ud->data;

    strbuf_t buf = STRBUF_CREATE;

    int status = log_gelf_fmt_notification(&buf, n);
    if (status != 0) {
        PLUGIN_ERROR("Failed to render notification.");
        strbuf_destroy(&buf);
        return 0;
    }

    log_gelf_send(ctx, buf.ptr, strbuf_len(&buf));

    strbuf_destroy(&buf);

    return 0;
}

static void log_gelf_free(void *arg)
{
    log_gelf_t *ctx = arg;

    if (ctx == NULL)
        return;

    ctx->rc--;
    if (ctx->rc > 0)
        return;

    if (ctx->fd >= 0)
        close(ctx->fd);

    free(ctx->instance);
    free(ctx->host);
    free(ctx);
}

static int log_gelf_config_instance(config_item_t *ci)
{
    log_gelf_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &ctx->instance);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(ctx);
        return status;
    }

    ctx->pckt_size = 1420;
    ctx->compress = true;
#ifdef NCOLLECTD_DEBUG
    ctx->log_level = LOG_DEBUG;
#else
    ctx->log_level = LOG_INFO;
#endif

    int ttl = 255;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "log-level") == 0) {
            status = cf_util_get_log_level(child, &ctx->log_level);
        } else if (strcasecmp(child->key, "host") == 0) {
            status = cf_util_get_string(child, &ctx->host);
        } else if (strcasecmp(child->key, "port") == 0) {
            status = cf_util_get_port_number(child, &ctx->port);
        } else if (strcasecmp(child->key, "packet-size") == 0) {
            status = cf_util_get_unsigned_int(child, &ctx->pckt_size);
        } else if (strcasecmp(child->key, "ttl") == 0) {
            status = cf_util_get_int(child, &ttl);
        } else if (strcasecmp(child->key, "compress") == 0) {
            status = cf_util_get_boolean(child, &ctx->compress);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        log_gelf_free(ctx);
        return -1;
    }

    if ((ttl < 1) || (ttl > 255)) {
        PLUGIN_ERROR("invalid ttl must be between 1 and 255.");
        log_gelf_free(ctx);
        return -1;
    }

    if (ctx->host == NULL) {
        ctx->host = strdup("localhost");
        if (ctx->host == NULL) {
            PLUGIN_ERROR("strdup failed");
            log_gelf_free(ctx);
            return -1;
        }
    }

    if (ctx->port == 0)
        ctx->port = 12201;

    ctx->fd = socket_connect_udp(ctx->host, ctx->port, ttl);
    if (ctx->fd < 0) {
        PLUGIN_ERROR("cannot open socket to %s:%d", ctx->host, ctx->port);
        log_gelf_free(ctx);
        return -1;
    }


    user_data_t user_data = { .data = ctx, .free_func = log_gelf_free };

    ctx->rc++;
    plugin_register_log("log_gelf", ctx->instance, log_gelf_log, &user_data);
    ctx->rc++;
    plugin_register_notification("log_gelf", ctx->instance, log_gelf_notification, &user_data);

    return 0;
}

static int log_gelf_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = log_gelf_config_instance(child);
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
    plugin_register_config("log_gelf", log_gelf_config);
}
