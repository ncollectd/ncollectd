// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>

#include "libutils/strbuf.h"
#include "libxson/tree.h"
#include "jsonpath.h"

#include "libtest/testing.h"

// https://cburgmer.github.io/json-path-comparison/
// https://github.com/cburgmer/json-path-comparison/tree/master
// https://github.com/cburgmer/json-path-comparison/blob/master/regression_suite/regression_suite.yaml

#define JSON_DOC "{\"books\":[{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95,\"id\":1},{\"category\":\"fiction\",\"author\":\"Evelyn Waugh\",\"title\":\"Sword of Honour\",\"price\":12.99,\"id\":2},{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99,\"id\":3},{\"category\":\"fiction\",\"author\":\"J. R. R. Tolkien\",\"title\":\"The Lord of the Rings\",\"isbn\":\"0-395-19395-8\",\"price\":22.99,\"id\":4}],\"services\":{\"delivery\":{\"servicegroup\":1000,\"description\":\"Next day delivery in local town\",\"active\":true,\"price\":5},\"bookbinding\":{\"servicegroup\":1001,\"description\":\"Printing and assembling book in A5 format\",\"active\":true,\"price\":154.99},\"restoration\":{\"servicegroup\":1002,\"description\":\"Various restoration methods\",\"active\":false,\"methods\":[{\"description\":\"Chemical cleaning\",\"price\":46},{\"description\":\"Pressing pages damaged by moisture\",\"price\":24.5},{\"description\":\"Rebinding torn book\",\"price\":99.49}]}},\"filters\":{\"price\":10,\"category\":\"fiction\",\"no filters\":\"no \\\"filters\\\"\"},\"closed message\":\"Store is closed\",\"tags\":[\"a\",\"b\",\"c\",\"d\",\"e\"]}"


