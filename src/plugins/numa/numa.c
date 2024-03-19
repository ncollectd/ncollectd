// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2012 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

static char *path_sys_node;

static int max_node = -1;

enum {
    FAM_NUMA_HIT,
    FAM_NUMA_MISS,
    FAM_NUMA_FOREIGN,
    FAM_NUMA_LOCAL_NODE,
    FAM_NUMA_OTHER_NODE,
    FAM_NUMA_INTERLEAVE_HIT,
    FAM_NUMA_MAX
};

static metric_family_t fams[FAM_NUMA_MAX] = {
    [FAM_NUMA_HIT] = {
        .name = "system_numa_hit",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of pages that were successfully allocated to this node.",
    },
    [FAM_NUMA_MISS] = {
        .name = "system_numa_miss",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of pages that were allocated on this node "
                "because of low memory on the intended node.",
    },
    [FAM_NUMA_FOREIGN] = {
        .name = "system_numa_foreign",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of pages initially intended for this node "
                "that were allocated to another node instead.",
    },
    [FAM_NUMA_LOCAL_NODE] = {
        .name = "system_numa_local_node",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of pages successfully allocated on this node, "
                "by a process on this node.",
    },
    [FAM_NUMA_OTHER_NODE] = {
        .name = "system_numa_other_node",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of pages allocated on this node, by a process on another node.",
    },
    [FAM_NUMA_INTERLEAVE_HIT] = {
        .name = "system_numa_interleave_hit",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of interleave policy pages successfully allocated to this node.",
    },
};

static int numa_read_node(int node)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/node%i/numastat", path_sys_node, node);

    FILE *fh = fopen(path, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Reading node %i failed: open(%s): %s", node, path, STRERRNO);
        return -1;
    }

    char buffer[128];
    int success = 0;
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[4];

        int status = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (status != 2) {
            PLUGIN_WARNING("Ignoring line with unexpected number of fields (node %i).", node);
            continue;
        }

        uint64_t v;
        status = parse_uinteger(fields[1], &v);
        if (status != 0)
            continue;

        value_t value = VALUE_COUNTER(v);

        char node_buffer[21];
        snprintf(node_buffer, sizeof(node_buffer), "%i", node);

        if (!strcmp(fields[0], "numa_hit")) {
            metric_family_append(&fams[FAM_NUMA_HIT], value, NULL,
                                 &(label_pair_const_t){.name="node", .value=node_buffer}, NULL);
            success++;
        } else if (!strcmp(fields[0], "numa_miss")) {
            metric_family_append(&fams[FAM_NUMA_MISS], value, NULL,
                                 &(label_pair_const_t){.name="node", .value=node_buffer}, NULL);
            success++;
        } else if (!strcmp(fields[0], "numa_foreign")) {
            metric_family_append(&fams[FAM_NUMA_FOREIGN], value, NULL,
                                 &(label_pair_const_t){.name="node", .value=node_buffer}, NULL);
            success++;
        } else if (!strcmp(fields[0], "local_node")) {
            metric_family_append(&fams[FAM_NUMA_LOCAL_NODE], value, NULL,
                                 &(label_pair_const_t){.name="node", .value=node_buffer}, NULL);
            success++;
        } else if (!strcmp(fields[0], "other_node")) {
            metric_family_append(&fams[FAM_NUMA_OTHER_NODE], value, NULL,
                                 &(label_pair_const_t){.name="node", .value=node_buffer}, NULL);
            success++;
        } else if (!strcmp(fields[0], "interleave_hit")) {
            metric_family_append(&fams[FAM_NUMA_INTERLEAVE_HIT], value, NULL,
                                 &(label_pair_const_t){.name="node", .value=node_buffer}, NULL);
            success++;
        }
    }

    fclose(fh);
    return success ? 0 : -1;
}

static int numa_read(void)
{
    if (max_node < 0) {
        PLUGIN_WARNING("No NUMA nodes were detected.");
        return -1;
    }

    int success = 0;
    for (int i = 0; i <= max_node; i++) {
        int status = numa_read_node(i);
        if (status == 0)
            success++;
    }

    plugin_dispatch_metric_family_array(fams, FAM_NUMA_MAX, 0);

    return success ? 0 : -1;
}

static int numa_init(void)
{
    path_sys_node = plugin_syspath("devices/system/node");
    if (path_sys_node == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    /* Determine the number of nodes on this machine. */
    while (true) {
        char path[PATH_MAX];
        struct stat statbuf = {0};
        int status;

        snprintf(path, sizeof(path), "%s/node%i", path_sys_node, max_node + 1);

        status = stat(path, &statbuf);
        if (status == 0) {
            max_node++;
            continue;
        } else if (errno == ENOENT) {
            break;
        } else { /* ((status != 0) && (errno != ENOENT)) */
            PLUGIN_ERROR("stat(%s) failed: %s", path, STRERRNO);
            return -1;
        }
    }

    PLUGIN_DEBUG("Found %i nodes.", max_node + 1);
    return 0;
}

static int numa_shutdown(void)
{
    free(path_sys_node);
    return 0;
}

void module_register(void)
{
    plugin_register_init("numa", numa_init);
    plugin_register_init("numa", numa_init);
    plugin_register_read("numa", numa_read);
    plugin_register_shutdown("numa", numa_shutdown);
}
