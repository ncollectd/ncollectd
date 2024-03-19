// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2006-2008 Red Hat Inc.
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Richard W.M. Jones <rjones at redhat.com>
// SPDX-FileContributor: Przemyslaw Szczerbik <przemyslawx.szczerbik at intel.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/complain.h"
#include "libutils/exclist.h"
#include "libutils/itoa.h"

#include <libgen.h> /* for basename(3) */
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>


#ifdef LIBVIR_CHECK_VERSION

#if LIBVIR_CHECK_VERSION(0, 9, 2)
#define HAVE_DOM_REASON 1
#endif

#if LIBVIR_CHECK_VERSION(0, 9, 5)
#define HAVE_BLOCK_STATS_FLAGS 1
#define HAVE_DOM_REASON_PAUSED_SHUTTING_DOWN 1
#endif

#if LIBVIR_CHECK_VERSION(0, 9, 10)
#define HAVE_DISK_ERR 1
#endif

#if LIBVIR_CHECK_VERSION(0, 9, 11)
#define HAVE_CPU_STATS 1
#define HAVE_DOM_STATE_PMSUSPENDED 1
#define HAVE_DOM_REASON_RUNNING_WAKEUP 1
#endif

/*
    virConnectListAllDomains() appeared in 0.10.2 (Sep 2012)
    Note that LIBVIR_CHECK_VERSION appeared a year later (Dec 2013,
    libvirt-1.2.0),
    so in some systems which actually have virConnectListAllDomains()
    we can't detect this.
 */
#if LIBVIR_CHECK_VERSION(0, 10, 2)
#define HAVE_LIST_ALL_DOMAINS 1
#endif

#if LIBVIR_CHECK_VERSION(1, 0, 1)
#define HAVE_DOM_REASON_PAUSED_SNAPSHOT 1
#endif

#if LIBVIR_CHECK_VERSION(1, 1, 1)
#define HAVE_DOM_REASON_PAUSED_CRASHED 1
#endif

#if LIBVIR_CHECK_VERSION(1, 2, 10)
#define HAVE_DOM_REASON_CRASHED 1
#endif

#if LIBVIR_CHECK_VERSION(1, 2, 11)
#define HAVE_FS_INFO 1
#endif

#if LIBVIR_CHECK_VERSION(1, 2, 15)
#define HAVE_DOM_REASON_PAUSED_STARTING_UP 1
#endif

#if LIBVIR_CHECK_VERSION(1, 3, 3)
#define HAVE_PERF_STATS 1
#define HAVE_DOM_REASON_POSTCOPY 1
#endif

#if LIBVIR_CHECK_VERSION(4, 10, 0)
#define HAVE_DOM_REASON_SHUTOFF_DAEMON 1
#endif

#if LIBVIR_CHECK_VERSION(5, 4, 0)
#define HAVE_VIR_DOMAIN_MEMORY_STAT_HUGETLB_PGALLOC 1
#define HAVE_VIR_DOMAIN_MEMORY_STAT_HUGETLB_PGFAIL 1
#endif

#endif

enum {
    FAM_VIRT_UP,
    FAM_VIRT_DOMAIN_STATE,
    FAM_VIRT_DOMAIN_STATE_NUMBER,
    FAM_VIRT_DOMAIN_REASON_NUMBER,
    FAM_VIRT_DOMAIN_FS,
    FAM_VIRT_DOMAIN_DISK_ERROR,
    FAM_VIRT_DOMAIN_VCPUS,
    FAM_VIRT_DOMAIN_VCPU_TIME_SECONDS,
    FAM_VIRT_DOMAIN_CPU_AFFINITY,
    FAM_VIRT_DOMAIN_VCPU_ALL_TIME_SECONDS,
    FAM_VIRT_DOMAIN_VCPU_ALL_SYSTEM_TIME_SECONDS,
    FAM_VIRT_DOMAIN_VCPU_ALL_USER_TIME_SECONDS,
    FAM_VIRT_DOMAIN_MEMORY_MAX_BYTES,
    FAM_VIRT_DOMAIN_MEMORY_BYTES,
    FAM_VIRT_DOMAIN_SWAP_IN_BYTES,
    FAM_VIRT_DOMAIN_SWAP_OUT_BYTES,
    FAM_VIRT_DOMAIN_MEMORY_UNUSED_BYTES,
    FAM_VIRT_DOMAIN_MEMORY_AVAILABLE_BYTES,
    FAM_VIRT_DOMAIN_MEMORY_USABLE_BYTES,
    FAM_VIRT_DOMAIN_MEMORY_RSS_BYTES,
    FAM_VIRT_DOMAIN_MEMORY_BALLOON_BYTES,
    FAM_VIRT_DOMAIN_MEMORY_MAJOR_PAGE_FAULT,
    FAM_VIRT_DOMAIN_MEMORY_MINOR_PAGE_FAULT,
    FAM_VIRT_DOMAIN_MEMORY_DISK_CACHE_BYTES,
    FAM_VIRT_DOMAIN_MEMORY_HUGETLB_PAGE_ALLOC,
    FAM_VIRT_DOMAIN_MEMORY_HUGETLB_PAGE_FAIL,
    FAM_VIRT_DOMAIN_INTERFACE_RECEIVE_BYTES,
    FAM_VIRT_DOMAIN_INTERFACE_RECEIVE_PACKETS,
    FAM_VIRT_DOMAIN_INTERFACE_RECEIVE_ERRORS,
    FAM_VIRT_DOMAIN_INTERFACE_RECEIVE_DROPS,
    FAM_VIRT_DOMAIN_INTERFACE_TRANSMIT_BYTES,
    FAM_VIRT_DOMAIN_INTERFACE_TRANSMIT_PACKETS,
    FAM_VIRT_DOMAIN_INTERFACE_TRANSMIT_ERRORS,
    FAM_VIRT_DOMAIN_INTERFACE_TRANSMIT_DROPS,
    FAM_VIRT_DOMAIN_BLOCK_READ_BYTES,
    FAM_VIRT_DOMAIN_BLOCK_READ_REQUESTS,
    FAM_VIRT_DOMAIN_BLOCK_READ_TIME_SECONDS,
    FAM_VIRT_DOMAIN_BLOCK_WRITE_BYTES,
    FAM_VIRT_DOMAIN_BLOCK_WRITE_REQUESTS,
    FAM_VIRT_DOMAIN_BLOCK_WRITE_TIME_SECONDS,
    FAM_VIRT_DOMAIN_BLOCK_FLUSH_REQUESTS,
    FAM_VIRT_DOMAIN_BLOCK_FLUSH_TIME_SECONDS,
    FAM_VIRT_DOMAIN_BLOCK_ALLOCATION,
    FAM_VIRT_DOMAIN_BLOCK_CAPACITY,
    FAM_VIRT_DOMAIN_BLOCK_PHYSICALSIZE,
    FAM_VIRT_DOMAIN_PERF_CMT,
    FAM_VIRT_DOMAIN_PERF_MBMT,
    FAM_VIRT_DOMAIN_PERF_MBML,
    FAM_VIRT_DOMAIN_PERF_CACHE_MISSES,
    FAM_VIRT_DOMAIN_PERF_CACHE_REFERENCES,
    FAM_VIRT_DOMAIN_PERF_INSTRUCTIONS,
    FAM_VIRT_DOMAIN_PERF_CPU_CYCLES,
    FAM_VIRT_DOMAIN_PERF_BRANCH_INSTRUCTIONS,
    FAM_VIRT_DOMAIN_PERF_BRANCH_MISSES,
    FAM_VIRT_DOMAIN_PERF_BUS_CYCLES,
    FAM_VIRT_DOMAIN_PERF_STALLED_CYCLES_FRONTEND,
    FAM_VIRT_DOMAIN_PERF_STALLED_CYCLES_BACKEND,
    FAM_VIRT_DOMAIN_PERF_REF_CPU_CYCLES,
    FAM_VIRT_DOMAIN_PERF_CPU_CLOCK,
    FAM_VIRT_DOMAIN_PERF_TASK_CLOCK,
    FAM_VIRT_DOMAIN_PERF_PAGE_FAULTS,
    FAM_VIRT_DOMAIN_PERF_CONTEXT_SWITCHES,
    FAM_VIRT_DOMAIN_PERF_CPU_MIGRATIONS,
    FAM_VIRT_DOMAIN_PERF_PAGE_FAULTS_MIN,
    FAM_VIRT_DOMAIN_PERF_PAGE_FAULTS_MAJ,
    FAM_VIRT_DOMAIN_PERF_ALIGNMENT_FAULTS,
    FAM_VIRT_DOMAIN_PERF_EMULATION_FAULTS,
    FAM_VIRT_MAX
};

