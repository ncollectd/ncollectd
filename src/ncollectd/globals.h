/* SPDX-License-Identifier: GPL-2.0-only OR MIT          */
/* SPDX-FileCopyrightText: Copyright (C) 2017 Google LLC */

#pragma once

#include "libutils/time.h"
#include "libmetric/label_set.h"

/* hostname_set updates hostname_g */
void hostname_set(char const *hostname);

extern char *hostname_g;
extern cdtime_t interval_g;
extern int pidfile_from_cli;
extern int timeout_g;
extern label_set_t labels_g;
