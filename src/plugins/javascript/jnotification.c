// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"
#include "libmetric/notification.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wpedantic"
#if __GNUC__ >= 8
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

#undef func_data

#include "libquickjs/cutils.h"
#include "libquickjs/quickjs-libc.h"

#include "jutil.h"

static JSClassID qjs_notification_class_id;

enum {
    NOTIF_GETSET_SEVERITY,
    NOTIF_GETSET_TIME,
    NOTIF_GETSET_NAME,
    NOTIF_GETSET_LABELS,
    NOTIF_GETSET_ANNOTATIONS
};

static void qjs_notification_finalizer(JSRuntime *rt, JSValue val)
{
    notification_t *n = JS_GetOpaque(val, qjs_notification_class_id);
    notification_free(n);
}

static JSValue qjs_notification_ctor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv)
{
    JSValue obj = JS_UNDEFINED;

    notification_t *n = calloc(1, sizeof(*n));
    if (n == NULL)
        return JS_EXCEPTION;

    n->severity = NOTIF_FAILURE;
    n->time = cdtime();

    if (!JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        const char *name = JS_ToCString(ctx, argv[0]);
        if (name == NULL)
            goto fail;
        n->name = strdup(name);
        JS_FreeCString(ctx, name);
    }

    if (!JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        int32_t severity = 0;
        if (JS_ToInt32(ctx, &severity, argv[1]))
            goto fail;
        if ((severity != NOTIF_FAILURE) && (severity != NOTIF_WARNING) && (severity != NOTIF_OKAY))
            goto fail;
        n->severity = severity;
    }

    if (!JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2])) {
        double ntime = 0;
        if (JS_ToFloat64(ctx, &ntime, argv[2]))
            goto fail;
        n->time = DOUBLE_TO_CDTIME_T(ntime);
    }

    if (!JS_IsUndefined(argv[3]) && !JS_IsNull(argv[3])) {
        JSValue ret = qjs_to_label_set(ctx, argv[3], &n->label);
        if (JS_IsException(ret))
            goto fail;
    }

    if (!JS_IsUndefined(argv[4]) && !JS_IsNull(argv[4])) {
        JSValue ret = qjs_to_label_set(ctx, argv[4], &n->annotation);
        if (JS_IsException(ret))
            goto fail;
    }

    /* using new_target to get the prototype is necessary when the class is extended. */
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, qjs_notification_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;
    JS_SetOpaque(obj, n);
    return obj;

 fail:
    notification_free(n);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue qjs_notification_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    notification_t *n = JS_GetOpaque2(ctx, this_val, qjs_notification_class_id);
    if (n == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case NOTIF_GETSET_SEVERITY:
        return JS_NewInt32(ctx, n->severity);
        break;
    case NOTIF_GETSET_TIME:
        return JS_NewFloat64(ctx, CDTIME_T_TO_DOUBLE(n->time));
        break;
    case NOTIF_GETSET_NAME:
        return JS_NewString(ctx, n->name);
        break;
    case NOTIF_GETSET_LABELS:
        return qjs_from_label_set(ctx, &n->label);
        break;
    case NOTIF_GETSET_ANNOTATIONS:
        return qjs_from_label_set(ctx, &n->annotation);
        break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_notification_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    notification_t *n = JS_GetOpaque2(ctx, this_val, qjs_notification_class_id);
    if (n == NULL)
        return JS_EXCEPTION;

    switch (magic) {
    case NOTIF_GETSET_SEVERITY: {
        int32_t severity = 0;
        if (JS_ToInt32(ctx, &severity, val))
            return JS_EXCEPTION;
        if ((severity != NOTIF_FAILURE) && (severity != NOTIF_WARNING) && (severity != NOTIF_OKAY))
            return JS_EXCEPTION;
        n->severity = severity;
    }   break;
    case NOTIF_GETSET_TIME: {
        double ntime = 0;
        if (JS_ToFloat64(ctx, &ntime, val))
            return JS_EXCEPTION;
        n->time = DOUBLE_TO_CDTIME_T(ntime);
    }   break;
    case NOTIF_GETSET_NAME: {
        const char *name = JS_ToCString(ctx, val);
        if (name == NULL)
            return JS_EXCEPTION;
        n->name = strdup(name);
        JS_FreeCString(ctx, name);
    }   break;
    case NOTIF_GETSET_LABELS:
        label_set_reset(&n->label);
        return qjs_to_label_set(ctx, val, &n->label);
        break;
    case NOTIF_GETSET_ANNOTATIONS:
        label_set_reset(&n->annotation);
        return qjs_to_label_set(ctx, val, &n->annotation);
        break;
    }

    return JS_UNDEFINED;
}

static JSValue qjs_notification_add_label(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    notification_t *n = JS_GetOpaque2(ctx, this_val, qjs_notification_class_id);
    if (n == NULL)
        return JS_EXCEPTION;

    const char *name = JS_ToCString(ctx, argv[0]);
    if (name == NULL)
        return JS_EXCEPTION;

    const char *value = JS_ToCString(ctx, argv[1]);
    if (value == NULL) {
        JS_FreeCString(ctx, name);
        return JS_EXCEPTION;
    }

    notification_label_set(n, name, value);

    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, value);

    return JS_UNDEFINED;
}

static JSValue qjs_notification_add_annotation(JSContext *ctx, JSValueConst this_val,
                                              int argc, JSValueConst *argv)
{
    notification_t *n = JS_GetOpaque2(ctx, this_val, qjs_notification_class_id);
    if (n == NULL)
        return JS_EXCEPTION;

    const char *name = JS_ToCString(ctx, argv[0]);
    if (name == NULL)
        return JS_EXCEPTION;

    const char *value = JS_ToCString(ctx, argv[1]);
    if (value == NULL) {
        JS_FreeCString(ctx, name);
        return JS_EXCEPTION;
    }

    notification_annotation_set(n, name, value);

    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, value);

    return JS_UNDEFINED;
}

