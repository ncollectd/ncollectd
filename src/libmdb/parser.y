%{
// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "libmetric/metric_match.h"
#include "libmdb/mql.h"
#include "libmdb/node.h"
#include "libmdb/util.h"
#include "libmdb/parser.h"
#include "libmdb/scanner.h"

void mql_yyerror (YYLTYPE *yylloc, __attribute__((unused)) void *scanner,
                                     mql_status_t *status, const char *msg)
{
    if (yylloc != NULL) {
        status->first_line = yylloc->first_line;
        status->first_column = yylloc->first_column;
        status->last_line = yylloc->last_line;
        status->last_column = yylloc->last_column;
    }
    status->errmsg = msg;
    return;
}

%}

%define api.pure full
%define api.prefix {mql_yy}
%define parse.trace true
%define parse.error verbose

%lex-param   { void *scanner }
%parse-param { void *scanner }{ mql_status_t *status }
%locations

%union {
    double  number;
    uint64_t duration;
    char     *string;
    mql_node_t  *node;
    mql_node_list_t *list;
    mql_labels_t *labels;
    metric_match_pair_t *label_match;
    metric_match_set_t *labels_match;
    mql_node_group_mod_t *group_mod;
}

%token <number> NUMBER

%token <string> AND
%token <string> AVG
%token <string> BOOL
%token <string> BOTTOMK
%token <string> BY
%token <string> COUNT
%token <string> COUNT_VALUES
%token <string> GROUP
%token <string> GROUP_LEFT
%token <string> GROUP_RIGHT
%token <string> IDENTIFIER
%token <string> IGNORING
%token <string> MAX
%token <string> MIN
%token <string> OFFSET
%token <string> ON
%token <string> OR
%token <string> QUANTILE
%token <string> STDDEV
%token <string> STDVAR
%token <string> SUM
%token <string> TOPK
%token <string> UNLESS
%token <string> WITHOUT

%token <string> START
%token <string> END

%token <string> STRING

%token <duration> DURATION

%token SUB
%token ADD
%token MUL
%token MOD
%token DIV
%token EQLC
%token NEQ
%token LTE
%token LSS
%token GTE
%token GTR
%token EQL_REGEX
%token NEQ_REGEX
%token POW
%token AT
%token LEFT_PAREN
%token RIGHT_PAREN
%token LEFT_BRACE
%token RIGHT_BRACE
%token LEFT_BRACKET
%token RIGHT_BRACKET
%token COMMA
%token EQL
%token COLON
%token SEMICOLON

%type <string> grouping_label
%type <string> metric_identifier
%type <string> label_identifier

%type <number> aggregate_op
%type <number> match_op

%type <duration> duration

%type <node> expr
%type <node> binary_expr
%type <node> aggregate_expr
%type <node> offset_expr
%type <node> at_expr
%type <node> subquery_expr
%type <node> vector_selector

%type <list> function_call_body
%type <list> function_call_args

%type <group_mod> bool_modifier
%type <group_mod> on_or_ignoring
%type <group_mod> group_modifiers

%type <labels_match> label_matchers
%type <labels_match> label_match_list

%type <label_match> label_matcher

%type <labels> grouping_label_list
%type <labels> grouping_labels

%left OR
%left AND UNLESS
%left EQLC GTE GTR LSS LTE NEQ
%left ADD SUB
%left MUL DIV MOD
%right POW

%nonassoc OFFSET
%nonassoc AT
%right LEFT_BRACKET

%start start_expr

%%

start_expr
    : expr
        { status->root = $1; }
    ;

expr
    : aggregate_expr
    | binary_expr
    | IDENTIFIER function_call_body
        { $$ = mql_node_call($1, $2); }
    | expr LEFT_BRACKET duration RIGHT_BRACKET
        { $$ = mql_node_matrix($1, $3); }
    | NUMBER
        { $$ = mql_node_number($1); }
    | offset_expr
    | at_expr
    | LEFT_PAREN expr RIGHT_PAREN
        { $$ = $2; }
    | STRING
        { $$ = mql_node_string($1); }
    | subquery_expr
        { $$ = $1; }
    | ADD expr %prec MUL
        { $$ = mql_node_unary(MQL_UNARY_OP_ADD, $2); }
    | SUB expr %prec MUL
        { $$ = mql_node_unary(MQL_UNARY_OP_SUB, $2); }
    | vector_selector
    ;

duration
    : DURATION
    ;

