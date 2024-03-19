/* SPDX-License-Identifier: GPL-2.0-only OR MIT                 */
/* SPDX-FileCopyrightText: Copyright (C) 2013 Florian Forster   */
/* SPDX-FileContributor: Florian Forster <octo at collectd.org> */

#pragma once

/**
 * Returns a random double value in the range [0..1), i.e. excluding 1.
 *
 * This function is thread- and reentrant-safe.
 */
double cdrand_d(void);

/**
 * cdrand_u returns a random uint32_t value uniformly distributed in the range
 * [0-2^32).
 *
 * This function is thread- and reentrant-safe.
 */
uint32_t cdrand_u(void);

/**
 * Returns a random long between min and max, inclusively.
 *
 * If min is larger than max, the result may be rounded incorrectly and may be
 * outside the intended range. This function is thread- and reentrant-safe.
 */
long cdrand_range(long min, long max);
