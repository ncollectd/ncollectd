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
#include "libdbquery/dbquery.h"

#ifdef HAVE_MYSQL_H
#include <mysql.h>
#elif defined(HAVE_MYSQL_MYSQL_H)
#include <mysql/mysql.h>
#endif

#include "mysql_fam.h"
#include "mysql_flags.h"

static cf_flags_t cmysql_flags[] = {
    { "globals",             COLLECT_GLOBALS           },
    { "acl",                 COLLECT_ACL               },
    { "aria",                COLLECT_ARIA              },
    { "binlog",              COLLECT_BINLOG            },
    { "commands",            COLLECT_COMMANDS          },
    { "features",            COLLECT_FEATURES          },
    { "handlers",            COLLECT_HANDLERS          },
    { "innodb",              COLLECT_INNODB            },
    { "innodb_cmp",          COLLECT_INNODB_CMP        },
    { "innodb_cmpmem",       COLLECT_INNODB_CMPMEM     },
    { "innodb_tablespace",   COLLECT_INNODB_TABLESPACE },
    { "myisam",              COLLECT_MYISAM            },
    { "perfomance_lost",     COLLECT_PERF_LOST         },
    { "qcache",              COLLECT_QCACHE            },
    { "slave",               COLLECT_SLAVE             },
    { "ssl",                 COLLECT_SSL               },
    { "wsrep",               COLLECT_WSREP             },
    { "client",              COLLECT_CLIENT_STATS      },
    { "user",                COLLECT_USER_STATS        },
    { "index",               COLLECT_INDEX_STATS       },
    { "table",               COLLECT_TABLE_STATS       },
    { "response_time",       COLLECT_RESPONSE_TIME     },
    { "master",              COLLECT_MASTER_STATS      },
    { "slave",               COLLECT_SLAVE_STATS       },
    { "heartbeat",           COLLECT_HEARTBEAT         },
};
static size_t cmysql_flags_size = STATIC_ARRAY_SIZE(cmysql_flags);

extern metric_family_t fam_mysql_status[FAM_MYSQL_STATUS_MAX];

struct cmysql_innodb {
    char *key;
    int fam;
    char *lname;
    char *lvalue;
};

const struct cmysql_innodb *cmysql_innodb_get_key (register const char *str, register size_t len);

struct cmysql_status {
    char *key;
    cmysql_flag_t flag;
    int fam;
    char *lname;
    char *lvalue;
};

const struct cmysql_status *cmysql_status_get_key (register const char *str, register size_t len);

typedef enum {
    SERVER_MARIADB,
    SERVER_PERCONA,
    SERVER_MYSQL
} cmysql_server_t;

typedef struct {
    char *instance;
    char *host;
    char *user;
    char *pass;
    char *database;

    /* mysql_ssl_set params */
    char *key;
    char *cert;
    char *ca;
    char *capath;
    char *cipher;

    char *socket;
    int port;
    int timeout;

    c_complain_t conn_complaint;

    char *metric_prefix;
    label_set_t labels;
    plugin_filter_t *filter;
    uint64_t flags;

    bool heartbeat_utc;
    char *heartbeat_schema;
    char *heartbeat_table;

    db_query_preparation_area_t **q_prep_areas;
    db_query_t **queries;
    size_t queries_num;

    bool primary_stats;
    bool replica_stats;

    bool replica_notif;
    bool replica_io_running;
    bool replica_sql_running;

    MYSQL *con;
    bool is_connected;
    unsigned long mysql_version;
    cmysql_server_t mysql_server;

    metric_family_t fams[FAM_MYSQL_STATUS_MAX];

} cmysql_database_t;

static db_query_t **queries;
static size_t queries_num;

static void cmysql_database_free(void *arg)
{
    cmysql_database_t *db = arg;

    if (db == NULL)
        return;

    if (db->con != NULL)
        mysql_close(db->con);

    free(db->host);
    free(db->user);
    free(db->pass);
    free(db->socket);
    free(db->instance);
    free(db->database);
    free(db->key);
    free(db->cert);
    free(db->ca);
    free(db->capath);
    free(db->cipher);

    free(db->metric_prefix);
    label_set_reset(&db->labels);
    plugin_filter_free(db->filter);
    free(db->heartbeat_schema);
    free(db->heartbeat_table);

    if (db->q_prep_areas) {
        for (size_t i = 0; i < db->queries_num; ++i)
            db_query_delete_preparation_area(db->q_prep_areas[i]);
    }
    free(db->q_prep_areas);
    /* N.B.: db->queries references objects "owned" by the global queries
     * variable. Free the array here, but not the content. */
    free(db->queries);

    free(db);
}

