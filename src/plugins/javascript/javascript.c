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

typedef struct {
    char *instance;
    char *filename;
    JSRuntime *rt;
    JSContext *ctx;
    size_t memory_limit;
    size_t stack_size;
    bool load_std;
    char **includes;
    size_t includes_num;
    cdtime_t interval;

    JSValue cb_init;
    JSValue cb_shutdown;
    JSValue cb_config;

    config_item_t *config;
    JSValue jconfig;

    pthread_mutex_t lock;
} qjs_script_t;

 typedef struct {
    qjs_script_t *qjs;
    JSValue cb;
} qjs_callback_t;

static qjs_script_t **qjs_scripts;
static size_t qjs_scripts_size;

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

static void qjs_plugin_name(bool full, strbuf_t *buf, const char *name, JSContext *ctx,
                            JSValue func)
{
    if (full)
        strbuf_putstr(buf, "javascript");

    if (name != NULL) {
        if (strbuf_len(buf) > 0)
            strbuf_putchar(buf, '/');
        strbuf_putstr(buf, name);
    }

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

static void qjs_call(JSContext *ctx, JSValueConst func)
{
    JSValue func_dup = JS_DupValue(ctx, func);
    JSValue ret = JS_Call(ctx, func_dup, JS_UNDEFINED, 0, NULL);
    JS_FreeValue(ctx, func_dup);
    if (JS_IsException(ret)) {
        qjs_dump_error(ctx);
    }
    JS_FreeValue(ctx, ret);
}

static void qjs_call1(JSContext *ctx, JSValueConst func, JSValue value)
{
    JSValue func_dup = JS_DupValue(ctx, func);
    JSValue ret = JS_Call(ctx, func_dup, JS_UNDEFINED, 1, (JSValueConst *)&value);
    JS_FreeValue(ctx, func_dup);
    if (JS_IsException(ret))
        qjs_dump_error(ctx);
    JS_FreeValue(ctx, ret);
}

static void qjc_callback_free(void *data)
{
    if (data == NULL)
        return;

    qjs_callback_t *qjc = data;

    JS_FreeValue(qjc->qjs->ctx, qjc->cb);

    free(qjc);
}

static int qjs_read(user_data_t *user_data)
{
    if (user_data == NULL)
        return -1;

    qjs_callback_t *qjc = user_data->data;

    pthread_mutex_lock(&qjc->qjs->lock);

    JS_UpdateStackTop(qjc->qjs->rt);

    qjs_call(qjc->qjs->ctx, qjc->cb);

    js_std_loop(qjc->qjs->ctx);

    pthread_mutex_unlock(&qjc->qjs->lock);

    return 0;
}

static int qjs_write(metric_family_t const *fam, user_data_t *user_data)
{
    if ((fam == NULL) || (user_data == NULL))
        return -1;

    qjs_callback_t *qjc = user_data->data;

    pthread_mutex_lock(&qjc->qjs->lock);

    JS_UpdateStackTop(qjc->qjs->rt);

//    JSValue jfam = qjs_from_fam(qjc->qjs->ctx, fam);
    JSValue jfam = qjs_metric_family_new(qjc->qjs->ctx, fam);

    qjs_call1(qjc->qjs->ctx, qjc->cb, jfam);

    js_std_loop(qjc->qjs->ctx);

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

    JS_UpdateStackTop(qjc->qjs->rt);

    JSValue jn = qjs_notification_new(qjc->qjs->ctx, n);

    qjs_call1(qjc->qjs->ctx, qjc->cb, jn);

    js_std_loop(qjc->qjs->ctx);

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

static JSValue qjs_register_read(JSContext *ctx, JSValueConst this_val, int argc,
                                 JSValueConst *argv)
{
    qjs_callback_t *qjc = calloc(1, sizeof(*qjc));
    if (qjc == NULL)
        return JS_UNDEFINED;

    qjs_script_t *qjs = JS_GetContextOpaque(ctx);
    if (qjs == NULL) {
        free(qjc);
        return JS_UNDEFINED;
    }
    JSValueConst func = argv[0];
    if (JS_IsNull(func)) {
        free(qjc);
        return JS_ThrowTypeError(ctx, "is null");
    }
    if (!JS_IsFunction(ctx, func)) {
        free(qjc);
        return JS_ThrowTypeError(ctx, "not a function");
    }

    JSValue dup_func = JS_DupValue(ctx, func);
    if (JS_IsNull(dup_func)) {
        free(qjc);

    }

    qjc->qjs = qjs;
    qjc->cb = dup_func;

    char buffer[1024];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);
    qjs_plugin_name(false, &buf, qjs->instance, ctx, dup_func);

    plugin_register_complex_read("javascript", buf.ptr, qjs_read, 0,
                                  &(user_data_t){.data = qjc, .free_func = qjc_callback_free});

    return JS_UNDEFINED;
}

static JSValue qjs_unregister_read(JSContext *ctx, JSValueConst this_val, int argc,
                                   JSValueConst *argv)
{
    qjs_script_t *qjs = JS_GetContextOpaque(ctx);
    if (qjs == NULL) {
        return JS_UNDEFINED;
    }

    JSValueConst func = argv[0];
    if (JS_IsNull(func)) {
        return JS_ThrowTypeError(ctx, "is null");
    }
    if (!JS_IsFunction(ctx, func)) {
        return JS_ThrowTypeError(ctx, "not a function");
    }

    char buffer[1024];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);
    qjs_plugin_name(true, &buf, qjs->instance, ctx, func);

    plugin_unregister_read(buf.ptr);

    return JS_UNDEFINED;
}

