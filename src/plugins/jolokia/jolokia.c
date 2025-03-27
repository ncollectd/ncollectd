// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2009 Doug MacEachern
// SPDX-FileCopyrightText: Copyright (C) 2006-2013 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Doug MacEachern <dougm at hyperic.com>
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Wilfried dothebart Goesgens <dothebart at citadel.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/complain.h"
#include "libutils/llist.h"
#include "libutils/strbuf.h"
#include "libxson/render.h"
#include "libxson/json_parse.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <curl/curl.h>

typedef struct {
    char *name;
    label_set_t labels;
} object_name_t;

typedef struct {
    char *attribute;
    char *metric_name;
    char *help;
    metric_type_t type;
    label_set_t labels;
    label_set_t labels_from;
} jlk_mbean_attribute_t;

typedef struct {
    jlk_mbean_attribute_t *ptr;
    size_t size;
} jlk_mbean_attribute_set_t;

typedef struct {
    char *mbean;
    object_name_t on;
    char *path;
    char *metric_prefix;
    label_set_t labels;
    label_set_t labels_from;
    jlk_mbean_attribute_set_t attributes;
} jlk_mbean_t;

typedef struct {
    jlk_mbean_t *ptr;
    size_t size;
} jlk_mbean_set_t;

typedef enum {
    JLK_NONE = 0,
    JLK_VALUE,
    JLK_TIMESTAMP,
    JLK_STATUS,
    JLK_ERROR,
    JLK_REQUEST,
    JLK_REQUEST_PATH,
    JLK_REQUEST_MBEAN,
    JLK_REQUEST_ATTRIBUTE,
} jlk_status_t;

typedef struct {
    char *attribute;
    double value;
} jlk_response_value_t;

typedef struct {
    char *path;
    char *mbean;
} jlk_response_request_t;

enum {
    RESPONSE_FOUND_PATH      = 1 << 1,
    RESPONSE_FOUND_MBEAN     = 1 << 2,
    RESPONSE_FOUND_ATTRIBUTE = 1 << 3,
    RESPONSE_FOUND_VALUE     = 1 << 4,
    RESPONSE_FOUND_TIMESTAMP = 1 << 5,
    RESPONSE_FOUND_STATUS    = 1 << 6,
};

typedef struct {
    uint64_t found;
    char *request_path;
    char *request_mbean;
    char *request_attribute;
    double value;
    cdtime_t timestamp;
    int status;
} jlk_response_t;

typedef struct {
    int depth;
    jlk_status_t stack[JSON_MAX_DEPTH];
    jlk_mbean_set_t *mbeans;
    jlk_response_t response;
    char *metric_prefix;
    label_set_t *labels;
    plugin_filter_t *filter;
} jlk_parser_t;

typedef struct {
    char *instance;
    char *url;
    char *user;
    char *pass;
    char *credentials;
    bool verify_peer;
    bool verify_host;
    char *cacert;
    struct curl_slist *headers;
    cdtime_t timeout;
    strbuf_t post_body;

    CURL *curl;
    char curl_errbuf[CURL_ERROR_SIZE];
    const unsigned char *start, *end;

    char *metric_prefix;
    label_set_t labels;
    plugin_filter_t *filter;

    json_parser_t parser;
    jlk_mbean_set_t mbeans;
} jlk_t;

#if 0
mbean = "java.lang:name=*,type=GarbageCollector"
#endif

static void jlk_response_reset(jlk_response_t *response)
{
    response->found = 0;
    free(response->request_path);
    response->request_path = NULL;
    free(response->request_mbean);
    response->request_mbean = NULL;
    free(response->request_attribute);
    response->request_attribute = NULL;
    response->value = 0;
    response->timestamp = 0;
    response->status = 0;
}

static void jlk_object_name_reset(object_name_t *on)
{
    free(on->name);
    label_set_reset(&on->labels);
}

