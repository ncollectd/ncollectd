/* SPDX-License-Identifier: GPL-2.0-only OR ISC         */
/* Copyright (c) 2007-2014, Lloyd Hilaiel <me@lloyd.io> */
/* From  http://github.com/lloyd/yajl                   */
#pragma once

#include "libxson/common.h"
#include <stddef.h>

/** attain a human readable, english, string for an error */
const char *json_status_to_string(json_status_t code);

/** an opaque handle to a parser */

/** allocate a parser handle
 *  \param callbacks  a yajl callbacks structure specifying the
 *                    functions to call when different JSON entities
 *                    are encountered in the input text.  May be NULL,
 *                    which is only useful for validation.
 *  \param afs        memory allocation functions, may be NULL for to use
 *                    C runtime library routines (malloc and friends)
 *  \param ctx        a context pointer that will be passed to callbacks.
 */
void json_parser_init(json_parser_t *hand, unsigned int flags,
                      const json_callbacks_t *callbacks, void *ctx);


/** free a parser handle */
void json_parser_free(json_parser_t *handle);

/** Parse some json!
 *  \param hand - a handle to the json parser allocated with json_alloc
 *  \param json_txt - a pointer to the UTF8 json text to be parsed
 *  \param json_txt_length - the length, in bytes, of input text
 */
json_status_t json_parser_parse(json_parser_t *hand, const unsigned char * json_txt,
                                size_t json_txt_length);

/** Parse any remaining buffered json.
 *  Since yajl is a stream-based parser, without an explicit end of
 *  input, yajl sometimes can't decide if content at the end of the
 *  stream is valid or not.  For example, if "1" has been fed in,
 *  yajl can't know whether another digit is next or some character
 *  that would terminate the integer token.
 *
 *  \param hand - a handle to the json parser allocated with json_alloc
 */
json_status_t json_parser_complete(json_parser_t *hand);

/** get an error string describing the state of the
 *  parse.
 *
 *  If verbose is non-zero, the message will include the JSON
 *  text where the error occured, along with an arrow pointing to
 *  the specific char.
 *
 *  \returns A dynamically allocated string will be returned which should
 *  be freed with json_free_error
 */
unsigned char * json_parser_get_error(json_parser_t *hand, int verbose,
                                      const unsigned char * json_txt, size_t json_txt_length);

/**
 * get the amount of data consumed from the last chunk passed to YAJL.
 *
 * In the case of a successful parse this can help you understand if
 * the entire buffer was consumed (which will allow you to handle
 * "junk at end of input").
 *
 * In the event an error is encountered during parsing, this function
 * affords the client a way to get the offset into the most recent
 * chunk where the error occured.  0 will be returned if no error
 * was encountered.
 */
size_t json_parser_get_bytes_consumed(json_parser_t *hand);

/** free an error returned from json_get_error */
void json_parser_free_error(unsigned char * str);
