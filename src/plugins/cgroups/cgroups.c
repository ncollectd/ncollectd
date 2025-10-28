// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2011 Michael Stapelberg
// SPDX-FileCopyrightText: Copyright (C) 2013 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Michael Stapelberg <michael at stapelberg.de>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"
#include "libutils/mount.h"

#ifdef HAVE_LINUX_CONFIG_H
#include <linux/config.h>
#endif

enum {
    FAM_CGROUPS_CPU_USAGE_SECONDS,
    FAM_CGROUPS_CPU_USER_SECONDS,
    FAM_CGROUPS_CPU_SYSTEM_SECONDS,
    FAM_CGROUPS_CPU_PERIODS,
    FAM_CGROUPS_CPU_THROTTLED,
    FAM_CGROUPS_CPU_THROTTLED_SECONDS,
    FAM_CGROUPS_PROCESSES,
    FAM_CGROUPS_MEMORY_BYTES,
    FAM_CGROUPS_MEMORY_SWAP_BYTES,
    FAM_CGROUPS_MEMORY_ANONYMOUS_BYTES,
    FAM_CGROUPS_MEMORY_PAGE_CACHE_BYTES,
    FAM_CGROUPS_MEMORY_KERNEL_STACK_BYTES,
    FAM_CGROUPS_MEMORY_PAGE_TABLES_BYTES,
    FAM_CGROUPS_MEMORY_KERNEL_PERCPU_BYTES,
    FAM_CGROUPS_MEMORY_SOCKET_BYTES,
    FAM_CGROUPS_MEMORY_SHMEM_BYTES,
    FAM_CGROUPS_MEMORY_PAGE_CACHE_MAPPED_BYTES,
    FAM_CGROUPS_MEMORY_PAGE_CACHE_DIRTY_BYTES,
    FAM_CGROUPS_MEMORY_PAGE_CACHE_WRITEBACK_BYTES,
    FAM_CGROUPS_MEMORY_SWAP_CACHED_BYTES,
    FAM_CGROUPS_MEMORY_TRANSPARENT_HUGEPAGES_ANONYMOUS_BYTES,
    FAM_CGROUPS_MEMORY_TRANSPARENT_HUGEPAGES_PAGE_CACHE_BYTES,
    FAM_CGROUPS_MEMORY_TRANSPARENT_HUGEPAGES_SHMEM_BYTES,
    FAM_CGROUPS_MEMORY_ANONYMOUS_INACTIVE_BYTES,
    FAM_CGROUPS_MEMORY_ANONYMOUS_ACTIVE_BYTES,
    FAM_CGROUPS_MEMORY_PAGE_CACHE_INACTIVE_BYTES,
    FAM_CGROUPS_MEMORY_PAGE_CACHE_ACTIVE_BYTES,
    FAM_CGROUPS_MEMORY_UNEVICTABLE_BYTES,
    FAM_CGROUPS_MEMORY_SLAB_RECLAIMABLE_BYTES,
    FAM_CGROUPS_MEMORY_SLAB_UNRECLAIMABLE_BYTES,
    FAM_CGROUPS_MEMORY_SLAB_BYTES,
    FAM_CGROUPS_MEMORY_WORKINGSET_REFAULT_ANONYMOUS,
    FAM_CGROUPS_MEMORY_WORKINGSET_REFAULT_FILE,
    FAM_CGROUPS_MEMORY_WORKINGSET_ACTIVATE_ANONYMOUS,
    FAM_CGROUPS_MEMORY_WORKINGSET_ACTIVATE_FILE,
    FAM_CGROUPS_MEMORY_WORKINGSET_RESTORE_ANONYMOUS,
    FAM_CGROUPS_MEMORY_WORKINGSET_RESTORE_FILE,
    FAM_CGROUPS_MEMORY_WORKINGSET_NODERECLAIM,
    FAM_CGROUPS_MEMORY_TRANSPARENT_HUGEPAGES_FAULT_ALLOC,
    FAM_CGROUPS_MEMORY_TRANSPARENT_HUGEPAGES_COLLAPSE_ALLOC,
    FAM_CGROUPS_MEMORY_PAGE_FAULTS,
    FAM_CGROUPS_MEMORY_PAGE_MAJOR_FAULTS,
    FAM_CGROUPS_MEMORY_PAGE_REFILLS,
    FAM_CGROUPS_MEMORY_PAGE_SCANS,
    FAM_CGROUPS_MEMORY_PAGE_STEALS,
    FAM_CGROUPS_MEMORY_PAGE_ACTIVATES,
    FAM_CGROUPS_MEMORY_PAGE_DEACTIVATES,
    FAM_CGROUPS_MEMORY_PAGE_LAZY_FREE,
    FAM_CGROUPS_MEMORY_PAGE_LAZY_FREED,
    FAM_CGROUPS_NUMA_ANONYMOUS_BYTES,
    FAM_CGROUPS_NUMA_PAGE_CACHE_BYTES,
    FAM_CGROUPS_NUMA_KERNEL_STACK_BYTES,
    FAM_CGROUPS_NUMA_PAGE_TABLES_BYTES,
    FAM_CGROUPS_NUMA_SHMEM_BYTES,
    FAM_CGROUPS_NUMA_PAGE_CACHE_MAPPED_BYTES,
    FAM_CGROUPS_NUMA_PAGE_CACHE_DIRTY_BYTES,
    FAM_CGROUPS_NUMA_PAGE_CACHE_WRITEBACK_BYTES,
    FAM_CGROUPS_NUMA_SWAP_CACHED_BYTES,
    FAM_CGROUPS_NUMA_TRANSPARENT_HUGEPAGES_ANONYMOUS_BYTES,
    FAM_CGROUPS_NUMA_TRANSPARENT_HUGEPAGES_PAGE_CACHE_BYTES,
    FAM_CGROUPS_NUMA_TRANSPARENT_HUGEPAGES_SHMEM_BYTES,
    FAM_CGROUPS_NUMA_ANONYMOUS_INACTIVE_BYTES,
    FAM_CGROUPS_NUMA_ANONYMOUS_ACTIVE_BYTES,
    FAM_CGROUPS_NUMA_PAGE_CACHE_INACTIVE_BYTES,
    FAM_CGROUPS_NUMA_PAGE_CACHE_ACTIVE_BYTES,
    FAM_CGROUPS_NUMA_UNEVICTABLE_BYTES,
    FAM_CGROUPS_NUMA_SLAB_RECLAIMABLE_BYTES,
    FAM_CGROUPS_NUMA_SLAB_UNRECLAIMABLE_BYTES,
    FAM_CGROUPS_NUMA_WORKINGSET_REFAULT_ANONYMOUS,
    FAM_CGROUPS_NUMA_WORKINGSET_REFAULT_FILE,
    FAM_CGROUPS_NUMA_WORKINGSET_ACTIVATE_ANONYMOUS,
    FAM_CGROUPS_NUMA_WORKINGSET_ACTIVATE_FILE,
    FAM_CGROUPS_NUMA_WORKINGSET_RESTORE_ANONYMOUS,
    FAM_CGROUPS_NUMA_WORKINGSET_RESTORE_FILE,
    FAM_CGROUPS_NUMA_WORKINGSET_NODERECLAIM,
    FAM_CGROUPS_IO_READ_BYTES,
    FAM_CGROUPS_IO_WRITE_BYTES,
    FAM_CGROUPS_IO_READ_IOS,
    FAM_CGROUPS_IO_WRITE_IOS,
    FAM_CGROUPS_IO_DISCARTED_BYTES,
    FAM_CGROUPS_IO_DISCARTED_IOS,
    FAM_CGROUPS_PRESSURE_CPU_WAITING,
    FAM_CGROUPS_PRESSURE_CPU_STALLED,
    FAM_CGROUPS_PRESSURE_IO_WAITING,
    FAM_CGROUPS_PRESSURE_IO_STALLED,
    FAM_CGROUPS_PRESSURE_MEMORY_WAITING,
    FAM_CGROUPS_PRESSURE_MEMORY_STALLED,
    FAM_CGROUPS_MAX,
};

