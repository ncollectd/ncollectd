// SPDX-License-Identifier: GPL-2.0-only OR PostgreSQL
// SPDX-FileCopyrightText: Copyright (c) 2019-2023, PostgreSQL Global Development Group
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <string.h>
#include <stdlib.h>
#include <regex.h>
#include <math.h>

#include "jsonpath.h"

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

/*
 * Context of jsonpath execution.
 */
typedef struct {
    json_value_t *root;            /* for $ evaluation */
    json_value_t *current;         /* for @ evaluation */
    bool throwErrors;              /* with "false" all suppressible errors are * suppressed */
} jsonpath_exec_ctx_t;

/* Result of jsonpath predicate evaluation */
typedef enum {
    JSONPATH_BOOL_FALSE   = 0,
    JSONPATH_BOOL_TRUE    = 1,
    JSONPATH_BOOL_UNKNOWN = 2
} jsonpath_bool_t;

/* Result of jsonpath expression evaluation */

#define jperIsError(jper)            ((jper) == JSONPATH_EXEC_RESULT_ERROR)

/*
 * List of jsonb values with shortcut for single-value list.
 */

/* strict/lax flags is decomposed into four [un]wrap/error flags */
#define jspThrowErrors(cxt)             ((cxt)->throwErrors)

/* Convenience macro: return or throw error depending on context */
#define RETURN_ERROR(throw_error) \
do { \
    if (jspThrowErrors(cxt)) \
        throw_error; \
    else \
        return JSONPATH_EXEC_RESULT_ERROR; \
} while (0)

typedef jsonpath_bool_t (*jsonpath_pedicate_callback_t) (jsonpath_item_t *jsp,
                                                         json_value_t *larg,
                                                         json_value_t *rarg,
                                                         void *param);
typedef double (*binary_arithm_callback_t) (double num1, double num2, bool *error);
typedef double (*unary_arithm_callback_t) (double num1, bool *error);

static jsonpath_exec_result_t jsonpath_exec_item(jsonpath_exec_ctx_t *cxt,
                                                 jsonpath_item_t *jsp, json_value_t *jb, json_value_list_t *found);

static jsonpath_exec_result_t jsonpath_exec_item_opt_unwrap_target(jsonpath_exec_ctx_t *cxt,
                                                                   jsonpath_item_t *jsp, json_value_t *jb,
                                                                   json_value_list_t *found, bool unwrap);
static jsonpath_exec_result_t jsonpath_execute_next_item(jsonpath_exec_ctx_t *cxt,
                                                         jsonpath_item_t *cur, jsonpath_item_t *next,
                                                         json_value_t *v, json_value_list_t *found);

static jsonpath_exec_result_t jsonpath_exec_any_item(jsonpath_exec_ctx_t *cxt,
                                                     jsonpath_item_t *jsp, json_value_t *jbc, json_value_list_t *found,
                                                     uint32_t level, uint32_t first, uint32_t last,
                                                     bool unwrapNext);

static void json_value_list_append(json_value_list_t *jvl, json_value_t *jbv)
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

static int json_value_list_length(const json_value_list_t *jvl)
{
    return jvl->singleton ? 1 : jsonpath_list_length(jvl->list);
}

static bool json_value_list_is_empty(json_value_list_t *jvl)
{
    return !jvl->singleton && (jvl->list == NULL);
}

static json_value_t *json_value_list_head(json_value_list_t *jvl)
{
    return jvl->singleton ? jvl->singleton : jsonpath_list_initial(jvl->list);
}

#if 0
static jsonpath_list_t *json_value_list_get_list(json_value_list_t *jvl)
{
    if (jvl->singleton)
        return jsonpath_list_make1(jvl->singleton);

    return jvl->list;
}
#endif
void json_value_list_init_iterator(const json_value_list_t *jvl, json_value_list_iterator_t *it)
{
    if (jvl->singleton) {
        it->value = jvl->singleton;
        it->list = NULL;
        it->next = NULL;
    } else if (jvl->list != NULL) {
        it->value = (json_value_t *) jsonpath_list_initial(jvl->list);
        it->list = jvl->list;
        it->next = jsonpath_list_second_cell(jvl->list);
    } else {
        it->value = NULL;
        it->list = NULL;
        it->next = NULL;
    }
}

/*
 * Get the next item from the sequence advancing iterator.
 */
json_value_t *json_value_list_next(__attribute__((unused)) const json_value_list_t *jvl, json_value_list_iterator_t *it)
{
    json_value_t *result = it->value;

    if (it->next) {
        it->value = jsonpath_list_first(it->next);
        it->next = jsonpath_list_next(it->list, it->next);
    } else {
        it->value = NULL;
    }

    return result;
}

void json_value_list_destroy(json_value_list_t *jvl)
{
    if (jvl == NULL)
        return;

    if (jvl->singleton != NULL) {
        json_value_free(jvl->singleton);
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
        json_value_free(c->ptr_value);
    }
    jsonpath_list_free(jvl->list);
    jvl->list = NULL;
}

/* Get scalar of given type or NULL on type mismatch */
static json_value_t *getScalar(json_value_t *scalar, json_type_t type)
{
    /* Scalars should be always extracted during jsonpath execution. */
//    Assert(scalar->type != jbvBinary || !JsonContainerIsScalar(scalar->val.binary.data));

    return scalar->type == type ? scalar : NULL;
}

double numeric_add_opt_error(double num1, double num2, __attribute__((unused)) bool *error)
{
    return num1 + num2;
}

double numeric_sub_opt_error(double num1, double num2, __attribute__((unused)) bool *error)
{
    return num1 - num2;
}

double numeric_mul_opt_error(double num1, double num2, __attribute__((unused)) bool *error)
{
    return num1 * num2;
}

double numeric_div_opt_error(double num1, double num2, __attribute__((unused)) bool *error)
{
    return num1 / num2;
}

double numeric_mod_opt_error(double num1, double num2, __attribute__((unused)) bool *error)
{
    return (int64_t)num1 % (int64_t)num2;
}

double numeric_uminus(double num1, __attribute__((unused)) bool *error)
{
    return -num1;
}

double numeric_abs(double num1, __attribute__((unused)) bool *error)
{
    return fabs(num1);
}

double numeric_floor(double num1, __attribute__((unused)) bool *error)
{
    return floor(num1);
}

double numeric_ceil(double num1, __attribute__((unused)) bool *error)
{
    return ceil(num1);
}

/*
 * Convert jsonpath's scalar or variable node to actual jsonb value.
 *
 * If node is a variable then its id returned, otherwise 0 returned.
 */
static void getjsonpath_item(__attribute__((unused)) jsonpath_exec_ctx_t *cxt, jsonpath_item_t *item, json_value_t *value)
{
    switch (item->type) {
    case JSONPATH_NULL:
        json_value_set_null(value);
        break;
    case JSONPATH_BOOL:
        if (jsonpath_get_bool(item))
            json_value_set_true(value);
        else
            json_value_set_false(value);
        break;
    case JSONPATH_NUMERIC:
        json_value_set_number(value, jsonpath_get_numeric(item));
        break;
    case JSONPATH_STRING: {
        int len = 0;
        json_value_set_string(value, jsonpath_get_string(item, &len));
    }   break;
    default:
#if 0
        elog(ERROR, "unexpected jsonpath item type");
#endif
        break;
    }
}



