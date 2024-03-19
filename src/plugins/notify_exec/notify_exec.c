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
    pid_t pid;
    program_list_t *next;
};

typedef struct {
    program_list_t *pl;
    notification_t *n;
} program_list_and_notification_t;

static program_list_t *pl_head;

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
    char buffer[4096];
    strbuf_t buf = STRBUF_CREATE_FIXED(buffer, sizeof(buffer));

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
        return NULL;
    }

    size_t i = 0;
    while (i < default_env_size) {
        envp[i] = strdup(default_envp[i]);
        if (envp[i] == NULL) {
            notify_exec_free_envp(envp);
            PLUGIN_ERROR("strdup failed.");
            return NULL;
        }
        i++;
    }

    int status = strbuf_putstr(&buf, "NOTIFICATION_TIMESTAMP=");
    status |= strbuf_putuint(&buf, CDTIME_T_TO_TIME_T(n->time));
    envp[i] = strdup(buf.ptr);
    if (envp[i] == NULL) {
        notify_exec_free_envp(envp);
        PLUGIN_ERROR("strdup failed.");
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
        status |= strbuf_putstr(&buf, "UNKNOW");
        break;
    }
    envp[i] = strdup(buf.ptr);
    if (envp[i] == NULL) {
        notify_exec_free_envp(envp);
        PLUGIN_ERROR("strdup failed.");
        return NULL;
    }
    i++;
    strbuf_reset(&buf);

    status |= strbuf_putstr(&buf, "NOTIFICATION_NAME=");
    status |= strbuf_putstr(&buf, n->name);
    envp[i] = strdup(buf.ptr);
    if (envp[i] == NULL) {
        notify_exec_free_envp(envp);
        PLUGIN_ERROR("strdup failed.");
        return NULL;
    }
    i++;
    strbuf_reset(&buf);

    for (size_t j = 0; j < n->label.num; j++) {
        status |= strbuf_putstr(&buf, "NOTIFICATION_LABEL_");
        status |= strbuf_putstrtoupper(&buf, n->label.ptr[j].name);
        status |= strbuf_putchar(&buf, '=');
        status |= strbuf_putstr(&buf, n->label.ptr[j].value);
        envp[i] = strdup(buf.ptr);
        if (envp[i] == NULL) {
            notify_exec_free_envp(envp);
            PLUGIN_ERROR("strdup failed.");
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
        envp[i] = strdup(buf.ptr);
        if (envp[i] == NULL) {
            notify_exec_free_envp(envp);
            PLUGIN_ERROR("strdup failed.");
            return NULL;
        }
        i++;
        strbuf_reset(&buf);
    }

    envp[i] = NULL;

    if (status != 0) {
        notify_exec_free_envp(envp);
        PLUGIN_ERROR("Failed to build enviroment.");
        return NULL;
    }

    return envp;
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
        break;
    }

    pl->exec.envp = envp == NULL ? pl->envp : envp;

    int fd = 0;
    int pid = exec_fork_child(&pl->exec, false, &fd, NULL, NULL);
    if (pid < 0) {
        notification_free(n);
        free(arg);
        strbuf_destroy(&buf);
        notify_exec_free_envp(envp);
        pthread_exit((void *)1);
    }
    pl->pid = pid;

    if (pl->format != PROGRAM_FORMAT_NOTIFICATION_ENV) {
        if (strbuf_len(&buf) > 0) {
            ssize_t status = swrite(fd, buf.ptr, strbuf_len(&buf));
            if (status != 0) {
                PLUGIN_ERROR("write(%i) failed: %s", fd, STRERRNO);
                kill(pid, SIGTERM);
                close(fd);
                notification_free(n);
                free(arg);
                strbuf_destroy(&buf);
                notify_exec_free_envp(envp);
                pthread_exit((void *)1);
                return NULL;
            }
        }
    }

    close(fd);

    int status;
    waitpid(pid, &status, 0);
    PLUGIN_DEBUG("Child %i exited with status %i.", pid, status);

    pl->pid = 0;

    notification_free(n);
    free(arg);
    strbuf_destroy(&buf);
    notify_exec_free_envp(envp);

    pthread_exit((void *)0);
    return NULL;
}

static int notify_exec_notification(const notification_t *n,
                                    user_data_t __attribute__((unused)) * user_data)
{
    for (program_list_t *pm = pl_head; pm != NULL; pm = pm->next) {
        if (metric_match_cmp(&pm->match, n->name, &n->label) == false)
            continue;

        /* Skip if a child is already running. */
        if (pm->pid != 0)
            continue;

        program_list_and_notification_t *pln = calloc(1, sizeof(*pln));
        if (pln == NULL) {
            PLUGIN_ERROR("calloc failed.");
            continue;
        }

        pln->pl = pm;
        pln->n = notification_clone(n);
        if (pln->n == NULL) {
            PLUGIN_ERROR("notification_clone failed.");
            free(pln);
            continue;
        }

        pthread_t t = (pthread_t){0};
        int status = plugin_thread_create(&t, notify_exec_notification_one, (void *)pln,
                                              "notify exec");
        if (status == 0) {
            pthread_detach(t);
        } else {
            PLUGIN_ERROR("plugin_thread_create failed.");
        }
    }

    return 0;
}

static void notify_exec_free(void *arg)
{
    program_list_t *pm = arg;

    if (pm == NULL)
        return;

    if (pm->pid > 0) {
        kill(pm->pid, SIGTERM);
        PLUGIN_INFO("Sent SIGTERM to %hu", (unsigned short int)pm->pid);

        waitpid(pm->pid, NULL, 0);
    }

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
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
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

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("if-match", child->key) == 0) {
            status = notify_exec_config_match(child, format);
        } else if (strcasecmp("format", child->key) == 0) {
            status = notify_exec_config_format(child, &format);
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

static int notify_exec_shutdown(void)
{
    program_list_t *pm = pl_head;
    while (pm != NULL) {
        program_list_t *next = pm->next;
        notify_exec_free(pm);
        pm = next;
    }

    return 0;
}

static int notify_exec_init(void)
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
    plugin_register_init("notify_exec", notify_exec_init);
    plugin_register_config("notify_exec", notify_exec_config);
    plugin_register_notification(NULL, "notify_exec", notify_exec_notification, NULL);
    plugin_register_shutdown("notify_exec", notify_exec_shutdown);
}
