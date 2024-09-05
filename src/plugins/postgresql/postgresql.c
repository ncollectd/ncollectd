// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2008-2012 Sebastian Harl
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"
#include "libutils/complain.h"
#include "libdbquery/dbquery.h"

#include <libpq-fe.h>
#include <pg_config_manual.h>

#include "pg_fam.h"
#include "pg_stats.h"

static metric_family_t pg_fams[FAM_PG_MAX] = {
    [FAM_PG_UP] = {
        .name = "pg_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the PostgreSQL server be reached.",
    },
    [FAM_PG_DATABASE_BACKENDS] = {
        .name = "pg_database_backends",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of backends currently connected to this database.",
    },
    [FAM_PG_DATABASE_XACT_COMMIT] = {
        .name = "pg_database_xact_commit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of transactions in this database that have been committed",
    },
    [FAM_PG_DATABASE_XACT_ROLLBACK] = {
        .name = "pg_database_xact_rollback",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of transactions in this database that have been rolled back.",
    },
    [FAM_PG_DATABASE_BLKS_READ] = {
        .name = "pg_database_blks_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of disk blocks read in this database.",
    },
    [FAM_PG_DATABASE_BLKS_HIT] = {
        .name = "pg_database_blks_hit",
        .help = "Number of times disk blocks were found already in the buffer cache.",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_PG_DATABASE_RETURNED_ROWS] = {
        .name = "pg_database_returned_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rows returned by queries in this database.",
    },
    [FAM_PG_DATABASE_FETCHED_ROWS] = {
        .name = "pg_database_fetched_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rows fetched by queries in this database.",
    },
    [FAM_PG_DATABASE_INSERTED_ROWS] = {
        .name = "pg_database_inserted_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rows inserted by queries in this database.",
    },
    [FAM_PG_DATABASE_UPDATED_ROWS] = {
        .name = "pg_database_updated_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rows updated by queries in this database.",
    },
    [FAM_PG_DATABASE_DELETED_ROWS] = {
        .name = "pg_database_deleted_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rows deleted by queries in this database.",
    },
    [FAM_PG_DATABASE_CONFLICTS] = {
        .name = "pg_database_conflicts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries canceled due to conflicts with recovery in this database.",
    },
    [FAM_PG_DATABASE_TEMP_FILES] = {
        .name = "pg_database_temp_files",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of temporary files created by queries in this database.",
    },
    [FAM_PG_DATABASE_TEMP_BYTES] = {
        .name = "pg_database_temp_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total amount of data written to temporary files by queries in this database.",
    },
    [FAM_PG_DATABASE_DEADLOCKS] = {
        .name = "pg_database_deadlocks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of deadlocks detected in this database.",
    },
    [FAM_PG_DATABASE_CHECKSUM_FAILURES] = {
        .name = "pg_database_checksum_failures",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of data page checksum failures detected in this database",
    },
    [FAM_PG_DATABASE_CHECKSUM_LAST_FAILURE] = {
        .name = "pg_database_checksum_last_failure",
        .type = METRIC_TYPE_GAUGE,
        .help = "Timestamp with time zone Time at which the last data page checksum failure "
                "was detected in this database (or on a shared object).",
    },
    [FAM_PG_DATABASE_BLK_READ_TIME_SECONDS] = {
        .name = "pg_database_blk_read_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time spent reading data file blocks by backends in this database, in seconds.",
    },
    [FAM_PG_DATABASE_BLK_WRITE_TIME_SECONDS] = {
        .name = "pg_database_blk_write_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time spent writing data file blocks by backends in this database, in seconds."
    },
    [FAM_PG_DATABASE_SESSION_TIME_SECONDS] = {
        .name = "pg_database_session_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time spent by database sessions in this database, in seconds.",
    },
    [FAM_PG_DATABASE_ACTIVE_TIME_SECONDS] = {
        .name = "pg_database_active_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time spent executing SQL statements in this database, in seconds.",
    },
    [FAM_PG_DATABASE_IDLE_IN_TRANSACTION_TIME_SECONDS] = {
        .name = "pg_database_idle_in_transaction_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time spent idling while in a transaction in this database, in seconds.",
    },
    [FAM_PG_DATABASE_SESSIONS] = {
        .name = "pg_database_sessions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of sessions established to this database.",
    },
    [FAM_PG_DATABASE_SESSIONS_ABANDONED ] = {
        .name = "pg_database_sessions_abandoned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of database sessions to this database that "
                "were terminated because connection to the client was lost.",
    },
    [FAM_PG_DATABASE_SESSIONS_FATAL] = {
        .name = "pg_database_ sessions_fatal",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of database sessions to this database that "
                "were terminated by fatal errors.",
    },
    [FAM_PG_DATABASE_SESSIONS_KILLED] = {
        .name = "pg_database_sessions_killed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of database sessions to this database that "
                "were terminated by operator intervention",
    },
    [FAM_PG_DATABASE_SIZE_BYTES] = {
        .name = "pg_database_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Disk space used by the database.",
    },
    [FAM_PG_DATABASE_LOCKS] = {
        .name = "pg_database_locks",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of locks in the database.",
    },
    [FAM_PG_DATABASE_CONFLICTS_TABLESPACE] = {
        .name = "pg_database_conflicts_tablespace",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries in this database that have been canceled "
                "due to dropped tablespaces.",
    },
    [FAM_PG_DATABASE_CONFLICTS_LOCK] = {
        .name = "pg_database_conflicts_lock",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries in this database that have been canceled due to lock timeouts.",
    },
    [FAM_PG_DATABASE_CONFLICTS_SNAPSHOT] = {
        .name = "pg_database_conflicts_snapshot",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries in this database that have been canceled due to old snapshots.",
    },
    [FAM_PG_DATABASE_CONFLICTS_BUFFERPIN] = {
        .name = "pg_database_conflicts_bufferpin",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries in this database that have been canceled due to pinned buffers.",
    },
    [FAM_PG_DATABASE_CONFLICTS_DEADLOCK] = {
        .name = "pg_database_conflicts_deadlock",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries in this database that have been canceled due to deadlocks.",
    },
    [FAM_PG_TABLE_SEQ_SCAN] = {
        .name = "pg_table_seq_scan",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of sequential scans initiated on this table.",
    },
    [FAM_PG_TABLE_SEQ_READ_ROWS] = {
        .name = "pg_table_seq_read_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of live rows fetched by sequential scans.",
    },
    [FAM_PG_TABLE_IDX_SCAN] = {
        .name = "pg_table_idx_scan",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of index scans initiated on this table.",
    },
    [FAM_PG_TABLE_IDX_FETCH_ROWS] = {
        .name = "pg_table_idx_fetch_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of live rows fetched by index scans.",
    },
    [FAM_PG_TABLE_INSERTED_ROWS] = {
        .name = "pg_table_inserted_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rows inserted.",
    },
    [FAM_PG_TABLE_UPDATED_ROWS] = {
        .name = "pg_table_updated_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rows updated (includes HOT updated rows).",
    },
    [FAM_PG_TABLE_DELETED_ROWS] = {
        .name = "pg_table_deleted_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rows deleted.",
    },
    [FAM_PG_TABLE_HOT_UPDATED_ROWS] = {
        .name = "pg_table_hot_updated_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rows HOT updated (i.e., with no separate index update required).",
    },
    [FAM_PG_TABLE_LIVE_ROWS] = {
        .name = "pg_table_live_rows",
        .type = METRIC_TYPE_GAUGE,
        .help = "Estimated number of live rows.",
    },
    [FAM_PG_TABLE_DEAD_ROWS] = {
        .name = "pg_table_dead_rows",
        .type = METRIC_TYPE_GAUGE,
        .help = "Estimated number of dead rows.",
    },
    [FAM_PG_TABLE_MODIFIED_SINCE_ANALYZE_ROWS] = {
        .name = "pg_table_modified_since_analyze_rows",
        .type = METRIC_TYPE_GAUGE,
        .help = "Estimated number of rows modified since this table was last analyzed.",
    },
    [FAM_PG_TABLE_INSERTED_SINCE_VACUUM_ROWS] = {
        .name = "pg_table_inserted_since_vacuum_rows",
        .type = METRIC_TYPE_GAUGE,
        .help = "Estimated number of rows inserted since this table was last vacuumed.",
    },
    [FAM_PG_TABLE_LAST_VACUUM_SECONDS] = {
        .name = "pg_table_last_vacuum_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Last time at which this table was manually vacuumed (not counting VACUUM FULL).",
    },
    [FAM_PG_TABLE_LAST_AUTOVACUUM_SECONDS] = {
        .name = "pg_table_last_autovacuum_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Last time at which this table was vacuumed by the autovacuum daemon.",
    },
    [FAM_PG_TABLE_LAST_ANALYZE_SECONDS] = {
        .name = "pg_table_last_analyze_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Last time at which this table was manually analyzed.",
    },
    [FAM_PG_TABLE_LAST_AUTOANALYZE_SECONDS] = {
        .name = "pg_table_last_autoanalyze_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Last time at which this table was analyzed by the autovacuum daemon.",
    },
    [FAM_PG_TABLE_VACUUM] = {
        .name = "pg_table_vacuum",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of times this table has been manually vacuumed (not counting VACUUM FULL).",
    },
    [FAM_PG_TABLE_AUTOVACUUM] = {
        .name = "pg_table_autovacuum",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times this table has been vacuumed by the autovacuum daemon.",
    },
    [FAM_PG_TABLE_ANALYZE] = {
        .name = "pg_table_analyze",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times this table has been manually analyzed.",
    },
    [FAM_PG_TABLE_AUTOANALYZE] = {
        .name = "pg_table_autoanalyze",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times this table has been analyzed by the autovacuum daemon.",
    },
    [FAM_PG_TABLE_HEAP_READ_BLOCKS] = {
        .name = "pg_table_heap_read_blocks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of disk blocks read from this table.",
    },
    [FAM_PG_TABLE_HEAP_HIT_BLOCKS] = {
        .name = "pg_table_heap_hit_blocks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of buffer hits in this table.",
    },
    [FAM_PG_TABLE_IDX_READ_BLOCKS] = {
        .name = "pg_table_idx_read_blocks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of disk blocks read from all indexes on this table.",
    },
    [FAM_PG_TABLE_IDX_HIT_BLOCKS] = {
        .name = "pg_table_idx_hit_blocks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of buffer hits in all indexes on this table.",
    },
    [FAM_PG_TABLE_TOAST_READ_BLOCKS] = {
        .name = "pg_table_toast_read_blocks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of disk blocks read from this table's TOAST table (if any).",
    },
    [FAM_PG_TABLE_TOAST_HIT_BLOCKS] = {
        .name = "pg_table_toast_hit_blocks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of buffer hits in this table's TOAST table (if any).",
    },
    [FAM_PG_TABLE_TIDX_READ_BLOCKS] = {
        .name = "pg_table_tidx_read_blocks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of disk blocks read from this table's TOAST table indexes (if any).",
    },
    [FAM_PG_TABLE_TIDX_HIT_BLOCKS] = {
        .name = "pg_table_tidx_hit_blocks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of buffer hits in this table's TOAST table indexes (if any).",
    },
    [FAM_PG_TABLE_LAST_SEQ_SCAN_SECONDS] = {
        .name = "pg_table_last_seq_scan_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "The time of the last sequential scan on this table, "
                "based on the most recent transaction stop time.",
    },
    [FAM_PG_TABLE_LAST_IDX_SCAN_SECONDS] = {
        .name = "pg_table_last_idx_scan_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "The time of the last index scan on this table, "
                "based on the most recent transaction stop time.",
    },
    [FAM_PG_TABLE_NEWPAGE_UPDATED_ROWS] = {
        .name = "pg_table_newpage_updated_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of rows updated where the successor version goes onto a new heap page, "
                "leaving behind an original version with a t_ctid field that points "
                "to a different heap page. These are always non-HOT updates.",
    },
    [FAM_PG_TABLE_SIZE_BYTES] = {
        .name = "pg_table_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "On-disk size of the table in bytes.",
    },
    [FAM_PG_TABLE_INDEXES_SIZE_BYTES] = {
        .name = "pg_table_indexes_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "On-disk size of the table's indexes in bytes.",
    },
    [FAM_PG_FUNCTION_CALLS] = {
        .name = "pg_function_calls",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times this function has been called.",
    },
    [FAM_PG_FUNCTION_TOTAL_TIME_SECONDS] = {
        .name = "pg_function_total_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total time spent in this function and all other functions "
                "called by it, in seconds.",
    },
    [FAM_PG_FUNCTION_SELF_TIME_SECONDS] = {
        .name = "pg_function_self_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total time spent in this function itself, not including other functions "
                "called by it, in seconds.",
    },
    [FAM_PG_INDEX_IDX_SCAN] = {
        .name = "pg_index_idx_scan",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of index scans initiated on this index.",
    },
    [FAM_PG_INDEX_IDX_READ_ROWS] = {
        .name = "pg_index_idx_read_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of index entries returned by scans on this index.",
    },
    [FAM_PG_INDEX_IDX_FETCH_ROWS] = {
        .name = "pg_index_idx_fetch_rows",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of live table rows fetched by simple index scans using this index.",
    },
    [FAM_PG_INDEX_IDX_READ_BLOCKS] = {
        .name = "pg_index_idx_read_blocks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of disk blocks read from this index.",
    },
    [FAM_PG_INDEX_IDX_HIT_BLOCKS] = {
        .name = "pg_index_idx_hit_blocks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of buffer hits in this index.",
    },
    [FAM_PG_SEQUENCES_READ_BLOCKS] = {
        .name = "pg_sequences_read_blocks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of disk blocks read from this sequence.",
    },
    [FAM_PG_SEQUENCES_HIT_BLOCKS] = {
        .name = "pg_sequences_hit_blocks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of buffer hits in this sequence.",
    },
    [FAM_PG_ACTIVITY_CONNECTIONS] = {
        .name = "pg_activity_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of connections in this state",
    },
    [FAM_PG_ACTIVITY_MAX_TX_SECONDS] = {
        .name = "pg_activity_max_tx_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Max duration in seconds any active transaction has been running",
    },
    [FAM_PG_REPLICATION_CURRENT_WAL_LSN] = {
        .name = "pg_replication_current_wal_lsn",
        .type = METRIC_TYPE_GAUGE,
        .help = "WAL position.",
    },
    [FAM_PG_REPLICATION_CURRENT_WAL_LSN_BYTES] = {
        .name = "pg_replication_current_wal_lsn_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "WAL position in bytes.",
    },
    [FAM_PG_REPLICATION_WAL_LSN_DIFF] = {
        .name = "pg_replication_wal_lsn_diff",
        .type = METRIC_TYPE_GAUGE,
        .help = "Lag in bytes between master and slave.",
    },
    [FAM_PG_REPLICATION_SLOT_ACTIVE] = {
        .name = "pg_replication_slot_active",
        .type = METRIC_TYPE_GAUGE,
        .help = "Flag indicating if the slot is active.",
    },
    [FAM_PG_REPLICATION_SLOT_WAL_LSN_DIFF_BYTES] = {
        .name = "pg_replication_slot_wal_lsn_diff_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Replication lag in bytes.",
    },
    [FAM_PG_ARCHIVER_ARCHIVED] = {
        .name = "pg_archiver_archived",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of WAL files that have been successfully archive.",
    },
    [FAM_PG_ARCHIVER_FAILED] = {
        .name = "pg_archiver_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of failed attempts for archiving WAL files.",
    },
    [FAM_PG_ARCHIVER_LAST_ARCHIVE_AGE_SECONDS] = {
        .name = "pg_archiver_last_archive_age_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Time in seconds since last WAL segment was successfully archived.",
    },
    [FAM_PG_SLRU_BLOCKS_ZEROED] = {
        .name = "pg_slru_blocks_zeroed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of blocks zeroed during initializations.",
    },
    [FAM_PG_SLRU_BLOCKS_HIT] = {
        .name = "pg_slru_blocks_hit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times disk blocks were found already in the SLRU.",
    },
    [FAM_PG_SLRU_BLOCKS_READ] = {
        .name = "pg_slru_blocks_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of disk blocks read for this SLRU.",
    },
    [FAM_PG_SLRU_BLOCKS_WRITTEN] = {
        .name = "pg_slru_blocks_written",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of disk blocks written for this SLRU.",
    },
    [FAM_PG_SLRU_BLOCKS_EXISTS] = {
        .name = "pg_slru_blocks_exists",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of blocks checked for existence for this SLRU.",
    },
    [FAM_PG_SLRU_FLUSHES] = {
        .name = "pg_slru_flushes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of flushes of dirty data for this SLRU.",
    },
    [FAM_PG_SLRU_TRUNCATES] = {
        .name = "pg_slru_truncates",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of truncates for this SLRU.",
    },
    [FAM_PG_CHECKPOINTS_TIMED] = {
        .name = "pg_checkpoints_timed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of scheduled checkpoints that have been performed.",
    },
    [FAM_PG_CHECKPOINTS_REQ] = {
        .name = "pg_checkpoints_req",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of requested checkpoints that have been performed.",
    },
    [FAM_PG_CHECKPOINT_WRITE_TIME_SECONDS] = {
        .name = "pg_checkpoint_write_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total amount of time in seconds that has been spent in the portion "
                "of checkpoint processing where files are written to disk.",
    },
    [FAM_PG_CHECKPOINT_SYNC_TIME_SECONDS] = {
        .name = "pg_checkpoint_sync_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total amount of in seconds time that has been spent in the portion "
                "of checkpoint processing where files are synchronized to disk.",
    },
    [FAM_PG_CHECKPOINT_BUFFERS] = {
        .name = "pg_checkpoint_buffers",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of buffers written during checkpoints.",
    },
    [FAM_PG_BGWRITER_BUFFERS_CLEAN] = {
        .name = "pg_bgwriter_buffers_clean",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of buffers written by the background writer.",
    },
    [FAM_PG_BGWRITER_MAXWRITTEN_CLEAN] = {
        .name = "pg_bgwriter_maxwritten_clean",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the background writer stopped a cleaning scan because "
                "it had written too many buffers.",
    },
    [FAM_PG_BGWRITER_BUFFERS_BACKEND] = {
        .name = "pg_bgwriter_buffers_backend",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of buffers written directly by a backend.",
    },
    [FAM_PG_BGWRITER_BUFFERS_BACKEND_FSYNC] = {
        .name = "pg_bgwriter_buffers_backend_fsync",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a backend had to execute its own fsync call "
                "(normally the background writer handles those even when "
                "the backend does its own write).",
    },
    [FAM_PG_BGWRITER_BUFFERS_ALLOC] = {
        .name = "pg_bgwriter_buffers_alloc",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of buffers allocated.",
    },
    [FAM_PG_IO_READ_BYTES] = {
        .name = "pg_io_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of read bytes.",
    },
    [FAM_PG_IO_READ_TIME_SECONDS] = {
        .name = "pg_io_read_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total time spent in read operations in seconds.",
    },
    [FAM_PG_IO_WRITE_BYTES] = {
        .name = "pg_io_write_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help= "Total number of write bytes",
    },
    [FAM_PG_IO_WRITE_TIME_SECONDS] = {
        .name = "pg_io_write_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help= "Total time spent in write operations in seconds.",
    },
    [FAM_PG_IO_WRITEBACKS_BYTES] = {
        .name = "pg_io_writebacks_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help= "Total bytes which the process requested the kernel write out to permanent storage.",
    },
    [FAM_PG_IO_WRITEBACKS_TIME_SECONDS] = {
        .name = "pg_io_writebacks_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help= "Total time spent in writeback operations in seconds.",
    },
    [FAM_PG_IO_EXTENDS_BYTES] = {
        .name = "pg_io_extends_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help= "Total number of bytes in relation extend operations.",
    },
    [FAM_PG_IO_EXTENDS_TIME_SECONDS] = {
        .name = "pg_io_extends_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help= "Total time spent in extend operations in seconds.",
    },
    [FAM_PG_IO_HITS] = {
        .name = "pg_io_hits",
        .type = METRIC_TYPE_COUNTER,
        .help= "Total number of times a desired block was found in a shared buffer.",
    },
    [FAM_PG_IO_EVICTIONS] = {
        .name = "pg_io_evictions",
        .type = METRIC_TYPE_COUNTER,
        .help= "Total number of times a block has been written out from a shared or local buffer "
               "in order to make it available for another use.",
    },
    [FAM_PG_IO_REUSES] = {
        .name = "pg_io_reuses",
        .type = METRIC_TYPE_COUNTER,
        .help= "Total the number of times an existing buffer in a size-limited ring buffer "
               "outside of shared buffers was reused as part of an I/O operation "
               "in the bulkread, bulkwrite, or vacuum contexts.",
    },
    [FAM_PG_IO_FSYNCS] = {
        .name = "pg_io_fsyncs",
        .type = METRIC_TYPE_COUNTER,
        .help= "Total number of fsync calls. These are only tracked in context normal.",
    },
    [FAM_PG_IO_FSYNCS_TIME_SECONDS] = {
        .name = "pg_io_fsyncs_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help= "Time spent in fsync operations in seconds.",
    },
};

