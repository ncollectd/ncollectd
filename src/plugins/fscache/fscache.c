// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2009 Edward "Koko" Konetzko
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Edward "Koko" Konetzko <konetzed at quixoticagony.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <stdio.h>  /* a header needed for FILE */
#include <stdlib.h> /* used for atoi */
#include <string.h> /* a header needed for scanf function */


#ifndef KERNEL_LINUX
#error "This module only supports the Linux implementation of fscache"
#endif

#define BUFSIZE 1024

/*
see /proc/fs/fscache/stats
see Documentation/filesystems/caching/fscache.txt in linux kernel >= 2.6.30
*/

enum {
    FAM_FSCACHE_COOKIE_INDEX,
    FAM_FSCACHE_COOKIE_DATA,
    FAM_FSCACHE_COOKIE_SPECIAL,
    FAM_FSCACHE_OBJECT_ALLOC,
    FAM_FSCACHE_OBJECT_NO_ALLOC,
    FAM_FSCACHE_OBJECT_AVAIL,
    FAM_FSCACHE_OBJECT_DEAD,
    FAM_FSCACHE_CHECKAUX_NONE,
    FAM_FSCACHE_CHECKAUX_OKAY,
    FAM_FSCACHE_CHECKAUX_UPDATE,
    FAM_FSCACHE_CHECKAUX_OBSOLETE,
    FAM_FSCACHE_MARKS,
    FAM_FSCACHE_UNCACHES,
    FAM_FSCACHE_ACQUIRES,
    FAM_FSCACHE_ACQUIRES_NULL,
    FAM_FSCACHE_ACQUIRES_NO_CACHE,
    FAM_FSCACHE_ACQUIRES_OK,
    FAM_FSCACHE_ACQUIRES_NOBUFS,
    FAM_FSCACHE_ACQUIRES_OOM,
    FAM_FSCACHE_OBJECT_LOOKUPS,
    FAM_FSCACHE_OBJECT_LOOKUPS_NEGATIVE,
    FAM_FSCACHE_OBJECT_LOOKUPS_POSITIVE,
    FAM_FSCACHE_OBJECT_CREATED,
    FAM_FSCACHE_OBJECT_LOOKUPS_TIMED_OUT,
    FAM_FSCACHE_INVALIDATES,
    FAM_FSCACHE_INVALIDATES_RUN,
    FAM_FSCACHE_UPDATES,
    FAM_FSCACHE_UPDATES_NULL,
    FAM_FSCACHE_UPDATES_RUN,
    FAM_FSCACHE_RELINQUISHES,
    FAM_FSCACHE_RELINQUISHES_NULL,
    FAM_FSCACHE_RELINQUISHES_WAITCRT,
    FAM_FSCACHE_RELINQUISHES_RETIRE,
    FAM_FSCACHE_ATTR_CHANGED,
    FAM_FSCACHE_ATTR_CHANGED_OK,
    FAM_FSCACHE_ATTR_CHANGED_NOBUFS,
    FAM_FSCACHE_ATTR_CHANGED_NOMEM,
    FAM_FSCACHE_ATTR_CHANGED_CALLS,
    FAM_FSCACHE_ALLOCS,
    FAM_FSCACHE_ALLOCS_OK,
    FAM_FSCACHE_ALLOCS_WAIT,
    FAM_FSCACHE_ALLOCS_NOBUFS,
    FAM_FSCACHE_ALLOCS_INTR,
    FAM_FSCACHE_ALLOC_OPS,
    FAM_FSCACHE_ALLOC_OP_WAITS,
    FAM_FSCACHE_ALLOCS_OBJECT_DEAD,
    FAM_FSCACHE_RETRIEVALS,
    FAM_FSCACHE_RETRIEVALS_OK,
    FAM_FSCACHE_RETRIEVALS_WAIT,
    FAM_FSCACHE_RETRIEVALS_NODATA,
    FAM_FSCACHE_RETRIEVALS_NOBUFS,
    FAM_FSCACHE_RETRIEVALS_INTR,
    FAM_FSCACHE_RETRIEVALS_NOMEM,
    FAM_FSCACHE_RETRIEVAL_OPS,
    FAM_FSCACHE_RETRIEVAL_OP_WAITS,
    FAM_FSCACHE_RETRIEVALS_OBJECT_DEAD,
    FAM_FSCACHE_STORE,
    FAM_FSCACHE_STORE_OK,
    FAM_FSCACHE_STORE_AGAIN,
    FAM_FSCACHE_STORE_NOBUFS,
    FAM_FSCACHE_STORE_OOM,
    FAM_FSCACHE_STORE_OPS,
    FAM_FSCACHE_STORE_CALLS,
    FAM_FSCACHE_STORE_PAGES,
    FAM_FSCACHE_STORE_RADIX_DELETES,
    FAM_FSCACHE_STORE_PAGES_OVER_LIMIT,
    FAM_FSCACHE_STORE_VMSCAN_NOT_STORING,
    FAM_FSCACHE_STORE_VMSCAN_GONE,
    FAM_FSCACHE_STORE_VMSCAN_BUSY,
    FAM_FSCACHE_STORE_VMSCAN_CANCELLED,
    FAM_FSCACHE_STORE_VMSCAN_WAIT,
    FAM_FSCACHE_OP_PENDING,
    FAM_FSCACHE_OP_RUN,
    FAM_FSCACHE_OP_ENQUEUE,
    FAM_FSCACHE_OP_CANCELLED,
    FAM_FSCACHE_OP_REJECTED,
    FAM_FSCACHE_OP_INITIALISED,
    FAM_FSCACHE_OP_DEFERRED_RELEASE,
    FAM_FSCACHE_OP_RELEASE,
    FAM_FSCACHE_OP_GC,
    FAM_FSCACHE_CACHEOP_ALLOC_OBJECT,
    FAM_FSCACHE_CACHEOP_LOOKUP_OBJECT,
    FAM_FSCACHE_CACHEOP_LOOKUP_COMPLETE,
    FAM_FSCACHE_CACHEOP_GRAB_OBJECT,
    FAM_FSCACHE_CACHEOP_INVALIDATE_OBJECT,
    FAM_FSCACHE_CACHEOP_UPDATE_OBJECT,
    FAM_FSCACHE_CACHEOP_DROP_OBJECT,
    FAM_FSCACHE_CACHEOP_PUT_OBJECT,
    FAM_FSCACHE_CACHEOP_SYNC_CACHE,
    FAM_FSCACHE_CACHEOP_ATTR_CHANGED,
    FAM_FSCACHE_CACHEOP_READ_OR_ALLOC_PAGE,
    FAM_FSCACHE_CACHEOP_READ_OR_ALLOC_PAGES,
    FAM_FSCACHE_CACHEOP_ALLOCATE_PAGE,
    FAM_FSCACHE_CACHEOP_ALLOCATE_PAGES,
    FAM_FSCACHE_CACHEOP_WRITE_PAGE,
    FAM_FSCACHE_CACHEOP_UNCACHE_PAGE,
    FAM_FSCACHE_CACHEOP_DISSOCIATE_PAGES,
    FAM_FSCACHE_CACHE_NO_SPACE_REJECT,
    FAM_FSCACHE_CACHE_STALE_OBJECTS,
    FAM_FSCACHE_CACHE_RETIRED_OBJECTS,
    FAM_FSCACHE_CACHE_CULLED_OBJECTS,
    FAM_FSCACHE_MAX,
};

