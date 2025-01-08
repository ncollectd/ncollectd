// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "expr.h"
#include "libutils/dtoa.h"
#include "libutils/time.h"

void expr_value_free(expr_value_t *value)
{
    if (value == NULL)
        return;

    if (value->type == EXPR_VALUE_STRING)
        free(value->string);

    free(value);
}

expr_value_t *expr_value_alloc(expr_value_type_t type)
{
    expr_value_t *value = calloc(1, sizeof(*value));
    if (value == NULL)
        return NULL;

    value->type = type;

    return value;
}

expr_value_t *expr_value_clone(expr_value_t *value)
{
    expr_value_t *rvalue = expr_value_alloc(value->type);
    if (rvalue == NULL)
        return NULL;

    switch (value->type) {
    case EXPR_VALUE_NUMBER:
        rvalue->number = value->number;
        break;
    case EXPR_VALUE_STRING:
        rvalue->string = strdup(value->string);
        if (rvalue->string == NULL) {
            free(rvalue);
            return NULL;
        }
        break;
    case EXPR_VALUE_BOOLEAN:
        rvalue->boolean = value->boolean;
        break;
    }

    return rvalue;
}

expr_value_t *expr_value_alloc_bool(bool boolean)
{
    expr_value_t *value = expr_value_alloc(EXPR_VALUE_BOOLEAN);
    if (value == NULL)
        return NULL;

    value->boolean = boolean;

    return value;
}

expr_value_t *expr_value_alloc_number(double number)
{
    expr_value_t *value = expr_value_alloc(EXPR_VALUE_NUMBER);
    if (value == NULL)
        return NULL;

    value->number = number;

    return value;
}

expr_value_t *expr_value_alloc_string(char *str)
{
    expr_value_t *value = expr_value_alloc(EXPR_VALUE_STRING);
    if (value == NULL)
        return NULL;

    value->string = strdup(str);

    return value;
}

bool expr_value_to_bool(expr_value_t *value)
{
    if (value == NULL)
        return false;

    bool boolean = false;
    switch (value->type) {
    case EXPR_VALUE_NUMBER:
        boolean = value->number == 0.0 ? false: true;
        break;
    case EXPR_VALUE_STRING: {
        errno = 0;
        char *endptr = NULL;
        double num = strtod(value->string, &endptr);
        if (errno == 0)
            boolean = false;
        else if ((endptr != NULL) && (*endptr != '\0'))
            boolean = false;
        else
            boolean = num == 0.0 ? false: true;
    }   break;
    case EXPR_VALUE_BOOLEAN:
        boolean = value->boolean;
        break;
    }

    expr_value_free(value);
    return boolean;
}

double expr_value_to_number(expr_value_t *value)
{
    if (value == NULL)
        return NAN;

    double num = NAN;
    switch (value->type) {
    case EXPR_VALUE_NUMBER:
        num = value->number;
        break;
    case EXPR_VALUE_STRING:
        if (value->string == NULL) {
            num = NAN;
        } else {
            errno = 0;
            char *endptr = NULL;
            num = (double)strtod(value->string, &endptr);
            if (errno != 0) {
                num = NAN;
            } else if ((endptr != NULL) && (*endptr != '\0')) {
                num = NAN;
            }
        }
        break;
    case EXPR_VALUE_BOOLEAN:
        num = value->boolean ? 1.0 : 0.0;
        break;
    }

    expr_value_free(value);
    return num;
}

static bool expr_eval_match(expr_node_t *node)
{
    expr_value_t *lval = expr_eval(node->regex_expr);
    if (lval == NULL) {
        return  node->type == EXPR_MATCH ? false : true;
    }
    if (lval->type != EXPR_VALUE_STRING) {
        expr_value_free(lval);
        return  node->type == EXPR_MATCH ? false : true;
    }

    int status = regexec(&node->regex, lval->string, 0, NULL, 0);
    if (status == 0) {
        expr_value_free(lval);
        return node->type == EXPR_MATCH ? true : false;
    }

    expr_value_free(lval);
    return node->type == EXPR_MATCH ? false : true;
}

