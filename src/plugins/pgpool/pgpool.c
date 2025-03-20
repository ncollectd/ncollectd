// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2008-2012 Sebastian Harl
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "config.h"

#if defined(STRPTIME_NEEDS_STANDARDS) || defined(HAVE_STRPTIME_STANDARDS)
#    ifndef _ISOC99_SOURCE
#        define _ISOC99_SOURCE 1
#    endif
#    ifndef _POSIX_C_SOURCE
#        define _POSIX_C_SOURCE 200809L
#    endif
#    ifndef _XOPEN_SOURCE
#        define _XOPEN_SOURCE 500
#    endif
#endif

#ifdef TIMEGM_NEEDS_DEFAULT
#    ifndef _DEFAULT_SOURCE
#        define _DEFAULT_SOURCE 1
#    endif
#elif defined(TIMEGM_NEEDS_BSD)
#    ifndef _BSD_SOURCE
#        define _BSD_SOURCE 1
#    endif
#endif

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/complain.h"

#include <libpq-fe.h>
#include <pg_config_manual.h>

enum {
    FAM_PGPOOL_UP,
	FAM_PGPOOL_BACKEND_SLOTS,
	FAM_PGPOOL_BACKEND_SLOTS_INUSE,
	FAM_PGPOOL_PROCESSES,
	FAM_PGPOOL_PROCESSES_INUSE,
    FAM_PGPOOL_NODE_STATUS,
    FAM_PGPOOL_NODE_SELECTS,
    FAM_PGPOOL_NODE_REPLICATION_DELAY_SECONDS,
    FAM_PGPOOL_BACKEND_STATUS,
    FAM_PGPOOL_BACKEND_SELECT,
    FAM_PGPOOL_BACKEND_INSERT,
    FAM_PGPOOL_BACKEND_UPDATE,
    FAM_PGPOOL_BACKEND_DELETE,
    FAM_PGPOOL_BACKEND_DDL,
    FAM_PGPOOL_BACKEND_OTHER,
    FAM_PGPOOL_BACKEND_PANIC,
    FAM_PGPOOL_BACKEND_FATAL,
    FAM_PGPOOL_BACKEND_ERROR,
    FAM_PGPOOL_BACKEND_HEALTH_CHECK_STATUS,
    FAM_PGPOOL_BACKEND_HEALTH_CHECK,
    FAM_PGPOOL_BACKEND_HEALTH_CHECK_SUCCESS,
    FAM_PGPOOL_BACKEND_HEALTH_CHECK_FAIL,
    FAM_PGPOOL_BACKEND_HEALTH_CHECK_SKIP,
    FAM_PGPOOL_BACKEND_HEALTH_CHECK_RETRY,
    FAM_PGPOOL_BACKEND_HEALTH_CHECK_LAST_SECONDS,
    FAM_PGPOOL_BACKEND_HEALTH_CHECK_LAST_SUCCESSFUL_SECONDS,
    FAM_PGPOOL_BACKEND_HEALTH_CHECK_LAST_SKIP_SECONDS,
    FAM_PGPOOL_BACKEND_HEALTH_CHECK_LAST_FAILED_SECONDS,
    FAM_PGPOOL_CACHE_HITS,
    FAM_PGPOOL_CACHE_SELECTS,
    FAM_PGPOOL_CACHE_HIT_RATIO,
    FAM_PGPOOL_CACHE_HASH_ENTRIES,
    FAM_PGPOOL_CACHE_USED_HASH_ENTRIES,
    FAM_PGPOOL_CACHE_ENTRIES,
    FAM_PGPOOL_CACHE_ENTRIES_USED_BYTES,
    FAM_PGPOOL_CACHE_ENTRIES_FREE_BYTES,
    FAM_PGPOOL_CACHE_ENTRIES_FRAGMENTED_BYTES,
    FAM_PGPOOL_MAX,
};

