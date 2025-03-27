// SPDX-License-Identifier: GPL-2.0-only OR PostgreSQL
// SPDX-FileCopyrightText: Copyright (c) 2019-2023, PostgreSQL Global Development Group
// from posgreql src/backend/utils/adt/jsonpath_gram.y
%{
#include <string.h>
#include <stdlib.h>
#include <regex.h>

#include "plugins/match_jsonpath/jsonpath_list.h"
#include "plugins/match_jsonpath/jsonpath_internal.h"
#include "plugins/match_jsonpath/jsonpath_gram.h"
#include "plugins/match_jsonpath/jsonpath_scan.h"

static jsonpath_item_t *make_item_type(jsonpath_item_type_t type);
static jsonpath_item_t *make_item_string(jsonpath_string_t *s);
static jsonpath_item_t *make_item_key(jsonpath_string_t *s);
static jsonpath_item_t *make_item_numeric(jsonpath_string_t *s);
static jsonpath_item_t *make_item_bool(bool val);
static jsonpath_item_t *make_item_binary(jsonpath_item_type_t type,
                                               jsonpath_item_t *la,
                                               jsonpath_item_t *ra);
static jsonpath_item_t *make_item_unary(jsonpath_item_type_t type, jsonpath_item_t *a);
static jsonpath_item_t *make_item_call(jsonpath_item_type_t type, jsonpath_item_t *arg);

static jsonpath_item_t *make_item_list(jsonpath_list_t *list);
static jsonpath_item_t *make_item_union(jsonpath_list_t *list);
static jsonpath_item_t *make_item_dsc_union(jsonpath_list_t *list);
static jsonpath_item_t *make_index_array(int32_t idx);
static jsonpath_item_t *make_slice(int32_t start, int32_t end, int32_t step);
static jsonpath_item_t *make_item_regex(jsonpath_item_type_t type, jsonpath_item_t *expr,
                                        jsonpath_string_t *pattern);

void jsonpath_yyerror(JSONPATH_YYLTYPE *loc, void *scanner, jsonpath_parse_result_t *result,
                      const char *message);

%}

/* BISON Declarations */
%define api.pure full
%define api.prefix {jsonpath_yy}

%define parse.trace true
%define parse.error verbose

/* expect 0 */

%lex-param   {void *scanner }
%parse-param {void *scanner }{jsonpath_parse_result_t *result}
%locations

%union
{
    jsonpath_string_t        str;
    jsonpath_list_t         *elems;
    jsonpath_item_t         *value;
    jsonpath_parse_result_t *result;
    jsonpath_item_type_t     optype;
    int32_t                  integer;
}

%token   <str>     NULL_P TRUE_P FALSE_P
%token   <str>     IDENT_P QSTRING_P STRING_P NUMERIC_P INT_P REGEX_P MATCH_P SEARCH_P
%token   <str>     OR_P AND_P NOT_P
%token   <str>     LESS_P LESSEQUAL_P EQUAL_P NOTEQUAL_P GREATEREQUAL_P GREATER_P REGEXOP_P
%token   <str>     ANY_P
%token   <str>     ABS_P AVG_P MAX_P MIN_P COUNT_P VALUE_P LENGTH_P FLOOR_P DOUBLE_P CEIL_P

%type    <result>   result
%type    <elems>    segments selection
// %type    <optype>   method
%type    <str>      key
%type    <value>    expr selector root_query segment root_query_or_expr

%type    <integer>  integer

%left    OR_P AND_P
%left    EQUAL_P NOTEQUAL_P LESS_P GREATER_P LESSEQUAL_P GREATEREQUAL_P REGEXOP_P
%right   NOT_P
%left    '+' '-'
%left    '*' '/' '%'
%left    UMINUS UPLUS
%nonassoc '(' ')'

%destructor {
    jsonpath_list_t  *list = $$;
    if (list != NULL) {
        int len = jsonpath_list_length(list);
        for (int i = 0; i < len ; i++) {
            jsonpath_list_cell_t *c = jsonpath_list_nth_cell(list, i);
            if (c == NULL)
                continue;
            jsonpath_item_free(c->ptr_value);
        }
        jsonpath_list_free(list);
    }
} <elems>

%destructor {
    jsonpath_item_free($$);
} <value>

%%

result:
    root_query_or_expr            {
                                    result->expr = $1;
                                    result->lax = true;
                                    result->error = false;
                                    (void) yynerrs;
                                  }
    | /* EMPTY */                 {
                                    result->expr = NULL;
                                    result->error = false;
                                    (void) yynerrs;
                                  }
    ;

root_query_or_expr:
    root_query                    { $$ = $1; }
    | '(' expr ')'                { $$ = $2; }
    ;

