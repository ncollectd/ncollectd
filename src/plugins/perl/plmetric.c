// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "cperl.h"

#include "libutils/common.h"


#define CPL_METRIC_MEMBERS                                         \
    { "labels",   CPL_MEMBER_HASH,   0                       },    \
    { "time",     CPL_MEMBER_DOUBLE, CPL_MEMBER_REPR_NONZERO },    \
    { "interval", CPL_MEMBER_DOUBLE, CPL_MEMBER_REPR_NONZERO }

static const cpl_member_t cpl_metric_members[] = {
    CPL_METRIC_MEMBERS
};

static const cpl_member_t cpl_metric_double_members[] = {
    { "value", CPL_MEMBER_DOUBLE, CPL_MEMBER_REQUIRED },
    CPL_METRIC_MEMBERS
};

static const cpl_member_t cpl_metric_long_members[] = {
    { "value", CPL_MEMBER_INT64, CPL_MEMBER_REQUIRED },
    CPL_METRIC_MEMBERS
};

static const cpl_member_t cpl_metric_ulong_members[] = {
    { "value", CPL_MEMBER_UINT64, CPL_MEMBER_REQUIRED },
    CPL_METRIC_MEMBERS
};

static const cpl_member_t cpl_metric_state_set_members[] = {
    { "set", CPL_MEMBER_HASH, CPL_MEMBER_REQUIRED },
    CPL_METRIC_MEMBERS
};

static const cpl_member_t cpl_metric_info_members[] = {
    { "info", CPL_MEMBER_HASH, CPL_MEMBER_REQUIRED },
    CPL_METRIC_MEMBERS
};

static const cpl_member_t cpl_metric_summary_members[] = {
    { "sum",       CPL_MEMBER_DOUBLE, CPL_MEMBER_REQUIRED },
    { "count",     CPL_MEMBER_UINT64, CPL_MEMBER_REQUIRED },
    { "quantiles", CPL_MEMBER_ARRAY,  CPL_MEMBER_REQUIRED },
    CPL_METRIC_MEMBERS
};

static const cpl_member_t cpl_metric_histogram_members[] = {
    { "sum",     CPL_MEMBER_DOUBLE, CPL_MEMBER_REQUIRED },
    { "buckets", CPL_MEMBER_ARRAY,  CPL_MEMBER_REQUIRED },
    CPL_METRIC_MEMBERS
};

#define CPL_METRIC_CLASS(n, u, m, own)        \
    {                                         \
        .name        = (n),                   \
        .base        = CPL_CLASS_METRIC,      \
        .usage       = (u),                   \
        .members     = (m),                   \
        .members_num = STATIC_ARRAY_SIZE(m),  \
        .own_members = (own),                 \
    }

#define CPL_METRIC_USAGE "[, labels][, time][, interval]"

static const cpl_class_t cpl_metric_classes[] = {
    {
        .name        = CPL_CLASS_METRIC,
        .base        = NULL,
        .usage       = "[labels][, time][, interval]",
        .members     = cpl_metric_members,
        .members_num = STATIC_ARRAY_SIZE(cpl_metric_members),
        .own_members = STATIC_ARRAY_SIZE(cpl_metric_members),
    },
    CPL_METRIC_CLASS(CPL_CLASS_METRIC_UNKNOWN_DOUBLE, "value" CPL_METRIC_USAGE,
                     cpl_metric_double_members, 1),
    CPL_METRIC_CLASS(CPL_CLASS_METRIC_UNKNOWN_LONG, "value" CPL_METRIC_USAGE,
                     cpl_metric_long_members, 1),
    CPL_METRIC_CLASS(CPL_CLASS_METRIC_GAUGE_DOUBLE, "value" CPL_METRIC_USAGE,
                     cpl_metric_double_members, 1),
    CPL_METRIC_CLASS(CPL_CLASS_METRIC_GAUGE_LONG, "value" CPL_METRIC_USAGE,
                     cpl_metric_long_members, 1),
    CPL_METRIC_CLASS(CPL_CLASS_METRIC_COUNTER_ULONG, "value" CPL_METRIC_USAGE,
                     cpl_metric_ulong_members, 1),
    CPL_METRIC_CLASS(CPL_CLASS_METRIC_COUNTER_DOUBLE, "value" CPL_METRIC_USAGE,
                     cpl_metric_double_members, 1),
    CPL_METRIC_CLASS(CPL_CLASS_METRIC_STATE_SET, "set" CPL_METRIC_USAGE,
                     cpl_metric_state_set_members, 1),
    CPL_METRIC_CLASS(CPL_CLASS_METRIC_INFO, "info" CPL_METRIC_USAGE,
                     cpl_metric_info_members, 1),
    CPL_METRIC_CLASS(CPL_CLASS_METRIC_SUMMARY, "sum, count, quantiles" CPL_METRIC_USAGE,
                     cpl_metric_summary_members, 3),
    CPL_METRIC_CLASS(CPL_CLASS_METRIC_HISTOGRAM, "sum, buckets" CPL_METRIC_USAGE,
                     cpl_metric_histogram_members, 2),
    CPL_METRIC_CLASS(CPL_CLASS_METRIC_GAUGE_HISTOGRAM, "sum, buckets" CPL_METRIC_USAGE,
                     cpl_metric_histogram_members, 2),
};

