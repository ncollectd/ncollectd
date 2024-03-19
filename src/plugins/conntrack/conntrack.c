// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2009 Tomasz Pala
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Tomasz Pala <gotar at pld-linux.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

// based on entropy.c by:
//   Florian octo Forster <octo at collectd.org>

#include "plugin.h"
#include "libutils/common.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

enum {
    FAM_CONNTRACK_USED,
    FAM_CONNTRACK_MAX,
    FAM_CONNTRACK_NF,
};

static metric_family_t fams[FAM_CONNTRACK_NF] = {
    [FAM_CONNTRACK_USED] = {
        .name = "system_conntrack_used",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of currently allocated flow entries.",
    },
    [FAM_CONNTRACK_MAX] = {
        .name = "system_conntrack_max",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size of connection tracking table.",
    },
};

static char *conntrack_path;
static char *conntrack_file;
static char *conntrack_old_file;

static char *conntrack_max_path;
static char *conntrack_max_file;
static char *conntrack_max_old_file;

static int conntrack_read(void)
{
    if (conntrack_path == NULL) {
        if (access(conntrack_file, R_OK) == 0) {
            conntrack_path = conntrack_file;
        } else if (access(conntrack_old_file, R_OK) == 0) {
            conntrack_path = conntrack_old_file;
        } else {
            PLUGIN_ERROR("Not found nf_conntrack_count or ip_conntrack_count");
            return -1;
        }
    }

    double conntrack = 0;
    if (parse_double_file(conntrack_path, &conntrack) != 0) {
        PLUGIN_ERROR("Reading \"%s\" failed.", conntrack_path);
        return -1;
    }

    if (conntrack_max_path == NULL) {
        if (access(conntrack_max_file, R_OK) == 0) {
            conntrack_max_path = conntrack_max_file;
        } else if (access(conntrack_max_old_file, R_OK) == 0) {
            conntrack_max_path = conntrack_max_old_file;
        } else {
            PLUGIN_ERROR("Not found nf_conntrack_max or ip_conntrack_max");
            return -1;
        }
    }

    double conntrack_max = 0;
    if (parse_double_file(conntrack_max_path, &conntrack_max) != 0) {
        PLUGIN_ERROR("Reading \"%s\" failed.", conntrack_max_path);
        return -1;
    }

    metric_family_append(&fams[FAM_CONNTRACK_USED], VALUE_GAUGE(conntrack), NULL, NULL);
    metric_family_append(&fams[FAM_CONNTRACK_MAX], VALUE_GAUGE(conntrack_max), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_CONNTRACK_NF, 0);
    return 0;
}

static int conntrack_init(void)
{
    conntrack_file = plugin_procpath("sys/net/netfilter/nf_conntrack_count");
    if (conntrack_file == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    conntrack_old_file = plugin_procpath("sys/net/ipv4/netfilter/ip_conntrack_count");
    if (conntrack_old_file == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    conntrack_max_file = plugin_procpath("sys/net/netfilter/nf_conntrack_max");
    if (conntrack_max_file == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    conntrack_max_old_file = plugin_procpath("sys/net/ipv4/netfilter/ip_conntrack_max");
    if (conntrack_max_old_file == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int conntrack_shutdown(void)
{
    free(conntrack_file);
    free(conntrack_old_file);
    free(conntrack_max_file);
    free(conntrack_max_old_file);
    return 0;
}

void module_register(void)
{
    plugin_register_init("conntrack", conntrack_init);
    plugin_register_read("conntrack", conntrack_read);
    plugin_register_shutdown("conntrack", conntrack_shutdown);
}
