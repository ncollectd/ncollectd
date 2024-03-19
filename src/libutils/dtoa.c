// SPDX-License-Identifier: GPL-2.0-only OR Apache-2.0

// File from the swift project: stdlib/public/runtime/SwiftDtoa.cpp
// Copyright (c) 2018-2020 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//===---------------------------------------------------------------------===//
/// For other formats, SwiftDtoa uses a modified form of the Grisu2
/// algorithm from Florian Loitsch; "Printing Floating-Point Numbers
/// Quickly and Accurately with Integers", 2010.
/// https://doi.org/10.1145/1806596.1806623
///
/// Some of the Grisu2 modifications were suggested by the "Errol
/// paper": Marc Andrysco, Ranjit Jhala, Sorin Lerner; "Printing
/// Floating-Point Numbers: A Faster, Always Correct Method", 2016.
/// https://doi.org/10.1145/2837614.2837654
/// In particular, the Errol paper explored the impact of higher-precision
/// fixed-width arithmetic on Grisu2 and showed a way to rapidly test
/// the correctness of such algorithms.
///
/// A few further improvements were inspired by the Ryu algorithm
/// from Ulf Anders; "Ryū: fast float-to-string conversion", 2018.
/// https://doi.org/10.1145/3296979.3192369
///
/// In summary, this implementation is:
///
/// * Fast.  It uses only fixed-width integer arithmetic and has
///   constant memory requirements.  For double-precision values on
///   64-bit processors, it is competitive with Ryu. For double-precision
///   values on 32-bit processors, and higher-precision values on all
///   processors, it is considerably faster.
///
/// * Always Accurate. Converting the decimal form back to binary
///   will always yield exactly the same value. For the IEEE 754
///   formats, the round-trip will produce exactly the same bit
///   pattern in memory.
///
/// * Always Short.  This always selects an accurate result with the
///   minimum number of significant digits.
///
/// * Always Close.  Among all accurate, short results, this always
///   chooses the result that is closest to the exact floating-point
///   value. (In case of an exact tie, it rounds the last digit even.)
///
/// * Portable.  The code is written in portable C99.  The core
///   implementations utilize only fixed-size integer arithmetic.
///   128-bit integer support is utilized if present but is not
///   necessary.  There are thin wrappers that accept platform-native
///   floating point types and delegate to the portable core
///   functions.
///
// ----------------------------------------------------------------------------

#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#if __GNUC__ >= 10
#pragma GCC diagnostic ignored "-Warith-conversion"
#endif

#if !((FLT_RADIX == 2) && (DBL_MANT_DIG == 53) && (DBL_MIN_EXP == -1021) && (DBL_MAX_EXP == 1024))
    #error "Double on this system not use binary64 format"
#endif

#if defined(__SIZEOF_INT128__)
  // We get a significant speed boost if we can use the __uint128_t
  // type that's present in GCC and Clang on 64-bit architectures.  In
  // particular, we can do 128-bit arithmetic directly and can
  // represent 256-bit integers as collections of 64-bit elements.
  #define HAVE_UINT128_T 1
#else
  // On 32-bit, we use slower code that manipulates 128-bit
  // and 256-bit integers as collections of 32-bit elements.
  #define HAVE_UINT128_T 0
#endif

// A table of all two-digit decimal numbers
static const char asciiDigitTable[] =
  "0001020304050607080910111213141516171819"
  "2021222324252627282930313233343536373839"
  "4041424344454647484950515253545556575859"
  "6061626364656667686970717273747576777879"
  "8081828384858687888990919293949596979899";

