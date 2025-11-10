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
    if (version < 100000)
        return 0;

    char *stmt = "SELECT application_name, client_addr, state, sync_state,"
                 "       COALESCE(pg_wal_lsn_diff(CASE WHEN pg_is_in_recovery() THEN pg_last_wal_receive_lsn() ELSE pg_current_wal_lsn() END, sent_lsn), 0) AS sent_lsn_lag,"
                 "       COALESCE(pg_wal_lsn_diff(CASE WHEN pg_is_in_recovery() THEN pg_last_wal_receive_lsn() ELSE pg_current_wal_lsn() END, write_lsn), 0) AS write_lsn_lag,"
                 "       COALESCE(pg_wal_lsn_diff(CASE WHEN pg_is_in_recovery() THEN pg_last_wal_receive_lsn() ELSE pg_current_wal_lsn() END, flush_lsn), 0) AS flush_lsn_lag,"
                 "       COALESCE(pg_wal_lsn_diff(CASE WHEN pg_is_in_recovery() THEN pg_last_wal_receive_lsn() ELSE pg_current_wal_lsn() END, replay_lsn), 0) AS replay_lsn_lag,"
                 "       EXTRACT(EPOCH from write_lag) as write_lag,"
                 "       EXTRACT(EPOCH from flush_lag) as flush_lag,"
                 "       EXTRACT(EPOCH from replay_lag) as replay_lag"
                 "  FROM pg_stat_replication"
                 " WHERE application_name NOT IN ('pg_basebackup', 'pg_rewind');";

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
    if (fields < 11) {
        PQclear(res);
        return 0;
    }

    struct {
        int field;
        int fam;
    } pg_fields[] = {
        {  4, FAM_PG_REPLICATION_WAL_SEND_LAG_BYTES },
        {  5, FAM_PG_REPLICATION_WAL_WRITE_LAG_BYTES },
        {  6, FAM_PG_REPLICATION_WAL_FLUSH_LAG_BYTES },
        {  7, FAM_PG_REPLICATION_WAL_REPLAY_LAG_BYTES },
        {  8, FAM_PG_REPLICATION_WAL_WRITE_LAG_SECONDS },
        {  9, FAM_PG_REPLICATION_WAL_FLUSH_LAG_SECONDS },
        { 10, FAM_PG_REPLICATION_WAL_REPLAY_LAG_SECONDS },
    };
    size_t pg_fields_size = STATIC_ARRAY_SIZE(pg_fields);

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_application_name = PQgetvalue(res, i, 0);

        char *col_client_addr = NULL;
        if (!PQgetisnull(res, i, 1))
            col_client_addr = PQgetvalue(res, i, 1);

        if (!PQgetisnull(res, i, 2)) {
            char *col_state = PQgetvalue(res, i, 2);
            state_t states[] = {
                { .name = "startup",   .enabled = false },
                { .name = "catchup",   .enabled = false },
                { .name = "streaming", .enabled = false },
                { .name = "backup",    .enabled = false },
                { .name = "stopping",  .enabled = false },
            };
            state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
            for (size_t j = 0; j < set.num ; j++) {
                if (strncmp(set.ptr[j].name, col_state, strlen(set.ptr[j].name)) == 0) {
                    set.ptr[j].enabled = true;
                    break;
                }
            }
            metric_family_append(&fams[FAM_PG_REPLICATION_STATE], VALUE_STATE_SET(set), labels,
                                 &(label_pair_const_t){.name="application",
                                                       .value=col_application_name},
                                 &(label_pair_const_t){.name="client_addr",
                                                       .value=col_client_addr},
                                 NULL);
        }

        if (!PQgetisnull(res, i, 3)) {
            char *col_sync_state = PQgetvalue(res, i, 3);
            state_t states[] = {
                { .name = "async",     .enabled = false },
                { .name = "potential", .enabled = false },
                { .name = "sync",      .enabled = false },
                { .name = "quorum",    .enabled = false },
            };
            state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
            for (size_t j = 0; j < set.num ; j++) {
                if (strncmp(set.ptr[j].name, col_sync_state, strlen(set.ptr[j].name)) == 0) {
                    set.ptr[j].enabled = true;
                    break;
                }
            }
            metric_family_append(&fams[FAM_PG_REPLICATION_SYNC_STATE], VALUE_STATE_SET(set), labels,
                                 &(label_pair_const_t){.name="application",
                                                       .value=col_application_name},
                                 &(label_pair_const_t){.name="client_addr",
                                                       .value=col_client_addr},
                                 NULL);
        }

        for (size_t n = 0; n < pg_fields_size ; n++) {
            int field = pg_fields[n].field;
            if (!PQgetisnull(res, i, field)) {
                metric_family_t *fam = &fams[pg_fields[n].fam];
                value_t value = {0};
                if (fam->type == METRIC_TYPE_GAUGE) {
                    value = VALUE_GAUGE(atof(PQgetvalue(res, i, field)));
                } else if (fam->type == METRIC_TYPE_COUNTER) {
                    value = VALUE_COUNTER(atol(PQgetvalue(res, i, field)));
                } else {
                    continue;
                }

                metric_family_append(fam, value, labels,
                                     &(label_pair_const_t){.name="application",
                                                           .value=col_application_name},
                                     &(label_pair_const_t){.name="client_addr",
                                                           .value=col_client_addr},
                                     NULL);
            }
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
