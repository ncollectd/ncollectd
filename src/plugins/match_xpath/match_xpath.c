// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2009,2010 Amit Gupta
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Amit Gupta <amit.gupta221 at gmail.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/llist.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

typedef struct {
    char *key;
    char *value_from;
} metric_label_from_t;

typedef struct {
    char *prefix;
    char *url;
} match_namespace_t;

typedef struct {
    match_namespace_t *namespaces;
    size_t num;
} match_namespace_list_t;

struct match_xpath_metric_s {
    char *path;

    char *metric;
    char *metric_prefix;
    char *metric_from;
    match_metric_type_t type;
    char *help;

    label_set_t labels;

    metric_label_from_t *labels_from;
    size_t labels_from_num;

    char *value_from;
    char *time_from;

    int is_table;  // ???
    unsigned long magic; // ???
    struct match_xpath_metric_s *next;
};
typedef struct match_xpath_metric_s match_xpath_metric_t;

typedef struct {
    char *metric_prefix;
    label_set_t labels;
    match_namespace_list_t ns_list;
    match_xpath_metric_t *metrics;
} match_xpath_t;

static cdtime_t match_xpath_parse_time(char const *tbuf)
{
    double t;
    char *endptr = NULL;

    errno = 0;
    t = strtod(tbuf, &endptr);
    if ((errno != 0) || (endptr == NULL) || (endptr[0] != 0))
        return cdtime();

    return DOUBLE_TO_CDTIME_T(t);
}

static void match_xpath_destroy(void *user_data)
{
    match_xpath_t *xpath = user_data;
    if (xpath == NULL)
        return;

    match_xpath_metric_t *xpath_metric = xpath->metrics;
    while(xpath_metric != NULL) {
        match_xpath_metric_t *next = xpath_metric->next;
        free(xpath_metric->path);
        free(xpath_metric->value_from);
        free(xpath_metric);
        xpath_metric = next;
    }

    if (xpath->ns_list.namespaces != NULL) {
        for (size_t i = 0; i < xpath->ns_list.num; i++) {
            free(xpath->ns_list.namespaces[i].prefix);
            free(xpath->ns_list.namespaces[i].url);
        }
        free(xpath->ns_list.namespaces);
    }

    free(xpath);
}

static xmlXPathObjectPtr match_xpath_evaluate_xpath(xmlXPathContextPtr xpath_ctx, char *expr)
{
    xmlXPathObjectPtr xpath_obj = xmlXPathEvalExpression(BAD_CAST expr, xpath_ctx);
    if (xpath_obj == NULL) {
        PLUGIN_WARNING("Error unable to evaluate xpath expression \"%s\". Skipping...", expr);
        return NULL;
    }
    return xpath_obj;
}

static int match_xpath_if_not_text_node(xmlNodePtr node)
{
    if ((node->type == XML_TEXT_NODE) ||
        (node->type == XML_ATTRIBUTE_NODE) ||
        (node->type == XML_ELEMENT_NODE))
        return 0;

    PLUGIN_WARNING("Node \"%s\" doesn't seem to be a text node. Skipping...", node->name);
    return -1;
}

/* Returned value should be freed with xmlFree(). */
static char *match_xpath_get_text_node(xmlXPathContextPtr xpath_ctx, char *expr, const char *option)
{
    xmlXPathObjectPtr values_node_obj = match_xpath_evaluate_xpath(xpath_ctx, expr);
    if (values_node_obj == NULL)
        return NULL; /* Error already logged. */

    xmlNodeSetPtr values_node = values_node_obj->nodesetval;
    size_t tmp_size = (values_node) ? values_node->nodeNr : 0;

    if (tmp_size == 0) {
        PLUGIN_WARNING("relative xpath expression \"%s\" from '%s' doesn't match "
                       "any of the nodes.", expr, option);
        xmlXPathFreeObject(values_node_obj);
        return NULL;
    }

    if (tmp_size > 1) {
        PLUGIN_WARNING("relative xpath expression \"%s\" from '%s' is expected to return "
                       "only one text node. Skipping the node.", expr, option);
        xmlXPathFreeObject(values_node_obj);
        return NULL;
    }

    /* ignoring the element if other than textnode/attribute*/
    if (match_xpath_if_not_text_node(values_node->nodeTab[0])) {
        PLUGIN_WARNING("relative xpath expression \"%s\" from '%s' is expected to return "
                       "only text/attribute node which is not the case. "
                       "Skipping the node.", expr, option);
        xmlXPathFreeObject(values_node_obj);
        return NULL;
    }

    char *node_value = (char *)xmlNodeGetContent(values_node->nodeTab[0]);

    /* free up object */
    xmlXPathFreeObject(values_node_obj);

    return node_value;
}

