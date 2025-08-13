// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "log.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"
#include "libformat/format.h"

#include <time.h>

int log_text(strbuf_t *buf, size_t flags, const log_msg_t *msg)
{
    int status = 0;
    int fields = 0;

    if (flags & LOG_PRINT_TIMESTAMP) {
        char timestamp_str[64] = "";
        struct tm timestamp_tm;
        localtime_r(&CDTIME_T_TO_TIME_T(msg->time), &timestamp_tm);

        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S", &timestamp_tm);
        timestamp_str[sizeof(timestamp_str) - 1] = '\0';

        status |= strbuf_putchar(buf, '[');
        status |= strbuf_putstr(buf, timestamp_str);
        status |= strbuf_putchar(buf, ']');
        fields++;
    }

    if (flags & LOG_PRINT_SEVERITY) {
        if (fields)
            status |= strbuf_putchar(buf, ' ');
        switch (msg->severity) {
        case LOG_ERR:
            status |= strbuf_putstr(buf, "[error]");
            break;
        case LOG_WARNING:
            status |= strbuf_putstr(buf, "[warning]");
            break;
        case LOG_NOTICE:
            status |= strbuf_putstr(buf, "[notice]");
            break;
        case LOG_INFO:
            status |= strbuf_putstr(buf, "[info]");
            break;
        case LOG_DEBUG:
            status |= strbuf_putstr(buf, "[debug]");
            break;
        default:
            status |= strbuf_putstr(buf, "[unknown]");
            break;
        }
        fields++;
    }

    if ((msg->plugin != NULL) && (flags & LOG_PRINT_PLUGIN)) {
        if (fields)
            status |= strbuf_putchar(buf, ' ');
        status |= strbuf_putstrn(buf, "plugin ", strlen("plugin "));
        status |= strbuf_putstr(buf, msg->plugin);
        fields++;
    }

    if ((msg->func != NULL) && (flags & LOG_PRINT_FUNCTION)) {
        if (fields)
            status |= strbuf_putchar(buf, ' ');
        status |= strbuf_putstr(buf, msg->func);
        if ((msg->file != NULL) && (flags & LOG_PRINT_FILE))
            status |= strbuf_putchar(buf, '(');
        fields++;
    }

    if ((msg->file != NULL) && (flags & LOG_PRINT_FILE)) {
        if (fields && !((msg->func != NULL) && (flags & LOG_PRINT_FUNCTION)))
            status |= strbuf_putchar(buf, ' ');
        status |= strbuf_putstr(buf, msg->file);
        if ((msg->line != 0) && (flags & LOG_PRINT_LINE)) {
            status |= strbuf_putchar(buf, ':');
            status |= strbuf_putint(buf, msg->line);
        }
        if ((msg->func != NULL) && (flags & LOG_PRINT_FUNCTION))
            status |= strbuf_putchar(buf, ')');
        fields++;
    }

    if ((msg->msg != NULL) && (flags & LOG_PRINT_MESSAGE)) {
        if (fields)
            status |= strbuf_putstrn(buf, ": ", strlen(": "));
        status |= strbuf_putstr(buf, msg->msg);
    }

    return status;
}
