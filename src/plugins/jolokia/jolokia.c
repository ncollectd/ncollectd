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
#include "libutils/avltree.h"
#include "libxson/render.h"
#include "libxson/json_parse.h"
#include "libxson/tree.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <curl/curl.h>

#define CHAR_GS 0x1D
#define CHAR_RS 0x1E

#define MBEAN_MAX_KEY_SIZE  4096

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
    char *name;
    c_avl_tree_t *mbeans;
} jlk_mbean_set_t;

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

    CURL *curl;
    char curl_errbuf[CURL_ERROR_SIZE];

    char *metric_prefix;
    label_set_t labels;
    plugin_filter_t *filter;

    size_t mbeans_size;
    char **post_body;
    size_t *mbeans;
} jlk_t;

size_t g_mbean_set_num;
static jlk_mbean_set_t *g_mbean_set;

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

    labels++;

    size_t name_len = labels - str;
    on->name = strndup(str, name_len);
    if (on->name == NULL)
        return -1;

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
            continue;

        if ((pairb->value[0] == '*') && (pairb->value[1] == '\0'))
            continue;

        if (strcmp(paira->value, pairb->value) != 0)
            return false;
    }

    return true;
}

static int jlk_object_name_to_str(object_name_t *on, strbuf_t *buf)
{
    int status = strbuf_putstr(buf, on->name);
    for (size_t i=0; i < on->labels.num; i++) {
        if (i != 0)
            status |= strbuf_putchar(buf, ',');
        label_pair_t *pair = &on->labels.ptr[i];
        status |= strbuf_putstr(buf, pair->name);
        status |= strbuf_putchar(buf, '=');
        status |= strbuf_putstr(buf, pair->value);
    }
    return status;
}

static char *jlk_mbean_key(jlk_mbean_t *mbean)
{
    char buff[MBEAN_MAX_KEY_SIZE];
    strbuf_t buf = STRBUF_CREATE_STATIC(buff);

    object_name_t on = {0};

    int status = jlk_object_name_parse(&on, mbean->mbean);
    if (status != 0) {
        jlk_object_name_reset(&on);
        return NULL;
    }

    status = jlk_object_name_to_str(&on, &buf);
    status |= strbuf_putchar(&buf, CHAR_GS);
    if (mbean->path!= NULL)
        status |= strbuf_putstr(&buf, mbean->path);

    status |= strbuf_putchar(&buf, CHAR_GS);
    for (size_t i = 0; i < mbean->attributes.size; i++) {
        if (i != 0)
            status |= strbuf_putchar(&buf, CHAR_RS);
        status |= strbuf_putstr(&buf, mbean->attributes.ptr[i].attribute);
    }

    jlk_object_name_reset(&on);

    if (status != 0)
        return NULL;

    return strdup(buf.ptr);
}

static jlk_mbean_attribute_t *jlk_mbean_find_attribute(jlk_mbean_t *mbean, char *name)
{
    for (size_t i = 0; i < mbean->attributes.size; i++) {
        jlk_mbean_attribute_t *attribute = &mbean->attributes.ptr[i];
        if (strcmp(name, attribute->attribute) == 0)
            return attribute;
    }

    return NULL;
}

static void jlk_submit(jlk_t *jlk, jlk_mbean_t *mbean, jlk_mbean_attribute_t *attribute,
                                   object_name_t *on, xson_value_t *nvalue, double timestamp)
{
    if (mbean == NULL)
        return;

    double number = 0.0;
    if (xson_value_is_number(nvalue)) {
        number = xson_value_get_number(nvalue);
    } else if (xson_value_is_bool(nvalue)) {
        number = xson_value_get_boolean(nvalue) ? 1.0 : 0.0;
    } else {
        return;
    }

    value_t value = {0};

    if (attribute->type == METRIC_TYPE_COUNTER) {
        uint64_t inumber = number;  
        if (number != (double)inumber)
            value = VALUE_COUNTER_FLOAT64(number);
        else
            value = VALUE_COUNTER(inumber);
    } else if (attribute->type == METRIC_TYPE_GAUGE) {
        value = VALUE_GAUGE(number);
    } else {
        return;
    }

    label_set_t mlabels = {0};

    if (jlk->labels.num > 0)
        label_set_add_set(&mlabels, true, jlk->labels);

    if (mbean->labels.num > 0)
        label_set_add_set(&mlabels, true, mbean->labels);

    for (size_t k =0; k < mbean->labels_from.num; k++) {
        label_pair_t *pair =  &mbean->labels_from.ptr[k];
        label_pair_t *pair_value = label_set_read(on->labels, pair->value);
        if (pair_value == NULL)
            continue;
        label_set_add(&mlabels, true, pair->name, pair_value->value);
    }

    if (attribute->labels.num > 0)
        label_set_add_set(&mlabels, true, attribute->labels);

    for (size_t k =0; k < attribute->labels_from.num; k++) {
        label_pair_t *pair =  &attribute->labels_from.ptr[k];
        label_pair_t *pair_value = label_set_read(on->labels, pair->value);
        if (pair_value == NULL)
            continue;
        label_set_add(&mlabels, true, pair->name, pair_value->value);
    }

    strbuf_t buf = STRBUF_CREATE;
    if (jlk->metric_prefix != NULL)
        strbuf_putstr(&buf, jlk->metric_prefix);
    if (mbean->metric_prefix != NULL)
        strbuf_putstr(&buf, mbean->metric_prefix);
    strbuf_putstr(&buf, attribute->metric_name);

    metric_family_t fam = {0};
    fam.name = buf.ptr;
    fam.type = attribute->type;
    fam.help = attribute->help;

    metric_family_append(&fam, value, &mlabels, NULL);

    plugin_dispatch_metric_family_filtered(&fam, jlk->filter, TIME_T_TO_CDTIME_T(timestamp));

    strbuf_destroy(&buf);
    label_set_reset(&mlabels);

    return;
}