int match_xpath_match_node(match_xpath_t *xpath, match_xpath_metric_t *xpath_metric,
                           match_metric_family_set_t *set,  xmlXPathContextPtr xpath_ctx)
{
    cdtime_t t = 0;
    if (xpath_metric->time_from != NULL) {
        char *value = match_xpath_get_text_node(xpath_ctx, xpath_metric->time_from, "TimeFrom");
        if (value != NULL) {
            t = match_xpath_parse_time(value);
            xmlFree(value);
        }
    }

    strbuf_t buf = STRBUF_CREATE;

    if (xpath->metric_prefix != NULL)
        strbuf_print(&buf, xpath->metric_prefix);
    if (xpath_metric->metric_prefix != NULL)
        strbuf_print(&buf, xpath_metric->metric_prefix);

    if (xpath_metric->metric_from != NULL) {
        char *value = match_xpath_get_text_node(xpath_ctx, xpath_metric->metric_from, "MetricFrom");
        if (value != NULL) {
            strbuf_print(&buf, value);
            xmlFree(value);
        }
    } else {
        strbuf_print(&buf, xpath_metric->metric);
    }

    label_set_t mlabel = {0};

    for (size_t i = 0; i < xpath->labels.num; i++) {
        label_set_add(&mlabel, true, xpath->labels.ptr[i].name, xpath->labels.ptr[i].value);
    }

    for (size_t i = 0; i < xpath_metric->labels.num; i++) {
        label_set_add(&mlabel, true, xpath_metric->labels.ptr[i].name,
                                     xpath_metric->labels.ptr[i].value);
    }

    if (xpath_metric->labels_from_num > 0) {
        for (size_t i = 0; i < xpath_metric->labels_from_num; ++i) {
            char *value = match_xpath_get_text_node(xpath_ctx,
                                                    xpath_metric->labels_from[i].value_from,
                                                    "LabelFrom");
            if (value != NULL) {
                label_set_add(&mlabel, true, xpath_metric->labels_from[i].key, value);
                xmlFree(value);
            }
        }
    }

    char *value = match_xpath_get_text_node(xpath_ctx, xpath_metric->value_from, "ValueFrom");
    if (value != NULL) {
        plugin_match_metric_family_set_add(set, buf.ptr, xpath_metric->help, NULL,
                                           xpath_metric->type, &mlabel, value, t);
        xmlFree(value);
    }

    label_set_reset(&mlabel);
    strbuf_destroy(&buf);
    return 0;
}

static int match_xpath_match_metric(match_xpath_t *xpath, match_xpath_metric_t *xpath_metric,
                                    match_metric_family_set_t *set, xmlXPathContextPtr xpath_ctx)
{
    xmlXPathObjectPtr base_node_obj = match_xpath_evaluate_xpath(xpath_ctx, xpath_metric->path);
    if (base_node_obj == NULL)
        return -1; /* error is logged already */

    xmlNodeSetPtr base_nodes = base_node_obj->nodesetval;
    int total_nodes = (base_nodes) ? base_nodes->nodeNr : 0;

    if (total_nodes == 0) {
        PLUGIN_ERROR("xpath expression \"%s\" doesn't match any of the nodes. "
                     "Skipping the xpath block...", xpath_metric->path);
        xmlXPathFreeObject(base_node_obj);
        return -1;
    }

    /* If base_xpath returned multiple results, then */
    /* InstanceFrom or PluginInstanceFrom in the xpath block is required */
#if 0
    if ((total_nodes > 1) && (xpath->instance == NULL) && (xpath->plugin_instance_from == NULL)) {
        PLUGIN_ERROR("InstanceFrom or PluginInstanceFrom is must in xpath block "
                     "since the base xpath expression \"%s\" "
                     "returned multiple results. Skipping the xpath block...", xpath->path);
        xmlXPathFreeObject(base_node_obj);
        return -1;
    }
#endif

    for (int i = 0; i < total_nodes; i++) {
        xpath_ctx->node = base_nodes->nodeTab[i];
        match_xpath_match_node(xpath, xpath_metric, set, xpath_ctx);
    }

    xmlXPathFreeObject(base_node_obj);
    return 0;
}

