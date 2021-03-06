// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2008-2012  Sebastian Harl
// Copyright (C) 2009       Florian Forster
// Authors:
//   Sebastian Harl <sh at tokkee.org>
//   Florian Forster <octo at collectd.org>

/*
 * This module collects PostgreSQL database statistics.
 */

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"
#include "utils/strbuf/strbuf.h"

#include "utils/db_query/db_query.h"
#include "utils_cache.h"
#include "utils_complain.h"

#include <libpq-fe.h>
#include <pg_config_manual.h>

#define log_err(...) ERROR("postgresql: " __VA_ARGS__)
#define log_warn(...) WARNING("postgresql: " __VA_ARGS__)
#define log_info(...) INFO("postgresql: " __VA_ARGS__)
#define log_debug(...) DEBUG("postgresql: " __VA_ARGS__)

#ifndef C_PSQL_DEFAULT_CONF
#define C_PSQL_DEFAULT_CONF PKGDATADIR "/postgresql_default.conf"
#endif

/* Appends the (parameter, value) pair to the string
 * pointed to by 'buf' suitable to be used as argument
 * for PQconnectdb(). If value equals NULL, the pair
 * is ignored. */
#define C_PSQL_PAR_APPEND(buf, buf_len, parameter, value)                      \
  if ((0 < (buf_len)) && (NULL != (value)) && ('\0' != *(value))) {            \
    int s = ssnprintf(buf, buf_len, " %s = '%s'", parameter, value);           \
    if (0 < s) {                                                               \
      buf += s;                                                                \
      buf_len -= s;                                                            \
    }                                                                          \
  }

/* Returns the tuple (major, minor, patchlevel)
 * for the given version number. */
#define C_PSQL_SERVER_VERSION3(server_version)                                 \
  (server_version) / 10000,                                                    \
      (server_version) / 100 - (int)((server_version) / 10000) * 100,          \
      (server_version) - (int)((server_version) / 100) * 100

/* Returns true if the given host specifies a
 * UNIX domain socket. */
#define C_PSQL_IS_UNIX_DOMAIN_SOCKET(host)                                     \
  ((NULL == (host)) || ('\0' == *(host)) || ('/' == *(host)))

/* Returns the tuple (host, delimiter, port) for a
 * given (host, port) pair. Depending on the value of
 * 'host' a UNIX domain socket or a TCP socket is
 * assumed. */
#define C_PSQL_SOCKET3(host, port)                                             \
  ((NULL == (host)) || ('\0' == *(host))) ? DEFAULT_PGSOCKET_DIR : host,       \
      C_PSQL_IS_UNIX_DOMAIN_SOCKET(host) ? "/.s.PGSQL." : ":", port

typedef enum {
  C_PSQL_PARAM_HOST = 1,
  C_PSQL_PARAM_DB,
  C_PSQL_PARAM_USER,
  C_PSQL_PARAM_INTERVAL,
  C_PSQL_PARAM_INSTANCE,
} c_psql_param_t;

/* Parameter configuration. Stored as `user data' in the query objects. */
typedef struct {
  c_psql_param_t *params;
  int params_num;
} c_psql_user_data_t;

typedef struct {
  PGconn *conn;
  c_complain_t conn_complaint;

  int proto_version;
  int server_version;

  int max_params_num;

  /* user configuration */
  udb_query_preparation_area_t **q_prep_areas;
  udb_query_t **queries;
  size_t queries_num;

  /* make sure we don't access the database object in parallel */
  pthread_mutex_t db_lock;

  char *host;
  char *port;
  char *database;
  char *user;
  char *password;

  char *instance;

  char *metric_prefix;
  label_set_t labels;

  char *sslmode;

  char *krbsrvname;

  char *service;

  int ref_cnt;
} c_psql_database_t;

static c_psql_database_t **databases;
static size_t databases_num;

static udb_query_t **queries;
static size_t queries_num;

static c_psql_database_t *c_psql_database_new(const char *name)
{
  c_psql_database_t **tmp;
  c_psql_database_t *db;

  db = calloc(1, sizeof(*db));
  if (NULL == db) {
    log_err("Out of memory.");
    return NULL;
  }

  tmp = realloc(databases, (databases_num + 1) * sizeof(*databases));
  if (NULL == tmp) {
    log_err("Out of memory.");
    sfree(db);
    return NULL;
  }

  databases = tmp;
  databases[databases_num] = db;
  ++databases_num;

  db->conn = NULL;

  C_COMPLAIN_INIT(&db->conn_complaint);

  pthread_mutex_init(&db->db_lock, /* attrs = */ NULL);

  db->database = sstrdup(name);
  db->instance = sstrdup(name);

  return db;
} 