static int jsonpath_exec_any_item_value(jsonpath_exec_ctx_t *cxt,
                                jsonpath_item_t *jsp, json_value_t *v,
                                json_value_list_t *found, uint32_t level, uint32_t first, uint32_t last,
                                bool unwrapNext,
                                jsonpath_exec_result_t *res)
{
//    if ((level >= first) || (first == UINT32_MAX && last == UINT32_MAX && json_value_is_scalar(v))) { /* leaves only requested */
    if (level >= first) {
        /* check expression */
        if (jsp) {
            *res = jsonpath_exec_item_opt_unwrap_target(cxt, jsp, v, found, unwrapNext);

            if (jperIsError(*res))
                return 1;

            if (*res == JSONPATH_EXEC_RESULT_OK && !found)
                return 1;
        } else if (found) {
            json_value_list_append(found, json_value_clone(v));
        } else {
            *res = JSONPATH_EXEC_RESULT_OK;
            return 0;
        }
    }

    if ((level < last) && !json_value_is_scalar(v)) {
        *res = jsonpath_exec_any_item (cxt, jsp, v, found, level + 1, first, last, unwrapNext);

        if (jperIsError(*res))
            return 1;

        if ((*res == JSONPATH_EXEC_RESULT_OK) && (found == NULL))
            return 1;
    }

    return -1;
}

static jsonpath_exec_result_t jsonpath_exec_any_item(jsonpath_exec_ctx_t *cxt,
                                             jsonpath_item_t *jsp, json_value_t *jbc,
                                             json_value_list_t *found, uint32_t level, uint32_t first, uint32_t last,
                                             bool unwrapNext)
{
    jsonpath_exec_result_t res = JSONPATH_EXEC_RESULT_NOT_FOUND;
//    JsonbIterator *it;
//    int32_t        r;
    int status = 0;

//    check_stack_depth();

    if (level > last)
        return res;

    json_value_t *v = jbc;
    switch (v->type) {
    case JSON_TYPE_NULL:
    case JSON_TYPE_STRING:
    case JSON_TYPE_NUMBER:
    case JSON_TYPE_TRUE:
    case JSON_TYPE_FALSE:
        status = jsonpath_exec_any_item_value(cxt, jsp, v, found, level, first, last, unwrapNext, &res);
        if (status >= 0)
            return res;
        break;
    case JSON_TYPE_OBJECT:
        for (size_t i = 0; i < v->object.len; i++) {
            json_value_t *nv = &v->object.keyval[i].value;
            status = jsonpath_exec_any_item_value(cxt, jsp, nv, found, level, first, last, unwrapNext, &res);
            if (status >= 0)
                return res;
        }
        break;
    case JSON_TYPE_ARRAY:
        for (size_t i = 0; i < v->array.len; i++) {
            json_value_t *nv = &v->array.values[i];
            status = jsonpath_exec_any_item_value(cxt, jsp, nv, found, level, first, last, unwrapNext, &res);
            if (status >= 0)
                return res;
        }
        break;
    }

#if 0
    it = JsonbIteratorInit(jbc);

    /*
     * Recursively iterate over jsonb objects/arrays
     */
    while ((r = JsonbIteratorNext(&it, &v, true)) != WJB_DONE) {
        if (r == WJB_KEY) {
            r = JsonbIteratorNext(&it, &v, true);
//            Assert(r == WJB_VALUE);
        }

        if (r == WJB_VALUE || r == WJB_ELEM) {

            if (level >= first ||
                (first == UINT32_MAX && last == UINT32_MAX &&
                 v.type != jbvBinary))  { /* leaves only requested */
                /* check expression */
                if (jsp) {
                    if (ignoreStructuralErrors) {
                        bool savedIgnoreStructuralErrors = cxt->ignoreStructuralErrors;
                        cxt->ignoreStructuralErrors = true;
                        res = jsonpath_exec_item_opt_unwrap_target(cxt, jsp, &v, found, unwrapNext);
                        cxt->ignoreStructuralErrors = savedIgnoreStructuralErrors;
                    } else {
                        res = jsonpath_exec_item_opt_unwrap_target(cxt, jsp, &v, found, unwrapNext);
                    }

                    if (jperIsError(res))
                        break;

                    if (res == JSONPATH_EXEC_RESULT_OK && !found)
                        break;
                } else if (found) {
                    json_value_list_append(found, copyJsonbValue(&v));
                } else {
                    return JSONPATH_EXEC_RESULT_OK;
                }
            }

            if (level < last && v.type == jbvBinary) {
                res = jsonpath_exec_any_item (cxt, jsp, &v, found, level + 1, first, last, ignoreStructuralErrors, unwrapNext);

                if (jperIsError(res))
                    break;

                if (res == JSONPATH_EXEC_RESULT_OK && found == NULL)
                    break;
            }
        }
    }
#endif
    return res;
}

/*
 * Same as jsonpath_exec_item(), but when "unwrap == true" automatically unwraps
 * each array item from the resulting sequence in lax mode.
 */
static jsonpath_exec_result_t jsonpath_exec_item_opt_unwrap_result(jsonpath_exec_ctx_t *cxt,
                                                                   jsonpath_item_t *jsp,
                                                                   json_value_t *jb,
                                                                   __attribute__((unused))  bool unwrap,
                                                                   json_value_list_t *found)
{
// FIXME
#if 0
//    if (unwrap && true) {
    if (unwrap) {
        json_value_list_t seq = {0};
        json_value_list_iterator_t it;
        jsonpath_exec_result_t res = jsonpath_exec_item(cxt, jsp, jb, &seq);
        json_value_t *item;

        if (jperIsError(res))
            return res;

        json_value_list_init_iterator(&seq, &it);
        while ((item = json_value_list_next(&seq, &it))) {
//            Assert(item->type != JSON_TYPE_ARRAY);

            if (json_value_type(item) == JSON_TYPE_ARRAY) {
                if (json_value_array_size(item) == 0)
                    json_value_list_append(found, item);
                else
                    jsonpath_exec_item_unwrap_target_array(cxt, NULL, item, found, false);
            } else if (json_value_type(item) == JSON_TYPE_OBJECT) {
                if (json_value_object_size(item) == 0)
                    json_value_list_append(found, item);
                else
                    jsonpath_exec_item_unwrap_target_array(cxt, NULL, item, found, false);
            } else {
                json_value_list_append(found, item);
            }
        }

        return JSONPATH_EXEC_RESULT_OK;
    }
#endif
    return jsonpath_exec_item(cxt, jsp, jb, found);
}
/*
 * Same as jsonpath_exec_item_opt_unwrap_result(), but with error suppression.
 */
