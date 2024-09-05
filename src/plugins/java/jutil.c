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
} jcached_method_t;

typedef struct {
    const char *name;
    jclass *class;
    jcached_method_t *methods;
} jcached_class_t;

static jcached_class_t classes[] = {
    { "java/util/ArrayList", &gref.array_list.class, (jcached_method_t[]) {
        { "<init>", "(I)V",                  &gref.array_list.constructor },
        { "add",    "(Ljava/lang/Object;)Z", &gref.array_list.add         },
        { NULL,     NULL,                    NULL                         }}},
    { "java/util/List", &gref.list.class, (jcached_method_t[]) {
        { "size",    "()I",                   &gref.list.size        },
        { "toArray", "()[Ljava/lang/Object;", &gref.list.to_array    },
        { NULL,      NULL,                    NULL                   }}},
    { "java/util/HashMap", &gref.hash_map.class, (jcached_method_t[]) {
        { "<init>", "(I)V", &gref.hash_map.constructor  },
        { "put",    "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;", &gref.hash_map.put },
        { NULL,     NULL,                     NULL        }}},
    { "java/lang/Boolean", &gref.boolean.class, (jcached_method_t[]) {
        { "<init>",       "(Z)V", &gref.boolean.constructor   },
        { "booleanValue", "()Z",  &gref.boolean.boolean_value },
        { NULL,           NULL,   NULL                        }}},
    { "java/lang/Long", &gref._long.class, (jcached_method_t[]) {
        { "<init>", "(J)V", &gref._long.constructor },
        { NULL,     NULL,   NULL                    }}},
    { "java/lang/Double", &gref._double.class, (jcached_method_t[]) {
        { "<init>", "(D)V", &gref._double.constructor },
        { NULL,     NULL,   NULL                      }}},
    { "java/util/Map", &gref.map.class, (jcached_method_t[]) {
        { "entrySet", "()Ljava/util/Set;", &gref.map.entry_set },
        { NULL,       NULL,                NULL                }}},
    { "java/util/Set", &gref.set.class, (jcached_method_t[]) {
        { "iterator", "()Ljava/util/Iterator;", &gref.set.iterator },
        { NULL,       NULL,                     NULL               }}},
    { "java/util/Iterator", &gref.iterator.class, (jcached_method_t[]) {
        { "hasNext", "()Z",                  &gref.iterator.has_next },
        { "next",    "()Ljava/lang/Object;", &gref.iterator.next      },
        { NULL,      NULL,                   NULL                     }}},
    { "java/util/Map$Entry", &gref.map_entry.class, (jcached_method_t[]) {
        { "getKey",   "()Ljava/lang/Object;", &gref.map_entry.get_key   },
        { "getValue", "()Ljava/lang/Object;", &gref.map_entry.get_value },
        { NULL,       NULL,                   NULL                      }}},
    { "org/ncollectd/api/Metric", &gref.metric.class,  (jcached_method_t[]) {
        { "setLabels",   "(Ljava/util/HashMap;)V", &gref.metric.set_labels },
        { "setTime",     "(J)V", &gref.metric.set_time     },
        { "setInterval", "(J)V", &gref.metric.set_interval },
        { "getLabels",   "()Ljava/util/HashMap;", &gref.metric.get_labels  },
        { "getTime",     "()J",  &gref.metric.get_time     },
        { "getInterval", "()J",  &gref.metric.get_interval },
        { NULL,          NULL,   NULL                      }}},
    { "org/ncollectd/api/MetricFamily", &gref.metric_family.class, (jcached_method_t[]) {
        { "<init>",     "(ILjava/lang/String;)V",        &gref.metric_family.constructor },
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
        { "<init>",    "(J)V", &gref.metric_counter.constructor_double },
        { "<init>",    "(D)V", &gref.metric_counter.constructor_long   },
        { "getType",   "()I",  &gref.metric_counter.get_type           },
        { "getDouble", "()D",  &gref.metric_counter.get_double         },
        { "getLong",   "()J",  &gref.metric_counter.get_long           },
        { NULL,        NULL,   NULL                                    }}},
    { "org/ncollectd/api/MetricInfo", &gref.metric_info.class, (jcached_method_t[]) {
        { "<init>",  "(Ljava/util/HashMap;)V", &gref.metric_info.constructor },
        { "getInfo", "()Ljava/util/HashMap;",  &gref.metric_info.get_info    },
        { NULL,      NULL,     NULL                                    }}},
    { "org/ncollectd/api/MetricStateSet", &gref.metric_state_set.class, (jcached_method_t[]) {
        { "<init>", "(Ljava/util/HashMap;)V", &gref.metric_state_set.constructor },
        { "getSet", "()Ljava/util/HashMap;",  &gref.metric_state_set.get_set     },
        { NULL,      NULL,                     NULL                    }}},
    { "org/ncollectd/api/MetricHistogram", &gref.metric_histogram.class, (jcached_method_t[]) {
        { "<init>",     "(DLjava/util/List;)V", &gref.metric_histogram.constructor },
        { "getSum",     "()D", &gref.metric_histogram.get_sum  },
        { "getBuckets", "()Ljava/util/List;", &gref.metric_histogram.get_buckets },
        { NULL,         NULL,                     NULL         }}},
    { "org/ncollectd/api/MetricHistogramBucket", &gref.metric_histogram_bucket.class, (jcached_method_t[]) {
        { "<init>",     "(JD)V", &gref.metric_histogram_bucket.constructor },
        { "getCounter", "()J",   &gref.metric_histogram_bucket.get_counter },
        { "getMaximum", "()D",   &gref.metric_histogram_bucket.get_maximum },
        { NULL,         NULL,    NULL                                      }}},
    { "org/ncollectd/api/MetricSummary", &gref.metric_summary.class, (jcached_method_t[]) {
        { "<init>",       "(DJLjava/util/List;)V", &gref.metric_summary.constructor },
        { "getSum",       "()D", &gref.metric_summary.get_sum   },
        { "getCount",     "()D", &gref.metric_summary.get_count },
        { "getQuantiles", "()Ljava/util/List;", &gref.metric_summary.get_quantiles },
        { NULL,           NULL,  NULL                           }}},
    { "org/ncollectd/api/MetricSummaryQuantile", &gref.metric_summary_quantile.class, (jcached_method_t[]) {
        { "<init>",      "(DD)V", &gref.metric_summary_quantile.constructor  },
        { "getQuantile", "()D",   &gref.metric_summary_quantile.get_quantile },
        { "getValue",    "()D",   &gref.metric_summary_quantile.get_value    },
        { NULL,          NULL,    NULL                                       }}},
    { "org/ncollectd/api/Notification", &gref.notification.class, (jcached_method_t[]) {
        { "<init>",         "()V",                      &gref.notification.constructor     },
        { "setName",        "(Ljava/lang/String;)V",    &gref.notification.set_name        },
        { "setTime",        "(J)V",                     &gref.notification.set_time        },
        { "setSeverity",    "(I)V",                     &gref.notification.set_severity    },
        { "setLabels",      "(Ljava/util/HashMap;)V", &gref.notification.set_labels      },
        { "setAnnotations", "(Ljava/util/HashMap;)V", &gref.notification.set_annotations },
        { "getName",        "()Ljava/lang/String;",     &gref.notification.get_name        },
        { "getTime",        "()J",                      &gref.notification.get_time        },
        { "getSeverity",    "()I",                      &gref.notification.get_severity    },
        { "getLabels",      "()Ljava/util/HashMap;",  &gref.notification.get_labels      },
        { "getAnnotations", "()Ljava/util/HashMap;",  &gref.notification.get_annotations },
        { NULL,             NULL,                       NULL                               }}},
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
            PLUGIN_ERROR("Looking up the '%s' class failed.", classes[i].name);
            return -1;
        }

        jcached_method_t *methods = classes[i].methods;
        for (size_t j = 0; methods[j].name != NULL; j++) {
            jmethodID method_id = (*jvm_env)->GetMethodID (jvm_env, class,
                                                           methods[j].name, methods[j].signature);
            if (method_id == NULL) {
                PLUGIN_ERROR ("Looking method '%s' with signatute '%s' in class '%s' failed.",
                              methods[j].name, methods[j].signature, classes[i].name);
                return -1;
            }
            *(methods[j].method_id) = method_id;
        }

        jclass global_class = (*jvm_env)->NewGlobalRef(jvm_env, class);
        if (global_class == NULL) {
            PLUGIN_ERROR ("Global class reference create failed for '%s'.", classes[i].name);
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

    return 0;
}

