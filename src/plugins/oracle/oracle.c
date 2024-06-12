// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2008,2009  noris network AG
// SPDX-FileCopyrightText: Copyright (C) 2012 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libdbquery/dbquery.h"

#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <oci.h>

typedef struct {
    char *name;
    char *host;
    char *connect_id;
    char *username;
    char *password;

    char *metric_prefix;
    label_set_t labels;
    plugin_filter_t *filter;

    db_query_preparation_area_t **q_prep_areas;
    db_query_t **queries;
    size_t queries_num;

    OCISvcCtx *oci_service_context;
    OCIEnv *oci_env;
    OCIError *oci_error;
} o_database_t;

static db_query_t **queries;
static size_t queries_num;

static void o_report_error(const char *where, const char *db_name, const char *query_name,
                                              const char *what, OCIError *eh)
{
    char buffer[2048];
    sb4 error_code;
    int status;

    if (db_name == NULL)
        db_name = "(none)";
    if (query_name == NULL)
        query_name = "(none)";

    /* An operation may cause / return multiple errors. Loop until we have
     * handled all errors available (with a fail-save limit of 16). */
    for (unsigned int record_number = 1; record_number <= 16; record_number++) {
        memset(buffer, 0, sizeof(buffer));
        error_code = -1;

        status = OCIErrorGet(eh, (ub4)record_number, /* sqlstate = */ NULL, &error_code,
                                 (text *)&buffer[0], (ub4)sizeof(buffer), OCI_HTYPE_ERROR);
        buffer[sizeof(buffer) - 1] = '\0';

        if (status == OCI_NO_DATA)
            return;

        if (status == OCI_SUCCESS) {
            size_t buffer_length;

            buffer_length = strlen(buffer);
            while ((buffer_length > 0) && (buffer[buffer_length - 1] < 32)) {
                buffer_length--;
                buffer[buffer_length] = 0;
            }

            PLUGIN_ERROR("%s (db = %s, query = %s): %s failed: %s", where,
                         db_name, query_name, what, buffer);
        } else {
            PLUGIN_ERROR("%s (db = %s, query = %s): %s failed. "
                         "Additionally, OCIErrorGet failed with status %i.",
                         where, db_name, query_name, what, status);
            return;
        }
    }
}

static void o_database_free(void *arg)
{
    o_database_t *db = arg;
    if (db == NULL)
        return;

    free(db->name);
    free(db->connect_id);
    free(db->username);
    free(db->password);
    free(db->queries);

    free(db->metric_prefix);
    label_set_reset(&db->labels);
    plugin_filter_free(db->filter);

    if (db->oci_service_context != NULL)
        OCIHandleFree(db->oci_service_context, OCI_HTYPE_SVCCTX);

    for (size_t i = 0; i < db->queries_num; i++) {
        OCIStmt *oci_statement = db_query_preparation_area_get_user_data(db->q_prep_areas[i]);
        if (oci_statement != NULL) {
            OCIHandleFree(oci_statement, OCI_HTYPE_STMT);
            db_query_preparation_area_set_user_data(db->q_prep_areas[i], NULL);
        }
        db_query_delete_preparation_area(db->q_prep_areas[i]);
    }
    free(db->q_prep_areas);

    if (db->oci_env != NULL)
        OCIHandleFree(db->oci_env, OCI_HTYPE_ENV);

    free(db);
}

