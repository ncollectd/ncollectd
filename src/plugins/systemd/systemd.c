// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2011 Michael Stapelberg
// SPDX-FileCopyrightText: Copyright (C) 2013 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Michael Stapelberg <michael at stapelberg.de>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <systemd/sd-daemon.h>
#include <systemd/sd-bus.h>

#ifdef HAVE_LINUX_CONFIG_H
#include <linux/config.h>
#endif
#include <linux/magic.h>
#include <sys/vfs.h>

typedef enum {
    CGROUP_UNKNOWN         = -1,
    CGROUP_V2_SYSTEMD      = 0,
    CGROUP_V2_UNIFIED      = 1,
    CGROUP_V2_ALL          = 2,
} cgroup_systemd_t;

#ifndef CGROUP2_SUPER_MAGIC
#define CGROUP2_SUPER_MAGIC	0x63677270
#endif

static cgroup_systemd_t cgroup_type = CGROUP_UNKNOWN;
static char **units;
static size_t units_size;

enum {
    FAM_SYSTEMD_UNIT_LOAD_STATE,
    FAM_SYSTEMD_UNIT_ACTIVE_STATE,
    FAM_SYSTEMD_UNIT_SUB_STATE,
    FAM_SYSTEMD_UNIT_START_TIME_SECONDS,
    FAM_SYSTEMD_UNIT_TASKS_CURRENT,
    FAM_SYSTEMD_UNIT_TASKS_MAX,
    FAM_SYSTEMD_SERVICE_RESTART,
    FAM_SYSTEMD_TIMER_LAST_TRIGGER_SECONDS,
    FAM_SYSTEMD_SOCKET_ACCEPTED_CONNECTIONS,
    FAM_SYSTEMD_SOCKET_CURRENT_CONNECTIONS,
    FAM_SYSTEMD_SOCKET_REFUSED_CONNECTIONS,
    FAM_SYSTEMD_UNIT_CPU_USAGE_SECONDS,
    FAM_SYSTEMD_UNIT_CPU_USER_SECONDS,
    FAM_SYSTEMD_UNIT_CPU_SYSTEM_SECONDS,
    FAM_SYSTEMD_UNIT_CPU_PERIODS,
    FAM_SYSTEMD_UNIT_CPU_THROTTLED,
    FAM_SYSTEMD_UNIT_CPU_THROTTLED_SECONDS,
    FAM_SYSTEMD_UNIT_PROCESSES,
    FAM_SYSTEMD_UNIT_MEMORY_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_SWAP_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_ANONYMOUS_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_PAGE_CACHE_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_KERNEL_STACK_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_PAGE_TABLES_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_KERNEL_PERCPU_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_SOCKET_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_SHMEM_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_PAGE_CACHE_MAPPED_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_PAGE_CACHE_DIRTY_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_PAGE_CACHE_WRITEBACK_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_SWAP_CACHED_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_TRANSPARENT_HUGEPAGES_ANONYMOUS_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_TRANSPARENT_HUGEPAGES_PAGE_CACHE_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_TRANSPARENT_HUGEPAGES_SHMEM_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_ANONYMOUS_INACTIVE_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_ANONYMOUS_ACTIVE_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_PAGE_CACHE_INACTIVE_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_PAGE_CACHE_ACTIVE_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_UNEVICTABLE_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_SLAB_RECLAIMABLE_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_SLAB_UNRECLAIMABLE_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_SLAB_BYTES,
    FAM_SYSTEMD_UNIT_MEMORY_WORKINGSET_REFAULT_ANONYMOUS,
    FAM_SYSTEMD_UNIT_MEMORY_WORKINGSET_REFAULT_FILE,
    FAM_SYSTEMD_UNIT_MEMORY_WORKINGSET_ACTIVATE_ANONYMOUS,
    FAM_SYSTEMD_UNIT_MEMORY_WORKINGSET_ACTIVATE_FILE,
    FAM_SYSTEMD_UNIT_MEMORY_WORKINGSET_RESTORE_ANONYMOUS,
    FAM_SYSTEMD_UNIT_MEMORY_WORKINGSET_RESTORE_FILE,
    FAM_SYSTEMD_UNIT_MEMORY_WORKINGSET_NODERECLAIM,
    FAM_SYSTEMD_UNIT_MEMORY_TRANSPARENT_HUGEPAGES_FAULT_ALLOC,
    FAM_SYSTEMD_UNIT_MEMORY_TRANSPARENT_HUGEPAGES_COLLAPSE_ALLOC,
    FAM_SYSTEMD_UNIT_MEMORY_PAGE_FAULTS,
    FAM_SYSTEMD_UNIT_MEMORY_PAGE_MAJOR_FAULTS,
    FAM_SYSTEMD_UNIT_MEMORY_PAGE_REFILLS,
    FAM_SYSTEMD_UNIT_MEMORY_PAGE_SCANS,
    FAM_SYSTEMD_UNIT_MEMORY_PAGE_STEALS,
    FAM_SYSTEMD_UNIT_MEMORY_PAGE_ACTIVATES,
    FAM_SYSTEMD_UNIT_MEMORY_PAGE_DEACTIVATES,
    FAM_SYSTEMD_UNIT_MEMORY_PAGE_LAZY_FREE,
    FAM_SYSTEMD_UNIT_MEMORY_PAGE_LAZY_FREED,
    FAM_SYSTEMD_UNIT_NUMA_ANONYMOUS_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_PAGE_CACHE_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_KERNEL_STACK_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_PAGE_TABLES_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_SHMEM_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_PAGE_CACHE_MAPPED_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_PAGE_CACHE_DIRTY_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_PAGE_CACHE_WRITEBACK_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_SWAP_CACHED_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_TRANSPARENT_HUGEPAGES_ANONYMOUS_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_TRANSPARENT_HUGEPAGES_PAGE_CACHE_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_TRANSPARENT_HUGEPAGES_SHMEM_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_ANONYMOUS_INACTIVE_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_ANONYMOUS_ACTIVE_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_PAGE_CACHE_INACTIVE_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_PAGE_CACHE_ACTIVE_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_UNEVICTABLE_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_SLAB_RECLAIMABLE_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_SLAB_UNRECLAIMABLE_BYTES,
    FAM_SYSTEMD_UNIT_NUMA_WORKINGSET_REFAULT_ANONYMOUS,
    FAM_SYSTEMD_UNIT_NUMA_WORKINGSET_REFAULT_FILE,
    FAM_SYSTEMD_UNIT_NUMA_WORKINGSET_ACTIVATE_ANONYMOUS,
    FAM_SYSTEMD_UNIT_NUMA_WORKINGSET_ACTIVATE_FILE,
    FAM_SYSTEMD_UNIT_NUMA_WORKINGSET_RESTORE_ANONYMOUS,
    FAM_SYSTEMD_UNIT_NUMA_WORKINGSET_RESTORE_FILE,
    FAM_SYSTEMD_UNIT_NUMA_WORKINGSET_NODERECLAIM,
    FAM_SYSTEMD_UNIT_IO_READ_BYTES,
    FAM_SYSTEMD_UNIT_IO_WRITE_BYTES,
    FAM_SYSTEMD_UNIT_IO_READ_IOS,
    FAM_SYSTEMD_UNIT_IO_WRITE_IOS,
    FAM_SYSTEMD_UNIT_IO_DISCARTED_BYTES,
    FAM_SYSTEMD_UNIT_IO_DISCARTED_IOS,
    FAM_SYSTEMD_UNIT_PRESSURE_CPU_WAITING,
    FAM_SYSTEMD_UNIT_PRESSURE_CPU_STALLED,
    FAM_SYSTEMD_UNIT_PRESSURE_IO_WAITING,
    FAM_SYSTEMD_UNIT_PRESSURE_IO_STALLED,
    FAM_SYSTEMD_UNIT_PRESSURE_MEMORY_WAITING,
    FAM_SYSTEMD_UNIT_PRESSURE_MEMORY_STALLED,
    FAM_SYSTEMD_MAX,
};