/* Convert a jlong to a java.lang.Number */
jobject ctoj_jlong_to_number(JNIEnv *jvm_env, jlong value)
{
    return (*jvm_env)->NewObject(jvm_env, gref._long.class, gref._long.constructor, value);
}

/* Convert a jdouble to a java.lang.Number */
jobject ctoj_jdouble_to_number(JNIEnv *jvm_env, jdouble value)
{
    return (*jvm_env)->NewObject(jvm_env, gref._double.class, gref._double.constructor, value);
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

        jobject put_obj = (*jvm_env)->CallObjectMethod(jvm_env, o_hash, gref.hash_map.put,
                                                                o_name, o_value);
        if (put_obj == NULL) {
            PLUGIN_ERROR("CallObjectMethod to put in HashTable failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_name);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_value);
            return NULL;
        }

        (*jvm_env)->DeleteLocalRef(jvm_env, o_name);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_value);
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

        jobject o_value = (*jvm_env)->NewObject(jvm_env, gref.boolean.class,
                                                         gref.boolean.constructor,
                                                         (jboolean)state->enabled);
        if (o_value == NULL) {
            PLUGIN_ERROR("Failed to create Boolean object.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_name);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);
            return NULL;
        }

        jobject put_obj = (*jvm_env)->CallObjectMethod(jvm_env, o_hash, gref.hash_map.put,
                                                                        o_name, o_value);
        if (put_obj == NULL) {
            PLUGIN_ERROR("CallObjectMethod to put in HashTable failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_name);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_value);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);
            return NULL;
        }

        (*jvm_env)->DeleteLocalRef(jvm_env, o_name);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_value);
    }

    return o_hash;
}

