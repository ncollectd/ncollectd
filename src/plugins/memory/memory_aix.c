// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2020 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009 Simon Kuhnle
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Simon Kuhnle <simon at blarzwurst.de>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <libperfstat.h>
#include <sys/protosw.h>

#include "memory.h"

extern metric_family_t fams[];

static int pagesize;

int memory_read(void)
{
    perfstat_memory_total_t pmemory = {0};

    int status = perfstat_memory_total(NULL, &pmemory, sizeof(pmemory), 1);
    if (status < 0) {
        PLUGIN_ERROR("perfstat_memory_total failed: %s", STRERRNO);
        return errno;
    }

    /* Unfortunately, the AIX documentation is not very clear on how these
     * numbers relate to one another. The only thing is states explicitly
     * is:
     *   real_total = real_process + real_free + numperm + real_system
     *
     * Another segmentation, which would be closer to the numbers reported
     * by the "svmon" utility, would be:
     *   real_total = real_free + real_inuse
     *   real_inuse = "active" + real_pinned + numperm
     */

    metric_family_append(&fams[FAM_MEMORY_FREE_BYTES],
                         VALUE_GAUGE(pmemory.real_free * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_CACHED_BYTES],
                         VALUE_GAUGE(pmemory.numperm * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_KERNEL_BYTES],
                         VALUE_GAUGE(pmemory.real_system * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_MEMORY_USED_BYTES],
                         VALUE_GAUGE(pmemory.real_process * pagesize), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_MEMORY_MAX, 0);

    return 0;
}

int memory_init(void)
{
    pagesize = getpagesize();
    return 0;
}
