// SPDX-License-Identifier: GPL-2.0-only OR PostgreSQL
// SPDX-FileCopyrightText: Copyright (c) 2019-2023, PostgreSQL Global Development Group
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <string.h>
#include <stdlib.h>
#include <regex.h>
#include <math.h>

#include "jsonpath.h"
#include "libxson/tree.h"

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

typedef struct {
    xson_value_t *root;
    xson_value_t *current;
    bool throw_errors;
    char *buffer_error;
    size_t buffer_error_size;
} jsonpath_exec_ctx_t;

typedef enum {
    JSONPATH_BOOL_FALSE   = 0,
    JSONPATH_BOOL_TRUE    = 1,
    JSONPATH_BOOL_UNKNOWN = 2
} jsonpath_bool_t;

typedef jsonpath_bool_t (*jsonpath_pedicate_callback_t) (jsonpath_item_t *jsp,
                                                         xson_value_t *larg,
                                                         xson_value_t *rarg,
                                                         void *param);
typedef double (*binary_arithm_callback_t) (double num1, double num2, bool *error);
typedef double (*unary_arithm_callback_t) (double num1, bool *error);

static jsonpath_exec_result_t jsonpath_exec_item(jsonpath_exec_ctx_t *ctx,
                                                 jsonpath_item_t *jsp, xson_value_t *jb,
                                                 xson_value_list_t *found);

static jsonpath_exec_result_t jsonpath_exec_item_opt_unwrap_target(jsonpath_exec_ctx_t *ctx,
                                                                   jsonpath_item_t *jsp,
                                                                   xson_value_t *jb,
                                                                   xson_value_list_t *found,
                                                                   bool unwrap);
static jsonpath_exec_result_t jsonpath_execute_next_item(jsonpath_exec_ctx_t *ctx,
                                                         jsonpath_item_t *cur,
                                                         jsonpath_item_t *next,
                                                         xson_value_t *v,
                                                         xson_value_list_t *found);

static jsonpath_exec_result_t jsonpath_exec_any_item(jsonpath_exec_ctx_t *ctx, jsonpath_item_t *jsp,
                                                     xson_value_t *jbc, xson_value_list_t *found,
                                                     uint32_t level, uint32_t first, uint32_t last,
                                                     bool unwrap_next);

static void xson_value_list_append(xson_value_list_t *jvl, xson_value_t *jbv)
{
    if (jvl->singleton) {
        jvl->list = jsonpath_list_make2(jvl->singleton, jbv);
        jvl->singleton = NULL;
    } else if (!jvl->list) {
        jvl->singleton = jbv;
    } else {
        jvl->list = jsonpath_list_append(jvl->list, jbv);
    }
}

static int xson_value_list_length(const xson_value_list_t *jvl)
{
    return jvl->singleton ? 1 : jsonpath_list_length(jvl->list);
}

static bool xson_value_list_is_empty(xson_value_list_t *jvl)
{
    return !jvl->singleton && (jvl->list == NULL);
}

static xson_value_t *xson_value_list_head(xson_value_list_t *jvl)
{
    return jvl->singleton ? jvl->singleton : jsonpath_list_initial(jvl->list);
}

#if 0
static void xson_value_dump(FILE *fp, xson_value_t *jbv)
{
    strbuf_t buf = {0};
    xson_tree_render(jbv, &buf, XSON_RENDER_TYPE_JSON, 0);
    fputs(buf.ptr, fp);
    strbuf_destroy(&buf);
}

static void dump(FILE *fp,  xson_value_t *xv, jsonpath_item_t *ji,  char *name, const char *fmt, ...)
{
    fputs(name, fp);
    fputs(" <", fp);
    if (xv != NULL)
        xson_value_dump(fp, xv);
    fputs("><", fp);
    if (ji != NULL)
        jsonpath_dump_item(fp, ji);
    fputs("> ", fp);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
}

static jsonpath_list_t *xson_value_list_get_list(xson_value_list_t *jvl)
{
    if (jvl->singleton)
        return jsonpath_list_make1(jvl->singleton);

    return jvl->list;
}
#endif

void xson_value_list_init_iterator(const xson_value_list_t *jvl, xson_value_list_iterator_t *it)
{
    if (jvl->singleton) {
        it->value = jvl->singleton;
        it->list = NULL;
        it->next = NULL;
    } else if (jvl->list != NULL) {
        it->value = (xson_value_t *) jsonpath_list_initial(jvl->list);
        it->list = jvl->list;
        it->next = jsonpath_list_second_cell(jvl->list);
    } else {
        it->value = NULL;
        it->list = NULL;
        it->next = NULL;
    }
}

xson_value_t *xson_value_list_next(__attribute__((unused)) const xson_value_list_t *jvl,
                                   xson_value_list_iterator_t *it)
{
    xson_value_t *result = it->value;

    if (it->next) {
        it->value = jsonpath_list_first(it->next);
        it->next = jsonpath_list_next(it->list, it->next);
    } else {
        it->value = NULL;
    }

    return result;
}

void xson_value_list_destroy(xson_value_list_t *jvl)
{
    if (jvl == NULL)
        return;

    if (jvl->singleton != NULL) {
        xson_value_free(jvl->singleton);
        jvl->singleton = NULL;
        return;
    }

    if (jvl->list == NULL)
        return;


    int len = jsonpath_list_length(jvl->list);
    for (int i = 0; i < len ; i++) {
        jsonpath_list_cell_t *c = jsonpath_list_nth_cell(jvl->list, i);
        if (c == NULL)
            continue;
        xson_value_free(c->ptr_value);
    }
    jsonpath_list_free(jvl->list);
    jvl->list = NULL;
}

static xson_value_t *xson_value_get_scalar(xson_value_t *scalar, xson_type_t type)
{
    return scalar->type == type ? scalar : NULL;
}

static double numeric_add_opt_error(double num1, double num2, __attribute__((unused)) bool *error)
{
    return num1 + num2;
}

static double numeric_sub_opt_error(double num1, double num2, __attribute__((unused)) bool *error)
{
    return num1 - num2;
}

static double numeric_mul_opt_error(double num1, double num2, __attribute__((unused)) bool *error)
{
    return num1 * num2;
}

