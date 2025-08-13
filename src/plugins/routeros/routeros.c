// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009,2010 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include "ros_api.h"

enum {
    FAM_ROUTEROS_UP,
    FAM_ROUTEROS_IF_RX_PACKETS,
    FAM_ROUTEROS_IF_TX_PACKETS,
    FAM_ROUTEROS_IF_RX_BYTES,
    FAM_ROUTEROS_IF_TX_BYTES,
    FAM_ROUTEROS_IF_RX_ERRORS,
    FAM_ROUTEROS_IF_TX_ERRORS,
    FAM_ROUTEROS_IF_RX_DROPPED,
    FAM_ROUTEROS_IF_TX_DROPPED,
    FAM_ROUTEROS_REGTABLE_RX_BITRATE,
    FAM_ROUTEROS_REGTABLE_TX_BITRATE,
    FAM_ROUTEROS_REGTABLE_RX_SIGNAL_POWER,
    FAM_ROUTEROS_REGTABLE_TX_SIGNAL_POWER,
    FAM_ROUTEROS_REGTABLE_RX_SIGNAL_QUALITY,
    FAM_ROUTEROS_REGTABLE_TX_SIGNAL_QUALITY,
    FAM_ROUTEROS_REGTABLE_RX_BYTES,
    FAM_ROUTEROS_REGTABLE_TX_BYTES,
    FAM_ROUTEROS_REGTABLE_SIGNAL_TO_NOISE,
    FAM_ROUTEROS_CPU_LOAD,
    FAM_ROUTEROS_MEMORY_USED_BYTES,
    FAM_ROUTEROS_MEMORY_FREE_BYTES,
    FAM_ROUTEROS_SECTORS_WRITTEN,
    FAM_ROUTEROS_BAD_BLOCKS,
    FAM_ROUTEROS_SYSTEM_VOTAGE,
    FAM_ROUTEROS_SYSTEM_TEMPERATURE,
    FAM_ROUTEROS_MAX,
};

