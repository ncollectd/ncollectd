// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2020 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009 Simon Kuhnle
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Simon Kuhnle <simon at blarzwurst.de>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/kstat.h"

#include "memory.h"

extern metric_family_t fams[];

static int pagesize;

static kstat_ctl_t *kc;
static kstat_t *ksp;
static kstat_t *ksz;

int memory_read(void)
{
    /* Most of the additions here were taken as-is from the k9toolkit from
     * Brendan Gregg and are subject to change I guess */

    if ((ksp == NULL) || (ksz == NULL))
        return EINVAL;

    long long mem_used = get_kstat_value(ksp, "pagestotal");
    long long mem_free = get_kstat_value(ksp, "pagesfree");
    long long mem_lock = get_kstat_value(ksp, "pageslocked");
    long long mem_kern = 0;
    long long mem_unus = 0;
    long long arcsize = get_kstat_value(ksz, "size");

    long long pp_kernel = get_kstat_value(ksp, "pp_kernel");
    long long physmem = get_kstat_value(ksp, "physmem");
    long long availrmem = get_kstat_value(ksp, "availrmem");

    if ((mem_used < 0LL) || (mem_free < 0LL) || (mem_lock < 0LL)) {
        PLUGIN_ERROR("one of used, free or locked is negative.");
        return -1;
    }

    mem_unus = physmem - mem_used;

    if (mem_used < (mem_free + mem_lock)) {
        /* source: http://wesunsolve.net/bugid/id/4909199
         * this seems to happen when swap space is small, e.g. 2G on a 32G system
         * we will make some assumptions here
         * educated solaris internals help welcome here */
        PLUGIN_DEBUG("pages total is smaller than \"free\" "
                     "+ \"locked\". This is probably due to small swap space");
        mem_free = availrmem;
        mem_used = 0;
    } else {
        mem_used -= mem_free + mem_lock;
    }

    /* mem_kern is accounted for in mem_lock */
    if (pp_kernel < mem_lock) {
        mem_kern = pp_kernel;
        mem_lock -= pp_kernel;
    } else {
        mem_kern = mem_lock;
        mem_lock = 0;
    }

    metric_family_append(&fams[FAM_MEMORY_USED_BYTES],
                         VALUE_GAUGE(mem_used * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_FREE_BYTES],
                         VALUE_GAUGE(mem_free * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_LOCKED_BYTES],
                         VALUE_GAUGE(mem_lock * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_KERNEL_BYTES],
                         VALUE_GAUGE((mem_kern * pagesize) - arcsize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_UNUSED_BYTES],
                         VALUE_GAUGE(mem_unus * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_ARC_BYTES], VALUE_GAUGE(arcsize), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_MEMORY_MAX, 0);

    return 0;
}

int memory_init(void)
{
    /* getpagesize(3C) tells me this does not fail.. */
    pagesize = getpagesize();

    if (kc == NULL)
        kc = kstat_open();

    if (kc == NULL) {
        PLUGIN_ERROR("kstat_open failed.");
        return -1;
    }

    if (get_kstat(kc, &ksp, "unix", 0, "system_pages") != 0) {
        ksp = NULL;
        return -1;
    }

    if (get_kstat(kc, &ksz, "zfs", 0, "arcstats") != 0) {
        ksz = NULL;
        return -1;
    }

    return 0;
}