// Tables with powers of 10
//
// The constant powers of 10 here represent pure fractions
// with a binary point at the far left. (Each number in
// this first table is implicitly divided by 2^128.)
//
// Table size: 896 bytes
//
// A 64-bit significand allows us to exactly represent powers of 10 up
// to 10^27.  In 128 bits, we can exactly represent powers of 10 up to
// 10^55.  As with all of these tables, the binary exponent is not stored;
// it is computed by the `binaryExponentFor10ToThe(p)` function.
static const uint64_t powersOf10_Exact128[56 * 2] = {
    // Low order ... high order
    0x0000000000000000ULL, 0x8000000000000000ULL, // x 2^1 == 10^0 exactly
    0x0000000000000000ULL, 0xa000000000000000ULL, // x 2^4 == 10^1 exactly
    0x0000000000000000ULL, 0xc800000000000000ULL, // x 2^7 == 10^2 exactly
    0x0000000000000000ULL, 0xfa00000000000000ULL, // x 2^10 == 10^3 exactly
    0x0000000000000000ULL, 0x9c40000000000000ULL, // x 2^14 == 10^4 exactly
    0x0000000000000000ULL, 0xc350000000000000ULL, // x 2^17 == 10^5 exactly
    0x0000000000000000ULL, 0xf424000000000000ULL, // x 2^20 == 10^6 exactly
    0x0000000000000000ULL, 0x9896800000000000ULL, // x 2^24 == 10^7 exactly
    0x0000000000000000ULL, 0xbebc200000000000ULL, // x 2^27 == 10^8 exactly
    0x0000000000000000ULL, 0xee6b280000000000ULL, // x 2^30 == 10^9 exactly
    0x0000000000000000ULL, 0x9502f90000000000ULL, // x 2^34 == 10^10 exactly
    0x0000000000000000ULL, 0xba43b74000000000ULL, // x 2^37 == 10^11 exactly
    0x0000000000000000ULL, 0xe8d4a51000000000ULL, // x 2^40 == 10^12 exactly
    0x0000000000000000ULL, 0x9184e72a00000000ULL, // x 2^44 == 10^13 exactly
    0x0000000000000000ULL, 0xb5e620f480000000ULL, // x 2^47 == 10^14 exactly
    0x0000000000000000ULL, 0xe35fa931a0000000ULL, // x 2^50 == 10^15 exactly
    0x0000000000000000ULL, 0x8e1bc9bf04000000ULL, // x 2^54 == 10^16 exactly
    0x0000000000000000ULL, 0xb1a2bc2ec5000000ULL, // x 2^57 == 10^17 exactly
    0x0000000000000000ULL, 0xde0b6b3a76400000ULL, // x 2^60 == 10^18 exactly
    0x0000000000000000ULL, 0x8ac7230489e80000ULL, // x 2^64 == 10^19 exactly
    0x0000000000000000ULL, 0xad78ebc5ac620000ULL, // x 2^67 == 10^20 exactly
    0x0000000000000000ULL, 0xd8d726b7177a8000ULL, // x 2^70 == 10^21 exactly
    0x0000000000000000ULL, 0x878678326eac9000ULL, // x 2^74 == 10^22 exactly
    0x0000000000000000ULL, 0xa968163f0a57b400ULL, // x 2^77 == 10^23 exactly
    0x0000000000000000ULL, 0xd3c21bcecceda100ULL, // x 2^80 == 10^24 exactly
    0x0000000000000000ULL, 0x84595161401484a0ULL, // x 2^84 == 10^25 exactly
    0x0000000000000000ULL, 0xa56fa5b99019a5c8ULL, // x 2^87 == 10^26 exactly
    0x0000000000000000ULL, 0xcecb8f27f4200f3aULL, // x 2^90 == 10^27 exactly
    0x4000000000000000ULL, 0x813f3978f8940984ULL, // x 2^94 == 10^28 exactly
    0x5000000000000000ULL, 0xa18f07d736b90be5ULL, // x 2^97 == 10^29 exactly
    0xa400000000000000ULL, 0xc9f2c9cd04674edeULL, // x 2^100 == 10^30 exactly
    0x4d00000000000000ULL, 0xfc6f7c4045812296ULL, // x 2^103 == 10^31 exactly
    0xf020000000000000ULL, 0x9dc5ada82b70b59dULL, // x 2^107 == 10^32 exactly
    0x6c28000000000000ULL, 0xc5371912364ce305ULL, // x 2^110 == 10^33 exactly
    0xc732000000000000ULL, 0xf684df56c3e01bc6ULL, // x 2^113 == 10^34 exactly
    0x3c7f400000000000ULL, 0x9a130b963a6c115cULL, // x 2^117 == 10^35 exactly
    0x4b9f100000000000ULL, 0xc097ce7bc90715b3ULL, // x 2^120 == 10^36 exactly
    0x1e86d40000000000ULL, 0xf0bdc21abb48db20ULL, // x 2^123 == 10^37 exactly
    0x1314448000000000ULL, 0x96769950b50d88f4ULL, // x 2^127 == 10^38 exactly
    0x17d955a000000000ULL, 0xbc143fa4e250eb31ULL, // x 2^130 == 10^39 exactly
    0x5dcfab0800000000ULL, 0xeb194f8e1ae525fdULL, // x 2^133 == 10^40 exactly
    0x5aa1cae500000000ULL, 0x92efd1b8d0cf37beULL, // x 2^137 == 10^41 exactly
    0xf14a3d9e40000000ULL, 0xb7abc627050305adULL, // x 2^140 == 10^42 exactly
    0x6d9ccd05d0000000ULL, 0xe596b7b0c643c719ULL, // x 2^143 == 10^43 exactly
    0xe4820023a2000000ULL, 0x8f7e32ce7bea5c6fULL, // x 2^147 == 10^44 exactly
    0xdda2802c8a800000ULL, 0xb35dbf821ae4f38bULL, // x 2^150 == 10^45 exactly
    0xd50b2037ad200000ULL, 0xe0352f62a19e306eULL, // x 2^153 == 10^46 exactly
    0x4526f422cc340000ULL, 0x8c213d9da502de45ULL, // x 2^157 == 10^47 exactly
    0x9670b12b7f410000ULL, 0xaf298d050e4395d6ULL, // x 2^160 == 10^48 exactly
    0x3c0cdd765f114000ULL, 0xdaf3f04651d47b4cULL, // x 2^163 == 10^49 exactly
    0xa5880a69fb6ac800ULL, 0x88d8762bf324cd0fULL, // x 2^167 == 10^50 exactly
    0x8eea0d047a457a00ULL, 0xab0e93b6efee0053ULL, // x 2^170 == 10^51 exactly
    0x72a4904598d6d880ULL, 0xd5d238a4abe98068ULL, // x 2^173 == 10^52 exactly
    0x47a6da2b7f864750ULL, 0x85a36366eb71f041ULL, // x 2^177 == 10^53 exactly
    0x999090b65f67d924ULL, 0xa70c3c40a64e6c51ULL, // x 2^180 == 10^54 exactly
    0xfff4b4e3f741cf6dULL, 0xd0cf4b50cfe20765ULL, // x 2^183 == 10^55 exactly
};

// Rounded values supporting the full range of binary64
//
// Table size: 464 bytes
//
// We only store every 28th power of ten here.
// We can multiply by an exact 64-bit power of
// ten from the table above to reconstruct the
// significand for any power of 10.
static const uint64_t powersOf10_Binary64[] = {
    // low-order half, high-order half
    0x3931b850df08e738, 0x95fe7e07c91efafa, // x 2^-1328 ~= 10^-400
    0xba954f8e758fecb3, 0x9774919ef68662a3, // x 2^-1235 ~= 10^-372
    0x9028bed2939a635c, 0x98ee4a22ecf3188b, // x 2^-1142 ~= 10^-344
    0x47b233c92125366e, 0x9a6bb0aa55653b2d, // x 2^-1049 ~= 10^-316
    0x4ee367f9430aec32, 0x9becce62836ac577, // x 2^-956 ~= 10^-288
    0x6f773fc3603db4a9, 0x9d71ac8fada6c9b5, // x 2^-863 ~= 10^-260
    0xc47bc5014a1a6daf, 0x9efa548d26e5a6e1, // x 2^-770 ~= 10^-232
    0x80e8a40eccd228a4, 0xa086cfcd97bf97f3, // x 2^-677 ~= 10^-204
    0xb8ada00e5a506a7c, 0xa21727db38cb002f, // x 2^-584 ~= 10^-176
    0xc13e60d0d2e0ebba, 0xa3ab66580d5fdaf5, // x 2^-491 ~= 10^-148
    0xc2974eb4ee658828, 0xa54394fe1eedb8fe, // x 2^-398 ~= 10^-120
    0xcb4ccd500f6bb952, 0xa6dfbd9fb8e5b88e, // x 2^-305 ~= 10^-92
    0x3f2398d747b36224, 0xa87fea27a539e9a5, // x 2^-212 ~= 10^-64
    0xdde50bd1d5d0b9e9, 0xaa242499697392d2, // x 2^-119 ~= 10^-36
    0xfdc20d2b36ba7c3d, 0xabcc77118461cefc, // x 2^-26 ~= 10^-8
    0x0000000000000000, 0xad78ebc5ac620000, // x 2^67 == 10^20 exactly
    0x9670b12b7f410000, 0xaf298d050e4395d6, // x 2^160 == 10^48 exactly
    0x3b25a55f43294bcb, 0xb0de65388cc8ada8, // x 2^253 ~= 10^76
    0x58edec91ec2cb657, 0xb2977ee300c50fe7, // x 2^346 ~= 10^104
    0x29babe4598c311fb, 0xb454e4a179dd1877, // x 2^439 ~= 10^132
    0x577b986b314d6009, 0xb616a12b7fe617aa, // x 2^532 ~= 10^160
    0x0c11ed6d538aeb2f, 0xb7dcbf5354e9bece, // x 2^625 ~= 10^188
    0x6d953e2bd7173692, 0xb9a74a0637ce2ee1, // x 2^718 ~= 10^216
    0x9d6d1ad41abe37f1, 0xbb764c4ca7a4440f, // x 2^811 ~= 10^244
    0x4b2d8644d8a74e18, 0xbd49d14aa79dbc82, // x 2^904 ~= 10^272
    0xe0470a63e6bd56c3, 0xbf21e44003acdd2c, // x 2^997 ~= 10^300
    0x505f522e53053ff2, 0xc0fe908895cf3b44, // x 2^1090 ~= 10^328
    0xcca845ab2beafa9a, 0xc2dfe19c8c055535, // x 2^1183 ~= 10^356
    0x1027fff56784f444, 0xc4c5e310aef8aa17, // x 2^1276 ~= 10^384
};

