// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2008-2012 Sebastian Harl
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2024 Manuel Sanmartín
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/complain.h"

#include <libpq-fe.h>
#include <pg_config_manual.h>

enum {
    FAM_PGBOUNCER_UP,
    FAM_PGBOUNCER_DATABASE_POOL_SIZE,
    FAM_PGBOUNCER_DATABASE_RESERVE_POOL,
    FAM_PGBOUNCER_DATABASE_MAX_CONNECTIONS,
    FAM_PGBOUNCER_DATABASE_CURRENT_CONNECTIONS,
    FAM_PGBOUNCER_DATABASE_PAUSED,
    FAM_PGBOUNCER_DATABASE_DISABLED,
    FAM_PGBOUNCER_QUERIES_POOLED,
    FAM_PGBOUNCER_QUERIES_DURATION_SECONDS,
    FAM_PGBOUNCER_RECEIVED_BYTES,
    FAM_PGBOUNCER_QUERIES,
    FAM_PGBOUNCER_SENT_BYTES,
    FAM_PGBOUNCER_CLIENT_WAIT_SECONDS,
    FAM_PGBOUNCER_SQL_TRANSACTIONS_POOLED,
    FAM_PGBOUNCER_SERVER_IN_TRANSACTION_SECONDS,
    FAM_PGBOUNCER_POOL_CLIENT_ACTIVE_CONNECTIONS,
    FAM_PGBOUNCER_POOL_CLIENT_ACTIVE_CANCEL_CONNECTIONS,
    FAM_PGBOUNCER_POOL_CLIENT_WAITING_CONNECTIONS,
    FAM_PGBOUNCER_POOL_CLIENT_WAITING_CANCEL_CONNECTIONS,
    FAM_PGBOUNCER_POOL_SERVER_ACTIVE_CONNECTIONS,
    FAM_PGBOUNCER_POOL_SERVER_ACTIVE_CANCEL_CONNECTIONS,
    FAM_PGBOUNCER_POOL_SERVER_BEING_CANCELED_CONNECTIONS,
    FAM_PGBOUNCER_POOL_SERVER_IDLE_CONNECTIONS,
    FAM_PGBOUNCER_POOL_SERVER_USED_CONNECTIONS,
    FAM_PGBOUNCER_POOL_SERVER_TESTING_CONNECTIONS,
    FAM_PGBOUNCER_POOL_SERVER_LOGIN_CONNECTIONS,
    FAM_PGBOUNCER_POOL_CLIENT_MAXWAIT_SECONDS,
    FAM_PGBOUNCER_DATABASES,
    FAM_PGBOUNCER_USERS,
    FAM_PGBOUNCER_POOLS,
    FAM_PGBOUNCER_FREE_CLIENTS,
    FAM_PGBOUNCER_USED_CLIENTS,
    FAM_PGBOUNCER_LOGIN_CLIENTS,
    FAM_PGBOUNCER_FREE_SERVERS,
    FAM_PGBOUNCER_USED_SERVERS,
    FAM_PGBOUNCER_CACHED_DNS_NAMES,
    FAM_PGBOUNCER_CACHED_DNS_ZONES,
    FAM_PGBOUNCER_IN_FLIGHT_DNS_QUERIES,
    FAM_PGBOUNCER_MAX,
};