static int jlk_parse_response_value(jlk_t *jlk, jlk_mbean_t *mbean, xson_value_t *value,
                                    double timestamp)
{
    if (xson_value_is_object(value)) {
        for (size_t i = 0; i < xson_value_object_size(value); i++) {
            xson_keyval_t *kv = xson_value_object_at(value, i);
            if (kv == NULL)
                break;

            char *key = xson_keyval_key(kv);
            xson_value_t *key_value =  xson_keyval_value(kv);

            if (xson_value_is_object(key_value)) {
                object_name_t on = {0};
                int status = jlk_object_name_parse(&on, key);
                if (status != 0) {
                    PLUGIN_ERROR("Failed to parse objet name: '%s'.", key);
                    jlk_object_name_reset(&on);
                    return -1;
                }

                if (!jlk_object_name_cmp(&mbean->on, &on)) {
                    jlk_object_name_reset(&on);
                    continue;
                }

                for (size_t j = 0; j < xson_value_object_size(key_value); j++) {
                    xson_keyval_t *akv = xson_value_object_at(key_value, j);
                    if (akv == NULL)
                        break;
                    char *akey = xson_keyval_key(akv);
                    xson_value_t *akey_value =  xson_keyval_value(akv);

                    jlk_mbean_attribute_t *attribute = jlk_mbean_find_attribute(mbean, akey);
                    if (attribute == NULL)
                        continue;

                    jlk_submit(jlk, mbean, attribute, &on, akey_value, timestamp);
                }

                jlk_object_name_reset(&on);
            } else {
                jlk_mbean_attribute_t *attribute = jlk_mbean_find_attribute(mbean, key);
                if (attribute == NULL)
                    continue;
                jlk_submit(jlk, mbean, attribute, &mbean->on, key_value, timestamp);
            }
        }
    } else {
        if (mbean->attributes.size != 1)
            return -1;
        jlk_submit(jlk, mbean, &mbean->attributes.ptr[0], &mbean->on, value, timestamp);
    }

    return 0;
}

static int jlk_parse_response_request_to_key(xson_value_t *tree, strbuf_t *buf)
{
    if (!xson_value_is_object(tree))
        return -1;

    char *path = NULL;
    char *mbean = NULL;
    xson_value_t *attributes = NULL;

    for (size_t i = 0; i < xson_value_object_size(tree); i++) {
        xson_keyval_t *kv = xson_value_object_at(tree, i);
        if (kv == NULL)
            break;

        char *key = xson_keyval_key(kv);

        if (strcmp(key, "path") == 0) {
            path = xson_value_get_string(xson_keyval_value(kv));
        } else if (strcmp(key, "mbean") == 0) {
            mbean = xson_value_get_string(xson_keyval_value(kv));
        } else if (strcmp(key, "attribute") == 0) {
            attributes = xson_keyval_value(kv);
        }
    }

    if (mbean == NULL)
        return -1;

    int status = strbuf_putstr(buf, mbean);
    status |= strbuf_putchar(buf, CHAR_GS);
    if (path != NULL)
        status |= strbuf_putstr(buf, path);
    status |= strbuf_putchar(buf, CHAR_GS);

    if (attributes != NULL) {
        if (xson_value_is_array(attributes)) {
            for (size_t i = 0; i < xson_value_array_size(attributes); i++) {
                char *attribute = xson_value_get_string(xson_value_array_at(attributes, i));
                if (attribute == NULL)
                    return -1;
                if (i != 0)
                    status |= strbuf_putchar(buf, CHAR_RS);
                status |= strbuf_putstr(buf, attribute);
            }
        } else if (xson_value_is_string(attributes)) {
            char *attribute = xson_value_get_string(attributes);
            if (attribute != NULL) {
                status |= strbuf_putstr(buf, attribute);
            }
        }
    }

    return status;
}

