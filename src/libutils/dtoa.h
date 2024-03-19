/* SPDX-License-Identifier: GPL-2.0-only */

#pragma once

#include <stddef.h>

#define DTOA_MAX 24

size_t dtoa(double d, char *dest, size_t length);
