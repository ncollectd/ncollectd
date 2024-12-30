// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libdbquery/dbquery.h"

#include <sybfront.h>
#include <sybdb.h>

typedef struct {
    char *name;
    char *metric_prefix;
    label_set_t labels;
    plugin_filter_t *filter;

    char *server;
    char *user;
    char *pass;
    char *dbname;

    char *ping_query;

    db_query_preparation_area_t **q_prep_areas;
    db_query_t **queries;
    size_t queries_num;

    DBPROCESS *dbproc;
    unsigned int version;
} mssql_database_t;

static int do_dbinit;

static db_query_t **queries;
static size_t queries_num;
static size_t databases_num;

static int mssql_error(__attribute__((unused)) DBPROCESS *dbproc, int severity, int dberr,
                       int oserr, char *dberrstr, char *oserrstr)
{
    if (oserr) {
        PLUGIN_ERROR("Error: severity(%d)  message: %d:%s  OS error: %d:%s.",
                     severity, dberr, dberrstr, oserr, oserrstr);
    } else {
        PLUGIN_ERROR("Error: severity(%d)  message: %d:%s.", severity, dberr, dberrstr);
    }

    return 2;
}

static int mssql_msg(__attribute__((unused)) DBPROCESS *dbproc, DBINT msgno, int msgstate,
                     int severity, char *msgtext, char *srvname, char *proc, int line)
{
    enum {
        changed_database = 5701,
        changed_language = 5703
    };

    if ((msgno == changed_database) || (msgno == changed_language))
        return 0;

    if (msgno > 0) {
        if (proc != NULL) {
            PLUGIN_INFO("Message: msgno(%d) severity(%d) state(%d) server(%s) "
                        "procedure(%s:%d) message: %s.",
                        msgno, severity, msgstate, srvname, proc, line, msgtext);
        } else {
            PLUGIN_INFO("Message: msgno(%d) severity(%d) state(%d) server(%s) message: %s.",
                        msgno, severity, msgstate, srvname, msgtext);
        }

    }

    return 0;
}

static int mssql_disconnect(mssql_database_t *db)
{
    if (db->dbproc == NULL)
        return 0;

    dbclose(db->dbproc);
    db->dbproc = NULL;
    db->version = 0;

    return 0;
}

static unsigned int mssql_version(mssql_database_t *db)
{
    RETCODE rc = dbcmd(db->dbproc, "SELECT CAST(SERVERPROPERTY('productversion') AS VARCHAR)");
    if (rc == FAIL) {
        PLUGIN_ERROR("dbcmd() failed.");
        return -1;
    }

    rc = dbsqlexec(db->dbproc);
    if (rc == FAIL) {
        PLUGIN_ERROR("dbsqlexec() failed.");
        dbfreebuf(db->dbproc);
        return -1;
    }

    char buffer[256];
    buffer[0] = '\0';
    int status = 0;

    if (dbresults(db->dbproc) != SUCCEED)
        return 0;

    int ncols = dbnumcols(db->dbproc);
    if (ncols != 1) {
        dbcanquery(db->dbproc);
        return 0;
    }

    rc = dbbind(db->dbproc, 1, NTBSTRINGBIND, sizeof(buffer)-1, (BYTE*)buffer);
    if (rc == FAIL) {
        PLUGIN_ERROR("dbbind() failed.");
        dbcanquery(db->dbproc);
        return 0;
    }

    rc = dbnullbind(db->dbproc, 1, &status);
    if (rc == FAIL) {
        PLUGIN_ERROR("dbnullbind() failed.");
        dbcanquery(db->dbproc);
        return 0;
    }

    dbnextrow(db->dbproc);

    dbcanquery(db->dbproc);

    if (buffer[0] == '\0')
        return 0;
    if (status == -1)
        return 0;

    int ndots = 0;
    for (int n = 0; buffer[n] != '\0'; n++) {
        if (buffer[n] == '.')
            ndots++;
    }

    unsigned int mult3[] = { 1, 100, 1000000, 100000000};
    unsigned int mult2[] = { 1, 10000, 1000000};
    unsigned int *mult;
    switch(ndots) {
    case 2:
        mult = mult2;
        break;
    case 3:
        mult = mult3;
        break;
    default:
        return 0;
    }

    unsigned int version = 0;
    char *dot;
    char *start = (char *)buffer;
    int i = 0;
    for (i = 0; ((dot = strrchr(start, '.')) != NULL) && (i < ndots); i++) {
        version += atoi(dot + 1) * mult[i];
        *dot = '\0';
    }

    version += atoi(start) * mult[i];

    return version;
}

static bool mssql_ping(mssql_database_t *db)
{
    if (db->ping_query == NULL)
        return true;

    RETCODE rc = dbcmd(db->dbproc, db->ping_query);
    if (rc == FAIL) {
        PLUGIN_ERROR("dbcmd() failed.");
        return false;
    }

    rc = dbsqlexec(db->dbproc);
    if (rc == FAIL) {
        PLUGIN_ERROR("dbsqlexec() failed.");
        dbfreebuf(db->dbproc);
        return false;
    }

    if (dbresults(db->dbproc) != SUCCEED) {
        dbcanquery(db->dbproc);
        return false;
    }

    dbcanquery(db->dbproc);

    return true;
}

