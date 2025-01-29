// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2005-2007 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Alvaro Barcellos <alvaro.barcellos at gmail.com>

#include "ncollectd.h"
#include "globals.h"
#include "cmd.h"
#include "configfile.h"
#include "plugin_internal.h"
#include "httpd.h"
#include "libutils/common.h"

#include <netdb.h>
#include <sys/types.h>

#include <sys/stat.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifndef NCOLLECTD_LOCALE
#define NCOLLECTD_LOCALE "C"
#endif

static int loop;

static int init_hostname(void)
{
    const char *str = global_option_get("hostname");
    if (str && str[0] != '\0') {
        hostname_set(str);
        return 0;
    }

    long hostname_len = sysconf(_SC_HOST_NAME_MAX);
    if (hostname_len == -1)
        hostname_len = NI_MAXHOST;

    char hostname[hostname_len];

    if (gethostname(hostname, hostname_len) != 0) {
        fprintf(stderr, "'gethostname' failed and no hostname was configured.\n");
        return -1;
    }

    hostname_set(hostname);

    str = global_option_get("fqdn-lookup");
    if (IS_FALSE(str))
        return 0;

    struct addrinfo *ai_list;
    struct addrinfo ai_hints = {.ai_flags = AI_CANONNAME};

    int status = getaddrinfo(hostname, NULL, &ai_hints, &ai_list);
    if (status != 0) {
        ERROR("Looking up '%s' failed. You have set the 'fqdn-lookup' option, "
              "but I cannot resolve my hostname to a fully qualified domain name. "
              "Please fix the network configuration.", hostname);
        return -1;
    }

    for (struct addrinfo *ai_ptr = ai_list; ai_ptr; ai_ptr = ai_ptr->ai_next) {
        if (ai_ptr->ai_canonname == NULL)
            continue;

        hostname_set(ai_ptr->ai_canonname);
        break;
    }

    freeaddrinfo(ai_list);
    return 0;
}

static int init_global_variables(void)
{
    interval_g = cf_get_default_interval();
    assert(interval_g > 0);
    DEBUG("interval_g = %.3f;", CDTIME_T_TO_DOUBLE(interval_g));

    const char *str = global_option_get("timeout");
    if (str == NULL)
        str = "2";
    timeout_g = atoi(str);
    if (timeout_g <= 1) {
        fprintf(stderr, "Cannot set the timeout to a correct value.\n"
                        "Please check your settings.\n");
        return -1;
    }
    DEBUG("timeout_g = %i;", timeout_g);

    if (init_hostname() != 0)
        return -1;
    DEBUG("hostname_g = %s;", hostname_g);

    return 0;
}

static int change_basedir(const char *orig_dir, bool create)
{
    char *dir = strdup(orig_dir);
    if (dir == NULL) {
        ERROR("strdup failed: %s", STRERRNO);
        return -1;
    }

    size_t dirlen = strlen(dir);
    while ((dirlen > 0) && (dir[dirlen - 1] == '/'))
        dir[--dirlen] = '\0';

    if (dirlen == 0) {
        free(dir);
        return -1;
    }

    int status = chdir(dir);
    if (status == 0) {
        free(dir);
        return 0;
    } else if (!create || (errno != ENOENT)) {
        ERROR("change_basedir: chdir (%s): %s", dir, STRERRNO);
        free(dir);
        return -1;
    }

    status = mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO);
    if (status != 0) {
        ERROR("change_basedir: mkdir (%s): %s", dir, STRERRNO);
        free(dir);
        return -1;
    }

    status = chdir(dir);
    if (status != 0) {
        ERROR("change_basedir: chdir (%s): %s", dir, STRERRNO);
        free(dir);
        return -1;
    }

    free(dir);
    return 0;
}

__attribute__((noreturn)) static void exit_usage(int status)
{
    printf("Usage: " PACKAGE_NAME " [OPTIONS]\n\n"
           "Available options:\n"
           "  General:\n"
           "      -C <file>       Configuration file.\n"
           "                          Default: " CONFIGFILE "\n"
           "      -t              Test config and exit.\n"
           "      -T              Test plugin read and exit.\n"
           "      -P <file>       PID-file.\n"
           "                          Default: " PIDFILE "\n"
           "      -f              Don't fork to the background.\n"
           "      -B              Don't create the BaseDir\n"
           "      -d              Dump config file to stdout\n"
           "      -h              Display help (this message)\n"
           "\nBuiltin defaults:\n"
           "  Config file         " CONFIGFILE "\n"
           "  PID file            " PIDFILE "\n"
           "  Plugin directory    " PLUGINDIR "\n"
           "  Data directory      " PKGLOCALSTATEDIR "\n"
           "\n" PACKAGE_NAME " " PACKAGE_VERSION ", http://ncollectd.org/\n");
    exit(status);
}

