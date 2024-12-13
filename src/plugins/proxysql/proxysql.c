// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2006-2010 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008 Mirko Buffoni
// SPDX-FileCopyrightText: Copyright (C) 2009 Doug MacEachern
// SPDX-FileCopyrightText: Copyright (C) 2009 Sebastian tokkee Harl
// SPDX-FileCopyrightText: Copyright (C) 2009 Rodolphe Quiédeville
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Mirko Buffoni <briareos at eswat.org>
// SPDX-FileContributor: Doug MacEachern <dougm at hyperic.com>
// SPDX-FileContributor: Sebastian tokkee Harl <sh at tokkee.org>
// SPDX-FileContributor: Rodolphe Quiédeville <rquiedeville at bearstech.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/complain.h"

#ifdef HAVE_MYSQL_H
#include <mysql.h>
#elif defined(HAVE_MYSQL_MYSQL_H)
#include <mysql/mysql.h>
#endif

enum {
    FAM_PROXYSQL_UP,
    FAM_PROXYSQL_UPTIME_SECONDS,
    FAM_PROXYSQL_ACTIVE_TRANSACTIONS,
    FAM_PROXYSQL_CLIENT_CONNECTIONS_ABORTED,
    FAM_PROXYSQL_CLIENT_CONNECTIONS_CONNECTED,
    FAM_PROXYSQL_CLIENT_CONNECTIONS_CREATED,
    FAM_PROXYSQL_CLIENT_CONNECTIONS_NON_IDLE,
    FAM_PROXYSQL_CLIENT_CONNECTIONS_HOSTGROUP_LOCKED,
    FAM_PROXYSQL_SERVER_CONNECTIONS_ABORTED,
    FAM_PROXYSQL_SERVER_CONNECTIONS_CONNECTED,
    FAM_PROXYSQL_SERVER_CONNECTIONS_CREATED,
    FAM_PROXYSQL_SERVER_CONNECTIONS_DELAYED,
    FAM_PROXYSQL_BACKEND_QUERY_TIME_SECONDS,
    FAM_PROXYSQL_QUERIES_BACKENDS_RECV_BYTES,
    FAM_PROXYSQL_QUERIES_BACKENDS_SENT_BYTES,
    FAM_PROXYSQL_QUERIES_FRONTENDS_RECV_BYTES,
    FAM_PROXYSQL_QUERIES_FRONTENDS_SENT_BYTES,
    FAM_PROXYSQL_BACKEND_LAGGING_DURING_QUERY,
    FAM_PROXYSQL_BACKEND_OFFLINE_DURING_QUERY,
    FAM_PROXYSQL_MYSQL_BACKEND_BUFFERS_BYTES,
    FAM_PROXYSQL_MYSQL_FRONTEND_BUFFERS_BYTES,
    FAM_PROXYSQL_MYSQL_KILLED_BACKEND_CONNECTIONS,
    FAM_PROXYSQL_MYSQL_KILLED_BACKEND_QUERIES,
    FAM_PROXYSQL_MYSQL_UNEXPECTED_FRONTEND_COM_QUIT,
    FAM_PROXYSQL_MYSQL_UNEXPECTED_FRONTEND_PACKETS,
    FAM_PROXYSQL_CLIENT_HOST_ERROR_KILLED_CONNECTIONS,
    FAM_PROXYSQL_HOSTGROUP_LOCKED_QUERIES,
    FAM_PROXYSQL_HOSTGROUP_LOCKED_SET_CMDS,
    FAM_PROXYSQL_MAX_CONNECT_TIMEOUTS,
    FAM_PROXYSQL_AUTOMATIC_DETECTED_SQL_INJECTION,
    FAM_PROXYSQL_WHITELISTED_SQLI_FINGERPRINT,
    FAM_PROXYSQL_GENERATED_ERROR_PACKETS,
    FAM_PROXYSQL_MYSQL_SESSION_INTERNAL_BYTES,
    FAM_PROXYSQL_COM_BACKEND_STMT,
    FAM_PROXYSQL_COM_FRONTEND_STMT,
    FAM_PROXYSQL_MYSQL_THREAD_WORKERS,
    FAM_PROXYSQL_MYSQL_MONITOR_WORKERS,
    FAM_PROXYSQL_MYSQL_MONITOR_WORKERS_AUX,
    FAM_PROXYSQL_MYSQL_MONITOR_WORKERS_STARTED,
    FAM_PROXYSQL_MYSQL_MONITOR_CONNECT_CHECK_ERR,
    FAM_PROXYSQL_MYSQL_MONITOR_CONNECT_CHECK_OK,
    FAM_PROXYSQL_MYSQL_MONITOR_PING_CHECK_ERR,
    FAM_PROXYSQL_MYSQL_MONITOR_PING_CHECK_OK,
    FAM_PROXYSQL_MYSQL_MONITOR_READ_ONLY_CHECK_ERR,
    FAM_PROXYSQL_MYSQL_MONITOR_READ_ONLY_CHECK_OK,
    FAM_PROXYSQL_MYSQL_MONITOR_REPLICATION_LAG_CHECK_ERR,
    FAM_PROXYSQL_MYSQL_MONITOR_REPLICATION_LAG_CHECK_OK,
    FAM_PROXYSQL_CONNPOOL_GET_CONN_SUCCESS,
    FAM_PROXYSQL_CONNPOOL_GET_CONN_FAILURE,
    FAM_PROXYSQL_CONNPOOL_GET_CONN_IMMEDIATE,
    FAM_PROXYSQL_CONNPOOL_GET_CONN_LATENCY_AWARENESS,
    FAM_PROXYSQL_QUESTIONS,
    FAM_PROXYSQL_SLOW_QUERIES,
    FAM_PROXYSQL_GTID_CONSISTENT_QUERIES,
    FAM_PROXYSQL_GTID_SESSION_COLLECTED,
    FAM_PROXYSQL_MIRROR_CONCURRENCY,
    FAM_PROXYSQL_MIRROR_QUEUE_LENGTH,
    FAM_PROXYSQL_QUERIES_WITH_MAX_LAG,
    FAM_PROXYSQL_QUERIES_WITH_MAX_LAG_DELAYED,
    FAM_PROXYSQL_QUERIES_WITH_MAX_LAG_WAIT_TIME_SECONDS,
    FAM_PROXYSQL_AWS_AURORA_REPLICAS_SKIPPED_DURING_QUERY,
    FAM_PROXYSQL_ACCESS_DENIED_MAX_CONNECTIONS,
    FAM_PROXYSQL_ACCESS_DENIED_MAX_USER_CONNECTIONS,
    FAM_PROXYSQL_ACCESS_DENIED_WRONG_PASSWORD,
    FAM_PROXYSQL_MYSQL_HOSTGROUPS_MANAGER_CONNECTION_POLL_GET,
    FAM_PROXYSQL_MYSQL_HOSTGROUPS_MANAGER_CONNECTION_POLL_GET_OK,
    FAM_PROXYSQL_MYSQL_HOSTGROUPS_MANAGER_CONNECTION_POLL_PUSH,
    FAM_PROXYSQL_MYSQL_HOSTGROUPS_MANAGER_CONNECTION_POLL_DESTROY,
    FAM_PROXYSQL_MYSQL_HOSTGROUPS_MANAGER_CONNECTION_POLL_RESET,
    FAM_PROXYSQL_CONNECTION_POOL_MEMORY_BYTES,
    FAM_PROXYSQL_STMT_CLIENT_ACTIVE,
    FAM_PROXYSQL_STMT_CLIENT_ACTIVE_UNIQUE,
    FAM_PROXYSQL_STMT_SERVER_ACTIVE,
    FAM_PROXYSQL_STMT_SERVER_ACTIVE_UNIQUE,
    FAM_PROXYSQL_STMT_CACHED,
    FAM_PROXYSQL_STMT_MAX_STMT_ID,
    FAM_PROXYSQL_QUERY_CACHE_MEMORY_BYTES,
    FAM_PROXYSQL_QUERY_CACHE_ENTRIES,
    FAM_PROXYSQL_QUERY_CACHE_PURGED,
    FAM_PROXYSQL_QUERY_CACHE_IN_BYTES,
    FAM_PROXYSQL_QUERY_CACHE_OUT_BYTES,
    FAM_PROXYSQL_QUERY_CACHE_COUNT_GET,
    FAM_PROXYSQL_QUERY_CACHE_COUNT_GET_OK,
    FAM_PROXYSQL_QUERY_CACHE_COUNT_SET,
    FAM_PROXYSQL_QUERY_PROCESSOR_TIME_SECONDS,
    FAM_PROXYSQL_JEMALLOC_ALLOCATED_BYTES,
    FAM_PROXYSQL_JEMALLOC_ACTIVE_BYTES,
    FAM_PROXYSQL_JEMALLOC_MAPPED_BYTES,
    FAM_PROXYSQL_JEMALLOC_METADATA_BYTES,
    FAM_PROXYSQL_JEMALLOC_RESIDENT_BYTES,
    FAM_PROXYSQL_MEMORY_AUTH_BYTES,
    FAM_PROXYSQL_SQLITE_MEMORY_BYTES,
    FAM_PROXYSQL_QUERY_DIGEST_MEMORY_BYTES,
    FAM_PROXYSQL_MYSQL_QUERY_RULES_MEMORY_BYTES,
    FAM_PROXYSQL_MYSQL_FIREWALL_USERS_TABLE_BYTES,
    FAM_PROXYSQL_MYSQL_FIREWALL_USERS_CONFIG_BYTES,
    FAM_PROXYSQL_MYSQL_FIREWALL_RULES_TABLE_BYTES,
    FAM_PROXYSQL_MYSQL_FIREWALL_RULES_CONFIG_BYTES,
    FAM_PROXYSQL_STACK_MEMORY_MYSQL_THREADS_BYTES,
    FAM_PROXYSQL_STACK_MEMORY_ADMIN_THREADS_BYTES,
    FAM_PROXYSQL_STACK_MEMORY_CLUSTER_THREADS_BYTES,
    FAM_PROXYSQL_COMMAND_TIME_SECONDS,
    FAM_PROXYSQL_USER_FRONTEND_CONNECTIONS,
    FAM_PROXYSQL_USER_FRONTEND_MAX_CONNECTIONS,
    FAM_PROXYSQL_CONNECTION_POOL_STATUS,
    FAM_PROXYSQL_CONNECTION_POOL_CONNECTIONS_USED,
    FAM_PROXYSQL_CONNECTION_POOL_CONNECTIONS_FREE,
    FAM_PROXYSQL_CONNECTION_POOL_CONNECTIONS_OK,
    FAM_PROXYSQL_CONNECTION_POOL_CONNECTIONS_ERROR,
    FAM_PROXYSQL_CONNECTION_POOL_MAX_CONNECTIONS_USED,
    FAM_PROXYSQL_CONNECTION_POOL_QUERIES,
    FAM_PROXYSQL_CONNECTION_POOL_QUERIES_GTID_SYNC,
    FAM_PROXYSQL_CONNECTION_POOL_DATA_SEND_BYTES,
    FAM_PROXYSQL_CONNECTION_POOL_DATA_RECV_BYTES,
    FAM_PROXYSQL_CONNECTION_POOL_LATENCY_SECONDS,
    FAM_PROXYSQL_MAX
};

