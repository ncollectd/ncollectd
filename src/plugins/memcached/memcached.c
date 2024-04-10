// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2007 Antony Dovgal
// SPDX-FileCopyrightText: Copyright (C) 2007-2012  Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2009 Doug MacEachern
// SPDX-FileCopyrightText: Copyright (C) 2009 Franck Lombardi
// SPDX-FileCopyrightText: Copyright (C) 2012 Nicolas Szalay
// SPDX-FileCopyrightText: Copyright (C) 2017 Pavel Rochnyak
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Antony Dovgal <tony at daylessday dot org>
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Doug MacEachern <dougm at hyperic.com>
// SPDX-FileContributor: Franck Lombardi
// SPDX-FileContributor: Nicolas Szalay
// SPDX-FileContributor: Pavel Rochnyak <pavel2000 ngs.ru>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>

#include <poll.h>

#include "plugins/memcached/memcached_fam.h"
#include "plugins/memcached/memcached_stats.h"

#define MEMCACHED_DEF_HOST "127.0.0.1"
#define MEMCACHED_DEF_PORT "11211"
#define MEMCACHED_CONNECT_TIMEOUT 10000
#define MEMCACHED_IO_TIMEOUT 5000

typedef struct {
    char *name;
    char *host;
    char *socket;
    char *port;
    label_set_t labels;
    metric_family_t fams[FAM_MEMCACHED_MAX];
    int fd;
} memcached_t;

static void memcached_free(void *arg)
{
    memcached_t *st = arg;
    if (st == NULL)
        return;

    if (st->fd >= 0) {
        shutdown(st->fd, SHUT_RDWR);
        close(st->fd);
        st->fd = -1;
    }

    label_set_reset(&st->labels);

    free(st->name);
    free(st->host);
    free(st->socket);
    free(st->port);
    free(st);
}