static int o_read_database_query(o_database_t *db,
                                 db_query_t *q,
                                 db_query_preparation_area_t *prep_area)
{
    int status;

    OCIStmt *oci_statement = db_query_preparation_area_get_user_data(prep_area);

    /* Prepare the statement */
    if (oci_statement == NULL) {
        const char *statement = db_query_get_statement(q);
        assert(statement != NULL);

        status = OCIHandleAlloc(db->oci_env, (void *)&oci_statement, OCI_HTYPE_STMT,
                                /* user_data_size = */ 0, /* user_data = */ NULL);
        if (status != OCI_SUCCESS) {
            o_report_error("o_read_database_query", db->name, db_query_get_name(q),
                           "OCIHandleAlloc", db->oci_error);
            oci_statement = NULL;
            return -1;
        }

        status = OCIStmtPrepare(oci_statement, db->oci_error, (const text *)statement,
                                (ub4)strlen(statement), OCI_NTV_SYNTAX, OCI_DEFAULT);
        if (status != OCI_SUCCESS) {
            o_report_error("o_read_database_query", db->name, db_query_get_name(q),
                           "OCIStmtPrepare", db->oci_error);
            OCIHandleFree(oci_statement, OCI_HTYPE_STMT);
            oci_statement = NULL;
            return -1;
        }
        db_query_preparation_area_set_user_data(prep_area, oci_statement);

        PLUGIN_DEBUG("o_read_database_query (%s, %s): Successfully allocated statement handle.",
                     db->name, db_query_get_name(q));
    }

    assert(oci_statement != NULL);

    /* Execute the statement */
    status = OCIStmtExecute(db->oci_service_context, oci_statement, db->oci_error,
                            /* iters = */ 0, /* rowoff = */ 0,
                            /* snap_in = */ NULL, /* snap_out = */ NULL,
                            /* mode = */ OCI_DEFAULT);
    if (status != OCI_SUCCESS) {
        o_report_error("o_read_database_query", db->name, db_query_get_name(q),
                       "OCIStmtExecute", db->oci_error);
        return -1;
    }

    size_t column_num = 0;
    /* Acquire the number of columns returned. */
    do {
        ub4 param_counter = 0;
        status = OCIAttrGet(oci_statement, OCI_HTYPE_STMT,
                                           &param_counter, /* size pointer = */ NULL,
                                           OCI_ATTR_PARAM_COUNT, db->oci_error);
        if (status != OCI_SUCCESS) {
            o_report_error("o_read_database_query", db->name, db_query_get_name(q),
                           "OCIAttrGet", db->oci_error);
            return -1;
        }

        column_num = (size_t)param_counter;
    } while (0);

/* Allocate the following buffers:
 *
 *  +---------------+-----------------------------------+
 *  ! Name          ! Size                              !
 *  +---------------+-----------------------------------+
 *  ! column_names  ! column_num x DATA_MAX_NAME_LEN    !
 *  ! column_values ! column_num x DATA_MAX_NAME_LEN    !
 *  ! oci_defines   ! column_num x sizeof (OCIDefine *) !
 *  +---------------+-----------------------------------+
 *
 */
#define NUMBER_BUFFER_SIZE 64

#define FREE_ALL                  \
    if (column_names != NULL) {   \
        free(column_names[0]);   \
        free(column_names);      \
    }                             \
    if (column_values != NULL) {  \
        free(column_values[0]);  \
        free(column_values);     \
    }                             \
    free(oci_defines)

#define ALLOC_OR_FAIL(ptr, ptr_size)                               \
    do {                                                           \
        size_t alloc_size = (size_t)((ptr_size));                  \
        (ptr) = calloc(1, alloc_size);                             \
        if ((ptr) == NULL) {                                       \
            FREE_ALL;                                              \
            PLUGIN_ERROR("o_read_database_query: calloc failed."); \
            return -1;                                             \
        }                                                          \
    } while (0)

    /* Initialize everything to NULL so the above works. */
    char **column_names = NULL;
    char **column_values = NULL;
    OCIDefine **oci_defines = NULL;

    ALLOC_OR_FAIL(column_names, column_num * sizeof(char *));
    ALLOC_OR_FAIL(column_names[0], column_num * DATA_MAX_NAME_LEN);
    for (size_t i = 1; i < column_num; i++)
        column_names[i] = column_names[i - 1] + DATA_MAX_NAME_LEN;

    ALLOC_OR_FAIL(column_values, column_num * sizeof(char *));
    ALLOC_OR_FAIL(column_values[0], column_num * DATA_MAX_NAME_LEN);
    for (size_t i = 1; i < column_num; i++)
        column_values[i] = column_values[i - 1] + DATA_MAX_NAME_LEN;

    ALLOC_OR_FAIL(oci_defines, column_num * sizeof(OCIDefine *));
    /* End of buffer allocations. */

    /* ``Define'' the returned data, i. e. bind the columns to the buffers allocated above. */
    for (size_t i = 0; i < column_num; i++) {
        char *column_name;
        ub4 column_name_length;
        OCIParam *oci_param;

        oci_param = NULL;

        status = OCIParamGet(oci_statement, OCI_HTYPE_STMT, db->oci_error,
                                                 (void *)&oci_param, (ub4)(i + 1));
        if (status != OCI_SUCCESS) {
            /* This is probably alright */
            PLUGIN_DEBUG("o_read_database_query: status = %#x (= %i);", (unsigned int)status, status);
            o_report_error("o_read_database_query", db->name, db_query_get_name(q),
                           "OCIParamGet", db->oci_error);
            break;
        }

        column_name = NULL;
        column_name_length = 0;
        status = OCIAttrGet(oci_param, OCI_DTYPE_PARAM, &column_name,
                                       &column_name_length, OCI_ATTR_NAME, db->oci_error);
        if (status != OCI_SUCCESS) {
            OCIDescriptorFree(oci_param, OCI_DTYPE_PARAM);
            o_report_error("o_read_database_query", db->name, db_query_get_name(q),
                           "OCIAttrGet (OCI_ATTR_NAME)", db->oci_error);
            continue;
        }

        OCIDescriptorFree(oci_param, OCI_DTYPE_PARAM);
        oci_param = NULL;

        /* Copy the name to column_names. Warning: The ``string'' returned by OCI
         * may not be null terminated! */
        memset(column_names[i], 0, DATA_MAX_NAME_LEN);
        if (column_name_length >= DATA_MAX_NAME_LEN)
            column_name_length = DATA_MAX_NAME_LEN - 1;
        memcpy(column_names[i], column_name, column_name_length);
        column_names[i][column_name_length] = 0;

        PLUGIN_DEBUG("o_read_database_query: column_names[%" PRIsz "] = %s;"
                     " column_name_length = %" PRIu32 ";",
                     i, column_names[i], (uint32_t)column_name_length);

        status = OCIDefineByPos(oci_statement, &oci_defines[i], db->oci_error,
                                               (ub4)(i + 1), column_values[i], DATA_MAX_NAME_LEN,
                                               SQLT_STR, NULL, NULL, NULL, OCI_DEFAULT);
        if (status != OCI_SUCCESS) {
            o_report_error("o_read_database_query", db->name, db_query_get_name(q),
                           "OCIDefineByPos", db->oci_error);
            continue;
        }
    }

    status = db_query_prepare_result(q, prep_area, db->metric_prefix, &db->labels,
                                        db->name, column_names, column_num);
    if (status != 0) {
        PLUGIN_ERROR("o_read_database_query (%s, %s): db_query_prepare_result failed.",
                     db->name, db_query_get_name(q));
        FREE_ALL;
        return -1;
    }

    /* Fetch and handle all the rows that matched the query. */
    while (true) {
        status = OCIStmtFetch2(oci_statement, db->oci_error,
                               /* nrows = */ 1, /* orientation = */ OCI_FETCH_NEXT,
                               /* fetch offset = */ 0, /* mode = */ OCI_DEFAULT);
        if (status == OCI_NO_DATA) {
            break;
        } else if ((status != OCI_SUCCESS) && (status != OCI_SUCCESS_WITH_INFO)) {
            o_report_error("o_read_database_query", db->name, db_query_get_name(q),
                           "OCIStmtFetch2", db->oci_error);
            break;
        }

        status = db_query_handle_result(q, prep_area, column_values, db->filter);
        if (status != 0) {
            PLUGIN_WARNING("o_read_database_query (%s, %s): db_query_handle_result failed.",
                            db->name, db_query_get_name(q));
        }
    }

    db_query_finish_result(q, prep_area);

    /* DEBUG ("oracle plugin: o_read_database_query: This statement succeeded: %s", q->statement); */
    FREE_ALL;

    return 0;
#undef FREE_ALL
#undef ALLOC_OR_FAIL
}