static metric_family_t proxysql_fams[FAM_PROXYSQL_MAX] = {
    [FAM_PROXYSQL_UP] = {
        .name = "proxysql_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the proxysql server be reached.",
    },
    [FAM_PROXYSQL_UPTIME_SECONDS] = {
        .name = "proxysql_uptime_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total uptime of ProxySQL in seconds",
    },
    [FAM_PROXYSQL_ACTIVE_TRANSACTIONS] = {
        .name = "proxysql_active_transactions",
        .type = METRIC_TYPE_GAUGE,
        .help = "Count of how many client connections are currently processing a transaction.",
    },
    [FAM_PROXYSQL_CLIENT_CONNECTIONS_ABORTED] = {
        .name = "proxysql_client_connections_aborted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Client failed connections (or closed improperly).",
    },
    [FAM_PROXYSQL_CLIENT_CONNECTIONS_CONNECTED] = {
        .name = "proxysql_client_connections_connected",
        .type = METRIC_TYPE_COUNTER,
        .help = "Client connections that are currently connected",
    },
    [FAM_PROXYSQL_CLIENT_CONNECTIONS_CREATED] = {
        .name = "proxysql_client_connections_created",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of client connections created",
    },
    [FAM_PROXYSQL_CLIENT_CONNECTIONS_NON_IDLE] = {
        .name = "proxysql_client_connections_non_idle",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of client connections that are currently handled by the main worker threads.",
    },
    [FAM_PROXYSQL_CLIENT_CONNECTIONS_HOSTGROUP_LOCKED]= {
        .name = "proxysql_client_connections_hostgroup_locked",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of client connection locked to a specific hostgroup.",
    },
    [FAM_PROXYSQL_SERVER_CONNECTIONS_ABORTED] = {
        .name = "proxysql_server_connections_aborted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Backend failed connections (or closed improperly).",
    },
    [FAM_PROXYSQL_SERVER_CONNECTIONS_CONNECTED] = {
        .name = "proxysql_server_connections_connected",
        .type = METRIC_TYPE_COUNTER,
        .help = "Backend connections that are currently connected.",
    },
    [FAM_PROXYSQL_SERVER_CONNECTIONS_CREATED] = {
        .name = "proxysql_server_connections_created",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of backend connections created",
    },
    [FAM_PROXYSQL_SERVER_CONNECTIONS_DELAYED] = {
        .name = "proxysql_server_connections_delayed",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_BACKEND_QUERY_TIME_SECONDS] = {
        .name = "proxysql_backend_query_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time spent making network calls to communicate with the backends.",
    },
    [FAM_PROXYSQL_QUERIES_BACKENDS_RECV_BYTES] = {
        .name = "proxysql_queries_backends_recv_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_QUERIES_BACKENDS_SENT_BYTES] = {
        .name = "proxysql_queries_backends_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_QUERIES_FRONTENDS_RECV_BYTES] = {
        .name = "proxysql_queries_frontends_recv_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_QUERIES_FRONTENDS_SENT_BYTES] = {
        .name = "proxysql_queries_frontends_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_BACKEND_LAGGING_DURING_QUERY] = {
        .name = "proxysql_backend_lagging_during_query",
        .type = METRIC_TYPE_COUNTER,
        .help = "Query failed because server was shunned due to lag.",
    },
    [FAM_PROXYSQL_BACKEND_OFFLINE_DURING_QUERY] = {
        .name = "proxysql_backend_offline_during_query",
        .type = METRIC_TYPE_COUNTER,
        .help = "Query failed because server was offline.",
    },
    [FAM_PROXYSQL_MYSQL_BACKEND_BUFFERS_BYTES] = {
        .name = "proxysql_mysql_backend_buffers_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Buffers related to backend connections if “fast_forward” "
                "is used (0 means fast_forward is not used)",
    },
    [FAM_PROXYSQL_MYSQL_FRONTEND_BUFFERS_BYTES] = {
        .name = "proxysql_mysql_frontend_buffers_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Buffers related to frontend connections (read/write buffers and other queues)",
    },
    [FAM_PROXYSQL_MYSQL_KILLED_BACKEND_CONNECTIONS] = {
        .name = "proxysql_mysql_killed_backend_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of backend connection killed.",
    },
    [FAM_PROXYSQL_MYSQL_KILLED_BACKEND_QUERIES] = {
        .name = "proxysql_mysql_killed_backend_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Killed backend queries.",
    },
    [FAM_PROXYSQL_MYSQL_UNEXPECTED_FRONTEND_COM_QUIT] = {
        .name = "proxysql_mysql_unexpected_frontend_com_quit",
        .type = METRIC_TYPE_COUNTER,
        .help = "Unexpected 'COM_QUIT' received from the client.",
    },
    [FAM_PROXYSQL_MYSQL_UNEXPECTED_FRONTEND_PACKETS] = {
        .name = "proxysql_mysql_unexpected_frontend_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Unexpected packet received from client.",
    },
    [FAM_PROXYSQL_CLIENT_HOST_ERROR_KILLED_CONNECTIONS] = {
        .name = "proxysql_client_host_error_killed_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Killed client connections because address exceeded 'client_host_error_counts'.",
    },
    [FAM_PROXYSQL_HOSTGROUP_LOCKED_QUERIES] = {
        .name = "proxysql_hostgroup_locked_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Query blocked because connection is locked into some hostgroup "
                "but is trying to reach other.",
    },
    [FAM_PROXYSQL_HOSTGROUP_LOCKED_SET_CMDS] = {
        .name = "proxysql_hostgroup_locked_set_cmds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of connections that have been locked in a hostgroup.",
    },
    [FAM_PROXYSQL_MAX_CONNECT_TIMEOUTS] = {
        .name = "proxysql_max_connect_timeouts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Maximum connection timeout reached when trying to connect to backend sever.",
    },
    [FAM_PROXYSQL_AUTOMATIC_DETECTED_SQL_INJECTION] = {
        .name = "proxysql_automatic_detected_sql_injection",
        .type = METRIC_TYPE_COUNTER,
	    .help = "Blocked a detected 'sql injection' attempt.",
    },
    [FAM_PROXYSQL_WHITELISTED_SQLI_FINGERPRINT] = {
        .name = "proxysql_whitelisted_sqli_fingerprint",
        .type = METRIC_TYPE_COUNTER,
		.help = "Detected a whitelisted 'sql injection' fingerprint.",
    },
    [FAM_PROXYSQL_GENERATED_ERROR_PACKETS] = {
        .name = "proxysql_generated_error_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total generated error packets.",
    },
    [FAM_PROXYSQL_MYSQL_SESSION_INTERNAL_BYTES] = {
        .name = "proxysql_mysql_session_internal_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Other memory used by ProxySQL to handle MySQL Sessions.",
    },
    [FAM_PROXYSQL_COM_FRONTEND_STMT] = {
        .name = "proxysql_com_frontend_stmt",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of statements (PREPARE|EXECUTE|CLOSE) "
                "executed by clients.",
    },
    [FAM_PROXYSQL_COM_BACKEND_STMT] = {
        .name = "proxysql_com_backend_stmt",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of statements (PREPARE|EXECUTE|CLOSE) "
                "executed by ProxySQL against the backends.",
    },
    [FAM_PROXYSQL_MYSQL_THREAD_WORKERS] = {
        .name = "proxysql_mysql_thread_workers",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of MySQL Thread workers i.e. “mysql-threads”.",
    },
    [FAM_PROXYSQL_MYSQL_MONITOR_WORKERS] = {
        .name = "proxysql_mysql_monitor_workers",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of monitor threads.",
    },
    [FAM_PROXYSQL_MYSQL_MONITOR_WORKERS_AUX] = {
        .name = "proxysql_mysql_monitor_workers_aux",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_PROXYSQL_MYSQL_MONITOR_WORKERS_STARTED] = {
        .name = "proxysql_mysql_monitor_workers_started",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_MYSQL_MONITOR_CONNECT_CHECK_ERR] = {
        .name = "proxysql_mysql_monitor_connect_check_err",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_MYSQL_MONITOR_CONNECT_CHECK_OK] = {
        .name = "proxysql_mysql_monitor_connect_check_ok",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_MYSQL_MONITOR_PING_CHECK_ERR] = {
        .name = "proxysql_mysql_monitor_ping_check_err",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_MYSQL_MONITOR_PING_CHECK_OK] = {
        .name = "proxysql_mysql_monitor_ping_check_ok",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_MYSQL_MONITOR_READ_ONLY_CHECK_ERR] = {
        .name = "proxysql_mysql_monitor_read_only_check_err",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_MYSQL_MONITOR_READ_ONLY_CHECK_OK] = {
        .name = "proxysql_mysql_monitor_read_only_check_ok",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_MYSQL_MONITOR_REPLICATION_LAG_CHECK_ERR] = {
        .name = "proxysql_mysql_monitor_replication_lag_check_err",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_MYSQL_MONITOR_REPLICATION_LAG_CHECK_OK] = {
        .name = "proxysql_mysql_monitor_replication_lag_check_ok",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_CONNPOOL_GET_CONN_SUCCESS] = {
        .name = "proxysql_connpool_get_conn_success",
        .type = METRIC_TYPE_COUNTER,
        .help= "The session is able to get a connection, "
               "either from per-thread cache or connection pool.",
    },
    [FAM_PROXYSQL_CONNPOOL_GET_CONN_FAILURE] = {
        .name = "proxysql_connpool_get_conn_failure",
        .type = METRIC_TYPE_COUNTER,
        .help = "The connection pool cannot provide any connection.",
    },
    [FAM_PROXYSQL_CONNPOOL_GET_CONN_IMMEDIATE] = {
        .name = "proxysql_connpool_get_conn_immediate",
        .type = METRIC_TYPE_COUNTER,
        .help = "The connection is provided from per-thread cache.",
    },
    [FAM_PROXYSQL_CONNPOOL_GET_CONN_LATENCY_AWARENESS] = {
        .name = "proxysql_connpool_get_conn_latency_awareness",
        .type = METRIC_TYPE_COUNTER,
        .help = "The connection was picked using the latency awareness algorithm.",
    },
    [FAM_PROXYSQL_QUESTIONS] = {
        .name = "proxysql_questions",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of client requests / statements executed",
    },
    [FAM_PROXYSQL_SLOW_QUERIES] = {
        .name = "proxysql_slow_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of queries with an execution time "
                "greater than “mysql-long_query_time” milliseconds",
    },
    [FAM_PROXYSQL_GTID_CONSISTENT_QUERIES] = {
        .name = "proxysql_gtid_consistent_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total queries with GTID consistent read.",
    },
    [FAM_PROXYSQL_GTID_SESSION_COLLECTED] = {
        .name = "proxysql_gtid_session_collected",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total queries with GTID session state.",
    },
    [FAM_PROXYSQL_MIRROR_CONCURRENCY] = {
        .name = "proxysql_mirror_concurrency",
        .type = METRIC_TYPE_COUNTER,
		.help = "Mirror current concurrency",
    },
    [FAM_PROXYSQL_MIRROR_QUEUE_LENGTH] = {
        .name = "proxysql_mirror_queue_length",
        .type = METRIC_TYPE_COUNTER,
		.help = "Mirror queue length",
    },
    [FAM_PROXYSQL_QUERIES_WITH_MAX_LAG] = {
        .name = "proxysql_queries_with_max_lag",
        .type = METRIC_TYPE_COUNTER,
        .help = "Received queries that have a 'max_lag' attribute.",
    },
    [FAM_PROXYSQL_QUERIES_WITH_MAX_LAG_DELAYED] = {
        .name = "proxysql_queries_with_max_lag_delayed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Query delayed because no connection was selected due to 'max_lag' annotation.",
    },
    [FAM_PROXYSQL_QUERIES_WITH_MAX_LAG_WAIT_TIME_SECONDS] = {
        .name = "proxysql_queries_with_max_lag_wait_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total waited time due to connection selection because of 'max_lag' annotation."
    },
    [FAM_PROXYSQL_AWS_AURORA_REPLICAS_SKIPPED_DURING_QUERY] = {
        .name = "proxysql_aws_aurora_replicas_skipped_during_query",
        .type = METRIC_TYPE_COUNTER,
        .help = "Replicas skipped due to current lag being higher than 'max_lag' annotation.",
    },
    [FAM_PROXYSQL_ACCESS_DENIED_MAX_CONNECTIONS] = {
        .name = "proxysql_access_denied_max_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_ACCESS_DENIED_MAX_USER_CONNECTIONS] = {
        .name = "proxysql_access_denied_max_user_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_ACCESS_DENIED_WRONG_PASSWORD] = {
        .name = "proxysql_access_denied_wrong_password",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_MYSQL_HOSTGROUPS_MANAGER_CONNECTION_POLL_GET] = {
        .name = "proxysql_mysql_hostgroups_manager_connection_poll_get",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of requests made to the connection pool.",
    },
    [FAM_PROXYSQL_MYSQL_HOSTGROUPS_MANAGER_CONNECTION_POLL_GET_OK] = {
        .name = "proxysql_mysql_hostgroups_manager_connection_poll_get_ok",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of successful requests to the connection pool "
                "(i.e. where a connection was available).",
    },
    [FAM_PROXYSQL_MYSQL_HOSTGROUPS_MANAGER_CONNECTION_POLL_PUSH] = {
        .name = "proxysql_mysql_hostgroups_manager_connection_poll_push",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of connections returned to the connection pool.",
    },
    [FAM_PROXYSQL_MYSQL_HOSTGROUPS_MANAGER_CONNECTION_POLL_DESTROY] = {
        .name = "proxysql_mysql_hostgroups_manager_connection_poll_destroy",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of connections considered unhealthy and therefore closed.",
    },
    [FAM_PROXYSQL_MYSQL_HOSTGROUPS_MANAGER_CONNECTION_POLL_RESET] = {
        .name = "proxysql_mysql_hostgroups_manager_connection_poll_reset",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of connections that have been reset / re-initialized "
                "using “COM_CHANGE_USER”.",
    },
    [FAM_PROXYSQL_CONNECTION_POOL_MEMORY_BYTES] = {
        .name = "proxysql_connection_pool_memory_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory used by the connection pool to store connections metadata.",
    },
    [FAM_PROXYSQL_STMT_CLIENT_ACTIVE] = {
        .name = "proxysql_stmt_client_active",
        .type = METRIC_TYPE_GAUGE, // FIXME ¿METRIC_TYPE_COUNTER?
        .help = "The number of prepared statements that are in use by clients.",
    },
    [FAM_PROXYSQL_STMT_CLIENT_ACTIVE_UNIQUE] = {
        .name = "proxysql_stmt_client_active_unique",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of unique prepared statements currently in use by clients.",
    },
    [FAM_PROXYSQL_STMT_SERVER_ACTIVE] = {
        .name = "proxysql_stmt_server_active",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total number of prepared statements currently available "
                "across all backend connections.",
    },
    [FAM_PROXYSQL_STMT_SERVER_ACTIVE_UNIQUE] = {
        .name = "proxysql_stmt_server_active_unique",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of unique prepared statements currently available "
                "across all backend connections.",
    },
    [FAM_PROXYSQL_STMT_CACHED] = {
        .name = "proxysql_stmt_cached",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of global prepared statements for which ProxySQL has metadata.",
    },
    [FAM_PROXYSQL_STMT_MAX_STMT_ID] = {
        .name = "proxysql_stmt_max_stmt_id",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum “stmt_id” ever used.",
    },
    [FAM_PROXYSQL_QUERY_CACHE_MEMORY_BYTES] = {
        .name = "proxysql_query_cache_memory_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory currently used by the query cache.",
    },
    [FAM_PROXYSQL_QUERY_CACHE_ENTRIES] = {
        .name = "proxysql_query_cache_entries",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of entries currently stored in the query cache.",
    },
    [FAM_PROXYSQL_QUERY_CACHE_PURGED] = {
        .name = "proxysql_query_cache_purged",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of entries purged by the Query Cache due to TTL expiration.",
    },
    [FAM_PROXYSQL_QUERY_CACHE_IN_BYTES] = {
        .name = "proxysql_query_cache_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes sent into the Query Cache.",
    },
    [FAM_PROXYSQL_QUERY_CACHE_OUT_BYTES] = {
        .name = "proxysql_query_cache_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes read from the Query Cache.",
    },
    [FAM_PROXYSQL_QUERY_CACHE_COUNT_GET] = {
        .name = "proxysql_query_cache_count_get",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of read requests.",
    },
    [FAM_PROXYSQL_QUERY_CACHE_COUNT_GET_OK] = {
        .name = "proxysql_query_cache_count_get_ok",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of successful read requests.",
    },
    [FAM_PROXYSQL_QUERY_CACHE_COUNT_SET] = {
        .name = "proxysql_query_cache_count_set",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of write requests.",
    },
    [FAM_PROXYSQL_QUERY_PROCESSOR_TIME_SECONDS] = {
        .name = "proxysql_query_processor_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The time spent inside the Query Processor to determine what "
                "action needs to be taken with the query (internal module).",
    },
    [FAM_PROXYSQL_JEMALLOC_ALLOCATED_BYTES] = {
        .name = "proxysql_jemalloc_allocated_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Bytes allocated by the application.",
    },
    [FAM_PROXYSQL_JEMALLOC_ACTIVE_BYTES] = {
        .name = "proxysql_jemalloc_active_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Bytes in pages allocated by the application.",
    },
    [FAM_PROXYSQL_JEMALLOC_MAPPED_BYTES] = {
        .name = "proxysql_jemalloc_mapped_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Bytes in extents mapped by the allocator.",
    },
    [FAM_PROXYSQL_JEMALLOC_METADATA_BYTES] = {
        .name = "proxysql_jemalloc_metadata_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Bytes dedicated to metadata.",
    },
    [FAM_PROXYSQL_JEMALLOC_RESIDENT_BYTES] = {
        .name = "proxysql_jemalloc_resident_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Bytes in physically resident data pages mapped by the allocator.",
    },
    [FAM_PROXYSQL_MEMORY_AUTH_BYTES] = {
        .name = "proxysql_memory_auth_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory used by the authentication module to store user credentials and attributes.",
    },
    [FAM_PROXYSQL_SQLITE_MEMORY_BYTES] = {
        .name = "proxysql_sqlite_memory_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory used by the embedded SQLite.",
    },
    [FAM_PROXYSQL_QUERY_DIGEST_MEMORY_BYTES] = {
        .name = "proxysql_query_digest_memory_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory used to store data related to stats_mysql_query_digest.",
    },
    [FAM_PROXYSQL_MYSQL_QUERY_RULES_MEMORY_BYTES] = {
        .name = "proxysql_mysql_query_rules_memory_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory used by query rules.",
    },
    [FAM_PROXYSQL_MYSQL_FIREWALL_USERS_TABLE_BYTES] = {
        .name = "proxysql_mysql_firewall_users_table_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory used for the lookup table of firewall users.",
    },
    [FAM_PROXYSQL_MYSQL_FIREWALL_USERS_CONFIG_BYTES] = {
        .name = "proxysql_mysql_firewall_users_config_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory used for configuration of firewall users.",
    },
    [FAM_PROXYSQL_MYSQL_FIREWALL_RULES_TABLE_BYTES] = {
        .name = "proxysql_mysql_firewall_rules_table_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory used for the lookup table of firewall rules.",
    },
    [FAM_PROXYSQL_MYSQL_FIREWALL_RULES_CONFIG_BYTES] = {
        .name = "proxysql_mysql_firewall_rules_config_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory used for configuration of firewall users.",
    },
    [FAM_PROXYSQL_STACK_MEMORY_MYSQL_THREADS_BYTES] = {
        .name = "proxysql_stack_memory_mysql_threads_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory of MySQL worker threads * stack size.",
    },
    [FAM_PROXYSQL_STACK_MEMORY_ADMIN_THREADS_BYTES] = {
        .name = "proxysql_stack_memory_admin_threads_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory of admin connections * stack size.",
    },
    [FAM_PROXYSQL_STACK_MEMORY_CLUSTER_THREADS_BYTES] = {
        .name = "proxysql_stack_memory_cluster_threads_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory of ProxySQL Cluster threads * stack size.",
    },
    [FAM_PROXYSQL_COMMAND_TIME_SECONDS] = {
        .name = "proxysql_command_time_seconds",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = NULL,
    },
    [FAM_PROXYSQL_USER_FRONTEND_CONNECTIONS] = {
        .name = "proxysql_user_frontend_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of connections currently used by this user.",
    },
    [FAM_PROXYSQL_USER_FRONTEND_MAX_CONNECTIONS] = {
        .name = "proxysql_user_frontend_max_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum number of connections this user is allowed to use.",
    },
    [FAM_PROXYSQL_CONNECTION_POOL_STATUS] = {
        .name = "proxysql_connection_pool_status",
        .type = METRIC_TYPE_STATE_SET,
        .help = "The status of the backend server.",
    },
    [FAM_PROXYSQL_CONNECTION_POOL_CONNECTIONS_USED] = {
        .name = "proxysql_connection_pool_connections_used",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many connections are currently used by ProxySQL "
                "for sending queries to the backend server",
    },
    [FAM_PROXYSQL_CONNECTION_POOL_CONNECTIONS_FREE] = {
        .name = "proxysql_connection_pool_connections_free",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many connections are currently free.",
    },
    [FAM_PROXYSQL_CONNECTION_POOL_CONNECTIONS_OK] = {
        .name = "proxysql_connection_pool_connections_ok",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many connections were established successfully.",
    },
    [FAM_PROXYSQL_CONNECTION_POOL_CONNECTIONS_ERROR] = {
        .name = "proxysql_connection_pool_connections_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "How many connections weren’t established successfully.",
    },
    [FAM_PROXYSQL_CONNECTION_POOL_MAX_CONNECTIONS_USED] = {
        .name = "proxysql_connection_pool_max_connections_used",
        .type = METRIC_TYPE_GAUGE,
        .help = "High water mark of connections used by ProxySQL "
                "for sending queries to the backend server.",
    },
    [FAM_PROXYSQL_CONNECTION_POOL_QUERIES] = {
        .name = "proxysql_connection_pool_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of queries routed towards this particular backend server.",
    },
    [FAM_PROXYSQL_CONNECTION_POOL_QUERIES_GTID_SYNC] = {
        .name = "proxysql_connection_pool_queries_gtid_sync",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PROXYSQL_CONNECTION_POOL_DATA_SEND_BYTES] = {
        .name = "proxysql_connection_pool_data_send_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The amount of data sent to the backend. "
                "This does not include metadata (packets’ headers).",
    },
    [FAM_PROXYSQL_CONNECTION_POOL_DATA_RECV_BYTES] = {
        .name = "proxysql_connection_pool_data_recv_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The amount of data received from the backend. This does not include metadata.",
    },
    [FAM_PROXYSQL_CONNECTION_POOL_LATENCY_SECONDS] = {
        .name = "proxysql_connection_pool_latency_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current ping time in seconds, as reported from Monitor.",
    },
};

