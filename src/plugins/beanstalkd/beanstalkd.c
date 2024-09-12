// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/socket.h"

#include <poll.h>

enum {
    FAM_BEANSTALKD_UP,
    FAM_BEANSTALKD_CURRENT_JOBS_URGENT,
    FAM_BEANSTALKD_CURRENT_JOBS_READY,
    FAM_BEANSTALKD_CURRENT_JOBS_RESERVED,
    FAM_BEANSTALKD_CURRENT_JOBS_DELAYED,
    FAM_BEANSTALKD_CURRENT_JOBS_BURIED,
    FAM_BEANSTALKD_COMMAND,
    FAM_BEANSTALKD_JOB_TIMEOUTS,
    FAM_BEANSTALKD_JOBS,
    FAM_BEANSTALKD_MAX_JOB_SIZE_BYTES,
    FAM_BEANSTALKD_CURRENT_TUBES,
    FAM_BEANSTALKD_CURRENT_CONNECTIONS,
    FAM_BEANSTALKD_CURRENT_PRODUCERS,
    FAM_BEANSTALKD_CURRENT_WORKERS,
    FAM_BEANSTALKD_CURRENT_WAITING,
    FAM_BEANSTALKD_CONNECTIONS,
    FAM_BEANSTALKD_CPU_USER_TIME_SECONDS,
    FAM_BEANSTALKD_CPU_SYSTEM_TIME_SECONDS,
    FAM_BEANSTALKD_UPTIME_SECONDS,
    FAM_BEANSTALKD_BINLOG_OLDEST_INDEX,
    FAM_BEANSTALKD_BINLOG_CURRENT_INDEX,
    FAM_BEANSTALKD_BINLOG_MAX_SIZE_BYTES,
    FAM_BEANSTALKD_BINLOG_RECORDS_WRITTEN,
    FAM_BEANSTALKD_BINLOG_RECORDS_MIGRATED,
    FAM_BEANSTALKD_DRAINING,
    FAM_BEANSTALKD_MAX,
};

#include "plugins/beanstalkd/beanstalkd_stats.h"

static metric_family_t fams[FAM_BEANSTALKD_MAX] = {
    [FAM_BEANSTALKD_UP] = {
        .name = "beanstalkd_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the beanstalkd server be reached.",
    },
    [FAM_BEANSTALKD_CURRENT_JOBS_URGENT] = {
        .name = "beanstalkd_current_jobs_urgent",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current number of ready jobs with priority < 1024.",
    },
    [FAM_BEANSTALKD_CURRENT_JOBS_READY] = {
        .name = "beanstalkd_current_jobs_ready",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current number of jobs in the ready queue.",
    },
    [FAM_BEANSTALKD_CURRENT_JOBS_RESERVED] = {
        .name = "beanstalkd_current_jobs_reserved",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current number of jobs reserved by all clients.",
    },
    [FAM_BEANSTALKD_CURRENT_JOBS_DELAYED] = {
        .name = "beanstalkd_current_jobs_delayed",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current number of delayed jobs.",
    },
    [FAM_BEANSTALKD_CURRENT_JOBS_BURIED] = {
        .name = "beanstalkd_current_jobs_buried",
        .type = METRIC_TYPE_GAUGE,
        .help = "The current number of is the number of buried jobs.",
    },
    [FAM_BEANSTALKD_COMMAND] = {
        .name = "beanstalkd_command",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of commandas of this type.",
    },
    [FAM_BEANSTALKD_JOB_TIMEOUTS] = {
        .name = "beanstalkd_job_timeouts",
        .type = METRIC_TYPE_COUNTER,
        .help = "The count of times a job has timed out.",
    },
    [FAM_BEANSTALKD_JOBS] = {
        .name = "beanstalkd_jobs",
        .type = METRIC_TYPE_COUNTER,
        .help = "The cumulative count of jobs created.",
    },
    [FAM_BEANSTALKD_MAX_JOB_SIZE_BYTES] = {
        .name = "beanstalkd_max_job_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum number of bytes in a job.",
    },
    [FAM_BEANSTALKD_CURRENT_TUBES] = {
        .name = "beanstalkd_current_tubes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of currently-existing tubes.",
    },
    [FAM_BEANSTALKD_CURRENT_CONNECTIONS] = {
        .name = "beanstalkd_current_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of currently open connections.",
    },
    [FAM_BEANSTALKD_CURRENT_PRODUCERS] = {
        .name = "beanstalkd_current_producers",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of open connections that have each issued "
                "at least one put command.",
    },
    [FAM_BEANSTALKD_CURRENT_WORKERS] = {
        .name = "beanstalkd_current_workers",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of open connections that have each issued "
                "at least one reserve command.",
    },
    [FAM_BEANSTALKD_CURRENT_WAITING] = {
        .name = "beanstalkd_current_waiting",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of open connections that have issued "
                "a reserve command but not yet received a response.",
    },
    [FAM_BEANSTALKD_CONNECTIONS] = {
        .name = "beanstalkd_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "The cumulative count of connections.",
    },
    [FAM_BEANSTALKD_CPU_USER_TIME_SECONDS] = {
        .name = "beanstalkd_cpu_user_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The cumulative user CPU time of this process in seconds.",
    },
    [FAM_BEANSTALKD_CPU_SYSTEM_TIME_SECONDS] = {
        .name = "beanstalkd_cpu_system_time_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "The cumulative system CPU time of this process in seconds.",
    },
    [FAM_BEANSTALKD_UPTIME_SECONDS] = {
        .name = "beanstalkd_uptime_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of seconds since this server process started running.",
    },
    [FAM_BEANSTALKD_BINLOG_OLDEST_INDEX] = {
        .name = "beanstalkd_binlog_oldest_index",
        .type = METRIC_TYPE_GAUGE,
        .help = "The index of the oldest binlog file needed to store the current jobs.",
    },
    [FAM_BEANSTALKD_BINLOG_CURRENT_INDEX] = {
        .name = "beanstalkd_binlog_current_index",
        .type = METRIC_TYPE_GAUGE,
        .help = "The index of the current binlog file being written to. "
                "If binlog is not active this value will be 0.",
    },
    [FAM_BEANSTALKD_BINLOG_MAX_SIZE_BYTES] = {
        .name = "beanstalkd_binlog_max_size_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum size in bytes a binlog file is allowed "
                "to get before a new binlog file is opened.",
    },
    [FAM_BEANSTALKD_BINLOG_RECORDS_WRITTEN] = {
        .name = "beanstalkd_binlog_records_written",
        .type = METRIC_TYPE_COUNTER,
        .help = "The cumulative number of records written to the binlog.",
    },
    [FAM_BEANSTALKD_BINLOG_RECORDS_MIGRATED] = {
        .name = "beanstalkd_binlog_records_migrated",
        .type = METRIC_TYPE_COUNTER,
        .help = "The cumulative number of records written as part of compaction.",
    },
    [FAM_BEANSTALKD_DRAINING] = {
        .name = "beanstalkd_draining",
        .type = METRIC_TYPE_GAUGE,
        .help = "Is set to 1 if the server is in drain mode, 0 otherwise.",
    },
};