static metric_family_t fams_virt[FAM_VIRT_MAX] = {
    [FAM_VIRT_UP] = {
        .name = "virt_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Can connecto to libvirt."
    },
    [FAM_VIRT_DOMAIN_STATE] = {
        .name = "virt_domain_state",
        .type = METRIC_TYPE_STATE_SET,
        .help = "Domain state.",
    },
    [FAM_VIRT_DOMAIN_STATE_NUMBER] = {
        .name = "virt_domain_state_number",
        .type = METRIC_TYPE_GAUGE,
        .help = "Domain state number.",
    },
    [FAM_VIRT_DOMAIN_REASON_NUMBER] = {
        .name = "virt_domain_reason_number",
        .type = METRIC_TYPE_GAUGE,
        .help = "Domain reason number.",
    },
    [FAM_VIRT_DOMAIN_FS] = {
        .name = "virt_domain_fs",
        .type = METRIC_TYPE_INFO,
        .help = "File system information.",
    },
    [FAM_VIRT_DOMAIN_DISK_ERROR] = {
        .name = "virt_domain_disk_error",
        .type = METRIC_TYPE_STATE_SET,
        .help = "Domain disks errors.",
    },
    [FAM_VIRT_DOMAIN_VCPUS] = {
        .name = "virt_domain_vcpus",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of virtual CPUs for the domain.",
    },
    [FAM_VIRT_DOMAIN_VCPU_TIME_SECONDS] = {
        .name = "virt_domain_vcpu_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VIRT_DOMAIN_CPU_AFFINITY] = {
        .name = "virt_domain_cpu_affinity",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_VIRT_DOMAIN_VCPU_ALL_TIME_SECONDS] = {
        .name = "virt_domain_vcpu_all_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of CPU time used by the domain's VCPU, in seconds.",
    },
    [FAM_VIRT_DOMAIN_VCPU_ALL_SYSTEM_TIME_SECONDS] = {
        .name = "virt_domain_vcpu_all_system_time_seconds",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_VIRT_DOMAIN_VCPU_ALL_USER_TIME_SECONDS] = {
        .name = "virt_domain_vcpu_all_user_time_seconds",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_VIRT_DOMAIN_MEMORY_MAX_BYTES] = {
        .name = "virt_domain_memory_max_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum memory in bytes allowed.",
    },
    [FAM_VIRT_DOMAIN_MEMORY_BYTES] = {
        .name = "virt_domain_memory_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The memory in bytes used by the domain.",
    },
    [FAM_VIRT_DOMAIN_SWAP_IN_BYTES] = {
        .name = "virt_domain_swap_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total amount of data read from swap space in bytes.",
    },
    [FAM_VIRT_DOMAIN_SWAP_OUT_BYTES] = {
        .name = "virt_domain_swap_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total amount of memory written out to swap space in bytes.",
    },
    [FAM_VIRT_DOMAIN_MEMORY_UNUSED_BYTES] = {
        .name = "virt_domain_memory_unused_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The amount of memory left completely unused by the domain in bytes.",
    },
    [FAM_VIRT_DOMAIN_MEMORY_AVAILABLE_BYTES] = {
        .name = "virt_domain_memory_available_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total amount of usable memory as seen by the domain in bytes.",
    },
    [FAM_VIRT_DOMAIN_MEMORY_USABLE_BYTES] = {
        .name = "virt_domain_memory_usable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory usable of the domain(corresponds to 'Available' in /proc/meminfo), "
                "in bytes.",
    },
    [FAM_VIRT_DOMAIN_MEMORY_RSS_BYTES] = {
        .name = "virt_domain_memory_rss_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Resident Set Size of the process running the domain in bytes",
    },
    [FAM_VIRT_DOMAIN_MEMORY_BALLOON_BYTES] = {
        .name = "virt_domain_memory_balloon_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current balloon size in bytes.",
    },
    [FAM_VIRT_DOMAIN_MEMORY_MAJOR_PAGE_FAULT] = {
        .name = "virt_domain_memory_major_page_fault",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VIRT_DOMAIN_MEMORY_MINOR_PAGE_FAULT] = {
        .name = "virt_domain_memory_minor_page_fault",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_VIRT_DOMAIN_MEMORY_DISK_CACHE_BYTES] = {
        .name = "virt_domain_memory_disk_cache_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The amount of memory, that can be quickly reclaimed "
                "without additional I/O (in bytes)",
    },
    [FAM_VIRT_DOMAIN_MEMORY_HUGETLB_PAGE_ALLOC] = {
        .name = "virt_domain_memory_hugetlb_page_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of successful huge page allocations "
                "from inside the domain via virtio balloon.",
    },
    [FAM_VIRT_DOMAIN_MEMORY_HUGETLB_PAGE_FAIL] = {
        .name = "virt_domain_memory_hugetlb_page_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of failed huge page allocations "
                "from inside the domain via virtio balloon.",
    },
    [FAM_VIRT_DOMAIN_INTERFACE_RECEIVE_BYTES] = {
        .name = "virt_domain_interface_receive_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes received on a network interface, in bytes.",
    },
    [FAM_VIRT_DOMAIN_INTERFACE_RECEIVE_PACKETS] = {
        .name = "virt_domain_interface_receive_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packets received on a network interface.",
    },
    [FAM_VIRT_DOMAIN_INTERFACE_RECEIVE_ERRORS] = {
        .name = "virt_domain_interface_receive_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packet receive errors on a network interface.",
    },
    [FAM_VIRT_DOMAIN_INTERFACE_RECEIVE_DROPS] = {
        .name = "virt_domain_interface_receive_drops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packet receive drops on a network interface.",
    },
    [FAM_VIRT_DOMAIN_INTERFACE_TRANSMIT_BYTES] = {
        .name = "virt_domain_interface_transmit_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes transmitted on a network interface, in bytes.",
    },
    [FAM_VIRT_DOMAIN_INTERFACE_TRANSMIT_PACKETS] = {
        .name = "virt_domain_interface_transmit_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packets transmitted on a network interface.",
    },
    [FAM_VIRT_DOMAIN_INTERFACE_TRANSMIT_ERRORS] = {
        .name = "virt_domain_interface_transmit_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packet transmit errors on a network interface.",
    },
    [FAM_VIRT_DOMAIN_INTERFACE_TRANSMIT_DROPS] = {
        .name = "virt_domain_interface_transmit_drops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packet transmit drops on a network interface.",
    },
    [FAM_VIRT_DOMAIN_BLOCK_READ_BYTES] = {
        .name = "virt_domain_block_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes read from a block device, in bytes.",
    },
    [FAM_VIRT_DOMAIN_BLOCK_READ_REQUESTS] = {
        .name = "virt_domain_block_read_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of read requests from a block device.",
    },
    [FAM_VIRT_DOMAIN_BLOCK_READ_TIME_SECONDS] = {
        .name = "virt_domain_block_read_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total time spent on reads from a block device, in seconds.",
    },
    [FAM_VIRT_DOMAIN_BLOCK_WRITE_BYTES] = {
        .name = "virt_domain_block_write_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes written to a block device, in bytes.",
    },
    [FAM_VIRT_DOMAIN_BLOCK_WRITE_REQUESTS] = {
        .name = "virt_domain_block_write_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of write requests to a block device.",
    },
    [FAM_VIRT_DOMAIN_BLOCK_WRITE_TIME_SECONDS] = {
        .name = "virt_domain_block_write_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total time spent on writes on a block device, in seconds.",
    },
    [FAM_VIRT_DOMAIN_BLOCK_FLUSH_REQUESTS] = {
        .name = "virt_domain_block_flush_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total flush requests from a block device.",
    },
    [FAM_VIRT_DOMAIN_BLOCK_FLUSH_TIME_SECONDS] = {
        .name = "virt_domain_block_flush_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total time spent on cache flushing to a block device, in seconds.",
    },
    [FAM_VIRT_DOMAIN_BLOCK_ALLOCATION] = {
        .name = "virt_domain_block_allocation",
        .type = METRIC_TYPE_GAUGE,
        .help = "Offset of the highest written sector on a block device.",
    },
    [FAM_VIRT_DOMAIN_BLOCK_CAPACITY] = {
        .name = "virt_domain_block_capacity",
        .type = METRIC_TYPE_GAUGE,
        .help = "Logical size in bytes of the block device    backing image.",
    },
    [FAM_VIRT_DOMAIN_BLOCK_PHYSICALSIZE] = {
        .name = "virt_domain_block_physicalsize",
        .type = METRIC_TYPE_GAUGE,
        .help = "Physical size in bytes of the container of the backing image.",
    },
    [FAM_VIRT_DOMAIN_PERF_CMT] = {
        .name = "virt_domain_perf_cmt",
        .type = METRIC_TYPE_COUNTER,
        .help = "CMT perf event which can be used to measure the usage of cache (bytes) by "
                "applications running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_MBMT] = {
        .name = "virt_domain_perf_mbmt",
        .type = METRIC_TYPE_COUNTER,
        .help = "MBMT perf event which can be used to monitor total system bandwidth (bytes/s) "
                "from one level of cache to another.",
    },
    [FAM_VIRT_DOMAIN_PERF_MBML] = {
        .name = "virt_domain_perf_mbml",
        .type = METRIC_TYPE_COUNTER,
        .help = "MBML perf event which can be used to monitor the amount of data (bytes/s) sent "
                "through the memory controller on the socket.",
    },
    [FAM_VIRT_DOMAIN_PERF_CACHE_MISSES] = {
        .name = "virt_domain_perf_cache_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cache_misses perf event which can be used to measure the count of cache misses by "
                "applications running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_CACHE_REFERENCES] = {
        .name = "virt_domain_perf_cache_references",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cache_references perf event which can be used to measure the count of cache hits "
                "by applications running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_INSTRUCTIONS] = {
        .name = "virt_domain_perf_instructions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Instructions perf event which can be used to measure the count of instructions "
                "by applications running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_CPU_CYCLES] = {
        .name = "virt_domain_perf_cpu_cycles",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cpu_cycles perf event describing the total/elapsed cpu cycles.",
    },
    [FAM_VIRT_DOMAIN_PERF_BRANCH_INSTRUCTIONS] = {
        .name = "virt_domain_perf_branch_instructions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Branch_instructions perf event which can be used to measure the count of "
                "branch instructions by applications running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_BRANCH_MISSES] = {
        .name = "virt_domain_perf_branch_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Branch_misses perf event which can be used to measure the count of branch misses "
                "by applications running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_BUS_CYCLES] = {
        .name = "virt_domain_perf_bus_cycles",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bus_cycles perf event which can be used to measure the count of bus cycles "
                "by applications running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_STALLED_CYCLES_FRONTEND] = {
        .name = "virt_domain_perf_stalled_cycles_frontend",
        .type = METRIC_TYPE_COUNTER,
        .help = "Stalled_cycles_frontend  perf event which can be used to measure the count "
                "of stalled cpu cycles in the frontend of the instruction processor pipeline "
                "by applications running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_STALLED_CYCLES_BACKEND] = {
        .name = "virt_domain_perf_stalled_cycles_backend",
        .type = METRIC_TYPE_COUNTER,
        .help = "Stalled_cycles_backend perf event which can be used to measure the count "
                "of stalled cpu cycles in the backend of the instruction processor pipeline "
                "by application running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_REF_CPU_CYCLES] = {
        .name = "virt_domain_perf_ref_cpu_cycles",
        .type = METRIC_TYPE_COUNTER,
        .help = "Ref_cpu_cycles perf event which can be used to measure the count of "
                "total cpu cycles not affected by CPU frequency scaling "
                 "by applications running on the platform."
    },
    [FAM_VIRT_DOMAIN_PERF_CPU_CLOCK] = {
        .name = "virt_domain_perf_cpu_clock",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cpu_clock perf event which can be used to measure the count of cpu "
                "clock time by applications running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_TASK_CLOCK] = {
        .name = "virt_domain_perf_task_clock",
        .type = METRIC_TYPE_COUNTER,
        .help = "Task_clock perf event which can be used to measure the count of task "
                "clock time by applications running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_PAGE_FAULTS] = {
        .name = "virt_domain_perf_page_faults",
        .type = METRIC_TYPE_COUNTER,
        .help = "Page_faults perf event which can be used to measure the count of page "
                "faults by applications running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_CONTEXT_SWITCHES] = {
        .name = "virt_domain_perf_context_switches",
        .type = METRIC_TYPE_COUNTER,
        .help = "Context_switches perf event which can be used to measure the count of context "
                "switches by applications running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_CPU_MIGRATIONS] = {
        .name = "virt_domain_perf_cpu_migrations",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cpu_migrations perf event which can be used to measure the count of cpu "
                "migrations by applications running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_PAGE_FAULTS_MIN] = {
        .name = "virt_domain_perf_page_faults_min",
        .type = METRIC_TYPE_COUNTER,
        .help = "Page_faults_min perf event which can be used to measure the count of minor page "
                "faults by applications running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_PAGE_FAULTS_MAJ] = {
        .name = "virt_domain_perf_page_faults_maj",
        .type = METRIC_TYPE_COUNTER,
        .help = "Page_faults_maj perf event which can be used to measure the count of major page "
                "faults by applications running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_ALIGNMENT_FAULTS] = {
        .name = "virt_domain_perf_alignment_faults",
        .type = METRIC_TYPE_COUNTER,
        .help = "Alignment_faults perf event which can be used to measure the count of alignment "
                "faults by applications running on the platform.",
    },
    [FAM_VIRT_DOMAIN_PERF_EMULATION_FAULTS] = {
        .name = "virt_domain_perf_emulation_faults",
        .type = METRIC_TYPE_COUNTER,
        .help = "Emulation_faults perf event which can be used to measure the count of emulation "
                "faults by applications running on the platform.",
    },


};

/* Actual list of block devices found on last refresh. */
struct block_device {
    virDomainPtr dom; /* domain */
    char *path;       /* name of block device */
    bool has_source;  /* information whether source is defined or not */
};

/* Actual list of network interfaces found on last refresh. */
struct interface_device {
    virDomainPtr dom; /* domain */
    char *path;       /* name of interface device */
    char *address;    /* mac address of interface device */
    char *number;     /* interface device number */
};

typedef struct domain_s {
    virDomainPtr ptr;
    virDomainInfo info;
    bool active;
} domain_t;

struct lv_read_state {
    /* Actual list of domains found on last refresh. */
    domain_t *domains;
    int nr_domains;

    struct block_device *block_devices;
    int nr_block_devices;

    struct interface_device *interface_devices;
    int nr_interface_devices;
};

/* structure used for aggregating notification-thread data*/
typedef struct virt_notif_thread_s {
    pthread_t event_loop_tid;
    int domain_event_cb_id;
    pthread_mutex_t active_mutex; /* protects 'is_active' member access*/
    bool is_active;
} virt_notif_thread_t;

enum {
    COLLECT_VIRT_DISK                 = 1 << 1,
    COLLECT_VIRT_PCPU                 = 1 << 2,
    COLLECT_VIRT_CPU_UTIL             = 1 << 3,
    COLLECT_VIRT_DOMAIN_STATE         = 1 << 4,
#ifdef HAVE_PERF_STATS
    COLLECT_VIRT_PERF                 = 1 << 5,
#endif
    COLLECT_VIRT_VCPUPIN              = 1 << 6,
#ifdef HAVE_DISK_ERR
    COLLECT_VIRT_DISK_ERR             = 1 << 7,
#endif
#ifdef HAVE_FS_INFO
    COLLECT_VIRT_FS_INFO              = 1 << 8,
#endif
    COLLECT_VIRT_DISK_ALLOCATION      = 1 << 11,
    COLLECT_VIRT_DISK_CAPACITY        = 1 << 12,
    COLLECT_VIRT_DISK_PHYSICAL        = 1 << 13,
    COLLECT_VIRT_MEMORY               = 1 << 14,
    COLLECT_VIRT_VCPU                 = 1 << 15,
};

static cf_flags_t virt_flags_list[] = {
    { "disk",                   COLLECT_VIRT_DISK                 },
    { "pcpu",                   COLLECT_VIRT_PCPU                 },
    { "cpu_util",               COLLECT_VIRT_CPU_UTIL             },
    { "domain_state",           COLLECT_VIRT_DOMAIN_STATE         },
#ifdef HAVE_PERF_STATS
    { "perf",                   COLLECT_VIRT_PERF                 },
#endif
    { "vcpupin",                COLLECT_VIRT_VCPUPIN              },
#ifdef HAVE_DISK_ERR
    { "disk_err",               COLLECT_VIRT_DISK_ERR             },
#endif
#ifdef HAVE_FS_INFO
    { "fs_info",                COLLECT_VIRT_FS_INFO              },
#endif
    { "disk_allocation",        COLLECT_VIRT_DISK_ALLOCATION      },
    { "disk_capacity",          COLLECT_VIRT_DISK_CAPACITY        },
    { "disk_physical",          COLLECT_VIRT_DISK_PHYSICAL        },
    { "memory",                 COLLECT_VIRT_MEMORY               },
    { "vcpu",                   COLLECT_VIRT_VCPU                 },
};
static size_t virt_flags_size = STATIC_ARRAY_SIZE(virt_flags_list);

struct lv_read_instance {
    struct lv_read_state read_state;
    size_t id;
};

enum bd_field { target, source };
enum if_field { if_address, if_name, if_number };

typedef struct {
    char *name;
    /* Connection. */
    virConnectPtr conn;
    char *conn_string;
    c_complain_t conn_complain;

    /* Node information required for %CPU */
    virNodeInfo nodeinfo;
    /* Seconds between list refreshes, 0 disables completely. */
    cdtime_t refresh_interval;
    /* Time that we last refreshed. */
    cdtime_t last_refresh;
    /* List of domains, if specified. */
    exclist_t excl_domains;
    /* List of block devices, if specified. */
    exclist_t excl_block_devices;
    /* List of network interface devices, if specified. */
    exclist_t excl_interface_devices;

    /* PersistentNotification is false by default */
    bool persistent_notification;
    //bool persistent_notification = false;

    /* Thread used for handling libvirt notifications events */
    virt_notif_thread_t notif_thread;
    /* BlockDeviceFormatBasename */
    enum bd_field blockdevice_format;
    //enum bd_field blockdevice_format = target;
    enum if_field interface_format;
    //enum if_field interface_format = if_name;

    struct lv_read_instance inst;

    uint64_t flags;
    label_set_t labels;
    metric_family_t fams[FAM_VIRT_MAX];
} virt_ctx_t;

static bool exclist_device_match(exclist_t *excl, const char *domname, const char *devpath);

const char *domain_states[] = {
    [VIR_DOMAIN_NOSTATE]     = "no state",
    [VIR_DOMAIN_RUNNING]     = "the domain is running",
    [VIR_DOMAIN_BLOCKED]     = "the domain is blocked on resource",
    [VIR_DOMAIN_PAUSED]      = "the domain is paused by user",
    [VIR_DOMAIN_SHUTDOWN]    = "the domain is being shut down",
    [VIR_DOMAIN_SHUTOFF]     = "the domain is shut off",
    [VIR_DOMAIN_CRASHED]     = "the domain is crashed",
#ifdef HAVE_DOM_STATE_PMSUSPENDED
    [VIR_DOMAIN_PMSUSPENDED] = "the domain is suspended by guest power management",
#endif
};

static int map_domain_event_to_state(int event)
{
    int ret;
    switch (event) {
    case VIR_DOMAIN_EVENT_STARTED:
        ret = VIR_DOMAIN_RUNNING;
        break;
    case VIR_DOMAIN_EVENT_SUSPENDED:
        ret = VIR_DOMAIN_PAUSED;
        break;
    case VIR_DOMAIN_EVENT_RESUMED:
        ret = VIR_DOMAIN_RUNNING;
        break;
    case VIR_DOMAIN_EVENT_STOPPED:
        ret = VIR_DOMAIN_SHUTOFF;
        break;
    case VIR_DOMAIN_EVENT_SHUTDOWN:
        ret = VIR_DOMAIN_SHUTDOWN;
        break;
#ifdef HAVE_DOM_STATE_PMSUSPENDED
    case VIR_DOMAIN_EVENT_PMSUSPENDED:
        ret = VIR_DOMAIN_PMSUSPENDED;
        break;
#endif
#ifdef HAVE_DOM_REASON_CRASHED
    case VIR_DOMAIN_EVENT_CRASHED:
        ret = VIR_DOMAIN_CRASHED;
        break;
#endif
    default:
        ret = VIR_DOMAIN_NOSTATE;
    }
    return ret;
}

