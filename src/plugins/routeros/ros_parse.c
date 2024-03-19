// SPDX-License-Identifier: GPL-2.0-only OR ISC
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at verplant.org>

// from  librouteros - src/ros_parse.c

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include "ros_api.h"

bool ros_sstrtob (const char *str)
{
    if (str == NULL)
        return false;

    if (strcasecmp ("true", str) == 0)
        return true;
    return false;
}

unsigned int ros_sstrtoui (const char *str)
{
    unsigned int ret;
    char *endptr;

    if (str == NULL)
        return 0;

    errno = 0;
    endptr = NULL;
    ret = (unsigned int) strtoul (str, &endptr, /* base = */ 10);
    if ((endptr == str) || (errno != 0))
        return 0;

    return ret;
}

uint64_t ros_sstrtoui64 (const char *str)
{
    uint64_t ret;
    char *endptr;

    if (str == NULL)
        return 0;

    errno = 0;
    endptr = NULL;
    ret = (uint64_t) strtoull (str, &endptr, /* base = */ 10);
    if ((endptr == str) || (errno != 0))
        return 0;

    return ret;
}

double ros_sstrtod (const char *str)
{
    double ret;
    char *endptr;

    if (str == NULL)
        return NAN;

    errno = 0;
    endptr = NULL;
    ret = strtod (str, &endptr);
    if ((endptr == str) || (errno != 0))
        return NAN;

    return ret;
}

int ros_sstrto_rx_tx_counters (const char *str, uint64_t *rx, uint64_t *tx)
{
    const char *ptr;
    char *endptr;

    if ((rx == NULL) || (tx == NULL))
        return EINVAL;

    *rx = 0;
    *tx = 0;

    if (str == NULL)
        return EINVAL;

    ptr = str;
    errno = 0;
    endptr = NULL;
    *rx = (uint64_t) strtoull (ptr, &endptr, /* base = */ 10);
    if ((endptr == str) || (errno != 0)) {
        *rx = 0;
        return EIO;
    }

    assert (endptr != NULL);
    if ((*endptr != '/') && (*endptr != ','))
        return EIO;

    ptr = endptr + 1;
    errno = 0;
    endptr = NULL;
    *tx = (uint64_t) strtoull (ptr, &endptr, /* base = */ 10);
    if ((endptr == str) || (errno != 0)) {
        *rx = 0;
        *tx = 0;
        return EIO;
    }

    return 0;
}

/* have_hour is initially set to false and later set to true when the first
 * colon is found. It is used to determine whether the number before the colon
 * is hours or minutes. External code should use the sstrtodate() macro. */
uint64_t _ros_sstrtodate (const char *str, bool have_hour)
{
    uint64_t ret;
    char *endptr;

    if ((str == NULL) || (*str == 0))
        return 0;

    /* Example string: 6w6d18:33:07 */
    errno = 0;
    endptr = NULL;
    ret = (uint64_t) strtoull (str, &endptr, /* base = */ 10);
    if ((endptr == str) || (errno != 0))
        return 0;

    switch (*endptr) {
    case 'y': ret *= 365 * 86400; break;
    case 'w': ret *=   7 * 86400; break;
    case 'd': ret *=       86400; break;
    case 'h': ret *=        3600; break;
    case 'm': ret *=          60; break;
    case 's': ret *=           1; break;
    case ':': ret *= have_hour ? 60 : 3600; have_hour = true; break;
    }

    return ret + _ros_sstrtodate (endptr + 1, have_hour);
}
