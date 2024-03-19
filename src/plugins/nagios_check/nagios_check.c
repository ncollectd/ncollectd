// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2007-2010 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2007-2009 Sebastian Harl
// SPDX-FileCopyrightText: Copyright (C) 2008 Peter Holik
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Peter Holik <peter at holik.at>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#define _DEFAULT_SOURCE
#define _BSD_SOURCE /* For setgroups */

/* _GNU_SOURCE is needed in Linux to use execvpe */
#define _GNU_SOURCE

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/dtoa.h"
#include "libutils/exec.h"

#include <poll.h>

#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#endif

typedef enum {
    CHECK_STATUS_UNKNOW,
    CHECK_STATUS_OKAY,
    CHECK_STATUS_WARNING,
    CHECK_STATUS_FAILURE,
} check_status_t;

typedef struct {
    char *instance;
    cexec_t exec;
    cdtime_t interval;

    char *notification;
    label_set_t labels;
    label_set_t annotations;

    check_status_t check_status;
    cdtime_t last_notif;
    cdtime_t refresh_interval;
    bool persist;
    bool persist_ok;

    int status;
    pid_t pid;
    bool running;
    pthread_mutex_t lock;
} program_t;

static const size_t MAX_CHECK_SIZE = 4096;

static void nagios_check_dispatch_notification(program_t *pm, char *output)
{
    check_status_t check_status = CHECK_STATUS_UNKNOW;
    severity_t severity = NOTIF_OKAY;

    switch (WEXITSTATUS(pm->status)) {
    case 0:
        severity = NOTIF_OKAY;
        check_status = CHECK_STATUS_OKAY;
        break;
    case 1:
        severity = NOTIF_WARNING;
        check_status = CHECK_STATUS_WARNING;
        break;
    case 2:
        severity = NOTIF_FAILURE;
        check_status = CHECK_STATUS_FAILURE;
        break;
    case 3:
        severity = NOTIF_FAILURE;
        check_status = CHECK_STATUS_FAILURE;
        break;
    default:
        severity = NOTIF_FAILURE;
        check_status = CHECK_STATUS_FAILURE;
        break;
    }

    cdtime_t now = cdtime();

    if (check_status == pm->check_status) {
        if (pm->refresh_interval > 0) {
            if ((now - pm->last_notif) < pm->refresh_interval)
                return;
            if (pm->persist == false)
                return;
            if ((pm->persist_ok == false) && (check_status == CHECK_STATUS_OKAY))
                return;
        } else {
            if (pm->persist == false)
                return;
            if ((pm->persist_ok == false) && (check_status == CHECK_STATUS_OKAY))
                return;
        }
    }

    pm->check_status = check_status;
    pm->last_notif = now;

    char *long_output = NULL;
    char *perfdata = NULL;
    if (output != NULL) {
        perfdata = strchr(output, '|');
        if (perfdata != NULL) {
            *perfdata = '\0';
            perfdata++;
            long_output = strchr(perfdata, '|');
            if (long_output != NULL) {
                *long_output = '\0';
                long_output++;
            }
        }
    }

    notification_t n = {
        .severity = severity,
        .time = now,
        .name = pm->notification,
    };

    label_set_add_set(&n.label, true, pm->labels);
    label_set_add_set(&n.annotation, true, pm->labels);

    if ((output != NULL) && (*output != '\0')) {
        strstripnewline(output);
        notification_annotation_set(&n, "summary", output);
    }
    if (perfdata != NULL) {
        strstripnewline(perfdata);
        notification_annotation_set(&n, "perfdata", perfdata);
    }
    if (long_output != NULL) {
        strstripnewline(long_output);
        notification_annotation_set(&n, "long_output", long_output);
    }

    plugin_dispatch_notification(&n);
}

