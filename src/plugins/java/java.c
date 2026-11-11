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

struct java_plugin_class_s {
    char *name;
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
    jobject object;
    jmethodID method;
};
typedef struct cjni_callback_info_s cjni_callback_info_t;

/*
 * Global variables
 */
static JavaVM *jvm;
static pthread_key_t jvm_env_key;
static bool jvm_failed;

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
    if (o_fam == NULL) {
        PLUGIN_ERROR("The metric family is null.");
        return -1;
    }

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
    if (o_notification == NULL) {
        PLUGIN_ERROR("The notification is null.");
        return -1;
    }

    notification_t n = {0};
    int status;

    status = jtoc_notification(jvm_env, &n, o_notification);
    if (status != 0) {
        PLUGIN_ERROR("jtoc_notification failed.");
        notification_reset(&n);
        return -1;
    }

    status = plugin_dispatch_notification(&n);

    notification_reset(&n);

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

    int status = plugin_register_complex_read("java", cbi->name, cjni_read,
                                              MS_TO_CDTIME_T(interval),
                                              &(user_data_t){
                                                .data = cbi,
                                                .free_func = cjni_callback_info_destroy, });

    (*jvm_env)->DeleteLocalRef(jvm_env, o_read);

    return (status == 0) ? 0 : -1;
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

    int status = plugin_register_write("java", cbi->name, cjni_write, NULL, 0, 0,
                                       &(user_data_t){ .data = cbi,
                                                       .free_func = cjni_callback_info_destroy });

    (*jvm_env)->DeleteLocalRef(jvm_env, o_write);

    return (status == 0) ? 0 : -1;
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

    int status = plugin_register_log("java", cbi->name, cjni_log,
                                     &(user_data_t){.data = cbi,
                                                    .free_func = cjni_callback_info_destroy, });

    (*jvm_env)->DeleteLocalRef(jvm_env, o_log);

    return (status == 0) ? 0 : -1;
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

    int status = plugin_register_notification("java", cbi->name, cjni_notification,
                                              &(user_data_t){
                                                .data = cbi,
                                                .free_func = cjni_callback_info_destroy });

    (*jvm_env)->DeleteLocalRef(jvm_env, o_notification);

    return (status == 0) ? 0 : -1;
}
static void JNICALL cjni_api_log(JNIEnv *jvm_env, __attribute__((unused)) jobject this,
                                 jint severity, jobject o_message)
{
    if (severity < LOG_ERR)
        severity = LOG_ERR;
    if (severity > LOG_DEBUG)
        severity = LOG_DEBUG;

    if (o_message == NULL) {
        plugin_log(severity, NULL, 0, NULL, "%s", "null");
        return;
    }

    const char *c_str = (*jvm_env)->GetStringUTFChars(jvm_env, o_message, 0);
    if (c_str == NULL) {
        PLUGIN_ERROR("cjni_api_log: GetStringUTFChars failed.");
        return;
    }

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

static void cjni_callback_info_free(cjni_callback_info_t *cbi)
{
    if (cbi == NULL)
        return;

    free(cbi->name);
    free(cbi);
}

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
        PLUGIN_ERROR("Unknown type: %#x", (unsigned int)type);
        return NULL;
    }

    if ((o_name == NULL) || (o_callback == NULL)) {
        PLUGIN_ERROR("The callback name and object must not be null.");
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
        PLUGIN_ERROR("strdup failed.");
        (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_name, c_name);
        cjni_callback_info_free(cbi);
        return NULL;
    }

    (*jvm_env)->ReleaseStringUTFChars(jvm_env, o_name, c_name);

    cbi->object = (*jvm_env)->NewGlobalRef(jvm_env, o_callback);
    if (cbi->object == NULL) {
        PLUGIN_ERROR("NewGlobalRef failed.");
        cjni_callback_info_free(cbi);
        return NULL;
    }

    jclass class = (*jvm_env)->GetObjectClass(jvm_env, cbi->object);
    if (class == NULL) {
        PLUGIN_ERROR("GetObjectClass failed.");
        (*jvm_env)->DeleteGlobalRef(jvm_env, cbi->object);
        cjni_callback_info_free(cbi);
        return NULL;
    }

    cbi->method = (*jvm_env)->GetMethodID(jvm_env, class, method_name, method_signature);
    (*jvm_env)->DeleteLocalRef(jvm_env, class);
    if (cbi->method == NULL) {
        PLUGIN_ERROR("Cannot find the `%s' method with signature `%s'.",
                     method_name, method_signature);
        (*jvm_env)->DeleteGlobalRef(jvm_env, cbi->object);
        cjni_callback_info_free(cbi);
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
        cjni_callback_info_free(cbi);
        return -1;
    }
    java_callbacks = tmp;
    java_callbacks[java_callbacks_num] = *cbi;
    java_callbacks_num++;

    pthread_mutex_unlock(&java_callbacks_lock);

    /* The name and the global reference are now owned by java_callbacks. */
    free(cbi);

    return 0;
}

