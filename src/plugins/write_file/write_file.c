// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2007 Sebastian Harl
// SPDX-FileCopyrightText: Copyright (C) 2007,2008 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"
#include "libformat/format.h"

typedef struct {
    char *instance;
    char *file;
    FILE *fh;
    bool do_close;
    strbuf_t buf;
    format_stream_metric_t format_metric;
    format_notification_t format_notification;
} write_file_t;

static void write_file_free(void *data)
{
    if (data == NULL)
        return;

    write_file_t *wf = data;

    free(wf->instance);
    free(wf->file);
    strbuf_destroy(&wf->buf);

    if (wf->do_close && (wf->fh != NULL))
        fclose(wf->fh);

    free(wf);
}

static int write_file(write_file_t *wf, void *data, size_t len)
{
    if (wf == NULL)
        return -EINVAL;

    if (wf->fh == NULL) {
        wf->do_close = false;

        if (wf->file == NULL) {
            wf->fh = stderr;
        } else if (strcasecmp(wf->file, "stderr") == 0) {
            wf->fh = stderr;
        } else if (strcasecmp(wf->file, "stdout") == 0) {
            wf->fh = stdout;
        } else {
            wf->fh = fopen(wf->file, "a");
            if (wf->fh == NULL) {
                PLUGIN_ERROR("Open '%s' failed: %s\n", wf->file, STRERRNO);
                return -1;
            }
            wf->do_close = true;
        }
    }

    if (len > 0) {
        size_t ret = fwrite(data, 1, len, wf->fh);
        if (ret == 0) {
            PLUGIN_WARNING("fwrite failed: %s", STRERRNO);
        } else if (len != ret) {
            PLUGIN_WARNING("Only write %zu bytes of %zu.", ret, len);
        }
    }

    if (!wf->do_close)
        fflush(wf->fh);

    return 0;
}

static int write_file_notif(notification_t const *n, user_data_t *user_data)
{
    if (user_data == NULL)
        return -EINVAL;

    write_file_t *wf = user_data->data;
    if (wf == NULL)
        return -EINVAL;

    strbuf_reset(&wf->buf);

    int status = format_notification(wf->format_notification, &wf->buf, n);
    if (status != 0) {
        PLUGIN_ERROR("Format metric failed: %d", status);
        return -1;
    }

    write_file(wf, wf->buf.ptr, strbuf_len(&wf->buf));

    return 0;
}

static int write_file_metric(metric_family_t const *fam, user_data_t *user_data)
{
    if (user_data == NULL)
        return -EINVAL;

    write_file_t *wf = user_data->data;
    if (wf == NULL)
        return -EINVAL;

    if (fam->metric.num == 0)
        return 0;

    strbuf_reset(&wf->buf);

    format_stream_metric_ctx_t ctx = {0};

    int status = format_stream_metric_begin(&ctx, wf->format_metric, &wf->buf);
    status |= format_stream_metric_family(&ctx, fam);
    status |= format_stream_metric_end(&ctx);
    if (status != 0) {
        PLUGIN_ERROR("Format metric failed: %d", status);
        return -1;
    }

    write_file(wf, wf->buf.ptr, strbuf_len(&wf->buf));

    return 0;
}

static int write_file_config_instance(config_item_t *ci)
{
    write_file_t *wf = calloc(1, sizeof(*wf));
    if (wf == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &wf->instance);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(wf);
        return -1;
    }

    wf->format_metric = FORMAT_STREAM_METRIC_OPENMETRICS_TEXT;
    wf->format_notification = FORMAT_NOTIFICATION_JSON;

    cf_send_t send = SEND_METRICS;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("format-metric", child->key) == 0) {
            status = config_format_stream_metric(child, &wf->format_metric);
        } else if (strcasecmp("format-notification", child->key) == 0) {
            status = config_format_notification(child, &wf->format_notification);
        } else if (strcasecmp("write", child->key) == 0) {
            status = cf_uti_get_send(child, &send);
        } else if (strcasecmp("file", child->key) == 0) {
            status = cf_util_get_string(child, &wf->file);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        write_file_free(wf);
        return -1;
    }

    status = strbuf_resize(&wf->buf, 4096);
    if (status != 0) {
        PLUGIN_ERROR("buffer resize failed.");
        write_file_free(wf);
        return -1;
    }

    user_data_t user_data = (user_data_t){ .data = wf, .free_func = write_file_free};

    if (send == SEND_NOTIFICATIONS)
        return plugin_register_notification("write_file", wf->instance, write_file_notif,
                                                          &user_data);

    return plugin_register_write("write_file", wf->instance, write_file_metric, NULL, 0, 0,
                                               &user_data);
}

static int write_file_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = write_file_config_instance(child);
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
    plugin_register_config("write_file", write_file_config);
}
