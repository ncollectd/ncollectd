// SPDX-License-Identifier: GPL-2.0-only or PostgreSQL
// SPDX-FileCopyrightText: Copyright (c) 2019-2023, PostgreSQL Global Development Group
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"

#include <sys/uio.h>
#include <regex.h>

#include "libutils/strbuf.h"
#include "plugins/match_jsonpath/jsonpath.h"
#include "plugins/match_jsonpath/jsonpath_list.h"
#include "plugins/match_jsonpath/jsonpath_internal.h"
#include "plugins/match_jsonpath/jsonpath_gram.h"
#define YYSTYPE JSONPATH_YYSTYPE
#define YYLTYPE JSONPATH_YYLTYPE
#include "plugins/match_jsonpath/jsonpath_scan.h"

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
    case JSONPATH_LENGTH:
        return "length";
    case JSONPATH_COUNT:
        return "count";
    case JSONPATH_ABS:
        return "abs";
    case JSONPATH_AVG:
        return "avg";
    case JSONPATH_MAX:
        return "max";
    case JSONPATH_MIN:
        return "min";
    case JSONPATH_FLOOR:
        return "floor";
    case JSONPATH_CEILING:
        return "ceiling";
    case JSONPATH_DOUBLE:
        return "double";
    default:
#if 0
        elog(ERROR, "unrecognized jsonpath item type: %d", type);
#endif
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

