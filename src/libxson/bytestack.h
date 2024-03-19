/* SPDX-License-Identifier: GPL-2.0-only OR ISC                   */
/* SPDX-FileCopyrightText: Copyright (c) 2007-2014, Lloyd Hilaiel */
/* SPDX-FileContributor: Lloyd Hilaiel <me at lloyd.io>           */
#pragma once

/* A header only implementation of a simple stack of bytes, used in YAJL
 * to maintain parse state.  */
#include "libxson/common.h"

#define JSON_BS_INC 128

/* initialize a bytestack */
#define JSON_BS_INIT(obs) {    \
        (obs).stack = NULL;    \
        (obs).size = 0;        \
        (obs).used = 0;        \
    }

/* initialize a bytestack */
#define json_bs_free(obs)                 \
    if ((obs).stack) free((obs).stack);

#define json_bs_current(obs)               \
    (assert((obs).used > 0), (obs).stack[(obs).used - 1])

#define json_bs_push(obs, byte) {                       \
    if (((obs).size - (obs).used) == 0) {               \
        (obs).size += JSON_BS_INC;                      \
        (obs).stack = realloc((void *) (obs).stack, (obs).size);\
    }                                                   \
    (obs).stack[((obs).used)++] = (byte);               \
}

/* removes the top item of the stack, returns nothing */
#define json_bs_pop(obs) { ((obs).used)--; }

#define json_bs_set(obs, byte)                          \
    (obs).stack[((obs).used) - 1] = (byte);