static metric_family_t fams[FAM_CGROUPS_MAX] = {
    [FAM_CGROUPS_CPU_USAGE_SECONDS] = {
        .name = "system_cgroups_cpu_usage_seconds",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_CGROUPS_CPU_USER_SECONDS] = {
        .name = "system_cgroups_cpu_user_seconds",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_CGROUPS_CPU_SYSTEM_SECONDS] = {
        .name = "system_cgroups_cpu_system_seconds",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_CGROUPS_CPU_PERIODS] = {
        .name = "system_cgroups_cpu_periods",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of period intervals that have elapsed.",
    },
    [FAM_CGROUPS_CPU_THROTTLED] = {
        .name = "system_cgroups_cpu_throttled",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times tasks in a cgroup have been throttled "
                "(that is, not allowed to run because they have exhausted all "
                "of the available time as specified by their quota).",
    },
    [FAM_CGROUPS_CPU_THROTTLED_SECONDS] = {
        .name = "system_cgroups_cpu_throttled_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total time duration (in seconds) for which tasks "
                "in the cgroup have been throttled."
    },
    [FAM_CGROUPS_PROCESSES] = {
        .name = "system_cgroups_processes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of processes currently in the cgroup and its descendants.",
    },
    [FAM_CGROUPS_MEMORY_BYTES] = {
        .name = "system_cgroups_memory_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total amount of memory currently being used "
                "by the cgroup and its descendants.",
    },
    [FAM_CGROUPS_MEMORY_SWAP_BYTES] = {
        .name = "system_cgroups_memory_swap_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total amount of swap currently being used by the cgroup and its descendants.",
    },
    [FAM_CGROUPS_MEMORY_ANONYMOUS_BYTES] = {
        .name = "system_cgroups_memory_anonymous_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used in anonymous mappings such as "
                "brk(), sbrk(), and mmap(MAP_ANONYMOUS)",
    },
    [FAM_CGROUPS_MEMORY_PAGE_CACHE_BYTES] = {
        .name = "system_cgroups_memory_page_cache_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used to cache filesystem data, "
                "including tmpfs and shared memory.",
    },
    [FAM_CGROUPS_MEMORY_KERNEL_STACK_BYTES] = {
        .name = "system_cgroups_memory_kernel_stack_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory allocated to kernel stacks.",
    },
    [FAM_CGROUPS_MEMORY_PAGE_TABLES_BYTES] = {
        .name = "system_cgroups_memory_page_tables_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory allocated for page tables.",
    },
    [FAM_CGROUPS_MEMORY_KERNEL_PERCPU_BYTES] = {
        .name = "system_cgroups_memory_kernel_percpu_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used for storing per-cpu kernel data structures.",
    },
    [FAM_CGROUPS_MEMORY_SOCKET_BYTES] = {
        .name = "system_cgroups_memory_socket_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used in network transmission buffers.",
    },
    [FAM_CGROUPS_MEMORY_SHMEM_BYTES] = {
        .name = "system_cgroups_memory_shmem_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data that is swap-backed, "
                "such as tmpfs, shm segments, shared anonymous mmap()s,",
    },
    [FAM_CGROUPS_MEMORY_PAGE_CACHE_MAPPED_BYTES] = {
        .name = "system_cgroups_memory_page_cache_mapped_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data mapped with mmap().",
    },
    [FAM_CGROUPS_MEMORY_PAGE_CACHE_DIRTY_BYTES] = {
        .name = "system_cgroups_memory_page_cache_dirty_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data that was modified "
                "but not yet written back to disk.",
    },
    [FAM_CGROUPS_MEMORY_PAGE_CACHE_WRITEBACK_BYTES] = {
        .name = "system_cgroups_memory_page_cache_writeback_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data that was modified and "
                "is currently being written back to disk.",
    },
    [FAM_CGROUPS_MEMORY_SWAP_CACHED_BYTES] = {
        .name = "system_cgroups_memory_swap_cached_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of swap cached in memory. "
                "The swapcache is accounted against both memory and swap usage.",
    },
    [FAM_CGROUPS_MEMORY_TRANSPARENT_HUGEPAGES_ANONYMOUS_BYTES] = {
        .name = "system_cgroups_memory_transparent_hugepages_anonymous_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used in anonymous mappings backed by transparent hugepages.",
    },
    [FAM_CGROUPS_MEMORY_TRANSPARENT_HUGEPAGES_PAGE_CACHE_BYTES] = {
        .name = "system_cgroups_memory_transparent_hugepages_page_cache_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data backed by transparent hugepages.",
    },
    [FAM_CGROUPS_MEMORY_TRANSPARENT_HUGEPAGES_SHMEM_BYTES] = {
        .name = "system_cgroups_memory_transparent_hugepages_shmem_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of shm, tmpfs, shared anonymous mmap()s backed by transparent hugepages.",
    },
    [FAM_CGROUPS_MEMORY_ANONYMOUS_INACTIVE_BYTES] = {
        .name = "system_cgroups_memory_anonymous_inactive_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_CGROUPS_MEMORY_ANONYMOUS_ACTIVE_BYTES] = {
        .name = "system_cgroups_memory_anonymous_active_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_CGROUPS_MEMORY_PAGE_CACHE_INACTIVE_BYTES] = {
        .name = "system_cgroups_memory_page_cache_inactive_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_CGROUPS_MEMORY_PAGE_CACHE_ACTIVE_BYTES] = {
        .name = "system_cgroups_memory_page_cache_active_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_CGROUPS_MEMORY_UNEVICTABLE_BYTES] = {
        .name = "system_cgroups_memory_unevictable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_CGROUPS_MEMORY_SLAB_RECLAIMABLE_BYTES] = {
        .name = "system_cgroups_memory_slab_reclaimable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Part of “slab” that might be reclaimed, such as dentries and inodes.",
    },
    [FAM_CGROUPS_MEMORY_SLAB_UNRECLAIMABLE_BYTES] = {
        .name = "system_cgroups_memory_slab_unreclaimable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Part of “slab” that cannot be reclaimed on memory pressure.",
    },
    [FAM_CGROUPS_MEMORY_SLAB_BYTES] = {
        .name = "system_cgroups_memory_slab_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used for storing in-kernel data structures.",
    },
    [FAM_CGROUPS_MEMORY_WORKINGSET_REFAULT_ANONYMOUS] = {
        .name = "system_cgroups_memory_workingset_refault_anonymous",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of refaults of previously evicted anonymous pages.",
    },
    [FAM_CGROUPS_MEMORY_WORKINGSET_REFAULT_FILE] = {
        .name = "system_cgroups_memory_workingset_refault_file",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of refaults of previously evicted file pages.",
    },
    [FAM_CGROUPS_MEMORY_WORKINGSET_ACTIVATE_ANONYMOUS] = {
        .name = "system_cgroups_memory_workingset_activate_anonymous",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of refaulted anonymous pages that were immediately activated.",
    },
    [FAM_CGROUPS_MEMORY_WORKINGSET_ACTIVATE_FILE] = {
        .name = "system_cgroups_memory_workingset_activate_file",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of refaulted file pages that were immediately activated.",
    },
    [FAM_CGROUPS_MEMORY_WORKINGSET_RESTORE_ANONYMOUS] = {
        .name = "system_cgroups_memory_workingset_restore_anonymous",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of restored anonymous pages which have been detected "
                "as an active workingset before they got reclaimed.",
    },
    [FAM_CGROUPS_MEMORY_WORKINGSET_RESTORE_FILE] = {
        .name = "system_cgroups_memory_workingset_restore_file",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of restored file pages which have been detected as "
                "an active workingset before they got reclaimed.",
    },
    [FAM_CGROUPS_MEMORY_WORKINGSET_NODERECLAIM] = {
        .name = "system_cgroups_memory_workingset_nodereclaim",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a shadow node has been reclaimed.",
    },
    [FAM_CGROUPS_MEMORY_PAGE_FAULTS] = {
        .name = "system_cgroups_memory_page_faults",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of page faults incurred.",
    },
    [FAM_CGROUPS_MEMORY_PAGE_MAJOR_FAULTS] = {
        .name = "system_cgroups_memory_page_major_faults",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of major page faults incurred.",
    },
    [FAM_CGROUPS_MEMORY_PAGE_REFILLS] = {
        .name = "system_cgroups_memory_page_refills",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of scanned pages (in an active LRU list)",
    },
    [FAM_CGROUPS_MEMORY_PAGE_SCANS] = {
        .name = "system_cgroups_memory_page_scans",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of scanned pages (in an inactive LRU list)",
    },
    [FAM_CGROUPS_MEMORY_PAGE_STEALS] = {
        .name = "system_cgroups_memory_page_steals",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of reclaimed pages.",
    },
    [FAM_CGROUPS_MEMORY_PAGE_ACTIVATES] = {
        .name = "system_cgroups_memory_page_activates",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of pages moved to the active LRU list.",
    },
    [FAM_CGROUPS_MEMORY_PAGE_DEACTIVATES] = {
        .name = "system_cgroups_memory_page_deactivates",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of pages moved to the inactive LRU list.",
    },
    [FAM_CGROUPS_MEMORY_PAGE_LAZY_FREE] = {
        .name = "system_cgroups_memory_page_lazy_free",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of pages postponed to be freed under memory pressure.",
    },
    [FAM_CGROUPS_MEMORY_PAGE_LAZY_FREED] = {
        .name = "system_cgroups_memory_page_lazy_freed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of reclaimed lazyfree pages.",
    },
    [FAM_CGROUPS_MEMORY_TRANSPARENT_HUGEPAGES_FAULT_ALLOC] = {
        .name = "system_cgroups_memory_transparent_hugepages_fault_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of transparent hugepages which were allocated to satisfy a page fault. "
                "This counter is not present when CONFIG_TRANSPARENT_HUGEPAGE is not set.",
    },
    [FAM_CGROUPS_MEMORY_TRANSPARENT_HUGEPAGES_COLLAPSE_ALLOC] = {
        .name = "system_cgroups_memory_transparent_hugepages_collapse_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of transparent hugepages which were allocated to allow collapsing "
                "an existing range of pages."
    },
    [FAM_CGROUPS_NUMA_ANONYMOUS_BYTES] = {
        .name = "system_cgroups_numa_anonymous_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used in anonymous mappings such as "
                "brk(), sbrk(), and mmap(MAP_ANONYMOUS)",
    },
    [FAM_CGROUPS_NUMA_PAGE_CACHE_BYTES] = {
        .name = "system_cgroups_numa_page_cache_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used to cache filesystem data, "
                "including tmpfs and shared memory.",
    },
    [FAM_CGROUPS_NUMA_KERNEL_STACK_BYTES] = {
        .name = "system_cgroups_numa_kernel_stack_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory allocated to kernel stacks.",
    },
    [FAM_CGROUPS_NUMA_PAGE_TABLES_BYTES] = {
        .name = "system_cgroups_numa_page_tables_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory allocated for page tables.",
    },
    [FAM_CGROUPS_NUMA_SHMEM_BYTES] = {
        .name = "system_cgroups_numa_shmem_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data that is swap-backed, such as "
                "tmpfs, shm segments, shared anonymous mmap()s,",
    },
    [FAM_CGROUPS_NUMA_PAGE_CACHE_MAPPED_BYTES] = {
        .name = "system_cgroups_numa_page_cache_mapped_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data mapped with mmap().",
    },
    [FAM_CGROUPS_NUMA_PAGE_CACHE_DIRTY_BYTES] = {
        .name = "system_cgroups_numa_page_cache_dirty_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data that was modified but not "
                "yet written back to disk.",
    },
    [FAM_CGROUPS_NUMA_PAGE_CACHE_WRITEBACK_BYTES] = {
        .name = "system_cgroups_numa_page_cache_writeback_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data that was modified and is "
                "currently being written back to disk.",
    },
    [FAM_CGROUPS_NUMA_SWAP_CACHED_BYTES] = {
        .name = "system_cgroups_numa_swap_cached_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of swap cached in memory. "
                "The swapcache is accounted against both memory and swap usage.",
    },
    [FAM_CGROUPS_NUMA_TRANSPARENT_HUGEPAGES_ANONYMOUS_BYTES] = {
        .name = "system_cgroups_numa_transparent_hugepages_anonymous_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used in anonymous mappings backed by transparent hugepages.",
    },
    [FAM_CGROUPS_NUMA_TRANSPARENT_HUGEPAGES_PAGE_CACHE_BYTES] = {
        .name = "system_cgroups_numa_transparent_hugepages_page_cache_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data backed by transparent hugepages.",
    },
    [FAM_CGROUPS_NUMA_TRANSPARENT_HUGEPAGES_SHMEM_BYTES] = {
        .name = "system_cgroups_numa_transparent_hugepages_shmem_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of shm, tmpfs, shared anonymous mmap()s backed by transparent hugepages.",
    },
    [FAM_CGROUPS_NUMA_ANONYMOUS_INACTIVE_BYTES] = {
        .name = "system_cgroups_numa_anonymous_inactive_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_CGROUPS_NUMA_ANONYMOUS_ACTIVE_BYTES] = {
        .name = "system_cgroups_numa_anonymous_active_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_CGROUPS_NUMA_PAGE_CACHE_INACTIVE_BYTES] = {
        .name = "system_cgroups_numa_page_cache_inactive_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_CGROUPS_NUMA_PAGE_CACHE_ACTIVE_BYTES] = {
        .name = "system_cgroups_numa_page_cache_active_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_CGROUPS_NUMA_UNEVICTABLE_BYTES] = {
        .name = "system_cgroups_numa_unevictable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_CGROUPS_NUMA_SLAB_RECLAIMABLE_BYTES] = {
        .name = "system_cgroups_numa_slab_reclaimable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Part of “slab” that might be reclaimed, such as dentries and inodes.",
    },
    [FAM_CGROUPS_NUMA_SLAB_UNRECLAIMABLE_BYTES] = {
        .name = "system_cgroups_numa_slab_unreclaimable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Part of “slab” that cannot be reclaimed on memory pressure.",
    },
    [FAM_CGROUPS_NUMA_WORKINGSET_REFAULT_ANONYMOUS] = {
        .name = "system_cgroups_numa_workingset_refault_anonymous",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of refaults of previously evicted anonymous pages.",
    },
    [FAM_CGROUPS_NUMA_WORKINGSET_REFAULT_FILE] = {
        .name = "system_cgroups_numa_workingset_refault_file",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of refaults of previously evicted file pages.",
    },
    [FAM_CGROUPS_NUMA_WORKINGSET_ACTIVATE_ANONYMOUS] = {
        .name = "system_cgroups_numa_workingset_activate_anonymous",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of refaulted anonymous pages that were immediately activated.",
    },
    [FAM_CGROUPS_NUMA_WORKINGSET_ACTIVATE_FILE] = {
        .name = "system_cgroups_numa_workingset_activate_file",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of refaulted file pages that were immediately activated.",
    },
    [FAM_CGROUPS_NUMA_WORKINGSET_RESTORE_ANONYMOUS] = {
        .name = "system_cgroups_numa_workingset_restore_anonymous",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of restored anonymous pages which have been detected as "
                "an active workingset before they got reclaimed.",
    },
    [FAM_CGROUPS_NUMA_WORKINGSET_RESTORE_FILE] = {
        .name = "system_cgroups_numa_workingset_restore_file",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of restored file pages which have been detected as "
                "an active workingset before they got reclaimed.",
    },
    [FAM_CGROUPS_NUMA_WORKINGSET_NODERECLAIM] = {
        .name = "system_cgroups_numa_workingset_nodereclaim",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a shadow node has been reclaimed.",
    },
    [FAM_CGROUPS_IO_READ_BYTES] = {
        .name = "system_cgroups_io_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bytes read.",
    },
    [FAM_CGROUPS_IO_WRITE_BYTES] = {
        .name = "system_cgroups_io_write_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bytes written.",
    },
    [FAM_CGROUPS_IO_READ_IOS] = {
        .name = "system_cgroups_io_read_ios",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of read IOs.",
    },
    [FAM_CGROUPS_IO_WRITE_IOS] = {
        .name = "system_cgroups_io_write_ios",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of write IOs.",
    },
    [FAM_CGROUPS_IO_DISCARTED_BYTES] = {
        .name = "system_cgroups_io_discarted_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bytes discarded",
    },
    [FAM_CGROUPS_IO_DISCARTED_IOS] = {
        .name = "system_cgroups_io_discarted_ios",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of discard IOs",
    },
    [FAM_CGROUPS_PRESSURE_CPU_WAITING] = {
        .name = "system_cgroups_pressure_cpu_waiting",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which at least some tasks in the cgroup "
                "are stalled on the cpu.",
    },
    [FAM_CGROUPS_PRESSURE_CPU_STALLED] = {
        .name = "system_cgroups_pressure_cpu_stalled",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which all non-idle tasks in the cgroup "
                "are stalled on the cpu simultaneously.",
    },
    [FAM_CGROUPS_PRESSURE_IO_WAITING] = {
        .name = "system_cgroups_pressure_io_waiting",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which at least some tasks in the cgroup "
                "are stalled on the io.",
    },
    [FAM_CGROUPS_PRESSURE_IO_STALLED] = {
        .name = "system_cgroups_pressure_io_stalled",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which all non-idle tasks in the cgroup "
                "are stalled on the io simultaneously."
    },
    [FAM_CGROUPS_PRESSURE_MEMORY_WAITING] = {
        .name = "system_cgroups_pressure_memory_waiting",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which at least some tasks in the cgroup "
                "are stalled on the memory.",
    },
    [FAM_CGROUPS_PRESSURE_MEMORY_STALLED] = {
        .name = "system_cgroups_pressure_memory_stalled",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which all non-idle tasks in the cgroup "
                "are stalled on the memory simultaneously.",
    },
};

