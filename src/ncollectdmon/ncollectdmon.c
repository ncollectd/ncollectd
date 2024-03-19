// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2007  Sebastian Harl
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <syslog.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef PREFIX
#define PREFIX "/opt/" PACKAGE_NAME
#endif

#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR PREFIX "/var"
#endif

#ifndef NCOLLECTDMON_PIDFILE
#define NCOLLECTDMON_PIDFILE LOCALSTATEDIR "/run/ncollectdmon.pid"
#endif /* ! NCOLLECTDMON_PIDFILE */

#ifndef WCOREDUMP
#define WCOREDUMP(s) 0
#endif /* ! WCOREDUMP */

static int loop;
static int restart;

static const char *pidfile;
static pid_t ncollectd_pid;

__attribute__((noreturn)) static void exit_usage(const char *name)
{
    printf("Usage: %s <options> [-- <ncollectd options>]\n"
                 "\nAvailable options:\n"
                 "  -h               Display this help and exit.\n"
                 "  -c <path>  Path to the ncollectd binary.\n"
                 "  -P <file>  PID-file.\n"
                 "\nFor <ncollectd options> see ncollectd.conf(5).\n"
                 "\n" PACKAGE_NAME " " PACKAGE_VERSION "\n",
                 name);
    exit(0);
}

static int pidfile_create(void)
{
    FILE *file;

    if (pidfile == NULL)
        pidfile = NCOLLECTDMON_PIDFILE;

    if ((file = fopen(pidfile, "w")) == NULL) {
        syslog(LOG_ERR, "Error: couldn't open PID-file (%s) for writing: %s",
                         pidfile, strerror(errno));
        return -1;
    }

    fprintf(file, "%d\n", (int)getpid());
    fclose(file);
    return 0;
}

static int pidfile_delete(void)
{
    assert(pidfile);

    if (unlink(pidfile) != 0) {
        syslog(LOG_ERR, "Error: couldn't delete PID-file (%s): %s", pidfile,
                        strerror(errno));
        return -1;
    }
    return 0;
}

static int daemonize(void)
{
    if (chdir("/") != 0) {
        fprintf(stderr, "Error: chdir() failed: %s\n", strerror(errno));
        return -1;
    }

    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) != 0) {
        fprintf(stderr, "Error: getrlimit() failed: %s\n", strerror(errno));
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Error: fork() failed: %s\n", strerror(errno));
        return -1;
    } else if (pid != 0) {
        exit(0);
    }

    if (pidfile_create() != 0)
        return -1;

    setsid();

    if (rl.rlim_max == RLIM_INFINITY)
        rl.rlim_max = 1024;

    for (int i = 0; i < (int)rl.rlim_max; ++i)
        close(i);

    int dev_null = open("/dev/null", O_RDWR);
    if (dev_null == -1) {
        syslog(LOG_ERR, "Error: couldn't open /dev/null: %s", strerror(errno));
        return -1;
    }

    if (dup2(dev_null, STDIN_FILENO) == -1) {
        close(dev_null);
        syslog(LOG_ERR, "Error: couldn't connect STDIN to /dev/null: %s",
                     strerror(errno));
        return -1;
    }

    if (dup2(dev_null, STDOUT_FILENO) == -1) {
        close(dev_null);
        syslog(LOG_ERR, "Error: couldn't connect STDOUT to /dev/null: %s",
                     strerror(errno));
        return -1;
    }

    if (dup2(dev_null, STDERR_FILENO) == -1) {
        close(dev_null);
        syslog(LOG_ERR, "Error: couldn't connect STDERR to /dev/null: %s",
                     strerror(errno));
        return -1;
    }

    if ((dev_null != STDIN_FILENO) && (dev_null != STDOUT_FILENO) &&
            (dev_null != STDERR_FILENO))
        close(dev_null);

    return 0;
}

static int ncollectd_start(char **argv)
{
    pid_t pid = fork();

    if (pid < 0) {
        syslog(LOG_ERR, "Error: fork() failed: %s", strerror(errno));
        return -1;
    } else if (pid != 0) {
        ncollectd_pid = pid;
        return 0;
    }

    execvp(argv[0], argv);
    syslog(LOG_ERR, "Error: execvp(%s) failed: %s", argv[0], strerror(errno));
    exit(-1);
}

static int ncollectd_stop(void)
{
    if (ncollectd_pid == 0)
        return 0;

    if (kill(ncollectd_pid, SIGTERM) != 0) {
        syslog(LOG_ERR, "Error: kill() failed: %s", strerror(errno));
        return -1;
    }
    return 0;
}

