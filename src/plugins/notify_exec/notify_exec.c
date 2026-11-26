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
#include "libutils/strbuf.h"
#include "libmetric/metric_match.h"
#include "libformat/format.h"

#include <poll.h>
#include <fcntl.h>

#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#endif

struct program_list;
typedef struct program_list program_list_t;

typedef enum {
    PROGRAM_FORMAT_NOTIFICATION_TEXT,
    PROGRAM_FORMAT_NOTIFICATION_JSON,
    PROGRAM_FORMAT_NOTIFICATION_PROTOB,
    PROGRAM_FORMAT_NOTIFICATION_ENV
} program_format_notification_t;

struct program_list {
    metric_match_t match;
    program_format_notification_t format;
    char **envp;
    cexec_t exec;
    cdtime_t timeout;
    pid_t pid;
    program_list_t *next;
};

typedef struct {
    program_list_t *pl;
    notification_t *n;
} program_list_and_notification_t;

static program_list_t *pl_head;
/* Protects the pid field of the program_list_t entries. */
static pthread_mutex_t pl_lock = PTHREAD_MUTEX_INITIALIZER;

static void notify_exec_set_pid(program_list_t *pl, pid_t pid)
{
    pthread_mutex_lock(&pl_lock);
    pl->pid = pid;
    pthread_mutex_unlock(&pl_lock);
}

/* Number of running notification threads, used to wait for them in shutdown. */
static pthread_mutex_t threads_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t threads_cond = PTHREAD_COND_INITIALIZER;
static int threads_num;

static void notify_exec_thread_done(void)
{
    pthread_mutex_lock(&threads_lock);
    threads_num--;
    pthread_cond_broadcast(&threads_cond);
    pthread_mutex_unlock(&threads_lock);
}

/* Wait up to timeout seconds for all the notification threads to finish.
 * Returns true if all the threads have finished. */
static bool notify_exec_wait_threads(time_t timeout)
{
    struct timespec ts = CDTIME_T_TO_TIMESPEC(cdtime() + TIME_T_TO_CDTIME_T(timeout));

    pthread_mutex_lock(&threads_lock);
    while (threads_num > 0) {
        if (pthread_cond_timedwait(&threads_cond, &threads_lock, &ts) == ETIMEDOUT)
            break;
    }
    bool done = threads_num == 0;
    pthread_mutex_unlock(&threads_lock);

    return done;
}

static void notify_exec_free_envp(char **envp)
{
    if (envp == NULL)
        return;

    size_t env_size = 0;
    while (envp[env_size] != NULL) {
        free(envp[env_size]);
        ++env_size;
    }
    free(envp);
}