static MYSQL *cmysql_get_connection(cmysql_database_t *db)
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
        PLUGIN_ERROR("mysql_init failed: %s", mysql_error(db->con));
        return NULL;
    }

    /* Configure TCP connect timeout (default: 0) */
    db->con->options.connect_timeout = db->timeout;

    mysql_ssl_set(db->con, db->key, db->cert, db->ca, db->capath, db->cipher);

    MYSQL *con = mysql_real_connect(db->con, db->host, db->user, db->pass, db->database,
                                    db->port, db->socket, 0);
    if (con == NULL) {
        c_complain(LOG_ERR, &db->conn_complaint,
                   "Failed to connect to database %s at server %s: %s",
                   (db->database != NULL) ? db->database : "<none>",
                   (db->host != NULL) ? db->host : "localhost", mysql_error(db->con));
        return NULL;
    }

    cipher = mysql_get_ssl_cipher(db->con);

    db->mysql_version = mysql_get_server_version(db->con);

    const char *info = mysql_get_server_info(db->con);
    if (info != NULL) {
        if (strstr(info, "MariaDB") != NULL) {
            db->mysql_server = SERVER_MARIADB;
        } else if (strstr(info, "Percona") != NULL) {
            db->mysql_server = SERVER_PERCONA;
        } else {
            db->mysql_server = SERVER_MYSQL;
        }
    } else {
        db->mysql_server = SERVER_MARIADB;
    }

    c_do_release(LOG_INFO, &db->conn_complaint,
                 "Successfully connected to database %s at server %s with cipher %s "
                 "(server version: %s, protocol version: %u) ",
                 (db->database != NULL) ? db->database : "<none>",
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

static int cmysql_read_primary_stats(__attribute__((unused)) cmysql_database_t *db, MYSQL *con)
{
    const char *query = "SHOW MASTER STATUS";

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL) {
        PLUGIN_ERROR("Failed to get primary statistics: '%s' did not return any rows.", query);
        mysql_free_result(res);
        return -1;
    }

    int field_num = mysql_num_fields(res);
    if (field_num < 2) {
        PLUGIN_ERROR("Failed to get primary statistics: '%s' returned less than two columns.",
                    query);
        mysql_free_result(res);
        return -1;
    }

//    unsigned long long position = atoll(row[1]);
//    derive_submit("mysql_log_position", "master-bin", position, db);

    row = mysql_fetch_row(res);
    if (row != NULL)
        PLUGIN_WARNING("`%s' returned more than one row ignoring further results.", query);

    mysql_free_result(res);

    return 0;
}

static int cmysql_read_replica_stats(cmysql_database_t *db, MYSQL *con)
{
#if 0
    /* WTF? libmysqlclient does not seem to provide any means to
     * translate a column name to a column index ... :-/ */
    const int READ_MASTER_LOG_POS_IDX = 6;
    const int SLAVE_IO_RUNNING_IDX = 10;
    const int SLAVE_SQL_RUNNING_IDX = 11;
    const int EXEC_MASTER_LOG_POS_IDX = 21;
    const int SECONDS_BEHIND_MASTER_IDX = 32;
#endif
    const char *query = "SHOW SLAVE STATUS";

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL) {
        PLUGIN_ERROR("Failed to get replica statistics: '%s' did not return any rows.", query);
        mysql_free_result(res);
        return -1;
    }

    int field_num = mysql_num_fields(res);
    if (field_num < 33) {
        PLUGIN_ERROR("Failed to get replica statistics: '%s' returned less than 33 columns.", query);
        mysql_free_result(res);
        return -1;
    }

    if (db->replica_stats) {
#if 0
        unsigned long long counter;
        double gauge;

        gauge_submit("bool", "slave-sql-running", (row[SLAVE_SQL_RUNNING_IDX] != NULL) && (strcasecmp(row[SLAVE_SQL_RUNNING_IDX], "yes") == 0), db);

        gauge_submit("bool", "slave-io-running", (row[SLAVE_IO_RUNNING_IDX] != NULL) && (strcasecmp(row[SLAVE_IO_RUNNING_IDX], "yes") == 0), db);

        counter = atoll(row[READ_MASTER_LOG_POS_IDX]);
        derive_submit("mysql_log_position", "slave-read", counter, db);

        counter = atoll(row[EXEC_MASTER_LOG_POS_IDX]);
        derive_submit("mysql_log_position", "slave-exec", counter, db);

        if (row[SECONDS_BEHIND_MASTER_IDX] != NULL) {
            gauge = atof(row[SECONDS_BEHIND_MASTER_IDX]);
            gauge_submit("time_offset", NULL, gauge, db);
        }
#endif
    }

    if (db->replica_notif) {
#if 0
        notification_t n = {0,  cdtime(), "", "", "mysql", "", "time_offset", "", NULL};

        char *io, *sql;

        io = row[SLAVE_IO_RUNNING_IDX];
        sql = row[SLAVE_SQL_RUNNING_IDX];

        set_host(db, n.host, sizeof(n.host));

        /* Assured by "mysql_config_database" */
        assert(db->instance != NULL);
        sstrncpy(n.plugin_instance, db->instance, sizeof(n.plugin_instance));

        if (((io == NULL) || (strcasecmp(io, "yes") != 0)) && (db->replica_io_running)) {
            n.severity = NOTIF_WARNING;
            ssnprintf(n.message, sizeof(n.message), "replica I/O thread not started or not connected to primary");
            plugin_dispatch_notification(&n);
            db->replica_io_running = false;
        } else if (((io != NULL) && (strcasecmp(io, "yes") == 0)) && (!db->replica_io_running)) {
            n.severity = NOTIF_OKAY;
            ssnprintf(n.message, sizeof(n.message), "replica I/O thread started and connected to primary");
            plugin_dispatch_notification(&n);
            db->replica_io_running = true;
        }

        if (((sql == NULL) || (strcasecmp(sql, "yes") != 0)) && (db->replica_sql_running)) {
            n.severity = NOTIF_WARNING;
            ssnprintf(n.message, sizeof(n.message), "replica SQL thread not started");
            plugin_dispatch_notification(&n);
            db->replica_sql_running = false;
        } else if (((sql != NULL) && (strcasecmp(sql, "yes") == 0)) && (!db->replica_sql_running)) {
            n.severity = NOTIF_OKAY;
            ssnprintf(n.message, sizeof(n.message), "replica SQL thread started");
            plugin_dispatch_notification(&n);
            db->replica_sql_running = true;
        }
#endif
    }

    row = mysql_fetch_row(res);
    if (row != NULL)
        PLUGIN_WARNING("`%s' returned more than one row - ignoring further results.", query);

    mysql_free_result(res);

    return 0;
}

