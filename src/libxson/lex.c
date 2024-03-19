// SPDX-License-Identifier: GPL-2.0-only OR ISC
// Copyright (c) 2007-2014, Lloyd Hilaiel <me@lloyd.io>
// From  http://github.com/lloyd/yajl

#include "libxson/common.h"
#include "libxson/lex.h"
#include "libxson/buf.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#ifdef JSON_LEXER_DEBUG
static const char *json_tok2str(json_tok_t tok)
{
    switch (tok) {
        case JSON_TOK_BOOL:
            return "bool";
        case JSON_TOK_COLON:
            return "colon";
        case JSON_TOK_COMMA:
            return "comma";
        case JSON_TOK_EOF:
            return "eof";
        case JSON_TOK_ERROR:
            return "error";
        case JSON_TOK_LEFT_BRACE:
            return "brace";
        case JSON_TOK_LEFT_BRACKET:
            return "bracket";
        case JSON_TOK_NULL:
            return "null";
        case JSON_TOK_INTEGER:
            return "integer";
        case JSON_TOK_DOUBLE:
            return "double";
        case JSON_TOK_RIGHT_BRACE:
            return "brace";
        case JSON_TOK_RIGHT_BRACKET:
            return "bracket";
        case JSON_TOK_STRING:
            return "string";
        case JSON_TOK_STRING_with_escapes:
            return "string_with_escapes";
    }
    return "unknown";
}
#endif

/* Impact of the stream parsing feature on the lexer:
 *
 * YAJL support stream parsing.  That is, the ability to parse the first
 * bits of a chunk of JSON before the last bits are available (still on
 * the network or disk).  This makes the lexer more complex.  The
 * responsibility of the lexer is to handle transparently the case where
 * a chunk boundary falls in the middle of a token.  This is
 * accomplished is via a buffer and a character reading abstraction.
 *
 * Overview of implementation
 *
 * When we lex to end of input string before end of token is hit, we
 * copy all of the input text composing the token into our lexBuf.
 *
 * Every time we read a character, we do so through the read_char function.
 * read_char's responsibility is to handle pulling all chars from the buffer
 * before pulling chars from input text
 */

static inline unsigned char read_char(json_lexer_t *lexer, const unsigned char *txt, size_t *offset)
{
    if (lexer->buf_in_use && json_buf_len(&lexer->buf) &&
        (lexer->buff_offset < json_buf_len(&lexer->buf)))
        return *((const unsigned char *) json_buf_data(&lexer->buf) + (lexer->buff_offset)++);
    return txt[(*offset)++];
}
#if 0
#define read_char(lxr, txt, off)                      \
    (((lxr)->buf_in_use && json_buf_len(&(lxr)->buf) && lxr->buff_offset < json_buf_len(&(lxr)->buf)) ? \
     (*((const unsigned char *) json_buf_data(&(lxr)->buf) + ((lxr)->buff_offset)++)) : \
     ((txt)[(*(off))++]))
#endif

static inline void unread_char(json_lexer_t *lexer, size_t *offset)
{
     if (*offset > 0)
         (*offset)--;
     lexer->buff_offset--;
}
#if 0
#define unread_char(lxr, off) ((*(off) > 0) ? (*(off))-- : ((lxr)->buff_offset--))
#endif

void json_lexer_init(json_lexer_t *lexer, unsigned int validate_utf8)
{
    memset(lexer, 0, sizeof(*lexer));
    lexer->validate_utf8 = validate_utf8;
}

void json_lex_free(json_lexer_t *lxr)
{
    json_buf_free(&lxr->buf);
    return;
}

/* a lookup table which lets us quickly determine three things:
 * VEC - valid escaped control char
 * note.  the solidus '/' may be escaped or not.
 * IJC - invalid json char
 * VHC - valid hex char
 * NFP - needs further processing (from a string scanning perspective)
 * NUC - needs utf8 checking when enabled (from a string scanning perspective)
 */
#define VEC 0x01
#define IJC 0x02
#define VHC 0x04
#define NFP 0x08
#define NUC 0x10

