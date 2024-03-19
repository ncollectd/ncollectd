/* SPDX-License-Identifier: GPL-2.0-only or PostgreSQL                                    */
/* SPDX-FileCopyrightText: Copyright (c) 2019-2023, PostgreSQL Global Development Group   */
/* from: postgreql src/backend/utils/adt/jsonpath_scan.l                                  */
%{
#include <stdlib.h>
#include <string.h>
#include "plugins/match_jsonpath/jsonpath_list.h"

#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"

#include "plugins/match_jsonpath/jsonpath_internal.h"
#include "plugins/match_jsonpath/jsonpath_gram.h"

#define YYSTYPE JSONPATH_YYSTYPE
#define YYLTYPE JSONPATH_YYLTYPE

static void add_string(JSONPATH_YYLTYPE *yylloc, void *scanner, bool init, char *s, int l);
static void add_char(JSONPATH_YYLTYPE *yylloc, void *scanner, bool init, char c);
static enum jsonpath_yytokentype check_keyword(void *scanner);
static bool parse_unicode(JSONPATH_YYLTYPE *loc, void *scanner, char *s, int l);
static bool parse_hexchar(JSONPATH_YYLTYPE *yylloc, void *scanner, char *s);

void jsonpath_yyerror(JSONPATH_YYLTYPE *yylloc, void *scanner, jsonpath_parse_result_t *result,const char *message);

/* Avoid exit() on fatal scanner errors (a bit ugly -- see yy_fatal_error) */
#if 0
#undef fprintf
#define fprintf(file, fmt, msg)  fprintf_to_ereport(fmt, msg)

static void fprintf_to_ereport(__attribute__((unused)) const char *fmt, const char *msg)
{
    fprintf(stderr, "%s", msg);
    ereport(ERROR, (errmsg_internal("%s", msg)));
}
#endif

/* LCOV_EXCL_START */

%}

/* %option 8bit */
/* %option nodefault */
/* %option warn */

%option bison-bridge
%option bison-locations
%option noyywrap
%option noinput
%option nounput
%option nostdinit
%option never-interactive
%option reentrant
%option extra-type="jsonpath_string_t *"
%option prefix="jsonpath_yy"

/*
 * We use exclusive states for quoted and non-quoted strings,
 * quoted variable names and C-style comments.
 * Exclusive states:
 *  <xq> - quoted strings
 *  <xnq> - non-quoted strings
 */

%x xq
%x xsq
%x xrq
%x xnq
%x xc