static void mssql_database_free(void *arg)
{
    mssql_database_t *db = arg;
    if (db == NULL)
        return;

    mssql_disconnect(db);

    free(db->name);
    free(db->metric_prefix);
    free(db->server);
    free(db->dbname);
    free(db->user);
    free(db->pass);
    free(db->ping_query);

    label_set_reset(&db->labels);
    plugin_filter_free(db->filter);

    if (db->q_prep_areas)
        for (size_t i = 0; i < db->queries_num; ++i)
            db_query_delete_preparation_area(db->q_prep_areas[i]);
    free(db->q_prep_areas);
    /* N.B.: db->queries references objects "owned" by the global queries
     * variable. Free the array here, but not the content. */
    free(db->queries);

    free(db);
}

static int mssql_read_database_query(mssql_database_t *db, db_query_t *q,
                                     db_query_preparation_area_t *prep_area)
{
    char **column_names = NULL;
    char **column_values = NULL;
    int *column_types = NULL;
    int *column_status = NULL;
    int column_num = 0;

    int status = 0;
    char *statement = db_query_get_statement(q);
    assert(statement != NULL);

    RETCODE rc = dbcmd(db->dbproc, statement);
    if (rc == FAIL) {
        PLUGIN_ERROR("dbcmd() failed.");
        status = -1;
        goto error;
    }

    rc = dbsqlexec(db->dbproc);
    if (rc == FAIL) {
        PLUGIN_ERROR("dbsqlexec() failed.");
        dbfreebuf(db->dbproc);
        status = -1;
        goto error;
    }

    RETCODE rrc = dbresults(db->dbproc);
    if (rrc != SUCCEED) {
        status = -1;
        goto error;
    }

    column_num = dbnumcols(db->dbproc);

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
    for (int i = 1; i < column_num; i++) {
        column_names[i] = column_names[i - 1] + DATA_MAX_NAME_LEN;
    }

    column_types = calloc(column_num, sizeof(*column_types));
    if (column_types == NULL) {
        PLUGIN_ERROR("calloc failed.");
        status = -1;
        goto error;
    }

    column_status = calloc(column_num, sizeof(*column_status));
    if (column_status == NULL) {
        PLUGIN_ERROR("calloc failed.");
        status = -1;
        goto error;
    }

    column_values = calloc(column_num, sizeof(*column_values));
    if (column_values == NULL) {
        PLUGIN_ERROR("calloc failed.");
        status = -1;
        goto error;
    }

    for (int col = 0; col < column_num; col++) {
        char *column_name = column_names[col];

        char *name = dbcolname(db->dbproc, col + 1);
        sstrncpy(column_name, name, DATA_MAX_NAME_LEN);

        int type = dbcoltype(db->dbproc, col + 1);

        int size = dbcollen(db->dbproc, col +1);

        if (type != SYBCHAR) {
            size = dbprcollen(db->dbproc, col + 1);
            if (size > 255)
                size = 255;
        }

        column_types[col] = type;

        char *buffer = calloc(1, size + 1);
        if (buffer == NULL) {
            PLUGIN_ERROR("calloc failed.");
            goto error;
        }

        column_values[col] = buffer;

        rc = dbbind(db->dbproc, col + 1, NTBSTRINGBIND,  size+1, (BYTE*)buffer);
        if (rc == FAIL)
            goto error;

        rc = dbnullbind(db->dbproc, col + 1, &column_status[col]);
        if (rc == FAIL)
            goto error;

    }

    status = db_query_prepare_result(q, prep_area, db->metric_prefix, &db->labels,
                                        db->name, column_names, column_num);

    if (status != 0) {
        PLUGIN_ERROR("db_query_prepare_result failed with status %i.", status);
        goto error;
    }

    /* Iterate over all rows and call `db_query_handle_result' with each list of values. */
    while ((rrc != NO_MORE_RESULTS) && (rrc != FAIL)) {
        int row_code;
        while ((row_code = dbnextrow(db->dbproc)) != NO_MORE_ROWS) {
            if (row_code != REG_ROW)
                continue;

            for (int col = 0; col < column_num; col++) {
                if (column_status[col] == -1)
                    column_values[col][0] = '\0';
            }

            /* If all values were copied successfully, call `db_query_handle_result'
             * to dispatch the row to the daemon. */
            status = db_query_handle_result(q, prep_area, column_values, db->filter);
            if (status != 0) {
                PLUGIN_ERROR("mssql_read_database_query (%s, %s): db_query_handle_result failed.",
                             db->name, db_query_get_name(q));
                goto error;
            }
        }

        rrc = dbresults(db->dbproc);
    }

    /* Tell the db query interface that we're done with this query. */
    db_query_finish_result(q, prep_area);

    status = 0;

error:
    dbcanquery(db->dbproc);

    if (column_names != NULL) {
        free(column_names[0]);
        free(column_names);
    }

    if (column_values != NULL) {
        for (int col = 0; col < column_num; col++) {
            free(column_values[col]);
        }
        free(column_values);
    }

    if (column_types != NULL)
        free(column_types);

    if (column_status != NULL)
        free(column_status );

    return status;
}