static metric_family_t pgpool_fams[FAM_PGPOOL_MAX] = {
    [FAM_PGPOOL_UP] = {
        .name = "pgpool_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "",
    },
	[FAM_PGPOOL_BACKEND_SLOTS] = {
	    .name = "pgpool_backend_slots",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of total possible backend connection slots.",
    },
	[FAM_PGPOOL_BACKEND_SLOTS_INUSE] = {
	    .name = "pgpool_backend_slots_inuse",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of backend connection slots in use.",
    },
	[FAM_PGPOOL_PROCESSES] = {
	    .name = "pgpool_processes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of total child processed.",
    },
	[FAM_PGPOOL_PROCESSES_INUSE] = {
	    .name = "pgpool_processes_inuse",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of used child processes.",
    },
    [FAM_PGPOOL_NODE_STATUS] = {
        .name = "pgpool_node_status",
        .type = METRIC_TYPE_GAUGE,
        .help = "Backend node Status (1 for up or waiting, 0 for down or unused)",
    },
    [FAM_PGPOOL_NODE_SELECTS] = {
        .name = "pgpool_node_selects",
        .help = "SELECT statement counts issued to each backend",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_PGPOOL_NODE_REPLICATION_DELAY_SECONDS] = {
        .name = "pgpool_node_replication_delay_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Replication delay",
    },
    [FAM_PGPOOL_BACKEND_STATUS] = {
        .name = "pgpool_backend_status",
        .type = METRIC_TYPE_GAUGE,
        .help = "Backend node Status (1 for up or waiting, 0 for down or unused)."
    },
    [FAM_PGPOOL_BACKEND_SELECT] = {
        .name = "pgpool_backend_select",
        .type = METRIC_TYPE_COUNTER,
        .help = "SELECT statement counts issued to each backend"
    },
    [FAM_PGPOOL_BACKEND_INSERT] = {
        .name = "pgpool_backend_insert",
        .type = METRIC_TYPE_COUNTER,
        .help = "INSERT statement counts issued to each backend"
    },
    [FAM_PGPOOL_BACKEND_UPDATE] = {
        .name = "pgpool_backend_update",
        .type = METRIC_TYPE_COUNTER,
        .help = "UPDATE statement counts issued to each backend"
    },
    [FAM_PGPOOL_BACKEND_DELETE] = {
        .name = "pgpool_backend_delete",
        .type = METRIC_TYPE_COUNTER,
        .help = "DELETE statement counts issued to each backend"
    },
    [FAM_PGPOOL_BACKEND_DDL] = {
        .name = "pgpool_backend_ddl",
        .type = METRIC_TYPE_COUNTER,
        .help = "DDL statement counts issued to each backend"
    },
    [FAM_PGPOOL_BACKEND_OTHER] = {
        .name = "pgpool_backend_other",
        .type = METRIC_TYPE_COUNTER,
        .help = "other statement counts issued to each backend"
    },
    [FAM_PGPOOL_BACKEND_PANIC] = {
        .name = "pgpool_backend_panic",
        .type = METRIC_TYPE_COUNTER,
        .help = "Panic message counts returned from backend"
    },
    [FAM_PGPOOL_BACKEND_FATAL] = {
        .name = "pgpool_backend_fatal",
        .type = METRIC_TYPE_COUNTER,
        .help = "Fatal message counts returned from backend"
    },
    [FAM_PGPOOL_BACKEND_ERROR] = {
        .name = "pgpool_backend_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Error message counts returned from backend"
    },
    [FAM_PGPOOL_BACKEND_HEALTH_CHECK_STATUS] = {
        .name = "pgpool_backend_health_check_status",
        .type = METRIC_TYPE_GAUGE,
        .help = "Backend node Status (1 for up or waiting, 0 for down or unused).",
    },
    [FAM_PGPOOL_BACKEND_HEALTH_CHECK] = {
        .name = "pgpool_backend_health_check",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of health check count in total.",
    },
    [FAM_PGPOOL_BACKEND_HEALTH_CHECK_SUCCESS] = {
        .name = "pgpool_backend_health_check_success",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of successful health check count in total.",
    },
    [FAM_PGPOOL_BACKEND_HEALTH_CHECK_FAIL] = {
        .name = "pgpool_backend_health_check_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of failed health check count in total.",
    },
    [FAM_PGPOOL_BACKEND_HEALTH_CHECK_SKIP] = {
        .name = "pgpool_backend_health_check_skip",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of skipped health check count in total.",
    },
    [FAM_PGPOOL_BACKEND_HEALTH_CHECK_RETRY] = {
        .name = "pgpool_backend_health_check_retry",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of retried health check count in total.",
    },
    [FAM_PGPOOL_BACKEND_HEALTH_CHECK_LAST_SECONDS] = {
        .name = "pgpool_backend_health_check_last_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Timestamp in seconds of last health check.",
    },
    [FAM_PGPOOL_BACKEND_HEALTH_CHECK_LAST_SUCCESSFUL_SECONDS] = {
        .name = "pgpool_backend_health_check_last_successful_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Timestamp in seconds of last successful health check.",
    },
    [FAM_PGPOOL_BACKEND_HEALTH_CHECK_LAST_SKIP_SECONDS] = {
        .name = "pgpool_backend_health_check_last_skip_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Timestamp in seconds of last skipped health check.",
    },
    [FAM_PGPOOL_BACKEND_HEALTH_CHECK_LAST_FAILED_SECONDS] = {
        .name = "pgpool_backend_health_check_last_failed_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Timestamp in seconds of last failed health check.",
    },
    [FAM_PGPOOL_CACHE_HITS] = {
        .name = "pgpool_cache_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of hits against the query cache.",
    },
    [FAM_PGPOOL_CACHE_SELECTS] = {
        .name = "pgpool_cache_selects",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of SELECT that did not hit against the query cache.",
    },
    [FAM_PGPOOL_CACHE_HIT_RATIO] = {
        .name = "pgpool_cache_hit_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "The cache hit ratio. Calculated as num_cache_hits/(num_cache_hits+num_selects).",
    },
    [FAM_PGPOOL_CACHE_HASH_ENTRIES] = {
        .name = "pgpool_cache_hash_entries",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of entries in the hash table used to manage the cache.",
    },
    [FAM_PGPOOL_CACHE_USED_HASH_ENTRIES] = {
        .name = "pgpool_cache_used_hash_entries",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of used hash entries.",
    },
    [FAM_PGPOOL_CACHE_ENTRIES] = {
        .name = "pgpool_cache_entries",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of cache entries already used.",
    },
    [FAM_PGPOOL_CACHE_ENTRIES_USED_BYTES] = {
        .name = "pgpool_cache_entries_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The size in bytes of used cache size."
    },
    [FAM_PGPOOL_CACHE_ENTRIES_FREE_BYTES] = {
        .name = "pgpool_cache_entries_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The size in bytes of free cache size.",
    },
    [FAM_PGPOOL_CACHE_ENTRIES_FRAGMENTED_BYTES] = {
        .name = "pgpool_cache_entries_fragmented_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The size in bytes of the fragmented cache.",
    },
};

