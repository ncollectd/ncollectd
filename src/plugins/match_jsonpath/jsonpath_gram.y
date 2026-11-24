// SPDX-License-Identifier: GPL-2.0-only OR PostgreSQL
// SPDX-FileCopyrightText: Copyright (c) 2019-2023, PostgreSQL Global Development Group
// from posgreql src/backend/utils/adt/jsonpath_gram.y
%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <regex.h>

#include "plugins/match_jsonpath/jsonpath_list.h"
#include "plugins/match_jsonpath/jsonpath_internal.h"
#include "plugins/match_jsonpath/jsonpath_gram.h"
#include "plugins/match_jsonpath/jsonpath_scan.h"

/* RFC 9535: integers must be within the I-JSON range [-(2^53)+1, (2^53)-1] */
#define JSONPATH_MAX_SAFE_INTEGER 9007199254740991LL

static jsonpath_item_t *make_item_type(jsonpath_item_type_t type);
static jsonpath_item_t *make_item_string(jsonpath_string_t *s);
static jsonpath_item_t *make_item_key(jsonpath_string_t *s);
static jsonpath_item_t *make_item_numeric(void *scanner, jsonpath_parse_result_t *result,
                                          jsonpath_string_t *s);
static int make_integer(jsonpath_string_t *s, bool negative, int32_t *ret);
static jsonpath_item_t *make_item_bool(bool val);
static jsonpath_item_t *make_item_binary(jsonpath_item_type_t type,
                                               jsonpath_item_t *la,
                                               jsonpath_item_t *ra);
static jsonpath_item_t *make_item_unary(jsonpath_item_type_t type, jsonpath_item_t *a);
static jsonpath_item_t *make_item_call(jsonpath_item_type_t type, jsonpath_item_t *arg);

static void list_free_items(jsonpath_list_t *list);
static jsonpath_list_t *list_append_item(void *scanner, jsonpath_parse_result_t *result,
                                         jsonpath_list_t *list, jsonpath_item_t *item);
static jsonpath_list_t *list_prepend_item(void *scanner, jsonpath_parse_result_t *result,
                                          jsonpath_list_t *list, jsonpath_item_t *item);
static jsonpath_item_t *make_item_list(jsonpath_list_t *list);
static jsonpath_item_t *make_item_union(jsonpath_list_t *list);
static jsonpath_item_t *make_item_dsc_union(jsonpath_list_t *list);
static jsonpath_item_t *make_index_array(int32_t idx);
static jsonpath_item_t *make_slice(int32_t start, int32_t end, int32_t step);
static jsonpath_item_t *make_item_regex(void *scanner, jsonpath_parse_result_t *result,
                                        jsonpath_item_type_t type, jsonpath_item_t *expr,
                                        jsonpath_string_t *pattern);

void jsonpath_yyerror(JSONPATH_YYLTYPE *loc, void *scanner, jsonpath_parse_result_t *result,
                      const char *message);

%}

%define api.pure full
%define api.prefix {jsonpath_yy}

%define parse.trace true
%define parse.error verbose

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
%token             LEXERROR_P

%type    <result>   result
%type    <elems>    segments selection
%type    <str>      key
%type    <value>    expr fexpr selector segment

%type    <integer>  integer

%left    OR_P AND_P
%left    EQUAL_P NOTEQUAL_P LESS_P GREATER_P LESSEQUAL_P GREATEREQUAL_P REGEXOP_P
%right   NOT_P
%left    '+' '-'
%left    '*' '/' '%'
%left    UMINUS UPLUS
%nonassoc '(' ')'

%destructor {
    list_free_items($$);
} <elems>

%destructor {
    jsonpath_item_free($$);
} <value>

%%

result:
    expr                         {
                                    /* a NULL item or an error from the scanner: out of memory */
                                    if (($1 == NULL) || (result->error_msg[0] != '\0')) {
                                        jsonpath_item_free($1);
                                        jsonpath_yyerror(&yyloc, scanner, result, "out of memory");
                                        YYABORT;
                                    }
                                    result->expr = $1;
                                    result->lax = true;
                                    result->error = false;
                                    (void) yynerrs;
                                  }
    | /* EMPTY */                 {
                                    (void) yynerrs;
                                    jsonpath_yyerror(&yyloc, scanner, result,
                                                     "empty jsonpath expression");
                                    YYABORT;
                                  }
    ;

