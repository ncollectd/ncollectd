// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2011 Cyril Feraudet
// SPDX-FileCopyrightText: Copyright (C) 2012 Florian "octo" Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Cyril Feraudet <cyril at feraudet.com>
// SPDX-FileContributor: Florian "octo" Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

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

static char **interfaces;
static size_t interfaces_num;

static void ethstat_submit_value(const char *device, const char *name, uint64_t value)
{
    char fam_name[256];
    ssnprintf(fam_name, sizeof(fam_name), "system_ethstat_%s", name);

    metric_family_t fam = {
        .name = fam_name,
        .type = METRIC_TYPE_COUNTER,
    };

    metric_family_append(&fam, VALUE_COUNTER(value), NULL,
                         &(label_pair_const_t){.name="device", .value=device}, NULL);

    plugin_dispatch_metric_family(&fam, 0);
}

static int ethstat_read_interface(char *device)
{
    int fd = socket(AF_INET, SOCK_DGRAM, /* protocol = */ 0);
    if (fd < 0) {
        PLUGIN_ERROR("Failed to open control socket: %s", STRERRNO);
        return 1;
    }

    struct ethtool_drvinfo drvinfo = {.cmd = ETHTOOL_GDRVINFO};
    struct ifreq req = {.ifr_data = (void *)&drvinfo};

    sstrncpy(req.ifr_name, device, sizeof(req.ifr_name));

    int status = ioctl(fd, SIOCETHTOOL, &req);
    if (status < 0) {
        close(fd);
        PLUGIN_ERROR("Failed to get driver information from %s: %s", device, STRERRNO);
        return -1;
    }

    size_t n_stats = (size_t)drvinfo.n_stats;
    if (n_stats < 1) {
        close(fd);
        PLUGIN_ERROR("No stats available for %s", device);
        return -1;
    }

    size_t strings_size = sizeof(struct ethtool_gstrings) + (n_stats * ETH_GSTRING_LEN);
    size_t stats_size = sizeof(struct ethtool_stats) + (n_stats * sizeof(uint64_t));

    struct ethtool_gstrings *strings = malloc(strings_size);
    struct ethtool_stats *stats = malloc(stats_size);
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
        PLUGIN_ERROR("Cannot get strings from %s: %s", device, STRERRNO);
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
        PLUGIN_ERROR("Reading statistics from %s failed: %s", device, STRERRNO);
        return -1;
    }

    for (size_t i = 0; i < n_stats; i++) {
        char *stat_name;

        stat_name = (void *)&strings->data[i * ETH_GSTRING_LEN];
        /* Remove leading spaces in key name */
        while (isspace((int)*stat_name))
            stat_name++;

        PLUGIN_DEBUG("device = '%s': %s = %" PRIu64, device, stat_name, (uint64_t)stats->data[i]);
        ethstat_submit_value(device, stat_name, (uint64_t)stats->data[i]);
    }

    close(fd);
    free(strings);
    free(stats);

    return 0;
}

static int ethstat_read(void)
{
    for (size_t i = 0; i < interfaces_num; i++)
        ethstat_read_interface(interfaces[i]);

    return 0;
}

static int ethstat_add_interface(const config_item_t *ci)
{
    char **tmp = realloc(interfaces, sizeof(*interfaces) * (interfaces_num + 1));
    if (tmp == NULL)
        return -1;
    interfaces = tmp;
    interfaces[interfaces_num] = NULL;

    int status = cf_util_get_string(ci, interfaces + interfaces_num);
    if (status != 0)
        return status;

    interfaces_num++;
    PLUGIN_INFO("Registered interface %s", interfaces[interfaces_num - 1]);

    return 0;
}

static int ethstat_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("interface", child->key) == 0) {
            ethstat_add_interface(child);
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

    for (size_t i = 0; i < interfaces_num; i++)
        free(interfaces[i]);

    free(interfaces);
    return 0;
}

void module_register(void)
{
    plugin_register_config("ethstat", ethstat_config);
    plugin_register_read("ethstat", ethstat_read);
    plugin_register_shutdown("ethstat", ethstat_shutdown);
}