//
// Predefine various arithmetic helpers.  Most implementations and extensive
// comments are at the bottom of this file.
//

// The power-of-10 tables do not directly store the associated binary
// exponent.  That's because the binary exponent is a simple linear
// function of the decimal power (and vice versa), so it's just as
// fast (and uses much less memory) to compute it:

// The binary exponent corresponding to a particular power of 10.
// This matches the power-of-10 tables across the full range of binary128.
#define binaryExponentFor10ToThe(p) ((int)(((((int64_t)(p)) * 55732705) >> 24) + 1))

// A decimal exponent that approximates a particular binary power.
#define decimalExponentFor2ToThe(e) ((int)(((int64_t)e * 20201781) >> 26))

//
// Helper functions used only by the single-precision binary32 formatter
//

// Helpers used by binary32, binary64, float80, and binary128.
//

#if HAVE_UINT128_T
typedef __uint128_t swift_uint128_t;
#define initialize128WithHighLow64(dest, high64, low64) ((dest) = ((__uint128_t)(high64) << 64) | (low64))
#define shiftLeft128(u128, shift) (*(u128) <<= shift)
#else
typedef struct {
    uint32_t low, b, c, high;
} swift_uint128_t;
#define initialize128WithHighLow64(dest, high64, low64) \
    ((dest).low = (uint32_t)(low64),                    \
     (dest).b = (uint32_t)((low64) >> 32),              \
     (dest).c = (uint32_t)(high64),                     \
     (dest).high = (uint32_t)((high64) >> 32))
static void shiftLeft128(swift_uint128_t *, int shift);
#endif


//
// Helper functions needed by the binary64 formatter.
//

#if HAVE_UINT128_T
#define isLessThan128x128(lhs, rhs) ((lhs) < (rhs))
#define subtract128x128(lhs, rhs) (*(lhs) -= (rhs))
#define multiply128xu32(lhs, rhs) (*(lhs) *= (rhs))
#define initialize128WithHigh64(dest, value) ((dest) = (__uint128_t)(value) << 64)
#define extractHigh64From128(arg) ((uint64_t)((arg) >> 64))
#define is128bitZero(arg) ((arg) == 0)
static int extractIntegerPart128(__uint128_t *fixed128, int integerBits) {
    const int fractionBits = 128 - integerBits;
    int integerPart = (int)(*fixed128 >> fractionBits);
    const swift_uint128_t fixedPointMask = (((__uint128_t)1 << fractionBits) - 1);
    *fixed128 &= fixedPointMask;
    return integerPart;
}
#define shiftRightRoundingDown128(val, shift) ((val) >> (shift))
#define shiftRightRoundingUp128(val, shift) (((val) + (((uint64_t)1 << (shift)) - 1)) >> (shift))

#else

#define initialize128WithHigh64(dest, value)            \
    ((dest).low = (dest).b = 0,                         \
     (dest).c = (uint32_t)(value),                      \
     (dest).high = (uint32_t)((value) >> 32))
#define extractHigh64From128(arg) (((uint64_t)(arg).high << 32)|((arg).c))
#define is128bitZero(dest) \
  (((dest).low | (dest).b | (dest).c | (dest).high) == 0)
// Treat a uint128_t as a fixed-point value with `integerBits` bits in
// the integer portion.  Return the integer portion and zero it out.
static int extractIntegerPart128(swift_uint128_t *fixed128, int integerBits) {
    const int highFractionBits = 32 - integerBits;
    int integerPart = (int)(fixed128->high >> highFractionBits);
    fixed128->high &= ((uint32_t)1 << highFractionBits) - 1;
    return integerPart;
}
#endif

// ================================================================
//
// Helpers to output formatted results for infinity, zero, and NaN
//
// ================================================================

static inline size_t infinity(char *dest, size_t len, int negative)
{
  if (negative) {
    if (len >= 5) {
      memcpy(dest, "-inf", 5);
      return 4;
    }
  } else {
    if (len >= 4) {
      memcpy(dest, "inf", 4);
      return 3;
    }
  }
  if (len > 0) {
    dest[0] = '\0';
  }
  return 0;
}

static inline size_t zero(char *dest, size_t len, int negative)
{
  if (negative) {
    if (len >= 3) {
      memcpy(dest, "-0", 3);
      return 2;
    }
  } else {
    if (len >= 2) {
      memcpy(dest, "0", 2);
      return 1;
    }
  }
  if (len > 0) {
    dest[0] = '\0';
  }
  return 0;
}

static inline size_t nan_details(char *dest, size_t len, int negative, int quiet)
{
  if (negative) {
    if (quiet) {
      if (len >= 5) {
        memcpy(dest, "-nan", 5);
        return 4;
      }
    } else {
      if (len >= 6) {
        memcpy(dest, "-snan", 6);
        return 5;
      }
    }
  } else {
    if (quiet) {
      if (len >= 4) {
        memcpy(dest, "nan", 4);
        return 3;
      }
    } else {
      if (len >= 5) {
        memcpy(dest, "snan", 5);
        return 4;
      }
    }
  }
  if (len > 0) {
    dest[0] = '\0';
  }
  return 0;
}

// ================================================================
//
// Arithmetic helpers
//
// ================================================================