special         [\?\%\$\.\[\]\{\}\(\)\|\&\!\=\<\>\@\#\,\*:\-\+\/]
blank           [ \t\n\r\f]
/* "other" means anything that's not special, blank, or '\' or '"' */
other           [^\?\%\$\.\[\]\{\}\(\)\|\&\!\=\<\>\@\#\,\*:\-\+\/\\\"\' \t\n\r\f]

decdigit        [0-9]
hexdigit        [0-9A-Fa-f]
octdigit        [0-7]
bindigit        [0-1]

/* DecimalInteger in ECMAScript; must not start with 0 unless it's exactly 0 FIXME ¿?*/
decinteger      (0|[1-9](_?{decdigit})*)
/* DecimalDigits in ECMAScript; only used as part of other rules */
decdigits       {decdigit}(_?{decdigit})*
/* Non-decimal integers; in ECMAScript, these must not have underscore after prefix */
hexinteger      0[xX]{hexdigit}(_?{hexdigit})*
octinteger      0[oO]{octdigit}(_?{octdigit})*
bininteger      0[bB]{bindigit}(_?{bindigit})*

/* decimal         ({decinteger}\.{decdigits}?|\.{decdigits}) */
decimal         (({decinteger}(\.{decdigits})?)|\.{decdigits})
real            ({decinteger}|{decimal})[Ee][-+]?{decdigits}
realfail        ({decinteger}|{decimal})[Ee][-+]

decinteger_junk {decinteger}{other}
decimal_junk    {decimal}{other}
real_junk       {real}{other}

unicode         \\u{hexdigit}{4}
unicodefail     \\u{hexdigit}{0,3}
hex_char        \\x{hexdigit}{2}
hex_fail        \\x{hexdigit}{0,1}

%%

<xnq>{other}+           {
                            add_string(yylloc, yyscanner, false, yytext, yyleng);
                        }
<xnq>{blank}+           {
                            yylval->str = *yyextra;
                            BEGIN INITIAL;
                            return check_keyword(yyscanner);
                        }
<xnq>({special}|\")     {
                            yylval->str = *yyextra;
                            yyless(0);
                            BEGIN INITIAL;
                            return check_keyword(yyscanner);
                        }
<xnq><<EOF>>            {
                            yylval->str = *yyextra;
                            BEGIN INITIAL;
                            return check_keyword(yyscanner);
                        }
<xnq,xq,xrq,xsq>\\b     {   add_char(yylloc, yyscanner, false, '\b'); }
<xnq,xq,xrq,xsq>\\f     {   add_char(yylloc, yyscanner, false, '\f'); }
<xnq,xq,xrq,xsq>\\n     {   add_char(yylloc, yyscanner, false, '\n'); }
<xnq,xq,xrq,xsq>\\r     {   add_char(yylloc, yyscanner, false, '\r'); }
<xnq,xq,xrq,xsq>\\t     {   add_char(yylloc, yyscanner, false, '\t'); }
<xnq,xq,xrq,xsq>\\v     {   add_char(yylloc, yyscanner, false, '\v'); }
<xnq,xq,xrq,xsq>{unicode}+  {
                            if (!parse_unicode(yylloc, yyscanner, yytext, yyleng))
                                yyterminate();
                        }
<xnq,xq,xrq,xsq>{hex_char}  {
                            if (!parse_hexchar(yylloc, yyscanner, yytext))
                                yyterminate();
                        }
<xnq,xq,xrq,xsq>{unicode}*{unicodefail} {
                            jsonpath_yyerror(yylloc, yyscanner, NULL, "invalid Unicode escape sequence");
                            yyterminate();
                        }
<xnq,xq,xrq,xsq>{hex_fail}  {
                           jsonpath_yyerror(yylloc, yyscanner, NULL, "invalid hexadecimal character sequence");
                            yyterminate();
                        }
<xnq,xq,xrq,xsq>{unicode}+\\    {
                                /* throw back the \\, and treat as unicode */
                            yyless(yyleng - 1);
                            if (!parse_unicode(yylloc, yyscanner, yytext, yyleng))
                                yyterminate();
                        }
<xnq,xq,xrq,xsq>\\.     {   add_char(yylloc, yyscanner, false, yytext[1]); }
<xnq,xq,xrq,xsq>\\      {
                           jsonpath_yyerror(yylloc, yyscanner, NULL, "unexpected end after backslash");
                            yyterminate();
                        }
<xq,xsq><<EOF>>         {
                           jsonpath_yyerror(yylloc, yyscanner, NULL, "unterminated quoted string");
                            yyterminate();
                        }
<xq>\"                  {
                            yylval->str = *yyextra;
                            BEGIN INITIAL;
                            return QSTRING_P;
                        }
<xq>[^\\\"]+            {   add_string(yylloc, yyscanner, false, yytext, yyleng); }

<xsq>[^\\\']+           {   add_string(yylloc, yyscanner, false, yytext, yyleng); }
<xsq>\'                 {
                            yylval->str = *yyextra;
                            BEGIN INITIAL;
                            return QSTRING_P;
                        }
<xrq>[^\\/]+            {   add_string(yylloc, yyscanner, false, yytext, yyleng); }
<xrq>\/                 {
                            yylval->str = *yyextra;
                            BEGIN INITIAL;
                            return REGEX_P;
                        }
\&\&                    {   return AND_P; }
\|\|                    {   return OR_P; }
\!                      {   return NOT_P; }
\.\.                    {   return ANY_P; }
\<                      {   return LESS_P; }
\<\=                    {   return LESSEQUAL_P; }
\=\=                    {   return EQUAL_P; }
\!\=                    {   return NOTEQUAL_P; }
\>\=                    {   return GREATEREQUAL_P; }
\>                      {   return GREATER_P; }
\=\~                    {   return REGEXOP_P; }
{special}               {   return *yytext; }
{blank}+                {   /* ignore */ }
{decinteger}            {
                            add_string(yylloc, yyscanner, true, yytext, yyleng);
                            add_char(yylloc, yyscanner, false, '\0');
                            yylval->str = *yyextra;
                            return INT_P;
                        }
{real}                  {
                            add_string(yylloc, yyscanner, true, yytext, yyleng);
                            add_char(yylloc, yyscanner, false, '\0');
                            yylval->str = *yyextra;
                            return NUMERIC_P;
                        }
{decimal}               {
                            add_string(yylloc, yyscanner, true, yytext, yyleng);
                            add_char(yylloc, yyscanner, false, '\0');
                            yylval->str = *yyextra;
                            return NUMERIC_P;
                        }
{hexinteger}            {
                            add_string(yylloc, yyscanner, true, yytext, yyleng);
                            add_char(yylloc, yyscanner, false, '\0');
                            yylval->str = *yyextra;
                            return INT_P;
                        }
{octinteger}            {
                            add_string(yylloc, yyscanner, true, yytext, yyleng);
                            add_char(yylloc, yyscanner, false, '\0');
                            yylval->str = *yyextra;
                            return INT_P;
                        }
{bininteger}            {
                            add_string(yylloc, yyscanner, true, yytext, yyleng);
                            add_char(yylloc, yyscanner, false, '\0');
                            yylval->str = *yyextra;
                            return INT_P;
                        }
{realfail}              {
                           jsonpath_yyerror(yylloc, yyscanner, NULL, "invalid numeric literal");
                            yyterminate();
                        }
{decinteger_junk}       {
                           jsonpath_yyerror(yylloc, yyscanner, NULL, "trailing junk after numeric literal");
                            yyterminate();
                        }
{decimal_junk}          {
                           jsonpath_yyerror(yylloc, yyscanner, NULL, "trailing junk after numeric literal");
                            yyterminate();
                        }
{real_junk}             {
                           jsonpath_yyerror(yylloc, yyscanner, NULL, "trailing junk after numeric literal");
                            yyterminate();
                        }
\"                      {
                            add_char(yylloc, yyscanner, true, '\0');
                            BEGIN xq;
                        }
\'                      {
                            add_char(yylloc, yyscanner, true, '\0');
                            BEGIN xsq;
                        }
