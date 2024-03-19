/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

%{
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "libexpr/expr.h"

#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"

#include "libexpr/parser.h"

#define YYSTYPE EXPR_YYSTYPE
#define YYLTYPE EXPR_YYLTYPE

void expr_yyerror(EXPR_YYLTYPE *loc, void *scanner, void *result, const char *message);

static char *expr_unquote(char *str);
static double expr_strtod(char *str, size_t suffix);
static double expr_duration (char *str);

#if 0
integer (0|[1-9][0-9]*)
decimal (((0|[1-9][0-9]*)(\.[0-9]+)?)|\.[0-9]+)
real    ({decinteger}|{decimal})[Ee][-+]?{decdigits}
        ((0|[1-9][0-9]*)|(((0|[1-9][0-9]*)(\.[0-9]+)?)|\.[0-9]+))([Ee][+\-]?[0-9]+)?

number ([0-9]+|[0-9]+\.[0-9]*|[0-9]*\.[0-9]+)([Ee][+\-]?[0-9]+)?
#endif

%}

%option bison-bridge
%option bison-locations
%option noyywrap
%option noinput
%option nounput
%option nostdinit
%option never-interactive
%option reentrant
%option extra-type="expr_node_t *"
%option prefix="expr_yy"

number     ((0|[1-9][0-9]*)|(0|[1-9][0-9]*)\.[0-9]*|\.[0-9]+)([Ee][+\-]?[0-9]+)?
identifier [_a-zA-Z][_a-zA-Z0-9]*
duration   ([0-9]+(\.[0-9]*)?(ms|s|m|h|d|w|y))+
%%

"true"              { return T_TRUE;  }
"false"             { return T_FALSE; }
"NAN"               { yylval->number = NAN;        return T_NUMBER; }
"e"                 { yylval->number = M_E;        return T_NUMBER; }
"pi"                { yylval->number = M_PI;       return T_NUMBER; }
"pi_2"              { yylval->number = M_PI_2;     return T_NUMBER; }
"pi_4"              { yylval->number = M_PI_4;     return T_NUMBER; }
"1_pi"              { yylval->number = M_1_PI;     return T_NUMBER; }
"2_pi"              { yylval->number = M_2_PI;     return T_NUMBER; }
"log2e"             { yylval->number = M_LOG2E;    return T_NUMBER; }
"log10e"            { yylval->number = M_LOG10E;   return T_NUMBER; }
"ln2"               { yylval->number = M_LN2;      return T_NUMBER; }
"ln10"              { yylval->number = M_LN10;     return T_NUMBER; }
"2_sqrtpi"          { yylval->number = M_2_SQRTPI; return T_NUMBER; }
"sqrt2"             { yylval->number = M_SQRT2;    return T_NUMBER; }
"sqrt1_2"           { yylval->number = M_SQRT1_2;  return T_NUMBER; }
"time"              { return T_FUNC_TIME;      }
"random"            { return T_FUNC_RANDOM;   }
"exp"               { return T_FUNC_EXP;      }
"expm1"             { return T_FUNC_EXPM1;    }
"log"               { return T_FUNC_LOG;      }
"log2"              { return T_FUNC_LOG2;     }
"log10"             { return T_FUNC_LOG10;    }
"log1p"             { return T_FUNC_LOG1P;    }
"sqrt"              { return T_FUNC_SQRT;     }
"cbrt"              { return T_FUNC_CBRT;     }
"sin"               { return T_FUNC_SIN;      }
"cos"               { return T_FUNC_COS;      }
"tan"               { return T_FUNC_TAN;      }
"asin"              { return T_FUNC_ASIN;     }
"acos"              { return T_FUNC_ACOS;     }
"atan"              { return T_FUNC_ATAN;     }
"cosh"              { return T_FUNC_COSH;     }
"sinh"              { return T_FUNC_SINH;     }
"tanh"              { return T_FUNC_TANH;     }
"acosh"             { return T_FUNC_ACOSH;    }
"asinh"             { return T_FUNC_ASINH;    }
"atanh"             { return T_FUNC_ATANH;    }
"abs"               { return T_FUNC_ABS;      }
"ceil"              { return T_FUNC_CEIL;     }
"floor"             { return T_FUNC_FLOOR;    }
"round"             { return T_FUNC_ROUND;    }
"trunc"             { return T_FUNC_TRUNC;    }
"isnan"             { return T_FUNC_ISNAN;    }
"isinf"             { return T_FUNC_ISINF;    }
"isnormal"          { return T_FUNC_ISNORMAL; }
"pow"               { return T_FUNC_POW;      }
"hypot"             { return T_FUNC_HYPOT;    }
"atan2"             { return T_FUNC_ATAN2;    }
"max"               { return T_FUNC_MAX;      }
"min"               { return T_FUNC_MIN;      }
"sum"               { return T_FUNC_SUM;      }
"avg"               { return T_FUNC_AVG;      }
"all"               { return T_FUNC_ALL;      }
"any"               { return T_FUNC_ANY;      }