static JSValue qjs_notification_dispatch(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    notification_t *n = JS_GetOpaque2(ctx, this_val, qjs_notification_class_id);
    if (n == NULL)
        return JS_EXCEPTION;

    plugin_dispatch_notification(n);

    return JS_UNDEFINED;
}

static JSValue qjs_notification_to_string(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    notification_t *n = JS_GetOpaque2(ctx, this_val, qjs_notification_class_id);
    if (n == NULL)
        return JS_EXCEPTION;

    strbuf_t buf = STRBUF_CREATE;

    int status = strbuf_putstr(&buf, "{ name: \"");
    status |= strbuf_putescape_json(&buf, n->name);
    status |= strbuf_putstr(&buf, "\", severity: ");
    switch(n->severity) {
    case NOTIF_FAILURE:
        status |= strbuf_putstr(&buf, "Notification.FAILURE");
        break;
    case NOTIF_WARNING:
        status |= strbuf_putstr(&buf, "Notification.WARNING");
        break;
    case NOTIF_OKAY:
        status |= strbuf_putstr(&buf, "Notification.OKAY");
        break;
    }
    status |= strbuf_putstr(&buf, ", time: ");
    status |= strbuf_putdouble(&buf, CDTIME_T_TO_DOUBLE(n->time));
    status |= strbuf_putstr(&buf, ", labels: {");
    for (size_t i = 0; i < n->label.num; i++) {
        label_pair_t *pair = &n->label.ptr[i];
        if (i != 0)
            status |= strbuf_putchar(&buf, ',');
        status |= strbuf_putchar(&buf, ' ');
        status |= strbuf_putstr(&buf, pair->name);
        status |= strbuf_putstr(&buf, ": \"");
        status |= strbuf_putescape_json(&buf, pair->value);
        status |= strbuf_putchar(&buf, '"');

    }
    status |= strbuf_putstr(&buf, " }, annotations: {");
    for (size_t i = 0; i < n->annotation.num; i++) {
        label_pair_t *pair = &n->annotation.ptr[i];
        if (i != 0)
            status |= strbuf_putchar(&buf, ',');
        status |= strbuf_putchar(&buf, ' ');
        status |= strbuf_putstr(&buf, pair->name);
        status |= strbuf_putstr(&buf, ": \"");
        status |= strbuf_putescape_json(&buf, pair->value);
        status |= strbuf_putchar(&buf, '"');
    }
    status |= strbuf_putstr(&buf, " } }");

    if (status != 0) {
        strbuf_destroy(&buf);
        return JS_EXCEPTION;
    }

    JSValue string = JS_NewStringLen(ctx, buf.ptr, strbuf_len(&buf));

    strbuf_destroy(&buf);

    return string;
}

static JSClassDef qjs_notification_class = {
    "Notification",
    .finalizer = qjs_notification_finalizer,
};

static const JSCFunctionListEntry qjs_notification_proto_funcs[] = {
    JS_PROP_INT32_DEF("FAILURE", NOTIF_FAILURE, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("WARNING", NOTIF_WARNING, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("OKAY", NOTIF_OKAY, JS_PROP_CONFIGURABLE),
    JS_CGETSET_MAGIC_DEF("severity", qjs_notification_get, qjs_notification_set,
                                     NOTIF_GETSET_SEVERITY),
    JS_CGETSET_MAGIC_DEF("time", qjs_notification_get, qjs_notification_set, NOTIF_GETSET_TIME),
    JS_CGETSET_MAGIC_DEF("name", qjs_notification_get, qjs_notification_set, NOTIF_GETSET_NAME),
    JS_CGETSET_MAGIC_DEF("labels", qjs_notification_get, qjs_notification_set, NOTIF_GETSET_LABELS),
    JS_CGETSET_MAGIC_DEF("annotations", qjs_notification_get, qjs_notification_set,
                                        NOTIF_GETSET_ANNOTATIONS),
    JS_CFUNC_DEF("add_label", 2, qjs_notification_add_label),
    JS_CFUNC_DEF("add_annotation", 2, qjs_notification_add_annotation),
    JS_CFUNC_DEF("dispatch", 0, qjs_notification_dispatch),
    JS_CFUNC_DEF("toString", 0, qjs_notification_to_string),
};

int qjs_notification_init(JSContext *ctx, JSModuleDef *m)
{
    JS_NewClassID(&qjs_notification_class_id);
    JS_NewClass(JS_GetRuntime(ctx), qjs_notification_class_id, &qjs_notification_class);

    JSValue notification_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, notification_proto, qjs_notification_proto_funcs,
                                    countof(qjs_notification_proto_funcs));

    JSValue notification_class = JS_NewCFunction2(ctx, qjs_notification_ctor,
                                                       "Notification", 5,
                                                       JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, notification_class, notification_proto);
    JS_SetClassProto(ctx, qjs_notification_class_id, notification_proto);

    JS_SetModuleExport(ctx, m, "Notification", notification_class);

    return 0;
}

JSValue qjs_notification_new(JSContext *ctx, const notification_t *n)
{
    JSValue obj = JS_NewObjectClass(ctx, qjs_notification_class_id);
    if (JS_IsException(obj))
        return obj;

    notification_t *ndup = notification_clone(n);
    if (ndup == NULL) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }

    JS_SetOpaque(obj, ndup);

    return obj;
}