static int o_read_database(user_data_t *ud)
{
    o_database_t *db = ud->data;

    int status;

    if (db->oci_service_context != NULL) {
        OCIServer *server_handle;
        ub4 connection_status;

        server_handle = NULL;
        status = OCIAttrGet((void *)db->oci_service_context, OCI_HTYPE_SVCCTX,
                            (void *)&server_handle, /* size pointer = */ NULL,
                            OCI_ATTR_SERVER, db->oci_error);
        if (status != OCI_SUCCESS) {
            o_report_error("o_read_database", db->name, NULL, "OCIAttrGet", db->oci_error);
            return -1;
        }

        if (server_handle == NULL) {
            connection_status = OCI_SERVER_NOT_CONNECTED;
        } else {
            connection_status = 0;
            status = OCIAttrGet((void *)server_handle, OCI_HTYPE_SERVER,
                                (void *)&connection_status, /* size pointer = */ NULL,
                                OCI_ATTR_SERVER_STATUS, db->oci_error);
            if (status != OCI_SUCCESS) {
                o_report_error("o_read_database", db->name, NULL, "OCIAttrGet", db->oci_error);
                return -1;
            }
        }

        if (connection_status != OCI_SERVER_NORMAL) {
            PLUGIN_INFO("Connection to %s lost. Trying to reconnect.", db->name);
            OCIHandleFree(db->oci_service_context, OCI_HTYPE_SVCCTX);
            db->oci_service_context = NULL;
        }
    }

    if (db->oci_service_context == NULL) {
        status = OCILogon(db->oci_env, db->oci_error, &db->oci_service_context,
                          (OraText *)db->username, (ub4)strlen(db->username),
                          (OraText *)db->password, (ub4)strlen(db->password),
                          (OraText *)db->connect_id, (ub4)strlen(db->connect_id));
        if ((status != OCI_SUCCESS) && (status != OCI_SUCCESS_WITH_INFO)) {
            char errfunc[256];

            ssnprintf(errfunc, sizeof(errfunc), "OCILogon(\"%s\")", db->connect_id);

            o_report_error("o_read_database", db->name, NULL, errfunc, db->oci_error);
            PLUGIN_DEBUG("OCILogon (%s): db->oci_service_context = %p;",
                         db->connect_id, (void *)db->oci_service_context);
            db->oci_service_context = NULL;
            return -1;
        } else if (status == OCI_SUCCESS_WITH_INFO) {
            /* TODO: Print NOTIFY message. */
        }
        assert(db->oci_service_context != NULL);
    }

    PLUGIN_DEBUG("o_read_database: db->connect_id = %s; db->oci_service_context = %p;",
                  db->connect_id, (void *)db->oci_service_context);

    for (size_t i = 0; i < db->queries_num; i++)
        o_read_database_query(db, db->queries[i], db->q_prep_areas[i]);

    return 0;
}

