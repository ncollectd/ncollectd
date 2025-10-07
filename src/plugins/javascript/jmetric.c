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

#include "jutil.h"

static JSClassID qjs_metric_unknown_class_id;
static JSClassID qjs_metric_gauge_class_id;
static JSClassID qjs_metric_counter_class_id;
static JSClassID qjs_metric_info_class_id;
static JSClassID qjs_metric_state_set_class_id;
static JSClassID qjs_metric_summary_class_id;
static JSClassID qjs_metric_gauge_histogram_class_id;
static JSClassID qjs_metric_histogram_class_id;

enum {
    METRIC_GETSET_TIME,
    METRIC_GETSET_INTERVAL,
    METRIC_GETSET_LABELS,
    METRIC_GETSET_VALUE,
    METRIC_GETSET_TYPE,
    METRIC_GETSET_SUM,
    METRIC_GETSET_COUNT,
    METRIC_GETSET_QUANTILES,
    METRIC_GETSET_BUCKETS
};

static int state_set_to_string(strbuf_t *buf, state_set_t *set)
{
    int status = strbuf_putchar(buf, '{');

    for (size_t i = 0; i < set->num; i++) {
        state_t *state = &set->ptr[i];
        if (i != 0)
            status |= strbuf_putchar(buf, ',');
        status |= strbuf_putchar(buf, ' ');
        status |= strbuf_putstr(buf, state->name);
        status |= strbuf_putstr(buf, ": ");
        status |= strbuf_putstr(buf, state->enabled ? "true" : "false");
    }

    status |= strbuf_putchar(buf, '}');

    return status;
}

int metric_to_string(strbuf_t *buf, metric_t *m, metric_type_t type)
{
    int status = strbuf_putstr(buf, "{ time: ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->time));
    status |= strbuf_putstr(buf, ", interval: ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->interval));
    status |= strbuf_putstr(buf, ", labels: ");
    status |= label_set_to_string(buf, &m->label);
    status |= strbuf_putstr(buf, ", value: ");

    switch(type) {
    case METRIC_TYPE_UNKNOWN:
        if (m->value.unknown.type == UNKNOWN_FLOAT64) {
            status |= strbuf_putdouble(buf, m->value.unknown.float64);
        } else {
            status |= strbuf_putint(buf, m->value.unknown.int64);
        }
        break;
    case METRIC_TYPE_GAUGE:
        if (m->value.gauge.type == GAUGE_FLOAT64) {
            status |= strbuf_putdouble(buf, m->value.gauge.float64);
        } else {
            status |= strbuf_putint(buf, m->value.gauge.int64);
        }
        break;
    case METRIC_TYPE_COUNTER:
        if (m->value.counter.type == COUNTER_UINT64) {
            status |= strbuf_putuint(buf, m->value.counter.uint64);
        } else {
            status |= strbuf_putdouble(buf, m->value.counter.float64);
        }
        break;
    case METRIC_TYPE_STATE_SET:
        status |= state_set_to_string(buf, &m->value.state_set);
        break;
    case METRIC_TYPE_INFO:
        status |= label_set_to_string(buf, &m->value.info);
        break;
    case METRIC_TYPE_SUMMARY:
        break;
    case METRIC_TYPE_HISTOGRAM:
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        break;
    }
    return status | strbuf_putstr(buf, " }");
}

static void qjs_metric_unknown_finalizer(JSRuntime *rt, JSValue val)
{
    metric_t *m = JS_GetOpaque(val, qjs_metric_unknown_class_id);
    if (m != NULL) {
        metric_reset(m, METRIC_TYPE_UNKNOWN);
        free(m);
    }
}

static JSValue qjs_metric_unknown_ctor(JSContext *ctx, JSValueConst new_target, int argc,
                                       JSValueConst *argv)
{
    JSValue obj = JS_UNDEFINED;

    metric_t *m = calloc(1, sizeof(*m));
    if (m == NULL)
        return JS_EXCEPTION;

    // "value", "labels", "time", "interval"

    if (!JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        if (JS_IsNumber(argv[0])) {
            double value = 0;
            if (JS_ToFloat64(ctx, &value, argv[0]))
                return JS_EXCEPTION;
            m->value = VALUE_UNKNOWN_FLOAT64(value);
        } else if (JS_IsBigInt(ctx, argv[0])) {
            int64_t value = 0;
            if (JS_ToBigInt64(ctx, &value, argv[0]))
                return JS_EXCEPTION;
            m->value = VALUE_UNKNOWN_INT64(value);
        } else {
            return JS_EXCEPTION;
        }
    }

    if (!JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        JSValue ret = qjs_to_label_set(ctx, argv[1], &m->label);
        if (!JS_IsUndefined(ret))
            return JS_EXCEPTION;
    }

    if (!JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2])) {
        double mtime = 0;
        if (JS_ToFloat64(ctx, &mtime, argv[2]))
            return JS_EXCEPTION;
        m->time = DOUBLE_TO_CDTIME_T(mtime);
    }

    if (!JS_IsUndefined(argv[3]) && !JS_IsNull(argv[3])) {
        double minterval = 0;
        if (JS_ToFloat64(ctx, &minterval, argv[3]))
            return JS_EXCEPTION;
        m->interval = DOUBLE_TO_CDTIME_T(minterval);
    }

    /* using new_target to get the prototype is necessary when the class is extended. */
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, qjs_metric_unknown_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;

    JS_SetOpaque(obj, m);

    return obj;

 fail:
    metric_reset(m, METRIC_TYPE_UNKNOWN);
    free(m);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue qjs_metric_unknown_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_unknown_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case METRIC_GETSET_TIME:
        return JS_NewFloat64(ctx, CDTIME_T_TO_DOUBLE(m->time));
        break;
    case METRIC_GETSET_INTERVAL:
        return JS_NewFloat64(ctx, CDTIME_T_TO_DOUBLE(m->interval));
        break;
    case METRIC_GETSET_LABELS:
        return qjs_from_label_set(ctx, &m->label);
        break;
    case METRIC_GETSET_VALUE:
        if (m->value.unknown.type == UNKNOWN_FLOAT64) {
            return JS_NewFloat64(ctx, m->value.unknown.float64);
        } else {
            return JS_NewBigInt64(ctx, m->value.unknown.int64);
        }
        break;
    case METRIC_GETSET_TYPE:
        return JS_NewInt32(ctx, m->value.unknown.type == UNKNOWN_FLOAT64 ?
                                UNKNOWN_FLOAT64 : UNKNOWN_INT64); 
        break;
    default:
        break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_metric_unknown_set(JSContext *ctx, JSValueConst this_val, JSValue val,
                                                      int magic)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_unknown_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case METRIC_GETSET_TIME: {
       double ntime = 0;
        if (JS_ToFloat64(ctx, &ntime, val))
            return JS_EXCEPTION;
        m->time = DOUBLE_TO_CDTIME_T(ntime);
    }   break;
    case METRIC_GETSET_INTERVAL: {
       double ninterval = 0;
        if (JS_ToFloat64(ctx, &ninterval, val))
            return JS_EXCEPTION;
        m->interval = DOUBLE_TO_CDTIME_T(ninterval);
    }   break;
    case METRIC_GETSET_LABELS:
        label_set_reset(&m->label);
        return qjs_to_label_set(ctx, val, &m->label);
        break;
    case METRIC_GETSET_VALUE:
        if (m->value.unknown.type == UNKNOWN_FLOAT64) {
            double value = 0;
            if (JS_ToFloat64(ctx, &value, val))
                return JS_EXCEPTION;
            m->value = VALUE_UNKNOWN_FLOAT64(value);
        } else {
            int64_t value = 0;
            if (JS_ToBigInt64(ctx, &value, val))
                return JS_EXCEPTION;
            m->value = VALUE_UNKNOWN_INT64(value);
        }
        break;
    default:
        break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_metric_unknown_to_string(JSContext *ctx, JSValueConst this_val,
                                                   int argc, JSValueConst *argv)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_unknown_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    strbuf_t buf = STRBUF_CREATE;

    metric_to_string(&buf, m, METRIC_TYPE_UNKNOWN);

    JSValue string = JS_NewStringLen(ctx, buf.ptr, strbuf_len(&buf));

    strbuf_destroy(&buf);

    return string;
}

static JSClassDef qjs_metric_unknown_class = {
    "MetricUnknown",
    .finalizer = qjs_metric_unknown_finalizer
};

