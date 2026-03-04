// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2018 Intel Corporation. All rights reserved.
// SPDX-FileCopyrightText: Copyright (C) 2021 Atos. All rights reserved.
// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Marcin Mozejko <marcinx.mozejko at intel.com>
// SPDX-FileContributor: Martin Kennelly <martin.kennelly at intel.com>
// SPDX-FileContributor: Adrian Boczkowski <adrianx.boczkowski at intel.com>
// SPDX-FileContributor: Mathieu Stoffel <mathieu.stoffel at atos.net>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/avltree.h"
#include "libutils/llist.h"
#include "libutils/strlist.h"
#include "libutils/dtoa.h"

#include "libxson/render.h"
#include "libxson/json_parse.h"
#include "libxson/tree.h"

#include <curl/curl.h>

typedef struct {
    char *name;
    char *metric;
    metric_type_t type;
    char *help;
    label_set_t labels;
    label_set_t labels_from;
    double scale;
    double shift;
} redfish_attribute_t;

typedef struct {
    char *name;
    char *metric;
    metric_type_t type;
    char *help;
    label_set_t labels;
    label_set_t labels_from;
    double scale;
    double shift;
    uint64_t *select_ids;
    uint64_t select_ids_num;
    strlist_t select_attr;
    label_set_t select_attr_values;
} redfish_property_t;

typedef struct {
    char *name;
    label_set_t labels;
    char *metric_prefix;
    llist_t *properties;
} redfish_resource_t;

typedef struct {
    char *name;
    char *endpoint;
    label_set_t labels;
    char *metric_prefix;
    llist_t *resources;
    llist_t *attributes;
} redfish_query_t;

struct redfish_instance_s;
typedef struct redfish_instance_s redfish_instance_t;

typedef struct {
    CURL *curl;
    struct curl_slist *headers;
    char curl_error[CURL_ERROR_SIZE];
    xson_tree_parser_t *parser;
    char parser_error[CURL_ERROR_SIZE];
    redfish_instance_t *instance;
    redfish_query_t *query;
} redfish_request_t;

struct redfish_instance_s {
    char *name;
    char *url;
    char *user;
    char *pass;
    char *token;
    char *version;
    bool verify_peer;
    bool verify_host;
    char *cacert;
    char *proxy;
    bool proxy_tunnel;
    label_set_t labels;
    char *metric_prefix;
    cdtime_t timeout;
    plugin_filter_t *filter;
    CURLM *curl_multi;
    redfish_request_t *requests;
    size_t requests_num;
};

#define REDFISH_VERSION "v1"

static c_avl_tree_t *queries;

void redfish_request_reset(redfish_request_t *request)
{
    request->curl_error[0] = '\0';
    if (request->curl != NULL) {
        curl_easy_cleanup(request->curl);
        request->curl = NULL;
    }

    if (request->headers != NULL) {
        curl_slist_free_all(request->headers);
        request->headers = NULL;
    }

    request->parser_error[0] = '\0';
    if (request->parser != NULL) {
        xson_tree_parser_free(request->parser);
        request->parser = NULL;
    }
}

static void redfish_instance_free(void *data)
{
    redfish_instance_t *instance = data;
    if (instance == NULL)
        return;

    free(instance->name);
    free(instance->url);
    free(instance->user);
    free(instance->pass);
    free(instance->token);
    free(instance->version);

    free(instance->proxy);

    free(instance->cacert);
    label_set_reset(&instance->labels);
    free(instance->metric_prefix);
    plugin_filter_free(instance->filter);

    for (size_t i = 0; i < instance->requests_num; i++) {
        redfish_request_reset(&instance->requests[i]);
    }
    free(instance->requests);

    free(instance);
}

static void redfish_property_free(redfish_property_t *property)
{
    if (property == NULL)
        return;

    free(property->name);
    free(property->metric);
    label_set_reset(&property->labels);
    label_set_reset(&property->labels_from);
    strlist_destroy(&property->select_attr);
    label_set_reset(&property->select_attr_values);
    free(property->select_ids);

    free(property);
}

static void redfish_resource_free(redfish_resource_t *resource)
{
    if (resource == NULL)
        return;

    for (llentry_t *le = llist_head(resource->properties); le != NULL; le = le->next) {
        redfish_property_free((redfish_property_t *)le->value);
    }
    llist_destroy(resource->properties);

    label_set_reset(&resource->labels);
    free(resource->metric_prefix);
    free(resource->name);
    free(resource);
}

static void redfish_attribute_free(redfish_attribute_t *attr)
{
    if (attr == NULL)
        return;

    free(attr->name);
    free(attr->metric);
    free(attr->help);
    label_set_reset(&attr->labels);
    label_set_reset(&attr->labels_from);

    free(attr);
}