static int do_init(void)
{
#ifdef HAVE_SETLOCALE
    if (setlocale(LC_NUMERIC, NCOLLECTD_LOCALE) == NULL)
        WARNING("setlocale (\"%s\") failed.", NCOLLECTD_LOCALE);

    /* Update the environment, so that libraries that are calling
     * setlocale(LC_NUMERIC, "") don't accidentally revert these changes. */
    unsetenv("LC_ALL");
    setenv("LC_NUMERIC", NCOLLECTD_LOCALE, /* overwrite = */ 1);
#endif

    return plugin_init_all();
}

static int do_loop(void)
{
    cdtime_t interval = cf_get_default_interval();
    cdtime_t wait_until = cdtime() + interval;

    while (loop == 0) {
        // TODO do something

        cdtime_t now = cdtime();
        if (now >= wait_until) {
            WARNING("Not sleeping because the next interval is %.3f seconds in the past!",
                    CDTIME_T_TO_DOUBLE(now - wait_until));
            wait_until = now + interval;
            continue;
        }

        struct timespec ts_wait = CDTIME_T_TO_TIMESPEC(wait_until - now);
        wait_until = wait_until + interval;

        while ((loop == 0) && (nanosleep(&ts_wait, &ts_wait) != 0)) {
            if (errno != EINTR) {
                ERROR("nanosleep failed: %s", STRERRNO);
                return -1;
            }
        }
    }

    return 0;
}

static int do_shutdown(void)
{
    return plugin_shutdown_all();
}

static void read_cmdline(int argc, char **argv, struct cmdline_config *config)
{
    /* read options */
    while (true) {
        int c = getopt(argc, argv, "BdhtTfC:P:");
        if (c == -1)
            break;

        switch (c) {
        case 'B':
            config->create_basedir = false;
            break;
        case 'C':
            config->configfile = optarg;
            break;
        case 't':
            config->test_config = true;
            break;
        case 'T':
            config->test_readall = true;
            global_option_set("read-threads", "-1", 1);
            config->daemonize = false;
            break;
        case 'P':
            global_option_set("pid-file", optarg, 1);
            break;
        case 'f':
            config->daemonize = false;
            break;
        case 'd':
            config->dump_config = true;
            break;
        case 'h':
            exit_usage(EXIT_SUCCESS);
        default:
            exit_usage(EXIT_FAILURE);
        }
    }
}

static int configure_collectd(struct cmdline_config *config)
{
    /*
     * Read options from the config file, the environment and the command
     * line (in that order, with later options overwriting previous ones in
     * general).
     * Also, this will automatically load modules.
     */
    if (cf_read(config->configfile, config->dump_config)) {
        fprintf(stderr, "Error: Parsing the config file failed!\n");
        return 1;
    }

    /*
     * Change directory. We do this _after_ reading the config and loading
     * modules to relative paths work as expected.
     */
    const char *basedir = global_option_get("base-dir");
    if (basedir == NULL) {
        fprintf(stderr, "Don't have a basedir to use. This should not happen. Ever.");
        return 1;
    } else if (change_basedir(basedir, config->create_basedir)) {
        fprintf(stderr, "Error: Unable to change to directory '%s'.\n", basedir);
        return 1;
    }

    /*
     * Set global variables or, if that fails, exit. We cannot run with
     * them being uninitialized. If nothing is configured, then defaults
     * are being used. So this means that the user has actually done
     * something wrong.
     */
    if (init_global_variables() != 0)
        return 1;

    return 0;
}

void stop_ncollectd(void) { loop++; }

struct cmdline_config init_config(int argc, char **argv)
{
    struct cmdline_config config = {
        .daemonize = true,
        .create_basedir = true,
        .configfile = CONFIGFILE,
    };

    read_cmdline(argc, argv, &config);

    if (optind < argc)
        exit_usage(EXIT_FAILURE);

    plugin_init_ctx();

    if (configure_collectd(&config) != 0)
        exit(EXIT_FAILURE);

    if (config.test_config)
        exit(EXIT_SUCCESS);

    return config;
}

int run_loop(bool test_readall, void (*notify_func)(void))
{
    int exit_status = 0;

    if (do_init() != 0) {
        ERROR("Error: one or more plugin init callbacks failed.");
        exit_status = 1;
    }

    if (test_readall) {
        if (plugin_read_all_once() != 0) {
            ERROR("Error: one or more plugin read callbacks failed.");
            exit_status = 1;
        }
    } else {
        http_server_init();

        if (notify_func != NULL) {
            /* notify upstart or systemd that initialization has completed. */
            notify_func();
        }

        INFO("Initialization complete, entering read-loop.");
        do_loop();

        http_server_shutdown();
    }

    /* close syslog */
    INFO("Exiting normally.");

    if (do_shutdown() != 0) {
        ERROR("Error: one or more plugin shutdown callbacks failed.");
        exit_status = 1;
    }

    return exit_status;
}
