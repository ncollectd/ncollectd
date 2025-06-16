// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright(c) 2016 Intel Corporation. All rights reserved.
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Jaroslav Safka <jaroslavx.safka at intel.com>
// SPDX-FileContributor: Kim-Marie Jones <kim-marie.jones at intel.com>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

static bool report_numa = true;
static bool report_mm = true;

static bool values_pages = true;
static bool values_bytes = false;

static char *path_sys_mm_hugepages;
static char *path_sys_node;

enum {
    FAM_HUGEPAGES_NR,
    FAM_HUGEPAGES_FREE,
    FAM_HUGEPAGES_RESERVED,
    FAM_HUGEPAGES_SURPLUS,
    FAM_HUGEPAGES_NR_BYTES,
    FAM_HUGEPAGES_FREE_BYTES,
    FAM_HUGEPAGES_RESERVED_BYTES,
    FAM_HUGEPAGES_SURPLUS_BYTES,
    FAM_HUGEPAGES_NODE_NR,
    FAM_HUGEPAGES_NODE_FREE,
    FAM_HUGEPAGES_NODE_SURPLUS,
    FAM_HUGEPAGES_NODE_NR_BYTES,
    FAM_HUGEPAGES_NODE_FREE_BYTES,
    FAM_HUGEPAGES_NODE_SURPLUS_BYTES,
    FAM_HUGEPAGES_MAX,
};

static metric_family_t fams[FAM_HUGEPAGES_MAX] = {
    [FAM_HUGEPAGES_NR] = {
        .name = "system_hugepages_nr",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current number of \"persistent\" huge pages in the kernel's huge page pool."
    },
    [FAM_HUGEPAGES_FREE] = {
        .name = "system_hugepages_free",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of huge pages in the pool that are not yet allocated."
    },
    [FAM_HUGEPAGES_RESERVED] = {
        .name = "system_hugepages_reserved",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of huge pages for which a commitment to allocate from the pool "
                "has been made, but no allocation has yet been made."
    },
    [FAM_HUGEPAGES_SURPLUS] = {
        .name = "system_hugepages_surplus",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of huge pages in the pool above the value in /proc/sys/vm/nr_hugepages."
    },
    [FAM_HUGEPAGES_NR_BYTES] = {
        .name = "system_hugepages_nr_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current size in bytes of \"persistent\" huge pages in the kernel's "
                "huge page pool."
    },
    [FAM_HUGEPAGES_FREE_BYTES] = {
        .name = "system_hugepages_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The size in bytes of huge pages in the pool that are not yet allocated."
    },
    [FAM_HUGEPAGES_RESERVED_BYTES] = {
        .name = "system_hugepages_reserved_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The size in bytes of huge pages for which a commitment to allocate "
                "from the pool has been made, but no allocation has yet been made."
    },
    [FAM_HUGEPAGES_SURPLUS_BYTES] = {
        .name = "system_hugepages_surplus_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The size in bytes of huge pages in the pool above the value in "
                "/proc/sys/vm/nr_hugepages."
    },
    [FAM_HUGEPAGES_NODE_NR] = {
        .name = "system_hugepages_node_nr",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current number of \"persistent\" huge pages in the kernel's huge page pool."
    },
    [FAM_HUGEPAGES_NODE_FREE] = {
        .name = "system_hugepages_node_free",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of huge pages in the pool that are not yet allocated."
    },
    [FAM_HUGEPAGES_NODE_SURPLUS] = {
        .name = "system_hugepages_node_surplus",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of huge pages in the pool above the value in "
                "/proc/sys/vm/nr_hugepages."
    },
    [FAM_HUGEPAGES_NODE_NR_BYTES] = {
        .name = "system_hugepages_node_nr_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current size in bytes of \"persistent\" huge pages in "
                "the kernel's huge page pool"
    },
    [FAM_HUGEPAGES_NODE_FREE_BYTES] = {
        .name = "system_hugepages_node_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The size in bytes of huge pages in the pool that are not yet allocated"
    },
    [FAM_HUGEPAGES_NODE_SURPLUS_BYTES] = {
        .name = "system_hugepages_node_surplus_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The size in bytes of huge pages in the pool above the value in "
                "/proc/sys/vm/nr_hugepages"
    },
};