root_query:
    '$'                           { $$ = make_item_type(JSONPATH_ROOT); }
    | '$' segments                { $$ = make_item_list(jsonpath_list_prepend($2,
                                                           make_item_type(JSONPATH_ROOT)));
                                  }
    ;

segments:
    segment                       { $$ = jsonpath_list_make1($1); }
    | segments segment            { $$ = jsonpath_list_append($1, $2); }
    ;

key:
    IDENT_P
    | STRING_P
    | NULL_P
    | TRUE_P
    | FALSE_P
    | ABS_P
    | AVG_P
    | MAX_P
    | MIN_P
    | CEIL_P
    | VALUE_P
    | COUNT_P
    | FLOOR_P
    | MATCH_P
    | DOUBLE_P
    | LENGTH_P
    | SEARCH_P
    ;

segment:
    '[' selection ']'              { $$ = make_item_union($2); }
    | '.' '*'                      { $$ = make_item_type(JSONPATH_ANYKEY); }
    | '.' key                      { $$ = make_item_key(&$2); }
    | ANY_P '[' selection ']'      { $$ = make_item_dsc_union($3); }
    | ANY_P '*'                    { $$ = make_item_dsc_union(jsonpath_list_make1(
                                                                 make_item_type(JSONPATH_ANYKEY)));
                                   }
    | ANY_P key                    { $$ = make_item_dsc_union(jsonpath_list_make1(
                                                                 make_item_key(&$2)));
                                   }
    ;

selection:
    selector                       { $$ = jsonpath_list_make1($1); }
    | selection ',' selector       { $$ = jsonpath_list_append($1, $3); }
    ;

integer:
    INT_P                          { $$ = atoi($1.val);}
    | '-' INT_P                    { $$ = - atoi($2.val);}
    ;

selector:
    QSTRING_P                       { $$ = make_item_key(&$1); }
    | '*'                           { $$ = make_item_type(JSONPATH_ANYKEY); }
    | integer                       { $$ = make_index_array($1); }
    | '?' expr                      { $$ = make_item_unary(JSONPATH_FILTER, $2); }
    | ':'                           { $$ = make_slice(0, INT32_MAX, 1); }
    | integer ':'                   { $$ = make_slice($1, INT32_MAX, 1); }
    | integer ':' integer           { $$ = make_slice($1, $3, 1); }
    | ':' integer                   { $$ = make_slice(0, $2, 1); }
    | integer ':' integer ':'       { $$ = make_slice($1, $3, 1); }
    | integer ':' integer ':' integer {
                                      if ($5 == 0) {
                                          jsonpath_yyerror(&yyloc, scanner, result,
                                                           "in array slice step cannot be 0.");
                                          YYABORT;
                                      } else {
                                          $$ = make_slice($1, $3, $5);
                                      }
                                    }
    | integer ':' ':' integer       {
                                      int32_t step = $4;
                                      if (step > 0) {
                                          $$ = make_slice($1, INT32_MAX, step);
                                      } else if (step == 0) {
                                          jsonpath_yyerror(&yyloc, scanner, result,
                                                           "in array slice step cannot be 0.");
                                          YYABORT;
                                      } else {
                                          $$ = make_slice($1, -INT32_MAX, step);
                                      }
                                    }
    | ':' integer ':' integer       {
                                      int32_t step = $4;
                                      if (step > 0) {
                                          $$ = make_slice(0, $2, step);
                                      } else if (step == 0) {
                                          jsonpath_yyerror(&yyloc, scanner, result,
                                                           "in array slice step cannot be 0.");
                                          YYABORT;
                                      } else {
                                          $$ = make_slice(INT32_MAX, $2, step);
                                      }
                                    }
    | ':' ':' integer               {
                                      int32_t step = $3;
                                      if (step > 0) {
                                          $$ = make_slice(0, INT32_MAX, step);
                                      } else if (step == 0) {
                                          jsonpath_yyerror(&yyloc, scanner, result,
                                                           "in array slice step cannot be 0.");
                                          YYABORT;
                                      } else {
                                          $$ = make_slice(INT32_MAX, -INT32_MAX, step);
                                      }
                                    }
    | ':' ':'                       {  $$ = make_slice(0, INT32_MAX, 1); }
    ;