#define C_PSQL_IS_UNIX_DOMAIN_SOCKET(host)                       \
    (((host) == NULL) || (*(host) == '\0') || (*(host) == '/'))

/* Returns the tuple (host, delimiter, port) for a given (host, port) pair.
 * Depending on the value of 'host' a UNIX domain socket or a TCP socket is assumed. */
#define C_PSQL_SOCKET3(host, port)                                          \
    (((host) == NULL) || (*(host) == '\0')) ? DEFAULT_PGSOCKET_DIR : host,  \
            C_PSQL_IS_UNIX_DOMAIN_SOCKET(host) ? "/.s.PGSQL." : ":", port

typedef struct {
    char *instance;

    PGconn *conn;
    c_complain_t conn_complaint;

    char *host;
    char *port;
    char *database;
    char *user;
    char *password;
    char *sslmode;

    label_set_t labels;
    plugin_filter_t *filter;
    metric_family_t fams[FAM_PGPOOL_MAX];
} pgpool_instance_t;

static void pgpool_instance_delete(void *data)
{
    pgpool_instance_t *db = data;
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

    label_set_reset(&db->labels);
    plugin_filter_free(db->filter);

    free(db);
}

static int pgpool_connect(pgpool_instance_t *db)
{
    if ((!db) || (!db->database))
        return -1;

    char conninfo[4096];
    strbuf_t buf = STRBUF_CREATE_STATIC(conninfo);
    struct {
        char *param;
        char *value;
    } pgpool_params[] = {
        { "dbname",           db->database       },
        { "host",             db->host           },
        { "port",             db->port           },
        { "user",             db->user           },
        { "password",         db->password       },
        { "sslmode",          db->sslmode        },
        { "application_name", "ncollectd_pgpool" }
    };
    size_t pgpool_params_size = STATIC_ARRAY_SIZE(pgpool_params);

    for (size_t i = 0; i  < pgpool_params_size; i++) {
        if ((pgpool_params[i].value != NULL) && (pgpool_params[i].value[0] != '\0')) {
            strbuf_putstr(&buf, pgpool_params[i].param);
            strbuf_putstr(&buf, " = '");
            strbuf_putstr(&buf, pgpool_params[i].value);
            strbuf_putstr(&buf, "' ");
        }
    }

    db->conn = PQconnectdb(conninfo);
    return 0;
}