static const JSCFunctionListEntry qjs_metric_unknown_proto_funcs[] = {
    JS_PROP_INT32_DEF("FLOAT64", UNKNOWN_FLOAT64, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("INT64", UNKNOWN_INT64, JS_PROP_CONFIGURABLE),
    JS_CGETSET_MAGIC_DEF("time", qjs_metric_unknown_get, qjs_metric_unknown_set,
                                 METRIC_GETSET_TIME),
    JS_CGETSET_MAGIC_DEF("interval", qjs_metric_unknown_get, qjs_metric_unknown_set,
                                     METRIC_GETSET_INTERVAL),
    JS_CGETSET_MAGIC_DEF("labels", qjs_metric_unknown_get, qjs_metric_unknown_set,
                                   METRIC_GETSET_LABELS),
    JS_CGETSET_MAGIC_DEF("value", qjs_metric_unknown_get, qjs_metric_unknown_set,
                                  METRIC_GETSET_VALUE),
    JS_CGETSET_MAGIC_DEF("type", qjs_metric_unknown_get, NULL,
                                 METRIC_GETSET_TYPE),
    JS_CFUNC_DEF("toString", 0, qjs_metric_unknown_to_string),
};

static int qjs_metric_unknown_init(JSContext *ctx, JSModuleDef *m)
{
    /* create the Point class */
    JS_NewClassID(&qjs_metric_unknown_class_id);
    JS_NewClass(JS_GetRuntime(ctx), qjs_metric_unknown_class_id, &qjs_metric_unknown_class);

    JSValue metric_unknown_double_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, metric_unknown_double_proto, qjs_metric_unknown_proto_funcs,
                                    countof(qjs_metric_unknown_proto_funcs));

    JSValue metric_unknown_double_class = JS_NewCFunction2(ctx, qjs_metric_unknown_ctor,
                                                           "MetricUnknown", 4,
                                                           JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, metric_unknown_double_class, metric_unknown_double_proto);
    JS_SetClassProto(ctx, qjs_metric_unknown_class_id, metric_unknown_double_proto);

    JS_SetModuleExport(ctx, m, "MetricUnknown", metric_unknown_double_class);

    return 0;
}

static void qjs_metric_gauge_finalizer(JSRuntime *rt, JSValue val)
{
    metric_t *m = JS_GetOpaque(val, qjs_metric_gauge_class_id);
    if (m != NULL) {
        metric_reset(m, METRIC_TYPE_GAUGE);
        free(m);
    }
}

static JSValue qjs_metric_gauge_ctor(JSContext *ctx, JSValueConst new_target, int argc,
                                       JSValueConst *argv)
{
    JSValue obj = JS_UNDEFINED;

    metric_t *m = calloc(1, sizeof(*m));
    if (m == NULL)
        return JS_EXCEPTION;

    // "value", "labels", "time", "interval"

    if (!JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        if (JS_IsNumber(argv[0])) {
            double value = 0;
            if (JS_ToFloat64(ctx, &value, argv[0]))
                return JS_EXCEPTION;
            m->value = VALUE_GAUGE_FLOAT64(value);
        } else if (JS_IsBigInt(ctx, argv[0])) {
            int64_t value = 0;
            if (JS_ToBigInt64(ctx, &value, argv[0]))
                return JS_EXCEPTION;
            m->value = VALUE_GAUGE_INT64(value);
        } else {
            return JS_EXCEPTION;
        }
    }

    if (!JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        JSValue ret = qjs_to_label_set(ctx, argv[1], &m->label);
        if (!JS_IsUndefined(ret))
            return JS_EXCEPTION;
    }

    if (!JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2])) {
        double mtime = 0;
        if (JS_ToFloat64(ctx, &mtime, argv[2]))
            return JS_EXCEPTION;
        m->time = DOUBLE_TO_CDTIME_T(mtime);
    }

    if (!JS_IsUndefined(argv[3]) && !JS_IsNull(argv[3])) {
        double minterval = 0;
        if (JS_ToFloat64(ctx, &minterval, argv[3]))
            return JS_EXCEPTION;
        m->interval = DOUBLE_TO_CDTIME_T(minterval);
    }

    /* using new_target to get the prototype is necessary when the class is extended. */
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, qjs_metric_gauge_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;

    JS_SetOpaque(obj, m);

    return obj;

 fail:
    metric_reset(m, METRIC_TYPE_GAUGE);
    free(m);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue qjs_metric_gauge_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_gauge_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case METRIC_GETSET_TIME:
        return JS_NewFloat64(ctx, CDTIME_T_TO_DOUBLE(m->time));
        break;
    case METRIC_GETSET_INTERVAL:
        return JS_NewFloat64(ctx, CDTIME_T_TO_DOUBLE(m->interval));
        break;
    case METRIC_GETSET_LABELS:
        return qjs_from_label_set(ctx, &m->label);
        break;
    case METRIC_GETSET_VALUE:
        if (m->value.gauge.type == GAUGE_FLOAT64) {
            return JS_NewFloat64(ctx, m->value.gauge.float64);
        } else {
            return JS_NewBigInt64(ctx, m->value.gauge.int64);
        }
        break;
    case METRIC_GETSET_TYPE:
        return JS_NewInt32(ctx, m->value.gauge.type == GAUGE_FLOAT64 ?
                                GAUGE_FLOAT64 : GAUGE_INT64); 
        break;
    default:
        break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_metric_gauge_set(JSContext *ctx, JSValueConst this_val, JSValue val,
                                                      int magic)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_gauge_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case METRIC_GETSET_TIME: {
       double ntime = 0;
        if (JS_ToFloat64(ctx, &ntime, val))
            return JS_EXCEPTION;
        m->time = DOUBLE_TO_CDTIME_T(ntime);
    }   break;
    case METRIC_GETSET_INTERVAL: {
       double ninterval = 0;
        if (JS_ToFloat64(ctx, &ninterval, val))
            return JS_EXCEPTION;
        m->interval = DOUBLE_TO_CDTIME_T(ninterval);
    }   break;
    case METRIC_GETSET_LABELS:
        label_set_reset(&m->label);
        return qjs_to_label_set(ctx, val, &m->label);
        break;
    case METRIC_GETSET_VALUE:
        if (m->value.gauge.type == GAUGE_FLOAT64) {
            double value = 0;
            if (JS_ToFloat64(ctx, &value, val))
                return JS_EXCEPTION;
            m->value = VALUE_GAUGE_FLOAT64(value);
        } else {
            int64_t value = 0;
            if (JS_ToBigInt64(ctx, &value, val))
                return JS_EXCEPTION;
            m->value = VALUE_GAUGE_INT64(value);
        }
        break;
    default:
        break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_metric_gauge_to_string(JSContext *ctx, JSValueConst this_val,
                                                   int argc, JSValueConst *argv)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_gauge_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    strbuf_t buf = STRBUF_CREATE;

    metric_to_string(&buf, m, METRIC_TYPE_GAUGE);

    JSValue string = JS_NewStringLen(ctx, buf.ptr, strbuf_len(&buf));

    strbuf_destroy(&buf);

    return string;
}

static JSClassDef qjs_metric_gauge_class = {
    "MetricGauge",
    .finalizer = qjs_metric_gauge_finalizer
};

static const JSCFunctionListEntry qjs_metric_gauge_proto_funcs[] = {
    JS_PROP_INT32_DEF("FLOAT64", GAUGE_FLOAT64, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("INT64", GAUGE_INT64, JS_PROP_CONFIGURABLE),
    JS_CGETSET_MAGIC_DEF("time", qjs_metric_gauge_get, qjs_metric_gauge_set,
                                 METRIC_GETSET_TIME),
    JS_CGETSET_MAGIC_DEF("interval", qjs_metric_gauge_get, qjs_metric_gauge_set,
                                     METRIC_GETSET_INTERVAL),
    JS_CGETSET_MAGIC_DEF("labels", qjs_metric_gauge_get, qjs_metric_gauge_set,
                                   METRIC_GETSET_LABELS),
    JS_CGETSET_MAGIC_DEF("value", qjs_metric_gauge_get, qjs_metric_gauge_set,
                                  METRIC_GETSET_VALUE),
    JS_CGETSET_MAGIC_DEF("type", qjs_metric_gauge_get, NULL,
                                 METRIC_GETSET_TYPE),
    JS_CFUNC_DEF("toString", 0, qjs_metric_gauge_to_string),
};

static int qjs_metric_gauge_init(JSContext *ctx, JSModuleDef *m)
{
    /* create the Point class */
    JS_NewClassID(&qjs_metric_gauge_class_id);
    JS_NewClass(JS_GetRuntime(ctx), qjs_metric_gauge_class_id, &qjs_metric_gauge_class);

    JSValue metric_gauge_double_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, metric_gauge_double_proto, qjs_metric_gauge_proto_funcs,
                                    countof(qjs_metric_gauge_proto_funcs));

    JSValue metric_gauge_double_class = JS_NewCFunction2(ctx, qjs_metric_gauge_ctor,
                                                           "MetricGauge", 4,
                                                           JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, metric_gauge_double_class, metric_gauge_double_proto);
    JS_SetClassProto(ctx, qjs_metric_gauge_class_id, metric_gauge_double_proto);

    JS_SetModuleExport(ctx, m, "MetricGauge", metric_gauge_double_class);

    return 0;
}