static void redfish_query_free(redfish_query_t *query)
{
    if (query == NULL)
        return;

    for (llentry_t *le = llist_head(query->resources); le != NULL; le = le->next) {
        redfish_resource_free((redfish_resource_t *)le->value);
    }
    llist_destroy(query->resources);

    for (llentry_t *le = llist_head(query->attributes); le != NULL; le = le->next) {
        redfish_attribute_free((redfish_attribute_t *)(le->value));
    }
    llist_destroy(query->attributes);

    free(query->name);
    free(query->endpoint);
    free(query->metric_prefix);
    label_set_reset(&query->labels);
    free(query);
}

static bool redfish_get_status(xson_value_t *xvalue)
{
    if (!xson_value_is_object(xvalue))
        return false;

    xson_value_t *state = xson_value_object_find(xvalue, "State");
    if (state == NULL)
        return false;

    if (!xson_value_is_string(state))
        return false;

    if (strcmp(xson_value_get_string(state), "Enabled") != 0)
        return false;

    xson_value_t *health = xson_value_object_find(xvalue, "Health");
    if (health == NULL)
        return false;

    if (!xson_value_is_string(health))
        return false;

    if (strcmp(xson_value_get_string(health), "OK") == 0)
        return true;

    return false;
}

static int redfish_get_value(xson_value_t *xvalue, metric_type_t type, value_t *value,
                         double scale, double shift)
{
    if (type == METRIC_TYPE_GAUGE) {
        double number = 0;
        if (xson_value_is_string(xvalue)) {
            if (parse_double(xson_value_get_string(xvalue), &number) != 0)
                return -1;
        } else if (xson_value_is_number(xvalue)) {
            number = xson_value_get_number(xvalue);
        }
        if (scale != 0)
            number *= scale;
        if (shift != 0)
            number += shift;
        *value = VALUE_GAUGE(number);
    } else if (type == METRIC_TYPE_COUNTER) {
        if (xson_value_is_string(xvalue)) {
            uint64_t number = 0;
            if (parse_uinteger(xson_value_get_string(xvalue), &number) != 0)
                return -1;
            *value = VALUE_COUNTER(number);
        } else if (xson_value_is_number(xvalue)) {
            *value = VALUE_COUNTER((uint64_t)xson_value_get_number(xvalue));
        }
    }

    return 0;
}

static void redfish_process_attribute(const redfish_instance_t *instance,
                                      const redfish_query_t *query,
                                      const redfish_attribute_t *attribute,
                                      xson_value_t *obj, cdtime_t rtime)
{
    xson_value_t *vattr = xson_value_object_find(obj, attribute->name);
    if (vattr == NULL) {
        PLUGIN_ERROR("Could not find the attribute '%s' in the payload associated "
                     "with the query '%s'", attribute->name, query->name);
        return;
    }

    value_t value = {0};

    if (strcmp(attribute->name, "Status") == 0) {
        value = VALUE_GAUGE(redfish_get_status(vattr) ? 1.0 : 0.0);
    } else {
        int status = redfish_get_value(vattr, attribute->type, &value,
                                       attribute->scale, attribute->shift);
        if (status != 0) {
            PLUGIN_WARNING("Cannot parse value for attribute '%s'.", attribute->name);
            return;
        }
    }

    strbuf_t buf = STRBUF_CREATE;

    if (instance->metric_prefix != NULL)
        strbuf_putstr(&buf, instance->metric_prefix);
    if (query->metric_prefix != NULL)
        strbuf_putstr(&buf, query->metric_prefix);

    strbuf_putstr(&buf, attribute->metric);

    label_set_t labels = {0};

    if (instance->labels.num > 0)
        label_set_add_set(&labels, true, instance->labels);
    if (query->labels.num > 0)
        label_set_add_set(&labels, true, query->labels);
    if (attribute->labels.num > 0)
        label_set_add_set(&labels, true, attribute->labels);

    for (size_t i = 0; i < attribute->labels_from.num; i++) {
        label_pair_t *pair = &attribute->labels_from.ptr[i];

        xson_value_t *lvalue = xson_value_object_find(obj, pair->value);
        if (lvalue != NULL) {
            if (xson_value_is_string(lvalue)) {
                label_set_add(&labels, true, pair->name, xson_value_get_string(lvalue));
            } else if (xson_value_is_number(lvalue)) {
                char number[DTOA_MAX];
                dtoa(xson_value_get_number(lvalue), number, sizeof(number));
                label_set_add(&labels, true, pair->name, number);
            } else if (xson_value_is_true(lvalue)) {
                label_set_add(&labels, true, pair->name, "true");
            } else if (xson_value_is_false(lvalue)) {
                label_set_add(&labels, true, pair->name, "false");
            }
        }
    }

    metric_family_t fam = {0};
    fam.name = buf.ptr;
    fam.type = attribute->type;
    fam.help = attribute->help;

    metric_t m = {0};
    m.label = labels;
    m.value = value;

    metric_family_metric_append(&fam, m);
    plugin_dispatch_metric_family_filtered(&fam, instance->filter, rtime);

    metric_reset(&m, fam.type);
    metric_family_metric_reset(&fam);
    strbuf_destroy(&buf);
}

