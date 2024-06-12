/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

enum {
    FAM_HAPROXY_PROCESS_NBTHREAD,
    FAM_HAPROXY_PROCESS_NBPROC,
    FAM_HAPROXY_PROCESS_RELATIVE_PROCESS_ID,
    FAM_HAPROXY_PROCESS_UPTIME_SECONDS,
    FAM_HAPROXY_PROCESS_START_TIME_SECONDS,
    FAM_HAPROXY_PROCESS_MAX_MEMORY_BYTES,
    FAM_HAPROXY_PROCESS_POOL_ALLOCATED_BYTES,
    FAM_HAPROXY_PROCESS_POOL_USED_BYTES,
    FAM_HAPROXY_PROCESS_POOL_FAILURES,
    FAM_HAPROXY_PROCESS_MAX_FDS,
    FAM_HAPROXY_PROCESS_MAX_SOCKETS,
    FAM_HAPROXY_PROCESS_MAX_CONNECTIONS,
    FAM_HAPROXY_PROCESS_HARD_MAX_CONNECTIONS,
    FAM_HAPROXY_PROCESS_CURRENT_CONNECTIONS,
    FAM_HAPROXY_PROCESS_CONNECTIONS,
    FAM_HAPROXY_PROCESS_REQUESTS,
    FAM_HAPROXY_PROCESS_MAX_SSL_CONNECTIONS,
    FAM_HAPROXY_PROCESS_CURRENT_SSL_CONNECTIONS,
    FAM_HAPROXY_PROCESS_SSL_CONNECTIONS,
    FAM_HAPROXY_PROCESS_MAX_PIPES,
    FAM_HAPROXY_PROCESS_PIPES_USED,
    FAM_HAPROXY_PROCESS_PIPES_FREE,
    FAM_HAPROXY_PROCESS_CURRENT_CONNECTION_RATE,
    FAM_HAPROXY_PROCESS_LIMIT_CONNECTION_RATE,
    FAM_HAPROXY_PROCESS_MAX_CONNECTION_RATE,
    FAM_HAPROXY_PROCESS_CURRENT_SESSION_RATE,
    FAM_HAPROXY_PROCESS_LIMIT_SESSION_RATE,
    FAM_HAPROXY_PROCESS_MAX_SESSION_RATE,
    FAM_HAPROXY_PROCESS_CURRENT_SSL_RATE,
    FAM_HAPROXY_PROCESS_LIMIT_SSL_RATE,
    FAM_HAPROXY_PROCESS_MAX_SSL_RATE,
    FAM_HAPROXY_PROCESS_CURRENT_FRONTEND_SSL_KEY_RATE,
    FAM_HAPROXY_PROCESS_MAX_FRONTEND_SSL_KEY_RATE,
    FAM_HAPROXY_PROCESS_FRONTEND_SSL_REUSE,
    FAM_HAPROXY_PROCESS_CURRENT_BACKEND_SSL_KEY_RATE,
    FAM_HAPROXY_PROCESS_MAX_BACKEND_SSL_KEY_RATE,
    FAM_HAPROXY_PROCESS_SSL_CACHE_LOOKUPS,
    FAM_HAPROXY_PROCESS_SSL_CACHE_MISSES,
    FAM_HAPROXY_PROCESS_HTTP_COMP_BYTES_IN,
    FAM_HAPROXY_PROCESS_HTTP_COMP_BYTES_OUT,
    FAM_HAPROXY_PROCESS_LIMIT_HTTP_COMP,
    FAM_HAPROXY_PROCESS_CURRENT_ZLIB_MEMORY,
    FAM_HAPROXY_PROCESS_MAX_ZLIB_MEMORY,
    FAM_HAPROXY_PROCESS_CURRENT_TASKS,
    FAM_HAPROXY_PROCESS_CURRENT_RUN_QUEUE,
    FAM_HAPROXY_PROCESS_IDLE_TIME_PERCENT,
    FAM_HAPROXY_PROCESS_STOPPING,
    FAM_HAPROXY_PROCESS_JOBS,
    FAM_HAPROXY_PROCESS_UNSTOPPABLE_JOBS,
    FAM_HAPROXY_PROCESS_LISTENERS,
    FAM_HAPROXY_PROCESS_ACTIVE_PEERS,
    FAM_HAPROXY_PROCESS_CONNECTED_PEERS,
    FAM_HAPROXY_PROCESS_DROPPED_LOGS,
    FAM_HAPROXY_PROCESS_BUSY_POLLING_ENABLED,
    FAM_HAPROXY_PROCESS_FAILED_RESOLUTIONS,
    FAM_HAPROXY_PROCESS_BYTES_OUT,
    FAM_HAPROXY_PROCESS_SPLICED_BYTES_OUT,
    FAM_HAPROXY_PROCESS_BYTES_OUT_RATE,
    FAM_HAPROXY_PROCESS_RECV_LOGS,
    FAM_HAPROXY_PROCESS_BUILD_INFO,
    FAM_HAPROXY_PROCESS_TAINTED,
    FAM_HAPROXY_PROCESS_WARNINGS,
    FAM_HAPROXY_PROCESS_MAXCONN_REACHED,
    FAM_HAPROXY_PROCESS_BOOTTIME_SECONDS,
    FAM_HAPROXY_PROCESS_NICED_TASKS,
    FAM_HAPROXY_PROCESS_MAX,
};

