// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2019-2020 Google LLC
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "log.h"
#include "libutils/pack.h"
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

int metric_family_append_va(metric_family_t *fam, value_t v, label_set_t *labels, va_list ap)
{
    if (fam == NULL)
        return EINVAL;

    metric_t m = { 0 };
    if (labels != NULL) {
        int status = label_set_clone(&m.label, *labels);
        if (status != 0)
            return status;
    }

    va_list apc;
    va_copy(apc, ap);
    label_pair_t *pair;

    while ((pair = va_arg(apc, label_pair_t *)) != NULL) {
        if ((pair->name != NULL) && (pair->value != NULL)) {
            int status = label_set_add(&m.label, true, pair->name, pair->value);
            if (status != 0) {
                metric_reset(&m, fam->type);
                va_end(apc);
                return status;
            }
        }
    }

    va_end(apc);

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

__attribute__ ((sentinel(0)))
int metric_family_append(metric_family_t *fam, value_t v, label_set_t *labels, ...)
{
    va_list ap;

    va_start(ap, labels);

    int status = metric_family_append_va(fam, v, labels, ap);

    va_end(ap);

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

enum {
    METRIC_LABEL_ID           = 0x10,
    METRIC_TIME_ID            = 0x20,
    METRIC_INTERVAL_ID        = 0x30,
    METRIC_UNKNOWN_ID         = 0x40,
    METRIC_GAUGE_ID           = 0x50,
    METRIC_COUNTER_ID         = 0x60,
    METRIC_STATE_SET_ID       = 0x70,
    METRIC_INFO_ID            = 0x80,
    METRIC_SUMMARY_ID         = 0x90,
    METRIC_HISTOGRAM_ID       = 0xa0,
    METRIC_GAUGE_HISTOGRAM_ID = 0xb0,
};

static int metric_pack(buf_t *buf, metric_type_t type, metric_t *m)
{
    size_t begin = 0;
    int status = pack_block_begin(buf, &begin);

    status |= label_set_pack(buf, METRIC_LABEL_ID, &m->label);
    status |= pack_uint64(buf, METRIC_TIME_ID, m->time);
    status |= pack_uint64(buf, METRIC_INTERVAL_ID, m->interval);

    switch(type) {
    case METRIC_TYPE_UNKNOWN:
        if (m->value.unknown.type ==  UNKNOWN_FLOAT64) {
            status |= pack_double(buf, METRIC_UNKNOWN_ID | 0x01, m->value.unknown.float64);
        } else {
            status |= pack_int64(buf, METRIC_UNKNOWN_ID | 0x02, m->value.unknown.int64);
        }
        break;
    case METRIC_TYPE_GAUGE:
        if (m->value.gauge.type == GAUGE_FLOAT64) {
            status |= pack_double(buf, METRIC_GAUGE_ID | 0x01, m->value.gauge.float64);
        } else {
            status |= pack_int64(buf, METRIC_GAUGE_ID | 0x02, m->value.gauge.int64);
        }
        break;
    case METRIC_TYPE_COUNTER:
        if (m->value.counter.type == COUNTER_UINT64) {
            status |= pack_uint64(buf, METRIC_COUNTER_ID | 0x01, m->value.counter.uint64);
        } else {
            status |= pack_double(buf, METRIC_COUNTER_ID | 0x02, m->value.counter.float64);
        }
        break;
    case METRIC_TYPE_STATE_SET:
        status |= state_set_pack(buf, METRIC_STATE_SET_ID, &m->value.state_set);
        break;
    case METRIC_TYPE_INFO:
        status |= label_set_pack(buf, METRIC_INFO_ID, &m->value.info);
        break;
    case METRIC_TYPE_SUMMARY:
        status |= summary_pack(buf, METRIC_SUMMARY_ID, m->value.summary);
        break;
    case METRIC_TYPE_HISTOGRAM:
        status |= histogram_pack(buf, METRIC_HISTOGRAM_ID, m->value.histogram);
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        status |= histogram_pack(buf, METRIC_GAUGE_HISTOGRAM_ID, m->value.histogram);
        break;
    }

    status |= pack_block_end(buf, begin);

    return status;
}

static int metric_unpack(rbuf_t *rbuf, metric_type_t type, metric_list_t *metrics)
{
    rbuf_t srbuf = {0};
    int status = unpack_block(rbuf, &srbuf);
    if (status != 0) {
        return -1;
    }

    metric_t *tmp = realloc(metrics->ptr, sizeof(*metrics->ptr) * (metrics->num + 1));
    if (tmp == NULL)
        return -1;
    metrics->ptr = tmp;

    memset(&metrics->ptr[metrics->num], 0, sizeof(*metrics->ptr));

    metric_t *m = &metrics->ptr[metrics->num];

    while (true) {
        uint8_t id = 0;

        status = unpack_id(&srbuf, &id);

        switch (id & 0xf0) {
        case METRIC_LABEL_ID:
            status = label_set_unpack(&srbuf, id, &m->label);
            break;
        case METRIC_TIME_ID:
            status = unpack_uint64(&srbuf, &m->time);
            break;
        case METRIC_INTERVAL_ID:
            status = unpack_uint64(&srbuf, &m->interval);
            break;
        case METRIC_UNKNOWN_ID:
            if (likely(type == METRIC_TYPE_UNKNOWN)) {
                if ((id & 0x0f) == 0x01) {
                    double value = 0;
                    status = unpack_double(&srbuf, &value);
                    m->value = VALUE_UNKNOWN_FLOAT64(value);
                } else if ((id & 0x0f) == 0x02) {
                    int64_t value = 0;
                    status = unpack_int64(&srbuf, &value);
                    m->value = VALUE_UNKNOWN_INT64(value);
                } else {
                    status = 1;
                }
            } else {
                status = 1;
            }
            break;
        case METRIC_GAUGE_ID:
            if (likely(type == METRIC_TYPE_GAUGE)) {
                if ((id & 0x0f) == 0x01) {
                    double value = 0;
                    status = unpack_double(&srbuf, &value);
                    m->value = VALUE_GAUGE_FLOAT64(value);
                } else if ((id & 0x0f) == 0x02) {
                    int64_t value = 0;
                    status = unpack_int64(&srbuf, &value);
                    m->value = VALUE_GAUGE_INT64(value);
                } else {
                    status = 1;
                }
            } else {
                status = 1;
            }
            break;
        case METRIC_COUNTER_ID:
            if (likely(type == METRIC_TYPE_COUNTER)) {
                if ((id & 0x0f) == 0x01) {
                    uint64_t value = 0;
                    status = unpack_uint64(&srbuf, &value);
                    m->value = VALUE_COUNTER_UINT64(value);
                } else if ((id & 0x0f) == 0x02) {
                    double value = 0;
                    status = unpack_double(&srbuf, &value);
                    m->value = VALUE_COUNTER_FLOAT64(value);
                } else {
                    status = 1;
                }
            } else {
                status = 1;
            }
            break;
        case METRIC_STATE_SET_ID:
            if (likely(type == METRIC_TYPE_STATE_SET)) {
                status = state_set_unpack(&srbuf, id, &m->value.state_set);
            } else {
                status = 1;
            }
            break;
        case METRIC_INFO_ID:
            if (likely(type == METRIC_TYPE_INFO)) {
                status = label_set_unpack(&srbuf, id, &m->value.info);
            } else {
                status = 1;
            }
            break;
        case METRIC_SUMMARY_ID:
            if (likely(type == METRIC_TYPE_SUMMARY)) {
                status = summary_unpack(&srbuf, &m->value.summary);
            } else {
                status = 1;
            }
            break;
        case METRIC_HISTOGRAM_ID:
            if (likely(type == METRIC_TYPE_HISTOGRAM)) {
                status = histogram_unpack(&srbuf, &m->value.histogram);
            } else {
                status = 1;
            }
            break;
        case METRIC_GAUGE_HISTOGRAM_ID:
            if (likely(type == METRIC_TYPE_GAUGE_HISTOGRAM)) {
                status = histogram_unpack(&srbuf, &m->value.histogram);
            } else {
                status = 1;
            }
            break;
        }

        if (status != 0)
            break;

        if (rbuf_remain(&srbuf) == 0)
            break;
    }

    if (status != 0)
        return -1;

    metrics->num++;

    unpack_avance_block(rbuf, &srbuf);

    return 0;
}

static int metric_list_unpack(rbuf_t *rbuf, uint8_t id, metric_type_t type, metric_list_t *metrics)
{
    size_t len = 0;
    int status = unpack_size(rbuf, id, &len);
    if (status != 0)
        return status;

    for (size_t i = 0; i < len; i++) {
        status = metric_unpack(rbuf, type, metrics);
        if (status != 0)
            break;
    }

    return status;
}

enum {
    METRIC_FAMILY_NAME_ID        = 0x10,
    METRIC_FAMILY_HELP_ID        = 0x20,
    METRIC_FAMILY_UNIT_ID        = 0x30,
    METRIC_FAMILY_TYPE_ID        = 0x40,
    METRIC_FAMILY_METRIC_LIST_ID = 0x50
};

int metric_family_pack(buf_t *buf, metric_family_t *fam)
{
    if (fam == NULL)
        return 0;

    size_t begin = 0;
    int status = pack_block_begin(buf, &begin);
    status |= pack_string(buf, METRIC_FAMILY_NAME_ID, fam->name);
    if (fam->help != NULL)
        status |= pack_string(buf, METRIC_FAMILY_HELP_ID, fam->help);
    if (fam->unit != NULL)
        status |= pack_string(buf, METRIC_FAMILY_UNIT_ID, fam->unit);
    status |= pack_uint8(buf, METRIC_FAMILY_TYPE_ID, fam->type);

    if (fam->metric.num > 0) {
        status |= pack_size(buf, METRIC_FAMILY_METRIC_LIST_ID, fam->metric.num);
        for (size_t i = 0; i < fam->metric.num; i++) {
            metric_t *m = &fam->metric.ptr[i];
            status |= metric_pack(buf, fam->type, m);
        }
    }

    status |= pack_block_end(buf, begin);
    return status;
}

metric_family_t *metric_family_unpack(rbuf_t *rbuf)
{
    rbuf_t srbuf = {0};
    int status = unpack_block(rbuf, &srbuf);
    if (status != 0) {
        return NULL;
    }

    metric_family_t *fam = calloc(1, sizeof(*fam));
    if (fam == NULL) {
        ERROR("calloc failed.");
        return NULL;
    }

    while (true) {
        uint8_t id = 0;

        status |= unpack_id(&srbuf, &id);

        switch (id & 0xf0) {
        case METRIC_FAMILY_NAME_ID:
            status |= unpack_string(&srbuf, id, &fam->name);
            break;
        case METRIC_FAMILY_HELP_ID:
            status |= unpack_string(&srbuf, id, &fam->help);
            break;
        case METRIC_FAMILY_UNIT_ID:
            status |= unpack_string(&srbuf, id, &fam->unit);
            break;
        case METRIC_FAMILY_TYPE_ID: {
            uint8_t type = 0;
            status |= unpack_uint8(&srbuf, &type);
            fam->type = type;
        }   break;
        case METRIC_FAMILY_METRIC_LIST_ID:
            status |= metric_list_unpack(&srbuf, id, fam->type, &fam->metric);
            break;
        }

        if (status != 0)
            break;

        if (rbuf_remain(&srbuf) == 0)
            break;
    }

    if (status != 0) {
        metric_family_free(fam);
        return NULL;
    }

    unpack_avance_block(rbuf, &srbuf);

    return fam;
}
