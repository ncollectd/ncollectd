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

        if ((*jvm_env)->ExceptionCheck(jvm_env)) {
            PLUGIN_ERROR("ArrayList.add failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_list);
            return NULL;
        }
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

        if ((*jvm_env)->ExceptionCheck(jvm_env)) {
            PLUGIN_ERROR("ArrayList.add failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_list);
            return NULL;
        }
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

jobject ctoj_metric(JNIEnv *jvm_env, metric_type_t type, const metric_t *m)
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
            (*jvm_env)->DeleteLocalRef(jvm_env, o_metric);
            return NULL;
        }

        (*jvm_env)->CallVoidMethod(jvm_env, o_metric, gref.metric.set_labels, o_labels);

        (*jvm_env)->DeleteLocalRef(jvm_env, o_labels);

        if ((*jvm_env)->ExceptionCheck(jvm_env))
            goto error;
    }

    if (m->time > 0) {
        (*jvm_env)->CallVoidMethod(jvm_env, o_metric, gref.metric.set_time,
                                            (jdouble)CDTIME_T_TO_DOUBLE(m->time));
        if ((*jvm_env)->ExceptionCheck(jvm_env))
            goto error;
    }

    if (m->interval > 0) {
        (*jvm_env)->CallVoidMethod(jvm_env, o_metric, gref.metric.set_interval,
                                            (jdouble)CDTIME_T_TO_DOUBLE(m->interval));
        if ((*jvm_env)->ExceptionCheck(jvm_env))
            goto error;
    }

    return o_metric;
error:
    PLUGIN_ERROR("Setting the metric labels, time or interval failed.");
    (*jvm_env)->DeleteLocalRef(jvm_env, o_metric);
    return NULL;
}

int jtoc_list_to_array(JNIEnv *jvm_env, jobject object, jmethodID method_id,
                       jobjectArray *ret_array, jsize *ret_size)
{
    *ret_array = NULL;
    *ret_size = 0;

    jobject o_list = (*jvm_env)->CallObjectMethod(jvm_env, object, method_id);
    if ((*jvm_env)->ExceptionCheck(jvm_env))
        return -1;
    if (o_list == NULL)
        return 0;

    jobjectArray o_array = (*jvm_env)->CallObjectMethod(jvm_env, o_list, gref.list.to_array);
    (*jvm_env)->DeleteLocalRef(jvm_env, o_list);
    if ((*jvm_env)->ExceptionCheck(jvm_env) || (o_array == NULL)) {
        PLUGIN_ERROR("List.toArray failed.");
        return -1;
    }

    *ret_array = o_array;
    *ret_size = (*jvm_env)->GetArrayLength(jvm_env, o_array);
    return 0;
}

static int jtoc_metric_histogram(JNIEnv *jvm_env, metric_t *m, jobject o_metric)
{
    jdouble sum = (*jvm_env)->CallDoubleMethod(jvm_env, o_metric, gref.metric_histogram.get_sum);
    if ((*jvm_env)->ExceptionCheck(jvm_env))
        return -1;

    jobjectArray o_buckets = NULL;
    jsize size = 0;
    if (jtoc_list_to_array(jvm_env, o_metric, gref.metric_histogram.get_buckets,
                           &o_buckets, &size) != 0) {
       PLUGIN_ERROR("Reading the histogram buckets failed.");
        return -1;
    }

    m->value.histogram = calloc(1, sizeof(histogram_t) + sizeof(histogram_bucket_t)*(size_t)size);
    if (m->value.histogram == NULL) {
        PLUGIN_ERROR("calloc failed.");
        goto error;
    }

    m->value.histogram->num = size;
    m->value.histogram->sum = sum;

    for (jsize i = 0; i < size; i++) {
        jobject o_bucket = (*jvm_env)->GetObjectArrayElement(jvm_env, o_buckets, i);
        if (o_bucket == NULL) {
            PLUGIN_ERROR("Histogram bucket %d is null.", (int)i);
            goto error;
        }

        if (!(*jvm_env)->IsInstanceOf(jvm_env, o_bucket, gref.metric_histogram_bucket.class)) {
            PLUGIN_ERROR("Histogram bucket %d is not a MetricHistogramBucket.", (int)i);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_bucket);
            goto error;
        }

        jlong counter = (*jvm_env)->CallLongMethod(jvm_env, o_bucket,
                                                   gref.metric_histogram_bucket.get_counter);

        jdouble maximum = 0;
        if (!(*jvm_env)->ExceptionCheck(jvm_env))
            maximum = (*jvm_env)->CallDoubleMethod(jvm_env, o_bucket,
                                                   gref.metric_histogram_bucket.get_maximum);

        (*jvm_env)->DeleteLocalRef(jvm_env, o_bucket);

        if ((*jvm_env)->ExceptionCheck(jvm_env))
            goto error;

        m->value.histogram->buckets[i].counter = counter;
        m->value.histogram->buckets[i].maximum = maximum;
    }

    if (o_buckets != NULL)
        (*jvm_env)->DeleteLocalRef(jvm_env, o_buckets);

    return 0;