static JSValue qjs_register_init(JSContext *ctx, JSValueConst this_val, int argc,
                                 JSValueConst *argv)
{
    qjs_script_t *qjs = JS_GetContextOpaque(ctx);
    if (qjs == NULL)
        return JS_UNDEFINED;

    if (!JS_IsNull(qjs->cb_init)) {
        JS_FreeValue(qjs->ctx, qjs->cb_init);
        qjs->cb_init = JS_NULL;
    }

    JSValueConst func = argv[0];
    if (JS_IsNull(func))
        return JS_ThrowTypeError(ctx, "is null");
    if (!JS_IsFunction(ctx, func))
        return JS_ThrowTypeError(ctx, "not a function");

    JSValue dup_func = JS_DupValue(ctx, func);
    if (JS_IsNull(dup_func)) {

    }

    qjs->cb_init = dup_func;


    return JS_UNDEFINED;
}

static JSValue qjs_unregister_init(JSContext *ctx, JSValueConst this_val, int argc,
                                   JSValueConst *argv)
{
    qjs_script_t *qjs = JS_GetContextOpaque(ctx);
    if (qjs == NULL)
        return JS_UNDEFINED;
    
    if (!JS_IsNull(qjs->cb_init)) {
        JS_FreeValue(qjs->ctx, qjs->cb_init);
        qjs->cb_init = JS_NULL;
    }
    
    return JS_UNDEFINED;
}

static JSValue qjs_register_write(JSContext *ctx, JSValueConst this_val, int argc,
                                  JSValueConst *argv)
{
    qjs_callback_t *qjc = calloc(1, sizeof(*qjc));
    if (qjc == NULL)
        return JS_UNDEFINED;

    qjs_script_t *qjs = JS_GetContextOpaque(ctx);
    if (qjs == NULL) {
        free(qjc);
        return JS_UNDEFINED;
    }

    JSValueConst func = argv[0];
    if (JS_IsNull(func)) {
        free(qjc);
        return JS_ThrowTypeError(ctx, "not a function");
    }
    if (!JS_IsFunction(ctx, func)) {
        free(qjc);
        return JS_ThrowTypeError(ctx, "not a function");
    }

    JSValue dup_func = JS_DupValue(ctx, func);
    if (JS_IsNull(dup_func)) {
        free(qjc);

    }

    qjc->qjs = qjs;
    qjc->cb = dup_func;

    char buffer[1024];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);
    qjs_plugin_name(false, &buf, qjs->instance, ctx, dup_func);

    user_data_t user_data = { .data = qjc, .free_func = qjc_callback_free };
    plugin_register_write("javascript", buf.ptr, qjs_write, NULL, 0, 0, &user_data);

    return JS_UNDEFINED;
}

