// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"

#include <libpq-fe.h>

#include "pg_fam.h"

int pg_stat_user_table(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                     char *schema, char *table)
{
    if (version < 70200)
        return 0;

    char buffer[1024];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);

    int stmt_params = 0;
    const char *param_values[2] = {NULL, NULL};
    int param_lengths[2] = {0, 0};
    int param_formats[2] = {0, 0};

    strbuf_putstr(&buf, "SELECT current_database() dbname, schemaname, relname, seq_scan,"
                        "       seq_tup_read, idx_scan, idx_tup_fetch, n_tup_ins, n_tup_upd,"
                        "       n_tup_del, "
                        "       EXTRACT(epoch from COALESCE(last_vacuum, '1970-01-01Z')),"
                        "       EXTRACT(epoch from COALESCE(last_autovacuum, '1970-01-01Z')),"
                        "       EXTRACT(epoch from COALESCE(last_analyze, '1970-01-01Z')),"
                        "       EXTRACT(epoch from COALESCE(last_autoanalyze, '1970-01-01Z'))");
    if (version >= 80300)
        strbuf_putstr(&buf, ", n_tup_hot_upd, n_live_tup, n_dead_tup");
    if (version >= 90100)
        strbuf_putstr(&buf, ", vacuum_count, autovacuum_count, analyze_count, autoanalyze_count");
    if (version >= 90400)
        strbuf_putstr(&buf, ", n_mod_since_analyze");
    if (version >= 130000)
        strbuf_putstr(&buf, ", n_ins_since_vacuum");
    if (version >= 160000)
        strbuf_putstr(&buf, ", EXTRACT(epoch from COALESCE(last_seq_scan, '1970-01-01Z'))"
                            ", EXTRACT(epoch from COALESCE(last_idx_scan, '1970-01-01Z'))"
                            ", n_tup_newpage_upd");

    strbuf_putstr(&buf, " FROM pg_stat_user_tables");

    if ((schema == NULL) && (table == NULL)) {
        /* all tables */
    } else if ((schema != NULL) && (table == NULL)) {
        strbuf_putstr(&buf, " WHERE schemaname = $1");
        stmt_params = 1;
        param_values[0] = schema;
        param_lengths[0] = strlen(schema);
    } else if ((schema != NULL) && (table != NULL)) {
        strbuf_putstr(&buf, " WHERE schemaname = $1 and relname = $2");
        stmt_params = 2;
        param_values[0] = schema;
        param_lengths[0] = strlen(schema);
        param_values[1] = table;
        param_lengths[1] = strlen(table);
    } else {
        return 0;
    }

    char *stmt = buf.ptr;

    struct {
        int field;
        int minversion;
        int fam;
    } pg_fields[] = {
        {  3,  70200, FAM_PG_TABLE_SEQ_SCAN                    },
        {  4,  70200, FAM_PG_TABLE_SEQ_READ_ROWS               },
        {  5,  70200, FAM_PG_TABLE_IDX_SCAN                    },
        {  6,  70200, FAM_PG_TABLE_IDX_FETCH_ROWS              },
        {  7,  70200, FAM_PG_TABLE_INSERTED_ROWS               },
        {  8,  70200, FAM_PG_TABLE_UPDATED_ROWS                },
        {  9,  70200, FAM_PG_TABLE_DELETED_ROWS                },
        { 10,  70200, FAM_PG_TABLE_LAST_VACUUM_SECONDS         },
        { 11,  70200, FAM_PG_TABLE_LAST_AUTOVACUUM_SECONDS     },
        { 12,  70200, FAM_PG_TABLE_LAST_ANALYZE_SECONDS        },
        { 13,  70200, FAM_PG_TABLE_LAST_AUTOANALYZE_SECONDS    },
        { 14,  80300, FAM_PG_TABLE_HOT_UPDATED_ROWS            },
        { 15,  80300, FAM_PG_TABLE_LIVE_ROWS                   },
        { 16,  80300, FAM_PG_TABLE_DEAD_ROWS                   },
        { 17,  90100, FAM_PG_TABLE_VACUUM                      },
        { 18,  90100, FAM_PG_TABLE_AUTOVACUUM                  },
        { 19,  90100, FAM_PG_TABLE_ANALYZE                     },
        { 20,  90100, FAM_PG_TABLE_AUTOANALYZE                 },
        { 21,  90400, FAM_PG_TABLE_MODIFIED_SINCE_ANALYZE_ROWS },
        { 22, 130000, FAM_PG_TABLE_INSERTED_SINCE_VACUUM_ROWS  },
        { 23, 160000, FAM_PG_TABLE_LAST_SEQ_SCAN_SECONDS       },
        { 24, 160000, FAM_PG_TABLE_LAST_IDX_SCAN_SECONDS       },
        { 25, 160000, FAM_PG_TABLE_NEWPAGE_UPDATED_ROWS        },
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

    int expected = 14;
    if (version >= 80300)
        expected = 17;
    if (version >= 90100)
        expected = 21;
    if (version >= 90400)
        expected = 22;
    if (version >= 130000)
        expected = 23;
    if (version >= 160000)
        expected = 26;

    if (fields < expected) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_database = PQgetvalue(res, i, 0);

        if (PQgetisnull(res, i, 1))
            continue;
        char *col_schema = PQgetvalue(res, i, 1);

        if (PQgetisnull(res, i, 2))
            continue;
        char *col_table = PQgetvalue(res, i, 2);

        for (size_t n = 0; n < pg_fields_size ; n++) {
            if (version < pg_fields[n].minversion)
                continue;
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
                                     &(label_pair_const_t){.name="schema", .value=col_schema},
                                     &(label_pair_const_t){.name="table", .value=col_table},
                                     NULL);
            }
        }
    }

    PQclear(res);

    return 0;
}

