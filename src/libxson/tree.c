// SPDX-License-Identifier: GPL-2.0-only OR MIT
// Copyright (c) 2010-2011  Florian Forster  <ff at octo.it>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "libutils/common.h"
#include "libutils/dtoa.h"
#include "libxson/json_parse.h"
#include "libxson/value.h"
#include "libxson/render.h"

typedef struct {
    xson_value_t *value;
} xson_stack_t;

typedef struct {
    xson_value_t *root;
    xson_stack_t stack[JSON_MAX_DEPTH];
    int depth;
    char *errbuf;
    size_t errbuf_size;
} context_t;

#define JSON_ERROR(ctx,...)                                             \
    do {                                                                \
        if ((ctx)->errbuf != NULL)                                      \
            snprintf ((ctx)->errbuf, (ctx)->errbuf_size, __VA_ARGS__);  \
    } while(0)

static int context_push(context_t *ctx, xson_value_t *v)
{
    if ((ctx->depth + 1) >= JSON_MAX_DEPTH) {
        JSON_ERROR (ctx, "Out of memory");
        return ENOMEM;
    }
    ctx->depth++;
    ctx->stack[ctx->depth].value = v;

    return 0;
}

static xson_value_t *context_pop(context_t *ctx)
{
    if (ctx->depth == 0) {
        JSON_ERROR (ctx, "context_pop: " "Bottom of stack reached prematurely");
        return NULL;
    }
    ctx->depth--;
    xson_value_t *v = ctx->stack[ctx->depth].value;
//    ctx->stack[ctx->depth].value = NULL;
    return v;
}

static xson_value_t *context_peek(context_t *ctx, bool key)
{
    xson_value_t *sval = ctx->stack[ctx->depth].value;
    if (sval == NULL)
        return NULL;

    if (sval->type == XSON_TYPE_OBJECT) {
        if (key)
            return sval;
        return &sval->object.keyval[sval->object.len-1].value;
    } else if (sval->type == XSON_TYPE_ARRAY) {
        xson_value_t *tmp = realloc(sval->array.values,
                                  sizeof(*sval->array.values) * (sval->array.len + 1));
        if (tmp == NULL) {
            JSON_ERROR ((context_t *) ctx, "Out of memory");
            return NULL;
        }
        sval->array.values = tmp;
        xson_value_t *jval = &sval->array.values[sval->array.len];
        jval->type = XSON_TYPE_NULL;
        sval->array.len++;
        return jval;
    } else if (sval->type == XSON_TYPE_NULL) {
        return sval;
    }

    return NULL;
}

static bool handle_string (void *ctx, const char *str, size_t str_len)
{
    char *nstr = malloc (str_len + 1);
    if (nstr == NULL) {
        JSON_ERROR ((context_t *) ctx, "Out of memory");
        return false;
    }
    if (str_len > 0)
        memcpy(nstr, str, str_len);
    nstr[str_len] = 0;

    xson_value_t *jval = context_peek(ctx, false);
    if (jval == NULL) {
        free(nstr);
        return false;
    }

    jval->type = XSON_TYPE_STRING;
    jval->string = nstr;
    return true;
}

static bool handle_number(void * ctx, const char * number_val, size_t number_len)
{
    xson_value_t *jval = context_peek(ctx, false);
    if (jval == NULL)
        return false;
    char num[64];
    sstrnncpy(num, sizeof(num), number_val, number_len);
    jval->type = XSON_TYPE_NUMBER;
    jval->number = atof(num); // FIXME
    return true;
}

static bool handle_start_map (void *ctx)
{
    xson_value_t *jval = context_peek(ctx, false);
    if (jval == NULL)
        return false;
    jval->type = XSON_TYPE_OBJECT;
    jval->object.keyval = NULL;
    jval->object.len = 0;

    return context_push (ctx, jval) == 0 ? true : false;
}

static bool handle_map_key(void * ctx, const char *key, size_t size)
{
    char *str_key = malloc (size + 1);
    if (str_key == NULL) {
        free(str_key);
        JSON_ERROR ((context_t *) ctx, "Out of memory");
        return false;
    }
    if (size > 0)
        memcpy(str_key, key, size);
    str_key[size] = 0;

    xson_value_t *jval = context_peek(ctx, true); // FIXME
    if (jval == NULL) {
        free(str_key);
        return false;
    }

    xson_keyval_t *tmp = realloc(jval->object.keyval,
                                 sizeof(*jval->object.keyval) * (jval->object.len + 1));
    if (tmp == NULL) {
        free(str_key);
        JSON_ERROR ((context_t *) ctx, "Out of memory");
        return false;
    }
    jval->object.keyval = tmp;

    jval->object.keyval[jval->object.len].key = str_key;
    jval->object.keyval[jval->object.len].value.type = XSON_TYPE_NULL;
    jval->object.len++;

    return true;
}

