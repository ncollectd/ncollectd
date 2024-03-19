// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2010 Håko J Dugstad Johnsen
// SPDX-FileCopyrightText: Copyright (C) 2010 Sebastian Harl
// SPDX-FileContributor: Håko J Dugstad Johnsen <hakon-dugstad.johnsen at telenor.com>n
// SPDX-FileContributor: Sebastian "tokkee" Harl <sh at tokkee.org>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <assert.h>
#include <errno.h>

#ifdef NAN_STATIC_DEFAULT
#include <math.h>
#elif defined(NAN_STATIC_ISOC)
#ifndef __USE_ISOC99
#define DISABLE_ISOC99 1
#define __USE_ISOC99 1
#endif
#include <math.h>
#ifdef DISABLE_ISOC99
#undef DISABLE_ISOC99
#undef __USE_ISOC99
#endif
/* #endif NAN_STATIC_ISOC */
#elif NAN_ZERO_ZERO
#include <math.h>
#ifdef NAN
#undef NAN
#endif
#define NAN (0.0 / 0.0)
#ifndef isnan
#define isnan(f) ((f) != (f))
#endif /* !defined(isnan) */
#ifndef isfinite
#define isfinite(f) (((f) - (f)) == 0.0)
#endif
#ifndef isinf
#define isinf(f) (!isfinite(f) && !isnan(f))
#endif
#endif /* NAN_ZERO_ZERO */

#ifdef HAVE_GETOPT_H
#    include <getopt.h>
#elif !defined(HAVE_GETOPT_LONG)
#    include "libutils/getopt_long.h"
#endif

#include <sys/socket.h>
#include <sys/un.h>
#include <stdint.h>
#include <stdbool.h>

#include "libmetric/notification.h"
#include "libmdb/mdb.h"
#include "ncollectdctl/client.h"

#define STATIC_ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

#ifndef PREFIX
#define PREFIX "/opt/" PACKAGE_NAME
#endif

#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR PREFIX "/var"
#endif

#define DEFAULT_SOCK LOCALSTATEDIR "/run/" PACKAGE_NAME "-unixsock"

typedef enum {
    OUTPUT_FORMAT_TXT,
    OUTPUT_FORMAT_JSON,
    OUTPUT_FORMAT_JSON_PRETTY,
    OUTPUT_FORMAT_YAML,
    OUTPUT_FORMAT_TABLE
} output_format_t;

typedef enum {
    CMD_PLUGINS_READERS,
    CMD_PLUGINS_WRITERS,
    CMD_PLUGINS_LOGGERS,
    CMD_PLUGINS_NOTIFICATORS,
} cmd_plugins_t;

#if 0
/* _ssnprintf returns result from vsnprintf (consistent with snprintf) */
static int _ssnprintf(char *str, size_t sz, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    int ret = vsnprintf(str, sz, format, ap);

    va_end(ap);

    return ret;
}
#endif