static jsonpath_exec_result_t jsonpath_exec_item_opt_unwrap_result_nothrow(jsonpath_exec_ctx_t *cxt,
                                                                           jsonpath_item_t *jsp,
                                                                           json_value_t *jb, bool unwrap,
                                                                           json_value_list_t *found)
{
    bool throwErrors = cxt->throwErrors;
    cxt->throwErrors = false;
    jsonpath_exec_result_t res = jsonpath_exec_item_opt_unwrap_result(cxt, jsp, jb, unwrap, found);
    cxt->throwErrors = throwErrors;
    return res;
}

/*
 * Execute binary arithmetic expression on singleton numeric operands.
 * Array operands are automatically unwrapped in lax mode.
 */
static jsonpath_exec_result_t jsonpath_exec_binary_expr(jsonpath_exec_ctx_t *cxt, jsonpath_item_t *jsp,
                        json_value_t *jb, binary_arithm_callback_t func,
                        json_value_list_t *found)
{
    jsonpath_exec_result_t jper;
    jsonpath_item_t *elem;
    json_value_list_t lseq = {0};
    json_value_list_t rseq = {0};
    json_value_t *lval;
    json_value_t *rval;
    double rnum = 0;
    double lnum = 0;
    double res;


    elem = jsonpath_get_left_arg(jsp);
    /*
     * XXX: By standard only operands of multiplicative expressions are
     * unwrapped.  We extend it to other binary arithmetic expressions too.
     */
    jper = jsonpath_exec_item_opt_unwrap_result(cxt, elem, jb, true, &lseq);
    if (jperIsError(jper)) {
        json_value_list_destroy(&lseq);
        return jper;
    }

    if (json_value_list_length(&lseq) != 1 || !(lval = getScalar(json_value_list_head(&lseq), JSON_TYPE_NUMBER))) {
#if 0
        RETURN_ERROR(ereport(ERROR,
                             (errcode(ERRCODE_SINGLETON_SQL_JSON_ITEM_REQUIRED),
                              errmsg("left operand of jsonpath operator %s is not a single numeric value",
                                     jsonpath_operation_name(jsp->type)))));
#endif
        json_value_list_destroy(&lseq);
        return JSONPATH_EXEC_RESULT_ERROR;
    }

    lnum = lval->number;

    elem = jsonpath_get_right_arg(jsp);

    jper = jsonpath_exec_item_opt_unwrap_result(cxt, elem, jb, true, &rseq);
    if (jperIsError(jper)) {
        json_value_list_destroy(&lseq);
        json_value_list_destroy(&rseq);
        return jper;
    }


    if (json_value_list_length(&rseq) != 1 || !(rval = getScalar(json_value_list_head(&rseq), JSON_TYPE_NUMBER))) {
#if 0
        RETURN_ERROR(ereport(ERROR,
                             (errcode(ERRCODE_SINGLETON_SQL_JSON_ITEM_REQUIRED),
                              errmsg("right operand of jsonpath operator %s is not a single numeric value",
                                     jsonpath_operation_name(jsp->type)))));
#endif
        json_value_list_destroy(&lseq);
        json_value_list_destroy(&rseq);
        return JSONPATH_EXEC_RESULT_ERROR;
    }

    rnum = rval->number;

    if (jspThrowErrors(cxt)) {
//        res = func(lval->number, rval->number, NULL);
        res = func(lnum, rnum, NULL);
    } else {
        bool error = false;

//        res = func(lval->number, rval->number, &error);
        res = func(lnum, rnum, &error);

        if (error) {
            json_value_list_destroy(&lseq);
            json_value_list_destroy(&rseq);
            return JSONPATH_EXEC_RESULT_ERROR;
        }
    }

    elem = jsonpath_get_next(jsp);
    if ((elem == NULL) && !found) {
        json_value_list_destroy(&lseq);
        json_value_list_destroy(&rseq);
        return JSONPATH_EXEC_RESULT_OK;
    }

    json_value_list_destroy(&lseq);
    json_value_list_destroy(&rseq);

#if 0
    lval = calloc(1, sizeof(*lval));
    lval->type = JSON_TYPE_NUMBER;
    lval->number = res;
#endif
    json_value_t jbv = {0};
    json_value_set_number(&jbv, res);

    return jsonpath_execute_next_item(cxt, jsp, elem, &jbv, found);
}

/*
 * Execute unary arithmetic expression for each numeric item in its operand's
 * sequence.  Array operand is automatically unwrapped in lax mode.
 */
static jsonpath_exec_result_t jsonpath_exec_unary_expr(jsonpath_exec_ctx_t *cxt, jsonpath_item_t *jsp,
                       json_value_t *jb, unary_arithm_callback_t func, json_value_list_t *found)
{
    jsonpath_exec_result_t jper;
    jsonpath_exec_result_t jper2;
    jsonpath_item_t *elem;
    json_value_list_t seq = {0};
    json_value_list_iterator_t it;
    json_value_t *val;

    elem = jsonpath_get_arg(jsp);
    jper = jsonpath_exec_item_opt_unwrap_result(cxt, elem, jb, true, &seq);

    if (jperIsError(jper)) {
        json_value_list_destroy(&seq);
        return jper;
    }

    jper = JSONPATH_EXEC_RESULT_NOT_FOUND;

    elem = jsonpath_get_next(jsp);

    json_value_list_init_iterator(&seq, &it);
    while ((val = json_value_list_next(&seq, &it))) {
        if ((val = getScalar(val, JSON_TYPE_NUMBER))) {
            if (!found && (elem == NULL)) {
                json_value_list_destroy(&seq);
                return JSONPATH_EXEC_RESULT_OK;
            }
        } else {
            if (!found && elem == NULL)
                continue;        /* skip non-numerics processing */
#if 0
            RETURN_ERROR(ereport(ERROR,
                                 (errcode(ERRCODE_SQL_JSON_NUMBER_NOT_FOUND),
                                  errmsg("operand of unary jsonpath operator %s is not a numeric value",
                                         jsonpath_operation_name(jsp->type)))));
#endif
        }

        if (func) {
            bool error = false;
            val->number = func(val->number, &error); // FIXME
        }

        jper2 = jsonpath_execute_next_item(cxt, jsp, elem, val, found);

        if (jperIsError(jper2)) {
            json_value_list_destroy(&seq);
            return jper2;
        }

        if (jper2 == JSONPATH_EXEC_RESULT_OK) {
            if (!found) {
                json_value_list_destroy(&seq);
                return JSONPATH_EXEC_RESULT_OK;
            }
            jper = JSONPATH_EXEC_RESULT_OK;
        }
    }

    json_value_list_destroy(&seq);

    return jper;
}



/*
 * Unwrap current array item and execute jsonpath for each of its elements.
 */
static jsonpath_exec_result_t jsonpath_exec_item_unwrap_target_array(jsonpath_exec_ctx_t *cxt, jsonpath_item_t *jsp,
                             json_value_t *jb, json_value_list_t *found,
                             bool unwrapElements)
{
#if 0
    if (jb->type != jbvBinary) {
//        Assert(jb->type != JSON_TYPE_ARRAY);
        elog(ERROR, "invalid jsonb array value type: %d", jb->type);
    }
#endif

    return jsonpath_exec_any_item (cxt, jsp, jb, found, 1, 1, 1, unwrapElements);
}

