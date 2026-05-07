// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wpedantic"
#if __GNUC__ >= 8
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

#include "libquickjs/cutils.h"
#include "libquickjs/quickjs-libc.h"

#include "jmetric.h"
#include "jmetricfamily.h"
#include "jnotification.h"

typedef enum {
    QJS_CB_INIT,
    QJS_CB_READ,
    QJS_CB_WRITE,
    QJS_CB_SHUTDOWN,
    QJS_CB_CONFIG,
    QJS_CB_NOTIFICATION,
} qjs_cb_type_t;

struct qjs_callback;
typedef struct qjs_callback qjs_callback_t;

struct qjs_script;
typedef struct qjs_script qjs_script_t;

struct qjs_callback {
    JSValue cb;
    JSValue data;
    char *plugin_name;
    char *plugin_full_name;
    char *name;
    qjs_script_t *qjs;
    qjs_callback_t *next;
};

struct qjs_script {
    char *filename;
    size_t memory_limit;
    size_t stack_size;
    bool load_std;
    char **includes;
    size_t includes_num;
    JSRuntime *rt;
    JSContext *ctx;
    qjs_callback_t *cb_init;
    qjs_callback_t *cb_shutdown;
    qjs_callback_t *cb_config;
    pthread_mutex_t lock;
    qjs_script_t *next;
};

static char base_path[PATH_MAX];
static qjs_script_t *qjs_scripts;
static config_item_t *config_block;

static void qjs_strbuf_value(void *opaque, const char *buf, size_t len)
{
    strbuf_putstrn((strbuf_t *)opaque, buf, len);
}

static void qjs_dump_error(JSContext *ctx)
{
    char buff[4096];
    strbuf_t sbuf = STRBUF_CREATE_FIXED(buff, sizeof(buff));

    JSValue exception_val = JS_GetException(ctx);
    JS_PrintValue(ctx, qjs_strbuf_value, &sbuf, exception_val, NULL);
    JS_FreeValue(ctx, exception_val);

    PLUGIN_ERROR("%s", sbuf.ptr);
}

static JSValue qjs_from_config(JSContext *ctx, config_item_t *ci)
{
    if (ci == NULL)
        return JS_NULL;

    JSValue jci = JS_NewObject(ctx);
    if (JS_IsException(jci))
        return jci;

    JS_DefinePropertyValueStr(ctx, jci, "key", JS_NewString(ctx, ci->key), JS_PROP_C_W_E);

    JSValue jvalues = JS_NewArray(ctx);
    if (JS_IsException(jvalues))
        goto fail;

    for (int i = 0; i < ci->values_num; i++) {
        config_value_t *value = &ci->values[i];

        JSValue jvalue = JS_NULL;

        switch(value->type) {
        case CONFIG_TYPE_STRING:
            jvalue = JS_NewString(ctx, value->value.string);
            break;
        case CONFIG_TYPE_NUMBER:
            jvalue = JS_NewFloat64(ctx, value->value.number);
            break;
        case CONFIG_TYPE_BOOLEAN:
            jvalue = JS_NewBool(ctx, value->value.boolean);
            break;
        case CONFIG_TYPE_REGEX:
            jvalue = JS_NewString(ctx, value->value.string);
            break;
        }

        JS_DefinePropertyValueUint32(ctx, jvalues, i, jvalue, JS_PROP_C_W_E);
    }

    JS_DefinePropertyValueStr(ctx, jci, "values", jvalues, JS_PROP_C_W_E);


    JSValue jchildrens = JS_NewArray(ctx);
    if (JS_IsException(jchildrens))
        goto fail;

    for (int i = 0; i < ci->children_num; i++) {

        JSValue jchildren = qjs_from_config(ctx, &ci->children[i]);
        if (JS_IsException(jchildren)) {
            JS_FreeValue(ctx, jchildrens);
            goto fail;
        }

        JS_DefinePropertyValueUint32(ctx, jchildrens, i, jchildren, JS_PROP_C_W_E);
    }

    JS_DefinePropertyValueStr(ctx, jci, "childrens", jchildrens, JS_PROP_C_W_E);

    return jci;

fail:
    JS_FreeValue(ctx, jci);
    return JS_EXCEPTION;
}

