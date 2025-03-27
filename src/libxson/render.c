// SPDX-License-Identifier: GPL-2.0-only

#include "ncollectd.h"
#include "libxson/render.h"
#include "libxson/json_render.h"
#include "libxson/yaml_render.h"

#include <stdbool.h>
#include <stdlib.h>

xson_render_status_t xson_render_open(xson_render_t *r, xson_render_block_t type,
                                       __attribute__((unused)) ssize_t size)
{
    if (unlikely(r->state[r->depth] == XSON_RENDER_STATE_ERROR))
        return XSON_RENDER_STATUS_IN_ERROR_STATE;

    if (unlikely(r->state[r->depth] == XSON_RENDER_STATE_COMPLETE))
        return XSON_RENDER_STATUS_GENERATION_COMPLETE;

    if (unlikely((r->state[r->depth] == XSON_RENDER_STATE_MAP_KEY) ||
                 (r->state[r->depth] == XSON_RENDER_STATE_MAP_START))) {
        r->state[r->depth] = XSON_RENDER_STATE_ERROR;
        return XSON_RENDER_STATUS_KEYS_MUST_BE_STRINGS;
    }

    if (unlikely((r->depth+1) >= XSON_MAX_DEPTH)) {
        r->state[r->depth] = XSON_RENDER_STATE_ERROR;
        return XSON_RENDER_STATUS_MAX_DEPTH_EXCEEDED;
    }

    r->depth++;

    int status = 0;
    switch(r->type) {
    case XSON_RENDER_TYPE_JSON:
        status = render_json_open(r, type);
        break;
    case XSON_RENDER_TYPE_SYAML:
        status = render_syaml_open(r, type);
        break;
    case XSON_RENDER_TYPE_JSONB:
//        status = jsonb_render_open(r, type);
        break;
    }

    if (unlikely(status != 0)) {
        r->state[r->depth] = XSON_RENDER_STATE_ERROR;
        return XSON_RENDER_STATUS_IN_ERROR_STATE;
    }

    switch(type){
    case XSON_RENDER_BLOCK_MAP:
        r->state[r->depth] = XSON_RENDER_STATE_MAP_START;
        break;
    case XSON_RENDER_BLOCK_ARRAY:
        r->state[r->depth] = XSON_RENDER_STATE_ARRAY_START;
        break;
    }

    return XSON_RENDER_STATUS_OK;
}

xson_render_status_t xson_render_close(xson_render_t *r, xson_render_block_t type)
{
    if (unlikely(r->state[r->depth] == XSON_RENDER_STATE_ERROR))
        return XSON_RENDER_STATUS_IN_ERROR_STATE;

    if (unlikely(r->state[r->depth] == XSON_RENDER_STATE_COMPLETE))
        return XSON_RENDER_STATUS_GENERATION_COMPLETE;

    if (unlikely(r->depth == 0))
        return XSON_RENDER_STATUS_GENERATION_COMPLETE;

    r->depth--;

    switch (r->state[r->depth]) {
    case XSON_RENDER_STATE_START:
        r->state[r->depth] = XSON_RENDER_STATE_COMPLETE;
        break;
    case XSON_RENDER_STATE_MAP_KEY:
        r->state[r->depth] = XSON_RENDER_STATE_MAP_VAL;
        break;
    case XSON_RENDER_STATE_ARRAY_START:
        r->state[r->depth] = XSON_RENDER_STATE_IN_ARRAY;
        break;
    case XSON_RENDER_STATE_MAP_VAL:
        r->state[r->depth] = XSON_RENDER_STATE_MAP_KEY;
        break;
    default:
        break;
    }

    int status = 0;

    switch(r->type) {
    case XSON_RENDER_TYPE_JSON:
        status = render_json_close(r, type);
        break;
    case XSON_RENDER_TYPE_SYAML:
        status = render_syaml_close(r, type);
        break;
    case XSON_RENDER_TYPE_JSONB:
//        status = jsonb_render_close(r, type);
        break;
    }

    if (unlikely(status != 0)) {
        r->state[r->depth] = XSON_RENDER_STATE_ERROR;
        return XSON_RENDER_STATUS_IN_ERROR_STATE;
    }

    return XSON_RENDER_STATUS_OK;
}