static int jlk_object_name_parse(object_name_t *on, char *str)
{
    char *labels = strchr(str, ':');
    if (labels == NULL)
        return -1;

    size_t name_len = labels - str;
    on->name = strndup(str, name_len);
    if (on->name == NULL) {
        return -1;
    }

    labels++;

    while (true) {
        if (*labels == '\0')
            return 0;

        char *key = labels;
        char *value = strchr(key, '=');
        if (value == NULL)
            return -1;

        size_t key_len = value - key;
        value++;
        if (*value == '\0')
            return -1;

        char *end = strchr(value, ',');
        size_t value_len = 0;
        if (end == NULL)
            value_len = strlen(value);
        else
            value_len = end - value;

        _label_set_add(&on->labels, true, true, key, key_len, value, value_len);

        if (end == NULL)
            break;
        labels = end + 1;
    }

    return 0;
}

static bool jlk_object_name_cmp(object_name_t *oa, object_name_t *ob)
{
    if (strcmp(oa->name, ob->name) != 0)
        return false;

    if (oa->labels.num != ob->labels.num)
        return false;

    for (size_t i = 0; i < oa->labels.num; i++) {
        label_pair_t *paira = &oa->labels.ptr[i];
        label_pair_t *pairb = &ob->labels.ptr[i];
        if (strcmp(paira->name, pairb->name) != 0)
            return false;

        if ((paira->value[0] == '*') && (paira->value[1] == '\0'))
            return false;

        if ((pairb->value[0] == '*') && (pairb->value[1] == '\0'))
            return false;

        if (strcmp(paira->value, pairb->value) != 0)
            return false;
    }

    return true;
}

static void jlk_submit(char *metric_prefix, label_set_t *labels, plugin_filter_t *filter,
                       jlk_mbean_set_t *mbeans, jlk_response_t *response)
{
    if (mbeans == NULL || response == NULL)
        return;

    object_name_t on = {0};
    int status = jlk_object_name_parse(&on, response->request_mbean);
    if (status != 0) {
        PLUGIN_ERROR("Failed to parse objet name: '%s'.", response->request_mbean);
        return;
    }


    for (size_t i = 0; i < mbeans->size; i++) {
        jlk_mbean_t *mbean = &mbeans->ptr[i];

        if (!jlk_object_name_cmp(&mbean->on, &on))
            continue;

        if (mbean->path != NULL) {
            if (response->request_path == NULL)
                continue;
            if (strcmp(mbean->path, response->request_path) != 0)
                continue;
        }

        if (response->request_attribute == NULL)
            continue;

        for (size_t j = 0; j < mbean->attributes.size; j++) {
            jlk_mbean_attribute_t *attribute = &mbean->attributes.ptr[j];

            if (strcmp(attribute->attribute, response->request_attribute) != 0)
                continue;

            label_set_t mlabels = {0};

            if (labels->num > 0)
                label_set_add_set(&mlabels, true, *labels);

            if (mbean->labels.num > 0)
                label_set_add_set(&mlabels, true, mbean->labels);

            for (size_t k =0; k < mbean->labels_from.num; k++) {
                label_pair_t *pair =  &mbean->labels_from.ptr[k];
                label_pair_t *pair_value = label_set_read(on.labels, pair->value);
                if (pair_value == NULL)
                    continue;
                label_set_add(&mlabels, true, pair->name, pair_value->value);
            }

            if (attribute->labels.num > 0)
                label_set_add_set(&mlabels, true, attribute->labels);

            for (size_t k =0; k < attribute->labels_from.num; k++) {
                label_pair_t *pair =  &attribute->labels_from.ptr[k];
                label_pair_t *pair_value = label_set_read(on.labels, pair->value);
                if (pair_value == NULL)
                    continue;
                label_set_add(&mlabels, true, pair->name, pair_value->value);
            }

            strbuf_t buf = STRBUF_CREATE;
            if (metric_prefix != NULL)
                strbuf_putstr(&buf, metric_prefix);
            if (mbean->metric_prefix != NULL)
                strbuf_putstr(&buf, mbean->metric_prefix);
            strbuf_putstr(&buf, attribute->metric_name);

            metric_family_t fam = {0};
            fam.name = buf.ptr;
            fam.type = attribute->type;
            fam.help = attribute->help;

            value_t value = {0};

            if (fam.type == METRIC_TYPE_COUNTER)
                value = VALUE_COUNTER(response->value);
            else
                value = VALUE_GAUGE(response->value);

            metric_family_append(&fam, value, &mlabels, NULL);
            plugin_dispatch_metric_family_filtered(&fam, filter, 0);

//fprintf(stderr, "[%s][%s][%s]%f\n", response->request_mbean, response->request_path, response->request_attribute, response->value);
            strbuf_destroy(&buf);
            label_set_reset(&mlabels);
        }
    }

    jlk_object_name_reset(&on);
}