static JSValue qjs_call0(JSContext *ctx, JSValueConst func, JSValue data)
{
    JSValue func_dup = JS_DupValue(ctx, func);
    JSValue ret;
    if (!JS_IsNull(data))
        ret = JS_Call(ctx, func_dup, JS_UNDEFINED, 1, (JSValueConst *)&data);
    else
        ret = JS_Call(ctx, func_dup, JS_UNDEFINED, 0, NULL);
    JS_FreeValue(ctx, func_dup);

    return ret;
}

static JSValue qjs_call1(JSContext *ctx, JSValueConst func, JSValue data, JSValue value)
{
    JSValueConst argv[2];
    int argc = 0;

    argv[argc] = value;
    if (!JS_IsNull(data)) {
        argc++;
        argv[argc] = data;
    }

    JSValue func_dup = JS_DupValue(ctx, func);
    JSValue ret = JS_Call(ctx, func_dup, JS_UNDEFINED, argc+1, argv);

    JS_FreeValue(ctx, func_dup);

    return ret;
}

static void qjs_callback_free(void *data)
{
    if (data == NULL)
        return;

    qjs_callback_t *qjc = data;

    JS_FreeValue(qjc->qjs->ctx, qjc->cb);
    JS_FreeValue(qjc->qjs->ctx, qjc->data);
    free(qjc->plugin_name);
    free(qjc->plugin_full_name);
    free(qjc->name);

    free(qjc);
}

static void qjs_callback_list_remove(qjs_callback_t **head, const char *name)
{
    qjs_callback_t *qjc = *head;
    qjs_callback_t *prev = NULL;
    while (qjc != NULL) {
        if (strcmp(qjc->plugin_full_name, name) == 0)
            break;
        prev = qjc;
        qjc = qjc->next;
    }

    if (qjc == NULL)
        return;

    if (prev == NULL)
        *head = qjc->next;
    else
        prev->next = qjc->next;

    qjs_callback_free(qjc);
}

static void qjs_callback_list_free(qjs_callback_t *qjc)
{
    if (qjc == NULL)
        return;

    while(qjc != NULL){
        qjs_callback_t *next = qjc->next;
        qjs_callback_free(qjc);
        qjc = next;
    }
}

static qjs_callback_t *qjs_callback_alloc(qjs_script_t *qjs, JSValueConst func,
                                          JSValueConst data, JSValueConst name)
{
    JSValue dup_func = JS_DupValue(qjs->ctx, func);
    if (JS_IsNull(dup_func))
        return NULL;

    JSValue dup_data = JS_UNDEFINED;
    if (!JS_IsNull(data)) {
        dup_data = JS_DupValue(qjs->ctx, data);
        if (JS_IsNull(dup_data)) {
            JS_FreeValue(qjs->ctx, dup_func);
            return NULL;
        }
    }

    char *dup_name = NULL;
    if (JS_IsString(name)) {
        const char *jname = JS_ToCString(qjs->ctx, name);
        if (jname != NULL) {
            dup_name = strdup(jname);
            JS_FreeCString(qjs->ctx, jname);
        }
    }

    qjs_callback_t *qjc = calloc(1, sizeof(*qjc));
    if (qjc == NULL) {
        JS_FreeValue(qjs->ctx, dup_func);
        JS_FreeValue(qjs->ctx, dup_data);
        return NULL;
    }

    qjc->qjs = qjs;
    qjc->cb = dup_func;
    qjc->data = dup_data;
    qjc->name = dup_name;

    char buffer[256];
    ssnprintf(buffer, sizeof(buffer), "%p", (void *)qjc);
    qjc->plugin_name = strdup(buffer);

    ssnprintf(buffer, sizeof(buffer), "javascript/%p", (void *)qjc);
    qjc->plugin_full_name = strdup(buffer);

#if 0
    if (!JS_IsNull(func)) {
        JSValue jfunc_name = JS_GetPropertyStr(ctx, func, "name");
        if (!JS_IsException(jfunc_name)) {
            const char *func_name = JS_ToCString(ctx, jfunc_name);
            if (func_name != NULL) {
                if (strbuf_len(buf) > 0)
                    strbuf_putchar(buf, '/');
                strbuf_putstr(buf, func_name);
                JS_FreeCString(ctx, func_name);
            }
        }
    }
#endif
    return qjc;
}