// The core algorithm relies heavily on fixed-point arithmetic with
// 128-bit and 256-bit integer values. (For binary32/64 and
// float80/binary128, respectively.) They also need precise control
// over all rounding.
//
// Note that most arithmetic operations are the same for integers and
// fractions, so we can just use the normal integer operations in most
// places.  Multiplication however, is different for fixed-size
// fractions.  Integer multiplication preserves the low-order part and
// discards the high-order part (ignoring overflow).  Fraction
// multiplication preserves the high-order part and discards the
// low-order part (rounding).  So most of the arithmetic helpers here
// are for multiplication.

// Note: With 64-bit GCC and Clang, we get a noticable performance
// gain by using `__uint128_t`.  Otherwise, we have to break things
// down into 32-bit chunks so we don't overflow 64-bit temporaries.

// Multiply a 128-bit fraction by a 64-bit fraction, rounding down.
static inline swift_uint128_t multiply128x64RoundingDown(swift_uint128_t lhs, uint64_t rhs)
{
#if HAVE_UINT128_T
    uint64_t lhsl = (uint64_t)lhs;
    uint64_t lhsh = (uint64_t)(lhs >> 64);
    swift_uint128_t h = (swift_uint128_t)lhsh * rhs;
    swift_uint128_t l = (swift_uint128_t)lhsl * rhs;
    return h + (l >> 64);
#else
    swift_uint128_t result;
    static const uint64_t mask32 = UINT32_MAX;
    uint64_t rhs0 = rhs & mask32;
    uint64_t rhs1 = rhs >> 32;
    uint64_t t = (lhs.low) * rhs0;
    t >>= 32;
    uint64_t a = (lhs.b) * rhs0;
    uint64_t b = (lhs.low) * rhs1;
    t += a + (b & mask32);
    t >>= 32;
    t += (b >> 32);
    a = lhs.c * rhs0;
    b = lhs.b * rhs1;
    t += (a & mask32) + (b & mask32);
    result.low = t;
    t >>= 32;
    t += (a >> 32) + (b >> 32);
    a = lhs.high * rhs0;
    b = lhs.c * rhs1;
    t += (a & mask32) + (b & mask32);
    result.b = t;
    t >>= 32;
    t += (a >> 32) + (b >> 32);
    t += lhs.high * rhs1;
    result.c = t;
    result.high = t >> 32;
    return result;
#endif
}

// Multiply a 128-bit fraction by a 64-bit fraction, rounding up.
static inline swift_uint128_t multiply128x64RoundingUp(swift_uint128_t lhs, uint64_t rhs)
{
#if HAVE_UINT128_T
    uint64_t lhsl = (uint64_t)lhs;
    uint64_t lhsh = (uint64_t)(lhs >> 64);
    swift_uint128_t h = (swift_uint128_t)lhsh * rhs;
    swift_uint128_t l = (swift_uint128_t)lhsl * rhs;
    static const __uint128_t bias = ((__uint128_t)1 << 64) - 1;
    return h + ((l + bias) >> 64);
#else
    swift_uint128_t result;
    static const uint64_t mask32 = UINT32_MAX;
    uint64_t rhs0 = rhs & mask32;
    uint64_t rhs1 = rhs >> 32;
    uint64_t t = (lhs.low) * rhs0 + mask32;
    t >>= 32;
    uint64_t a = (lhs.b) * rhs0;
    uint64_t b = (lhs.low) * rhs1;
    t += (a & mask32) + (b & mask32) + mask32;
    t >>= 32;
    t += (a >> 32) + (b >> 32);
    a = lhs.c * rhs0;
    b = lhs.b * rhs1;
    t += (a & mask32) + (b & mask32);
    result.low = t;
    t >>= 32;
    t += (a >> 32) + (b >> 32);
    a = lhs.high * rhs0;
    b = lhs.c * rhs1;
    t += (a & mask32) + (b & mask32);
    result.b = t;
    t >>= 32;
    t += (a >> 32) + (b >> 32);
    t += lhs.high * rhs1;
    result.c = t;
    result.high = t >> 32;
    return result;
#endif
}

#if !HAVE_UINT128_T
// Multiply a 128-bit fraction by a 32-bit integer in a 32-bit environment.
// (On 64-bit, we use a fast inline macro.)
static inline void multiply128xu32(swift_uint128_t *lhs, uint32_t rhs)
{
    uint64_t t = (uint64_t)(lhs->low) * rhs;
    lhs->low = (uint32_t)t;
    t = (t >> 32) + (uint64_t)(lhs->b) * rhs;
    lhs->b = (uint32_t)t;
    t = (t >> 32) + (uint64_t)(lhs->c) * rhs;
    lhs->c = (uint32_t)t;
    t = (t >> 32) + (uint64_t)(lhs->high) * rhs;
    lhs->high = (uint32_t)t;
}

// Compare two 128-bit integers in a 32-bit environment
// (On 64-bit, we use a fast inline macro.)
static inline int isLessThan128x128(swift_uint128_t lhs, swift_uint128_t rhs)
{
    return ((lhs.high < rhs.high)
            || ((lhs.high == rhs.high)
                && ((lhs.c < rhs.c)
                    || ((lhs.c == rhs.c)
                        && ((lhs.b < rhs.b)
                            || ((lhs.b == rhs.b)
                                && (lhs.low < rhs.low)))))));
}

// Subtract 128-bit values in a 32-bit environment
static inline void subtract128x128(swift_uint128_t *lhs, swift_uint128_t rhs)
{
    uint64_t t = (uint64_t)lhs->low + (~rhs.low) + 1;
    lhs->low = (uint32_t)t;
    t = (t >> 32) + lhs->b + (~rhs.b);
    lhs->b = (uint32_t)t;
    t = (t >> 32) + lhs->c + (~rhs.c);
    lhs->c = (uint32_t)t;
    t = (t >> 32) + lhs->high + (~rhs.high);
    lhs->high = (uint32_t)t;
}
#endif

#if !HAVE_UINT128_T
// Shift a 128-bit integer right, rounding down.
static inline swift_uint128_t shiftRightRoundingDown128(swift_uint128_t lhs, int shift)
{
    // Note: Shift is always less than 32
    swift_uint128_t result;
    uint64_t t = (uint64_t)lhs.low >> shift;
    t += ((uint64_t)lhs.b << (32 - shift));
    result.low = t;
    t >>= 32;
    t += ((uint64_t)lhs.c << (32 - shift));
    result.b = t;
    t >>= 32;
    t += ((uint64_t)lhs.high << (32 - shift));
    result.c = t;
    t >>= 32;
    result.high = t;
    return result;
}
#endif

