// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (c) 2015, Jiri Tyr
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Jiri Tyr <jiri.tyr at gmail.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

enum {
    FAM_HOST_FILE_HANDLES_USED,
    FAM_HOST_FILE_HANDLES_UNUSED,
    FAM_HOST_FILE_HANDLES_MAX,
    FAM_HOST_FILE_MAX,
};

static metric_family_t fams[FAM_HOST_FILE_MAX] = {
    [FAM_HOST_FILE_HANDLES_USED] = {
        .name = "system_file_handles_used",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_HOST_FILE_HANDLES_UNUSED] = {
        .name = "system_file_handles_unused",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_HOST_FILE_HANDLES_MAX] = {
        .name = "system_file_handles_max",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
};

static char *path_proc_file_nr;

static int fhcount_read(void)
{
    FILE *fp = fopen(path_proc_file_nr, "r");
    if (fp == NULL) {
        PLUGIN_ERROR("Cannot open '%s': %s", path_proc_file_nr, STRERRNO);
        return -1;
    }

    char buffer[64];
    if (fgets(buffer, sizeof(buffer), fp) == NULL) {
        PLUGIN_ERROR("fgets: %s", STRERRNO);
        fclose(fp);
        return -1;
    }
    fclose(fp);

    char *fields[3];
    int numfields = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
    if (numfields != 3) {
        PLUGIN_ERROR("Line doesn't contain 3 fields");
        return -1;
    }

    // Define the values
    double used = 0;
    strtodouble(fields[0], &used);
    metric_family_append(&fams[FAM_HOST_FILE_HANDLES_USED], VALUE_GAUGE(used), NULL, NULL);

    double unused = 0;
    strtodouble(fields[1], &unused);
    metric_family_append(&fams[FAM_HOST_FILE_HANDLES_UNUSED], VALUE_GAUGE(unused), NULL, NULL);

    double max = 0;
    strtodouble(fields[2], &max);
    metric_family_append(&fams[FAM_HOST_FILE_HANDLES_MAX], VALUE_GAUGE(max), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_HOST_FILE_MAX, 0);
    return 0;
}

static int fhcount_init(void)
{
    path_proc_file_nr = plugin_procpath("sys/fs/file-nr");
    if (path_proc_file_nr == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int fhcount_shutdown(void)
{
    free(path_proc_file_nr);
    return 0;
}

void module_register(void)
{
    plugin_register_init("fhcount", fhcount_init);
    plugin_register_read("fhcount", fhcount_read);
    plugin_register_shutdown("fhcount", fhcount_shutdown);
}