#include "plugins/cgroups/memorystat.h"

#ifndef CONFIG_HZ
#define CONFIG_HZ 100
#endif

static exclist_t excl_cgroup;

typedef enum {
    KIND_CGROUP_V2,
    KIND_CGROUP_V1_CPUACCT,
    KIND_CGROUP_V1_BLKIO,
    KIND_CGROUP_V1_MEMORY,
} kind_cgroup_t;

static int read_blkio_io(int dir_fd, const char *filename, const char *cgroup_name,
                         metric_family_t *fam_read, metric_family_t *fam_write,
                         metric_family_t *fam_discart)
{
    FILE *fh = fopenat(dir_fd, filename, "r");
    if (fh == NULL) {
        PLUGIN_DEBUG("fopenat ('%s') at '%s' failed: %s", filename, cgroup_name, STRERRNO);
        return -1;
    }

    char buf[1024];
    while (fgets(buf, sizeof(buf), fh) != NULL) {
        char *fields[16];

        int numfields = strsplit(buf, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields != 3)
            continue;

        metric_family_t *fam = NULL;
        if (!strcmp(fields[1], "Read"))
            fam = fam_read;
        else if (!strcmp(fields[1], "Write"))
            fam = fam_write;
        else if (!strcmp(fields[1], "Discard"))
            fam = fam_discart;

        if (fam == NULL)
            continue;

        char *mayor = fields[0];
        char *minor = strchr(fields[0], ':');
        if (minor == NULL)
            continue;
        *(minor++) = 0;

        uint64_t val = 0;
        int status = strtouint(fields[2], &val);
        if (status != 0)
            continue;

        metric_family_append(fam, VALUE_COUNTER(val), NULL,
                             &(label_pair_const_t){.name="minor", .value=minor},
                             &(label_pair_const_t){.name="mayor", .value=mayor},
                             &(label_pair_const_t){.name="cgroup", .value=cgroup_name},
                             NULL);
    }

    fclose(fh);
    return 0;
}

