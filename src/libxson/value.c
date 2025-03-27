// SPDX-License-Identifier: GPL-2.0-only

#include "libutils/dtoa.h"
#include "libxson/common.h"
#include "libxson/value.h"

#include <stdbool.h>
#include <stdlib.h>

const char *xson_value_type_name(xson_value_t *val)
{
    switch (val->type) {
    case XSON_TYPE_STRING:
        return "string";
        break;
    case XSON_TYPE_NUMBER:
        return "number";
        break;
    case XSON_TYPE_OBJECT:
        return "object";
        break;
    case XSON_TYPE_ARRAY:
        return "array";
        break;
    case XSON_TYPE_TRUE:
        return "true";
        break;
    case XSON_TYPE_FALSE:
        return "false";
        break;
    case XSON_TYPE_NULL:
        return "null";
        break;
    }

    return "unknow";
}

static void xson_object_free (xson_value_t *v);
static void xson_array_free (xson_value_t *v);

static void xson_object_free (xson_value_t *v)
{
    if (v == NULL)
        return;

    if (v->type != XSON_TYPE_OBJECT)
        return;

    for (size_t i = 0; i < v->object.len; i++) {
        free(v->object.keyval[i].key);
        xson_value_t *nv = &v->object.keyval[i].value;
        switch (nv->type) {
        case XSON_TYPE_STRING:
            free(nv->string);
            break;
        case XSON_TYPE_NUMBER:
            break;
        case XSON_TYPE_OBJECT:
            xson_object_free(nv);
            break;
        case XSON_TYPE_ARRAY:
            xson_array_free(nv);
            break;
        case XSON_TYPE_TRUE:
            break;
        case XSON_TYPE_FALSE:
            break;
        case XSON_TYPE_NULL:
            break;
        }
    }
    free(v->object.keyval);
}

static void xson_array_free (xson_value_t *v)
{
    if (v == NULL)
        return;

    if (v->type != XSON_TYPE_ARRAY)
        return;

    for (size_t i = 0; i < v->array.len; i++) {
        xson_value_t *nv = &v->array.values[i];
        switch (nv->type) {
        case XSON_TYPE_STRING:
            free(nv->string);
            break;
        case XSON_TYPE_NUMBER:
            break;
        case XSON_TYPE_OBJECT:
            xson_object_free(nv);
            break;
        case XSON_TYPE_ARRAY:
            xson_array_free(nv);
            break;
        case XSON_TYPE_TRUE:
            break;
        case XSON_TYPE_FALSE:
            break;
        case XSON_TYPE_NULL:
            break;
        }
    }
    free(v->array.values);
}

void xson_value_free (xson_value_t *v)
{
    if (v == NULL)
        return;

    switch (v->type) {
    case XSON_TYPE_STRING:
        free(v->string);
        break;
    case XSON_TYPE_NUMBER:
        break;
    case XSON_TYPE_OBJECT:
        xson_object_free(v);
        break;
    case XSON_TYPE_ARRAY:
        xson_array_free(v);
        break;
    case XSON_TYPE_TRUE:
        break;
    case XSON_TYPE_FALSE:
        break;
    case XSON_TYPE_NULL:
        break;
    }

    free(v);
}

void xson_value_clear (xson_value_t *v)
{
    if (v == NULL)
        return;
    switch (v->type) {
    case XSON_TYPE_STRING:
        free(v->string);
        break;
    case XSON_TYPE_NUMBER:
        break;
    case XSON_TYPE_OBJECT:
        xson_object_free(v);
        break;
    case XSON_TYPE_ARRAY:
        xson_array_free(v);
        break;
    case XSON_TYPE_TRUE:
        break;
    case XSON_TYPE_FALSE:
        break;
    case XSON_TYPE_NULL:
        break;
    }
    v->type = XSON_TYPE_NULL;
}

xson_value_t *xson_value_array_append(xson_value_t *val)
{
    if (val == NULL)
        return NULL;
    if (val->type != XSON_TYPE_ARRAY)
        return NULL;

    xson_value_t *tmp = realloc(val->array.values, (val->array.len+1)*sizeof(val->array.values[0]));
    if (tmp == NULL)
        return NULL;

    val->array.values = tmp;
    xson_value_t *v = &val->array.values[val->array.len];
    val->array.len++;
    v->type = XSON_TYPE_NULL;

    return v;
}

xson_value_t *xson_value_object_append(xson_value_t *val, char *name)
{
    if (val == NULL)
        return NULL;
    if (val->type != XSON_TYPE_OBJECT)
        return NULL;

    char *dname = strdup(name);
    if (dname == NULL)
        return NULL;

    xson_keyval_t *tmp = realloc(val->object.keyval,
                                 (val->object.len+1)*sizeof(val->object.keyval[0]));
    if (tmp == NULL) {
        free(dname);
        return NULL;
    }

    val->object.keyval = tmp;
    val->object.keyval[val->object.len].key = dname;
    xson_value_t *v = &val->object.keyval[val->object.len].value;
    v->type = XSON_TYPE_NULL;
    val->object.len++;
    return val;

}

