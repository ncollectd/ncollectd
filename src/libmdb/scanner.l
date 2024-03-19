%{
// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <stdio.h>
#include <math.h>

#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"

#include "libmdb/mql.h"
#include "libmdb/node.h"
#include "libmdb/util.h"
#include "libmdb/parser.h"

#define YYSTYPE         MQL_YYSTYPE
#define YYLTYPE         MQL_YYLTYPE
%}

%option bison-bridge
%option bison-locations
%option noyywrap
%option noinput
%option nounput
%option nostdinit
%option never-interactive
%option reentrant

%option prefix="mql_yy"

FLOAT_NUMBER [0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?
HEX_NUMBER   0[xX][0-9a-fA-F]+
INF          [nN][aA][nN]
NAN          [iI][nN][fF]
NUMBER       [\-\+]?({FLOAT_NUMBER}|{HEX_NUMBER}|{INF}|{NAN})
IDENTIFIER   [a-zA-Z_][a-zA-Z0-9_:]*
/* ??? : */
DURATION     ([0-9]+(\.[0-9]*)?(ms|s|m|h|d|w|y))+
/* DURATION [\-\+]?([0-9]*(\.[0-9]*)?(ms|s|m|h|d|w|y))+ */
%%

sum           return SUM;
avg           return AVG;
count         return COUNT;
min           return MIN;
max           return MAX;
group         return GROUP;
stddev        return STDDEV;
stdvar        return STDVAR;
topk          return TOPK;
bottomk       return BOTTOMK;
count_values  return COUNT_VALUES;
quantile      return QUANTILE;

offset        return OFFSET;
by            return BY;
without       return WITHOUT;
on            return ON;
ignoring      return IGNORING;
group_left    return GROUP_LEFT;
group_right   return GROUP_RIGHT;
bool          return BOOL;

and           return AND;
or            return OR;
unless        return UNLESS;

start         return START;
end           return END;

-             return SUB;
\+            return ADD;
\*            return MUL;
%             return MOD;
\/            return DIV;
==            return EQLC;
!=            return NEQ;
\<=           return LTE;
\<            return LSS;
\>=           return GTE;
\>            return GTR;
=~            return EQL_REGEX;
!~            return NEQ_REGEX;
\^            return POW;

@             return AT;

\(            return LEFT_PAREN;
\)            return RIGHT_PAREN;
\{            return LEFT_BRACE;
\}            return RIGHT_BRACE;
\[            return LEFT_BRACKET;
\]            return RIGHT_BRACKET;
,             return COMMA;
=             return EQL;
:             return COLON;
;             return SEMICOLON;

[ \t\n]+      { /* skip spaces */ }

{IDENTIFIER}  { yylval->string = strdup(yytext); /* fprintf(stderr, "identifier: <%s>\n", yytext); */ return IDENTIFIER; }

{DURATION}    { yylval->duration = mql_parse_duration(yytext); /* fprintf(stderr, "duration: <%s>\n", yytext); */ return DURATION; }

{NUMBER}      { yylval->number = strtod (yytext, NULL); return NUMBER; }

\"([^\"\n]|\\\")*\"  { yylval->string = mql_unquote(yytext); return STRING; }
\'([^\'\n]|\\\')*\'  { yylval->string = mql_unquote(yytext); return STRING; }

