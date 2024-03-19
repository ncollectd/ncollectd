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

struct cjni_jvm_env_s {
    JNIEnv *jvm_env;
    int reference_counter;
};
typedef struct cjni_jvm_env_s cjni_jvm_env_t;

struct java_plugin_class_s {
    char *name;
    jclass class;
    jobject object;
};
typedef struct java_plugin_class_s java_plugin_class_t;

#define CB_TYPE_CONFIG 1
#define CB_TYPE_INIT 2
#define CB_TYPE_READ 3
#define CB_TYPE_WRITE 4
#define CB_TYPE_FLUSH 5
#define CB_TYPE_SHUTDOWN 6
#define CB_TYPE_LOG 7
#define CB_TYPE_NOTIFICATION 8

struct cjni_callback_info_s {
    char *name;
    int type;
    jclass class;
    jobject object;
    jmethodID method;
};
typedef struct cjni_callback_info_s cjni_callback_info_t;

/*
 * Global variables
 */
static JavaVM *jvm;
static pthread_key_t jvm_env_key;

/* Configuration options for the JVM. */
static char **jvm_argv;
static size_t jvm_argc;

/* List of class names to load */
static java_plugin_class_t *java_classes_list;
static size_t java_classes_list_len;

/* List of config, init, and shutdown callbacks. */
static cjni_callback_info_t *java_callbacks;
static size_t java_callbacks_num;
static pthread_mutex_t java_callbacks_lock = PTHREAD_MUTEX_INITIALIZER;

static config_item_t *config_block;

/*
 * Prototypes
 *
 * Mostly functions that are needed by the Java interface (``native'')
 * functions.
 */
static void cjni_callback_info_destroy(void *arg);
static cjni_callback_info_t *cjni_callback_info_create(JNIEnv *jvm_env, jobject o_name,
                                                       jobject o_callback, int type);
static int cjni_callback_register(JNIEnv *jvm_env, jobject o_name, jobject o_callback, int type);
static int cjni_read(user_data_t *user_data);
static int cjni_write(metric_family_t const *fam, user_data_t *ud);
// static int cjni_flush(cdtime_t timeout, user_data_t *ud);
static void cjni_log(const log_msg_t *msg, user_data_t *ud);
static int cjni_notification(const notification_t *n, user_data_t *ud);

/*
 * C to Java conversion functions
 */
#if 0
static int ctoj_string(JNIEnv *jvm_env, const char *string, jclass class_ptr, jobject object_ptr,
                                        const char *method_name)
{
    jmethodID m_set;
    jstring o_string;

    /* Create a java.lang.String */
    o_string = (*jvm_env)->NewStringUTF(jvm_env, (string != NULL) ? string : "");
    if (o_string == NULL) {
        PLUGIN_ERROR("ctoj_string: NewStringUTF failed.");
        return -1;
    }

    /* Search for the `void setFoo (String s)' method. */
    m_set = (*jvm_env)->GetMethodID(jvm_env, class_ptr, method_name, "(Ljava/lang/String;)V");
    if (m_set == NULL) {
        PLUGIN_ERROR("ctoj_string: Cannot find method `void %s (String)'.", method_name);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_string);
        return -1;
    }

    /* Call the method. */
    (*jvm_env)->CallVoidMethod(jvm_env, object_ptr, m_set, o_string);

    /* Decrease reference counter on the java.lang.String object. */
    (*jvm_env)->DeleteLocalRef(jvm_env, o_string);

    return 0;
}
#endif

#if 0
static jstring ctoj_output_string(JNIEnv *jvm_env, const char *string)
{
    jstring o_string;

    /* Create a java.lang.String */
    o_string = (*jvm_env)->NewStringUTF(jvm_env, (string != NULL) ? string : "");
    if (o_string == NULL) {
        PLUGIN_ERROR("NewStringUTF failed.");
        return NULL;
    }

    return o_string;
}
#endif

static int ctoj_int(JNIEnv *jvm_env, jint value, jclass class_ptr, jobject object_ptr,
                                     const char *method_name)
{
    jmethodID m_set;

    /* Search for the `void setFoo (int i)' method. */
    m_set = (*jvm_env)->GetMethodID(jvm_env, class_ptr, method_name, "(I)V");
    if (m_set == NULL) {
        PLUGIN_ERROR("Cannot find method `void %s (int)'.", method_name);
        return -1;
    }

    (*jvm_env)->CallVoidMethod(jvm_env, object_ptr, m_set, value);

    return 0;
}

static int ctoj_long(JNIEnv *jvm_env, jlong value, jclass class_ptr, jobject object_ptr,
                                      const char *method_name)
{
    jmethodID m_set;

    /* Search for the `void setFoo (long l)' method. */
    m_set = (*jvm_env)->GetMethodID(jvm_env, class_ptr, method_name, "(J)V");
    if (m_set == NULL) {
        PLUGIN_ERROR("Cannot find method `void %s (long)'.", method_name);
        return -1;
    }

    (*jvm_env)->CallVoidMethod(jvm_env, object_ptr, m_set, value);

    return 0;
}
#if 0
static int ctoj_double(JNIEnv *jvm_env, jdouble value, jclass class_ptr, jobject object_ptr,
                                        const char *method_name)
{
    jmethodID m_set;

    /* Search for the `void setFoo (double d)' method. */
    m_set = (*jvm_env)->GetMethodID(jvm_env, class_ptr, method_name, "(D)V");
    if (m_set == NULL) {
        PLUGIN_ERROR("Cannot find method `void %s (double)'.", method_name);
        return -1;
    }

    (*jvm_env)->CallVoidMethod(jvm_env, object_ptr, m_set, value);

    return 0;
}

/* Convert a jlong to a java.lang.Number */
static jobject ctoj_jlong_to_number(JNIEnv *jvm_env, jlong value)
{
    jclass c_long;
    jmethodID m_long_constructor;

    /* Look up the java.lang.Long class */
    c_long = (*jvm_env)->FindClass(jvm_env, "java/lang/Long");
    if (c_long == NULL) {
        PLUGIN_ERROR("Looking up the java.lang.Long class failed.");
        return NULL;
    }

    m_long_constructor = (*jvm_env)->GetMethodID(jvm_env, c_long, "<init>", "(J)V");
    if (m_long_constructor == NULL) {
        PLUGIN_ERROR("Looking up the 'Long (long)' constructor failed.");
        return NULL;
    }

    return (*jvm_env)->NewObject(jvm_env, c_long, m_long_constructor, value);
}
#endif
/* Convert a jdouble to a java.lang.Number */
static jobject ctoj_jdouble_to_number(JNIEnv *jvm_env, jdouble value)
{
    jclass c_double;
    jmethodID m_double_constructor;

    /* Look up the java.lang.Long class */
    c_double = (*jvm_env)->FindClass(jvm_env, "java/lang/Double");
    if (c_double == NULL) {
        PLUGIN_ERROR("Looking up the java.lang.Double class failed.");
        return NULL;
    }

    m_double_constructor = (*jvm_env)->GetMethodID(jvm_env, c_double, "<init>", "(D)V");
    if (m_double_constructor == NULL) {
        PLUGIN_ERROR("Looking up the 'Double (double)' constructor failed.");
        return NULL;
    }

    return (*jvm_env)->NewObject(jvm_env, c_double, m_double_constructor, value);
}

#if 0
/* Convert a value_t to a java.lang.Number */
static jobject ctoj_value_to_number(JNIEnv *jvm_env, value_t value, int ds_type)
{
    if (ds_type == DS_TYPE_COUNTER)
        return ctoj_jlong_to_number(jvm_env, (jlong)value.counter);
    else if (ds_type == DS_TYPE_GAUGE)
        return ctoj_jdouble_to_number(jvm_env, (jdouble)value.gauge);
    if (ds_type == DS_TYPE_DERIVE)
        return ctoj_jlong_to_number(jvm_env, (jlong)value.derive);
    else
        return NULL;
}

