// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

static char *path_sys_avc_cache_stats;

enum {
    FAM_AVC_CACHE_LOOKUPS,
    FAM_AVC_CACHE_HITS,
    FAM_AVC_CACHE_MISSES,
    FAM_AVC_CACHE_ALLOCATIONS,
    FAM_AVC_CACHE_RECLAIMS,
    FAM_AVC_CACHE_FREES,
    FAM_AVC_CACHE_MAX,
};

static metric_family_t fams[FAM_AVC_CACHE_MAX] = {
    [FAM_AVC_CACHE_LOOKUPS] = {
        .name = "system_avc_cache_lookups",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of access vector lookups.",
    },
    [FAM_AVC_CACHE_HITS] = {
        .name = "system_avc_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of access vector hits.",
    },
    [FAM_AVC_CACHE_MISSES] = {
        .name = "system_avc_cache_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of cache misses.",
    },
    [FAM_AVC_CACHE_ALLOCATIONS] = {
        .name = "system_avc_cache_allocations",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of AVC entries allocated.",
    },
    [FAM_AVC_CACHE_RECLAIMS] = {
        .name = "system_avc_cache_reclaims",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of current total reclaimed AVC entries.",
    },
    [FAM_AVC_CACHE_FREES] = {
        .name = "system_avc_cache_frees",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of free AVC entries.",
    },
};

static int avccache_read(void)
{
    FILE *fh = fopen(path_sys_avc_cache_stats, "r");
    if (unlikely(fh == NULL)) {
        PLUGIN_WARNING("Unable to open '%s'", path_sys_avc_cache_stats);
        return EINVAL;
    }

    cdtime_t submit = cdtime();

    char buffer[256];
    char *fields[8];
    if (unlikely(fgets(buffer, sizeof(buffer), fh) == NULL)) {
        fclose(fh);
        return 0;
    }

    uint64_t total_lookups = 0;
    uint64_t total_hits = 0;
    uint64_t total_misses = 0;
    uint64_t total_allocations = 0;
    uint64_t total_reclaims = 0;
    uint64_t total_frees = 0;

    while(fgets(buffer, sizeof(buffer), fh) != NULL) {
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

        if (unlikely(fields_num < 6))
            continue;

        total_lookups     += (uint64_t)strtol(fields[0], NULL, 10);
        total_hits        += (uint64_t)strtol(fields[1], NULL, 10);
        total_misses      += (uint64_t)strtol(fields[2], NULL, 10);
        total_allocations += (uint64_t)strtol(fields[3], NULL, 10);
        total_reclaims    += (uint64_t)strtol(fields[4], NULL, 10);
        total_frees       += (uint64_t)strtol(fields[5], NULL, 10);
    }

    fclose(fh);

    metric_family_append(&fams[FAM_AVC_CACHE_LOOKUPS],
                         VALUE_COUNTER(total_lookups), NULL, NULL);
    metric_family_append(&fams[FAM_AVC_CACHE_HITS],
                         VALUE_COUNTER(total_hits), NULL, NULL);
    metric_family_append(&fams[FAM_AVC_CACHE_MISSES],
                         VALUE_COUNTER(total_misses), NULL, NULL);
    metric_family_append(&fams[FAM_AVC_CACHE_ALLOCATIONS],
                         VALUE_COUNTER(total_allocations), NULL, NULL);
    metric_family_append(&fams[FAM_AVC_CACHE_RECLAIMS],
                         VALUE_COUNTER(total_reclaims), NULL, NULL);
    metric_family_append(&fams[FAM_AVC_CACHE_FREES],
                         VALUE_COUNTER(total_frees), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_AVC_CACHE_MAX, submit);

    return 0;
}

static int avccache_init(void)
{
    path_sys_avc_cache_stats = plugin_syspath("fs/selinux/avc/cache_stats");
    if (path_sys_avc_cache_stats == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }
    return 0;
}

static int avccache_shutdown(void)
{
    free(path_sys_avc_cache_stats);
    return 0;
}

void module_register(void)
{
    plugin_register_init("avccache", avccache_init);
    plugin_register_read("avccache", avccache_read);
    plugin_register_shutdown("avccache", avccache_shutdown);
}