static int read_io_stat(int dir_fd, const char *cgroup_name)
{
    FILE *fh = fopenat(dir_fd, "io.stat", "r");
    if (fh == NULL) {
        PLUGIN_DEBUG("fopenat ('io.stat') at '%s' failed: %s", cgroup_name, STRERRNO);
        return -1;
    }

    char buf[1024];
    while (fgets(buf, sizeof(buf), fh) != NULL) {
        char *fields[16];

        int numfields = strsplit(buf, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields < 7)
            continue;

        char *mayor = fields[0];
        char *minor = strchr(fields[0], ':');
        if (minor == NULL)
            continue;
        *(minor++) = 0;

        for (int i=1; i < 7; i++) {
            char *key = fields[i];
            char *value = strchr(fields[i], '=');
            if (value == NULL)
                continue;
            *(value++) = 0;

            uint64_t val = 0;
            int status = strtouint(value, &val);
            if (status != 0)
                continue;

            if (!strcmp(key, "rbytes"))
                metric_family_append(&fams[FAM_CGROUPS_IO_READ_BYTES], VALUE_COUNTER(val), NULL,
                                     &(label_pair_const_t){.name="minor", .value=minor},
                                     &(label_pair_const_t){.name="mayor", .value=mayor},
                                     &(label_pair_const_t){.name="cgroup", .value=cgroup_name},
                                     NULL);
            else if (!strcmp(key, "wbytes"))
                metric_family_append(&fams[FAM_CGROUPS_IO_WRITE_BYTES], VALUE_COUNTER(val), NULL,
                                     &(label_pair_const_t){.name="minor", .value=minor},
                                     &(label_pair_const_t){.name="mayor", .value=mayor},
                                     &(label_pair_const_t){.name="cgroup", .value=cgroup_name},
                                     NULL);
            else if (!strcmp(key, "rios"))
                metric_family_append(&fams[FAM_CGROUPS_IO_READ_IOS], VALUE_COUNTER(val), NULL,
                                     &(label_pair_const_t){.name="minor", .value=minor},
                                     &(label_pair_const_t){.name="mayor", .value=mayor},
                                     &(label_pair_const_t){.name="cgroup", .value=cgroup_name},
                                     NULL);
            else if (!strcmp(key, "wios"))
                metric_family_append(&fams[FAM_CGROUPS_IO_WRITE_IOS], VALUE_COUNTER(val), NULL,
                                     &(label_pair_const_t){.name="minor", .value=minor},
                                     &(label_pair_const_t){.name="mayor", .value=mayor},
                                     &(label_pair_const_t){.name="cgroup", .value=cgroup_name},
                                     NULL);
            else if (!strcmp(key, "dbytes"))
                metric_family_append(&fams[FAM_CGROUPS_IO_DISCARTED_BYTES], VALUE_COUNTER(val), NULL,
                                     &(label_pair_const_t){.name="minor", .value=minor},
                                     &(label_pair_const_t){.name="mayor", .value=mayor},
                                     &(label_pair_const_t){.name="cgroup", .value=cgroup_name},
                                     NULL);
            else if (!strcmp(key, "dios"))
                metric_family_append(&fams[FAM_CGROUPS_IO_DISCARTED_IOS], VALUE_COUNTER(val), NULL,
                                     &(label_pair_const_t){.name="minor", .value=minor},
                                     &(label_pair_const_t){.name="mayor", .value=mayor},
                                     &(label_pair_const_t){.name="cgroup", .value=cgroup_name},
                                     NULL);

        }
    }

    fclose(fh);
    return 0;
}

