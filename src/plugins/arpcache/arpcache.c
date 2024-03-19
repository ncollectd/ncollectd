// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

enum {
    FAM_ARP_CACHE_ENTRIES,
    FAM_ARP_CACHE_ALLOCS,
    FAM_ARP_CACHE_DESTROYS,
    FAM_ARP_CACHE_HASH_GROWS,
    FAM_ARP_CACHE_LOOKUPS,
    FAM_ARP_CACHE_HITS,
    FAM_ARP_CACHE_RES_FAILED,
    FAM_ARP_CACHE_PERIODIC_GC_RUNS,
    FAM_ARP_CACHE_FORCED_GC_RUNS,
    FAM_ARP_CACHE_UNRESOLVED_DISCARDS,
    FAM_ARP_CACHE_TABLE_FULLS,
    FAM_ARP_CACHE_MAX
};

static metric_family_t fams_arp_cache[FAM_ARP_CACHE_MAX] = {
    [FAM_ARP_CACHE_ENTRIES] = {
        .name = "system_arp_cache_entries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of entries in the neighbor table.",
    },
    [FAM_ARP_CACHE_ALLOCS] = {
        .name = "system_arp_cache_allocs",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many neighbor entries have been allocated.",
    },
    [FAM_ARP_CACHE_DESTROYS] = {
        .name = "system_arp_cache_destroys",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many neighbor entries have been removed.",
    },
    [FAM_ARP_CACHE_HASH_GROWS] = {
        .name = "system_arp_cache_hash_grows",
        .type = METRIC_TYPE_COUNTER,
        .help = "How often the neighbor (hash) table was increased.",
    },
    [FAM_ARP_CACHE_LOOKUPS] = {
        .name = "system_arp_cache_lookups",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many lookups were performed.",
    },
    [FAM_ARP_CACHE_HITS] = {
        .name = "system_arp_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many lookups were successful.",
    },
    [FAM_ARP_CACHE_RES_FAILED] = {
        .name = "system_arp_cache_res_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many neighbor lookups failed.",
    },
    [FAM_ARP_CACHE_PERIODIC_GC_RUNS] = {
        .name = "system_arp_cache_periodic_gc_runs",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many garbage collection runs were executed.",
    },
    [FAM_ARP_CACHE_FORCED_GC_RUNS] = {
        .name = "system_arp_cache_forced_gc_runs",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many forced garbage collection runs were executed.",
    },
    [FAM_ARP_CACHE_UNRESOLVED_DISCARDS] = {
        .name = "system_arp_cache_unresolved_discards",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many neighbor table entries were discarded due to lookup failure.",
    },
    [FAM_ARP_CACHE_TABLE_FULLS] = {
        .name = "system_arp_cache_table_fulls",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of table overflows.",
    },
};

enum {
    FAM_NDISC_CACHE_ENTRIES,
    FAM_NDISC_CACHE_ALLOCS,
    FAM_NDISC_CACHE_DESTROYS,
    FAM_NDISC_CACHE_HASH_GROWS,
    FAM_NDISC_CACHE_LOOKUPS,
    FAM_NDISC_CACHE_HITS,
    FAM_NDISC_CACHE_RES_FAILED,
    FAM_NDISC_CACHE_RCV_PROBES_MCAST,
    FAM_NDISC_CACHE_RCV_PROBES_UCAST,
    FAM_NDISC_CACHE_PERIODIC_GC_RUNS,
    FAM_NDISC_CACHE_FORCED_GC_RUNS,
    FAM_NDISC_CACHE_UNRESOLVED_DISCARDS,
    FAM_NDISC_CACHE_TABLE_FULLS,
    FAM_NDISC_CACHE_MAX
};

static metric_family_t fams_ndisc_cache[FAM_NDISC_CACHE_MAX] = {
    [FAM_NDISC_CACHE_ENTRIES] = {
        .name = "system_ndisc_cache_entries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of entries in the neighbor table.",
    },
    [FAM_NDISC_CACHE_ALLOCS] = {
        .name = "system_ndisc_cache_allocs",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many neighbor entries have been allocated.",
    },
    [FAM_NDISC_CACHE_DESTROYS] = {
        .name = "system_ndisc_cache_destroys",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many neighbor entries have been removed.",
    },
    [FAM_NDISC_CACHE_HASH_GROWS] = {
        .name = "system_ndisc_cache_hash_grows",
        .type = METRIC_TYPE_COUNTER,
        .help = "How often the neighbor (hash) table was increased.",
    },
    [FAM_NDISC_CACHE_LOOKUPS] = {
        .name = "system_ndisc_cache_lookups",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many lookups were performed.",
    },
    [FAM_NDISC_CACHE_HITS] = {
        .name = "system_ndisc_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many lookups were successful.",
    },
    [FAM_NDISC_CACHE_RES_FAILED] = {
        .name = "system_ndisc_cache_res_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many neighbor lookups failed.",
    },
    [FAM_NDISC_CACHE_RCV_PROBES_MCAST] = {
        .name = "system_ndisc_cache_rcv_probes_mcast",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many multicast neighbor solicitations were received.",
    },
    [FAM_NDISC_CACHE_RCV_PROBES_UCAST] = {
        .name = "system_ndisc_cache_rcv_probes_ucast",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many unicast neighbor solicitations were received.",
    },
    [FAM_NDISC_CACHE_PERIODIC_GC_RUNS] = {
        .name = "system_ndisc_cache_periodic_gc_runs",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many garbage collection runs were executed.",
    },
    [FAM_NDISC_CACHE_FORCED_GC_RUNS] = {
        .name = "system_ndisc_cache_forced_gc_runs",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many forced garbage collection runs were executed.",
    },
    [FAM_NDISC_CACHE_UNRESOLVED_DISCARDS] = {
        .name = "system_ndisc_cache_unresolved_discards",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many neighbor table entries were discarded due to lookup failure.",
    },
    [FAM_NDISC_CACHE_TABLE_FULLS] = {
        .name = "system_ndisc_cache_table_fulls",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of table overflows.",
    },
};