static double numeric_div_opt_error(double num1, double num2, __attribute__((unused)) bool *error)
{
    return num1 / num2;
}

static double numeric_mod_opt_error(double num1, double num2, __attribute__((unused)) bool *error)
{
    return (int64_t)num1 % (int64_t)num2;
}

static double numeric_uminus(double num1, __attribute__((unused)) bool *error)
{
    return -num1;
}

static double numeric_abs(double num1, __attribute__((unused)) bool *error)
{
    return fabs(num1);
}

static double numeric_floor(double num1, __attribute__((unused)) bool *error)
{
    return floor(num1);
}

static double numeric_ceil(double num1, __attribute__((unused)) bool *error)
{
    return ceil(num1);
}

static void jsonpath_error(jsonpath_exec_ctx_t *ctx, const char *format, ...)
{
    if ((ctx->buffer_error == NULL) || (ctx->buffer_error_size == 0))
        return;

    va_list ap;
    va_start(ap, format);
    char msg[1024];
    vsnprintf(msg, sizeof(msg), format, ap);
    msg[sizeof(msg) - 1] = '\0';
    va_end(ap);
}

static void jsonpath_set_item(__attribute__((unused)) jsonpath_exec_ctx_t *ctx,
                              jsonpath_item_t *item, xson_value_t *value)
{
    switch (item->type) {
    case JSONPATH_NULL:
        xson_value_set_null(value);
        break;
    case JSONPATH_BOOL:
        if (jsonpath_get_bool(item))
            xson_value_set_true(value);
        else
            xson_value_set_false(value);
        break;
    case JSONPATH_NUMERIC:
        xson_value_set_number(value, jsonpath_get_numeric(item));
        break;
    case JSONPATH_STRING: {
        int len = 0;
        xson_value_set_string(value, jsonpath_get_string(item, &len));
    }   break;
    default:
#if 0
        elog(ERROR, "unexpected jsonpath item type");
#endif
        break;
    }
}

static int jsonpath_exec_any_item_value(jsonpath_exec_ctx_t *ctx,
                                        jsonpath_item_t *jsp, xson_value_t *v,
                                        xson_value_list_t *found,
                                        uint32_t level, uint32_t first, uint32_t last,
                                        bool unwrap_next,
                                        jsonpath_exec_result_t *res)
{
    if (level >= first) {
        if (jsp) {
            *res = jsonpath_exec_item_opt_unwrap_target(ctx, jsp, v, found, unwrap_next);

            if (*res == JSONPATH_EXEC_RESULT_ERROR)
                return 1;

            if (*res == JSONPATH_EXEC_RESULT_OK && !found)
                return 1;
        } else if (found) {
            xson_value_list_append(found, xson_value_clone(v));
        } else {
            *res = JSONPATH_EXEC_RESULT_OK;
            return 0;
        }
    }

    if ((level < last) && !xson_value_is_scalar(v)) {
        *res = jsonpath_exec_any_item (ctx, jsp, v, found, level + 1, first, last, unwrap_next);

        if (*res == JSONPATH_EXEC_RESULT_ERROR)
            return 1;

        if ((*res == JSONPATH_EXEC_RESULT_OK) && (found == NULL))
            return 1;
    }

    return -1;
}

static jsonpath_exec_result_t jsonpath_exec_any_item(jsonpath_exec_ctx_t *ctx,
                                                     jsonpath_item_t *jsp, xson_value_t *jbc,
                                                     xson_value_list_t *found,
                                                     uint32_t level, uint32_t first, uint32_t last,
                                                     bool unwrap_next)
{
    jsonpath_exec_result_t res = JSONPATH_EXEC_RESULT_NOT_FOUND;
    int status = 0;

    if (level > last)
        return res;

    xson_value_t *v = jbc;
    switch (v->type) {
    case XSON_TYPE_NULL:
    case XSON_TYPE_STRING:
    case XSON_TYPE_NUMBER:
    case XSON_TYPE_TRUE:
    case XSON_TYPE_FALSE:
        status = jsonpath_exec_any_item_value(ctx, jsp, v, found, level, first, last,
                                              unwrap_next, &res);
        if (status >= 0)
            return res;
        break;
    case XSON_TYPE_OBJECT:
        for (size_t i = 0; i < v->object.len; i++) {
            xson_value_t *nv = &v->object.keyval[i].value;
            status = jsonpath_exec_any_item_value(ctx, jsp, nv, found, level, first, last,
                                                  unwrap_next, &res);
            if (status >= 0)
                return res;
        }
        break;
    case XSON_TYPE_ARRAY:
        for (size_t i = 0; i < v->array.len; i++) {
            xson_value_t *nv = &v->array.values[i];
            status = jsonpath_exec_any_item_value(ctx, jsp, nv, found, level, first, last,
                                                  unwrap_next, &res);
            if (status >= 0)
                return res;
        }
        break;
    }

    return res;
}

static jsonpath_exec_result_t jsonpath_exec_item_nothrow(jsonpath_exec_ctx_t *ctx,
                                                         jsonpath_item_t *jsp, xson_value_t *jb,
                                                         xson_value_list_t *found)
{
    bool throw_errors = ctx->throw_errors;
    ctx->throw_errors = false;
    jsonpath_exec_result_t res = jsonpath_exec_item(ctx, jsp, jb, found);
    ctx->throw_errors = throw_errors;
    return res;
}