static metric_family_t fam[FAM_SYSTEMD_MAX] = {
    [FAM_SYSTEMD_UNIT_LOAD_STATE] = {
        .name = "systemd_unit_load_state",
        .type = METRIC_TYPE_STATE_SET,
        .help = "Reflects whether the unit definition was properly loaded.",
    },
    [FAM_SYSTEMD_UNIT_ACTIVE_STATE] = {
        .name = "systemd_unit_active_state",
        .type = METRIC_TYPE_STATE_SET,
        .help = "The high-level unit activation state.",
    },
    [FAM_SYSTEMD_UNIT_SUB_STATE] = {
        .name = "systemd_unit_sub_state",
        .type = METRIC_TYPE_STATE_SET,
        .help = "The low-level unit activation state, values depend on unit type.",
    },
    [FAM_SYSTEMD_UNIT_START_TIME_SECONDS] = {
        .name = "systemd_unit_start_time_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Start time of the unit since unix epoch in seconds.",
    },
    [FAM_SYSTEMD_UNIT_TASKS_CURRENT] = {
        .name = "systemd_unit_tasks_current",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of tasks per Systemd unit.",
    },
    [FAM_SYSTEMD_UNIT_TASKS_MAX] = {
        .name = "systemd_unit_tasks_max",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum number of tasks per Systemd unit.",
    },
    [FAM_SYSTEMD_SERVICE_RESTART] = {
        .name = "systemd_service_restart",
        .type = METRIC_TYPE_COUNTER,
        .help = "Service unit count of restart triggers.",
    },
    [FAM_SYSTEMD_TIMER_LAST_TRIGGER_SECONDS] = {
        .name = "systemd_timer_last_trigger_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Seconds since epoch of last trigger.",
    },
    [FAM_SYSTEMD_SOCKET_ACCEPTED_CONNECTIONS] = {
        .name = "systemd_socket_accepted_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of accepted socket connections.",
    },
    [FAM_SYSTEMD_SOCKET_CURRENT_CONNECTIONS] = {
        .name = "systemd_socket_current_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of socket connections.",
    },
    [FAM_SYSTEMD_SOCKET_REFUSED_CONNECTIONS] = {
        .name = "systemd_socket_refused_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of refused socket connections.",
    },
    [FAM_SYSTEMD_UNIT_CPU_USAGE_SECONDS] = {
        .name = "systemd_unit_cpu_usage_seconds",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SYSTEMD_UNIT_CPU_USER_SECONDS] = {
        .name = "systemd_unit_cpu_user_seconds",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SYSTEMD_UNIT_CPU_SYSTEM_SECONDS] = {
        .name = "systemd_unit_cpu_system_seconds",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_SYSTEMD_UNIT_CPU_PERIODS] = {
        .name = "systemd_unit_cpu_periods",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of period intervals that have elapsed.",
    },
    [FAM_SYSTEMD_UNIT_CPU_THROTTLED] = {
        .name = "systemd_unit_cpu_throttled",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times tasks in a cgroup have been throttled "
                "(that is, not allowed to run because they have exhausted all "
                "of the available time as specified by their quota).",
    },
    [FAM_SYSTEMD_UNIT_CPU_THROTTLED_SECONDS] = {
        .name = "systemd_unit_cpu_throttled_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total time duration (in seconds) for which tasks "
                "in the cgroup have been throttled."
    },
    [FAM_SYSTEMD_UNIT_PROCESSES] = {
        .name = "systemd_unit_processes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of processes currently in the cgroup and its descendants.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_BYTES] = {
        .name = "systemd_unit_memory_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total amount of memory currently being used "
                "by the cgroup and its descendants.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_SWAP_BYTES] = {
        .name = "systemd_unit_memory_swap_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total amount of swap currently being used by the cgroup and its descendants.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_ANONYMOUS_BYTES] = {
        .name = "systemd_unit_memory_anonymous_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used in anonymous mappings such as "
                "brk(), sbrk(), and mmap(MAP_ANONYMOUS)",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_PAGE_CACHE_BYTES] = {
        .name = "systemd_unit_memory_page_cache_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used to cache filesystem data, "
                "including tmpfs and shared memory.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_KERNEL_STACK_BYTES] = {
        .name = "systemd_unit_memory_kernel_stack_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory allocated to kernel stacks.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_PAGE_TABLES_BYTES] = {
        .name = "systemd_unit_memory_page_tables_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory allocated for page tables.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_KERNEL_PERCPU_BYTES] = {
        .name = "systemd_unit_memory_kernel_percpu_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used for storing per-cpu kernel data structures.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_SOCKET_BYTES] = {
        .name = "systemd_unit_memory_socket_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used in network transmission buffers.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_SHMEM_BYTES] = {
        .name = "systemd_unit_memory_shmem_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data that is swap-backed, "
                "such as tmpfs, shm segments, shared anonymous mmap()s,",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_PAGE_CACHE_MAPPED_BYTES] = {
        .name = "systemd_unit_memory_page_cache_mapped_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data mapped with mmap().",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_PAGE_CACHE_DIRTY_BYTES] = {
        .name = "systemd_unit_memory_page_cache_dirty_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data that was modified "
                "but not yet written back to disk.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_PAGE_CACHE_WRITEBACK_BYTES] = {
        .name = "systemd_unit_memory_page_cache_writeback_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data that was modified and "
                "is currently being written back to disk.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_SWAP_CACHED_BYTES] = {
        .name = "systemd_unit_memory_swap_cached_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of swap cached in memory. "
                "The swapcache is accounted against both memory and swap usage.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_TRANSPARENT_HUGEPAGES_ANONYMOUS_BYTES] = {
        .name = "systemd_unit_memory_transparent_hugepages_anonymous_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used in anonymous mappings backed by transparent hugepages.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_TRANSPARENT_HUGEPAGES_PAGE_CACHE_BYTES] = {
        .name = "systemd_unit_memory_transparent_hugepages_page_cache_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data backed by transparent hugepages.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_TRANSPARENT_HUGEPAGES_SHMEM_BYTES] = {
        .name = "systemd_unit_memory_transparent_hugepages_shmem_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of shm, tmpfs, shared anonymous mmap()s backed by transparent hugepages.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_ANONYMOUS_INACTIVE_BYTES] = {
        .name = "systemd_unit_memory_anonymous_inactive_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SYSTEMD_UNIT_MEMORY_ANONYMOUS_ACTIVE_BYTES] = {
        .name = "systemd_unit_memory_anonymous_active_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SYSTEMD_UNIT_MEMORY_PAGE_CACHE_INACTIVE_BYTES] = {
        .name = "systemd_unit_memory_page_cache_inactive_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SYSTEMD_UNIT_MEMORY_PAGE_CACHE_ACTIVE_BYTES] = {
        .name = "systemd_unit_memory_page_cache_active_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SYSTEMD_UNIT_MEMORY_UNEVICTABLE_BYTES] = {
        .name = "systemd_unit_memory_unevictable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SYSTEMD_UNIT_MEMORY_SLAB_RECLAIMABLE_BYTES] = {
        .name = "systemd_unit_memory_slab_reclaimable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Part of “slab” that might be reclaimed, such as dentries and inodes.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_SLAB_UNRECLAIMABLE_BYTES] = {
        .name = "systemd_unit_memory_slab_unreclaimable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Part of “slab” that cannot be reclaimed on memory pressure.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_SLAB_BYTES] = {
        .name = "systemd_unit_memory_slab_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used for storing in-kernel data structures.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_WORKINGSET_REFAULT_ANONYMOUS] = {
        .name = "systemd_unit_memory_workingset_refault_anonymous",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of refaults of previously evicted anonymous pages.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_WORKINGSET_REFAULT_FILE] = {
        .name = "systemd_unit_memory_workingset_refault_file",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of refaults of previously evicted file pages.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_WORKINGSET_ACTIVATE_ANONYMOUS] = {
        .name = "systemd_unit_memory_workingset_activate_anonymous",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of refaulted anonymous pages that were immediately activated.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_WORKINGSET_ACTIVATE_FILE] = {
        .name = "systemd_unit_memory_workingset_activate_file",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of refaulted file pages that were immediately activated.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_WORKINGSET_RESTORE_ANONYMOUS] = {
        .name = "systemd_unit_memory_workingset_restore_anonymous",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of restored anonymous pages which have been detected "
                "as an active workingset before they got reclaimed.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_WORKINGSET_RESTORE_FILE] = {
        .name = "systemd_unit_memory_workingset_restore_file",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of restored file pages which have been detected as "
                "an active workingset before they got reclaimed.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_WORKINGSET_NODERECLAIM] = {
        .name = "systemd_unit_memory_workingset_nodereclaim",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a shadow node has been reclaimed.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_PAGE_FAULTS] = {
        .name = "systemd_unit_memory_page_faults",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of page faults incurred.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_PAGE_MAJOR_FAULTS] = {
        .name = "systemd_unit_memory_page_major_faults",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of major page faults incurred.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_PAGE_REFILLS] = {
        .name = "systemd_unit_memory_page_refills",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of scanned pages (in an active LRU list)",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_PAGE_SCANS] = {
        .name = "systemd_unit_memory_page_scans",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of scanned pages (in an inactive LRU list)",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_PAGE_STEALS] = {
        .name = "systemd_unit_memory_page_steals",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of reclaimed pages.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_PAGE_ACTIVATES] = {
        .name = "systemd_unit_memory_page_activates",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of pages moved to the active LRU list.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_PAGE_DEACTIVATES] = {
        .name = "systemd_unit_memory_page_deactivates",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of pages moved to the inactive LRU list.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_PAGE_LAZY_FREE] = {
        .name = "systemd_unit_memory_page_lazy_free",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of pages postponed to be freed under memory pressure.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_PAGE_LAZY_FREED] = {
        .name = "systemd_unit_memory_page_lazy_freed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of reclaimed lazyfree pages.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_TRANSPARENT_HUGEPAGES_FAULT_ALLOC] = {
        .name = "systemd_unit_memory_transparent_hugepages_fault_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of transparent hugepages which were allocated to satisfy a page fault. "
                "This counter is not present when CONFIG_TRANSPARENT_HUGEPAGE is not set.",
    },
    [FAM_SYSTEMD_UNIT_MEMORY_TRANSPARENT_HUGEPAGES_COLLAPSE_ALLOC] = {
        .name = "systemd_unit_memory_transparent_hugepages_collapse_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of transparent hugepages which were allocated to allow collapsing "
                "an existing range of pages."
    },
    [FAM_SYSTEMD_UNIT_NUMA_ANONYMOUS_BYTES] = {
        .name = "systemd_unit_numa_anonymous_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used in anonymous mappings such as "
                "brk(), sbrk(), and mmap(MAP_ANONYMOUS)",
    },
    [FAM_SYSTEMD_UNIT_NUMA_PAGE_CACHE_BYTES] = {
        .name = "systemd_unit_numa_page_cache_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used to cache filesystem data, "
                "including tmpfs and shared memory.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_KERNEL_STACK_BYTES] = {
        .name = "systemd_unit_numa_kernel_stack_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory allocated to kernel stacks.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_PAGE_TABLES_BYTES] = {
        .name = "systemd_unit_numa_page_tables_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory allocated for page tables.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_SHMEM_BYTES] = {
        .name = "systemd_unit_numa_shmem_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data that is swap-backed, such as "
                "tmpfs, shm segments, shared anonymous mmap()s,",
    },
    [FAM_SYSTEMD_UNIT_NUMA_PAGE_CACHE_MAPPED_BYTES] = {
        .name = "systemd_unit_numa_page_cache_mapped_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data mapped with mmap().",
    },
    [FAM_SYSTEMD_UNIT_NUMA_PAGE_CACHE_DIRTY_BYTES] = {
        .name = "systemd_unit_numa_page_cache_dirty_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data that was modified but not "
                "yet written back to disk.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_PAGE_CACHE_WRITEBACK_BYTES] = {
        .name = "systemd_unit_numa_page_cache_writeback_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data that was modified and is "
                "currently being written back to disk.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_SWAP_CACHED_BYTES] = {
        .name = "systemd_unit_numa_swap_cached_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of swap cached in memory. "
                "The swapcache is accounted against both memory and swap usage.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_TRANSPARENT_HUGEPAGES_ANONYMOUS_BYTES] = {
        .name = "systemd_unit_numa_transparent_hugepages_anonymous_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory used in anonymous mappings backed by transparent hugepages.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_TRANSPARENT_HUGEPAGES_PAGE_CACHE_BYTES] = {
        .name = "systemd_unit_numa_transparent_hugepages_page_cache_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of cached filesystem data backed by transparent hugepages.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_TRANSPARENT_HUGEPAGES_SHMEM_BYTES] = {
        .name = "systemd_unit_numa_transparent_hugepages_shmem_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of shm, tmpfs, shared anonymous mmap()s backed by transparent hugepages.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_ANONYMOUS_INACTIVE_BYTES] = {
        .name = "systemd_unit_numa_anonymous_inactive_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SYSTEMD_UNIT_NUMA_ANONYMOUS_ACTIVE_BYTES] = {
        .name = "systemd_unit_numa_anonymous_active_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SYSTEMD_UNIT_NUMA_PAGE_CACHE_INACTIVE_BYTES] = {
        .name = "systemd_unit_numa_page_cache_inactive_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SYSTEMD_UNIT_NUMA_PAGE_CACHE_ACTIVE_BYTES] = {
        .name = "systemd_unit_numa_page_cache_active_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SYSTEMD_UNIT_NUMA_UNEVICTABLE_BYTES] = {
        .name = "systemd_unit_numa_unevictable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SYSTEMD_UNIT_NUMA_SLAB_RECLAIMABLE_BYTES] = {
        .name = "systemd_unit_numa_slab_reclaimable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Part of “slab” that might be reclaimed, such as dentries and inodes.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_SLAB_UNRECLAIMABLE_BYTES] = {
        .name = "systemd_unit_numa_slab_unreclaimable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Part of “slab” that cannot be reclaimed on memory pressure.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_WORKINGSET_REFAULT_ANONYMOUS] = {
        .name = "systemd_unit_numa_workingset_refault_anonymous",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of refaults of previously evicted anonymous pages.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_WORKINGSET_REFAULT_FILE] = {
        .name = "systemd_unit_numa_workingset_refault_file",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of refaults of previously evicted file pages.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_WORKINGSET_ACTIVATE_ANONYMOUS] = {
        .name = "systemd_unit_numa_workingset_activate_anonymous",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of refaulted anonymous pages that were immediately activated.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_WORKINGSET_ACTIVATE_FILE] = {
        .name = "systemd_unit_numa_workingset_activate_file",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of refaulted file pages that were immediately activated.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_WORKINGSET_RESTORE_ANONYMOUS] = {
        .name = "systemd_unit_numa_workingset_restore_anonymous",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of restored anonymous pages which have been detected as "
                "an active workingset before they got reclaimed.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_WORKINGSET_RESTORE_FILE] = {
        .name = "systemd_unit_numa_workingset_restore_file",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of restored file pages which have been detected as "
                "an active workingset before they got reclaimed.",
    },
    [FAM_SYSTEMD_UNIT_NUMA_WORKINGSET_NODERECLAIM] = {
        .name = "systemd_unit_numa_workingset_nodereclaim",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a shadow node has been reclaimed.",
    },
    [FAM_SYSTEMD_UNIT_IO_READ_BYTES] = {
        .name = "systemd_unit_io_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bytes read.",
    },
    [FAM_SYSTEMD_UNIT_IO_WRITE_BYTES] = {
        .name = "systemd_unit_io_write_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bytes written.",
    },
    [FAM_SYSTEMD_UNIT_IO_READ_IOS] = {
        .name = "systemd_unit_io_read_ios",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of read IOs.",
    },
    [FAM_SYSTEMD_UNIT_IO_WRITE_IOS] = {
        .name = "systemd_unit_io_write_ios",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of write IOs.",
    },
    [FAM_SYSTEMD_UNIT_IO_DISCARTED_BYTES] = {
        .name = "systemd_unit_io_discarted_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bytes discarded",
    },
    [FAM_SYSTEMD_UNIT_IO_DISCARTED_IOS] = {
        .name = "systemd_unit_io_discarted_ios",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of discard IOs",
    },
    [FAM_SYSTEMD_UNIT_PRESSURE_CPU_WAITING] = {
        .name = "systemd_unit_pressure_cpu_waiting",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which at least some tasks in the cgroup "
                "are stalled on the cpu.",
    },
    [FAM_SYSTEMD_UNIT_PRESSURE_CPU_STALLED] = {
        .name = "systemd_unit_pressure_cpu_stalled",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which all non-idle tasks in the cgroup "
                "are stalled on the cpu simultaneously.",
    },
    [FAM_SYSTEMD_UNIT_PRESSURE_IO_WAITING] = {
        .name = "systemd_unit_pressure_io_waiting",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which at least some tasks in the cgroup "
                "are stalled on the io.",
    },
    [FAM_SYSTEMD_UNIT_PRESSURE_IO_STALLED] = {
        .name = "systemd_unit_pressure_io_stalled",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which all non-idle tasks in the cgroup "
                "are stalled on the io simultaneously."
    },
    [FAM_SYSTEMD_UNIT_PRESSURE_MEMORY_WAITING] = {
        .name = "systemd_unit_pressure_memory_waiting",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which at least some tasks in the cgroup "
                "are stalled on the memory.",
    },
    [FAM_SYSTEMD_UNIT_PRESSURE_MEMORY_STALLED] = {
        .name = "systemd_unit_pressure_memory_stalled",
        .type = METRIC_TYPE_COUNTER,
        .help = "The share of time in which all non-idle tasks in the cgroup "
                "are stalled on the memory simultaneously.",
    },
};