struct test {
    char *id;
    char *selector;
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
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"third\"", "\"forth\"", "\"fifth\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_large_number_for_end_and_negative_step",
        .selector = "$[2:-113667776004:-1]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"third\"", "\"second\"", "\"first\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_large_number_for_start",
        .selector = "$[-113667776004:2]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"first\"", "\"second\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_large_number_for_start_end_negative_step",
        .selector = "$[113667776004:2:-1]",
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
        .document = "[\"first\", \"second\"]",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "array_slice_with_range_of_1",
        .selector = "$[0:1]",
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
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"first\"", "\"third\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_step_0",
        .selector = "$[0:3:0]",
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
    {
        .id = "array_slice_with_step_1",
        .selector = "$[0:3:1]",
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
        .document = "[\"first\", \"second\", \"third\", \"forth\", \"fifth\"]",
        .result = (char *[]){"\"first\"", "\"third\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "array_slice_with_step_empty",
        .selector = "$[1:3:]",
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
#if 0
    {
        .id = "current_with_dot_notation",
        .selector = "@.a",
        .document = "{\"a\": 1}",
        .result = NULL,
        .rcode = JSONPATH_EXEC_RESULT_ERROR,
    },
#endif
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
        .document = "[{\"id\": 42, \"name\": \"forty-two\"}, {\"id\": 1, \"name\": \"one\"}]",
        .result = (char *[]){"\"forty-two\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_after_recursive_descent",
        .selector = "$..key",
        .document = "{\"object\": {\"key\": \"value\", \"array\": [{\"key\": \"something\"}, {\"key\": {\"key\": \"russian dolls\"}}]}, \"key\": \"top\"}",
        .result = (char *[]){"\"top\"", "\"value\"", "\"something\"", "{\"key\":\"russian dolls\"}", "\"russian dolls\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_after_recursive_descent_after_dot_notation",
        .selector = "$.store..price",
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
        .document = "{\"key\": \"value\", \"another key\": {\"complex\": \"string\", \"primitives\": [0, 1]}}",
        .result = (char *[]){"\"value\"", "{\"complex\":\"string\",\"primitives\":[0,1]}", "\"string\"", "[0,1]", "0", "1", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_with_wildcard_after_recursive_descent_on_null_value_array",
        .selector = "$..*",
        .document = "[40, null, 42]",
        .result = (char *[]){"40", "null", "42", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "dot_notation_with_wildcard_after_recursive_descent_on_scalar",
        .selector = "$..*",
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
        .document = "[{\"key\": 1}, {\"key\": 3}, {\"key\": \"nice\"}, {\"key\": true}, {\"key\": null}, {\"key\": false}, {\"key\": {}}, {\"key\": []}, {\"key\": -1}, {\"key\": 0}, {\"key\": \"\"}]",
        .result = (char *[]){"{\"key\":1}", "{\"key\":3}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_boolean_or_operator_and_value_true",
        .selector = "$[?(@.key>0 || true)]",
        .document = "[{\"key\": 1}, {\"key\": 3}, {\"key\": \"nice\"}, {\"key\": true}, {\"key\": null}, {\"key\": false}, {\"key\": {}}, {\"key\": []}, {\"key\": -1}, {\"key\": 0}, {\"key\": \"\"}]",
        .result = (char *[]){"{\"key\":1}", "{\"key\":3}", "{\"key\":\"nice\"}", "{\"key\":true}", "{\"key\":null}", "{\"key\":false}", "{\"key\":{}}", "{\"key\":[]}", "{\"key\":-1}", "{\"key\":0}", "{\"key\":\"\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_bracket_notation",
        .selector = "$[?(@['key']==42)]",
        .document = "[{\"key\": 0}, {\"key\": 42}, {\"key\": -1}, {\"key\": 41}, {\"key\": 43}, {\"key\": 42.0001}, {\"key\": 41.9999}, {\"key\": 100}, {\"some\": \"value\"}]",
        .result = (char *[]){"{\"key\":42}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_bracket_notation_and_current_object_literal",
        .selector = "$[?(@['@key']==42)]",
        .document = "[{\"@key\": 0}, {\"@key\": 42}, {\"key\": 42}, {\"@key\": 43}, {\"some\": \"value\"}]",
        .result = (char *[]){"{\"@key\":42}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_bracket_notation_with_-1",
        .selector = "$[?(@[-1]==2)]",
        .document = "[[2, 3], [\"a\"], [0, 2], [2]]",
        .result = (char *[]){"[0,2]", "[2]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_bracket_notation_with_number",
        .selector = "$[?(@[1]=='b')]",
        .document = "[[\"a\", \"b\"], [\"x\", \"y\"]]",
        .result = (char *[]){"[\"a\",\"b\"]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_bracket_notation_with_number_on_object",
        .selector = "$[?(@[1]=='b')]",
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
        .document = "[{\"key\": \"some\"}, {\"key\": \"value\"}]",
        .result = (char *[]){"{\"key\":\"value\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "filter_expression_with_equals_string_with_unicode_character_escape",
        .selector = "$[?(@.key==\"Mot\\u00f6rhead\")]",
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
#if 0
    {
        .id = "filter_expression_with_equals_with_path_and_path",
        .selector = "$[?(@.key1==@.key2)]",
        .document = "[{\"key1\": 10, \"key2\": 10}, {\"key1\": 42, \"key2\": 50}, {\"key1\": 10}, {\"key2\": 10}, {}, {\"key1\": null, \"key2\": null}, {\"key1\": null}, {\"key2\": null}, {\"key1\": 0, \"key2\": 0}, {\"key1\": 0}, {\"key2\": 0}, {\"key1\": -1, \"key2\": -1}, {\"key1\": \"\", \"key2\": \"\"}, {\"key1\": false, \"key2\": false}, {\"key1\": false}, {\"key2\": false}, {\"key1\": true, \"key2\": true}, {\"key1\": [], \"key2\": []}, {\"key1\": {}, \"key2\": {}}, {\"key1\": {\"a\": 1, \"b\": 2}, \"key2\": {\"b\": 2, \"a\": 1}}]",
        .result = (char *[]){"{\"key1\":10,\"key2\":10}", "{}", "{\"key1\":null,\"key2\":null}", "{\"key1\":0,\"key2\":0}", "{\"key1\":-1,\"key2\":-1}", "{\"key1\":\"\",\"key2\":\"\"}", "{\"key1\":false,\"key2\":false}", "{\"key1\":true,\"key2\":true}", "{\"key1\":[],\"key2\":[]}", "{\"key1\":{},\"key2\":{}}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
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
#if 0
    {
        .id = "filter_expression_with_not_equals",
        .selector = "$[?(@.key!=42)]",
        .document = "[{\"key\": 0}, {\"key\": 42}, {\"key\": -1}, {\"key\": 1}, {\"key\": 41}, {\"key\": 43}, {\"key\": 42.0001}, {\"key\": 41.9999}, {\"key\": 100}, {\"key\": \"some\"}, {\"key\": \"42\"}, {\"key\": null}, {\"key\": 420}, {\"key\": \"\"}, {\"key\": {}}, {\"key\": []}, {\"key\": [42]}, {\"key\": {\"key\": 42}}, {\"key\": {\"some\": 42}}, {\"some\": \"value\"}]",
        .result = (char *[]){"{\"key\":0}", "{\"key\":-1}", "{\"key\":1}", "{\"key\":41}", "{\"key\":43}", "{\"key\":42.0001}", "{\"key\":41.9999}", "{\"key\":100}", "{\"key\":\"some\"}", "{\"key\":\"42\"}", "{\"key\":null}", "{\"key\":420}", "{\"key\":\"\"}", "{\"key\":{}}", "{\"key\":[]}", "{\"key\":[42]}", "{\"key\":{\"key\":42}}", "{\"key\":{\"some\":42}}", "{\"some\":\"value\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
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
        .document = "{\"key\": \"value\", \"another\": \"entry\"}",
        .result = (char *[]){"\"value\"", "\"entry\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_keys_after_array_slice",
        .selector = "$[:]['c','d']",
        .document = "[{\"c\": \"cc1\", \"d\": \"dd1\", \"e\": \"ee1\"}, {\"c\": \"cc2\", \"d\": \"dd2\", \"e\": \"ee2\"}]",
        .result = (char *[]){"\"cc1\"", "\"dd1\"", "\"cc2\"", "\"dd2\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_keys_after_bracket_notation",
        .selector = "$[0]['c','d']",
        .document = "[{\"c\": \"cc1\", \"d\": \"dd1\", \"e\": \"ee1\"}, {\"c\": \"cc2\", \"d\": \"dd2\", \"e\": \"ee2\"}]",
        .result = (char *[]){"\"cc1\"", "\"dd1\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_keys_after_dot_notation_with_wildcard",
        .selector = "$.*['c','d']",
        .document = "[{\"c\": \"cc1\", \"d\": \"dd1\", \"e\": \"ee1\"}, {\"c\": \"cc2\", \"d\": \"dd2\", \"e\": \"ee2\"}]",
        .result = (char *[]){"\"cc1\"", "\"dd1\"", "\"cc2\"", "\"dd2\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_keys_after_recursive_descent",
        .selector = "$..['c','d']",
        .document = "[{\"c\": \"cc1\", \"d\": \"dd1\", \"e\": \"ee1\"}, {\"c\": \"cc2\", \"child\": {\"d\": \"dd2\"}}, {\"c\": \"cc3\"}, {\"d\": \"dd4\"}, {\"child\": {\"c\": \"cc5\"}}]",
        .result = (char *[]){"\"cc1\"", "\"cc2\"", "\"cc3\"", "\"cc5\"", "\"dd1\"", "\"dd2\"", "\"dd4\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "union_with_keys_on_object_without_key",
        .selector = "$['missing','key']",
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
        .document = "[{\"a\": 0, \"d\": \"e\"}, {\"a\":0.1, \"d\": \"f\"}, {\"a\":\"0\", \"d\": \"g\"}]",
        .result = (char *[]){"{\"a\":0,\"d\":\"e\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "equals_number_with_and_without_decimal_fraction",
        .selector = "$[?@.a==1.0]",
        .document = "[{\"a\": 1, \"d\": \"e\"}, {\"a\":2, \"d\": \"f\"}, {\"a\":\"1\", \"d\": \"g\"}]",
        .result = (char *[]){"{\"a\":1,\"d\":\"e\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "equals_number_exponent",
        .selector = "$[?@.a==1e2]",
        .document = "[{\"a\": 100, \"d\": \"e\"}, {\"a\":100.1, \"d\": \"f\"}, {\"a\":\"100\", \"d\": \"g\"}]",
        .result = (char *[]){"{\"a\":100,\"d\":\"e\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "equals_number_positive_exponent",
        .selector = "$[?@.a==1e+2]",
        .document = "[{\"a\": 100, \"d\": \"e\"}, {\"a\":100.1, \"d\": \"f\"}, {\"a\":\"100\", \"d\": \"g\"}]",
        .result = (char *[]){"{\"a\":100,\"d\":\"e\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "equals_number_negative_exponent",
        .selector = "$[?@.a==1e-2]",
        .document = "[{\"a\": 0.01, \"d\": \"e\"}, {\"a\":0.02, \"d\": \"f\"}, {\"a\":\"0.01\", \"d\": \"g\"}]",
        .result = (char *[]){"{\"a\":0.01,\"d\":\"e\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "equals_number_decimal_fraction",
        .selector = "$[?@.a==1.1]",
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
        .document = "[{\"a\": 110, \"d\": \"e\"}, {\"a\":110.1, \"d\": \"f\"}, {\"a\":\"110\", \"d\": \"g\"}]",
        .result = (char *[]){"{\"a\":110,\"d\":\"e\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "equals_number_decimal_fraction_positive_exponent",
        .selector = "$[?@.a==1.1e+2]",
        .document = "[{\"a\": 110, \"d\": \"e\"}, {\"a\":110.1, \"d\": \"f\"}, {\"a\":\"110\", \"d\": \"g\"}]",
        .result = (char *[]){"{\"a\":110,\"d\":\"e\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "equals_number_decimal_fraction_negative_exponent",
        .selector = "$[?@.a==1.1e-2]",
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
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"{\"b\":\"kilo\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[?(@.b == 'kilo')]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"{\"b\":\"kilo\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[?@>3.5]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"5","4","6", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[?@.b]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"{\"b\":\"j\"}","{\"b\":\"k\"}","{\"b\":{}}","{\"b\":\"kilo\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[?@.*]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}]","{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$[?@[?@.b]]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}]", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.o[?@<3, ?@<3]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"1","2","1","2", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[?@<2 || @.b == \"k\"]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"1","{\"b\":\"k\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id = NULL,
        .selector = "$.a[?match(@.b,\"[jk]\")]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"{\"b\":\"j\"}","{\"b\":\"k\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[?search(@.b,\"[jk]\")]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"{\"b\":\"j\"}","{\"b\":\"k\"}","{\"b\":\"kilo\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id = NULL,
        .selector = "$.o[?@>1 && @<4]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"2","3", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.o[?@.u || @.x]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"{\"u\":6}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id = NULL,
        .selector = "$.a[?@.b == $.x]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"3","5","1","2","4","6", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.a[?@ == @]",
        .document = "{\"a\":[3,5,1,2,4,6,{\"b\":\"j\"},{\"b\":\"k\"},{\"b\":{}},{\"b\":\"kilo\"}],\"o\":{\"p\":1,\"q\":2,\"r\":3,\"s\":5,\"t\":{\"u\":6}},\"e\":\"f\"}",
        .result = (char *[]){"3","5","1","2","4","6","{\"b\":\"j\"}","{\"b\":\"k\"}","{\"b\":{}}","{\"b\":\"kilo\"}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
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
        .document = "{\"a\":null,\"b\":[null],\"c\":[{}],\"null\":1}",
        .result = (char *[]){"null", NULL},
    },
    {
        .id = NULL,
        .selector = "$.b[?@==null]",
        .document = "{\"a\":null,\"b\":[null],\"c\":[{}],\"null\":1}",
        .result = (char *[]){"null", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.c[?@.d==null]",
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
#if 0
    {
        .id = NULL,
        .selector = "$.absent1 == $.absent2",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.absent1 <= $.absent2",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id = NULL,
        .selector = "$.absent == 'g'",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.absent1 != $.absent2",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id = NULL,
        .selector = "$.absent != 'g'",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id = NULL,
        .selector = "1 <= 2",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "1 > 2",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "13 == '13'",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "'a' <= 'b'",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "'a' > 'b'",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id = NULL,
        .selector = "$.obj == $.arr",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.obj != $.arr",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.obj == $.obj",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id = NULL,
        .selector = "$.obj != $.obj",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id = NULL,
        .selector = "$.arr == $.arr",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id = NULL,
        .selector = "$.arr != $.arr",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id = NULL,
        .selector = "$.obj == 17",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.obj != 17",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id = NULL,
        .selector = "$.obj <= $.arr",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.obj < $.arr",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id = NULL,
        .selector = "$.obj <= $.obj",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.arr <= $.arr",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id = NULL,
        .selector = "1 <= $.arr",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "1 >= $.arr",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "1 > $.arr",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "1 < $.arr",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"false", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "true <= true",
        .document = "{\"obj\":{\"x\":\"y\"},\"arr\":[2,3]}",
        .result = (char *[]){"true", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "true > true",
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
        .document = JSON_DOC,
        .result = (char *[]){"\"Evelyn Waugh\"","\"Sword of Honour\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.id == 2 || @.id == 4)].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sword of Honour\"","\"The Lord of the Rings\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.id == 4 - 0.4 * 5)].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sword of Honour\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.id == 2)].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sword of Honour\"" ,NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(!(@.id == 2))].title",
        .document =  JSON_DOC,
        .result = (char *[]){"\"Sayings of the Century\"","\"Moby Dick\"","\"The Lord of the Rings\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.id != 2)].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sayings of the Century\"","\"Moby Dick\"","\"The Lord of the Rings\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.title =~ \" of \")].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sayings of the Century\"","\"Sword of Honour\"","\"The Lord of the Rings\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.price > 12.99)].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"The Lord of the Rings\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.author > \"Herman Melville\")].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sayings of the Century\"","\"The Lord of the Rings\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.price > $.filters.price)].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sword of Honour\"","\"The Lord of the Rings\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.category == $.filters.category)].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sword of Honour\"","\"Moby Dick\"","\"The Lord of the Rings\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.category == \"fiction\" && @.price < 10)].title",
        .document = JSON_DOC,
        .result =  (char *[]){"\"Moby Dick\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.services[?(@.active == true )].servicegroup",
        .document = JSON_DOC,
        .result = (char *[]){"1000","1001", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.services[?(@.active == false )].servicegroup",
        .document = JSON_DOC,
        .result = (char *[]){"1002", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$..id",
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
        .document = JSON_DOC,
        .result = (char *[]){"\"Printing and assembling book in A5 format\"", "\"Rebinding torn book\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.services..price",
        .document = JSON_DOC,
        .result = (char *[]){"5", "154.99", "46", "24.5", "99.49", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "length($.books)",
        .document = JSON_DOC,
        .result = (char *[]){"4", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "count($.tags[:-3])",
        .document = JSON_DOC,
        .result = (char *[]){"2", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "count($..id)",
        .document = JSON_DOC,
        .result = (char *[]){"4", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.id == 2)].title",
        .document = JSON_DOC,
        .result = (char *[]){"\"Sword of Honour\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "length($.tags)",
        .document = JSON_DOC,
        .result = (char *[]){"5", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "count($.tags)",
        .document = JSON_DOC,
        .result = (char *[]){"1", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#if 0
    {
        .id = NULL,
        .selector = "$.books[*].price",
        .document = JSON_DOC,
        .result = (char *[]){"8.95", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "count($.books[*].price)",
        .document = JSON_DOC,
        .result = (char *[]){"4", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "min($.books[*].price)",
        .document = JSON_DOC,
        .result = (char *[]){"8.95", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "min($..price)",
        .document = JSON_DOC,
        .result = (char *[]){"5", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "max($..price)",
        .document = JSON_DOC,
        .result = (char *[]){"154.99", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "$.books[?(@.category == \"fiction\")].price",
        .document = JSON_DOC,
        .result = (char *[]){"14.99", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "avg($.books[?(@.category == \"fiction\")].price)",
        .document = JSON_DOC,
        .result = (char *[]){"14.99", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = NULL,
        .selector = "max($.books[?(@.category == \"fiction\")].price)",
        .document = JSON_DOC,
        .result = (char *[]){"22.99", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
#endif
    {
        .id = NULL,
        .selector = "$.books[?(@.category == $.filters.xyz)].title",
        .document = JSON_DOC,
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = NULL,
        .selector = "min($[0])",
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
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"8.95", "12.99", "8.99", "22.99", "399", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "the third book",
        .selector = "$..book[2]",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "the third book's author",
        .selector = "$..book[2].author",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"\"Herman Melville\"", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "empty result: the third book does not have a publisher member",
        .selector = "$..book[2].publisher",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){NULL},
        .rcode = JSONPATH_EXEC_RESULT_NOT_FOUND,
    },
    {
        .id = "the last book in order",
        .selector = "$..book[-1]",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"{\"category\":\"fiction\",\"author\":\"J. R. R. Tolkien\",\"title\":\"The Lord of the Rings\",\"isbn\":\"0-395-19395-8\",\"price\":22.99}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "the first two books",
        .selector = "$..book[0,1]",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95}", "{\"category\":\"fiction\",\"author\":\"Evelyn Waugh\",\"title\":\"Sword of Honour\",\"price\":12.99}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "the first two books",
        .selector = "$..book[:2]",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95}","{\"category\":\"fiction\",\"author\":\"Evelyn Waugh\",\"title\":\"Sword of Honour\",\"price\":12.99}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "all books with an ISBN number",
        .selector = "$..book[?@.isbn]",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99}","{\"category\":\"fiction\",\"author\":\"J. R. R. Tolkien\",\"title\":\"The Lord of the Rings\",\"isbn\":\"0-395-19395-8\",\"price\":22.99}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "all books cheaper than 10",
        .selector = "$..book[?@.price<10]",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95}", "{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99}", NULL},
        .rcode = JSONPATH_EXEC_RESULT_OK,
    },
    {
        .id = "all member values and array elements contained in the input value",
        .selector = "$..*",
        .document = "{ \"store\": { \"book\": [ { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 }, { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 }, { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 }, { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 } ], \"bicycle\": { \"color\": \"red\", \"price\": 399 } } }",
        .result = (char *[]){"{\"book\":[{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95},{\"category\":\"fiction\",\"author\":\"Evelyn Waugh\",\"title\":\"Sword of Honour\",\"price\":12.99},{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99},{\"category\":\"fiction\",\"author\":\"J. R. R. Tolkien\",\"title\":\"The Lord of the Rings\",\"isbn\":\"0-395-19395-8\",\"price\":22.99}],\"bicycle\":{\"color\":\"red\",\"price\":399}}", "[{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95},{\"category\":\"fiction\",\"author\":\"Evelyn Waugh\",\"title\":\"Sword of Honour\",\"price\":12.99},{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99},{\"category\":\"fiction\",\"author\":\"J. R. R. Tolkien\",\"title\":\"The Lord of the Rings\",\"isbn\":\"0-395-19395-8\",\"price\":22.99}]", "{\"color\":\"red\",\"price\":399}", "{\"category\":\"reference\",\"author\":\"Nigel Rees\",\"title\":\"Sayings of the Century\",\"price\":8.95}", "{\"category\":\"fiction\",\"author\":\"Evelyn Waugh\",\"title\":\"Sword of Honour\",\"price\":12.99}", "{\"category\":\"fiction\",\"author\":\"Herman Melville\",\"title\":\"Moby Dick\",\"isbn\":\"0-553-21311-3\",\"price\":8.99}", "{\"category\":\"fiction\",\"author\":\"J. R. R. Tolkien\",\"title\":\"The Lord of the Rings\",\"isbn\":\"0-395-19395-8\",\"price\":22.99}", "\"reference\"", "\"Nigel Rees\"", "\"Sayings of the Century\"", "8.95", "\"fiction\"", "\"Evelyn Waugh\"", "\"Sword of Honour\"", "12.99", "\"fiction\"", "\"Herman Melville\"", "\"Moby Dick\"", "\"0-553-21311-3\"", "8.99", "\"fiction\"", "\"J. R. R. Tolkien\"","\"The Lord of the Rings\"", "\"0-395-19395-8\"", "22.99", "\"red\"", "399", NULL},
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
                        jsonpath_exec_result_t expect_rcode, json_value_list_t *vresult,
                        jsonpath_exec_result_t rcode)
{
    if (vresult == NULL) {
        fprintf(stderr, " expect %s [", get_rcode(expect_rcode));
        if (expect_result != NULL) {
            for (int n = 0; expect_result[n] != NULL; n++) {
                fprintf(stderr, "%s,", expect_result[n]);
            }
        }
        fprintf(stderr, "] result %s []\n", get_rcode(rcode));
        return;
    }

    if (vresult->singleton != NULL) {
        fprintf(stderr, " expect %s [", get_rcode(expect_rcode));
        if (expect_result != NULL) {
            for (int n = 0; expect_result[n] != NULL; n++) {
                fprintf(stderr, "%s,", expect_result[n]);
            }
        }
        fprintf(stderr, "] result %s [", get_rcode(rcode));
        strbuf_reset(buf);
        json_tree_render(vresult->singleton, buf, XSON_RENDER_TYPE_JSON, 0);
        fprintf(stderr, "%s]\n", buf->ptr);
    } else if (vresult->list != NULL) {
        fprintf(stderr, " expect %s [", get_rcode(expect_rcode));
        if (expect_result != NULL) {
            for (int n = 0; expect_result[n] != NULL; n++) {
                fprintf(stderr, "%s,", expect_result[n]);
            }
        }
        fprintf(stderr, "] result %s [", get_rcode(rcode));
        jsonpath_list_cell_t *cell;
        foreach(cell, vresult->list) {
            json_value_t *rval = cell->ptr_value;
            strbuf_reset(buf);
            json_tree_render(rval, buf, XSON_RENDER_TYPE_JSON, 0);
            fprintf(stderr, "%s,", buf->ptr);
        }
        fprintf(stderr, "]\n");
    } else {
        fprintf(stderr, " expect %s [", get_rcode(expect_rcode));
        if (expect_result != NULL) {
            for (int n = 0; expect_result[n] != NULL; n++) {
                fprintf(stderr, "%s,", expect_result[n]);
            }
        }
        fprintf(stderr, "] result %s []\n", get_rcode(rcode));
    }
}
#endif

static int cmp_result(strbuf_t *buf, char **expect_result, json_value_list_t *vresult)
{
    if (vresult->singleton != NULL) {
        if ((expect_result == NULL) || (expect_result[0] == NULL) )
            return -1;
        if (expect_result[1] != NULL)
            return -1;
        strbuf_reset(buf);
        json_tree_render(vresult->singleton, buf, XSON_RENDER_TYPE_JSON, 0);
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
            json_value_t *rval = cell->ptr_value;
            strbuf_reset(buf);
            json_tree_render(rval, buf, XSON_RENDER_TYPE_JSON, 0);
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

static bool jsonpath_test(char *id, char *selector, char *document, char **expect_result,
                          jsonpath_exec_result_t expect_rcode)
{
    (void)id;
    strbuf_t buf = {0};
    char error[256] = {'\0'};

    json_value_t *v = json_tree_parser(document, error, sizeof(error));
    if (v == NULL) {
#ifdef DEBUG
        fprintf(stderr, "ERROR %s %s error parsing: %s\n", id, selector, error);
#endif
        return false;
    }

//    strbuf_reset(&buf);
//    json_tree_render(v, &buf, XSON_RENDER_TYPE_JSON, XSON_RENDER_OPTION_JSON_BEAUTIFY);
//    fprintf(stderr, "%s\n", buf.ptr);

    jsonpath_item_t *expr = jsonpath_parser(selector);

    if (expr == NULL) {
        if (expect_rcode == JSONPATH_EXEC_RESULT_ERROR) {
#ifdef DEBUG
            fprintf(stderr, "OK    %s %s\n", id, selector);
#endif
            json_value_free(v);
            return true;
        }
#ifdef DEBUG
        fprintf(stderr, "FAIL  %s %s", id, selector);
        dump_result(&buf, expect_result, expect_rcode, NULL, JSONPATH_EXEC_RESULT_ERROR);
#endif
        strbuf_destroy(&buf);
        json_value_free(v);
        return false;
    }

//    strbuf_reset(buf);
//    jsonpath_print_item(buf, expr, false, true);
//    fprintf(stderr, "%s\n", buf->ptr);

    json_value_list_t vresult = {0};
    jsonpath_exec_result_t eresult = jsonpath_exec(expr, v, true, &vresult);

    if (expect_rcode == eresult) {
        if (expect_rcode == JSONPATH_EXEC_RESULT_ERROR) {
#ifdef DEBUG
            fprintf(stderr, "OK    %s %s\n", id, selector);
#endif
            jsonpath_item_free(expr);
            json_value_free(v);
            json_value_list_destroy(&vresult);
            strbuf_destroy(&buf);
            return true;
        }
        if (expect_rcode == JSONPATH_EXEC_RESULT_NOT_FOUND) {
#ifdef DEBUG
            fprintf(stderr, "OK    %s %s\n", id, selector);
#endif
            jsonpath_item_free(expr);
            json_value_free(v);
            json_value_list_destroy(&vresult);
            strbuf_destroy(&buf);
            return true;
        }
        if (cmp_result(&buf, expect_result, &vresult) == 0) {
#ifdef DEBUG
            fprintf(stderr, "OK    %s %s\n", id, selector);
#endif
            jsonpath_item_free(expr);
            json_value_free(v);
            json_value_list_destroy(&vresult);
            strbuf_destroy(&buf);
            return true;
        }
    }
#ifdef DEBUG
    fprintf(stderr, "FAIL  %s %s", id, selector);
    dump_result(&buf, expect_result, expect_rcode, &vresult, eresult);
#endif
    jsonpath_item_free(expr);
    json_value_free(v);
    json_value_list_destroy(&vresult);
    strbuf_destroy(&buf);
    return false;
}

DEF_TEST(parser)
{
    for (size_t i = 0 ; test[i].selector != NULL; i++) {
#if 0
        if (argc > 1) {
            bool found = false;
            for (int n = 1; n < argc; n++) {
                if (strcmp(test[i].id, argv[n]) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found)
                continue;
        }
#endif
        bool result = jsonpath_test(test[i].id, test[i].selector, test[i].document,
                                    test[i].result, test[i].rcode);

        char buffer[256];
        if (test[i].id == NULL)
            snprintf(buffer, sizeof(buffer), "%s", test[i].selector);
        else
            snprintf(buffer, sizeof(buffer), "%s: %s", test[i].id, test[i].selector);

        EXPECT_EQ_INT_STR(1, result, buffer);
    }

    return 0;
}

//int main(int argc, char *argv[])
int main(void)
{
    RUN_TEST(parser);

    END_TEST;
}