static int read_cpu_stat_v1(int dir_fd, const char *cgroup_name)
{
    FILE *fh = fopenat(dir_fd, "cpuacct.stat", "r");
    if (fh == NULL) {
        PLUGIN_DEBUG("fdopen ('cpuacct.stat') at '%s' failed: %s", cgroup_name, STRERRNO);
        return -1;
    }

    char buf[1024];
    while (fgets(buf, sizeof(buf), fh) != NULL) {
        char *fields[8];

        /* Expected format v1:
         *   user: 12345
         *   system: 23456
         * Or:
         *   user 12345
         *   system 23456
         * user and system are in USER_HZ unit.
         */
        int numfields = strsplit(buf, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields != 2)
            continue;

        char *key = fields[0];
        size_t key_len = strlen(key);

        if (key_len < 2)
            continue;
        /* Strip colon off the first column, if found */
        if (key[key_len - 1] == ':')
            key[key_len - 1] = '\0';

        uint64_t counter;
        int status = strtouint(fields[1], &counter);
        if (status != 0)
            continue;

        if (!strcmp(key, "user"))
            metric_family_append(&fams[FAM_CGROUPS_CPU_USER_SECONDS],
                                 VALUE_COUNTER_FLOAT64((double)counter / 1000000000.0), NULL,
                                 &(label_pair_const_t){.name="cgroup", .value=cgroup_name}, NULL);
        else if (!strcmp(key, "system"))
            metric_family_append(&fams[FAM_CGROUPS_CPU_SYSTEM_SECONDS],
                                 VALUE_COUNTER_FLOAT64((double)counter / 1000000000.0), NULL,
                                 &(label_pair_const_t){.name="cgroup", .value=cgroup_name}, NULL);
    }

    fclose(fh);

    return 0;
}