#include "plugins/systemd/memorystat.h"

static bool strendswith(const char *s, const char *suffix)
{
    size_t sl = strlen(s);
    size_t pl = strlen(suffix);

    if (pl == 0)
        return false;

    if (sl < pl)
        return false;

    if (strcmp(s + sl - pl, suffix) == 0)
        return true;

    return false;
}

static int get_property_uint32(sd_bus *bus, const char *destination, const char *path,
                               const char *interface, const char *member, uint32_t *value)
{
    sd_bus_message *reply = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;

    int status = sd_bus_call_method(bus, destination, path, "org.freedesktop.DBus.Properties",
                                         "Get", &error, &reply, "ss", interface, member);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    sd_bus_error_free(&error);

    status = sd_bus_message_enter_container(reply, 'v', "u");
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    uint32_t number = 0;
    status = sd_bus_message_read_basic(reply, 'u', &number);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    *value = number;

    sd_bus_message_unref(reply);

    return 0;
}

static int get_property_uint64(sd_bus *bus, const char *destination, const char *path,
                               const char *interface, const char *member, uint64_t *value)
{
    sd_bus_message *reply = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;

    int status = sd_bus_call_method(bus, destination, path, "org.freedesktop.DBus.Properties",
                                         "Get", &error, &reply, "ss", interface, member);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    sd_bus_error_free(&error);

    status = sd_bus_message_enter_container(reply, 'v', "t");
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    uint64_t number = 0;
    status = sd_bus_message_read_basic(reply, 't', &number);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    *value = number;

    sd_bus_message_unref(reply);

    return 0;
}

