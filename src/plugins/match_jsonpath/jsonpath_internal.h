/* SPDX-License-Identifier: GPL-2.0-only or PostgreSQL                                  */
/* SPDX-FileCopyrightText: Copyright (c) 1996-2023, PostgreSQL Global Development Group */
/* SPDX-FileCopyrightText: Copyright (c) 1994, Regents of the University of California  */

#pragma once

/* struct JsonPathString is shared between scan and gram */
typedef struct {
    char *val;
    int len;
    int total;
} jsonpath_string_t;

#include "jsonpath.h"

#if 0
#define YY_DECL extern int jsonpath_yylex(JSONPATH_YYSTYPE *yylval_param, \
                                          JSONPATH_YYLTYPE *yylloc_param, \
                                          yyscan_t yyscanner, \
                                          jsonpath_parse_result_t *result)

#include "plugins/match_jsonpath/jsonpath_gram.h"
#define YYSTYPE JSONPATH_YYSTYPE
#define YYLTYPE JSONPATH_YYLTYPE
#include "plugins/match_jsonpath/jsonpath_scan.h"

YY_DECL;
#endif

extern int jsonpath_yyparse(void *scanner, jsonpath_parse_result_t *result);
// extern void jsonpath_yyerror(JSONPATH_YYLTYPE *yylloc, void *scanner, jsonpath_parse_result_t **result,const char *message);
