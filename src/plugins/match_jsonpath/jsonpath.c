// SPDX-License-Identifier: GPL-2.0-only OR PostgreSQL
// SPDX-FileCopyrightText: Copyright (c) 2019-2023, PostgreSQL Global Development Group
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"

#include <sys/uio.h>
#include <regex.h>

#include "plugins/match_jsonpath/jsonpath.h"
#include "plugins/match_jsonpath/jsonpath_list.h"
#include "plugins/match_jsonpath/jsonpath_internal.h"
#include "plugins/match_jsonpath/jsonpath_gram.h"
#define YYSTYPE JSONPATH_YYSTYPE
#define YYLTYPE JSONPATH_YYLTYPE
#include "plugins/match_jsonpath/jsonpath_scan.h"

const char *jsonpath_item_name(jsonpath_item_type_t item)
{
    switch (item) {
    case JSONPATH_NULL:
        return "NULL";
    case JSONPATH_STRING:
        return "STRING";
    case JSONPATH_NUMERIC:
        return "NUMERIC";
    case JSONPATH_BOOL:
        return "BOOL";
    case JSONPATH_AND:
        return "AND";
    case JSONPATH_OR:
        return "OR";
    case JSONPATH_NOT:
        return "NOT";
    case JSONPATH_EQUAL:
        return "EQUAL";
    case JSONPATH_NOTEQUAL:
        return "NOTEQUAL";
    case JSONPATH_LESS:
        return "LESS";
    case JSONPATH_GREATER:
        return "GREATER";
    case JSONPATH_LESSOREQUAL:
        return "LESSOREQUAL";
    case JSONPATH_GREATEROREQUAL:
        return "GREATEROREQUAL";
    case JSONPATH_REGEX:
        return "REGEX";
    case JSONPATH_ADD:
        return "ADD";
    case JSONPATH_SUB:
        return "SUB";
    case JSONPATH_MUL:
        return "MUL";
    case JSONPATH_DIV:
        return "DIV";
    case JSONPATH_MOD:
        return "MOD";
    case JSONPATH_PLUS:
        return "PLUS";
    case JSONPATH_MINUS:
        return "MINUS";
    case JSONPATH_ANYKEY:
        return "ANYKEY";
    case JSONPATH_INDEXARRAY:
        return "INDEXARRAY";
    case JSONPATH_SLICE:
        return "SLICE";
    case JSONPATH_KEY:
        return "KEY";
    case JSONPATH_UNION:
        return "UNION";
    case JSONPATH_DSC_UNION:
        return "DSC_UNION";
    case JSONPATH_CURRENT:
        return "CURRENT";
    case JSONPATH_ROOT:
        return "ROOT";
    case JSONPATH_FILTER:
        return "FILTER";
    case JSONPATH_ABS:
        return "ABS";
    case JSONPATH_AVG:
        return "AVG";
    case JSONPATH_MAX:
        return "MAX";
    case JSONPATH_MIN:
        return "MIN";
    case JSONPATH_CEIL:
        return "CEIL";
    case JSONPATH_VALUE:
        return "VALUE";
    case JSONPATH_COUNT:
        return "COUNT";
    case JSONPATH_FLOOR:
        return "FLOOR";
    case JSONPATH_MATCH:
        return "MATCH";
    case JSONPATH_DOUBLE:
        return "DOUBLE";
    case JSONPATH_LENGTH:
        return "LENGTH";
    case JSONPATH_SEARCH:
        return "SEARCH";
    }

    return NULL;
}

const char * jsonpath_operation_name(jsonpath_item_type_t type)
{
    switch (type) {
    case JSONPATH_AND:
        return "&&";
    case JSONPATH_OR:
        return "||";
    case JSONPATH_EQUAL:
        return "==";
    case JSONPATH_NOTEQUAL:
        return "!=";
    case JSONPATH_LESS:
        return "<";
    case JSONPATH_GREATER:
        return ">";
    case JSONPATH_LESSOREQUAL:
        return "<=";
    case JSONPATH_GREATEROREQUAL:
        return ">=";
    case JSONPATH_PLUS:
    case JSONPATH_ADD:
        return "+";
    case JSONPATH_MINUS:
    case JSONPATH_SUB:
        return "-";
    case JSONPATH_MUL:
        return "*";
    case JSONPATH_DIV:
        return "/";
    case JSONPATH_MOD:
        return "%";
    case JSONPATH_ABS:
        return "abs";
    case JSONPATH_AVG:
        return "avg";
    case JSONPATH_MAX:
        return "max";
    case JSONPATH_MIN:
        return "min";
    case JSONPATH_CEIL:
        return "ceil";
    case JSONPATH_VALUE:
        return "value";
    case JSONPATH_COUNT:
        return "count";
    case JSONPATH_FLOOR:
        return "floor";
    case JSONPATH_MATCH:
        return "match";
    case JSONPATH_DOUBLE:
        return "double";
    case JSONPATH_LENGTH:
        return "length";
    case JSONPATH_SEARCH:
        return "search";
    default:
        return NULL;
    }
}