/* Appends the (parameter, value) pair to the string pointed to by 'buf' suitable
 * to be used as argument for PQconnectdb(). If value equals NULL, the pair is ignored. */
/* Returns the tuple (major, minor, patchlevel) for the given version number. */
#define C_PSQL_SERVER_VERSION3(server_version)                               \
    (server_version) / 10000,                                                \
            (server_version) / 100 - (int)((server_version) / 10000) * 100,  \
            (server_version) - (int)((server_version) / 100) * 100

/* Returns true if the given host specifies a UNIX domain socket. */
#define C_PSQL_IS_UNIX_DOMAIN_SOCKET(host)                       \
    (((host) == NULL) || (*(host) == '\0') || (*(host) == '/'))

/* Returns the tuple (host, delimiter, port) for a given (host, port) pair.
 * Depending on the value of 'host' a UNIX domain socket or a TCP socket is assumed. */
#define C_PSQL_SOCKET3(host, port)                                          \
    (((host) == NULL) || (*(host) == '\0')) ? DEFAULT_PGSOCKET_DIR : host,  \
            C_PSQL_IS_UNIX_DOMAIN_SOCKET(host) ? "/.s.PGSQL." : ":", port

typedef enum {
    COLLECT_DATABASE           = (1 <<  0),
    COLLECT_DATABASE_SIZE      = (1 <<  1),
    COLLECT_DATABASE_LOCKS     = (1 <<  2),
    COLLECT_DATABASE_CONFLICTS = (1 <<  3),
    COLLECT_TABLE              = (1 <<  4),
    COLLECT_TABLE_IO           = (1 <<  5),
    COLLECT_TABLE_SIZE         = (1 <<  6),
    COLLECT_INDEXES            = (1 <<  7),
    COLLECT_INDEXES_IO         = (1 <<  8),
    COLLECT_SEQUENCES_IO       = (1 <<  9),
    COLLECT_FUNCTIONS          = (1 <<  10),
    COLLECT_ACTIVITY           = (1 <<  11),
    COLLECT_REPLICATION_SLOTS  = (1 <<  12),
    COLLECT_REPLICATION        = (1 <<  13),
    COLLECT_ARCHIVER           = (1 <<  14),
    COLLECT_BGWRITER           = (1 <<  15),
    COLLECT_SLRU               = (1 <<  16),
    COLLECT_IO                 = (1 <<  17),
    COLLECT_CHECKPOINTER       = (1 <<  18),
} pg_flag_t;