segments:
    segment                       {
                                    $$ = list_append_item(scanner, result, NULL, $1);
                                    if ($$ == NULL)
                                        YYABORT;
                                  }
    | segments segment            {
                                    $$ = list_append_item(scanner, result, $1, $2);
                                    if ($$ == NULL)
                                        YYABORT;
                                  }
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
    | ANY_P '*'                    {
                                     jsonpath_list_t *list = list_append_item(scanner, result, NULL,
                                                                 make_item_type(JSONPATH_ANYKEY));
                                     if (list == NULL)
                                         YYABORT;
                                     $$ = make_item_dsc_union(list);
                                   }
    | ANY_P key                    {
                                     jsonpath_list_t *list = list_append_item(scanner, result, NULL,
                                                                              make_item_key(&$2));
                                     if (list == NULL)
                                         YYABORT;
                                     $$ = make_item_dsc_union(list);
                                   }
    ;

selection:
    selector                       {
                                     $$ = list_append_item(scanner, result, NULL, $1);
                                     if ($$ == NULL)
                                         YYABORT;
                                   }
    | selection ',' selector       {
                                     $$ = list_append_item(scanner, result, $1, $3);
                                     if ($$ == NULL)
                                         YYABORT;
                                   }
    ;

integer:
    INT_P                          {
                                     if (make_integer(&$1, false, &$$) != 0) {
                                         jsonpath_yyerror(&yyloc, scanner, result,
                                                          "integer out of range");
                                         YYABORT;
                                     }
                                   }
    | '-' INT_P                    {
                                     if (make_integer(&$2, true, &$$) != 0) {
                                         jsonpath_yyerror(&yyloc, scanner, result,
                                                          "integer out of range");
                                         YYABORT;
                                     }
                                   }
    ;

selector:
    QSTRING_P                       { $$ = make_item_key(&$1); }
    | '*'                           { $$ = make_item_type(JSONPATH_ANYKEY); }
    | integer                       { $$ = make_index_array($1); }
    | '?' fexpr                     { $$ = make_item_unary(JSONPATH_FILTER, $2); }
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
    | '$' segments                  {
                                      jsonpath_list_t *list = list_prepend_item(scanner, result, $2,
                                                                  make_item_type(JSONPATH_ROOT));
                                      if (list == NULL)
                                          YYABORT;
                                      $$ = make_item_list(list);
                                    }
    | QSTRING_P                     { $$ = make_item_string(&$1);  }
    | NULL_P                        { $$ = make_item_string(NULL); }
    | TRUE_P                        { $$ = make_item_bool(true);   }
    | FALSE_P                       { $$ = make_item_bool(false);  }
    | NUMERIC_P                     {
                                      $$ = make_item_numeric(scanner, result, &$1);
                                      if ($$ == NULL)
                                          YYABORT;
                                    }
    | INT_P                         {
                                      $$ = make_item_numeric(scanner, result, &$1);
                                      if ($$ == NULL)
                                          YYABORT;
                                    }
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
    | expr REGEXOP_P QSTRING_P      {
                                      $$ = make_item_regex(scanner, result, JSONPATH_REGEX, $1, &$3);
                                      if ($$ == NULL)
                                          YYABORT;
                                    }
    | ABS_P '(' expr ')'            { $$ = make_item_call(JSONPATH_ABS, $3); }
    | AVG_P '(' expr ')'            { $$ = make_item_call(JSONPATH_AVG, $3); }
    | MAX_P '(' expr ')'            { $$ = make_item_call(JSONPATH_MAX, $3); }
    | MIN_P '(' expr ')'            { $$ = make_item_call(JSONPATH_MIN, $3); }
    | CEIL_P '(' expr')'            { $$ = make_item_call(JSONPATH_CEIL, $3); }
    | VALUE_P '(' expr ')'          { $$ = make_item_call(JSONPATH_VALUE, $3); }
    | COUNT_P '(' expr ')'          { $$ = make_item_call(JSONPATH_COUNT, $3); }
    | FLOOR_P '(' expr ')'          { $$ = make_item_call(JSONPATH_FLOOR, $3); }
    | MATCH_P '(' expr ',' QSTRING_P ')' {
                                      $$ = make_item_regex(scanner, result, JSONPATH_MATCH, $3, &$5);
                                      if ($$ == NULL)
                                          YYABORT;
                                    }
    | DOUBLE_P '(' expr ')'         { $$ = make_item_call(JSONPATH_DOUBLE, $3); }
    | LENGTH_P '(' expr ')'         { $$ = make_item_call(JSONPATH_LENGTH, $3); }
    | SEARCH_P '(' expr ',' QSTRING_P')' {
                                      $$ = make_item_regex(scanner, result, JSONPATH_SEARCH, $3, &$5);
                                      if ($$ == NULL)
                                          YYABORT;
                                    }
    ;