static JSValue qjs_unregister_write(JSContext *ctx, JSValueConst this_val, int argc,
                                    JSValueConst *argv)
{
    qjs_script_t *qjs = JS_GetContextOpaque(ctx);
    if (qjs == NULL) {
        return JS_UNDEFINED;
    }

    JSValueConst func = argv[0];
    if (JS_IsNull(func)) {
        return JS_ThrowTypeError(ctx, "not a function");
    }
    if (!JS_IsFunction(ctx, func)) {
        return JS_ThrowTypeError(ctx, "not a function");
    }

    char buffer[1024];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);
    qjs_plugin_name(true, &buf, qjs->instance, ctx, func);

    plugin_unregister_write(buf.ptr);

    return JS_UNDEFINED;
}

static JSValue qjs_register_config(JSContext *ctx, JSValueConst this_val, int argc,
                                   JSValueConst *argv)
{
    qjs_script_t *qjs = JS_GetContextOpaque(ctx);
    if (qjs == NULL)
        return JS_UNDEFINED;

    if (!JS_IsNull(qjs->cb_config)) {
        JS_FreeValue(qjs->ctx, qjs->cb_config);
         qjs->cb_config = JS_NULL;
    }

    JSValueConst func = argv[0];
    if (JS_IsNull(func))
        return JS_ThrowTypeError(ctx, "is null");
    if (!JS_IsFunction(ctx, func))
        return JS_ThrowTypeError(ctx, "not a function");

    JSValue dup_func = JS_DupValue(ctx, func);
    if (JS_IsNull(dup_func)) {

    }

    qjs->cb_config = dup_func;

    return JS_UNDEFINED;
}