static metric_family_t pgbouncer_fams[FAM_PGBOUNCER_MAX] = {
    [FAM_PGBOUNCER_UP] = {
        .name = "pgbouncer_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "",
    },
    [FAM_PGBOUNCER_DATABASE_POOL_SIZE] = {
        .name = "pgbouncer_database_pool_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum number of server connections."
    },
    [FAM_PGBOUNCER_DATABASE_RESERVE_POOL] = {
        .name = "pgbouncer_database_reserve_pool",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum number of additional connections for this database.",
    },
    [FAM_PGBOUNCER_DATABASE_MAX_CONNECTIONS] = {
        .name = "pgbouncer_database_max_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum number of allowed connections for this database.",
    },
    [FAM_PGBOUNCER_DATABASE_CURRENT_CONNECTIONS] = {
        .name = "pgbouncer_database_current_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of connections for this database.",
    },
    [FAM_PGBOUNCER_DATABASE_PAUSED] = {
        .name = "pgbouncer_database_paused",
        .type = METRIC_TYPE_GAUGE,
        .help = "1 if this database is currently paused, else 0."
    },
    [FAM_PGBOUNCER_DATABASE_DISABLED] = {
        .name = "pgbouncer_database_disabled",
        .type = METRIC_TYPE_GAUGE,
        .help = "1 if this database is currently disabled, else 0."
    },
    [FAM_PGBOUNCER_QUERIES_POOLED] = {
        .name = "pgbouncer_queries_pooled",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of SQL queries pooled.",
    },
    [FAM_PGBOUNCER_QUERIES_DURATION_SECONDS] = {
        .name = "pgbouncer_queries_duration_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of seconds spent by pgbouncer when "
                "actively connected to PostgreSQL, executing queries.",
    },
    [FAM_PGBOUNCER_RECEIVED_BYTES] = {
        .name = "pgbouncer_received_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total volume in bytes of network traffic received by pgbouncer, shown as bytes.",
    },
    [FAM_PGBOUNCER_QUERIES] = {
        .name = "pgbouncer_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of SQL requests pooled by pgbouncer, shown as requests.",
    },
    [FAM_PGBOUNCER_SENT_BYTES] = {
        .name = "pgbouncer_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total volume in bytes of network traffic sent by pgbouncer, shown as bytes.",
    },
    [FAM_PGBOUNCER_CLIENT_WAIT_SECONDS] = {
        .name = "pgbouncer_client_wait_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time spent by clients waiting for a server in seconds.",
    },
    [FAM_PGBOUNCER_SQL_TRANSACTIONS_POOLED] = {
        .name = "pgbouncer_sql_transactions_pooled",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of SQL transactions pooled.",
    },
    [FAM_PGBOUNCER_SERVER_IN_TRANSACTION_SECONDS] = {
        .name = "pgbouncer_server_in_transaction_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of seconds spent by pgbouncer when connected to PostgreSQL "
                "in a transaction, either idle in transaction or executing queries.",
    },
    [FAM_PGBOUNCER_POOL_CLIENT_ACTIVE_CONNECTIONS] = {
        .name = "pgbouncer_pool_client_active_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Client connections linked to server connection and able to process queries, "
                "shown as connection.",
    },
    [FAM_PGBOUNCER_POOL_CLIENT_ACTIVE_CANCEL_CONNECTIONS] = {
        .name = "pgbouncer_pool_client_active_cancel_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Client connections that have forwarded query cancellations to the server "
                "and are waiting for the server response.",
    },
    [FAM_PGBOUNCER_POOL_CLIENT_WAITING_CONNECTIONS] = {
        .name = "pgbouncer_pool_client_waiting_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Client connections waiting on a server connection, shown as connection.",
    },
    [FAM_PGBOUNCER_POOL_CLIENT_WAITING_CANCEL_CONNECTIONS] = {
        .name = "pgbouncer_pool_client_waiting_cancel_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Client connections that have not forwarded query cancellations to the server yet.",
    },
    [FAM_PGBOUNCER_POOL_SERVER_ACTIVE_CONNECTIONS] = {
        .name = "pgbouncer_pool_server_active_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Server connections linked to a client connection, shown as connection.",
    },
    [FAM_PGBOUNCER_POOL_SERVER_ACTIVE_CANCEL_CONNECTIONS] = {
        .name = "pgbouncer_pool_server_active_cancel_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Server connections that are currently forwarding a cancel request.",
    },
    [FAM_PGBOUNCER_POOL_SERVER_BEING_CANCELED_CONNECTIONS] = {
        .name = "pgbouncer_pool_server_being_canceled_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Servers that normally could become idle but are waiting to do so "
                "until all in-flight cancel requests have completed that were sent "
                "to cancel a query on this server.",
    },
    [FAM_PGBOUNCER_POOL_SERVER_IDLE_CONNECTIONS] = {
        .name = "pgbouncer_pool_server_idle_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Server connections idle and ready for a client query, shown as connection.",
    },
    [FAM_PGBOUNCER_POOL_SERVER_USED_CONNECTIONS] = {
        .name = "pgbouncer_pool_server_used_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Server connections idle more than server_check_delay, "
                "needing server_check_query, shown as connection.",
    },
    [FAM_PGBOUNCER_POOL_SERVER_TESTING_CONNECTIONS] = {
        .name = "pgbouncer_pool_server_testing_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Server connections currently running either server_reset_query "
                "or server_check_query, shown as connection",
    },
    [FAM_PGBOUNCER_POOL_SERVER_LOGIN_CONNECTIONS] = {
        .name = "pgbouncer_pool_server_login_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Server connections currently in the process of logging in, shown as connection.",
    },
    [FAM_PGBOUNCER_POOL_CLIENT_MAXWAIT_SECONDS] = {
        .name = "pgbouncer_pool_client_maxwait_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Age of oldest unserved client connection, shown as second.",
    },
    [FAM_PGBOUNCER_DATABASES] = {
        .name = "pgbouncer_databases",
        .type = METRIC_TYPE_GAUGE,
        .help = "Count of databases.",
    },
    [FAM_PGBOUNCER_USERS] = {
        .name = "pgbouncer_users",
        .type = METRIC_TYPE_GAUGE,
        .help = "Count of users.",
    },
    [FAM_PGBOUNCER_POOLS] = {
        .name = "pgbouncer_pools",
        .type = METRIC_TYPE_GAUGE,
        .help = "Count of pools.",
    },
    [FAM_PGBOUNCER_FREE_CLIENTS] = {
        .name = "pgbouncer_free_clients",
        .type = METRIC_TYPE_GAUGE,
        .help = "Count of free clients.",
    },
    [FAM_PGBOUNCER_USED_CLIENTS] = {
        .name = "pgbouncer_used_clients",
        .type = METRIC_TYPE_GAUGE,
        .help = "Count of used clients.",
    },
    [FAM_PGBOUNCER_LOGIN_CLIENTS] = {
        .name = "pgbouncer_login_clients",
        .type = METRIC_TYPE_GAUGE,
        .help = "Count of clients in login state.",
    },
    [FAM_PGBOUNCER_FREE_SERVERS] = {
        .name = "pgbouncer_free_servers",
        .type = METRIC_TYPE_GAUGE,
        .help = "Count of free servers.",
    },
    [FAM_PGBOUNCER_USED_SERVERS] = {
        .name = "pgbouncer_used_servers",
        .type = METRIC_TYPE_GAUGE,
        .help = "Count of used servers.",
    },
    [FAM_PGBOUNCER_CACHED_DNS_NAMES] = {
        .name = "pgbouncer_cached_dns_names",
        .type = METRIC_TYPE_GAUGE,
        .help = "Count of DNS names in the cache.",
    },
    [FAM_PGBOUNCER_CACHED_DNS_ZONES] = {
        .name = "pgbouncer_cached_dns_zones",
        .type = METRIC_TYPE_GAUGE,
        .help = "Count of DNS zones in the cache.",
    },
    [FAM_PGBOUNCER_IN_FLIGHT_DNS_QUERIES] = {
        .name = "pgbouncer_in_flight_dns_queries",
        .type = METRIC_TYPE_GAUGE,
        .help = "Count of in-flight DNS queries.",
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
    metric_family_t fams[FAM_PGBOUNCER_MAX];
} pgb_instance_t;

static void pgb_instance_delete(void *data)
{
    pgb_instance_t *db = data;
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

static int pgb_connect(pgb_instance_t *db)
{
    if ((!db) || (!db->database))
        return -1;

    char conninfo[4096];
    strbuf_t buf = STRBUF_CREATE_STATIC(conninfo);
    struct {
        char *param;
        char *value;
    } pgb_params[] = {
        { "dbname",           db->database          },
        { "host",             db->host              },
        { "port",             db->port              },
        { "user",             db->user              },
        { "password",         db->password          },
        { "sslmode",          db->sslmode           },
        { "application_name", "ncollectd_pgbouncer" }
    };
    size_t pgb_params_size = STATIC_ARRAY_SIZE(pgb_params);

    for (size_t i = 0; i  < pgb_params_size; i++) {
        if ((pgb_params[i].value != NULL) && (pgb_params[i].value[0] != '\0')) {
            strbuf_putstr(&buf, pgb_params[i].param);
            strbuf_putstr(&buf, " = '");
            strbuf_putstr(&buf, pgb_params[i].value);
            strbuf_putstr(&buf, "' ");
        }
    }

    db->conn = PQconnectdb(conninfo);
    return 0;
}

static int pgb_check_connection(pgb_instance_t *db)
{
    bool init = false;

    if (!db->conn) {
        init = true;

        /* trigger c_release() */
        if (db->conn_complaint.interval == 0)
            db->conn_complaint.interval = 1;

        pgb_connect(db);
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

static char *pgb_get_string(const PGresult *res, int row, const char *column_name)
{
    int col = PQfnumber(res, column_name);
    if (col < 0)
        return NULL;

    if (PQgetisnull(res, row, col))
        return NULL;

    return PQgetvalue(res, row, col);
}

__attribute__ ((sentinel(0)))
static int pgb_metric_append(const PGresult *res, int row, const char *column_name,
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

static int pgb_show_databases(PGconn *conn, metric_family_t *fams, label_set_t *labels)
{
    PGresult *res = PQexec(conn, "SHOW DATABASES");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexec failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    struct {
        int fam;
        char *name;
        double scale;
    } columns[] = {
        { FAM_PGBOUNCER_DATABASE_POOL_SIZE,           "pool_size",           1.0 },
        { FAM_PGBOUNCER_DATABASE_RESERVE_POOL,        "reserve_pool",        1.0 },
        { FAM_PGBOUNCER_DATABASE_MAX_CONNECTIONS,     "max_connections",     1.0 },
        { FAM_PGBOUNCER_DATABASE_CURRENT_CONNECTIONS, "current_connections", 1.0 },
        { FAM_PGBOUNCER_DATABASE_PAUSED,              "paused",              1.0 },
        { FAM_PGBOUNCER_DATABASE_DISABLED,            "disabled",            1.0 },
    };
    size_t columns_size = STATIC_ARRAY_SIZE(columns);

    for (int i = 0; i < PQntuples(res); i++) {
        char *name = pgb_get_string(res, i, "name");
        if (name == NULL)
            continue;
        char *database = pgb_get_string(res, i, "database");
        if (database == NULL)
            continue;
        char *host = pgb_get_string(res, i, "host");
        char *port = pgb_get_string(res, i, "port");

        for (size_t j = 0; j < columns_size ; j++) {
            pgb_metric_append(res, i, columns[j].name, &fams[columns[j].fam],
                              columns[j].scale, labels,
                              &(label_pair_const_t){.name="name", .value=name},
                              &(label_pair_const_t){.name="host", .value=host},
                              &(label_pair_const_t){.name="port", .value=port},
                              &(label_pair_const_t){.name="database", .value=database}, NULL);
        }
    }

    PQclear(res);

    return 0;
}

static int pgb_show_stats(PGconn *conn, metric_family_t *fams, label_set_t *labels)
{
    PGresult *res = PQexec(conn, "SHOW STATS");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexec failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    struct {
        int fam;
        char *name;
        double scale;
    } columns[] = {
        { FAM_PGBOUNCER_QUERIES_POOLED,                "total_query_count",             1.0  },
        { FAM_PGBOUNCER_QUERIES_DURATION_SECONDS,      "total_query_time",              1e-6 },
        { FAM_PGBOUNCER_RECEIVED_BYTES,                "total_received",                1.0  },
        { FAM_PGBOUNCER_QUERIES,                       "total_requests",                1.0  },
        { FAM_PGBOUNCER_SENT_BYTES,                    "total_sent",                    1.0  },
        { FAM_PGBOUNCER_CLIENT_WAIT_SECONDS,           "total_wait_time",               1e-6 },
        { FAM_PGBOUNCER_SQL_TRANSACTIONS_POOLED,       "total_xact_count",              1.0  },
        { FAM_PGBOUNCER_SERVER_IN_TRANSACTION_SECONDS, "total_xact_time",               1e-6 },
// "total_server_assignment_count", 1.0  },
    };
    size_t columns_size = STATIC_ARRAY_SIZE(columns);

    for (int i = 0; i < PQntuples(res); i++) {
        char *database = pgb_get_string(res, i, "database");
        if (database == NULL)
            continue;

        for (size_t j = 0; j < columns_size ; j++) {
            pgb_metric_append(res, i, columns[j].name, &fams[columns[j].fam],
                              columns[j].scale, labels,
                              &(label_pair_const_t){.name="database", .value=database}, NULL);
        }
    }

    PQclear(res);

    return 0;
}

static int pgb_show_pools(PGconn *conn, metric_family_t *fams, label_set_t *labels)
{
    PGresult *res = PQexec(conn, "SHOW POOLS");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexec failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    struct {
        int fam;
        char *name;
        double scale;
    } columns[] = {
        { FAM_PGBOUNCER_POOL_CLIENT_ACTIVE_CONNECTIONS,         "cl_active",             1.0 },
        { FAM_PGBOUNCER_POOL_CLIENT_ACTIVE_CANCEL_CONNECTIONS,  "cl_active_cancel_req",  1.0 },
        { FAM_PGBOUNCER_POOL_CLIENT_WAITING_CONNECTIONS,        "cl_waiting",            1.0 },
        { FAM_PGBOUNCER_POOL_CLIENT_WAITING_CANCEL_CONNECTIONS, "cl_waiting_cancel_req", 1.0 },
        { FAM_PGBOUNCER_POOL_SERVER_ACTIVE_CONNECTIONS,         "sv_active",             1.0 },
        { FAM_PGBOUNCER_POOL_SERVER_ACTIVE_CANCEL_CONNECTIONS,  "sv_active_cancel",      1.0 },
        { FAM_PGBOUNCER_POOL_SERVER_BEING_CANCELED_CONNECTIONS, "sv_being_canceled",     1.0 },
        { FAM_PGBOUNCER_POOL_SERVER_IDLE_CONNECTIONS,           "sv_idle",               1.0 },
        { FAM_PGBOUNCER_POOL_SERVER_USED_CONNECTIONS,           "sv_used",               1.0 },
        { FAM_PGBOUNCER_POOL_SERVER_TESTING_CONNECTIONS,        "sv_tested",             1.0 },
        { FAM_PGBOUNCER_POOL_SERVER_LOGIN_CONNECTIONS,          "sv_login",              1.0 },
    };
    size_t columns_size = STATIC_ARRAY_SIZE(columns);

    for (int i = 0; i < PQntuples(res); i++) {
        char *database = pgb_get_string(res, i, "database");
        if (database == NULL)
            continue;
        char *user = pgb_get_string(res, i, "user");
        if (user == NULL)
            continue;

        for (size_t j = 0; j < columns_size ; j++) {
            pgb_metric_append(res, i, columns[j].name, &fams[columns[j].fam],
                              columns[j].scale, labels,
                              &(label_pair_const_t){.name="database", .value=database},
                              &(label_pair_const_t){.name="user",     .value=user    }, NULL);
        }

        char *maxwait = pgb_get_string(res, i, "maxwait");
        char *maxwait_us = pgb_get_string(res, i, "maxwait_us");
        if ((maxwait == NULL) || (maxwait_us == NULL))
            continue;

        double dmaxwait = atof(maxwait);
        double dmaxwait_us = atof(maxwait_us);

        metric_family_append(&fams[FAM_PGBOUNCER_POOL_CLIENT_MAXWAIT_SECONDS],
                             VALUE_GAUGE(dmaxwait + dmaxwait_us * 1e-6), labels,
                            &(label_pair_const_t){.name="database", .value=database},
                            &(label_pair_const_t){.name="user",     .value=user    }, NULL);
    }

    PQclear(res);

    return 0;
}

static int pgb_show_lists(PGconn *conn, metric_family_t *fams, label_set_t *labels)
{
    PGresult *res = PQexec(conn, "SHOW LISTS");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexec failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    struct {
        int fam;
        char *name;
    } rows[] = {
        { FAM_PGBOUNCER_DATABASES,             "databases"     },
        { FAM_PGBOUNCER_USERS,                 "users"         },
        { FAM_PGBOUNCER_POOLS,                 "pools"         },
        { FAM_PGBOUNCER_FREE_CLIENTS,          "free_clients"  },
        { FAM_PGBOUNCER_USED_CLIENTS,          "used_clients"  },
        { FAM_PGBOUNCER_LOGIN_CLIENTS,         "login_clients" },
        { FAM_PGBOUNCER_FREE_SERVERS,          "free_servers"  },
        { FAM_PGBOUNCER_USED_SERVERS,          "used_servers"  },
        { FAM_PGBOUNCER_CACHED_DNS_NAMES,      "dns_names"     },
        { FAM_PGBOUNCER_CACHED_DNS_ZONES,      "dns_zones"     },
        { FAM_PGBOUNCER_IN_FLIGHT_DNS_QUERIES, "dns_queries"   },
    };
    size_t rows_size = STATIC_ARRAY_SIZE(rows);

    int fields = PQnfields(res);
    if (fields != 2) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        if (PQgetisnull(res, i, 1))
            continue;

        char *list = PQgetvalue(res, i, 0);
        char *item = PQgetvalue(res, i, 1);

        for (size_t j = 0; j < rows_size ; j++) {
            if (strcmp(list, rows[j].name) == 0) {
                metric_family_t *fam = &fams[rows[j].fam];

                switch(fam->type) {
                case METRIC_TYPE_COUNTER:
                    metric_family_append(fam, VALUE_COUNTER(atoi(item)), labels, NULL);
                    break;
                case METRIC_TYPE_GAUGE:
                    metric_family_append(fam, VALUE_GAUGE(atof(item)), labels, NULL);
                    break;
                default:
                    break;
                }

                break;
            }
        }
    }

    PQclear(res);

    return 0;
}

static int pgb_read(user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    pgb_instance_t *db = ud->data;

    if (pgb_check_connection(db) != 0) {
        metric_family_append(&db->fams[FAM_PGBOUNCER_UP], VALUE_GAUGE(0), &db->labels, NULL);
        plugin_dispatch_metric_family(&db->fams[FAM_PGBOUNCER_UP], 0);
        return 0;
    }

    metric_family_append(&db->fams[FAM_PGBOUNCER_UP], VALUE_GAUGE(1), &db->labels, NULL);

    cdtime_t submit = cdtime();

    pgb_show_databases(db->conn, db->fams, &db->labels);
    pgb_show_stats(db->conn, db->fams, &db->labels);
    pgb_show_pools(db->conn, db->fams, &db->labels);
    pgb_show_lists(db->conn, db->fams, &db->labels);

    plugin_dispatch_metric_family_array_filtered(db->fams, FAM_PGBOUNCER_MAX, db->filter, submit);

    return 0;
}

static int pgb_config_instance(config_item_t *ci)
{
    pgb_instance_t *db = calloc(1, sizeof(*db));
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

    memcpy(db->fams, pgbouncer_fams, sizeof(db->fams[0])*FAM_PGBOUNCER_MAX);

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
        } else if (strcasecmp(child->key, "password") == 0) {
            status = cf_util_get_string(child, &db->password);
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
        pgb_instance_delete(db);
        return -1;
    }

    if (db->database == NULL) {
        db->database = strdup("pgbouncer");
        if (db->database == NULL) {
            PLUGIN_ERROR("strdup failed.");
            pgb_instance_delete(db);
            return -1;
        }
    }

    label_set_add(&db->labels, true, "instance", db->instance);

    return plugin_register_complex_read("pgbouncer", db->instance, pgb_read, interval,
                                        &(user_data_t){.data=db, .free_func=pgb_instance_delete});
}

static int pgb_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "instance") == 0) {
            status = pgb_config_instance(child);
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
    plugin_register_config("pgbouncer", pgb_config);
}