typedef struct pg_stat_filter {
    char *arg1;
    char *arg2;
    char *arg3;
    struct pg_stat_filter *next;
} pg_stat_filter_t;

typedef struct {
    char *instance;

    PGconn *conn;
    c_complain_t conn_complaint;
    int proto_version;
    int server_version;

    char *host;
    char *port;
    char *database;
    char *user;
    char *password;

    char *sslmode;
    char *krbsrvname;
    char *service;

    char *metric_prefix;
    label_set_t labels;

    uint64_t flags;

    pg_stat_filter_t *pg_stat_database;
    pg_stat_filter_t *pg_database_size;
    pg_stat_filter_t *pg_database_locks;
    pg_stat_filter_t *pg_stat_database_conflicts;
    pg_stat_filter_t *pg_replication_slots;
    pg_stat_filter_t *pg_stat_activity;
    pg_stat_filter_t *pg_stat_table;
    pg_stat_filter_t *pg_stat_table_io;
    pg_stat_filter_t *pg_table_size;
    pg_stat_filter_t *pg_stat_indexes;
    pg_stat_filter_t *pg_stat_indexes_io;
    pg_stat_filter_t *pg_stat_sequence_io;
    pg_stat_filter_t *pg_stat_function;

    plugin_filter_t *filter;

    db_query_preparation_area_t **q_prep_areas;
    db_query_t **queries;
    size_t queries_num;

    metric_family_t fams[FAM_PG_MAX];
} psql_database_t;