static JSValue qjs_unregister_config(JSContext *ctx, JSValueConst this_val, int argc,
                                     JSValueConst *argv)
{
    qjs_script_t *qjs = JS_GetContextOpaque(ctx);
    if (qjs == NULL)
        return JS_UNDEFINED;

    if (!JS_IsNull(qjs->cb_config)) {
        JS_FreeValue(qjs->ctx, qjs->cb_config);
         qjs->cb_config = JS_NULL;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_register_shutdown(JSContext *ctx, JSValueConst this_val, int argc,
                                     JSValueConst *argv)
{
    qjs_script_t *qjs = JS_GetContextOpaque(ctx);
    if (qjs == NULL)
        return JS_UNDEFINED;

    if (!JS_IsNull(qjs->cb_shutdown)) {
        JS_FreeValue(qjs->ctx, qjs->cb_shutdown);
         qjs->cb_shutdown = JS_NULL;
    }

    JSValueConst func = argv[0];
    if (JS_IsNull(func))
        return JS_ThrowTypeError(ctx, "is null");
    if (!JS_IsFunction(ctx, func))
        return JS_ThrowTypeError(ctx, "not a function");

    JSValue dup_func = JS_DupValue(ctx, func);
    if (JS_IsNull(dup_func)) {

    }

    qjs->cb_shutdown = dup_func;

    return JS_UNDEFINED;
}

static JSValue qjs_unregister_shutdown(JSContext *ctx, JSValueConst this_val, int argc,
                                       JSValueConst *argv)
{
    qjs_script_t *qjs = JS_GetContextOpaque(ctx);
    if (qjs == NULL)
        return JS_UNDEFINED;

    if (!JS_IsNull(qjs->cb_shutdown)) {
        JS_FreeValue(qjs->ctx, qjs->cb_shutdown);
         qjs->cb_shutdown = JS_NULL;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_register_notification(JSContext *ctx, JSValueConst this_val, int argc,
                                         JSValueConst *argv)
{
    qjs_callback_t *qjc = calloc(1, sizeof(*qjc));
    if (qjc == NULL)
        return JS_UNDEFINED;

    qjs_script_t *qjs = JS_GetContextOpaque(ctx);
    if (qjs == NULL) {
        free(qjc);
        return JS_UNDEFINED;
    }

    JSValueConst func = argv[0];
    if (JS_IsNull(func)) {
        free(qjc);
        return JS_ThrowTypeError(ctx, "not a function");
    }
    if (!JS_IsFunction(ctx, func)) {
        free(qjc);
        return JS_ThrowTypeError(ctx, "not a function");
    }

    JSValue dup_func = JS_DupValue(ctx, func);
    if (JS_IsNull(dup_func)) {
        free(qjc);

    }

    qjc->qjs = qjs;
    qjc->cb = dup_func;

    char buffer[1024];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);
    qjs_plugin_name(false, &buf, qjs->instance, ctx, dup_func);

    plugin_register_notification("javascript", buf.ptr, qjs_notification,
                                  &(user_data_t){.data = qjc, .free_func = qjc_callback_free});

    return JS_UNDEFINED;
}

static JSValue qjs_unregister_notification(JSContext *ctx, JSValueConst this_val, int argc,
                                           JSValueConst *argv)
{
    qjs_script_t *qjs = JS_GetContextOpaque(ctx);
    if (qjs == NULL) {
        return JS_UNDEFINED;
    }

    JSValueConst func = argv[0];
    if (JS_IsNull(func)) {
        return JS_ThrowTypeError(ctx, "not a function");
    }
    if (!JS_IsFunction(ctx, func)) {
        return JS_ThrowTypeError(ctx, "not a function");
    }

    char buffer[1024];
    strbuf_t buf = STRBUF_CREATE_STATIC(buffer);
    qjs_plugin_name(true, &buf, qjs->instance, ctx, func);

    plugin_unregister_notification(buf.ptr);

    return JS_UNDEFINED;
}

static const JSCFunctionListEntry qjs_ncollectd_funcs[] = {
    JS_PROP_INT32_DEF("UNKNOWN", METRIC_TYPE_UNKNOWN, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("GAUGE", METRIC_TYPE_GAUGE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("COUNTER", METRIC_TYPE_COUNTER, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("STATE_SET", METRIC_TYPE_STATE_SET, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("INFO", METRIC_TYPE_INFO, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("SUMMARY", METRIC_TYPE_SUMMARY , JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("HISTOGRAM", METRIC_TYPE_HISTOGRAM, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("GAUGE_HISTOGRAM", METRIC_TYPE_GAUGE_HISTOGRAM, JS_PROP_CONFIGURABLE),
    JS_CFUNC_MAGIC_DEF("debug", 1, qjs_log, LOG_DEBUG),
    JS_CFUNC_MAGIC_DEF("error", 1, qjs_log, LOG_ERR),
    JS_CFUNC_MAGIC_DEF("info", 1, qjs_log, LOG_INFO),
    JS_CFUNC_MAGIC_DEF("notice", 1, qjs_log, LOG_NOTICE),
    JS_CFUNC_MAGIC_DEF("warning", 1, qjs_log, LOG_WARNING),
    JS_CFUNC_DEF("register_read", 1, qjs_register_read),
    JS_CFUNC_DEF("register_init", 1, qjs_register_init),
    JS_CFUNC_DEF("register_write", 1, qjs_register_write),
    JS_CFUNC_DEF("register_config", 1, qjs_register_config),
    JS_CFUNC_DEF("register_shutdown", 1, qjs_register_shutdown),
    JS_CFUNC_DEF("register_notification", 1, qjs_register_notification),
    JS_CFUNC_DEF("unregister_read", 1, qjs_unregister_read),
    JS_CFUNC_DEF("unregister_init", 1, qjs_unregister_init),
    JS_CFUNC_DEF("unregister_write", 1, qjs_unregister_write),
    JS_CFUNC_DEF("unregister_config", 1, qjs_unregister_config),
    JS_CFUNC_DEF("unregister_shutdown", 1, qjs_unregister_shutdown),
    JS_CFUNC_DEF("unregister_notification", 1, qjs_unregister_notification),
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
    size_t buf_len = 0;
    uint8_t *buf = js_load_file(ctx, &buf_len, filename);
    if (buf == NULL) {
        PLUGIN_ERROR("Cannot open '%s': %s.", filename, STRERRNO);
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

    free(qjs->instance);
    free(qjs->filename);

    for (size_t i = 0; i < qjs->includes_num; i++) {
        free(qjs->includes[i]);
    }
    free(qjs->includes);

    if (qjs->config != NULL)
        config_free(qjs->config);
    if (!JS_IsNull(qjs->jconfig))
        JS_FreeValue(qjs->ctx, qjs->jconfig);

    if (!JS_IsNull(qjs->cb_config))
        JS_FreeValue(qjs->ctx, qjs->cb_config);
    if (!JS_IsNull(qjs->cb_init))
        JS_FreeValue(qjs->ctx, qjs->cb_init);
    if (!JS_IsNull(qjs->cb_shutdown))
        JS_FreeValue(qjs->ctx, qjs->cb_shutdown);

    js_std_free_handlers(qjs->rt);
    JS_FreeContext(qjs->ctx);
    JS_FreeRuntime(qjs->rt);

    pthread_mutex_unlock(&qjs->lock);
    pthread_mutex_destroy(&qjs->lock);

    free(qjs);
}

static int qjs_script_init(qjs_script_t *qjs)
{
    qjs->cb_init = JS_NULL;
    qjs->cb_shutdown = JS_NULL;
    qjs->cb_config = JS_NULL;
    qjs->jconfig = JS_NULL;

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

    qjs->jconfig = qjs_from_config(qjs->ctx, qjs->config);
    config_free(qjs->config);
    qjs->config = NULL;

    if (JS_IsException(qjs->jconfig)) {
        PLUGIN_ERROR("Failed to conviert configuration");
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

    qjs->includes[qjs->includes_num] = include;
    qjs->includes_num++;

    return 0;
}

static int qjs_config_instance(config_item_t *ci)
{
    qjs_script_t *qjs = calloc(1, sizeof(*qjs));
    if (qjs == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    qjs->load_std = true;

    int status = cf_util_get_string(ci, &qjs->instance);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(qjs);
        return status;
    }
    assert(qjs->instance != NULL);

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
        } else if (strcasecmp("script", child->key) == 0) {
            status = cf_util_get_string(child, &qjs->filename);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &qjs->interval);
        } else if (strcasecmp("config", child->key) == 0) {
            qjs->config = config_clone(child);
            if (qjs->config == NULL)
                status = -1;
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

    if (qjs->filename == NULL) {
        PLUGIN_ERROR("Missing script filename.");
        qjs_script_free(qjs);
        return -1;
    }

    qjs_script_t **tmp = realloc(qjs_scripts, sizeof(qjs_script_t *) * (qjs_scripts_size + 1));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        qjs_script_free(qjs);
        return -1;
    }
    qjs_scripts = tmp;
    qjs_scripts[qjs_scripts_size] = qjs;
    qjs_scripts_size++;

    return 0;
}

static int qjs_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = qjs_config_instance(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int qjs_init(void)
{
    for (size_t i = 0; i < qjs_scripts_size; i++) {
        qjs_script_t *qjs = qjs_scripts[i];
        int status = qjs_script_init(qjs);
        if (status != 0)
            return -1;
        if (!JS_IsNull(qjs->cb_config))
            qjs_call1(qjs->ctx, qjs->cb_config, qjs->jconfig);
        if (!JS_IsNull(qjs->cb_init))
            qjs_call(qjs->ctx, qjs->cb_init);
    }

    return 0;
}

static int qjs_shutdown(void)
{
    for (size_t i = 0; i < qjs_scripts_size; i++) {
        qjs_script_t *qjs = qjs_scripts[i];
        if (!JS_IsNull(qjs->cb_shutdown))
            qjs_call(qjs->ctx, qjs->cb_shutdown);
        qjs_script_free(qjs);
    }

    free(qjs_scripts);
    qjs_scripts = NULL;

    return 0;
}

void module_register(void)
{
    plugin_register_config("javascript", qjs_config);
    plugin_register_init("javascript", qjs_init);
    plugin_register_shutdown("javascript", qjs_shutdown);
}
