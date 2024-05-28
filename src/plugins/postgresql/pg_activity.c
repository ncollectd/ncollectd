// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"

#include <libpq-fe.h>

#include "pg_fam.h"

int pg_stat_activity (PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                    char *db)
{
    if (version < 90200)
        return 0;

    char buffer[1024];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);

    int stmt_params = 0;
    const char *param_values[1] = {NULL};
    int param_lengths[1] = {0};
    int param_formats[1] = {0};

    strbuf_putstr(&buf, "SELECT pg_database.datname, tmp.state, COALESCE(count,0) as count,"
                        "       COALESCE(max_tx,0) as max_tx"
                        "  FROM ( VALUES ('active'),"
                        "                ('idle'),"
                        "                ('idle in transaction'),"
                        "                ('idle in transaction (aborted)'),"
                        "                ('fastpath function call'),"
                        "                ('disabled')) AS tmp(state) CROSS JOIN pg_database "
                        "LEFT JOIN ( SELECT datname, state, count(*) AS count,"
                        "            MAX(EXTRACT(EPOCH FROM now() - xact_start))::float AS max_tx"
                        "            FROM pg_stat_activity GROUP BY datname,state ) AS tmp2"
                        " ON tmp.state = tmp2.state AND pg_database.datname = tmp2.datname");
    if (db != NULL) {
        strbuf_putstr(&buf, " WHERE datname = $1");
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
    if (fields < 4) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_database = PQgetvalue(res, i, 0);

        if (PQgetisnull(res, i, 1))
            continue;
        char *col_state = PQgetvalue(res, i, 1);

        if (!PQgetisnull(res, i, 2))
            metric_family_append(&fams[FAM_PG_ACTIVITY_CONNECTIONS],
                                 VALUE_GAUGE(atol(PQgetvalue(res, i, 2))), labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="state", .value=col_state},
                                 NULL);

        if (!PQgetisnull(res, i, 3))
            metric_family_append(&fams[FAM_PG_ACTIVITY_MAX_TX_SECONDS],
                                 VALUE_GAUGE(atol(PQgetvalue(res, i, 3))), labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="state", .value=col_state},
                                 NULL);
    }

    PQclear(res);

    return 0;
}
