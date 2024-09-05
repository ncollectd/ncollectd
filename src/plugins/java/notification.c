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

/* Convert a notification_t to a org/ncollectd/api/Notification */
jobject ctoj_notification(JNIEnv *jvm_env, const notification_t *n)
{
    /* Create a new instance. */
    jobject o_notification = (*jvm_env)->NewObject(jvm_env, gref.notification.class,
                                                            gref.notification.constructor);
    if (o_notification == NULL) {
        PLUGIN_ERROR("Creating a new Notification instance failed.");
        return NULL;
    }

    int status = ctoj_string(jvm_env, n->name, o_notification, gref.notification.set_name);
    if (status != 0) {
        PLUGIN_ERROR("ctoj_string (setName) failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_notification);
        return NULL;
    }

    /* Set the `time' member. Java stores time in milliseconds. */
    (*jvm_env)->CallVoidMethod(jvm_env, o_notification, gref.notification.set_time,
                                        (jlong)CDTIME_T_TO_MS(n->time));

    /* Set the `severity' member.. */
    (*jvm_env)->CallVoidMethod(jvm_env, o_notification, gref.notification.set_severity,
                                        (jint)n->severity);

    if (n->label.num > 0 ) {
        jobject o_labels = ctoj_label_set_object(jvm_env, &n->label);
        if (o_labels == NULL) {
            PLUGIN_ERROR("ctoj_label_set_object failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_notification);
           return NULL;
        }
        (*jvm_env)->CallVoidMethod(jvm_env, o_notification, gref.notification.set_labels,
                                            o_labels);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_labels);
    }

    if (n->annotation.num > 0 ) {
        jobject o_annotations = ctoj_label_set_object(jvm_env, &n->annotation);
        if (o_annotations == NULL) {
            PLUGIN_ERROR("ctoj_label_set_object failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_notification);
           return NULL;
        }
        (*jvm_env)->CallVoidMethod(jvm_env, o_notification, gref.notification.set_annotations,
                                            o_annotations);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_annotations);
    }

    return o_notification;
}

/* Convert a org/ncollectd/api/Notification to a notification_t. */
int jtoc_notification(JNIEnv *jvm_env, notification_t *n, jobject object_ptr)
{
    char *name = NULL;
    int status = jtoc_string(jvm_env, &name, object_ptr, gref.notification.get_name);
    if (status != 0) {
        PLUGIN_ERROR("jtoc_string (getName) failed.");
        return -1;
    }
    n->name = name;

    jlong tmp_long = (*jvm_env)->CallLongMethod(jvm_env, object_ptr, gref.notification.get_time);
    /* Java measures time in milliseconds. */
    n->time = MS_TO_CDTIME_T(tmp_long);

    jint tmp_int = (*jvm_env)->CallIntMethod(jvm_env, object_ptr, gref.notification.get_severity);
    n->severity = (int)tmp_int;

    status = jtoc_label_set(jvm_env, &n->label, object_ptr, gref.notification.get_labels);
    if (status != 0) {
        PLUGIN_ERROR("jtoc_label_set (getLabels) failed.");
        return -1;
    }

    status = jtoc_label_set(jvm_env, &n->annotation, object_ptr, gref.notification.get_annotations);
    if (status != 0) {
        PLUGIN_ERROR("jtoc_label_set (getAnnotation) failed.");
        return -1;
    }

    return 0;
}
