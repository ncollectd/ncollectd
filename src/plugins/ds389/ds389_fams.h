/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

enum {
    FAM_DS389_UP = 0,
    FAM_DS389_VERSION_INFO,
    FAM_DS389_START_TIME_SECONDS,
    FAM_DS389_THREADS,
    FAM_DS389_CURRENT_CONNECTIONS,
    FAM_DS389_CONNECTIONS,
    FAM_DS389_CURRENT_CONNECTIONS_MAXTHREADS,
    FAM_DS389_CONNECTIONS_MAXTHREADS,
    FAM_DS389_DTABLESIZE,
    FAM_DS389_READWAITERS,
    FAM_DS389_OPS_INITIATED,
    FAM_DS389_OPS_COMPLETED,
    FAM_DS389_SEND_ENTRIES,
    FAM_DS389_NBACKENDS,
    FAM_DS389_BINDS_ANONYMOUS,
    FAM_DS389_BINDS_UNAUTH,
    FAM_DS389_BINDS_SIMPLEAUTH,
    FAM_DS389_BINDS_STRONGAUTH,
    FAM_DS389_BINDS_SECURITYERRORS,
    FAM_DS389_OPS_IN,
    FAM_DS389_OPS_COMPARE,
    FAM_DS389_OPS_ADDENTRY,
    FAM_DS389_OPS_REMOVEENTRY,
    FAM_DS389_OPS_MODIFYENTRY,
    FAM_DS389_OPS_MODIFYRDN,
    FAM_DS389_OPS_SEARCH,
    FAM_DS389_OPS_ONELEVELSEARCH,
    FAM_DS389_OPS_WHOLESUBTREESEARCH,
    FAM_DS389_REFERRALS,
    FAM_DS389_SECURITYERRORS,
    FAM_DS389_ERRORS,
    FAM_DS389_RECV_BYTES,
    FAM_DS389_SENT_BYTES,
    FAM_DS389_ENTRIES_RETURNED,
    FAM_DS389_REFERRALS_RETURNED,
    FAM_DS389_DB_CACHE_HITS,
    FAM_DS389_DB_CACHE_TRIES,
    FAM_DS389_DB_CACHE_HIT_RATIO,
    FAM_DS389_DB_CACHE_PAGEIN,
    FAM_DS389_DB_CACHE_PAGEOUT,
    FAM_DS389_DB_CACHE_ROEVICT,
    FAM_DS389_DB_CACHE_RWEVICT,
    FAM_DS389_NDN_CACHE_TRIES,
    FAM_DS389_NDN_CACHE_HITS,
    FAM_DS389_NDN_CACHE_MISSES,
    FAM_DS389_NDN_CACHE_HIT_RATIO,
    FAM_DS389_NDN_CACHE_EVICTIONS,
    FAM_DS389_NDN_CACHE_SIZE_BYTES,
    FAM_DS389_NDN_CACHE_MAX_SIZE,
    FAM_DS389_NDN_CACHE_COUNT,
    FAM_DS389_NDN_CACHE_THREADSIZE,
    FAM_DS389_NDN_CACHE_THREADSLOTS,
    FAM_DS389_DB_ABORT_RATE,
    FAM_DS389_DB_ACTIVE_TXNS,
    FAM_DS389_DB_CACHE_REGION_WAIT,
    FAM_DS389_DB_CACHE_SIZE_BYTES,
    FAM_DS389_DB_CLEAN_PAGES,
    FAM_DS389_DB_COMMIT,
    FAM_DS389_DB_DEADLOCK,
    FAM_DS389_DB_DIRTY_PAGES,
    FAM_DS389_DB_HASH_BUCKETS,
    FAM_DS389_DB_HASH_ELEMENTS_EXAMINE,
    FAM_DS389_DB_HASH_SEARCH,
    FAM_DS389_DB_LOCK_CONFLICTS,
    FAM_DS389_DB_LOCK_REGION_WAIT,
    FAM_DS389_DB_LOCK_REQUEST,
    FAM_DS389_DB_LOCKERS,
    FAM_DS389_DB_CONFIGURED_LOCKS,
    FAM_DS389_DB_CURRENT_LOCKS,
    FAM_DS389_DB_MAX_LOCKS,
    FAM_DS389_DB_CURRENT_LOCK_OBJECTS,
    FAM_DS389_DB_MAX_LOCK_OBJECTS,
    FAM_DS389_DB_LOG_BYTES_SINCE_CHECKPOINT,
    FAM_DS389_DB_LOG_REGION_WAIT,
    FAM_DS389_DB_LOG_WRITE_RATE,
    FAM_DS389_DB_LONGEST_CHAIN_LENGTH,
    FAM_DS389_DB_PAGE_CREATE,
    FAM_DS389_DB_PAGE_READ,
    FAM_DS389_DB_PAGE_RO_EVICT,
    FAM_DS389_DB_PAGE_RW_EVICT,
    FAM_DS389_DB_PAGE_TRICKLE,
    FAM_DS389_DB_PAGE_WRITE,
    FAM_DS389_DB_PAGES_IN_USE,
    FAM_DS389_DB_TXN_REGION_WAIT,
    FAM_DS389_LINK_ADD,
    FAM_DS389_LINK_DELETE,
    FAM_DS389_LINK_MODIFY,
    FAM_DS389_LINK_RENAME,
    FAM_DS389_LINK_SEARCH_BASE,
    FAM_DS389_LINK_SEARCH_ONELEVEL,
    FAM_DS389_LINK_SEARCH_SUBTREE,
    FAM_DS389_LINK_ABANDON,
    FAM_DS389_LINK_BIND,
    FAM_DS389_LINK_UNBIND,
    FAM_DS389_LINK_COMPARE,
    FAM_DS389_LINK_CONNECTION_OPERATION,
    FAM_DS389_LINK_CONNECTION_BIND,
    FAM_DS389_BACKEND_READONLY,
    FAM_DS389_BACKEND_ENTRY_CACHE_HITS,
    FAM_DS389_BACKEND_ENTRY_CACHE_TRIES,
    FAM_DS389_BACKEND_ENTRY_CACHE_HIT_RATIO,
    FAM_DS389_BACKEND_ENTRY_CACHE_SIZE,
    FAM_DS389_BACKEND_ENTRY_CACHE_SIZE_MAX,
    FAM_DS389_BACKEND_ENTRY_CACHE_COUNT,
    FAM_DS389_BACKEND_ENTRY_CACHE_COUNT_MAX,
    FAM_DS389_BACKEND_DN_CACHE_HITS,
    FAM_DS389_BACKEND_DN_CACHE_TRIES,
    FAM_DS389_BACKEND_DN_CACHE_HIT_RATIO,
    FAM_DS389_BACKEND_DN_CACHE_SIZE,
    FAM_DS389_BACKEND_DN_CACHE_SIZE_MAX,
    FAM_DS389_BACKEND_DN_CACHE_COUNT,
    FAM_DS389_BACKEND_DN_CACHE_COUNT_MAX,
    FAM_DS389_BACKEND_DBFILE_CACHE_HIT,
    FAM_DS389_BACKEND_DBFILE_CACHE_MISS,
    FAM_DS389_BACKEND_DBFILE_PAGEIN,
    FAM_DS389_BACKEND_DBFILE_PAGEOUT,
    FAM_DS389_REPLICA_LAST_UPDATE_STATUS,
    FAM_DS389_REPLICA_LAST_UPDATE_START_SECONDS,
    FAM_DS389_REPLICA_LAST_UPDATE_END_SECONDS,
    FAM_DS389_MAX,
};