int pg_statio_user_tables(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                        char *schema, char *table)
{
    if (version < 70200)
        return 0;

    char buffer[512];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);

    int stmt_params = 0;
    const char *param_values[2] = {NULL, NULL};
    int param_lengths[2] = {0, 0};
    int param_formats[2] = {0, 0};

    strbuf_putstr(&buf, "SELECT current_database() dbname, schemaname, relname, heap_blks_read,"
                        "       heap_blks_hit, idx_blks_read, idx_blks_hit, toast_blks_read,"
                        "       toast_blks_hit, tidx_blks_read, tidx_blks_hit"
                        "  FROM pg_statio_user_tables");

    if ((schema == NULL) && (table == NULL)) {
        /* all tables */
    } else if ((schema != NULL) && (table == NULL)) {
        strbuf_putstr(&buf, " WHERE schemaname = $1");
        stmt_params = 1;
        param_values[0] = schema;
        param_lengths[0] = strlen(schema);
    } else if ((schema != NULL) && (table != NULL)) {
        strbuf_putstr(&buf, " WHERE schemaname = $1 and relname = $2");
        stmt_params = 2;
        param_values[0] = schema;
        param_lengths[0] = strlen(schema);
        param_values[1] = table;
        param_lengths[1] = strlen(table);
    } else {
        return 0;
    }

    char *stmt = buf.ptr;

    struct {
        int field;
        int fam;
    } pg_fields[] = {
        {  3, FAM_PG_TABLE_HEAP_READ_BLOCKS  },
        {  4, FAM_PG_TABLE_HEAP_HIT_BLOCKS   },
        {  5, FAM_PG_TABLE_IDX_READ_BLOCKS   },
        {  6, FAM_PG_TABLE_IDX_HIT_BLOCKS    },
        {  7, FAM_PG_TABLE_TOAST_READ_BLOCKS },
        {  8, FAM_PG_TABLE_TOAST_HIT_BLOCKS  },
        {  9, FAM_PG_TABLE_TIDX_READ_BLOCKS  },
        { 10, FAM_PG_TABLE_TIDX_HIT_BLOCKS   },
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

    if (fields < 11) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_database = PQgetvalue(res, i, 0);

        if (PQgetisnull(res, i, 1))
            continue;
        char *col_schema = PQgetvalue(res, i, 1);

        if (PQgetisnull(res, i, 2))
            continue;
        char *col_table = PQgetvalue(res, i, 2);

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
                                     &(label_pair_const_t){.name="schema", .value=col_schema},
                                     &(label_pair_const_t){.name="table", .value=col_table},
                                     NULL);
            }
        }
    }

    PQclear(res);

    return 0;
}