/* Callback for `pthread_key_create'. It frees the data contained in
 * `jvm_env_key' and prints a warning if the reference counter is not zero. */
static void cjni_jvm_env_destroy(void *args)
{
    if ((args == NULL) || (jvm == NULL))
        return;

    (*jvm)->DetachCurrentThread(jvm);
}

/* Register ``native'' functions with the JVM. Native functions are C-functions
 * that can be called by Java code. */
static int cjni_init_native(JNIEnv *jvm_env)
{
    jclass api_class_ptr = (*jvm_env)->FindClass(jvm_env, "org/ncollectd/api/NCollectd");
    if (api_class_ptr == NULL) {
        cjni_exception_check(jvm_env, "class lookup", "org/ncollectd/api/NCollectd");
        PLUGIN_ERROR("Cannot find the API class \"org.ncollectd.api"
                     ".NCollectd\". Please set the correct class path "
                     "using 'JVMArg \"-Djava.class.path=...\"'.");
        return -1;
    }

    int status = (*jvm_env)->RegisterNatives(jvm_env, api_class_ptr, jni_api_functions,
                                                      (jint)jni_api_functions_num);
    (*jvm_env)->DeleteLocalRef(jvm_env, api_class_ptr);
    if (status != 0) {
        cjni_exception_check(jvm_env, "RegisterNatives", NULL);
        PLUGIN_ERROR("RegisterNatives failed with status %i.", status);
        return -1;
    }

    if (cjni_cache_classes(jvm_env) != 0)
        return -1;

    return 0;
}

/* Create the JVM. This is called when the first thread tries to access the JVM
 * via cjni_thread_attach. */
static int cjni_create_jvm(void)
{
    if (jvm != NULL)
        return 0;

    if (jvm_failed)
        return -1;
    jvm_failed = true;

    int status = pthread_key_create(&jvm_env_key, cjni_jvm_env_destroy);
    if (status != 0) {
        PLUGIN_ERROR("cjni_create_jvm: pthread_key_create failed with status %i.", status);
        return -1;
    }

    JNIEnv *jvm_env = NULL;
    JavaVMOption vm_options[jvm_argc + 1];
    JavaVMInitArgs vm_args = {0};
    vm_args.version = JNI_VERSION_1_2;
    vm_args.options = vm_options;
    vm_args.nOptions = (jint)jvm_argc + 1;

    /* Do not let the JVM install its own SIGINT, SIGTERM, SIGHUP and SIGQUIT
     * handlers: they replace the ones of ncollectd and exit the process without
     * the orderly shutdown of the daemon. */
    static char reduce_signals[] = "-Xrs";
    vm_options[0] = (JavaVMOption){ .optionString = reduce_signals };

    for (size_t i = 0; i < jvm_argc; i++) {
        PLUGIN_DEBUG("jvm_argv[%" PRIsz "] = %s", i, jvm_argv[i]);
        vm_options[i + 1] = (JavaVMOption){ .optionString = jvm_argv[i] };
    }

    JavaVM *new_jvm = NULL;
    status = JNI_CreateJavaVM(&new_jvm, (void *)&jvm_env, (void *)&vm_args);
    if (status != 0) {
        PLUGIN_ERROR("JNI_CreateJavaVM failed with status %i.", status);
        pthread_key_delete(jvm_env_key);
        return -1;
    }
    assert(new_jvm != NULL);
    assert(jvm_env != NULL);

    /* Call RegisterNatives. If it fails the JVM is not usable, and it cannot be
     * destroyed and created again in the same process. */
    status = cjni_init_native(jvm_env);
    if (status != 0) {
        PLUGIN_ERROR("cjni_create_jvm: cjni_init_native failed.");
        pthread_key_delete(jvm_env_key);
        return -1;
    }

    jvm = new_jvm;
    jvm_failed = false;

    PLUGIN_DEBUG("The JVM has been created.");
    return 0;
}

/* Return the JNI environment of the current thread, creating the JVM and
 * attaching the thread when needed. Native threads are attached as daemon
 * threads and stay attached until they exit (see cjni_jvm_env_destroy). */