static int cmysql_read_heartbeat(cmysql_database_t *db, MYSQL *con)
{
    char query[512];
    ssnprintf(query, sizeof(query),
              "SELECT MAX(UNIX_TIMESTAMP(%s) - UNIX_TIMESTAMP(ts)) AS delay, server_id"
              "  FROM %s.%s GROUP BY server_id",
              db->heartbeat_utc ? "UTC_TIMESTAMP(6)" : "NOW(6)",
              db->heartbeat_schema != NULL ? db->heartbeat_schema : "heartbeat",
              db->heartbeat_table != NULL ? db->heartbeat_table : "heartbeat");

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    if (mysql_num_fields(res) != 2) {
        mysql_free_result(res);
        return -1;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        if ((row[0] == NULL) || (row[1] == NULL))
            continue;

        metric_family_append(&db->fams[FAM_MYSQL_HEARTBEAT_DELAY_SECONDS],
                            VALUE_GAUGE(atof(row[0])), &db->labels,
                            &(label_pair_const_t){.name = "server_id", .value = row[1]},
                            NULL);
    }

    mysql_free_result(res);
    return 0;
}

static int cmysql_read_query_response_time(cmysql_database_t *db, MYSQL *con,
                                           const char *query, int fam)
{
    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    if (mysql_num_fields(res) != 3) {
        mysql_free_result(res);
        return -1;
    }

    histogram_t *h = histogram_new();
    double sum = 0.0;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        if ((row[0] == NULL) || (row[1] == NULL) || (row[2] == NULL))
            continue;

        if (strncmp("TOO LONG", row[0], strlen("TOO LONG")) == 0)
            continue;

        double maximum = atof(row[0]);
        uint64_t counter = atoll(row[1]);
        sum += atof(row[2]);

        histogram_t *tmp = histogram_bucket_append(h, maximum, counter);
        if (tmp == NULL) {
            histogram_destroy(h);
            h = NULL;
            break;
        }
        h = tmp;
    }

    if (h != NULL) {
        h->sum = sum;
        metric_family_append(&db->fams[fam], VALUE_HISTOGRAM(h),  &db->labels, NULL);
        histogram_destroy(h);
    }

    mysql_free_result(res);
    return 0;
}

static int cmysql_read_query_response_time_all(cmysql_database_t *db, MYSQL *con)
{
    const char *query = "SELECT time, count, total"
                        " FROM information_schema.query_response_time";

    return cmysql_read_query_response_time(db, con,
                                           query, FAM_MYSQL_QUERY_RESPONSE_TIME_SECONDS);
}

//  PERCONA Percona Server for MySQL 5.7.10-1: Feature ported from Percona Server for MySQL 5.6
static int cmysql_read_query_response_time_read(cmysql_database_t *db, MYSQL *con)
{
    const char *query = "SELECT time, count, total"
                        "  FROM information_schema.query_response_time_read";

    return cmysql_read_query_response_time(db, con,
                                           query, FAM_MYSQL_READ_QUERY_RESPONSE_TIME_SECONDS);
}

//  PERCONA Percona Server for MySQL 5.7.10-1: Feature ported from Percona Server for MySQL 5.6
static int cmysql_read_query_response_time_write(cmysql_database_t *db, MYSQL *con)
{
    const char *query = "SELECT time, count, total"
                        "  FROM information_schema.query_response_time_write";

    return cmysql_read_query_response_time(db, con,
                                           query, FAM_MYSQL_WRITE_QUERY_RESPONSE_TIME_SECONDS);
}

static int cmysql_read_table(cmysql_database_t *db, MYSQL *con)
{
    const char *query = "SELECT table_schema, table_name, data_length, index_length, data_free"
                        "  FROM information_schema.tables"
                        " WHERE table_schema NOT IN ('mysql', 'performance_schema',"
                        "                            'information_schema')";

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    if (mysql_num_fields(res) != 5) {
        mysql_free_result(res);
        return -1;
    }

    static struct {
        int field;
        int fam;
    } fields[] = {
        { 2, FAM_MYSQL_TABLE_DATA_SIZE_BYTES  },
        { 3, FAM_MYSQL_TABLE_INDEX_SIZE_BYTES },
        { 4, FAM_MYSQL_TABLE_DATA_FREE_BYTES  },
    };
    static size_t fields_size = STATIC_ARRAY_SIZE(fields);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        if ((row[0] == NULL) || (row[1] == NULL))
            continue;

        for (size_t n = 0; n < fields_size ; n++) {
            value_t value = {0};

            if (row[fields[n].field] == NULL)
                continue;

            if (db->fams[fields[n].fam].type == METRIC_TYPE_GAUGE) {
                value = VALUE_GAUGE(atof(row[fields[n].field]));
            } else if (db->fams[fields[n].fam].type == METRIC_TYPE_COUNTER) {
                value = VALUE_COUNTER(atoll(row[fields[n].field]));
            } else {
                continue;
            }

            metric_family_append(&db->fams[fields[n].fam], value, &db->labels,
                                 &(label_pair_const_t){.name = "table_schema", .value = row[0]},
                                 &(label_pair_const_t){.name = "table_name", .value = row[1]},
                                 NULL);
        }
    }

    mysql_free_result(res);
    return 0;
}