static db_query_t **queries;
static size_t queries_num;

typedef struct {
    char *option;
    uint64_t flag;
} pg_flags_t;

static pg_flags_t pg_flags[] = {
    { "database",           COLLECT_DATABASE           },
    { "database_size",      COLLECT_DATABASE_SIZE      },
    { "database_locks",     COLLECT_DATABASE_LOCKS     },
    { "database_conflicts", COLLECT_DATABASE_CONFLICTS },
    { "table",              COLLECT_TABLE              },
    { "table_io",           COLLECT_TABLE_IO           },
    { "table_size",         COLLECT_TABLE_SIZE         },
    { "indexes",            COLLECT_INDEXES            },
    { "indexes_io",         COLLECT_INDEXES_IO         },
    { "sequences_io",       COLLECT_SEQUENCES_IO       },
    { "functions",          COLLECT_FUNCTIONS          },
    { "activity",           COLLECT_ACTIVITY           },
    { "replication_slots",  COLLECT_REPLICATION_SLOTS  },
    { "replication",        COLLECT_REPLICATION        },
    { "archiver",           COLLECT_ARCHIVER           },
    { "bgwriter",           COLLECT_BGWRITER           },
    { "slru",               COLLECT_SLRU               },
    { "io",                 COLLECT_SLRU               },
    { "checkpointer",       COLLECT_CHECKPOINTER       },
};
static size_t pg_flags_size = STATIC_ARRAY_SIZE(pg_flags);