static int memcached_connect_unix(memcached_t *st)
{
    struct sockaddr_un serv_addr = {0};

    serv_addr.sun_family = AF_UNIX;
    sstrncpy(serv_addr.sun_path, st->socket, sizeof(serv_addr.sun_path));

    /* create our socket descriptor */
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        PLUGIN_ERROR("socket(2) failed: %s", STRERRNO);
        return -1;
    }

    /* connect to the memcached daemon */
    int status = connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (status != 0) {
        shutdown(fd, SHUT_RDWR);
        close(fd);
        return -1;
    }

    /* switch to non-blocking mode */
    int flags = fcntl(fd, F_GETFL);
    status = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (status != 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static int memcached_connect_inet(memcached_t *st)
{
    struct addrinfo *ai_list;
    int fd = -1;

    struct addrinfo ai_hints = {.ai_family = AF_UNSPEC,
                                .ai_flags = AI_ADDRCONFIG,
                                .ai_socktype = SOCK_STREAM};

    int status = getaddrinfo(st->host, st->port, &ai_hints, &ai_list);
    if (status != 0) {
        PLUGIN_ERROR("getaddrinfo(%s,%s) failed: %s", st->host, st->port,
                     (status == EAI_SYSTEM) ? STRERRNO : gai_strerror(status));
        return -1;
    }

    for (struct addrinfo *ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next) {
        /* create our socket descriptor */
        fd = socket(ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol);
        if (fd < 0) {
            PLUGIN_WARNING("socket(2) failed: %s", STRERRNO);
            continue;
        }

        /* switch socket to non-blocking mode */
        int flags = fcntl(fd, F_GETFL);
        status = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        if (status != 0) {
            close(fd);
            fd = -1;
            continue;
        }

        /* connect to the memcached daemon */
        status = (int)connect(fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
        if (status != 0 && errno != EINPROGRESS) {
            shutdown(fd, SHUT_RDWR);
            close(fd);
            fd = -1;
            continue;
        }

        /* Wait until connection establishes */
        struct pollfd pollfd = {
            .fd = fd,
            .events = POLLOUT,
        };
        do {
            status = poll(&pollfd, 1, MEMCACHED_CONNECT_TIMEOUT);
        } while (status < 0 && errno == EINTR);
        if (status <= 0) {
            close(fd);
            fd = -1;
            continue;
        }

        /* Check if all is good */
        int socket_error;
        status = getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&socket_error,
                                &(socklen_t){sizeof(socket_error)});
        if (status != 0 || socket_error != 0) {
            close(fd);
            fd = -1;
            continue;
        }
        /* A socket is opened and connection succeeded. We're done. */
        break;
    }

    freeaddrinfo(ai_list);
    return fd;
}

static void memcached_connect(memcached_t *st)
{
    if (st->fd >= 0)
        return;

    if (st->socket != NULL)
        st->fd = memcached_connect_unix(st);
    else
        st->fd = memcached_connect_inet(st);

    if (st->fd >= 0)
        PLUGIN_INFO("Instance \"%s\": connection established.", st->name);
}

static int memcached_query_daemon(char *cmd, char *buffer, size_t buffer_size, memcached_t *st)
{
    int status;
    size_t buffer_fill;

    memcached_connect(st);
    if (st->fd < 0) {
        PLUGIN_ERROR("Instance \"%s\" could not connect to daemon.", st->name);
        return -1;
    }

    struct pollfd pollfd = {
        .fd = st->fd,
        .events = POLLOUT,
    };

    do {
        status = poll(&pollfd, 1, MEMCACHED_IO_TIMEOUT);
    } while (status < 0 && errno == EINTR);

    if (status <= 0) {
        PLUGIN_ERROR("poll() failed for write() call.");
        close(st->fd);
        st->fd = -1;
        return -1;
    }

    status = swrite(st->fd, cmd, strlen(cmd));
    if (status != 0) {
        PLUGIN_ERROR("Instance \"%s\": write(2) failed: %s", st->name,
                    STRERRNO);
        shutdown(st->fd, SHUT_RDWR);
        close(st->fd);
        st->fd = -1;
        return -1;
    }

    /* receive data from the memcached daemon */
    memset(buffer, 0, buffer_size);

    buffer_fill = 0;
    pollfd.events = POLLIN;
    while (1) {
        do {
            status = poll(&pollfd, 1, MEMCACHED_IO_TIMEOUT);
        } while (status < 0 && errno == EINTR);

        if (status <= 0) {
            PLUGIN_ERROR("Instance \"%s\": Timeout reading from socket", st->name);
            close(st->fd);
            st->fd = -1;
            return -1;
        }

        do {
            status = recv(st->fd, buffer + buffer_fill, buffer_size - buffer_fill, 0);
        } while (status < 0 && errno == EINTR);

        char const end_token[5] = {'E', 'N', 'D', '\r', '\n'};
        if (status < 0) {

#if EAGAIN != EWOULDBLOCK
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                continue;
#else
            if (errno == EAGAIN)
                continue;
#endif

            PLUGIN_ERROR("Instance \"%s\": Error reading from socket: %s", st->name, STRERRNO);
            shutdown(st->fd, SHUT_RDWR);
            close(st->fd);
            st->fd = -1;
            return -1;
        } else if (status == 0) {
            PLUGIN_ERROR("Instance \"%s\": Connection closed by peer", st->name);
            close(st->fd);
            st->fd = -1;
            return -1;
        }

        buffer_fill += status;
        if (buffer_fill > buffer_size) {
            buffer_fill = buffer_size;
            PLUGIN_WARNING("Instance \"%s\": Message was truncated.", st->name);
            shutdown(st->fd, SHUT_RDWR);
            close(st->fd);
            st->fd = -1;
            break;
        }

        /* If buffer ends in end_token, we have all the data. */
        if (memcmp(buffer + buffer_fill - sizeof(end_token), end_token,
                             sizeof(end_token)) == 0)
            break;
    }

    status = 0;
    if (buffer_fill == 0) {
        PLUGIN_WARNING("Instance \"%s\": No data returned by memcached.", st->name);
        status = -1;
    }

    return status;
}

static int memcached_read_stats_slabs(memcached_t *st)
{
    char buf[4096];
    char *fields[3];
    char *line;

    if (memcached_query_daemon("stats slabs\r\n", buf, sizeof(buf), st) < 0)
        return -1;

    char *ptr = buf;
    char *saveptr = NULL;
    while ((line = strtok_r(ptr, "\n\r", &saveptr)) != NULL) {
        ptr = NULL;

        if (strsplit(line, fields, 3) != 3)
            continue;

        // 1:chunk_size 96
        char *class = fields[1];
        char *name = strchr(class, ':');
        if (name == NULL)
            continue;
        *name = '\0';
        name++;

        char key[64] = {'s', 'l', 'a', 'b', 's', ':', '\0' };
        sstrncpy(key + strlen("slabs:"), name, sizeof(key) - strlen("slabs:"));

        const struct memcached_metric *mm = memcached_get_key(key, strlen(key));
        if (mm != NULL) {
            metric_family_t *fam = &st->fams[mm->fam];

            value_t value = {0};
            if (fam->type == METRIC_TYPE_COUNTER) {
                value = VALUE_COUNTER(atoll(fields[2]));
            } else if (fam->type == METRIC_TYPE_GAUGE) {
                value = VALUE_GAUGE(atoll(fields[2]));
            } else {
                continue;
            }
            metric_family_append(fam, value, &st->labels,
                                 &(label_pair_const_t){.name="class", .value=class}, NULL);
        }
    }

    return 0;
}

static int memcached_read_stats_items(memcached_t *st)
{
    char buf[4096];
    char *fields[3];
    char *line;

    if (memcached_query_daemon("stats items\r\n", buf, sizeof(buf), st) < 0)
        return -1;

    char *ptr = buf;
    char *saveptr = NULL;
    while ((line = strtok_r(ptr, "\n\r", &saveptr)) != NULL) {
        ptr = NULL;
        if (strsplit(line, fields, 3) != 3)
            continue;

        // items:1:number
        if (strncmp("items:", fields[1], strlen("items:")) != 0)
            continue;

        char *class = fields[1] + strlen("items:");
        char *name = strchr(class, ':');
        if (name == NULL)
            continue;
        *name = '\0';
        name++;

        char key[64] = {'i', 't', 'e', 'm', 's', ':', '\0' };
        sstrncpy(key + strlen("items:"), name, sizeof(key) - strlen("items:"));

        const struct memcached_metric *mm = memcached_get_key(key, strlen(key));
        if (mm != NULL) {
            metric_family_t *fam = &st->fams[mm->fam];

            value_t value = {0};
            if (fam->type == METRIC_TYPE_COUNTER) {
                value = VALUE_COUNTER(atoll(fields[2]));
            } else if (fam->type == METRIC_TYPE_GAUGE) {
                value = VALUE_GAUGE(atoll(fields[2]));
            } else {
                continue;
            }
            metric_family_append(fam, value, &st->labels,
                                 &(label_pair_const_t){.name="class", .value=class}, NULL);
        }
    }

    return 0;
}

static int memcached_read_stats(memcached_t *st)
{
    char buf[4096];
    char *fields[3];
    char *line;

    /* get data from daemon */
    if (memcached_query_daemon("stats\r\n", buf, sizeof(buf), st) < 0)
        return -1;

    char *ptr = buf;
    char *saveptr = NULL;
    while ((line = strtok_r(ptr, "\n\r", &saveptr)) != NULL) {
        ptr = NULL;
        if (strsplit(line, fields, 3) != 3)
            continue;

        size_t name_len = strlen(fields[1]);
        if (name_len == 0)
            continue;

        const struct memcached_metric *mm = memcached_get_key(fields[1], name_len);
        if (mm != NULL) {
            metric_family_t *fam = &st->fams[mm->fam];

            value_t value = {0};

            switch (mm->fam) {
                case FAM_MEMCACHED_VERSION:
                    label_set_add(&value.info, true, "version", fields[2]);
                    break;
                case FAM_MEMCACHED_RUSAGE_USER_SECONDS:
                case FAM_MEMCACHED_RUSAGE_SYSTEM_SECONDS:
                    value = VALUE_COUNTER_FLOAT64(atof(fields[2]));
                    break;
                case FAM_MEMCACHED_TIME_LISTEN_DISABLED_SECONDS:
                    value = VALUE_COUNTER_FLOAT64(atof(fields[2]) * 1000000.0);
                    break;
                default:
                    if (fam->type == METRIC_TYPE_COUNTER) {
                        value = VALUE_COUNTER(atoll(fields[2]));
                    } else if (fam->type == METRIC_TYPE_GAUGE) {
                         value = VALUE_GAUGE(atof(fields[2]));
                    } else {
                        continue;
                    }
                    break;
            }

            metric_family_append(fam, value, &st->labels, NULL);
            if (mm->fam == FAM_MEMCACHED_VERSION) {
                label_set_reset(&value.info);
            }
        }
    }

    return 0;
}

static int memcached_read(user_data_t *user_data)
{
    memcached_t *st = user_data->data;

    int status = memcached_read_stats(st);
    if (status != 0) {
        metric_family_append(&st->fams[FAM_MEMCACHED_UP], VALUE_GAUGE(0), &st->labels, NULL);
        plugin_dispatch_metric_family(&st->fams[FAM_MEMCACHED_UP], 0);
        return 0;
    }

    metric_family_append(&st->fams[FAM_MEMCACHED_UP], VALUE_GAUGE(1), &st->labels, NULL);

    memcached_read_stats_items(st);
    memcached_read_stats_slabs(st);

    plugin_dispatch_metric_family_array(st->fams, FAM_MEMCACHED_MAX, 0);
    return 0;
}

static int config_add_instance(config_item_t *ci)
{
    int status = 0;

    memcached_t *st = calloc(1, sizeof(*st));
    if (st == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return ENOMEM;
    }

    memcpy(st->fams, fams, sizeof(*fams) * FAM_MEMCACHED_MAX);

    st->fd = -1;

    status = cf_util_get_string(ci, &st->name);
    if (status != 0) {
        free(st);
        return status;
    }

    cdtime_t interval = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("socket", child->key) == 0) {
            status = cf_util_get_string(child, &st->socket);
        } else if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &st->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_service(child, &st->port);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &st->labels);
        } else {
            PLUGIN_WARNING("Option `%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        memcached_free(st);
        return -1;
    }

    if (st->socket == NULL) {
        if (st->host == NULL) {
            st->host = strdup(MEMCACHED_DEF_HOST);
            if (st->host == NULL) {
                memcached_free(st);
                return -1;
            }
        }

        if (st->port == NULL) {
            st->port = strdup(MEMCACHED_DEF_PORT);
            if (st->port == NULL) {
                memcached_free(st);
                return -1;
            }
        }
    }

    label_set_add(&st->labels, true, "instance", st->name);

    return plugin_register_complex_read("memcached", st->name, memcached_read, interval,
                                        &(user_data_t){.data=st, .free_func=memcached_free});
}

static int memcached_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = config_add_instance(child);
        } else {
            PLUGIN_WARNING("The configuration option \"%s\" is not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("memcached", memcached_config);
}