static int pgpool_check_connection(pgpool_instance_t *db)
{
    bool init = false;

    if (!db->conn) {
        init = true;

        /* trigger c_release() */
        if (db->conn_complaint.interval == 0)
            db->conn_complaint.interval = 1;

        pgpool_connect(db);
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
    }

    if (c_would_release(&db->conn_complaint)) {
        char *server_host = PQhost(db->conn);

        c_do_release(LOG_INFO, &db->conn_complaint,
                                 "Successfully %sconnected to pgbouncer (db %s) (user %s) "
                                 "at server %s%s%s ",
                                 init ? "" : "re", PQdb(db->conn), PQuser(db->conn),
                                 C_PSQL_SOCKET3(server_host, PQport(db->conn)));

    }
    return 0;
}

static char *pgpool_get_string(const PGresult *res, int row, const char *column_name)
{
    int col = PQfnumber(res, column_name);
    if (col < 0)
        return NULL;

    if (PQgetisnull(res, row, col))
        return NULL;

    return PQgetvalue(res, row, col);
}

__attribute__ ((sentinel(0)))
static double pgpool_status_append(const PGresult *res, int row, const char *column_name,
                                   metric_family_t *fam, label_set_t *labels, ...)
{
    int col = PQfnumber(res, column_name);
    if (col < 0)
        return -1;

    if (PQgetisnull(res, row, col))
        return -1;


    char *value = PQgetvalue(res, row, col);
    if (value == NULL)
        return -1;

    double status = 0;
    switch(value[0]) {
    case 't':
        if (strcmp(value, "true") == 0)
            status = 1;
        break;
    case 'u':
        if (strcmp(value, "up") == 0)
            status = 1;
        else if (strcmp(value, "unused") == 0)
            status = 0;
        break;
    case 'w':
        if (strcmp(value, "waiting") == 0)
            status = 1;
        break;
    case 'f':
        if (strcmp(value, "false") == 0)
            status = 0;
        break;
    case 'd':
        if (strcmp(value, "down") == 0)
            status = 0;
        break;
    }

    va_list ap;
    va_start(ap, labels);

    switch(fam->type) {
    case METRIC_TYPE_COUNTER:
        metric_family_append_va(fam, VALUE_COUNTER_FLOAT64(status), labels, ap);
        break;
    case METRIC_TYPE_GAUGE:
        metric_family_append_va(fam, VALUE_GAUGE(status), labels, ap);
        break;
    default:
        break;
    }

    va_end(ap);

    return 0;
}

__attribute__ ((sentinel(0)))
static double pgpool_timestamp_append(const PGresult *res, int row, const char *column_name,
                                      metric_family_t *fam, label_set_t *labels, ...)
{
    int col = PQfnumber(res, column_name);
    if (col < 0)
        return -1;

    if (PQgetisnull(res, row, col))
        return -1;


    char *value = PQgetvalue(res, row, col);
    if (value == NULL)
        return -1;

    time_t ts = 0;

    if (value[0] != '\0') {
        struct tm tm = {0};
        char *tmp = strptime(value, "%Y-%m-%d %T", &tm);
        if (tmp == NULL) {
            PLUGIN_ERROR("strptime failed.");
            return -1;
        }
#ifdef HAVE_TIMEGM
        ts = timegm(&tm);
        if (ts == ((time_t)-1)) {
            PLUGIN_ERROR("timegm() failed: %s", STRERRNO);
            return -1;
        }
#else
        ts = mktime(&tm);
        if (ts == ((time_t)-1)) {
            PLUGIN_ERROR("mktime() failed: %s", STRERRNO);
            return -1;
        }
        /* mktime assumes that tm is local time. Luckily, it also sets timezone to
         * the offset used for the conversion, and we undo the conversion to convert
         * back to UTC. */
        ts = ts - timezone;
#endif
    }

    va_list ap;
    va_start(ap, labels);

    switch(fam->type) {
    case METRIC_TYPE_COUNTER:
        metric_family_append_va(fam, VALUE_COUNTER(ts), labels, ap);
        break;
    case METRIC_TYPE_GAUGE:
        metric_family_append_va(fam, VALUE_GAUGE(ts), labels, ap);
        break;
    default:
        break;
    }

    va_end(ap);

    return 0;
}