aggregate_op
    : AVG
        { $$ = MQL_AGGREGATE_OP_AVG; }
    | BOTTOMK
        { $$ = MQL_AGGREGATE_OP_BOTTOMK; }
    | COUNT
        { $$ = MQL_AGGREGATE_OP_COUNT; }
    | COUNT_VALUES
        { $$ = MQL_AGGREGATE_OP_COUNT_VALUES; }
    | GROUP
        { $$ = MQL_AGGREGATE_OP_GROUP; }
    | MAX
        { $$ = MQL_AGGREGATE_OP_MAX; }
    | MIN
        { $$ = MQL_AGGREGATE_OP_MIN; }
    | QUANTILE
        { $$ = MQL_AGGREGATE_OP_QUANTILE; }
    | STDDEV
        { $$ = MQL_AGGREGATE_OP_STDDEV; }
    | STDVAR
        { $$ = MQL_AGGREGATE_OP_STDVAR; }
    | SUM
        { $$ = MQL_AGGREGATE_OP_SUM; }
    | TOPK
        { $$ = MQL_AGGREGATE_OP_TOPK; }
    ;

aggregate_expr
    : aggregate_op BY grouping_labels function_call_body
        { $$ = mql_node_aggregate($1, MQL_AGGREGATE_BY, $3, $4); }
    | aggregate_op WITHOUT grouping_labels function_call_body
        { $$ = mql_node_aggregate($1, MQL_AGGREGATE_WITHOUT, $3, $4); }
    | aggregate_op function_call_body BY grouping_labels
        { $$ = mql_node_aggregate($1, MQL_AGGREGATE_BY, $4, $2); }
    | aggregate_op function_call_body WITHOUT grouping_labels
        { $$ = mql_node_aggregate($1, MQL_AGGREGATE_WITHOUT, $4, $2); }
    | aggregate_op function_call_body
        { $$ = mql_node_aggregate($1, MQL_AGGREGATE_NONE, NULL, $2); }
    | aggregate_op error
        { $$ = NULL; /*FIXME XXX */ }
    ;

bool_modifier
    : /* empty */
        { $$ = mql_node_group_mod(false); }
    | BOOL
        { $$ = mql_node_group_mod(true); }
    ;

on_or_ignoring
    : bool_modifier IGNORING grouping_labels
        { $$ = mql_node_group_mod_inclexcl($1, MQL_INCLEXCL_IGNORING, $3); }
    | bool_modifier ON grouping_labels
        { $$ = mql_node_group_mod_inclexcl($1, MQL_INCLEXCL_ON, $3); }
    ;

group_modifiers
    : bool_modifier /* empty */
        { $$ = $1; }
    | on_or_ignoring /* empty */
        { $$ = $1; }
    | on_or_ignoring GROUP_LEFT grouping_labels
        { $$ = mql_node_group_mod_group($1, MQL_GROUP_LEFT, $3); }
    | on_or_ignoring GROUP_RIGHT grouping_labels
        { $$ = mql_node_group_mod_group($1, MQL_GROUP_RIGHT, $3); }
    ;

binary_expr
    : expr ADD       group_modifiers expr
        { $$ = mql_node_binary($1, MQL_BINARY_OP_ADD, $3, $4); }
    | expr DIV       group_modifiers expr
        { $$ = mql_node_binary($1, MQL_BINARY_OP_DIV, $3, $4); }
    | expr EQLC      group_modifiers expr
        { $$ = mql_node_binary($1, MQL_BINARY_OP_EQLC, $3, $4); }
    | expr GTE       group_modifiers expr
        { $$ = mql_node_binary($1, MQL_BINARY_OP_GTE , $3, $4); }
    | expr GTR       group_modifiers expr
        { $$ = mql_node_binary($1, MQL_BINARY_OP_GTR, $3, $4); }
    | expr AND       group_modifiers expr
        { $$ = mql_node_binary($1, MQL_BINARY_OP_AND, $3, $4); }
    | expr OR            group_modifiers expr
        { $$ = mql_node_binary($1, MQL_BINARY_OP_OR , $3, $4); }
    | expr LSS       group_modifiers expr
        { $$ = mql_node_binary($1, MQL_BINARY_OP_LSS, $3, $4); }
    | expr LTE       group_modifiers expr
        { $$ = mql_node_binary($1, MQL_BINARY_OP_LTE, $3, $4); }
    | expr UNLESS  group_modifiers expr
        { $$ = mql_node_binary($1, MQL_BINARY_OP_UNLESS, $3, $4); }
    | expr MOD       group_modifiers expr
        { $$ = mql_node_binary($1, MQL_BINARY_OP_MOD, $3, $4); }
    | expr MUL       group_modifiers expr
        { $$ = mql_node_binary($1, MQL_BINARY_OP_MUL, $3, $4); }
    | expr NEQ       group_modifiers expr
        { $$ = mql_node_binary($1, MQL_BINARY_OP_NEQ, $3, $4); }
    | expr POW       group_modifiers expr
        { $$ = mql_node_binary($1, MQL_BINARY_OP_POW, $3, $4); }
    | expr SUB       group_modifiers expr
        { $$ = mql_node_binary($1, MQL_BINARY_OP_SUB, $3, $4); }
    ;

