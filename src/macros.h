/* SPDX-License-Identifier: GPL-2.0-only */

#pragma once

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

#define STATIC_ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
