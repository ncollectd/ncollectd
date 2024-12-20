// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2024 Matwey V. Kornilov
// SPDX-FileCopyrightText: Copyright (C) 2024 Manuel Sanmartín
// SPDX-FileContributor: Matwey V. Kornilov <matwey.kornilov at gmail.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/avltree.h"

#include <cadef.h>

typedef union {
    double float64;
    uint64_t int64;
} epics_value_t;

typedef struct {
    epics_value_t *ptr;
    long int num;
} epics_value_list_t;

typedef enum {
    EPICS_PV_VALUE,
    EPICS_PV_LABEL
} epics_pv_type_t;

typedef struct {
    char *name;
    chtype ch_type;
    chid id;
    evid eid;
    bool is_active;
    epics_pv_type_t type;
    union {
        char *label;
        epics_value_list_t vl;
    };
} epics_pv_t;

typedef struct {
    char *name;
    char *help;
    metric_type_t type;
    label_set_t labels;
    char *value_from;
    long int value_idx;
    label_set_t labels_from;
} epics_metric_t;

static epics_metric_t **epics_metrics;
static size_t epics_metrics_num;

static char *g_metric_prefix;
static label_set_t g_labels;

static c_avl_tree_t *pv_tree;
static pthread_mutex_t pv_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t thread_id;
static volatile bool thread_loop;

static void epics_pv_free(epics_pv_t *pv)
{
    if (pv == NULL)
        return;

    free(pv->name);

    switch(pv->type) {
    case EPICS_PV_VALUE:
        free(pv->vl.ptr);
        break;
    case EPICS_PV_LABEL:
        free(pv->label);
        break;
    }


    free(pv);
}

static void epics_pv_tree_free(c_avl_tree_t *tree)
{
    if (tree == NULL)
        return;

    while (true) {
        epics_pv_t *pv = NULL;
        char *name = NULL;
        int status = c_avl_pick(tree, (void *)&name, (void *)&pv);
        if (status != 0)
            break;
        epics_pv_free(pv);
    }

    c_avl_destroy(tree);

}

static epics_pv_t *epics_pv_tree_get(c_avl_tree_t *tree, char *name)
{
    if ((tree == NULL) || (name == NULL))
        return NULL;

    epics_pv_t *pv = NULL;
    int status = c_avl_get(tree, pv, (void *)&name);
    if (status != 0)
        return NULL;

    return pv;
}

