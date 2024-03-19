// SPDX-License-Identifier: GPL-2.0-only OR BSD-3-Clause
// SPDX-FileCopyrightText: Copyright (C) 1993, 1994 Thomas Koenig
// SPDX-FileCopyrightText: Copyright (C) 1993 David Parsons
// SPDX-FileCopyrightText: Copyright (C) 1999 Oleg Cherevko (aka Olwi Deer)
// SPDX-FileCopyrightText: Copyright (C) 1999 Tobi Oetiker
// SPDX-FileContributor: Thomas Koenig
// SPDX-FileContributor: David Parsons
// SPDX-FileContributor: Oleg Cherevko (aka Olwi Deer)
// SPDX-FileContributor: Tobi Oetiker

/*
 * The BNF-like specification of the time syntax parsed is below:
 *
 * As usual, [ X ] means that X is optional, { X } means that X may
 * be either omitted or specified as many times as needed,
 * alternatives are separated by |, brackets are used for grouping.
 * (# marks the beginning of comment that extends to the end of line)
 *
 * TIME-SPECIFICATION ::= TIME-REFERENCE [ OFFSET-SPEC ] |
 *                               OFFSET-SPEC   |
 *               ( START | END ) OFFSET-SPEC
 *
 * TIME-REFERENCE ::= NOW | TIME-OF-DAY-SPEC [ DAY-SPEC-1 ] |
 *                        [ TIME-OF-DAY-SPEC ] DAY-SPEC-2
 *
 * TIME-OF-DAY-SPEC ::= NUMBER (':') NUMBER [am|pm] | # HH:MM
 *                     'noon' | 'midnight' | 'teatime'
 *
 * DAY-SPEC-1 ::= NUMBER '/' NUMBER '/' NUMBER |  # MM/DD/[YY]YY
 *                NUMBER '.' NUMBER '.' NUMBER |  # DD.MM.[YY]YY
 *                NUMBER                          # Seconds since 1970
 *                NUMBER                          # YYYYMMDD
 *
 * DAY-SPEC-2 ::= MONTH-NAME NUMBER [NUMBER] |    # Month DD [YY]YY
 *                'yesterday' | 'today' | 'tomorrow' |
 *                DAY-OF-WEEK
 *
 *
 * OFFSET-SPEC ::= '+'|'-' NUMBER TIME-UNIT { ['+'|'-'] NUMBER TIME-UNIT }
 *
 * TIME-UNIT ::= SECONDS | MINUTES | HOURS |
 *               DAYS | WEEKS | MONTHS | YEARS
 *
 * NOW ::= 'now' | 'n'
 *
 * START ::= 'start' | 's'
 * END   ::= 'end' | 'e'
 *
 * SECONDS ::= 'seconds' | 'second' | 'sec' | 's'
 * MINUTES ::= 'minutes' | 'minute' | 'min' | 'm'
 * HOURS   ::= 'hours' | 'hour' | 'hr' | 'h'
 * DAYS    ::= 'days' | 'day' | 'd'
 * WEEKS   ::= 'weeks' | 'week' | 'wk' | 'w'
 * MONTHS  ::= 'months' | 'month' | 'mon' | 'm'
 * YEARS   ::= 'years' | 'year' | 'yr' | 'y'
 *
 * MONTH-NAME ::= 'jan' | 'january' | 'feb' | 'february' | 'mar' | 'march' |
 *                'apr' | 'april' | 'may' | 'jun' | 'june' | 'jul' | 'july' |
 *                'aug' | 'august' | 'sep' | 'september' | 'oct' | 'october' |
 *          'nov' | 'november' | 'dec' | 'december'
 *
 * DAY-OF-WEEK ::= 'sunday' | 'sun' | 'monday' | 'mon' | 'tuesday' | 'tue' |
 *                 'wednesday' | 'wed' | 'thursday' | 'thu' | 'friday' | 'fri' |
 *                 'saturday' | 'sat'
 *
 *
 * As you may note, there is an ambiguity with respect to
 * the 'm' time unit (which can mean either minutes or months).
 * To cope with this, code tries to read users mind :) by applying
 * certain heuristics. There are two of them:
 *
 * 1. If 'm' is used in context of (i.e. right after the) years,
 *    months, weeks, or days it is assumed to mean months, while
 *    in the context of hours, minutes, and seconds it means minutes.
 *    (e.g., in -1y6m or +3w1m 'm' means 'months', while in
 *    -3h20m or +5s2m 'm' means 'minutes')
 *
 * 2. Out of context (i.e. right after the '+' or '-' sign) the
 *    meaning of 'm' is guessed from the number it directly follows.
 *    Currently, if the number absolute value is below 6 it is assumed
 *    that 'm' means months, otherwise it is treated as minutes.
 *    (e.g., -6m == -6 minutes, while +5m == +5 months)
 *
 */