static int get_property_buffer(sd_bus *bus, const char *destination, const char *path,
                               const char *interface, const char *member, char *value,
                               size_t value_size)
{
    sd_bus_message *reply = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int status = sd_bus_call_method(bus, destination, path, "org.freedesktop.DBus.Properties",
                                         "Get", &error, &reply, "ss", interface, member);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    sd_bus_error_free(&error);

    status = sd_bus_message_enter_container(reply, 'v', "s");
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    const char *string = NULL;
    status = sd_bus_message_read_basic(reply, 's', &string);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    if (string != NULL)
        sstrncpy(value, string, value_size);

    sd_bus_message_unref(reply);

    return 0;
}


static int read_io_stat(int dir_fd, const char *unit_name)
{
    FILE *fh = fopenat(dir_fd, "io.stat", "r");
    if (fh == NULL) {
        PLUGIN_DEBUG("fopenat ('io.stat') at '%s' failed: %s", unit_name, STRERRNO);
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
                metric_family_append(&fam[FAM_SYSTEMD_UNIT_IO_READ_BYTES], VALUE_COUNTER(val), NULL,
                                     &(label_pair_const_t){.name="minor", .value=minor},
                                     &(label_pair_const_t){.name="mayor", .value=mayor},
                                     &(label_pair_const_t){.name="unit", .value=unit_name},
                                     NULL);
            else if (!strcmp(key, "wbytes"))
                metric_family_append(&fam[FAM_SYSTEMD_UNIT_IO_WRITE_BYTES], VALUE_COUNTER(val), NULL,
                                     &(label_pair_const_t){.name="minor", .value=minor},
                                     &(label_pair_const_t){.name="mayor", .value=mayor},
                                     &(label_pair_const_t){.name="unit", .value=unit_name},
                                     NULL);
            else if (!strcmp(key, "rios"))
                metric_family_append(&fam[FAM_SYSTEMD_UNIT_IO_READ_IOS], VALUE_COUNTER(val), NULL,
                                     &(label_pair_const_t){.name="minor", .value=minor},
                                     &(label_pair_const_t){.name="mayor", .value=mayor},
                                     &(label_pair_const_t){.name="unit", .value=unit_name},
                                     NULL);
            else if (!strcmp(key, "wios"))
                metric_family_append(&fam[FAM_SYSTEMD_UNIT_IO_WRITE_IOS], VALUE_COUNTER(val), NULL,
                                     &(label_pair_const_t){.name="minor", .value=minor},
                                     &(label_pair_const_t){.name="mayor", .value=mayor},
                                     &(label_pair_const_t){.name="unit", .value=unit_name},
                                     NULL);
            else if (!strcmp(key, "dbytes"))
                metric_family_append(&fam[FAM_SYSTEMD_UNIT_IO_DISCARTED_BYTES], VALUE_COUNTER(val), NULL,
                                     &(label_pair_const_t){.name="minor", .value=minor},
                                     &(label_pair_const_t){.name="mayor", .value=mayor},
                                     &(label_pair_const_t){.name="unit", .value=unit_name},
                                     NULL);
            else if (!strcmp(key, "dios"))
                metric_family_append(&fam[FAM_SYSTEMD_UNIT_IO_DISCARTED_IOS], VALUE_COUNTER(val), NULL,
                                     &(label_pair_const_t){.name="minor", .value=minor},
                                     &(label_pair_const_t){.name="mayor", .value=mayor},
                                     &(label_pair_const_t){.name="unit", .value=unit_name},
                                     NULL);

        }
    }

    fclose(fh);
    return 0;
}

