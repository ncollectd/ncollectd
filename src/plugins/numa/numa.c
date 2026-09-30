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

static int numa_read_node(int dir_fd, __attribute__((unused)) const char *path,
                          const char *entry, __attribute__((unused))  void *ud)
{
    if (strncmp(entry, "node", strlen("node")) != 0)
        return 0;

    const char *node = entry + strlen("node");

    if (!isdigit(*node))
        return 0;

    char path_numastat[PATH_MAX];
    ssnprintf(path_numastat, sizeof(path_numastat), "%s/numastat", entry);

    FILE *fh = fopenat(dir_fd, path_numastat, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Reading node %s failed: open(%s): %s", node, path_numastat, STRERRNO);
        return 0;
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[4];

        int status = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (status != 2) {
            PLUGIN_WARNING("Ignoring line with unexpected number of fields (node %s).", node);
            continue;
        }

        uint64_t value;
        if (strtouint(fields[1], &value) != 0)
            continue;

        if (!strcmp(fields[0], "numa_hit")) {
            metric_family_append(&fams[FAM_NUMA_HIT], VALUE_COUNTER(value), NULL,
                                 &LABEL_PAIR_CONST("node", node), NULL);
        } else if (!strcmp(fields[0], "numa_miss")) {
            metric_family_append(&fams[FAM_NUMA_MISS], VALUE_COUNTER(value), NULL,
                                 &LABEL_PAIR_CONST("node", node), NULL);
        } else if (!strcmp(fields[0], "numa_foreign")) {
            metric_family_append(&fams[FAM_NUMA_FOREIGN], VALUE_COUNTER(value), NULL,
                                 &LABEL_PAIR_CONST("node", node), NULL);
        } else if (!strcmp(fields[0], "local_node")) {
            metric_family_append(&fams[FAM_NUMA_LOCAL_NODE], VALUE_COUNTER(value), NULL,
                                 &LABEL_PAIR_CONST("node", node), NULL);
        } else if (!strcmp(fields[0], "other_node")) {
            metric_family_append(&fams[FAM_NUMA_OTHER_NODE], VALUE_COUNTER(value), NULL,
                                 &LABEL_PAIR_CONST("node", node), NULL);
        } else if (!strcmp(fields[0], "interleave_hit")) {
            metric_family_append(&fams[FAM_NUMA_INTERLEAVE_HIT], VALUE_COUNTER(value), NULL,
                                 &LABEL_PAIR_CONST("node", node), NULL);
        }
    }

    fclose(fh);

    return 0;
}

static int numa_read(void)
{
    walk_directory(path_sys_node, numa_read_node, NULL, 0);

    plugin_dispatch_metric_family_array(fams, FAM_NUMA_MAX, 0);

    return 0;
}

static int numa_init(void)
{
    path_sys_node = plugin_syspath("devices/system/node");
    if (path_sys_node == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

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
    plugin_register_read("numa", numa_read);
    plugin_register_shutdown("numa", numa_shutdown);
}
