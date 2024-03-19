// SPDX-License-Identifier: GPL-2.0-or-later

#include <inttypes.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * log10 from Hacker's Delight
 * https://github.com/ulfjack/ryu/pull/75
 * https://github.com/jorgbrown
 */

static inline uint32_t digits10(const uint64_t val)
{
    static const uint64_t table[20] = {
        0ull,
        9ull,
        99ull,
        999ull,
        9999ull,
        99999ull,
        999999ull,
        9999999ull,
        99999999ull,
        999999999ull,
        9999999999ull,
        99999999999ull,
        999999999999ull,
        9999999999999ull,
        99999999999999ull,
        999999999999999ull,
        9999999999999999ull,
        99999999999999999ull,
        999999999999999999ull,
        9999999999999999999ull
    };

    static const unsigned char digits2N[64] = {
        1,  1,  1,  1,  2,  2,  2,  3,
        3,  3,  4,  4,  4,  4,  5,  5,
        5,  6,  6,  6,  7,  7,  7,  7,
        8,  8,  8,  9,  9,  9,  10, 10,
        10, 10, 11, 11, 11, 12, 12, 12,
        13, 13, 13, 13, 14, 14, 14, 15,
        15, 15, 16, 16, 16, 16, 17, 17,
        17, 18, 18, 18, 19, 19, 19, 19
    };

    if (val == 0)
        return 1;

    uint32_t guess = digits2N [63 ^ __builtin_clzll(val)];
    return  guess + (val > table[guess] ? 1 : 0);
}

/*
 * https://www.facebook.com/notes/facebook-engineering/three-optimization-tips-for-c/10151361643253920
 */

size_t uitoa(uint64_t value, char* dst)
{
    static const char digits[201] =
        "0001020304050607080910111213141516171819"
        "2021222324252627282930313233343536373839"
        "4041424344454647484950515253545556575859"
        "6061626364656667686970717273747576777879"
        "8081828384858687888990919293949596979899";

    uint32_t const length = digits10(value);
    uint32_t next = length - 1;

    while (value >= 100) {
        int const i = (value % 100) * 2;
        value /= 100;
        dst[next] = digits[i + 1];
        dst[next - 1] = digits[i];
        next -= 2;
    }

    /* Handle last 1-2 digits */
    if (value < 10) {
        dst[next] = '0' + (uint32_t) value;
    } else {
        int i = (uint32_t) value * 2;
        dst[next] = digits[i + 1];
        dst[next - 1] = digits[i];
    }

    dst[length] = '\0';
    return (size_t)length;
}