/*
 * Execute next jsonpath item if exists.  Otherwise put "v" to the "found"
 * list if provided.
 */
static jsonpath_exec_result_t jsonpath_execute_next_item(jsonpath_exec_ctx_t *cxt,
                                                         jsonpath_item_t *cur, jsonpath_item_t *next,
                                                         json_value_t *v, json_value_list_t *found)
{
    bool hasNext = false;

    if (!cur) {
        hasNext = next != NULL;
    } else if (next) {
        hasNext = jsonpath_has_next(cur);
    } else {
        next = jsonpath_get_next(cur);
        hasNext = next != NULL;
    }

    if (hasNext)
        return jsonpath_exec_item(cxt, next, v, found);

    if (found != NULL) {
        json_value_list_append(found, json_value_clone(v));
    }

    return JSONPATH_EXEC_RESULT_OK;
}


/*
 * Execute unary or binary predicate.
 *
 * Predicates have existence semantics, because their operands are item
 * sequences.  Pairs of items from the left and right operand's sequences are
 * checked.  TRUE returned only if any pair satisfying the condition is found.
 * In strict mode, even if the desired pair has already been found, all pairs
 * still need to be examined to check the absence of errors.  If any error
 * occurs, UNKNOWN (analogous to SQL NULL) is returned.
 */
static jsonpath_bool_t jsonpath_exec_predicate(jsonpath_exec_ctx_t *cxt, jsonpath_item_t *pred,
                                               jsonpath_item_t *larg, jsonpath_item_t *rarg,
                                               json_value_t *jb,
                                               bool unwrapRightArg, jsonpath_pedicate_callback_t exec,
                                               void *param)
{
    jsonpath_exec_result_t res;
    json_value_list_iterator_t lseqit;
    json_value_list_t lseq = {0};
    json_value_list_t rseq = {0};
    json_value_t *lval;
    bool error = false;
    bool found = false;

    /* Left argument is always auto-unwrapped. */
    res = jsonpath_exec_item_opt_unwrap_result_nothrow(cxt, larg, jb, true, &lseq);
    if (jperIsError(res)) {
        json_value_list_destroy(&lseq);
        return JSONPATH_BOOL_UNKNOWN;
    }

    if (rarg) {
        /* Right argument is conditionally auto-unwrapped. */
        res = jsonpath_exec_item_opt_unwrap_result_nothrow(cxt, rarg, jb, unwrapRightArg, &rseq);
        if (jperIsError(res)) {
            json_value_list_destroy(&lseq);
            json_value_list_destroy(&rseq);
            return JSONPATH_BOOL_UNKNOWN;
        }
    }

    json_value_list_init_iterator(&lseq, &lseqit);
    while ((lval = json_value_list_next(&lseq, &lseqit))) {
        json_value_list_iterator_t rseqit;
        json_value_t *rval;
        bool first = true;
        json_value_list_init_iterator(&rseq, &rseqit);
        if (rarg)
            rval = json_value_list_next(&rseq, &rseqit);
        else
            rval = NULL;
        /* Loop over right arg sequence or do single pass otherwise */
        while (rarg ? (rval != NULL) : first) {
            if (exec != NULL) {
                jsonpath_bool_t bres = exec(pred, lval, rval, param);

                if (bres == JSONPATH_BOOL_UNKNOWN) {
#if 0
                    if (jspStrictAbsenseOfErrors(cxt)) {
                        json_value_list_destroy(&lseq);
                        json_value_list_destroy(&rseq);
                        return JSONPATH_BOOL_UNKNOWN;
                    }
#endif
                    error = true;
                } else if (bres == JSONPATH_BOOL_TRUE) {
#if 0
                    if (!jspStrictAbsenseOfErrors(cxt)) {
                        json_value_list_destroy(&lseq);
                        json_value_list_destroy(&rseq);
                        return JSONPATH_BOOL_TRUE;
                    }
#endif
                    found = true;
                }
            } else {
#if 0
                if (!jspStrictAbsenseOfErrors(cxt)) {
                    json_value_list_destroy(&lseq);
                    json_value_list_destroy(&rseq);
                    return JSONPATH_BOOL_TRUE;
                }
#endif
                found = true;
            }

            first = false;
            if (rarg)
                rval = json_value_list_next(&rseq, &rseqit);
        }
    }

    json_value_list_destroy(&lseq);
    json_value_list_destroy(&rseq);

    if (found)                   /* possible only in strict mode */
        return JSONPATH_BOOL_TRUE;

    if (error)                  /* possible only in lax mode */
        return JSONPATH_BOOL_UNKNOWN;

    return JSONPATH_BOOL_FALSE;
}

/* Comparison predicate callback. */
/*
 * Compare two SQL/JSON items using comparison operation 'op'.
 */
static jsonpath_bool_t jsonpath_compare_items(int32_t op, json_value_t *jb1, json_value_t *jb2)
{
    double cmp = 0;

    if ((jb1 == NULL) || (jb2 == NULL))
        return JSONPATH_BOOL_UNKNOWN;

    if (jb1->type != jb2->type) {
        if (jb1->type == JSON_TYPE_NULL || jb2->type == JSON_TYPE_NULL)

            /*
             * Equality and order comparison of nulls to non-nulls returns
             * always false, but inequality comparison returns true.
             */
            return op == JSONPATH_NOTEQUAL ? JSONPATH_BOOL_TRUE : JSONPATH_BOOL_FALSE;
        /* Non-null items of different types are not comparable. */
//        return JSONPATH_BOOL_UNKNOWN; // FIXME
        return JSONPATH_BOOL_FALSE;
    }

    switch (jb1->type) {
    case JSON_TYPE_NULL:
        cmp = 0;
        break;
    case JSON_TYPE_TRUE:
    case JSON_TYPE_FALSE: {
        bool b1 = json_value_is_true(jb1);
        bool b2 = json_value_is_true(jb2);
        cmp = b1 == b2 ? 0 : b1 ? 1 : -1;
    }   break;
    case JSON_TYPE_NUMBER:
        cmp = jb1->number - jb2->number; // FIXME
        break;
    case JSON_TYPE_STRING:
        cmp = strcmp(jb1->string, jb2->string);
        break;
    case JSON_TYPE_ARRAY:
    case JSON_TYPE_OBJECT:
        return JSONPATH_BOOL_UNKNOWN;    /* non-scalars are not comparable */
        break;
    default:
#if 0
        elog(ERROR, "invalid jsonb value type %d", jb1->type);
#endif
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
#if 0
        elog(ERROR, "unrecognized jsonpath operation: %d", op);
#endif
        return JSONPATH_BOOL_UNKNOWN;
    }

    return res ? JSONPATH_BOOL_TRUE : JSONPATH_BOOL_FALSE;
}