/* Convert a data_source_t to a org/ncollectd/api/DataSource */
static jobject ctoj_data_source(JNIEnv *jvm_env, const data_source_t *dsrc)
{
    jclass c_datasource;
    jmethodID m_datasource_constructor;
    jobject o_datasource;
    int status;

    /* Look up the DataSource class */
    c_datasource = (*jvm_env)->FindClass(jvm_env, "org/ncollectd/api/DataSource");
    if (c_datasource == NULL) {
        PLUGIN_ERROR("FindClass (org/ncollectd/api/DataSource) failed.");
        return NULL;
    }

    /* Lookup the `ValueList ()' constructor. */
    m_datasource_constructor = (*jvm_env)->GetMethodID(jvm_env, c_datasource, "<init>", "()V");
    if (m_datasource_constructor == NULL) {
        PLUGIN_ERROR("Cannot find the `DataSource ()' constructor.");
        return NULL;
    }

    /* Create a new instance. */
    o_datasource = (*jvm_env)->NewObject(jvm_env, c_datasource, m_datasource_constructor);
    if (o_datasource == NULL) {
        PLUGIN_ERROR("Creating a new DataSource instance failed.");
        return NULL;
    }

    /* Set name via `void setName (String name)' */
    status = ctoj_string(jvm_env, dsrc->name, c_datasource, o_datasource, "setName");
    if (status != 0) {
        PLUGIN_ERROR("ctoj_string (setName) failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_datasource);
        return NULL;
    }

    /* Set type via `void setType (int type)' */
    status = ctoj_int(jvm_env, dsrc->type, c_datasource, o_datasource, "setType");
    if (status != 0) {
        PLUGIN_ERROR("ctoj_int (setType) failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_datasource);
        return NULL;
    }

    /* Set min via `void setMin (double min)' */
    status = ctoj_double(jvm_env, dsrc->min, c_datasource, o_datasource, "setMin");
    if (status != 0) {
        PLUGIN_ERROR("ctoj_double (setMin) failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_datasource);
        return NULL;
    }

    /* Set max via `void setMax (double max)' */
    status = ctoj_double(jvm_env, dsrc->max, c_datasource, o_datasource, "setMax");
    if (status != 0) {
        PLUGIN_ERROR("ctoj_double (setMax) failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_datasource);
        return NULL;
    }

    return o_datasource;
}
#endif

/* Convert a config_value_t to a org/ncollectd/api/OConfigValue */
static jobject ctoj_config_value(JNIEnv *jvm_env, config_value_t ocvalue)
{
    jclass c_ocvalue;
    jmethodID m_ocvalue_constructor;
    jobject o_argument;
    jobject o_ocvalue;

    m_ocvalue_constructor = NULL;
    o_argument = NULL;

    c_ocvalue = (*jvm_env)->FindClass(jvm_env, "org/ncollectd/api/OConfigValue");
    if (c_ocvalue == NULL) {
        PLUGIN_ERROR("FindClass (org/ncollectd/api/OConfigValue) failed.");
        return NULL;
    }

    if (ocvalue.type == CONFIG_TYPE_BOOLEAN) {
        jboolean tmp_boolean;

        tmp_boolean = (ocvalue.value.boolean == 0) ? JNI_FALSE : JNI_TRUE;

        m_ocvalue_constructor =
                (*jvm_env)->GetMethodID(jvm_env, c_ocvalue, "<init>", "(Z)V");
        if (m_ocvalue_constructor == NULL) {
            PLUGIN_ERROR("Cannot find the 'OConfigValue (boolean)' constructor.");
            return NULL;
        }

        return (*jvm_env)->NewObject(jvm_env, c_ocvalue, m_ocvalue_constructor,
                                                                 tmp_boolean);
    } /* if (ocvalue.type == CONFIG_TYPE_BOOLEAN) */
    else if (ocvalue.type == CONFIG_TYPE_STRING) {
        m_ocvalue_constructor = (*jvm_env)->GetMethodID(jvm_env, c_ocvalue, "<init>",
                                                                 "(Ljava/lang/String;)V");
        if (m_ocvalue_constructor == NULL) {
            PLUGIN_ERROR("Cannot find the `OConfigValue (String)' constructor.");
            return NULL;
        }

        o_argument = (*jvm_env)->NewStringUTF(jvm_env, ocvalue.value.string);
        if (o_argument == NULL) {
            PLUGIN_ERROR("Creating a String object failed.");
            return NULL;
        }
    } else if (ocvalue.type == CONFIG_TYPE_NUMBER) {
        m_ocvalue_constructor = (*jvm_env)->GetMethodID(jvm_env, c_ocvalue, "<init>",
                                                                 "(Ljava/lang/Number;)V");
        if (m_ocvalue_constructor == NULL) {
            PLUGIN_ERROR("Cannot find the `OConfigValue (Number)' constructor.");
            return NULL;
        }

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

    o_ocvalue = (*jvm_env)->NewObject(jvm_env, c_ocvalue, m_ocvalue_constructor, o_argument);
    if (o_ocvalue == NULL) {
        PLUGIN_ERROR("Creating an OConfigValue object failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_argument);
        return NULL;
    }

    (*jvm_env)->DeleteLocalRef(jvm_env, o_argument);
    return o_ocvalue;
}

/* Convert a config_item_t to a org/ncollectd/api/OConfigItem */
static jobject ctoj_config_item(JNIEnv *jvm_env, const config_item_t *ci)
{
    jclass c_ocitem;
    jmethodID m_ocitem_constructor;
    jmethodID m_addvalue;
    jmethodID m_addchild;
    jobject o_key;
    jobject o_ocitem;

    c_ocitem = (*jvm_env)->FindClass(jvm_env, "org/ncollectd/api/OConfigItem");
    if (c_ocitem == NULL) {
        PLUGIN_ERROR("FindClass (org/ncollectd/api/OConfigItem) failed.");
        return NULL;
    }

    /* Get the required methods: m_ocitem_constructor, m_addvalue, and m_addchild */
    m_ocitem_constructor = (*jvm_env)->GetMethodID(jvm_env, c_ocitem, "<init>", "(Ljava/lang/String;)V");
    if (m_ocitem_constructor == NULL) {
        PLUGIN_ERROR("Cannot find the 'OConfigItem (String)' constructor.");
        return NULL;
    }

    m_addvalue = (*jvm_env)->GetMethodID(jvm_env, c_ocitem, "addValue", "(Lorg/ncollectd/api/OConfigValue;)V");
    if (m_addvalue == NULL) {
        PLUGIN_ERROR("Cannot find the 'addValue (OConfigValue)' method.");
        return NULL;
    }

    m_addchild = (*jvm_env)->GetMethodID(jvm_env, c_ocitem, "addChild", "(Lorg/ncollectd/api/OConfigItem;)V");
    if (m_addchild == NULL) {
        PLUGIN_ERROR("Cannot find the 'addChild (OConfigItem)' method.");
        return NULL;
    }
    /* Create a String object with the key.
     * Needed for calling the constructor. */
    o_key = (*jvm_env)->NewStringUTF(jvm_env, ci->key);
    if (o_key == NULL) {
        PLUGIN_ERROR("Creating String object failed.");
        return NULL;
    }

    /* Create an OConfigItem object */
    o_ocitem = (*jvm_env)->NewObject(jvm_env, c_ocitem, m_ocitem_constructor, o_key);
    if (o_ocitem == NULL) {
        PLUGIN_ERROR("Creating an OConfigItem object failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_key);
        return NULL;
    }

    /* We don't need the String object any longer.. */
    (*jvm_env)->DeleteLocalRef(jvm_env, o_key);

    /* Call OConfigItem.addValue for each value */
    for (int i = 0; i < ci->values_num; i++) /* {{{ */
    {
        jobject o_value;

        o_value = ctoj_config_value(jvm_env, ci->values[i]);
        if (o_value == NULL) {
            PLUGIN_ERROR("Creating an OConfigValue object failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_ocitem);
            return NULL;
        }

        (*jvm_env)->CallVoidMethod(jvm_env, o_ocitem, m_addvalue, o_value);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_value);
    }

    /* Call OConfigItem.addChild for each child */
    for (int i = 0; i < ci->children_num; i++) /* {{{ */
    {
        jobject o_child;

        o_child = ctoj_config_item(jvm_env, ci->children + i);
        if (o_child == NULL) {
            PLUGIN_ERROR("Creating an OConfigItem object failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_ocitem);
            return NULL;
        }

        (*jvm_env)->CallVoidMethod(jvm_env, o_ocitem, m_addchild, o_child);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_child);
    }

    return o_ocitem;
}
#if 0
/* Convert a data_set_t to a org/ncollectd/api/DataSet */
static jobject ctoj_data_set(JNIEnv *jvm_env, const data_set_t *ds)
{
    jclass c_dataset;
    jmethodID m_constructor;
    jmethodID m_add;
    jobject o_type;
    jobject o_dataset;

    /* Look up the org/ncollectd/api/DataSet class */
    c_dataset = (*jvm_env)->FindClass(jvm_env, "org/ncollectd/api/DataSet");
    if (c_dataset == NULL) {
        PLUGIN_ERROR("Looking up the org/ncollectd/api/DataSet class failed.");
        return NULL;
    }

    /* Search for the `DataSet (String type)' constructor. */
    m_constructor = (*jvm_env)->GetMethodID(jvm_env, c_dataset, "<init>", "(Ljava/lang/String;)V");
    if (m_constructor == NULL) {
        PLUGIN_ERROR("Looking up the 'DataSet (String)' constructor failed.");
        return NULL;
    }

    /* Search for the `void addDataSource (DataSource)' method. */
    m_add = (*jvm_env)->GetMethodID(jvm_env, c_dataset, "addDataSource", "(Lorg/ncollectd/api/DataSource;)V");
    if (m_add == NULL) {
        PLUGIN_ERROR("Looking up the 'addDataSource (DataSource)' method failed.");
        return NULL;
    }

    o_type = (*jvm_env)->NewStringUTF(jvm_env, ds->type);
    if (o_type == NULL) {
        PLUGIN_ERROR("Creating a String object failed.");
        return NULL;
    }

    o_dataset = (*jvm_env)->NewObject(jvm_env, c_dataset, m_constructor, o_type);
    if (o_dataset == NULL) {
        PLUGIN_ERROR("Creating a DataSet object failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_type);
        return NULL;
    }

    /* Decrease reference counter on the java.lang.String object. */
    (*jvm_env)->DeleteLocalRef(jvm_env, o_type);

    for (size_t i = 0; i < ds->ds_num; i++) {
        jobject o_datasource;

        o_datasource = ctoj_data_source(jvm_env, ds->ds + i);
        if (o_datasource == NULL) {
            PLUGIN_ERROR("ctoj_data_source (%s.%s) failed", ds->type, ds->ds[i].name);
            (*jvm_env)->DeleteLocalRef(jvm_env, o_dataset);
            return NULL;
        }

        (*jvm_env)->CallVoidMethod(jvm_env, o_dataset, m_add, o_datasource);

        (*jvm_env)->DeleteLocalRef(jvm_env, o_datasource);
    }

    return o_dataset;
}
#endif
#if 0
static int ctoj_value_list_add_value(JNIEnv *jvm_env, value_t value, int ds_type,
                                                      jclass class_ptr, jobject object_ptr)
{
    jmethodID m_addvalue;
    jobject o_number;

    m_addvalue = (*jvm_env)->GetMethodID(jvm_env, class_ptr, "addValue", "(Ljava/lang/Number;)V");
    if (m_addvalue == NULL) {
        PLUGIN_ERROR("Cannot find method `void addValue (Number)'.");
        return -1;
    }

    o_number = ctoj_value_to_number(jvm_env, value, ds_type);
    if (o_number == NULL) {
        PLUGIN_ERROR("ctoj_value_to_number failed.");
        return -1;
    }

    (*jvm_env)->CallVoidMethod(jvm_env, object_ptr, m_addvalue, o_number);

    (*jvm_env)->DeleteLocalRef(jvm_env, o_number);

    return 0;
}
#endif
#if 0
static int ctoj_value_list_add_data_set(JNIEnv *jvm_env, jclass c_valuelist, jobject o_valuelist,
                                                         const data_set_t *ds)
{
    jmethodID m_setdataset;
    jobject o_dataset;

    /* Look for the `void setDataSource (List<DataSource> ds)' method. */
    m_setdataset = (*jvm_env)->GetMethodID(jvm_env, c_valuelist, "setDataSet",
                                                                 "(Lorg/ncollectd/api/DataSet;)V");
    if (m_setdataset == NULL) {
        PLUGIN_ERROR("Cannot find the `void setDataSet (DataSet)' method.");
        return -1;
    }

    /* Create a DataSet object. */
    o_dataset = ctoj_data_set(jvm_env, ds);
    if (o_dataset == NULL) {
        PLUGIN_ERROR("ctoj_data_set (%s) failed.", ds->type);
        return -1;
    }

    /* Actually call the method. */
    (*jvm_env)->CallVoidMethod(jvm_env, o_valuelist, m_setdataset, o_dataset);

    /* Decrease reference counter on the List<DataSource> object. */
    (*jvm_env)->DeleteLocalRef(jvm_env, o_dataset);

    return 0;
}

/* Convert a value_list_t (and data_set_t) to a org/ncollectd/api/ValueList */
static jobject ctoj_value_list(JNIEnv *jvm_env, const data_set_t *ds, const value_list_t *vl)
{
    jclass c_valuelist;
    jmethodID m_valuelist_constructor;
    jobject o_valuelist;
    int status;

    /* First, create a new ValueList instance..
     * Look up the class.. */
    c_valuelist = (*jvm_env)->FindClass(jvm_env, "org/ncollectd/api/ValueList");
    if (c_valuelist == NULL) {
        PLUGIN_ERROR("FindClass (org/ncollectd/api/ValueList) failed.");
        return NULL;
    }

    /* Lookup the `ValueList ()' constructor. */
    m_valuelist_constructor = (*jvm_env)->GetMethodID(jvm_env, c_valuelist, "<init>", "()V");
    if (m_valuelist_constructor == NULL) {
        PLUGIN_ERROR("Cannot find the 'ValueList ()' constructor.");
        return NULL;
    }

    /* Create a new instance. */
    o_valuelist = (*jvm_env)->NewObject(jvm_env, c_valuelist, m_valuelist_constructor);
    if (o_valuelist == NULL) {
        PLUGIN_ERROR("Creating a new ValueList instance failed.");
        return NULL;
    }

    status = ctoj_value_list_add_data_set(jvm_env, c_valuelist, o_valuelist, ds);
    if (status != 0) {
        PLUGIN_ERROR("ctoj_value_list_add_data_set failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_valuelist);
        return NULL;
    }

/* Set the strings.. */
#define SET_STRING(str, method_name)                                               \
    do {                                                                           \
        status = ctoj_string(jvm_env, str, c_valuelist, o_valuelist, method_name); \
        if (status != 0) {                                                         \
            PLUGIN_ERROR("ctoj_string (%s) failed.", method_name);                 \
            (*jvm_env)->DeleteLocalRef(jvm_env, o_valuelist);                      \
            return NULL;                                                           \
        }                                                                          \
    } while (0)

    SET_STRING(vl->host, "setHost");
    SET_STRING(vl->plugin, "setPlugin");
    SET_STRING(vl->plugin_instance, "setPluginInstance");
    SET_STRING(vl->type, "setType");
    SET_STRING(vl->type_instance, "setTypeInstance");

#undef SET_STRING

    /* Set the `time' member. Java stores time in milliseconds. */
    status = ctoj_long(jvm_env, (jlong)CDTIME_T_TO_MS(vl->time), c_valuelist, o_valuelist, "setTime");
    if (status != 0) {
        PLUGIN_ERROR("ctoj_long (setTime) failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_valuelist);
        return NULL;
    }

    /* Set the `interval' member.. */
    status = ctoj_long(jvm_env, (jlong)CDTIME_T_TO_MS(vl->interval), c_valuelist, o_valuelist, "setInterval");
    if (status != 0) {
        PLUGIN_ERROR("ctoj_long (setInterval) failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_valuelist);
        return NULL;
    }

#if 0
    for (size_t i = 0; i < vl->values_len; i++) {
        status = ctoj_value_list_add_value(jvm_env, vl->values[i], ds->ds[i].type, c_valuelist, o_valuelist);
        if (status != 0) {
            PLUGIN_ERROR("ctoj_value_list_add_value failed.");
            (*jvm_env)->DeleteLocalRef(jvm_env, o_valuelist);
            return NULL;
        }
    }
#endif

    return o_valuelist;
}
#endif

/* Convert a notification_t to a org/ncollectd/api/Notification */
static jobject ctoj_notification(JNIEnv *jvm_env, const notification_t *n)
{
    jclass c_notification;
    jmethodID m_constructor;
    jobject o_notification;
    int status;

    /* First, create a new Notification instance..
     * Look up the class.. */
    c_notification = (*jvm_env)->FindClass(jvm_env, "org/ncollectd/api/Notification");
    if (c_notification == NULL) {
        PLUGIN_ERROR("FindClass (org/ncollectd/api/Notification) failed.");
        return NULL;
    }

    /* Lookup the `Notification ()' constructor. */
    m_constructor = (*jvm_env)->GetMethodID(jvm_env, c_notification, "<init>", "()V");
    if (m_constructor == NULL) {
        PLUGIN_ERROR("Cannot find the 'Notification ()' constructor.");
        return NULL;
    }

    /* Create a new instance. */
    o_notification =
            (*jvm_env)->NewObject(jvm_env, c_notification, m_constructor);
    if (o_notification == NULL) {
        PLUGIN_ERROR("Creating a new Notification instance failed.");
        return NULL;
    }
#if 0
/* Set the strings.. */
#define SET_STRING(str, method_name)                                       \
    do {                                                                   \
        status = ctoj_string(jvm_env, str, c_notification, o_notification, \
                                                 method_name);             \
        if (status != 0) {                                                 \
            PLUGIN_ERROR("ctoj_string (%s) failed.", method_name);         \
            (*jvm_env)->DeleteLocalRef(jvm_env, o_notification);           \
            return NULL;                                                   \
        }                                                                  \
    } while (0)

    SET_STRING(n->host, "setHost");
    SET_STRING(n->plugin, "setPlugin");
    SET_STRING(n->plugin_instance, "setPluginInstance");
    SET_STRING(n->type, "setType");
    SET_STRING(n->type_instance, "setTypeInstance");
    SET_STRING(n->message, "setMessage");

#undef SET_STRING
#endif
    /* Set the `time' member. Java stores time in milliseconds. */
    status = ctoj_long(jvm_env, (jlong)CDTIME_T_TO_MS(n->time), c_notification, o_notification, "setTime");
    if (status != 0) {
        PLUGIN_ERROR("ctoj_long (setTime) failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_notification);
        return NULL;
    }

    /* Set the `severity' member.. */
    status = ctoj_int(jvm_env, (jint)n->severity, c_notification, o_notification, "setSeverity");
    if (status != 0) {
        PLUGIN_ERROR("ctoj_int (setSeverity) failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, o_notification);
        return NULL;
    }

    return o_notification;
}

/*
 * Java to C conversion functions
 */
#if 0
/* Call a `String <method> ()' method. */
static int jtoc_string(JNIEnv *jvm_env, char *buffer, size_t buffer_size, int empty_okay,
                                        jclass class_ptr, jobject object_ptr,
                                        const char *method_name)
{
    jmethodID method_id;
    jobject string_obj;
    const char *c_str;

    method_id = (*jvm_env)->GetMethodID(jvm_env, class_ptr, method_name, "()Ljava/lang/String;");
    if (method_id == NULL) {
        PLUGIN_ERROR("Cannot find method `String %s ()'.", method_name);
        return -1;
    }

    string_obj = (*jvm_env)->CallObjectMethod(jvm_env, object_ptr, method_id);
    if ((string_obj == NULL) && (empty_okay == 0)) {
        PLUGIN_ERROR("CallObjectMethod (%s) failed.", method_name);
        return -1;
    } else if ((string_obj == NULL) && (empty_okay != 0)) {
        memset(buffer, 0, buffer_size);
        return 0;
    }

    c_str = (*jvm_env)->GetStringUTFChars(jvm_env, string_obj, 0);
    if (c_str == NULL) {
        PLUGIN_ERROR("GetStringUTFChars failed.");
        (*jvm_env)->DeleteLocalRef(jvm_env, string_obj);
        return -1;
    }

    sstrncpy(buffer, c_str, buffer_size);

    (*jvm_env)->ReleaseStringUTFChars(jvm_env, string_obj, c_str);
    (*jvm_env)->DeleteLocalRef(jvm_env, string_obj);

    return 0;
}
#endif

/* Call an `int <method> ()' method. */
static int jtoc_int(JNIEnv *jvm_env, jint *ret_value, jclass class_ptr, jobject object_ptr,
                                     const char *method_name)
{
    jmethodID method_id;

    method_id = (*jvm_env)->GetMethodID(jvm_env, class_ptr, method_name, "()I");
    if (method_id == NULL) {
        PLUGIN_ERROR("Cannot find method `int %s ()'.", method_name);
        return -1;
    }

    *ret_value = (*jvm_env)->CallIntMethod(jvm_env, object_ptr, method_id);

    return 0;
}

/* Call a `long <method> ()' method. */
static int jtoc_long(JNIEnv *jvm_env, jlong *ret_value, jclass class_ptr, jobject object_ptr,
                                      const char *method_name)
{
    jmethodID method_id;

    method_id = (*jvm_env)->GetMethodID(jvm_env, class_ptr, method_name, "()J");
    if (method_id == NULL) {
        PLUGIN_ERROR("Cannot find method `long %s ()'.", method_name);
        return -1;
    }

    *ret_value = (*jvm_env)->CallLongMethod(jvm_env, object_ptr, method_id);

    return 0;
}

#if 0
/* Call a `double <method> ()' method. */
static int jtoc_double(JNIEnv *jvm_env, jdouble *ret_value, jclass class_ptr, jobject object_ptr,
                                        const char *method_name)
{
    jmethodID method_id;

    method_id = (*jvm_env)->GetMethodID(jvm_env, class_ptr, method_name, "()D");
    if (method_id == NULL) {
        PLUGIN_ERROR("Cannot find method `double %s ()'.", method_name);
        return -1;
    }

    *ret_value = (*jvm_env)->CallDoubleMethod(jvm_env, object_ptr, method_id);

    return 0;
}
#endif

#if 0
static int jtoc_value(JNIEnv *jvm_env, value_t *ret_value, int ds_type, jobject object_ptr)
{
    jclass class_ptr;
    int status;

    class_ptr = (*jvm_env)->GetObjectClass(jvm_env, object_ptr);

    if (ds_type == DS_TYPE_GAUGE) {
        jdouble tmp_double;

        status =
                jtoc_double(jvm_env, &tmp_double, class_ptr, object_ptr, "doubleValue");
        if (status != 0) {
            PLUGIN_ERROR("jtoc_double failed.");
            return -1;
        }
        (*ret_value).gauge = (gauge_t)tmp_double;
    } else {
        jlong tmp_long;

        status = jtoc_long(jvm_env, &tmp_long, class_ptr, object_ptr, "longValue");
        if (status != 0) {
            PLUGIN_ERROR("jtoc_long failed.");
            return -1;
        }

        if (ds_type == DS_TYPE_DERIVE)
            (*ret_value).derive = (derive_t)tmp_long;
        else
            (*ret_value).counter = (counter_t)tmp_long;
    }

    return 0;
}

/* Read a List<Number>, convert it to `value_t' and add it to the given
 * `value_list_t'. */
static int jtoc_values_array(JNIEnv *jvm_env, const data_set_t *ds, value_list_t *vl,
                                              jclass class_ptr, jobject object_ptr)
{
    jmethodID m_getvalues;
    jmethodID m_toarray;
    jobject o_list;
    jobjectArray o_number_array;

    value_t *values;

    size_t values_num = ds->ds_num;

    values = NULL;
    o_number_array = NULL;
    o_list = NULL;

#define BAIL_OUT(status)                                    \
    free(values);                                           \
    if (o_number_array != NULL)                             \
        (*jvm_env)->DeleteLocalRef(jvm_env, o_number_array);\
    if (o_list != NULL)                                     \
        (*jvm_env)->DeleteLocalRef(jvm_env, o_list);        \
    return status;

    /* Call: List<Number> ValueList.getValues () */
    m_getvalues = (*jvm_env)->GetMethodID(jvm_env, class_ptr, "getValues", "()Ljava/util/List;");
    if (m_getvalues == NULL) {
        PLUGIN_ERROR("Cannot find method `List getValues ()'.");
        BAIL_OUT(-1);
    }

    o_list = (*jvm_env)->CallObjectMethod(jvm_env, object_ptr, m_getvalues);
    if (o_list == NULL) {
        PLUGIN_ERROR("CallObjectMethod (getValues) failed.");
        BAIL_OUT(-1);
    }

    /* Call: Number[] List.toArray () */
    m_toarray = (*jvm_env)->GetMethodID(jvm_env, (*jvm_env)->GetObjectClass(jvm_env, o_list),
                                                 "toArray", "()[Ljava/lang/Object;");
    if (m_toarray == NULL) {
        PLUGIN_ERROR("Cannot find method `Object[] toArray ()'.");
        BAIL_OUT(-1);
    }

    o_number_array = (*jvm_env)->CallObjectMethod(jvm_env, o_list, m_toarray);
    if (o_number_array == NULL) {
        PLUGIN_ERROR("CallObjectMethod (toArray) failed.");
        BAIL_OUT(-1);
    }

    values = calloc(values_num, sizeof(*values));
    if (values == NULL) {
        PLUGIN_ERROR("calloc failed.");
        BAIL_OUT(-1);
    }

    for (size_t i = 0; i < values_num; i++) {
        jobject o_number;
        int status;

        o_number =
                (*jvm_env)->GetObjectArrayElement(jvm_env, o_number_array, (jsize)i);
        if (o_number == NULL) {
            PLUGIN_ERROR("GetObjectArrayElement (%zu) failed.", i);
            BAIL_OUT(-1);
        }

        status = jtoc_value(jvm_env, values + i, ds->ds[i].type, o_number);
        if (status != 0) {
            PLUGIN_ERROR("jtoc_value (%zu) failed.", i);
            BAIL_OUT(-1);
        }
    } /* for (i = 0; i < values_num; i++) */

    vl->values = values;
    vl->values_len = values_num;

#undef BAIL_OUT
    (*jvm_env)->DeleteLocalRef(jvm_env, o_number_array);
    (*jvm_env)->DeleteLocalRef(jvm_env, o_list);
    return 0;
}

/* Convert a org/ncollectd/api/ValueList to a value_list_t. */
static int jtoc_value_list(JNIEnv *jvm_env, value_list_t *vl, jobject object_ptr)
{
    jclass class_ptr;
    int status;
    jlong tmp_long;
    const data_set_t *ds;

    class_ptr = (*jvm_env)->GetObjectClass(jvm_env, object_ptr);
    if (class_ptr == NULL) {
        PLUGIN_ERROR("jtoc_value_list: GetObjectClass failed.");
        return -1;
    }

/* eo == empty okay */
#define SET_STRING(buffer, method, eo)                                         \
    do {                                                                       \
        status = jtoc_string(jvm_env, buffer, sizeof(buffer), eo, class_ptr,   \
                                                 object_ptr, method);          \
        if (status != 0) {                                                     \
            PLUGIN_ERROR("jtoc_value_list: jtoc_string (%s) failed.", method); \
            return -1;                                                         \
        }                                                                      \
    } while (0)

    SET_STRING(vl->type, "getType", /* empty = */ 0);

    ds = plugin_get_ds(vl->type);
    if (ds == NULL) {
        PLUGIN_ERROR("Data-set `%s' is not defined. "
                     "Please consult the types.db(5) manpage for mor information.",
                      vl->type);
        return -1;
    }

    SET_STRING(vl->host, "getHost", /* empty = */ 0);
    SET_STRING(vl->plugin, "getPlugin", /* empty = */ 0);
    SET_STRING(vl->plugin_instance, "getPluginInstance", /* empty = */ 1);
    SET_STRING(vl->type_instance, "getTypeInstance", /* empty = */ 1);

#undef SET_STRING

    status = jtoc_long(jvm_env, &tmp_long, class_ptr, object_ptr, "getTime");
    if (status != 0) {
        PLUGIN_ERROR("jtoc_long (getTime) failed.");
        return -1;
    }
    /* Java measures time in milliseconds. */
    vl->time = MS_TO_CDTIME_T(tmp_long);

    status = jtoc_long(jvm_env, &tmp_long, class_ptr, object_ptr, "getInterval");
    if (status != 0) {
        PLUGIN_ERROR("jtoc_long (getInterval) failed.");
        return -1;
    }
    vl->interval = MS_TO_CDTIME_T(tmp_long);

    status = jtoc_values_array(jvm_env, ds, vl, class_ptr, object_ptr);
    if (status != 0) {
        PLUGIN_ERROR("jtoc_values_array failed.");
        return -1;
    }

    return 0;
}
#endif

/* Convert a org/ncollectd/api/Notification to a notification_t. */
static int jtoc_notification(JNIEnv *jvm_env, notification_t *n, jobject object_ptr)
{
    jclass class_ptr;
    int status;
    jlong tmp_long;
    jint tmp_int;

    class_ptr = (*jvm_env)->GetObjectClass(jvm_env, object_ptr);
    if (class_ptr == NULL) {
        PLUGIN_ERROR("GetObjectClass failed.");
        return -1;
    }
#if 0
/* eo == empty okay */
#define SET_STRING(buffer, method, eo)                                       \
    do {                                                                     \
        status = jtoc_string(jvm_env, buffer, sizeof(buffer), eo, class_ptr, \
                                                 object_ptr, method);        \
        if (status != 0) {                                                   \
            PLUGIN_ERROR("jtoc_string (%s) failed.",  method);               \
            return -1;                                                       \
        }                                                                    \
    } while (0)

    SET_STRING(n->host, "getHost", /* empty = */ 1);
    SET_STRING(n->plugin, "getPlugin", /* empty = */ 1);
    SET_STRING(n->plugin_instance, "getPluginInstance", /* empty = */ 1);
    SET_STRING(n->type, "getType", /* empty = */ 1);
    SET_STRING(n->type_instance, "getTypeInstance", /* empty = */ 1);
    SET_STRING(n->message, "getMessage", /* empty = */ 0);

#undef SET_STRING
#endif
    status = jtoc_long(jvm_env, &tmp_long, class_ptr, object_ptr, "getTime");
    if (status != 0) {
        PLUGIN_ERROR("jtoc_long (getTime) failed.");
        return -1;
    }
    /* Java measures time in milliseconds. */
    n->time = MS_TO_CDTIME_T(tmp_long);

    status = jtoc_int(jvm_env, &tmp_int, class_ptr, object_ptr, "getSeverity");
    if (status != 0) {
        PLUGIN_ERROR("jtoc_int (getSeverity) failed.");
        return -1;
    }
    n->severity = (int)tmp_int;

    return 0;
}

/*
 * Functions accessible from Java
 */
static jint JNICALL cjni_api_dispatch_values(JNIEnv *jvm_env, jobject this, jobject java_vl)
{
    int status = 0;
    (void)jvm_env;
    (void)this;
    (void)java_vl;
#if 0
    value_list_t vl = VALUE_LIST_INIT;

    PLUGIN_DEBUG("java_vl = %p;", (void *)java_vl);

    status = jtoc_value_list(jvm_env, &vl, java_vl);
    if (status != 0) {
        PLUGIN_ERROR("jtoc_value_list failed.");
        return -1;
    }

    status = plugin_dispatch_values(&vl);

    free(vl.values);
#endif
    return status;
}

static jint JNICALL cjni_api_dispatch_notification(JNIEnv *jvm_env,
                                                   __attribute__((unused)) jobject this,
                                                   jobject o_notification)
{
    notification_t n = {0};
    int status;

    status = jtoc_notification(jvm_env, &n, o_notification);
    if (status != 0) {
        PLUGIN_ERROR("jtoc_notification failed.");
        return -1;
    }

    status = plugin_dispatch_notification(&n);

    return status;
}

static jint JNICALL cjni_api_register_config(JNIEnv *jvm_env, __attribute__((unused)) jobject this,
                                             jobject o_name, jobject o_config)
{
    return cjni_callback_register(jvm_env, o_name, o_config, CB_TYPE_CONFIG);
}

static jint JNICALL cjni_api_register_init(JNIEnv *jvm_env, __attribute__((unused)) jobject this,
                                           jobject o_name, jobject o_config)
{
    return cjni_callback_register(jvm_env, o_name, o_config, CB_TYPE_INIT);
}

static jint JNICALL cjni_api_register_read(JNIEnv *jvm_env, __attribute__((unused)) jobject this,
                                           jobject o_name, jobject o_read)
{
    cjni_callback_info_t *cbi;

    cbi = cjni_callback_info_create(jvm_env, o_name, o_read, CB_TYPE_READ);
    if (cbi == NULL)
        return -1;

    PLUGIN_DEBUG("Registering new read callback: %s", cbi->name);

    plugin_register_complex_read(
            /* group = */ NULL, cbi->name, cjni_read,
            /* interval = */ 0,
            &(user_data_t){
                    .data = cbi,
                    .free_func = cjni_callback_info_destroy,
            });

    (*jvm_env)->DeleteLocalRef(jvm_env, o_read);

    return 0;
}

static jint JNICALL cjni_api_register_write(JNIEnv *jvm_env, __attribute__((unused)) jobject this,
                                            jobject o_name, jobject o_write)
{
// FIXME
    cjni_callback_info_t *cbi = cjni_callback_info_create(jvm_env, o_name, o_write, CB_TYPE_WRITE);
    if (cbi == NULL)
        return -1;

#if 0
    cjni_callback_info_t *cbi;
    cbi = cjni_callback_info_create(jvm_env, o_name, o_flush, CB_TYPE_FLUSH);
    if (cbi == NULL)
        return -1;
#endif

    PLUGIN_DEBUG("Registering new write callback: %s", cbi->name);

    plugin_register_write("java", cbi->name, cjni_write, NULL, 0, 0,
                          &(user_data_t){ .data = cbi, .free_func = cjni_callback_info_destroy });

    (*jvm_env)->DeleteLocalRef(jvm_env, o_write);

    return 0;
}

static jint JNICALL cjni_api_register_shutdown(JNIEnv *jvm_env, __attribute__((unused)) jobject this,
                                               jobject o_name, jobject o_shutdown)
{
    return cjni_callback_register(jvm_env, o_name, o_shutdown, CB_TYPE_SHUTDOWN);
}

static jint JNICALL cjni_api_register_log(JNIEnv *jvm_env, __attribute__((unused)) jobject this,
                                          jobject o_name, jobject o_log)
{
    cjni_callback_info_t *cbi;

    cbi = cjni_callback_info_create(jvm_env, o_name, o_log, CB_TYPE_LOG);
    if (cbi == NULL)
        return -1;

    PLUGIN_DEBUG("Registering new log callback: %s", cbi->name);

    plugin_register_log("java", cbi->name, cjni_log,
                        &(user_data_t){ .data = cbi, .free_func = cjni_callback_info_destroy, });

    (*jvm_env)->DeleteLocalRef(jvm_env, o_log);

    return 0;
}

static jint JNICALL cjni_api_register_notification(JNIEnv *jvm_env,
                                                   __attribute__((unused)) jobject this,
                                                   jobject o_name, jobject o_notification)
{
    cjni_callback_info_t *cbi;

    cbi = cjni_callback_info_create(jvm_env, o_name, o_notification, CB_TYPE_NOTIFICATION);
    if (cbi == NULL)
        return -1;

    PLUGIN_DEBUG("Registering new notification callback: %s", cbi->name);

    plugin_register_notification("java", cbi->name, cjni_notification,
                                 &(user_data_t){ .data = cbi, .free_func = cjni_callback_info_destroy });

    (*jvm_env)->DeleteLocalRef(jvm_env, o_notification);

    return 0;
}
static void JNICALL cjni_api_log(JNIEnv *jvm_env, __attribute__((unused)) jobject this,
                                 jint severity, jobject o_message)
{
    const char *c_str;

    c_str = (*jvm_env)->GetStringUTFChars(jvm_env, o_message, 0);
    if (c_str == NULL) {
        PLUGIN_ERROR("cjni_api_log: GetStringUTFChars failed.");
        return;
    }

    if (severity < LOG_ERR)
        severity = LOG_ERR;
    if (severity > LOG_DEBUG)
        severity = LOG_DEBUG;

    plugin_log(severity, NULL, 0, NULL, "%s", c_str);

    (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_message, c_str);
}

#if 0
static jstring JNICALL cjni_api_get_hostname(JNIEnv *jvm_env, __attribute__((unused)) jobject this)
{
    return ctoj_output_string(jvm_env, hostname_g);
}
#endif

/* List of ``native'' functions, i. e. C-functions that can be called from Java. */
static JNINativeMethod jni_api_functions[] = {
    {"dispatchValues",       "(Lorg/ncollectd/api/ValueList;)I",                                       cjni_api_dispatch_values},
    {"dispatchNotification", "(Lorg/ncollectd/api/Notification;)I",                                    cjni_api_dispatch_notification},
    {"registerConfig",       "(Ljava/lang/String;Lorg/ncollectd/api/CollectdConfigInterface;)I",       cjni_api_register_config},
    {"registerInit",         "(Ljava/lang/String;Lorg/ncollectd/api/CollectdInitInterface;)I",         cjni_api_register_init},
    {"registerRead",         "(Ljava/lang/String;Lorg/ncollectd/api/CollectdReadInterface;)I",         cjni_api_register_read},
    {"registerWrite",        "(Ljava/lang/String;Lorg/ncollectd/api/CollectdWriteInterface;)I",        cjni_api_register_write},
    {"registerShutdown",     "(Ljava/lang/String;Lorg/ncollectd/api/CollectdShutdownInterface;)I",     cjni_api_register_shutdown},
    {"registerLog",          "(Ljava/lang/String;Lorg/ncollectd/api/CollectdLogInterface;)I",          cjni_api_register_log},
    {"registerNotification", "(Ljava/lang/String;Lorg/ncollectd/api/CollectdNotificationInterface;)I", cjni_api_register_notification},
    {"log",                  "(ILjava/lang/String;)V",                                                 cjni_api_log},
#if 0
    {"getHostname",          "()Ljava/lang/String;",                                                   cjni_api_get_hostname},
#endif
};
static size_t jni_api_functions_num = sizeof(jni_api_functions) / sizeof(jni_api_functions[0]);

/*
 * Functions
 */
/* Allocate a `cjni_callback_info_t' given the type and objects necessary for
 * all registration functions. */
static cjni_callback_info_t *
    cjni_callback_info_create(JNIEnv *jvm_env, jobject o_name, jobject o_callback, int type)
{
    const char *c_name;
    cjni_callback_info_t *cbi;
    const char *method_name;
    const char *method_signature;

    switch (type) {
    case CB_TYPE_CONFIG:
        method_name = "config";
        method_signature = "(Lorg/ncollectd/api/ConfigItem;)I";
        break;
    case CB_TYPE_INIT:
        method_name = "init";
        method_signature = "()I";
        break;
    case CB_TYPE_READ:
        method_name = "read";
        method_signature = "()I";
        break;
    case CB_TYPE_WRITE:
        method_name = "write";
        method_signature = "(Lorg/ncollectd/api/MetricFamily;)I";
        break;
    case CB_TYPE_FLUSH:
        method_name = "flush";
        method_signature = "(Ljava/lang/Number;)I";
        break;
    case CB_TYPE_SHUTDOWN:
        method_name = "shutdown";
        method_signature = "()I";
        break;
    case CB_TYPE_LOG:
        method_name = "log";
        method_signature = "(ILjava/lang/String;)V";
        break;
    case CB_TYPE_NOTIFICATION:
        method_name = "notification";
        method_signature = "(Lorg/ncollectd/api/Notification;)I";
        break;
    default:
        PLUGIN_ERROR("cjni_callback_info_create: Unknown type: %#x", (unsigned int)type);
        return NULL;
    }

    c_name = (*jvm_env)->GetStringUTFChars(jvm_env, o_name, 0);
    if (c_name == NULL) {
        PLUGIN_ERROR("GetStringUTFChars failed.");
        return NULL;
    }

    cbi = calloc(1, sizeof(*cbi));
    if (cbi == NULL) {
        PLUGIN_ERROR("calloc failed.");
        (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_name, c_name);
        return NULL;
    }
    cbi->type = type;

    cbi->name = strdup(c_name);
    if (cbi->name == NULL) {
        pthread_mutex_unlock(&java_callbacks_lock);
        PLUGIN_ERROR("strdup failed.");
        (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_name, c_name);
        free(cbi);
        return NULL;
    }

    (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_name, c_name);

    cbi->object = (*jvm_env)->NewGlobalRef(jvm_env, o_callback);
    if (cbi->object == NULL) {
        PLUGIN_ERROR("NewGlobalRef failed.");
        free(cbi->name);
        free(cbi);
        return NULL;
    }

    cbi->class = (*jvm_env)->GetObjectClass(jvm_env, cbi->object);
    if (cbi->class == NULL) {
        PLUGIN_ERROR("GetObjectClass failed.");
        (*jvm_env)->DeleteGlobalRef(jvm_env, cbi->object);
        free(cbi->name);
        free(cbi);
        return NULL;
    }

    cbi->method = (*jvm_env)->GetMethodID(jvm_env, cbi->class, method_name, method_signature);
    if (cbi->method == NULL) {
        PLUGIN_ERROR("Cannot find the `%s' method with signature `%s'.", method_name, method_signature);
        (*jvm_env)->DeleteGlobalRef(jvm_env, cbi->object);
        free(cbi->name);
        free(cbi);
        return NULL;
    }

    return cbi;
}

/* Allocate a `cjni_callback_info_t' via `cjni_callback_info_create' and add it
 * to the global `java_callbacks' variable. This is used for `config', `init',
 * and `shutdown' callbacks. */
static int cjni_callback_register(JNIEnv *jvm_env, jobject o_name, jobject o_callback, int type)
{
    cjni_callback_info_t *cbi;
    cjni_callback_info_t *tmp;
#ifdef NCOLLECTD_DEBUG
    const char *type_str;
#endif

    cbi = cjni_callback_info_create(jvm_env, o_name, o_callback, type);
    if (cbi == NULL)
        return -1;

#ifdef NCOLLECTD_DEBUG
    switch (type) {
    case CB_TYPE_CONFIG:
        type_str = "config";
        break;
    case CB_TYPE_INIT:
        type_str = "init";
        break;
    case CB_TYPE_SHUTDOWN:
        type_str = "shutdown";
        break;
    default:
        type_str = "<unknown>";
    }
    PLUGIN_DEBUG("Registering new %s callback: %s", type_str, cbi->name);
#endif

    pthread_mutex_lock(&java_callbacks_lock);

    tmp = realloc(java_callbacks,
                                (java_callbacks_num + 1) * sizeof(*java_callbacks));
    if (tmp == NULL) {
        pthread_mutex_unlock(&java_callbacks_lock);
        PLUGIN_ERROR("cjni_callback_register: realloc failed.");

        (*jvm_env)->DeleteGlobalRef(jvm_env, cbi->object);
        free(cbi);

        return -1;
    }
    java_callbacks = tmp;
    java_callbacks[java_callbacks_num] = *cbi;
    java_callbacks_num++;

    pthread_mutex_unlock(&java_callbacks_lock);

    free(cbi);
    return 0;
}

/* Callback for `pthread_key_create'. It frees the data contained in
 * `jvm_env_key' and prints a warning if the reference counter is not zero. */
static void cjni_jvm_env_destroy(void *args)
{
    cjni_jvm_env_t *cjni_env;

    if (args == NULL)
        return;

    cjni_env = (cjni_jvm_env_t *)args;

    if (cjni_env->reference_counter > 0) {
        PLUGIN_ERROR("cjni_env->reference_counter = %i;", cjni_env->reference_counter);
    }

    if (cjni_env->jvm_env != NULL) {
        PLUGIN_ERROR("cjni_env->jvm_env = %p;", (void *)cjni_env->jvm_env);
    }

    /* The pointer is allocated in `cjni_thread_attach' */
    free(cjni_env);
}

/* Register ``native'' functions with the JVM. Native functions are C-functions
 * that can be called by Java code. */
static int cjni_init_native(JNIEnv *jvm_env)
{
    jclass api_class_ptr;
    int status;

    api_class_ptr = (*jvm_env)->FindClass(jvm_env, "org/ncollectd/api/NCollectd");
    if (api_class_ptr == NULL) {
        PLUGIN_ERROR("Cannot find the API class \"org.ncollectd.api"
                     ".NCollectd\". Please set the correct class path "
                     "using 'JVMArg \"-Djava.class.path=...\"'.");
        return -1;
    }

    status = (*jvm_env)->RegisterNatives(
            jvm_env, api_class_ptr, jni_api_functions, (jint)jni_api_functions_num);
    if (status != 0) {
        PLUGIN_ERROR("RegisterNatives failed with status %i.", status);
        return -1;
    }

    return 0;
}

/* Create the JVM. This is called when the first thread tries to access the JVM
 * via cjni_thread_attach. */
static int cjni_create_jvm(void)
{
    JNIEnv *jvm_env;
    JavaVMInitArgs vm_args = {0};
    JavaVMOption vm_options[jvm_argc];

    int status;

    if (jvm != NULL)
        return 0;

    status = pthread_key_create(&jvm_env_key, cjni_jvm_env_destroy);
    if (status != 0) {
        PLUGIN_ERROR("cjni_create_jvm: pthread_key_create failed with status %i.", status);
        return -1;
    }

    jvm_env = NULL;

    vm_args.version = JNI_VERSION_1_2;
    vm_args.options = vm_options;
    vm_args.nOptions = (jint)jvm_argc;

    for (size_t i = 0; i < jvm_argc; i++) {
        PLUGIN_DEBUG("jvm_argv[%" PRIsz "] = %s", i, jvm_argv[i]);
        vm_args.options[i].optionString = jvm_argv[i];
    }

    status = JNI_CreateJavaVM(&jvm, (void *)&jvm_env, (void *)&vm_args);
    if (status != 0) {
        PLUGIN_ERROR("JNI_CreateJavaVM failed with status %i.", status);
        return -1;
    }
    assert(jvm != NULL);
    assert(jvm_env != NULL);

    /* Call RegisterNatives */
    status = cjni_init_native(jvm_env);
    if (status != 0) {
        PLUGIN_ERROR("cjni_create_jvm: cjni_init_native failed.");
        return -1;
    }

    PLUGIN_DEBUG("The JVM has been created.");
    return 0;
}

/* Increase the reference counter to the JVM for this thread. If it was zero,
 * attach the JVM first. */
static JNIEnv *cjni_thread_attach(void)
{
    cjni_jvm_env_t *cjni_env;
    JNIEnv *jvm_env;

    /* If we're the first thread to access the JVM, we'll have to create it
     * first.. */
    if (jvm == NULL) {
        int status;

        status = cjni_create_jvm();
        if (status != 0) {
            PLUGIN_ERROR("cjni_create_jvm failed.");
            return NULL;
        }
    }
    assert(jvm != NULL);

    cjni_env = pthread_getspecific(jvm_env_key);
    if (cjni_env == NULL) {
        /* This pointer is free'd in `cjni_jvm_env_destroy'. */
        cjni_env = calloc(1, sizeof(*cjni_env));
        if (cjni_env == NULL) {
            PLUGIN_ERROR("calloc failed.");
            return NULL;
        }
        cjni_env->reference_counter = 0;
        cjni_env->jvm_env = NULL;

        pthread_setspecific(jvm_env_key, cjni_env);
    }

    if (cjni_env->reference_counter > 0) {
        cjni_env->reference_counter++;
        jvm_env = cjni_env->jvm_env;
    } else {
        int status;
        JavaVMAttachArgs args = {0};

        assert(cjni_env->jvm_env == NULL);

        args.version = JNI_VERSION_1_2;

        status = (*jvm)->AttachCurrentThread(jvm, (void *)&jvm_env, (void *)&args);
        if (status != 0) {
            PLUGIN_ERROR("AttachCurrentThread failed with status %i.", status);
            return NULL;
        }

        cjni_env->reference_counter = 1;
        cjni_env->jvm_env = jvm_env;
    }

    PLUGIN_DEBUG("cjni_env->reference_counter = %i", cjni_env->reference_counter);
    assert(jvm_env != NULL);
    return jvm_env;
}

/* Decrease the reference counter of this thread. If it reaches zero, detach
 * from the JVM. */
static int cjni_thread_detach(void)
{
    cjni_jvm_env_t *cjni_env;
    int status;

    cjni_env = pthread_getspecific(jvm_env_key);
    if (cjni_env == NULL) {
        PLUGIN_ERROR("pthread_getspecific failed.");
        return -1;
    }

    assert(cjni_env->reference_counter > 0);
    assert(cjni_env->jvm_env != NULL);

    cjni_env->reference_counter--;
    PLUGIN_DEBUG("cjni_env->reference_counter = %i", cjni_env->reference_counter);

    if (cjni_env->reference_counter > 0)
        return 0;

    status = (*jvm)->DetachCurrentThread(jvm);
    if (status != 0) {
        PLUGIN_ERROR("DetachCurrentThread failed with status %i.", status);
    }

    cjni_env->reference_counter = 0;
    cjni_env->jvm_env = NULL;

    return 0;
}

static int cjni_config_add_jvm_arg(config_item_t *ci)
{
    char **tmp;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("`JVMArg' needs exactly one string argument.");
        return -1;
    }

    if (jvm != NULL) {
        PLUGIN_ERROR("All `JVMArg' options MUST appear before all "
                    "`LoadPlugin' options! The JVM is already started and I have to "
                    "ignore this argument: %s",
                    ci->values[0].value.string);
        return -1;
    }

    tmp = realloc(jvm_argv, sizeof(char *) * (jvm_argc + 1));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        return -1;
    }
    jvm_argv = tmp;

    jvm_argv[jvm_argc] = strdup(ci->values[0].value.string);
    if (jvm_argv[jvm_argc] == NULL) {
        PLUGIN_ERROR("strdup failed.");
        return -1;
    }
    jvm_argc++;

    return 0;
}

static int cjni_config_load_plugin(config_item_t *ci)
{
    JNIEnv *jvm_env;
    java_plugin_class_t *class;
    jmethodID constructor_id;
    jobject tmp_object;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("`LoadPlugin' needs exactly one string argument.");
        return -1;
    }

    jvm_env = cjni_thread_attach();
    if (jvm_env == NULL)
        return -1;

    class = realloc(java_classes_list, (java_classes_list_len + 1) * sizeof(*java_classes_list));
    if (class == NULL) {
        PLUGIN_ERROR("realloc failed.");
        cjni_thread_detach();
        return -1;
    }
    java_classes_list = class;
    class = java_classes_list + java_classes_list_len;

    memset(class, 0, sizeof(*class));
    class->name = strdup(ci->values[0].value.string);
    if (class->name == NULL) {
        PLUGIN_ERROR("strdup failed.");
        cjni_thread_detach();
        return -1;
    }
    class->class = NULL;
    class->object = NULL;

    /* Replace all dots ('.') with slashes ('/'). Dots are usually used
       thorough the Java community, but (Sun's) `FindClass' and friends need slashes. */
    for (size_t i = 0; class->name[i] != 0; i++) {
        if (class->name[i] == '.')
            class->name[i] = '/';
    }

    PLUGIN_DEBUG("Loading class %s", class->name);

    class->class = (*jvm_env)->FindClass(jvm_env, class->name);
    if (class->class == NULL) {
        PLUGIN_ERROR("cjni_config_load_plugin: FindClass (%s) failed.",
                    class->name);
        cjni_thread_detach();
        free(class->name);
        return -1;
    }

    constructor_id =
            (*jvm_env)->GetMethodID(jvm_env, class->class, "<init>", "()V");
    if (constructor_id == NULL) {
        PLUGIN_ERROR("Could not find the constructor for `%s'.", class->name);
        cjni_thread_detach();
        free(class->name);
        return -1;
    }

    tmp_object = (*jvm_env)->NewObject(jvm_env, class->class, constructor_id);
    if (tmp_object != NULL)
        class->object = (*jvm_env)->NewGlobalRef(jvm_env, tmp_object);
    else
        class->object = NULL;
    if (class->object == NULL) {
        PLUGIN_ERROR("Could not create a new `%s' object.", class->name);
        cjni_thread_detach();
        free(class->name);
        return -1;
    }

    cjni_thread_detach();

    java_classes_list_len++;

    return 0;
}

static int cjni_config_plugin_block(config_item_t *ci)
{
    JNIEnv *jvm_env;
    cjni_callback_info_t *cbi;
    jobject o_ocitem;
    const char *name;

    jclass class;
    jmethodID method;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("'Plugin' blocks need exactly one string argument.");
        return -1;
    }

    name = ci->values[0].value.string;

    cbi = NULL;
    for (size_t i = 0; i < java_callbacks_num; i++) {
        if (java_callbacks[i].type != CB_TYPE_CONFIG)
            continue;

        if (strcmp(name, java_callbacks[i].name) != 0)
            continue;

        cbi = java_callbacks + i;
        break;
    }

    if (cbi == NULL) {
        PLUGIN_NOTICE("Configuration block for `%s' found, but no such "
                      "configuration callback has been registered. Please make sure, the "
                      "'LoadPlugin' lines precede the 'Plugin' blocks.", name);
        return 0;
    }

    PLUGIN_DEBUG("Configuring %s", name);

    jvm_env = cjni_thread_attach();
    if (jvm_env == NULL)
        return -1;

    o_ocitem = ctoj_config_item(jvm_env, ci);
    if (o_ocitem == NULL) {
        PLUGIN_ERROR("ctoj_config_item failed.");
        cjni_thread_detach();
        return -1;
    }

    class = (*jvm_env)->GetObjectClass(jvm_env, cbi->object);
    method = (*jvm_env)->GetMethodID(jvm_env, class, "config", "(Lorg/ncollectd/api/OConfigItem;)I");

    (*jvm_env)->CallIntMethod(jvm_env, cbi->object, method, o_ocitem);

    (*jvm_env)->DeleteLocalRef(jvm_env, o_ocitem);
    cjni_thread_detach();
    return 0;
}

static int cjni_config_perform(config_item_t *ci)
{
    int success;
    int errors;
    int status;

    success = 0;
    errors = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("JVMArg", child->key) == 0) {
            status = cjni_config_add_jvm_arg(child);
            if (status == 0)
                success++;
            else
                errors++;
        } else if (strcasecmp("LoadPlugin", child->key) == 0) {
            status = cjni_config_load_plugin(child);
            if (status == 0)
                success++;
            else
                errors++;
        } else if (strcasecmp("Plugin", child->key) == 0) {
            status = cjni_config_plugin_block(child);
            if (status == 0)
                success++;
            else
                errors++;
        } else {
            PLUGIN_WARNING("Option `%s' not allowed here.", child->key);
            errors++;
        }
    }

    PLUGIN_DEBUG("jvm_argc = %" PRIsz ";", jvm_argc);
    PLUGIN_DEBUG("java_classes_list_len = %" PRIsz ";", java_classes_list_len);

    if ((success == 0) && (errors > 0)) {
        PLUGIN_ERROR("All statements failed.");
        return -1;
    }

    return 0;
}

/* Copy the children of `ci' to the global `config_block' variable. */
static int cjni_config_callback(config_item_t *ci)
{
    config_item_t *ci_copy;
    config_item_t *tmp;

    assert(ci != NULL);
    if (ci->children_num == 0)
        return 0; /* nothing to do */

    ci_copy = config_clone(ci);
    if (ci_copy == NULL) {
        PLUGIN_ERROR("config_clone failed.");
        return -1;
    }

    if (config_block == NULL) {
        config_block = ci_copy;
        return 0;
    }

    tmp = realloc(config_block->children,
                  (config_block->children_num + ci_copy->children_num) * sizeof(*tmp));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        config_free(ci_copy);
        return -1;
    }
    config_block->children = tmp;

    /* Copy the pointers */
    memcpy(config_block->children + config_block->children_num, ci_copy->children,
           ci_copy->children_num * sizeof(*ci_copy->children));
    config_block->children_num += ci_copy->children_num;

    /* Delete the pointers from the copy, so `config_free' can't free them. */
    memset(ci_copy->children, 0,
           ci_copy->children_num * sizeof(*ci_copy->children));
    ci_copy->children_num = 0;

    config_free(ci_copy);

    return 0;
}

/* Free the data contained in the `user_data_t' pointer passed to `cjni_read'
 * and `cjni_write'. In particular, delete the global reference to the Java
 * object. */
static void cjni_callback_info_destroy(void *arg)
{
    JNIEnv *jvm_env;
    cjni_callback_info_t *cbi;

    PLUGIN_DEBUG("(arg = %p);", arg);

    cbi = (cjni_callback_info_t *)arg;

    /* This condition can occur when shutting down. */
    if (jvm == NULL) {
        free(cbi);
        return;
    }

    if (arg == NULL)
        return;

    jvm_env = cjni_thread_attach();
    if (jvm_env == NULL) {
        PLUGIN_ERROR("cjni_thread_attach failed.");
        return;
    }

    (*jvm_env)->DeleteGlobalRef(jvm_env, cbi->object);

    cbi->method = NULL;
    cbi->object = NULL;
    cbi->class = NULL;
    free(cbi);

    cjni_thread_detach();
}

/* Call the CB_TYPE_READ callback pointed to by the `user_data_t' pointer. */
static int cjni_read(user_data_t *ud)
{
    JNIEnv *jvm_env;
    cjni_callback_info_t *cbi;
    int ret_status;

    if (jvm == NULL) {
        PLUGIN_ERROR("jvm == NULL");
        return -1;
    }

    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    jvm_env = cjni_thread_attach();
    if (jvm_env == NULL)
        return -1;

    cbi = (cjni_callback_info_t *)ud->data;

    ret_status = (*jvm_env)->CallIntMethod(jvm_env, cbi->object, cbi->method);

    cjni_thread_detach();
    return ret_status;
}

/* Call the CB_TYPE_WRITE callback pointed to by the `user_data_t' pointer. */
static int cjni_write(metric_family_t const *fam, user_data_t *ud)
{
    if (jvm == NULL) {
        PLUGIN_ERROR("jvm == NULL");
        return -1;
    }

    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    JNIEnv *jvm_env = cjni_thread_attach();
    if (jvm_env == NULL)
        return -1;

    cjni_callback_info_t *cbi = (cjni_callback_info_t *)ud->data;
    (void)fam;
    jobject fam_java = NULL;
#if 0
    jobject vl_java = ctoj_value_list(jvm_env, ds, vl);
    if (vl_java == NULL) {
        PLUGIN_ERROR("ctoj_value_list failed.");
        cjni_thread_detach();
        return -1;
    }
#endif

    int ret_status = (*jvm_env)->CallIntMethod(jvm_env, cbi->object, cbi->method, fam_java);

    (*jvm_env)->DeleteLocalRef(jvm_env, fam_java);

    cjni_thread_detach();
    return ret_status;
}
#if 0
// FIXME
/* Call the CB_TYPE_FLUSH callback pointed to by the `user_data_t' pointer. */
static int cjni_flush(cdtime_t timeout, user_data_t *ud)
{
    JNIEnv *jvm_env;
    cjni_callback_info_t *cbi;
    jobject o_timeout;
    int ret_status;

    if (jvm == NULL) {
        PLUGIN_ERROR("jvm == NULL");
        return -1;
    }

    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    jvm_env = cjni_thread_attach();
    if (jvm_env == NULL)
        return -1;

    cbi = (cjni_callback_info_t *)ud->data;

    o_timeout = ctoj_jdouble_to_number(jvm_env, (jdouble)CDTIME_T_TO_DOUBLE(timeout));
    if (o_timeout == NULL) {
        PLUGIN_ERROR("Converting double to Number object failed.");
        cjni_thread_detach();
        return -1;
    }

    ret_status = (*jvm_env)->CallIntMethod(jvm_env, cbi->object, cbi->method, o_timeout);

    (*jvm_env)->DeleteLocalRef(jvm_env, o_timeout);

    cjni_thread_detach();
    return ret_status;
}
#endif

/* Call the CB_TYPE_LOG callback pointed to by the `user_data_t' pointer. */
static void cjni_log(const log_msg_t *msg, user_data_t *ud)
{
    if (jvm == NULL)
        return;

    if ((ud == NULL) || (ud->data == NULL))
        return;

    JNIEnv *jvm_env = cjni_thread_attach();
    if (jvm_env == NULL)
        return;

    cjni_callback_info_t *cbi = (cjni_callback_info_t *)ud->data;

    jobject o_message = (*jvm_env)->NewStringUTF(jvm_env, msg->msg);
    if (o_message == NULL) {
        cjni_thread_detach();
        return;
    }

    (*jvm_env)->CallVoidMethod(jvm_env, cbi->object, cbi->method, (jint)msg->severity, o_message);

    (*jvm_env)->DeleteLocalRef(jvm_env, o_message);

    cjni_thread_detach();
}

/* Call the CB_TYPE_NOTIFICATION callback pointed to by the `user_data_t'
 * pointer. */
static int cjni_notification(const notification_t *n, user_data_t *ud)
{
    JNIEnv *jvm_env;
    cjni_callback_info_t *cbi;
    jobject o_notification;
    int ret_status;

    if (jvm == NULL) {
        PLUGIN_ERROR("jvm == NULL");
        return -1;
    }

    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    jvm_env = cjni_thread_attach();
    if (jvm_env == NULL)
        return -1;

    cbi = (cjni_callback_info_t *)ud->data;

    o_notification = ctoj_notification(jvm_env, n);
    if (o_notification == NULL) {
        PLUGIN_ERROR("ctoj_notification failed.");
        cjni_thread_detach();
        return -1;
    }

    ret_status = (*jvm_env)->CallIntMethod(jvm_env, cbi->object, cbi->method, o_notification);

    (*jvm_env)->DeleteLocalRef(jvm_env, o_notification);

    cjni_thread_detach();
    return ret_status;
}

/* Iterate over `java_callbacks' and call all CB_TYPE_INIT callbacks. */
static int cjni_init_plugins(JNIEnv *jvm_env)
{
    int status;

    for (size_t i = 0; i < java_callbacks_num; i++) {
        if (java_callbacks[i].type != CB_TYPE_INIT)
            continue;

        PLUGIN_DEBUG("Initializing %s", java_callbacks[i].name);

        status = (*jvm_env)->CallIntMethod(jvm_env, java_callbacks[i].object,
                                                    java_callbacks[i].method);
        if (status != 0) {
            PLUGIN_ERROR("Initializing `%s' failed with status %i. Removing read function.",
                         java_callbacks[i].name, status);
            plugin_unregister_read(java_callbacks[i].name);
        }
    }

    return 0;
}

/* Iterate over `java_callbacks' and call all CB_TYPE_SHUTDOWN callbacks. */
static int cjni_shutdown_plugins(JNIEnv *jvm_env)
{
    int status;

    for (size_t i = 0; i < java_callbacks_num; i++) {
        if (java_callbacks[i].type != CB_TYPE_SHUTDOWN)
            continue;

        PLUGIN_DEBUG("Shutting down %s", java_callbacks[i].name);

        status = (*jvm_env)->CallIntMethod(jvm_env, java_callbacks[i].object,
                                                    java_callbacks[i].method);
        if (status != 0) {
            PLUGIN_ERROR("Shutting down `%s' failed with status %i. ",
                         java_callbacks[i].name, status);
        }
    }

    return 0;
}

static int cjni_shutdown(void)
{
    JNIEnv *jvm_env;
    JavaVMAttachArgs args = {0};
    int status;

    if (jvm == NULL)
        return 0;

    jvm_env = NULL;
    args.version = JNI_VERSION_1_2;

    status = (*jvm)->AttachCurrentThread(jvm, (void *)&jvm_env, &args);
    if (status != 0) {
        PLUGIN_ERROR("AttachCurrentThread failed with status %i.", status);
        return -1;
    }

    /* Execute all the shutdown functions registered by plugins. */
    cjni_shutdown_plugins(jvm_env);

    /* Release all the global references to callback functions */
    for (size_t i = 0; i < java_callbacks_num; i++) {
        if (java_callbacks[i].object != NULL) {
            (*jvm_env)->DeleteGlobalRef(jvm_env, java_callbacks[i].object);
            java_callbacks[i].object = NULL;
        }
        free(java_callbacks[i].name);
    }
    java_callbacks_num = 0;
    free(java_callbacks);

    /* Release all the global references to directly loaded classes. */
    for (size_t i = 0; i < java_classes_list_len; i++) {
        if (java_classes_list[i].object != NULL) {
            (*jvm_env)->DeleteGlobalRef(jvm_env, java_classes_list[i].object);
            java_classes_list[i].object = NULL;
        }
        free(java_classes_list[i].name);
    }
    java_classes_list_len = 0;
    free(java_classes_list);

    /* Destroy the JVM */
    PLUGIN_DEBUG("Destroying the JVM.");
    (*jvm)->DestroyJavaVM(jvm);
    jvm = NULL;
    jvm_env = NULL;

    pthread_key_delete(jvm_env_key);

    /* Free the JVM argument list */
    for (size_t i = 0; i < jvm_argc; i++)
        free(jvm_argv[i]);
    jvm_argc = 0;
    free(jvm_argv);

    return 0;
}

/* Initialization: Create a JVM, load all configured classes and call their
 * `config' and `init' callback methods. */
static int cjni_init(void)
{
    JNIEnv *jvm_env;

    if ((config_block == NULL) && (jvm == NULL)) {
        PLUGIN_ERROR("No configuration block for the java plugin was found.");
        return -1;
    }

    if (config_block != NULL) {
        cjni_config_perform(config_block);
        config_free(config_block);
    }

    if (jvm == NULL) {
        PLUGIN_ERROR("jvm == NULL");
        return -1;
    }

    jvm_env = cjni_thread_attach();
    if (jvm_env == NULL)
        return -1;

    cjni_init_plugins(jvm_env);

    cjni_thread_detach();
    return 0;
}

void module_register(void)
{
    plugin_register_config("java", cjni_config_callback);
    plugin_register_init("java", cjni_init);
    plugin_register_shutdown("java", cjni_shutdown);
}