static metric_family_t fams[FAM_FSCACHE_MAX] = {
    [FAM_FSCACHE_COOKIE_INDEX] = {
        .name = "system_fscache_cookie_index",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of index cookies allocated.",
    },
    [FAM_FSCACHE_COOKIE_DATA] = {
        .name = "system_fscache_cookie_data",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of data storage cookies allocated.",
    },
    [FAM_FSCACHE_COOKIE_SPECIAL] = {
        .name = "system_fscache_cookie_special",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of special cookies allocated.",
    },
    [FAM_FSCACHE_OBJECT_ALLOC] = {
        .name = "system_fscache_object_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of objects allocated.",
    },
    [FAM_FSCACHE_OBJECT_NO_ALLOC] = {
        .name = "system_fscache_object_no_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of object allocation failures.",
    },
    [FAM_FSCACHE_OBJECT_AVAIL] = {
        .name = "system_fscache_object_avail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of objects that reached the available state.",
    },
    [FAM_FSCACHE_OBJECT_DEAD] = {
        .name = "system_fscache_object_dead",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total mumber of objects that reached the dead state.",
    },
    [FAM_FSCACHE_CHECKAUX_NONE] = {
        .name = "system_fscache_checkaux_none",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of objects that didn't have a coherency check.",
    },
    [FAM_FSCACHE_CHECKAUX_OKAY] = {
        .name = "system_fscache_checkaux_okay",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of objects that passed a coherency check.",
    },
    [FAM_FSCACHE_CHECKAUX_UPDATE] = {
        .name = "system_fscache_checkaux_update",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of objects that needed a coherency data update.",
    },
    [FAM_FSCACHE_CHECKAUX_OBSOLETE] = {
        .name = "system_fscache_checkaux_obsolete",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of objects that were declared obsolete.",
    },
    [FAM_FSCACHE_MARKS] = {
        .name = "system_fscache_marks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of pages marked as being cached.",
    },
    [FAM_FSCACHE_UNCACHES] = {
        .name = "system_fscache_uncaches",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of uncache page requests seen.",
    },
    [FAM_FSCACHE_ACQUIRES] = {
        .name = "system_fscache_acquires",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of acquire cookie requests seen.",
    },
    [FAM_FSCACHE_ACQUIRES_NULL] = {
        .name = "system_fscache_acquires_null",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of acquire requests given a NULL parent.",
    },
    [FAM_FSCACHE_ACQUIRES_NO_CACHE] = {
        .name = "system_fscache_acquires_no_cache",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of acquire requests rejected due to no cache available.",
    },
    [FAM_FSCACHE_ACQUIRES_OK] = {
        .name = "system_fscache_acquires_ok",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of acquire requests succeeded.",
    },
    [FAM_FSCACHE_ACQUIRES_NOBUFS] = {
        .name = "system_fscache_acquires_nobufs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of acquire requests rejected due to error.",
    },
    [FAM_FSCACHE_ACQUIRES_OOM] = {
        .name = "system_fscache_acquires_oom",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of acquire requests failed on ENOMEM.",
    },
    [FAM_FSCACHE_OBJECT_LOOKUPS] = {
        .name = "system_fscache_object_lookups",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of lookup calls made on cache backends.",
    },
    [FAM_FSCACHE_OBJECT_LOOKUPS_NEGATIVE] = {
        .name = "system_fscache_object_lookups_negative",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of negative lookups made.",
    },
    [FAM_FSCACHE_OBJECT_LOOKUPS_POSITIVE] = {
        .name = "system_fscache_object_lookups_positive",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of positive lookups made.",
    },
    [FAM_FSCACHE_OBJECT_CREATED] = {
        .name = "system_fscache_object_created",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of objects created by lookup.",
    },
    [FAM_FSCACHE_OBJECT_LOOKUPS_TIMED_OUT] = {
        .name = "system_fscache_object_lookups_timed_out",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of lookups timed out and requeued.",
    },
    [FAM_FSCACHE_INVALIDATES] = {
        .name = "system_fscache_invalidates",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of invalidations.",
    },
    [FAM_FSCACHE_INVALIDATES_RUN] = {
        .name = "system_fscache_invalidates_run",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of invalidations granted CPU time.",
    },
    [FAM_FSCACHE_UPDATES] = {
        .name = "system_fscache_updates",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of update cookie requests seen.",
    },
    [FAM_FSCACHE_UPDATES_NULL] = {
        .name = "system_fscache_updates_null",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of update requests given a NULL parent.",
    },
    [FAM_FSCACHE_UPDATES_RUN] = {
        .name = "system_fscache_updates_run",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of update requests granted CPU time.",
    },
    [FAM_FSCACHE_RELINQUISHES] = {
        .name = "system_fscache_relinquishes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of relinquish cookie requests seen.",
    },
    [FAM_FSCACHE_RELINQUISHES_NULL] = {
        .name = "system_fscache_relinquishes_null",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of relinquish cookie given a NULL parent.",
    },
    [FAM_FSCACHE_RELINQUISHES_WAITCRT] = {
        .name = "system_fscache_relinquishes_waitcrt",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of relinquish cookie waited on completion of creation.",
    },
    [FAM_FSCACHE_RELINQUISHES_RETIRE] = {
        .name = "system_fscache_relinquishes_retire",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of relinquish retries.",
    },
    [FAM_FSCACHE_ATTR_CHANGED] = {
        .name = "system_fscache_attr_changed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of attribute changed requests seen.",
    },
    [FAM_FSCACHE_ATTR_CHANGED_OK] = {
        .name = "system_fscache_attr_changed_ok",
        .type = METRIC_TYPE_COUNTER,
        .help =  "Total number of attribute changed requests queued.",
    },
    [FAM_FSCACHE_ATTR_CHANGED_NOBUFS] = {
        .name = "system_fscache_attr_changed_nobufs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of attribute changed rejected -ENOBUFS.",
    },
    [FAM_FSCACHE_ATTR_CHANGED_NOMEM] = {
        .name = "system_fscache_attr_changed_nomem",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of attribute changed failed -ENOMEM.",
    },
    [FAM_FSCACHE_ATTR_CHANGED_CALLS] = {
        .name = "system_fscache_attr_changed_calls",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of attribute changed ops given CPU time.",
    },
    [FAM_FSCACHE_ALLOCS] = {
        .name = "system_fscache_allocs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of allocation requests seen.",
    },
    [FAM_FSCACHE_ALLOCS_OK] = {
        .name = "system_fscache_allocs_ok",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of successful allocation requests.",
    },
    [FAM_FSCACHE_ALLOCS_WAIT] = {
        .name = "system_fscache_allocs_wait",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of allocation requests that waited on lookup completion.",
    },
    [FAM_FSCACHE_ALLOCS_NOBUFS] = {
        .name = "system_fscache_allocs_nobufs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of allocation requests rejected -ENOBUFS.",
    },
    [FAM_FSCACHE_ALLOCS_INTR] = {
        .name = "system_fscache_allocs_intr",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of allocation requests aborted -ERESTARTSYS.",
    },
    [FAM_FSCACHE_ALLOC_OPS] = {
        .name = "system_fscache_alloc_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of allocation requests submitted.",
    },
    [FAM_FSCACHE_ALLOC_OP_WAITS] = {
        .name = "system_fscache_alloc_op_waits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of allocation requests waited for CPU time.",
    },
    [FAM_FSCACHE_ALLOCS_OBJECT_DEAD] = {
        .name = "system_fscache_allocs_object_dead",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of allocation requests aborted due to object death.",
    },
    [FAM_FSCACHE_RETRIEVALS] = {
        .name = "system_fscache_retrievals",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of retrieval (read) requests seen.",
    },
    [FAM_FSCACHE_RETRIEVALS_OK] = {
        .name = "system_fscache_retrievals_ok",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of successful retrieval requests.",
    },
    [FAM_FSCACHE_RETRIEVALS_WAIT] = {
        .name = "system_fscache_retrievals_wait",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of retrieval requests that waited on lookup completion.",
    },
    [FAM_FSCACHE_RETRIEVALS_NODATA] = {
        .name = "system_fscache_retrievals_nodata",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of retrieval requests returned -ENODATA.",
    },
    [FAM_FSCACHE_RETRIEVALS_NOBUFS] = {
        .name = "system_fscache_retrievals_nobufs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of retrieval requests rejected -ENOBUFS.",
    },
    [FAM_FSCACHE_RETRIEVALS_INTR] = {
        .name = "system_fscache_retrievals_intr",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of retrieval requests aborted -ERESTARTSYS.",
    },
    [FAM_FSCACHE_RETRIEVALS_NOMEM] = {
        .name = "system_fscache_retrievals_nomem",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of retrieval requests failed -ENOMEM.",
    },
    [FAM_FSCACHE_RETRIEVAL_OPS] = {
        .name = "system_fscache_retrieval_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of retrieval requests submitted.",
    },
    [FAM_FSCACHE_RETRIEVAL_OP_WAITS] = {
        .name = "system_fscache_retrieval_op_waits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of retrieval requests waited for CPU time.",
    },
    [FAM_FSCACHE_RETRIEVALS_OBJECT_DEAD] = {
        .name = "system_fscache_retrievals_object_dead",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of retrieval requests aborted due to object death.",
    },
    [FAM_FSCACHE_STORE] = {
        .name = "system_fscache_store",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of storage (write) requests seen.",
    },
    [FAM_FSCACHE_STORE_OK] = {
        .name = "system_fscache_store_ok",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of successful store requests.",
    },
    [FAM_FSCACHE_STORE_AGAIN] = {
        .name = "system_fscache_store_again",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of store requests on a page already pending storage.",
    },
    [FAM_FSCACHE_STORE_NOBUFS] = {
        .name = "system_fscache_store_nobufs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of store requests rejected -ENOBUFS.",
    },
    [FAM_FSCACHE_STORE_OOM] = {
        .name = "system_fscache_store_oom",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of store requests failed -ENOMEM.",
    },
    [FAM_FSCACHE_STORE_OPS] = {
        .name = "system_fscache_store_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of store requests submitted.",
    },
    [FAM_FSCACHE_STORE_CALLS] = {
        .name = "system_fscache_store_calls",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of store requests granted CPU time.",
    },
    [FAM_FSCACHE_STORE_PAGES] = {
        .name = "system_fscache_store_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of pages given store requests processing time.",
    },
    [FAM_FSCACHE_STORE_RADIX_DELETES] = {
        .name = "system_fscache_store_radix_deletes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of store requests deleted from tracking tree.",
    },
    [FAM_FSCACHE_STORE_PAGES_OVER_LIMIT] = {
        .name = "system_fscache_store_pages_over_limit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of store requests over store limit.",
    },
    [FAM_FSCACHE_STORE_VMSCAN_NOT_STORING] = {
        .name = "system_fscache_store_vmscan_not_storing",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of release requests against pages with no pending store.",
    },
    [FAM_FSCACHE_STORE_VMSCAN_GONE] = {
        .name = "system_fscache_store_vmscan_gone",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of release requests against pages stored by time lock granted.",
    },
    [FAM_FSCACHE_STORE_VMSCAN_BUSY] = {
        .name = "system_fscache_store_vmscan_busy",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of release requests ignored due to in-progress store.",
    },
    [FAM_FSCACHE_STORE_VMSCAN_CANCELLED] = {
        .name = "system_fscache_store_vmscan_cancelled",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of page stores cancelled due to release request.",
    },
    [FAM_FSCACHE_STORE_VMSCAN_WAIT] = {
        .name = "system_fscache_store_vmscan_wait",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of page stores waited for CPU time.",
    },
    [FAM_FSCACHE_OP_PENDING] = {
        .name = "system_fscache_op_pending",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times async ops added to pending queues.",
    },
    [FAM_FSCACHE_OP_RUN] = {
        .name = "system_fscache_op_run",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times async ops given CPU time.",
    },
    [FAM_FSCACHE_OP_ENQUEUE] = {
        .name = "system_fscache_op_enqueue",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times async ops queued for processing.",
    },
    [FAM_FSCACHE_OP_CANCELLED] = {
        .name = "system_fscache_op_cancelled",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of async ops cancelled.",
    },
    [FAM_FSCACHE_OP_REJECTED] = {
        .name = "system_fscache_op_rejected",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of async ops rejected due to object lookup/create failure.",
    },
    [FAM_FSCACHE_OP_INITIALISED] = {
        .name = "system_fscache_op_initialised",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of async ops initialised.",
    },
    [FAM_FSCACHE_OP_DEFERRED_RELEASE] = {
        .name = "system_fscache_op_deferred_release",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of async ops queued for deferred release.",
    },
    [FAM_FSCACHE_OP_RELEASE] = {
        .name = "system_fscache_op_release",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of async ops released (should equal ini=N when idle).",
    },
    [FAM_FSCACHE_OP_GC] = {
        .name = "system_fscache_op_gc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of deferred-release async ops garbage collected.",
    },
    [FAM_FSCACHE_CACHEOP_ALLOC_OBJECT] = {
        .name = "system_fscache_cacheop_alloc_object",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress alloc_object() cache ops.",
    },
    [FAM_FSCACHE_CACHEOP_LOOKUP_OBJECT] = {
        .name = "system_fscache_cacheop_lookup_object",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress lookup_object() cache ops.",
    },
    [FAM_FSCACHE_CACHEOP_LOOKUP_COMPLETE] = {
        .name = "system_fscache_cacheop_lookup_complete",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress lookup_complete() cache ops.",
    },
    [FAM_FSCACHE_CACHEOP_GRAB_OBJECT] = {
        .name = "system_fscache_cacheop_grab_object",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress grab_object() cache ops.",
    },
    [FAM_FSCACHE_CACHEOP_INVALIDATE_OBJECT] = {
        .name = "system_fscache_cacheop_invalidate_object" ,
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress invalidate_object() cache ops.",
    },
    [FAM_FSCACHE_CACHEOP_UPDATE_OBJECT] = {
        .name = "system_fscache_cacheop_update_object",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress update_object() cache ops.",
    },
    [FAM_FSCACHE_CACHEOP_DROP_OBJECT] = {
        .name = "system_fscache_cacheop_drop_object",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress drop_object() cache ops.",
    },
    [FAM_FSCACHE_CACHEOP_PUT_OBJECT] = {
        .name = "system_fscache_cacheop_put_object",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress put_object() cache ops.",
    },
    [FAM_FSCACHE_CACHEOP_SYNC_CACHE] = {
        .name = "system_fscache_cacheop_sync_cache",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress sync_cache() cache ops.",
    },
    [FAM_FSCACHE_CACHEOP_ATTR_CHANGED] = {
        .name = "system_fscache_cacheop_attr_changed",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress attr_changed() cache ops.",
    },
    [FAM_FSCACHE_CACHEOP_READ_OR_ALLOC_PAGE] = {
        .name = "system_fscache_cacheop_read_or_alloc_page",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress read_or_alloc_page() cache ops.",
    },
    [FAM_FSCACHE_CACHEOP_READ_OR_ALLOC_PAGES] = {
        .name = "system_fscache_cacheop_read_or_alloc_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress read_or_alloc_pages() cache ops.",
    },
    [FAM_FSCACHE_CACHEOP_ALLOCATE_PAGE] = {
        .name = "system_fscache_cacheop_allocate_page",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress allocate_page() cache ops.",
    },
    [FAM_FSCACHE_CACHEOP_ALLOCATE_PAGES] = {
        .name = "system_fscache_cacheop_allocate_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress allocate_pages() cache ops.",
    },
    [FAM_FSCACHE_CACHEOP_WRITE_PAGE] = {
        .name = "system_fscache_cacheop_write_page",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress write_page() cache ops.",
    },
    [FAM_FSCACHE_CACHEOP_UNCACHE_PAGE] = {
        .name = "system_fscache_cacheop_uncache_page",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress uncache_page() cache ops.",
    },
    [FAM_FSCACHE_CACHEOP_DISSOCIATE_PAGES] = {
        .name = "system_fscache_cacheop_dissociate_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of in-progress dissociate_pages() cache ops.",
    },
    [FAM_FSCACHE_CACHE_NO_SPACE_REJECT] = {
        .name = "system_fscache_cache_no_space_reject",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of object lookups/creations rejected due to lack of space.",
    },
    [FAM_FSCACHE_CACHE_STALE_OBJECTS] = {
        .name = "system_fscache_cache_stale_objects",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of stale objects deleted.",
    },
    [FAM_FSCACHE_CACHE_RETIRED_OBJECTS] = {
        .name = "system_fscache_cache_retired_objects",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of objects retired when relinquished.",
    },
    [FAM_FSCACHE_CACHE_CULLED_OBJECTS] = {
        .name = "system_fscache_cache_culled_objects",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of objects culled.",
    },
};

