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

/* Convert a config_value_t to a org/ncollectd/api/ConfigValue */
jobject ctoj_config_value(JNIEnv *jvm_env, config_value_t ocvalue)
{
    jmethodID m_ocvalue_constructor = NULL;
    jobject o_argument = NULL;

    if (ocvalue.type == CONFIG_TYPE_BOOLEAN) {
        jboolean tmp_boolean = (ocvalue.value.boolean == 0) ? JNI_FALSE : JNI_TRUE;
        m_ocvalue_constructor = gref.config_value.constructor_bool;
        return (*jvm_env)->NewObject(jvm_env, gref.config_value.class, m_ocvalue_constructor,
                                              tmp_boolean);
    } else if (ocvalue.type == CONFIG_TYPE_STRING) {
        m_ocvalue_constructor = gref.config_value.constructor_string;
        o_argument = (*jvm_env)->NewStringUTF(jvm_env, ocvalue.value.string);
        if (o_argument == NULL) {
            PLUGIN_ERROR("Creating a String object failed.");
            return NULL;
        }
    } else if (ocvalue.type == CONFIG_TYPE_NUMBER) {
        m_ocvalue_constructor = gref.config_value.constructor_number;
        o_argument = ctoj_jdouble_to_number(jvm_env, (jdouble)ocvalue.value.number);
        if (o_argument == NULL) {
            PLUGIN_ERROR("Creating a Number object failed.");
            return NULL;
        }
    } else {
        return NULL;
    }

    assert(m_ocvalue_constructor != NULL);
    assert(o_argument != NULL);

    jobject o_ocvalue = (*jvm_env)->NewObject(jvm_env, gref.config_value.class,
                                                       m_ocvalue_constructor,
                                                       o_argument);
    if (o_ocvalue == NULL) {
        PLUGIN_ERROR("Creating an ConfigValue object failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_argument);
        return NULL;
    }

    (*jvm_env)->DeleteLocalRef(jvm_env, o_argument);
    return o_ocvalue;
}

/* Convert a config_item_t to a org/ncollectd/api/ConfigItem */
jobject ctoj_config_item(JNIEnv *jvm_env, const config_item_t *ci)
{
    /* Create a String object with the key.
     * Needed for calling the constructor. */
    jobject o_key = (*jvm_env)->NewStringUTF(jvm_env, ci->key);
    if (o_key == NULL) {
        PLUGIN_ERROR("Creating String object failed.");
        return NULL;
    }

    /* Create an ConfigItem object */
    jobject o_ocitem = (*jvm_env)->NewObject(jvm_env, gref.config_item.class,
                                                      gref.config_item.constructor, o_key);
    if (o_ocitem == NULL) {
        PLUGIN_ERROR("Creating an ConfigItem object failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
        return NULL;
    }

    /* We don't need the String object any longer.. */
    (*jvm_env)->DeleteLocalRef(jvm_env, o_key);

    /* Call ConfigItem.addValue for each value */
    for (int i = 0; i < ci->values_num; i++) {
        jobject o_value = ctoj_config_value(jvm_env, ci->values[i]);
        if (o_value == NULL) {
            PLUGIN_ERROR("Creating an ConfigValue object failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_ocitem);
            return NULL;
        }

        (*jvm_env)->CallVoidMethod(jvm_env, o_ocitem, gref.config_item.add_value, o_value);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_value);
    }

    /* Call ConfigItem.addChild for each child */
    for (int i = 0; i < ci->children_num; i++) {
        jobject o_child = ctoj_config_item(jvm_env, ci->children + i);
        if (o_child == NULL) {
            PLUGIN_ERROR("Creating an ConfigItem object failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_ocitem);
            return NULL;
        }

        (*jvm_env)->CallVoidMethod(jvm_env, o_ocitem, gref.config_item.add_child, o_child);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_child);
    }

    return o_ocitem;
}
