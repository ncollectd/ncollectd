// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#define SD_JOURNAL_SUPPRESS_LOCATION
#include <systemd/sd-journal.h>

#ifdef NCOLLECTD_DEBUG
    static int log_level = LOG_DEBUG;
#else
    static int log_level = LOG_INFO;
#endif

static severity_t notif_severity;

static void sd_log(const log_msg_t *msg, user_data_t __attribute__((unused)) * user_data)
{
    if (msg->severity > log_level)
        return;

    struct iovec iov[6];
    size_t n=0;

    char func[256] = "";
    if (msg->func != NULL) {
        iov[n].iov_len = ssnprintf(func, sizeof(func), "CODE_FUNC=%s", msg->func);
        if (iov[n].iov_len > 0)
            iov[n++].iov_base = func;
    }

    char file[256] = "";
    if (msg->file != NULL) {
        iov[n].iov_len = ssnprintf(file, sizeof(file), "CODE_FILE=%s", msg->file);
        if (iov[n].iov_len > 0)
            iov[n++].iov_base = file;
    }

    char line[256] = "";
    if (msg->line > 0) {
        iov[n].iov_len = ssnprintf(line, sizeof(line), "CODE_LINE=%d", msg->line);
        if (iov[n].iov_len > 0)
            iov[n++].iov_base = line;
    }

    char priority[64] = "";
    iov[n].iov_len = ssnprintf(priority, sizeof(priority), "PRIORITY=%d", msg->severity);
    if (iov[n].iov_len > 0)
        iov[n++].iov_base = priority;

    char plugin[256] = "";
    if (msg->plugin != NULL) {
        iov[n].iov_len = ssnprintf(plugin, sizeof(plugin), "PLUGIN=%s", msg->plugin);
        if (iov[n].iov_len > 0)
            iov[n++].iov_base = plugin;
    }

    char message[1024] = "";
    if (msg->msg != NULL) {
        iov[n].iov_len = ssnprintf(message, sizeof(message), "MESSAGE=%s", msg->msg);
        if (iov[n].iov_len > 0)
            iov[n++].iov_base = message;
    }

    if (n > 0)
        sd_journal_sendv(iov, n);
}

static int sd_notification(const notification_t *n, user_data_t __attribute__((unused)) *user_data)
{
    if (n->severity > notif_severity)
        return 0;

    int log_severity;
    switch (n->severity) {
        case NOTIF_FAILURE:
            log_severity = LOG_ERR;
            break;
        case NOTIF_WARNING:
            log_severity = LOG_WARNING;
            break;
        case NOTIF_OKAY:
            log_severity = LOG_NOTICE;
            break;
        default:
            log_severity = LOG_ERR;
    }

    strbuf_t buf = STRBUF_CREATE;
    strbuf_print(&buf, "Notification: ");
    notification_marshal(&buf, n);

    log_msg_t msg = { .severity = log_severity, .msg = buf.ptr };

    sd_log(&msg, NULL);

    strbuf_destroy(&buf);
    return 0;
}

static int sd_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "log-level") == 0) {
            status = cf_util_get_log_level(child, &log_level);
        } else if (strcasecmp(child->key, "notify-level") == 0) {
            status = cf_util_get_severity(child, &notif_severity);
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

void module_register(void)
{
    plugin_register_config("log_systemd", sd_config);
    plugin_register_log("log_systemd", NULL, sd_log, NULL);
    plugin_register_notification("log_systemd", NULL, sd_notification, NULL);
}
