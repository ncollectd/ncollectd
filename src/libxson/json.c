// SPDX-License-Identifier: GPL-2.0-only OR ISC
// Copyright (c) 2007-2014, Lloyd Hilaiel <me@lloyd.io>
// From  http://github.com/lloyd/yajl/yajl.c

#include "libxson/common.h"
#include "libxson/lex.h"
#include "libxson/parser.h"
#include "libxson/bytestack.h"
#include "libxson/buf.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

const char *json_status_to_string(json_status_t stat)
{
    switch (stat) {
        case JSON_STATUS_OK:
            return "ok, no error";
            break;
        case JSON_STATUS_CLIENT_CANCELED:
            return "client canceled parse";
            break;
        case JSON_STATUS_ERROR:
            return "parse error";
            break;
    }
    return "unknown";
}


void json_parser_init(json_parser_t *hand, unsigned int flags,
                      const json_callbacks_t *callbacks, void *ctx)
{
    memset(hand, 0, sizeof(*hand));
    hand->callbacks = callbacks;
    hand->ctx = ctx;
    hand->flags = flags,
    json_lexer_init(&hand->lexer, !(hand->flags & JSON_DONT_VALIDATE_STRINGS));
    JSON_BS_INIT(hand->state_stack);
    json_bs_push(hand->state_stack, JSON_PARSER_STATE_START);
}

void json_parser_free(json_parser_t *handle)
{
    json_bs_free(handle->state_stack);
    json_buf_free(&handle->decode_buf);
    json_lex_free(&handle->lexer);
}

json_status_t json_parser_parse(json_parser_t *hand, const unsigned char *json_txt, size_t json_txt_len)
{
    return json_do_parse(hand, json_txt, json_txt_len);
}


json_status_t json_parser_complete(json_parser_t *hand)
{
    /* The lexer is lazy allocated in the first call to parse.  if parse is
     * never called, then no data was provided to parse at all.  This is a
     * "premature EOF" error unless JSON_ALLOW_PARTIAL_VALUES is specified.
     * allocating the lexer now is the simplest possible way to handle this
     * case while preserving all the other semantics of the parser
     * (multiple values, partial values, etc). */
    return json_do_finish(hand);
}

unsigned char * json_parser_get_error(json_parser_t *hand, int verbose,
                               const unsigned char * json_txt, size_t json_txt_len)
{
    return json_render_error_string(hand, json_txt, json_txt_len, verbose);
}

size_t json_parser_get_bytes_consumed(json_parser_t *hand)
{
    if (!hand) return 0;
    else return hand->bytes_consumed;
}


void json_parser_free_error(unsigned char * str)
{
    free(str);
}

/* XXX: add utility routines to parse from file */
