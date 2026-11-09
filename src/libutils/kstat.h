/* SPDX-License-Identifier: GPL-2.0-only OR MIT                          */
/* SPDX-FileCopyrightText: Copyright (C) 2005-2014  Florian octo Forster */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org>     */
/* SPDX-FileContributor: Niki W. Waibel <niki.waibel at gmx.net>         */

#pragma once

#include <kstat.h>

int get_kstat(kstat_ctl_t *kc, kstat_t **ksp_ptr, char *module, int instance, char *name);

long long get_kstat_value(kstat_t *ksp, const char *name);