#include "plugins/proxysql/proxysql.h"

typedef struct {
    char *instance;
    char *host;
    char *user;
    char *pass;

    /* proxysql_ssl_set params */
    char *key;
    char *cert;
    char *ca;
    char *capath;
    char *cipher;

    char *socket;
    int port;
    int timeout;

    c_complain_t conn_complaint;

    label_set_t labels;
    plugin_filter_t *filter;

    MYSQL *con;
    bool is_connected;
    unsigned long proxysql_version;

    metric_family_t fams[FAM_PROXYSQL_MAX];
} proxysql_t;

static void proxysql_free(void *arg)
{
    proxysql_t *db = arg;

    if (db == NULL)
        return;

    if (db->con != NULL)
        mysql_close(db->con);

    free(db->host);
    free(db->user);
    free(db->pass);
    free(db->socket);
    free(db->instance);
    free(db->key);
    free(db->cert);
    free(db->ca);
    free(db->capath);
    free(db->cipher);

    label_set_reset(&db->labels);
    plugin_filter_free(db->filter);

    free(db);
}

static MYSQL *proxysql_get_connection(proxysql_t *db)
{
    const char *cipher;

    if (db->is_connected) {
        int status = mysql_ping(db->con);
        if (status == 0)
            return db->con;

        PLUGIN_WARNING("Lost connection to instance '%s': %s",
                        db->instance, mysql_error(db->con));
    }

    db->is_connected = false;

    /* Close the old connection before initializing a new one. */
    if (db->con != NULL) {
        mysql_close(db->con);
        db->con = NULL;
    }

    db->con = mysql_init(NULL);
    if (db->con == NULL) {
        PLUGIN_ERROR("proxysql_init failed: %s", mysql_error(db->con));
        return NULL;
    }

    /* Configure TCP connect timeout (default: 0) */
    db->con->options.connect_timeout = db->timeout;

    mysql_ssl_set(db->con, db->key, db->cert, db->ca, db->capath, db->cipher);

    MYSQL *con = mysql_real_connect(db->con, db->host, db->user, db->pass, NULL,
                                    db->port, db->socket, 0);
    if (con == NULL) {
        c_complain(LOG_ERR, &db->conn_complaint,
                   "Failed to connect to proxysql at server %s: %s",
                   (db->host != NULL) ? db->host : "localhost", mysql_error(db->con));
        return NULL;
    }

    cipher = mysql_get_ssl_cipher(db->con);

    db->proxysql_version = mysql_get_server_version(db->con);

    c_do_release(LOG_INFO, &db->conn_complaint,
                 "Successfully connected to proxysql at server %s with cipher %s "
                 "(server version: %s, protocol version: %u) ",
                 mysql_get_host_info(db->con), (cipher != NULL) ? cipher : "<none>",
                 mysql_get_server_info(db->con), mysql_get_proto_info(db->con));

    db->is_connected = true;
    return db->con;
}

