// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2010-2013 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2010 Akkarit Sangpetch
// SPDX-FileCopyrightText: Copyright (C) 2012 Chris Lundquist
// SPDX-FileCopyrightText: Copyright (C) 2017 Saikrishna Arcot
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Akkarit Sangpetch <asangpet at andrew.cmu.edu>
// SPDX-FileContributor: Chris Lundquist <clundquist at bluebox.net>
// SPDX-FileContributor: Saikrishna Arcot <saiarcot895 at gmail.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/dtoa.h"
#include "libmetric/marshal.h"

#pragma GCC diagnostic ignored "-Wcast-qual"
#ifdef HAVE_MONGOC_H
#include <mongoc.h>
#elif defined(HAVE_MONGOC_MONGOC_H)
#include <mongoc/mongoc.h>
#endif

typedef struct {
    char *name;
    char *uri;
    char *host;
    int port;
    char *database_name;
    char *user;
    char *passwd;

    char *metric_collection_name;
    char *metric_timestamp_field;
    char *metric_metadata_field;
    char *metric_value_field;

    char *notification_collection_name;
    char *notification_name_field;
    char *notification_severity_field;
    char *notification_timestamp_field;
    char *notification_labels_field;
    char *notification_annotations_field;

    bool connected;
    cf_send_t send;

    mongoc_client_t *client;
    mongoc_database_t *database;
    mongoc_collection_t *collection;

    bson_t **documents;
    size_t documents_size;
    size_t documents_alloc;

    int ttl;
} write_mongodb_t;