\/                      {
                            add_char(yylloc, yyscanner, true, '\0');
                            BEGIN xrq;
                        }
\\                      {
                            yyless(0);
                            add_char(yylloc, yyscanner, true, '\0');
                            BEGIN xnq;
                        }
{other}+                {
                            add_string(yylloc, yyscanner, true, yytext, yyleng);
                            BEGIN xnq;
                        }
<<EOF>>                 {   yyterminate(); }

%%

/* LCOV_EXCL_STOP */

void jsonpath_yyerror(__attribute__((unused)) JSONPATH_YYLTYPE *loc,
                      __attribute__((unused)) void *scanner,
                      jsonpath_parse_result_t *result,
                      const char *message)
{

    if (result != NULL) {
        result->error = true;
        snprintf(result->error_msg, sizeof(result->error_msg), "%s", message);
    } else {
        fprintf(stderr, "jsonpath_yyerror : %s\n", message);
    }
}

typedef struct {
    int16_t len;
    bool lowercase;
    int  val;
    const char *keyword;
} jsonpath_keyword_t;

/*
 * Array of key words should be sorted by length and then
 * alphabetical order
 */
static const jsonpath_keyword_t keywords[] = {
    {  3, false, ABS_P,         "abs"        },
    {  3, false, AVG_P,         "avg"        },
    {  3, false, MAX_P,         "max"        },
    {  3, false, MIN_P,         "min"        },
    {  4, true,  NULL_P,        "null"       },
    {  4, true,  TRUE_P,        "true"       },
    {  5, false, COUNT_P,       "count"      },
    {  5, true,  FALSE_P,       "false"      },
    {  5, false, FLOOR_P,       "floor"      },
    {  6, false, DOUBLE_P,      "double"     },
    {  6, false, LENGTH_P,      "length"     },
    {  7, false, CEILING_P,     "ceiling"    },
};
#define STATIC_ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
static size_t keywords_len = STATIC_ARRAY_SIZE(keywords);

/* Check if current scanstring value is a keyword */
static enum jsonpath_yytokentype check_keyword(void *scanner)
{
    int res = IDENT_P;
    int diff;
    const jsonpath_keyword_t *stop_low = keywords,
                             *stop_high = keywords + keywords_len,
                             *stop_middle;

    jsonpath_string_t *scanstring = yyget_extra(scanner);

    if (scanstring->len > keywords[keywords_len - 1].len)
        return res;

    while (stop_low < stop_high) {
        stop_middle = stop_low + ((stop_high - stop_low) >> 1);

        if (stop_middle->len == scanstring->len)
            diff = strncasecmp(stop_middle->keyword, scanstring->val, scanstring->len);
        else
            diff = stop_middle->len - scanstring->len;

        if (diff < 0) {
            stop_low = stop_middle + 1;
        } else if (diff > 0) {
            stop_high = stop_middle;
        } else {
            if (stop_middle->lowercase)
                diff = strncmp(stop_middle->keyword, scanstring->val, scanstring->len);
            if (diff == 0)
                res = stop_middle->val;
            break;
        }
    }

    return res;
}

/*
 * Resize scanstring so that it can append string of given length.
 * Reinitialize if required.
 */
