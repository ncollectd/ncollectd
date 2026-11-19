// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/itoa.h"
#include "libutils/dtoa.h"
#include "libdbquery/dbquery.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <sqlcli1.h>
#pragma GCC diagnostic pop

typedef struct {
    char *name;
    char *metric_prefix;
    label_set_t labels;
    plugin_filter_t *filter;

    char *conn;
    char *alias;
    char *user;
    char *pass;
    cdtime_t timeout;

    db_query_preparation_area_t **q_prep_areas;
    db_query_t **queries;
    size_t queries_num;

    SQLHDBC hdbc;
    SQLHENV henv;
    bool connected;
    unsigned int version;
} db2_database_t;

static db_query_t **queries;
static size_t queries_num;

static int db2_read_database(user_data_t *ud);

static const char *db2_strerror(SQLHANDLE hdl, SQLSMALLINT htype, char *buffer, size_t size)
{
    SQLCHAR sqlstate[SQL_SQLSTATE_SIZE + 1] = {0};
    SQLINTEGER nerror = 0;
    SQLCHAR emsg[4096];
    SQLSMALLINT emsg_size = 0;

    SQLRETURN rc = SQLGetDiagRec(htype, hdl, 1, sqlstate, &nerror,
                                        emsg, sizeof(emsg) - 1, &emsg_size);
    if (SQL_SUCCEEDED(rc)) {
        if ((emsg_size < 0) || (emsg_size > (SQLSMALLINT)(sizeof(emsg) - 1)))
            emsg_size = sizeof(emsg) - 1;
        emsg[emsg_size] = '\0';
        ssnprintf(buffer, size, "SqlState: %s ErrorCode: %d %s", sqlstate, nerror, emsg);
        return buffer;
    }

    buffer[0] = '\0';
    return buffer;
}

static int db2_disconnect(db2_database_t *db)
{
    int status = 0;

    if (db->hdbc != SQL_NULL_HDBC) {
        SQLRETURN rc;
        if (db->connected) {
            rc = SQLDisconnect(db->hdbc);
            if (!SQL_SUCCEEDED(rc)) {
                char errbuf[1024];
                PLUGIN_ERROR("unable to disconnect %s: %s", db->name,
                             db2_strerror(db->hdbc, SQL_HANDLE_DBC, errbuf, sizeof(errbuf)));
                status = -1;
            }
            db->connected = false;
            db->version = 0;
        }

        rc = SQLFreeHandle(SQL_HANDLE_DBC, db->hdbc);
        if (!SQL_SUCCEEDED(rc)) {
            PLUGIN_ERROR("unable to free connection handle %s", db->name);
            status = -1;
        }
        db->hdbc = SQL_NULL_HDBC;
    }

    if (db->henv != SQL_NULL_HENV) {
        SQLRETURN rc = SQLFreeHandle(SQL_HANDLE_ENV, db->henv);
        if (!SQL_SUCCEEDED(rc)) {
            PLUGIN_ERROR("unable to free environment handle %s", db->name);
            status = -1;
        }
        db->henv = SQL_NULL_HENV;
    }

    return status;
}

static SQLULEN db2_timeout(db2_database_t *db)
{
    cdtime_t timeout = db->timeout != 0 ? db->timeout : plugin_get_interval();
    uint64_t seconds = (CDTIME_T_TO_MS(timeout) + 999) / 1000;

    if (seconds < 1)
        return 1;
    /* maximum login timeout accepted by the DB2 CLI */
    if (seconds > 32767)
        return 32767;
    return (SQLULEN)seconds;
}

static void db2_set_query_timeout(db2_database_t *db, SQLHSTMT hstmt)
{
    SQLRETURN rc = SQLSetStmtAttr(hstmt, SQL_ATTR_QUERY_TIMEOUT,
                                  (SQLPOINTER)(uintptr_t)db2_timeout(db), 0);
    if (!SQL_SUCCEEDED(rc)) {
        char errbuf[1024];
        PLUGIN_WARNING("Unable to set the query timeout in %s: %s", db->name,
                       db2_strerror(hstmt, SQL_HANDLE_STMT, errbuf, sizeof(errbuf)));
    }
}

