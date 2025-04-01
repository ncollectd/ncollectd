// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <ctype.h>

#include "libutils/strbuf.h"
#include "libxson/tree.h"
#include "jsonpath.h"

#include "libtest/testing.h"

static int g_argc;
static char **g_argv;

#define JSON_DOC "{\"books\":[{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95,\"id\":1},{\"category\":\"fiction\",\"author\":\"Evelyn Waugh\",\"title\":\"Sword of Honour\",\"price\":12.99,\"id\":2},{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99,\"id\":3},{\"category\":\"fiction\",\"author\":\"J. R. R. Tolkien\",\"title\":\"The Lord of the Rings\",\"isbn\":\"0-395-19395-8\",\"price\":22.99,\"id\":4}],\"services\":{\"delivery\":{\"servicegroup\":1000,\"description\":\"Next day delivery in local town\",\"active\":true,\"price\":5},\"bookbinding\":{\"servicegroup\":1001,\"description\":\"Printing and assembling book in A5 format\",\"active\":true,\"price\":154.99},\"restoration\":{\"servicegroup\":1002,\"description\":\"Various restoration methods\",\"active\":false,\"methods\":[{\"description\":\"Chemical cleaning\",\"price\":46},{\"description\":\"Pressing pages damaged by moisture\",\"price\":24.5},{\"description\":\"Rebinding torn book\",\"price\":99.49}]}},\"filters\":{\"price\":10,\"category\":\"fiction\",\"no filters\":\"no \\\"filters\\\"\"},\"closed message\":\"Store is closed\",\"tags\":[\"a\",\"b\",\"c\",\"d\",\"e\"]}"

struct test {
    char *id;
    char *selector;
    char *alt_selector;
    char *document;
    char **result;
    jsonpath_exec_result_t rcode;
};