static metric_family_t fams[FAM_ROUTEROS_MAX] = {
    [FAM_ROUTEROS_UP] = {
        .name = "routeros_up",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ROUTEROS_IF_RX_PACKETS] = {
        .name = "routeros_if_rx_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ROUTEROS_IF_TX_PACKETS] = {
        .name = "routeros_if_tx_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ROUTEROS_IF_RX_BYTES] = {
        .name = "routeros_if_rx_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ROUTEROS_IF_TX_BYTES] = {
        .name = "routeros_if_tx_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ROUTEROS_IF_RX_ERRORS] = {
        .name = "routeros_if_rx_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ROUTEROS_IF_TX_ERRORS] = {
        .name = "routeros_if_tx_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ROUTEROS_IF_RX_DROPPED] = {
        .name = "routeros_if_rx_dropped",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ROUTEROS_IF_TX_DROPPED] = {
        .name = "routeros_if_tx_dropped",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ROUTEROS_REGTABLE_RX_BITRATE] = {
        .name = "routeros_regtable_rx_bitrate",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ROUTEROS_REGTABLE_TX_BITRATE] = {
        .name = "routeros_regtable_tx_bitrate",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ROUTEROS_REGTABLE_RX_SIGNAL_POWER] = {
        .name = "routeros_regtable_rx_signal_power",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ROUTEROS_REGTABLE_TX_SIGNAL_POWER] = {
        .name = "routeros_regtable_tx_signal_power",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ROUTEROS_REGTABLE_RX_SIGNAL_QUALITY] = {
        .name = "routeros_regtable_rx_signal_quality",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ROUTEROS_REGTABLE_TX_SIGNAL_QUALITY] = {
        .name = "routeros_regtable_tx_signal_quality",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ROUTEROS_REGTABLE_RX_BYTES] = {
        .name = "routeros_regtable_rx_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ROUTEROS_REGTABLE_TX_BYTES] = {
        .name = "routeros_regtable_tx_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ROUTEROS_REGTABLE_SIGNAL_TO_NOISE] = {
        .name = "routeros_regtable_signal_to_noise",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ROUTEROS_CPU_LOAD] = {
        .name = "routeros_cpu_load",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ROUTEROS_MEMORY_USED_BYTES] = {
        .name = "routeros_memory_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ROUTEROS_MEMORY_FREE_BYTES] = {
        .name = "routeros_memory_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ROUTEROS_SECTORS_WRITTEN] = {
        .name = "routeros_sectors_written",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_ROUTEROS_BAD_BLOCKS] = {
        .name = "routeros_bad_blocks",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ROUTEROS_SYSTEM_VOTAGE] = {
        .name = "routeros_system_votage",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_ROUTEROS_SYSTEM_TEMPERATURE] = {
        .name = "routeros_system_temperature",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
};

typedef struct {
    char *name;
    char *node;
    char *service;
    char *username;
    char *password;

    label_set_t labels;
    plugin_filter_t *filter;
    metric_family_t fams[FAM_ROUTEROS_MAX];
    ros_connection_t *connection;
} cr_data_t;

static int handle_interface(__attribute__((unused)) ros_connection_t *c,
                            const ros_interface_t *i, void *user_data)
{
    if ((i == NULL) || (user_data == NULL))
        return EINVAL;

    cr_data_t *rd = user_data;

    while (i != NULL) {
        if (!i->running) {
            i = i->next;
            continue;
        }

        metric_family_append(&rd->fams[FAM_ROUTEROS_IF_RX_PACKETS],
                             VALUE_COUNTER(i->rx_packets), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=i->name}, NULL);
        metric_family_append(&rd->fams[FAM_ROUTEROS_IF_TX_PACKETS],
                             VALUE_COUNTER(i->tx_packets), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=i->name}, NULL);
        metric_family_append(&rd->fams[FAM_ROUTEROS_IF_RX_BYTES],
                             VALUE_COUNTER(i->rx_bytes), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=i->name}, NULL);
        metric_family_append(&rd->fams[FAM_ROUTEROS_IF_TX_BYTES],
                             VALUE_COUNTER(i->tx_bytes), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=i->name}, NULL);
        metric_family_append(&rd->fams[FAM_ROUTEROS_IF_RX_ERRORS],
                             VALUE_COUNTER(i->rx_errors), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=i->name}, NULL);
        metric_family_append(&rd->fams[FAM_ROUTEROS_IF_TX_ERRORS],
                             VALUE_COUNTER(i->tx_errors), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=i->name}, NULL);
        metric_family_append(&rd->fams[FAM_ROUTEROS_IF_RX_DROPPED],
                             VALUE_COUNTER(i->rx_drops), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=i->name}, NULL);
        metric_family_append(&rd->fams[FAM_ROUTEROS_IF_TX_DROPPED],
                             VALUE_COUNTER(i->tx_drops), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=i->name}, NULL);

        i = i->next;
    }

    return 0;
}