#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define TIME_OK NULL

typedef enum {
    ABSOLUTE_TIME,
    RELATIVE_TO_START_TIME,
    RELATIVE_TO_END_TIME,
    RELATIVE_TO_EPOCH
} rrd_timetype_t;

typedef struct rrd_time_value {
    rrd_timetype_t type;
    long offset;
    struct tm tm;
} rrd_time_value_t;

enum { /* symbols */
    MIDNIGHT,
    NOON,
    TEATIME,
    PM,
    AM,
    YESTERDAY,
    TODAY,
    TOMORROW,
    NOW,
    START,
    END,
    EPOCH,
    SECONDS,
    MINUTES,
    HOURS,
    DAYS,
    WEEKS,
    MONTHS,
    YEARS,
    MONTHS_MINUTES,
    NUMBER,
    PLUS,
    MINUS,
    DOT,
    COLON,
    SLASH,
    ID,
    JUNK,
    JAN,
    FEB,
    MAR,
    APR,
    MAY,
    JUN,
    JUL,
    AUG,
    SEP,
    OCT,
    NOV,
    DEC,
    SUN,
    MON,
    TUE,
    WED,
    THU,
    FRI,
    SAT
};

/* the below is for plus_minus() */
#define PREVIOUS_OP    (-1)

/* parse translation table - table driven parsers can be your FRIEND!
 */
typedef struct {
    char     *name;  /* token name */
    int       value; /* token id */
} special_token_t;

static const special_token_t various_words[] = {
    {" midnight",  MIDNIGHT  }, /* 00:00:00 of today or tomorrow */
    { "noon",      NOON      }, /* 12:00:00 of today or tomorrow */
    { "teatime",   TEATIME   }, /* 16:00:00 of today or tomorrow */
    { "am",        AM        }, /* morning times for 0-12 clock */
    { "pm",        PM        }, /* evening times for 0-12 clock */
    { "tomorrow",  TOMORROW  },
    { "yesterday", YESTERDAY },
    { "today",     TODAY     },
    { "now",       NOW       },
    { "n",         NOW       },
    { "start",     START     },
    { "s",         START     },
    { "end",       END       },
    { "e",         END       },
    { "epoch",     EPOCH     },
    { "jan",       JAN       },
    { "feb",       FEB       },
    { "mar",       MAR       },
    { "apr",       APR       },
    { "may",       MAY       },
    { "jun",       JUN       },
    { "jul",       JUL       },
    { "aug",       AUG       },
    { "sep",       SEP       },
    { "oct",       OCT       },
    { "nov",       NOV       },
    { "dec",       DEC       },
    { "january",   JAN       },
    { "february",  FEB       },
    { "march",     MAR       },
    { "april",     APR       },
    { "may",       MAY       },
    { "june",      JUN       },
    { "july",      JUL       },
    { "august",    AUG       },
    { "september", SEP       },
    { "october",   OCT       },
    { "november",  NOV       },
    { "december",  DEC       },
    { "sunday",    SUN       },
    { "sun",       SUN       },
    { "monday",    MON       },
    { "mon",       MON       },
    { "tuesday",   TUE       },
    { "tue",       TUE       },
    { "wednesday", WED       },
    { "wed",       WED       },
    { "thursday",  THU       },
    { "thu",       THU       },
    { "friday",    FRI       },
    { "fri",       FRI       },
    { "saturday",  SAT       },
    { "sat",       SAT       },
    { NULL,        0         } /*** SENTINEL ***/
};