static int cmysql_read_client_statistics(cmysql_database_t *db, MYSQL *con)
{
    if (db->mysql_server != SERVER_MARIADB)
        return 0;
    if (db->mysql_version < 100101)
        return 0;

    const char *query = "SELECT client, total_connections, concurrent_connections, connected_time,"
                        "       busy_time, cpu_time, bytes_received, bytes_sent,"
                        "       binlog_bytes_written, rows_read, rows_sent, rows_deleted,"
                        "       rows_inserted, rows_updated, select_commands, update_commands,"
                        "       other_commands, commit_transactions, rollback_transactions,"
                        "       denied_connections, lost_connections, access_denied,"
                        "       empty_queries, total_ssl_connections, max_statement_time_exceeded"
                        "  FROM information_schema.client_statistics";

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    if (mysql_num_fields(res) != 25) {
        mysql_free_result(res);
        return -1;
    }

    static struct {
        int field;
        int fam;
    } fields[] = {
        { 1,  FAM_MYSQL_CLIENT_CONNECTIONS                 },
        { 2,  FAM_MYSQL_CLIENT_CONCURRENT_CONNECTIONS      },
        { 3,  FAM_MYSQL_CLIENT_CONNECTED_TIME_SECONDS      },
        { 4,  FAM_MYSQL_CLIENT_BUSY_TIME_SECONDS           },
        { 5,  FAM_MYSQL_CLIENT_CPU_TIME_SECONDS            },
        { 6,  FAM_MYSQL_CLIENT_RECEIVED_BYTES              },
        { 7,  FAM_MYSQL_CLIENT_SENT_BYTES                  },
        { 8,  FAM_MYSQL_CLIENT_BINLOG_WRITTEN_BYTES        },
        { 9,  FAM_MYSQL_CLIENT_READ_ROWS                   },
        { 10, FAM_MYSQL_CLIENT_SENT_ROWS                   },
        { 11, FAM_MYSQL_CLIENT_DELETED_ROWS                },
        { 12, FAM_MYSQL_CLIENT_INSERTED_ROWS               },
        { 13, FAM_MYSQL_CLIENT_UPDATED_ROWS                },
        { 14, FAM_MYSQL_CLIENT_SELECT_COMMANDS             },
        { 15, FAM_MYSQL_CLIENT_UPDATE_COMMANDS             },
        { 16, FAM_MYSQL_CLIENT_OTHER_COMMANDS              },
        { 17, FAM_MYSQL_CLIENT_COMMIT_TRANSACTIONS         },
        { 18, FAM_MYSQL_CLIENT_ROLLBACK_TRANSACTIONS       },
        { 19, FAM_MYSQL_CLIENT_DENIED_CONNECTIONS          },
        { 20, FAM_MYSQL_CLIENT_LOST_CONNECTIONS            },
        { 21, FAM_MYSQL_CLIENT_ACCESS_DENIED               },
        { 22, FAM_MYSQL_CLIENT_EMPTY_QUERIES               },
        { 23, FAM_MYSQL_CLIENT_SSL_CONNECTIONS             },
        { 24, FAM_MYSQL_CLIENT_MAX_STATEMENT_TIME_EXCEEDED },
    };
    static size_t fields_size = STATIC_ARRAY_SIZE(fields);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        if (row[0] == NULL)
            continue;

        for (size_t n = 0; n < fields_size ; n++) {
            value_t value = {0};

            if (row[fields[n].field] == NULL)
                continue;

            if (db->fams[fields[n].fam].type == METRIC_TYPE_GAUGE) {
                value = VALUE_GAUGE(atof(row[fields[n].field]));
            } else if (db->fams[fields[n].fam].type == METRIC_TYPE_COUNTER) {
                if (fields[n].fam == FAM_MYSQL_CLIENT_BUSY_TIME_SECONDS) {
                    value = VALUE_COUNTER_FLOAT64(atof(row[fields[n].field]));
                } else if (fields[n].fam == FAM_MYSQL_CLIENT_CPU_TIME_SECONDS) {
                    value = VALUE_COUNTER_FLOAT64(atof(row[fields[n].field]));
                } else {
                    value = VALUE_COUNTER(atoll(row[fields[n].field]));
                }
            } else {
                continue;
            }

            metric_family_append(&db->fams[fields[n].fam], value, &db->labels,
                                 &(label_pair_const_t){.name = "client", .value = row[0]}, NULL);
        }
    }

    mysql_free_result(res);
    return 0;
}

static int cmysql_read_user_statistics(cmysql_database_t *db, MYSQL *con)
{
    if (db->mysql_server != SERVER_MARIADB)
        return 0;
    if (db->mysql_version  < 100101)
        return 0;

    const char *query = "SELECT user, total_connections, concurrent_connections, connected_time,"
                        "       busy_time, cpu_time, bytes_received, bytes_sent,"
                        "       binlog_bytes_written, rows_read, rows_sent, rows_deleted,"
                        "       rows_inserted, rows_updated, select_commands, update_commands,"
                        "       other_commands, commit_transactions, rollback_transactions,"
                        "       denied_connections, lost_connections, access_denied,"
                        "       empty_queries, total_ssl_connections, max_statement_time_exceeded"
                        "  FROM information_schema.user_statistics";

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    if (mysql_num_fields(res) != 25) {
        mysql_free_result(res);
        return -1;
    }

    static struct {
        int field;
        int fam;
    } fields[] = {
        { 1,  FAM_MYSQL_USER_CONNECTIONS                 },
        { 2,  FAM_MYSQL_USER_CONCURRENT_CONNECTIONS      },
        { 3,  FAM_MYSQL_USER_CONNECTED_TIME_SECONDS      },
        { 4,  FAM_MYSQL_USER_BUSY_TIME_SECONDS           },
        { 5,  FAM_MYSQL_USER_CPU_TIME                    },
        { 6,  FAM_MYSQL_USER_RECEIVED_BYTES              },
        { 7,  FAM_MYSQL_USER_SENT_BYTES                  },
        { 8,  FAM_MYSQL_USER_BINLOG_WRITTEN_BYTES        },
        { 9,  FAM_MYSQL_USER_READ_ROWS                   },
        { 10, FAM_MYSQL_USER_SENT_ROWS                   },
        { 11, FAM_MYSQL_USER_DELETED_ROWS                },
        { 12, FAM_MYSQL_USER_INSERTED_ROWS               },
        { 13, FAM_MYSQL_USER_UPDATED_ROWS                },
        { 14, FAM_MYSQL_USER_SELECT_COMMANDS             },
        { 15, FAM_MYSQL_USER_UPDATE_COMMANDS             },
        { 16, FAM_MYSQL_USER_OTHER_COMMANDS              },
        { 17, FAM_MYSQL_USER_COMMIT_TRANSACTIONS         },
        { 18, FAM_MYSQL_USER_ROLLBACK_TRANSACTIONS       },
        { 19, FAM_MYSQL_USER_DENIED_CONNECTIONS          },
        { 20, FAM_MYSQL_USER_LOST_CONNECTIONS            },
        { 21, FAM_MYSQL_USER_ACCESS_DENIED               },
        { 22, FAM_MYSQL_USER_EMPTY_QUERIES               },
        { 23, FAM_MYSQL_USER_TOTAL_SSL_CONNECTIONS       },
        { 24, FAM_MYSQL_USER_MAX_STATEMENT_TIME_EXCEEDED },
    };
    static size_t fields_size = STATIC_ARRAY_SIZE(fields);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        if (row[0] == NULL)
            continue;

        for (size_t n = 0; n < fields_size ; n++) {
            value_t value = {0};

            if (row[fields[n].field] == NULL)
                continue;

            if (db->fams[fields[n].fam].type == METRIC_TYPE_GAUGE) {
                value = VALUE_GAUGE(atof(row[fields[n].field]));
            } else if (db->fams[fields[n].fam].type == METRIC_TYPE_COUNTER) {
                if (fields[n].fam == FAM_MYSQL_USER_BUSY_TIME_SECONDS) {
                    value = VALUE_COUNTER_FLOAT64(atof(row[fields[n].field]));
                } else if (fields[n].fam == FAM_MYSQL_USER_CPU_TIME) {
                    value = VALUE_COUNTER_FLOAT64(atof(row[fields[n].field]));
                } else {
                    value = VALUE_COUNTER(atoll(row[fields[n].field]));
                }
            } else {
                continue;
            }

            metric_family_append(&db->fams[fields[n].fam], value, &db->labels,
                                 &(label_pair_const_t){.name = "user", .value = row[0]}, NULL);
        }
    }

    mysql_free_result(res);
    return 0;
}