static int jsonpath_operation_priority(jsonpath_item_type_t op)
{
    switch (op) {
    case JSONPATH_OR:
        return 0;
    case JSONPATH_AND:
        return 1;
    case JSONPATH_EQUAL:
    case JSONPATH_NOTEQUAL:
    case JSONPATH_LESS:
    case JSONPATH_GREATER:
    case JSONPATH_LESSOREQUAL:
    case JSONPATH_GREATEROREQUAL:
        return 2;
    case JSONPATH_ADD:
    case JSONPATH_SUB:
        return 3;
    case JSONPATH_MUL:
    case JSONPATH_DIV:
    case JSONPATH_MOD:
        return 4;
    case JSONPATH_PLUS:
    case JSONPATH_MINUS:
        return 5;
    default:
        return 6;
    }
}

static inline bool jsonpath_operation_priority_lte(jsonpath_item_type_t op1,
                                                   jsonpath_item_type_t op2)
{
    return jsonpath_operation_priority(op1) <= jsonpath_operation_priority(op2);
}

void jsonpath_print_item(strbuf_t *buf, jsonpath_item_t *v, bool in_key, bool bsp, bool bracketes)
{
    jsonpath_item_t *elem;

    if (v == NULL)
        return;

    switch (v->type) {
    case JSONPATH_NULL:
        strbuf_putstr(buf, "null");
        break;
    case JSONPATH_KEY:
        if (in_key)
            strbuf_putchar(buf, '.');
        strbuf_putescape_json(buf, jsonpath_get_string(v, NULL));
        break;
    case JSONPATH_STRING:
        strbuf_putchar(buf, '"');
        strbuf_putescape_json(buf, jsonpath_get_string(v, NULL));
        strbuf_putchar(buf, '"');
        break;
    case JSONPATH_NUMERIC:
        if (jsonpath_has_next(v))
            strbuf_putchar(buf, '(');
        strbuf_putdouble(buf, jsonpath_get_numeric(v));
        if (jsonpath_has_next(v))
            strbuf_putchar(buf, ')');
        break;
    case JSONPATH_BOOL:
        if (jsonpath_get_bool(v))
            strbuf_putstr(buf, "true");
        else
            strbuf_putstr(buf, "false");
        break;
    case JSONPATH_AND:
    case JSONPATH_OR:
    case JSONPATH_EQUAL:
    case JSONPATH_NOTEQUAL:
    case JSONPATH_LESS:
    case JSONPATH_GREATER:
    case JSONPATH_LESSOREQUAL:
    case JSONPATH_GREATEROREQUAL:
    case JSONPATH_ADD:
    case JSONPATH_SUB:
    case JSONPATH_MUL:
    case JSONPATH_DIV:
    case JSONPATH_MOD:
        if (bracketes)
            strbuf_putchar(buf, '(');
        elem = jsonpath_get_left_arg(v);
        jsonpath_print_item(buf, elem, false, bsp,
                            jsonpath_operation_priority_lte(elem->type, v->type));
        if (bsp)
            strbuf_putchar(buf, ' ');
        else if ((v->type == JSONPATH_AND) || (v->type == JSONPATH_OR))
            strbuf_putchar(buf, ' ');

        strbuf_putstr(buf, jsonpath_operation_name(v->type));
        if (bsp)
            strbuf_putchar(buf, ' ');
        else if ((v->type == JSONPATH_AND) || (v->type == JSONPATH_OR))
            strbuf_putchar(buf, ' ');
        elem = jsonpath_get_right_arg(v);
        jsonpath_print_item(buf, elem, false, bsp,
                            jsonpath_operation_priority_lte(elem->type, v->type));
        if (bracketes)
            strbuf_putchar(buf, ')');
        break;
    case JSONPATH_REGEX:
        if (bracketes)
            strbuf_putchar(buf, '(');
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false, bsp,
                            jsonpath_operation_priority_lte(elem->type, v->type));
        if (bsp)
            strbuf_putstr(buf, "=~ \"");
        else
            strbuf_putstr(buf, "=~\"");
        strbuf_putescape_json(buf, v->value.regex.pattern);
        strbuf_putchar(buf, '\"');
        if (bracketes)
            strbuf_putchar(buf, ')');
        break;
    case JSONPATH_PLUS:
    case JSONPATH_MINUS:
        if (bracketes)
            strbuf_putchar(buf, '(');
        strbuf_putchar(buf, v->type == JSONPATH_PLUS ? '+' : '-');
        elem = jsonpath_get_arg(v);
        jsonpath_print_item(buf, elem, false, bsp,
                            jsonpath_operation_priority_lte(elem->type, v->type));
        if (bracketes)
            strbuf_putchar(buf, ')');
        break;
    case JSONPATH_UNION: {
        strbuf_putchar(buf, '[');
        if ((v->value.iunion.len == 1) && ((v->value.iunion.items[0]->type == JSONPATH_KEY) ||
            (v->value.iunion.items[0]->type == JSONPATH_ANYKEY))) {
            if (v->value.iunion.items[0]->type == JSONPATH_KEY) {
                strbuf_putchar(buf, '\'');
                strbuf_putescape_squote(buf, jsonpath_get_string(v->value.iunion.items[0], NULL));
                strbuf_putchar(buf, '\'');
            } else {
                strbuf_putchar(buf, '*');
            }
        } else {
            for (int32_t i = 0; i < v->value.iunion.len; i++) {
                if (i > 0)
                    strbuf_putchar(buf, ',');
                jsonpath_print_item(buf, v->value.iunion.items[i], false, bsp, true);
            }
        }
        strbuf_putchar(buf, ']');
    }   break;
    case JSONPATH_DSC_UNION: {
        strbuf_putstr(buf, "..[");
        if ((v->value.iunion.len == 1) && ((v->value.iunion.items[0]->type == JSONPATH_KEY) ||
            (v->value.iunion.items[0]->type == JSONPATH_ANYKEY))) {
            if (v->value.iunion.items[0]->type == JSONPATH_KEY) {
                strbuf_putchar(buf, '\'');
                strbuf_putescape_squote(buf, jsonpath_get_string(v->value.iunion.items[0], NULL));
                strbuf_putchar(buf, '\'');
            } else {
                strbuf_putchar(buf, '*');
            }
        } else {
            for (int32_t i = 0; i < v->value.iunion.len; i++) {
                if (i > 0)
                    strbuf_putchar(buf, ',');
                jsonpath_print_item(buf, v->value.iunion.items[i], false, bsp, true);
            }
        }
        strbuf_putchar(buf, ']');
    }   break;
    case JSONPATH_FILTER:
        strbuf_putstr(buf, "?(");
        elem = jsonpath_get_arg(v);
        jsonpath_print_item(buf, elem, false, bsp, false);
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_NOT:
        strbuf_putstr(buf, "!(");
        elem = jsonpath_get_arg(v);
        jsonpath_print_item(buf, elem, false, bsp, false);
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_CURRENT:
        strbuf_putchar(buf, '@');
        break;
    case JSONPATH_ROOT:
        strbuf_putchar(buf, '$');
        break;
    case JSONPATH_ANYKEY:
        if (in_key)
            strbuf_putchar(buf, '.');
        strbuf_putchar(buf, '*');
        break;
    case JSONPATH_INDEXARRAY:
        strbuf_putint(buf, v->value.array.idx);
        break;
    case JSONPATH_SLICE:
        if ((v->value.slice.start != 0) && (v->value.slice.start != INT32_MAX))
            strbuf_putint(buf, v->value.slice.start);
        strbuf_putchar(buf, ':');
        if ((v->value.slice.end != INT32_MAX) && (v->value.slice.end != -INT32_MAX))
            strbuf_putint(buf, v->value.slice.end);
        if (v->value.slice.step != 1) {
            strbuf_putchar(buf, ':');
            strbuf_putint(buf, v->value.slice.step);
        }
        break;
    case JSONPATH_LENGTH:
        strbuf_putstr(buf, "length(");
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false, bsp,
                            jsonpath_operation_priority_lte(elem->type, v->type));
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_COUNT:
        strbuf_putstr(buf, "count(");
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false, bsp,
                            jsonpath_operation_priority_lte(elem->type, v->type));
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_MATCH:
        strbuf_putstr(buf, "match(");
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false, bsp,
                            jsonpath_operation_priority_lte(elem->type, v->type));
        strbuf_putchar(buf, ',');
        strbuf_putstr(buf, "\"");
        strbuf_putescape_json(buf, v->value.regex.pattern);
        strbuf_putchar(buf, '\"');
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_SEARCH:
        strbuf_putstr(buf, "search(");
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false, bsp,
                            jsonpath_operation_priority_lte(elem->type, v->type));
        strbuf_putchar(buf, ',');
        strbuf_putstr(buf, "\"");
        strbuf_putescape_json(buf, v->value.regex.pattern);
        strbuf_putchar(buf, '\"');
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_VALUE:
        strbuf_putstr(buf, "value(");
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false, bsp,
                            jsonpath_operation_priority_lte(elem->type, v->type));
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_AVG:
        strbuf_putstr(buf, "avg(");
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false, bsp,
                            jsonpath_operation_priority_lte(elem->type, v->type));
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_MAX:
        strbuf_putstr(buf, "max(");
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false, bsp,
                            jsonpath_operation_priority_lte(elem->type, v->type));
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_MIN:
        strbuf_putstr(buf, "min(");
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false, bsp,
                            jsonpath_operation_priority_lte(elem->type, v->type));
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_ABS:
        strbuf_putstr(buf, "abs(");
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false, bsp,
                            jsonpath_operation_priority_lte(elem->type, v->type));
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_CEIL:
        strbuf_putstr(buf, "ceil(");
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false, bsp,
                            jsonpath_operation_priority_lte(elem->type, v->type));
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_FLOOR:
        strbuf_putstr(buf, "floor(");
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false, bsp,
                            jsonpath_operation_priority_lte(elem->type, v->type));
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_DOUBLE:
        strbuf_putstr(buf, "double(");
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false, bsp,
                            jsonpath_operation_priority_lte(elem->type, v->type));
        strbuf_putchar(buf, ')');
        break;
    default:
        fprintf(stderr, "unrecognized jsonpath item type: %u\n", v->type);
        break;
    }

    elem = jsonpath_get_next(v);
    if (elem != NULL)
        jsonpath_print_item(buf, elem, true, bsp, true);
}