static void qjs_metric_counter_finalizer(JSRuntime *rt, JSValue val)
{
    metric_t *m = JS_GetOpaque(val, qjs_metric_counter_class_id);
    if (m != NULL) {
        metric_reset(m, METRIC_TYPE_COUNTER);
        free(m);
    }
}

static JSValue qjs_metric_counter_ctor(JSContext *ctx, JSValueConst new_target, int argc,
                                       JSValueConst *argv)
{
    JSValue obj = JS_UNDEFINED;

    metric_t *m = calloc(1, sizeof(*m));
    if (m == NULL)
        return JS_EXCEPTION;

    // "value", "labels", "time", "interval"

    if (!JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        if (JS_IsBigInt(ctx, argv[0])) {
            int64_t value = 0;
            if (JS_ToBigInt64(ctx, &value, argv[0]))
                return JS_EXCEPTION;
            m->value = VALUE_COUNTER_UINT64(value);
        } else if (JS_IsNumber(argv[0])) {
            double value = 0;
            if (JS_ToFloat64(ctx, &value, argv[0]))
                return JS_EXCEPTION;
            m->value = VALUE_COUNTER_FLOAT64(value);
        } else {
            return JS_EXCEPTION;
        }
    }

    if (!JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        JSValue ret = qjs_to_label_set(ctx, argv[1], &m->label);
        if (!JS_IsUndefined(ret))
            return JS_EXCEPTION;
    }

    if (!JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2])) {
        double mtime = 0;
        if (JS_ToFloat64(ctx, &mtime, argv[2]))
            return JS_EXCEPTION;
        m->time = DOUBLE_TO_CDTIME_T(mtime);
    }

    if (!JS_IsUndefined(argv[3]) && !JS_IsNull(argv[3])) {
        double minterval = 0;
        if (JS_ToFloat64(ctx, &minterval, argv[3]))
            return JS_EXCEPTION;
        m->interval = DOUBLE_TO_CDTIME_T(minterval);
    }

    /* using new_target to get the prototype is necessary when the class is extended. */
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, qjs_metric_counter_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;

    JS_SetOpaque(obj, m);

    return obj;

 fail:
    metric_reset(m, METRIC_TYPE_COUNTER);
    free(m);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue qjs_metric_counter_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_counter_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case METRIC_GETSET_TIME:
        return JS_NewFloat64(ctx, CDTIME_T_TO_DOUBLE(m->time));
        break;
    case METRIC_GETSET_INTERVAL:
        return JS_NewFloat64(ctx, CDTIME_T_TO_DOUBLE(m->interval));
        break;
    case METRIC_GETSET_LABELS:
        return qjs_from_label_set(ctx, &m->label);
        break;
    case METRIC_GETSET_VALUE:
        if (m->value.counter.type == COUNTER_UINT64) {
            return JS_NewBigUint64(ctx, m->value.counter.uint64);
        } else {
            return JS_NewFloat64(ctx, m->value.counter.float64);
        }
        break;
    case METRIC_GETSET_TYPE:
        return JS_NewInt32(ctx, m->value.counter.type == COUNTER_UINT64 ?
                                COUNTER_UINT64 : COUNTER_FLOAT64); 
        break;
    default:
        break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_metric_counter_set(JSContext *ctx, JSValueConst this_val, JSValue val,
                                                      int magic)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_counter_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case METRIC_GETSET_TIME: {
       double ntime = 0;
        if (JS_ToFloat64(ctx, &ntime, val))
            return JS_EXCEPTION;
        m->time = DOUBLE_TO_CDTIME_T(ntime);
    }   break;
    case METRIC_GETSET_INTERVAL: {
       double ninterval = 0;
        if (JS_ToFloat64(ctx, &ninterval, val))
            return JS_EXCEPTION;
        m->interval = DOUBLE_TO_CDTIME_T(ninterval);
    }   break;
    case METRIC_GETSET_LABELS:
        label_set_reset(&m->label);
        return qjs_to_label_set(ctx, val, &m->label);
        break;
    case METRIC_GETSET_VALUE:
        if (m->value.counter.type == COUNTER_UINT64) {
            int64_t value = 0;
            if (JS_ToBigInt64(ctx, &value, val))
                return JS_EXCEPTION;
            m->value = VALUE_COUNTER_UINT64(value);
        } else {
            double value = 0;
            if (JS_ToFloat64(ctx, &value, val))
                return JS_EXCEPTION;
            m->value = VALUE_COUNTER_FLOAT64(value);
        }
        break;
    default:
        break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_metric_counter_to_string(JSContext *ctx, JSValueConst this_val,
                                                   int argc, JSValueConst *argv)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_counter_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    strbuf_t buf = STRBUF_CREATE;

    metric_to_string(&buf, m, METRIC_TYPE_COUNTER);

    JSValue string = JS_NewStringLen(ctx, buf.ptr, strbuf_len(&buf));

    strbuf_destroy(&buf);

    return string;
}

static JSClassDef qjs_metric_counter_class = {
    "MetricCounter",
    .finalizer = qjs_metric_counter_finalizer
};

static const JSCFunctionListEntry qjs_metric_counter_proto_funcs[] = {
    JS_PROP_INT32_DEF("UINT64", COUNTER_UINT64, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("FLOAT64", COUNTER_FLOAT64, JS_PROP_CONFIGURABLE),
    JS_CGETSET_MAGIC_DEF("time", qjs_metric_counter_get, qjs_metric_counter_set,
                                 METRIC_GETSET_TIME),
    JS_CGETSET_MAGIC_DEF("interval", qjs_metric_counter_get, qjs_metric_counter_set,
                                     METRIC_GETSET_INTERVAL),
    JS_CGETSET_MAGIC_DEF("labels", qjs_metric_counter_get, qjs_metric_counter_set,
                                   METRIC_GETSET_LABELS),
    JS_CGETSET_MAGIC_DEF("value", qjs_metric_counter_get, qjs_metric_counter_set,
                                  METRIC_GETSET_VALUE),
    JS_CGETSET_MAGIC_DEF("type", qjs_metric_counter_get, NULL,
                                 METRIC_GETSET_TYPE),
    JS_CFUNC_DEF("toString", 0, qjs_metric_counter_to_string),
};

static int qjs_metric_counter_init(JSContext *ctx, JSModuleDef *m)
{
    /* create the Point class */
    JS_NewClassID(&qjs_metric_counter_class_id);
    JS_NewClass(JS_GetRuntime(ctx), qjs_metric_counter_class_id, &qjs_metric_counter_class);

    JSValue metric_counter_double_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, metric_counter_double_proto, qjs_metric_counter_proto_funcs,
                                    countof(qjs_metric_counter_proto_funcs));

    JSValue metric_counter_double_class = JS_NewCFunction2(ctx, qjs_metric_counter_ctor,
                                                           "MetricCounter", 4,
                                                           JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, metric_counter_double_class, metric_counter_double_proto);
    JS_SetClassProto(ctx, qjs_metric_counter_class_id, metric_counter_double_proto);

    JS_SetModuleExport(ctx, m, "MetricCounter", metric_counter_double_class);

    return 0;
}

static void qjs_metric_info_finalizer(JSRuntime *rt, JSValue val)
{
    metric_t *m = JS_GetOpaque(val, qjs_metric_info_class_id);
    if (m != NULL) {
        metric_reset(m, METRIC_TYPE_INFO);
        free(m);
    }
}

