// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <fcntl.h>
#include <poll.h>

#define OLSRD_DEFAULT_NODE "localhost"
#define OLSRD_DEFAULT_SERVICE "2006"

enum {
    FAM_OLSRD_LINK_QUALITY_RATIO,
    FAM_OLSRD_NEIGHBOR_LINK_QUALITY_RATIO,
    FAM_OLSRD_ROUTE_METRIC_HOPS,
    FAM_OLSRD_ROUTE_METRIC_COST,
    FAM_OLSRD_TOPOLOGY_LINK_QUALITY_RATIO,
    FAM_OLSRD_TOPOLOGY_NEIGHBOR_LINK_QUALITY_RATIO,
    FAM_OLSRD_TOPOLOGY_COST,
    FAM_OLSRD_MAX,
};

static metric_family_t fams[FAM_OLSRD_MAX] = {
    [FAM_OLSRD_LINK_QUALITY_RATIO] = {
        .name = "olsrd_link_quality_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Link quality (LQ) of the link to the neighbor.",
    },
    [FAM_OLSRD_NEIGHBOR_LINK_QUALITY_RATIO] = {
        .name = "olsrd_neighbor_link_quality_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Neighbor link quality (NLQ) of the link to the neighbor.",
    },
    [FAM_OLSRD_ROUTE_METRIC_HOPS] = {
        .name = "olsrd_route_metric_hops",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of hops of the route to the destination.",
    },
    [FAM_OLSRD_ROUTE_METRIC_COST] = {
        .name = "olsrd_route_metric_cost",
        .type = METRIC_TYPE_GAUGE,
        .help = "Expected transmission count (ETX) of the route to the destination.",
    },
    [FAM_OLSRD_TOPOLOGY_LINK_QUALITY_RATIO] = {
        .name = "olsrd_topology_link_quality_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Link quality (LQ) of the topology entry.",
    },
    [FAM_OLSRD_TOPOLOGY_NEIGHBOR_LINK_QUALITY_RATIO] = {
        .name = "olsrd_topology_neighbor_link_quality_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Neighbor link quality (NLQ) of the topology entry.",
    },
    [FAM_OLSRD_TOPOLOGY_COST] = {
        .name = "olsrd_topology_cost",
        .type = METRIC_TYPE_GAUGE,
        .help = "Cost of the topology entry.",
    },
};

typedef struct {
    char *instance;
    char *host;
    char *port;
    label_set_t labels;
    plugin_filter_t *filter;
    metric_family_t fams[FAM_OLSRD_MAX];
    complain_t format_complaint;
} olsrd_t;

/* Strip trailing newline characters. Returns length of string. */
static size_t strchomp(char *buffer)
{
    size_t buffer_len;

    buffer_len = strlen(buffer);
    while ((buffer_len > 0) &&
           ((buffer[buffer_len - 1] == '\r') || (buffer[buffer_len - 1] == '\n'))) {
        buffer_len--;
        buffer[buffer_len] = 0;
    }

    return buffer_len;
}

static size_t strtabsplit(char *string, char **fields, size_t size)
{
    size_t i;
    char *ptr;
    char *saveptr;

    i = 0;
    ptr = string;
    saveptr = NULL;
    while ((fields[i] = strtok_r(ptr, " \t\r\n", &saveptr)) != NULL) {
        ptr = NULL;
        i++;

        if (i >= size)
            break;
    }

    return i;
}

/* Connect with a timeout and set the send and receive timeouts of the socket. */
static int olsrd_connect_timeout(int fd, const struct sockaddr *addr, socklen_t addrlen,
                                 cdtime_t timeout)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0)
        return -1;

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;

    int status = connect(fd, addr, addrlen);
    if ((status != 0) && (errno == EINPROGRESS)) {
        struct pollfd pfd = {.fd = fd, .events = POLLOUT};
        do {
            status = poll(&pfd, 1, (int)CDTIME_T_TO_MS(timeout));
        } while ((status < 0) && (errno == EINTR));

        if (status == 0) {
            errno = ETIMEDOUT;
            return -1;
        } else if (status < 0) {
            return -1;
        }

        int error = 0;
        socklen_t error_len = sizeof(error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &error_len) != 0)
            return -1;
        if (error != 0) {
            errno = error;
            return -1;
        }
        status = 0;
    }

    if (status != 0)
        return -1;

    if (fcntl(fd, F_SETFL, flags) < 0)
        return -1;

    struct timeval tv = CDTIME_T_TO_TIMEVAL(timeout);
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
        PLUGIN_WARNING("setsockopt(SO_RCVTIMEO) failed: %s", STRERRNO);
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0)
        PLUGIN_WARNING("setsockopt(SO_SNDTIMEO) failed: %s", STRERRNO);

    return 0;
}