static void redfish_process_resource_object(const redfish_instance_t *instance,
                                            const redfish_query_t *query,
                                            const redfish_resource_t *resource,
                                            const redfish_property_t *property,
                                            xson_value_t *obj, cdtime_t rtime)
{
    xson_value_t *vprop = xson_value_object_find(obj, property->name);
    if (vprop == NULL) {
        PLUGIN_ERROR("Failure retreiving property '%s' from resource '%s'",
                     property->name, resource->name);
        return;
    }

    value_t value = {0};

    if (strcmp(property->name, "Status") == 0) {
        value = VALUE_GAUGE(redfish_get_status(vprop) ? 1.0 : 0.0);
    } else {
        int status = redfish_get_value(vprop, property->type, &value,
                                       property->scale, property->shift);
        if (status != 0) {
            PLUGIN_WARNING("Cannot parse value for property '%s'.", property->name);
            return;
        }
    }

    strbuf_t buf = STRBUF_CREATE;

    if (instance->metric_prefix != NULL)
        strbuf_putstr(&buf, instance->metric_prefix);
    if (query->metric_prefix != NULL)
        strbuf_putstr(&buf, query->metric_prefix);
    if (resource->metric_prefix != NULL)
        strbuf_putstr(&buf, resource->metric_prefix);

    strbuf_putstr(&buf, property->metric);

    label_set_t labels = {0};

    if (instance->labels.num > 0)
        label_set_add_set(&labels, true, instance->labels);
    if (query->labels.num > 0)
        label_set_add_set(&labels, true, query->labels);
    if (resource->labels.num > 0)
        label_set_add_set(&labels, true, resource->labels);
    if (property->labels.num > 0)
        label_set_add_set(&labels, true, property->labels);

    for (size_t i = 0; i < property->labels_from.num; i++) {
        label_pair_t *pair = &property->labels_from.ptr[i];

        xson_value_t *lvalue = xson_value_object_find(obj, pair->value);
        if (lvalue != NULL) {
            if (xson_value_is_string(lvalue)) {
                label_set_add(&labels, true, pair->name, xson_value_get_string(lvalue));
            } else if (xson_value_is_number(lvalue)) {
                char number[DTOA_MAX];
                dtoa(xson_value_get_number(lvalue), number, sizeof(number));
                label_set_add(&labels, true, pair->name, number);
            } else if (xson_value_is_true(lvalue)) {
                label_set_add(&labels, true, pair->name, "true");
            } else if (xson_value_is_false(lvalue)) {
                label_set_add(&labels, true, pair->name, "false");
            }
        }
    }

    metric_family_t fam = {0};
    fam.name = buf.ptr;
    fam.type = property->type;
    fam.help = property->help;

    metric_t m = {0};
    m.label = labels;
    m.value = value;

    metric_family_metric_append(&fam, m);
    plugin_dispatch_metric_family_filtered(&fam, instance->filter, rtime);

    metric_reset(&m, fam.type);
    metric_family_metric_reset(&fam);
    strbuf_destroy(&buf);
}

static void redfish_process_resource_property(const redfish_instance_t *instance,
                                              const redfish_query_t *query,
                                              const redfish_resource_t *resource,
                                              const redfish_property_t *property,
                                              xson_value_t *vresource, cdtime_t rtime)
{
    if (xson_value_is_object(vresource)) {
        redfish_process_resource_object(instance, query, resource, property, vresource, rtime);
    } else if (xson_value_is_array(vresource)) {
        for (size_t i = 0; i  < xson_value_array_size(vresource); i++) {

            if ((property->select_ids != NULL) && (property->select_ids_num > 0)) {
                /* Roaming all the specified IDs to determine whether or not the
                 * currently considered one is among them: */
                bool id_selected = false;
                /***/
                for (uint64_t j = 0; j < property->select_ids_num; j++) {
                    if (i == (property->select_ids)[j]) {
                        id_selected = true;
                        break;
                    }
                }

                if (!id_selected)
                    continue;
            }

            xson_value_t *obj = xson_value_array_at(vresource, i);
            if (obj == NULL) {
                PLUGIN_ERROR("Fail to retrieving array member for resource '%s'", resource->name);
                continue;
            }

            if (property->select_attr.size > 0) {
                bool match = true;

                for (size_t j = 0; j < property->select_attr.size ; j++) {
                    char *name = property->select_attr.ptr[j];
                    xson_value_t *vobj = xson_value_object_find(obj, name);
                    if (vobj == NULL) {
                        match = false;
                        break;
                    }
                }

                if (!match)
                    continue;
            }

            if (property->select_attr_values.num > 0) {
                bool match = true;

                for (size_t j = 0; j < property->select_attr_values.num; j++) {
                    label_pair_t *pair = &property->select_attr_values.ptr[j];

                    xson_value_t *vobj = xson_value_object_find(obj, pair->name);
                    if (vobj == NULL) {
                        match = false;
                        break;
                    }

                    if (xson_value_is_string(vobj)) {
                        if (strcmp(pair->value, xson_value_get_string(vobj)) != 0) {
                            match = false;
                            break;
                        }
                    } else if (xson_value_is_number(vobj)) {
                        double vnum = xson_value_get_number(vobj);
                        char num[DTOA_MAX];
                        dtoa(vnum, num, sizeof(num));
                        if (strcmp(pair->value, num) != 0) {
                            match = false;
                            break;
                        }
                    } else {
                        match = false;
                        break;
                    }
                }

                if (!match)
                    continue;
            }

            redfish_process_resource_object(instance, query, resource, property, obj, rtime);
        }
    }
}