static int qjs_cb_call(qjs_callback_t *qjc)
{
    pthread_mutex_lock(&qjc->qjs->lock);

    JS_UpdateStackTop(qjc->qjs->rt);
    JSValue ret = qjs_call0(qjc->qjs->ctx, qjc->cb, qjc->data);
    js_std_loop(qjc->qjs->ctx);
    if (JS_IsException(ret)) {
        qjs_dump_error(qjc->qjs->ctx);
        pthread_mutex_unlock(&qjc->qjs->lock);
        return -1;
    }

    JS_FreeValue(qjc->qjs->ctx, ret);

    pthread_mutex_unlock(&qjc->qjs->lock);

    return 0;
}

static int qjs_cb_config(qjs_callback_t *qjc, config_item_t *ci)
{
    pthread_mutex_lock(&qjc->qjs->lock);

    JSValue jconfig = qjs_from_config(qjc->qjs->ctx, ci);
    if (JS_IsException(jconfig)) {
        PLUGIN_ERROR("Failed to convert configuration");
        pthread_mutex_unlock(&qjc->qjs->lock);
        return -1;
    }

    JS_UpdateStackTop(qjc->qjs->rt);
    JSValue ret = qjs_call1(qjc->qjs->ctx, qjc->cb, qjc->data, jconfig);
    js_std_loop(qjc->qjs->ctx);
    if (JS_IsException(ret)) {
        qjs_dump_error(qjc->qjs->ctx);
        pthread_mutex_unlock(&qjc->qjs->lock);
        return -1;
    }

    JS_FreeValue(qjc->qjs->ctx, ret);
    JS_FreeValue(qjc->qjs->ctx, jconfig);

    pthread_mutex_unlock(&qjc->qjs->lock);

    return 0;
}

static int qjs_read(user_data_t *user_data)
{
    if (user_data == NULL)
        return -1;

    qjs_callback_t *qjc = user_data->data;

    pthread_mutex_lock(&qjc->qjs->lock);

    JS_UpdateStackTop(qjc->qjs->rt);
    JSValue ret = qjs_call0(qjc->qjs->ctx, qjc->cb, qjc->data);
    js_std_loop(qjc->qjs->ctx);
    if (JS_IsException(ret)) {
        qjs_dump_error(qjc->qjs->ctx);
        pthread_mutex_unlock(&qjc->qjs->lock);
        return -1;
    }

    JS_FreeValue(qjc->qjs->ctx, ret);

    pthread_mutex_unlock(&qjc->qjs->lock);

    return 0;
}

static int qjs_write(metric_family_t const *fam, user_data_t *user_data)
{
    if ((fam == NULL) || (user_data == NULL))
        return -1;

    qjs_callback_t *qjc = user_data->data;

    pthread_mutex_lock(&qjc->qjs->lock);

    JSValue jfam = qjs_metric_family_new(qjc->qjs->ctx, fam);
    if (JS_IsException(jfam)) {
        qjs_dump_error(qjc->qjs->ctx);
        pthread_mutex_unlock(&qjc->qjs->lock);
        return -1;
    }

    JS_UpdateStackTop(qjc->qjs->rt);
    JSValue ret = qjs_call1(qjc->qjs->ctx, qjc->cb, qjc->data, jfam);
    js_std_loop(qjc->qjs->ctx);
    if (JS_IsException(ret)) {
        qjs_dump_error(qjc->qjs->ctx);
        JS_FreeValue(qjc->qjs->ctx, jfam);
        pthread_mutex_unlock(&qjc->qjs->lock);
        return -1;
    }

    JS_FreeValue(qjc->qjs->ctx, ret);

    JS_FreeValue(qjc->qjs->ctx, jfam);

    pthread_mutex_unlock(&qjc->qjs->lock);

    return 0;
}

