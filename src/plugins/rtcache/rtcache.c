// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

enum {
    FAM_RT_CACHE_ENTRIES,
    FAM_RT_CACHE_IN_SLOW_TOT,
    FAM_RT_CACHE_IN_SLOW_MC,
    FAM_RT_CACHE_IN_NO_ROUTE,
    FAM_RT_CACHE_IN_BRD,
    FAM_RT_CACHE_IN_MARTIAN_DST,
    FAM_RT_CACHE_IN_MARTIAN_SRC,
    FAM_RT_CACHE_OUT_SLOW_TOT,
    FAM_RT_CACHE_OUT_SLOW_MC,
    FAM_RT_CACHE_MAX
};

static metric_family_t fams_rt_cache[FAM_RT_CACHE_MAX] = {
    [FAM_RT_CACHE_ENTRIES] = {
        .name = "system_rt_cache_entries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of entries in routing cache.",
    },
    [FAM_RT_CACHE_IN_SLOW_TOT] = {
        .name = "system_rt_cache_in_slow_tot",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of routing cache entries added for input traffic.",
    },
    [FAM_RT_CACHE_IN_SLOW_MC] = {
        .name = "system_rt_cache_in_slow_mc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of multicast routing cache entries added for input traffic.",
    },
    [FAM_RT_CACHE_IN_NO_ROUTE] = {
        .name = "system_rt_cache_in_no_route",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of input packets for which no routing table entry was found.",
    },
    [FAM_RT_CACHE_IN_BRD] = {
        .name = "system_rt_cache_in_brd",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of matched input broadcast packets.",
    },
    [FAM_RT_CACHE_IN_MARTIAN_DST] = {
        .name = "system_rt_cache_in_martian_dst",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of incoming martian destination packets.",
    },
    [FAM_RT_CACHE_IN_MARTIAN_SRC] = {
        .name = "system_rt_cache_in_martian_src",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of incoming martian source packets.",
    },
    [FAM_RT_CACHE_OUT_SLOW_TOT] = {
        .name = "system_rt_cache_out_slow_tot",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of routing cache entries added for output traffic.",
    },
    [FAM_RT_CACHE_OUT_SLOW_MC] = {
        .name = "system_rt_cache_out_slow_mc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of multicast routing cache entries added for output traffic.",
    },
};

typedef struct {
    int field;
    int fam;
} field_fam_t;

static field_fam_t field_fam[] = {
    {  0, FAM_RT_CACHE_ENTRIES        },
    {  2, FAM_RT_CACHE_IN_SLOW_TOT    },
    {  3, FAM_RT_CACHE_IN_SLOW_MC     },
    {  4, FAM_RT_CACHE_IN_NO_ROUTE    },
    {  5, FAM_RT_CACHE_IN_BRD         },
    {  6, FAM_RT_CACHE_IN_MARTIAN_DST },
    {  7, FAM_RT_CACHE_IN_MARTIAN_SRC },
    {  9, FAM_RT_CACHE_OUT_SLOW_TOT   },
    { 10, FAM_RT_CACHE_OUT_SLOW_MC    }
};
static size_t field_fam_size = STATIC_ARRAY_SIZE(fams_rt_cache);

static char *path_proc_rt_cache;

static int rt_cache_read(void)
{
    FILE *fh = fopen(path_proc_rt_cache, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Unable to open '%s': %s", path_proc_rt_cache, STRERRNO);
        return -1;
    }

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fh) == NULL) {
        fclose(fh);
        PLUGIN_WARNING("Unable to read  '%s'", path_proc_rt_cache);
        return -1;
    }

    char *fields[17];
    for (int ncpu = 0; fgets(buffer, sizeof(buffer), fh) != NULL ; ncpu++) {
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

        if (fields_num < 17)
            continue;

        char cpu[64];
        ssnprintf(cpu, sizeof(cpu), "%d", ncpu);

        for (size_t i = 0; i < field_fam_size; i++) {
            uint64_t val = (uint64_t)strtoull(fields[field_fam[i].field], NULL, 16);
            metric_family_append(&fams_rt_cache[field_fam[i].fam], VALUE_COUNTER(val), NULL,
                                 &(label_pair_const_t){.name="cpu", .value=cpu}, NULL);
        }
    }
    fclose(fh);

    plugin_dispatch_metric_family_array(fams_rt_cache, FAM_RT_CACHE_MAX, 0);
    return 0;
}

static int rt_cache_init(void)
{
    path_proc_rt_cache = plugin_procpath("net/stat/rt_cache");
    if (path_proc_rt_cache == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int rt_cache_shutdown(void)
{
    free(path_proc_rt_cache);
    return 0;
}

void module_register(void)
{
    plugin_register_init("rtcache", rt_cache_init);
    plugin_register_read("rtcache", rt_cache_read);
    plugin_register_shutdown("rtcache", rt_cache_shutdown);
}