static MYSQL_RES *exec_query(MYSQL *con, const char *query)
{
    size_t query_len = strlen(query);

    if (mysql_real_query(con, query, query_len)) {
        PLUGIN_ERROR("Failed to execute query: %s", mysql_error(con));
        PLUGIN_INFO("SQL query was: %s", query);
        return NULL;
    }

    MYSQL_RES *res = mysql_store_result(con);
    if (res == NULL) {
        PLUGIN_ERROR("Failed to store query result: %s", mysql_error(con));
        PLUGIN_INFO("SQL query was: %s", query);
        return NULL;
    }

    return res;
}

static int proxysql_stats_mysql_global(proxysql_t *db, MYSQL *con)
{
    const char *query = "SELECT Variable_Name, Variable_Value FROM stats_mysql_global;";

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        char *key = row[0];
        char *val = row[1];

        if ((key == NULL) || (val == NULL))
            continue;

        const struct proxysql_kv *kv = proxysql_kv_get_key(key, strlen(key));
        if (kv == NULL)
            continue;
        if (kv->fam < 0)
            continue;
        if (kv->fam == FAM_PROXYSQL_SQLITE_MEMORY_BYTES)
            continue;

        metric_family_t *fam = &db->fams[kv->fam];

        value_t value = {0};
        if (fam->type == METRIC_TYPE_GAUGE) {
            value = VALUE_GAUGE(atof(val));
        } else if (fam->type == METRIC_TYPE_COUNTER) {
            if (kv->scale != 0.0) {
                value = VALUE_COUNTER_FLOAT64((double)atoll(val) * kv->scale);
            } else {
                value = VALUE_COUNTER(atoll(val));
            }
        }

        if (kv->lname != NULL) {
            metric_family_append(fam, value, &db->labels,
                                 &(label_pair_const_t){.name = kv->lname,
                                                       .value = kv->lvalue}, NULL);
        } else {
            metric_family_append(fam, value, &db->labels, NULL);
        }
    }

    mysql_free_result(res);
    return 0;
}

