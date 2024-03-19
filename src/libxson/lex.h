/* SPDX-License-Identifier: GPL-2.0-only OR ISC         */
/* Copyright (c) 2007-2014, Lloyd Hilaiel <me@lloyd.io> */
/* From  http://github.com/lloyd/yajl                   */
#pragma once

#include "libxson/common.h"

void json_lexer_init(json_lexer_t *lexer, unsigned int validate_utf8);

void json_lex_free(json_lexer_t *lexer);

/**
 * run/continue a lex. "offset" is an input/output parameter.
 * It should be initialized to zero for a
 * new chunk of target text, and upon subsetquent calls with the same
 * target text should passed with the value of the previous invocation.
 *
 * the client may be interested in the value of offset when an error is
 * returned from the lexer.  This allows the client to render useful
 * error messages.
 *
 * When you pass the next chunk of data, context should be reinitialized
 * to zero.
 *
 * Finally, the output buffer is usually just a pointer into the json_txt,
 * however in cases where the entity being lexed spans multiple chunks,
 * the lexer will buffer the entity and the data returned will be
 * a pointer into that buffer.
 *
 * This behavior is abstracted from client code except for the performance
 * implications which require that the client choose a reasonable chunk
 * size to get adequate performance.
 */
json_tok_t json_lex_lex(json_lexer_t *lexer, const unsigned char * json_txt,
                        size_t json_txt_len, size_t * offset,
                        const unsigned char ** out_buf, size_t * out_len);

/** have a peek at the next token, but don't move the lexer forward */
json_tok_t json_lex_peek(json_lexer_t *lexer, const unsigned char * json_txt,
                         size_t json_txt_len, size_t offset);


const char *json_lex_error_to_string(json_lex_error_t error);

/** allows access to more specific information about the lexical
 *  error when json_lex_lex returns json_tok_error. */
json_lex_error_t json_lex_get_error(json_lexer_t *lexer);

/** get the current offset into the most recently lexed json string. */
size_t json_lex_current_offset(json_lexer_t *lexer);

/** get the number of lines lexed by this lexer instance */
size_t json_lex_current_line(json_lexer_t *lexer);

/** get the number of chars lexed by this lexer instance since the last
 *  \n or \r */
size_t json_lex_current_char(json_lexer_t *lexer);
