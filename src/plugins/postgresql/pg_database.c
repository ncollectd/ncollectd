// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"

#include <libpq-fe.h>

#include "pg_fam.h"

int pg_stat_database(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                   char *db)
{
    if (version < 70200)
        return 0;

    char buffer[1024];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);

    int stmt_params = 0;
    const char *param_values[1] = {NULL};
    int param_lengths[1] = {0};
    int param_formats[1] = {0};

    strbuf_putstr(&buf, "SELECT datname, numbackends, xact_commit, xact_rollback"
                        ", blks_read, blks_hit");

    if (version >= 80300)
        strbuf_putstr(&buf, ", tup_returned, tup_fetched, tup_inserted, tup_updated, tup_deleted");
    if (version >= 90100)
        strbuf_putstr(&buf, ", conflicts");
    if (version >= 90200)
        strbuf_putstr(&buf, ", temp_files, temp_bytes, deadlocks"
                            ", blk_read_time, blk_write_time");
    if (version >= 120000)
        strbuf_putstr(&buf, ", checksum_failures"
                            ", EXTRACT(epoch from COALESCE(checksum_last_failure, '1970-01-01Z'))");
    if (version >= 140000)
        strbuf_putstr(&buf, ", session_time, active_time, idle_in_transaction_time"
                            ", sessions, sessions_abandoned, sessions_fatal, sessions_killed");

    strbuf_putstr(&buf, "  FROM pg_stat_database");

    if (db == NULL) {
        strbuf_putchar(&buf, ';');
    } else {
        strbuf_putstr(&buf, " WHERE datname = $1;");
        stmt_params = 1;
        param_values[0] = db;
        param_lengths[0] = strlen(db);
    }

    char *stmt = buf.ptr;

    struct {
        int field;
        int minversion;
        double scale;
        int fam;
    } pg_fields[] = {
        {  1,  70200, 0.0,   FAM_PG_DATABASE_BACKENDS                         },
        {  2,  70200, 0.0,   FAM_PG_DATABASE_XACT_COMMIT                      },
        {  3,  70200, 0.0,   FAM_PG_DATABASE_XACT_ROLLBACK                    },
        {  4,  70200, 0.0,   FAM_PG_DATABASE_BLKS_READ                        },
        {  5,  70200, 0.0,   FAM_PG_DATABASE_BLKS_HIT                         },
        {  6,  80300, 0.0,   FAM_PG_DATABASE_RETURNED_ROWS                    },
        {  7,  80300, 0.0,   FAM_PG_DATABASE_FETCHED_ROWS                     },
        {  8,  80300, 0.0,   FAM_PG_DATABASE_INSERTED_ROWS                    },
        {  9,  80300, 0.0,   FAM_PG_DATABASE_UPDATED_ROWS                     },
        { 10,  80300, 0.0,   FAM_PG_DATABASE_DELETED_ROWS                     },
        { 11,  90100, 0.0,   FAM_PG_DATABASE_CONFLICTS                        },
        { 12,  90200, 0.0,   FAM_PG_DATABASE_TEMP_FILES                       },
        { 13,  90200, 0.0,   FAM_PG_DATABASE_TEMP_BYTES                       },
        { 14,  90200, 0.0,   FAM_PG_DATABASE_DEADLOCKS                        },
        { 15,  90200, 0.001, FAM_PG_DATABASE_BLK_READ_TIME_SECONDS            },
        { 16,  90200, 0.001, FAM_PG_DATABASE_BLK_WRITE_TIME_SECONDS           },
        { 17, 120000, 0.0,   FAM_PG_DATABASE_CHECKSUM_FAILURES                },
        { 18, 120000, 0.0,   FAM_PG_DATABASE_CHECKSUM_LAST_FAILURE            },
        { 19, 140000, 0.001, FAM_PG_DATABASE_SESSION_TIME_SECONDS             },
        { 20, 140000, 0.001, FAM_PG_DATABASE_ACTIVE_TIME_SECONDS              },
        { 21, 140000, 0.001, FAM_PG_DATABASE_IDLE_IN_TRANSACTION_TIME_SECONDS },
        { 22, 140000, 0.0,   FAM_PG_DATABASE_SESSIONS                         },
        { 23, 140000, 0.0,   FAM_PG_DATABASE_SESSIONS_ABANDONED               },
        { 24, 140000, 0.0,   FAM_PG_DATABASE_SESSIONS_FATAL                   },
        { 25, 140000, 0.0,   FAM_PG_DATABASE_SESSIONS_KILLED                  },
    };
    size_t pg_fields_size = STATIC_ARRAY_SIZE(pg_fields);

    PGresult *res = PQprepare(conn, "", stmt, stmt_params, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PLUGIN_ERROR("PQprepare failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    PQclear(res);

    res = PQexecPrepared(conn, "", stmt_params, param_values, param_lengths, param_formats, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexecPrepared failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    int expected = 6;
    if (version >= 80300)
        expected = 11;
    if (version >= 90100)
        expected = 12;
    if (version >= 90200)
        expected = 17;
    if (version >= 120000)
        expected = 19;
    if (version >= 140000)
        expected = 26;

    int fields = PQnfields(res);
    if (fields < expected) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_database = PQgetvalue(res, i, 0);

        for (size_t n = 0; n < pg_fields_size ; n++) {
            if (version < pg_fields[n].minversion)
                continue;
            int field = pg_fields[n].field;
            double scale = pg_fields[n].scale;
            if (!PQgetisnull(res, i, field)) {
                metric_family_t *fam = &fams[pg_fields[n].fam];
                value_t value = {0};
                if (fam->type == METRIC_TYPE_GAUGE) {
                    if (scale != 0.0) {
                        value = VALUE_GAUGE(atof(PQgetvalue(res, i, field)) * scale);
                    } else {
                        value = VALUE_GAUGE(atof(PQgetvalue(res, i, field)));
                    }
                } else if (fam->type == METRIC_TYPE_COUNTER) {
                    if (scale != 0.0) {
                        value = VALUE_COUNTER_FLOAT64(atof(PQgetvalue(res, i, field)) * scale);
                    } else {
                        value = VALUE_COUNTER( atol(PQgetvalue(res, i, field)));
                    }
                } else {
                    continue;
                }

                metric_family_append(fam, value, labels,
                                     &(label_pair_const_t){.name="database", .value=col_database},
                                     NULL);
            }
        }
    }

    PQclear(res);

    return 0;
}

int pg_database_size(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                   char *db)
{
    (void)version;
    char buffer[256];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);

    int stmt_params = 0;
    const char *param_values[1] = {NULL};
    int param_lengths[1] = {0};
    int param_formats[1] = {0};

    strbuf_putstr(&buf, "SELECT pg_database.datname, pg_database_size(pg_database.datname)"
                        "  FROM pg_database");

    if (db == NULL) {
        strbuf_putchar(&buf, ';');
    } else {
        strbuf_putstr(&buf, " WHERE datname = $1;");
        stmt_params = 1;
        param_values[0] = db;
        param_lengths[0] = strlen(db);
    }

    char *stmt = buf.ptr;

    PGresult *res = PQprepare(conn, "", stmt, stmt_params, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PLUGIN_ERROR("PQprepare failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    PQclear(res);

    res = PQexecPrepared(conn, "", stmt_params, param_values, param_lengths, param_formats, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexecPrepared failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    int fields = PQnfields(res);
    if (fields < 2)
        return 0;

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_database = PQgetvalue(res, i, 0);

        if (!PQgetisnull(res, i, 1))
            metric_family_append(&fams[FAM_PG_DATABASE_SIZE_BYTES],
                                 VALUE_GAUGE(atof(PQgetvalue(res, i, 1))), labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 NULL);
    }

    PQclear(res);

    return 0;
}

int pg_database_locks(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                    char *db)
{
    if (version < 70200)
        return 0;

    char *stmt = NULL;
    int stmt_params = 0;
    const char *param_values[1] = {NULL};
    int param_lengths[1] = {0};
    int param_formats[1] = {0};

    if (db == NULL) {
        stmt = "SELECT pg_database.datname, tmp.mode, COALESCE(count,0) "
               "  FROM ( VALUES ('accesssharelock'),"
               "                ('rowsharelock'),"
               "                ('rowexclusivelock'),"
               "                ('shareupdateexclusivelock'),"
               "                ('sharelock'),"
               "                ('sharerowexclusivelock'),"
               "                ('exclusivelock'),"
               "                ('accessexclusivelock'),"
               "                ('sireadlock')) AS tmp(mode) CROSS JOIN pg_database "
               "LEFT JOIN "
               "(SELECT database, lower(mode) AS mode,count(*) AS count "
               "   FROM pg_locks WHERE database IS NOT NULL "
               "  GROUP BY database, lower(mode)) AS tmp2 "
               "ON tmp.mode = tmp2.mode AND pg_database.oid = tmp2.database";
     } else {
        stmt = "SELECT pg_database.datname, tmp.mode, COALESCE(count,0) "
               "  FROM ( VALUES ('accesssharelock'),"
               "                ('rowsharelock'),"
               "                ('rowexclusivelock'),"
               "                ('shareupdateexclusivelock'),"
               "                ('sharelock'),"
               "                ('sharerowexclusivelock'),"
               "                ('exclusivelock'),"
               "                ('accessexclusivelock'),"
               "                ('sireadlock')) AS tmp(mode) CROSS JOIN pg_database "
               "LEFT JOIN "
               "(SELECT database, lower(mode) AS mode,count(*) AS count "
               "   FROM pg_locks WHERE database IS NOT NULL "
               "  GROUP BY database, lower(mode)) AS tmp2 "
               "ON tmp.mode = tmp2.mode AND pg_database.oid = tmp2.database "
               "WHERE pg_database.datname = $1";
        stmt_params = 1;
        param_values[0] = db;
        param_lengths[0] = strlen(db);
    }

    PGresult *res = PQprepare(conn, "", stmt, stmt_params, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PLUGIN_ERROR("PQprepare failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    PQclear(res);

    res = PQexecPrepared(conn, "", stmt_params, param_values, param_lengths, param_formats, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexecPrepared failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    int fields = PQnfields(res);
    if (fields < 3)
        return 0;

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_database = PQgetvalue(res, i, 0);

        if (PQgetisnull(res, i, 1))
            continue;
        char *col_mode= PQgetvalue(res, i, 1);

        if (!PQgetisnull(res, i, 2))
            metric_family_append(&fams[FAM_PG_DATABASE_LOCKS],
                                 VALUE_GAUGE(atof(PQgetvalue(res, i, 2))), labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="mode", .value=col_mode},
                                 NULL);
    }

    PQclear(res);

    return 0;
}

int pg_stat_database_conflicts(PGconn *conn, int version, metric_family_t *fams,
                                             label_set_t *labels, char *db)
{
    if (version < 90100)
        return 0;

    char buffer[256];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);

    int stmt_params = 0;
    const char *param_values[1] = {NULL};
    int param_lengths[1] = {0};
    int param_formats[1] = {0};

    strbuf_putstr(&buf, "SELECT datname, confl_tablespace, confl_lock, confl_snapshot,"
                        "       confl_bufferpin, confl_deadlock"
                        "  FROM pg_stat_database_conflicts");

    if (db == NULL) {
        strbuf_putchar(&buf, ';');
    } else {
        strbuf_putstr(&buf, " WHERE datname = $1;");
        stmt_params = 1;
        param_values[0] = db;
        param_lengths[0] = strlen(db);
    }

    char *stmt = buf.ptr;

    struct {
        int field;
        int fam;
    } pg_fields[] = {
        {  1, FAM_PG_DATABASE_CONFLICTS_TABLESPACE },
        {  2, FAM_PG_DATABASE_CONFLICTS_LOCK       },
        {  3, FAM_PG_DATABASE_CONFLICTS_SNAPSHOT   },
        {  4, FAM_PG_DATABASE_CONFLICTS_BUFFERPIN  },
        {  5, FAM_PG_DATABASE_CONFLICTS_DEADLOCK   },
    };
    size_t pg_fields_size = STATIC_ARRAY_SIZE(pg_fields);

    PGresult *res = PQprepare(conn, "", stmt, stmt_params, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PLUGIN_ERROR("PQprepare failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    PQclear(res);

    res = PQexecPrepared(conn, "", stmt_params, param_values, param_lengths, param_formats, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexecPrepared failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    int fields = PQnfields(res);
    if (fields < 6) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_database = PQgetvalue(res, i, 0);

        for (size_t n = 0; n < pg_fields_size ; n++) {
            int field = pg_fields[n].field;
            if (!PQgetisnull(res, i, field)) {
                metric_family_t *fam = &fams[pg_fields[n].fam];
                value_t value = {0};
                if (fam->type == METRIC_TYPE_GAUGE) {
                    value = VALUE_GAUGE(atof(PQgetvalue(res, i, field)));
                } else if (fam->type == METRIC_TYPE_COUNTER) {
                    value = VALUE_COUNTER( atol(PQgetvalue(res, i, field)));
                } else {
                    continue;
                }

                metric_family_append(fam, value, labels,
                                     &(label_pair_const_t){.name="database", .value=col_database},
                                     NULL);
            }
        }
    }

    PQclear(res);

    return 0;
}