static int hp_read_hugepages(__attribute__((unused)) int dirfd, const char *path,
                             const char *entry, void *ud)
{
    if (strncmp("hugepages-", entry, strlen("hugepages-")) != 0)
        return 0;

    char *node = ud;

    const char *hpage = entry + strlen("hugepages-");
    long hpage_size = strtol(hpage, NULL, 10);

    double value = 0;

    char hpath[PATH_MAX];
    ssnprintf(hpath, sizeof(hpath), "%s/%s/nr_hugepages", path, entry);
    if (parse_double_file(hpath, &value) == 0) {
        if (node == NULL) {
            if (values_pages)
                metric_family_append(&fams[FAM_HUGEPAGES_NR], VALUE_GAUGE(value), NULL,
                                     &(label_pair_const_t){.name = "page_size", .value = hpage},
                                     NULL);

            if (values_bytes) {
                value *= hpage_size * 1024;
                metric_family_append(&fams[FAM_HUGEPAGES_NR_BYTES], VALUE_GAUGE(value), NULL,
                                     &(label_pair_const_t){.name = "page_size", .value = hpage},
                                     NULL);
            }
        } else {
            if (values_pages)
                metric_family_append(&fams[FAM_HUGEPAGES_NODE_NR], VALUE_GAUGE(value), NULL,
                                     &(label_pair_const_t){.name = "page_size", .value = hpage},
                                     &(label_pair_const_t){.name = "node", .value = node}, NULL);
            if (values_bytes) {
                value *= hpage_size * 1024;
                metric_family_append(&fams[FAM_HUGEPAGES_NODE_NR_BYTES], VALUE_GAUGE(value), NULL,
                                     &(label_pair_const_t){.name = "page_size", .value = hpage},
                                     &(label_pair_const_t){.name = "node", .value = node}, NULL);
            }
        }
    }

    ssnprintf(hpath, sizeof(hpath), "%s/%s/free_hugepages", path, entry);
    if (parse_double_file(hpath, &value) == 0) {
        if (node == NULL) {
            if (values_pages)
                metric_family_append(&fams[FAM_HUGEPAGES_FREE], VALUE_GAUGE(value), NULL,
                                     &(label_pair_const_t){.name = "page_size", .value = hpage},
                                     NULL);
            if (values_bytes) {
                value *= hpage_size * 1024;
                metric_family_append(&fams[FAM_HUGEPAGES_FREE_BYTES], VALUE_GAUGE(value), NULL,
                                     &(label_pair_const_t){.name = "page_size", .value = hpage},
                                     NULL);
            }
        } else {
            if (values_pages)
                metric_family_append(&fams[FAM_HUGEPAGES_NODE_FREE], VALUE_GAUGE(value), NULL,
                                     &(label_pair_const_t){.name = "page_size", .value = hpage},
                                     &(label_pair_const_t){.name = "node", .value = node}, NULL);
            if (values_bytes) {
                value *= hpage_size * 1024;
                metric_family_append(&fams[FAM_HUGEPAGES_NODE_FREE_BYTES], VALUE_GAUGE(value), NULL,
                                     &(label_pair_const_t){.name = "page_size", .value = hpage},
                                     &(label_pair_const_t){.name = "node", .value = node}, NULL);
            }
        }
    }

    ssnprintf(hpath, sizeof(hpath), "%s/%s/surplus_hugepages", path, entry);
    if (parse_double_file(hpath, &value) == 0) {
        if (node == NULL) {
            if (values_pages)
                metric_family_append(&fams[FAM_HUGEPAGES_SURPLUS], VALUE_GAUGE(value), NULL,
                                     &(label_pair_const_t){.name = "page_size", .value = hpage},
                                     NULL);
            if (values_bytes) {
                value *= hpage_size * 1024;
                metric_family_append(&fams[FAM_HUGEPAGES_SURPLUS_BYTES], VALUE_GAUGE(value), NULL,
                                     &(label_pair_const_t){.name = "page_size", .value = hpage},
                                     NULL);
            }
        } else {
            if (values_pages)
                metric_family_append(&fams[FAM_HUGEPAGES_NODE_SURPLUS], VALUE_GAUGE(value), NULL,
                                     &(label_pair_const_t){.name = "page_size", .value = hpage},
                                     &(label_pair_const_t){.name = "node", .value = node}, NULL);
            if (values_bytes) {
                value *= hpage_size * 1024;
                metric_family_append(&fams[FAM_HUGEPAGES_NODE_SURPLUS_BYTES], VALUE_GAUGE(value),
                                     NULL,
                                     &(label_pair_const_t){.name = "page_size", .value = hpage},
                                     &(label_pair_const_t){.name = "node", .value = node}, NULL);
            }
        }
    }

    if (node == NULL) {
        ssnprintf(hpath, sizeof(hpath), "%s/%s/resv_hugepages", path, entry);
        if (parse_double_file(hpath, &value) == 0) {
            if (values_pages)
                metric_family_append(&fams[FAM_HUGEPAGES_RESERVED], VALUE_GAUGE(value), NULL,
                                     &(label_pair_const_t){.name = "page_size", .value = hpage},
                                     NULL);
            if (values_bytes) {
                value *= hpage_size * 1024;
                metric_family_append(&fams[FAM_HUGEPAGES_RESERVED_BYTES], VALUE_GAUGE(value), NULL,
                                     &(label_pair_const_t){.name = "page_size", .value = hpage},
                                     NULL);
            }
        }
    }

    return 0;
}