static const char charLookupTable[256] =
{
/*00*/ IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    ,
/*08*/ IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    ,
/*10*/ IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    ,
/*18*/ IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    ,

/*20*/ 0      , 0      , NFP|VEC|IJC, 0      , 0      , 0      , 0      , 0      ,
/*28*/ 0      , 0      , 0      , 0      , 0      , 0      , 0      , VEC    ,
/*30*/ VHC    , VHC    , VHC    , VHC    , VHC    , VHC    , VHC    , VHC    ,
/*38*/ VHC    , VHC    , 0      , 0      , 0      , 0      , 0      , 0      ,

/*40*/ 0      , VHC    , VHC    , VHC    , VHC    , VHC    , VHC    , 0      ,
/*48*/ 0      , 0      , 0      , 0      , 0      , 0      , 0      , 0      ,
/*50*/ 0      , 0      , 0      , 0      , 0      , 0      , 0      , 0      ,
/*58*/ 0      , 0      , 0      , 0      , NFP|VEC|IJC, 0      , 0      , 0      ,

/*60*/ 0      , VHC    , VEC|VHC, VHC    , VHC    , VHC    , VEC|VHC, 0      ,
/*68*/ 0      , 0      , 0      , 0      , 0      , 0      , VEC    , 0      ,
/*70*/ 0      , 0      , VEC    , 0      , VEC    , 0      , 0      , 0      ,
/*78*/ 0      , 0      , 0      , 0      , 0      , 0      , 0      , 0      ,

       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,

       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,

       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,

       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC
};

/** process a variable length utf8 encoded codepoint.
 *
 *  returns:
 *    JSON_TOK_STRING - if valid utf8 char was parsed and offset was
 *                      advanced
 *    JSON_TOK_EOF - if end of input was hit before validation could
 *                   complete
 *    JSON_TOK_ERROR - if invalid utf8 was encountered
 *
 *  NOTE: on error the offset will point to the first char of the
 *  invalid utf8 */

static json_tok_t json_lex_utf8_char(json_lexer_t *lexer, const unsigned char *json_txt,
                                     size_t json_txt_len, size_t *offset, unsigned char c)
{
    if (c <= 0x7f) {
        /* single byte */
        return JSON_TOK_STRING;
    } else if ((c >> 5) == 0x6) {
        /* two byte */
        if (*offset >= json_txt_len)
            return JSON_TOK_EOF;
        c = read_char(lexer, json_txt, offset);
        if ((c >> 6) == 0x2) return JSON_TOK_STRING;
    } else if ((c >> 4) == 0x0e) {
        /* three byte */
        if (*offset >= json_txt_len)
            return JSON_TOK_EOF;
        c = read_char(lexer, json_txt, offset);
        if ((c >> 6) == 0x2) {
            if (*offset >= json_txt_len)
                return JSON_TOK_EOF;
            c = read_char(lexer, json_txt, offset);
            if ((c >> 6) == 0x2) return JSON_TOK_STRING;
        }
    } else if ((c >> 3) == 0x1e) {
        /* four byte */
        if (*offset >= json_txt_len)
            return JSON_TOK_EOF;
        c = read_char(lexer, json_txt, offset);
        if ((c >> 6) == 0x2) {
            if (*offset >= json_txt_len)
                return JSON_TOK_EOF;
            c = read_char(lexer, json_txt, offset);
            if ((c >> 6) == 0x2) {
                if (*offset >= json_txt_len)
                    return JSON_TOK_EOF;
                c = read_char(lexer, json_txt, offset);
                if ((c >> 6) == 0x2) return JSON_TOK_STRING;
            }
        }
    }

    return JSON_TOK_ERROR;
}

/* lex a string.  input is the lexer, pointer to beginning of
 * json text, and start of string (offset).
 * a token is returned which has the following meanings:
 * JSON_TOK_STRING: lex of string was successful.  offset points to
 *                  terminating '"'.
 * JSON_TOK_EOF: end of text was encountered before we could complete
 *               the lex.
 * JSON_TOK_ERROR: embedded in the string were unallowable chars.  offset
 *               points to the offending char
 */

/** scan a string for interesting characters that might need further
 *  review.  return the number of chars that are uninteresting and can
 *  be skipped.
 * (lth) hi world, any thoughts on how to make this routine faster? */
static size_t json_string_scan(const unsigned char *buf, size_t len, int utf8_check)
{
    unsigned char mask = IJC|NFP|(utf8_check ? NUC : 0);
    size_t skip = 0;

    while (skip < len && !(charLookupTable[*buf] & mask)) {
        skip++;
        buf++;
    }
    return skip;
}