__attribute__ ((sentinel(0)))
static int pgpool_metric_append(const PGresult *res, int row, const char *column_name,
                                metric_family_t *fam, double scale, label_set_t *labels, ...)
{
    int col = PQfnumber(res, column_name);
    if (col < 0)
        return -1;

    if (PQgetisnull(res, row, col))
        return -1;

    char *value = PQgetvalue(res, row, col);
    if (value == NULL)
        return -1;

    va_list ap;
    va_start(ap, labels);

    switch(fam->type) {
    case METRIC_TYPE_COUNTER:
        if (scale != 1.0)
            metric_family_append_va(fam, VALUE_COUNTER_FLOAT64(atof(value) * scale), labels, ap);
        else
            metric_family_append_va(fam, VALUE_COUNTER(atoi(value)), labels, ap);
        break;
    case METRIC_TYPE_GAUGE:
        metric_family_append_va(fam, VALUE_GAUGE(atof(value) * scale), labels, ap);
        break;
    default:
        break;
    }

    va_end(ap);

    return 0;
}

static int pgpool_show_pool_pools(PGconn *conn, metric_family_t *fams, label_set_t *labels)
{
    PGresult *res = PQexec(conn, "SHOW POOL_POOLS");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexec failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    int fields = PQnfields(res);
    if (fields < 5)
        goto error;

    int pool_pid_idx = PQfnumber(res, "pool_pid");
    if (pool_pid_idx < 0)
        goto error;

    int username_idx = PQfnumber(res, "username");
    if (username_idx < 0)
        goto error;


    double backends = 0;
    double backends_inuse = 0;

    for (int i = 0; i < PQntuples(res); i++) {
        if (!PQgetisnull(res, i, pool_pid_idx)) {
            char *pool_pid = PQgetvalue(res, i, pool_pid_idx);
            if ((pool_pid != NULL) && (pool_pid[0] != '\0')) {
                backends++;
            }
        }

        if (!PQgetisnull(res, i, username_idx)) {
            char *username = PQgetvalue(res, i, username_idx);
            if ((username != NULL) && (username[0] != '\0')) {
                backends_inuse++;
            }
        }
    }

    metric_family_append(&fams[FAM_PGPOOL_BACKEND_SLOTS], VALUE_GAUGE(backends), labels, NULL);
    metric_family_append(&fams[FAM_PGPOOL_BACKEND_SLOTS_INUSE], VALUE_GAUGE(backends_inuse),
                         labels, NULL);

    PQclear(res);
    return 0;

error:
    PQclear(res);
    return -1;
}

static int pgpool_show_pool_processes(PGconn *conn, metric_family_t *fams, label_set_t *labels)
{
    PGresult *res = PQexec(conn, "SHOW POOL_PROCESSES");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexec failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    int fields = PQnfields(res);
    if (fields < 2) {
        PQclear(res);
        return -1;
    }

    int database_idx = PQfnumber(res, "database");
    if (database_idx < 0)
        goto error;

    int username_idx = PQfnumber(res, "username");
    if (username_idx < 0)
        goto error;

    double processes = 0;
    double processes_inuse = 0;

    for (int i = 0; i < PQntuples(res); i++) {
        processes++;
        if (!PQgetisnull(res, i, database_idx) && !PQgetisnull(res, i, username_idx)) {
            char *database = PQgetvalue(res, i, database_idx);
            char *username = PQgetvalue(res, i, username_idx);
            if ((database != NULL) && (database[0] != '\0') &&
                (username != NULL) && (username[0] != '\0')) {
                processes_inuse++;
            }
        }
    }

    metric_family_append(&fams[FAM_PGPOOL_PROCESSES], VALUE_GAUGE(processes), labels, NULL);
    metric_family_append(&fams[FAM_PGPOOL_PROCESSES_INUSE], VALUE_GAUGE(processes_inuse), labels, NULL);

    PQclear(res);
    return 0;

error:
    PQclear(res);
    return -1;
}

