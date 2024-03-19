// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText Copyright (C) 2019 Asaf Kahlon
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Asaf Kahlon <asafka7 at gmail.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

#include <unistd.h>

#define MAX_ORDER 11
#define BUDDYINFO_FIELDS MAX_ORDER + 4 // "Node" + node_num + "zone" + Name + (MAX_ORDER entries)
#define NUM_OF_KB(pagesize, order) ((pagesize) / 1024) * (1 << (order))

static char *path_proc_buddyinfo;
static int pagesize;
static exclist_t excl_zone;

static metric_family_t fam = {
    .name = "system_buddyinfo_freepages",
    .type = METRIC_TYPE_GAUGE,
    .help = "Number of pages of a certain order (a certain size) "
            "that are available at any given time."
};

static int buddyinfo_read(void)
{
    FILE *fh = fopen(path_proc_buddyinfo, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Cannot open '%s': %s", path_proc_buddyinfo, STRERRNO);
        return -1;
    }

    cdtime_t submit = cdtime();

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *dummy = strstr(buffer, "Node");
        if (dummy == NULL)
            continue;

        char *fields[BUDDYINFO_FIELDS];
        int numfields = strsplit(dummy, fields, BUDDYINFO_FIELDS);
        if (numfields != BUDDYINFO_FIELDS) {
            PLUGIN_WARNING("Line '%s' doesn't contain %d orders, skipping...", buffer, MAX_ORDER);
            continue;
        }

        int node_num = atoi(fields[1]);
        char *zone = fields[3];

        if (!exclist_match(&excl_zone, zone))
            continue;

        char node_name[16];
        ssnprintf(node_name, sizeof(node_name), "%d", node_num);

        for (int i = 1; i <= MAX_ORDER; i++) {
            char pagesize_kb[8];
            ssnprintf(pagesize_kb, sizeof(pagesize_kb), "%d", NUM_OF_KB(pagesize, i - 1));
            metric_family_append(&fam, VALUE_GAUGE(atoi(fields[i + 3])), NULL,
                                 &(label_pair_const_t){.name="node", .value=node_name},
                                 &(label_pair_const_t){.name="zone", .value=zone},
                                 &(label_pair_const_t){.name="pagesize_kb", .value=pagesize_kb},
                                 NULL);
        }
    }

    fclose(fh);

    plugin_dispatch_metric_family(&fam, submit);

    return 0;
}

static int buddyinfo_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "zone") == 0) {
            status = cf_util_exclist(child, &excl_zone);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}


static int buddyinfo_init(void)
{
    pagesize = getpagesize();

    path_proc_buddyinfo = plugin_procpath("buddyinfo");
    if (path_proc_buddyinfo == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int buddyinfo_shutdown(void)
{
    free(path_proc_buddyinfo);
    exclist_reset(&excl_zone);
    return 0;
}

void module_register(void)
{
    plugin_register_init("buddyinfo", buddyinfo_init);
    plugin_register_config("buddyinfo", buddyinfo_config);
    plugin_register_read("buddyinfo", buddyinfo_read);
    plugin_register_shutdown("buddy", buddyinfo_shutdown);
}
