// SPDX-License-Identifier: GPL-2.0-only

#include "libutils/dtoa.h"
#include "libxson/common.h"
#include "libxson/value.h"

#include <stdbool.h>
#include <stdlib.h>

const char *json_value_type_name(json_value_t *val)
{
    switch (val->type) {
    case JSON_TYPE_STRING:
        return "string";
        break;
    case JSON_TYPE_NUMBER:
        return "number";
        break;
    case JSON_TYPE_OBJECT:
        return "object";
        break;
    case JSON_TYPE_ARRAY:
        return "array";
        break;
    case JSON_TYPE_TRUE:
        return "true";
        break;
    case JSON_TYPE_FALSE:
        return "false";
        break;
    case JSON_TYPE_NULL:
        return "null";
        break;
    }

    return "unknow";
}

static void json_object_free (json_value_t *v);
static void json_array_free (json_value_t *v);

static void json_object_free (json_value_t *v)
{
    if (v == NULL)
        return;

    if (v->type != JSON_TYPE_OBJECT)
        return;

    for (size_t i = 0; i < v->object.len; i++) {
        free(v->object.keyval[i].key);
        json_value_t *nv = &v->object.keyval[i].value;
        switch (nv->type) {
        case JSON_TYPE_STRING:
            free(nv->string);
            break;
        case JSON_TYPE_NUMBER:
            break;
        case JSON_TYPE_OBJECT:
            json_object_free(nv);
            break;
        case JSON_TYPE_ARRAY:
            json_array_free(nv);
            break;
        case JSON_TYPE_TRUE:
            break;
        case JSON_TYPE_FALSE:
            break;
        case JSON_TYPE_NULL:
            break;
        }
    }
    free(v->object.keyval);
}

static void json_array_free (json_value_t *v)
{
    if (v == NULL)
        return;

    if (v->type != JSON_TYPE_ARRAY)
        return;

    for (size_t i = 0; i < v->array.len; i++) {
        json_value_t *nv = &v->array.values[i];
        switch (nv->type) {
        case JSON_TYPE_STRING:
            free(nv->string);
            break;
        case JSON_TYPE_NUMBER:
            break;
        case JSON_TYPE_OBJECT:
            json_object_free(nv);
            break;
        case JSON_TYPE_ARRAY:
            json_array_free(nv);
            break;
        case JSON_TYPE_TRUE:
            break;
        case JSON_TYPE_FALSE:
            break;
        case JSON_TYPE_NULL:
            break;
        }
    }
    free(v->array.values);
}

void json_value_free (json_value_t *v)
{
    if (v == NULL)
        return;

    switch (v->type) {
    case JSON_TYPE_STRING:
        free(v->string);
        break;
    case JSON_TYPE_NUMBER:
        break;
    case JSON_TYPE_OBJECT:
        json_object_free(v);
        break;
    case JSON_TYPE_ARRAY:
        json_array_free(v);
        break;
    case JSON_TYPE_TRUE:
        break;
    case JSON_TYPE_FALSE:
        break;
    case JSON_TYPE_NULL:
        break;
    }

    free(v);
}

void json_value_clear (json_value_t *v)
{
    if (v == NULL)
        return;
    switch (v->type) {
    case JSON_TYPE_STRING:
        free(v->string);
        break;
    case JSON_TYPE_NUMBER:
        break;
    case JSON_TYPE_OBJECT:
        json_object_free(v);
        break;
    case JSON_TYPE_ARRAY:
        json_array_free(v);
        break;
    case JSON_TYPE_TRUE:
        break;
    case JSON_TYPE_FALSE:
        break;
    case JSON_TYPE_NULL:
        break;
    }
    v->type = JSON_TYPE_NULL;
}

json_value_t *json_value_array_append(json_value_t *val)
{
    if (val == NULL)
        return NULL;
    if (val->type != JSON_TYPE_ARRAY)
        return NULL;

    json_value_t *tmp = realloc(val->array.values, (val->array.len+1)*sizeof(val->array.values[0]));
    if (tmp == NULL)
        return NULL;

    val->array.values = tmp;
    json_value_t *v = &val->array.values[val->array.len];
    val->array.len++;
    v->type = JSON_TYPE_NULL;

    return v;
}