static int pgpool_show_pool_health_check_stats(PGconn *conn, unsigned int version,
                                               metric_family_t *fams, label_set_t *labels)
{
    if (version < 40200)
        return 0;

    PGresult *res = PQexec(conn, "SHOW POOL_HEALTH_CHECK_STATS");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexec failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    int fields = PQnfields(res);
    if (fields < 20) {
        PQclear(res);
        return -1;
    }

    struct {
        int fam;
        bool ts;
        char *name;
    } columns[] = {
        { FAM_PGPOOL_BACKEND_HEALTH_CHECK,                         false, "total_count"                  },
        { FAM_PGPOOL_BACKEND_HEALTH_CHECK_SUCCESS,                 false, "success_count"                },
        { FAM_PGPOOL_BACKEND_HEALTH_CHECK_FAIL,                    false, "fail_count"                   },
        { FAM_PGPOOL_BACKEND_HEALTH_CHECK_SKIP,                    false, "skip_count"                   },
        { FAM_PGPOOL_BACKEND_HEALTH_CHECK_RETRY,                   false, "retry_count"                  },
        { FAM_PGPOOL_BACKEND_HEALTH_CHECK_LAST_SECONDS,            true,  "last_health_check"            },
        { FAM_PGPOOL_BACKEND_HEALTH_CHECK_LAST_SUCCESSFUL_SECONDS, true,  "last_successful_health_check" },
        { FAM_PGPOOL_BACKEND_HEALTH_CHECK_LAST_SKIP_SECONDS,       true,  "last_skip_health_check"       },
        { FAM_PGPOOL_BACKEND_HEALTH_CHECK_LAST_FAILED_SECONDS,     true,  "last_failed_health_check"     },
    };
    size_t columns_size = STATIC_ARRAY_SIZE(columns);

    for (int i = 0; i < PQntuples(res); i++) {
        char *hostname = pgpool_get_string(res, i, "hostname");
        if (hostname == NULL)
            continue;
        char *port = pgpool_get_string(res, i, "port");
        if (port == NULL)
            continue;
        char *role = pgpool_get_string(res, i, "role");
        if (role == NULL)
            continue;

        pgpool_status_append(res, i, "status", &fams[FAM_PGPOOL_BACKEND_HEALTH_CHECK_STATUS],
                             labels,
                             &(label_pair_const_t){.name="hostname", .value=hostname},
                             &(label_pair_const_t){.name="port", .value=port},
                             &(label_pair_const_t){.name="role", .value=role}, NULL);

        for (size_t j = 0; j < columns_size ; j++) {
            if (columns[j].ts)
                pgpool_timestamp_append(res, i, columns[j].name, &fams[columns[j].fam], labels,
                                        &(label_pair_const_t){.name="hostname", .value=hostname},
                                        &(label_pair_const_t){.name="port", .value=port},
                                        &(label_pair_const_t){.name="role", .value=role}, NULL);
            else
                pgpool_metric_append(res, i, columns[j].name, &fams[columns[j].fam], 1.0, labels,
                                     &(label_pair_const_t){.name="hostname", .value=hostname},
                                     &(label_pair_const_t){.name="port", .value=port},
                                     &(label_pair_const_t){.name="role", .value=role}, NULL);
        }
    }

    PQclear(res);

    return 0;
}

static int pgpool_show_pool_backend_stats(PGconn *conn, unsigned int version,
                                          metric_family_t *fams, label_set_t *labels)
{
    if (version < 40200)
        return 0;

    PGresult *res = PQexec(conn, "SHOW POOL_BACKEND_STATS");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexec failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    int fields = PQnfields(res);
    if (fields < 14) {
        PQclear(res);
        return -1;
    }

    struct {
        int fam;
        char *name;
    } columns[] = {
        { FAM_PGPOOL_BACKEND_SELECT,   "select_cnt" },
        { FAM_PGPOOL_BACKEND_INSERT,   "insert_cnt" },
        { FAM_PGPOOL_BACKEND_UPDATE,   "update_cnt" },
        { FAM_PGPOOL_BACKEND_DELETE,   "delete_cnt" },
        { FAM_PGPOOL_BACKEND_DDL,      "ddl_cnt"    },
        { FAM_PGPOOL_BACKEND_OTHER,    "other_cnt"  },
        { FAM_PGPOOL_BACKEND_PANIC,    "panic_cnt"  },
        { FAM_PGPOOL_BACKEND_FATAL,    "fatal_cnt"  },
        { FAM_PGPOOL_BACKEND_ERROR,    "error_cnt"  },
    };
    size_t columns_size = STATIC_ARRAY_SIZE(columns);

    for (int i = 0; i < PQntuples(res); i++) {
        char *hostname = pgpool_get_string(res, i, "hostname");
        if (hostname == NULL)
            continue;
        char *port = pgpool_get_string(res, i, "port");
        if (port == NULL)
            continue;
        char *role = pgpool_get_string(res, i, "role");
        if (role == NULL)
            continue;

        pgpool_status_append(res, i, "status", &fams[FAM_PGPOOL_BACKEND_STATUS], labels,
                             &(label_pair_const_t){.name="hostname", .value=hostname},
                             &(label_pair_const_t){.name="port", .value=port},
                             &(label_pair_const_t){.name="role", .value=role}, NULL);

        for (size_t j = 0; j < columns_size ; j++) {
            pgpool_metric_append(res, i, columns[j].name, &fams[columns[j].fam], 1.0, labels,
                                 &(label_pair_const_t){.name="hostname", .value=hostname},
                                 &(label_pair_const_t){.name="port", .value=port},
                                 &(label_pair_const_t){.name="role", .value=role}, NULL);
        }
    }

    PQclear(res);

    return 0;
}

