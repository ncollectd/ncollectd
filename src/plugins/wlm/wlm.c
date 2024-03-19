// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2010-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include <sys/wlm.h>

static struct wlm_info *wlminfo;
static int prev_wlmcnt;

static exclist_t excl_class;

enum {
    FAM_WLM_CPU_RATIO,
    FAM_WLM_MEMORY_RATIO,
    FAM_WLM_IO_RATIO,
    FAM_WLM_MAX
};

static metric_family_t fams[FAM_WLM_MAX] = {
    [FAM_WLM_CPU_RATIO] = {
        .name = "system_wlm_cpu_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_WLM_MEMORY_RATIO] = {
        .name = "system_wlm_memory_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_WLM_IO_RATIO] = {
        .name = "system_wlm_io_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
};

static int wlm_read (void)
{
    struct wlm_args wlmargs = {.versflags = WLM_VERSION};

    /* Get the number of WLMs */
    int wlmcnt = 0;
    int status = wlm_get_info(&wlmargs, NULL, &wlmcnt);
    if (status != 0) {
        PLUGIN_WARNING("wlm_get_info: %s", STRERRNO);
        return -1;
    }

    if (wlmcnt <= 0)
        return 0;

    /*  If necessary, reallocate the "wlminfo" memory */
    if (prev_wlmcnt != wlmcnt || wlminfo == NULL) {
        struct wlm_info *tmp = realloc(wlminfo, wlmcnt * sizeof(struct wlm_info));
        if (tmp == NULL) {
            PLUGIN_ERROR("realloc failed.");
            return -1;
        }
        wlminfo = tmp;
    }
    prev_wlmcnt = wlmcnt;

    /* Get wlm_info of all WLMs */
    status = wlm_get_info(&wlmargs, wlminfo, &wlmcnt);
    if (status != 0) {
        PLUGIN_WARNING ("wlm_get_info: %s", STRERRNO);
        return -1;
    }

    /* Iterate over all WLMs and dispatch information */
    struct wlm_info *pwlminfo;
    int i;
    for (i = 0, pwlminfo = wlminfo; i < wlmcnt; i++, pwlminfo++) {
        if (!exclist_match(&excl_class, pwlminfo->i_descr.name))
            continue;

        metric_family_append(&fams[FAM_WLM_CPU_RATIO],
                             VALUE_GAUGE((double)pwlminfo->i_regul[WLM_RES_CPU].consum/100.0), NULL,
                             &(label_pair_const_t){.name="class", .value=pwlminfo->i_descr.name},
                             NULL);
        metric_family_append(&fams[FAM_WLM_MEMORY_RATIO],
                             VALUE_GAUGE((double)pwlminfo->i_regul[WLM_RES_MEM].consum/100.0), NULL,
                             &(label_pair_const_t){.name="class", .value=pwlminfo->i_descr.name},
                             NULL);
        metric_family_append(&fams[FAM_WLM_IO_RATIO],
                             VALUE_GAUGE((double)pwlminfo->i_regul[WLM_RES_BIO].consum/100.0), NULL,
                             &(label_pair_const_t){.name="class", .value=pwlminfo->i_descr.name},
                             NULL);
    }

    plugin_dispatch_metric_family_array(fams, FAM_WLM_MAX, 0);

    return 0;
}

static int wlm_config (config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp ("class", child->key) == 0) {
            status = cf_util_exclist(child, &excl_class);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    return 0;
}

static int wlm_init(void)
{
    if (wlm_initialize(WLM_VERSION) != 0) {
        PLUGIN_WARNING ("wlm_initialize: %s", STRERRNO);
        return -1;
    }

    return 0;
}

static int wlm_shutdown(void)
{
    exclist_reset(&excl_class);

    return 0;
}

void module_register (void)
{
    plugin_register_config("wlm", wlm_config);
    plugin_register_init("wlm", wlm_init);
    plugin_register_shutdown("wlm", wlm_shutdown);
    plugin_register_read("wlm", wlm_read);
}