static int jlk_parse_response(jlk_t *jlk, c_avl_tree_t *mbeans, xson_value_t *tree)
{
    if (!xson_value_is_object(tree)) {
        return -1;
    }

    xson_value_t *request = NULL;
    xson_value_t *value = NULL;
    double rstatus = 0;
    double timestamp = 0;

    for (size_t i = 0; i < xson_value_object_size(tree); i++) {
        xson_keyval_t *kv = xson_value_object_at(tree, i);
        if (kv == NULL)
            break;

        char *key = xson_keyval_key(kv);

        if (strcmp(key, "request") == 0) {
            request = xson_keyval_value(kv);
        } else if (strcmp(key, "value") == 0) {
            value = xson_keyval_value(kv);
        } else if (strcmp(key, "status") == 0) {
            rstatus = xson_value_get_number(xson_keyval_value(kv));
        } else if (strcmp(key, "timestamp") == 0) {
            timestamp = xson_value_get_number(xson_keyval_value(kv));
        }
    }

    if (request == NULL)
        return 0;

    if (value == NULL)
        return 0;

    if (rstatus != 200)
        return 0;

    char buff[MBEAN_MAX_KEY_SIZE];
    strbuf_t buf = STRBUF_CREATE_STATIC(buff);

    int status = jlk_parse_response_request_to_key(request, &buf);
    if (status != 0)
        return 0;

    jlk_mbean_t *found_mbean = NULL;
    status = c_avl_get(mbeans, buf.ptr, (void *)&found_mbean);
    if (status != 0)
        return 0;

    return jlk_parse_response_value(jlk, found_mbean, value, timestamp);
}

static size_t jlk_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    size_t len = size * nmemb;
    if (len <= 0)
        return len;

    xson_tree_parser_t *parser = user_data;
    if (parser == NULL)
        return 0;

    int status = xson_tree_parser_chunk(parser, buf, len);
    if (status != 0) {
        PLUGIN_ERROR("Failed to parse response");
        return 0;
    }

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