static json_tok_t json_lex_string(json_lexer_t *lexer, const unsigned char * json_txt,
                                  size_t json_txt_len, size_t * offset)
{
    json_tok_t tok = JSON_TOK_ERROR;
    int has_escapes = 0;

    for (;;) {
        unsigned char c;

        /* now jump into a faster scanning routine to skip as much
         * of the buffers as possible */
        {
            const unsigned char * p;
            size_t len;

            if ((lexer->buf_in_use && json_buf_len(&lexer->buf) &&
                 lexer->buff_offset < json_buf_len(&lexer->buf))) {
                p = ((const unsigned char *) json_buf_data(&lexer->buf) +
                     (lexer->buff_offset));
                len = json_buf_len(&lexer->buf) - lexer->buff_offset;
                lexer->buff_offset += json_string_scan(p, len, lexer->validate_utf8);
            } else if (*offset < json_txt_len) {
                p = json_txt + *offset;
                len = json_txt_len - *offset;
                *offset += json_string_scan(p, len, lexer->validate_utf8);
            }
        }

        if (*offset >= json_txt_len) {
            tok = JSON_TOK_EOF;
            goto finish_string_lex;
        }

        c = read_char(lexer, json_txt, offset);

        /* quote terminates */
        if (c == '"') {
            tok = JSON_TOK_STRING;
            break;
        } else if (c == '\\') {
            /* backslash escapes a set of control chars, */
            has_escapes = 1;
            if (*offset >= json_txt_len) {
                tok = JSON_TOK_EOF;
                goto finish_string_lex;
            }

            /* special case \u */
            c = read_char(lexer, json_txt, offset);
            if (c == 'u') {
                unsigned int i = 0;

                for (i=0;i<4;i++) {
                    if (*offset >= json_txt_len) {
                        tok = JSON_TOK_EOF;
                        goto finish_string_lex;
                    }
                    c = read_char(lexer, json_txt, offset);
                    if (!(charLookupTable[c] & VHC)) {
                        /* back up to offending char */
                        unread_char(lexer, offset);
                        lexer->error = JSON_LEX_STRING_INVALID_HEX_CHAR;
                        goto finish_string_lex;
                    }
                }
            } else if (!(charLookupTable[c] & VEC)) {
                /* back up to offending char */
                unread_char(lexer, offset);
                lexer->error = JSON_LEX_STRING_INVALID_ESCAPED_CHAR;
                goto finish_string_lex;
            }
        } else if(charLookupTable[c] & IJC) {
            /* when not validating UTF8 it's a simple table lookup to determine
             * if the present character is invalid */
            /* back up to offending char */
            unread_char(lexer, offset);
            lexer->error = JSON_LEX_STRING_INVALID_JSON_CHAR;
            goto finish_string_lex;
        } else if (lexer->validate_utf8) {
            /* when in validate UTF8 mode we need to do some extra work */
            json_tok_t t = json_lex_utf8_char(lexer, json_txt, json_txt_len, offset, c);

            if (t == JSON_TOK_EOF) {
                tok = JSON_TOK_EOF;
                goto finish_string_lex;
            } else if (t == JSON_TOK_ERROR) {
                lexer->error = JSON_LEX_STRING_INVALID_UTF8;
                goto finish_string_lex;
            }
        }
        /* accept it, and move on */
    }
  finish_string_lex:
    /* tell our buddy, the parser, wether he needs to process this string
     * again */
    if (has_escapes && tok == JSON_TOK_STRING)
        tok = JSON_TOK_STRING_WITH_ESCAPES;

    return tok;
}