/* Call a `String <method> ()' method. */
int jtoc_string(JNIEnv *jvm_env, char **str, jobject object_ptr, jmethodID method_id)
{
    jobject string_obj = (*jvm_env)->CallObjectMethod(jvm_env, object_ptr, method_id);
    if (string_obj == NULL) {
        *str = NULL;
        return 0;
    }

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

int jtoc_label_set(JNIEnv *jvm_env, label_set_t *label, jobject object_ptr, jmethodID method_id)
{
    jobject o_hash = (*jvm_env)->CallObjectMethod(jvm_env, object_ptr, method_id);
    if (o_hash == NULL) {
        PLUGIN_ERROR("CallObjectMethod failed.");
        return -1;
    }

    jobject o_set = (*jvm_env)->CallObjectMethod(jvm_env, o_hash, gref.map.entry_set);
    if (o_set == NULL) {
        PLUGIN_ERROR("CallObjectMethod (entrySet) failed.");
        return -1;
    }

    jobject o_iterator = (*jvm_env)->CallObjectMethod(jvm_env, o_set, gref.set.iterator);
    if (o_iterator == NULL) {
        (*jvm_env)->DeleteLocalRef(jvm_env, o_set);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);
        PLUGIN_ERROR("CallObjectMethod (iterator) failed.");
        return -1;
    }

    while ((*jvm_env)->CallBooleanMethod(jvm_env, o_iterator, gref.iterator.has_next)) {
        jobject o_entry = (*jvm_env)->CallObjectMethod(jvm_env, o_iterator, gref.iterator.next);
        if (o_entry == NULL) {
            PLUGIN_ERROR("CallObjectMethod (next) failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_iterator);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_set);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);
            return -1;
        }

        jobject o_key = (*jvm_env)->CallObjectMethod(jvm_env, o_entry, gref.map_entry.get_key);
        if (o_key == NULL) {
            (*jvm_env)->DeleteLocalRef(jvm_env, o_entry);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_iterator);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_set);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);
            PLUGIN_ERROR("CallObjectMethod (getKey) failed.");
            return -1;
        }

        const char *s_key = (*jvm_env)->GetStringUTFChars(jvm_env, o_key, 0);
        if (s_key == NULL) {
            PLUGIN_ERROR("GetStringUTFChars failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_entry);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_iterator);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_set);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);
            return -1;
        }

        jobject o_value = (*jvm_env)->CallObjectMethod(jvm_env, o_entry, gref.map_entry.get_value);
        if (o_value == NULL) {
            PLUGIN_ERROR("CallObjectMethod (getValue) failed.");
            (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_key, s_key);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_entry);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_iterator);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_set);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);
            return -1;
        }

        const char *s_value = (*jvm_env)->GetStringUTFChars(jvm_env, o_value, 0);
        if (s_value == NULL) {
            PLUGIN_ERROR("GetStringUTFChars failed.");
            (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_key, s_key);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_value);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_entry);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_iterator);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_set);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);
            return -1;
        }

        label_set_add(label, true, s_key, s_value);

        (*jvm_env)->DeleteLocalRef(jvm_env, o_entry);
        (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_key, s_key);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
        (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_value, s_value);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_value);

    }

    (*jvm_env)->DeleteLocalRef(jvm_env, o_iterator);
    (*jvm_env)->DeleteLocalRef(jvm_env, o_set);
    (*jvm_env)->DeleteLocalRef(jvm_env, o_hash);

    return 0;
}

