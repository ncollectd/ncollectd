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

typedef struct {
    const char *name;
    const char *signature;
    jmethodID *method_id;
    bool is_static;
} jcached_method_t;

typedef struct {
    const char *name;
    jclass *class;
    jcached_method_t *methods;
} jcached_class_t;

static jcached_class_t classes[] = {
    { "java/lang/Object", &gref.object.class, (jcached_method_t[]) {
        { "toString", "()Ljava/lang/String;", &gref.object.to_string },
        { NULL,       NULL,                   NULL                   }}},
    { "java/lang/Throwable", &gref.throwable.class, (jcached_method_t[]) {
        { "getStackTrace", "()[Ljava/lang/StackTraceElement;", &gref.throwable.get_stack_trace },
        { NULL,            NULL,                               NULL                            }}},
    { "java/lang/String", &gref.string.class, (jcached_method_t[]) {
        { NULL, NULL, NULL }}},
    { "java/util/ArrayList", &gref.array_list.class, (jcached_method_t[]) {
        { "<init>", "(I)V",                  &gref.array_list.constructor },
        { "add",    "(Ljava/lang/Object;)Z", &gref.array_list.add         },
        { NULL,     NULL,                    NULL                         }}},
    { "java/util/List", &gref.list.class, (jcached_method_t[]) {
        { "toArray", "()[Ljava/lang/Object;", &gref.list.to_array },
        { NULL,      NULL,                    NULL                }}},
    { "java/util/HashMap", &gref.hash_map.class, (jcached_method_t[]) {
        { "<init>", "(I)V", &gref.hash_map.constructor  },
        { "put",    "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;", &gref.hash_map.put },
        { NULL,     NULL,                     NULL        }}},
    { "java/lang/Boolean", &gref.boolean.class, (jcached_method_t[]) {
        { "valueOf",      "(Z)Ljava/lang/Boolean;", &gref.boolean.value_of, true },
        { "booleanValue", "()Z",                    &gref.boolean.boolean_value  },
        { NULL,           NULL,                     NULL                         }}},
    { "java/lang/Long", &gref._long.class, (jcached_method_t[]) {
        { "valueOf", "(J)Ljava/lang/Long;", &gref._long.value_of, true },
        { NULL,      NULL,                  NULL                       }}},
    { "java/lang/Double", &gref._double.class, (jcached_method_t[]) {
        { "valueOf", "(D)Ljava/lang/Double;", &gref._double.value_of, true },
        { NULL,      NULL,                    NULL                         }}},
    { "java/util/Map", &gref.map.class, (jcached_method_t[]) {
        { "entrySet", "()Ljava/util/Set;", &gref.map.entry_set },
        { NULL,       NULL,                NULL                }}},
    { "java/util/Set", &gref.set.class, (jcached_method_t[]) {
        { "iterator", "()Ljava/util/Iterator;", &gref.set.iterator },
        { NULL,       NULL,                     NULL               }}},
    { "java/util/Iterator", &gref.iterator.class, (jcached_method_t[]) {
        { "hasNext", "()Z",                  &gref.iterator.has_next },
        { "next",    "()Ljava/lang/Object;", &gref.iterator.next     },
        { NULL,      NULL,                   NULL                    }}},
    { "java/util/Map$Entry", &gref.map_entry.class, (jcached_method_t[]) {
        { "getKey",   "()Ljava/lang/Object;", &gref.map_entry.get_key   },
        { "getValue", "()Ljava/lang/Object;", &gref.map_entry.get_value },
        { NULL,       NULL,                   NULL                      }}},
    { "org/ncollectd/api/Metric", &gref.metric.class,  (jcached_method_t[]) {
        { "setLabels",   "(Ljava/util/HashMap;)V", &gref.metric.set_labels   },
        { "setTime",     "(D)V",                   &gref.metric.set_time     },
        { "setInterval", "(D)V",                   &gref.metric.set_interval },
        { "getLabels",   "()Ljava/util/HashMap;",  &gref.metric.get_labels   },
        { "getTime",     "()D",                    &gref.metric.get_time     },
        { "getInterval", "()D",                    &gref.metric.get_interval },
        { NULL,          NULL,                     NULL                      }}},
    { "org/ncollectd/api/MetricFamily", &gref.metric_family.class, (jcached_method_t[]) {
        { "<init>",     "(Ljava/lang/String;I)V",        &gref.metric_family.constructor },
        { "setHelp",    "(Ljava/lang/String;)V",         &gref.metric_family.set_help    },
        { "setUnit",    "(Ljava/lang/String;)V",         &gref.metric_family.set_unit    },
        { "addMetric",  "(Lorg/ncollectd/api/Metric;)V", &gref.metric_family.add_metric  },
        { "getName",    "()Ljava/lang/String;",          &gref.metric_family.get_name    },
        { "getHelp",    "()Ljava/lang/String;",          &gref.metric_family.get_help    },
        { "getUnit",    "()Ljava/lang/String;",          &gref.metric_family.get_unit    },
        { "getType",    "()I",                           &gref.metric_family.get_type    },
        { "getMetrics", "()Ljava/util/List;",            &gref.metric_family.get_metrics },
        { NULL,         NULL,                            NULL                            }}},
    { "org/ncollectd/api/MetricUnknown", &gref.metric_unknown.class, (jcached_method_t[]) {
        { "<init>",    "(D)V", &gref.metric_unknown.constructor_double },
        { "<init>",    "(J)V", &gref.metric_unknown.constructor_long   },
        { "getType",   "()I",  &gref.metric_unknown.get_type           },
        { "getDouble", "()D",  &gref.metric_unknown.get_double         },
        { "getLong",   "()J",  &gref.metric_unknown.get_long           },
        { NULL,        NULL,   NULL                                   }}},
    { "org/ncollectd/api/MetricGauge", &gref.metric_gauge.class, (jcached_method_t[]) {
        { "<init>",    "(D)V", &gref.metric_gauge.constructor_double },
        { "<init>",    "(J)V", &gref.metric_gauge.constructor_long   },
        { "getType",   "()I",  &gref.metric_gauge.get_type           },
        { "getDouble", "()D",  &gref.metric_gauge.get_double         },
        { "getLong",   "()J",  &gref.metric_gauge.get_long           },
        { NULL,        NULL,   NULL                                  }}},
    { "org/ncollectd/api/MetricCounter", &gref.metric_counter.class, (jcached_method_t[]) {
        { "<init>",    "(D)V", &gref.metric_counter.constructor_double },
        { "<init>",    "(J)V", &gref.metric_counter.constructor_long   },
        { "getType",   "()I",  &gref.metric_counter.get_type           },
        { "getDouble", "()D",  &gref.metric_counter.get_double         },
        { "getLong",   "()J",  &gref.metric_counter.get_long           },
        { NULL,        NULL,   NULL                                    }}},
    { "org/ncollectd/api/MetricInfo", &gref.metric_info.class, (jcached_method_t[]) {
        { "<init>",  "(Ljava/util/HashMap;)V", &gref.metric_info.constructor },
        { "getInfo", "()Ljava/util/HashMap;",  &gref.metric_info.get_info    },
        { NULL,      NULL,                     NULL                          }}},
    { "org/ncollectd/api/MetricStateSet", &gref.metric_state_set.class, (jcached_method_t[]) {
        { "<init>", "(Ljava/util/HashMap;)V", &gref.metric_state_set.constructor },
        { "getSet", "()Ljava/util/HashMap;",  &gref.metric_state_set.get_set     },
        { NULL,      NULL,                    NULL                               }}},
    { "org/ncollectd/api/MetricHistogram", &gref.metric_histogram.class, (jcached_method_t[]) {
        { "<init>",     "(DLjava/util/List;)V", &gref.metric_histogram.constructor },
        { "getSum",     "()D",                  &gref.metric_histogram.get_sum     },
        { "getBuckets", "()Ljava/util/List;",   &gref.metric_histogram.get_buckets },
        { NULL,         NULL,                   NULL                               }}},
    { "org/ncollectd/api/MetricHistogramBucket", &gref.metric_histogram_bucket.class, (jcached_method_t[]) {
        { "<init>",     "(JD)V", &gref.metric_histogram_bucket.constructor },
        { "getCounter", "()J",   &gref.metric_histogram_bucket.get_counter },
        { "getMaximum", "()D",   &gref.metric_histogram_bucket.get_maximum },
        { NULL,         NULL,    NULL                                      }}},
    { "org/ncollectd/api/MetricSummary", &gref.metric_summary.class, (jcached_method_t[]) {
        { "<init>",       "(DJLjava/util/List;)V", &gref.metric_summary.constructor },
        { "getSum",       "()D",                   &gref.metric_summary.get_sum       },
        { "getCount",     "()D",                   &gref.metric_summary.get_count     },
        { "getQuantiles", "()Ljava/util/List;",    &gref.metric_summary.get_quantiles },
        { NULL,           NULL,                    NULL                               }}},
    { "org/ncollectd/api/MetricSummaryQuantile", &gref.metric_summary_quantile.class, (jcached_method_t[]) {
        { "<init>",      "(DD)V", &gref.metric_summary_quantile.constructor  },
        { "getQuantile", "()D",   &gref.metric_summary_quantile.get_quantile },
        { "getValue",    "()D",   &gref.metric_summary_quantile.get_value    },
        { NULL,          NULL,    NULL                                       }}},
    { "org/ncollectd/api/Notification", &gref.notification.class, (jcached_method_t[]) {
        { "<init>",         "()V",                    &gref.notification.constructor     },
        { "setName",        "(Ljava/lang/String;)V",  &gref.notification.set_name        },
        { "setTime",        "(D)V",                   &gref.notification.set_time        },
        { "setSeverity",    "(I)V",                   &gref.notification.set_severity    },
        { "setLabels",      "(Ljava/util/HashMap;)V", &gref.notification.set_labels      },
        { "setAnnotations", "(Ljava/util/HashMap;)V", &gref.notification.set_annotations },
        { "getName",        "()Ljava/lang/String;",   &gref.notification.get_name        },
        { "getTime",        "()D",                    &gref.notification.get_time        },
        { "getSeverity",    "()I",                    &gref.notification.get_severity    },
        { "getLabels",      "()Ljava/util/HashMap;",  &gref.notification.get_labels      },
        { "getAnnotations", "()Ljava/util/HashMap;",  &gref.notification.get_annotations },
        { NULL,             NULL,                     NULL                               }}},
    { "org/ncollectd/api/ConfigValue",  &gref.config_value.class, (jcached_method_t[]) {
        { "<init>", "(Z)V",                  &gref.config_value.constructor_bool   },
        { "<init>", "(Ljava/lang/String;)V", &gref.config_value.constructor_string },
        { "<init>", "(Ljava/lang/Number;)V", &gref.config_value.constructor_number },
        { NULL,      NULL,                   NULL                                  }}},
    { "org/ncollectd/api/ConfigItem", &gref.config_item.class, (jcached_method_t[]) {
        { "<init>",   "(Ljava/lang/String;)V",              &gref.config_item.constructor },
        { "addValue", "(Lorg/ncollectd/api/ConfigValue;)V", &gref.config_item.add_value   },
        { "addChild", "(Lorg/ncollectd/api/ConfigItem;)V",  &gref.config_item.add_child   },
        { NULL,       NULL,                                 NULL                          }}},
    { NULL, NULL, NULL },
};