static int read_cpu_stat_v2(int dir_fd, const char *unit_name)
{
    FILE *fh = fopenat(dir_fd, "cpu.stat", "r");
    if (fh == NULL) {
        PLUGIN_DEBUG("fdopen ('%s') failed: %s", unit_name, STRERRNO);
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
            metric_family_append(&fam[FAM_SYSTEMD_UNIT_CPU_USAGE_SECONDS],
                                 VALUE_COUNTER_FLOAT64((double)counter / 1000000.0), NULL,
                                 &(label_pair_const_t){.name="unit", .value=unit_name}, NULL);
        else if (!strcmp(key, "user_usec"))
            metric_family_append(&fam[FAM_SYSTEMD_UNIT_CPU_USER_SECONDS],
                                 VALUE_COUNTER_FLOAT64((double)counter / 1000000.0), NULL,
                                 &(label_pair_const_t){.name="unit", .value=unit_name}, NULL);
        else if (!strcmp(key, "system_usec"))
            metric_family_append(&fam[FAM_SYSTEMD_UNIT_CPU_SYSTEM_SECONDS],
                                 VALUE_COUNTER_FLOAT64((double)counter / 1000000.0), NULL,
                                 &(label_pair_const_t){.name="unit", .value=unit_name}, NULL);
        else if (!strcmp(key, "nr_periods"))
            metric_family_append(&fam[FAM_SYSTEMD_UNIT_CPU_PERIODS],
                                 VALUE_COUNTER_FLOAT64((double)counter), NULL,
                                 &(label_pair_const_t){.name="unit", .value=unit_name}, NULL);
        else if (!strcmp(key, "nr_throttled"))
            metric_family_append(&fam[FAM_SYSTEMD_UNIT_CPU_THROTTLED],
                                 VALUE_COUNTER_FLOAT64((double)counter), NULL,
                                 &(label_pair_const_t){.name="unit", .value=unit_name}, NULL);
        else if (!strcmp(key, "throttled_usec"))
            metric_family_append(&fam[FAM_SYSTEMD_UNIT_CPU_THROTTLED_SECONDS],
                                 VALUE_COUNTER_FLOAT64((double)counter / 1000000.0), NULL,
                                 &(label_pair_const_t){.name="unit", .value=unit_name}, NULL);
    }

    fclose(fh);

    return 0;
}

static int read_memory_numa_stat(int dir_fd, const char *unit_name)
{
    FILE *fh = fopenat(dir_fd, "memory.numa_stat", "r");
    if (fh == NULL) {
        PLUGIN_DEBUG("fdopen ('memory.numa_stat') at '%s' failed: %s", unit_name, STRERRNO);
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
            if (fam[ms->numa_fam].type == METRIC_TYPE_COUNTER) {
                uint64_t raw = 0;
                int status = strtouint(number, &raw);
                if (status != 0)
                    continue;
                value = VALUE_COUNTER(raw);
            } else if (fam[ms->numa_fam].type == METRIC_TYPE_GAUGE) {
                double raw = 0;
                int status = strtodouble(number, &raw);
                if (status != 0)
                    continue;
                value = VALUE_GAUGE(raw);
            } else {
                continue;
            }

            metric_family_append(&fam[ms->numa_fam], value, NULL,
                                 &(label_pair_const_t){.name="zone", .value=zone},
                                 &(label_pair_const_t){.name="unit", .value=unit_name},
                                 NULL);
        }
    }

    fclose(fh);
    return 0;
}

static int read_memory_stat(int dir_fd, const char *unit_name)
{
    FILE *fh = fopenat(dir_fd, "memory.stat", "r");
    if (fh == NULL) {
        PLUGIN_DEBUG("fdopen ('memory.stat') at '%s' failed: %s", unit_name, STRERRNO);
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
        if (fam[ms->fam].type == METRIC_TYPE_COUNTER) {
            uint64_t raw = 0;
            int status = strtouint(fields[1], &raw);
            if (status != 0)
                continue;
            value = VALUE_COUNTER(raw);
        } else if (fam[ms->fam].type == METRIC_TYPE_GAUGE) {
            double raw = 0;
            int status = strtodouble(fields[1], &raw);
            if (status != 0)
                continue;
            value = VALUE_GAUGE(raw);
        } else {
            continue;
        }

        metric_family_append(&fam[ms->fam], value, NULL,
                             &(label_pair_const_t){.name="unit", .value=unit_name}, NULL);
    }

    fclose(fh);

    return 0;
}