static int match_xpath_match(match_metric_family_set_t *set, char *buffer, void *user_data)
{
    match_xpath_t *xpath = user_data;
    if (xpath == NULL)
        return -1;

    /* Load the XML */
    xmlDocPtr doc = xmlParseDoc(BAD_CAST buffer);
    if (doc == NULL) {
        PLUGIN_ERROR("Failed to parse the xml document - %s", buffer);
        return -1;
    }

    xmlXPathContextPtr xpath_ctx = xmlXPathNewContext(doc);
    if (xpath_ctx == NULL) {
        PLUGIN_ERROR("Failed to create the xml context");
        xmlFreeDoc(doc);
        return -1;
    }

    for (size_t i = 0; i < xpath->ns_list.num; i++) {
        match_namespace_t const *ns = xpath->ns_list.namespaces + i;
        int status = xmlXPathRegisterNs(xpath_ctx, BAD_CAST ns->prefix, BAD_CAST ns->url);
        if (status != 0) {
            PLUGIN_ERROR("unable to register NS with prefix=\"%s\" and href=\"%s\"\n",
                         ns->prefix, ns->url);
            xmlXPathFreeContext(xpath_ctx);
            xmlFreeDoc(doc);
            return status;
        }
    }

    match_xpath_metric_t *xpath_metric = xpath->metrics;
    while (xpath_metric != NULL) {
        match_xpath_match_metric(xpath, xpath_metric, set, xpath_ctx);
        xpath_metric = xpath_metric->next;
    }

    xmlXPathFreeContext(xpath_ctx);
    xmlFreeDoc(doc);
    return 0;
}

static int match_xpath_config_append_label(metric_label_from_t **var,
                                           size_t *len, config_item_t *ci)
{
    if (ci->values_num != 2) {
        PLUGIN_ERROR("\"%s\" expects two arguments.", ci->key);
        return -1;
    }
    if ((CONFIG_TYPE_STRING != ci->values[0].type) ||
        (CONFIG_TYPE_STRING != ci->values[1].type)) {
        PLUGIN_ERROR("\"%s\" expects two strings arguments.", ci->key);
        return -1;
    }

    metric_label_from_t *tmp = realloc(*var, ((*len) + 1) * sizeof(**var));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed: %s.", STRERRNO);
        return -1;
    }
    *var = tmp;

    tmp[*len].key = strdup(ci->values[0].value.string);
    if (tmp[*len].key == NULL) {
        PLUGIN_ERROR("strdup failed.");
        return -1;
    }

    tmp[*len].value_from = strdup(ci->values[1].value.string);
    if (tmp[*len].value_from == NULL) {
        PLUGIN_ERROR("strdup failed.");
        return -1;
    }

    *len = (*len) + 1;
    return 0;
}

/*
  match xpath {
      mamespace-Prefix URL
      metric-prefix
      label
      metric {
          xpath       "table[@id=\"magic_level\"]/tr"
          values-from "td[2]/span[@class=\"level\"]"
      }
  }
*/