#ifdef HAVE_DOM_REASON
static int map_domain_event_detail_to_reason(int event, int detail)
{
    int ret;
    switch (event) {
    case VIR_DOMAIN_EVENT_STARTED:
        switch (detail) {
        case VIR_DOMAIN_EVENT_STARTED_BOOTED: /* Normal startup from boot */
            ret = VIR_DOMAIN_RUNNING_BOOTED;
            break;
        case VIR_DOMAIN_EVENT_STARTED_MIGRATED: /* Incoming migration from another host */
            ret = VIR_DOMAIN_RUNNING_MIGRATED;
            break;
        case VIR_DOMAIN_EVENT_STARTED_RESTORED: /* Restored from a state file */
            ret = VIR_DOMAIN_RUNNING_RESTORED;
            break;
        case VIR_DOMAIN_EVENT_STARTED_FROM_SNAPSHOT: /* Restored from snapshot */
            ret = VIR_DOMAIN_RUNNING_FROM_SNAPSHOT;
            break;
#ifdef HAVE_DOM_REASON_RUNNING_WAKEUP
        case VIR_DOMAIN_EVENT_STARTED_WAKEUP: /* Started due to wakeup event */
            ret = VIR_DOMAIN_RUNNING_WAKEUP;
            break;
#endif
        default:
            ret = VIR_DOMAIN_RUNNING_UNKNOWN;
        }
        break;
    case VIR_DOMAIN_EVENT_SUSPENDED:
        switch (detail) {
        case VIR_DOMAIN_EVENT_SUSPENDED_PAUSED: /* Normal suspend due to admin pause */
            ret = VIR_DOMAIN_PAUSED_USER;
            break;
        case VIR_DOMAIN_EVENT_SUSPENDED_MIGRATED: /* Suspended for offline migration */
            ret = VIR_DOMAIN_PAUSED_MIGRATION;
            break;
        case VIR_DOMAIN_EVENT_SUSPENDED_IOERROR: /* Suspended due to a disk I/O error */
            ret = VIR_DOMAIN_PAUSED_IOERROR;
            break;
        case VIR_DOMAIN_EVENT_SUSPENDED_WATCHDOG: /* Suspended due to a watchdog firing */
            ret = VIR_DOMAIN_PAUSED_WATCHDOG;
            break;
        case VIR_DOMAIN_EVENT_SUSPENDED_RESTORED: /* Restored from paused state file */
            ret = VIR_DOMAIN_PAUSED_UNKNOWN;
            break;
        case VIR_DOMAIN_EVENT_SUSPENDED_FROM_SNAPSHOT: /* Restored from paused snapshot */
            ret = VIR_DOMAIN_PAUSED_FROM_SNAPSHOT;
            break;
        case VIR_DOMAIN_EVENT_SUSPENDED_API_ERROR: /* Suspended after failure during
                                                      libvirt API call */
            ret = VIR_DOMAIN_PAUSED_UNKNOWN;
            break;
#ifdef HAVE_DOM_REASON_POSTCOPY
        case VIR_DOMAIN_EVENT_SUSPENDED_POSTCOPY: /* Suspended for post-copy migration */
            ret = VIR_DOMAIN_PAUSED_POSTCOPY;
            break;
        case VIR_DOMAIN_EVENT_SUSPENDED_POSTCOPY_FAILED: /* Suspended after failed post-copy */
            ret = VIR_DOMAIN_PAUSED_POSTCOPY_FAILED;
            break;
#endif
        default:
            ret = VIR_DOMAIN_PAUSED_UNKNOWN;
        }
        break;
    case VIR_DOMAIN_EVENT_RESUMED:
        switch (detail) {
        case VIR_DOMAIN_EVENT_RESUMED_UNPAUSED: /* Normal resume due to admin unpause */
            ret = VIR_DOMAIN_RUNNING_UNPAUSED;
            break;
        case VIR_DOMAIN_EVENT_RESUMED_MIGRATED: /* Resumed for completion of migration */
            ret = VIR_DOMAIN_RUNNING_MIGRATED;
            break;
        case VIR_DOMAIN_EVENT_RESUMED_FROM_SNAPSHOT: /* Resumed from snapshot */
            ret = VIR_DOMAIN_RUNNING_FROM_SNAPSHOT;
            break;
#ifdef HAVE_DOM_REASON_POSTCOPY
        case VIR_DOMAIN_EVENT_RESUMED_POSTCOPY: /* Resumed, but migration is still
                                                   running in post-copy mode */
            ret = VIR_DOMAIN_RUNNING_POSTCOPY;
            break;
#endif
        default:
            ret = VIR_DOMAIN_RUNNING_UNKNOWN;
        }
        break;
    case VIR_DOMAIN_EVENT_STOPPED:
        switch (detail) {
        case VIR_DOMAIN_EVENT_STOPPED_SHUTDOWN: /* Normal shutdown */
            ret = VIR_DOMAIN_SHUTOFF_SHUTDOWN;
            break;
        case VIR_DOMAIN_EVENT_STOPPED_DESTROYED: /* Forced poweroff from host */
            ret = VIR_DOMAIN_SHUTOFF_DESTROYED;
            break;
        case VIR_DOMAIN_EVENT_STOPPED_CRASHED: /* Guest crashed */
            ret = VIR_DOMAIN_SHUTOFF_CRASHED;
            break;
        case VIR_DOMAIN_EVENT_STOPPED_MIGRATED: /* Migrated off to another host */
            ret = VIR_DOMAIN_SHUTOFF_MIGRATED;
            break;
        case VIR_DOMAIN_EVENT_STOPPED_SAVED: /* Saved to a state file */
            ret = VIR_DOMAIN_SHUTOFF_SAVED;
            break;
        case VIR_DOMAIN_EVENT_STOPPED_FAILED: /* Host emulator/mgmt failed */
            ret = VIR_DOMAIN_SHUTOFF_FAILED;
            break;
        case VIR_DOMAIN_EVENT_STOPPED_FROM_SNAPSHOT: /* Offline snapshot loaded */
            ret = VIR_DOMAIN_SHUTOFF_FROM_SNAPSHOT;
            break;
        default:
            ret = VIR_DOMAIN_SHUTOFF_UNKNOWN;
        }
        break;
    case VIR_DOMAIN_EVENT_SHUTDOWN:
        switch (detail) {
        case VIR_DOMAIN_EVENT_SHUTDOWN_FINISHED: /* Guest finished shutdown sequence */
#ifdef LIBVIR_CHECK_VERSION
#if LIBVIR_CHECK_VERSION(3, 4, 0)
        case VIR_DOMAIN_EVENT_SHUTDOWN_GUEST: /* Domain finished shutting down after
                                                 request from the guest itself (e.g.
                                                 hardware-specific action) */
        case VIR_DOMAIN_EVENT_SHUTDOWN_HOST:    /* Domain finished shutting down after
                                                   request from the host (e.g. killed
                                                   by a signal) */
#endif
#endif
            ret = VIR_DOMAIN_SHUTDOWN_USER;
            break;
        default:
            ret = VIR_DOMAIN_SHUTDOWN_UNKNOWN;
        }
        break;
#ifdef HAVE_DOM_STATE_PMSUSPENDED
    case VIR_DOMAIN_EVENT_PMSUSPENDED:
        switch (detail) {
        case VIR_DOMAIN_EVENT_PMSUSPENDED_MEMORY: /* Guest was PM suspended to memory */
            ret = VIR_DOMAIN_PMSUSPENDED_UNKNOWN;
            break;
        case VIR_DOMAIN_EVENT_PMSUSPENDED_DISK: /* Guest was PM suspended to disk */
            ret = VIR_DOMAIN_PMSUSPENDED_DISK_UNKNOWN;
            break;
        default:
            ret = VIR_DOMAIN_PMSUSPENDED_UNKNOWN;
        }
        break;
#endif
    case VIR_DOMAIN_EVENT_CRASHED:
        switch (detail) {
        case VIR_DOMAIN_EVENT_CRASHED_PANICKED: /* Guest was panicked */
            ret = VIR_DOMAIN_CRASHED_PANICKED;
            break;
        default:
            ret = VIR_DOMAIN_CRASHED_UNKNOWN;
        }
        break;
    default:
        ret = VIR_DOMAIN_NOSTATE_UNKNOWN;
    }
    return ret;
}

#define DOMAIN_STATE_REASON_MAX_SIZE 20
const char *domain_reasons[][DOMAIN_STATE_REASON_MAX_SIZE] = {
    [VIR_DOMAIN_NOSTATE] = {
        [VIR_DOMAIN_NOSTATE_UNKNOWN]            = "the reason is unknown",
    },
    [VIR_DOMAIN_RUNNING] = {
        [VIR_DOMAIN_RUNNING_UNKNOWN]            = "the reason is unknown",
        [VIR_DOMAIN_RUNNING_BOOTED]             = "normal startup from boot",
        [VIR_DOMAIN_RUNNING_MIGRATED]           = "migrated from another host",
        [VIR_DOMAIN_RUNNING_RESTORED]           = "restored from a state file",
        [VIR_DOMAIN_RUNNING_FROM_SNAPSHOT]      = "restored from snapshot",
        [VIR_DOMAIN_RUNNING_UNPAUSED]           = "returned from paused state",
        [VIR_DOMAIN_RUNNING_MIGRATION_CANCELED] = "returned from migration",
        [VIR_DOMAIN_RUNNING_SAVE_CANCELED]      = "returned from failed save process",
#ifdef HAVE_DOM_REASON_RUNNING_WAKEUP
        [VIR_DOMAIN_RUNNING_WAKEUP]             = "returned from pmsuspended due to wakeup event",
#endif
#ifdef HAVE_DOM_REASON_CRASHED
        [VIR_DOMAIN_RUNNING_CRASHED]            = "resumed from crashed",
#endif
#ifdef HAVE_DOM_REASON_POSTCOPY
        [VIR_DOMAIN_RUNNING_POSTCOPY]           = "running in post-copy migration mode",
#endif
    },
    [VIR_DOMAIN_BLOCKED] = {
        [VIR_DOMAIN_BLOCKED_UNKNOWN]            = "the reason is unknown",
    },
    [VIR_DOMAIN_PAUSED] = {
        [VIR_DOMAIN_PAUSED_UNKNOWN]             = "the reason is unknown",
        [VIR_DOMAIN_PAUSED_USER]                = "paused on user request",
        [VIR_DOMAIN_PAUSED_MIGRATION]           = "paused for offline migration",
        [VIR_DOMAIN_PAUSED_SAVE]                = "paused for save",
        [VIR_DOMAIN_PAUSED_DUMP]                = "paused for offline core dump",
        [VIR_DOMAIN_PAUSED_IOERROR]             = "paused due to a disk I/O error",
        [VIR_DOMAIN_PAUSED_WATCHDOG]            = "paused due to a watchdog event",
        [VIR_DOMAIN_PAUSED_FROM_SNAPSHOT]       = "paused after restoring from snapshot",
#ifdef HAVE_DOM_REASON_PAUSED_SHUTTING_DOWN
        [VIR_DOMAIN_PAUSED_SHUTTING_DOWN]       = "paused during shutdown process",
#endif
#ifdef HAVE_DOM_REASON_PAUSED_SNAPSHOT
        [VIR_DOMAIN_PAUSED_SNAPSHOT]            = "paused while creating a snapshot",
#endif
#ifdef HAVE_DOM_REASON_PAUSED_CRASHED
        [VIR_DOMAIN_PAUSED_CRASHED]             = "paused due to a guest crash",
#endif
#ifdef HAVE_DOM_REASON_PAUSED_STARTING_UP
        [VIR_DOMAIN_PAUSED_STARTING_UP]         = "the domain is being started",
#endif
#ifdef HAVE_DOM_REASON_POSTCOPY
        [VIR_DOMAIN_PAUSED_POSTCOPY]            = "paused for post-copy migration",
        [VIR_DOMAIN_PAUSED_POSTCOPY_FAILED]     = "paused after failed post-copy",
#endif
    },
    [VIR_DOMAIN_SHUTDOWN] = {
        [VIR_DOMAIN_SHUTDOWN_UNKNOWN]           = "the reason is unknown",
        [VIR_DOMAIN_SHUTDOWN_USER]              = "shutting down on user request",
    },
    [VIR_DOMAIN_SHUTOFF] = {
        [VIR_DOMAIN_SHUTOFF_UNKNOWN]            = "the reason is unknown",
        [VIR_DOMAIN_SHUTOFF_SHUTDOWN]           = "normal shutdown",
        [VIR_DOMAIN_SHUTOFF_DESTROYED]          = "forced poweroff",
        [VIR_DOMAIN_SHUTOFF_CRASHED]            = "domain crashed",
        [VIR_DOMAIN_SHUTOFF_MIGRATED]           = "migrated to another host",
        [VIR_DOMAIN_SHUTOFF_SAVED]              = "saved to a file",
        [VIR_DOMAIN_SHUTOFF_FAILED]             = "domain failed to start",
        [VIR_DOMAIN_SHUTOFF_FROM_SNAPSHOT]      = "restored from a snapshot which was taken "
                                                  "while domain was shutoff",
#ifdef HAVE_DOM_REASON_SHUTOFF_DAEMON
        [VIR_DOMAIN_SHUTOFF_DAEMON]             = "daemon decides to kill domain during "
                                                  "reconnection processing",
#endif
    },
    [VIR_DOMAIN_CRASHED] = {
        [VIR_DOMAIN_CRASHED_UNKNOWN]            = "the reason is unknown",
#ifdef VIR_DOMAIN_CRASHED_PANICKED
        [VIR_DOMAIN_CRASHED_PANICKED]           = "domain panicked",
#endif
    },
#ifdef HAVE_DOM_STATE_PMSUSPENDED
    [VIR_DOMAIN_PMSUSPENDED] = {
        [VIR_DOMAIN_PMSUSPENDED_UNKNOWN]        = "the reason is unknown",
    },
#endif
};
#endif

static void free_domains(struct lv_read_state *state);
static int add_domain(struct lv_read_state *state, virDomainPtr dom, bool active);

static void free_block_devices(struct lv_read_state *state);
static int add_block_device(struct lv_read_state *state, virDomainPtr dom,
                            const char *path, bool has_source);

static void free_interface_devices(struct lv_read_state *state);
static int add_interface_device(struct lv_read_state *state, virDomainPtr dom,
                                const char *path, const char *address,
                                unsigned int number);

#define METADATA_VM_PARTITION_URI "http://ovirt.org/ovirtmap/tag/1.0"
#define METADATA_VM_PARTITION_ELEMENT "tag"
#define METADATA_VM_PARTITION_PREFIX "ovirtmap"

#define BUFFER_MAX_LEN 256

static int refresh_lists(virt_ctx_t *ctx, struct lv_read_instance *inst);
static int register_event_impl(void);
static int start_event_loop(virt_ctx_t *ctx, virt_notif_thread_t *thread_data);

struct lv_block_stats {
    virDomainBlockStatsStruct bi;

    long long rd_total_times;
    long long wr_total_times;

    long long fl_req;
    long long fl_total_times;
};

static void init_block_stats(struct lv_block_stats *bstats)
{
    if (bstats == NULL)
        return;

    bstats->bi.rd_req = -1;
    bstats->bi.wr_req = -1;
    bstats->bi.rd_bytes = -1;
    bstats->bi.wr_bytes = -1;

    bstats->rd_total_times = -1;
    bstats->wr_total_times = -1;
    bstats->fl_req = -1;
    bstats->fl_total_times = -1;
}

static void init_block_info(virDomainBlockInfoPtr binfo)
{
    binfo->allocation = -1;
    binfo->capacity = -1;
    binfo->physical = -1;
}

#ifdef HAVE_BLOCK_STATS_FLAGS