expr:
    '$'                             { $$ = make_item_type(JSONPATH_ROOT); }
    | '$' segments                  { $$ = make_item_list(jsonpath_list_prepend($2,
                                                             make_item_type(JSONPATH_ROOT)));
                                    }
    | '@'                           { $$ = make_item_type(JSONPATH_CURRENT); }
    | '@' segments                  { $$ = make_item_list(jsonpath_list_prepend($2,
                                                             make_item_type(JSONPATH_CURRENT)));
                                    }
    | QSTRING_P                     { $$ = make_item_string(&$1);  }
    | NULL_P                        { $$ = make_item_string(NULL); }
    | TRUE_P                        { $$ = make_item_bool(true);   }
    | FALSE_P                       { $$ = make_item_bool(false);  }
    | NUMERIC_P                     { $$ = make_item_numeric(&$1); }
    | INT_P                         { $$ = make_item_numeric(&$1); }
    | '(' expr ')'                  { $$ = $2; }
    | '+' expr %prec UPLUS          { $$ = make_item_unary(JSONPATH_PLUS, $2); }
    | '-' expr %prec UMINUS         { $$ = make_item_unary(JSONPATH_MINUS, $2); }
    | expr '+' expr                 { $$ = make_item_binary(JSONPATH_ADD, $1, $3); }
    | expr '-' expr                 { $$ = make_item_binary(JSONPATH_SUB, $1, $3); }
    | expr '*' expr                 { $$ = make_item_binary(JSONPATH_MUL, $1, $3); }
    | expr '/' expr                 { $$ = make_item_binary(JSONPATH_DIV, $1, $3); }
    | expr '%' expr                 { $$ = make_item_binary(JSONPATH_MOD, $1, $3); }
    | expr AND_P          expr      { $$ = make_item_binary(JSONPATH_AND, $1, $3); }
    | expr OR_P           expr      { $$ = make_item_binary(JSONPATH_OR, $1, $3); }
    | expr EQUAL_P        expr      { $$ = make_item_binary(JSONPATH_EQUAL, $1, $3); }
    | expr NOTEQUAL_P     expr      { $$ = make_item_binary(JSONPATH_NOTEQUAL, $1, $3); }
    | expr LESS_P         expr      { $$ = make_item_binary(JSONPATH_LESS, $1, $3); }
    | expr GREATER_P      expr      { $$ = make_item_binary(JSONPATH_GREATER, $1, $3); }
    | expr LESSEQUAL_P    expr      { $$ = make_item_binary(JSONPATH_LESSOREQUAL, $1, $3); }
    | expr GREATEREQUAL_P expr      { $$ = make_item_binary(JSONPATH_GREATEROREQUAL, $1, $3); }
    | NOT_P expr                    { $$ = make_item_unary(JSONPATH_NOT, $2); }
    | expr REGEXOP_P QSTRING_P      { $$ = make_item_regex(JSONPATH_REGEX, $1, &$3); }
    | ABS_P '(' expr ')'            { $$ = make_item_call(JSONPATH_ABS, $3); }
    | AVG_P '(' expr ')'            { $$ = make_item_call(JSONPATH_AVG, $3); }
    | MAX_P '(' expr ')'            { $$ = make_item_call(JSONPATH_MAX, $3); }
    | MIN_P '(' expr ')'            { $$ = make_item_call(JSONPATH_MIN, $3); }
    | CEIL_P '(' expr')'            { $$ = make_item_call(JSONPATH_CEIL, $3); }
    | VALUE_P '(' expr ')'          { $$ = make_item_call(JSONPATH_VALUE, $3); }
    | COUNT_P '(' expr ')'          { $$ = make_item_call(JSONPATH_COUNT, $3); }
    | FLOOR_P '(' expr ')'          { $$ = make_item_call(JSONPATH_FLOOR, $3); }
    | MATCH_P '(' expr ',' QSTRING_P ')' { $$ = make_item_regex(JSONPATH_MATCH, $3, &$5); }
    | DOUBLE_P '(' expr ')'         { $$ = make_item_call(JSONPATH_DOUBLE, $3); }
    | LENGTH_P '(' expr ')'         { $$ = make_item_call(JSONPATH_LENGTH, $3); }
    | SEARCH_P '(' expr ',' QSTRING_P')'{ $$ = make_item_regex(JSONPATH_SEARCH, $3, &$5); }
    ;

%%

static jsonpath_item_t *make_item_type(jsonpath_item_type_t type)
{
    jsonpath_item_t *v = calloc(1, sizeof(*v));
    if (v == NULL)
        return NULL;

    v->type = type;

    return v;
}

static jsonpath_item_t *make_item_string(jsonpath_string_t *s)
{
    if (s == NULL)
        return make_item_type(JSONPATH_NULL);

    jsonpath_item_t *v = make_item_type(JSONPATH_STRING);
    if (v == NULL)
        return NULL;
    v->value.string.val = strdup(s->val);
    v->value.string.len = s->len;

    return v;
}

static jsonpath_item_t *make_item_key(jsonpath_string_t *s)
{
    jsonpath_item_t *v = make_item_string(s);
    if (v == NULL)
        return NULL;
    v->type = JSONPATH_KEY;

    return v;
}