fexpr:
    '$'                             { $$ = make_item_type(JSONPATH_ROOT); }
    | '$' segments                  {
                                      jsonpath_list_t *list = list_prepend_item(scanner, result, $2,
                                                                  make_item_type(JSONPATH_ROOT));
                                      if (list == NULL)
                                          YYABORT;
                                      $$ = make_item_list(list);
                                    }
    | '@'                           { $$ = make_item_type(JSONPATH_CURRENT); }
    | '@' segments                  {
                                      jsonpath_list_t *list = list_prepend_item(scanner, result, $2,
                                                                  make_item_type(JSONPATH_CURRENT));
                                      if (list == NULL)
                                          YYABORT;
                                      $$ = make_item_list(list);
                                    }
    | QSTRING_P                     { $$ = make_item_string(&$1);  }
    | NULL_P                        { $$ = make_item_string(NULL); }
    | TRUE_P                        { $$ = make_item_bool(true);   }
    | FALSE_P                       { $$ = make_item_bool(false);  }
    | NUMERIC_P                     {
                                      $$ = make_item_numeric(scanner, result, &$1);
                                      if ($$ == NULL)
                                          YYABORT;
                                    }
    | INT_P                         {
                                      $$ = make_item_numeric(scanner, result, &$1);
                                      if ($$ == NULL)
                                          YYABORT;
                                    }
    | '(' fexpr ')'                 { $$ = $2; }
    | '+' fexpr %prec UPLUS         { $$ = make_item_unary(JSONPATH_PLUS, $2); }
    | '-' fexpr %prec UMINUS        { $$ = make_item_unary(JSONPATH_MINUS, $2); }
    | fexpr '+' fexpr               { $$ = make_item_binary(JSONPATH_ADD, $1, $3); }
    | fexpr '-' fexpr               { $$ = make_item_binary(JSONPATH_SUB, $1, $3); }
    | fexpr '*' fexpr               { $$ = make_item_binary(JSONPATH_MUL, $1, $3); }
    | fexpr '/' fexpr               { $$ = make_item_binary(JSONPATH_DIV, $1, $3); }
    | fexpr '%' fexpr               { $$ = make_item_binary(JSONPATH_MOD, $1, $3); }
    | fexpr AND_P          fexpr    { $$ = make_item_binary(JSONPATH_AND, $1, $3); }
    | fexpr OR_P           fexpr    { $$ = make_item_binary(JSONPATH_OR, $1, $3); }
    | fexpr EQUAL_P        fexpr    { $$ = make_item_binary(JSONPATH_EQUAL, $1, $3); }
    | fexpr NOTEQUAL_P     fexpr    { $$ = make_item_binary(JSONPATH_NOTEQUAL, $1, $3); }
    | fexpr LESS_P         fexpr    { $$ = make_item_binary(JSONPATH_LESS, $1, $3); }
    | fexpr GREATER_P      fexpr    { $$ = make_item_binary(JSONPATH_GREATER, $1, $3); }
    | fexpr LESSEQUAL_P    fexpr    { $$ = make_item_binary(JSONPATH_LESSOREQUAL, $1, $3); }
    | fexpr GREATEREQUAL_P fexpr    { $$ = make_item_binary(JSONPATH_GREATEROREQUAL, $1, $3); }
    | NOT_P fexpr                   { $$ = make_item_unary(JSONPATH_NOT, $2); }
    | fexpr REGEXOP_P QSTRING_P     {
                                      $$ = make_item_regex(scanner, result, JSONPATH_REGEX, $1, &$3);
                                      if ($$ == NULL)
                                          YYABORT;
                                    }
    | ABS_P '(' fexpr ')'           { $$ = make_item_call(JSONPATH_ABS, $3); }
    | AVG_P '(' fexpr ')'           { $$ = make_item_call(JSONPATH_AVG, $3); }
    | MAX_P '(' fexpr ')'           { $$ = make_item_call(JSONPATH_MAX, $3); }
    | MIN_P '(' fexpr ')'           { $$ = make_item_call(JSONPATH_MIN, $3); }
    | CEIL_P '(' fexpr')'           { $$ = make_item_call(JSONPATH_CEIL, $3); }
    | VALUE_P '(' fexpr ')'         { $$ = make_item_call(JSONPATH_VALUE, $3); }
    | COUNT_P '(' fexpr ')'         { $$ = make_item_call(JSONPATH_COUNT, $3); }
    | FLOOR_P '(' fexpr ')'         { $$ = make_item_call(JSONPATH_FLOOR, $3); }
    | MATCH_P '(' fexpr ',' QSTRING_P ')' {
                                      $$ = make_item_regex(scanner, result, JSONPATH_MATCH, $3, &$5);
                                      if ($$ == NULL)
                                          YYABORT;
                                    }
    | DOUBLE_P '(' fexpr ')'        { $$ = make_item_call(JSONPATH_DOUBLE, $3); }
    | LENGTH_P '(' fexpr ')'        { $$ = make_item_call(JSONPATH_LENGTH, $3); }
    | SEARCH_P '(' fexpr ',' QSTRING_P')' {
                                      $$ = make_item_regex(scanner, result, JSONPATH_SEARCH, $3, &$5);
                                      if ($$ == NULL)
                                          YYABORT;
                                    }
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

    if (s->val == NULL)
        return NULL;

    jsonpath_item_t *v = make_item_type(JSONPATH_STRING);
    if (v == NULL)
        return NULL;
    v->value.string.val = strdup(s->val);
    if (v->value.string.val == NULL) {
        free(v);
        return NULL;
    }
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

