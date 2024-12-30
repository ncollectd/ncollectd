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
        .help = NULL,
    },
    [FAM_OLSRD_NEIGHBOR_LINK_QUALITY_RATIO] = {
        .name = "olsrd_neighbor_link_quality_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OLSRD_ROUTE_METRIC_HOPS] = {
        .name = "olsrd_route_metric_hops",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OLSRD_ROUTE_METRIC_COST] = {
        .name = "olsrd_route_metric_cost",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OLSRD_TOPOLOGY_LINK_QUALITY_RATIO] = {
        .name = "olsrd_topology_link_quality_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OLSRD_TOPOLOGY_NEIGHBOR_LINK_QUALITY_RATIO] = {
        .name = "olsrd_topology_neighbor_link_quality_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OLSRD_TOPOLOGY_COST] = {
        .name = "olsrd_topology_cost",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
};

typedef struct {
    char *instance;
    char *host;
    char *port;
    label_set_t labels;
    plugin_filter_t *filter;
    metric_family_t fams[FAM_OLSRD_MAX];
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

        status = connect(fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
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

    if (strtodouble(fields[3], &value) != 0) {
        PLUGIN_ERROR("Cannot parse link quality: %s", fields[3]);
    } else {
         metric_family_append(&oi->fams[FAM_OLSRD_LINK_QUALITY_RATIO],
                             VALUE_GAUGE(value), &oi->labels,
                             &(label_pair_const_t){.name="local_ip", .value=fields[0]},
                             &(label_pair_const_t){.name="remote_ip", .value=fields[1]},
                             NULL);
    }

    if (strtodouble(fields[4], &value) != 0) {
        PLUGIN_ERROR("Cannot parse neighbor link quality: %s", fields[4]);
    } else {
        metric_family_append(&oi->fams[FAM_OLSRD_NEIGHBOR_LINK_QUALITY_RATIO],
                             VALUE_GAUGE(value), &oi->labels,
                             &(label_pair_const_t){.name="local_ip", .value=fields[0]},
                             &(label_pair_const_t){.name="remote_ip", .value=fields[1]},
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

    if (strtodouble(fields[2], &value) != 0) {
        PLUGIN_ERROR("Unable to parse metric: %s", fields[2]);
    } else {
        metric_family_append(&oi->fams[FAM_OLSRD_ROUTE_METRIC_HOPS],
                             VALUE_GAUGE(value), &oi->labels,
                             &(label_pair_const_t){.name="destination", .value=fields[0]},
                             &(label_pair_const_t){.name="interface", .value=fields[4]},
                             NULL);
    }

    if (strtodouble(fields[3], &value) != 0) {
        PLUGIN_ERROR("Unable to parse ETX: %s", fields[3]);
    } else {
        metric_family_append(&oi->fams[FAM_OLSRD_ROUTE_METRIC_COST],
                             VALUE_GAUGE(value), &oi->labels,
                             &(label_pair_const_t){.name="destination", .value=fields[0]},
                             &(label_pair_const_t){.name="interface", .value=fields[4]},
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

    if (strtodouble(fields[2], &value) != 0) {
        PLUGIN_ERROR("Unable to parse LQ: %s", fields[2]);
    } else {
        metric_family_append(&oi->fams[FAM_OLSRD_TOPOLOGY_LINK_QUALITY_RATIO],
                             VALUE_GAUGE(value), &oi->labels,
                             &(label_pair_const_t){.name="destination", .value=fields[0]},
                             &(label_pair_const_t){.name="last_hop", .value=fields[1]},
                             NULL);
    }

    if (strtodouble(fields[3], &value) != 0) {
        PLUGIN_ERROR("Unable to parse NLQ: %s", fields[3]);
    } else {
        metric_family_append(&oi->fams[FAM_OLSRD_TOPOLOGY_NEIGHBOR_LINK_QUALITY_RATIO],
                             VALUE_GAUGE(value), &oi->labels,
                             &(label_pair_const_t){.name="destination", .value=fields[0]},
                             &(label_pair_const_t){.name="last_hop", .value=fields[1]},
                             NULL);
    }

    if (strtodouble(fields[4], &value) != 0) {
        PLUGIN_ERROR("Unable to parse Cost: %s", fields[3]);
    } else {
        metric_family_append(&oi->fams[FAM_OLSRD_TOPOLOGY_COST],
                             VALUE_GAUGE(value), &oi->labels,
                             &(label_pair_const_t){.name="destination", .value=fields[0]},
                             &(label_pair_const_t){.name="last_hop", .value=fields[1]},
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

            (*callback)(oi, fields_num, fields);
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

    fputs("\r\n", fh);
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

    if ((status != 0) && (oi->host!=NULL)) {
        oi->host = strdup("localhost");
        if (oi->host == NULL)
            status = 1;
    }
    if ((status != 0) && (oi->port!=NULL)) {
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