#if 0
            if (strcasecmp(key, "interval") == 0) {
                char *endptr;

                vl.interval = (double)strtol(value, &endptr, 0);

                if (endptr == value) {
                    fprintf(stderr, "ERROR: Failed to parse interval as number: %s.\n",
                                    value);
                    return -1;
                } else if ((endptr != NULL) && (*endptr != '\0')) {
                    fprintf(stderr,
                                    "WARNING: Ignoring trailing garbage after "
                                    "interval: %s.\n",
                                    endptr);
                }
#endif

static output_format_t output_format = OUTPUT_FORMAT_TXT;
static table_style_type_t table_style = TABLE_STYLE_SIMPLE;
static char *unix_socket = NULL;
static char *program_name;

extern char *optarg;
extern int optind;


#ifndef LOG_ERR
#define LOG_ERR 3
#endif
#ifndef LOG_WARNING
#define LOG_WARNING 4
#endif
#ifndef LOG_NOTICE
#define LOG_NOTICE 5
#endif
#ifndef LOG_INFO
#define LOG_INFO 6
#endif
#ifndef LOG_DEBUG
#define LOG_DEBUG 7
#endif


cdtime_t plugin_get_interval(void)
{
    return 0;
}

void daemon_log(int level, const char *file, int line, const char *func, const char *format, ...)
{
    char msg[1024];

    va_list ap;
    va_start(ap, format);
    vsnprintf(msg, sizeof(msg), format, ap);
    msg[sizeof(msg) - 1] = '\0';
    va_end(ap);

    char *slevel="";
    if (level == LOG_ERR) {
        slevel = "ERROR: ";
    } else if (level == LOG_WARNING) {
        slevel = "WARNING: ";
    } else if (level == LOG_NOTICE) {
        slevel = "NOTICE: ";
    } else if (level == LOG_INFO) {
        slevel = "INFO: ";
    } else if (level == LOG_DEBUG) {
        slevel = "DEBUG: ";
    }

    fprintf(stderr, "%s%s(%s:%d): %s\n", slevel, func, file, line, msg);
}

void plugin_log(int level, const char *file, int line, const char *func, const char *format, ...)
{
    char msg[1024];

    va_list ap;
    va_start(ap, format);
    vsnprintf(msg, sizeof(msg), format, ap);
    msg[sizeof(msg) - 1] = '\0';
    va_end(ap);

    char *slevel="";
    if (level == LOG_ERR) {
        slevel = "ERROR: ";
    } else if (level == LOG_WARNING) {
        slevel = "WARNING: ";
    } else if (level == LOG_NOTICE) {
        slevel = "NOTICE: ";
    } else if (level == LOG_INFO) {
        slevel = "INFO: ";
    } else if (level == LOG_DEBUG) {
        slevel = "DEBUG: ";
    }

    fprintf(stderr, "%s%s(%s:%d): %s\n", slevel, func, file, line, msg);
}

static int set_output_format(char *format)
{
    if (strcasecmp("json", format) == 0) {
        output_format = OUTPUT_FORMAT_JSON;
    } else if (strcasecmp("json-pretty", format) == 0) {
        output_format = OUTPUT_FORMAT_JSON_PRETTY;
    } else if (strcasecmp("yaml", format) == 0) {
        output_format = OUTPUT_FORMAT_YAML;
    } else if (strcasecmp("text", format) == 0) {
        output_format = OUTPUT_FORMAT_TXT;
    } else if (strcasecmp("txt", format) == 0) {
        output_format = OUTPUT_FORMAT_TXT;
    } else if (strcasecmp("table", format) == 0) {
        output_format = OUTPUT_FORMAT_TABLE;
        table_style = TABLE_STYLE_SIMPLE;
    } else if (strcasecmp("table-bold", format) == 0) {
        output_format = OUTPUT_FORMAT_TABLE;
        table_style = TABLE_STYLE_BOLD;
    } else if (strcasecmp("table-border-bold", format) == 0) {
        output_format = OUTPUT_FORMAT_TABLE;
        table_style = TABLE_STYLE_BORDER_BOLD;
    } else if (strcasecmp("table-double", format) == 0) {
        output_format = OUTPUT_FORMAT_TABLE;
        table_style = TABLE_STYLE_DOUBLE;
    } else if (strcasecmp("table-border-double", format) == 0) {
        output_format = OUTPUT_FORMAT_TABLE;
        table_style = TABLE_STYLE_BORDER_DOUBLE;
    } else if (strcasecmp("table-round", format) == 0) {
        output_format = OUTPUT_FORMAT_TABLE;
        table_style = TABLE_STYLE_ROUND;
    } else if (strcasecmp("table-ascii", format) == 0) {
        output_format = OUTPUT_FORMAT_TABLE;
        table_style = TABLE_STYLE_ASCII;
    } else {
        fprintf(stderr, "Unknow output format: \"%s\"\n"
                        "Must be one of:\n"
                        "    json\n    json-pretty\n    yaml\n    text\n    table\n"
                        "    table-bold\n    table-border-bold\n    table-double\n"
                        "    table-border-double\n    table-round\n    table-ascii\n", format);
        return -1;
    }
    return 0;
}

static int set_unix_socket(char *format)
{
    free(unix_socket);
    unix_socket = strdup(format);
    return 0;
}

static int cmd_plugins(cmd_plugins_t kind, int argc, char **argv)
{
    char *plugin_name = NULL;
    switch (kind) {
    case CMD_PLUGINS_READERS:
        plugin_name = "readers";
        break;
    case CMD_PLUGINS_WRITERS:
        plugin_name = "writers";
        break;
    case CMD_PLUGINS_LOGGERS:
        plugin_name = "loggers";
        break;
    case CMD_PLUGINS_NOTIFICATORS:
        plugin_name = "notificators";
        break;
    }

    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            {"unix-socket", required_argument, 0, 'u'},
            {"output",      required_argument, 0, 'o'},
            {"help",        no_argument,       0, 'h'},
            {0,             0,                 0,  0 }
        };

        int status = 0;

        int c = getopt_long(argc, argv, "u:o:h", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'u':
            status = set_unix_socket(optarg);
            break;
        case 'o':
            status = set_output_format(optarg);
            break;
        case 'h':
            fprintf(stderr, "Usage: %s %s [OPTION]\n\n"
                            "List plugins with the read callback.\n\n"
                            "Available options:\n\n"
                            "  -u, --unix-socket=PATH \n"
                            "  -o, --output=FORMAT \n"
                            "  -h, --help\n", program_name, plugin_name);
            return 0;
            break;
        }

        if (status != 0)
            return -1;
    }

    client_t *client = client_create(unix_socket);
    if (client == NULL) {
        return -1;
    }

    strlist_t *list = NULL;

    switch (kind){
    case CMD_PLUGINS_READERS:
        list = client_get_plugins_readers(client);
        break;
    case CMD_PLUGINS_WRITERS:
        list = client_get_plugins_writers(client);
        break;
    case CMD_PLUGINS_LOGGERS:
        list = client_get_plugins_loggers(client);
        break;
    case CMD_PLUGINS_NOTIFICATORS:
        list = client_get_plugins_notificators(client);
        break;
    }

    if (list == NULL) {
        client_destroy(client);
        return -1;
    }

    strbuf_t buf = STRBUF_CREATE;

    int status = 0;

    switch (output_format) {
    case OUTPUT_FORMAT_TXT:
        status = mdb_strlist_to_text(list, &buf);
        break;
    case OUTPUT_FORMAT_JSON:
        status = mdb_strlist_to_json(list, &buf, false);
        break;
    case OUTPUT_FORMAT_JSON_PRETTY:
        status = mdb_strlist_to_json(list, &buf, true);
        break;
    case OUTPUT_FORMAT_YAML:
        status = mdb_strlist_to_yaml(list, &buf);
        break;
    case OUTPUT_FORMAT_TABLE:
        switch (kind){
        case CMD_PLUGINS_READERS:
            status = mdb_strlist_to_table(list, table_style, &buf, "READERS");
            break;
        case CMD_PLUGINS_WRITERS:
            status = mdb_strlist_to_table(list, table_style, &buf, "WRITERS");
            break;
        case CMD_PLUGINS_LOGGERS:
            status = mdb_strlist_to_table(list, table_style, &buf, "LOGGERS");
            break;
        case CMD_PLUGINS_NOTIFICATORS:
            status = mdb_strlist_to_table(list, table_style, &buf, "NOTIFICATORS");
            break;
        }
        break;
    }

    if (status != 0) {
        strlist_free(list);
        client_destroy(client);
        strbuf_destroy(&buf);
        return -1;
    }

    size_t ret = fwrite(buf.ptr, 1, strbuf_len(&buf), stdout);
    if (ret != strbuf_len(&buf)) {
    }

    strlist_free(list);
    client_destroy(client);
    strbuf_destroy(&buf);

    return 0;
}

static int cmd_readers(int argc, char **argv)
{
    return cmd_plugins(CMD_PLUGINS_READERS, argc, argv);
}

static int cmd_writers(int argc, char **argv)
{
    return cmd_plugins(CMD_PLUGINS_WRITERS, argc, argv);
}

static int cmd_loggers(int argc, char **argv)
{
    return cmd_plugins(CMD_PLUGINS_LOGGERS, argc, argv);
}

static int cmd_notifiers(int argc, char **argv)
{
    return cmd_plugins(CMD_PLUGINS_NOTIFICATORS, argc, argv);
}

static int cmd_write(int argc, char **argv)
{
    char *file_data = NULL;
    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            {"unix-socket", required_argument, 0, 'u'},
            {"data",        required_argument, 0, 'd'},
            {"help",        no_argument,       0, 'h'},
            {0,             0,                 0,  0 }
        };

        int status = 0;

        int c = getopt_long(argc, argv, "u:d:h", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'u':
            status = set_unix_socket(optarg);
            break;
        case 'd':
            file_data = optarg;
            break;
        case 'h':
            fprintf(stderr, "Usage: %s write [OPTION]\n\n"
                            "Send metrics to the daemon.\n\n"
                            "Available options:\n\n"
                            "  -u, --unix-socket=PATH\n"
                            "  -d, --data=FILENAME\n"
                            "  -h, --help\n", program_name);
            return 0;
            break;
        }

        if (status != 0)
            return -1;
    }

    strbuf_t buf = STRBUF_CREATE;

    FILE *fp = stdin;
    if (file_data != NULL) {
        fp = fopen(file_data, "r");
        if (fp == NULL) {
            fprintf(stderr, "Cannot open file: '%s'.\n", file_data);
            return -1;
        }
    }

    char buffer[8192];
    while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) {
        strbuf_putstr(&buf, buffer);
    }

    if (fp != stdin)
        fclose(fp);

    client_t *client = client_create(unix_socket);
    if (client == NULL) {
        return -1;
    }

    int status = client_post_write(client, buf.ptr, strbuf_len(&buf));
    if (status != 0) {

    }

    client_destroy(client);
    strbuf_destroy(&buf);

    return 0;
}

