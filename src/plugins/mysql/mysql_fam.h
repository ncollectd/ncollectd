/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

enum {
    FAM_MYSQL_UP,

    FAM_MYSQL_ABORTED_CLIENTS,
    FAM_MYSQL_ABORTED_CONNECTS,
    FAM_MYSQL_ABORTED_CONNECTS_PREAUTH,
    FAM_MYSQL_ACCESS_DENIED_ERRORS,
    FAM_MYSQL_BUSY_TIME_SECONDS,
    FAM_MYSQL_RECEIVED_BYTES,
    FAM_MYSQL_SENT_BYTES,
    FAM_MYSQL_CONNECTION_ERRORS,
    FAM_MYSQL_CONNECTIONS,
    FAM_MYSQL_CPU_TIME,
    FAM_MYSQL_STATEMENT_TIME_EXCEEDED,
    FAM_MYSQL_MAX_USED_CONNECTIONS,
    FAM_MYSQL_MEMORY_USED_BYTES,
    FAM_MYSQL_MEMORY_USED_INITIAL_BYTES,
    FAM_MYSQL_OPEN_FILES,
    FAM_MYSQL_OPEN_STREAMS,
    FAM_MYSQL_OPEN_TABLE_DEFINITIONS,
    FAM_MYSQL_OPEN_TABLES,
    FAM_MYSQL_OPENED_FILES,
    FAM_MYSQL_OPENED_PLUGIN_LIBRARIES,
    FAM_MYSQL_OPENED_TABLE_DEFINITIONS,
    FAM_MYSQL_OPENED_TABLES,
    FAM_MYSQL_OPENED_VIEWS,
    FAM_MYSQL_PREPARED_STMT,
    FAM_MYSQL_COLUMN_COMPRESSIONS,
    FAM_MYSQL_COLUMN_DECOMPRESSIONS,
    FAM_MYSQL_QUERIES,
    FAM_MYSQL_QUESTIONS,
    FAM_MYSQL_ROWS_READ,
    FAM_MYSQL_ROWS_SENT,
    FAM_MYSQL_ROWS_TMP_READ,
    FAM_MYSQL_SELECT_FULL_JOIN,
    FAM_MYSQL_SELECT_FULL_RANGE_JOIN,
    FAM_MYSQL_SELECT_RANGE,
    FAM_MYSQL_SELECT_RANGE_CHECK,
    FAM_MYSQL_SELECT_SCAN,
    FAM_MYSQL_SUBQUERY_CACHE_HIT,
    FAM_MYSQL_SUBQUERY_CACHE_MISS,
    FAM_MYSQL_SYNCS,
    FAM_MYSQL_TABLE_LOCKS_IMMEDIATE,
    FAM_MYSQL_TABLE_LOCKS_WAITED,
    FAM_MYSQL_TABLE_OPEN_CACHE_ACTIVE_INSTANCES,
    FAM_MYSQL_TABLE_OPEN_CACHE_HITS,
    FAM_MYSQL_TABLE_OPEN_CACHE_MISSES,
    FAM_MYSQL_TABLE_OPEN_CACHE_OVERFLOWS,
    FAM_MYSQL_TC_LOG_MAX_PAGES_USED,
    FAM_MYSQL_TC_LOG_PAGE_SIZE,
    FAM_MYSQL_TC_LOG_PAGE_WAITS,
    FAM_MYSQL_THREADPOOL_IDLE_THREADS,
    FAM_MYSQL_THREADPOOL_THREADS,
    FAM_MYSQL_THREADS_CACHED,
    FAM_MYSQL_THREADS_CONNECTED,
    FAM_MYSQL_THREADS_CREATED,
    FAM_MYSQL_THREADS_RUNNING,
    FAM_MYSQL_UPDATE_SCAN,
    FAM_MYSQL_UPTIME_SECONDS,
    FAM_MYSQL_UPTIME_SINCE_FLUSH_STATUS_SECONDS,
    FAM_MYSQL_SLOW_LAUNCH_THREADS,
    FAM_MYSQL_SLOW_QUERIES,
    FAM_MYSQL_SORT_MERGE_PASSES,
    FAM_MYSQL_SORT_PRIORITY_QUEUE_SORTS,
    FAM_MYSQL_SORT_RANGE,
    FAM_MYSQL_SORT_ROWS,
    FAM_MYSQL_SORT_SCAN,
    FAM_MYSQL_CREATED_TMP_DISK_TABLES,
    FAM_MYSQL_CREATED_TMP_FILES,
    FAM_MYSQL_CREATED_TMP_TABLES,
    FAM_MYSQL_DELETE_SCAN,
    FAM_MYSQL_EMPTY_QUERIES,
    FAM_MYSQL_EXECUTED_EVENTS,
    FAM_MYSQL_EXECUTED_TRIGGERS,
    FAM_MYSQL_ACL_GRANTS,
    FAM_MYSQL_ACL_PROXY_USERS,
    FAM_MYSQL_ACL_ROLES,
    FAM_MYSQL_ACL_USERS,
    FAM_MYSQL_ARIA_PAGECACHE_BLOCKS_NOT_FLUSHED,
    FAM_MYSQL_ARIA_PAGECACHE_BLOCKS_UNUSED,
    FAM_MYSQL_ARIA_PAGECACHE_BLOCKS_USED,
    FAM_MYSQL_ARIA_PAGECACHE_READ_REQUESTS,
    FAM_MYSQL_ARIA_PAGECACHE_READS,
    FAM_MYSQL_ARIA_PAGECACHE_WRITE_REQUESTS,
    FAM_MYSQL_ARIA_PAGECACHE_WRITES,
    FAM_MYSQL_ARIA_TRANSACTION_LOG_SYNCS,
    FAM_MYSQL_BINLOG_COMMITS,
    FAM_MYSQL_BINLOG_GROUP_COMMITS,
    FAM_MYSQL_BINLOG_GROUP_COMMIT_TRIGGER_COUNT,
    FAM_MYSQL_BINLOG_GROUP_COMMIT_TRIGGER_LOCK_WAIT,
    FAM_MYSQL_BINLOG_GROUP_COMMIT_TRIGGER_TIMEOUT,
    FAM_MYSQL_BINLOG_SNAPSHOT_POSITION,
    FAM_MYSQL_BINLOG_WRITTEN_BYTES,
    FAM_MYSQL_BINLOG_CACHE_DISK_USE,
    FAM_MYSQL_BINLOG_CACHE_USE,
    FAM_MYSQL_BINLOG_STMT_CACHE_DISK_USE,
    FAM_MYSQL_BINLOG_STMT_CACHE_USE,
    FAM_MYSQL_COMMANDS,
    FAM_MYSQL_FEATURE,
    FAM_MYSQL_HANDLERS,
    FAM_MYSQL_INNODB_BUFFER_POOL_DATA_BYTES,
    FAM_MYSQL_INNODB_BUFFER_POOL_DIRTY_BYTES,
    FAM_MYSQL_INNODB_BUFFER_POOL_PAGES,
    FAM_MYSQL_INNODB_BUFFER_POOL_DIRTY_PAGES,
    FAM_MYSQL_INNODB_BUFFER_POOL_PAGE_CHANGES,
    FAM_MYSQL_INNODB_BUFFER_POOL_PAGES_COUNT,
    FAM_MYSQL_INNODB_BUFFER_POOL_READ_AHEAD_RANDOM,
    FAM_MYSQL_INNODB_CHECKPOINT_AGE,
    FAM_MYSQL_INNODB_CHECKPOINT_MAX_AGE,
    FAM_MYSQL_INNODB_DATA_FSYNCS,
    FAM_MYSQL_INNODB_DATA_PENDING_FSYNCS,
    FAM_MYSQL_INNODB_DATA_PENDING_READS,
    FAM_MYSQL_INNODB_DATA_PENDING_WRITES,
    FAM_MYSQL_INNODB_DATA_READ_BYTES,
    FAM_MYSQL_INNODB_DATA_READS,
    FAM_MYSQL_INNODB_DATA_WRITES,
    FAM_MYSQL_INNODB_DATA_WRITTEN_BYTES,
    FAM_MYSQL_INNODB_DEADLOCKS,
    FAM_MYSQL_INNODB_HISTORY_LIST_LENGTH,
    FAM_MYSQL_INNODB_INSERT_BUFFER_DISCARDED_DELETE_MARKS,
    FAM_MYSQL_INNODB_INSERT_BUFFER_DISCARDED_DELETES,
    FAM_MYSQL_INNODB_INSERT_BUFFER_DISCARDED_INSERTS,
    FAM_MYSQL_INNODB_INSERT_BUFFER_MERGED_DELETE_MARKS,
    FAM_MYSQL_INNODB_INSERT_BUFFER_MERGED_DELETES,
    FAM_MYSQL_INNODB_INSERT_BUFFER_MERGED_INSERTS,
    FAM_MYSQL_INNODB_INSERT_BUFFER_MERGES,
    FAM_MYSQL_INNODB_INSERT_BUFFER_FREE_LIST,
    FAM_MYSQL_INNODB_INSERT_BUFFER_SEGMENT_SIZE,
    FAM_MYSQL_INNODB_INSERT_BUFFER_SIZE,
    FAM_MYSQL_INNODB_LOG_SEQUENCE_CURRENT,
    FAM_MYSQL_INNODB_LOG_SEQUENCE_FLUSHED,
    FAM_MYSQL_INNODB_LOG_SEQUENCE_LAST_CHECKPOINT,
    FAM_MYSQL_INNODB_MASTER_THREAD_ACTIVE_LOOPS,
    FAM_MYSQL_INNODB_MASTER_THREAD_IDLE_LOOPS,
    FAM_MYSQL_INNODB_MAX_TRX_ID,
    FAM_MYSQL_INNODB_MEM_ADAPTIVE_HASH,
    FAM_MYSQL_INNODB_MEM_DICTIONARY,
    FAM_MYSQL_INNODB_PAGE_SIZE_BYTES,
    FAM_MYSQL_INNODB_PAGES_CREATED,
    FAM_MYSQL_INNODB_PAGES_READ,
    FAM_MYSQL_INNODB_PAGES_WRITTEN,
    FAM_MYSQL_INNODB_ROW_OPS,
    FAM_MYSQL_INNODB_SYSTEM_ROWS_OPS,
    FAM_MYSQL_INNODB_NUM_OPEN_FILES,
    FAM_MYSQL_INNODB_TRUNCATED_STATUS_WRITES,
    FAM_MYSQL_INNODB_AVAILABLE_UNDO_LOGS,
    FAM_MYSQL_INNODB_UNDO_TRUNCATIONS,
    FAM_MYSQL_INNODB_PAGE_COMPRESSION_SAVED_BYTES,
    FAM_MYSQL_INNODB_NUM_INDEX_PAGES_WRITTEN,
    FAM_MYSQL_INNODB_NUM_NON_INDEX_PAGES_WRITTEN,
    FAM_MYSQL_INNODB_NUM_PAGES_PAGE_COMPRESSED,
    FAM_MYSQL_INNODB_NUM_PAGE_COMPRESSED_TRIM_OP,
    FAM_MYSQL_INNODB_NUM_PAGES_PAGE_DECOMPRESSED,
    FAM_MYSQL_INNODB_NUM_PAGES_PAGE_COMPRESSION_ERROR,
    FAM_MYSQL_INNODB_NUM_PAGES_ENCRYPTED,
    FAM_MYSQL_INNODB_NUM_PAGES_DECRYPTED,
    FAM_MYSQL_INNODB_DEFRAGMENT_COMPRESSION_FAILURES,
    FAM_MYSQL_INNODB_DEFRAGMENT_FAILURES,
    FAM_MYSQL_INNODB_DEFRAGMENT_COUNT,
    FAM_MYSQL_INNODB_INSTANT_ALTER_COLUMN,
    FAM_MYSQL_INNODB_ONLINEDDL_ROWLOG_ROWS,
    FAM_MYSQL_INNODB_ONLINEDDL_ROWLOG_PCT_USED,
    FAM_MYSQL_INNODB_ONLINEDDL_PCT_PROGRESS,
    FAM_MYSQL_INNODB_ENCRYPTION_ROTATION_PAGES_READ_FROM_CACHE,
    FAM_MYSQL_INNODB_ENCRYPTION_ROTATION_PAGES_READ_FROM_DISK,
    FAM_MYSQL_INNODB_ENCRYPTION_ROTATION_PAGES_MODIFIED,
    FAM_MYSQL_INNODB_ENCRYPTION_ROTATION_PAGES_FLUSHED,
    FAM_MYSQL_INNODB_ENCRYPTION_ROTATION_ESTIMATED_IOPS,
    FAM_MYSQL_INNODB_ENCRYPTION_KEY_ROTATION_LIST_LENGTH,
    FAM_MYSQL_INNODB_ENCRYPTION_N_MERGE_BLOCKS_ENCRYPTED,
    FAM_MYSQL_INNODB_ENCRYPTION_N_MERGE_BLOCKS_DECRYPTED,
    FAM_MYSQL_INNODB_ENCRYPTION_N_ROWLOG_BLOCKS_ENCRYPTED,
    FAM_MYSQL_INNODB_ENCRYPTION_N_ROWLOG_BLOCKS_DECRYPTED,
    FAM_MYSQL_INNODB_ENCRYPTION_N_TEMP_BLOCKS_ENCRYPTED,
    FAM_MYSQL_INNODB_ENCRYPTION_N_TEMP_BLOCKS_DECRYPTED,
    FAM_MYSQL_INNODB_ENCRYPTION_NUM_KEY_REQUESTS,

