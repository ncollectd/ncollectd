/* SPDX-License-Identifier: GPL-2.0-only OR ISC                      */
/* SPDX-FileCopyrightText: Copyright (C) 2009 Florian octo Forster   */
/* SPDX-FileContributor: Florian octo Forster <octo at verplant.org> */
/* from librouteros - src/ros_parse.h                                */

#pragma once

#include <stdbool.h>

bool ros_sstrtob (const char *str);

unsigned int ros_sstrtoui (const char *str);

uint64_t ros_sstrtoui64 (const char *str);

double ros_sstrtod (const char *str);

int ros_sstrto_rx_tx_counters (const char *str, uint64_t *rx, uint64_t *tx);

uint64_t _ros_sstrtodate (const char *str, bool have_hour);

#define ros_sstrtodate(str) _ros_sstrtodate((str), 0)
