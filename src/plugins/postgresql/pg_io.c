// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"

#include <libpq-fe.h>

#include "pg_fam.h"

int pg_stat_io(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels)
{
    if (version < 160000)
        return 0;

    char *stmt = "SELECT backend_type, object, context, reads * op_bytes, read_time,"
                 "       writes * op_bytes, write_time, writebacks * op_bytes, writeback_time,"
                 "       extends * op_bytes, extend_time, hits, evictions, reuses,"
                 "       fsyncs, fsync_time"
                 "  FROM pg_stat_io";

    struct {
        int field;
        double scale;
        int fam;
    } pg_fields[] = {
        {  3, 0.0,   FAM_PG_IO_READ_BYTES              },
        {  4, 0.001, FAM_PG_IO_READ_TIME_SECONDS       },
        {  5, 0.0,   FAM_PG_IO_WRITE_BYTES             },
        {  6, 0.001, FAM_PG_IO_WRITE_TIME_SECONDS      },
        {  7, 0.0,   FAM_PG_IO_WRITEBACKS_BYTES        },
        {  8, 0.001, FAM_PG_IO_WRITEBACKS_TIME_SECONDS },
        {  9, 0.0,   FAM_PG_IO_EXTENDS_BYTES           },
        { 10, 0.001, FAM_PG_IO_EXTENDS_TIME_SECONDS    },
        { 11, 0.0,   FAM_PG_IO_HITS                    },
        { 12, 0.0,   FAM_PG_IO_EVICTIONS               },
        { 13, 0.0,   FAM_PG_IO_REUSES                  },
        { 14, 0.0,   FAM_PG_IO_FSYNCS                  },
        { 15, 0.001, FAM_PG_IO_FSYNCS_TIME_SECONDS     },
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
    if (fields < 16) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_backend = PQgetvalue(res, i, 0);

        if (PQgetisnull(res, i, 1))
            continue;
        char *col_object = PQgetvalue(res, i, 1);

        if (PQgetisnull(res, i, 2))
            continue;
        char *col_context = PQgetvalue(res, i, 2);

        for (size_t n = 0; n < pg_fields_size ; n++) {
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
                        value = VALUE_COUNTER(atol(PQgetvalue(res, i, field)));
                    }
                } else {
                    continue;
                }

                metric_family_append(fam, value, labels,
                                     &(label_pair_const_t){.name="backend", .value=col_backend},
                                     &(label_pair_const_t){.name="object", .value=col_object},
                                     &(label_pair_const_t){.name="context", .value=col_context},
                                     NULL);
            }
        }
    }

    PQclear(res);

    return 0;
}