static int  cmd_read(__attribute__((unused)) int argc, __attribute__((unused)) char **argv)
{
    while (true) {
        int option_index = 0;
// the consolidation function that is applied to the data you want to fetch AVERAGE,MIN,MAX,LAST
//   int64 step_ms = 1;  // Query step size in milliseconds.
//   int64 start_ms = 3; // Start time in milliseconds.
//   int64 end_ms = 4;   // End time in milliseconds.
//
//   string func = 2;    // String representation of surrounding function or aggregation.
//   repeated string grouping = 5; // List of label names used in aggregation.
//   bool by = 6; // Indicate whether it is without or by.
//   int64 range_ms = 7; // Range vector selector range in milliseconds.

        static struct option long_options[] = {
            {"unix-socket", required_argument, 0, 'u'},
            {"start",       required_argument, 0, 's'},
            {"end",         required_argument, 0, 'e'},
            {"resolution",  required_argument, 0, 'r'},
            {"help",        no_argument,       0, 'h'},
            {0,             0,                 0,  0 }
        };

        int status = 0;

        int c = getopt_long(argc, argv, "u:s:e:r:h", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'u':
            status = set_unix_socket(optarg);
            break;
        case 'o':
            printf("option o with value '%s'\n", optarg);
            status = set_output_format(optarg);
            break;
        case 'h':
            fprintf(stderr, "Usage: %s write [OPTIONS]\n\n"
                            "Send metrics to the daemon.\n\n"
                            "Available options:\n\n"
                            "  -u, --unix-socket=PATH \n"
                            "  -d, --data=FORMAT \n"
                            "  -h, --help\n", program_name);
            return 0;
            break;
        }

        if (status != 0)
            return -1;
    }
// FIXME
    return 0;
}

static int cmd_notification_append_label(label_set_t *set, char *arg)
{
    char *label = arg;
    char *eq = strchr(arg, '=');
    if (eq == NULL)
        return -1;

    size_t label_size = eq - label;
    char *value = eq + 1;
    size_t value_size = strlen(value);

    return _label_set_add(set, true, false, label, label_size, value, value_size);
}

static int cmd_notification(int argc, char **argv)
{
    notification_t n = {0};

    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            {"unix-socket", required_argument, 0, 'u'},
            {"name",        required_argument, 0, 'n'},
            {"label",       required_argument, 0, 'l'},
            {"annotation",  required_argument, 0, 'a'},
            {"severity",    required_argument, 0, 's'},
            {"time",        required_argument, 0, 't'},
            {"help",        no_argument,       0, 'h'},
            {0,             0,                 0,  0 }
        };

        int status = 0;

        int c = getopt_long(argc, argv, "u:n:l:a:s:t:h", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'u':
            status = set_unix_socket(optarg);
            if (status < 0) {
            }
            break;
        case 'n':
            if (n.name != NULL)
                free(n.name);
            n.name = strdup(optarg);
            //if (n->name != NULL)
            break;
        case 'l':
            status = cmd_notification_append_label(&n.label, optarg);
            if (status < 0) {
            }
            break;
        case 'a':
            status = cmd_notification_append_label(&n.annotation, optarg);
            if (status < 0) {
            }
            break;
        case 's':
            if (strcasecmp(optarg, "OKAY") == 0) {
                n.severity = NOTIF_OKAY;
            } else if (strcasecmp(optarg, "WARNING") == 0) {
                n.severity = NOTIF_WARNING;
            } else if (strcasecmp(optarg, "FAILURE") == 0) {
                n.severity = NOTIF_FAILURE;
            } else {
                //
            }
            break;
        case 't': {
            double timestamp = atof(optarg);
            n.time = DOUBLE_TO_CDTIME_T(timestamp);
        }   break;
        case 'h':
            fprintf(stderr, "Usage: %s notification [OPTIONS]\n\n"
                            "Send notification to the daemon.\n\n"
                            "Available options:\n\n"
                            "  -u, --unix-socket=PATH\n"
                            "  -n, --name=NAME\n"
                            "  -l, --label=KEY=VALUE\n"
                            "  -a, --annotation=KEY=VALUE\n"
                            "  -s, --severity=OKAY|WARNING|FAILURE\n"
                            "  -t, --time=TIMESTAMP"
                            "  -h, --help\n", program_name);
            break;
        }
    }

    client_t *client = client_create(unix_socket);
    if (client == NULL) {
        notification_reset(&n);
        return -1;
    }

    if (n.time == 0)
        n.time = cdtime();

    int status = client_post_notification(client, &n);
    if (status != 0) {

    }

    notification_reset(&n);
    client_destroy(client);
    return 0;
}

static int cmd_query(int argc, char **argv)
{
    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            {"unix-socket", required_argument, 0, 'u'},
            {"output",      required_argument, 0, 'o'},
            {"time",        required_argument, 0, 't'},
            {"help",        no_argument,       0, 'h'},
            {0,             0,                 0,  0 }
        };

        int c = getopt_long(argc, argv, "t:h", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 't':
            printf("option t with value '%s'\n", optarg);
            break;
        case 'h':
            break;
        }
    }
// FIXME
    return 0;
}

static int cmd_query_range(int argc, char **argv)
{
    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            {"unix-socket", required_argument, 0, 'u'},
            {"output",      required_argument, 0, 'o'},
            {"start",       required_argument, 0, 's'},
            {"end",         required_argument, 0, 'e'},
            {"step",        required_argument, 0, 'S'},
            {"help",        no_argument,       0, 'h'},
            {0,             0,                 0,  0 }
        };

        int status = 0;

        int c = getopt_long(argc, argv, "s:e:S:h", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'u':
            status = set_unix_socket(optarg);
            break;
        case 'm':
            break;
        case 'o':
            printf("option o with value '%s'\n", optarg);
            status = set_output_format(optarg);
            break;
        case 's':
            printf("option s with value '%s'\n", optarg);
            break;
        case 'e':
            printf("option e with value '%s'\n", optarg);
            break;
        case 'S':
            printf("option S with value '%s'\n", optarg);
            break;
        case 'h':
            break;
        }

        if (status != 0)
            return -1;
    }

    if (optind >= argc) {
        fprintf(stderr, "%s: missing query\n", argv[0]);
        //exit_usage(argv[0], 1);
    }

// FIXME
    return 0;
}

