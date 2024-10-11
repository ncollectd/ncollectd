// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

static char *path_sys_resctrl_mon_data;
static char *path_sys_resctrl_mon_groups;

enum {
    FAM_RESCTRL_LLC_OCCUPANCY_BYTES,
    FAM_RESCTRL_MEM_BANDWIDTH_LOCAL_BYTES,
    FAM_RESCTRL_MEM_BANDWIDTH_TOTAL_BYTES,
    FAM_RESCTRL_MAX,
};

static metric_family_t fams[FAM_RESCTRL_MAX] = {
    [FAM_RESCTRL_LLC_OCCUPANCY_BYTES] = {
        .name = "system_resctrl_llc_occupancy_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current snapshot of Last level cache occupancy of the corresponding domain.",
    },
    [FAM_RESCTRL_MEM_BANDWIDTH_LOCAL_BYTES] = {
        .name = "system_resctrl_mem_bandwidth_local_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Local memory bandwidth usage in bytes of the corresponding domain.",
    },
    [FAM_RESCTRL_MEM_BANDWIDTH_TOTAL_BYTES] = {
        .name = "system_resctrl_mem_bandwidth_total_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total memory bandwidth usage in bytes of the corresponding domain.",
    },
};

typedef struct {
    const char *domain;
    const char *group;
} resctrl_labels_t;

static int resctrl_read_domain(int dir_fd, __attribute__((unused)) const char *path,
                               const char *entry, void *ud)
{
    resctrl_labels_t *rl = ud;

    if (strcmp(entry, "llc_occupancy") == 0) {
        uint64_t value;
        int status = filetouint_at(dir_fd, "llc_occupancy", &value);
        if (likely(status == 0))
            metric_family_append(&fams[FAM_RESCTRL_LLC_OCCUPANCY_BYTES],
                                 VALUE_GAUGE(value), NULL,
                                 &(label_pair_const_t){.name="domain", .value=rl->domain},
                                 &(label_pair_const_t){.name="group", .value=rl->group}, NULL);
    } else if (strcmp(entry, "mbm_local_bytes") == 0) {
        uint64_t value;
        int status = filetouint_at(dir_fd, "mbm_local_bytes", &value);
        if (likely(status == 0))
            metric_family_append(&fams[FAM_RESCTRL_MEM_BANDWIDTH_LOCAL_BYTES],
                                 VALUE_COUNTER(value), NULL,
                                 &(label_pair_const_t){.name="domain", .value=rl->domain},
                                 &(label_pair_const_t){.name="group", .value=rl->group}, NULL);
    } else if (strcmp(entry, "mbm_total_bytes") == 0) {
        uint64_t value;
        int status = filetouint_at(dir_fd, "mbm_total_bytes", &value);
        if (likely(status == 0))
            metric_family_append(&fams[FAM_RESCTRL_MEM_BANDWIDTH_TOTAL_BYTES],
                                 VALUE_COUNTER(value), NULL,
                                 &(label_pair_const_t){.name="domain", .value=rl->domain},
                                 &(label_pair_const_t){.name="group", .value=rl->group}, NULL);
    }

    return 0;
}

static int resctrl_read_mon_data(int dir_fd, __attribute__((unused)) const char *path,
                                 const char *entry, void *ud)
{
    resctrl_labels_t *rl = ud;

    if (strncmp(entry, "mon_L3_", strlen("mon_L3_")) != 0)
        return 0;

    const char *domain = entry + strlen("mon_L3_");
    if ((domain[0] == '0') && (domain[1] != '\0'))
        domain++;

    rl->domain = domain;

    walk_directory_at(dir_fd, entry, resctrl_read_domain, rl, 0);

    return 0;
}

static int resctrl_read_mon_groups(int dir_fd, __attribute__((unused)) const char *path,
                                   const char *entry, __attribute__((unused)) void *ud)
{
    if (entry[0] == '.')
        return 0;

    resctrl_labels_t rl = {.domain = NULL, .group = entry};

    walk_directory_at(dir_fd, entry, resctrl_read_mon_data, &rl, 0);

    return 0;
}

static int resctrl_read(void)
{
    if (access(path_sys_resctrl_mon_data, R_OK|X_OK) == 0) {
        resctrl_labels_t rl = {.domain = NULL, .group = "global"};
        walk_directory(path_sys_resctrl_mon_data, resctrl_read_mon_data, &rl, 0);
    }

    if (access(path_sys_resctrl_mon_groups, R_OK|X_OK) == 0)
        walk_directory(path_sys_resctrl_mon_groups, resctrl_read_mon_groups, NULL, 0);

    plugin_dispatch_metric_family_array(fams, FAM_RESCTRL_MAX, 0);
    return 0;
}

static int resctrl_init(void)
{
    path_sys_resctrl_mon_data = plugin_syspath("fs/resctrl/mon_data");
    if (path_sys_resctrl_mon_data == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    path_sys_resctrl_mon_groups = plugin_syspath("fs/resctrl/mon_groups");
    if (path_sys_resctrl_mon_groups == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    return 0;
}

static int resctrl_shutdown(void)
{
    free(path_sys_resctrl_mon_data);
    free(path_sys_resctrl_mon_groups);
    return 0;
}

void module_register(void)
{
    plugin_register_init("resctrl", resctrl_init);
    plugin_register_read("resctrl", resctrl_read);
    plugin_register_shutdown("resctrl", resctrl_shutdown);
}
