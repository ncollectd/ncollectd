// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libmetric/metric.h"
#include "plugins/varnish/varnish_fam.h"

metric_family_t fams[FAM_VARNISH_MAX] = {
    [FAM_VARNISH_UP] = {
        .name = "varnish_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "If varnish process is running.",
    },
    [FAM_VARNISH_MGT_UPTIME_SECONDS] = {
        .name = "varnish_mgt_uptime_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Uptime in seconds of the management process.",
    },
    [FAM_VARNISH_MGT_CHILD_START] = {
        .name = "varnish_mgt_child_start",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the child process has been started.",
    },
    [FAM_VARNISH_MGT_CHILD_EXIT] = {
        .name = "varnish_mgt_child_exit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the child process has been cleanly stopped.",
    },
    [FAM_VARNISH_MGT_CHILD_STOP] = {
        .name = "varnish_mgt_child_stop",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the child process has exited with an unexpected return code.",
    },
    [FAM_VARNISH_MGT_CHILD_DIED] = {
        .name = "varnish_mgt_child_died",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the child process has died due to signals.",
    },
    [FAM_VARNISH_MGT_CHILD_DUMP] = {
        .name = "varnish_mgt_child_dump",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the child process has produced core dumps.",
    },
    [FAM_VARNISH_MGT_CHILD_PANIC] = {
        .name = "varnish_mgt_child_panic",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the management process has caught a child panic.",
    },
    [FAM_VARNISH_SUMMS] = {
        .name = "varnish_summs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times per-thread statistics were summed into the global counters.",
    },
    [FAM_VARNISH_UPTIME_SECONDS] = {
        .name = "varnish_uptime_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "How long the child process has been running.",
    },
    [FAM_VARNISH_SESSION] = {
        .name = "varnish_session",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of sessions successfully accepted.",
    },
    [FAM_VARNISH_SESSION_FAIL] = {
        .name = "varnish_session_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of failures to accept TCP connection.",
    },
    [FAM_VARNISH_SESSION_FAIL_ECONNABORTED] = {
        .name = "varnish_session_fail_econnaborted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Session accept failures: connection aborted by the client.",
    },
    [FAM_VARNISH_SESSION_FAIL_EINTR] = {
        .name = "varnish_session_fail_eintr",
        .type = METRIC_TYPE_COUNTER,
        .help = "Session accept failures: the accept() call was interrupted.",
    },
    [FAM_VARNISH_SESSION_FAIL_EMFILE] = {
        .name = "varnish_session_fail_emfile",
        .type = METRIC_TYPE_COUNTER,
        .help = "Session accept failures: too many open files.",
    },
    [FAM_VARNISH_SESSION_FAIL_EBADF] = {
        .name = "varnish_session_fail_ebadf",
        .type = METRIC_TYPE_COUNTER,
        .help = "Session accept failures: the listen socket file descriptor was invalid.",
    },
    [FAM_VARNISH_SESSION_FAIL_ENOMEM] = {
        .name = "varnish_session_fail_enomem",
        .type = METRIC_TYPE_COUNTER,
        .help = "Session accept failures: most likely insufficient socket buffer memory.",
    },
    [FAM_VARNISH_SESSION_FAIL_OTHER] = {
        .name = "varnish_session_fail_other",
        .type = METRIC_TYPE_COUNTER,
        .help = "Session accept failures: other.",
    },
    [FAM_VARNISH_CLIENT_REQUEST_400] = {
        .name = "varnish_client_request_400",
        .type = METRIC_TYPE_COUNTER,
        .help = "Client requests received, subject to 400 errors: means we couldn't "
                "make sense of the request, it was malformed in some drastic way.",
    },
    [FAM_VARNISH_CLIENT_REQUEST_417] = {
        .name = "varnish_client_request_417",
        .type = METRIC_TYPE_COUNTER,
        .help = "Client requests received, subject to 417 errors: "
                "means that something went wrong with an Expect: header.",
    },
    [FAM_VARNISH_CLIENT_REQUEST] = {
        .name = "varnish_client_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "The count of parseable client requests seen.",
    },
    [FAM_VARNISH_ESI_REQUESTS] = {
        .name = "varnish_esi_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of ESI subrequests made.",
    },
    [FAM_VARNISH_CACHE_HIT] = {
        .name = "varnish_cache_hit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of cache hits. A cache hit indicates that an object has been "
                "delivered to a client without fetching it from a backend server.",
    },
    [FAM_VARNISH_CACHE_HIT_GRACE] = {
        .name = "varnish_cache_hit_grace",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of cache hits with grace. A cache hit with grace "
                "is a cache hit where the object is expired.",
    },
    [FAM_VARNISH_CACHE_HITPASS] = {
        .name = "varnish_cache_hitpass",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of hits for pass. A cache hit for pass indicates that Varnish is going "
                "to pass the request to the backend and this decision has been cached in it self.",
    },
    [FAM_VARNISH_CACHE_HITMISS] = {
        .name = "varnish_cache_hitmiss",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of hits for miss. A cache hit for miss indicates that Varnish is going "
                "to proceed as for a cache miss without request coalescing, "
                "and this decision has been cached.",
    },
    [FAM_VARNISH_CACHE_MISS] = {
        .name = "varnish_cache_miss",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of misses. A cache miss indicates the object was fetched "
                "from the backend before delivering it to the client.",
    },
    [FAM_VARNISH_BACKEND_TOTAL_RESPONSE_UNCACHEABLE] = {
        .name = "varnish_total_backend_total_response_uncacheable",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of backend responses considered uncacheable.",
    },
    [FAM_VARNISH_BACKEND_TOTAL_RESPONSE_SHORTLIVED] = {
        .name = "varnish_backend_total_response_shortlived",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of objects created with ttl+grace+keep "
                "shorter than the 'shortlived' runtime parameter.",
    },
    [FAM_VARNISH_BACKEND_TOTAL_CONNECTION] = {
        .name = "varnish_backend_total_connection",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many backend connections have successfully been established.",
    },
    [FAM_VARNISH_BACKEND_TOTAL_UNHEALTHY] = {
        .name = "varnish_backend_total_unhealthy",
        .type = METRIC_TYPE_COUNTER,
        .help = "Backend conn. not attempted.",
    },
    [FAM_VARNISH_BACKEND_TOTAL_BUSY] = {
        .name = "varnish_backend_total_busy",
        .type = METRIC_TYPE_COUNTER,
        .help = "Backend conn. too many.",
    },
    [FAM_VARNISH_BACKEND_TOTAL_FAIL] = {
        .name = "varnish_backend_total_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Backend conn. failures.",
    },
    [FAM_VARNISH_BACKEND_TOTAL_REUSE] = {
        .name = "varnish_backend_total_reuse",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of backend connection reuses. This counter is increased whenever "
                "we reuse a recycled connection.",
    },
    [FAM_VARNISH_BACKEND_TOTAL_RECYCLE] = {
        .name = "varnish_backend_total_recycle",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of backend connection recycles. This counter is increased whenever "
                "we have a keep-alive connection that is put back into the pool of connections.",
    },
    [FAM_VARNISH_BACKEND_TOTAL_RETRY] = {
        .name = "varnish_backend_total_retry",
        .type = METRIC_TYPE_COUNTER,
        .help = "Backend conn. retry.",
    },
    [FAM_VARNISH_FETCH_HEAD] = {
        .name = "varnish_fetch_head",
        .type = METRIC_TYPE_COUNTER,
        .help = "Fetch no body because the request is HEAD.",
    },
    [FAM_VARNISH_FETCH_LENGTH] = {
        .name = "varnish_fetch_length",
        .type = METRIC_TYPE_COUNTER,
        .help = "Fetch body with Content-Length.",
    },
    [FAM_VARNISH_FETCH_CHUNKED] = {
        .name = "varnish_fetch_chunked",
        .type = METRIC_TYPE_COUNTER,
        .help = "Fetch witch chunked.",
    },
    [FAM_VARNISH_FETCH_EOF] = {
        .name = "varnish_fetch_eof",
        .type = METRIC_TYPE_COUNTER,
        .help = "Fetch body with EOF",
    },
    [FAM_VARNISH_FETCH_BAD] = {
        .name = "varnish_fetch_bad",
        .type = METRIC_TYPE_COUNTER,
        .help = "Fetch body length/fetch could not be determined.",
    },
    [FAM_VARNISH_FETCH_NONE] = {
        .name = "varnish_fetch_none",
        .type = METRIC_TYPE_COUNTER,
        .help = "Fetch no body",
    },
    [FAM_VARNISH_FETCH_1XX] = {
        .name = "varnish_fetch_1xx",
        .type = METRIC_TYPE_COUNTER,
        .help = "Fetch no body because of 1XX response.",
    },
    [FAM_VARNISH_FETCH_204] = {
        .name = "varnish_fetch_204",
        .type = METRIC_TYPE_COUNTER,
        .help = "Fetch no body because of 204 response.",
    },
    [FAM_VARNISH_FETCH_304] = {
        .name = "varnish_fetch_304",
        .type = METRIC_TYPE_COUNTER,
        .help = "Fetch with no body because of 304 response.",
    },
    [FAM_VARNISH_FETCH_FAST304] = {
        .name = "varnish_fetch_fast304",
        .type = METRIC_TYPE_COUNTER,
        .help = "Fast 304 inserts.",
    },
    [FAM_VARNISH_FETCH_STALE_DELIVER] = {
        .name = "varnish_fetch_stale_deliver",
        .type = METRIC_TYPE_COUNTER,
        .help = "Stale deliveries.",
    },
    [FAM_VARNISH_FETCH_STALE_REARM] = {
        .name = "varnish_fetch_stale_rearm",
        .type = METRIC_TYPE_COUNTER,
        .help = "Stale object rearmed.",
    },
    [FAM_VARNISH_FETCH_FAILED] = {
        .name = "varnish_fetch_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Fetch failed (all causes).",
    },
    [FAM_VARNISH_FETCH_NO_THREAD] = {
        .name = "varnish_fetch_no_thread",
        .type = METRIC_TYPE_COUNTER,
        .help = "Fetch failed no thread available.",
    },
    [FAM_VARNISH_THREAD_POOLS] = {
        .name = "varnish_thread_pools",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of thread pools.",
    },
    [FAM_VARNISH_THREADS] = {
        .name = "varnish_threads",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of threads in all pools.",
    },
    [FAM_VARNISH_THREADS_LIMITED] = {
        .name = "varnish_threads_limited",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times more threads were needed, but limit was reached in a thread pool.",
    },
    [FAM_VARNISH_THREADS_CREATED] = {
        .name = "varnish_threads_created",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of threads created in all pools.",
    },
    [FAM_VARNISH_THREADS_DESTROYED] = {
        .name = "varnish_threads_destroyed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of threads destroyed in all pools.",
    },
    [FAM_VARNISH_THREADS_FAILED] = {
        .name = "varnish_threads_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times creating a thread failed.",
    },
    [FAM_VARNISH_THREAD_QUEUE_LEN] = {
        .name = "varnish_thread_queue_len",
        .type = METRIC_TYPE_GAUGE,
        .help = "Length of session queue waiting for threads.",
    },
    [FAM_VARNISH_REQUEST_BUSY_SLEEP] = {
        .name = "varnish_request_busy_sleep",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of requests sent to sleep without a worker thread because "
                "they found a busy object.",
    },
    [FAM_VARNISH_REQUEST_BUSY_WAKEUP] = {
        .name = "varnish_request_busy_wakeup",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of requests taken off the busy object sleep list and rescheduled.",
    },
    [FAM_VARNISH_REQUEST_BUSY_KILLED] = {
        .name = "varnish_request_busy_killed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of requests killed from the busy object sleep list due "
                "to lack of resources.",
    },
    [FAM_VARNISH_SESSION_QUEUED] = {
        .name = "varnish_session_queued",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times session was queued waiting for a thread."
    },
    [FAM_VARNISH_SESSION_DROPPED] = {
        .name = "varnish_session_dropped",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times an HTTP/1 session was dropped because "
                "the queue was too long already.",
    },
    [FAM_VARNISH_REQUEST_DROPPED] = {
        .name = "varnish_request_dropped",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times an HTTP/2 stream was refused because "
                "the queue was too long already.",
    },
    [FAM_VARNISH_OBJECTS] = {
        .name = "varnish_objects",
        .type = METRIC_TYPE_GAUGE,
        .help = "Approximate number of HTTP objects (headers + body, if present) in the cache.",
    },
    [FAM_VARNISH_OBJECTS_VAMPIRE] = {
        .name = "varnish_objects_vampire",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of unresurrected objects",
    },
    [FAM_VARNISH_OBJECTS_CORE] = {
        .name = "varnish_objects_core",
        .type = METRIC_TYPE_GAUGE,
        .help = "Approximate number of object metadata elements in the cache.",
    },
    [FAM_VARNISH_OBJECTS_HEAD] = {
        .name = "varnish_objects_head",
        .type = METRIC_TYPE_GAUGE,
        .help = "Approximate number of different hash entries in the cache.",
    },
    [FAM_VARNISH_BACKENDS] = {
        .name = "varnish_backends",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of backends known to us.",
    },
    [FAM_VARNISH_OBJECTS_EXPIRED] = {
        .name = "varnish_objects_expired",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of objects that expired from cache because of old age.",
    },
    [FAM_VARNISH_OBJECTS_LRU_NUKED] = {
        .name = "varnish_objects_lru_nuked",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many objects have been forcefully evicted from storage "
                "to make room for a new object.",
    },
    [FAM_VARNISH_OBJECTS_LRU_MOVED] = {
        .name = "varnish_objects_lru_moved",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of move operations done on the LRU list.",
    },
    [FAM_VARNISH_OBJECTS_LRU_LIMITED] = {
        .name = "varnish_objects_lru_limited",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times more storage space were needed, "
                "but limit was reached in a nuke_limit.",
    },
    [FAM_VARNISH_LOST_HDR] = {
        .name = "varnish_lost_hdr",
        .type = METRIC_TYPE_COUNTER,
        .help = "HTTP header overflows.",
    },
    [FAM_VARNISH_SESSIONS] = {
        .name = "varnish_sessions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total sessions seen.",
    },
    [FAM_VARNISH_PIPES] = {
        .name = "varnish_pipes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of ongoing pipe sessions.",
    },
    [FAM_VARNISH_PIPE_LIMITED] = {
        .name = "varnish_pipe_limited",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times more pipes were needed, but the limit was reached.",
    },
    [FAM_VARNISH_SESSION_PIPE] = {
        .name = "varnish_session_pipe",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total pipe sessions seen.",
    },
    [FAM_VARNISH_PASS] = {
        .name = "varnish_pass",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total pass-ed requests seen.",
    },
    [FAM_VARNISH_FETCH] = {
        .name = "varnish_fetch",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total backend fetches initiated, including background fetches.",
    },
    [FAM_VARNISH_FETCH_BACKGROUND] = {
        .name = "varnish_fetch_background",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total backend background fetches initiated.",
    },
    [FAM_VARNISH_SYNTH_RESPONSE] = {
        .name = "varnish_synth_response",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total synthetic responses made.",
    },
    [FAM_VARNISH_REQUEST_HDR_BYTES] = {
        .name = "varnish_request_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total request header bytes received.",
    },
    [FAM_VARNISH_REQUEST_BODY_BYTES] = {
        .name = "varnish_request_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total request body bytes received.",
    },
    [FAM_VARNISH_RESPONSE_HDR_BYTES] = {
        .name = "varnish_response_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total response header bytes transmitted.",
    },
    [FAM_VARNISH_RESPONSE_BODY_BYTES] = {
        .name = "varnish_response_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total response body bytes transmitted.",
    },
    [FAM_VARNISH_PIPE_HDR_BYTES] = {
        .name = "varnish_pipe_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total request bytes received for piped sessions.",
    },
    [FAM_VARNISH_PIPE_IN_BYTES] = {
        .name = "varnish_pipe_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes forwarded from clients in pipe sessions.",
    },
    [FAM_VARNISH_PIPE_OUT_BYTES] = {
        .name = "varnish_pipe_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes forwarded to clients in pipe sessions.",
    },
    [FAM_VARNISH_SESSION_CLOSED] = {
        .name = "varnish_session_closed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Session Closed.",
    },
    [FAM_VARNISH_SESSION_CLOSED_ERROR] = {
        .name = "varnish_session_closed_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of sessions closed with errors.",
    },
    [FAM_VARNISH_SESSION_READAHEAD] = {
        .name = "varnish_session_readahead",
        .type = METRIC_TYPE_COUNTER,
        .help = "Session Read Ahead.",
    },
    [FAM_VARNISH_SESSION_HERD] = {
        .name = "varnish_session_herd",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the timeout_linger triggered.",
    },
    [FAM_VARNISH_SESSION_CLOSE] = {
        .name = "varnish_main_session_close",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of session closed by reason.",
    },
    [FAM_VARNISH_CLIENT_RESPONSE_500] = {
        .name = "varnish_client_response_500",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times we failed a response due to running out of "
                "workspace memory during delivery.",
    },
    [FAM_VARNISH_WORKSPACE_BACKEND_OVERFLOW] = {
        .name = "varnish_workspace_backend_overflow",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times we ran out of space in workspace_backend.",
    },
    [FAM_VARNISH_WORKSPACE_CLIENT_OVERFLOW] = {
        .name = "varnish_workspace_client_overflow",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times we ran out of space in workspace_client.",
    },
    [FAM_VARNISH_WORKSPACE_THREAD_OVERFLOW] = {
        .name = "varnish_workspace_thread_overflow",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times we ran out of space in workspace_thread.",
    },
    [FAM_VARNISH_WORKSPACE_SESSION_OVERFLOW] = {
        .name = "varnish_workspace_session_overflow",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times we ran out of space in workspace_session.",
    },
    [FAM_VARNISH_SHM_RECORDS] = {
        .name = "varnish_shm_records",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of log records written to the shared memory log.",
    },
    [FAM_VARNISH_SHM_WRITES] = {
        .name = "varnish_shm_writes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of individual writes to the shared memory log."
    },
    [FAM_VARNISH_SHM_FLUSHES] = {
        .name = "varnish_shm_flushes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of writes performed before the end of a buffered task "
                "because adding a record to a batch would exceed vsl_buffer.",
    },
    [FAM_VARNISH_SHM_CONTENTION] = {
        .name = "varnish_shm_contention",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a write had to wait for the lock.",
    },
    [FAM_VARNISH_SHM_CYCLES] = {
        .name = "varnish_shm_cycles",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times a write of log records would reach past the end "
                "of the shared memory log, cycling back to the beginning."
    },
    [FAM_VARNISH_SHM_BYTES] = {
        .name = "varnish_shm_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes written to the shared memory log.",
    },
    [FAM_VARNISH_BACKEND_TOTAL_REQUEST] = {
        .name = "varnish_backend_total_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "Backend requests made",
    },
    [FAM_VARNISH_VCL] = {
        .name = "varnish_vcl",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of loaded VCLs in total",
    },
    [FAM_VARNISH_VCL_AVAIL] = {
        .name = "varnish_vcl_avail",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of VCLs available",
    },
    [FAM_VARNISH_VCL_DISCARD] = {
        .name = "varnish_vcl_discard",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of discarded VCLs",
    },
    [FAM_VARNISH_VCL_FAIL] = {
        .name = "varnish_vcl_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of failures which prevented VCL from completing.",
    },
    [FAM_VARNISH_BANS] = {
        .name = "varnish_bans",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of all bans in system, including bans superseded by newer bans and "
                "bans already checked by the ban-lurker.",
    },
    [FAM_VARNISH_BANS_COMPLETED] = {
        .name = "varnish_bans_completed",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bans which are no longer active, either because they got checked by "
                "the ban-lurker or superseded by newer identical bans.",
    },
    [FAM_VARNISH_BANS_OBJ] = {
        .name = "varnish_bans_obj",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bans which use obj.* variables. "
                "These bans can possibly be washed by the ban-lurker.",
    },
    [FAM_VARNISH_BANS_REQ] = {
        .name = "varnish_bans_req",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bans which use req.* variables. "
                "These bans can not be washed by the ban-lurker.",
    },
    [FAM_VARNISH_BANS_ADDED] = {
        .name = "varnish_bans_added",
        .type = METRIC_TYPE_COUNTER,
        .help = "Counter of bans added to ban list.",
    },
    [FAM_VARNISH_BANS_DELETED] = {
        .name = "varnish_bans_deleted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Counter of bans deleted from ban list.",
    },
    [FAM_VARNISH_BANS_TESTED] = {
        .name = "varnish_bans_tested",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of how many bans and objects have been tested "
                "against each other during hash lookup.",
    },
    [FAM_VARNISH_BANS_OBJ_KILLED] = {
        .name = "varnish_bans_obj_killed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of objects killed by bans during object lookup.",
    },
    [FAM_VARNISH_BANS_LURKER_TESTED] = {
        .name = "varnish_bans_lurker_tested",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of how many bans and objects have been tested "
                "against each other by the ban-lurker.",
    },
    [FAM_VARNISH_BANS_TESTS_TESTED] = {
        .name = "varnish_bans_tests_tested",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of how many tests and objects have been tested "
                "against each other during lookup.",
    },
    [FAM_VARNISH_BANS_LURKER_TESTS_TESTED] = {
        .name = "varnish_bans_lurker_tests_tested",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of how many tests and objects have been tested "
                "against each other by the ban-lurker.",
    },
    [FAM_VARNISH_BANS_LURKER_OBJ_KILLED] = {
        .name = "varnish_bans_lurker_obj_killed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of objects killed by the ban-lurker.",
    },
    [FAM_VARNISH_BANS_LURKER_OBJ_KILLED_CUTOFF] = {
        .name = "varnish_bans_lurker_obj_killed_cutoff",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of objects killed by the ban-lurker to "
                "keep the number of bans below ban_cutoff.",
    },
    [FAM_VARNISH_BANS_DUPS] = {
        .name = "varnish_bans_dups",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of bans replaced by later identical bans.",
    },
    [FAM_VARNISH_BANS_LURKER_CONTENTION] = {
        .name = "varnish_bans_lurker_contention",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the ban-lurker had to wait for lookups.",
    },
    [FAM_VARNISH_BANS_PERSISTED_BYTES] = {
        .name = "varnish_bans_persisted_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes used by the persisted ban lists.",
    },
    [FAM_VARNISH_BANS_PERSISTED_FRAGMENTATION_BYTES] = {
        .name = "varnish_bans_persisted_fragmentation_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of extra bytes accumulated through dropped and completed bans "
                "in the persistent ban lists.",
    },
    [FAM_VARNISH_PURGES] = {
        .name = "varnish_purges",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of purge operations executed.",
    },
    [FAM_VARNISH_PURGED_OBJECTS] = {
        .name = "varnish_purged_objects",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of purged objects.",
    },
    [FAM_VARNISH_EXPIRY_MAILED] = {
        .name = "varnish_expiry_mailed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of objects mailed to expiry thread for handling.",
    },
    [FAM_VARNISH_EXPIRY_RECEIVED] = {
        .name = "varnish_expiry_received",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of objects received by expiry thread for handling.",
    },
    [FAM_VARNISH_HCB_NOLOCK] = {
        .name = "varnish_hcb_nolock",
        .type = METRIC_TYPE_COUNTER,
        .help = "HCB Lookups without lock",
    },
    [FAM_VARNISH_HCB_LOCK] = {
        .name = "varnish_hcb_lock",
        .type = METRIC_TYPE_COUNTER,
        .help = "HCB Lookups with lock.",
    },
    [FAM_VARNISH_HCB_INSERT] = {
        .name = "varnish_hcb_insert",
        .type = METRIC_TYPE_COUNTER,
        .help = "HCB Inserts.",
    },
    [FAM_VARNISH_ESI_ERRORS] = {
        .name = "varnish_esi_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "ESI parse errors (unlock).",
    },
    [FAM_VARNISH_ESI_WARNINGS] = {
        .name = "varnish_esi_warnings",
        .type = METRIC_TYPE_COUNTER,
        .help = "ESI parse warnings (unlock).",
    },
    [FAM_VARNISH_ESI_MAX_DEPTH] = {
        .name = "varnish_esi_max_depth",
        .type = METRIC_TYPE_COUNTER,
        .help = "ESI hit max_esi_depth.",
    },
    [FAM_VARNISH_VMODS] = {
        .name = "varnish_vmods",
        .type = METRIC_TYPE_GAUGE,
        .help = "Loaded VMODs.",
    },
    [FAM_VARNISH_GZIP] = {
        .name = "varnish_gzip",
        .type = METRIC_TYPE_COUNTER,
        .help = "Gzip operations.",
    },
    [FAM_VARNISH_GUNZIP] = {
        .name = "varnish_gunzip",
        .type = METRIC_TYPE_COUNTER,
        .help = "Gunzip operations.",
    },
    [FAM_VARNISH_TEST_GUNZIP] = {
        .name = "varnish_test_gunzip",
        .type = METRIC_TYPE_COUNTER,
        .help = "Test gunzip operations.",
    },
    [FAM_VARNISH_GOTO_DNS_CACHE_HITS] = {
        .name = "varnish_goto_dns_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cache hits within vmod_goto's DNS.",
    },
    [FAM_VARNISH_GOTO_DNS_LOOKUPS] = {
        .name = "varnish_goto_dns_lookups",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of DNS lookups from vmod_goto's DNS.",
    },
    [FAM_VARNISH_GOTO_DNS_LOOKUP_FAILS] = {
        .name = "varnish_goto_dns_lookup_fails",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of unresolved DNS lookups from vmod_goto's DNS.",
    },
    [FAM_VARNISH_BROTLI_COMPRESSIONS] = {
        .name = "varnish_brotli_compressions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Brotli compressions.",
    },
    [FAM_VARNISH_BROTLI_IN_BYTES] = {
        .name = "varnish_brotli_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bytes into brotli compressor.",
    },
    [FAM_VARNISH_BROTLI_OUT_BYTES] = {
        .name = "varnish_brotli_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bytes out of brotli compressor.",
    },
    [FAM_VARNISH_BROTLI_COMPRESSIONS_FAILURES] = {
        .name = "varnish_brotli_compressions_failures",
        .type = METRIC_TYPE_COUNTER,
        .help = "Brotli compressions failures",
    },
    [FAM_VARNISH_BROTLI_DECOMPRESSIONS] = {
        .name = "varnish_brotli_decompressions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Brotli decompressions.",
    },
    [FAM_VARNISH_BROTLI_DECOMPRESSOR_IN_BYTES] = {
        .name = "varnish_brotli_decompressor_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bytes into brotli decompressor.",
    },
    [FAM_VARNISH_BROTLI_DECOMPRESSOR_OUT_BYTES] = {
        .name = "varnish_brotli_decompressor_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Bytes out of brotli decompressor.",
    },
    [FAM_VARNISH_BROTLI_DECOMPRESSOR_FAILURES] = {
        .name = "varnish_brotli_decompressor_failures",
        .type = METRIC_TYPE_COUNTER,
        .help = "Brotli decompression failures.",
    },
    [FAM_VARNISH_BROTLI_TEST_DECOMPRESSIONS] = {
        .name = "varnish_brotli_test_decompressions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Brotli test decompressions.",
    },
    [FAM_VARNISH_BROTLI_TEST_DECOMPRESSIONS_FAILURES] = {
        .name = "varnish_brotli_test_decompressions_failures",
        .type = METRIC_TYPE_COUNTER,
        .help = "Brotli test decompressions failures.",
    },
    [FAM_VARNISH_MEMPOOL_LIVE] = {
        .name = "varnish_mempool_live",
        .type = METRIC_TYPE_GAUGE,
        .help = "In use.",
    },
    [FAM_VARNISH_MEMPOOL_POOL] = {
        .name = "varnish_mempool_pool",
        .type = METRIC_TYPE_GAUGE,
        .help = "In Pool.",
    },
    [FAM_VARNISH_MEMPOOL_SIZE_WANTED_BYTES] = {
        .name = "varnish_mempool_size_wanted_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size requested.",
    },
    [FAM_VARNISH_MEMPOOL_SIZE_ACTUAL_BYTES] = {
        .name = "varnish_mempool_size_actual_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size allocated.",
    },
    [FAM_VARNISH_MEMPOOL_ALLOCIONS] = {
        .name = "varnish_mempool_allocions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Allocations.",
    },
    [FAM_VARNISH_MEMPOOL_FREES] = {
        .name = "varnish_mempool_frees",
        .type = METRIC_TYPE_COUNTER,
        .help = "Frees.",
    },
    [FAM_VARNISH_MEMPOOL_RECYCLE] = {
        .name = "varnish_mempool_recycle",
        .type = METRIC_TYPE_COUNTER,
        .help = "Recycled from pool.",
    },
    [FAM_VARNISH_MEMPOOL_TIMEOUT] = {
        .name = "varnish_mempool_timeout",
        .type = METRIC_TYPE_COUNTER,
        .help = "Timed out from pool.",
    },
    [FAM_VARNISH_MEMPOOL_TOOSMALL] = {
        .name = "varnish_mempool_toosmall",
        .type = METRIC_TYPE_COUNTER,
        .help = "Too small to recycle.",
    },
    [FAM_VARNISH_MEMPOOL_SURPLUS] = {
        .name = "varnish_mempool_surplus",
        .type = METRIC_TYPE_COUNTER,
        .help = "Too many for pool.",
    },
    [FAM_VARNISH_MEMPOOL_RANDRY] = {
        .name = "varnish_mempool_randry",
        .type = METRIC_TYPE_COUNTER,
        .help = "Pool ran dry.",
    },
    [FAM_VARNISH_LOCK_CREATED] = {
        .name = "varnish_lock_created",
        .type = METRIC_TYPE_COUNTER,
        .help = "Created locks.",
    },
    [FAM_VARNISH_LOCK_DESTROY] = {
        .name = "varnish_lock_destroy",
        .type = METRIC_TYPE_COUNTER,
        .help = "Destroyed locks.",
    },
    [FAM_VARNISH_LOCK_LOCKS] = {
        .name = "varnish_lock_locks",
        .type = METRIC_TYPE_COUNTER,
        .help = "Lock Operations.",
    },
    [FAM_VARNISH_LOCK_DBG_BUSY] = {
        .name = "varnish_lock_dbg_busy",
        .type = METRIC_TYPE_COUNTER,
        .help = "Contended lock operations.",
    },
    [FAM_VARNISH_LOCK_DBG_TRY_FAIL] = {
        .name = "varnish_lock_dbg_try_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Contended trylock operations.",
    },
    [FAM_VARNISH_SMA_ALLOC_REQUEST] = {
        .name = "varnish_sma_alloc_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the storage has been asked to provide a storage segment.",
    },
    [FAM_VARNISH_SMA_ALLOC_FAIL] = {
        .name = "varnish_sma_alloc_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the storage has failed to provide a storage segment.",
    },
    [FAM_VARNISH_SMA_ALLOCATED_BYTES] = {
        .name = "varnish_sma_allocated_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of total bytes allocated by this storage.",
    },
    [FAM_VARNISH_SMA_FREED_BYTES] = {
        .name = "varnish_sma_freed_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of total bytes returned to this storage.",
    },
    [FAM_VARNISH_SMA_ALLOC_OUTSTANDING] = {
        .name = "varnish_sma_alloc_outstanding",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of storage allocations outstanding.",
    },
    [FAM_VARNISH_SMA_OUTSTANDING_BYTES] = {
        .name = "varnish_sma_outstanding_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes allocated from the storage.",
    },
    [FAM_VARNISH_SMA_AVAILABLE_BYTES] = {
        .name = "varnish_sma_available_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes left in the storage.",
    },
    [FAM_VARNISH_SMA_YKEY_KEYS] = {
        .name = "varnish_sma_ykey_keys",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of YKeys registered.",
    },
    [FAM_VARNISH_SMA_YKEY_PURGED] = {
        .name = "varnish_sma_ykey_purged",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of objects purged with YKey.",
    },
    [FAM_VARNISH_SMF_ALLOC_REQUEST] = {
        .name = "varnish_smf_alloc_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the storage has been asked to provide a storage segment.",
    },
    [FAM_VARNISH_SMF_ALLOC_FAIL] = {
        .name = "varnish_smf_alloc_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the storage has failed to provide a storage segment.",
    },
    [FAM_VARNISH_SMF_ALLOCATED_BYTES] = {
        .name = "varnish_smf_allocated_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of total bytes allocated by this storage.",
    },
    [FAM_VARNISH_SMF_FREED_BYTES] = {
        .name = "varnish_smf_freed_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of total bytes returned to this storage.",
    },
    [FAM_VARNISH_SMF_ALLOC_OUTSTANDING] = {
        .name = "varnish_smf_alloc_outstanding",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of storage allocations outstanding.",
    },
    [FAM_VARNISH_SMF_OUTSTANDING_BYTES] = {
        .name = "varnish_smf_outstanding_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes allocated from the storage.",
    },
    [FAM_VARNISH_SMF_AVAILABLE_BYTES] = {
        .name = "varnish_smf_avilable_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes left in the storage.",
    },
    [FAM_VARNISH_SMF_STRUCTS] = {
        .name = "varnish_smf_structs",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of struct smf",
    },
    [FAM_VARNISH_SMF_STRUCTS_SMALL_FREE] = {
        .name = "varnish_smf_structs_small_free",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of small free smf",
    },
    [FAM_VARNISH_SMF_STRUCTS_LARGE_FREE] = {
        .name = "varnish_smf_structs_large_free",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of large free smf",
    },
    [FAM_VARNISH_SMU_ALLOC_REQUEST] = {
        .name = "varnish_smu_alloc_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the storage has been asked to provide a storage segment.",
    },
    [FAM_VARNISH_SMU_ALLOC_FAIL] = {
        .name = "varnish_smu_alloc_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the storage has failed to provide a storage segment.",
    },
    [FAM_VARNISH_SMU_ALLOCATED_BYTES] = {
        .name = "varnish_smu_allocated_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of total bytes allocated by this storage.",
    },
    [FAM_VARNISH_SMU_FREED_BYTES] = {
        .name = "varnish_smu_freed_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of total bytes returned to this storage.",
    },
    [FAM_VARNISH_SMU_ALLOC_OUTSTANDING] = {
        .name = "varnish_smu_alloc_outstanding",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of storage allocations outstanding.",
    },
    [FAM_VARNISH_SMU_OUTSTANDING_BYTES] = {
        .name = "varnish_smu_outstanding_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes allocated from the storage.",
    },
    [FAM_VARNISH_SMU_AVAILABLE_BYTES] = {
        .name = "varnish_smu_available_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes left in the storage.",
    },
    [FAM_VARNISH_MSE_ALLOC_REQUEST] = {
        .name = "varnish_mse_alloc_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the storage has been asked to provide a storage segment.",
    },
    [FAM_VARNISH_MSE_ALLOC_FAIL] = {
        .name = "varnish_mse_alloc_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times the storage has failed to provide a storage segment.",
    },
    [FAM_VARNISH_MSE_ALLOCATED_BYTES] = {
        .name = "varnish_mse_allocated_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of total bytes allocated by this storage.",
    },
    [FAM_VARNISH_MSE_FREED_BYTES] = {
        .name = "varnish_mse_freed_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of total bytes returned to this storage.",
    },
    [FAM_VARNISH_MSE_ALLOC_OUTSTANDING] = {
        .name = "varnish_mse_alloc_outstanding",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of storage allocations outstanding.",
    },
    [FAM_VARNISH_MSE_OUTSTANDING_BYTES] = {
        .name = "varnish_mse_outstanding_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes allocated from the storage.",
    },
    [FAM_VARNISH_MSE_AVAILABLE_BYTES] = {
        .name = "varnish_mse_available_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes left in the storage.",
    },
    [FAM_VARNISH_MSE_FAIL_MALLOC] = {
        .name = "varnish_mse_fail_malloc",
        .type = METRIC_TYPE_COUNTER,
        .help = "System allocator failures.",
    },
    [FAM_VARNISH_MSE_MEMCACHE_HIT] = {
        .name = "varnish_mse_memcache_hit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Stored objects cache hits.",
    },
    [FAM_VARNISH_MSE_MEMCACHE_MISS] = {
        .name = "varnish_mse_memcache_miss",
        .type = METRIC_TYPE_COUNTER,
        .help = "Stored objects cache misses.",
    },
    [FAM_VARNISH_MSE_YKEY_KEYS] = {
        .name = "varnish_mse_ykey_keys",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of YKeys registered",
    },
    [FAM_VARNISH_MSE_YKEY_PURGED] = {
        .name = "varnish_mse_ykey_purged",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of objects purged with YKey.",
    },
    [FAM_VARNISH_MSE_LRU_MOVED] = {
        .name = "varnish_mse_lru_moved",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of LRU moved objects",
    },
    [FAM_VARNISH_MSE_LRU_NUKED] = {
        .name = "varnish_mse_lru_nuked",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of LRU nuked objects",
    },
    [FAM_VARNISH_MSE_VARY] = {
        .name = "varnish_mse_vary",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of Vary header keys.",
    },
    [FAM_VARNISH_VBE_UP] = {
        .name = "varnish_vbe_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Backend up as per the latest health probe.",
    },
    [FAM_VARNISH_VBE_REQUEST_HDR_BYTES] = {
        .name = "varnish_vbe_request_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total backend request header bytes sent.",
    },
    [FAM_VARNISH_VBE_REQUEST_BODY_BYTES] = {
        .name = "varnish_vbe_request_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total backend request body bytes sent.",
    },
    [FAM_VARNISH_VBE_RESPONSE_HDR_BYTES] = {
        .name = "varnish_vbe_response_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total backend response header bytes received.",
    },
    [FAM_VARNISH_VBE_RESPONSE_BODY_BYTES] = {
        .name = "varnish_vbe_response_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total backend response body bytes received.",
    },
    [FAM_VARNISH_VBE_PIPE_HDR_BYTES] = {
        .name = "varnish_vbe_pipe_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total request bytes sent for piped sessions.",
    },
    [FAM_VARNISH_VBE_PIPE_OUT_BYTES] = {
        .name = "varnish_vbe_pipe_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes forwarded to backend in pipe sessions.",
    },
    [FAM_VARNISH_VBE_PIPE_IN_BYTES] = {
        .name = "varnish_vbe_pipe_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes forwarded from backend in pipe sessions.",
    },
    [FAM_VARNISH_VBE_CONNECTIONS] = {
        .name = "varnish_vbe_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of currently used connections to the backend.",
    },
    [FAM_VARNISH_VBE_REQUESTS] = {
        .name = "varnish_vbe_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Backend requests sent.",
    },
    [FAM_VARNISH_VBE_UNHEALTHY] = {
        .name = "varnish_vbe_unhealthy",
        .type = METRIC_TYPE_COUNTER,
        .help = "Fetches not attempted due to backend being unhealthy.",
    },
    [FAM_VARNISH_VBE_BUSY] = {
        .name = "varnish_vbe_busy",
        .type = METRIC_TYPE_COUNTER,
        .help = "Fetches not attempted due to backend being busy. "
                "Number of times the max_connections limit was reached.",
    },
    [FAM_VARNISH_VBE_FAIL] = {
        .name = "varnish_vbe_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Counter of failed opens.",
    },
    [FAM_VARNISH_VBE_HELDDOWN] = {
        .name = "varnish_vbe_helddown",
        .type = METRIC_TYPE_COUNTER,
        .help = "Connections not attempted during the backend_local_error_holddown or "
                "backend_remote_error_holddown interval after a fundamental connection issue.",
    },
    [FAM_VARNISH_ACCG_DIAG_SET_KEY_FAILURE] = {
        .name = "varnish_accg_diag_set_key_failure",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total failed attempts to set key for any reason.",
    },
    [FAM_VARNISH_ACCG_DIAG_OUT_OF_KEY_SLOTS] = {
        .name = "varnish_accg_diag_out_of_key_slots",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total failed attempts to set key due to no key slots.",
    },
    [FAM_VARNISH_ACCG_DIAG_KEY_WITHOUT_NAMESPACE] = {
        .name = "varnish_accg_diag_key_without_namespace",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total failed attempts to set key due to no namespace.",
    },
    [FAM_VARNISH_ACCG_DIAG_NAMESPACE_ALREADY_SET] = {
        .name = "varnish_accg_diag_namespace_already_set",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total attempts to escape namespace.",
    },
    [FAM_VARNISH_ACCG_DIAG_NAMESPACE_UNDEFINED] = {
        .name = "varnish_accg_diag_namespace_undefined",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total failed to set namespace due to not created.",
    },
    [FAM_VARNISH_ACCG_DIAG_CREATE_NAMESPACE_FAILURE] = {
        .name = "varnish_accg_diag_create_namespace_failure",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total failed attempts to create namespace for any reason.",
    },
    [FAM_VARNISH_ACCG_CLIENT_REQUEST] = {
        .name = "varnish_accg_client_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of client requests processed.",
    },
    [FAM_VARNISH_ACCG_CLIENT_REQUEST_HDR_BYTES] = {
        .name = "varnish_accg_client_request_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client request headers size in bytes.",
    },
    [FAM_VARNISH_ACCG_CLIENT_REQUEST_BODY_BYTES] = {
        .name = "varnish_accg_client_request_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client request body size in bytes.",
    },
    [FAM_VARNISH_ACCG_CLIENT_RESPONSE_HDR_BYTES] = {
        .name = "varnish_accg_client_response_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client response headers size in bytes.",
    },
    [FAM_VARNISH_ACCG_CLIENT_RESPONSE_BODY_BYTES] = {
        .name = "varnish_accg_client_response_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client response body size in bytes.",
    },
    [FAM_VARNISH_ACCG_CLIENT_HIT] = {
        .name = "varnish_accg_client_hit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of client hits.",
    },
    [FAM_VARNISH_ACCG_CLIENT_HIT_REQUEST_HDR_BYTES] = {
        .name = "varnish_accg_client_hit_request_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client request header bytes for hits.",
    },
    [FAM_VARNISH_ACCG_CLIENT_HIT_REQUEST_BODY_BYTES] = {
        .name = "varnish_accg_client_hit_request_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client request body bytes for hits.",
    },
    [FAM_VARNISH_ACCG_CLIENT_HIT_RESPONSE_HDR_BYTES] = {
        .name = "varnish_accg_client_hit_response_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client response header bytes for hits.",
    },
    [FAM_VARNISH_ACCG_CLIENT_HIT_RESPONSE_BODY_BYTES] = {
        .name = "varnish_accg_client_hit_response_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client response body bytes for hits.",
    },
    [FAM_VARNISH_ACCG_CLIENT_MISS] = {
        .name = "varnish_accg_client_miss",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of client request misses.",
    },
    [FAM_VARNISH_ACCG_CLIENT_MISS_REQUEST_HDR_BYTES] = {
        .name = "varnish_accg_client_miss_request_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client request header bytes for misses.",
    },
    [FAM_VARNISH_ACCG_CLIENT_MISS_REQUEST_BODY_BYTES] = {
        .name = "varnish_accg_client_miss_request_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client request body bytes for misses.",
    },
    [FAM_VARNISH_ACCG_CLIENT_MISS_RESPONSE_HDR_BYTES] = {
        .name = "varnish_accg_client_miss_response_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client response header bytes for misses.",
    },
    [FAM_VARNISH_ACCG_CLIENT_MISS_RESPONSE_BODY_BYTES] = {
        .name = "varnish_accg_client_miss_response_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client response body bytes for misses.",
    },
    [FAM_VARNISH_ACCG_CLIENT_PASS] = {
        .name = "varnish_accg_client_pass",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of client request passes.",
    },
    [FAM_VARNISH_ACCG_CLIENT_PASS_REQUEST_HDR_BYTES] = {
        .name = "varnish_accg_client_pass_request_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client request header bytes for passes.",
    },
    [FAM_VARNISH_ACCG_CLIENT_PASS_REQUEST_BODY_BYTES] = {
        .name = "varnish_accg_client_pass_request_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client request body bytes for passes.",
    },
    [FAM_VARNISH_ACCG_CLIENT_PASS_RESPONSE_HDR_BYTES] = {
        .name = "varnish_accg_client_pass_response_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client response header bytes for passes.",
    },
    [FAM_VARNISH_ACCG_CLIENT_PASS_RESPONSE_BODY_BYTES] = {
        .name = "varnish_accg_client_pass_response_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client response body bytes for passes.",
    },
    [FAM_VARNISH_ACCG_CLIENT_SYNTH] = {
        .name = "varnish_accg_client_synth",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of client request synths.",
    },
    [FAM_VARNISH_ACCG_CLIENT_SYNTH_REQUEST_HDR_BYTES] = {
        .name = "varnish_accg_client_synth_request_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client request header bytes for synths.",
    },
    [FAM_VARNISH_ACCG_CLIENT_SYNTH_REQUEST_BODY_BYTES] = {
        .name = "varnish_accg_client_synth_request_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client request body bytes for synths.",
    },
    [FAM_VARNISH_ACCG_CLIENT_SYNTH_RESPONSE_HDR_BYTES] = {
        .name = "varnish_accg_client_synth_response_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client response header bytes for synths.",
    },
    [FAM_VARNISH_ACCG_CLIENT_SYNTH_RESPONSE_BODY_BYTES] = {
        .name = "varnish_accg_client_synth_response_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client response body bytes for synths.",
    },
    [FAM_VARNISH_ACCG_CLIENT_PIPE] = {
        .name = "varnish_accg_client_pipe",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of client request pipes.",
    },
    [FAM_VARNISH_ACCG_CLIENT_PIPE_REQUEST_HDR_BYTES] = {
        .name = "varnish_accg_client_pipe_request_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client request header bytes for pipes.",
    },
    [FAM_VARNISH_ACCG_CLIENT_PIPE_REQUEST_BODY_BYTES] = {
        .name = "varnish_accg_client_pipe_request_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client request body bytes for pipes.",
    },
    [FAM_VARNISH_ACCG_CLIENT_PIPE_RESPONSE_HDR_BYTES] = {
        .name = "varnish_accg_client_pipe_response_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client response header bytes for pipes.",
    },
    [FAM_VARNISH_ACCG_CLIENT_PIPE_RESPONSE_BODY_BYTES] = {
        .name = "varnish_accg_client_pipe_response_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client response body bytes for pipes.",
    },
    [FAM_VARNISH_ACCG_CLIENT_REPONSE] = {
        .name = "varnish_accg_client_reponse",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total client response by http response code.",
    },
    [FAM_VARNISH_ACCG_BACKEND_REQUEST] = {
        .name = "varnish_accg_backend_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of backend requests processed.",
    },
    [FAM_VARNISH_ACCG_BACKEND_REQUEST_HDR_BYTES] = {
        .name = "varnish_accg_backend_request_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total backend request header bytes total.",
    },
    [FAM_VARNISH_ACCG_BACKEND_REQUEST_BODY_BYTES] = {
        .name = "varnish_accg_backend_request_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total backend request body bytes total.",
    },
    [FAM_VARNISH_ACCG_BACKEND_RESPONSE_HDR_BYTES] = {
        .name = "varnish_accg_backend_response_hdr_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total backend response header bytes total.",
    },
    [FAM_VARNISH_ACCG_BACKEND_RESPONSE_BODY_BYTES] = {
        .name = "varnish_accg_backend_response_body_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total backend response body bytes total.",
    },
    [FAM_VARNISH_ACCG_BACKEND_RESPONSE] = {
        .name = "varnish_accg_backend_response",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of backend responses by http response code.",
    },
};
