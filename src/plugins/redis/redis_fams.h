/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

enum {
    FAM_REDIS_UP,
    FAM_REDIS_VERSION_INFO,
    FAM_REDIS_MODE,
    FAM_REDIS_UPTIME_SECONDS,
    FAM_REDIS_CLIENTS_CONNECTED,
    FAM_REDIS_CLUSTER_CONNECTIONS,
    FAM_REDIS_MAX_CLIENTS,
    FAM_REDIS_CLIENT_LONGEST_OUTPUT_LIST,
    FAM_REDIS_CLIENT_BIGGEST_INPUT_BUF,
    FAM_REDIS_CLIENT_RECENT_MAX_OUTPUT_BUFFER_BYTES,
    FAM_REDIS_CLIENT_RECENT_MAX_INPUT_BUFFER_BYTES,
    FAM_REDIS_CLIENTS_BLOCKED,
    FAM_REDIS_CLIENTS_TRACKING,
    FAM_REDIS_CLIENTS_IN_TIMEOUT_TABLE,
    FAM_REDIS_IO_THREADS_ACTIVE,
    FAM_REDIS_MEMORY_USED_BYTES,
    FAM_REDIS_MEMORY_USED_RSS_BYTES,
    FAM_REDIS_MEMORY_USED_PEAK_BYTES,
    FAM_REDIS_MEMORY_USED_PEAK_RATIO,
    FAM_REDIS_MEMORY_USED_OVERHEAD_BYTES,
    FAM_REDIS_MEMORY_USED_STARTUP_BYTES,
    FAM_REDIS_MEMORY_USED_DATASET_BYTES,
    FAM_REDIS_MEMORY_USED_DATASET_RATIO,
    FAM_REDIS_MEMORY_USED_LUA_BYTES,
    FAM_REDIS_MEMORY_USED_SCRIPTS_BYTES,
    FAM_REDIS_MEMORY_MAX_BYTES,
    FAM_REDIS_NUMBER_OF_CACHED_SCRIPTS,
    FAM_REDIS_ALLOCATOR_ALLOCATED_BYTES,
    FAM_REDIS_ALLOCATOR_ACTIVE_BYTES,
    FAM_REDIS_ALLOCATOR_RESIDENT_BYTES,
    FAM_REDIS_ALLOCATOR_FRAG_RATIO,
    FAM_REDIS_ALLOCATOR_FRAG_BYTES,
    FAM_REDIS_ALLOCATOR_RSS_RATIO,
    FAM_REDIS_ALLOCATOR_RSS_BYTES,
    FAM_REDIS_RSS_OVERHEAD_RATIO,
    FAM_REDIS_RSS_OVERHEAD_BYTES,
    FAM_REDIS_MEMORY_FRAGMENTATION_RATIO,
    FAM_REDIS_MEMORY_FRAGMENTATION_BYTES,
    FAM_REDIS_MEMORY_NOT_COUNTED_FOR_EVICT_BYTES,
    FAM_REDIS_MEMORY_REPLICATION_BACKLOG_BYTES,
    FAM_REDIS_MEMORY_CLIENTS_SLAVES_BYTES,
    FAM_REDIS_MEMORY_CLIENTS_NORMAL_BYTES,
    FAM_REDIS_MEMORY_AOF_BUFFER_BYTES,
    FAM_REDIS_ACTIVE_DEFRAG_RUNNING,
    FAM_REDIS_LAZYFREE_PENDING_OBJECTS,
    FAM_REDIS_LAZYFREED_OBJECTS,
    FAM_REDIS_LOADING_DUMP_FILE,
    FAM_REDIS_RDB_CHANGES_SINCE_LAST_SAVE,
    FAM_REDIS_RDB_BGSAVE_IN_PROGRESS,
    FAM_REDIS_RDB_LAST_SAVE_TIME,
    FAM_REDIS_RDB_LAST_BGSAVE_STATUS,
    FAM_REDIS_RDB_LAST_BGSAVE_DURATION_SECONDS,
    FAM_REDIS_RDB_CURRENT_BGSAVE_DURATION_SECONDS,
    FAM_REDIS_RDB_LAST_COW_SIZE_BYTES,
    FAM_REDIS_AOF_ENABLED,
    FAM_REDIS_AOF_REWRITE_IN_PROGRESS,
    FAM_REDIS_AOF_REWRITE_SCHEDULED,
    FAM_REDIS_AOF_LAST_REWRITE_DURATION_SECONDS,
    FAM_REDIS_AOF_CURRENT_REWRITE_DURATION_SECONDS,
    FAM_REDIS_AOF_LAST_BGREWRITE_STATUS,
    FAM_REDIS_AOF_LAST_WRITE_STATUS,
    FAM_REDIS_AOF_LAST_COW_SIZE_BYTES,
    FAM_REDIS_AOF_CURRENT_SIZE_BYTES,
    FAM_REDIS_AOF_BASE_SIZE_BYTES,
    FAM_REDIS_AOF_PENDING_REWRITE,
    FAM_REDIS_AOF_BUFFER_LENGTH_BYTES,
    FAM_REDIS_AOF_REWRITE_BUFFER_LENGTH_BYTES,
    FAM_REDIS_AOF_PENDING_BIO_FSYNC,
    FAM_REDIS_AOF_DELAYED_FSYNC,
    FAM_REDIS_MODULE_FORK_IN_PROGRESS,
    FAM_REDIS_MODULE_FORK_LAST_COW_SIZE_BYTES,
    FAM_REDIS_CONNECTIONS_RECEIVED,
    FAM_REDIS_COMMANDS_PROCESSED,
    FAM_REDIS_INPUT_BYTES,
    FAM_REDIS_OUTPUT_BYTES,
    FAM_REDIS_REJECTED_CONNECTIONS,
    FAM_REDIS_REPLICA_RESYNCS_FULL,
    FAM_REDIS_REPLICA_PARTIAL_RESYNC_ACCEPTED,
    FAM_REDIS_REPLICA_PARTIAL_RESYNC_DENIED,
    FAM_REDIS_EXPIRED_KEYS,
    FAM_REDIS_EXPIRED_STALE_RATIO,
    FAM_REDIS_EXPIRED_TIME_CAP_REACHED,
    FAM_REDIS_EXPIRE_CYCLE_CPU_MSECONDS,
    FAM_REDIS_EVICTED_KEYS,
    FAM_REDIS_KEYSPACE_HITS,
    FAM_REDIS_KEYSPACE_MISSES,
    FAM_REDIS_PUBSUB_CHANNELS,
    FAM_REDIS_PUBSUB_PATTERNS,
    FAM_REDIS_LATEST_FORK_USECECONDS,
    FAM_REDIS_FORKS,
    FAM_REDIS_MIGRATE_CACHED_SOCKETS,
    FAM_REDIS_SLAVE_EXPIRES_TRACKED_KEYS,
    FAM_REDIS_ACTIVE_DEFRAG_HITS,
    FAM_REDIS_ACTIVE_DEFRAG_MISSES,
    FAM_REDIS_ACTIVE_DEFRAG_KEY_HITS,
    FAM_REDIS_ACTIVE_DEFRAG_KEY_MISSES,
    FAM_REDIS_TRACKING_KEYS,
    FAM_REDIS_TRACKING_ITEMS,
    FAM_REDIS_TRACKING_PREFIXES,
    FAM_REDIS_UNEXPECTED_ERROR_REPLIES,
    FAM_REDIS_ERROR_REPLIES,
    FAM_REDIS_READS_PROCESSED,
    FAM_REDIS_WRITES_PROCESSED,
    FAM_REDIS_IO_THREADED_READS_PROCESSED,
    FAM_REDIS_IO_THREADED_WRITES_PROCESSED,
    FAM_REDIS_CPU_SYS_SECONDS,
    FAM_REDIS_CPU_USER_SECONDS,
    FAM_REDIS_CPU_SYS_CHILDREN_SECONDS,
    FAM_REDIS_CPU_USER_CHILDREN_SECONDS,
    FAM_REDIS_CPU_SYS_MAIN_THREAD_SECONDS,
    FAM_REDIS_CPU_USER_MAIN_THREAD_SECONDS,
    FAM_REDIS_CLUSTER_ENABLED,
    FAM_REDIS_CONNECTED_SLAVES,
    FAM_REDIS_GOOD_SLAVES,
    FAM_REDIS_ROLE,
    FAM_REDIS_MASTER_FAILOVER_STATE,
    FAM_REDIS_MASTER_REPL_OFFSET,
    FAM_REDIS_SECOND_REPL_OFFSET,
    FAM_REDIS_REPLICATION_BACKLOG_ACTIVE,
    FAM_REDIS_REPLICATION_BACKLOG_SIZE_BYTE,
    FAM_REDIS_REPLICATION_BACKLOG_FIRST_BYTE_OFFSET,
    FAM_REDIS_REPLICATION_BACKLOG_HIST_BYTES,
    FAM_REDIS_MASTER_LINK_UP,
    FAM_REDIS_MASTER_LAST_IO_SECONDS,
    FAM_REDIS_MASTER_SYNC_IN_PROGRESS,
    FAM_REDIS_SLAVE_REPLICATION_OFFSET,
    FAM_REDIS_SLAVE_PRIORITY,
    FAM_REDIS_SLAVE_READ_ONLY,

