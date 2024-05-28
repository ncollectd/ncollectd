// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"

#include <libpq-fe.h>

#include "pg_fam.h"

int pg_stat_archiver(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels)
{
    if (version < 90400)
        return 0;

    char *stmt = "SELECT archived_count, failed_count,"
                 "       extract(epoch from now() - last_archived_time) AS last_archive_age"
                 "  FROM pg_stat_archiver";

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
    if (fields < 3) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (!PQgetisnull(res, i, 0))
            metric_family_append(&fams[FAM_PG_ARCHIVER_ARCHIVED],
                                 VALUE_COUNTER(atoi(PQgetvalue(res, i, 0))), labels, NULL);

        if (!PQgetisnull(res, i, 1))
            metric_family_append(&fams[FAM_PG_ARCHIVER_FAILED],
                                 VALUE_COUNTER(atoi(PQgetvalue(res, i, 1))), labels, NULL);

        if (!PQgetisnull(res, i, 2))
            metric_family_append(&fams[FAM_PG_ARCHIVER_LAST_ARCHIVE_AGE_SECONDS],
                                 VALUE_GAUGE(atof(PQgetvalue(res, i, 2))), labels, NULL);
    }

    PQclear(res);

    return 0;
}