function_call_body
    : LEFT_PAREN function_call_args RIGHT_PAREN
        { $$ = $2; }
    | LEFT_PAREN RIGHT_PAREN
        { $$ = NULL; }
    ;

function_call_args
    : function_call_args COMMA expr
        { $$ = mql_node_list_append($1, $3); }
    | expr
        { $$ = mql_node_list_append(NULL, $1); }
    | function_call_args COMMA
        { $$ = $1; }
    ;

offset_expr
    : expr OFFSET duration
        { $$ = mql_node_offset($1, $3); }
    | expr OFFSET SUB duration
        { $$ = mql_node_offset($1, -$4); }
    | expr OFFSET error
        { $$ = NULL; /*FIXME XXX*/ }
    ;

at_expr
    : expr AT NUMBER
        { $$ = mql_node_at($1, MQL_AT_TIMESTAMP, $3); }
    | expr AT START LEFT_PAREN RIGHT_PAREN
        { $$ = mql_node_at($1, MQL_AT_START, 0); }
    | expr AT END LEFT_PAREN RIGHT_PAREN
        { $$ = mql_node_at($1, MQL_AT_END, 0); }
    | expr AT error
        { $$ = NULL; /* FIXME XXX */}
    ;

subquery_expr
    : expr LEFT_BRACKET duration COLON duration RIGHT_BRACKET
        { $$ = mql_node_subquery($1, $3, $5); }
    | expr LEFT_BRACKET duration COLON RIGHT_BRACKET
        { $$ = mql_node_subquery($1, $3, 0); }
    | expr LEFT_BRACKET duration COLON duration error
        { $$ = NULL; /*FIXME XXX*/ }
    | expr LEFT_BRACKET duration COLON error
        { $$ = NULL; /*FIXME XXX*/ }
    | expr LEFT_BRACKET duration error
        { $$ = NULL; /*FIXME XXX*/ }
    | expr LEFT_BRACKET error
        { $$ = NULL; /*FIXME XXX*/ }
    ;

metric_identifier
    : AVG
        { $$ = "avg"; }
    | BOTTOMK
        { $$ = "bottomk"; }
    | BY
        { $$ = "by"; }
    | COUNT
        { $$ = "count"; }
    | COUNT_VALUES
        { $$ = "count_values"; }
    | END
        { $$ = "end"; }
    | GROUP
        { $$ = "group"; }
    | IDENTIFIER
        { $$ = $1; }
    | AND
        { $$ = "and"; }
    | OR
        { $$ = "or"; }
    | UNLESS
        { $$ = "unless"; }
    | MAX
        { $$ = "max"; }
    | MIN
        { $$ = "min"; }
    | OFFSET
        { $$ = "offset"; }
    | QUANTILE
        { $$ = "quantile"; }
    | START
        { $$ = "start"; }
    | STDDEV
        { $$ = "stddev"; }
    | STDVAR
        { $$ = "stdvar"; }
    | SUM
        { $$ = "sum"; }
    | TOPK
        { $$ = "topk"; }
    | WITHOUT
        { $$ = "without"; }
    | error
        { $$ = NULL; /*FIXME*/ }
    ;

vector_selector
    : metric_identifier label_matchers
        { $$ = mql_node_vector($1, $2); }
    | metric_identifier
        { $$ = mql_node_vector($1, NULL); }
    | label_matchers
        { $$ = mql_node_vector(NULL, $1); /* FIXME ¿?  */}
    ;

label_matchers
    : LEFT_BRACE label_match_list RIGHT_BRACE
        { $$ = $2; }
    | LEFT_BRACE label_match_list COMMA RIGHT_BRACE
        { $$ = $2; }
    | LEFT_BRACE RIGHT_BRACE
        { $$ = NULL; }
    ;

label_match_list
    : label_match_list COMMA label_matcher
        { metric_match_set_append($1, $3); $$ = $1; }
    | label_matcher
        {
          metric_match_set_t *set = metric_match_set_alloc();
          if (set == NULL) {
              $$ = NULL;
          } else {
              metric_match_set_append(set, $1);
              $$ = set;
          }
        }
    | label_match_list error
        { $$ = NULL; /*FIXME XXX */ }
    ;