error:
    if (o_buckets != NULL)
        (*jvm_env)->DeleteLocalRef(jvm_env, o_buckets);
    return -1;
}

static int jtoc_metric_summary(JNIEnv *jvm_env, metric_t *m, jobject o_metric)
{
    jdouble sum = (*jvm_env)->CallDoubleMethod(jvm_env, o_metric, gref.metric_summary.get_sum);
    if ((*jvm_env)->ExceptionCheck(jvm_env))
        return -1;

    jdouble count = (*jvm_env)->CallDoubleMethod(jvm_env, o_metric, gref.metric_summary.get_count);
    if ((*jvm_env)->ExceptionCheck(jvm_env))
        return -1;

    jobjectArray o_quantiles = NULL;
    jsize size = 0;
    if (jtoc_list_to_array(jvm_env, o_metric, gref.metric_summary.get_quantiles,
                           &o_quantiles, &size) != 0) {
        PLUGIN_ERROR("Reading the summary quantiles failed.");
        return -1;
    }

    m->value.summary = calloc(1, sizeof(summary_t) + sizeof(summary_quantile_t)*(size_t)size);
    if (m->value.summary == NULL) {
        PLUGIN_ERROR("calloc failed.");
        goto error;
    }

    m->value.summary->num = size;
    m->value.summary->sum = sum;
    m->value.summary->count = count;

    for (jsize i = 0; i < size; i++) {
        jobject o_quantile = (*jvm_env)->GetObjectArrayElement(jvm_env, o_quantiles, i);
        if (o_quantile == NULL) {
            PLUGIN_ERROR("Summary quantile %d is null.", (int)i);
            goto error;
        }

        if (!(*jvm_env)->IsInstanceOf(jvm_env, o_quantile, gref.metric_summary_quantile.class)) {
            PLUGIN_ERROR("Summary quantile %d is not a MetricSummaryQuantile.", (int)i);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_quantile);
            goto error;
        }

        jdouble quantile = (*jvm_env)->CallDoubleMethod(jvm_env, o_quantile,
                                                        gref.metric_summary_quantile.get_quantile);

        jdouble value = 0;
        if (!(*jvm_env)->ExceptionCheck(jvm_env))
            value = (*jvm_env)->CallDoubleMethod(jvm_env, o_quantile,
                                                 gref.metric_summary_quantile.get_value);

        (*jvm_env)->DeleteLocalRef(jvm_env, o_quantile);

        if ((*jvm_env)->ExceptionCheck(jvm_env))
            goto error;

        m->value.summary->quantiles[i].quantile = quantile;
        m->value.summary->quantiles[i].value = value;
    }

    if (o_quantiles != NULL)
        (*jvm_env)->DeleteLocalRef(jvm_env, o_quantiles);

    return 0;

error:
    if (o_quantiles != NULL)
        (*jvm_env)->DeleteLocalRef(jvm_env, o_quantiles);
    return -1;
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
    if ((*jvm_env)->ExceptionCheck(jvm_env))
        return -1;

    if (type == COUNTER_UINT64) {
        m->value.counter.type = COUNTER_UINT64;
        jlong value = (*jvm_env)->CallLongMethod(jvm_env, o_metric, gref.metric_counter.get_long);
        m->value.counter.uint64 = value;
    } else if (type == COUNTER_FLOAT64) {
        m->value.counter.type = COUNTER_FLOAT64;
        jdouble value = (*jvm_env)->CallDoubleMethod(jvm_env, o_metric,
                                                              gref.metric_counter.get_double);
        m->value.counter.float64 = value;
    } else {
        PLUGIN_ERROR("Unknown counter type %d.", (int)type);
        return -1;
    }

    return (*jvm_env)->ExceptionCheck(jvm_env) ? -1 : 0;
}