static void pg_stat_filter_free(pg_stat_filter_t *filter)
{
    if (filter == NULL)
        return;

    while (filter != NULL) {
        free(filter->arg1);
        free(filter->arg2);
        free(filter->arg3);

        pg_stat_filter_t *next = filter->next;
        free(filter);
        filter = next;
    }
}

static void psql_database_delete(void *data)
{
    psql_database_t *db = data;
    if (data == NULL)
        return;

    PQfinish(db->conn);
    db->conn = NULL;

    free(db->instance);

    free(db->database);
    free(db->host);
    free(db->port);
    free(db->user);
    free(db->password);

    free(db->sslmode);

    free(db->krbsrvname);

    free(db->service);

    free(db->metric_prefix);
    label_set_reset(&db->labels);
    plugin_filter_free(db->filter);

    pg_stat_filter_free(db->pg_stat_database);
    pg_stat_filter_free(db->pg_database_size);
    pg_stat_filter_free(db->pg_database_locks);
    pg_stat_filter_free(db->pg_stat_database_conflicts);
    pg_stat_filter_free(db->pg_replication_slots);
    pg_stat_filter_free(db->pg_stat_activity);
    pg_stat_filter_free(db->pg_stat_table);
    pg_stat_filter_free(db->pg_stat_table_io);
    pg_stat_filter_free(db->pg_table_size);
    pg_stat_filter_free(db->pg_stat_indexes);
    pg_stat_filter_free(db->pg_stat_indexes_io);
    pg_stat_filter_free(db->pg_stat_sequence_io);
    pg_stat_filter_free(db->pg_stat_function);

    free(db);
}

static int psql_connect(psql_database_t *db)
{
    if ((!db) || (!db->database))
        return -1;

    char conninfo[4096];
    strbuf_t buf = STRBUF_CREATE_STATIC(conninfo);
    struct {
        char *param;
        char *value;
    } psql_params[] = {
        { "dbname",           db->database           },
        { "host",             db->host               },
        { "port",             db->port               },
        { "user",             db->user               },
        { "password",         db->password           },
        { "sslmode",          db->sslmode            },
        { "krbsrvname",       db->krbsrvname         },
        { "service",          db->service            },
        { "application_name", "ncollectd_postgresql" }
    };
    size_t psql_params_size = STATIC_ARRAY_SIZE(psql_params);

    for (size_t i = 0; i  < psql_params_size; i++) {
        if ((psql_params[i].value != NULL) && (psql_params[i].value[0] != '\0')) {
            strbuf_putstr(&buf, psql_params[i].param);
            strbuf_putstr(&buf, " = '");
            strbuf_putstr(&buf, psql_params[i].value);
            strbuf_putstr(&buf, "' ");
        }
    }

    db->conn = PQconnectdb(conninfo);
    db->proto_version = PQprotocolVersion(db->conn);
    return 0;
}

static int psql_check_connection(psql_database_t *db)
{
    bool init = false;

    if (!db->conn) {
        init = true;

        /* trigger c_release() */
        if (db->conn_complaint.interval == 0)
            db->conn_complaint.interval = 1;

        psql_connect(db);
    }

    if (PQstatus(db->conn) != CONNECTION_OK) {
        PQreset(db->conn);

        /* trigger c_release() */
        if (db->conn_complaint.interval == 0)
            db->conn_complaint.interval = 1;

        if (PQstatus(db->conn) != CONNECTION_OK) {
            c_complain(LOG_ERR, &db->conn_complaint,
                                "Failed to connect to database %s: %s", db->database,
                                PQerrorMessage(db->conn));
            return -1;
        }

        db->proto_version = PQprotocolVersion(db->conn);
    }

    db->server_version = PQserverVersion(db->conn);

    if (c_would_release(&db->conn_complaint)) {
        char *server_host;
        int server_version;

        server_host = PQhost(db->conn);
        server_version = PQserverVersion(db->conn);

        c_do_release(LOG_INFO, &db->conn_complaint,
                                 "Successfully %sconnected to database %s (user %s) "
                                 "at server %s%s%s (server version: %d.%d.%d, "
                                 "protocol version: %d, pid: %d)",
                                 init ? "" : "re", PQdb(db->conn), PQuser(db->conn),
                                 C_PSQL_SOCKET3(server_host, PQport(db->conn)),
                                 C_PSQL_SERVER_VERSION3(server_version), db->proto_version,
                                 PQbackendPID(db->conn));

        if (db->proto_version < 3)
            PLUGIN_WARNING("Protocol version %d does not support parameters.", db->proto_version);
    }
    return 0;
}

static int psql_exec_query(psql_database_t *db, db_query_t *q, db_query_preparation_area_t *prep_area)
{
    char **column_names = NULL;
    char **column_values = NULL;

    PGresult *res = NULL;

    res = PQexec(db->conn, db_query_get_statement(q));
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("Failed to execute SQL query: %s", PQerrorMessage(db->conn));
        PLUGIN_INFO("SQL query was: %s", db_query_get_statement(q));
        PQclear(res);
        return -1;
    }

    int status = -1;

    int rows_num = PQntuples(res);
    if (rows_num < 1)
        goto error;

    int column_num = PQnfields(res);
    column_names = calloc(column_num, sizeof(*column_names));
    if (column_names == NULL) {
        PLUGIN_ERROR("calloc failed.");
        goto error;
    }

    column_values = calloc(column_num, sizeof(*column_values));
    if (column_values == NULL) {
        PLUGIN_ERROR("calloc failed.");
        goto error;
    }

    for (int col = 0; col < column_num; ++col) {
        /* Pointers returned by PQfname are freed by PQclear */
        column_names[col] = PQfname(res, col);
        if (column_names[col] == NULL) {
            PLUGIN_ERROR("Failed to resolve name of column %i.", col);
            goto error;
        }
    }

    status = db_query_prepare_result(q, prep_area, db->metric_prefix, &db->labels,
                                        db->database, column_names, (size_t)column_num);

    if (status != 0) {
        PLUGIN_ERROR("db_query_prepare_result failed with status %i.", status);
        goto error;
    }

    for (int row = 0; row < rows_num; ++row) {
        int col;
        for (col = 0; col < column_num; ++col) {
            /* Pointers returned by `PQgetvalue' are freed by `PQclear' via
             * `BAIL_OUT'. */
            column_values[col] = PQgetvalue(res, row, col);
            if (column_values[col] == NULL) {
                PLUGIN_ERROR("Failed to get value at (row = %i, col = %i).", row, col);
                break;
            }
        }

        /* check for an error */
        if (col < column_num)
            continue;

        status = db_query_handle_result(q, prep_area, column_values, db->filter);
        if (status != 0) {
            PLUGIN_ERROR("db_query_handle_result failed with status %i.", status);
            goto error;
        }
    }

    db_query_finish_result(q, prep_area);

    status = 0;