static size_t redfish_request_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    size_t len = size * nmemb;
    if (len <= 0)
        return len;

    redfish_request_t *request = user_data;
    if (request == NULL)
        return 0;
    if (request->parser == NULL)
        return 0;

    int status = xson_tree_parser_chunk(request->parser, buf, len);
    if (status != 0) {
        PLUGIN_ERROR("Failed to parse response");
        return 0;
    }

    return len;
}

static int redfish_add_header(struct curl_slist **list, const char *key, const char *value)
{
    char header[1024];
    ssnprintf(header, sizeof(header), "%s: %s", key, value);

    struct curl_slist *temp = curl_slist_append(*list, header);
    if (temp == NULL)
        return -1;

    *list = temp;

    return 0;
}

int redfish_request_init(redfish_request_t *request)
{
    request->curl = curl_easy_init();
    if (request->curl == NULL) {
        PLUGIN_ERROR("curl_easy_init failed.");
        return -1;
    }

    CURLcode rcode = 0;

    rcode = curl_easy_setopt(request->curl, CURLOPT_PRIVATE, request);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_PRIVATE failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(request->curl, CURLOPT_NOSIGNAL, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_NOSIGNAL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(request->curl, CURLOPT_WRITEFUNCTION, redfish_request_callback);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEFUNCTION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(request->curl, CURLOPT_WRITEDATA, request);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEDATA failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(request->curl, CURLOPT_USERAGENT, NCOLLECTD_USERAGENT);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERAGENT failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(request->curl, CURLOPT_ERRORBUFFER, request->curl_error);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(request->curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_FOLLOWLOCATION failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(request->curl, CURLOPT_MAXREDIRS, 50L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_MAXREDIRS failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(request->curl, CURLOPT_TIMEOUT_MS,
                             (long)CDTIME_T_TO_MS(request->instance->timeout));
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    redfish_add_header(&request->headers, "Accept", "application/json");
    redfish_add_header(&request->headers, "OData-Version", "4.0");
    if(request->instance->token) {
        char tmp[1024];
        ssnprintf(tmp, sizeof(tmp), "Bearer %s", request->instance->token);
        redfish_add_header(&request->headers, "Authorization", tmp);
    }

    rcode = curl_easy_setopt(request->curl, CURLOPT_HTTPHEADER, request->headers);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_HTTPHEADER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }


    if (request->instance->user != NULL) {
        rcode = curl_easy_setopt(request->curl, CURLOPT_USERNAME, request->instance->user);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERNAME failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(request->curl, CURLOPT_PASSWORD,
                                 (request->instance->pass == NULL) ? "" : request->instance->pass);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_PASSWORD failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

    rcode = curl_easy_setopt(request->curl, CURLOPT_SSL_VERIFYPEER,
                             (long)request->instance->verify_peer);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_VERIFYPEER failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    rcode = curl_easy_setopt(request->curl, CURLOPT_SSL_VERIFYHOST,
                             request->instance->verify_host ? 2L : 0L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_SSL_VERIFYHOST failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    if (request->instance->cacert != NULL) {
        rcode = curl_easy_setopt(request->curl, CURLOPT_CAINFO, request->instance->cacert);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_CAINFO failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

    if (request->instance->proxy != NULL) {
        rcode = curl_easy_setopt(request->curl, CURLOPT_PROXY, request->instance->proxy);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_PROXY failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(request->curl, CURLOPT_HTTPPROXYTUNNEL,
                                 request->instance->proxy_tunnel ? 1L : 0L);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_HTTPPROXYTUNNEL failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

    char url[1024];
    size_t len = strlen(request->instance->url);
    ssnprintf(url, sizeof(url), "%s%sredfish/%s%s", request->instance->url,
                                request->instance->url[len-1] == '/' ? "" : "/",
                                request->instance->version,
                                request->query->endpoint);

    rcode = curl_easy_setopt(request->curl, CURLOPT_URL, url);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_URL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }

    if (request->parser != NULL)
        xson_tree_parser_free(request->parser);

    request->parser = xson_tree_parser_init(request->parser_error, sizeof(request->parser_error));
    if (request->parser == NULL) {
        PLUGIN_ERROR("xson_tree_parser_init failed.");
        return -1;
    }

    return 0;
}

static int redfish_request_end(redfish_request_t *request)
{
    if (request == NULL)
        return -1;
    
    long code = 0;
    CURLcode rcode = curl_easy_getinfo(request->curl, CURLINFO_RESPONSE_CODE, &code);  
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_getinfo CURLINFO_RESPONSE_CODE failed: %s",
                     curl_easy_strerror(rcode));
        xson_tree_parser_free(request->parser);
        request->parser = NULL;
        return -1;
    }

    if ((code < 200) || (code > 299)) {
        xson_tree_parser_free(request->parser);
        request->parser = NULL;
        return -1;
    }

    xson_value_t *tree = xson_tree_parser_complete(request->parser);
    if (tree == NULL) {
        PLUGIN_ERROR("xson_tree_parser_complete failed: %s", request->parser_error);
        xson_tree_parser_free(request->parser);
        request->parser = NULL;
        return -1;
    }

    xson_tree_parser_free(request->parser);
    request->parser = NULL;
    request->parser_error[0] = '\0';

    cdtime_t now = cdtime();

    for (llentry_t *llres = llist_head(request->query->resources); llres != NULL; llres = llres->next) {
        redfish_resource_t *res = (redfish_resource_t *)llres->value;

        xson_value_t *vresource = xson_value_object_find(tree, res->name);
        if (vresource == NULL) {
            PLUGIN_WARNING("Could not find resource '%s'", res->name);
            continue;
        }

        for (llentry_t *llprop = llist_head(res->properties); llprop != NULL; llprop = llprop->next) {
            redfish_property_t *prop = (redfish_property_t *)llprop->value;
            redfish_process_resource_property(request->instance, request->query, res, prop,
                                              vresource, now);
        }
    }

    for (llentry_t *llattr = llist_head(request->query->attributes); llattr != NULL; llattr = llattr->next) {
        redfish_attribute_t *attr = (redfish_attribute_t *)(llattr->value);
        redfish_process_attribute(request->instance, request->query, attr, tree, now);
    }

    xson_value_free(tree);

    return 0;
}

static int redfish_read(user_data_t *user_data)
{
    redfish_instance_t *instance = user_data->data;
    if (instance == NULL) {
        PLUGIN_ERROR("user_data pointer is NULL.");
        return 0;
    }

    if (instance->curl_multi == NULL) {
        instance->curl_multi = curl_multi_init();
        if (instance->curl_multi == NULL) {
            PLUGIN_ERROR("curl_multi_init failed.");
            return -1;
        }
        curl_multi_setopt(instance->curl_multi, CURLMOPT_MAX_HOST_CONNECTIONS, 4L);
    }

    for (size_t i = 0; i < instance->requests_num; i++) {
        redfish_request_t *request = &instance->requests[i];
        if (request->curl == NULL) {
            int status = redfish_request_init(request);
            if (status != 0) {
                redfish_request_reset(request);
                continue;
            }
        }

        curl_multi_add_handle(instance->curl_multi, request->curl);
    }

    int still_running = 0;

    do {
        int numfds;

        CURLMcode mc = curl_multi_perform(instance->curl_multi, &still_running);
        if (mc != CURLM_OK)
            break;

        struct CURLMsg *m;
        do {
            int msgq = 0;
            m = curl_multi_info_read(instance->curl_multi, &msgq);
            if(m && (m->msg == CURLMSG_DONE)) {
                CURL *e = m->easy_handle;
                redfish_request_t *request = NULL;
                curl_easy_getinfo(e, CURLINFO_PRIVATE, &request);
                redfish_request_end(request);
                curl_multi_remove_handle(instance->curl_multi, e);
                redfish_request_reset(request);
            }
        } while(m);

        mc = curl_multi_poll(instance->curl_multi, NULL, 0, 100, &numfds);
        if (mc != CURLM_OK) {

        }

    } while (still_running);

    for (size_t i = 0; i < instance->requests_num; i++) {
        if (instance->requests[i].curl != NULL) {
            curl_multi_remove_handle(instance->curl_multi, instance->requests[i].curl);
            redfish_request_reset(&instance->requests[i]);
        }
    }

    curl_multi_cleanup(instance->curl_multi);
    instance->curl_multi = NULL;

    return 0;
}

static int redfish_config_get_uint64_array (const config_item_t *ci,
                                            size_t *ret_size, uint64_t **ret_values)
{
    if ((ci == NULL) || (ret_size == NULL) || (ret_values == NULL))
        return EINVAL;

    if (ci->values_num <= 0) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires a list of numbers.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    for (int i=0 ; i < ci->values_num; i++) {
        if (ci->values[i].type != CONFIG_TYPE_NUMBER) {
            PLUGIN_ERROR("The argument %d in option '%s' at %s:%d must be a number.",
                         i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }
    }

    uint64_t *values = malloc(sizeof(uint64_t) * ci->values_num);
    if (values == NULL) {
        PLUGIN_ERROR("malloc failed: %s.", STRERRNO);
        return -1;
    }

    for (int i=0 ; i < ci->values_num; i++) {
        values[i] = (uint64_t)ci->values[i].value.number;
    }

    *ret_values = values;
    *ret_size = ci->values_num;

    return 0;
}

static int redfish_config_strlist(const config_item_t *ci, strlist_t *sl)
{
    if ((ci == NULL) || (sl == NULL))
        return EINVAL;

    if (ci->values_num <= 0) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires a list of string.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    for (int i=0 ; i < ci->values_num; i++) {
        if (ci->values[i].type != CONFIG_TYPE_STRING) {
            PLUGIN_ERROR("The argument %d in option '%s' at %s:%d must be a string.",
                         i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }
    }

    for (int i=0 ; i < ci->values_num; i++) {
        strlist_append(sl, ci->values[i].value.string);
    }

    return 0;
}

static int redfish_config_property(redfish_resource_t *resource, config_item_t *ci)
{
    redfish_property_t *property = calloc(1, sizeof(*property));
    if (property == NULL) {
        PLUGIN_ERROR("Failed to allocate memory for property");
        return -1;
    }

    property->type = METRIC_TYPE_GAUGE;
    property->scale = 0.0;
    property->shift = 0.0;

    int status = cf_util_get_string(ci, &property->name);
    if (status != 0) {
        PLUGIN_ERROR("Could not get property argument in resource '%s'", resource->name);
        free(property);
        return -1;
    }

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("type", child->key) == 0) {
            status = cf_util_get_metric_type(child, &property->type);
        } else if (strcasecmp("help", child->key) == 0) {
            status = cf_util_get_string(child, &property->help);
        } else if (strcasecmp("metric", child->key) == 0) {
            status = cf_util_get_string(child, &property->metric);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &property->labels);
        } else if (strcasecmp("label-from", child->key) == 0) {
            status = cf_util_get_label(child, &property->labels_from);
        } else if (strcasecmp("scale", child->key) == 0) {
            status = cf_util_get_double(child, &property->scale);
        } else if (strcasecmp("shift", child->key) == 0) {
            status = cf_util_get_double(child, &property->shift);
        } else if (strcasecmp("select-id", child->key) == 0) {
            status = redfish_config_get_uint64_array(child, &property->select_ids_num,
                                                            &property->select_ids);
        } else if (strcasecmp("select-attr", child->key) == 0) {
            status = redfish_config_strlist(child, &property->select_attr);
        } else if (strcasecmp("select-attr-value", child->key) == 0) {
            status = cf_util_get_label(child, &property->select_attr_values);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        redfish_property_free(property);
        return -1;
    }

    if (property->metric == NULL) {
        PLUGIN_ERROR("Missing 'metric' for property '%s' in resource '%s'.",
                     property->name, resource->name);
        redfish_property_free(property);
        return -1;
    }

    if ((strcmp(property->name, "Status") == 0) && (property->type != METRIC_TYPE_GAUGE)) {
        PLUGIN_ERROR("The type of metric '%s' for property 'Status' must be 'gauge'.",
                      property->metric);
        redfish_property_free(property);
        return -1;
    }

    llentry_t *entry = llentry_create(property->name, property);
    if (entry == NULL) {
        PLUGIN_ERROR("Failed to allocate memory for property");
        redfish_property_free(property);
        return -1;
    }

    llist_append(resource->properties, entry);

    return 0;
}

static int redfish_config_resource(redfish_query_t *query, config_item_t *ci)
{
    redfish_resource_t *resource = calloc(1, sizeof(*resource));
    if (resource == NULL) {
        PLUGIN_ERROR("Failed to allocate memory for resource");
        return -ENOMEM;
    }

    resource->properties = llist_create();
    if (resource->properties == NULL) {
        redfish_resource_free(resource);
        return -1;
    }

    int status = cf_util_get_string(ci, &resource->name);
    if (status != 0) {
        PLUGIN_ERROR("Could not get resource name for query '%s'", query->name);
        redfish_resource_free(resource);
        return -1;
    }

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("property", child->key) == 0) {
            status = redfish_config_property(resource, child);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &resource->metric_prefix);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &resource->labels);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        redfish_resource_free(resource);
        return -1;
    }

    llentry_t *entry = llentry_create(resource->name, resource);
    if (entry == NULL) {
        PLUGIN_ERROR("Failed to allocate memory for resource list entry");
        redfish_resource_free(resource);
        return -1;
    }

    llist_append(query->resources, entry);

    return 0;
}