static int qjs_notification(const notification_t *n, user_data_t *user_data)
{
    if ((n == NULL) || (user_data == NULL))
        return -1;

    qjs_callback_t *qjc = user_data->data;

    pthread_mutex_lock(&qjc->qjs->lock);

    JSValue jn = qjs_notification_new(qjc->qjs->ctx, n);
    if (JS_IsException(jn)) {
        qjs_dump_error(qjc->qjs->ctx);
        pthread_mutex_unlock(&qjc->qjs->lock);
        return -1;
    }

    JS_UpdateStackTop(qjc->qjs->rt);
    JSValue ret = qjs_call1(qjc->qjs->ctx, qjc->cb, qjc->data, jn);
    js_std_loop(qjc->qjs->ctx);
    if (JS_IsException(ret)) {
        qjs_dump_error(qjc->qjs->ctx);
        JS_FreeValue(qjc->qjs->ctx, jn);
        pthread_mutex_unlock(&qjc->qjs->lock);
        return -1;
    }

    JS_FreeValue(qjc->qjs->ctx, ret);

    JS_FreeValue(qjc->qjs->ctx, jn);

    pthread_mutex_unlock(&qjc->qjs->lock);

    return 0;
}

static JSValue qjs_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv,
                                       int level)
{
    JSPrintValueOptions opts = {0};
    char buff[65536];
    strbuf_t sbuf = STRBUF_CREATE_FIXED(buff, sizeof(buff));

    for(int i = 0; i < argc; i++) {
        if (i != 0)
            strbuf_putchar(&sbuf, ' ');
        JSValueConst v = argv[i];
        if (JS_IsString(v)) {
            size_t len;
            const char *str = JS_ToCStringLen(ctx, &len, v);
            if (!str)
                return JS_EXCEPTION;
            strbuf_putstrn(&sbuf, str, len);
            JS_FreeCString(ctx, str);
        } else {
            JS_PrintValue(ctx, qjs_strbuf_value, &sbuf, argv[0], &opts);
        }
    }

    // TODO add js file  and line nuber
    plugin_log(level, NULL, 0, NULL, "%s", sbuf.ptr);

    return JS_UNDEFINED;
}

static JSValue qjs_register_generic(JSContext *ctx, JSValueConst this_val, int argc,
                                    JSValueConst *argv, int type)
{
    qjs_script_t *qjs = JS_GetContextOpaque(ctx);
    if (qjs == NULL)
        return JS_UNDEFINED;

    JSValueConst func = JS_UNDEFINED;
    JSValueConst data = JS_UNDEFINED;
    JSValueConst name = JS_UNDEFINED;
    cdtime_t interval = 0;

    if (argc == 0)
        return JS_ThrowTypeError(ctx, "missing function");
    if (argc > 0) {
        func = argv[0];
        if (JS_IsNull(func))
            return JS_ThrowTypeError(ctx, "function is null");
        if (!JS_IsFunction(ctx, func))
            return JS_ThrowTypeError(ctx, "not a function");
    }

    if (type == QJS_CB_READ) {
        if ((argc > 1) && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
            double jinterval = 0;
            if (JS_ToFloat64(ctx, &jinterval, argv[1]))
                return JS_ThrowTypeError(ctx, "interval is not a number");
            interval = DOUBLE_TO_CDTIME_T(jinterval);
        }
        if ((argc > 2) && !JS_IsNull(argv[2]))
            data = argv[1];
        if ((argc > 3) && !JS_IsNull(argv[3]))
            name = argv[2];
    } else {
        if ((argc > 1) && !JS_IsNull(argv[1]))
            data = argv[1];
        if ((argc > 2) && !JS_IsNull(argv[2]))
            name = argv[2];
    }

    qjs_callback_t *qjc = qjs_callback_alloc(qjs, func, data, name);
    if (qjc == NULL)
        return JS_ThrowTypeError(ctx, "cannot alloc callback");

    switch(type) {
    case QJS_CB_INIT:
        qjc->next = qjs->cb_init;
        qjs->cb_init = qjc;
        break;
    case QJS_CB_READ:
        plugin_register_complex_read("javascript", qjc->plugin_name, qjs_read, interval,
                                     &(user_data_t){.data = qjc, .free_func = qjs_callback_free});
        break;
    case QJS_CB_WRITE:
        plugin_register_write("javascript", qjc->plugin_name, qjs_write, NULL, 0, 0,
                              &(user_data_t){.data = qjc, .free_func = qjs_callback_free});
        break;
    case QJS_CB_SHUTDOWN:
        qjc->next = qjs->cb_shutdown;
        qjs->cb_shutdown = qjc;
        break;
    case QJS_CB_CONFIG:
        qjc->next = qjs->cb_config;
        qjs->cb_config = qjc;
        break;
    case QJS_CB_NOTIFICATION:
        plugin_register_notification("javascript", qjc->plugin_name, qjs_notification,
                                     &(user_data_t){.data = qjc, .free_func = qjs_callback_free});
        break;
    }

    return JS_NewString(ctx, qjc->plugin_full_name);
}

