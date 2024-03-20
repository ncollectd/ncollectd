// SPDX-License-Identifier: ISC OR BSD-2-Clause
// SPDX-FileCopyrightText: Copyright (c) 2002 Todd C. Miller
// SPDX-FileCopyrightText: Copyright (c) 2000 The NetBSD Foundation, Inc.
// SPDX-FileContributor: Todd C. Miller <Todd.Miller at courtesan.com>

// Sponsored in part by the Defense Advanced Research Projects
// Agency (DARPA) and Air Force Research Laboratory, Air Force
// Materiel Command, USAF, under agreement number F39502-99-1-0512.

// This code is derived from software contributed to The NetBSD Foundation
// by Dieter Baron and Thomas Klausner.

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "libutils/getopt_long.h"

int optreset;   /* reset getopt */

#define PRINT_ERROR    ((opterr) && (*options != ':'))

#define FLAG_PERMUTE   0x01    /* permute non-options to the end of argv */
#define FLAG_ALLARGS   0x02    /* treat non-options as args to option "-1" */
#define FLAG_LONGONLY  0x04    /* operate as getopt_long_only */

/* return values */
#define BADCH       (int)'?'
#define BADARG      ((*options == ':') ? (int)':' : (int)'?')
#define INORDER     (int)1

static char EMSG[] = "";

#define NO_PREFIX      (-1)
#define D_PREFIX       0
#define DD_PREFIX      1
#define W_PREFIX       2

static int getopt_internal(int, char **, const char *, const struct option *, int *, int);
static int parse_long_options(char * const *, const char *, const struct option *, int *, int, int);
static int gcd(int, int);
static void permute_args(int, int, int, char **);

static char *place = EMSG; /* option letter processing */

/* XXX: set optreset to 1 rather than these two */
static int nonopt_start = -1; /* first non option argument (for permute) */
static int nonopt_end = -1;   /* first option after non options (for permute) */

/* Error messages */
static const char recargchar[] = "option requires an argument -- %c";
static const char illoptchar[] = "illegal option -- %c"; /* From P1003.2 */
static int dash_prefix = NO_PREFIX;
static const char gnuoptchar[] = "invalid option -- %c";

static const char recargstring[] = "option `%s%s' requires an argument";
static const char ambig[] = "option `%s%.*s' is ambiguous";
static const char noarg[] = "option `%s%.*s' doesn't allow an argument";
static const char illoptstring[] = "unrecognized option `%s%s'";

/* Compute the greatest common divisor of a and b. */
static int gcd(int a, int b)
{
    int c = a % b;
    while (c != 0) {
        a = b;
        b = c;
        c = a % b;
    }

    return (b);
}

/*
 * Exchange the block from nonopt_start to nonopt_end with the block
 * from nonopt_end to opt_end (keeping the same order of arguments
 * in each block).
 */
static void permute_args(int panonopt_start, int panonopt_end, int opt_end, char **nargv)
{
    int cstart, cyclelen, i, j, ncycle, nnonopts, nopts, pos;
    char *swap;

   /*
    * compute lengths of blocks and number and size of cycles
    */
    nnonopts = panonopt_end - panonopt_start;
    nopts = opt_end - panonopt_end;
    ncycle = gcd(nnonopts, nopts);
    cyclelen = (opt_end - panonopt_start) / ncycle;

    for (i = 0; i < ncycle; i++) {
        cstart = panonopt_end+i;
        pos = cstart;
        for (j = 0; j < cyclelen; j++) {
            if (pos >= panonopt_end)
                pos -= nnonopts;
            else
                pos += nopts;
            swap = nargv[pos];
            ((char **) nargv)[pos] = nargv[cstart];
            ((char **)nargv)[cstart] = swap;
        }
    }
}

/*
 * parse_long_options Parse long options in argc/argv argument vector.
 * Returns -1 if short_too is set and the option does not match long_options.
 */