static char **notify_exec_notification2env(notification_t *n, char **default_envp)
{
    strbuf_t buf = STRBUF_CREATE;

    size_t default_env_size = 0;
    if (default_envp != NULL) {
        while (default_envp[default_env_size] != NULL) {
            ++default_env_size;
        }
    }

    size_t env_size = default_env_size + 3 + n->label.num + n->annotation.num;

    char **envp = malloc(sizeof(char *)*(env_size+1));
    if (envp == NULL) {
        PLUGIN_ERROR("malloc failed.");
        strbuf_destroy(&buf);
        return NULL;
    }

    size_t i = 0;
    while (i < default_env_size) {
        envp[i] = strdup(default_envp[i]);
        if (envp[i] == NULL) {
            notify_exec_free_envp(envp);
            strbuf_destroy(&buf);
            PLUGIN_ERROR("strdup failed.");
            return NULL;
        }
        i++;
    }

    int status = strbuf_putstr(&buf, "NOTIFICATION_TIMESTAMP=");
    status |= strbuf_putuint(&buf, CDTIME_T_TO_TIME_T(n->time));
    envp[i] = status == 0 ? strdup(buf.ptr) : NULL;
    if (envp[i] == NULL) {
        notify_exec_free_envp(envp);
        strbuf_destroy(&buf);
        PLUGIN_ERROR("Failed to build environment.");
        return NULL;
    }
    i++;
    strbuf_reset(&buf);

    status |= strbuf_putstr(&buf, "NOTIFICATION_SEVERITY=");
    switch(n->severity) {
    case NOTIF_FAILURE:
        status |= strbuf_putstr(&buf, "FAILURE");
        break;
    case NOTIF_WARNING:
        status |= strbuf_putstr(&buf, "WARNING");
        break;
    case NOTIF_OKAY:
        status |= strbuf_putstr(&buf, "OKAY");
        break;
    default:
        status |= strbuf_putstr(&buf, "UNKNOWN");
        break;
    }
    envp[i] = status == 0 ? strdup(buf.ptr) : NULL;
    if (envp[i] == NULL) {
        notify_exec_free_envp(envp);
        strbuf_destroy(&buf);
        PLUGIN_ERROR("Failed to build environment.");
        return NULL;
    }
    i++;
    strbuf_reset(&buf);

    status |= strbuf_putstr(&buf, "NOTIFICATION_NAME=");
    status |= strbuf_putstr(&buf, n->name);
    envp[i] = status == 0 ? strdup(buf.ptr) : NULL;
    if (envp[i] == NULL) {
        notify_exec_free_envp(envp);
        strbuf_destroy(&buf);
        PLUGIN_ERROR("Failed to build environment.");
        return NULL;
    }
    i++;
    strbuf_reset(&buf);

    for (size_t j = 0; j < n->label.num; j++) {
        status |= strbuf_putstr(&buf, "NOTIFICATION_LABEL_");
        status |= strbuf_putstrtoupper(&buf, n->label.ptr[j].name);
        status |= strbuf_putchar(&buf, '=');
        status |= strbuf_putstr(&buf, n->label.ptr[j].value);
        envp[i] = status == 0 ? strdup(buf.ptr) : NULL;
        if (envp[i] == NULL) {
            notify_exec_free_envp(envp);
            strbuf_destroy(&buf);
            PLUGIN_ERROR("Failed to build environment.");
            return NULL;
        }
        i++;
        strbuf_reset(&buf);
    }

    for (size_t j = 0; j < n->annotation.num; j++) {
        status |= strbuf_putstr(&buf, "NOTIFICATION_ANNOTATION_");
        status |= strbuf_putstrtoupper(&buf, n->annotation.ptr[j].name);
        status |= strbuf_putchar(&buf, '=');
        status |= strbuf_putstr(&buf, n->annotation.ptr[j].value);
        envp[i] = status == 0 ? strdup(buf.ptr) : NULL;
        if (envp[i] == NULL) {
            notify_exec_free_envp(envp);
            strbuf_destroy(&buf);
            PLUGIN_ERROR("Failed to build environment.");
            return NULL;
        }
        i++;
        strbuf_reset(&buf);
    }

    envp[i] = NULL;

    strbuf_destroy(&buf);

    if (status != 0) {
        notify_exec_free_envp(envp);
        PLUGIN_ERROR("Failed to build environment.");
        return NULL;
    }

    return envp;
}

/* Write the whole buffer to fd before the deadline (0 means no deadline). */
static int notify_exec_write(int fd, const char *data, size_t len, cdtime_t deadline)
{
    if (deadline == 0)
        return swrite(fd, data, len) == 0 ? 0 : -1;

    int flags = fcntl(fd, F_GETFL);
    if ((flags < 0) || (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0))
        return -1;

    while (len > 0) {
        cdtime_t now = cdtime();
        if (now >= deadline) {
            errno = ETIMEDOUT;
            return -1;
        }

        struct pollfd pfd = {.fd = fd, .events = POLLOUT};
        int status = poll(&pfd, 1, (int)CDTIME_T_TO_MS(deadline - now));
        if (status < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        } else if (status == 0) {
            continue;
        }

        ssize_t written = write(fd, data, len);
        if (written < 0) {
            if ((errno == EAGAIN) || (errno == EINTR))
                continue;
            return -1;
        }

        data += written;
        len -= (size_t)written;
    }

    return 0;
}

/* Wait for the child until the deadline (0 means no deadline), then send
 * SIGTERM and, if it does not exit in one second, SIGKILL. */
static int notify_exec_waitpid(pid_t pid, int *status, cdtime_t deadline)
{
    if (deadline == 0)
        return waitpid(pid, status, 0);

    int sig = SIGTERM;
    while (true) {
        pid_t ret = waitpid(pid, status, WNOHANG);
        if (ret != 0)
            return ret;

        if (cdtime() >= deadline) {
            if (sig == SIGKILL) {
                PLUGIN_WARNING("Child %i timed out, sending SIGKILL.", (int)pid);
                kill(pid, SIGKILL);
                return waitpid(pid, status, 0);
            }
            PLUGIN_WARNING("Child %i timed out, sending SIGTERM.", (int)pid);
            kill(pid, SIGTERM);
            sig = SIGKILL;
            deadline = cdtime() + TIME_T_TO_CDTIME_T(1);
        }

        struct timespec ts = {.tv_sec = 0, .tv_nsec = 100000000};
        nanosleep(&ts, NULL);
    }

    return -1;
}