void jsonpath_dump_item(FILE *fp, jsonpath_item_t *v)
{
    strbuf_t buf = {0};
    jsonpath_print_item(&buf, v, false, true, true);
    if (buf.ptr != NULL)
        fputs(buf.ptr, fp);
    strbuf_destroy(&buf);
}

void jsonpath_item_free(jsonpath_item_t *item)
{
    if (item == NULL)
        return;

    while (item != NULL) {
        switch (item->type) {
        case JSONPATH_BOOL:
        case JSONPATH_NULL:
        case JSONPATH_NUMERIC:
        case JSONPATH_CURRENT:
        case JSONPATH_ROOT:
        case JSONPATH_ANYKEY:
        case JSONPATH_INDEXARRAY:
        case JSONPATH_SLICE:
            break;
        case JSONPATH_KEY:
        case JSONPATH_STRING:
            free(jsonpath_get_string(item, NULL));
            break;
        case JSONPATH_AND:
        case JSONPATH_OR:
        case JSONPATH_EQUAL:
        case JSONPATH_NOTEQUAL:
        case JSONPATH_LESS:
        case JSONPATH_GREATER:
        case JSONPATH_LESSOREQUAL:
        case JSONPATH_GREATEROREQUAL:
        case JSONPATH_ADD:
        case JSONPATH_SUB:
        case JSONPATH_MUL:
        case JSONPATH_DIV:
        case JSONPATH_MOD:
            jsonpath_item_free(jsonpath_get_left_arg(item));
            jsonpath_item_free(jsonpath_get_right_arg(item));
            break;
        case JSONPATH_REGEX:
        case JSONPATH_MATCH:
        case JSONPATH_SEARCH:
            jsonpath_item_free(item->value.regex.expr);
            free(item->value.regex.pattern);
            regfree(&item->value.regex.regex);
            break;
        case JSONPATH_PLUS:
        case JSONPATH_MINUS:
        case JSONPATH_NOT:
        case JSONPATH_FILTER:
            jsonpath_item_free(jsonpath_get_arg(item));
            break;
        case JSONPATH_UNION:
        case JSONPATH_DSC_UNION:
            for (int32_t i = 0; i < item->value.iunion.len; i++) {
                jsonpath_item_free(item->value.iunion.items[i]);
            }
            free(item->value.iunion.items);
            break;
        case JSONPATH_ABS:
        case JSONPATH_AVG:
        case JSONPATH_MAX:
        case JSONPATH_MIN:
        case JSONPATH_CEIL:
        case JSONPATH_VALUE:
        case JSONPATH_COUNT:
        case JSONPATH_FLOOR:
        case JSONPATH_DOUBLE:
        case JSONPATH_LENGTH:
            jsonpath_item_free(jsonpath_get_arg(item));
            break;
        }

        jsonpath_item_t *next = jsonpath_get_next(item);
        free(item);
        item = next;
    }
}