    FAM_MYSQL_INNODB_METADATA_TABLE_HANDLES_OPENED,
    FAM_MYSQL_INNODB_METADATA_TABLE_HANDLES_CLOSED,
    FAM_MYSQL_INNODB_METADATA_TABLE_REFERENCE_COUNT,
    FAM_MYSQL_INNODB_LOCK_DEADLOCKS,
    FAM_MYSQL_INNODB_LOCK_TIMEOUTS,
    FAM_MYSQL_INNODB_LOCK_REC_LOCK_WAITS,
    FAM_MYSQL_INNODB_LOCK_TABLE_LOCK_WAITS,
    FAM_MYSQL_INNODB_LOCK_REC_LOCK_REQUESTS,
    FAM_MYSQL_INNODB_LOCK_REC_LOCK_CREATED,
    FAM_MYSQL_INNODB_LOCK_REC_LOCK_REMOVED,
    FAM_MYSQL_INNODB_LOCK_REC_LOCKS,
    FAM_MYSQL_INNODB_LOCK_TABLE_LOCK_CREATED,
    FAM_MYSQL_INNODB_LOCK_TABLE_LOCK_REMOVED,
    FAM_MYSQL_INNODB_LOCK_TABLE_LOCKS,
    FAM_MYSQL_INNODB_LOCK_ROW_LOCK_CURRENT_WAITS,
    FAM_MYSQL_INNODB_LOCK_ROW_LOCK_TIME,
    FAM_MYSQL_INNODB_LOCK_ROW_LOCK_TIME_MAX,
    FAM_MYSQL_INNODB_LOCK_ROW_LOCK_WAITS,
    FAM_MYSQL_INNODB_LOCK_ROW_LOCK_TIME_AVG,
    FAM_MYSQL_INNODB_BUFFER_POOL_SIZE,
    FAM_MYSQL_INNODB_BUFFER_POOL_READS,
    FAM_MYSQL_INNODB_BUFFER_POOL_READ_REQUESTS,
    FAM_MYSQL_INNODB_BUFFER_POOL_WRITE_REQUESTS,
    FAM_MYSQL_INNODB_BUFFER_POOL_WAIT_FREE,
    FAM_MYSQL_INNODB_BUFFER_POOL_READ_AHEAD,
    FAM_MYSQL_INNODB_BUFFER_POOL_READ_AHEAD_EVICTED,
    FAM_MYSQL_INNODB_BUFFER_POOL_PAGES_ALL,
    FAM_MYSQL_INNODB_BUFFER_POOL_PAGES_MISC,
    FAM_MYSQL_INNODB_BUFFER_POOL_PAGES_DATA,
    FAM_MYSQL_INNODB_BUFFER_POOL_BYTES_DATA,
    FAM_MYSQL_INNODB_BUFFER_POOL_PAGES_DIRTY,
    FAM_MYSQL_INNODB_BUFFER_POOL_BYTES_DIRTY,
    FAM_MYSQL_INNODB_BUFFER_POOL_PAGES_FREE,
    FAM_MYSQL_INNODB_BUFFER_PAGES_CREATED,
    FAM_MYSQL_INNODB_BUFFER_PAGES_WRITTEN,
    FAM_MYSQL_INNODB_BUFFER_PAGES_READ,
    FAM_MYSQL_INNODB_BUFFER_INDEX_SEC_REC_CLUSTER_READS,
    FAM_MYSQL_INNODB_BUFFER_INDEX_SEC_REC_CLUSTER_READS_AVOIDED,
    FAM_MYSQL_INNODB_BUFFER_DATA_READS,
    FAM_MYSQL_INNODB_BUFFER_DATA_WRITTEN,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_BATCH_SCANNED,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_BATCH_NUM_SCAN,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_BATCH_SCANNED_PER_CALL,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_BATCH_TOTAL_PAGES,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_BATCHES,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_BATCH_PAGES,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_NEIGHBOR_TOTAL_PAGES,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_NEIGHBOR,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_NEIGHBOR_PAGES,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_N_TO_FLUSH_REQUESTED,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_N_TO_FLUSH_BY_AGE,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_ADAPTIVE_AVG_TIME,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_ADAPTIVE_AVG_PASS,
    FAM_MYSQL_INNODB_BUFFER_LRU_GET_FREE_LOOPS,
    FAM_MYSQL_INNODB_BUFFER_LRU_GET_FREE_WAITS,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_AVG_PAGE_RATE,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_LSN_AVG_RATE,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_PCT_FOR_DIRTY,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_PCT_FOR_LSN,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_SYNC_WAITS,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_ADAPTIVE_TOTAL_PAGES,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_ADAPTIVE,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_ADAPTIVE_PAGES,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_SYNC_TOTAL_PAGES,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_SYNC,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_SYNC_PAGES,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_BACKGROUND_TOTAL_PAGES,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_BACKGROUND,
    FAM_MYSQL_INNODB_BUFFER_FLUSH_BACKGROUND_PAGES,
    FAM_MYSQL_INNODB_BUFFER_LRU_BATCH_SCANNED,
    FAM_MYSQL_INNODB_BUFFER_LRU_BATCH_NUM_SCAN,
    FAM_MYSQL_INNODB_BUFFER_LRU_BATCH_SCANNED_PER_CALL,
    FAM_MYSQL_INNODB_BUFFER_LRU_BATCH_FLUSH_TOTAL_PAGES,
    FAM_MYSQL_INNODB_BUFFER_LRU_BATCHES_FLUSH,
    FAM_MYSQL_INNODB_BUFFER_LRU_BATCH_FLUSH_PAGES,
    FAM_MYSQL_INNODB_BUFFER_LRU_BATCH_EVICT_TOTAL_PAGES,
    FAM_MYSQL_INNODB_BUFFER_LRU_BATCHES_EVICT,
    FAM_MYSQL_INNODB_BUFFER_LRU_BATCH_EVICT_PAGES,
    FAM_MYSQL_INNODB_BUFFER_LRU_SINGLE_FLUSH_FAILURE_COUNT,
    FAM_MYSQL_INNODB_BUFFER_LRU_GET_FREE_SEARCH,
    FAM_MYSQL_INNODB_BUFFER_LRU_SEARCH_SCANNED,
    FAM_MYSQL_INNODB_BUFFER_LRU_SEARCH_NUM_SCAN,
    FAM_MYSQL_INNODB_BUFFER_LRU_SEARCH_SCANNED_PER_CALL,
    FAM_MYSQL_INNODB_BUFFER_LRU_UNZIP_SEARCH_SCANNED,
    FAM_MYSQL_INNODB_BUFFER_LRU_UNZIP_SEARCH_NUM_SCAN,
    FAM_MYSQL_INNODB_BUFFER_LRU_UNZIP_SEARCH_SCANNED_PER_CALL,
    FAM_MYSQL_INNODB_BUFFER_PAGE_READ,
    FAM_MYSQL_INNODB_BUFFER_PAGE_WRITTEN,
    FAM_MYSQL_INNODB_OS_DATA_READS,
    FAM_MYSQL_INNODB_OS_DATA_WRITES,
    FAM_MYSQL_INNODB_OS_DATA_FSYNCS,
    FAM_MYSQL_INNODB_OS_PENDING_READS,
    FAM_MYSQL_INNODB_OS_PENDING_WRITES,
    FAM_MYSQL_INNODB_OS_LOG_WRITTEN_BYTES,
    FAM_MYSQL_INNODB_OS_LOG_FSYNCS,
    FAM_MYSQL_INNODB_OS_LOG_PENDING_FSYNCS,
    FAM_MYSQL_INNODB_OS_LOG_PENDING_WRITES,
    FAM_MYSQL_INNODB_TRX_RW_COMMITS,
    FAM_MYSQL_INNODB_TRX_RO_COMMITS,
    FAM_MYSQL_INNODB_TRX_NL_RO_COMMITS,
    FAM_MYSQL_INNODB_TRX_COMMITS_INSERT_UPDATE,
    FAM_MYSQL_INNODB_TRX_ROLLBACKS,
    FAM_MYSQL_INNODB_TRX_ROLLBACKS_SAVEPOINT,
    FAM_MYSQL_INNODB_TRX_ACTIVE_TRANSACTIONS,
    FAM_MYSQL_INNODB_TRX_RSEG_HISTORY_LEN,
    FAM_MYSQL_INNODB_TRX_UNDO_SLOTS_USED,
    FAM_MYSQL_INNODB_TRX_UNDO_SLOTS_CACHED,
    FAM_MYSQL_INNODB_TRX_RSEG_CURRENT_SIZE,
    FAM_MYSQL_INNODB_PURGE_DEL_MARK_RECORDS,
    FAM_MYSQL_INNODB_PURGE_UPD_EXIST_OR_EXTERN_RECORDS,
    FAM_MYSQL_INNODB_PURGE_INVOKED,
    FAM_MYSQL_INNODB_PURGE_UNDO_LOG_PAGES,
    FAM_MYSQL_INNODB_PURGE_DML_DELAY_USEC,
    FAM_MYSQL_INNODB_PURGE_STOP_COUNT,
    FAM_MYSQL_INNODB_PURGE_RESUME_COUNT,
    FAM_MYSQL_INNODB_LOG_CHECKPOINTS,
    FAM_MYSQL_INNODB_LOG_LSN_LAST_FLUSH,
    FAM_MYSQL_INNODB_LOG_LSN_LAST_CHECKPOINT,
    FAM_MYSQL_INNODB_LOG_LSN_CURRENT,
    FAM_MYSQL_INNODB_LOG_LSN_CHECKPOINT_AGE,
    FAM_MYSQL_INNODB_LOG_LSN_BUF_POOL_OLDEST,
    FAM_MYSQL_INNODB_LOG_MAX_MODIFIED_AGE_ASYNC,
    FAM_MYSQL_INNODB_LOG_PENDING_LOG_FLUSHES,
    FAM_MYSQL_INNODB_LOG_PENDING_CHECKPOINT_WRITES,
    FAM_MYSQL_INNODB_LOG_NUM_LOG_IO,
    FAM_MYSQL_INNODB_LOG_WAITS,
    FAM_MYSQL_INNODB_LOG_WRITE_REQUESTS,
    FAM_MYSQL_INNODB_LOG_WRITES,
    FAM_MYSQL_INNODB_LOG_PADDED,
    FAM_MYSQL_INNODB_COMPRESS_PAGES_COMPRESSED,
    FAM_MYSQL_INNODB_COMPRESS_PAGES_DECOMPRESSED,
    FAM_MYSQL_INNODB_COMPRESSION_PAD_INCREMENTS,
    FAM_MYSQL_INNODB_COMPRESSION_PAD_DECREMENTS,
    FAM_MYSQL_INNODB_COMPRESS_SAVED,
    FAM_MYSQL_INNODB_COMPRESS_PAGES_PAGE_COMPRESSED,
    FAM_MYSQL_INNODB_COMPRESS_PAGE_COMPRESSED_TRIM_OP,
    FAM_MYSQL_INNODB_COMPRESS_PAGES_PAGE_DECOMPRESSED,
    FAM_MYSQL_INNODB_COMPRESS_PAGES_PAGE_COMPRESSION_ERROR,
    FAM_MYSQL_INNODB_COMPRESS_PAGES_ENCRYPTED,
    FAM_MYSQL_INNODB_COMPRESS_PAGES_DECRYPTED,
    FAM_MYSQL_INNODB_INDEX_PAGE_SPLITS,
    FAM_MYSQL_INNODB_INDEX_PAGE_MERGE_ATTEMPTS,
    FAM_MYSQL_INNODB_INDEX_PAGE_MERGE_SUCCESSFUL,
    FAM_MYSQL_INNODB_INDEX_PAGE_REORG_ATTEMPTS,
    FAM_MYSQL_INNODB_INDEX_PAGE_REORG_SUCCESSFUL,
    FAM_MYSQL_INNODB_INDEX_PAGE_DISCARDS,
    FAM_MYSQL_INNODB_ADAPTIVE_HASH_SEARCHES,
    FAM_MYSQL_INNODB_ADAPTIVE_HASH_SEARCHES_BTREE,
    FAM_MYSQL_INNODB_ADAPTIVE_HASH_PAGES_ADDED,
    FAM_MYSQL_INNODB_ADAPTIVE_HASH_PAGES_REMOVED,
    FAM_MYSQL_INNODB_ADAPTIVE_HASH_ROWS_ADDED,
    FAM_MYSQL_INNODB_ADAPTIVE_HASH_ROWS_REMOVED,
    FAM_MYSQL_INNODB_ADAPTIVE_HASH_ROWS_DELETED_NO_HASH_ENTRY,
    FAM_MYSQL_INNODB_ADAPTIVE_HASH_ROWS_UPDATED,
    FAM_MYSQL_INNODB_FILE_NUM_OPEN_FILES,
    FAM_MYSQL_INNODB_IBUF_MERGES_INSERT,
    FAM_MYSQL_INNODB_IBUF_MERGES_DELETE_MARK,
    FAM_MYSQL_INNODB_IBUF_MERGES_DELETE,
    FAM_MYSQL_INNODB_IBUF_MERGES_DISCARD_INSERT,
    FAM_MYSQL_INNODB_IBUF_MERGES_DISCARD_DELETE_MARK,
    FAM_MYSQL_INNODB_IBUF_MERGES_DISCARD_DELETE,
    FAM_MYSQL_INNODB_IBUF_MERGES,
    FAM_MYSQL_INNODB_IBUF_SIZE,
    FAM_MYSQL_INNODB_MASTER_THREAD_SLEEPS,
    FAM_MYSQL_INNODB_ACTIVITY_COUNT,
    FAM_MYSQL_INNODB_MASTER_ACTIVE_LOOPS,
    FAM_MYSQL_INNODB_MASTER_IDLE_LOOPS,
    FAM_MYSQL_INNODB_BACKGROUND_DROP_TABLE_USEC,
    FAM_MYSQL_INNODB_LOG_FLUSH_USEC,
    FAM_MYSQL_INNODB_DICT_LRU_USEC,
    FAM_MYSQL_INNODB_DICT_LRU_COUNT_ACTIVE,
    FAM_MYSQL_INNODB_DICT_LRU_COUNT_IDLE,
    FAM_MYSQL_INNODB_DBLWR_WRITES,
    FAM_MYSQL_INNODB_DBLWR_WRITTEN_PAGES,
    FAM_MYSQL_INNODB_PAGE_SIZE,
    FAM_MYSQL_INNODB_RWLOCK_S_SPIN_WAITS,
    FAM_MYSQL_INNODB_RWLOCK_X_SPIN_WAITS,
    FAM_MYSQL_INNODB_RWLOCK_SX_SPIN_WAITS,
    FAM_MYSQL_INNODB_RWLOCK_S_SPIN_ROUNDS,
    FAM_MYSQL_INNODB_RWLOCK_X_SPIN_ROUNDS,
    FAM_MYSQL_INNODB_RWLOCK_SX_SPIN_ROUNDS,
    FAM_MYSQL_INNODB_RWLOCK_S_OS_WAITS,
    FAM_MYSQL_INNODB_RWLOCK_X_OS_WAITS,
    FAM_MYSQL_INNODB_RWLOCK_SX_OS_WAITS,
    FAM_MYSQL_INNODB_DML_READS,
    FAM_MYSQL_INNODB_DML_INSERTS,
    FAM_MYSQL_INNODB_DML_DELETES,
    FAM_MYSQL_INNODB_DML_UPDATES,
    FAM_MYSQL_INNODB_DML_SYSTEM_READS,
    FAM_MYSQL_INNODB_DML_SYSTEM_INSERTS,
    FAM_MYSQL_INNODB_DML_SYSTEM_DELETES,
    FAM_MYSQL_INNODB_DML_SYSTEM_UPDATES,
    FAM_MYSQL_INNODB_DDL_BACKGROUND_DROP_INDEXES,
    FAM_MYSQL_INNODB_DDL_BACKGROUND_DROP_TABLES,
    FAM_MYSQL_INNODB_DDL_ONLINE_CREATE_INDEX,
    FAM_MYSQL_INNODB_DDL_PENDING_ALTER_TABLE,
    FAM_MYSQL_INNODB_DDL_SORT_FILE_ALTER_TABLE,
    FAM_MYSQL_INNODB_DDL_LOG_FILE_ALTER_TABLE,
    FAM_MYSQL_INNODB_ICP_ATTEMPTS,
    FAM_MYSQL_INNODB_ICP_NO_MATCH,
    FAM_MYSQL_INNODB_ICP_OUT_OF_RANGE,
    FAM_MYSQL_INNODB_ICP_MATCH,
    FAM_MYSQL_INNODB_CMP_COMPRESS_OPS,
    FAM_MYSQL_INNODB_CMP_COMPRESS_OPS_OK,
    FAM_MYSQL_INNODB_CMP_COMPRESS_TIME_SECONDS,
    FAM_MYSQL_INNODB_CMP_UNCOMPRESS_OPS,
    FAM_MYSQL_INNODB_CMP_UNCOMPRESS_TIME_SECONDS,
    FAM_MYSQL_INNODB_CMPMEM_USED_PAGES,
    FAM_MYSQL_INNODB_CMPMEM_FREE_PAGES,
    FAM_MYSQL_INNODB_CMPMEM_RELOCATION_OPS,
    FAM_MYSQL_INNODB_CMPMEM_RELOCATION_TIME_SECOND,
    FAM_MYSQL_INNODB_TABLESPACE_FILE_SIZE_BYTES,
    FAM_MYSQL_INNODB_TABLESPACE_ALLOCATED_SIZE_BYTES,