static jsonpath_item_t *make_item_numeric(void *scanner, jsonpath_parse_result_t *result,
                                          jsonpath_string_t *s)
{
    if (s->val == NULL) {
        jsonpath_yyerror(NULL, scanner, result, "out of memory");
        return NULL;
    }

    char *endptr = NULL;
    double numeric = strtod(s->val, &endptr);
    if ((endptr == s->val) || (*endptr != '\0') || !isfinite(numeric)) {
        jsonpath_yyerror(NULL, scanner, result, "invalid numeric literal");
        return NULL;
    }

    jsonpath_item_t *v = make_item_type(JSONPATH_NUMERIC);
    if (v == NULL) {
        jsonpath_yyerror(NULL, scanner, result, "out of memory");
        return NULL;
    }

    v->value.numeric = numeric;
    return v;
}

static int make_integer(jsonpath_string_t *s, bool negative, int32_t *ret)
{
    if (s->val == NULL)
        return -1;

    errno = 0;
    char *endptr = NULL;
    long long value = strtoll(s->val, &endptr, 10);
    if ((errno != 0) || (endptr == s->val) || (*endptr != '\0') ||
        (value < 0) || (value > JSONPATH_MAX_SAFE_INTEGER))
        return -1;

    /* Indexes outside of the int32_t range never select an element: clamp them,
     * keeping INT32_MAX and -INT32_MAX for the default bounds of the slices. */
    if (value > (INT32_MAX - 1))
        value = INT32_MAX - 1;

    *ret = negative ? -(int32_t)value : (int32_t)value;
    return 0;
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
    jsonpath_item_t *v = NULL;
    if ((la != NULL) && (ra != NULL))
        v = make_item_type(type);
    if (v == NULL) {
        jsonpath_item_free(la);
        jsonpath_item_free(ra);
        return NULL;
    }

    v->value.args.left = la;
    v->value.args.right = ra;

    return v;
}

static jsonpath_item_t *make_item_unary(jsonpath_item_type_t type, jsonpath_item_t *a)
{
    jsonpath_item_t *v = NULL;
    if (a != NULL)
        v = make_item_type(type);
    if (v == NULL) {
        jsonpath_item_free(a);
        return NULL;
    }

    v->value.arg = a;

    return v;
}

