// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/avltree.h"

#define MAILERS_MAX      25
#define MAILSTATS_MAGIC  0x1B1DE
#define MAILER_LEN       20

#define DEFAULT_SENDMAIL_CF "/etc/mail/sendmail.cf"
#define DEFAULT_QUEUE_DIR   "/var/spool/mqueue"

enum {
    FAM_SENDMAIL_FROM_CONNECTIONS,
    FAM_SENDMAIL_TO_CONNECTIONS,
    FAM_SENDMAIL_REJECT_CONNECTIONS,
    FAM_SENDMAIL_MAILER_FROM_MESSAGES,
    FAM_SENDMAIL_MAILER_FROM_BYTES,
    FAM_SENDMAIL_MAILER_TO_MESSAGES,
    FAM_SENDMAIL_MAILER_TO_BYTES,
    FAM_SENDMAIL_MAILER_REJECT_MESSAGES,
    FAM_SENDMAIL_MAILER_DISCARTED_MESSAGES,
    FAM_SENDMAIL_MAILER_QUARANTINED_MESSAGES,
    FAM_SENDMAIL_QUEUE_SIZE,
    FAM_SENDMAIL_QUEUE_MESSAGE_SIZE_BYTES,
    FAM_SENDMAIL_QUEUE_MESSAGE_AGE_SECONDS,
    FAM_SENDMAIL_MAX,
};

typedef struct {
    char *name;
    char *queue_path;
    char *cf_path;
    struct stat cf_stat;
    char status_path[MAXPATHLEN];
    double *buckets_queue_size_buckets;
    size_t buckets_queue_size_size;
    double *buckets_queue_age_buckets;
    size_t buckets_queue_age_size;
    label_set_t labels;
    plugin_filter_t *filter;
    cdtime_t timeout;
    char mailers[MAILERS_MAX][MAILER_LEN + 1];
    metric_family_t fams[FAM_SENDMAIL_MAX];
} sendmail_ctx_t;

typedef struct {
    int    magic;
    int    version;
    time_t itime;
    short  size;
    long   nf[MAILERS_MAX];
    long   bf[MAILERS_MAX];
    long   nt[MAILERS_MAX];
    long   bt[MAILERS_MAX];
    long   nr[MAILERS_MAX];
    long   nd[MAILERS_MAX];
} mailstats_v2_t;

typedef struct {
    int    magic;
    int    version;
    time_t itime;
    short  size;
    long   cf;
    long   ct;
    long   cr;
    long   nf[MAILERS_MAX];
    long   bf[MAILERS_MAX];
    long   nt[MAILERS_MAX];
    long   bt[MAILERS_MAX];
    long   nr[MAILERS_MAX];
    long   nd[MAILERS_MAX];
} mailstats_v3_t;

typedef struct {
    int    magic;
    int    version;
    time_t itime;
    short  size;
    long   cf;
    long   ct;
    long   cr;
    long   nf[MAILERS_MAX];
    long   bf[MAILERS_MAX];
    long   nt[MAILERS_MAX];
    long   bt[MAILERS_MAX];
    long   nr[MAILERS_MAX];
    long   nd[MAILERS_MAX];
    long   nq[MAILERS_MAX];
} mailstats_v4_t;

