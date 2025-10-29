// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"

#include <libpq-fe.h>

#include "pg_fam.h"

int pg_stat_replication(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels)
{
    if (version < 90200)
        return 0;

    char *stmt = NULL;

    if (version >= 100000)  {
        stmt = "SELECT application_name, client_addr, state,"
               "       (case pg_is_in_recovery() when 't' then null else pg_current_wal_lsn() end) AS pg_current_wal_lsn,"
               "       (case pg_is_in_recovery() when 't' then null else pg_wal_lsn_diff(pg_current_wal_lsn(), replay_lsn)::float end) AS pg_wal_lsn_diff,"
               "       (case pg_is_in_recovery() when 't' then null else pg_wal_lsn_diff(pg_current_wal_lsn(), pg_lsn('0/0'))::float end) AS pg_current_wal_lsn_bytes"
               "  FROM pg_stat_replication";
    } else {
        stmt = "SELECT application_name, client_addr, state,"
               "       (case pg_is_in_recovery() when 't' then null else pg_current_xlog_location() end) AS pg_current_xlog_location,"
               "       (case pg_is_in_recovery() when 't' then null else pg_xlog_location_diff(pg_current_xlog_location(), replay_location)::float end) AS pg_xlog_location_diff"
               "  FROM pg_stat_replication";
    }

    PGresult *res = PQprepare(conn, "", stmt, 0, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PLUGIN_ERROR("PQprepare failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    PQclear(res);

    res = PQexecPrepared(conn, "", 0, NULL, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexecPrepared failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    int fields = PQnfields(res);
    if (version >= 100000)  {
        if (fields < 6) {
            PQclear(res);
            return 0;
        }
    } else {
        if (fields < 5) {
            PQclear(res);
            return 0;
        }
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_application_name = PQgetvalue(res, i, 0);

        char *col_client_addr = NULL;
        if (!PQgetisnull(res, i, 1))
            col_client_addr = PQgetvalue(res, i, 1);

        if (PQgetisnull(res, i, 2))
            continue;
        char *col_state = PQgetvalue(res, i, 2);

        char *col_current_wal_lsn = NULL;
        if (!PQgetisnull(res, i, 3))
            col_current_wal_lsn = PQgetvalue(res, i, 3);

        if (!PQgetisnull(res, i, 4))
            metric_family_append(&fams[FAM_PG_REPLICATION_WAL_LSN_DIFF],
                                 VALUE_GAUGE(atof(PQgetvalue(res, i, 4))), labels,
                                 &(label_pair_const_t){.name="application", .value=col_application_name},
                                 &(label_pair_const_t){.name="client_addr", .value=col_client_addr},
                                 &(label_pair_const_t){.name="wal", .value= col_current_wal_lsn},
                                 &(label_pair_const_t){.name="state", .value=col_state},
                                 NULL);
        if (version >= 100000) {
            if (!PQgetisnull(res, i, 5))
                metric_family_append(&fams[FAM_PG_REPLICATION_CURRENT_WAL_LSN_BYTES],
                                     VALUE_GAUGE(atof(PQgetvalue(res, i, 5))), labels,
                                     &(label_pair_const_t){.name="application", .value=col_application_name},
                                     &(label_pair_const_t){.name="client_addr", .value=col_client_addr},
                                     &(label_pair_const_t){.name="wal", .value= col_current_wal_lsn},
                                     &(label_pair_const_t){.name="state", .value=col_state},
                                     NULL);
        }
    }

    PQclear(res);

    return 0;
}

int pg_replication_slots(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                       char *db)
{
    if (version < 90400)
        return 0;

    char buffer[256];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);

    int stmt_params = 0;
    const char *param_values[1] = {NULL};
    int param_lengths[1] = {0};
    int param_formats[1] = {0};

    int status = strbuf_putstr(&buf,
        "SELECT slot_name, database, active::int,"
        "       (case pg_is_in_recovery() when 't' then 0::float else pg_wal_lsn_diff(pg_current_wal_lsn(), restart_lsn)::float end) AS pg_wal_lsn_diff"
        "  FROM pg_replication_slots");

    if (db != NULL) {
        status |= strbuf_putstr(&buf, " WHERE database = $1");
        stmt_params = 1;
        param_values[0] = db;
        param_lengths[0] = strlen(db);
    }

    if (status != 0) {
        PLUGIN_ERROR("Failed to create statement.");
        return -1;
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
    if (fields < 4) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_slot_name = PQgetvalue(res, i, 0);

        char *col_database = "null";
        if (!PQgetisnull(res, i, 1))
            col_database = PQgetvalue(res, i, 1);

        if (!PQgetisnull(res, i, 2))
            metric_family_append(&fams[FAM_PG_REPLICATION_SLOT_ACTIVE],
                                 VALUE_GAUGE(atof(PQgetvalue(res, i, 2))), labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="slot", .value=col_slot_name},
                                 NULL);

        if (!PQgetisnull(res, i, 3))
            metric_family_append(&fams[FAM_PG_REPLICATION_SLOT_WAL_LSN_DIFF_BYTES],
                                 VALUE_GAUGE(atof(PQgetvalue(res, i, 3))), labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="slot", .value=col_slot_name},
                                 NULL);
    }

    PQclear(res);

    return 0;
}
