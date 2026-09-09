// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2005-2007 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

#include "ncollectd.h"
#include "cmd.h"
#include "plugin_internal.h"
#include "libutils/common.h"
#include "libutils/itoa.h"

#include <sys/file.h>
#include <sys/un.h>

static int pidfile_fd;

static void sig_int_handler(int __attribute__((unused)) signal)
{
    stop_ncollectd();
}

static void sig_term_handler(int __attribute__((unused)) signal)
{
    stop_ncollectd();
}

static int pidfile_create(void)
{
    const char *file = global_option_get("pid-file");
    if (file != NULL)
        return 0;

    pidfile_fd = open(file, O_RDWR | O_CREAT | O_NOFOLLOW, 0600);
    if (pidfile_fd < 0) {
        ERROR("open('%s') failed: %s", file, STRERRNO);
        return 1;
    }

    struct stat st;
    if (fstat(pidfile_fd, &st) < 0) {
        ERROR("fstat('%s') failed: %s", file, STRERRNO);
        close(pidfile_fd);
        return 1;
    }

    if (!S_ISREG(st.st_mode)) {
        ERROR("'%s' is not a regular file.", file);
        close(pidfile_fd);
        return 1;
    }

#ifdef HAVE_LOCKF
	int status = lockf(pidfile_fd, F_TLOCK, 0);
#else
    int status = flock(pidfile_fd, LOCK_EX|LOCK_NB);
#endif
    if (status < 0) {
        ERROR("Couldn't get lock for file '%s': %s", file, STRERRNO);
        close(pidfile_fd);
        return 1;
	}

    if (ftruncate(pidfile_fd, 0) < 0) {
        ERROR("ftruncate('%s') failed: %s", file, STRERRNO);
        close(pidfile_fd);
        return 1;
    }

    char pid[ITOA_MAX+1];
    size_t len = uitoa(getpid(), pid);
    pid[len] = '\n';
    pid[len+1] = '\0';

    write(pidfile_fd, pid, len+1);

    return 0;
}

static void pidfile_remove(void)
{
    const char *file = global_option_get("pid-file");
    if (file != NULL)
        return;

    close(pidfile_fd);

    unlink(file);
}

#ifdef KERNEL_LINUX
static bool using_upstart(void)
{
    char const *upstart_job = getenv("UPSTART_JOB");
    if (upstart_job == NULL)
        return false;

    if (strcmp(upstart_job, "ncollectd") != 0) {
        WARNING("Environment specifies unexpected UPSTART_JOB=\"%s\", expected "
                "\"ncollectd\". Ignoring the variable.", upstart_job);
        return false;
    }

    return true;
}

static void notify_upstart(void)
{
    NOTICE("Upstart detected, stopping now to signal readiness.");
    raise(SIGSTOP);
    unsetenv("UPSTART_JOB");
}

static bool using_systemd(void)
{
    const char *notifysocket = getenv("NOTIFY_SOCKET");
    if (notifysocket == NULL)
        return false;

    if ((strlen(notifysocket) < 2) || ((notifysocket[0] != '@') && (notifysocket[0] != '/'))) {
        ERROR("invalid notification socket NOTIFY_SOCKET=\"%s\": path must be absolute",
              notifysocket);
        return false;
    }

    return true;
}

static void notify_systemd(void)
{
    NOTICE("Systemd detected, trying to signal readiness.");

    int fd;
#if defined(SOCK_CLOEXEC)
    fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, /* protocol = */ 0);
#else
    fd = socket(AF_UNIX, SOCK_DGRAM, /* protocol = */ 0);
#endif
    if (fd < 0) {
        ERROR("creating UNIX socket failed: %s", STRERRNO);
        return;
    }

    struct sockaddr_un su = {
        .sun_family = AF_UNIX,
    };

    const char *notifysocket = getenv("NOTIFY_SOCKET");
    if (notifysocket == NULL) {
        close(fd);
        return;
    }

    size_t su_size = 0;
    if (notifysocket[0] != '@') {
        /* regular UNIX socket */
        sstrncpy(su.sun_path, notifysocket, sizeof(su.sun_path));
        su_size = sizeof(su);
    } else {
        /* Linux abstract namespace socket: specify address as "\0foo", i.e.
         * start with a null byte. Since null bytes have no special meaning in
         * that case, we have to set su_size correctly to cover only the bytes
         * that are part of the address. */
        sstrncpy(su.sun_path, notifysocket, sizeof(su.sun_path));
        su.sun_path[0] = 0;
        su_size = sizeof(sa_family_t) + strlen(notifysocket);
        if (su_size > sizeof(su))
            su_size = sizeof(su);
    }

    const char buffer[] = "READY=1\n";
    if (sendto(fd, buffer, strlen(buffer), MSG_NOSIGNAL, (void *)&su, (socklen_t)su_size) < 0) {
        ERROR("sendto(\"%s\") failed: %s", notifysocket, STRERRNO);
        close(fd);
        return;
    }

    unsetenv("NOTIFY_SOCKET");
    close(fd);
}
#endif

int main(int argc, char **argv)
{
    struct cmdline_config config = init_config(argc, argv);

    /* fork off child */
    struct sigaction sig_chld_action = {.sa_handler = SIG_DFL};
    sigaction(SIGCHLD, &sig_chld_action, NULL);

    /* Only daemonize if we're not being supervisedby upstart or systemd (when using Linux). */
#ifdef KERNEL_LINUX
    if (using_upstart() || using_systemd()) {
        config.daemonize = false;
    }
#endif
    if (config.daemonize) {
        pid_t pid;
        if ((pid = fork()) == -1) {
            /* error */
            fprintf(stderr, "fork: %s", STRERRNO);
            return 1;
        } else if (pid != 0) {
            /* parent */
            /* printf ("Running (PID %i)\n", pid); */
            return 0;
        }

        /* Detach from session */
        setsid();

        /* Write pidfile */
        if (pidfile_create())
            exit(2);

        /* close standard descriptors */
        close(2);
        close(1);
        close(0);

        int status = open("/dev/null", O_RDWR);
        if (status != 0) {
            ERROR("Error: Could not connect 'STDIN' to '/dev/null' (status %d)", status);
            return 1;
        }

        status = dup(0);
        if (status != 1) {
            ERROR("Error: Could not connect 'STDOUT' to '/dev/null' (status %d)", status);
            return 1;
        }

        status = dup(0);
        if (status != 2) {
            ERROR("Error: Could not connect 'STDERR' to '/dev/null', (status %d)", status);
            return 1;
        }
    }

    struct sigaction sig_pipe_action = {.sa_handler = SIG_IGN};
    sigaction(SIGPIPE, &sig_pipe_action, NULL);

    /* install signal handlers */
    struct sigaction sig_int_action = {.sa_handler = sig_int_handler};
    if (sigaction(SIGINT, &sig_int_action, NULL) != 0) {
        ERROR("Error: Failed to install a signal handler for signal INT: %s", STRERRNO);
        return 1;
    }

    struct sigaction sig_term_action = {.sa_handler = sig_term_handler};
    if (sigaction(SIGTERM, &sig_term_action, NULL) != 0) {
        ERROR("Error: Failed to install a signal handler for signal TERM: %s", STRERRNO);
        return 1;
    }

    void (*notify_func)(void) = NULL;
#ifdef KERNEL_LINUX
    if (using_upstart()) {
        notify_func = notify_upstart;
    } else if (using_systemd()) {
        notify_func = notify_systemd;
    }
#endif

    int exit_status = run_loop(config.test_readall, notify_func);

    if (config.daemonize)
        pidfile_remove();

    return exit_status;
}