static jsonpath_item_t *make_item_call(jsonpath_item_type_t type, jsonpath_item_t *arg)
{
    jsonpath_item_t *v = NULL;
    if (arg != NULL)
        v = make_item_type(type);
    if (v == NULL) {
        jsonpath_item_free(arg);
        return NULL;
    }

    v->value.arg = arg;

    return v;
}

static void list_free_items(jsonpath_list_t *list)
{
    if (list == NULL)
        return;

    int len = jsonpath_list_length(list);
    for (int i = 0; i < len ; i++) {
        jsonpath_list_cell_t *c = jsonpath_list_nth_cell(list, i);
        if (c == NULL)
            continue;
        jsonpath_item_free(c->ptr_value);
    }
    jsonpath_list_free(list);
}

static jsonpath_list_t *list_append_item(void *scanner, jsonpath_parse_result_t *result,
                                         jsonpath_list_t *list, jsonpath_item_t *item)
{
    jsonpath_list_t *nlist = NULL;
    if (item != NULL)
        nlist = jsonpath_list_append(list, item);

    if (nlist == NULL) {
        list_free_items(list);
        jsonpath_item_free(item);
        jsonpath_yyerror(NULL, scanner, result, "out of memory");
    }

    return nlist;
}

static jsonpath_list_t *list_prepend_item(void *scanner, jsonpath_parse_result_t *result,
                                          jsonpath_list_t *list, jsonpath_item_t *item)
{
    jsonpath_list_t *nlist = NULL;
    if (item != NULL)
        nlist = jsonpath_list_prepend(list, item);

    if (nlist == NULL) {
        list_free_items(list);
        jsonpath_item_free(item);
        jsonpath_yyerror(NULL, scanner, result, "out of memory");
    }

    return nlist;
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
    if (list == NULL)
        return NULL;

    jsonpath_item_t *v = make_item_type(JSONPATH_UNION);
    if (v == NULL) {
        list_free_items(list);
        return NULL;
    }

    int len = jsonpath_list_length(list);

    v->value.iunion.len = len;
    v->value.iunion.items = calloc(len, sizeof(jsonpath_item_t *));
    if (v->value.iunion.items == NULL) {
        free(v);
        list_free_items(list);
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
    if (list == NULL)
        return NULL;

    jsonpath_item_t *v = make_item_type(JSONPATH_DSC_UNION);
    if (v == NULL) {
        list_free_items(list);
        return NULL;
    }

    int len = jsonpath_list_length(list);

    v->value.iunion.len = len;
    v->value.iunion.items = calloc(len, sizeof(jsonpath_item_t *));
    if (v->value.iunion.items == NULL) {
        free(v);
        list_free_items(list);
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

static jsonpath_item_t *make_item_regex(void *scanner, jsonpath_parse_result_t *result,
                                        jsonpath_item_type_t type, jsonpath_item_t *expr,
                                        jsonpath_string_t *pattern)
{
    jsonpath_item_t *v = NULL;
    if ((expr != NULL) && (pattern->val != NULL))
        v = make_item_type(type);
    if (v == NULL) {
        jsonpath_item_free(expr);
        jsonpath_yyerror(NULL, scanner, result, "out of memory");
        return NULL;
    }

    v->value.regex.expr = expr;
    v->value.regex.pattern = strdup(pattern->val);
    if (v->value.regex.pattern == NULL) {
        jsonpath_item_free(v);
        jsonpath_yyerror(NULL, scanner, result, "out of memory");
        return NULL;
    }

    int cflags = REG_EXTENDED;
    if (type != JSONPATH_MATCH)
        cflags |= REG_NOSUB;

    int status = regcomp(&v->value.regex.regex,  v->value.regex.pattern, cflags);
    if (status != 0) {
        char errbuf[128];
        regerror(status, &v->value.regex.regex, errbuf, sizeof(errbuf));
        char message[JSONPATH_ERROR_MSG_SIZE];
        snprintf(message, sizeof(message), "invalid regular expression \"%s\": %s",
                 v->value.regex.pattern, errbuf);
        free(v->value.regex.pattern);
        v->value.regex.pattern = NULL;
        jsonpath_item_free(v);
        jsonpath_yyerror(NULL, scanner, result, message);
        return NULL;
    }

    return v;
}
