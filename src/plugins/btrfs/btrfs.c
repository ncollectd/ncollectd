// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2018 Martin Langlotz
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Martin Langlotz <stackshadow at evilbrain.de>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/llist.h"
#include "libutils/mount.h"

#include <btrfs/ioctl.h>

static bool btrfs_is_init;
static bool btrfs_conf_refresh_always;
static llist_t *llist_btrfs_paths;

static int btrfs_init(void);

enum {
    FAM_BTRFS_WRITE_ERRORS,
    FAM_BTRFS_READ_ERRORS,
    FAM_BTRFS_FLUSH_ERRORS,
    FAM_BTRFS_CORRUPTION_ERRORS,
    FAM_BTRFS_GENERATION_ERRORS,
    FAM_BTRFS_MAX,
};

static metric_family_t fams[FAM_BTRFS_MAX] = {
    [FAM_BTRFS_WRITE_ERRORS] = {
        .name = "system_brtfs_write_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "EIO or EREMOTEIO write errors from lower layers.",
    },
    [FAM_BTRFS_READ_ERRORS] = {
        .name = "system_brtfs_read_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "EIO or EREMOTEIO read errors from lower layers.",
    },
    [FAM_BTRFS_FLUSH_ERRORS] = {
        .name = "system_brtfs_flush_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "EIO or EREMOTEIO flush errors from lower layers.",
    },
    [FAM_BTRFS_CORRUPTION_ERRORS] = {
        .name = "system_brtfs_corruption_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of checksums errors, bytenr error or contents is illegal: "
                "this is an indication that the block was damaged during read or write, "
                "or written to wrong location or read from wrong location.",
    },
    [FAM_BTRFS_GENERATION_ERRORS] = {
        .name = "system_brtfs_generation_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "An indication that blocks have not been written.",
    },
};

static int btrfs_mountlist_create(void)
{
    if (llist_btrfs_paths != NULL) {
        llentry_t *e = llist_head(llist_btrfs_paths);
        while (e != NULL) {
            free(e->key);
            e = e->next;
        }
        llist_destroy(llist_btrfs_paths);
    }

    llist_btrfs_paths = llist_create();
    if (llist_btrfs_paths == NULL) {
        return -1;
    }
    return 0;
}

static int btrfs_mountlist_read(void)
{
    FILE *file_mounts = setmntent("/proc/mounts", "r");
    if (file_mounts == NULL) {
        return -1;
    }

    struct mntent *mount_entry = getmntent(file_mounts);
    while (mount_entry != NULL) {
        if (strncmp(mount_entry->mnt_type, "btrfs", 5) == 0) {
            char *mnt_dir = strdup(mount_entry->mnt_dir);
            if (mnt_dir == NULL) {
                PLUGIN_ERROR("strdup failed");
                continue;
            }
            llentry_t *entry = llentry_create(mnt_dir, NULL);
            if (entry == NULL) {
                PLUGIN_ERROR("llentry_create failed");
                free(mnt_dir);
                continue;
            }
            llist_append(llist_btrfs_paths, entry);
        }
        mount_entry = getmntent(file_mounts);
    }

    endmntent(file_mounts);
    return 0;
}