int pg_table_size(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                char *schema, char *table)
{
    if (version < 90000)
        return 0;

    char buffer[512];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);
    strbuf_putstr(&buf, "SELECT current_database() dbname, table_schema, table_name, "
                        "       pg_total_relation_size('\"'||table_schema||'\".\"'||table_name||'\"') total_relation_size,"
                        "       pg_indexes_size('\"'||table_schema||'\".\"'||table_name||'\"') indexes_size"
                        "  FROM information_schema.tables "
                        " WHERE table_type = 'BASE TABLE' ");

    int stmt_params = 0;
    const char *param_values[2] = {NULL, NULL};
    int param_lengths[2] = {0, 0};
    int param_formats[2] = {0, 0};

    if ((schema == NULL) && (table == NULL)) {
        /* all tables */
    } else if ((schema != NULL) && (table == NULL)) {
        strbuf_putstr(&buf, "AND table_schema = $1");
        stmt_params = 1;
        param_values[0] = schema;
        param_lengths[0] = strlen(schema);
    } else if ((schema != NULL) && (table != NULL)) {
        strbuf_putstr(&buf, "AND table_schema = $1 AND table_name = $2");
        stmt_params = 2;
        param_values[0] = schema;
        param_lengths[0] = strlen(schema);
        param_values[1] = table;
        param_lengths[1] = strlen(table);
    } else {
        return 0;
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

    if (fields < 5) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_database = PQgetvalue(res, i, 0);

        if (PQgetisnull(res, i, 1))
            continue;
        char *col_schema = PQgetvalue(res, i, 1);

        if (PQgetisnull(res, i, 2))
            continue;
        char *col_table = PQgetvalue(res, i, 2);

        if (!PQgetisnull(res, i, 3))
            metric_family_append(&fams[FAM_PG_TABLE_SIZE_BYTES],
                                 VALUE_GAUGE(atof(PQgetvalue(res, i, 3))), labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="schema", .value=col_schema},
                                 &(label_pair_const_t){.name="table", .value=col_table},
                                 NULL);

        if (!PQgetisnull(res, i, 4))
            metric_family_append(&fams[FAM_PG_TABLE_INDEXES_SIZE_BYTES],
                                 VALUE_GAUGE(atof(PQgetvalue(res, i, 4))), labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="schema", .value=col_schema},
                                 &(label_pair_const_t){.name="table", .value=col_table},
                                 NULL);
    }

    PQclear(res);

    return 0;

}

int pg_stat_user_functions(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                         char *schema, char *function)
{
    if (version < 80400)
        return 0;

    char buffer[512];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);

    int stmt_params = 0;
    const char *param_values[2] = {NULL, NULL};
    int param_lengths[2] = {0, 0};
    int param_formats[2] = {0, 0};

    strbuf_putstr(&buf, "SELECT current_database() dbname, schemaname, funcname, calls,"
                        "       total_time, self_time"
                        "  FROM pg_stat_user_functions");

    if ((schema == NULL) && (function == NULL)) {
        /* all functions */
    } else if ((schema != NULL) && (function == NULL)) {
        strbuf_putstr(&buf, " WHERE schemaname = $1");
        stmt_params = 1;
        param_values[0] = schema;
        param_lengths[0] = strlen(schema);
    } else if ((schema != NULL) && (function != NULL)) {
        strbuf_putstr(&buf, " WHERE schemaname = $1 and funcname = $2");
        stmt_params = 2;
        param_values[0] = schema;
        param_lengths[0] = strlen(schema);
        param_values[1] = function;
        param_lengths[1] = strlen(function);
    } else {
        return 0;
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

    if (fields < 6) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_database = PQgetvalue(res, i, 0);

        if (PQgetisnull(res, i, 1))
            continue;
        char *col_schema = PQgetvalue(res, i, 1);

        if (PQgetisnull(res, i, 2))
            continue;
        char *col_function = PQgetvalue(res, i, 2);

        if (!PQgetisnull(res, i, 3))
            metric_family_append(&fams[FAM_PG_FUNCTION_CALLS],
                                 VALUE_COUNTER( atol(PQgetvalue(res, i, 3))), labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="schema", .value=col_schema},
                                 &(label_pair_const_t){.name="function", .value=col_function},
                                 NULL);

        if (!PQgetisnull(res, i, 4))
            metric_family_append(&fams[FAM_PG_FUNCTION_TOTAL_TIME_SECONDS],
                                 VALUE_COUNTER_FLOAT64(((double)atol(PQgetvalue(res, i, 4))/1000.0)),
                                 labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="schema", .value=col_schema},
                                 &(label_pair_const_t){.name="function", .value=col_function},
                                 NULL);

        if (!PQgetisnull(res, i, 5))
            metric_family_append(&fams[FAM_PG_FUNCTION_SELF_TIME_SECONDS],
                                 VALUE_COUNTER_FLOAT64(((double)atol(PQgetvalue(res, i, 5))/1000.0)),
                                 labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="schema", .value=col_schema},
                                 &(label_pair_const_t){.name="function", .value=col_function},
                                 NULL);
    }

    PQclear(res);

    return 0;
}

