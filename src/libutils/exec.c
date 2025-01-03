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

#include "log.h"
#include "libutils/common.h"
#include "libutils/dtoa.h"
#include "libutils/config.h"
#include "libutils/exec.h"

#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <sys/types.h>

extern char **environ;
static const long int MAX_GRBUF_SIZE = 65536;

__attribute__((noreturn))
static void exec_child(const char *file, char *const argv[], char *envp[],
                       int uid, int gid, int egid)
{
#ifdef HAVE_SETGROUPS
    if (getuid() == 0) {
        gid_t glist[2];
        size_t glist_len;

        glist[0] = gid;
        glist_len = 1;

        if ((gid != egid) && (egid != -1)) {
            glist[1] = egid;
            glist_len = 2;
        }

        setgroups(glist_len, glist);
    }
#endif

    int status = setgid(gid);
    if (status != 0) {
        PLUGIN_ERROR("setgid (%i) failed: %s", gid, STRERRNO);
        exit(-1);
    }

    if (egid != -1) {
        status = setegid(egid);
        if (status != 0) {
            PLUGIN_ERROR("setegid (%i) failed: %s", egid, STRERRNO);
            exit(-1);
        }
    }

    status = setuid(uid);
    if (status != 0) {
        PLUGIN_ERROR("setuid (%i) failed: %s", uid, STRERRNO);
        exit(-1);
    }

#ifdef HAVE_EXECVPE
    execvpe(file, argv, envp);
#else
    environ = envp;
    execvp(file, argv);
#endif

    PLUGIN_ERROR("Failed to execute '%s': %s", file, STRERRNO);
    exit(-1);
}

static void reset_signal_mask(void)
{
    sigset_t ss;

    sigemptyset(&ss);
    sigprocmask(SIG_SETMASK, &ss, /* old mask = */ NULL);
}

static int create_pipe(int fd_pipe[2])
{
    int status = pipe(fd_pipe);
    if (status != 0) {
        PLUGIN_ERROR("pipe failed: %s", STRERRNO);
        return -1;
    }

    if ((fd_pipe[0] < 0) || (fd_pipe[1] < 0)) {
        PLUGIN_ERROR("pipe failed. Return negative file descriptor.");
        return -1;
    }

    return 0;
}

static void close_pipe(int fd_pipe[2])
{
    if (fd_pipe[0] >= 0)
        close(fd_pipe[0]);

    if (fd_pipe[1] >= 0)
        close(fd_pipe[1]);
}

/*
 * Get effective group ID from group name.
 * Input arguments:
 *             pl  :program list struct with group name
 *             gid :group id to fallback in case egid cannot be determined.
 * Returns:
 *             egid effective group id if successfull,
 *                        -1 if group is not defined/not found.
 *                        -2 for any buffer allocation error.
 */
static int getegr_id(char *group, int gid)
{
    if ((group == NULL) || (*group == '\0'))
        return gid;

    struct group *gr_ptr = NULL;
    struct group gr;

    long int grbuf_size = sysconf(_SC_GETGR_R_SIZE_MAX);
    if (grbuf_size <= 0)
        grbuf_size = sysconf(_SC_PAGESIZE);
    if (grbuf_size <= 0)
        grbuf_size = 4096;

    char *grbuf = NULL;
    do {
        char *temp = realloc(grbuf, grbuf_size);
        if (temp == NULL) {
            PLUGIN_ERROR("getegr_id for %s: realloc buffer[%ld] failed ", group, grbuf_size);
            free(grbuf);
            return -2;
        }
        grbuf = temp;
        if (getgrnam_r(group, &gr, grbuf, grbuf_size, &gr_ptr) == 0) {
            free(grbuf);
            if (gr_ptr == NULL) {
                PLUGIN_ERROR("No such group: '%s'", group);
                return -1;
            }
            return gr.gr_gid;
        } else if (errno == ERANGE) {
            grbuf_size += grbuf_size; // increment buffer size and try again
        } else {
            PLUGIN_ERROR("getegr_id failed %s", STRERRNO);
            free(grbuf);
            return -2;
        }
    } while (grbuf_size <= MAX_GRBUF_SIZE);

    PLUGIN_ERROR("getegr_id Max grbuf size reached for %s", group);
    free(grbuf);
    return -2;
}