static jsonpath_exec_result_t jsonpath_exec_binary_expr(jsonpath_exec_ctx_t *ctx,
                                                        jsonpath_item_t *jsp,
                                                        xson_value_t *jb,
                                                        binary_arithm_callback_t func,
                                                        xson_value_list_t *found)
{
    xson_value_list_t lseq = {0};
    xson_value_list_t rseq = {0};
    xson_value_t *lval;
    xson_value_t *rval;
    double rnum = 0;
    double lnum = 0;
    double res;


    jsonpath_item_t *elem = jsonpath_get_left_arg(jsp);
    jsonpath_exec_result_t jper = jsonpath_exec_item(ctx, elem, jb, &lseq);
    if (jper == JSONPATH_EXEC_RESULT_ERROR) {
        xson_value_list_destroy(&lseq);
        return jper;
    }

    if (xson_value_list_length(&lseq) != 1 ||
        !(lval = xson_value_get_scalar(xson_value_list_head(&lseq), XSON_TYPE_NUMBER))) {
        jsonpath_error(ctx, "left operand of jsonpath operator %s is not a single numeric value",
                       jsonpath_operation_name(jsp->type));
        xson_value_list_destroy(&lseq);
        return JSONPATH_EXEC_RESULT_ERROR;
    }

    lnum = lval->number;

    elem = jsonpath_get_right_arg(jsp);

    jper = jsonpath_exec_item(ctx, elem, jb, &rseq);
    if (jper == JSONPATH_EXEC_RESULT_ERROR) {
        xson_value_list_destroy(&lseq);
        xson_value_list_destroy(&rseq);
        return jper;
    }


    if (xson_value_list_length(&rseq) != 1 ||
        !(rval = xson_value_get_scalar(xson_value_list_head(&rseq), XSON_TYPE_NUMBER))) {
        jsonpath_error(ctx, "right operand of jsonpath operator %s is not a single numeric value",
                       jsonpath_operation_name(jsp->type));
        xson_value_list_destroy(&lseq);
        xson_value_list_destroy(&rseq);
        return JSONPATH_EXEC_RESULT_ERROR;
    }

    rnum = rval->number;

    if (ctx->throw_errors) {
        res = func(lnum, rnum, NULL);
    } else {
        bool error = false;
        res = func(lnum, rnum, &error);
        if (error) {
            xson_value_list_destroy(&lseq);
            xson_value_list_destroy(&rseq);
            return JSONPATH_EXEC_RESULT_ERROR;
        }
    }

    elem = jsonpath_get_next(jsp);
    if ((elem == NULL) && !found) {
        xson_value_list_destroy(&lseq);
        xson_value_list_destroy(&rseq);
        return JSONPATH_EXEC_RESULT_OK;
    }

    xson_value_list_destroy(&lseq);
    xson_value_list_destroy(&rseq);

    xson_value_t jbv = {0};
    xson_value_set_number(&jbv, res);

    return jsonpath_execute_next_item(ctx, jsp, elem, &jbv, found);
}

static jsonpath_exec_result_t jsonpath_exec_unary_expr(jsonpath_exec_ctx_t *ctx,
                                                       jsonpath_item_t *jsp,
                                                       xson_value_t *jb,
                                                       unary_arithm_callback_t func,
                                                       xson_value_list_t *found)
{
    jsonpath_exec_result_t jper2;
    xson_value_list_t seq = {0};
    xson_value_list_iterator_t it;
    xson_value_t *val;

    jsonpath_item_t *elem = jsonpath_get_arg(jsp);
    jsonpath_exec_result_t jper = jsonpath_exec_item(ctx, elem, jb, &seq);
    if (jper == JSONPATH_EXEC_RESULT_ERROR) {
        xson_value_list_destroy(&seq);
        return jper;
    }

    jper = JSONPATH_EXEC_RESULT_NOT_FOUND;

    elem = jsonpath_get_next(jsp);

    xson_value_list_init_iterator(&seq, &it);
    while ((val = xson_value_list_next(&seq, &it))) {
        if ((val = xson_value_get_scalar(val, XSON_TYPE_NUMBER))) {
            if (!found && (elem == NULL)) {
                xson_value_list_destroy(&seq);
                return JSONPATH_EXEC_RESULT_OK;
            }
        } else {
            if (!found && elem == NULL)
                continue;

            jsonpath_error(ctx, "operand of unary jsonpath operator %s is not a numeric value",
                           jsonpath_operation_name(jsp->type));
            return JSONPATH_EXEC_RESULT_ERROR;
        }

        if (func) {
            bool error = false;
            val->number = func(val->number, &error);
        }

        jper2 = jsonpath_execute_next_item(ctx, jsp, elem, val, found);

        if (jper2 == JSONPATH_EXEC_RESULT_ERROR) {
            xson_value_list_destroy(&seq);
            return jper2;
        }

        if (jper2 == JSONPATH_EXEC_RESULT_OK) {
            if (!found) {
                xson_value_list_destroy(&seq);
                return JSONPATH_EXEC_RESULT_OK;
            }
            jper = JSONPATH_EXEC_RESULT_OK;
        }
    }

    xson_value_list_destroy(&seq);

    return jper;
}

static jsonpath_exec_result_t jsonpath_execute_next_item(jsonpath_exec_ctx_t *ctx,
                                                         jsonpath_item_t *cur,
                                                         jsonpath_item_t *next,
                                                         xson_value_t *v, xson_value_list_t *found)
{
    bool has_next = false;

    if (cur == NULL) {
        has_next = next != NULL;
    } else if (next) {
        has_next = jsonpath_has_next(cur);
    } else {
        next = jsonpath_get_next(cur);
        if (next == NULL) {
            next = jsonpath_get_follow(cur);
        }
        has_next = next != NULL;
    }

    if (has_next)
        return jsonpath_exec_item(ctx, next, v, found);

    if (found != NULL)
        xson_value_list_append(found, xson_value_clone(v));

    return JSONPATH_EXEC_RESULT_OK;
}