error:
    free(column_names);
    free(column_values);
    PQclear(res);

    return status;
}

static int psql_read(user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    psql_database_t *db = ud->data;

    assert(NULL != db->database);

    if (psql_check_connection(db) != 0) {
        metric_family_append(&db->fams[FAM_PG_UP], VALUE_GAUGE(0), &db->labels, NULL);
        plugin_dispatch_metric_family(&db->fams[FAM_PG_UP], 0);
        return 0;
    }

    metric_family_append(&db->fams[FAM_PG_UP], VALUE_GAUGE(1), &db->labels, NULL);

    cdtime_t submit = cdtime();

    if (db->flags & COLLECT_DATABASE) {
        if (db->pg_stat_database == NULL) {
            pg_stat_database(db->conn, db->server_version, db->fams, &db->labels, NULL);
        } else {
            pg_stat_filter_t *filter = db->pg_stat_database;
            while (filter != NULL) {
                pg_stat_database(db->conn, db->server_version, db->fams, &db->labels, filter->arg1);
                filter = filter->next;
            }
        }
    }

    if (db->flags & COLLECT_DATABASE_SIZE) {
        if (db->pg_database_size == NULL) {
            pg_database_size(db->conn, db->server_version, db->fams, &db->labels, NULL);
        } else {
            pg_stat_filter_t *filter = db->pg_database_size;
            while (filter != NULL) {
                pg_database_size(db->conn, db->server_version, db->fams, &db->labels, filter->arg1);
                filter = filter->next;
            }
        }
    }

    if (db->flags & COLLECT_DATABASE_LOCKS) {
        if (db->pg_database_locks == NULL) {
            pg_database_locks(db->conn, db->server_version, db->fams, &db->labels, NULL);
        } else {
            pg_stat_filter_t *filter = db->pg_database_locks;
            while (filter != NULL) {
                pg_database_locks(db->conn, db->server_version, db->fams, &db->labels, filter->arg1);
                filter = filter->next;
            }
        }
    }

    if (db->flags & COLLECT_DATABASE_CONFLICTS) {
        if (db->pg_stat_database_conflicts == NULL) {
            pg_stat_database_conflicts(db->conn, db->server_version, db->fams, &db->labels, NULL);
        } else {
            pg_stat_filter_t *filter = db->pg_stat_database_conflicts;
            while (filter != NULL) {
                pg_stat_database_conflicts(db->conn, db->server_version, db->fams, &db->labels,
                                                     filter->arg1);
                filter = filter->next;
            }
        }
    }

    if (db->flags & COLLECT_TABLE) {
        if (db->pg_stat_table == NULL) {
            pg_stat_user_table(db->conn, db->server_version, db->fams, &db->labels,
                                         NULL, NULL);
        } else {
            pg_stat_filter_t *filter = db->pg_stat_table;
            while (filter != NULL) {
                pg_stat_user_table(db->conn, db->server_version, db->fams, &db->labels,
                                             filter->arg1, filter->arg2);
                filter = filter->next;
            }
        }
    }

    if (db->flags & COLLECT_TABLE_IO) {
        if (db->pg_stat_table_io == NULL) {
            pg_statio_user_tables(db->conn, db->server_version, db->fams, &db->labels,
                                            NULL, NULL);
        } else {
            pg_stat_filter_t *filter = db->pg_stat_table_io;
            while (filter != NULL) {
                pg_statio_user_tables(db->conn, db->server_version, db->fams, &db->labels,
                                                filter->arg1, filter->arg2);
                filter = filter->next;
            }
        }
    }

    if (db->flags & COLLECT_TABLE_SIZE) {
        if (db->pg_table_size == NULL) {
            pg_table_size(db->conn, db->server_version, db->fams, &db->labels, NULL, NULL);
        } else {
            pg_stat_filter_t *filter = db->pg_table_size;
            while (filter != NULL) {
                pg_table_size(db->conn, db->server_version, db->fams, &db->labels,
                                        filter->arg1, filter->arg2);
                filter = filter->next;
            }
        }
    }

    if (db->flags & COLLECT_INDEXES) {
        if (db->pg_stat_indexes == NULL) {
            pg_stat_user_indexes(db->conn, db->server_version, db->fams, &db->labels,
                                           NULL, NULL, NULL);
        } else {
            pg_stat_filter_t *filter = db->pg_stat_indexes;
            while (filter != NULL) {
                pg_stat_user_indexes(db->conn, db->server_version, db->fams, &db->labels,
                                               filter->arg1, filter->arg2, filter->arg3);
                filter = filter->next;
            }
        }
    }

    if (db->flags & COLLECT_INDEXES_IO) {
        if (db->pg_stat_indexes_io == NULL) {
            pg_statio_user_indexes(db->conn, db->server_version, db->fams, &db->labels,
                                             NULL, NULL, NULL);
        } else {
            pg_stat_filter_t *filter = db->pg_stat_indexes_io;
            while (filter != NULL) {
                pg_statio_user_indexes(db->conn, db->server_version, db->fams, &db->labels,
                                                 filter->arg1, filter->arg2, filter->arg3);
                filter = filter->next;
            }
        }
    }

    if (db->flags & COLLECT_SEQUENCES_IO) {
        if (db->pg_stat_sequence_io == NULL) {
            pg_statio_user_sequences(db->conn, db->server_version, db->fams, &db->labels,
                                               NULL, NULL);
        } else {
            pg_stat_filter_t *filter = db->pg_stat_sequence_io;
            while (filter != NULL) {
                pg_statio_user_sequences(db->conn, db->server_version, db->fams, &db->labels,
                                                   filter->arg1, filter->arg2);
                filter = filter->next;
            }
        }
    }

    if (db->flags & COLLECT_FUNCTIONS) {
        if (db->pg_stat_function == NULL) {
            pg_stat_user_functions(db->conn, db->server_version, db->fams, &db->labels,
                                             NULL, NULL);
        } else {
            pg_stat_filter_t *filter = db->pg_stat_function;
            while (filter != NULL) {
                pg_stat_user_functions(db->conn, db->server_version, db->fams, &db->labels,
                                                 filter->arg1, filter->arg2);
                filter = filter->next;
            }
        }
    }

    if (db->flags & COLLECT_ACTIVITY) {
        if (db->pg_stat_activity == NULL) {
            pg_stat_activity (db->conn, db->server_version, db->fams, &db->labels, NULL);
        } else {
            pg_stat_filter_t *filter = db->pg_stat_activity;
            while (filter != NULL) {
                pg_stat_activity (db->conn, db->server_version, db->fams, &db->labels,
                                            filter->arg1);
                filter = filter->next;
            }
        }
    }

    if (db->flags & COLLECT_REPLICATION_SLOTS) {
        if (db->pg_replication_slots == NULL) {
            pg_replication_slots(db->conn, db->server_version, db->fams, &db->labels, NULL);
        } else {
            pg_stat_filter_t *filter = db->pg_replication_slots;
            while (filter != NULL) {
                pg_replication_slots(db->conn, db->server_version, db->fams, &db->labels,
                                               filter->arg1);
                filter = filter->next;
            }
        }
    }

    if (db->flags & COLLECT_REPLICATION)
        pg_stat_replication(db->conn, db->server_version, db->fams, &db->labels);
    if (db->flags & COLLECT_ARCHIVER)
        pg_stat_archiver(db->conn, db->server_version, db->fams, &db->labels);
    if (db->flags & COLLECT_BGWRITER)
        pg_stat_bgwriter(db->conn, db->server_version, db->fams, &db->labels);
    if (db->flags & COLLECT_SLRU)
        pg_stat_slru(db->conn, db->server_version, db->fams, &db->labels);
    if (db->flags & COLLECT_IO)
        pg_stat_io(db->conn, db->server_version, db->fams, &db->labels);
    if (db->flags & COLLECT_CHECKPOINTER)
        pg_stat_checkpointer(db->conn, db->server_version, db->fams, &db->labels);

    plugin_dispatch_metric_family_array_filtered(db->fams, FAM_PG_MAX, db->filter, submit);

    for (size_t i = 0; i < db->queries_num; i++) {
        db_query_preparation_area_t *prep_area = db->q_prep_areas[i];
        db_query_t *q = db->queries[i];

        if ((db->server_version != 0) && (db_query_check_version(q, db->server_version) <= 0))
            continue;

        psql_exec_query(db, q, prep_area);
    }

    return 0;
}

