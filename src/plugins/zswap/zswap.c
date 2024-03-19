// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

enum {
    FAM_ZSWAP_POOL_LIMIT_HIT,
    FAM_ZSWAP_REJECT_RECLAIM_FAIL,
    FAM_ZSWAP_REJECT_ALLOC_FAIL,
    FAM_ZSWAP_REJECT_KMEMCACHE_FAIL,
    FAM_ZSWAP_REJECT_COMPRESS_POOR,
    FAM_ZSWAP_WRITTEN_BACK_PAGES,
    FAM_ZSWAP_DUPLICATE_ENTRY,
    FAM_ZSWAP_STORED_BYTES,
    FAM_ZSWAP_POOL_TOTAL_BYTES,
    FAM_ZSWAP_SAME_FILLED_BYTES,
    FAM_ZSWAP_MAX,
};

static metric_family_t fams[FAM_ZSWAP_MAX] = {
    [FAM_ZSWAP_POOL_LIMIT_HIT] = {
        .name = "system_zswap_pool_limit_hit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pool limit was hit (see zswap_max_pool_percent module parameter).",
    },
    [FAM_ZSWAP_REJECT_RECLAIM_FAIL] = {
        .name = "system_zswap_reject_reclaim_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Store failed due to a reclaim failure after pool limit was reached.",
    },
    [FAM_ZSWAP_REJECT_ALLOC_FAIL] = {
        .name = "system_zswap_reject_alloc_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Store failed because underlying allocator could not get memory.",
    },
    [FAM_ZSWAP_REJECT_KMEMCACHE_FAIL] = {
        .name = "system_zswap_reject_kmemcache_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Store failed because the entry metadata could not be allocated (rare).",
    },
    [FAM_ZSWAP_REJECT_COMPRESS_POOR] = {
        .name = "system_zswap_reject_compress_poor",
        .type = METRIC_TYPE_COUNTER,
        .help = "Compressed page was too big for the allocator to (optimally) store.",
    },
    [FAM_ZSWAP_WRITTEN_BACK_PAGES] = {
        .name = "system_zswap_written_back_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pages written back when pool limit was reached.",
    },
    [FAM_ZSWAP_DUPLICATE_ENTRY] = {
        .name = "system_zswap_duplicate_entry",
        .type = METRIC_TYPE_COUNTER,
        .help = "Duplicate store was encountered (rare).",
    },
    [FAM_ZSWAP_STORED_BYTES] = {
        .name = "system_zswap_stored_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Compressed bytes currently stored in zswap.",
    },
    [FAM_ZSWAP_POOL_TOTAL_BYTES] = {
        .name = "system_zswap_pool_total_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total bytes used by the compressed storage.",
    },
    [FAM_ZSWAP_SAME_FILLED_BYTES] = {
        .name = "system_zswap_same_filled_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Same-value filled pages stored in zswap in bytes.",
    },
};

static uint64_t pagesize;

struct zswap_files {
    bool page;
    char *file;
    unsigned int fam;
};
static struct zswap_files zswap_files[] = {
    { false, "duplicate_entry",       FAM_ZSWAP_DUPLICATE_ENTRY       },
    { false, "pool_limit_hit",        FAM_ZSWAP_POOL_LIMIT_HIT        },
    { false, "reject_alloc_fail",     FAM_ZSWAP_REJECT_ALLOC_FAIL     },
    { false, "reject_compress_poor",  FAM_ZSWAP_REJECT_COMPRESS_POOR  },
    { false, "reject_kmemcache_fail", FAM_ZSWAP_REJECT_KMEMCACHE_FAIL },
    { false, "reject_reclaim_fail",   FAM_ZSWAP_REJECT_RECLAIM_FAIL   },
    { false, "written_back_pages",    FAM_ZSWAP_WRITTEN_BACK_PAGES    },
    { false, "pool_total_size",       FAM_ZSWAP_POOL_TOTAL_BYTES      },
    { true , "same_filled_pages",     FAM_ZSWAP_SAME_FILLED_BYTES     },
    { true , "stored_pages",          FAM_ZSWAP_STORED_BYTES          },
    { true , "pool_pages",            FAM_ZSWAP_POOL_TOTAL_BYTES      },
};

static char *path_sys_zswap;

static int zswap_read(void)
{
    int zswap_dir = open(path_sys_zswap, O_RDONLY | O_DIRECTORY);
    if (zswap_dir < 0) {
        PLUGIN_ERROR("Cannot open '%s': %s", path_sys_zswap, STRERROR(errno));
        return -1;
    }

    for(size_t i = 0; i < STATIC_ARRAY_SIZE(zswap_files); i++) {
        value_t value = {0};
        metric_family_t *fam = &fams[zswap_files[i].fam];
        if (fam->type == METRIC_TYPE_COUNTER) {
            uint64_t ret_value = 0;
            int status = filetouint_at(zswap_dir, zswap_files[i].file, &ret_value);
            if (status != 0)
                continue;
            if (zswap_files[i].page)
                value = VALUE_COUNTER(ret_value * pagesize);
        } else if (fam->type == METRIC_TYPE_GAUGE) {
            double ret_value = 0;
            int status = filetodouble_at(zswap_dir, zswap_files[i].file, &ret_value);
            if (status != 0)
                continue;
            if (zswap_files[i].page)
                value = VALUE_GAUGE(ret_value * pagesize);
        } else {
            continue;
        }
        metric_family_append(fam, value, NULL, NULL);
    }
    close(zswap_dir);

    plugin_dispatch_metric_family_array(fams, FAM_ZSWAP_MAX, 0);
    return 0;
}

static int zswap_init(void)
{
    path_sys_zswap = plugin_syspath("kernel/debug/zswap");
    if (path_sys_zswap == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    pagesize = (int64_t)sysconf(_SC_PAGESIZE);
    return 0;
}

static int zswap_shutdown(void)
{
    free(path_sys_zswap);
    return 0;
}

void module_register(void)
{
    plugin_register_init("zswap", zswap_init);
    plugin_register_read("zswap", zswap_read);
    plugin_register_shutdown("zswap", zswap_shutdown);
}