static int handle_regtable(__attribute__((unused)) ros_connection_t *c,
                           const ros_registration_table_t *r, void *user_data)
{
    if ((r == NULL) || (user_data == NULL))
        return EINVAL;
    cr_data_t *rd = user_data;

    while (r != NULL) {
        const char *name = r->radio_name;
        if (name == NULL)
            name = r->mac_address;
        if (name == NULL)
            name = "default";

        metric_family_append(&rd->fams[FAM_ROUTEROS_REGTABLE_RX_BITRATE],
                             VALUE_GAUGE(1000000.0 * r->rx_rate), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=r->interface},
                             &(label_pair_const_t){.name="radio", .value=name},
                             NULL);
        metric_family_append(&rd->fams[FAM_ROUTEROS_REGTABLE_TX_BITRATE],
                             VALUE_GAUGE(1000000.0 * r->tx_rate), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=r->interface},
                             &(label_pair_const_t){.name="radio", .value=name},
                             NULL);
        metric_family_append(&rd->fams[FAM_ROUTEROS_REGTABLE_RX_SIGNAL_POWER],
                             VALUE_GAUGE(r->rx_signal_strength), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=r->interface},
                             &(label_pair_const_t){.name="radio", .value=name},
                             NULL);
        metric_family_append(&rd->fams[FAM_ROUTEROS_REGTABLE_TX_SIGNAL_POWER],
                             VALUE_GAUGE(r->tx_signal_strength), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=r->interface},
                             &(label_pair_const_t){.name="radio", .value=name},
                             NULL);
        metric_family_append(&rd->fams[FAM_ROUTEROS_REGTABLE_RX_SIGNAL_QUALITY],
                             VALUE_GAUGE(r->rx_ccq), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=r->interface},
                             &(label_pair_const_t){.name="radio", .value=name},
                             NULL);
        metric_family_append(&rd->fams[FAM_ROUTEROS_REGTABLE_TX_SIGNAL_QUALITY],
                             VALUE_GAUGE(r->tx_ccq), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=r->interface},
                             &(label_pair_const_t){.name="radio", .value=name},
                             NULL);
        metric_family_append(&rd->fams[FAM_ROUTEROS_REGTABLE_RX_BYTES],
                             VALUE_COUNTER(r->rx_bytes), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=r->interface},
                             &(label_pair_const_t){.name="radio", .value=name},
                             NULL);
        metric_family_append(&rd->fams[FAM_ROUTEROS_REGTABLE_TX_BYTES],
                             VALUE_COUNTER(r->tx_bytes), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=r->interface},
                             &(label_pair_const_t){.name="radio", .value=name},
                             NULL);
        metric_family_append(&rd->fams[FAM_ROUTEROS_REGTABLE_SIGNAL_TO_NOISE],
                             VALUE_GAUGE(r->signal_to_noise), &rd->labels,
                             &(label_pair_const_t){.name="interface", .value=r->interface},
                             &(label_pair_const_t){.name="radio", .value=name},
                             NULL);
        r = r->next;
    }

    return 0;
}

static int handle_system_resource(__attribute__((unused)) ros_connection_t *c,
                                  const ros_system_resource_t *r,
                                  __attribute__((unused)) void *user_data)
{
    if ((r == NULL) || (user_data == NULL))
        return EINVAL;
    cr_data_t *rd = user_data;

    metric_family_append(&rd->fams[FAM_ROUTEROS_CPU_LOAD],
                         VALUE_GAUGE(r->cpu_load), &rd->labels, NULL);
    metric_family_append(&rd->fams[FAM_ROUTEROS_MEMORY_USED_BYTES],
                         VALUE_GAUGE(r->total_memory - r->free_memory), &rd->labels, NULL);
    metric_family_append(&rd->fams[FAM_ROUTEROS_MEMORY_FREE_BYTES],
                         VALUE_GAUGE(r->free_memory), &rd->labels, NULL);
    metric_family_append(&rd->fams[FAM_ROUTEROS_SECTORS_WRITTEN],
                         VALUE_COUNTER(r->write_sect_total), &rd->labels, NULL);
    metric_family_append(&rd->fams[FAM_ROUTEROS_BAD_BLOCKS],
                         VALUE_GAUGE(r->bad_blocks), &rd->labels, NULL);

    return 0;
}

static int handle_system_health(__attribute__((unused)) ros_connection_t *c,
                                const ros_system_health_t *r,
                                __attribute__((unused)) void *user_data)
{
    if ((r == NULL) || (user_data == NULL))
        return EINVAL;

    cr_data_t *rd = user_data;

    metric_family_append(&rd->fams[FAM_ROUTEROS_SYSTEM_VOTAGE],
                         VALUE_GAUGE(r->voltage), &rd->labels, NULL);
    metric_family_append(&rd->fams[FAM_ROUTEROS_SYSTEM_TEMPERATURE],
                         VALUE_GAUGE(r->temperature), &rd->labels, NULL);
    return 0;
}