static int parse_long_options(char * const *nargv, const char *options,
                              const struct option *long_options, int *idx,
                              int short_too, int flags)
{
    char *has_equal;
    const char *current_dash;
    size_t current_argv_len;
    int i, match, exact_match, second_partial_match;

    char *current_argv = place;
    switch (dash_prefix) {
    case D_PREFIX:
        current_dash = "-";
        break;
    case DD_PREFIX:
        current_dash = "--";
        break;
    case W_PREFIX:
        current_dash = "-W ";
        break;
    default:
        current_dash = "";
        break;
    }
    match = -1;
    exact_match = 0;
    second_partial_match = 0;

    optind++;

    if ((has_equal = strchr(current_argv, '=')) != NULL) {
        /* argument found (--option=arg) */
        current_argv_len = has_equal - current_argv;
        has_equal++;
    } else
        current_argv_len = strlen(current_argv);

    for (i = 0; long_options[i].name; i++) {
        /* find matching long option */
        if (strncmp(current_argv, long_options[i].name, current_argv_len))
            continue;

        if (strlen(long_options[i].name) == current_argv_len) {
            /* exact match */
            match = i;
            exact_match = 1;
            break;
        }
        /*
     * If this is a known short option, don't allow
     * a partial match of a single character.
     */
        if (short_too && current_argv_len == 1)
            continue;

        if (match == -1) /* first partial match */
            match = i;
        else if ((flags & FLAG_LONGONLY) ||
                 long_options[i].has_arg != long_options[match].has_arg ||
                 long_options[i].flag != long_options[match].flag ||
                 long_options[i].val != long_options[match].val)
            second_partial_match = 1;
    }

    if (!exact_match && second_partial_match) {
        /* ambiguous abbreviation */
        if (PRINT_ERROR)
            fprintf(stderr, ambig, current_dash, (int)current_argv_len, current_argv);
        optopt = 0;
        return (BADCH);
    }

    if (match != -1) { /* option found */
        if (long_options[match].has_arg == no_argument && has_equal) {
            if (PRINT_ERROR)
                fprintf(stderr, noarg, current_dash, (int)current_argv_len, current_argv);
            /* XXX: GNU sets optopt to val regardless of flag */
            if (long_options[match].flag == NULL)
                optopt = long_options[match].val;
            else
                optopt = 0;
            return (BADCH);
        }

        if (long_options[match].has_arg == required_argument ||
            long_options[match].has_arg == optional_argument) {
            if (has_equal) {
                optarg = has_equal;
            } else if (long_options[match].has_arg == required_argument) {
                /*
             * optional argument doesn't use next nargv
             */
                optarg = nargv[optind++];
            }
        }

        if ((long_options[match].has_arg == required_argument) && (optarg == NULL)) {
            /* Missing argument; leading ':' indicates no errorshould be generated. */
            if (PRINT_ERROR)
                fprintf(stderr, recargstring, current_dash, current_argv);
            /* XXX: GNU sets optopt to val regardless of flag */
            if (long_options[match].flag == NULL)
                optopt = long_options[match].val;
            else
                optopt = 0;
            --optind;
            return (BADARG);
        }
    } else { /* unknown option */
        if (short_too) {
            --optind;
            return (-1);
        }
        if (PRINT_ERROR)
            fprintf(stderr, illoptstring, current_dash, current_argv);
        optopt = 0;
        return (BADCH);
    }
    if (idx)
        *idx = match;
    if (long_options[match].flag) {
        *long_options[match].flag = long_options[match].val;
        return (0);
    } else
        return (long_options[match].val);
}

