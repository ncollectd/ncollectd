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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

#ifdef HAVE_SYS_QUOTA_H
    #include <sys/quota.h>
#endif

#ifdef HAVE_XFS_XQM_H
    #include <xfs/xqm.h>
#endif

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

enum {
    FAM_QUOTA_INODES,
    FAM_QUOTA_INODES_TIME,
    FAM_QUOTA_INODES_HARD_LIMIT,
    FAM_QUOTA_INODES_SOFT_LIMIT,
    FAM_QUOTA_SPACE_BYTES,
    FAM_QUOTA_SPACE_HARD_LIMIT,
    FAM_QUOTA_SPACE_SOFT_LIMIT,
    FAM_QUOTA_SPACE_TIME,
    FAM_QUOTA_MAX
};

static metric_family_t fams[FAM_QUOTA_MAX] = {
    [FAM_QUOTA_INODES] = {
        .name = "system_quota_inodes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of allocated inodes.",
    },
    [FAM_QUOTA_INODES_TIME] = {
        .name = "system_quota_inodes_time",
        .type = METRIC_TYPE_GAUGE,
        .help = "Time limit for excessive files.",
    },
    [FAM_QUOTA_INODES_HARD_LIMIT] = {
        .name = "system_quota_inodes_hard_limit",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum number of allocated inodes.",
    },
    [FAM_QUOTA_INODES_SOFT_LIMIT] = {
        .name = "system_quota_inodes_soft_limit",
        .type = METRIC_TYPE_GAUGE,
        .help = "Preferred inode limit.",
    },
    [FAM_QUOTA_SPACE_BYTES] = {
        .name = "system_quota_space_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current occupied space.",
    },
    [FAM_QUOTA_SPACE_HARD_LIMIT] = {
        .name = "system_quota_space_hard_limit",
        .type = METRIC_TYPE_GAUGE,
        .help = "Absolute limit on disk quota blocks alloc.",
    },
    [FAM_QUOTA_SPACE_SOFT_LIMIT] = {
        .name = "system_quota_space_soft_limit",
        .type = METRIC_TYPE_GAUGE,
        .help = "Preferred limit on disk quota blocks.",
    },
    [FAM_QUOTA_SPACE_TIME] = {
        .name = "system_quota_space_time",
        .type = METRIC_TYPE_GAUGE,
        .help = "Time limit for excessive disk use.",
    },
};

struct user {
    struct passwd p;
    struct user *next;
};

static exclist_t excl_device;
static exclist_t excl_mountpoint;
static exclist_t excl_fstype;
static exclist_t excl_userid;
static exclist_t excl_username;

static bool by_device;
static int  reload_users_interval;
static bool report_by_username = true;
static bool report_zero;
static time_t lastUserCheck;
static struct user *userlist;

static bool quota_match_username(const struct passwd *p)
{
    return exclist_match(&excl_username, p->pw_name);
}

static bool quota_match_userid(const struct passwd *p)
{
    char uid[20];

    snprintf(uid, sizeof(uid), "%u", p->pw_uid);

    return exclist_match(&excl_userid, uid);
}

static void quota_add_user(const struct passwd *p)
{
    struct user *u;

    u = (struct user *)calloc(1, sizeof(*u));
    if (NULL != u) {
        u->p = *p;
        u->next = userlist;
        userlist = u;
    }
}

static void quota_build_userlist(void)
{
    struct stat st;
    struct passwd *p;
    time_t now;

    while (NULL != userlist) {
        struct user *u = userlist;
        userlist = u->next;
        u->next = NULL;
        free(u);
    }

    now = time(NULL);

    if (reload_users_interval > 0) {
        if (now - lastUserCheck < reload_users_interval)
            return;
    } else {
        if (stat("/etc/passwd", &st) != 0)
            return;

        if (st.st_mtime < lastUserCheck) {
            return;
        }
    }

    lastUserCheck = now;

    if (report_by_username) {
        while ((p = getpwent()) != NULL) {
            if (quota_match_username(p))
                quota_add_user(p);
        }
    } else {
        while ((p = getpwent()) != NULL) {
            if (quota_match_userid(p))
                quota_add_user(p);
        }
    }
}