static FILE *olsrd_connect(olsrd_t *oi)
{
    struct addrinfo *ai_list;
    struct addrinfo ai_hints = {.ai_family = AF_UNSPEC,
                                .ai_flags = AI_ADDRCONFIG,
                                .ai_protocol = IPPROTO_TCP,
                                .ai_socktype = SOCK_STREAM};

    int ai_return = getaddrinfo(oi->host, oi->port, &ai_hints, &ai_list);
    if (ai_return != 0) {
        PLUGIN_ERROR("getaddrinfo (%s, %s) failed: %s", oi->host, oi->port, gai_strerror(ai_return));
        return NULL;
    }

    FILE *fh = NULL;
    for (struct addrinfo *ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next) {
        int fd;
        int status;

        fd = socket(ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol);
        if (fd < 0) {
            PLUGIN_ERROR("socket failed: %s", STRERRNO);
            continue;
        }

        /* Do not block the read thread for more than one interval. */
        status = olsrd_connect_timeout(fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen,
                                       plugin_get_interval());
        if (status != 0) {
            PLUGIN_ERROR("connect failed: %s", STRERRNO);
            close(fd);
            continue;
        }

        fh = fdopen(fd, "r+");
        if (fh == NULL) {
            PLUGIN_ERROR("fdopen failed.");
            close(fd);
            continue;
        }

        break;
    }

    freeaddrinfo(ai_list);

    return fh;
}

/* Parse a numeric field, "INFINITE" is used by txtinfo for unreachable costs. */
static int olsrd_parse_value(const char *field, double *value)
{
    if (strcasecmp(field, "INFINITE") == 0) {
        *value = INFINITY;
        return 0;
    }

    return strtodouble(field, value);
}

static int olsrd_cb_ignore(__attribute__((unused)) olsrd_t *oi,
                           __attribute__((unused)) size_t fields_num,
                           __attribute__((unused)) char **fields)
{
    return 0;
}

static int olsrd_cb_links(olsrd_t *oi, size_t fields_num, char **fields)
{
    /* Fields:
     *  0 = Local IP
     *  1 = Remote IP
     *  2 = Hyst.
     *  3 = LQ
     *  4 = NLQ
     *  5 = Cost */
    if (fields_num != 6)
        return -1;

    double value = 0;

    if (olsrd_parse_value(fields[3], &value) != 0) {
        PLUGIN_DEBUG("Cannot parse link quality: %s", fields[3]);
    } else {
         metric_family_append(&oi->fams[FAM_OLSRD_LINK_QUALITY_RATIO],
                             VALUE_GAUGE(value), &oi->labels,
                             &LABEL_PAIR_CONST("local_ip", fields[0]),
                             &LABEL_PAIR_CONST("remote_ip", fields[1]),
                             NULL);
    }

    if (olsrd_parse_value(fields[4], &value) != 0) {
        PLUGIN_DEBUG("Cannot parse neighbor link quality: %s", fields[4]);
    } else {
        metric_family_append(&oi->fams[FAM_OLSRD_NEIGHBOR_LINK_QUALITY_RATIO],
                             VALUE_GAUGE(value), &oi->labels,
                             &LABEL_PAIR_CONST("local_ip", fields[0]),
                             &LABEL_PAIR_CONST("remote_ip", fields[1]),
                             NULL);
    }

    return 0;
}