label_matcher
    : label_identifier match_op STRING
        { $$ = metric_match_pair_alloc($1, $2, $3); }
    | label_identifier match_op error
        { $$ = NULL; /* FIXME XXX */ }
    | label_identifier error
        { $$ = NULL; /* FIXME XXX */ }
    ;

match_op
    : EQL
        { $$ = METRIC_MATCH_OP_EQL; }
    | NEQ
        { $$ = METRIC_MATCH_OP_NEQ; }
    | EQL_REGEX
        { $$ = METRIC_MATCH_OP_EQL_REGEX; }
    | NEQ_REGEX
        { $$ = METRIC_MATCH_OP_NEQ_REGEX; }
    ;

label_identifier
    : AND
        { $$ = strdup("and"); }
    | AVG
        { $$ = strdup("avg"); }
    | BOOL
        { $$ = strdup("bool"); }
    | BOTTOMK
        { $$ = strdup("bottomk"); }
    | BY
        { $$ = strdup("by"); }
    | COUNT
        { $$ = strdup("count"); }
    | COUNT_VALUES
        { $$ = strdup("count_values"); }
    | END
        { $$ = strdup("end"); }
    | GROUP
        { $$ = strdup("group"); }
    | GROUP_LEFT
        { $$ = strdup("group_left"); }
    | GROUP_RIGHT
        { $$ = strdup("group_right"); }
    | IDENTIFIER
        { $$ = $1; }
    | IGNORING
        { $$ = strdup("ignoring"); }
    | MAX
        { $$ = strdup("max"); }
    | MIN
        { $$ = strdup("min"); }
    | OFFSET
        { $$ = strdup("offset"); }
    | ON
        { $$ = strdup("on"); }
    | OR
        { $$ = strdup("or"); }
    | QUANTILE
        { $$ = strdup("quantile"); }
    | START
        { $$ = strdup("start"); }
    | STDDEV
        { $$ = strdup("stddev"); }
    | STDVAR
        { $$ = strdup("stdvar"); }
    | SUM
        { $$ = strdup("sum"); }
    | TOPK
        { $$ = strdup("topk"); }
    | UNLESS
        { $$ = strdup("unless"); }
    | WITHOUT
        { $$ = strdup("without"); }
    | error
        { $$ = NULL; /* FIXME XXX */ }
    ;

grouping_label
    : AND
        { $$ = strdup("and"); }
    | AVG
        { $$ = strdup("avg"); }
    | BOOL
        { $$ = strdup("bool"); }
    | BOTTOMK
        { $$ = strdup("bottomk"); }
    | BY
        { $$ = strdup("by"); }
    | COUNT
        { $$ = strdup("count"); }
    | COUNT_VALUES
        { $$ = strdup("count_values"); }
    | END
        { $$ = strdup("end"); }
    | GROUP
        { $$ = strdup("group"); }
    | GROUP_LEFT
        { $$ = strdup("group_left"); }
    | GROUP_RIGHT
        { $$ = strdup("group_right"); }
    | IDENTIFIER
        { $$ = $1; }
    | IGNORING
        { $$ = strdup("ignoring"); }
    | MAX
        { $$ = strdup("max"); }
    | MIN
        { $$ = strdup("min"); }
    | OFFSET
        { $$ = strdup("offset"); }
    | ON
        { $$ = strdup("on"); }
    | OR
        { $$ = strdup("or"); }
    | UNLESS
        { $$ = strdup("unless"); }
    | QUANTILE
        { $$ = strdup("quantile"); }
    | START
        { $$ = strdup("start"); }
    | STDDEV
        { $$ = strdup("stdev"); }
    | STDVAR
        { $$ = strdup("stdvar"); }
    | SUM
        { $$ = strdup("sum"); }
    | TOPK
        { $$ = strdup("topk"); }
    | error
        { $$ = NULL; /*FIXME XXX*/ }
    ;

grouping_label_list
    : grouping_label_list COMMA grouping_label
        { $$ = mql_labels_append($1, $3); }
    | grouping_label
        { $$ = mql_labels_append(NULL, $1); }
    | grouping_label_list error
        { $$ = NULL; /*FIXME XXX*/ }
    ;

grouping_labels
    : LEFT_PAREN grouping_label_list RIGHT_PAREN
        { $$ = $2; }
    | LEFT_PAREN grouping_label_list COMMA RIGHT_PAREN
        { $$ = $2; }
    | LEFT_PAREN RIGHT_PAREN
        { $$ = NULL; }
    | error
        { $$ = NULL; /*FIXME XXX*/ }
    ;

