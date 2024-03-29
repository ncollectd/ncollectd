/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

enum {
    FAM_MEMCACHED_UP,
    FAM_MEMCACHED_UPTIME_SECONDS,
    FAM_MEMCACHED_VERSION,
    FAM_MEMCACHED_RUSAGE_USER_SECONDS,
    FAM_MEMCACHED_RUSAGE_SYSTEM_SECONDS,
    FAM_MEMCACHED_MAX_CONNECTIONS,
    FAM_MEMCACHED_CURRENT_CONNECTIONS,
    FAM_MEMCACHED_CONNECTIONS,
    FAM_MEMCACHED_REJECTED_CONNECTIONS,
    FAM_MEMCACHED_CONNECTION_STRUCTURES,
    FAM_MEMCACHED_RESPONSE_OBJECTS_OOM,
    FAM_MEMCACHED_RESPONSE_OBJECTS,
    FAM_MEMCACHED_RESPONSE_OBJECTS_BYTES,
    FAM_MEMCACHED_READ_BUFFERS,
    FAM_MEMCACHED_READ_BUFFERS_BYTES,
    FAM_MEMCACHED_READ_BUFFERS_FREE_BYTES,
    FAM_MEMCACHED_READ_BUFFERS_OOM,
    FAM_MEMCACHED_RESERVED_FDS,
    FAM_MEMCACHED_CMD_GET,
    FAM_MEMCACHED_CMD_SET,
    FAM_MEMCACHED_CMD_FLUSH,
    FAM_MEMCACHED_CMD_TOUCH,
    FAM_MEMCACHED_CMD_META,
    FAM_MEMCACHED_GET_HITS,
    FAM_MEMCACHED_GET_MISSES,
    FAM_MEMCACHED_GET_EXPIRE,
    FAM_MEMCACHED_GET_FLUSHED,
    FAM_MEMCACHED_GET_EXTSTORE,
    FAM_MEMCACHED_GET_ABORTED_EXTSTORE,
    FAM_MEMCACHED_GET_OOM_EXTSTORE,
    FAM_MEMCACHED_RECACHE_FROM_EXTSTORE,
    FAM_MEMCACHED_MISS_FROM_EXTSTORE,
    FAM_MEMCACHED_BADCRC_FROM_EXTSTORE,
    FAM_MEMCACHED_DELETE_MISSES,
    FAM_MEMCACHED_DELETE_HITS,
    FAM_MEMCACHED_INCR_MISSES,
    FAM_MEMCACHED_INCR_HITS,
    FAM_MEMCACHED_DECR_MISSES,
    FAM_MEMCACHED_DECR_HITS,
    FAM_MEMCACHED_CAS_MISSES,
    FAM_MEMCACHED_CAS_HITS,
    FAM_MEMCACHED_CAS_BADVAL,
    FAM_MEMCACHED_TOUCH_HITS,
    FAM_MEMCACHED_TOUCH_MISSES,
    FAM_MEMCACHED_AUTH_CMDS,
    FAM_MEMCACHED_AUTH_ERRORS,
    FAM_MEMCACHED_IDLE_KICKS,
    FAM_MEMCACHED_READ_BYTES,
    FAM_MEMCACHED_WRITTEN_BYTES,
    FAM_MEMCACHED_LIMIT_BYTES,
    FAM_MEMCACHED_ACCEPTING_CONNECTIONS,
    FAM_MEMCACHED_CONNECTIONS_LISTENER_DISABLED,
    FAM_MEMCACHED_TIME_LISTEN_DISABLED_SECONDS,
    FAM_MEMCACHED_THREADS,
    FAM_MEMCACHED_CONNECTIONS_YIELDED,
    FAM_MEMCACHED_HASH_POWER_LEVEL,
    FAM_MEMCACHED_HASH_BYTES,
    FAM_MEMCACHED_HASH_IS_EXPANDING,
    FAM_MEMCACHED_SLAB_REASSIGN_RESCUES,
    FAM_MEMCACHED_SLAB_REASSIGN_EVICTIONS_NOMEM,
    FAM_MEMCACHED_SLAB_REASSIGN_CHUNK_RESCUES,
    FAM_MEMCACHED_SLAB_REASSIGN_INLINE_RECLAIM,
    FAM_MEMCACHED_SLAB_REASSIGN_BUSY_ITEMS,
    FAM_MEMCACHED_SLAB_REASSIGN_BUSY_DELETES,
    FAM_MEMCACHED_SLAB_REASSIGN_RUNNING,
    FAM_MEMCACHED_SLABS_MOVED,
    FAM_MEMCACHED_LRU_CRAWLER_RUNNING,
    FAM_MEMCACHED_LRU_CRAWLER_STARTS,
    FAM_MEMCACHED_LRU_MAINTAINER_JUGGLES,
    FAM_MEMCACHED_MALLOC_FAILS,
    FAM_MEMCACHED_LOG_WORKER_DROPPED,
    FAM_MEMCACHED_LOG_WORKER_WRITTEN,
    FAM_MEMCACHED_LOG_WATCHER_SKIPPED,
    FAM_MEMCACHED_LOG_WATCHER_SENT,
    FAM_MEMCACHED_SSL_NEW_SESSIONS,
    FAM_MEMCACHED_SSL_HANDSHAKE_ERRORS,
    FAM_MEMCACHED_TIME_SINCE_SERVER_CERT_REFRESH_SECONDS,
    FAM_MEMCACHED_UNEXECTED_NAPI_IDS,
    FAM_MEMCACHED_ROUND_ROBIN_FALLBACK,
    FAM_MEMCACHED_EXPIRED_UNFETCHED,
    FAM_MEMCACHED_EVICTED_UNFETCHED,
    FAM_MEMCACHED_EVICTED_ACTIVE,
    FAM_MEMCACHED_EVICTIONS,
    FAM_MEMCACHED_RECLAIMED,
    FAM_MEMCACHED_CRAWLER_RECLAIMED,
    FAM_MEMCACHED_CRAWLER_ITEMS_CHECKED,
    FAM_MEMCACHED_LRUTAIL_REFLOCKED,
    FAM_MEMCACHED_MOVES_TO_COLD,
    FAM_MEMCACHED_MOVES_TO_WARM,
    FAM_MEMCACHED_MOVES_WITHIN_LRU,
    FAM_MEMCACHED_DIRECT_RECLAIMS,
    FAM_MEMCACHED_LRU_BUMPS_DROPPED,
    FAM_MEMCACHED_BYTES,
    FAM_MEMCACHED_CURRENT_ITEMS,
    FAM_MEMCACHED_TOTAL_ITEMS,
    FAM_MEMCACHED_SLAB_GLOBAL_PAGE_POOL,
    FAM_MEMCACHED_ITEMS,
    FAM_MEMCACHED_ITEMS_HOT,
    FAM_MEMCACHED_ITEMS_WARM,
    FAM_MEMCACHED_ITEMS_COLD,
    FAM_MEMCACHED_ITEMS_TEMP,
    FAM_MEMCACHED_ITEMS_AGE_HOT_SECONDS,
    FAM_MEMCACHED_ITEMS_AGE_WARM_SECONDS,
    FAM_MEMCACHED_ITEMS_AGE_SECONDS,
    FAM_MEMCACHED_ITEMS_REQUESTED_BYTES,
    FAM_MEMCACHED_ITEMS_EVICTED,
    FAM_MEMCACHED_ITEMS_EVICTED_NONZERO,
    FAM_MEMCACHED_ITEMS_EVICTED_SECONDS,
    FAM_MEMCACHED_ITEMS_OUTOFMEMORY,
    FAM_MEMCACHED_ITEMS_TAILREPAIRS,
    FAM_MEMCACHED_ITEMS_RECLAIMED,
    FAM_MEMCACHED_ITEMS_EXPIRED_UNFETCHED,
    FAM_MEMCACHED_ITEMS_EVICTED_UNFETCHED,
    FAM_MEMCACHED_ITEMS_EVICTED_ACTIVE,
    FAM_MEMCACHED_ITEMS_CRAWLER_RECLAIMED,
    FAM_MEMCACHED_ITEMS_CRAWLER_ITEMS_CHECKED,
    FAM_MEMCACHED_ITEMS_LRUTAIL_REFLOCKED,
    FAM_MEMCACHED_ITEMS_MOVES_TO_COLD,
    FAM_MEMCACHED_ITEMS_MOVES_TO_WARM,
    FAM_MEMCACHED_ITEMS_MOVES_WITHIN_LRU,
    FAM_MEMCACHED_ITEMS_DIRECT_RECLAIMS,
    FAM_MEMCACHED_ITEMS_HITS_TO_HOT,
    FAM_MEMCACHED_ITEMS_HITS_TO_WARM,
    FAM_MEMCACHED_ITEMS_HITS_TO_COLD,
    FAM_MEMCACHED_ITEMS_HITS_TO_TEMP,
    FAM_MEMCACHED_SLAB_CHUNK_BYTES,
    FAM_MEMCACHED_SLAB_CHUNKS_PER_PAGE,
    FAM_MEMCACHED_SLAB_TOTAL_PAGES,
    FAM_MEMCACHED_SLAB_TOTAL_CHUNKS,
    FAM_MEMCACHED_SLAB_USED_CHUNKS,
    FAM_MEMCACHED_SLAB_FREE_CHUNKS,
    FAM_MEMCACHED_SLAB_FREE_CHUNKS_END,
    FAM_MEMCACHED_SLAB_GET_HITS,
    FAM_MEMCACHED_SLAB_CMD_SET,
    FAM_MEMCACHED_SLAB_DELETE_HITS,
    FAM_MEMCACHED_SLAB_INCR_HITS,
    FAM_MEMCACHED_SLAB_DECR_HITS,
    FAM_MEMCACHED_SLAB_CAS_HITS,
    FAM_MEMCACHED_SLAB_CAS_BADVAL,
    FAM_MEMCACHED_SLAB_TOUCH_HITS,
    FAM_MEMCACHED_SLABS_ACTIVE,
    FAM_MEMCACHED_SLABS_ALLOC_BYTES,
    FAM_MEMCACHED_EXTSTORE_COMPACT_LOST,
    FAM_MEMCACHED_EXTSTORE_COMPACT_RESCUES,
    FAM_MEMCACHED_EXTSTORE_COMPACT_SKIPPED,
    FAM_MEMCACHED_EXTSTORE_PAGE_ALLOCATED,
    FAM_MEMCACHED_EXTSTORE_PAGE_EVICTIONS,
    FAM_MEMCACHED_EXTSTORE_PAGE_RECLAIMS,
    FAM_MEMCACHED_EXTSTORE_PAGES_FREE,
    FAM_MEMCACHED_EXTSTORE_PAGES_USED,
    FAM_MEMCACHED_EXTSTORE_OBJECTS_EVICTED,
    FAM_MEMCACHED_EXTSTORE_OBJECTS_READ,
    FAM_MEMCACHED_EXTSTORE_OBJECTS_WRITTEN,
    FAM_MEMCACHED_EXTSTORE_OBJECTS_USED,
    FAM_MEMCACHED_EXTSTORE_BYTES_EVICTED,
    FAM_MEMCACHED_EXTSTORE_BYTES_WRITTEN,
    FAM_MEMCACHED_EXTSTORE_BYTES_READ,
    FAM_MEMCACHED_EXTSTORE_BYTES_USED,
    FAM_MEMCACHED_EXTSTORE_BYTES_FRAGMENTED,
    FAM_MEMCACHED_EXTSTORE_LIMIT_BYTES,
    FAM_MEMCACHED_EXTSTORE_IO_QUEUE,
    FAM_MEMCACHED_MAX,
};