static metric_family_t fams_haproxy_process[FAM_HAPROXY_PROCESS_MAX] = {
    [FAM_HAPROXY_PROCESS_NBTHREAD] = {
        .name = "haproxy_process_nbthread",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of started threads (global.nbthread).",
    },
    [FAM_HAPROXY_PROCESS_NBPROC] = {
        .name = "haproxy_process_nbproc",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of started worker processes (global.nbproc).",
    },
    [FAM_HAPROXY_PROCESS_RELATIVE_PROCESS_ID] = {
        .name = "haproxy_process_relative_process_id",
        .type = METRIC_TYPE_GAUGE,
        .help = "Relative worker process number (1..Nbproc).",
    },
    [FAM_HAPROXY_PROCESS_UPTIME_SECONDS] = {
        .name = "haproxy_process_uptime_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "How long ago this worker process was started (seconds).",
    },
    [FAM_HAPROXY_PROCESS_START_TIME_SECONDS] = {
        .name = "haproxy_process_start_time_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Start time in seconds.",
    },
    [FAM_HAPROXY_PROCESS_MAX_MEMORY_BYTES] = {
        .name = "haproxy_process_max_memory_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Worker process's hard limit on memory usage in byes (-m on command line).",
    },
    [FAM_HAPROXY_PROCESS_POOL_ALLOCATED_BYTES] = {
        .name = "haproxy_process_pool_allocated_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory allocated in pools (in bytes).",
    },
    [FAM_HAPROXY_PROCESS_POOL_USED_BYTES] = {
        .name = "haproxy_process_pool_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of pool memory currently used (in bytes).",
    },
    [FAM_HAPROXY_PROCESS_POOL_FAILURES] = {
        .name = "haproxy_process_pool_failures",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of failed pool allocations since this worker was started.",
    },
    [FAM_HAPROXY_PROCESS_MAX_FDS] = {
        .name = "haproxy_process_max_fds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Hard limit on the number of per-process file descriptors.",
    },
    [FAM_HAPROXY_PROCESS_MAX_SOCKETS] = {
        .name = "haproxy_process_max_sockets",
        .type = METRIC_TYPE_GAUGE,
        .help = "Hard limit on the number of per-process sockets.",
    },
    [FAM_HAPROXY_PROCESS_MAX_CONNECTIONS] = {
        .name = "haproxy_process_max_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Hard limit on the number of per-process connections "
                "(configured or imposed by Ulimit-n).",
    },
    [FAM_HAPROXY_PROCESS_HARD_MAX_CONNECTIONS] = {
        .name = "haproxy_process_hard_max_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Hard limit on the number of per-process connections "
                "(imposed by Memmax_MB or Ulimit-n).",
    },
    [FAM_HAPROXY_PROCESS_CURRENT_CONNECTIONS] = {
        .name = "haproxy_process_current_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of connections on this worker process.",
    },
    [FAM_HAPROXY_PROCESS_CONNECTIONS] = {
        .name = "haproxy_process_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of connections on this worker process since started.",
    },
    [FAM_HAPROXY_PROCESS_REQUESTS] = {
        .name = "haproxy_process_requests",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of requests on this worker process since started.",
    },
    [FAM_HAPROXY_PROCESS_MAX_SSL_CONNECTIONS] = {
        .name = "haproxy_process_max_ssl_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Hard limit on the number of per-process SSL endpoints (front+back).",
    },
    [FAM_HAPROXY_PROCESS_CURRENT_SSL_CONNECTIONS] = {
        .name = "haproxy_process_current_ssl_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of SSL endpoints on this worker process (front+back).",
    },
    [FAM_HAPROXY_PROCESS_SSL_CONNECTIONS] = {
        .name = "haproxy_process_ssl_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of SSL endpoints on this worker process since started (front+back).",
    },
    [FAM_HAPROXY_PROCESS_MAX_PIPES] = {
        .name = "haproxy_process_max_pipes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Hard limit on the number of pipes for splicing.",
    },
    [FAM_HAPROXY_PROCESS_PIPES_USED] = {
        .name = "haproxy_process_pipes_used",
        .type = METRIC_TYPE_COUNTER,
        .help = "Current number of pipes in use in this worker process.",
    },
    [FAM_HAPROXY_PROCESS_PIPES_FREE] = {
        .name = "haproxy_process_pipes_free",
        .type = METRIC_TYPE_COUNTER,
        .help = "Current number of allocated and available pipes in this worker process.",
    },
    [FAM_HAPROXY_PROCESS_CURRENT_CONNECTION_RATE] = {
        .name = "haproxy_process_current_connection_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of front connections created on this worker process over the last second.",
    },
    [FAM_HAPROXY_PROCESS_LIMIT_CONNECTION_RATE] = {
        .name = "haproxy_process_limit_connection_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Hard limit for ConnRate (global.maxconnrate).",
    },
    [FAM_HAPROXY_PROCESS_MAX_CONNECTION_RATE] = {
        .name = "haproxy_process_max_connection_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Highest ConnRate reached on this worker process since started "
                "(in connections per second).",
    },
    [FAM_HAPROXY_PROCESS_CURRENT_SESSION_RATE] = {
        .name = "haproxy_process_current_session_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of sessions created on this worker process over the last second.",
    },
    [FAM_HAPROXY_PROCESS_LIMIT_SESSION_RATE] = {
        .name = "haproxy_process_limit_session_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Hard limit for SessRate (global.maxsessrate).",
    },
    [FAM_HAPROXY_PROCESS_MAX_SESSION_RATE] = {
        .name = "haproxy_process_max_session_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Highest SessRate reached on this worker process since started "
                "(in sessions per second).",
    },
    [FAM_HAPROXY_PROCESS_CURRENT_SSL_RATE] = {
        .name = "haproxy_process_current_ssl_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of SSL connections created on this worker process over the last second.",
    },
    [FAM_HAPROXY_PROCESS_LIMIT_SSL_RATE] = {
        .name = "haproxy_process_limit_ssl_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Hard limit for SslRate (global.maxsslrate).",
    },
    [FAM_HAPROXY_PROCESS_MAX_SSL_RATE] = {
        .name = "haproxy_process_max_ssl_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Highest SslRate reached on this worker process since started "
                "(in connections per second).",
    },
    [FAM_HAPROXY_PROCESS_CURRENT_FRONTEND_SSL_KEY_RATE] = {
        .name = "haproxy_process_current_frontend_ssl_key_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of SSL keys created on frontends in this worker process "
                "over the last second.",
    },
    [FAM_HAPROXY_PROCESS_MAX_FRONTEND_SSL_KEY_RATE] = {
        .name = "haproxy_process_max_frontend_ssl_key_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Highest SslFrontendKeyRate reached on this worker process since started "
                "(in SSL keys per second).",
    },
    [FAM_HAPROXY_PROCESS_FRONTEND_SSL_REUSE] = {
        .name = "haproxy_process_frontend_ssl_reuse",
        .type = METRIC_TYPE_GAUGE,
        .help = "Percent of frontend SSL connections which did not require a new key.",
    },
    [FAM_HAPROXY_PROCESS_CURRENT_BACKEND_SSL_KEY_RATE] = {
        .name = "haproxy_process_current_backend_ssl_key_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of SSL keys created on backends in this worker process "
                "over the last second.",
    },
    [FAM_HAPROXY_PROCESS_MAX_BACKEND_SSL_KEY_RATE] = {
        .name = "haproxy_process_max_backend_ssl_key_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Highest SslBackendKeyRate reached on this worker process since started "
                "(in SSL keys per second).",
    },
    [FAM_HAPROXY_PROCESS_SSL_CACHE_LOOKUPS] = {
        .name = "haproxy_process_ssl_cache_lookups",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of SSL session ID lookups in the SSL session cache "
                "on this worker since started.",
    },
    [FAM_HAPROXY_PROCESS_SSL_CACHE_MISSES] = {
        .name = "haproxy_process_ssl_cache_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of SSL session ID lookups that didn't find a session "
                "in the SSL session cache on this worker since started.",
    },
    [FAM_HAPROXY_PROCESS_HTTP_COMP_BYTES_IN] = {
        .name = "haproxy_process_http_comp_bytes_in",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes submitted to the HTTP compressor in this "
                "worker process over the last second.",
    },
    [FAM_HAPROXY_PROCESS_HTTP_COMP_BYTES_OUT] = {
        .name = "haproxy_process_http_comp_bytes_out",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes emitted by the HTTP compressor in this worker "
                "process over the last second.",
    },
    [FAM_HAPROXY_PROCESS_LIMIT_HTTP_COMP] = {
        .name = "haproxy_process_limit_http_comp",
        .type = METRIC_TYPE_GAUGE,
        .help = "Limit of CompressBpsOut beyond which HTTP compression is automatically disabled.",
    },
    [FAM_HAPROXY_PROCESS_CURRENT_ZLIB_MEMORY] = {
        .name = "haproxy_process_current_zlib_memory",
        .type = METRIC_TYPE_GAUGE,
        .help = "Amount of memory currently used by HTTP compression on the current "
                "worker process (in bytes).",
    },
    [FAM_HAPROXY_PROCESS_MAX_ZLIB_MEMORY] = {
        .name = "haproxy_process_max_zlib_memory",
        .type = METRIC_TYPE_GAUGE,
        .help = "Limit on the amount of memory used by HTTP compression above which "
                "it is automatically disabled (in bytes).",
    },
    [FAM_HAPROXY_PROCESS_CURRENT_TASKS] = {
        .name = "haproxy_process_current_tasks",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of tasks in the current worker process (active + sleeping).",
    },
    [FAM_HAPROXY_PROCESS_CURRENT_RUN_QUEUE] = {
        .name = "haproxy_process_current_run_queue",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of active tasks+tasklets in the current worker process.",
    },
    [FAM_HAPROXY_PROCESS_IDLE_TIME_PERCENT] = {
        .name = "haproxy_process_idle_time_percent",
        .type = METRIC_TYPE_GAUGE,
        .help = "Percentage of last second spent waiting in the current worker thread.",
    },
    [FAM_HAPROXY_PROCESS_STOPPING] = {
        .name = "haproxy_process_stopping",
        .type = METRIC_TYPE_GAUGE,
        .help = "1 if the worker process is currently stopping.",
    },
    [FAM_HAPROXY_PROCESS_JOBS] = {
        .name = "haproxy_process_jobs",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of active jobs on the current worker process "
                "(frontend connections).",
    },
    [FAM_HAPROXY_PROCESS_UNSTOPPABLE_JOBS] = {
        .name = "haproxy_process_unstoppable_jobs",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of unstoppable jobs on the current worker process "
                "(master connections).",
    },
    [FAM_HAPROXY_PROCESS_LISTENERS] = {
        .name = "haproxy_process_listeners",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of active listeners on the current worker process",
    },
    [FAM_HAPROXY_PROCESS_ACTIVE_PEERS] = {
        .name = "haproxy_process_active_peers",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of verified active peers connections on the current "
                "worker process.",
    },
    [FAM_HAPROXY_PROCESS_CONNECTED_PEERS] = {
        .name = "haproxy_process_connected_peers",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of peers having passed the connection step on the "
                "current worker process.",
    },
    [FAM_HAPROXY_PROCESS_DROPPED_LOGS] = {
        .name = "haproxy_process_dropped_logs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of dropped logs for current worker process since started.",
    },
    [FAM_HAPROXY_PROCESS_BUSY_POLLING_ENABLED] = {
        .name = "haproxy_process_busy_polling_enabled",
        .type = METRIC_TYPE_GAUGE,
        .help = "1 if busy-polling is currently in use on the worker process.",
    },
    [FAM_HAPROXY_PROCESS_FAILED_RESOLUTIONS] = {
        .name = "haproxy_process_failed_resolutions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of failed DNS resolutions in current worker process since started.",
    },
    [FAM_HAPROXY_PROCESS_BYTES_OUT] = {
        .name = "haproxy_process_bytes_out",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes emitted by current worker process since started.",
    },
    [FAM_HAPROXY_PROCESS_SPLICED_BYTES_OUT] = {
        .name = "haproxy_process_spliced_bytes_out",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes emitted by current worker process through "
                "a kernel pipe since started.",
    },
    [FAM_HAPROXY_PROCESS_BYTES_OUT_RATE] = {
        .name = "haproxy_process_bytes_out_rate",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes emitted by current worker process over the last second.",
    },
    [FAM_HAPROXY_PROCESS_RECV_LOGS] = {
        .name = "haproxy_process_recv_logs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of log messages received by log-forwarding listeners "
                "on this worker process since started.",
    },
    [FAM_HAPROXY_PROCESS_BUILD_INFO] = {
        .name = "haproxy_process_build_info",
        .type = METRIC_TYPE_INFO,
        .help = "Build info.",
    },
    [FAM_HAPROXY_PROCESS_TAINTED] = {
        .name = "haproxy_process_tainted",
        .type = METRIC_TYPE_GAUGE,
        .help = "Experimental features used.",
    },
    [FAM_HAPROXY_PROCESS_WARNINGS] = {
        .name = "haproxy_process_warnings",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total warnings issued.",
    },
    [FAM_HAPROXY_PROCESS_MAXCONN_REACHED] = {
        .name = "haproxy_process_maxconn_reached",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times an accepted connection resulted in Maxconn being reached.",
    },
    [FAM_HAPROXY_PROCESS_BOOTTIME_SECONDS] = {
        .name = "haproxy_process_boottime_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "How long ago it took to parse and process the config before being "
                "ready in seconds.",
    },
    [FAM_HAPROXY_PROCESS_NICED_TASKS] = {
        .name = "haproxy_process_niced_tasks",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of active tasks+tasklets in the current worker process "
                "(Run_queue) that are niced.",
    },
};
