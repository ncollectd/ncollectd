// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "configfile.h"
#include "plugin_internal.h"
#include "plugin_match.h"
#include "libutils/common.h"
#include "libutils/avltree.h"

typedef struct {
    label_set_t label;
    value_t value;
    uint64_t values_num;
} match_metric_t;

typedef struct {
    char *name;
    char *help;
    char *unit;
    match_metric_type_t type;
    c_avl_tree_t *metrics;
} match_metric_family_t;

struct match_metric_family_set_s {
    c_avl_tree_t *tree;
};

struct match_s {
    char name[DATA_MAX_NAME_LEN];
    plugin_match_proc_t proc;
    struct match_s *next;
};
typedef struct match_s match_t;

static match_t *match_list_head;

struct plugin_match_s {
    char *name;
    void *user_data;
    plugin_match_proc_t proc;
    match_metric_family_set_t set;
    struct plugin_match_s *next;
};

static match_t *get_match(char *name)
{
    match_t *ptr = match_list_head;
    while (ptr != NULL) {
        if (strcasecmp(ptr->name, name) == 0)
            break;
        ptr = ptr->next;
    }

    if (ptr == NULL) {
        char plugin_name[DATA_MAX_NAME_LEN];

        int status = ssnprintf(plugin_name, sizeof(plugin_name), "match_%s", name);
        if ((status < 0) || ((size_t)status >= sizeof(plugin_name))) {
            PLUGIN_ERROR("Loading plugin \"match_%s\" failed: "
                         "plugin name would have been truncated.", name);
            return NULL;
        }

        status = plugin_load(plugin_name, /* flags = */ 0);
        if (status != 0) {
            PLUGIN_ERROR("Loading plugin \"%s\" failed with status %i.", plugin_name, status);
            return NULL;
        }

        ptr = match_list_head;
        while (ptr != NULL) {
            if (strcasecmp(ptr->name, name) == 0)
                break;
            ptr = ptr->next;
        }
    }

    return ptr;
}

int plugin_register_match(const char *name, plugin_match_proc_t proc)
{
    match_t *m = calloc(1, sizeof(*m));
    if (m == NULL)
        return -ENOMEM;

    sstrncpy(m->name, name, sizeof(m->name));
    memcpy(&m->proc, &proc, sizeof(m->proc));

    if (match_list_head == NULL) {
        match_list_head = m;
    } else {
        match_t *ptr = match_list_head;
        while (ptr->next != NULL)
            ptr = ptr->next;
        ptr->next = m;
    }

    return 0;
}

void plugin_free_register_match(void)
{
    match_t *m = match_list_head;
    while (m != NULL) {
        match_t *next = m->next;
        free(m);
        m = next;
    }
    match_list_head = NULL;
}


int plugin_match_config(const config_item_t *ci, plugin_match_t **plugin_match_list)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        WARNING("'Match' blocks require exactly one string argument.");
        return -1;
    }

    match_t *match = get_match(ci->values[0].value.string);
    if (match == NULL) {
        PLUGIN_WARNING("Cannot find match plugin: '%s'.", ci->values[0].value.string);
        return -1;
    }

    plugin_match_t *pm = calloc(1, sizeof(*pm));
    if (pm == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    pm->name = strdup(match->name);
    if (pm->name == NULL) {
        PLUGIN_ERROR("strdup failed.");
        free(pm);
        return -1;
    }

    pm->set.tree = c_avl_create((int (*)(const void *, const void *))strcmp);
    if (pm->set.tree == NULL) {
        PLUGIN_ERROR("avl_create failed.");
        free(pm->name);
        free(pm);
        return -1;
    }

    memcpy(&pm->proc, &match->proc, sizeof(pm->proc));

    if (pm->proc.config != NULL) {
        int status = (*pm->proc.config)(ci, &pm->user_data);
        if (status != 0) {
            PLUGIN_WARNING("Failed to create match: '%s'.", pm->name);
            c_avl_destroy(pm->set.tree);
            free(pm->name);
            free(pm);
            return -1;
        }
    }

    if (*plugin_match_list == NULL) {
        *plugin_match_list = pm;
    } else {
        plugin_match_t *ptr = *plugin_match_list;
        while (ptr->next != NULL)
            ptr = ptr->next;
        ptr->next = pm;
    }

    return 0;
}

