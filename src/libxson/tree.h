/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once

#include "libxson/common.h"
#include "libxson/value.h"
#include "libxson/render.h"

xson_value_t *xson_tree_parser(const char *input, char *error_buffer, size_t error_buffer_size);

struct xson_tree_parser;
typedef struct xson_tree_parser xson_tree_parser_t;

xson_tree_parser_t *xson_tree_parser_init(char *error_buffer, size_t error_buffer_size);

int xson_tree_parser_chunk(xson_tree_parser_t *parser, const unsigned char *input, size_t len);

xson_value_t *xson_tree_parser_complete(xson_tree_parser_t *parser);

void xson_tree_parser_free(xson_tree_parser_t *parser);

int xson_tree_render(xson_value_t *v, strbuf_t *buf,
                     xson_render_type_t type, xson_render_option_t options);