static int redfish_config_attribute(redfish_query_t *query, config_item_t *ci)
{
    redfish_attribute_t *attr = calloc(1, sizeof(*attr));
    if (attr == NULL) {
        PLUGIN_ERROR("Failed to allocate memory for a query attribute");
        return -ENOMEM;
    }

    attr->type = METRIC_TYPE_GAUGE;

    int status = cf_util_get_string(ci, &(attr->name));
    if (status != 0) {
        PLUGIN_ERROR("Could not get the name of an attribute for query '%s'", query->name);
        free(attr);
    }

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("type", child->key) == 0) {
            status = cf_util_get_metric_type(child, &attr->type);
        } else if (strcasecmp("help", child->key) == 0) {
            status = cf_util_get_string(child, &attr->help);
        } else if (strcasecmp("metric", child->key) == 0) {
            status = cf_util_get_string(child, &attr->metric);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &attr->labels);
        } else if (strcasecmp("label-from", child->key) == 0) {
            status = cf_util_get_label(child, &attr->labels_from);
        } else if (strcasecmp("scale", child->key) == 0) {
            status = cf_util_get_double(child, &attr->scale);
        } else if (strcasecmp("shift", child->key) == 0) {
            status = cf_util_get_double(child, &attr->shift);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        redfish_attribute_free(attr);
        return -1;
    }

    if (attr->metric == NULL) {
        PLUGIN_ERROR("Missing 'metric' for attribute '%s' in query '%s'.",
                     attr->name, query->name);
        redfish_attribute_free(attr);
        return -1;
    }

    if ((strcmp(attr->name, "Status") == 0) && (attr->type != METRIC_TYPE_GAUGE)) {
        PLUGIN_ERROR("The type of metric '%s' for attribute 'Status' must be 'gauge'.",
                     attr->metric);
        redfish_attribute_free(attr);
        return -1;
    }

    llentry_t *entry = llentry_create(attr->name, attr);
    if (entry == NULL) {
        PLUGIN_ERROR("Failed to allocate memory for an attribute list entry");
        redfish_attribute_free(attr);
        return -1;
    }

    llist_append(query->attributes, entry);

    return 0;
}