static int cr_read(user_data_t *user_data)
{
    int status;

    if (user_data == NULL)
        return EINVAL;

    cr_data_t *rd = user_data->data;
    if (rd == NULL)
        return EINVAL;

    if (rd->connection == NULL) {
        rd->connection = ros_connect(rd->node, rd->service, rd->username, rd->password);
        if (rd->connection == NULL) {
            PLUGIN_ERROR("ros_connect failed: %s", STRERRNO);
            metric_family_append(&rd->fams[FAM_ROUTEROS_UP], VALUE_GAUGE(0), &rd->labels, NULL);
            plugin_dispatch_metric_family_filtered(&rd->fams[FAM_ROUTEROS_UP], rd->filter, 0);
            return 0;
        }
    }

    cdtime_t submit = cdtime();

    metric_family_append(&rd->fams[FAM_ROUTEROS_UP], VALUE_GAUGE(1), &rd->labels, NULL);

    assert(rd->connection != NULL);
    while (true) {
        status = ros_interface(rd->connection, handle_interface, /* user data = */ rd);
        if (status != 0) {
            PLUGIN_ERROR("ros_interface failed: %s", STRERROR(status));
            break;
        }

        status = ros_registration_table(rd->connection, handle_regtable, /* user data = */ rd);
        if (status != 0) {
            PLUGIN_ERROR("ros_registration_table failed: %s", STRERROR(status));
            break;
        }

        status = ros_system_resource(rd->connection, handle_system_resource, /* user data = */ rd);
        if (status != 0) {
            PLUGIN_ERROR("ros_system_resource failed: %s", STRERROR(status));
            break;
        }

        status = ros_system_health(rd->connection, handle_system_health, /* user data = */ rd);
        if (status != 0) {
            PLUGIN_ERROR("ros_system_health failed: %s", STRERROR(status));
            break;
        }

        break;
    }

    if (status != 0) {
        ros_disconnect(rd->connection);
        rd->connection = NULL;
    }

    plugin_dispatch_metric_family_array_filtered(rd->fams, FAM_ROUTEROS_MAX, rd->filter, submit);

    return 0;
}

static void cr_free_data(void *ptr)
{
    if (ptr == NULL)
        return;

    cr_data_t *rd = ptr;

    ros_disconnect(rd->connection);
    rd->connection = NULL;

    free(rd->name);
    free(rd->node);
    free(rd->service);
    free(rd->username);
    free(rd->password);

    label_set_reset(&rd->labels);

    free(rd);
}

static int cr_config_router(config_item_t *ci)
{
    cr_data_t *router_data = calloc(1, sizeof(*router_data));
    if (router_data == NULL)
        return -1;

    int status = cf_util_get_string(ci, &router_data->name);
    if (status != 0) {
        cr_free_data(router_data);
        return status;
    }

    memcpy(router_data->fams, fams, sizeof(router_data->fams[0])*FAM_ROUTEROS_MAX);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &router_data->node);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_service(child, &router_data->service);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &router_data->username);
        } else if (strcasecmp("user-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &router_data->username);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &router_data->password);
        } else if (strcasecmp("password-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &router_data->password);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &router_data->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &router_data->filter);
        } else {
            PLUGIN_ERROR("Unknown config option `%s'.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status == 0) {
        if (router_data->node == NULL) {
            PLUGIN_ERROR("No `Host' option within a `Router' block. Where should I connect to?");
            status = -1;
        }

        if (router_data->password == NULL) {
            PLUGIN_ERROR("No `Password' option within a `Router' block. "
                         "How should I authenticate?");
            status = -1;
        }
    }

    if ((status == 0) && (router_data->username == NULL)) {
        router_data->username = sstrdup("admin");
        if (router_data->username == NULL) {
            PLUGIN_ERROR("sstrdup failed.");
            status = -1;
        }
    }

    if (status != 0) {
        cr_free_data(router_data);
        return status;
    }

    label_set_add(&router_data->labels, true, "instance", router_data->name);

    return plugin_register_complex_read("routeros", router_data->node, cr_read, interval,
                            &(user_data_t){.data=router_data, .free_func=cr_free_data});
}

static int cr_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = cr_config_router(child);
        } else {
            PLUGIN_ERROR("Unknown config option `%s'.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("routeros", cr_config);
}