static JSValue qjs_metric_info_ctor(JSContext *ctx, JSValueConst new_target, int argc,
                                       JSValueConst *argv)
{
    JSValue obj = JS_UNDEFINED;

    metric_t *m = calloc(1, sizeof(*m));
    if (m == NULL)
        return JS_EXCEPTION;

    // "value", "labels", "time", "interval"

    if (!JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        JSValue ret = qjs_to_label_set(ctx, argv[0], &m->value.info);
        if (!JS_IsUndefined(ret))
            return JS_EXCEPTION;
    }

    if (!JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        JSValue ret = qjs_to_label_set(ctx, argv[1], &m->label);
        if (!JS_IsUndefined(ret))
            return JS_EXCEPTION;
    }

    if (!JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2])) {
        double mtime = 0;
        if (JS_ToFloat64(ctx, &mtime, argv[2]))
            return JS_EXCEPTION;
        m->time = DOUBLE_TO_CDTIME_T(mtime);
    }

    if (!JS_IsUndefined(argv[3]) && !JS_IsNull(argv[3])) {
        double minterval = 0;
        if (JS_ToFloat64(ctx, &minterval, argv[3]))
            return JS_EXCEPTION;
        m->interval = DOUBLE_TO_CDTIME_T(minterval);
    }

    /* using new_target to get the prototype is necessary when the class is extended. */
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, qjs_metric_info_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;

    JS_SetOpaque(obj, m);

    return obj;

 fail:
    metric_reset(m, METRIC_TYPE_INFO);
    free(m);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue qjs_metric_info_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_info_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case METRIC_GETSET_TIME:
        return JS_NewFloat64(ctx, CDTIME_T_TO_DOUBLE(m->time));
        break;
    case METRIC_GETSET_INTERVAL:
        return JS_NewFloat64(ctx, CDTIME_T_TO_DOUBLE(m->interval));
        break;
    case METRIC_GETSET_LABELS:
        return qjs_from_label_set(ctx, &m->label);
        break;
    case METRIC_GETSET_VALUE:
        return qjs_from_label_set(ctx, &m->value.info);
        break;
    default:
        break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_metric_info_set(JSContext *ctx, JSValueConst this_val, JSValue val,
                                                      int magic)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_info_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case METRIC_GETSET_TIME: {
       double ntime = 0;
        if (JS_ToFloat64(ctx, &ntime, val))
            return JS_EXCEPTION;
        m->time = DOUBLE_TO_CDTIME_T(ntime);
    }   break;
    case METRIC_GETSET_INTERVAL: {
       double ninterval = 0;
        if (JS_ToFloat64(ctx, &ninterval, val))
            return JS_EXCEPTION;
        m->interval = DOUBLE_TO_CDTIME_T(ninterval);
    }   break;
    case METRIC_GETSET_LABELS:
        label_set_reset(&m->label);
        return qjs_to_label_set(ctx, val, &m->label);
        break;
    case METRIC_GETSET_VALUE:
        label_set_reset(&m->value.info);
        return qjs_to_label_set(ctx, val, &m->value.info);
        break;
    default:
        break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_metric_info_to_string(JSContext *ctx, JSValueConst this_val,
                                                   int argc, JSValueConst *argv)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_info_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    strbuf_t buf = STRBUF_CREATE;

    metric_to_string(&buf, m, METRIC_TYPE_COUNTER);

    JSValue string = JS_NewStringLen(ctx, buf.ptr, strbuf_len(&buf));

    strbuf_destroy(&buf);

    return string;
}

static JSClassDef qjs_metric_info_class = {
    "MetricInfo",
    .finalizer = qjs_metric_info_finalizer
};

static const JSCFunctionListEntry qjs_metric_info_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("time", qjs_metric_info_get, qjs_metric_info_set,
                                 METRIC_GETSET_TIME),
    JS_CGETSET_MAGIC_DEF("interval", qjs_metric_info_get, qjs_metric_info_set,
                                     METRIC_GETSET_INTERVAL),
    JS_CGETSET_MAGIC_DEF("labels", qjs_metric_info_get, qjs_metric_info_set,
                                   METRIC_GETSET_LABELS),
    JS_CGETSET_MAGIC_DEF("value", qjs_metric_info_get, qjs_metric_info_set,
                                  METRIC_GETSET_VALUE),
    JS_CFUNC_DEF("toString", 0, qjs_metric_info_to_string),
};

static int qjs_metric_info_init(JSContext *ctx, JSModuleDef *m)
{
    /* create the Point class */
    JS_NewClassID(&qjs_metric_info_class_id);
    JS_NewClass(JS_GetRuntime(ctx), qjs_metric_info_class_id, &qjs_metric_info_class);

    JSValue metric_info_double_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, metric_info_double_proto, qjs_metric_info_proto_funcs,
                                    countof(qjs_metric_info_proto_funcs));

    JSValue metric_info_double_class = JS_NewCFunction2(ctx, qjs_metric_info_ctor,
                                                             "MetricInfo", 4,
                                                             JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, metric_info_double_class, metric_info_double_proto);
    JS_SetClassProto(ctx, qjs_metric_info_class_id, metric_info_double_proto);

    JS_SetModuleExport(ctx, m, "MetricInfo", metric_info_double_class);

    return 0;
}

static JSValue qjs_from_state_set(JSContext *ctx, const state_set_t *set)
{
    JSValue jset = JS_NewObject(ctx);
    if (JS_IsException(jset))
        return jset;

    for (size_t i = 0; i < set->num; i++) {
        char const *name = set->ptr[i].name;
        bool value = set->ptr[i].enabled;
        JS_DefinePropertyValueStr(ctx, jset, name, JS_NewBool(ctx, value), JS_PROP_C_W_E);
    }

    return jset;
}

static JSValue qjs_to_state_set(JSContext *ctx, JSValueConst jset, state_set_t *set)
{
    if (!JS_IsObject(jset))
        return JS_ThrowTypeError(ctx, "state set must be an object");

    JSPropertyEnum *tab;
    uint32_t len;
    if (JS_GetOwnPropertyNames(ctx, &tab, &len, jset, JS_GPN_ENUM_ONLY | JS_GPN_STRING_MASK))
        return JS_ThrowTypeError(ctx, "cannot get property names");

    for(uint32_t i = 0; i < len; i++) {
        JSValue val = JS_GetProperty(ctx, jset, tab[i].atom);
        if (JS_IsException(val))
            goto fail;

        int ret = JS_ToBool(ctx, val);
        if (ret < 0)
            goto fail;
        bool value = ret == 0 ? false : true;
        
        const char *key = JS_AtomToCString(ctx, tab[i].atom);
        if (key == NULL) {
            goto fail;
        }

        state_set_add(set, key, value);

        JS_FreeCString(ctx, key);
    }

    JS_FreePropertyEnum(ctx, tab, len);
    return JS_UNDEFINED;

fail:
    JS_FreePropertyEnum(ctx, tab, len);
    return JS_EXCEPTION;
}

static void qjs_metric_state_set_finalizer(JSRuntime *rt, JSValue val)
{
    metric_t *m = JS_GetOpaque(val, qjs_metric_state_set_class_id);
    if (m != NULL) {
        metric_reset(m, METRIC_TYPE_STATE_SET);
        free(m);
    }
}

static JSValue qjs_metric_state_set_ctor(JSContext *ctx, JSValueConst new_target, int argc,
                                       JSValueConst *argv)
{
    JSValue obj = JS_UNDEFINED;

    metric_t *m = calloc(1, sizeof(*m));
    if (m == NULL)
        return JS_EXCEPTION;

    // "value", "labels", "time", "interval"

    if (!JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        JSValue ret = qjs_to_state_set(ctx, argv[0], &m->value.state_set);
        if (!JS_IsUndefined(ret))
            return JS_EXCEPTION;
    }

    if (!JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        JSValue ret = qjs_to_label_set(ctx, argv[1], &m->label);
        if (!JS_IsUndefined(ret))
            return JS_EXCEPTION;
    }

    if (!JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2])) {
        double mtime = 0;
        if (JS_ToFloat64(ctx, &mtime, argv[2]))
            return JS_EXCEPTION;
        m->time = DOUBLE_TO_CDTIME_T(mtime);
    }

    if (!JS_IsUndefined(argv[3]) && !JS_IsNull(argv[3])) {
        double minterval = 0;
        if (JS_ToFloat64(ctx, &minterval, argv[3]))
            return JS_EXCEPTION;
        m->interval = DOUBLE_TO_CDTIME_T(minterval);
    }

    /* using new_target to get the prototype is necessary when the class is extended. */
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, qjs_metric_state_set_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;

    JS_SetOpaque(obj, m);

    return obj;

 fail:
    metric_reset(m, METRIC_TYPE_STATE_SET);
    free(m);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue qjs_metric_state_set_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_state_set_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case METRIC_GETSET_TIME:
        return JS_NewFloat64(ctx, CDTIME_T_TO_DOUBLE(m->time));
        break;
    case METRIC_GETSET_INTERVAL:
        return JS_NewFloat64(ctx, CDTIME_T_TO_DOUBLE(m->interval));
        break;
    case METRIC_GETSET_LABELS:
        return qjs_from_label_set(ctx, &m->label);
        break;
    case METRIC_GETSET_VALUE:
        return qjs_from_state_set(ctx, &m->value.state_set);
        break;
    default:
        break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_metric_state_set_set(JSContext *ctx, JSValueConst this_val, JSValue val,
                                                      int magic)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_state_set_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case METRIC_GETSET_TIME: {
       double ntime = 0;
        if (JS_ToFloat64(ctx, &ntime, val))
            return JS_EXCEPTION;
        m->time = DOUBLE_TO_CDTIME_T(ntime);
    }   break;
    case METRIC_GETSET_INTERVAL: {
       double ninterval = 0;
        if (JS_ToFloat64(ctx, &ninterval, val))
            return JS_EXCEPTION;
        m->interval = DOUBLE_TO_CDTIME_T(ninterval);
    }   break;
    case METRIC_GETSET_LABELS:
        label_set_reset(&m->label);
        return qjs_to_label_set(ctx, val, &m->label);
        break;
    case METRIC_GETSET_VALUE:
        state_set_reset(&m->value.state_set);
        return qjs_to_state_set(ctx, val, &m->value.state_set);
        break;
    default:
        break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_metric_state_set_to_string(JSContext *ctx, JSValueConst this_val,
                                                   int argc, JSValueConst *argv)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_state_set_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    strbuf_t buf = STRBUF_CREATE;

    metric_to_string(&buf, m, METRIC_TYPE_COUNTER);

    JSValue string = JS_NewStringLen(ctx, buf.ptr, strbuf_len(&buf));

    strbuf_destroy(&buf);

    return string;
}