static int jlk_curl_perform(jlk_t *jlk, char *post_body, void *data)
{
    CURLcode rcode = curl_easy_setopt(jlk->curl, CURLOPT_WRITEDATA, data);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEDATA failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(jlk->curl, CURLOPT_POSTFIELDS, post_body);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_POSTFIELDS failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    jlk->curl_errbuf[0] = '\0';
    int status = curl_easy_perform(jlk->curl);
    if (status != CURLE_OK) {
        char *url = NULL;
        curl_easy_getinfo(jlk->curl, CURLINFO_EFFECTIVE_URL, &url);
        PLUGIN_ERROR("curl_easy_perform failed with status %d: %s (%s)",
                     status, jlk->curl_errbuf, url == NULL ? jlk->url : url);
        return -1;
    }

    long rc;
    curl_easy_getinfo(jlk->curl, CURLINFO_RESPONSE_CODE, &rc);

    /* The response code is zero if a non-HTTP transport was used. */
    if ((rc != 0) && (rc != 200)) {
        char *url = NULL;
        curl_easy_getinfo(jlk->curl, CURLINFO_EFFECTIVE_URL, &url);
        PLUGIN_ERROR("curl_easy_perform failed with response code %ld (%s)",
                     rc, url == NULL ? jlk->url : url);
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

    for (size_t i = 0; i < jlk->mbeans_size; i++) {
        jlk_mbean_set_t *mbeans_set = &g_mbean_set[jlk->mbeans[i]];

        char error_buffer[256];
        error_buffer[0] = '\0';
        xson_tree_parser_t *parser = xson_tree_parser_init(error_buffer, sizeof(error_buffer));
        if (parser == NULL)
            return -1;

        int status = jlk_curl_perform(jlk, jlk->post_body[i], parser);
        if (status < 0) {
            if (error_buffer[0] != '\0')
                PLUGIN_ERROR("json_parse failed: %s", error_buffer);
            xson_tree_parser_free(parser);
            return -1;
        }

        xson_value_t *tree = xson_tree_parser_complete(parser);
        if (tree == NULL) {
            PLUGIN_ERROR("json_parse_complete failed: %s", error_buffer);
            xson_tree_parser_free(parser);
            return -1;
        }

        xson_tree_parser_free(parser);

        if (xson_value_is_array(tree)) {
            for (size_t j = 0; j < xson_value_array_size(tree); j++) {
                jlk_parse_response(jlk, mbeans_set->mbeans, xson_value_array_at(tree, j));
            }
        } else {
            jlk_parse_response(jlk, mbeans_set->mbeans, tree);
        }

        xson_value_free(tree);
    }

    return 0;
}

static int jlk_build_post(c_avl_tree_t *mbeans, strbuf_t *buf)
{
    c_avl_iterator_t *iter = c_avl_get_iterator(mbeans);
    if (iter == NULL)
        return -1;

    xson_render_t r = {0};
    xson_render_init(&r, buf, XSON_RENDER_TYPE_JSON, 0);

    int status = xson_render_array_open(&r);

    jlk_mbean_t *mbean = NULL;
    while (c_avl_iterator_next(iter, NULL, (void *)&mbean) == 0) {
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

    c_avl_iterator_destroy(iter);

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
    free(attribute->help);
    label_set_reset(&attribute->labels);
    label_set_reset(&attribute->labels_from);
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
    label_set_reset(&mbean->labels_from);
    jlk_mbean_attribute_set_free(&mbean->attributes);

    free(mbean);
}

static void jlk_free(void *arg)
{
    jlk_t *jlk = (jlk_t *)arg;
    if (jlk == NULL)
        return;

    if (jlk->curl != NULL)
        curl_easy_cleanup(jlk->curl);
    jlk->curl = NULL;

    free(jlk->instance);

    free(jlk->url);
    free(jlk->user);
    free(jlk->pass);
    free(jlk->credentials);
    free(jlk->cacert);

    curl_slist_free_all(jlk->headers);

    if (jlk->post_body != NULL) {
        for (size_t i = 0; i < jlk->mbeans_size; i++) {
           free(jlk->post_body[i]);
        }
        free(jlk->post_body);
    }
    free(jlk->mbeans);

    free(jlk->metric_prefix);
    label_set_reset(&jlk->labels);
    plugin_filter_free(jlk->filter);

    free(jlk);
}

static int jlk_config_add_attribute(config_item_t *ci, jlk_mbean_attribute_set_t *attributes)
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

static int jlk_config_add_mbean(config_item_t *ci, c_avl_tree_t *mbeans)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The 'mbean' block needs exactly one string argument.");
        return -1;
    }

    jlk_mbean_t *mbean = calloc(1, sizeof(*mbean));
    if (mbean == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &(mbean->mbean));
    if (status != 0) {
        jlk_mbean_free(mbean);
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
            status = jlk_config_add_attribute(child, &mbean->attributes);
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
        PLUGIN_ERROR("Failed to parse mbean: '%s'.", mbean->mbean);
        return -1;
    }

    char *mbean_key = jlk_mbean_key(mbean);
    if (mbean_key == NULL) {
        PLUGIN_ERROR("Invalid mbean name: '%s'.", mbean->mbean);
        return -1;
    }

    jlk_mbean_t *found_mbean = NULL;
    status = c_avl_get(mbeans, mbean_key, (void *)&found_mbean);
    if (status == 0) {
        PLUGIN_ERROR("mbean '%s' at %s:%d is already defined.",
                      mbean->mbean, cf_get_file(ci), cf_get_lineno(ci));
        jlk_mbean_free(mbean);
        free(mbean_key);
        return -1;
    }

    status = c_avl_insert(mbeans, mbean_key, mbean);
    if (status != 0) {
        PLUGIN_ERROR("Failed to insert mbean '%s' at  %s:%d in avl tree.",
                     mbean->mbean, cf_get_file(ci), cf_get_lineno(ci));
        jlk_mbean_free(mbean);
    }

    return 0;
}