int plugin_match(plugin_match_t *plugin_match_list, char *str)
{
    int status = 0;

    plugin_match_t *pm = plugin_match_list;
    while (pm != NULL) {
        if (pm->proc.match != NULL)
            status |= pm->proc.match(&pm->set, str, pm->user_data);
        pm = pm->next;
    }

    return status;
}

void plugin_match_shutdown(plugin_match_t *plugin_match_list)
{
    plugin_match_t *pm = plugin_match_list;
    while (pm != NULL) {
        if (pm->proc.destroy != NULL)
            pm->proc.destroy(pm->user_data);
        plugin_match_t *next = pm->next;
        if (pm->set.tree != NULL) {
            while (true) {
                char *name = NULL;
                match_metric_family_t *mfam = NULL;
                int status = c_avl_pick(pm->set.tree, (void *)&name, (void *)&mfam);
                if (status != 0)
                    break;

                if (mfam->metrics != NULL) {
                    while (true) {
                     label_set_t *labels = NULL;
                        match_metric_t *mm = NULL;
                        status = c_avl_pick(mfam->metrics, (void *)&labels, (void *)&mm);
                        if (status != 0)
                            break;
                        if (mm != NULL) {
                            label_set_reset(&mm->label);
                            free(mm);
                        }
                    }
                    c_avl_destroy(mfam->metrics);
                }
                free(mfam->name);
                free(mfam->help);
                free(mfam->unit);
                free(mfam);
            }
            c_avl_destroy(pm->set.tree);
        }
        free(pm->name);
        free(pm);
        pm = next;
    }
}

