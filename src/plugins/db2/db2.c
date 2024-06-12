// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/itoa.h"
#include "libutils/dtoa.h"
#include "libdbquery/dbquery.h"

#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <sqlcli1.h>

typedef struct {
    char *name;
    char *metric_prefix;
    label_set_t labels;
    plugin_filter_t *filter;

    char *conn;
    char *alias;
    char *user;
    char *pass;

    db_query_preparation_area_t **q_prep_areas;
    db_query_t **queries;
    size_t queries_num;

    SQLHDBC hdbc;
    SQLHENV henv;
} db2_database_t;

static db_query_t **queries;
static size_t queries_num;

static int db2_read_database(user_data_t *ud);

static const char *db2_strerror(SQLHANDLE hdl, SQLSMALLINT htype, char *buffer, size_t size)
{
    SQLCHAR sqlstate[6];
    SQLINTEGER nerror;
    SQLCHAR emsg[4096];
    SQLSMALLINT emsg_size = 0;

    SQLRETURN rc = SQLGetDiagRec(htype, hdl, 1, sqlstate, &nerror,
                                        emsg, sizeof(emsg) - 1, &emsg_size);
    if (rc != SQL_NO_DATA_FOUND) {
        emsg[emsg_size] = '\0';
        ssnprintf(buffer, size, "SqlState: %s ErrorCode: %d  %s\n", sqlstate, nerror, emsg);
        return buffer;
    }

    buffer[0] = '\0';
    return buffer;
}

static int db2_disconnect(db2_database_t *db)
{
    if (db->hdbc != SQL_NULL_HDBC) {
        SQLRETURN rc = SQLDisconnect(db->hdbc);
        if (rc != SQL_SUCCESS) {
            char errbuf[1024];
            PLUGIN_ERROR("unable to disconnect %s: %s", db->name,
                         db2_strerror(db->hdbc, SQL_HANDLE_DBC, errbuf, sizeof(errbuf)));
            return -1;
        }

        rc = SQLFreeHandle(SQL_HANDLE_DBC, db->hdbc);
        if (rc != SQL_SUCCESS) {
            PLUGIN_ERROR("unable to free connection handle %s", db->name);
            return -1;
        }
        db->hdbc = SQL_NULL_HDBC;
    }

    if (db->henv != SQL_NULL_HENV) {
        SQLRETURN rc = SQLFreeHandle(SQL_HANDLE_ENV, db->henv);
        if (rc != SQL_SUCCESS) {
            PLUGIN_ERROR("unable to free environment handle %s", db->name);
            return -1;
        }
        db->henv = SQL_NULL_HENV;
    }

    return 0;
}

static unsigned int db2_version(db2_database_t *db)
{
    SQLCHAR buffer[256];
    SQLSMALLINT len = 0;

    SQLRETURN rc = SQLGetInfo(db->hdbc, SQL_DBMS_VER, buffer, sizeof(buffer) - 1, &len);
    if (rc != SQL_SUCCESS) {
        char errbuf[1024];
        PLUGIN_ERROR("SQLGetInfo failed in %s: %s", db->name,
                     db2_strerror(db->hdbc, SQL_HANDLE_DBC, errbuf, sizeof(errbuf)));
        return 0;
    }

    if (len > (SQLSMALLINT)(sizeof(buffer) - 1))
        len = sizeof(buffer) - 1;

    buffer[len] = '\0';

    unsigned int version = 0;
    unsigned int mult = 1;
    char *dot;
    char *start = (char *)buffer;
    int i = 0;
    for (i = 0; (dot = strrchr(start, '.')) != NULL && i < 5; i++) {
        version += atoi(dot + 1) * mult;
        *dot = '\0';
        mult *= 100;
    }
    version += atoi(start) * mult;

    if (i == 5)
        return 0;

    return version;
}

