// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2009 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009 Paul Sadauskas
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Paul Sadauskas <psadauskas at gmail.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"
#include "libutils/mount.h"

#ifdef HAVE_STATVFS
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#define STATANYFS statvfs
#define STATANYFS_STR "statvfs"
#define BLOCKSIZE(s) ((s).f_frsize ? (s).f_frsize : (s).f_bsize)
#elif defined(HAVE_STATFS)
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#define STATANYFS statfs
#define STATANYFS_STR "statfs"
#define BLOCKSIZE(s) (s).f_bsize
#else
#error "No applicable input method."
#endif

static exclist_t excl_device;
static exclist_t excl_mountpoint;
static exclist_t excl_fstype;
static exclist_t excl_errors;

static bool log_once;

enum {
    FAM_DF_FREE_BYTES,
    FAM_DF_RESERVED_BYTES,
    FAM_DF_USED_BYTES,
    FAM_DF_FREE_INODES,
    FAM_DF_RESERVED_INODES,
    FAM_DF_USED_INODES,
    FAM_DF_MAX,
};

static metric_family_t fams[FAM_DF_MAX] = {
    [FAM_DF_FREE_BYTES] = {
        .name = "system_df_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total amount of space in bytes available to unprivileged user.",
    },
    [FAM_DF_RESERVED_BYTES] = {
        .name = "system_df_reserved_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Space reserved by the system which is not normally available to a user.",
    },
    [FAM_DF_USED_BYTES] = {
        .name = "system_df_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total amount of space allocated to existing files in the file system.",
    },
    [FAM_DF_FREE_INODES] = {
        .name = "system_df_free_inodes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Free Inodes in the filesystem.",
    },
    [FAM_DF_RESERVED_INODES] = {
        .name = "system_df_reserved_inodes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Inodes reserverd in the filesystem.",
    },
    [FAM_DF_USED_INODES] = {
        .name = "system_df_used_inodes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Used inodes in the filesystem.",
    },
};