    FAM_MYSQL_MYISAM_KEY_BLOCKS_NOT_FLUSHED,
    FAM_MYSQL_MYISAM_KEY_BLOCKS_UNUSED,
    FAM_MYSQL_MYISAM_KEY_BLOCKS_USED,
    FAM_MYSQL_MYISAM_KEY_BLOCKS_WARM,
    FAM_MYSQL_MYISAM_KEY_READ_REQUESTS,
    FAM_MYSQL_MYISAM_KEY_READS,
    FAM_MYSQL_MYISAM_KEY_WRITE_REQUESTS,
    FAM_MYSQL_MYISAM_KEY_WRITES,
    FAM_MYSQL_PERFORMANCE_SCHEMA_LOST,
    FAM_MYSQL_QCACHE_FREE_BLOCKS,
    FAM_MYSQL_QCACHE_FREE_BYTES,
    FAM_MYSQL_QCACHE_HITS,
    FAM_MYSQL_QCACHE_INSERTS,
    FAM_MYSQL_QCACHE_LOWMEM_PRUNES,
    FAM_MYSQL_QCACHE_NOT_CACHED,
    FAM_MYSQL_QCACHE_QUERIES_IN_CACHE,
    FAM_MYSQL_QCACHE_TOTAL_BLOCKS,
    FAM_MYSQL_SLAVE_CONNECTIONS,
    FAM_MYSQL_SLAVE_OPEN_TEMP_TABLES,
    FAM_MYSQL_SLAVE_RECEIVED_HEARTBEATS,
    FAM_MYSQL_SLAVE_RETRIED_TRANSACTIONS,
    FAM_MYSQL_SLAVE_RUNNING,
    FAM_MYSQL_SLAVE_SKIPPED_ERRORS,
    FAM_MYSQL_SLAVES_CONNECTED,
    FAM_MYSQL_SLAVES_RUNNING,
    FAM_MYSQL_SSL_ACCEPT_RENEGOTIATES,
    FAM_MYSQL_SSL_ACCEPTS,
    FAM_MYSQL_SSL_CALLBACK_CACHE_HITS,
    FAM_MYSQL_SSL_CLIENT_CONNECTS,
    FAM_MYSQL_SSL_CONNECT_RENEGOTIATES,
    FAM_MYSQL_SSL_CTX_VERIFY_DEPTH,
    FAM_MYSQL_SSL_DEFAULT_TIMEOUT,
    FAM_MYSQL_SSL_FINISHED_ACCEPTS,
    FAM_MYSQL_SSL_FINISHED_CONNECTS,
    FAM_MYSQL_SSL_SESSION_CACHE_HITS,
    FAM_MYSQL_SSL_SESSION_CACHE_MISSES,
    FAM_MYSQL_SSL_SESSION_CACHE_OVERFLOWS,
    FAM_MYSQL_SSL_SESSION_CACHE_SIZE,
    FAM_MYSQL_SSL_SESSION_CACHE_TIMEOUTS,
    FAM_MYSQL_SSL_SESSIONS_REUSED,
    FAM_MYSQL_SSL_USED_SESSION_CACHE_ENTRIES,
    FAM_MYSQL_SSL_VERIFY_DEPTH,
    FAM_MYSQL_WSREP_APPLIER_THREAD_COUNT,
    FAM_MYSQL_WSREP_LOCAL_BF_ABORTS,
    FAM_MYSQL_WSREP_LOCAL_LOCAL_CACHED_DOWNTO,
    FAM_MYSQL_WSREP_LOCAL_CERT_FAILURES,
    FAM_MYSQL_WSREP_LOCAL_COMMITS,
    FAM_MYSQL_WSREP_LOCAL_INDEX,
    FAM_MYSQL_WSREP_LOCAL_RECV_QUEUE,
    FAM_MYSQL_WSREP_LOCAL_REPLAYS,
    FAM_MYSQL_WSREP_LOCAL_SEND_QUEUE,
    FAM_MYSQL_WSREP_APPLY_OOOE,
    FAM_MYSQL_WSREP_APPLY_OOOL,
    FAM_MYSQL_WSREP_APPLY_WINDOW,
    FAM_MYSQL_WSREP_CERT_DEPS_DISTANCE,
    FAM_MYSQL_WSREP_CERT_INDEX_SIZE,
    FAM_MYSQL_WSREP_CERT_INTERVAL,
    FAM_MYSQL_WSREP_CLUSTER_CONF_ID,
    FAM_MYSQL_WSREP_CLUSTER_SIZE,
    FAM_MYSQL_WSREP_CLUSTER_WEIGHT,
    FAM_MYSQL_WSREP_COMMIT_OOOE,
    FAM_MYSQL_WSREP_COMMIT_OOOL,
    FAM_MYSQL_WSREP_COMMIT_WINDOW,
    FAM_MYSQL_WSREP_CONNECTED,
    FAM_MYSQL_WSREP_DESYNC_COUNT,
    FAM_MYSQL_WSREP_EVS_DELAYED,
    FAM_MYSQL_WSREP_FLOW_CONTROL_PAUSED,
    FAM_MYSQL_WSREP_FLOW_CONTROL_PAUSED_SECONDS,
    FAM_MYSQL_WSREP_FLOW_CONTROL_RECV,
    FAM_MYSQL_WSREP_FLOW_CONTROL_SENT,
    FAM_MYSQL_WSREP_LAST_COMMITTED,
    FAM_MYSQL_WSREP_OPEN_CONNECTIONS,
    FAM_MYSQL_WSREP_OPEN_TRANSACTIONS,
    FAM_MYSQL_WSREP_RECEIVED,
    FAM_MYSQL_WSREP_RECEIVED_BYTES,
    FAM_MYSQL_WSREP_REPL_DATA_BYTES,
    FAM_MYSQL_WSREP_REPL_KEYS,
    FAM_MYSQL_WSREP_REPL_KEYS_BYTES,
    FAM_MYSQL_WSREP_REPL_OTHER_BYTES,
    FAM_MYSQL_WSREP_REPLICATED,
    FAM_MYSQL_WSREP_REPLICATED_BYTES,
    FAM_MYSQL_WSREP_ROLLBACKER_THREAD_COUNT,
    FAM_MYSQL_WSREP_THREAD_COUNT,

