// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

static char *path_sys_xfs;

enum {
    COLLECT_XFS_EXTENT_ALLOC = 1 << 1,
    COLLECT_XFS_ABT          = 1 << 2,
    COLLECT_XFS_BLK_MAP      = 1 << 3,
    COLLECT_XFS_BMBT         = 1 << 4,
    COLLECT_XFS_DIR          = 1 << 5,
    COLLECT_XFS_TRANS        = 1 << 6,
    COLLECT_XFS_IG           = 1 << 7,
    COLLECT_XFS_LOG          = 1 << 8,
    COLLECT_XFS_PUSH_AIL     = 1 << 9,
    COLLECT_XFS_XSTRAT       = 1 << 10,
    COLLECT_XFS_RW           = 1 << 11,
    COLLECT_XFS_ATTR         = 1 << 12,
    COLLECT_XFS_ICLUSTER     = 1 << 13,
    COLLECT_XFS_VNODES       = 1 << 14,
    COLLECT_XFS_BUF          = 1 << 15,
    COLLECT_XFS_ABTB2        = 1 << 16,
    COLLECT_XFS_ABTC2        = 1 << 17,
    COLLECT_XFS_BMBT2        = 1 << 18,
    COLLECT_XFS_IBT2         = 1 << 19,
    COLLECT_XFS_FIBT2        = 1 << 20,
    COLLECT_XFS_RMAPBT       = 1 << 21,
    COLLECT_XFS_REFCNTBT     = 1 << 22,
    COLLECT_XFS_QM           = 1 << 23,
    COLLECT_XFS_XPC          = 1 << 24,
    COLLECT_XFS_DEFER_RELOG  = 1 << 25,
};

static cf_flags_t xfs_flags_list[] = {
    {"ExtentAlloc", COLLECT_XFS_EXTENT_ALLOC },
    {"Abt"        , COLLECT_XFS_ABT          },
    {"BlkMap"     , COLLECT_XFS_BLK_MAP      },
    {"Bmbt"       , COLLECT_XFS_BMBT         },
    {"Dir"        , COLLECT_XFS_DIR          },
    {"Trans"      , COLLECT_XFS_TRANS        },
    {"IG"         , COLLECT_XFS_IG           },
    {"Log"        , COLLECT_XFS_LOG          },
    {"PushAil"    , COLLECT_XFS_PUSH_AIL     },
    {"XStrat"     , COLLECT_XFS_XSTRAT       },
    {"RW"         , COLLECT_XFS_RW           },
    {"Attr"       , COLLECT_XFS_ATTR         },
    {"ICluster"   , COLLECT_XFS_ICLUSTER     },
    {"VNodes"     , COLLECT_XFS_VNODES       },
    {"Buf"        , COLLECT_XFS_BUF          },
    {"Abtb2"      , COLLECT_XFS_ABTB2        },
    {"Abtc2"      , COLLECT_XFS_ABTC2        },
    {"Bmbt2"      , COLLECT_XFS_BMBT2        },
    {"Ibt2"       , COLLECT_XFS_IBT2         },
    {"Fibt2"      , COLLECT_XFS_FIBT2        },
    {"rMapBt"     , COLLECT_XFS_RMAPBT       },
    {"RefCntBt"   , COLLECT_XFS_REFCNTBT     },
    {"Qm"         , COLLECT_XFS_QM           },
    {"Xpc"        , COLLECT_XFS_XPC          },
    {"defer_relog", COLLECT_XFS_DEFER_RELOG  },
};

#include "plugins/xfs/xfs_stats_fam.h"
#include "plugins/xfs/xfs_stats.h"

static exclist_t xfs_excl_device;
static uint64_t xfs_flags = COLLECT_XFS_RW;

static int xfs_read_stats(__attribute__((unused)) int dir_fd, char const *dir,
                          char const *file, __attribute__((unused)) void *user_data)
{
    char path[PATH_MAX];
    ssnprintf(path, sizeof(path), "%s/%s/stats/stats", dir, file);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        PLUGIN_ERROR("fopen (%s) failed: %s", path, STRERRNO);
        return 0;
    }

    char line[1024];
    while (fgets(line, sizeof(line) - 1, fp) != NULL) {
        char *fields[32];

        line[sizeof(line) - 1] = '\0';

        int numfields = strsplit(line, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields < 2)
            continue;

        const struct xfs_stats_metric *xsm = xfs_stats_get_key(fields[0], strlen(fields[0]));
        if (xsm == NULL)
            continue;

        if (!(xfs_flags & xsm->collect))
            continue;

        if ((numfields - 1) != (int)xsm->entries_size)
            continue;

        for (size_t i = 0; i < xsm->entries_size; i++) {
            long long v = atoll(fields[i + 1]);
            value_t value = {0};
            metric_family_t *fam = &fams[xsm->entries[i]];
            if (fam->type == METRIC_TYPE_COUNTER)
                value = VALUE_COUNTER(v);
            else
                value = VALUE_GAUGE(v);
            metric_family_append(fam, value, NULL,
                                 &(label_pair_const_t){.name="device", .value=file}, NULL);
        }
    }

    fclose(fp);
    return 0;
}

static int xfs_read_sysfs(int dir_fd, char const *dir, char const *file, void *user_data)
{
    if (strcmp(file, "stats") == 0)
        return 0;

    if (!exclist_match(&xfs_excl_device, file))
        return 0;

    return xfs_read_stats(dir_fd, dir, file, user_data);
}

static int xfs_read(void)
{
    int status = walk_directory(path_sys_xfs, xfs_read_sysfs, /* user_data = */ NULL,
                                                              /* include hidden */ 0);
    if (status != 0)
        return -1;

    plugin_dispatch_metric_family_array(fams, FAM_XFS_MAX, 0);

    return 0;
}

static int xfs_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "device") == 0) {
            status = cf_util_exclist(child, &xfs_excl_device);
        } else if (strcasecmp(child->key, "collect") == 0) {
            status = cf_util_get_flags(child, xfs_flags_list,
                                       STATIC_ARRAY_SIZE(xfs_flags_list), &xfs_flags);
        }

        if (status != 0)
            return -1;
    }

    return status;
}

static int xfs_init(void)
{
    path_sys_xfs = plugin_syspath("fs/xfs");
    if (path_sys_xfs == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int xfs_shutdown(void)
{
    exclist_reset(&xfs_excl_device);
    free(path_sys_xfs);
    return 0;
}

void module_register(void)
{
    plugin_register_init("xfs", xfs_init);
    plugin_register_config("xfs", xfs_config);
    plugin_register_read("xfs", xfs_read);
    plugin_register_shutdown("xfs", xfs_shutdown);
}