static metric_family_t fams[FAM_SENDMAIL_MAX] = {
    [FAM_SENDMAIL_FROM_CONNECTIONS] = {
        .name = "sendmail_from_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number from connections.",
    },
    [FAM_SENDMAIL_TO_CONNECTIONS] = {
        .name = "sendmail_to_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number to connections.",
    },
    [FAM_SENDMAIL_REJECT_CONNECTIONS] = {
        .name = "sendmail_reject_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of rejected connections.",
    },
    [FAM_SENDMAIL_MAILER_FROM_MESSAGES] = {
        .name = "sendmail_mailer_from_messages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of messages from the mailer.",
    },
    [FAM_SENDMAIL_MAILER_FROM_BYTES] = {
        .name = "sendmail_mailer_from_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes from the mailer.",
    },
    [FAM_SENDMAIL_MAILER_TO_MESSAGES] = {
        .name = "sendmail_mailer_to_messages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of messages to the mailer.",
    },
    [FAM_SENDMAIL_MAILER_TO_BYTES] = {
        .name = "sendmail_mailer_to_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes to the mailer.",
    },
    [FAM_SENDMAIL_MAILER_REJECT_MESSAGES] = {
        .name = "sendmail_mailer_reject_messages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of messages rejected by the mailer.",
    },
    [FAM_SENDMAIL_MAILER_DISCARTED_MESSAGES] = {
        .name = "sendmail_mailer_discarted_messages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of messages discarded by the mailer.",
    },
    [FAM_SENDMAIL_MAILER_QUARANTINED_MESSAGES] = {
        .name = "sendmail_mailer_quarantined_messages",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of messages quarantined by the mailer.",
    },
    [FAM_SENDMAIL_QUEUE_SIZE] = {
        .name = "sendmail_queue_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of messages in Sendmail's queue.",
    },
    [FAM_SENDMAIL_QUEUE_MESSAGE_SIZE_BYTES] = {
        .name = "sendmail_queue_message_size_bytes",
        .type = METRIC_TYPE_GAUGE_HISTOGRAM,
        .help = "Size of messages in Sendmail's message queue, in bytes.",
    },
    [FAM_SENDMAIL_QUEUE_MESSAGE_AGE_SECONDS] = {
        .name = "sendmail_queue_message_age_seconds",
        .type = METRIC_TYPE_GAUGE_HISTOGRAM,
        .help = "Age of messages in Sendmail's message queue, in seconds.",
    },
};

static int sendmail_read_queue(sendmail_ctx_t *ctx)
{
    DIR *dh = opendir(ctx->queue_path);
    if (dh == NULL) {
        PLUGIN_ERROR("Cannot open '%s': %s", ctx->queue_path, STRERRNO);
        return -1;
    }

    int dfd = dirfd(dh);
    if (dfd < 0) {
        PLUGIN_ERROR("Cannot get directory stream file descriptor '%s': %s",
                      ctx->queue_path, STRERRNO);
        return -1;
    }

    histogram_t *queue_msg_size = histogram_new_custom(ctx->buckets_queue_size_size,
                                                       ctx->buckets_queue_size_buckets);
    histogram_t *queue_msg_age = histogram_new_custom(ctx->buckets_queue_age_size,
                                                      ctx->buckets_queue_age_buckets);
    if ((queue_msg_size == NULL) || (queue_msg_age == NULL)) {
        closedir(dh);
        return -1;
    }

    double now = CDTIME_T_TO_DOUBLE(cdtime());
    uint64_t queue_size = 0;

    struct dirent *ent;
    while ((ent = readdir(dh)) != NULL) {
        if ((ent->d_name[0] != 'd') || (ent->d_name[1] != 'f'))
            continue;

        struct stat statbuf;
        int status = fstatat(dfd, ent->d_name, &statbuf, 0);
        if (status != 0) {
            if (errno == ENOENT)
                continue;
            PLUGIN_ERROR("Cannot stat file: '%s' in dir '%s': %s.",
                         ent->d_name, ctx->queue_path, STRERRNO);
            continue;
        }

        if ((statbuf.st_size > 0) && (S_ISREG(statbuf.st_mode))) {
#ifdef KERNEL_DARWIN
            double waiting = now - TIMESPEC_TO_DOUBLE(&statbuf.st_mtimespec);
#else
            double waiting = now - TIMESPEC_TO_DOUBLE(&statbuf.st_mtim);
#endif
            if (waiting < 0)
                waiting = 0;
            histogram_update(queue_msg_age, waiting);
            histogram_update(queue_msg_size, statbuf.st_size);
            queue_size++;
        }

    }

    closedir(dh);

    metric_family_append(&ctx->fams[FAM_SENDMAIL_QUEUE_SIZE],
                         VALUE_GAUGE(queue_size), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_SENDMAIL_QUEUE_MESSAGE_SIZE_BYTES],
                         VALUE_HISTOGRAM(queue_msg_size), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_SENDMAIL_QUEUE_MESSAGE_AGE_SECONDS],
                         VALUE_HISTOGRAM(queue_msg_age), &ctx->labels, NULL);

    histogram_destroy(queue_msg_size);
    histogram_destroy(queue_msg_age);

    return 0;
}

