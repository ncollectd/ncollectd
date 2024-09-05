// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2008-2012 Sebastian Harl
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/complain.h"
#include "libutils/dtoa.h"

#include <libpq-fe.h>
#include <pg_config_manual.h>

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

typedef struct {
    PGconn *conn;
    c_complain_t conn_complaint;

    int proto_version;
    int server_version;

    int max_params_num;

    /* writer "caching" settings */
    cdtime_t commit_interval;
    cdtime_t next_commit;

    cdtime_t flush_timeout;

    char *instance;
    char *host;
    char *port;
    char *database;
    char *user;
    char *password;
    char *sslmode;
    char *krbsrvname;
    char *service;
    char *statement;

    int refcnt;
} psql_database_t;

static int write_psql_connect(psql_database_t *db)
{
    if ((db == NULL) || (db->database == NULL))
        return -1;

    char conninfo[4096];
    strbuf_t buf = STRBUF_CREATE_STATIC(conninfo);
    struct {
        char *param;
        char *value;
    } psql_params[] = {
        { "dbname",           db->database           },
        { "host",             db->host               },
        { "port",             db->port               },
        { "user",             db->user               },
        { "password",         db->password           },
        { "sslmode",          db->sslmode            },
        { "krbsrvname",       db->krbsrvname         },
        { "service",          db->service            },
        { "application_name", PACKAGE_NAME           }
    };
    size_t psql_params_size = STATIC_ARRAY_SIZE(psql_params);

    for (size_t i = 0; i  < psql_params_size; i++) {
        if ((psql_params[i].value != NULL) && (psql_params[i].value[0] != '\0')) {
            strbuf_putstr(&buf, psql_params[i].param);
            strbuf_putstr(&buf, " = '");
            strbuf_putstr(&buf, psql_params[i].value);
            strbuf_putstr(&buf, "' ");
        }
    }

    db->conn = PQconnectdb(conninfo);
    db->proto_version = PQprotocolVersion(db->conn);
    return 0;
}