static int proxysql_stats_memory_metricsl(proxysql_t *db, MYSQL *con)
{
    const char *query = "SELECT Variable_Name, Variable_Value FROM stats_memory_metrics;";

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        char *key = row[0];
        char *val = row[1];

        if ((key == NULL) || (val == NULL))
            continue;

        const struct proxysql_kv *kv = proxysql_kv_get_key(key, strlen(key));
        if (kv == NULL)
            continue;
        if (kv->fam < 0)
            continue;

        metric_family_t *fam = &db->fams[kv->fam];

        value_t value = {0};
        if (fam->type == METRIC_TYPE_GAUGE) {
            value = VALUE_GAUGE(atof(val));
        } else if (fam->type == METRIC_TYPE_COUNTER) {
            if (kv->scale != 0.0) {
                value = VALUE_COUNTER_FLOAT64((double)atoll(val) * kv->scale);
            } else {
                value = VALUE_COUNTER(atoll(val));
            }
        }

        if (kv->lname != NULL) {
            metric_family_append(fam, value, &db->labels,
                                 &(label_pair_const_t){.name = kv->lname,
                                                       .value = kv->lvalue}, NULL);
        } else {
            metric_family_append(fam, value, &db->labels, NULL);
        }
    }

    mysql_free_result(res);
    return 0;
}
static int proxysql_stats_mysql_commands_counters(proxysql_t *db, MYSQL *con)
{
    const char *query = "SELECT Command, Total_Time_us, Total_cnt, cnt_100us, cnt_500us, "
                        "       cnt_1ms, cnt_5ms, cnt_10ms, cnt_50ms, cnt_100ms, cnt_500ms, "
                        "       cnt_1s, cnt_5s, cnt_10s, cnt_INFs "
                        "  FROM stats_mysql_commands_counters;";

    static double buckets[11] = {0.0001, 0.0005, 0.001, 0.005, 0.010, 0.050, 0.100, 0.500, 1, 5, 10};
    static size_t buckets_size = STATIC_ARRAY_SIZE(buckets);

    histogram_t *histogram = histogram_new_custom(buckets_size, buckets);
    if (histogram == NULL)
        return -1;

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL) {
        histogram_destroy(histogram);
        return -1;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        bool do_historgram = true;
        for (size_t i = 0; i < 15; i++) {
            if (row[i] == NULL) {
                do_historgram = false;
                break;
            }
        }

        if (!do_historgram)
            continue;

        histogram->sum = atof(row[1]) * 1e-6;

        histogram->buckets[1].counter = atof(row[3]); // cnt_100us
        histogram->buckets[2].counter = histogram->buckets[1].counter + atof(row[4]); // cnt_500us
        histogram->buckets[3].counter = histogram->buckets[2].counter + atof(row[5]); // cnt_1ms
        histogram->buckets[4].counter = histogram->buckets[3].counter + atof(row[6]); // cnt_5ms
        histogram->buckets[5].counter = histogram->buckets[4].counter + atof(row[7]); // cnt_10ms
        histogram->buckets[6].counter = histogram->buckets[5].counter + atof(row[8]); // cnt_50ms
        histogram->buckets[7].counter = histogram->buckets[6].counter + atof(row[9]); // cnt_100ms
        histogram->buckets[8].counter = histogram->buckets[7].counter + atof(row[10]); // cnt_500ms
        histogram->buckets[9].counter = histogram->buckets[8].counter + atof(row[11]); // cnt_1s
        histogram->buckets[10].counter = histogram->buckets[9].counter + atof(row[12]); // cnt_5s
        histogram->buckets[11].counter = histogram->buckets[10].counter + atof(row[13]); // cnt_10s
        histogram->buckets[0].counter = histogram->buckets[11].counter + atof(row[14]); // cnt_inf

        metric_family_append(&db->fams[FAM_PROXYSQL_COMMAND_TIME_SECONDS],
                             VALUE_HISTOGRAM(histogram), &db->labels,
                             &(label_pair_const_t){.name = "command", .value = row[0]},
                             NULL);

        histogram_reset(histogram);
    }

    histogram_destroy(histogram);

    mysql_free_result(res);
    return 0;
}