json_value_t *json_value_object_append(json_value_t *val, char *name)
{
    if (val == NULL)
        return NULL;
    if (val->type != JSON_TYPE_OBJECT)
        return NULL;

    char *dname = strdup(name);
    if (dname == NULL)
        return NULL;

    json_keyval_t *tmp = realloc(val->object.keyval, (val->object.len+1)*sizeof(val->object.keyval[0]));
    if (tmp == NULL) {
        free(dname);
        return NULL;
    }

    val->object.keyval = tmp;
    val->object.keyval[val->object.len].key = dname;
    json_value_t *v = &val->object.keyval[val->object.len].value;
    v->type = JSON_TYPE_NULL;
    val->object.len++;
    return val;

}

static int json_value_clone_array(json_value_t *src, json_value_t *dst);
static int json_value_clone_object(json_value_t *src, json_value_t *dst);

static int json_value_clone_array(json_value_t *src, json_value_t *dst)
{
    dst->type = JSON_TYPE_ARRAY;
    dst->array.len = src->array.len;

    if (src->array.len > 0) {
        dst->array.values = calloc(src->array.len, sizeof(src->array.values[0]));
        if (dst->array.values == NULL)
            return -1;
    }

    for (size_t i=0 ; i < src->array.len ; i++) {
        json_value_t *src_val = &src->array.values[i];
        json_value_t *dst_val = &dst->array.values[i];
        switch (src_val->type) {
        case JSON_TYPE_STRING:
            json_value_set_string(dst_val, src_val->string);
            break;
        case JSON_TYPE_NUMBER:
            json_value_set_number(dst_val, src_val->number);
            break;
        case JSON_TYPE_OBJECT:
            json_value_clone_object(src_val, dst_val);
            break;
        case JSON_TYPE_ARRAY:
            json_value_clone_array(src_val, dst_val);
            break;
        case JSON_TYPE_TRUE:
            json_value_set_true(dst_val);
            break;
        case JSON_TYPE_FALSE:
            json_value_set_false(dst_val);
            break;
        case JSON_TYPE_NULL:
            json_value_set_null(dst_val);
            break;
        }
    }
    return 0;
}

static int json_value_clone_object(json_value_t *src, json_value_t *dst)
{
    dst->type = JSON_TYPE_OBJECT;
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
        json_value_t *src_val = &src->object.keyval[i].value;
        json_value_t *dst_val = &dst->object.keyval[i].value;
        switch (src_val->type) {
        case JSON_TYPE_STRING:
            json_value_set_string(dst_val, src_val->string);
            break;
        case JSON_TYPE_NUMBER:
            json_value_set_number(dst_val, src_val->number);
            break;
        case JSON_TYPE_OBJECT:
            json_value_clone_object(src_val, dst_val);
            break;
        case JSON_TYPE_ARRAY:
            json_value_clone_array(src_val, dst_val);
            break;
        case JSON_TYPE_TRUE:
            json_value_set_true(dst_val);
            break;
        case JSON_TYPE_FALSE:
            json_value_set_false(dst_val);
            break;
        case JSON_TYPE_NULL:
            json_value_set_null(dst_val);
            break;
        }
    }
    return 0;
}

json_value_t *json_value_clone(json_value_t *src)
{
    if (src == NULL)
        return NULL;

    json_value_t *dst = calloc(1, sizeof(*dst));
    if (dst == NULL)
        return NULL;

    switch (src->type) {
    case JSON_TYPE_STRING:
        json_value_set_string(dst, src->string);
        break;
    case JSON_TYPE_NUMBER:
        json_value_set_number(dst, src->number);
        break;
    case JSON_TYPE_OBJECT:
        json_value_set_object(dst);
        json_value_clone_object(src, dst);
        break;
    case JSON_TYPE_ARRAY:
        json_value_set_array(dst);
        json_value_clone_array(src, dst);
        break;
    case JSON_TYPE_TRUE:
        json_value_set_true(dst);
        break;
    case JSON_TYPE_FALSE:
        json_value_set_false(dst);
        break;
    case JSON_TYPE_NULL:
        json_value_set_null(dst);
        break;
    }

    return dst;
}

int json_value_to_number(json_value_t *val)
{
    switch(val->type) {
    case JSON_TYPE_NULL:
        val->type = JSON_TYPE_NUMBER;
        val->number = 0.0; // NAN
        break;
    case JSON_TYPE_STRING:
// FIXME
        break;
    case JSON_TYPE_NUMBER:
        break;
    case JSON_TYPE_TRUE:
        val->type = JSON_TYPE_NUMBER;
        val->number = 1.0;
        break;
    case JSON_TYPE_FALSE:
        val->type = JSON_TYPE_NUMBER;
        val->number = 0.0;
        break;
    default:
        return -1;
        break;
    }
    return 0;
}