#if !HAVE_UINT128_T
// Shift a 128-bit integer right, rounding up.
static inline swift_uint128_t shiftRightRoundingUp128(swift_uint128_t lhs, int shift)
{
    swift_uint128_t result;
    const uint64_t bias = (1 << shift) - 1;
    uint64_t t = ((uint64_t)lhs.low + bias) >> shift;
    t += ((uint64_t)lhs.b << (32 - shift));
    result.low = t;
    t >>= 32;
    t += ((uint64_t)lhs.c << (32 - shift));
    result.b = t;
    t >>= 32;
    t += ((uint64_t)lhs.high << (32 - shift));
    result.c = t;
    t >>= 32;
    result.high = t;
    return result;
}
#endif

 // Shift a 128-bit integer left, discarding high bits
#if !HAVE_UINT128_T
static inline void shiftLeft128(swift_uint128_t *lhs, int shift)
{
    // Note: Shift is always less than 32
    uint64_t t = (uint64_t)lhs->high << (shift + 32);
    t += (uint64_t)lhs->c << shift;
    lhs->high = t >> 32;
    t <<= 32;
    t += (uint64_t)lhs->b << shift;
    lhs->c = t >> 32;
    t <<= 32;
    t += (uint64_t)lhs->low << shift;
    lhs->b = t >> 32;
    lhs->low = t;
}
#endif

// Given a power `p`, this returns three values:
// * 128-bit fractions `lower` and `upper`
// * integer `exponent`
//
// Note: This function takes on average about 10% of the total runtime
// for formatting a double, as the general case here requires several
// multiplications to accurately reconstruct the lower and upper
// bounds.
//
// The returned values satisty the following:
// ```
//    lower * 2^exponent <= 10^p <= upper * 2^exponent
// ```
//
// Note: Max(*upper - *lower) = 3
static inline void intervalContainingPowerOf10_Binary64(int p, swift_uint128_t *lower, swift_uint128_t *upper, int *exponent)
{
    if (p >= 0 && p <= 55) {
        // Use one 64-bit exact value
        swift_uint128_t exact;
        initialize128WithHighLow64(exact,
                                   powersOf10_Exact128[p * 2 + 1],
                                   powersOf10_Exact128[p * 2]);
        *upper = exact;
        *lower = exact;
        *exponent = binaryExponentFor10ToThe(p);
        return;
    }

    // Multiply a 128-bit approximate value with a 64-bit exact value
    int index = p + 400;
    // Copy a pair of uint64_t into a swift_uint128_t
    int mainPower = index / 28;
    const uint64_t *base_p = powersOf10_Binary64 + mainPower * 2;
    swift_uint128_t base;
    initialize128WithHighLow64(base, base_p[1], base_p[0]);
    int extraPower = index - mainPower * 28;
    int baseExponent = binaryExponentFor10ToThe(p - extraPower);

    int e = baseExponent;
    if (extraPower == 0) {
        // We're using a tightly-rounded lower bound, so +1 gives a tightly-rounded upper bound
        *lower = base;
#if HAVE_UINT128_T
        *upper = *lower + 1;
#else
        *upper = *lower;
        upper->low += 1;
#endif
    } else {
        // We need to multiply two values to get a lower bound
        int64_t extra = powersOf10_Exact128[extraPower * 2 + 1];
        e += binaryExponentFor10ToThe(extraPower);
        *lower = multiply128x64RoundingDown(base, extra);
        // +2 is enough to get an upper bound
        // (Verified through exhaustive testing.)
#if HAVE_UINT128_T
        *upper = *lower + 2;
#else
        *upper = *lower;
        upper->low += 2;
#endif
    }
    *exponent = e;
}


static int finishFormatting(char *dest, size_t length, char *p,
                            char *firstOutputChar, int forceExponential, int base10Exponent)
{
    int digitCount = p - firstOutputChar - 1;
    if (base10Exponent < -4 || forceExponential) {
      // Exponential form: convert "0123456" => "1.23456e78"
      firstOutputChar[0] = firstOutputChar[1];
      if (digitCount > 1) {
        firstOutputChar[1] = '.';
      } else {
        p--;
      }
      // Add exponent at the end
      if (p > dest + length - 5) {
        dest[0] = '\0';
        return 0;
      }
      *p++ = 'e';
      if (base10Exponent < 0) {
        *p++ = '-';
        base10Exponent = -base10Exponent;
      } else {
        *p++ = '+';
      }
      if (base10Exponent > 99) {
        if (base10Exponent > 999) {
          if (p > dest + length - 5) {
            dest[0] = '\0';
            return 0;
          }
          memcpy(p, asciiDigitTable + (base10Exponent / 100) * 2, 2);
          p += 2;
        } else {
          if (p > dest + length - 4) {
            dest[0] = '\0';
            return 0;
          }
          *p++ = (base10Exponent / 100) + '0';
        }
        base10Exponent %= 100;
      }
      memcpy(p, asciiDigitTable + base10Exponent * 2, 2);
      p += 2;
    } else if (base10Exponent < 0) { // "0123456" => "0.00123456"
      // Slide digits back in buffer and prepend zeros and a period
      if (p > dest + length + base10Exponent - 1) {
        dest[0] = '\0';
        return 0;
      }
      memmove(firstOutputChar - base10Exponent, firstOutputChar, p - firstOutputChar);
      /* coverity[NO_EFFECT] */
      memset(firstOutputChar, '0', -base10Exponent);
      firstOutputChar[1] = '.';
      p += -base10Exponent;
    } else if (base10Exponent + 1 < digitCount) { // "0123456" => "123.456"
      // Slide integer digits forward and insert a '.'
      memmove(firstOutputChar, firstOutputChar + 1, base10Exponent + 1);
      firstOutputChar[base10Exponent + 1] = '.';
    } else { // "0123456" => "12345600.0"
      // Slide digits forward 1 and append suitable zeros and '.0'
      if (p + base10Exponent - digitCount > dest + length - 3) {
        dest[0] = '\0';
        return 0;
      }
      memmove(firstOutputChar, firstOutputChar + 1, p - firstOutputChar - 1);
      p -= 1;
      memset(p, '0', base10Exponent - digitCount + 1);
      p += base10Exponent - digitCount + 1;
//      *p++ = '.'; // XXX
//      *p++ = '0'; // XXX
    }
    *p = '\0';
    return p - dest;
}

// Format an IEEE 754 double-precision binary64 format floating-point number.

