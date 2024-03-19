// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

static char *path_sys_edac;

enum {
    FAM_EDAC_MC_CORRECTABLE_ERRORS,
    FAM_EDAC_MC_UNCORRECTABLE_ERRORS,
    FAM_EDAC_CSROW_CORRECTABLE_ERRORS,
    FAM_EDAC_CSROW_UNCORRECTABLE_ERRORS,
    FAM_EDAC_CHANNEL_CORRECTABLE_ERRORS,
    FAM_EDAC_MAX,
};

static metric_family_t fams[FAM_EDAC_MAX] = {
    [FAM_EDAC_MC_CORRECTABLE_ERRORS] = {
        .name = "system_edac_mc_correctable_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total count of correctable errors that have occurred on this memory controller.",
    },
    [FAM_EDAC_MC_UNCORRECTABLE_ERRORS] = {
        .name = "system_edac_mc_uncorrectable_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total count of uncorrectable errors that have occurred on this memory controller.",
    },
    [FAM_EDAC_CSROW_CORRECTABLE_ERRORS] = {
        .name = "system_edac_csrow_correctable_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total correctable memory errors for this csrow.",
    },
    [FAM_EDAC_CSROW_UNCORRECTABLE_ERRORS] = {
        .name = "system_edac_csrow_uncorrectable_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total uncorrectable memory errors for this csrow.",
    },
    [FAM_EDAC_CHANNEL_CORRECTABLE_ERRORS] = {
        .name = "system_edac_channel_correctable_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total count of correctable errors that have occurred on this channel.",
    },
};

typedef struct {
    const char *controller;
    const char *csrow;
} edac_labels_t;

static int edac_read_channel(__attribute__((unused)) int dir_fd,
                             __attribute__((unused)) const char *path,
                             const char *entry, void *ud)
{

    if (strncmp(entry, "ch", strlen("ch")) != 0)
        return 0;
    char *end = strchr(entry, '_');
    if (end == NULL)
        return 0;
    if (strcmp(end, "_ce_count") != 0)
        return 0;

    uint64_t value;
    int status = filetouint_at(dir_fd, entry, &value);
    if (likely(status == 0)) {
        const char *channel = entry + strlen("ch");
        *end = '\0';

        edac_labels_t *el = ud;

        metric_family_append(&fams[FAM_EDAC_CHANNEL_CORRECTABLE_ERRORS], VALUE_COUNTER(value), NULL,
                             &(label_pair_const_t){.name="controller", .value=el->controller},
                             &(label_pair_const_t){.name="csrow", .value=el->csrow},
                             &(label_pair_const_t){.name="channel", .value=channel}, NULL);
    }

    return 0;
}

static int edac_read_csrow(int dir_fd, __attribute__((unused)) const char *path,
                           const char *entry, void *ud)
{
    if (strncmp(entry, "csrow", strlen("csrow")) != 0)
        return 0;

    const char *csrow = entry + strlen("csrow");
    edac_labels_t *el = ud;
    uint64_t value = 0;
    char fpath[PATH_MAX];
    int status = 0;

    el->csrow = csrow;

    ssnprintf(fpath, sizeof(fpath), "%s/%s", entry, "ce_count");
    status = filetouint_at(dir_fd, fpath, &value);
    if (likely(status == 0))
        metric_family_append(&fams[FAM_EDAC_CSROW_CORRECTABLE_ERRORS], VALUE_COUNTER(value), NULL,
                             &(label_pair_const_t){.name="controller", .value=el->controller},
                             &(label_pair_const_t){.name="csrow", .value=csrow},
                             NULL);

    ssnprintf(fpath, sizeof(fpath), "%s/%s", entry, "ue_count");
    status = filetouint_at(dir_fd, fpath, &value);
    if (likely(status == 0))
        metric_family_append(&fams[FAM_EDAC_CSROW_UNCORRECTABLE_ERRORS], VALUE_COUNTER(value), NULL,
                             &(label_pair_const_t){.name="controller", .value=el->controller},
                             &(label_pair_const_t){.name="csrow", .value=csrow},
                             NULL);

    walk_directory_at(dir_fd, entry, edac_read_channel, el, 0);
    return 0;
}

static int edac_read_mc(int dir_fd, __attribute__((unused)) const char *path,
                        const char *entry, __attribute__((unused))  void *ud)
{
    if (strncmp(entry, "mc", strlen("mc")) != 0)
        return 0;

    const char *controller = entry + strlen("mc");
    uint64_t value = 0;
    int status = 0;
    char fpath[PATH_MAX];

    ssnprintf(fpath, sizeof(fpath), "%s/%s", entry, "ce_count");
    status = filetouint_at(dir_fd, fpath, &value);
    if (likely(status == 0))
        metric_family_append(&fams[FAM_EDAC_MC_CORRECTABLE_ERRORS], VALUE_COUNTER(value), NULL,
                             &(label_pair_const_t){.name="controller", .value=controller},
                             NULL);

    ssnprintf(fpath, sizeof(fpath), "%s/%s", entry, "ue_count");
    status = filetouint_at(dir_fd, fpath, &value);
    if (likely(status == 0))
        metric_family_append(&fams[FAM_EDAC_MC_UNCORRECTABLE_ERRORS], VALUE_COUNTER(value), NULL,
                             &(label_pair_const_t){.name="controller", .value=controller},
                             NULL);

    ssnprintf(fpath, sizeof(fpath), "%s/%s", entry, "ce_noinfo_count");
    status = filetouint_at(dir_fd, fpath, &value);
    if (likely(status == 0))
        metric_family_append(&fams[FAM_EDAC_CSROW_CORRECTABLE_ERRORS], VALUE_COUNTER(value), NULL,
                             &(label_pair_const_t){.name="controller", .value=controller},
                             &(label_pair_const_t){.name="csrow", .value="unknown"},
                             NULL);

    ssnprintf(fpath, sizeof(fpath), "%s/%s", entry, "ue_noinfo_count");
    status = filetouint_at(dir_fd, fpath, &value);
    if (likely(status == 0))
        metric_family_append(&fams[FAM_EDAC_CSROW_UNCORRECTABLE_ERRORS], VALUE_COUNTER(value), NULL,
                             &(label_pair_const_t){.name="controller", .value=controller},
                             &(label_pair_const_t){.name="csrow", .value="unknown"},
                             NULL);

    edac_labels_t el = {.controller = controller };

    walk_directory_at(dir_fd, entry, edac_read_csrow, &el, 0);

    return 0;
}

static int edac_read(void)
{
    walk_directory(path_sys_edac, edac_read_mc, NULL, 0);
    plugin_dispatch_metric_family_array(fams, FAM_EDAC_MAX, 0);
    return 0;
}

static int edac_init(void)
{
    path_sys_edac = plugin_syspath("devices/system/edac/mc");
    if (path_sys_edac == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    return 0;
}

static int edac_shutdown(void)
{
    free(path_sys_edac);
    return 0;
}

void module_register(void)
{
    plugin_register_init("edac", edac_init);
    plugin_register_read("edac", edac_read);
    plugin_register_shutdown("edac", edac_shutdown);
}