static void c_psql_database_delete(void *data)
{
  c_psql_database_t *db = data;

  --db->ref_cnt;
  /* readers and writers may access this database */
  if (db->ref_cnt > 0)
    return;

  /* wait for the lock to be released by the last writer */
  pthread_mutex_lock(&db->db_lock);

  if (db->next_commit > 0)
    c_psql_commit(db);

  PQfinish(db->conn);
  db->conn = NULL;

  if (db->q_prep_areas)
    for (size_t i = 0; i < db->queries_num; ++i)
      udb_query_delete_preparation_area(db->q_prep_areas[i]);
  free(db->q_prep_areas);

  sfree(db->queries);
  db->queries_num = 0;

  pthread_mutex_unlock(&db->db_lock);

  pthread_mutex_destroy(&db->db_lock);

  sfree(db->database);
  sfree(db->host);
  sfree(db->port);
  sfree(db->user);
  sfree(db->password);

  sfree(db->instance);

  sfree(db->sslmode);

  sfree(db->krbsrvname);

  sfree(db->service);

  sfree(db->metric_prefix);
  label_set_reset(&db->labels);

  /* don't care about freeing or reordering the 'databases' array
   * this is done in 'shutdown'; also, don't free the database instance
   * object just to make sure that in case anybody accesses it before
   * shutdown won't segfault */
  return;
}

static int c_psql_connect(c_psql_database_t *db)
{
  char conninfo[4096];
  char *buf = conninfo;
  int buf_len = sizeof(conninfo);
  int status;

  if ((!db) || (!db->database))
    return -1;

  status = ssnprintf(buf, buf_len, "dbname = '%s'", db->database);
  if (0 < status) {
    buf += status;
    buf_len -= status;
  }

  C_PSQL_PAR_APPEND(buf, buf_len, "host", db->host);
  C_PSQL_PAR_APPEND(buf, buf_len, "port", db->port);
  C_PSQL_PAR_APPEND(buf, buf_len, "user", db->user);
  C_PSQL_PAR_APPEND(buf, buf_len, "password", db->password);
  C_PSQL_PAR_APPEND(buf, buf_len, "sslmode", db->sslmode);
  C_PSQL_PAR_APPEND(buf, buf_len, "krbsrvname", db->krbsrvname);
  C_PSQL_PAR_APPEND(buf, buf_len, "service", db->service);
  C_PSQL_PAR_APPEND(buf, buf_len, "application_name", "ncollectd_postgresql");

  db->conn = PQconnectdb(conninfo);
  db->proto_version = PQprotocolVersion(db->conn);
  return 0;
}

static int c_psql_check_connection(c_psql_database_t *db)
{
  bool init = false;

  if (!db->conn) {
    init = true;

    /* trigger c_release() */
    if (0 == db->conn_complaint.interval)
      db->conn_complaint.interval = 1;

    c_psql_connect(db);
  }

  if (CONNECTION_OK != PQstatus(db->conn)) {
    PQreset(db->conn);

    /* trigger c_release() */
    if (0 == db->conn_complaint.interval)
      db->conn_complaint.interval = 1;

    if (CONNECTION_OK != PQstatus(db->conn)) {
      c_complain(LOG_ERR, &db->conn_complaint,
                 "Failed to connect to database %s (%s): %s", db->database,
                 db->instance, PQerrorMessage(db->conn));
      return -1;
    }

    db->proto_version = PQprotocolVersion(db->conn);
  }

  db->server_version = PQserverVersion(db->conn);

  if (c_would_release(&db->conn_complaint)) {
    char *server_host;
    int server_version;

    server_host = PQhost(db->conn);
    server_version = PQserverVersion(db->conn);

    c_do_release(LOG_INFO, &db->conn_complaint,
                 "Successfully %sconnected to database %s (user %s) "
                 "at server %s%s%s (server version: %d.%d.%d, "
                 "protocol version: %d, pid: %d)",
                 init ? "" : "re", PQdb(db->conn), PQuser(db->conn),
                 C_PSQL_SOCKET3(server_host, PQport(db->conn)),
                 C_PSQL_SERVER_VERSION3(server_version), db->proto_version,
                 PQbackendPID(db->conn));

    if (3 > db->proto_version)
      log_warn("Protocol version %d does not support parameters.",
               db->proto_version);
  }
  return 0;
}