static const special_token_t time_multipliers[] = {
    { "second",  SECONDS        }, /* seconds multiplier */
    { "seconds", SECONDS        }, /* (pluralized) */
    { "sec",     SECONDS        }, /* (generic) */
    { "s",       SECONDS        }, /* (short generic) */
    { "minute",  MINUTES        }, /* minutes multiplier */
    { "minutes", MINUTES        }, /* (pluralized) */
    { "min",     MINUTES        }, /* (generic) */
    { "m",       MONTHS_MINUTES }, /* (short generic) */
    { "hour",    HOURS          }, /* hours ... */
    { "hours",   HOURS          }, /* (pluralized) */
    { "hr",      HOURS          }, /* (generic) */
    { "h",       HOURS          }, /* (short generic) */
    { "day",     DAYS           }, /* days ... */
    { "days",    DAYS           }, /* (pluralized) */
    { "d",       DAYS           }, /* (short generic) */
    { "week",    WEEKS          }, /* week ... */
    { "weeks",   WEEKS          }, /* (pluralized) */
    { "wk",      WEEKS          }, /* (generic) */
    { "w",       WEEKS          }, /* (short generic) */
    { "month",   MONTHS         }, /* week ... */
    { "months",  MONTHS         }, /* (pluralized) */
    { "mon",     MONTHS         }, /* (generic) */
    { "year",    YEARS          }, /* year ... */
    { "years",   YEARS          }, /* (pluralized) */
    { "yr",      YEARS          }, /* (generic) */
    { "y",       YEARS          }, /* (short generic) */
    { NULL,      0              }  /*** SENTINEL ***/
};

#define MAX_ERR_MSG_LEN    256
typedef struct {
    /* context dependent list of specials for parser to recognize,
       required for us to be able distinguish between 'mon' as 'month'
       and 'mon' as 'monday' */
    const special_token_t *specials;
    const char **scp; /* scanner - pointer at arglist */
    char scc;         /* scanner - count of remaining arguments */
    const char *sct;  /* scanner - next char pointer in current argument */
    int need;         /* scanner - need to advance to next argument */
    char *sc_token;   /* scanner - token buffer */
    size_t sc_len;    /* scanner - length of token buffer */
    int sc_tokid;     /* scanner - token id */
    char errmsg[MAX_ERR_MSG_LEN];
} pase_time_ctx_t;

/* Local functions */
static void ensure_mem_free(pase_time_ctx_t *ctx)
{
    if (ctx->sc_token) {
        free(ctx->sc_token);
        ctx->sc_token = NULL;
    }
}

/*
 * A hack to compensate for the lack of the C++ exceptions
 *
 * Every function func that might generate parsing "exception"
 * should return TIME_OK (aka NULL) or pointer to the error message,
 * and should be called like this: try(func(args));
 *
 * if the try is not successful it will reset the token pointer ...
 *
 * [NOTE: when try(...) is used as the only statement in the "if-true"
 *  part of the if statement that also has an "else" part it should be
 *  either enclosed in the curly braces (despite the fact that it looks
 *  like a single statement) or NOT followed by the ";"]
 */
#define try(c, b)    { \
        char *_e; \
        if((_e=(b))) { \
            ensure_mem_free((c)); \
            return _e; \
        } \
    }

/*
 * The panic() function was used in the original code to die, we redefine
 * it as macro to start the chain of ascending returns that in conjunction
 * with the try(b) above will simulate a sort of "exception handling"
 */

#define panic(e)    { return (e); }

/*
 * ve() and e() are used to set the return error,
 * the most appropriate use for these is inside panic(...)
 */

static char *ve(pase_time_ctx_t *ctx, char *fmt, va_list ap)
{
#ifdef HAVE_VSNPRINTF
    vsnprintf(ctx->errmsg, MAX_ERR_MSG_LEN, fmt, ap);
#else
    vsprintf(ctx->errmsg, fmt, ap);
#endif
    ensure_mem_free(ctx);
    return (ctx->errmsg);
}

static char *e(pase_time_ctx_t *ctx, char *fmt, ...)
{
    va_list   ap;
    va_start(ap, fmt);
    char *err = ve(ctx, fmt, ap);
    va_end(ap);
    return (err);
}