static int cmd_series(int argc, char **argv)
{
    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            {"unix-socket", required_argument, 0, 'u'},
            {"match",       required_argument, 0, 'm'},
            {"output",      required_argument, 0, 'o'},
            {"help",        no_argument,       0, 'h'},
            {0,             0,                 0,  0 }
        };

        int status = 0;

        int c = getopt_long(argc, argv, "u:m:o:h", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'u':
            status = set_unix_socket(optarg);
            break;
        case 'm':
            break;
        case 'o':
            status = set_output_format(optarg);
            break;
        case 'h':
            fprintf(stderr, "Usage: %s write [OPTION]\n\n"
                            "Show series.\n\n"
                            "Available options:\n\n"
                            "  -u, --unix-socket=PATH \n"
                            "  -m, --match=MATCH \n"
                            "  -o, --output=FORMAT \n"
                            "  -h, --help\n", program_name);
            return 0;
            break;
        }

        if (status != 0)
            return -1;
    }

    client_t *client = client_create(unix_socket);
    if (client == NULL) {
        return -1;
    }

    mdb_series_list_t *list = client_get_series(client);
    if (list == NULL) {
        client_destroy(client);
        return -1;
    }

    strbuf_t buf = STRBUF_CREATE;

    int status = 0;

    switch (output_format) {
    case OUTPUT_FORMAT_TXT:
        status = mdb_series_list_to_text(list, &buf);
        break;
    case OUTPUT_FORMAT_JSON:
        status = mdb_series_list_to_json(list, &buf, false);
        break;
    case OUTPUT_FORMAT_JSON_PRETTY:
        status = mdb_series_list_to_json(list, &buf, true);
        break;
    case OUTPUT_FORMAT_YAML:
        status = mdb_series_list_to_yaml(list, &buf);
        break;
    case OUTPUT_FORMAT_TABLE:
        status = mdb_series_list_to_table(list, table_style, &buf);
        break;
    }

    if (status != 0) {
        mdb_series_list_free(list);
        client_destroy(client);
        strbuf_destroy(&buf);
        return -1;
    }

    size_t ret = fwrite(buf.ptr, 1, strbuf_len(&buf), stdout);
    if (ret != strbuf_len(&buf)) {
    }

    mdb_series_list_free(list);
    client_destroy(client);
    strbuf_destroy(&buf);

    return 0;
}

static int cmd_family_metrics(int argc, char **argv)
{
    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            {"unix-socket", required_argument, 0, 'u'},
            {"match",  required_argument, 0, 'm'},
            {"output", required_argument, 0, 'o'},
            {"help",   no_argument,       0, 'h'},
            {0,        0,                 0,  0 }
        };

        int status = 0;

        int c = getopt_long(argc, argv, "u:m:o:h", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'u':
            status = set_unix_socket(optarg);
            break;
        case 'm':
            printf("option m with value '%s'\n", optarg);
            break;
        case 'o':
            status = set_output_format(optarg);
            break;
        case 'h':
            fprintf(stderr, "Usage: %s write [OPTION]\n\n"
                            "Show family metrics.\n\n"
                            "Available options:\n\n"
                            "  -u, --unix-socket=PATH \n"
                            "  -m, --match=MATCH \n"
                            "  -o, --output=FORMAT \n"
                            "  -h, --help\n", program_name);
            return 0;
            break;
        }

        if (status != 0)
            return -1;
    }

    client_t *client = client_create(unix_socket);
    if (client == NULL) {
        return -1;
    }

    mdb_family_metric_list_t *list = client_get_family_metrics(client);
    if (list == NULL) {
        client_destroy(client);
        return -1;
    }

    strbuf_t buf = STRBUF_CREATE;

    int status = 0;

    switch (output_format) {
    case OUTPUT_FORMAT_TXT:
        status = mdb_family_metric_list_to_text(list, &buf);
        break;
    case OUTPUT_FORMAT_JSON:
        status = mdb_family_metric_list_to_json(list, &buf, false);
        break;
    case OUTPUT_FORMAT_JSON_PRETTY:
        status = mdb_family_metric_list_to_json(list, &buf, true);
        break;
    case OUTPUT_FORMAT_YAML:
        status = mdb_family_metric_list_to_yaml(list, &buf);
        break;
    case OUTPUT_FORMAT_TABLE:
        status = mdb_family_metric_list_to_table(list, table_style, &buf);
        break;
    }

    if (status != 0) {
        mdb_family_metric_list_free(list);
        client_destroy(client);
        strbuf_destroy(&buf);
        return -1;
    }

    size_t ret = fwrite(buf.ptr, 1, strbuf_len(&buf), stdout);
    if (ret != strbuf_len(&buf)) {
    }

    mdb_family_metric_list_free(list);
    client_destroy(client);
    strbuf_destroy(&buf);

    return 0;
}

static int cmd_metrics(int argc, char **argv)
{
    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            {"unix-socket", required_argument, 0, 'u'},
            {"output",      required_argument, 0, 'o'},
            {"help",        no_argument,       0, 'h'},
            {0,             0,                 0,  0 }
        };

        int status = 0;

        int c = getopt_long(argc, argv, "u:o:h", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'u':
            status = set_unix_socket(optarg);
            break;
        case 'o':
            status = set_output_format(optarg);
            break;
        case 'h':
            fprintf(stderr, "Usage: %s labels [OPTION] <metric>\n\n"
                            "Show metrics.\n\n"
                            "Available options:\n\n"
                            "  -u, --unix-socket=PATH \n"
                            "  -o, --output=FORMAT \n"
                            "  -h, --help\n", program_name);
            break;
        }

        if (status != 0)
            return -1;
    }

    client_t *client = client_create(unix_socket);
    if (client == NULL) {
        return -1;
    }

    strlist_t *list = client_get_metrics(client);
    if (list == NULL) {
        client_destroy(client);
        return -1;
    }

    strbuf_t buf = STRBUF_CREATE;

    int status = 0;

    switch (output_format) {
    case OUTPUT_FORMAT_TXT:
        status = mdb_strlist_to_text(list, &buf);
        break;
    case OUTPUT_FORMAT_JSON:
        status = mdb_strlist_to_json(list, &buf, false);
        break;
    case OUTPUT_FORMAT_JSON_PRETTY:
        status = mdb_strlist_to_json(list, &buf, true);
        break;
    case OUTPUT_FORMAT_YAML:
        status = mdb_strlist_to_yaml(list, &buf);
        break;
    case OUTPUT_FORMAT_TABLE:
        status = mdb_strlist_to_table(list, table_style, &buf, "METRICS");
        break;
    }

    if (status != 0) {
        strlist_free(list);
        client_destroy(client);
        strbuf_destroy(&buf);
        return -1;
    }

    size_t ret = fwrite(buf.ptr, 1, strbuf_len(&buf), stdout);
    if (ret != strbuf_len(&buf)) {
    }

    strlist_free(list);
    client_destroy(client);
    strbuf_destroy(&buf);

    return 0;
}