static expr_value_t *expr_eval_binary(expr_node_t *node)
{
    double lnum = expr_value_to_number(expr_eval(node->left));
    double rnum = expr_value_to_number(expr_eval(node->right));

    if ((lnum == NAN) || (rnum == NAN))
        return expr_value_alloc_number(NAN);

    double num = 0;

    switch (node->type) {
    case EXPR_ADD:
        num = lnum + rnum;
        break;
    case EXPR_SUB:
        num = lnum - rnum;
        break;
    case EXPR_MUL:
        num = lnum * rnum;
        break;
    case EXPR_DIV:
        if (rnum == 0)
            num = NAN;
        else
            num = lnum / rnum;
        break;
    case EXPR_MOD:
        if (rnum == 0)
           num = NAN;
        else
            num = (long long int)lnum % (long long int)rnum;
        break;
    case EXPR_BIT_AND:
    case EXPR_BIT_OR:
    case EXPR_BIT_XOR:
    case EXPR_BIT_LSHIFT:
    case EXPR_BIT_RSHIFT: {
        uint64_t lint = lnum;
        uint64_t rint = rnum;
        uint64_t inum = 0;
        switch (node->type) {
        case EXPR_BIT_AND:
            inum = lint & rint;
            break;
        case EXPR_BIT_OR:
            inum = lint | rint;
            break;
        case EXPR_BIT_XOR:
            inum = lint ^ rint;
            break;
        case EXPR_BIT_LSHIFT:
            inum = lint << rint;
            break;
        case EXPR_BIT_RSHIFT:
            inum = lint >> rint;
            break;
        default:
            break;
        };
        num = inum;
    }   break;
    default:
        break;
    }

    return expr_value_alloc_number(num);
}

static bool expr_eval_cmp(expr_node_t *node)
{
    expr_value_t *lval = expr_eval(node->left);
    if (lval == NULL)
        return false;

    expr_value_t *rval = expr_eval(node->right);
    if (rval == NULL) {
        expr_value_free(lval);
        return false;
    }

    double cmp = 0;
    bool res = false;

    switch (lval->type) {
    case EXPR_VALUE_NUMBER:
        switch (rval->type) {
        case EXPR_VALUE_NUMBER:
            cmp = lval->number - rval->number;
            break;
        case EXPR_VALUE_STRING: {
            errno = 0;
            char *endptr = NULL;
            double tmp = (double )strtod(rval->string, &endptr);
            if (errno != 0)
                goto exit;
            if ((endptr != NULL) && (*endptr != '\0'))
                goto exit;
            cmp = lval->number - tmp;
        }   break;
        case EXPR_VALUE_BOOLEAN: {
            bool lboolean = lval->number == 0 ? false : true;
            cmp = lboolean == rval->boolean ? 0 : lboolean ? 1 : -1;
        }   break;
        }
        break;
    case EXPR_VALUE_STRING:
        switch (rval->type) {
        case EXPR_VALUE_NUMBER: {
            char num[DTOA_MAX];
            dtoa(rval->number, num, sizeof(num));
            cmp = strcmp(rval->string, num);
        }   break;
        case EXPR_VALUE_STRING:
            cmp = strcmp(rval->string, lval->string);
            break;
        case EXPR_VALUE_BOOLEAN: {
            if (rval->boolean)
                cmp = strcmp(rval->string, "true");
            else
                cmp = strcmp(rval->string, "false");
        }   break;
        }
        break;
    case EXPR_VALUE_BOOLEAN:
        switch (rval->type) {
        case EXPR_VALUE_NUMBER: {
            bool boolean = rval->number == 0 ? false : true;
            cmp = lval->boolean == boolean ? 0 : lval->boolean ? 1 : -1;
        }   break;
        case EXPR_VALUE_STRING:
            if (strcmp("true", lval->string) == 0) {
                cmp = lval->boolean == true ? 0 : -1;
            } else if (strcmp("false", lval->string) == 0) {
                cmp = lval->boolean == false ? 0 : 1;
            } else {
                goto exit;
            }
            break;
        case EXPR_VALUE_BOOLEAN:
            cmp = lval->boolean == rval->boolean ? 0 : lval->boolean ? 1 : -1;
            break;
        }
        break;
    }

    switch (node->type) {
    case EXPR_EQL:
        res = (cmp == 0);
        break;
    case EXPR_NQL:
        res = (cmp != 0);
        break;
    case EXPR_LT:
        res = (cmp < 0);
        break;
    case EXPR_GT:
        res = (cmp > 0);
        break;
    case EXPR_LTE:
        res = (cmp <= 0);
        break;
    case EXPR_GTE:
        res = (cmp >= 0);
        break;
    default:
        break;
    }

exit:
    expr_value_free(rval);
    expr_value_free(lval);

    return res;
}

