// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2007 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifdef KERNEL_LINUX

static char *path_proc_entropy_avail;
static char *path_proc_poolsize;

#elif defined(KERNEL_NETBSD)
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/rnd.h>
#include <sys/types.h>
#ifdef HAVE_SYS_RNDIO_H
#include <sys/rndio.h>
#endif
#include <paths.h>

#else
#error "No applicable input method."
#endif

enum {
    FAM_HOST_ENTROPY_AVAILABLE_BITS,
    FAM_HOST_ENTROPY_POOL_SIZE_BITS,
    FAM_HOST_ENTROPY_MAX,
};

static metric_family_t fams[FAM_HOST_ENTROPY_MAX] = {
    [FAM_HOST_ENTROPY_AVAILABLE_BITS] = {
        .name = "system_entropy_available_bits",
        .type = METRIC_TYPE_GAUGE,
        .help = "Bits of available entropy",
    },
    [FAM_HOST_ENTROPY_POOL_SIZE_BITS] = {
        .name = "system_entropy_pool_size_bits",
        .type = METRIC_TYPE_GAUGE,
        .help = "Bits of entropy pool",
    },
};

static int entropy_read(void)
{
    double available = 0;
    double poolsize = 0;

#ifdef KERNEL_LINUX

    if (parse_double_file(path_proc_entropy_avail, &available) != 0) {
        PLUGIN_ERROR("Reading '%s' failed.", path_proc_entropy_avail);
        return -1;
    }

    if (parse_double_file(path_proc_poolsize, &poolsize) != 0) {
        PLUGIN_ERROR("Reading '%s' failed.", path_proc_poolsize);
        return -1;
    }

#elif defined(KERNEL_NETBSD)
/* Improved to keep the /dev/urandom open, since there's a consumption
 * of entropy from /dev/random for every open of /dev/urandom, and this
 * will end up opening /dev/urandom lots of times.
 */
    rndpoolstat_t rs;
    static int fd = 0;

    if (fd == 0) {
        fd = open(_PATH_URANDOM, O_RDONLY, 0644);
        if (fd < 0) {
            fd = 0;
            return -1;
        }
    }

    if (ioctl(fd, RNDGETPOOLSTAT, &rs) < 0) {
        close(fd);
        fd = 0; /* signal a reopening on next attempt */
        return -1;
    }

    available = rs.curentropy;
    poolsize = rs.poolsize;
#endif

    metric_family_append(&fams[FAM_HOST_ENTROPY_AVAILABLE_BITS],
                         VALUE_GAUGE(available), NULL, NULL);
    metric_family_append(&fams[FAM_HOST_ENTROPY_POOL_SIZE_BITS],
                         VALUE_GAUGE(poolsize), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_HOST_ENTROPY_MAX, 0);

    return 0;
}

#ifdef KERNEL_LINUX
static int entropy_init(void)
{
    path_proc_entropy_avail = plugin_procpath("sys/kernel/random/entropy_avail");
    if (path_proc_entropy_avail == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    path_proc_poolsize = plugin_procpath("sys/kernel/random/poolsize");
    if (path_proc_poolsize == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int entropy_shutdown(void)
{
    free(path_proc_entropy_avail);
    free(path_proc_poolsize);
    return 0;
}
#endif

void module_register(void)
{
#ifdef KERNEL_LINUX
    plugin_register_init("entropy", entropy_init);
    plugin_register_shutdown("entropy", entropy_shutdown);
#endif
    plugin_register_read("entropy", entropy_read);
}