void cpl_metric_boot(pTHX)
{
    for (size_t i = 0; i < STATIC_ARRAY_SIZE(cpl_metric_classes); i++)
        cpl_class_boot(aTHX_ &cpl_metric_classes[i]);
}

static SV *cpl_buckets_to_sv(pTHX_ const histogram_t *h)
{
    AV *buckets = newAV();

    for (size_t i = 0; (h != NULL) && (i < h->num); i++) {
        AV *bucket = newAV();
        av_push(bucket, newSVuv((UV)h->buckets[i].counter));
        av_push(bucket, newSVnv(h->buckets[i].maximum));
        av_push(buckets, newRV_noinc((SV *)bucket));
    }

    return newRV_noinc((SV *)buckets);
}

static SV *cpl_quantiles_to_sv(pTHX_ const summary_t *s)
{
    AV *quantiles = newAV();

    for (size_t i = 0; (s != NULL) && (i < s->num); i++) {
        AV *quantile = newAV();
        av_push(quantile, newSVnv(s->quantiles[i].quantile));
        av_push(quantile, newSVnv(s->quantiles[i].value));
        av_push(quantiles, newRV_noinc((SV *)quantile));
    }

    return newRV_noinc((SV *)quantiles);
}

SV *cpl_metric_to_sv(pTHX_ metric_type_t type, const metric_t *m)
{
    HV *hv = NULL;
    SV *self = NULL;

    switch (type) {
    case METRIC_TYPE_UNKNOWN:
        if (m->value.unknown.type == UNKNOWN_INT64) {
            self = cpl_object_new(aTHX_ CPL_CLASS_METRIC_UNKNOWN_LONG, &hv);
            cpl_object_set(aTHX_ hv, "value", newSViv((IV)m->value.unknown.int64));
        } else {
            self = cpl_object_new(aTHX_ CPL_CLASS_METRIC_UNKNOWN_DOUBLE, &hv);
            cpl_object_set(aTHX_ hv, "value", newSVnv(m->value.unknown.float64));
        }
        break;
    case METRIC_TYPE_GAUGE:
        if (m->value.gauge.type == GAUGE_INT64) {
            self = cpl_object_new(aTHX_ CPL_CLASS_METRIC_GAUGE_LONG, &hv);
            cpl_object_set(aTHX_ hv, "value", newSViv((IV)m->value.gauge.int64));
        } else {
            self = cpl_object_new(aTHX_ CPL_CLASS_METRIC_GAUGE_DOUBLE, &hv);
            cpl_object_set(aTHX_ hv, "value", newSVnv(m->value.gauge.float64));
        }
        break;
    case METRIC_TYPE_COUNTER:
        if (m->value.counter.type == COUNTER_FLOAT64) {
            self = cpl_object_new(aTHX_ CPL_CLASS_METRIC_COUNTER_DOUBLE, &hv);
            cpl_object_set(aTHX_ hv, "value", newSVnv(m->value.counter.float64));
        } else {
            self = cpl_object_new(aTHX_ CPL_CLASS_METRIC_COUNTER_ULONG, &hv);
            cpl_object_set(aTHX_ hv, "value", newSVuv((UV)m->value.counter.uint64));
        }
        break;
    case METRIC_TYPE_STATE_SET: {
        self = cpl_object_new(aTHX_ CPL_CLASS_METRIC_STATE_SET, &hv);
        HV *set = newHV();
        for (size_t i = 0; i < m->value.state_set.num; i++) {
            const state_t *state = &m->value.state_set.ptr[i];
            (void)hv_store(set, state->name, (I32)strlen(state->name),
                           newSVsv(boolSV(state->enabled)), 0);
        }
        cpl_object_set(aTHX_ hv, "set", newRV_noinc((SV *)set));
    }   break;
    case METRIC_TYPE_INFO:
        self = cpl_object_new(aTHX_ CPL_CLASS_METRIC_INFO, &hv);
        cpl_object_set(aTHX_ hv, "info", cpl_labels_to_sv(aTHX_ &m->value.info));
        break;
    case METRIC_TYPE_SUMMARY: {
        const summary_t *s = m->value.summary;
        self = cpl_object_new(aTHX_ CPL_CLASS_METRIC_SUMMARY, &hv);
        cpl_object_set(aTHX_ hv, "sum", newSVnv(s != NULL ? s->sum : 0.0));
        cpl_object_set(aTHX_ hv, "count", newSVuv(s != NULL ? (UV)s->count : 0));
        cpl_object_set(aTHX_ hv, "quantiles", cpl_quantiles_to_sv(aTHX_ s));
    }   break;
    case METRIC_TYPE_HISTOGRAM:
    case METRIC_TYPE_GAUGE_HISTOGRAM: {
        const histogram_t *h = m->value.histogram;
        self = cpl_object_new(aTHX_ type == METRIC_TYPE_HISTOGRAM ? CPL_CLASS_METRIC_HISTOGRAM
                                                                  : CPL_CLASS_METRIC_GAUGE_HISTOGRAM,
                              &hv);
        cpl_object_set(aTHX_ hv, "sum", newSVnv(h != NULL ? h->sum : 0.0));
        cpl_object_set(aTHX_ hv, "buckets", cpl_buckets_to_sv(aTHX_ h));
    }   break;
    }

    if (self == NULL)
        return newSV(0);

    cpl_object_set(aTHX_ hv, "labels", cpl_labels_to_sv(aTHX_ &m->label));
    cpl_object_set(aTHX_ hv, "time", newSVnv(CDTIME_T_TO_DOUBLE(m->time)));
    cpl_object_set(aTHX_ hv, "interval", newSVnv(CDTIME_T_TO_DOUBLE(m->interval)));

    return self;
}