static jsonpath_bool_t jsonpath_exec_predicate(jsonpath_exec_ctx_t *ctx, jsonpath_item_t *pred,
                                               jsonpath_item_t *larg, jsonpath_item_t *rarg,
                                               xson_value_t *jb,
                                               jsonpath_pedicate_callback_t exec, void *param)
{
    xson_value_list_iterator_t lseqit;
    xson_value_list_t lseq = {0};
    xson_value_list_t rseq = {0};
    xson_value_t *lval;
    bool error = false;
    bool found = false;

    /* Left argument is always auto-unwrapped. */
    jsonpath_exec_result_t res = jsonpath_exec_item_nothrow(ctx, larg, jb, &lseq);
    if (res == JSONPATH_EXEC_RESULT_ERROR) {
        xson_value_list_destroy(&lseq);
        return JSONPATH_BOOL_UNKNOWN;
    }

    if (rarg) {
        /* Right argument is conditionally auto-unwrapped. */
        res = jsonpath_exec_item_nothrow(ctx, rarg, jb, &rseq);
        if (res == JSONPATH_EXEC_RESULT_ERROR) {
            xson_value_list_destroy(&lseq);
            xson_value_list_destroy(&rseq);
            return JSONPATH_BOOL_UNKNOWN;
        }

        if (xson_value_list_is_empty(&lseq) && xson_value_list_is_empty(&rseq)) {
            xson_value_list_destroy(&lseq);
            xson_value_list_destroy(&rseq);

            if (pred->type == JSONPATH_EQUAL)
                return JSONPATH_BOOL_TRUE;
            else if (pred->type == JSONPATH_NOTEQUAL)
                return JSONPATH_BOOL_FALSE;

            return JSONPATH_BOOL_TRUE;
        }
    }

    xson_value_list_init_iterator(&lseq, &lseqit);
    while ((lval = xson_value_list_next(&lseq, &lseqit))) {
        xson_value_list_iterator_t rseqit;
        xson_value_t *rval;
        bool first = true;
        xson_value_list_init_iterator(&rseq, &rseqit);
        if (rarg)
            rval = xson_value_list_next(&rseq, &rseqit);
        else
            rval = NULL;
        /* Loop over right arg sequence or do single pass otherwise */
        while (rarg ? (rval != NULL) : first) {
            if (exec != NULL) {
                jsonpath_bool_t bres = exec(pred, lval, rval, param);

                if (bres == JSONPATH_BOOL_UNKNOWN) {
                    error = true;
                } else if (bres == JSONPATH_BOOL_TRUE) {
                    found = true;
                }
            } else {
                found = true;
            }

            first = false;
            if (rarg)
                rval = xson_value_list_next(&rseq, &rseqit);
        }
    }

    xson_value_list_destroy(&lseq);
    xson_value_list_destroy(&rseq);

    if (found)
        return JSONPATH_BOOL_TRUE;

    if (error)
        return JSONPATH_BOOL_UNKNOWN;

    return JSONPATH_BOOL_FALSE;
}

static jsonpath_bool_t jsonpath_compare_items(int32_t op, xson_value_t *jb1, xson_value_t *jb2)
{
    double cmp = 0;

    if ((jb1 == NULL) || (jb2 == NULL))
        return JSONPATH_BOOL_UNKNOWN;

    if (jb1->type != jb2->type) {
        if (op == JSONPATH_NOTEQUAL)
            return JSONPATH_BOOL_TRUE;
        else
            return JSONPATH_BOOL_FALSE;
    }

    switch (jb1->type) {
    case XSON_TYPE_NULL:
        cmp = 0;
        break;
    case XSON_TYPE_TRUE:
    case XSON_TYPE_FALSE: {
        bool b1 = xson_value_is_true(jb1);
        bool b2 = xson_value_is_true(jb2);
        cmp = b1 == b2 ? 0 : b1 ? 1 : -1;
    }   break;
    case XSON_TYPE_NUMBER:
        cmp = jb1->number - jb2->number;
        break;
    case XSON_TYPE_STRING:
        cmp = strcmp(jb1->string, jb2->string);
        break;
    case XSON_TYPE_ARRAY:
    case XSON_TYPE_OBJECT:
        cmp = xson_value_cmp(jb1, jb2);
        break;
    }

    bool res;
    switch (op) {
    case JSONPATH_EQUAL:
        res = (cmp == 0);
        break;
    case JSONPATH_NOTEQUAL:
        res = (cmp != 0);
        break;
    case JSONPATH_LESS:
        res = (cmp < 0);
        break;
    case JSONPATH_GREATER:
        res = (cmp > 0);
        break;
    case JSONPATH_LESSOREQUAL:
        res = (cmp <= 0);
        break;
    case JSONPATH_GREATEROREQUAL:
        res = (cmp >= 0);
        break;
    default:
        return JSONPATH_BOOL_UNKNOWN;
    }

    return res ? JSONPATH_BOOL_TRUE : JSONPATH_BOOL_FALSE;
}

static jsonpath_bool_t jsonpath_exec_cmp(jsonpath_item_t *cmp, xson_value_t *lv, xson_value_t *rv,
                                         __attribute__((unused)) void *p)
{
    return jsonpath_compare_items(cmp->type, lv, rv);
}

static jsonpath_bool_t jsonpath_exec_regex(jsonpath_item_t *jsp,
                                           xson_value_t *str,
                                           __attribute__((unused)) xson_value_t *rarg,
                                           __attribute__((unused)) void *param)
{
    if (!(str = xson_value_get_scalar(str, XSON_TYPE_STRING)))
        return JSONPATH_BOOL_UNKNOWN;

    regoff_t len = strlen(str->string);
    regmatch_t pmatch = { 0 };

    int status = regexec(&jsp->value.regex.regex, str->string, 1, &pmatch, 0);
    if (status == 0) {
        if (jsp->type == JSONPATH_MATCH) {
            if ((pmatch.rm_so == 0) && (pmatch.rm_eo == len))
                return JSONPATH_BOOL_TRUE;
            return JSONPATH_BOOL_FALSE;
        }
        return JSONPATH_BOOL_TRUE;
    }

    return JSONPATH_BOOL_FALSE;
}

