// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2009  Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008  Justo Alonso Achaques
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Justo Alonso Achaques <justo.alonso at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include <jni.h>

#pragma GCC diagnostic ignored "-Wpedantic"

#if !defined(JNI_VERSION_1_2)
#error "Need JNI 1.2 compatible interface!"
#endif

#include "jutil.h"

extern jcached_refs_t gref;

static jobject ctoj_metric_histogram(JNIEnv *jvm_env, const metric_t *m)
{
    jobject o_list = (*jvm_env)->NewObject(jvm_env, gref.array_list.class,
                                                    gref.array_list.constructor,
                                                    (jint)m->value.histogram->num);
    if (o_list == NULL) {
        PLUGIN_ERROR("Creating a new ArrayList instance failed.");
        return NULL;
    }

    for (size_t i = 0; i < m->value.histogram->num; i++) {
        histogram_bucket_t *bucket = &m->value.histogram->buckets[i];

        jobject o_bucket = (*jvm_env)->NewObject(jvm_env, gref.metric_histogram_bucket.class,
                                                          gref.metric_histogram_bucket.constructor,
                                                          (jlong)bucket->counter,
                                                          (jdouble)bucket->maximum);
        if (o_bucket == NULL) {
            PLUGIN_ERROR("Creating a new MetricHistogramBucket instance failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_list);
            return NULL;
        }

        (*jvm_env)->CallBooleanMethod(jvm_env, o_list, gref.array_list.add, o_bucket);

        (*jvm_env)->DeleteLocalRef(jvm_env, o_bucket);
    }


    jobject o_metric = (*jvm_env)->NewObject(jvm_env, gref.metric_histogram.class,
                                                      gref.metric_histogram.constructor,
                                                      (jdouble)m->value.histogram->sum,
                                                      o_list);
    if (o_metric == NULL) {
        PLUGIN_ERROR("Creating a new org.ncollectd.api.MetricHistogram instance failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_list);
        return NULL;
    }

    (*jvm_env)->DeleteLocalRef(jvm_env, o_list);

    return o_metric;
}

static jobject ctoj_metric_summary(JNIEnv *jvm_env, const metric_t *m)
{
    jobject o_list = (*jvm_env)->NewObject(jvm_env, gref.array_list.class,
                                                    gref.array_list.constructor,
                                                    (jint)m->value.summary->num);
    if (o_list == NULL) {
        PLUGIN_ERROR("Creating a new ArrayList instance failed.");
        return NULL;
    }

    for (size_t i = 0; i < m->value.summary->num; i++) {
        summary_quantile_t *quantile = &m->value.summary->quantiles[i];

        jobject o_quantile = (*jvm_env)->NewObject(jvm_env, gref.metric_summary_quantile.class,
                                                            gref.metric_summary_quantile.constructor,
                                                            (jdouble)quantile->quantile,
                                                            (jdouble)quantile->value);
        if (o_quantile == NULL) {
            PLUGIN_ERROR("Creating a new MetricSummaryQuantile instance failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_list);
            return NULL;
        }

        (*jvm_env)->CallBooleanMethod(jvm_env, o_list, gref.array_list.add, o_quantile);

        (*jvm_env)->DeleteLocalRef(jvm_env, o_quantile);
    }


    jobject o_metric = (*jvm_env)->NewObject(jvm_env, gref.metric_summary.class,
                                                      gref.metric_summary.constructor,
                                                      (jdouble)m->value.summary->sum,
                                                      (jlong)m->value.summary->count,
                                                      o_list);
    if (o_metric == NULL) {
        PLUGIN_ERROR("Creating a new org.ncollectd.api.MetricSummary instance failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_list);
        return NULL;
    }

    (*jvm_env)->DeleteLocalRef(jvm_env, o_list);

    return o_metric;
}

static jobject ctoj_metric_info(JNIEnv *jvm_env, const metric_t *m)
{
    jobject o_info = ctoj_label_set_object(jvm_env, &m->value.info);
    if (o_info == NULL)
        return NULL;

    jobject o_metric = (*jvm_env)->NewObject(jvm_env, gref.metric_info.class,
                                                      gref.metric_info.constructor, o_info);
    if (o_metric == NULL) {
        PLUGIN_ERROR("Creating a new MetricInfo instance failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_info);
        return NULL;
    }

    (*jvm_env)->DeleteLocalRef(jvm_env, o_info);

    return o_metric;
}

static jobject ctoj_metric_state_set(JNIEnv *jvm_env, const metric_t *m)
{
    jobject o_set = ctoj_state_set_object(jvm_env, &m->value.state_set);
    if (o_set == NULL)
        return NULL;

    jobject o_metric = (*jvm_env)->NewObject(jvm_env, gref.metric_state_set.class,
                                                      gref.metric_state_set.constructor, o_set);
    if (o_metric == NULL) {
        PLUGIN_ERROR("Creating a new MetricStateSet instance failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_set);
        return NULL;
    }

    (*jvm_env)->DeleteLocalRef(jvm_env, o_set);

    return o_metric;
}

static jobject ctoj_metric_counter(JNIEnv *jvm_env, const metric_t *m)
{

    if (m->value.counter.type == COUNTER_UINT64) {
        jobject o_metric = (*jvm_env)->NewObject(jvm_env, gref.metric_counter.class,
                                                          gref.metric_counter.constructor_long,
                                                          (jlong)m->value.counter.uint64);
        if (o_metric == NULL) {
            PLUGIN_ERROR("Creating a new MetricCounter instance failed.");
            return NULL;
        }

        return o_metric;
    } else if (m->value.counter.type == COUNTER_FLOAT64) {
        jobject o_metric = (*jvm_env)->NewObject(jvm_env, gref.metric_counter.class,
                                                          gref.metric_counter.constructor_double,
                                                          (jdouble)m->value.counter.float64);
        if (o_metric == NULL) {
            PLUGIN_ERROR("Creating a new MetricCounter instance failed.");
            return NULL;
        }

        return o_metric;
    }

    return NULL;
}

static jobject ctoj_metric_gauge(JNIEnv *jvm_env, const metric_t *m)
{
    if (m->value.gauge.type == GAUGE_FLOAT64 ) {
        jobject o_metric = (*jvm_env)->NewObject(jvm_env, gref.metric_gauge.class,
                                                          gref.metric_gauge.constructor_double,
                                                          (jdouble)m->value.gauge.float64);
        if (o_metric == NULL) {
            PLUGIN_ERROR("Creating a new MetricGauge instance failed.");
            return NULL;
        }

        return o_metric;
    } else if (m->value.gauge.type == GAUGE_INT64) {
        jobject o_metric = (*jvm_env)->NewObject(jvm_env, gref.metric_gauge.class,
                                                          gref.metric_gauge.constructor_long,
                                                          (jlong)m->value.gauge.int64);
        if (o_metric == NULL) {
            PLUGIN_ERROR("Creating a new MetricGauge instance failed.");
            return NULL;
        }

        return o_metric;
    }

    return NULL;
}

static jobject ctoj_metric_unknown(JNIEnv *jvm_env, const metric_t *m)
{
    if (m->value.unknown.type == UNKNOWN_FLOAT64) {
        jobject o_metric = (*jvm_env)->NewObject(jvm_env, gref.metric_unknown.class,
                                                          gref.metric_unknown.constructor_double,
                                                          (jdouble)m->value.unknown.float64);
        if (o_metric == NULL) {
            PLUGIN_ERROR("Creating a new MetricUnknown instance failed.");
            return NULL;
        }

        return o_metric;
    } else if (m->value.unknown.type == UNKNOWN_INT64) {
        jobject o_metric = (*jvm_env)->NewObject(jvm_env, gref.metric_unknown.class,
                                                          gref.metric_unknown.constructor_long,
                                                          (jlong)m->value.unknown.int64);
        if (o_metric == NULL) {
            PLUGIN_ERROR("Creating a new MetricUnknown instance failed.");
            return NULL;
        }

        return o_metric;
    }

    return NULL;
}

static jobject ctoj_metric(JNIEnv *jvm_env, metric_type_t type, const metric_t *m)
{
    jobject o_metric = NULL;

    switch(type) {
    case METRIC_TYPE_UNKNOWN:
        o_metric = ctoj_metric_unknown(jvm_env, m);
        break;
    case METRIC_TYPE_GAUGE:
        o_metric = ctoj_metric_gauge(jvm_env, m);
        break;
    case METRIC_TYPE_COUNTER:
        o_metric = ctoj_metric_counter(jvm_env, m);
        break;
    case METRIC_TYPE_STATE_SET:
        o_metric = ctoj_metric_state_set(jvm_env, m);
        break;
    case METRIC_TYPE_INFO:
        o_metric = ctoj_metric_info(jvm_env, m);
        break;
    case METRIC_TYPE_SUMMARY:
        o_metric = ctoj_metric_summary(jvm_env, m);
        break;
    case METRIC_TYPE_HISTOGRAM:
        o_metric = ctoj_metric_histogram(jvm_env, m);
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        o_metric = ctoj_metric_histogram(jvm_env, m);
        break;
    }

    if (o_metric == NULL)
        return NULL;

    if (m->label.num > 0) {
        jobject o_labels = ctoj_label_set_object(jvm_env, &m->label);
        if (o_labels == NULL) {
            PLUGIN_ERROR("ctoj_label_set_object failed.");
            return NULL;
        }

        (*jvm_env)->CallVoidMethod(jvm_env, o_metric, gref.metric.set_labels, o_labels);

        (*jvm_env)->DeleteLocalRef(jvm_env, o_labels);
    }

    if (m->time > 0) {
        /* Java measures time in milliseconds. */
         (*jvm_env)->CallVoidMethod(jvm_env, o_metric, gref.metric.set_time,
                                             (jlong)CDTIME_T_TO_MS(m->time));
    }

    if (m->interval > 0) {
        /* Java measures time in milliseconds. */
         (*jvm_env)->CallVoidMethod(jvm_env, o_metric, gref.metric.set_interval,
                                             (jlong)CDTIME_T_TO_MS(m->interval));
    }

    return o_metric;
}

jobject ctoj_metric_family(JNIEnv *jvm_env, const metric_family_t *fam)
{
    jstring o_name = (*jvm_env)->NewStringUTF(jvm_env, (fam->name != NULL) ? fam->name : "");
    if (o_name == NULL) {
        PLUGIN_ERROR("Cannot create String object.");
        return NULL;
    }

    jint type = fam->type;;

    jobject o_fam = (*jvm_env)->NewObject(jvm_env, gref.metric_family.class,
                                                   gref.metric_family.constructor, type, o_name);
    if (o_fam == NULL) {
        PLUGIN_ERROR("Creating a new MetricFamily instance failed.");
        return NULL;
    }

    (*jvm_env)->DeleteLocalRef(jvm_env, o_name);

    if (fam->help != NULL) {
        int status = ctoj_string(jvm_env, fam->help, o_fam, gref.metric_family.set_help);
        if (status != 0) {
            PLUGIN_ERROR("ctoj_string (setName) failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_fam);
            return NULL;
        }
    }

    if (fam->unit != NULL) {
        int status = ctoj_string(jvm_env, fam->unit, o_fam, gref.metric_family.set_unit);
        if (status != 0) {
            PLUGIN_ERROR("ctoj_string (setUnit) failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_fam);
            return NULL;
        }
    }

    for (size_t i = 0; i < fam->metric.num; i++) {
        metric_t *m = &fam->metric.ptr[i];
        jobject o_metric = ctoj_metric(jvm_env, fam->type, m);
        if (o_metric != NULL)
            (*jvm_env)->CallVoidMethod(jvm_env, o_fam, gref.metric_family.add_metric, o_metric);
    }

    return o_fam;
}

static int jtoc_metric_histogram(JNIEnv *jvm_env, metric_t *m, jobject o_metric)
{
    jdouble sum = (*jvm_env)->CallDoubleMethod(jvm_env, o_metric, gref.metric_histogram.get_sum);

    jobject o_list = (*jvm_env)->CallObjectMethod(jvm_env, o_metric,
                                                           gref.metric_histogram.get_buckets);
    if (o_list == NULL) {
        PLUGIN_ERROR("CallObjectMethod (getQuantiles) failed.");
        return -1;
    }

    jint size = (*jvm_env)->CallIntMethod(jvm_env, o_list, gref.list.size);

    jobject o_buckets_array = (*jvm_env)->CallObjectMethod(jvm_env, o_list, gref.list.to_array);
    if (o_buckets_array == NULL) {
        (*jvm_env)->DeleteLocalRef(jvm_env, o_list);
        PLUGIN_ERROR("CallObjectMethod (toArray) failed.");
        return -1;
    }

    m->value.histogram = calloc(1, sizeof(histogram_t) + sizeof(histogram_bucket_t)*size);
    if (m->value.histogram == NULL) {
        (*jvm_env)->DeleteLocalRef(jvm_env, o_list);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_buckets_array);
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    m->value.histogram->num = size;
    m->value.histogram->sum = sum;

    for (jint i = 0; i < size; i++) {
        jobject o_bucket = (*jvm_env)->GetObjectArrayElement(jvm_env, o_buckets_array, i);
        if (o_bucket == NULL) {
            (*jvm_env)->DeleteLocalRef(jvm_env, o_list);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_buckets_array);
            PLUGIN_ERROR("GetObjectArrayElement (%d) failed.", i);
            return -1;
        }

        jlong counter = (*jvm_env)->CallLongMethod(jvm_env, o_bucket,
                                                   gref.metric_histogram_bucket.get_counter);

        jdouble maximum = (*jvm_env)->CallDoubleMethod(jvm_env, o_bucket,
                                                       gref.metric_histogram_bucket.get_maximum);

        (*jvm_env)->DeleteLocalRef(jvm_env, o_bucket); // FIXME ¿?

        m->value.histogram->buckets[i].counter = counter;
        m->value.histogram->buckets[i].maximum = maximum;
    }

    (*jvm_env)->DeleteLocalRef(jvm_env, o_list);
    (*jvm_env)->DeleteLocalRef(jvm_env, o_buckets_array);

    return 0;
}

static int jtoc_metric_summary(JNIEnv *jvm_env, metric_t *m, jobject o_metric)
{
    jdouble sum = (*jvm_env)->CallDoubleMethod(jvm_env, o_metric, gref.metric_summary.get_sum);

    jdouble count = (*jvm_env)->CallDoubleMethod(jvm_env, o_metric, gref.metric_summary.get_count);

    jobject o_list = (*jvm_env)->CallObjectMethod(jvm_env, o_metric,
                                                           gref.metric_summary.get_quantiles);
    if (o_list == NULL) {
        PLUGIN_ERROR("CallObjectMethod (getQuantiles) failed.");
        return -1;
    }

    jint size = (*jvm_env)->CallIntMethod(jvm_env, o_list, gref.list.size);

    jobject o_quantiles_array = (*jvm_env)->CallObjectMethod(jvm_env, o_list, gref.list.to_array);
    if (o_quantiles_array == NULL) {
        PLUGIN_ERROR("CallObjectMethod (toArray) failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_list);
        return -1;
    }

    m->value.summary = calloc(1, sizeof(summary_t) + sizeof(summary_quantile_t)*size);
    if (m->value.summary == NULL) {
        PLUGIN_ERROR("calloc failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_list);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_quantiles_array);
        return -1;
    }

    m->value.summary->num = size;
    m->value.summary->sum = sum;
    m->value.summary->count = count;

    for (jint i = 0; i < size; i++) {
        jobject o_quantile = (*jvm_env)->GetObjectArrayElement(jvm_env, o_quantiles_array, i);
        if (o_quantile == NULL) {
            PLUGIN_ERROR("GetObjectArrayElement (%d) failed.", i);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_list);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_quantiles_array);
            return -1;
        }

        jdouble quantile = (*jvm_env)->CallDoubleMethod(jvm_env, o_quantile,
                                                        gref.metric_summary_quantile.get_quantile);

        jdouble value = (*jvm_env)->CallDoubleMethod(jvm_env, o_quantile,
                                                     gref.metric_summary_quantile.get_value);

        m->value.summary->quantiles[i].quantile = quantile;
        m->value.summary->quantiles[i].value = value;
    }

    (*jvm_env)->DeleteLocalRef(jvm_env, o_list);
    (*jvm_env)->DeleteLocalRef(jvm_env, o_quantiles_array);

    return 0;
}

static int jtoc_metric_state_set(JNIEnv *jvm_env, metric_t *m, jobject o_metric)
{
    int status = jtoc_state_set(jvm_env, &m->value.state_set, o_metric,
                                         gref.metric_state_set.get_set);
    if (status != 0) {
        PLUGIN_ERROR("jtoc_label_set (getSet) failed.");
        return -1;
    }

    return 0;
}

static int jtoc_metric_info(JNIEnv *jvm_env, metric_t *m, jobject o_metric)
{
    int status = jtoc_label_set(jvm_env, &m->value.info, o_metric, gref.metric_info.get_info);
    if (status != 0) {
        PLUGIN_ERROR("jtoc_label_set (getInfo) failed.");
        return -1;
    }

    return 0;
}

static int jtoc_metric_counter(JNIEnv *jvm_env, metric_t *m, jobject o_metric)
{
    jint type = (*jvm_env)->CallIntMethod(jvm_env, o_metric, gref.metric_counter.get_type);

    if (type == COUNTER_UINT64) {
        m->value.counter.type = COUNTER_UINT64;
        jlong value = (*jvm_env)->CallLongMethod(jvm_env, o_metric, gref.metric_counter.get_long);
        m->value.counter.uint64 = value;
    } else if (type == COUNTER_FLOAT64) {
        m->value.counter.type = COUNTER_FLOAT64;
        jdouble value = (*jvm_env)->CallDoubleMethod(jvm_env, o_metric,
                                                              gref.metric_counter.get_double);
        m->value.counter.float64 = value;
    }

    return 0;
}

static int jtoc_metric_gauge(JNIEnv *jvm_env, metric_t *m, jobject o_metric)
{
    jint type = (*jvm_env)->CallIntMethod(jvm_env, o_metric, gref.metric_gauge.get_type);

    if (type == GAUGE_FLOAT64) {
        m->value.gauge.type = GAUGE_FLOAT64;
        jdouble value = (*jvm_env)->CallDoubleMethod(jvm_env, o_metric,
                                                              gref.metric_gauge.get_double);
        m->value.gauge.float64 = value;
    } else if (type == GAUGE_INT64) {
        m->value.gauge.type = GAUGE_INT64;
        jlong value = (*jvm_env)->CallLongMethod(jvm_env, o_metric,
                                                          gref.metric_gauge.get_long);
        m->value.gauge.int64 = value;
    }

    return 0;
}

static int jtoc_metric_unknown(JNIEnv *jvm_env, metric_t *m, jobject o_metric)
{
    jint type = (*jvm_env)->CallIntMethod(jvm_env, o_metric, gref.metric_unknown.get_type);

    if (type == UNKNOWN_FLOAT64) {
        m->value.unknown.type = UNKNOWN_FLOAT64;
        jdouble value = (*jvm_env)->CallDoubleMethod(jvm_env, o_metric,
                                                              gref.metric_unknown.get_double);
        m->value.unknown.float64 = value;
    } else if (type == UNKNOWN_INT64) {
        m->value.unknown.type = UNKNOWN_INT64;
        jlong value = (*jvm_env)->CallLongMethod(jvm_env, o_metric, gref.metric_unknown.get_long);
        m->value.unknown.int64 = value;
    }

    return 0;
}

static int jtoc_metric(JNIEnv *jvm_env, metric_type_t type, metric_t *m, jobject o_metric)
{
    jclass c_metric = (*jvm_env)->GetObjectClass(jvm_env, o_metric);
    if (c_metric == NULL) {
        PLUGIN_ERROR("jtoc_metric: GetObjectClass failed.");
        return -1;
    }

    switch(type) {
    case METRIC_TYPE_UNKNOWN:
        jtoc_metric_unknown(jvm_env, m, o_metric);
        break;
    case METRIC_TYPE_GAUGE:
        jtoc_metric_gauge(jvm_env, m, o_metric);
        break;
    case METRIC_TYPE_COUNTER:
        jtoc_metric_counter(jvm_env, m, o_metric);
        break;
    case METRIC_TYPE_STATE_SET:
        jtoc_metric_state_set(jvm_env, m, o_metric);
        break;
    case METRIC_TYPE_INFO:
        jtoc_metric_info(jvm_env, m, o_metric);
        break;
    case METRIC_TYPE_SUMMARY:
        jtoc_metric_summary(jvm_env, m, o_metric);
        break;
    case METRIC_TYPE_HISTOGRAM:
        jtoc_metric_histogram(jvm_env, m, o_metric);
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        jtoc_metric_histogram(jvm_env, m, o_metric);
        break;
    }

    int status = jtoc_label_set(jvm_env, &m->label, o_metric, gref.metric.get_labels);
    if (status != 0) {
        PLUGIN_ERROR("jtoc_label_set (getLabels) failed.");
        return -1;
    }

    jlong time = (*jvm_env)->CallLongMethod(jvm_env, o_metric, gref.metric.get_time);
    /* Java measures time in milliseconds. */
    m->time = MS_TO_CDTIME_T(time);

    jlong interval = (*jvm_env)->CallLongMethod(jvm_env, o_metric, gref.metric.get_interval);
    /* Java measures time in milliseconds. */
    m->interval = MS_TO_CDTIME_T(interval);

    return 0;
}

int jtoc_metric_family(JNIEnv *jvm_env, metric_family_t *fam, jobject o_fam)
{
    // FIXME class equals
    jclass c_fam = (*jvm_env)->GetObjectClass(jvm_env, o_fam);
    if (c_fam == NULL) {
        PLUGIN_ERROR("jtoc_metric_family: GetObjectClass failed.");
        return -1;
    }

    char *name = NULL;
    int status = jtoc_string(jvm_env, &name, o_fam, gref.metric_family.get_name);
    if (status != 0) {
        PLUGIN_ERROR("jtoc_value_list: jtoc_string (getName) failed.");
        return -1;
    }
    fam->name = name;

    char *help = NULL;
    status = jtoc_string(jvm_env, &help, o_fam, gref.metric_family.get_help);
    if (status != 0) {
        PLUGIN_ERROR("jtoc_value_list: jtoc_string (getHelp) failed.");
        return -1;
    }
    fam->help = help;

    char *unit = NULL;
    status = jtoc_string(jvm_env, &unit, o_fam, gref.metric_family.get_unit);
    if (status != 0) {
        PLUGIN_ERROR("jtoc_value_list: jtoc_string (getUnit) failed.");
        return -1;
    }
    fam->unit = unit;

    jint type = (*jvm_env)->CallIntMethod(jvm_env, o_fam, gref.metric_family.get_type);
    fam->type = type;

    jobject o_metrics = (*jvm_env)->CallObjectMethod(jvm_env, o_fam,
                                                              gref.metric_family.get_metrics);
    if (o_metrics == NULL) {
        PLUGIN_ERROR("CallObjectMethod (getMetrics) failed.");
        return -1;
    }

    jobject o_metrics_array = (*jvm_env)->CallObjectMethod(jvm_env, o_metrics, gref.list.to_array);
    if (o_metrics_array == NULL) {
        PLUGIN_ERROR("CallObjectMethod (toArray) failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_metrics);
        return -1;
    }

    jsize metrics_size = (*jvm_env)->GetArrayLength(jvm_env, o_metrics_array);

    metric_t *metrics = calloc((size_t)metrics_size, sizeof(*metrics));
    if (metrics == NULL) {
        PLUGIN_ERROR("calloc failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_metrics);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_metrics_array);
        return -1;
    }

    fam->metric.ptr = metrics;
    fam->metric.num = metrics_size;

    for (jsize i = 0; i < metrics_size; i++) {
        metric_t *m = &fam->metric.ptr[(size_t)i];

        jobject o_metric = (*jvm_env)->GetObjectArrayElement(jvm_env, o_metrics_array, i);
        if (o_metric == NULL) {
            PLUGIN_ERROR("GetObjectArrayElement (%zu) failed.", (size_t)i);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_metrics);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_metrics_array);
            return -1;
        }

        jtoc_metric(jvm_env, fam->type, m, o_metric);

        (*jvm_env)->DeleteLocalRef(jvm_env, o_metric); // FIXME ¿?
    }

    (*jvm_env)->DeleteLocalRef(jvm_env, o_metrics);
    (*jvm_env)->DeleteLocalRef(jvm_env, o_metrics_array);

    return 0;
}
