// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2012 Pierre-Yves Ritschard
// SPDX-FileCopyrightText: Copyright (C) 2011 Scott Sanders
// SPDX-FileCopyrightText: Copyright (C) 2009 Paul Sadauskas
// SPDX-FileCopyrightText: Copyright (C) 2009 Doug MacEachern
// SPDX-FileCopyrightText: Copyright (C) 2007-2012 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2013-2014 Limelight Networks, Inc.
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Doug MacEachern <dougm at hyperic.com>
// SPDX-FileContributor: Paul Sadauskas <psadauskas at gmail.com>
// SPDX-FileContributor: Scott Sanders <scott at jssjr.com>
// SPDX-FileContributor: Pierre-Yves Ritschard <pyr at spootnik.org>
// SPDX-FileContributor: Brett Hawn <bhawn at llnw.com>
// SPDX-FileContributor: Kevin Bowling <kbowling at llnw.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

// Based on the write_graphite and write_tsdb plugin.

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/random.h"
#include "libutils/strbuf.h"
#include "libformat/format.h"

#include <netdb.h>

#ifndef DEFAULT_NODE
#define DEFAULT_NODE "localhost"
#endif

#ifndef DEFAULT_SERVICE
#define DEFAULT_SERVICE "4242"
#endif

#ifndef SEND_BUF_SIZE
#define SEND_BUF_SIZE 65536
#endif

typedef struct {
    char *instance;

    struct addrinfo *ai;
    cdtime_t ai_last_update;
    int sock_fd;

    char *node;
    char *service;

    format_stream_metric_t format;
    strbuf_t buf;

    bool connect_failed_log_enabled;
    int connect_dns_failed_attempts_remaining;
    cdtime_t next_random_ttl;

    cdtime_t resolve_interval;
    cdtime_t resolve_jitter;
} write_tcp_callback_t;

static cdtime_t new_random_ttl(write_tcp_callback_t *cb)
{
    if (cb->resolve_jitter == 0)
        return 0;

    return (cdtime_t)cdrand_range(0, (long)cb->resolve_jitter);
}

static void set_sock_opts(int sockfd)
{
    int status;
    int socktype;

    status = getsockopt(sockfd, SOL_SOCKET, SO_TYPE, &socktype, &(socklen_t){sizeof(socktype)});
    if (status != 0) {
        PLUGIN_WARNING("failed to determine socket type");
        return;
    }

    if (socktype == SOCK_STREAM) {
        status = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &(int){1}, sizeof(int));
        if (status != 0)
            PLUGIN_WARNING("failed to set socket keepalive flag");

#ifdef TCP_KEEPIDLE
        int tcp_keepidle = (int)((CDTIME_T_TO_MS(plugin_get_interval()) - 1) / 100 + 1);
        status = setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &tcp_keepidle,
                            sizeof(tcp_keepidle));
        if (status != 0)
            PLUGIN_WARNING("failed to set socket tcp keepalive time");
#endif

#ifdef TCP_KEEPINTVL
        int tcp_keepintvl = (int)((CDTIME_T_TO_MS(plugin_get_interval()) - 1) / 1000 + 1);
        status = setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &tcp_keepintvl,
                            sizeof(tcp_keepintvl));
        if (status != 0)
            PLUGIN_WARNING("failed to set socket tcp keepalive interval");
#endif
  }
}