static int proxysql_stats_mysql_users(proxysql_t *db, MYSQL *con)
{
    const char *query = "SELECT username, frontend_connections, frontend_max_connections "
                        "  FROM stats_mysql_users;";

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {

        if ((row[0] == NULL) || (row[1] == NULL) || (row[2] == NULL))
            continue;

        metric_family_append(&db->fams[FAM_PROXYSQL_USER_FRONTEND_CONNECTIONS],
                             VALUE_GAUGE(atof(row[1])), &db->labels,
                             &(label_pair_const_t){.name = "username", .value = row[0]},
                             NULL);
        metric_family_append(&db->fams[FAM_PROXYSQL_USER_FRONTEND_MAX_CONNECTIONS],
                             VALUE_GAUGE(atof(row[2])), &db->labels,
                             &(label_pair_const_t){.name = "username", .value = row[0]},
                             NULL);
    }

    mysql_free_result(res);
    return 0;
}

static int proxysql_stats_mysql_connection_pool(proxysql_t *db, MYSQL *con)
{
    const char *query = "SELECT hostgroup, srv_host, srv_port, status, ConnUsed, ConnFree, ConnOK, "
                        "       ConnERR, MaxConnUsed, Queries, Queries_GTID_sync, Bytes_data_sent, "
                        "       Bytes_data_recv, Latency_us "
                        "  FROM stats_mysql_connection_pool;";

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    if (mysql_num_fields(res) != 14) {
        mysql_free_result(res);
        return -1;
    }

    static struct {
        int field;
        double scale;
        int fam;
    } fields[] = {
        { 4,   0.0, FAM_PROXYSQL_CONNECTION_POOL_CONNECTIONS_USED     },
        { 5,   0.0, FAM_PROXYSQL_CONNECTION_POOL_CONNECTIONS_FREE     },
        { 6,   0.0, FAM_PROXYSQL_CONNECTION_POOL_CONNECTIONS_OK       },
        { 7,   0.0, FAM_PROXYSQL_CONNECTION_POOL_CONNECTIONS_ERROR    },
        { 8,   0.0, FAM_PROXYSQL_CONNECTION_POOL_MAX_CONNECTIONS_USED },
        { 9,   0.0, FAM_PROXYSQL_CONNECTION_POOL_QUERIES              },
        { 10,  0.0, FAM_PROXYSQL_CONNECTION_POOL_QUERIES_GTID_SYNC    },
        { 11,  0.0, FAM_PROXYSQL_CONNECTION_POOL_DATA_SEND_BYTES      },
        { 12,  0.0, FAM_PROXYSQL_CONNECTION_POOL_DATA_RECV_BYTES      },
        { 13, 1e-6, FAM_PROXYSQL_CONNECTION_POOL_LATENCY_SECONDS      },
    };
    static size_t fields_size = STATIC_ARRAY_SIZE(fields);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        char *hostgroup = row[0];
        char *srv_host = row[1];
        char *srv_port = row[2];

        if ((hostgroup == NULL) || (srv_host == NULL) || (srv_port == NULL))
            continue;

        if (row[3] != NULL) {
            int status = atoi(row[3]);
            state_t states[] = {
                    { .name = "ONLINE",        .enabled = status == 1 },
                    { .name = "SHUNNED",       .enabled = status == 2 },
                    { .name = "OFFLINE_SOFT",  .enabled = status == 3 },
                    { .name = "OFFLINE_HARD.", .enabled = status == 4 },
            };
            state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
            metric_family_append(&db->fams[FAM_PROXYSQL_CONNECTION_POOL_STATUS],
                                 VALUE_STATE_SET(set), &db->labels,
                                 &(label_pair_const_t){.name = "hostgroup", .value = row[0]},
                                 &(label_pair_const_t){.name = "srv_host", .value = row[1]},
                                 &(label_pair_const_t){.name = "srv_port", .value = row[2]},
                                 NULL);
         }

        for (size_t n = 0; n < fields_size ; n++) {
            value_t value = {0};

            if (row[fields[n].field] == NULL)
                continue;

            if (db->fams[fields[n].fam].type == METRIC_TYPE_GAUGE) {
                value = VALUE_GAUGE(atof(row[fields[n].field]));
            } else if (db->fams[fields[n].fam].type == METRIC_TYPE_COUNTER) {
                if (fields[n].scale != 0.0) {
                    value = VALUE_COUNTER_FLOAT64((double)atoll(row[fields[n].field]) *
                                                  fields[n].scale);
                } else {
                    value = VALUE_COUNTER(atoll(row[fields[n].field]));
                }
            } else {
                continue;
            }

            metric_family_append(&db->fams[fields[n].fam], value, &db->labels,
                                 &(label_pair_const_t){.name = "hostgroup", .value = row[0]},
                                 &(label_pair_const_t){.name = "srv_host", .value = row[1]},
                                 &(label_pair_const_t){.name = "srv_port", .value = row[2]},
                                 NULL);
        }
    }

    mysql_free_result(res);
    return 0;
}