static void *nagios_check_read_one(void *arg)
{
    program_t *pm = (program_t *)arg;

    char buffer[4096];
    char buffer_err[4096];
    char *pbuffer = buffer;
    char *pbuffer_err = buffer_err;

    int fd, fd_err;
    int status = exec_fork_child(&pm->exec, false, NULL, &fd, &fd_err);
    if (status < 0) {
        /* Reset the "running" flag */
        pthread_mutex_lock(&pm->lock);
        pm->running = false;
        pthread_mutex_unlock(&pm->lock);
        pthread_exit((void *)1);
    }
    pm->pid = status;

    assert(pm->pid != 0);

    struct pollfd fds[2] = {{0}};
    fds[0].fd = fd;
    fds[0].events = POLLIN;
    fds[1].fd = fd_err;
    fds[1].events = POLLIN;

    strbuf_t buf = STRBUF_CREATE;

    while (true) {
        status = poll(fds, STATIC_ARRAY_SIZE(fds), -1);
        if (status < 0) {
            if (errno == EINTR)
                continue;
            break;
        }

        if (fds[0].revents & (POLLIN | POLLHUP)) {
            int len = read(fd, pbuffer, sizeof(buffer) - 1 - (pbuffer - buffer));
            if (len < 0) {
                if (errno == EAGAIN || errno == EINTR)
                    continue;
                break;
            } else if (len == 0) {
                break; /* We've reached EOF */
            }

            pbuffer[len] = '\0';

            len += pbuffer - buffer;
            pbuffer = buffer;

            if (strbuf_len(&buf) < MAX_CHECK_SIZE) {
                size_t avail = MAX_CHECK_SIZE - strbuf_len(&buf);
                size_t pbuffer_len = len;
                if (pbuffer_len > avail)
                    pbuffer_len = avail;
                status = strbuf_putstrn(&buf, pbuffer, pbuffer_len);
                status |= strbuf_putchar(&buf, '\n');
                if (status < 0)
                    PLUGIN_WARNING("Failed to fill check response buffer.");
            }
            pbuffer += len;

            /* not completely read ? */
            if (pbuffer - buffer < len) {
                len -= pbuffer - buffer;
                memmove(buffer, pbuffer, len);
                pbuffer = buffer + len;
            } else {
                pbuffer = buffer;
            }
        } else if (fds[0].revents & (POLLERR | POLLNVAL)) {
            PLUGIN_ERROR("Failed to read pipe from '%s'.", pm->exec.exec);
            break;
        } else if (fds[1].revents & (POLLIN | POLLHUP)) {
            int len = read(fd_err, pbuffer_err,
                                   sizeof(buffer_err) - 1 - (pbuffer_err - buffer_err));

            if (len < 0) {
                if (errno == EAGAIN || errno == EINTR)
                    continue;
                break;
            } else if (len == 0) {
                /* We've reached EOF */
                PLUGIN_DEBUG("Program '%s' has closed STDERR.", pm->exec.exec);

                /* Clean up file descriptor */
                if (fd_err > 0)
                    close(fd_err);
                fd_err = -1;
                fds[1].fd = -1;
                fds[1].events = 0;
                continue;
            }

            pbuffer_err[len] = '\0';

            len += pbuffer_err - buffer_err;
            pbuffer_err = buffer_err;

            char *pnl;
            while ((pnl = strchr(pbuffer_err, '\n'))) {
                *pnl = '\0';
                if (*(pnl - 1) == '\r')
                    *(pnl - 1) = '\0';

                PLUGIN_ERROR("exec_read_one: error = %s", pbuffer_err);

                pbuffer_err = ++pnl;
            }
            /* not completely read ? */
            if (pbuffer_err - buffer_err < len) {
                len -= pbuffer_err - buffer_err;
                memmove(buffer_err, pbuffer_err, len);
                pbuffer_err = buffer_err + len;
            } else {
                pbuffer_err = buffer_err;
            }
        } else if (fds[1].revents & (POLLERR | POLLNVAL)) {
            PLUGIN_WARNING("Ignoring STDERR for program '%s'.", pm->exec.exec);
            /* Clean up file descriptor */
            if ((fds[1].revents & POLLNVAL) == 0) {
                if (fd_err > 0)
                    close(fd_err);
                fd_err = -1;
            }
            fds[1].fd = -1;
            fds[1].events = 0;
        }
    }

    PLUGIN_DEBUG("Waiting for '%s' to exit.", pm->exec.exec);
    if (waitpid(pm->pid, &pm->status, 0) < 0) {
        PLUGIN_ERROR("waitpid failed: %s", STRERRNO);
    } else {
        PLUGIN_DEBUG("Child %i exited with status %i.", (int)pm->pid, pm->status);
        nagios_check_dispatch_notification(pm, buf.ptr);
    }

    strbuf_destroy(&buf);

    pm->pid = 0;

    pthread_mutex_lock(&pm->lock);
    pm->running = false;
    pthread_mutex_unlock(&pm->lock);

    close(fd);
    if (fd_err >= 0)
        close(fd_err);

    pthread_exit((void *)0);
    return NULL;
}