/* parse a token, checking if it's something special to us */
static int parse_token(pase_time_ctx_t *ctx, char *arg)
{
    for (int i = 0; ctx->specials[i].name != NULL; i++) {
        if (strcasecmp(ctx->specials[i].name, arg) == 0)
            return ctx->sc_tokid = ctx->specials[i].value;
    }

    /* not special - must be some random id */
    return ctx->sc_tokid = ID;
}

/* init_scanner() sets up the scanner to eat arguments */
static char *init_scanner(pase_time_ctx_t *ctx, int argc, const char **argv)
{
    ctx->scp = argv;
    ctx->scc = argc;
    ctx->need = 1;
    ctx->sc_len = 1;

    while (argc-- > 0)
        ctx->sc_len += strlen(*argv++);

    ctx->sc_token = (char *) malloc(ctx->sc_len * sizeof(char));
    if (ctx->sc_token == NULL)
        return "Failed to allocate memory";
    return TIME_OK;
}

/* token() fetches a token from the input stream */
static int token(pase_time_ctx_t *ctx)
{
    while (1) {
        memset(ctx->sc_token, '\0', ctx->sc_len);
        ctx->sc_tokid = EOF;
        int idx = 0;

        /* if we need to read another argument, walk along the argument list;
         * when we fall off the arglist, we'll just return EOF forever
         */
        if (ctx->need) {
            if (ctx->scc < 1)
                return ctx->sc_tokid;
            ctx->sct = *ctx->scp;
            ctx->scp++;
            ctx->scc--;
            ctx->need = 0;
        }
        /* eat whitespace now - if we walk off the end of the argument,
         * we'll continue, which puts us up at the top of the while loop
         * to fetch the next argument in
         */
        while (isspace((unsigned char) *ctx->sct) || *ctx->sct == '_' || *ctx->sct == ',')
            ++ctx->sct;
        if (!*ctx->sct) {
            ctx->need = 1;
            continue;
        }

        /* preserve the first character of the new token */
        ctx->sc_token[0] = *ctx->sct++;

        /* then see what it is */
        if (isdigit((unsigned char) (ctx->sc_token[0]))) {
            while (isdigit((unsigned char) (*ctx->sct)))
                ctx->sc_token[++idx] = *ctx->sct++;
            ctx->sc_token[++idx] = '\0';
            return ctx->sc_tokid = NUMBER;
        } else if (isalpha((unsigned char) (ctx->sc_token[0]))) {
            while (isalpha((unsigned char) (*ctx->sct)))
                ctx->sc_token[++idx] = *ctx->sct++;
            ctx->sc_token[++idx] = '\0';
            return parse_token(ctx, ctx->sc_token);
        } else {
            switch (ctx->sc_token[0]) {
            case ':':
                return ctx->sc_tokid = COLON;
            case '.':
                return ctx->sc_tokid = DOT;
            case '+':
                return ctx->sc_tokid = PLUS;
            case '-':
                return ctx->sc_tokid = MINUS;
            case '/':
                return ctx->sc_tokid = SLASH;
            default:
                /*OK, we did not make it ... */
                ctx->sct--;
                return ctx->sc_tokid = EOF;
            }
        }
    }
}

/* expect2() gets a token and complains if it's not the token we want */
static char *expect2(pase_time_ctx_t *ctx, int desired, char *complain_fmt, ...)
{
    va_list   ap;
    va_start(ap, complain_fmt);
    if (token(ctx) != desired) {
        char *msg = ve(ctx, complain_fmt, ap);
        va_end(ap);
        panic(msg);
    }
    va_end(ap);
    return TIME_OK;
}

/* plus_minus() is used to parse a single NUMBER TIME-UNIT pair
 *              for the OFFSET-SPEC.
 *              It also applies those m-guessing heuristics.  */