static bool jlk_cb_string(void *ctx, const char *val, size_t len)
{
    jlk_parser_t *jlk_parser = (jlk_parser_t *)ctx;

//fprintf(stderr, "STRING: (%d)<%d>[%.*s]\n", (int)jlk_parser->depth, (int)jlk_parser->stack[1], (int)len, val);

    switch (jlk_parser->depth) {
    case 2:
        switch (jlk_parser->stack[1]) {
            case JLK_REQUEST_PATH:
                jlk_parser->response.request_path = strndup((const char *)val, len);
                jlk_parser->response.found |= RESPONSE_FOUND_PATH;
                break;
            case JLK_REQUEST_MBEAN:
                jlk_parser->response.request_mbean = strndup((const char *)val, len);
                jlk_parser->response.found |= RESPONSE_FOUND_MBEAN;
                break;
            case JLK_REQUEST_ATTRIBUTE:
                jlk_parser->response.request_attribute = strndup((const char *)val, len);
                jlk_parser->response.found |= RESPONSE_FOUND_ATTRIBUTE;
                break;
            default:
                break;
        }
        break;
    }

    return true;
}

static bool jlk_cb_number(void *ctx, const char *number_val, size_t number_len)
{
    jlk_parser_t *jlk_parser = (jlk_parser_t *)ctx;

    char number[256];
    if (number_len > sizeof(number))
        number_len = sizeof(number)-1;
    sstrncpy(number, number_val, number_len);

//fprintf(stderr, "NUMBER: (%d)[%.*s]\n", (int)jlk_parser->depth, (int)number_len, number_val);
    switch (jlk_parser->depth) {
    case 1:
        switch (jlk_parser->stack[0]) {
            case JLK_VALUE:
                jlk_parser->response.value = atof(number);
                jlk_parser->response.found |= RESPONSE_FOUND_VALUE;
                break;
            case JLK_TIMESTAMP:
                jlk_parser->response.timestamp = atol(number);
                jlk_parser->response.found |= RESPONSE_FOUND_TIMESTAMP;
                break;
            case JLK_STATUS:
                jlk_parser->response.status = atol(number);
                jlk_parser->response.found |= RESPONSE_FOUND_STATUS;
                break;
            default:
                break;
        }
        break;
    }

    return true;
}

static bool jlk_cb_map_key(void *ctx, const char *key, size_t key_len)
{
    jlk_parser_t *jlk_parser = (jlk_parser_t *)ctx;
//fprintf(stderr, "VAL: (%d)[%.*s]\n", (int)jlk_parser->depth, (int)string_len, key);
    switch (jlk_parser->depth) {
    case 1:
        switch(key_len) {
        case 5: // "value" "error"
            if (strncmp("value", (const char *)key, key_len) == 0) {
                jlk_parser->stack[0] = JLK_VALUE;
            } else if (strncmp("error", (const char *)key, key_len) == 0) {
                jlk_parser->stack[0] = JLK_ERROR;
            } else {
                jlk_parser->stack[0] = JLK_NONE;
            }
            break;
        case 6: // "status"
            if (strncmp("status", (const char *)key, key_len) == 0) {
                jlk_parser->stack[0] = JLK_STATUS;
            } else {
                jlk_parser->stack[0] = JLK_NONE;
            }
            break;
        case 7: // "request"
            if (strncmp("request", (const char *)key, key_len) == 0) {
                jlk_parser->stack[0] = JLK_REQUEST;
            } else {
                jlk_parser->stack[0] = JLK_NONE;
            }
            break;
        case 9: // "timestamp"
            if (strncmp("timestamp", (const char *)key, key_len) == 0) {
                jlk_parser->stack[0] = JLK_TIMESTAMP;
            } else {
                jlk_parser->stack[0] = JLK_NONE;
            }
            break;
        default:
            jlk_parser->stack[0] = JLK_NONE;
            break;
        }
        break;
    case 2:
        switch(key_len) {
        case 4: // "path"
            if (strncmp("path", (const char *)key, key_len) == 0) {
                jlk_parser->stack[1] = JLK_REQUEST_PATH;
            } else {
                jlk_parser->stack[1] = JLK_NONE;
            }
            break;
        case 5: // "mbean"
            if (strncmp("mbean", (const char *)key, key_len) == 0) {
                jlk_parser->stack[1] = JLK_REQUEST_MBEAN;
            } else {
                jlk_parser->stack[1] = JLK_NONE;
            }
            break;
        case 9: // "attribute"
            if (strncmp("attribute", (const char *)key, key_len) == 0) {
                jlk_parser->stack[1] = JLK_REQUEST_ATTRIBUTE;
            } else {
                jlk_parser->stack[1] = JLK_NONE;
            }
            break;
        default:
            jlk_parser->stack[1] = JLK_NONE;
            break;
        }
        break;
    }

    return true;
}