static int epics_pv_tree_add(c_avl_tree_t **tree, char *name, epics_pv_type_t type)
{
    if (*tree == NULL) {
        *tree = c_avl_create((int (*)(const void *, const void *))strcmp);
        if (tree == NULL) {
            PLUGIN_ERROR("Failed to create tree.");
            return -1;
        }
    }
    
    epics_pv_t *pv = NULL;
    int status = c_avl_get(*tree, name, (void *)&pv);
    if (status == 0) {
        if (pv->type == type)
            return 0;
        PLUGIN_ERROR("Existing PV with diferent type: '%s'.", name);
        return -1;
    }   

    char *name_dup = strdup(name);
    if (name_dup == NULL) {
        PLUGIN_ERROR("strdup failed.");
        return -1;
    }

    pv = calloc(1, sizeof(*pv));
    if (pv == NULL) {
        free(name_dup);
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    pv->name = name_dup;
    pv->type = type;

    status = c_avl_insert(*tree, pv->name, pv);
    if (status != 0) {
        epics_pv_free(pv);
        return -1;
    }

    return 0;
}

static int printf_handler(const char *pformat, va_list args)
{
#ifdef NCOLLECT_DEBUG
    char msg[1024] = ""; // Size inherits from plugin_log()

    int status = vsnprintf(msg, sizeof(msg), pformat, args);

    if (status < 0) {
        return status;
    }

    msg[strcspn(msg, "\r\n")] = '\0';
    plugin_log(LOG_DEBUG, "%s", msg);

    return status;
#else
    (void)pformat;
    (void)args;
    return 0;
#endif
}

static void event_handler(evargs args)
{
    epics_pv_t *pv =  (epics_pv_t *)args.usr;

    if (args.status != ECA_NORMAL) {
        PLUGIN_ERROR("Error %s at channel \"%s\"", ca_message(args.status), pv->name);
        return;
    }

    pthread_mutex_lock(&pv_lock);

    switch (pv->type) {
    case EPICS_PV_VALUE:
        if (args.count != pv->vl.num) {
            PLUGIN_ERROR("Unexpected channel element count %ld for channel '%s'",
                          args.count, pv->name);
        } else {
            switch (args.type) {
            case DBR_SHORT:
                if (pv->ch_type == DBR_LONG) {
                    const dbr_short_t *value = (const dbr_short_t *)args.dbr;
                    for (long int i = 0; i < pv->vl.num; i++) {
                        pv->vl.ptr[i].int64 = value[i];
                    }
                } else {
                    PLUGIN_ERROR("Unexpected data type '%s' for channel '%s'.",
                                 dbr_type_to_text(args.type), pv->name);
                }
                break;
            case DBR_LONG:
                if (pv->ch_type == DBR_LONG) {
                    const dbr_long_t *value = (const dbr_long_t *)args.dbr;
                    for (long int i = 0; i < pv->vl.num; i++) {
                        pv->vl.ptr[i].int64 = value[i];
                    }
                } else {
                    PLUGIN_ERROR("Unexpected data type '%s' for channel '%s'.",
                                 dbr_type_to_text(args.type), pv->name);
                }
                break;
            case DBR_FLOAT:
                if (pv->ch_type == DBR_DOUBLE) {
                    const dbr_float_t *value = (const dbr_float_t *)args.dbr;
                    for (long int i = 0; i < pv->vl.num; i++) {
                        pv->vl.ptr[i].float64 = value[i];
                    }
                } else {
                    PLUGIN_ERROR("Unexpected data type '%s' for channel '%s'.",
                                 dbr_type_to_text(args.type), pv->name);
                }
                break;
            case DBR_DOUBLE:
                if (pv->ch_type == DBR_DOUBLE) {
                    const dbr_double_t *value = (const dbr_double_t *)args.dbr;
                    for (long int i = 0; i < pv->vl.num; i++) {
                        pv->vl.ptr[i].float64 = value[i];
                    }
                } else {
                    PLUGIN_ERROR("Unexpected data type '%s' for channel '%s'.",
                                 dbr_type_to_text(args.type), pv->name);
                }
                break;
            case DBR_ENUM:
                if (pv->ch_type == DBR_DOUBLE) {
                    const dbr_enum_t *value = (const dbr_enum_t *)args.dbr;
                    for (long int i = 0; i < pv->vl.num; i++) {
                        pv->vl.ptr[i].float64 = value[i];
                    }
                } else {
                    PLUGIN_ERROR("Unexpected data type '%s' for channel '%s'.",
                                 dbr_type_to_text(args.type), pv->name);
                }
                break;
            default:
                PLUGIN_ERROR("Unexpected data type '%s' for channel '%s'.",
                             dbr_type_to_text(args.type), pv->name);
                break;
            }
        }
        break;
    case EPICS_PV_LABEL:
        if (args.count == 1) {
            free(pv->label);
            pv->label = strdup((const char *)args.dbr);
            if (pv->label == NULL)
                PLUGIN_ERROR("Cannot allocate memory for '%s' value", pv->name);
        } else {
            PLUGIN_ERROR("Unexpected channel element count %ld for channel '%s'",
                          args.count, pv->name);
        }
        break;
    }

    pthread_mutex_unlock(&pv_lock);
}

static void connection_handler(struct connection_handler_args args)
{
    epics_pv_t *pv = (epics_pv_t *)ca_puser(args.chid);

    pthread_mutex_lock(&pv_lock);

    switch (args.op) {
    case CA_OP_CONN_UP:
        if (pv->eid) {
            PLUGIN_INFO("Channel '%s' reconnected", pv->name);
            pv->is_active = true;
            return;
        }

        switch (pv->type) {
        case EPICS_PV_VALUE: {
            int ch_type = ca_field_type(pv->id);

            switch(ch_type) {
            case DBR_SHORT:
            case DBR_LONG:
                pv->ch_type = DBR_LONG;
                break;
            case DBR_FLOAT:
            case DBR_DOUBLE:
            case DBR_ENUM:
                pv->ch_type = DBR_DOUBLE;
                break;
            default:
                PLUGIN_ERROR("Unsuported channel type '%s' for channel '%s'.",
                             dbf_type_to_text(pv->ch_type), pv->name);
                pthread_mutex_unlock(&pv_lock);
                break;
            }

            long int count = ca_element_count(pv->id);

            pv->vl.ptr = calloc(count, sizeof(*pv->vl.ptr));
            if (pv->vl.ptr == NULL) {
                PLUGIN_ERROR("Cannot allocate memory for %ld values or channel '%s'",
                             count, pv->name);
            } else {
                pv->vl.num = count;
                if (pv->ch_type == DBR_DOUBLE) {
                    for (long int i = 0; i < pv->vl.num; i++) {
                        pv->vl.ptr[i].float64 = 0.0;
                    }
                }

                int ret = ca_create_subscription(pv->ch_type, pv->vl.num, pv->id,
                                         DBE_VALUE | DBE_ALARM, event_handler, pv, &pv->eid);
                if (ret != ECA_NORMAL) {
                    PLUGIN_ERROR("CA error %s occurred while trying to create "
                                 "subscription for channel '%s'", ca_message(ret), pv->name);
                } else {
                    pv->is_active = true;
                }
            }
        }   break;
        case EPICS_PV_LABEL:
            pv->ch_type = DBR_STRING;
            break;
        }
        break;
    case CA_OP_CONN_DOWN:
        PLUGIN_WARNING("Channel \"%s\" disconnected", pv->name);
        pv->is_active = false;
        break;
    }

    pthread_mutex_unlock(&pv_lock);
}

static void *epics_thread(__attribute__ ((__unused__)) void *args)
{
    long ret = ca_context_create(ca_disable_preemptive_callback);
    if (ret != ECA_NORMAL) {
        // FIXME: report error back to start_thread()
        PLUGIN_ERROR("CA error %s occurred while trying to start channel access",
                      ca_message(ret));
        return (void *)1;
    }

    ca_replace_printf_handler(&printf_handler);

    pthread_mutex_lock(&pv_lock);

    c_avl_iterator_t *iter = c_avl_get_iterator(pv_tree);
    char *name = NULL;
    epics_pv_t *pv = NULL;
    while (c_avl_iterator_next(iter, (void *)&name, (void *)&pv) == 0) {
        ret = ca_create_channel(pv->name, &connection_handler, pv, 0, &pv->id);
        if (ret != ECA_NORMAL) {
            PLUGIN_ERROR("CA error %s occurred while trying to create channel \"%s\"",
                         ca_message(ret), pv->name);
            ret = 1;
            pthread_mutex_unlock(&pv_lock);
            goto error;
        }
    }
    c_avl_iterator_destroy(iter);

    pthread_mutex_unlock(&pv_lock);

    const double timeout = 2.0;
    while (thread_loop != 0) {
        ca_pend_event(timeout);
    }

error:

    pthread_mutex_lock(&pv_lock);

    iter = c_avl_get_iterator(pv_tree);
    while (c_avl_iterator_next(iter, (void *)&name, (void *)&pv) == 0) {
        if (pv->eid) {
            ca_clear_subscription(pv->eid);
            pv->eid = 0;
        }
        ca_clear_channel(pv->id);
    }

    c_avl_iterator_destroy(iter);

    pthread_mutex_unlock(&pv_lock);

    ca_context_destroy();

    return (void *)ret;
}

static int epics_dispatch_metric(epics_metric_t *em)
{
    label_set_t labels = {0};

    label_set_clone(&labels, g_labels);
    label_set_add_set(&labels, true, em->labels);

    for (size_t i = 0; i < em->labels_from.num; i++) {
        label_pair_t *pair = &em->labels_from.ptr[i];
        epics_pv_t *pv_label = epics_pv_tree_get(pv_tree, pair->value);
        if (pv_label == NULL) {
            label_set_reset(&labels);
            return -1;
        }

        if (!pv_label->is_active) {
            label_set_reset(&labels);
            return -1;
        }

        if (pv_label->type != EPICS_PV_LABEL) {
            label_set_reset(&labels);
            return -1;
        }

        label_set_add(&labels, true, pair->name, pv_label->label);
    }

    epics_pv_t *pv_value = epics_pv_tree_get(pv_tree, em->value_from);
    if (pv_value == NULL) {
        label_set_reset(&labels);
        return -1;
    }

    if (pv_value->type != EPICS_PV_LABEL) {
        label_set_reset(&labels);
        return -1;
    }

    if (em->value_idx > pv_value->vl.num) {
        label_set_reset(&labels);
        return -1;
    }
 
    value_t value = {0};

    if (em->type == METRIC_TYPE_COUNTER) {
        if (pv_value->type == DBR_LONG) {
            value = VALUE_COUNTER(pv_value->vl.ptr[em->value_idx].int64);
        } else if (pv_value->type == DBR_DOUBLE) {
            value = VALUE_COUNTER_FLOAT64(pv_value->vl.ptr[em->value_idx].float64);
        }
    } else if (em->type == METRIC_TYPE_GAUGE) {
        if (pv_value->type == DBR_LONG) {
            value = VALUE_GAUGE_INT64(pv_value->vl.ptr[em->value_idx].int64);
        } else if (pv_value->type == DBR_DOUBLE) {
            value = VALUE_GAUGE(pv_value->vl.ptr[em->value_idx].float64);
        }
    }

    metric_family_t fam = {0};
    fam.type = em->type;
    fam.help = em->help;
        
    char buffer[1024];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);
    if (g_metric_prefix != NULL)
        strbuf_putstr(&buf, g_metric_prefix);
    strbuf_putstr(&buf, em->name);

    fam.name = buf.ptr;

    metric_family_append(&fam, value, &labels, NULL);

    plugin_dispatch_metric_family(&fam, 0);

    return 0;
}