static json_tok_t json_lex_number(json_lexer_t *lexer, const unsigned char *json_txt,
                                  size_t json_txt_len, size_t *offset)
{
    /** XXX: numbers are the only entities in json that we must lex
     *       _beyond_ in order to know that they are complete.  There
     *       is an ambiguous case for integers at EOF. */


    json_tok_t tok = JSON_TOK_INTEGER;

    if (*offset >= json_txt_len)
        return JSON_TOK_EOF;

    unsigned char c = read_char(lexer, json_txt, offset);

    /* optional leading minus */
    if (c == '-') {
        if (*offset >= json_txt_len)
            return JSON_TOK_EOF;
        c = read_char(lexer, json_txt, offset);
    }

    /* a single zero, or a series of integers */
    if (c == '0') {
        if (*offset >= json_txt_len)
            return JSON_TOK_EOF;
        c = read_char(lexer, json_txt, offset);
    } else if (c >= '1' && c <= '9') {
        do {
            if (*offset >= json_txt_len)
                return JSON_TOK_EOF;
            c = read_char(lexer, json_txt, offset);
        } while (c >= '0' && c <= '9');
    } else {
        unread_char(lexer, offset);
        lexer->error = JSON_LEX_MISSING_INTEGER_AFTER_MINUS;
        return JSON_TOK_ERROR;
    }

    /* optional fraction (indicates this is floating point) */
    if (c == '.') {
        int numRd = 0;

        if (*offset >= json_txt_len)
            return JSON_TOK_EOF;
        c = read_char(lexer, json_txt, offset);

        while (c >= '0' && c <= '9') {
            numRd++;
            if (*offset >= json_txt_len)
                return JSON_TOK_EOF;
            c = read_char(lexer, json_txt, offset);
        }

        if (!numRd) {
            unread_char(lexer, offset);
            lexer->error = JSON_LEX_MISSING_INTEGER_AFTER_DECIMAL;
            return JSON_TOK_ERROR;
        }
        tok = JSON_TOK_DOUBLE;
    }

    /* optional exponent (indicates this is floating point) */
    if (c == 'e' || c == 'E') {
        if (*offset >= json_txt_len)
            return JSON_TOK_EOF;
        c = read_char(lexer, json_txt, offset);

        /* optional sign */
        if (c == '+' || c == '-') {
            if (*offset >= json_txt_len)
                return JSON_TOK_EOF;
            c = read_char(lexer, json_txt, offset);
        }

        if (c >= '0' && c <= '9') {
            do {
                if (*offset >= json_txt_len)
                    return JSON_TOK_EOF;
                c = read_char(lexer, json_txt, offset);
            } while (c >= '0' && c <= '9');
        } else {
            unread_char(lexer, offset);
            lexer->error = JSON_LEX_MISSING_INTEGER_AFTER_EXPONENT;
            return JSON_TOK_ERROR;
        }
        tok = JSON_TOK_DOUBLE;
    }

    /* we always go "one too far" */
    unread_char(lexer, offset);

    return tok;
}

json_tok_t json_lex_lex(json_lexer_t *lexer, const unsigned char *json_txt,
                        size_t json_txt_len, size_t *offset,
                        const unsigned char **out_buf, size_t *out_len)
{
    json_tok_t tok = JSON_TOK_ERROR;
    unsigned char c;
    size_t start_offset = *offset;

    *out_buf = NULL;
    *out_len = 0;

    for (;;) {
        assert(*offset <= json_txt_len);

        if (*offset >= json_txt_len) {
            tok = JSON_TOK_EOF;
            goto lexed;
        }

        c = read_char(lexer, json_txt, offset);

        switch (c) {
        case '{':
            tok = JSON_TOK_LEFT_BRACKET;
            goto lexed;
        case '}':
            tok = JSON_TOK_RIGHT_BRACKET;
            goto lexed;
        case '[':
            tok = JSON_TOK_LEFT_BRACE;
            goto lexed;
        case ']':
            tok = JSON_TOK_RIGHT_BRACE;
            goto lexed;
        case ',':
            tok = JSON_TOK_COMMA;
            goto lexed;
        case ':':
            tok = JSON_TOK_COLON;
            goto lexed;
        case '\t': case '\n': case '\v': case '\f': case '\r': case ' ':
            start_offset++;
            break;
        case 't': {
            const char * want = "rue";
            do {
                if (*offset >= json_txt_len) {
                    tok = JSON_TOK_EOF;
                    goto lexed;
                }
                c = read_char(lexer, json_txt, offset);
                if (c != *want) {
                    unread_char(lexer, offset);
                    lexer->error = JSON_LEX_INVALID_STRING;
                    tok = JSON_TOK_ERROR;
                    goto lexed;
                }
            } while (*(++want));
            tok = JSON_TOK_BOOL;
            goto lexed;
        }
        case 'f': {
            const char * want = "alse";
            do {
                if (*offset >= json_txt_len) {
                    tok = JSON_TOK_EOF;
                    goto lexed;
                }
                c = read_char(lexer, json_txt, offset);
                if (c != *want) {
                    unread_char(lexer, offset);
                    lexer->error = JSON_LEX_INVALID_STRING;
                    tok = JSON_TOK_ERROR;
                    goto lexed;
                }
            } while (*(++want));
            tok = JSON_TOK_BOOL;
            goto lexed;
        }
        case 'n': {
            const char * want = "ull";
            do {
                if (*offset >= json_txt_len) {
                    tok = JSON_TOK_EOF;
                    goto lexed;
                }
                c = read_char(lexer, json_txt, offset);
                if (c != *want) {
                    unread_char(lexer, offset);
                    lexer->error = JSON_LEX_INVALID_STRING;
                    tok = JSON_TOK_ERROR;
                    goto lexed;
                }
            } while (*(++want));
            tok = JSON_TOK_NULL;
            goto lexed;
        }
        case '"': {
            tok = json_lex_string(lexer, (const unsigned char *) json_txt, json_txt_len, offset);
            goto lexed;
        }
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9': {
            /* integer parsing wants to start from the beginning */
            unread_char(lexer, offset);
            tok = json_lex_number(lexer, (const unsigned char *) json_txt, json_txt_len, offset);
            goto lexed;
        }
        default:
            lexer->error = JSON_LEX_INVALID_CHAR;
            tok = JSON_TOK_ERROR;
            goto lexed;
        }
    }


