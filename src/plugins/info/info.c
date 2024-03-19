// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"

static metric_family_t **info_fams;
static size_t info_fams_num;

static int info_read(void)
{
    if (info_fams == NULL)
        return 0;

    for (size_t i = 0; i < info_fams_num; i++) {
        metric_family_t fam = {
            .name = info_fams[i]->name,
            .help = info_fams[i]->help,
            .type = METRIC_TYPE_INFO,
        };

        int status = metric_list_clone(&fam.metric, info_fams[i]->metric, &fam);
        if (status != 0) {
            PLUGIN_ERROR("metric_list_clone failed.");
            continue;
        }

        plugin_dispatch_metric_family(&fam, 0);
    }

    return 0;
}

static int info_metric_append(char *name, char *help, metric_t *m)
{
    for (size_t i = 0; i < info_fams_num; i++) {
        metric_family_t *cur_fam = info_fams[i];
        if (strcmp(cur_fam->name, name) == 0) {
            metric_family_metric_append(cur_fam, *m);
            return 0;
        }
    }

    metric_family_t fam = {
        .name = name,
        .help = help,
        .type = METRIC_TYPE_INFO,
    };

    metric_family_t *new_fam = metric_family_clone(&fam);
    if (new_fam == NULL) {
        PLUGIN_ERROR("metric_family_clone failed.");
        return -1;
    }
    metric_family_metric_append(new_fam, *m);

    metric_family_t **tmp = realloc(info_fams, sizeof(*tmp) * (info_fams_num + 1));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        metric_family_free(new_fam);
        return -1;
    }
    info_fams = tmp;
    info_fams[info_fams_num] = new_fam;
    info_fams_num++;

    return 0;
}

static int info_config_metric(config_item_t *ci)
{
    char *name = NULL;
    char *help = NULL;
    metric_t m = {0};

    int status = cf_util_get_string(ci, &name);
    if (status != 0)
        return status;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *item = ci->children + i;

        if (!strcasecmp(item->key, "label")) {
            status = cf_util_get_label(item, &m.label);
        } else if (!strcasecmp(item->key, "help")) {
            status = cf_util_get_string(item, &help);
        } else if (!strcasecmp(item->key, "info")) {
            status = cf_util_get_label(item, &m.value.info);
        } else {
            PLUGIN_ERROR("Unknown configuration option: %s", item->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status == 0)
        status = info_metric_append(name, help, &m);

    label_set_reset(&m.value.info);

    free(name);
    free(help);
    metric_reset(&m, METRIC_TYPE_INFO);
    return status;
}

static int info_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("metric", child->key) == 0) {
            status = info_config_metric(child);
        } else {
            PLUGIN_ERROR("Unknown configuration option: %s", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int info_shutdown(void)
{
    if (info_fams == NULL)
        return 0;

    for (size_t i = 0; i < info_fams_num; i++) {
        metric_family_free(info_fams[i]);
    }

    free(info_fams);
    return 0;
}

static int info_init(void)
{
    metric_t m = {0};

    label_set_add(&m.value.info, true, "version", PACKAGE_VERSION);
    int status = info_metric_append("ncollectd", NULL, &m);
    metric_reset(&m, METRIC_TYPE_INFO);

    return status;
}

void module_register(void)
{
    plugin_register_init("info", info_init);
    plugin_register_config("info", info_config);
    plugin_register_read("info", info_read);
    plugin_register_shutdown("info", info_shutdown);
}