static int psql_config_add_filter(pg_stat_filter_t **stat_filter, char *arg1, size_t arg1_len,
                                                                  char *arg2, size_t arg2_len,
                                                                  char *arg3, size_t arg3_len)
{
    pg_stat_filter_t *filter = calloc(1, sizeof(*filter));
    if (filter == NULL) {
        PLUGIN_ERROR("calloc failed");
        return -1;
    }

    if ((arg1 != NULL) && (arg1_len > 0)) {
        filter->arg1 = strndup(arg1, arg1_len);
        if (filter->arg1 == NULL) {
            PLUGIN_ERROR("strdup failed.");
            pg_stat_filter_free(filter);
            return -1;
        }
    }

    if ((arg2 != NULL) && (arg2_len > 0)) {
        filter->arg2 = strndup(arg2, arg2_len);
        if (filter->arg2 == NULL) {
            PLUGIN_ERROR("strdup failed.");
            pg_stat_filter_free(filter);
            return -1;
        }
    }

    if ((arg3 != NULL) && (arg3_len > 0)) {
        filter->arg3 = strndup(arg3, arg3_len);
        if (filter->arg3 == NULL) {
            PLUGIN_ERROR("strdup failed.");
            pg_stat_filter_free(filter);
            return -1;
        }
    }

    pg_stat_filter_t *next = *stat_filter;
    *stat_filter = filter;
    filter->next = next;

    return 0;
}