#define GET_BLOCK_STATS_VALUE(NAME, FIELD)     \
    if (!strcmp(param[i].field, NAME)) {       \
        bstats->FIELD = param[i].value.l;      \
        continue;                              \
    }

static int get_block_stats(struct lv_block_stats *bstats, virTypedParameterPtr param, int nparams)
{
    if (bstats == NULL || param == NULL)
        return -1;

    for (int i = 0; i < nparams; ++i) {
        /* ignore type. Everything must be LLONG anyway. */
        GET_BLOCK_STATS_VALUE("rd_operations", bi.rd_req);
        GET_BLOCK_STATS_VALUE("wr_operations", bi.wr_req);
        GET_BLOCK_STATS_VALUE("rd_bytes", bi.rd_bytes);
        GET_BLOCK_STATS_VALUE("wr_bytes", bi.wr_bytes);
        GET_BLOCK_STATS_VALUE("rd_total_times", rd_total_times);
        GET_BLOCK_STATS_VALUE("wr_total_times", wr_total_times);
        GET_BLOCK_STATS_VALUE("flush_operations", fl_req);
        GET_BLOCK_STATS_VALUE("flush_total_times", fl_total_times);
    }

    return 0;
}

#undef GET_BLOCK_STATS_VALUE

#endif

/* ERROR(...) macro for virterrors. */
#define VIRT_ERROR(conn, s)                                              \
    do {                                                                 \
        virErrorPtr err;                                                 \
        err = (conn) ? virConnGetLastError((conn)) : virGetLastError();  \
        if (err)                                                         \
            PLUGIN_ERROR("%s failed: %s", (s), err->message);            \
    } while (0)

static void submit_notif(char *name, virDomainPtr domain, int severity, const char *msg)
{
    notification_t n = {
        .severity = severity,
        .time = cdtime(),
        .name = name,
    };

    const char *ndomain = virDomainGetName(domain);
    notification_label_set(&n, "domain", ndomain);

    char uuid[VIR_UUID_STRING_BUFLEN] = {0};
    virDomainGetUUIDString(domain, uuid);
    notification_label_set(&n, "uuid", uuid);

    notification_annotation_set(&n, "summary", msg);

    plugin_dispatch_notification(&n);
}

static void domain_state_submit_notif(virDomainPtr dom, int state, int reason)
{
    if ((state < 0) || (state >= (int)STATIC_ARRAY_SIZE(domain_states))) {
        PLUGIN_ERROR("Array index out of bounds: state=%d", state);
        return;
    }

    char msg[DATA_MAX_NAME_LEN];
    const char *state_str = domain_states[state];
#ifdef HAVE_DOM_REASON
    if ((reason < 0) || (reason >= (int)STATIC_ARRAY_SIZE(domain_reasons[0]))) {
        PLUGIN_ERROR("Array index out of bounds: reason=%d", reason);
        return;
    }

    const char *reason_str = domain_reasons[state][reason];
    /* Array size for domain reasons is fixed, but different domain states can
     * have different number of reasons. We need to check if reason was
     * successfully parsed */
    if (!reason_str) {
        PLUGIN_ERROR("Invalid reason (%d) for domain state: %s", reason, state_str);
        return;
    }
#else
    const char *reason_str = "N/A";
#endif

    ssnprintf(msg, sizeof(msg), "Domain state: %s. Reason: %s", state_str, reason_str);

    int severity;
    switch (state) {
    case VIR_DOMAIN_NOSTATE:
    case VIR_DOMAIN_RUNNING:
    case VIR_DOMAIN_SHUTDOWN:
    case VIR_DOMAIN_SHUTOFF:
        severity = NOTIF_OKAY;
        break;
    case VIR_DOMAIN_BLOCKED:
    case VIR_DOMAIN_PAUSED:
#ifdef DOM_STATE_PMSUSPENDED
    case VIR_DOMAIN_PMSUSPENDED:
#endif
        severity = NOTIF_WARNING;
        break;
    case VIR_DOMAIN_CRASHED:
        severity = NOTIF_FAILURE;
        break;
    default:
        PLUGIN_ERROR("Unrecognized domain state (%d)", state);
        return;
    }

    submit_notif("virt_domain_state", dom, severity, msg);
}

static int lv_connect(virt_ctx_t *ctx)
{
    if (ctx->conn == NULL) {
        /* event implementation must be registered before connection is opened */
        if (!ctx->persistent_notification) {
            if (register_event_impl() != 0)
                return -1;
        }
/* `conn_string == NULL' is acceptable */
#ifdef HAVE_FS_INFO
        /* virDomainGetFSInfo requires full read-write access connection */
        if (ctx->flags & COLLECT_VIRT_FS_INFO) {
            ctx->conn = virConnectOpen(ctx->conn_string);
        } else {
            ctx->conn = virConnectOpenReadOnly(ctx->conn_string);
        }
#else
        ctx->conn = virConnectOpenReadOnly(ctx->conn_string);
#endif
        if (ctx->conn == NULL) {
            c_complain(LOG_ERR, &ctx->conn_complain, "Unable to connect: virConnectOpen failed.");
            return -1;
        }
        int status = virNodeGetInfo(ctx->conn, &ctx->nodeinfo);
        if (status != 0) {
            PLUGIN_ERROR("virNodeGetInfo failed");
            virConnectClose(ctx->conn);
            ctx->conn = NULL;
            return -1;
        }

        if (!ctx->persistent_notification) {
            if (start_event_loop(ctx, &ctx->notif_thread) != 0) {
                virConnectClose(ctx->conn);
                ctx->conn = NULL;
                return -1;
            }
        }
    }
    c_release(LOG_NOTICE, &ctx->conn_complain, "Connection established.");
    return 0;
}

static void lv_disconnect(virt_ctx_t *ctx)
{
    if (ctx->conn != NULL)
        virConnectClose(ctx->conn);
    ctx->conn = NULL;
    PLUGIN_WARNING("closed connection to libvirt");
}

static int lv_domain_block_stats(virt_ctx_t *ctx, virDomainPtr dom,
                                 const char *path, struct lv_block_stats *bstats)
{
#ifdef HAVE_BLOCK_STATS_FLAGS
    int nparams = 0;
    if (virDomainBlockStatsFlags(dom, path, NULL, &nparams, 0) < 0 ||
            nparams <= 0) {
        VIRT_ERROR(ctx->conn, "getting the disk params count");
        return -1;
    }

    virTypedParameterPtr params = calloc(nparams, sizeof(*params));
    if (params == NULL) {
        PLUGIN_ERROR("alloc(%i) for block=%s parameters failed.", nparams, path);
        return -1;
    }

    int rc = -1;
    if (virDomainBlockStatsFlags(dom, path, params, &nparams, 0) < 0) {
        VIRT_ERROR(ctx->conn, "getting the disk params values");
    } else {
        rc = get_block_stats(bstats, params, nparams);
    }

    virTypedParamsClear(params, nparams);
    free(params);
    return rc;
#else
    return virDomainBlockStats(dom, path, &(bstats->bi), sizeof(bstats->bi));
#endif
}

#ifdef HAVE_PERF_STATS