static bool jlk_cb_start_map(void *ctx)
{
    jlk_parser_t *jlk_parser = (jlk_parser_t *)ctx;
    jlk_parser->depth++;
    jlk_parser->stack[jlk_parser->depth] = JLK_NONE;

    return true;
}

static bool jlk_cb_end_map(void *ctx)
{
    jlk_parser_t *jlk_parser = (jlk_parser_t *)ctx;
    jlk_parser->depth--;
    if (jlk_parser->depth > 0) {
        jlk_parser->stack[jlk_parser->depth-1] = JLK_NONE;
    } else {
        jlk_parser->depth = 0;
        jlk_submit(jlk_parser->metric_prefix, jlk_parser->labels, jlk_parser->filter,
                   jlk_parser->mbeans, &jlk_parser->response);
        jlk_response_reset(&jlk_parser->response);
    }

    return true;
}

static xson_callbacks_t jlk_callbacks = {
    .xson_null        = NULL,
    .xson_boolean     = NULL,
    .xson_integer     = NULL,
    .xson_double      = NULL,
    .xson_number      = jlk_cb_number,
    .xson_string      = jlk_cb_string,
    .xson_start_map   = jlk_cb_start_map,
    .xson_map_key     = jlk_cb_map_key,
    .xson_end_map     = jlk_cb_end_map,
    .xson_start_array = NULL,
    .xson_end_array   = NULL,
};

static size_t jlk_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    size_t len = size * nmemb;
    if (len <= 0)
        return len;

    jlk_t *jlk = user_data;
    if (jlk == NULL)
        return 0;

    json_status_t status = json_parser_parse(&jlk->parser, buf, len);
    if (status != JSON_STATUS_OK)
        return -1;

    return len;
}