static JNIEnv *cjni_thread_attach(void)
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

    JNIEnv *jvm_env = NULL;
    jint status = (*jvm)->GetEnv(jvm, (void **)&jvm_env, JNI_VERSION_1_2);
    if (status == JNI_OK)
        return jvm_env;

    if (status != JNI_EDETACHED) {
        PLUGIN_ERROR("GetEnv failed with status %i.", (int)status);
        return NULL;
    }

    JavaVMAttachArgs args = {0};
    args.version = JNI_VERSION_1_2;
    status = (*jvm)->AttachCurrentThreadAsDaemon(jvm, (void **)&jvm_env, (void *)&args);
    if (status != JNI_OK) {
        PLUGIN_ERROR("AttachCurrentThreadAsDaemon failed with status %i.", (int)status);
        return NULL;
    }

    pthread_setspecific(jvm_env_key, jvm_env);
    assert(jvm_env != NULL);
    return jvm_env;
}

/* Native threads stay attached and never return to Java, so the local
 * references they create are only released by a local frame. */
static int cjni_push_local_frame(JNIEnv *jvm_env, jint capacity)
{
    if ((*jvm_env)->PushLocalFrame(jvm_env, capacity) == 0)
        return 0;

    (*jvm_env)->ExceptionClear(jvm_env);
    PLUGIN_ERROR("Failed to allocate JVM local references.");
    return -1;
}

