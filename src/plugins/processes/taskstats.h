/* SPDX-License-Identifier: GPL-2.0-only OR ISC                      */
/* SPDX-FileCopyrightText: Copyright (C) 2017 Florian octo Forster   */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org> */

#pragma once

#include "libutils/time.h"

struct ts_s;
typedef struct ts_s ts_t;

typedef struct {
    uint64_t cpu_ns;
    uint64_t blkio_ns;
    uint64_t swapin_ns;
    uint64_t freepages_ns;
    /* v14: Delay waiting for IRQ/SOFTIRQ */
    uint64_t irq_ns;
} ts_delay_t;

ts_t *ts_create(void);
void ts_destroy(ts_t *);

/* ts_delay_by_tgid returns Linux delay accounting information for the task
 * identified by tgid. Returns zero on success and an errno otherwise. */
int ts_delay_by_tgid(ts_t *ts, uint32_t tgid, ts_delay_t *out);