int pg_stat_user_indexes(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                       char *schema, char *table, char *index)
{
    if (version < 70200)
        return 0;

    char buffer[512];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);

    int stmt_params = 0;
    const char *param_values[3] = {NULL, NULL, NULL};
    int param_lengths[3] = {0, 0, 0};
    int param_formats[3] = {0, 0, 0};

    strbuf_putstr(&buf, "SELECT current_database() dbname, schemaname, relname, indexrelname,"
                        "       idx_scan, idx_tup_read, idx_tup_fetch"
                        "  FROM pg_stat_user_indexes");

    if ((schema == NULL) && (table == NULL) && (index == NULL)) {
        /* all indexes */
    } else if ((schema != NULL) && (table == NULL) && (index == NULL)) {
        strbuf_putstr(&buf, " WHERE schemaname = $1");
        stmt_params = 1;
        param_values[0] = schema;
        param_lengths[0] = strlen(schema);
    } else if ((schema != NULL) && (table != NULL) && (index == NULL)) {
        strbuf_putstr(&buf, " WHERE schemaname = $1 and relname = $2");
        stmt_params = 2;
        param_values[0] = schema;
        param_lengths[0] = strlen(schema);
        param_values[1] = table;
        param_lengths[1] = strlen(table);
    } else if ((schema != NULL) && (table != NULL) && (index != NULL)) {
        strbuf_putstr(&buf, " WHERE schemaname = $1 and relname = $2 and indexrelname =$3");
        stmt_params = 3;
        param_values[0] = schema;
        param_lengths[0] = strlen(schema);
        param_values[1] = table;
        param_lengths[1] = strlen(table);
        param_values[2] = index;
        param_lengths[2] = strlen(index);
    } else {
        return 0;
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
    if (fields < 7) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_database = PQgetvalue(res, i, 0);

        if (PQgetisnull(res, i, 1))
            continue;
        char *col_schema = PQgetvalue(res, i, 1);

        if (PQgetisnull(res, i, 2))
            continue;
        char *col_table = PQgetvalue(res, i, 2);

        if (PQgetisnull(res, i, 3))
            continue;
        char *col_index = PQgetvalue(res, i, 3);

        if (!PQgetisnull(res, i, 4))
            metric_family_append(&fams[FAM_PG_INDEX_IDX_SCAN],
                                 VALUE_COUNTER(atol(PQgetvalue(res, i, 4))), labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="schema", .value=col_schema},
                                 &(label_pair_const_t){.name="table", .value=col_table},
                                 &(label_pair_const_t){.name="index", .value=col_index},
                                 NULL);

        if (!PQgetisnull(res, i, 5))
            metric_family_append(&fams[FAM_PG_INDEX_IDX_READ_ROWS],
                                 VALUE_COUNTER(atol(PQgetvalue(res, i, 5))), labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="schema", .value=col_schema},
                                 &(label_pair_const_t){.name="table", .value=col_table},
                                 &(label_pair_const_t){.name="index", .value=col_index},
                                 NULL);

        if (!PQgetisnull(res, i, 6))
            metric_family_append(&fams[FAM_PG_INDEX_IDX_FETCH_ROWS],
                                 VALUE_COUNTER(atol(PQgetvalue(res, i, 6))), labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="schema", .value=col_schema},
                                 &(label_pair_const_t){.name="table", .value=col_table},
                                 &(label_pair_const_t){.name="index", .value=col_index},
                                 NULL);
    }

    PQclear(res);

    return 0;
}

