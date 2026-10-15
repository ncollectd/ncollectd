// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#define COLLECT_XFS_EXTENT_ALLOC (1ULL << 0)
#define COLLECT_XFS_ABT          (1ULL << 1)  
#define COLLECT_XFS_BLK_MAP      (1ULL << 2)
#define COLLECT_XFS_BMBT         (1ULL << 3)
#define COLLECT_XFS_DIR          (1ULL << 4)
#define COLLECT_XFS_TRANS        (1ULL << 5)
#define COLLECT_XFS_IG           (1ULL << 6)
#define COLLECT_XFS_LOG          (1ULL << 7)
#define COLLECT_XFS_PUSH_AIL     (1ULL << 8)
#define COLLECT_XFS_XSTRAT       (1ULL << 9)
#define COLLECT_XFS_RW           (1ULL << 10)
#define COLLECT_XFS_ATTR         (1ULL << 11)
#define COLLECT_XFS_ICLUSTER     (1ULL << 12)
#define COLLECT_XFS_VNODES       (1ULL << 13)
#define COLLECT_XFS_BUF          (1ULL << 14)
#define COLLECT_XFS_ABTB2        (1ULL << 15)
#define COLLECT_XFS_ABTC2        (1ULL << 16)
#define COLLECT_XFS_BMBT2        (1ULL << 17)
#define COLLECT_XFS_IBT2         (1ULL << 18)
#define COLLECT_XFS_FIBT2        (1ULL << 19)
#define COLLECT_XFS_RMAPBT       (1ULL << 20)
#define COLLECT_XFS_REFCNTBT     (1ULL << 21)
#define COLLECT_XFS_RMAPBT_MEM   (1ULL << 22)
#define COLLECT_XFS_RCBAGBT      (1ULL << 23)
#define COLLECT_XFS_RTRMAPBT     (1ULL << 24)
#define COLLECT_XFS_RTRMAPBT_MEM (1ULL << 25)
#define COLLECT_XFS_RTREFCNTBT   (1ULL << 26)
#define COLLECT_XFS_QM           (1ULL << 27)
#define COLLECT_XFS_ZONED        (1ULL << 28)
#define COLLECT_XFS_METAFILE     (1ULL << 29)
#define COLLECT_XFS_XPC          (1ULL << 30)
#define COLLECT_XFS_DEFER_RELOG  (1ULL << 31)
#define COLLECT_XFS_GC_XPC       (1ULL << 32)

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
    {"rMapBtMem"  , COLLECT_XFS_RMAPBT_MEM   },
    {"RcBagBt"    , COLLECT_XFS_RCBAGBT      },
    {"RtrMapBt"   , COLLECT_XFS_RTRMAPBT     },
    {"RtrMapBtMem", COLLECT_XFS_RTRMAPBT_MEM },
    {"RtRedCntBt" , COLLECT_XFS_RTREFCNTBT   },
    {"Qm"         , COLLECT_XFS_QM           },
    {"Zoned"      , COLLECT_XFS_ZONED        },
    {"MetaFile"   , COLLECT_XFS_METAFILE     },
    {"Xpc"        , COLLECT_XFS_XPC          },
    {"defer_relog", COLLECT_XFS_DEFER_RELOG  },
    {"GcXpc"      , COLLECT_XFS_GC_XPC       },
};

#include "plugins/xfs/xfs_stats_fam.h"
#include "plugins/xfs/xfs_stats.h"

static char *path_sys_xfs;
static exclist_t xfs_excl_device;
static plugin_filter_t *filter;
static uint64_t xfs_flags = COLLECT_XFS_RW;

static int xfs_read_stats(__attribute__((unused)) int dir_fd, char const *dir,
                          char const *file, __attribute__((unused)) void *user_data)
{
    char path[PATH_MAX];
    ssnprintf(path, sizeof(path), "%s/%s/stats/stats", dir, file);

    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return 0;

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

        int offset = 1;
        if (xsm->key2 != NULL) {
            if (strcmp(fields[1], xsm->key2) != 0)
                continue;
            if (numfields < 3)
                continue;
            offset = 2;
        }

        if (!(xfs_flags & xsm->collect))
            continue;

        size_t min_size = (numfields - offset) < (int)xsm->entries_size ?
                          (size_t)(numfields - offset)
                          : xsm->entries_size;

        for (size_t i = 0; i < min_size; i++) {
            metric_family_t *fam = &fams[xsm->entries[i]];
            uint64_t value = 0;
            if (strtouint(fields[i + offset], &value) == 0) { 
                if (fam->type == METRIC_TYPE_COUNTER)
                    metric_family_append(fam, VALUE_COUNTER(value), NULL,
                                         &LABEL_PAIR_CONST("device", file), NULL);
                else
                    metric_family_append(fam, VALUE_GAUGE(value), NULL,
                                         &LABEL_PAIR_CONST("device", file), NULL);
            }
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
    int status = walk_directory(path_sys_xfs, xfs_read_sysfs, NULL, 0);

    plugin_dispatch_metric_family_array_filtered(fams, FAM_XFS_MAX, filter, 0);

    return status;
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
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = plugin_filter_configure(child, &filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
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
    plugin_filter_free(filter);
    return 0;
}

void module_register(void)
{
    plugin_register_init("xfs", xfs_init);
    plugin_register_config("xfs", xfs_config);
    plugin_register_read("xfs", xfs_read);
    plugin_register_shutdown("xfs", xfs_shutdown);
}