static JSValue qjs_unregister_generic(JSContext *ctx, JSValueConst this_val, int argc,
                                      JSValueConst *argv, int type)
{
    qjs_script_t *qjs = JS_GetContextOpaque(ctx);
    if (qjs == NULL)
        return JS_UNDEFINED;

    if (argc != 1)
        return JS_ThrowTypeError(ctx, "missing identifier");

    if (JS_IsNull(argv[0]))
        return JS_ThrowTypeError(ctx, "identifier is null");
    if (!JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "identifier is not a string");

    const char *name = JS_ToCString(ctx, argv[0]);
    if (name == NULL)
        return JS_UNDEFINED;

    switch(type) {
    case QJS_CB_INIT:
        qjs_callback_list_remove(&qjs->cb_init, name);
        break;
    case QJS_CB_READ:
        plugin_unregister_read(name);
        break;
    case QJS_CB_WRITE:
        plugin_unregister_write(name);
        break;
    case QJS_CB_SHUTDOWN:
        qjs_callback_list_remove(&qjs->cb_shutdown, name);
        break;
    case QJS_CB_CONFIG:
        qjs_callback_list_remove(&qjs->cb_config, name);
        break;
    case QJS_CB_NOTIFICATION:
        plugin_unregister_notification(name);
        break;
    }

    JS_FreeCString(ctx, name);

    return JS_UNDEFINED;
}