int json_value_to_string(json_value_t *val)
{
    char *str = NULL;

    switch(val->type) {
    case JSON_TYPE_NULL:
        str = strdup("null");
        if (str == NULL) {
            return -1;
        }
        val->type = JSON_TYPE_STRING;
        val->string = str;
        break;
    case JSON_TYPE_STRING:
        break;
    case JSON_TYPE_NUMBER:
//FIXME
        break;
    case JSON_TYPE_TRUE:
        str = strdup("true");
        if (str == NULL) {
            return -1;
        }
        val->type = JSON_TYPE_STRING;
        val->string = str;
        break;
    case JSON_TYPE_FALSE:
        str = strdup("false");
        if (str == NULL) {
            return -1;
        }
        val->type = JSON_TYPE_STRING;
        val->string = str;
        break;
    default:
        return -1;
        break;
    }
    return 0;
}

int json_value_to_boolean(json_value_t *val)
{
    switch(val->type) {
    case JSON_TYPE_NULL:
        val->type = JSON_TYPE_FALSE;
        break;
    case JSON_TYPE_STRING:
        if (val->string[0] != '\0') {
            val->type = JSON_TYPE_TRUE;
        } else {
            val->type = JSON_TYPE_FALSE;
        }
        free(val->string);
        break;
    case JSON_TYPE_NUMBER:
        if (val->number != 0.0)
            val->type = JSON_TYPE_TRUE;
        else
            val->type = JSON_TYPE_FALSE;
        break;
    case JSON_TYPE_TRUE:
        break;
    case JSON_TYPE_FALSE:
        break;
    default:
        return -1;
        break;
    }
    return 0;
}

static int json_value_as_double(const json_value_t *val, double *number)
{
    switch(val->type) {
    case JSON_TYPE_NULL:
        *number = 0.0;
        break;
    case JSON_TYPE_STRING:
        *number = atof(val->string);
        break;
    case JSON_TYPE_NUMBER:
        *number = val->number;
        break;
    case JSON_TYPE_TRUE:
        *number = 1.0;
        break;
    case JSON_TYPE_FALSE:
        *number = 0.0;
        break;
    default:
        return -1;
        break;
    }
    return 0;
}

static char *json_value_as_string(const json_value_t *val, char *str, size_t size)
{
    switch(val->type) {
    case JSON_TYPE_NULL:
        return "";
        break;
    case JSON_TYPE_STRING:
        return val->string;
        break;
    case JSON_TYPE_NUMBER:
        size = dtoa(val->number, str, size);
        str[size] = '\0';
        return NULL;
        break;
    case JSON_TYPE_TRUE:
        return "true";
        break;
    case JSON_TYPE_FALSE:
        return "false";
        break;
    default:
        return "";
        break;
    }
    return "";
}

static bool json_canbe_number(const json_value_t *val)
{
    switch(val->type) {
    case JSON_TYPE_NULL:
        return true;
        break;
    case JSON_TYPE_STRING:
        // FIXME
        return true;
        break;
    case JSON_TYPE_NUMBER:
        return true;
        break;
    case JSON_TYPE_OBJECT:
        return false;
        break;
    case JSON_TYPE_ARRAY:
        return false;
        break;
    case JSON_TYPE_TRUE:
        return true;
        break;
    case JSON_TYPE_FALSE:
        return true;
        break;
    }
    return false;
}

int json_value_cmp(const json_value_t *val1, const json_value_t *val2)
{
    if (((val1->type != JSON_TYPE_STRING) || json_canbe_number(val1)) &&
        ((val2->type != JSON_TYPE_STRING) || json_canbe_number(val1))) {
        double dval1 = 0.0;
        json_value_as_double(val1, &dval1);
        double dval2 = 0.0;
        json_value_as_double(val2, &dval2);

    } else if (val1->type == JSON_TYPE_STRING) {
        char buf[DTOA_MAX+1];
        return strcmp(val1->string, json_value_as_string(val2, buf, sizeof(buf)));
    } else if (val2->type == JSON_TYPE_STRING) {
        char buf[DTOA_MAX+1];
        return strcmp(json_value_as_string(val1, buf, sizeof(buf)), val2->string);
    }
    return -1;
}
