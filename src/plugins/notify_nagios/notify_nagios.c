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

static char replace_svc[256] = {
    [';'] = 1,
    ['\n'] = 1,
};

static char escape_message[256] = {
    ['\\'] = '\\',
    ['\n'] = 'n',
};

static char *nagios_command_file;

static int nagios_print(char const *buffer)
{
    char const *file = NAGIOS_COMMAND_FILE;
    if (nagios_command_file != NULL)
        file = nagios_command_file;

    int fd = open(file, O_WRONLY | O_APPEND);
    if (fd < 0) {
        PLUGIN_ERROR("Opening \"%s\" failed: %s", file, STRERRNO);
        return -1;
    }

    struct flock lock = {
        .l_type = F_WRLCK,
        .l_whence = SEEK_END
    };

    int status = fcntl(fd, F_GETLK, &lock);
    if (status != 0) {
        PLUGIN_ERROR("Failed to acquire write lock on \"%s\": %s", file, STRERRNO);
        close(fd);
        return -1;
    }

    status = (int)lseek(fd, 0, SEEK_END);
    if (status == -1) {
        PLUGIN_ERROR("Seeking to end of \"%s\" failed: %s", file, STRERRNO);
        close(fd);
        return -1;
    }

    status = (int)swrite(fd, buffer, strlen(buffer));
    if (status != 0) {
        PLUGIN_ERROR("Writing to \"%s\" failed: %s", file, STRERRNO);
        close(fd);
        return -1;
    }

    close(fd);

    return 0;
}

// [<timestamp>] PROCESS_SERVICE_CHECK_RESULT;<host_name>;<svc_description>;<return_code>;<plugin_output>
static int nagios_notify(const notification_t *n, __attribute__((unused)) user_data_t *user_data)
{
    int status = 0;

    strbuf_t buf = STRBUF_CREATE;

    status |= strbuf_putchar(&buf, '[');
    status |= strbuf_putuint(&buf, CDTIME_T_TO_TIME_T(n->time));
    status |= strbuf_putstr(&buf, "] PROCESS_SERVICE_CHECK_RESULT;");
    label_pair_t *hostname_pair = label_set_read(n->label, "hostname");
    if (hostname_pair != NULL)
        status |= strbuf_putstr(&buf, hostname_pair->value);
    status |= strbuf_putchar(&buf, ';');
    status |= strbuf_putstr(&buf, n->name);
    if (((hostname_pair == NULL) && (n->label.num > 0)) ||
        ((hostname_pair != NULL) && (n->label.num > 1)) ) {
        status |= strbuf_putchar(&buf, '{');
        for (size_t i = 0; i < n->label.num; i++) {
            if (strcmp(n->label.ptr[i].name, "hostname") != 0) {
                if (i != 0)
                    status |= strbuf_putchar(&buf, ',');
                status |= strbuf_putstr(&buf, n->label.ptr[i].name);
                status |= strbuf_putstr(&buf, "=\"");
                status |= strbuf_putreplace_set(&buf, n->label.ptr[i].value, replace_svc, ' ');
                status |= strbuf_putchar(&buf, '"');
            }
        }
        status |= strbuf_putchar(&buf, '}');
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
    label_pair_t *pair = label_set_read(n->annotation, "message");
    if (pair != NULL)
        status |= strbuf_putescape_set(&buf, pair->value, escape_message, '\\');
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
    plugin_register_config("notify_nagios", nagios_config);
    plugin_register_notification(NULL, "notify_nagios", nagios_notify, NULL);
}