static int cmysql_read_index_statistics(cmysql_database_t *db, MYSQL *con)
{
    if (db->mysql_server != SERVER_MARIADB)
        return 0;
    if (db->mysql_version  < 100101)
        return 0;

    const char *query = "SELECT table_schema, table_name, index_name, rows_read"
                        "  FROM information_schema.index_statistics";

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    if (mysql_num_fields(res) != 4) {
        mysql_free_result(res);
        return -1;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        if ((row[0] == NULL) || (row[1] == NULL) || (row[2] == NULL))
            continue;

        if (row[3] != NULL)
            metric_family_append(&db->fams[FAM_MYSQL_INDEX_ROWS_READ],
                                VALUE_COUNTER(atoll(row[3])),  &db->labels,
                                &(label_pair_const_t){.name = "table_schema", .value = row[0]},
                                &(label_pair_const_t){.name = "table_name", .value = row[1]},
                                &(label_pair_const_t){.name = "index_name", .value = row[2]},
                                NULL);
    }

    mysql_free_result(res);
    return 0;
}

static int cmysql_read_table_statistics(cmysql_database_t *db, MYSQL *con)
{
    if (db->mysql_server != SERVER_MARIADB)
        return 0;
    if (db->mysql_version  < 100101)
        return 0;

    const char *query = "SELECT table_schema, table_name, rows_read, rows_changed, "
                        "       rows_changed_x_indexes"
                        "  FROM information_schema.table_statistics";

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    if (mysql_num_fields(res) != 5) {
        mysql_free_result(res);
        return -1;
    }

    static struct {
        int field;
        int fam;
    } fields[] = {
        { 2, FAM_MYSQL_TABLE_ROWS_READ              },
        { 3, FAM_MYSQL_TABLE_ROWS_CHANGED           },
        { 4, FAM_MYSQL_TABLE_ROWS_CHANGED_X_INDEXES },
    };
    static size_t fields_size = STATIC_ARRAY_SIZE(fields);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        if ((row[0] == NULL) || (row[1] == NULL))
            continue;

        for (size_t n = 0; n < fields_size ; n++) {
            value_t value = {0};

            if (row[fields[n].field] == NULL)
                continue;

            if (db->fams[fields[n].fam].type == METRIC_TYPE_GAUGE) {
                value = VALUE_GAUGE(atof(row[fields[n].field]));
            } else if (db->fams[fields[n].fam].type == METRIC_TYPE_COUNTER) {
                value = VALUE_GAUGE(atoll(row[fields[n].field]));
            } else {
                continue;
            }

            metric_family_append(&db->fams[fields[n].fam], value, &db->labels,
                                 &(label_pair_const_t){.name = "table_schema", .value = row[0]},
                                 &(label_pair_const_t){.name = "table_name", .value = row[1]},
                                 NULL);
        }
    }

    mysql_free_result(res);
    return 0;
}

static int cmysql_read_innodb_tablespace(cmysql_database_t *db, MYSQL *con)
{
    const char *query;

    if ((db->mysql_server == SERVER_MYSQL) && (db->mysql_version >= 80030)) {
        query = "SELECT name, file_size, allocated_size"
                "  FROM information_schema.innodb_tablespaces";
    } else {
        query = "SELECT name, file_size, allocated_size"
                "  FROM information_schema.innodb_sys_tablespaces";
    }

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    if (mysql_num_fields(res) != 3) {
        mysql_free_result(res);
        return -1;
    }

    static struct {
        int field;
        int fam;
    } fields[] = {
        { 1 , FAM_MYSQL_INNODB_TABLESPACE_FILE_SIZE_BYTES      },
        { 2 , FAM_MYSQL_INNODB_TABLESPACE_ALLOCATED_SIZE_BYTES },
    };
    static size_t fields_size = STATIC_ARRAY_SIZE(fields);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        if (row[0] == NULL)
            continue;

        for (size_t n = 0; n < fields_size ; n++) {
            value_t value = {0};

            if (row[fields[n].field] == NULL)
                continue;

            if (db->fams[fields[n].fam].type == METRIC_TYPE_GAUGE) {
                value = VALUE_GAUGE(atof(row[fields[n].field]));
            } else if (db->fams[fields[n].fam].type == METRIC_TYPE_COUNTER) {
                value = VALUE_COUNTER(atoll(row[fields[n].field]));
            } else {
                continue;
            }

            metric_family_append(&db->fams[fields[n].fam], value, &db->labels,
                                 &(label_pair_const_t){.name = "tablespace", .value = row[0]},
                                 NULL);
        }
    }

    mysql_free_result(res);
    return 0;
}

