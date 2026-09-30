// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

static char *path_proc_slabinfo;

enum {
    FAM_SLABINFO_OBJECTS_ACTIVE,
    FAM_SLABINFO_OBJECTS,
    FAM_SLABINFO_OBJECT_BYTES,
    FAM_SLABINFO_SLAB_OBJECTS,
    FAM_SLABINFO_SLAB_BYTES,
    FAM_SLABINFO_SLABS_ACTIVE,
    FAM_SLABINFO_SLABS,
    FAM_SLABINFO_MAX,
};

static metric_family_t fams[FAM_SLABINFO_MAX] = {
    [FAM_SLABINFO_OBJECTS_ACTIVE] = {
        .name = "system_slabinfo_objects_active",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of objects that are currently active (i.e., in use).",
    },
    [FAM_SLABINFO_OBJECTS] = {
        .name = "system_slabinfo_objects",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total number of allocated objects "
                "(i.e., objects that are both in use and not in use).",
    },
    [FAM_SLABINFO_OBJECT_BYTES] = {
        .name = "system_slabinfo_object_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The size of objects in this slab, in bytes.",
    },
    [FAM_SLABINFO_SLAB_OBJECTS] = {
        .name = "system_slabinfo_slab_objects",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of objects stored in each slab.",
    },
    [FAM_SLABINFO_SLAB_BYTES] = {
        .name = "system_slabinfo_slab_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of bytes allocated for each slab.",
    },
    [FAM_SLABINFO_SLABS_ACTIVE] = {
        .name = "system_slabinfo_slabs_active",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of active slabs.",
    },
    [FAM_SLABINFO_SLABS] = {
        .name = "system_slabinfo_slabs",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total number of slabs.",
    },
};

static exclist_t excl_cache;
static plugin_filter_t *filter;
static int64_t pagesize;

static int slabinfo_read(void)
{
    if (path_proc_slabinfo == NULL)
        return 0;

    FILE *fh = fopen(path_proc_slabinfo, "r");
    if (unlikely(fh == NULL)) {
        PLUGIN_ERROR("Unable to open '%s': %s", path_proc_slabinfo, STRERRNO);
        return -1;
    }

    char buffer[512];
    if (unlikely(fgets(buffer, sizeof(buffer), fh) == NULL)) {
        PLUGIN_ERROR("Unable to read %s", path_proc_slabinfo);
        fclose(fh);
        return -1;
    }

    if (unlikely(strcmp(buffer, "slabinfo - version: 2.1\n") != 0)) {
        PLUGIN_ERROR("invalid version");
        fclose(fh);
        return -1;
    }

    char *fields[16];
    while(fgets(buffer, sizeof(buffer), fh) != NULL) {
        if (unlikely(buffer[0] == '#'))
            continue;

        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

        if (unlikely(fields_num < 16))
            continue;

        if (!exclist_match(&excl_cache, fields[0]))
            continue;

        uint64_t value;

        if (strtouint(fields[1], &value) == 0)
            metric_family_append(&fams[FAM_SLABINFO_OBJECTS_ACTIVE], VALUE_GAUGE(value), NULL,
                                 &LABEL_PAIR_CONST("cache_name", fields[0]), NULL);
        if (strtouint(fields[2], &value) == 0)
            metric_family_append(&fams[FAM_SLABINFO_OBJECTS], VALUE_GAUGE(value), NULL,
                                 &LABEL_PAIR_CONST("cache_name", fields[0]), NULL);
        if (strtouint(fields[3], &value) == 0)
            metric_family_append(&fams[FAM_SLABINFO_OBJECT_BYTES], VALUE_GAUGE(value), NULL,
                                 &LABEL_PAIR_CONST("cache_name", fields[0]), NULL);
        if (strtouint(fields[4], &value) == 0)
            metric_family_append(&fams[FAM_SLABINFO_SLAB_OBJECTS], VALUE_GAUGE(value), NULL,
                                 &LABEL_PAIR_CONST("cache_name", fields[0]), NULL);
        if (strtouint(fields[5], &value) == 0)
            metric_family_append(&fams[FAM_SLABINFO_SLAB_BYTES], VALUE_GAUGE(value * pagesize), NULL,
                                 &LABEL_PAIR_CONST("cache_name", fields[0]), NULL);
        if (strtouint(fields[13], &value) == 0)
            metric_family_append(&fams[FAM_SLABINFO_SLABS_ACTIVE], VALUE_GAUGE(value), NULL,
                                 &LABEL_PAIR_CONST("cache_name", fields[0]), NULL);
        if (strtouint(fields[14], &value) == 0)
            metric_family_append(&fams[FAM_SLABINFO_SLABS], VALUE_GAUGE(value), NULL,
                                 &LABEL_PAIR_CONST("cache_name", fields[0]), NULL);
    }

    fclose(fh);

    plugin_dispatch_metric_family_array_filtered(fams, FAM_SLABINFO_MAX, filter, 0);

    return 0;
}

static int slabinfo_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "cache") == 0) {
            status = cf_util_exclist(child, &excl_cache);
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = plugin_filter_configure(child, &filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0)
        return -1;

    return 0;
}

static int slabinfo_init(void)
{
    path_proc_slabinfo = plugin_procpath("slabinfo");
    if (path_proc_slabinfo == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    if (access(path_proc_slabinfo, R_OK) != 0) {
        PLUGIN_ERROR("Cannot access to '%s': %s.", path_proc_slabinfo, STRERRNO);
        free(path_proc_slabinfo);
        path_proc_slabinfo = NULL;
        return -1;
    }

    pagesize = (int64_t)sysconf(_SC_PAGESIZE);
    return 0;
}

static int slabinfo_shutdown(void)
{
    free(path_proc_slabinfo);
    exclist_reset(&excl_cache);
    plugin_filter_free(filter);
    return 0;
}

void module_register(void)
{
    plugin_register_config("slabinfo", slabinfo_config);
    plugin_register_init("slabinfo", slabinfo_init);
    plugin_register_read("slabinfo", slabinfo_read);
    plugin_register_shutdown("slabinfo", slabinfo_shutdown);
}