static int write_mongodb_initialize(write_mongodb_t *db, char *collection)
{
    if (db->connected)
        return 0;

    if (db->client != NULL) {
        mongoc_client_destroy(db->client);
        db->client = NULL;
    }

    mongoc_uri_t *uri = NULL;
    if (db->uri != NULL) {
        PLUGIN_INFO("Connecting to the configured URI");

        bson_error_t error;
        uri = mongoc_uri_new_with_error(db->uri, &error);
        if (uri == NULL) {
            PLUGIN_ERROR("Cannot parse URI: %s", error.message);
            return -1;
        }
    } else {
        PLUGIN_INFO("Connecting to [%s]:%d", db->host, db->port);

        uri = mongoc_uri_new_for_host_port(db->host, db->port);
        if (uri == NULL) {
            PLUGIN_ERROR("Cannot create URI object.");
            return -1;
        }
    }

    if (db->user != NULL) {
        if (!mongoc_uri_set_username(uri, db->user)) {
            PLUGIN_ERROR("Failed to set user in URI.");
            mongoc_uri_destroy(uri);
            return -1;
        }
    }

    if (db->passwd != NULL) {
        if (!mongoc_uri_set_password(uri, db->passwd)) {
            PLUGIN_ERROR("Failed to set password in URI.");
            mongoc_uri_destroy(uri);
            return -1;
        }
    }

    if (db->database_name != NULL) {
        if (!mongoc_uri_set_database(uri, db->database_name)) {
            PLUGIN_ERROR("Failed to set database in URI.");
            mongoc_uri_destroy(uri);
            return -1;
        }
    }

    db->client = mongoc_client_new_from_uri(uri);
    if (db->client == NULL) {
        PLUGIN_ERROR("Connecting to [%s]:%d failed.", db->host, db->port);
        mongoc_uri_destroy(uri);
        return -1;
    }

    mongoc_uri_destroy(uri);

    db->database = mongoc_client_get_database(db->client, db->database_name);
    if (db->database == NULL) {
        PLUGIN_ERROR("error creating/getting database");
        mongoc_client_destroy(db->client);
        db->client = NULL;
        db->connected = false;
        return -1;
    }

    bson_error_t has_error = {0};
    bool has_collection = mongoc_database_has_collection(db->database, collection, &has_error);
    if (!has_collection && (has_error.code != 0)) {
        PLUGIN_ERROR("Error checking collection '%s': %s", collection, has_error.message);
        mongoc_database_destroy(db->database);
        db->database = NULL;
        mongoc_client_destroy(db->client);
        db->client = NULL;
        db->connected = false;
        return -1;
    }

    if (!has_collection) {

        if (db->send == SEND_METRICS) {
            bson_t *b = bson_new();
            if (b == NULL) {
                PLUGIN_ERROR("bson_new failed.");
                mongoc_database_destroy(db->database);
                db->database = NULL;
                mongoc_client_destroy(db->client);
                db->client = NULL;
                return -1;
            }

            bson_t ts;
            BSON_APPEND_DOCUMENT_BEGIN(b, "timeseries", &ts);
            BSON_APPEND_UTF8(&ts, "timeField", db->metric_timestamp_field);
            BSON_APPEND_UTF8(&ts, "metaField", db->metric_metadata_field);
            BSON_APPEND_UTF8(&ts, "granularity", "seconds");
            if (db->ttl > 0)
                BSON_APPEND_INT64(&ts, "expireAfterSeconds", db->ttl);
            bson_append_document_end(b, &ts);

            bson_error_t error;
            db->collection = mongoc_database_create_collection(db->database, collection, b, &error);
            if (db->collection == NULL) {
                PLUGIN_ERROR("Error creating collection '%s': %s (domain %u, code %u)",
                             collection, error.message, error.domain, error.code);
                bson_destroy(b);
                mongoc_database_destroy(db->database);
                db->database = NULL;
                mongoc_client_destroy(db->client);
                db->client = NULL;
                db->connected = false;
                return -1;
            }
            bson_destroy(b);
        } else if (db->send == SEND_NOTIFICATIONS) {
            bson_error_t error;
            db->collection = mongoc_database_create_collection(db->database, collection, NULL, &error);
            if (db->collection == NULL) {
                PLUGIN_ERROR("Error creating collection '%s': %s (domain %u, code %u)",
                             collection, error.message, error.domain, error.code);
                mongoc_database_destroy(db->database);
                db->database = NULL;
                mongoc_client_destroy(db->client);
                db->client = NULL;
                db->connected = false;
                return -1;
            }
        }
    } else {
        db->collection = mongoc_client_get_collection(db->client, db->database_name, collection);
        if (db->collection == NULL) {
            PLUGIN_ERROR("Error getting collection");
            mongoc_database_destroy(db->database);
            db->database = NULL;
            mongoc_client_destroy(db->client);
            db->client = NULL;
            db->connected = false;
            return -1;
        }
    }

    db->connected = true;

    return 0;
}