static void resize_string(void *scanner, bool init, int append_len)
{
    jsonpath_string_t *scanstring = yyget_extra(scanner);
    if (init) {
        if (scanstring->val == NULL) {
            scanstring->total = append_len < 32 ? 32 : append_len;
            scanstring->val = (char *) calloc(1, scanstring->total);
            // FIXME NULL
            scanstring->val[0] = '\0';
            scanstring->len = 0;
        } else {
            scanstring->val[0] = '\0';
            scanstring->len = 0;
        }
    } else {
        if (scanstring->len + append_len >= scanstring->total) {
            while ((scanstring->len + append_len + 1) >= scanstring->total)
                scanstring->total *= 2;
            scanstring->val = realloc(scanstring->val, scanstring->total);
            // FIXME NULL
        }
    }
}

/* Add set of bytes at "s" of length "l" to scanstring */
static void add_string(__attribute__((unused)) JSONPATH_YYLTYPE *loc, void *scanner, bool init, char *s, int l)
{
    jsonpath_string_t *scanstring = yyget_extra(scanner);
    resize_string(scanner, init, l + 1);
    memcpy(scanstring->val + scanstring->len, s, l);
    scanstring->len += l;
    scanstring->val[scanstring->len] = '\0';
}

/* Add single byte "c" to scanstring */
static void add_char(__attribute__((unused)) JSONPATH_YYLTYPE *loc, void *scanner, bool init, char c)
{
    jsonpath_string_t *scanstring = yyget_extra(scanner);
    resize_string(scanner, init, 1);
    scanstring->val[scanstring->len] = c;
    if (c != '\0') {
        scanstring->len++;
        scanstring->val[scanstring->len] = '\0';
    }
}

/* Turn hex character into integer */
static bool hexval(JSONPATH_YYLTYPE *loc, void *scanner, char c, int *result)
{
    if (c >= '0' && c <= '9') {
        *result = c - '0';
        return true;
    }
    if (c >= 'a' && c <= 'f') {
        *result = c - 'a' + 0xA;
        return true;
    }
    if (c >= 'A' && c <= 'F') {
        *result = c - 'A' + 0xA;
        return true;
    }

   jsonpath_yyerror(loc, scanner, NULL, "invalid hexadecimal digit");
    return false;
}

/* Parse sequence of hex-encoded characters */
static bool parse_hexchar(JSONPATH_YYLTYPE *loc, void *scanner, char *s)
{
    int s2, s3, ch;
    if (!hexval(loc, scanner, s[2], &s2))
        return false;
    if (!hexval(loc, scanner, s[3], &s3))
        return false;

    ch = (s2 << 4) | s3;

    add_char(loc, scanner, false, ch);
    return true;
}

static void hex2digit(unsigned int *val, const unsigned char *hex)
{
    for (unsigned int i=0; i<4; i++) {
        unsigned char c = hex[i];
        if (c >= 'A') c = (c & ~0x20) - 7;
        c -= '0';
        *val = (*val << 4) | c;
    }
}


static bool parse_unicode(JSONPATH_YYLTYPE *loc, void *scanner, char *s, int l)
{
    if (l != 6)
        return false;

    if ((s[0] != '\\') || (s[1] != 'u'))
        return false;

    unsigned int codepoint = 0;
    hex2digit(&codepoint, (unsigned char *)s + 2);

#if 0
     /* check if this is a surrogate */ // FIXME
    if ((codepoint & 0xFC00) == 0xD800) {
        if (str[end] == '\\' && str[end + 1] == 'u') {
            unsigned int surrogate = 0;
            hex2digit(&surrogate, str + end + 2);
            codepoint = (((codepoint & 0x3F) << 10) |
                        ((((codepoint >> 6) & 0xF) + 1) << 16) |
                        (surrogate & 0x3FF));
            end += 5;
        } else {
            unescaped = "?";
            break;
        }
    }
#endif

    if (codepoint < 0x80) {
        add_char(loc, scanner, false, (char) codepoint);
    } else if (codepoint < 0x0800) {
        add_char(loc, scanner, false, (char) ((codepoint >> 6) | 0xC0));
        add_char(loc, scanner, false, (char) ((codepoint & 0x3F) | 0x80));
    } else if (codepoint < 0x10000) {
        add_char(loc, scanner, false, (char) ((codepoint >> 12) | 0xE0));
        add_char(loc, scanner, false, (char) (((codepoint >> 6) & 0x3F) | 0x80));
        add_char(loc, scanner, false, (char) ((codepoint & 0x3F) | 0x80));
    } else if (codepoint < 0x200000) {
        add_char(loc, scanner, false, (char)((codepoint >> 18) | 0xF0));
        add_char(loc, scanner, false, (char)(((codepoint >> 12) & 0x3F) | 0x80));
        add_char(loc, scanner, false, (char)(((codepoint >> 6) & 0x3F) | 0x80));
        add_char(loc, scanner, false, (char)((codepoint & 0x3F) | 0x80));
    } else {
        add_char(loc, scanner, false, '?');
    }

    return true;
}
