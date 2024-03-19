/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

%{
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <regex.h>

#include "libexpr/expr.h"
#include "libexpr/parser.h"
#include "libexpr/scanner.h"

static expr_node_t *make_string(char *str);
static expr_node_t *make_number(double number);
static expr_node_t *make_boolean(bool boolean);
static expr_node_t *make_match(bool match, expr_node_t *expr, char *pattern);
static expr_node_t *make_unary(expr_node_type_t type, expr_node_t *arg);
static expr_node_t *make_binary(expr_node_type_t type, expr_node_t *left, expr_node_t *right);
static expr_node_t *make_call0(expr_node_type_t type);
static expr_node_t *make_call1(expr_node_type_t type, expr_node_t *arg0);
static expr_node_t *make_call2(expr_node_type_t type, expr_node_t *arg0, expr_node_t *arg1);
static expr_node_t *make_if(expr_node_t *expr, expr_node_t *expr_then, expr_node_t *expr_else);
static expr_node_t *make_loop(expr_node_type_t type, expr_node_t *id,
                              expr_node_t *start, expr_node_t *end, expr_node_t *step,
                              expr_node_t *arg);

static expr_node_t *make_identifier(char *name);
static expr_node_t *identifier_append_idx(expr_node_t *id, int idx);
static expr_node_t *identifier_append_name(expr_node_t *id, char *name);
static expr_node_t *resolve_identifier(expr_node_t *node, expr_symtab_t *symtab);

void expr_yyerror(EXPR_YYLTYPE *loc, void *scanner, void *result, const char *message);

%}

%define api.pure full
%define api.prefix {expr_yy}
%define parse.trace true
%define parse.error verbose

%lex-param   {void *scanner}
%parse-param {void *scanner}{expr_parse_result_t *result}
%locations

%union {
    expr_node_t *node;
    expr_node_type_t type;
    double number;
    char *string;
}

%token <type> T_FUNC_RANDOM T_FUNC_TIME
%token <type> T_FUNC_EXP T_FUNC_EXPM1 T_FUNC_LOG T_FUNC_LOG2 T_FUNC_LOG10
%token <type> T_FUNC_LOG1P T_FUNC_SQRT T_FUNC_CBRT T_FUNC_SIN T_FUNC_COS
%token <type> T_FUNC_TAN T_FUNC_ASIN T_FUNC_ACOS T_FUNC_ATAN T_FUNC_COSH
%token <type> T_FUNC_SINH T_FUNC_TANH T_FUNC_CTAN T_FUNC_ACOSH T_FUNC_ASINH
%token <type> T_FUNC_ATANH T_FUNC_ABS T_FUNC_CEIL T_FUNC_FLOOR T_FUNC_ROUND
%token <type> T_FUNC_TRUNC T_FUNC_ISNAN T_FUNC_ISINF T_FUNC_ISNORMAL
%token <type> T_FUNC_POW T_FUNC_HYPOT T_FUNC_ATAN2 T_FUNC_MAX T_FUNC_MIN
%token <type> T_FUNC_SUM T_FUNC_AVG T_FUNC_ALL T_FUNC_ANY

%type <type> call0 call1 call2 aggregate
%type <node> expr identifier
%type <string> T_STRING
%type <number> T_NUMBER
%type <string> T_IDENTIFIER

%token T_TRUE
%token T_FALSE
%token T_COMMA
%token T_DOT
%token T_STRING
%token T_NUMBER
%token T_IDENTIFIER
%token T_LBRACK
%token T_RBRACK
%token T_FOR
%token T_TO

%right	 T_QMARK
%right	 T_COLON
%left    T_OR
%left    T_AND
%nonassoc T_EQL T_NQL T_LT T_GT T_LTE T_GTE T_MATCH T_NMATCH
%right   T_NOT
%left    T_PLUS T_MINUS
%left    T_MUL T_DIV T_MOD
%left    UMINUS
%left    T_BIT_AND T_BIT_OR T_BIT_XOR
%left    T_BIT_LSHIFT T_BIT_RSHIFT
%right   T_BIT_NOT

%nonassoc T_LPAREN T_RPAREN

%start input
%%

input
    : expr {
        result->root = $1;
        result->error = false;
        (void) yynerrs;
      }
    ;

call0
    : T_FUNC_RANDOM   { $$ = EXPR_FUNC_RANDOM;   }
    | T_FUNC_TIME     { $$ = EXPR_FUNC_TIME;      }
    ;