static JSClassDef qjs_metric_state_set_class = {
    "MetricStateSet",
    .finalizer = qjs_metric_state_set_finalizer
};

static const JSCFunctionListEntry qjs_metric_state_set_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("time", qjs_metric_state_set_get, qjs_metric_state_set_set,
                                 METRIC_GETSET_TIME),
    JS_CGETSET_MAGIC_DEF("interval", qjs_metric_state_set_get, qjs_metric_state_set_set,
                                     METRIC_GETSET_INTERVAL),
    JS_CGETSET_MAGIC_DEF("labels", qjs_metric_state_set_get, qjs_metric_state_set_set,
                                   METRIC_GETSET_LABELS),
    JS_CGETSET_MAGIC_DEF("value", qjs_metric_state_set_get, qjs_metric_state_set_set,
                                  METRIC_GETSET_VALUE),
    JS_CFUNC_DEF("toString", 0, qjs_metric_state_set_to_string),
};

static int qjs_metric_state_set_init(JSContext *ctx, JSModuleDef *m)
{
    /* create the Point class */
    JS_NewClassID(&qjs_metric_state_set_class_id);
    JS_NewClass(JS_GetRuntime(ctx), qjs_metric_state_set_class_id, &qjs_metric_state_set_class);

    JSValue metric_state_set_double_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, metric_state_set_double_proto, qjs_metric_state_set_proto_funcs,
                                    countof(qjs_metric_state_set_proto_funcs));

    JSValue metric_state_set_double_class = JS_NewCFunction2(ctx, qjs_metric_state_set_ctor,
                                                             "MetricStateSet", 4,
                                                             JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, metric_state_set_double_class, metric_state_set_double_proto);
    JS_SetClassProto(ctx, qjs_metric_state_set_class_id, metric_state_set_double_proto);

    JS_SetModuleExport(ctx, m, "MetricStateSet", metric_state_set_double_class);

    return 0;
}

static JSValue qjs_from_quantiles(JSContext *ctx, summary_t *summary)
{
    JSValue jquantiles = JS_NewArray(ctx);
    if (JS_IsException(jquantiles))
        return jquantiles;

    for (size_t i = 0; i < summary->num; i++) {
        summary_quantile_t *quantile = &summary->quantiles[i];

        JSValue jquantile = JS_NewArray(ctx);
        if (JS_IsException(jquantile)) {
            JS_FreeValue(ctx, jquantiles);
            return jquantile;
        }

        JS_DefinePropertyValueUint32(ctx, jquantile, 0,
                                     JS_NewFloat64(ctx, quantile->quantile), JS_PROP_C_W_E);
        JS_DefinePropertyValueUint32(ctx, jquantile, 1,
                                     JS_NewFloat64(ctx, quantile->value), JS_PROP_C_W_E);
    
        JS_DefinePropertyValueUint32(ctx, jquantiles, i, jquantile, JS_PROP_C_W_E);
    }

    return jquantiles;
}

static JSValue qjs_to_quantile(JSContext *ctx, JSValueConst jpair, summary_quantile_t *quantile)
{       
    uint32_t qlen = 0;
    qjs_array_get_length(ctx, jpair, &qlen);

    if (qlen != 2)
        return JS_EXCEPTION;

    JSValueConst jquantile = JS_GetPropertyUint32(ctx, jpair, 0);
    if (JS_IsException(jquantile))
        return jquantile;
    if (JS_IsUndefined(jquantile))
        return JS_EXCEPTION;
    if (!JS_IsNumber(jquantile)) {
        JS_FreeValue(ctx, jquantile);
        return JS_EXCEPTION;
    }

    if (JS_ToFloat64(ctx, &quantile->quantile, jquantile)) {
        JS_FreeValue(ctx, jquantile);
        return JS_EXCEPTION;
    }

    JS_FreeValue(ctx, jquantile);

    JSValueConst jvalue = JS_GetPropertyUint32(ctx, jpair, 1);
    if (JS_IsException(jvalue)) {
        return jvalue;
    }
    if (JS_IsUndefined(jvalue)) {
        return JS_EXCEPTION;
    }
    if (!JS_IsNumber(jvalue)) {
        JS_FreeValue(ctx, jvalue);
        return JS_EXCEPTION;
    }

    if (JS_ToFloat64(ctx, &quantile->value, jvalue)) {
        JS_FreeValue(ctx, jvalue);
        return JS_EXCEPTION;
    }

    JS_FreeValue(ctx, jvalue);
    
    return JS_UNDEFINED; 
}

static JSValue qjs_to_quantiles(JSContext *ctx, JSValueConst jquantiles, summary_t **rsummary)
{
    if (!JS_IsArray(ctx, jquantiles))
//       return JS_ThrowTypeError(ctx, "label set must be an object");
        return JS_EXCEPTION;

    uint32_t len = 0;
    qjs_array_get_length(ctx, jquantiles, &len);

    summary_t *summary = *rsummary;

    for (uint32_t i = 0; i < len; i++) {
        JSValueConst jpair = JS_GetPropertyUint32(ctx, jquantiles, i);
        if (JS_IsException(jpair))
            return jpair;

        if (JS_IsUndefined(jpair))
            return JS_EXCEPTION;
 
        if (!JS_IsArray(ctx, jpair)) {
            JS_FreeValue(ctx, jpair);
            return JS_EXCEPTION;
        }
        
        summary_quantile_t quantile = {0, 0};
        JSValue jret = qjs_to_quantile(ctx, jpair, &quantile);
        if (JS_IsException(jret)) {
            JS_FreeValue(ctx, jpair);
            return jret;
        }

        summary = summary_quantile_append(summary, quantile.quantile, quantile.value);

        JS_FreeValue(ctx, jpair);
    }

    *rsummary = summary;

    return JS_UNDEFINED;
}

static void qjs_metric_summary_finalizer(JSRuntime *rt, JSValue val)
{
    metric_t *m = JS_GetOpaque(val, qjs_metric_summary_class_id);
    if (m != NULL) {
        metric_reset(m, METRIC_TYPE_SUMMARY);
        free(m);
    }
}