static int epics_read(void)
{
    pthread_mutex_lock(&pv_lock);

    for (size_t i = 0; i < epics_metrics_num; i++) {
        epics_metric_t *em = epics_metrics[i];
        epics_dispatch_metric(em);
    }

    pthread_mutex_unlock(&pv_lock);

    return 0;
}

static void epics_metric_free(epics_metric_t *em)
{
    if (em == NULL)
        return;

    free(em->name);
    free(em->help);
    label_set_reset(&em->labels);
    label_set_reset(&em->labels_from);
    free(em->value_from);

    free(em);
}

static int epics_config_value_from(config_item_t *ci, char **ret_str, long int *ret_num)
{
    if (ci->values_num == 1) {
        if (ci->values[0].type != CONFIG_TYPE_STRING)
            goto error;
    } else if (ci->values_num == 2) {
        if ((ci->values[0].type != CONFIG_TYPE_STRING) ||
            (ci->values[1].type != CONFIG_TYPE_NUMBER)) {
            goto  error;
        }
    } else {
        goto error;
    }

    *ret_num = (long int)ci->values[0].value.number;

    char *string = strdup(ci->values[0].value.string);
    if (string == NULL) {
        PLUGIN_ERROR("strdup failed.");
        return -1;
    }

    if (*ret_str != NULL)
        free(*ret_str);
    *ret_str = string;

    return 0;

error:
    PLUGIN_ERROR("The '%s' option in %s:%d requires one string and an optional index as argument.",
                 ci->key, cf_get_file(ci), cf_get_lineno(ci));
    return -1;
}