    FAM_REDIS_COMMANDS_DURATION_SECONDS,
    FAM_REDIS_COMMANDS,
    FAM_REDIS_COMMANDS_REJECTED,
    FAM_REDIS_COMMANDS_FAILED,
    FAM_REDIS_DB_KEYS,
    FAM_REDIS_DB_KEYS_EXPIRING,
    FAM_REDIS_ERRORS,
    FAM_REDIS_SLAVE_STATE,
    FAM_REDIS_SLAVE_LAG,
    FAM_REDIS_SLAVE_OFFSET,

    FAM_REDIS_SENTINEL_MASTERS,
    FAM_REDIS_SENTINEL_TILT,
    FAM_REDIS_SENTINEL_RUNNING_SCRIPTS,
    FAM_REDIS_SENTINEL_SCRIPTS_QUEUE_LENGTH,
    FAM_REDIS_SENTINEL_SIMULATE_FAILURE_FLAGS,
    FAM_REDIS_SENTINEL_MASTER_STATUS,
    FAM_REDIS_SENTINEL_MASTER_SLAVES,
    FAM_REDIS_SENTINEL_MASTER_SENTINELS,

    FAM_REDIS_MAX,
};

static metric_family_t fams_redis[FAM_REDIS_MAX] = {
    [FAM_REDIS_UP] = {
        .name = "redis_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the redis server be reached.",
    },
    [FAM_REDIS_VERSION_INFO] = {
        .name = "redis_version_info",
        .type = METRIC_TYPE_INFO,
        .help = "Version of the Redis server.",
    },
    [FAM_REDIS_MODE] = {
        .name = "redis_mode",
        .type = METRIC_TYPE_STATE_SET,
        .help = "The server's mode.",
    },
    [FAM_REDIS_UPTIME_SECONDS] = {
        .name = "redis_uptime_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of seconds since Redis server start.",
    },
    [FAM_REDIS_CLIENTS_CONNECTED] = {
        .name = "redis_clients_connected",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of client connections (excluding connections from replicas).",
    },
    [FAM_REDIS_CLUSTER_CONNECTIONS] = {
        .name = "redis_cluster_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "An approximation of the number of sockets used by the cluster's bus.",
    },
    [FAM_REDIS_MAX_CLIENTS] = {
        .name = "redis_max_clients",
        .type = METRIC_TYPE_GAUGE,
        .help = "The value of the maxclients configuration directive.",
    },
    [FAM_REDIS_CLIENT_LONGEST_OUTPUT_LIST] = {
        .name = "redis_client_longest_output_list",
        .type = METRIC_TYPE_GAUGE,
        .help = "Longest output list among current client connections.",
    },
    [FAM_REDIS_CLIENT_BIGGEST_INPUT_BUF] = {
        .name = "redis_client_biggest_input_buf",
        .type = METRIC_TYPE_GAUGE,
        .help = "Biggest input buffer among current client connections.",
    },
    [FAM_REDIS_CLIENT_RECENT_MAX_OUTPUT_BUFFER_BYTES] = {
        .name = "redis_client_recent_max_output_buffer_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Biggest output buffer among current client connections.",
    },
    [FAM_REDIS_CLIENT_RECENT_MAX_INPUT_BUFFER_BYTES] = {
        .name = "redis_client_recent_max_input_buffer_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Biggest input buffer among current client connections.",
    },
    [FAM_REDIS_CLIENTS_BLOCKED] = {
        .name = "redis_clients_blocked",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of clients pending on a blocking call "
                "(BLPOP, BRPOPLPUSH, BLMOVE, BZPOPMIN, BZPOPMAX).",
    },
    [FAM_REDIS_CLIENTS_TRACKING] = {
        .name = "redis_clients_tracking",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of clients being tracked (CLIENT TRACKING).",
    },
    [FAM_REDIS_CLIENTS_IN_TIMEOUT_TABLE] = {
        .name = "redis_clients_in_timeout_table",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of clients in the clients timeout table.",
    },
    [FAM_REDIS_IO_THREADS_ACTIVE] = {
        .name = "redis_io_threads_active",
        .type = METRIC_TYPE_GAUGE,
        .help = "Flag indicating if I/O threads are active.",
    },
    [FAM_REDIS_MEMORY_USED_BYTES] = {
        .name = "redis_memory_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of bytes allocated by Redis using its allocator.",
    },
    [FAM_REDIS_MEMORY_USED_RSS_BYTES] = {
        .name = "redis_memory_used_rss_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes that Redis allocated as seen by the operating system "
                "(a.k.a resident set size).",
    },
    [FAM_REDIS_MEMORY_USED_PEAK_BYTES] = {
        .name = "redis_memory_used_peak_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Peak memory consumed by Redis (in bytes).",
    },
    [FAM_REDIS_MEMORY_USED_PEAK_RATIO] = {
        .name = "redis_memory_used_peak_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "The percentage of used_memory_peak out of used_memory.",
    },
    [FAM_REDIS_MEMORY_USED_OVERHEAD_BYTES] = {
        .name = "redis_memory_used_overhead_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The sum in bytes of all overheads that the server allocated "
                "for managing its internal data structures.",
    },
    [FAM_REDIS_MEMORY_USED_STARTUP_BYTES] = {
        .name = "redis_memory_used_startup_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Initial amount of memory consumed by Redis at startup in bytes.",
    },
    [FAM_REDIS_MEMORY_USED_DATASET_BYTES] = {
        .name = "redis_memory_used_dataset_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The size in bytes of the dataset (used_memory_overhead subtracted "
                "from used_memory).",
    },
    [FAM_REDIS_MEMORY_USED_DATASET_RATIO] = {
        .name = "redis_memory_used_dataset_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "The percentage of used_memory_dataset out of the net memory usage "
                "(used_memory minus used_memory_startup).",
    },
    [FAM_REDIS_MEMORY_USED_LUA_BYTES] = {
        .name = "redis_memory_used_lua_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes used by the Lua engine.",
    },
    [FAM_REDIS_MEMORY_USED_SCRIPTS_BYTES] = {
        .name = "redis_memory_used_scripts_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes used by cached Lua scripts.",
    },
    [FAM_REDIS_MEMORY_MAX_BYTES] = {
        .name = "redis_memory_max_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The value of the maxmemory configuration directive.",
    },
    [FAM_REDIS_NUMBER_OF_CACHED_SCRIPTS] = {
        .name = "redis_number_of_cached_scripts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of cached scripts.",
    },
    [FAM_REDIS_ALLOCATOR_ALLOCATED_BYTES] = {
        .name = "redis_allocator_allocated_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Redis memory allocator stats.",
    },
    [FAM_REDIS_ALLOCATOR_ACTIVE_BYTES] = {
        .name = "redis_allocator_active_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Redis memory allocator stats.",
    },
    [FAM_REDIS_ALLOCATOR_RESIDENT_BYTES] = {
        .name = "redis_allocator_resident_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Redis memory allocator stats.",
    },
    [FAM_REDIS_ALLOCATOR_FRAG_RATIO] = {
        .name = "redis_allocator_frag_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Redis memory allocator stats.",
    },
    [FAM_REDIS_ALLOCATOR_FRAG_BYTES] = {
        .name = "redis_allocator_frag_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Redis memory allocator stats.",
    },
    [FAM_REDIS_ALLOCATOR_RSS_RATIO] = {
        .name = "redis_allocator_rss_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Redis memory allocator stats.",
    },
    [FAM_REDIS_ALLOCATOR_RSS_BYTES] = {
        .name = "redis_allocator_rss_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Redis memory allocator stats.",
    },
    [FAM_REDIS_RSS_OVERHEAD_RATIO] = {
        .name = "redis_rss_overhead_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total RSS overhead ratio, includinig fragmentation, but not just it.",
    },
    [FAM_REDIS_RSS_OVERHEAD_BYTES] = {
        .name = "redis_rss_overhead_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total RSS overhead, includinig fragmentation, but not just it.",
    },
    [FAM_REDIS_MEMORY_FRAGMENTATION_RATIO] = {
        .name = "redis_memory_fragmentation_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Ratio between used_memory_rss and used_memory.",
    },
    [FAM_REDIS_MEMORY_FRAGMENTATION_BYTES] = {
        .name = "redis_memory_fragmentation_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Diference between used_memory_rss and used_memory.",
    },
    [FAM_REDIS_MEMORY_NOT_COUNTED_FOR_EVICT_BYTES] = {
        .name = "redis_memory_not_counted_for_evict_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The sum of AOF buffers and slaves output buffers.",
    },
    [FAM_REDIS_MEMORY_REPLICATION_BACKLOG_BYTES] = {
        .name = "redis_memory_replication_backlog_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Replication backlog memory size.",
    },
    [FAM_REDIS_MEMORY_CLIENTS_SLAVES_BYTES] = {
        .name = "redis_memory_clients_slaves_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The memory used by the slave clients.",
    },
    [FAM_REDIS_MEMORY_CLIENTS_NORMAL_BYTES] = {
        .name = "redis_memory_clients_normal_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The memory used by the normal clients.",
    },
    [FAM_REDIS_MEMORY_AOF_BUFFER_BYTES] = {
        .name = "redis_memory_aof_buffer_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "AOF buffer size",
    },
    [FAM_REDIS_ACTIVE_DEFRAG_RUNNING] = {
        .name = "redis_active_defrag_running",
        .type = METRIC_TYPE_GAUGE,
        .help = "Flag indicating if active defragmentation is active.",
    },
    [FAM_REDIS_LAZYFREE_PENDING_OBJECTS] = {
        .name = "redis_lazyfree_pending_objects",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of objects waiting to be freed (as a result "
                "of calling UNLINK, or FLUSHDB and FLUSHALL with the ASYNC option).",
    },
    [FAM_REDIS_LAZYFREED_OBJECTS] = {
        .name = "redis_lazyfreed_objects",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of objects that have been freed (as a result "
                "of calling UNLINK,  or FLUSHDB and FLUSHALL with the ASYNC option).",
    },
    [FAM_REDIS_LOADING_DUMP_FILE] = {
        .name = "redis_loading_dump_file",
        .type = METRIC_TYPE_GAUGE,
        .help = "Flag indicating if the load of a dump file is on-going.",
    },
    [FAM_REDIS_RDB_CHANGES_SINCE_LAST_SAVE] = {
        .name = "redis_rdb_changes_since_last_save",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of changes since the last dump.",
    },
    [FAM_REDIS_RDB_BGSAVE_IN_PROGRESS] = {
        .name = "redis_rdb_bgsave_in_progress",
        .type = METRIC_TYPE_GAUGE,
        .help = "Flag indicating a RDB save is on-going.",
    },
    [FAM_REDIS_RDB_LAST_SAVE_TIME] = {
        .name = "redis_rdb_last_save_time",
        .type = METRIC_TYPE_GAUGE,
        .help = "Epoch-based timestamp of last successful RDB save.",
    },
    [FAM_REDIS_RDB_LAST_BGSAVE_STATUS] = {
        .name = "redis_rdb_last_bgsave_status",
        .type = METRIC_TYPE_GAUGE,
        .help = "Status of the last RDB save operation.",
    },
    [FAM_REDIS_RDB_LAST_BGSAVE_DURATION_SECONDS] = {
        .name = "redis_rdb_last_bgsave_duration_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Duration of the last RDB save operation in seconds.",
    },
    [FAM_REDIS_RDB_CURRENT_BGSAVE_DURATION_SECONDS] = {
        .name = "redis_rdb_current_bgsave_duration_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Duration of the on-going RDB save operation if any.",
    },
    [FAM_REDIS_RDB_LAST_COW_SIZE_BYTES] = {
        .name = "redis_rdb_last_cow_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The size in bytes of copy-on-write memory during the last RDB save operation.",
    },
    [FAM_REDIS_AOF_ENABLED] = {
        .name = "redis_aof_enabled",
        .type = METRIC_TYPE_GAUGE,
        .help = "Flag indicating AOF logging is activated.",
    },
    [FAM_REDIS_AOF_REWRITE_IN_PROGRESS] = {
        .name = "redis_aof_rewrite_in_progress",
        .type = METRIC_TYPE_GAUGE,
        .help = "Flag indicating a AOF rewrite operation is on-going.",
    },
    [FAM_REDIS_AOF_REWRITE_SCHEDULED] = {
        .name = "redis_aof_rewrite_scheduled",
        .type = METRIC_TYPE_GAUGE,
        .help = "Flag indicating an AOF rewrite operation will be scheduled "
                "once the on-going RDB save is complete.",
    },
    [FAM_REDIS_AOF_LAST_REWRITE_DURATION_SECONDS] = {
        .name = "redis_aof_last_rewrite_duration_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Duration of the last AOF rewrite operation in seconds.",
    },
    [FAM_REDIS_AOF_CURRENT_REWRITE_DURATION_SECONDS] = {
        .name = "redis_aof_current_rewrite_duration_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Duration of the on-going AOF rewrite operation if any.",
    },
    [FAM_REDIS_AOF_LAST_BGREWRITE_STATUS] = {
        .name = "redis_aof_last_bgrewrite_status",
        .type = METRIC_TYPE_GAUGE,
        .help = "Status of the last AOF rewrite operation.",
    },
    [FAM_REDIS_AOF_LAST_WRITE_STATUS] = {
        .name = "redis_aof_last_write_status",
        .type = METRIC_TYPE_GAUGE,
        .help = "Status of the last write operation to the AOF.",
    },
    [FAM_REDIS_AOF_LAST_COW_SIZE_BYTES] = {
        .name = "redis_aof_last_cow_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The size in bytes of copy-on-write memory during the last AOF rewrite operation.",
    },
    [FAM_REDIS_AOF_CURRENT_SIZE_BYTES] = {
        .name = "redis_aof_current_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "AOF current file size.",
    },
    [FAM_REDIS_AOF_BASE_SIZE_BYTES] = {
        .name = "redis_aof_base_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "AOF file size on latest startup or rewrite.",
    },
    [FAM_REDIS_AOF_PENDING_REWRITE] = {
        .name = "redis_aof_pending_rewrite",
        .type = METRIC_TYPE_GAUGE,
        .help = "Flag indicating an AOF rewrite operation will be scheduled once "
                "the on-going RDB save is complete.",
    },
    [FAM_REDIS_AOF_BUFFER_LENGTH_BYTES] = {
        .name = "redis_aof_buffer_length_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size of the AOF buffer.",
    },
    [FAM_REDIS_AOF_REWRITE_BUFFER_LENGTH_BYTES] = {
        .name = "redis_aof_rewrite_buffer_length_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size of the AOF rewrite buffer.",
    },
    [FAM_REDIS_AOF_PENDING_BIO_FSYNC] = {
        .name = "redis_aof_pending_bio_fsync",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of fsync pending jobs in background I/O queue.",
    },
    [FAM_REDIS_AOF_DELAYED_FSYNC] = {
        .name = "redis_aof_delayed_fsync",
        .type = METRIC_TYPE_GAUGE,
        .help = "Delayed fsync counter.",
    },
    [FAM_REDIS_MODULE_FORK_IN_PROGRESS] = {
        .name = "redis_module_fork_in_progress",
        .type = METRIC_TYPE_GAUGE,
        .help = "Flag indicating a module fork is on-going.",
    },
    [FAM_REDIS_MODULE_FORK_LAST_COW_SIZE_BYTES] = {
        .name = "redis_module_fork_last_cow_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The size in bytes of copy-on-write memory during the last module fork operation.",
    },
    [FAM_REDIS_CONNECTIONS_RECEIVED] = {
        .name = "redis_connections_received",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of connections accepted by the server.",
    },
    [FAM_REDIS_COMMANDS_PROCESSED] = {
        .name = "redis_commands_processed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of commands processed by the server.",
    },
    [FAM_REDIS_INPUT_BYTES] = {
        .name = "redis_input_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes read from the network.",
    },
    [FAM_REDIS_OUTPUT_BYTES] = {
        .name = "redis_output_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes written to the network.",
    },
    [FAM_REDIS_REJECTED_CONNECTIONS] = {
        .name = "redis_rejected_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of connections rejected because of maxclients limit.",
    },
    [FAM_REDIS_REPLICA_RESYNCS_FULL] = {
        .name = "redis_replica_resyncs_full",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of full resyncs with replicas.",
    },
    [FAM_REDIS_REPLICA_PARTIAL_RESYNC_ACCEPTED] = {
        .name = "redis_replica_partial_resync_accepted",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of accepted partial resync requests.",
    },
    [FAM_REDIS_REPLICA_PARTIAL_RESYNC_DENIED] = {
        .name = "redis_replica_partial_resync_denied",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of denied partial resync requests.",
    },
    [FAM_REDIS_EXPIRED_KEYS] = {
        .name = "redis_expired_keys",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of key expiration events.",
    },
    [FAM_REDIS_EXPIRED_STALE_RATIO] = {
        .name = "redis_expired_stale_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "The percentage of keys probably expired.",
    },
    [FAM_REDIS_EXPIRED_TIME_CAP_REACHED] = {
        .name = "redis_expired_time_cap_reached",
        .type = METRIC_TYPE_COUNTER,
        .help = "The count of times that active expiry cycles have stopped early.",
    },
    [FAM_REDIS_EXPIRE_CYCLE_CPU_MSECONDS] = {
        .name = "redis_expire_cycle_cpu_mseconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The cumulative amount of time spend on active expiry cycles.",
    },
    [FAM_REDIS_EVICTED_KEYS] = {
        .name = "redis_evicted_keys",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of evicted keys due to maxmemory limit.",
    },
    [FAM_REDIS_KEYSPACE_HITS] = {
        .name = "redis_keyspace_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of successful lookup of keys in the main dictionary.",
    },
    [FAM_REDIS_KEYSPACE_MISSES] = {
        .name = "redis_keyspace_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of failed lookup of keys in the main dictionary.",
    },
    [FAM_REDIS_PUBSUB_CHANNELS] = {
        .name = "redis_pubsub_channels",
        .type = METRIC_TYPE_GAUGE,
        .help = "Global number of pub/sub channels with client subscriptions.",
    },
    [FAM_REDIS_PUBSUB_PATTERNS] = {
        .name = "redis_pubsub_patterns",
        .type = METRIC_TYPE_GAUGE,
        .help = "Global number of pub/sub pattern with client subscriptions.",
    },
    [FAM_REDIS_LATEST_FORK_USECECONDS] = {
        .name = "redis_latest_fork_usececonds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Duration of the latest fork operation in microseconds.",
    },
    [FAM_REDIS_FORKS] = {
        .name = "redis_forks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of fork operations since the server start.",
    },
    [FAM_REDIS_MIGRATE_CACHED_SOCKETS] = {
        .name = "redis_migrate_cached_sockets",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of sockets open for MIGRATE purposes.",
    },
    [FAM_REDIS_SLAVE_EXPIRES_TRACKED_KEYS] = {
        .name = "redis_slave_expires_tracked_keys",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of keys tracked for expiry purposes "
                "(applicable only to writable replicas).",
    },
    [FAM_REDIS_ACTIVE_DEFRAG_HITS] = {
        .name = "redis_active_defrag_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of value reallocations performed by active the defragmentation process.",
    },
    [FAM_REDIS_ACTIVE_DEFRAG_MISSES] = {
        .name = "redis_active_defrag_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of aborted value reallocations started by "
                "the active defragmentation process.",
    },
    [FAM_REDIS_ACTIVE_DEFRAG_KEY_HITS] = {
        .name = "redis_active_defrag_key_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of keys that were actively defragmented.",
    },
    [FAM_REDIS_ACTIVE_DEFRAG_KEY_MISSES] = {
        .name = "redis_active_defrag_key_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of keys that were skipped by the active defragmentation process.",
    },
    [FAM_REDIS_TRACKING_KEYS] = {
        .name = "redis_tracking_keys",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of keys being tracked by the server.",
    },
    [FAM_REDIS_TRACKING_ITEMS] = {
        .name = "redis_tracking_items",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of items, that is the sum of clients number for each key, "
                "that are being tracked.",
    },
    [FAM_REDIS_TRACKING_PREFIXES] = {
        .name = "redis_tracking_prefixes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of tracked prefixes in server's prefix table "
                "(only applicable for broadcast mode).",
    },
    [FAM_REDIS_UNEXPECTED_ERROR_REPLIES] = {
        .name = "redis_unexpected_error_replies",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of unexpected error replies, that are types of errors "
                "from an AOF load or replication.",
    },
    [FAM_REDIS_ERROR_REPLIES] = {
        .name = "redis_error_replies",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of issued error replies, that is the sum of rejected commands "
                "(errors prior command execution) and failed commands "
                "(errors within the command execution).",
    },
    [FAM_REDIS_READS_PROCESSED] = {
        .name = "redis_reads_processed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of read events processed.",
    },
    [FAM_REDIS_WRITES_PROCESSED] = {
        .name = "redis_writes_processed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of write events processed.",
    },
    [FAM_REDIS_IO_THREADED_READS_PROCESSED] = {
        .name = "redis_io_threaded_reads_processed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of read events processed by the main and I/O threads.",
    },
    [FAM_REDIS_IO_THREADED_WRITES_PROCESSED] = {
        .name = "redis_io_threaded_writes_processed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of write events processed by the main and I/O threads.",
    },
    [FAM_REDIS_CPU_SYS_SECONDS] = {
        .name = "redis_cpu_sys_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "System CPU consumed by the Redis server, which is the sum of system "
                "CPU consumed by all threads of the server process "
                "(main thread and background threads).",
    },
    [FAM_REDIS_CPU_USER_SECONDS] = {
        .name = "redis_cpu_user_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "User CPU consumed by the Redis server, which is the sum of user "
                "CPU consumed by all threads of the server process "
                "(main thread and background threads).",
    },
    [FAM_REDIS_CPU_SYS_CHILDREN_SECONDS] = {
        .name = "redis_cpu_sys_children_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "System CPU consumed by the background processes.",
    },
    [FAM_REDIS_CPU_USER_CHILDREN_SECONDS] = {
        .name = "redis_cpu_user_children_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "User CPU consumed by the background processes.",
    },
    [FAM_REDIS_CPU_SYS_MAIN_THREAD_SECONDS] = {
        .name = "redis_cpu_sys_main_thread_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "System CPU consumed by the Redis server main thread.",
    },
    [FAM_REDIS_CPU_USER_MAIN_THREAD_SECONDS] = {
        .name = "redis_cpu_user_main_thread_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "User CPU consumed by the Redis server main thread.",
    },
    [FAM_REDIS_CLUSTER_ENABLED] = {
        .name = "redis_cluster_enabled",
        .type = METRIC_TYPE_GAUGE,
        .help = "Indicate Redis cluster is enabled.",
    },
    [FAM_REDIS_CONNECTED_SLAVES] = {
        .name = "redis_connected_slaves",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of connected replicas.",
    },
    [FAM_REDIS_GOOD_SLAVES] = {
        .name = "redis_good_slaves",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of replicas currently considered good.",
    },
    [FAM_REDIS_ROLE] = {
        .name = "redis_role",
        .type = METRIC_TYPE_STATE_SET,
        .help = "Value is \"master\" if the instance is replica of no one, "
                "or \"slave\" if the instance is a replica of some master instance.",
    },
    [FAM_REDIS_MASTER_FAILOVER_STATE] = {
        .name = "redis_master_failover_state",
        .type = METRIC_TYPE_STATE_SET,
        .help = "The state of an ongoing failover, if any.",
    },
    [FAM_REDIS_MASTER_REPL_OFFSET] = {
        .name = "redis_master_repl_offset",
        .type = METRIC_TYPE_GAUGE,
        .help = "The server's current replication offset.",
    },
    [FAM_REDIS_SECOND_REPL_OFFSET] = {
        .name = "redis_second_repl_offset",
        .type = METRIC_TYPE_GAUGE,
        .help = "The offset up to which replication IDs are accepted.",
    },
    [FAM_REDIS_REPLICATION_BACKLOG_ACTIVE] = {
        .name = "redis_replication_backlog_active",
        .type = METRIC_TYPE_GAUGE,
        .help = "Flag indicating replication backlog is active.",
    },
    [FAM_REDIS_REPLICATION_BACKLOG_SIZE_BYTE] = {
        .name = "redis_replication_backlog_size_byte",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total size in bytes of the replication backlog buffer.",
    },
    [FAM_REDIS_REPLICATION_BACKLOG_FIRST_BYTE_OFFSET] = {
        .name = "redis_replication_backlog_first_byte_offset",
        .type = METRIC_TYPE_GAUGE,
        .help = "The master offset of the replication backlog buffer.",
    },
    [FAM_REDIS_REPLICATION_BACKLOG_HIST_BYTES] = {
        .name = "redis_replication_backlog_hist_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size in bytes of the data in the replication backlog buffer.",
    },
    [FAM_REDIS_MASTER_LINK_UP] = {
        .name = "redis_master_link_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Status of the link (up/down).",
    },
    [FAM_REDIS_MASTER_LAST_IO_SECONDS] = {
        .name = "redis_master_last_io_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of seconds since the last interaction with master.",
    },
    [FAM_REDIS_MASTER_SYNC_IN_PROGRESS] = {
        .name = "redis_master_sync_in_progress",
        .type = METRIC_TYPE_GAUGE,
        .help = "Indicate the master is syncing to the replica.",
    },
    [FAM_REDIS_SLAVE_REPLICATION_OFFSET] = {
        .name = "redis_slave_replication_offset",
        .type = METRIC_TYPE_GAUGE,
        .help = "The replication offset of the replica instance.",
    },
    [FAM_REDIS_SLAVE_PRIORITY] = {
        .name = "redis_slave_priority",
        .type = METRIC_TYPE_GAUGE,
        .help = "The priority of the instance as a candidate for failover.",
    },
    [FAM_REDIS_SLAVE_READ_ONLY] = {
        .name = "redis_slave_read_only",
        .type = METRIC_TYPE_GAUGE,
        .help = "Flag indicating if the replica is read-only.",
    },
    [FAM_REDIS_COMMANDS_DURATION_SECONDS] = {
        .name = "redis_commands_duration_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total CPU time consumed by these commands.",
    },
    [FAM_REDIS_COMMANDS] = {
        .name = "redis_commands",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of calls that reached command execution (not rejected).",
    },
    [FAM_REDIS_COMMANDS_REJECTED] = {
        .name = "redis_commands_rejected",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rejected calls (errors prior command execution).",
    },
    [FAM_REDIS_COMMANDS_FAILED] = {
        .name = "redis_commands_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of failed calls (errors within the command execution.",
    },
    [FAM_REDIS_DB_KEYS] = {
        .name = "redis_db_keys",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_REDIS_DB_KEYS_EXPIRING] = {
        .name = "redis_db_keys_expiring",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_REDIS_ERRORS] = {
        .name = "redis_errors",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of errors that occurred within Redis, "
                "based upon the reply error prefix.",
    },
    [FAM_REDIS_SLAVE_STATE] = {
        .name = "redis_slave_state",
        .type = METRIC_TYPE_STATE_SET,
        .help = NULL,
    },
    [FAM_REDIS_SLAVE_LAG] = {
        .name = "redis_slave_lag",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_REDIS_SLAVE_OFFSET] = {
        .name = "redis_slave_offset",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_REDIS_SENTINEL_MASTERS] = {
        .name = "redis_sentinel_masters",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of masters this sentinel is watching.",
    },
    [FAM_REDIS_SENTINEL_TILT] = {
        .name = "redis_sentinel_tilt",
        .type = METRIC_TYPE_GAUGE,
        .help = "Sentinel is in TILT mode.",
    },
    [FAM_REDIS_SENTINEL_RUNNING_SCRIPTS] = {
        .name = "redis_sentinel_running_scripts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of scripts in execution right now.",
    },
    [FAM_REDIS_SENTINEL_SCRIPTS_QUEUE_LENGTH] = {
        .name = "redis_sentinel_scripts_queue_length",
        .type = METRIC_TYPE_GAUGE,
        .help = "Queue of user scripts to execute.",
    },
    [FAM_REDIS_SENTINEL_SIMULATE_FAILURE_FLAGS] = {
        .name = "redis_sentinel_simulate_failure_flags",
        .type = METRIC_TYPE_GAUGE,
        .help = "Failures simulations.",
    },
    [FAM_REDIS_SENTINEL_MASTER_STATUS] = {
        .name = "sentinel_master_status",
        .type = METRIC_TYPE_STATE_SET,
        .help = "Master status on Sentinel.",
    },
    [FAM_REDIS_SENTINEL_MASTER_SLAVES] = {
        .name = "sentinel_master_slaves",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of slaves of the master.",
    },
    [FAM_REDIS_SENTINEL_MASTER_SENTINELS] = {
        .name = "sentinel_master_sentinels",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of sentinels monitoring this master.",
    },
};