typedef struct {
    int field;
    int fam;
} field_fam_t;

static field_fam_t fields_ndisc_cache[] = {
    { 0,  FAM_NDISC_CACHE_ENTRIES             },
    { 1,  FAM_NDISC_CACHE_ALLOCS              },
    { 2,  FAM_NDISC_CACHE_DESTROYS            },
    { 3,  FAM_NDISC_CACHE_HASH_GROWS          },
    { 4,  FAM_NDISC_CACHE_LOOKUPS             },
    { 5,  FAM_NDISC_CACHE_HITS                },
    { 6,  FAM_NDISC_CACHE_RES_FAILED          },
    { 7,  FAM_NDISC_CACHE_RCV_PROBES_MCAST    },
    { 8,  FAM_NDISC_CACHE_RCV_PROBES_UCAST    },
    { 9,  FAM_NDISC_CACHE_PERIODIC_GC_RUNS    },
    { 10, FAM_NDISC_CACHE_FORCED_GC_RUNS      },
    { 11, FAM_NDISC_CACHE_UNRESOLVED_DISCARDS },
    { 12, FAM_NDISC_CACHE_TABLE_FULLS         },
};
static size_t fields_ndisc_cache_size = STATIC_ARRAY_SIZE(fields_ndisc_cache);

static field_fam_t fields_arp_cache[] = {
    { 0,  FAM_ARP_CACHE_ENTRIES             },
    { 1,  FAM_ARP_CACHE_ALLOCS              },
    { 2,  FAM_ARP_CACHE_DESTROYS            },
    { 3,  FAM_ARP_CACHE_HASH_GROWS          },
    { 4,  FAM_ARP_CACHE_LOOKUPS             },
    { 5,  FAM_ARP_CACHE_HITS                },
    { 6,  FAM_ARP_CACHE_RES_FAILED          },
    { 9,  FAM_ARP_CACHE_PERIODIC_GC_RUNS    },
    { 10, FAM_ARP_CACHE_FORCED_GC_RUNS      },
    { 11, FAM_ARP_CACHE_UNRESOLVED_DISCARDS },
    { 12, FAM_ARP_CACHE_TABLE_FULLS         },
};
static size_t fields_arp_cache_size = STATIC_ARRAY_SIZE(fields_arp_cache);

static char *path_proc_ndisc_cache;
static char *path_proc_arp_cache;

static int cache_read(const char *file, metric_family_t *fams,  size_t fams_size,
                                        field_fam_t *field_fam, size_t field_fam_size)
{
    FILE *fh = fopen(file, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Unable to open %s", file);
        return -1;
    }

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fh) == NULL) {
        fclose(fh);
        PLUGIN_WARNING("Unable to read  %s", file);
        return -1;
    }

    char *fields[13];
    for (int ncpu = 0; fgets(buffer, sizeof(buffer), fh) != NULL ; ncpu++) {
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

        if (fields_num < 13)
            continue;

        char cpu[64];
        ssnprintf(cpu, sizeof(cpu), "%d", ncpu);

        for (size_t i = 0; i < field_fam_size; i++) {
            uint64_t n = strtoull(fields[field_fam[i].field], NULL, 16);
            metric_family_append(&fams[field_fam[i].fam], VALUE_COUNTER(n), NULL,
                                 &(label_pair_const_t){.name="cpu", .value=cpu}, NULL);
        }
    }
    fclose(fh);

    plugin_dispatch_metric_family_array(fams, fams_size, 0);
    return 0;
}

static int arp_cache_read(void)
{
    int status = cache_read(path_proc_arp_cache, fams_arp_cache, FAM_ARP_CACHE_MAX,
                                            fields_arp_cache, fields_arp_cache_size);
    status |=  cache_read(path_proc_ndisc_cache, fams_ndisc_cache, FAM_NDISC_CACHE_MAX,
                                            fields_ndisc_cache, fields_ndisc_cache_size);
    return status;
}

static int arp_cache_init(void)
{
    path_proc_ndisc_cache = plugin_procpath("net/stat/ndisc_cache");
    if (path_proc_ndisc_cache == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    path_proc_arp_cache = plugin_procpath("net/stat/arp_cache");
    if (path_proc_arp_cache == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int arp_cache_shutdown(void)
{
    free(path_proc_ndisc_cache);
    free(path_proc_arp_cache);
    return 0;
}

void module_register(void)
{
    plugin_register_init("arpcache", arp_cache_init);
    plugin_register_read("arpcache", arp_cache_read);
    plugin_register_shutdown("arpcache", arp_cache_shutdown);
}