static int redfish_config_query(config_item_t *ci)
{
    redfish_query_t *query = calloc(1, sizeof(*query));
    if (query == NULL) {
        PLUGIN_ERROR("Failed to allocate memory for query");
        return -ENOMEM;
    }

    query->resources = llist_create();
    if (query->resources == NULL) {
        redfish_query_free(query);
        return -1;
    }

    query->attributes = llist_create();
    if (query->attributes == NULL) {
        redfish_query_free(query);
        return -1;
    }

    int status = cf_util_get_string(ci, &query->name);
    if (status != 0) {
        PLUGIN_ERROR("Unable to get query name.");
        redfish_query_free(query);
        return -1;
    }

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("end-point", child->key) == 0) {
            status = cf_util_get_string(child, &query->endpoint);
        } else if (strcasecmp("resource", child->key) == 0) {
            status = redfish_config_resource(query, child);
        } else if (strcasecmp("attribute", child->key) == 0) {
            status = redfish_config_attribute(query, child);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &query->metric_prefix);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &query->labels);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        redfish_query_free(query);
        return -1;
    }

    if (query->endpoint == NULL) {
        PLUGIN_ERROR("Missing 'end-point' for query '%s'.", query->name);
        redfish_query_free(query);
        return -1;
    }

    if (query->endpoint[0] != '/') {
        PLUGIN_ERROR("'end-point' for querty '%s' is missing root ('/').", query->name);
        redfish_query_free(query);
        return -1;
    }

    if (queries == NULL) {
        queries = c_avl_create((int (*)(const void *, const void *))strcmp);
        if (queries == NULL) {
            redfish_query_free(query);
            return -1;
        }
    }

    status = c_avl_insert(queries, query->name, query);
    if (status != 0) {
        redfish_query_free(query);
        return -1;
    }

    return 0;
}

