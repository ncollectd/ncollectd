// SPDX-License-Identifier: GPL-2.0-only

#include "ncollectd.h"
#include "log.h"
#include "libutils/config.h"
#include "libcompress/compress.h"
#include "libcompress/csnappy.h"
#include "libcompress/slz.h"

int config_compress(config_item_t *ci, compress_format_t *format)
{
    if ((ci->values_num < 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The '%s' config option needs at least one string argument.", ci->key);
        return -1;
    }

    char *fmt = ci->values[0].value.string;

    if (strcasecmp(fmt, "none") == 0) {
        *format = COMPRESS_FORMAT_NONE;
    } else if (strcasecmp(fmt, "snappy") == 0) {
        *format = COMPRESS_FORMAT_SNAPPY;
    } else if (strcasecmp(fmt, "gzip") == 0) {
        *format = COMPRESS_FORMAT_GZIP;
    } else if (strcasecmp(fmt, "zlib") == 0) {
        *format = COMPRESS_FORMAT_ZLIB;
    } else if (strcasecmp(fmt, "deflate") == 0) {
        *format = COMPRESS_FORMAT_DEFLATE;
    } else {
        PLUGIN_ERROR("Invalid format string: %s", fmt);
        return -1;
    }

    return 0;
}

const char *compress_get_encoding(compress_format_t format)
{
    switch (format) {
    case COMPRESS_FORMAT_NONE:
        return NULL;
    case COMPRESS_FORMAT_SNAPPY:
        return "snappy";
    case COMPRESS_FORMAT_GZIP:
        return "gzip";
    case COMPRESS_FORMAT_ZLIB:
        return "zlib"; // FIXME
    case COMPRESS_FORMAT_DEFLATE:
        return "deflate";
    }

    return NULL;
}

char *compress(compress_format_t format, char *in_data, size_t in_data_len, size_t *out_data_len)
{
    switch (format) {
    case COMPRESS_FORMAT_NONE:
        *out_data_len = in_data_len;
        return in_data;
        break;
    case COMPRESS_FORMAT_SNAPPY: {
        uint32_t compressed_length = csnappy_max_compressed_length(in_data_len);
        char *out_data = malloc(sizeof(*out_data)*compressed_length);
        if (out_data == NULL) {
            PLUGIN_ERROR("malloc failed");
            return NULL;
        }

        void *working = malloc(CSNAPPY_WORKMEM_BYTES);
        if (working == NULL) {
            free(out_data);
            PLUGIN_ERROR("malloc failed");
            return NULL;
        }
        uint32_t len = 0;
        csnappy_compress(in_data, in_data_len, out_data, &len,
                                  working, CSNAPPY_WORKMEM_BYTES_POWER_OF_TWO);
        free(working);
        *out_data_len = len;
        return out_data;
    }   break;
    case COMPRESS_FORMAT_GZIP:
    case COMPRESS_FORMAT_ZLIB:
    case COMPRESS_FORMAT_DEFLATE: {
        int fmt = 0;
        if (format == COMPRESS_FORMAT_GZIP) {
            fmt = SLZ_FMT_GZIP;
        } else if (format == COMPRESS_FORMAT_ZLIB) {
            fmt = SLZ_FMT_ZLIB;
        } else if (format == COMPRESS_FORMAT_DEFLATE) {
            fmt = SLZ_FMT_DEFLATE;
        }

        struct slz_stream strm = {0};
        int status = slz_init(&strm, 1, fmt);
        if (status == 0)
            return NULL;

        char *out_data = malloc(sizeof(*out_data)*(in_data_len+12));
        if (out_data == NULL) {
            PLUGIN_ERROR("malloc failed");
            return NULL;
        }

        long size = slz_encode(&strm, out_data, in_data, in_data_len, 0);
        if (size < 0) {
            PLUGIN_ERROR("slz_encode failed");
            free(out_data);
            return NULL;
        }

        size += slz_finish(&strm, out_data+size);

        *out_data_len = size;
        return out_data;
    }   break;
    }

    return NULL;
}

int compress_free(compress_format_t format, char *data)
{
    switch (format) {
    case COMPRESS_FORMAT_NONE:
        break;
    case COMPRESS_FORMAT_SNAPPY:
        free(data);
        break;
    case COMPRESS_FORMAT_GZIP:
    case COMPRESS_FORMAT_ZLIB:
    case COMPRESS_FORMAT_DEFLATE:
        free(data);
        break;
    }

    return 0;
}