int cjni_cache_classes(JNIEnv *jvm_env)
{
    for (size_t i = 0; classes[i].name != NULL; i++) {
        jclass class = (*jvm_env)->FindClass (jvm_env, classes[i].name);
        if (class == NULL) {
            cjni_exception_check(jvm_env, "class lookup", classes[i].name);
            PLUGIN_ERROR("Looking up the '%s' class failed.", classes[i].name);
            return -1;
        }

        jcached_method_t *methods = classes[i].methods;
        for (size_t j = 0; methods[j].name != NULL; j++) {
            jmethodID method_id;
            if (methods[j].is_static)
                method_id = (*jvm_env)->GetStaticMethodID(jvm_env, class,
                                                          methods[j].name, methods[j].signature);
            else
                method_id = (*jvm_env)->GetMethodID (jvm_env, class,
                                                     methods[j].name, methods[j].signature);
            if (method_id == NULL) {
                cjni_exception_check(jvm_env, "method lookup", methods[j].name);
                PLUGIN_ERROR ("Looking method '%s' with signatute '%s' in class '%s' failed.",
                              methods[j].name, methods[j].signature, classes[i].name);
                (*jvm_env)->DeleteLocalRef (jvm_env, class);
                return -1;
            }
            *(methods[j].method_id) = method_id;
        }

        jclass global_class = (*jvm_env)->NewGlobalRef(jvm_env, class);
        if (global_class == NULL) {
            cjni_exception_check(jvm_env, "class lookup", classes[i].name);
            PLUGIN_ERROR ("Global class reference create failed for '%s'.", classes[i].name);
            (*jvm_env)->DeleteLocalRef (jvm_env, class);
            return -1;
        }
        (*jvm_env)->DeleteLocalRef (jvm_env, class);

        *(classes[i].class) = global_class;
    }

    return 0;
}