static int get_perf_events(virt_ctx_t *ctx, virDomainPtr domain,
                           const char *ndomain, const char *uuid)
{
    virDomainStatsRecordPtr *stats = NULL;
    /* virDomainListGetStats requires a NULL terminated list of domains */
    virDomainPtr domain_array[] = {domain, NULL};

    int status = virDomainListGetStats(domain_array, VIR_DOMAIN_STATS_PERF, &stats, 0);
    if (status == -1) {
        PLUGIN_ERROR("virDomainListGetStats failed with status %i.", status);

        virErrorPtr err = virGetLastError();
        if (err->code == VIR_ERR_NO_SUPPORT) {
            PLUGIN_ERROR("Disabled unsupported selector: perf");
            ctx->flags &= ~(COLLECT_VIRT_PERF);
        }

        return -1;
    }

    for (int i = 0; i < status; i++) {
        virDomainStatsRecordPtr perf = stats[i];

        for (int j = 0; j < perf->nparams; j++) {
            size_t len = strlen(perf->params[j].field);
            int fam = -1;

            switch (len) {
            case 3:
                if (strncmp(perf->params[j].field, "cmt", 3) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_CMT;
                break;
            case 4:
                if (strncmp(perf->params[j].field, "mbmt", 4) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_MBMT;
                else if (strncmp(perf->params[j].field, "mbml", 4) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_MBML;
                break;
            case 9:
                if (strncmp(perf->params[j].field, "cpu_clock", 9) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_CPU_CLOCK;
                break;
            case 10:
                if (strncmp(perf->params[j].field, "bus_cycles", 10) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_BUS_CYCLES;
                else if (strncmp(perf->params[j].field, "task_clock", 10) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_TASK_CLOCK;
                else if (strncmp(perf->params[j].field, "cpu_cycles", 10) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_CPU_CYCLES;
                break;
            case 11:
                if (strncmp(perf->params[j].field, "page_faults", 11) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_PAGE_FAULTS;
                break;
            case 12:
                if (strncmp(perf->params[j].field, "cache_misses", 12) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_CACHE_MISSES;
                else if (strncmp(perf->params[j].field, "instructions", 12) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_INSTRUCTIONS;
                break;
            case 13:
                if (strncmp(perf->params[j].field, "branch_misses", 13) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_BRANCH_MISSES;
                break;
            case 14:
                if (strncmp(perf->params[j].field, "cpu_migrations", 14) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_CPU_MIGRATIONS;
                else if (strncmp(perf->params[j].field, "ref_cpu_cycles", 14) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_REF_CPU_CYCLES;
                break;
            case 15:
                if (strncmp(perf->params[j].field, "page_faults_min", 15) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_PAGE_FAULTS_MIN;
                else if (strncmp(perf->params[j].field, "page_faults_maj", 15) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_PAGE_FAULTS_MAJ;
                break;
            case 16:
                if (strncmp(perf->params[j].field, "cache_references", 16) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_CACHE_REFERENCES;
                else if (strncmp(perf->params[j].field, "context_switches", 16) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_CONTEXT_SWITCHES;
                else if (strncmp(perf->params[j].field, "alignment_faults", 16) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_ALIGNMENT_FAULTS;
                else if (strncmp(perf->params[j].field, "emulation_faults", 16) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_EMULATION_FAULTS;
                break;
            case 19:
                if (strncmp(perf->params[j].field, "branch_instructions", 19) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_BRANCH_INSTRUCTIONS;
                break;
            case 22:
                if (strncmp(perf->params[j].field, "stalled_cycles_backend", 22) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_STALLED_CYCLES_BACKEND;
                break;
            case 23:
                if (strncmp(perf->params[j].field, "stalled_cycles_frontend", 23) == 0)
                    fam = FAM_VIRT_DOMAIN_PERF_STALLED_CYCLES_FRONTEND;
                break;
            }

            if ((fam > 0) && (ctx->fams[fam].type == METRIC_TYPE_COUNTER)) {
                metric_family_append(&ctx->fams[fam], VALUE_COUNTER(perf->params[j].value.ul),
                                     &ctx->labels,
                                     &(label_pair_const_t){.name="domain", .value=ndomain },
                                     &(label_pair_const_t){.name="uuid", .value=uuid},
                                     NULL);

            }
        }
    }

    virDomainStatsRecordListFree(stats);
    return 0;
}
#endif

static int get_vcpu_stats(virt_ctx_t *ctx, virDomainPtr domain, unsigned short nr_virt_cpu,
                          const char *ndomain, const char *uuid)
{
    int max_cpus = VIR_NODEINFO_MAXCPUS(ctx->nodeinfo);

    virVcpuInfoPtr vinfo = calloc(nr_virt_cpu, sizeof(*vinfo));
    if (vinfo == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int cpu_map_len = 0;
    unsigned char *cpumaps = NULL;
    if (ctx->flags & COLLECT_VIRT_VCPUPIN) {
        cpu_map_len = VIR_CPU_MAPLEN(max_cpus);
        cpumaps = calloc(nr_virt_cpu, cpu_map_len);

        if (cpumaps == NULL) {
            PLUGIN_ERROR("calloc failed.");
            free(vinfo);
            return -1;
        }
    }

    int status = virDomainGetVcpus(domain, vinfo, nr_virt_cpu, cpumaps, cpu_map_len);
    if (status < 0) {
        PLUGIN_ERROR("virDomainGetVcpus failed with status %i.", status);

        virErrorPtr err = virGetLastError();
        if (err->code == VIR_ERR_NO_SUPPORT) {
            if (ctx->flags & COLLECT_VIRT_VCPU)
                PLUGIN_ERROR("Disabled unsupported selector: vcpu");
            if (ctx->flags & COLLECT_VIRT_VCPUPIN)
                PLUGIN_ERROR("Disabled unsupported selector: vcpupin");
            ctx->flags &= ~(COLLECT_VIRT_VCPU | COLLECT_VIRT_VCPUPIN);
        }

        free(cpumaps);
        free(vinfo);
        return -1;
    }

    for (int i = 0; i < nr_virt_cpu; i++) {
        if (ctx->flags & COLLECT_VIRT_VCPU) {
            char cpu[ITOA_MAX];
            itoa(vinfo[i].number, cpu);
            metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_VCPU_TIME_SECONDS],
                                 VALUE_COUNTER_FLOAT64((double)vinfo[i].cpuTime/(double)1e9),
                                 &ctx->labels,
                                 &(label_pair_const_t){.name="domain", .value=ndomain },
                                 &(label_pair_const_t){.name="uuid", .value=uuid},
                                 &(label_pair_const_t){.name="cpu", .value=cpu},
                                 NULL);
        }
        if (ctx->flags & COLLECT_VIRT_VCPUPIN) {
            char nvcpu[ITOA_MAX];
            itoa(i, nvcpu);
            for (int cpu = 0; cpu < max_cpus; cpu++) {
                char ncpu[ITOA_MAX];
                itoa(cpu, ncpu);
                bool is_set = VIR_CPU_USABLE(cpumaps, cpu_map_len, i, cpu);

                metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_CPU_AFFINITY],
                                     VALUE_GAUGE(is_set), &ctx->labels,
                                     &(label_pair_const_t){.name="domain", .value=ndomain },
                                     &(label_pair_const_t){.name="uuid", .value=uuid},
                                     &(label_pair_const_t){.name="cpu", .value=ncpu},
                                     &(label_pair_const_t){.name="vcpu", .value=nvcpu},
                                     NULL);
            }
        }
    }

    free(cpumaps);
    free(vinfo);
    return 0;
}

#ifdef HAVE_CPU_STATS
static int get_pcpu_stats(virt_ctx_t *ctx, virDomainPtr dom, const char *ndomain, const char *uuid)
{
    int nparams = virDomainGetCPUStats(dom, NULL, 0, -1, 1, 0);
    if (nparams < 0) {
        VIRT_ERROR(ctx->conn, "getting the CPU params count");

        virErrorPtr err = virGetLastError();
        if (err->code == VIR_ERR_NO_SUPPORT) {
            PLUGIN_ERROR("Disabled unsupported selector: pcpu");
            ctx->flags &= ~(COLLECT_VIRT_PCPU);
        }

        return -1;
    }

    virTypedParameterPtr param = calloc(nparams, sizeof(*param));
    if (param == NULL) {
        PLUGIN_ERROR("alloc(%i) for cpu parameters failed.", nparams);
        return -1;
    }

    int ret = virDomainGetCPUStats(dom, param, nparams, -1, 1, 0); // total stats.
    if (ret < 0) {
        virTypedParamsClear(param, nparams);
        free(param);
        VIRT_ERROR(ctx->conn, "getting the CPU params values");
        return -1;
    }

    unsigned long long total_user_cpu_time = 0;
    unsigned long long total_syst_cpu_time = 0;

    for (int i = 0; i < nparams; ++i) {
        if (!strcmp(param[i].field, "user_time"))
            total_user_cpu_time = param[i].value.ul;
        else if (!strcmp(param[i].field, "system_time"))
            total_syst_cpu_time = param[i].value.ul;
    }

    if (total_user_cpu_time > 0 || total_syst_cpu_time > 0) {
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_VCPU_ALL_SYSTEM_TIME_SECONDS],
                            VALUE_COUNTER_FLOAT64((double)total_syst_cpu_time / (double)1e9),
                            &ctx->labels,
                            &(label_pair_const_t){.name="domain", .value=ndomain },
                            &(label_pair_const_t){.name="uuid", .value=uuid},
                            NULL);
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_VCPU_ALL_USER_TIME_SECONDS],
                            VALUE_COUNTER_FLOAT64((double)total_user_cpu_time / (double)1e9),
                            &ctx->labels,
                            &(label_pair_const_t){.name="domain", .value=ndomain },
                            &(label_pair_const_t){.name="uuid", .value=uuid},
                            NULL);

    }

    virTypedParamsClear(param, nparams);
    free(param);

    return 0;
}
#endif

#ifdef HAVE_DOM_REASON
static int submit_domain_state(virt_ctx_t *ctx, virDomainPtr domain,
                               const char *ndomain, const char *uuid)
{
    int domain_state = 0;
    int domain_reason = 0;

    int status = virDomainGetState(domain, &domain_state, &domain_reason, 0);
    if (status != 0) {
        PLUGIN_ERROR("virDomainGetState failed with status %i.", status);
        return status;
    }

    state_t states[] = {
        { .name = "NOSTATE",     .enabled = false },
        { .name = "RUNNING",     .enabled = false },
        { .name = "BLOCKED",     .enabled = false },
        { .name = "PAUSED",      .enabled = false },
        { .name = "SHUTDOWN",    .enabled = false },
        { .name = "SHUTOFF",     .enabled = false },
        { .name = "CRASHED",     .enabled = false },
        { .name = "PMSUSPENDED", .enabled = false },
    };
    state_set_t set_states = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };

    switch(domain_state) {
    case VIR_DOMAIN_NOSTATE:
        set_states.ptr[0].enabled = true;
        break;
    case VIR_DOMAIN_RUNNING:
        set_states.ptr[1].enabled = true;
        break;
    case VIR_DOMAIN_BLOCKED:
        set_states.ptr[2].enabled = true;
        break;
    case VIR_DOMAIN_PAUSED:
        set_states.ptr[3].enabled = true;
        break;
    case VIR_DOMAIN_SHUTDOWN:
        set_states.ptr[4].enabled = true;
        break;
    case VIR_DOMAIN_SHUTOFF:
        set_states.ptr[5].enabled = true;
        break;
    case VIR_DOMAIN_CRASHED:
        set_states.ptr[6].enabled = true;
        break;
    case VIR_DOMAIN_PMSUSPENDED:
        set_states.ptr[7].enabled = true;
        break;
    default:
        set_states.ptr[0].enabled = true;
        break;
    }

   metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_STATE],
                         VALUE_STATE_SET(set_states), &ctx->labels,
                         &(label_pair_const_t){.name="domain", .value=ndomain },
                         &(label_pair_const_t){.name="uuid", .value=uuid},
                         NULL);

   metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_STATE_NUMBER],
                         VALUE_GAUGE(domain_state), &ctx->labels,
                         &(label_pair_const_t){.name="domain", .value=ndomain },
                         &(label_pair_const_t){.name="uuid", .value=uuid},
                         NULL);

   metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_REASON_NUMBER],
                         VALUE_GAUGE(domain_reason), &ctx->labels,
                         &(label_pair_const_t){.name="domain", .value=ndomain },
                         &(label_pair_const_t){.name="uuid", .value=uuid},
                         NULL);
    return 0;
}

#ifdef HAVE_LIST_ALL_DOMAINS
static int get_domain_state_notify(virDomainPtr domain)
{
    int domain_state = 0;
    int domain_reason = 0;

    int status = virDomainGetState(domain, &domain_state, &domain_reason, 0);
    if (status != 0) {
        PLUGIN_ERROR("virDomainGetState failed with status %i.", status);
        return status;
    }

    domain_state_submit_notif(domain, domain_state, domain_reason);

    return status;
}
#endif
#endif

static int get_memory_stats(virt_ctx_t *ctx, virDomainPtr domain,
                            const char *ndomain, const char *uuid)
{
    virDomainMemoryStatPtr minfo = calloc(VIR_DOMAIN_MEMORY_STAT_NR, sizeof(*minfo));
    if (minfo == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int mem_stats = virDomainMemoryStats(domain, minfo, VIR_DOMAIN_MEMORY_STAT_NR, 0);
    if (mem_stats < 0) {
        PLUGIN_ERROR("virDomainMemoryStats failed with mem_stats %i.", mem_stats);
        free(minfo);

        virErrorPtr err = virGetLastError();
        if (err->code == VIR_ERR_NO_SUPPORT) {
            PLUGIN_ERROR("Disabled unsupported selector: memory");
            ctx->flags &= ~(COLLECT_VIRT_MEMORY);
        }

        return -1;
    }

    for (int i = 0; i < mem_stats; i++) {
        int fam = -1;
        uint64_t val = minfo[i].val;

        switch (minfo[i].tag) {
        case VIR_DOMAIN_MEMORY_STAT_SWAP_IN:
            fam = FAM_VIRT_DOMAIN_SWAP_IN_BYTES;
            break;
        case VIR_DOMAIN_MEMORY_STAT_SWAP_OUT:
            fam = FAM_VIRT_DOMAIN_SWAP_OUT_BYTES;
            break;
        case VIR_DOMAIN_MEMORY_STAT_MAJOR_FAULT:
            fam = FAM_VIRT_DOMAIN_MEMORY_MAJOR_PAGE_FAULT;
            break;
        case VIR_DOMAIN_MEMORY_STAT_MINOR_FAULT:
            fam = FAM_VIRT_DOMAIN_MEMORY_MINOR_PAGE_FAULT;
            break;
        case VIR_DOMAIN_MEMORY_STAT_UNUSED:
            fam = FAM_VIRT_DOMAIN_MEMORY_UNUSED_BYTES;
            val *= 1024;
            break;
        case VIR_DOMAIN_MEMORY_STAT_AVAILABLE:
            fam = FAM_VIRT_DOMAIN_MEMORY_AVAILABLE_BYTES;
            val *= 1024;
            break;
        case VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON:
            fam = FAM_VIRT_DOMAIN_MEMORY_BALLOON_BYTES;
            val *= 1024;
            break;
        case VIR_DOMAIN_MEMORY_STAT_RSS:
            fam = FAM_VIRT_DOMAIN_MEMORY_RSS_BYTES;
            val *= 1024;
            break;
        case VIR_DOMAIN_MEMORY_STAT_USABLE:
            fam = FAM_VIRT_DOMAIN_MEMORY_USABLE_BYTES;
            val *= 1024;
            break;
        case VIR_DOMAIN_MEMORY_STAT_DISK_CACHES:
            fam = FAM_VIRT_DOMAIN_MEMORY_DISK_CACHE_BYTES;
            val *= 1024;
            break;
#ifdef HAVE_VIR_DOMAIN_MEMORY_STAT_HUGETLB_PGALLOC
        case VIR_DOMAIN_MEMORY_STAT_HUGETLB_PGALLOC:
            fam = FAM_VIRT_DOMAIN_MEMORY_HUGETLB_PAGE_ALLOC;
            break;
#endif
#ifdef HAVE_VIR_DOMAIN_MEMORY_STAT_HUGETLB_PGFAIL
        case VIR_DOMAIN_MEMORY_STAT_HUGETLB_PGFAIL:
            fam = FAM_VIRT_DOMAIN_MEMORY_HUGETLB_PAGE_FAIL;
            break;
#endif
        default:
            break;
        }

        if (fam > 0) {
           if (ctx->fams[fam].type == METRIC_TYPE_COUNTER) {
              metric_family_append(&ctx->fams[fam], VALUE_COUNTER(val), &ctx->labels,
                                    &(label_pair_const_t){.name="domain", .value=ndomain},
                                    &(label_pair_const_t){.name="uuid", .value=uuid},
                                    NULL);
           } else if (ctx->fams[fam].type == METRIC_TYPE_GAUGE) {
              metric_family_append(&ctx->fams[fam], VALUE_GAUGE(val), &ctx->labels,
                                    &(label_pair_const_t){.name="domain", .value=ndomain },
                                    &(label_pair_const_t){.name="uuid", .value=uuid},
                                    NULL);
           }
        }
    }

    free(minfo);
    return 0;
}

#ifdef HAVE_DISK_ERR
static int get_disk_err(virt_ctx_t *ctx, virDomainPtr domain,
                        const char *ndomain, const char *uuid)
{
    /* Get preferred size of disk errors array */
    int disk_err_count = virDomainGetDiskErrors(domain, NULL, 0, 0);
    if (disk_err_count == -1) {
        PLUGIN_ERROR("failed to get preferred size of disk errors array");

        virErrorPtr err = virGetLastError();

        if (err->code == VIR_ERR_NO_SUPPORT) {
            PLUGIN_ERROR("Disabled unsupported selector: disk_err");
            ctx->flags &= ~(COLLECT_VIRT_DISK_ERR);
        }

        return -1;
    }

    PLUGIN_DEBUG("Preferred size of disk errors array: %d for domain %s",
                 disk_err_count, virDomainGetName(domain));
    virDomainDiskError disk_err[disk_err_count];

    disk_err_count = virDomainGetDiskErrors(domain, disk_err, disk_err_count, 0);
    if (disk_err_count == -1) {
        PLUGIN_ERROR("virDomainGetDiskErrors failed with status %d", disk_err_count);
        return -1;
    }

    PLUGIN_DEBUG("detected %d disk errors in domain %s", disk_err_count, virDomainGetName(domain));

    for (int i = 0; i < disk_err_count; ++i) {
        state_t states[] = {
            { .name = "NONE",    .enabled = false },
            { .name = "UNSPEC",  .enabled = false },
            { .name = "NOSPACE", .enabled = false },
        };
        state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };

        switch(disk_err[i].error) {
        case VIR_DOMAIN_DISK_ERROR_NONE:
            set.ptr[0].enabled = true;
            break;
        case VIR_DOMAIN_DISK_ERROR_UNSPEC:
            set.ptr[1].enabled = true;
            break;
        case VIR_DOMAIN_DISK_ERROR_NO_SPACE:
            set.ptr[2].enabled = true;
            break;
        }

       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_DISK_ERROR],
                             VALUE_STATE_SET(set), &ctx->labels,
                             &(label_pair_const_t){.name="domain", .value=ndomain },
                             &(label_pair_const_t){.name="uuid", .value=uuid},
                             &(label_pair_const_t){.name="disk", .value=disk_err[i].disk},
                             NULL);

        free(disk_err[i].disk);
    }

    return 0;
}
#endif /* HAVE_DISK_ERR */

static int get_block_device_stats(virt_ctx_t *ctx, struct block_device *block_dev)
{
    if (!block_dev) {
        PLUGIN_ERROR("get_block_stats NULL pointer");
        return -1;
    }

    virDomainBlockInfo binfo;
    init_block_info(&binfo);

    /* Fetching block info stats only if needed*/
    if (ctx->flags & (COLLECT_VIRT_DISK_ALLOCATION |
                      COLLECT_VIRT_DISK_CAPACITY |
                      COLLECT_VIRT_DISK_PHYSICAL)) {
        /* Block info statistics can be only fetched from devices with 'source' defined */
        if (block_dev->has_source) {
            if (virDomainGetBlockInfo(block_dev->dom, block_dev->path, &binfo, 0) < 0) {
                PLUGIN_ERROR("virDomainGetBlockInfo failed for path: %s", block_dev->path);

                virErrorPtr err = virGetLastError();
                if (err->code == VIR_ERR_NO_SUPPORT) {

                    if (ctx->flags & COLLECT_VIRT_DISK_ALLOCATION)
                        PLUGIN_ERROR("Disabled unsupported selector: disk_allocation");
                    if (ctx->flags & COLLECT_VIRT_DISK_CAPACITY)
                        PLUGIN_ERROR("Disabled unsupported selector: disk_capacity");
                    if (ctx->flags & COLLECT_VIRT_DISK_PHYSICAL)
                        PLUGIN_ERROR("Disabled unsupported selector: disk_physical");

                    ctx->flags &= ~(COLLECT_VIRT_DISK_ALLOCATION |
                                    COLLECT_VIRT_DISK_CAPACITY |
                                    COLLECT_VIRT_DISK_PHYSICAL);
                }

                return -1;
            }
        }
    }

    struct lv_block_stats bstats;
    init_block_stats(&bstats);

    if (lv_domain_block_stats(ctx, block_dev->dom, block_dev->path, &bstats) < 0) {
        PLUGIN_ERROR("lv_domain_block_stats failed");
        return -1;
    }

    const char *ndomain = virDomainGetName(block_dev->dom);
    char uuid[VIR_UUID_STRING_BUFLEN] = {0};
    virDomainGetUUIDString(block_dev->dom, uuid);

    if (bstats.bi.rd_req != -1) {
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_BLOCK_READ_REQUESTS],
                            VALUE_COUNTER(bstats.bi.rd_req), &ctx->labels,
                            &(label_pair_const_t){.name="domain", .value=ndomain },
                            &(label_pair_const_t){.name="uuid", .value=uuid},
                            &(label_pair_const_t){.name="device", .value=block_dev->path},
                            NULL);
    }

    if (bstats.bi.wr_req != -1) {
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_BLOCK_WRITE_REQUESTS],
                            VALUE_COUNTER(bstats.bi.wr_req), &ctx->labels,
                            &(label_pair_const_t){.name="domain", .value=ndomain },
                            &(label_pair_const_t){.name="uuid", .value=uuid},
                            &(label_pair_const_t){.name="device", .value=block_dev->path},
                            NULL);
    }

    if (bstats.bi.rd_bytes != -1) {
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_BLOCK_READ_BYTES],
                            VALUE_COUNTER(bstats.bi.rd_bytes), &ctx->labels,
                            &(label_pair_const_t){.name="domain", .value=ndomain },
                            &(label_pair_const_t){.name="uuid", .value=uuid},
                            &(label_pair_const_t){.name="device", .value=block_dev->path},
                            NULL);
    }

    if (bstats.bi.wr_bytes != -1) {
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_BLOCK_WRITE_BYTES],
                            VALUE_COUNTER(bstats.bi.rd_bytes), &ctx->labels,
                            &(label_pair_const_t){.name="domain", .value=ndomain },
                            &(label_pair_const_t){.name="uuid", .value=uuid},
                            &(label_pair_const_t){.name="device", .value=block_dev->path},
                            NULL);
    }

    if (ctx->flags & COLLECT_VIRT_DISK) {
        if (bstats.rd_total_times != -1) {
           metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_BLOCK_READ_TIME_SECONDS],
                                VALUE_COUNTER((double)bstats.rd_total_times * 1e-9),
                                &ctx->labels,
                                &(label_pair_const_t){.name="domain", .value=ndomain },
                                &(label_pair_const_t){.name="uuid", .value=uuid},
                                &(label_pair_const_t){.name="device", .value=block_dev->path},
                                NULL);
        }

        if (bstats.wr_total_times != -1) {
           metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_BLOCK_WRITE_TIME_SECONDS],
                                VALUE_COUNTER_FLOAT64((double)bstats.wr_total_times * 1e-9),
                                &ctx->labels,
                                &(label_pair_const_t){.name="domain", .value=ndomain },
                                &(label_pair_const_t){.name="uuid", .value=uuid},
                                &(label_pair_const_t){.name="device", .value=block_dev->path},
                                NULL);
        }

        if (bstats.fl_req != -1) {
           metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_BLOCK_FLUSH_REQUESTS],
                                VALUE_COUNTER(bstats.fl_req), &ctx->labels,
                                &(label_pair_const_t){.name="domain", .value=ndomain },
                                &(label_pair_const_t){.name="uuid", .value=uuid},
                                &(label_pair_const_t){.name="device", .value=block_dev->path},
                                NULL);
        }

        if (bstats.fl_total_times != -1) {
           metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_BLOCK_FLUSH_TIME_SECONDS],
                                VALUE_COUNTER_FLOAT64((double)bstats.fl_total_times * 1e-9),
                                &ctx->labels,
                                &(label_pair_const_t){.name="domain", .value=ndomain },
                                &(label_pair_const_t){.name="uuid", .value=uuid},
                                &(label_pair_const_t){.name="device", .value=block_dev->path},
                                NULL);
        }

    }

    if ((ctx->flags & COLLECT_VIRT_DISK_ALLOCATION)) { //  && binfo.allocation != -1
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_BLOCK_ALLOCATION],
                             VALUE_GAUGE(binfo.allocation), &ctx->labels,
                             &(label_pair_const_t){.name="domain", .value=ndomain },
                             &(label_pair_const_t){.name="uuid", .value=uuid},
                             &(label_pair_const_t){.name="device", .value=block_dev->path},
                             NULL);
    }
    if ((ctx->flags & COLLECT_VIRT_DISK_CAPACITY)) { // && binfo.capacity != -1
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_BLOCK_CAPACITY],
                             VALUE_GAUGE(binfo.capacity), &ctx->labels,
                             &(label_pair_const_t){.name="domain", .value=ndomain },
                             &(label_pair_const_t){.name="uuid", .value=uuid},
                             &(label_pair_const_t){.name="device", .value=block_dev->path},
                             NULL);
    }
    if ((ctx->flags & COLLECT_VIRT_DISK_PHYSICAL)) { // && binfo.physical != -1
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_BLOCK_PHYSICALSIZE],
                             VALUE_GAUGE(binfo.physical), &ctx->labels,
                             &(label_pair_const_t){.name="domain", .value=ndomain },
                             &(label_pair_const_t){.name="uuid", .value=uuid},
                             &(label_pair_const_t){.name="device", .value=block_dev->path},
                             NULL);
    }

    return 0;
}

