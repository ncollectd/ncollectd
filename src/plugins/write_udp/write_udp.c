// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2012 Pierre-Yves Ritschard
// SPDX-FileCopyrightText: Copyright (C) 2011 Scott Sanders
// SPDX-FileCopyrightText: Copyright (C) 2009 Paul Sadauskas
// SPDX-FileCopyrightText: Copyright (C) 2009 Doug MacEachern
// SPDX-FileCopyrightText: Copyright (C) 2007-2013 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Doug MacEachern <dougm at hyperic.com>
// SPDX-FileContributor: Paul Sadauskas <psadauskas at gmail.com>
// SPDX-FileContributor: Scott Sanders <scott at jssjr.com>
// SPDX-FileContributor: Pierre-Yves Ritschard <pyr at spootnik.org>
// Based on the write_http plugin.

#include "plugin.h"
#include "libutils/common.h"
#include "libformat/format.h"

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifndef DEFAULT_NODE
#define DEFAULT_NODE "localhost"
#endif

#ifndef DEFAULT_SERVICE
#define DEFAULT_SERVICE "2003"
#endif

/* Ethernet - (IPv6 + TCP) = 1500 - (40 + 32) = 1428 */
#ifndef SEND_BUF_SIZE
#define SEND_BUF_SIZE 1428
#endif

typedef struct {
    char *name;
    int sock_fd;
    char *host;
    char *service;
    int ttl;
    int packet_size;
    format_dgram_metric_t format;
    cdtime_t flush_timeout;
    strbuf_t buf;
    char *send_buf;
    size_t send_buf_size;
    size_t send_buf_free;
    size_t send_buf_fill;
    cdtime_t send_buf_init_time;
} write_udp_callback_t;

static void write_udp_reset_buffer(write_udp_callback_t *cb)
{
    cb->send_buf_free = cb->send_buf_size;
    cb->send_buf_fill = 0;
    cb->send_buf_init_time = cdtime();
}

static int write_udp_send_buffer(write_udp_callback_t *cb)
{
    if (cb->sock_fd < 0)
        return -1;

    ssize_t status = swrite(cb->sock_fd, cb->send_buf, cb->send_buf_fill);
    if (status != 0) {
        PLUGIN_ERROR("send to %s:%s failed with status %zi (%s)",
                     cb->host, cb->service, status, STRERRNO);
        close(cb->sock_fd);
        cb->sock_fd = -1;
        return -1;
    }

    return 0;
}

static int write_udp_flush_internal(write_udp_callback_t *cb, cdtime_t timeout)
{
    int status;

    PLUGIN_DEBUG("timeout = %.3f; send_buf_fill = %" PRIsz, (double)timeout, cb->send_buf_fill);

    /* timeout == 0  => flush unconditionally */
    if (timeout > 0) {
        cdtime_t now = cdtime();
        if ((cb->send_buf_init_time + timeout) > now)
            return 0;
    }

    if (cb->send_buf_fill == 0) {
        cb->send_buf_init_time = cdtime();
        return 0;
    }

    status = write_udp_send_buffer(cb);
    write_udp_reset_buffer(cb);

    return status;
}

