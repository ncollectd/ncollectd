// SPDX-License-Identifier: GPL-2.0-only

#include "ncollectd.h"
#include "libxson/render.h"

#include <stdbool.h>
#include <stdlib.h>

int render_json_open(xson_render_t *r, xson_render_block_t type)
{
    int status = 0;

    if ((r->state[r->depth-1] == XSON_RENDER_STATE_MAP_KEY) ||
        (r->state[r->depth-1] == XSON_RENDER_STATE_IN_ARRAY)) {
        status |= strbuf_putchar(r->buf, ',');
        if ((r->flags & XSON_RENDER_OPTION_JSON_BEAUTIFY))
            status |= strbuf_putchar(r->buf, '\n');
    } else if (r->state[r->depth-1] == XSON_RENDER_STATE_MAP_VAL) {
        if ((r->flags & XSON_RENDER_OPTION_JSON_BEAUTIFY))
            status |= strbuf_putchar(r->buf, ' ');
    }

    if ((r->flags & XSON_RENDER_OPTION_JSON_BEAUTIFY) &&
        (r->state[r->depth-1] != XSON_RENDER_STATE_MAP_VAL) && ((r->depth - 1) > 0)) {
        status |= strbuf_putxchar(r->buf, ' ', 4 * (r->depth - 1));
    }

    switch(type){
    case XSON_RENDER_BLOCK_MAP:
        status |= strbuf_putchar(r->buf, '{');
        break;
    case XSON_RENDER_BLOCK_ARRAY:
        status |= strbuf_putchar(r->buf, '[');
        break;
    }

    if (r->flags & XSON_RENDER_OPTION_JSON_BEAUTIFY)
        status |= strbuf_putchar(r->buf, '\n');

    return status;
}

int render_json_close(xson_render_t *r, xson_render_block_t type)
{
    int status = 0;

    if (r->flags & XSON_RENDER_OPTION_JSON_BEAUTIFY)
        status |= strbuf_putchar(r->buf, '\n');

    if ((r->flags & XSON_RENDER_OPTION_JSON_BEAUTIFY) &&
        (r->state[r->depth] != XSON_RENDER_STATE_MAP_VAL) && (r->depth > 0)) {
        status |= strbuf_putxchar(r->buf, ' ', 4 * r->depth);
    }

    switch(type){
    case XSON_RENDER_BLOCK_MAP:
        status |= strbuf_putchar(r->buf, '}');
        break;
    case XSON_RENDER_BLOCK_ARRAY:
        status |= strbuf_putchar(r->buf, ']');
        break;
    }

    if (r->flags & XSON_RENDER_OPTION_JSON_BEAUTIFY) {
        if (r->state[r->depth] == XSON_RENDER_STATE_COMPLETE)
            status |= strbuf_putchar(r->buf, '\n');
    }

    return status;
}

int render_json_key(xson_render_t *r, xson_render_key_type_t type, xson_render_key_t k)
{
    int status = 0;

    if ((r->state[r->depth] == XSON_RENDER_STATE_MAP_KEY) ||
        (r->state[r->depth] == XSON_RENDER_STATE_IN_ARRAY)) {
        status |= strbuf_putchar(r->buf, ',');
        if ((r->flags & XSON_RENDER_OPTION_JSON_BEAUTIFY))
            status |= strbuf_putchar(r->buf, '\n');
    } else if (r->state[r->depth] == XSON_RENDER_STATE_MAP_VAL) {
        if ((r->flags & XSON_RENDER_OPTION_JSON_BEAUTIFY))
            status |= strbuf_putchar(r->buf, ' ');
    }

    if ((r->flags & XSON_RENDER_OPTION_JSON_BEAUTIFY) &&
        (r->state[r->depth] != XSON_RENDER_STATE_MAP_VAL) && (r->depth > 0)) {
        status |= strbuf_putxchar(r->buf, ' ', 4 * r->depth);
    }

    status |= strbuf_putchar(r->buf, '"');
    switch(type) {
    case XSON_RENDER_KEY_TYPE_STRING:
        status |= strbuf_putescape_json(r->buf, k.string);
        break;
    case XSON_RENDER_KEY_TYPE_IOV:
        for (int i = 0; i < k.iovcnt; i++) {
            status |= strbuf_putnescape_json(r->buf, k.iov[i].iov_base, k.iov[i].iov_len);
        }
        break;
    }
    status |= strbuf_putstrn(r->buf, "\":", 2);

    return status;
}

int render_json_value(xson_render_t *r, xson_render_value_type_t type, xson_render_value_t v)
{
    int status = 0;

    if ((r->state[r->depth] == XSON_RENDER_STATE_MAP_KEY) ||
        (r->state[r->depth] == XSON_RENDER_STATE_IN_ARRAY)) {
        status |= strbuf_putchar(r->buf, ',');
        if ((r->flags & XSON_RENDER_OPTION_JSON_BEAUTIFY))
            status |= strbuf_putchar(r->buf, '\n');
    } else if (r->state[r->depth] == XSON_RENDER_STATE_MAP_VAL) {
        if ((r->flags & XSON_RENDER_OPTION_JSON_BEAUTIFY))
            status |= strbuf_putchar(r->buf, ' ');
    }

    if ((r->flags & XSON_RENDER_OPTION_JSON_BEAUTIFY) &&
        (r->state[r->depth] != XSON_RENDER_STATE_MAP_VAL) && (r->depth > 0)) {
        status |= strbuf_putxchar(r->buf, ' ', 4 * r->depth);
    }

    switch(type) {
    case XSON_RENDER_VALUE_TYPE_NULL:
        status |= strbuf_putstrn(r->buf, "null", strlen("null"));
        break;
    case XSON_RENDER_VALUE_TYPE_STRING:
        status |= strbuf_putchar(r->buf, '"');
        status |= strbuf_putescape_json(r->buf, v.string);
        status |= strbuf_putchar(r->buf, '"');
        break;
    case XSON_RENDER_VALUE_TYPE_IOV:
        status |= strbuf_putchar(r->buf, '"');
        for (int i = 0; i < v.iovcnt; i++) {
            status |= strbuf_putnescape_json(r->buf, v.iov[i].iov_base, v.iov[i].iov_len);
        }
        status |= strbuf_putchar(r->buf, '"');
        break;
    case XSON_RENDER_VALUE_TYPE_DOUBLE:
        status |= strbuf_putdouble(r->buf, v.dnumber);
        break;
    case XSON_RENDER_VALUE_TYPE_INTEGER:
        status |= strbuf_putint(r->buf, v.inumber);
        break;
    case XSON_RENDER_VALUE_TYPE_TRUE:
        status |= strbuf_putstrn(r->buf, "true", strlen("true"));
        break;
    case XSON_RENDER_VALUE_TYPE_FALSE:
        status |= strbuf_putstrn(r->buf, "false", strlen("false"));
        break;
    }

    if (r->flags & XSON_RENDER_OPTION_JSON_BEAUTIFY) {
        if (r->state[r->depth] == XSON_RENDER_STATE_START) // -> XSON_RENDER_STATE_COMPLETE
            status |= strbuf_putchar(r->buf, '\n');
    }

    return status;
}
