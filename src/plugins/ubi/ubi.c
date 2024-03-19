// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

static char *path_sys_class_ubi;

enum {
    FAM_UBI_BAD_PHYSICAL_ERASEBLOCKS,
    FAM_UBI_MAXIMUM_PHYSICAL_ERASEBLOCKS,
    FAM_UBI_MAX,
};

static metric_family_t fams[FAM_UBI_MAX] = {
    [FAM_UBI_BAD_PHYSICAL_ERASEBLOCKS] = {
        .name = "system_ubi_bad_physical_eraseblocks",
        .type = METRIC_TYPE_GAUGE,
        .help = "Count of bad physical eraseblocks on the underlying MTD device.",
    },
    [FAM_UBI_MAXIMUM_PHYSICAL_ERASEBLOCKS] = {
        .name = "system_ubi_maximum_physical_eraseblocks",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum physical eraseblock erase counter value.",
    },
};

static exclist_t excl_device;

static int ubi_read_device(int dirfd, __attribute__((unused)) const char *path,
                           const char *entry, __attribute__((unused)) void *ud)
{
    if (!exclist_match(&excl_device, entry))
        return 0;

    int devfd = openat(dirfd, entry, O_RDONLY | O_DIRECTORY);
    if (unlikely(devfd < 0))
        return 0;

    uint64_t value = 0;

    int status = filetouint_at(devfd, "bad_peb_count", &value);
    if (unlikely(status != 0)) {
        PLUGIN_ERROR("Did not find an integer in bad_peb_count");
    } else {
        metric_family_append(&fams[FAM_UBI_BAD_PHYSICAL_ERASEBLOCKS], VALUE_GAUGE(value), NULL,
                             &(label_pair_const_t){.name="device", .value="entry"}, NULL);
    }

    status = filetouint_at(devfd, "max_ec", &value);
    if (unlikely(status != 0)) {
        PLUGIN_ERROR("Did not find an integer in max_ec");
    } else {
        metric_family_append(&fams[FAM_UBI_MAXIMUM_PHYSICAL_ERASEBLOCKS], VALUE_GAUGE(value), NULL,
                             &(label_pair_const_t){.name="device", .value="entry"}, NULL);
    }

    close(devfd);

    return 0;
}

static int ubi_read(void)
{
    walk_directory(path_sys_class_ubi, ubi_read_device, NULL, 0);

    plugin_dispatch_metric_family_array(fams, FAM_UBI_MAX, 0);
    return 0;
}

static int ubi_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "device") == 0) {
            status = cf_util_exclist(child, &excl_device);
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

static int ubi_init(void)
{
    path_sys_class_ubi = plugin_procpath("class/ubi");
    if (path_sys_class_ubi == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    return 0;
}

static int ubi_shutdown(void)
{
    free(path_sys_class_ubi);
    exclist_reset(&excl_device);
    return 0;
}

void module_register(void)
{
    plugin_register_init("ubi", ubi_init);
    plugin_register_config("ubi", ubi_config);
    plugin_register_read("ubi", ubi_read);
    plugin_register_shutdown("ubi", ubi_shutdown);
}
