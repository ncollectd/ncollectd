// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"

#include <libpq-fe.h>

#include "pg_fam.h"

int pg_buffercache(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels)
{
    if (version < 160000)
        return 0;

    char *stmt = "SELECT buffers_used, buffers_unused, buffers_dirty, buffers_pinned"
                 "  FROM pg_buffercache_summary()";

    struct {
        int field;
        int fam;
    } pg_fields[] = {
        { 0, FAM_PG_BUFFERCACHE_BUFFERS_USED},
        { 1, FAM_PG_BUFFERCACHE_BUFFERS_UNUSED},
        { 2, FAM_PG_BUFFERCACHE_BUFFERS_DIRTY},
        { 3, FAM_PG_BUFFERCACHE_BUFFERS_PINNED}
    };
    size_t pg_fields_size = STATIC_ARRAY_SIZE(pg_fields);

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
    if (fields < 4) {
        PQclear(res);
        return 0;
    }

    if (PQntuples(res) > 0) {
        for (size_t n = 0; n < pg_fields_size ; n++) {
            int field = pg_fields[n].field;
            if (!PQgetisnull(res, 0, field)) {
                metric_family_t *fam = &fams[pg_fields[n].fam];
                value_t value = {0};
                if (fam->type == METRIC_TYPE_GAUGE) {
                    value = VALUE_GAUGE(atof(PQgetvalue(res, 0, field)));
                } else if (fam->type == METRIC_TYPE_COUNTER) {
                    value = VALUE_COUNTER( atol(PQgetvalue(res, 0, field)));
                } else {
                    continue;
                }
                metric_family_append(fam, value, labels, NULL);
            }
        }
    }

    PQclear(res);

    return 0;
}