#include "plugins/fscache/fscache.h"

static char *path_proc_fscache;

static int fscache_read(void)
{
    FILE *fh;
    fh = fopen(path_proc_fscache, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Cannot open file '%s'", path_proc_fscache);
        return -1;
    }

    char linebuffer[BUFSIZE];
    /*
     *  cat /proc/fs/fscache/stats
     *          FS-Cache statistics
     *          Cookies: idx=0 dat=0 spc=0
     *          Objects: alc=0 nal=0 avl=0 ded=0
     *          ChkAux : non=0 ok=0 upd=0 obs=0
     *          Pages  : mrk=0 unc=0
     *          Acquire: n=0 nul=0 noc=0 ok=0 nbf=0 oom=0
     *          Lookups: n=0 neg=0 pos=0 crt=0 tmo=0
     *          Invals : n=0 run=0
     *          Updates: n=0 nul=0 run=0
     *          Relinqs: n=0 nul=0 wcr=0 rtr=0
     *          AttrChg: n=0 ok=0 nbf=0 oom=0 run=0
     *          Allocs : n=0 ok=0 wt=0 nbf=0 int=0
     *          Allocs : ops=0 owt=0 abt=0
     *          Retrvls: n=0 ok=0 wt=0 nod=0 nbf=0 int=0 oom=0
     *          Retrvls: ops=0 owt=0 abt=0
     *          Stores : n=0 ok=0 agn=0 nbf=0 oom=0
     *          Stores : ops=0 run=0 pgs=0 rxd=0 olm=0
     *          VmScan : nos=0 gon=0 bsy=0 can=0 wt=0
     *          Ops      : pend=0 run=0 enq=0 can=0 rej=0
     *          Ops      : ini=0 dfr=0 rel=0 gc=0
     *          CacheOp: alo=0 luo=0 luc=0 gro=0
     *          CacheOp: inv=0 upo=0 dro=0 pto=0 atc=0 syn=0
     *          CacheOp: rap=0 ras=0 alp=0 als=0 wrp=0 ucp=0 dsp=0
     *          CacheEv: nsp=0 stl=0 rtr=0 cul=0
     */

    while (fgets(linebuffer, sizeof(linebuffer), fh) != NULL) {
        char section[DATA_MAX_NAME_LEN];
        char *lineptr;
        char *fields[32];
        int fields_num;

        /* Find the colon and replace it with a null byte */
        lineptr = strchr(linebuffer, ':');
        if (lineptr == NULL)
            continue;
        *lineptr = 0;
        lineptr++;

        /* Copy and clean up the section name */
        sstrncpy(section, linebuffer, sizeof(section));
        size_t section_len = strlen(section);
        while ((section_len > 0) && isspace((int)section[section_len - 1])) {
            section_len--;
            section[section_len] = 0;
        }
        if (section_len == 0)
            continue;

        fields_num = strsplit(lineptr, fields, STATIC_ARRAY_SIZE(fields));
        if (fields_num <= 0)
            continue;

        for (int i = 0; i < fields_num; i++) {
            char *field_name;
            char *field_value_str;
            int status;

            field_name = fields[i];
            assert(field_name != NULL);

            field_value_str = strchr(field_name, '=');
            if (field_value_str == NULL)
                continue;
            *field_value_str = 0;
            size_t field_name_len = field_value_str - field_name;
            field_value_str++;

            sstrncpy(section + section_len, field_name, sizeof(section) - section_len);

            /* coverity[TAINTED_SCALAR] */
            const struct fscache_metric *fsm = fscache_get_key(section,
                                                               section_len + field_name_len);
            if (fsm != NULL) {
                uint64_t rvalue;
                status = strtouint(field_value_str, &rvalue);
                if (status != 0)
                    continue;

                metric_family_t *fam = &fams[fsm->fam];

                value_t value = {0};
                if (fam->type == METRIC_TYPE_COUNTER) {
                    value = VALUE_COUNTER(rvalue);
                } else if (fam->type == METRIC_TYPE_GAUGE) {
                    value = VALUE_GAUGE(rvalue);
                } else {
                    continue;
                }

                metric_family_append(fam, value, NULL, NULL);

            }
        }
    }
    fclose(fh);

    plugin_dispatch_metric_family_array(fams, FAM_FSCACHE_MAX, 0);
    return 0;
}

static int fscache_init(void)
{
    path_proc_fscache = plugin_procpath("fs/fscache/stats");
    if (path_proc_fscache == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int fscache_shutdown(void)
{
    free(path_proc_fscache);
    return 0;
}

void module_register(void)
{
    plugin_register_init("fscache", fscache_init);
    plugin_register_read("fscache", fscache_read);
    plugin_register_shutdown("fscache", fscache_shutdown);
}
