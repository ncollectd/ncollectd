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

#ifdef HAVE_SYS_CAPABILITY_H
    #include <sys/capability.h>
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
        .name = "system_quota_inodes_grace_expiration_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Time when the inodes soft limit grace period expires, "
                "in seconds since the epoch.",
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
        .name = "system_quota_space_hard_limit_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Absolute limit on disk space in bytes.",
    },
    [FAM_QUOTA_SPACE_SOFT_LIMIT] = {
        .name = "system_quota_space_soft_limit_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Preferred limit on disk space in bytes.",
    },
    [FAM_QUOTA_SPACE_TIME] = {
        .name = "system_quota_space_grace_expiration_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Time when the disk space soft limit grace period expires, "
                "in seconds since the epoch.",
    },
};

struct user {
    uid_t uid;
    char *name;
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
    if (u == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return;
    }

    /* The strings of struct passwd are overwritten by the next getpwent call. */
    u->uid = p->pw_uid;
    u->name = strdup(p->pw_name);
    if (u->name == NULL) {
        PLUGIN_ERROR("strdup failed.");
        free(u);
        return;
    }

    u->next = userlist;
    userlist = u;
}

static void quota_userlist_free(void)
{
    struct user *u = userlist;
    while (u != NULL) {
        struct user *next = u->next;
        free(u->name);
        free(u);
        u = next;
    }
    userlist = NULL;
}

static void quota_userlist_build(void)
{
    struct stat st;
    struct passwd *p;
    time_t now;

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

    quota_userlist_free();

    /* Rewind the passwd database, otherwise a reload would return no entries. */
    setpwent();
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
    endpwent();
}