static jsonpath_bool_t executeComparison(jsonpath_item_t *cmp, json_value_t *lv, json_value_t *rv,
                                         __attribute__((unused)) void *p)
{
    return jsonpath_compare_items(cmp->type, lv, rv);
}


static jsonpath_bool_t executeRegex(jsonpath_item_t *jsp,
                                    json_value_t *str,
                                    __attribute__((unused)) json_value_t *rarg,
                                    __attribute__((unused)) void *param)
{
    if (!(str = getScalar(str, JSON_TYPE_STRING)))
        return JSONPATH_BOOL_UNKNOWN;

    int status = regexec(&jsp->value.regex.regex, str->string, 0, NULL, 0);
    if (status == 0)
        return JSONPATH_BOOL_TRUE;

    return JSONPATH_BOOL_FALSE;
}

/* Execute boolean-valued jsonpath expression. */
static jsonpath_bool_t jsonpath_exec_bool(jsonpath_exec_ctx_t *cxt, jsonpath_item_t *jsp,
                                       json_value_t *jb, bool canHaveNext)
{
    jsonpath_item_t *larg;
    jsonpath_item_t *rarg;
    jsonpath_bool_t res;
    jsonpath_bool_t res2;

    if (!canHaveNext && jsonpath_has_next(jsp)) {
#if 0
        elog(ERROR, "boolean jsonpath item cannot have next item");
#endif
    }

    switch (jsp->type) {
    case JSONPATH_AND:
        larg = jsonpath_get_left_arg(jsp);
        res = jsonpath_exec_bool(cxt, larg, jb, false);
        if (res == JSONPATH_BOOL_FALSE)
            return JSONPATH_BOOL_FALSE;
        /*
         * SQL/JSON says that we should check second arg in case of
         * JSONPATH_EXEC_RESULT_ERROR
         */
        rarg = jsonpath_get_right_arg(jsp);
        res2 = jsonpath_exec_bool(cxt, rarg, jb, false);

        return res2 == JSONPATH_BOOL_TRUE ? res : res2;
        break;
    case JSONPATH_OR:
        larg = jsonpath_get_left_arg(jsp);
        res = jsonpath_exec_bool(cxt, larg, jb, false);

        if (res == JSONPATH_BOOL_TRUE)
            return JSONPATH_BOOL_TRUE;

        rarg = jsonpath_get_right_arg(jsp);
        res2 = jsonpath_exec_bool(cxt, rarg, jb, false);

        return res2 == JSONPATH_BOOL_FALSE ? res : res2;
        break;
    case JSONPATH_NOT:
        larg = jsonpath_get_arg(jsp);
        res = jsonpath_exec_bool(cxt, larg, jb, false);

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
        return jsonpath_exec_predicate(cxt, jsp, larg, rarg, jb, true, executeComparison, cxt);
        break;
    case JSONPATH_REGEX:{
        larg = jsp->value.regex.expr;
        return jsonpath_exec_predicate(cxt, jsp, larg, NULL, jb, false, executeRegex, cxt);
    }   break;
    case JSONPATH_BOOL:
        return jsp->value.boolean ? JSONPATH_BOOL_TRUE : JSONPATH_BOOL_FALSE;
        break;
    case JSONPATH_ROOT:
        return jsonpath_exec_predicate(cxt, jsp, jsp, NULL, jb, false, NULL, cxt);
        break;
    case JSONPATH_CURRENT:
        return jsonpath_exec_predicate(cxt, jsp, jsp, NULL, jb, false, NULL, cxt);
        break;
    default:
#if 0
        elog(ERROR, "invalid boolean jsonpath item type: %d", jsp->type);
#endif
        return JSONPATH_BOOL_UNKNOWN;
        break;
    }
}

/*
 * Execute numeric item methods (.abs(), .floor(), .ceil()) using the specified
 * user function 'func'.
 */
static jsonpath_exec_result_t executeNumericItemMethod(jsonpath_exec_ctx_t *cxt, jsonpath_item_t *jsp,
                         json_value_t *jb, bool unwrap, unary_arithm_callback_t func,
                         json_value_list_t *found)
{
    jsonpath_item_t *next;

    if (unwrap && json_value_type(jb) == JSON_TYPE_ARRAY)
        return jsonpath_exec_item_unwrap_target_array(cxt, jsp, jb, found, false);

    if (!(jb = getScalar(jb, JSON_TYPE_NUMBER))) {
#if 0
        RETURN_ERROR(ereport(ERROR,
                             (errcode(ERRCODE_NON_NUMERIC_SQL_JSON_ITEM),
                              errmsg("jsonpath item method .%s() can only be applied to a numeric value",
                                     jsonpath_operation_name(jsp->type)))));
#endif
        return JSONPATH_EXEC_RESULT_ERROR;
    }

    double datum = func(jb->number, NULL);

    next = jsonpath_get_next(jsp);
    if ((next == NULL) && !found)
        return JSONPATH_EXEC_RESULT_OK;

#if 0
    jb = calloc(1, sizeof(*jb));
    jb->type = JSON_TYPE_NUMBER;
    jb->number = datum;
#endif
    json_value_t jbv = {0};
    json_value_set_number(&jbv, datum);

    return jsonpath_execute_next_item(cxt, jsp, next, jb, found);
}

static jsonpath_bool_t jsonpath_exec_nested_bool_item(jsonpath_exec_ctx_t *cxt,
                                                      jsonpath_item_t *jsp,
                                                      json_value_t *jb)
{
    json_value_t *prev;
    jsonpath_bool_t res;

    prev = cxt->current;
    cxt->current = jb;
    res = jsonpath_exec_bool(cxt, jsp, jb, false);
    cxt->current = prev;

    return res;
}

/*
 * Convert boolean execution status 'res' to a boolean JSON item and execute
 * next jsonpath.
 */
static jsonpath_exec_result_t jsonpath_append_bool_result(jsonpath_exec_ctx_t *cxt,
                                                          jsonpath_item_t *jsp,
                                                          json_value_list_t *found,
                                                          jsonpath_bool_t res)
{
    json_value_t jbv = {0};

    jsonpath_item_t *next = jsonpath_get_next(jsp);
    if ((next == NULL) && (found == NULL))
        return JSONPATH_EXEC_RESULT_OK;            /* found singleton boolean value */

    if (res == JSONPATH_BOOL_UNKNOWN) {
        json_value_set_false(&jbv);
//        json_value_set_null(&jbv); // FIXME
    } else {
        if (res == JSONPATH_BOOL_TRUE)
            json_value_set_true(&jbv);
        else
            json_value_set_false(&jbv);
    }

    return jsonpath_execute_next_item(cxt, jsp, next, &jbv, found);
}

/*
 * Execute jsonpath with automatic unwrapping of current item in lax mode.
 */
static jsonpath_exec_result_t jsonpath_exec_item(jsonpath_exec_ctx_t *cxt, jsonpath_item_t *jsp,
            json_value_t *jb, json_value_list_t *found)
{
    return jsonpath_exec_item_opt_unwrap_target(cxt, jsp, jb, found, true);
}