void cjni_cache_classes_release(JNIEnv *jvm_env)
{
    for (size_t i = 0; classes[i].name != NULL; i++) {
        if (*(classes[i].class) != NULL)
            (*jvm_env)->DeleteGlobalRef(jvm_env, *(classes[i].class));
    }
}

/* Copy the result of `object.toString()' to `buf'. Errors are ignored. */
static void cjni_object_to_string(JNIEnv *jvm_env, jobject object, char *buf, size_t size)
{
    jstring o_string = (*jvm_env)->CallObjectMethod(jvm_env, object, gref.object.to_string);
    if ((*jvm_env)->ExceptionCheck(jvm_env) || (o_string == NULL)) {
        (*jvm_env)->ExceptionClear(jvm_env);
        return;
    }

    const char *c_string = (*jvm_env)->GetStringUTFChars(jvm_env, o_string, NULL);
    if (c_string != NULL) {
        sstrncpy(buf, c_string, size);
        (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_string, c_string);
    } else {
        (*jvm_env)->ExceptionClear(jvm_env);
    }

    (*jvm_env)->DeleteLocalRef(jvm_env, o_string);
}

/* If a Java exception is pending, log it together with the frame that threw
 * it, clear it and return true. Native code must not call other JNI functions
 * while an exception is pending, and a callback that throws has failed. */