static int sendmail_read_mailers(sendmail_ctx_t *ctx)
{
    struct stat cf_stat;
    int status = stat(ctx->cf_path, &cf_stat);
    if (status != 0) {
        PLUGIN_ERROR("Cannot stat '%s': %s.", ctx->cf_path, STRERRNO);
        return -1;
    }

    if ((cf_stat.st_ino == ctx->cf_stat.st_ino) &&
        (cf_stat.st_dev == ctx->cf_stat.st_dev) &&
        (cf_stat.st_mtime == ctx->cf_stat.st_mtime) &&
        (cf_stat.st_size == ctx->cf_stat.st_size))
        return 0;

    memcpy(&ctx->cf_stat, &cf_stat, sizeof(struct stat));

    FILE *fh = fopen(ctx->cf_path, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Cannot open '%s': %s.", ctx->cf_path, STRERRNO);
        return -1;
    }

    int n = 0;
    sstrncpy(ctx->mailers[n++], "prog", MAILER_LEN);
    sstrncpy(ctx->mailers[n++], "*file*", MAILER_LEN);
    sstrncpy(ctx->mailers[n++], "*include*", MAILER_LEN);

    char line[MAXPATHLEN+20];

    while (fgets(line, sizeof(line), fh) != NULL) {
        char *end = strchr(line, '#');
        if (end != NULL)
            *end = '\0';

        size_t line_len = strlen(line);
        if (line_len == 0)
            continue;

        strnrtrim(line, line_len);

        char *c = line;
        if (*c == 'M') {
            c++;

            if (n >= MAILERS_MAX) {
                PLUGIN_ERROR("Too many mailers defined, %d max.", MAILERS_MAX);
                break;
            }

            end = strchr(c, ',');
            if (end == NULL)
                continue;
            *end = '\0';

            char *mailer = strntrim(c, strlen(c));

            int i;
            for (i = 0; i < n; i++) {
                if (strcmp(ctx->mailers[i], mailer) == 0)
                    break;
            }
            if (i == n) {
                sstrncpy(ctx->mailers[n], mailer, MAILER_LEN);
                n++;
            }

        } else if (*c == 'O') {
            c++;
            if (strncasecmp(c, " StatusFile", 11) == 0) {
                if (isalnum(c[11]))
                    continue;
                c = strchr(c, '=');
                if (c == NULL)
                    continue;
                c++;
                while (isspace(*c)) c++;

                sstrncpy(ctx->status_path, c, sizeof(ctx->status_path));
            }
        }

    }

    fclose(fh);

    for (; n < MAILERS_MAX; n++)
        ctx->mailers[n][0] = '\0';

    return 0;
}

static int sendmail_read_mailstats_v2(sendmail_ctx_t *ctx, mailstats_v2_t *stats)
{
    for (int i = 0; i < MAILERS_MAX; i++) {
        if (ctx->mailers[i][0] == '\0')
            continue;

        if (!(stats->nf[i] || stats->nt[i] || stats->nr[i] || stats->nd[i]))
            continue;

        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_FROM_MESSAGES],
                             VALUE_COUNTER(stats->nf[i]), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_FROM_BYTES],
                             VALUE_COUNTER(stats->bf[i]*1024), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_TO_MESSAGES],
                             VALUE_COUNTER(stats->nt[i]), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_TO_BYTES],
                             VALUE_COUNTER(stats->bt[i]*1024), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_REJECT_MESSAGES],
                             VALUE_COUNTER(stats->nr[i]), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_DISCARTED_MESSAGES],
                             VALUE_COUNTER(stats->nd[i]), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
    }

    return 0;
}

static int sendmail_read_mailstats_v3(sendmail_ctx_t *ctx, mailstats_v3_t *stats)
{
    metric_family_append(&ctx->fams[FAM_SENDMAIL_FROM_CONNECTIONS],
                         VALUE_COUNTER(stats->cf), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_SENDMAIL_TO_CONNECTIONS],
                         VALUE_COUNTER(stats->ct), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_SENDMAIL_REJECT_CONNECTIONS],
                         VALUE_COUNTER(stats->cr), &ctx->labels, NULL);

    for (int i = 0; i < MAILERS_MAX; i++) {
        if (ctx->mailers[i][0] == '\0')
            continue;

        if (!(stats->nf[i] || stats->nt[i] || stats->nr[i] || stats->nd[i]))
            continue;

        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_FROM_MESSAGES],
                             VALUE_COUNTER(stats->nf[i]), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_FROM_BYTES],
                             VALUE_COUNTER(stats->bf[i]*1024), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_TO_MESSAGES],
                             VALUE_COUNTER(stats->nt[i]), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_TO_BYTES],
                             VALUE_COUNTER(stats->bt[i]*1024), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_REJECT_MESSAGES],
                             VALUE_COUNTER(stats->nr[i]), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_DISCARTED_MESSAGES],
                             VALUE_COUNTER(stats->nd[i]), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);

    }

    return 0;
}