/*
 * Main jsonpath executor function: walks on jsonpath structure, finds
 * relevant parts of jsonb and evaluates expressions over them.
 * When 'unwrap' is true current SQL/JSON item is unwrapped if it is an array.
 */
static jsonpath_exec_result_t jsonpath_exec_item_opt_unwrap_target(jsonpath_exec_ctx_t *cxt,
                                                                   jsonpath_item_t *jsp,
                                                                   json_value_t *jb,
                                                                   json_value_list_t *found,
                                                                   bool unwrap)
{
    jsonpath_item_t *elem;
    jsonpath_exec_result_t res = JSONPATH_EXEC_RESULT_NOT_FOUND;
//    JsonBaseObjectInfo baseObject;

//    check_stack_depth();
//    CHECK_FOR_INTERRUPTS();

    switch (jsp->type) {
        /* all boolean item types: */
    case JSONPATH_AND:
    case JSONPATH_OR:
    case JSONPATH_NOT:
    case JSONPATH_EQUAL:
    case JSONPATH_NOTEQUAL:
    case JSONPATH_LESS:
    case JSONPATH_GREATER:
    case JSONPATH_LESSOREQUAL:
    case JSONPATH_GREATEROREQUAL:
    case JSONPATH_REGEX:{
        jsonpath_bool_t st = jsonpath_exec_bool(cxt, jsp, jb, true);

        res = jsonpath_append_bool_result(cxt, jsp, found, st);
    }   break;
    case JSONPATH_KEY:
        if (json_value_type(jb) == JSON_TYPE_OBJECT) {
            json_value_t *v;

            int32_t len = 0;
            char *key = jsonpath_get_string(jsp, &len); // FIXME

            v = json_value_object_find(jb, key);
            if (v != NULL)
                res = jsonpath_execute_next_item(cxt, jsp, NULL, v, found);
        }
        break;
    case JSONPATH_ROOT:
        jb = cxt->root;
#if 0
        baseObject = setBaseObject(cxt, jb, 0);
#endif
        res = jsonpath_execute_next_item(cxt, jsp, NULL, jb, found);
#if 0
        cxt->baseObject = baseObject;
#endif
        break;
    case JSONPATH_CURRENT:
        res = jsonpath_execute_next_item(cxt, jsp, NULL, cxt->current, found);
        break;
    case JSONPATH_ANYARRAY:
        if (json_value_type(jb) == JSON_TYPE_ARRAY) {
            elem = jsonpath_get_next(jsp);
            res = jsonpath_exec_item_unwrap_target_array(cxt, elem, jb, found, true);
        } else {
            res = jsonpath_execute_next_item(cxt, jsp, NULL, jb, found);
        }
        break;
    case JSONPATH_UNION: {
        res = JSONPATH_EXEC_RESULT_NOT_FOUND;
        for (int32_t i = 0; i < jsp->value.iunion.len; i++) {
            jsonpath_item_t *uelem = jsp->value.iunion.items[i];
            res = jsonpath_execute_next_item(cxt, NULL, uelem, jb, found);

            if (jperIsError(res))
                break;

            if (res == JSONPATH_EXEC_RESULT_OK && !found)
                break;
        }
    }   break;
    case JSONPATH_DSC_UNION: {
        res = JSONPATH_EXEC_RESULT_NOT_FOUND;
        for (int32_t i = 0; i < jsp->value.iunion.len; i++) {
            jsonpath_item_t *uelem = jsp->value.iunion.items[i];
//            res = jsonpath_execute_next_item(cxt, NULL, uelem, jb, found, false);
            if (!json_value_is_scalar(jb)) {
#if 1
                if (json_value_is_array(jb)) {
                    json_value_t a = {.type = JSON_TYPE_ARRAY, .array.len = 1, .array.values = jb };
                    res = jsonpath_exec_any_item (cxt, uelem, &a, found,
                                                  1, 1, UINT32_MAX,
                                                  true);
                } else if(json_value_is_object(jb)) {
                    json_value_t o = {.type = JSON_TYPE_OBJECT, .object.len = 1,
                                      .object.keyval = &(json_keyval_t){.key = "", .value = *jb}};
                    res = jsonpath_exec_any_item (cxt, uelem, &o, found,
                                                  1, 1, UINT32_MAX,
                                                  true);

                }
#endif
#if 0
                res = jsonpath_exec_any_item (cxt, uelem, jb, found,
                                              1, 1, UINT32_MAX,
                                              false, true);
#endif
                if (jperIsError(res))
                    break;

                if (res == JSONPATH_EXEC_RESULT_OK && !found)
                    break;
            }

        }
    }   break;
    case JSONPATH_INDEXARRAY:
//        if (json_value_type(jb) == JSON_TYPE_ARRAY || jspAutoWrap(cxt)) {
        if (json_value_type(jb) == JSON_TYPE_ARRAY) {
            elem = jsonpath_get_next(jsp);
            res = JSONPATH_EXEC_RESULT_NOT_FOUND;

            int32_t len = (int32_t)json_value_array_size(jb);

            int32_t idx = jsp->value.array.idx;

            if (idx < 0)
                idx = len + idx;

            if ((idx < 0) || (idx >= len))
                break;

//            idx = MIN(MAX(idx, 0), len);

            json_value_t *v = json_value_array_at(jb, idx);
            if (v != NULL) {
                if ((elem == NULL) && !found)
                    return JSONPATH_EXEC_RESULT_OK;

                res = jsonpath_execute_next_item(cxt, jsp, elem, v, found);
#if 0
            if (jperIsError(res))
                break;
            if (res == JSONPATH_EXEC_RESULT_OK && !found)
                break;
#endif
            }
        } else {
            // FIXME
        }
        break;
    case JSONPATH_SLICE:
//        if (json_value_type(jb) == JSON_TYPE_ARRAY || jspAutoWrap(cxt)) {
        if (json_value_type(jb) == JSON_TYPE_ARRAY) {
            int32_t len = (int32_t)json_value_array_size(jb);

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
                    json_value_t *v = json_value_array_at(jb, i);
                    if (v == NULL)
                        continue;

                    if ((elem == NULL) && !found)
                        return JSONPATH_EXEC_RESULT_OK;

                    res = jsonpath_execute_next_item(cxt, jsp, elem, v, found);

                    if (jperIsError(res))
                        break;

                    if (res == JSONPATH_EXEC_RESULT_OK && !found)
                        break;
                }
            } else {
                for (int32_t i = start; end < i; i += step) {
                    json_value_t *v = json_value_array_at(jb, i);
                    if (v == NULL)
                        continue;

                    if ((elem == NULL) && !found)
                        return JSONPATH_EXEC_RESULT_OK;

                    res = jsonpath_execute_next_item(cxt, jsp, elem, v, found);

                    if (jperIsError(res))
                        break;

                    if (res == JSONPATH_EXEC_RESULT_OK && !found)
                        break;
                }
            }

        }
        break;
