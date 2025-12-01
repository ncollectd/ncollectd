// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2011 Cyril Feraudet
// SPDX-FileCopyrightText: Copyright (C) 2012 Florian "octo" Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Cyril Feraudet <cyril at feraudet.com>
// SPDX-FileContributor: Florian "octo" Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/time.h"

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif
#ifdef HAVE_LINUX_SOCKIOS_H
#include <linux/sockios.h>
#endif
#ifdef HAVE_LINUX_ETHTOOL_H
#include <linux/ethtool.h>
#endif

typedef struct {
    char *device;
    plugin_filter_t *filter;
} interface_t;

static interface_t *interfaces;
static size_t interfaces_num;

static void ethstat_submit_value(interface_t *interface, const char *name, uint64_t value,
                                                         cdtime_t ts)
{
    char fam_name[ETH_GSTRING_LEN + 32];
    ssnprintf(fam_name, sizeof(fam_name), "system_ethstat_%s", name);

    metric_family_t fam = {
        .name = fam_name,
        .type = METRIC_TYPE_COUNTER,
    };

    metric_family_append(&fam, VALUE_COUNTER(value), NULL,
                         &(label_pair_const_t){.name="device", .value=interface->device}, NULL);

    plugin_dispatch_metric_family_filtered(&fam, interface->filter, ts);
}

static int ethstat_read_interface(interface_t *interface)
{
    int fd = socket(AF_INET, SOCK_DGRAM, /* protocol = */ 0);
    if (fd < 0) {
        PLUGIN_ERROR("Failed to open control socket: %s", STRERRNO);
        return 1;
    }

    struct ethtool_drvinfo drvinfo = {.cmd = ETHTOOL_GDRVINFO};
    struct ifreq req = {.ifr_data = (void *)&drvinfo};

    sstrncpy(req.ifr_name, interface->device, sizeof(req.ifr_name));

    int status = ioctl(fd, SIOCETHTOOL, &req);
    if (status < 0) {
        close(fd);
        PLUGIN_ERROR("Failed to get driver information from %s: %s", interface->device, STRERRNO);
        return -1;
    }

    size_t n_stats = (size_t)drvinfo.n_stats;
    if (n_stats < 1) {
        close(fd);
        PLUGIN_ERROR("No stats available for %s", interface->device);
        return -1;
    }

    size_t strings_size = sizeof(struct ethtool_gstrings) + (n_stats * ETH_GSTRING_LEN);
    size_t stats_size = sizeof(struct ethtool_stats) + (n_stats * sizeof(uint64_t));

    struct ethtool_gstrings *strings = calloc(1, strings_size);
    struct ethtool_stats *stats = calloc(1, stats_size);
    if ((strings == NULL) || (stats == NULL)) {
        close(fd);
        free(strings);
        free(stats);
        PLUGIN_ERROR("malloc failed.");
        return -1;
    }

    strings->cmd = ETHTOOL_GSTRINGS;
    strings->string_set = ETH_SS_STATS;
    strings->len = n_stats;
    req.ifr_data = (void *)strings;
    status = ioctl(fd, SIOCETHTOOL, &req);
    if (status < 0) {
        close(fd);
        free(strings);
        free(stats);
        PLUGIN_ERROR("Cannot get strings from %s: %s", interface->device, STRERRNO);
        return -1;
    }

    stats->cmd = ETHTOOL_GSTATS;
    stats->n_stats = n_stats;
    req.ifr_data = (void *)stats;
    status = ioctl(fd, SIOCETHTOOL, &req);
    if (status < 0) {
        close(fd);
        free(strings);
        free(stats);
        PLUGIN_ERROR("Reading statistics from %s failed: %s", interface->device, STRERRNO);
        return -1;
    }

    cdtime_t ts = cdtime();

    for (size_t i = 0; i < n_stats; i++) {
        char *stat_name = (char *)&strings->data[i * ETH_GSTRING_LEN];

        stat_name[ETH_GSTRING_LEN-1] = '\0';
        while (isspace((int)*stat_name))
            stat_name++;

        PLUGIN_DEBUG("device = '%s': %s = %" PRIu64, interface->device, stat_name,
                                                     (uint64_t)stats->data[i]);
        ethstat_submit_value(interface, stat_name, (uint64_t)stats->data[i], ts);
    }

    close(fd);
    free(strings);
    free(stats);

    return 0;
}

static int ethstat_read(void)
{
    for (size_t i = 0; i < interfaces_num; i++)
        ethstat_read_interface(&interfaces[i]);

    return 0;
}

static int ethstat_config_interface(const config_item_t *ci)
{
    interface_t *tmp = realloc(interfaces, sizeof(*interfaces) * (interfaces_num + 1));
    if (tmp == NULL)
        return -1;
    interfaces = tmp;
    interface_t *interface = &interfaces[interfaces_num];
    interface->device = NULL;
    interface->filter = NULL;

    int status = cf_util_get_string(ci, &interface->device);
    if (status != 0)
        return status;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &interface->filter);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        free(interface->device);
        plugin_filter_free(interface->filter);
        return -1;
    }

    interfaces_num++;

    PLUGIN_INFO("Registered interface %s", interface->device);

    return 0;
}

static int ethstat_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("interface", child->key) == 0) {
            ethstat_config_interface(child);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int ethstat_shutdown(void)
{
    if (interfaces == NULL)
        return 0;

    for (size_t i = 0; i < interfaces_num; i++) {
        free(interfaces[i].device);
        plugin_filter_free(interfaces[i].filter);
    }

    free(interfaces);
    return 0;
}

void module_register(void)
{
    plugin_register_config("ethstat", ethstat_config);
    plugin_register_read("ethstat", ethstat_read);
    plugin_register_shutdown("ethstat", ethstat_shutdown);
}