static int read_cpu_stat_v2(int dir_fd, const char *cgroup_name)
{
    FILE *fh = fopenat(dir_fd, "cpu.stat", "r");
    if (fh == NULL) {
        PLUGIN_DEBUG("fdopen ('%s') failed: %s", cgroup_name, STRERRNO);
        return -1;
    }

    char buf[1024];
    while (fgets(buf, sizeof(buf), fh) != NULL) {
        char *fields[8];

        int numfields = strsplit(buf, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields != 2)
            continue;

        char *key = fields[0];

        uint64_t counter;
        int status = strtouint(fields[1], &counter);
        if (status != 0)
            continue;

        if (!strcmp(key, "usage_usec"))
            metric_family_append(&fams[FAM_CGROUPS_CPU_USAGE_SECONDS],
                                 VALUE_COUNTER_FLOAT64((double)counter / 1000000.0), NULL,
                                 &(label_pair_const_t){.name="cgroup", .value=cgroup_name}, NULL);
        else if (!strcmp(key, "user_usec"))
            metric_family_append(&fams[FAM_CGROUPS_CPU_USER_SECONDS],
                                 VALUE_COUNTER_FLOAT64((double)counter / 1000000.0), NULL,
                                 &(label_pair_const_t){.name="cgroup", .value=cgroup_name}, NULL);
        else if (!strcmp(key, "system_usec"))
            metric_family_append(&fams[FAM_CGROUPS_CPU_SYSTEM_SECONDS],
                                 VALUE_COUNTER_FLOAT64((double)counter / 1000000.0), NULL,
                                 &(label_pair_const_t){.name="cgroup", .value=cgroup_name}, NULL);
        else if (!strcmp(key, "nr_periods"))
            metric_family_append(&fams[FAM_CGROUPS_CPU_PERIODS],
                                 VALUE_COUNTER_FLOAT64((double)counter), NULL,
                                 &(label_pair_const_t){.name="cgroup", .value=cgroup_name}, NULL);
        else if (!strcmp(key, "nr_throttled"))
            metric_family_append(&fams[FAM_CGROUPS_CPU_THROTTLED],
                                 VALUE_COUNTER_FLOAT64((double)counter), NULL,
                                 &(label_pair_const_t){.name="cgroup", .value=cgroup_name}, NULL);
        else if (!strcmp(key, "throttled_usec"))
            metric_family_append(&fams[FAM_CGROUPS_CPU_THROTTLED_SECONDS],
                                 VALUE_COUNTER_FLOAT64((double)counter / 1000000.0), NULL,
                                 &(label_pair_const_t){.name="cgroup", .value=cgroup_name}, NULL);
    }

    fclose(fh);

    return 0;
}