static int cmysql_read_innodb_cmp(cmysql_database_t *db, MYSQL *con)
{
    const char *query = "SELECT page_size, compress_ops, compress_ops_ok, compress_time,"
                        "       uncompress_ops, uncompress_time"
                        "  FROM information_schema.innodb_cmp";

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    if (mysql_num_fields(res) != 6) {
        mysql_free_result(res);
        return -1;
    }

    static struct {
        int field;
        int fam;
    } fields[] = {
        { 1, FAM_MYSQL_INNODB_CMP_COMPRESS_OPS            },
        { 2, FAM_MYSQL_INNODB_CMP_COMPRESS_OPS_OK         },
        { 3, FAM_MYSQL_INNODB_CMP_COMPRESS_TIME_SECONDS   },
        { 4, FAM_MYSQL_INNODB_CMP_UNCOMPRESS_OPS          },
        { 5, FAM_MYSQL_INNODB_CMP_UNCOMPRESS_TIME_SECONDS },
    };
    static size_t fields_size = STATIC_ARRAY_SIZE(fields);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        if (row[0] == NULL)
            continue;

        for (size_t n = 0; n < fields_size ; n++) {
            value_t value = {0};

            if (row[fields[n].field] == NULL)
                continue;

            if (db->fams[fields[n].fam].type == METRIC_TYPE_GAUGE) {
                value = VALUE_GAUGE(atof(row[fields[n].field]));
            } else if (db->fams[fields[n].fam].type == METRIC_TYPE_COUNTER) {
                value = VALUE_COUNTER(atoll(row[fields[n].field]));
            } else {
                continue;
            }

            metric_family_append(&db->fams[fields[n].fam], value, &db->labels,
                                 &(label_pair_const_t){.name = "page_size", .value = row[0]},
                                 NULL);
        }
    }

    mysql_free_result(res);
    return 0;
}

static int cmysql_read_innodb_cmpmem(cmysql_database_t *db, MYSQL *con)
{
    const char *query = "SELECT page_size, buffer_pool_instance, pages_used, pages_free, "
                        "       relocation_ops, relocation_time "
                        "  FROM information_schema.innodb_cmpmem";

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    if (mysql_num_fields(res) != 6) {
        mysql_free_result(res);
        return -1;
    }

    static struct {
        int field;
        int fam;
    } fields[] = {
        { 2, FAM_MYSQL_INNODB_CMPMEM_USED_PAGES             },
        { 3, FAM_MYSQL_INNODB_CMPMEM_FREE_PAGES             },
        { 4, FAM_MYSQL_INNODB_CMPMEM_RELOCATION_OPS         },
        { 5, FAM_MYSQL_INNODB_CMPMEM_RELOCATION_TIME_SECOND },
    };
    static size_t fields_size = STATIC_ARRAY_SIZE(fields);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        if ((row[0] == NULL) || (row[1] == NULL))
            continue;

        for (size_t n = 0; n < fields_size ; n++) {
            value_t value = {0};

            if (row[fields[n].field] == NULL)
                continue;

            if (db->fams[fields[n].fam].type == METRIC_TYPE_GAUGE) {
                value = VALUE_GAUGE(atof(row[fields[n].field]));
            } else if (db->fams[fields[n].fam].type == METRIC_TYPE_COUNTER) {
                value = VALUE_COUNTER(atoll(row[fields[n].field]));
            } else {
                continue;
            }

            metric_family_append(&db->fams[fields[n].fam], value, &db->labels,
                                 &(label_pair_const_t){.name = "page_size", .value = row[0]},
                                 &(label_pair_const_t){.name = "buffer_pool", .value = row[1]},
                                 NULL);
        }
    }

    mysql_free_result(res);
    return 0;
}

static int cmysql_read_innodb_metrics(cmysql_database_t *db, MYSQL *con)
{
    if (db->mysql_version < 50600)
        return 0;

    const char *query;

    if (db->mysql_version >= 100500)
        query = "SELECT name, count, type FROM information_schema.innodb_metrics"
                " WHERE enabled";
    else
        query = "SELECT name, count, type FROM information_schema.innodb_metrics"
                " WHERE status = 'enabled'";

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        char *key = row[0];
        char *val = row[1];

        if ((key == NULL) || (val == NULL))
            continue;

        const struct cmysql_innodb *minnodb = cmysql_innodb_get_key(key, strlen(key));
        if (minnodb == NULL)
            continue;

        metric_family_t *fam = &db->fams[minnodb->fam];

        value_t value = {0};
        if (fam->type == METRIC_TYPE_GAUGE) {
            value = VALUE_GAUGE(atof(val));
        } else if (fam->type == METRIC_TYPE_COUNTER) {
            value = VALUE_COUNTER(atoll(val));
        }

        if (minnodb->lname != NULL) {
            metric_family_append(fam, value, &db->labels,
                                 &(label_pair_const_t){.name = minnodb->lname,
                                                       .value = minnodb->lvalue}, NULL);
        } else {
            metric_family_append(fam, value, &db->labels, NULL);
        }
    }

    mysql_free_result(res);
    return 0;
}

