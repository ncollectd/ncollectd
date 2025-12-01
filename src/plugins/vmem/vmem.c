// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

static char *path_proc_vmstat;
static plugin_filter_t *filter;

enum {
    FAM_VM_ZONE_PAGE_STATE,
    FAM_VM_NUMA_EVENT,
    FAM_VM_NODE_PAGE_STATE,
    FAM_VM_WORKINGSET_NODES,
    FAM_VM_PAGES_DIRTIED,
    FAM_VM_PAGES_WRITTEN,
    FAM_VM_PAGES_THROTTLED_WRITTEN,
    FAM_VM_FOLL_PIN_ACQUIRED,
    FAM_VM_FOLL_PIN_RELEASED,
    FAM_VM_KERNEL_STACK_BYTES,
    FAM_VM_SHADOW_CALL_STACK_BYTES,
    FAM_VM_DIRTY_THRESHOLD,
    FAM_VM_DIRTY_BACKGROUND_THRESHOLD,
    FAM_VM_PAGE_IN,
    FAM_VM_PAGE_OUT,
    FAM_VM_SWAP_IN,
    FAM_VM_SWAP_OUT,
    FAM_VM_PAGE_FAULTS,
    FAM_VM_MAJOR_PAGE_FAULTS,
    FAM_VM_PAGE_ALLOC,
    FAM_VM_ALLOC_STALL,
    FAM_VM_PAGE_SKIP,
    FAM_VM_PAGE_STEAL_DIRECT,
    FAM_VM_PAGE_STEAL_KSWAPD,
    FAM_VM_PAGE_STEAL_KHUGEPAGED,
    FAM_VM_PAGE_STEAL_ANON,
    FAM_VM_PAGE_STEAL_FILE,
    FAM_VM_PAGE_DEMOTE_KSWAPD,
    FAM_VM_PAGE_DEMOTE_DIRECT,
    FAM_VM_PAGE_DEMOTE_KHUGEPAGED,
    FAM_VM_PAGE_SCAN_KSWAPD,
    FAM_VM_PAGE_SCAN_DIRECT,
    FAM_VM_PAGE_SCAN_KHUGEPAGED,
    FAM_VM_PAGE_SCAN_DIRECT_THROTTLE,
    FAM_VM_PAGE_SCAN_ANON,
    FAM_VM_PAGE_SCAN_FILE,
    FAM_VM_PAGE_REFILL,
    FAM_VM_PAGE_FREE,
    FAM_VM_PAGE_ACTIVATE,
    FAM_VM_PAGE_DEACTIVATE,
    FAM_VM_PAGE_PROMOTE_SUCCESS,
    FAM_VM_PAGE_PROMOTE_CANDIDATE,
    FAM_VM_PAGE_LAZY_FREE,
    FAM_VM_PAGE_LAZY_FREED,
    FAM_VM_PAGE_REUSE,
    FAM_VM_ZONE_RECLAIM_FAILED,
    FAM_VM_PAGE_INODE_STEAL,
    FAM_VM_SLABS_SCANNED,
    FAM_VM_KSWAPD_STEAL,
    FAM_VM_KSWAPD_INODE_STEAL,
    FAM_VM_KSWAPD_LOW_WMARK_HIT_QUICKLY,
    FAM_VM_KSWAPD_HIGH_WMARK_HIT_QUICKLY,
    FAM_VM_PAGE_OUTRUN,
    FAM_VM_PAGE_ROTATED,
    FAM_VM_DROP_PAGECACHE,
    FAM_VM_DROP_SLAB,
    FAM_VM_OOM_KILL,
    FAM_VM_NUMA_PTE_UPDATES,
    FAM_VM_NUMA_HUGE_PTE_UPDATES,
    FAM_VM_NUMA_HINT_FAULTS,
    FAM_VM_NUMA_HINT_FAULTS_LOCAL,
    FAM_VM_NUMA_PAGES_MIGRATED,
    FAM_VM_PAGE_MIGRATE_SUCCESS,
    FAM_VM_PAGE_MIGRATE_FAIL,
    FAM_VM_THP_MIGRATION_SUCCESS,
    FAM_VM_THP_MIGRATION_FAIL,
    FAM_VM_THP_MIGRATION_SPLIT,
    FAM_VM_COMPACT_MIGRATE_SCANNED,
    FAM_VM_COMPACT_FREE_SCANNED,
    FAM_VM_COMPACT_ISOLATED,
    FAM_VM_COMPACT_STALL,
    FAM_VM_COMPACT_FAIL,
    FAM_VM_COMPACT_SUCCESS,
    FAM_VM_COMPACT_DAEMON_WAKE,
    FAM_VM_COMPACT_DAEMON_MIGRATE_SCANNED,
    FAM_VM_COMPACT_DAEMON_FREE_SCANNED,
    FAM_VM_HTLB_BUDDY_ALLOC_SUCCESS,
    FAM_VM_HTLB_BUDDY_ALLOC_FAIL,
    FAM_VM_CMA_ALLOC_SUCCESS,
    FAM_VM_CMA_ALLOC_FAIL,
    FAM_VM_UNEVICTABLE_PAGES_CULLED,
    FAM_VM_UNEVICTABLE_PAGES_SCANNED,
    FAM_VM_UNEVICTABLE_PAGES_RESCUED,
    FAM_VM_UNEVICTABLE_PAGES_MLOCKED,
    FAM_VM_UNEVICTABLE_PAGES_MUNLOCKED,
    FAM_VM_UNEVICTABLE_PAGES_CLEARED,
    FAM_VM_UNEVICTABLE_PAGES_STRANDED,
    FAM_VM_THP_FAULT_ALLOC,
    FAM_VM_THP_FAULT_FALLBACK,
    FAM_VM_THP_FAULT_FALLBACK_CHARGE,
    FAM_VM_THP_COLLAPSE_ALLOC,
    FAM_VM_THP_COLLAPSE_ALLOC_FAILED,
    FAM_VM_THP_FILE_ALLOC,
    FAM_VM_THP_FILE_FALLBACK,
    FAM_VM_THP_FILE_FALLBACK_CHARGE,
    FAM_VM_THP_FILE_MAPPED,
    FAM_VM_THP_SPLIT_PAGE,
    FAM_VM_THP_SPLIT_PAGE_FAILED,
    FAM_VM_THP_DEFERRED_SPLIT_PAGE,
    FAM_VM_THP_SPLIT_PMD,
    FAM_VM_THP_SCAN_EXCEED_NONE_PTE,
    FAM_VM_THP_SCAN_EXCEED_SWAP_PTE,
    FAM_VM_THP_SCAN_EXCEED_SHARE_PTE,
    FAM_VM_THP_SPLIT_PUD,
    FAM_VM_THP_ZERO_PAGE_ALLOC,
    FAM_VM_THP_ZERO_PAGE_ALLOC_FAILED,
    FAM_VM_THP_SWPOUT,
    FAM_VM_THP_SWPOUT_FALLBACK,
    FAM_VM_BALLOON_INFLATE,
    FAM_VM_BALLOON_DEFLATE,
    FAM_VM_BALLOON_MIGRATE,
    FAM_VM_TLB_REMOTE_FLUSH,
    FAM_VM_TLB_REMOTE_FLUSH_RECEIVED,
    FAM_VM_TLB_LOCAL_FLUSH_ALL,
    FAM_VM_TLB_LOCAL_FLUSH_ONE,
    FAM_VM_SWAP_READAHEAD,
    FAM_VM_SWAP_READAHEAD_HIT,
    FAM_VM_KSM_SWPIN_COPY,
    FAM_VM_COW_KSM,
    FAM_VM_ZSWAP_IN,
    FAM_VM_ZSWAP_OUT,
    FAM_VM_DIRECT_MAP_LEVEL2_SPLITS,
    FAM_VM_DIRECT_MAP_LEVEL3_SPLITS,
    FAM_VM_VMA_LOCK_SUCCESS,
    FAM_VM_VMA_LOCK_ABORT,
    FAM_VM_VMA_LOCK_RETRY,
    FAM_VM_VMA_LOCK_MISS,
    FAM_VM_MAX,
};