static int psql_config_collect(const config_item_t *ci, psql_database_t *db)
{
    if (ci->values_num == 0) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires one or more arguments.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    for (int i = 0; i < ci->values_num; i++) {
        if (ci->values[i].type != CONFIG_TYPE_STRING) {
            PLUGIN_ERROR("The %d argument of '%s' option in %s:%d must be a string.",
                         i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }

        char *option = ci->values[i].value.string;
        bool negate = false;
        if (option[0] == '!') {
            negate = true;
            option++;
        }

        if (!strcasecmp("ALL", option)) {
            if (negate) {
                db->flags = 0ULL;
            } else {
                db->flags = ~0ULL;
            }
            continue;
        }

        size_t option_len = 0;
        char *arg1 = strchr(option, '(');
        if (arg1 != NULL) {
            if (negate) {
                PLUGIN_ERROR("The %d argument of '%s' option in %s:%d cannot be negated.",
                             i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
                return -1;
            }
            option_len = arg1 - option;
            arg1++;

            size_t arg1_len = 0;
            size_t arg2_len = 0;
            size_t arg3_len = 0;
            char *arg3 = NULL;

            char *arg2 = strchr(arg1, ',');
            if (arg2 != NULL) {
                arg1_len = arg2 - arg1;
                arg2++;
                arg3 = strchr(arg2, ',');
                if (arg3 != NULL) {
                    arg2_len = arg3 - arg2;
                    arg3++;
                    char *end = strchr(arg3, ')');
                    if (end != NULL) {
                        PLUGIN_ERROR("Missing end ')' in the %d argument of '%s' option in %s:%d.",
                                      i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
                        return -1;
                    }
                    arg3_len = end - arg3;
                } else {
                    char *end = strchr(arg2, ')');
                    if (end != NULL) {
                        PLUGIN_ERROR("Missing end ')' in the %d argument of '%s' option in %s:%d.",
                                      i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
                        return -1;
                    }
                    arg2_len = end - arg2;
                }

            } else {
                char *end = strchr(arg1, ')');
                if (end == NULL) {
                    PLUGIN_ERROR("Missing end ')' in the %d argument of '%s' option in %s:%d.",
                                  i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
                    return -1;
                }
                arg1_len = end - arg1;
            }

            if (arg1_len == 0) {
                PLUGIN_ERROR("Missing argument the %d option of '%s' key in %s:%d.",
                             i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
                return -1;
            }

            int status = 0;
            if (!strncasecmp("table", option, option_len)) {
                db->flags |= COLLECT_TABLE;
                status = psql_config_add_filter(&db->pg_stat_table, arg1, arg1_len,
                                                                    arg2, arg2_len,
                                                                    arg3, arg3_len);
            } else if (!strncasecmp("table_io", option, option_len)) {
                db->flags |= COLLECT_TABLE_IO;
                status = psql_config_add_filter(&db->pg_stat_table_io, arg1, arg1_len,
                                                                       arg2, arg2_len,
                                                                       arg3, arg3_len);
            } else if (!strncasecmp("table_size", option, option_len)) {
                db->flags |= COLLECT_TABLE_SIZE;
                status = psql_config_add_filter(&db->pg_table_size, arg1, arg1_len,
                                                                    arg2, arg2_len,
                                                                    arg3, arg3_len);
            } else if (!strncasecmp("indexes", option, option_len)) {
                db->flags |= COLLECT_INDEXES;
                status = psql_config_add_filter(&db->pg_stat_indexes, arg1, arg1_len,
                                                                      arg2, arg2_len,
                                                                      arg3, arg3_len);
            } else if (!strncasecmp("indexes_io", option, option_len)) {
                db->flags |= COLLECT_INDEXES_IO;
                status = psql_config_add_filter(&db->pg_stat_indexes_io, arg1, arg1_len,
                                                                         arg2, arg2_len,
                                                                         arg3, arg3_len);
            } else if (!strncasecmp("sequences_io", option, option_len)) {
                db->flags |= COLLECT_SEQUENCES_IO;
                status = psql_config_add_filter(&db->pg_stat_sequence_io, arg1, arg1_len,
                                                                          arg2, arg2_len,
                                                                          arg3, arg3_len);
            } else if (!strncasecmp("functions", option, option_len)) {
                db->flags |= COLLECT_FUNCTIONS;
                status = psql_config_add_filter(&db->pg_stat_function, arg1, arg1_len,
                                                                       arg2, arg2_len,
                                                                       arg3, arg3_len);
            } else if (!strncasecmp("database", option, option_len)) {
                db->flags |= COLLECT_DATABASE;
                status = psql_config_add_filter(&db->pg_stat_database, arg1, arg1_len,
                                                                       arg2, arg2_len,
                                                                       arg3, arg3_len);
            } else if (!strncasecmp("database_size", option, option_len)) {
                db->flags |= COLLECT_DATABASE_SIZE;
                status = psql_config_add_filter(&db->pg_database_size, arg1, arg1_len,
                                                                       arg2, arg2_len,
                                                                       arg3, arg3_len);
            } else if (!strncasecmp("database_locks",  option, option_len)) {
                db->flags |= COLLECT_DATABASE_LOCKS;
                status = psql_config_add_filter(&db->pg_database_locks, arg1, arg1_len,
                                                                        arg2, arg2_len,
                                                                        arg3, arg3_len);
            } else if (!strncasecmp("database_conflicts", option, option_len)) {
                db->flags |= COLLECT_DATABASE_CONFLICTS;
                status = psql_config_add_filter(&db->pg_stat_database_conflicts, arg1, arg1_len,
                                                                                 arg2, arg2_len,
                                                                                 arg3, arg3_len);
            } else if (!strncasecmp("activity", option, option_len)) {
                db->flags |= COLLECT_ACTIVITY;
                status = psql_config_add_filter(&db->pg_stat_activity, arg1, arg1_len,
                                                                       arg2, arg2_len,
                                                                       arg3, arg3_len);
            } else if (!strncasecmp("replication_slots", option, option_len)) {
                db->flags |= COLLECT_REPLICATION_SLOTS;
                status = psql_config_add_filter(&db->pg_replication_slots, arg1, arg1_len,
                                                                           arg2, arg2_len,
                                                                           arg3, arg3_len);
            } else {
                PLUGIN_ERROR("The %d argument of '%s' option in %s:%d doesn't have arguments.",
                             i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
                return -1;
            }

            if (status != 0) {
                PLUGIN_ERROR("Failed to process the %d argument of '%s' option in %s:%d.",
                             i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
                return -1;
            }

            continue;
        }

        for (size_t j = 0; j < pg_flags_size; j++)  {
            if (!strcasecmp(pg_flags[j].option, option)) {
                if (negate) {
                    db->flags &= ~pg_flags[j].flag;
                } else {
                    db->flags |= pg_flags[j].flag;
                }
            }
        }
    }

    return 0;
}

static int psql_config_database(config_item_t *ci)
{
    psql_database_t *db = calloc(1, sizeof(*db));
    if (db == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &db->instance);
    if (status != 0) {
        PLUGIN_ERROR("'instance' expects a single string argument.");
        free(db);
        return -1;
    }

    db->flags = COLLECT_DATABASE | COLLECT_DATABASE_SIZE | COLLECT_DATABASE_LOCKS |
                COLLECT_DATABASE_CONFLICTS | COLLECT_ACTIVITY | COLLECT_REPLICATION_SLOTS |
                COLLECT_REPLICATION | COLLECT_ARCHIVER | COLLECT_BGWRITER | COLLECT_SLRU |
                COLLECT_IO | COLLECT_CHECKPOINTER;

    memcpy(db->fams, pg_fams, sizeof(db->fams[0])*FAM_PG_MAX);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "database") == 0) {
            status = cf_util_get_string(child, &db->database);
        } else if (strcasecmp(child->key, "host") == 0) {
            status = cf_util_get_string(child, &db->host);
        } else if (strcasecmp(child->key, "port") == 0) {
            status = cf_util_get_service(child, &db->port);
        } else if (strcasecmp(child->key, "user") == 0) {
            status = cf_util_get_string(child, &db->user);
        } else if (strcasecmp(child->key, "user-env") == 0) {
            status = cf_util_get_string_env(child, &db->user);
        } else if (strcasecmp(child->key, "password") == 0) {
            status = cf_util_get_string(child, &db->password);
        } else if (strcasecmp(child->key, "password-env") == 0) {
            status = cf_util_get_string_env(child, &db->password);
        } else if (strcasecmp(child->key, "metric-prefix") == 0) {
            status = cf_util_get_string(child, &db->metric_prefix);
        } else if (strcasecmp(child->key, "label") == 0) {
            status = cf_util_get_label(child, &db->labels);
        } else if (strcasecmp(child->key, "ssl-mode") == 0) {
            status = cf_util_get_string(child, &db->sslmode);
        } else if (strcasecmp(child->key, "krb-srv-name") == 0) {
            status = cf_util_get_string(child, &db->krbsrvname);
        } else if (strcasecmp(child->key, "service") == 0) {
            status = cf_util_get_string(child, &db->service);
        } else if (strcasecmp(child->key, "query") == 0) {
            status = db_query_pick_from_list(child, queries, queries_num,
                                                    &db->queries, &db->queries_num);
        } else if (strcasecmp(child->key, "collect") == 0) {
            status = psql_config_collect(child, db);
        } else if (strcasecmp(child->key, "interval") == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = plugin_filter_configure(child, &db->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        psql_database_delete(db);
        return -1;
    }

    if (db->database == NULL) {
        PLUGIN_ERROR("Instance '%s': No 'database' has been configured.", db->instance);
        psql_database_delete(db);
        return -1;
    }

    label_set_add(&db->labels, true, "instance", db->instance);

    return plugin_register_complex_read("postgresql", db->instance, psql_read, interval,
                                        &(user_data_t){.data=db, .free_func=psql_database_delete});
}

static int psql_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "instance") == 0) {
            status = psql_config_database(child);
        } else if (strcasecmp("query", child->key) == 0) {
            status = db_query_create(&queries, &queries_num, child, NULL);
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

static int psql_shutdown(void)
{
    db_query_free(queries, queries_num);
    queries = NULL;
    queries_num = 0;
    return 0;
}

void module_register(void)
{
    plugin_register_config("postgresql", psql_config);
    plugin_register_shutdown("postgresql", psql_shutdown);
}
