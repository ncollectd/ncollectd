// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2017 Google LLC

/*
 * Explicit order is required or _FILE_OFFSET_BITS will have definition mismatches on Solaris
 * See Github Issue #3193 for details
 */
#include "libutils/common.h"
#include "globals.h"

/*
 * Global variables
 */
char *hostname_g;
cdtime_t interval_g;
int timeout_g;
label_set_t labels_g;

void hostname_set(char const *hostname)
{
    char *h = strdup(hostname);
    if (h == NULL)
        return;

    free(hostname_g);
    hostname_g = h;
    label_set_add(&labels_g, true, "hostname", hostname_g);
}