#ifdef HAVE_FS_INFO

static int get_fs_info(virt_ctx_t *ctx, virDomainPtr domain, const char *ndomain, const char *uuid)
{
    virDomainFSInfoPtr *fs_info = NULL;
    int ret = 0;

    int mount_points_cnt = virDomainGetFSInfo(domain, &fs_info, 0);
    if (mount_points_cnt == -1) {
        PLUGIN_ERROR("virDomainGetFSInfo failed: %d", mount_points_cnt);

        virErrorPtr err = virGetLastError();
        if (err->code == VIR_ERR_NO_SUPPORT) {
            PLUGIN_ERROR("Disabled unsupported selector: fs_info");
            ctx->flags &= ~(COLLECT_VIRT_FS_INFO);
        }

        return -1;
    }

    for (int i = 0; i < mount_points_cnt; i++) {
        label_set_t info = {0};

        label_set_add(&info, false, "mountpoint", fs_info[i]->mountpoint);
        label_set_add(&info, false, "name", fs_info[i]->name);
        label_set_add(&info, false, "fstype", fs_info[i]->fstype);

        for (size_t j = 0; j < fs_info[i]->ndevAlias; j++) {
            char alias[64];
            ssnprintf(alias, sizeof(alias), "devalias%zu", j);
            label_set_add(&info, false, alias, fs_info[i]->devAlias[j]);
        }

        metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_FS], VALUE_INFO(info), &ctx->labels,
                             &(label_pair_const_t){.name="domain", .value=ndomain },
                             &(label_pair_const_t){.name="uuid", .value=uuid}, NULL);

        label_set_reset(&info);

        virDomainFSInfoFree(fs_info[i]);
    }

    free(fs_info);
    return ret;
}

#endif

static int get_domain_metrics(virt_ctx_t *ctx, domain_t *domain,
                              const char *ndomain, const char *uuid)
{
    if (!domain || !domain->ptr) {
        PLUGIN_ERROR("get_domain_metrics: NULL pointer");
        return -1;
    }

    virDomainInfo info;
    int status = virDomainGetInfo(domain->ptr, &info);
    if (status != 0) {
        PLUGIN_ERROR("virDomainGetInfo failed with status %i.", status);
        return -1;
    }

    if (ctx->flags & COLLECT_VIRT_DOMAIN_STATE) {
#ifdef HAVE_DOM_REASON
        /* At this point we already know domain's state from virDomainGetInfo call,
         * however it doesn't provide a reason for entering particular state.
         * We need to get it from virDomainGetState.
         */
        status = submit_domain_state(ctx, domain->ptr, ndomain, uuid);
        if (status != 0)
            PLUGIN_WARNING("Failed to get domain reason.");
#endif
    }

    /* Gather remaining stats only for running domains */
    if (info.state != VIR_DOMAIN_RUNNING)
        return 0;

#ifdef HAVE_CPU_STATS
    if (ctx->flags & COLLECT_VIRT_PCPU)
        get_pcpu_stats(ctx, domain->ptr, ndomain, uuid);
#endif

    metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_VCPUS],
                         VALUE_GAUGE(info.nrVirtCpu), &ctx->labels,
                         &(label_pair_const_t){.name="domain", .value=ndomain },
                         &(label_pair_const_t){.name="uuid", .value=uuid},
                         NULL);

    metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_VCPU_ALL_TIME_SECONDS],
                         VALUE_COUNTER_FLOAT64((double)info.cpuTime * (double)1e-9), &ctx->labels,
                         &(label_pair_const_t){.name="domain", .value=ndomain },
                         &(label_pair_const_t){.name="uuid", .value=uuid},
                         NULL);

    metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_MEMORY_MAX_BYTES],
                         VALUE_GAUGE(info.maxMem * 1024), &ctx->labels,
                         &(label_pair_const_t){.name="domain", .value=ndomain },
                         &(label_pair_const_t){.name="uuid", .value=uuid},
                         NULL);

    metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_MEMORY_BYTES],
                         VALUE_GAUGE(info.memory * 1024), &ctx->labels,
                         &(label_pair_const_t){.name="domain", .value=ndomain },
                         &(label_pair_const_t){.name="uuid", .value=uuid},
                         NULL);

    if (ctx->flags & (COLLECT_VIRT_VCPU | COLLECT_VIRT_VCPUPIN)) {
        status = get_vcpu_stats(ctx, domain->ptr, info.nrVirtCpu, ndomain, uuid);
        if (status != 0)
            PLUGIN_WARNING("Failed to get vcpu stats.");
    }

    if (ctx->flags & COLLECT_VIRT_MEMORY) {
        status = get_memory_stats(ctx, domain->ptr, ndomain, uuid);
        if (status != 0)
            PLUGIN_WARNING("Failed to get memory stats.");
    }

#ifdef HAVE_PERF_STATS
    if (ctx->flags & COLLECT_VIRT_PERF) {
        status = get_perf_events(ctx, domain->ptr, ndomain, uuid);
        if (status != 0)
            PLUGIN_WARNING("Failed to get performance monitoring events.");
    }
#endif

#ifdef HAVE_FS_INFO
    if (ctx->flags & COLLECT_VIRT_FS_INFO) {
        status = get_fs_info(ctx, domain->ptr, ndomain, uuid);
        if (status != 0)
            PLUGIN_WARNING("Failed to get file system info.");
    }
#endif

#ifdef HAVE_DISK_ERR
    if (ctx->flags & COLLECT_VIRT_DISK_ERR) {
        status = get_disk_err(ctx, domain->ptr, ndomain, uuid);
        if (status != 0)
            PLUGIN_WARNING("Failed to get disk errors.");
    }
#endif

    /* Update cached virDomainInfo. It has to be done after cpu_submit */
    memcpy(&domain->info, &info, sizeof(domain->info));

    return 0;
}

static int get_if_dev_stats(virt_ctx_t *ctx, struct interface_device *if_dev)
{
    if (!if_dev) {
        PLUGIN_ERROR("get_if_dev_stats: NULL pointer");
        return -1;
    }

    virDomainInterfaceStatsStruct stats = {0};
    if (virDomainInterfaceStats(if_dev->dom, if_dev->path, &stats, sizeof(stats)) != 0) {
        PLUGIN_ERROR("virDomainInterfaceStats failed");
        return -1;
    }

    const char *ndomain = virDomainGetName(if_dev->dom);
    char uuid[VIR_UUID_STRING_BUFLEN] = {0};
    virDomainGetUUIDString(if_dev->dom, uuid);

    if ((stats.rx_bytes != -1) && (stats.tx_bytes != -1)) {
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_INTERFACE_RECEIVE_BYTES],
                             VALUE_COUNTER(stats.rx_bytes), &ctx->labels,
                             &(label_pair_const_t){.name="domain", .value=ndomain },
                             &(label_pair_const_t){.name="uuid", .value=uuid},
                             &(label_pair_const_t){.name="device", .value=if_dev->path},
                             &(label_pair_const_t){.name="device_number", .value=if_dev->number},
                             &(label_pair_const_t){.name="address", .value=if_dev->address},
                             NULL);
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_INTERFACE_TRANSMIT_BYTES],
                             VALUE_COUNTER(stats.tx_bytes), &ctx->labels,
                             &(label_pair_const_t){.name="domain", .value=ndomain },
                             &(label_pair_const_t){.name="uuid", .value=uuid},
                             &(label_pair_const_t){.name="device", .value=if_dev->path},
                             &(label_pair_const_t){.name="device_number", .value=if_dev->number},
                             &(label_pair_const_t){.name="address", .value=if_dev->address},
                             NULL);
    }

    if ((stats.rx_packets != -1) && (stats.tx_packets != -1)) {
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_INTERFACE_RECEIVE_PACKETS],
                             VALUE_COUNTER(stats.rx_packets), &ctx->labels,
                             &(label_pair_const_t){.name="domain", .value=ndomain },
                             &(label_pair_const_t){.name="uuid", .value=uuid},
                             &(label_pair_const_t){.name="device", .value=if_dev->path},
                             &(label_pair_const_t){.name="device_number", .value=if_dev->number},
                             &(label_pair_const_t){.name="address", .value=if_dev->address},
                             NULL);
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_INTERFACE_TRANSMIT_PACKETS],
                             VALUE_COUNTER(stats.tx_packets), &ctx->labels,
                             &(label_pair_const_t){.name="domain", .value=ndomain },
                             &(label_pair_const_t){.name="uuid", .value=uuid},
                             &(label_pair_const_t){.name="device", .value=if_dev->path},
                             &(label_pair_const_t){.name="device_number", .value=if_dev->number},
                             &(label_pair_const_t){.name="address", .value=if_dev->address},
                             NULL);
    }

    if ((stats.rx_errs != -1) && (stats.tx_errs != -1)) {
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_INTERFACE_RECEIVE_ERRORS],
                             VALUE_COUNTER(stats.rx_errs), &ctx->labels,
                             &(label_pair_const_t){.name="domain", .value=ndomain },
                             &(label_pair_const_t){.name="uuid", .value=uuid},
                             &(label_pair_const_t){.name="device", .value=if_dev->path},
                             &(label_pair_const_t){.name="device_number", .value=if_dev->number},
                             &(label_pair_const_t){.name="address", .value=if_dev->address},
                             NULL);
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_INTERFACE_TRANSMIT_ERRORS],
                             VALUE_COUNTER(stats.tx_errs), &ctx->labels,
                             &(label_pair_const_t){.name="domain", .value=ndomain },
                             &(label_pair_const_t){.name="uuid", .value=uuid},
                             &(label_pair_const_t){.name="device", .value=if_dev->path},
                             &(label_pair_const_t){.name="device_number", .value=if_dev->number},
                             &(label_pair_const_t){.name="address", .value=if_dev->address},
                             NULL);
    }

    if ((stats.rx_drop != -1) && (stats.tx_drop != -1)) {
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_INTERFACE_RECEIVE_DROPS],
                             VALUE_COUNTER(stats.rx_drop), &ctx->labels,
                             &(label_pair_const_t){.name="domain", .value=ndomain },
                             &(label_pair_const_t){.name="uuid", .value=uuid},
                             &(label_pair_const_t){.name="device", .value=if_dev->path},
                             &(label_pair_const_t){.name="device_number", .value=if_dev->number},
                             &(label_pair_const_t){.name="address", .value=if_dev->address},
                             NULL);
       metric_family_append(&ctx->fams[FAM_VIRT_DOMAIN_INTERFACE_TRANSMIT_DROPS],
                             VALUE_COUNTER(stats.tx_drop), &ctx->labels,
                             &(label_pair_const_t){.name="domain", .value=ndomain },
                             &(label_pair_const_t){.name="uuid", .value=uuid},
                             &(label_pair_const_t){.name="device", .value=if_dev->path},
                             &(label_pair_const_t){.name="device_number", .value=if_dev->number},
                             &(label_pair_const_t){.name="address", .value=if_dev->address},
                             NULL);
    }

    return 0;
}