xson_render_status_t xson_render_key(xson_render_t *r, xson_render_key_type_t type,
                                                       xson_render_key_t k)
{
    if (unlikely(r->state[r->depth] == XSON_RENDER_STATE_ERROR))
        return XSON_RENDER_STATUS_IN_ERROR_STATE;

    if (unlikely(r->state[r->depth] == XSON_RENDER_STATE_COMPLETE))
        return XSON_RENDER_STATUS_GENERATION_COMPLETE;

#if 0
    if ((r->state[r->depth] != XSON_RENDER_STATE_MAP_START) &&
        (r->state[r->depth] != XSON_RENDER_STATE_MAP_VAL))
        return XSON_RENDER_STATUS_XXX;
#endif
    int status = 0;

    switch(r->type) {
    case XSON_RENDER_TYPE_JSON:
        status = render_json_key(r, type, k);
        break;
    case XSON_RENDER_TYPE_SYAML:
        status = render_syaml_key(r, type, k);
        break;
    case XSON_RENDER_TYPE_JSONB:
//        status = jsonb_render_key(r, type, k);
        break;
    }

    r->state[r->depth] = XSON_RENDER_STATE_MAP_VAL;

    if (unlikely(status != 0)) {
        r->state[r->depth] = XSON_RENDER_STATE_ERROR;
        return XSON_RENDER_STATUS_IN_ERROR_STATE;
    }

    return XSON_RENDER_STATUS_OK;
}

xson_render_status_t xson_render_value(xson_render_t *r,
                                       xson_render_value_type_t type, xson_render_value_t v)
{
    if (unlikely(r->state[r->depth] == XSON_RENDER_STATE_ERROR))
        return XSON_RENDER_STATUS_IN_ERROR_STATE;

    if (unlikely(r->state[r->depth] == XSON_RENDER_STATE_COMPLETE))
        return XSON_RENDER_STATUS_GENERATION_COMPLETE;

    int status = 0;

    switch(r->type) {
    case XSON_RENDER_TYPE_JSON:
        status = render_json_value(r, type, v);
        break;
    case XSON_RENDER_TYPE_SYAML:
        status = render_syaml_value(r, type, v);
        break;
    case XSON_RENDER_TYPE_JSONB:
//        status = jsonb_render_value(r, type, v);
        break;
    }

    if (unlikely(status != 0)) {
        r->state[r->depth] = XSON_RENDER_STATE_ERROR;
        return XSON_RENDER_STATUS_IN_ERROR_STATE;
    }

    switch (r->state[r->depth]) {
    case XSON_RENDER_STATE_START:
        r->state[r->depth] = XSON_RENDER_STATE_COMPLETE;
        break;
    case XSON_RENDER_STATE_MAP_KEY:
        r->state[r->depth] = XSON_RENDER_STATE_MAP_VAL;
        break;
    case XSON_RENDER_STATE_ARRAY_START:
        r->state[r->depth] = XSON_RENDER_STATE_IN_ARRAY;
        break;
    case XSON_RENDER_STATE_MAP_VAL:
        r->state[r->depth] = XSON_RENDER_STATE_MAP_KEY;
        break;
    default:
        break;
    }

    return XSON_RENDER_STATUS_OK;
}

void xson_render_clear(xson_render_t *r)
{
    r->depth = 0;
    memset(r->state, 0, sizeof(r->state));
    strbuf_reset(r->buf);
}

void xson_render_reset(xson_render_t *r, const char *sep)
{
    r->depth = 0;
    memset(r->state, 0, sizeof(r->state));
    if (sep != NULL)
        strbuf_putstr(r->buf, sep);
}

void xson_render_init(xson_render_t *r, strbuf_t *buf, xson_render_type_t type,
                                                       xson_render_option_t options)
{
    r->type = type;
    r->flags = options;
    r->depth = 0;
    r->buf = buf;

    memset(r->state, 0, sizeof(r->state));
    memset(r->block_length, 0, sizeof(r->block_length));
    memset(r->block_size, 0, sizeof(r->block_size));
}