static char *plus_minus(pase_time_ctx_t *ctx, rrd_time_value_t * ptv, int doop)
{
    static int op = PLUS;
    static int prev_multiplier = -1;
    int delta;

    if (doop >= 0) {
        op = doop;
        try(ctx, expect2 (ctx, NUMBER, "There should be number after '%c'", op == PLUS ? '+' : '-'));
        prev_multiplier = -1;   /* reset months-minutes guessing mechanics */
    }
    /* if doop is < 0 then we repeat the previous op
     * with the prefetched number */

    delta = atoi(ctx->sc_token);

    if (token(ctx) == MONTHS_MINUTES) {
        /* hard job to guess what does that -5m means: -5mon or -5min? */
        switch (prev_multiplier) {
        case DAYS:
        case WEEKS:
        case MONTHS:
        case YEARS:
            ctx->sc_tokid = MONTHS;
            break;

        case SECONDS:
        case MINUTES:
        case HOURS:
            ctx->sc_tokid = MINUTES;
            break;

        default:
            if (delta < 6)  /* it may be some other value but in this context
                             * who needs less than 6 min deltas? */
                ctx->sc_tokid = MONTHS;
            else
                ctx->sc_tokid = MINUTES;
        }
    }
    prev_multiplier = ctx->sc_tokid;
    switch (ctx->sc_tokid) {
    case YEARS:
        ptv->tm.tm_year += ( op == PLUS) ? delta : -delta;
        return TIME_OK;
    case MONTHS:
        ptv->tm.tm_mon += ( op == PLUS) ? delta : -delta;
        return TIME_OK;
    case WEEKS:
        delta *= 7;
        /* FALLTHRU */
    case DAYS:
        ptv->tm.tm_mday += ( op == PLUS) ? delta : -delta;
        return TIME_OK;
    case HOURS:
        ptv->offset += (op == PLUS) ? delta * 60 * 60 : -delta * 60 * 60;
        return TIME_OK;
    case MINUTES:
        ptv->offset += (op == PLUS) ? delta * 60 : -delta * 60;
        return TIME_OK;
    case SECONDS:
        ptv->offset += (op == PLUS) ? delta : -delta;
        return TIME_OK;
    default:           /*default unit is seconds */
        ptv->offset += (op == PLUS) ? delta : -delta;
        return TIME_OK;
    }
    panic(e(ctx, "well-known time unit expected after %d", delta));
    /* NORETURN */
    return TIME_OK;
}

/* tod() computes the time of day (TIME-OF-DAY-SPEC) */
static char *tod(pase_time_ctx_t *ctx, rrd_time_value_t * ptv)
{
    int hour, minute = 0;
    int tlen;

    /* save token status in  case we must abort */
    int scc_sv = ctx->scc;
    const char *sct_sv = ctx->sct;
    int sc_tokid_sv = ctx->sc_tokid;

    tlen = strlen(ctx->sc_token);

    /* first pick out the time of day - we assume a HH (COLON|DOT) MM time */
    if (tlen > 2) {
        return TIME_OK;
    }

    hour = atoi(ctx->sc_token);

    token(ctx);
    if (ctx->sc_tokid == SLASH || ctx->sc_tokid == DOT) {
        /* guess we are looking at a date */
        ctx->scc = scc_sv;
        ctx->sct = sct_sv;
        ctx->sc_tokid = sc_tokid_sv;
        snprintf(ctx->sc_token, ctx->sc_len, "%d", hour);
        return TIME_OK;
    }
    if (ctx->sc_tokid == COLON) {
        try(ctx, expect2(ctx, NUMBER, "Parsing HH:MM syntax, expecting MM as number, got none"));
        minute = atoi(ctx->sc_token);
        if (minute > 59) {
            panic(e(ctx, "parsing HH:MM syntax, got MM = %d (>59!)", minute));
        }
        token(ctx);
    }

    /* check if an AM or PM specifier was given */
    if (ctx->sc_tokid == AM || ctx->sc_tokid == PM) {
        if (hour > 12) {
            panic(e(ctx, "there cannot be more than 12 AM or PM hours"));
        }
        if (ctx->sc_tokid == PM) {
            if (hour != 12) /* 12:xx PM is 12:xx, not 24:xx */
                hour += 12;
        } else {
            if (hour == 12) /* 12:xx AM is 00:xx, not 12:xx */
                hour = 0;
        }
        token(ctx);
    } else if (hour > 23) {
        /* guess it was not a time then ... */
        ctx->scc = scc_sv;
        ctx->sct = sct_sv;
        ctx->sc_tokid = sc_tokid_sv;
        snprintf(ctx->sc_token, ctx->sc_len, "%d", hour);
        return TIME_OK;
    }
    ptv->tm.tm_hour = hour;
    ptv->tm.tm_min = minute;
    ptv->tm.tm_sec = 0;

    if (ptv->tm.tm_hour == 24) {
        ptv->tm.tm_hour = 0;
        ptv->tm.tm_mday++;
    }
    return TIME_OK;
}