static void jsonpath_fill_followers(jsonpath_item_t *item)
{
    if (item == NULL)
        return;

    while (item != NULL) {
        switch (item->type) {
        case JSONPATH_BOOL:
        case JSONPATH_NULL:
        case JSONPATH_NUMERIC:
        case JSONPATH_CURRENT:
        case JSONPATH_ROOT:
        case JSONPATH_ANYKEY:
        case JSONPATH_INDEXARRAY:
        case JSONPATH_SLICE:
        case JSONPATH_KEY:
        case JSONPATH_STRING:
            break;
        case JSONPATH_AND:
        case JSONPATH_OR:
        case JSONPATH_EQUAL:
        case JSONPATH_NOTEQUAL:
        case JSONPATH_LESS:
        case JSONPATH_GREATER:
        case JSONPATH_LESSOREQUAL:
        case JSONPATH_GREATEROREQUAL:
        case JSONPATH_ADD:
        case JSONPATH_SUB:
        case JSONPATH_MUL:
        case JSONPATH_DIV:
        case JSONPATH_MOD:
            jsonpath_fill_followers(jsonpath_get_left_arg(item));
            jsonpath_fill_followers(jsonpath_get_right_arg(item));
            break;
        case JSONPATH_REGEX:
        case JSONPATH_MATCH:
        case JSONPATH_SEARCH:
            jsonpath_fill_followers(item->value.regex.expr);
            break;
        case JSONPATH_PLUS:
        case JSONPATH_MINUS:
        case JSONPATH_NOT:
        case JSONPATH_FILTER:
            jsonpath_fill_followers(jsonpath_get_arg(item));
            break;
        case JSONPATH_UNION:
        case JSONPATH_DSC_UNION: {
            jsonpath_item_t *inext = jsonpath_get_next(item);
            for (int32_t i = 0; i < item->value.iunion.len; i++) {
                jsonpath_item_t *uitem = item->value.iunion.items[i];
                while (uitem != NULL) {
                    if (uitem->next == NULL)
                        break;
                    uitem = uitem->next;
                }
                if (uitem != NULL)
                    uitem->follow = inext;
            }
        }   break;
        case JSONPATH_ABS:
        case JSONPATH_AVG:
        case JSONPATH_MAX:
        case JSONPATH_MIN:
        case JSONPATH_CEIL:
        case JSONPATH_VALUE:
        case JSONPATH_COUNT:
        case JSONPATH_FLOOR:
        case JSONPATH_DOUBLE:
        case JSONPATH_LENGTH:
            jsonpath_fill_followers(jsonpath_get_arg(item));
            break;
        }

        jsonpath_item_t *next = jsonpath_get_next(item);
        item = next;
    }
}

jsonpath_item_t *jsonpath_parser(const char *query, char *buffer_error, size_t buffer_error_size)
{
    yyscan_t scanner;

    jsonpath_string_t scanstring = {0};
    jsonpath_yylex_init_extra(&scanstring, &scanner);
//  jsonpath_yyset_debug(1, scanner);

    YY_BUFFER_STATE  buffer = jsonpath_yy_scan_string(query, scanner);

    jsonpath_parse_result_t parse_result = {0};
    parse_result.expr = NULL;
    parse_result.error = true;
    int status = jsonpath_yyparse(scanner, &parse_result);

    jsonpath_yy_delete_buffer(buffer, scanner);
    jsonpath_yylex_destroy(scanner);
    free(scanstring.val);

    if (status != 0) {
        if (parse_result.error == true) {
            if (parse_result.expr != NULL)
                jsonpath_item_free(parse_result.expr);
            if ((buffer_error != NULL) && (buffer_error_size > 0)) {
                buffer_error[0] = '\0';
                sstrncpy(buffer_error, parse_result.error_msg, buffer_error_size);
            }
        }
        return NULL;
    }

    jsonpath_fill_followers(parse_result.expr);

    return parse_result.expr;
}