static int cmd_labels(int argc, char **argv)
{
    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            {"unix-socket", required_argument, 0, 'u'},
            {"output",      required_argument, 0, 'o'},
            {"help",        no_argument,       0, 'h'},
            {0,             0,                 0,  0 }
        };

        int status = 0;

        int c = getopt_long(argc, argv, "u:o:h", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'u':
            status = set_unix_socket(optarg);
            break;
        case 'o':
            status = set_output_format(optarg);
            break;
        case 'h':
            fprintf(stderr, "Usage: %s labels [OPTION] <metric>\n\n"
                            "Show labels used by metric.\n\n"
                            "Available options:\n\n"
                            "  -u, --unix-socket=PATH \n"
                            "  -o, --output=FORMAT \n"
                            "  -h, --help\n", program_name);
            break;
        }

        if (status != 0)
            return -1;
    }

    if ((optind + 1) != argc) {
        fprintf(stderr, "%s: missing metric name\n", argv[0]);
        return -1;
    }
    // FIXME
    char *metric = argv[optind];

    client_t *client = client_create(unix_socket);
    if (client == NULL) {
        return -1;
    }

    strlist_t *list = client_get_metric_labels(client, metric);
    if (list == NULL) {
        client_destroy(client);
        return -1;
    }

    strbuf_t buf = STRBUF_CREATE;

    int status = 0;

    switch (output_format) {
    case OUTPUT_FORMAT_TXT:
        status = mdb_strlist_to_text(list, &buf);
        break;
    case OUTPUT_FORMAT_JSON:
        status = mdb_strlist_to_json(list, &buf, false);
        break;
    case OUTPUT_FORMAT_JSON_PRETTY:
        status = mdb_strlist_to_json(list, &buf, true);
        break;
    case OUTPUT_FORMAT_YAML:
        status = mdb_strlist_to_yaml(list, &buf);
        break;
    case OUTPUT_FORMAT_TABLE:
        status = mdb_strlist_to_table(list, table_style, &buf, "LABELS");
        break;
    }

    if (status != 0) {
        strlist_free(list);
        client_destroy(client);
        strbuf_destroy(&buf);
        return -1;
    }

    size_t ret = fwrite(buf.ptr, 1, strbuf_len(&buf), stdout);
    if (ret != strbuf_len(&buf)) {
    }

    strlist_free(list);
    client_destroy(client);
    strbuf_destroy(&buf);

    return 0;
}

static int cmd_label_values(int argc, char **argv)
{
    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            {"unix-socket", required_argument, 0, 'u'},
            {"output",      required_argument, 0, 'o'},
            {"help",        no_argument,       0, 'h'},
            {0,             0,                 0,  0 }
        };

        int status = 0;

        int c = getopt_long(argc, argv, "o:h", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'u':
            status = set_unix_socket(optarg);
            break;
        case 'o':
            status = set_output_format(optarg);
            break;
        case 'h':
            fprintf(stderr, "Usage: %s label-values [OPTION] <metric> <label>\n\n"
                            "Show label values of metric label.\n\n"
                            "Available options:\n\n"
                            "  -u, --unix-socket=PATH \n"
                            "  -o, --output=FORMAT \n"
                            "  -h, --help\n", program_name);
            break;
        }

        if (status != 0)
            return -1;
    }

    if ((optind + 2) != argc) {
        fprintf(stderr, "%s: missing metric name\n", argv[0]);
        return -1;
    }
    // FIXME
    char *metric = argv[optind];
    char *label = argv[optind+1];

    client_t *client = client_create(unix_socket);
    if (client == NULL) {
        return -1;
    }

    strlist_t *list = client_get_metric_label_values(client, metric, label);
    if (list == NULL) {
        client_destroy(client);
        return -1;
    }

    strbuf_t buf = STRBUF_CREATE;

    int status = 0;

    switch (output_format) {
    case OUTPUT_FORMAT_TXT:
        status = mdb_strlist_to_text(list, &buf);
        break;
    case OUTPUT_FORMAT_JSON:
        status = mdb_strlist_to_json(list, &buf, false);
        break;
    case OUTPUT_FORMAT_JSON_PRETTY:
        status = mdb_strlist_to_json(list, &buf, true);
        break;
    case OUTPUT_FORMAT_YAML:
        status = mdb_strlist_to_yaml(list, &buf);
        break;
    case OUTPUT_FORMAT_TABLE:
        status = mdb_strlist_to_table(list, table_style, &buf, label);
        break;
    }

    if (status != 0) {
        strlist_free(list);
        client_destroy(client);
        strbuf_destroy(&buf);
        return -1;
    }

    size_t ret = fwrite(buf.ptr, 1, strbuf_len(&buf), stdout);
    if (ret != strbuf_len(&buf)) {

    }

    strlist_free(list);
    client_destroy(client);
    strbuf_destroy(&buf);

    return 0;
}

#if 0

    [-s|--start time]
    [-e|--end time]
    [-S|--step seconds]