static int jlk_init_curl(jlk_t *jlk)
{
    if (jlk->curl != NULL)
        return 0;

    jlk->curl = curl_easy_init();
    if (jlk->curl == NULL) {
        PLUGIN_ERROR("curl_easy_init failed.");
        return -1;
    }

    CURLcode rcode = 0;

    rcode = curl_easy_setopt(jlk->curl, CURLOPT_NOSIGNAL, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_NOSIGNAL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(jlk->curl, CURLOPT_WRITEFUNCTION, jlk_curl_callback);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEFUNCTION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(jlk->curl, CURLOPT_WRITEDATA, jlk);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEDATA failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(jlk->curl, CURLOPT_USERAGENT, PACKAGE_NAME "/" PACKAGE_VERSION);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERAGENT failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(jlk->curl, CURLOPT_ERRORBUFFER, jlk->curl_errbuf);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(jlk->curl, CURLOPT_URL, jlk->url);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_URL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }


    if (jlk->user != NULL) {
        size_t credentials_size = strlen(jlk->user)+2;
        if (jlk->pass != NULL)
            credentials_size += strlen(jlk->pass);

        jlk->credentials = calloc(1, credentials_size);
        if (jlk->credentials == NULL) {
            PLUGIN_ERROR("calloc failed.");
            return -1;
        }

        ssnprintf(jlk->credentials, credentials_size, "%s:%s", jlk->user,
                                  (jlk->pass == NULL) ? "" : jlk->pass);
        rcode = curl_easy_setopt(jlk->curl, CURLOPT_USERPWD, jlk->credentials);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERPWD failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

    rcode = curl_easy_setopt(jlk->curl, CURLOPT_SSL_VERIFYPEER, (long)jlk->verify_peer);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_VERIFYPEER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(jlk->curl, CURLOPT_SSL_VERIFYHOST, jlk->verify_host ? 2L : 0L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_VERIFYHOST failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    if (jlk->cacert != NULL) {
        rcode = curl_easy_setopt(jlk->curl, CURLOPT_CAINFO, jlk->cacert);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_CAINFO failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

    if (jlk->headers != NULL) {
        rcode = curl_easy_setopt(jlk->curl, CURLOPT_HTTPHEADER, jlk->headers);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_HTTPHEADER failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

    if (strbuf_len(&jlk->post_body) > 0) {
        rcode = curl_easy_setopt(jlk->curl, CURLOPT_POSTFIELDS, jlk->post_body.ptr);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_POSTFIELDS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

#ifdef HAVE_CURLOPT_TIMEOUT_MS
    if (jlk->timeout > 0) {
        rcode = curl_easy_setopt(jlk->curl, CURLOPT_TIMEOUT_MS,
                                            (long)CDTIME_T_TO_MS(jlk->timeout));
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    } else {
        rcode = curl_easy_setopt(jlk->curl, CURLOPT_TIMEOUT_MS,
                                            (long)CDTIME_T_TO_MS(plugin_get_interval()));
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }
#endif

    return 0;
}

static int jlk_curl_perform(jlk_t *jlk)
{
    char *url = NULL;

    curl_easy_getinfo(jlk->curl, CURLINFO_EFFECTIVE_URL, &url);

    int status = curl_easy_perform(jlk->curl);
    if (status != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_perform failed with status %i: %s (%s)",
                     status, jlk->curl_errbuf, (url != NULL) ? url : "<null>");
        return -1;
    }

    long rc;
    curl_easy_getinfo(jlk->curl, CURLINFO_RESPONSE_CODE, &rc);

    /* The response code is zero if a non-HTTP transport was used. */
    if ((rc != 0) && (rc != 200)) {
        PLUGIN_ERROR("curl_easy_perform failed with response code %ld (%s)", rc, url);
        return -1;
    }

    return 0;
}

static int jlk_read(user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }
    jlk_t *jlk = ud->data;

    if (jlk->curl == NULL) {
        int status = jlk_init_curl(jlk);
        if (status != 0)
            return -1;
    }

    jlk_parser_t ctx = {0};

    ctx.mbeans = &jlk->mbeans;
    ctx.metric_prefix = jlk->metric_prefix;
    ctx.labels = &jlk->labels;
    ctx.filter = jlk->filter;

    json_parser_init(&jlk->parser, 0, &jlk_callbacks, &ctx);

    int status = jlk_curl_perform(jlk);
    if (status < 0) {
        json_parser_free (&jlk->parser);
        return -1;
    }

    status = json_parser_complete(&jlk->parser);
    if (status != JSON_STATUS_OK) {
        unsigned char *errmsg = json_parser_get_error(&jlk->parser, 1, NULL, 0);
        PLUGIN_ERROR("json_parse_complete failed: %s", (const char *)errmsg);
        json_parser_free_error(errmsg);
        json_parser_free (&jlk->parser);
        return -1;
    }

    json_parser_free(&jlk->parser);
    return 0;
}

static int jlk_build_post(jlk_t *jlk)
{
    xson_render_t r = {0};
    xson_render_init(&r, &jlk->post_body, XSON_RENDER_TYPE_JSON, 0);

    int status = xson_render_array_open(&r);
    for (size_t i = 0; i < jlk->mbeans.size; i++) {
        jlk_mbean_t *mbean = &jlk->mbeans.ptr[i];

        if (mbean->attributes.size == 0)
            continue;

        status |= xson_render_map_open(&r);

        status |= xson_render_key_string(&r, "type");
        status |= xson_render_string(&r, "read");

        if (mbean->path != NULL) {
            status |= xson_render_key_string(&r, "path");
            status |= xson_render_string(&r, mbean->path);
        }

        if (mbean->mbean != NULL) {
            status |= xson_render_key_string(&r, "mbean");
            status |= xson_render_string(&r, mbean->mbean);
        }

        if (mbean->attributes.size > 0) {
            jlk_mbean_attribute_set_t *attributes = &mbean->attributes;

            if (attributes->size == 1) {
                jlk_mbean_attribute_t *attribute = &attributes->ptr[0];
                if (attribute->attribute != NULL) {
                    status |= xson_render_key_string(&r, "attribute");
                    status |= xson_render_string(&r, attribute->attribute);
                }
            } else {
                status |= xson_render_key_string(&r, "attribute");
                status |= xson_render_array_open(&r);
                for (size_t j = 0; j < attributes->size; j++) {
                    jlk_mbean_attribute_t *attribute = &attributes->ptr[j];
                    if (attribute->attribute != NULL)
                        status |= xson_render_string(&r, attribute->attribute);
                }
                status |= xson_render_array_close(&r);
            }
        }

        status |= xson_render_map_close(&r);
    }

    status |= xson_render_array_close(&r);

    return status;
}

static int jlk_config_append_string(const char *name, struct curl_slist **dest, config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("`%s' needs exactly one string argument.", name);
        return -1;
    }

    *dest = curl_slist_append(*dest, ci->values[0].value.string);
    if (*dest == NULL)
        return -1;

    return 0;
}

static void jlk_mbean_attribute_free(jlk_mbean_attribute_t *attribute)
{
    if (attribute == NULL)
        return;
    free(attribute->attribute);
    free(attribute->metric_name);
    label_set_reset(&attribute->labels);
}

static void jlk_mbean_attribute_set_free(jlk_mbean_attribute_set_t *attributes)
{
    for (size_t i = 0; i < attributes->size; i++) {
        jlk_mbean_attribute_free(&attributes->ptr[i]);
    }
    free(attributes->ptr);
    attributes->size = 0;
}

static void jlk_mbean_free(jlk_mbean_t *mbean)
{
    if (mbean == NULL)
        return;
    free(mbean->mbean);
    jlk_object_name_reset(&mbean->on);
    free(mbean->path);
    free(mbean->metric_prefix);
    label_set_reset(&mbean->labels);
    jlk_mbean_attribute_set_free(&mbean->attributes);
}

static void jlk_mbean_set_free(jlk_mbean_set_t *mbeans)
{
    for (size_t i = 0; i < mbeans->size; i++) {
        jlk_mbean_free(&mbeans->ptr[i]);
    }
    free(mbeans->ptr);
    mbeans->size = 0;
}

static void jlk_free(void *arg)
{
    jlk_t *jlk = (jlk_t *)arg;
    if (jlk == NULL)
        return;

    if (jlk->curl != NULL)
        curl_easy_cleanup(jlk->curl);
    jlk->curl = NULL;
#if 0
    jlk_destroy_bean_configs(jlk->bean_configs);
    jlk->bean_configs = NULL;
#endif
    free(jlk->instance);

    free(jlk->url);
    free(jlk->user);
    free(jlk->pass);
    free(jlk->credentials);
    free(jlk->cacert);
    strbuf_destroy(&jlk->post_body);


    curl_slist_free_all(jlk->headers);
    /// todo: freeme    jlk_attribute_values_t *attributepool;

    jlk_mbean_set_free(&jlk->mbeans);

    label_set_reset(&jlk->labels);
    plugin_filter_free(jlk->filter);

    free(jlk);
}


static int jlk_config_add_mbean_attribute(config_item_t *ci, jlk_mbean_attribute_set_t *attributes)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The 'attribute' block needs exactly one string argument.");
        return -1;
    }

    jlk_mbean_attribute_t *tmp = realloc(attributes->ptr,
                                 sizeof(*attributes->ptr) * (attributes->size + 1));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        return -1;
    }
    attributes->ptr= tmp;
    jlk_mbean_attribute_t *attribute = &attributes->ptr[attributes->size];
    memset(attribute, 0, sizeof(*attribute));
    attributes->size++;

    int status = cf_util_get_string(ci, &attribute->attribute);
    if (status != 0) {
        jlk_mbean_attribute_free(attribute);
        attributes->size--;
        return status;
    }

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("metric", child->key) == 0) {
            status = cf_util_get_string(child, &attribute->metric_name);
        } else if (strcasecmp("type", child->key) == 0) {
            status = cf_util_get_metric_type(child, &attribute->type);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &attribute->labels);
        } else if (strcasecmp("label-from", child->key) == 0) {
            status = cf_util_get_label(child, &attribute->labels_from);
        } else if (strcasecmp("help", child->key) == 0) {
            status = cf_util_get_string(child, &attribute->help);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0)
        return -1;

    return 0;
}