\|\|                { return T_OR;          }
\&\&                { return T_AND;         }
\=\=                { return T_EQL;         }
\!\=                { return T_NQL;         }
\=\~                { return T_MATCH;       }
\!\=                { return T_NMATCH;      }
\>=                 { return T_GTE;         }
\>                  { return T_GT;          }
\<=                 { return T_LTE;         }
\<                  { return T_LT;          }
\!                  { return T_NOT;         }
\+                  { return T_PLUS;        }
\-                  { return T_MINUS;       }
\*                  { return T_MUL;         }
\/                  { return T_DIV;         }
\%                  { return T_MOD;         }

\&                  { return T_BIT_AND;     }
\|                  { return T_BIT_OR;      }
\^                  { return T_BIT_XOR;     }
\<\<                { return T_BIT_LSHIFT;  }
\>\>                { return T_BIT_RSHIFT;  }
\~                  { return T_BIT_NOT;     }

\?                  { return T_QMARK;       }
\:                  { return T_COLON;       }

\[                  { return T_LBRACK;      }
\]                  { return T_RBRACK;      }
\(                  { return T_LPAREN;      }
\)                  { return T_RPAREN;      }
\,                  { return T_COMMA;       }
\.                  { return T_DOT;         }

[ \t\n\r\f]+        { /* ignore */          }

0[xX][a-fA-F0-9]+   { yylval->number = strtoull(yytext, NULL, 16); return T_NUMBER; }
0[0-7]*             { yylval->number = strtoull(yytext, NULL, 8);  return T_NUMBER; }
0[bB][0-1]+         { yylval->number = strtoull(yytext+2, NULL, 2);  return T_NUMBER; }

{number}"Ki"        { yylval->number = expr_strtod(yytext, 2) * 1024;  return T_NUMBER; }
{number}"Mi"        { yylval->number = expr_strtod(yytext, 2) * 1048576;  return T_NUMBER; }
{number}"Gi"        { yylval->number = expr_strtod(yytext, 2) * 1073741824;  return T_NUMBER; }
{number}"Ti"        { yylval->number = expr_strtod(yytext, 2) * 1099511627776;  return T_NUMBER; }
{number}"k"         { yylval->number = expr_strtod(yytext, 1) * 1000;  return T_NUMBER; }
{number}"M"         { yylval->number = expr_strtod(yytext, 1) * 1000000;  return T_NUMBER; }
{number}"G"         { yylval->number = expr_strtod(yytext, 1) * 1000000000;  return T_NUMBER; }
{number}"T"         { yylval->number = expr_strtod(yytext, 1) * 1000000000000; return T_NUMBER; }

{number}            { yylval->number = strtod(yytext, NULL); return T_NUMBER;     }
{duration}          { yylval->number = expr_duration(yytext); return T_NUMBER;     }

{identifier}        { yylval->string = strdup(yytext);       return T_IDENTIFIER; }
\"([^\"\n]|\\\")*\" { yylval->string = expr_unquote(yytext); return T_STRING;     }
\'([^\'\n]|\\\')*\' { yylval->string = expr_unquote(yytext); return T_STRING;     }