/* getopt_internal Parse argc/argv argument vector.  Called by user level routines. */
static int getopt_internal(int nargc, char **nargv, const char *options,
                           const struct option *long_options, int *idx, int flags)
{
    char *oli;                  /* option letter list index */
    int optchar, short_too;
    static int posixly_correct = -1;

    if (options == NULL)
        return (-1);

   /*
    * XXX Some GNU programs (like cvs) set optind to 0 instead of
    * XXX using optreset.  Work around this braindamage.
    */
    if (optind == 0)
        optind = optreset = 1;

       /*
    * Disable GNU extensions if POSIXLY_CORRECT is set or options
    * string begins with a '+'.
    */
    if (posixly_correct == -1 || optreset)
        posixly_correct = (getenv("POSIXLY_CORRECT") != NULL);
    if (*options == '-')
        flags |= FLAG_ALLARGS;
    else if (posixly_correct || *options == '+')
        flags &= ~FLAG_PERMUTE;
    if (*options == '+' || *options == '-')
        options++;

    optarg = NULL;
    if (optreset)
        nonopt_start = nonopt_end = -1;
start:
    if (optreset || !*place) { /* update scanning pointer */
        optreset = 0;
        if (optind >= nargc) { /* end of argument vector */
            place = EMSG;
            if (nonopt_end != -1) {
                /* do permutation, if we have to */
                permute_args(nonopt_start, nonopt_end,
                optind, nargv);
                optind -= nonopt_end - nonopt_start;
            } else if (nonopt_start != -1) {
                /*
             * If we skipped non-options, set optind
             * to the first of them.
             */
                optind = nonopt_start;
            }
            nonopt_start = nonopt_end = -1;
            return (-1);
        }
        if (*(place = nargv[optind]) != '-' || place[1] == '\0') {
            place = EMSG;       /* found non-option */
            if (flags & FLAG_ALLARGS) {
                /*
             * GNU extension:
             * return non-option as argument to option 1
             */
                optarg = nargv[optind++];
                return (INORDER);
            }
            if (!(flags & FLAG_PERMUTE)) {
                /*
             * If no permutation wanted, stop parsing
             * at first non-option.
             */
                return (-1);
            }
            /* do permutation */
            if (nonopt_start == -1)
                nonopt_start = optind;
            else if (nonopt_end != -1) {
                permute_args(nonopt_start, nonopt_end,
                optind, nargv);
                nonopt_start = optind -
                (nonopt_end - nonopt_start);
                nonopt_end = -1;
            }
            optind++;
            /* process next argument */
            goto start;
        }
        if (nonopt_start != -1 && nonopt_end == -1)
            nonopt_end = optind;

    /*
     * If we have "-" do nothing, if "--" we are done.
     */
        if (place[1] != '\0' && *++place == '-' && place[1] == '\0') {
            optind++;
            place = EMSG;
            /*
         * We found an option (--), so if we skipped
         * non-options, we have to permute.
         */
            if (nonopt_end != -1) {
                permute_args(nonopt_start, nonopt_end,
                optind, nargv);
                optind -= nonopt_end - nonopt_start;
            }
            nonopt_start = nonopt_end = -1;
            return (-1);
        }
    }

   /*
    * Check long options if:
    *  1) we were passed some
    *  2) the arg is not just "-"
    *  3) either the arg starts with -- we are getopt_long_only()
    */
    if (long_options != NULL && place != nargv[optind] &&
        (*place == '-' || (flags & FLAG_LONGONLY))) {
        short_too = 0;
        dash_prefix = D_PREFIX;
        if (*place == '-') {
            place++;        /* --foo long option */
            if (*place == '\0')
                return (BADARG);    /* malformed option */
            dash_prefix = DD_PREFIX;
        } else if (*place != ':' && strchr(options, *place) != NULL)
            short_too = 1;      /* could be short option too */

        optchar = parse_long_options(nargv, options, long_options,
        idx, short_too, flags);
        if (optchar != -1) {
            place = EMSG;
            return (optchar);
        }
    }

    if ((optchar = (int)*place++) == (int)':' || (optchar == (int)'-' && *place != '\0') ||
        (oli = strchr(options, optchar)) == NULL) {
        /*
     * If the user specified "-" and  '-' isn't listed in
     * options, return -1 (non-option) as per POSIX.
     * Otherwise, it is an unknown option character (or ':').
     */
        if (optchar == (int)'-' && *place == '\0')
            return (-1);
        if (!*place)
            ++optind;
        if (PRINT_ERROR)
            fprintf(stderr, posixly_correct ? illoptchar : gnuoptchar, optchar);
        optopt = optchar;
        return (BADCH);
    }

    if (long_options != NULL && optchar == 'W' && oli[1] == ';') {
        /* -W long-option */
        if (*place) { /* no space */
            /* NOTHING */;
        } else if (++optind >= nargc) {   /* no arg */
            place = EMSG;
            if (PRINT_ERROR)
                fprintf(stderr, recargchar, optchar);
            optopt = optchar;
            return (BADARG);
        } else { /* white space */
            place = nargv[optind];
        }
        dash_prefix = W_PREFIX;
        optchar = parse_long_options(nargv, options, long_options,
        idx, 0, flags);
        place = EMSG;
        return (optchar);
    }
    if (*++oli != ':') { /* doesn't take argument */
        if (!*place)
            ++optind;
    } else {   /* takes (optional) argument */
        optarg = NULL;
        if (*place) { /* no white space */
            optarg = place;
        } else if (oli[1] != ':') {       /* arg not optional */
            if (++optind >= nargc) {    /* no arg */
                place = EMSG;
                if (PRINT_ERROR)
                    fprintf(stderr, recargchar, optchar);
                optopt = optchar;
                return (BADARG);
            } else {
                optarg = nargv[optind];
            }
        }
        place = EMSG;
        ++optind;
    }
    /* dump back option letter */
    return (optchar);
}

/* getopt_long Parse argc/argv argument vector. */
int getopt_long(int nargc, char **nargv, const char *options,
                const struct option *long_options, int *idx)
{
    return (getopt_internal(nargc, nargv, options, long_options, idx, FLAG_PERMUTE));
}

/* getopt_long_only Parse argc/argv argument vector. */
int getopt_long_only(int nargc, char **nargv, const char *options,
                     const struct option *long_options, int *idx)
{
    return (getopt_internal(nargc, nargv, options, long_options, idx, FLAG_PERMUTE|FLAG_LONGONLY));
}
