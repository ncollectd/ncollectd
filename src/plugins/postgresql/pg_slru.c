// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"

#include <libpq-fe.h>

#include "pg_fam.h"

int pg_stat_slru(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels)
{
    if (version < 130000)
        return 0;

    char *stmt = "SELECT name, blks_zeroed, blks_hit, blks_read, blks_written,"
                 "       blks_exists, flushes, truncates"
                 "  FROM pg_stat_slru";

    struct {
        int field;
        int fam;
    } pg_fields[] = {
        { 1, FAM_PG_SLRU_BLOCKS_ZEROED  },
        { 2, FAM_PG_SLRU_BLOCKS_HIT     },
        { 3, FAM_PG_SLRU_BLOCKS_READ    },
        { 4, FAM_PG_SLRU_BLOCKS_WRITTEN },
        { 5, FAM_PG_SLRU_BLOCKS_EXISTS  },
        { 6, FAM_PG_SLRU_FLUSHES        },
        { 7, FAM_PG_SLRU_TRUNCATES      },
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
    if (fields < 8) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_name = PQgetvalue(res, i, 0);

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
                                     &(label_pair_const_t){.name="name", .value=col_name},
                                     NULL);
            }
        }
    }

    PQclear(res);

    return 0;
}