static const JSCFunctionListEntry qjs_ncollectd_funcs[] = {
    JS_PROP_INT32_DEF("METRIC_UNKNOWN", METRIC_TYPE_UNKNOWN, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("METRIC_GAUGE", METRIC_TYPE_GAUGE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("METRIC_COUNTER", METRIC_TYPE_COUNTER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("METRIC_STATE_SET", METRIC_TYPE_STATE_SET, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("METRIC_INFO", METRIC_TYPE_INFO, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("METRIC_SUMMARY", METRIC_TYPE_SUMMARY , JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("METRIC_HISTOGRAM", METRIC_TYPE_HISTOGRAM, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("METRIC_GAUGE_HISTOGRAM", METRIC_TYPE_GAUGE_HISTOGRAM, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("NOTIF_FAILURE", NOTIF_FAILURE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("NOTIF_WARNING", NOTIF_WARNING, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("NOTIF_OKAY", NOTIF_WARNING, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("LOG_ERR", LOG_ERR, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("LOG_WARNING", LOG_WARNING, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("LOG_NOTICE", LOG_NOTICE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("LOG_INFO", LOG_INFO, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("LOG_DEBUG", LOG_DEBUG, JS_PROP_CONFIGURABLE),
    JS_CFUNC_MAGIC_DEF("debug", 1, qjs_log, LOG_DEBUG),
    JS_CFUNC_MAGIC_DEF("error", 1, qjs_log, LOG_ERR),
    JS_CFUNC_MAGIC_DEF("info", 1, qjs_log, LOG_INFO),
    JS_CFUNC_MAGIC_DEF("notice", 1, qjs_log, LOG_NOTICE),
    JS_CFUNC_MAGIC_DEF("warning", 1, qjs_log, LOG_WARNING),
    JS_CFUNC_MAGIC_DEF("register_read", 1, qjs_register_generic, QJS_CB_READ),
    JS_CFUNC_MAGIC_DEF("register_write", 1, qjs_register_generic, QJS_CB_WRITE),
    JS_CFUNC_MAGIC_DEF("register_notification", 1, qjs_register_generic, QJS_CB_NOTIFICATION),
    JS_CFUNC_MAGIC_DEF("register_init", 1, qjs_register_generic, QJS_CB_INIT),
    JS_CFUNC_MAGIC_DEF("register_config", 1, qjs_register_generic, QJS_CB_CONFIG),
    JS_CFUNC_MAGIC_DEF("register_shutdown", 1, qjs_register_generic, QJS_CB_SHUTDOWN),
    JS_CFUNC_MAGIC_DEF("unregister_read", 1, qjs_unregister_generic, QJS_CB_READ),
    JS_CFUNC_MAGIC_DEF("unregister_write", 1, qjs_unregister_generic, QJS_CB_WRITE),
    JS_CFUNC_MAGIC_DEF("unregister_notification", 1, qjs_unregister_generic, QJS_CB_NOTIFICATION),
    JS_CFUNC_MAGIC_DEF("unregister_init", 1, qjs_unregister_generic, QJS_CB_INIT),
    JS_CFUNC_MAGIC_DEF("unregister_config", 1, qjs_unregister_generic, QJS_CB_CONFIG),
    JS_CFUNC_MAGIC_DEF("unregister_shutdown", 1, qjs_unregister_generic, QJS_CB_SHUTDOWN),
};

static int qjs_ncollectd_init(JSContext *ctx, JSModuleDef *m)
{
    qjs_notification_init(ctx, m);
    qjs_metric_all_init(ctx, m);
    qjs_metric_family_init(ctx, m);

    JS_SetModuleExportList(ctx, m, qjs_ncollectd_funcs,  countof(qjs_ncollectd_funcs));

    return 0;
}

static JSModuleDef *qjs_init_module_ncollectd(JSContext *ctx, char *name)
{
    JSModuleDef *m = JS_NewCModule(ctx, name, qjs_ncollectd_init);
    if (m == NULL)
        return NULL;

    JS_AddModuleExportList(ctx, m, qjs_ncollectd_funcs, countof(qjs_ncollectd_funcs));
    JS_AddModuleExport(ctx, m, "Notification");
    JS_AddModuleExport(ctx, m, "MetricFamily");
    JS_AddModuleExport(ctx, m, "MetricUnknown");
    JS_AddModuleExport(ctx, m, "MetricGauge");
    JS_AddModuleExport(ctx, m, "MetricCounter");
    JS_AddModuleExport(ctx, m, "MetricInfo");
    JS_AddModuleExport(ctx, m, "MetricStateSet");
    JS_AddModuleExport(ctx, m, "MetricSummary");
    JS_AddModuleExport(ctx, m, "MetricGaugeHistogram");
    JS_AddModuleExport(ctx, m, "MetricHistogram");

    return m;
}

static int qjs_eval_buf(JSContext *ctx, const void *buf, int buf_len, const char *filename,
                        int flags)
{
    JSValue val;
    int ret;

    if ((flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE) {
        /* for the modules, we compile then run to be able to set import.meta */
        val = JS_Eval(ctx, buf, buf_len, filename, flags | JS_EVAL_FLAG_COMPILE_ONLY);
        if (!JS_IsException(val)) {
            js_module_set_import_meta(ctx, val, TRUE, TRUE);
            val = JS_EvalFunction(ctx, val);
        }
        val = js_std_await(ctx, val);
    } else {
        val = JS_Eval(ctx, buf, buf_len, filename, flags);
    }

    if (JS_IsException(val)) {
        qjs_dump_error(ctx);
        ret = -1;
    } else {
        ret = 0;
    }

    JS_FreeValue(ctx, val);
    return ret;
}

static int qjs_eval_file(JSContext *ctx, const char *filename)
{
    char file_path[PATH_MAX];

    if (base_path[0] != '\0') {
        ssnprintf(file_path, sizeof(file_path), "%s/%s", base_path, filename);
    } else {
        ssnprintf(file_path, sizeof(file_path), "%s", filename);
    }

    size_t buf_len = 0;
    uint8_t *buf = js_load_file(ctx, &buf_len, file_path);
    if (buf == NULL) {
        PLUGIN_ERROR("Cannot open '%s': %s.", file_path, STRERRNO);
        return -1;
    }

    bool module = false;
    if (has_suffix(filename, ".mjs") || JS_DetectModule((const char *)buf, buf_len))
        module = true;

    int eval_flags = 0;
    if (module)
        eval_flags = JS_EVAL_TYPE_MODULE;
    else
        eval_flags = JS_EVAL_TYPE_GLOBAL;

    int ret = qjs_eval_buf(ctx, buf, buf_len, filename, eval_flags);
    js_free(ctx, buf);

    return ret;
}

static void qjs_script_free(qjs_script_t *qjs)
{
    if (qjs == NULL)
        return;

    pthread_mutex_lock(&qjs->lock);

    free(qjs->filename);

    for (size_t i = 0; i < qjs->includes_num; i++) {
        free(qjs->includes[i]);
    }
    free(qjs->includes);

    qjs_callback_list_free(qjs->cb_config);
    qjs_callback_list_free(qjs->cb_init);
    qjs_callback_list_free(qjs->cb_shutdown);

    js_std_free_handlers(qjs->rt);
    JS_FreeContext(qjs->ctx);
    JS_FreeRuntime(qjs->rt);

    pthread_mutex_unlock(&qjs->lock);
    pthread_mutex_destroy(&qjs->lock);

    free(qjs);
}

static int qjs_script_init(qjs_script_t *qjs)
{
    qjs->cb_init = NULL;
    qjs->cb_shutdown = NULL;
    qjs->cb_config = NULL;

    qjs->rt = JS_NewRuntime();

    pthread_mutex_init(&qjs->lock, NULL);

    if (qjs->memory_limit != 0)
        JS_SetMemoryLimit(qjs->rt, qjs->memory_limit);
    if (qjs->stack_size != 0)
        JS_SetMaxStackSize(qjs->rt, qjs->stack_size);

    js_std_init_handlers(qjs->rt);

    qjs->ctx = JS_NewContext(qjs->rt);
    if (qjs->ctx == NULL) {
        PLUGIN_ERROR("Cannot allocate JS context.");
        return -1;
    }

    JS_SetContextOpaque(qjs->ctx, qjs);

    js_init_module_std(qjs->ctx, "std");
    js_init_module_os(qjs->ctx, "os");
    js_std_add_helpers(qjs->ctx, -1, NULL);

    if (qjs->load_std) {
        const char *str = "import * as std from 'std';\n"
                          "import * as os from 'os';\n"
                          "globalThis.std = std;\n"
                          "globalThis.os = os;\n";
        qjs_eval_buf(qjs->ctx, str, strlen(str), "<input>", JS_EVAL_TYPE_MODULE);
    }

    {
        qjs_init_module_ncollectd(qjs->ctx, "ncollectd");

        const char *str = "import * as ncollectd from 'ncollectd';\n"
                          "globalThis.ncollectd = ncollectd;\n";
        qjs_eval_buf(qjs->ctx, str, strlen(str), "<input>", JS_EVAL_TYPE_MODULE);
    }

    for(size_t i = 0; i < qjs->includes_num; i++) {
        if (qjs_eval_file(qjs->ctx, qjs->includes[i])) {
            PLUGIN_ERROR("Failed to eval %s.", qjs->includes[i]);
            return -1;
        }
    }

    if (qjs_eval_file(qjs->ctx, qjs->filename)) {
        PLUGIN_ERROR("Failed to eval %s.", qjs->filename);
        return -1;
    }

    return 0;
}

static int qjs_config_script_add_include(qjs_script_t *qjs, config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    char *include= strdup(ci->values[0].value.string);
    if (include == NULL) {
        PLUGIN_ERROR("strdup failed.");
        return -1;
    }

    char **tmp = realloc(qjs->includes, sizeof(char *)*(qjs->includes_num + 1));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        free(include);
        return -1;
    }

    qjs->includes = tmp;
    qjs->includes[qjs->includes_num] = include;
    qjs->includes_num++;

    return 0;
}

static int qjs_config_script(config_item_t *ci)
{
    qjs_script_t *qjs = calloc(1, sizeof(*qjs));
    if (qjs == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    qjs->load_std = true;

    int status = cf_util_get_string(ci, &qjs->filename);
    if (status != 0) {
        PLUGIN_ERROR("Missing filename.");
        free(qjs);
        return status;
    }

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("memory-limit", child->key) == 0) {
            unsigned int num = 0;
            status = cf_util_get_unsigned_int(child, &num);
            qjs->memory_limit = num;
        } else if (strcasecmp("stack-size", child->key) == 0) {
            unsigned int num = 0;
            status = cf_util_get_unsigned_int(child, &num);
            qjs->stack_size = num;
        } else if (strcasecmp("load-std", child->key) == 0) {
            status = cf_util_get_boolean(child, &qjs->load_std);
        } else if (strcasecmp("include", child->key) == 0) {
            status = qjs_config_script_add_include(qjs, child);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        qjs_script_free(qjs);
        return -1;
    }

    qjs->next = qjs_scripts;
    qjs_scripts = qjs;

    return 0;
}

static int qjs_config_plugin(const config_item_t *ci)
{
    if (ci->children_num == 0)
        return 0;

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

static int qjs_config_base_path(const config_item_t *ci)
{
    int status = cf_util_get_string_buffer(ci, base_path, sizeof(base_path));
    if (status != 0)
        return status;

    size_t len = strlen(base_path);
    while ((len > 0) && (base_path[len - 1] == '/')) {
        len--;
        base_path[len] = '\0';
    }

    PLUGIN_DEBUG("base_path = '%s';", base_path);

    return 0;
}

static int qjs_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("base-path", child->key) == 0) {
            status = qjs_config_base_path(child);
        } else if (strcasecmp("load-plugin", child->key) == 0) {
            status = qjs_config_script(child);
        } else if (strcasecmp("plugin", child->key) == 0) {
            status = qjs_config_plugin(child);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0)
        return -1;

    return 0;
}

static int qjs_init(void)
{
    qjs_script_t *qjs = qjs_scripts;
    while (qjs != NULL) {
        int status = qjs_script_init(qjs);
        if (status != 0)
            return -1;

        qjs_callback_t *qjc = qjs->cb_config;
        while (qjc != NULL) {
            for (int i = 0; i < config_block->children_num; i++) {
                config_item_t *child = config_block->children + i;
                if (qjc->name != NULL) {
                    if (strcmp(qjc->name, child->key) == 0) {
                        qjs_cb_config(qjc, child);
                        break;
                    }
                }
            }
            qjc = qjc->next;
        }

        qjc = qjs->cb_init;
        while (qjc != NULL) {
            qjs_cb_call(qjc);
            qjc = qjc->next;
        }

        qjs = qjs->next;
    }

    return 0;
}

static int qjs_shutdown(void)
{
    qjs_script_t *qjs = qjs_scripts;
    while (qjs != NULL) {
        qjs_script_t *next = qjs->next;
        qjs_callback_t *qjc = qjs->cb_shutdown;
        while (qjc != NULL) {
            qjs_cb_call(qjc);
            qjc = qjc->next;
        }
        qjs_script_free(qjs);
        qjs = next;
    }

    qjs_scripts = NULL;

    if (config_block != NULL)
        config_free(config_block);

    return 0;
}

void module_register(void)
{
    plugin_register_config("javascript", qjs_config);
    plugin_register_init("javascript", qjs_init);
    plugin_register_shutdown("javascript", qjs_shutdown);
}