/* assign_date() assigns a date, adjusting year as appropriate */
static char *assign_date(pase_time_ctx_t *ctx, rrd_time_value_t *ptv, long mday, long mon, long year)
{
    if (year > 138) {
        if (year > 1970) {
            year -= 1900;
        } else {
            panic(e(ctx, "invalid year %d (should be either 00-99 or >1900)", (int)year));
        }
    } else if (year >= 0 && year < 38) {
        year += 100;    /* Allow year 2000-2037 to be specified as   */
    }
    /* 00-37 until the problem of 2038 year will */
    /* arise for unices with 32-bit time_t :)    */
    if (year < 70) {
        panic(e(ctx, "won't handle dates before epoch (01/01/1970), sorry"));
    }

    ptv->tm.tm_mday = mday;
    ptv->tm.tm_mon = mon;
    ptv->tm.tm_year = year;

    return TIME_OK;
}

/* day() picks apart DAY-SPEC-[12] */
static char *day(pase_time_ctx_t *ctx, rrd_time_value_t * ptv)
{
    /* using time_t seems to help portability with 64bit oses */
    time_t  mday = 0, wday, mon, year = ptv->tm.tm_year;
    switch (ctx->sc_tokid) {
    case YESTERDAY:
        ptv->tm.  tm_mday--;
        /* FALLTHRU */
    case TODAY:  /* force ourselves to stay in today - no further processing */
        token(ctx);
        break;
    case TOMORROW:
        ptv->tm.tm_mday++;
        token(ctx);
        break;
    case JAN:
    case FEB:
    case MAR:
    case APR:
    case MAY:
    case JUN:
    case JUL:
    case AUG:
    case SEP:
    case OCT:
    case NOV:
    case DEC:
        /* do month mday [year] */
        mon = (ctx->sc_tokid - JAN);
        try(ctx, expect2(ctx, NUMBER, "the day of the month should follow month name"));
        mday = atol(ctx->sc_token);
        if (token(ctx) == NUMBER) {
            year = atol(ctx->sc_token);
            token(ctx);
        } else {
            year = ptv->tm.tm_year;
        }
        try(ctx, assign_date(ctx, ptv, mday, mon, year));
        break;

    case SUN:
    case MON:
    case TUE:
    case WED:
    case THU:
    case FRI:
    case SAT:
        /* do a particular day of the week */
        wday = (ctx->sc_tokid - SUN);
        ptv->tm.tm_mday += ( wday - ptv->tm.tm_wday);
        token(ctx);
        break;
        /* mday = ptv->tm.tm_mday;
           mday += (wday - ptv->tm.tm_wday);
           ptv->tm.tm_wday = wday;

           try(assign_date(ptv, mday, ptv->tm.tm_mon, ptv->tm.tm_year));
           break;
         */
    case NUMBER:
        /* get numeric <sec since 1970>, MM/DD/[YY]YY, or DD.MM.[YY]YY */
        mon = atol(ctx->sc_token);
        if (mon > 10 * 365 * 24 * 60 * 60) {
            localtime_r(&mon,&(ptv->tm));
            token(ctx);
            break;
        }

        if (mon > 19700101 && mon < 24000101) { /*works between 1900 and 2400 */
            char cmon[3], cmday[3], cyear[5];
            strncpy(cyear, ctx->sc_token, 4);
            cyear[4] = '\0';
            year = atol(cyear);
            strncpy(cmon, &(ctx->sc_token[4]), 2);
            cmon[2] = '\0';
            mon = atol(cmon);
            strncpy(cmday, &(ctx->sc_token[6]), 2);
            cmday[2] = '\0';
            mday = atol(cmday);
            token(ctx);
        } else {
            token(ctx);
            if (mon <= 31 && (ctx->sc_tokid == SLASH || ctx->sc_tokid == DOT)) {
                int sep;
                sep = ctx->sc_tokid;
                try(ctx, expect2(ctx, NUMBER, "there should be %s number after '%c'",
                            sep == DOT ? "month" : "day",
                            sep == DOT ? '.' : '/'));
                mday = atol(ctx->sc_token);
                if (token(ctx) == sep) {
                    try(ctx, expect2(ctx, NUMBER, "there should be year number after '%c'",
                         sep == DOT ? '.' : '/'));
                    year = atol(ctx->sc_token);
                    token(ctx);
                }

                /* flip months and days for European timing
                 */
                if (sep == DOT) {
                    long x = mday;
                    mday = mon;
                    mon = x;
                }
            }
        }

        mon--;
        if (mon < 0 || mon > 11) {
            panic(e(ctx, "did you really mean month %d?", (int)(mon + 1)));
        }
        if (mday < 1 || mday > 31) {
            panic(e(ctx, "I'm afraid that %d is not a valid day of the month", (int)mday));
        }
        try(ctx, assign_date(ctx, ptv, mday, mon, year));
        break;
    }
    return TIME_OK;
}

