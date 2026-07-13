// SPDX-License-Identifier: GPL-2.0-only OR MIT OR BSD-2-Clause
// SPDX-FileCopyrightText: Copyright (c) 2010 Pierre-Yves Ritschard
// SPDX-FileCopyrightText: Copyright (c) 2011 Stefan Rinkes
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileCopyrightText: Copyright (c) 2021 Rubicon Communications, LLC (Netgate)
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

#if defined(DIOCGETSTATUSNV)

#elif defined(DIOCGETSTATUS)

#ifndef FCNT_NAMES
#    if FCNT_MAX != 3
#        error "Unexpected value for FCNT_MAX"
#    endif
#    define FCNT_NAMES {"search", "insert", "removals", NULL}
#endif

#ifndef SCNT_NAMES
#    if SCNT_MAX != 3
#        error "Unexpected value for SCNT_MAX"
#    endif
#    define SCNT_NAMES {"search", "insert", "removals", NULL}
#endif

static char const *pf_reasons[PFRES_MAX + 1] = PFRES_NAMES;
static char const *pf_lcounters[LCNT_MAX + 1] = LCNT_NAMES;
static char const *pf_fcounters[FCNT_MAX + 1] = FCNT_NAMES;
static char const *pf_scounters[SCNT_MAX + 1] = SCNT_NAMES;

#else
#    error "Neither of them is defined: DIOCGETSTATUSNV DIOCGETSTATUS"
#endif

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

static int pf_ioctl(int dev, uint cmd, size_t size, nvlist_t **nvl)
{
    struct pfioc_nv nv;
    size_t nvlen;

    void *data = nvlist_pack(*nvl, &nvlen);
    if (nvlen > size)
        size = nvlen;

    int status = 0;

retry:
    nv.data = malloc(size);
    if (nv.data == NULL) {
        status = ENOMEM;
        goto out;
    }

    memcpy(nv.data, data, nvlen);

    nv.len = nvlen;
    nv.size = size;

    status = ioctl(dev, cmd, &nv);
    if ((status == -1) && (errno == ENOSPC)) {
        size *= 2;
        free(nv.data);
        goto retry;
    }

    nvlist_destroy(*nvl);
    *nvl = NULL;

    if (status == 0) {
        *nvl = nvlist_unpack(nv.data, nv.len, 0);
        if (*nvl == NULL) {
            status = EIO;
            goto out;
        }
    } else {
        status = errno;
    }

out:
    free(data);
    free(nv.data);

    return status;
}

static void pf_status_counters(const nvlist_t *nvl, metric_family_t *fam, char *label)
{
    size_t counters_len;
    const uint64_t *counts = nvlist_get_number_array(nvl, "counters", &counters_len);
    size_t names_len;
    const char *const *names = nvlist_get_string_array(nvl, "names", &names_len);

    assert(counters_len == names_len);

    for (size_t i = 0; i < counters_len; i++) {
        metric_family_append(fam, VALUE_COUNTER(counts[i]), NULL,
                             &LABEL_PAIR_CONST(label, names[i]), NULL);
    }
}

static int pf_read(void)
{
    int fd = open(pf_device, O_RDONLY);
    if (fd < 0) {
        PLUGIN_ERROR("Unable to open %s: %s", pf_device, STRERRNO);
        return -1;
    }

#ifdef DIOCGETSTATUS
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
                             &LABEL_PAIR_CONST("counter", pf_reasons[i]), NULL);
    for (int i = 0; i < LCNT_MAX; i++)
        metric_family_append(&fams[FAM_PF_LIMITS], VALUE_COUNTER(state.lcounters[i]), NULL,
                             &LABEL_PAIR_CONST("limit", pf_lcounters[i]), NULL);
    for (int i = 0; i < FCNT_MAX; i++)
        metric_family_append(&fams[FAM_PF_STATE], VALUE_COUNTER(state.fcounters[i]), NULL,
                             &LABEL_PAIR_CONST("state", pf_fcounters[i]), NULL);
    for (int i = 0; i < SCNT_MAX; i++)
        metric_family_append(&fams[FAM_PF_SOURCE], VALUE_COUNTER(state.scounters[i]), NULL,
                             &LABEL_PAIR_CONST("source", pf_scounters[i]), NULL);

    metric_family_append(&fams[FAM_PF_STATES], VALUE_GAUGE(state.states), NULL, NULL);

#elif defined(DIOCGETSTATUSNV)
    nvlist_t *nvl = nvlist_create(0);
    int status = pf_ioctl(fd, DIOCGETSTATUSNV, 4096, &nvl);
    if (status != 0) {
        PLUGIN_ERROR("ioctl(DIOCGETSTATUSNV) failed: %s", STRERRNO);
        nvlist_destroy(nvl);
        close(fd);
        return -1;
    }

    close(fd);

    pf_status_counters(nvlist_get_nvlist(nvl, "counters"), &fams[FAM_PF_COUNTERS], "counter");
    pf_status_counters(nvlist_get_nvlist(nvl, "lcounters"), &fams[FAM_PF_LIMITS], "limit");
    pf_status_counters(nvlist_get_nvlist(nvl, "fcounters"), &fams[FAM_PF_STATE], "state");
    pf_status_counters(nvlist_get_nvlist(nvl, "scounters"), &fams[FAM_PF_SOURCE], "source");

    metric_family_append(&fams[FAM_PF_STATES],
                         VALUE_GAUGE(nvlist_get_number(nvl, "states")), NULL, NULL);

    nvlist_destroy(nvl);
#endif

    plugin_dispatch_metric_family_array(fams, FAM_PF_MAX, 0);
    return 0;
}

void module_register(void)
{
    plugin_register_read("pf", pf_read);
}