// The calling convention here assumes that C `double` is this format,
// but otherwise, this does not utilize any floating-point arithmetic
// or library routines.
static size_t swift_dtoa_optimal_binary64_p(const void *d, char *dest, size_t length)
{
    // Bits in raw significand (not including hidden bit, if present)
    static const int significandBitCount = DBL_MANT_DIG - 1;
    static const uint64_t significandMask
        = ((uint64_t)1 << significandBitCount) - 1;
    // Bits in raw exponent
    static const int exponentBitCount = 11;
    static const int exponentMask = (1 << exponentBitCount) - 1;
    // Note: IEEE 754 conventionally uses 1023 as the exponent
    // bias.  That's because they treat the significand as a
    // fixed-point number with one bit (the hidden bit) integer
    // portion.  The logic here reconstructs the significand as a
    // pure fraction, so we need to accommodate that when
    // reconstructing the binary exponent.
    static const int64_t exponentBias = (1 << (exponentBitCount - 1)) - 2; // 1022

    // Step 0: Deconstruct an IEEE 754 binary64 double-precision value
    uint64_t raw = *(const uint64_t *)d;
    int exponentBitPattern = (raw >> significandBitCount) & exponentMask;
    uint64_t significandBitPattern = raw & significandMask;
    int negative = raw >> 63;

    // Step 1: Handle the various input cases:
    if (length < 1) {
      return 0;
    }
    int binaryExponent;
    int isBoundary = significandBitPattern == 0;
    uint64_t significand;
    if (exponentBitPattern == exponentMask) { // NaN or Infinity
        if (isBoundary) { // Infinity
            return infinity(dest, length, negative);
        } else {
            const int quiet = (raw >> (significandBitCount - 1)) & 1;
            return nan_details(dest, length, negative, quiet);
        }
    } else if (exponentBitPattern == 0) {
        if (isBoundary) { // Zero
          return zero(dest, length, negative);
        } else { // subnormal
            binaryExponent = 1 - exponentBias;
            significand = significandBitPattern << (64 - significandBitCount - 1);
        }
    } else { // normal
        binaryExponent = exponentBitPattern - exponentBias;
        uint64_t hiddenBit = (uint64_t)1 << significandBitCount;
        uint64_t fullSignificand = significandBitPattern + hiddenBit;
        significand = fullSignificand << (64 - significandBitCount - 1);
    }

    // Step 2: Determine the exact unscaled target interval

    // Grisu-style algorithms construct the shortest decimal digit
    // sequence within a specific interval.  To build the appropriate
    // interval, we start by computing the midpoints between this
    // floating-point value and the adjacent ones.  Note that this
    // step is an exact computation.

    uint64_t halfUlp = (uint64_t)1 << (64 - significandBitCount - 2);
    uint64_t quarterUlp = halfUlp >> 1;
    uint64_t upperMidpointExact = significand + halfUlp;

    uint64_t lowerMidpointExact = significand - (isBoundary ? quarterUlp : halfUlp);

    int isOddSignificand = (significandBitPattern & 1) != 0;

    // Step 3: Estimate the base 10 exponent

    // Grisu algorithms are based in part on a simple technique for
    // generating a base-10 form for a binary floating-point number.
    // Start with a binary floating-point number `f * 2^e` and then
    // estimate the decimal exponent `p`. You can then rewrite your
    // original number as:
    //
    // ```
    //     f * 2^e * 10^-p * 10^p
    // ```
    //
    // The last term is part of our output, and a good estimate for
    // `p` will ensure that `2^e * 10^-p` is close to 1.  Multiplying
    // the first three terms then yields a fraction suitable for
    // producing the decimal digits.  Here we use a very fast estimate
    // of `p` that is never off by more than 1; we'll have
    // opportunities later to correct any error.

    int base10Exponent = decimalExponentFor2ToThe(binaryExponent);

    // Step 4: Compute a power-of-10 scale factor

    // Compute `10^-p` to 128-bit precision.  We generate
    // both over- and under-estimates to ensure we can exactly
    // bound the later use of these values.
    swift_uint128_t powerOfTenRoundedDown;
    swift_uint128_t powerOfTenRoundedUp;
    int powerOfTenExponent = 0;
    static const int bulkFirstDigits = 7;
    static const int bulkFirstDigitFactor = 1000000; // 10^(bulkFirstDigits - 1)
    // Note the extra factor of 10^bulkFirstDigits -- that will give
    // us a headstart on digit generation later on.  (In contrast, Ryu
    // uses an extra factor of 10^17 here to get all the digits up
    // front, but then has to back out any extra digits.  Doing that
    // with a 17-digit value requires 64-bit division, which is the
    // root cause of Ryu's poor performance on 32-bit processors.  We
    // also might have to back out extra digits if 7 is too many, but
    // will only need 32-bit division in that case.)
    intervalContainingPowerOf10_Binary64(-base10Exponent + bulkFirstDigits - 1,
                                       &powerOfTenRoundedDown,
                                       &powerOfTenRoundedUp,
                                       &powerOfTenExponent);
    const int extraBits = binaryExponent + powerOfTenExponent;

    // Step 5: Scale the interval (with rounding)

    // As mentioned above, the final digit generation works
    // with an interval, so we actually apply the scaling
    // to the upper and lower midpoint values separately.

    // As part of the scaling here, we'll switch from a pure
    // fraction with zero bit integer portion and 128-bit fraction
    // to a fixed-point form with 32 bits in the integer portion.
    static const int integerBits = 32;

    // We scale the interval in one of two different ways,
    // depending on whether the significand is even or odd...

    swift_uint128_t u, l;
    if (isOddSignificand) {
        // Case A: Narrow the interval (odd significand)

        // Loitsch' original Grisu2 always rounds so as to narrow the
        // interval.  Since our digit generation will select a value
        // within the scaled interval, narrowing the interval
        // guarantees that we will find a digit sequence that converts
        // back to the original value.

        // This ensures accuracy but, as explained in Loitsch' paper,
        // this carries a risk that there will be a shorter digit
        // sequence outside of our narrowed interval that we will
        // miss. This risk obviously gets lower with increased
        // precision, but it wasn't until the Errol paper that anyone
        // had a good way to test whether a particular implementation
        // had sufficient precision. That paper shows a way to enumerate
        // the worst-case numbers; those numbers that are extremely close
        // to the mid-points between adjacent floating-point values.
        // These are the values that might sit just outside of the
        // narrowed interval. By testing these values, we can verify
        // the correctness of our implementation.

        // Multiply out the upper midpoint, rounding down...
        swift_uint128_t u1 = multiply128x64RoundingDown(powerOfTenRoundedDown,
                                                    upperMidpointExact);
        // Account for residual binary exponent and adjust
        // to the fixed-point format
        u = shiftRightRoundingDown128(u1, integerBits - extraBits);

        // Conversely for the lower midpoint...
        swift_uint128_t l1 = multiply128x64RoundingUp(powerOfTenRoundedUp,
                                                  lowerMidpointExact);
        l = shiftRightRoundingUp128(l1, integerBits - extraBits);

    } else {
        // Case B: Widen the interval (even significand)

        // As explained in Errol Theorem 6, in certain cases there is
        // a short decimal representation at the exact boundary of the
        // scaled interval.  When such a number is converted back to
        // binary, it will get rounded to the adjacent even
        // significand.

        // So when the significand is even, we round so as to widen
        // the interval in order to ensure that the exact midpoints
        // are considered.  Of couse, this ensures that we find a
        // short result but carries a risk of selecting a result
        // outside of the exact scaled interval (which would be
        // inaccurate).

        // The same testing approach described above (based on results
        // in the Errol paper) also applies
        // to this case.

        swift_uint128_t u1 = multiply128x64RoundingUp(powerOfTenRoundedUp,
                                                  upperMidpointExact);
        u = shiftRightRoundingUp128(u1, integerBits - extraBits);

        swift_uint128_t l1 = multiply128x64RoundingDown(powerOfTenRoundedDown,
                                                    lowerMidpointExact);
        l = shiftRightRoundingDown128(l1, integerBits - extraBits);
    }

    // Step 6: Align first digit, adjust exponent

    // Calculations above used an estimate for the power-of-ten scale.
    // Here, we compensate for any error in that estimate by testing
    // whether we have the expected number of digits in the integer
    // portion and correcting as necesssary.  This also serves to
    // prune leading zeros from subnormals.

    // Except for subnormals, this loop should never run more than once.
    // For subnormals, this might run as many as 16 + bulkFirstDigits
    // times.
#if HAVE_UINT128_T
    while (u < ((__uint128_t)bulkFirstDigitFactor << (128 - integerBits)))
#else
    while (u.high < ((uint32_t)bulkFirstDigitFactor << (32 - integerBits)))
#endif
    {
        base10Exponent -= 1;
        multiply128xu32(&l, 10);
        multiply128xu32(&u, 10);
    }

    // Step 7: Produce decimal digits

    // One standard approach generates digits for the scaled upper and
    // lower boundaries and stops when at the first digit that
    // differs. For example, note that 0.1234 is the shortest decimal
    // between u = 0.123456 and l = 0.123345.

    // Grisu optimizes this by generating digits for the upper bound
    // (multiplying by 10 to isolate each digit) while simultaneously
    // scaling the interval width `delta`.  As we remove each digit
    // from the upper bound, the remainder is the difference between
    // the base-10 value generated so far and the true upper bound.
    // When that remainder is less than the scaled width of the
    // interval, we know the current digits specify a value within the
    // target interval.

    // The logic below actually blends three different digit-generation
    // strategies:
    // * The first digits are already in the integer portion of the
    //   fixed-point value, thanks to the `bulkFirstDigits` factor above.
    //   We can just break those down and write them out.
    // * If we generated too many digits, we use a Ryu-inspired technique
    //   to backtrack.
    // * If we generated too few digits (the usual case), we use an
    //   optimized form of the Grisu2 method to produce the remaining
    //   values.

    // Generate digits for `t` with interval width `delta = u - l`
    swift_uint128_t t = u;
    swift_uint128_t delta = u;
    subtract128x128(&delta, l);

    char *p = dest;
    if (negative) {
      if (p >= dest + length) {
        dest[0] = '\0';
        return 0;
      }
      *p++ = '-';
    }
    char * const firstOutputChar = p;

    // The `bulkFirstDigits` adjustment above already set up the first 7 digits
    // Format as 8 digits (with a leading zero that we'll exploit later on).
    uint32_t d12345678 = extractIntegerPart128(&t, integerBits);

    if (!isLessThan128x128(delta, t)) {
      // Oops!  We have too many digits.  Back out the extra ones to
      // get the right answer.  This is similar to Ryu, but since
      // we've only produced seven digits, we only need 32-bit
      // arithmetic here.  A few notes:
      // * Our target hardware always supports 32-bit hardware division,
      //   so this should be reasonably fast.
      // * For small integers (like "2"), Ryu would have to back out 16
      //   digits; we only have to back out 6.
      // * Very few double-precision values actually need fewer than 7
      //   digits.  So this is rarely used except in workloads that
      //   specifically use double for small integers.  This is more
      //   common for binary32, of course.

      // TODO: Add benchmarking for "small integers" -1000...1000 to
      // verify that this does not unduly penalize those values.

      // Why this is critical for performance: In order to use the
      // 8-digits-at-a-time optimization below, we need at least 30
      // bits in the integer part of our fixed-point format above.  If
      // we only use bulkDigits = 1, that leaves only 128 - 30 = 98
      // bit accuracy for our scaling step, which isn't enough
      // (binary64 needs ~110 bits for correctness).  So we have to
      // use a large bulkDigits value to make full use of the 128-bit
      // scaling above, which forces us to have some form of logic to
      // handle the case of too many digits.  The alternatives are to
      // use >128 bit values (slower) or do some complex finessing of
      // bit counts by working with powers of 5 instead of 10.

#if HAVE_UINT128_T
      uint64_t uHigh = u >> 64;
      uint64_t lHigh = l >> 64;
      if (0 != (uint64_t)l) {
        lHigh += 1;
      }
#else
      uint64_t uHigh = ((uint64_t)u.high << 32) + u.c;
      uint64_t lHigh = ((uint64_t)l.high << 32) + l.c;
      if (0 != (l.b | l.low)) {
        lHigh += 1;
      }
#endif
      uint64_t tHigh;
      if (isBoundary) {
        tHigh = (uHigh + lHigh * 2) / 3;
      } else {
        tHigh = (uHigh + lHigh) / 2;
      }

      uint32_t u0 = uHigh >> (64 - integerBits);
      uint32_t l0 = lHigh >> (64 - integerBits);
      if ((lHigh & ((1ULL << (64 - integerBits)) - 1)) != 0) {
        l0 += 1;
      }
      uint32_t t0 = tHigh >> (64 - integerBits);
      int t0digits = 8;

      uint32_t u1 = u0 / 10;
      uint32_t l1 = (l0 + 9) / 10;
      int trailingZeros = is128bitZero(t);
      int droppedDigit = ((tHigh * 10) >> (64 - integerBits)) % 10;
      while (u1 >= l1 && u1 != 0) {
        u0 = u1;
        l0 = l1;
        trailingZeros &= droppedDigit == 0;
        droppedDigit = t0 % 10;
        t0 /= 10;
        t0digits--;
        u1 = u0 / 10;
        l1 = (l0 + 9) / 10;
      }
      // Correct the final digit
      if (droppedDigit > 5 || (droppedDigit == 5 && !trailingZeros)) {
        t0 += 1;
      } else if (droppedDigit == 5 && trailingZeros) {
        t0 += 1;
        t0 &= ~1;
      }
      // t0 has t0digits digits.  Write them out
      if (p > dest + length - t0digits - 1) { // Make sure we have space
        dest[0] = '\0';
        return 0;
      }
      int i = t0digits;
      while (i > 1) { // Write out 2 digits at a time back-to-front
        i -= 2;
        memcpy(p + i, asciiDigitTable + (t0 % 100) * 2, 2);
        t0 /= 100;
      }
      if (i > 0) { // Handle an odd number of digits
        p[0] = t0 + '0';
      }
      p += t0digits; // Move the pointer past the digits we just wrote
    } else {
      //
      // Our initial scaling did not produce too many digits.
      // The `d12345678` value holds the first 7 digits (plus
      // a leading zero that will be useful later).  We write
      // those out and then incrementally generate as many
      // more digits as necessary.  The remainder of this
      // algorithm is basically just Grisu2.
      //

      if (p > dest + length - 9) {
        dest[0] = '\0';
        return 0;
      }
      // Write out the 7 digits we got earlier + leading zero
      int d1234 = d12345678 / 10000;
      int d5678 = d12345678 % 10000;
      int d78 = d5678 % 100;
      int d56 = d5678 / 100;
      memcpy(p + 6, asciiDigitTable + d78 * 2, 2);
      memcpy(p + 4, asciiDigitTable + d56 * 2, 2);
      int d34 = d1234 % 100;
      int d12 = d1234 / 100;
      memcpy(p + 2, asciiDigitTable + d34 * 2, 2);
      memcpy(p, asciiDigitTable + d12 * 2, 2);
      p += 8;

      // Seven digits wasn't enough, so let's get some more.
      // Most binary64 values need >= 15 digits total.  We already have seven,
      // so try grabbing the next 8 digits all at once.
      // (This is suboptimal for binary32, but the code savings
      // from sharing this implementation are worth it.)
      static const uint32_t bulkDigitFactor = 100000000; // 10^(15-bulkFirstDigits)
      swift_uint128_t d0 = delta;
      multiply128xu32(&d0, bulkDigitFactor);
      swift_uint128_t t0 = t;
      multiply128xu32(&t0, bulkDigitFactor);
      int bulkDigits = extractIntegerPart128(&t0, integerBits); // 9 digits
      if (isLessThan128x128(d0, t0)) {
        if (p > dest + length - 9) {
          dest[0] = '\0';
          return 0;
        }
        // Next 8 digits are good; add them to the output
        int d1234 = bulkDigits / 10000;
        int d5678 = bulkDigits % 10000;
        int d78 = d5678 % 100;
        int d56 = d5678 / 100;
        memcpy(p + 6, asciiDigitTable + d78 * 2, 2);
        memcpy(p + 4, asciiDigitTable + d56 * 2, 2);
        int d34 = d1234 % 100;
        int d12 = d1234 / 100;
        memcpy(p + 2, asciiDigitTable + d34 * 2, 2);
        memcpy(p, asciiDigitTable + d12 * 2, 2);
        p += 8;

        t = t0;
        delta = d0;
      }

      // Finish up by generating and writing one digit at a time.
      while (isLessThan128x128(delta, t)) {
        if (p > dest + length - 2) {
          dest[0] = '\0';
          return 0;
        }
        multiply128xu32(&delta, 10);
        multiply128xu32(&t, 10);
        *p++ = '0' + extractIntegerPart128(&t, integerBits);
      }

      // Adjust the final digit to be closer to the original value.  This accounts
      // for the fact that sometimes there is more than one shortest digit
      // sequence.

      // For example, consider how the above would work if you had the
      // value 0.1234 and computed u = 0.1257, l = 0.1211.  The above
      // digit generation works with `u`, so produces 0.125.  But the
      // values 0.122, 0.123, and 0.124 are just as short and 0.123 is
      // therefore the best choice, since it's closest to the original
      // value.

      // We know delta and t are both less than 10.0 here, so we can
      // shed some excess integer bits to simplify the following:
      const int adjustIntegerBits = 4; // Integer bits for "adjust" phase
      shiftLeft128(&delta, integerBits - adjustIntegerBits);
      shiftLeft128(&t, integerBits - adjustIntegerBits);

      // Note: We've already consumed most of our available precision,
      // so it's okay to just work in 64 bits for this...
      uint64_t deltaHigh64 = extractHigh64From128(delta);
      uint64_t tHigh64 = extractHigh64From128(t);

      // If `delta < t + 1.0`, then the interval is narrower than
      // one decimal digit, so there is no other option.
      if (deltaHigh64 >= tHigh64 + ((uint64_t)1 << (64 - adjustIntegerBits))) {
        uint64_t skew;
        if (isBoundary) {
          // If we're at the boundary where the exponent shifts,
          // then the original value is 1/3 of the way from
          // the bottom of the interval ...
          skew = deltaHigh64 - deltaHigh64 / 3 - tHigh64;
        } else {
          // ... otherwise it's exactly in the middle.
          skew = deltaHigh64 / 2 - tHigh64;
        }

        // The `skew` above is the difference between our
        // computed digits and the original exact value.
        // Use that to offset the final digit:
        uint64_t one = (uint64_t)(1) << (64 - adjustIntegerBits);
        uint64_t fractionMask = one - 1;
        uint64_t oneHalf = one >> 1;
        if ((skew & fractionMask) == oneHalf) {
          int adjust = (int)(skew >> (64 - adjustIntegerBits));
          // If the skew is exactly integer + 1/2, round the
          // last digit even after adjustment
          p[-1] -= adjust;
          p[-1] &= ~1;
        } else {
          // Else round to nearest...
          int adjust = (int)((skew + oneHalf) >> (64 - adjustIntegerBits));
          p[-1] -= adjust;
        }
      }
    }

    // Step 8: Shuffle digits into the final textual form
    int forceExponential = binaryExponent > 54 || (binaryExponent == 54 && !isBoundary);
    return finishFormatting(dest, length, p, firstOutputChar, forceExponential, base10Exponent);
}

size_t dtoa(double d, char *dest, size_t length)
{
  return swift_dtoa_optimal_binary64_p(&d, dest, length);
}