static int cmysql_read_status(cmysql_database_t *db, MYSQL *con)
{
    const char *query = db->mysql_version >= 50002 ? "SHOW GLOBAL STATUS" : "SHOW STATUS";

    MYSQL_RES *res = exec_query(con, query);
    if (res == NULL)
        return -1;

    if (mysql_num_fields(res) != 2) {
        mysql_free_result(res);
        return -1;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        char *key = row[0];
        char *val = row[1];

        if ((key == NULL) || (val == NULL))
            continue;

        const struct cmysql_status *mstatus = cmysql_status_get_key(key, strlen(key));
        if (mstatus == NULL)
            continue;

        if (!(db->flags & mstatus->flag))
            continue;

        metric_family_t *fam = &db->fams[mstatus->fam];

        value_t value = {0};
        if (fam->type == METRIC_TYPE_GAUGE) {
            value = VALUE_GAUGE(atof(val));
        } else if (fam->type == METRIC_TYPE_COUNTER) {
            if (mstatus->fam == FAM_MYSQL_BUSY_TIME_SECONDS) {
                value = VALUE_COUNTER_FLOAT64(atof(val));
            } else if (mstatus->fam == FAM_MYSQL_WSREP_FLOW_CONTROL_PAUSED_SECONDS) {
                value = VALUE_COUNTER_FLOAT64((double)atoll(val) * 1e-9);
            } else {
                value = VALUE_COUNTER((uint64_t)atoll(val));
            }
        }

        if (mstatus->lname != NULL) {
            metric_family_append(fam, value, &db->labels,
                                 &(label_pair_const_t){.name = mstatus->lname,
                                                       .value = mstatus->lvalue}, NULL);
        } else {
            metric_family_append(fam, value, &db->labels, NULL);
        }
    }

    mysql_free_result(res);
    return 0;
}

static int cmysql_read_database_query(cmysql_database_t *db, MYSQL *con,
                                      db_query_t *q, db_query_preparation_area_t *prep_area)
{
    size_t column_num = 0;
    char **column_names = NULL;
    char **column_values = NULL;
    MYSQL_RES *res = NULL;

    int status = -1;

    char *statement = db_query_get_statement(q);
    assert(statement != NULL);
    size_t statement_len = strlen(statement);

    if (mysql_real_query(con, statement, statement_len)) {
        PLUGIN_ERROR("Failed to execute query: %s", mysql_error(con));
        PLUGIN_INFO("SQL query was: %s", statement);
        goto error;
    }

    res = mysql_store_result(con);
    if (res == NULL) {
        PLUGIN_ERROR("Failed to store query result: %s", mysql_error(con));
        PLUGIN_INFO("SQL query was: %s", statement);
        goto error;
    }

    column_num = mysql_num_fields(res);

    /* Allocate `column_names' and `column_values'. */
    column_names = calloc(column_num, sizeof(*column_names));
    if (column_names == NULL) {
        PLUGIN_ERROR("calloc failed.");
        goto error;
    }

    column_names[0] = calloc(column_num, DATA_MAX_NAME_LEN);
    if (column_names[0] == NULL) {
        PLUGIN_ERROR("calloc failed.");
        goto error;
    }
    for (size_t i = 1; i < column_num; i++) {
        column_names[i] = column_names[i - 1] + DATA_MAX_NAME_LEN;
    }

    column_values = calloc(column_num, sizeof(*column_values));
    if (column_values == NULL) {
        PLUGIN_ERROR("calloc failed.");
        goto error;
    }

    column_values[0] = calloc(column_num, DATA_MAX_NAME_LEN);
    if (column_values[0] == NULL) {
        PLUGIN_ERROR("calloc failed.");
        goto error;
    }
    for (size_t i = 1; i < column_num; i++)
        column_values[i] = column_values[i - 1] + DATA_MAX_NAME_LEN;

    for (size_t i = 0; i < column_num; i++) {
        MYSQL_FIELD *field = mysql_fetch_field(res);
        if (field == NULL) {
            PLUGIN_ERROR("mysql_fetch_field %zu in %s at %s failed : %s",
                          i+1, db_query_get_name(q), db->instance, mysql_error(con));
            goto error;
        }

        sstrncpy(column_names[i], field->name, DATA_MAX_NAME_LEN);
    }

    status = db_query_prepare_result(q, prep_area, db->metric_prefix, &db->labels,
                                        db->instance, column_names, column_num);
    if (status != 0) {
        PLUGIN_ERROR("db_query_prepare_result failed with status %i.", status);
        goto error;
    }

    /* Iterate over all rows and call `db_query_handle_result' with each list of values. */
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        /* Copy the value of the columns to `column_values' */
        for (size_t i = 0; i < column_num; i++) {
            if (row[i] == NULL) {
                column_values[i][0] = '\0';
            } else {
                sstrncpy(column_values[i], row[i], DATA_MAX_NAME_LEN);
            }
        }

        /* If all values were copied successfully, call `db_query_handle_result'
         * to dispatch the row to the daemon. */
        status = db_query_handle_result(q, prep_area, column_values, db->filter);
        if (status != 0) {
            PLUGIN_ERROR("%s in %s: db_query_handle_result failed.",
                         db->instance, db_query_get_name(q));
            goto error;
        }
    }

    /* Tell the db query interface that we're done with this query. */
    db_query_finish_result(q, prep_area);

    status = 0;

error:

    if (column_names != NULL) {
        free(column_names[0]);
        free(column_names);
    }

    if (column_values != NULL) {
        free(column_values[0]);
        free(column_values);
    }

    if (res != NULL)
        mysql_free_result(res);

    return status;
}

