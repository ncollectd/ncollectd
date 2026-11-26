// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2018 Martin Langlotz
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Martin Langlotz <stackshadow at evilbrain.de>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/itoa.h"
#include "libutils/llist.h"
#include "libutils/mount.h"

#include <btrfs/ioctl.h>

static bool btrfs_is_init;
static bool btrfs_conf_refresh_always;
static llist_t *llist_btrfs_paths;
static char *path_proc_mounts;

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
        .name = "system_btrfs_write_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "EIO or EREMOTEIO write errors from lower layers.",
    },
    [FAM_BTRFS_READ_ERRORS] = {
        .name = "system_btrfs_read_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "EIO or EREMOTEIO read errors from lower layers.",
    },
    [FAM_BTRFS_FLUSH_ERRORS] = {
        .name = "system_btrfs_flush_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "EIO or EREMOTEIO flush errors from lower layers.",
    },
    [FAM_BTRFS_CORRUPTION_ERRORS] = {
        .name = "system_btrfs_corruption_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of checksums errors, bytenr error or contents is illegal: "
                "this is an indication that the block was damaged during read or write, "
                "or written to wrong location or read from wrong location.",
    },
    [FAM_BTRFS_GENERATION_ERRORS] = {
        .name = "system_btrfs_generation_errors",
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
    FILE *file_mounts = setmntent(path_proc_mounts, "r");
    if (file_mounts == NULL) {
        PLUGIN_ERROR("setmntent('%s') failed: %s", path_proc_mounts, STRERRNO);
        return -1;
    }

    struct mntent mount_entry;
    char buffer[4096];
    while (getmntent_r(file_mounts, &mount_entry, buffer, sizeof(buffer)) != NULL) {
        if (strcmp(mount_entry.mnt_type, "btrfs") == 0) {
            char *mnt_dir = strdup(mount_entry.mnt_dir);
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
    }

    endmntent(file_mounts);
    return 0;
}

static void btrfs_fsid_to_str(const uint8_t *fsid, char *buf)
{
    static const char hex[] = "0123456789abcdef";

    for (size_t i = 0; i < BTRFS_FSID_SIZE; i++) {
        if ((i == 4) || (i == 6) || (i == 8) || (i == 10))
            *(buf++) = '-';
        *(buf++) = hex[fsid[i] >> 4];
        *(buf++) = hex[fsid[i] & 0x0f];
    }
    *buf = '\0';
}

static int btrfs_submit_read_stats(char *mount_path, uint8_t *fsids, size_t *fsids_num)
{
    int fd = open(mount_path, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        PLUGIN_ERROR("open on '%s' failed: %s", mount_path, STRERRNO);
        return -1;
    }

    struct btrfs_ioctl_fs_info_args fs_args = {0};
    int ret = ioctl(fd, BTRFS_IOC_FS_INFO, &fs_args);
    if (ret < 0) {
        PLUGIN_ERROR("ioctl(BTRFS_IOC_FS_INFO) on '%s' failed: %s", mount_path, STRERRNO);
        close(fd);
        return -1;
    }

    /* Skip subvolumes and bind mounts of an already reported filesystem. */
    for (size_t i = 0; i < *fsids_num; i++) {
        if (memcmp(fsids + i * BTRFS_FSID_SIZE, fs_args.fsid, BTRFS_FSID_SIZE) == 0) {
            close(fd);
            return 0;
        }
    }
    memcpy(fsids + *fsids_num * BTRFS_FSID_SIZE, fs_args.fsid, BTRFS_FSID_SIZE);
    (*fsids_num)++;

    char uuid[2 * BTRFS_FSID_SIZE + 5];
    btrfs_fsid_to_str(fs_args.fsid, uuid);

    /* Device ids can have gaps, devid 0 is the target of a running replace. */
    for (uint64_t devid = 0; devid <= fs_args.max_id; devid++) {
        struct btrfs_ioctl_dev_info_args dev_info = {0};
        dev_info.devid = devid;
 
        ret = ioctl(fd, BTRFS_IOC_DEV_INFO, &dev_info);
        if (ret < 0) {
            if (errno != ENODEV)
                PLUGIN_ERROR("ioctl(BTRFS_IOC_DEV_INFO) on '%s' for devid %" PRIu64
                             " failed: %s", mount_path, devid, STRERRNO);
            continue;
        }

        struct btrfs_ioctl_get_dev_stats dev_stats = {0};
        dev_stats.devid = devid;
        dev_stats.nr_items = BTRFS_DEV_STAT_VALUES_MAX;
        dev_stats.flags = 0;

        ret = ioctl(fd, BTRFS_IOC_GET_DEV_STATS, &dev_stats);
        if (ret < 0) {
            if (errno != ENODEV)
                PLUGIN_ERROR("ioctl(BTRFS_IOC_GET_DEV_STATS) on '%s' for devid %" PRIu64
                             " failed: %s", mount_path, devid, STRERRNO);
            continue;
        }

        char devid_str[ITOA_MAX];
        uitoa(devid, devid_str);

        dev_info.path[sizeof(dev_info.path) - 1] = '\0';
        const char *device = NULL;
        if (dev_info.path[0] != '\0')
            device = (const char *)dev_info.path;

        metric_family_append(&fams[FAM_BTRFS_WRITE_ERRORS],
                             VALUE_COUNTER(dev_stats.values[BTRFS_DEV_STAT_WRITE_ERRS]), NULL,
                             &LABEL_PAIR_CONST("path", mount_path),
                             &LABEL_PAIR_CONST("devid", devid_str),
                             &LABEL_PAIR_CONST("device", device), NULL);
        metric_family_append(&fams[FAM_BTRFS_READ_ERRORS],
                             VALUE_COUNTER(dev_stats.values[BTRFS_DEV_STAT_READ_ERRS]), NULL,
                             &LABEL_PAIR_CONST("path", mount_path),
                             &LABEL_PAIR_CONST("devid", devid_str),
                             &LABEL_PAIR_CONST("device", device), NULL);
        metric_family_append(&fams[FAM_BTRFS_FLUSH_ERRORS],
                             VALUE_COUNTER(dev_stats.values[BTRFS_DEV_STAT_FLUSH_ERRS]), NULL,
                             &LABEL_PAIR_CONST("path", mount_path),
                             &LABEL_PAIR_CONST("devid", devid_str),
                             &LABEL_PAIR_CONST("device", device), NULL);
        metric_family_append(&fams[FAM_BTRFS_CORRUPTION_ERRORS],
                             VALUE_COUNTER(dev_stats.values[BTRFS_DEV_STAT_CORRUPTION_ERRS]), NULL,
                             &LABEL_PAIR_CONST("path", mount_path),
                             &LABEL_PAIR_CONST("devid", devid_str),
                             &LABEL_PAIR_CONST("device", device), NULL);
        metric_family_append(&fams[FAM_BTRFS_GENERATION_ERRORS],
                             VALUE_COUNTER(dev_stats.values[BTRFS_DEV_STAT_GENERATION_ERRS]), NULL,
                             &LABEL_PAIR_CONST("path", mount_path),
                             &LABEL_PAIR_CONST("devid", devid_str),
                             &LABEL_PAIR_CONST("device", device), NULL);
    }

    close(fd);

    plugin_dispatch_metric_family_array(fams, FAM_BTRFS_MAX, 0);

    return 0;
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

    int num = llist_size(llist_btrfs_paths);
    if (num <= 0)
        return 0;

    uint8_t *fsids = calloc(num, BTRFS_FSID_SIZE);
    if (fsids == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    size_t fsids_num = 0;
    bool stale = false;
    for (llentry_t *e = llist_head(llist_btrfs_paths); e != NULL; e = e->next) {
        if (btrfs_submit_read_stats(e->key, fsids, &fsids_num) != 0)
            stale = true;
    }

    free(fsids);

    /* A cached mount point is gone or unusable: reload the list on the next read. */
    if (stale && !btrfs_conf_refresh_always)
        btrfs_is_init = false;

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

    free(path_proc_mounts);

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

    if (path_proc_mounts == NULL) {
        path_proc_mounts = plugin_procpath("mounts");
        if (path_proc_mounts == NULL) {
            PLUGIN_ERROR("Cannot get proc path.");
            return -1;
        }
    }

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
