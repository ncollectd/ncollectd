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

#include "libquickjs/quickjs.h"

JSValue qjs_array_get_length(JSContext *ctx, JSValueConst array, uint32_t *len)
{
    JSValue val = JS_GetPropertyStr(ctx, array, "length");
    if (JS_IsException(val))
        return JS_EXCEPTION;
    int ret = JS_ToUint32(ctx, len, val);
    JS_FreeValue(ctx, val);
    if (ret)
        return JS_EXCEPTION;
    return JS_UNDEFINED;
}

int qjs_to_cdtime(JSContext *ctx, cdtime_t *t, JSValueConst val)
{
    double d = 0;
    if (JS_ToFloat64(ctx, &d, val))
        return -1;

    if (!isfinite(d) || (d < 0) || (d >= (double)(UINT64_C(1) << 34))) {
        JS_ThrowRangeError(ctx, "invalid time or interval");
        return -1;
    }

    *t = DOUBLE_TO_CDTIME_T(d);
    return 0;
}

JSValue qjs_set_string(JSContext *ctx, char **str, JSValueConst val, bool nullable)
{
    char *dup = NULL;

    if (!nullable || (!JS_IsUndefined(val) && !JS_IsNull(val))) {
        const char *jstr = JS_ToCString(ctx, val);
        if (jstr == NULL)
            return JS_EXCEPTION;
        dup = strdup(jstr);
        JS_FreeCString(ctx, jstr);
        if (dup == NULL)
            return JS_ThrowOutOfMemory(ctx);
    }

    free(*str);
    *str = dup;

    return JS_UNDEFINED;
}

JSValue qjs_from_label_set(JSContext *ctx, const label_set_t *set)
{
    JSValue jset = JS_NewObject(ctx);
    if (JS_IsException(jset))
        return jset;

    for (size_t i = 0; i < set->num; i++) {
        char const *name = set->ptr[i].name;
        char const *value = set->ptr[i].value;
        JS_DefinePropertyValueStr(ctx, jset, name, JS_NewString(ctx, value), JS_PROP_C_W_E);
    }

    return jset;
}

JSValue qjs_to_label_set(JSContext *ctx, JSValueConst jset, label_set_t *set)
{
    if (!JS_IsObject(jset))
        return JS_ThrowTypeError(ctx, "label set must be an object");

    JSPropertyEnum *tab;
    uint32_t len;
    if (JS_GetOwnPropertyNames(ctx, &tab, &len, jset, JS_GPN_ENUM_ONLY | JS_GPN_STRING_MASK))
        return JS_ThrowTypeError(ctx, "cannot get property names");

    for(uint32_t i = 0; i < len; i++) {
        JSValue val = JS_GetProperty(ctx, jset, tab[i].atom);
        if (JS_IsException(val))
            goto fail;
        const char *value = JS_ToCString(ctx, val);
        JS_FreeValue(ctx, val);
        if (value == NULL)
            goto fail;
        const char *key = JS_AtomToCString(ctx, tab[i].atom);
        if (key == NULL) {
            JS_FreeCString(ctx, value);
            goto fail;
        }

        int status = label_set_add(set, true, key, value);
        if (status != 0)
            JS_ThrowTypeError(ctx, "invalid label '%s'", key);

        JS_FreeCString(ctx, key);
        JS_FreeCString(ctx, value);

        if (status != 0)
            goto fail;
    }

    JS_FreePropertyEnum(ctx, tab, len);
    return JS_UNDEFINED;

fail:
    JS_FreePropertyEnum(ctx, tab, len);
    return JS_EXCEPTION;
}

int label_set_to_string(strbuf_t *buf, label_set_t *set)
{
    int status = strbuf_putchar(buf, '{');

    for (size_t i = 0; i < set->num; i++) {
        label_pair_t *pair = &set->ptr[i];
        if (i != 0)
            status |= strbuf_putchar(buf, ',');
        status |= strbuf_putchar(buf, ' ');
        status |= strbuf_putstr(buf, pair->name);
        status |= strbuf_putstr(buf, ": \"");
        status |= strbuf_putescape_json(buf, pair->value);
        status |= strbuf_putchar(buf, '"');

    }

    status |= strbuf_putchar(buf, '}');

    return status;
}