static int match_xpath_config_metric(const config_item_t *ci, match_xpath_t *xpath)
{
    match_xpath_metric_t *xpath_metric = calloc(1, sizeof(*xpath_metric));
    if (xpath_metric == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    if (xpath->metrics == NULL) {
        xpath->metrics = xpath_metric;
    } else {
        match_xpath_metric_t *ptr = xpath->metrics;
        while (ptr->next != NULL)
            ptr = ptr->next;
        ptr->next = xpath_metric;
    }

    int status = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("xpath", child->key) == 0)
            status = cf_util_get_string(ci, &xpath_metric->path);
        else if (strcasecmp("type", child->key) == 0)
            status = cf_util_get_match_metric_type(child, &xpath_metric->type);
        else if (strcasecmp("metric", child->key) == 0)
            status = cf_util_get_string(child, &xpath_metric->metric);
        else if (strcasecmp("metric-prefix", child->key) == 0)
            status = cf_util_get_string(child, &xpath_metric->metric_prefix);
        else if (strcasecmp("help", child->key) == 0)
            status = cf_util_get_string(child, &xpath_metric->help);
        else if (strcasecmp("label", child->key) == 0)
            status = cf_util_get_label(child, &xpath_metric->labels);
        else if (strcasecmp("metric-from", child->key) == 0)
            status = cf_util_get_string(child, &xpath_metric->metric_from);
        else if (strcasecmp("label-from", child->key) == 0)
            status = match_xpath_config_append_label(&xpath_metric->labels_from,
                                                     &xpath_metric->labels_from_num, child);
        else if (strcasecmp("value-from", child->key) == 0)
            status = cf_util_get_string(child, &xpath_metric->value_from);
        else if (strcasecmp("time-from", child->key) == 0)
            status = cf_util_get_string(child, &xpath_metric->time_from);
        else {
            PLUGIN_WARNING("Option `%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        return -1;
    }

    if (xpath_metric->type == 0) {
        PLUGIN_WARNING("`Type' missing in `xpath' block.");
        return -1;
    }

    /* error out if xpath->path is an empty string */
    if (strlen(xpath_metric->path) == 0) {
        PLUGIN_ERROR("invalid xpath. xpath value can't be an empty string");
        return -1;
    }

    if (xpath_metric->value_from == NULL) {
        PLUGIN_WARNING("`ValuesFrom' missing in `xpath' block.");
        return -1;
    }

    return 0;
}

static int match_xpath_add_namespace(config_item_t *ci,  match_namespace_list_t *list)
{
    if ((ci->values_num != 2) ||
        (ci->values[0].type != CONFIG_TYPE_STRING) ||
        (ci->values[1].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The `Namespace' option needs exactly two string arguments.");
        return EINVAL;
    }

    match_namespace_t *ns = realloc(list->namespaces,
                                    sizeof(*list->namespaces) * (list->num + 1));
    if (ns == NULL) {
        PLUGIN_ERROR("realloc failed.");
        return ENOMEM;
    }
    list->namespaces = ns;
    ns = list->namespaces + list->num;
    memset(ns, 0, sizeof(*ns));

    ns->prefix = strdup(ci->values[0].value.string);
    ns->url = strdup(ci->values[1].value.string);

    if ((ns->prefix == NULL) || (ns->url == NULL)) {
        free(ns->prefix);
        free(ns->url);
        PLUGIN_ERROR("strdup failed.");
        return ENOMEM;
    }

    list->num++;
    return 0;
}

static int match_xpath_config(const config_item_t *ci, void **user_data)
{
    *user_data = NULL;

    match_xpath_t *xpath = calloc(1, sizeof(*xpath));
    if (xpath == NULL)
        return -1;

    int status = 0;
    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *option = ci->children + i;

        if (strcasecmp("metric-prefix", option->key) == 0) {
            status = cf_util_get_string(option, &xpath->metric_prefix);
        } else if (strcasecmp("label", option->key) == 0) {
            status = cf_util_get_label(option, &xpath->labels);
        } else if (strcasecmp("namespace", option->key) == 0) {
            status = match_xpath_add_namespace(option, &xpath->ns_list);
        } else if (strcasecmp("metric", option->key) == 0) {
            status = match_xpath_config_metric(option, xpath);
        } else {
            PLUGIN_WARNING("Option `%s' not allowed here.", option->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        match_xpath_destroy(xpath);
        return -1;
    }

    *user_data = xpath;

    return 0;
}

void module_register(void)
{
    plugin_match_proc_t proc = {0};

    proc.config = match_xpath_config;
    proc.destroy = match_xpath_destroy;
    proc.match = match_xpath_match;

    plugin_register_match("xpath", proc);
}