static int sendmail_read_mailstats_v4(sendmail_ctx_t *ctx, mailstats_v4_t *stats)
{
    metric_family_append(&ctx->fams[FAM_SENDMAIL_FROM_CONNECTIONS],
                         VALUE_COUNTER(stats->cf), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_SENDMAIL_TO_CONNECTIONS],
                         VALUE_COUNTER(stats->ct), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_SENDMAIL_REJECT_CONNECTIONS],
                         VALUE_COUNTER(stats->cr), &ctx->labels, NULL);

    for (int i = 0; i < MAILERS_MAX; i++) {
        if (ctx->mailers[i][0] == '\0')
            continue;

        if (!(stats->nf[i] || stats->nt[i] || stats->nq[i] || stats->nr[i] || stats->nd[i]))
            continue;

        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_FROM_MESSAGES],
                             VALUE_COUNTER(stats->nf[i]), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_FROM_BYTES],
                             VALUE_COUNTER(stats->bf[i]*1024), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_TO_MESSAGES],
                             VALUE_COUNTER(stats->nt[i]), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_TO_BYTES],
                             VALUE_COUNTER(stats->bt[i]*1024), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_REJECT_MESSAGES],
                             VALUE_COUNTER(stats->nr[i]), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_DISCARTED_MESSAGES],
                             VALUE_COUNTER(stats->nd[i]), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
        metric_family_append(&ctx->fams[FAM_SENDMAIL_MAILER_QUARANTINED_MESSAGES],
                             VALUE_COUNTER(stats->nq[i]), &ctx->labels,
                             &(label_pair_const_t){.name="mailer", .value=ctx->mailers[i]}, NULL);
    }

    return 0;
}

static int sendmail_read_mailstats(sendmail_ctx_t *ctx)
{
    int fd = open(ctx->status_path, O_RDONLY, 0600);
    if (fd < 0) {
        PLUGIN_ERROR("Cannot open '%s': %s.", ctx->status_path, STRERRNO);
        return -1;
    }

    mailstats_v4_t stats;
    ssize_t size = read(fd, &stats, sizeof(stats));

    close(fd);

    if (size < 0) {
        PLUGIN_ERROR("Cannot read '%s': %s.", ctx->status_path, STRERRNO);
        return -1;
    }

    if (size == 0)
        return 0;

    if (stats.magic != MAILSTATS_MAGIC) {
        PLUGIN_ERROR("Incorrect magic number in '%s'.", ctx->status_path);
        return -1;
    }

    switch(stats.version) {
    case 4:
        if ((size != sizeof(mailstats_v4_t)) || (stats.size != sizeof(mailstats_v4_t))) {
            PLUGIN_ERROR("Incorrect file size: '%s'.", ctx->status_path);
            return -1;
        }
        sendmail_read_mailstats_v4(ctx, (mailstats_v4_t *)&stats);
        break;
    case 3:
        if ((size != sizeof(mailstats_v3_t)) || (stats.size != sizeof(mailstats_v3_t))) {
            PLUGIN_ERROR("Incorrect file size: '%s'.", ctx->status_path);
            return -1;
        }
        sendmail_read_mailstats_v3(ctx, (mailstats_v3_t *)&stats);
        break;
    case 2:
        if ((size != sizeof(mailstats_v2_t)) || (stats.size != sizeof(mailstats_v2_t))) {
            PLUGIN_ERROR("Incorrect file size: '%s'.", ctx->status_path);
            return -1;
        }
        sendmail_read_mailstats_v2(ctx, (mailstats_v2_t *)&stats);
        break;
    default:
        PLUGIN_ERROR("Incorrect stats version: '%s'.", ctx->status_path);
        break;
    }

    return 0;
}

