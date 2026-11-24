// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2007 Sebastian Harl
// SPDX-FileCopyrightText: Copyright (C) 2007,2008 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libformat/format.h"

#include <fcntl.h>

#ifdef NCOLLECTD_DEBUG
static int log_level = LOG_DEBUG;
#else
static int log_level = LOG_INFO;
#endif

static pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;

static char *log_file;
static size_t log_flags = LOG_PRINT_ALL;
static format_log_t log_fmt = FORMAT_LOG_TEXT;

static void logfile_log(const log_msg_t *msg, user_data_t __attribute__((unused)) * user_data)
{
    if (msg == NULL)
        return;

    if (msg->severity > log_level)
        return;

    strbuf_t buf = STRBUF_CREATE;

    int status = format_log(&buf, log_fmt, log_flags, msg);
    if ((status != 0) || (buf.ptr == NULL)) {
        fprintf(stderr, "logfile plugin: format message failed.\n");
        strbuf_destroy(&buf);
        return;
    }

    pthread_mutex_lock(&file_lock);

    FILE *fh = NULL;
    bool do_close = false;
    if (log_file == NULL) {
        fh = stderr;
    } else if (strcasecmp(log_file, "stderr") == 0) {
        fh = stderr;
    } else if (strcasecmp(log_file, "stdout") == 0) {
        fh = stdout;
    } else {
        int flags = O_WRONLY | O_APPEND | O_CREAT;
#ifdef O_CLOEXEC
        flags |= O_CLOEXEC;
#endif
        int fd = open(log_file, flags, 0640);
        if (fd < 0) {
            fprintf(stderr, "logfile plugin: open (%s) failed: %s\n", log_file, STRERRNO);
        } else {
#ifndef O_CLOEXEC
            fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
            fh = fdopen(fd, "a");
            if (fh == NULL) {
                fprintf(stderr, "logfile plugin: fdopen (%s) failed: %s\n", log_file, STRERRNO);
                close(fd);
            }
        }
        do_close = true;
    }

    if (fh != NULL) {
        fprintf(fh, "%s\n", buf.ptr);

        if (do_close) {
            fclose(fh);
        } else {
            fflush(fh);
        }
    }

    pthread_mutex_unlock(&file_lock);

    strbuf_destroy(&buf);
}

static int logfile_notification(const notification_t *n,
                                user_data_t __attribute__((unused)) * user_data)
{
    if (n == NULL)
        return 0;

    int severity;
    switch (n->severity) {
        case NOTIF_FAILURE:
            severity = LOG_ERR;
            break;
        case NOTIF_WARNING:
            severity = LOG_WARNING;
            break;
        case NOTIF_OKAY:
            severity = LOG_NOTICE;
            break;
        default:
            severity = LOG_ERR;
            break;
    }

    strbuf_t buf = STRBUF_CREATE;

    int status = strbuf_print(&buf, "Notification: ");
    status |= notification_marshal(&buf, n);
    if ((status != 0) || (buf.ptr == NULL)) {
        fprintf(stderr, "logfile plugin: marshal notification failed.\n");
        strbuf_destroy(&buf);
        return -1;
    }

    log_msg_t log = {
        .severity = severity,
        .time = (n->time != 0) ? n->time : cdtime(),
        .msg = buf.ptr,
    };

    logfile_log(&log, NULL);

    strbuf_destroy(&buf);
    return 0;
}

static int logfile_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "log-level") == 0) {
            status = cf_util_get_log_level(child, &log_level);
        } else if (strcasecmp(child->key, "file") == 0) {
            status = cf_util_get_string(child, &log_file);
        } else if (strcasecmp(child->key, "format") == 0) {
            status = config_format_log(child, &log_fmt, &log_flags);
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

static int logfile_shutdown(void)
{
    free(log_file);

    return 0;
}

void module_register(void)
{
    plugin_register_config("log_file", logfile_config);
    plugin_register_log("log_file", NULL, logfile_log, NULL);
    plugin_register_notification("log_file", NULL, logfile_notification, NULL);
    plugin_register_shutdown("log_file", logfile_shutdown);
}