#if 0
    case JSONPATH_LAST: {
        json_value_t    tmpjbv;
        json_value_t *lastjbv;
        int            last;
        elem = jsonpath_get_next(jsp);

        if (cxt->innermostArraySize < 0) {
#if 0
            elog(ERROR, "evaluating jsonpath LAST outside of array subscript");
#endif
        }

        if (elem == NULL && !found) {
            res = JSONPATH_EXEC_RESULT_OK;
            break;
        }

        last = cxt->innermostArraySize - 1;

        lastjbv = elem != NULL? &tmpjbv : calloc(1, sizeof(*lastjbv));

        lastjbv->type = JSON_TYPE_NUMBER;
        lastjbv->number = last; // FIXME

        res = jsonpath_execute_next_item(cxt, jsp, elem, lastjbv, found, elem != NULL ? true : false);
    }   break;
#endif
    case JSONPATH_ANYKEY:
        if (json_value_type(jb) == JSON_TYPE_OBJECT) {
            elem = jsonpath_get_next(jsp);
            return jsonpath_exec_any_item(cxt, elem, jb, found, 1, 1, 1, true);
        } else if (unwrap && json_value_type(jb) == JSON_TYPE_ARRAY) {
            elem = jsonpath_get_next(jsp);
//            return jsonpath_exec_item_unwrap_target_array(cxt, jsp, jb, found, true);
            return jsonpath_exec_item_unwrap_target_array(cxt, elem, jb, found, true);
        }
        break;
    case JSONPATH_ADD:
        return jsonpath_exec_binary_expr(cxt, jsp, jb, numeric_add_opt_error, found);
        break;
    case JSONPATH_SUB:
        return jsonpath_exec_binary_expr(cxt, jsp, jb, numeric_sub_opt_error, found);
        break;
    case JSONPATH_MUL:
        return jsonpath_exec_binary_expr(cxt, jsp, jb, numeric_mul_opt_error, found);
        break;
    case JSONPATH_DIV:
        return jsonpath_exec_binary_expr(cxt, jsp, jb, numeric_div_opt_error, found);
        break;
    case JSONPATH_MOD:
        return jsonpath_exec_binary_expr(cxt, jsp, jb, numeric_mod_opt_error, found);
        break;
    case JSONPATH_PLUS:
        return jsonpath_exec_unary_expr(cxt, jsp, jb, NULL, found);
        break;
    case JSONPATH_MINUS:
        return jsonpath_exec_unary_expr(cxt, jsp, jb, numeric_uminus, found);
        break;
    case JSONPATH_FILTER: {
        if (unwrap && json_value_type(jb) == JSON_TYPE_ARRAY) {
            return jsonpath_exec_item_unwrap_target_array(cxt, jsp, jb, found, false);
        }
        if (unwrap && json_value_type(jb) == JSON_TYPE_OBJECT) {
            return jsonpath_exec_item_unwrap_target_array(cxt, jsp, jb, found, false);
        }

        elem = jsonpath_get_arg(jsp);
        jsonpath_bool_t st = jsonpath_exec_nested_bool_item(cxt, elem, jb);
        if (st != JSONPATH_BOOL_TRUE) {
            res = JSONPATH_EXEC_RESULT_NOT_FOUND;
        } else {
            res = jsonpath_execute_next_item(cxt, jsp, NULL, jb, found);
        }
    }   break;
    case JSONPATH_NULL:
    case JSONPATH_BOOL:
    case JSONPATH_NUMERIC:
    case JSONPATH_STRING: {
        elem = jsonpath_get_next(jsp);
        if ((elem == NULL) && !found) {
            /*
             * Skip evaluation, but not for variables.  We must
             * trigger an error for the missing variable.
             */
            res = JSONPATH_EXEC_RESULT_OK;
            break;
        }

        // v = elem == NULL ? &vbuf : calloc(1, sizeof(*v));
//        baseObject = cxt->baseObject;
        json_value_t jv = {0};
        getjsonpath_item(cxt, jsp, &jv);

        res = jsonpath_execute_next_item(cxt, jsp, elem, &jv, found);
        if (json_value_is_string(&jv))
            free(jv.string);

//        cxt->baseObject = baseObject;
    }   break;
    case JSONPATH_LENGTH: {
        json_value_list_t lseq = {0};

        size_t size = 0;

    /* Left argument is always auto-unwrapped. */
        res = jsonpath_exec_item_opt_unwrap_result_nothrow(cxt, jsp->value.arg, jb, true, &lseq);
        if (!jperIsError(res)) {
            if (json_value_list_length(&lseq) == 1) {
                json_value_t *lval = json_value_list_head(&lseq);
                if (lval != NULL) {
                    if (json_value_type(lval) == JSON_TYPE_ARRAY) {
                        size = json_value_array_size(lval);
                    } else if (json_value_type(lval) == JSON_TYPE_OBJECT) {
                        size = json_value_object_size(lval);
                    } else if (json_value_type(lval) == JSON_TYPE_STRING) {
                        if (lval->string != NULL)
                            size = strlen(lval->string); // FIXME
                    }
                }
            }
        }

        json_value_list_destroy(&lseq);

        jb = json_value_alloc_number(size);
        res = jsonpath_execute_next_item(cxt, jsp, NULL, jb, found);
        json_value_free(jb); // FIXME
    }   break;
    case JSONPATH_COUNT: {
        json_value_list_t lseq = {0};

        size_t size = 0;

    /* Left argument is always auto-unwrapped. */
        res = jsonpath_exec_item_opt_unwrap_result_nothrow(cxt, jsp->value.arg, jb, true, &lseq);
        if (!jperIsError(res)) {
            size = json_value_list_length(&lseq);
        }

        json_value_list_destroy(&lseq);

        jb = json_value_alloc_number(size);
        res = jsonpath_execute_next_item(cxt, jsp, NULL, jb, found);
        json_value_free(jb); // FIXME

    }   break;
#if 0
    case JSONPATH_TYPE: {
        json_value_t *jbv = json_value_alloc_string(json_value_type_name(jb));

        res = jsonpath_execute_next_item(cxt, jsp, NULL, jbv, found);
    }   break;
#endif
#if 0
    case JSONPATH_SIZE: {

        size_t size = 0;
        if (!json_value_is_array(jb)) {
            if (!jspAutoWrap(cxt)) {
                if (!jspIgnoreStructuralErrors(cxt)) {
#if 0
                    RETURN_ERROR(ereport(ERROR,
                                         (errcode(ERRCODE_SQL_JSON_ARRAY_NOT_FOUND),
                                          errmsg("jsonpath item method .%s() can only be applied to an array",
                                                 jsonpath_operation_name(jsp->type)))));
#endif
                }
                break;
            }

            size = 1;
        } else {
            size = json_value_array_size(jb);
        }

        jb = json_value_alloc_number(size);

        res = jsonpath_execute_next_item(cxt, jsp, NULL, jb, found);
    }   break;