static jsonpath_bool_t jsonpath_exec_bool(jsonpath_exec_ctx_t *ctx, jsonpath_item_t *jsp,
                                          xson_value_t *jb)
{
    jsonpath_item_t *larg = NULL;
    jsonpath_item_t *rarg = NULL;
    jsonpath_bool_t res;
    jsonpath_bool_t res2;

    switch (jsp->type) {
    case JSONPATH_AND:
        larg = jsonpath_get_left_arg(jsp);
        res = jsonpath_exec_bool(ctx, larg, jb);
        if (res == JSONPATH_BOOL_FALSE)
            return JSONPATH_BOOL_FALSE;

        rarg = jsonpath_get_right_arg(jsp);
        res2 = jsonpath_exec_bool(ctx, rarg, jb);

        return res2 == JSONPATH_BOOL_TRUE ? res : res2;
        break;
    case JSONPATH_OR:
        larg = jsonpath_get_left_arg(jsp);
        res = jsonpath_exec_bool(ctx, larg, jb);
        if (res == JSONPATH_BOOL_TRUE)
            return JSONPATH_BOOL_TRUE;

        rarg = jsonpath_get_right_arg(jsp);
        res2 = jsonpath_exec_bool(ctx, rarg, jb);

        return res2 == JSONPATH_BOOL_FALSE ? res : res2;
        break;
    case JSONPATH_NOT:
        larg = jsonpath_get_arg(jsp);
        res = jsonpath_exec_bool(ctx, larg, jb);
        if (res == JSONPATH_BOOL_UNKNOWN)
            return JSONPATH_BOOL_UNKNOWN;

        return res == JSONPATH_BOOL_TRUE ? JSONPATH_BOOL_FALSE : JSONPATH_BOOL_TRUE;
        break;
    case JSONPATH_EQUAL:
    case JSONPATH_NOTEQUAL:
    case JSONPATH_LESS:
    case JSONPATH_GREATER:
    case JSONPATH_LESSOREQUAL:
    case JSONPATH_GREATEROREQUAL:
        larg = jsonpath_get_left_arg(jsp);
        rarg = jsonpath_get_right_arg(jsp);
        return jsonpath_exec_predicate(ctx, jsp, larg, rarg, jb, jsonpath_exec_cmp, ctx);
        break;
    case JSONPATH_MATCH:
    case JSONPATH_SEARCH:
    case JSONPATH_REGEX:{
        larg = jsp->value.regex.expr;
        return jsonpath_exec_predicate(ctx, jsp, larg, NULL, jb, jsonpath_exec_regex, ctx);
    }   break;
    case JSONPATH_BOOL:
        return jsp->value.boolean ? JSONPATH_BOOL_TRUE : JSONPATH_BOOL_FALSE;
        break;
    case JSONPATH_ROOT:
        return jsonpath_exec_predicate(ctx, jsp, jsp, NULL, jb, NULL, ctx);
        break;
    case JSONPATH_CURRENT:
        return jsonpath_exec_predicate(ctx, jsp, jsp, NULL, jb, NULL, ctx);
        break;
    default:
#if 0
        elog(ERROR, "invalid boolean jsonpath item type: %d", jsp->type);
#endif
        return JSONPATH_BOOL_UNKNOWN;
        break;
    }
}

static jsonpath_exec_result_t jsonpath_exec_numeric_item(jsonpath_exec_ctx_t *ctx,
                                                         jsonpath_item_t *jsp, xson_value_t *jb,
                                                         unary_arithm_callback_t func,
                                                         xson_value_list_t *found)
{
    xson_value_list_t lseq = {0};
    jsonpath_exec_result_t res = jsonpath_exec_item_nothrow(ctx, jsp->value.arg, jb, &lseq);
    if (res != JSONPATH_EXEC_RESULT_ERROR) {
        xson_value_list_iterator_t lseqit;
        xson_value_t *lval;
        xson_value_list_init_iterator(&lseq, &lseqit);
        while ((lval = xson_value_list_next(&lseq, &lseqit))) {
            if (lval->type == XSON_TYPE_NUMBER) {
                double datum = func(lval->number, NULL);
                xson_value_t jbv = {0};
                xson_value_set_number(&jbv, datum);
                res = jsonpath_execute_next_item(ctx, jsp, NULL, &jbv, found);
            } else {
                jsonpath_error(ctx, "jsonpath item method .%s() can only be applied "
                                    "to a numeric value", jsonpath_operation_name(jsp->type));
                res = JSONPATH_EXEC_RESULT_ERROR;
            }

            if (res == JSONPATH_EXEC_RESULT_ERROR)
                break;
        }
    }
    xson_value_list_destroy(&lseq);
    return res;
}

static jsonpath_bool_t jsonpath_exec_nested_bool_item(jsonpath_exec_ctx_t *ctx,
                                                      jsonpath_item_t *jsp,
                                                      xson_value_t *jb)
{
    xson_value_t *prev;
    jsonpath_bool_t res;

    prev = ctx->current;
    ctx->current = jb;
    res = jsonpath_exec_bool(ctx, jsp, jb);
    ctx->current = prev;

    return res;
}

static jsonpath_exec_result_t jsonpath_append_bool_result(jsonpath_exec_ctx_t *ctx,
                                                          jsonpath_item_t *jsp,
                                                          xson_value_list_t *found,
                                                          jsonpath_bool_t res)
{
    xson_value_t jbv = {0};

    jsonpath_item_t *next = jsonpath_get_next(jsp);
    if ((next == NULL) && (found == NULL))
        return JSONPATH_EXEC_RESULT_OK;

    if (res == JSONPATH_BOOL_UNKNOWN) {
        xson_value_set_false(&jbv);
    } else {
        if (res == JSONPATH_BOOL_TRUE)
            xson_value_set_true(&jbv);
        else
            xson_value_set_false(&jbv);
    }

    return jsonpath_execute_next_item(ctx, jsp, next, &jbv, found);
}

static jsonpath_exec_result_t jsonpath_exec_item(jsonpath_exec_ctx_t *ctx, jsonpath_item_t *jsp,
                                                 xson_value_t *jb, xson_value_list_t *found)
{
    return jsonpath_exec_item_opt_unwrap_target(ctx, jsp, jb, found, true);
}