static bool expr_eval_bool(expr_node_t *node)
{
    switch (node->type) {
    case EXPR_AND: {
        bool lval = expr_eval_bool(node->left);
        if (lval == false)
            return false;

        bool rval = expr_eval_bool(node->right);
        return rval == true ? lval : rval;
    }   break;
    case EXPR_OR: {
        bool lval = expr_eval_bool(node->left);
        if (lval == true)
            return true;

        bool rval = expr_eval_bool(node->right);
        return rval == false ? lval : rval;
    }   break;
    case EXPR_NOT: {
        bool lval = expr_eval_bool(node->arg);
        return lval == true ? false : true;
    }   break;
    case EXPR_EQL:
    case EXPR_NQL:
    case EXPR_LT:
    case EXPR_GT:
    case EXPR_LTE:
    case EXPR_GTE:
        return expr_eval_cmp(node);
        break;
    case EXPR_MATCH:
    case EXPR_NMATCH:
        return expr_eval_match(node);
        break;
    case EXPR_BOOL:
        return node->boolean;
        break;
    case EXPR_NUMBER:
        return node->number == 0 ? false : true;
    default:
        return false;
        break;
    }

    return false;
}

static expr_value_t *expr_eval_if(expr_node_t *node)
{
    bool expr = expr_eval_bool(node->expr);

    if (expr)
        return expr_eval(node->expr_then);

    return expr_eval(node->expr_else);
}

static expr_value_t *expr_eval_minus(expr_node_t *node)
{
    expr_value_t *value = expr_eval(node->arg);
    if (value == NULL)
        return expr_value_alloc_number(NAN);

    double num = NAN;
    switch (value->type) {
    case EXPR_VALUE_NUMBER:
        num = -value->number;
        break;
    case EXPR_VALUE_STRING: {
        errno = 0;
        char *endptr = NULL;
        num = -(double)strtod(value->string, &endptr);
        if (errno == 0)
            num = NAN;
        else if ((endptr != NULL) && (*endptr != '\0'))
            num = NAN;
    }   break;
    case EXPR_VALUE_BOOLEAN:
        num = - (value->boolean ? 1.0 : 0.0);
        break;
    }

    expr_value_free(value);
    return expr_value_alloc_number(num);
}

static expr_value_t *expr_eval_bitwise_not(expr_node_t *node)
{
    expr_value_t *value = expr_eval(node->arg);
    if (value == NULL)
        return expr_value_alloc_number(NAN);

    double num = NAN;
    switch (value->type) {
    case EXPR_VALUE_NUMBER: {
        uint64_t ival = value->number;
        ival = ~ival;
        num = ival;
    }   break;
    case EXPR_VALUE_STRING: {
        errno = 0;
        char *endptr = NULL;
        num = strtod(value->string, &endptr);
        if (errno == 0) {
            num = NAN;
        } else if ((endptr != NULL) && (*endptr != '\0')) {
            num = NAN;
        } else {
            uint64_t ival = num;
            ival = ~ival;
            num = ival;
        }
    }   break;
    case EXPR_VALUE_BOOLEAN: {
        num = - (value->boolean ? 1.0 : 0.0);
        uint64_t ival = num;
        ival = ~ival;
        num = ival;
    }   break;
    }

    expr_value_free(value);
    return expr_value_alloc_number(num);
}