static int jtoc_metric_gauge(JNIEnv *jvm_env, metric_t *m, jobject o_metric)
{
    jint type = (*jvm_env)->CallIntMethod(jvm_env, o_metric, gref.metric_gauge.get_type);
    if ((*jvm_env)->ExceptionCheck(jvm_env))
        return -1;

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
    } else {
        PLUGIN_ERROR("Unknown gauge type %d.", (int)type);
        return -1;
    }

    return (*jvm_env)->ExceptionCheck(jvm_env) ? -1 : 0;
}

static int jtoc_metric_unknown(JNIEnv *jvm_env, metric_t *m, jobject o_metric)
{
    jint type = (*jvm_env)->CallIntMethod(jvm_env, o_metric, gref.metric_unknown.get_type);
    if ((*jvm_env)->ExceptionCheck(jvm_env))
        return -1;

    if (type == UNKNOWN_FLOAT64) {
        m->value.unknown.type = UNKNOWN_FLOAT64;
        jdouble value = (*jvm_env)->CallDoubleMethod(jvm_env, o_metric,
                                                              gref.metric_unknown.get_double);
        m->value.unknown.float64 = value;
    } else if (type == UNKNOWN_INT64) {
        m->value.unknown.type = UNKNOWN_INT64;
        jlong value = (*jvm_env)->CallLongMethod(jvm_env, o_metric, gref.metric_unknown.get_long);
        m->value.unknown.int64 = value;
    } else {
        PLUGIN_ERROR("Unknown value type %d.", (int)type);
        return -1;
    }

    return (*jvm_env)->ExceptionCheck(jvm_env) ? -1 : 0;
}

int jtoc_metric(JNIEnv *jvm_env, metric_type_t type, metric_t *m, jobject o_metric)
{
    int status = -1;

    switch(type) {
    case METRIC_TYPE_UNKNOWN:
        status = jtoc_metric_unknown(jvm_env, m, o_metric);
        break;
    case METRIC_TYPE_GAUGE:
        status = jtoc_metric_gauge(jvm_env, m, o_metric);
        break;
    case METRIC_TYPE_COUNTER:
        status = jtoc_metric_counter(jvm_env, m, o_metric);
        break;
    case METRIC_TYPE_STATE_SET:
        status = jtoc_metric_state_set(jvm_env, m, o_metric);
        break;
    case METRIC_TYPE_INFO:
        status = jtoc_metric_info(jvm_env, m, o_metric);
        break;
    case METRIC_TYPE_SUMMARY:
        status = jtoc_metric_summary(jvm_env, m, o_metric);
        break;
    case METRIC_TYPE_HISTOGRAM:
        status = jtoc_metric_histogram(jvm_env, m, o_metric);
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        status = jtoc_metric_histogram(jvm_env, m, o_metric);
        break;
    }

    if (status != 0)
        return -1;

    status = jtoc_label_set(jvm_env, &m->label, o_metric, gref.metric.get_labels);
    if (status != 0) {
        PLUGIN_ERROR("jtoc_label_set (getLabels) failed.");
        return -1;
    }

    jdouble time = (*jvm_env)->CallDoubleMethod(jvm_env, o_metric, gref.metric.get_time);
    if ((*jvm_env)->ExceptionCheck(jvm_env))
        return -1;
    m->time = DOUBLE_TO_CDTIME_T(time);

    jdouble interval = (*jvm_env)->CallDoubleMethod(jvm_env, o_metric, gref.metric.get_interval);
    if ((*jvm_env)->ExceptionCheck(jvm_env))
        return -1;
    m->interval = DOUBLE_TO_CDTIME_T(interval);

    return 0;
}

jclass jtoc_metric_class(metric_type_t type)
{
    switch(type) {
    case METRIC_TYPE_UNKNOWN:
        return gref.metric_unknown.class;
    case METRIC_TYPE_GAUGE:
        return gref.metric_gauge.class;
    case METRIC_TYPE_COUNTER:
        return gref.metric_counter.class;
    case METRIC_TYPE_STATE_SET:
        return gref.metric_state_set.class;
    case METRIC_TYPE_INFO:
        return gref.metric_info.class;
    case METRIC_TYPE_SUMMARY:
        return gref.metric_summary.class;
    case METRIC_TYPE_HISTOGRAM:
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        return gref.metric_histogram.class;
    }

    return NULL;
}

