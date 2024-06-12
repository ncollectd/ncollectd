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

typedef struct {
    char *instance;
    cexec_t exec;
    cdtime_t interval;
    char *metric_prefix;
    size_t metric_prefix_size;
    label_set_t labels;
    plugin_filter_t *filter;
    int status;
    pid_t pid;
    bool running;
    pthread_mutex_t lock;
} program_t;

static void *exec_read_one(void *arg)
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

    metric_family_t fam = {0};

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

            char *pnl;
            while ((pnl = strchr(pbuffer, '\n'))) {
                *pnl = '\0';
                if (*(pnl - 1) == '\r')
                    *(pnl - 1) = '\0';

                status = metric_parse_line(&fam, plugin_dispatch_metric_family_filtered, pm->filter,
                                                 pm->metric_prefix, pm->metric_prefix_size,
                                                 &pm->labels, 0, 0, pbuffer);
                if (status < 0)
                    PLUGIN_WARNING("Cannot parse '%s'.", pbuffer);

                pbuffer = ++pnl;
            }

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

    PLUGIN_DEBUG("exec_read_one: Waiting for '%s' to exit.", pm->exec.exec);
    if (waitpid(pm->pid, &status, 0) < 0) {
        PLUGIN_DEBUG("waitpid failed: %s", STRERRNO);
    }
    PLUGIN_DEBUG("Child %i exited with status %i.", (int)pm->pid, status);

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

static int exec_read(user_data_t *user_data)
{
    program_t *pm = user_data->data;

    pthread_mutex_lock(&pm->lock);

    if (pm->running) {
        pthread_mutex_unlock(&pm->lock);
        return 0;
    }

    pm->running = true;
    pthread_mutex_unlock(&pm->lock);

    pthread_t t;
    int status = plugin_thread_create(&t, exec_read_one, (void *)pm, "exec read");
    if (status == 0) {
        pthread_detach(t);
    } else {
        PLUGIN_ERROR("plugin_thread_create failed.");
    }

    return 0;
}

static void exec_free(void *arg)
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

    label_set_reset(&pm->labels);
    plugin_filter_free(pm->filter);

    free(pm->metric_prefix);

    free(pm);
}

static int exec_config_exec(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The 'exec' block needs exactly one string argument.");
        return -1;
    }

    program_t *pm = calloc(1, sizeof(*pm));
    if (pm == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }
    pm->interval = plugin_get_interval();

    int status = cf_util_get_string(ci, &pm->instance);
    if (status != 0) {
        PLUGIN_ERROR("Invalid exec name.");
        exec_free(pm);
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
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &pm->labels);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &pm->metric_prefix);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &pm->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        exec_free(pm);
        return -1;
    }

    if (pm->metric_prefix != NULL)
        pm->metric_prefix_size = strlen(pm->metric_prefix);

    pthread_mutex_init(&pm->lock, NULL);

    char interval[DTOA_MAX];
    dtoa(CDTIME_T_TO_DOUBLE(pm->interval), interval, DTOA_MAX);
    cexec_append_env(&pm->exec, "NCOLLECTD_INTERVAL", interval);

    return plugin_register_complex_read("exec", pm->instance, exec_read, pm->interval,
                                        &(user_data_t){.data = pm, .free_func = exec_free});
}

static int exec_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = exec_config_exec(child);
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

static int exec_init(void)
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
    plugin_register_config("exec", exec_config);
    plugin_register_init("exec", exec_init);
}