static int domain_lifecycle_event_cb(__attribute__((unused)) virConnectPtr con_,
                                     virDomainPtr dom, int event, int detail,
                                     __attribute__((unused)) void *opaque)
{
    int domain_state = map_domain_event_to_state(event);
    int domain_reason = 0; /* 0 means UNKNOWN reason for any state */
#ifdef HAVE_DOM_REASON
    domain_reason = map_domain_event_detail_to_reason(event, detail);
#endif
    domain_state_submit_notif(dom, domain_state, domain_reason);

    return 0;
}

static void virt_eventloop_timeout_cb(int timer ATTRIBUTE_UNUSED,
                                      __attribute__((unused)) void *timer_info)
{
}

static int register_event_impl(void)
{
    if (virEventRegisterDefaultImpl() < 0) {
        virErrorPtr err = virGetLastError();
        PLUGIN_ERROR("error while event implementation registering: %s",
                     err && err->message ? err->message : "Unknown error");
        return -1;
    }

    if (virEventAddTimeout(CDTIME_T_TO_MS(plugin_get_interval()),
                           virt_eventloop_timeout_cb, NULL, NULL) < 0) {
        virErrorPtr err = virGetLastError();
        PLUGIN_ERROR("virEventAddTimeout failed: %s",
                     err && err->message ? err->message : "Unknown error");
        return -1;
    }

    return 0;
}

static void virt_notif_thread_set_active(virt_notif_thread_t *thread_data, const bool active)
{
    assert(thread_data != NULL);
    pthread_mutex_lock(&thread_data->active_mutex);
    thread_data->is_active = active;
    pthread_mutex_unlock(&thread_data->active_mutex);
}

static bool virt_notif_thread_is_active(virt_notif_thread_t *thread_data)
{
    bool active = false;

    assert(thread_data != NULL);
    pthread_mutex_lock(&thread_data->active_mutex);
    active = thread_data->is_active;
    pthread_mutex_unlock(&thread_data->active_mutex);

    return active;
}

/* worker function running default event implementation */
static void *event_loop_worker(void *arg)
{
    virt_notif_thread_t *thread_data = (virt_notif_thread_t *)arg;

    while (virt_notif_thread_is_active(thread_data)) {
        if (virEventRunDefaultImpl() < 0) {
            virErrorPtr err = virGetLastError();
            PLUGIN_ERROR("failed to run event loop: %s\n",
                         err && err->message ? err->message : "Unknown error");
        }
    }

    return NULL;
}

static int virt_notif_thread_init(virt_notif_thread_t *thread_data)
{
    assert(thread_data != NULL);

    int ret = pthread_mutex_init(&thread_data->active_mutex, NULL);
    if (ret != 0) {
        PLUGIN_ERROR("Failed to initialize mutex, err %d", ret);
        return ret;
    }

    /**
     * '0' and positive integers are meaningful ID's, therefore setting
     * domain_event_cb_id to '-1'
     */
    thread_data->domain_event_cb_id = -1;
    pthread_mutex_lock(&thread_data->active_mutex);
    thread_data->is_active = false;
    pthread_mutex_unlock(&thread_data->active_mutex);

    return 0;
}

/* register domain event callback and start event loop thread */
static int start_event_loop(virt_ctx_t *ctx, virt_notif_thread_t *thread_data)
{
    assert(thread_data != NULL);
    thread_data->domain_event_cb_id = virConnectDomainEventRegisterAny(
            ctx->conn, NULL, VIR_DOMAIN_EVENT_ID_LIFECYCLE,
            VIR_DOMAIN_EVENT_CALLBACK(domain_lifecycle_event_cb), NULL, NULL);
    if (thread_data->domain_event_cb_id == -1) {
        PLUGIN_ERROR("error while callback registering");
        return -1;
    }

    PLUGIN_DEBUG("starting event loop");

    virt_notif_thread_set_active(thread_data, 1);
    if (pthread_create(&thread_data->event_loop_tid, NULL, event_loop_worker, thread_data)) {
        PLUGIN_ERROR("failed event loop thread creation");
        virt_notif_thread_set_active(thread_data, 0);
        virConnectDomainEventDeregisterAny(ctx->conn, thread_data->domain_event_cb_id);
        thread_data->domain_event_cb_id = -1;
        return -1;
    }

    return 0;
}

/* stop event loop thread and deregister callback */
static void stop_event_loop(virt_ctx_t *ctx, virt_notif_thread_t *thread_data)
{
    PLUGIN_DEBUG("stopping event loop");

    /* Stopping loop */
    if (virt_notif_thread_is_active(thread_data)) {
        virt_notif_thread_set_active(thread_data, 0);
        if (pthread_join(ctx->notif_thread.event_loop_tid, NULL) != 0)
            PLUGIN_ERROR("stopping notification thread failed");
    }

    /* ... and de-registering event handler */
    if (ctx->conn != NULL && thread_data->domain_event_cb_id != -1) {
        virConnectDomainEventDeregisterAny(ctx->conn, thread_data->domain_event_cb_id);
        thread_data->domain_event_cb_id = -1;
    }
}

static int persistent_domains_state_notification(virt_ctx_t *ctx)
{
    int status = 0;
#ifdef HAVE_LIST_ALL_DOMAINS
    virDomainPtr *domains = NULL;
    int n = virConnectListAllDomains(ctx->conn, &domains, VIR_CONNECT_LIST_DOMAINS_PERSISTENT);
    if (n < 0) {
        VIRT_ERROR(ctx->conn, "reading list of persistent domains");
        status = -1;
    } else {
        PLUGIN_DEBUG("getting state of %i persistent domains", n);
        /* Fetch each persistent domain's state and notify it */
        int n_notified = n;
        for (int i = 0; i < n; ++i) {
            status = get_domain_state_notify(domains[i]);
            if (status != 0) {
                n_notified--;
                PLUGIN_ERROR("could not notify state of domain %s", virDomainGetName(domains[i]));
            }
            virDomainFree(domains[i]);
        }

        free(domains);
        PLUGIN_DEBUG("notified state of %i persistent domains", n_notified);
    }
#else
    int n = virConnectNumOfDomains(ctx->conn);
    if (n > 0) {
        /* Get list of domains. */
        int *domids = calloc(n, sizeof(*domids));
        if (domids == NULL) {
            PLUGIN_ERROR("calloc failed.");
            return -1;
        }
        n = virConnectListDomains(ctx->conn, domids, n);
        if (n < 0) {
            VIRT_ERROR(conn, "reading list of domains");
            free(domids);
            return -1;
        }
        /* Fetch info of each active domain and notify it */
        for (int i = 0; i < n; ++i) {
            virDomainInfo info;
            virDomainPtr dom = NULL;
            dom = virDomainLookupByID(ctx->conn, domids[i]);
            if (dom == NULL) {
                VIRT_ERROR(conn, "virDomainLookupByID");
                /* Could be that the domain went away -- ignore it anyway. */
                continue;
            }
            status = virDomainGetInfo(dom, &info);
            if (status == 0)
                /* virDomainGetState is not available. Submit 0, which corresponds to
                 * unknown reason. */
                domain_state_submit_notif(dom, info.state, 0);
            else
                PLUGIN_ERROR("virDomainGetInfo failed with status %i.", status);

            virDomainFree(dom);
        }
        free(domids);
    }
#endif

    return status;
}

static int lv_read(user_data_t *ud)
{
    if (ud->data == NULL) {
        PLUGIN_ERROR("NULL userdata");
        return -1;
    }

    virt_ctx_t *ctx = ud->data;
    struct lv_read_instance *inst = &ctx->inst;
    struct lv_read_state *state = &inst->read_state;

    if (lv_connect(ctx) < 0)
        return -1;

    /* Wait until inst#0 establish connection */
    if (ctx->conn == NULL) {
        PLUGIN_DEBUG("%zu: Wait until establish connection", inst->id);
        return 0;
    }

    int ret = virConnectIsAlive(ctx->conn);
    if (ret == 0) { /* Connection lost */
        if (inst->id == 0) {
            c_complain(LOG_ERR, &ctx->conn_complain, "Lost connection.");

            if (!ctx->persistent_notification)
                stop_event_loop(ctx, &ctx->notif_thread);

            lv_disconnect(ctx);
            ctx->last_refresh = 0;
        }
        return -1;
    }

    cdtime_t t = cdtime();

    /* Need to refresh domain or device lists? */
    if ((ctx->last_refresh == 0) ||
        ((ctx->refresh_interval > 0) && ((ctx->last_refresh + ctx->refresh_interval) <= t))) {
        if (refresh_lists(ctx, inst) != 0) {
            if (inst->id == 0) {
                if (!ctx->persistent_notification)
                    stop_event_loop(ctx, &ctx->notif_thread);
                lv_disconnect(ctx);
            }
            return -1;
        }
        ctx->last_refresh = t;
    }

    /* persistent domains state notifications are handled by instance 0 */
    if (inst->id == 0 && ctx->persistent_notification) {
        int status = persistent_domains_state_notification(ctx);
        if (status != 0) {
            PLUGIN_DEBUG("persistent_domains_state_notifications returned with status %i", status);
        }
    }

#ifdef NCOLLECTD_DEBUG
    for (int i = 0; i < state->nr_domains; ++i) {
        PLUGIN_DEBUG("domain %s", virDomainGetName(state->domains[i].ptr));
    }
    for (int i = 0; i < state->nr_block_devices; ++i) {
        PLUGIN_DEBUG("block device %d %s:%s", i, virDomainGetName(state->block_devices[i].dom),
                     state->block_devices[i].path);
    }
    for (int i = 0; i < state->nr_interface_devices; ++i) {
        PLUGIN_DEBUG("interface device %d %s:%s", i,
                     virDomainGetName(state->interface_devices[i].dom),
                     state->interface_devices[i].path);
    }
#endif

    /* Get domains' metrics */
    for (int i = 0; i < state->nr_domains; ++i) {
        domain_t *dom = &state->domains[i];
        int status = 0;

        const char *ndomain = virDomainGetName(dom->ptr);
        char uuid[VIR_UUID_STRING_BUFLEN] = {0};
        virDomainGetUUIDString(dom->ptr, uuid);

        if (dom->active) {
            status = get_domain_metrics(ctx, dom, ndomain, uuid);
        }
#ifdef HAVE_DOM_REASON
        else if (ctx->flags & COLLECT_VIRT_DOMAIN_STATE) {
            status = submit_domain_state(ctx, dom->ptr, ndomain, uuid);
        }
#endif

        if (status != 0)
            PLUGIN_ERROR("failed to get metrics for domain=%s", virDomainGetName(dom->ptr));
    }

    /* Get block device stats for each domain. */
    for (int i = 0; i < state->nr_block_devices; ++i) {
        int status = get_block_device_stats(ctx, &state->block_devices[i]);
        if (status != 0)
            PLUGIN_ERROR("failed to get stats for block device (%s) in domain %s",
                         state->block_devices[i].path,
                         virDomainGetName(state->block_devices[i].dom));
    }

    /* Get interface stats for each domain. */
    for (int i = 0; i < state->nr_interface_devices; ++i) {
        int status = get_if_dev_stats(ctx, &state->interface_devices[i]);
        if (status != 0)
            PLUGIN_ERROR("failed to get interface stats for device (%s) in domain %s",
                         state->interface_devices[i].path,
                         virDomainGetName(state->interface_devices[i].dom));
    }

    plugin_dispatch_metric_family_array(ctx->fams, FAM_VIRT_MAX, 0);

    return 0;
}

static void lv_clean_read_state(struct lv_read_state *state)
{
    free_block_devices(state);
    free_interface_devices(state);
    free_domains(state);
}

static void lv_add_block_devices(virt_ctx_t *ctx, struct lv_read_state *state, virDomainPtr dom,
                                 const char *domname,
                                 xmlXPathContextPtr xpath_ctx)
{
    xmlXPathObjectPtr xpath_obj =
            xmlXPathEval((const xmlChar *)"/domain/devices/disk", xpath_ctx);
    if (xpath_obj == NULL) {
        PLUGIN_DEBUG("no disk xpath-object found for domain %s", domname);
        return;
    }

    if (xpath_obj->type != XPATH_NODESET || xpath_obj->nodesetval == NULL) {
        PLUGIN_DEBUG("no disk node found for domain %s", domname);
        goto cleanup;
    }

    xmlNodeSetPtr xml_block_devices = xpath_obj->nodesetval;
    for (int i = 0; i < xml_block_devices->nodeNr; ++i) {
        xmlNodePtr xml_device = xpath_obj->nodesetval->nodeTab[i];
        char *path_str = NULL;
        char *source_str = NULL;

        if (!xml_device)
            continue;

        /* Fetching path and source for block device */
        for (xmlNodePtr child = xml_device->children; child; child = child->next) {
            if (child->type != XML_ELEMENT_NODE)
                continue;

            /* we are interested only in either "target" or "source" elements */
            if (xmlStrEqual(child->name, (const xmlChar *)"target")) {
                path_str = (char *)xmlGetProp(child, (const xmlChar *)"dev");
            } else if (xmlStrEqual(child->name, (const xmlChar *)"source")) {
                /* name of the source is located in "dev" or "file" element (it depends
                 * on type of source). Trying "dev" at first*/
                source_str = (char *)xmlGetProp(child, (const xmlChar *)"dev");
                if (!source_str)
                    source_str = (char *)xmlGetProp(child, (const xmlChar *)"file");
            }
            /* ignoring any other element*/
        }

        /* source_str will be interpreted as a device path if blockdevice_format
         *  param is set to 'source'. */
        const char *device_path = (ctx->blockdevice_format == source) ? source_str : path_str;

        if (!device_path) {
            /* no path found and we can't add block_device without it */
            PLUGIN_WARNING("Could not generate device path for disk in "
                           "domain %s - disk device will be ignored in reports", domname);
            goto cont;
        }

        if (exclist_device_match(&ctx->excl_block_devices, domname, device_path) == 0) {
            /* we only have to store information whether 'source' exists or not */
            bool has_source = (source_str != NULL) ? true : false;
            add_block_device(state, dom, device_path, has_source);
        }

    cont:
        if (path_str)
            xmlFree(path_str);

        if (source_str)
            xmlFree(source_str);
    }

cleanup:
    xmlXPathFreeObject(xpath_obj);
}