char *rrd_parsetime(pase_time_ctx_t *ctx, const char *tspec, rrd_time_value_t * ptv)
{
    time_t now = time(NULL);
    int hr = 0;

    /* this MUST be initialized to zero for midnight/noon/teatime */

    ctx->specials = various_words;    /* initialize special words context */

    try(ctx, init_scanner(ctx, 1, &tspec));

    /* establish the default time reference */
    ptv->type = ABSOLUTE_TIME;
    ptv->offset = 0;
    localtime_r(&now,&(ptv->tm));
    ptv->tm.tm_isdst = -1;    /* mk time can figure dst by default ... */

    token(ctx);
    switch (ctx->sc_tokid) {
    case PLUS:
    case MINUS:
        break;  /* jump to OFFSET-SPEC part */
    case EPOCH:
        ptv->type = RELATIVE_TO_EPOCH;
        goto KeepItRelative;
    case START:
        ptv->type = RELATIVE_TO_START_TIME;
        goto KeepItRelative;
    case END:
        ptv->type = RELATIVE_TO_END_TIME;
      KeepItRelative:
        ptv->tm.tm_sec = 0;
        ptv->tm.tm_min = 0;
        ptv->tm.tm_hour = 0;
        ptv->tm.tm_mday = 0;
        ptv->tm.tm_mon = 0;
        ptv->tm.tm_year = 0;
        /* FALLTHRU */
    case NOW: {
        int time_reference = ctx->sc_tokid;
        token(ctx);
        if (ctx->sc_tokid == PLUS || ctx->sc_tokid == MINUS)
            break;
        if (time_reference != NOW) {
            panic(e(ctx, "'start' or 'end' MUST be followed by +|- offset"));
        } else if (ctx->sc_tokid != EOF) {
            panic(e(ctx, "if 'now' is followed by a token it must be +|- offset"));
        }
    }   break;
        /* Only absolute time specifications below */
    case NUMBER: {
        long hour_sv = ptv->tm.tm_hour;
        long year_sv = ptv->tm.tm_year;

        ptv->tm.  tm_hour = 30;
        ptv->tm.  tm_year = 30000;

        try(ctx, tod(ctx, ptv))
        try(ctx, day(ctx, ptv))
        if (ptv->tm.tm_hour == 30 && ptv->tm.tm_year != 30000) {
            try(ctx, tod(ctx, ptv))
        }
        if (ptv->tm.tm_hour == 30) {
            ptv->tm.  tm_hour = hour_sv;
        }
        if (ptv->tm.tm_year == 30000) {
            ptv->tm.  tm_year = year_sv;
        }
    }   break;
        /* fix month parsing */
    case JAN:
    case FEB:
    case MAR:
    case APR:
    case MAY:
    case JUN:
    case JUL:
    case AUG:
    case SEP:
    case OCT:
    case NOV:
    case DEC:
        try(ctx, day(ctx, ptv));
        if (ctx->sc_tokid != NUMBER)
            break;
        try(ctx, tod(ctx, ptv))
            break;

        /* evil coding for TEATIME|NOON|MIDNIGHT - we've initialized
         * hr to zero up above, then fall into this case in such a
         * way so we add +12 +4 hours to it for teatime, +12 hours
         * to it for noon, and nothing at all for midnight, then
         * set our rettime to that hour before leaping into the
         * month scanner
         */
    case TEATIME:
        hr += 4;
        /* FALLTHRU */
    case NOON:
        hr += 12;
        /* FALLTHRU */
    case MIDNIGHT:
        /* if (ptv->tm.tm_hour >= hr) {
           ptv->tm.tm_mday++;
           ptv->tm.tm_wday++;
           } *//* shifting does not makes sense here ... noon is noon */
        ptv->tm.tm_hour = hr;
        ptv->tm.tm_min = 0;
        ptv->tm.tm_sec = 0;

        token(ctx);
        try(ctx, day(ctx, ptv));
        break;
    default:
        panic(e(ctx, "unparsable time: %s%s", ctx->sc_token, ctx->sct));
        break;
    }

    /* the OFFSET-SPEC part
       (NOTE, the sc_tokid was prefetched for us by the previous code) */
    if (ctx->sc_tokid == PLUS || ctx->sc_tokid == MINUS) {
        ctx->specials = time_multipliers; /* switch special words context */
        while (ctx->sc_tokid == PLUS || ctx->sc_tokid == MINUS || ctx->sc_tokid == NUMBER) {
            if (ctx->sc_tokid == NUMBER) {
                try(ctx, plus_minus(ctx, ptv, PREVIOUS_OP));
            } else
                try(ctx, plus_minus(ctx, ptv, ctx->sc_tokid));
            token(ctx);    /* We will get EOF eventually but that's OK, since
                              token() will return us as many EOFs as needed */
        }
    }

    /* now we should be at EOF */
    if (ctx->sc_tokid != EOF) {
        panic(e(ctx, "unparsable trailing text: '...%s%s'", ctx->sc_token, ctx->sct));
    }

    if (ptv->type == ABSOLUTE_TIME) {
        if (mktime(&ptv->tm) == -1) {   /* normalize & check */
            /* can happen for "nonexistent" times, e.g. around 3am */
            /* when winter -> summer time correction eats a hour */
            panic(e(ctx, "the specified time is incorrect (out of range?)"));
        }
    }
    ensure_mem_free(ctx);

    return TIME_OK;
}


