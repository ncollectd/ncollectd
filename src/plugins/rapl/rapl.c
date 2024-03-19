// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

enum {
    FAM_RAPL_ENERGY,
    FAM_RAPL_MAX,
};

static metric_family_t fams[FAM_RAPL_MAX] = {
    [FAM_RAPL_ENERGY] = {
        .name = "system_rapl_energy_joules",
        .type = METRIC_TYPE_COUNTER,
        .help = "Current energy counter in joules",
    },
};

static char *path_sys_rapl;

static int rapl_read_dir(int dir_fd, __attribute__((unused)) const char *path,
                         const char *entry, __attribute__((unused))  void *ud)
{
    if (strncmp(entry, "intel-rapl:", strlen("intel-rapl:")) != 0)
        return 0;

    int dir_rapl_fd = openat(dir_fd, entry, O_RDONLY | O_DIRECTORY);
    if (unlikely(dir_rapl_fd < 0))
        return 0;

    if (faccessat(dir_rapl_fd, "name", R_OK, 0) != 0) {
        close(dir_rapl_fd);
        return 0;
    }

    char domain[256];
    ssize_t status = read_file_at(dir_rapl_fd, "name", domain, sizeof(domain));
    if (status < 0) {
        close(dir_rapl_fd);
        return 0;
    }
    strstripnewline(domain);

    uint64_t energy = 0;
    status = filetouint_at(dir_rapl_fd, "energy_uj", &energy);
    if (status != 0) {
        close(dir_rapl_fd);
        return 0;
    }

    close(dir_rapl_fd);

    metric_family_append(&fams[FAM_RAPL_ENERGY],
                         VALUE_COUNTER_FLOAT64(((double)energy)/1000000.0), NULL,
                         &(label_pair_const_t){.name="domain", .value=domain},
                         &(label_pair_const_t){.name="zone", .value=entry + strlen("intel-rapl:")},
                         NULL);

    return 0;
}

static int rapl_read(void)
{
    walk_directory(path_sys_rapl, rapl_read_dir, NULL, 0);

    plugin_dispatch_metric_family_array(fams, FAM_RAPL_MAX, 0);

    return 0;
}

static int rapl_init(void)
{
    path_sys_rapl = plugin_syspath("class/powercap");
    if (path_sys_rapl == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    return 0;
}

static int rapl_shutdown(void)
{
    free(path_sys_rapl);

    return 0;
}

void module_register(void)
{
    plugin_register_init("rapl", rapl_init);
    plugin_register_shutdown("rapl", rapl_shutdown);
    plugin_register_read("rapl", rapl_read);
}