static int xson_value_clone_array(xson_value_t *src, xson_value_t *dst);
static int xson_value_clone_object(xson_value_t *src, xson_value_t *dst);

static int xson_value_clone_array(xson_value_t *src, xson_value_t *dst)
{
    dst->type = XSON_TYPE_ARRAY;
    dst->array.len = src->array.len;

    if (src->array.len > 0) {
        dst->array.values = calloc(src->array.len, sizeof(src->array.values[0]));
        if (dst->array.values == NULL)
            return -1;
    }

    for (size_t i=0 ; i < src->array.len ; i++) {
        xson_value_t *src_val = &src->array.values[i];
        xson_value_t *dst_val = &dst->array.values[i];
        switch (src_val->type) {
        case XSON_TYPE_STRING:
            xson_value_set_string(dst_val, src_val->string);
            break;
        case XSON_TYPE_NUMBER:
            xson_value_set_number(dst_val, src_val->number);
            break;
        case XSON_TYPE_OBJECT:
            xson_value_clone_object(src_val, dst_val);
            break;
        case XSON_TYPE_ARRAY:
            xson_value_clone_array(src_val, dst_val);
            break;
        case XSON_TYPE_TRUE:
            xson_value_set_true(dst_val);
            break;
        case XSON_TYPE_FALSE:
            xson_value_set_false(dst_val);
            break;
        case XSON_TYPE_NULL:
            xson_value_set_null(dst_val);
            break;
        }
    }
    return 0;
}

static int xson_value_clone_object(xson_value_t *src, xson_value_t *dst)
{
    dst->type = XSON_TYPE_OBJECT;
    dst->object.len = src->object.len;

    if (src->object.len > 0) {
        dst->object.keyval = calloc(src->object.len, sizeof(src->object.keyval[0]));
        if (dst->object.keyval == NULL)
            return -1;
    }

    for (size_t i=0 ; i < src->object.len ; i++) {
        dst->object.keyval[i].key = strdup(src->object.keyval[i].key);
        if (dst->object.keyval[i].key == NULL)
            continue;
        xson_value_t *src_val = &src->object.keyval[i].value;
        xson_value_t *dst_val = &dst->object.keyval[i].value;
        switch (src_val->type) {
        case XSON_TYPE_STRING:
            xson_value_set_string(dst_val, src_val->string);
            break;
        case XSON_TYPE_NUMBER:
            xson_value_set_number(dst_val, src_val->number);
            break;
        case XSON_TYPE_OBJECT:
            xson_value_clone_object(src_val, dst_val);
            break;
        case XSON_TYPE_ARRAY:
            xson_value_clone_array(src_val, dst_val);
            break;
        case XSON_TYPE_TRUE:
            xson_value_set_true(dst_val);
            break;
        case XSON_TYPE_FALSE:
            xson_value_set_false(dst_val);
            break;
        case XSON_TYPE_NULL:
            xson_value_set_null(dst_val);
            break;
        }
    }
    return 0;
}

xson_value_t *xson_value_clone(xson_value_t *src)
{
    if (src == NULL)
        return NULL;

    xson_value_t *dst = calloc(1, sizeof(*dst));
    if (dst == NULL)
        return NULL;

    switch (src->type) {
    case XSON_TYPE_STRING:
        xson_value_set_string(dst, src->string);
        break;
    case XSON_TYPE_NUMBER:
        xson_value_set_number(dst, src->number);
        break;
    case XSON_TYPE_OBJECT:
        xson_value_set_object(dst);
        xson_value_clone_object(src, dst);
        break;
    case XSON_TYPE_ARRAY:
        xson_value_set_array(dst);
        xson_value_clone_array(src, dst);
        break;
    case XSON_TYPE_TRUE:
        xson_value_set_true(dst);
        break;
    case XSON_TYPE_FALSE:
        xson_value_set_false(dst);
        break;
    case XSON_TYPE_NULL:
        xson_value_set_null(dst);
        break;
    }

    return dst;
}

int xson_value_to_number(xson_value_t *val)
{
    switch(val->type) {
    case XSON_TYPE_NULL:
        val->type = XSON_TYPE_NUMBER;
        val->number = 0.0; // NAN
        break;
    case XSON_TYPE_STRING:
// FIXME
        break;
    case XSON_TYPE_NUMBER:
        break;
    case XSON_TYPE_TRUE:
        val->type = XSON_TYPE_NUMBER;
        val->number = 1.0;
        break;
    case XSON_TYPE_FALSE:
        val->type = XSON_TYPE_NUMBER;
        val->number = 0.0;
        break;
    default:
        return -1;
        break;
    }
    return 0;
}

int xson_value_to_string(xson_value_t *val)
{
    char *str = NULL;

    switch(val->type) {
    case XSON_TYPE_NULL:
        str = strdup("null");
        if (str == NULL) {
            return -1;
        }
        val->type = XSON_TYPE_STRING;
        val->string = str;
        break;
    case XSON_TYPE_STRING:
        break;
    case XSON_TYPE_NUMBER:
//FIXME
        break;
    case XSON_TYPE_TRUE:
        str = strdup("true");
        if (str == NULL) {
            return -1;
        }
        val->type = XSON_TYPE_STRING;
        val->string = str;
        break;
    case XSON_TYPE_FALSE:
        str = strdup("false");
        if (str == NULL) {
            return -1;
        }
        val->type = XSON_TYPE_STRING;
        val->string = str;
        break;
    default:
        return -1;
        break;
    }
    return 0;
}