int cf_util_get_match_metric_type(const config_item_t *ci, match_metric_type_t *type)
{
    if (ci->values_num != 2) {
        PLUGIN_WARNING("Option 'type' needs exactly two strings argument.");
        return -1;
    }

    if ((ci->values[0].type != CONFIG_TYPE_STRING) ||
        (ci->values[1].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("Option 'type' needs exactly two strings argument.");
        return -1;
    }

    char const *stype = ci->values[0].value.string;
    char const *stypeflag = ci->values[1].value.string;

    if (strcasecmp("gauge", stype) == 0) {
        if (strcasecmp("average", stypeflag) == 0)
            *type = MATCH_METRIC_TYPE_GAUGE_AVERAGE;
        else if (strcasecmp("min", stypeflag) == 0)
            *type = MATCH_METRIC_TYPE_GAUGE_MIN;
        else if (strcasecmp("max", stypeflag) == 0)
            *type = MATCH_METRIC_TYPE_GAUGE_MAX;
        else if (strcasecmp("last", stypeflag) == 0)
            *type = MATCH_METRIC_TYPE_GAUGE_LAST;
        else if (strcasecmp("inc", stypeflag) == 0)
            *type = MATCH_METRIC_TYPE_GAUGE_INC;
        else if (strcasecmp("add", stypeflag) == 0)
            *type = MATCH_METRIC_TYPE_GAUGE_ADD;
        else if (strcasecmp("persist", stypeflag) == 0)
            *type = MATCH_METRIC_TYPE_GAUGE_PERSIST;
        else {
            return -1;
        }
    } else if (strcasecmp("counter", stype) == 0) {
        if (strcasecmp("set", stypeflag) == 0)
            *type = MATCH_METRIC_TYPE_COUNTER_SET;
        else if (strcasecmp("add", stypeflag) == 0)
            *type = MATCH_METRIC_TYPE_COUNTER_ADD;
        else if (strcasecmp("inc", stypeflag) == 0)
            *type = MATCH_METRIC_TYPE_COUNTER_INC;
        else {
            return -1;
        }
    } else {
        return -1;
    }

    return 0;
}

static void match_metric_value_reset(match_metric_t *mm, match_metric_type_t type)
{
    if (mm == NULL)
        return;

    if ((type & MATCH_METRIC_TYPE_MASK) == MATCH_METRIC_TYPE_GAUGE) {
        if (type != MATCH_METRIC_TYPE_GAUGE_PERSIST) {
            mm->value.gauge.float64 = type == MATCH_METRIC_TYPE_GAUGE_INC ? 0 : NAN;
            mm->values_num = 0;
        }
    }
}

static int match_metric_value_set(match_metric_t *mm, match_metric_type_t type, char const *svalue)
{
    switch (type & MATCH_METRIC_TYPE_MASK) {
    case MATCH_METRIC_TYPE_GAUGE: {
        double value = 0;
        if (type != MATCH_METRIC_TYPE_GAUGE_INC) {
            if (svalue == NULL)
                return -1;
            char *endptr = NULL;
            value = (double)strtod(svalue, &endptr);
            if (svalue == endptr)
                return -1;
        }

        switch(type) {
        case MATCH_METRIC_TYPE_GAUGE_AVERAGE:
            if (mm->values_num == 0) {
                mm->value.gauge.float64 = value;
            } else {
                double f = ((double)mm->values_num) / ((double)(mm->values_num + 1));
                mm->value.gauge.float64 = (mm->value.gauge.float64 * f) + (value * (1.0 - f));
            }
            mm->values_num++;
            break;
        case MATCH_METRIC_TYPE_GAUGE_MIN:
            if (mm->values_num == 0)
                mm->value.gauge.float64 = value;
            else if (mm->value.gauge.float64 > value)
                mm->value.gauge.float64 = value;
            break;
        case MATCH_METRIC_TYPE_GAUGE_MAX:
            if (mm->values_num == 0)
                mm->value.gauge.float64 = value;
            else if (mm->value.gauge.float64 < value)
                mm->value.gauge.float64 = value;
            break;
        case MATCH_METRIC_TYPE_GAUGE_LAST:
            mm->value.gauge.float64 = value;
            break;
        case MATCH_METRIC_TYPE_GAUGE_INC:
            if (isnan(mm->value.gauge.float64))
                mm->value.gauge.float64 = 1;
            else
                mm->value.gauge.float64 += 1;
            break;
        case MATCH_METRIC_TYPE_GAUGE_ADD:
            if (mm->values_num == 0)
                mm->value.gauge.float64 = value;
            else
                mm->value.gauge.float64 += value;
            break;
        case MATCH_METRIC_TYPE_GAUGE_PERSIST:
            mm->value.gauge.float64 = value;
            break;
        default:
            PLUGIN_ERROR("Invalid metric type.");
            break;
        }
    }
        break;
    case MATCH_METRIC_TYPE_COUNTER: {
        uint64_t value = 0;
        if (type != MATCH_METRIC_TYPE_COUNTER_INC) {
            if (svalue == NULL)
                return -1;
            char *endptr = NULL;
            value = (uint64_t)strtoull(svalue, &endptr, 0);
            if (svalue == endptr)
                return -1;
        }

        switch(type) {
        case MATCH_METRIC_TYPE_COUNTER_SET:
            mm->value.counter.uint64 = value;
            break;
        case MATCH_METRIC_TYPE_COUNTER_ADD:
            mm->value.counter.uint64 += value;
            break;
        case MATCH_METRIC_TYPE_COUNTER_INC:
            mm->value.counter.uint64++;
            break;
        default:
            PLUGIN_ERROR("Invalid metric type.");
            break;
        }
    }
        break;
    default:
        PLUGIN_ERROR("Invalid metric type.");
        break;
    }

    return 0;
}

static int match_metric_cmp_labels(const void *a, const void *b)
{
    label_set_t const *label_a = a;
    label_set_t const *label_b = b;

    if (label_a->num != label_b->num)
        return label_a->num - label_b->num;

    for (size_t i=0 ; i < label_a->num; i++) {
        int cmp = strcmp(label_a->ptr[i].name, label_b->ptr[i].name);
        if (cmp != 0)
            return cmp;

        cmp = strcmp(label_a->ptr[i].value, label_b->ptr[i].value);
        if (cmp != 0)
            return cmp;
    }

    return 0;
}

int plugin_match_metric_family_set_add(match_metric_family_set_t *set,
                                       char *name, char *help, char *unit,
                                       match_metric_type_t type, label_set_t *labels,
                                       char const *svalue, __attribute__((unused)) cdtime_t t)
{

    match_metric_family_t *mfam = NULL;
    int status = c_avl_get(set->tree, name, (void *)&mfam);
    if (status != 0) {
        mfam = calloc(1, sizeof(*mfam));
        if (unlikely(mfam == NULL)) {
            PLUGIN_ERROR("calloc failed.");
            return -1;
        }

        if (name != NULL) {
            mfam->name = strdup(name);
            if (mfam->name == NULL) {
                PLUGIN_ERROR("strdup failed.");
                free(mfam);
                return -1;
            }
        }

        if (help != NULL) {
            mfam->help = strdup(help);
            if (mfam->help == NULL) {
                PLUGIN_ERROR("strdup failed.");
                free(mfam->name);
                free(mfam);
                return -1;
            }
        }

        if (unit != NULL) {
            mfam->unit = strdup(unit);
            if (mfam->unit == NULL) {
                PLUGIN_ERROR("strdup failed.");
                free(mfam->name);
                free(mfam->help);
                free(mfam);
                return -1;
            }
        }

        mfam->type = type;

        mfam->metrics = c_avl_create(match_metric_cmp_labels);
        if (unlikely(mfam->metrics == NULL)) {
            PLUGIN_ERROR("avl creation failed.");
            free(mfam->name);
            free(mfam->help);
            free(mfam->unit);
            free(mfam);
            return -1;
        }

        status = c_avl_insert(set->tree, mfam->name, mfam);
        if (unlikely(status != 0)) {
            return -1;
        }
    }

    match_metric_t *mm = NULL;
    status = c_avl_get(mfam->metrics, labels, (void *)&mm);
    if (status != 0) {
        mm = calloc(1, sizeof(*mm));
        if (unlikely(mm == NULL)) {
            PLUGIN_ERROR("calloc failed.");
            return -1;
        }

        label_set_clone(&mm->label, *labels);

        match_metric_value_reset(mm, type);

        status = c_avl_insert(mfam->metrics, &mm->label, mm);
        if (unlikely(status != 0)) {
            PLUGIN_ERROR("avl insert failed.");
            return -1;
        }
    }

    match_metric_value_set(mm, type, svalue);

    return 0;
}

int plugin_match_dispatch(plugin_match_t *plugin_match_list, plugin_filter_t *filter,
                          label_set_t *labels, bool reset)
{
    plugin_match_t *pm = plugin_match_list;
    while (pm != NULL) {
        char *name = NULL;
        match_metric_family_t *mfam = NULL;
        c_avl_iterator_t *fiter = c_avl_get_iterator(pm->set.tree);
        while (c_avl_iterator_next(fiter,(void *)&name, (void *)&mfam) == 0) {
            metric_type_t type = METRIC_TYPE_UNKNOWN;
            switch (mfam->type & MATCH_METRIC_TYPE_MASK) {
            case MATCH_METRIC_TYPE_GAUGE:
                type = METRIC_TYPE_GAUGE;
                break;
            case MATCH_METRIC_TYPE_COUNTER:
                type = METRIC_TYPE_COUNTER;
                break;
            default:
                PLUGIN_WARNING("unsupported match metric type");
                continue;
                break;
            }

            metric_family_t fam = {
                .name = mfam->name,
                .help = mfam->help,
                .unit = mfam->unit,
                .type = type,
            };

            label_set_t *mlabels = NULL;
            match_metric_t *mm = NULL;
            c_avl_iterator_t *miter = c_avl_get_iterator(mfam->metrics);
            while (c_avl_iterator_next(miter,(void *)&mlabels, (void *)&mm) == 0) {
                metric_t m = {0};

                switch (fam.type) {
                case METRIC_TYPE_GAUGE:
                    m.value = VALUE_GAUGE(mm->value.gauge.float64);
                    break;
                case METRIC_TYPE_COUNTER:
                    m.value = VALUE_COUNTER(mm->value.counter.uint64);
                    break;
                default:
                    PLUGIN_WARNING("unsupported metric type in match");
                    continue;
                    break;
                }

                label_set_clone(&m.label, *labels);
                label_set_add_set(&m.label, true, mm->label);

                metric_family_metric_append(&fam, m);

                label_set_reset(&m.label);

                if (reset)
                    match_metric_value_reset(mm, mfam->type);
            }
            c_avl_iterator_destroy(miter);

            plugin_dispatch_metric_family_filtered(&fam, filter, 0);

        }
        c_avl_iterator_destroy(fiter);
        pm = pm->next;
    }

    return 0;
}