static void quota_report_disk(const char *disk_name, const char *device,
                              __attribute__((unused)) const long long blocksize)
{
    static complain_t complaint = {0};
    struct dqblk info;
    struct user *user;

    for (user = userlist; user != NULL; user = user->next) {
        /* quotactl needs the block device, not the mount point. */
        if (quotactl(QCMD(Q_GETQUOTA, USRQUOTA), device, user->uid, (caddr_t)&info) != 0) {
            /* Quotas not supported or not enabled in this filesystem. */
            if ((errno == ESRCH) || (errno == ENOTBLK) || (errno == ENODEV) ||
                (errno == ENOENT) || (errno == ENOSYS))
                return;
            PLUGIN_ONCE_WARNING(&complaint,
                                "quota plugin: quotactl(Q_GETQUOTA, %s, %u) failed: %s",
                                device, (unsigned int)user->uid, STRERRNO);
            continue;
        }

        if (report_zero || (info.dqb_curinodes != 0)) {
            if ((info.dqb_valid & QIF_INODES) == QIF_INODES)
                metric_family_append(&fams[FAM_QUOTA_INODES],
                                     VALUE_GAUGE(info.dqb_curinodes), NULL,
                                     &LABEL_PAIR_CONST("disk", disk_name),
                                     &LABEL_PAIR_CONST("user", user->name),
                                     NULL);

            /* A zero time means that no grace period is running. */
            if (((info.dqb_valid & QIF_ITIME) == QIF_ITIME) && (info.dqb_itime != 0))
                metric_family_append(&fams[FAM_QUOTA_INODES_TIME],
                                     VALUE_GAUGE(info.dqb_itime), NULL,
                                     &LABEL_PAIR_CONST("disk", disk_name),
                                     &LABEL_PAIR_CONST("user", user->name),
                                     NULL);

            if ((info.dqb_valid & QIF_ILIMITS) == QIF_ILIMITS) {
               if (info.dqb_ihardlimit != 0)
                   metric_family_append(&fams[FAM_QUOTA_INODES_HARD_LIMIT],
                                        VALUE_GAUGE(info.dqb_ihardlimit), NULL,
                                        &LABEL_PAIR_CONST("disk", disk_name),
                                        &LABEL_PAIR_CONST("user", user->name),
                                        NULL);
               if (info.dqb_isoftlimit != 0)
                   metric_family_append(&fams[FAM_QUOTA_INODES_SOFT_LIMIT],
                                        VALUE_GAUGE(info.dqb_isoftlimit), NULL,
                                        &LABEL_PAIR_CONST("disk", disk_name),
                                        &LABEL_PAIR_CONST("user", user->name),
                                        NULL);
            }
        }

        if (report_zero || (info.dqb_curspace != 0)) {
            if ((info.dqb_valid & QIF_SPACE) == QIF_SPACE)
                metric_family_append(&fams[FAM_QUOTA_SPACE_BYTES],
                                     VALUE_GAUGE(info.dqb_curspace), NULL,
                                     &LABEL_PAIR_CONST("disk", disk_name),
                                     &LABEL_PAIR_CONST("user", user->name),
                                     NULL);

            if ((info.dqb_valid & QIF_BLIMITS) == QIF_BLIMITS) {
               if (info.dqb_bhardlimit != 0)
                   metric_family_append(&fams[FAM_QUOTA_SPACE_HARD_LIMIT],
                                        VALUE_GAUGE((double)info.dqb_bhardlimit * QIF_DQBLKSIZE),
                                        NULL,
                                        &LABEL_PAIR_CONST("disk", disk_name),
                                        &LABEL_PAIR_CONST("user", user->name),
                                        NULL);
               if (info.dqb_bsoftlimit != 0)
                   metric_family_append(&fams[FAM_QUOTA_SPACE_SOFT_LIMIT],
                                        VALUE_GAUGE((double)info.dqb_bsoftlimit * QIF_DQBLKSIZE),
                                        NULL,
                                        &LABEL_PAIR_CONST("disk", disk_name),
                                        &LABEL_PAIR_CONST("user", user->name),
                                        NULL);
            }

            if (((info.dqb_valid & QIF_BTIME) == QIF_BTIME) && (info.dqb_btime != 0))
                metric_family_append(&fams[FAM_QUOTA_SPACE_TIME],
                                     VALUE_GAUGE(info.dqb_btime), NULL,
                                     &LABEL_PAIR_CONST("disk", disk_name),
                                     &LABEL_PAIR_CONST("user", user->name),
                                     NULL);
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

    quota_userlist_build();

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

        if ((mnt_ptr->device == NULL) || (mnt_ptr->device[0] != '/'))
            continue;

        quota_report_disk(disk_name, mnt_ptr->device, BLOCKSIZE(statbuf));
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
        } else if (strcasecmp(child->key, "report-by-device") == 0) {
            status = cf_util_get_boolean(child, &by_device);
        } else if (strcasecmp(child->key, "report-zero") == 0) {
            status = cf_util_get_boolean(child, &report_zero);
        } else if (strcasecmp(child->key, "reload-users-interval") == 0) {
            status = cf_util_get_int(child, &reload_users_interval);
            if ((status == 0) && (reload_users_interval < 0)) {
                PLUGIN_ERROR("Option '%s' in %s:%d must be a positive integer.",
                             child->key, cf_get_file(child), cf_get_lineno(child));
                status = -1;
            }
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

static int quota_init(void)
{
#if defined(HAVE_SYS_CAPABILITY_H) && defined(CAP_SYS_ADMIN)
    if (plugin_check_capability(CAP_SYS_ADMIN) != 0) {
        if (getuid() == 0)
            PLUGIN_WARNING("Running ncollectd as root, but the "
                           "CAP_SYS_ADMIN capability is missing. The plugin's read "
                           "function will probably fail. Is your init system dropping "
                           "capabilities?");
        else
            PLUGIN_WARNING("ncollectd doesn't have the CAP_SYS_ADMIN "
                           "capability. If you don't want to run ncollectd as root, try "
                           "running \"setcap cap_sys_admin=ep\" on the ncollectd binary.");
    }
#endif
    return 0;
}

static int quota_shutdown(void)
{
    exclist_reset(&excl_device);
    exclist_reset(&excl_mountpoint);
    exclist_reset(&excl_fstype);
    exclist_reset(&excl_userid);
    exclist_reset(&excl_username);

    quota_userlist_free();

    return 0;
}

void module_register(void)
{
    plugin_register_config("quota", quota_config);
    plugin_register_init("quota", quota_init);
    plugin_register_read("quota", quota_read);
    plugin_register_shutdown("quota", quota_shutdown);
}
