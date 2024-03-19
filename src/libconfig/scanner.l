/* SPDX-License-Identifier: GPL-2.0-only OR MIT                  */
/* SPDX-FileCopyrightText: Copyright (C) 2007  Florian Forster   */
/* SPDX-FileCopyrightText: Copyright (C) 2008  Sebastian Harl    */
/* SPDX-FileContributor: Florian Forster <octo at collectd.org>  */
/* SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>       */

%{
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wnull-dereference"

#include <stdlib.h>
#include <string.h>
#include "libconfig/config.h"
#include "libconfig/aux_types.h"
#include "libconfig/parser.h"

/* multiline string buffer */
static char *ml_buffer;
static size_t ml_pos;
static size_t ml_len;

#define ml_free (ml_len - ml_pos)

static void ml_append (yyscan_t yyscanner, char *string);

#define YYSTYPE    CONFIG_YYSTYPE
#define YYLTYPE    CONFIG_YYLTYPE

#ifdef yyterminate
# undef yyterminate
#endif
#define yyterminate() \
    do { free (ml_buffer); ml_buffer = NULL; ml_pos = 0; ml_len = 0; \
        return YY_NULL; } while (0)
%}

%option bison-bridge
%option bison-locations
%option noyywrap
%option noinput
%option nounput
%option nostdinit
%option never-interactive
%option reentrant
%option yylineno

%option prefix="config_yy"

%x DQUOTED_ML
%x SQUOTED_ML
%x HEREDOC_ML

