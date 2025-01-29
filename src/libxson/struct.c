// SPDX-License-Identifier: GPL-2.0-only

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <string.h>

#include "libutils/common.h"
#include "libxson/json_parse.h"
#include "libxson/parser.h"
#include "libxson/struct.h"

#pragma GCC diagnostic ignored "-Wcast-align"

typedef struct {
    void *st;
    json_struct_attr_t *parent;
    json_struct_attr_t *attrs;
    json_struct_attr_t *attr;
} json_stack_t;

typedef struct {
    json_stack_t stack[JSON_MAX_DEPTH];
    int depth;
    char *errbuf;
    size_t errbuf_size;
} context_t;

#define JSON_ERROR(ctx,...)                                             \
    do {                                                                \
        if ((ctx)->errbuf != NULL)                                      \
            snprintf ((ctx)->errbuf, (ctx)->errbuf_size, __VA_ARGS__);  \
    } while(0)

static int context_push(context_t *ctx,
                        json_struct_attr_t *parent, json_struct_attr_t *attrs, void *st)
{
    if ((ctx->depth + 1) >= JSON_MAX_DEPTH) {
        JSON_ERROR (ctx, "Out of memory");
        return ENOMEM;
    }
    ctx->stack[ctx->depth].st = st;
    ctx->stack[ctx->depth].parent = parent;
    ctx->stack[ctx->depth].attrs = attrs;
    ctx->stack[ctx->depth].attr = NULL;
    ctx->depth++;

    return 0;
}

static json_stack_t *context_pop(context_t *ctx)
{
    if (ctx->depth == 0) {
        JSON_ERROR (ctx, "context_pop: " "Bottom of stack reached prematurely");
        return NULL;
    }
    json_stack_t *stack = &ctx->stack[ctx->depth-1];
    ctx->depth--;
    return stack;
}

static json_stack_t *context_peek(context_t *ctx)
{
    if (ctx->depth == 0) {
        JSON_ERROR (ctx, "context_peek: " "Bottom of stack reached prematurely");
        return NULL;
    }

    json_stack_t *stack = &ctx->stack[ctx->depth-1];
#if 0
    if (sval->type == JSON_TYPE_OBJECT)
        return &sval->object.keyval[sval->object.len-1].value;

    if (sval->type == JSON_TYPE_ARRAY) {
        json_value_t *tmp = realloc(sval->array.values,
                                  sizeof(*sval->array.values) * (sval->array.len + 1));
        if (tmp == NULL) {
            JSON_ERROR ((context_t *) ctx, "Out of memory");
            return NULL;
        }
        json_value_t *jval = &sval->array.values[sval->array.len];
        jval->type = JSON_TYPE_NULL;
        sval->array.len++;
        return jval;
    }

    if (sval->type == JSON_TYPE_NULL)
        return sval;
#endif
    return stack;
}

static size_t json_struct_set_defaults(void *st, json_struct_attr_t *attr)
{
    for (size_t i = 0; ; i++) {
        switch (attr[i].type) {
        case JSON_STRUCT_TYPE_NONE:
            return i+1;
            break;
        case JSON_STRUCT_TYPE_ARRAY:
            break;
        case JSON_STRUCT_TYPE_MAP:
            break;
        case JSON_STRUCT_TYPE_OBJECT:
            break;
        case JSON_STRUCT_TYPE_BOOLEAN:
            *(bool *)((char *)st + attr[i].offset) = attr[i].boolean;
            break;
        case JSON_STRUCT_TYPE_INT64:
            *(int64_t *)((char *)st + attr[i].offset) = attr[i].int64;
            break;
        case JSON_STRUCT_TYPE_UINT64:
            *(uint64_t *)((char *)st + attr[i].offset) = attr[i].uint64;
            break;
        case JSON_STRUCT_TYPE_DOUBLE:
            *(double *)((char *)st + attr[i].offset) = attr[i].float64;
            break;
        case JSON_STRUCT_TYPE_STRING:
            if (attr[i].string != NULL) {
                char *str = strdup(attr[i].string);
                if (str != NULL)
                    *(char **)((char *)st + attr[i].offset) = str;
            }
            break;
        }
    }
    return 0;
}