typedef struct {
    int level;
    size_t len;
    char buffer[1024];
} notify_exec_output_t;

/* Read from fd and log every complete line, returns false on EOF or error. */
static bool notify_exec_output_read(int fd, notify_exec_output_t *out, const char *cmd)
{
    ssize_t len = read(fd, out->buffer + out->len, sizeof(out->buffer) - 1 - out->len);
    if (len < 0) {
        if ((errno == EINTR) || (errno == EAGAIN))
            return true;
        return false;
    }

    if (len == 0) {
        if (out->len > 0) {
            out->buffer[out->len] = '\0';
            plugin_log(out->level, __FILE__, __LINE__, __func__, "%s: %s", cmd, out->buffer);
            out->len = 0;
        }
        return false;
    }

    out->len += (size_t)len;
    out->buffer[out->len] = '\0';

    char *start = out->buffer;
    char *end;
    while ((end = strchr(start, '\n')) != NULL) {
        *end = '\0';
        plugin_log(out->level, __FILE__, __LINE__, __func__, "%s: %s", cmd, start);
        start = end + 1;
    }

    size_t rest = out->len - (size_t)(start - out->buffer);
    if (rest == (sizeof(out->buffer) - 1)) {
        /* Line too long, log what we have. */
        plugin_log(out->level, __FILE__, __LINE__, __func__, "%s: %s", cmd, out->buffer);
        rest = 0;
    } else if (rest > 0) {
        memmove(out->buffer, start, rest);
    }
    out->len = rest;

    return true;
}

/* Log the stdout (as info) and stderr (as warnings) of the child until both
 * are closed, so the child does not get a SIGPIPE when writing to them. */
static void notify_exec_read_output(const char *cmd, int fd_out, int fd_err)
{
    notify_exec_output_t out[2] = {{.level = LOG_INFO}, {.level = LOG_WARNING}};
    struct pollfd pfd[2] = {{.fd = fd_out, .events = POLLIN},
                            {.fd = fd_err, .events = POLLIN}};
    int open_fds = 2;

    while (open_fds > 0) {
        int status = poll(pfd, 2, -1);
        if (status < 0) {
            if (errno == EINTR)
                continue;
            PLUGIN_ERROR("poll failed: %s", STRERRNO);
            break;
        }

        for (size_t i = 0; i < 2; i++) {
            if ((pfd[i].fd < 0) || (pfd[i].revents == 0))
                continue;
            if (!notify_exec_output_read(pfd[i].fd, &out[i], cmd)) {
                pfd[i].fd = -1;
                open_fds--;
            }
        }
    }
}

