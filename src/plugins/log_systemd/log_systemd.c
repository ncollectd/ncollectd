// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <stdatomic.h>

#define SD_JOURNAL_SUPPRESS_LOCATION
#include <systemd/sd-journal.h>

#ifdef NCOLLECTD_DEBUG
    static int log_level = LOG_DEBUG;
#else
    static int log_level = LOG_INFO;
#endif

static severity_t notif_severity;

/* Format a journal field in buf and point iov to it. The length is limited
 * to the bytes really written in buf, the field is truncated if needed. */
__attribute__((format(printf, 4, 5)))
static bool sd_iov_set(struct iovec *iov, char *buf, size_t size, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int len = vsnprintf(buf, size, format, ap);
    va_end(ap);

    if (len <= 0)
        return false;

    iov->iov_base = buf;
    iov->iov_len = ((size_t)len >= size) ? size - 1 : (size_t)len;
    return true;
}

static atomic_flag sendv_error_reported = ATOMIC_FLAG_INIT;

static void sd_log(const log_msg_t *msg, user_data_t __attribute__((unused)) * user_data)
{
    if (msg == NULL)
        return;

    if (msg->severity > log_level)
        return;

    struct iovec iov[6];
    size_t n=0;

    char func[256] = "";
    if (msg->func != NULL) {
        if (sd_iov_set(&iov[n], func, sizeof(func), "CODE_FUNC=%s", msg->func))
            n++;
    }

    char file[256] = "";
    if (msg->file != NULL) {
        if (sd_iov_set(&iov[n], file, sizeof(file), "CODE_FILE=%s", msg->file))
            n++;
    }

    char line[256] = "";
    if (msg->line > 0) {
        if (sd_iov_set(&iov[n], line, sizeof(line), "CODE_LINE=%d", msg->line))
            n++;
    }

    char priority[64] = "";
    if (sd_iov_set(&iov[n], priority, sizeof(priority), "PRIORITY=%d", msg->severity))
        n++;

    char plugin[256] = "";
    if (msg->plugin != NULL) {
        if (sd_iov_set(&iov[n], plugin, sizeof(plugin), "PLUGIN=%s", msg->plugin))
            n++;
    }

    char message[1024] = "";
    if (msg->msg != NULL) {
        if (sd_iov_set(&iov[n], message, sizeof(message), "MESSAGE=%s", msg->msg))
            n++;
    }

    if (n > 0) {
        int status = sd_journal_sendv(iov, n);
        /* Can not use the plugin log functions here, report it once to stderr. */
        if ((status < 0) && !atomic_flag_test_and_set(&sendv_error_reported))
            fprintf(stderr, "log_systemd plugin: sd_journal_sendv failed: %s\n",
                            STRERROR(-status));
    }
}

static int sd_notification(const notification_t *n, user_data_t __attribute__((unused)) *user_data)
{
    if (n == NULL)
        return EINVAL;

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
    int status = strbuf_print(&buf, "Notification: ");
    status |= notification_marshal(&buf, n);
    if ((status != 0) || (buf.ptr == NULL)) {
        strbuf_destroy(&buf);
        return -1;
    }

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