static int write_mongodb_notif(notification_t const *n, user_data_t *ud)
{
    if ((n == NULL) || (ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    write_mongodb_t *db = ud->data;

    if (write_mongodb_initialize(db, db->notification_collection_name) < 0) {
        PLUGIN_ERROR("Error making connection to server");
        return -1;
    }

    strbuf_t buf = STRBUF_CREATE;
    strbuf_resize(&buf, 1024);

    int status = strbuf_putstr(&buf, n->name);
    if (n->label.num > 0)
        status = status || label_set_marshal(&buf, n->label);

    if (status != 0) {
        strbuf_destroy(&buf);
        PLUGIN_ERROR("Failed to marshal notification");
        return -1;
    }

    bson_t *selector = BCON_NEW ("_id", BCON_UTF8(buf.ptr));

    strbuf_destroy(&buf);

    if (n->severity == NOTIF_OKAY) {
        bson_error_t error;
        bool ret = mongoc_collection_delete_one(db->collection, selector,  NULL, NULL, &error);
        if (ret == false) {
            PLUGIN_ERROR("Error deleting from collection: %s (domain %u, code %u)",
                         error.message, error.domain, error.code);
        }

        bson_destroy(selector);
        return 0;
    }

    bson_t *b = bson_new();
    if (b == NULL) {
        PLUGIN_ERROR("bson_new failed.");
        return -1;
    }

    BSON_APPEND_UTF8(b, db->notification_name_field, n->name);
    BSON_APPEND_DATE_TIME(b, db->notification_timestamp_field, CDTIME_T_TO_MS(n->time));

    switch(n->severity) {
    case NOTIF_FAILURE:
        BSON_APPEND_UTF8(b, db->notification_severity_field, "FAILURE");
        break;
    case NOTIF_WARNING:
        BSON_APPEND_UTF8(b, db->notification_severity_field, "WARNING");
        break;
    case NOTIF_OKAY:
        BSON_APPEND_UTF8(b, db->notification_severity_field, "OKAY");
        break;
    }

    if (n->label.num > 0) {
        bson_t labels;
        BSON_APPEND_DOCUMENT_BEGIN(b, db->notification_labels_field, &labels);
        for (size_t i = 0; i < n->label.num; i++) {
            BSON_APPEND_UTF8(&labels, n->label.ptr[i].name, n->label.ptr[i].value);
        }
        bson_append_document_end(b, &labels);
    }

    if (n->annotation.num > 0) {
        bson_t annotations;
        BSON_APPEND_DOCUMENT_BEGIN(b, db->notification_annotations_field, &annotations);
        for (size_t i = 0; i < n->annotation.num; i++) {
            BSON_APPEND_UTF8(&annotations, n->annotation.ptr[i].name, n->annotation.ptr[i].value);
        }
        bson_append_document_end(b, &annotations);
    }

    /* Keys with '.' or '$' would be interpreted as paths or operators by the server. */
    size_t error_location;
    if (!bson_validate(b, BSON_VALIDATE_UTF8 | BSON_VALIDATE_DOT_KEYS | BSON_VALIDATE_DOLLAR_KEYS,
                       &error_location)) {
        PLUGIN_ERROR("Error in generated BSON document at byte %" PRIsz, error_location);
        bson_destroy(selector);
        bson_destroy(b);
        return -1;
    }

    bson_t *update = BCON_NEW ("$set", BCON_DOCUMENT(b));
    bson_t *opts = BCON_NEW ("upsert", BCON_BOOL(true));

    bson_error_t error;
    bool ret = mongoc_collection_update_one(db->collection, selector, update, opts, NULL, &error);
    bson_destroy(opts);
    if (ret == false) {
        PLUGIN_ERROR("Error updating collection: %s (domain %u, code %u)",
                     error.message, error.domain, error.code);
    }

    bson_destroy(selector);
    bson_destroy(b);
    bson_destroy(update);
    return 0;
}

static int write_mongodb_insert_documents(write_mongodb_t *db)
{
    if (db->documents_size == 0)
        return 0;

    int status = 0;

    if (db->collection != NULL) {
        bson_error_t error;
        bool ret = mongoc_collection_insert_many(db->collection, (const bson_t **)db->documents,
                                                 db->documents_size, NULL, NULL, &error);
        if (ret == false) {
            PLUGIN_ERROR("Error inserting in collection: %s (domain %u, code %u)",
                         error.message, error.domain, error.code);
            status = -1;
        }
    }

    for(size_t i = 0; i < db->documents_size; i++) {
        bson_destroy(db->documents[i]);
        db->documents[i] = NULL;
    }
    db->documents_size = 0;

    return status;
}

static int write_mongodb_metric_value(write_mongodb_t *db, strbuf_t *buf,
                                      char *metric, char *metric_suffix,
                                      const label_set_t *labels1, const label_set_t *labels2,
                                      bool is_int64, int64_t int64_value, double value,
                                      cdtime_t time)
{
    bson_t *b = bson_new();
    if (b == NULL) {
        PLUGIN_ERROR("bson_new failed.");
        return -1;
    }

    bson_t meta;
    BSON_APPEND_DOCUMENT_BEGIN(b, db->metric_metadata_field, &meta);

    if (metric_suffix == NULL) {
        BSON_APPEND_UTF8(&meta, "__name__", metric);
    } else {
        strbuf_reset(buf);
        int status = strbuf_putstr(buf, metric);
        if (metric_suffix != NULL)
            status |= strbuf_putstr(buf, metric_suffix);

        if (status != 0) {
            PLUGIN_ERROR("Failed to concatenate metric name.");
            bson_destroy(b);
            return -1;
        }

        BSON_APPEND_UTF8(&meta, "__name__", buf->ptr);
    }

    /* A label named "__name__" would collide with the metric name. */
    if (labels1 != NULL) {
        for (size_t i = 0; i < labels1->num; i++) {
            if (strcmp(labels1->ptr[i].name, "__name__") == 0)
                continue;
            BSON_APPEND_UTF8(&meta, labels1->ptr[i].name, labels1->ptr[i].value);
        }
    }

    if (labels2 != NULL) {
        for (size_t i = 0; i < labels2->num; i++) {
            if (strcmp(labels2->ptr[i].name, "__name__") == 0)
                continue;
            BSON_APPEND_UTF8(&meta, labels2->ptr[i].name, labels2->ptr[i].value);
        }
    }

    bson_append_document_end(b, &meta);

    BSON_APPEND_DATE_TIME(b, db->metric_timestamp_field, CDTIME_T_TO_MS(time));
    if (is_int64)
        BSON_APPEND_INT64(b, db->metric_value_field, int64_value);
    else
        BSON_APPEND_DOUBLE(b, db->metric_value_field, value);

    size_t error_location;
    if (!bson_validate(b, BSON_VALIDATE_UTF8 | BSON_VALIDATE_DOT_KEYS | BSON_VALIDATE_DOLLAR_KEYS,
                       &error_location)) {
        PLUGIN_ERROR("Error in generated BSON document at byte %" PRIsz, error_location);
        bson_destroy(b);
        return -1;
    }

    if ((db->documents_size) + 1 > db->documents_alloc)
        write_mongodb_insert_documents(db);

    db->documents[db->documents_size] = b;
    db->documents_size++;

    return 0;
}

static int write_mongodb_metric(write_mongodb_t *db, strbuf_t *buf,
                                char *metric, char *metric_suffix,
                                const label_set_t *labels1, const label_set_t *labels2,
                                double value, cdtime_t time)
{
    return write_mongodb_metric_value(db, buf, metric, metric_suffix, labels1, labels2,
                                      false, 0, value, time);
}

/* Integer counters are written as int64 to avoid losing precision above 2^53. */
static int write_mongodb_metric_uint64(write_mongodb_t *db, strbuf_t *buf,
                                       char *metric, char *metric_suffix,
                                       const label_set_t *labels1, const label_set_t *labels2,
                                       uint64_t value, cdtime_t time)
{
    if (value > INT64_MAX)
        return write_mongodb_metric_value(db, buf, metric, metric_suffix, labels1, labels2,
                                          false, 0, (double)value, time);

    return write_mongodb_metric_value(db, buf, metric, metric_suffix, labels1, labels2,
                                      true, (int64_t)value, 0, time);
}

int write_mongodb_create_bson(write_mongodb_t *db, metric_family_t const *fam)
{
    int status = 0;

    if (fam == NULL)
        return EINVAL;

    if (fam->metric.num == 0)
        return 0;

    strbuf_t buf = {0};

    for (size_t i = 0; i < fam->metric.num; i++) {
        metric_t *m = &fam->metric.ptr[i];
        switch(fam->type) {
        case METRIC_TYPE_UNKNOWN: {
            double value = m->value.unknown.type == UNKNOWN_FLOAT64 ? m->value.unknown.float64
                                                                    : m->value.unknown.int64;
            status |= write_mongodb_metric(db, &buf, fam->name, NULL,
                                           &m->label, NULL, value, m->time);
        }   break;
        case METRIC_TYPE_GAUGE: {
            double value = m->value.gauge.type == GAUGE_FLOAT64 ? m->value.gauge.float64
                                                                : m->value.gauge.int64;
            status |= write_mongodb_metric(db, &buf, fam->name, NULL,
                                           &m->label, NULL, value, m->time);
        }   break;
        case METRIC_TYPE_COUNTER:
            if (m->value.counter.type == COUNTER_UINT64)
                status |= write_mongodb_metric_uint64(db, &buf, fam->name, "_total", &m->label,
                                                      NULL, m->value.counter.uint64, m->time);
            else
                status |= write_mongodb_metric(db, &buf, fam->name, "_total", &m->label,
                                               NULL, m->value.counter.float64, m->time);
            break;
        case METRIC_TYPE_STATE_SET:
            for (size_t j = 0; j < m->value.state_set.num; j++) {
                label_pair_t label_pair = {.name = fam->name,
                                           .value = m->value.state_set.ptr[j].name};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                double value = m->value.state_set.ptr[j].enabled ? 1 : 0;

                status |= write_mongodb_metric(db, &buf, fam->name, NULL,
                                               &m->label, &label_set, value, m->time);
            }
            break;
        case METRIC_TYPE_INFO:
            status |= write_mongodb_metric(db, &buf, fam->name, "_info",
                                           &m->label, &m->value.info, 1, m->time);
            break;
        case METRIC_TYPE_SUMMARY: {
            summary_t *s = m->value.summary;

            for (int j = s->num - 1; j >= 0; j--) {
                char quantile[DTOA_MAX];

                dtoa(s->quantiles[j].quantile, quantile, sizeof(quantile));

                label_pair_t label_pair = {.name = "quantile", .value = quantile};
                label_set_t label_set = {.num = 1, .ptr = &label_pair};
                status |= write_mongodb_metric(db, &buf, fam->name, NULL, &m->label, &label_set,
                                               s->quantiles[j].value, m->time);
            }

            status |= write_mongodb_metric_uint64(db, &buf, fam->name, "_count",
                                                  &m->label, NULL, s->count, m->time);
            status |= write_mongodb_metric(db, &buf, fam->name, "_sum",
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
                status |= write_mongodb_metric_uint64(db, &buf, fam->name, "_bucket", &m->label,
                                                      &label_set, h->buckets[j].counter, m->time);
            }

            status |= write_mongodb_metric_uint64(db, &buf, fam->name,
                                                  fam->type == METRIC_TYPE_HISTOGRAM ? "_count" : "_gcount",
                                                  &m->label, NULL, histogram_counter(h), m->time);
            status |= write_mongodb_metric(db, &buf, fam->name,
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

static int write_mongodb_fam(metric_family_t const *fam, user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL))
        return -1;

    write_mongodb_t *db = ud->data;


    if (write_mongodb_initialize(db, db->metric_collection_name) < 0) {
        PLUGIN_ERROR("Error making connection to server");
        return -1;
    }

    int status = write_mongodb_create_bson(db, fam);
    if (status != 0) {
        PLUGIN_ERROR("Error making insert bson");
        return -1;
    }

    return 0;
}

static int write_mongodb_flush(__attribute__((unused)) cdtime_t timeout, user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL))
        return -1;

    return write_mongodb_insert_documents(ud->data);
}

static void write_mongodb_free(void *ptr)
{
    write_mongodb_t *db = ptr;
    if (db == NULL)
        return;

    /* insert the pending documents before closing the connection */
    write_mongodb_insert_documents(db);

    if (db->collection != NULL)
        mongoc_collection_destroy(db->collection);

    if (db->database != NULL)
        mongoc_database_destroy(db->database);

    if (db->client != NULL)
        mongoc_client_destroy(db->client);

    db->connected = false;

    free(db->name);
    free(db->uri);
    free(db->host);
    free(db->database_name);
    free(db->user);
    free(db->passwd);

    free(db->metric_collection_name);
    free(db->metric_timestamp_field);
    free(db->metric_metadata_field);
    free(db->metric_value_field);

    free(db->notification_collection_name);
    free(db->notification_name_field);
    free(db->notification_severity_field);
    free(db->notification_timestamp_field);
    free(db->notification_labels_field);
    free(db->notification_annotations_field);

    for(size_t i = 0; i < db->documents_size; i++)
        bson_destroy(db->documents[i]);
    free(db->documents);

    free(db);
}

static int write_mongodb_config_database(config_item_t *ci)
{
    write_mongodb_t *db = calloc(1, sizeof(*db));
    if (db == NULL)
      return ENOMEM;

    db->host = strdup("localhost");
    if (db->host == NULL) {
        free(db);
        return ENOMEM;
    }
    db->port = MONGOC_DEFAULT_PORT;

    int status = cf_util_get_string(ci, &db->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        write_mongodb_free(db);
        return status;
    }

    db->send = SEND_METRICS;

    unsigned int bulk_size = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("uri", child->key) == 0) {
            status = cf_util_get_string(child, &db->uri);
        } else if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &db->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_port_number(child, &db->port);
        } else if (strcasecmp("store-rates", child->key) == 0) {
            PLUGIN_WARNING("Option 'store-rates' in %s:%d is obsolete and ignored.",
                           cf_get_file(child), cf_get_lineno(child));
        } else if (strcasecmp("database", child->key) == 0) {
            status = cf_util_get_string(child, &db->database_name);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &db->user);
        } else if (strcasecmp("user-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &db->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &db->passwd);
        } else if (strcasecmp("password-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &db->passwd);
        } else if (strcasecmp("metric-collection", child->key) == 0) {
            status = cf_util_get_string(child, &db->metric_collection_name);
        } else if (strcasecmp("metric-timestamp-field", child->key) == 0) {
            status = cf_util_get_string(child, &db->metric_timestamp_field);
        } else if (strcasecmp("metric-metadata-field", child->key) == 0) {
            status = cf_util_get_string(child, &db->metric_metadata_field);
        } else if (strcasecmp("metric-value-field", child->key) == 0) {
            status = cf_util_get_string(child, &db->metric_value_field);
        } else if (strcasecmp("notification-collection", child->key) == 0) {
            status = cf_util_get_string(child, &db->notification_collection_name);
        } else if (strcasecmp("notification-name-field", child->key) == 0) {
            status = cf_util_get_string(child, &db->notification_name_field);
        } else if (strcasecmp("notification-severity-field", child->key) == 0) {
            status = cf_util_get_string(child, &db->notification_severity_field);
        } else if (strcasecmp("notification-timestamp-field", child->key) == 0) {
            status = cf_util_get_string(child, &db->notification_timestamp_field);
        } else if (strcasecmp("notification-labels-field", child->key) == 0) {
            status = cf_util_get_string(child, &db->notification_labels_field);
        } else if (strcasecmp("notification-annotations-field", child->key) == 0) {
            status = cf_util_get_string(child, &db->notification_annotations_field);
        } else if (strcasecmp("ttl", child->key) == 0) {
            status = cf_util_get_int(child, &db->ttl);
        } else if (strcasecmp("bulk-size", child->key) == 0) {
            status = cf_util_get_unsigned_int(child, &bulk_size);
        } else if (strcasecmp(child->key, "write") == 0) {
            status = cf_uti_get_send(child, &db->send);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        write_mongodb_free(db);
        return -1;
    }

    if ((db->database_name == NULL) || (db->user == NULL) || (db->passwd == NULL)) {
        PLUGIN_WARNING("Authentication requires the 'database', 'user' and 'password' "
                       "options to be specified, but at last one of them is missing. "
                       "Authentication will NOT be " "used.");
    }

    if (db->database_name == NULL) {
        db->database_name = strdup("ncollectd");
        if (db->database_name == NULL) {
            PLUGIN_ERROR("strdup failed");
            write_mongodb_free(db);
            return -1;
        }
    }

    if (db->send == SEND_METRICS) {
        if (db->metric_collection_name == NULL) {
            db->metric_collection_name = strdup("metrics");
            if (db->metric_collection_name == NULL) {
                PLUGIN_ERROR("strdup failed");
                write_mongodb_free(db);
                return -1;
            }
        }

        if (db->metric_timestamp_field == NULL) {
            db->metric_timestamp_field = strdup("timestamp");
            if (db->metric_timestamp_field == NULL) {
                PLUGIN_ERROR("strdup failed");
                write_mongodb_free(db);
                return -1;
            }
        }

        if (db->metric_metadata_field == NULL) {
            db->metric_metadata_field = strdup("metadata");
            if (db->metric_metadata_field == NULL) {
                PLUGIN_ERROR("strdup failed");
                write_mongodb_free(db);
                return -1;
            }
        }

        if (db->metric_value_field == NULL) {
            db->metric_value_field = strdup("value");
            if (db->metric_value_field == NULL) {
                PLUGIN_ERROR("strdup failed");
                write_mongodb_free(db);
                return -1;
            }
        }

        db->documents_alloc = bulk_size == 0 ? 512 : bulk_size;
        db->documents_size = 0;
        db->documents = calloc(db->documents_alloc, sizeof(*db->documents));
        if (db->documents == NULL) {
            PLUGIN_ERROR("calloc failed");
            write_mongodb_free(db);
            return -1;
        }

        return plugin_register_write("write_mongodb", db->name, write_mongodb_fam,
                                     write_mongodb_flush, 0, 0,
                                     &(user_data_t){ .data = db, .free_func = write_mongodb_free });
    } else if (db->send == SEND_NOTIFICATIONS) {
        if (db->notification_collection_name == NULL) {
            db->notification_collection_name = strdup("notifications");
            if (db->notification_collection_name == NULL) {
                PLUGIN_ERROR("strdup failed");
                write_mongodb_free(db);
                return -1;
            }
        }

        if (db->notification_name_field == NULL) {
            db->notification_name_field = strdup("name");
            if (db->notification_name_field == NULL) {
                PLUGIN_ERROR("strdup failed");
                write_mongodb_free(db);
                return -1;
            }
        }

        if (db->notification_severity_field == NULL) {
            db->notification_severity_field = strdup("severity");
            if (db->notification_severity_field == NULL) {
                PLUGIN_ERROR("strdup failed");
                write_mongodb_free(db);
                return -1;
            }
        }

        if (db->notification_timestamp_field == NULL) {
            db->notification_timestamp_field = strdup("timestamp");
            if (db->notification_timestamp_field == NULL) {
                PLUGIN_ERROR("strdup failed");
                write_mongodb_free(db);
                return -1;
            }
        }

        if (db->notification_labels_field == NULL) {
            db->notification_labels_field = strdup("labels");
            if (db->notification_labels_field == NULL) {
                PLUGIN_ERROR("strdup failed");
                write_mongodb_free(db);
                return -1;
            }
        }

        if (db->notification_annotations_field == NULL) {
            db->notification_annotations_field = strdup("annotations");
            if (db->notification_annotations_field == NULL) {
                PLUGIN_ERROR("strdup failed");
                write_mongodb_free(db);
                return -1;
            }
        }

        return plugin_register_notification("write_mongodb", db->name, write_mongodb_notif,
                                     &(user_data_t){ .data = db, .free_func = write_mongodb_free });
    }

    return -1;
}

static int write_mongodb_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = write_mongodb_config_database(child);
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

static int write_mongodb_init(void)
{
    mongoc_init();
    return 0;
}

static int write_mongodb_shutdown(void)
{
    mongoc_cleanup();
    return 0;
}

void module_register(void)
{
    plugin_register_init("write_mongodb", write_mongodb_init);
    plugin_register_shutdown("write_mongodb", write_mongodb_shutdown);
    plugin_register_config("write_mongodb", write_mongodb_config);
}