static struct test test[] = {
    {
        .id = "array_slice",
        .selector = "$[1:3]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"second\"", "\"third\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_on_exact_match",
        .selector = "$[0:5]",
        .alt_selector = "$[:5]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"first\"", "\"second\"", "\"third\"", "\"forth\"", "\"fifth\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_on_non_overlapping_array",
        .selector = "$[7:10]",
        .document = "[\"first\", \"second\", \"third\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "array_slice_on_object",
        .selector = "$[1:3]",
        .document = "{\":\": 42, \"more\": \"string\", \"a\": 1, \"b\": 2, \"c\": 3, \"1:3\": \"nice\"}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "array_slice_on_partially_overlapping_array",
        .selector = "$[1:10]",
        .document = "[\"first\", \"second\", \"third\"]",
        .result = (char *[]){"\"second\"", "\"third\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_large_number_for_end",
        .selector = "$[2:113667776004]",
        .alt_selector = "$[2:1998626308]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"third\"", "\"forth\"", "\"fifth\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_large_number_for_end_and_negative_step",
        .selector = "$[2:-113667776004:-1]",
        .alt_selector = "$[2:-1998626308:-1]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"third\"", "\"second\"", "\"first\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_large_number_for_start",
        .selector = "$[-113667776004:2]",
        .alt_selector = "$[-1998626308:2]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"first\"", "\"second\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_large_number_for_start_end_negative_step",
        .selector = "$[113667776004:2:-1]",
        .alt_selector = "$[1998626308:2:-1]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"fifth\"","\"forth\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_negative_start_and_end_and_range_of_-1",
        .selector = "$[-4:-5]",
        .document = "[2, \"a\", 4, 5, 100, \"nice\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "array_slice_with_negative_start_and_end_and_range_of_0",
        .selector = "$[-4:-4]",
        .document = "[2, \"a\", 4, 5, 100, \"nice\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "array_slice_with_negative_start_and_end_and_range_of_1",
        .selector = "$[-4:-3]",
        .document = "[2, \"a\", 4, 5, 100, \"nice\"]",
        .result = (char *[]){"4", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_negative_start_and_positive_end_and_range_of_-1",
        .selector = "$[-4:1]",
        .document = "[2, \"a\", 4, 5, 100, \"nice\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "array_slice_with_negative_start_and_positive_end_and_range_of_0",
        .selector = "$[-4:2]",
        .document = "[2, \"a\", 4, 5, 100, \"nice\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "array_slice_with_negative_start_and_positive_end_and_range_of_1",
        .selector = "$[-4:3]",
        .document = "[2, \"a\", 4, 5, 100, \"nice\"]",
        .result = (char *[]){"4", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_negative_step",
        .selector = "$[3:0:-2]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"forth\"", "\"second\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_negative_step_and_start_greater_than_end",
        .selector = "$[0:3:-2]",
        .alt_selector = "$[:3:-2]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "array_slice_with_negative_step_on_partially_overlapping_array",
        .selector = "$[7:3:-1]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"fifth\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_negative_step_only",
        .selector = "$[::-2]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"fifth\"", "\"third\"", "\"first\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_open_end",
        .selector = "$[1:]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"second\"", "\"third\"", "\"forth\"", "\"fifth\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_open_end_and_negative_step",
        .selector = "$[3::-1]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"forth\"", "\"third\"", "\"second\"", "\"first\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_open_start",
        .selector = "$[:2]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"first\"", "\"second\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_open_start_and_end",
        .selector = "$[:]",
        .document = "[\"first\", \"second\"]",
        .result = (char *[]){"\"first\"", "\"second\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_open_start_and_end_and_step_empty",
        .selector = "$[::]",
        .alt_selector = "$[:]",
        .document = "[\"first\", \"second\"]",
        .result = (char *[]){"\"first\"", "\"second\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_open_start_and_end_on_object",
        .selector = "$[:]",
        .document = "{\":\": 42, \"more\": \"string\"}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "array_slice_with_open_start_and_negative_step",
        .selector = "$[:2:-1]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"fifth\"", "\"forth\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_positive_start_and_negative_end_and_range_of_-1",
        .selector = "$[3:-4]",
        .document = "[2, \"a\", 4, 5, 100, \"nice\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "array_slice_with_positive_start_and_negative_end_and_range_of_0",
        .selector = "$[3:-3]",
        .document = "[2, \"a\", 4, 5, 100, \"nice\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "array_slice_with_positive_start_and_negative_end_and_range_of_1",
        .selector = "$[3:-2]",
        .document = "[2, \"a\", 4, 5, 100, \"nice\"]",
        .result = (char *[]){"5", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_range_of_-1",
        .selector = "$[2:1]",
        .document = "[\"first\", \"second\", \"third\", \"forth\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "array_slice_with_range_of_0",
        .selector = "$[0:0]",
        .alt_selector = "$[:0]",
        .document = "[\"first\", \"second\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "array_slice_with_range_of_1",
        .selector = "$[0:1]",
        .alt_selector = "$[:1]",
        .document = "[\"first\", \"second\"]",
        .result = (char *[]){"\"first\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_start_-1_and_open_end",
        .selector = "$[-1:]",
        .document = "[\"first\", \"second\", \"third\"]",
        .result = (char *[]){"\"third\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_start_-2_and_open_end",
        .selector = "$[-2:]",
        .document = "[\"first\", \"second\", \"third\"]",
        .result = (char *[]){"\"second\"", "\"third\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_start_large_negative_number_and_open_end_on_short_array",
        .selector = "$[-4:]",
        .document = "[\"first\", \"second\", \"third\"]",
        .result = (char *[]){"\"first\"", "\"second\"", "\"third\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_step",
        .selector = "$[0:3:2]",
        .alt_selector = "$[:3:2]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"first\"", "\"third\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_step_0",
        .selector = "$[0:3:0]",
        .alt_selector = "$[:3:0]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "array_slice_with_step_1",
        .selector = "$[0:3:1]",
        .alt_selector = "$[:3]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"first\"", "\"second\"", "\"third\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_step_and_leading_zeros",
        .selector = "$[010:024:010]",
        .document = "[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "array_slice_with_step_but_end_not_aligned",
        .selector = "$[0:4:2]",
        .alt_selector = "$[:4:2]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"first\"", "\"third\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_step_empty",
        .selector = "$[1:3:]",
        .alt_selector = "$[1:3]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"second\"", "\"third\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_step_only",
        .selector = "$[::2]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"first\"", "\"third\"", "\"fifth\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation",
        .selector = "$['key']",
        .document = "{\"key\": \"value\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_after_recursive_descent",
        .selector = "$..[0]",
        .document = "[\"first\", {\"key\": [\"first nested\", {\"more\": [{\"nested\": [\"deepest\", \"second\"]}, [\"more\", \"values\"]]}]}]",
        .result = (char *[]){"\"first\"", "\"first nested\"", "{\"nested\":[\"deepest\",\"second\"]}", "\"deepest\"", "\"more\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_on_object_without_key",
        .selector = "$['missing']",
        .document = "{\"key\": \"value\"}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "bracket_notation_with_NFC_path_on_NFD_key",
        .selector = "$['ü']",
        .document = "{\"u\u0308\": 42}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "bracket_notation_with_dot",
        .selector = "$['two.some']",
        .document = "{\"one\": {\"key\": \"value\"}, \"two\": {\"some\": \"more\", \"key\": \"other value\"}, \"two.some\": \"42\"}",
        .result = (char *[]){"\"42\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_double_quotes",
        .selector = "$[\"key\"]",
        .alt_selector = "$['key']",
        .document = "{\"key\": \"value\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_empty_path",
        .selector = "$[]",
        .document = "{\"\": 42, \"''\": 123, \"\\\"\\\"\": 222}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "bracket_notation_with_empty_string",
        .selector = "$['']",
        .document = "{\"\": 42, \"''\": 123, \"\\\"\\\"\": 222}",
        .result = (char *[]){"42", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_empty_string_doubled_quoted",
        .selector = "$[\"\"]",
        .alt_selector = "$['']",
        .document = "{\"\": 42, \"''\": 123, \"\\\"\\\"\": 222}",
        .result = (char *[]){"42", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_negative_number_on_short_array",
        .selector = "$[-2]",
        .document = "[\"one element\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "bracket_notation_with_number",
        .selector = "$[2]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"third\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_number_-1",
        .selector = "$[-1]",
        .document = "[\"first\", \"second\", \"third\"]",
        .result = (char *[]){"\"third\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_number_-1_on_empty_array",
        .selector = "$[-1]",
        .document = "[]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "bracket_notation_with_number_0",
        .selector = "$[0]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"first\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_number_after_dot_notation_with_wildcard_on_nested_arrays_with_different_length",
        .selector = "$.*[1]",
        .document = "[[1], [2, 3]]",
        .result = (char *[]){"3", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_number_on_object",
        .selector = "$[0]",
        .document = "{\"0\": \"value\"}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "bracket_notation_with_number_on_short_array",
        .selector = "$[1]",
        .document = "[\"one element\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "bracket_notation_with_number_on_string",
        .selector = "$[0]",
        .document = "\"Hello World\"",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "bracket_notation_with_quoted_array_slice_literal",
        .selector = "$[':']",
        .document = "{\":\": \"value\", \"another\": \"entry\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_quoted_closing_bracket_literal",
        .selector = "$[']']",
        .document = "{\"]\": 42}",
        .result = (char *[]){"42", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_quoted_current_object_literal",
        .selector = "$['@']",
        .document = "{\"@\": \"value\", \"another\": \"entry\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_quoted_dot_literal",
        .selector = "$['.']",
        .document = "{\".\": \"value\", \"another\": \"entry\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_quoted_dot_wildcard",
        .selector = "$['.*']",
        .document = "{\"key\": 42, \".*\": 1, \"\": 10}",
        .result = (char *[]){"1", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_quoted_double_quote_literal",
        .selector = "$['\"']",
        .document = "{\"\\\"\": \"value\", \"another\": \"entry\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_quoted_escaped_backslash",
        .selector = "$['\\\\']",
        .document = "{\"\\\\\": \"value\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_quoted_escaped_single_quote",
        .selector = "$['\\'']",
        .document = "{\"'\": \"value\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_quoted_number_on_object",
        .selector = "$['0']",
        .document = "{\"0\": \"value\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_quoted_root_literal",
        .selector = "$['$']",
        .document = "{\"$\": \"value\", \"another\": \"entry\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_quoted_special_characters_combined",
        .selector = "$[':@.\"$,*\\'\\\\']",
        .document = "{\":@.\\\"$,*'\\\\\": 42}",
        .result = (char *[]){"42", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_quoted_string_and_unescaped_single_quote",
        .selector = "$['single'quote']",
        .document = "{\"single'quote\": \"value\"}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "bracket_notation_with_quoted_union_literal",
        .selector = "$[',']",
        .document = "{\",\": \"value\", \"another\": \"entry\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_quoted_wildcard_literal",
        .selector = "$['*']",
        .document = "{\"*\": \"value\", \"another\": \"entry\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_quoted_wildcard_literal_on_object_without_key",
        .selector = "$['*']",
        .document = "{\"another\": \"entry\"}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "bracket_notation_with_spaces",
        .selector = "$[ 'a' ]",
        .alt_selector = "$['a']",
        .document = "{\" a\": 1, \"a\": 2, \" a \": 3, \"a \": 4, \" 'a' \": 5, \" 'a\": 6, \"a' \": 7, \" \\\"a\\\" \": 8, \"\\\"a\\\"\": 9}",
        .result = (char *[]){"2", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_string_including_dot_wildcard",
        .selector = "$['ni.*']",
        .document = "{\"nice\": 42, \"ni.*\": 1, \"mice\": 100}",
        .result = (char *[]){"1", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_two_literals_separated_by_dot",
        .selector = "$['two'.'some']",
        .document = "{\"one\": {\"key\": \"value\"}, \"two\": {\"some\": \"more\", \"key\": \"other value\"}, \"two.some\": \"42\", \"two'.'some\": \"43\"}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "bracket_notation_with_two_literals_separated_by_dot_without_quotes",
        .selector = "$[two.some]",
        .document = "{\"one\": {\"key\": \"value\"}, \"two\": {\"some\": \"more\", \"key\": \"other value\"}, \"two.some\": \"42\"}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "bracket_notation_with_wildcard_after_array_slice",
        .selector = "$[0:2][*]",
        .alt_selector = "$[:2][*]",
        .document = "[[1, 2], [\"a\", \"b\"], [0, 0]]",
        .result = (char *[]){"1", "2", "\"a\"", "\"b\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_wildcard_after_dot_notation_after_bracket_notation_with_wildcard",
        .selector = "$[*].bar[*]",
        .document = "[{\"bar\": [42]}]",
        .result = (char *[]){"42", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_wildcard_after_recursive_descent",
        .selector = "$..[*]",
        .document = "{\"key\": \"value\", \"another key\": {\"complex\": \"string\", \"primitives\": [0, 1]}}",
        .result = (char *[]){"\"value\"", "{\"complex\":\"string\",\"primitives\":[0,1]}", "\"string\"", "[0,1]", "0", "1", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_wildcard_on_array",
        .selector = "$[*]",
        .document = "[\"string\", 42, {\"key\": \"value\"}, [0, 1]]",
        .result = (char *[]){"\"string\"", "42", "{\"key\":\"value\"}", "[0,1]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_wildcard_on_empty_array",
        .selector = "$[*]",
        .document = "[]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "bracket_notation_with_wildcard_on_empty_object",
        .selector = "$[*]",
        .document = "{}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "bracket_notation_with_wildcard_on_null_value_array",
        .selector = "$[*]",
        .document = "[40, null, 42]",
        .result = (char *[]){"40", "null", "42", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_with_wildcard_on_object",
        .selector = "$[*]",
        .document = "{\"some\": \"string\", \"int\": 42, \"object\": {\"key\": \"value\"}, \"array\": [0, 1]}",
        .result = (char *[]){"\"string\"", "42", "{\"key\":\"value\"}", "[0,1]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "bracket_notation_without_quotes",
        .selector = "$[key]",
        .document = "{\"key\": \"value\"}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "current_with_dot_notation",
        .selector = "@.a",
        .document = "{\"a\": 1}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_bracket_notation",
        .selector = "$.['key']",
        .document = "{\"key\": \"value\", \"other\": {\"key\": [{\"key\": 42}]}}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_bracket_notation_with_double_quotes",
        .selector = "$.[\"key\"]",
        .document = "{\"key\": \"value\", \"other\": {\"key\": [{\"key\": 42}]}}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_bracket_notation_without_quotes",
        .selector = "$.[key]",
        .document = "{\"key\": \"value\", \"other\": {\"key\": [{\"key\": 42}]}}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_notation",
        .selector = "$.key",
        .document = "{\"key\": \"value\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_after_array_slice",
        .selector = "$[0:2].key",
        .alt_selector = "$[:2].key",
        .document = "[{\"key\": \"ey\"}, {\"key\": \"bee\"}, {\"key\": \"see\"}]",
        .result = (char *[]){"\"ey\"", "\"bee\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_after_bracket_notation_after_recursive_descent",
        .selector = "$..[1].key",
        .document = "{\"k\": [{\"key\": \"some value\"}, {\"key\": 42}], \"kk\": [[{\"key\": 100}, {\"key\": 200}, {\"key\": 300}], [{\"key\": 400}, {\"key\": 500}, {\"key\": 600}]], \"key\": [0, 1]}",
        .result = (char *[]){"42", "200", "500", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_after_bracket_notation_with_wildcard",
        .selector = "$[*].a",
        .document = "[{\"a\": 1}, {\"a\": 1}]",
        .result = (char *[]){"1", "1", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_after_bracket_notation_with_wildcard_on_one_matching",
        .selector = "$[*].a",
        .document = "[{\"a\": 1}]",
        .result = (char *[]){"1", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_after_bracket_notation_with_wildcard_on_some_matching",
        .selector = "$[*].a",
        .document = "[{\"a\": 1}, {\"b\": 1}]",
        .result = (char *[]){"1", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_after_filter_expression",
        .selector = "$[?(@.id==42)].name",
        .alt_selector = "$[?(@.id == 42)].name",
        .document = "[{\"id\": 42, \"name\": \"forty-two\"}, {\"id\": 1, \"name\": \"one\"}]",
        .result = (char *[]){"\"forty-two\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_after_recursive_descent",
        .selector = "$..key",
        .alt_selector = "$..['key']",
        .document = "{\"object\": {\"key\": \"value\", \"array\": [{\"key\": \"something\"}, {\"key\": {\"key\": \"russian dolls\"}}]}, \"key\": \"top\"}",
        .result = (char *[]){"\"top\"", "\"value\"", "\"something\"", "{\"key\":\"russian dolls\"}", "\"russian dolls\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_after_recursive_descent_after_dot_notation",
        .selector = "$.store..price",
        .alt_selector = "$.store..['price']",
        .document = "{\"store\": {\"book\": [{\"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95}, {\"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99}, {\"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99}, {\"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99}], \"bicycle\": {\"color\": \"red\", \"price\": 19.95}}}",
        .result = (char *[]){"8.95", "12.99", "8.99", "22.99", "19.95", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_after_recursive_descent_with_extra_dot",
        .selector = "$...key",
        .document = "{\"object\": {\"key\": \"value\", \"array\": [{\"key\": \"something\"}, {\"key\": {\"key\": \"russian dolls\"}}]}, \"key\": \"top\"}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_notation_after_union",
        .selector = "$[0,2].key",
        .document = "[{\"key\": \"ey\"}, {\"key\": \"bee\"}, {\"key\": \"see\"}]",
        .result = (char *[]){"\"ey\"", "\"see\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_after_union_with_keys",
        .selector = "$['one','three'].key",
        .alt_selector = "$[one,three].key",
        .document = "{\"one\": {\"key\": \"value\"}, \"two\": {\"k\": \"v\"}, \"three\": {\"some\": \"more\", \"key\": \"other value\"}}",
        .result = (char *[]){"\"value\"", "\"other value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_on_array",
        .selector = "$.key",
        .document = "[0, 1]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "dot_notation_on_array_value",
        .selector = "$.key",
        .document = "{\"key\": [\"first\", \"second\"]}",
        .result = (char *[]){"[\"first\",\"second\"]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_on_array_with_containing_object_matching_key",
        .selector = "$.id",
        .document = "[{\"id\": 2}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "dot_notation_on_empty_object_value",
        .selector = "$.key",
        .document = "{\"key\": {}}",
        .result = (char *[]){"{}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_on_null_value",
        .selector = "$.key",
        .document = "{\"key\": null}",
        .result = (char *[]){"null", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_on_object_without_key",
        .selector = "$.missing",
        .document = "{\"key\": \"value\"}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "dot_notation_with_dash",
        .selector = "$.key-dash",
        .document = "{\"key\": 42, \"key-\": 43, \"-\": 44, \"dash\": 45, \"-dash\": 46, \"\": 47, \"key-dash\": \"value\", \"something\": \"else\"}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_notation_with_double_quotes",
        .selector = "$.\"key\"",
        .document = "{\"key\": \"value\", \"\\\"key\\\"\": 42}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_notation_with_double_quotes_after_recursive_descent",
        .selector = "$..\"key\"",
        .document = "{\"object\": {\"key\": \"value\", \"\\\"key\\\"\": 100, \"array\": [{\"key\": \"something\", \"\\\"key\\\"\": 0}, {\"key\": {\"key\": \"russian dolls\"}, \"\\\"key\\\"\": {\"\\\"key\\\"\": 99}}]}, \"key\": \"top\", \"\\\"key\\\"\": 42}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_notation_with_empty_path",
        .selector = "$.",
        .document = "{\"key\": 42, \"\": 9001, \"''\": \"nice\"}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_notation_with_key_named_in",
        .selector = "$.in",
        .document = "{\"in\": \"value\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_with_key_named_length",
        .selector = "$.length",
        .document = "{\"length\": \"value\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_with_key_named_length_on_array",
        .selector = "$.length",
        .document = "[4, 5, 6]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "dot_notation_with_key_named_null",
        .selector = "$.null",
        .document = "{\"null\": \"value\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_with_key_named_true",
        .selector = "$.true",
        .document = "{\"true\": \"value\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_with_key_root_literal",
        .selector = "$.$",
        .document = "{\"$\": \"value\"}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_notation_with_non_ASCII_key",
        .selector = "$.屬性",
        .document = "{\"\u5c6c\u6027\": \"value\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_with_number",
        .selector = "$.2",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_notation_with_number_-1",
        .selector = "$.-1",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_notation_with_number_on_object",
        .selector = "$.2",
        .document = "{\"a\": \"first\", \"2\": \"second\", \"b\": \"third\"}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_notation_with_single_quotes",
        .selector = "$.'key'",
        .document = "{\"key\": \"value\", \"'key'\": 42}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_notation_with_single_quotes_after_recursive_descent",
        .selector = "$..'key'",
        .document = "{\"object\": {\"key\": \"value\", \"'key'\": 100, \"array\": [{\"key\": \"something\", \"'key'\": 0}, {\"key\": {\"key\": \"russian dolls\"}, \"'key'\": {\"'key'\": 99}}]}, \"key\": \"top\", \"'key'\": 42}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_notation_with_single_quotes_and_dot",
        .selector = "$.'some.key'",
        .document = "{\"some.key\": 42, \"some\": {\"key\": \"value\"}, \"'some.key'\": 43}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_notation_with_space_padded_key",
        .selector = "$. a ",
        .alt_selector = "$.a",
        .document = "{\" a\": 1, \"a\": 2, \" a \": 3, \"\": 4}",
        .result = (char *[]){"2", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_with_wildcard_after_dot_notation_after_dot_notation_with_wildcard",
        .selector = "$.*.bar.*",
        .document = "[{\"bar\": [42]}]",
        .result = (char *[]){"42", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_with_wildcard_after_dot_notation_with_wildcard_on_nested_arrays",
        .selector = "$.*.*",
        .document = "[[1, 2, 3], [4, 5, 6]]",
        .result = (char *[]){"1", "2", "3", "4", "5", "6", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_with_wildcard_after_recursive_descent",
        .selector = "$..*",
        .alt_selector = "$..[*]",
        .document = "{\"key\": \"value\", \"another key\": {\"complex\": \"string\", \"primitives\": [0, 1]}}",
        .result = (char *[]){"\"value\"", "{\"complex\":\"string\",\"primitives\":[0,1]}", "\"string\"", "[0,1]", "0", "1", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_with_wildcard_after_recursive_descent_on_null_value_array",
        .selector = "$..*",
        .alt_selector = "$..[*]",
        .document = "[40, null, 42]",
        .result = (char *[]){"40", "null", "42", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_with_wildcard_after_recursive_descent_on_scalar",
        .selector = "$..*",
        .alt_selector = "$..[*]",
        .document = "42",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "dot_notation_with_wildcard_on_array",
        .selector = "$.*",
        .document = "[\"string\", 42, {\"key\": \"value\"}, [0, 1]]",
        .result = (char *[]){"\"string\"", "42", "{\"key\":\"value\"}", "[0,1]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_with_wildcard_on_empty_array",
        .selector = "$.*",
        .document = "[]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "dot_notation_with_wildcard_on_empty_object",
        .selector = "$.*",
        .document = "{}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "dot_notation_with_wildcard_on_object",
        .selector = "$.*",
        .document = "{\"some\": \"string\", \"int\": 42, \"object\": {\"key\": \"value\"}, \"array\": [0, 1]}",
        .result = (char *[]){"\"string\"", "42", "{\"key\":\"value\"}", "[0,1]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_without_dot",
        .selector = "$a",
        .document = "{\"a\": 1, \"$a\": 2}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_notation_without_root",
        .selector = ".key",
        .document = "{\"key\": \"value\"}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "dot_notation_without_root_and_dot",
        .selector = "key",
        .document = "{\"key\": \"value\"}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "empty",
        .selector = "",
        .document = "{\"a\": 42, \"\": 21}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_after_dot_notation_with_wildcard_after_recursive_descent",
        .selector = "$..*[?(@.id>2)]",
        .alt_selector = "$..[*][?(@.id>2)]",
        .document = "[{\"complext\": {\"one\": [{\"name\": \"first\", \"id\": 1}, {\"name\": \"next\", \"id\": 2}, {\"name\": \"another\", \"id\": 3}, {\"name\": \"more\", \"id\": 4}], \"more\": {\"name\": \"next to last\", \"id\": 5}}}, {\"name\": \"last\", \"id\": 6}]",
        .result = (char *[]){"{\"name\":\"next to last\",\"id\":5}", "{\"name\":\"another\",\"id\":3}", "{\"name\":\"more\",\"id\":4}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_after_recursive_descent",
        .selector = "$..[?(@.id==2)]",
        .document = "{\"id\": 2, \"more\": [{\"id\": 2}, {\"more\": {\"id\": 2}}, {\"id\": {\"id\": 2}}, [{\"id\": 2}]]}",
        .result = (char *[]){"{\"id\":2}", "{\"id\":2}", "{\"id\":2}", "{\"id\":2}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_on_object",
        .selector = "$[?(@.key)]",
        .document = "{\"key\": 42, \"another\": {\"key\": 1}}",
        .result = (char *[]){"{\"key\":1}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_addition",
        .selector = "$[?(@.key+50==100)]",
        .document = "[{\"key\": 60}, {\"key\": 50}, {\"key\": 10}, {\"key\": -50}, {\"key+50\": 100}]",
        .result = (char *[]){"{\"key\":50}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_boolean_and_operator",
        .selector = "$[?(@.key>42 && @.key<44)]",
        .alt_selector = "$[?(@.key>42&&@.key<44)]",
        .document = "[{\"key\": 42}, {\"key\": 43}, {\"key\": 44}]",
        .result = (char *[]){"{\"key\":43}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_boolean_and_operator_and_value_false",
        .selector = "$[?(@.key>0 && false)]",
        .document = "[{\"key\": 1}, {\"key\": 3}, {\"key\": \"nice\"}, {\"key\": true}, {\"key\": null}, {\"key\": false}, {\"key\": {}}, {\"key\": []}, {\"key\": -1}, {\"key\": 0}, {\"key\": \"\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "filter_expression_with_boolean_and_operator_and_value_true",
        .selector = "$[?(@.key>0 && true)]",
        .document = "[{\"key\": 1}, {\"key\": 3}, {\"key\": \"nice\"}, {\"key\": true}, {\"key\": null}, {\"key\": false}, {\"key\": {}}, {\"key\": []}, {\"key\": -1}, {\"key\": 0}, {\"key\": \"\"}]",
        .result = (char *[]){"{\"key\":1}", "{\"key\":3}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_boolean_or_operator",
        .selector = "$[?(@.key>43 || @.key<43)]",
        .document = "[{\"key\": 42}, {\"key\": 43}, {\"key\": 44}]",
        .result = (char *[]){"{\"key\":42}","{\"key\":44}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_boolean_or_operator_and_value_false",
        .selector = "$[?(@.key>0 || false)]",
        .alt_selector = "$[?(@.key > 0 || false)]",
        .document = "[{\"key\": 1}, {\"key\": 3}, {\"key\": \"nice\"}, {\"key\": true}, {\"key\": null}, {\"key\": false}, {\"key\": {}}, {\"key\": []}, {\"key\": -1}, {\"key\": 0}, {\"key\": \"\"}]",
        .result = (char *[]){"{\"key\":1}", "{\"key\":3}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_boolean_or_operator_and_value_true",
        .selector = "$[?(@.key>0 || true)]",
        .alt_selector = "$[?(@.key > 0 || true)]",
        .document = "[{\"key\": 1}, {\"key\": 3}, {\"key\": \"nice\"}, {\"key\": true}, {\"key\": null}, {\"key\": false}, {\"key\": {}}, {\"key\": []}, {\"key\": -1}, {\"key\": 0}, {\"key\": \"\"}]",
        .result = (char *[]){"{\"key\":1}", "{\"key\":3}", "{\"key\":\"nice\"}", "{\"key\":true}", "{\"key\":null}", "{\"key\":false}", "{\"key\":{}}", "{\"key\":[]}", "{\"key\":-1}", "{\"key\":0}", "{\"key\":\"\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_bracket_notation",
        .selector = "$[?(@['key']==42)]",
        .alt_selector = "$[?(@['key'] == 42)]",
        .document = "[{\"key\": 0}, {\"key\": 42}, {\"key\": -1}, {\"key\": 41}, {\"key\": 43}, {\"key\": 42.0001}, {\"key\": 41.9999}, {\"key\": 100}, {\"some\": \"value\"}]",
        .result = (char *[]){"{\"key\":42}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_bracket_notation_and_current_object_literal",
        .selector = "$[?(@['@key']==42)]",
        .alt_selector = "$[?(@['@key'] == 42)]",
        .document = "[{\"@key\": 0}, {\"@key\": 42}, {\"key\": 42}, {\"@key\": 43}, {\"some\": \"value\"}]",
        .result = (char *[]){"{\"@key\":42}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_bracket_notation_with_-1",
        .selector = "$[?(@[-1]==2)]",
        .alt_selector = "$[?(@[-1] == 2)]",
        .document = "[[2, 3], [\"a\"], [0, 2], [2]]",
        .result = (char *[]){"[0,2]", "[2]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_bracket_notation_with_number",
        .selector = "$[?(@[1]=='b')]",
        .alt_selector = "$[?(@[1]==\"b\")]",
        .document = "[[\"a\", \"b\"], [\"x\", \"y\"]]",
        .result = (char *[]){"[\"a\",\"b\"]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_bracket_notation_with_number_on_object",
        .selector = "$[?(@[1]=='b')]",
        .alt_selector = "$[?(@[1]==\"b\")]",
        .document = "{\"1\": [\"a\", \"b\"], \"2\": [\"x\", \"y\"]}",
        .result = (char *[]){"[\"a\",\"b\"]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_current_object",
        .selector = "$[?(@)]",
        .document = "[\"some value\", null, \"value\", 0, 1, -1, \"\", [], {}, false, true]",
        .result = (char *[]){"\"some value\"", "null", "\"value\"", "0", "1", "-1" , "\"\"", "[]", "{}", "false", "true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_different_grouped_operators",
        .selector = "$[?(@.a && (@.b || @.c))]",
        .document = "[{\"a\": true}, {\"a\": true, \"b\": true}, {\"a\": true, \"b\": true, \"c\": true}, {\"b\": true, \"c\": true}, {\"a\": true, \"c\": true}, {\"c\": true}, {\"b\": true}]",
        .result = (char *[]){"{\"a\":true,\"b\":true}", "{\"a\":true,\"b\":true,\"c\":true}", "{\"a\":true,\"c\":true}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_different_ungrouped_operators",
        .selector = "$[?(@.a && @.b || @.c)]",
        .document = "[{\"a\": true, \"b\": true}, {\"a\": true, \"b\": true, \"c\": true}, {\"b\": true, \"c\": true}, {\"a\": true, \"c\": true}, {\"a\": true}, {\"b\": true}, {\"c\": true}, {\"d\": true}, {}]",
        .result = (char *[]){"{\"a\":true,\"b\":true}", "{\"a\":true,\"b\":true,\"c\":true}", "{\"b\":true,\"c\":true}", "{\"a\":true,\"c\":true}", "{\"c\":true}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_division",
        .selector = "$[?(@.key/10==5)]",
        .alt_selector = "$[?(@.key / 10 == 5)]",
        .document = "[{\"key\": 60}, {\"key\": 50}, {\"key\": 10}, {\"key\": -50}, {\"key/10\": 5}]",
        .result = (char *[]){"{\"key\":50}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_dot_notation_with_dash",
        .selector = "$[?(@.key-dash == 'value')]",
        .document = "[{\"key-dash\": \"value\"}]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_dot_notation_with_number",
        .selector = "$[?(@.2 == 'second')]",
        .document = "[{\"a\": \"first\", \"2\": \"second\", \"b\": \"third\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_dot_notation_with_number_on_array",
        .selector = "$[?(@.2 == 'third')]",
        .document = "[[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_empty_expression",
        .selector = "$[?()]",
        .document = "[1, {\"key\": 42}, \"value\", null]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_equals",
        .selector = "$[?(@.key==42)]",
        .alt_selector = "$[?(@.key == 42)]",
        .document = "[{\"key\": 0}, {\"key\": 42}, {\"key\": -1}, {\"key\": 1}, {\"key\": 41}, {\"key\": 43}, {\"key\": 42.0001}, {\"key\": 41.9999}, {\"key\": 100}, {\"key\": \"some\"}, {\"key\": \"42\"}, {\"key\": null}, {\"key\": 420}, {\"key\": \"\"}, {\"key\": {}}, {\"key\": []}, {\"key\": [42]}, {\"key\": {\"key\": 42}}, {\"key\": {\"some\": 42}}, {\"some\": \"value\"}]",
        .result = (char *[]){"{\"key\":42}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_array",
        .selector = "$[?(@.d==[\"v1\",\"v2\"])]",
        .document = "[{\"d\": [\"v1\", \"v2\"]}, {\"d\": [\"a\", \"b\"]}, {\"d\": \"v1\"}, {\"d\": \"v2\"}, {\"d\": {}}, {\"d\": []}, {\"d\": null}, {\"d\": -1}, {\"d\": 0}, {\"d\": 1}, {\"d\": \"['v1','v2']\"}, {\"d\": \"['v1', 'v2']\"}, {\"d\": \"v1,v2\"}, {\"d\": \"[\\\"v1\\\", \\\"v2\\\"]\"}, {\"d\": \"[\\\"v1\\\", \\\"v2\\\"]\"}]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_equals_array_for_array_slice_with_range_1",
        .selector = "$[?(@[0:1]==[1])]",
        .document = "[[1, 2, 3], [1], [2, 3], 1, 2]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_equals_array_for_dot_notation_with_star",
        .selector = "$[?(@.*==[1,2])]",
        .document = "[[1, 2], [2, 3], [1], [2], [1, 2, 3], 1, 2, 3]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_equals_array_or_equals_true",
        .selector = "$[?(@.d==[\"v1\",\"v2\"] || (@.d == true))]",
        .document = "[{\"d\": [\"v1\", \"v2\"]}, {\"d\": [\"a\", \"b\"]}, {\"d\": true}]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_equals_array_with_single_quotes",
        .selector = "$[?(@.d==['v1','v2'])]",
        .document = "[{\"d\": [\"v1\", \"v2\"]}, {\"d\": [\"a\", \"b\"]}, {\"d\": \"v1\"}, {\"d\": \"v2\"}, {\"d\": {}}, {\"d\": []}, {\"d\": null}, {\"d\": -1}, {\"d\": 0}, {\"d\": 1}, {\"d\": \"['v1','v2']\"}, {\"d\": \"['v1', 'v2']\"}, {\"d\": \"v1,v2\"}, {\"d\": \"[\\\"v1\\\", \\\"v2\\\"]\"}, {\"d\": \"[\\\"v1\\\",\\\"v2\\\"]\"}]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_equals_boolean_expression_value",
        .selector = "$[?((@.key<44)==false)]",
        .document = "[{\"key\": 42}, {\"key\": 43}, {\"key\": 44}]",
        .result = (char *[]){"{\"key\":44}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_false",
        .selector = "$[?(@.key==false)]",
        .document = "[{\"some\": \"some value\"}, {\"key\": true}, {\"key\": false}, {\"key\": null}, {\"key\": \"value\"}, {\"key\": \"\"}, {\"key\": 0}, {\"key\": 1}, {\"key\": -1}, {\"key\": 42}, {\"key\": {}}, {\"key\": []}]",
        .result = (char *[]){"{\"key\":false}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_null",
        .selector = "$[?(@.key==null)]",
        .document = "[{\"some\": \"some value\"}, {\"key\": true}, {\"key\": false}, {\"key\": null}, {\"key\": \"value\"}, {\"key\": \"\"}, {\"key\": 0}, {\"key\": 1}, {\"key\": -1}, {\"key\": 42}, {\"key\": {}}, {\"key\": []}]",
        .result = (char *[]){"{\"key\":null}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_number_for_array_slice_with_range_1",
        .selector = "$[?(@[0:1]==1)]",
        .alt_selector = "$[?(@[:1]==1)]",
        .document = "[[1, 2, 3], [1], [2, 3], 1, 2]",
        .result = (char *[]){"[1,2,3]", "[1]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_number_for_bracket_notation_with_star",
        .selector = "$[?(@[*]==2)]",
        .document = "[[1, 2], [2, 3], [1], [2], [1, 2, 3], 1, 2, 3]",
        .result = (char *[]){"[1,2]", "[2,3]", "[2]", "[1,2,3]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_number_for_dot_notation_with_star",
        .selector = "$[?(@.*==2)]",
        .document = "[[1, 2], [2, 3], [1], [2], [1, 2, 3], 1, 2, 3]",
        .result = (char *[]){"[1,2]", "[2,3]", "[2]", "[1,2,3]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_number_with_fraction",
        .selector = "$[?(@.key==-0.123e2)]",
        .alt_selector = "$[?(@.key==-12.3)]",
        .document = "[{\"key\": -12.3}, {\"key\": -0.123}, {\"key\": -12}, {\"key\": 12.3}, {\"key\": 2}, {\"key\": \"-0.123e2\"}]",
        .result = (char *[]){"{\"key\":-12.3}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_number_with_leading_zeros",
        .selector = "$[?(@.key==010)]",
        .document = "[{\"key\": \"010\"}, {\"key\": \"10\"}, {\"key\": 10}, {\"key\": 0}, {\"key\": 8}]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_equals_object",
        .selector = "$[?(@.d=={\"k\":\"v\"})]",
        .document = "[{\"d\": {\"k\": \"v\"}}, {\"d\": {\"a\": \"b\"}}, {\"d\": \"k\"}, {\"d\": \"v\"}, {\"d\": {}}, {\"d\": []}, {\"d\": null}, {\"d\": -1}, {\"d\": 0}, {\"d\": 1}, {\"d\": \"[object Object]\"}, {\"d\": \"{\\\"k\\\": \\\"v\\\"}\"}, {\"d\": \"{\\\"k\\\":\\\"v\\\"}\"}, \"v\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_equals_on_array_of_numbers",
        .selector = "$[?(@==42)]",
        .document = "[0, 42, -1, 41, 43, 42.0001, 41.9999, null, 100]",
        .result = (char *[]){"42", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_on_array_without_match",
        .selector = "$[?(@.key==43)]",
        .document = "[{\"key\": 42}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "filter_expression_with_equals_on_object",
        .selector = "$[?(@.key==42)]",
        .document = "{\"a\": {\"key\": 0}, \"b\": {\"key\": 42}, \"c\": {\"key\": -1}, \"d\": {\"key\": 41}, \"e\": {\"key\": 43}, \"f\": {\"key\": 42.0001}, \"g\": {\"key\": 41.9999}, \"h\": {\"key\": 100}, \"i\": {\"some\": \"value\"}}",
        .result = (char *[]){"{\"key\":42}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_on_object_with_key_matching_query",
        .selector = "$[?(@.id==2)]",
        .document = "{\"id\": 2}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "filter_expression_with_equals_string",
        .selector = "$[?(@.key==\"value\")]",
        .document = "[{\"key\": \"some\"}, {\"key\": \"value\"}, {\"key\": null}, {\"key\": 0}, {\"key\": 1}, {\"key\": -1}, {\"key\": \"\"}, {\"key\": {}}, {\"key\": []}, {\"key\": \"valuemore\"}, {\"key\": \"morevalue\"}, {\"key\": [\"value\"]}, {\"key\": {\"some\": \"value\"}}, {\"key\": {\"key\": \"value\"}}, {\"some\": \"value\"}]",
        .result = (char *[]){"{\"key\":\"value\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_string_in_NFC",
        .selector = "$[?(@.key==\"Motörhead\")]",
        .document = "[{\"key\": \"something\"}, {\"key\": \"Mot\\u00f6rhead\"}, {\"key\": \"mot\\u00f6rhead\"}, {\"key\": \"Motorhead\"}, {\"key\": \"Motoo\\u0308rhead\"}, {\"key\": \"motoo\\u0308rhead\"}]",
        .result = (char *[]){"{\"key\":\"Motörhead\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_string_with_current_object_literal",
        .selector = "$[?(@.key==\"hi@example.com\")]",
        .document = "[{\"key\": \"some\"}, {\"key\": \"value\"}, {\"key\": \"hi@example.com\"}]",
        .result = (char *[]){"{\"key\":\"hi@example.com\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_string_with_dot_literal",
        .selector = "$[?(@.key==\"some.value\")]",
        .document = "[{\"key\": \"some\"}, {\"key\": \"value\"}, {\"key\": \"some.value\"}]",
        .result = (char *[]){"{\"key\":\"some.value\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_string_with_single_quotes",
        .selector = "$[?(@.key=='value')]",
        .alt_selector = "$[?(@.key==\"value\")]",
        .document = "[{\"key\": \"some\"}, {\"key\": \"value\"}]",
        .result = (char *[]){"{\"key\":\"value\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_string_with_unicode_character_escape",
        .selector = "$[?(@.key==\"Mot\\u00f6rhead\")]",
        .alt_selector = "$[?(@.key==\"Motörhead\")]",
        .document = "[{\"key\": \"something\"}, {\"key\": \"Mot\\u00f6rhead\"}, {\"key\": \"mot\\u00f6rhead\"}, {\"key\": \"Motorhead\"}, {\"key\": \"Motoo\\u0308rhead\"}, {\"key\": \"motoo\\u0308rhead\"}]",
        .result = (char *[]){"{\"key\":\"Motörhead\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_true",
        .selector = "$[?(@.key==true)]",
        .document = "[{\"some\": \"some value\"}, {\"key\": true}, {\"key\": false}, {\"key\": null}, {\"key\": \"value\"}, {\"key\": \"\"}, {\"key\": 0}, {\"key\": 1}, {\"key\": -1}, {\"key\": 42}, {\"key\": {}}, {\"key\": []}]",
        .result = (char *[]){"{\"key\":true}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_with_path_and_path",
        .selector = "$[?(@.key1==@.key2)]",
        .document = "[{\"key1\": 10, \"key2\": 10}, {\"key1\": 42, \"key2\": 50}, {\"key1\": 10}, {\"key2\": 10}, {}, {\"key1\": null, \"key2\": null}, {\"key1\": null}, {\"key2\": null}, {\"key1\": 0, \"key2\": 0}, {\"key1\": 0}, {\"key2\": 0}, {\"key1\": -1, \"key2\": -1}, {\"key1\": \"\", \"key2\": \"\"}, {\"key1\": false, \"key2\": false}, {\"key1\": false}, {\"key2\": false}, {\"key1\": true, \"key2\": true}, {\"key1\": [], \"key2\": []}, {\"key1\": {}, \"key2\": {}}, {\"key1\": {\"a\": 1, \"b\": 2}, \"key2\": {\"b\": 2, \"a\": 1}}]",
        .result = (char *[]){"{\"key1\":10,\"key2\":10}", "{}", "{\"key1\":null,\"key2\":null}", "{\"key1\":0,\"key2\":0}", "{\"key1\":-1,\"key2\":-1}", "{\"key1\":\"\",\"key2\":\"\"}", "{\"key1\":false,\"key2\":false}", "{\"key1\":true,\"key2\":true}", "{\"key1\":[],\"key2\":[]}", "{\"key1\":{},\"key2\":{}}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_with_root_reference",
        .selector = "$.items[?(@.key==$.value)]",
        .document = "{\"value\": 42, \"items\": [{\"key\": 10}, {\"key\": 42}, {\"key\": 50}]}",
        .result = (char *[]){"{\"key\":42}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_greater_than",
        .selector = "$[?(@.key>42)]",
        .document = "[{\"key\": 0}, {\"key\": 42}, {\"key\": -1}, {\"key\": 41}, {\"key\": 43}, {\"key\": 42.0001}, {\"key\": 41.9999}, {\"key\": 100}, {\"key\": \"43\"}, {\"key\": \"42\"}, {\"key\": \"41\"}, {\"key\": \"value\"}, {\"some\": \"value\"}]",
        .result = (char *[]){"{\"key\":43}", "{\"key\":42.0001}", "{\"key\":100}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_greater_than_or_equal",
        .selector = "$[?(@.key>=42)]",
        .document = "[{\"key\": 0}, {\"key\": 42}, {\"key\": -1}, {\"key\": 41}, {\"key\": 43}, {\"key\": 42.0001}, {\"key\": 41.9999}, {\"key\": 100}, {\"key\": \"43\"}, {\"key\": \"42\"}, {\"key\": \"41\"}, {\"key\": \"value\"}, {\"some\": \"value\"}]",
        .result = (char *[]){"{\"key\":42}", "{\"key\":43}", "{\"key\":42.0001}", "{\"key\":100}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_greater_than_string",
        .selector = "$[?(@.key>\"VALUE\")]",
        .document = "[{\"key\": 0}, {\"key\": 42}, {\"key\": -1}, {\"key\": 41}, {\"key\": 43}, {\"key\": 42.0001}, {\"key\": 41.9999}, {\"key\": 100}, {\"key\": \"43\"}, {\"key\": \"42\"}, {\"key\": \"41\"}, {\"key\": \"alpha\"}, {\"key\": \"ALPHA\"}, {\"key\": \"value\"}, {\"key\": \"VALUE\"}, {\"some\": \"value\"}, {\"some\": \"VALUE\"}]",
        .result = (char *[]){"{\"key\":\"alpha\"}", "{\"key\":\"value\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_in_array_of_values",
        .selector = "$[?(@.d in [2, 3])]",
        .document = "[{\"d\": 1}, {\"d\": 2}, {\"d\": 1}, {\"d\": 3}, {\"d\": 4}]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_in_current_object",
        .selector = "$[?(2 in @.d)]",
        .document = "[{\"d\": [1, 2, 3]}, {\"d\": [2]}, {\"d\": [1]}, {\"d\": [3, 4]}, {\"d\": [4, 2]}]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_length_free_function",
        .selector = "$[?(length(@) == 4)]",
        .alt_selector = "$[?(length(@)==4)]",
        .document = "[[1, 2, 3, 4, 5], [1, 2, 3, 4], [1, 2, 3]]",
        .result = (char *[]){"[1,2,3,4]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_length_function",
        .selector = "$[?(@.length() == 4)]",
        .document = "[[1, 2, 3, 4, 5], [1, 2, 3, 4], [1, 2, 3]]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_length_property",
        .selector = "$[?(@.length == 4)]",
        .alt_selector = "$[?(@.length==4)]",
        .document = "[[1, 2, 3, 4, 5], [1, 2, 3, 4], [1, 2, 3]]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "filter_expression_with_less_than",
        .selector = "$[?(@.key<42)]",
        .document = "[{\"key\": 0}, {\"key\": 42}, {\"key\": -1}, {\"key\": 41}, {\"key\": 43}, {\"key\": 42.0001}, {\"key\": 41.9999}, {\"key\": 100}, {\"key\": \"43\"}, {\"key\": \"42\"}, {\"key\": \"41\"}, {\"key\": \"value\"}, {\"some\": \"value\"}]",
        .result = (char *[]){"{\"key\":0}", "{\"key\":-1}", "{\"key\":41}", "{\"key\":41.9999}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_less_than_or_equal",
        .selector = "$[?(@.key<=42)]",
        .document = "[{\"key\": 0}, {\"key\": 42}, {\"key\": -1}, {\"key\": 41}, {\"key\": 43}, {\"key\": 42.0001}, {\"key\": 41.9999}, {\"key\": 100}, {\"key\": \"43\"}, {\"key\": \"42\"}, {\"key\": \"41\"}, {\"key\": \"value\"}, {\"some\": \"value\"}]",
        .result = (char *[]){"{\"key\":0}", "{\"key\":42}", "{\"key\":-1}", "{\"key\":41}", "{\"key\":41.9999}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_local_dot_key_and_null_in_data",
        .selector = "$[?(@.key='value')]",
        .document = "[{\"key\": 0}, {\"key\": \"value\"}, null, {\"key\": 42}, {\"some\": \"value\"}]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_multiplication",
        .selector = "$[?(@.key*2==100)]",
        .document = "[{\"key\": 60}, {\"key\": 50}, {\"key\": 10}, {\"key\": -50}, {\"key*2\": 100}]",
        .result = (char *[]){"{\"key\":50}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_negation_and_equals",
        .selector = "$[?(!(@.key==42))]",
        .document = "[{\"key\": 0}, {\"key\": 42}, {\"key\": -1}, {\"key\": 41}, {\"key\": 43}, {\"key\": 42.0001}, {\"key\": 41.9999}, {\"key\": 100}, {\"key\": \"43\"}, {\"key\": \"42\"}, {\"key\": \"41\"}, {\"key\": \"value\"}, {\"some\": \"value\"}]",
        .result = (char *[]){"{\"key\":0}", "{\"key\":-1}", "{\"key\":41}", "{\"key\":43}", "{\"key\":42.0001}", "{\"key\":41.9999}", "{\"key\":100}", "{\"key\":\"43\"}", "{\"key\":\"42\"}", "{\"key\":\"41\"}", "{\"key\":\"value\"}", "{\"some\":\"value\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_negation_and_equals_array_or_equals_true",
        .selector = "$[?(!(@.d==[\"v1\",\"v2\"]) || (@.d == true))]",
        .document = "[{\"d\": [\"v1\", \"v2\"]}, {\"d\": [\"a\", \"b\"]}, {\"d\": true}]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_negation_and_less_than",
        .selector = "$[?(!(@.key<42))]",
        .document = "[{\"key\": 0}, {\"key\": 42}, {\"key\": -1}, {\"key\": 41}, {\"key\": 43}, {\"key\": 42.0001}, {\"key\": 41.9999}, {\"key\": 100}, {\"key\": \"43\"}, {\"key\": \"42\"}, {\"key\": \"41\"}, {\"key\": \"value\"}, {\"some\": \"value\"}]",
        .result = (char *[]){"{\"key\":42}", "{\"key\":43}", "{\"key\":42.0001}", "{\"key\":100}", "{\"key\":\"43\"}", "{\"key\":\"42\"}", "{\"key\":\"41\"}", "{\"key\":\"value\"}", "{\"some\":\"value\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_negation_and_without_value",
        .selector = "$[?(!@.key)]",
        .alt_selector = "$[?(!(@.key))]",
        .document = "[{\"some\": \"some value\"}, {\"key\": true}, {\"key\": false}, {\"key\": null}, {\"key\": \"value\"}, {\"key\": \"\"}, {\"key\": 0}, {\"key\": 1}, {\"key\": -1}, {\"key\": 42}, {\"key\": {}}, {\"key\": []}]",
        .result = (char *[]){"{\"some\":\"some value\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_non_singular_existence_test",
        .selector = "$[?(@.a.*)]",
        .document = "[{\"a\": 0}, {\"a\": \"x\"}, {\"a\": false}, {\"a\": true}, {\"a\": null}, {\"a\": []}, {\"a\": [1]}, {\"a\": [1, 2]}, {\"a\": {}}, {\"a\": {\"x\": \"y\"}}, {\"a\": {\"x\": \"y\", \"w\": \"z\"}}]",
        .result = (char *[]){"{\"a\":[1]}", "{\"a\":[1,2]}", "{\"a\":{\"x\":\"y\"}}", "{\"a\":{\"x\":\"y\",\"w\":\"z\"}}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_not_equals",
        .selector = "$[?(@.key!=42)]",
        .document = "[{\"key\": 0}, {\"key\": 42}, {\"key\": -1}, {\"key\": 1}, {\"key\": 41}, {\"key\": 43}, {\"key\": 42.0001}, {\"key\": 41.9999}, {\"key\": 100}, {\"key\": \"some\"}, {\"key\": \"42\"}, {\"key\": null}, {\"key\": 420}, {\"key\": \"\"}, {\"key\": {}}, {\"key\": []}, {\"key\": [42]}, {\"key\": {\"key\": 42}}, {\"key\": {\"some\": 42}}, {\"some\": \"value\"}]",
        .result = (char *[]){"{\"key\":0}", "{\"key\":-1}", "{\"key\":1}", "{\"key\":41}", "{\"key\":43}", "{\"key\":42.0001}", "{\"key\":41.9999}", "{\"key\":100}", "{\"key\":\"some\"}", "{\"key\":\"42\"}", "{\"key\":null}", "{\"key\":420}", "{\"key\":\"\"}", "{\"key\":{}}", "{\"key\":[]}", "{\"key\":[42]}", "{\"key\":{\"key\":42}}", "{\"key\":{\"some\":42}}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_not_equals_array_or_equals_true",
        .selector = "$[?((@.d!=[\"v1\",\"v2\"]) || (@.d == true))]",
        .document = "[{\"d\": [\"v1\", \"v2\"]}, {\"d\": [\"a\", \"b\"]}, {\"d\": true}]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_parent_axis_operator",
        .selector = "$[*].bookmarks[?(@.page == 45)]^^^",
        .document = "[{\"title\": \"Sayings of the Century\", \"bookmarks\": [{\"page\": 40}]}, {\"title\": \"Sword of Honour\", \"bookmarks\": [{\"page\": 35}, {\"page\": 45}]}, {\"title\": \"Moby Dick\", \"bookmarks\": [{\"page\": 3035}, {\"page\": 45}]}]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_regular_expression",
//        .selector = "$[?(@.name=~/hello.*/)]", // FIXME
        .selector = "$[?(@.name=~\"hello.*\")]",
        .document = "[{\"name\": \"hullo world\"}, {\"name\": \"hello world\"}, {\"name\": \"yes hello world\"}, {\"name\": \"HELLO WORLD\"}, {\"name\": \"good bye\"}]",
        .result = (char *[]){"{\"name\":\"hello world\"}", "{\"name\":\"yes hello world\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_regular_expression_from_member",
//        .selector = "$[?(@.name=~/@.pattern/)]", // FIXME
        .selector = "$[?(@.name=~\"@.pattern\")]",
        .document = "[{\"name\": \"hullo world\"}, {\"name\": \"hello world\"}, {\"name\": \"yes hello world\"}, {\"name\": \"HELLO WORLD\"}, {\"name\": \"good bye\"}, {\"pattern\": \"hello.*\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "filter_expression_with_set_wise_comparison_to_scalar",
        .selector = "$[?(@[*]>=4)]",
        .document = "[[1, 2], [3, 4], [5, 6]]",
        .result = (char *[]){"[3,4]", "[5,6]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_set_wise_comparison_to_set",
        .selector = "$.x[?(@[*]>=$.y[*])]",
        .document = "{\"x\": [[1, 2], [3, 4], [5, 6]], \"y\": [3, 4, 5]}",
        .result = (char *[]){"[3,4]", "[5,6]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_single_equal",
        .selector = "$[?(@.key=42)]",
        .document = "[{\"key\": 0}, {\"key\": 42}, {\"key\": -1}, {\"key\": 1}, {\"key\": 41}, {\"key\": 43}, {\"key\": 42.0001}, {\"key\": 41.9999}, {\"key\": 100}, {\"key\": \"some\"}, {\"key\": \"42\"}, {\"key\": null}, {\"key\": 420}, {\"key\": \"\"}, {\"key\": {}}, {\"key\": []}, {\"key\": [42]}, {\"key\": {\"key\": 42}}, {\"key\": {\"some\": 42}}, {\"some\": \"value\"}]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_subfilter",
        .selector = "$[?(@.a[?(@.price>10)])]",
        .document = "[{\"a\": [{\"price\": 1}, {\"price\": 3}]}, {\"a\": [{\"price\": 11}]}, {\"a\": [{\"price\": 8}, {\"price\": 12}, {\"price\": 3}]}, {\"a\": []}]",
        .result = (char *[]){"{\"a\":[{\"price\":11}]}", "{\"a\":[{\"price\":8},{\"price\":12},{\"price\":3}]}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_subpaths",
        .selector = "$[?(@.a.b==3)]",
        .document = "[{\"a\": {\"b\": 3}}, {\"a\": {\"b\": 2}}]",
        .result = (char *[]){"{\"a\":{\"b\":3}}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_subpaths_deeply_nested",
        .selector = "$[?(@.a.b.c==3)]",
        .document = "[{\"a\": {\"b\": {\"c\": 3}}}, {\"a\": 3}, {\"c\": 3}, {\"a\": {\"b\": {\"c\": 2}}}]",
        .result = (char *[]){"{\"a\":{\"b\":{\"c\":3}}}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_subtraction",
        .selector = "$[?(@.key-50==-100)]",
        .document = "[{\"key\": 60}, {\"key\": 50}, {\"key\": 10}, {\"key\": -50}, {\"key-50\": -100}]",
        .result = (char *[]){"{\"key\":-50}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_tautological_comparison",
        .selector = "$[?(1==1)]",
        .document = "[1, 3, \"nice\", true, null, false, {}, [], -1, 0, \"\"]",
        .result = (char *[]){"1", "3", "\"nice\"", "true", "null", "false", "{}", "[]", "-1", "0", "\"\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_triple_equal",
        .selector = "$[?(@.key===42)]",
        .document = "[{\"key\": 0}, {\"key\": 42}, {\"key\": -1}, {\"key\": 1}, {\"key\": 41}, {\"key\": 43}, {\"key\": 42.0001}, {\"key\": 41.9999}, {\"key\": 100}, {\"key\": \"some\"}, {\"key\": \"42\"}, {\"key\": null}, {\"key\": 420}, {\"key\": \"\"}, {\"key\": {}}, {\"key\": []}, {\"key\": [42]}, {\"key\": {\"key\": 42}}, {\"key\": {\"some\": 42}}, {\"some\": \"value\"}]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "filter_expression_with_value",
        .selector = "$[?(@.key)]",
        .document = "[{\"some\": \"some value\"}, {\"key\": true}, {\"key\": false}, {\"key\": null}, {\"key\": \"value\"}, {\"key\": \"\"}, {\"key\": 0}, {\"key\": 1}, {\"key\": -1}, {\"key\": 42}, {\"key\": {}}, {\"key\": []}]",
        .result = (char *[]){"{\"key\":true}", "{\"key\":false}", "{\"key\":null}", "{\"key\":\"value\"}", "{\"key\":\"\"}", "{\"key\":0}", "{\"key\":1}", "{\"key\":-1}", "{\"key\":42}", "{\"key\":{}}", "{\"key\":[]}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_value_after_dot_notation_with_wildcard_on_array_of_objects",
        .selector = "$.*[?(@.key)]",
        .document = "[{\"some\": \"some value\"}, {\"key\": \"value\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "filter_expression_with_value_after_recursive_descent",
        .selector = "$..[?(@.id)]",
        .document = "{\"id\": 2, \"more\": [{\"id\": 2}, {\"more\": {\"id\": 2}}, {\"id\": {\"id\": 2}}, [{\"id\": 2}]]}",
        .result = (char *[]){"{\"id\":2}", "{\"id\":{\"id\":2}}", "{\"id\":2}", "{\"id\":2}", "{\"id\":2}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_value_false",
        .selector = "$[?(false)]",
        .document = "[1, 3, \"nice\", true, null, false, {}, [], -1, 0, \"\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "filter_expression_with_value_from_recursive_descent",
        .selector = "$[?(@..child)]",
        .alt_selector = "$[?(@..['child'])]",
        .document = "[{\"key\": [{\"child\": 1}, {\"child\": 2}]}, {\"key\": [{\"child\": 2}]}, {\"key\": [{}]}, {\"key\": [{\"something\": 42}]}, {}]",
        .result = (char *[]){"{\"key\":[{\"child\":1},{\"child\":2}]}", "{\"key\":[{\"child\":2}]}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_value_null",
        .selector = "$[?(null)]",
        .document = "[1, 3, \"nice\", true, null, false, {}, [], -1, 0, \"\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "filter_expression_with_value_true",
        .selector = "$[?(true)]",
        .document = "[1, 3, \"nice\", true, null, false, {}, [], -1, 0, \"\"]",
        .result = (char *[]){"1", "3", "\"nice\"", "true", "null", "false", "{}", "[]", "-1", "0", "\"\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_without_parens",
        .selector = "$[?@.key==42]",
        .alt_selector = "$[?(@.key==42)]",
        .document = "[{\"key\": 0}, {\"key\": 42}, {\"key\": -1}, {\"key\": 1}, {\"key\": 41}, {\"key\": 43}, {\"key\": 42.0001}, {\"key\": 41.9999}, {\"key\": 100}, {\"key\": \"some\"}, {\"key\": \"42\"}, {\"key\": null}, {\"key\": 420}, {\"key\": \"\"}, {\"key\": {}}, {\"key\": []}, {\"key\": [42]}, {\"key\": {\"key\": 42}}, {\"key\": {\"some\": 42}}, {\"some\": \"value\"}]",
        .result = (char *[]){"{\"key\":42}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_without_value",
        .selector = "$[?(@.key)]",
        .document = "[{\"some\": \"some value\"}, {\"key\": true}, {\"key\": false}, {\"key\": null}, {\"key\": \"value\"}, {\"key\": \"\"}, {\"key\": 0}, {\"key\": 1}, {\"key\": -1}, {\"key\": 42}, {\"key\": {}}, {\"key\": []}]",
        .result = (char *[]){"{\"key\":true}", "{\"key\":false}", "{\"key\":null}", "{\"key\":\"value\"}", "{\"key\":\"\"}", "{\"key\":0}", "{\"key\":1}", "{\"key\":-1}", "{\"key\":42}", "{\"key\":{}}", "{\"key\":[]}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "function_sum",
        .selector = "$.data.sum()",
        .document = "{\"data\": [1, 2, 3, 4]}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "parens_notation",
        .selector = "$(key,more)",
        .document = "{\"key\": 1, \"some\": 2, \"more\": 3}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id = "recursive_descent",
        .selector = "$..",
        .document = "[{\"a\": {\"b\": \"c\"}}, [0, 1]]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "recursive_descent_after_dot_notation",
        .selector = "$.key..",
        .document = "{\"some key\": \"value\", \"key\": {\"complex\": \"string\", \"primitives\": [0, 1]}}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "recursive_descent_on_nested_arrays",
        .selector = "$..*",
        .alt_selector = "$..[*]",
        .document = "[[0], [1]]",
        .result = (char *[]){"[0]", "[1]", "0", "1", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "root",
        .selector = "$",
        .document = "{\"key\": \"value\", \"another key\": {\"complex\": [\"a\", 1]}}",
        .result = (char *[]){"{\"key\":\"value\",\"another key\":{\"complex\":[\"a\",1]}}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "root_on_scalar",
        .selector = "$",
        .document = "42",
        .result = (char *[]){"42", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "root_on_scalar_false",
        .selector = "$",
        .document = "false",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "root_on_scalar_true",
        .selector = "$",
        .document = "true",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "script_expression",
        .selector = "$[(@.length-1)]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "union",
        .selector = "$[0,1]",
        .document = "[\"first\", \"second\", \"third\"]",
        .result = (char *[]){"\"first\"", "\"second\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_duplication_from_array",
        .selector = "$[0,0]",
        .document = "[\"a\"]",
        .result = (char *[]){"\"a\"", "\"a\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_duplication_from_object",
        .selector = "$['a','a']",
        .alt_selector = "$[a,a]",
        .document = "{\"a\": 1}",
        .result = (char *[]){"1", "1", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_filter",
        .selector = "$[?(@.key<3),?(@.key>6)]",
        .document = "[{\"key\": 1}, {\"key\": 8}, {\"key\": 3}, {\"key\": 10}, {\"key\": 7}, {\"key\": 2}, {\"key\": 6}, {\"key\": 4}]",
        .result = (char *[]){"{\"key\":1}", "{\"key\":2}", "{\"key\":8}", "{\"key\":10}", "{\"key\":7}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_keys",
        .selector = "$['key','another']",
        .alt_selector = "$[key,another]",
        .document = "{\"key\": \"value\", \"another\": \"entry\"}",
        .result = (char *[]){"\"value\"", "\"entry\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_keys_after_array_slice",
        .selector = "$[:]['c','d']",
        .alt_selector = "$[:][c,d]",
        .document = "[{\"c\": \"cc1\", \"d\": \"dd1\", \"e\": \"ee1\"}, {\"c\": \"cc2\", \"d\": \"dd2\", \"e\": \"ee2\"}]",
        .result = (char *[]){"\"cc1\"", "\"dd1\"", "\"cc2\"", "\"dd2\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_keys_after_bracket_notation",
        .selector = "$[0]['c','d']",
        .alt_selector = "$[0][c,d]",
        .document = "[{\"c\": \"cc1\", \"d\": \"dd1\", \"e\": \"ee1\"}, {\"c\": \"cc2\", \"d\": \"dd2\", \"e\": \"ee2\"}]",
        .result = (char *[]){"\"cc1\"", "\"dd1\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_keys_after_dot_notation_with_wildcard",
        .selector = "$.*['c','d']",
        .alt_selector = "$.*[c,d]",
        .document = "[{\"c\": \"cc1\", \"d\": \"dd1\", \"e\": \"ee1\"}, {\"c\": \"cc2\", \"d\": \"dd2\", \"e\": \"ee2\"}]",
        .result = (char *[]){"\"cc1\"", "\"dd1\"", "\"cc2\"", "\"dd2\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_keys_after_recursive_descent",
        .selector = "$..['c','d']",
        .alt_selector = "$..[c,d]",
        .document = "[{\"c\": \"cc1\", \"d\": \"dd1\", \"e\": \"ee1\"}, {\"c\": \"cc2\", \"child\": {\"d\": \"dd2\"}}, {\"c\": \"cc3\"}, {\"d\": \"dd4\"}, {\"child\": {\"c\": \"cc5\"}}]",
        .result = (char *[]){"\"cc1\"", "\"cc2\"", "\"cc3\"", "\"cc5\"", "\"dd1\"", "\"dd2\"", "\"dd4\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_keys_on_object_without_key",
        .selector = "$['missing','key']",
        .alt_selector = "$[missing,key]",
        .document = "{\"key\": \"value\", \"another\": \"entry\"}",
        .result = (char *[]){"\"value\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_numbers_in_decreasing_order",
        .selector = "$[4,1]",
        .document = "[1, 2, 3, 4, 5]",
        .result = (char *[]){"5", "2", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_repeated_matches_after_dot_notation_with_wildcard",
        .selector = "$.*[0,:5]",
        .document = "{\"a\": [\"string\", null, true], \"b\": [false, \"string\", 5.4]}",
        .result = (char *[]){"\"string\"", "\"string\"", "null", "true", "false", "false", "\"string\"", "5.4", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_slice_and_number",
        .selector = "$[1:3,4]",
        .document = "[1, 2, 3, 4, 5]",
        .result = (char *[]){"2", "3", "5", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_spaces",
        .selector = "$[ 0 , 1 ]",
        .alt_selector = "$[0,1]",
        .document = "[\"first\", \"second\", \"third\"]",
        .result = (char *[]){"\"first\"", "\"second\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_wildcard_and_number",
        .selector = "$[*,1]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"first\"","\"second\"","\"third\"","\"forth\"","\"fifth\"","\"second\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },

    {
        .id = "equals_number_zero_and_negative_zero",
        .selector = "$[?@.a==-0]",
        .alt_selector = "$[?(@.a==-0)]",
        .document = "[{\"a\": 0, \"d\": \"e\"}, {\"a\":0.1, \"d\": \"f\"}, {\"a\":\"0\", \"d\": \"g\"}]",
        .result = (char *[]){"{\"a\":0,\"d\":\"e\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "equals_number_with_and_without_decimal_fraction",
        .selector = "$[?@.a==1.0]",
        .alt_selector = "$[?(@.a==1)]",
        .document = "[{\"a\": 1, \"d\": \"e\"}, {\"a\":2, \"d\": \"f\"}, {\"a\":\"1\", \"d\": \"g\"}]",
        .result = (char *[]){"{\"a\":1,\"d\":\"e\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "equals_number_exponent",
        .selector = "$[?@.a==1e2]",
        .alt_selector = "$[?(@.a==100)]",
        .document = "[{\"a\": 100, \"d\": \"e\"}, {\"a\":100.1, \"d\": \"f\"}, {\"a\":\"100\", \"d\": \"g\"}]",
        .result = (char *[]){"{\"a\":100,\"d\":\"e\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "equals_number_positive_exponent",
        .selector = "$[?@.a==1e+2]",
        .alt_selector = "$[?(@.a==100)]",
        .document = "[{\"a\": 100, \"d\": \"e\"}, {\"a\":100.1, \"d\": \"f\"}, {\"a\":\"100\", \"d\": \"g\"}]",
        .result = (char *[]){"{\"a\":100,\"d\":\"e\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "equals_number_negative_exponent",
        .selector = "$[?@.a==1e-2]",
        .alt_selector = "$[?(@.a==0.01)]",
        .document = "[{\"a\": 0.01, \"d\": \"e\"}, {\"a\":0.02, \"d\": \"f\"}, {\"a\":\"0.01\", \"d\": \"g\"}]",
        .result = (char *[]){"{\"a\":0.01,\"d\":\"e\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "equals_number_decimal_fraction",
        .selector = "$[?@.a==1.1]",
        .alt_selector = "$[?(@.a==1.1)]",
        .document = "[{\"a\": 1.1, \"d\": \"e\"}, {\"a\":1.0, \"d\": \"f\"}, {\"a\":\"1.1\", \"d\": \"g\"}]",
        .result = (char *[]){"{\"a\":1.1,\"d\":\"e\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "equals_number_decimal_fraction_no_fractional_digit",
        .selector = "$[?@.a==1.]",
        .document = "[{\"a\": 1.1, \"d\": \"e\"}, {\"a\":1.0, \"d\": \"f\"}, {\"a\":\"1.1\", \"d\": \"g\"}]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id = "equals_number_decimal_fraction_exponent",
        .selector = "$[?@.a==1.1e2]",
        .alt_selector = "$[?(@.a==110)]",
        .document = "[{\"a\": 110, \"d\": \"e\"}, {\"a\":110.1, \"d\": \"f\"}, {\"a\":\"110\", \"d\": \"g\"}]",
        .result = (char *[]){"{\"a\":110,\"d\":\"e\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "equals_number_decimal_fraction_positive_exponent",
        .selector = "$[?@.a==1.1e+2]",
        .alt_selector = "$[?(@.a==110)]",
        .document = "[{\"a\": 110, \"d\": \"e\"}, {\"a\":110.1, \"d\": \"f\"}, {\"a\":\"110\", \"d\": \"g\"}]",
        .result = (char *[]){"{\"a\":110,\"d\":\"e\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "equals_number_decimal_fraction_negative_exponent",
        .selector = "$[?@.a==1.1e-2]",
        .alt_selector = "$[?(@.a==0.011)]",
        .document = "[{\"a\": 0.011, \"d\": \"e\"}, {\"a\":0.012, \"d\": \"f\"}, {\"a\":\"0.011\", \"d\": \"g\"}]",
        .result = (char *[]){"{\"a\":0.011,\"d\":\"e\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[0,3]",
        .document = "[\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\"]",
        .result = (char *[]){"\"a\"","\"d\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[0:2,5]",
        .alt_selector = "$[:2,5]",
        .document = "[\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\"]",
        .result = (char *[]){"\"a\"","\"b\"","\"f\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[0,0]",
        .document = "[\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\"]",
        .result = (char *[]){"\"a\"","\"a\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[1]",
        .document = "[\"a\",\"b\"]",
        .result = (char *[]){"\"b\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[-2]",
        .document = "[\"a\",\"b\"]",
        .result = (char *[]){"\"a\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[1:3]",
        .document = "[\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\"]",
        .result = (char *[]){"\"b\"","\"c\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[5:]",
        .document = "[\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\"]",
        .result = (char *[]){"\"f\"","\"g\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[1:5:2]",
        .document = "[\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\"]",
        .result = (char *[]){"\"b\"","\"d\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[5:1:-2]",
        .document = "[\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\"]",
        .result = (char *[]){"\"f\"","\"d\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[::-1]",
        .document = "[\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\"]",
        .result = (char *[]){"\"g\"","\"f\"","\"e\"","\"d\"","\"c\"","\"b\"","\"a\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$",
        .document = "{\"k\":\"v\"}",
        .result = (char *[]){"{\"k\":\"v\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[*]",
        .document = "{\"o\":{\"j\": 1,\"k\": 2},\"a\":[5,3]}",
        .result = (char *[]){"{\"j\":1,\"k\":2}","[5,3]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.*",
        .document = "{\"o\":{\"j\": 1,\"k\": 2},\"a\":[5,3]}",
        .result = (char *[]){"{\"j\":1,\"k\":2}","[5,3]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.o[*]",
        .document = "{\"o\":{\"j\": 1,\"k\": 2},\"a\":[5,3]}",
        .result = (char *[]){"1","2", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.o.*",
        .document = "{\"o\":{\"j\": 1,\"k\": 2},\"a\":[5,3]}",
        .result = (char *[]){"1","2", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.o[*,*]",
        .document = "{\"o\":{\"j\": 1,\"k\": 2},\"a\":[5,3]}",
        .result = (char *[]){"1","2","1","2", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[*]",
        .document = "{\"o\":{\"j\": 1,\"k\": 2},\"a\":[5,3]}",
        .result = (char *[]){"5","3", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[1]",
        .document = "[\"a\",\"b\"]",
        .result = (char *[]){"\"b\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[-2]",
        .document = "[\"a\",\"b\"]",
        .result = (char *[]){"\"a\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[?@.b == 'kilo']",
        .alt_selector = "$.a[?(@.b==\"kilo\")]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"{\"b\":\"kilo\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[?(@.b == 'kilo')]",
        .alt_selector = "$.a[?(@.b==\"kilo\")]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"{\"b\":\"kilo\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[?@>3.5]",
        .alt_selector = "$.a[?(@>3.5)]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"5","4","6", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[?@.b]",
        .alt_selector = "$.a[?(@.b)]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"{\"b\":\"j\"}","{\"b\":\"k\"}","{\"b\":{}}","{\"b\":\"kilo\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[?@.*]",
        .alt_selector = "$[?(@.*)]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}]","{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[?@[?@.b]]",
        .alt_selector = "$[?(@[?(@.b)])]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.o[?@<3, ?@<3]",
        .alt_selector = "$.o[?(@<3),?(@<3)]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"1","2","1","2", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[?@<2 || @.b == \"k\"]",
        .alt_selector = "$.a[?(@<2 || @.b==\"k\")]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"1","{\"b\":\"k\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[?search(@.b,\"[jk]\")]",
        .alt_selector = "$.a[?(search(@.b,\"[jk]\"))]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"{\"b\":\"j\"}","{\"b\":\"k\"}","{\"b\":\"kilo\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[?match(@.b,\"[jk]\")]",
        .alt_selector = "$.a[?(match(@.b,\"[jk]\"))]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"{\"b\":\"j\"}","{\"b\":\"k\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.o[?@>1 && @<4]",
        .alt_selector = "$.o[?(@>1 && @<4)]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"2","3", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.o[?@.u || @.x]",
        .alt_selector = "$.o[?(@.u || @.x)]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"{\"u\":6}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[?@.b == $.x]",
        .alt_selector = "$.a[?(@.b==$.x)]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"3","5","1","2","4","6", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[?@ == @]",
        .alt_selector = "$.a[?(@==@)]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"3","5","1","2","4","6","{\"b\":\"j\"}","{\"b\":\"k\"}","{\"b\":{}}","{\"b\":\"kilo\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a",
        .document = "{\"a\":null,\"b\":[null],\"c\":[{}],\"null\":1}",
        .result = (char *[]){"null", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[0]",
        .document = "{\"a\":null,\"b\":[null],\"c\":[{}],\"null\":1}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = NULL,
        .selector = "$.a.d",
        .document = "{\"a\":null,\"b\":[null],\"c\":[{}],\"null\":1}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = NULL,
        .selector = "$.b[0]",
        .document = "{\"a\":null,\"b\":[null],\"c\":[{}],\"null\":1}",
        .result = (char *[]){"null", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.b[*]",
        .document = "{\"a\":null,\"b\":[null],\"c\":[{}],\"null\":1}",
        .result = (char *[]){"null", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.b[?@]",
        .alt_selector = "$.b[?(@)]",
        .document = "{\"a\":null,\"b\":[null],\"c\":[{}],\"null\":1}",
        .result = (char *[]){"null", NULL},
    },
    {
        .id = NULL,
        .selector = "$.b[?@==null]",
        .alt_selector = "$.b[?(@==null)]",
        .document = "{\"a\":null,\"b\":[null],\"c\":[{}],\"null\":1}",
        .result = (char *[]){"null", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.c[?@.d==null]",
        .alt_selector = "$.c[?(@.d==null)]",
        .document = "{\"a\":null,\"b\":[null],\"c\":[{}],\"null\":1}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = NULL,
        .selector = "$.null",
        .document = "{\"a\":null,\"b\":[null],\"c\":[{}],\"null\":1}",
        .result = (char *[]){"1", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.absent1 == $.absent2",
        .alt_selector = "($.absent1==$.absent2)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.absent1 <= $.absent2",
        .alt_selector = "($.absent1<=$.absent2)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.absent == 'g'",
        .alt_selector = "($.absent==\"g\")",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.absent1 != $.absent2",
        .alt_selector = "($.absent1!=$.absent2)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.absent != 'g'",
        .alt_selector = "($.absent!=\"g\")",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "1 <= 2",
        .alt_selector = "(1<=2)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "1 > 2",
        .alt_selector = "(1>2)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "13 == '13'",
        .alt_selector = "(13==\"13\")",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "'a' <= 'b'",
        .alt_selector = "(\"a\"<=\"b\")",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "'a' > 'b'",
        .alt_selector = "(\"a\">\"b\")",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.obj == $.arr",
        .alt_selector = "($.obj==$.arr)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.obj != $.arr",
        .alt_selector = "($.obj!=$.arr)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.obj == $.obj",
        .alt_selector = "($.obj==$.obj)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.obj != $.obj",
        .alt_selector = "($.obj!=$.obj)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.arr == $.arr",
        .alt_selector = "($.arr==$.arr)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.arr != $.arr",
        .alt_selector = "($.arr!=$.arr)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.obj == 17",
        .alt_selector = "($.obj==17)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.obj != 17",
        .alt_selector = "($.obj!=17)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.obj <= $.arr",
        .alt_selector = "($.obj<=$.arr)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.obj < $.arr",
        .alt_selector = "($.obj<$.arr)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.obj <= $.obj",
        .alt_selector = "($.obj<=$.obj)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.arr <= $.arr",
        .alt_selector = "($.arr<=$.arr)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "1 <= $.arr",
        .alt_selector = "(1<=$.arr)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "1 >= $.arr",
        .alt_selector = "(1>=$.arr)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "1 > $.arr",
        .alt_selector = "(1>$.arr)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "1 < $.arr",
        .alt_selector = "(1<$.arr)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "true <= true",
        .alt_selector = "(true<=true)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "true > true",
        .alt_selector = "(true>true)",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.phoneNumbers[:1].type",
        .document = "{\"firstName\":\"John\",\"lastName\":\"doe\",\"age\":26,\"address\":{\"streetAddress\":\"naist street\",\"city\":\"Nara\",\"postalCode\": \"630-0192\"},\"phoneNumbers\":[{\"type\":\"iPhone\",\"number\":\"0123-4567-8888\"},{\"type\":\"home\",\"number\":\"0123-4567-8910\"}]}",
        .result = (char *[]){"\"iPhone\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.filters.price",
        .document = JSON_DOC,
        .result = (char *[]){"10", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.filters.category",
        .document = JSON_DOC,
        .result = (char *[]){"\"fiction\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.filters",
        .document = JSON_DOC,
        .result = (char *[]){"{\"price\":10,\"category\":\"fiction\",\"no filters\":\"no \\\"filters\\\"\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.filters[\"no filters\"]",
        .alt_selector = "$.filters['no filters']",
        .document = JSON_DOC,
        .result = (char *[]){"\"no \\\"filters\\\"\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.filters['no filters']",
        .document = JSON_DOC,
        .result = (char *[]){"\"no \\\"filters\\\"\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[1].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sword of Honour\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[-1].author",
        .document = JSON_DOC,
        .result = (char *[]){"\"J. R. R. Tolkien\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[0, 2].title",
        .alt_selector = "$.books[0,2].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sayings of the Century\"", "\"Moby Dick\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.tags[:]",
        .document = JSON_DOC,
        .result = (char *[]){"\"a\"","\"b\"","\"c\"","\"d\"","\"e\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.tags[:3]",
        .document = JSON_DOC,
        .result = (char *[]){"\"a\"","\"b\"","\"c\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.tags[1:4]",
        .document = JSON_DOC,
        .result = (char *[]){"\"b\"","\"c\"","\"d\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.tags[-2:]",
        .document = JSON_DOC,
        .result = (char *[]){"\"d\"","\"e\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.tags[:-3]",
        .document = JSON_DOC,
        .result = (char *[]){"\"a\"","\"b\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.tags[2:]",
        .document = JSON_DOC,
        .result = (char *[]){"\"c\"","\"d\"","\"e\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[1]['author', \"title\"]",
        .alt_selector = "$.books[1][author,title]",
        .document = JSON_DOC,
        .result = (char *[]){"\"Evelyn Waugh\"","\"Sword of Honour\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.id == 2 || @.id == 4)].title",
        .alt_selector = "$.books[?(@.id==2 || @.id==4)].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sword of Honour\"","\"The Lord of the Rings\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.id == 4 - 0.4 * 5)].title",
        .alt_selector = "$.books[?(@.id==4-0.4*5)].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sword of Honour\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.id == 2)].title",
        .alt_selector = "$.books[?(@.id==2)].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sword of Honour\"" ,NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(!(@.id == 2))].title",
        .alt_selector = "$.books[?(!(@.id==2))].title",
        .document =  JSON_DOC,
        .result = (char *[]){"\"Sayings of the Century\"","\"Moby Dick\"","\"The Lord of the Rings\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.id != 2)].title",
        .alt_selector = "$.books[?(@.id!=2)].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sayings of the Century\"","\"Moby Dick\"","\"The Lord of the Rings\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.title =~ \" of \")].title",
        .alt_selector = "$.books[?(@.title=~\" of \")].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sayings of the Century\"","\"Sword of Honour\"","\"The Lord of the Rings\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.price > 12.99)].title",
        .alt_selector = "$.books[?(@.price>12.99)].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"The Lord of the Rings\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.author > \"Herman Melville\")].title",
        .alt_selector = "$.books[?(@.author>\"Herman Melville\")].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sayings of the Century\"","\"The Lord of the Rings\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.price > $.filters.price)].title",
        .alt_selector = "$.books[?(@.price>$.filters.price)].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sword of Honour\"","\"The Lord of the Rings\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.category == $.filters.category)].title",
        .alt_selector = "$.books[?(@.category==$.filters.category)].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sword of Honour\"","\"Moby Dick\"","\"The Lord of the Rings\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.category == \"fiction\" && @.price < 10)].title",
        .alt_selector = "$.books[?(@.category==\"fiction\" && @.price<10)].title",
        .document = JSON_DOC,
        .result =  (char *[]){"\"Moby Dick\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.services[?(@.active == true )].servicegroup",
        .alt_selector = "$.services[?(@.active==true)].servicegroup",
        .document = JSON_DOC,
        .result = (char *[]){"1000","1001", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.services[?(@.active == false )].servicegroup",
        .alt_selector = "$.services[?(@.active==false)].servicegroup",
        .document = JSON_DOC,
        .result = (char *[]){"1002", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$..id",
        .alt_selector = "$..['id']",
        .document = JSON_DOC,
        .result = (char *[]){"1", "2", "3", "4", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$..[?(@.id)]",
        .document = JSON_DOC,
        .result = (char *[]){"{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95,\"id\":1}", "{\"category\":\"fiction\",\"author\":\"Evelyn Waugh\",\"title\":\"Sword of Honour\",\"price\":12.99,\"id\":2}", "{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99,\"id\":3}", "{\"category\":\"fiction\",\"author\":\"J. R. R. Tolkien\",\"title\":\"The Lord of the Rings\",\"isbn\":\"0-395-19395-8\",\"price\":22.99,\"id\":4}", NULL} },
    {
        .id = NULL,
        .selector = "$.services..[?(@.price > 50)].description",
        .alt_selector = "$.services..[?(@.price>50)].description",
        .document = JSON_DOC,
        .result = (char *[]){"\"Printing and assembling book in A5 format\"", "\"Rebinding torn book\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.services..price",
        .alt_selector = "$.services..['price']",
        .document = JSON_DOC,
        .result = (char *[]){"5", "154.99", "46", "24.5", "99.49", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "length($.books)",
        .alt_selector = "length($.books)",
        .document = JSON_DOC,
        .result = (char *[]){"4", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "count($.tags[:-3])",
        .alt_selector = "count($.tags[:-3])",
        .document = JSON_DOC,
        .result = (char *[]){"2", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "count($..['id'])",
        .alt_selector = "count($..['id'])",
        .document = JSON_DOC,
        .result = (char *[]){"4", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.id == 2)].title",
        .alt_selector = "$.books[?(@.id==2)].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sword of Honour\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "length($.tags)",
        .alt_selector = "length($.tags)",
        .document = JSON_DOC,
        .result = (char *[]){"5", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "count($.tags)",
        .alt_selector = "count($.tags)",
        .document = JSON_DOC,
        .result = (char *[]){"1", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[*].price",
        .document = JSON_DOC,
        .result = (char *[]){"8.95", "12.99", "8.99", "22.99", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "count($.books[*].price)",
        .alt_selector = "count($.books[*].price)",
        .document = JSON_DOC,
        .result = (char *[]){"4", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "min($.books[*].price)",
        .alt_selector = "min($.books[*].price)",
        .document = JSON_DOC,
        .result = (char *[]){"8.95", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "min($..price)",
        .alt_selector = "min($..['price'])",
        .document = JSON_DOC,
        .result = (char *[]){"5", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "max($..price)",
        .alt_selector = "max($..['price'])",
        .document = JSON_DOC,
        .result = (char *[]){"154.99", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.category == \"fiction\")].price",
        .alt_selector = "$.books[?(@.category==\"fiction\")].price",
        .document = JSON_DOC,
        .result = (char *[]){"12.99","8.99","22.99", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "avg($.books[?(@.category == \"fiction\")].price)",
        .alt_selector = "avg($.books[?(@.category==\"fiction\")].price)",
        .document = JSON_DOC,
        .result = (char *[]){"14.99", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "max($.books[?(@.category == \"fiction\")].price)",
        .alt_selector = "max($.books[?(@.category==\"fiction\")].price)",
        .document = JSON_DOC,
        .result = (char *[]){"22.99", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.category == $.filters.xyz)].title",
        .alt_selector = "$.books[?(@.category==$.filters.xyz)].title",
        .document = JSON_DOC,
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = NULL,
        .selector = "min($[0])",
        .alt_selector = "min($[0])",
        .document = "[[5,4,1,2,4]]",
        .result = (char *[]){"1", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "the authors of all books in the store",
        .selector = "$.store.book[*].author",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"\"Nigel Rees\"","\"Evelyn Waugh\"","\"Herman Melville\"","\"J. R. R. Tolkien\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "all authors",
        .selector = "$..author",
        .alt_selector = "$..['author']",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"\"Nigel Rees\"","\"Evelyn Waugh\"","\"Herman Melville\"","\"J. R. R. Tolkien\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "all things in store, which are some books and a red bicycle",
        .selector = "$.store.*",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"[{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95},{\"category\":\"fiction\",\"author\":\"Evelyn Waugh\",\"title\":\"Sword of Honour\",\"price\":12.99},{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99},{\"category\":\"fiction\",\"author\":\"J. R. R. Tolkien\",\"title\":\"The Lord of the Rings\",\"isbn\":\"0-395-19395-8\",\"price\":22.99}]", "{\"color\":\"red\",\"price\":399}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "the prices of everything in the store",
        .selector = "$.store..price",
        .alt_selector = "$.store..['price']",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"8.95", "12.99", "8.99", "22.99", "399", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "the third book",
        .selector = "$..book[2]",
        .alt_selector = "$..['book'][2]",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "the third book's author",
        .selector = "$..book[2].author",
        .alt_selector = "$..['book'][2].author",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"\"Herman Melville\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "empty result: the third book does not have a publisher member",
        .selector = "$..book[2].publisher",
        .alt_selector = "$..['book'][2].publisher",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "the last book in order",
        .selector = "$..book[-1]",
        .alt_selector = "$..['book'][-1]",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"{\"category\":\"fiction\",\"author\":\"J. R. R. Tolkien\",\"title\":\"The Lord of the Rings\",\"isbn\":\"0-395-19395-8\",\"price\":22.99}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "the first two books",
        .selector = "$..book[0,1]",
        .alt_selector = "$..['book'][0,1]",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95}", "{\"category\":\"fiction\",\"author\":\"Evelyn Waugh\",\"title\":\"Sword of Honour\",\"price\":12.99}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "the first two books",
        .selector = "$..book[:2]",
        .alt_selector = "$..['book'][:2]",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95}","{\"category\":\"fiction\",\"author\":\"Evelyn Waugh\",\"title\":\"Sword of Honour\",\"price\":12.99}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "all books with an ISBN number",
        .selector = "$..book[?@.isbn]",
        .alt_selector = "$..['book'][?(@.isbn)]",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99}","{\"category\":\"fiction\",\"author\":\"J. R. R. Tolkien\",\"title\":\"The Lord of the Rings\",\"isbn\":\"0-395-19395-8\",\"price\":22.99}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "all books cheaper than 10",
        .selector = "$..book[?@.price<10]",
        .alt_selector = "$..['book'][?(@.price<10)]",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95}", "{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "all member values and array elements contained in the input value",
        .selector = "$..*",
        .alt_selector = "$..[*]",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"{\"book\":[{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95},{\"category\":\"fiction\",\"author\":\"Evelyn Waugh\",\"title\":\"Sword of Honour\",\"price\":12.99},{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99},{\"category\":\"fiction\",\"author\":\"J. R. R. Tolkien\",\"title\":\"The Lord of the Rings\",\"isbn\":\"0-395-19395-8\",\"price\":22.99}],\"bicycle\":{\"color\":\"red\",\"price\":399}}", "[{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95},{\"category\":\"fiction\",\"author\":\"Evelyn Waugh\",\"title\":\"Sword of Honour\",\"price\":12.99},{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99},{\"category\":\"fiction\",\"author\":\"J. R. R. Tolkien\",\"title\":\"The Lord of the Rings\",\"isbn\":\"0-395-19395-8\",\"price\":22.99}]", "{\"color\":\"red\",\"price\":399}", "{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95}", "{\"category\":\"fiction\",\"author\":\"Evelyn Waugh\",\"title\":\"Sword of Honour\",\"price\":12.99}", "{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99}", "{\"category\":\"fiction\",\"author\":\"J. R. R. Tolkien\",\"title\":\"The Lord of the Rings\",\"isbn\":\"0-395-19395-8\",\"price\":22.99}", "\"reference\"", "\"Nigel Rees\"", "\"Sayings of the Century\"", "8.95", "\"fiction\"", "\"Evelyn Waugh\"", "\"Sword of Honour\"", "12.99", "\"fiction\"", "\"Herman Melville\"", "\"Moby Dick\"", "\"0-553-21311-3\"", "8.99", "\"fiction\"", "\"J. R. R. Tolkien\"","\"The Lord of the Rings\"", "\"0-395-19395-8\"", "22.99", "\"red\"", "399", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="function double",
        .selector = "double($.string)",
        .document = "{\"string\":\"1.2867\"}",
        .result = (char *[]){"1.2867", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="function floor",
        .selector = "floor($.number)",
        .document = "{\"number\":1.5}",
        .result = (char *[]){"1", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="function ceil",
        .selector = "ceil($.number)",
        .document = "{\"number\":1.5}",
        .result = (char *[]){"2", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="function abs",
        .selector = "abs($.number)",
        .document = "{\"number\":-1.5}",
        .result = (char *[]){"1.5", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    
// RFC 9535
    {
        .id ="basic, root",
        .selector = "$",
        .document = "[\"first\",\"second\"]",
        .result = (char *[]){"[\"first\",\"second\"]",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="basic, no leading whitespace",
        .selector = " $",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="basic, no trailing whitespace",
        .selector = "$ ",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="basic, name shorthand",
        .selector = "$.a",
        .document = "{\"a\":\"A\",\"b\":\"B\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, name shorthand, extended unicode ☺",
        .selector = "$.☺",
        .document = "{\"\\u263a\":\"A\",\"b\":\"B\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, name shorthand, underscore",
        .selector = "$._",
        .document = "{\"_\":\"A\",\"_foo\":\"B\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, name shorthand, symbol",
        .selector = "$.&",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="basic, name shorthand, number",
        .selector = "$.1",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="basic, name shorthand, absent data",
        .selector = "$.c",
        .document = "{\"a\":\"A\",\"b\":\"B\"}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="basic, name shorthand, array data",
        .selector = "$.a",
        .document = "[\"first\",\"second\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="basic, name shorthand, object data, nested",
        .selector = "$.a.b.c",
        .document = "{\"a\":{\"b\":{\"c\":\"C\"}}}",
        .result = (char *[]){"\"C\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, wildcard shorthand, object data",
        .selector = "$.*",
        .document = "{\"a\":\"A\",\"b\":\"B\"}",
        .result = (char *[]){"\"A\"","\"B\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, wildcard shorthand, array data",
        .selector = "$.*",
        .document = "[\"first\",\"second\"]",
        .result = (char *[]){"\"first\"","\"second\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, wildcard selector, array data",
        .selector = "$[*]",
        .document = "[\"first\",\"second\"]",
        .result = (char *[]){"\"first\"","\"second\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, wildcard shorthand, then name shorthand",
        .selector = "$.*.a",
        .document = "{\"x\":{\"a\":\"Ax\",\"b\":\"Bx\"},\"y\":{\"a\":\"Ay\",\"b\":\"By\"}}",
        .result = (char *[]){"\"Ax\"","\"Ay\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, multiple selectors",
        .selector = "$[0,2]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"0","2",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, multiple selectors, space instead of comma",
        .selector = "$[0 2]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="basic, selector, leading comma",
        .selector = "$[,0]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="basic, selector, trailing comma",
        .selector = "$[0,]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="basic, multiple selectors, name and index, array data",
        .selector = "$['a',1]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"1",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, multiple selectors, name and index, object data",
        .selector = "$['a',1]",
        .document = "{\"a\":1,\"b\":2}",
        .result = (char *[]){"1",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, multiple selectors, index and slice",
        .selector = "$[1,5:7]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"1","5","6",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, multiple selectors, index and slice, overlapping",
        .selector = "$[1,0:3]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"1","0","1","2",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, multiple selectors, duplicate index",
        .selector = "$[1,1]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"1","1",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, multiple selectors, wildcard and index",
        .selector = "$[*,1]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"0","1","2","3","4","5","6","7","8","9","1",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, multiple selectors, wildcard and name",
        .selector = "$[*,'a']",
        .document = "{\"a\":\"A\",\"b\":\"B\"}",
        .result = (char *[]){"\"A\"","\"B\"","\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, multiple selectors, wildcard and slice",
        .selector = "$[*,0:2]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"0","1","2","3","4","5","6","7","8","9","0","1",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, multiple selectors, multiple wildcards",
        .selector = "$[*,*]",
        .document = "[0,1,2]",
        .result = (char *[]){"0","1","2","0","1","2",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, empty segment",
        .selector = "$[]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="basic, descendant segment, index",
        .selector = "$..[1]",
        .document = "{\"o\":[0,1,[2,3]]}",
        .result = (char *[]){"1","3",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, descendant segment, name shorthand",
        .selector = "$..a",
        .document = "{\"o\":[{\"a\":\"b\"},{\"a\":\"c\"}]}",
        .result = (char *[]){"\"b\"","\"c\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, descendant segment, wildcard shorthand, array data",
        .selector = "$..*",
        .document = "[0,1]",
        .result = (char *[]){"0","1",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, descendant segment, wildcard selector, array data",
        .selector = "$..[*]",
        .document = "[0,1]",
        .result = (char *[]){"0","1",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, descendant segment, wildcard selector, nested arrays",
        .selector = "$..[*]",
        .document = "[[[1]],[2]]",
        .result = (char *[]){"[[1]]","[2]","[1]","1","2",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, descendant segment, wildcard selector, nested objects",
        .selector = "$..[*]",
        .document = "{\"a\":{\"c\":{\"e\":1}},\"b\":{\"d\":2}}",
        .result = (char *[]){"{\"c\":{\"e\":1}}","{\"d\":2}","{\"e\":1}","1","2",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, descendant segment, wildcard shorthand, object data",
        .selector = "$..*",
        .document = "{\"a\":\"b\"}",
        .result = (char *[]){"\"b\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, descendant segment, wildcard shorthand, nested data",
        .selector = "$..*",
        .document = "{\"o\":[{\"a\":\"b\"}]}",
        .result = (char *[]){"[{\"a\":\"b\"}]","{\"a\":\"b\"}","\"b\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, descendant segment, multiple selectors",
        .selector = "$..['a','d']",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"\"b\"","\"c\"","\"e\"","\"f\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, descendant segment, object traversal, multiple selectors",
        .selector = "$..['a','d']",
        .document = "{\"x\":{\"a\":\"b\",\"d\":\"e\"},\"y\":{\"a\":\"c\",\"d\":\"f\"}}",
        .result = (char *[]){"\"b\"","\"c\"","\"e\"","\"f\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="basic, bald descendant segment",
        .selector = "$..",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="basic, current node identifier without filter selector",
        .selector = "$[@.a]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="basic, root node identifier in brackets without filter selector",
        .selector = "$[$.a]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, existence, without segments",
        .selector = "$[?@]",
        .document = "{\"a\":1,\"b\":null}",
        .result = (char *[]){"1","null",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, existence",
        .selector = "$[?@.a]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, existence, present with null",
        .selector = "$[?@.a]",
        .document = "[{\"a\":null,\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":null,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, absolute existence, without segments",
        .selector = "$[?$]",
        .document = "{\"a\":1,\"b\":null}",
        .result = (char *[]){"1","null",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, absolute existence, with segments",
        .selector = "$[?$.*.a]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}","{\"b\":\"c\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals string, single quotes",
        .selector = "$[?@.a=='b']",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals numeric string, single quotes",
        .selector = "$[?@.a=='1']",
        .document = "[{\"a\":\"1\",\"d\":\"e\"},{\"a\":1,\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"1\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals string, double quotes",
        .selector = "$[?@.a==\"b\"]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals numeric string, double quotes",
        .selector = "$[?@.a==\"1\"]",
        .document = "[{\"a\":\"1\",\"d\":\"e\"},{\"a\":1,\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"1\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number",
        .selector = "$[?@.a==1]",
        .document = "[{\"a\":1,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"},{\"a\":2,\"d\":\"f\"},{\"a\":\"1\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":1,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals null",
        .selector = "$[?@.a==null]",
        .document = "[{\"a\":null,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":null,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals null, absent from data",
        .selector = "$[?@.a==null]",
        .document = "[{\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="filter, equals true",
        .selector = "$[?@.a==true]",
        .document = "[{\"a\":true,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":true,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals false",
        .selector = "$[?@.a==false]",
        .document = "[{\"a\":false,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":false,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals self",
        .selector = "$[?@==@]",
        .document = "[1,null,true,{\"a\":\"b\"},[false]]",
        .result = (char *[]){"1","null","true","{\"a\":\"b\"}","[false]",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, absolute, equals self",
        .selector = "$[?$==$]",
        .document = "[1,null,true,{\"a\":\"b\"},[false]]",
        .result = (char *[]){"1","null","true","{\"a\":\"b\"}","[false]",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals, absent from index selector equals absent from name selector",
        .selector = "$[?@.absent==@.list[9]]",
        .document = "[{\"list\":[1]}]",
        .result = (char *[]){"{\"list\":[1]}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="filter, deep equality, arrays",
        .selector = "$[?@.a==@.b]",
        .document = "[{\"a\":false,\"b\":[1,2]},{\"a\":[[1,[2]]],\"b\":[[1,[2]]]},{\"a\":[[1,[2]]],\"b\":[[[2],1]]},{\"a\":[[1,[2]]],\"b\":[[1,2]]}]",
        .result = (char *[]){"{\"a\":[[1,[2]]],\"b\":[[1,[2]]]}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
#if 0
    {
        .id ="filter, deep equality, objects",
        .selector = "$[?@.a==@.b]",
        .document = "[{\"a\":false,\"b\":{\"x\":1,\"y\":{\"z\":1}}},{\"a\":{\"x\":1,\"y\":{\"z\":1}},\"b\":{\"x\":1,\"y\":{\"z\":1}}},{\"a\":{\"x\":1,\"y\":{\"z\":1}},\"b\":{\"y\":{\"z\":1},\"x\":1}},{\"a\":{\"x\":1,\"y\":{\"z\":1}},\"b\":{\"x\":1}},{\"a\":{\"x\":1,\"y\":{\"z\":1}},\"b\":{\"x\":1,\"y\":{\"z\":2}}}]",
        .result = (char *[]){"{\"a\":{\"x\":1,\"y\":{\"z\":1}},\"b\":{\"x\":1,\"y\":{\"z\":1}}}","{\"a\":{\"x\":1,\"y\":{\"z\":1}},\"b\":{\"x\":1,\"y\":{\"z\":1}}}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id ="filter, not-equals string, single quotes",
        .selector = "$[?@.a!='b']",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"c\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, not-equals numeric string, single quotes",
        .selector = "$[?@.a!='1']",
        .document = "[{\"a\":\"1\",\"d\":\"e\"},{\"a\":1,\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":1,\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, not-equals string, single quotes, different type",
        .selector = "$[?@.a!='b']",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"a\":1,\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":1,\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, not-equals string, double quotes",
        .selector = "$[?@.a!=\"b\"]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"c\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, not-equals numeric string, double quotes",
        .selector = "$[?@.a!=\"1\"]",
        .document = "[{\"a\":\"1\",\"d\":\"e\"},{\"a\":1,\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":1,\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, not-equals string, double quotes, different types",
        .selector = "$[?@.a!=\"b\"]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"a\":1,\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":1,\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, not-equals number",
        .selector = "$[?@.a!=1]",
        .document = "[{\"a\":1,\"d\":\"e\"},{\"a\":2,\"d\":\"f\"},{\"a\":\"1\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":2,\"d\":\"f\"}","{\"a\":\"1\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, not-equals number, different types",
        .selector = "$[?@.a!=1]",
        .document = "[{\"a\":1,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"c\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, not-equals null",
        .selector = "$[?@.a!=null]",
        .document = "[{\"a\":null,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"c\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="filter, not-equals null, absent from data",
        .selector = "$[?@.a!=null]",
        .document = "[{\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"d\":\"e\"}","{\"a\":\"c\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id ="filter, not-equals true",
        .selector = "$[?@.a!=true]",
        .document = "[{\"a\":true,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"c\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, not-equals false",
        .selector = "$[?@.a!=false]",
        .document = "[{\"a\":false,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"c\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, less than string, single quotes",
        .selector = "$[?@.a<'c']",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, less than string, double quotes",
        .selector = "$[?@.a<\"c\"]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, less than number",
        .selector = "$[?@.a<10]",
        .document = "[{\"a\":1,\"d\":\"e\"},{\"a\":10,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"},{\"a\":20,\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":1,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, less than null",
        .selector = "$[?@.a<null]",
        .document = "[{\"a\":null,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="filter, less than true",
        .selector = "$[?@.a<true]",
        .document = "[{\"a\":true,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="filter, less than false",
        .selector = "$[?@.a<false]",
        .document = "[{\"a\":false,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="filter, less than or equal to string, single quotes",
        .selector = "$[?@.a<='c']",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}","{\"a\":\"c\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, less than or equal to string, double quotes",
        .selector = "$[?@.a<=\"c\"]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}","{\"a\":\"c\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, less than or equal to number",
        .selector = "$[?@.a<=10]",
        .document = "[{\"a\":1,\"d\":\"e\"},{\"a\":10,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"},{\"a\":20,\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":1,\"d\":\"e\"}","{\"a\":10,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, less than or equal to null",
        .selector = "$[?@.a<=null]",
        .document = "[{\"a\":null,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":null,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, less than or equal to true",
        .selector = "$[?@.a<=true]",
        .document = "[{\"a\":true,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":true,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, less than or equal to false",
        .selector = "$[?@.a<=false]",
        .document = "[{\"a\":false,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":false,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, greater than string, single quotes",
        .selector = "$[?@.a>'c']",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"},{\"a\":\"d\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"d\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, greater than string, double quotes",
        .selector = "$[?@.a>\"c\"]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"},{\"a\":\"d\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"d\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, greater than number",
        .selector = "$[?@.a>10]",
        .document = "[{\"a\":1,\"d\":\"e\"},{\"a\":10,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"},{\"a\":20,\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":20,\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, greater than null",
        .selector = "$[?@.a>null]",
        .document = "[{\"a\":null,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="filter, greater than true",
        .selector = "$[?@.a>true]",
        .document = "[{\"a\":true,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="filter, greater than false",
        .selector = "$[?@.a>false]",
        .document = "[{\"a\":false,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="filter, greater than or equal to string, single quotes",
        .selector = "$[?@.a>='c']",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"},{\"a\":\"d\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"c\",\"d\":\"f\"}","{\"a\":\"d\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, greater than or equal to string, double quotes",
        .selector = "$[?@.a>=\"c\"]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"},{\"a\":\"d\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"c\",\"d\":\"f\"}","{\"a\":\"d\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, greater than or equal to number",
        .selector = "$[?@.a>=10]",
        .document = "[{\"a\":1,\"d\":\"e\"},{\"a\":10,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"},{\"a\":20,\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":10,\"d\":\"e\"}","{\"a\":20,\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, greater than or equal to null",
        .selector = "$[?@.a>=null]",
        .document = "[{\"a\":null,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":null,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, greater than or equal to true",
        .selector = "$[?@.a>=true]",
        .document = "[{\"a\":true,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":true,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, greater than or equal to false",
        .selector = "$[?@.a>=false]",
        .document = "[{\"a\":false,\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":false,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, exists and not-equals null, absent from data",
        .selector = "$[?@.a&&@.a!=null]",
        .document = "[{\"d\":\"e\"},{\"a\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"c\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, exists and exists, data false",
        .selector = "$[?@.a&&@.b]",
        .document = "[{\"a\":false,\"b\":false},{\"b\":false},{\"c\":false}]",
        .result = (char *[]){"{\"a\":false,\"b\":false}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, exists or exists, data false",
        .selector = "$[?@.a||@.b]",
        .document = "[{\"a\":false,\"b\":false},{\"b\":false},{\"c\":false}]",
        .result = (char *[]){"{\"a\":false,\"b\":false}","{\"b\":false}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, and",
        .selector = "$[?@.a>0&&@.a<10]",
        .document = "[{\"a\":-10,\"d\":\"e\"},{\"a\":5,\"d\":\"f\"},{\"a\":20,\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":5,\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, or",
        .selector = "$[?@.a=='b'||@.a=='d']",
        .document = "[{\"a\":\"a\",\"d\":\"e\"},{\"a\":\"b\",\"d\":\"f\"},{\"a\":\"c\",\"d\":\"f\"},{\"a\":\"d\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"f\"}","{\"a\":\"d\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, not expression",
        .selector = "$[?!(@.a=='b')]",
        .document = "[{\"a\":\"a\",\"d\":\"e\"},{\"a\":\"b\",\"d\":\"f\"},{\"a\":\"d\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"a\",\"d\":\"e\"}","{\"a\":\"d\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, not exists",
        .selector = "$[?!@.a]",
        .document = "[{\"a\":\"a\",\"d\":\"e\"},{\"d\":\"f\"},{\"a\":\"d\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, not exists, data null",
        .selector = "$[?!@.a]",
        .document = "[{\"a\":null,\"d\":\"e\"},{\"d\":\"f\"},{\"a\":\"d\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, non-singular existence, wildcard",
        .selector = "$[?@.*]",
        .document = "[1,[],[2],{},{\"a\":3}]",
        .result = (char *[]){"[2]","{\"a\":3}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, non-singular existence, multiple",
        .selector = "$[?@[0, 0, 'a']]",
        .document = "[1,[],[2],[2,3],{\"a\":3},{\"b\":4},{\"a\":3,\"b\":4}]",
        .result = (char *[]){"[2]","[2,3]","{\"a\":3}","{\"a\":3,\"b\":4}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, non-singular existence, slice",
        .selector = "$[?@[0:2]]",
        .document = "[1,[],[2],[2,3,4],{},{\"a\":3}]",
        .result = (char *[]){"[2]","[2,3,4]",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, non-singular existence, negated",
        .selector = "$[?!@.*]",
        .document = "[1,[],[2],{},{\"a\":3}]",
        .result = (char *[]){"1","[]","{}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0 
    {
        .id ="filter, non-singular query in comparison, slice",
        .selector = "$[?@[0:0]==0]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, non-singular query in comparison, all children",
        .selector = "$[?@[*]==0]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, non-singular query in comparison, descendants",
        .selector = "$[?@..a==0]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, non-singular query in comparison, combined",
        .selector = "$[?@.a[*].a==0]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="filter, nested",
        .selector = "$[?@[?@>1]]",
        .document = "[[0],[0,1],[0,1,2],[42]]",
        .result = (char *[]){"[0,1,2]","[42]",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, name segment on primitive, selects nothing",
        .selector = "$[?@.a == 1]",
        .document = "{\"a\":1}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="filter, name segment on array, selects nothing",
        .selector = "$[?@['0'] == 5]",
        .document = "[[5,6]]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="filter, index segment on object, selects nothing",
        .selector = "$[?@[0] == 5]",
        .document = "[{\"0\":5}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="filter, followed by name selector",
        .selector = "$[?@.a==1].b.x",
        .document = "[{\"a\":1,\"b\":{\"x\":2}}]",
        .result = (char *[]){"2",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, followed by child segment that selects multiple elements",
        .selector = "$[?@.z=='_']['x','y']",
        .document = "[{\"x\":1,\"y\":null,\"z\":\"_\"}]",
        .result = (char *[]){"1","null",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="filter, relative non-singular query, index, equal",
        .selector = "$[?(@[0, 0]==42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, relative non-singular query, index, not equal",
        .selector = "$[?(@[0, 0]!=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, relative non-singular query, index, less-or-equal",
        .selector = "$[?(@[0, 0]<=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, relative non-singular query, name, equal",
        .selector = "$[?(@['a', 'a']==42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, relative non-singular query, name, not equal",
        .selector = "$[?(@['a', 'a']!=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, relative non-singular query, name, less-or-equal",
        .selector = "$[?(@['a', 'a']<=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, relative non-singular query, combined, equal",
        .selector = "$[?(@[0, '0']==42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, relative non-singular query, combined, not equal",
        .selector = "$[?(@[0, '0']!=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, relative non-singular query, combined, less-or-equal",
        .selector = "$[?(@[0, '0']<=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, relative non-singular query, wildcard, equal",
        .selector = "$[?(@.*==42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, relative non-singular query, wildcard, not equal",
        .selector = "$[?(@.*!=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, relative non-singular query, wildcard, less-or-equal",
        .selector = "$[?(@.*<=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, relative non-singular query, slice, equal",
        .selector = "$[?(@[0:0]==42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, relative non-singular query, slice, not equal",
        .selector = "$[?(@[0:0]!=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, relative non-singular query, slice, less-or-equal",
        .selector = "$[?(@[0:0]<=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, absolute non-singular query, index, equal",
        .selector = "$[?($[0, 0]==42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, absolute non-singular query, index, not equal",
        .selector = "$[?($[0, 0]!=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, absolute non-singular query, index, less-or-equal",
        .selector = "$[?($[0, 0]<=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, absolute non-singular query, name, equal",
        .selector = "$[?($['a', 'a']==42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, absolute non-singular query, name, not equal",
        .selector = "$[?($['a', 'a']!=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, absolute non-singular query, name, less-or-equal",
        .selector = "$[?($['a', 'a']<=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, absolute non-singular query, combined, equal",
        .selector = "$[?($[0, '0']==42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, absolute non-singular query, combined, not equal",
        .selector = "$[?($[0, '0']!=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, absolute non-singular query, combined, less-or-equal",
        .selector = "$[?($[0, '0']<=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, absolute non-singular query, wildcard, equal",
        .selector = "$[?($.*==42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, absolute non-singular query, wildcard, not equal",
        .selector = "$[?($.*!=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, absolute non-singular query, wildcard, less-or-equal",
        .selector = "$[?($.*<=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, absolute non-singular query, slice, equal",
        .selector = "$[?($[0:0]==42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, absolute non-singular query, slice, not equal",
        .selector = "$[?($[0:0]!=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, absolute non-singular query, slice, less-or-equal",
        .selector = "$[?($[0:0]<=42)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="filter, multiple selectors",
        .selector = "$[?@.a,?@.b]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}","{\"b\":\"c\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, multiple selectors, comparison",
        .selector = "$[?@.a=='b',?@.b=='x']",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, multiple selectors, overlapping",
        .selector = "$[?@.a,?@.d]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}","{\"a\":\"b\",\"d\":\"e\"}","{\"b\":\"c\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, multiple selectors, filter and index",
        .selector = "$[?@.a,1]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}","{\"b\":\"c\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, multiple selectors, filter and wildcard",
        .selector = "$[?@.a,*]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}","{\"a\":\"b\",\"d\":\"e\"}","{\"b\":\"c\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, multiple selectors, filter and slice",
        .selector = "$[?@.a,1:]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"},{\"g\":\"h\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}","{\"b\":\"c\",\"d\":\"f\"}","{\"g\":\"h\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, multiple selectors, comparison filter, index and slice",
        .selector = "$[1, ?@.a=='b', 1:]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"b\":\"c\",\"d\":\"f\"}","{\"a\":\"b\",\"d\":\"e\"}","{\"b\":\"c\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, zero and negative zero",
        .selector = "$[?@.a==0]",
        .document = "[{\"a\":0,\"d\":\"e\"},{\"a\":0.1,\"d\":\"f\"},{\"a\":\"0\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":0,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, negative zero and zero",
        .selector = "$[?@.a==-0]",
        .document = "[{\"a\":0,\"d\":\"e\"},{\"a\":0.1,\"d\":\"f\"},{\"a\":\"0\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":0,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, with and without decimal fraction",
        .selector = "$[?@.a==1.0]",
        .document = "[{\"a\":1,\"d\":\"e\"},{\"a\":2,\"d\":\"f\"},{\"a\":\"1\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":1,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, exponent",
        .selector = "$[?@.a==1e2]",
        .document = "[{\"a\":100,\"d\":\"e\"},{\"a\":100.1,\"d\":\"f\"},{\"a\":\"100\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":100,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, exponent upper e",
        .selector = "$[?@.a==1E2]",
        .document = "[{\"a\":100,\"d\":\"e\"},{\"a\":100.1,\"d\":\"f\"},{\"a\":\"100\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":100,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, positive exponent",
        .selector = "$[?@.a==1e+2]",
        .document = "[{\"a\":100,\"d\":\"e\"},{\"a\":100.1,\"d\":\"f\"},{\"a\":\"100\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":100,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, negative exponent",
        .selector = "$[?@.a==1e-2]",
        .document = "[{\"a\":0.01,\"d\":\"e\"},{\"a\":0.02,\"d\":\"f\"},{\"a\":\"0.01\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":0.01,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, exponent 0",
        .selector = "$[?@.a==1e0]",
        .document = "[{\"a\":1,\"d\":\"e\"},{\"a\":2,\"d\":\"f\"},{\"a\":\"1\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":1,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, exponent -0",
        .selector = "$[?@.a==1e-0]",
        .document = "[{\"a\":1,\"d\":\"e\"},{\"a\":2,\"d\":\"f\"},{\"a\":\"1\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":1,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, exponent +0",
        .selector = "$[?@.a==1e+0]",
        .document = "[{\"a\":1,\"d\":\"e\"},{\"a\":2,\"d\":\"f\"},{\"a\":\"1\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":1,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, exponent leading -0",
        .selector = "$[?@.a==1e-02]",
        .document = "[{\"a\":0.01,\"d\":\"e\"},{\"a\":0.02,\"d\":\"f\"},{\"a\":\"0.01\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":0.01,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, exponent +00",
        .selector = "$[?@.a==1e+00]",
        .document = "[{\"a\":1,\"d\":\"e\"},{\"a\":2,\"d\":\"f\"},{\"a\":\"1\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":1,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, decimal fraction",
        .selector = "$[?@.a==1.1]",
        .document = "[{\"a\":1.1,\"d\":\"e\"},{\"a\":1,\"d\":\"f\"},{\"a\":\"1.1\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":1.1,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, decimal fraction, trailing 0",
        .selector = "$[?@.a==1.10]",
        .document = "[{\"a\":1.1,\"d\":\"e\"},{\"a\":1,\"d\":\"f\"},{\"a\":\"1.1\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":1.1,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, decimal fraction, exponent",
        .selector = "$[?@.a==1.1e2]",
        .document = "[{\"a\":110,\"d\":\"e\"},{\"a\":110.1,\"d\":\"f\"},{\"a\":\"110\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":110,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, decimal fraction, positive exponent",
        .selector = "$[?@.a==1.1e+2]",
        .document = "[{\"a\":110,\"d\":\"e\"},{\"a\":110.1,\"d\":\"f\"},{\"a\":\"110\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":110,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, equals number, decimal fraction, negative exponent",
        .selector = "$[?@.a==1.1e-2]",
        .document = "[{\"a\":0.011,\"d\":\"e\"},{\"a\":0.012,\"d\":\"f\"},{\"a\":\"0.011\",\"d\":\"g\"}]",
        .result = (char *[]){"{\"a\":0.011,\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="filter, equals number, invalid plus",
        .selector = "$[?@.a==+1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, equals number, invalid minus space",
        .selector = "$[?@.a==- 1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, equals number, invalid double minus",
        .selector = "$[?@.a==--1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, equals number, invalid no int digit",
        .selector = "$[?@.a==.1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, equals number, invalid minus no int digit",
        .selector = "$[?@.a==-.1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="filter, equals number, invalid 00",
        .selector = "$[?@.a==00]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, equals number, invalid leading 0",
        .selector = "$[?@.a==01]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, equals number, invalid no fractional digit",
        .selector = "$[?@.a==1.]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, equals number, invalid middle minus",
        .selector = "$[?@.a==1.-1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, equals number, invalid no fractional digit e",
        .selector = "$[?@.a==1.e1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, equals number, invalid no e digit",
        .selector = "$[?@.a==1e]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, equals number, invalid no e digit minus",
        .selector = "$[?@.a==1e-]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, equals number, invalid double e",
        .selector = "$[?@.a==1eE1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, equals number, invalid e digit double minus",
        .selector = "$[?@.a==1e--1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, equals number, invalid e digit plus minus",
        .selector = "$[?@.a==1e+-1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, equals number, invalid e decimal",
        .selector = "$[?@.a==1e2.3]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, equals number, invalid multi e",
        .selector = "$[?@.a==1e2e3]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#if 0
    {
        .id ="filter, equals, special nothing",
        .selector = "$.values[?length(@.a) == value($..c)]",
        .document = "{\"c\":\"cd\",\"values\":[{\"a\":\"ab\"},{\"c\":\"d\"},{\"a\":null}]}",
        .result = (char *[]){"{\"c\":\"d\"}","{\"a\":null}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id ="filter, equals, empty node list and empty node list",
        .selector = "$[?@.a == @.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"c\":3}]",
        .result = (char *[]){"{\"c\":3}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="filter, equals, empty node list and special nothing",
        .selector = "$[?@.a == length(@.b)]",
        .document = "[{\"a\":1},{\"b\":2},{\"c\":3}]",
        .result = (char *[]){"{\"b\":2}","{\"c\":3}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id ="filter, object data",
        .selector = "$[?@<3]",
        .document = "{\"a\":1,\"b\":2,\"c\":3}",
        .result = (char *[]){"1","2",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="filter, and binds more tightly than or",
        .selector = "$[?@.a || @.b && @.c]",
        .document = "[{\"a\":1},{\"b\":2,\"c\":3},{\"c\":3},{\"b\":2},{\"a\":1,\"b\":2,\"c\":3}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2,\"c\":3}","{\"a\":1,\"b\":2,\"c\":3}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id ="filter, left to right evaluation",
        .selector = "$[?@.a && @.b || @.c]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":1,\"b\":2},{\"a\":1,\"c\":3},{\"b\":1,\"c\":3},{\"c\":3},{\"a\":1,\"b\":2,\"c\":3}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}","{\"a\":1,\"c\":3}","{\"b\":1,\"c\":3}","{\"c\":3}","{\"a\":1,\"b\":2,\"c\":3}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, group terms, left",
        .selector = "$[?(@.a || @.b) && @.c]",
        .document = "[{\"a\":1,\"b\":2},{\"a\":1,\"c\":3},{\"b\":2,\"c\":3},{\"a\":1},{\"b\":2},{\"c\":3},{\"a\":1,\"b\":2,\"c\":3}]",
        .result = (char *[]){"{\"a\":1,\"c\":3}","{\"b\":2,\"c\":3}","{\"a\":1,\"b\":2,\"c\":3}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, group terms, right",
        .selector = "$[?@.a && (@.b || @.c)]",
        .document = "[{\"a\":1},{\"a\":1,\"b\":2},{\"a\":1,\"c\":2},{\"b\":2},{\"c\":2},{\"a\":1,\"b\":2,\"c\":3}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}","{\"a\":1,\"c\":2}","{\"a\":1,\"b\":2,\"c\":3}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, string literal, single quote in double quotes",
        .selector = "$[?@ == \"quoted' literal\"]",
        .document = "[\"quoted' literal\",\"a\",\"quoted\\\\' literal\"]",
        .result = (char *[]){"\"quoted' literal\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, string literal, double quote in single quotes",
        .selector = "$[?@ == 'quoted\" literal']",
        .document = "[\"quoted\\\" literal\",\"a\",\"quoted\\\\\\\" literal\",\"'quoted\\\" literal'\"]",
        .result = (char *[]){"\"quoted\\\" literal\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, string literal, escaped single quote in single quotes",
        .selector = "$[?@ == 'quoted\\' literal']",
        .document = "[\"quoted' literal\",\"a\",\"quoted\\\\' literal\",\"'quoted\\\" literal'\"]",
        .result = (char *[]){"\"quoted' literal\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="filter, string literal, escaped double quote in double quotes",
        .selector = "$[?@ == \"quoted\\\" literal\"]",
        .document = "[\"quoted\\\" literal\",\"a\",\"quoted\\\\\\\" literal\",\"'quoted\\\" literal'\"]",
        .result = (char *[]){"\"quoted\\\" literal\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="filter, literal true must be compared",
        .selector = "$[?true]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, literal false must be compared",
        .selector = "$[?false]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, literal string must be compared",
        .selector = "$[?'abc']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, literal int must be compared",
        .selector = "$[?2]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, literal float must be compared",
        .selector = "$[?2.2]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, literal null must be compared",
        .selector = "$[?null]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, and, literals must be compared",
        .selector = "$[?true && false]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, or, literals must be compared",
        .selector = "$[?true || false]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, and, right hand literal must be compared",
        .selector = "$[?true == false && false]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, or, right hand literal must be compared",
        .selector = "$[?true == false || false]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, and, left hand literal must be compared",
        .selector = "$[?false && true == false]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, or, left hand literal must be compared",
        .selector = "$[?false || true == false]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="filter, true, incorrectly capitalized",
        .selector = "$[?@==True]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, false, incorrectly capitalized",
        .selector = "$[?@==False]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="filter, null, incorrectly capitalized",
        .selector = "$[?@==Null]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="index selector, first element",
        .selector = "$[0]",
        .document = "[\"first\",\"second\"]",
        .result = (char *[]){"\"first\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="index selector, second element",
        .selector = "$[1]",
        .document = "[\"first\",\"second\"]",
        .result = (char *[]){"\"second\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="index selector, out of bound",
        .selector = "$[2]",
        .document = "[\"first\",\"second\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
#if 0
    {
        .id ="index selector, min exact index",
        .selector = "$[-9007199254740991]",
        .document = "[\"first\",\"second\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="index selector, max exact index",
        .selector = "$[9007199254740991]",
        .document = "[\"first\",\"second\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="index selector, min exact index - 1",
        .selector = "$[-9007199254740992]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="index selector, max exact index + 1",
        .selector = "$[9007199254740992]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
#if 0
    {
        .id ="index selector, overflowing index",
        .selector = "$[231584178474632390847141970017375815706539969331281128078915168015826259279872]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="index selector, not actually an index, overflowing index leads into general text",
        .selector = "$[231584178474632390847141970017375815706539969331281128078915168SomeRandomText]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="index selector, negative",
        .selector = "$[-1]",
        .document = "[\"first\",\"second\"]",
        .result = (char *[]){"\"second\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="index selector, more negative",
        .selector = "$[-2]",
        .document = "[\"first\",\"second\"]",
        .result = (char *[]){"\"first\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="index selector, negative out of bound",
        .selector = "$[-3]",
        .document = "[\"first\",\"second\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="index selector, on object",
        .selector = "$[0]",
        .document = "{\"foo\":1}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="index selector, leading 0",
        .selector = "$[01]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="index selector, decimal",
        .selector = "$[1.0]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="index selector, plus",
        .selector = "$[+1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#if 0
    {
        .id ="index selector, minus space",
        .selector = "$[- 1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="index selector, -0",
        .selector = "$[-0]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="index selector, leading -0",
        .selector = "$[-01]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes",
        .selector = "$[\"a\"]",
        .document = "{\"a\":\"A\",\"b\":\"B\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, double quotes, absent data",
        .selector = "$[\"c\"]",
        .document = "{\"a\":\"A\",\"b\":\"B\"}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="name selector, double quotes, array data",
        .selector = "$[\"a\"]",
        .document = "[\"first\",\"second\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="name selector, name, double quotes, contains single quote",
        .selector = "$[\"a'\"]",
        .document = "{\"a'\":\"A\",\"b\":\"B\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, name, double quotes, nested",
        .selector = "$[\"a\"][\"b\"][\"c\"]",
        .document = "{\"a\":{\"b\":{\"c\":\"C\"}}}",
        .result = (char *[]){"\"C\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="name selector, double quotes, embedded U+0000",
        .selector = "$[\" \"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0001",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0002",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0003",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0004",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0005",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0006",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0007",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0008",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0009",
        .selector = "$[\"\t\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+000A",
        .selector = "$[\"\n\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+000B",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+000C",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+000D",
        .selector = "$[\"\r\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+000E",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+000F",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0010",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0011",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0012",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0013",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0014",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0015",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0016",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0017",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0018",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0019",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+001A",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+001B",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+001C",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+001D",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+001E",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+001F",
        .selector = "$[\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded U+0020",
        .selector = "$[\" \"]",
        .document = "{\" \":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, double quotes, embedded U+007F",
        .selector = "$[\"\"]",
        .document = "{\"\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id ="name selector, double quotes, supplementary plane character",
        .selector = "$[\"𝄞\"]",
        .document = "{\"\\ud834\\udd1e\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, double quotes, escaped double quote",
        .selector = "$[\"\\\"\"]",
        .document = "{\"\\\"\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, double quotes, escaped reverse solidus",
        .selector = "$[\"\\\\\"]",
        .document = "{\"\\\\\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, double quotes, escaped solidus",
        .selector = "$[\"\\/\"]",
        .document = "{\"\\/\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, double quotes, escaped backspace",
        .selector = "$[\"\\b\"]",
        .document = "{\"\\b\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, double quotes, escaped form feed",
        .selector = "$[\"\\f\"]",
        .document = "{\"\\f\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, double quotes, escaped line feed",
        .selector = "$[\"\\n\"]",
        .document = "{\"\\n\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, double quotes, escaped carriage return",
        .selector = "$[\"\\r\"]",
        .document = "{\"\\r\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, double quotes, escaped tab",
        .selector = "$[\"\\t\"]",
        .document = "{\"\\t\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, double quotes, escaped ☺, upper case hex",
        .selector = "$[\"\\u263A\"]",
        .document = "{\"\\u263a\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, double quotes, escaped ☺, lower case hex",
        .selector = "$[\"\\u263a\"]",
        .document = "{\"\\u263a\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="name selector, double quotes, surrogate pair 𝄞",
        .selector = "$[\"\\uD834\\uDD1E\"]",
        .document = "{\"\\ud834\\udd1e\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, double quotes, surrogate pair 😀",
        .selector = "$[\"\\uD83D\\uDE00\"]",
        .document = "{\"\\ud83d\\ude00\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, double quotes, before high surrogates",
        .selector = "$[\"\\uD7FF\\uD7FF\"]",
        .document = "{\"\\ud7ff\\ud7ff\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, double quotes, after low surrogates",
        .selector = "$[\"\\uE000\\uE000\"]",
        .document = "{\"\\ue000\\ue000\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
#if 0
    {
        .id ="name selector, double quotes, invalid escaped single quote",
        .selector = "$[\"\\'\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, embedded double quote",
        .selector = "$[\"\"\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, incomplete escape",
        .selector = "$[\"\\\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, escape at end of line",
        .selector = "$[\"\\\n\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, question mark escape",
        .selector = "$[\"\\?\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, bell escape",
        .selector = "$[\"\\a\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, vertical tab escape",
        .selector = "$[\"\\v\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, 0 escape",
        .selector = "$[\"\\0\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, x escape",
        .selector = "$[\"\\x12\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, n escape",
        .selector = "$[\"\\N{LATIN CAPITAL LETTER A}\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, unicode escape no hex",
        .selector = "$[\"\\u\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, unicode escape too few hex",
        .selector = "$[\"\\u123\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, unicode escape upper u",
        .selector = "$[\"\\U1234\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, unicode escape upper u long",
        .selector = "$[\"\\U0010FFFF\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, unicode escape plus",
        .selector = "$[\"\\u+1234\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, unicode escape brackets",
        .selector = "$[\"\\u{1234}\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, unicode escape brackets long",
        .selector = "$[\"\\u{10ffff}\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, single high surrogate",
        .selector = "$[\"\\uD800\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, single low surrogate",
        .selector = "$[\"\\uDC00\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, high high surrogate",
        .selector = "$[\"\\uD800\\uD800\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, low low surrogate",
        .selector = "$[\"\\uDC00\\uDC00\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, surrogate non-surrogate",
        .selector = "$[\"\\uD800\\u1234\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, non-surrogate surrogate",
        .selector = "$[\"\\u1234\\uDC00\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, surrogate supplementary",
        .selector = "$[\"\\uD800𝄞\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, supplementary surrogate",
        .selector = "$[\"𝄞\\uDC00\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, surrogate incomplete low",
        .selector = "$[\"\\uD800\\uDC0\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="name selector, single quotes",
        .selector = "$['a']",
        .document = "{\"a\":\"A\",\"b\":\"B\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, single quotes, absent data",
        .selector = "$['c']",
        .document = "{\"a\":\"A\",\"b\":\"B\"}",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="name selector, single quotes, array data",
        .selector = "$['a']",
        .document = "[\"first\",\"second\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
#if 0
    {
        .id ="name selector, single quotes, embedded U+0000",
        .selector = "$[' ']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0001",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0002",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0003",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0004",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0005",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0006",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0007",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0008",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0009",
        .selector = "$['\t']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+000A",
        .selector = "$['\n']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+000B",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+000C",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+000D",
        .selector = "$['\r']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+000E",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+000F",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0010",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0011",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0012",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0013",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0014",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0015",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0016",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0017",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0018",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+0019",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+001A",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+001B",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+001C",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+001D",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+001E",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, embedded U+001F",
        .selector = "$['']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="name selector, single quotes, embedded U+0020",
        .selector = "$[' ']",
        .document = "{\" \":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, single quotes, escaped single quote",
        .selector = "$['\\'']",
        .document = "{\"'\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, single quotes, escaped reverse solidus",
        .selector = "$['\\\\']",
        .document = "{\"\\\\\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, single quotes, escaped solidus",
        .selector = "$['\\/']",
        .document = "{\"\\/\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, single quotes, escaped backspace",
        .selector = "$['\\b']",
        .document = "{\"\\b\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, single quotes, escaped form feed",
        .selector = "$['\\f']",
        .document = "{\"\\f\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, single quotes, escaped line feed",
        .selector = "$['\\n']",
        .document = "{\"\\n\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, single quotes, escaped carriage return",
        .selector = "$['\\r']",
        .document = "{\"\\r\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, single quotes, escaped tab",
        .selector = "$['\\t']",
        .document = "{\"\\t\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, single quotes, escaped ☺, upper case hex",
        .selector = "$['\\u263A']",
        .document = "{\"\\u263a\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, single quotes, escaped ☺, lower case hex",
        .selector = "$['\\u263a']",
        .document = "{\"\\u263a\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="name selector, single quotes, surrogate pair 𝄞",
        .selector = "$['\\uD834\\uDD1E']",
        .document = "{\"\\ud834\\udd1e\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, single quotes, surrogate pair 😀",
        .selector = "$['\\uD83D\\uDE00']",
        .document = "{\"\\ud83d\\ude00\":\"A\"}",
        .result = (char *[]){"\"A\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, single quotes, invalid escaped double quote",
        .selector = "$['\\\"']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="name selector, single quotes, embedded single quote",
        .selector = "$[''']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, single quotes, incomplete escape",
        .selector = "$['\\']",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="name selector, double quotes, empty",
        .selector = "$[\"\"]",
        .document = "{\"a\":\"A\",\"b\":\"B\",\"\":\"C\"}",
        .result = (char *[]){"\"C\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="name selector, single quotes, empty",
        .selector = "$['']",
        .document = "{\"a\":\"A\",\"b\":\"B\",\"\":\"C\"}",
        .result = (char *[]){"\"C\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, slice selector",
        .selector = "$[1:3]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"1","2",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, slice selector with step",
        .selector = "$[1:6:2]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"1","3","5",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, slice selector with everything omitted, short form",
        .selector = "$[:]",
        .document = "[0,1,2,3]",
        .result = (char *[]){"0","1","2","3",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, slice selector with everything omitted, long form",
        .selector = "$[::]",
        .document = "[0,1,2,3]",
        .result = (char *[]){"0","1","2","3",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, slice selector with start omitted",
        .selector = "$[:2]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"0","1",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, slice selector with start and end omitted",
        .selector = "$[::2]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"0","2","4","6","8",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, negative step with default start and end",
        .selector = "$[::-1]",
        .document = "[0,1,2,3]",
        .result = (char *[]){"3","2","1","0",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, negative step with default start",
        .selector = "$[:0:-1]",
        .document = "[0,1,2,3]",
        .result = (char *[]){"3","2","1",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, negative step with default end",
        .selector = "$[2::-1]",
        .document = "[0,1,2,3]",
        .result = (char *[]){"2","1","0",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, larger negative step",
        .selector = "$[::-2]",
        .document = "[0,1,2,3]",
        .result = (char *[]){"3","1",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, negative range with default step",
        .selector = "$[-1:-3]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="slice selector, negative range with negative step",
        .selector = "$[-1:-3:-1]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"9","8",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, negative range with larger negative step",
        .selector = "$[-1:-6:-2]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"9","7","5",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, larger negative range with larger negative step",
        .selector = "$[-1:-7:-2]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"9","7","5",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, negative from, positive to",
        .selector = "$[-5:7]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"5","6",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, negative from",
        .selector = "$[-2:]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"8","9",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, positive from, negative to",
        .selector = "$[1:-1]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"1","2","3","4","5","6","7","8",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, negative from, positive to, negative step",
        .selector = "$[-1:1:-1]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"9","8","7","6","5","4","3","2",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, positive from, negative to, negative step",
        .selector = "$[7:-5:-1]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"7","6",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, in serial, on nested array",
        .selector = "$[1:3][1:2]",
        .document = "[[\"a\",\"b\",\"c\"],[\"d\",\"e\",\"f\"],[\"g\",\"h\",\"i\"]]",
        .result = (char *[]){"\"e\"","\"h\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, in serial, on flat array",
        .selector = "$[1:3][::]",
        .document = "[0,1,2,3,4,5]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="slice selector, negative from, negative to, positive step",
        .selector = "$[-5:-2]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"5","6","7",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, too many colons",
        .selector = "$[1:2:3:4]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, non-integer array index",
        .selector = "$[1:2:a]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#if 0
    {
        .id ="slice selector, zero step",
        .selector = "$[1:2:0]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
#endif
    {
        .id ="slice selector, empty range",
        .selector = "$[2:2]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="slice selector, slice selector with everything omitted with empty array",
        .selector = "$[:]",
        .document = "[]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="slice selector, negative step with empty array",
        .selector = "$[::-1]",
        .document = "[]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="slice selector, maximal range with positive step",
        .selector = "$[0:10]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"0","1","2","3","4","5","6","7","8","9",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, maximal range with negative step",
        .selector = "$[9:0:-1]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"9","8","7","6","5","4","3","2","1",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, excessively large to value",
        .selector = "$[2:113667776004]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"2","3","4","5","6","7","8","9",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, excessively small from value",
        .selector = "$[-113667776004:1]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"0",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, excessively large from value with negative step",
        .selector = "$[113667776004:0:-1]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"9","8","7","6","5","4","3","2","1",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, excessively small to value with negative step",
        .selector = "$[3:-113667776004:-1]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"3","2","1","0",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, excessively large step",
        .selector = "$[1:10:113667776004]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"1",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="slice selector, excessively small step",
        .selector = "$[-1:-10:-113667776004]",
        .document = "[0,1,2,3,4,5,6,7,8,9]",
        .result = (char *[]){"9",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="slice selector, start, min exact",
        .selector = "$[-9007199254740991::]",
        .document = "[]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="slice selector, start, max exact",
        .selector = "$[9007199254740991::]",
        .document = "[]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="slice selector, start, min exact - 1",
        .selector = "$[-9007199254740992::]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, start, max exact + 1",
        .selector = "$[9007199254740992::]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, end, min exact",
        .selector = "$[:-9007199254740991:]",
        .document = "[]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="slice selector, end, max exact",
        .selector = "$[:9007199254740991:]",
        .document = "[]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="slice selector, end, min exact - 1",
        .selector = "$[:-9007199254740992:]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, end, max exact + 1",
        .selector = "$[:9007199254740992:]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, step, min exact",
        .selector = "$[::-9007199254740991]",
        .document = "[]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="slice selector, step, max exact",
        .selector = "$[::9007199254740991]",
        .document = "[]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="slice selector, step, min exact - 1",
        .selector = "$[::-9007199254740992]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, step, max exact + 1",
        .selector = "$[::9007199254740992]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, overflowing to value",
        .selector = "$[2:231584178474632390847141970017375815706539969331281128078915168015826259279872]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, underflowing from value",
        .selector = "$[-231584178474632390847141970017375815706539969331281128078915168015826259279872:1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, overflowing from value with negative step",
        .selector = "$[231584178474632390847141970017375815706539969331281128078915168015826259279872:0:-1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, underflowing to value with negative step",
        .selector = "$[3:-231584178474632390847141970017375815706539969331281128078915168015826259279872:-1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, overflowing step",
        .selector = "$[1:10:231584178474632390847141970017375815706539969331281128078915168015826259279872]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, underflowing step",
        .selector = "$[-1:-10:-231584178474632390847141970017375815706539969331281128078915168015826259279872]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="slice selector, start, leading 0",
        .selector = "$[01::]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, start, decimal",
        .selector = "$[1.0::]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, start, plus",
        .selector = "$[+1::]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, start, minus space",
        .selector = "$[- 1::]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, start, -0",
        .selector = "$[-0::]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, start, leading -0",
        .selector = "$[-01::]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, end, leading 0",
        .selector = "$[:01:]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, end, decimal",
        .selector = "$[:1.0:]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, end, plus",
        .selector = "$[:+1:]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, end, minus space",
        .selector = "$[:- 1:]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, end, -0",
        .selector = "$[:-0:]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, end, leading -0",
        .selector = "$[:-01:]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, step, leading 0",
        .selector = "$[::01]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, step, decimal",
        .selector = "$[::1.0]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, step, plus",
        .selector = "$[::+1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#if 0
    {
        .id ="slice selector, step, minus space",
        .selector = "$[::- 1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="slice selector, step, -0",
        .selector = "$[::-0]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="slice selector, step, leading -0",
        .selector = "$[::-01]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, count, count function",
        .selector = "$[?count(@..*)>2]",
        .document = "[{\"a\":[1,2,3]},{\"a\":[1],\"d\":\"f\"},{\"a\":1,\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":[1,2,3]}","{\"a\":[1],\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, count, single-node arg",
        .selector = "$[?count(@.a)>1]",
        .document = "[{\"a\":[1,2,3]},{\"a\":[1],\"d\":\"f\"},{\"a\":1,\"d\":\"f\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="functions, count, multiple-selector arg",
        .selector = "$[?count(@['a','d'])>1]",
        .document = "[{\"a\":[1,2,3]},{\"a\":[1],\"d\":\"f\"},{\"a\":1,\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":[1],\"d\":\"f\"}","{\"a\":1,\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="functions, count, non-query arg, number",
        .selector = "$[?count(1)>2]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, count, non-query arg, string",
        .selector = "$[?count('string')>2]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, count, non-query arg, true",
        .selector = "$[?count(true)>2]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, count, non-query arg, false",
        .selector = "$[?count(false)>2]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, count, non-query arg, null",
        .selector = "$[?count(null)>2]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, count, result must be compared",
        .selector = "$[?count(@..*)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, count, no params",
        .selector = "$[?count()==1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, count, too many params",
        .selector = "$[?count(@.a,@.b)==1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="functions, length, string data",
        .selector = "$[?length(@.a)>=2]",
        .document = "[{\"a\":\"ab\"},{\"a\":\"d\"}]",
        .result = (char *[]){"{\"a\":\"ab\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="functions, length, string data, unicode",
        .selector = "$[?length(@)==2]",
        .document = "[\"\\u263a\",\"\\u263a\\u263a\",\"\\u263a\\u263a\\u263a\",\"\\u0436\",\"\\u0436\\u0436\",\"\\u0436\\u0436\\u0436\",\"\\u78e8\",\"\\u963f\\u7f8e\",\"\\u5f62\\u58f0\\u5b57\"]",
        .result = (char *[]){"\"\\u263a\\u263a\"","\"\\u0436\\u0436\"","\"\\u963f\\u7f8e\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id ="functions, length, array data",
        .selector = "$[?length(@.a)>=2]",
        .document = "[{\"a\":[1,2,3]},{\"a\":[1]}]",
        .result = (char *[]){"{\"a\":[1,2,3]}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, length, missing data",
        .selector = "$[?length(@.a)>=2]",
        .document = "[{\"d\":\"f\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="functions, length, number arg",
        .selector = "$[?length(1)>=2]",
        .document = "[{\"d\":\"f\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="functions, length, true arg",
        .selector = "$[?length(true)>=2]",
        .document = "[{\"d\":\"f\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="functions, length, false arg",
        .selector = "$[?length(false)>=2]",
        .document = "[{\"d\":\"f\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="functions, length, null arg",
        .selector = "$[?length(null)>=2]",
        .document = "[{\"d\":\"f\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
#if 0
    {
        .id ="functions, length, result must be compared",
        .selector = "$[?length(@.a)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, length, no params",
        .selector = "$[?length()==1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, length, too many params",
        .selector = "$[?length(@.a,@.b)==1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, length, non-singular query arg",
        .selector = "$[?length(@.*)<3]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, length, arg is a function expression",
        .selector = "$.values[?length(@.a)==length(value($..c))]",
        .document = "{\"c\":\"cd\",\"values\":[{\"a\":\"ab\"},{\"a\":\"d\"}]}",
        .result = (char *[]){"{\"a\":\"ab\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, length, arg is special nothing",
        .selector = "$[?length(value(@.a))>0]",
        .document = "[{\"a\":\"ab\"},{\"c\":\"d\"},{\"a\":null}]",
        .result = (char *[]){"{\"a\":\"ab\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id ="functions, match, found match",
        .selector = "$[?match(@.a, 'a.*')]",
        .document = "[{\"a\":\"ab\"}]",
        .result = (char *[]){"{\"a\":\"ab\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, match, double quotes",
        .selector = "$[?match(@.a, \"a.*\")]",
        .document = "[{\"a\":\"ab\"}]",
        .result = (char *[]){"{\"a\":\"ab\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="functions, match, regex from the document",
        .selector = "$.values[?match(@, $.regex)]",
        .document = "{\"regex\":\"b.?b\",\"values\":[\"abc\",\"bcd\",\"bab\",\"bba\",\"bbab\",\"b\",true,[],{}]}",
        .result = (char *[]){"\"bab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id ="functions, match, don't select match",
        .selector = "$[?!match(@.a, 'a.*')]",
        .document = "[{\"a\":\"ab\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="functions, match, not a match",
        .selector = "$[?match(@.a, 'a.*')]",
        .document = "[{\"a\":\"bc\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="functions, match, select non-match",
        .selector = "$[?!match(@.a, 'a.*')]",
        .document = "[{\"a\":\"bc\"}]",
        .result = (char *[]){"{\"a\":\"bc\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, match, non-string first arg",
        .selector = "$[?match(1, 'a.*')]",
        .document = "[{\"a\":\"bc\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
#if 0
    {
        .id ="functions, match, non-string second arg",
        .selector = "$[?match(@.a, 1)]",
        .document = "[{\"a\":\"bc\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="functions, match, filter, match function, unicode char class, uppercase",
        .selector = "$[?match(@, '\\\\p{Lu}')]",
        .document = "[\"\\u0436\",\"\\u0416\",\"1\",\"\\u0436\\u0416\",true,[],{}]",
        .result = (char *[]){"\"\\u0416\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, match, filter, match function, unicode char class negated, uppercase",
        .selector = "$[?match(@, '\\\\P{Lu}')]",
        .document = "[\"\\u0436\",\"\\u0416\",\"1\",true,[],{}]",
        .result = (char *[]){"\"\\u0436\"","\"1\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, match, filter, match function, unicode, surrogate pair",
        .selector = "$[?match(@, 'a.b')]",
        .document = "[\"a\\ud800\\udd01b\",\"ab\",\"1\",true,[],{}]",
        .result = (char *[]){"\"a\\ud800\\udd01b\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, match, dot matcher on \\u2028",
        .selector = "$[?match(@, '.')]",
        .document = "[\"\\u2028\",\"\\r\",\"\\n\",true,[],{}]",
        .result = (char *[]){"\"\\u2028\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, match, dot matcher on \\u2029",
        .selector = "$[?match(@, '.')]",
        .document = "[\"\\u2029\",\"\\r\",\"\\n\",true,[],{}]",
        .result = (char *[]){"\"\\u2029\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, match, result cannot be compared",
        .selector = "$[?match(@.a, 'a.*')==true]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, match, too few params",
        .selector = "$[?match(@.a)==1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, match, too many params",
        .selector = "$[?match(@.a,@.b,@.c)==1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, match, arg is a function expression",
        .selector = "$.values[?match(@.a, value($..['regex']))]",
        .document = "{\"regex\":\"a.*\",\"values\":[{\"a\":\"ab\"},{\"a\":\"ba\"}]}",
        .result = (char *[]){"{\"a\":\"ab\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id ="functions, match, dot in character class",
        .selector = "$[?match(@, 'a[.b]c')]",
        .document = "[\"abc\",\"a.c\",\"axc\"]",
        .result = (char *[]){"\"abc\"","\"a.c\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, match, escaped dot",
        .selector = "$[?match(@, 'a\\\\.c')]",
        .document = "[\"abc\",\"a.c\",\"axc\"]",
        .result = (char *[]){"\"a.c\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="functions, match, escaped backslash before dot",
        .selector = "$[?match(@, 'a\\\\\\\\.c')]",
        .document = "[\"abc\",\"a.c\",\"axc\",\"a\\\\\\u2028c\"]",
        .result = (char *[]){"\"a\\\\\\u2028c\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, match, escaped left square bracket",
        .selector = "$[?match(@, 'a\\\\[.c')]",
        .document = "[\"abc\",\"a.c\",\"a[\\u2028c\"]",
        .result = (char *[]){"\"a[\\u2028c\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, match, escaped right square bracket",
        .selector = "$[?match(@, 'a[\\\\].]c')]",
        .document = "[\"abc\",\"a.c\",\"a\\u2028c\",\"a]c\"]",
        .result = (char *[]){"\"a.c\"","\"a]c\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id ="functions, match, explicit caret",
        .selector = "$[?match(@, '^ab.*')]",
        .document = "[\"abc\",\"axc\",\"ab\",\"xab\"]",
        .result = (char *[]){"\"abc\"","\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, match, explicit dollar",
        .selector = "$[?match(@, '.*bc$')]",
        .document = "[\"abc\",\"axc\",\"ab\",\"abcx\"]",
        .result = (char *[]){"\"abc\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, search, at the end",
        .selector = "$[?search(@.a, 'a.*')]",
        .document = "[{\"a\":\"the end is ab\"}]",
        .result = (char *[]){"{\"a\":\"the end is ab\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, search, double quotes",
        .selector = "$[?search(@.a, \"a.*\")]",
        .document = "[{\"a\":\"the end is ab\"}]",
        .result = (char *[]){"{\"a\":\"the end is ab\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, search, at the start",
        .selector = "$[?search(@.a, 'a.*')]",
        .document = "[{\"a\":\"ab is at the start\"}]",
        .result = (char *[]){"{\"a\":\"ab is at the start\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, search, in the middle",
        .selector = "$[?search(@.a, 'a.*')]",
        .document = "[{\"a\":\"contains two matches\"}]",
        .result = (char *[]){"{\"a\":\"contains two matches\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="functions, search, regex from the document",
        .selector = "$.values[?search(@, $.regex)]",
        .document = "{\"regex\":\"b.?b\",\"values\":[\"abc\",\"bcd\",\"bab\",\"bba\",\"bbab\",\"b\",true,[],{}]}",
        .result = (char *[]){"\"bab\"","\"bba\"","\"bbab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id ="functions, search, don't select match",
        .selector = "$[?!search(@.a, 'a.*')]",
        .document = "[{\"a\":\"contains two matches\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="functions, search, not a match",
        .selector = "$[?search(@.a, 'a.*')]",
        .document = "[{\"a\":\"bc\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="functions, search, select non-match",
        .selector = "$[?!search(@.a, 'a.*')]",
        .document = "[{\"a\":\"bc\"}]",
        .result = (char *[]){"{\"a\":\"bc\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, search, non-string first arg",
        .selector = "$[?search(1, 'a.*')]",
        .document = "[{\"a\":\"bc\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
#if 0
    {
        .id ="functions, search, non-string second arg",
        .selector = "$[?search(@.a, 1)]",
        .document = "[{\"a\":\"bc\"}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="functions, search, filter, search function, unicode char class, uppercase",
        .selector = "$[?search(@, '\\\\p{Lu}')]",
        .document = "[\"\\u0436\",\"\\u0416\",\"1\",\"\\u0436\\u0416\",true,[],{}]",
        .result = (char *[]){"\"\\u0416\"","\"\\u0436\\u0416\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, search, filter, search function, unicode char class negated, uppercase",
        .selector = "$[?search(@, '\\\\P{Lu}')]",
        .document = "[\"\\u0436\",\"\\u0416\",\"1\",true,[],{}]",
        .result = (char *[]){"\"\\u0436\"","\"1\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, search, filter, search function, unicode, surrogate pair",
        .selector = "$[?search(@, 'a.b')]",
        .document = "[\"a\\ud800\\udd01bc\",\"abc\",\"1\",true,[],{}]",
        .result = (char *[]){"\"a\\ud800\\udd01bc\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, search, dot matcher on \\u2028",
        .selector = "$[?search(@, '.')]",
        .document = "[\"\\u2028\",\"\\r\\u2028\\n\",\"\\r\",\"\\n\",true,[],{}]",
        .result = (char *[]){"\"\\u2028\"","\"\\r\\u2028\\n\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, search, dot matcher on \\u2029",
        .selector = "$[?search(@, '.')]",
        .document = "[\"\\u2029\",\"\\r\\u2029\\n\",\"\\r\",\"\\n\",true,[],{}]",
        .result = (char *[]){"\"\\u2029\"","\"\\r\\u2029\\n\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, search, result cannot be compared",
        .selector = "$[?search(@.a, 'a.*')==true]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, search, too few params",
        .selector = "$[?search(@.a)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="functions, search, too many params",
        .selector = "$[?search(@.a,@.b,@.c)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#if 0
    {
        .id ="functions, search, arg is a function expression",
        .selector = "$.values[?search(@, value($..['regex']))]",
        .document = "{\"regex\":\"b.?b\",\"values\":[\"abc\",\"bcd\",\"bab\",\"bba\",\"bbab\",\"b\",true,[],{}]}",
        .result = (char *[]){"\"bab\"","\"bba\"","\"bbab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id ="functions, search, dot in character class",
        .selector = "$[?search(@, 'a[.b]c')]",
        .document = "[\"x abc y\",\"x a.c y\",\"x axc y\"]",
        .result = (char *[]){"\"x abc y\"","\"x a.c y\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, search, escaped dot",
        .selector = "$[?search(@, 'a\\\\.c')]",
        .document = "[\"x abc y\",\"x a.c y\",\"x axc y\"]",
        .result = (char *[]){"\"x a.c y\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="functions, search, escaped backslash before dot",
        .selector = "$[?search(@, 'a\\\\\\\\.c')]",
        .document = "[\"x abc y\",\"x a.c y\",\"x axc y\",\"x a\\\\\\u2028c y\"]",
        .result = (char *[]){"\"x a\\\\\\u2028c y\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, search, escaped left square bracket",
        .selector = "$[?search(@, 'a\\\\[.c')]",
        .document = "[\"x abc y\",\"x a.c y\",\"x a[\\u2028c y\"]",
        .result = (char *[]){"\"x a[\\u2028c y\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, search, escaped right square bracket",
        .selector = "$[?search(@, 'a[\\\\].]c')]",
        .document = "[\"x abc y\",\"x a.c y\",\"x a\\u2028c y\",\"x a]c y\"]",
        .result = (char *[]){"\"x a.c y\"","\"x a]c y\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="functions, value, single-value nodelist",
        .selector = "$[?value(@.*)==4]",
        .document = "[[4],{\"foo\":4},[5],{\"foo\":5},4]",
        .result = (char *[]){"[4]","{\"foo\":4}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id ="functions, value, multi-value nodelist",
        .selector = "$[?value(@.*)==4]",
        .document = "[[4,4],{\"foo\":4,\"bar\":4}]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id ="functions, value, too few params",
        .selector = "$[?value()==4]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="functions, value, too many params",
        .selector = "$[?value(@.a,@.b)==4]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#if 0
    {
        .id ="functions, value, result must be compared",
        .selector = "$[?value(@.a)]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="functions, value, well-typed",
        .selector = "$[?value(@..color) == \"red\"]",
        .document = "{\"store\":{\"book\":[{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95},{ \"category\":\"fiction\",\"author\":\"Evelyn Waugh\",\"title\":\"Sword of Honour\",\"price\":12.99},{ \"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99},{\"category\":\"fiction\",\"author\":\"J. R. R. Tolkien\",\"title\":\"The Lord of the Rings\",\"isbn\":\"0-395-19395-8\",\"price\":22.99}],\"bicycle\":{\"color\":\"red\",\"price\":399}}}",
        .result = (char *[]){"{\"book\":[{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95},{\"category\":\"fiction\",\"author\":\"Evelyn Waugh\",\"title\":\"Sword of Honour\",\"price\":12.99},{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99},{\"category\":\"fiction\",\"author\":\"J. R. R. Tolkien\",\"title\":\"The Lord of the Rings\",\"isbn\":\"0-395-19395-8\",\"price\":22.99}],\"bicycle\":{\"color\":\"red\",\"price\":399}}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, filter, space between question mark and expression",
        .selector = "$[? @.a]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, filter, newline between question mark and expression",
        .selector = "$[?\n@.a]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, filter, tab between question mark and expression",
        .selector = "$[?\t@.a]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, filter, return between question mark and expression",
        .selector = "$[?\r@.a]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, filter, space between question mark and parenthesized expression",
        .selector = "$[? (@.a)]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, filter, newline between question mark and parenthesized expression",
        .selector = "$[?\n(@.a)]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, filter, tab between question mark and parenthesized expression",
        .selector = "$[?\t(@.a)]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, filter, return between question mark and parenthesized expression",
        .selector = "$[?\r(@.a)]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, filter, space between parenthesized expression and bracket",
        .selector = "$[?(@.a) ]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, filter, newline between parenthesized expression and bracket",
        .selector = "$[?(@.a)\n]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, filter, tab between parenthesized expression and bracket",
        .selector = "$[?(@.a)\t]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, filter, return between parenthesized expression and bracket",
        .selector = "$[?(@.a)\r]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, filter, space between bracket and question mark",
        .selector = "$[ ?@.a]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, filter, newline between bracket and question mark",
        .selector = "$[\n?@.a]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, filter, tab between bracket and question mark",
        .selector = "$[\t?@.a]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, filter, return between bracket and question mark",
        .selector = "$[\r?@.a]",
        .document = "[{\"a\":\"b\",\"d\":\"e\"},{\"b\":\"c\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"b\",\"d\":\"e\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="whitespace, functions, space between function name and parenthesis",
        .selector = "$[?count (@.*)==1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="whitespace, functions, newline between function name and parenthesis",
        .selector = "$[?count\n(@.*)==1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="whitespace, functions, tab between function name and parenthesis",
        .selector = "$[?count\t(@.*)==1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="whitespace, functions, return between function name and parenthesis",
        .selector = "$[?count\r(@.*)==1]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="whitespace, functions, space between parenthesis and arg",
        .selector = "$[?count( @.*)==1]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, newline between parenthesis and arg",
        .selector = "$[?count(\n@.*)==1]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, tab between parenthesis and arg",
        .selector = "$[?count(\t@.*)==1]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, return between parenthesis and arg",
        .selector = "$[?count(\r@.*)==1]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, space between arg and comma",
        .selector = "$[?search(@ ,'[a-z]+')]",
        .document = "[\"foo\",\"123\"]",
        .result = (char *[]){"\"foo\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, newline between arg and comma",
        .selector = "$[?search(@\n,'[a-z]+')]",
        .document = "[\"foo\",\"123\"]",
        .result = (char *[]){"\"foo\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, tab between arg and comma",
        .selector = "$[?search(@\t,'[a-z]+')]",
        .document = "[\"foo\",\"123\"]",
        .result = (char *[]){"\"foo\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, return between arg and comma",
        .selector = "$[?search(@\r,'[a-z]+')]",
        .document = "[\"foo\",\"123\"]",
        .result = (char *[]){"\"foo\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, space between comma and arg",
        .selector = "$[?search(@, '[a-z]+')]",
        .document = "[\"foo\",\"123\"]",
        .result = (char *[]){"\"foo\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, newline between comma and arg",
        .selector = "$[?search(@,\n'[a-z]+')]",
        .document = "[\"foo\",\"123\"]",
        .result = (char *[]){"\"foo\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, tab between comma and arg",
        .selector = "$[?search(@,\t'[a-z]+')]",
        .document = "[\"foo\",\"123\"]",
        .result = (char *[]){"\"foo\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, return between comma and arg",
        .selector = "$[?search(@,\r'[a-z]+')]",
        .document = "[\"foo\",\"123\"]",
        .result = (char *[]){"\"foo\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, space between arg and parenthesis",
        .selector = "$[?count(@.* )==1]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, newline between arg and parenthesis",
        .selector = "$[?count(@.*\n)==1]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, tab between arg and parenthesis",
        .selector = "$[?count(@.*\t)==1]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, return between arg and parenthesis",
        .selector = "$[?count(@.*\r)==1]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, spaces in a relative singular selector",
        .selector = "$[?length(@ .a .b) == 3]",
        .document = "[{\"a\":{\"b\":\"foo\"}},{}]",
        .result = (char *[]){"{\"a\":{\"b\":\"foo\"}}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, newlines in a relative singular selector",
        .selector = "$[?length(@\n.a\n.b) == 3]",
        .document = "[{\"a\":{\"b\":\"foo\"}},{}]",
        .result = (char *[]){"{\"a\":{\"b\":\"foo\"}}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, tabs in a relative singular selector",
        .selector = "$[?length(@\t.a\t.b) == 3]",
        .document = "[{\"a\":{\"b\":\"foo\"}},{}]",
        .result = (char *[]){"{\"a\":{\"b\":\"foo\"}}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, returns in a relative singular selector",
        .selector = "$[?length(@\r.a\r.b) == 3]",
        .document = "[{\"a\":{\"b\":\"foo\"}},{}]",
        .result = (char *[]){"{\"a\":{\"b\":\"foo\"}}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="whitespace, functions, spaces in an absolute singular selector",
        .selector = "$..[?length(@)==length($ [0] .a)]",
        .document = "[{\"a\":\"foo\"},{}]",
        .result = (char *[]){"\"foo\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, newlines in an absolute singular selector",
        .selector = "$..[?length(@)==length($\n[0]\n.a)]",
        .document = "[{\"a\":\"foo\"},{}]",
        .result = (char *[]){"\"foo\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, tabs in an absolute singular selector",
        .selector = "$..[?length(@)==length($\t[0]\t.a)]",
        .document = "[{\"a\":\"foo\"},{}]",
        .result = (char *[]){"\"foo\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, functions, returns in an absolute singular selector",
        .selector = "$..[?length(@)==length($\r[0]\r.a)]",
        .document = "[{\"a\":\"foo\"},{}]",
        .result = (char *[]){"\"foo\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id ="whitespace, operators, space before ||",
        .selector = "$[?@.a ||@.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"c\":3}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline before ||",
        .selector = "$[?@.a\n||@.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"c\":3}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab before ||",
        .selector = "$[?@.a\t||@.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"c\":3}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return before ||",
        .selector = "$[?@.a\r||@.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"c\":3}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space after ||",
        .selector = "$[?@.a|| @.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"c\":3}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline after ||",
        .selector = "$[?@.a||\n@.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"c\":3}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab after ||",
        .selector = "$[?@.a||\t@.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"c\":3}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return after ||",
        .selector = "$[?@.a||\r@.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"c\":3}]",
        .result = (char *[]){"{\"a\":1}","{\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space before &&",
        .selector = "$[?@.a &&@.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline before &&",
        .selector = "$[?@.a\n&&@.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab before &&",
        .selector = "$[?@.a\t&&@.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return before &&",
        .selector = "$[?@.a\r&&@.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space after &&",
        .selector = "$[?@.a&& @.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline after &&",
        .selector = "$[?@.a&& @.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab after &&",
        .selector = "$[?@.a&& @.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return after &&",
        .selector = "$[?@.a&& @.b]",
        .document = "[{\"a\":1},{\"b\":2},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space before ==",
        .selector = "$[?@.a ==@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline before ==",
        .selector = "$[?@.a\n==@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab before ==",
        .selector = "$[?@.a\t==@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return before ==",
        .selector = "$[?@.a\r==@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space after ==",
        .selector = "$[?@.a== @.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline after ==",
        .selector = "$[?@.a==\n@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab after ==",
        .selector = "$[?@.a==\t@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return after ==",
        .selector = "$[?@.a==\r@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space before !=",
        .selector = "$[?@.a !=@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline before !=",
        .selector = "$[?@.a\n!=@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab before !=",
        .selector = "$[?@.a\t!=@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return before !=",
        .selector = "$[?@.a\r!=@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space after !=",
        .selector = "$[?@.a!= @.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline after !=",
        .selector = "$[?@.a!=\n@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab after !=",
        .selector = "$[?@.a!=\t@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return after !=",
        .selector = "$[?@.a!=\r@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space before <",
        .selector = "$[?@.a <@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline before <",
        .selector = "$[?@.a\n<@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab before <",
        .selector = "$[?@.a\t<@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return before <",
        .selector = "$[?@.a\r<@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space after <",
        .selector = "$[?@.a< @.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline after <",
        .selector = "$[?@.a<\n@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab after <",
        .selector = "$[?@.a<\t@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return after <",
        .selector = "$[?@.a<\r@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space before >",
        .selector = "$[?@.b >@.a]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline before >",
        .selector = "$[?@.b\n>@.a]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab before >",
        .selector = "$[?@.b\t>@.a]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return before >",
        .selector = "$[?@.b\r>@.a]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space after >",
        .selector = "$[?@.b> @.a]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline after >",
        .selector = "$[?@.b>\n@.a]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab after >",
        .selector = "$[?@.b>\t@.a]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return after >",
        .selector = "$[?@.b>\r@.a]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2}]",
        .result = (char *[]){"{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space before <=",
        .selector = "$[?@.a <=@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}","{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline before <=",
        .selector = "$[?@.a\n<=@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}","{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab before <=",
        .selector = "$[?@.a\t<=@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}","{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return before <=",
        .selector = "$[?@.a\r<=@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}","{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space after <=",
        .selector = "$[?@.a<= @.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}","{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline after <=",
        .selector = "$[?@.a<=\n@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}","{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab after <=",
        .selector = "$[?@.a<=\t@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}","{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return after <=",
        .selector = "$[?@.a<=\r@.b]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}","{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space before >=",
        .selector = "$[?@.b >=@.a]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}","{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline before >=",
        .selector = "$[?@.b\n>=@.a]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}","{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab before >=",
        .selector = "$[?@.b\t>=@.a]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}","{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return before >=",
        .selector = "$[?@.b\r>=@.a]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}","{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space after >=",
        .selector = "$[?@.b>= @.a]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}","{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline after >=",
        .selector = "$[?@.b>=\n@.a]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}","{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab after >=",
        .selector = "$[?@.b>=\t@.a]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}","{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return after >=",
        .selector = "$[?@.b>=\r@.a]",
        .document = "[{\"a\":1,\"b\":1},{\"a\":1,\"b\":2},{\"a\":2,\"b\":1}]",
        .result = (char *[]){"{\"a\":1,\"b\":1}","{\"a\":1,\"b\":2}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space between logical not and test expression",
        .selector = "$[?! @.a]",
        .document = "[{\"a\":\"a\",\"d\":\"e\"},{\"d\":\"f\"},{\"a\":\"d\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline between logical not and test expression",
        .selector = "$[?!\n@.a]",
        .document = "[{\"a\":\"a\",\"d\":\"e\"},{\"d\":\"f\"},{\"a\":\"d\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab between logical not and test expression",
        .selector = "$[?!\t@.a]",
        .document = "[{\"a\":\"a\",\"d\":\"e\"},{\"d\":\"f\"},{\"a\":\"d\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return between logical not and test expression",
        .selector = "$[?!\r@.a]",
        .document = "[{\"a\":\"a\",\"d\":\"e\"},{\"d\":\"f\"},{\"a\":\"d\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, space between logical not and parenthesized expression",
        .selector = "$[?! (@.a=='b')]",
        .document = "[{\"a\":\"a\",\"d\":\"e\"},{\"a\":\"b\",\"d\":\"f\"},{\"a\":\"d\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"a\",\"d\":\"e\"}","{\"a\":\"d\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, newline between logical not and parenthesized expression",
        .selector = "$[?!\n(@.a=='b')]",
        .document = "[{\"a\":\"a\",\"d\":\"e\"},{\"a\":\"b\",\"d\":\"f\"},{\"a\":\"d\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"a\",\"d\":\"e\"}","{\"a\":\"d\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, tab between logical not and parenthesized expression",
        .selector = "$[?!\t(@.a=='b')]",
        .document = "[{\"a\":\"a\",\"d\":\"e\"},{\"a\":\"b\",\"d\":\"f\"},{\"a\":\"d\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"a\",\"d\":\"e\"}","{\"a\":\"d\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, operators, return between logical not and parenthesized expression",
        .selector = "$[?!\r(@.a=='b')]",
        .document = "[{\"a\":\"a\",\"d\":\"e\"},{\"a\":\"b\",\"d\":\"f\"},{\"a\":\"d\",\"d\":\"f\"}]",
        .result = (char *[]){"{\"a\":\"a\",\"d\":\"e\"}","{\"a\":\"d\",\"d\":\"f\"}",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, space between root and bracket",
        .selector = "$ ['a']",
        .document = "{\"a\":\"ab\"}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, newline between root and bracket",
        .selector = "$\n['a']",
        .document = "{\"a\":\"ab\"}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, tab between root and bracket",
        .selector = "$\t['a']",
        .document = "{\"a\":\"ab\"}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, return between root and bracket",
        .selector = "$\r['a']",
        .document = "{\"a\":\"ab\"}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, space between bracket and bracket",
        .selector = "$['a'] ['b']",
        .document = "{\"a\":{\"b\":\"ab\"}}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, newline between bracket and bracket",
        .selector = "$['a'] \n['b']",
        .document = "{\"a\":{\"b\":\"ab\"}}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, tab between bracket and bracket",
        .selector = "$['a'] \t['b']",
        .document = "{\"a\":{\"b\":\"ab\"}}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, return between bracket and bracket",
        .selector = "$['a'] \r['b']",
        .document = "{\"a\":{\"b\":\"ab\"}}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, space between root and dot",
        .selector = "$ .a",
        .document = "{\"a\":\"ab\"}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, newline between root and dot",
        .selector = "$\n.a",
        .document = "{\"a\":\"ab\"}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, tab between root and dot",
        .selector = "$\t.a",
        .document = "{\"a\":\"ab\"}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, return between root and dot",
        .selector = "$\r.a",
        .document = "{\"a\":\"ab\"}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id ="whitespace, selectors, space between dot and name",
        .selector = "$. a",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="whitespace, selectors, newline between dot and name",
        .selector = "$.\na",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="whitespace, selectors, tab between dot and name",
        .selector = "$.\ta",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="whitespace, selectors, return between dot and name",
        .selector = "$.\ra",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="whitespace, selectors, space between recursive descent and name",
        .selector = "$.. a",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="whitespace, selectors, newline between recursive descent and name",
        .selector = "$..\na",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="whitespace, selectors, tab between recursive descent and name",
        .selector = "$..\ta",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
    {
        .id ="whitespace, selectors, return between recursive descent and name",
        .selector = "$..\ra",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR
    },
#endif
    {
        .id ="whitespace, selectors, space between bracket and selector",
        .selector = "$[ 'a']",
        .document = "{\"a\":\"ab\"}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, newline between bracket and selector",
        .selector = "$[\n'a']",
        .document = "{\"a\":\"ab\"}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, tab between bracket and selector",
        .selector = "$[\t'a']",
        .document = "{\"a\":\"ab\"}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, return between bracket and selector",
        .selector = "$[\r'a']",
        .document = "{\"a\":\"ab\"}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, space between selector and bracket",
        .selector = "$['a' ]",
        .document = "{\"a\":\"ab\"}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, newline between selector and bracket",
        .selector = "$['a'\n]",
        .document = "{\"a\":\"ab\"}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, tab between selector and bracket",
        .selector = "$['a'\t]",
        .document = "{\"a\":\"ab\"}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, return between selector and bracket",
        .selector = "$['a'\r]",
        .document = "{\"a\":\"ab\"}",
        .result = (char *[]){"\"ab\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, space between selector and comma",
        .selector = "$['a' ,'b']",
        .document = "{\"a\":\"ab\",\"b\":\"bc\"}",
        .result = (char *[]){"\"ab\"","\"bc\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, newline between selector and comma",
        .selector = "$['a'\n,'b']",
        .document = "{\"a\":\"ab\",\"b\":\"bc\"}",
        .result = (char *[]){"\"ab\"","\"bc\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, tab between selector and comma",
        .selector = "$['a'\t,'b']",
        .document = "{\"a\":\"ab\",\"b\":\"bc\"}",
        .result = (char *[]){"\"ab\"","\"bc\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, return between selector and comma",
        .selector = "$['a'\r,'b']",
        .document = "{\"a\":\"ab\",\"b\":\"bc\"}",
        .result = (char *[]){"\"ab\"","\"bc\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, space between comma and selector",
        .selector = "$['a', 'b']",
        .document = "{\"a\":\"ab\",\"b\":\"bc\"}",
        .result = (char *[]){"\"ab\"","\"bc\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, newline between comma and selector",
        .selector = "$['a',\n'b']",
        .document = "{\"a\":\"ab\",\"b\":\"bc\"}",
        .result = (char *[]){"\"ab\"","\"bc\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, tab between comma and selector",
        .selector = "$['a',\t'b']",
        .document = "{\"a\":\"ab\",\"b\":\"bc\"}",
        .result = (char *[]){"\"ab\"","\"bc\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, selectors, return between comma and selector",
        .selector = "$['a',\r'b']",
        .document = "{\"a\":\"ab\",\"b\":\"bc\"}",
        .result = (char *[]){"\"ab\"","\"bc\"",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, slice, space between start and colon",
        .selector = "$[1 :5:2]",
        .document = "[1,2,3,4,5,6]",
        .result = (char *[]){"2","4",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, slice, newline between start and colon",
        .selector = "$[1\n:5:2]",
        .document = "[1,2,3,4,5,6]",
        .result = (char *[]){"2","4",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, slice, tab between start and colon",
        .selector = "$[1\t:5:2]",
        .document = "[1,2,3,4,5,6]",
        .result = (char *[]){"2","4",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, slice, return between start and colon",
        .selector = "$[1\r:5:2]",
        .document = "[1,2,3,4,5,6]",
        .result = (char *[]){"2","4",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, slice, space between colon and end",
        .selector = "$[1: 5:2]",
        .document = "[1,2,3,4,5,6]",
        .result = (char *[]){"2","4",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, slice, newline between colon and end",
        .selector = "$[1:\n5:2]",
        .document = "[1,2,3,4,5,6]",
        .result = (char *[]){"2","4",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, slice, tab between colon and end",
        .selector = "$[1:\t5:2]",
        .document = "[1,2,3,4,5,6]",
        .result = (char *[]){"2","4",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, slice, return between colon and end",
        .selector = "$[1:\r5:2]",
        .document = "[1,2,3,4,5,6]",
        .result = (char *[]){"2","4",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, slice, space between end and colon",
        .selector = "$[1:5 :2]",
        .document = "[1,2,3,4,5,6]",
        .result = (char *[]){"2","4",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, slice, newline between end and colon",
        .selector = "$[1:5\n:2]",
        .document = "[1,2,3,4,5,6]",
        .result = (char *[]){"2","4",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, slice, tab between end and colon",
        .selector = "$[1:5\t:2]",
        .document = "[1,2,3,4,5,6]",
        .result = (char *[]){"2","4",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, slice, return between end and colon",
        .selector = "$[1:5\r:2]",
        .document = "[1,2,3,4,5,6]",
        .result = (char *[]){"2","4",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, slice, space between colon and step",
        .selector = "$[1:5: 2]",
        .document = "[1,2,3,4,5,6]",
        .result = (char *[]){"2","4",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, slice, newline between colon and step",
        .selector = "$[1:5:\n2]",
        .document = "[1,2,3,4,5,6]",
        .result = (char *[]){"2","4",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, slice, tab between colon and step",
        .selector = "$[1:5:\t2]",
        .document = "[1,2,3,4,5,6]",
        .result = (char *[]){"2","4",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id ="whitespace, slice, return between colon and step",
        .selector = "$[1:5:\r2]",
        .document = "[1,2,3,4,5,6]",
        .result = (char *[]){"2","4",NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = NULL,
        .document = NULL,
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_OK,
    }
};

#ifdef DEBUG
static char *get_rcode(jsonpath_exec_result_t rcode)
{
    switch(rcode) {
        case JSONPATH_EXEC_RESULT_OK:
            return "OK";
            break;
        case JSONPATH_EXEC_RESULT_NOT_FOUND:
            return "NOT FOUND";
            break;
        case JSONPATH_EXEC_RESULT_ERROR:
            return "ERROR";
            break;
    }
    return "UNKNOW";
}

static void dump_result(strbuf_t *buf, char **expect_result,
                        jsonpath_exec_result_t expect_rcode, xson_value_list_t *vresult,
                        jsonpath_exec_result_t rcode)
{
    if (vresult == NULL) {
        fprintf(stderr, " expect %s [", get_rcode(expect_rcode));
        if (expect_result != NULL) {
            for (int n = 0; expect_result[n] != NULL; n++) {
                if (n != 0)
                    fputc(',', stderr);
                fprintf(stderr, "%s", expect_result[n]);
            }
        }
        fprintf(stderr, "] result %s []\n", get_rcode(rcode));
        return;
    }

    if (vresult->singleton != NULL) {
        fprintf(stderr, " expect %s [", get_rcode(expect_rcode));
        if (expect_result != NULL) {
            for (int n = 0; expect_result[n] != NULL; n++) {
                if (n != 0)
                    fputc(',', stderr);
                fprintf(stderr, "%s", expect_result[n]);
            }
        }
        fprintf(stderr, "] result %s [", get_rcode(rcode));
        strbuf_reset(buf);
        xson_tree_render(vresult->singleton, buf, XSON_RENDER_TYPE_JSON, 0);
        fprintf(stderr, "%s]\n", buf->ptr);
    } else if (vresult->list != NULL) {
        fprintf(stderr, " expect %s [", get_rcode(expect_rcode));
        if (expect_result != NULL) {
            for (int n = 0; expect_result[n] != NULL; n++) {
                if (n != 0)
                    fputc(',', stderr);
                fprintf(stderr, "%s", expect_result[n]);
            }
        }
        fprintf(stderr, "] result %s [", get_rcode(rcode));
        int n = 0;
        jsonpath_list_cell_t *cell;
        foreach(cell, vresult->list) {
            xson_value_t *rval = cell->ptr_value;
            strbuf_reset(buf);
            xson_tree_render(rval, buf, XSON_RENDER_TYPE_JSON, 0);
            if (n != 0)
                fputc(',', stderr);
            fprintf(stderr, "%s", buf->ptr);
            n++;
        }
        fprintf(stderr, "]\n");
    } else {
        fprintf(stderr, " expect %s [", get_rcode(expect_rcode));
        if (expect_result != NULL) {
            for (int n = 0; expect_result[n] != NULL; n++) {
                if (n != 0)
                    fputc(',', stderr);
                fprintf(stderr, "%s", expect_result[n]);
            }
        }
        fprintf(stderr, "] result %s []\n", get_rcode(rcode));
    }
}
#endif

static bool isalldigits(char *str)
{
    while (*str != '\0') {
        if (!isdigit(*str))
            return false;
        str++;
    }

    return true;
}
static int cmp_result(strbuf_t *buf, char **expect_result, xson_value_list_t *vresult)
{
    if (vresult->singleton != NULL) {
        if ((expect_result == NULL) || (expect_result[0] == NULL) )
            return -1;
        if (expect_result[1] != NULL)
            return -1;
        strbuf_reset(buf);
        xson_tree_render(vresult->singleton, buf, XSON_RENDER_TYPE_JSON, 0);
        if (strcmp(expect_result[0], buf->ptr) != 0)
            return -1;
    } else if (vresult->list != NULL) {
        if ((expect_result == NULL) || (expect_result[0] == NULL))
            return -1;

        int n = 0;
        while (expect_result[n] != NULL) n++;

        int len = jsonpath_list_length(vresult->list);

        if (len != n)
            return -1;

        jsonpath_list_cell_t *cell;
        n=0;
        foreach(cell, vresult->list) {
            xson_value_t *rval = cell->ptr_value;
            strbuf_reset(buf);
            xson_tree_render(rval, buf, XSON_RENDER_TYPE_JSON, 0);
            if (strcmp(expect_result[n], buf->ptr) != 0)
                return -1;
            n++;
        }
    } else {
        if (expect_result != NULL)
            return -1;
    }

    return 0;
}

static int escape_selector(strbuf_t *buf, char const *str)
{
    size_t len = strlen(str);

    if (unlikely(strbuf_avail(buf) < len)) {
        if (strbuf_resize(buf, len) != 0)
            return ENOMEM;
    }

    if (unlikely(buf->ptr == NULL))
        return ENOMEM;

    size_t n = 0;
    while (n < len) {
        if (unlikely(strbuf_avail(buf) < 2)) {
          if (strbuf_resize(buf, 2) != 0)
            return ENOMEM;
        }

        unsigned char c = (unsigned char)str[n];
        switch(c) {
        case '\n':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = 'n';
            break;
        case '\r':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = 'r';
            break;
        case '\t':
            buf->ptr[buf->pos++] = '\\';
            buf->ptr[buf->pos++] = 't';
            break;
        default:
            buf->ptr[buf->pos++] = (char)c;
            break;
        }

        n++;
    }

    buf->ptr[buf->pos] = '\0';
    return 0;
}

static bool jsonpath_test(char *id, char *selector, char *alt_selector,
                          char *document, char **expect_result,
                          jsonpath_exec_result_t expect_rcode)
{
    (void)id;
    strbuf_t buf = {0};
    strbuf_t sbuf = {0};
    char error[256] = {'\0'};

    escape_selector(&sbuf, selector);

    jsonpath_item_t *expr = jsonpath_parser(selector, error, sizeof(error));
    if (expr == NULL) {
        if (expect_rcode == JSONPATH_EXEC_RESULT_ERROR) {
#ifdef DEBUG
            fprintf(stderr, "OK    %s %s error parsing: %s\n", id, sbuf.ptr, error);
#endif
            strbuf_destroy(&sbuf);
            return true;
        }
#ifdef DEBUG
        fprintf(stderr, "FAIL  %s %s error parsing: %s\n", id, sbuf.ptr, error);
        dump_result(&buf, expect_result, expect_rcode, NULL, JSONPATH_EXEC_RESULT_ERROR);
#endif
        strbuf_destroy(&buf);
        strbuf_destroy(&sbuf);
        return false;
    }

#if 0
    strbuf_reset(&buf);
    jsonpath_print_item(&buf, expr, false, false, true);
    if (strcmp(selector, buf.ptr) != 0) {
        if (alt_selector != NULL) {
            if (strcmp(alt_selector, buf.ptr) != 0) {
#ifdef DEBUG
                fprintf(stderr, "FAIL  %s %s != %s", id, sbuf.ptr, buf.ptr);
                dump_result(&buf, expect_result, expect_rcode, NULL, JSONPATH_EXEC_RESULT_ERROR);
#endif
                strbuf_destroy(&buf);
                strbuf_destroy(&sbuf);
                return false;
            }
        } else {
#ifdef DEBUG
            fprintf(stderr, "FAIL  %s %s != %s", id, selector, buf.ptr);
            dump_result(&buf, expect_result, expect_rcode, NULL, JSONPATH_EXEC_RESULT_ERROR);
#endif
            strbuf_destroy(&buf);
            strbuf_destroy(&sbuf);
            return false;
        }
    }
#else
    (void)alt_selector;
#endif

    if (document == NULL) {
#ifdef DEBUG
        fprintf(stderr, "Missing cocument  %s %s\n", id, sbuf.ptr);
#endif
        strbuf_destroy(&buf);
        strbuf_destroy(&sbuf);
        return false;
    }

    xson_value_t *v = xson_tree_parser(document, error, sizeof(error));
    if (v == NULL) {
#ifdef DEBUG
        fprintf(stderr, "ERROR %s %s error parsing: %s\n", id, sbuf.ptr, error);
#endif
        return false;
    }

    xson_value_list_t vresult = {0};
    jsonpath_exec_result_t eresult = jsonpath_exec(expr, v, &vresult, NULL, 0);

    if (expect_rcode == eresult) {
        if (expect_rcode == JSONPATH_EXEC_RESULT_ERROR) {
#ifdef DEBUG
            fprintf(stderr, "OK    %s %s\n", id, sbuf.ptr);
#endif
            jsonpath_item_free(expr);
            xson_value_free(v);
            xson_value_list_destroy(&vresult);
            strbuf_destroy(&buf);
            strbuf_destroy(&sbuf);
            return true;
        }
        if (expect_rcode == JSONPATH_EXEC_RESULT_NOT_FOUND) {
#ifdef DEBUG
            fprintf(stderr, "OK    %s %s\n", id, sbuf.ptr);
#endif
            jsonpath_item_free(expr);
            xson_value_free(v);
            xson_value_list_destroy(&vresult);
            strbuf_destroy(&buf);
            strbuf_destroy(&sbuf);
            return true;
        }
        if (cmp_result(&buf, expect_result, &vresult) == 0) {
#ifdef DEBUG
            fprintf(stderr, "OK    %s %s\n", id, sbuf.ptr);
#endif
            jsonpath_item_free(expr);
            xson_value_free(v);
            xson_value_list_destroy(&vresult);
            strbuf_destroy(&buf);
            strbuf_destroy(&sbuf);
            return true;
        }
    }
#ifdef DEBUG
    fprintf(stderr, "FAIL  %s %s", id, sbuf.ptr);
    dump_result(&buf, expect_result, expect_rcode, &vresult, eresult);
#endif
    jsonpath_item_free(expr);
    xson_value_free(v);
    xson_value_list_destroy(&vresult);
    strbuf_destroy(&buf);
    strbuf_destroy(&sbuf);
    return false;
}

DEF_TEST(parser)
{
    for (size_t i = 0 ; test[i].selector != NULL; i++) {
        if (g_argc > 1) {
            bool found = false;
            for (int n = 1; n < g_argc; n++) {
                if (isalldigits(g_argv[n])) {
                    if ((i+1) == (size_t)atoi(g_argv[n])) {
                        found = true;
                        break;
                    }
                } else if (test[i].id != NULL) {
                    if (strcmp(test[i].id, g_argv[n]) == 0) {
                        found = true;
                        break;
                    }
                }
            }
            if (!found)
                continue;
        }

        bool result = jsonpath_test(test[i].id, test[i].selector, test[i].alt_selector,
                                    test[i].document, test[i].result, test[i].rcode);

        char buffer[256];
        strbuf_t sbuf = {0};
        escape_selector(&sbuf, test[i].selector);
        if (test[i].id == NULL)
            snprintf(buffer, sizeof(buffer), "%s", sbuf.ptr);
        else
            snprintf(buffer, sizeof(buffer), "%s: %s", test[i].id, sbuf.ptr);
        strbuf_destroy(&sbuf);

        EXPECT_EQ_INT_STR(1, result, buffer);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    g_argc = argc;
    g_argv = argv;

    RUN_TEST(parser);

    END_TEST;
}
