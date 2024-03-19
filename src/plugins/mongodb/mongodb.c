// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2010 Ryan Cox
// SPDX-FileCopyrightText: Copyright (C) 2012 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2013 John (J5) Palmieri
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Ryan Cox <ryan.a.cox at gmail.com>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: John (J5) Palmieri <j5 at stackdriver.com>
// SPDX-FileContributor: Corey Kosak <kosak at google.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#pragma GCC diagnostic ignored "-Wcast-qual"

#include <mongoc.h>
#include <bson.h>

#include "plugins/mongodb/mongodb_fam.h"
#include "plugins/mongodb/mongodb_stats.h"

extern metric_family_t fams_mongodb[FAM_MONGODB_MAX];

typedef struct {
    char *name;
    char *host;
    int port;
    char *user;
    char *password;
    label_set_t labels;
    bool prefer_secondary_query;
    mongoc_client_t *client;
    metric_family_t fams[FAM_MONGODB_MAX];
} mongodb_inst_t;

static void mongodb_free(void *arg)
{
    mongodb_inst_t *ctx = arg;
    if (ctx == NULL)
        return;

    free(ctx->name);
    free(ctx->host);
    free(ctx->user);
    free(ctx->password);
    label_set_reset(&ctx->labels);
    mongoc_client_destroy(ctx->client);

    free(ctx);
}

static int mongodb_cmd_ping(mongodb_inst_t *ctx)
{
    bson_t *cmd = BCON_NEW ("ping", BCON_INT32 (1));

    bson_t reply = BSON_INITIALIZER;
    bson_error_t error;
    if (!mongoc_client_command_simple(ctx->client, "admin", cmd, NULL, &reply, &error)) {
        PLUGIN_ERROR("%s", error.message);
        bson_destroy (&reply);
        bson_destroy (cmd);
        return -1;
    }

    bson_destroy (&reply);
    bson_destroy (cmd);
    return 0;
}

static int mongodb_connect(mongodb_inst_t *ctx)
{
    if (ctx->client != NULL) {
        if (mongodb_cmd_ping(ctx) == 0)
            return 0;

        mongoc_client_destroy(ctx->client);
    }

    char uri[1024];

    int result = 0;
    if (ctx->user != NULL) {
        result = snprintf(uri, sizeof(uri), "mongodb://%s:%s@%s:%d/admin",
                          ctx->user, ctx->password, ctx->host, ctx->port);
    } else {
        result = snprintf(uri, sizeof(uri), "mongodb://%s:%d/admin", ctx->host, ctx->port);
    }

    if (result < 0 ||  result >= (int)(sizeof(uri))) {
        PLUGIN_ERROR("no space in buffer for build connection uri");
        return -1;
    }

    ctx->client = mongoc_client_new(uri);
    if (ctx->client == NULL) {
        PLUGIN_ERROR("mongoc_client_new failed.");
        return -1;
    }

    if (mongodb_cmd_ping(ctx) == 0)
       return 0;

    return -1;
}

static int mongodb_metric_append(mongodb_inst_t *ctx, bson_iter_t *iter, strbuf_t *buf,
                                 char *label, char *key)
{
    const struct mongodb_stats *mdbs = mongodb_stats_get_key(buf->ptr, strbuf_len(buf));
    if (mdbs == NULL) {
//      fprintf(stderr, "%s\n", buf->ptr);
        return -1;
    }

    metric_family_t *fam = &ctx->fams[mdbs->fam];
    double scale = mdbs->scale;

    value_t value = {0};

    switch (bson_iter_type(iter)) {
    case BSON_TYPE_INT32:
    case BSON_TYPE_INT64: {
        int64_t num = bson_iter_type(iter) == BSON_TYPE_INT32 ?
                      bson_iter_int32(iter) : bson_iter_int64(iter);
        if (fam->type == METRIC_TYPE_COUNTER) {
            if (scale != 0.0)
                value = VALUE_COUNTER_FLOAT64(num * scale);
            else
                value = VALUE_COUNTER(num);
        } else if (fam->type == METRIC_TYPE_GAUGE) {
            if (scale != 0.0)
                value = VALUE_GAUGE(num * scale);
            else
                value = VALUE_GAUGE(num);
        } else {
            return -1;
        }
    }   break;
    case BSON_TYPE_DOUBLE: {
        double num = bson_iter_double(iter);
        if (fam->type == METRIC_TYPE_COUNTER) {
            if (scale != 0.0)
                value = VALUE_COUNTER_FLOAT64(num * scale);
            else
                value = VALUE_COUNTER_FLOAT64(num);
        } else if (fam->type == METRIC_TYPE_GAUGE) {
            if (scale != 0.0)
                value = VALUE_GAUGE(num * scale);
            else
                value = VALUE_GAUGE(num);
        } else {
            return -1;
        }
    }   break;
    default:
        PLUGIN_ERROR("unrecognized iter type %s %u.", buf->ptr, bson_iter_type(iter));
        return -1;
    }

    metric_family_append(fam, value, &ctx->labels,
                         &(label_pair_const_t){.name=mdbs->label1, .value=mdbs->key1},
                         &(label_pair_const_t){.name=mdbs->label2, .value=mdbs->key2},
                         &(label_pair_const_t){.name=label, .value=key},
                         NULL);

    return 0;
}

