// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

// [-+]?([0-9]*(\.[0-9]*)?[a-z]+)+
int64_t mql_parse_duration (char *str)
{
    char *ptr;
    bool negative = false;
    int64_t lscale = -1;
    int64_t duration = 0;

    ptr = str;
    if ((*ptr == '-') || (*ptr == '+')) {
        negative = *ptr == '-';
        ptr++;
    }

    if ((*ptr == '0') && (ptr[1] == '\0')) {
        return 0;
    }

    while (ptr[0] != '\0') {

        int64_t whole = 0;;
        int64_t nwhole = 0;
        while ((ptr[0] >= '0') && (ptr[0] <= '9')) {
            whole = whole * 10 + (ptr[0] - '0');
            nwhole++;
            ptr++;
        }

        int64_t decimal = 0;
        int64_t sdecimal = 1;
        if (ptr[0] == '.') {
            ptr++;
            while ((ptr[0] >= '0') && (ptr[0] <= '9')) {
                decimal = decimal * 10 + (ptr[0] - '0');
                sdecimal *= 10;
                ptr++;
            }
        }

        if ((nwhole == 0) && (sdecimal == 1)) {
            // *error = EINVAL;
        }

        int64_t scale = 0;
        if (ptr[0] == 'm') {
            if (ptr[1] == 's') {
                scale = 1;
                ptr+=2;
            } else {
                scale = 60 * 1000L;
                ptr+=1;
            }
        } else if (ptr[0] == 's') {
            scale = 1000L;
            ptr+=1;
        } else if (ptr[0] == 'h') {
            scale = 3600 * 1000L;
            ptr+=1;
        } else if (ptr[0] == 'd') {
            scale = 24 * 3600 * 1000L;
            ptr+=1;
        } else if (ptr[0] == 'w') {
            scale = 7 * 24 * 3600 * 1000L;
            ptr+=1;
        } else if (ptr[0] == 'y') {
            scale = 365 * 24 * 3600 * (int64_t)1000L;
            ptr+=1;
        } else {
            // *error = EINVAL;
        }

        if ((lscale > 0) && (scale >= lscale)) {
            // *error = ERANGE;
        }
        lscale = scale;

        whole *= scale;
        if (decimal > 0) {
            whole += (int64_t)((double)decimal * (double)((scale)/sdecimal));
            if (whole < 0) { // overflow
                // *error = ERANGE;
            }
        }

        duration += whole;
        if (duration < 0) {
            // *error = ERANGE;
        }
    }

    return negative ? -duration : duration;
}





char *mql_unquote(char *str)
{
    size_t len = strlen(str);
    if (len < 2) {
        return NULL;
    }

    char quote = *str;
    if (quote == '\'') {
        if (str[len-1] == quote) {
            str[len-1] = '\0';
            str++;
            return strdup(str);
        } else {
            return NULL;
        }
    }

    if (quote != '"')
        return NULL;
    if (str[len-1] != quote)
        return NULL;

    str[len-1] = '\0';
    str++;

    char *dstr = malloc(sizeof(char)*(len-1));
    if (dstr == NULL) {
        return NULL;
    }

    char *c = str;
    char *d = dstr;

    while (*c != '\0') {
        if (*c == '\\') {
            c++;
            if (*c == '\0')
                break;
            switch (*c) {
                case 'a':
                        *d = '\a';
                        break;
                case 'b':
                        *d = '\b';
                        break;
                case 'f':
                        *d = '\f';
                        break;
                case 'n':
                        *d = '\n';
                        break;
                case 'r':
                        *d = '\r';
                        break;
                case 't':
                        *d = '\t';
                        break;
                case 'v':
                        *d = '\v';
                        break;
                case '\\':
                        *d = '\\';
                        break;
                case '"':
                        *d = '"';
                        break;
                default:
                        *d = *c;
                        break;
            }
        } else {
            *d = *c;
        }
        c++;
        d++;
    }
    *d = '\0';

    return dstr;
}

static char mql_chars[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 2, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 4,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#define MQL_LABEL_ALPHA            (1|4)
#define MQL_LABEL_ALPHANUM     (1|4|8)
#define MQL_METRIC_ALPHA           (1|4|2)
#define MQL_METRIC_ALPHANUM    (1|4|2|8)

/* Label names must match the regex `[a-zA-Z_][a-zA-Z0-9_]*`.
 * Label names beginning with __ are reserved for internal use. */
bool mql_islabel(char *str)
{
    if (str == NULL)
        return false;

    if (str[0] == '\0')
        return  false;

    if (!(mql_chars[(int)str[0]] & MQL_LABEL_ALPHA))
        return false;

    size_t i=1;
    while(str[i] != '\0') {
        if (!(mql_chars[(int)str[i]] & MQL_LABEL_ALPHANUM))
            return false;
        i++;
    }

    return true;
}

/* Metric names must match the regex `[a-zA-Z_:][a-zA-Z0-9_:]*` */
bool mql_ismetric(char *str)
{
    if (str == NULL)
        return false;

    if (str[0] == '\0')
        return  false;

    if (!(mql_chars[(int)str[0]] & MQL_METRIC_ALPHA))
        return false;

    size_t i=1;
    while(str[i] != '\0') {
        if (!(mql_chars[(int)str[i]] & MQL_METRIC_ALPHANUM))
            return false;
        i++;
    }

    return true;
}
