/* SPDX-License-Identifier: GPL-2.0-only OR ISC         */
/* Copyright (c) 2007-2014, Lloyd Hilaiel <me@lloyd.io> */
/* From  http://github.com/lloyd/yajl                   */
#pragma once

#include "libxson/common.h"

void json_string_decode(json_buf_t *buf, const unsigned char * str, size_t length);

int json_string_validate_utf8(const unsigned char * s, size_t len);
