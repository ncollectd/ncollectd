// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

static char *path_sys_net;

enum {
    FAM_BONDING_SLAVES,
    FAM_BONDING_ACTIVE,
    FAM_BONDING_MAX,
};

static metric_family_t fams[FAM_BONDING_MAX] = {
    [FAM_BONDING_SLAVES] = {
        .name = "system_bonding_slaves",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of configured slaves per bonding interface.",
    },
    [FAM_BONDING_ACTIVE] = {
        .name = "system_bonding_active",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of active slaves per bonding interface.",
    },
};

static int bonding_read(void)
{
    char path[PATH_MAX];
    ssnprintf(path, sizeof(path), "%s/bonding_masters", path_sys_net);

    char masters[4097];
    ssize_t size = read_file(path, masters, sizeof(masters));
    if (size <= 0) {
        PLUGIN_ERROR("Cannot read \"%s\".", path);
        return -1;
    }

    char *master_ptr = masters;
    char *master_saveptr = NULL;
    char *master;
    while ((master = strtok_r(master_ptr, " \n", &master_saveptr)) != NULL) {
        master_ptr =NULL;
        ssnprintf(path, sizeof(path), "%s/%s/bonding/slaves", path_sys_net, master);

        char slaves[4097];
        size = read_file(path, slaves, sizeof(slaves));
        if (size <= 0) {
            PLUGIN_ERROR("Cannot read \"%s\".", path);
            continue;
        }

        uint64_t bonding_slaves = 0;
        uint64_t bonding_active = 0;

        char *slave_ptr = slaves;
        char *slave_saveptr = NULL;
        char *slave;
        while ((slave = strtok_r(slave_ptr, " \n", &slave_saveptr)) != NULL) {
            slave_ptr = NULL;
            ssnprintf(path, sizeof(path), "%s/%s/lower_%s/bonding_slave/mii_status",
                                          path_sys_net, master, slave);
            char mii_status[64];
            size = read_file(path, mii_status, sizeof(mii_status));
            if (size <= 0) {
                ssnprintf(path, sizeof(path), "%s/%s/slave_%s/bonding_slave/mii_status",
                                               path_sys_net, master, slave);
                size = read_file(path, mii_status, sizeof(mii_status));
                if (size <= 0) {
                    PLUGIN_ERROR("Cannot read \"%s\".", path);
                    continue;
                }
            }

            bonding_slaves++;
            if (strcmp(strntrim(mii_status, (size_t)size), "up") == 0)
                bonding_active++;
        }
        metric_family_append(&fams[FAM_BONDING_SLAVES], VALUE_GAUGE(bonding_slaves), NULL,
                             &(label_pair_const_t){.name="master", .value=master}, NULL);
        metric_family_append(&fams[FAM_BONDING_ACTIVE], VALUE_GAUGE(bonding_active), NULL,
                             &(label_pair_const_t){.name="master", .value=master}, NULL);
        master_ptr = NULL;
    }

    plugin_dispatch_metric_family_array(fams, FAM_BONDING_MAX, 0);
    return 0;
}

static int bonding_init(void)
{
    path_sys_net = plugin_syspath("class/net");
    if (path_sys_net == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    return 0;
}

static int bonding_shutdown(void)
{
    free(path_sys_net);
    return 0;
}

void module_register(void)
{
    plugin_register_init("bonding", bonding_init);
    plugin_register_read("bonding", bonding_read);
    plugin_register_shutdown("bonding", bonding_shutdown);
}
