/* SPDX-License-Identifier: BSD-2-Clause                                  */
/* SPDX-FileCopyrightText: Copyright (c) 2000 The NetBSD Foundation, Inc. */

#pragma once

#include <sys/cdefs.h>

/*
 * GNU-like getopt_long()/getopt_long_only() with 4.4BSD optreset extension.
 * getopt() is declared here too for GNU programs.
 */
#define no_argument        0
#define required_argument  1
#define optional_argument  2

struct option {
    /* name of long option */
    const char *name;
    /* one of no_argument, required_argument, and optional_argument:
     * whether option takes an argument */
    int has_arg;
    /* if not NULL, set *flag to val when option found */
    int *flag;
    /* if flag not NULL, value to set *flag to; else return value */
    int val;
};

int getopt_long(int, char **, const char *, const struct option *, int *);
int getopt_long_only(int, char **, const char *, const struct option *, int *);