/*
 * plugin oracle {
 *   Query "qinstance0" {
 *       statement "SELECT name, value FROM table"
 *       result {
 *           Type "gauge"
 *           InstancesFrom "name"
 *           ValuesFrom "value"
 *       }
 *   }
 *
 *   database "instance1" {
 *       ConnectID "db01"
 *       Username "oracle"
 *       Password "secret"
 *       Query "qinstance0"
 *   }
 * }
 */

static int o_config_add_database(config_item_t *ci)
{
    o_database_t *db;
    int status;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The 'instance' block needs exactly one string argument.");
        return -1;
    }

    db = calloc(1, sizeof(*db));
    if (db == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }
    db->name = NULL;
    db->connect_id = NULL;
    db->username = NULL;
    db->password = NULL;

    status = cf_util_get_string(ci, &db->name);
    if (status != 0) {
        free(db);
        return status;
    }

    cdtime_t interval = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("connect-id", child->key) == 0) {
            status = cf_util_get_string(child, &db->connect_id);
        } else if (strcasecmp("username", child->key) == 0) {
            status = cf_util_get_string(child, &db->username);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &db->password);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &db->labels);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &db->metric_prefix);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("query", child->key) == 0) {
            status = db_query_pick_from_list(child, queries, queries_num,
                                                    &db->queries, &db->queries_num);

        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &db->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    /* Check that all necessary options have been given. */
    while (status == 0) {
        if (db->connect_id == NULL) {
            PLUGIN_WARNING("'connect-id' not given for query `%s'", db->name);
            status = -1;
        }
        if (db->username == NULL) {
            PLUGIN_WARNING("'username' not given for query `%s'", db->name);
            status = -1;
        }
        if (db->password == NULL) {
            PLUGIN_WARNING("'password' not given for query `%s'", db->name);
            status = -1;
        }

        break;
    }

    while ((status == 0) && (db->queries_num > 0)) {
        db->q_prep_areas = calloc(db->queries_num, sizeof(*db->q_prep_areas));
        if (db->q_prep_areas == NULL) {
            PLUGIN_WARNING("calloc failed");
            status = -1;
            break;
        }

        for (size_t i = 0; i < db->queries_num; ++i) {
            db->q_prep_areas[i] = db_query_allocate_preparation_area(db->queries[i]);

            if (db->q_prep_areas[i] == NULL) {
                PLUGIN_WARNING("db_query_allocate_preparation_area failed");
                status = -1;
                break;
            }
        }

        break;
    }

    if ((status == 0) && (db->oci_env == NULL)) {
        status = OCIEnvCreate(&db->oci_env, /* mode           = */ OCI_THREADED,
                                            /* context        = */ NULL,
                                            /* malloc         = */ NULL,
                                            /* realloc        = */ NULL,
                                            /* free           = */ NULL,
                                            /* user_data_size = */ 0,
                                            /* user_data_ptr  = */ NULL);
        if (status != 0) {
            PLUGIN_ERROR("OCIEnvCreate failed with status %i.", status);
        } else {
            status = OCIHandleAlloc(db->oci_env, (void *)&db->oci_error, OCI_HTYPE_ERROR, 0, NULL);
            if (status != OCI_SUCCESS) {
                PLUGIN_ERROR("OCIHandleAlloc (OCI_HTYPE_ERROR) failed with status %i.", status);
            }
        }
    }

    if (status != 0) {
        o_database_free(db);
        return -1;
    }

    return plugin_register_complex_read("oracle", db->name, o_read_database, interval,
                                        &(user_data_t){.data=db, .free_func=o_database_free });
}

static int o_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("query", child->key) == 0) {
            status = db_query_create(&queries, &queries_num, child, NULL);
        } else if (strcasecmp("instance", child->key) == 0) {
            status = o_config_add_database(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (queries_num > 0) {
            PLUGIN_DEBUG("o_config: queries_num = %" PRIsz "; queries[0] = %p",
                         queries_num, (void *)queries[0]);
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int o_shutdown(void)
{
    db_query_free(queries, queries_num);
    queries = NULL;
    queries_num = 0;

    return 0;
}

void module_register(void)
{
    plugin_register_config("oracle", o_config);
    plugin_register_shutdown("oracle", o_shutdown);
}
