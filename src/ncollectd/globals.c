// SPDX-License-Identifier: GPL-2.0-only OR MIT
// Copyright (C) 2017  Google LLC

/*
 * Explicit order is required or _FILE_OFFSET_BITS will have definition mismatches on Solaris
 * See Github Issue #3193 for details
 */
#include "utils/common/common.h"
#include "globals.h"
// clang-format on

#if HAVE_KSTAT_H
#include <kstat.h>
#endif

/*
 * Global variables
 */
char *hostname_g;
cdtime_t interval_g;
int timeout_g;
#if HAVE_KSTAT_H
kstat_ctl_t *kc;
#endif

void hostname_set(char const *hostname) {
  char *h = strdup(hostname);
  if (h == NULL)
    return;

  sfree(hostname_g);
  hostname_g = h;
}