static JSValue qjs_metric_summary_ctor(JSContext *ctx, JSValueConst new_target, int argc,
                                       JSValueConst *argv)
{
    JSValue obj = JS_UNDEFINED;

    metric_t *m = calloc(1, sizeof(*m));
    if (m == NULL)
        return JS_EXCEPTION;

    summary_t *summary = summary_new();
    if (summary == NULL) {
        free(m);
        return JS_EXCEPTION;
    }
    
    m->value.summary = summary;

    // "sum", "count", "quantiles", "labels", "time", "interval"

    if (!JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        double sum = 0;
        if (JS_ToFloat64(ctx, &sum, argv[0]))
            return JS_EXCEPTION;
        m->value.summary->sum = sum;
    }

    if (!JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        int64_t count = 0;
        if (JS_ToInt64(ctx, &count, argv[1])) 
            return JS_EXCEPTION;
        m->value.summary->count = count;
    }

    if (!JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2])) {
        JSValue ret = qjs_to_quantiles(ctx, argv[2], &m->value.summary);
        if (!JS_IsUndefined(ret))
            return JS_EXCEPTION;
    }

    if (!JS_IsUndefined(argv[3]) && !JS_IsNull(argv[3])) {
        JSValue ret = qjs_to_label_set(ctx, argv[3], &m->label);
        if (!JS_IsUndefined(ret))
            return JS_EXCEPTION;
    }

    if (!JS_IsUndefined(argv[4]) && !JS_IsNull(argv[4])) {
        double mtime = 0;
        if (JS_ToFloat64(ctx, &mtime, argv[4]))
            return JS_EXCEPTION;
        m->time = DOUBLE_TO_CDTIME_T(mtime);
    }

    if (!JS_IsUndefined(argv[5]) && !JS_IsNull(argv[5])) {
        double minterval = 0;
        if (JS_ToFloat64(ctx, &minterval, argv[5]))
            return JS_EXCEPTION;
        m->interval = DOUBLE_TO_CDTIME_T(minterval);
    }

    /* using new_target to get the prototype is necessary when the class is extended. */
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, qjs_metric_summary_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;

    JS_SetOpaque(obj, m);

    return obj;

 fail:
    metric_reset(m, METRIC_TYPE_SUMMARY);
    free(m);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue qjs_metric_summary_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_summary_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case METRIC_GETSET_TIME:
        return JS_NewFloat64(ctx, CDTIME_T_TO_DOUBLE(m->time));
        break;
    case METRIC_GETSET_INTERVAL:
        return JS_NewFloat64(ctx, CDTIME_T_TO_DOUBLE(m->interval));
        break;
    case METRIC_GETSET_LABELS:
        return qjs_from_label_set(ctx, &m->label);
        break;
    case METRIC_GETSET_SUM:
        return JS_NewFloat64(ctx, m->value.summary->sum);
        break;
    case METRIC_GETSET_COUNT:
        return JS_NewInt64(ctx, m->value.summary->count);
        break;
    case METRIC_GETSET_QUANTILES:
        return qjs_from_quantiles(ctx, m->value.summary);
        break;
    default:
        break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_metric_summary_set(JSContext *ctx, JSValueConst this_val, JSValue val,
                                                      int magic)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_summary_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case METRIC_GETSET_TIME: {
       double ntime = 0;
        if (JS_ToFloat64(ctx, &ntime, val))
            return JS_EXCEPTION;
        m->time = DOUBLE_TO_CDTIME_T(ntime);
    }   break;
    case METRIC_GETSET_INTERVAL: {
       double ninterval = 0;
        if (JS_ToFloat64(ctx, &ninterval, val))
            return JS_EXCEPTION;
        m->interval = DOUBLE_TO_CDTIME_T(ninterval);
    }   break;
    case METRIC_GETSET_LABELS:
        label_set_reset(&m->label);
        return qjs_to_label_set(ctx, val, &m->label);
        break;
    case METRIC_GETSET_SUM: {
       double sum = 0;
        if (JS_ToFloat64(ctx, &sum, val))
            return JS_EXCEPTION;
        m->value.summary->sum = sum;
    }   break;
    case METRIC_GETSET_COUNT: {
        int64_t count = 0;
        if (JS_ToInt64(ctx, &count, val))
            return JS_EXCEPTION;
        m->value.summary->count = count;
    }   break;
    case METRIC_GETSET_QUANTILES: {
        JSValue ret = qjs_to_quantiles(ctx, val, &m->value.summary);
        if (!JS_IsUndefined(ret))
            return JS_EXCEPTION;
    }   break;
    default:
        break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_metric_summary_to_string(JSContext *ctx, JSValueConst this_val,
                                                   int argc, JSValueConst *argv)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_summary_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    strbuf_t buf = STRBUF_CREATE;

    metric_to_string(&buf, m, METRIC_TYPE_COUNTER);

    JSValue string = JS_NewStringLen(ctx, buf.ptr, strbuf_len(&buf));

    strbuf_destroy(&buf);

    return string;
}

static JSClassDef qjs_metric_summary_class = {
    "MetricSummary",
    .finalizer = qjs_metric_summary_finalizer
};

static const JSCFunctionListEntry qjs_metric_summary_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("time", qjs_metric_summary_get, qjs_metric_summary_set,
                                 METRIC_GETSET_TIME),
    JS_CGETSET_MAGIC_DEF("interval", qjs_metric_summary_get, qjs_metric_summary_set,
                                     METRIC_GETSET_INTERVAL),
    JS_CGETSET_MAGIC_DEF("labels", qjs_metric_summary_get, qjs_metric_summary_set,
                                   METRIC_GETSET_LABELS),
    JS_CGETSET_MAGIC_DEF("sum", qjs_metric_summary_get, qjs_metric_summary_set,
                                METRIC_GETSET_SUM),
    JS_CGETSET_MAGIC_DEF("count", qjs_metric_summary_get, qjs_metric_summary_set,
                                  METRIC_GETSET_COUNT),
    JS_CGETSET_MAGIC_DEF("quantiles", qjs_metric_summary_get, qjs_metric_summary_set,
                                      METRIC_GETSET_QUANTILES),
    JS_CFUNC_DEF("toString", 0, qjs_metric_summary_to_string),
};

static int qjs_metric_summary_init(JSContext *ctx, JSModuleDef *m)
{
    /* create the Point class */
    JS_NewClassID(&qjs_metric_summary_class_id);
    JS_NewClass(JS_GetRuntime(ctx), qjs_metric_summary_class_id, &qjs_metric_summary_class);

    JSValue metric_summary_double_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, metric_summary_double_proto, qjs_metric_summary_proto_funcs,
                                    countof(qjs_metric_summary_proto_funcs));

    JSValue metric_summary_double_class = JS_NewCFunction2(ctx, qjs_metric_summary_ctor,
                                                             "MetricSummary", 6,
                                                             JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, metric_summary_double_class, metric_summary_double_proto);
    JS_SetClassProto(ctx, qjs_metric_summary_class_id, metric_summary_double_proto);

    JS_SetModuleExport(ctx, m, "MetricSummary", metric_summary_double_class);

    return 0;
}

///

static JSValue qjs_from_buckets(JSContext *ctx, histogram_t *histogram)
{
    JSValue jbuckets = JS_NewArray(ctx);
    if (JS_IsException(jbuckets))
        return jbuckets;

    for (size_t i = 0; i < histogram->num; i++) {
        histogram_bucket_t *bucket = &histogram->buckets[i];

        JSValue jbucket = JS_NewArray(ctx);
        if (JS_IsException(jbucket)) {
            JS_FreeValue(ctx, jbucket);
            return jbucket;
        }

        JS_DefinePropertyValueUint32(ctx, jbucket, 0,
                                     JS_NewBigUint64(ctx, bucket->counter), JS_PROP_C_W_E);
        JS_DefinePropertyValueUint32(ctx, jbucket, 1,
                                     JS_NewFloat64(ctx, bucket->maximum), JS_PROP_C_W_E);
    
        JS_DefinePropertyValueUint32(ctx, jbuckets, i, jbucket, JS_PROP_C_W_E);
    }

    return jbuckets;
}

static JSValue qjs_to_bucket(JSContext *ctx, JSValueConst jpair, histogram_bucket_t *bucket)
{       
    uint32_t qlen = 0;
    qjs_array_get_length(ctx, jpair, &qlen);

    if (qlen != 2)
        return JS_EXCEPTION;

    JSValueConst jcounter = JS_GetPropertyUint32(ctx, jpair, 0);
    if (JS_IsException(jcounter))
        return jcounter;
    if (JS_IsUndefined(jcounter))
        return JS_EXCEPTION;

    if (!(JS_IsBigInt(ctx, jcounter) || JS_IsNumber(jcounter))) {
        JS_FreeValue(ctx, jcounter);
        return JS_EXCEPTION;
    }

    int64_t value = 0;        
    if (JS_ToInt64Ext(ctx, &value, jcounter)) {
        JS_FreeValue(ctx, jcounter);
        return JS_EXCEPTION;
    }

    bucket->counter = value;

    JS_FreeValue(ctx, jcounter);

    JSValueConst jmaximun = JS_GetPropertyUint32(ctx, jpair, 1);
    if (JS_IsException(jmaximun)) {
        return jmaximun;
    }
    if (JS_IsUndefined(jmaximun)) {
        return JS_EXCEPTION;
    }
    if (!JS_IsNumber(jmaximun)) {
        JS_FreeValue(ctx, jmaximun);
        return JS_EXCEPTION;
    }

    if (JS_ToFloat64(ctx, &bucket->maximum, jmaximun)) {
        JS_FreeValue(ctx, jmaximun);
        return JS_EXCEPTION;
    }

    JS_FreeValue(ctx, jmaximun);
    
    return JS_UNDEFINED; 
}

