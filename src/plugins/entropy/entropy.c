/**
 * collectd - src/entropy.c
 * Copyright (C) 2007       Florian octo Forster
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Florian octo Forster <octo at collectd.org>
 **/

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

static int entropy_read(void);

#if !KERNEL_LINUX && !KERNEL_NETBSD
#error "No applicable input method."
#endif

static void entropy_submit(gauge_t available, gauge_t poolsize)
{
  metric_family_t fams[] = {
    {
      .name = "host_entropy_available_bits",
      .help = "Bits of available entropy",
      .type = METRIC_TYPE_GAUGE,
    },
    {
      .name = "host_entropy_pool_size_bits",
      .help = "Bits of entropy pool",
      .type = METRIC_TYPE_GAUGE,
    },
  };

  metric_family_metric_append(&fams[0], (metric_t){ .value.gauge = available, });
  metric_family_metric_append(&fams[1], (metric_t){ .value.gauge = poolsize, });

  for (size_t i = 0; i < STATIC_ARRAY_SIZE(fams); i++) {
    int status = plugin_dispatch_metric_family(&fams[i]);
    if (status != 0) {
      ERROR("entropy plugin: plugin_dispatch_metric_family failed: %s",
            STRERROR(status));
    }
    metric_family_metric_reset(&fams[i]);
  }
}

#if KERNEL_LINUX
#define ENTROPY_AVAILABLE_FILE "/proc/sys/kernel/random/entropy_avail"
#define ENTROPY_POOLSIZE_FILE "/proc/sys/kernel/random/poolsize"

static int entropy_read(void)
{
  value_t available;
  if (parse_value_file(ENTROPY_AVAILABLE_FILE, &available, DS_TYPE_GAUGE) != 0) {
    ERROR("entropy plugin: Reading \"" ENTROPY_AVAILABLE_FILE "\" failed.");
    return -1;
  }
 
  value_t poolsize;
  if (parse_value_file(ENTROPY_POOLSIZE_FILE, &poolsize, DS_TYPE_GAUGE) != 0) {
    ERROR("entropy plugin: Reading \"" ENTROPY_POOLSIZE_FILE "\" failed.");
    return -1;
  }

  entropy_submit(available.gauge, poolsize.gauge);
  return 0;
}
#endif /* KERNEL_LINUX */

#if KERNEL_NETBSD
/* Provide a NetBSD implementation, partial from rndctl.c */

/*
 * Improved to keep the /dev/urandom open, since there's a consumption
 * of entropy from /dev/random for every open of /dev/urandom, and this
 * will end up opening /dev/urandom lots of times.
 */

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/rnd.h>
#include <sys/types.h>
#if HAVE_SYS_RNDIO_H
#include <sys/rndio.h>
#endif
#include <paths.h>

static int entropy_read(void)
{
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
    (void)close(fd);
    fd = 0; /* signal a reopening on next attempt */
    return -1;
  }

  entropy_submit(rs.curentropy, rs.poolsize);

  return 0;
}

#endif /* KERNEL_NETBSD */


void module_register(void)
{
  plugin_register_read("entropy", entropy_read);
}
