/* SPDX-License-Identifier: GPL-2.0-only */

#pragma once

#include <stddef.h>
#include <stdint.h>

#define ITOA_MAX 21

size_t uitoa(uint64_t value, char* dst);

static inline size_t itoa(int64_t value, char* dst)
{
    size_t sign = 0;
    uint64_t uvalue = 0;
    if (value < 0) {
        *(dst++) = '-';
        sign = 1;
        uvalue = (uint64_t)(-value);
    } else {
        uvalue = (uint64_t)value;
    }

    return uitoa(uvalue, dst) + sign;
}