static JSValue qjs_to_buckets(JSContext *ctx, JSValueConst jbuckets, histogram_t **rhistogram)
{
    if (!JS_IsArray(ctx, jbuckets))
        return JS_EXCEPTION;

    uint32_t len = 0;
    qjs_array_get_length(ctx, jbuckets, &len);

    histogram_t *histogram = *rhistogram;

    for (uint32_t i = 0; i < len; i++) {
        JSValueConst jpair = JS_GetPropertyUint32(ctx, jbuckets, i);
        if (JS_IsException(jpair))
            return jpair;

        if (JS_IsUndefined(jpair))
            return JS_EXCEPTION;

        if (!JS_IsArray(ctx, jpair)) {
            JS_FreeValue(ctx, jpair);
            return JS_EXCEPTION;
        }

        histogram_bucket_t bucket = {0, 0};
        JSValue jret = qjs_to_bucket(ctx, jpair, &bucket);
        if (JS_IsException(jret)) {
            JS_FreeValue(ctx, jpair);
            return jret;
        }

        histogram = histogram_bucket_append(histogram, bucket.maximum, bucket.counter);

        JS_FreeValue(ctx, jpair);
    }

    *rhistogram = histogram;

    return JS_UNDEFINED;
}

static JSValue qjs_metric_generic_histogram_ctor(JSContext *ctx, JSValueConst new_target, int argc,
                                                 JSValueConst *argv, JSClassID class_id,
                                                 metric_type_t type)
{
    JSValue obj = JS_UNDEFINED;

    metric_t *m = calloc(1, sizeof(*m));
    if (m == NULL)
        return JS_EXCEPTION;

    histogram_t *histogram = histogram_new();
    if (histogram == NULL) {
        free(m);
        return JS_EXCEPTION;
    }
    
    m->value.histogram = histogram;
    // "gsum", "buckets", "labels", "time", "interval"

    if (!JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        double sum = 0;
        if (JS_ToFloat64(ctx, &sum, argv[0]))
            return JS_EXCEPTION;
        m->value.summary->sum = sum;
    }

    if (!JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        JSValue ret = qjs_to_buckets(ctx, argv[1], &m->value.histogram);
        if (!JS_IsUndefined(ret))
            return JS_EXCEPTION;
    }

    if (!JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2])) {
        JSValue ret = qjs_to_label_set(ctx, argv[2], &m->label);
        if (!JS_IsUndefined(ret))
            return JS_EXCEPTION;
    }

    if (!JS_IsUndefined(argv[3]) && !JS_IsNull(argv[3])) {
        double mtime = 0;
        if (JS_ToFloat64(ctx, &mtime, argv[3]))
            return JS_EXCEPTION;
        m->time = DOUBLE_TO_CDTIME_T(mtime);
    }

    if (!JS_IsUndefined(argv[4]) && !JS_IsNull(argv[4])) {
        double minterval = 0;
        if (JS_ToFloat64(ctx, &minterval, argv[4]))
            return JS_EXCEPTION;
        m->interval = DOUBLE_TO_CDTIME_T(minterval);
    }

    /* using new_target to get the prototype is necessary when the class is extended. */
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;

    JS_SetOpaque(obj, m);

    return obj;

 fail:
    metric_reset(m, type);
    free(m);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue qjs_metric_generic_histogram_get(JSContext *ctx, JSValueConst this_val, int magic,
                                                JSClassID class_id)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case METRIC_GETSET_TIME:
        return JS_NewFloat64(ctx, CDTIME_T_TO_DOUBLE(m->time));
        break;
    case METRIC_GETSET_INTERVAL:
        return JS_NewFloat64(ctx, CDTIME_T_TO_DOUBLE(m->interval));
        break;
    case METRIC_GETSET_LABELS:
        return qjs_from_label_set(ctx, &m->label);
        break;
    case METRIC_GETSET_SUM:
        return JS_NewFloat64(ctx, histogram_sum(m->value.histogram));
        break;
    case METRIC_GETSET_BUCKETS:
        return qjs_from_buckets(ctx, m->value.histogram);
        break;
    default:
        break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_metric_generic_histogram_set(JSContext *ctx, JSValueConst this_val, JSValue val,
                                                int magic, JSClassID class_id)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case METRIC_GETSET_TIME: {
       double ntime = 0;
        if (JS_ToFloat64(ctx, &ntime, val))
            return JS_EXCEPTION;
        m->time = DOUBLE_TO_CDTIME_T(ntime);
    }   break;
    case METRIC_GETSET_INTERVAL: {
       double ninterval = 0;
        if (JS_ToFloat64(ctx, &ninterval, val))
            return JS_EXCEPTION;
        m->interval = DOUBLE_TO_CDTIME_T(ninterval);
    }   break;
    case METRIC_GETSET_LABELS:
        label_set_reset(&m->label);
        return qjs_to_label_set(ctx, val, &m->label);
        break;
    case METRIC_GETSET_SUM: {
       double sum = 0;
        if (JS_ToFloat64(ctx, &sum, val))
            return JS_EXCEPTION;
        m->value.histogram->sum = sum;
    }   break;
    case METRIC_GETSET_BUCKETS: {
        double sum = histogram_sum(m->value.histogram);
        free(m->value.histogram);
        m->value.histogram = histogram_new();
        m->value.histogram->sum = sum;
        JSValue ret = qjs_to_buckets(ctx, val, &m->value.histogram);
        if (!JS_IsUndefined(ret))
            return JS_EXCEPTION;
    }   break;
    default:
        break;
    }

    return JS_UNDEFINED;
}

static void qjs_metric_gauge_histogram_finalizer(JSRuntime *rt, JSValue val)
{
    metric_t *m = JS_GetOpaque(val, qjs_metric_gauge_histogram_class_id);
    if (m != NULL) {
        metric_reset(m, METRIC_TYPE_GAUGE_HISTOGRAM);
        free(m);
    }
}

static JSValue qjs_metric_gauge_histogram_ctor(JSContext *ctx, JSValueConst new_target, int argc,
                                       JSValueConst *argv)
{
    return qjs_metric_generic_histogram_ctor(ctx, new_target, argc, argv,
                                             qjs_metric_gauge_histogram_class_id,
                                             METRIC_TYPE_GAUGE_HISTOGRAM);
}


static JSValue qjs_metric_gauge_histogram_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    return qjs_metric_generic_histogram_get(ctx, this_val, magic,
                                            qjs_metric_gauge_histogram_class_id);
}

static JSValue qjs_metric_gauge_histogram_set(JSContext *ctx, JSValueConst this_val, JSValue val,
                                                      int magic)
{
    return qjs_metric_generic_histogram_set(ctx, this_val, val, magic,
                                            qjs_metric_gauge_histogram_class_id);
}

static JSValue qjs_metric_gauge_histogram_to_string(JSContext *ctx, JSValueConst this_val,
                                                   int argc, JSValueConst *argv)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_gauge_histogram_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    strbuf_t buf = STRBUF_CREATE;

    metric_to_string(&buf, m, METRIC_TYPE_GAUGE_HISTOGRAM);

    JSValue string = JS_NewStringLen(ctx, buf.ptr, strbuf_len(&buf));

    strbuf_destroy(&buf);

    return string;
}

static JSClassDef qjs_metric_gauge_histogram_class = {
    "MetricGaugeHistogram",
    .finalizer = qjs_metric_gauge_histogram_finalizer
};

static const JSCFunctionListEntry qjs_metric_gauge_histogram_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("time", qjs_metric_gauge_histogram_get, qjs_metric_gauge_histogram_set,
                                 METRIC_GETSET_TIME),
    JS_CGETSET_MAGIC_DEF("interval", qjs_metric_gauge_histogram_get, qjs_metric_gauge_histogram_set,
                                     METRIC_GETSET_INTERVAL),
    JS_CGETSET_MAGIC_DEF("labels", qjs_metric_gauge_histogram_get, qjs_metric_gauge_histogram_set,
                                   METRIC_GETSET_LABELS),
    JS_CGETSET_MAGIC_DEF("gsum", qjs_metric_gauge_histogram_get, qjs_metric_gauge_histogram_set,
                                 METRIC_GETSET_SUM),
    JS_CGETSET_MAGIC_DEF("buckets", qjs_metric_gauge_histogram_get, qjs_metric_gauge_histogram_set,
                                    METRIC_GETSET_BUCKETS),
    JS_CFUNC_DEF("toString", 0, qjs_metric_gauge_histogram_to_string),
};