static int olsrd_cb_routes(olsrd_t *oi, size_t fields_num, char **fields)
{
    /* Fields:
     *  0 = Destination
     *  1 = Gateway IP
     *  2 = Metric
     *  3 = ETX
     *  4 = Interface */
    if (fields_num != 5)
        return -1;

    double value = 0;

    if (olsrd_parse_value(fields[2], &value) != 0) {
        PLUGIN_DEBUG("Unable to parse metric: %s", fields[2]);
    } else {
        metric_family_append(&oi->fams[FAM_OLSRD_ROUTE_METRIC_HOPS],
                             VALUE_GAUGE(value), &oi->labels,
                             &LABEL_PAIR_CONST("destination", fields[0]),
                             &LABEL_PAIR_CONST("interface", fields[4]),
                             NULL);
    }

    if (olsrd_parse_value(fields[3], &value) != 0) {
        PLUGIN_DEBUG("Unable to parse ETX: %s", fields[3]);
    } else {
        metric_family_append(&oi->fams[FAM_OLSRD_ROUTE_METRIC_COST],
                             VALUE_GAUGE(value), &oi->labels,
                             &LABEL_PAIR_CONST("destination", fields[0]),
                             &LABEL_PAIR_CONST("interface", fields[4]),
                             NULL);
    }

    return 0;
}

static int olsrd_cb_topology(olsrd_t *oi, size_t fields_num, char **fields)
{
    /* Fields:
     *  0 = Dest. IP
     *  1 = Last hop IP
     *  2 = LQ
     *  3 = NLQ
     *  4 = Cost */
    if (fields_num != 5)
        return -1;

    double value = 0;

    if (olsrd_parse_value(fields[2], &value) != 0) {
        PLUGIN_DEBUG("Unable to parse LQ: %s", fields[2]);
    } else {
        metric_family_append(&oi->fams[FAM_OLSRD_TOPOLOGY_LINK_QUALITY_RATIO],
                             VALUE_GAUGE(value), &oi->labels,
                             &LABEL_PAIR_CONST("destination", fields[0]),
                             &LABEL_PAIR_CONST("last_hop", fields[1]),
                             NULL);
    }

    if (olsrd_parse_value(fields[3], &value) != 0) {
        PLUGIN_DEBUG("Unable to parse NLQ: %s", fields[3]);
    } else {
        metric_family_append(&oi->fams[FAM_OLSRD_TOPOLOGY_NEIGHBOR_LINK_QUALITY_RATIO],
                             VALUE_GAUGE(value), &oi->labels,
                             &LABEL_PAIR_CONST("destination", fields[0]),
                             &LABEL_PAIR_CONST("last_hop", fields[1]),
                             NULL);
    }

    if (olsrd_parse_value(fields[4], &value) != 0) {
        PLUGIN_DEBUG("Unable to parse Cost: %s", fields[4]);
    } else {
        metric_family_append(&oi->fams[FAM_OLSRD_TOPOLOGY_COST],
                             VALUE_GAUGE(value), &oi->labels,
                             &LABEL_PAIR_CONST("destination", fields[0]),
                             &LABEL_PAIR_CONST("last_hop", fields[1]),
                             NULL);
    }

    return 0;
}

static int olsrd_read_table(FILE *fh, olsrd_t *oi,
                            int (*callback)(olsrd_t *oi, size_t fields_num, char **fields))
{
    char buffer[1024];
    int lineno = 0;
    char *rline = NULL;
    while ((rline = fgets(buffer, sizeof(buffer), fh)) != NULL) {
        /* An empty line ends the table. */
        size_t buffer_len = strchomp(buffer);
        if (buffer_len == 0)
            break;
        if (lineno > 0) {
            char *fields[32];
            size_t fields_num = strtabsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

            if ((*callback)(oi, fields_num, fields) != 0)
                PLUGIN_ONCE_WARNING(&oi->format_complaint,
                                    "Unexpected number of columns (%zu) in a table row "
                                    "from %s:%s.", fields_num, oi->host, oi->port);
        }
        lineno++;
    }

    if (rline == NULL)
        return -1;

    return 0;
}

static void olsrd_free(void *arg)
{
    olsrd_t *oi = arg;

    if (oi == NULL)
        return;

    free(oi->instance);
    free(oi->host);
    free(oi->port);
    label_set_reset(&oi->labels);
    plugin_filter_free(oi->filter);

    free(oi);
}