static int db2_ping(db2_database_t *db)
{
    static char *ping_query = "SELECT 1 FROM SYSIBM.SYSDUMMY1 WHERE 0=1 WITH UR";

    SQLHSTMT hstmt = SQL_NULL_HSTMT;
    SQLRETURN rc = SQLAllocHandle(SQL_HANDLE_STMT, db->hdbc, &hstmt);
    if ((rc != SQL_SUCCESS) && (rc != SQL_SUCCESS_WITH_INFO)) {
        char errbuf[1024];
        PLUGIN_ERROR("SQLAllocHandle STMT failed in %s: %s", db->name,
                     db2_strerror(db->hdbc, SQL_HANDLE_DBC, errbuf, sizeof(errbuf)));
        return 0;
    }

    /* coverity[NEGATIVE_RETURNS] */
    rc = SQLExecDirect(hstmt, (UCHAR *)ping_query, SQL_NTS);
    if ((rc != SQL_SUCCESS) && (rc != SQL_SUCCESS_WITH_INFO)) {
        char errbuf[1024];
        PLUGIN_ERROR("Error executing ping query in %s: %s", db->name,
                     db2_strerror(hstmt, SQL_HANDLE_STMT, errbuf, sizeof(errbuf)));
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        return 0;
    }

    while (SQLMoreResults(hstmt) != SQL_NO_DATA)
        ;

    if (hstmt != SQL_NULL_HSTMT)
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

    return 1;
}

static int db2_get_data(SQLHSTMT hstmt, SQLUSMALLINT idx, SQLSMALLINT type,
                                          char *buffer, size_t buffer_size)
{
    SQLSMALLINT ctype = SQL_C_CHAR;

    switch(type) {
    case SQL_XML:
    case SQL_BIT:
    case SQL_BLOB:
    case SQL_VARBINARY:
    case SQL_LONGVARBINARY:
    case SQL_BINARY:
        ctype = SQL_C_BINARY;
        break;
    default:
        ctype = SQL_C_CHAR;
        break;
    }

    buffer[0] = '\0';
    SQLLEN ptr;
    SQLRETURN rc = SQLGetData(hstmt, idx, ctype, buffer, buffer_size, &ptr);
    if (rc != SQL_SUCCESS) {
        char errbuf[1024];
        PLUGIN_ERROR("SQLGetData failed: %s",
                     db2_strerror(hstmt, SQL_HANDLE_STMT, errbuf, sizeof(errbuf)));
        return -1;
    }

    return 0;
}

static void db2_database_free(void *arg)
{
    db2_database_t *db = arg;
    if (db == NULL)
        return;

    db2_disconnect(db);

    free(db->name);
    free(db->metric_prefix);
    free(db->conn);
    free(db->alias);
    free(db->user);
    free(db->pass);

    label_set_reset(&db->labels);
    plugin_filter_free(db->filter);

    if (db->q_prep_areas != NULL) {
        for (size_t i = 0; i < db->queries_num; i++) {
            db_query_delete_preparation_area(db->q_prep_areas[i]);
        }
        free(db->q_prep_areas);
    }
    /* N.B.: db->queries references objects "owned" by the global queries
     * variable. Free the array here, but not the content. */
    free(db->queries);

    free(db);
}

