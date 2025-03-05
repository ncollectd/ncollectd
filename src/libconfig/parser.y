// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2007,2008  Florian Forster
// SPDX-FileContributor: Florian Forster <octo at collectd.org>

%{
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "libconfig/config.h"
#include "libconfig/aux_types.h"
#include "libconfig/parser.h"
#include "libconfig/scanner.h"

static char *unsquote (const char *orig);
static char *undquote (const char *orig);
static char *unrquote (const char *orig);
static char *unhquote (const char *orig);

static void yyerror(CONFIG_YYLTYPE *yylloc, void *scanner, config_status_t *status, const char *msg);
static config_file_t *get_file_ref(void);

extern config_item_t *ci_root;
extern config_file_t *c_file;
%}

%define api.pure full
%define api.prefix {config_yy}
%define parse.trace false
%define parse.error verbose

%lex-param   { void *scanner }
%parse-param { void *scanner }{ config_status_t *status }
%locations

%start entire_file

%union {
    double  number;
    int     boolean;
    char   *string;
    config_value_t  cv;
    config_item_t   ci;
    argument_list_t  al;
    statement_list_t sl;
}

%token <number> NUMBER
%token <boolean> BTRUE BFALSE
%token <string> SQUOTED_STRING DQUOTED_STRING UNQUOTED_STRING HEREDOC_STRING REGEX
%token OPENBRAC CLOSEBRAC EOL

%type <string> string
%type <string> regex
%type <string> identifier
/* arguments */
%type <cv> argument
%type <al> argument_list
/* blocks */
%type <ci> block_begin
%type <ci> block
/* statements */
%type <ci> option
%type <ci> statement
%type <sl> statement_list
%type <ci> entire_file

%%
string:
    SQUOTED_STRING     {$$ = unsquote($1);}
    | DQUOTED_STRING   {$$ = undquote($1);}
    | HEREDOC_STRING   {$$ = unhquote($1);}
    | UNQUOTED_STRING  {$$ = strdup($1);}
    ;

regex:
    REGEX            {$$ = unrquote($1);}
    ;

argument:
    NUMBER          {$$.value.number = $1; $$.type = CONFIG_TYPE_NUMBER;}
    | BTRUE         {$$.value.boolean = 1; $$.type = CONFIG_TYPE_BOOLEAN;}
    | BFALSE        {$$.value.boolean = 0; $$.type = CONFIG_TYPE_BOOLEAN;}
    | string        {$$.value.string = $1; $$.type = CONFIG_TYPE_STRING;}
    | regex         {$$.value.string = $1; $$.type = CONFIG_TYPE_REGEX;}
    ;

argument_list:
    argument_list argument
    {
        $$ = $1;
        config_value_t *tmp = realloc($$.argument, ($$.argument_num+1) * sizeof(*$$.argument));
        if (tmp == NULL) {
            yyerror(&yylloc, scanner, status, "realloc failed");
            YYERROR;
        }
        $$.argument = tmp;
        $$.argument[$$.argument_num] = $2;
        $$.argument_num++;
    }
    | argument
    {
        $$.argument = calloc(1, sizeof(*$$.argument));
        if ($$.argument == NULL) {
            yyerror(&yylloc, scanner, status, "calloc failed");
            YYERROR;
        }
        $$.argument[0] = $1;
        $$.argument_num = 1;
    }
    ;

identifier:
    UNQUOTED_STRING    {$$ = strdup ($1);}
    ;

option:
    identifier EOL
    {
        memset(&$$, 0, sizeof($$));
        $$.key = $1;
        $$.lineno = yylloc.first_line;
        $$.file = get_file_ref();
    }
    | identifier argument_list EOL
    {
        memset(&$$, 0, sizeof($$));
        $$.key = $1;
        $$.lineno = yylloc.first_line;
        $$.file = get_file_ref();
        $$.values = $2.argument;
        $$.values_num = $2.argument_num;
    }
    ;

block_begin_eol:
    OPENBRAC EOL
    | EOL OPENBRAC
    ;

block_begin:
    identifier block_begin_eol
    {
        memset(&$$, 0, sizeof($$));
        $$.key = $1;
        $$.lineno = yylloc.first_line;
        $$.file = get_file_ref();
    }
    | identifier argument_list block_begin_eol
    {
        memset(&$$, 0, sizeof($$));
        $$.key = $1;
        $$.lineno = yylloc.first_line;
        $$.file = get_file_ref();
        $$.values = $2.argument;
        $$.values_num = $2.argument_num;
    }
    ;

block:
    block_begin statement_list CLOSEBRAC EOL
    {
        $$ = $1;
        $$.children = $2.statement;
        $$.children_num = $2.statement_num;
    }
    | block_begin CLOSEBRAC EOL
    {
        $$ = $1;
        $$.children = NULL;
        $$.children_num = 0;
    }
    ;

statement:
    option     {$$ = $1;}
    | block    {$$ = $1;}
    | EOL      {memset(&$$, 0, sizeof($$));}
    ;

statement_list:
    statement_list statement
    {
        $$ = $1;
        if ($2.key != NULL) {
            config_item_t *tmp = realloc($$.statement, ($$.statement_num+1) * sizeof(*tmp));
            if (tmp == NULL) {
                yyerror(&yylloc, scanner, status, "realloc failed");
                YYERROR;
            }
            $$.statement = tmp;
            $$.statement[$$.statement_num] = $2;
            $$.statement_num++;
        }
    }
    | statement
    {
        if ($1.key != NULL) {
            $$.statement = calloc(1, sizeof(*$$.statement));
            if ($$.statement == NULL) {
                yyerror(&yylloc, scanner, status, "calloc failed");
                YYERROR;
            }
            $$.statement[0] = $1;
            $$.statement_num = 1;
        } else {
            $$.statement = NULL;
            $$.statement_num = 0;
        }
    }
    ;

