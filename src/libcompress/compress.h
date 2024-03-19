/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once

#include "libconfig/config.h"
#include "libutils/strbuf.h"
#include "libutils/buf.h"

typedef enum {
    COMPRESS_FORMAT_NONE,
    COMPRESS_FORMAT_SNAPPY,
    COMPRESS_FORMAT_GZIP,
    COMPRESS_FORMAT_ZLIB,
    COMPRESS_FORMAT_DEFLATE
} compress_format_t;

int config_compress(config_item_t *ci, compress_format_t *format);

char *compress(compress_format_t format, char *in_data, size_t in_data_len, size_t *out_data_len);

int compress_free(compress_format_t format, char *data);

const char *compress_get_encoding(compress_format_t format);