static int read_pressure_file(int dir_fd, const char *filename, const char *unit_name,
                               metric_family_t *fam_waiting, metric_family_t *fam_stalled)
{
    FILE *fh = fopenat(dir_fd, filename, "r");
    if (fh == NULL) {
        PLUGIN_DEBUG("fdopen ('%s') at %s failed: %s", filename, unit_name, STRERRNO);
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
                                 &(label_pair_const_t){.name="unit", .value=unit_name}, NULL);
        } else if ((strcmp(fields[0], "full") == 0) && (fam_stalled != NULL)) {
            metric_family_append(fam_stalled, value, NULL,
                                 &(label_pair_const_t){.name="unit", .value=unit_name}, NULL);
        }
    }

    fclose(fh);

    return 0;
}

static int read_cgroup_file(int dir_fd, const char *filename,
                            const char *unit_name, metric_family_t *fam_file)
{
    value_t value = {0};

    if (fam_file->type == METRIC_TYPE_COUNTER)  {
        uint64_t raw = 0;
        int status = filetouint_at(dir_fd, filename, &raw);
        if (status != 0)
            return -1;
        value = VALUE_COUNTER(raw);
    } else if (fam_file->type == METRIC_TYPE_GAUGE) {
        double raw = 0;
        int status = filetodouble_at(dir_fd, filename, &raw);
        if (status != 0)
            return -1;
        value = VALUE_GAUGE(raw);
    } else {
        return -1;
    }

    metric_family_append(fam_file, value, NULL,
                         &(label_pair_const_t){.name="unit", .value=unit_name}, NULL);

    return 0;
}

static char *cgroup_systemd_path(cgroup_systemd_t type)
{
    switch(type) {
    case CGROUP_UNKNOWN:
        return NULL;
    case CGROUP_V2_SYSTEMD:
        return "/sys/fs/cgroup/systemd";
    case CGROUP_V2_UNIFIED:
        return "/sys/fs/cgroup/unified";
    case CGROUP_V2_ALL:
        return "/sys/fs/cgroup";
    }
    return NULL;
}

static int read_cgroup(int cgroup_fd, sd_bus *bus, char *unit_path, const char *unit_name,
                       char *interface)
{
    if (cgroup_fd < 0)
        return -1;

    char control_group[PATH_MAX];
    control_group[0] = '\0';
    int status = get_property_buffer(bus, "org.freedesktop.systemd1", unit_path, interface,
                                     "ControlGroup", control_group, sizeof(control_group));
    if (status != 0)
        return -1;

    if (control_group[0] == '\0')
        return -1;

    char *cgroup_path = control_group;
    if (cgroup_path[0] == '/')
        cgroup_path++;
    if (cgroup_path[0] == '\0')
        return -1;

    int dir_fd = openat(cgroup_fd, cgroup_path, O_RDONLY | O_DIRECTORY);
    if (dir_fd < 0) {
        PLUGIN_DEBUG("Cannot open '%s': %s", cgroup_path, STRERRNO);
        return -1;
    }

    read_cpu_stat_v2(dir_fd, unit_name);

    read_cgroup_file(dir_fd, "pids.current", unit_name,
                             &fam[FAM_SYSTEMD_UNIT_PROCESSES]);

    read_io_stat(dir_fd, unit_name);

    read_cgroup_file(dir_fd, "memory.current", unit_name,
                             &fam[FAM_SYSTEMD_UNIT_MEMORY_BYTES]);

    read_cgroup_file(dir_fd, "memory.swap.current", unit_name,
                             &fam[FAM_SYSTEMD_UNIT_MEMORY_SWAP_BYTES]);

    read_memory_stat(dir_fd, unit_name);

    read_memory_numa_stat(dir_fd, unit_name);

    read_pressure_file(dir_fd, "cpu.pressure", unit_name,
                               &fam[FAM_SYSTEMD_UNIT_PRESSURE_CPU_WAITING],
                               &fam[FAM_SYSTEMD_UNIT_PRESSURE_CPU_STALLED]);

    read_pressure_file(dir_fd, "io.pressure", unit_name,
                               &fam[FAM_SYSTEMD_UNIT_PRESSURE_IO_WAITING],
                               &fam[FAM_SYSTEMD_UNIT_PRESSURE_IO_STALLED]);

    read_pressure_file(dir_fd, "memory.pressure", unit_name,
                               &fam[FAM_SYSTEMD_UNIT_PRESSURE_MEMORY_WAITING],
                               &fam[FAM_SYSTEMD_UNIT_PRESSURE_MEMORY_STALLED]);

    close(dir_fd);

    return 0;
}

static cgroup_systemd_t cgroup_systemd_type(void)
{
    struct statfs fs;
    int status = statfs("/sys/fs/cgroup/", &fs);
    if (status < 0) {
        PLUGIN_ERROR("statfs failed on '/sys/fs/cgroup/': %s", STRERRNO);
        return CGROUP_UNKNOWN;
    }

    if (fs.f_type == CGROUP2_SUPER_MAGIC) {
        PLUGIN_INFO("Found cgroup2 on /sys/fs/cgroup/, full unified hierarchy.");
        return CGROUP_V2_ALL;
    }

    if (fs.f_type == SYSFS_MAGIC) {
        PLUGIN_ERROR("No filesystem is currently mounted on /sys/fs/cgroup.");
        return CGROUP_UNKNOWN;
    }

    if (fs.f_type != TMPFS_MAGIC) {
        PLUGIN_ERROR("Unknown filesystem type %llx mounted on /sys/fs/cgroup.",
                     (unsigned long long)fs.f_type);
        return CGROUP_UNKNOWN;
    }

    status = statfs("/sys/fs/cgroup/unified/", &fs);
    if ((status == 0) && (fs.f_type == CGROUP2_SUPER_MAGIC)) {
        PLUGIN_INFO("Found cgroup2 on /sys/fs/cgroup/unified, unified hierarchy for systemd.");
        return CGROUP_V2_UNIFIED;
    }

    status = statfs("/sys/fs/cgroup/systemd/", &fs);
    if (status < 0) {
        if (errno == ENOENT) {
            PLUGIN_ERROR("Unsupported cgroupsv1 setup detected: systemd hierarchy not found.");
            return CGROUP_UNKNOWN;
        }
        PLUGIN_ERROR("statfs failed on '/sys/fs/cgroup/systemd': %s", STRERRNO);
        return CGROUP_UNKNOWN;
    }

    if (fs.f_type == CGROUP2_SUPER_MAGIC) {
        PLUGIN_INFO("Found cgroup2 on /sys/fs/cgroup/systemd, "
                    "unified hierarchy for systemd controller (v232 variant)");
        return CGROUP_V2_SYSTEMD;
    }

    PLUGIN_INFO("Unexpected filesystem type %llx mounted on /sys/fs/cgroup/systemd.",
                 (unsigned long long) fs.f_type);
    return CGROUP_UNKNOWN;
}

