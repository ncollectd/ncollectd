// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

enum {
    FAM_ZONEINFO_FREE_PAGES,
    FAM_ZONEINFO_MIN_PAGES,
    FAM_ZONEINFO_LOW_PAGES,
    FAM_ZONEINFO_HIGH_PAGES,
    FAM_ZONEINFO_SCANNED_PAGES,
    FAM_ZONEINFO_SPANNED_PAGES,
    FAM_ZONEINFO_PRESENT_PAGES,
    FAM_ZONEINFO_MANAGED_PAGES,
    FAM_ZONEINFO_ACTIVE_ANON_PAGES,
    FAM_ZONEINFO_INACTIVE_ANON_PAGES,
    FAM_ZONEINFO_ISOLATED_ANON_PAGES,
    FAM_ZONEINFO_ANON_PAGES,
    FAM_ZONEINFO_ANON_TRANSPARENT_HUGEPAGES,
    FAM_ZONEINFO_ACTIVE_FILE_PAGES,
    FAM_ZONEINFO_INACTIVE_FILE_PAGES,
    FAM_ZONEINFO_ISOLATED_FILE_PAGES,
    FAM_ZONEINFO_FILE_PAGES,
    FAM_ZONEINFO_SLAB_RECLAIMABLE_PAGES,
    FAM_ZONEINFO_SLAB_UNRECLAIMABLE_PAGES,
    FAM_ZONEINFO_MLOCK_PAGES,
    FAM_ZONEINFO_KERNEL_STACKS,
    FAM_ZONEINFO_KERNEL_STACK_BYTES,
    FAM_ZONEINFO_MAPPED_PAGES,
    FAM_ZONEINFO_DIRTY_PAGES,
    FAM_ZONEINFO_WRITEBACK_PAGES,
    FAM_ZONEINFO_UNEVICTABLE_PAGES,
    FAM_ZONEINFO_SHMEM_PAGES,
    FAM_ZONEINFO_NR_DIRTIED,
    FAM_ZONEINFO_NR_WRITTEN,
    FAM_ZONEINFO_NUMA_HIT,
    FAM_ZONEINFO_NUMA_MISS,
    FAM_ZONEINFO_NUMA_FOREIGN,
    FAM_ZONEINFO_NUMA_INTERLEAVE,
    FAM_ZONEINFO_NUMA_LOCAL,
    FAM_ZONEINFO_NUMA_OTHER,
    FAM_ZONEINFO_MAX,
};

#include "plugins/zoneinfo/zoneinfo.h"