#endif
    case JSONPATH_AVG:

        break;
    case JSONPATH_MAX: {
        json_value_list_t lseq = {0};

        double max = INFINITY;

    /* Left argument is always auto-unwrapped. */
        res = jsonpath_exec_item_opt_unwrap_result_nothrow(cxt, jsp->value.arg, jb, true, &lseq);
        if (!jperIsError(res)) {
            int len = json_value_list_length(&lseq);
            if (len == 1) {
                json_value_t *lval = json_value_list_head(&lseq);
                if (lval != NULL) {
                    if (json_value_type(lval) == JSON_TYPE_ARRAY) {
                        for (size_t i = 0; i < lval->array.len; i++) {
                            json_value_t *nv = &lval->array.values[i];
                            if (nv->type != JSON_TYPE_NUMBER)
                                continue;
                            if (max == INFINITY) {
                                max = nv->number;
                            } else if (nv->number > max) {
                                max = nv->number;
                            }
                        }
                    } else if (json_value_type(lval) == JSON_TYPE_NUMBER) {
                        max = lval->number;
                    }
                }
            } else if (len > 1) {
                json_value_list_iterator_t lseqit;
                json_value_t *lval;
                json_value_list_init_iterator(&lseq, &lseqit);
                while ((lval = json_value_list_next(&lseq, &lseqit))) {
                    if (lval->type == JSON_TYPE_OBJECT) {
//                        for (size_t i = 0; i < lval->object.len; i++)
//                            fprintf(stderr, "[%s]\n", lval->object.keyval[i].key);
                    }
                    if (lval->type != JSON_TYPE_NUMBER)
                        continue;
                    if (max == INFINITY) {
                        max = lval->number;
                    } else if (lval->number > max) {
                        max = lval->number;
                    }

                }
            }
        }

        json_value_list_destroy(&lseq);

        jb = json_value_alloc_number(max);
        res = jsonpath_execute_next_item(cxt, jsp, NULL, jb, found);
        json_value_free(jb); // FIXME
    }   break;
    case JSONPATH_MIN: {
        json_value_list_t lseq = {0};

        double min = INFINITY;

    /* Left argument is always auto-unwrapped. */
        res = jsonpath_exec_item_opt_unwrap_result_nothrow(cxt, jsp->value.arg, jb, true, &lseq);
        if (!jperIsError(res)) {
            int len = json_value_list_length(&lseq);
// fprintf(stderr, "* min [%d]\n", len);
            if (len == 1) {
                json_value_t *lval = json_value_list_head(&lseq);
                if (lval != NULL) {
                    if (json_value_type(lval) == JSON_TYPE_ARRAY) {
                        for (size_t i = 0; i < lval->array.len; i++) {
                            json_value_t *nv = &lval->array.values[i];
                            if (nv->type != JSON_TYPE_NUMBER)
                                continue;
                            if (nv->number < min)
                                min = nv->number;
                        }
                    } else if (json_value_type(lval) == JSON_TYPE_NUMBER) {
                        min = lval->number;
                    }
                }
            } else if (len > 1) {
                json_value_list_iterator_t lseqit;
                json_value_t *lval;
                json_value_list_init_iterator(&lseq, &lseqit);
                while ((lval = json_value_list_next(&lseq, &lseqit))) {
// fprintf(stderr, "* min 1 type: %d\n", lval->type);
                    if (lval->type != JSON_TYPE_NUMBER)
                        continue;
// fprintf(stderr, "* min 2\n");
                    if (lval->number < min)
                        min = lval->number;
// fprintf(stderr, "* min 3 %f\n", min);

                }
            }
            json_value_list_destroy(&lseq);
        }

        if (min != INFINITY) {
            jb = json_value_alloc_number(min);
            res = jsonpath_execute_next_item(cxt, jsp, NULL, jb, found);
            json_value_free(jb); // FIXME
        }

    }   break;
    case JSONPATH_ABS:
        return executeNumericItemMethod(cxt, jsp, jb, unwrap, numeric_abs, found);
        break;
    case JSONPATH_FLOOR:
        return executeNumericItemMethod(cxt, jsp, jb, unwrap, numeric_floor, found);
        break;
    case JSONPATH_CEILING:
        return executeNumericItemMethod(cxt, jsp, jb, unwrap, numeric_ceil, found);
        break;
    case JSONPATH_DOUBLE: {
        json_value_t    jbv;

        if (unwrap && json_value_type(jb) == JSON_TYPE_ARRAY)
            return jsonpath_exec_item_unwrap_target_array(cxt, jsp, jb, found, false);

        if (jb->type == JSON_TYPE_NUMBER) {
            double val = jb->number;
            if (isinf(val) || isnan(val)) {
#if 0
                RETURN_ERROR(ereport(ERROR,
                                     (errcode(ERRCODE_NON_NUMERIC_SQL_JSON_ITEM),
                                      errmsg("numeric argument of jsonpath item method .%s() is out of range for type double precision",
                                             jsonpath_operation_name(jsp->type)))));
#endif
            }
            res = JSONPATH_EXEC_RESULT_OK;
        } else if (jb->type == JSON_TYPE_STRING) {
            /* cast string as double */
            double val = atof(jb->string); // FIXME

            if (isinf(val) || isnan(val)) {
#if 0
                RETURN_ERROR(ereport(ERROR,
                                     (errcode(ERRCODE_NON_NUMERIC_SQL_JSON_ITEM),
                                      errmsg("string argument of jsonpath item method .%s() is not a valid representation of a double precision number",
                                             jsonpath_operation_name(jsp->type)))));
#endif
            }

            jb = &jbv;
            json_value_set_number(jb, val); // FIXME
            res = JSONPATH_EXEC_RESULT_OK;
        }

        if (res == JSONPATH_EXEC_RESULT_NOT_FOUND) {
#if 0
            RETURN_ERROR(ereport(ERROR,
                                 (errcode(ERRCODE_NON_NUMERIC_SQL_JSON_ITEM),
                                  errmsg("jsonpath item method .%s() can only be applied to a string or numeric value",
                                         jsonpath_operation_name(jsp->type)))));
#endif
        }
        res = jsonpath_execute_next_item(cxt, jsp, NULL, jb, found);
    } break;
    default:
#if 0
        elog(ERROR, "unrecognized jsonpath item type: %d", jsp->type);
#endif
        break;
    }

    return res;
}

jsonpath_exec_result_t jsonpath_exec(jsonpath_item_t *jsp, json_value_t *json, bool throwErrors,
                                     json_value_list_t *result)
{
    jsonpath_exec_ctx_t cxt;
    jsonpath_exec_result_t res;
    json_value_t *jbv = json;

    cxt.root = jbv;
    cxt.current = jbv;
    cxt.throwErrors = throwErrors;

    res = jsonpath_exec_item(&cxt, jsp, jbv, result);

//  Assert(!throwErrors || !jperIsError(res));
    if (jperIsError(res))
        return res;

    return json_value_list_is_empty(result) ? JSONPATH_EXEC_RESULT_NOT_FOUND : JSONPATH_EXEC_RESULT_OK;
}