typedef struct {
    char *name;
    char *host;
    int port;
    cdtime_t timeout;
    label_set_t labels;
    plugin_filter_t *filter;
    metric_family_t fams[FAM_BEANSTALKD_MAX];
} beanstalkd_ctx_t;

#define BEANSTALKD_PORT 11300

static int beanstalkd_query_stats(beanstalkd_ctx_t *ctx, char *buffer, size_t buffer_size)
{
    int sd = socket_connect_tcp(ctx->host, ctx->port, 0, 0);
    if (sd < 0) {
        PLUGIN_ERROR("Instance \"%s\" could not connect to daemon.", ctx->name);
        return -1;
    }

    int status = (int)swrite(sd, "stats\r\n", strlen("stats\r\n"));
    if (status != 0) {
        PLUGIN_ERROR("write(2) failed: %s", STRERRNO);
        close(sd);
        return -1;
    }

    int flags = fcntl(sd, F_GETFL);
    status = fcntl(sd, F_SETFL, flags | O_NONBLOCK);
    if (status != 0) {
        close(sd);
        return -1;
    }

    memset(buffer, 0, buffer_size);
    size_t buffer_fill = 0;

    struct pollfd pollfd = {
        .fd = sd,
        .events = POLLIN,
    };

    do {
        status = poll(&pollfd, 1, CDTIME_T_TO_MS(ctx->timeout));
    } while (status < 0 && errno == EINTR);

    if (status <= 0) {
        PLUGIN_ERROR("Timeout reading from socket");
        return -1;
    }

    while ((status = (int)recv(sd, buffer + buffer_fill, buffer_size - buffer_fill, 0)) != 0) {
        if (status < 0) {
            if (errno == EAGAIN)
                break;
            if (errno == EINTR)
                continue;
            PLUGIN_ERROR("Error reading from socket: %s", STRERRNO);
            close(sd);
            return -1;
        }

        buffer_fill += (size_t)status;
    }

    status = 0;
    if (buffer_fill == 0) {
        PLUGIN_WARNING("No data returned by MNTR command.");
        status = -1;
    }

    close(sd);
    return status;
}