static PGresult *c_psql_exec_query_noparams(c_psql_database_t *db, udb_query_t *q)
{
  return PQexec(db->conn, udb_query_get_statement(q));
}

static PGresult *c_psql_exec_query_params(c_psql_database_t *db, udb_query_t *q,
                                          c_psql_user_data_t *data)
{
  const char *params[db->max_params_num];
  char interval[64];

  if ((data == NULL) || (data->params_num == 0))
    return c_psql_exec_query_noparams(db, q);

  assert(db->max_params_num >= data->params_num);

  for (int i = 0; i < data->params_num; ++i) {
    switch (data->params[i]) {
    case C_PSQL_PARAM_HOST:
      params[i] =
          C_PSQL_IS_UNIX_DOMAIN_SOCKET(db->host) ? "localhost" : db->host;
      break;
    case C_PSQL_PARAM_DB:
      params[i] = db->database;
      break;
    case C_PSQL_PARAM_USER:
      params[i] = db->user;
      break;
    case C_PSQL_PARAM_INTERVAL:
      ssnprintf(interval, sizeof(interval), "%.3f",
                CDTIME_T_TO_DOUBLE(plugin_get_interval()));
      params[i] = interval;
      break;
    case C_PSQL_PARAM_INSTANCE:
      params[i] = db->instance;
      break;
    default:
      assert(0);
    }
  }

  return PQexecParams(db->conn, udb_query_get_statement(q), data->params_num,
                      NULL, (const char *const *)params, NULL, NULL, 0);
}

static int c_psql_exec_query(c_psql_database_t *db, udb_query_t *q, udb_query_preparation_area_t *prep_area)
{
  PGresult *res;

  c_psql_user_data_t *data;

  char **column_names;
  char **column_values;
  int column_num;

  int rows_num;
  int status;

  /* The user data may hold parameter information, but may be NULL. */
  data = udb_query_get_user_data(q);

  /* Versions up to `3' don't know how to handle parameters. */
  if (3 <= db->proto_version)
    res = c_psql_exec_query_params(db, q, data);
  else if ((NULL == data) || (0 == data->params_num))
    res = c_psql_exec_query_noparams(db, q);
  else {
    log_err("Connection to database \"%s\" (%s) does not support "
            "parameters (protocol version %d) - "
            "cannot execute query \"%s\".",
            db->database, db->instance, db->proto_version,
            udb_query_get_name(q));
    return -1;
  }

  /* give c_psql_write() a chance to acquire the lock if called recursively
   * through dispatch_values(); this will happen if, both, queries and
   * writers are configured for a single connection */
  pthread_mutex_unlock(&db->db_lock);

  column_names = NULL;
  column_values = NULL;

  if (PGRES_TUPLES_OK != PQresultStatus(res)) {
    pthread_mutex_lock(&db->db_lock);

    if ((CONNECTION_OK != PQstatus(db->conn)) &&
        (0 == c_psql_check_connection(db))) {
      PQclear(res);
      return c_psql_exec_query(db, q, prep_area);
    }

    log_err("Failed to execute SQL query: %s", PQerrorMessage(db->conn));
    log_info("SQL query was: %s", udb_query_get_statement(q));
    PQclear(res);
    return -1;
  }

#define BAIL_OUT(status)                                                       \
  sfree(column_names);                                                         \
  sfree(column_values);                                                        \
  PQclear(res);                                                                \
  pthread_mutex_lock(&db->db_lock);                                            \
  return status

  rows_num = PQntuples(res);
  if (1 > rows_num) {
    BAIL_OUT(0);
  }

  column_num = PQnfields(res);
  column_names = calloc(column_num, sizeof(*column_names));
  if (column_names == NULL) {
    log_err("calloc failed.");
    BAIL_OUT(-1);
  }

  column_values = calloc(column_num, sizeof(*column_values));
  if (column_values == NULL) {
    log_err("calloc failed.");
    BAIL_OUT(-1);
  }

  for (int col = 0; col < column_num; ++col) {
    /* Pointers returned by `PQfname' are freed by `PQclear' via
     * `BAIL_OUT'. */
    column_names[col] = PQfname(res, col);
    if (NULL == column_names[col]) {
      log_err("Failed to resolve name of column %i.", col);
      BAIL_OUT(-1);
    }
  }

  status =
      udb_query_prepare_result(q, prep_area, db->metric_prefix, &db->labels,
                               db->instance, column_names, (size_t)column_num);

  if (0 != status) {
    log_err("udb_query_prepare_result failed with status %i.", status);
    BAIL_OUT(-1);
  }

  for (int row = 0; row < rows_num; ++row) {
    int col;
    for (col = 0; col < column_num; ++col) {
      /* Pointers returned by `PQgetvalue' are freed by `PQclear' via
       * `BAIL_OUT'. */
      column_values[col] = PQgetvalue(res, row, col);
      if (NULL == column_values[col]) {
        log_err("Failed to get value at (row = %i, col = %i).", row, col);
        break;
      }
    }

    /* check for an error */
    if (col < column_num)
      continue;

    status = udb_query_handle_result(q, prep_area, column_values);
    if (status != 0) {
      log_err("udb_query_handle_result failed with status %i.", status);
    }
  } /* for (row = 0; row < rows_num; ++row) */

  udb_query_finish_result(q, prep_area);

  BAIL_OUT(0);
#undef BAIL_OUT
}