static int olsrd_read(user_data_t *user_data)
{
    olsrd_t *oi = user_data->data;

    FILE *fh = olsrd_connect(oi);
    if (fh == NULL)
        return -1;

    fputs("/all \r\n", fh);
    fflush(fh);

    int status = 0;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        size_t buffer_len = strchomp(buffer);
        if (buffer_len == 0)
            continue;

        if (strcmp("Table: Links", buffer) == 0) {
            status = olsrd_read_table(fh, oi, olsrd_cb_links);
        } else if (strcmp("Table: Neighbors", buffer) == 0) {
            status = olsrd_read_table(fh, oi, olsrd_cb_ignore);
        } else if (strcmp("Table: Topology", buffer) == 0) {
            status = olsrd_read_table(fh, oi, olsrd_cb_topology);
        } else if (strcmp("Table: HNA", buffer) == 0) {
            status = olsrd_read_table(fh, oi, olsrd_cb_ignore);
        } else if (strcmp("Table: MID", buffer) == 0) {
            status = olsrd_read_table(fh, oi, olsrd_cb_ignore);
        } else if (strcmp("Table: Routes", buffer) == 0) {
            status = olsrd_read_table(fh, oi, olsrd_cb_routes);
        } else {
            if ((strcmp("HTTP/1.0 200 OK", buffer) != 0) &&
                (strcmp("Content-type: text/plain", buffer) != 0)) {
                PLUGIN_DEBUG("Unable to handle line: %s", buffer);
            }
        }

        if (status != 0)
            break;
    }

    if (ferror(fh))
        PLUGIN_WARNING("Error reading the response from %s:%s, the metrics may be incomplete.",
                       oi->host, oi->port);

    fclose(fh);

    plugin_dispatch_metric_family_array_filtered(oi->fams, FAM_OLSRD_MAX, oi->filter, 0);
    return 0;
}

static int olsrd_config_instance(config_item_t *ci)
{
    olsrd_t *oi = calloc(1, sizeof(*oi));
    if (oi == NULL) {
        PLUGIN_ERROR("calloc failed: %s.", STRERRNO);
        return ENOMEM;
    }

    memcpy(oi->fams, fams, FAM_OLSRD_MAX * sizeof(*fams));

    int status = cf_util_get_string(ci, &oi->instance);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        olsrd_free(oi);
        return -1;
    }

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *option = ci->children + i;
        if (strcasecmp("host", option->key) == 0) {
             status = cf_util_get_string(option, &oi->host);
        } else if (strcasecmp("port", option->key) == 0) {
            status = cf_util_get_service(option, &oi->port);
        } else if (strcasecmp("interval", option->key) == 0) {
            status = cf_util_get_cdtime(option, &interval);
        } else if (strcasecmp("label", option->key) == 0) {
            status = cf_util_get_label(option, &oi->labels);
        } else if (strcasecmp("filter", option->key) == 0) {
            status = plugin_filter_configure(option, &oi->filter);
        } else {
            PLUGIN_ERROR("Unknown configuration option given: %s", option->key);
            status = 1;
        }

        if (status != 0)
            break;
    }

    if ((status == 0) && (oi->host == NULL)) {
        oi->host = strdup("localhost");
        if (oi->host == NULL)
            status = 1;
    }
    if ((status == 0) && (oi->port == NULL)) {
        oi->port = strdup("2006");
        if (oi->port == NULL)
            status = 1;
    }

    if (status != 0) {
        olsrd_free(oi);
        return -1;
    }

    label_set_add(&oi->labels, true, "instance", oi->instance);

    return plugin_register_complex_read("olsrd", oi->instance, olsrd_read, interval,
                                        &(user_data_t){ .data = oi, .free_func = olsrd_free });
}

static int olsrd_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *option = ci->children + i;

        if (strcasecmp("instance", option->key) == 0) {
            status = olsrd_config_instance(option);
        } else {
            PLUGIN_ERROR("Option '%s' not allowed in olsrd configuration. "
                         "It will be ignored.", option->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("olsrd", olsrd_config);
}