entire_file:
    statement_list
    {
        ci_root = calloc(1, sizeof(*ci_root));
        if (ci_root == NULL) {
            yyerror(&yylloc, scanner, status, "calloc failed");
            YYERROR;
        }
        ci_root->children = $1.statement;
        ci_root->children_num = $1.statement_num;
    }
    | /* epsilon */
    {
        ci_root = calloc(1, sizeof(*ci_root));
        if (ci_root == NULL) {
            yyerror(&yylloc, scanner, status, "calloc failed");
            YYERROR;
        }
    }
    ;

%%
static void yyerror(CONFIG_YYLTYPE *yylloc, __attribute__((unused)) void *scanner,
                    config_status_t *status, const char *msg)
{
    if (status != NULL) {
        snprintf(status->error, sizeof(status->error), "Parse error in file '%s', line %i: %s",
                 c_file->name, yylloc->first_line, msg);

    } else {
        fprintf(stderr, "Parse error in file '%s', line %i: %s\n",
                        c_file->name, yylloc->first_line, msg);
    }
}

static bool is_octal(char c)
{
    return ((c >= '0') && (c <= '7'));
}

static bool is_hex(char c)
{
    return (((c >= '0') && (c <= '9')) ||
            ((c >= 'a') && (c <= 'f')) ||
            ((c >= 'A') && (c <= 'F')));
}

static int hex2int(char c)
{
    if ((c >= '0') && (c <= '9')) {
        return c - '0';
    } else if ((c >= 'a') && (c <='f')) {
        return c - 'a' + 10;
    } else if ((c >= 'A') && (c <='F')) {
        return c - 'A' + 10;
    }
    return 0;
}

static char *undquote (const char *orig)
{
    size_t len = strlen(orig);
    if ((len < 2) || (orig[0] != '"') || (orig[len -1] != '"'))
        return strdup(orig);

    len -= 2;
    orig++;

    char *ret = malloc(len+1);
    if (ret == NULL)
        return NULL;

    size_t n = 0;
    size_t i = 0;
    while (i < len) {
        if (orig[i] == '\\') {
            i++;
            if (i >= len)
                break;
            switch(orig[i]) {
            case '"':
                ret[n++] = '"';
                break;
            case '\\':
                ret[n++] = '\\';
                break;
            case 'a':
                ret[n++] = '\a';
                break;
            case 'b':
                ret[n++] = '\b';
                break;
            case 'f':
                ret[n++] = '\f';
                break;
            case 'v':
                ret[n++] = '\v';
                break;
            case 'n':
                ret[n++] = '\n';
                break;
            case 'r':
                ret[n++] = '\r';
                break;
            case 't':
                ret[n++] = '\t';
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7': {
                int octal = orig[i] - '0';
                if (((i+1) < len) && is_octal(orig[i+1])) {
                    i++;
                    octal <<= 3;
                    octal += orig[i] - '0';
                    if (((i+1) < len) && is_octal(orig[i+1])) {
                        i++;
                        octal <<= 3;
                        octal += orig[i] - '0';
                    }
                }
                if ((octal > 0) && (octal < 255))
                    ret[n++] = octal;
            }   break;
            case 'x': {
                int hex = 0;
                if (((i+1) < len) && is_hex(orig[i+1])) {
                    i++;
                    hex = hex2int(orig[i]);
                    if (((i+1) < len) && is_hex(orig[i+1])) {
                        i++;
                        hex <<= 4;
                        hex = hex2int(orig[i]);
                    }
                }
                if ((hex > 0) && (hex < 255))
                    ret[n++] = hex;
            }   break;
            default:
                ret[n++] = '\\';
                ret[n++] = orig[i];
                break;
            }
            i++;
        } else {
            ret[n++] = orig[i++];
        }
    }
    ret[n] = '\0';

    return ret;
}

static char *unsquote (const char *orig)
{
    char *ret = strdup (orig);
    if (ret == NULL)
        return NULL;

    size_t len = strlen (ret);

    if ((len < 2) || (ret[0] != '\'') || (ret[len - 1] != '\''))
        return ret;

    len -= 2;
    memmove (ret, ret + 1, len);
    ret[len] = 0;

    for (size_t i = 0; i < len; i++) {
        if ((ret[i] == '\\') && (ret[i+1] == '\'')) {
            memmove (ret + i, ret + (i + 1), len - i);
            len--;
        }
    }

    return ret;
}

static char *unrquote (const char *orig)
{
    char *ret = strdup (orig);
    if (ret == NULL)
        return NULL;

    size_t len = strlen (ret);

    if ((len < 2) || (ret[0] != '/') || (ret[len - 1] != '/'))
        return ret;

    len -= 2;
    memmove (ret, ret + 1, len);
    ret[len] = 0;

    for (size_t i = 0; i < len; i++) {
        if ((ret[i] == '\\') && (ret[i+1] == '/')) {
            memmove (ret + i, ret + (i + 1), len - i);
            len--;
        }
    }

    return ret;
}

static char *unhquote (const char *orig)
{
    char *ret = strdup (orig);
    if (ret == NULL)
        return NULL;

    size_t len = strlen (ret);

    if ((len < 6) ||
        (ret[0] != '\'') || (ret[1] != '\'') || (ret[2] != '\'') ||
        (ret[len - 3] != '\'') || (ret[len - 2] != '\'') || (ret[len - 1] != '\''))
        return ret;

    len -= 6;
    memmove (ret, ret + 3, len);
    ret[len] = 0;

    return ret;
}

static config_file_t *get_file_ref(void)
{
    if (c_file == NULL)
        return NULL;
    c_file->refcnt++;
    return c_file;
}
