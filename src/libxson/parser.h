/* SPDX-License-Identifier: GPL-2.0-only OR ISC         */
/* Copyright (c) 2007-2014, Lloyd Hilaiel <me@lloyd.io> */
/* From  http://github.com/lloyd/yajl                   */
#pragma once

#include "libxson/common.h"

json_status_t json_do_parse(json_parser_t *handle, const unsigned char *json_txt, size_t json_txt_len);

json_status_t json_do_finish(json_parser_t *handle);

unsigned char * json_render_error_string(json_parser_t *hand, const unsigned char * json_txt,
                                         size_t json_txt_len, int verbose);

/* A little built in integer parsing routine with the same semantics as strtol
 * that's unaffected by LOCALE. */
long long json_parse_integer(const unsigned char *number, unsigned int length);