static unsigned int db2_version(db2_database_t *db)
{
    SQLCHAR buffer[256];
    SQLSMALLINT len = 0;

    SQLRETURN rc = SQLGetInfo(db->hdbc, SQL_DBMS_VER, buffer, sizeof(buffer) - 1, &len);
    if (!SQL_SUCCEEDED(rc)) {
        char errbuf[1024];
        PLUGIN_ERROR("SQLGetInfo failed in %s: %s", db->name,
                     db2_strerror(db->hdbc, SQL_HANDLE_DBC, errbuf, sizeof(errbuf)));
        return 0;
    }

    if (len > (SQLSMALLINT)(sizeof(buffer) - 1))
        len = sizeof(buffer) - 1;

    buffer[len] = '\0';

    /* SQL_DBMS_VER has the form mm.vv.rrrr, e.g. 11.05.0800 -> 11050800 */
    unsigned int major = 0, minor = 0, release = 0;
    if (sscanf((char *)buffer, "%u.%u.%u", &major, &minor, &release) != 3) {
        PLUGIN_WARNING("Cannot parse the version '%s' of %s.", (char *)buffer, db->name);
        return 0;
    }

    return major * 1000000 + minor * 10000 + release;
}

static int db2_ping(db2_database_t *db)
{
    static char *ping_query = "SELECT 1 FROM SYSIBM.SYSDUMMY1 WHERE 0=1 WITH UR";

    SQLHSTMT hstmt = SQL_NULL_HSTMT;
    SQLRETURN rc = SQLAllocHandle(SQL_HANDLE_STMT, db->hdbc, &hstmt);
    if (!SQL_SUCCEEDED(rc)) {
        char errbuf[1024];
        PLUGIN_ERROR("SQLAllocHandle STMT failed in %s: %s", db->name,
                     db2_strerror(db->hdbc, SQL_HANDLE_DBC, errbuf, sizeof(errbuf)));
        return 0;
    }

    db2_set_query_timeout(db, hstmt);

    /* coverity[NEGATIVE_RETURNS] */
    rc = SQLExecDirect(hstmt, (UCHAR *)ping_query, SQL_NTS);
    if (!SQL_SUCCEEDED(rc)) {
        char errbuf[1024];
        PLUGIN_ERROR("Error executing ping query in %s: %s", db->name,
                     db2_strerror(hstmt, SQL_HANDLE_STMT, errbuf, sizeof(errbuf)));
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        return 0;
    }

    do {
        rc = SQLMoreResults(hstmt);
    } while (SQL_SUCCEEDED(rc));

    if (hstmt != SQL_NULL_HSTMT)
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

    return 1;
}