static int unit_service(sd_bus *bus, char *name, char *unit_path)
{
    uint32_t r = 0;
    int status = get_property_uint32(bus, "org.freedesktop.systemd1", unit_path,
                                          "org.freedesktop.systemd1.Service", "NRestarts", &r);
    if (status == 0)
        metric_family_append(&fam[FAM_SYSTEMD_SERVICE_RESTART], VALUE_COUNTER(r), NULL,
                             &(label_pair_const_t){.name="unit", .value = name}, NULL);

    uint64_t tasks = 0;
    status = get_property_uint64(bus, "org.freedesktop.systemd1", unit_path,
                                      "org.freedesktop.systemd1.Service", "TasksCurrent", &tasks);
    if (status == 0)
        metric_family_append(&fam[FAM_SYSTEMD_UNIT_TASKS_CURRENT], VALUE_GAUGE(tasks), NULL,
                             &(label_pair_const_t){.name="unit", .value = name}, NULL);

    uint64_t tasks_max = 0;
    status = get_property_uint64(bus, "org.freedesktop.systemd1", unit_path,
                                      "org.freedesktop.systemd1.Service", "TasksMax", &tasks_max);
    if (status == 0)
        metric_family_append(&fam[FAM_SYSTEMD_UNIT_TASKS_MAX], VALUE_GAUGE(tasks_max), NULL,
                             &(label_pair_const_t){.name="unit", .value = name}, NULL);

    return 0;
}

static int unit_socket(sd_bus *bus, char *name, char *unit_path)
{
    uint32_t accepted = 0;
    int status = get_property_uint32(bus, "org.freedesktop.systemd1", unit_path,
                                          "org.freedesktop.systemd1.Socket",
                                          "NAccepted", &accepted);
    if (status == 0)
        metric_family_append(&fam[FAM_SYSTEMD_SOCKET_ACCEPTED_CONNECTIONS],
                             VALUE_COUNTER(accepted), NULL,
                             &(label_pair_const_t){.name="unit", .value = name}, NULL);

    uint32_t connections = 0;
    status = get_property_uint32(bus, "org.freedesktop.systemd1", unit_path,
                                      "org.freedesktop.systemd1.Socket",
                                      "NConnections", &connections);
    if (status == 0)
        metric_family_append(&fam[FAM_SYSTEMD_SOCKET_CURRENT_CONNECTIONS],
                             VALUE_GAUGE(connections), NULL,
                             &(label_pair_const_t){.name="unit", .value = name}, NULL);

    uint32_t refused = 0;
    status = get_property_uint32(bus, "org.freedesktop.systemd1", unit_path,
                                      "org.freedesktop.systemd1.Socket", "NRefused", &refused);
    if (status == 0)
        metric_family_append(&fam[FAM_SYSTEMD_SOCKET_REFUSED_CONNECTIONS],
                             VALUE_COUNTER(refused), NULL,
                             &(label_pair_const_t){.name="unit", .value = name}, NULL);

    return 0;
}

static int unit_timer(sd_bus *bus, char *name, char *unit_path)
{
    uint64_t last = 0;
    int status = get_property_uint64(bus, "org.freedesktop.systemd1", unit_path,
                                          "org.freedesktop.systemd1.Timer",
                                          "LastTriggerUSec", &last);
    if (status == 0) {
        double last_trigger = last/1e6;
        metric_family_append(&fam[FAM_SYSTEMD_TIMER_LAST_TRIGGER_SECONDS],
                             VALUE_GAUGE(last_trigger), NULL,
                             &(label_pair_const_t){.name="unit", .value = name}, NULL);
    }

    return 0;
}

static int submit_unit(int cgroup_fd, sd_bus *bus, char *unit, char *unit_path,
                       char *load_state, char *active_state, char *sub_state)
{
    state_t load_states[] = {
        { .name = "stub",        .enabled = false },
        { .name = "loaded",      .enabled = false },
        { .name = "not-found",   .enabled = false },
        { .name = "bad-setting", .enabled = false },
        { .name = "error",       .enabled = false },
        { .name = "merged",      .enabled = false },
        { .name = "masked",      .enabled = false },
    };
    state_set_t load_set = { .num = STATIC_ARRAY_SIZE(load_states), .ptr = load_states };
    for (size_t j = 0; j < load_set.num ; j++) {
        if (strcmp(load_set.ptr[j].name, load_state) == 0) {
            load_set.ptr[j].enabled = true;
            break;
        }
    }
    metric_family_append(&fam[FAM_SYSTEMD_UNIT_LOAD_STATE], VALUE_STATE_SET(load_set), NULL,
                         &(label_pair_const_t){.name="unit", .value=unit}, NULL);

    state_t active_states[] = {
        { .name = "active",       .enabled = false },
        { .name = "reloading",    .enabled = false },
        { .name = "inactive",     .enabled = false },
        { .name = "failed",       .enabled = false },
        { .name = "activating",   .enabled = false },
        { .name = "deactivating", .enabled = false },
        { .name = "maintenance",  .enabled = false },
    };
    state_set_t active_set = { .num = STATIC_ARRAY_SIZE(active_states), .ptr = active_states };
    for (size_t j = 0; j < active_set.num ; j++) {
        if (strcmp(active_set.ptr[j].name, active_state) == 0) {
            active_set.ptr[j].enabled = true;
            break;
        }
    }
    metric_family_append(&fam[FAM_SYSTEMD_UNIT_ACTIVE_STATE], VALUE_STATE_SET(active_set), NULL,
                         &(label_pair_const_t){.name="unit", .value=unit}, NULL);

    state_t sub_states[] = {
        { .name = "dead",                       .enabled = false },
        { .name = "condition",                  .enabled = false },
        { .name = "start-pre",                  .enabled = false },
        { .name = "start",                      .enabled = false },
        { .name = "start-post",                 .enabled = false },
        { .name = "running",                    .enabled = false },
        { .name = "exited",                     .enabled = false },
        { .name = "reload",                     .enabled = false },
        { .name = "reload-signal",              .enabled = false },
        { .name = "reload-notify",              .enabled = false },
        { .name = "stop",                       .enabled = false },
        { .name = "stop-watchdog",              .enabled = false },
        { .name = "stop-sigterm",               .enabled = false },
        { .name = "stop-sigkill",               .enabled = false },
        { .name = "stop-post",                  .enabled = false },
        { .name = "final-watchdog",             .enabled = false },
        { .name = "final-sigterm",              .enabled = false },
        { .name = "final-sigkill",              .enabled = false },
        { .name = "failed",                     .enabled = false },
        { .name = "dead-before-auto-restart",   .enabled = false },
        { .name = "failed-before-auto-restart", .enabled = false },
        { .name = "dead-resources-pinned",      .enabled = false },
        { .name = "auto-restart",               .enabled = false },
        { .name = "auto-restart-queued",        .enabled = false },
        { .name = "cleaning",                   .enabled = false },
    };
    state_set_t sub_set = { .num = STATIC_ARRAY_SIZE(sub_states), .ptr = sub_states };
    for (size_t j = 0; j < sub_set.num ; j++) {
        if (strcmp(sub_set.ptr[j].name, sub_state) == 0) {
            sub_set.ptr[j].enabled = true;
            break;
        }
    }
    metric_family_append(&fam[FAM_SYSTEMD_UNIT_SUB_STATE], VALUE_STATE_SET(sub_set), NULL,
                         &(label_pair_const_t){.name="unit", .value=unit}, NULL);


    double start_time = 0.0;
    if (strcmp(active_state, "active") == 0) {
        uint64_t timestamp = 0;
        int status = get_property_uint64(bus, "org.freedesktop.systemd1", unit_path,
                                              "org.freedesktop.systemd1.Unit",
                                              "ActiveEnterTimestamp", &timestamp);
        if (status == 0)
            start_time = (double)timestamp/1e6;
    }

    metric_family_append(&fam[FAM_SYSTEMD_UNIT_START_TIME_SECONDS], VALUE_GAUGE(start_time), NULL,
                         &(label_pair_const_t){.name="unit", .value=unit}, NULL);


    if (strendswith(unit, ".service")) {
        unit_service(bus, unit, unit_path);
        read_cgroup(cgroup_fd, bus, unit_path, unit, "org.freedesktop.systemd1.Service");
    } else if (strendswith(unit, ".socket")) {
        unit_socket(bus, unit, unit_path);
        read_cgroup(cgroup_fd, bus, unit_path, unit, "org.freedesktop.systemd1.Socket");
    } else if (strendswith(unit, ".timer")) {
        unit_timer(bus, unit, unit_path);
    } else if (strendswith(unit, ".slice")) {
        read_cgroup(cgroup_fd, bus, unit_path, unit, "org.freedesktop.systemd1.Slice");
    }

    return 0;
}

