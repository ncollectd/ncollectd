// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define YYSTYPE MQL_YYSTYPE
#define YYLTYPE MQL_YYLTYPE

#include "libmdb/mql.h"
#include "libmdb/node.h"
#include "libmdb/parser.h"
#include "libmdb/scanner.h"

int mql_parser(char *query, mql_status_t *status)
{
    yyscan_t scanner;
    YY_BUFFER_STATE buffer;

//  mql_yydebug = 1;

    if (query == NULL)
        return -1;

    mql_yylex_init(&scanner);
    mql_yyset_debug(1, scanner);

    buffer = mql_yy_scan_string(query, scanner);

    int ret = mql_yyparse(scanner, status);
    if (ret > 0) {
        mql_yy_delete_buffer(buffer, scanner);
        return -1;
    }

    mql_yy_delete_buffer(buffer, scanner);
    return  0;
}