call1
    : T_FUNC_EXP      { $$ = EXPR_FUNC_EXP;      }
    | T_FUNC_EXPM1    { $$ = EXPR_FUNC_EXPM1;    }
    | T_FUNC_LOG      { $$ = EXPR_FUNC_LOG;      }
    | T_FUNC_LOG2     { $$ = EXPR_FUNC_LOG2;     }
    | T_FUNC_LOG10    { $$ = EXPR_FUNC_LOG10;    }
    | T_FUNC_LOG1P    { $$ = EXPR_FUNC_LOG1P;    }
    | T_FUNC_SQRT     { $$ = EXPR_FUNC_SQRT;     }
    | T_FUNC_CBRT     { $$ = EXPR_FUNC_CBRT;     }
    | T_FUNC_SIN      { $$ = EXPR_FUNC_SIN;      }
    | T_FUNC_COS      { $$ = EXPR_FUNC_COS;      }
    | T_FUNC_TAN      { $$ = EXPR_FUNC_TAN;      }
    | T_FUNC_ASIN     { $$ = EXPR_FUNC_ASIN;     }
    | T_FUNC_ACOS     { $$ = EXPR_FUNC_ACOS;     }
    | T_FUNC_ATAN     { $$ = EXPR_FUNC_ATAN;     }
    | T_FUNC_COSH     { $$ = EXPR_FUNC_COSH;     }
    | T_FUNC_SINH     { $$ = EXPR_FUNC_SINH;     }
    | T_FUNC_TANH     { $$ = EXPR_FUNC_TANH;     }
    | T_FUNC_ACOSH    { $$ = EXPR_FUNC_ACOSH;    }
    | T_FUNC_ASINH    { $$ = EXPR_FUNC_ASINH;    }
    | T_FUNC_ATANH    { $$ = EXPR_FUNC_ATANH;    }
    | T_FUNC_ABS      { $$ = EXPR_FUNC_ABS;      }
    | T_FUNC_CEIL     { $$ = EXPR_FUNC_CEIL;     }
    | T_FUNC_FLOOR    { $$ = EXPR_FUNC_FLOOR;    }
    | T_FUNC_ROUND    { $$ = EXPR_FUNC_ROUND;    }
    | T_FUNC_TRUNC    { $$ = EXPR_FUNC_TRUNC;    }
    | T_FUNC_ISNAN    { $$ = EXPR_FUNC_ISNAN;    }
    | T_FUNC_ISINF    { $$ = EXPR_FUNC_ISINF;    }
    | T_FUNC_ISNORMAL { $$ = EXPR_FUNC_ISNORMAL; }
    ;

call2
    : T_FUNC_POW      { $$ = EXPR_FUNC_POW;      }
    | T_FUNC_HYPOT    { $$ = EXPR_FUNC_HYPOT;    }
    | T_FUNC_ATAN2    { $$ = EXPR_FUNC_ATAN2;    }
    | T_FUNC_MAX      { $$ = EXPR_FUNC_MAX;      }
    | T_FUNC_MIN      { $$ = EXPR_FUNC_MIN;      }
    ;

aggregate
    : T_FUNC_SUM      { $$ = EXPR_AGG_SUM;       }
    | T_FUNC_AVG      { $$ = EXPR_AGG_AVG;       }
    | T_FUNC_ALL      { $$ = EXPR_AGG_ALL;       }
    | T_FUNC_ANY      { $$ = EXPR_AGG_ANY;       }
    ;

identifier
    : T_IDENTIFIER                          { $$ = make_identifier($1);            }
    | identifier T_LBRACK T_NUMBER T_RBRACK { $$ = identifier_append_idx($1, $3);  }
    | identifier T_DOT T_IDENTIFIER         { $$ = identifier_append_name($1, $3); }
    ;