static int read_memory_numa_stat(int dir_fd, const char *cgroup_name)
{
    FILE *fh = fopenat(dir_fd, "memory.numa_stat", "r");
    if (fh == NULL) {
        PLUGIN_DEBUG("fdopen ('memory.numa_stat') at '%s' failed: %s", cgroup_name, STRERRNO);
        return -1;
    }

    char buf[1024];
    while (fgets(buf, sizeof(buf), fh) != NULL) {
        char *fields[256];

        int numfields = strsplit(buf, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields < 2)
            continue;

        char *key = fields[0];
        size_t key_len = strlen(key);

        const struct memorystat_metric *ms = memorystat_get_key (key, key_len);
        if (ms == NULL)
            continue;

        if (ms->numa_fam < 0)
            continue;

        for (int i=1; i < numfields; i++) {
            char *zone = fields[i];
            if (*zone != 'N')
                continue;
            zone++;

            char *number = strchr(fields[i], '=');
            if (number == NULL)
                continue;
            *(number++) = 0;

            value_t value = {0};
            if (fams[ms->numa_fam].type == METRIC_TYPE_COUNTER) {
                uint64_t raw = 0;
                int status = strtouint(number, &raw);
                if (status != 0)
                    continue;
                value = VALUE_COUNTER(raw);
            } else if (fams[ms->numa_fam].type == METRIC_TYPE_GAUGE) {
                double raw = 0;
                int status = strtodouble(number, &raw);
                if (status != 0)
                    continue;
                value = VALUE_GAUGE(raw);
            } else {
                continue;
            }

            metric_family_append(&fams[ms->numa_fam], value, NULL,
                                 &(label_pair_const_t){.name="zone", .value=zone},
                                 &(label_pair_const_t){.name="cgroup", .value=cgroup_name},
                                 NULL);
        }
    }

    fclose(fh);
    return 0;
}

static int read_memory_stat(int dir_fd, const char *cgroup_name)
{
    FILE *fh = fopenat(dir_fd, "memory.stat", "r");
    if (fh == NULL) {
        PLUGIN_DEBUG("fdopen ('memory.stat') at '%s' failed: %s", cgroup_name, STRERRNO);
        return -1;
    }

    char buf[1024];
    while (fgets(buf, sizeof(buf), fh) != NULL) {
        char *fields[8];

        int numfields = strsplit(buf, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields != 2)
            continue;

        char *key = fields[0];
        size_t key_len = strlen(key);

        const struct memorystat_metric *ms = memorystat_get_key (key, key_len);
        if (ms == NULL)
            continue;

        value_t value = {0};
        if (fams[ms->fam].type == METRIC_TYPE_COUNTER) {
            uint64_t raw = 0;
            int status = strtouint(fields[1], &raw);
            if (status != 0)
                continue;
            value = VALUE_COUNTER(raw);
        } else if (fams[ms->fam].type == METRIC_TYPE_GAUGE) {
            double raw = 0;
            int status = strtodouble(fields[1], &raw);
            if (status != 0)
                continue;
            value = VALUE_GAUGE(raw);
        } else {
            continue;
        }

        metric_family_append(&fams[ms->fam], value, NULL,
                             &(label_pair_const_t){.name="cgroup", .value=cgroup_name}, NULL);
    }

    fclose(fh);

    return 0;
}

static int read_pressure_file(int dir_fd, const char *filename, const char *cgroup_name,
                               metric_family_t *fam_waiting, metric_family_t *fam_stalled)
{
    FILE *fh = fopenat(dir_fd, filename, "r");
    if (fh == NULL) {
        PLUGIN_DEBUG("fdopen ('%s') at %s failed: %s", filename, cgroup_name, STRERRNO);
        return -1;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[5] = {NULL};

        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (fields_num != 5)
            continue;

        if (strncmp(fields[4], "total=", strlen("total=")) != 0)
            continue;

        value_t value = VALUE_COUNTER(atoll(fields[4] + strlen("total=")));

        if ((strcmp(fields[0], "some") == 0) && (fam_waiting != NULL)) {
            metric_family_append(fam_waiting, value, NULL,
                                 &(label_pair_const_t){.name="cgroup", .value=cgroup_name}, NULL);
        } else if ((strcmp(fields[0], "full") == 0) && (fam_stalled != NULL)) {
            metric_family_append(fam_stalled, value, NULL,
                                 &(label_pair_const_t){.name="cgroup", .value=cgroup_name}, NULL);
        }
    }

    fclose(fh);

    return 0;
}

static int  read_cgroup_file(int dir_fd, const char *filename,
                             const char *cgroup_name, metric_family_t *fam)
{
    value_t value = {0};

    if (fam->type == METRIC_TYPE_COUNTER)  {
        uint64_t raw = 0;
        int status = filetouint_at(dir_fd, filename, &raw);
        if (status != 0)
            return -1;
        value = VALUE_COUNTER(raw);
    } else if (fam->type == METRIC_TYPE_GAUGE) {
        double raw = 0;
        int status = filetodouble_at(dir_fd, filename, &raw);
        if (status != 0)
            return -1;
        value = VALUE_GAUGE(raw);
    } else {
        return -1;
    }

    metric_family_append(fam, value, NULL,
                         &(label_pair_const_t){.name="cgroup", .value=cgroup_name}, NULL);

    return 0;
}

