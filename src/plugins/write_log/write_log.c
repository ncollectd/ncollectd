// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2015 Pierre-Yves Ritschard
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Pierre-Yves Ritschard <pyr at spootnik.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"
#include "libformat/format.h"

static format_stream_metric_t wl_format_metric = FORMAT_STREAM_METRIC_OPENMETRICS_TEXT;

static int wl_write(metric_family_t const *fam, __attribute__((unused)) user_data_t *user_data)
{
    if (fam->metric.num == 0)
        return 0;

    strbuf_t buf = STRBUF_CREATE;
    format_stream_metric_ctx_t ctx = {0};

    int status = format_stream_metric_begin(&ctx, wl_format_metric, &buf);
    status |= format_stream_metric_family(&ctx, fam);
    status |= format_stream_metric_end(&ctx);

    if (status != 0) {
        PLUGIN_ERROR("format metric failed: %d", status);
    } else {
        plugin_log(LOG_INFO, NULL, 0, NULL, "%s", buf.ptr);
    }

    strbuf_destroy(&buf);

    return 0;
}

static int wl_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("format-metric", child->key) == 0) {
            status = config_format_stream_metric(child, &wl_format_metric);
        } else {
            PLUGIN_ERROR("Invalid configuration option: `%s'.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("write_log", wl_config);
    plugin_register_write(NULL, "write_log", wl_write, NULL, 0, 0, NULL);
}
