// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

enum {
    FAM_KSM_PAGES_TO_SCAN,
    FAM_KSM_SLEEP_SECONDS,
    FAM_KSM_MERGE_ACROSS_NODES,
    FAM_KSM_RUN,
    FAM_KSM_MAX_PAGE_SHARING,
    FAM_KSM_STABLE_NODE_CHAINS_PRUNE_SECONDS,
    FAM_KSM_USE_ZERO_PAGES,
    FAM_KSM_PAGES_SHARED,
    FAM_KSM_PAGES_SHARING,
    FAM_KSM_PAGES_UNSHARED,
    FAM_KSM_PAGES_VOLATILE,
    FAM_KSM_FULL_SCANS,
    FAM_KSM_STABLE_NODE_CHAINS,
    FAM_KSM_STABLE_NODE_DUPS,
    FAM_KSM_MAX,
};

static metric_family_t fams[FAM_KSM_MAX] = {
    [FAM_KSM_PAGES_TO_SCAN] = {
        .name = "system_ksm_pages_to_scan",
        .type = METRIC_TYPE_GAUGE,
        .help = "How many pages to scan before ksmd goes to sleep.",
    },
    [FAM_KSM_SLEEP_SECONDS] = {
        .name = "system_ksm_sleep_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "How many seconds ksmd should sleep before next scan.",
    },
    [FAM_KSM_MERGE_ACROSS_NODES] = {
        .name = "system_ksm_merge_across_nodes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Specifies if pages from different NUMA nodes can be merged.",
    },
    [FAM_KSM_RUN] = {
        .name = "system_ksm_run",
        .type = METRIC_TYPE_GAUGE,
        .help = "When set to 0, stop ksmd from running but keep merged pages. "
                "When set to 1 run ksmd. When set to 2 stop ksmd and unmerge all pages "
                "currently merged, but leave mergeable areas registered for next run.",
    },
    [FAM_KSM_MAX_PAGE_SHARING] = {
        .name = "system_ksm_max_page_sharing",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum sharing allowed for each KSM page."
    },
    [FAM_KSM_STABLE_NODE_CHAINS_PRUNE_SECONDS] = {
        .name = "system_ksm_stable_node_chains_prune_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Specifies how frequently KSM checks the metadata of the pages that "
                "hit the deduplication limit for stale information."
    },
    [FAM_KSM_USE_ZERO_PAGES] = {
        .name = "system_ksm_use_zero_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "When set to 1, empty pages are merged with the kernel zero page(s) "
                "instead of with each other as it would happen normally."
    },
    [FAM_KSM_PAGES_SHARED] = {
        .name = "system_ksm_pages_shared",
        .type = METRIC_TYPE_GAUGE,
        .help = "How many shared pages are being used.",
    },
    [FAM_KSM_PAGES_SHARING] = {
        .name = "system_ksm_pages_sharing",
        .type = METRIC_TYPE_GAUGE,
        .help = "How many more sites are sharing them i.e. how much saved.",
    },
    [FAM_KSM_PAGES_UNSHARED] = {
        .name = "system_ksm_pages_unshared",
        .type = METRIC_TYPE_GAUGE,
        .help = "How many pages unique but repeatedly checked for merging.",
    },
    [FAM_KSM_PAGES_VOLATILE] = {
        .name = "system_ksm_pages_volatile",
        .type = METRIC_TYPE_GAUGE,
        .help = "How many pages changing too fast to be placed in a tree.",
    },
    [FAM_KSM_FULL_SCANS] = {
        .name = "system_ksm_full_scans",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times all mergeable areas have been scanned.",
    },
    [FAM_KSM_STABLE_NODE_CHAINS] = {
        .name = "system_ksm_stable_node_chains",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of KSM pages that hit the max_page_sharing limit.",
    },
    [FAM_KSM_STABLE_NODE_DUPS] = {
        .name = "system_ksm_stable_node_dups",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of duplicated KSM pages.",
    },
};

static char *path_sys_ksm;

struct ksm_files {
    double scale;
    char *file;
    unsigned int fam;
};
static struct ksm_files ksm_files[] = {
    { 1,     "pages_to_scan",                      FAM_KSM_PAGES_TO_SCAN                    },
    { 0.001, "sleep_millisecs",                    FAM_KSM_SLEEP_SECONDS                    },
    { 1,     "merge_across_nodes",                 FAM_KSM_MERGE_ACROSS_NODES               },
    { 1,     "run",                                FAM_KSM_RUN                              },
    { 1,     "max_page_sharing",                   FAM_KSM_MAX_PAGE_SHARING                 },
    { 0.001, "stable_node_chains_prune_millisecs", FAM_KSM_STABLE_NODE_CHAINS_PRUNE_SECONDS },
    { 1,     "full_scans",                         FAM_KSM_USE_ZERO_PAGES                   },
    { 1,     "pages_shared",                       FAM_KSM_PAGES_SHARED                     },
    { 1,     "pages_sharing",                      FAM_KSM_PAGES_SHARING                    },
    { 1,     "pages_unshared",                     FAM_KSM_PAGES_UNSHARED                   },
    { 1,     "pages_volatile",                     FAM_KSM_PAGES_VOLATILE                   },
    { 1,     "stable_node_chains",                 FAM_KSM_FULL_SCANS                       },
    { 1,     "stable_node_dups",                   FAM_KSM_STABLE_NODE_CHAINS               },
    { 1,     "use_zero_pages",                     FAM_KSM_STABLE_NODE_DUPS                 },
};

static int ksm_read(void)
{
    int ksm_dir = open(path_sys_ksm, O_RDONLY | O_DIRECTORY);
    if (ksm_dir < 0) {
        PLUGIN_ERROR("Cannot open '%s': %s",  path_sys_ksm, STRERRNO);
        return -1;
    }

    for(size_t i = 0; i < STATIC_ARRAY_SIZE(ksm_files); i++) {
         value_t value = {0};
         metric_family_t *fam = &fams[ksm_files[i].fam];
         if (fam->type == METRIC_TYPE_COUNTER) {
             uint64_t raw = 0;
             int status = filetouint_at(ksm_dir, ksm_files[i].file, &raw);
             if (status != 0)
                 continue;
             value = VALUE_COUNTER(raw);
         } else if (fam->type == METRIC_TYPE_GAUGE) {
             double raw = 0;
             int status = filetodouble_at(ksm_dir, ksm_files[i].file, &raw);
             if (status != 0)
                 continue;
             value = VALUE_GAUGE(raw * ksm_files[i].scale);
         }
         metric_family_append(fam, value, NULL, NULL);
    }

    plugin_dispatch_metric_family_array(fams, FAM_KSM_MAX, 0);

    close(ksm_dir);
    return 0;
}

static int ksm_init(void)
{
    path_sys_ksm = plugin_syspath("kernel/mm/ksm");
    if (path_sys_ksm == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int ksm_shutdown(void)
{
    free(path_sys_ksm);
    return 0;
}

void module_register(void)
{
    plugin_register_init("ksm", ksm_init);
    plugin_register_read("ksm", ksm_read);
    plugin_register_shutdown("ksm", ksm_shutdown);
}