int rrd_proc_start_end(rrd_time_value_t *start_tv, rrd_time_value_t *end_tv, time_t *start, time_t *end)
{
    if (start_tv->type == RELATIVE_TO_END_TIME &&   /* same as the line above */
        end_tv->type == RELATIVE_TO_START_TIME) {
//        rrd_set_error("the start and end times cannot be specified "
//                      "relative to each other");
        return -1;
    }

    if (start_tv->type == RELATIVE_TO_START_TIME) {
//        rrd_set_error("the start time cannot be specified relative to itself");
        return -1;
    }

    if (end_tv->type == RELATIVE_TO_END_TIME) {
//        rrd_set_error("the end time cannot be specified relative to itself");
        return -1;
    }

    if (start_tv->type == RELATIVE_TO_END_TIME) {
        struct tm tmtmp;

        *end = mktime(&(end_tv->tm)) + end_tv->offset;
        localtime_r(end, &tmtmp);    /* reinit end including offset */
        tmtmp.tm_mday += start_tv->tm.tm_mday;
        tmtmp.tm_mon += start_tv->tm.tm_mon;
        tmtmp.tm_year += start_tv->tm.tm_year;

        *start = mktime(&tmtmp) + start_tv->offset;
    } else {
        *start = mktime(&(start_tv->tm)) + start_tv->offset;
    }

    if (end_tv->type == RELATIVE_TO_START_TIME) {
        struct tm tmtmp;

        *start = mktime(&(start_tv->tm)) + start_tv->offset;
        localtime_r(start, &tmtmp);
        tmtmp.tm_mday += end_tv->tm.tm_mday;
        tmtmp.tm_mon += end_tv->tm.tm_mon;
        tmtmp.tm_year += end_tv->tm.tm_year;

        *end = mktime(&tmtmp) + end_tv->offset;
    } else {
        *end = mktime(&(end_tv->tm)) + end_tv->offset;
    }

    return 0;
}