static int jlk_config_add_mbean_set(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The 'mbeans' block needs exactly one string argument.");
        return -1;
    }

    jlk_mbean_set_t *tmp = realloc(g_mbean_set, sizeof(jlk_mbean_set_t) * (g_mbean_set_num + 1));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        return -1;
    }

    g_mbean_set = tmp;
    jlk_mbean_set_t *mbean_set = &g_mbean_set[g_mbean_set_num];
    mbean_set->name = NULL;
    mbean_set->mbeans = NULL;

    int status = cf_util_get_string(ci, &(mbean_set->name));
    if (status != 0)
        return -1;

    mbean_set->mbeans = c_avl_create((int (*)(const void *, const void *))strcmp);
    if (mbean_set->mbeans == NULL) {
        free(mbean_set->name);
        return -1;
    }

    g_mbean_set_num++;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp("mbean", child->key) == 0) {
            status = jlk_config_add_mbean(child, mbean_set->mbeans);
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

static int jlk_config_pick_mbean(config_item_t *ci, jlk_t *jlk)
{
    if (ci->values_num == 0) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires one or more arguments.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    for (int i = 0; i < ci->values_num; i++) {
        if (ci->values[i].type != CONFIG_TYPE_STRING) {
            PLUGIN_ERROR("The %d argument of '%s' option in %s:%d must be a string.",
                         i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }

        const char *name = ci->values[i].value.string;

        bool added = false;
        for (size_t n = 0; n < g_mbean_set_num; n++) {
            if (strcmp(name, g_mbean_set[n].name) != 0)
                continue;

            size_t *tmp = realloc(jlk->mbeans, sizeof(size_t) * (jlk->mbeans_size + 1));
            if (tmp == NULL) {
                PLUGIN_ERROR("realloc failed.");
                return -1;
            }

            jlk->mbeans = tmp;
            jlk->mbeans[jlk->mbeans_size] = n;
            jlk->mbeans_size++;

            added = true;
            break;
        }

        if (added == false) {
            PLUGIN_ERROR("Cannot find mbeans group '%s'. Make sure the 'mbeans' "
                         "block is above the instance definition!", name);
            return -ENOENT;
        }
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
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &jlk->user);
        } else if (strcasecmp("user-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &jlk->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &jlk->pass);
        } else if (strcasecmp("password-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &jlk->pass);
        } else if (strcasecmp("verify-peer", child->key) == 0) {
            status = cf_util_get_boolean(child, &jlk->verify_peer);
        } else if (strcasecmp("verify-host", child->key) == 0) {
            status = cf_util_get_boolean(child, &jlk->verify_host);
        } else if (strcasecmp("ca-cert", child->key) == 0) {
            status = cf_util_get_string(child, &jlk->cacert);
        } else if (strcasecmp("header", child->key) == 0) {
            status = jlk_config_append_string("Header", &jlk->headers, child);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &jlk->timeout);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &jlk->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &jlk->metric_prefix);
        } else if (strcasecmp("collect", child->key) == 0) {
            status = jlk_config_pick_mbean(child, jlk);
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

    if (jlk->url == NULL) {
        PLUGIN_ERROR("Missing url.");
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

    if (jlk->mbeans_size == 0) {
        PLUGIN_ERROR("No mbeans configured for collection.");
        jlk_free(jlk);
        return -1;
    }

    jlk->post_body = calloc(jlk->mbeans_size, sizeof(char *));
    if (jlk->post_body == NULL) {
        PLUGIN_ERROR("calloc failed.");
        jlk_free(jlk);
        return -1;
    }

    strbuf_t buf = STRBUF_CREATE;
    for (size_t i = 0; i < jlk->mbeans_size; i++) {
        strbuf_reset(&buf);
        jlk_mbean_set_t *mbean_set = &g_mbean_set[jlk->mbeans[i]];
        status = jlk_build_post(mbean_set->mbeans, &buf);
        if (status != 0) {
            PLUGIN_ERROR("Failed to build POST data");
            jlk_free(jlk);
            return -1;
        }
        char *post = strdup(buf.ptr);
        if (post == NULL) {
            PLUGIN_ERROR("strdup failed.");
            jlk_free(jlk);
            return -1;
        }
        jlk->post_body[i] = post;
    }
    strbuf_destroy(&buf);

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
        } else if (strcasecmp("mbeans", child->key) == 0) {
            status = jlk_config_add_mbean_set(child);
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

static int jlk_shutdown(void)
{
    for (size_t i = 0; i < g_mbean_set_num; i++) {
        jlk_mbean_set_t *mbean_set = &g_mbean_set[i];
        while (true) {
            char *key = NULL;
            jlk_mbean_t *mbean = NULL;
            int status = c_avl_pick(mbean_set->mbeans, (void *)&key, (void *)&mbean);
            if (status != 0)
                break;
            free(key);
            jlk_mbean_free(mbean);
        }
        c_avl_destroy(mbean_set->mbeans);
        free(mbean_set->name);
    }
    free(g_mbean_set);
    g_mbean_set = NULL;
    g_mbean_set_num = 0;

    return 0;
}

void module_register(void)
{
    plugin_register_config("jolokia", jlk_config);
    plugin_register_shutdown("jolokia", jlk_shutdown);
}