static int btrfs_submit_read_stats(char *mount_path)
{
    int ret = 0;

    DIR *dirstream = opendir(mount_path);
    if (dirstream == NULL) {
        PLUGIN_ERROR("open on %s failed %s", mount_path, strerror(errno));
        return -1;
    }

    int fd = dirfd(dirstream);
    if (fd < 0) {
        PLUGIN_ERROR("open on %s failed: %s", mount_path, strerror(errno));
        ret = -1;
        goto onerr;
    }

    struct btrfs_ioctl_fs_info_args fs_args = {0};
    ret = ioctl(fd, BTRFS_IOC_FS_INFO, &fs_args);
    if (ret < 0) {
        PLUGIN_ERROR("ioctl(BTRFS_IOC_FS_INFO) on %s failed: %s", mount_path, strerror(errno));
        goto onerr;
    }

    struct btrfs_ioctl_get_dev_stats dev_stats_args = {0};
    dev_stats_args.devid = fs_args.max_id;
    dev_stats_args.nr_items = BTRFS_DEV_STAT_VALUES_MAX;
    dev_stats_args.flags = 0;

    ret = ioctl(fd, BTRFS_IOC_GET_DEV_STATS, &dev_stats_args);
    if (ret < 0) {
        PLUGIN_ERROR("ioctl(BTRFS_IOC_GET_DEV_STATS) on %s failed: %s",
                     mount_path, strerror(errno));
        goto onerr;
    }

    metric_family_append(&fams[FAM_BTRFS_WRITE_ERRORS],
                         VALUE_COUNTER(dev_stats_args.values[BTRFS_DEV_STAT_WRITE_ERRS]), NULL,
                         &(label_pair_const_t){.name="path", .value=mount_path}, NULL);
    metric_family_append(&fams[FAM_BTRFS_READ_ERRORS],
                         VALUE_COUNTER(dev_stats_args.values[BTRFS_DEV_STAT_READ_ERRS]), NULL,
                         &(label_pair_const_t){.name="path", .value=mount_path}, NULL);
    metric_family_append(&fams[FAM_BTRFS_FLUSH_ERRORS],
                         VALUE_COUNTER(dev_stats_args.values[BTRFS_DEV_STAT_FLUSH_ERRS]), NULL,
                         &(label_pair_const_t){.name="path", .value=mount_path}, NULL);
    metric_family_append(&fams[FAM_BTRFS_CORRUPTION_ERRORS],
                         VALUE_COUNTER(dev_stats_args.values[BTRFS_DEV_STAT_CORRUPTION_ERRS]), NULL,
                         &(label_pair_const_t){.name="path", .value=mount_path}, NULL);
    metric_family_append(&fams[FAM_BTRFS_GENERATION_ERRORS],
                         VALUE_COUNTER(dev_stats_args.values[BTRFS_DEV_STAT_GENERATION_ERRS]), NULL,
                         &(label_pair_const_t){.name="path", .value=mount_path}, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_BTRFS_MAX, 0);

onerr:
    closedir(dirstream);
    close(fd);
    return ret;
}

static int btrfs_read(void)
{
    int ret = btrfs_init();
    if (ret < 0) {
        return -1;
    }

    if (btrfs_conf_refresh_always) {
        PLUGIN_DEBUG("Refresh mounts.");

        ret = btrfs_mountlist_create();
        if (ret < 0) {
            return -1;
        }

        btrfs_mountlist_read();
    }

    for (llentry_t *e = llist_head(llist_btrfs_paths); e != NULL; e = e->next) {
        btrfs_submit_read_stats(e->key);
    }

    return 0;
}

static int btrfs_shutdown(void)
{

    if (llist_btrfs_paths != NULL) {
        llentry_t *e = llist_head(llist_btrfs_paths);
        while (e != NULL) {
            free(e->key);
            e = e->next;
        }
        llist_destroy(llist_btrfs_paths);
    }

    return 0;
}

static int btrfs_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "refresh-mounts") == 0) {
            status = cf_util_get_boolean(child, &btrfs_conf_refresh_always);
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

static int btrfs_init(void)
{
    if (btrfs_is_init)
        return 0;

    int ret = btrfs_mountlist_create();
    if (ret < 0) {
        return -1;
    }

    ret = btrfs_mountlist_read();
    if (ret < 0) {
        return -1;
    }

    btrfs_is_init = true;
    return 0;
}

void module_register(void)
{
    plugin_register_config("btrfs", btrfs_config);
    plugin_register_read("btrfs", btrfs_read);
    plugin_register_shutdown("btrfs", btrfs_shutdown);
}