static int write_psql_check_connection(psql_database_t *db)
{
    bool init = false;

    if (db->conn == NULL) {
        init = true;
        /* trigger c_release() */
        if (db->conn_complaint.interval == 0)
            db->conn_complaint.interval = 1;

        write_psql_connect(db);
    }

    if (PQstatus(db->conn) != CONNECTION_OK) {
        PQreset(db->conn);

        /* trigger c_release() */
        if (db->conn_complaint.interval == 0)
            db->conn_complaint.interval = 1;

        if (PQstatus(db->conn) != CONNECTION_OK) {
            c_complain(LOG_ERR, &db->conn_complaint,
                                "Failed to connect to database %s: %s", db->database,
                                PQerrorMessage(db->conn));
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

        if (db->proto_version < 3)
            PLUGIN_WARNING("Protocol version %d does not support parameters.", db->proto_version);
    }

    return 0;
}

static int write_psql_begin(psql_database_t *db)
{
    int status = 1;
    PGresult *r = PQexec(db->conn, "BEGIN");
    if (r != NULL) {
        if (PQresultStatus(r) == PGRES_COMMAND_OK) {
            db->next_commit = cdtime() + db->commit_interval;
            status = 0;
        } else {
            PLUGIN_WARNING("Failed to initiate transaction: %s", PQerrorMessage(db->conn));
        }
        PQclear(r);
    }
    return status;
}

static int write_psql_commit(psql_database_t *db)
{
    int status = 1;
    PGresult *r = PQexec(db->conn, "COMMIT");
    if (r != NULL) {
        if (PQresultStatus(r) == PGRES_COMMAND_OK) {
            db->next_commit = 0;
            PLUGIN_DEBUG("Successfully committed transaction.");
            status = 0;
        } else {
            PLUGIN_WARNING("Failed to commit transaction: %s", PQerrorMessage(db->conn));
        }
        PQclear(r);
    }
    return status;
}

static void write_psql_free(void *data)
{
    if (data == NULL)
        return;

    psql_database_t *db = data;

    db->refcnt--;
    if (db->refcnt != 0)
        return;

    if (db->conn != NULL) {
        if (db->next_commit > 0)
            write_psql_commit(db);
        PQfinish(db->conn);
        db->conn = NULL;
    }

    free(db->instance);
    free(db->database);
    free(db->host);
    free(db->port);
    free(db->user);
    free(db->password);
    free(db->sslmode);
    free(db->krbsrvname);
    free(db->service);
    free(db->statement);

    free(db);

    return;
}

static int write_psql_insert(psql_database_t *db, const char **params, size_t params_size)
{
    if (write_psql_check_connection(db) != 0)
        return -1;

    if ((db->commit_interval > 0) && (db->next_commit == 0))
        write_psql_begin(db);

    PGresult *res = PQexecParams(db->conn, db->statement, params_size,
                                 NULL, (const char *const *)params, NULL, NULL, 0);
    if ((PQresultStatus(res) != PGRES_COMMAND_OK) && (PQresultStatus(res) != PGRES_TUPLES_OK)) {
        PQclear(res);

        bool success = false;

        if ((PQstatus(db->conn) != CONNECTION_OK) && (write_psql_check_connection(db) == 0)) {
            /* try again */
            res = PQexecParams(db->conn, db->statement, params_size,
                               NULL, (const char *const *)params, NULL, NULL, 0);
            if ((PQresultStatus(res) == PGRES_COMMAND_OK) ||
                (PQresultStatus(res) == PGRES_TUPLES_OK)) {
                PQclear(res);
            }

            success = true;
        }

        if (success == false) {
            PLUGIN_ERROR("Failed to execute SQL query: %s", PQerrorMessage(db->conn));
            PLUGIN_INFO("SQL query was: '%s', params: %s, %s, %s, %s",
                        db->statement, params[0], params[1], params[2], params[3]);

            /* this will abort any current transaction -> restart */
            if (db->next_commit > 0)
                write_psql_commit(db);

            return -1;
        }
    } else {
       PQclear(res);
    }

    if ((db->next_commit > 0) && (cdtime() > db->next_commit))
        write_psql_commit(db);

    return 0;
}

static int write_psql_notif(notification_t const *n, user_data_t *ud)
{
    if ((n == NULL) || (ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    psql_database_t *db = ud->data;

    strbuf_t buf = STRBUF_CREATE;
    int status = strbuf_resize(&buf, 4096);
    if (status != 0) {
        PLUGIN_ERROR("Buffer resize failed.");
        return -1;
    }

    char time_str[RFC3339NANO_SIZE] = {'\0'};
    if (rfc3339nano_local(time_str, sizeof(time_str), n->time) != 0) {
        PLUGIN_ERROR("Failed to convert time to RFC 3339 format");
        strbuf_destroy(&buf);
        return -1;
    }

    char *labels_str = NULL;
    if (n->label.num > 0) {
        status = strbuf_putchar(&buf, '{');
        for (size_t i = 0; i < n->label.num; i++, n++) {
            if (i != 0)
                status |= strbuf_putchar(&buf, ',');
            status |= strbuf_putstrn(&buf, "{'", 2);
            status |= strbuf_putstr(&buf, n->label.ptr[i].name);
            status |= strbuf_putstrn(&buf, "','", 3);
            status |= strbuf_putescape_label(&buf, n->label.ptr[i].value);
            status |= strbuf_putstrn(&buf, "'}", 2);
        }
        status |= strbuf_putchar(&buf, '}');
        status |= strbuf_putchar(&buf, ' ');
        if (status != 0) {
            PLUGIN_ERROR("Failed to format labels.");
            return -1;
        }
    }
    size_t labels_len = strbuf_len(&buf);

    char *annotations_str = NULL;
    if (n->annotation.num > 0) {
        status = strbuf_putchar(&buf, '{');
        for (size_t i = 0; i < n->annotation.num; i++, n++) {
            if (i != 0)
                status |= strbuf_putchar(&buf, ',');
            status |= strbuf_putstrn(&buf, "{'", 2);
            status |= strbuf_putstr(&buf, n->annotation.ptr[i].name);
            status |= strbuf_putstrn(&buf, "','", 3);
            status |= strbuf_putescape_label(&buf, n->annotation.ptr[i].value);
            status |= strbuf_putstrn(&buf, "'}", 2);
        }
        status |= strbuf_putchar(&buf, '}');
        if (status != 0) {
            PLUGIN_ERROR("Failed to format annotations.");
            return -1;
        }
    }

    if (labels_len > 0) {
        labels_str = buf.ptr;
        buf.ptr[labels_len-1] = '\0';
        if (n->annotation.num > 0)
            annotations_str = buf.ptr + labels_len;
    }

    char *severity_str = NULL;
    switch(n->severity) {
    case NOTIF_FAILURE:
        severity_str = "FAILURE";
        break;
    case NOTIF_WARNING:
        severity_str = "WARNING";
        break;
    case NOTIF_OKAY:
        severity_str = "OKAY";
        break;
    }

    const char *params[5];
    params[0] = n->name;
    params[1] = labels_str;
    params[2] = annotations_str;
    params[3] = severity_str;
    params[4] = time_str;

    write_psql_insert(db, params, STATIC_ARRAY_SIZE(params));

    strbuf_destroy(&buf);

    return 0;
}

static int write_psql_metric(psql_database_t *db, strbuf_t *buf,
                             char *metric, char *metric_suffix,
                             const label_set_t *labels1, const label_set_t *labels2,
                             double value, cdtime_t time)
{
    strbuf_reset(buf);

    int status = strbuf_putstr(buf, metric);
    if (metric_suffix != NULL)
        status |= strbuf_putstr(buf, metric_suffix);

    if (status != 0) {
        PLUGIN_ERROR("Failed to concatenate metric name.");
        return -1;
    }

    size_t metric_len = strbuf_len(buf);
    strbuf_putchar(buf, ' ');

    if (((labels1 != NULL) && (labels1->num > 0)) || ((labels2 != NULL) && (labels2->num > 0))) {
        size_t n = 0;
        status |= strbuf_putchar(buf, '{');

        if (labels1 != NULL) {
            for (size_t i = 0; i < labels1->num; i++, n++) {
                if (n != 0)
                    status |= strbuf_putchar(buf, ',');
                status |= strbuf_putstrn(buf, "{'", 2);
                status |= strbuf_putstr(buf, labels1->ptr[i].name);
                status |= strbuf_putstrn(buf, "','", 3);
                status |= strbuf_putescape_label(buf, labels1->ptr[i].value);
                status |= strbuf_putstrn(buf, "'}", 2);
            }
        }

        if (labels2 != NULL) {
            for (size_t i = 0; i < labels2->num; i++, n++) {
                if (n != 0)
                    status |= strbuf_putchar(buf, ',');
                status |= strbuf_putstrn(buf, "{'", 2);
                status |= strbuf_putstr(buf, labels2->ptr[i].name);
                status |= strbuf_putstrn(buf, "','", 3);
                status |= strbuf_putescape_label(buf, labels2->ptr[i].value);
                status |= strbuf_putstrn(buf, "'}", 2);
            }
        }

        status |= strbuf_putchar(buf, '}');
    }

    if (status != 0) {
        PLUGIN_ERROR("Failed to concatenate metric labels.");
        return -1;
    }

    char time_str[RFC3339NANO_SIZE] = {'\0'};
    if (rfc3339nano_local(time_str, sizeof(time_str), time) != 0) {
        PLUGIN_ERROR("Failed to convert time to RFC 3339 format");
        return -1;
    }

    buf->ptr[metric_len] = '\0';
    const char *metric_str = buf->ptr;
    const char *labels_str = NULL;
    if ((strbuf_len(buf) - (metric_len + 1)) > 0)
        labels_str = buf->ptr + metric_len + 1;

    char value_str[DTOA_MAX];
    dtoa(value, value_str, sizeof(value_str));

    const char *params[4];
    params[0] = metric_str;
    params[1] = labels_str;
    params[2] = value_str;
    params[3] = time_str;

    return write_psql_insert(db, params, STATIC_ARRAY_SIZE(params));
}

static int write_psql(metric_family_t const *fam, user_data_t *ud)
{
    if (fam == NULL)
        return EINVAL;

    if (fam->metric.num == 0)
        return 0;

    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    strbuf_t buf = STRBUF_CREATE;
    int status = strbuf_resize(&buf, 4096);
    if (status != 0) {
        PLUGIN_ERROR("Buffer resize failed.");
        return -1;
    }

    psql_database_t *db = ud->data;

    for (size_t i = 0; i < fam->metric.num; i++) {
        metric_t *m = &fam->metric.ptr[i];
        switch(fam->type) {
        case METRIC_TYPE_UNKNOWN: {
            double value = m->value.unknown.type == UNKNOWN_FLOAT64 ?
                           m->value.unknown.float64 : m->value.unknown.int64;
            status |= write_psql_metric(db, &buf, fam->name, NULL,
                                        &m->label, NULL, value, m->time);
        }   break;
        case METRIC_TYPE_GAUGE: {
            double value = m->value.gauge.type == GAUGE_FLOAT64 ?
                           m->value.gauge.float64 : m->value.gauge.int64;
            status |= write_psql_metric(db, &buf, fam->name, NULL,
                                        &m->label, NULL, value, m->time);
        }   break;
        case METRIC_TYPE_COUNTER: {
            double value = m->value.counter.type == COUNTER_UINT64 ?
                           m->value.counter.uint64 : m->value.counter.float64;
            status |= write_psql_metric(db, &buf, fam->name, "_total",
                                        &m->label, NULL, value, m->time);
        }   break;
        case METRIC_TYPE_STATE_SET:
            for (size_t j = 0; j < m->value.state_set.num; j++) {
                label_pair_t label_pair = {.name = fam->name,
                                           .value = m->value.state_set.ptr[j].name};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                double value = m->value.state_set.ptr[j].enabled ? 1 : 0;
                status |= write_psql_metric(db, &buf, fam->name, NULL,
                                            &m->label, &label_set, value, m->time);
            }
            break;
        case METRIC_TYPE_INFO:
            status |= write_psql_metric(db, &buf, fam->name, "_info",
                                        &m->label, &m->value.info, 1, m->time);
            break;
        case METRIC_TYPE_SUMMARY: {
            summary_t *s = m->value.summary;

            for (int j = s->num - 1; j >= 0; j--) {
                char quantile[DTOA_MAX];

                dtoa(s->quantiles[j].quantile, quantile, sizeof(quantile));

                label_pair_t label_pair = {.name = "quantile", .value = quantile};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                status |= write_psql_metric(db, &buf, fam->name, NULL,
                                            &m->label, &label_set, s->quantiles[j].value, m->time);
            }

            status |= write_psql_metric(db, &buf, fam->name, "_count",
                                        &m->label, NULL, s->count, m->time);
            status |= write_psql_metric(db, &buf, fam->name, "_sum",
                                        &m->label, NULL, s->sum, m->time);
        }   break;
        case METRIC_TYPE_HISTOGRAM:
        case METRIC_TYPE_GAUGE_HISTOGRAM: {
            histogram_t *h = m->value.histogram;

            for (int j = h->num - 1; j >= 0; j--) {
                char le[DTOA_MAX];
                dtoa(h->buckets[j].maximum, le, sizeof(le));

                label_pair_t label_pair = {.name = "le", .value = le};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                status |= write_psql_metric(db, &buf, fam->name, "_bucket",
                                            &m->label, &label_set, h->buckets[j].counter, m->time);
            }

            status |= write_psql_metric(db, &buf, fam->name,
                                        fam->type == METRIC_TYPE_HISTOGRAM ? "_count" : "_gcount",
                                        &m->label, NULL, histogram_counter(h), m->time);
            status |= write_psql_metric(db, &buf, fam->name,
                                        fam->type == METRIC_TYPE_HISTOGRAM ?  "_sum" : "_gsum",
                                        &m->label, NULL, histogram_sum(h), m->time);
        }   break;
        }

        if (status != 0)
            break;
    }

    strbuf_destroy(&buf);

    return status;
}

/* We cannot flush single identifiers as all we do is to commit the currently
 * running transaction, thus making sure that all written data is actually
 * visible to everybody. */
static int write_psql_flush(__attribute__((unused)) cdtime_t timeout, user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL))
        return -1;

    psql_database_t *db = ud->data;

    /* don't commit if the timeout is larger than the regular commit
     * interval as in that case all requested data has already been
     * committed */
    if ((db->next_commit > 0) && (cdtime() > db->next_commit))
        write_psql_commit(db);

    return 0;
}

static int write_psql_config_database(config_item_t *ci)
{
    psql_database_t *db = calloc(1, sizeof(*db));
    if (db == NULL) {
        PLUGIN_ERROR("Out of memory.");
        return -1;
    }

    C_COMPLAIN_INIT(&db->conn_complaint);

    int status = cf_util_get_string(ci, &db->instance);
    if (status != 0) {
        PLUGIN_ERROR("Missing database name.");
        write_psql_free(db);
        return status;
    }

    cf_send_t send = SEND_METRICS;
    cdtime_t flush_interval = 0;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "host") == 0) {
            status = cf_util_get_string(child, &db->host);
        } else if (strcasecmp(child->key, "port") == 0) {
            status = cf_util_get_service(child, &db->port);
        } else if (strcasecmp(child->key, "database") == 0) {
            status = cf_util_get_string(child, &db->database);
        } else if (strcasecmp(child->key, "user") == 0) {
            status = cf_util_get_string(child, &db->user);
        } else if (strcasecmp(child->key, "user-env") == 0) {
            status = cf_util_get_string_env(child, &db->user);
        } else if (strcasecmp(child->key, "password") == 0) {
            status = cf_util_get_string(child, &db->password);
        } else if (strcasecmp(child->key, "password-env") == 0) {
            status = cf_util_get_string_env(child, &db->password);
        } else if (strcasecmp(child->key, "ssl-mode") == 0) {
            status = cf_util_get_string(child, &db->sslmode);
        } else if (strcasecmp(child->key, "krb-srvname") == 0) {
            status = cf_util_get_string(child, &db->krbsrvname);
        } else if (strcasecmp(child->key, "service") == 0) {
            status = cf_util_get_string(child, &db->service);
        } else if (strcasecmp(child->key, "statement") == 0) {
            status = cf_util_get_string(child, &db->statement);
        } else if (strcasecmp(child->key, "flush-interval") == 0) {
            status = cf_util_get_cdtime(child, &flush_interval);
        } else if (strcasecmp(child->key, "flush-timeout") == 0) {
            status = cf_util_get_cdtime(child, &db->flush_timeout);
        } else if (strcasecmp(child->key, "commit-interval") == 0) {
            status = cf_util_get_cdtime(child, &db->commit_interval);
        } else if (strcasecmp(child->key, "write") == 0) {
            status = cf_uti_get_send(child, &send);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        write_psql_free(db);
        return status;
    }

    if (db->database == NULL) {
        PLUGIN_ERROR("The database name is missing.");
        write_psql_free(db);
        return -1;
    }

    if (db->statement == NULL) {
        PLUGIN_ERROR("You do not have any statement assigned to this database connection.");
        write_psql_free(db);
        return -1;
    }

    user_data_t user_data = (user_data_t){.data = db, .free_func = write_psql_free};

    if (send == SEND_NOTIFICATIONS)
        return plugin_register_notification("write_postgresql", db->instance, write_psql_notif,
                                                                &user_data);

    return plugin_register_write("write_postgresql", db->instance, write_psql,
                                 write_psql_flush, flush_interval, db->flush_timeout, &user_data);
}

static int write_psql_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "instance") == 0) {
            status = write_psql_config_database(child);
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

void module_register(void)
{
    plugin_register_config("write_postgresql", write_psql_config);
}