static metric_family_t zoneinfo_fams[FAM_ZONEINFO_MAX] = {
    [FAM_ZONEINFO_FREE_PAGES] = {
        .name = "system_zoneinfo_free_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of free pages in the zone.",
    },
    [FAM_ZONEINFO_MIN_PAGES] = {
        .name = "system_zoneinfo_min_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Zone watermark pages_min.",
    },
    [FAM_ZONEINFO_LOW_PAGES] = {
        .name = "system_zoneinfo_low_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Zone watermark pages_low.",
    },
    [FAM_ZONEINFO_HIGH_PAGES] = {
        .name = "system_zoneinfo_high_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Zone watermark pages_high.",
    },
    [FAM_ZONEINFO_SCANNED_PAGES] = {
        .name = "system_zoneinfo_scanned_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Pages scanned since last reclaim.",
    },
    [FAM_ZONEINFO_SPANNED_PAGES] = {
        .name = "system_zoneinfo_spanned_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total pages spanned by the zone, including holes.",
    },
    [FAM_ZONEINFO_PRESENT_PAGES] = {
        .name = "system_zoneinfo_present_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Physical pages existing within the zone.",
    },
    [FAM_ZONEINFO_MANAGED_PAGES] = {
        .name = "system_zoneinfo_managed_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Present pages managed by the buddy system.",
    },
    [FAM_ZONEINFO_ACTIVE_ANON_PAGES] = {
        .name = "system_zoneinfo_active_anon_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of anonymous pages recently more used.",
    },
    [FAM_ZONEINFO_INACTIVE_ANON_PAGES] = {
        .name = "system_zoneinfo_inactive_anon_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of anonymous pages recently less used.",
    },
    [FAM_ZONEINFO_ISOLATED_ANON_PAGES] = {
        .name = "system_zoneinfo_isolated_anon_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Temporary isolated pages from anon lru.",
    },
    [FAM_ZONEINFO_ANON_PAGES] = {
        .name = "system_zoneinfo_anon_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of anonymous pages currently used by the system.",
    },
    [FAM_ZONEINFO_ANON_TRANSPARENT_HUGEPAGES] = {
        .name = "system_zoneinfo_anon_transparent_hugepages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of anonymous transparent huge pages currently used by the system.",
    },
    [FAM_ZONEINFO_ACTIVE_FILE_PAGES] = {
        .name = "system_zoneinfo_active_file_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of active pages with file-backing.",
    },
    [FAM_ZONEINFO_INACTIVE_FILE_PAGES] = {
        .name = "system_zoneinfo_inactive_file_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of inactive pages with file-backing.",
    },
    [FAM_ZONEINFO_ISOLATED_FILE_PAGES] = {
        .name = "system_zoneinfo_isolated_file_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Temporary isolated pages from file lru.",
    },
    [FAM_ZONEINFO_FILE_PAGES] = {
        .name = "system_zoneinfo_file_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of file pages.",
    },
    [FAM_ZONEINFO_SLAB_RECLAIMABLE_PAGES] = {
        .name = "system_zoneinfo_slab_reclaimable_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of reclaimable slab pages.",
    },
    [FAM_ZONEINFO_SLAB_UNRECLAIMABLE_PAGES] = {
        .name = "system_zoneinfo_slab_unreclaimable_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of unreclaimable slab pages.",
    },
    [FAM_ZONEINFO_MLOCK_PAGES] = {
        .name = "system_zoneinfo_mlock_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of mlock()ed pages found and moved off LRU.",
    },
    [FAM_ZONEINFO_KERNEL_STACKS] = {
        .name = "system_zoneinfo_kernel_stacks",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of kernel stacks.",
    },
    [FAM_ZONEINFO_KERNEL_STACK_BYTES] = {
        .name = "system_zoneinfo_kernel_stack_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory consumed by the kernel stacks in bytes.",
    },
    [FAM_ZONEINFO_MAPPED_PAGES] = {
        .name = "system_zoneinfo_mapped_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of mapped pages.",
    },
    [FAM_ZONEINFO_DIRTY_PAGES] = {
        .name = "system_zoneinfo_dirty_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of dirty pages.",
    },
    [FAM_ZONEINFO_WRITEBACK_PAGES] = {
        .name = "system_zoneinfo_writeback_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of writeback pages.",
    },
    [FAM_ZONEINFO_UNEVICTABLE_PAGES] = {
        .name = "system_zoneinfo_unevictable_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of unevictable pages.",
    },
    [FAM_ZONEINFO_SHMEM_PAGES] = {
        .name = "system_zoneinfo_shmem_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of shmem pages (included tmpfs/GEM pages).",
    },
    [FAM_ZONEINFO_NR_DIRTIED] = {
        .name = "system_zoneinfo_nr_dirtied",
        .type = METRIC_TYPE_COUNTER,
        .help = "Page dirtyings since bootup.",
    },
    [FAM_ZONEINFO_NR_WRITTEN] = {
        .name = "system_zoneinfo_nr_written",
        .type = METRIC_TYPE_COUNTER,
        .help = "Page writings since bootup.",
    },
    [FAM_ZONEINFO_NUMA_HIT] = {
        .name = "system_zoneinfo_numa_hit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Allocated in intended node.",
    },
    [FAM_ZONEINFO_NUMA_MISS] = {
        .name = "system_zoneinfo_numa_miss",
        .type = METRIC_TYPE_COUNTER,
        .help = "Allocated in non intended node.",
    },
    [FAM_ZONEINFO_NUMA_FOREIGN] = {
        .name = "system_zoneinfo_numa_foreign",
        .type = METRIC_TYPE_COUNTER,
        .help = "Was intended here, hit elsewhere.",
    },
    [FAM_ZONEINFO_NUMA_INTERLEAVE] = {
        .name = "system_zoneinfo_numa_interleave",
        .type = METRIC_TYPE_COUNTER,
        .help = "Interleaver preferred this zone.",
    },
    [FAM_ZONEINFO_NUMA_LOCAL] = {
        .name = "system_zoneinfo_numa_local",
        .type = METRIC_TYPE_COUNTER,
        .help = "Allocation from local node.",
    },
    [FAM_ZONEINFO_NUMA_OTHER] = {
        .name = "system_zoneinfo_numa_other",
        .type = METRIC_TYPE_COUNTER,
        .help = "Allocation from other node.",
    },
};