int xson_value_to_boolean(xson_value_t *val)
{
    switch(val->type) {
    case XSON_TYPE_NULL:
        val->type = XSON_TYPE_FALSE;
        break;
    case XSON_TYPE_STRING:
        if (val->string[0] != '\0') {
            val->type = XSON_TYPE_TRUE;
        } else {
            val->type = XSON_TYPE_FALSE;
        }
        free(val->string);
        break;
    case XSON_TYPE_NUMBER:
        if (val->number != 0.0)
            val->type = XSON_TYPE_TRUE;
        else
            val->type = XSON_TYPE_FALSE;
        break;
    case XSON_TYPE_TRUE:
        break;
    case XSON_TYPE_FALSE:
        break;
    default:
        return -1;
        break;
    }
    return 0;
}

#if 0
static int xson_value_as_double(const xson_value_t *val, double *number)
{
    switch(val->type) {
    case XSON_TYPE_NULL:
        *number = 0.0;
        break;
    case XSON_TYPE_STRING:
        *number = atof(val->string);
        break;
    case XSON_TYPE_NUMBER:
        *number = val->number;
        break;
    case XSON_TYPE_TRUE:
        *number = 1.0;
        break;
    case XSON_TYPE_FALSE:
        *number = 0.0;
        break;
    default:
        return -1;
        break;
    }
    return 0;
}

static char *xson_value_as_string(const xson_value_t *val, char *str, size_t size)
{
    switch(val->type) {
    case XSON_TYPE_NULL:
        return "";
        break;
    case XSON_TYPE_STRING:
        return val->string;
        break;
    case XSON_TYPE_NUMBER:
        size = dtoa(val->number, str, size);
        str[size] = '\0';
        return NULL;
        break;
    case XSON_TYPE_TRUE:
        return "true";
        break;
    case XSON_TYPE_FALSE:
        return "false";
        break;
    default:
        return "";
        break;
    }
    return "";
}

static bool xson_canbe_number(const xson_value_t *val)
{
    switch(val->type) {
    case XSON_TYPE_NULL:
        return true;
        break;
    case XSON_TYPE_STRING:
        // FIXME
        return true;
        break;
    case XSON_TYPE_NUMBER:
        return true;
        break;
    case XSON_TYPE_OBJECT:
        return false;
        break;
    case XSON_TYPE_ARRAY:
        return false;
        break;
    case XSON_TYPE_TRUE:
        return true;
        break;
    case XSON_TYPE_FALSE:
        return true;
        break;
    }
    return false;
}
#endif

int xson_value_cmp(const xson_value_t *val1, const xson_value_t *val2)
{
    if (val1->type != val2->type)
        return (int)val1->type - (int)val2->type;

    switch(val1->type) {
    case XSON_TYPE_NULL:
        return 0;
    case XSON_TYPE_STRING:
        return strcmp(val1->string, val2->string);
    case XSON_TYPE_NUMBER:
        return val1->number - val2->number;
    case XSON_TYPE_OBJECT:
        if (val1->object.len != val2->object.len)
            return (int)val1->object.len - (int)val2->object.len;
        for (size_t i = 0; i < val1->object.len; i++) {
            xson_keyval_t *kv1 = &val1->object.keyval[i];
            xson_keyval_t *kv2 = &val2->object.keyval[i];
            int cmp = strcmp(kv1->key, kv2->key);
            if (cmp != 0)
                return cmp;
            cmp = xson_value_cmp(&kv1->value, &kv1->value);
            if (cmp != 0)
                return cmp;
        }
        return false;
    case XSON_TYPE_ARRAY:
        if (val1->array.len != val2->array.len)
            return (int)val1->array.len - (int)val2->array.len;
        for (size_t i = 0; i < val1->array.len; i++) {
            if (!xson_value_cmp(&val1->array.values[i], &val2->array.values[i]))
                return 0;
        }
        return false;
    case XSON_TYPE_TRUE:
        return 0;
    case XSON_TYPE_FALSE:
        return 0;
    }
#if 0
    if (((val1->type != XSON_TYPE_STRING) || xson_canbe_number(val1)) &&
        ((val2->type != XSON_TYPE_STRING) || xson_canbe_number(val1))) {
        double dval1 = 0.0;
        xson_value_as_double(val1, &dval1);
        double dval2 = 0.0;
        xson_value_as_double(val2, &dval2);

    } else if (val1->type == XSON_TYPE_STRING) {
        char buf[DTOA_MAX+1];
        return strcmp(val1->string, xson_value_as_string(val2, buf, sizeof(buf)));
    } else if (val2->type == XSON_TYPE_STRING) {
        char buf[DTOA_MAX+1];
        return strcmp(xson_value_as_string(val1, buf, sizeof(buf)), val2->string);
    }
#endif
    return 1;
}
