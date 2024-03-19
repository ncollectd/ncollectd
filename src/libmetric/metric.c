// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2019-2020 Google LLC
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "log.h"
#include "libmetric/metric.h"
#include "libmetric/metric_chars.h"

const char *metric_type_str (metric_type_t type)
{
    switch(type) {
    case METRIC_TYPE_UNKNOWN:
        return "unknown";
    case METRIC_TYPE_GAUGE:
        return "gauge";
    case METRIC_TYPE_COUNTER:
        return "counter";
    case METRIC_TYPE_STATE_SET:
        return "stateset";
    case METRIC_TYPE_INFO:
        return "info";
    case METRIC_TYPE_SUMMARY:
        return "summary";
    case METRIC_TYPE_HISTOGRAM:
        return "histogram";
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        return "gaugehistogram";
    }
    return NULL;
}

int metric_reset(metric_t *m, metric_type_t type)
{
    if (m == NULL)
        return EINVAL;

    label_set_reset(&m->label);

    switch(type) {
    case METRIC_TYPE_UNKNOWN:
    case METRIC_TYPE_GAUGE:
    case METRIC_TYPE_COUNTER:
        break;
    case METRIC_TYPE_STATE_SET:
        state_set_reset(&m->value.state_set);
        break;
    case METRIC_TYPE_INFO:
        label_set_reset(&m->value.info);
        break;
    case METRIC_TYPE_SUMMARY:
        summary_destroy(m->value.summary);
        break;
    case METRIC_TYPE_HISTOGRAM:
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        histogram_destroy(m->value.histogram);
        break;
    }

    memset(m, 0, sizeof(*m));
    return 0;
}

int metric_label_set(metric_t *m, char const *name, char const *value)
{
    if ((m == NULL) || (name == NULL))
        return EINVAL;

    return label_set_add(&m->label, true, name, value);
}

char const *metric_label_get(metric_t const *m, char const *name)
{
    if ((m == NULL) || (name == NULL)) {
        errno = EINVAL;
        return NULL;
    }

    label_pair_t *set = label_set_read(m->label, name);
    if (set == NULL)
        return NULL;

    return set->value;
}

int metric_list_add(metric_list_t *metrics, metric_t m, metric_type_t type)
{
    if (metrics == NULL)
        return EINVAL;

    metric_t *tmp = realloc(metrics->ptr, sizeof(*metrics->ptr) * (metrics->num + 1));
    if (tmp == NULL)
        return errno;
    metrics->ptr = tmp;

    memset(&metrics->ptr[metrics->num], 0, sizeof(*metrics->ptr));

    metric_t *nm = &metrics->ptr[metrics->num];
    nm->time = m.time;
    nm->interval = m.interval;

    int status = label_set_clone(&nm->label, m.label);
    if (status != 0) {
        metric_reset(nm, type);
        return status;
    }

    switch (type) {
    case METRIC_TYPE_UNKNOWN:
    case METRIC_TYPE_GAUGE:
    case METRIC_TYPE_COUNTER:
        nm->value = m.value;
        break;
    case METRIC_TYPE_STATE_SET:
        status = state_set_clone(&nm->value.state_set, m.value.state_set);
        break;
    case METRIC_TYPE_INFO:
        status = label_set_clone(&nm->value.info, m.value.info);
        break;
    case METRIC_TYPE_SUMMARY:
        nm->value.summary = summary_clone(m.value.summary);
        if (nm->value.summary == NULL)
            status = ENOMEM;
        break;
    case METRIC_TYPE_HISTOGRAM:
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        nm->value.histogram = histogram_clone(m.value.histogram);
        if (nm->value.histogram == NULL)
            status = ENOMEM;
        break;
    }

    if (status != 0) {
        metric_reset(nm, type);
        return status;
    }

    metrics->num++;
    return 0;
}

int metric_list_append(metric_list_t *metrics, metric_t m)
{
    if (metrics == NULL)
        return EINVAL;

    metric_t *tmp = realloc(metrics->ptr, sizeof(*metrics->ptr) * (metrics->num + 1));
    if (tmp == NULL) {
        ERROR("realloc failed.");
        return -1;
    }
    metrics->ptr = tmp;

    memset(&metrics->ptr[metrics->num], 0, sizeof(*metrics->ptr));

    metric_t *nm = &metrics->ptr[metrics->num];
    nm->value = m.value;
    nm->time = m.time;
    nm->interval = m.interval;
    nm->label = m.label;

    metrics->num++;
    return 0;
}

void metric_list_reset(metric_list_t *metrics, metric_type_t type)
{
    if (metrics == NULL)
        return;

    for (size_t i = 0; i < metrics->num; i++) {
        metric_reset(metrics->ptr + i, type);
    }
    free(metrics->ptr);

    metrics->ptr = NULL;
    metrics->num = 0;
}

