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

jcached_refs_t gref;

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
 * Functions accessible from Java
 */
static jint JNICALL cjni_api_dispatch_metric_family(JNIEnv *jvm_env,
                                                    __attribute__((unused)) jobject this,
                                                    jobject o_fam)
{
    metric_family_t fam = {0};

    int status = jtoc_metric_family(jvm_env, &fam, o_fam);
    if (status != 0) {
        PLUGIN_ERROR("jtoc_metric_family failed.");
        return -1;
    }

    plugin_dispatch_metric_family(&fam, 0);

    free(fam.name);
    free(fam.help);
    free(fam.unit);
    metric_list_reset(&fam.metric, fam.type);

    return 0;
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

static jint JNICALL cjni_api_register_read_time(JNIEnv *jvm_env,
                                                __attribute__((unused)) jobject this,
                                                jobject o_name, jobject o_read, jlong interval)
{
    cjni_callback_info_t *cbi;

    cbi = cjni_callback_info_create(jvm_env, o_name, o_read, CB_TYPE_READ);
    if (cbi == NULL)
        return -1;

    PLUGIN_DEBUG("Registering new read callback: %s", cbi->name);

    plugin_register_complex_read("java", cbi->name, cjni_read, MS_TO_CDTIME_T(interval),
            &(user_data_t){.data = cbi, .free_func = cjni_callback_info_destroy, });

    (*jvm_env)->DeleteLocalRef(jvm_env, o_read);

    return 0;
}

static jint JNICALL cjni_api_register_read(JNIEnv *jvm_env, jobject this,
                                           jobject o_name, jobject o_read)
{
    return cjni_api_register_read_time(jvm_env, this, o_name, o_read, 0);
}

static jint JNICALL cjni_api_register_write(JNIEnv *jvm_env, __attribute__((unused)) jobject this,
                                            jobject o_name, jobject o_write)
{
    cjni_callback_info_t *cbi = cjni_callback_info_create(jvm_env, o_name, o_write, CB_TYPE_WRITE);
    if (cbi == NULL)
        return -1;

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

/* List of ``native'' functions, i. e. C-functions that can be called from Java. */
static JNINativeMethod jni_api_functions[] = {
    {"dispatchMetricFamily", "(Lorg/ncollectd/api/MetricFamily;)I",
                             cjni_api_dispatch_metric_family },
    {"dispatchNotification", "(Lorg/ncollectd/api/Notification;)I",
                             cjni_api_dispatch_notification },
    {"registerConfig",       "(Ljava/lang/String;Lorg/ncollectd/api/NCollectdConfigInterface;)I",
                             cjni_api_register_config },
    {"registerInit",         "(Ljava/lang/String;Lorg/ncollectd/api/NCollectdInitInterface;)I",
                             cjni_api_register_init },
    {"registerRead",         "(Ljava/lang/String;Lorg/ncollectd/api/NCollectdReadInterface;)I",
                             cjni_api_register_read },
    {"registerRead",         "(Ljava/lang/String;Lorg/ncollectd/api/NCollectdReadInterface;J)I",
                             cjni_api_register_read_time },
    {"registerWrite",        "(Ljava/lang/String;Lorg/ncollectd/api/NCollectdWriteInterface;)I",
                             cjni_api_register_write },
    {"registerShutdown",     "(Ljava/lang/String;Lorg/ncollectd/api/NCollectdShutdownInterface;)I",
                             cjni_api_register_shutdown },
    {"registerLog",          "(Ljava/lang/String;Lorg/ncollectd/api/NCollectdLogInterface;)I",
                             cjni_api_register_log },
    {"registerNotification", "(Ljava/lang/String;Lorg/ncollectd/api/NCollectdNotificationInterface;)I",
                             cjni_api_register_notification },
    {"log",                  "(ILjava/lang/String;)V",
                             cjni_api_log },
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
        PLUGIN_ERROR("Cannot find the `%s' method with signature `%s'.",
                     method_name, method_signature);
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
#ifdef NCOLLECTD_DEBUG
    const char *type_str;
#endif

    cjni_callback_info_t *cbi = cjni_callback_info_create(jvm_env, o_name, o_callback, type);
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

    cjni_callback_info_t *tmp = realloc(java_callbacks,
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
    if (args == NULL)
        return;

    cjni_jvm_env_t *cjni_env = (cjni_jvm_env_t *)args;

    if (cjni_env->reference_counter > 0) {
        PLUGIN_ERROR("cjni_env->reference_counter = %i;", cjni_env->reference_counter);
    }

    if (cjni_env->jvm_env != NULL) {
        PLUGIN_ERROR("cjni_env->jvm_env = %p;", (void *)cjni_env->jvm_env);
    }

    /* The pointer is allocated in cjni_thread_attach */
    free(cjni_env);
}

/* Register ``native'' functions with the JVM. Native functions are C-functions
 * that can be called by Java code. */
static int cjni_init_native(JNIEnv *jvm_env)
{
    jclass api_class_ptr = (*jvm_env)->FindClass(jvm_env, "org/ncollectd/api/NCollectd");
    if (api_class_ptr == NULL) {
        PLUGIN_ERROR("Cannot find the API class \"org.ncollectd.api"
                     ".NCollectd\". Please set the correct class path "
                     "using 'JVMArg \"-Djava.class.path=...\"'.");
        return -1;
    }

    int status = (*jvm_env)->RegisterNatives(jvm_env, api_class_ptr, jni_api_functions,
                                                      (jint)jni_api_functions_num);
    if (status != 0) {
        PLUGIN_ERROR("RegisterNatives failed with status %i.", status);
        return -1;
    }

    cjni_cache_classes(jvm_env);

    return 0;
}

/* Create the JVM. This is called when the first thread tries to access the JVM
 * via cjni_thread_attach. */
static int cjni_create_jvm(void)
{
    if (jvm != NULL)
        return 0;

    int status = pthread_key_create(&jvm_env_key, cjni_jvm_env_destroy);
    if (status != 0) {
        PLUGIN_ERROR("cjni_create_jvm: pthread_key_create failed with status %i.", status);
        return -1;
    }

    JNIEnv *jvm_env = NULL;
    JavaVMOption vm_options[jvm_argc];
    JavaVMInitArgs vm_args = {0};
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
static JNIEnv *cjni_thread_attach(jboolean once)
{
    /* If we're the first thread to access the JVM, we'll have to create it first.. */
    if (jvm == NULL) {
        int status = cjni_create_jvm();
        if (status != 0) {
            PLUGIN_ERROR("cjni_create_jvm failed.");
            return NULL;
        }
    }

    assert(jvm != NULL);

    cjni_jvm_env_t *cjni_env = pthread_getspecific(jvm_env_key);
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

    JNIEnv *jvm_env = NULL;

    if (cjni_env->reference_counter > 0) {
        if (once == JNI_FALSE)
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
    cjni_jvm_env_t *cjni_env = pthread_getspecific(jvm_env_key);
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

    int status = (*jvm)->DetachCurrentThread(jvm);
    if (status != 0) {
        PLUGIN_ERROR("DetachCurrentThread failed with status %i.", status);
    }

    cjni_env->reference_counter = 0;
    cjni_env->jvm_env = NULL;

    return 0;
}

static int cjni_config_add_jvm_arg(config_item_t *ci)
{
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

    char **tmp = realloc(jvm_argv, sizeof(char *) * (jvm_argc + 1));
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
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("`LoadPlugin' needs exactly one string argument.");
        return -1;
    }

    JNIEnv *jvm_env = cjni_thread_attach(JNI_FALSE);
    if (jvm_env == NULL)
        return -1;

    java_plugin_class_t *class = realloc(java_classes_list,
                                         (java_classes_list_len + 1) * sizeof(*java_classes_list));
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

    jmethodID constructor_id = (*jvm_env)->GetMethodID(jvm_env, class->class, "<init>", "()V");
    if (constructor_id == NULL) {
        PLUGIN_ERROR("Could not find the constructor for `%s'.", class->name);
        cjni_thread_detach();
        free(class->name);
        return -1;
    }

    jobject tmp_object = (*jvm_env)->NewObject(jvm_env, class->class, constructor_id);
    if (tmp_object != NULL) {
        class->object = (*jvm_env)->NewGlobalRef(jvm_env, tmp_object);
    } else {
        class->object = NULL;
    }

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
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("'Plugin' blocks need exactly one string argument.");
        return -1;
    }

    const char *name = ci->values[0].value.string;

    cjni_callback_info_t *cbi = NULL;
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

    JNIEnv *jvm_env = cjni_thread_attach(JNI_FALSE);
    if (jvm_env == NULL)
        return -1;

    jobject o_ocitem = ctoj_config_item(jvm_env, ci);
    if (o_ocitem == NULL) {
        PLUGIN_ERROR("ctoj_config_item failed.");
        cjni_thread_detach();
        return -1;
    }

    jclass class = (*jvm_env)->GetObjectClass(jvm_env, cbi->object);
    jmethodID method = (*jvm_env)->GetMethodID(jvm_env, class, "config",
                                                        "(Lorg/ncollectd/api/ConfigItem;)I");

    (*jvm_env)->CallIntMethod(jvm_env, cbi->object, method, o_ocitem);

    (*jvm_env)->DeleteLocalRef(jvm_env, o_ocitem);
    cjni_thread_detach();
    return 0;
}

static int cjni_config_perform(config_item_t *ci)
{
    int success = 0;
    int errors = 0;
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("jvm-arg", child->key) == 0) {
            status = cjni_config_add_jvm_arg(child);
            if (status == 0)
                success++;
            else
                errors++;
        } else if (strcasecmp("load-plugin", child->key) == 0) {
            status = cjni_config_load_plugin(child);
            if (status == 0)
                success++;
            else
                errors++;
        } else if (strcasecmp("plugin", child->key) == 0) {
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
    assert(ci != NULL);
    if (ci->children_num == 0)
        return 0; /* nothing to do */

    config_item_t *ci_copy = config_clone(ci);
    if (ci_copy == NULL) {
        PLUGIN_ERROR("config_clone failed.");
        return -1;
    }

    if (config_block == NULL) {
        config_block = ci_copy;
        return 0;
    }

    config_item_t *tmp = realloc(config_block->children,
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
    PLUGIN_DEBUG("(arg = %p);", arg);

    cjni_callback_info_t *cbi = (cjni_callback_info_t *)arg;

    /* This condition can occur when shutting down. */
    if (jvm == NULL) {
        free(cbi);
        return;
    }

    if (arg == NULL)
        return;

    JNIEnv *jvm_env = cjni_thread_attach(JNI_FALSE);
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
    if (jvm == NULL) {
        PLUGIN_ERROR("jvm == NULL");
        return -1;
    }

    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    JNIEnv *jvm_env = cjni_thread_attach(JNI_TRUE);
    if (jvm_env == NULL)
        return -1;

    cjni_callback_info_t *cbi = (cjni_callback_info_t *)ud->data;

    int ret_status = (*jvm_env)->CallIntMethod(jvm_env, cbi->object, cbi->method);

    // cjni_thread_detach();
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

    JNIEnv *jvm_env = cjni_thread_attach(JNI_TRUE);
    if (jvm_env == NULL)
        return -1;

    cjni_callback_info_t *cbi = (cjni_callback_info_t *)ud->data;

    jobject fam_java = ctoj_metric_family(jvm_env, fam);
    if (fam_java == NULL) {
        PLUGIN_ERROR("ctoj_metric_family failed.");
        // cjni_thread_detach();
        return -1;
    }

    int ret_status = (*jvm_env)->CallIntMethod(jvm_env, cbi->object, cbi->method, fam_java);

    (*jvm_env)->DeleteLocalRef(jvm_env, fam_java);

    // cjni_thread_detach();
    return ret_status;
}

/* Call the CB_TYPE_LOG callback pointed to by the `user_data_t' pointer. */
static void cjni_log(const log_msg_t *msg, user_data_t *ud)
{
    if (jvm == NULL)
        return;

    if ((ud == NULL) || (ud->data == NULL))
        return;

    JNIEnv *jvm_env = cjni_thread_attach(JNI_FALSE);
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
    if (jvm == NULL) {
        PLUGIN_ERROR("jvm == NULL");
        return -1;
    }

    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    JNIEnv *jvm_env = cjni_thread_attach(JNI_FALSE);
    if (jvm_env == NULL)
        return -1;

    cjni_callback_info_t *cbi = (cjni_callback_info_t *)ud->data;

    jobject o_notification = ctoj_notification(jvm_env, n);
    if (o_notification == NULL) {
        PLUGIN_ERROR("ctoj_notification failed.");
        cjni_thread_detach();
        return -1;
    }

    int ret_status = (*jvm_env)->CallIntMethod(jvm_env, cbi->object, cbi->method, o_notification);

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
    if (jvm == NULL)
        return 0;

    JNIEnv *jvm_env = NULL;
    JavaVMAttachArgs args = {0};
    args.version = JNI_VERSION_1_2;

    int status = (*jvm)->AttachCurrentThread(jvm, (void *)&jvm_env, &args);
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

    cjni_cache_classes_release(jvm_env);

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

    JNIEnv *jvm_env = cjni_thread_attach(JNI_FALSE);
    if (jvm_env == NULL)
        return -1;

    cjni_init_plugins(jvm_env);

    /* Since we have loaded classes and methods with this thread. It has to remain attached */
    // cjni_thread_detach();
    return 0;
}

void module_register(void)
{
    plugin_register_config("java", cjni_config_callback);
    plugin_register_init("java", cjni_init);
    plugin_register_shutdown("java", cjni_shutdown);
}