static int c_psql_read(user_data_t *ud)
{
  c_psql_database_t *db;

  int success = 0;

  if ((ud == NULL) || (ud->data == NULL)) {
    log_err("c_psql_read: Invalid user data.");
    return -1;
  }

  db = ud->data;

  assert(NULL != db->database);
  assert(NULL != db->instance);
  assert(NULL != db->queries);

  pthread_mutex_lock(&db->db_lock);

  if (0 != c_psql_check_connection(db)) {
    pthread_mutex_unlock(&db->db_lock);
    return -1;
  }

  for (size_t i = 0; i < db->queries_num; ++i) {
    udb_query_preparation_area_t *prep_area;
    udb_query_t *q;

    prep_area = db->q_prep_areas[i];
    q = db->queries[i];

    if ((0 != db->server_version) &&
        (udb_query_check_version(q, db->server_version) <= 0))
      continue;
    if (0 == c_psql_exec_query(db, q, prep_area))
      success = 1;
  }

  pthread_mutex_unlock(&db->db_lock);

  if (!success)
    return -1;
  return 0;
}

static int c_psql_shutdown(void)
{
  bool had_flush = false;

  plugin_unregister_read_group("postgresql");

  for (size_t i = 0; i < databases_num; ++i) {
    c_psql_database_t *db = databases[i];
    sfree(db);
  }

  udb_query_free(queries, queries_num);
  queries = NULL;
  queries_num = 0;

  sfree(databases);
  databases = NULL;
  databases_num = 0;

  return 0;
} /* c_psql_shutdown */

static int config_query_param_add(udb_query_t *q, oconfig_item_t *ci) {
  c_psql_user_data_t *data;
  const char *param_str;

  c_psql_param_t *tmp;

  data = udb_query_get_user_data(q);
  if (data == NULL) {
    data = calloc(1, sizeof(*data));
    if (data == NULL) {
      log_err("Out of memory.");
      return -1;
    }
    data->params = NULL;
    data->params_num = 0;

    udb_query_set_user_data(q, data);
  }

  tmp = realloc(data->params, (data->params_num + 1) * sizeof(*data->params));
  if (tmp == NULL) {
    log_err("Out of memory.");
    return -1;
  }
  data->params = tmp;

  param_str = ci->values[0].value.string;
  if (0 == strcasecmp(param_str, "hostname"))
    data->params[data->params_num] = C_PSQL_PARAM_HOST;
  else if (0 == strcasecmp(param_str, "database"))
    data->params[data->params_num] = C_PSQL_PARAM_DB;
  else if (0 == strcasecmp(param_str, "username"))
    data->params[data->params_num] = C_PSQL_PARAM_USER;
  else if (0 == strcasecmp(param_str, "interval"))
    data->params[data->params_num] = C_PSQL_PARAM_INTERVAL;
  else if (0 == strcasecmp(param_str, "instance"))
    data->params[data->params_num] = C_PSQL_PARAM_INSTANCE;
  else {
    log_err("Invalid parameter \"%s\".", param_str);
    return 1;
  }

  data->params_num++;
  return 0;
} /* config_query_param_add */

static int config_query_callback(udb_query_t *q, oconfig_item_t *ci) {
  if (0 == strcasecmp("Param", ci->key))
    return config_query_param_add(q, ci);

  log_err("Option not allowed within a Query block: `%s'", ci->key);

  return -1;
}