static int df_read(void)
{
#ifdef HAVE_STATVFS
    struct statvfs statbuf;
#elif defined(HAVE_STATFS)
    struct statfs statbuf;
#endif
    /* struct STATANYFS statbuf; */
    cu_mount_t *mnt_list = NULL;
    if (cu_mount_getlist(&mnt_list) == NULL) {
        PLUGIN_ERROR("cu_mount_getlist failed.");
        return -1;
    }

    for (cu_mount_t *mnt_ptr = mnt_list; mnt_ptr != NULL; mnt_ptr = mnt_ptr->next) {
        char const *dev = (mnt_ptr->spec_device != NULL) ? mnt_ptr->spec_device : mnt_ptr->device;

        if (!exclist_match(&excl_device, dev))
            continue;
        if (!exclist_match(&excl_mountpoint, mnt_ptr->dir))
            continue;
        if (!exclist_match(&excl_fstype, mnt_ptr->type))
            continue;

        if (STATANYFS(mnt_ptr->dir, &statbuf) < 0) {
            if ((log_once == false) || !exclist_match(&excl_errors, mnt_ptr->dir)) {
                if (log_once)
                    exclist_add_excl_string(&excl_errors, mnt_ptr->dir);
                PLUGIN_ERROR(STATANYFS_STR "(%s) failed: %s", mnt_ptr->dir, STRERRNO);
            }
            continue;
        } else {
            if (log_once)
                exclist_remove_excl_string(&excl_errors, mnt_ptr->dir);
        }

        if (!statbuf.f_blocks)
            continue;

        unsigned long long blocksize = BLOCKSIZE(statbuf);
        /* Sanity-check for the values in the struct */
        /* Check for negative "available" byes. For example UFS can
         * report negative free space for user. Notice. blk_reserved
         * will start to diminish after this. */
#ifdef HAVE_STATVFS
        /* Cast and temporary variable are needed to avoid
         * compiler warnings.
         * ((struct statvfs).f_bavail is unsigned (POSIX)) */
        int64_t signed_bavail = (int64_t)statbuf.f_bavail;
        if (signed_bavail < 0)
            statbuf.f_bavail = 0;
#elif defined(HAVE_STATFS)
        if (statbuf.f_bavail < 0)
            statbuf.f_bavail = 0;
#endif
        /* Make sure that f_blocks >= f_bfree >= f_bavail */
        if (statbuf.f_bfree < statbuf.f_bavail)
            statbuf.f_bfree = statbuf.f_bavail;
        if (statbuf.f_blocks < statbuf.f_bfree)
            statbuf.f_blocks = statbuf.f_bfree;

        uint64_t blk_free = (uint64_t)statbuf.f_bavail * blocksize;
        metric_family_append(&fams[FAM_DF_FREE_BYTES], VALUE_GAUGE(blk_free), NULL,
                             &(label_pair_const_t){.name="device", .value=dev},
                             &(label_pair_const_t){.name="fstype", .value=mnt_ptr->type},
                             &(label_pair_const_t){.name="mountpoint", .value=mnt_ptr->dir}, NULL);

        uint64_t blk_reserved = (uint64_t)(statbuf.f_bfree - statbuf.f_bavail) * blocksize;
        metric_family_append(&fams[FAM_DF_RESERVED_BYTES], VALUE_GAUGE(blk_reserved), NULL,
                             &(label_pair_const_t){.name="device", .value=dev},
                             &(label_pair_const_t){.name="fstype", .value=mnt_ptr->type},
                             &(label_pair_const_t){.name="mountpoint", .value=mnt_ptr->dir}, NULL);

        uint64_t blk_used = (uint64_t)(statbuf.f_blocks - statbuf.f_bfree) * blocksize;
        metric_family_append(&fams[FAM_DF_USED_BYTES], VALUE_GAUGE(blk_used), NULL,
                             &(label_pair_const_t){.name="device", .value=dev},
                             &(label_pair_const_t){.name="fstype", .value=mnt_ptr->type},
                             &(label_pair_const_t){.name="mountpoint", .value=mnt_ptr->dir}, NULL);

        /* inode handling */
        if (statbuf.f_files != 0 && statbuf.f_ffree != 0) {
            /* Sanity-check for the values in the struct */
            if (statbuf.f_ffree < statbuf.f_favail)
                statbuf.f_ffree = statbuf.f_favail;
            if (statbuf.f_files < statbuf.f_ffree)
                statbuf.f_files = statbuf.f_ffree;

            uint64_t inode_free = (uint64_t)statbuf.f_favail;
            metric_family_append(&fams[FAM_DF_FREE_INODES], VALUE_GAUGE(inode_free), NULL,
                                 &(label_pair_const_t){.name="device", .value=dev},
                                 &(label_pair_const_t){.name="fstype", .value=mnt_ptr->type},
                                 &(label_pair_const_t){.name="mountpoint", .value=mnt_ptr->dir},
                                 NULL);

            uint64_t inode_reserved = (uint64_t)(statbuf.f_ffree - statbuf.f_favail);
            metric_family_append(&fams[FAM_DF_RESERVED_INODES], VALUE_GAUGE(inode_reserved), NULL,
                                 &(label_pair_const_t){.name="device", .value=dev},
                                 &(label_pair_const_t){.name="fstype", .value=mnt_ptr->type},
                                 &(label_pair_const_t){.name="mountpoint", .value=mnt_ptr->dir},
                                 NULL);

            uint64_t inode_used = (uint64_t)(statbuf.f_files - statbuf.f_ffree);
            metric_family_append(&fams[FAM_DF_USED_INODES], VALUE_GAUGE(inode_used), NULL,
                                 &(label_pair_const_t){.name="device", .value=dev},
                                 &(label_pair_const_t){.name="fstype", .value=mnt_ptr->type},
                                 &(label_pair_const_t){.name="mountpoint", .value=mnt_ptr->dir},
                                 NULL);
        }
    }

    cu_mount_freelist(mnt_list);

    plugin_dispatch_metric_family_array(fams, FAM_DF_MAX, 0);
    return 0;
}

static int df_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "device") == 0) {
            status = cf_util_exclist(child, &excl_device);
        } else if (strcasecmp(child->key, "mount-point") == 0) {
            status = cf_util_exclist(child, &excl_mountpoint);
        } else if (strcasecmp(child->key, "fs-type") == 0) {
            status = cf_util_exclist(child, &excl_fstype );
        } else if (strcasecmp(child->key, "log-once") == 0) {
            status = cf_util_get_boolean(child, &log_once);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0)
        return -1;

    return 0;
}

static int df_shutdown(void)
{
    exclist_reset(&excl_device);
    exclist_reset(&excl_mountpoint);
    exclist_reset(&excl_fstype);
    exclist_reset(&excl_errors);
    return 0;
}

void module_register(void)
{
    plugin_register_config("df", df_config);
    plugin_register_read("df", df_read);
    plugin_register_shutdown("df", df_shutdown);
}