static int cmysql_read(user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    cmysql_database_t *db = (cmysql_database_t *)ud->data;

    /* An error message will have been printed in this case */
    MYSQL *con = cmysql_get_connection(db);
    if (con == NULL) {
        metric_family_append(&db->fams[FAM_MYSQL_UP], VALUE_GAUGE(0), &db->labels, NULL);
        plugin_dispatch_metric_family_filtered(&db->fams[FAM_MYSQL_UP], db->filter, 0);
        return 0;
    }

    metric_family_append(&db->fams[FAM_MYSQL_UP], VALUE_GAUGE(1), &db->labels, NULL);

    cmysql_read_status(db, con);

    if (db->flags & COLLECT_INNODB)
        cmysql_read_innodb_metrics(db, con);

    if (db->flags & COLLECT_INNODB_CMP)
        cmysql_read_innodb_cmp(db, con);

    if (db->flags & COLLECT_INNODB_CMPMEM)
        cmysql_read_innodb_cmpmem(db, con);

    if (db->flags & COLLECT_INNODB_TABLESPACE)
        cmysql_read_innodb_tablespace(db, con);

    if (db->flags & COLLECT_CLIENT_STATS)
        cmysql_read_client_statistics(db, con);

    if (db->flags & COLLECT_USER_STATS)
        cmysql_read_user_statistics(db, con);

    if (db->flags & COLLECT_INDEX_STATS)
        cmysql_read_index_statistics(db, con);

    if (db->flags & COLLECT_TABLE_STATS) {
        cmysql_read_table_statistics(db, con);
        cmysql_read_table(db, con);
    }

    if (db->flags & COLLECT_RESPONSE_TIME) {
        cmysql_read_query_response_time_all(db, con);
        cmysql_read_query_response_time_read(db, con);
        cmysql_read_query_response_time_write(db, con);
    }

    if (db->flags & COLLECT_HEARTBEAT)
        cmysql_read_heartbeat(db, con);

    if (db->primary_stats)
        cmysql_read_primary_stats(db, con);

    if ((db->replica_stats) || (db->replica_notif))
        cmysql_read_replica_stats(db, con);

    plugin_dispatch_metric_family_array_filtered(db->fams, FAM_MYSQL_STATUS_MAX, db->filter, 0);

    for (size_t i = 0; i < db->queries_num; i++) {
        /* Check if we know the database's version and if so, if this query applies
         * to that version. */
        if ((db->mysql_version!= 0) &&
            (db_query_check_version(db->queries[i], db->mysql_version) == 0))
            continue;

        cmysql_read_database_query(db, con, db->queries[i], db->q_prep_areas[i]);
    }


    return 0;
}

/* Configuration handling functions
 *
 * plugin mysql {
 *   database "plugin_instance1" {
 *       Host "localhost"
 *       Port 22000
 *       ...
 *   }
 * }
 */
static int cmysql_config_database(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The 'database' block needs exactly one string argument.");
        return -1;
    }

    cmysql_database_t *db = calloc(1, sizeof(*db));
    if (db == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    db->flags = COLLECT_GLOBALS;

    C_COMPLAIN_INIT(&db->conn_complaint);

    /* trigger a notification, if it's not running */
    db->replica_io_running = true;
    db->replica_sql_running = true;

    memcpy(db->fams, fam_mysql_status, sizeof(db->fams[0])*FAM_MYSQL_STATUS_MAX);

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
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &db->pass);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_port_number(child, &db->port);
        } else if (strcasecmp("socket", child->key) == 0) {
            status = cf_util_get_string(child, &db->socket);
        } else if (strcasecmp("database", child->key) == 0) {
            status = cf_util_get_string(child, &db->database);
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
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &db->metric_prefix);
        } else if (strcasecmp("heartbeat-utc", child->key) == 0) {
            status = cf_util_get_boolean(child, &db->heartbeat_utc);
        } else if (strcasecmp("heartbeat-schema", child->key) == 0) {
            status = cf_util_get_string(child, &db->heartbeat_schema);
        } else if (strcasecmp("heartbeat-table", child->key) == 0) {
            status = cf_util_get_string(child, &db->heartbeat_table);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &db->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("collect", child->key) == 0) {
            status = cf_util_get_flags(child, cmysql_flags, cmysql_flags_size, &db->flags);
        } else if (strcasecmp("query", child->key) == 0) {
            status = db_query_pick_from_list(child, queries, queries_num,
                                                    &db->queries, &db->queries_num);
        } else if (strcasecmp("master-stats", child->key) == 0) {
            status = cf_util_get_boolean(child, &db->primary_stats);
        } else if (strcasecmp("slave-stats", child->key) == 0) {
            status = cf_util_get_boolean(child, &db->replica_stats);
        } else if (strcasecmp("slave-notifications", child->key) == 0) {
            status = cf_util_get_boolean(child, &db->replica_notif);
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

    while ((status == 0) && (db->queries_num > 0)) {
        db->q_prep_areas = calloc(db->queries_num, sizeof(*db->q_prep_areas));
        if (db->q_prep_areas == NULL) {
            PLUGIN_WARNING("calloc failed");
            status = -1;
            break;
        }

        for (size_t i = 0; i < db->queries_num; ++i) {
            db->q_prep_areas[i] = db_query_allocate_preparation_area(db->queries[i]);

            if (db->q_prep_areas[i] == NULL) {
                PLUGIN_WARNING("db_query_allocate_preparation_area failed");
                status = -1;
                break;
            }
        }

        break;
    }

    /* If all went well, register this database for reading */
    if (status != 0) {
        cmysql_database_free(db);
        return -1;
    }

    label_set_add(&db->labels, true, "instance", db->instance);

    if ((db->host != NULL) && (strcmp(db->host, "") != 0) &&
        (strcmp(db->host, "127.0.0.1") != 0) && (strcmp(db->host, "localhost") != 0)) {
        label_set_add(&db->labels, false, "hostname", db->host);
    }

    return plugin_register_complex_read("mysql", db->instance, cmysql_read, interval,
                                        &(user_data_t){.data=db, .free_func=cmysql_database_free});
}

static int cmysql_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = cmysql_config_database(child);
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

static int cmysql_shutdown(void)
{
    db_query_free(queries, queries_num);
    queries = NULL;
    queries_num = 0;
    return 0;
}

void module_register(void)
{
    plugin_register_config("mysql", cmysql_config);
    plugin_register_shutdown("mysql", cmysql_shutdown);
}