bool cpl_metric_check_type(pTHX_ metric_type_t type, SV *sv)
{
    if (!sv_isobject(sv) || (SvTYPE(SvRV(sv)) != SVt_PVHV))
        return false;

    switch (type) {
    case METRIC_TYPE_UNKNOWN:
        return sv_derived_from(sv, CPL_CLASS_METRIC_UNKNOWN_DOUBLE) ||
               sv_derived_from(sv, CPL_CLASS_METRIC_UNKNOWN_LONG);
    case METRIC_TYPE_GAUGE:
        return sv_derived_from(sv, CPL_CLASS_METRIC_GAUGE_DOUBLE) ||
               sv_derived_from(sv, CPL_CLASS_METRIC_GAUGE_LONG);
    case METRIC_TYPE_COUNTER:
        return sv_derived_from(sv, CPL_CLASS_METRIC_COUNTER_ULONG) ||
               sv_derived_from(sv, CPL_CLASS_METRIC_COUNTER_DOUBLE);
    case METRIC_TYPE_STATE_SET:
        return sv_derived_from(sv, CPL_CLASS_METRIC_STATE_SET);
    case METRIC_TYPE_INFO:
        return sv_derived_from(sv, CPL_CLASS_METRIC_INFO);
    case METRIC_TYPE_SUMMARY:
        return sv_derived_from(sv, CPL_CLASS_METRIC_SUMMARY);
    case METRIC_TYPE_HISTOGRAM:
        return sv_derived_from(sv, CPL_CLASS_METRIC_HISTOGRAM);
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        return sv_derived_from(sv, CPL_CLASS_METRIC_GAUGE_HISTOGRAM);
    }

    return false;
}