int pg_statio_user_indexes(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                         char *schema, char *table, char *index)
{
    if (version < 70200)
        return 0;

    char buffer[512];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);

    int stmt_params = 0;
    const char *param_values[3] = {NULL, NULL, NULL};
    int param_lengths[3] = {0, 0, 0};
    int param_formats[3] = {0, 0, 0};

    strbuf_putstr(&buf, "SELECT current_database() dbname, schemaname, relname, indexrelname,"
                        "       idx_blks_read, idx_blks_hit"
                        "  FROM pg_statio_user_indexes");

    if ((schema == NULL) && (table == NULL) && (index == NULL)) {
        /* all indexes */
    } else if ((schema != NULL) && (table == NULL) && (index == NULL)) {
        strbuf_putstr(&buf, " WHERE schemaname = $1");
        stmt_params = 1;
        param_values[0] = schema;
        param_lengths[0] = strlen(schema);
    } else if ((schema != NULL) && (table != NULL) && (index == NULL)) {
        strbuf_putstr(&buf, " WHERE schemaname = $1 and relname = $2");
        stmt_params = 2;
        param_values[0] = schema;
        param_lengths[0] = strlen(schema);
        param_values[1] = table;
        param_lengths[1] = strlen(table);
    } else if ((schema != NULL) && (table != NULL) && (index != NULL)) {
        strbuf_putstr(&buf, " WHERE schemaname = $1 and relname = $2 and indexrelname =$3");
        stmt_params = 3;
        param_values[0] = schema;
        param_lengths[0] = strlen(schema);
        param_values[1] = table;
        param_lengths[1] = strlen(table);
        param_values[2] = index;
        param_lengths[2] = strlen(index);
    } else {
        return 0;
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
    if (fields < 6) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_database = PQgetvalue(res, i, 0);

        if (PQgetisnull(res, i, 1))
            continue;
        char *col_schema = PQgetvalue(res, i, 1);

        if (PQgetisnull(res, i, 2))
            continue;
        char *col_table = PQgetvalue(res, i, 2);

        if (PQgetisnull(res, i, 3))
            continue;
        char *col_index = PQgetvalue(res, i, 3);

        if (!PQgetisnull(res, i, 4))
            metric_family_append(&fams[FAM_PG_INDEX_IDX_READ_BLOCKS],
                                 VALUE_COUNTER(atol(PQgetvalue(res, i, 4))), labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="schema", .value=col_schema},
                                 &(label_pair_const_t){.name="table", .value=col_table},
                                 &(label_pair_const_t){.name="index", .value=col_index},
                                 NULL);

        if (!PQgetisnull(res, i, 5))
            metric_family_append(&fams[FAM_PG_INDEX_IDX_HIT_BLOCKS],
                                 VALUE_COUNTER(atol(PQgetvalue(res, i, 5))), labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="schema", .value=col_schema},
                                 &(label_pair_const_t){.name="table", .value=col_table},
                                 &(label_pair_const_t){.name="index", .value=col_index},
                                 NULL);
    }

    PQclear(res);

    return 0;
}

int pg_statio_user_sequences(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                           char *schema, char *sequence)
{
    if (version < 70200)
        return 0;

    char buffer[512];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);

    int stmt_params = 0;
    const char *param_values[2] = {NULL, NULL};
    int param_lengths[2] = {0, 0};
    int param_formats[2] = {0, 0};

    strbuf_putstr(&buf, "SELECT current_database() dbname, schemaname, relname,"
                        "       blks_read, blks_hit"
                        "  FROM pg_statio_user_sequences");

    if ((schema == NULL) && (sequence == NULL)) {
        /* all schemmas */
    } else if ((schema != NULL) && (sequence == NULL)) {
        strbuf_putstr(&buf, " WHERE schemaname = $1");
        stmt_params = 1;
        param_values[0] = schema;
        param_lengths[0] = strlen(schema);
    } else if ((schema != NULL) && (sequence != NULL)) {
        strbuf_putstr(&buf, " WHERE schemaname = $1 and relname = $2");
        stmt_params = 2;
        param_values[0] = schema;
        param_lengths[0] = strlen(schema);
        param_values[1] = sequence;
        param_lengths[1] = strlen(sequence);
    } else {
        return 0;
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
    if (fields < 5) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *col_database = PQgetvalue(res, i, 0);

        if (PQgetisnull(res, i, 1))
            continue;
        char *col_schema = PQgetvalue(res, i, 1);

        if (PQgetisnull(res, i, 2))
            continue;
        char *col_sequence = PQgetvalue(res, i, 2);

        if (!PQgetisnull(res, i, 3))
            metric_family_append(&fams[FAM_PG_SEQUENCES_READ_BLOCKS],
                                 VALUE_COUNTER(atol(PQgetvalue(res, i, 3))), labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="schema", .value=col_schema},
                                 &(label_pair_const_t){.name="sequence", .value=col_sequence},
                                 NULL);

        if (!PQgetisnull(res, i, 4))
            metric_family_append(&fams[FAM_PG_SEQUENCES_HIT_BLOCKS],
                                 VALUE_COUNTER(atol(PQgetvalue(res, i, 4))), labels,
                                 &(label_pair_const_t){.name="database", .value=col_database},
                                 &(label_pair_const_t){.name="schema", .value=col_schema},
                                 &(label_pair_const_t){.name="sequence", .value=col_sequence},
                                 NULL);
    }

    PQclear(res);

    return 0;
}
