// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/avltree.h"

static char *path_proc_arp;

typedef struct {
    double num;
    char device[];
} arp_device_t;

static metric_family_t fam_arp = {
    .name = "system_arp_entries",
    .type = METRIC_TYPE_GAUGE,
    .help = "ARP entries by device.",
};

static int arp_read(void)
{
    FILE *fh = fopen(path_proc_arp, "r");
    if (unlikely(fh == NULL)) {
        PLUGIN_WARNING("Unable to open %s", path_proc_arp);
        return EINVAL;
    }

    cdtime_t submit = cdtime();
    c_avl_tree_t *tree = c_avl_create((int (*)(const void *, const void *))strcmp);
    if (unlikely(tree == NULL)) {
        fclose(fh);
        return -1;
    }

    char buffer[256];
    char *fields[8];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

        if (unlikely(fields_num < 6))
            continue;
        if (strcmp(fields[0], "IP") == 0)
            continue;

        char *device = fields[5];
        if (unlikely(device == NULL))
            continue;

        size_t len = strlen(device);
        if (unlikely(len == 0))
             continue;

        arp_device_t *arp_device = NULL;
        int status = c_avl_get(tree, device, (void *)&arp_device);
        if (status == 0) {
            assert(arp_device != NULL);
            arp_device->num++;
            continue;
        }

        arp_device = calloc(1, sizeof(*arp_device)+len+1);
        if (unlikely(arp_device == NULL))
            continue;

        arp_device->num = 1;
        memcpy(arp_device->device, device, len+1);
        status = c_avl_insert(tree, arp_device->device, arp_device);
        if (unlikely(status != 0)) {
            free(arp_device);
            continue;
        }
    }
    fclose(fh);

    while (true) {
        arp_device_t *arp_device = NULL;
        char *key = NULL;
        int status = c_avl_pick(tree, (void *)&key, (void *)&arp_device);
        if (status != 0)
            break;
        metric_family_append(&fam_arp, VALUE_GAUGE(arp_device->num), NULL,
                             &(label_pair_const_t){.name="device", .value=key}, NULL);
        free(arp_device);
    }

    c_avl_destroy(tree);
    plugin_dispatch_metric_family(&fam_arp, submit);
    return 0;
}

static int arp_init(void)
{
    path_proc_arp = plugin_procpath("net/arp");
    if (path_proc_arp == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int arp_shutdown(void)
{
    free(path_proc_arp);
    return 0;
}

void module_register(void)
{
    plugin_register_init("arp", arp_init);
    plugin_register_read("arp", arp_read);
    plugin_register_shutdown("arp", arp_shutdown);
}