static void quota_report_disk(const char *disk_name, char *dir,
                              __attribute__((unused)) const long long blocksize)
{
    struct dqblk info;
    struct user *user;

    for (user = userlist; user != NULL; user = user->next) {
        if (quotactl(Q_GETQUOTA, dir, user->p.pw_uid, (caddr_t)&info) != 0)
            continue;

        if (report_zero || (info.dqb_curinodes != 0)) {
            if ((info.dqb_valid & QIF_INODES) == QIF_INODES) {
                metric_family_append(&fams[FAM_QUOTA_INODES],
                                     VALUE_GAUGE(info.dqb_curinodes), NULL,
                                     &(label_pair_const_t){.name="disk", .value=disk_name},
                                     &(label_pair_const_t){.name="user", .value=user->p.pw_name,},
                                     NULL);
            }

            if ((info.dqb_valid & QIF_ITIME) == QIF_ITIME) {
                metric_family_append(&fams[FAM_QUOTA_INODES_TIME],
                                     VALUE_GAUGE(info.dqb_itime), NULL,
                                     &(label_pair_const_t){.name="disk", .value=disk_name},
                                     &(label_pair_const_t){.name="user", .value=user->p.pw_name,},
                                     NULL);
            }

            if ((info.dqb_valid & QIF_ILIMITS) == QIF_ILIMITS) {
                metric_family_append(&fams[FAM_QUOTA_INODES_HARD_LIMIT],
                                     VALUE_GAUGE(info.dqb_ihardlimit), NULL,
                                     &(label_pair_const_t){.name="disk", .value=disk_name},
                                     &(label_pair_const_t){.name="user", .value=user->p.pw_name,},
                                     NULL);
                metric_family_append(&fams[FAM_QUOTA_INODES_SOFT_LIMIT],
                                     VALUE_GAUGE(info.dqb_isoftlimit), NULL,
                                     &(label_pair_const_t){.name="disk", .value=disk_name},
                                     &(label_pair_const_t){.name="user", .value=user->p.pw_name,},
                                     NULL);
            }
        }

        if (report_zero || (info.dqb_curspace != 0)) {
            if ((info.dqb_valid & QIF_SPACE) == QIF_SPACE) {
                metric_family_append(&fams[FAM_QUOTA_SPACE_BYTES],
                                     VALUE_GAUGE(info.dqb_curspace), NULL,
                                     &(label_pair_const_t){.name="disk", .value=disk_name},
                                     &(label_pair_const_t){.name="user", .value=user->p.pw_name,},
                                     NULL);
            }

            if ((info.dqb_valid & QIF_BLIMITS) == QIF_BLIMITS) {
                metric_family_append(&fams[FAM_QUOTA_SPACE_HARD_LIMIT],
                                     VALUE_GAUGE(info.dqb_bhardlimit), NULL,
                                     &(label_pair_const_t){.name="disk", .value=disk_name},
                                     &(label_pair_const_t){.name="user", .value=user->p.pw_name,},
                                     NULL);
                metric_family_append(&fams[FAM_QUOTA_SPACE_SOFT_LIMIT],
                                     VALUE_GAUGE(info.dqb_bsoftlimit), NULL,
                                     &(label_pair_const_t){.name="disk", .value=disk_name},
                                     &(label_pair_const_t){.name="user", .value=user->p.pw_name,},
                                     NULL);
            }

            if ((info.dqb_valid & QIF_BTIME) == QIF_BTIME) {
                metric_family_append(&fams[FAM_QUOTA_SPACE_TIME],
                                     VALUE_GAUGE(info.dqb_btime), NULL,
                                     &(label_pair_const_t){.name="disk", .value=disk_name},
                                     &(label_pair_const_t){.name="user", .value=user->p.pw_name,},
                                     NULL);
            }
        }
    }
}