static int qjs_metric_gauge_histogram_init(JSContext *ctx, JSModuleDef *m)
{
    /* create the Point class */
    JS_NewClassID(&qjs_metric_gauge_histogram_class_id);
    JS_NewClass(JS_GetRuntime(ctx), qjs_metric_gauge_histogram_class_id,
                                    &qjs_metric_gauge_histogram_class);

    JSValue metric_gauge_histogram_double_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, metric_gauge_histogram_double_proto,
                                    qjs_metric_gauge_histogram_proto_funcs,
                                    countof(qjs_metric_gauge_histogram_proto_funcs));

    JSValue metric_gauge_histogram_double_class = JS_NewCFunction2(ctx,
                                                           qjs_metric_gauge_histogram_ctor,
                                                           "MetricGaugeHistogram", 5,
                                                           JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, metric_gauge_histogram_double_class,
                           metric_gauge_histogram_double_proto);
    JS_SetClassProto(ctx, qjs_metric_gauge_histogram_class_id,
                          metric_gauge_histogram_double_proto);

    JS_SetModuleExport(ctx, m, "MetricGaugeHistogram", metric_gauge_histogram_double_class);

    return 0;
}

static void qjs_metric_histogram_finalizer(JSRuntime *rt, JSValue val)
{
    metric_t *m = JS_GetOpaque(val, qjs_metric_histogram_class_id);
    if (m != NULL) {
        metric_reset(m, METRIC_TYPE_HISTOGRAM);
        free(m);
    }
}

static JSValue qjs_metric_histogram_ctor(JSContext *ctx, JSValueConst new_target, int argc,
                                       JSValueConst *argv)
{
    return qjs_metric_generic_histogram_ctor(ctx, new_target, argc, argv,
                                             qjs_metric_histogram_class_id, METRIC_TYPE_HISTOGRAM);
}


static JSValue qjs_metric_histogram_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    return qjs_metric_generic_histogram_get(ctx, this_val, magic,
                                            qjs_metric_histogram_class_id);
}

static JSValue qjs_metric_histogram_set(JSContext *ctx, JSValueConst this_val, JSValue val,
                                                      int magic)
{
    return qjs_metric_generic_histogram_set(ctx, this_val, val, magic,
                                            qjs_metric_histogram_class_id);
}


static JSValue qjs_metric_histogram_to_string(JSContext *ctx, JSValueConst this_val,
                                              int argc, JSValueConst *argv)
{
    metric_t *m = JS_GetOpaque2(ctx, this_val, qjs_metric_histogram_class_id);
    if (m == NULL)
        return JS_EXCEPTION;

    strbuf_t buf = STRBUF_CREATE;

    metric_to_string(&buf, m, METRIC_TYPE_HISTOGRAM);

    JSValue string = JS_NewStringLen(ctx, buf.ptr, strbuf_len(&buf));

    strbuf_destroy(&buf);

    return string;
}

static JSClassDef qjs_metric_histogram_class = {
    "MetricHistogram",
    .finalizer = qjs_metric_histogram_finalizer
};

static const JSCFunctionListEntry qjs_metric_histogram_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("time", qjs_metric_histogram_get, qjs_metric_histogram_set,
                                 METRIC_GETSET_TIME),
    JS_CGETSET_MAGIC_DEF("interval", qjs_metric_histogram_get, qjs_metric_histogram_set,
                                     METRIC_GETSET_INTERVAL),
    JS_CGETSET_MAGIC_DEF("labels", qjs_metric_histogram_get, qjs_metric_histogram_set,
                                   METRIC_GETSET_LABELS),
    JS_CGETSET_MAGIC_DEF("sum", qjs_metric_histogram_get, qjs_metric_histogram_set,
                                METRIC_GETSET_SUM),
    JS_CGETSET_MAGIC_DEF("buckets", qjs_metric_histogram_get, qjs_metric_histogram_set,
                                    METRIC_GETSET_BUCKETS),
    JS_CFUNC_DEF("toString", 0, qjs_metric_histogram_to_string),
};

static int qjs_metric_histogram_init(JSContext *ctx, JSModuleDef *m)
{
    /* create the Point class */
    JS_NewClassID(&qjs_metric_histogram_class_id);
    JS_NewClass(JS_GetRuntime(ctx), qjs_metric_histogram_class_id, &qjs_metric_histogram_class);

    JSValue metric_histogram_double_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, metric_histogram_double_proto, qjs_metric_histogram_proto_funcs,
                                    countof(qjs_metric_histogram_proto_funcs));

    JSValue metric_histogram_double_class = JS_NewCFunction2(ctx, qjs_metric_histogram_ctor,
                                                             "MetricHistogram", 5,
                                                             JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, metric_histogram_double_class, metric_histogram_double_proto);
    JS_SetClassProto(ctx, qjs_metric_histogram_class_id, metric_histogram_double_proto);

    JS_SetModuleExport(ctx, m, "MetricHistogram", metric_histogram_double_class);

    return 0;
}

JSValue qjs_metric_new(JSContext *ctx, metric_t *m, metric_type_t type)
{
    JSClassID class_id;

    switch(type) {
    case METRIC_TYPE_UNKNOWN:
        class_id = qjs_metric_unknown_class_id;
        break;
    case METRIC_TYPE_GAUGE:
        class_id = qjs_metric_gauge_class_id;
        break;
    case METRIC_TYPE_COUNTER:
        class_id = qjs_metric_counter_class_id;
        break;
    case METRIC_TYPE_STATE_SET:
        class_id = qjs_metric_state_set_class_id;
        break;
    case METRIC_TYPE_INFO:
        class_id = qjs_metric_info_class_id;
        break;
    case METRIC_TYPE_SUMMARY:
        class_id = qjs_metric_summary_class_id;
        break;
    case METRIC_TYPE_HISTOGRAM:
        class_id = qjs_metric_histogram_class_id;
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        class_id = qjs_metric_gauge_histogram_class_id;
        break;
    default:
        return JS_EXCEPTION; 
        break;
    }

    JSValue obj = JS_NewObjectClass(ctx, class_id);
    if (JS_IsException(obj))
        return obj;

    metric_t *mdup = calloc(1, sizeof(*mdup));
    if (mdup == NULL) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    mdup->time = m->time;
    mdup->interval = m->interval;
    label_set_clone(&mdup->label, m->label);
    metric_value_clone(&mdup->value, m->value, type);

    JS_SetOpaque(obj, mdup);

    return obj;
}

metric_type_t qjs_metric_get_metric_type(JSContext *ctx, JSValueConst v)
{
    JSClassID class_id = JS_GetClassID(v);

    if (class_id == qjs_metric_unknown_class_id) {
        return METRIC_TYPE_UNKNOWN;
    } else if (class_id == qjs_metric_gauge_class_id) {
        return METRIC_TYPE_GAUGE;
    } else if (class_id == qjs_metric_counter_class_id) {
        return METRIC_TYPE_COUNTER;
    } else if (class_id == qjs_metric_info_class_id) {
        return METRIC_TYPE_INFO;
    } else if (class_id == qjs_metric_state_set_class_id) {
        return METRIC_TYPE_STATE_SET;
    } else if (class_id == qjs_metric_summary_class_id) {
        return METRIC_TYPE_SUMMARY;
    } else if (class_id == qjs_metric_gauge_histogram_class_id) {
        return METRIC_TYPE_GAUGE_HISTOGRAM;
    } else if (class_id == qjs_metric_histogram_class_id) {
        return METRIC_TYPE_HISTOGRAM;
    }

    return METRIC_TYPE_UNKNOWN;
}

metric_t *qjs_metric_get_metric(JSContext *ctx, JSValueConst v)
{
    JSClassID class_id = JS_GetClassID(v);

    metric_t *m = JS_GetOpaque2(ctx, v, class_id);
    if (m != NULL)
        return m;

    return NULL;
}

int qjs_metric_all_init(JSContext *ctx, JSModuleDef *m)
{
    qjs_metric_unknown_init(ctx, m);
    qjs_metric_gauge_init(ctx, m);
    qjs_metric_counter_init(ctx, m);
    qjs_metric_info_init(ctx, m);
    qjs_metric_state_set_init(ctx, m);
    qjs_metric_summary_init(ctx, m);
    qjs_metric_gauge_histogram_init(ctx, m);
    qjs_metric_histogram_init(ctx, m);
    return 0;
}