static char *path_proc_zoneinfo;

static int zoneinfo_read(void)
{
    FILE *fh = fopen(path_proc_zoneinfo, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Unable to open '%s': %s", path_proc_zoneinfo, STRERRNO);
        return -1;
    }

    char buffer[256];
    char node[32] = {'\0'};
    char zone[64] = {'\0'};
    bool per_node = false;
    bool has_node_stats = false;
    char *fields[6];
    while(fgets(buffer, sizeof(buffer), fh) != NULL) {
        if (strncmp(buffer, "Node ", strlen("Node ")) == 0) {
            per_node = false;
            int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
            if (fields_num != 4)
                continue;

            char *ptr = fields[1];
            while (isdigit(*ptr))
                ptr++;
            *ptr = '\0';

            sstrncpy(node, fields[1], sizeof(node));
            sstrncpy(zone, fields[3], sizeof(zone));

            continue;
        }

        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

        /* Since Linux 4.8 the node statistics are printed between "per-node stats"
         * and "pages free" in the first populated zone of each node. */
        if ((fields_num == 2) && (strcmp(fields[0], "per-node") == 0) &&
            (strcmp(fields[1], "stats") == 0)) {
            per_node = true;
            has_node_stats = true;
            continue;
        }

        if ((fields_num == 3) && (strcmp(fields[0], "pages") == 0) &&
            (strcmp(fields[1], "free") == 0)) {
            per_node = false;
            continue;
        }

        if (fields_num != 2)
            continue;

        const struct zoneinfo_metric *zoneinfo = zoneinfo_get_key (fields[0], strlen(fields[0]));
        if (zoneinfo == NULL)
            continue;
        if (zoneinfo->fam < 0)
            continue;

        int fam = zoneinfo->fam;
        /* Before Linux 4.8 (the release that added the "per-node stats" section)
         * nr_kernel_stack was the number of kernel stacks instead of KiB. */
        if ((fam == FAM_ZONEINFO_KERNEL_STACK_BYTES) && !has_node_stats)
            fam = FAM_ZONEINFO_KERNEL_STACKS;

        const char *zone_label = per_node ? NULL : zone;

        uint64_t value = 0;
        if (strtouint(fields[1], &value) != 0)
            continue;

        if (zoneinfo_fams[fam].type == METRIC_TYPE_COUNTER) {
            metric_family_append(&zoneinfo_fams[fam], VALUE_COUNTER(value), NULL,
                                &LABEL_PAIR_CONST("node", node),
                                &LABEL_PAIR_CONST("zone", zone_label), NULL);
        } else if (zoneinfo_fams[fam].type == METRIC_TYPE_GAUGE) {
            if (fam == FAM_ZONEINFO_KERNEL_STACK_BYTES)
                value *= 1024;
            metric_family_append(&zoneinfo_fams[fam], VALUE_GAUGE(value), NULL,
                                &LABEL_PAIR_CONST("node", node),
                                &LABEL_PAIR_CONST("zone", zone_label), NULL);
        }

    }

    fclose(fh);

    plugin_dispatch_metric_family_array(zoneinfo_fams, FAM_ZONEINFO_MAX, 0);
    return 0;
}


static int zoneinfo_init(void)
{
    path_proc_zoneinfo = plugin_procpath("zoneinfo");
    if (path_proc_zoneinfo == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int zoneinfo_shutdown(void)
{
    free(path_proc_zoneinfo);
    return 0;
}

void module_register(void)
{
    plugin_register_init("zoneinfo", zoneinfo_init);
    plugin_register_read("zoneinfo", zoneinfo_read);
    plugin_register_shutdown("zoneinfo", zoneinfo_shutdown);
}
