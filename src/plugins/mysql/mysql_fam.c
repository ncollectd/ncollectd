// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libmetric/metric.h"
#include "plugins/mysql/mysql_fam.h"

metric_family_t fam_mysql_status[FAM_MYSQL_STATUS_MAX] = {
    [FAM_MYSQL_UP] = {
        .name = "mysql_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the mysql server be reached.",
    },
    [FAM_MYSQL_ABORTED_CLIENTS] = {
        .name = "mysql_aborted_clients",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of aborted client connections.",
    },
    [FAM_MYSQL_ABORTED_CONNECTS] = {
        .name = "mysql_aborted_connects",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of failed server connection attempts.",
    },
    [FAM_MYSQL_ABORTED_CONNECTS_PREAUTH] = {
        .name = "mysql_aborted_connects_preauth",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of connection attempts that were aborted prior to authentication.",
    },
    [FAM_MYSQL_ACCESS_DENIED_ERRORS] = {
        .name = "mysql_access_denied_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of connection attempts that were aborted prior to authentication.",
    },
    [FAM_MYSQL_BUSY_TIME_SECONDS] = {
        .name = "mysql_busy_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cumulative time in seconds of activity on connections",
    },
    [FAM_MYSQL_RECEIVED_BYTES] = {
        .name = "mysql_received_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes received from all clients",
    },
    [FAM_MYSQL_SENT_BYTES] = {
        .name = "mysql_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes sent to all clients",
    },
    [FAM_MYSQL_CONNECTION_ERRORS] = {
        .name = "mysql_connection_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of MySQL connection errors",
    },
    [FAM_MYSQL_CONNECTIONS] = {
        .name = "mysql_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of connection attempts (both successful and unsuccessful)",
    },
    [FAM_MYSQL_CPU_TIME] = {
        .name = "mysql_cpu_time",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total CPU time used",
    },
    [FAM_MYSQL_STATEMENT_TIME_EXCEEDED] = {
        .name = "mysql_statement_time_exceeded",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of queries that exceeded the execution time "
                " specified by max_statement_time.",
    },
    [FAM_MYSQL_MAX_USED_CONNECTIONS] = {
        .name = "mysql_max_used_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum number of connections that have been in use simultaneously "
                "since the server started.",
    },
    [FAM_MYSQL_MEMORY_USED_BYTES] = {
        .name = "mysql_memory_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Global or per-connection memory usage, in bytes",
    },
    [FAM_MYSQL_MEMORY_USED_INITIAL_BYTES] = {
        .name = "mysql_memory_used_initial_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory that was used when the server started "
                "to service the user connections",
    },
    [FAM_MYSQL_OPEN_FILES] = {
        .name = "mysql_open_files",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of regular files currently opened by the server. "
                "Does not include sockets or pipes, or storage engines using internal functions",
    },
    [FAM_MYSQL_OPEN_STREAMS] = {
        .name = "mysql_open_streams",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of currently opened streams, usually log files",
    },
    [FAM_MYSQL_OPEN_TABLE_DEFINITIONS] = {
        .name = "mysql_open_table_definitions",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of currently cached .frm files",
    },
    [FAM_MYSQL_OPEN_TABLES] = {
        .name = "mysql_open_tables",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of currently opened tables, excluding temporary tables",
    },
    [FAM_MYSQL_OPENED_FILES] = {
        .name = "mysql_opened_files",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of files the server has opened",
    },
    [FAM_MYSQL_OPENED_PLUGIN_LIBRARIES] = {
        .name = "mysql_opened_plugin_libraries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of shared libraries that the server has opened to load plugins",
    },
    [FAM_MYSQL_OPENED_TABLE_DEFINITIONS] = {
        .name = "mysql_opened_table_definitions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of .frm files that have been cached",
    },
    [FAM_MYSQL_OPENED_TABLES] = {
        .name = "mysql_opened_tables",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of tables the server has opened",
    },
    [FAM_MYSQL_OPENED_VIEWS] = {
        .name = "mysql_opened_views",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of views the server has opened",
    },
    [FAM_MYSQL_PREPARED_STMT] = {
        .name = "mysql_prepared_stmt",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current number of prepared statements",
    },
    [FAM_MYSQL_COLUMN_COMPRESSIONS] = {
        .name = "mysql_column_compressions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented each time field data is compressed.",
    },
    [FAM_MYSQL_COLUMN_DECOMPRESSIONS] = {
        .name = "mysql_column_decompressions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Incremented each time field data is decompressed.",
    },
    [FAM_MYSQL_QUERIES] = {
        .name = "mysql_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of statements executed by the server, "
                "excluding COM_PING and COM_STATISTICS.",
    },
    [FAM_MYSQL_QUESTIONS] = {
        .name = "mysql_questions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of statements executed by the server, excluding COM_PING, COM_STATISTICS, "
                "COM_STMT_PREPARE, COM_STMT_CLOSE, and COM_STMT_RESET statements.",
    },
    [FAM_MYSQL_ROWS_READ] = {
        .name = "mysql_rows_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of requests to read a row (excluding temporary tables).",
    },
    [FAM_MYSQL_ROWS_SENT] = {
        .name = "mysql_rows_sent",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rows sent to the client.",
    },
    [FAM_MYSQL_ROWS_TMP_READ] = {
        .name = "mysql_rows_tmp_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of requests to read a row in a temporary table.",
    },
    [FAM_MYSQL_SELECT_FULL_JOIN] = {
        .name = "mysql_select_full_join",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of joins that perform table scans because they do not use indexes.",
    },
    [FAM_MYSQL_SELECT_FULL_RANGE_JOIN] = {
        .name = "mysql_select_full_range_join",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of joins that used a range search on a reference table.",
    },
    [FAM_MYSQL_SELECT_RANGE] = {
        .name = "mysql_select_range",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of joins that used ranges on the first table.",
    },
    [FAM_MYSQL_SELECT_RANGE_CHECK] = {
        .name = "mysql_select_range_check",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of joins without keys that check for key usage after each row.",
    },
    [FAM_MYSQL_SELECT_SCAN] = {
        .name = "mysql_select_scan",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of joins that did a full scan of the first table.",
    },
    [FAM_MYSQL_SUBQUERY_CACHE_HIT] = {
        .name = "mysql_subquery_cache_hit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Counter for all subquery cache hits.",
    },
    [FAM_MYSQL_SUBQUERY_CACHE_MISS] = {
        .name = "mysql_subquery_cache_miss",
        .type = METRIC_TYPE_COUNTER,
        .help = "Counter for all subquery cache misses.",
    },
    [FAM_MYSQL_SYNCS] = {
        .name = "mysql_syncs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times my_sync() has been called, "
                "or the number of times the server has had to force data to disk.",
    },
    [FAM_MYSQL_TABLE_LOCKS_IMMEDIATE] = {
        .name = "mysql_table_locks_immediate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of table locks which were completed immediately.",
    },
    [FAM_MYSQL_TABLE_LOCKS_WAITED] = {
        .name = "mysql_table_locks_waited",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of table locks which had to wait. Indicates table lock contention.",
    },
    [FAM_MYSQL_TABLE_OPEN_CACHE_ACTIVE_INSTANCES] = {
        .name = "mysql_table_open_cache_active_instances",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of active instances for open tables cache lookups.",
    },
    [FAM_MYSQL_TABLE_OPEN_CACHE_HITS] = {
        .name = "mysql_table_open_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of hits for open tables cache lookups.",
    },
    [FAM_MYSQL_TABLE_OPEN_CACHE_MISSES] = {
        .name = "mysql_table_open_cache_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of misses for open tables cache lookups.",
    },
    [FAM_MYSQL_TABLE_OPEN_CACHE_OVERFLOWS] = {
        .name = "mysql_table_open_cache_overflows",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of overflows for open tables cache lookups.",
    },
    [FAM_MYSQL_TC_LOG_MAX_PAGES_USED] = {
        .name = "mysql_tc_log_max_pages_used",
        .type = METRIC_TYPE_GAUGE,
        .help = "Max number of pages used by the memory-mapped file-based "
                "transaction coordinator log.",
    },
    [FAM_MYSQL_TC_LOG_PAGE_SIZE] = {
        .name = "mysql_tc_log_page_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Page size of the memory-mapped file-based transaction coordinator log.",
    },
    [FAM_MYSQL_TC_LOG_PAGE_WAITS] = {
        .name = "mysql_tc_log_page_waits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a two-phase commit was forced to wait "
                "for a free memory-mapped file-based transaction coordinator log page.",
    },
    [FAM_MYSQL_THREADPOOL_IDLE_THREADS] = {
        .name = "mysql_threadpool_idle_threads",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of inactive threads in the thread pool.",
    },
    [FAM_MYSQL_THREADPOOL_THREADS] = {
        .name = "mysql_threadpool_threads",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of threads in the thread pool.",
    },
    [FAM_MYSQL_THREADS_CACHED] = {
        .name = "mysql_threads_cached",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of threads cached in the thread cache.",
    },
    [FAM_MYSQL_THREADS_CONNECTED] = {
        .name = "mysql_threads_connected",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of clients connected to the server.",
    },
    [FAM_MYSQL_THREADS_CREATED] = {
        .name = "mysql_threads_created",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of threads created to respond to client connections.",
    },
    [FAM_MYSQL_THREADS_RUNNING] = {
        .name = "mysql_threads_running",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of client connections that are actively running a command.",
    },
    [FAM_MYSQL_UPDATE_SCAN] = {
        .name = "mysql_update_scan",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of updates that required a full table scan.",
    },
    [FAM_MYSQL_UPTIME_SECONDS] = {
        .name = "mysql_uptime_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of seconds the server has been running.",
    },
    [FAM_MYSQL_UPTIME_SINCE_FLUSH_STATUS_SECONDS] = {
        .name = "mysql_uptime_since_flush_status_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of seconds since the last FLUSH STATUS.",
    },
    [FAM_MYSQL_SLOW_LAUNCH_THREADS] = {
        .name = "mysql_slow_launch_threads",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of threads that have taken more than slow_launch_time seconds to create.",
    },
    [FAM_MYSQL_SLOW_QUERIES] = {
        .name = "mysql_slow_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that have taken more than long_query_time seconds.",
    },
    [FAM_MYSQL_SORT_MERGE_PASSES] = {
        .name = "mysql_sort_merge_passes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of merge passes that the sort algorithm has had to do.",
    },
    [FAM_MYSQL_SORT_PRIORITY_QUEUE_SORTS] = {
        .name = "mysql_sort_priority_queue_sorts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times that sorting was done through a priority queue.",
    },
    [FAM_MYSQL_SORT_RANGE] = {
        .name = "mysql_sort_range",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of sorts that were done using ranges.",
    },
    [FAM_MYSQL_SORT_ROWS] = {
        .name = "mysql_sort_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of sorted rows.",
    },
    [FAM_MYSQL_SORT_SCAN] = {
        .name = "mysql_sort_scan",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of sorts that were done by scanning the table.",
    },
    [FAM_MYSQL_CREATED_TMP_DISK_TABLES] = {
        .name = "mysql_created_tmp_disk_tables",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of internal on-disk temporary tables created by the server "
                "while executing statements.",
    },
    [FAM_MYSQL_CREATED_TMP_FILES] = {
        .name = "mysql_created_tmp_files",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many temporary files mysqld has created."
    },
    [FAM_MYSQL_CREATED_TMP_TABLES] = {
        .name = "mysql_created_tmp_tables",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of internal temporary tables created by the server "
                "while executing statements."
    },
    [FAM_MYSQL_DELETE_SCAN] = {
        .name = "mysql_delete_scan",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of DELETEs that required a full table scan.",
    },
    [FAM_MYSQL_EMPTY_QUERIES] = {
        .name = "mysql_empty_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries returning no results.",
    },
    [FAM_MYSQL_EXECUTED_EVENTS] = {
        .name = "mysql_executed_events",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times events created with CREATE EVENT have executed.",
    },
    [FAM_MYSQL_EXECUTED_TRIGGERS] = {
        .name = "mysql_executed_triggers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times triggers created with CREATE TRIGGER have executed.",
    },
    [FAM_MYSQL_ACL_GRANTS] = {
        .name = "mysql_acl_grants",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of permissions granted.",
    },
    [FAM_MYSQL_ACL_PROXY_USERS] = {
        .name = "mysql_acl_proxy_users",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of proxy permissions granted (rows in the mysql.proxies_priv table).",
    },
    [FAM_MYSQL_ACL_ROLES] = {
        .name = "mysql_acl_roles",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of roles (rows in the mysql.user table where is_role='Y').",
    },
    [FAM_MYSQL_ACL_USERS] = {
        .name = "mysql_acl_users",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of users (rows in the mysql.user table where is_role='N').",
    },
    [FAM_MYSQL_ARIA_PAGECACHE_BLOCKS_NOT_FLUSHED] = {
        .name = "mysql_aria_pagecache_blocks_not_flushed",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of dirty blocks in the Aria page cache.",
    },
    [FAM_MYSQL_ARIA_PAGECACHE_BLOCKS_UNUSED] = {
        .name = "mysql_aria_pagecache_blocks_unused",
        .type = METRIC_TYPE_GAUGE,
        .help = "Free blocks in the Aria page cache.",
    },
    [FAM_MYSQL_ARIA_PAGECACHE_BLOCKS_USED] = {
        .name = "mysql_aria_pagecache_blocks_used",
        .type = METRIC_TYPE_GAUGE,
        .help = "Blocks used in the Aria page cache.",
    },
    [FAM_MYSQL_ARIA_PAGECACHE_READ_REQUESTS] = {
        .name = "mysql_aria_pagecache_read_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of requests to read something from the Aria page cache.",
    },
    [FAM_MYSQL_ARIA_PAGECACHE_READS] = {
        .name = "mysql_aria_pagecache_reads",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of Aria page cache read requests "
                "that caused a block to be read from the disk.",
    },
    [FAM_MYSQL_ARIA_PAGECACHE_WRITE_REQUESTS] = {
        .name = "mysql_aria_pagecache_write_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of requests to write a block to the Aria page cache.",
    },
    [FAM_MYSQL_ARIA_PAGECACHE_WRITES] = {
        .name = "mysql_aria_pagecache_writes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of blocks written to disk from the Aria page cache.",
    },
    [FAM_MYSQL_ARIA_TRANSACTION_LOG_SYNCS] = {
        .name = "mysql_aria_transaction_log_syncs",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of Aria log fsyncs.",
    },
    [FAM_MYSQL_BINLOG_COMMITS] = {
        .name = "mysq_binlog_commits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of transactions committed to the binary log.",
    },
    [FAM_MYSQL_BINLOG_GROUP_COMMITS] = {
        .name = "mysq_binlog_group_commits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of group commits done to the binary log."
    },
    [FAM_MYSQL_BINLOG_GROUP_COMMIT_TRIGGER_COUNT] = {
        .name = "mysq_binlog_group_commit_trigger_count",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of group commits triggered because of the number of "
                "binary log commits in the group reached the limit.",
    },
    [FAM_MYSQL_BINLOG_GROUP_COMMIT_TRIGGER_LOCK_WAIT] = {
        .name = "mysq_binlog_group_commit_trigger_lock_wait",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of group commits triggered because a "
                "binary log commit was being delayed because of a lock wait "
                "where the lock was held by a prior binary log commit.",
    },
    [FAM_MYSQL_BINLOG_GROUP_COMMIT_TRIGGER_TIMEOUT] = {
        .name = "mysq_binlog_group_commit_trigger_timeout",
        .type = METRIC_TYPE_GAUGE,
        .help = "",
    },
    [FAM_MYSQL_BINLOG_SNAPSHOT_POSITION] = {
        .name = "mysq_binlog_snapshot_position",
        .type = METRIC_TYPE_GAUGE,
        .help = "",
    },
    [FAM_MYSQL_BINLOG_WRITTEN_BYTES] = {
        .name = "mysq_binlog_written_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of bytes written to the binary log."
    },
    [FAM_MYSQL_BINLOG_CACHE_DISK_USE] = {
        .name = "mysq_binlog_cache_disk_use",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of transactions which used a temporary disk cache because "
                "they could not fit in the regular binary log cache.",
    },
    [FAM_MYSQL_BINLOG_CACHE_USE] = {
        .name = "mysq_binlog_cache_use",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of transaction which used the regular binary log cache, "
                "being smaller than binlog_cache_size.",
    },
    [FAM_MYSQL_BINLOG_STMT_CACHE_DISK_USE] = {
        .name = "mysq_binlog_stmt_cache_disk_use",
        .type = METRIC_TYPE_GAUGE,
        .help = "",
    },
    [FAM_MYSQL_BINLOG_STMT_CACHE_USE] = {
        .name = "mysq_binlog_stmt_cache_use",
        .type = METRIC_TYPE_GAUGE,
        .help = "",
    },
    [FAM_MYSQL_COMMANDS] = {
        .name = "mysql_commands",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of executed MySQL commands",
    },
    [FAM_MYSQL_FEATURE] = {
        .name = "mysql_feature",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times that this feature was used.",
    },
    [FAM_MYSQL_HANDLERS] = {
        .name = "mysql_handlers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of executed MySQL handlers.",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_DATA_BYTES] = {
        .name = "mysql_innodb_buffer_pool_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes contained in the InnoDB buffer pool, "
                "both dirty (modified) and clean (unmodified).",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_DIRTY_BYTES] = {
        .name = "mysql_innodb_buffer_pool_dirty_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of dirty (modified) bytes contained in the InnoDB buffer pool.",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_PAGES] = {
        .name = "mysql_innodb_buffer_pool_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Innodb buffer pool pages by state.",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_DIRTY_PAGES] = {
        .name = "mysql_innodb_buffer_pool_dirty_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Innodb buffer pool dirty pages.",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_PAGE_CHANGES] = {
        .name = "mysql_innodb_buffer_pool_page_changes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Innodb buffer pool page state changes.",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_PAGES_COUNT] = {
        .name = "mysql_innodb_buffer_pool_pages_count",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current size of the InnoDB buffer pool, in pages.",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_READ_AHEAD_RANDOM] = {
        .name = "mysql_innodb_buffer_pool_read_ahead_random",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of random read-aheads.",
    },
    [FAM_MYSQL_INNODB_CHECKPOINT_AGE] = {
        .name = "mysql_innodb_checkpoint_age",
        .type = METRIC_TYPE_GAUGE,
        .help = "The checkpoint age.",
    },
    [FAM_MYSQL_INNODB_CHECKPOINT_MAX_AGE] = {
        .name = "mysql_innodb_checkpoint_max_age",
        .type = METRIC_TYPE_GAUGE,
        .help = "Max checkpoint age.",
    },
    [FAM_MYSQL_INNODB_DATA_FSYNCS] = {
        .name = "mysql_innodb_data_fsyncs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of InnoDB fsync (sync-to-disk) calls.",
    },
    [FAM_MYSQL_INNODB_DATA_PENDING_FSYNCS] = {
        .name = "mysql_innodb_data_pending_fsyncs",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of pending InnoDB fsync (sync-to-disk) calls.",
    },
    [FAM_MYSQL_INNODB_DATA_PENDING_READS] = {
        .name = "mysql_innodb_data_pending_reads",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of pending InnoDB reads.",
    },
    [FAM_MYSQL_INNODB_DATA_PENDING_WRITES] = {
        .name = "mysql_innodb_data_pending_writes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of pending InnoDB writes.",
    },
    [FAM_MYSQL_INNODB_DATA_READ_BYTES] = {
        .name = "mysql_innodb_data_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of InnoDB bytes read since server startup.",
    },
    [FAM_MYSQL_INNODB_DATA_READS] = {
        .name = "mysql_innodb_data_reads",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of InnoDB read operations.",
    },
    [FAM_MYSQL_INNODB_DATA_WRITES] = {
        .name = "mysql_innodb_data_writes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of InnoDB write operations.",
    },
    [FAM_MYSQL_INNODB_DATA_WRITTEN_BYTES] = {
        .name = "mysql_innodb_data_written_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of InnoDB bytes written since server startup.",
    },
    [FAM_MYSQL_INNODB_DEADLOCKS] = {
        .name = "mysql_innodb_deadlocks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of InnoDB deadlocks.",
    },
    [FAM_MYSQL_INNODB_HISTORY_LIST_LENGTH] = {
        .name = "mysql_innodb_history_list_length",
        .type = METRIC_TYPE_GAUGE,
        .help = "Innodb unflushed changes.",
    },
    [FAM_MYSQL_INNODB_INSERT_BUFFER_DISCARDED_DELETE_MARKS] = {
        .name = "mysql_innodb_insert_buffer_discarded_delete_marks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of discarded buffered delete mark operations.",
    },
    [FAM_MYSQL_INNODB_INSERT_BUFFER_DISCARDED_DELETES] = {
        .name = "mysql_innodb_insert_buffer_discarded_deletes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of discarded buffered delete operations.",
    },
    [FAM_MYSQL_INNODB_INSERT_BUFFER_DISCARDED_INSERTS] = {
        .name = "mysql_innodb_insert_buffer_discarded_inserts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of discarded buffered delete operations.",
    },
    [FAM_MYSQL_INNODB_INSERT_BUFFER_MERGED_DELETE_MARKS] = {
        .name = "mysql_innodb_insert_buffer_merged_delete_marks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of delete mark operations merged.",
    },
    [FAM_MYSQL_INNODB_INSERT_BUFFER_MERGED_DELETES] = {
        .name = "mysql_innodb_insert_buffer_merged_deletes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of delete operations merged.",
    },
    [FAM_MYSQL_INNODB_INSERT_BUFFER_MERGED_INSERTS] = {
        .name = "mysql_innodb_insert_buffer_merged_inserts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of insert operations merged.",
    },
    [FAM_MYSQL_INNODB_INSERT_BUFFER_MERGES] = {
        .name = "mysql_innodb_insert_buffer_merges",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL
    },
    [FAM_MYSQL_INNODB_INSERT_BUFFER_FREE_LIST] = {
        .name = "mysql_innodb_insert_buffer_free_list",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL
    },
    [FAM_MYSQL_INNODB_INSERT_BUFFER_SEGMENT_SIZE] = {
        .name = "mysql_innodb_insert_buffer_segment_size",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL
    },
    [FAM_MYSQL_INNODB_INSERT_BUFFER_SIZE] = {
        .name = "mysql_innodb_insert_buffer_size",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL
    },
    [FAM_MYSQL_INNODB_LOG_SEQUENCE_CURRENT] = {
        .name = "mysql_innodb_log_sequence_current",
        .type = METRIC_TYPE_GAUGE,
        .help = "Log sequence number.",
    },
    [FAM_MYSQL_INNODB_LOG_SEQUENCE_FLUSHED] = {
        .name = "mysql_innodb_log_sequence_flushed",
        .type = METRIC_TYPE_GAUGE,
        .help = "Flushed up to log sequence number",
    },
    [FAM_MYSQL_INNODB_LOG_SEQUENCE_LAST_CHECKPOINT] = {
        .name = "mysql_innodb_log_sequence_last_checkpoint",
        .type = METRIC_TYPE_GAUGE,
        .help = "Log sequence number last checkpoint",
    },
    [FAM_MYSQL_INNODB_MASTER_THREAD_ACTIVE_LOOPS] = {
        .name = "mysql_innodb_master_thread_active_loops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the above one-second loop was executed for active server states.",
    },
    [FAM_MYSQL_INNODB_MASTER_THREAD_IDLE_LOOPS] = {
        .name = "mysql_innodb_master_thread_idle_loops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the above one-second loop was executed for idle server states.",
    },
    [FAM_MYSQL_INNODB_MAX_TRX_ID] = {
        .name = "mysql_innodb_max_trx_id",
        .type = METRIC_TYPE_GAUGE,
        .help = "Next free transaction id number."
    },
    [FAM_MYSQL_INNODB_MEM_ADAPTIVE_HASH] = {
        .name = "mysql_innodb_mem_adaptive_hash",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MYSQL_INNODB_MEM_DICTIONARY] = {
        .name = "mysql_innodb_mem_dictionary",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_MYSQL_INNODB_PAGE_SIZE_BYTES] = {
        .name = "mysql_innodb_page_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "InnoDB page size",
    },
    [FAM_MYSQL_INNODB_PAGES_CREATED] = {
        .name = "mysql_innodb_pages_created",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of pages created by operations on InnoDB tables."
    },
    [FAM_MYSQL_INNODB_PAGES_READ] = {
        .name = "mysql_innodb_pages_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of pages read from the InnoDB buffer pool by "
                "operations on InnoDB tables.",
    },
    [FAM_MYSQL_INNODB_PAGES_WRITTEN] = {
        .name = "mysql_innodb_pages_written",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of pages written by operations on InnoDB tables.",
    },
    [FAM_MYSQL_INNODB_ROW_OPS] = {
        .name = "mysql_innodb_row_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number rows per operation."
    },
    [FAM_MYSQL_INNODB_SYSTEM_ROWS_OPS] = {
        .name = "mysql_innodb_system_rows_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number rows belonging to system-created schemas per operation."
    },
    [FAM_MYSQL_INNODB_NUM_OPEN_FILES] = {
        .name = "mysql_innodb_num_open_files",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of files InnoDB currently holds open.",
    },
    [FAM_MYSQL_INNODB_TRUNCATED_STATUS_WRITES] = {
        .name = "mysql_innodb_truncated_status_writes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times output from the SHOW ENGINE INNODB STATUS statement "
                "has been truncated.",
    },
    [FAM_MYSQL_INNODB_AVAILABLE_UNDO_LOGS] = {
        .name = "mysql_innodb_available_undo_logs",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total number of available InnoDB rollback segments.",
    },
    [FAM_MYSQL_INNODB_UNDO_TRUNCATIONS] = {
        .name = "mysql_innodb_undo_truncations",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of undo tablespace truncation operations.",
    },
    [FAM_MYSQL_INNODB_PAGE_COMPRESSION_SAVED_BYTES] = {
        .name = "mysql_innodb_page_compression_saved",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes saved by page compression.",
    },
    [FAM_MYSQL_INNODB_NUM_INDEX_PAGES_WRITTEN] = {
        .name = "mysql_innodb_num_index_pages_written",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MYSQL_INNODB_NUM_NON_INDEX_PAGES_WRITTEN] = {
        .name = "mysql_innodb_num_non_index_pages_written",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MYSQL_INNODB_NUM_PAGES_PAGE_COMPRESSED] = {
        .name = "mysql_innodb_num_pages_page_compressed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of pages that are page compressed.",
    },
    [FAM_MYSQL_INNODB_NUM_PAGE_COMPRESSED_TRIM_OP] = {
        .name = "mysql_innodb_num_page_compressed_trim_op",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of trim operations performed.",
    },
    [FAM_MYSQL_INNODB_NUM_PAGES_PAGE_DECOMPRESSED] = {
        .name = "mysql_innodb_num_pages_page_decompressed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of pages compressed with page compression that are decompressed.",
    },
    [FAM_MYSQL_INNODB_NUM_PAGES_PAGE_COMPRESSION_ERROR] = {
        .name = "mysql_innodb_num_pages_page_compression_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of compression errors.",
    },
    [FAM_MYSQL_INNODB_NUM_PAGES_ENCRYPTED] = {
        .name = "mysql_innodb_num_pages_encrypted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of pages page encrypted.",
    },
    [FAM_MYSQL_INNODB_NUM_PAGES_DECRYPTED] = {
        .name = "mysql_innodb_num_pages_decrypted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of pages page decrypted.",
    },
    [FAM_MYSQL_INNODB_DEFRAGMENT_COMPRESSION_FAILURES] = {
        .name = "mysql_innodb_defragment_compression_failures",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of defragment re-compression failures.",
    },
    [FAM_MYSQL_INNODB_DEFRAGMENT_FAILURES] = {
        .name = "mysql_innodb_defragment_failures",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of defragment failures.",
    },
    [FAM_MYSQL_INNODB_DEFRAGMENT_COUNT] = {
        .name = "mysql_innodb_defragment_count",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of defragment operations.",
    },
    [FAM_MYSQL_INNODB_INSTANT_ALTER_COLUMN] = {
        .name = "mysql_innodb_instant_alter_column",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of instant ADD COLUMN.", // XXX
    },
    [FAM_MYSQL_INNODB_ONLINEDDL_ROWLOG_ROWS] = {
        .name = "mysql_innodb_onlineddl_rowlog_rows",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of rows stored in the row log buffer.",
    },
    [FAM_MYSQL_INNODB_ONLINEDDL_ROWLOG_PCT_USED] = {
        .name = "mysql_innodb_onlineddl_rowlog_pct_used",
        .type = METRIC_TYPE_GAUGE, // XXX
        .help = "Shows row log buffer usage in 5-digit integer (10000 means 100.00%)",
    },
    [FAM_MYSQL_INNODB_ONLINEDDL_PCT_PROGRESS] = {
        .name = "mysql_innodb_onlineddl_pct_progress",
        .type = METRIC_TYPE_GAUGE,
        .help = "Shows the progress of in-place alter table.",
    },

    [FAM_MYSQL_INNODB_ENCRYPTION_ROTATION_PAGES_READ_FROM_CACHE] = {
        .name = "mysql_innodb_encryption_rotation_pages_read_from_cache",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MYSQL_INNODB_ENCRYPTION_ROTATION_PAGES_READ_FROM_DISK] = {
        .name = "mysql_innodb_encryption_rotation_pages_read_from_disk",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MYSQL_INNODB_ENCRYPTION_ROTATION_PAGES_MODIFIED] = {
        .name = "mysql_innodb_encryption_rotation_pages_modified",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MYSQL_INNODB_ENCRYPTION_ROTATION_PAGES_FLUSHED] = {
        .name = "mysql_innodb_encryption_rotation_pages_flushed",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MYSQL_INNODB_ENCRYPTION_ROTATION_ESTIMATED_IOPS] = {
        .name = "mysql_innodb_encryption_rotation_estimated_iops",
        .type = METRIC_TYPE_GAUGE, // XXX
        .help = NULL,
    },
    [FAM_MYSQL_INNODB_ENCRYPTION_KEY_ROTATION_LIST_LENGTH] = {
        .name = "mysql_innodb_encryption_key_rotation_list_length",
        .type = METRIC_TYPE_GAUGE, // XXX
        .help = NULL,
    },
    [FAM_MYSQL_INNODB_ENCRYPTION_N_MERGE_BLOCKS_ENCRYPTED] = {
        .name = "mysql_innodb_encryption_n_merge_blocks_encrypted",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MYSQL_INNODB_ENCRYPTION_N_MERGE_BLOCKS_DECRYPTED] = {
        .name = "mysql_innodb_encryption_n_merge_blocks_decrypted",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MYSQL_INNODB_ENCRYPTION_N_ROWLOG_BLOCKS_ENCRYPTED] = {
        .name = "mysql_innodb_encryption_n_rowlog_blocks_encrypted",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MYSQL_INNODB_ENCRYPTION_N_ROWLOG_BLOCKS_DECRYPTED] = {
        .name = "mysql_innodb_encryption_n_rowlog_blocks_decrypted",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MYSQL_INNODB_ENCRYPTION_N_TEMP_BLOCKS_ENCRYPTED] = {
        .name = "mysql_innodb_encryption_n_temp_blocks_encrypted",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MYSQL_INNODB_ENCRYPTION_N_TEMP_BLOCKS_DECRYPTED] = {
        .name = "mysql_innodb_encryption_n_temp_blocks_decrypted",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MYSQL_INNODB_ENCRYPTION_NUM_KEY_REQUESTS] = {
        .name = "mysql_innodb_encryption_num_key_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },


    [FAM_MYSQL_INNODB_METADATA_TABLE_HANDLES_OPENED] = {
        .name = "mysql_innodb_metadata_table_handles_opened",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of table handles opened",
    },
    [FAM_MYSQL_INNODB_METADATA_TABLE_HANDLES_CLOSED] = {
        .name = "mysql_innodb_metadata_table_handles_closed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of table handles closed",
    },
    [FAM_MYSQL_INNODB_METADATA_TABLE_REFERENCE_COUNT] = {
        .name = "mysql_innodb_metadata_table_reference_count",
        .type = METRIC_TYPE_COUNTER,
        .help = "Table reference counter",
    },
    [FAM_MYSQL_INNODB_LOCK_DEADLOCKS] = {
        .name = "mysql_innodb_lock_deadlocks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of deadlocks",
    },
    [FAM_MYSQL_INNODB_LOCK_TIMEOUTS] = {
        .name = "mysql_innodb_lock_timeouts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of lock timeouts",
    },
    [FAM_MYSQL_INNODB_LOCK_REC_LOCK_WAITS] = {
        .name = "mysql_innodb_lock_rec_lock_waits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times enqueued into record lock wait queue",
    },
    [FAM_MYSQL_INNODB_LOCK_TABLE_LOCK_WAITS] = {
        .name = "mysql_innodb_lock_table_lock_waits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times enqueued into table lock wait queue",
    },
    [FAM_MYSQL_INNODB_LOCK_REC_LOCK_REQUESTS] = {
        .name = "mysql_innodb_lock_rec_lock_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of record locks requested",
    },
    [FAM_MYSQL_INNODB_LOCK_REC_LOCK_CREATED] = {
        .name = "mysql_innodb_lock_rec_lock_created",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of record locks created",
    },
    [FAM_MYSQL_INNODB_LOCK_REC_LOCK_REMOVED] = {
        .name = "mysql_innodb_lock_rec_lock_removed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of record locks removed from the lock queue",
    },
    [FAM_MYSQL_INNODB_LOCK_REC_LOCKS] = {
        .name = "mysql_innodb_lock_rec_locks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Current number of record locks on tables",
    },
    [FAM_MYSQL_INNODB_LOCK_TABLE_LOCK_CREATED] = {
        .name = "mysql_innodb_lock_table_lock_created",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of table locks created",
    },
    [FAM_MYSQL_INNODB_LOCK_TABLE_LOCK_REMOVED] = {
        .name = "mysql_innodb_lock_table_lock_removed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of table locks removed from the lock queue",
    },
    [FAM_MYSQL_INNODB_LOCK_TABLE_LOCKS] = {
        .name = "mysql_innodb_lock_table_locks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Current number of table locks on tables",
    },
    [FAM_MYSQL_INNODB_LOCK_ROW_LOCK_CURRENT_WAITS] = {
        .name = "mysql_innodb_lock_row_lock_current_waits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of row locks currently being waited for (innodb_row_lock_current_waits)",
    },
    [FAM_MYSQL_INNODB_LOCK_ROW_LOCK_TIME] = {
        .name = "mysql_innodb_lock_row_lock_time",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time spent in acquiring row locks, in milliseconds (innodb_row_lock_time)",
    },
    [FAM_MYSQL_INNODB_LOCK_ROW_LOCK_TIME_MAX] = {
        .name = "mysql_innodb_lock_row_lock_time_max",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum time to acquire a row lock, in milliseconds (innodb_row_lock_time_max)",
    },
    [FAM_MYSQL_INNODB_LOCK_ROW_LOCK_WAITS] = {
        .name = "mysql_innodb_lock_row_lock_waits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a row lock had to be waited for (innodb_row_lock_waits)",
    },
    [FAM_MYSQL_INNODB_LOCK_ROW_LOCK_TIME_AVG] = {
        .name = "mysql_innodb_lock_row_lock_time_avg",
        .type = METRIC_TYPE_GAUGE,
        .help = "The average time to acquire a row lock, in milliseconds (innodb_row_lock_time_avg)",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_SIZE] = {
        .name = "mysql_innodb_buffer_pool_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Server buffer pool size (all buffer pools) in bytes",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_READS] = {
        .name = "mysql_innodb_buffer_pool_reads",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of reads directly from disk (innodb_buffer_pool_reads)",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_READ_REQUESTS] = {
        .name = "mysql_innodb_buffer_pool_read_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of logical read requests (innodb_buffer_pool_read_requests)",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_WRITE_REQUESTS] = {
        .name = "mysql_innodb_buffer_pool_write_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of write requests (innodb_buffer_pool_write_requests)",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_WAIT_FREE] = {
        .name = "mysql_innodb_buffer_pool_wait_free",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times waited for free buffer (innodb_buffer_pool_wait_free)",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_READ_AHEAD] = {
        .name = "mysql_innodb_buffer_pool_read_ahead",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages read as read ahead (innodb_buffer_pool_read_ahead)",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_READ_AHEAD_EVICTED] = {
        .name = "mysql_innodb_buffer_pool_read_ahead_evicted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Read-ahead pages evicted without being accessed (innodb_buffer_pool_read_ahead_evicted)",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_PAGES_TOTAL] = {
        .name = "mysql_innodb_buffer_pool_pages_total",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total buffer pool size in pages (innodb_buffer_pool_pages_total)",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_PAGES_MISC] = {
        .name = "mysql_innodb_buffer_pool_pages_misc",
        .type = METRIC_TYPE_GAUGE,
        .help = "Buffer pages for misc use such as row locks or the adaptive hash index (innodb_buffer_pool_pages_misc)",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_PAGES_DATA] = {
        .name = "mysql_innodb_buffer_pool_pages_data",
        .type = METRIC_TYPE_GAUGE,
        .help = "Buffer pages containing data (innodb_buffer_pool_pages_data)",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_BYTES_DATA] = {
        .name = "mysql_innodb_buffer_pool_bytes_data",
        .type = METRIC_TYPE_GAUGE,
        .help = "Buffer bytes containing data (innodb_buffer_pool_bytes_data)",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_PAGES_DIRTY] = {
        .name = "mysql_innodb_buffer_pool_pages_dirty",
        .type = METRIC_TYPE_GAUGE,
        .help = "Buffer pages currently dirty (innodb_buffer_pool_pages_dirty)",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_BYTES_DIRTY] = {
        .name = "mysql_innodb_buffer_pool_bytes_dirty",
        .type = METRIC_TYPE_GAUGE,
        .help = "Buffer bytes currently dirty (innodb_buffer_pool_bytes_dirty)",
    },
    [FAM_MYSQL_INNODB_BUFFER_POOL_PAGES_FREE] = {
        .name = "mysql_innodb_buffer_pool_pages_free",
        .type = METRIC_TYPE_GAUGE,
        .help = "Buffer pages currently free (innodb_buffer_pool_pages_free)",
    },
    [FAM_MYSQL_INNODB_BUFFER_PAGES_CREATED] = {
        .name = "mysql_innodb_buffer_pages_created",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages created (innodb_pages_created)",
    },
    [FAM_MYSQL_INNODB_BUFFER_PAGES_WRITTEN] = {
        .name = "mysql_innodb_buffer_pages_written",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages written (innodb_pages_written)",
    },
    [FAM_MYSQL_INNODB_BUFFER_PAGES_READ] = {
        .name = "mysql_innodb_buffer_pages_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages read (innodb_pages_read)",
    },
    [FAM_MYSQL_INNODB_BUFFER_INDEX_SEC_REC_CLUSTER_READS] = {
        .name = "mysql_innodb_buffer_index_sec_rec_cluster_reads",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of secondary record reads triggered cluster read",
    },
    [FAM_MYSQL_INNODB_BUFFER_INDEX_SEC_REC_CLUSTER_READS_AVOIDED] = {
        .name = "mysql_innodb_buffer_index_sec_rec_cluster_reads_avoided",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of secondary record reads avoided triggering cluster read",
    },
    [FAM_MYSQL_INNODB_BUFFER_DATA_READS] = {
        .name = "mysql_innodb_buffer_data_reads",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of data read in bytes (innodb_data_reads)",
    },
    [FAM_MYSQL_INNODB_BUFFER_DATA_WRITTEN] = {
        .name = "mysql_innodb_buffer_data_written",
        .type = METRIC_TYPE_COUNTER,
        .help = "Amount of data written in bytes (innodb_data_written)",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_BATCH_SCANNED] = {
        .name = "mysql_innodb_buffer_flush_batch_scanned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total pages scanned as part of flush batch",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_BATCH_NUM_SCAN] = {
        .name = "mysql_innodb_buffer_flush_batch_num_scan",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times buffer flush list flush is called",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_BATCH_SCANNED_PER_CALL] = {
        .name = "mysql_innodb_buffer_flush_batch_scanned_per_call",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pages scanned per flush batch scan",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_BATCH_TOTAL_PAGES] = {
        .name = "mysql_innodb_buffer_flush_batch_total_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total pages flushed as part of flush batch",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_BATCHES] = {
        .name = "mysql_innodb_buffer_flush_batches",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of flush batches",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_BATCH_PAGES] = {
        .name = "mysql_innodb_buffer_flush_batch_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pages queued as a flush batch",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_NEIGHBOR_TOTAL_PAGES] = {
        .name = "mysql_innodb_buffer_flush_neighbor_total_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total neighbors flushed as part of neighbor flush",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_NEIGHBOR] = {
        .name = "mysql_innodb_buffer_flush_neighbor",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times neighbors flushing is invoked",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_NEIGHBOR_PAGES] = {
        .name = "mysql_innodb_buffer_flush_neighbor_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pages queued as a neighbor batch",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_N_TO_FLUSH_REQUESTED] = {
        .name = "mysql_innodb_buffer_flush_n_to_flush_requested",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages requested for flushing.",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_N_TO_FLUSH_BY_AGE] = {
        .name = "mysql_innodb_buffer_flush_n_to_flush_by_age",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages target by LSN Age for flushing.",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_ADAPTIVE_AVG_TIME] = {
        .name = "mysql_innodb_buffer_flush_adaptive_avg_time",
        .type = METRIC_TYPE_COUNTER,
        .help = "Avg time (ms) spent for adaptive flushing recently.",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_ADAPTIVE_AVG_PASS] = {
        .name = "mysql_innodb_buffer_flush_adaptive_avg_pass",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of adaptive flushes passed during the recent Avg period.",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_GET_FREE_LOOPS] = {
        .name = "mysql_innodb_buffer_LRU_get_free_loops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total loops in LRU get free.",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_GET_FREE_WAITS] = {
        .name = "mysql_innodb_buffer_LRU_get_free_waits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total sleep waits in LRU get free.",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_AVG_PAGE_RATE] = {
        .name = "mysql_innodb_buffer_flush_avg_page_rate",
        .type = METRIC_TYPE_COUNTER,
        .help = "Average number of pages at which flushing is happening",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_LSN_AVG_RATE] = {
        .name = "mysql_innodb_buffer_flush_lsn_avg_rate",
        .type = METRIC_TYPE_COUNTER,
        .help = "Average redo generation rate",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_PCT_FOR_DIRTY] = {
        .name = "mysql_innodb_buffer_flush_pct_for_dirty",
        .type = METRIC_TYPE_COUNTER,
        .help = "Percent of IO capacity used to avoid max dirty page limit",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_PCT_FOR_LSN] = {
        .name = "mysql_innodb_buffer_flush_pct_for_lsn",
        .type = METRIC_TYPE_COUNTER,
        .help = "Percent of IO capacity used to avoid reusable redo space limit",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_SYNC_WAITS] = {
        .name = "mysql_innodb_buffer_flush_sync_waits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a wait happens due to sync flushing",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_ADAPTIVE_TOTAL_PAGES] = {
        .name = "mysql_innodb_buffer_flush_adaptive_total_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total pages flushed as part of adaptive flushing",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_ADAPTIVE] = {
        .name = "mysql_innodb_buffer_flush_adaptive",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of adaptive batches",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_ADAPTIVE_PAGES] = {
        .name = "mysql_innodb_buffer_flush_adaptive_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pages queued as an adaptive batch",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_SYNC_TOTAL_PAGES] = {
        .name = "mysql_innodb_buffer_flush_sync_total_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total pages flushed as part of sync batches",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_SYNC] = {
        .name = "mysql_innodb_buffer_flush_sync",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of sync batches",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_SYNC_PAGES] = {
        .name = "mysql_innodb_buffer_flush_sync_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pages queued as a sync batch",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_BACKGROUND_TOTAL_PAGES] = {
        .name = "mysql_innodb_buffer_flush_background_total_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total pages flushed as part of background batches",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_BACKGROUND] = {
        .name = "mysql_innodb_buffer_flush_background",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of background batches",
    },
    [FAM_MYSQL_INNODB_BUFFER_FLUSH_BACKGROUND_PAGES] = {
        .name = "mysql_innodb_buffer_flush_background_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pages queued as a background batch",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_BATCH_SCANNED] = {
        .name = "mysql_innodb_buffer_LRU_batch_scanned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total pages scanned as part of LRU batch",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_BATCH_NUM_SCAN] = {
        .name = "mysql_innodb_buffer_LRU_batch_num_scan",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times LRU batch is called",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_BATCH_SCANNED_PER_CALL] = {
        .name = "mysql_innodb_buffer_LRU_batch_scanned_per_call",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pages scanned per LRU batch call",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_BATCH_FLUSH_TOTAL_PAGES] = {
        .name = "mysql_innodb_buffer_LRU_batch_flush_total_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total pages flushed as part of LRU batches",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_BATCHES_FLUSH] = {
        .name = "mysql_innodb_buffer_LRU_batches_flush",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of LRU batches",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_BATCH_FLUSH_PAGES] = {
        .name = "mysql_innodb_buffer_LRU_batch_flush_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pages queued as an LRU batch",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_BATCH_EVICT_TOTAL_PAGES] = {
        .name = "mysql_innodb_buffer_LRU_batch_evict_total_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total pages evicted as part of LRU batches",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_BATCHES_EVICT] = {
        .name = "mysql_innodb_buffer_LRU_batches_evict",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of LRU batches",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_BATCH_EVICT_PAGES] = {
        .name = "mysql_innodb_buffer_LRU_batch_evict_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pages queued as an LRU batch",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_SINGLE_FLUSH_FAILURE_COUNT] = {
        .name = "mysql_innodb_buffer_LRU_single_flush_failure_count",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times attempt to flush a single page from LRU failed",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_GET_FREE_SEARCH] = {
        .name = "mysql_innodb_buffer_LRU_get_free_search",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of searches performed for a clean page",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_SEARCH_SCANNED] = {
        .name = "mysql_innodb_buffer_LRU_search_scanned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total pages scanned as part of LRU search",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_SEARCH_NUM_SCAN] = {
        .name = "mysql_innodb_buffer_LRU_search_num_scan",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times LRU search is performed",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_SEARCH_SCANNED_PER_CALL] = {
        .name = "mysql_innodb_buffer_LRU_search_scanned_per_call",
        .type = METRIC_TYPE_COUNTER,
        .help = "Page scanned per single LRU search",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_UNZIP_SEARCH_SCANNED] = {
        .name = "mysql_innodb_buffer_LRU_unzip_search_scanned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total pages scanned as part of LRU unzip search",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_UNZIP_SEARCH_NUM_SCAN] = {
        .name = "mysql_innodb_buffer_LRU_unzip_search_num_scan",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times LRU unzip search is performed",
    },
    [FAM_MYSQL_INNODB_BUFFER_LRU_UNZIP_SEARCH_SCANNED_PER_CALL] = {
        .name = "mysql_innodb_buffer_LRU_unzip_search_scanned_per_call",
        .type = METRIC_TYPE_COUNTER,
        .help = "Page scanned per single LRU unzip search",
    },
    [FAM_MYSQL_INNODB_BUFFER_PAGE_READ] = {
        .name = "mysql_innodb_buffer_page_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of buffer pages read by type.",
    },
    [FAM_MYSQL_INNODB_BUFFER_PAGE_WRITTEN] = {
        .name = "mysql_innodb_buffer_page_written",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of buffer pages written by type.",
    },
    [FAM_MYSQL_INNODB_OS_DATA_READS] = {
        .name = "mysql_innodb_os_data_reads",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of reads initiated (innodb_data_reads)",
    },
    [FAM_MYSQL_INNODB_OS_DATA_WRITES] = {
        .name = "mysql_innodb_os_data_writes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of writes initiated (innodb_data_writes)",
    },
    [FAM_MYSQL_INNODB_OS_DATA_FSYNCS] = {
        .name = "mysql_innodb_os_data_fsyncs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of fsync() calls (innodb_data_fsyncs)",
    },
    [FAM_MYSQL_INNODB_OS_PENDING_READS] = {
        .name = "mysql_innodb_os_pending_reads",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of reads pending",
    },
    [FAM_MYSQL_INNODB_OS_PENDING_WRITES] = {
        .name = "mysql_innodb_os_pending_writes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of writes pending",
    },
    [FAM_MYSQL_INNODB_OS_LOG_WRITTEN_BYTES] = {
        .name = "mysql_innodb_os_log_written_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bytes of log written (innodb_os_log_written)",
    },
    [FAM_MYSQL_INNODB_OS_LOG_FSYNCS] = {
        .name = "mysql_innodb_os_log_fsyncs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of fsync log writes (innodb_os_log_fsyncs)",
    },
    [FAM_MYSQL_INNODB_OS_LOG_PENDING_FSYNCS] = {
        .name = "mysql_innodb_os_log_pending_fsyncs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pending fsync write (innodb_os_log_pending_fsyncs)",
    },
    [FAM_MYSQL_INNODB_OS_LOG_PENDING_WRITES] = {
        .name = "mysql_innodb_os_log_pending_writes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pending log file writes (innodb_os_log_pending_writes)",
    },
    [FAM_MYSQL_INNODB_TRX_RW_COMMITS] = {
        .name = "mysql_innodb_trx_rw_commits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of read-write transactions  committed",
    },
    [FAM_MYSQL_INNODB_TRX_RO_COMMITS] = {
        .name = "mysql_innodb_trx_ro_commits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of read-only transactions committed",
    },
    [FAM_MYSQL_INNODB_TRX_NL_RO_COMMITS] = {
        .name = "mysql_innodb_trx_nl_ro_commits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of non-locking auto-commit read-only transactions committed",
    },
    [FAM_MYSQL_INNODB_TRX_COMMITS_INSERT_UPDATE] = {
        .name = "mysql_innodb_trx_commits_insert_update",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of transactions committed with inserts and updates",
    },
    [FAM_MYSQL_INNODB_TRX_ROLLBACKS] = {
        .name = "mysql_innodb_trx_rollbacks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of transactions rolled back",
    },
    [FAM_MYSQL_INNODB_TRX_ROLLBACKS_SAVEPOINT] = {
        .name = "mysql_innodb_trx_rollbacks_savepoint",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of transactions rolled back to savepoint",
    },
    [FAM_MYSQL_INNODB_TRX_ACTIVE_TRANSACTIONS] = {
        .name = "mysql_innodb_trx_active_transactions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of active transactions",
    },
    [FAM_MYSQL_INNODB_TRX_RSEG_HISTORY_LEN] = {
        .name = "mysql_innodb_trx_rseg_history_len",
        .type = METRIC_TYPE_GAUGE,
        .help = "Length of the TRX_RSEG_HISTORY list",
    },
    [FAM_MYSQL_INNODB_TRX_UNDO_SLOTS_USED] = {
        .name = "mysql_innodb_trx_undo_slots_used",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of undo slots used",
    },
    [FAM_MYSQL_INNODB_TRX_UNDO_SLOTS_CACHED] = {
        .name = "mysql_innodb_trx_undo_slots_cached",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of undo slots cached",
    },
    [FAM_MYSQL_INNODB_TRX_RSEG_CURRENT_SIZE] = {
        .name = "mysql_innodb_trx_rseg_current_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current rollback segment size in pages",
    },
    [FAM_MYSQL_INNODB_PURGE_DEL_MARK_RECORDS] = {
        .name = "mysql_innodb_purge_del_mark_records",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of delete-marked rows purged",
    },
    [FAM_MYSQL_INNODB_PURGE_UPD_EXIST_OR_EXTERN_RECORDS] = {
        .name = "mysql_innodb_purge_upd_exist_or_extern_records",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of purges on updates of existing records and updates on delete marked "
                "record with externally stored field.",
    },
    [FAM_MYSQL_INNODB_PURGE_INVOKED] = {
        .name = "mysql_innodb_purge_invoked",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times purge was invoked.",
    },
    [FAM_MYSQL_INNODB_PURGE_UNDO_LOG_PAGES] = {
        .name = "mysql_innodb_purge_undo_log_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of undo log pages handled by the purge.",
    },
    [FAM_MYSQL_INNODB_PURGE_DML_DELAY_USEC] = {
        .name = "mysql_innodb_purge_dml_delay_usec",
        .type = METRIC_TYPE_GAUGE,
        .help = "Microseconds DML to be delayed due to purge lagging.",
    },
    [FAM_MYSQL_INNODB_PURGE_STOP_COUNT] = {
        .name = "mysql_innodb_purge_stop_count",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of times purge was stopped.",
    },
    [FAM_MYSQL_INNODB_PURGE_RESUME_COUNT] = {
        .name = "mysql_innodb_purge_resume_count",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of times purge was resumed.",
    },
    [FAM_MYSQL_INNODB_LOG_CHECKPOINTS] = {
        .name = "mysql_innodb_log_checkpoints",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of checkpoints.",
    },
    [FAM_MYSQL_INNODB_LOG_LSN_LAST_FLUSH] = {
        .name = "mysql_innodb_log_lsn_last_flush",
        .type = METRIC_TYPE_GAUGE,
        .help = "LSN of Last flush.",
    },
    [FAM_MYSQL_INNODB_LOG_LSN_LAST_CHECKPOINT] = {
        .name = "mysql_innodb_log_lsn_last_checkpoint",
        .type = METRIC_TYPE_GAUGE,
        .help = "LSN at last checkpoint.",
    },
    [FAM_MYSQL_INNODB_LOG_LSN_CURRENT] = {
        .name = "mysql_innodb_log_lsn_current",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current LSN value",
    },
    [FAM_MYSQL_INNODB_LOG_LSN_CHECKPOINT_AGE] = {
        .name = "mysql_innodb_log_lsn_checkpoint_age",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current LSN value minus LSN at last checkpoint",
    },
    [FAM_MYSQL_INNODB_LOG_LSN_BUF_POOL_OLDEST] = {
        .name = "mysql_innodb_log_lsn_buf_pool_oldest",
        .type = METRIC_TYPE_GAUGE,
        .help = "The oldest modified block LSN in the buffer pool",
    },
    [FAM_MYSQL_INNODB_LOG_MAX_MODIFIED_AGE_ASYNC] = {
        .name = "mysql_innodb_log_max_modified_age_async",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum LSN difference; when exceeded, start asynchronous preflush",
    },
    [FAM_MYSQL_INNODB_LOG_PENDING_LOG_FLUSHES] = {
        .name = "mysql_innodb_log_pending_log_flushes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Pending log flushes",
    },
    [FAM_MYSQL_INNODB_LOG_PENDING_CHECKPOINT_WRITES] = {
        .name = "mysql_innodb_log_pending_checkpoint_writes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Pending checkpoints",
    },
    [FAM_MYSQL_INNODB_LOG_NUM_LOG_IO] = {
        .name = "mysql_innodb_log_num_log_io",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of log I/Os",
    },
    [FAM_MYSQL_INNODB_LOG_WAITS] = {
        .name = "mysql_innodb_log_waits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times InnoDB was forced to wait for log writes to be flushed "
                "due to the log buffer being too small.",
    },
    [FAM_MYSQL_INNODB_LOG_WRITE_REQUESTS] = {
        .name = "mysql_innodb_log_write_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of requests to write to the InnoDB redo log.",
    },
    [FAM_MYSQL_INNODB_LOG_WRITES] = {
        .name = "mysql_innodb_log_writes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of writes to the InnoDB redo log.",
    },
    [FAM_MYSQL_INNODB_LOG_PADDED] = {
        .name = "mysql_innodb_log_padded",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bytes of log padded for log write ahead",
    },
    [FAM_MYSQL_INNODB_COMPRESS_PAGES_COMPRESSED] = {
        .name = "mysql_innodb_compress_pages_compressed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages compressed",
    },
    [FAM_MYSQL_INNODB_COMPRESS_PAGES_DECOMPRESSED] = {
        .name = "mysql_innodb_compress_pages_decompressed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages decompressed",
    },
    [FAM_MYSQL_INNODB_COMPRESSION_PAD_INCREMENTS] = {
        .name = "mysql_innodb_compression_pad_increments",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times padding is incremented to avoid compression failures",
    },
    [FAM_MYSQL_INNODB_COMPRESSION_PAD_DECREMENTS] = {
        .name = "mysql_innodb_compression_pad_decrements",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times padding is decremented due to good compressibility",
    },
    [FAM_MYSQL_INNODB_COMPRESS_SAVED] = {
        .name = "mysql_innodb_compress_saved",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes saved by page compression",
    },
    [FAM_MYSQL_INNODB_COMPRESS_PAGES_PAGE_COMPRESSED] = {
        .name = "mysql_innodb_compress_pages_page_compressed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages compressed by page compression",
    },
    [FAM_MYSQL_INNODB_COMPRESS_PAGE_COMPRESSED_TRIM_OP] = {
        .name = "mysql_innodb_compress_page_compressed_trim_op",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of TRIM operation performed by page compression",
    },
    [FAM_MYSQL_INNODB_COMPRESS_PAGES_PAGE_DECOMPRESSED] = {
        .name = "mysql_innodb_compress_pages_page_decompressed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages decompressed by page compression",
    },
    [FAM_MYSQL_INNODB_COMPRESS_PAGES_PAGE_COMPRESSION_ERROR] = {
        .name = "mysql_innodb_compress_pages_page_compression_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of page compression errors",
    },
    [FAM_MYSQL_INNODB_COMPRESS_PAGES_ENCRYPTED] = {
        .name = "mysql_innodb_compress_pages_encrypted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages encrypted",
    },
    [FAM_MYSQL_INNODB_COMPRESS_PAGES_DECRYPTED] = {
        .name = "mysql_innodb_compress_pages_decrypted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages decrypted",
    },
    [FAM_MYSQL_INNODB_INDEX_PAGE_SPLITS] = {
        .name = "mysql_innodb_index_page_splits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of index page splits",
    },
    [FAM_MYSQL_INNODB_INDEX_PAGE_MERGE_ATTEMPTS] = {
        .name = "mysql_innodb_index_page_merge_attempts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of index page merge attempts",
    },
    [FAM_MYSQL_INNODB_INDEX_PAGE_MERGE_SUCCESSFUL] = {
        .name = "mysql_innodb_index_page_merge_successful",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of successful index page merges",
    },
    [FAM_MYSQL_INNODB_INDEX_PAGE_REORG_ATTEMPTS] = {
        .name = "mysql_innodb_index_page_reorg_attempts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of index page reorganization attempts",
    },
    [FAM_MYSQL_INNODB_INDEX_PAGE_REORG_SUCCESSFUL] = {
        .name = "mysql_innodb_index_page_reorg_successful",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of successful index page reorganizations",
    },
    [FAM_MYSQL_INNODB_INDEX_PAGE_DISCARDS] = {
        .name = "mysql_innodb_index_page_discards",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of index pages discarded",
    },
    [FAM_MYSQL_INNODB_ADAPTIVE_HASH_SEARCHES] = {
        .name = "mysql_innodb_adaptive_hash_searches",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of successful searches using Adaptive Hash Index",
    },
    [FAM_MYSQL_INNODB_ADAPTIVE_HASH_SEARCHES_BTREE] = {
        .name = "mysql_innodb_adaptive_hash_searches_btree",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of searches using B-tree on an index search",
    },
    [FAM_MYSQL_INNODB_ADAPTIVE_HASH_PAGES_ADDED] = {
        .name = "mysql_innodb_adaptive_hash_pages_added",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of index pages on which the Adaptive Hash Index is built",
    },
    [FAM_MYSQL_INNODB_ADAPTIVE_HASH_PAGES_REMOVED] = {
        .name = "mysql_innodb_adaptive_hash_pages_removed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of index pages whose corresponding Adaptive Hash Index entries were removed",
    },
    [FAM_MYSQL_INNODB_ADAPTIVE_HASH_ROWS_ADDED] = {
        .name = "mysql_innodb_adaptive_hash_rows_added",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of Adaptive Hash Index rows added",
    },
    [FAM_MYSQL_INNODB_ADAPTIVE_HASH_ROWS_REMOVED] = {
        .name = "mysql_innodb_adaptive_hash_rows_removed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of Adaptive Hash Index rows removed",
    },
    [FAM_MYSQL_INNODB_ADAPTIVE_HASH_ROWS_DELETED_NO_HASH_ENTRY] = {
        .name = "mysql_innodb_adaptive_hash_rows_deleted_no_hash_entry",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rows deleted that did not have corresponding Adaptive Hash Index entries",
    },
    [FAM_MYSQL_INNODB_ADAPTIVE_HASH_ROWS_UPDATED] = {
        .name = "mysql_innodb_adaptive_hash_rows_updated",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of Adaptive Hash Index rows updated",
    },
    [FAM_MYSQL_INNODB_FILE_NUM_OPEN_FILES] = {
        .name = "mysql_innodb_file_num_open_files",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of files currently open (innodb_num_open_files)",
    },
    [FAM_MYSQL_INNODB_IBUF_MERGES_INSERT] = {
        .name = "mysql_innodb_ibuf_merges_insert",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of inserted records merged by change buffering",
    },
    [FAM_MYSQL_INNODB_IBUF_MERGES_DELETE_MARK] = {
        .name = "mysql_innodb_ibuf_merges_delete_mark",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of deleted records merged by change buffering",
    },
    [FAM_MYSQL_INNODB_IBUF_MERGES_DELETE] = {
        .name = "mysql_innodb_ibuf_merges_delete",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of purge records merged by change buffering",
    },
    [FAM_MYSQL_INNODB_IBUF_MERGES_DISCARD_INSERT] = {
        .name = "mysql_innodb_ibuf_merges_discard_insert",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of insert merged operations discarded",
    },
    [FAM_MYSQL_INNODB_IBUF_MERGES_DISCARD_DELETE_MARK] = {
        .name = "mysql_innodb_ibuf_merges_discard_delete_mark",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of deleted merged operations discarded",
    },
    [FAM_MYSQL_INNODB_IBUF_MERGES_DISCARD_DELETE] = {
        .name = "mysql_innodb_ibuf_merges_discard_delete",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of purge merged  operations discarded",
    },
    [FAM_MYSQL_INNODB_IBUF_MERGES] = {
        .name = "mysql_innodb_ibuf_merges",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of change buffer merges",
    },
    [FAM_MYSQL_INNODB_IBUF_SIZE] = {
        .name = "mysql_innodb_ibuf_size",
        .type = METRIC_TYPE_COUNTER,
        .help = "Change buffer size in pages",
    },
    [FAM_MYSQL_INNODB_MASTER_THREAD_SLEEPS] = {
        .name = "mysql_innodb_master_thread_sleeps",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times (seconds) master thread sleeps",
    },
    [FAM_MYSQL_INNODB_ACTIVITY_COUNT] = {
        .name = "mysql_innodb_activity_count",
        .type = METRIC_TYPE_COUNTER,
        .help = "Current server activity count",
    },
    [FAM_MYSQL_INNODB_MASTER_ACTIVE_LOOPS] = {
        .name = "mysql_innodb_master_active_loops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times master thread performs its tasks when server is active",
    },
    [FAM_MYSQL_INNODB_MASTER_IDLE_LOOPS] = {
        .name = "mysql_innodb_master_idle_loops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times master thread performs its tasks when server is idle",
    },
    [FAM_MYSQL_INNODB_BACKGROUND_DROP_TABLE_USEC] = {
        .name = "mysql_innodb_background_drop_table_usec",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time (in microseconds) spent to process drop table list",
    },
    [FAM_MYSQL_INNODB_LOG_FLUSH_USEC] = {
        .name = "mysql_innodb_log_flush_usec",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time (in microseconds) spent to flush log records",
    },
    [FAM_MYSQL_INNODB_DICT_LRU_USEC] = {
        .name = "mysql_innodb_dict_lru_usec",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time (in microseconds) spent to process DICT LRU list",
    },
    [FAM_MYSQL_INNODB_DICT_LRU_COUNT_ACTIVE] = {
        .name = "mysql_innodb_dict_lru_count_active",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of tables evicted from DICT LRU list in the active loop",
    },
    [FAM_MYSQL_INNODB_DICT_LRU_COUNT_IDLE] = {
        .name = "mysql_innodb_dict_lru_count_idle",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of tables evicted from DICT LRU list in the idle loop",
    },
    [FAM_MYSQL_INNODB_DBLWR_WRITES] = {
        .name = "mysql_innodb_dblwr_writes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of doublewrite operations that have been performed (innodb_dblwr_writes)",
    },
    [FAM_MYSQL_INNODB_DBLWR_WRITTEN_PAGES] = {
        .name = "mysql_innodb_dblwr_written_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of pages that have been written for doublewrite operations (innodb_dblwr_pages_written)",
    },
    [FAM_MYSQL_INNODB_PAGE_SIZE] = {
        .name = "mysql_innodb_page_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "InnoDB page size in bytes (innodb_page_size)",
    },
    [FAM_MYSQL_INNODB_RWLOCK_S_SPIN_WAITS] = {
        .name = "mysql_innodb_rwlock_s_spin_waits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rwlock spin waits due to shared latch request",
    },
    [FAM_MYSQL_INNODB_RWLOCK_X_SPIN_WAITS] = {
        .name = "mysql_innodb_rwlock_x_spin_waits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rwlock spin waits due to exclusive latch request",
    },
    [FAM_MYSQL_INNODB_RWLOCK_SX_SPIN_WAITS] = {
        .name = "mysql_innodb_rwlock_sx_spin_waits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rwlock spin waits due to sx latch request",
    },
    [FAM_MYSQL_INNODB_RWLOCK_S_SPIN_ROUNDS] = {
        .name = "mysql_innodb_rwlock_s_spin_rounds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rwlock spin loop rounds due to shared latch request",
    },
    [FAM_MYSQL_INNODB_RWLOCK_X_SPIN_ROUNDS] = {
        .name = "mysql_innodb_rwlock_x_spin_rounds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rwlock spin loop rounds due to exclusive latch request",
    },
    [FAM_MYSQL_INNODB_RWLOCK_SX_SPIN_ROUNDS] = {
        .name = "mysql_innodb_rwlock_sx_spin_rounds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rwlock spin loop rounds due to sx latch request",
    },
    [FAM_MYSQL_INNODB_RWLOCK_S_OS_WAITS] = {
        .name = "mysql_innodb_rwlock_s_os_waits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of OS waits due to shared latch request",
    },
    [FAM_MYSQL_INNODB_RWLOCK_X_OS_WAITS] = {
        .name = "mysql_innodb_rwlock_x_os_waits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of OS waits due to exclusive latch request",
    },
    [FAM_MYSQL_INNODB_RWLOCK_SX_OS_WAITS] = {
        .name = "mysql_innodb_rwlock_sx_os_waits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of OS waits due to sx latch request",
    },
    [FAM_MYSQL_INNODB_DML_READS] = {
        .name = "mysql_innodb_dml_reads",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rows read",
    },
    [FAM_MYSQL_INNODB_DML_INSERTS] = {
        .name = "mysql_innodb_dml_inserts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rows inserted",
    },
    [FAM_MYSQL_INNODB_DML_DELETES] = {
        .name = "mysql_innodb_dml_deletes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rows deleted",
    },
    [FAM_MYSQL_INNODB_DML_UPDATES] = {
        .name = "mysql_innodb_dml_updates",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rows updated",
    },
    [FAM_MYSQL_INNODB_DML_SYSTEM_READS] = {
        .name = "mysql_innodb_dml_system_reads",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of system rows read",
    },
    [FAM_MYSQL_INNODB_DML_SYSTEM_INSERTS] = {
        .name = "mysql_innodb_dml_system_inserts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of system rows inserted",
    },
    [FAM_MYSQL_INNODB_DML_SYSTEM_DELETES] = {
        .name = "mysql_innodb_dml_system_deletes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of system rows deleted",
    },
    [FAM_MYSQL_INNODB_DML_SYSTEM_UPDATES] = {
        .name = "mysql_innodb_dml_system_updates",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of system rows updated",
    },
    [FAM_MYSQL_INNODB_DDL_BACKGROUND_DROP_INDEXES] = {
        .name = "mysql_innodb_ddl_background_drop_indexes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of indexes waiting to be dropped after failed index creation",
    },
    [FAM_MYSQL_INNODB_DDL_BACKGROUND_DROP_TABLES] = {
        .name = "mysql_innodb_ddl_background_drop_tables",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of tables in background drop table list",
    },
    [FAM_MYSQL_INNODB_DDL_ONLINE_CREATE_INDEX] = {
        .name = "mysql_innodb_ddl_online_create_index",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of indexes being created online",
    },
    [FAM_MYSQL_INNODB_DDL_PENDING_ALTER_TABLE] = {
        .name = "mysql_innodb_ddl_pending_alter_table",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of ALTER TABLE, CREATE INDEX, DROP INDEX in progress",
    },
    [FAM_MYSQL_INNODB_DDL_SORT_FILE_ALTER_TABLE] = {
        .name = "mysql_innodb_ddl_sort_file_alter_table",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of sort files created during alter table",
    },
    [FAM_MYSQL_INNODB_DDL_LOG_FILE_ALTER_TABLE] = {
        .name = "mysql_innodb_ddl_log_file_alter_table",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of log files created during alter table",
    },
    [FAM_MYSQL_INNODB_ICP_ATTEMPTS] = {
        .name = "mysql_innodb_icp_attempts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of attempts for index push-down condition checks",
    },
    [FAM_MYSQL_INNODB_ICP_NO_MATCH] = {
        .name = "mysql_innodb_icp_no_match",
        .type = METRIC_TYPE_COUNTER,
        .help = "Index push-down condition does not match",
    },
    [FAM_MYSQL_INNODB_ICP_OUT_OF_RANGE] = {
        .name = "mysql_innodb_icp_out_of_range",
        .type = METRIC_TYPE_COUNTER,
        .help = "Index push-down condition out of range",
    },
    [FAM_MYSQL_INNODB_ICP_MATCH] = {
        .name = "mysql_innodb_icp_match",
        .type = METRIC_TYPE_COUNTER,
        .help = "Index push-down condition matches",
    },
    [FAM_MYSQL_INNODB_CMP_COMPRESS_OPS] = {
        .name = "mysql_innodb_cmp_compress_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a B-tree page of the size PAGE_SIZE has been compressed.",
    },
    [FAM_MYSQL_INNODB_CMP_COMPRESS_OPS_OK] = {
        .name = "mysql_innodb_cmp_compress_ops_ok",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a B-tree page of the size PAGE_SIZE has been successfully compressed.",
    },
    [FAM_MYSQL_INNODB_CMP_COMPRESS_TIME_SECONDS] = {
        .name = "mysql_innodb_cmp_compress_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total time in seconds spent in attempts to compress B-tree pages.",
    },
    [FAM_MYSQL_INNODB_CMP_UNCOMPRESS_OPS] = {
        .name = "mysql_innodb_cmp_uncompress_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a B-tree page of the size PAGE_SIZE has been uncompressed.",
    },
    [FAM_MYSQL_INNODB_CMP_UNCOMPRESS_TIME_SECONDS] = {
        .name = "mysql_innodb_cmp_uncompress_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total time in seconds spent in uncompressing B-tree pages.",
    },
    [FAM_MYSQL_INNODB_CMPMEM_USED_PAGES] = {
        .name = "mysql_innodb_cmpmem_used_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of blocks of the size PAGE_SIZE that are currently in use.",
    },
    [FAM_MYSQL_INNODB_CMPMEM_FREE_PAGES] = {
        .name = "mysql_innodb_cmpmem_free_pages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of blocks of the size PAGE_SIZE that "
                "are currently available for allocation.",
    },
    [FAM_MYSQL_INNODB_CMPMEM_RELOCATION_OPS] = {
        .name = "mysql_innodb_cmpmem_relocation_ops",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a block of the size PAGE_SIZE has been relocated.",
    },
    [FAM_MYSQL_INNODB_CMPMEM_RELOCATION_TIME_SECOND] = {
        .name = "mysql_innodb_cmpmem_relocation_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total time in seconds spent in relocating blocks.",
    },
    [FAM_MYSQL_INNODB_TABLESPACE_FILE_SIZE_BYTES] = {
        .name = "mysql_innodb_tablespace_file_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The apparent size of the file, which represents the maximum size of the file, "
                "uncompressed.",
    },
    [FAM_MYSQL_INNODB_TABLESPACE_ALLOCATED_SIZE_BYTES] = {
        .name = "mysql_innodb_tablespace_allocated_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The actual size of the file, which is the amount of space allocated on disk.",
    },


    [FAM_MYSQL_MYISAM_KEY_BLOCKS_NOT_FLUSHED] = {
        .name = "mysql_myisam_key_blocks_not_flushed",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of key blocks in the MyISAM key cache that have changed "
                "but have not yet been flushed to disk.",
    },
    [FAM_MYSQL_MYISAM_KEY_BLOCKS_UNUSED] = {
        .name = "mysql_myisam_key_blocks_unused",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of unused blocks in the MyISAM key cache.",
    },
    [FAM_MYSQL_MYISAM_KEY_BLOCKS_USED] = {
        .name = "mysql_myisam_key_blocks_used",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of used blocks in the MyISAM key cache.",
    },
    [FAM_MYSQL_MYISAM_KEY_BLOCKS_WARM] = {
        .name = "mysql_myisam_key_blocks_warm",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of cache blocks in the warm list in the MyISAM key cache.",
    },
    [FAM_MYSQL_MYISAM_KEY_READ_REQUESTS] = {
        .name = "mysql_myisam_key_read_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of requests to read a key block from the MyISAM key cache.",
    },
    [FAM_MYSQL_MYISAM_KEY_READS] = {
        .name = "mysql_myisam_key_reads",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of physical reads of a key block from disk into the MyISAM key cache.",
    },
    [FAM_MYSQL_MYISAM_KEY_WRITE_REQUESTS] = {
        .name = "mysql_myisam_key_write_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of requests to write a key block to the MyISAM key cache.",
    },
    [FAM_MYSQL_MYISAM_KEY_WRITES] = {
        .name = "mysql_myisam_key_writes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of physical writes of a key block from the MyISAM key cache to disk.",
    },
    [FAM_MYSQL_PERFORMANCE_SCHEMA_LOST] = {
        .name = "mysql_performance_schema_lost",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of MySQL instrumentations that could not be loaded or created "
                "due to memory constraints.",
    },
    [FAM_MYSQL_QCACHE_FREE_BLOCKS] = {
        .name = "mysql_qcache_free_blocks",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of free memory blocks in the query cache.",
    },
    [FAM_MYSQL_QCACHE_FREE_BYTES] = {
        .name = "mysql_qcache_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The amount of free memory for the query cache.",
    },
    [FAM_MYSQL_QCACHE_HITS] = {
        .name = "mysql_qcache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of query cache hits.",
    },
    [FAM_MYSQL_QCACHE_INSERTS] = {
        .name = "mysql_qcache_inserts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries added to the query cache.",
    },
    [FAM_MYSQL_QCACHE_LOWMEM_PRUNES] = {
        .name = "mysql_qcache_lowmem_prunes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that were deleted from the query cache because of low memory.",
    },
    [FAM_MYSQL_QCACHE_NOT_CACHED] = {
        .name = "mysql_qcache_not_cached",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of noncached queries "
                "(not cacheable, or not cached due to the query_cache_type setting).",
    },
    [FAM_MYSQL_QCACHE_QUERIES_IN_CACHE] = {
        .name = "mysql_qcache_queries_in_cache",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of queries registered in the query cache.",
    },
    [FAM_MYSQL_QCACHE_TOTAL_BLOCKS] = {
        .name = "mysql_qcache_total_blocks",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of blocks in the query cache.",
    },
    [FAM_MYSQL_SLAVE_CONNECTIONS] = {
        .name = "mysql_slave_connections",
        .type = METRIC_TYPE_GAUGE, // XXX
        .help = "Number of REGISTER_SLAVE attempts. "
                "In practice the number of times slaves has tried to connect to the master.",
    },
    [FAM_MYSQL_SLAVE_OPEN_TEMP_TABLES] = {
        .name = "mysql_slave_open_temp_tables",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of temporary tables the slave has open.",
    },
    [FAM_MYSQL_SLAVE_RECEIVED_HEARTBEATS] = {
        .name = "mysql_slave_received_heartbeats",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of heartbeats the slave has received from the master.",
    },
    [FAM_MYSQL_SLAVE_RETRIED_TRANSACTIONS] = {
        .name = "mysql_slave_retried_transactions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the slave has retried transactions since the server started.",
    },
    [FAM_MYSQL_SLAVE_RUNNING] = {
        .name = "mysql_slave_running",
        .type = METRIC_TYPE_GAUGE,
        .help = "Whether the slave is running (both I/O and SQL threads running) or not.",
    },
    [FAM_MYSQL_SLAVE_SKIPPED_ERRORS] = {
        .name = "mysql_slave_skipped_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times a slave has skipped errors.",
    },
    [FAM_MYSQL_SLAVES_CONNECTED] = {
        .name = "mysql_slaves_connected",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of slaves connected.",
    },
    [FAM_MYSQL_SLAVES_RUNNING] = {
        .name = "mysql_slaves_running",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of slave SQL threads running.",
    },
    [FAM_MYSQL_SSL_ACCEPT_RENEGOTIATES] = {
        .name = "mysql_ssl_accept_renegotiates",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of negotiates needed to establish the connection.",
    },
    [FAM_MYSQL_SSL_ACCEPTS] = {
        .name = "mysql_ssl_accepts",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of accepted SSL connections.",
    },
    [FAM_MYSQL_SSL_CALLBACK_CACHE_HITS] = {
        .name = "mysql_ssl_callback_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of callback cache hits.",
    },
    [FAM_MYSQL_SSL_CLIENT_CONNECTS] = {
        .name = "mysql_ssl_client_connects",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SSL connection attempts to an SSL-enabled "
                "replication source server.",
    },
    [FAM_MYSQL_SSL_CONNECT_RENEGOTIATES] = {
        .name = "mysqĺ_ssl_connect_renegotiates",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of negotiates needed to establish the connection "
              "to an SSL-enabled replication source server.",
    },
    [FAM_MYSQL_SSL_CTX_VERIFY_DEPTH] = {
        .name = "mysql_ssl_ctx_verify_depth",
        .type = METRIC_TYPE_GAUGE,
        .help = "The SSL context verification depth (how many certificates in the chain are tested).",
    },
    [FAM_MYSQL_SSL_DEFAULT_TIMEOUT] = {
        .name = "mysql_ssl_default_timeout",
        .type = METRIC_TYPE_GAUGE,
        .help = "The default SSL timeout.",
    },
    [FAM_MYSQL_SSL_FINISHED_ACCEPTS] = {
        .name = "mysql_ssl_finished_accepts",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of successful SSL connections to the server.",
    },
    [FAM_MYSQL_SSL_FINISHED_CONNECTS] = {
        .name = "mysql_ssl_finished_connects",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of successful replica connections to an "
                "SSL-enabled replication source server.",
    },
    [FAM_MYSQL_SSL_SESSION_CACHE_HITS] = {
        .name = "mysq_ssl_session_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SSL session cache hits.",
    },
    [FAM_MYSQL_SSL_SESSION_CACHE_MISSES] = {
        .name = "mysql_ssl_session_cache_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SSL session cache misses.",
    },
    [FAM_MYSQL_SSL_SESSION_CACHE_OVERFLOWS] = {
        .name = "mysql_ssl_session_cache_overflows",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SSL session cache overflows.",
    },
    [FAM_MYSQL_SSL_SESSION_CACHE_SIZE] = {
        .name = "mysql_sl_session_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "The SSL session cache size.",
    },
    [FAM_MYSQL_SSL_SESSION_CACHE_TIMEOUTS] = {
        .name = "mysql_ssl_session_cache_timeouts",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SSL session cache timeouts.",
    },
    [FAM_MYSQL_SSL_SESSIONS_REUSED] = {
        .name = "mysql_ssl_sessions_reused",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many SSL connections were reused from the cache.",
    },
    [FAM_MYSQL_SSL_USED_SESSION_CACHE_ENTRIES] = {
        .name = "mysql_ssl_used_session_cache_entries",
        .type = METRIC_TYPE_GAUGE,
        .help = "How many SSL session cache entries were used.",
    },
    [FAM_MYSQL_WSREP_APPLIER_THREAD_COUNT] = {
        .name = "mysql_wsrep_applier_thread_count",
        .type = METRIC_TYPE_COUNTER,
        .help = "Current number of applier threads.",
    },
    [FAM_MYSQL_WSREP_LOCAL_BF_ABORTS] = {
        .name = "mysql_wsrep_local_bf_aborts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of local transactions aborted by slave transactions "
                "while being executed.",
    },
    [FAM_MYSQL_WSREP_LOCAL_LOCAL_CACHED_DOWNTO] = {
        .name = "mysql_wsrep_local_cached_downto",
        .type = METRIC_TYPE_GAUGE,
        .help = "The lowest sequence number, or seqno, in the write-set cache (GCache).",
    },
    [FAM_MYSQL_WSREP_LOCAL_CERT_FAILURES] = {
        .name = "mysql_wsrep_local_cert_failures",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of local transactions that failed the certification test.",
    },
    [FAM_MYSQL_WSREP_LOCAL_COMMITS] = {
        .name = "mysql_wsrep_local_commits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of local transactions committed on the node.",
    },
    [FAM_MYSQL_WSREP_LOCAL_INDEX] = {
        .name = "mysql_wsrep_local_index",
        .type = METRIC_TYPE_GAUGE,
        .help = "The node's index in the cluster. The index is zero-based.",
    },
    [FAM_MYSQL_WSREP_LOCAL_RECV_QUEUE] = {
        .name = "mysql_wsrep_local_recv_queue",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current length of the receive queue, "
                "which is the number of writesets waiting to be applied."
    },
    [FAM_MYSQL_WSREP_LOCAL_REPLAYS] = {
        .name = "mysql_wsrep_local_replays",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of transaction replays due to asymmetric lock granularity.",
    },
    [FAM_MYSQL_WSREP_LOCAL_SEND_QUEUE] = {
        .name = "mysql_wsrep_local_send_queue",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current length of the send queue, "
                "which is the number of writesets waiting to be sent.",
    },
    [FAM_MYSQL_WSREP_APPLY_OOOE] = {
        .name = "mysql_wsrep_apply_oooe",
        .type = METRIC_TYPE_COUNTER,
        .help = "How often writesets have been applied out of order, "
                "an indicators of parallelization efficiency.",
    },
    [FAM_MYSQL_WSREP_APPLY_OOOL] = {
        .name = "mysql_wsrep_apply_oool",
        .type = METRIC_TYPE_COUNTER,
        .help = "How often writesets with a higher sequence number were applied "
                "before ones with a lower sequence number, implying slow writesets." ,
    },
    [FAM_MYSQL_WSREP_APPLY_WINDOW] = {
        .name = "mysql_wsrep_apply_window",
        .type =  METRIC_TYPE_GAUGE,
        .help = "Average distance between highest and lowest concurrently applied seqno.",
    },
    [FAM_MYSQL_WSREP_CERT_DEPS_DISTANCE] = {
        .name = "mysql_wsrep_cert_deps_distance",
        .type = METRIC_TYPE_GAUGE,
        .help = "Average distance between the highest and the lowest sequence numbers "
                "that can possibly be applied in parallel.",
    },
    [FAM_MYSQL_WSREP_CERT_INDEX_SIZE] = {
        .name = "mysql_wsrep_cert_index_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of entries in the certification index.",
    },
    [FAM_MYSQL_WSREP_CERT_INTERVAL] = {
        .name = "mysql_wsrep_cert_interval",
        .type = METRIC_TYPE_GAUGE,
        .help = "Average number of transactions received while a transaction replicates.",
    },
    [FAM_MYSQL_WSREP_CLUSTER_CONF_ID] = {
        .name = "mysql_wsrep_cluster_conf_id",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of cluster membership changes that have taken place.",
    },
    [FAM_MYSQL_WSREP_CLUSTER_SIZE] = {
        .name = "mysql_wsrep_cluster_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of nodes currently in the cluster.",
    },
    [FAM_MYSQL_WSREP_CLUSTER_WEIGHT] = {
        .name = "mysql_wsrep_cluster_weight",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total weight of the current members in the cluster.",
    },
    [FAM_MYSQL_WSREP_COMMIT_OOOE] = {
        .name = "mysql_wsrep_commit_oooe",
        .type = METRIC_TYPE_COUNTER,
        .help = "How often a transaction was committed out of order.",
    },
    [FAM_MYSQL_WSREP_COMMIT_OOOL] = {
        .name = "mysql_wsrep_commit_oool",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MYSQL_WSREP_COMMIT_WINDOW] = {
        .name = "mysql_wsrep_commit_window",
        .type =  METRIC_TYPE_GAUGE,
        .help = "Average distance between highest and lowest concurrently committed seqno.",
    },
    [FAM_MYSQL_WSREP_CONNECTED] = {
        .name = "mysql_wsrep_connected",
        .type = METRIC_TYPE_GAUGE,
        .help = "Whether or not MariaDB is connected to the wsrep provider.",
    },

    [FAM_MYSQL_WSREP_DESYNC_COUNT] = {
        .name = "mysql_wsrep_desync_count",
        .type = METRIC_TYPE_GAUGE,
        .help = "Returns the number of operations in progress that require "
                "the node to temporarily desync from the cluster.",
    },
    [FAM_MYSQL_WSREP_EVS_DELAYED] = {
        .name = "mysql_wsrep_evs_delayed",
        .help = "Provides a comma separated list of all the nodes "
                "this node has registered on its delayed list.",
    },
    [FAM_MYSQL_WSREP_FLOW_CONTROL_PAUSED] = {
        .name = "mysql_wsrep_flow_control_paused",
        .type = METRIC_TYPE_GAUGE,
        .help = "The fraction of time since the last FLUSH STATUS command that replication was paused due to flow control.",
    },
    [FAM_MYSQL_WSREP_FLOW_CONTROL_PAUSED_SECONDS] = {
        .name = "mysql_wsrep_flow_control_paused_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total time spent in a paused state measured in seconds.",
    },
    [FAM_MYSQL_WSREP_FLOW_CONTROL_RECV] = {
        .name = "mysql_wsrep_flow_control_recv",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of FC_PAUSE events received as well as sent "
                "since the most recent status query.",
    },
    [FAM_MYSQL_WSREP_FLOW_CONTROL_SENT] = {
        .name = "mysql_wsrep_flow_control_sent",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of FC_PAUSE events sent since the most recent status query.",
    },
    [FAM_MYSQL_WSREP_LAST_COMMITTED] = {
        .name = "mysql_wsrep_last_committed",
        .type = METRIC_TYPE_GAUGE,
        .help = "Sequence number of the most recently committed transaction.",
    },
    [FAM_MYSQL_WSREP_OPEN_CONNECTIONS] = {
        .name = "mysql_wsrep_open_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of open connection objects inside the wsrep provider.",
    },
    [FAM_MYSQL_WSREP_OPEN_TRANSACTIONS] = {
        .name = "mysql_wsrep_open_transactions",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of locally running transactions which have been registered "
                "inside the wsrep provider.",
    },
    [FAM_MYSQL_WSREP_RECEIVED] = {
        .name = "mysql_wsrep_received",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of writesets received from other nodes.",
    },
    [FAM_MYSQL_WSREP_RECEIVED_BYTES] = {
        .name = "mysql_wsrep_received_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total size in bytes of all writesets received from other nodes.",
    },
    [FAM_MYSQL_WSREP_REPL_DATA_BYTES] = {
        .name = "mysql_wsrep_repl_data_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total size of data replicated.",
    },
    [FAM_MYSQL_WSREP_REPL_KEYS] = {
        .name = "mysql_wsrep_repl_keys",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of keys replicated.",
    },
    [FAM_MYSQL_WSREP_REPL_KEYS_BYTES] = {
        .name = "mysql_wsrep_repl_keys_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total size of keys replicated.",
    },
    [FAM_MYSQL_WSREP_REPL_OTHER_BYTES] = {
        .name = "mysql_wsrep_repl_other_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total size of other bits replicated.",
    },
    [FAM_MYSQL_WSREP_REPLICATED] = {
        .name = "mysql_wsrep_replicated",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of writesets replicated to other nodes.",
    },
    [FAM_MYSQL_WSREP_REPLICATED_BYTES] = {
        .name = "mysql_wsrep_replicated_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total size in bytes of all writesets replicated to other nodes.",
    },
    [FAM_MYSQL_WSREP_ROLLBACKER_THREAD_COUNT] = {
        .name = "mysql_wsrep_rollbacker_thread_count",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of rollbacker threads.",
    },
    [FAM_MYSQL_WSREP_THREAD_COUNT] = {
        .name = "mysql_wsrep_thread_count",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of wsrep (applier/rollbacker) threads.",
    },

    [FAM_MYSQL_USER_CONNECTIONS] = {
        .name = "mysql_user_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of connections created for this user.",
    },
    [FAM_MYSQL_USER_CONCURRENT_CONNECTIONS] = {
        .name = "mysql_user_concurrent_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of concurrent connections for this user.",
    },
    [FAM_MYSQL_USER_CONNECTED_TIME_SECONDS] = {
        .name = "mysql_user_connected_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The cumulative number of seconds elapsed while there "
                "were connections from this user.",
    },
    [FAM_MYSQL_USER_BUSY_TIME_SECONDS] = {
        .name = "mysql_user_busy_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The cumulative number of seconds there was activity "
                "on connections from this user.",
    },
    [FAM_MYSQL_USER_CPU_TIME] = {
        .name = "mysql_user_cpu_time",
        .type = METRIC_TYPE_COUNTER,
        .help = "The cumulative CPU time elapsed while servicing this user's connections.",
    },
    [FAM_MYSQL_USER_RECEIVED_BYTES] = {
        .name = "mysql_user_received_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bytes received from this user's connections.",
    },
    [FAM_MYSQL_USER_SENT_BYTES] = {
        .name = "mysql_user_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bytes sent to this user's connections.",
    },
    [FAM_MYSQL_USER_BINLOG_WRITTEN_BYTES] = {
        .name = "mysql_user_binlog_written_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bytes written to the binary log from this user's connections.",
    },
    [FAM_MYSQL_USER_READ_ROWS] = {
        .name = "mysql_user_read_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rows read by this user's connections.",
    },
    [FAM_MYSQL_USER_SENT_ROWS] = {
        .name = "mysql_user_sent_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rows sent by this user's connections.",
    },
    [FAM_MYSQL_USER_DELETED_ROWS] = {
        .name = "mysql_user_deleted_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rows deleted by this user's connections.",
    },
    [FAM_MYSQL_USER_INSERTED_ROWS] = {
        .name = "mysql_user_inserted_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rows inserted by this user's connections.",
    },
    [FAM_MYSQL_USER_UPDATED_ROWS] = {
        .name = "mysql_user_updated_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rows updated by this user's connections.",
    },
    [FAM_MYSQL_USER_SELECT_COMMANDS] = {
        .name = "mysql_user_select_commands",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SELECT commands executed from this user's connections.",
    },
    [FAM_MYSQL_USER_UPDATE_COMMANDS] = {
        .name = "mysql_user_update_commands",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of UPDATE commands executed from this user's connections.",
    },
    [FAM_MYSQL_USER_OTHER_COMMANDS] = {
        .name = "mysql_user_other_commands",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of other commands executed from this user's connections.",
    },
    [FAM_MYSQL_USER_COMMIT_TRANSACTIONS] = {
        .name = "mysql_user_commit_transactions",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of COMMIT commands issued by this user's connections.",
    },
    [FAM_MYSQL_USER_ROLLBACK_TRANSACTIONS] = {
        .name = "mysql_user_rollback_transactions",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ROLLBACK commands issued by this user's connections.",
    },
    [FAM_MYSQL_USER_DENIED_CONNECTIONS] = {
        .name = "mysql_user_denied_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of connections denied to this user.",
    },
    [FAM_MYSQL_USER_LOST_CONNECTIONS] = {
        .name = "mysql_user_lost_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of this user's connections that were terminated uncleanly.",
    },
    [FAM_MYSQL_USER_ACCESS_DENIED] = {
        .name = "mysql_user_access_denied",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times this user's connections issued commands that were denied.",
    },
    [FAM_MYSQL_USER_EMPTY_QUERIES] = {
        .name = "mysql_user_empty_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times this user's connections sent empty queries to the server.",
    },
    [FAM_MYSQL_USER_TOTAL_SSL_CONNECTIONS] = {
        .name = "mysql_user_total_ssl_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of TLS connections created for this user.",
    },
    [FAM_MYSQL_USER_MAX_STATEMENT_TIME_EXCEEDED] = {
        .name = "mysql_user_max_statement_time_exceeded",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times a statement was aborted.",
    },
    [FAM_MYSQL_INDEX_ROWS_READ] = {
        .name = "mysql_index_rows_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rows read from this index.",
    },
    [FAM_MYSQL_TABLE_ROWS_READ] = {
        .name = "mysql_table_rows_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rows read from the table.",
    },
    [FAM_MYSQL_TABLE_ROWS_CHANGED] = {
        .name = "mysql_table_rows_changed",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rows changed in the table.",
    },
    [FAM_MYSQL_TABLE_ROWS_CHANGED_X_INDEXES] = {
        .name = "mysql_table_rows_changed_x_indexes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rows changed in the table, "
                "multiplied by the number of indexes changed.",
    },
    [FAM_MYSQL_CLIENT_CONNECTIONS] = {
        .name = "mysql_client_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of connections created for this client.",
    },
    [FAM_MYSQL_CLIENT_CONCURRENT_CONNECTIONS] = {
        .name = "mysql_client_concurrent_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of concurrent connections for this client.",
    },
    [FAM_MYSQL_CLIENT_CONNECTED_TIME_SECONDS] = {
        .name = "mysql_client_connected_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The cumulative number of seconds elapsed while "
                "there were connections from this client.",
    },
    [FAM_MYSQL_CLIENT_BUSY_TIME_SECONDS] = {
        .name = "mysql_client_busy_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The cumulative number of seconds there was activity on "
                "connections from this client.",
    },
    [FAM_MYSQL_CLIENT_CPU_TIME_SECONDS] = {
        .name = "mysql_client_cpu_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The cumulative CPU time elapsed while servicing this client's connections.",
    },
    [FAM_MYSQL_CLIENT_RECEIVED_BYTES] = {
        .name = "mysql_client_received_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bytes received from this client's connections.",
    },
    [FAM_MYSQL_CLIENT_SENT_BYTES] = {
        .name = "mysql_client_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bytes sent to this client's connections.",
    },
    [FAM_MYSQL_CLIENT_BINLOG_WRITTEN_BYTES] = {
        .name = "mysql_client_binlog_written_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bytes written to the binary log from this client's connections.",
    },
    [FAM_MYSQL_CLIENT_READ_ROWS] = {
        .name = "mysql_client_read_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rows read by this client's connections.",
    },
    [FAM_MYSQL_CLIENT_SENT_ROWS] = {
        .name = "mysql_client_sent_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rows sent by this client's connections.",
    },
    [FAM_MYSQL_CLIENT_DELETED_ROWS] = {
        .name = "mysql_client_deleted_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rows deleted by this client's connections.",
    },
    [FAM_MYSQL_CLIENT_INSERTED_ROWS] = {
        .name = "mysql_client_inserted_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rows inserted by this client's connections.",
    },
    [FAM_MYSQL_CLIENT_UPDATED_ROWS] = {
        .name = "mysql_client_updated_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rows updated by this client's connections.",
    },
    [FAM_MYSQL_CLIENT_SELECT_COMMANDS] = {
        .name = "mysql_client_select_commands",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SELECT commands executed from this client's connections.",
    },
    [FAM_MYSQL_CLIENT_UPDATE_COMMANDS] = {
        .name = "mysql_client_update_commands",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of UPDATE commands executed from this client's connections.",
    },
    [FAM_MYSQL_CLIENT_OTHER_COMMANDS] = {
        .name = "mysql_client_other_commands",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of other commands executed from this client's connections.",
    },
    [FAM_MYSQL_CLIENT_COMMIT_TRANSACTIONS] = {
        .name = "mysql_client_commit_transactions",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of COMMIT commands issued by this client's connections.",
    },
    [FAM_MYSQL_CLIENT_ROLLBACK_TRANSACTIONS] = {
        .name = "mysql_client_rollback_transactions",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of ROLLBACK commands issued by this client's connections.",
    },
    [FAM_MYSQL_CLIENT_DENIED_CONNECTIONS] = {
        .name = "mysql_client_denied_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of connections denied to this client.",
    },
    [FAM_MYSQL_CLIENT_LOST_CONNECTIONS] = {
        .name = "mysql_client_lost_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of this client's connections that were terminated uncleanly.",
    },
    [FAM_MYSQL_CLIENT_ACCESS_DENIED] = {
        .name = "mysql_client_access_denied",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times this client's connections issued commands that were denied.",
    },
    [FAM_MYSQL_CLIENT_EMPTY_QUERIES] = {
        .name = "mysql_client_empty_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times this client's connections sent queries "
                "that returned no results to the server.",
    },
    [FAM_MYSQL_CLIENT_SSL_CONNECTIONS] = {
        .name = "mysql_client_ssl_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of TLS connections created for this client.",
    },
    [FAM_MYSQL_CLIENT_MAX_STATEMENT_TIME_EXCEEDED] = {
        .name = "mysql_client_max_statement_time_exceeded",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times a statement was aborted.",
    },
    [FAM_MYSQL_TABLE_DATA_SIZE_BYTES] = {
        .name = "mysql_table_data_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "For MyISAM the size of the data file, in bytes. "
                "For InnoDB the approximate amount of space allocated "
                "for the clustered index, in bytes",
    },
    [FAM_MYSQL_TABLE_INDEX_SIZE_BYTES] = {
        .name = "mysql_table_index_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "For MyISAM is the size of the index file, in bytes."
                "For InnoDB is the approximate amount of space allocated "
                "for non-clustered indexes, in bytes.",
    },
    [FAM_MYSQL_TABLE_DATA_FREE_BYTES] = {
        .name = "mysql_table_data_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of allocated but unused bytes."
    },

    [FAM_MYSQL_QUERY_RESPONSE_TIME_SECONDS] = {
        .name = "mysql_query_response_time_seconds",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "The number of all queries by duration they took to execute.",
    },
    [FAM_MYSQL_READ_QUERY_RESPONSE_TIME_SECONDS] = {
        .name = "mysql_read_query_response_time_seconds",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "The number of read queries by duration they took to execute.",
    },
    [FAM_MYSQL_WRITE_QUERY_RESPONSE_TIME_SECONDS] = {
        .name = "mysql_write_query_response_time_seconds",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "The number of write queries by duration they took to execute.",
    },

    [FAM_MYSQL_HEARTBEAT_DELAY_SECONDS] = {
        .name = "mysql_heartbeat_delay_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Timestamp stored in the heartbeat table.",
    },
};