static int quota_read(void)
{
#ifdef HAVE_STATVFS
    struct statvfs statbuf;
#elif defined(HAVE_STATFS)
    struct statfs statbuf;
#endif
    int retval = 0;
    cu_mount_t *mnt_list;

    mnt_list = NULL;
    if (cu_mount_getlist(&mnt_list) == NULL) {
        PLUGIN_ERROR("cu_mount_getlist failed.");
        return -1;
    }

    quota_build_userlist();

    for (cu_mount_t *mnt_ptr = mnt_list; mnt_ptr != NULL; mnt_ptr = mnt_ptr->next) {
        char disk_name[256];
        cu_mount_t *dup_ptr;

        char const *dev = (mnt_ptr->spec_device != NULL) ? mnt_ptr->spec_device : mnt_ptr->device;

        if (!exclist_match(&excl_device, dev)) {
//            mnt_ptr->ignored = 1;
            continue;
        }
        if (!exclist_match(&excl_mountpoint, mnt_ptr->dir)) {
//            mnt_ptr->ignored = 1;
            continue;
        }
        if (!exclist_match(&excl_fstype, mnt_ptr->type)) {
//            mnt_ptr->ignored = 1;
            continue;
        }

        /* search for duplicates *in front of* the current mnt_ptr. */
        for (dup_ptr = mnt_list; dup_ptr != NULL; dup_ptr = dup_ptr->next) {
            /* No duplicate found: mnt_ptr is the first of its kind. */
            if (dup_ptr == mnt_ptr) {
                dup_ptr = NULL;
                break;
            }
//            if (dup_ptr->ignored == 1)
//                continue;

            /* Duplicate found: leave non-NULL dup_ptr. */
            if (by_device && (mnt_ptr->spec_device != NULL) &&
                    (dup_ptr->spec_device != NULL) &&
                    (strcmp(mnt_ptr->spec_device, dup_ptr->spec_device) == 0))
                break;
            else if (!by_device && (strcmp(mnt_ptr->dir, dup_ptr->dir) == 0))
                break;
        }

        /* ignore duplicates */
        if (dup_ptr != NULL)
            continue;

        if (STATANYFS(mnt_ptr->dir, &statbuf) < 0) {
            PLUGIN_ERROR(STATANYFS_STR "(%s) failed: %s", mnt_ptr->dir, STRERRNO);
            continue;
        }

        if (by_device) {
            /* eg, /dev/hda1    -- strip off the "/dev/" */
            if (strncmp(dev, "/dev/", strlen("/dev/")) == 0)
                sstrncpy(disk_name, dev + strlen("/dev/"), sizeof(disk_name));
            else
                sstrncpy(disk_name, dev, sizeof(disk_name));

            if (strlen(disk_name) < 1) {
                PLUGIN_DEBUG("no device name for mountpoint %s, skipping", mnt_ptr->dir);
                continue;
            }
        } else {
            sstrncpy(disk_name, mnt_ptr->dir,  sizeof(disk_name));
        }

        quota_report_disk(disk_name, mnt_ptr->dir, BLOCKSIZE(statbuf));
    }

    plugin_dispatch_metric_family_array(fams, FAM_QUOTA_MAX, 0);

    cu_mount_freelist(mnt_list);
    return retval;
}

static int quota_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "device") == 0) {
            status = cf_util_exclist(child, &excl_device);
        } else if (strcasecmp(child->key, "mount-point") == 0) {
            status = cf_util_exclist(child, &excl_mountpoint);
        } else if (strcasecmp(child->key, "fs-type") == 0) {
            status = cf_util_exclist(child, &excl_fstype);
        } else if (strcasecmp(child->key, "user-id") == 0) {
            status = cf_util_exclist(child, &excl_userid);
        } else if (strcasecmp(child->key, "user-name") == 0) {
            status = cf_util_exclist(child, &excl_username);
        } else if (strcasecmp(child->key, "report-by-user-name") == 0) {
            status = cf_util_get_boolean(child, &report_by_username);
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

static int quota_shutdown(void)
{
    exclist_reset(&excl_device);
    exclist_reset(&excl_mountpoint);
    exclist_reset(&excl_fstype);
    exclist_reset(&excl_userid);
    exclist_reset(&excl_username);
    return 0;
}

void module_register(void)
{
    plugin_register_config("quota", quota_config);
    plugin_register_read("quota", quota_read);
    plugin_register_init("quota", quota_shutdown);
}