static int cjni_config_add_jvm_arg(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("`JVMArg' needs exactly one string argument.");
        return -1;
    }

    if ((jvm != NULL) || jvm_failed) {
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

    JNIEnv *jvm_env = cjni_thread_attach();
    if (jvm_env == NULL)
        return -1;

    java_plugin_class_t *class = realloc(java_classes_list,
                                         (java_classes_list_len + 1) * sizeof(*java_classes_list));
    if (class == NULL) {
        PLUGIN_ERROR("realloc failed.");
        return -1;
    }
    java_classes_list = class;
    class = java_classes_list + java_classes_list_len;

    memset(class, 0, sizeof(*class));
    class->name = strdup(ci->values[0].value.string);
    if (class->name == NULL) {
        PLUGIN_ERROR("strdup failed.");
        return -1;
    }
    class->object = NULL;

    /* Replace all dots ('.') with slashes ('/'). Dots are usually used
       thorough the Java community, but (Sun's) `FindClass' and friends need slashes. */
    for (size_t i = 0; class->name[i] != 0; i++) {
        if (class->name[i] == '.')
            class->name[i] = '/';
    }

    PLUGIN_DEBUG("Loading class %s", class->name);

    if (cjni_push_local_frame(jvm_env, 4) != 0) {
        free(class->name);
        return -1;
    }

    jclass c_plugin = (*jvm_env)->FindClass(jvm_env, class->name);
    if (c_plugin == NULL) {
        PLUGIN_ERROR("FindClass (%s) failed.", class->name);
        goto error;
    }

    jmethodID constructor_id = (*jvm_env)->GetMethodID(jvm_env, c_plugin, "<init>", "()V");
    if (constructor_id == NULL) {
        PLUGIN_ERROR("Could not find the constructor for `%s'.", class->name);
        goto error;
    }

    jobject tmp_object = (*jvm_env)->NewObject(jvm_env, c_plugin, constructor_id);
    if (tmp_object != NULL) {
        class->object = (*jvm_env)->NewGlobalRef(jvm_env, tmp_object);
    } else {
        class->object = NULL;
    }

    if (class->object == NULL) {
        PLUGIN_ERROR("Could not create a new `%s' object.", class->name);
        goto error;
    }

    (*jvm_env)->PopLocalFrame(jvm_env, NULL);

    java_classes_list_len++;

    return 0;

error:
    cjni_exception_check(jvm_env, "load-plugin", class->name);
    (*jvm_env)->PopLocalFrame(jvm_env, NULL);
    free(class->name);

    return -1;
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

    JNIEnv *jvm_env = cjni_thread_attach();
    if (jvm_env == NULL)
        return -1;

    if (cjni_push_local_frame(jvm_env, 8) != 0)
        return -1;

    jobject o_ocitem = ctoj_config_item(jvm_env, ci);
    if (o_ocitem == NULL) {
        cjni_exception_check(jvm_env, "config callback", name);
        (*jvm_env)->PopLocalFrame(jvm_env, NULL);
        return -1;
    }
    
    int status = (*jvm_env)->CallIntMethod(jvm_env, cbi->object, cbi->method, o_ocitem);
    if (cjni_exception_check(jvm_env, "config callback", name))
        status = -1;

    (*jvm_env)->PopLocalFrame(jvm_env, NULL);

    if (status != 0) {
        PLUGIN_ERROR("Configuring `%s' failed with status %i.", name, status);
        return -1;
    }

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
        cjni_callback_info_free(cbi);
        return;
    }

    if (arg == NULL)
        return;

    JNIEnv *jvm_env = cjni_thread_attach();
    if (jvm_env == NULL) {
        PLUGIN_ERROR("cjni_thread_attach failed.");
        return;
    }

    (*jvm_env)->DeleteGlobalRef(jvm_env, cbi->object);

    cbi->method = NULL;
    cbi->object = NULL;
    cjni_callback_info_free(cbi);
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

    JNIEnv *jvm_env = cjni_thread_attach();
    if (jvm_env == NULL)
        return -1;

    if (cjni_push_local_frame(jvm_env, 16) != 0)
        return -1;

    cjni_callback_info_t *cbi = (cjni_callback_info_t *)ud->data;

    int ret_status = (*jvm_env)->CallIntMethod(jvm_env, cbi->object, cbi->method);
    if (cjni_exception_check(jvm_env, "read callback", cbi->name))
        ret_status = -1;

    (*jvm_env)->PopLocalFrame(jvm_env, NULL);

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

    if (cjni_push_local_frame(jvm_env, 16) != 0)
        return -1;

    cjni_callback_info_t *cbi = (cjni_callback_info_t *)ud->data;

    jobject fam_java = ctoj_metric_family(jvm_env, fam);
    if (fam_java == NULL) {
        cjni_exception_check(jvm_env, "write callback", cbi->name);
        (*jvm_env)->PopLocalFrame(jvm_env, NULL);
        PLUGIN_ERROR("ctoj_metric_family failed.");
        return -1;
    }

    int ret_status = (*jvm_env)->CallIntMethod(jvm_env, cbi->object, cbi->method, fam_java);
    if (cjni_exception_check(jvm_env, "write callback", cbi->name))
        ret_status = -1;

    (*jvm_env)->PopLocalFrame(jvm_env, NULL);

    return ret_status;
}

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

    /* We may be called from native code with a pending exception, e.g. when an
     * error is logged inside a native method: keep it for the caller. */
    jthrowable o_pending = NULL;
    if ((*jvm_env)->ExceptionCheck(jvm_env)) {
        o_pending = (*jvm_env)->ExceptionOccurred(jvm_env);
        (*jvm_env)->ExceptionClear(jvm_env);
    }

    cjni_callback_info_t *cbi = (cjni_callback_info_t *)ud->data;

    jobject o_message = (*jvm_env)->NewStringUTF(jvm_env, msg->msg);
    if (o_message != NULL) {
        (*jvm_env)->CallVoidMethod(jvm_env, cbi->object, cbi->method, (jint)msg->severity,
                                            o_message);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_message);
    }

    cjni_exception_check(jvm_env, "log callback", cbi->name);

    if (o_pending != NULL) {
        (*jvm_env)->Throw(jvm_env, o_pending);
        (*jvm_env)->DeleteLocalRef(jvm_env, o_pending);
    }
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

    JNIEnv *jvm_env = cjni_thread_attach();
    if (jvm_env == NULL)
        return -1;

    if (cjni_push_local_frame(jvm_env, 16) != 0)
        return -1;

    cjni_callback_info_t *cbi = (cjni_callback_info_t *)ud->data;

    jobject o_notification = ctoj_notification(jvm_env, n);
    if (o_notification == NULL) {
        cjni_exception_check(jvm_env, "notification callback", cbi->name);
        PLUGIN_ERROR("ctoj_notification failed.");
        return -1;
    }

    int ret_status = (*jvm_env)->CallIntMethod(jvm_env, cbi->object, cbi->method, o_notification);
    if (cjni_exception_check(jvm_env, "notification callback", cbi->name))
        ret_status = -1;

    (*jvm_env)->PopLocalFrame(jvm_env, NULL);

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
        if (cjni_exception_check(jvm_env, "init callback", java_callbacks[i].name))
            status = -1;
        if (status != 0) {
            PLUGIN_ERROR("Initializing `%s' failed with status %i. Removing read function.",
                         java_callbacks[i].name, status);
            char full_name[1024];
            ssnprintf(full_name, sizeof(full_name), "java/%s", java_callbacks[i].name);
            plugin_unregister_read(full_name);
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
        if (cjni_exception_check(jvm_env, "shutdown callback", java_callbacks[i].name))
            status = -1;
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

    /* Destroy the JVM. Clear `jvm' and the thread-specific key first, so that
     * callbacks and exiting threads do not use the JVM while it is destroyed. */
    PLUGIN_DEBUG("Destroying the JVM.");
    JavaVM *vm = jvm;
    jvm = NULL;

    pthread_key_delete(jvm_env_key);

    (*vm)->DestroyJavaVM(vm);
    jvm_env = NULL;

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

    JNIEnv *jvm_env = cjni_thread_attach();
    if (jvm_env == NULL)
        return -1;

    cjni_init_plugins(jvm_env);

    return 0;
}

void module_register(void)
{
    plugin_register_config("java", cjni_config_callback);
    plugin_register_init("java", cjni_init);
    plugin_register_shutdown("java", cjni_shutdown);
}