/*

<Plugin postgresql>
	<Server>

    

    <database xxxx>
    </database>
      

	</Server>
</Plugin>

 */


static int c_psql_config_database(oconfig_item_t *ci) {
  c_psql_database_t *db;

  cdtime_t interval = 0;
  char cb_name[DATA_MAX_NAME_LEN];
  static bool have_flush;

  if ((1 != ci->values_num) || (OCONFIG_TYPE_STRING != ci->values[0].type)) {
    log_err("<Database> expects a single string argument.");
    return 1;
  }

  db = c_psql_database_new(ci->values[0].value.string);
  if (db == NULL)
    return -1;

  int status = 0;
  for (int i = 0; i < ci->children_num; ++i) {
    oconfig_item_t *c = ci->children + i;

    if (0 == strcasecmp(c->key, "Host"))
      status = cf_util_get_string(c, &db->host);
    else if (0 == strcasecmp(c->key, "Port"))
      status = cf_util_get_service(c, &db->port);
    else if (0 == strcasecmp(c->key, "User"))
      status = cf_util_get_string(c, &db->user);
    else if (0 == strcasecmp(c->key, "Password"))
      status = cf_util_get_string(c, &db->password);


    else if (0 == strcasecmp(c->key, "Instance"))
      status = cf_util_get_string(c, &db->instance);
    else if (0 == strcasecmp(c->key, "MetricPrefix"))
      status = cf_util_get_string(c, &db->metric_prefix);
    else if (0 == strcasecmp(c->key, "Label"))
      status = cf_util_get_label(c, &db->labels);


    else if (0 == strcasecmp(c->key, "SSLMode"))
      status = cf_util_get_string(c, &db->sslmode);
    else if (0 == strcasecmp(c->key, "KRBSrvName"))
      status = cf_util_get_string(c, &db->krbsrvname);
    else if (0 == strcasecmp(c->key, "Service"))
      status = cf_util_get_string(c, &db->service);

    else if (0 == strcasecmp(c->key, "Query"))
      status = udb_query_pick_from_list(c, queries, queries_num, &db->queries, &db->queries_num);

    else if (0 == strcasecmp(c->key, "Interval"))
      status = cf_util_get_cdtime(c, &interval);

    else {
      log_warn("Ignoring unknown config key \"%s\".", c->key);
      status = -1;
    }

    if (status != 0)
      break;
  }

  if (status != 0) {
    c_psql_database_delete(db);
    return -1;
  }

  if (db->queries_num > 0) {
    db->q_prep_areas = calloc(db->queries_num, sizeof(*db->q_prep_areas));
    if (db->q_prep_areas == NULL) {
      log_err("Out of memory.");
      c_psql_database_delete(db);
      return -1;
    }
  }

  for (int i = 0; (size_t)i < db->queries_num; ++i) {
    c_psql_user_data_t *data;
    data = udb_query_get_user_data(db->queries[i]);
    if ((data != NULL) && (data->params_num > db->max_params_num))
      db->max_params_num = data->params_num;

    db->q_prep_areas[i] = udb_query_allocate_preparation_area(db->queries[i]);

    if (db->q_prep_areas[i] == NULL) {
      log_err("Out of memory.");
      c_psql_database_delete(db);
      return -1;
    }
  }

  ssnprintf(cb_name, sizeof(cb_name), "postgresql-%s", db->instance);

  user_data_t ud = {.data = db, .free_func = c_psql_database_delete};

  if (db->queries_num > 0) {
    ++db->ref_cnt;
    plugin_register_complex_read("postgresql", cb_name, c_psql_read, interval,
                                 &ud);
  }

  return 0;
}

static int c_psql_config(oconfig_item_t *ci)
{
  for (int i = 0; i < ci->children_num; ++i) {
    oconfig_item_t *c = ci->children + i;

    if (0 == strcasecmp(c->key, "Query"))
      udb_query_create(&queries, &queries_num, c, /* callback = */ config_query_callback);
    else if (0 == strcasecmp(c->key, "Writer"))
      c_psql_config_writer(c);
    else if (0 == strcasecmp(c->key, "Database"))
      c_psql_config_database(c);
    else
      log_warn("Ignoring unknown config key \"%s\".", c->key);
  }
  return 0;
}

void module_register(void)
{
  plugin_register_complex_config("postgresql", c_psql_config);
  plugin_register_shutdown("postgresql", c_psql_shutdown);
}