static int hp_read_node(__attribute__((unused)) int dirfd,
                        const char *path, const char *entry,
                        __attribute__((unused)) void *ud)
{
    if (strncmp(entry, "node", strlen("node")) != 0)
        return 0;
    const char *node = entry + strlen("node");

    char npath[PATH_MAX];
    ssnprintf(npath, sizeof(npath), "%s/%s/hugepages", path, entry);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
    return walk_directory(npath, hp_read_hugepages, node, 0);
#pragma GCC diagnostic pop
}

static int hp_read(void)
{
    if (report_mm)
        walk_directory(path_sys_mm_hugepages, hp_read_hugepages, NULL, 0);

    if (report_numa)
        walk_directory(path_sys_node, hp_read_node, NULL, 0);

    plugin_dispatch_metric_family_array(fams, FAM_HUGEPAGES_MAX, 0);
    return 0;
}

static int hp_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("report-per-node-hp", child->key) == 0) {
            status = cf_util_get_boolean(child, &report_numa);
        } else if (strcasecmp("report-root-hp", child->key) == 0) {
            status = cf_util_get_boolean(child, &report_mm);
        } else if (strcasecmp("values-pages", child->key) == 0) {
            status = cf_util_get_boolean(child, &values_pages);
        } else if (strcasecmp("values-bytes", child->key) == 0) {
            status = cf_util_get_boolean(child, &values_bytes);
        } else {
            PLUGIN_ERROR("Invalid configuration option: '%s'.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int hp_init(void)
{
    path_sys_mm_hugepages = plugin_syspath("kernel/mm/hugepages");
    if (path_sys_mm_hugepages == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    path_sys_node = plugin_syspath("devices/system/node");
    if (path_sys_node == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    return 0;
}

static int hp_shutdown(void)
{
    free(path_sys_mm_hugepages);
    free(path_sys_node);
    return 0;
}

void module_register(void)
{
    plugin_register_init("hugepages", hp_init);
    plugin_register_config("hugepages", hp_config);
    plugin_register_read("hugepages", hp_read);
    plugin_register_shutdown("hugepages", hp_shutdown);
}