static SV *cpl_metric_required(pTHX_ HV *hv, const char *key)
{
    SV *sv = cpl_object_get(aTHX_ hv, key);
    if (sv == NULL)
        croak("%s is required", key);
    return sv;
}

static cdtime_t cpl_metric_time(pTHX_ HV *hv, const char *key)
{
    SV *sv = cpl_object_get(aTHX_ hv, key);
    if (sv == NULL)
        return 0;

    double value = cpl_sv_to_double(aTHX_ sv, key);
    if (value < 0)
        croak("%s must be a positive number", key);

    return DOUBLE_TO_CDTIME_T(value);
}

/* Converts the perl metric "sv" to "m". The memory allocated in "m" is not
 * freed if this croaks, the caller has to reset it. */
void cpl_metric_build(pTHX_ metric_type_t type, SV *sv, metric_t *m)
{
    if (!cpl_metric_check_type(aTHX_ type, sv)) {
        const char *class = sv_isobject(sv) ? HvNAME(SvSTASH(SvRV(sv))) : NULL;
        croak("%s can not be added to a metric family of type %s",
              class != NULL ? class : "a value which is not a metric",
              cpl_metric_type_name(type));
    }

    HV *hv = (HV *)SvRV(sv);

    switch (type) {
    case METRIC_TYPE_UNKNOWN:
        if (sv_derived_from(sv, CPL_CLASS_METRIC_UNKNOWN_LONG))
            m->value = VALUE_UNKNOWN_INT64(cpl_sv_to_int64(aTHX_
                                           cpl_metric_required(aTHX_ hv, "value"), "value"));
        else
            m->value = VALUE_UNKNOWN_FLOAT64(cpl_sv_to_double(aTHX_
                                             cpl_metric_required(aTHX_ hv, "value"), "value"));
        break;
    case METRIC_TYPE_GAUGE:
        if (sv_derived_from(sv, CPL_CLASS_METRIC_GAUGE_LONG))
            m->value = VALUE_GAUGE_INT64(cpl_sv_to_int64(aTHX_
                                         cpl_metric_required(aTHX_ hv, "value"), "value"));
        else
            m->value = VALUE_GAUGE_FLOAT64(cpl_sv_to_double(aTHX_
                                           cpl_metric_required(aTHX_ hv, "value"), "value"));
        break;
    case METRIC_TYPE_COUNTER:
        if (sv_derived_from(sv, CPL_CLASS_METRIC_COUNTER_DOUBLE))
            m->value = VALUE_COUNTER_FLOAT64(cpl_sv_to_double(aTHX_
                                             cpl_metric_required(aTHX_ hv, "value"), "value"));
        else
            m->value = VALUE_COUNTER_UINT64(cpl_sv_to_uint64(aTHX_
                                            cpl_metric_required(aTHX_ hv, "value"), "value"));
        break;
    case METRIC_TYPE_STATE_SET:
        cpl_build_state_set(aTHX_ cpl_object_get(aTHX_ hv, "set"), &m->value.state_set);
        break;
    case METRIC_TYPE_INFO:
        cpl_build_labels(aTHX_ cpl_object_get(aTHX_ hv, "info"), &m->value.info, "info");
        break;
    case METRIC_TYPE_SUMMARY:
        cpl_build_summary(aTHX_ cpl_metric_required(aTHX_ hv, "quantiles"), &m->value.summary);
        m->value.summary->sum = cpl_sv_to_double(aTHX_ cpl_metric_required(aTHX_ hv, "sum"),
                                                 "sum");
        m->value.summary->count = cpl_sv_to_uint64(aTHX_ cpl_metric_required(aTHX_ hv, "count"),
                                                   "count");
        break;
    case METRIC_TYPE_HISTOGRAM:
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        cpl_build_histogram(aTHX_ cpl_metric_required(aTHX_ hv, "buckets"), &m->value.histogram);
        m->value.histogram->sum = cpl_sv_to_double(aTHX_ cpl_metric_required(aTHX_ hv, "sum"),
                                                   "sum");
        break;
    }

    m->time = cpl_metric_time(aTHX_ hv, "time");
    m->interval = cpl_metric_time(aTHX_ hv, "interval");

    cpl_build_labels(aTHX_ cpl_object_get(aTHX_ hv, "labels"), &m->label, "labels");
}
