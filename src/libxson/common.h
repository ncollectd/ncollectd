/* SPDX-License-Identifier: GPL-2.0-only OR ISC         */
/* Copyright (c) 2007-2014, Lloyd Hilaiel <me@lloyd.io> */
/* From  http://github.com/lloyd/yajl                   */
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "libutils/strbuf.h"

#define JSON_MAX_DEPTH 128

typedef struct {
    size_t len;
    size_t used;
    unsigned char * data;
} json_buf_t;

typedef struct {
    unsigned char * stack;
    size_t size;
    size_t used;
} json_bytestack_t;

typedef enum {
    JSON_TOK_BOOL,
    JSON_TOK_COLON,
    JSON_TOK_COMMA,
    JSON_TOK_EOF,
    JSON_TOK_ERROR,
    JSON_TOK_LEFT_BRACE,
    JSON_TOK_LEFT_BRACKET,
    JSON_TOK_NULL,
    JSON_TOK_RIGHT_BRACE,
    JSON_TOK_RIGHT_BRACKET,
    /* we differentiate between integers and doubles to allow the
     * parser to interpret the number without re-scanning */
    JSON_TOK_INTEGER,
    JSON_TOK_DOUBLE,
    /* we differentiate between strings which require further processing,
     * and strings that do not */
    JSON_TOK_STRING,
    JSON_TOK_STRING_WITH_ESCAPES
} json_tok_t;

typedef enum {
    JSON_LEX_E_OK = 0,
    JSON_LEX_STRING_INVALID_UTF8,
    JSON_LEX_STRING_INVALID_ESCAPED_CHAR,
    JSON_LEX_STRING_INVALID_JSON_CHAR,
    JSON_LEX_STRING_INVALID_HEX_CHAR,
    JSON_LEX_INVALID_CHAR,
    JSON_LEX_INVALID_STRING,
    JSON_LEX_MISSING_INTEGER_AFTER_DECIMAL,
    JSON_LEX_MISSING_INTEGER_AFTER_EXPONENT,
    JSON_LEX_MISSING_INTEGER_AFTER_MINUS,
} json_lex_error_t;

typedef struct {
    /* the overal line and char offset into the data */
    size_t line_offset;
    size_t char_offset;
    /* error */
    json_lex_error_t error;
    /* a input buffer to handle the case where a token is spread over multiple chunks */
    json_buf_t buf;
    /* in the case where we have data in the lexBuf, buff_offset holds
     * the current offset into the lexBuf. */
    size_t buff_offset;
    /* are we using the lex buf? */
    unsigned int buf_in_use;
    /* shall we validate utf8 inside strings? */
    unsigned int validate_utf8;
} json_lexer_t;

typedef enum {
    /** no error was encountered */
    JSON_STATUS_OK,
    /** a client callback returned zero, stopping the parse */
    JSON_STATUS_CLIENT_CANCELED,
    /** An error occured during the parse.  Call json_get_error for
     *  more information about the encountered error */
    JSON_STATUS_ERROR
} json_status_t;

typedef enum {
    JSON_PARSER_STATE_START = 0,
    JSON_PARSER_STATE_PARSE_COMPLETE,
    JSON_PARSER_STATE_PARSE_ERROR,
    JSON_PARSER_STATE_LEXICAL_ERROR,
    JSON_PARSER_STATE_MAP_START,
    JSON_PARSER_STATE_MAP_SEP,
    JSON_PARSER_STATE_MAP_NEED_VAL,
    JSON_PARSER_STATE_MAP_GOT_VAL,
    JSON_PARSER_STATE_MAP_NEED_KEY,
    JSON_PARSER_STATE_ARRAY_START,
    JSON_PARSER_STATE_ARRAY_GOT_VAL,
    JSON_PARSER_STATE_ARRAY_NEED_VAL,
    JSON_PARSER_STATE_GOT_VALUE,
} json_parser_state_t;

