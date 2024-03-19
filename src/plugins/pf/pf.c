// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (c) 2010 Pierre-Yves Ritschard
// SPDX-FileCopyrightText: Copyright (c) 2011 Stefan Rinkes
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Pierre-Yves Ritschard <pyr at openbsd.org>
// SPDX-FileContributor: Stefan Rinkes <stefan.rinkes at gmail.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <net/pfvar.h>

#ifndef FCNT_NAMES
#if FCNT_MAX != 3
#error "Unexpected value for FCNT_MAX"
#endif
#define FCNT_NAMES {"search", "insert", "removals", NULL}
#endif

#ifndef SCNT_NAMES
#if SCNT_MAX != 3
#error "Unexpected value for SCNT_MAX"
#endif
#define SCNT_NAMES {"search", "insert", "removals", NULL}
#endif

static char const *pf_reasons[PFRES_MAX + 1] = PFRES_NAMES;
static char const *pf_lcounters[LCNT_MAX + 1] = LCNT_NAMES;
static char const *pf_fcounters[FCNT_MAX + 1] = FCNT_NAMES;
static char const *pf_scounters[SCNT_MAX + 1] = SCNT_NAMES;

static char const *pf_device = "/dev/pf";

enum {
    FAM_PF_COUNTERS,
    FAM_PF_LIMITS,
    FAM_PF_STATE,
    FAM_PF_SOURCE,
    FAM_PF_STATES,
    FAM_PF_MAX
};

static metric_family_t fams[FAM_PF_MAX] = {
    [FAM_PF_COUNTERS] = {
        .name = "system_pf_counters",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PF_LIMITS] = {
        .name = "system_pf_limits",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PF_STATE] = {
        .name = "system_pf_state",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PF_SOURCE] = {
        .name = "system_pf_source",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PF_STATES] = {
        .name = "system_pf_states",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
};

static int pf_read(void)
{
    int fd = open(pf_device, O_RDONLY);
    if (fd < 0) {
        PLUGIN_ERROR("Unable to open %s: %s", pf_device, STRERRNO);
        return -1;
    }

    struct pf_status state;
    int status = ioctl(fd, DIOCGETSTATUS, &state);
    if (status != 0) {
        PLUGIN_ERROR("ioctl(DIOCGETSTATUS) failed: %s", STRERRNO);
        close(fd);
        return -1;
    }

    close(fd);

    if (!state.running) {
        PLUGIN_WARNING("PF is not running.");
        return -1;
    }

    for (int i = 0; i < PFRES_MAX; i++)
        metric_family_append(&fams[FAM_PF_COUNTERS], VALUE_COUNTER(state.counters[i]), NULL,
                             &(label_pair_const_t){.name="counter", .value=pf_reasons[i]}, NULL);
    for (int i = 0; i < LCNT_MAX; i++)
        metric_family_append(&fams[FAM_PF_LIMITS], VALUE_COUNTER(state.lcounters[i]), NULL,
                             &(label_pair_const_t){.name="limit", .value=pf_lcounters[i]}, NULL);
    for (int i = 0; i < FCNT_MAX; i++)
        metric_family_append(&fams[FAM_PF_STATE], VALUE_COUNTER(state.fcounters[i]), NULL,
                             &(label_pair_const_t){.name="state", .value=pf_fcounters[i]}, NULL);
    for (int i = 0; i < SCNT_MAX; i++)
        metric_family_append(&fams[FAM_PF_SOURCE], VALUE_COUNTER(state.scounters[i]), NULL,
                             &(label_pair_const_t){.name="source", .value=pf_scounters[i]}, NULL);

    metric_family_append(&fams[FAM_PF_STATES], VALUE_GAUGE(state.states), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_PF_MAX, 0);
    return 0;
}

void module_register(void)
{
    plugin_register_read("pf", pf_read);
}