static void *notify_exec_notification_one(void *arg)
{
    program_list_t *pl = ((program_list_and_notification_t *)arg)->pl;
    notification_t *n = ((program_list_and_notification_t *)arg)->n;

    strbuf_t buf = STRBUF_CREATE;
    char **envp = NULL;
    switch (pl->format) {
    case PROGRAM_FORMAT_NOTIFICATION_TEXT:
        format_notification(FORMAT_NOTIFICATION_TEXT, &buf, n);
        break;
    case PROGRAM_FORMAT_NOTIFICATION_JSON:
        format_notification(FORMAT_NOTIFICATION_JSON, &buf, n);
        break;
    case PROGRAM_FORMAT_NOTIFICATION_PROTOB:
        format_notification(FORMAT_NOTIFICATION_PROTOB, &buf, n);
        break;
    case PROGRAM_FORMAT_NOTIFICATION_ENV:
        envp = notify_exec_notification2env(n, pl->envp);
        if (envp == NULL) {
            /* Do not run the program without the notification. */
            PLUGIN_ERROR("Failed to build the environment for notification '%s'.", n->name);
            notification_free(n);
            free(arg);
            strbuf_destroy(&buf);
            pthread_exit((void *)1);
        }
        break;
    }

    pl->exec.envp = envp == NULL ? pl->envp : envp;

    cdtime_t deadline = pl->timeout > 0 ? cdtime() + pl->timeout : 0;

    int fd = -1;
    int fd_out = -1;
    int fd_err = -1;
    int pid = exec_fork_child(&pl->exec, false, &fd, &fd_out, &fd_err);
    if (pid < 0) {
        notify_exec_set_pid(pl, 0);
        notification_free(n);
        free(arg);
        strbuf_destroy(&buf);
        notify_exec_free_envp(envp);
        notify_exec_thread_done();
        pthread_exit((void *)1);
    }
    notify_exec_set_pid(pl, pid);

    if (pl->format != PROGRAM_FORMAT_NOTIFICATION_ENV) {
        if (strbuf_len(&buf) > 0) {
            int status = notify_exec_write(fd, buf.ptr, strbuf_len(&buf), deadline);
            if (status != 0) {
                PLUGIN_ERROR("write(%i) failed: %s", fd, STRERRNO);
                kill(pid, SIGTERM);
                close(fd);
                close(fd_out);
                close(fd_err);
                notify_exec_waitpid(pid, NULL, cdtime() + TIME_T_TO_CDTIME_T(1));
                notify_exec_set_pid(pl, 0);
                notification_free(n);
                free(arg);
                strbuf_destroy(&buf);
                notify_exec_free_envp(envp);
                notify_exec_thread_done();
                pthread_exit((void *)1);
                return NULL;
            }
        }
    }

    close(fd);

    notify_exec_read_output(pl->exec.exec != NULL ? pl->exec.exec : "", fd_out, fd_err);
    close(fd_out);
    close(fd_err);

    int status = 0;
    if (notify_exec_waitpid(pid, &status, deadline) == pid) {
        if (WIFEXITED(status) && (WEXITSTATUS(status) != 0)) {
            PLUGIN_WARNING("Child %i exited with status %i.", pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            PLUGIN_WARNING("Child %i was terminated by signal %i.", pid, WTERMSIG(status));
        } else {
            PLUGIN_DEBUG("Child %i exited with status %i.", pid, status);
        }
    }

    notify_exec_set_pid(pl, 0);

    notification_free(n);
    free(arg);
    strbuf_destroy(&buf);
    notify_exec_free_envp(envp);
    notify_exec_thread_done();
    pthread_exit((void *)0);
    return NULL;
}

static int notify_exec_notification(const notification_t *n,
                                    user_data_t __attribute__((unused)) * user_data)
{
    for (program_list_t *pm = pl_head; pm != NULL; pm = pm->next) {
        if (metric_match_cmp(&pm->match, n->name, &n->label) == false)
            continue;

        /* Skip if a child is already running, otherwise reserve the slot
         * (pid = -1) until the child is forked. */
        pthread_mutex_lock(&pl_lock);
        bool running = pm->pid != 0;
        if (!running)
            pm->pid = -1;
        pthread_mutex_unlock(&pl_lock);

        if (running) {
            PLUGIN_WARNING("Program '%s' is still running, discarding notification '%s'.",
                           pm->exec.exec != NULL ? pm->exec.exec : "", n->name);
            continue;
        }

        program_list_and_notification_t *pln = calloc(1, sizeof(*pln));
        if (pln == NULL) {
            PLUGIN_ERROR("calloc failed.");
            notify_exec_set_pid(pm, 0);
            continue;
        }

        pln->pl = pm;
        pln->n = notification_clone(n);
        if (pln->n == NULL) {
            PLUGIN_ERROR("notification_clone failed.");
            free(pln);
            notify_exec_set_pid(pm, 0);
            continue;
        }

        pthread_mutex_lock(&threads_lock);
        threads_num++;
        pthread_mutex_unlock(&threads_lock);

        pthread_t t = (pthread_t){0};
        int status = plugin_thread_create(&t, notify_exec_notification_one, (void *)pln,
                                              "notify exec");
        if (status == 0) {
            pthread_detach(t);
        } else {
            PLUGIN_ERROR("plugin_thread_create failed.");
            notification_free(pln->n);
            free(pln);
            notify_exec_set_pid(pm, 0);
            notify_exec_thread_done();
        }
    }

    return 0;
}

static void notify_exec_free(void *arg)
{
    program_list_t *pm = arg;

    if (pm == NULL)
        return;

    exec_reset(&pm->exec);
    notify_exec_free_envp(pm->envp);

    if (pm->match.name != NULL)
        metric_match_set_free(pm->match.name);
    if (pm->match.labels != NULL)
        metric_match_set_free(pm->match.labels);

    free(pm);
}

static int notify_exec_config_format(const config_item_t *ci, program_format_notification_t *format)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    char *option = ci->values[0].value.string;

    if (strcmp(option, "text") == 0) {
        *format = PROGRAM_FORMAT_NOTIFICATION_TEXT;
    } else if (strcmp(option, "json") == 0) {
        *format = PROGRAM_FORMAT_NOTIFICATION_JSON;
    } else if (strcmp(option, "protob") == 0) {
        *format = PROGRAM_FORMAT_NOTIFICATION_PROTOB;
    } else if (strcmp(option, "env") == 0) {
        *format = PROGRAM_FORMAT_NOTIFICATION_ENV;
    } else if (strcmp(option, "environment") == 0) {
        *format = PROGRAM_FORMAT_NOTIFICATION_ENV;
    } else {
        PLUGIN_ERROR("Invalid notification format: '%s' in %s:%d.",
                     option, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    return 0;
}

static int notify_exec_config_get_match(const config_item_t *ci, program_list_t *pm)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    int status = metric_match_unmarshal(&pm->match, ci->values[0].value.string);
    if(status != 0) {
        PLUGIN_ERROR("Cannot parse match in %s:%d.", cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    return 0;
}

static int notify_exec_config_match(config_item_t *ci, program_format_notification_t format)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The 'if-match' block needs exactly one string argument.");
        return -1;
    }

    program_list_t *pm = calloc(1, sizeof(*pm));
    if (pm == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    pm->format = format;
    pm->timeout = TIME_T_TO_CDTIME_T(60);

    int status = notify_exec_config_get_match(ci, pm);
    if (status != 0) {
        PLUGIN_ERROR("Invalid match filter.");
        notify_exec_free(pm);
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
        } else if (strcasecmp("format", child->key) == 0) {
            status = notify_exec_config_format(child, &pm->format);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &pm->timeout);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if ((status == 0) && (pm->exec.exec == NULL)) {
        PLUGIN_ERROR("Missing 'cmd' option in 'if-match' block in %s:%d.",
                     cf_get_file(ci), cf_get_lineno(ci));
        status = -1;
    }

    if (status != 0) {
        notify_exec_free(pm);
        return -1;
    }

    pm->envp = pm->exec.envp;
    pm->exec.envp = NULL;

    pm->next = pl_head;
    pl_head = pm;

    return 0;
}

static int notify_exec_config(config_item_t *ci)
{
    int status = 0;
    program_format_notification_t format = PROGRAM_FORMAT_NOTIFICATION_JSON;

    /* The global format applies to all the if-match blocks,
     * regardless of the order in the configuration. */
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("format", child->key) == 0) {
            status = notify_exec_config_format(child, &format);
            if (status != 0)
                return -1;
        }
    }

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("if-match", child->key) == 0) {
            status = notify_exec_config_match(child, format);
        } else if (strcasecmp("format", child->key) == 0) {
            continue;
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

static void notify_exec_kill_childs(int sig)
{
    for (program_list_t *pm = pl_head; pm != NULL; pm = pm->next) {
        pid_t pid = pm->pid;
        if (pid > 0) {
            kill(pid, sig);
            PLUGIN_INFO("Sent signal %d to %i", sig, (int)pid);
        }
    }
}

static int notify_exec_shutdown(void)
{
    /* The thread that forked each child is the one that waits for it. */
    notify_exec_kill_childs(SIGTERM);
    if (!notify_exec_wait_threads(5)) {
        notify_exec_kill_childs(SIGKILL);
        if (!notify_exec_wait_threads(5)) {
            PLUGIN_ERROR("Notification threads are still running, "
                         "not releasing the program list.");
            return 0;
        }
    }

    program_list_t *pm = pl_head;
    while (pm != NULL) {
        program_list_t *next = pm->next;
        notify_exec_free(pm);
        pm = next;
    }
    pl_head = NULL;

    return 0;
}

static int notify_exec_init(void)
{
#if defined(HAVE_SYS_CAPABILITY_H) && defined(CAP_SETUID) && defined(CAP_SETGID)
    if ((plugin_check_capability(CAP_SETUID) != 0) || (plugin_check_capability(CAP_SETGID) != 0)) {
        if (getuid() == 0)
            PLUGIN_WARNING("Running ncollectd as root, but the CAP_SETUID "
                           "or CAP_SETGID capabilities are missing. Executing the notification "
                           "programs will probably fail. Is your init system dropping capabilities?");
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
    plugin_register_init("notify_exec", notify_exec_init);
    plugin_register_config("notify_exec", notify_exec_config);
    plugin_register_notification("notify_exec", NULL, notify_exec_notification, NULL);
    plugin_register_shutdown("notify_exec", notify_exec_shutdown);
}