static metric_family_t fams[FAM_VM_MAX] = {
    [FAM_VM_ZONE_PAGE_STATE] = {
        .name = "system_vm_zone_page_state",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_VM_NUMA_EVENT] = {
        .name = "system_vm_numa_event",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_NODE_PAGE_STATE] = {
        .name = "system_vm_node_page_state",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_VM_WORKINGSET_NODES] = {
        .name = "system_vm_workingset_nodes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_VM_PAGES_DIRTIED] = {
        .name = "system_vm_pages_dirtied",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of dirty pages since boot."
    },
    [FAM_VM_PAGES_WRITTEN] = {
        .name = "system_vm_pages_written",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of written pages since boot."
    },
    [FAM_VM_PAGES_THROTTLED_WRITTEN] = {
        .name = "system_vm_pages_throttled_written",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of written pages while reclaim throttled since boot."
    },
    [FAM_VM_FOLL_PIN_ACQUIRED] = {
        .name = "system_vm_foll_pin_acquired",
        .type = METRIC_TYPE_COUNTER,
        .help = "This is the number of logical pins that have been acquired "
                "since the system was powered on.",
    },
    [FAM_VM_FOLL_PIN_RELEASED] = {
        .name = "system_vm_foll_pin_released",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of logical pins that have been released "
                "since the system was powered on.",
    },
    [FAM_VM_KERNEL_STACK_BYTES] = {
        .name = "system_vm_kernel_stack_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Sum of all kernel stacks in bytes.",
    },
    [FAM_VM_SHADOW_CALL_STACK_BYTES] = {
        .name = "system_vm_shadow_call_stack_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_VM_DIRTY_THRESHOLD] = {
        .name = "system_vm_dirty_threshold",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_VM_DIRTY_BACKGROUND_THRESHOLD] = {
        .name = "system_vm_dirty_background_threshold",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_VM_PAGE_IN] = {
        .name = "system_vm_page_in",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_PAGE_OUT] = {
        .name = "system_vm_page_out",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_SWAP_IN] = {
        .name = "system_vm_swap_in",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_SWAP_OUT] = {
        .name = "system_vm_swap_out",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_PAGE_FAULTS] = {
        .name = "system_vm_page_faults",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of page major and minor fault operations since boot.",
    },
    [FAM_VM_MAJOR_PAGE_FAULTS] = {
        .name = "system_vm_major_page_faults",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of major page fault operations since boot.",
    },
    [FAM_VM_PAGE_ALLOC] = {
        .name = "system_vm_page_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_ALLOC_STALL] = {
        .name = "system_vm_alloc_stall",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of direct reclaim calls (since the last boot).",
    },
    [FAM_VM_PAGE_SKIP] = {
        .name = "system_vm_page_skip",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_PAGE_STEAL_DIRECT] = {
        .name = "system_vm_page_steal_direct",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_PAGE_STEAL_KSWAPD] = {
        .name = "system_vm_page_steal_kswapd",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of reclaimed pages by kswapd.",
    },
    [FAM_VM_PAGE_STEAL_KHUGEPAGED] = {
        .name = "system_vm_page_steal_khugepaged",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of reclaimed pages directly.",
    },
    [FAM_VM_PAGE_STEAL_ANON] = {
        .name = "system_vm_page_steal_anon",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_PAGE_STEAL_FILE] = {
        .name = "system_vm_page_steal_file",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_PAGE_DEMOTE_KSWAPD] = {
        .name = "system_vm_page_demote_kswapd",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_PAGE_DEMOTE_DIRECT] = {
        .name = "system_vm_page_demote_direct",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_PAGE_DEMOTE_KHUGEPAGED] = {
        .name = "system_vm_page_demote_khugepaged",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_PAGE_SCAN_KSWAPD] = {
        .name = "system_vm_page_scan_kswapd",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of scanned pages by kswapd (in an inactive LRU list).",
    },
    [FAM_VM_PAGE_SCAN_DIRECT] = {
        .name = "system_vm_page_scan_direct",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of scanned pages directly (in an inactive LRU list).",
    },
    [FAM_VM_PAGE_SCAN_KHUGEPAGED] = {
        .name = "system_vm_page_scan_khugepaged",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of scanned pages by khugepaged (in an inactive LRU list).",
    },
    [FAM_VM_PAGE_SCAN_DIRECT_THROTTLE] = {
        .name = "system_vm_page_scan_direct_throttle",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_PAGE_SCAN_ANON] = {
        .name = "system_vm_page_scan_anon",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_PAGE_SCAN_FILE] = {
        .name = "system_vm_page_scan_file",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_PAGE_REFILL] = {
        .name = "system_vm_page_refill",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of scanned pages (in an active LRU list),",
    },
    [FAM_VM_PAGE_FREE] = {
        .name = "system_vm_page_free",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_PAGE_ACTIVATE] = {
        .name = "system_vm_page_activate",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of pages moved to the active LRU list.",
    },
    [FAM_VM_PAGE_DEACTIVATE] = {
        .name = "system_vm_page_deactivate",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of pages moved to the inactive LRU list.",
    },
    [FAM_VM_PAGE_PROMOTE_SUCCESS] = {
        .name = "system_vm_page_promote_success",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of pages successfully promoted.",
    },
    [FAM_VM_PAGE_PROMOTE_CANDIDATE] = {
        .name = "system_vm_page_promote_candidate",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of pages that are promoted and then demoted.",
    },
    [FAM_VM_PAGE_LAZY_FREE] = {
        .name = "system_vm_page_lazy_free",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of pages postponed to be freed under memory pressure.",
    },
    [FAM_VM_PAGE_LAZY_FREED] = {
        .name = "system_vm_page_lazy_freed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of reclaimed lazyfree pages.",
    },
    [FAM_VM_PAGE_REUSE] = {
        .name = "system_vm_page_reuse",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_ZONE_RECLAIM_FAILED] = {
        .name = "system_vm_zone_reclaim_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_PAGE_INODE_STEAL] = {
        .name = "system_vm_page_inode_steal",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_SLABS_SCANNED] = {
        .name = "system_vm_slabs_scanned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of slab objects scanned.",
    },
    [FAM_VM_KSWAPD_STEAL] = {
        .name = "system_vm_kswapd_steal",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total pages pages reclaimed by kswapd.",
    },
    [FAM_VM_KSWAPD_INODE_STEAL] = {
        .name = "system_vm_kswapd_inode_steal",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total pages reclaimed via kswapd inode freeing.",
    },
    [FAM_VM_KSWAPD_LOW_WMARK_HIT_QUICKLY] = {
        .name = "system_vm_kswapd_low_wmark_hit_quickly",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_KSWAPD_HIGH_WMARK_HIT_QUICKLY] = {
        .name = "system_vm_kswapd_high_wmark_hit_quickly",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_PAGE_OUTRUN] = {
        .name = "system_vm_page_outrun",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of kswapd's calls to page reclaim (since the last boot).",
    },
    [FAM_VM_PAGE_ROTATED] = {
        .name = "system_vm_page_rotated",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages rotated to tail of the LRU.",
    },
    [FAM_VM_DROP_PAGECACHE] = {
        .name = "system_vm_drop_pagecache",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_DROP_SLAB] = {
        .name = "system_vm_drop_slab",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_OOM_KILL] = {
        .name = "system_vm_oom_kill",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_NUMA_PTE_UPDATES] = {
        .name = "system_vm_numa_pte_updates",
        .type = METRIC_TYPE_COUNTER,
        .help = "The amount of base pages that were marked for NUMA hinting faults.",
    },
    [FAM_VM_NUMA_HUGE_PTE_UPDATES] = {
        .name = "system_vm_numa_huge_pte_updates",
        .type = METRIC_TYPE_COUNTER,
        .help = "The amount of transparent huge pages that were marked for NUMA hinting faults.",
    },
    [FAM_VM_NUMA_HINT_FAULTS] = {
        .name = "system_vm_numa_hint_faults",
        .type = METRIC_TYPE_COUNTER,
        .help = "Records how many NUMA hinting faults were trapped.",
    },
    [FAM_VM_NUMA_HINT_FAULTS_LOCAL] = {
        .name = "system_vm_numa_hint_faults_local",
        .type = METRIC_TYPE_COUNTER,
        .help = "Shows how many of the hinting faults were to local nodes.",
    },
    [FAM_VM_NUMA_PAGES_MIGRATED] = {
        .name = "system_vm_numa_pages_migrated",
        .type = METRIC_TYPE_COUNTER,
        .help = "Records how many pages were migrated because they were misplaced.",
    },
    [FAM_VM_PAGE_MIGRATE_SUCCESS] = {
        .name = "system_vm_page_migrate_success",
        .type = METRIC_TYPE_COUNTER,
        .help = "Counts normal page migration success.",
    },
    [FAM_VM_PAGE_MIGRATE_FAIL] = {
        .name = "system_vm_page_migrate_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Normal page migration failure.",
    },
    [FAM_VM_THP_MIGRATION_SUCCESS] = {
        .name = "system_vm_thp_migration_success",
        .type = METRIC_TYPE_COUNTER,
        .help = "A THP was migrated without being split.",
    },
    [FAM_VM_THP_MIGRATION_FAIL] = {
        .name = "system_vm_thp_migration_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "A THP could not be migrated nor it could be split.",
    },
    [FAM_VM_THP_MIGRATION_SPLIT] = {
        .name = "system_vm_thp_migration_split",
        .type = METRIC_TYPE_COUNTER,
        .help = "A THP was migrated, but not as such: first, the THP had to be split.",
    },
    [FAM_VM_COMPACT_MIGRATE_SCANNED] = {
        .name = "system_vm_compact_migrate_scanned",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_COMPACT_FREE_SCANNED] = {
        .name = "system_vm_compact_free_scanned",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_COMPACT_ISOLATED] = {
        .name = "system_vm_compact_isolated",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_COMPACT_STALL] = {
        .name = "system_vm_compact_stall",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented every time a process stalls to run memory compaction "
                "so that a huge page is free for use.",
    },
    [FAM_VM_COMPACT_FAIL] = {
        .name = "system_vm_compact_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented if the system compacted memory and freed a huge page for use.",
    },
    [FAM_VM_COMPACT_SUCCESS] = {
        .name = "system_vm_compact_success",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented if the system compacted memory and freed a huge page for use.",
    },
    [FAM_VM_COMPACT_DAEMON_WAKE] = {
        .name = "system_vm_compact_daemon_wake",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_COMPACT_DAEMON_MIGRATE_SCANNED] = {
        .name = "system_vm_compact_daemon_migrate_scanned",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_COMPACT_DAEMON_FREE_SCANNED] = {
        .name = "system_vm_compact_daemon_free_scanned",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_HTLB_BUDDY_ALLOC_SUCCESS] = {
        .name = "system_vm_htlb_buddy_alloc_success",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of successful huge page allocations.",
    },
    [FAM_VM_HTLB_BUDDY_ALLOC_FAIL] = {
        .name = "system_vm_htlb_buddy_alloc_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of failed huge page allocations.",
    },
    [FAM_VM_CMA_ALLOC_SUCCESS] = {
        .name = "system_vm_cma_alloc_success",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_CMA_ALLOC_FAIL] = {
        .name = "system_vm_cma_alloc_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_UNEVICTABLE_PAGES_CULLED] = {
        .name = "system_vm_unevictable_pages_culled",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_UNEVICTABLE_PAGES_SCANNED] = {
        .name = "system_vm_unevictable_pages_scanned",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_UNEVICTABLE_PAGES_RESCUED] = {
        .name = "system_vm_unevictable_pages_rescued",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_UNEVICTABLE_PAGES_MLOCKED] = {
        .name = "system_vm_unevictable_pages_mlocked",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_UNEVICTABLE_PAGES_MUNLOCKED] = {
        .name = "system_vm_unevictable_pages_munlocked",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_UNEVICTABLE_PAGES_CLEARED] = {
        .name = "system_vm_unevictable_pages_cleared",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_UNEVICTABLE_PAGES_STRANDED] = {
        .name = "system_vm_unevictable_pages_stranded",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },

    [FAM_VM_THP_FAULT_ALLOC] = {
        .name = "system_vm_thp_fault_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented every time a huge page is successfully allocated "
                "to handle a page fault."
    },
    [FAM_VM_THP_FAULT_FALLBACK] = {
        .name = "system_vm_thp_fault_fallback",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented if a page fault fails to allocate a huge page "
                "and instead falls back to using small pages."
    },
    [FAM_VM_THP_FAULT_FALLBACK_CHARGE] = {
        .name = "system_vm_thp_fault_fallback_charge",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented if a page fault fails to charge a huge page and instead falls back "
                "to using small pages even though the allocation was successful."
    },
    [FAM_VM_THP_COLLAPSE_ALLOC] = {
        .name = "system_vm_thp_collapse_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented by khugepaged when it has found a range of pages to collapse into "
                "one huge page and has successfully allocated a new huge page to store the data."
    },
    [FAM_VM_THP_COLLAPSE_ALLOC_FAILED] = {
        .name = "system_vm_thp_collapse_alloc_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented if khugepaged found a range of pages that should be collapsed "
                "into one huge page but failed the allocation.",
    },
    [FAM_VM_THP_FILE_ALLOC] = {
        .name = "system_vm_thp_file_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented every time a file huge page is successfully allocated."
    },
    [FAM_VM_THP_FILE_FALLBACK] = {
        .name = "system_vm_thp_file_fallback",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented if a file huge page is attempted to be allocated but fails "
                "and instead falls back to using small pages.",
    },
    [FAM_VM_THP_FILE_FALLBACK_CHARGE] = {
        .name = "system_vm_thp_file_fallback_charge",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented if a file huge page cannot be charged and instead falls back to "
                "using small pages even though the allocation was successful."
    },
    [FAM_VM_THP_FILE_MAPPED] = {
        .name = "system_vm_thp_file_mapped",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented every time a file huge page is mapped into user address space."
    },
    [FAM_VM_THP_SPLIT_PAGE] = {
        .name = "system_vm_thp_split_page",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented every time a huge page is split into base pages."
    },
    [FAM_VM_THP_SPLIT_PAGE_FAILED] = {
        .name = "system_vm_thp_split_page_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented if kernel fails to split huge page. "
                "This can happen if the page was pinned by somebody."
    },
    [FAM_VM_THP_DEFERRED_SPLIT_PAGE] = {
        .name = "system_vm_thp_deferred_split_page",
        .type = METRIC_TYPE_COUNTER,
        .help = "Is incremented when a huge page is put onto split queue.",
    },
    [FAM_VM_THP_SPLIT_PMD] = {
        .name = "system_vm_thp_split_pmd",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented every time a PMD split into table of PTEs.",
    },
    [FAM_VM_THP_SCAN_EXCEED_NONE_PTE] = {
        .name = "system_vm_thp_scan_exceed_none_pte",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL
    },
    [FAM_VM_THP_SCAN_EXCEED_SWAP_PTE] = {
        .name = "system_vm_thp_scan_exceed_swap_pte",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL
    },
    [FAM_VM_THP_SCAN_EXCEED_SHARE_PTE] = {
        .name = "system_vm_thp_scan_exceed_share_pte",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL
    },
    [FAM_VM_THP_SPLIT_PUD] = {
        .name = "system_vm_thp_split_pud",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL
    },
    [FAM_VM_THP_ZERO_PAGE_ALLOC] = {
        .name = "system_vm_thp_zero_page_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented every time a huge zero page used for thp is successfully allocated."
    },
    [FAM_VM_THP_ZERO_PAGE_ALLOC_FAILED] = {
        .name = "system_vm_thp_zero_page_alloc_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented if kernel fails to allocate huge zero page and "
                "falls back to using small pages."
    },
    [FAM_VM_THP_SWPOUT] = {
        .name = "system_vm_thp_swpout",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented every time a huge page is swapout in one piece without splitting."
    },
    [FAM_VM_THP_SWPOUT_FALLBACK] = {
        .name = "system_vm_thp_swpout_fallback",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented if a huge page has to be split before swapout."
    },
    [FAM_VM_BALLOON_INFLATE] = {
        .name = "system_vm_balloon_inflate",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of of virt guest balloon page inflations.",
    },
    [FAM_VM_BALLOON_DEFLATE] = {
        .name = "system_vm_balloon_deflate",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of virt guest balloon page deflations.",
    },
    [FAM_VM_BALLOON_MIGRATE] = {
        .name = "system_vm_balloon_migrate",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of virt guest balloon page migrations.",
    },
    [FAM_VM_TLB_REMOTE_FLUSH] = {
        .name = "system_vm_tlb_remote_flush",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented every time a cpu tried to flush other's tlbs.",
    },
    [FAM_VM_TLB_REMOTE_FLUSH_RECEIVED] = {
        .name = "system_vm_tlb_remote_flush_received",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented every time a cpu received ipi for flush.",
    },
    [FAM_VM_TLB_LOCAL_FLUSH_ALL] = {
        .name = "system_vm_tlb_local_flush_all",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_TLB_LOCAL_FLUSH_ONE] = {
        .name = "system_vm_tlb_local_flush_one",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_SWAP_READAHEAD] = {
        .name = "system_vm_swap_readahead",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of swap pages readahead.",
    },
    [FAM_VM_SWAP_READAHEAD_HIT] = {
        .name = "system_vm_swap_readahead_hit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of swap pages readahead that were used.",
    },
    [FAM_VM_KSM_SWPIN_COPY] = {
        .name = "system_vm_ksm_swpin_copy",
        .type = METRIC_TYPE_COUNTER,
        .help = "Is incremented every time a KSM page is copied when swapping in.",
    },
    [FAM_VM_COW_KSM] = {
        .name = "system_vm_cow_ksm",
        .type = METRIC_TYPE_COUNTER,
        .help = "Is incremented every time a KSM page triggers copy on write (COW) "
                "when users try to write to a KSM page, we have to make a copy."
    },
    [FAM_VM_ZSWAP_IN] = {
        .name = "system_vm_zswap_in",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_ZSWAP_OUT] = {
        .name = "system_vm_zswap_out",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },

    [FAM_VM_DIRECT_MAP_LEVEL2_SPLITS] = {
        .name = "system_vm_direct_map_level2_splits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of level 2 hugepage (direct mapped) split event counts since boot.",
    },
    [FAM_VM_DIRECT_MAP_LEVEL3_SPLITS] = {
        .name = "system_vm_direct_map_level3_splits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of level 3 hugepage (direct mapped) split event counts since boot.",
    },
    [FAM_VM_VMA_LOCK_SUCCESS] = {
        .name = "system_vm_vma_lock_success",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_VMA_LOCK_ABORT] = {
        .name = "system_vm_vma_lock_abort",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_VMA_LOCK_RETRY] = {
        .name = "system_vm_vma_lock_retry",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VM_VMA_LOCK_MISS] = {
        .name = "system_vm_vma_lock_miss",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
};

#include "plugins/vmem/vmem.h"

static int vmem_read(void)
{
    FILE *fh = fopen(path_proc_vmstat, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Cannot open '%s' failed: %s", path_proc_vmstat, STRERRNO);
        return -1;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[4] = {0};

        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (fields_num < 2)
            continue;

        const struct vmstat_metric *vm = vmstat_get_key(fields[0], strlen(fields[0]));
        if (vm == NULL)
            continue;

        if (vm->fam < 0)
            continue;

        uint64_t value = 0;
        if (strtouint(fields[1], &value) != 0)
            continue;

        value_t mvalue = {0};

        if (fams[vm->fam].type == METRIC_TYPE_COUNTER) {
            mvalue = VALUE_COUNTER(value);
        } else if (fams[vm->fam].type == METRIC_TYPE_GAUGE) {
            if ((vm->fam == FAM_VM_KERNEL_STACK_BYTES) ||
                (vm->fam == FAM_VM_SHADOW_CALL_STACK_BYTES))
                value *= 1024;
            mvalue = VALUE_GAUGE(value);
        }

        if ((vm->lkey != NULL) && (vm->lvalue != NULL))
            metric_family_append(&fams[vm->fam], mvalue, NULL,
                                 &(label_pair_const_t){.name=vm->lkey, .value=vm->lvalue}, NULL);
        else
            metric_family_append(&fams[vm->fam], mvalue, NULL, NULL);

    }

    fclose(fh);

    plugin_dispatch_metric_family_array_filtered(fams, FAM_VM_MAX, filter, 0);

    return 0;
}

static int vmem_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &filter);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int vmem_init(void)
{
    path_proc_vmstat = plugin_procpath("vmstat");
    if (path_proc_vmstat == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int vmem_shutdown(void)
{
    free(path_proc_vmstat);
    plugin_filter_free(filter);

    return 0;
}

void module_register(void)
{
    plugin_register_init("vmem", vmem_init);
    plugin_register_config("vmem", vmem_config);
    plugin_register_read("vmem", vmem_read);
    plugin_register_shutdown("vmem", vmem_shutdown);
}
