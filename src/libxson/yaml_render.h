/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once

#include "libxson/render.h"

int render_syaml_open(xson_render_t *r, xson_render_block_t type);

int render_syaml_close(xson_render_t *r, xson_render_block_t type);

int render_syaml_key(xson_render_t *r, xson_render_key_type_t type, xson_render_key_t k);

int render_syaml_value(xson_render_t *r, xson_render_value_type_t type, xson_render_value_t v);