int graph_ts_title(graph_ts_t *g, const char *title)
int graph_ts_vertical_label(graph_ts_t *g, const char *vlabel)
int graph_ts_right_axis(graph_ts_t *g, const char *
int graph_ts_right_axis_scale(graph_ts_t *g, double scale);
int graph_ts_right_axis_shift(graph_ts_t *g, double shift);
int graph_ts_right_axis_label(graph_ts_t *g, const char *label);
int graph_ts_right_axis_format(graph_ts_t *g, const char *fmt);
int graph_ts_right_axis_formatter(graph_ts_t *g, graph_axis_formatter_t fmt);
int graph_ts_width(graph_ts_t *g, size_t width);
int graph_ts_height(graph_ts_t *g, size_t height);
int graph_ts_only_graph(graph_ts_t *g);
int graph_ts_full_size_mode(graph_ts_t *g);
int graph_ts_upper_limit(graph_ts_t *g, double upper);
int graph_ts_lower_limit(graph_ts_t *g, double lower);
int graph_ts_rigid(graph_ts_t *g);
int graph_ts_allow_shrink(graph_ts_t *g);
int graph_ts_alt_autoscale(graph_ts_t *g);
int graph_ts_alt_autoscale_min(graph_ts_t *g);
int graph_ts_alt_autoscale_max(graph_ts_t *g);
int graph_ts_no_gridfit(graph_ts_t *g);
int graph_ts_x_grid(graph_ts_t *g, const char *gtm, const char *gst, const char *mtm, const char *mst, const char *ltm, const char *lst, const char *lpr, const char *lfm);
int graph_ts_week_fmt(graph_ts_t *g, const char *fmt);
int graph_ts_x_grid(graph_ts_t *g,

    [-y|--y-grid grid step:label factor]
    [-y|--y-grid none]

typedef enum {
    GRAPH_AXIS_FORMATTER_NUMERIC,
    GRAPH_AXIS_FORMATTER_TIMESTAMP,
    GRAPH_AXIS_FORMATTER_DURATION,
} graph_axis_formatter_t;

int graph_ts_left_axis_formatter(graph_ts_t *g, graph_axis_formatter_t fmt);

int graph_ts_alt_y_grid(graph_ts_t *g)
int graph_ts_logarithmic(graph_ts_t *g)
int graph_ts_units_exponent(graph_ts_t *g, double value);
int graph_ts_units_length(graph_ts_t *g, double value);

typedef enum {
    GRAPH_UNITS_SI,
} graph_units_t;

int graph_ts_units(graph_ts_t *g, graph_units_t units);

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} graph_color_t;

typedef enum {
    GRAPH_COLOR_,
} graph_colot_tag_t;

int graph_ts_color(graph_ts_t *g, graph_colot_tag_t tag, graph_color_t color);

int graph_ts_grid_dash(graph_ts_t *g, double on, double off);
int graph_ts_border(graph_ts_t *g, int width);
int graph_ts_dynamic_labels(graph_ts_t *g);

typedef enum {

} graph_font_tag_t;

graph_ts_font(graph_ts_t *g, graph_font_tag_t tag, double size, const char *font);


graph_ts_zoom(graph_ts_t *g, double factor);

    [-R|--font-render-mode {normal,light,mono}]
    [-B|--font-smoothing-threshold size]
    [-G|--graph-render-mode {normal,mono}]
    [-E|--slope-mode]
graph_ts_slope_mode(graph_ts_t *g);

    [-a|--imgformat PNG | SVG | SIXEL | PDF ]

graph_ts_no_legend(graph_ts_t *g);

typedef enum {
    GRAPH_LEGEND_POSITION_NORTH,
    GRAPH_LEGEND_POSITION_SOUTH,
    GRAPH_LEGEND_POSITION_WEST,
    GRAPH_LEGEND_POSITION_EAST,
} graph_legend_position_t;
graph_legend_position(graph_t *g, graph_legend_position_t position);

typedef enum {
    GRAPH_LEGEND_DIRECTION_TOPDOWN,
    GRAPH_LEGEND_DIRECTION_BOTTOMUP,
    GRAPH_LEGEND_DIRECTION_BOTTOMUP2,
} graph_legend_direction_t;
graph_legend_direction(graph_t *g, graph_legend_direction_t direction);

graph_force_rules_legend(graph_t *g);
graph_tab_width(graph_t *g, ssize_t width);
graph_base(graph_t *g, ssize_t base);
graph_watermark(graph_t *g, const char *watermark);



//  graph
//        PRINT:vname:format[:strftime|:valstrftime|:valstrfduration]
//        GPRINT:vname:format
//        COMMENT:text
//        VRULE:time#color[:[legend][:dashes[=on_s[,off_s[,on_s,off_s]...]][:dash-offset=offset]]]
//        HRULE:value#color[:[legend][:dashes[=on_s[,off_s[,on_s,off_s]...]][:dash-offset=offset]]]
//        LINE[width]:value[#color][:[legend][:STACK][:skipscale][:dashes[=on_s[,off_s[,on_s,off_s]...]][:dash-offset=offset]]]
//        AREA:value[#color][:[legend][:STACK][:skipscale]]
//        TICK:vname#rrggbb[aa][:fraction[:legend]]
//        SHIFT:vname:offset
//        TEXTALIGN:{left|right|justified|center}
#endif

static int cmd_graph(int argc, char **argv)
{
    while (true) {
        int option_index = 0;
        static struct option long_options[] = {
            {"start",                    required_argument, 0, 's'},
            {"end",                      required_argument, 0, 'e'},
            {"step",                     required_argument, 0, 'S'},
            {"title",                    required_argument, 0, 't'},
            {"vertical-label",           required_argument, 0, 'v'},
            {"right-axis",               required_argument, 0, 128},
            {"right-axis-label",         required_argument, 0, 129},
            {"right-axis-formatter",     required_argument, 0, 130},
            {"right-axis-format",        required_argument, 0, 131},
            {"width",                    required_argument, 0, 'w'},
            {"height",                   required_argument, 0, 'h'},
            {"only-graph",               no_argument,       0, 'j'},
            {"full-size-mode",           no_argument,       0, 'D'},
            {"upper-limit",              required_argument, 0, 'u'},
            {"lower-limit",              required_argument, 0, 'l'},
            {"rigid",                    no_argument,       0, 'r'},
            {"allow-shrink",             no_argument,       0, 132},
            {"alt-autoscale",            no_argument,       0, 'A'},
            {"alt-autoscale-min",        no_argument,       0, 'J'},
            {"alt-autoscale-max",        no_argument,       0, 'M'},
            {"no-gridfit",               no_argument,       0, 'N'},
            {"x-grid",                   required_argument, 0, 'x'},
            {"week-fmt",                 required_argument, 0, 133},
            {"y-grid",                   required_argument, 0, 'y'},
            {"left-axis-formatter",      required_argument, 0, 134},
            {"alt-y-grid",               required_argument, 0, 'Y'},
            {"logarithmic",              no_argument,       0, 'o'},
            {"units-exponent",           required_argument, 0, 'X'},
            {"units-length",             required_argument, 0, 'L'},
            {"units",                    required_argument, 0, 135},
            {"color",                    required_argument, 0, 'c'},
            {"grid-dash",                required_argument, 0, 136},
            {"border",                   required_argument, 0, 137},
            {"dynamic-labels",           no_argument,       0, 138},
            {"font",                     required_argument, 0, 'n'},
            {"zoom",                     required_argument, 0, 139},
            {"font-render-mode",         required_argument, 0, 'R'},
            {"font-smoothing-threshold", required_argument, 0, 'B'},
            {"graph-render-mode",        required_argument, 0, 'G'},
            {"slope-mode",               required_argument, 0, 'E'},
            {"no-legend",                required_argument, 0, 'g'},
            {"legend-position",          required_argument, 0, 140},
            {"legend-direction",         required_argument, 0, 141},
            {"force-rules-legend",       no_argument,       0, 'F'},
            {"tabwidth",                 required_argument, 0, 'T'},
            {"base",                     required_argument, 0, 'b'},
            {"watermark",                required_argument, 0, 'W'},
            {"format",                   required_argument, 0, 'a'},
            {"line",                     required_argument, 0, 142},
            {"area",                     required_argument, 0, 143},
            {"vrule",                    required_argument, 0, 144},
            {"hrule",                    required_argument, 0, 145},
            {"tick",                     required_argument, 0, 146},
            {"help",                     no_argument,       0, 'H'},
            {0,                          0,                 0,  0 }
        };

        int c = getopt_long(argc, argv, "s:e:S:t:v:w:h:jDu:l:rAJMNx:y:Y:oX:L:c:n:R:B:G:E:g:FT:b:W:a:H", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 's':  //  "start",                    required_argument
            break;
        case 'e':  //  "end",                      required_argument
            break;
        case 'S':  //  "step",                     required_argument
            break;
        case 't':  //  "title",                    required_argument
            break;
        case 'v':  //  "vertical-label",           required_argument
            break;
        case 128:  //  "right-axis",               required_argument
            break;
        case 129:  //  "right-axis-label",         required_argument
            break;
        case 130:  //  "right-axis-formatter",     required_argument
            break;
        case 131:  //  "right-axis-format",        required_argument
            break;
        case 'w':  //  "width",                    required_argument
            break;
        case 'h':  //  "height",                   required_argument
            break;
        case 'j':  //  "only-graph",               no_argument
            break;
        case 'D':  //  "full-size-mode",           no_argument
            break;
        case 'u':  //  "upper-limit",              required_argument
            break;
        case 'l':  //  "lower-limit",              required_argument
            break;
        case 'r':  //  "rigid",                    no_argument
            break;
        case 132:  //  "allow-shrink",             no_argument
            break;
        case 'A':  //  "alt-autoscale",            no_argument
            break;
        case 'J':  //  "alt-autoscale-min",        no_argument
            break;
        case 'M':  //  "alt-autoscale-max",        no_argument
            break;
        case 'N':  //  "no-gridfit",               no_argument
            break;
        case 'x':  //  "x-grid",                   required_argument
            break;
        case 133:  //  "week-fmt",                 required_argument
            break;
        case 'y':  //  "y-grid",                   required_argument
            break;
        case 134:  //  "left-axis-formatter",      required_argument
            break;
        case 'Y':  //  "alt-y-grid",               required_argument
            break;
        case 'o':  //  "logarithmic",              no_argument
            break;
        case 'X':  //  "units-exponent",           required_argument
            break;
        case 'L':  //  "units-length",             required_argument
            break;
        case 135:  //  "units",                    required_argument
            break;
        case 'c':  //  "color",                    required_argument
            break;
        case 136:  //  "grid-dash",                required_argument
            break;
        case 137:  //  "border",                   required_argument
            break;
        case 138:  //  "dynamic-labels",           no_argument
            break;
        case 'n':  //  "font",                     required_argument
            break;
        case 139:  //  "zoom",                     required_argument
            break;
        case 'R':  //  "font-render-mode",         required_argument
            break;
        case 'B':  //  "font-smoothing-threshold", required_argument
            break;
        case 'G':  //  "graph-render-mode"         required_argument
            break;
        case 'E':  //  "slope-mode",               required_argument
            break;
        case 'g':  //  "no-legend",                required_argument
            break;
        case 140:  //  "legend-position",          required_argument
            break;
        case 141:  //  "legend-direction",         required_argument
            break;
        case 'F':  //  "force-rules-legend",       no_argument
            break;
        case 'T':  //  "tabwidth",                 required_argument
            break;
        case 'b':  //  "base",                     required_argument
            break;
        case 'W':  //  "watermark",                required_argument
            break;
        case 'a':  //  "format",                   required_argument
            break;
        case 142:  //  "line",                     required_argument
//      [width]:value[#color][:[legend][:STACK][:skipscale][:dashes[=on_s[,off_s[,on_s,off_s]...]][:dash-offset=offset]]]
            break;
        case 143:  //  "area",                    required_argument
//      value[#color][:[legend][:STACK][:skipscale]]
            break;
        case 144:  //  "vrule",                   required_argument
//      time#color[:[legend][:dashes[=on_s[,off_s[,on_s,off_s]...]][:dash-offset=offset]]]
            break;
        case 145:  //  "hrule",                   required_argument
//      valuf#color[:[legend][:dashes[=on_s[,off_s[,on_s,off_s]...]][:dash-offset=offset]]]
            break;
        case 146:  //  "tick",                    required_argument
//      vname#rrggbb[aa][:fraction[:legend]]
            break;
        case 'H':  //  "help"
            break;
        }
    }
    return 0;
}

static int cmd_template(int argc, char **argv)
{
    static struct option long_options[] = {
        {"start",                    required_argument, 0, 's'},
        {"end",                      required_argument, 0, 'e'},
        {"step",                     required_argument, 0, 'S'},
        {"title",                    required_argument, 0, 't'},
        {"vertical-label",           required_argument, 0, 'v'},
        {"right-axis",               required_argument, 0, 128},
        {"right-axis-label",         required_argument, 0, 129},
        {"right-axis-formatter",     required_argument, 0, 130},
        {"right-axis-format",        required_argument, 0, 131},
        {"width",                    required_argument, 0, 'w'},
        {"height",                   required_argument, 0, 'h'},
        {"only-graph",               no_argument,       0, 'j'},
        {"full-size-mode",           no_argument,       0, 'D'},
        {"upper-limit",              required_argument, 0, 'u'},
        {"lower-limit",              required_argument, 0, 'l'},
        {"rigid",                    no_argument,       0, 'r'},
        {"allow-shrink",             no_argument,       0, 132},
        {"alt-autoscale",            no_argument,       0, 'A'},
        {"alt-autoscale-min",        no_argument,       0, 'J'},
        {"alt-autoscale-max",        no_argument,       0, 'M'},
        {"no-gridfit",               no_argument,       0, 'N'},
        {"x-grid",                   required_argument, 0, 'x'},
        {"week-fmt",                 required_argument, 0, 133},
        {"y-grid",                   required_argument, 0, 'y'},
        {"left-axis-formatter",      required_argument, 0, 134},
        {"alt-y-grid",               required_argument, 0, 'Y'},
        {"logarithmic",              no_argument,       0, 'o'},
        {"units-exponent",           required_argument, 0, 'X'},
        {"units-length",             required_argument, 0, 'L'},
        {"units",                    required_argument, 0, 135},
        {"color",                    required_argument, 0, 'c'},
        {"grid-dash",                required_argument, 0, 136},
        {"border",                   required_argument, 0, 137},
        {"dynamic-labels",           no_argument,       0, 138},
        {"font",                     required_argument, 0, 'n'},
        {"zoom",                     required_argument, 0, 139},
        {"font-render-mode",         required_argument, 0, 'R'},
        {"font-smoothing-threshold", required_argument, 0, 'B'},
        {"graph-render-mode",        required_argument, 0, 'G'},
        {"slope-mode",               required_argument, 0, 'E'},
        {"no-legend",                required_argument, 0, 'g'},
        {"legend-position",          required_argument, 0, 140},
        {"legend-direction",         required_argument, 0, 141},
        {"force-rules-legend",       no_argument,       0, 'F'},
        {"tabwidth",                 required_argument, 0, 'T'},
        {"base",                     required_argument, 0, 'b'},
        {"watermark",                required_argument, 0, 'W'},
        {"format",                   required_argument, 0, 'a'},
        {"help",                     no_argument,       0, 'H'},
        {0,                          0,                 0,  0 }
    };
    static char *optstr = "s:e:S:t:v:w:h:jDu:l:rAJMNx:y:Y:oX:L:c:n:R:B:G:E:g:FT:b:W:a:H";

    while (true) {
        int option_index = 0;

        int c = getopt_long(argc, argv, optstr, long_options, &option_index);
fprintf(stderr, "getopt: %d %d %s\n", c, opterr, optarg);
        if (c == -1)
            break;
        if (c == '?')
            return -1;
    }

    if (optind >= argc) {

        return -1;
    }

    char *template = argv[optind];
fprintf(stderr, "template: %s\n", template);
    for (int i = optind + 1; i < argc ; i++) {
         fprintf(stderr, "template var: %s\n", argv[i]);
    }


    optind = 1;
    while (true) {
        int option_index = 0;

        int c = getopt_long(argc, argv, optstr, long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 's':  //  "start",                    required_argument
            break;
        case 'e':  //  "end",                      required_argument
            break;
        case 'S':  //  "step",                     required_argument
            break;
        case 't':  //  "title",                    required_argument
            break;
        case 'v':  //  "vertical-label",           required_argument
            break;
        case 128:  //  "right-axis",               required_argument
            break;
        case 129:  //  "right-axis-label",         required_argument
            break;
        case 130:  //  "right-axis-formatter",     required_argument
            break;
        case 131:  //  "right-axis-format",        required_argument
            break;
        case 'w':  //  "width",                    required_argument
            break;
        case 'h':  //  "height",                   required_argument
            break;
        case 'j':  //  "only-graph",               no_argument
            break;
        case 'D':  //  "full-size-mode",           no_argument
            break;
        case 'u':  //  "upper-limit",              required_argument
            break;
        case 'l':  //  "lower-limit",              required_argument
            break;
        case 'r':  //  "rigid",                    no_argument
            break;
        case 132:  //  "allow-shrink",             no_argument
            break;
        case 'A':  //  "alt-autoscale",            no_argument
            break;
        case 'J':  //  "alt-autoscale-min",        no_argument
            break;
        case 'M':  //  "alt-autoscale-max",        no_argument
            break;
        case 'N':  //  "no-gridfit",               no_argument
            break;
        case 'x':  //  "x-grid",                   required_argument
            break;
        case 133:  //  "week-fmt",                 required_argument
            break;
        case 'y':  //  "y-grid",                   required_argument
            break;
        case 134:  //  "left-axis-formatter",      required_argument
            break;
        case 'Y':  //  "alt-y-grid",               required_argument
            break;
        case 'o':  //  "logarithmic",              no_argument
            break;
        case 'X':  //  "units-exponent",           required_argument
            break;
        case 'L':  //  "units-length",             required_argument
            break;
        case 135:  //  "units",                    required_argument
            break;
        case 'c':  //  "color",                    required_argument
            break;
        case 136:  //  "grid-dash",                required_argument
            break;
        case 137:  //  "border",                   required_argument
            break;
        case 138:  //  "dynamic-labels",           no_argument
            break;
        case 'n':  //  "font",                     required_argument
            break;
        case 139:  //  "zoom",                     required_argument
            break;
        case 'R':  //  "font-render-mode",         required_argument
            break;
        case 'B':  //  "font-smoothing-threshold", required_argument
            break;
        case 'G':  //  "graph-render-mode"         required_argument
            break;
        case 'E':  //  "slope-mode",               required_argument
            break;
        case 'g':  //  "no-legend",                required_argument
            break;
        case 140:  //  "legend-position",          required_argument
            break;
        case 141:  //  "legend-direction",         required_argument
            break;
        case 'F':  //  "force-rules-legend",       no_argument
            break;
        case 'T':  //  "tabwidth",                 required_argument
            break;
        case 'b':  //  "base",                     required_argument
            break;
        case 'W':  //  "watermark",                required_argument
            break;
        case 'a':  //  "format",                   required_argument
            break;
        case 'H':  //  "help"
            break;
        }
    }

    return 0;
}

static int cmd_help(__attribute__((unused)) int argc, __attribute__((unused)) char **argv)
{
    return 0;
}

typedef struct {
    char *name;
    int (*cmd)(int, char **);
    char *help;
} cmd_t;

static cmd_t cmds[] = {
    { "readers",        cmd_readers,          "List plugins with the read callback."   },
    { "writers",        cmd_writers,          "List plugins with the write callback."  },
    { "loggers",        cmd_loggers,          "List plugins with the log callback."    },
    { "notifiers",      cmd_notifiers,        "List plugins with the notify callback." },
    { "write",          cmd_write,            ""                                       },
    { "read",           cmd_read,             ""                                       },
    { "notification",   cmd_notification,     ""                                       },
    { "query",          cmd_query,            ""                                       },
    { "query-range",    cmd_query_range,      ""                                       },
    { "series",         cmd_series,           ""                                       },
    { "family-metrics", cmd_family_metrics,   ""                                       },
    { "metrics",        cmd_metrics,          ""                                       },
    { "labels",         cmd_labels,           ""                                       },
    { "label-values",   cmd_label_values,     ""                                       },
    { "graph",          cmd_graph,            ""                                       },
    { "template",       cmd_template,         ""                                       },
    { "help",           cmd_help,             ""                                       }
};
size_t cmds_size = STATIC_ARRAY_SIZE(cmds);

__attribute__((noreturn)) static void exit_usage(const char *name, int status)
{
    fprintf((status == 0) ? stdout : stderr,
            "Usage: %s <command> [cmd options]\n\n"
            "Available commands:\n\n", name);
    for (size_t i = 0; i < cmds_size; i++) {
        fprintf((status == 0) ? stdout : stderr,
                "  %-18s %s\n", cmds[i].name, cmds[i].help);
    }
    fprintf((status == 0) ? stdout : stderr,
            "\nFor help on a command, use:\n"
            "\n  %s <command> --help\n"
            "\n" PACKAGE_NAME " " PACKAGE_VERSION "\n", name);
    exit(status);
}

int main(int argc, char **argv)
{
    char *env_unix_socket = getenv("NCOLLECTDCTL_UNIX_SOCKET");
    if (env_unix_socket != NULL) {
        int status = set_unix_socket(env_unix_socket);
        if (status != 0)
            fprintf(stderr, "Failed to set NCOLLECTDCTL_UNIX_SOCKET: '%s'.\n", env_unix_socket);
    }

    char *env_format = getenv("NCOLLECTDCTL_OUTPUT_FORMAT");
    if (env_format != NULL) {
        int status = set_output_format(env_format);
        if (status != 0)
            fprintf(stderr, "Failed to set NCOLLECTDCTL_OUTPUT_FORMAT: '%s'.\n", env_format);
    }

    program_name = argv[0];
    if (argc < 2) {
        fprintf(stderr, "%s: missing command\n", argv[0]);
        exit_usage(argv[0], 1);
    }

    int status = 0;
    size_t i;
    for (i = 0; i < cmds_size; i++) {
        if (strcmp(argv[1], cmds[i].name) == 0) {
            status = cmds[i].cmd(argc - 1, argv + 1);
            if (status < 0)
                status = -status;
            break;
        }
    }

    free(unix_socket);

    if (i == cmds_size) {
        if (strcmp(argv[1], "-h") == 0)
            exit_usage(argv[0], 0);
        if (strcmp(argv[1], "--help") == 0)
            exit_usage(argv[0], 0);
        fprintf(stderr, "%s: invalid command: %s\n", argv[0], argv[1]);
        return 1;
    }

    return status;
}