static int nagios_check_read(user_data_t *user_data)
{
    program_t *pm = user_data->data;

    pthread_mutex_lock(&pm->lock);

    if (pm->running) {
        pthread_mutex_unlock(&pm->lock);
        return 0;
    }

    pm->running = true;
    pthread_mutex_unlock(&pm->lock);

    pthread_t t = (pthread_t){0};
    int status = plugin_thread_create(&t, nagios_check_read_one, (void *)pm, "exec read");
    if (status == 0) {
        pthread_detach(t);
    } else {
        PLUGIN_ERROR("plugin_thread_create failed.");
    }

    return 0;
}

static void nagios_check_free(void *arg)
{
    program_t *pm = arg;

    if (pm == NULL)
        return;

    if (pm->pid > 0) {
        kill(pm->pid, SIGTERM);
        PLUGIN_INFO("Sent SIGTERM to %hu", (unsigned short int)pm->pid);

        waitpid(pm->pid, NULL, 0);
    }

    free(pm->instance);

    exec_reset(&pm->exec);

    free(pm->notification);
    label_set_reset(&pm->labels);
    label_set_reset(&pm->annotations);


    free(pm);
}

static int nagios_check_config_instance(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The 'instance' block needs exactly one string argument.");
        return -1;
    }

    program_t *pm = calloc(1, sizeof(*pm));
    if (pm == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }
    pm->interval = plugin_get_interval();
    pm->persist = false;
    pm->persist_ok = false;

    int status = cf_util_get_string(ci, &pm->instance);
    if (status != 0) {
        PLUGIN_ERROR("Invalid check name.");
        nagios_check_free(pm);
        return -1;
    }

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("cmd", child->key) == 0) {
            status = cf_util_exec_cmd(child, &pm->exec);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &pm->exec.user);
        } else if (strcasecmp("group", child->key) == 0) {
            status = cf_util_get_string(child, &pm->exec.group);
        } else if (strcasecmp("env", child->key) == 0) {
            status = cf_util_exec_append_env(child, &pm->exec);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &pm->interval);
        } else if (strcasecmp("refresh", child->key) == 0) {
            status = cf_util_get_cdtime(child, &pm->refresh_interval);
        } else if (strcasecmp("persist", child->key) == 0) {
            status = cf_util_get_boolean(child, &pm->persist);
        } else if (strcasecmp("persist-ok", child->key) == 0) {
            status = cf_util_get_boolean(child, &pm->persist_ok);
        } else if (strcasecmp("notification", child->key) == 0) {
            status = cf_util_get_string(child, &pm->notification);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &pm->labels);
        } else if (strcasecmp("annotation", child->key) == 0) {
            status = cf_util_get_label(child, &pm->annotations);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        nagios_check_free(pm);
        return -1;
    }

    pthread_mutex_init(&pm->lock, NULL);

    return plugin_register_complex_read("nagios_check", pm->instance, nagios_check_read, pm->interval,
                                        &(user_data_t){.data = pm, .free_func = nagios_check_free});
}

static int nagios_check_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = nagios_check_config_instance(child);
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

static int nagios_check_init(void)
{
#if defined(HAVE_SYS_CAPABILITY_H) && defined(CAP_SETUID) && defined(CAP_SETGID)
    if ((plugin_check_capability(CAP_SETUID) != 0) || (plugin_check_capability(CAP_SETGID) != 0)) {
        if (getuid() == 0)
            PLUGIN_WARNING("Running ncollectd as root, but the CAP_SETUID "
                           "or CAP_SETGID capabilities are missing. The plugin's read function "
                           "will probably fail. Is your init system dropping capabilities?");
        else
            PLUGIN_WARNING("ncollectd doesn't have the CAP_SETUID or "
                           "CAP_SETGID capabilities. If you don't want to run ncollectd as root, "
                           "try running \"setcap 'cap_setuid=ep cap_setgid=ep'\" on the "
                           "ncollectd binary.");
    }
#endif
    return 0;
}

void module_register(void)
{
    plugin_register_config("nagios_check", nagios_check_config);
    plugin_register_init("nagios_check", nagios_check_init);
}