static int mssql_connect(mssql_database_t *db)
{
    if (db->dbproc != NULL) {
        if (mssql_ping(db))
            return 0; /* connection is alive */

        mssql_disconnect(db);
    }

    LOGINREC *login = dblogin();
    if (login == NULL) {
        PLUGIN_ERROR("Unable to allocate login structure.");
        return -1;
    }

    DBSETLUSER(login, db->user);
    DBSETLPWD(login, db->pass);

    db->dbproc = dbopen(login, db->server);
    if (db->dbproc == NULL) {
        dbloginfree(login);
        return -1;
    }

    dbloginfree(login);

    if (db->dbname != NULL) {
        RETCODE rc = dbuse(db->dbproc, db->dbname);
        if (rc == FAIL) {
            PLUGIN_ERROR("Unable to use to database %s.", db->dbname);
            return -1;
        }
    }

    db->version = mssql_version(db);

    return 0;
}

static int mssql_read_database(user_data_t *ud)
{
    mssql_database_t *db = (mssql_database_t *)ud->data;

    int status = mssql_connect(db);
    if (status != 0)
        return status;

    int success = 0;
    for (size_t i = 0; i < db->queries_num; i++) {
        /* Check if we know the database's version and if so, if this query applies
         * to that version. */
        if ((db->version != 0) && (db_query_check_version(db->queries[i], db->version) == 0))
            continue;

        status = mssql_read_database_query(db, db->queries[i], db->q_prep_areas[i]);
        if (status == 0)
            success++;
    }

    if (success == 0) {
        PLUGIN_ERROR("All queries failed for database `%s'.", db->name);
        return -1;
    }

    return 0;
}

static int mssql_config_add_database(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The 'instance' block needs exactly one string argument.");
        return -1;
    }

    mssql_database_t *db = calloc(1, sizeof(*db));
    if (db == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &db->name);
    if (status != 0) {
        free(db);
        return status;
    }

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("server", child->key) == 0) {
            status = cf_util_get_string(child, &db->server);
        } else if (strcasecmp("database", child->key) == 0) {
            status = cf_util_get_string(child, &db->dbname);
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
        } else if (strcasecmp("query", child->key) == 0) {
            status = db_query_pick_from_list(child, queries, queries_num,
                                                     &db->queries, &db->queries_num);
        } else if (strcasecmp("ping-query", child->key) == 0) {
            status = cf_util_get_string(child, &db->ping_query);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &db->filter);
        } else {
            PLUGIN_WARNING("Option '%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    /* Check that all necessary options have been given. */
    if (status == 0) {
        if (db->server == NULL) {
            PLUGIN_WARNING("'server' not given for instance '%s'", db->name);
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
        mssql_database_free(db);
        return -1;
    }

    databases_num++;

    return plugin_register_complex_read("mssql", db->name, mssql_read_database, interval,
                                        &(user_data_t){.data=db, .free_func=mssql_database_free });
}

static int mssql_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp("query", child->key) == 0) {
            status = db_query_create(&queries, &queries_num, child, NULL);
        } else if (strcasecmp("instance", child->key) == 0) {
            status = mssql_config_add_database(child);
        } else {
            PLUGIN_ERROR("Unknown config option '%s'.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int mssql_init(void)
{
    static int did_init;

    if (did_init == 0) {
        if (queries_num == 0) {
            PLUGIN_ERROR("No 'query' blocks have been found. Without them, "
                         "this plugin can't do anything useful, so we will return an error.");
            return -1;
        }

        if (databases_num == 0) {
            PLUGIN_ERROR("No 'instance' blocks have been found. Without them, "
                         "this plugin can't do anything useful, so we will return an error.");
            return -1;
        }

        did_init = 1;
    }

    if (do_dbinit == 0) {
        if (dbinit() == FAIL) {
            PLUGIN_ERROR("dbinit() failed.");
            return -1;
        }

        dberrhandle(mssql_error);
        dbmsghandle(mssql_msg);
    }

    do_dbinit++;

    return 0;
}

static int mssql_shutdown(void)
{
    databases_num = 0;
    db_query_free(queries, queries_num);
    queries = NULL;
    queries_num = 0;

    do_dbinit--;
    if (do_dbinit <= 0)
        dbexit();

    return 0;
}

void module_register(void)
{
    plugin_register_config("mssql", mssql_config);
    plugin_register_init("mssql", mssql_init);
    plugin_register_shutdown("mssql", mssql_shutdown);
}