    FAM_MYSQL_CLIENT_CONNECTIONS,
    FAM_MYSQL_CLIENT_CONCURRENT_CONNECTIONS,
    FAM_MYSQL_CLIENT_CONNECTED_TIME_SECONDS,
    FAM_MYSQL_CLIENT_BUSY_TIME_SECONDS,
    FAM_MYSQL_CLIENT_CPU_TIME_SECONDS,
    FAM_MYSQL_CLIENT_RECEIVED_BYTES,
    FAM_MYSQL_CLIENT_SENT_BYTES,
    FAM_MYSQL_CLIENT_BINLOG_WRITTEN_BYTES,
    FAM_MYSQL_CLIENT_READ_ROWS,
    FAM_MYSQL_CLIENT_SENT_ROWS,
    FAM_MYSQL_CLIENT_DELETED_ROWS,
    FAM_MYSQL_CLIENT_INSERTED_ROWS,
    FAM_MYSQL_CLIENT_UPDATED_ROWS,
    FAM_MYSQL_CLIENT_SELECT_COMMANDS,
    FAM_MYSQL_CLIENT_UPDATE_COMMANDS,
    FAM_MYSQL_CLIENT_OTHER_COMMANDS,
    FAM_MYSQL_CLIENT_COMMIT_TRANSACTIONS,
    FAM_MYSQL_CLIENT_ROLLBACK_TRANSACTIONS,
    FAM_MYSQL_CLIENT_DENIED_CONNECTIONS,
    FAM_MYSQL_CLIENT_LOST_CONNECTIONS,
    FAM_MYSQL_CLIENT_ACCESS_DENIED,
    FAM_MYSQL_CLIENT_EMPTY_QUERIES,
    FAM_MYSQL_CLIENT_SSL_CONNECTIONS,
    FAM_MYSQL_CLIENT_MAX_STATEMENT_TIME_EXCEEDED,
    FAM_MYSQL_USER_CONNECTIONS,
    FAM_MYSQL_USER_CONCURRENT_CONNECTIONS,
    FAM_MYSQL_USER_CONNECTED_TIME_SECONDS,
    FAM_MYSQL_USER_BUSY_TIME_SECONDS,
    FAM_MYSQL_USER_CPU_TIME,
    FAM_MYSQL_USER_RECEIVED_BYTES,
    FAM_MYSQL_USER_SENT_BYTES,
    FAM_MYSQL_USER_BINLOG_WRITTEN_BYTES,
    FAM_MYSQL_USER_READ_ROWS,
    FAM_MYSQL_USER_SENT_ROWS,
    FAM_MYSQL_USER_DELETED_ROWS,
    FAM_MYSQL_USER_INSERTED_ROWS,
    FAM_MYSQL_USER_UPDATED_ROWS,
    FAM_MYSQL_USER_SELECT_COMMANDS,
    FAM_MYSQL_USER_UPDATE_COMMANDS,
    FAM_MYSQL_USER_OTHER_COMMANDS,
    FAM_MYSQL_USER_COMMIT_TRANSACTIONS,
    FAM_MYSQL_USER_ROLLBACK_TRANSACTIONS,
    FAM_MYSQL_USER_DENIED_CONNECTIONS,
    FAM_MYSQL_USER_LOST_CONNECTIONS,
    FAM_MYSQL_USER_ACCESS_DENIED,
    FAM_MYSQL_USER_EMPTY_QUERIES,
    FAM_MYSQL_USER_TOTAL_SSL_CONNECTIONS,
    FAM_MYSQL_USER_MAX_STATEMENT_TIME_EXCEEDED,
    FAM_MYSQL_TABLE_ROWS_READ,
    FAM_MYSQL_TABLE_ROWS_CHANGED,
    FAM_MYSQL_TABLE_ROWS_CHANGED_X_INDEXES,
    FAM_MYSQL_INDEX_ROWS_READ,
    FAM_MYSQL_TABLE_DATA_SIZE_BYTES,
    FAM_MYSQL_TABLE_INDEX_SIZE_BYTES,
    FAM_MYSQL_TABLE_DATA_FREE_BYTES,

    FAM_MYSQL_QUERY_RESPONSE_TIME_SECONDS,
    FAM_MYSQL_READ_QUERY_RESPONSE_TIME_SECONDS,
    FAM_MYSQL_WRITE_QUERY_RESPONSE_TIME_SECONDS,

    FAM_MYSQL_HEARTBEAT_DELAY_SECONDS,

    FAM_MYSQL_STATUS_MAX
};
