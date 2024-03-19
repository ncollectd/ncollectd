// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2009  Anthony Dewhurst
// SPDX-FileCopyrightText: Copyright (C) 2012  Aurelien Rougemont
// SPDX-FileCopyrightText: Copyright (C) 2013  Xin Li
// SPDX-FileCopyrightText: Copyright (C) 2014  Marc Fournier
// SPDX-FileCopyrightText: Copyright (C) 2014  Wilfried Goesgens
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Anthony Dewhurst <dewhurst at gmail>
// SPDX-FileContributor: Aurelien Rougemont <beorn at gandi.net>
// SPDX-FileContributor: Xin Li <delphij at FreeBSD.org>
// SPDX-FileContributor: Marc Fournier <marc.fournier at camptocamp.com>
// SPDX-FileContributor: Wilfried Goesgens <dothebart at citadel.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include "zfs_flags.h"

static cf_flags_t zfs_flags_list[] = {
    {"abdstats",          COLLECT_ABDSTATS          },
    {"arcstats",          COLLECT_ARCSTATS          },
    {"dbufstats",         COLLECT_DBUFSTATS         },
    {"dmu_tx",            COLLECT_DMU_TX            },
    {"dnodestats",        COLLECT_DNODESTATS        },
    {"fm",                COLLECT_FM                },
    {"qat",               COLLECT_QAT               },
    {"vdev_cache_stats",  COLLECT_VDEV_CACHE_STATS  },
    {"vdev_mirror_stats", COLLECT_VDEV_MIRROR_STATS },
    {"xuio_stats",        COLLECT_XUIO_STATS        },
    {"zfetchstats",       COLLECT_ZFETCHSTATS       },
    {"zil",               COLLECT_ZIL               },
    {"state",             COLLECT_STATE             },
    {"io",                COLLECT_IO                },
    {"objset",            COLLECT_OBJSET            },
};
static size_t zfs_flags_list_size = STATIC_ARRAY_SIZE(zfs_flags_list);

uint64_t zfs_flags = COLLECT_ARCSTATS;

__attribute__(( weak ))
int zfs_read(void)
{
    return 0;
}

static int zfs_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "collect") == 0) {
            status = cf_util_get_flags(child, zfs_flags_list, zfs_flags_list_size, &zfs_flags);
        }

        if (status != 0)
            return -1;
    }

    return status;
}

__attribute__(( weak ))
int zfs_init(void)
{
    return 0;
}

__attribute__(( weak ))
int zfs_shutdown(void)
{
    return 0;
}

void module_register(void)
{
    plugin_register_init("zfs", zfs_init);
    plugin_register_config("zfs", zfs_config);
    plugin_register_read("zfs", zfs_read);
    plugin_register_shutdown("zfs", zfs_shutdown);
}
