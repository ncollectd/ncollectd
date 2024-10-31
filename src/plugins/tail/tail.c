// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2007-2008 C-Ware, Inc.
// SPDX-FileCopyrightText: Copyright (C) 2008-2013 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2013 Kris Nielander
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Kris Nielander <nielander at fox-it.com>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Luke Heberling <lukeh at c-ware.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/tail.h"

typedef struct {
    tail_t tail;
    bool whole;
    label_set_t labels;
    plugin_filter_t *filter;
    plugin_match_t *matches;
} ctail_t;

static int ctail_read(user_data_t *ud)
{
    ctail_t *ctail = ud->data;

    while (true) {
        char buf[8192];
        int status = tail_readline(&ctail->tail, buf, sizeof(buf));
        if (status != 0) {
            PLUGIN_ERROR("File '%s': tail_readline failed with status %i.",
                         ctail->tail.file, status);
            return -1;
        }

        /* check for EOF */
        if (buf[0] == 0)
          break;

        size_t len = strlen(buf);
        while (len > 0) {
            if (buf[len - 1] != '\n')
                break;
            buf[len - 1] = '\0';
            len--;
        }

        if (len == 0)
            continue;

        status = plugin_match(ctail->matches, buf);
        if (status != 0)
            PLUGIN_WARNING("plugin_match failed.");
    }

    if (ctail->whole)
        tail_close(&ctail->tail);

    plugin_match_dispatch(ctail->matches, ctail->filter, &ctail->labels, true);

    return 0;
}

static void ctail_free(void *arg)
{
    ctail_t *ctail = arg;
    if (ctail == NULL)
        return;

    tail_reset(&ctail->tail);

    if (ctail->matches != NULL)
        plugin_match_shutdown(ctail->matches);

    plugin_filter_free(ctail->filter);

    label_set_reset(&ctail->labels);

    free(ctail);
    return;
}

static int ctail_config_file(config_item_t *ci)
{
    cdtime_t interval = 0;

    char *file = NULL;
    int status = cf_util_get_string(ci, &file);
    if (status != 0)
        return status;

    ctail_t *ctail  = calloc(1, sizeof(*ctail));
    if (ctail == NULL)
        return -1;
    ctail->tail.file = file;
    ctail->whole = false;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("whole", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctail->whole);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ctail->labels);
        } else if (strcasecmp("match", child->key) == 0) {
            status = plugin_match_config(child, &ctail->matches);
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = plugin_filter_configure(child, &ctail->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status == 0) {
        if (ctail->matches == NULL) {
            PLUGIN_ERROR("No (valid) 'match' block ");
            status = -1;
        }
    }

    if (status != 0) {
        ctail_free(ctail);
        return -1;
    }

    label_set_add(&ctail->labels, true, "instance", ctail->tail.file);

    return plugin_register_complex_read("tail", ctail->tail.file, ctail_read, interval,
                                        &(user_data_t){.data=ctail, .free_func=ctail_free});
}

static int ctail_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("file", child->key) == 0) {
            status = ctail_config_file(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("tail", ctail_config);
}