static void close_all(int fd_in, int fd_out, int fd_err)
{
#if defined(HAVE_CLOSE_RANGE) || defined(HAVE_CLOSEFROM) || defined(HAVE_FCNTL_CLOSEM)
    int fd_max = fd_in;
    if (fd_out > fd_max)
        fd_max = fd_out;
    if (fd_err > fd_max)
        fd_max = fd_err;

#ifdef HAVE_CLOSE_RANGE
#ifdef CLOSE_RANGE_UNSHARE
    close_range(fd_max + 1, ~0U, CLOSE_RANGE_UNSHARE);
#else
    close_range(fd_max + 1, ~0U, 0);
#endif
#elif defined(HAVE_CLOSEFROM)
    closefrom(fd_max + 1);
#elif defined(HAVE_FCNTL_CLOSEM)
    fcntl(fd_max + 1, F_CLOSEM, 0);
#endif

    for (int fd = 0; fd < fd_max; fd++) {
        if ((fd == fd_in) || (fd == fd_out) || (fd == fd_err))
            continue;
        close(fd);
    }
#else
    int fd_num = getdtablesize();
    for (int fd = 0; fd < fd_num; fd++) {
        if ((fd == fd_in) || (fd == fd_out) || (fd == fd_err))
            continue;
        close(fd);
    }
#endif
}

/*
 * Creates three pipes (one for reading, one for writing and one for errors),
 * forks a child, sets up the pipes so that fd_in is connected to STDIN of
 * the child and fd_out is connected to STDOUT and fd_err is connected to STDERR
 * of the child. Then is calls 'exec_child'.
 */
int exec_fork_child(cexec_t *pm, bool can_be_root, int *fd_in, int *fd_out, int *fd_err)
{
    int fd_pipe_in[2] = {-1, -1};
    int fd_pipe_out[2] = {-1, -1};
    int fd_pipe_err[2] = {-1, -1};

    long int nambuf_size = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (nambuf_size <= 0)
        nambuf_size = sysconf(_SC_PAGESIZE);
    if (nambuf_size <= 0)
        nambuf_size = 4096;
    char nambuf[nambuf_size];

    if ((create_pipe(fd_pipe_in) == -1) || (create_pipe(fd_pipe_out) == -1) ||
        (create_pipe(fd_pipe_err) == -1))
        goto failed;

    int uid = getuid();
    int gid = getgid();

    if ((pm->user != NULL) && (*pm->user != '\0')) {
        struct passwd *sp_ptr = NULL;
        struct passwd sp = {0};
        int status = getpwnam_r(pm->user, &sp, nambuf, sizeof(nambuf), &sp_ptr);
        if (status != 0) {
            PLUGIN_ERROR("Failed to get user information for user ''%s'': %s",
                         pm->user, STRERROR(status));
            goto failed;
        }

        if (sp_ptr == NULL) {
            PLUGIN_ERROR("No such user: '%s'", pm->user);
            goto failed;
        }

        uid = sp.pw_uid;
        gid = sp.pw_gid;
    }

    if ((can_be_root == false) && (uid == 0)) {
        PLUGIN_ERROR("Cowardly refusing to exec program as root.");
        goto failed;
    }

    /* The group configured in the configfile is set as effective group, because
     * this way the forked process can (re-)gain the user's primary group. */
    int egid = getegr_id(pm->group, gid);
    if (egid == -2)
        goto failed;

    int pid = fork();
    if (pid < 0) {
        PLUGIN_ERROR("fork failed: %s", STRERRNO);
        goto failed;
    } else if (pid == 0) {
        /* Close all file descriptors but the pipe end we need. */
        close_all(fd_pipe_in[0], fd_pipe_out[1], fd_pipe_err[1]);

        /* Connect the 'in' pipe to STDIN */
        if (fd_pipe_in[0] != STDIN_FILENO) {
            dup2(fd_pipe_in[0], STDIN_FILENO);
            close(fd_pipe_in[0]);
        }

        /* Now connect the 'out' pipe to STDOUT */
        if (fd_pipe_out[1] != STDOUT_FILENO) {
            dup2(fd_pipe_out[1], STDOUT_FILENO);
            close(fd_pipe_out[1]);
        }

        /* Now connect the 'err' pipe to STDERR */
        if (fd_pipe_err[1] != STDERR_FILENO) {
            dup2(fd_pipe_err[1], STDERR_FILENO);
            close(fd_pipe_err[1]);
        }

        /* Unblock all signals */
        reset_signal_mask();

        exec_child(pm->exec, pm->argv, pm->envp, uid, gid, egid);
        /* does not return */
    }

    close(fd_pipe_in[0]);
    close(fd_pipe_out[1]);
    close(fd_pipe_err[1]);

    if (fd_in != NULL)
        *fd_in = fd_pipe_in[1];
    else
        close(fd_pipe_in[1]);

    if (fd_out != NULL)
        *fd_out = fd_pipe_out[0];
    else
        close(fd_pipe_out[0]);

    if (fd_err != NULL)
        *fd_err = fd_pipe_err[0];
    else
        close(fd_pipe_err[0]);

    return pid;

failed:
    close_pipe(fd_pipe_in);
    close_pipe(fd_pipe_out);
    close_pipe(fd_pipe_err);

    return -1;
}

