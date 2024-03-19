// SPDX-License-Identifier: GPL-2.0-only OR ISC
// Copyright (c) 2007-2014, Lloyd Hilaiel <me@lloyd.io>
// From  http://github.com/lloyd/yajl/encode.c

#include "libxson/encode.h"
#include "libxson/buf.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void hex2digit(unsigned int *val, const unsigned char *hex)
{
    unsigned int i;
    for (i=0;i<4;i++) {
        unsigned char c = hex[i];
        if (c >= 'A') c = (c & ~0x20) - 7;
        c -= '0';
        assert(!(c & 0xF0));
        *val = (*val << 4) | c;
    }
}

static void utf32toutf8(unsigned int codepoint, char *utf8_buf)
{
    if (codepoint < 0x80) {
        utf8_buf[0] = (char) codepoint;
        utf8_buf[1] = 0;
    } else if (codepoint < 0x0800) {
        utf8_buf[0] = (char) ((codepoint >> 6) | 0xC0);
        utf8_buf[1] = (char) ((codepoint & 0x3F) | 0x80);
        utf8_buf[2] = 0;
    } else if (codepoint < 0x10000) {
        utf8_buf[0] = (char) ((codepoint >> 12) | 0xE0);
        utf8_buf[1] = (char) (((codepoint >> 6) & 0x3F) | 0x80);
        utf8_buf[2] = (char) ((codepoint & 0x3F) | 0x80);
        utf8_buf[3] = 0;
    } else if (codepoint < 0x200000) {
        utf8_buf[0] =(char)((codepoint >> 18) | 0xF0);
        utf8_buf[1] =(char)(((codepoint >> 12) & 0x3F) | 0x80);
        utf8_buf[2] =(char)(((codepoint >> 6) & 0x3F) | 0x80);
        utf8_buf[3] =(char)((codepoint & 0x3F) | 0x80);
        utf8_buf[4] = 0;
    } else {
        utf8_buf[0] = '?';
        utf8_buf[1] = 0;
    }
}

void json_string_decode(json_buf_t *buf, const unsigned char * str, size_t len)
{
    size_t beg = 0;
    size_t end = 0;

    while (end < len) {
        if (str[end] == '\\') {
            char utf8_buf[5];
            char *unescaped = "?";
            json_buf_append(buf, str + beg, end - beg);
            switch (str[++end]) {
            case 'r':
                unescaped = "\r";
                break;
            case 'n':
                unescaped = "\n";
                break;
            case '\\':
                unescaped = "\\";
                break;
            case '/':
                unescaped = "/";
                break;
            case '"':
                unescaped = "\"";
                break;
            case 'f':
                unescaped = "\f";
                break;
            case 'b':
                unescaped = "\b";
                break;
            case 't':
                unescaped = "\t";
                break;
            case 'u': {
                unsigned int codepoint = 0;
                hex2digit(&codepoint, str + ++end);
                end+=3;
                /* check if this is a surrogate */
                if ((codepoint & 0xFC00) == 0xD800) {
                    end++;
                    if (str[end] == '\\' && str[end + 1] == 'u') {
                        unsigned int surrogate = 0;
                        hex2digit(&surrogate, str + end + 2);
                        codepoint = (((codepoint & 0x3F) << 10) |
                                    ((((codepoint >> 6) & 0xF) + 1) << 16) |
                                    (surrogate & 0x3FF));
                        end += 5;
                    } else {
                        unescaped = "?";
                        break;
                    }
                }

                utf32toutf8(codepoint, utf8_buf);
                unescaped = utf8_buf;

                if (codepoint == 0) {
                    json_buf_append(buf, unescaped, 1);
                    beg = ++end;
                    continue;
                }
            }   break;
#if 0
            default:
                assert("this should never happen" == NULL); // FIXME ¿?
                break;
#endif
            }
            json_buf_append(buf, unescaped, (unsigned int)strlen(unescaped));
            beg = ++end;
        } else {
            end++;
        }
    }
    json_buf_append(buf, str + beg, end - beg);
}

int json_string_validate_utf8(const unsigned char * s, size_t len)
{
    if (!len) return 1;
    if (!s) return 0;

    while (len--) {
        if (*s <= 0x7f) {
            /* single byte */
        } else if ((*s >> 5) == 0x6) {
            /* two byte */
            s++;
            if (!(len--))
                return 0;
            if (!((*s >> 6) == 0x2)) return 0;
        } else if ((*s >> 4) == 0x0e) {
            /* three byte */
            s++;
            if (!(len--))
                return 0;
            if (!((*s >> 6) == 0x2)) return 0;
            s++;
            if (!(len--))
                return 0;
            if (!((*s >> 6) == 0x2)) return 0;
        } else if ((*s >> 3) == 0x1e) {
            /* four byte */
            s++;
            if (!(len--))
                return 0;
            if (!((*s >> 6) == 0x2)) return 0;
            s++;
            if (!(len--))
                return 0;
            if (!((*s >> 6) == 0x2)) return 0;
            s++;
            if (!(len--))
                return 0;
            if (!((*s >> 6) == 0x2)) return 0;
        } else {
            return 0;
        }

        s++;
    }

    return 1;
}