static int jlk_config_add_mbean(config_item_t *ci, jlk_mbean_set_t *mbeans)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The 'mbean' block needs exactly one string argument.");
        return -1;
    }

    jlk_mbean_t *tmp = realloc(mbeans->ptr, sizeof(*mbeans->ptr) * (mbeans->size + 1));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        return -1;
    }
    mbeans->ptr= tmp;
    jlk_mbean_t *mbean = &mbeans->ptr[mbeans->size];
    memset(mbean, 0, sizeof(*mbean));
    mbeans->size++;

    int status = cf_util_get_string(ci, &mbean->mbean);
    if (status != 0) {
        jlk_mbean_free(mbean);
        mbeans->size--;
        return status;
    }

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("path", child->key) == 0) {
            status = cf_util_get_string(child, &mbean->path);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &mbean->metric_prefix);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &mbean->labels);
        } else if (strcasecmp("label-from", child->key) == 0) {
            status = cf_util_get_label(child, &mbean->labels_from);
        } else if (strcasecmp("attribute", child->key) == 0) {
            status = jlk_config_add_mbean_attribute(child, &mbean->attributes);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0)
        return -1;

    status = jlk_object_name_parse(&mbean->on, mbean->mbean);
    if (status != 0) {
        PLUGIN_ERROR("Failed to parse mbeam: '%s'.", mbean->mbean);
        return -1;
    }

    return 0;
}