int cf_util_exec_cmd(config_item_t *ci, cexec_t *pm)
{
    if ((ci->values_num < 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' in %s:%d option requires one or more string arguments.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    pm->exec = strdup(ci->values[0].value.string);
    if (pm->exec == NULL) {
        PLUGIN_ERROR("strdup failed.");
        return -1;
    }

    pm->argv = calloc(ci->values_num+1, sizeof(*pm->argv));
    if (pm->argv == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    char buffer[512];
    char *tmp = strrchr(ci->values[0].value.string, '/');
    if (tmp == NULL)
        sstrncpy(buffer, ci->values[0].value.string, sizeof(buffer));
    else
        sstrncpy(buffer, tmp + 1, sizeof(buffer));

    pm->argv[0] = strdup(buffer);
    if (pm->argv[0] == NULL) {
        PLUGIN_ERROR("strdup failed.");
        return -1;
    }

    for (int i = 1; i < ci->values_num; i++) {
        if (ci->values[i].type == CONFIG_TYPE_STRING) {
            pm->argv[i] = strdup(ci->values[i].value.string);
        } else {
            if (ci->values[i].type == CONFIG_TYPE_NUMBER) {
                snprintf(buffer, sizeof(buffer), "%lf", ci->values[i + 1].value.number);
            } else if (ci->values[i].type == CONFIG_TYPE_BOOLEAN) {
                if (ci->values[i].value.boolean)
                    sstrncpy(buffer, "true", sizeof(buffer));
                else
                    sstrncpy(buffer, "false", sizeof(buffer));
            } else {
                buffer[0] = '\0';
            }

            pm->argv[i] = strdup(buffer);
        }

        if (pm->argv[i] == NULL) {
            PLUGIN_ERROR("strdup failed.");
            break;
        }
    }

    return 0;
}

int cexec_append_env(cexec_t *pm, const char *key, const char *value)
{
    if ((key == NULL) || (value == NULL))
        return 0;

    size_t env_size = 0;
    if (pm->envp != NULL) {
        while (pm->envp[env_size] != NULL) {
            ++env_size;
        }
    }

    size_t str_size = strlen(key) + 1 + strlen(value) + 1;
    char *str = malloc (str_size);
    if (str == NULL) {
        PLUGIN_ERROR("malloc failed");
        return -1;
    }

    ssnprintf(str, str_size, "%s=%s", key, value);

    char **tmp = realloc(pm->envp, sizeof(char *) * (env_size + 2));
    if (tmp == NULL) {
        free(str);
        PLUGIN_ERROR("realloc failed");
        return -1;
    }

    pm->envp = tmp;
    pm->envp[env_size] = str;
    pm->envp[env_size+1] = NULL;

    return 0;
}

int cf_util_exec_append_env(config_item_t *ci, cexec_t *pm)
{
    if ((ci->values_num != 2) ||
        ((ci->values_num == 2) && ((ci->values[0].type != CONFIG_TYPE_STRING) ||
                                   (ci->values[1].type != CONFIG_TYPE_STRING)))) {
        PLUGIN_ERROR("The '%s' in %s:%d option requires exactly two string arguments.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    return cexec_append_env(pm, ci->values[0].value.string, ci->values[1].value.string);
}

void exec_reset(cexec_t *pm)
{
    if (pm == NULL)
        return;

    if (pm->user != NULL) {
        free(pm->user);
        pm->user = NULL;
    }

    if (pm->group != NULL) {
        free(pm->group);
        pm->group = NULL;
    }

    if (pm->exec != NULL) {
        free(pm->exec);
        pm->exec = NULL;
    }

    if (pm->argv != NULL) {
        for (int i = 0; pm->argv[i] != NULL; i++) {
            free(pm->argv[i]);
        }
        free(pm->argv);
        pm->argv = NULL;
    }

    if (pm->envp!=NULL) {
        size_t env_size=0;
        while (pm->envp[env_size] != NULL) {
            free(pm->envp[env_size]);
            env_size++;
        }
        free(pm->envp);
        pm->envp = NULL;
    }
}