WHITE_SPACE [\ \t\b]
NON_WHITE_SPACE [^\ \t\b]
EOL (\r\n|\n)
REGEX ([^/]+|\\.)*
HEREDOC_STRING ([^']|'([^']|'[^']))*
DQUOTED_STRING ([^\\"]+|\\.)*
SQUOTED_STRING ([^']+|\\.)*
UNQUOTED_STRING [A-Za-z][0-9A-Za-z_-]*
HEX_NUMBER 0[xX][0-9a-fA-F]+
OCT_NUMBER 0[0-7]+
DEC_NUMBER [\+\-]?[0-9]+
FLOAT_NUMBER [\+\-]?[0-9]*\.[0-9]+([eE][\+\-][0-9]+)?
NUMBER ({FLOAT_NUMBER}|{HEX_NUMBER}|{OCT_NUMBER}|{DEC_NUMBER})
BOOL_TRUE (true|yes|on)
BOOL_FALSE (false|no|off)
COMMENT #.*
PORT (6(5(5(3[0-5]|[0-2][0-9])|[0-4][0-9][0-9])|[0-4][0-9][0-9][0-9])|[1-5][0-9][0-9][0-9][0-9]|[1-9][0-9]?[0-9]?[0-9]?)

IP_BYTE (2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9])
IPV4_ADDR {IP_BYTE}\.{IP_BYTE}\.{IP_BYTE}\.{IP_BYTE}(:{PORT})?

/* IPv6 address according to http://www.ietf.org/rfc/rfc2373.txt
 * This supports embedded IPv4 addresses as well but does not strictly check
 * for the right prefix (::0:<v4> or ::FFFF:<v4>) because there are too many
 * ways to correctly represent the zero bytes. It's up to the user to check
 * for valid addresses. */
HEX16 ([0-9A-Fa-f]{1,4})
V6_PART ({HEX16}:{HEX16}|{IPV4_ADDR})
IPV6_BASE ({HEX16}:){6}{V6_PART}|::({HEX16}:){5}{V6_PART}|({HEX16})?::({HEX16}:){4}{V6_PART}|(({HEX16}:){0,1}{HEX16})?::({HEX16}:){3}{V6_PART}|(({HEX16}:){0,2}{HEX16})?::({HEX16}:){2}{V6_PART}|(({HEX16}:){0,3}{HEX16})?::{HEX16}:{V6_PART}|(({HEX16}:){0,4}{HEX16})?::{V6_PART}|(({HEX16}:){0,5}{HEX16})?::{HEX16}|(({HEX16}:){0,6}{HEX16})?::
IPV6_ADDR ({IPV6_BASE})|(\[{IPV6_BASE}\](:{PORT})?)

%%
{WHITE_SPACE}       |
{COMMENT}           {/* ignore */}

\\{EOL}             {/* continue line */}

{EOL}               {return (EOL);}
"{"                 {return (OPENBRAC);}
"}"                 {return (CLOSEBRAC);}
{BOOL_TRUE}         {yylval->boolean = 1; return (BTRUE);}
{BOOL_FALSE}        {yylval->boolean = 0; return (BFALSE);}

{IPV4_ADDR}         {yylval->string = yytext; return (UNQUOTED_STRING);}
{IPV6_ADDR}         {yylval->string = yytext; return (UNQUOTED_STRING);}

{NUMBER}            {yylval->number = strtod (yytext, NULL); return (NUMBER);}

"/"{REGEX}"/"       {yylval->string = yytext; return (REGEX);}
{UNQUOTED_STRING}   {yylval->string = yytext; return (UNQUOTED_STRING);}

\"{DQUOTED_STRING}\" {yylval->string = yytext; return (DQUOTED_STRING);}

\"{DQUOTED_STRING}\\{EOL} {
    size_t len = strlen (yytext);

    ml_pos = 0;

    /* remove "\\<EOL>" */
    if (yytext[len - 2] == '\r')
        len -= 3;
    else
        len -= 2;
    yytext[len] = '\0';

    ml_append (yyscanner, yytext);
    BEGIN (DQUOTED_ML);
}
<DQUOTED_ML>^{WHITE_SPACE}+ {/* remove leading white-space */}
<DQUOTED_ML>{NON_WHITE_SPACE}{DQUOTED_STRING}\\{EOL} {
    size_t len = strlen (yytext);

    /* remove "\\<EOL>" */
    if (yytext[len - 2] == '\r')
        len -= 3;
    else
        len -= 2;
    yytext[len] = '\0';

    ml_append(yyscanner, yytext);
}
<DQUOTED_ML>{NON_WHITE_SPACE}{DQUOTED_STRING}\" {
    ml_append(yyscanner, yytext);
    yylval->string = ml_buffer;

    BEGIN (INITIAL);
    return (DQUOTED_STRING);
}


"'"{SQUOTED_STRING}"'" {yylval->string = yytext; return (SQUOTED_STRING);}

"'"{SQUOTED_STRING}\\{EOL} {
    size_t len = strlen (yytext);

    ml_pos = 0;

    /* remove "\\<EOL>" */
    if (yytext[len - 2] == '\r')
        len -= 3;
    else
        len -= 2;
    yytext[len] = '\0';

    ml_append (yyscanner, yytext);
    BEGIN (SQUOTED_ML);
}

<SQUOTED_ML>^{WHITE_SPACE}+ {/* remove leading white-space */}

<SQUOTED_ML>{NON_WHITE_SPACE}{SQUOTED_STRING}\\{EOL} {
    size_t len = strlen (yytext);

    /* remove "\\<EOL>" */
    if (yytext[len - 2] == '\r')
        len -= 3;
    else
        len -= 2;
    yytext[len] = '\0';

    ml_append(yyscanner, yytext);
}

<SQUOTED_ML>{NON_WHITE_SPACE}{SQUOTED_STRING}"'" {
    ml_append(yyscanner, yytext);
    yylval->string = ml_buffer;

    BEGIN (INITIAL);
    return (SQUOTED_STRING);
}


"'''"{HEREDOC_STRING}"'''" {yylval->string = yytext; return (HEREDOC_STRING);}

"'''"{HEREDOC_STRING}{EOL} {
    ml_pos = 0;
    ml_append (yyscanner, yytext);
    BEGIN (HEREDOC_ML);
}

<HEREDOC_ML>{HEREDOC_STRING}{EOL} {
    ml_append(yyscanner, yytext);
}

<HEREDOC_ML>{HEREDOC_STRING}"'''" {
    ml_append(yyscanner, yytext);
    yylval->string = ml_buffer;

    BEGIN (INITIAL);
    return (HEREDOC_STRING);
}

%%
static void ml_append (yyscan_t yyscanner, char *string)
{
    size_t len = strlen (string);

    if (ml_free <= len) {
        ml_len += len - ml_free + 1;
        ml_buffer = realloc (ml_buffer, ml_len);
        if (ml_buffer == NULL)
            YY_FATAL_ERROR ("out of dynamic memory in ml_append");
    }

    int s = snprintf(ml_buffer + ml_pos, ml_free, "%s", string);
    if (s < 0 || (size_t)s >= ml_free)
        YY_FATAL_ERROR ("failed to write to multiline buffer");

    ml_pos += s;
    return;
}