static expr_value_t *expr_eval_symtab_entry(expr_id_t *id, expr_symtab_entry_t *entry)
{
    if (entry == NULL)
        return NULL;

    switch (entry->type) {
    case EXPR_SYMTAB_VALUE:
        return expr_value_clone(&entry->value);
        break;
    case EXPR_SYMTAB_VALUE_REF:
        return expr_value_clone(entry->value_ref);
        break;
    case EXPR_SYMTAB_CALLBACK:
        return entry->cb(id, entry->data);
        break;
    }

    return NULL;
}

expr_value_t *expr_eval(expr_node_t *node)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
    case EXPR_STRING:
        return expr_value_alloc_string(node->string);
        break;
    case EXPR_NUMBER:
        return expr_value_alloc_number(node->number);
        break;
    case EXPR_BOOL:
        return expr_value_alloc_bool(node->boolean);
        break;
    case EXPR_IDENTIFIER:
        return expr_eval_symtab_entry(&(node->id), node->entry);
        break;
    case EXPR_AND:
    case EXPR_OR:
    case EXPR_NOT:
    case EXPR_EQL:
    case EXPR_NQL:
    case EXPR_LT:
    case EXPR_GT:
    case EXPR_LTE:
    case EXPR_GTE:
    case EXPR_MATCH:
    case EXPR_NMATCH:
        return expr_value_alloc_bool(expr_eval_bool(node));
        break;
    case EXPR_ADD:
    case EXPR_SUB:
    case EXPR_MUL:
    case EXPR_DIV:
    case EXPR_MOD:
    case EXPR_BIT_AND:
    case EXPR_BIT_OR:
    case EXPR_BIT_XOR:
    case EXPR_BIT_LSHIFT:
    case EXPR_BIT_RSHIFT:
        return expr_eval_binary(node);
        break;
    case EXPR_BIT_NOT:
        return expr_eval_bitwise_not(node);
        break;
    case EXPR_MINUS:
        return expr_eval_minus(node);
        break;
    case EXPR_IF:
        return expr_eval_if(node);
        break;
    case EXPR_FUNC_RANDOM:
        /* coverity[DC.WEAK_CRYPTO] */
        return expr_value_alloc_number(random());
        break;
    case EXPR_FUNC_TIME:
        return expr_value_alloc_number(CDTIME_T_TO_DOUBLE(cdtime()));
        break;
    case EXPR_FUNC_EXP:
    case EXPR_FUNC_EXPM1:
    case EXPR_FUNC_LOG:
    case EXPR_FUNC_LOG2:
    case EXPR_FUNC_LOG10:
    case EXPR_FUNC_LOG1P:
    case EXPR_FUNC_SQRT:
    case EXPR_FUNC_CBRT:
    case EXPR_FUNC_SIN:
    case EXPR_FUNC_COS:
    case EXPR_FUNC_TAN:
    case EXPR_FUNC_ASIN:
    case EXPR_FUNC_ACOS:
    case EXPR_FUNC_ATAN:
    case EXPR_FUNC_COSH:
    case EXPR_FUNC_SINH:
    case EXPR_FUNC_TANH:
    case EXPR_FUNC_CTANH:
    case EXPR_FUNC_ACOSH:
    case EXPR_FUNC_ASINH:
    case EXPR_FUNC_ATANH:
    case EXPR_FUNC_ABS:
    case EXPR_FUNC_CEIL:
    case EXPR_FUNC_FLOOR:
    case EXPR_FUNC_ROUND:
    case EXPR_FUNC_TRUNC: {
        double arg0 = expr_value_to_number(expr_eval(node->arg0));
        double num = 0.0;
        switch(node->type) {
        case EXPR_FUNC_EXP:
            num = exp(arg0);
            break;
        case EXPR_FUNC_EXPM1:
            num = expm1(arg0);
            break;
        case EXPR_FUNC_LOG:
            num = log(arg0);
            break;
        case EXPR_FUNC_LOG2:
            num = log2(arg0);
            break;
        case EXPR_FUNC_LOG10:
            num = log10(arg0);
            break;
        case EXPR_FUNC_LOG1P:
            num = log1p(arg0);
            break;
        case EXPR_FUNC_SQRT:
            num = sqrt(arg0);
            break;
        case EXPR_FUNC_CBRT:
            num = cbrt(arg0);
            break;
        case EXPR_FUNC_SIN:
            num = sin(arg0);
            break;
        case EXPR_FUNC_COS:
            num = cos(arg0);
            break;
        case EXPR_FUNC_TAN:
            num = tan(arg0);
            break;
        case EXPR_FUNC_ASIN:
            num = asin(arg0);
            break;
        case EXPR_FUNC_ACOS:
            num = acos(arg0);
            break;
        case EXPR_FUNC_ATAN:
            num = atan(arg0);
            break;
        case EXPR_FUNC_COSH:
            num = cosh(arg0);
            break;
        case EXPR_FUNC_SINH:
            num = sinh(arg0);
            break;
        case EXPR_FUNC_TANH:
            num = tanh(arg0);
            break;
        case EXPR_FUNC_ACOSH:
            num = acosh(arg0);
            break;
        case EXPR_FUNC_ASINH:
            num = asinh(arg0);
            break;
        case EXPR_FUNC_ATANH:
            num = atanh(arg0);
            break;
        case EXPR_FUNC_ABS:
            num = fabs(arg0);
            break;
        case EXPR_FUNC_CEIL:
            num = ceil(arg0);
            break;
        case EXPR_FUNC_FLOOR:
            num = floor(arg0);
            break;
        case EXPR_FUNC_ROUND:
            num = round(arg0);
            break;
        case EXPR_FUNC_TRUNC:
            num = trunc(arg0);
            break;
        default:
            break;
        }
        return expr_value_alloc_number(num);
    }   break;
    case EXPR_FUNC_ISNAN:
    case EXPR_FUNC_ISINF:
    case EXPR_FUNC_ISNORMAL: {
        double arg0 = expr_value_to_number(expr_eval(node->arg0));
        int cmp = 0;
        switch(node->type) {
        case EXPR_FUNC_ISNAN:
            cmp = isnan(arg0);
            break;
        case EXPR_FUNC_ISINF:
            cmp = isinf(arg0);
            break;
        case EXPR_FUNC_ISNORMAL:
            cmp = isnormal(arg0);
            break;
        default:
            break;
        }
        return expr_value_alloc_bool(cmp == 0 ? false : true);
    }   break;
    case EXPR_FUNC_POW:
    case EXPR_FUNC_ATAN2:
    case EXPR_FUNC_HYPOT:
    case EXPR_FUNC_MAX:
    case EXPR_FUNC_MIN: {
        double arg0 = expr_value_to_number(expr_eval(node->arg0));
        double arg1 = expr_value_to_number(expr_eval(node->arg1));
        double num = 0.0;
        switch(node->type) {
        case EXPR_FUNC_POW:
            num = pow(arg0, arg1);
            break;
        case EXPR_FUNC_ATAN2:
            num = atan2(arg0, arg1);
            break;
        case EXPR_FUNC_HYPOT:
            num = hypot(arg0, arg1);
            break;
        case EXPR_FUNC_MAX:
            num = arg0 > arg1 ? arg0 : arg1;
            break;
        case EXPR_FUNC_MIN:
            num = arg0 < arg1 ? arg0 : arg1;
            break;
        default:
            break;
        }
        return expr_value_alloc_number(num);
    }   break;
    case EXPR_AGG_SUM:
    case EXPR_AGG_AVG:
    case EXPR_AGG_ALL:
    case EXPR_AGG_ANY:

        break;
    }

    return NULL;
}