static int db2_read_database_query(db2_database_t *db, db_query_t *q,
                                     db_query_preparation_area_t *prep_area)
{
    size_t column_num;
    char **column_names = NULL;
    char **column_values = NULL;
    SQLSMALLINT *column_types = NULL;
    SQLHSTMT hstmt = SQL_NULL_HSTMT;
    SQLRETURN rc;

    int status = 0;

    char *statement = db_query_get_statement(q);
    assert(statement != NULL);

    rc = SQLAllocHandle(SQL_HANDLE_STMT, db->hdbc, &hstmt);
    if ((rc != SQL_SUCCESS) && (rc != SQL_SUCCESS_WITH_INFO)) {
        char errbuf[1024];
        PLUGIN_ERROR("SQLAllocHandle STMT failed in %s: %s", db->name,
                     db2_strerror(db->hdbc, SQL_HANDLE_DBC, errbuf, sizeof(errbuf)));
        status = -1;
        goto error;
    }

    /* coverity[NEGATIVE_RETURNS] */
    rc = SQLExecDirect(hstmt, (UCHAR *)statement, SQL_NTS);
    if ((rc != SQL_SUCCESS) && (rc != SQL_SUCCESS_WITH_INFO)) {
        char errbuf[1024];
        PLUGIN_ERROR("SQLExecDirect failed in %s: %s", db->name,
                     db2_strerror(hstmt, SQL_HANDLE_STMT, errbuf, sizeof(errbuf)));
        status = -1;
        goto error;
    } else { /* Get the number of columns */
        SQLSMALLINT columns;

        rc = SQLNumResultCols(hstmt, &columns);
        if (rc != SQL_SUCCESS) {
            char errbuf[1024];
            PLUGIN_ERROR("db2_read_database_query (%s, %s): SQLNumResultCols failed : %s",
                        db->name, db_query_get_name(q),
                        db2_strerror(hstmt, SQL_HANDLE_STMT, errbuf, sizeof(errbuf)));
            status = -1;
            goto error;
        }

        column_num = (size_t)columns;
        PLUGIN_DEBUG("db2_read_database_query (%s, %s): There are %" PRIsz " columns.",
                      db->name, db_query_get_name(q), column_num);
    }

    /* Allocate `column_names' and `column_values'. */
    column_names = calloc(column_num, sizeof(*column_names));
    if (column_names == NULL) {
        PLUGIN_ERROR("calloc failed.");
        status = -1;
        goto error;
    }

    column_names[0] = calloc(column_num, DATA_MAX_NAME_LEN);
    if (column_names[0] == NULL) {
        PLUGIN_ERROR("calloc failed.");
        status = -1;
        goto error;
    }
    for (size_t i = 1; i < column_num; i++) {
        column_names[i] = column_names[i - 1] + DATA_MAX_NAME_LEN;
    }

    column_values = calloc(column_num, sizeof(*column_values));
    if (column_values == NULL) {
        PLUGIN_ERROR("calloc failed.");
        status = -1;
        goto error;
    }

    column_values[0] = calloc(column_num, DATA_MAX_NAME_LEN);
    if (column_values[0] == NULL) {
        PLUGIN_ERROR("calloc failed.");
        status = -1;
        goto error;
    }
    for (size_t i = 1; i < column_num; i++)
        column_values[i] = column_values[i - 1] + DATA_MAX_NAME_LEN;

    column_types = calloc(column_num, sizeof(SQLSMALLINT));
    if (column_types == NULL) {
        PLUGIN_ERROR("calloc failed.");
        status = -1;
        goto error;
    }

    for (size_t i = 0; i < column_num; i++) {
        SQLSMALLINT column_name_len = 0;
        SQLCHAR *column_name = (SQLCHAR *)column_names[i];

        rc = SQLDescribeCol(hstmt, (SQLUSMALLINT)(i + 1), column_name,
                                   DATA_MAX_NAME_LEN, &column_name_len, &(column_types[i]),
                                   NULL, NULL, NULL);
        if (rc != SQL_SUCCESS) {
            char errbuf[1024];
            PLUGIN_ERROR("db2_read_database_query (%s, %s): SQLDescribeCol %zu failed : %s",
                          db->name, db_query_get_name(q), i + 1,
                          db2_strerror(hstmt, SQL_HANDLE_STMT, errbuf, sizeof(errbuf)));
            status = -1;
            goto error;
        }
        column_names[i][column_name_len] = '\0';
    }

    status = db_query_prepare_result(q, prep_area, db->metric_prefix, &db->labels,
                                        db->name, column_names, column_num);

    if (status != 0) {
        PLUGIN_ERROR("db_query_prepare_result failed with status %i.", status);
        goto error;
    }

    /* Iterate over all rows and call `db_query_handle_result' with each list of
     * values. */
    while (true) {
        rc = SQLFetch(hstmt);
        if (rc == SQL_NO_DATA) {
            break;
        }

        if (rc != SQL_SUCCESS) {
            char errbuf[1024];
            PLUGIN_ERROR("db2_read_database_query (%s, %s): SQLFetch failed : %s",
                         db->name, db_query_get_name(q),
                         db2_strerror(hstmt, SQL_HANDLE_STMT, errbuf, sizeof(errbuf)));
            status = -1;
            goto error;
        }

        /* Copy the value of the columns to `column_values' */
        for (size_t i = 0; i < column_num; i++) {
            status = db2_get_data(hstmt, (SQLUSMALLINT)(i + 1), column_types[i],
                                           column_values[i], DATA_MAX_NAME_LEN);

            if (status != 0) {
                PLUGIN_ERROR("db2_read_database_query (%s, %s): "
                              "db2_result_get_field (%" PRIsz ") \"%s\" failed.",
                              db->name, db_query_get_name(q), i + 1, column_names[i]);
                status = -1;
                goto error;
            }
        }

        /* If all values were copied successfully, call `db_query_handle_result'
         * to dispatch the row to the daemon. */
        status = db_query_handle_result(q, prep_area, column_values, db->filter);
        if (status != 0) {
            PLUGIN_ERROR("db2_read_database_query (%s, %s): db_query_handle_result failed.",
                         db->name, db_query_get_name(q));
            goto error;
        }
    }

    /* Tell the db query interface that we're done with this query. */
    db_query_finish_result(q, prep_area);

    status = 0;

error:

    if (column_names != NULL) {
        free(column_names[0]);
        free(column_names);
    }

    if (column_values != NULL) {
        free(column_values[0]);
        free(column_values);
    }

    if (column_types != NULL) {
        free(column_types);
    }

    if (hstmt != SQL_NULL_HSTMT) {
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    }

    return status;
}

