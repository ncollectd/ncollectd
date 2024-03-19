// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libmetric/metric.h"

#include "mongodb_fam.h"

metric_family_t fams_mongodb[FAM_MONGODB_MAX] = {
    [FAM_MONGODB_UP] = {
        .name = "mongodb_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the mongodb server be reached.",
    },
    [FAM_MONGODB_UPTIME_SECONDS] = {
        .name = "mongodb_uptime_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of seconds that the current MongoDB process has been active."
    },
    [FAM_MONGODB_ASSERTS_REGULAR] = {
        .name = "mongodb_asserts_regular",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of regular assertions raised.",
    },
    [FAM_MONGODB_ASSERTS_MSG] = {
        .name = "mongodb_asserts_msg",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of message assertions raised."
    },
    [FAM_MONGODB_ASSERTS_USER] = {
        .name = "mongodb_asserts_user",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of 'user asserts' that have occurred."
    },
    [FAM_MONGODB_ASSERTS_ROLLOVERS] = {
        .name = "mongodb_asserts_rollovers",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times that the rollover counters have rolled over "
    },
    [FAM_MONGODB_BUCKET_CATALOG_BUCKETS] = {
        .name = "mongodb_bucket_catalog_buckets",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of buckets that store time series data internally.",
    },
    [FAM_MONGODB_BUCKET_CATALOG_OPEN_BUCKETS] = {
        .name = "mongodb_bucket_catalog_open_buckets",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of active, uncommitted writes to buckets.",
    },
    [FAM_MONGODB_BUCKET_CATALOG_IDLE_BUCKETS] = {
        .name = "mongodb_bucket_catalog_idle_buckets",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of buckets that are not full and can store incoming time series data.",
    },
    [FAM_MONGODB_BUCKET_CATALOG_MEMORY_USAGE_BYTES] = {
        .name = "mongodb_bucket_catalog_memory_usage_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of bytes used by internal bucketing data structures.",
    },
    [FAM_MONGODB_CONNECTIONS] = {
        .name = "mongodb_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of incoming connections from clients to the database server."
    },
    [FAM_MONGODB_CONNECTIONS_AVAILABLE] = {
        .name = "mongodb_connections_available",
        .type = METRIC_TYPE_GAUGE,
        .help ="The number of unused incoming connections available.",
    },
    [FAM_MONGODB_CONNECTIONS_CREATED] = {
        .name = "mongodb_connections_created",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of all incoming connections created to the server.",
    },
    [FAM_MONGODB_CONNECTIONS_ACTIVE] = {
        .name = "mongodb_connections_active",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of active client connections to the server.",
    },
    [FAM_MONGODB_CONNECTIONS_THREADED] = {
        .name = "mongodb_connections_threaded",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of incoming connections from clients that are assigned "
                "to threads that service client requests.",
    },
    [FAM_MONGODB_CONNECTIONS_EXHAUST_IS_MASTER] = {
        .name = "mongodb_connections_exhaust_is_master",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of connections whose last request was "
                "an isMaster request with exhaustAllowed.",
    },
    [FAM_MONGODB_CONNECTIONS_EXHAUST_HELLO] = {
        .name = "mongodb_connections_exhaust_hello",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of connections whose last request was "
                "a hello request with exhaustAllowed.",
    },
    [FAM_MONGODB_CONNECTIONS_AWAITING_TOPOLOGY_CHANGES] = {
        .name = "mongodb_connections_awaiting_topology_changes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of clients currently waiting in a hello or isMaster request "
                "for a topology change."
    },
    [FAM_MONGODB_HEAP_USAGE_BYTES] = {
        .name = "mongodb_heap_usage_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total size in bytes of heap space used by the database process.",
    },
    [FAM_MONGODB_PAGE_FAULTS] = {
        .name = "mongodb_page_faults",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total number of page faults.",
    },
    [FAM_MONGODB_LOCK_ACQUIRE] = {
        .name = "mongodb_lock_acquire",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the lock was acquired in the specified mode.",
    },
    [FAM_MONGODB_LOCK_ACQUIRE_WAIT] = {
        .name = "mongodb_lock_acquire_wait",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the lock acquisitions encountered waits because "
                "the locks were held in a conflicting mode.",
    },
    [FAM_MONGODB_LOCK_ACQUIRE_SECONDS] = {
        .name = "mongodb_lock_acquire_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cumulative wait time in seconds for the lock acquisitions.",
    },
    [FAM_MONGODB_LOCK_DEAD_LOCK] = {
        .name = "mongodb_lock_dead_lock",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the lock acquisitions encountered deadlocks.",
    },
    [FAM_MONGODB_MIRRORED_READS_SEEN] = {
        .name = "mongodb_mirrored_reads_seen",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of operations that support mirroring received by this member.",
    },
    [FAM_MONGODB_MIRRORED_READS_SENT] = {
        .name = "mongodb_mirrored_reads_sent",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of mirrored reads sent by this member when primary.",
    },
    [FAM_MONGODB_NETWORK_IN_BYTES] = {
        .name = "mongodb_network_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes that the server has received over network connections "
                "initiated by clients or other mongod or mongos instances.",
    },
    [FAM_MONGODB_NETWORK_OUT_BYTES] = {
        .name = "mongodb_network_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes that the server has sent over network connections "
                "initiated by clients or other mongod or mongos instances.",
    },
    [FAM_MONGODB_NETWORK_SLOW_DNS_OPERATIONS] = {
        .name = "mongodb_network_slow_dns_operations",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of DNS resolution operations which took longer than 1 second.",
    },
    [FAM_MONGODB_NETWORK_SLOW_SSL_OPERATIONS] = {
        .name = "mongodb_network_slow_ssl_operations",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of SSL handshake operations which took longer than 1 second.",
    },
    [FAM_MONGODB_NETWORK_REQUESTS] = {
        .name = "mongodb_network_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of distinct requests that the server has received.",
    },
    [FAM_MONGODB_NETWORK_TCP_FAST_OPEN_ACCEPTED] = {
        .name = "mongodb_network_tcp_fast_open_accepted",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of accepted incoming TCP Fast Open (TFO) connections.",
    },
    [FAM_MONGODB_NETWORK_COMPRESSOR_IN_BYTES] = {
        .name = "mongodb_network_compressor_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MONGODB_NETWORK_COMPRESSOR_OUT_BYTES] = {
        .name = "mongodb_network_compressor_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MONGODB_NETWORK_DECOMPRESSOR_IN_BYTES] = {
        .name = "mongodb_network_decompressor_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MONGODB_NETWORK_DECOMPRESSOR_OUT_BYTES] = {
        .name = "mongodb_network_decompressor_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_MONGODB_NETWORK_SERVICE_EXECUTORS_THREADS_RUNNING] = {
        .name = "mongodb_network_service_executors_threads_running",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of threads running in this service executor.",
    },
    [FAM_MONGODB_NETWORK_SERVICE_EXECUTORS_CLIENTS] = {
        .name = "mongodb_network_service_executors_clients",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of clients allocated to this service executor.",
    },
    [FAM_MONGODB_NETWORK_SERVICE_EXECUTORS_CLIENTS_RUNNING] = {
        .name = "mongodb_network_service_executors_clients_running",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of clients currently using this service executor to run requests.",
    },
    [FAM_MONGODB_NETWORK_SERVICE_EXECUTORS_CLIENTS_WAITING_FOR_DATA] = {
        .name = "mongodb_network_service_executors_clients_waiting_for_data",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of clients using this service executor that are waiting "
                "for incoming data from the network.",
    },
    [FAM_MONGODB_OPERATION_INSERT] = {
        .name = "mongodb_operation_insert",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of insert operations received.",
    },
    [FAM_MONGODB_OPERATION_QUERY] = {
        .name = "mongodb_operation_query",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of queries received.",
    },
    [FAM_MONGODB_OPERATION_UPDATE] = {
        .name = "mongodb_operation_update",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of update operations received.",
    },
    [FAM_MONGODB_OPERATION_DELETE] = {
        .name = "mongodb_operation_delete",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of delete operations",
    },
    [FAM_MONGODB_OPERATION_GETMORE] = {
        .name = "mongodb_operation_getmore",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of getMore operations.",
    },
    [FAM_MONGODB_OPERATION_COMMAND] = {
        .name = "mongodb_operation_command",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of commands issued to the database.",
    },
    [FAM_MONGODB_REPLICATED_OPERATION_INSERT] = {
        .name = "mongodb_replicated_operation_insert",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of replicated insert operations.",
    },
    [FAM_MONGODB_REPLICATED_OPERATION_QUERY] = {
        .name = "mongodb_replicated_operation_query",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of replicated queries.",
    },
    [FAM_MONGODB_REPLICATED_OPERATION_UPDATE] = {
        .name = "mongodb_replicated_operation_update",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of replicated update operations.",
    },
    [FAM_MONGODB_REPLICATED_OPERATION_DELETE] = {
        .name = "mongodb_replicated_operation_delete",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of replicated delete operations.",
    },
    [FAM_MONGODB_REPLICATED_OPERATION_GETMORE] = {
        .name = "mongodb_replicated_operation_getmore",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of getMore operations.",
    },
    [FAM_MONGODB_REPLICATED_OPERATION_COMMAND] = {
        .name = "mongodb_replicated_operation_command",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of replicated commands issued to the database.",
    },
    [FAM_MONGODB_MEMORY_RESIDENT_BYTES] = {
        .name = "mongodb_memory_resident_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The the amount of RAM in bytes, currently used by the database process.",
    },
    [FAM_MONGODB_MEMORY_VIRTUAL_BYTES] = {
        .name = "mongodb_memory_virtual_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The quantity in bytes, of virtual memory used by the mongod process.",
    },
    [FAM_MONGODB_AGGREGATION_PIPELINE_STAGES] = {
        .name = "mongodb_aggregation_pipeline_stages",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of executions for each aggregation pipeline stage.",
    },
    [FAM_MONGODB_OPERATOR_EXPRESSIONS] = {
        .name = "mongodb_operator_expressions",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of Expression Operator executions.",
    },
    [FAM_MONGODB_OPERATOR_GROUP_ACCUMULATORS] = {
        .name = "mongodb_operator_group_accumulators",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of group accumulator executions.",
    },
    [FAM_MONGODB_OPERATOR_MATCH] = {
        .name = "mongodb_operator_match",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of match expression executions.",
    },
    [FAM_MONGODB_OPERATOR_WINDOW_ACCUMULATORS] = {
        .name = "mongodb_operator_window_accumulators",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of window accumulator executions.",
    },
    [FAM_MONGODB_COMMANDS_FAILED] = {
        .name = "mongodb_commands_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times this command failed on this mongod.",
    },
    [FAM_MONGODB_COMMANDS] = {
        .name = "mongodb_commands",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times this command executed on this mongod.",
    },
#if 0

metrics.commands.clusterUpdate.pipeline
metrics.commands.findAndModify.pipeline
metrics.commands.update.pipeline

metrics.commands.clusterUpdate.arrayFilters
metrics.commands.findAndModify.arrayFilters
metrics.commands.update.arrayFilters


metrics.commands.update.pipeline

    The number of times an aggregation pipeline was used to update documents on this mongod. Subtract this value from the total number of updates to get the number of updates made with document syntax.

    The pipeline counter is only available for update and findAndModify operations.

metrics.commands.findAndModify.pipeline

    The number of times findAndModify() was used in an aggregation pipeline to update documents on this mongod.

    The pipeline counter is only available for update and findAndModify operations.

metrics.commands.update.arrayFilters

    The number of times an arrayFilter was used to update documents on this mongod.

    The arrayFilters counter is only available for update and findAndModify operations.

metrics.commands.findAndModify.arrayFilters

    The number of times an arrayFilter was used with findAndModify() to update documents on this mongod.

    The arrayFilters counter is only available for update and findAndModify operations.

#endif

    [FAM_MONGODB_DOCUMENT_DELETED] = {
        .name = "mongodb_document_deleted",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of documents deleted.",
    },
    [FAM_MONGODB_DOCUMENT_INSERTED] = {
        .name = "mongodb_document_inserted",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of documents inserted.",
    },
    [FAM_MONGODB_DOCUMENT_RETURNED] = {
        .name = "mongodb_document_returned",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of documents returned by queries.",
    },
    [FAM_MONGODB_DOCUMENT_UPDATED] = {
        .name = "mongodb_document_updated",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of documents updated.",
    },
    [FAM_MONGODB_DOTS_AND_DOLLARS_FIELDS_INSERTS] = {
        .name = "mongodb_dots_and_dollars_fields_inserts",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of how often insert operations ran using a dollar ($) prefixed name.",
    },
    [FAM_MONGODB_DOTS_AND_DOLLARS_FIELDS_UPDATES] = {
        .name = "mongodb_dots_and_dollars_fields_updates",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of how often update operations ran using a dollar ($) prefixed name.",
    },
    [FAM_MONGODB_TTL_DELETED_DOCUMENTS] = {
        .name = "mongodb_ttl_deleted_documents",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of documents deleted from collections with a ttl index.",
    },
    [FAM_MONGODB_TTL_PASSES] = {
        .name = "mongodb_ttl_passes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times the background process removes documents ",
                "from collections with a ttl index.",
    },
    [FAM_MONGODB_CURSOR_MORE_THAN_ONE_BATCH] = {
        .name = "mongodb_cursor_more_than_one_batch",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of cursors that have returned more than one batch.",
    },
    [FAM_MONGODB_CURSOR_TIMEOUT] = {
        .name = "mongodb_cursor_timeout",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of cursors that have timed out.",
    },
    [FAM_MONGODB_CURSOR_OPEN] = {
        .name = "mongodb_cursor_open",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of cursors that have been opened.",
    },
    [FAM_MONGODB_CURSOR_OPEN_NO_TIMEOUT] = {
        .name = "mongodb_cursor_open_no_timeout",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of open cursors with the option DBQuery.Option.noTimeout set "
                "to prevent timeout after a period of inactivity.",
    },
    [FAM_MONGODB_CURSOR_OPEN_PINNED] = {
        .name = "mongodb_cursor_open_pinned",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of 'pinned' open cursors.",
    },

    [FAM_MONGODB_CURSOR_CURRENT_OPEN] = {
        .name = "mongodb_cursor_current_open",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of cursors that MongoDB is maintaining for clients.",
    },
    [FAM_MONGODB_CURSOR_OPEN_SINGLE_TARGET] = {
        .name = "mongodb_cursor_open_single_target",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of cursors that only target a single shard.",
    },
    [FAM_MONGODB_CURSOR_OPEN_MULTI_TARGER] = {
        .name = "mongodb_cursor_open_multi_targer",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of cursors that only target more than one shard.",
    },
    [FAM_MONGODB_DATABASE_COLLECTIONS] = {
        .name = "mongodb_database_collections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of collections in the database."
    },
    [FAM_MONGODB_DATABASE_VIEWS] = {
        .name = "mongodb_database_views",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of views in the database.",
    },
    [FAM_MONGODB_DATABASE_OBJECTS] = {
        .name = "mongodb_database_objects",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of objects (specifically, documents) in the database "
                "across all collections.",
    },
    [FAM_MONGODB_DATABASE_DATA_BYTES] = {
        .name = "mongodb_database_data_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total size of the uncompressed data held in the database.",
    },
    [FAM_MONGODB_DATABASE_STORAGE_BYTES] = {
        .name = "mongodb_database_storage_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Sum of the space allocated to all collections in the database "
                "for document storage, including free space.",
    },
    [FAM_MONGODB_DATABASE_STORAGE_FREE_BYTES] = {
        .name = "mongodb_database_storage_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Sum of the free space allocated to all collections in the database "
                "for document storage.",
    },
    [FAM_MONGODB_DATABASE_INDEXES] = {
        .name = "mongodb_database_indexes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of indexes across all collections in the database.",
    },
    [FAM_MONGODB_DATABASE_INDEX_STORAGE_BYTES] = {
        .name = "mongodb_database_index_storage_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Sum of the space allocated to all indexes in the database, "
                "including free index space.",
    },
    [FAM_MONGODB_DATABASE_INDEX_STORAGE_FREE_BYTES] = {
        .name = "mongodb_database_index_storage_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Sum of the free space allocated to all indexes in the database.",
    },
    [FAM_MONGODB_DATABASE_BYTES] = {
        .name = "mongodb_database_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Sum of the space allocated for both documents and indexes"
                "in all collections in the database.",
    },
    [FAM_MONGODB_DATABASE_FILESYSTEM_USED_BYTES] = {
        .name = "mongodb_database_filesystem_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total size of all disk space in use on the filesystem where MongoDB stores data.",
    },
    [FAM_MONGODB_DATABASE_FILESYSTEM_BYTES] = {
        .name = "mongodb_database_filesystem_bytes",
       .type = METRIC_TYPE_GAUGE,
       .help = "Total size of all disk capacity on the filesystem where MongoDB stores data.",
    },
};