static void lv_add_network_interfaces(virt_ctx_t *ctx, struct lv_read_state *state,
                                      virDomainPtr dom, const char *domname,
                                      xmlXPathContextPtr xpath_ctx)
{
    xmlXPathObjectPtr xpath_obj = xmlXPathEval(
            (xmlChar *)"/domain/devices/interface[target[@dev]]", xpath_ctx);

    if (xpath_obj == NULL)
        return;

    if (xpath_obj->type != XPATH_NODESET || xpath_obj->nodesetval == NULL) {
        xmlXPathFreeObject(xpath_obj);
        return;
    }

    xmlNodeSetPtr xml_interfaces = xpath_obj->nodesetval;

    for (int j = 0; j < xml_interfaces->nodeNr; ++j) {
        char *path = NULL;
        char *address = NULL;
        const int itf_number = j + 1;

        xmlNodePtr xml_interface = xml_interfaces->nodeTab[j];
        if (!xml_interface)
            continue;

        for (xmlNodePtr child = xml_interface->children; child; child = child->next) {
            if (child->type != XML_ELEMENT_NODE)
                continue;

            if (xmlStrEqual(child->name, (const xmlChar *)"target")) {
                path = (char *)xmlGetProp(child, (const xmlChar *)"dev");
                if (!path)
                    continue;
            } else if (xmlStrEqual(child->name, (const xmlChar *)"mac")) {
                address = (char *)xmlGetProp(child, (const xmlChar *)"address");
                if (!address)
                    continue;
            }
        }

        bool device_ignored = false;
        switch (ctx->interface_format) {
        case if_name:
            if (!exclist_device_match(&ctx->excl_interface_devices, domname, path))
                device_ignored = true;
            break;
        case if_address:
            if (!exclist_device_match(&ctx->excl_interface_devices, domname, address))
                device_ignored = true;
            break;
        case if_number: {
            char number_string[4];
            ssnprintf(number_string, sizeof(number_string), "%d", itf_number);
            if (!exclist_device_match(&ctx->excl_interface_devices, domname, number_string))
                device_ignored = true;
        }   break;
        default:
            PLUGIN_ERROR("Unknown interface_format option: %u", ctx->interface_format);
            break;
        }

        if (!device_ignored)
            add_interface_device(state, dom, path, address, itf_number);

        if (path)
            xmlFree(path);
        if (address)
            xmlFree(address);
    }
    xmlXPathFreeObject(xpath_obj);
}

static bool is_domain_ignored(virt_ctx_t *ctx, virDomainPtr dom)
{
    const char *domname = virDomainGetName(dom);
    if (domname == NULL) {
        VIRT_ERROR(ctx->conn, "virDomainGetName failed, ignoring domain");
        return true;
    }

    if (!exclist_match(&ctx->excl_domains, domname)) {
        PLUGIN_DEBUG("ignoring domain '%s' because of ignorelist option", domname);
        return true;
    }

    return false;
}

static int refresh_lists(virt_ctx_t *ctx, struct lv_read_instance *inst)
{
    struct lv_read_state *state = &inst->read_state;
    int n;

#ifndef HAVE_LIST_ALL_DOMAINS
    n = virConnectNumOfDomains(conn);
    if (n < 0) {
        VIRT_ERROR(conn, "reading number of domains");
        return -1;
    }
#endif

    lv_clean_read_state(state);

#ifndef HAVE_LIST_ALL_DOMAINS
    if (n == 0)
        goto end;
#endif

#ifdef HAVE_LIST_ALL_DOMAINS
    virDomainPtr *domains, *domains_inactive;
    int m = virConnectListAllDomains(ctx->conn, &domains_inactive, VIR_CONNECT_LIST_DOMAINS_INACTIVE);
    n = virConnectListAllDomains(ctx->conn, &domains, VIR_CONNECT_LIST_DOMAINS_ACTIVE);
#else
    /* Get list of domains. */
    int *domids = calloc(n, sizeof(*domids));
    if (domids == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    n = virConnectListDomains(ctx->conn, domids, n);
#endif

    if (n < 0) {
        VIRT_ERROR(ctx->conn, "reading list of domains");
#ifndef HAVE_LIST_ALL_DOMAINS
        free(domids);
#else
        for (int i = 0; i < m; ++i)
            virDomainFree(domains_inactive[i]);
        free(domains_inactive);
#endif
        return -1;
    }

#ifdef HAVE_LIST_ALL_DOMAINS
    for (int i = 0; i < m; ++i)
        if (is_domain_ignored(ctx, domains_inactive[i]) ||
                add_domain(state, domains_inactive[i], 0) < 0) {
            /* domain ignored or failed during adding to domains list*/
            virDomainFree(domains_inactive[i]);
            domains_inactive[i] = NULL;
            continue;
        }
#endif

    /* Fetch each domain and add it to the list, unless ignore. */
    for (int i = 0; i < n; ++i) {

#ifdef HAVE_LIST_ALL_DOMAINS
        virDomainPtr dom = domains[i];
#else
        virDomainPtr dom = virDomainLookupByID(conn, domids[i]);
        if (dom == NULL) {
            VIRT_ERROR(ctx->conn, "virDomainLookupByID");
            /* Could be that the domain went away -- ignore it anyway. */
            continue;
        }
#endif

        if (is_domain_ignored(ctx, dom) || add_domain(state, dom, 1) < 0) {
            /*
             * domain ignored or failed during adding to domains list
             *
             * When domain is already tracked, then there is
             * no problem with memory handling (will be freed
             * with the rest of domains cached data)
             * But in case of error like this (error occurred
             * before adding domain to track) we have to take
             * care it ourselves and call virDomainFree
             */
            virDomainFree(dom);
            continue;
        }

        const char *domname = virDomainGetName(dom);
        if (domname == NULL) {
            VIRT_ERROR(ctx->conn, "virDomainGetName");
            continue;
        }

        virDomainInfo info;
        int status = virDomainGetInfo(dom, &info);
        if (status != 0) {
            PLUGIN_ERROR("virDomainGetInfo failed with status %i.", status);
            continue;
        }

        if (info.state != VIR_DOMAIN_RUNNING) {
            PLUGIN_DEBUG("skipping inactive domain %s", domname);
            continue;
        }

        /* Get a list of devices for this domain. */
        xmlDocPtr xml_doc = NULL;
        xmlXPathContextPtr xpath_ctx = NULL;

        char *xml = virDomainGetXMLDesc(dom, 0);
        if (!xml) {
            VIRT_ERROR(ctx->conn, "virDomainGetXMLDesc");
            goto cont;
        }

        /* Yuck, XML.  Parse out the devices. */
        xml_doc = xmlReadDoc((xmlChar *)xml, NULL, NULL, XML_PARSE_NONET);
        if (xml_doc == NULL) {
            VIRT_ERROR(ctx->conn, "xmlReadDoc");
            goto cont;
        }

        xpath_ctx = xmlXPathNewContext(xml_doc);

        /* Block devices. */
        lv_add_block_devices(ctx, state, dom, domname, xpath_ctx);

        /* Network interfaces. */
        lv_add_network_interfaces(ctx, state, dom, domname, xpath_ctx);

    cont:
        if (xpath_ctx)
            xmlXPathFreeContext(xpath_ctx);
        if (xml_doc)
            xmlFreeDoc(xml_doc);
        free(xml);
    }

#ifdef HAVE_LIST_ALL_DOMAINS
    /* NOTE: domains_active and domains_inactive data will be cleared during
         refresh of all domains (inside lv_clean_read_state function) so we need
         to free here only allocated arrays */
    free(domains);
    free(domains_inactive);
#else
    free(domids);

end:
#endif

    PLUGIN_DEBUG("refreshing domains=%d block_devices=%d iface_devices=%d",
                 state->nr_domains, state->nr_block_devices, state->nr_interface_devices);
    return 0;
}

static void free_domains(struct lv_read_state *state)
{
    if (state->domains) {
        for (int i = 0; i < state->nr_domains; ++i)
            virDomainFree(state->domains[i].ptr);
        free(state->domains);
    }
    state->domains = NULL;
    state->nr_domains = 0;
}

static int add_domain(struct lv_read_state *state, virDomainPtr dom, bool active)
{
    int new_size = sizeof(state->domains[0]) * (state->nr_domains + 1);

    domain_t *new_ptr = realloc(state->domains, new_size);
    if (new_ptr == NULL) {
        PLUGIN_ERROR("realloc failed in add_domain()");
        return -1;
    }

    state->domains = new_ptr;
    state->domains[state->nr_domains].ptr = dom;
    state->domains[state->nr_domains].active = active;
    memset(&state->domains[state->nr_domains].info, 0,
                 sizeof(state->domains[state->nr_domains].info));

    return state->nr_domains++;
}

static void free_block_devices(struct lv_read_state *state)
{
    if (state->block_devices) {
        for (int i = 0; i < state->nr_block_devices; ++i)
            free(state->block_devices[i].path);
        free(state->block_devices);
    }
    state->block_devices = NULL;
    state->nr_block_devices = 0;
}

static int add_block_device(struct lv_read_state *state, virDomainPtr dom,
                            const char *path, bool has_source)
{
    char *path_copy = strdup(path);
    if (!path_copy)
        return -1;

    int new_size =
            sizeof(state->block_devices[0]) * (state->nr_block_devices + 1);

    struct block_device *new_ptr = realloc(state->block_devices, new_size);
    if (new_ptr == NULL) {
        free(path_copy);
        return -1;
    }
    state->block_devices = new_ptr;
    state->block_devices[state->nr_block_devices].dom = dom;
    state->block_devices[state->nr_block_devices].path = path_copy;
    state->block_devices[state->nr_block_devices].has_source = has_source;
    return state->nr_block_devices++;
}

static void free_interface_devices(struct lv_read_state *state)
{
    if (state->interface_devices) {
        for (int i = 0; i < state->nr_interface_devices; ++i) {
            free(state->interface_devices[i].path);
            free(state->interface_devices[i].address);
            free(state->interface_devices[i].number);
        }
        free(state->interface_devices);
    }
    state->interface_devices = NULL;
    state->nr_interface_devices = 0;
}

static int add_interface_device(struct lv_read_state *state, virDomainPtr dom,
                                const char *path, const char *address,
                                unsigned int number)
{
    if ((path == NULL) || (address == NULL))
        return EINVAL;

    char *path_copy = strdup(path);
    if (!path_copy)
        return -1;

    char *address_copy = strdup(address);
    if (!address_copy) {
        free(path_copy);
        return -1;
    }

    char number_string[ITOA_MAX];
    itoa(number, number_string);
    char *number_copy = strdup(number_string);
    if (!number_copy) {
        free(path_copy);
        free(address_copy);
        return -1;
    }

    int new_size = sizeof(state->interface_devices[0]) * (state->nr_interface_devices + 1);

    struct interface_device *new_ptr =
            realloc(state->interface_devices, new_size);
    if (new_ptr == NULL) {
        free(path_copy);
        free(address_copy);
        free(number_copy);
        return -1;
    }

    state->interface_devices = new_ptr;
    state->interface_devices[state->nr_interface_devices].dom = dom;
    state->interface_devices[state->nr_interface_devices].path = path_copy;
    state->interface_devices[state->nr_interface_devices].address = address_copy;
    state->interface_devices[state->nr_interface_devices].number = number_copy;
    return state->nr_interface_devices++;
}

static bool exclist_device_match(exclist_t *excl, const char *domname, const char *devpath)
{
    if ((domname == NULL) || (devpath == NULL))
        return 0;

    size_t n = strlen(domname) + strlen(devpath) + 2;
    char *name = malloc(n);
    if (name == NULL) {
        PLUGIN_ERROR("malloc failed.");
        return 0;
    }
    ssnprintf(name, n, "%s:%s", domname, devpath);
    bool r = exclist_match(excl, name);
    free(name);
    return r;
}

static void lv_free(void *arg)
{
    if (arg == NULL)
        return;

    virt_ctx_t *ctx = arg;

    struct lv_read_instance *inst = &(ctx->inst);
    struct lv_read_state *state = &(inst->read_state);
    lv_clean_read_state(state);


    if (!ctx->persistent_notification)
        stop_event_loop(ctx, &ctx->notif_thread);

    lv_disconnect(ctx);

    exclist_reset(&ctx->excl_domains);
    exclist_reset(&ctx->excl_block_devices);
    exclist_reset(&ctx->excl_interface_devices);


    free(ctx);
}

static int lv_config_instance(config_item_t *ci)
{
    virt_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed");
        return -1;
    }

    memcpy(ctx->fams, fams_virt, sizeof(ctx->fams[0])*FAM_VIRT_MAX);

    int status = cf_util_get_string(ci, &ctx->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(ctx);
        return status;
    }

    ctx->refresh_interval = TIME_T_TO_CDTIME_T(60);
    cdtime_t interval = 0;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *c = ci->children + i;

        if (strcasecmp(c->key, "connection") == 0) {
            status = cf_util_get_string(c, &ctx->conn_string);
        } else if (strcasecmp(c->key, "refresh-interval") == 0) {
            status = cf_util_get_cdtime(c, &ctx->refresh_interval);
        } else if (strcasecmp(c->key, "interval") == 0) {
            status = cf_util_get_cdtime(c, &interval);
        } else if (strcasecmp(c->key, "label") == 0) {
            status = cf_util_get_label(c, &ctx->labels);
        } else if (strcasecmp(c->key, "collect") == 0) {
            status = cf_util_get_flags(c, virt_flags_list, virt_flags_size, &ctx->flags);
        } else if (strcasecmp(c->key, "domain") == 0) {
            status = cf_util_exclist(c, &ctx->excl_domains);
        } else if (strcasecmp(c->key, "block-device") == 0) {
            status = cf_util_exclist(c, &ctx->excl_block_devices);
        } else if (strcasecmp(c->key, "block-device-format") == 0) {
            char *device_format = NULL;
            status = cf_util_get_string(c, &device_format);
            if (status == 0) {
                if (strcasecmp(device_format, "target") == 0) {
                    ctx->blockdevice_format = target;
                } else if (strcasecmp(device_format, "source") == 0) {
                    ctx->blockdevice_format = source;
                } else {
                    PLUGIN_ERROR("unknown 'block-device-format': %s", device_format);
                    status = -1;
                }
                free(device_format);
            }
        } else if (strcasecmp(c->key, "interface-device") == 0) {
            status = cf_util_exclist(c, &ctx->excl_interface_devices);
        } else if (strcasecmp(c->key, "interface-format") == 0) {
            char *format = NULL;
            status = cf_util_get_string(c, &format);
            if (status == 0) {
                if (strcasecmp(format, "name") == 0) {
                    ctx->interface_format = if_name;
                } else if (strcasecmp(format, "address") == 0) {
                    ctx->interface_format = if_address;
                } else if (strcasecmp(format, "number") == 0) {
                    ctx->interface_format = if_number;
                } else {
                    PLUGIN_ERROR("unknown InterfaceFormat: %s", format);
                    status = -1;
                }
                free(format);
            }
        } else if (strcasecmp(c->key, "persistent-notification") == 0) {
            status = cf_util_get_boolean(c, &ctx->persistent_notification);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          c->key, cf_get_file(c), cf_get_lineno(c));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        lv_free(ctx);
        return -1;
    }

    if (!ctx->persistent_notification) {
        if (virt_notif_thread_init(&ctx->notif_thread) != 0) {
            lv_free(ctx);
            return -1;
        }
    }

    label_set_add(&ctx->labels, true, "instance", ctx->name);

    return plugin_register_complex_read("virt", ctx->name, lv_read, interval,
                                        &(user_data_t){.data = ctx, .free_func = lv_free});
}

static int lv_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = lv_config_instance(child);
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

static int lv_init(void)
{
    if (virInitialize() != 0)
        return -1;

    return 0;
}

void module_register(void)
{
    plugin_register_config("virt", lv_config);
    plugin_register_init("virt", lv_init);
}