static int db2_connect(db2_database_t *db)
{
    SQLCHAR buffer[256];
    SQLSMALLINT len;
    SQLRETURN rc = SQL_SUCCESS;
    int status;

    if (db->hdbc != SQL_NULL_HDBC) {
        status = db2_ping(db);
        if (status != 0) /* connection is alive */
            return 0;

        db2_disconnect(db);
    }

    rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &(db->henv));
    if (rc != SQL_SUCCESS) {
        PLUGIN_ERROR("db2_connect(%s): Unable to allocate environment handle", db->name);
        return -1;
    }

    /* set options */
    rc = SQLSetEnvAttr(db->henv, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
    if (rc != SQL_SUCCESS) {
        PLUGIN_ERROR("db2_connect(%s): Unable to set ODBC3 attribute", db->name);
        return -1;
    }

    rc = SQLAllocHandle(SQL_HANDLE_DBC, db->henv, &db->hdbc);
    if (rc != SQL_SUCCESS) {
        PLUGIN_ERROR("db2_connect(%s): Unable to allocate connection handle", db->name);
        return -1;
    }

    if (db->conn != NULL) {
        rc = SQLDriverConnect(db->hdbc, NULL, (SQLCHAR *)db->conn, SQL_NTS, buffer,
                                        sizeof(buffer), &len, SQL_DRIVER_COMPLETE);
        if (rc == SQL_SUCCESS_WITH_INFO) {
            buffer[len] = '\0';
            PLUGIN_WARNING("db2_connect(%s): SQLDriverConnect "
                           "reported the following diagnostics: %s", db->name, buffer);
            return 0;
        }
        if (rc != SQL_SUCCESS) {
            char errbuf[1024];
            PLUGIN_ERROR("db2_connect(%s): SQLDriverConnect failed : %s", db->name,
                         db2_strerror(db->hdbc, SQL_HANDLE_DBC, errbuf, sizeof(errbuf)));
            db2_disconnect(db);
            return -1;
        }
    } else {
        rc = SQLConnect(db->hdbc, (SQLCHAR *)db->alias, SQL_NTS, (SQLCHAR *)db->user,
                                  SQL_NTS, (SQLCHAR *)db->pass, SQL_NTS);
        if (rc != SQL_SUCCESS) {
            char errbuf[1024];
            PLUGIN_ERROR("db2_connect(%s): SQLConnect failed: %s", db->name,
                          db2_strerror(db->hdbc, SQL_HANDLE_DBC, errbuf, sizeof(errbuf)));
            db2_disconnect(db);
            return -1;
        }
    }

    return 0;
}