static bool handle_string (void *ctx, const char *str, size_t str_len)
{
    json_stack_t *stack = context_peek(ctx);
    if (stack == NULL)
        return false;

    if ((stack->attr == NULL) || (stack->parent == NULL))
        return true;

    if (stack->attr->type != JSON_STRUCT_TYPE_STRING)
        return false;

    char *nstr = NULL;
    if (str_len > 0) {
        nstr = malloc (str_len + 1);
        if (nstr == NULL) {
            JSON_ERROR ((context_t *) ctx, "Out of memory");
            return false;
        }
        memcpy(nstr, str, str_len);
        nstr[str_len] = 0;
    }

    if (stack->parent->type == JSON_STRUCT_TYPE_ARRAY) {

    } else if (stack->parent->type == JSON_STRUCT_TYPE_OBJECT) {
        char *astr = *(char **)((char *)stack->st + stack->attr->offset);
        if (astr != NULL)
            free(astr);
        *(char **)((char *)stack->st + stack->attr->offset) = nstr;
    } else {
        free(nstr);
        return false;
    }

    free(nstr);  // FIXME
//    void *ptr = (char *)stack->st + stack->attr->offset;
//    char **sptr = ptr;
//    *sptr = nstr;
    return true;
}

static bool handle_number(void * ctx, const char *number_val, size_t number_len)
{
    json_stack_t *stack = context_peek(ctx);
    if (stack == NULL)
        return false;

    if ((stack->attr == NULL) || (stack->parent == NULL))
        return true;

    if (stack->attr->type == JSON_STRUCT_TYPE_ARRAY) {
        size_t array_size = *(size_t *)((char *)stack->st + stack->attr->array.offset_size);

        size_t type_size = 0;
        switch (stack->attr->array.type) {
        case JSON_STRUCT_TYPE_INT64:
            type_size = sizeof(int64_t);
            break;
        case JSON_STRUCT_TYPE_UINT64:
            type_size = sizeof(uint64_t);
            break;
        case JSON_STRUCT_TYPE_DOUBLE:
            type_size = sizeof(double);
            break;
        default:
            break;
        }

        void *ptr = *(void **)((char *)stack->st + stack->attr->offset);
        void *new = realloc(ptr, type_size*(array_size+1));
        if (new == NULL) {
            return false;
        }
        *(void **)((char *)stack->st + stack->attr->offset) = new;

        switch (stack->attr->array.type) {
        case JSON_STRUCT_TYPE_INT64: {
            int64_t num = json_parse_integer((const unsigned char *)number_val, number_len);
            int64_t *array = *(int64_t **)((char *)stack->st + stack->attr->offset);
            array[array_size] = num;
            break;
            }
        case JSON_STRUCT_TYPE_UINT64: {
            uint64_t num = json_parse_integer((const unsigned char *)number_val, number_len);
            uint64_t *array = *(uint64_t **)((char *)stack->st + stack->attr->offset);
            array[array_size] = num;
            break;
            }
        case JSON_STRUCT_TYPE_DOUBLE: {
            double num = strtod(number_val, NULL); // FIXME
            double *array = *(double **)((char *)stack->st + stack->attr->offset);
            array[array_size] = num;
            break;
            }
        default:
            return false;
            break;
        }

        *(size_t *)((char *)stack->st + stack->attr->array.offset_size) = array_size+1;
    } else {
        switch (stack->attr->type) {
        case JSON_STRUCT_TYPE_INT64: {
            int64_t num = json_parse_integer((const unsigned char *)number_val, number_len);
            *(int64_t *)((char *)stack->st + stack->attr->offset) = num;
            break;
            }
        case JSON_STRUCT_TYPE_UINT64: {
            uint64_t num = json_parse_integer((const unsigned char *)number_val, number_len);
            *(uint64_t *)((char *)stack->st + stack->attr->offset) = num;
            break;
            }
        case JSON_STRUCT_TYPE_DOUBLE: {
            double num = strtod(number_val, NULL);
            *(double *)((char *)stack->st + stack->attr->offset) = num;
            break;
            }
        default:
            return false;
            break;
        }
    }

    return true;
}

static bool handle_start_map (void *ctx)
{
    json_stack_t *stack = context_peek(ctx);
    if (stack == NULL)
        return false;

    json_struct_attr_t *attrs = NULL;
    json_struct_attr_t *parent = NULL;

    context_t *sctx = ctx;

    if (sctx->depth == 1) {
        parent = stack->parent;
        attrs = parent->object.attrs;
    } else {
#if 0
        parent = stack->parent;
        attrs = parent->object.attrs;
#endif
        parent = stack->attr;
        attrs = parent->object.attrs;
    }

    /* coverity[REVERSE_INULL] */
    if (parent == NULL)
        return true;

    void *st = calloc(1, parent->object.struct_size);
    if (st == NULL)
        return true;
//  if (sctx->depth > 1) {
    if (stack->attr != NULL) {
//    void *pst = sctx->stack[sctx->depth-2].st;
        *(void **)((char *)stack->st + stack->attr->offset) = st;
    }
    json_struct_set_defaults(st, parent->object.attrs);

    if (sctx->depth == 1) {
fprintf(stderr, "1st: %p\n", stack->st);
        stack->st = st;
    }
fprintf(stderr, "2st: %p\n", stack->st);

    return context_push(ctx, parent, attrs, st) == 0 ? true : false;
}