expr
    : T_LPAREN expr T_RPAREN    { $$ = $2; }
    | expr T_EQL    expr        { $$ = make_binary(EXPR_EQL, $1, $3);  }
    | expr T_NQL    expr        { $$ = make_binary(EXPR_NQL, $1, $3);  }
    | expr T_LT     expr        { $$ = make_binary(EXPR_LT, $1, $3);   }
    | expr T_LTE    expr        { $$ = make_binary(EXPR_LTE, $1, $3);  }
    | expr T_GT     expr        { $$ = make_binary(EXPR_GT, $1, $3);   }
    | expr T_GTE    expr        { $$ = make_binary(EXPR_GTE, $1, $3);  }
    | expr T_MATCH  T_STRING    { $$ = make_match(true, $1, $3);       }
    | expr T_NMATCH T_STRING    { $$ = make_match(false, $1, $3);      }
    | expr T_OR     expr        { $$ = make_binary(EXPR_OR, $1, $3);   }
    | expr T_AND    expr        { $$ = make_binary(EXPR_AND, $1, $3);  }
    | expr T_PLUS   expr        { $$ = make_binary(EXPR_ADD, $1, $3);  }
    | expr T_MINUS  expr        { $$ = make_binary(EXPR_SUB, $1, $3);  }
    | expr T_MUL    expr        { $$ = make_binary(EXPR_MUL, $1, $3);  }
    | expr T_DIV    expr        { $$ = make_binary(EXPR_DIV, $1, $3);  }
    | expr T_MOD    expr        { $$ = make_binary(EXPR_MOD, $1, $3);  }

    | expr T_BIT_AND expr       { $$ = make_binary(EXPR_BIT_AND, $1, $3);    }
    | expr T_BIT_OR  expr       { $$ = make_binary(EXPR_BIT_OR, $1, $3);     }
    | expr T_BIT_XOR expr       { $$ = make_binary(EXPR_BIT_XOR, $1, $3);    }
    | expr T_BIT_LSHIFT expr    { $$ = make_binary(EXPR_BIT_LSHIFT, $1, $3); }
    | expr T_BIT_RSHIFT expr    { $$ = make_binary(EXPR_BIT_RSHIFT, $1, $3); }
    | T_BIT_NOT expr            { $$ = make_unary(EXPR_BIT_NOT, $2);         }

    | T_NOT expr %prec UMINUS   { $$ = make_unary(EXPR_NOT, $2);       }
    | T_MINUS expr %prec UMINUS { $$ = make_unary(EXPR_MINUS, $2);     }

    | expr T_QMARK expr T_COLON expr %prec T_QMARK { $$ = make_if($1, $3, $5); }

    | call0 T_LPAREN T_RPAREN                   { $$ = make_call0($1);         }
    | call1 T_LPAREN expr T_RPAREN              { $$ = make_call1($1, $3);     }
    | call2 T_LPAREN expr T_COMMA expr T_RPAREN { $$ = make_call2($1, $3, $5); }
    | aggregate T_LPAREN T_LBRACK expr T_FOR identifier T_TO identifier T_RBRACK T_RPAREN {
         $$ = make_loop($1, $6, NULL, $8, NULL, $4);
    }
    | identifier   { $$ = resolve_identifier($1, result->symtab); }
    | T_TRUE       { $$ = make_boolean(true);  }
    | T_FALSE      { $$ = make_boolean(false); }
    | T_NUMBER     { $$ = make_number($1);     }
    | T_STRING     { $$ = make_string($1);     }
    ;

%%

void expr_yyerror(EXPR_YYLTYPE *yylloc, __attribute__((unused)) void *scanner,
                  void *result, const char *message)
{
    expr_parse_result_t *eresult = result;

    if (eresult != NULL) {
        snprintf(eresult->error_msg, sizeof(eresult->error_msg), "error line %i: %s",
                 yylloc->first_line, message);

    } else {
        fprintf(stderr, "Parse error: %s\n", message); 
    }
}

static expr_node_t *make_node(expr_node_type_t type)
{
    expr_node_t *node = calloc(1, sizeof(*node));
    if (node == NULL) {
        return NULL;
    }

    node->type = type;
    return node;
}

static expr_node_t *make_string(char *str)
{
    if (str == NULL)
        return NULL;

    expr_node_t *node = make_node(EXPR_STRING);
    if (node == NULL)
        return NULL;

    node->string = str;
    return node;
}

static expr_node_t *make_boolean(bool boolean)
{
    expr_node_t *node = make_node(EXPR_BOOL);
    if (node == NULL)
        return NULL;

    node->boolean = boolean;
    return node;
}

static expr_node_t *make_number(double number)
{
    expr_node_t *node = make_node(EXPR_NUMBER);
    if (node == NULL)
        return NULL;

    node->number = number;
    return node;
}