static int db2_read_database(user_data_t *ud)
{
    metric_family_t fam_up = {
        .name = "db2_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the DB2 server be reached.",
    };

    db2_database_t *db = (db2_database_t *)ud->data;

    int status = db2_connect(db);
    if (status != 0) {
        metric_family_append(&fam_up, VALUE_GAUGE(0), &db->labels, NULL);
        plugin_dispatch_metric_family(&fam_up, 0);
        return status;
    }
    assert(db->alias != NULL || db->conn != NULL);

    metric_family_append(&fam_up, VALUE_GAUGE(1), &db->labels, NULL);
    plugin_dispatch_metric_family(&fam_up, 0);

    unsigned int db_version = db2_version(db);

    int success = 0;
    for (size_t i = 0; i < db->queries_num; i++) {
        /* Check if we know the database's version and if so, if this query applies
         * to that version. */
        if ((db_version != 0) && (db_query_check_version(db->queries[i], db_version) == 0))
            continue;

        status = db2_read_database_query(db, db->queries[i], db->q_prep_areas[i]);
        if (status == 0)
            success++;
    }

    if (success == 0) {
        PLUGIN_ERROR("All queries failed for database `%s'.", db->name);
        return -1;
    }

    return 0;
}

static int db2_config_add_database(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The 'instance' block needs exactly one string argument.");
        return -1;
    }

    db2_database_t *db = calloc(1, sizeof(*db));
    if (db == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    db->hdbc = SQL_NULL_HDBC;
    db->henv = SQL_NULL_HENV;

    int status = cf_util_get_string(ci, &db->name);
    if (status != 0) {
        free(db);
        return status;
    }

    /* Fill the `db2_database_t' structure.. */
    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("alias", child->key) == 0) {
            status = cf_util_get_string(child, &db->alias);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &db->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &db->pass);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &db->labels);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &db->metric_prefix);
        } else if (strcasecmp("connection", child->key) == 0) {
            status = cf_util_get_string(child, &db->conn);
        } else if (strcasecmp("query", child->key) == 0) {
            status = db_query_pick_from_list(child, queries, queries_num,
                                                     &db->queries, &db->queries_num);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
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
    if (status == 0) {
        if ((db->alias == NULL) && (db->conn == NULL)) {
            PLUGIN_WARNING("'alias' or 'connection' not given for database '%s'", db->name);
            status = -1;
        }
        if ((db->alias != NULL) && (db->conn != NULL)) {
            PLUGIN_WARNING("Only 'alias' or 'connection' can be given for database '%s'", db->name);
            status = -1;
        }
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

    if (status != 0) {
        db2_database_free(db);
        return -1;
    }

    label_set_add(&db->labels, true, "instance", db->name);

    return plugin_register_complex_read("db2", db->name, db2_read_database, interval,
                                        &(user_data_t){.data=db, .free_func=db2_database_free });
}

static int db2_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp("query", child->key) == 0) {
            status = db_query_create(&queries, &queries_num, child, /* callback = */ NULL);
        } else if (strcasecmp("instance", child->key) == 0) {
            status = db2_config_add_database(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int db2_shutdown(void)
{
    db_query_free(queries, queries_num);
    queries = NULL;
    queries_num = 0;

    return 0;
}

void module_register(void)
{
    plugin_register_config("db2", db2_config);
    plugin_register_shutdown("db2", db2_shutdown);
}