%%
static double expr_strtod(char *str, size_t suffix)
{
    size_t len = strlen(str);
    if (len <= suffix)
        return 0.0;
    char c = str[len-suffix];
    str[len-suffix] = '\0';
    double number = strtod(str, NULL);
    str[len-suffix] = c;
    return number;
}

static char *expr_unquote(char *str)
{
    size_t len = strlen(str);
    if (len < 2) {
        return NULL;
    }

    char quote = *str;
    if (quote == '\'') {
        if (str[len-1] == quote) {
            str[len-1] = '\0';
            str++;
            return strdup(str);
        } else {
            return NULL;
        }
    }

    if (quote != '"')
        return NULL;
    if (str[len-1] != quote)
        return NULL;

    str[len-1] = '\0';
    str++;

    char *dstr = malloc(sizeof(char)*(len-1));
    if (dstr == NULL) {
        return NULL;
    }

    char *c = str;
    char *d = dstr;

    while (*c != '\0') {
        if (*c == '\\') {
            c++;
            if (*c == '\0')
                break;
            switch (*c) {
                case 'a':
                        *d = '\a';
                        break;
                case 'b':
                        *d = '\b';
                        break;
                case 'f':
                        *d = '\f';
                        break;
                case 'n':
                        *d = '\n';
                        break;
                case 'r':
                        *d = '\r';
                        break;
                case 't':
                        *d = '\t';
                        break;
                case 'v':
                        *d = '\v';
                        break;
                case '\\':
                        *d = '\\';
                        break;
                case '"':
                        *d = '"';
                        break;
                default:
                        *d = *c;
                        break;
            }
        } else {
            *d = *c;
        }
        c++;
        d++;
    }
    *d = '\0';

    return dstr;
}

static double expr_duration (char *str)
{
    char *ptr;
    bool negative = false;
    int64_t lscale = -1;
    int64_t duration = 0;

    ptr = str;
    if ((*ptr == '-') || (*ptr == '+')) {
        negative = *ptr == '-';
        ptr++;
    }

    if ((*ptr == '0') && (ptr[1] == '\0')) {
        return 0;
    }

    while (ptr[0] != '\0') {

        int64_t whole = 0;;
        int64_t nwhole = 0;
        while ((ptr[0] >= '0') && (ptr[0] <= '9')) {
            whole = whole * 10 + (ptr[0] - '0');
            nwhole++;
            ptr++;
        }

        int64_t decimal = 0;
        int64_t sdecimal = 1;
        if (ptr[0] == '.') {
            ptr++;
            while ((ptr[0] >= '0') && (ptr[0] <= '9')) {
                decimal = decimal * 10 + (ptr[0] - '0');
                sdecimal *= 10;
                ptr++;
            }
        }

        if ((nwhole == 0) && (sdecimal == 1)) {
            // *error = EINVAL;
        }

        int64_t scale = 0;
        if (ptr[0] == 'm') {
            if (ptr[1] == 's') {
                scale = 1;
                ptr+=2;
            } else {
                scale = 60 * 1000L;
                ptr+=1;
            }
        } else if (ptr[0] == 's') {
            scale = 1000L;
            ptr+=1;
        } else if (ptr[0] == 'h') {
            scale = 3600 * 1000L;
            ptr+=1;
        } else if (ptr[0] == 'd') {
            scale = 24 * 3600 * 1000L;
            ptr+=1;
        } else if (ptr[0] == 'w') {
            scale = 7 * 24 * 3600 * 1000L;
            ptr+=1;
        } else if (ptr[0] == 'y') {
            scale = 365 * 24 * 3600 * (int64_t)1000L;
            ptr+=1;
        } else {
            // *error = EINVAL;
        }

        if ((lscale > 0) && (scale >= lscale)) {
            // *error = ERANGE;
        }
        lscale = scale;

        whole *= scale;
        if (decimal > 0) {
            whole += (int64_t)((double)decimal * (double)((scale)/sdecimal));
            if (whole < 0) { // overflow
                // *error = ERANGE;
            }
        }

        duration += whole;
        if (duration < 0) {
            // *error = ERANGE;
        }
    }

    if (duration == 0)
        return 0.0;

    return negative ? - ((double)duration/1000.0) : ((double)duration/1000.0);
}