static int systemd_read(void)
{
    sd_bus *bus = NULL;
    sd_bus_message *reply = NULL;

    if (sd_booted() <= 0)
        return -1;

    if (geteuid() != 0) {
        sd_bus_default_system(&bus);
    } else {
        int status = sd_bus_new(&bus);
        if (status < 0)
          return -1;

        status = sd_bus_set_address(bus, "unix:path=/run/systemd/private");
        if (status < 0)
           return -1;

        status = sd_bus_start(bus);
        if (status < 0)
           sd_bus_default_system(&bus);
    }

    sd_bus_error error = SD_BUS_ERROR_NULL;

    if (units_size == 0) {
        int status = sd_bus_call_method(bus, "org.freedesktop.systemd1",
                                             "/org/freedesktop/systemd1",
                                             "org.freedesktop.systemd1.Manager",
                                             "ListUnits", &error, &reply, NULL);
        if (status < 0)
           return -1;
    } else {
        sd_bus_message *m = NULL;
        int status = sd_bus_message_new_method_call(bus, &m, "org.freedesktop.systemd1",
                                                             "/org/freedesktop/systemd1",
                                                             "org.freedesktop.systemd1.Manager",
                                                             "ListUnitsByNames");
        if (status < 0)
            return -1;

        status = sd_bus_message_append_strv(m, units);
        if (status < 0) {
            sd_bus_message_unref(m);
            return -1;
        }

        status = sd_bus_call(bus, m, 0, &error, &reply);
        if (status < 0) {
            sd_bus_message_unref(m);
            return -1;
        }

        sd_bus_message_unref(m);
    }

    sd_bus_error_free(&error);

    int status = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(ssssssouso)");
    if (status < 0) {
        sd_bus_message_unref(reply);
        sd_bus_unref(bus);
        return -1;
    }

    int cgroup_fd = -1;
    if (cgroup_type != CGROUP_UNKNOWN) {
        char *cgroup_path = cgroup_systemd_path(cgroup_type);
        if (cgroup_path != NULL) {
            cgroup_fd = open(cgroup_path, O_RDONLY | O_DIRECTORY);
            if (cgroup_fd < 0) {
                PLUGIN_ERROR("Cannot open '%s': %s", cgroup_path, STRERRNO);
                sd_bus_message_exit_container(reply);
                sd_bus_message_unref(reply);
                sd_bus_unref(bus);
                return -1;
            }
        }
    }

    while (true) {
        char *unit = NULL;
        char *load_state = NULL;
        char *active_state = NULL;
        char *sub_state = NULL;
        char *unit_path = NULL;

        status = sd_bus_message_read(reply, "(ssssssouso)", &unit, NULL, &load_state,
                                                            &active_state, &sub_state, NULL,
                                                            &unit_path, NULL, NULL, NULL);
        if (status <= 0)
          break;

        submit_unit(cgroup_fd, bus, unit, unit_path, load_state, active_state, sub_state);
    }

    if (cgroup_fd >= 0)
        close(cgroup_fd);

    sd_bus_message_exit_container(reply);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);

    plugin_dispatch_metric_family_array(fam, FAM_SYSTEMD_MAX, 0);

    return 0;
}

static int systemd_config_append_unit(config_item_t *ci, char *suffix)
{
    if (ci == NULL)
        return -1;

    if (ci->values_num <= 0) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires a list of strings.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    for (int i=0 ; i < ci->values_num; i++) {
        if (ci->values[i].type != CONFIG_TYPE_STRING) {
            PLUGIN_ERROR("The argument %d in option '%s' at %s:%d must be a string.",
                         i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }
    }

    size_t new_size = units_size + ci->values_num + 1;

    char **tmp = realloc(units, sizeof(char *)*new_size);
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed");
        return -1;
    }

    units = tmp;

    for (int i=0 ; i < ci->values_num; i++) {
        units[units_size + i] = NULL;
    }

    size_t old_units_size = units_size;
    units_size = units_size + ci->values_num;

    for (int i=0 ; i < ci->values_num; i++) {
        if (suffix != NULL) {
            if (strendswith(ci->values[i].value.string, suffix)) {
                char *str = strdup(ci->values[i].value.string);
                if (str == NULL) {
                    PLUGIN_ERROR("strdup failed");
                    return -1;
                }
                units[old_units_size + i] = str;
            } else {
                size_t len = strlen(ci->values[i].value.string);
                size_t suffix_len = strlen(suffix);
                char *str = malloc(len + suffix_len + 1);
                if (str == NULL) {
                    PLUGIN_ERROR("strdup failed");
                    return -1;
                }
                ssnprintf(str, len + suffix_len + 1, "%s%s", ci->values[i].value.string, suffix);
                units[old_units_size + i] = str;
            }
        } else {
            char *str = strdup(ci->values[i].value.string);
            if (str == NULL) {
                PLUGIN_ERROR("strdup failed");
                return -1;
            }
            units[old_units_size + i] = str;
        }
    }

    units[units_size] = NULL;

    return 0;
}

static int systemd_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "unit") == 0) {
            status = systemd_config_append_unit(child, NULL);
        } else if (strcasecmp(child->key, "service") == 0) {
            status = systemd_config_append_unit(child, ".service");
        } else if (strcasecmp(child->key, "socket") == 0) {
            status = systemd_config_append_unit(child, ".socket");
        } else if (strcasecmp(child->key, "timer") == 0) {
            status = systemd_config_append_unit(child, ".timer");
        } else if (strcasecmp(child->key, "slice") == 0) {
            status = systemd_config_append_unit(child, ".slice");
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0)
        return -1;

    return 0;
}

static int systemd_shutdown(void)
{
    if (units == NULL)
        return 0;

    for (size_t i = 0; i < units_size; i++) {
        free(units[i]);
    }

    free(units);

    units_size = 0;
    units = NULL;

    return 0;
}

static int systemd_init(void)
{
    cgroup_type = cgroup_systemd_type();
    return 0;
}

void module_register(void)
{
    plugin_register_init("systemd", systemd_init);
    plugin_register_config("systemd", systemd_config);
    plugin_register_read("systemd", systemd_read);
    plugin_register_shutdown("systemd", systemd_shutdown);
}
