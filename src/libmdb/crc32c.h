/* SPDX-License-Identifier: GPL-2.0                                                   */
/* SPDX-FileCopyrightText: Copyright (C) 92, 1995-1999 Free Software Foundation, Inc. */

#pragma once

#include <inttypes.h>

uint32_t crc32c(unsigned char const *, unsigned long);

#if 0
extern bool crc32c_arm64_available;
extern bool crc32c_intel_available;

#ifdef ARCH_HAVE_CRC_CRYPTO
extern uint32_t crc32c_arm64(unsigned char const *, unsigned long);
extern void crc32c_arm64_probe(void);
#else
#define crc32c_arm64 crc32c_sw
static inline void crc32c_arm64_probe(void)
{
}
#endif /* ARCH_HAVE_CRC_CRYPTO */

#ifdef ARCH_HAVE_SSE4_2
extern uint32_t crc32c_intel(unsigned char const *, unsigned long);
extern void crc32c_intel_probe(void);
#else
#define crc32c_intel crc32c_sw
static inline void crc32c_intel_probe(void)
{
}
#endif /* ARCH_HAVE_SSE4_2 */

static inline uint32_t fio_crc32c(unsigned char const *buf, unsigned long len)
{
    if (crc32c_arm64_available)
        return crc32c_arm64(buf, len);

    if (crc32c_intel_available)
        return crc32c_intel(buf, len);

    return crc32c_sw(buf, len);
}
#endif
