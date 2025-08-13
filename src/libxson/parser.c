// SPDX-License-Identifier: GPL-2.0-only OR ISC
// Copyright (c) 2007-2014, Lloyd Hilaiel <me@lloyd.io>
// From  http://github.com/lloyd/yajl

#include "libxson/common.h"
#include "libxson/lex.h"
#include "libxson/parser.h"
#include "libxson/encode.h"
#include "libxson/buf.h"
#include "libxson/bytestack.h"

#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <math.h>

#define MAX_VALUE_TO_MULTIPLY ((LLONG_MAX / 10) + (LLONG_MAX % 10))

 /* same semantics as strtol */
long long json_parse_integer(const unsigned char *number, unsigned int length)
{
    long long ret  = 0;
    long sign = 1;
    const unsigned char *pos = number;
    if (*pos == '-') { pos++; sign = -1; }
    if (*pos == '+') { pos++; }

    while (pos < number + length) {
        if ( ret > MAX_VALUE_TO_MULTIPLY ) {
            errno = ERANGE;
            return sign == 1 ? LLONG_MAX : LLONG_MIN;
        }
        ret *= 10;
        if (LLONG_MAX - ret < (*pos - '0')) {
            errno = ERANGE;
            return sign == 1 ? LLONG_MAX : LLONG_MIN;
        }
        if (*pos < '0' || *pos > '9') {
            errno = ERANGE;
            return sign == 1 ? LLONG_MAX : LLONG_MIN;
        }
        ret += (*pos++ - '0');
    }

    return sign * ret;
}

unsigned char *json_render_error_string(json_parser_t *hand, const unsigned char * json_txt,
                                        size_t json_txt_len, int verbose)
{
    size_t offset = hand->bytes_consumed;
    unsigned char * str;
    const char * error_type = NULL;
    const char * error_text = NULL;
    char text[72];
    const char * arrow = "                     (right here) ------^\n";

    if (json_bs_current(hand->state_stack) == JSON_PARSER_STATE_PARSE_ERROR) {
        error_type = "parse";
        error_text = hand->parse_error;
    } else if (json_bs_current(hand->state_stack) == JSON_PARSER_STATE_LEXICAL_ERROR) {
        error_type = "lexical";
        error_text = json_lex_error_to_string(json_lex_get_error(&hand->lexer));
    } else {
        error_type = "unknown";
    }

    {
        size_t memneeded = 0;
        memneeded += strlen(error_type);
        memneeded += strlen(" error");
        if (error_text != NULL) {
            memneeded += strlen(": ");
            memneeded += strlen(error_text);
        }
        str = (unsigned char *) malloc(memneeded + 2);
        if (!str) return NULL;
        str[0] = 0;
        strcat((char *) str, error_type);
        strcat((char *) str, " error");
        if (error_text != NULL) {
            strcat((char *) str, ": ");
            strcat((char *) str, error_text);
        }
        strcat((char *) str, "\n");
    }

    /* now we append as many spaces as needed to make sure the error
     * falls at char 41, if verbose was specified */
    if (verbose) {
        size_t start, end, i;
        size_t spaces_needed;

        spaces_needed = (offset < 30 ? 40 - offset : 10);
        start = (offset >= 30 ? offset - 30 : 0);
        end = (offset + 30 > json_txt_len ? json_txt_len : offset + 30);

        for (i=0;i<spaces_needed;i++) text[i] = ' ';

        for (;start < end;start++, i++) {
            if (json_txt[start] != '\n' && json_txt[start] != '\r') {
                text[i] = json_txt[start];
            } else {
                text[i] = ' ';
            }
        }
        assert(i <= 71);
        text[i++] = '\n';
        text[i] = 0;
        {
            char * newStr = malloc((unsigned int)(strlen((char *) str) +
                                                  strlen((char *) text) +
                                                  strlen(arrow) + 1));
            if (newStr) {
                newStr[0] = 0;
                strcat((char *) newStr, (char *) str);
                strcat((char *) newStr, text);
                strcat((char *) newStr, arrow);
            }
            free(str);
            str = (unsigned char *) newStr;
        }
    }
    return str;
}

/* check for client cancellation */
#define _CC_CHK(x)                                                              \
    if (!(x)) {                                                                 \
        json_bs_set(hand->state_stack, JSON_PARSER_STATE_PARSE_ERROR);          \
        hand->parse_error = "client cancelled parse via callback return value"; \
        return JSON_STATUS_CLIENT_CANCELED;                                     \
    }