static jsonpath_exec_result_t jsonpath_exec_item_opt_unwrap_target(jsonpath_exec_ctx_t *ctx,
                                                                   jsonpath_item_t *jsp,
                                                                   xson_value_t *jb,
                                                                   xson_value_list_t *found,
                                                                   bool unwrap)
{
    jsonpath_item_t *elem = NULL;
    jsonpath_exec_result_t res = JSONPATH_EXEC_RESULT_NOT_FOUND;

    switch (jsp->type) {
    case JSONPATH_AND:
    case JSONPATH_OR:
    case JSONPATH_NOT:
    case JSONPATH_EQUAL:
    case JSONPATH_NOTEQUAL:
    case JSONPATH_LESS:
    case JSONPATH_GREATER:
    case JSONPATH_LESSOREQUAL:
    case JSONPATH_GREATEROREQUAL:
    case JSONPATH_MATCH:
    case JSONPATH_SEARCH:
    case JSONPATH_REGEX:{
        jsonpath_bool_t st = jsonpath_exec_bool(ctx, jsp, jb);

        res = jsonpath_append_bool_result(ctx, jsp, found, st);
    }   break;
    case JSONPATH_KEY:
        if (xson_value_type(jb) == XSON_TYPE_OBJECT) {
            xson_value_t *v;

            int32_t len = 0;
            char *key = jsonpath_get_string(jsp, &len);

            v = xson_value_object_find(jb, key);
            if (v != NULL)
                res = jsonpath_execute_next_item(ctx, jsp, NULL, v, found);
        }
        break;
    case JSONPATH_ROOT:
        jb = ctx->root;
        res = jsonpath_execute_next_item(ctx, jsp, NULL, jb, found);
        break;
    case JSONPATH_CURRENT:
        res = jsonpath_execute_next_item(ctx, jsp, NULL, ctx->current, found);
        break;
        break;
    case JSONPATH_UNION: {
        res = JSONPATH_EXEC_RESULT_NOT_FOUND;
        for (int32_t i = 0; i < jsp->value.iunion.len; i++) {
            jsonpath_item_t *uelem = jsp->value.iunion.items[i];
            res = jsonpath_execute_next_item(ctx, NULL, uelem, jb, found);

            if (res == JSONPATH_EXEC_RESULT_ERROR)
                break;

            if (res == JSONPATH_EXEC_RESULT_OK && !found)
                break;
        }

    }   break;
    case JSONPATH_DSC_UNION: {
        res = JSONPATH_EXEC_RESULT_NOT_FOUND;
        for (int32_t i = 0; i < jsp->value.iunion.len; i++) {
            jsonpath_item_t *uelem = jsp->value.iunion.items[i];
            if (!xson_value_is_scalar(jb)) {
                if (xson_value_is_array(jb)) {
                    xson_value_t a = {.type = XSON_TYPE_ARRAY, .array.len = 1, .array.values = jb };
                    res = jsonpath_exec_any_item (ctx, uelem, &a, found,
                                                  1, 1, UINT32_MAX,
                                                  true);
                } else if(xson_value_is_object(jb)) {
                    xson_value_t o = {.type = XSON_TYPE_OBJECT, .object.len = 1,
                                      .object.keyval = &(xson_keyval_t){.key = "", .value = *jb}};
                    res = jsonpath_exec_any_item (ctx, uelem, &o, found,
                                                  1, 1, UINT32_MAX,
                                                  true);

                }

                if (res == JSONPATH_EXEC_RESULT_ERROR)
                    break;

                if (res == JSONPATH_EXEC_RESULT_OK && !found)
                    break;
            }
        }
    }   break;
    case JSONPATH_INDEXARRAY:
        if (xson_value_type(jb) == XSON_TYPE_ARRAY) {
            elem = jsonpath_get_next(jsp);
            res = JSONPATH_EXEC_RESULT_NOT_FOUND;

            int32_t len = (int32_t)xson_value_array_size(jb);

            int32_t idx = jsp->value.array.idx;

            if (idx < 0)
                idx = len + idx;

            if ((idx < 0) || (idx >= len))
                break;

            xson_value_t *v = xson_value_array_at(jb, idx);
            if (v != NULL) {
                if ((elem == NULL) && !found)
                    return JSONPATH_EXEC_RESULT_OK;

                res = jsonpath_execute_next_item(ctx, jsp, elem, v, found);
            }
        } else {
            // FIXME
        }
        break;
    case JSONPATH_SLICE:
        if (xson_value_type(jb) == XSON_TYPE_ARRAY) {
            int32_t len = (int32_t)xson_value_array_size(jb);

            int32_t step = jsp->value.slice.step;
            if (step == 0)
                step = 1;

            int32_t start = jsp->value.slice.start;
            if (start == INT32_MAX)
                start = len - 1;
            else
                start = start >= 0 ? start : len + start;

            int32_t end = jsp->value.slice.end;
            if (end == INT32_MAX)
                end = len;
            else if (end == -INT32_MAX)
                end = -len - 1;
            else
                end = end >= 0 ? end : len + end;

            if (step >= 0) {
                start = MIN(MAX(start, 0), len);
                end = MIN(MAX(end, 0), len);
            } else {
                start = MIN(MAX(start, -1), len-1);
                end = MIN(MAX(end, -1), len-1);
            }

            elem = jsonpath_get_next(jsp);

            res = JSONPATH_EXEC_RESULT_NOT_FOUND;
            if (step >= 0) {
                for (int32_t i = start; i < end; i += step) {
                    xson_value_t *v = xson_value_array_at(jb, i);
                    if (v == NULL)
                        continue;

                    if ((elem == NULL) && !found)
                        return JSONPATH_EXEC_RESULT_OK;

                    res = jsonpath_execute_next_item(ctx, jsp, elem, v, found);

                    if (res == JSONPATH_EXEC_RESULT_ERROR)
                        break;

                    if (res == JSONPATH_EXEC_RESULT_OK && !found)
                        break;
                }
            } else {
                for (int32_t i = start; end < i; i += step) {
                    xson_value_t *v = xson_value_array_at(jb, i);
                    if (v == NULL)
                        continue;

                    if ((elem == NULL) && !found)
                        return JSONPATH_EXEC_RESULT_OK;

                    res = jsonpath_execute_next_item(ctx, jsp, elem, v, found);

                    if (res == JSONPATH_EXEC_RESULT_ERROR)
                        break;

                    if (res == JSONPATH_EXEC_RESULT_OK && !found)
                        break;
                }
            }

        }
        break;
    case JSONPATH_ANYKEY:
        if (xson_value_type(jb) == XSON_TYPE_OBJECT) {
            elem = jsonpath_get_next_follow(jsp);
            return jsonpath_exec_any_item(ctx, elem, jb, found, 1, 1, 1, true);
        } else if (unwrap && xson_value_type(jb) == XSON_TYPE_ARRAY) {
            elem = jsonpath_get_next_follow(jsp);
            return jsonpath_exec_any_item(ctx, elem, jb, found, 1, 1, 1, true);
        }
        break;
    case JSONPATH_ADD:
        return jsonpath_exec_binary_expr(ctx, jsp, jb, numeric_add_opt_error, found);
        break;
    case JSONPATH_SUB:
        return jsonpath_exec_binary_expr(ctx, jsp, jb, numeric_sub_opt_error, found);
        break;
    case JSONPATH_MUL:
        return jsonpath_exec_binary_expr(ctx, jsp, jb, numeric_mul_opt_error, found);
        break;
    case JSONPATH_DIV:
        return jsonpath_exec_binary_expr(ctx, jsp, jb, numeric_div_opt_error, found);
        break;
    case JSONPATH_MOD:
        return jsonpath_exec_binary_expr(ctx, jsp, jb, numeric_mod_opt_error, found);
        break;
    case JSONPATH_PLUS:
        return jsonpath_exec_unary_expr(ctx, jsp, jb, NULL, found);
        break;
    case JSONPATH_MINUS:
        return jsonpath_exec_unary_expr(ctx, jsp, jb, numeric_uminus, found);
        break;
    case JSONPATH_FILTER: {
        if (unwrap && xson_value_type(jb) == XSON_TYPE_ARRAY) {
            return jsonpath_exec_any_item(ctx, jsp, jb, found, 1, 1, 1, false);
        }
        if (unwrap && xson_value_type(jb) == XSON_TYPE_OBJECT) {
            return jsonpath_exec_any_item(ctx, jsp, jb, found, 1, 1, 1, false);
        }

        elem = jsonpath_get_arg(jsp);
        jsonpath_bool_t st = jsonpath_exec_nested_bool_item(ctx, elem, jb);
        if (st != JSONPATH_BOOL_TRUE) {
            res = JSONPATH_EXEC_RESULT_NOT_FOUND;
        } else {
            res = jsonpath_execute_next_item(ctx, jsp, NULL, jb, found);
        }
    }   break;
    case JSONPATH_NULL:
    case JSONPATH_BOOL:
    case JSONPATH_NUMERIC:
    case JSONPATH_STRING: {
        elem = jsonpath_get_next(jsp);
        if ((elem == NULL) && !found) {
            res = JSONPATH_EXEC_RESULT_OK;
            break;
        }

        xson_value_t jv = {0};
        jsonpath_set_item(ctx, jsp, &jv);

        res = jsonpath_execute_next_item(ctx, jsp, elem, &jv, found);
        if (xson_value_is_string(&jv))
            free(jv.string);

    }   break;
    case JSONPATH_LENGTH: {
        xson_value_list_t lseq = {0};

        size_t size = 0;
        res = jsonpath_exec_item_nothrow(ctx, jsp->value.arg, jb, &lseq);
        if (res != JSONPATH_EXEC_RESULT_ERROR) {
            if (xson_value_list_length(&lseq) == 1) {
                xson_value_t *lval = xson_value_list_head(&lseq);
                if (lval != NULL) {
                    if (xson_value_type(lval) == XSON_TYPE_ARRAY) {
                        size = xson_value_array_size(lval);
                    } else if (xson_value_type(lval) == XSON_TYPE_OBJECT) {
                        size = xson_value_object_size(lval);
                    } else if (xson_value_type(lval) == XSON_TYPE_STRING) {
                        if (lval->string != NULL)
                            size = strlen(lval->string); // FIXME unicode
                    }
                }
            }
        }

        xson_value_list_destroy(&lseq);

        xson_value_t xnum = {0};
        xson_value_set_number(&xnum, size);
        res = jsonpath_execute_next_item(ctx, jsp, NULL, &xnum, found);
    }   break;
    case JSONPATH_COUNT: {
        xson_value_list_t lseq = {0};

        size_t size = 0;
        res = jsonpath_exec_item_nothrow(ctx, jsp->value.arg, jb, &lseq);
        if (res != JSONPATH_EXEC_RESULT_ERROR) {
            size = xson_value_list_length(&lseq);
        }

        xson_value_list_destroy(&lseq);

        xson_value_t xnum = {0};
        xson_value_set_number(&xnum, size);
        res = jsonpath_execute_next_item(ctx, jsp, NULL, &xnum, found);
    }   break;
    case JSONPATH_VALUE: {
        xson_value_list_t lseq = {0};

        res = jsonpath_exec_item_nothrow(ctx, jsp->value.arg, jb, &lseq);
        if (res != JSONPATH_EXEC_RESULT_ERROR) {
            if (lseq.singleton != NULL) {
                res = jsonpath_execute_next_item(ctx, jsp, NULL, lseq.singleton, found);
            }
        }

        xson_value_list_destroy(&lseq);
    }   break;
    case JSONPATH_AVG: {
        xson_value_list_t lseq = {0};

        double sum = 0.0;
        size_t n = 0;

        res = jsonpath_exec_item(ctx, jsp->value.arg, jb, &lseq);
        if (res != JSONPATH_EXEC_RESULT_ERROR) {
            int len = xson_value_list_length(&lseq);
            if (len == 1) {
                xson_value_t *lval = xson_value_list_head(&lseq);
                if (lval != NULL) {
                    if (xson_value_type(lval) == XSON_TYPE_ARRAY) {
                        for (size_t i = 0; i < lval->array.len; i++) {
                            xson_value_t *nv = &lval->array.values[i];
                            if (nv->type != XSON_TYPE_NUMBER)
                                continue;
                            sum += nv->number;
                            n++;
                        }
                    } else if (xson_value_type(lval) == XSON_TYPE_NUMBER) {
                        sum = lval->number;
                        n = 1;
                    }
                }
            } else if (len > 1) {
                xson_value_list_iterator_t lseqit;
                xson_value_t *lval;
                xson_value_list_init_iterator(&lseq, &lseqit);
                while ((lval = xson_value_list_next(&lseq, &lseqit))) {
                    if (lval->type == XSON_TYPE_OBJECT) {
                    }
                    if (lval->type != XSON_TYPE_NUMBER)
                        continue;

                    sum += lval->number;
                    n++;
                }
            }
        }

        xson_value_list_destroy(&lseq);

        sum = sum / n;

        xson_value_t xnum = {0};
        xson_value_set_number(&xnum, sum);
        res = jsonpath_execute_next_item(ctx, jsp, NULL, &xnum, found);
    }   break;
    case JSONPATH_MAX: {
        xson_value_list_t lseq = {0};

        double max = INFINITY;

        res = jsonpath_exec_item(ctx, jsp->value.arg, jb, &lseq);
        if (res != JSONPATH_EXEC_RESULT_ERROR) {
            int len = xson_value_list_length(&lseq);
            if (len == 1) {
                xson_value_t *lval = xson_value_list_head(&lseq);
                if (lval != NULL) {
                    if (xson_value_type(lval) == XSON_TYPE_ARRAY) {
                        for (size_t i = 0; i < lval->array.len; i++) {
                            xson_value_t *nv = &lval->array.values[i];
                            if (nv->type != XSON_TYPE_NUMBER)
                                continue;
                            if (max == INFINITY) {
                                max = nv->number;
                            } else if (nv->number > max) {
                                max = nv->number;
                            }
                        }
                    } else if (xson_value_type(lval) == XSON_TYPE_NUMBER) {
                        max = lval->number;
                    }
                }
            } else if (len > 1) {
                xson_value_list_iterator_t lseqit;
                xson_value_t *lval;
                xson_value_list_init_iterator(&lseq, &lseqit);
                while ((lval = xson_value_list_next(&lseq, &lseqit))) {
                    if (lval->type == XSON_TYPE_OBJECT) {
                    }
                    if (lval->type != XSON_TYPE_NUMBER)
                        continue;
                    if (max == INFINITY) {
                        max = lval->number;
                    } else if (lval->number > max) {
                        max = lval->number;
                    }

                }
            }
        }

        xson_value_list_destroy(&lseq);

        xson_value_t xnum = {0};
        xson_value_set_number(&xnum, max);
        res = jsonpath_execute_next_item(ctx, jsp, NULL, &xnum, found);
    }   break;
    case JSONPATH_MIN: {
        xson_value_list_t lseq = {0};

        double min = INFINITY;

        res = jsonpath_exec_item_nothrow(ctx, jsp->value.arg, jb, &lseq);
        if (res != JSONPATH_EXEC_RESULT_ERROR) {
            int len = xson_value_list_length(&lseq);
            if (len == 1) {
                xson_value_t *lval = xson_value_list_head(&lseq);
                if (lval != NULL) {
                    if (xson_value_type(lval) == XSON_TYPE_ARRAY) {
                        for (size_t i = 0; i < lval->array.len; i++) {
                            xson_value_t *nv = &lval->array.values[i];
                            if (nv->type != XSON_TYPE_NUMBER)
                                continue;
                            if (nv->number < min)
                                min = nv->number;
                        }
                    } else if (xson_value_type(lval) == XSON_TYPE_NUMBER) {
                        min = lval->number;
                    }
                }
            } else if (len > 1) {
                xson_value_list_iterator_t lseqit;
                xson_value_t *lval;
                xson_value_list_init_iterator(&lseq, &lseqit);
                while ((lval = xson_value_list_next(&lseq, &lseqit))) {
                    if (lval->type != XSON_TYPE_NUMBER)
                        continue;
                    if (lval->number < min)
                        min = lval->number;

                }
            }
            xson_value_list_destroy(&lseq);
        }

        if (min != INFINITY) {
            xson_value_t xnum = {0};
            xson_value_set_number(&xnum, min);
            res = jsonpath_execute_next_item(ctx, jsp, NULL, &xnum, found);
        }

    }   break;
    case JSONPATH_ABS:
        return jsonpath_exec_numeric_item(ctx, jsp, jb, numeric_abs, found);
        break;
    case JSONPATH_FLOOR:
        return jsonpath_exec_numeric_item(ctx, jsp, jb, numeric_floor, found);
        break;
    case JSONPATH_CEIL:
        return jsonpath_exec_numeric_item(ctx, jsp, jb, numeric_ceil, found);
        break;
    case JSONPATH_DOUBLE: {
        xson_value_list_t lseq = {0};
        res = jsonpath_exec_item_nothrow(ctx, jsp->value.arg, jb, &lseq);
        if (res != JSONPATH_EXEC_RESULT_ERROR) {
            xson_value_list_iterator_t lseqit;
            xson_value_t *lval;
            xson_value_list_init_iterator(&lseq, &lseqit);
            while ((lval = xson_value_list_next(&lseq, &lseqit))) {
                if (lval->type == XSON_TYPE_NUMBER) {
                    res = jsonpath_execute_next_item(ctx, jsp, NULL, lval, found);
                } else if (lval->type == XSON_TYPE_STRING) {
                    double num =  atof(lval->string);     
                    xson_value_t xnum = {0};
                    xson_value_set_number(&xnum, num);
                    res = jsonpath_execute_next_item(ctx, jsp, NULL, &xnum, found);
                } else {
                    jsonpath_error(ctx, "jsonpath double method can only convert strings or numbers");
                    res = JSONPATH_EXEC_RESULT_ERROR;
                }

                if (res == JSONPATH_EXEC_RESULT_ERROR)
                    break;
            }
        }
        xson_value_list_destroy(&lseq);
    }   break;
    }

    return res;
}

jsonpath_exec_result_t jsonpath_exec(jsonpath_item_t *jsp, xson_value_t *jbv,
                                     xson_value_list_t *result,
                                     char *buffer_error, size_t buffer_error_size)
{
    jsonpath_exec_ctx_t ctx = {0};

    ctx.root = jbv;
    ctx.current = jbv;
    ctx.throw_errors = true;
    ctx.buffer_error = buffer_error;
    ctx.buffer_error_size = buffer_error_size;

    jsonpath_exec_result_t res = jsonpath_exec_item(&ctx, jsp, jbv, result);
    if (res == JSONPATH_EXEC_RESULT_ERROR)
        return res;

    return xson_value_list_is_empty(result) ? JSONPATH_EXEC_RESULT_NOT_FOUND
                                            : JSONPATH_EXEC_RESULT_OK;
}