static bool handle_end_map (void *ctx)
{
    xson_value_t *jval = context_pop (ctx);
    if (jval == NULL)
        return false;
    return true;
}

static bool handle_start_array (void *ctx)
{
    xson_value_t *jval = context_peek(ctx, false);
    if (jval == NULL)
        return false;
    jval->type = XSON_TYPE_ARRAY;
    jval->array.values = NULL;
    jval->array.len = 0;

    return context_push (ctx, jval) == 0 ? true : false;
}

static bool handle_end_array (void *ctx)
{
    xson_value_t *jval = context_pop(ctx);
    if (jval == NULL)
        return false;
    return true;
}

static bool handle_boolean (void *ctx, int value)
{
    xson_value_t *jval = context_peek(ctx, false);
    if (jval == NULL)
        return false;
    jval->type = value ? XSON_TYPE_TRUE : XSON_TYPE_FALSE;
    return true;
}

static bool handle_null (void *ctx)
{
    xson_value_t *jval = context_peek(ctx, false);
    if (jval == NULL)
        return false;
    jval->type = XSON_TYPE_NULL;
    return true;
}

static const xson_callbacks_t callbacks = {
   .xson_null        =   handle_null,        /* null        = */
   .xson_boolean     =   handle_boolean,     /* boolean     = */
   .xson_integer     =   NULL,               /* integer     = */
   .xson_double      =   NULL,               /* double      = */
   .xson_number      =   handle_number,      /* number      = */
   .xson_string      =   handle_string,      /* string      = */
   .xson_start_map   =   handle_start_map,   /* start map   = */
   .xson_map_key     =   handle_map_key,     /* map key     = */
   .xson_end_map     =   handle_end_map,     /* end map     = */
   .xson_start_array =   handle_start_array, /* start array = */
   .xson_end_array   =   handle_end_array    /* end array   = */
};

xson_value_t *xson_tree_parser(const char *input, char *error_buffer, size_t error_buffer_size)
{
    context_t ctx = {
        .root = NULL,
        .depth = 0,
        .errbuf = NULL,
        .errbuf_size = 0,
    };

    ctx.errbuf = error_buffer;
    ctx.errbuf_size = error_buffer_size;

    if (error_buffer != NULL)
        memset (error_buffer, 0, error_buffer_size);

    ctx.root = xson_value_alloc(XSON_TYPE_NULL);
    ctx.stack[0].value = ctx.root;
    ctx.depth = 0;

    json_parser_t handle;
    json_parser_init(&handle, 0, &callbacks, &ctx);

    json_status_t status = json_parser_parse(&handle, (const unsigned char *) input, strlen (input));
    if (status == JSON_STATUS_OK)
        status = json_parser_complete(&handle);

    if (status != JSON_STATUS_OK) {
        if (error_buffer != NULL && error_buffer_size > 0) {
            unsigned char *errmsg = json_parser_get_error(&handle, 1,
                                                   (const unsigned char *) input, strlen(input));
            ssnprintf(error_buffer, error_buffer_size, "%s", (char *)errmsg);
            json_parser_free_error(errmsg);
        }
        json_parser_free (&handle);
        return NULL;
    }

    json_parser_free (&handle);
    return ctx.root;
}

static xson_render_status_t render_value(xson_render_t *r, xson_value_t *v)
{
    xson_render_status_t rstatus = 0;

    switch (v->type) {
    case XSON_TYPE_STRING:
        rstatus = xson_render_string(r, v->string);
        break;
    case XSON_TYPE_NUMBER:
        rstatus = xson_render_double(r, v->number);
        break;
    case XSON_TYPE_OBJECT:
        rstatus = xson_render_map_open(r);
        for (size_t i=0 ; i < v->object.len ; i++) {
            rstatus |= xson_render_key_string(r, v->object.keyval[i].key);
            rstatus |= render_value(r, &v->object.keyval[i].value);
        }
        rstatus |= xson_render_map_close(r);
        break;
    case XSON_TYPE_ARRAY:
        rstatus = xson_render_array_open(r);
        for (size_t i = 0; i < v->array.len; i++) {
            rstatus |= render_value(r, &v->array.values[i]);
        }
        rstatus |= xson_render_array_close(r);
        break;
    case XSON_TYPE_TRUE:
        rstatus = xson_render_bool(r, true);
        break;
    case XSON_TYPE_FALSE:
        rstatus = xson_render_bool(r, false);
        break;
    case XSON_TYPE_NULL:
        rstatus = xson_render_null(r);
        break;
    }

    return rstatus;
}

int xson_tree_render(xson_value_t *v, strbuf_t *buf,
                     xson_render_type_t type, xson_render_option_t options)
{
    if (v == NULL)
        return -1;

    xson_render_t r = {0};

    xson_render_init(&r, buf, type, options);
    return render_value(&r, v);
}