static int beanstalkd_read(user_data_t *user_data)
{
    beanstalkd_ctx_t *ctx = user_data->data;

    if (ctx == NULL)
        return -1;
    
    char buffer[8192];
    cdtime_t submit = cdtime();
   
//    beanstalkd_query(ctx, "stats\r\n", buffer, sizeof(buffer));
    int status = beanstalkd_query_stats(ctx, buffer, sizeof(buffer));
    if (status != 0)
        goto error;

    if (strncmp("OK ", buffer, strlen("OK ")) != 0)
        goto error;

    char *data = strstr(buffer, "\r\n");
    if (data == NULL)
        goto error;
    data += 2;

    char *end = strstr(buffer, "\r\n");
    if (end == NULL)
        goto error;

    metric_family_append(&ctx->fams[FAM_BEANSTALKD_UP], VALUE_GAUGE(1), &ctx->labels, NULL);

    char *line = NULL;
    char *ptr = data;
    char *save_ptr = NULL;
    while ((line = strtok_r(ptr, "\n", &save_ptr)) != NULL) {
        ptr = NULL;
        char *fields[2];
        if (strsplit(line, fields, 2) != 2)
            continue;

        const struct beanstalkd_stats *bm = beanstalkd_stats_get_key(fields[0], strlen(fields[0]));
        if (bm == NULL)
            continue;

        metric_family_t *fam = &ctx->fams[bm->fam];

        value_t value = {0};
        switch (bm->fam) {
        case FAM_BEANSTALKD_CPU_USER_TIME_SECONDS:
        case FAM_BEANSTALKD_CPU_SYSTEM_TIME_SECONDS:
            value = VALUE_COUNTER_FLOAT64(atof(fields[1]));
            break;
        case FAM_BEANSTALKD_DRAINING:
            if (strcmp(fields[1], "true") == 0) {
                value = VALUE_GAUGE(1);
            } else {
                value = VALUE_GAUGE(0);
            }
            break;
        default:
            if (fam->type == METRIC_TYPE_COUNTER) {
                value = VALUE_COUNTER(atoll(fields[1]));
            } else if (fam->type == METRIC_TYPE_GAUGE) {
                value = VALUE_GAUGE(atof(fields[1]));
            }
            break;
        }
 
        if (bm->lname == NULL) {
            metric_family_append(fam, value, &ctx->labels, NULL);   
        } else {
            metric_family_append(fam, value, &ctx->labels,
                                 &(label_pair_const_t){.name=bm->lname, .value=bm->lvalue}, NULL);

        }
    }

    plugin_dispatch_metric_family_array_filtered(ctx->fams, FAM_BEANSTALKD_MAX, ctx->filter, submit);
    return 0;

error:
    metric_family_append(&ctx->fams[FAM_BEANSTALKD_UP], VALUE_GAUGE(0), &ctx->labels, NULL);
    plugin_dispatch_metric_family_filtered(&ctx->fams[FAM_BEANSTALKD_UP], ctx->filter, 0);
    return 0;
}

static void beanstalkd_free(void *arg)
{
    beanstalkd_ctx_t *ctx = arg;

    if (ctx == NULL)
        return;

    free(ctx->name);
    free(ctx->host);

    label_set_reset(&ctx->labels);
    plugin_filter_free(ctx->filter);

    free(ctx);
}

static int beanstalkd_config_instance (config_item_t *ci)
{
    beanstalkd_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    memcpy(ctx->fams, fams, sizeof(ctx->fams[0])*FAM_BEANSTALKD_MAX);

    int status = cf_util_get_string(ci, &ctx->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(ctx);
        return status;
    }
    assert(ctx->name != NULL);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "host") == 0) {
            status = cf_util_get_string(child, &ctx->host);
        } else if (strcasecmp(child->key, "port") == 0) {
            status = cf_util_get_port_number(child, &ctx->port);
        } else if (strcasecmp(child->key, "label") == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp(child->key, "timeout") == 0) {
            status = cf_util_get_cdtime(child, &ctx->timeout);
        } else if (strcasecmp(child->key, "interval") == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = plugin_filter_configure(child, &ctx->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        beanstalkd_free(ctx);
        return -1;
    }

    if (ctx->host == NULL) {
        ctx->host = strdup("localhost");
        if (ctx->host == NULL) {
            PLUGIN_ERROR("strdup failed.");
            beanstalkd_free(ctx);
            return -1;
        }
    }

    if (ctx->port == 0)
        ctx->port = BEANSTALKD_PORT;

    if (ctx->timeout == 0) {
        if (interval == 0)
            interval = plugin_get_interval();
        ctx->timeout = interval / 2;
    } else {
        if (interval == 0)
            interval = plugin_get_interval();
        if (ctx->timeout > interval) {
            PLUGIN_ERROR("Timeout: %.3f it's bigger than plugin interval: %.3f.",
                         CDTIME_T_TO_DOUBLE(ctx->timeout), CDTIME_T_TO_DOUBLE(interval));
            beanstalkd_free(ctx);
            return -1;
        }
    }

    label_set_add(&ctx->labels, true, "instance", ctx->name);

    return plugin_register_complex_read("beanstalkd", ctx->name, beanstalkd_read, interval,
                                        &(user_data_t){.data = ctx, .free_func = beanstalkd_free});
}

static int beanstalkd_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = beanstalkd_config_instance(child);
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
    plugin_register_config("beanstalkd", beanstalkd_config);
}