static int epics_config_metric(config_item_t *ci)
{
    epics_metric_t *em = calloc(1, sizeof(*em));
    if (em == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    em->type = METRIC_TYPE_GAUGE;

    int status = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("metric", child->key) == 0) {
            status = cf_util_get_string(child, &em->name);
        } else if (strcasecmp("help", child->key) == 0) {
            status = cf_util_get_string(child, &em->help);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &em->labels);
        } else if (strcasecmp("type", child->key) == 0) {
            status = cf_util_get_metric_type(child, &em->type);
        } else if (strcasecmp("label-from", child->key) == 0) {
            status = cf_util_get_label(child, &em->labels_from);
        } else if (strcasecmp("value-from", child->key) == 0) {
            status = epics_config_value_from(child, &em->value_from, &em->value_idx);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        epics_metric_free(em);
        return -1;
    }

    if (em->name == NULL) {
        PLUGIN_ERROR("No metric name has been configured.");
        epics_metric_free(em);
        return -1;
    }

    if (em->value_from == NULL) {
        PLUGIN_ERROR("No 'value-from' has been configured for metric '%s'.", em->name);
        epics_metric_free(em);
        return -1;
    }

    for (size_t i = 0; i < em->labels_from.num ; i++) {
        label_pair_t *pair = &em->labels_from.ptr[i];
        status = epics_pv_tree_add(&pv_tree, pair->value, EPICS_PV_LABEL);
        if (status != 0) {
            PLUGIN_ERROR("Failed to create PV for metric '%s'.", em->name);
            epics_metric_free(em);
            return -1;
        }
    }

    status = epics_pv_tree_add(&pv_tree, em->value_from, EPICS_PV_VALUE);
    if (status != 0) {
        PLUGIN_ERROR("Failed to create PV for metric '%s'.", em->name);
        epics_metric_free(em);
        return -1;
    }

    epics_metric_t **tmp = realloc(epics_metrics, sizeof(*tmp) * (epics_metrics_num + 1));
    if (tmp == NULL) {
        PLUGIN_ERROR("Realloc failed.");
        epics_metric_free(em);
        return -1;
    }

    epics_metrics = tmp;
    epics_metrics[epics_metrics_num] = em;
    epics_metrics_num++;

    return 0;
}

static int epics_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &g_metric_prefix);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &g_labels);
        } else if (strcasecmp("metric", child->key) == 0) {
            status = epics_config_metric(child);
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

static int epics_init(void)
{
    if (thread_loop == 0) {
        thread_loop = 1;
        int status = plugin_thread_create(&thread_id, epics_thread, (void *)0, "epics");
        if (status != 0) {
            thread_loop = 0;
            PLUGIN_ERROR("Starting thread failed: %d", status);
            return -1;
        }
    }

    return 0;
}

static int epics_shutdown(void)
{
    thread_loop = 0;
    pthread_join(thread_id, NULL);

    epics_pv_tree_free(pv_tree);
    pv_tree = NULL;

    if (epics_metrics_num > 0) {
        for (size_t i = 0; i < epics_metrics_num; i++) {
             epics_metric_free(epics_metrics[i]);
        }
        free(epics_metrics);
        epics_metrics_num = 0;
        epics_metrics = NULL;
    }

    label_set_reset(&g_labels);
    return 0;
}

void module_register(void)
{
    plugin_register_config("epics", epics_config);
    plugin_register_read("epics", epics_read);
    plugin_register_init("epics", epics_init);
    plugin_register_shutdown("epics", epics_shutdown);
}