static int jlk_config_add_instance(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The 'instance' block needs exactly one string argument.");
        return -1;
    }

    jlk_t *jlk = (jlk_t *)calloc(1, sizeof(jlk_t));
    if (jlk == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &jlk->instance);
    if (status != 0) {
        free(jlk);
        return status;
    }

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &jlk->url);
        } else if (jlk->url && strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &jlk->user);
        } else if (jlk->url && strcasecmp("user-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &jlk->user);
        } else if (jlk->url && strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &jlk->pass);
        } else if (jlk->url && strcasecmp("password-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &jlk->pass);
        } else if (jlk->url && strcasecmp("verify-peer", child->key) == 0) {
            status = cf_util_get_boolean(child, &jlk->verify_peer);
        } else if (jlk->url && strcasecmp("verify-host", child->key) == 0) {
            status = cf_util_get_boolean(child, &jlk->verify_host);
        } else if (jlk->url && strcasecmp("ca-cert", child->key) == 0) {
            status = cf_util_get_string(child, &jlk->cacert);
        } else if (jlk->url && strcasecmp("header", child->key) == 0) {
            status = jlk_config_append_string("Header", &jlk->headers, child);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &jlk->timeout);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &jlk->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &jlk->metric_prefix);
        } else if (strcasecmp("mbean", child->key) == 0) {
            status = jlk_config_add_mbean(child, &jlk->mbeans);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &jlk->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        jlk_free(jlk);
        return -1;
    }

    struct curl_slist *slist = curl_slist_append(jlk->headers, "Content-Type: application/json");
    if (slist == NULL) {
        PLUGIN_ERROR("curl_slist_append failed");
        jlk_free(jlk);
        return -1;
    }

    jlk->headers = slist;

    status = jlk_build_post(jlk);
    if (status != 0) {
        PLUGIN_ERROR("Failed to build POST data");
        jlk_free(jlk);
        return -1;
    }

    return plugin_register_complex_read("jolokia", jlk->instance, jlk_read, interval,
                                        &(user_data_t){.data=jlk, .free_func=jlk_free});
}

static int jlk_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = jlk_config_add_instance(child);
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
    plugin_register_config("jolokia", jlk_config);
}