static metric_family_t fams_ds389[FAM_DS389_MAX] = {
    [FAM_DS389_UP] = {
        .name = "ds389_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the ds389 server be reached.",
    },
    [FAM_DS389_VERSION_INFO] = {
        .name = "ds389_version_info",
        .type = METRIC_TYPE_INFO,
        .help = "Ds389 server version.",
    },
    [FAM_DS389_START_TIME_SECONDS] = {
        .name = "ds389_start_time_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "The time the ds389 server was started in unix epoch.",
    },
    [FAM_DS389_THREADS] = {
        .name = "ds389_threads",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current number of active threads used for handling requests.",
    },
    [FAM_DS389_CURRENT_CONNECTIONS] = {
        .name = "ds389_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of connections currently in service by the directory.",
    },
    [FAM_DS389_CONNECTIONS] = {
        .name = "ds389_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of connections handled by the directory since it started.",
    },
    [FAM_DS389_CURRENT_CONNECTIONS_MAXTHREADS] = {
        .name = "ds389_connections_maxthreads",
        .type = METRIC_TYPE_GAUGE,
        .help = "All connections that are currently in a max thread state.",
    },
    [FAM_DS389_CONNECTIONS_MAXTHREADS] = {
        .name = "ds389_connections_maxthreads",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many times a connection hit max thread.",
    },
    [FAM_DS389_DTABLESIZE] = {
        .name = "ds389_dtablesize",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of file descriptors available to the directory.",
    },
    [FAM_DS389_READWAITERS] = {
        .name = "ds389_readwaiters",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of threads waiting to read data from a client.",
    },
    [FAM_DS389_OPS_INITIATED] = {
        .name = "ds389_ops_initiated",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of operations the server has initiated since it started.",
    },
    [FAM_DS389_OPS_COMPLETED] = {
        .name = "ds389_ops_completed",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of operations the server has completed since it started.",
    },
    [FAM_DS389_SEND_ENTRIES] = {
        .name = "ds389_send_entries",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of entries sent to clients since the server started.",
    },
    [FAM_DS389_NBACKENDS] = {
        .name = "ds389_nbackends",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of back ends (databases) the server services.",
    },
    [FAM_DS389_BINDS_ANONYMOUS] = {
        .name = "ds389_binds_anonymous",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of anonymous bind requests.",
    },
    [FAM_DS389_BINDS_UNAUTH] = {
        .name = "ds389_binds_unauth",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of unauthenticated (anonymous) binds.",
    },
    [FAM_DS389_BINDS_SIMPLEAUTH] = {
        .name = "ds389_binds_simpleauth",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of LDAP simple bind requests (DN and password).",
    },
    [FAM_DS389_BINDS_STRONGAUTH] = {
        .name = "ds389_binds_strongauth",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of LDAP SASL bind requests, for all SASL mechanisms.",
    },
    [FAM_DS389_BINDS_SECURITYERRORS] = {
        .name = "ds389_binds_securityerrors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of number of times an invalid password was given in a bind request.",
    },
    [FAM_DS389_OPS_IN] = {
        .name = "ds389_ops_in",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of all requests received by the server.",
    },
    [FAM_DS389_OPS_COMPARE] = {
        .name = "ds389_ops_compare",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of LDAP compare requests.",
    },
    [FAM_DS389_OPS_ADDENTRY] = {
        .name = "ds389_ops_addentry",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of LDAP add requests.",
    },
    [FAM_DS389_OPS_REMOVEENTRY] = {
        .name = "ds389_ops_removeentry",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of LDAP delete requests.",
    },
    [FAM_DS389_OPS_MODIFYENTRY] = {
        .name = "ds389_ops_modifyentry",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of LDAP modify requests.",
    },
    [FAM_DS389_OPS_MODIFYRDN] = {
        .name = "ds389_ops_modifyrdn",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of LDAP modify RDN (modrdn) requests.",
    },
    [FAM_DS389_OPS_SEARCH] = {
        .name = "ds389_ops_search",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of LDAP search requests.",
    },
    [FAM_DS389_OPS_ONELEVELSEARCH] = {
        .name = "ds389_ops_onelevelsearch",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of one-level search operations.",
    },
    [FAM_DS389_OPS_WHOLESUBTREESEARCH] = {
        .name = "ds389_ops_wholesubtreesearch",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of subtree-level search operations.",
    },
    [FAM_DS389_REFERRALS] = {
        .name = "ds389_referrals",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of LDAP referrals returned.",
    },
    [FAM_DS389_SECURITYERRORS] = {
        .name = "ds389_securityerrors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of errors returned that were security related, "
                "such as invalid passwords, unknown or invalid authentication methods, "
                "or stronger authentication required.",
    },
    [FAM_DS389_ERRORS] = {
        .name = "ds389_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of errors returned.",
    },
    [FAM_DS389_RECV_BYTES] = {
        .name = "ds389_recv_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bytes received.",
    },
    [FAM_DS389_SENT_BYTES] = {
        .name = "ds389_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bytes sent."
    },
    [FAM_DS389_ENTRIES_RETURNED] = {
        .name = "ds389_entries_returned",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of entries returned as search results.",
    },
    [FAM_DS389_REFERRALS_RETURNED] = {
        .name = "ds389_referrals_returned",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of referrals returned as search results (continuation references).",
    },
    [FAM_DS389_DB_CACHE_HITS] = {
        .name = "ds389_db_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times the database cache successfully supplied a requested page.",
    },
    [FAM_DS389_DB_CACHE_TRIES] = {
        .name = "ds389_db_cache_tries",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times the database cache was asked for a page.",
    },
    [FAM_DS389_DB_CACHE_HIT_RATIO] = {
        .name = "ds389_db_cache_hit_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "The ratio of database cache hits to database cache tries.",
    },
    [FAM_DS389_DB_CACHE_PAGEIN] = {
        .name = "ds389_db_cache_pagein",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of pages read from disk into the database cache.",
    },
    [FAM_DS389_DB_CACHE_PAGEOUT] = {
        .name = "ds389_db_cache_pageout",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of pages written from the cache back to disk.",
    },
    [FAM_DS389_DB_CACHE_ROEVICT] = {
        .name = "ds389_db_cache_roevict",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of read-only pages discarded from the cache to make room for new pages.",
    },
    [FAM_DS389_DB_CACHE_RWEVICT] = {
        .name = "ds389_db_cache_rwevict",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of read-write pages discarded from the cache to make room for new pages.",
    },
    [FAM_DS389_NDN_CACHE_TRIES] = {
        .name = "ds389_ndn_cache_tries",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of Normalized DNs cache lookups since the instance was started.",
    },
    [FAM_DS389_NDN_CACHE_HITS] = {
        .name = "ds389_ndn_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Normalized DNs found within the cache.",
    },
    [FAM_DS389_NDN_CACHE_MISSES] = {
        .name = "ds389_ndn_cache_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Normalized DNs not found within the cache.",
    },
    [FAM_DS389_NDN_CACHE_HIT_RATIO] = {
        .name = "ds389_ndn_cache_hit_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Percentage of the normalized DNs found in the cache.",
    },
    [FAM_DS389_NDN_CACHE_EVICTIONS] = {
        .name = "ds389_ndn_cache_evictions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Normalized DNs  discarded from the cache.",
    },
    [FAM_DS389_NDN_CACHE_SIZE_BYTES] = {
        .name = "ds389_ndn_cache_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current size of the normalized DN cache in bytes.",
    },
    [FAM_DS389_NDN_CACHE_MAX_SIZE] = {
        .name = "ds389_ndn_cache_max_size",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_DS389_NDN_CACHE_COUNT] = {
        .name = "ds389_ndn_cache_count",
        .type = METRIC_TYPE_GAUGE,
         .help = "The number of normalized cached DNs.",
    },
    [FAM_DS389_NDN_CACHE_THREADSIZE] = {
        .name = "ds389_ndn_cache_thread_size",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_DS389_NDN_CACHE_THREADSLOTS] = {
        .name = "ds389_ndn_cache_thread_slots",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_DS389_DB_ABORT_RATE] = {
        .name = "ds389_db_abort_rate",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of transactions that have been aborted.",
    },
    [FAM_DS389_DB_ACTIVE_TXNS] = {
        .name = "ds389_db_active_txns",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of transactions that are currently active.",
    },
    [FAM_DS389_DB_CACHE_REGION_WAIT] = {
        .name = "ds389_db_cache_region_wait",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of times that a thread of control was forced to "
                "wait before obtaining the region lock.",
    },
    [FAM_DS389_DB_CACHE_SIZE_BYTES] = {
        .name = "ds389_db_cache_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total cache size in bytes.",
    },
    [FAM_DS389_DB_CLEAN_PAGES] = {
        .name = "ds389_db_clean_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "The clean pages currently in the cache.",
    },
    [FAM_DS389_DB_COMMIT] = {
        .name = "ds389_db_commit",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of transactions that have been committed.",
    },
    [FAM_DS389_DB_DEADLOCK] = {
        .name = "ds389_db_deadlock",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of deadlocks detected.",
    },
    [FAM_DS389_DB_DIRTY_PAGES] = {
        .name = "ds389_db_dirty_pages",
        .type = METRIC_TYPE_GAUGE,
        .help = "The dirty pages currently in the cache.",
    },
    [FAM_DS389_DB_HASH_BUCKETS] = {
        .name = "ds389_db_hash_buckets",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of hash buckets in buffer hash table.",
    },
    [FAM_DS389_DB_HASH_ELEMENTS_EXAMINE] = {
        .name = "ds389_db_hash_elements_examine",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of hash elements traversed during hash table lookups.",
    },
    [FAM_DS389_DB_HASH_SEARCH] = {
        .name = "ds389_db_hash_search",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of buffer hash table lookup.",
    },
    [FAM_DS389_DB_LOCK_CONFLICTS] = {
        .name = "ds389_db_lock_conflicts",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of locks not immediately available due to conflicts.",
    },
    [FAM_DS389_DB_LOCK_REGION_WAIT] = {
        .name = "ds389_db_lock_region_wait",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times that a thread of control "
                "was forced to wait before obtaining the region lock.",
    },
    [FAM_DS389_DB_LOCK_REQUEST] = {
        .name = "ds389_db_lock_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of locks requested.",
    },
    [FAM_DS389_DB_LOCKERS] = {
        .name = "ds389_db_lockers",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of current lockers."
    },
    [FAM_DS389_DB_CONFIGURED_LOCKS] = {
        .name = "ds389_db_configured_locks",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_DS389_DB_CURRENT_LOCKS] = {
        .name = "ds389_db_current_locks",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_DS389_DB_MAX_LOCKS] = {
        .name = "ds389_db_max_locks",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_DS389_DB_CURRENT_LOCK_OBJECTS] = {
        .name = "ds389_db_current_lock_objects",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_DS389_DB_MAX_LOCK_OBJECTS] = {
        .name = "ds389_db_max_lock_objects",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_DS389_DB_LOG_BYTES_SINCE_CHECKPOINT] = {
        .name = "ds389_db_log_bytes_since_checkpoint",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bytes written to this log since the last checkpoint.",
    },
    [FAM_DS389_DB_LOG_REGION_WAIT] = {
        .name = "ds389_db_log_region_wait",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times that a thread of control "
                "was forced to wait before obtaining the region lock.",
    },
    [FAM_DS389_DB_LOG_WRITE_RATE] = {
        .name = "ds389_db_log_write_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes written to the log since the last checkpoint.",
    },
    [FAM_DS389_DB_LONGEST_CHAIN_LENGTH] = {
        .name = "ds389_db_longest_chain_length",
        .type = METRIC_TYPE_GAUGE,
        .help = "The longest chain ever encountered in buffer hash table lookups.",
    },
    [FAM_DS389_DB_PAGE_CREATE] = {
        .name = "ds389_db_page_create",
        .type = METRIC_TYPE_COUNTER,
        .help = "The pages created in the cache.",
    },
    [FAM_DS389_DB_PAGE_READ] = {
        .name = "ds389_db_page_read",
        .type = METRIC_TYPE_COUNTER,
        .help = "The pages read into the cache.",
    },
    [FAM_DS389_DB_PAGE_RO_EVICT] = {
        .name = "ds389_db_page_ro_evict",
        .type = METRIC_TYPE_COUNTER,
        .help = "The clean pages forced from the cache.",
    },
    [FAM_DS389_DB_PAGE_RW_EVICT] = {
        .name = "ds389_db_page_rw_evict",
        .type = METRIC_TYPE_COUNTER,
        .help = "The dirty pages forced from the cache.",
    },
    [FAM_DS389_DB_PAGE_TRICKLE] = {
        .name = "ds389_db_page_trickle",
        .type = METRIC_TYPE_COUNTER,
        .help = "The dirty pages written using the memp_trickle interface.",
    },
    [FAM_DS389_DB_PAGE_WRITE] = {
        .name = "ds389_db_page_write",
        .type = METRIC_TYPE_COUNTER,
        .help = "The pages read into the cache.",
    },
    [FAM_DS389_DB_PAGES_IN_USE] = {
        .name = "ds389_db_pages_in_use",
        .type = METRIC_TYPE_GAUGE,
        .help = "All pages, clean or dirty, currently in use."
    },
    [FAM_DS389_DB_TXN_REGION_WAIT] = {
        .name = "ds389_db_txn_region_wait",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times that a thread of control was force "
                "to wait before obtaining the region lock."
    },
    [FAM_DS389_LINK_ADD] = {
        .name = "ds389_link_add",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of add operations received.",
    },
    [FAM_DS389_LINK_DELETE] = {
        .name = "ds389_link_delete",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of delete operations received.",
    },
    [FAM_DS389_LINK_MODIFY] = {
        .name = "ds389_link_modify",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of modify operations received.",
    },
    [FAM_DS389_LINK_RENAME] = {
        .name = "ds389_link_rename",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of rename operations received.",
    },
    [FAM_DS389_LINK_SEARCH_BASE] = {
        .name = "ds389_link_search_base",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of base-level searches received.",
    },
    [FAM_DS389_LINK_SEARCH_ONELEVEL] = {
        .name = "ds389_link_search_onelevel",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of one-level searches received.",
    },
    [FAM_DS389_LINK_SEARCH_SUBTREE] = {
        .name = "ds389_link_search_subtree",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of subtree searches received.",
    },
    [FAM_DS389_LINK_ABANDON] = {
        .name = "ds389_link_abandon",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of abandon operations received.",
    },
    [FAM_DS389_LINK_BIND] = {
        .name = "ds389_link_bind",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of bind request received.",
    },
    [FAM_DS389_LINK_UNBIND] = {
        .name = "ds389_link_unbind",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of unbinds received.",
    },
    [FAM_DS389_LINK_COMPARE] = {
        .name = "ds389_link_compare",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of compare operations received.",
    },
    [FAM_DS389_LINK_CONNECTION_OPERATION] = {
        .name = "ds389_link_connection_operation",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of open connections for normal operations.",
    },
    [FAM_DS389_LINK_CONNECTION_BIND] = {
        .name = "ds389_link_connection_bind",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of open connections for bind operations.",
    },
    [FAM_DS389_BACKEND_READONLY] = {
        .name = "ds389_backend_readonly",
        .type = METRIC_TYPE_GAUGE,
        .help = "Indicates whether the database is in read-only mode; "
                "0 means that the server is not in read-only mode, "
                "1 means that it is in read-only mode.",
    },
    [FAM_DS389_BACKEND_ENTRY_CACHE_HITS] = {
        .name = "ds389_backend_entry_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of successful entry cache lookup.",
    },
    [FAM_DS389_BACKEND_ENTRY_CACHE_TRIES] = {
        .name = "ds389_backend_entry_cache_tries",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of entry cache lookups since the directory was last started.",
    },
    [FAM_DS389_BACKEND_ENTRY_CACHE_HIT_RATIO] = {
        .name = "ds389_backend_entry_cache_hit_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Ratio that indicates the number of entry cache tries "
                "to successful entry cache lookups.",
    },
    [FAM_DS389_BACKEND_ENTRY_CACHE_SIZE] = {
        .name = "ds389_backend_entry_cache_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total size, in bytes, of directory entries "
                "currently present in the entry cache",
    },
    [FAM_DS389_BACKEND_ENTRY_CACHE_SIZE_MAX] = {
        .name = "ds389_backend_entry_cache_size_max_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum size, in bytes, of directory entries "
                "that can be maintained in the entry cache.",
    },
    [FAM_DS389_BACKEND_ENTRY_CACHE_COUNT] = {
        .name = "ds389_backend_entry_cache_count",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of directory entries currently present in the entry cache.",
    },
    [FAM_DS389_BACKEND_ENTRY_CACHE_COUNT_MAX] = {
        .name = "ds389_backend_entry_cache_count_max",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum number of directory entries "
                "that can be maintained in the entry cache.",
    },
    [FAM_DS389_BACKEND_DN_CACHE_HITS] = {
        .name = "ds389_backend_dn_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times the server could process a request by "
                "obtaining data from the cache rather than by going to the disk.",
    },
    [FAM_DS389_BACKEND_DN_CACHE_TRIES] = {
        .name = "ds389_backend_dn_cache_tries",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of database accesses since server startup.",
    },
    [FAM_DS389_BACKEND_DN_CACHE_HIT_RATIO] = {
        .name = "ds389_backend_dn_cache_hit_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "The ratio of cache tries to successful cache hits.",
    },
    [FAM_DS389_BACKEND_DN_CACHE_SIZE] = {
        .name = "ds389_backend_dn_cache_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total size, in bytes, of DNs currently present in the DN cache.",
    },
    [FAM_DS389_BACKEND_DN_CACHE_SIZE_MAX] = {
        .name = "ds389_backend_dn_cache_size_max_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum size, in bytes, of DNs that can be maintained in the DN cache.",
    },
    [FAM_DS389_BACKEND_DN_CACHE_COUNT] = {
        .name = "ds389_backend_dn_cache_count",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of DNs currently present in the DN cache.",
    },
    [FAM_DS389_BACKEND_DN_CACHE_COUNT_MAX] = {
        .name = "ds389_backend_dn_cache_count_max",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum number of DNs allowed in the DN cache.",
    },
    [FAM_DS389_BACKEND_DBFILE_CACHE_HIT] = {
        .name = "ds389_backend_dbfile_cache_hit",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times that a search result resulted in a cache hit "
                "on this specific file.",
    },
    [FAM_DS389_BACKEND_DBFILE_CACHE_MISS] = {
        .name = "ds389_backend_dbfile_cache_miss",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of times that a search result failed to hit "
                "the cache on this specific file.",
    },
    [FAM_DS389_BACKEND_DBFILE_PAGEIN] = {
        .name = "ds389_backend_dbfile_pagein",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of pages brought to the cache from this file.",
    },
    [FAM_DS389_BACKEND_DBFILE_PAGEOUT] = {
        .name = "ds389_backend_dbfile_pageout",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of pages for this file written from cache to disk.",
    },
    [FAM_DS389_REPLICA_LAST_UPDATE_STATUS] = {
        .name = "ds389_replica_last_update_status",
        .type = METRIC_TYPE_GAUGE,
        .help = "Replica last update staus",
    },
    [FAM_DS389_REPLICA_LAST_UPDATE_START_SECONDS] = {
        .name = "ds389_replica_last_update_start",
        .type = METRIC_TYPE_GAUGE,
        .help = "Replica last update start time in unix epoch",
    },
    [FAM_DS389_REPLICA_LAST_UPDATE_END_SECONDS] = {
        .name = "ds389_replica_last_update_end",
        .type = METRIC_TYPE_GAUGE,
        .help = "Replica last update end time in unix epoch",
    },
};