static int write_udp_callback_init(write_udp_callback_t *cb)
{
    if (cb->sock_fd > 0)
        return 0;

    struct addrinfo ai_hints = {.ai_family = AF_UNSPEC,
                                .ai_flags = AI_ADDRCONFIG,
                                .ai_socktype = SOCK_DGRAM};
    struct addrinfo *ai_list;
    int status = getaddrinfo(cb->host, cb->service, &ai_hints, &ai_list);
    if (status != 0) {
        PLUGIN_ERROR("getaddrinfo (%s, %s) failed: %s",
                     cb->host, cb->service, gai_strerror(status));
        return -1;
    }

    assert(ai_list != NULL);
    for (struct addrinfo *ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next) {
        cb->sock_fd = socket(ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol);
        if (cb->sock_fd < 0) {
            PLUGIN_ERROR("failed to open socket: %s", STRERRNO);
            continue;
        }

        status = connect(cb->sock_fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
        if (status != 0) {
            PLUGIN_ERROR("failed to connect to remote host: %s", STRERRNO);
            close(cb->sock_fd);
            cb->sock_fd = -1;
            continue;
        }

        if (cb->ttl > 0) {
            int ttl = cb->ttl;
            if (ai_ptr->ai_family == AF_INET) {
                struct sockaddr_in *addr = (struct sockaddr_in *)ai_ptr->ai_addr;
                int optname = IN_MULTICAST(ntohl(addr->sin_addr.s_addr)) ? IP_MULTICAST_TTL
                                                                         : IP_TTL;
                if (setsockopt(cb->sock_fd, IPPROTO_IP, optname, &ttl, sizeof(ttl)) != 0)
                    PLUGIN_WARNING("setsockopt(ipv4-ttl): %s", STRERRNO);
            } else if (ai_ptr->ai_family == AF_INET6) {
                struct sockaddr_in6 *addr = (struct sockaddr_in6 *)ai_ptr->ai_addr;
                int optname = IN6_IS_ADDR_MULTICAST(&addr->sin6_addr) ? IPV6_MULTICAST_HOPS
                                                                      : IPV6_UNICAST_HOPS;
                if (setsockopt(cb->sock_fd, IPPROTO_IPV6, optname, &ttl, sizeof(ttl)) != 0)
                    PLUGIN_WARNING("setsockopt(ipv6-ttl): %s", STRERRNO);
            }
        }

        break;
    }

    freeaddrinfo(ai_list);
    return 0;
}

static void write_udp_callback_free(void *data)
{
    if (data == NULL)
        return;

    write_udp_callback_t *cb = data;

    write_udp_flush_internal(cb, 0);

    if (cb->sock_fd >= 0) {
        close(cb->sock_fd);
        cb->sock_fd = -1;
    }

    free(cb->name);
    free(cb->host);
    free(cb->service);
    free(cb->send_buf);

    strbuf_destroy(&cb->buf);

    free(cb);
}

static int write_udp_flush(cdtime_t timeout, user_data_t *user_data)
{
    if (user_data == NULL)
        return -EINVAL;

    write_udp_callback_t *cb = user_data->data;

    int status = 0;
    if (cb->sock_fd < 0) {
        status = write_udp_callback_init(cb);
        if (status != 0) {
            /* An error message has already been printed. */
            return -1;
        }
    }

    status = write_udp_flush_internal(cb, timeout);

    return status;
}

static int write_udp_send_message(write_udp_callback_t *cb, char const *message, size_t message_len)
{
    int status = 0;
    if (cb->sock_fd < 0) {
        status = write_udp_callback_init(cb);
        if (status != 0)
            return -1;
    }

    if (message_len >= cb->send_buf_free) {
        status = write_udp_flush_internal(cb, 0);
        if (status != 0)
            return status;
    }

    /* Assert that we have enough space for this message. */
    assert(message_len < cb->send_buf_free);

    /* `message_len + 1' because `message_len' does not include the
     * trailing null byte. Neither does `send_buffer_fill'. */
    memcpy(cb->send_buf + cb->send_buf_fill, message, message_len);
    cb->send_buf_fill += message_len;
    cb->send_buf_free -= message_len;

    PLUGIN_DEBUG("[%s]:%s buf %" PRIsz "/%" PRIsz " (%.1f %%) \"%s\"",
                 cb->host, cb->service, cb->send_buf_fill, cb->send_buf_size,
                 100.0 * ((double)cb->send_buf_fill) / ((double)cb->send_buf_size),
                 message);

    return 0;
}

static int write_udp_write(metric_family_t const *fam, user_data_t *user_data)
{
    if ((fam == NULL) || (user_data == NULL))
        return EINVAL;

    write_udp_callback_t *cb = user_data->data;

    for (size_t i=0; i < fam->metric.num ; i++) {
        strbuf_reset(&cb->buf);
        int status = format_dgram_metric(cb->format, &cb->buf, fam, &fam->metric.ptr[i]);
        if (status == 0) {
            size_t size = strbuf_len(&cb->buf);
            write_udp_send_message(cb, cb->buf.ptr, size);
        }
    }

    return 0;
}

static int write_udp_config_instance(config_item_t *ci)
{
    write_udp_callback_t *cb = calloc(1, sizeof(*cb));
    if (cb == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }
    cb->sock_fd = -1;
    cb->name = NULL;
    cb->host = strdup(DEFAULT_NODE);
    cb->service = strdup(DEFAULT_SERVICE);
    cb->packet_size = SEND_BUF_SIZE;

    int status = cf_util_get_string(ci, &cb->name);
    if (status != 0) {
        write_udp_callback_free(cb);
        return status;
    }

    cdtime_t flush_interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &cb->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_service(child, &cb->service);
        } else if (strcasecmp("ttl", child->key) == 0) {
            status = cf_util_get_int(child, &cb->ttl);
            if (status == 0) {
                if ((cb->ttl <= 0) || (cb->ttl > 255)) {
                    PLUGIN_ERROR("'ttl' must be between 1 and 255.");
                    status = -1;
                }
            }
        } else if (strcasecmp("packet-max-size", child->key) == 0) {
            status = cf_util_get_int(child, &cb->packet_size);
            if (status == 0) {
                if ((cb->packet_size < 1024) || (cb->packet_size > 65535)) {
                    PLUGIN_ERROR("'packet-max-size' must be between 1024 and 65535.");
                    status = -1;
                }
            }
        } else if (strcasecmp("format-metric", child->key) == 0) {
            status = config_format_dgram_metric(child, &cb->format);
        } else if (strcasecmp("flush-interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &flush_interval);
        } else if (strcasecmp("flush-timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &cb->flush_timeout);
        } else {
            PLUGIN_ERROR("Invalid configuration option: %s.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        write_udp_callback_free(cb);
        return status;
    }

    cb->send_buf_size = cb->packet_size;
    cb->send_buf_free = cb->packet_size;
    cb->send_buf = malloc(cb->packet_size);
    if (cb->send_buf == NULL) {
        PLUGIN_ERROR("malloc failed.");
        write_udp_callback_free(cb);
        return -1;
    }

    status = strbuf_resize(&cb->buf, 4096);
    if (status != 0) {
        PLUGIN_ERROR("Buffer resize failed.");
        write_udp_callback_free(cb);
        return -1;
    }

    write_udp_callback_init(cb);

    plugin_register_write("write_udp", cb->name, write_udp_write,
                          write_udp_flush, flush_interval, cb->flush_timeout,
                          &(user_data_t){ .data = cb, .free_func = write_udp_callback_free });
    return 0;
}

static int write_udp_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = write_udp_config_instance(child);
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
    plugin_register_config("write_udp", write_udp_config);
}