json_status_t json_do_finish(json_parser_t *hand)
{
    json_status_t stat = json_do_parse(hand,(const unsigned char *) " ",1);

    if (stat != JSON_STATUS_OK)
        return stat;

    switch(json_bs_current(hand->state_stack)) {
    case JSON_PARSER_STATE_PARSE_ERROR:
    case JSON_PARSER_STATE_LEXICAL_ERROR:
        return JSON_STATUS_ERROR;
    case JSON_PARSER_STATE_GOT_VALUE:
    case JSON_PARSER_STATE_PARSE_COMPLETE:
        return JSON_STATUS_OK;
    default:
        if (!(hand->flags & JSON_ALLOW_PARTIAL_VALUES)) {
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_PARSE_ERROR);
            hand->parse_error = "premature EOF";
            return JSON_STATUS_ERROR;
        }
        return JSON_STATUS_OK;
    }
}

json_status_t json_do_parse(json_parser_t *hand, const unsigned char * json_txt, size_t json_txt_len)
{
    json_tok_t tok;
    const unsigned char * buf;
    size_t buf_len;
    size_t * offset = &(hand->bytes_consumed);

    *offset = 0;

around_again:
    switch (json_bs_current(hand->state_stack)) {
    case JSON_PARSER_STATE_PARSE_COMPLETE:
        if (hand->flags & JSON_ALLOW_MULTIPLE_VALUES) {
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_GOT_VALUE);
            goto around_again;
        }
        if (!(hand->flags & JSON_ALLOW_TRAILING_GARBAGE)) {
            if (*offset != json_txt_len) {
                tok = json_lex_lex(&hand->lexer, json_txt, json_txt_len,
                                   offset, &buf, &buf_len);
                if (tok != JSON_TOK_EOF) {
                    json_bs_set(hand->state_stack, JSON_PARSER_STATE_PARSE_ERROR);
                    hand->parse_error = "trailing garbage";
                }
                goto around_again;
            }
        }
        return JSON_STATUS_OK;
    case JSON_PARSER_STATE_LEXICAL_ERROR:
    case JSON_PARSER_STATE_PARSE_ERROR:
        return JSON_STATUS_ERROR;
    case JSON_PARSER_STATE_START:
    case JSON_PARSER_STATE_GOT_VALUE:
    case JSON_PARSER_STATE_MAP_NEED_VAL:
    case JSON_PARSER_STATE_ARRAY_NEED_VAL:
    case JSON_PARSER_STATE_ARRAY_START:  {
        /* for arrays and maps, we advance the state for this
         * depth, then push the state of the next depth.
         * If an error occurs during the parsing of the nesting
         * entity, the state at this level will not matter.
         * a state that needs pushing will be anything other
         * than state_start */

        json_parser_state_t state_to_push = JSON_PARSER_STATE_START;

        tok = json_lex_lex(&hand->lexer, json_txt, json_txt_len, offset, &buf, &buf_len);

        switch (tok) {
        case JSON_TOK_EOF:
            return JSON_STATUS_OK;
        case JSON_TOK_ERROR:
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_LEXICAL_ERROR);
            goto around_again;
        case JSON_TOK_STRING:
            if (hand->callbacks && hand->callbacks->xson_string) {
                _CC_CHK(hand->callbacks->xson_string(hand->ctx, (const char *)buf, buf_len));
            }
            break;
        case JSON_TOK_STRING_WITH_ESCAPES:
            if (hand->callbacks && hand->callbacks->xson_string) {
                json_buf_clear(&hand->decode_buf);
                json_string_decode(&hand->decode_buf, buf, buf_len);
                _CC_CHK(hand->callbacks->xson_string(hand->ctx,
                                                     (const char *)json_buf_data(&hand->decode_buf),
                                                     json_buf_len(&hand->decode_buf)));
            }
            break;
        case JSON_TOK_BOOL:
            if (hand->callbacks && hand->callbacks->xson_boolean) {
                _CC_CHK(hand->callbacks->xson_boolean(hand->ctx, *buf == 't'));
            }
            break;
        case JSON_TOK_NULL:
            if (hand->callbacks && hand->callbacks->xson_null) {
                _CC_CHK(hand->callbacks->xson_null(hand->ctx));
            }
            break;
        case JSON_TOK_LEFT_BRACKET:
            if (hand->callbacks && hand->callbacks->xson_start_map) {
                _CC_CHK(hand->callbacks->xson_start_map(hand->ctx));
            }
            state_to_push = JSON_PARSER_STATE_MAP_START;
            break;
        case JSON_TOK_LEFT_BRACE:
            if (hand->callbacks && hand->callbacks->xson_start_array) {
                _CC_CHK(hand->callbacks->xson_start_array(hand->ctx));
            }
            state_to_push = JSON_PARSER_STATE_ARRAY_START;
            break;
        case JSON_TOK_INTEGER:
            if (hand->callbacks) {
                if (hand->callbacks->xson_number) {
                    _CC_CHK(hand->callbacks->xson_number(hand->ctx,(const char *) buf, buf_len));
                } else if (hand->callbacks->xson_integer) {
                    long long int i = 0;
                    errno = 0;
                    i = json_parse_integer(buf, buf_len);
                    if ((i == LLONG_MIN || i == LLONG_MAX) && errno == ERANGE) {
                        json_bs_set(hand->state_stack, JSON_PARSER_STATE_PARSE_ERROR);
                        hand->parse_error = "integer overflow" ;
                        /* try to restore error offset */
                        if (*offset >= buf_len) *offset -= buf_len;
                        else *offset = 0;
                        goto around_again;
                    }
                    _CC_CHK(hand->callbacks->xson_integer(hand->ctx, i));
                }
            }
            break;
        case JSON_TOK_DOUBLE:
            if (hand->callbacks) {
                if (hand->callbacks->xson_number) {
                    _CC_CHK(hand->callbacks->xson_number(hand->ctx, (const char *) buf, buf_len));
                } else if (hand->callbacks->xson_double) {
                    double d = 0.0;
                    json_buf_clear(&hand->decode_buf);
                    json_buf_append(&hand->decode_buf, buf, buf_len);
                    buf = json_buf_data(&hand->decode_buf);
                    errno = 0;
                    d = strtod((const char *) buf, NULL);
                    if ((d == HUGE_VAL || d == -HUGE_VAL) && errno == ERANGE) {
                        json_bs_set(hand->state_stack, JSON_PARSER_STATE_PARSE_ERROR);
                        hand->parse_error = "numeric (floating point) "
                            "overflow";
                        /* try to restore error offset */
                        if (*offset >= buf_len) *offset -= buf_len;
                        else *offset = 0;
                        goto around_again;
                    }
                    _CC_CHK(hand->callbacks->xson_double(hand->ctx, d));
                }
            }
            break;
        case JSON_TOK_RIGHT_BRACE: {
            if (json_bs_current(hand->state_stack) ==
                JSON_PARSER_STATE_ARRAY_START)
            {
                if (hand->callbacks && hand->callbacks->xson_end_array) {
                    _CC_CHK(hand->callbacks->xson_end_array(hand->ctx));
                }
                json_bs_pop(hand->state_stack);
                goto around_again;
            }
            __attribute__ ((fallthrough));
        }
        case JSON_TOK_COLON:
        case JSON_TOK_COMMA:
        case JSON_TOK_RIGHT_BRACKET:
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_PARSE_ERROR);
            hand->parse_error = "unallowed token at this point in JSON text";
            goto around_again;
        default:
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_PARSE_ERROR);
            hand->parse_error = "invalid token, internal error";
            goto around_again;
        }
        /* got a value.  transition depends on the state we're in. */
        {
            json_parser_state_t s = json_bs_current(hand->state_stack);
            if (s == JSON_PARSER_STATE_START || s == JSON_PARSER_STATE_GOT_VALUE) {
                json_bs_set(hand->state_stack, JSON_PARSER_STATE_PARSE_COMPLETE);
            } else if (s == JSON_PARSER_STATE_MAP_NEED_VAL) {
                json_bs_set(hand->state_stack, JSON_PARSER_STATE_MAP_GOT_VAL);
            } else {
                json_bs_set(hand->state_stack, JSON_PARSER_STATE_ARRAY_GOT_VAL);
            }
        }
        if (state_to_push != JSON_PARSER_STATE_START) {
            json_bs_push(hand->state_stack, state_to_push);
        }

        goto around_again;
    }
    case JSON_PARSER_STATE_MAP_START:
    case JSON_PARSER_STATE_MAP_NEED_KEY: {
        /* only difference between these two states is that in
         * start '}' is valid, whereas in need_key, we've parsed
         * a comma, and a string key _must_ follow */
        tok = json_lex_lex(&hand->lexer, json_txt, json_txt_len, offset, &buf, &buf_len);
        switch (tok) {
        case JSON_TOK_EOF:
            return JSON_STATUS_OK;
        case JSON_TOK_ERROR:
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_LEXICAL_ERROR);
            goto around_again;
        case JSON_TOK_STRING_WITH_ESCAPES:
            if (hand->callbacks && hand->callbacks->xson_map_key) {
                json_buf_clear(&hand->decode_buf);
                json_string_decode(&hand->decode_buf, buf, buf_len);
                buf = json_buf_data(&hand->decode_buf);
                buf_len = json_buf_len(&hand->decode_buf);
            }
            /* intentional fall-through */
        case JSON_TOK_STRING:
            if (hand->callbacks && hand->callbacks->xson_map_key) {
                _CC_CHK(hand->callbacks->xson_map_key(hand->ctx, (const char *)buf, buf_len));
            }
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_MAP_SEP);
            goto around_again;
        case JSON_TOK_RIGHT_BRACKET:
            if (json_bs_current(hand->state_stack) ==
                JSON_PARSER_STATE_MAP_START)
            {
                if (hand->callbacks && hand->callbacks->xson_end_map) {
                    _CC_CHK(hand->callbacks->xson_end_map(hand->ctx));
                }
                json_bs_pop(hand->state_stack);
                goto around_again;
            }
            __attribute__ ((fallthrough));
        default:
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_PARSE_ERROR);
            hand->parse_error =
                "invalid object key (must be a string)";
            goto around_again;
        }
    }
    case JSON_PARSER_STATE_MAP_SEP: {
        tok = json_lex_lex(&hand->lexer, json_txt, json_txt_len, offset, &buf, &buf_len);
        switch (tok) {
        case JSON_TOK_COLON:
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_MAP_NEED_VAL);
            goto around_again;
        case JSON_TOK_EOF:
            return JSON_STATUS_OK;
        case JSON_TOK_ERROR:
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_LEXICAL_ERROR);
            goto around_again;
        default:
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_PARSE_ERROR);
            hand->parse_error = "object key and value must be separated by a colon (':')";
            goto around_again;
        }
    }
    case JSON_PARSER_STATE_MAP_GOT_VAL: {
        tok = json_lex_lex(&hand->lexer, json_txt, json_txt_len, offset, &buf, &buf_len);
        switch (tok) {
        case JSON_TOK_RIGHT_BRACKET:
            if (hand->callbacks && hand->callbacks->xson_end_map) {
                _CC_CHK(hand->callbacks->xson_end_map(hand->ctx));
            }
            json_bs_pop(hand->state_stack);
            goto around_again;
        case JSON_TOK_COMMA:
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_MAP_NEED_KEY);
            goto around_again;
        case JSON_TOK_EOF:
            return JSON_STATUS_OK;
        case JSON_TOK_ERROR:
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_LEXICAL_ERROR);
            goto around_again;
        default:
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_PARSE_ERROR);
            hand->parse_error = "after key and value, inside map, I expect ',' or '}'";
            /* try to restore error offset */
            if (*offset >= buf_len) *offset -= buf_len;
            else *offset = 0;
            goto around_again;
        }
    }
    case JSON_PARSER_STATE_ARRAY_GOT_VAL: {
        tok = json_lex_lex(&hand->lexer, json_txt, json_txt_len, offset, &buf, &buf_len);
        switch (tok) {
        case JSON_TOK_RIGHT_BRACE:
            if (hand->callbacks && hand->callbacks->xson_end_array) {
                _CC_CHK(hand->callbacks->xson_end_array(hand->ctx));
            }
            json_bs_pop(hand->state_stack);
            goto around_again;
        case JSON_TOK_COMMA:
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_ARRAY_NEED_VAL);
            goto around_again;
        case JSON_TOK_EOF:
            return JSON_STATUS_OK;
        case JSON_TOK_ERROR:
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_LEXICAL_ERROR);
            goto around_again;
        default:
            json_bs_set(hand->state_stack, JSON_PARSER_STATE_PARSE_ERROR);
            hand->parse_error = "after array element, I expect ',' or ']'";
            goto around_again;
        }
    }
    }

    abort();
    return JSON_STATUS_ERROR;
}