static int sendmail_read(user_data_t *user_data)
{
    sendmail_ctx_t *ctx = user_data->data;

    cdtime_t submit = cdtime();

    int status = sendmail_read_mailers(ctx);
    if (status == 0)
        sendmail_read_mailstats(ctx);

    sendmail_read_queue(ctx);

    plugin_dispatch_metric_family_array_filtered(ctx->fams, FAM_SENDMAIL_MAX, ctx->filter, submit);

    return 0;
}

static void sendmail_ctx_free(void *arg)
{
    sendmail_ctx_t *ctx = arg;

    if (ctx == NULL)
        return;

    free(ctx->name);
    free(ctx->cf_path);
    free(ctx->queue_path);

    free(ctx->buckets_queue_size_buckets);
    free(ctx->buckets_queue_age_buckets);

    label_set_reset(&ctx->labels);
    plugin_filter_free(ctx->filter);

    free(ctx);
}

static int sendmail_config_instance (config_item_t *ci)
{
    sendmail_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    memcpy(ctx->fams, fams, sizeof(ctx->fams[0])*FAM_SENDMAIL_MAX);

    int status = cf_util_get_string(ci, &ctx->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        sendmail_ctx_free(ctx);
        return status;
    }
    assert(ctx->name != NULL);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("config-path", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->cf_path);
        } else if (strcasecmp("queue-path", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->queue_path);
        } else if (strcasecmp("histogram-queue-size-buckets", child->key) == 0) {
            status = cf_util_get_double_array(child, &ctx->buckets_queue_size_size,
                                                     &ctx->buckets_queue_size_buckets);
        } else if (strcasecmp("histogram-queue-age-buckets", child->key) == 0) {
            status = cf_util_get_double_array(child, &ctx->buckets_queue_age_size,
                                                     &ctx->buckets_queue_age_buckets);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
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
        sendmail_ctx_free(ctx);
        return -1;
    }

    if (ctx->cf_path == NULL) {
        ctx->cf_path = strdup(DEFAULT_SENDMAIL_CF);
        if (ctx->cf_path == NULL) {
            PLUGIN_ERROR("strdup failed.");
            sendmail_ctx_free(ctx);
            return -1;
        }
    }

    if (ctx->queue_path == NULL) {
        ctx->queue_path = strdup(DEFAULT_QUEUE_DIR);
        if (ctx->queue_path == NULL) {
            PLUGIN_ERROR("strdup failed.");
            sendmail_ctx_free(ctx);
            return -1;
        }
    }

    if (ctx->buckets_queue_size_size == 0) {
        double buckets[] = {1024, 4096, 65536, 262144, 524288, 1048576, 4194304, 8388608,
                            16777216, 20971520};
        ctx->buckets_queue_size_size = STATIC_ARRAY_SIZE(buckets);
        ctx->buckets_queue_size_buckets = malloc(ctx->buckets_queue_size_size * sizeof(double));
        if (ctx->buckets_queue_size_buckets == NULL) {
            PLUGIN_ERROR("malloc failed.");
            sendmail_ctx_free(ctx);
            return -1;
        }
        memcpy(ctx->buckets_queue_size_buckets, buckets,
               ctx->buckets_queue_size_size * sizeof(double));
    }

    if (ctx->buckets_queue_age_size == 0) {
        double buckets[] = {10, 30, 60, 300, 900, 1800, 3600, 10800, 21600, 86400, 259200, 604800};
        ctx->buckets_queue_age_size = STATIC_ARRAY_SIZE(buckets);
        ctx->buckets_queue_age_buckets = malloc(ctx->buckets_queue_age_size * sizeof(double));
        if (ctx->buckets_queue_age_buckets == NULL) {
            PLUGIN_ERROR("malloc failed.");
            sendmail_ctx_free(ctx);
            return -1;
        }
        memcpy(ctx->buckets_queue_age_buckets, buckets,
               ctx->buckets_queue_age_size * sizeof(double));
    }

    label_set_add(&ctx->labels, true, "instance", ctx->name);

    return plugin_register_complex_read("sendmail", ctx->name, sendmail_read, interval,
                                        &(user_data_t){.data = ctx, .free_func = sendmail_ctx_free});
}

static int sendmail_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = sendmail_config_instance(child);
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
    plugin_register_config("sendmail", sendmail_config);
}