static int read_cgroup_stats(int cgroup_fd, const char *cgroup_name, kind_cgroup_t kind)
{
    switch(kind) {
        case KIND_CGROUP_V2:
            read_cpu_stat_v2(cgroup_fd, cgroup_name);

            read_cgroup_file(cgroup_fd, "pids.current", cgroup_name,
                                        &fams[FAM_CGROUPS_PROCESSES]);

            read_io_stat(cgroup_fd, cgroup_name);

            read_cgroup_file(cgroup_fd, "memory.current", cgroup_name,
                                        &fams[FAM_CGROUPS_MEMORY_BYTES]);

            read_cgroup_file(cgroup_fd, "memory.swap.current", cgroup_name,
                                        &fams[FAM_CGROUPS_MEMORY_SWAP_BYTES]);

            read_memory_stat(cgroup_fd, cgroup_name);

            read_memory_numa_stat(cgroup_fd, cgroup_name);

            read_pressure_file(cgroup_fd, "cpu.pressure", cgroup_name,
                                          &fams[FAM_CGROUPS_PRESSURE_CPU_WAITING],
                                          &fams[FAM_CGROUPS_PRESSURE_CPU_STALLED]);

            read_pressure_file(cgroup_fd, "io.pressure", cgroup_name,
                                          &fams[FAM_CGROUPS_PRESSURE_IO_WAITING],
                                          &fams[FAM_CGROUPS_PRESSURE_IO_STALLED]);

            read_pressure_file(cgroup_fd, "memory.pressure", cgroup_name,
                                          &fams[FAM_CGROUPS_PRESSURE_MEMORY_WAITING],
                                          &fams[FAM_CGROUPS_PRESSURE_MEMORY_STALLED]);
            break;
        case KIND_CGROUP_V1_CPUACCT:
            read_cpu_stat_v1(cgroup_fd, cgroup_name);
            break;
        case KIND_CGROUP_V1_BLKIO:
            read_blkio_io(cgroup_fd, "blkio.io_service_bytes", cgroup_name,
                                     &fams[FAM_CGROUPS_IO_READ_BYTES],
                                     &fams[FAM_CGROUPS_IO_WRITE_BYTES],
                                     &fams[FAM_CGROUPS_IO_DISCARTED_BYTES]);

            read_blkio_io(cgroup_fd, "blkio.io_serviced", cgroup_name,
                                     &fams[FAM_CGROUPS_IO_READ_IOS],
                                     &fams[FAM_CGROUPS_IO_WRITE_IOS],
                                     &fams[FAM_CGROUPS_IO_DISCARTED_IOS]);
            break;
        case KIND_CGROUP_V1_MEMORY:
            read_cgroup_file(cgroup_fd, "memory.usage_in_bytes", cgroup_name,
                                        &fams[FAM_CGROUPS_MEMORY_BYTES]);

            read_memory_stat(cgroup_fd, cgroup_name);
            break;
    }

    return 0;
}

static int read_cgroup(int dir_fd, const char *filename,
                       const char *cgroup_name, kind_cgroup_t kind)
{
    DIR *dir = opendirat(dir_fd, filename);
    if (dir == NULL)
        return 0;

    int cgroup_fd = dirfd(dir);

    if (exclist_match(&excl_cgroup, cgroup_name))
        read_cgroup_stats(cgroup_fd, cgroup_name, kind);

    struct dirent *dirent;
    while ((dirent = readdir(dir)) != NULL) {
        if ((dirent->d_type == DT_DIR) && (dirent->d_name[0] != '.')) {
            char path[PATH_MAX];
            ssnprintf(path, sizeof(path), "%s/%s", cgroup_name, dirent->d_name);
            read_cgroup(cgroup_fd, dirent->d_name, path, kind);
        }
    }

    closedir(dir);

    return 0;
}

static int read_cgroup_root(int dir_fd, const char *dirname, const char *filename, void *user_data)
{
    struct stat statbuf;
    int status = fstatat(dir_fd, filename, &statbuf, 0);
    if (status != 0) {
        PLUGIN_ERROR("stat (%s) in %s failed: %s.", filename, dirname, STRERRNO);
        return -1;
    }

    if (S_ISDIR(statbuf.st_mode)) {
        kind_cgroup_t kind = *(kind_cgroup_t *)user_data;
        read_cgroup(dir_fd, filename, filename, kind);
    }

    return 0;
}

static int cgroups_read(void)
{
    cu_mount_t *mnt_list = NULL;
    if (cu_mount_getlist(&mnt_list) == NULL) {
        PLUGIN_ERROR("cu_mount_getlist failed.");
        return -1;
    }

    bool cgroup_v2_found = false;
    bool cgroup_v1_cpuacct_found = false;
    bool cgroup_v1_blkio_found = false;
    bool cgroup_v1_memory_found = false;

    for (cu_mount_t *mnt_ptr = mnt_list; mnt_ptr != NULL; mnt_ptr = mnt_ptr->next) {
        if (!cgroup_v2_found && (strcmp(mnt_ptr->type, "cgroup2") == 0)) {
            kind_cgroup_t kind = KIND_CGROUP_V2;
            walk_directory(mnt_ptr->dir, read_cgroup_root, &kind, 0);
            cgroup_v2_found = true;
        } else if (strcmp(mnt_ptr->type, "cgroup") == 0) {
            if (!cgroup_v1_cpuacct_found &&
                cu_mount_checkoption(mnt_ptr->options, "cpuacct", /* full = */ 1)) {
                kind_cgroup_t kind = KIND_CGROUP_V1_CPUACCT;
                walk_directory(mnt_ptr->dir, read_cgroup_root, &kind, 0);
                cgroup_v1_cpuacct_found = true;
            }
            if (!cgroup_v1_blkio_found &&
                cu_mount_checkoption(mnt_ptr->options, "blkio", /* full = */ 1)) {
                kind_cgroup_t kind = KIND_CGROUP_V1_BLKIO;
                walk_directory(mnt_ptr->dir, read_cgroup_root, &kind, 0);
                cgroup_v1_blkio_found = true;
            }
            if (!cgroup_v1_memory_found &&
                cu_mount_checkoption(mnt_ptr->options, "memory", /* full = */ 1)) {
                kind_cgroup_t kind = KIND_CGROUP_V1_MEMORY;
                walk_directory(mnt_ptr->dir, read_cgroup_root, &kind, 0);
                cgroup_v1_memory_found = true;
            }
        }
    }

    cu_mount_freelist(mnt_list);

    if (!cgroup_v2_found && !cgroup_v1_cpuacct_found &&
        !cgroup_v1_blkio_found && !cgroup_v1_memory_found) {
        PLUGIN_WARNING("Unable to find cgroup mount-point.");
        return -1;
    }

    plugin_dispatch_metric_family_array(fams, FAM_CGROUPS_MAX, 0);

    return 0;
}

static int cgroups_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "cgroup") == 0) {
            status = cf_util_exclist(child, &excl_cgroup);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int cgroups_shutdown(void)
{
    exclist_reset(&excl_cgroup);
    return 0;
}

void module_register(void)
{
    plugin_register_config("cgroups", cgroups_config);
    plugin_register_read("cgroups", cgroups_read);
    plugin_register_shutdown("cgroups", cgroups_shutdown);
}