static jsonpath_item_t *make_item_numeric(jsonpath_string_t *s)
{
    jsonpath_item_t *v;

    v = make_item_type(JSONPATH_NUMERIC);
    if (v == NULL)
        return NULL;
    v->value.numeric = atof(s->val); // FIXME
    return v;
}

static jsonpath_item_t *make_item_bool(bool val)
{
    jsonpath_item_t *v = make_item_type(JSONPATH_BOOL);
    if (v == NULL)
        return NULL;

    v->value.boolean = val;

    return v;
}

static jsonpath_item_t * make_item_binary(jsonpath_item_type_t type,
                                                jsonpath_item_t *la,
                                                jsonpath_item_t *ra)
{
    jsonpath_item_t *v = make_item_type(type);
    if (v == NULL)
        return NULL;

    v->value.args.left = la;
    v->value.args.right = ra;

    return v;
}

static jsonpath_item_t *make_item_unary(jsonpath_item_type_t type, jsonpath_item_t *a)
{
    jsonpath_item_t *v = make_item_type(type);
    if (v == NULL)
        return NULL;

    v->value.arg = a;

    return v;
}

static jsonpath_item_t *make_item_call(jsonpath_item_type_t type, jsonpath_item_t *arg)
{
    jsonpath_item_t *v;

    v = make_item_type(type);
    if (v == NULL)
        return NULL;

    v->value.arg = arg;

    return v;
}

static jsonpath_item_t *make_item_list(jsonpath_list_t *list)
{
    if (list == NULL)
        return NULL;

    jsonpath_item_t *head, *end;
    jsonpath_list_cell_t   *cell;

    head = end = (jsonpath_item_t *) jsonpath_list_initial(list);

    if (jsonpath_list_length(list) == 1) {
        jsonpath_list_free(list);
        return head;
    }

    /* append items to the end of already existing list */
    while (end->next)
        end = end->next;

    for_each_from(cell, list, 1) {
        jsonpath_item_t *c = (jsonpath_item_t *) jsonpath_list_first(cell);

        end->next = c;
        end = c;
    }
    jsonpath_list_free(list);

    return head;
}

static jsonpath_item_t *make_item_union(jsonpath_list_t *list)
{
    jsonpath_item_t *v = make_item_type(JSONPATH_UNION);
    if (v == NULL)
        return NULL;

    int len = jsonpath_list_length(list);

    v->value.iunion.len = len;
    v->value.iunion.items = calloc(len, sizeof(jsonpath_item_t *));
    if (v->value.iunion.items == NULL) {
        free(v);
        return NULL;
    }

    for (int i = 0; i < len ; i++) {
        jsonpath_list_cell_t *c = jsonpath_list_nth_cell(list, i);
        if (c == NULL)
            continue;
        v->value.iunion.items[i] = c->ptr_value;
    }
    jsonpath_list_free(list);

    return v;
}

static jsonpath_item_t *make_item_dsc_union(jsonpath_list_t *list)
{
    jsonpath_item_t *v = make_item_type(JSONPATH_DSC_UNION);
    if (v == NULL)
        return NULL;

    int len = jsonpath_list_length(list);

    v->value.iunion.len = len;
    v->value.iunion.items = calloc(len, sizeof(jsonpath_item_t *));
    if (v->value.iunion.items == NULL) {
        free(v);
        return NULL;
    }

    for (int i = 0; i < len ; i++) {
        jsonpath_list_cell_t *c = jsonpath_list_nth_cell(list, i);
        if (c == NULL)
            continue;
        v->value.iunion.items[i] = c->ptr_value;
    }
    jsonpath_list_free(list);

    return v;
}

static jsonpath_item_t *make_index_array(int32_t idx)
{
    jsonpath_item_t *v = make_item_type(JSONPATH_INDEXARRAY);
    if (v == NULL)
        return NULL;
    v->value.array.idx = idx;
    return v;
}

static jsonpath_item_t *make_slice(int32_t start, int32_t end, int32_t step)
{
    jsonpath_item_t *v = make_item_type(JSONPATH_SLICE);
    if (v == NULL)
        return NULL;

    v->value.slice.start = start;
    v->value.slice.end = end;
    v->value.slice.step = step;

    return v;
}

static jsonpath_item_t *make_item_regex(jsonpath_item_type_t type, jsonpath_item_t *expr,
                                        jsonpath_string_t *pattern)
{
    jsonpath_item_t *v = make_item_type(type);
    if (v == NULL)
        return NULL;

    v->value.regex.expr = expr;
    v->value.regex.pattern = strdup(pattern->val);

    int cflags = REG_EXTENDED;

    int status = regcomp(&v->value.regex.regex,  v->value.regex.pattern, cflags);
    if (status != 0) {
        // FIXME check error
    }

    return v;
}