lexed:
    /* need to append to buffer if the buffer is in use or
     * if it's an EOF token */
    if (tok == JSON_TOK_EOF || lexer->buf_in_use) {
        if (!lexer->buf_in_use) json_buf_clear(&lexer->buf);
        lexer->buf_in_use = 1;
        json_buf_append(&lexer->buf, json_txt + start_offset, *offset - start_offset);
        lexer->buff_offset = 0;

        if (tok != JSON_TOK_EOF) {
            *out_buf = json_buf_data(&lexer->buf);
            *out_len = json_buf_len(&lexer->buf);
            lexer->buf_in_use = 0;
        }
    } else if (tok != JSON_TOK_ERROR) {
        *out_buf = json_txt + start_offset;
        *out_len = *offset - start_offset;
    }

    /* special case for strings. skip the quotes. */
    if (tok == JSON_TOK_STRING || tok == JSON_TOK_STRING_WITH_ESCAPES) {
        assert(*out_len >= 2);
        (*out_buf)++;
        *out_len -= 2;
    }


#ifdef JSON_LEXER_DEBUG
    if (tok == JSON_TOK_ERROR) {
        printf("lexical error: %s\n", json_lex_error_to_string(json_lex_get_error(lexer)));
    } else if (tok == JSON_TOK_EOF) {
        printf("EOF hit\n");
    } else {
        printf("lexed %s: '", json_tok2str(tok));
        fwrite(*out_buf, 1, *out_len, stdout);
        printf("'\n");
    }
#endif

    return tok;
}

const char *json_lex_error_to_string(json_lex_error_t error)
{
    switch (error) {
        case JSON_LEX_E_OK:
            return "ok, no error";
        case JSON_LEX_STRING_INVALID_UTF8:
            return "invalid bytes in UTF8 string.";
        case JSON_LEX_STRING_INVALID_ESCAPED_CHAR:
            return "inside a string, '\\' occurs before a character which it may not.";
        case JSON_LEX_STRING_INVALID_JSON_CHAR:
            return "invalid character inside string.";
        case JSON_LEX_STRING_INVALID_HEX_CHAR:
            return "invalid (non-hex) character occurs after '\\u' inside string.";
        case JSON_LEX_INVALID_CHAR:
            return "invalid char in json text.";
        case JSON_LEX_INVALID_STRING:
            return "invalid string in json text.";
        case JSON_LEX_MISSING_INTEGER_AFTER_EXPONENT:
            return "malformed number, a digit is required after the exponent.";
        case JSON_LEX_MISSING_INTEGER_AFTER_DECIMAL:
            return "malformed number, a digit is required after the decimal point.";
        case JSON_LEX_MISSING_INTEGER_AFTER_MINUS:
            return "malformed number, a digit is required after the minus sign.";
    }
    return "unknown error code";
}


/** allows access to more specific information about the lexical
 *  error when json_lex_lex returns JSON_TOK_ERROR. */
json_lex_error_t json_lex_get_error(json_lexer_t *lexer)
{
    if (lexer == NULL) return (json_lex_error_t) -1;
    return lexer->error;
}

size_t json_lex_current_line(json_lexer_t *lexer)
{
    return lexer->line_offset;
}

size_t json_lex_current_char(json_lexer_t *lexer)
{
    return lexer->char_offset;
}

json_tok_t json_lex_peek(json_lexer_t *lexer, const unsigned char * json_txt,
                       size_t json_txt_len, size_t offset)
{
    const unsigned char *out_buf;
    size_t out_len;
    size_t buf_len = json_buf_len(&lexer->buf);
    size_t buff_offset = lexer->buff_offset;
    unsigned int buf_in_use = lexer->buf_in_use;

    json_tok_t tok = json_lex_lex(lexer, json_txt, json_txt_len, &offset, &out_buf, &out_len);

    lexer->buff_offset = buff_offset;
    lexer->buf_in_use = buf_in_use;
    json_buf_truncate(&lexer->buf, buf_len);

    return tok;
}