int jtoc_state_set(JNIEnv *jvm_env, state_set_t *set, jobject object_ptr, jmethodID method_id)
{
    jobject o_hash = (*jvm_env)->CallObjectMethod(jvm_env, object_ptr, method_id);
    if (o_hash == NULL) {
        PLUGIN_ERROR("CallObjectMethod failed.");
        return -1;
    }

    jobject o_set = (*jvm_env)->CallObjectMethod(jvm_env, o_hash, gref.map.entry_set);
    if (o_set == NULL) {
        PLUGIN_ERROR("CallObjectMethod (entrySet) failed.");
        return -1;
    }

    jobject o_iterator = (*jvm_env)->CallObjectMethod(jvm_env, o_set, gref.set.iterator);
    if (o_iterator == NULL) {
        PLUGIN_ERROR("CallObjectMethod (iterator) failed.");
        return -1;
    }

    while ((*jvm_env)->CallBooleanMethod(jvm_env, o_iterator, gref.iterator.has_next)) {
        jobject o_entry = (*jvm_env)->CallObjectMethod(jvm_env, o_iterator, gref.iterator.next);
        if (o_entry == NULL) {
            PLUGIN_ERROR("CallObjectMethod (next) failed.");
            return -1;
        }

        jobject o_key = (*jvm_env)->CallObjectMethod(jvm_env, o_entry, gref.map_entry.get_key);
        if (o_key == NULL) {
            (*jvm_env)->DeleteLocalRef(jvm_env, o_entry);
            PLUGIN_ERROR("CallObjectMethod (getKey) failed.");
            return -1;
        }

        const char *s_key = (*jvm_env)->GetStringUTFChars(jvm_env, o_key, 0);
        if (s_key == NULL) {
            PLUGIN_ERROR("GetStringUTFChars failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_entry);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
            return -1;
        }

        jobject o_value = (*jvm_env)->CallObjectMethod(jvm_env, o_entry, gref.map_entry.get_value);
        if (o_value == NULL) {
            PLUGIN_ERROR("CallObjectMethod (getValue) failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_entry);
            (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_key, s_key);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
            return -1;
        }

        jboolean b_value = (*jvm_env)->CallBooleanMethod(jvm_env, o_value,
                                                                  gref.boolean.boolean_value);

        state_set_add(set, s_key, b_value);

        (*jvm_env)->DeleteLocalRef(jvm_env, o_entry);
        (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_key, s_key);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_value);

    }

    return 0;
}