void jsonpath_print_item(strbuf_t *buf, jsonpath_item_t *v, bool inKey, bool printBracketes)
{
    jsonpath_item_t *elem;

    if (v == NULL)
        return;
//    check_stack_depth();
//    CHECK_FOR_INTERRUPTS();
    switch (v->type) {
    case JSONPATH_NULL:
        strbuf_putstr(buf, "null");
        break;
    case JSONPATH_KEY:
        if (inKey)
            strbuf_putchar(buf, '.');
        strbuf_putescape_json(buf, jsonpath_get_string(v, NULL));
        break;
    case JSONPATH_STRING:
        strbuf_putescape_json(buf, jsonpath_get_string(v, NULL));
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
        if (printBracketes)
            strbuf_putchar(buf, '(');
        elem = jsonpath_get_left_arg(v);
        jsonpath_print_item(buf, elem, false,
                          jsonpath_operation_priority(elem->type) <= jsonpath_operation_priority(v->type));
        strbuf_putchar(buf, ' ');
        strbuf_putstr(buf, jsonpath_operation_name(v->type));
        strbuf_putchar(buf, ' ');
        elem = jsonpath_get_right_arg(v);
        jsonpath_print_item(buf, elem, false,
                          jsonpath_operation_priority(elem->type) <= jsonpath_operation_priority(v->type));
        if (printBracketes)
            strbuf_putchar(buf, ')');
        break;
    case JSONPATH_REGEX:
        if (printBracketes)
            strbuf_putchar(buf, '(');
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false,
                            jsonpath_operation_priority(elem->type) <= jsonpath_operation_priority(v->type));
        strbuf_putstr(buf, "=~ /");
        strbuf_putescape_json(buf, v->value.regex.pattern);
        if (printBracketes)
            strbuf_putchar(buf, ')');
        break;
#if 0
        if (v->value.like_regex.flags) {
            strbuf_putstr(buf, " flag \"");

            if (v->value.like_regex.flags & JSP_REGEX_ICASE)
                strbuf_putchar(buf, 'i');
            if (v->value.like_regex.flags & JSP_REGEX_DOTALL)
                strbuf_putchar(buf, 's');
            if (v->value.like_regex.flags & JSP_REGEX_MLINE)
                strbuf_putchar(buf, 'm');
            if (v->value.like_regex.flags & JSP_REGEX_WSPACE)
                strbuf_putchar(buf, 'x');
            if (v->value.like_regex.flags & JSP_REGEX_QUOTE)
                strbuf_putchar(buf, 'q');
        }

        break;
#endif
    case JSONPATH_PLUS:
    case JSONPATH_MINUS:
        if (printBracketes)
            strbuf_putchar(buf, '(');
        strbuf_putchar(buf, v->type == JSONPATH_PLUS ? '+' : '-');
        elem = jsonpath_get_arg(v);
        jsonpath_print_item(buf, elem, false,
                            jsonpath_operation_priority(elem->type) <=
                            jsonpath_operation_priority(v->type));
        if (printBracketes)
            strbuf_putchar(buf, ')');
        break;
    case JSONPATH_UNION: {
        strbuf_putchar(buf, '[');
        for (int32_t i = 0; i < v->value.iunion.len; i++) {
            if (i > 0)
                strbuf_putchar(buf, ',');
            jsonpath_print_item(buf, v->value.iunion.items[i], true, true);
        }
        strbuf_putchar(buf, ']');
    }   break;
    case JSONPATH_DSC_UNION: {
        strbuf_putstr(buf, "..[");
        for (int32_t i = 0; i < v->value.iunion.len; i++) {
            if (i > 0)
                strbuf_putchar(buf, ',');
            jsonpath_print_item(buf, v->value.iunion.items[i], true, true);
        }
        strbuf_putchar(buf, ']');
    }   break;
    case JSONPATH_FILTER:
        strbuf_putstr(buf, "?(");
        elem = jsonpath_get_arg(v);
        jsonpath_print_item(buf, elem, false, false);
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_NOT:
        strbuf_putstr(buf, "!(");
        elem = jsonpath_get_arg(v);
        jsonpath_print_item(buf, elem, false, false);
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_CURRENT:
//        Assert(!inKey);
        strbuf_putchar(buf, '@');
        break;
    case JSONPATH_ROOT:
//        Assert(!inKey);
        strbuf_putchar(buf, '$');
        break;
    case JSONPATH_ANYARRAY:
        strbuf_putstr(buf, "[*]");
        break;
    case JSONPATH_ANYKEY:
        if (inKey)
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
        jsonpath_print_item(buf, elem, false,
                            jsonpath_operation_priority(elem->type) <= jsonpath_operation_priority(v->type));
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_COUNT:
        strbuf_putstr(buf, "count(");
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false,
                            jsonpath_operation_priority(elem->type) <= jsonpath_operation_priority(v->type));
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_AVG:
        strbuf_putstr(buf, "avg(");
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false,
                            jsonpath_operation_priority(elem->type) <= jsonpath_operation_priority(v->type));
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_MAX:
        strbuf_putstr(buf, "max(");
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false,
                            jsonpath_operation_priority(elem->type) <= jsonpath_operation_priority(v->type));
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_MIN:
        strbuf_putstr(buf, "min(");
        elem = v->value.regex.expr;
        jsonpath_print_item(buf, elem, false,
                            jsonpath_operation_priority(elem->type) <= jsonpath_operation_priority(v->type));
        strbuf_putchar(buf, ')');
        break;
    case JSONPATH_ABS:
        strbuf_putstr(buf, ".abs()");
        break;
    case JSONPATH_FLOOR:
        strbuf_putstr(buf, ".floor()");
        break;
    case JSONPATH_CEILING:
        strbuf_putstr(buf, ".ceiling()");
        break;
    case JSONPATH_DOUBLE:
        strbuf_putstr(buf, ".double()");
        break;
    default:
        fprintf(stderr, "unrecognized jsonpath item type: %u\n", v->type);
        break;
    }

    elem = jsonpath_get_next(v);
    if (elem != NULL)
        jsonpath_print_item(buf, elem, true, true);
}