static void sig_int_term_handler(int __attribute__((unused)) signo)
{
    ++loop;
    return;
}

static void sig_hup_handler(int __attribute__((unused)) signo)
{
    ++restart;
    return;
}

static void log_status(int status)
{
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == 0)
            syslog(LOG_INFO, "Info: ncollectd terminated with exit status %i",
                              WEXITSTATUS(status));
        else
            syslog(LOG_WARNING, "Warning: ncollectd terminated with exit status %i",
                                WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        syslog(LOG_WARNING, "Warning: ncollectd was terminated by signal %i%s",
                             WTERMSIG(status), WCOREDUMP(status) ? " (core dumped)" : "");
    }
    return;
}

static void check_respawn(void)
{
    time_t t = time(NULL);

    static time_t timestamp;
    static int counter;

    if (timestamp >= t - 120)
        ++counter;
    else {
        timestamp = t;
        counter = 0;
    }

    if (counter >= 10) {
        unsigned int time_left = 300;
        syslog(LOG_ERR, "Error: ncollectd is respawning too fast - "
                        "disabled for %u seconds", time_left);

        while (((time_left = sleep(time_left)) > 0) && loop == 0)
            ;
    }
    return;
}

int main(int argc, char **argv)
{
    int ncollectd_argc = 0;
    char *ncollectd = NULL;
    char **ncollectd_argv = NULL;

    int i = 0;

    /* parse command line options */
    while (true) {
        int c = getopt(argc, argv, "hc:P:");

        if (c == -1)
            break;

        switch (c) {
        case 'c':
            ncollectd = optarg;
            break;
        case 'P':
            pidfile = optarg;
            break;
        case 'h':
        default:
            exit_usage(argv[0]);
        }
    }

    for (i = optind; i < argc; ++i)
        if (strcmp(argv[i], "-f") == 0)
            break;

    /* i < argc => -f already present */
    ncollectd_argc = 1 + argc - optind + ((i < argc) ? 0 : 1);
    ncollectd_argv = calloc(ncollectd_argc + 1, sizeof(*ncollectd_argv));

    if (ncollectd_argv == NULL) {
        fprintf(stderr, "Out of memory.");
        return 3;
    }

    ncollectd_argv[0] = (ncollectd == NULL) ? "ncollectd" : ncollectd;

    if (i == argc)
        ncollectd_argv[ncollectd_argc - 1] = "-f";

    for (i = optind; i < argc; ++i)
        ncollectd_argv[i - optind + 1] = argv[i];

    ncollectd_argv[ncollectd_argc] = NULL;

    openlog("ncollectdmon", LOG_CONS | LOG_PID, LOG_DAEMON);

    if (daemonize() == -1) {
        free(ncollectd_argv);
        return 1;
    }

    struct sigaction sa = {
        .sa_handler = sig_int_term_handler,
        .sa_flags = 0,
    };
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) != 0) {
        syslog(LOG_ERR, "Error: sigaction() failed: %s", strerror(errno));
        free(ncollectd_argv);
        return 1;
    }

    if (sigaction(SIGTERM, &sa, NULL) != 0) {
        syslog(LOG_ERR, "Error: sigaction() failed: %s", strerror(errno));
        free(ncollectd_argv);
        return 1;
    }

    sa.sa_handler = sig_hup_handler;

    if (sigaction(SIGHUP, &sa, NULL) != 0) {
        syslog(LOG_ERR, "Error: sigaction() failed: %s", strerror(errno));
        free(ncollectd_argv);
        return 1;
    }

    while (loop == 0) {
        int status = 0;

        if (ncollectd_start(ncollectd_argv) != 0) {
            syslog(LOG_ERR, "Error: failed to start ncollectd.");
            break;
        }

        assert(ncollectd_pid >= 0);
        while ((ncollectd_pid != waitpid(ncollectd_pid, &status, 0)) && errno == EINTR)
            if (loop != 0 || restart != 0)
                ncollectd_stop();

        ncollectd_pid = 0;

        log_status(status);
        check_respawn();

        if (restart != 0) {
            syslog(LOG_INFO, "Info: restarting ncollectd");
            restart = 0;
        } else if (loop == 0)
            syslog(LOG_WARNING, "Warning: restarting ncollectd");
    }

    syslog(LOG_INFO, "Info: shutting down ncollectdmon");

    pidfile_delete();
    closelog();

    free(ncollectd_argv);
    return 0;
}