static int pgpool_show_pool_nodes(PGconn *conn, metric_family_t *fams, label_set_t *labels)
{
    PGresult *res = PQexec(conn, "SHOW POOL_NODES");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexec failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    int fields = PQnfields(res);
    if (fields < 10) {
        PQclear(res);
        return -1;
    }

    struct {
        int fam;
        char *name;
    } columns[] = {
        { FAM_PGPOOL_NODE_SELECTS,                   "select_cnt"        },
        { FAM_PGPOOL_NODE_REPLICATION_DELAY_SECONDS, "replication_delay" },
    };
    size_t columns_size = STATIC_ARRAY_SIZE(columns);

    for (int i = 0; i < PQntuples(res); i++) {
        char *hostname = pgpool_get_string(res, i, "hostname");
        if (hostname == NULL)
            continue;
        char *port = pgpool_get_string(res, i, "port");
        if (port == NULL)
            continue;
        char *role = pgpool_get_string(res, i, "role");
        if (role == NULL)
            continue;

        pgpool_status_append(res, i, "status", &fams[FAM_PGPOOL_NODE_STATUS], labels,
                             &(label_pair_const_t){.name="hostname", .value=hostname},
                             &(label_pair_const_t){.name="port", .value=port},
                             &(label_pair_const_t){.name="role", .value=role}, NULL);

        for (size_t j = 0; j < columns_size ; j++) {
            pgpool_metric_append(res, i, columns[j].name, &fams[columns[j].fam], 1.0, labels,
                                 &(label_pair_const_t){.name="hostname", .value=hostname},
                                 &(label_pair_const_t){.name="port", .value=port},
                                 &(label_pair_const_t){.name="role", .value=role}, NULL);
        }
    }

    PQclear(res);

    return 0;
}

static int pgpool_show_pool_cache(PGconn *conn, unsigned int version,
                                  metric_family_t *fams, label_set_t *labels)
{
    PGresult *res = PQexec(conn, "SHOW POOL_CACHE");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexec failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    int fields = PQnfields(res);
    if (fields < 9) {
        PQclear(res);
        return 0;
    }

    int rows = PQntuples(res);
    if (rows != 1) {
        PQclear(res);
        return 0;
    }

    char *used_cache_entries_size_fix = version < 40300 ? "used_cache_enrties_size"
                                                        : "used_cache_entries_size";

    struct {
        int fam;
        char *name;
    } columns[] = {
        { FAM_PGPOOL_CACHE_HITS,                     "num_cache_hits"              },
        { FAM_PGPOOL_CACHE_SELECTS,                  "num_selects"                 },
        { FAM_PGPOOL_CACHE_HIT_RATIO,                "cache_hit_ratio"             },
        { FAM_PGPOOL_CACHE_HASH_ENTRIES,             "num_hash_entries"            },
        { FAM_PGPOOL_CACHE_USED_HASH_ENTRIES,        "used_hash_entries"           },
        { FAM_PGPOOL_CACHE_ENTRIES,                  "num_cache_entries"           },
        { FAM_PGPOOL_CACHE_ENTRIES_USED_BYTES,       used_cache_entries_size_fix   },
        { FAM_PGPOOL_CACHE_ENTRIES_FREE_BYTES,       "free_cache_entries_size"     },
        { FAM_PGPOOL_CACHE_ENTRIES_FRAGMENTED_BYTES, "fragment_cache_entries_size" },
    };
    size_t columns_size = STATIC_ARRAY_SIZE(columns);

    for (size_t i = 0; i < columns_size ; i++) {
        pgpool_metric_append(res, 0, columns[i].name, &fams[columns[i].fam], 1.0, labels,
                             NULL, NULL);
    }

    PQclear(res);

    return 0;
}