static int db2_get_data(SQLHSTMT hstmt, SQLUSMALLINT idx, SQLSMALLINT type,
                                          char *buffer, size_t buffer_size)
{
    SQLSMALLINT ctype = SQL_C_CHAR;
    SQLLEN size = (SQLLEN)buffer_size;

    switch(type) {
    case SQL_XML:
    case SQL_BIT:
    case SQL_BLOB:
    case SQL_VARBINARY:
    case SQL_LONGVARBINARY:
    case SQL_BINARY:
        ctype = SQL_C_BINARY;
        /* binary data is not NUL terminated by SQLGetData */
        size = (SQLLEN)buffer_size - 1;
        break;
    default:
        ctype = SQL_C_CHAR;
        break;
    }

    buffer[0] = '\0';
    SQLLEN ptr = 0;
    SQLRETURN rc = SQLGetData(hstmt, idx, ctype, buffer, size, &ptr);
    /* Values longer than the buffer are truncated (SQL_SUCCESS_WITH_INFO). */
    if (!SQL_SUCCEEDED(rc)) {
        char errbuf[1024];
        PLUGIN_ERROR("SQLGetData failed: %s",
                     db2_strerror(hstmt, SQL_HANDLE_STMT, errbuf, sizeof(errbuf)));
        return -1;
    }

    if (ptr == SQL_NULL_DATA)
        return 1;

    if (ctype == SQL_C_BINARY) {
        /* SQL_NO_TOTAL or truncated: the buffer is full */
        if ((ptr < 0) || (ptr > size))
            ptr = size;
        buffer[ptr] = '\0';
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
    if (!SQL_SUCCEEDED(rc)) {
        char errbuf[1024];
        PLUGIN_ERROR("SQLAllocHandle STMT failed in %s: %s", db->name,
                     db2_strerror(db->hdbc, SQL_HANDLE_DBC, errbuf, sizeof(errbuf)));
        status = -1;
        goto error;
    }

    db2_set_query_timeout(db, hstmt);

    /* coverity[NEGATIVE_RETURNS] */
    rc = SQLExecDirect(hstmt, (UCHAR *)statement, SQL_NTS);
    if (!SQL_SUCCEEDED(rc)) {
        char errbuf[1024];
        PLUGIN_ERROR("SQLExecDirect failed in %s: %s", db->name,
                     db2_strerror(hstmt, SQL_HANDLE_STMT, errbuf, sizeof(errbuf)));
        status = -1;
        goto error;
    } else { /* Get the number of columns */
        SQLSMALLINT columns;

        rc = SQLNumResultCols(hstmt, &columns);
        if (!SQL_SUCCEEDED(rc)) {
            char errbuf[1024];
            PLUGIN_ERROR("db2_read_database_query (%s, %s): SQLNumResultCols failed : %s",
                        db->name, db_query_get_name(q),
                        db2_strerror(hstmt, SQL_HANDLE_STMT, errbuf, sizeof(errbuf)));
            status = -1;
            goto error;
        }

        if (columns <= 0) {
            PLUGIN_ERROR("db2_read_database_query (%s, %s): The statement returned no columns.",
                         db->name, db_query_get_name(q));
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
        if (!SQL_SUCCEEDED(rc)) {
            char errbuf[1024];
            PLUGIN_ERROR("db2_read_database_query (%s, %s): SQLDescribeCol %zu failed : %s",
                          db->name, db_query_get_name(q), i + 1,
                          db2_strerror(hstmt, SQL_HANDLE_STMT, errbuf, sizeof(errbuf)));
            status = -1;
            goto error;
        }
        if ((column_name_len < 0) || (column_name_len >= DATA_MAX_NAME_LEN))
            column_name_len = DATA_MAX_NAME_LEN - 1;
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

        if (!SQL_SUCCEEDED(rc)) {
            char errbuf[1024];
            PLUGIN_ERROR("db2_read_database_query (%s, %s): SQLFetch failed : %s",
                         db->name, db_query_get_name(q),
                         db2_strerror(hstmt, SQL_HANDLE_STMT, errbuf, sizeof(errbuf)));
            status = -1;
            goto error;
        }

        /* Copy the value of the columns to `column_values' */
        bool null_value = false;
        for (size_t i = 0; i < column_num; i++) {
            status = db2_get_data(hstmt, (SQLUSMALLINT)(i + 1), column_types[i],
                                           column_values[i], DATA_MAX_NAME_LEN);

            if (status < 0) {
                PLUGIN_ERROR("db2_read_database_query (%s, %s): "
                              "db2_get_data (%" PRIsz ") \"%s\" failed.",
                              db->name, db_query_get_name(q), i + 1, column_names[i]);
                status = -1;
                goto error;
            }

            if (status > 0) {
                PLUGIN_DEBUG("db2_read_database_query (%s, %s): column %" PRIsz " \"%s\" "
                             "is NULL, skipping the row.",
                             db->name, db_query_get_name(q), i + 1, column_names[i]);
                null_value = true;
                break;
            }
        }

        if (null_value)
            continue;

        /* If all values were copied successfully, call `db_query_handle_result'
         * to dispatch the row to the daemon. */
        status = db_query_handle_result(q, prep_area, column_values, db->filter);
        if (status != 0) {
            PLUGIN_ERROR("db2_read_database_query (%s, %s): db_query_handle_result failed.",
                         db->name, db_query_get_name(q));
            goto error;
        }
    }

    status = 0;

error:
    /* Tell the db query interface that we're done with this query. */
    db_query_finish_result(q, prep_area);

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
    SQLRETURN rc = SQL_SUCCESS;
    int status;

    if (db->hdbc != SQL_NULL_HDBC) {
        status = db2_ping(db);
        if (status != 0) /* connection is alive */
            return 0;
    }

    db2_disconnect(db);

    rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &(db->henv));
    if (!SQL_SUCCEEDED(rc)) {
        PLUGIN_ERROR("db2_connect(%s): Unable to allocate environment handle", db->name);
        return -1;
    }

    /* set options */
    rc = SQLSetEnvAttr(db->henv, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
    if (!SQL_SUCCEEDED(rc)) {
        PLUGIN_ERROR("db2_connect(%s): Unable to set ODBC3 attribute", db->name);
        return -1;
    }

    rc = SQLAllocHandle(SQL_HANDLE_DBC, db->henv, &db->hdbc);
    if (!SQL_SUCCEEDED(rc)) {
        PLUGIN_ERROR("db2_connect(%s): Unable to allocate connection handle", db->name);
        return -1;
    }

    rc = SQLSetConnectAttr(db->hdbc, SQL_ATTR_LOGIN_TIMEOUT,
                           (SQLPOINTER)(uintptr_t)db2_timeout(db), 0);
    if (!SQL_SUCCEEDED(rc)) {
        char errbuf[1024];
        PLUGIN_WARNING("db2_connect(%s): Unable to set the login timeout: %s", db->name,
                       db2_strerror(db->hdbc, SQL_HANDLE_DBC, errbuf, sizeof(errbuf)));
    }

    if (db->conn != NULL) {
        rc = SQLDriverConnect(db->hdbc, NULL, (SQLCHAR *)db->conn, SQL_NTS, NULL, 0, NULL,
                                        SQL_DRIVER_NOPROMPT);
        if (!SQL_SUCCEEDED(rc)) {
            char errbuf[1024];
            PLUGIN_ERROR("db2_connect(%s): SQLDriverConnect failed : %s", db->name,
                         db2_strerror(db->hdbc, SQL_HANDLE_DBC, errbuf, sizeof(errbuf)));
            db2_disconnect(db);
            return -1;
        }
        if (rc == SQL_SUCCESS_WITH_INFO) {
            char errbuf[1024];
            PLUGIN_WARNING("db2_connect(%s): SQLDriverConnect reported the following "
                           "diagnostics: %s", db->name,
                           db2_strerror(db->hdbc, SQL_HANDLE_DBC, errbuf, sizeof(errbuf)));
        }
    } else {
        rc = SQLConnect(db->hdbc, (SQLCHAR *)db->alias, SQL_NTS, (SQLCHAR *)db->user,
                                  SQL_NTS, (SQLCHAR *)db->pass, SQL_NTS);
        if (!SQL_SUCCEEDED(rc)) {
            char errbuf[1024];
            PLUGIN_ERROR("db2_connect(%s): SQLConnect failed: %s", db->name,
                          db2_strerror(db->hdbc, SQL_HANDLE_DBC, errbuf, sizeof(errbuf)));
            db2_disconnect(db);
            return -1;
        }
        if (rc == SQL_SUCCESS_WITH_INFO) {
            char errbuf[1024];
            PLUGIN_WARNING("db2_connect(%s): SQLConnect reported the following diagnostics: %s",
                           db->name, db2_strerror(db->hdbc, SQL_HANDLE_DBC, errbuf, sizeof(errbuf)));
        }
    }

    db->connected = true;
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

    if (db->version == 0)
        db->version = db2_version(db);
    unsigned int db_version = db->version;

    size_t executed = 0;
    int success = 0;
    for (size_t i = 0; i < db->queries_num; i++) {
        /* Check if we know the database's version and if so, if this query applies
         * to that version. */
        if ((db_version != 0) && (db_query_check_version(db->queries[i], db_version) == 0))
            continue;

        executed++;
        status = db2_read_database_query(db, db->queries[i], db->q_prep_areas[i]);
        if (status == 0)
            success++;
    }

    if ((executed > 0) && (success == 0)) {
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
        } else if (strcasecmp("user-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &db->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &db->pass);
        } else if (strcasecmp("password-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &db->pass);
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
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &db->timeout);
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
        if ((db->conn != NULL) && ((db->user != NULL) || (db->pass != NULL)))
            PLUGIN_WARNING("'user' and 'password' are ignored with 'connection' for database "
                           "'%s': use UID and PWD in the connection string.", db->name);
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

    status = label_set_add(&db->labels, true, "instance", db->name);
    if (status != 0) {
        PLUGIN_ERROR("Failed to add the instance label for database '%s'.", db->name);
        db2_database_free(db);
        return -1;
    }

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