static int mongodb_process_database(mongodb_inst_t *ctx, char *db_name)
{
    bson_t reply = BSON_INITIALIZER;
    bson_error_t error;
    int result = -1;

    bson_t *request = BCON_NEW("dbStats", BCON_INT32(1),
                               "scale", BCON_INT32(1),
                               "freeStorage", BCON_INT32(1));
    if (!mongoc_client_command_simple(ctx->client, db_name, request, NULL, &reply, &error)) {
        PLUGIN_ERROR("dbStats command failed: %s.", error.message);
        goto leave;
    }

    bson_iter_t iter;
    if (!bson_iter_init(&iter, &reply)) {
        PLUGIN_ERROR("bson_iter_init failed.");
        goto leave;
    }

    char buffer[128];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);

    strbuf_putstr(&buf, "dbStats.");
    size_t offset = strbuf_offset(&buf);
    while (bson_iter_next (&iter)) {
        strbuf_resetto(&buf, offset);
        strbuf_putstr(&buf, bson_iter_key(&iter));
        if (bson_iter_type(&iter) != BSON_TYPE_DOCUMENT)
            mongodb_metric_append(ctx, &iter, &buf, "database", db_name);
    }

    result = 0;
 leave:
    bson_destroy(&reply);
    bson_destroy(request);
    return result;
}

// This code is identical to the method "mongoc_client_get_database_names"
// in the Mongo driver, except that it doesn't filter out the database
// called "local". This allows us to correctly enumerate and gather stats
// from all databases when there is a "local" database (the normal case),
// and when there is not (as in the sharding case).
static char ** mongodb_get_database_names (mongoc_client_t *client, bson_error_t *error)
{
     char **ret = NULL;

     BSON_ASSERT (client);

     mongoc_cursor_t *cursor = mongoc_client_find_databases_with_opts (client, NULL);

     int i = 0;
     const bson_t *doc;
     while (mongoc_cursor_next (cursor, &doc)) {
            const char *name;
            bson_iter_t iter;
            if (bson_iter_init (&iter, doc) && bson_iter_find (&iter, "name") &&
                BSON_ITER_HOLDS_UTF8 (&iter) && (name = bson_iter_utf8 (&iter, NULL))) {
                ret = (char **)bson_realloc (ret, sizeof(char*) * (i + 2));
                ret [i] = bson_strdup (name);
                ret [++i] = NULL;
            }
     }

     if (!ret && !mongoc_cursor_error (cursor, error)) {
            ret = (char **)bson_malloc0 (sizeof (void*));
     }

     mongoc_cursor_destroy (cursor);

    return ret;
}

static int mongodb_lookup(mongodb_inst_t *ctx, bson_iter_t *iter, strbuf_t *buf, int depth)
{
    if (BSON_ITER_HOLDS_DOCUMENT(iter)) {
        const uint8_t *docbuf = NULL;
        uint32_t doclen = 0;
        bson_iter_document(iter, &doclen, &docbuf);

        bson_t b;
        if (bson_init_static (&b, docbuf, doclen)) {
            bson_iter_t siter;
            if (bson_iter_init(&siter, &b)) {
                size_t offset = strbuf_offset(buf);
                while (bson_iter_next (&siter)) {
                    if (depth > 0)
                        strbuf_putchar(buf, '.');
                    strbuf_putstr(buf, bson_iter_key(&siter));
                    if (BSON_ITER_HOLDS_DOCUMENT(&siter)) {
                        mongodb_lookup(ctx, &siter, buf, depth + 1);
                    } else {
                        mongodb_metric_append(ctx, &siter, buf, NULL, NULL);
                    }
                    strbuf_resetto(buf, offset);
                }
            }
        }
    }

    return 0;
}