static unsigned int pgpool_show_pool_version(PGconn *conn)
{
    PGresult *res = PQexec(conn, "SHOW POOL_VERSION");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexec failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }

    int fields = PQnfields(res);
    if (fields != 1) {
        PQclear(res);
        return 0;
    }

    int rows = PQntuples(res);
    if (rows != 1) {
        PQclear(res);
        return 0;
    }

    if (PQgetisnull(res, 0, 0)) {
        PQclear(res);
        return 0;
    }

    char buffer[256] = {0};
    sstrncpy(buffer, PQgetvalue(res, 0, 0), sizeof(buffer));
    PQclear(res);

    if (buffer[0] == '\0')
        return 0;

    char *space = strchr(buffer, ' ');
    if (space != NULL)
        *space = '\0';

    int ndots = 0;
    for (int n = 0; buffer[n] != '\0'; n++) {
        if (buffer[n] == '.')
            ndots++;
    }

    unsigned int mult2[] = { 1, 100, 10000};
    unsigned int mult1[] = { 100, 10000};
    unsigned int *mult;
    switch(ndots) {
    case 1:
        mult = mult1;
        break;
    case 2:
        mult = mult2;
        break;
    default:
        return 0;
    }

    unsigned int version = 0;
    char *dot;
    char *start = (char *)buffer;
    int i = 0;
    for (i = 0; ((dot = strrchr(start, '.')) != NULL) && (i < ndots); i++) {
        version += atoi(dot + 1) * mult[i];
        *dot = '\0';
    }

    version += atoi(start) * mult[i];

    return version;
}

static int pgpool_read(user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    pgpool_instance_t *db = ud->data;

    if (pgpool_check_connection(db) != 0) {
        metric_family_append(&db->fams[FAM_PGPOOL_UP], VALUE_GAUGE(0), &db->labels, NULL);
        plugin_dispatch_metric_family(&db->fams[FAM_PGPOOL_UP], 0);
        return 0;
    }

    metric_family_append(&db->fams[FAM_PGPOOL_UP], VALUE_GAUGE(1), &db->labels, NULL);

    cdtime_t submit = cdtime();

    unsigned int version = pgpool_show_pool_version(db->conn);

    pgpool_show_pool_cache(db->conn, version, db->fams, &db->labels);
    pgpool_show_pool_nodes(db->conn, db->fams, &db->labels);
    pgpool_show_pool_backend_stats(db->conn, version, db->fams, &db->labels);
    pgpool_show_pool_health_check_stats(db->conn, version, db->fams, &db->labels);
    pgpool_show_pool_pools(db->conn, db->fams, &db->labels);
    pgpool_show_pool_processes(db->conn, db->fams, &db->labels);

    plugin_dispatch_metric_family_array_filtered(db->fams, FAM_PGPOOL_MAX, db->filter, submit);

    return 0;
}

static int pgpool_config_instance(config_item_t *ci)
{
    pgpool_instance_t *db = calloc(1, sizeof(*db));
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

    memcpy(db->fams, pgpool_fams, sizeof(db->fams[0])*FAM_PGPOOL_MAX);

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
        } else if (strcasecmp(child->key, "ssl-mode") == 0) {
            status = cf_util_get_string(child, &db->sslmode);
        } else if (strcasecmp(child->key, "label") == 0) {
            status = cf_util_get_label(child, &db->labels);
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
        pgpool_instance_delete(db);
        return -1;
    }

    if (db->database == NULL) {
        PLUGIN_ERROR("Instance '%s': No 'database' has been configured.", db->instance);
        pgpool_instance_delete(db);
        return -1;
    }

    label_set_add(&db->labels, true, "instance", db->instance);

    return plugin_register_complex_read("pgpool", db->instance, pgpool_read, interval,
                                        &(user_data_t){.data=db, .free_func=pgpool_instance_delete});
}

static int pgpool_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "instance") == 0) {
            status = pgpool_config_instance(child);
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

void module_register(void)
{
    plugin_register_config("pgpool", pgpool_config);
}