static bool handle_map_key(void * ctx, const char *key, size_t size)
{
    json_stack_t *stack = context_peek(ctx);
    if (stack == NULL)
        return false;

fprintf(stderr, "key st: %.*s %p\n", (int)size, key,stack->st);
    if (stack->parent == NULL)
        return true;

    json_struct_attr_t *attrs = stack->attrs;

    stack->attr = NULL;
    for (size_t i = 0; ; i++) {
        if (attrs[i].type == JSON_STRUCT_TYPE_NONE)
            break;
        if (attrs[i].attr_size == size) {
            if (strncmp((const char *)key, attrs[i].attr, size) == 0) {
                stack->attr = &attrs[i];
                break;
            }
        }
    }
fprintf(stderr, "key attr: %.*s \n", (int)size, key);

    return true;
}

static bool handle_end_map (void *ctx)
{
    context_pop(ctx);
    return true;
}

static bool handle_start_array (void *ctx)
{
    json_stack_t *stack = context_peek(ctx);
    if (stack == NULL)
        return false;
//    return context_push (ctx, l) == 0 ? 1 : 0;
    return true;
}

static bool handle_end_array (void *ctx)
{
    json_stack_t *stack = context_peek(ctx);
    if (stack == NULL)
        return false;
    return true;
}

static bool handle_boolean (void *ctx, int value)
{
    json_stack_t *stack = context_peek(ctx);
    if (stack == NULL)
        return false;
    if ((stack->attr == NULL) || (stack->parent == NULL))
        return true;

    if (stack->attr->type != JSON_STRUCT_TYPE_BOOLEAN)
        return false; //FIXME

    *(bool *)((char *)stack->st + stack->attr->offset) = value ? true : false;

    return true;
}

static bool handle_null (void *ctx)
{
    json_stack_t *stack = context_peek(ctx);
    if (stack == NULL)
        return false;

    if ((stack->attr == NULL) || (stack->parent == NULL))
        return true;

    if (stack->attr->type != JSON_STRUCT_TYPE_STRING)
        return false; // FIXME

    if (stack->parent->type == JSON_STRUCT_TYPE_ARRAY) {


    } else if (stack->parent->type == JSON_STRUCT_TYPE_OBJECT) {
        char *astr = *(char **)((char *)stack->st + stack->attr->offset);
        if (astr != NULL) {
//        free(astr);
        }
        astr = NULL;
    } else {
        return false;
    }

    return true;
}

static const json_callbacks_t callbacks = {
    .json_null        = handle_null,        /* null        = */
    .json_boolean     = handle_boolean,     /* boolean     = */
    .json_integer     = NULL,               /* integer     = */
    .json_double      = NULL,               /* double      = */
    .json_number      = handle_number,      /* number      = */
    .json_string      = handle_string,      /* string      = */
    .json_start_map   = handle_start_map,   /* start map   = */
    .json_map_key     = handle_map_key,     /* map key     = */
    .json_end_map     = handle_end_map,     /* end map     = */
    .json_start_array = handle_start_array, /* start array = */
    .json_end_array   = handle_end_array    /* end array   = */
};

void *json_struct_parse(json_struct_attr_t *parent,
                        const char *input, char *error_buffer, size_t error_buffer_size)
{
    context_t ctx = {
        .depth = 0,
        .errbuf = error_buffer,
        .errbuf_size = error_buffer_size
    };

    if (error_buffer != NULL)
        memset (error_buffer, 0, error_buffer_size);


    ctx.stack[0].st = NULL;
    if (parent->type == JSON_STRUCT_TYPE_OBJECT) {
        ctx.stack[0].attrs = parent->object.attrs;
    }
    ctx.stack[0].attr = NULL;
    ctx.stack[0].parent = parent;
    ctx.depth = 1;

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
//      json_parser_free(&handle);
        return NULL;
    }

    json_parser_free (&handle);
    return ctx.stack[0].st;
}

void *json_struct_parse_object(size_t struct_size, json_struct_attr_t *attrs,
                               const char *input, char *error_buffer, size_t error_buffer_size)
{
    json_struct_attr_t parent = {0};

    parent.type = JSON_STRUCT_TYPE_OBJECT;
    parent.object.struct_size = struct_size;
    parent.object.attrs = attrs;

    return json_struct_parse(&parent, input, error_buffer, error_buffer_size);
}