int metric_list_clone(metric_list_t *dest, metric_list_t src, metric_family_t *fam)
{
    if (src.num == 0)
        return 0;

    dest->ptr = calloc(src.num, sizeof(*(dest->ptr)));
    if (dest->ptr == NULL)
        return ENOMEM;

    dest->num = src.num;

    for (size_t i = 0; i < src.num; i++) {
        dest->ptr[i] = (metric_t){
                .time = src.ptr[i].time,
                .interval = src.ptr[i].interval,
        };

        int status = 0;

        switch (fam->type) {
        case METRIC_TYPE_UNKNOWN:
        case METRIC_TYPE_GAUGE:
        case METRIC_TYPE_COUNTER:
            dest->ptr[i].value = src.ptr[i].value;
            break;
        case METRIC_TYPE_STATE_SET:
            status = state_set_clone(&dest->ptr[i].value.state_set, src.ptr[i].value.state_set);
            break;
        case METRIC_TYPE_INFO:
            status = label_set_clone(&dest->ptr[i].value.info, src.ptr[i].value.info);
            break;
        case METRIC_TYPE_SUMMARY:
            dest->ptr[i].value.summary = summary_clone(src.ptr[i].value.summary);
            if (dest->ptr[i].value.summary == NULL)
                status = ENOMEM;
            break;
        case METRIC_TYPE_HISTOGRAM:
        case METRIC_TYPE_GAUGE_HISTOGRAM:
            dest->ptr[i].value.histogram = histogram_clone(src.ptr[i].value.histogram);
            if (dest->ptr[i].value.histogram == NULL)
                status = ENOMEM;
            break;
        }

        if (status != 0) {
            metric_list_reset(dest, fam->type);
            return status;
        }

        status = label_set_clone(&dest->ptr[i].label, src.ptr[i].label);
        if (status != 0) {
            metric_list_reset(dest, fam->type);
            return status;
        }
    }

    return 0;
}

int metric_value_clone(value_t *dst, value_t src, metric_type_t type)
{
    int status = 0;

    switch (type) {
    case METRIC_TYPE_UNKNOWN:
        dst->unknown = src.unknown;
        break;
    case METRIC_TYPE_GAUGE:
        dst->gauge = src.gauge;
        break;
    case METRIC_TYPE_COUNTER:
        dst->counter = src.counter;
        break;
    case METRIC_TYPE_STATE_SET:
        status = state_set_clone(&dst->state_set, src.state_set);
        break;
    case METRIC_TYPE_INFO:
        status = label_set_clone(&dst->info, src.info);
        break;
    case METRIC_TYPE_SUMMARY:
        dst->summary = summary_clone(src.summary);
        if (dst->summary == NULL)
            status = ENOMEM;
        break;
    case METRIC_TYPE_HISTOGRAM:
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        dst->histogram = histogram_clone(src.histogram);
        if (dst->histogram == NULL)
            status = ENOMEM;
        break;
    }

    return status;
}

int metric_family_metric_append(metric_family_t *fam, metric_t m)
{
    if (fam == NULL)
        return EINVAL;
    return metric_list_add(&fam->metric, m, fam->type);
}

__attribute__ ((sentinel(0)))
int metric_family_append(metric_family_t *fam, value_t v, label_set_t *labels, ...)
{
    if (fam == NULL)
        return EINVAL;

    metric_t m = { 0 };
    if (labels != NULL) {
        int status = label_set_clone(&m.label, *labels);
        if (status != 0)
            return status;
    }

    va_list ap;
    va_start(ap, labels);
    label_pair_t *pair;

    while ((pair = va_arg(ap, label_pair_t *)) != NULL) {
        if ((pair->name != NULL) && (pair->value != NULL)) {
            int status = label_set_add(&m.label, true, pair->name, pair->value);
            if (status != 0) {
                metric_reset(&m, fam->type);
                va_end(ap);
                return status;
            }
        }
    }

    va_end(ap);

    int status = metric_value_clone(&m.value, v, fam->type);
    if (status != 0) {
        metric_reset(&m, fam->type);
        return status;
    }

    status = metric_list_append(&fam->metric, m);
    if (status != 0) {
        metric_reset(&m, fam->type);
        return status;
    }

    return status;
}

int metric_family_metric_reset(metric_family_t *fam)
{
    if (fam == NULL)
        return EINVAL;

    metric_list_reset(&fam->metric, fam->type);
    return 0;
}

void metric_family_free(metric_family_t *fam)
{
    if (fam == NULL)
        return;

    free(fam->name);
    free(fam->help);
    free(fam->unit);
    metric_list_reset(&fam->metric, fam->type);
    free(fam);
}

metric_family_t *metric_family_clone(metric_family_t const *fam)
{
    if (fam == NULL) {
        errno = EINVAL;
        return NULL;
    }

    metric_family_t *ret = calloc(1, sizeof(*ret));
    if (ret == NULL)
        return NULL;

    ret->name = strdup(fam->name);
    if (fam->help != NULL)
        ret->help = strdup(fam->help);
    if (fam->unit != NULL)
        ret->unit = strdup(fam->unit);
    ret->type = fam->type;

    if (fam->metric.num > 0) {
        int status = metric_list_clone(&ret->metric, fam->metric, ret);
        if (status != 0) {
            metric_family_free(ret);
            errno = status;
            return NULL;
        }
    }

    return ret;
}

int metric_family_list_alloc(metric_family_list_t *faml, size_t num)
{
    if (faml->fixed)
        return -1;

    metric_family_t **ptr = calloc(num, sizeof(*ptr));
    if (ptr == NULL) {
        ERROR("calloc failed.");
        return -1;
    }

    faml->ptr = ptr;
    faml->size = num;
    faml->pos = 0;

    return 0;
}

void metric_family_list_reset(metric_family_list_t *faml)
{
    if (faml == NULL)
        return;

    for (size_t i=0; i < faml->pos; i++) {
        if (faml->ptr[i] != NULL)
            metric_family_free(faml->ptr[i]);
    }

    if (!faml->fixed)
        free(faml->ptr);
}