static expr_node_t *resolve_identifier(expr_node_t *node, expr_symtab_t *symtab)
{
    if (node == NULL)
        return NULL;

    expr_symtab_entry_t *entry = expr_symtab_lookup(symtab, &(node->id));
    // FIXME if (entry == NULL)

    node->entry = entry;

    return node;
}
#if 0
static expr_node_t *make_identifier(expr_symtab_t *symtab, char *name)
{
    if ((symtab == NULL) || (name == NULL))
        return NULL;

    expr_node_t *node = make_node(EXPR_IDENTIFIER);
    if (node == NULL)
        return NULL;

    expr_symtab_entry_t *entry = expr_symtab_lookup(symtab, name);
    if (entry == NULL) {
        // FIXME
        free(node);
        return NULL;
    }

    node->identifier = name;
    node->entry = entry;

    return node;
}
#endif
static expr_node_t *make_identifier_append(expr_node_t *node, expr_id_item_t *item)
{
    expr_id_item_t *tmp = realloc(node->id.ptr, sizeof(node->id.ptr[0])*(node->id.num+1));
    if (tmp == NULL) {
        // FIXME
        return node;
    }

    node->id.ptr = tmp;
    node->id.ptr[node->id.num].type = item->type;
    if (item->type == EXPR_ID_NAME) {
//      node->id.ptr[node->id.num].name = strdup(item->name);
        node->id.ptr[node->id.num].name = item->name;
        if (node->id.ptr[node->id.num].name == NULL) {
            // FIXME
            return node;
        }
    } else {
        node->id.ptr[node->id.num].idx = item->idx;
    }
    node->id.num++;

    return node;
}

static expr_node_t *make_identifier(char *name)
{
    if (name == NULL)
        return NULL;

    expr_node_t *node = make_node(EXPR_IDENTIFIER);
    if (node == NULL)
        return NULL;
    return make_identifier_append(node, &(expr_id_item_t){.type = EXPR_ID_NAME, .name = name});
}

static expr_node_t *identifier_append_idx(expr_node_t *id, int idx)
{
    return make_identifier_append(id, &(expr_id_item_t){.type = EXPR_ID_IDX, .idx = idx});
}

static expr_node_t *identifier_append_name(expr_node_t *id, char *name)
{
    return make_identifier_append(id, &(expr_id_item_t){.type = EXPR_ID_NAME, .name = name});
}

static expr_node_t *make_unary(expr_node_type_t type, expr_node_t *arg)
{
    expr_node_t *node = make_node(type);
    if (node == NULL)
        return NULL;

    node->arg = arg;
    return node;
}


static expr_node_t *make_binary(expr_node_type_t type, expr_node_t *left, expr_node_t *right)
{
    expr_node_t *node = make_node(type);
    if (node == NULL)
        return NULL;

    node->left = left;
    node->right = right;
    return node;

}

static expr_node_t *make_call0(expr_node_type_t type)
{
    expr_node_t *node = make_node(type);
    if (node == NULL)
        return NULL;

    return node;

}

static expr_node_t *make_call1(expr_node_type_t type, expr_node_t *arg0)
{
    expr_node_t *node = make_node(type);
    if (node == NULL)
        return NULL;

    node->arg0 = arg0;

    return node;

}

static expr_node_t *make_call2(expr_node_type_t type, expr_node_t *arg0, expr_node_t *arg1)
{
    expr_node_t *node = make_node(type);
    if (node == NULL)
        return NULL;

    node->arg0 = arg0;
    node->arg1 = arg1;

    return node;
}

static expr_node_t *make_loop(expr_node_type_t type, expr_node_t *id,
                              expr_node_t *start, expr_node_t *end, expr_node_t *step,
                              expr_node_t *arg)
{
    expr_node_t *node = make_node(type);
    if (node == NULL)
        return NULL;

    node->loop_id = id;
    node->loop_start = start;
    node->loop_end = end;
    node->loop_step = step;
    node->loop_expr = arg;

    return node;
}

static expr_node_t *make_if(expr_node_t *expr, expr_node_t *expr_then, expr_node_t *expr_else)
{
    expr_node_t *node = make_node(EXPR_IF);
    if (node == NULL)
        return NULL;

    node->expr = expr;
    node->expr_then = expr_then;
    node->expr_else = expr_else;

    return node;
}

static expr_node_t *make_match(bool match, expr_node_t *expr, char *pattern)
{
    if ((expr == NULL) || (pattern == NULL))
        return NULL;

    expr_node_t *node = make_node(match ? EXPR_MATCH : EXPR_NMATCH);
    if (node == NULL)
        return NULL;

    int status = regcomp(&node->regex,  pattern, 0);
    if (status != 0) {
        // FIXME
        free (node);
        return NULL;
    }

    node->regex_expr = expr;
    node->pattern = pattern;

    return node;
}