static int redfish_config_request(config_item_t *ci, redfish_instance_t *instance)
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

        char *query = ci->values[i].value.string;

        redfish_query_t *rquery;
        if (c_avl_get(queries, (void *)query, (void *)&rquery) != 0) {
            PLUGIN_ERROR("Cannot find query '%s' for instanace '%s'", query, instance->name);
            return -1;
        }

        redfish_request_t *tmp = realloc(instance->requests,
                                         sizeof(*tmp) * (instance->requests_num + 1));
        if (tmp == NULL) {
            PLUGIN_ERROR("realloc failed.");
            return -1;
        }

        instance->requests = tmp;
        memset(&instance->requests[instance->requests_num], 0, sizeof(redfish_request_t));
        instance->requests[instance->requests_num].query = rquery;
        instance->requests[instance->requests_num].instance = instance;
        instance->requests_num++;
    }

    return 0;
}

static int redfish_config_instance(config_item_t *ci)
{
    redfish_instance_t *instance = calloc(1, sizeof(*instance));
    if (instance == NULL) {
        PLUGIN_ERROR("Failed to allocate memory for instance");
        return -1;
    }

    int status = cf_util_get_string(ci, &instance->name);
    if (status != 0) {
        PLUGIN_ERROR("A instance was defined without an argument");
        free(instance);
        return -1;
    }

    cdtime_t interval = 0;

    instance->timeout = plugin_get_interval();
    instance->verify_peer = true;
    instance->verify_host = true;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &instance->url);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &instance->user);
        } else if (strcasecmp("user-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &instance->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &instance->pass);
        } else if (strcasecmp("password-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &instance->pass);
        } else if (strcasecmp("token", child->key) == 0) {
            status = cf_util_get_string(child, &instance->token);
        } else if (strcasecmp("version", child->key) == 0) {
            status = cf_util_get_string(child, &instance->version);
        } else if (strcasecmp("query", child->key) == 0) {
            status = redfish_config_request(child, instance);
        } else if (strcasecmp("verify-peer", child->key) == 0) {
            status = cf_util_get_boolean(child, &instance->verify_peer);
        } else if (strcasecmp("verify-host", child->key) == 0) {
            status = cf_util_get_boolean(child, &instance->verify_host);
        } else if (strcasecmp("ca-cert", child->key) == 0) {
            status = cf_util_get_string(child, &instance->cacert);
        } else if (strcasecmp("proxy", child->key) == 0) {
            status = cf_util_get_string(child, &instance->proxy);
        } else if (strcasecmp("proxy-tunnel", child->key) == 0) {
            status = cf_util_get_boolean(child, &instance->proxy_tunnel);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &instance->metric_prefix);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &instance->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &instance->timeout);
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = plugin_filter_configure(child, &instance->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    while (status == 0) {
        if (instance->url == NULL) {
            PLUGIN_ERROR("Instance '%s' has no 'url' attribute", instance->name);
            status = -1;
            break;
        }

        if ((instance->user != NULL) && (instance->token != NULL)) {
            PLUGIN_ERROR("In instance '%s' only one of 'user' or 'token' can be defined",
                         instance->name);
            status = -1;
            break;
        }

        if ((instance->user != NULL) && (instance->pass == NULL)) {
            PLUGIN_ERROR("Instance '%s' does not have password defined", instance->name);
            status = -1;
            break;
        }

        if (instance->requests_num == 0)
            PLUGIN_WARNING("Instance '%s' does not have queries", instance->name);

        if (instance->version == NULL) {
            instance->version = strdup(REDFISH_VERSION);
            if (instance->version == NULL) {
                PLUGIN_ERROR("strdup failed.");
                status = -1;
                break;
            }
        }

        break;
    }

    if (status != 0) {
        redfish_instance_free(instance);
        return -1;
    }

    label_set_add(&instance->labels, true, "instance", instance->name);

    return plugin_register_complex_read("redfish", instance->name, redfish_read, interval,
                                        &(user_data_t){.data = instance,
                                                       .free_func = redfish_instance_free});
}

static int redfish_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("query", child->key) == 0) {
            status = redfish_config_query(child);
        } else if (strcasecmp("instance", child->key) == 0) {
            status = redfish_config_instance(child);
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

static int redfish_init(void)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);

    return 0;
}

static int redfish_shutdown(void)
{
    if (queries != NULL) {
        c_avl_iterator_t *i = c_avl_get_iterator(queries);
        redfish_query_t *query;
        while (c_avl_iterator_next(i, NULL, (void *)(&query)) == 0) {
            redfish_query_free(query);
        }
        c_avl_iterator_destroy(i);
        c_avl_destroy(queries);
    }

    return 0;
}

void module_register(void)
{
    plugin_register_init("redfish", redfish_init);
    plugin_register_config("redfish", redfish_config);
    plugin_register_shutdown("redfish", redfish_shutdown);
}
