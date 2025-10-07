// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"
#include "libmetric/metric.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wpedantic"
#if __GNUC__ >= 8
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

#undef func_data

#include "libquickjs/cutils.h"
#include "libquickjs/quickjs-libc.h"

#include "jmetric.h"

static JSClassID qjs_metric_family_class_id;

enum {
    FAM_GETSET_NAME,
    FAM_GETSET_HELP,
    FAM_GETSET_UNIT,
    FAM_GETSET_TYPE,
    FAM_GETSET_METRICS
};

static void qjs_metric_family_finalizer(JSRuntime *rt, JSValue val)
{
    metric_family_t *n = JS_GetOpaque(val, qjs_metric_family_class_id);
    metric_family_free(n);
}

static JSValue qjs_metric_family_ctor(JSContext *ctx, JSValueConst new_target, int argc,
                                      JSValueConst *argv)
{
    JSValue obj = JS_UNDEFINED;

    metric_family_t *fam = calloc(1, sizeof(*fam));
    if (fam == NULL)
        return JS_EXCEPTION;

    fam->type = METRIC_TYPE_UNKNOWN;

    if (!JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        const char *name = JS_ToCString(ctx, argv[0]);
        if (name == NULL)
            goto fail;
        fam->name = strdup(name);
        JS_FreeCString(ctx, name);
    }

    if (!JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        int32_t type = 0;
        if (JS_ToInt32(ctx, &type, argv[1]))
            goto fail;
        if ((type != METRIC_TYPE_UNKNOWN) && (type != METRIC_TYPE_GAUGE) &&
            (type != METRIC_TYPE_COUNTER) && (type != METRIC_TYPE_STATE_SET) &&
            (type != METRIC_TYPE_INFO) && (type != METRIC_TYPE_SUMMARY) &&
            (type != METRIC_TYPE_HISTOGRAM) && (type != METRIC_TYPE_GAUGE_HISTOGRAM))
            goto fail;
        fam->type = type;
    }

    if (!JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2])) {
        const char *help = JS_ToCString(ctx, argv[2]);
        if (help == NULL)
            goto fail;
        fam->help = strdup(help);
        JS_FreeCString(ctx, help);
    }

    if (!JS_IsUndefined(argv[3]) && !JS_IsNull(argv[3])) {
        const char *unit = JS_ToCString(ctx, argv[3]);
        if (unit == NULL)
            goto fail;
        fam->unit = strdup(unit);
        JS_FreeCString(ctx, unit);
    }

    /* using new_target to get the prototype is necessary when the class is extended. */
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, qjs_metric_family_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;
    JS_SetOpaque(obj, fam);
    return obj;

 fail:
    metric_family_free(fam);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue qjs_metric_family_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    metric_family_t *fam = JS_GetOpaque2(ctx, this_val, qjs_metric_family_class_id);
    if (fam == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case FAM_GETSET_NAME:
        return JS_NewString(ctx, fam->name);
        break;
    case FAM_GETSET_HELP:
        return JS_NewString(ctx, fam->help);
        break;
    case FAM_GETSET_UNIT:
        return JS_NewString(ctx, fam->unit);
        break;
    case FAM_GETSET_TYPE:
        return JS_NewInt32(ctx, fam->type);
        break;
    case FAM_GETSET_METRICS: {
        JSValue jmetrics = JS_NewArray(ctx);
        if (JS_IsException(jmetrics))
            return jmetrics;
        for (uint32_t i = 0; i < fam->metric.num ; i++) {
            JSValue jmetric = qjs_metric_new(ctx, &fam->metric.ptr[i], fam->type);
            JS_DefinePropertyValueUint32(ctx, jmetrics, i, jmetric, JS_PROP_C_W_E);
        }
        return jmetrics;
    }   break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_metric_family_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    metric_family_t *fam = JS_GetOpaque2(ctx, this_val, qjs_metric_family_class_id);
    if (fam == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case FAM_GETSET_NAME: {
        const char *name = JS_ToCString(ctx, val);
        if (name == NULL)
            return JS_EXCEPTION;
        fam->name = strdup(name);
        JS_FreeCString(ctx, name);
    }   break;
    case FAM_GETSET_HELP: {
        const char *help = JS_ToCString(ctx, val);
        if (help == NULL)
            return JS_EXCEPTION;
        fam->help = strdup(help);
        JS_FreeCString(ctx, help);
    }   break;
    case FAM_GETSET_UNIT: {
        const char *unit = JS_ToCString(ctx, val);
        if (unit == NULL)
            return JS_EXCEPTION;
        fam->unit = strdup(unit);
        JS_FreeCString(ctx, unit);
    }   break;
    case FAM_GETSET_TYPE:
        return JS_EXCEPTION;
        break;
    case FAM_GETSET_METRICS:
        return JS_EXCEPTION;
        break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_metric_family_add_metric(JSContext *ctx, JSValueConst this_val,
                                            int argc, JSValueConst *argv)
{
    metric_family_t *fam = JS_GetOpaque2(ctx, this_val, qjs_metric_family_class_id);
    if (fam == NULL)
        return JS_EXCEPTION;

    if (JS_IsUndefined(argv[0]))
        return JS_UNDEFINED;

    if (JS_IsNull(argv[0]))
        return JS_UNDEFINED;

    metric_type_t type = qjs_metric_get_metric_type(ctx, argv[0]);
    if (type != fam->type)
        return JS_EXCEPTION;

    metric_t *m = qjs_metric_get_metric(ctx, argv[0]);
    if (m == NULL)
        return JS_EXCEPTION;

    metric_list_add(&fam->metric, *m, fam->type);

    return JS_UNDEFINED;
}

static JSValue qjs_metric_family_dispatch(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    metric_family_t *fam = JS_GetOpaque2(ctx, this_val, qjs_metric_family_class_id);
    if (fam == NULL)
        return JS_EXCEPTION;

    plugin_dispatch_metric_family(fam, 0);

    return JS_UNDEFINED;
}

static JSValue qjs_metric_family_to_string(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    metric_family_t *fam = JS_GetOpaque2(ctx, this_val, qjs_metric_family_class_id);
    if (fam == NULL)
        return JS_EXCEPTION;

    strbuf_t buf = STRBUF_CREATE;

    int status = strbuf_putstr(&buf, "{ name: \"");
    status |= strbuf_putescape_json(&buf, fam->name);
    if (fam->help != NULL) {
        status |= strbuf_putstr(&buf, ", help: \"");
        status |= strbuf_putescape_json(&buf, fam->help);
    }
    if (fam->unit != NULL) {
        status |= strbuf_putstr(&buf, ", unit: \"");
        status |= strbuf_putescape_json(&buf, fam->unit);
    }
    status |= strbuf_putstr(&buf, "\", type: ");
    switch(fam->type) {
    case METRIC_TYPE_UNKNOWN:
        status |= strbuf_putstr(&buf, "MetricFamily.UNKNOWN");
        break;
    case METRIC_TYPE_GAUGE:
        status |= strbuf_putstr(&buf, "MetricFamily.GAUGE");
        break;
    case METRIC_TYPE_COUNTER:
        status |= strbuf_putstr(&buf, "MetricFamily.COUNTER");
        break;
    case METRIC_TYPE_STATE_SET:
        status |= strbuf_putstr(&buf, "MetricFamily.STATE_SET");
        break;
    case METRIC_TYPE_INFO:
        status |= strbuf_putstr(&buf, "MetricFamily.INFO");
        break;
    case METRIC_TYPE_SUMMARY:
        status |= strbuf_putstr(&buf, "MetricFamily.SUMMARY");
        break;
    case METRIC_TYPE_HISTOGRAM:
        status |= strbuf_putstr(&buf, "MetricFamily.HISTOGRAM");
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        status |= strbuf_putstr(&buf, "MetricFamily.GAUGE_HISTOGRAM");
        break;
    }
    status |= strbuf_putstr(&buf, ", metrics: [ ");
    for (size_t i = 0; i < fam->metric.num; i++) {
        if (i != 0)
            status |= strbuf_putchar(&buf, ',');
        status |= strbuf_putchar(&buf, ' ');
        status |= metric_to_string(&buf, &fam->metric.ptr[i], fam->type);
    }
    status |= strbuf_putstr(&buf, " ] }");

    if (status != 0) {
        strbuf_destroy(&buf);
        return JS_EXCEPTION;
    }

    JSValue string = JS_NewStringLen(ctx, buf.ptr, strbuf_len(&buf));

    strbuf_destroy(&buf);

    return string;
}

static JSClassDef qjs_metric_family_class = {
    "MetricFamily",
    .finalizer = qjs_metric_family_finalizer,
};

static const JSCFunctionListEntry qjs_metric_family_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("name", qjs_metric_family_get, qjs_metric_family_set, FAM_GETSET_NAME),
    JS_CGETSET_MAGIC_DEF("help", qjs_metric_family_get, qjs_metric_family_set, FAM_GETSET_HELP),
    JS_CGETSET_MAGIC_DEF("unit", qjs_metric_family_get, qjs_metric_family_set, FAM_GETSET_UNIT),
    JS_CGETSET_MAGIC_DEF("type", qjs_metric_family_get, NULL, FAM_GETSET_TYPE),
    JS_CGETSET_MAGIC_DEF("metrics", qjs_metric_family_get, NULL, FAM_GETSET_METRICS),
    JS_CFUNC_DEF("add_metric", 1, qjs_metric_family_add_metric),
    JS_CFUNC_DEF("dispatch", 1, qjs_metric_family_dispatch),
    JS_CFUNC_DEF("toString", 0, qjs_metric_family_to_string),
};

int qjs_metric_family_init(JSContext *ctx, JSModuleDef *m)
{
    /* create the Point class */
    JS_NewClassID(&qjs_metric_family_class_id);
    JS_NewClass(JS_GetRuntime(ctx), qjs_metric_family_class_id, &qjs_metric_family_class);

    JSValue metric_family_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, metric_family_proto, qjs_metric_family_proto_funcs,
                                    countof(qjs_metric_family_proto_funcs));

    JSValue metric_family_class = JS_NewCFunction2(ctx, qjs_metric_family_ctor,
                                                   "MetricFamily", 4,
                                                   JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, metric_family_class, metric_family_proto);
    JS_SetClassProto(ctx, qjs_metric_family_class_id, metric_family_proto);

    JS_SetModuleExport(ctx, m, "MetricFamily", metric_family_class);

    return 0;
}

JSValue qjs_metric_family_new(JSContext *ctx, const metric_family_t *fam)
{
    JSValue obj = JS_NewObjectClass(ctx, qjs_metric_family_class_id);
    if (JS_IsException(obj))
        return obj;

    metric_family_t *famdup = metric_family_clone(fam);
    if (famdup == NULL) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }

    JS_SetOpaque(obj, famdup);

    return obj;
}
