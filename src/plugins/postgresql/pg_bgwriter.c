// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"

#include <libpq-fe.h>

#include "pg_fam.h"

int pg_stat_bgwriter(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels)
{
    if (version < 80300)
        return 0;

    char buffer[512];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);
    char *stmt = NULL;

    if (version < 170000) {
        int status = strbuf_putstr(&buf, "SELECT checkpoints_timed, checkpoints_req"
                                         ", buffers_checkpoint, buffers_clean, maxwritten_clean"
                                         ", buffers_backend, buffers_alloc");

        if (version >= 90100)
            status |= strbuf_putstr(&buf, ", buffers_backend_fsync");
        if (version >= 90200)
            status |= strbuf_putstr(&buf, ", checkpoint_write_time, checkpoint_sync_time");

        status |= strbuf_putstr(&buf, " FROM pg_stat_bgwriter");

        if (status != 0) {
            PLUGIN_ERROR("Failed to create statement.");
            return -1;
        }

        stmt = buf.ptr;
    } else {
        stmt = "SELECT buffers_clean, maxwritten_clean, buffers_alloc FROM pg_stat_bgwriter";
    }

    struct {
        int minversion;
        int maxversion;
        double scale;
        int fam;
    } pg_fields[] = {
        { 80300, 170000, 0.0,   FAM_PG_CHECKPOINTS_TIMED              },
        { 80300, 170000, 0.0,   FAM_PG_CHECKPOINTS_REQ                },
        { 80300, 170000, 0.0,   FAM_PG_CHECKPOINT_BUFFERS             },
        { 80300, 990000, 0.0,   FAM_PG_BGWRITER_BUFFERS_CLEAN         },
        { 80300, 990000, 0.0,   FAM_PG_BGWRITER_MAXWRITTEN_CLEAN      },
        { 80300, 170000, 0.0,   FAM_PG_BGWRITER_BUFFERS_BACKEND       },
        { 80300, 990000, 0.0,   FAM_PG_BGWRITER_BUFFERS_ALLOC         },
        { 90100, 170000, 0.0,   FAM_PG_BGWRITER_BUFFERS_BACKEND_FSYNC },
        { 90200, 170000, 0.001, FAM_PG_CHECKPOINT_WRITE_TIME_SECONDS  },
        { 90200, 170000, 0.001, FAM_PG_CHECKPOINT_SYNC_TIME_SECONDS   },
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

    int expected = 7;
    if (version >= 90100)
        expected = 8;
    if (version >= 90200)
        expected = 10;
    if (version >= 170000)
        expected = 3;

    int fields = PQnfields(res);
    if (fields < expected) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        int field = -1;
        for (size_t n = 0; n < pg_fields_size ; n++) {
            if (version < pg_fields[n].minversion)
                continue;
            if (version >= pg_fields[n].maxversion)
                continue;
            field++;
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

                metric_family_append(fam, value, labels, NULL);
            }
        }
    }

    PQclear(res);

    return 0;
}

int pg_stat_checkpointer(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels)
{
    if (version < 170000)
        return 0;

    char *stmt = "SELECT num_timed, num_requested, write_time, sync_time, buffers_written"
                 " FROM pg_stat_checkpointer";

    struct {
        int field;
        double scale;
        int fam;
    } pg_fields[] = {
        { 0, 0.0,   FAM_PG_CHECKPOINTS_TIMED              },
        { 1, 0.0,   FAM_PG_CHECKPOINTS_REQ                },
        { 2, 0.001, FAM_PG_CHECKPOINT_WRITE_TIME_SECONDS  },
        { 3, 0.001, FAM_PG_CHECKPOINT_SYNC_TIME_SECONDS   },
        { 4, 0.0,   FAM_PG_CHECKPOINT_BUFFERS             },
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
    if (fields < 5) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
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

                metric_family_append(fam, value, labels, NULL);
            }
        }
    }

    PQclear(res);

    return 0;
}