/** yajl is an event driven parser.  this means as json elements are
 *  parsed, you are called back to do something with the data.  The
 *  functions in this table indicate the various events for which
 *  you will be called back.  Each callback accepts a "context"
 *  pointer, this is a void * that is passed into the json_parse
 *  function which the client code may use to pass around context.
 *
 *  All callbacks return an integer.  If non-zero, the parse will
 *  continue.  If zero, the parse will be canceled and
 *  json_status_client_canceled will be returned from the parse.
 *
 *  \attention {
 *    A note about the handling of numbers:
 *
 *    yajl will only convert numbers that can be represented in a
 *    double or a 64 bit (long long) int.  All other numbers will
 *    be passed to the client in string form using the json_number
 *    callback.  Furthermore, if json_number is not NULL, it will
 *    always be used to return numbers, that is json_integer and
 *    json_double will be ignored.  If json_number is NULL but one
 *    of json_integer or json_double are defined, parsing of a
 *    number larger than is representable in a double or 64 bit
 *    integer will result in a parse error.
 *  }
 */
typedef struct {
    bool (* json_null)(void * ctx);
    bool (* json_boolean)(void * ctx, int bool_val);
    bool (* json_integer)(void * ctx, long long integer_val);
    bool (* json_double)(void * ctx, double double_val);
    /** A callback which passes the string representation of the number
     *  back to the client.  Will be used for all numbers when present */
    bool (* json_number)(void * ctx, const char * number_val, size_t number_len);

    /** strings are returned as pointers into the JSON text when,
     * possible, as a result, they are _not_ null padded */
    bool (* json_string)(void * ctx, const char * string_val, size_t string_len);

    bool (* json_start_map)(void * ctx);
    bool (* json_map_key)(void * ctx, const char *key_val, size_t key_len);
    bool (* json_end_map)(void * ctx);

    bool (* json_start_array)(void * ctx);
    bool (* json_end_array)(void * ctx);
} json_callbacks_t;

typedef struct {
    const json_callbacks_t *callbacks;
    void * ctx;
    json_lexer_t lexer;
    const char * parse_error;
    /* the number of bytes consumed from the last client buffer,
     * in the case of an error this will be an error offset, in the
     * case of an error this can be used as the error offset */
    size_t bytes_consumed;
    /* temporary storage for decoded strings */
    json_buf_t decode_buf;
    /* a stack of states.  access with json_state_XXX routines */
    json_bytestack_t state_stack;
    /* bitfield */
    unsigned int flags;
} json_parser_t;

typedef enum {
    /**
     * When set the parser will verify that all strings in JSON input are
     * valid UTF8 and will emit a parse error if this is not so.  When set,
     * this option makes parsing slightly more expensive (~7% depending
     * on processor and compiler in use)
     */
    JSON_DONT_VALIDATE_STRINGS = 0X01,
    /**
     * By default, upon calls to json_complete_parse(), yajl will
     * ensure the entire input text was consumed and will raise an error
     * otherwise.  Enabling this flag will cause yajl to disable this
     * check.  This can be useful when parsing json out of a that contains more
     * than a single JSON document.
     */
    JSON_ALLOW_TRAILING_GARBAGE = 0X02,
    /**
     * Allow multiple values to be parsed by a single handle.  The
     * entire text must be valid JSON, and values can be seperated
     * by any kind of whitespace.  This flag will change the
     * behavior of the parser, and cause it continue parsing after
     * a value is parsed, rather than transitioning into a
     * complete state.  This option can be useful when parsing multiple
     * values from an input stream.
     */
    JSON_ALLOW_MULTIPLE_VALUES = 0X04,
    /**
     * When json_complete_parse() is called the parser will
     * check that the top level value was completely consumed.  I.E.,
     * if called whilst in the middle of parsing a value
     * yajl will enter an error state (premature EOF).  Setting this
     * flag suppresses that check and the corresponding error.
     */
    JSON_ALLOW_PARTIAL_VALUES = 0X08
} json_option_t;