bool cjni_exception_check(JNIEnv *jvm_env, const char *what, const char *name)
{
    if (!(*jvm_env)->ExceptionCheck(jvm_env))
        return false;

    jthrowable o_exception = (*jvm_env)->ExceptionOccurred(jvm_env);
    (*jvm_env)->ExceptionClear(jvm_env);

    char message[512] = "";
    char where[512] = "";

    if ((o_exception != NULL) && (gref.object.to_string != NULL)) {
        cjni_object_to_string(jvm_env, o_exception, message, sizeof(message));

        jobjectArray o_trace = (*jvm_env)->CallObjectMethod(jvm_env, o_exception,
                                                            gref.throwable.get_stack_trace);
        if (!(*jvm_env)->ExceptionCheck(jvm_env) && (o_trace != NULL)) {
            if ((*jvm_env)->GetArrayLength(jvm_env, o_trace) > 0) {
                jobject o_frame = (*jvm_env)->GetObjectArrayElement(jvm_env, o_trace, 0);
                if (o_frame != NULL) {
                    cjni_object_to_string(jvm_env, o_frame, where, sizeof(where));
                    (*jvm_env)->DeleteLocalRef(jvm_env, o_frame);
                }
            }
            (*jvm_env)->DeleteLocalRef(jvm_env, o_trace);
        }
        (*jvm_env)->ExceptionClear(jvm_env);
    }

    if (o_exception != NULL)
        (*jvm_env)->DeleteLocalRef(jvm_env, o_exception);

    PLUGIN_ERROR("Unhandled Java exception in %s%s%s%s: %s%s%s", what,
                 (name != NULL) ? " '" : "", (name != NULL) ? name : "", (name != NULL) ? "'" : "",
                 message, (where[0] != 0) ? " at " : "", where);

    return true;
}

int ctoj_string(JNIEnv *jvm_env, const char *string, jobject object_ptr, jmethodID method_id)
{
    /* Create a java.lang.String */
    jstring o_string = (*jvm_env)->NewStringUTF(jvm_env, (string != NULL) ? string : "");
    if (o_string == NULL) {
        PLUGIN_ERROR("ctoj_string: NewStringUTF failed.");
        return -1;
    }

    /* Call the method. */
    (*jvm_env)->CallVoidMethod(jvm_env, object_ptr, method_id, o_string);

    /* Decrease reference counter on the java.lang.String object. */
    (*jvm_env)->DeleteLocalRef(jvm_env, o_string);

    if ((*jvm_env)->ExceptionCheck(jvm_env))
        return -1;

    return 0;
}

