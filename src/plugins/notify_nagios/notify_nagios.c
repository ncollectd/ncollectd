// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2015  Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

#include "plugin.h"
#include "libutils/common.h"

#define NAGIOS_OK       0
#define NAGIOS_WARNING  1
#define NAGIOS_CRITICAL 2
#define NAGIOS_UNKNOWN  3

#ifndef NAGIOS_COMMAND_FILE
#define NAGIOS_COMMAND_FILE "/usr/local/nagios/var/rw/nagios.cmd"
#endif

static char *nagios_command_file;

static int nagios_print(char const *buffer)
{
    char const *file = NAGIOS_COMMAND_FILE;
    int fd;
    int status;
    struct flock lock = {0};

    if (nagios_command_file != NULL)
        file = nagios_command_file;

    fd = open(file, O_WRONLY | O_APPEND);
    if (fd < 0) {
        status = errno;
        PLUGIN_ERROR("Opening \"%s\" failed: %s", file, STRERRNO);
        return status;
    }

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_END;

    status = fcntl(fd, F_GETLK, &lock);
    if (status != 0) {
        status = errno;
        PLUGIN_ERROR("Failed to acquire write lock on \"%s\": %s", file, STRERRNO);
        close(fd);
        return status;
    }

    status = (int)lseek(fd, 0, SEEK_END);
    if (status == -1) {
        status = errno;
        PLUGIN_ERROR("Seeking to end of \"%s\" failed: %s", file, STRERRNO);
        close(fd);
        return status;
    }

    status = (int)swrite(fd, buffer, strlen(buffer));
    if (status != 0) {
        status = errno;
        PLUGIN_ERROR("Writing to \"%s\" failed: %s", file, STRERRNO);
        close(fd);
        return status;
    }

    close(fd);
    return status;
}

// [<timestamp>] PROCESS_SERVICE_CHECK_RESULT;<host_name>;<svc_description>;<return_code>;<plugin_output>
static int nagios_notify(const notification_t *n, __attribute__((unused)) user_data_t *user_data)
{
    int status = 0;

    strbuf_t buf = STRBUF_CREATE;

    status |= strbuf_putchar(&buf, '[');
    status |= strbuf_putuint(&buf, CDTIME_T_TO_TIME_T(n->time));
    status |= strbuf_putstr(&buf, "] PROCESS_SERVICE_CHECK_RESULT;");
    label_pair_t *pair = label_set_read(n->label, "hostname");
    if (pair != NULL)
        status |= strbuf_putstr(&buf, pair->value);
    status |= strbuf_putchar(&buf, ';');
    for (size_t i = 0; i < n->label.num; i++) {
        if (strcmp(n->label.ptr[i].name, "hostname") != 0) {
            if (i != 0)
                status |= strbuf_putchar(&buf, ',');
            status |= strbuf_putstr(&buf, n->label.ptr[i].name);
            status |= strbuf_putstr(&buf, "=\"");
            // TODO escape ';'
            status |= strbuf_putescape_label(&buf, n->label.ptr[i].value);
            status |= strbuf_putchar(&buf, '"');
        }
    }
    status |= strbuf_putchar(&buf, ';');
    int code;
    switch (n->severity) {
    case NOTIF_OKAY:
        code = NAGIOS_OK;
        break;
    case NOTIF_WARNING:
        code = NAGIOS_WARNING;
        break;
    case NOTIF_FAILURE:
        code = NAGIOS_CRITICAL;
        break;
    default:
        code = NAGIOS_UNKNOWN;
        break;
    }
    status |= strbuf_putint(&buf, code);
    status |= strbuf_putchar(&buf, ';');
    pair = label_set_read(n->annotation, "message");
    if (pair != NULL)
        status |= strbuf_putstr(&buf, pair->value);
    status |= strbuf_putchar(&buf, '\n');

    if (status != 0) {
        strbuf_destroy(&buf);
        return status;
    }

    status = nagios_print(buf.ptr);

    strbuf_destroy(&buf);

    return status;
}

static int nagios_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("command-file", child->key) == 0) {
            status = cf_util_get_string(child, &nagios_command_file);
        } else {
            PLUGIN_ERROR("Unknown config option \"%s\".", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("notify_nagios", nagios_config);
    plugin_register_notification(NULL, "notify_nagios", nagios_notify, NULL);
}