void jsonpath_item_free(jsonpath_item_t *item)
{
    if (item == NULL)
        return;

    jsonpath_item_t *head = item;
    while (item != NULL) {
        if ((item->type == JSONPATH_UNION) || (item->type == JSONPATH_DSC_UNION)){
            jsonpath_item_t *next = item->shadow;
            if (next != NULL) {
                for (int32_t i = 0; i < item->value.iunion.len; i++) {
                    jsonpath_item_t *uitem = item->value.iunion.items[i];
                    while (uitem != NULL) {
                        if (uitem->next == next)
                            uitem->next = NULL;
                        uitem = uitem->next;
                    }
                }
                item->next = item->shadow;
            }
            item = item->next;
        } else {
            item = item->next;
        }
    }

    item = head;

    while (item != NULL) {
        switch (item->type) {
        case JSONPATH_BOOL:
        case JSONPATH_NULL:
        case JSONPATH_NUMERIC:
        case JSONPATH_CURRENT:
        case JSONPATH_ROOT:
        case JSONPATH_ANYARRAY:
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
        case JSONPATH_LENGTH:
        case JSONPATH_COUNT:
        case JSONPATH_AVG:
        case JSONPATH_MAX:
        case JSONPATH_MIN:
            jsonpath_item_free(jsonpath_get_arg(item));
            break;
        case JSONPATH_ABS:
        case JSONPATH_FLOOR:
        case JSONPATH_CEILING:
        case JSONPATH_DOUBLE:

        case JSONPATH_SUBSCRIPT: // FIXME
            break;
        }

        jsonpath_item_t *next = jsonpath_get_next(item);
        free(item);
        item = next;
    }
}

/* Interface to jsonpath parser */
jsonpath_item_t *jsonpath_parser(const char *query)
{
    yyscan_t scanner;

    jsonpath_string_t scanstring = {0};
    jsonpath_yylex_init_extra(&scanstring, &scanner);
//    jsonpath_yyset_debug(1, scanner);

    YY_BUFFER_STATE  buffer = jsonpath_yy_scan_string(query, scanner);

#if 0
    /* Might be left over after ereport() */
    // yy_init_globals();

    /* Make a scan buffer with special termination needed by flex.  */
    char *scanbuf = malloc(slen + 2);
    if (scanbuf == NULL) {
        // FIXME
    }
    memcpy(scanbuf, str, slen);
    scanbuf[slen] = scanbuf[slen + 1] = '\0';
    YY_BUFFER_STATE scanbufhandle = jsonpath_yy_scan_buffer(scanbuf, slen + 2, scanner);
#endif
    // BEGIN(INITIAL);

    jsonpath_parse_result_t parse_result = {0};
    parse_result.expr = NULL;
    parse_result.error = true;
    int status = jsonpath_yyparse(scanner, &parse_result);
#if 0
    if (status != 0)  {
       jsonpath_yyerror(yylloc, NULL, escontext, "invalid input"); /* shouldn't happen */
    }
#endif

    jsonpath_yy_delete_buffer(buffer, scanner);
    jsonpath_yylex_destroy(scanner);
    free(scanstring.val);
    
    if (status != 0) {
        if (parse_result.error == true) {
            if (parse_result.expr != NULL)
                jsonpath_item_free(parse_result.expr);
            PLUGIN_ERROR("Failed to parse '%s': %s", query, parse_result.error_msg);
        }
        return NULL;
    }

    jsonpath_item_t *item = parse_result.expr;
    while (item != NULL) {
        if ((item->type == JSONPATH_UNION) || (item->type == JSONPATH_DSC_UNION)){
            for (int32_t i = 0; i < item->value.iunion.len; i++) {
                jsonpath_item_t *uitem = item->value.iunion.items[i];
                while (uitem != NULL) {
                    if (uitem->next == NULL)
                        break;
                    uitem = uitem->next;
                }
                if (uitem != NULL)
                    uitem->next = item->next;
            }
            jsonpath_item_t *prev = item;
            item = item->next;
            prev->shadow = prev->next;
            prev->next = NULL;
        } else {
            item = item->next;
        }
    }

    return parse_result.expr;
}

