// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <stdint.h>
#include <string.h>
#include <sys/uio.h>
#include <regex.h>

#include "log.h"
#include "libutils/strbuf.h"
#include "libexpr/expr.h"
#include "libexpr/parser.h"
#define YYSTYPE EXPR_YYSTYPE
#define YYLTYPE EXPR_YYLTYPE
#include "libexpr/scanner.h"

void expr_node_free(expr_node_t *node)
{
    if (node == NULL)
        return;

    switch (node->type) {
    case EXPR_STRING:
        free(node->string);
        break;
    case EXPR_NUMBER:
    case EXPR_BOOL:
        break;
    case EXPR_IDENTIFIER:
        for (size_t i = 0; i < node->id.num; i++) {
            if (node->id.ptr[i].type == EXPR_ID_NAME)
                free(node->id.ptr[i].name);
        }
        free(node->id.ptr);
        break;
    case EXPR_AND:
    case EXPR_OR:
    case EXPR_EQL:
    case EXPR_NQL:
    case EXPR_LT:
    case EXPR_GT:
    case EXPR_LTE:
    case EXPR_GTE:
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
        expr_node_free(node->left);
        expr_node_free(node->right);
        break;
    case EXPR_BIT_NOT:
    case EXPR_NOT:
    case EXPR_MINUS:
        expr_node_free(node->arg);
        break;
    case EXPR_MATCH:
    case EXPR_NMATCH:
        free(node->pattern);
        expr_node_free(node->regex_expr);
        regfree(&node->regex);
        break;
    case EXPR_IF:
        expr_node_free(node->expr);
        expr_node_free(node->expr_then);
        expr_node_free(node->expr_else);
        break;
    case EXPR_FUNC_RANDOM:
    case EXPR_FUNC_TIME:
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
    case EXPR_FUNC_TRUNC:
    case EXPR_FUNC_ISNAN:
    case EXPR_FUNC_ISINF:
    case EXPR_FUNC_ISNORMAL:
        expr_node_free(node->arg0);
        break;
    case EXPR_FUNC_POW:
    case EXPR_FUNC_HYPOT:
    case EXPR_FUNC_ATAN2:
    case EXPR_FUNC_MAX:
    case EXPR_FUNC_MIN:
        expr_node_free(node->arg0);
        expr_node_free(node->arg1);
        break;

    case EXPR_AGG_SUM:
    case EXPR_AGG_AVG:
    case EXPR_AGG_ALL:
    case EXPR_AGG_ANY:
        expr_node_free(node->loop_id);
        expr_node_free(node->loop_start);
        expr_node_free(node->loop_end);
        expr_node_free(node->loop_step);
        expr_node_free(node->loop_expr);
        break;
    }

    free(node);
}


expr_node_t *expr_parse(char *str, expr_symtab_t *symtab)
{
#if 0
    if ((str == NULL) || (symtab == NULL))
        return NULL;
#endif
    yyscan_t scanner;

    expr_yylex_init (&scanner);
//    expr_string_t scanstring = {0};
//    expr_yylex_init_extra(&scanstring, &scanner);

    YY_BUFFER_STATE  buffer = expr_yy_scan_string(str, scanner);

    expr_parse_result_t parse_result = {0};
    parse_result.root = NULL;
    parse_result.error = true;
    parse_result.symtab = symtab;
    if (expr_yyparse(scanner, &parse_result) != 0)  {
        ERROR("parse failed: '%s': %s", str, parse_result.error_msg);
    }

    expr_yy_delete_buffer(buffer, scanner);
    expr_yylex_destroy(scanner);
//    free(scanstring.val);

    if (parse_result.error == true) {
        fprintf(stderr, "**** error true\n");
        if (parse_result.root != NULL)
            expr_node_free(parse_result.root);
        return NULL;
    }

    return parse_result.root;
}
