/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

enum {
    FAM_VARNISH_UP,
    FAM_VARNISH_MGT_UPTIME_SECONDS,
    FAM_VARNISH_MGT_CHILD_START,
    FAM_VARNISH_MGT_CHILD_EXIT,
    FAM_VARNISH_MGT_CHILD_STOP,
    FAM_VARNISH_MGT_CHILD_DIED,
    FAM_VARNISH_MGT_CHILD_DUMP,
    FAM_VARNISH_MGT_CHILD_PANIC,
    FAM_VARNISH_SUMMS,
    FAM_VARNISH_UPTIME_SECONDS,
    FAM_VARNISH_SESSION,
    FAM_VARNISH_SESSION_FAIL,
    FAM_VARNISH_SESSION_FAIL_ECONNABORTED,
    FAM_VARNISH_SESSION_FAIL_EINTR,
    FAM_VARNISH_SESSION_FAIL_EMFILE,
    FAM_VARNISH_SESSION_FAIL_EBADF,
    FAM_VARNISH_SESSION_FAIL_ENOMEM,
    FAM_VARNISH_SESSION_FAIL_OTHER,
    FAM_VARNISH_CLIENT_REQUEST_400,
    FAM_VARNISH_CLIENT_REQUEST_417,
    FAM_VARNISH_CLIENT_REQUEST,
    FAM_VARNISH_ESI_REQUESTS,
    FAM_VARNISH_CACHE_HIT,
    FAM_VARNISH_CACHE_HIT_GRACE,
    FAM_VARNISH_CACHE_HITPASS,
    FAM_VARNISH_CACHE_HITMISS,
    FAM_VARNISH_CACHE_MISS,
    FAM_VARNISH_BACKEND_TOTAL_RESPONSE_UNCACHEABLE,
    FAM_VARNISH_BACKEND_TOTAL_RESPONSE_SHORTLIVED,
    FAM_VARNISH_BACKEND_TOTAL_CONNECTION,
    FAM_VARNISH_BACKEND_TOTAL_UNHEALTHY,
    FAM_VARNISH_BACKEND_TOTAL_BUSY,
    FAM_VARNISH_BACKEND_TOTAL_FAIL,
    FAM_VARNISH_BACKEND_TOTAL_REUSE,
    FAM_VARNISH_BACKEND_TOTAL_RECYCLE,
    FAM_VARNISH_BACKEND_TOTAL_RETRY,
    FAM_VARNISH_FETCH_HEAD,
    FAM_VARNISH_FETCH_LENGTH,
    FAM_VARNISH_FETCH_CHUNKED,
    FAM_VARNISH_FETCH_EOF,
    FAM_VARNISH_FETCH_BAD,
    FAM_VARNISH_FETCH_NONE,
    FAM_VARNISH_FETCH_1XX,
    FAM_VARNISH_FETCH_204,
    FAM_VARNISH_FETCH_304,
    FAM_VARNISH_FETCH_FAST304,
    FAM_VARNISH_FETCH_STALE_DELIVER,
    FAM_VARNISH_FETCH_STALE_REARM,
    FAM_VARNISH_FETCH_FAILED,
    FAM_VARNISH_FETCH_NO_THREAD,
    FAM_VARNISH_THREAD_POOLS,
    FAM_VARNISH_THREADS,
    FAM_VARNISH_THREADS_LIMITED,
    FAM_VARNISH_THREADS_CREATED,
    FAM_VARNISH_THREADS_DESTROYED,
    FAM_VARNISH_THREADS_FAILED,
    FAM_VARNISH_THREAD_QUEUE_LEN,
    FAM_VARNISH_REQUEST_BUSY_SLEEP,
    FAM_VARNISH_REQUEST_BUSY_WAKEUP,
    FAM_VARNISH_REQUEST_BUSY_KILLED,
    FAM_VARNISH_SESSION_QUEUED,
    FAM_VARNISH_SESSION_DROPPED,
    FAM_VARNISH_REQUEST_DROPPED,
    FAM_VARNISH_OBJECTS,
    FAM_VARNISH_OBJECTS_VAMPIRE,
    FAM_VARNISH_OBJECTS_CORE,
    FAM_VARNISH_OBJECTS_HEAD,
    FAM_VARNISH_BACKENDS,
    FAM_VARNISH_OBJECTS_EXPIRED,
    FAM_VARNISH_OBJECTS_LRU_NUKED,
    FAM_VARNISH_OBJECTS_LRU_MOVED,
    FAM_VARNISH_OBJECTS_LRU_LIMITED,
    FAM_VARNISH_LOST_HDR,
    FAM_VARNISH_SESSIONS,
    FAM_VARNISH_PIPES,
    FAM_VARNISH_PIPE_LIMITED,
    FAM_VARNISH_SESSION_PIPE,
    FAM_VARNISH_PASS,
    FAM_VARNISH_FETCH,
    FAM_VARNISH_FETCH_BACKGROUND,
    FAM_VARNISH_SYNTH_RESPONSE,
    FAM_VARNISH_REQUEST_HDR_BYTES,
    FAM_VARNISH_REQUEST_BODY_BYTES,
    FAM_VARNISH_RESPONSE_HDR_BYTES,
    FAM_VARNISH_RESPONSE_BODY_BYTES,
    FAM_VARNISH_PIPE_HDR_BYTES,
    FAM_VARNISH_PIPE_IN_BYTES,
    FAM_VARNISH_PIPE_OUT_BYTES,
    FAM_VARNISH_SESSION_CLOSED,
    FAM_VARNISH_SESSION_CLOSED_ERROR,
    FAM_VARNISH_SESSION_READAHEAD,
    FAM_VARNISH_SESSION_HERD,
    FAM_VARNISH_SESSION_CLOSE,
    FAM_VARNISH_WORKSPACE_BACKEND_OVERFLOW,
    FAM_VARNISH_WORKSPACE_CLIENT_OVERFLOW,
    FAM_VARNISH_WORKSPACE_THREAD_OVERFLOW,
    FAM_VARNISH_WORKSPACE_SESSION_OVERFLOW,
    FAM_VARNISH_SHM_RECORDS,
    FAM_VARNISH_SHM_WRITES,
    FAM_VARNISH_SHM_FLUSHES,
    FAM_VARNISH_SHM_CONTENTION,
    FAM_VARNISH_SHM_CYCLES,
    FAM_VARNISH_SHM_BYTES,
    FAM_VARNISH_BACKEND_TOTAL_REQUEST,
    FAM_VARNISH_VCL,
    FAM_VARNISH_VCL_AVAIL,
    FAM_VARNISH_VCL_DISCARD,
    FAM_VARNISH_VCL_FAIL,
    FAM_VARNISH_CLIENT_RESPONSE_500,
    FAM_VARNISH_BANS,
    FAM_VARNISH_BANS_COMPLETED,
    FAM_VARNISH_BANS_OBJ,
    FAM_VARNISH_BANS_REQ,
    FAM_VARNISH_BANS_ADDED,
    FAM_VARNISH_BANS_DELETED,
    FAM_VARNISH_BANS_TESTED,
    FAM_VARNISH_BANS_OBJ_KILLED,
    FAM_VARNISH_BANS_LURKER_TESTED,
    FAM_VARNISH_BANS_TESTS_TESTED,
    FAM_VARNISH_BANS_LURKER_TESTS_TESTED,
    FAM_VARNISH_BANS_LURKER_OBJ_KILLED,
    FAM_VARNISH_BANS_LURKER_OBJ_KILLED_CUTOFF,
    FAM_VARNISH_BANS_DUPS,
    FAM_VARNISH_BANS_LURKER_CONTENTION,
    FAM_VARNISH_BANS_PERSISTED_BYTES,
    FAM_VARNISH_BANS_PERSISTED_FRAGMENTATION_BYTES,
    FAM_VARNISH_PURGES,
    FAM_VARNISH_PURGED_OBJECTS,
    FAM_VARNISH_EXPIRY_MAILED,
    FAM_VARNISH_EXPIRY_RECEIVED,
    FAM_VARNISH_HCB_NOLOCK,
    FAM_VARNISH_HCB_LOCK,
    FAM_VARNISH_HCB_INSERT,
    FAM_VARNISH_ESI_ERRORS,
    FAM_VARNISH_ESI_WARNINGS,
    FAM_VARNISH_ESI_MAX_DEPTH,
    FAM_VARNISH_VMODS,
    FAM_VARNISH_GZIP,
    FAM_VARNISH_GUNZIP,
    FAM_VARNISH_TEST_GUNZIP,
    FAM_VARNISH_GOTO_DNS_CACHE_HITS,
    FAM_VARNISH_GOTO_DNS_LOOKUPS,
    FAM_VARNISH_GOTO_DNS_LOOKUP_FAILS,
    FAM_VARNISH_BROTLI_COMPRESSIONS,
    FAM_VARNISH_BROTLI_IN_BYTES,
    FAM_VARNISH_BROTLI_OUT_BYTES,
    FAM_VARNISH_BROTLI_COMPRESSIONS_FAILURES,
    FAM_VARNISH_BROTLI_DECOMPRESSIONS,
    FAM_VARNISH_BROTLI_DECOMPRESSOR_IN_BYTES,
    FAM_VARNISH_BROTLI_DECOMPRESSOR_OUT_BYTES,
    FAM_VARNISH_BROTLI_DECOMPRESSOR_FAILURES,
    FAM_VARNISH_BROTLI_TEST_DECOMPRESSIONS,
    FAM_VARNISH_BROTLI_TEST_DECOMPRESSIONS_FAILURES,
    FAM_VARNISH_MEMPOOL_LIVE,
    FAM_VARNISH_MEMPOOL_POOL,
    FAM_VARNISH_MEMPOOL_SIZE_WANTED_BYTES,
    FAM_VARNISH_MEMPOOL_SIZE_ACTUAL_BYTES,
    FAM_VARNISH_MEMPOOL_ALLOCIONS,
    FAM_VARNISH_MEMPOOL_FREES,
    FAM_VARNISH_MEMPOOL_RECYCLE,
    FAM_VARNISH_MEMPOOL_TIMEOUT,
    FAM_VARNISH_MEMPOOL_TOOSMALL,
    FAM_VARNISH_MEMPOOL_SURPLUS,
    FAM_VARNISH_MEMPOOL_RANDRY,
    FAM_VARNISH_LOCK_CREATED,
    FAM_VARNISH_LOCK_DESTROY,
    FAM_VARNISH_LOCK_LOCKS,
    FAM_VARNISH_LOCK_DBG_BUSY,
    FAM_VARNISH_LOCK_DBG_TRY_FAIL,
    FAM_VARNISH_SMA_ALLOC_REQUEST,
    FAM_VARNISH_SMA_ALLOC_FAIL,
    FAM_VARNISH_SMA_ALLOCATED_BYTES,
    FAM_VARNISH_SMA_FREED_BYTES,
    FAM_VARNISH_SMA_ALLOC_OUTSTANDING,
    FAM_VARNISH_SMA_OUTSTANDING_BYTES,
    FAM_VARNISH_SMA_AVAILABLE_BYTES,
    FAM_VARNISH_SMA_YKEY_KEYS,
    FAM_VARNISH_SMA_YKEY_PURGED,
    FAM_VARNISH_SMF_ALLOC_REQUEST,
    FAM_VARNISH_SMF_ALLOC_FAIL,
    FAM_VARNISH_SMF_ALLOCATED_BYTES,
    FAM_VARNISH_SMF_FREED_BYTES,
    FAM_VARNISH_SMF_ALLOC_OUTSTANDING,
    FAM_VARNISH_SMF_OUTSTANDING_BYTES,
    FAM_VARNISH_SMF_AVAILABLE_BYTES,
    FAM_VARNISH_SMF_STRUCTS,
    FAM_VARNISH_SMF_STRUCTS_SMALL_FREE,
    FAM_VARNISH_SMF_STRUCTS_LARGE_FREE,
    FAM_VARNISH_SMU_ALLOC_REQUEST,
    FAM_VARNISH_SMU_ALLOC_FAIL,
    FAM_VARNISH_SMU_ALLOCATED_BYTES,
    FAM_VARNISH_SMU_FREED_BYTES,
    FAM_VARNISH_SMU_ALLOC_OUTSTANDING,
    FAM_VARNISH_SMU_OUTSTANDING_BYTES,
    FAM_VARNISH_SMU_AVAILABLE_BYTES,
    FAM_VARNISH_MSE_ALLOC_REQUEST,
    FAM_VARNISH_MSE_ALLOC_FAIL,
    FAM_VARNISH_MSE_ALLOCATED_BYTES,
    FAM_VARNISH_MSE_FREED_BYTES,
    FAM_VARNISH_MSE_ALLOC_OUTSTANDING,
    FAM_VARNISH_MSE_OUTSTANDING_BYTES,
    FAM_VARNISH_MSE_AVAILABLE_BYTES,
    FAM_VARNISH_MSE_FAIL_MALLOC,
    FAM_VARNISH_MSE_MEMCACHE_HIT,
    FAM_VARNISH_MSE_MEMCACHE_MISS,
    FAM_VARNISH_MSE_YKEY_KEYS,
    FAM_VARNISH_MSE_YKEY_PURGED,
    FAM_VARNISH_MSE_LRU_MOVED,
    FAM_VARNISH_MSE_LRU_NUKED,
    FAM_VARNISH_MSE_VARY,
    FAM_VARNISH_VBE_UP,
    FAM_VARNISH_VBE_REQUEST_HDR_BYTES,
    FAM_VARNISH_VBE_REQUEST_BODY_BYTES,
    FAM_VARNISH_VBE_RESPONSE_HDR_BYTES,
    FAM_VARNISH_VBE_RESPONSE_BODY_BYTES,
    FAM_VARNISH_VBE_PIPE_HDR_BYTES,
    FAM_VARNISH_VBE_PIPE_OUT_BYTES,
    FAM_VARNISH_VBE_PIPE_IN_BYTES,
    FAM_VARNISH_VBE_CONNECTIONS,
    FAM_VARNISH_VBE_REQUESTS,
    FAM_VARNISH_VBE_UNHEALTHY,
    FAM_VARNISH_VBE_BUSY,
    FAM_VARNISH_VBE_FAIL,
    FAM_VARNISH_VBE_HELDDOWN,
    FAM_VARNISH_ACCG_DIAG_SET_KEY_FAILURE,
    FAM_VARNISH_ACCG_DIAG_OUT_OF_KEY_SLOTS,
    FAM_VARNISH_ACCG_DIAG_KEY_WITHOUT_NAMESPACE,
    FAM_VARNISH_ACCG_DIAG_NAMESPACE_ALREADY_SET,
    FAM_VARNISH_ACCG_DIAG_NAMESPACE_UNDEFINED,
    FAM_VARNISH_ACCG_DIAG_CREATE_NAMESPACE_FAILURE,
    FAM_VARNISH_ACCG_CLIENT_REQUEST,
    FAM_VARNISH_ACCG_CLIENT_REQUEST_HDR_BYTES,
    FAM_VARNISH_ACCG_CLIENT_REQUEST_BODY_BYTES,
    FAM_VARNISH_ACCG_CLIENT_RESPONSE_HDR_BYTES,
    FAM_VARNISH_ACCG_CLIENT_RESPONSE_BODY_BYTES,
    FAM_VARNISH_ACCG_CLIENT_HIT,
    FAM_VARNISH_ACCG_CLIENT_HIT_REQUEST_HDR_BYTES,
    FAM_VARNISH_ACCG_CLIENT_HIT_REQUEST_BODY_BYTES,
    FAM_VARNISH_ACCG_CLIENT_HIT_RESPONSE_HDR_BYTES,
    FAM_VARNISH_ACCG_CLIENT_HIT_RESPONSE_BODY_BYTES,
    FAM_VARNISH_ACCG_CLIENT_MISS,
    FAM_VARNISH_ACCG_CLIENT_MISS_REQUEST_HDR_BYTES,
    FAM_VARNISH_ACCG_CLIENT_MISS_REQUEST_BODY_BYTES,
    FAM_VARNISH_ACCG_CLIENT_MISS_RESPONSE_HDR_BYTES,
    FAM_VARNISH_ACCG_CLIENT_MISS_RESPONSE_BODY_BYTES,
    FAM_VARNISH_ACCG_CLIENT_PASS,
    FAM_VARNISH_ACCG_CLIENT_PASS_REQUEST_HDR_BYTES,
    FAM_VARNISH_ACCG_CLIENT_PASS_REQUEST_BODY_BYTES,
    FAM_VARNISH_ACCG_CLIENT_PASS_RESPONSE_HDR_BYTES,
    FAM_VARNISH_ACCG_CLIENT_PASS_RESPONSE_BODY_BYTES,
    FAM_VARNISH_ACCG_CLIENT_SYNTH,
    FAM_VARNISH_ACCG_CLIENT_SYNTH_REQUEST_HDR_BYTES,
    FAM_VARNISH_ACCG_CLIENT_SYNTH_REQUEST_BODY_BYTES,
    FAM_VARNISH_ACCG_CLIENT_SYNTH_RESPONSE_HDR_BYTES,
    FAM_VARNISH_ACCG_CLIENT_SYNTH_RESPONSE_BODY_BYTES,
    FAM_VARNISH_ACCG_CLIENT_PIPE,
    FAM_VARNISH_ACCG_CLIENT_PIPE_REQUEST_HDR_BYTES,
    FAM_VARNISH_ACCG_CLIENT_PIPE_REQUEST_BODY_BYTES,
    FAM_VARNISH_ACCG_CLIENT_PIPE_RESPONSE_HDR_BYTES,
    FAM_VARNISH_ACCG_CLIENT_PIPE_RESPONSE_BODY_BYTES,
    FAM_VARNISH_ACCG_CLIENT_REPONSE,
    FAM_VARNISH_ACCG_BACKEND_REQUEST,
    FAM_VARNISH_ACCG_BACKEND_REQUEST_HDR_BYTES,
    FAM_VARNISH_ACCG_BACKEND_REQUEST_BODY_BYTES,
    FAM_VARNISH_ACCG_BACKEND_RESPONSE_HDR_BYTES,
    FAM_VARNISH_ACCG_BACKEND_RESPONSE_BODY_BYTES,
    FAM_VARNISH_ACCG_BACKEND_RESPONSE,

    FAM_VARNISH_MAX,
};