/* Convert a jlong to a java.lang.Number */
jobject ctoj_jlong_to_number(JNIEnv *jvm_env, jlong value)
{
    jobject o_number = (*jvm_env)->CallStaticObjectMethod(jvm_env, gref._long.class,
                                                          gref._long.value_of, value);
    if ((*jvm_env)->ExceptionCheck(jvm_env))
        return NULL;
    return o_number;
}

/* Convert a jdouble to a java.lang.Number */
jobject ctoj_jdouble_to_number(JNIEnv *jvm_env, jdouble value)
{
    jobject o_number = (*jvm_env)->CallStaticObjectMethod(jvm_env, gref._double.class,
                                                          gref._double.value_of, value);
    if ((*jvm_env)->ExceptionCheck(jvm_env))
        return NULL;
    return o_number;
}

jobject ctoj_label_set_object(JNIEnv *jvm_env, const label_set_t *label)
{
    jint size = label->num;

    jobject o_hash = (*jvm_env)->NewObject(jvm_env, gref.hash_map.class,
                                                    gref.hash_map.constructor, size);
    if (o_hash == NULL) {
        PLUGIN_ERROR("Failed to create HashMap object.");
        return NULL;
    }

    for (size_t i = 0; i < label->num; i++) {
        label_pair_t *pair = &label->ptr[i];

        jstring o_name = (*jvm_env)->NewStringUTF(jvm_env, (pair->name != NULL) ? pair->name : "");
        if (o_name == NULL) {
            PLUGIN_ERROR("ctoj_label_set: NewStringUTF failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);
            return NULL;
        }

        jstring o_value = (*jvm_env)->NewStringUTF(jvm_env, (pair->value != NULL) ? pair->value : "");
        if (o_value == NULL) {
            PLUGIN_ERROR("ctoj_label_set: NewStringUTF failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_name);
            return NULL;
        }

        jobject o_old = (*jvm_env)->CallObjectMethod(jvm_env, o_hash, gref.hash_map.put,
                                                              o_name, o_value);

        (*jvm_env)->DeleteLocalRef(jvm_env, o_name);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_value);

        if (o_old != NULL)
            (*jvm_env)->DeleteLocalRef(jvm_env, o_old);

        if ((*jvm_env)->ExceptionCheck(jvm_env)) {
            PLUGIN_ERROR("HashMap.put failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);
            return NULL;
        }
    }

    return o_hash;
}

jobject ctoj_state_set_object(JNIEnv *jvm_env, const state_set_t *set)
{
    jint size = set->num;

    jobject o_hash = (*jvm_env)->NewObject(jvm_env, gref.hash_map.class,
                                                    gref.hash_map.constructor, size);
    if (o_hash == NULL) {
        PLUGIN_ERROR("Failed to create HashMap object.");
        return NULL;
    }

    for (size_t i = 0; i < set->num; i++) {
        state_t *state = &set->ptr[i];

        jstring o_name = (*jvm_env)->NewStringUTF(jvm_env, (state->name != NULL) ? state->name : "");
        if (o_name == NULL) {
            PLUGIN_ERROR("ctoj_label_set: NewStringUTF failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);
            return NULL;
        }

        jobject o_value = (*jvm_env)->CallStaticObjectMethod(jvm_env, gref.boolean.class,
                                                             gref.boolean.value_of,
                                                             (jboolean)state->enabled);
        if ((*jvm_env)->ExceptionCheck(jvm_env) || (o_value == NULL)) {
            PLUGIN_ERROR("Failed to create Boolean object.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_name);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);
            return NULL;
        }

        jobject o_old = (*jvm_env)->CallObjectMethod(jvm_env, o_hash, gref.hash_map.put,
                                                              o_name, o_value);

        (*jvm_env)->DeleteLocalRef(jvm_env, o_name);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_value);

        if (o_old != NULL)
            (*jvm_env)->DeleteLocalRef(jvm_env, o_old);
    
        if ((*jvm_env)->ExceptionCheck(jvm_env)) {
            PLUGIN_ERROR("HashMap.put failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);
            return NULL;
        }
    }

    return o_hash;
}

/* Call a `String <method> ()' method. */
int jtoc_string(JNIEnv *jvm_env, char **str, jobject object_ptr, jmethodID method_id)
{
    *str = NULL;

    jobject string_obj = (*jvm_env)->CallObjectMethod(jvm_env, object_ptr, method_id);
    if ((*jvm_env)->ExceptionCheck(jvm_env))
        return -1;
    if (string_obj == NULL)
        return 0;

    const char *c_str = (*jvm_env)->GetStringUTFChars(jvm_env, string_obj, 0);
    if (c_str == NULL) {
        PLUGIN_ERROR("GetStringUTFChars failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, string_obj);
        return -1;
    }

    *str = strdup(c_str);

    (*jvm_env)->ReleaseStringUTFChars(jvm_env, string_obj, c_str);
    (*jvm_env)->DeleteLocalRef(jvm_env, string_obj);

    if (*str == NULL)
        return -1;

    return 0;
}

static int jtoc_label_pair(JNIEnv *jvm_env, label_set_t *label, jobject o_key, jobject o_value)
{
    if (!(*jvm_env)->IsInstanceOf(jvm_env, o_key, gref.string.class) ||
        !(*jvm_env)->IsInstanceOf(jvm_env, o_value, gref.string.class)) {
        PLUGIN_ERROR("Label names and values must be strings.");
        return -1;
    }

    const char *s_key = (*jvm_env)->GetStringUTFChars(jvm_env, o_key, 0);
    if (s_key == NULL) {
        PLUGIN_ERROR("GetStringUTFChars failed.");
        return -1;
    }

    const char *s_value = (*jvm_env)->GetStringUTFChars(jvm_env, o_value, 0);
    if (s_value == NULL) {
        PLUGIN_ERROR("GetStringUTFChars failed.");
        (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_key, s_key);
        return -1;
    }

    label_set_add(label, true, s_key, s_value);

    (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_value, s_value);
    (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_key, s_key);
    return 0;
}

int jtoc_label_set(JNIEnv *jvm_env, label_set_t *label, jobject object_ptr, jmethodID method_id)
{
    int status = -1;
    jobject o_set = NULL;
    jobject o_iterator = NULL;

    /* A null map is an empty set. */
    jobject o_hash = (*jvm_env)->CallObjectMethod(jvm_env, object_ptr, method_id);
    if ((*jvm_env)->ExceptionCheck(jvm_env))
        return -1;
    if (o_hash == NULL)
        return 0;

    o_set = (*jvm_env)->CallObjectMethod(jvm_env, o_hash, gref.map.entry_set);
    if ((*jvm_env)->ExceptionCheck(jvm_env) || (o_set == NULL)) {
        PLUGIN_ERROR("CallObjectMethod (entrySet) failed.");
        goto out;
    }

    o_iterator = (*jvm_env)->CallObjectMethod(jvm_env, o_set, gref.set.iterator);
    if ((*jvm_env)->ExceptionCheck(jvm_env) || (o_iterator == NULL)) {
        PLUGIN_ERROR("CallObjectMethod (iterator) failed.");
        goto out;
    }

    while (true) {
        jboolean has_next = (*jvm_env)->CallBooleanMethod(jvm_env, o_iterator,
                                                          gref.iterator.has_next);
        if ((*jvm_env)->ExceptionCheck(jvm_env))
            goto out;
        if (!has_next)
            break;

        jobject o_entry = (*jvm_env)->CallObjectMethod(jvm_env, o_iterator, gref.iterator.next);
        if ((*jvm_env)->ExceptionCheck(jvm_env) || (o_entry == NULL)) {
            PLUGIN_ERROR("CallObjectMethod (next) failed.");
            goto out;
        }

        jobject o_key = (*jvm_env)->CallObjectMethod(jvm_env, o_entry, gref.map_entry.get_key);
        jobject o_value = NULL;
        if (!(*jvm_env)->ExceptionCheck(jvm_env) && (o_key != NULL))
            o_value = (*jvm_env)->CallObjectMethod(jvm_env, o_entry, gref.map_entry.get_value);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_entry);
        if ((*jvm_env)->ExceptionCheck(jvm_env) || (o_key == NULL)) {
            PLUGIN_ERROR("Reading a label failed.");
            if (o_key != NULL)
                (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
            goto out;
        }

        if (o_value == NULL) {
            (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
            continue;
        }

        int ret = jtoc_label_pair(jvm_env, label, o_key, o_value);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_value);

        if (ret != 0)
            goto out;
    }

    status = 0;

out:
    if (o_iterator != NULL)
        (*jvm_env)->DeleteLocalRef(jvm_env, o_iterator);
    if (o_set != NULL)
        (*jvm_env)->DeleteLocalRef(jvm_env, o_set);
    (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);

    return status;
}

int jtoc_state_set(JNIEnv *jvm_env, state_set_t *set, jobject object_ptr, jmethodID method_id)
{
    int status = -1;
    jobject o_set = NULL;
    jobject o_iterator = NULL;

    /* A null map is an empty set. */
    jobject o_hash = (*jvm_env)->CallObjectMethod(jvm_env, object_ptr, method_id);
    if ((*jvm_env)->ExceptionCheck(jvm_env))
        return -1;
    if (o_hash == NULL)
        return 0;

    o_set = (*jvm_env)->CallObjectMethod(jvm_env, o_hash, gref.map.entry_set);
    if ((*jvm_env)->ExceptionCheck(jvm_env) || (o_set == NULL)) {
        PLUGIN_ERROR("CallObjectMethod (entrySet) failed.");
        goto out;
    }

    o_iterator = (*jvm_env)->CallObjectMethod(jvm_env, o_set, gref.set.iterator);
    if ((*jvm_env)->ExceptionCheck(jvm_env) || (o_iterator == NULL)) {
        PLUGIN_ERROR("CallObjectMethod (iterator) failed.");
        goto out;
    }

    while (true) {
        jboolean has_next = (*jvm_env)->CallBooleanMethod(jvm_env, o_iterator,
                                                          gref.iterator.has_next);
        if ((*jvm_env)->ExceptionCheck(jvm_env))
            goto out;
        if (!has_next)
            break;

        jobject o_entry = (*jvm_env)->CallObjectMethod(jvm_env, o_iterator, gref.iterator.next);
        if ((*jvm_env)->ExceptionCheck(jvm_env) || (o_entry == NULL)) {
            PLUGIN_ERROR("CallObjectMethod (next) failed.");
            goto out;
        }

        jobject o_key = (*jvm_env)->CallObjectMethod(jvm_env, o_entry, gref.map_entry.get_key);
        jobject o_value = NULL;
        if (!(*jvm_env)->ExceptionCheck(jvm_env) && (o_key != NULL))
            o_value = (*jvm_env)->CallObjectMethod(jvm_env, o_entry, gref.map_entry.get_value);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_entry);
        if ((*jvm_env)->ExceptionCheck(jvm_env) || (o_key == NULL) || (o_value == NULL)) {
            PLUGIN_ERROR("Reading a state failed.");
            if (o_key != NULL)
                (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
            goto out;
         }

        if (!(*jvm_env)->IsInstanceOf(jvm_env, o_key, gref.string.class) ||
            !(*jvm_env)->IsInstanceOf(jvm_env, o_value, gref.boolean.class)) {
            PLUGIN_ERROR("State names must be strings and their values booleans.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_value);
            goto out;
        }

        jboolean b_value = (*jvm_env)->CallBooleanMethod(jvm_env, o_value,
                                                         gref.boolean.boolean_value);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_value);
        if ((*jvm_env)->ExceptionCheck(jvm_env)) {
            (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
            goto out;
        }

        const char *s_key = (*jvm_env)->GetStringUTFChars(jvm_env, o_key, 0);
        if (s_key == NULL) {
            PLUGIN_ERROR("GetStringUTFChars failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
            goto out;
        }

        state_set_add(set, s_key, b_value);

        (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_key, s_key);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
    }

    status = 0;

out:
    if (o_iterator != NULL)
        (*jvm_env)->DeleteLocalRef(jvm_env, o_iterator);
    if (o_set != NULL)
        (*jvm_env)->DeleteLocalRef(jvm_env, o_set);
    (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);

    return status;
}