static int mongodb_server_status(mongodb_inst_t *ctx)
{
    bson_t cmd = BSON_INITIALIZER;
    BSON_APPEND_INT32 (&cmd, "serverStatus", 1);

    bson_t reply = BSON_INITIALIZER;
    bson_error_t error;
    if (!mongoc_client_command_simple(ctx->client, "admin", &cmd, NULL, &reply, &error)) {
        PLUGIN_ERROR("serverStatus  failed: %s.", error.message);
        bson_destroy(&cmd);
        return -1;
    }

    bson_iter_t iter;
    if (!bson_iter_init(&iter, &reply)) {
        PLUGIN_ERROR("bson_iter_init failed.");
        bson_destroy(&cmd);
        bson_destroy(&reply);
        return -1;
    }

    char buffer[256];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);

    while (bson_iter_next (&iter)) {
        strbuf_reset(&buf);
        strbuf_putstr(&buf, bson_iter_key(&iter));
        if (BSON_ITER_HOLDS_DOCUMENT(&iter)) {
            mongodb_lookup(ctx, &iter, &buf, 1);
        } else {
            mongodb_metric_append(ctx, &iter, &buf, NULL, NULL);
        }
    }

    bson_destroy(&cmd);
    bson_destroy(&reply);
    return 0;
}

static int mongodb_read(user_data_t *user_data)
{
    mongodb_inst_t *ctx = user_data->data;

    int status = mongodb_connect(ctx);
    if (status != 0) {
        metric_family_append(&ctx->fams[FAM_MONGODB_UP], VALUE_GAUGE(0), &ctx->labels, NULL);
        plugin_dispatch_metric_family(&ctx->fams[FAM_MONGODB_UP], 0);
        return 0;
    }

    metric_family_append(&ctx->fams[FAM_MONGODB_UP], VALUE_GAUGE(1), &ctx->labels, NULL);

    mongodb_server_status(ctx);

    bson_error_t error;
    // Get the list of databases, then process each database.
    char **databases = mongodb_get_database_names(ctx->client, &error);
    if (databases != NULL) {
        for (int i = 0; databases[i] != NULL; ++i) {
            if (mongodb_process_database(ctx, databases[i]) != 0) {
                // If there's an error, maybe it's only on one of the databases.
                PLUGIN_WARNING("mongodb_process_database '%s' failed. Continuing anyway...",
                               databases[i]);
            }
        }
        bson_strfreev(databases);
    } else {
        PLUGIN_ERROR("mongoc_client_get_database_names failed: %s.", error.message);
    }

    plugin_dispatch_metric_family_array(ctx->fams, FAM_MONGODB_MAX, 0);

    return 0;
}

static int mongodb_config_instance(config_item_t *ci)
{
    mongodb_inst_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    memcpy(ctx->fams, fams_mongodb, sizeof(ctx->fams[0])*FAM_MONGODB_MAX);

    int status = cf_util_get_string(ci, &ctx->name);
    if (status != 0) {
        free(ctx);
        return status;
    }

    ctx->port = MONGOC_DEFAULT_PORT;
    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_port_number(child, &ctx->port);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->password);
        } else if (strcasecmp("prefer-secondary-query", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctx->prefer_secondary_query);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = 1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        mongodb_free(ctx);
        return -1;
    }

    if (ctx->host == NULL) {
        ctx->host = strdup("localhost");
        if (ctx->host == NULL) {
            PLUGIN_ERROR("strdup failed.");
            mongodb_free(ctx);
            return -1;
        }
    }

    if ((ctx->user == NULL && ctx->password != NULL) ||
        (ctx->user != NULL && ctx->password == NULL)) {
        PLUGIN_ERROR("User and Password in the config either need to both"
                     " be specified or both be unspecified.");
        mongodb_free(ctx);
        return -1;
    }

    return plugin_register_complex_read("mongodb", ctx->name, mongodb_read, interval,
                                        &(user_data_t){.data=ctx, .free_func=mongodb_free});
}

static int mongodb_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = mongodb_config_instance(child);
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

static int mongodb_init(void)
{
    mongoc_init();
    return 0;
}

static int mongodb_shutdown(void)
{
    mongoc_cleanup();
    return 0;
}

void module_register(void)
{
    plugin_register_init("mongodb", &mongodb_init);
    plugin_register_config("mongodb", &mongodb_config);
    plugin_register_shutdown ("mongodb", &mongodb_shutdown);
}
