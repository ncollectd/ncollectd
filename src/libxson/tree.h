/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once

#include "libxson/common.h"
#include "libxson/value.h"
#include "libxson/render.h"

json_value_t *json_tree_parser(const char *input, char *error_buffer, size_t error_buffer_size);

int json_tree_render(json_value_t *v, strbuf_t *buf,
                     xson_render_type_t type, xson_render_option_t options);
