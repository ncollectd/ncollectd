// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "log.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"
#include "libxson/render.h"
#include "libformat/format.h"

int log_json(strbuf_t *buf, size_t flags, const log_msg_t *msg)
{
    xson_render_t r = {0};
    xson_render_init(&r, buf, XSON_RENDER_TYPE_JSON, 0);

    int status = xson_render_map_open(&r);

    if (flags & LOG_PRINT_TIMESTAMP) {
        status |= xson_render_key_string(&r, "timestamp");
        status |= xson_render_integer(&r, CDTIME_T_TO_DOUBLE(msg->time));
    }

    if ((msg->plugin != NULL) && (flags & LOG_PRINT_PLUGIN)) {
        status |= xson_render_key_string(&r, "plugin");
        status |= xson_render_string(&r, msg->plugin);
    }

    if ((msg->file != NULL) && (flags & LOG_PRINT_FILE)) {
        status |= xson_render_key_string(&r, "file");
        status |= xson_render_string(&r, msg->file);
    }

    if ((msg->line > 0) && (flags & LOG_PRINT_LINE)) {
        status |= xson_render_key_string(&r, "line");
        status |= xson_render_integer(&r, msg->line);
    }

    if ((msg->func != NULL) && (flags & LOG_PRINT_FUNCTION)) {
        status |= xson_render_key_string(&r, "function");
        status |= xson_render_string(&r, msg->func);
    }

    if (flags & LOG_PRINT_SEVERITY) {
        status |= xson_render_key_string(&r, "level");
        switch (msg->severity) {
        case LOG_ERR:
            status |= xson_render_string(&r, "error");
            break;
        case LOG_WARNING:
            status |= xson_render_string(&r, "warning");
            break;
        case LOG_NOTICE:
            status |= xson_render_string(&r, "notice");
            break;
        case LOG_INFO:
            status |= xson_render_string(&r, "info");
            break;
        case LOG_DEBUG:
            status |= xson_render_string(&r, "debug");
            break;
        default:
            status |= xson_render_string(&r, "unknown");
            break;
        }
    }

    if ((msg->msg != NULL) && (flags & LOG_PRINT_MESSAGE)) {
        status |= xson_render_key_string(&r, "message");
        status |= xson_render_string(&r, msg->msg);
    }

    status |= xson_render_map_close(&r);

    return status;
}
