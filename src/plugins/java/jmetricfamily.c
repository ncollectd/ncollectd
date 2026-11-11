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

jobject ctoj_metric_family(JNIEnv *jvm_env, const metric_family_t *fam)
{
    jstring o_name = (*jvm_env)->NewStringUTF(jvm_env, (fam->name != NULL) ? fam->name : "");
    if (o_name == NULL) {
        PLUGIN_ERROR("Cannot create String object.");
        return NULL;
    }

    jint type = fam->type;;

    jobject o_fam = (*jvm_env)->NewObject(jvm_env, gref.metric_family.class,
                                                   gref.metric_family.constructor, o_name, type);
    (*jvm_env)->DeleteLocalRef(jvm_env, o_name);
    if (o_fam == NULL) {
        PLUGIN_ERROR("Creating a new MetricFamily instance failed.");
        return NULL;
    }

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
        if (o_metric == NULL) {
            PLUGIN_ERROR("ctoj_metric failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_fam);
            return NULL;
        }
        (*jvm_env)->CallVoidMethod(jvm_env, o_fam, gref.metric_family.add_metric, o_metric);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_metric);
        if ((*jvm_env)->ExceptionCheck(jvm_env)) {
            PLUGIN_ERROR("MetricFamily.addMetric failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_fam);
            return NULL;
        }
    }

    return o_fam;
}

int jtoc_metric_family(JNIEnv *jvm_env, metric_family_t *fam, jobject o_fam)
{
    jobjectArray o_metrics = NULL;

    jint type = (*jvm_env)->CallIntMethod(jvm_env, o_fam, gref.metric_family.get_type);
    if ((*jvm_env)->ExceptionCheck(jvm_env))
        return -1;

    jclass c_metric = jtoc_metric_class((metric_type_t)type);
    if (c_metric == NULL) {
        PLUGIN_ERROR("Invalid metric family type %d.", (int)type);
        return -1;
    }

    fam->type = type;

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
        goto error;
    }
    fam->help = help;

    char *unit = NULL;
    status = jtoc_string(jvm_env, &unit, o_fam, gref.metric_family.get_unit);
    if (status != 0) {
        PLUGIN_ERROR("jtoc_value_list: jtoc_string (getUnit) failed.");
        goto error;
    }
    fam->unit = unit;

    jsize metrics_size = 0;
    if (jtoc_list_to_array(jvm_env, o_fam, gref.metric_family.get_metrics,
                           &o_metrics, &metrics_size) != 0) {
        PLUGIN_ERROR("Reading the metrics failed.");
         goto error;
     }

    if (metrics_size > 0) {
        metric_t *metrics = calloc((size_t)metrics_size, sizeof(*metrics));
        if (metrics == NULL) {
            PLUGIN_ERROR("calloc failed.");
            goto error;
        }
       
        fam->metric.ptr = metrics;
        fam->metric.num = metrics_size;
       
        for (jsize i = 0; i < metrics_size; i++) {
            metric_t *m = &fam->metric.ptr[(size_t)i];
       
            jobject o_metric = (*jvm_env)->GetObjectArrayElement(jvm_env, o_metrics, i);
            if (o_metric == NULL) {
                PLUGIN_ERROR("Metric %d of family '%s' is null.", (int)i,
                             (fam->name != NULL) ? fam->name : "");
                goto error;
            }

            if (!(*jvm_env)->IsInstanceOf(jvm_env, o_metric, c_metric)) {
                PLUGIN_ERROR("Metric %d of family '%s' does not match the family type.", (int)i,
                             (fam->name != NULL) ? fam->name : "");
                (*jvm_env)->DeleteLocalRef(jvm_env, o_metric);
                goto error;
            }

            status = jtoc_metric(jvm_env, fam->type, m, o_metric);

            (*jvm_env)->DeleteLocalRef(jvm_env, o_metric);

            if (status != 0) {
                PLUGIN_ERROR("Converting metric %d of family '%s' failed.", (int)i,
                             (fam->name != NULL) ? fam->name : "");
                goto error;
            }
        }
    }

    if (o_metrics != NULL)
        (*jvm_env)->DeleteLocalRef(jvm_env, o_metrics);

    return 0;

error:
    if (o_metrics != NULL)
        (*jvm_env)->DeleteLocalRef(jvm_env, o_metrics);
    free(fam->name);
    fam->name = NULL;
    free(fam->help);
    fam->help = NULL;
    free(fam->unit);
    fam->unit = NULL;
    metric_list_reset(&fam->metric, fam->type);
    return -1;
}