static metric_family_t fams[FAM_MEMCACHED_MAX] = {
    [FAM_MEMCACHED_UP] = {
        .name = "memcached_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the memcached server be reached",
    },
    [FAM_MEMCACHED_UPTIME_SECONDS] = {
        .name = "memcached_uptime_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of secs since the server started.",
    },
    [FAM_MEMCACHED_VERSION] = {
        .name = "memcached_version",
        .type = METRIC_TYPE_INFO,
        .help = "Version string of this server.",
    },
    [FAM_MEMCACHED_RUSAGE_USER_SECONDS] = {
        .name = "memcached_rusage_user_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Accumulated user time for this process.",
    },
    [FAM_MEMCACHED_RUSAGE_SYSTEM_SECONDS] = {
        .name = "memcached_rusage_system_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Accumulated system time for this process.",
    },
    [FAM_MEMCACHED_MAX_CONNECTIONS] = {
        .name = "memcached_max_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Max number of simultaneous connections.",
    },
    [FAM_MEMCACHED_CURRENT_CONNECTIONS] = {
        .name = "memcached_current_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of open connections.",
    },
    [FAM_MEMCACHED_CONNECTIONS] = {
        .name = "memcached_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of connections opened since the server started running.",
    },
    [FAM_MEMCACHED_REJECTED_CONNECTIONS] = {
        .name = "memcached_rejected_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Connections rejected in maxconns_fast mode.",
    },
    [FAM_MEMCACHED_CONNECTION_STRUCTURES] = {
        .name = "memcached_connection_structures",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of connection structures allocated by the server.",
    },
    [FAM_MEMCACHED_RESPONSE_OBJECTS_OOM] = {
        .name = "memcached_response_objects_oom",
        .type = METRIC_TYPE_COUNTER,
        .help = "Connections closed by lack of memory.",
    },
    [FAM_MEMCACHED_RESPONSE_OBJECTS] = {
        .name = "memcached_response_objects",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total response objects in use.",
    },
    [FAM_MEMCACHED_RESPONSE_OBJECTS_BYTES] = {
        .name = "memcached_response_objects_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes used for resp. objects. is a subset of bytes from read_buf_bytes.",
    },
    [FAM_MEMCACHED_READ_BUFFERS] = {
        .name = "memcached_read_buffers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total read/resp buffers allocated.",
    },
    [FAM_MEMCACHED_READ_BUFFERS_BYTES] = {
        .name = "memcached_read_buffers_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total read/resp buffer bytes allocated.",
    },
    [FAM_MEMCACHED_READ_BUFFERS_FREE_BYTES] = {
        .name = "memcached_read_buffers_free_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total read/resp buffer bytes cached.",
    },
    [FAM_MEMCACHED_READ_BUFFERS_OOM] = {
        .name = "memcached_read_buffers_oom",
        .type = METRIC_TYPE_COUNTER,
        .help = "Connections closed by lack of memory.",
    },
    [FAM_MEMCACHED_RESERVED_FDS] = {
        .name = "memcached_reserved_fds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of misc fds used internally.",
    },
    [FAM_MEMCACHED_CMD_GET] = {
        .name = "memcached_cmd_get",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cumulative number of retrieval reqs.",
    },
    [FAM_MEMCACHED_CMD_SET] = {
        .name = "memcached_cmd_set",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cumulative number of storage reqs.",
    },
    [FAM_MEMCACHED_CMD_FLUSH] = {
        .name = "memcached_cmd_flush",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cumulative number of flush reqs.",
    },
    [FAM_MEMCACHED_CMD_TOUCH] = {
        .name = "memcached_cmd_touch",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cumulative number of touch reqs.",
    },
    [FAM_MEMCACHED_CMD_META] = {
        .name = "memcached_cmd_meta",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cumulative number of meta reqs.",
    },
    [FAM_MEMCACHED_GET_HITS] = {
        .name = "memcached_get_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of keys that have been requested and found present.",
    },
    [FAM_MEMCACHED_GET_MISSES] = {
        .name = "memcached_get_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of items that have been requested and not found.",
    },
    [FAM_MEMCACHED_GET_EXPIRE] = {
        .name = "memcached_get_expire",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of items that have been requested but had already expired.",
    },
    [FAM_MEMCACHED_GET_FLUSHED] = {
        .name = "memcached_get_flushed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of items that have been requested but have been flushed via flush_all.",
    },
    [FAM_MEMCACHED_GET_EXTSTORE] = {
        .name = "memcached_get_extstore",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_MEMCACHED_GET_ABORTED_EXTSTORE] = {
        .name = "memcached_get_aborted_extstore",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_MEMCACHED_GET_OOM_EXTSTORE] = {
        .name = "memcached_get_oom_extstore",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_MEMCACHED_RECACHE_FROM_EXTSTORE] = {
        .name = "memcached_recache_from_extstore",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_MEMCACHED_MISS_FROM_EXTSTORE] = {
        .name = "memcached_miss_from_extstore",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_MEMCACHED_BADCRC_FROM_EXTSTORE] = {
        .name = "memcached_badcrc_from_extstore",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_MEMCACHED_DELETE_MISSES] = {
        .name = "memcached_delete_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of deletions reqs for missing keys.",
    },
    [FAM_MEMCACHED_DELETE_HITS] = {
        .name = "memcached_delete_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of deletion reqs resulting in an item being removed.",
    },
    [FAM_MEMCACHED_INCR_MISSES] = {
        .name = "memcached_incr_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of incr reqs against missing keys.",
    },
    [FAM_MEMCACHED_INCR_HITS] = {
        .name = "memcached_incr_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of successful incr reqs.",
    },
    [FAM_MEMCACHED_DECR_MISSES] = {
        .name = "memcached_decr_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of decr reqs against missing keys.",
    },
    [FAM_MEMCACHED_DECR_HITS] = {
        .name = "memcached_decr_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of successful decr reqs.",
    },
    [FAM_MEMCACHED_CAS_MISSES] = {
        .name = "memcached_cas_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of CAS reqs against missing keys.",
    },
    [FAM_MEMCACHED_CAS_HITS] = {
        .name = "memcached_cas_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of successful CAS reqs.",
    },
    [FAM_MEMCACHED_CAS_BADVAL] = {
        .name = "memcached_cas_badval",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of CAS reqs for which a key was found, but the CAS value did not match.",
    },
    [FAM_MEMCACHED_TOUCH_HITS] = {
        .name = "memcached_touch_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of keys that have been touched with a new expiration time.",
    },
    [FAM_MEMCACHED_TOUCH_MISSES] = {
        .name = "memcached_touch_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of items that have been touched and not found.",
    },
    [FAM_MEMCACHED_AUTH_CMDS] = {
        .name = "memcached_auth_cmds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of authentication commands handled, success or failure.",
    },
    [FAM_MEMCACHED_AUTH_ERRORS] = {
        .name = "memcached_auth_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of failed authentications.",
    },
    [FAM_MEMCACHED_IDLE_KICKS] = {
        .name = "memcached_idle_kicks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of connections closed due to reaching their idle timeout.",
    },
    [FAM_MEMCACHED_READ_BYTES] = {
        .name = "memcached_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes read by this server from network.",
    },
    [FAM_MEMCACHED_WRITTEN_BYTES] = {
        .name = "memcached_written_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes sent by this server to network.",
    },
    [FAM_MEMCACHED_LIMIT_BYTES] = {
        .name = "memcached_limit_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes this server is allowed to use for storage.",
    },
    [FAM_MEMCACHED_ACCEPTING_CONNECTIONS] = {
        .name = "memcached_accepting_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Whether or not server is accepting conns.",
    },
    [FAM_MEMCACHED_CONNECTIONS_LISTENER_DISABLED] = {
        .name = "memcached_connections_listener_disabled",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times that memcached has hit its connections limit "
                "and disabled its listener."
    },
    [FAM_MEMCACHED_TIME_LISTEN_DISABLED_SECONDS] = {
        .name = "memcached_time_listen_disabled_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of seconds in maxconns.",
    },
    [FAM_MEMCACHED_THREADS] = {
        .name = "memcached_threads",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of worker threads requested.",
    },
    [FAM_MEMCACHED_CONNECTIONS_YIELDED] = {
        .name = "memcached_connections_yielded",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times any connection yielded to another due to hitting the -R limit.",
    },
    [FAM_MEMCACHED_HASH_POWER_LEVEL] = {
        .name = "memcached_hash_power_level",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current size multiplier for hash table.",
    },
    [FAM_MEMCACHED_HASH_BYTES] = {
        .name = "memcached_hash_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Bytes currently used by hash tables.",
    },
    [FAM_MEMCACHED_HASH_IS_EXPANDING] = {
        .name = "memcached_hash_is_expanding",
        .type = METRIC_TYPE_GAUGE,
        .help = "Indicates if the hash table is being grown to a new size.",
    },
    [FAM_MEMCACHED_SLAB_REASSIGN_RESCUES] = {
        .name = "memcached_slab_reassign_rescues",
        .type = METRIC_TYPE_COUNTER,
        .help = "Items rescued from eviction in page move.",
    },
    [FAM_MEMCACHED_SLAB_REASSIGN_EVICTIONS_NOMEM] = {
        .name = "memcached_slab_reassign_evictions_nomem",
        .type = METRIC_TYPE_COUNTER,
        .help = "Valid items evicted during a page move (due to no free memory in slab).",
    },
    [FAM_MEMCACHED_SLAB_REASSIGN_CHUNK_RESCUES] = {
        .name = "memcached_slab_reassign_chunk_rescues",
        .type = METRIC_TYPE_COUNTER,
        .help = "Individual sections of an item rescued during a page move.",
    },
    [FAM_MEMCACHED_SLAB_REASSIGN_INLINE_RECLAIM] = {
        .name = "memcached_slab_reassign_inline_reclaim",
        .type = METRIC_TYPE_COUNTER,
        .help = "Internal stat counter for when the page mover clears memory "
                "from the chunk freelist when it wasn't expecting to.",
    },
    [FAM_MEMCACHED_SLAB_REASSIGN_BUSY_ITEMS] = {
        .name = "memcached_slab_reassign_busy_items",
        .type = METRIC_TYPE_COUNTER,
        .help = "Items busy during page move, requiring a retry before page can be moved.",
    },
    [FAM_MEMCACHED_SLAB_REASSIGN_BUSY_DELETES] = {
        .name = "memcached_slab_reassign_busy_deletes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Items busy during page move, requiring deletion before page can be moved.",
    },
    [FAM_MEMCACHED_SLAB_REASSIGN_RUNNING] = {
        .name = "memcached_slab_reassign_running",
        .type = METRIC_TYPE_GAUGE,
        .help = "If a slab page is being moved.",
    },
    [FAM_MEMCACHED_SLABS_MOVED] = {
        .name = "memcached_slabs_moved",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total slab pages moved.",
    },
    [FAM_MEMCACHED_LRU_CRAWLER_RUNNING] = {
        .name = "memcached_lru_crawler_running",
        .type = METRIC_TYPE_GAUGE,
        .help = "If crawl is in progress",
    },
    [FAM_MEMCACHED_LRU_CRAWLER_STARTS] = {
        .name = "memcached_lru_crawler_starts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Times an LRU crawler was started.",
    },
    [FAM_MEMCACHED_LRU_MAINTAINER_JUGGLES] = {
        .name = "memcached_lru_maintainer_juggles",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the LRU bg thread woke up.",
    },
    [FAM_MEMCACHED_MALLOC_FAILS] = {
        .name = "memcached_malloc_fails",
        .type = METRIC_TYPE_COUNTER,
        .help = "number of failed allocations."
    },
    [FAM_MEMCACHED_LOG_WORKER_DROPPED] = {
        .name = "memcached_log_worker_dropped",
        .type = METRIC_TYPE_COUNTER,
        .help = "Logs a worker never wrote due to full buf.",
    },
    [FAM_MEMCACHED_LOG_WORKER_WRITTEN] = {
        .name = "memcached_log_worker_written",
        .type = METRIC_TYPE_COUNTER,
        .help = "Logs written by a worker, to be picked up.",
    },
    [FAM_MEMCACHED_LOG_WATCHER_SKIPPED] = {
        .name = "memcached_log_watcher_skipped",
        .type = METRIC_TYPE_COUNTER,
        .help = "Logs not sent to slow watchers.",
    },
    [FAM_MEMCACHED_LOG_WATCHER_SENT] = {
        .name = "memcached_log_watcher_sent",
        .type = METRIC_TYPE_COUNTER,
        .help = "Logs written to watchers.",
    },
    [FAM_MEMCACHED_SSL_NEW_SESSIONS] = {
        .name = "memcached_ssl_new_sessions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Successfully negotiated new (non-reused) TLS sessions.",
    },
    [FAM_MEMCACHED_SSL_HANDSHAKE_ERRORS] = {
        .name = "memcached_ssl_handshake_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "TLS failures at accept/handshake time.",
    },
    [FAM_MEMCACHED_TIME_SINCE_SERVER_CERT_REFRESH_SECONDS] = {
        .name = "memcached_time_since_server_cert_refresh_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Time of the last server certificate refresh.",
    },
    [FAM_MEMCACHED_UNEXECTED_NAPI_IDS] = {
        .name = "memcached_unexected_napi_ids",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times an unexpected napi id is is received. See doc/napi_ids.txt.",
    },
    [FAM_MEMCACHED_ROUND_ROBIN_FALLBACK] = {
        .name = "memcached_round_robin_fallback",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times napi id of 0 is received resulting in fallback to "
                "round robin thread selection. See doc/napi_ids.txt.",
    },
    [FAM_MEMCACHED_EXPIRED_UNFETCHED] = {
        .name = "memcached_expired_unfetched",
        .type = METRIC_TYPE_COUNTER,
        .help = "Items pulled from LRU that were never touched by "
                "get/incr/append/etc before expiring.",
    },
    [FAM_MEMCACHED_EVICTED_UNFETCHED] = {
        .name = "memcached_evicted_unfetched",
        .type = METRIC_TYPE_COUNTER,
        .help = "Items evicted from LRU that were never touched by get/incr/append/etc.",
    },
    [FAM_MEMCACHED_EVICTED_ACTIVE] = {
        .name = "memcached_evicted_active",
        .type = METRIC_TYPE_COUNTER,
        .help = "Items evicted from LRU that had been hit recently but did not jump to top of LRU.",
    },
    [FAM_MEMCACHED_EVICTIONS] = {
        .name = "memcached_evictions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of valid items removed from cache to free memory for new items.",
    },
    [FAM_MEMCACHED_RECLAIMED] = {
        .name = "memcached_reclaimed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times an entry was stored using memory from an expired entry.",
    },
    [FAM_MEMCACHED_CRAWLER_RECLAIMED] = {
        .name = "memcached_crawler_reclaimed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total items freed by LRU Crawler.",
    },
    [FAM_MEMCACHED_CRAWLER_ITEMS_CHECKED] = {
        .name = "memcached_crawler_items_checked",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total items examined by LRU Crawler.",
    },
    [FAM_MEMCACHED_LRUTAIL_REFLOCKED] = {
        .name = "memcached_lrutail_reflocked",
        .type = METRIC_TYPE_COUNTER,
        .help = "Times LRU tail was found with active ref.",
    },
    [FAM_MEMCACHED_MOVES_TO_COLD] = {
        .name = "memcached_moves_to_cold",
        .type = METRIC_TYPE_COUNTER,
        .help = "Items moved from HOT/WARM to COLD LRU's.",
    },
    [FAM_MEMCACHED_MOVES_TO_WARM] = {
        .name = "memcached_moves_to_warm",
        .type = METRIC_TYPE_COUNTER,
        .help = "Items moved from COLD to WARM LRU.",
    },
    [FAM_MEMCACHED_MOVES_WITHIN_LRU] = {
        .name = "memcached_moves_within_lru",
        .type = METRIC_TYPE_COUNTER,
        .help = "Items reshuffled within HOT or WARM LRU's.",
    },
    [FAM_MEMCACHED_DIRECT_RECLAIMS] = {
        .name = "memcached_direct_reclaims",
        .type = METRIC_TYPE_COUNTER,
        .help = "Times worker threads had to directly reclaim or evict items.",
    },
    [FAM_MEMCACHED_LRU_BUMPS_DROPPED] = {
        .name = "memcached_lru_bumps_dropped",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_MEMCACHED_BYTES] = {
        .name = "memcached_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of bytes used to store items.",
    },
    [FAM_MEMCACHED_CURRENT_ITEMS] = {
        .name = "memcached_current_items",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of items stored.",
    },
    [FAM_MEMCACHED_TOTAL_ITEMS] = {
        .name = "memcached_total_items",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of items stored since the server started.",
    },
    [FAM_MEMCACHED_SLAB_GLOBAL_PAGE_POOL] = {
        .name = "memcached_slab_global_page_pool",
        .type = METRIC_TYPE_GAUGE,
        .help = "Slab pages returned to global pool for reassignment to other slab classes.",
    },
    [FAM_MEMCACHED_ITEMS] = {
        .name = "memcached_items",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of items presently stored in this class. "
                "Expired items are not automatically excluded.",
    },
    [FAM_MEMCACHED_ITEMS_HOT] = {
        .name = "memcached_items_hot",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of items presently stored in the HOT LRU.",
    },
    [FAM_MEMCACHED_ITEMS_WARM] = {
        .name = "memcached_items_warm",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of items presently stored in the WARM LRU.",
    },
    [FAM_MEMCACHED_ITEMS_COLD] = {
        .name = "memcached_items_cold",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of items presently stored in the COLD LRU.",
    },
    [FAM_MEMCACHED_ITEMS_TEMP] = {
        .name = "memcached_items_temp",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of items presently stored in the TEMPORARY LRU.",
    },
    [FAM_MEMCACHED_ITEMS_AGE_HOT_SECONDS] = {
        .name = "memcached_items_age_hot_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Age of the oldest item in HOT LRU.",
    },
    [FAM_MEMCACHED_ITEMS_AGE_WARM_SECONDS] = {
        .name = "memcached_items_age_warm_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Age of the oldest item in WARM LRU.",
    },
    [FAM_MEMCACHED_ITEMS_AGE_SECONDS] = {
        .name = "memcached_items_age_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Age of the oldest item in the LRU.",
    },
    [FAM_MEMCACHED_ITEMS_REQUESTED_BYTES] = {
        .name = "memcached_items_requested_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes requested to be stored in this LRU[*]",
    },
    [FAM_MEMCACHED_ITEMS_EVICTED] = {
        .name = "memcached_items_evicted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times an item had to be evicted from the LRU before it expired.",
    },
    [FAM_MEMCACHED_ITEMS_EVICTED_NONZERO] = {
        .name = "memcached_items_evicted_nonzero",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times an item which had an explicit expire time set "
                "had to be evicted from the LRU before it expired.",
    },
    [FAM_MEMCACHED_ITEMS_EVICTED_SECONDS] = {
        .name = "memcached_items_evicted_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Seconds since the last access for the most recent item evicted from this class. "
                "Use this to judge how recently active your evicted data is.",
    },
    [FAM_MEMCACHED_ITEMS_OUTOFMEMORY] = {
        .name = "memcached_items_outofmemory",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the underlying slab class was unable to store a new item. "
                "This means you are running with -M or an eviction failed.",
    },
    [FAM_MEMCACHED_ITEMS_TAILREPAIRS] = {
        .name = "memcached_items_tailrepairs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times we self-healed a slab with a refcount leak."
    },
    [FAM_MEMCACHED_ITEMS_RECLAIMED] = {
        .name = "memcached_items_reclaimed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times an entry was stored using memory from an expired entry.",
    },
    [FAM_MEMCACHED_ITEMS_EXPIRED_UNFETCHED] = {
        .name = "memcached_items_expired_unfetched",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of expired items reclaimed from the LRU "
                "which were never touched after being set.",
    },
    [FAM_MEMCACHED_ITEMS_EVICTED_UNFETCHED] = {
        .name = "memcached_items_evicted_unfetched",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of valid items evicted from the LRU "
                "which were never touched after being set.",
    },
    [FAM_MEMCACHED_ITEMS_EVICTED_ACTIVE] = {
        .name = "memcached_items_evicted_active",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of valid items evicted from the LRU which were recently touched "
                "but were evicted before being moved to the top of the LRU again.",
    },
    [FAM_MEMCACHED_ITEMS_CRAWLER_RECLAIMED] = {
        .name = "memcached_items_crawler_reclaimed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of items freed by the LRU Crawler.",
    },
    [FAM_MEMCACHED_ITEMS_CRAWLER_ITEMS_CHECKED] = {
        .name = "memcached_items_crawler_items_checked",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of items found to be refcount locked in the LRU tail.",
    },
    [FAM_MEMCACHED_ITEMS_LRUTAIL_REFLOCKED] = {
        .name = "memcached_items_lrutail_reflocked",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of items moved from HOT or WARM into COLD.",
    },
    [FAM_MEMCACHED_ITEMS_MOVES_TO_COLD] = {
        .name = "memcached_items_moves_to_cold",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of items moved from COLD to WARM.",
    },
    [FAM_MEMCACHED_ITEMS_MOVES_TO_WARM] = {
        .name = "memcached_items_moves_to_warm",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times active items were bumped within HOT or WARM.",
    },
    [FAM_MEMCACHED_ITEMS_MOVES_WITHIN_LRU] = {
        .name = "memcached_items_moves_within_lru",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times worker threads had to directly pull LRU tails "
                "to find memory for a new item.",
    },
    [FAM_MEMCACHED_ITEMS_DIRECT_RECLAIMS] = {
        .name = "memcached_items_direct_reclaims",
        .type = METRIC_TYPE_COUNTER,
        .help = "Times worker threads had to directly reclaim or evict items.",
    },
    [FAM_MEMCACHED_ITEMS_HITS_TO_HOT] = {
        .name = "memcached_items_hits_to_hot",
        .help = "Number of get_hits to  the HOT LRU.",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_MEMCACHED_ITEMS_HITS_TO_WARM] = {
        .name = "memcached_items_hits_to_warm",
        .help = "Number of get_hits to  the WARM LRU.",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_MEMCACHED_ITEMS_HITS_TO_COLD] = {
        .name = "memcached_items_hits_to_cold",
        .help = "Number of get_hits to  the COLD LRU.",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_MEMCACHED_ITEMS_HITS_TO_TEMP] = {
        .name = "memcached_items_hits_to_temp",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of get_hits to  the TEMPORARY LRU.",
    },
    [FAM_MEMCACHED_SLAB_CHUNK_BYTES] = {
        .name = "memcached_slab_chunk_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The amount of space each chunk uses. One item will use one chunk "
                "of the appropriate size.",
    },
    [FAM_MEMCACHED_SLAB_CHUNKS_PER_PAGE] = {
        .name = "memcached_slab_chunks_per_page",
        .type = METRIC_TYPE_GAUGE,
        .help = "How many chunks exist within one page. "
                "A page by default is less than or equal to one megabyte in size. "
                "Slabs are allocated by page, then broken into chunks.",
    },
    [FAM_MEMCACHED_SLAB_TOTAL_PAGES] = {
        .name = "memcached_slab_total_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of pages allocated to the slab class.",
    },
    [FAM_MEMCACHED_SLAB_TOTAL_CHUNKS] = {
        .name = "memcached_slab_total_chunks",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of chunks allocated to the slab class.",
    },
    [FAM_MEMCACHED_SLAB_USED_CHUNKS] = {
        .name = "memcached_slab_used_chunks",
        .type = METRIC_TYPE_GAUGE,
        .help = "How many chunks have been allocated to items.",
    },
    [FAM_MEMCACHED_SLAB_FREE_CHUNKS] = {
        .name = "memcached_slab_free_chunks",
        .type = METRIC_TYPE_GAUGE,
        .help = "Chunks not yet allocated to items, or freed via delete.",
    },
    [FAM_MEMCACHED_SLAB_FREE_CHUNKS_END] = {
        .name = "memcached_slab_free_chunks_end",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of free chunks at the end of the last allocated page.",
    },
    [FAM_MEMCACHED_SLAB_GET_HITS] = {
        .name = "memcached_slab_get_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of get requests serviced by this class.",
    },
    [FAM_MEMCACHED_SLAB_CMD_SET] = {
        .name = "memcached_slab_cmd_set",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of set requests storing data in this class.",
    },
    [FAM_MEMCACHED_SLAB_DELETE_HITS] = {
        .name = "memcached_slab_delete_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of successful deletes from this class.",
    },
    [FAM_MEMCACHED_SLAB_INCR_HITS] = {
        .name = "memcached_slab_incr_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of incrs modifying this class.",
    },
    [FAM_MEMCACHED_SLAB_DECR_HITS] = {
        .name = "memcached_slab_decr_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of decrs modifying this class.",
    },
    [FAM_MEMCACHED_SLAB_CAS_HITS] = {
        .name = "memcached_slab_cas_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of CAS commands modifying this class.",
    },
    [FAM_MEMCACHED_SLAB_CAS_BADVAL] = {
        .name = "memcached_slab_cas_badval",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of CAS commands that failed to modify a value due to a bad CAS id.",
    },
    [FAM_MEMCACHED_SLAB_TOUCH_HITS] = {
        .name = "memcached_slab_touch_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of touches serviced by this class.",
    },
    [FAM_MEMCACHED_SLABS_ACTIVE] = {
        .name = "memcached_slabs_active",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of slab classes allocated.",
    },
    [FAM_MEMCACHED_SLABS_ALLOC_BYTES] = {
        .name = "memcached_slabs_alloc_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total amount of memory allocated to slab pages.",
    },
    [FAM_MEMCACHED_EXTSTORE_COMPACT_LOST] = {
        .name = "memcached_extstore_compact_lost",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of items lost because they were locked during extstore compaction.",
    },
    [FAM_MEMCACHED_EXTSTORE_COMPACT_RESCUES] = {
        .name = "memcached_extstore_compact_rescues",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of items moved to a new page during extstore compaction,",
    },
    [FAM_MEMCACHED_EXTSTORE_COMPACT_SKIPPED] = {
        .name = "memcached_extstore_compact_skipped",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of items dropped due to inactivity during extstore compaction.",
    },
    [FAM_MEMCACHED_EXTSTORE_PAGE_ALLOCATED] = {
        .name = "memcached_extstore_page_allocated",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times a page was allocated in extstore.",
    },
    [FAM_MEMCACHED_EXTSTORE_PAGE_EVICTIONS] = {
        .name = "memcached_extstore_page_evictions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times a page was evicted from extstore.",
    },
    [FAM_MEMCACHED_EXTSTORE_PAGE_RECLAIMS] = {
        .name = "memcached_extstore_page_reclaims",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of times an empty extstore page was freed.",
    },
    [FAM_MEMCACHED_EXTSTORE_PAGES_FREE] = {
        .name = "memcached_extstore_pages_free",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of extstore pages not yet containing any items.",
    },
    [FAM_MEMCACHED_EXTSTORE_PAGES_USED] = {
        .name = "memcached_extstore_pages_used",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of extstore pages containing at least one item.",
    },
    [FAM_MEMCACHED_EXTSTORE_OBJECTS_EVICTED] = {
        .name = "memcached_extstore_objects_evicted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of items evicted from extstore to free up space.",
    },
    [FAM_MEMCACHED_EXTSTORE_OBJECTS_READ] = {
        .name = "memcached_extstore_objects_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of items read from extstore.",
    },
    [FAM_MEMCACHED_EXTSTORE_OBJECTS_WRITTEN] = {
        .name = "memcached_extstore_objects_written",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of items written to extstore.",
    },
    [FAM_MEMCACHED_EXTSTORE_OBJECTS_USED] = {
        .name = "memcached_extstore_objects_used",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of items stored in extstore.",
    },
    [FAM_MEMCACHED_EXTSTORE_BYTES_EVICTED] = {
        .name = "memcached_extstore_bytes_evicted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes evicted from extstore to free up space.",
    },
    [FAM_MEMCACHED_EXTSTORE_BYTES_WRITTEN] = {
        .name = "memcached_extstore_bytes_written",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes written to extstore.",
    },
    [FAM_MEMCACHED_EXTSTORE_BYTES_READ] = {
        .name = "memcached_extstore_bytes_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes read from extstore.",
    },
    [FAM_MEMCACHED_EXTSTORE_BYTES_USED] = {
        .name = "memcached_extstore_bytes_used",
        .type = METRIC_TYPE_COUNTER,
        .help = "Current number of bytes used to store items in extstore.",
    },
    [FAM_MEMCACHED_EXTSTORE_BYTES_FRAGMENTED] = {
        .name = "memcached_extstore_bytes_fragmented",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of bytes in extstore pages allocated "
                "but not used to store an object.",
    },
    [FAM_MEMCACHED_EXTSTORE_LIMIT_BYTES] = {
        .name = "memcached_extstore_limit_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes of external storage allocated for this server.",
    },
    [FAM_MEMCACHED_EXTSTORE_IO_QUEUE] = {
        .name = "memcached_extstore_io_queue",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size of io queue",
    },
};