static int proxysql_read(user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    proxysql_t *db = (proxysql_t *)ud->data;

    /* An error message will have been printed in this case */
    MYSQL *con = proxysql_get_connection(db);
    if (con == NULL) {
        metric_family_append(&db->fams[FAM_PROXYSQL_UP], VALUE_GAUGE(0), &db->labels, NULL);
        plugin_dispatch_metric_family_filtered(&db->fams[FAM_PROXYSQL_UP], db->filter, 0);
        return 0;
    }

    metric_family_append(&db->fams[FAM_PROXYSQL_UP], VALUE_GAUGE(1), &db->labels, NULL);

    proxysql_stats_mysql_global(db, con);
    proxysql_stats_memory_metricsl(db, con);
    proxysql_stats_mysql_commands_counters(db, con);
    proxysql_stats_mysql_users(db, con);
    proxysql_stats_mysql_connection_pool(db, con);

    plugin_dispatch_metric_family_array_filtered(db->fams, FAM_PROXYSQL_MAX, db->filter, 0);

    return 0;
}

static int proxysql_config_instance(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The 'instance' block needs exactly one string argument.");
        return -1;
    }

    proxysql_t *db = calloc(1, sizeof(*db));
    if (db == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    C_COMPLAIN_INIT(&db->conn_complaint);

    memcpy(db->fams, proxysql_fams, sizeof(db->fams[0])*FAM_PROXYSQL_MAX);

    int status = cf_util_get_string(ci, &db->instance);
    if (status != 0) {
        free(db);
        return status;
    }
    assert(db->instance != NULL);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &db->host);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &db->user);
        } else if (strcasecmp("user-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &db->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &db->pass);
        } else if (strcasecmp("password-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &db->pass);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_port_number(child, &db->port);
        } else if (strcasecmp("socket", child->key) == 0) {
            status = cf_util_get_string(child, &db->socket);
        } else if (strcasecmp("ssl-key", child->key) == 0) {
            status = cf_util_get_string(child, &db->key);
        } else if (strcasecmp("ssl-cert", child->key) == 0) {
            status = cf_util_get_string(child, &db->cert);
        } else if (strcasecmp("ssl-ca", child->key) == 0) {
            status = cf_util_get_string(child, &db->ca);
        } else if (strcasecmp("ssl-ca-path", child->key) == 0) {
            status = cf_util_get_string(child, &db->capath);
        } else if (strcasecmp("ssl-cipher", child->key) == 0) {
            status = cf_util_get_string(child, &db->cipher);
        } else if (strcasecmp("connect-timeout", child->key) == 0) {
            status = cf_util_get_int(child, &db->timeout);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &db->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("filter", child->key) == 0) {
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
        proxysql_free(db);
        return -1;
    }

    label_set_add(&db->labels, true, "instance", db->instance);

    return plugin_register_complex_read("proxysql", db->instance, proxysql_read, interval,
                                        &(user_data_t){.data=db, .free_func=proxysql_free});
}

static int proxysql_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = proxysql_config_instance(child);
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

static int proxysql_init(void)
{
    if (mysql_library_init(0, NULL, NULL)) {
        PLUGIN_ERROR("could not initialize MySQL client library.");
        return -1;
    }

    return 0;
}

static int proxysql_shutdown(void)
{
    mysql_library_end();
    return 0;
}

void module_register(void)
{
    plugin_register_init("proxysql", proxysql_init);
    plugin_register_config("proxysql", proxysql_config);
    plugin_register_shutdown("proxysql", proxysql_shutdown);
}