static int write_tcp_callback_init(write_tcp_callback_t *cb)
{
    int status;
    cdtime_t now;

    const char *node = cb->node ? cb->node : DEFAULT_NODE;
    const char *service = cb->service ? cb->service : DEFAULT_SERVICE;

    if (cb->sock_fd > 0)
        return 0;

    now = cdtime();
    if (cb->ai) {
        /* When we are here, we still have the IP in cache.
         * If we have remaining attempts without calling the DNS, we update the
         * last_update date so we keep the info until next time.
         * If there is no more attempts, we need to flush the cache.
         */

        if ((cb->ai_last_update + cb->resolve_interval + cb->next_random_ttl) < now) {
            cb->next_random_ttl = new_random_ttl(cb);
            if (cb->connect_dns_failed_attempts_remaining > 0) {
                cb->ai_last_update = now;
                cb->connect_dns_failed_attempts_remaining--;
            } else {
                freeaddrinfo(cb->ai);
                cb->ai = NULL;
            }
        }
    }

    if (cb->ai == NULL) {
        if ((cb->ai_last_update + cb->resolve_interval + cb->next_random_ttl) >= now) {
            PLUGIN_DEBUG("too many getaddrinfo(%s, %s) failures", node, service);
            return -1;
        }
        cb->ai_last_update = now;
        cb->next_random_ttl = new_random_ttl(cb);

        struct addrinfo ai_hints = {
                .ai_family = AF_UNSPEC,
                .ai_flags = AI_ADDRCONFIG,
                .ai_socktype = SOCK_STREAM,
        };

        status = getaddrinfo(node, service, &ai_hints, &cb->ai);
        if (status != 0) {
            if (cb->ai) {
                freeaddrinfo(cb->ai);
                cb->ai = NULL;
            }
            if (cb->connect_failed_log_enabled) {
                PLUGIN_ERROR("getaddrinfo(%s, %s) failed: %s", node, service, gai_strerror(status));
                cb->connect_failed_log_enabled = 0;
            }
            return -1;
        }
    }

    assert(cb->ai != NULL);
    for (struct addrinfo *ai = cb->ai; ai != NULL; ai = ai->ai_next) {
        cb->sock_fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (cb->sock_fd < 0)
            continue;

        set_sock_opts(cb->sock_fd);

        status = connect(cb->sock_fd, ai->ai_addr, ai->ai_addrlen);
        if (status != 0) {
            close(cb->sock_fd);
            cb->sock_fd = -1;
            continue;
        }

        break;
    }

    if (cb->sock_fd < 0) {
        PLUGIN_ERROR("Connecting to %s:%s failed. The last error was: %s", node, service, STRERRNO);
        return -1;
    }

    if (cb->connect_failed_log_enabled == 0) {
        PLUGIN_WARNING("Connecting to %s:%s succeeded.", node, service);
        cb->connect_failed_log_enabled = 1;
    }
    cb->connect_dns_failed_attempts_remaining = 1;

    return 0;
}

static void write_tcp_callback_free(void *data)
{
    write_tcp_callback_t *cb;

    if (data == NULL)
        return;

    cb = data;

    close(cb->sock_fd);
    cb->sock_fd = -1;

    strbuf_destroy(&cb->buf);

    free(cb->instance);
    free(cb->node);
    free(cb->service);

    free(cb);
}

static int write_tcp_write(metric_family_t const *fam, user_data_t *user_data)
{
    if ((fam == NULL) || (user_data == NULL))
        return EINVAL;

    write_tcp_callback_t *cb = user_data->data;

    format_stream_metric_ctx_t ctx = {0};

    strbuf_reset(&cb->buf);

    int status = format_stream_metric_begin(&ctx, cb->format, &cb->buf);
    status |= format_stream_metric_family(&ctx, fam);
    status |= format_stream_metric_end(&ctx);

    if (status != 0)
        return 0;

    if (cb->sock_fd < 0) {
        status = write_tcp_callback_init(cb);
        if (status != 0) {
            PLUGIN_ERROR("write_tcp_callback_init failed.");
            return -1;
        }
    }

    ssize_t ret = swrite(cb->sock_fd, cb->buf.ptr, strbuf_len(&cb->buf));
    if (ret != 0) {
        PLUGIN_ERROR("send failed with status %zi (%s)", ret , STRERRNO);
        close(cb->sock_fd);
        cb->sock_fd = -1;
        return -1;
    }

    return 0;
}

static int write_tcp_config_instance(config_item_t *ci)
{
    write_tcp_callback_t *cb = calloc(1, sizeof(*cb));
    if (cb == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &cb->instance);
    if (status != 0) {
        write_tcp_callback_free(cb);
        return status;
    }

    cb->sock_fd = -1;
    cb->connect_failed_log_enabled = 1;
    cb->next_random_ttl = new_random_ttl(cb);
    cb->format = FORMAT_STREAM_METRIC_OPENMETRICS_TEXT;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &cb->node);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_service(child, &cb->service);
        } else if (strcasecmp("resolve-interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &cb->resolve_interval);
        } else if (strcasecmp("resolve-jitter", child->key) == 0) {
            status = cf_util_get_cdtime(child, &cb->resolve_jitter);
        } else if (strcasecmp("format", child->key) == 0) {
            status = config_format_stream_metric(child, &cb->format);
        } else {
            PLUGIN_ERROR("Invalid configuration option: %s.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        write_tcp_callback_free(cb);
        return -1;
    }

    status = strbuf_resize(&cb->buf, 4096);
    if (status != 0) {
        PLUGIN_ERROR("Buffer resize failed.");
        write_tcp_callback_free(cb);
        return -1;
    }

    plugin_register_write("write_tcp", cb->instance, write_tcp_write, NULL, 0, 0,
                          &(user_data_t){.data = cb, .free_func = write_tcp_callback_free});
    return 0;
}

static int write_tcp_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = write_tcp_config_instance(child);
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
    plugin_register_config("write_tcp", write_tcp_config);
}
