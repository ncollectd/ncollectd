// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/avltree.h"
#include "libutils/tail.h"
#include "libutils/socket.h"

#include <regex.h>

#ifdef  HAVE_SD_JOURNAL
#include <systemd/sd-journal.h>
#endif

#define DEFAULT_SERVICE "postfix.service"
#define DEFAULT_LOG_PATH "/var/log/mail.log"
#define DEFAULT_SHOWQ_PATH "/var/spool/postfix/public/showq"

enum {
    FAM_POSTFIX_CLEANUP_MESSAGES_PROCESSED,
    FAM_POSTFIX_CLEANUP_MESSAGES_REJECTED,
    FAM_POSTFIX_CLEANUP_MESSAGES_NOT_ACCEPTED,
    FAM_POSTFIX_LMTP_DELIVERY_DELAY_SECONDS,
    FAM_POSTFIX_PIPE_DELIVERY_DELAY_SECONDS,
    FAM_POSTFIX_QMGR_MESSAGES_INSERTED_RECEIPIENTS,
    FAM_POSTFIX_QMGR_MESSAGES_INSERTED_SIZE_BYTES,
    FAM_POSTFIX_QMGR_MESSAGES_REMOVED,
    FAM_POSTFIX_QMGR_MESSAGES_EXPIRED,
    FAM_POSTFIX_SMTP_DELIVERY_DELAY_SECONDS,
    FAM_POSTFIX_SMTP_TLS_CONNECTIONS,
    FAM_POSTFIX_SMTP_MESSAGES_PROCESSED,
    FAM_POSTFIX_SMTP_SASL_MESSAGES_PROCESSED,
    FAM_POSTFIX_SMTP_CONNECTION_TIMED_OUT,
    FAM_POSTFIX_SMTPD_CONNECTS,
    FAM_POSTFIX_SMTPD_DISCONNECTS,
    FAM_POSTFIX_SMTPD_FORWARD_CONFIRMED_REVERSE_DNS_ERRORS,
    FAM_POSTFIX_SMTPD_CONNECTIONS_LOST,
    FAM_POSTFIX_SMTPD_MESSAGES_PROCESSED,
    FAM_POSTFIX_SMTPD_MESSAGES_REJECTED,
    FAM_POSTFIX_SMTPD_SASL_AUTHENTICATION_FAILURES,
    FAM_POSTFIX_SMTPD_TLS_CONNECTIONS,
    FAM_POSTFIX_BOUNCE_NON_DELIVERY_NOTIFICATION,
    FAM_POSTFIX_VIRTUAL_DELIVERED,
    FAM_POSTFIX_QUEUE_SIZE,
    FAM_POSTFIX_QUEUE_MESSAGE_SIZE_BYTES,
    FAM_POSTFIX_QUEUE_MESSAGE_AGE_SECONDS,
    FAM_POSTFIX_MAX,
};

enum {
    POSTFIX_REGEX_LOG,
    POSTFIX_REGEX_LMTP_PIPE_SMTP,
    POSTFIX_REGEX_QMGR_INSERT,
    POSTFIX_REGEX_QMGR_EXPIRED,
    POSTFIX_REGEX_SMTP_STATUS,
    POSTFIX_REGEX_SMTP_TLS,
    POSTFIX_REGEX_SMTP_CONNECTION_TIMEDOUT,
    POSTFIX_REGEX_SMTPD_FCRDNS_ERRORS,
    POSTFIX_REGEX_SMTPD_PROCESSES_SASL,
    POSTFIX_REGEX_SMTPD_REJECTS,
    POSTFIX_REGEX_SMTPD_LOST_CONNECTION,
    POSTFIX_REGEX_SMTPD_SASL_AUTH_FAILURES,
    POSTFIX_REGEX_SMTPD_TLS,
    POSTFIX_REGEX_MAX,
};

typedef enum {
    POSTFIX_LOG_FROM_NONE,
    POSTFIX_LOG_FROM_FILE,
    POSTFIX_LOG_FROM_SD_JOURNAL
} postfix_log_from_t;

typedef struct {
    histogram_t *lmtp_before_queue_manager_delays;
    histogram_t *lmtp_queue_manager_delays;
    histogram_t *lmtp_connection_setup_delays;
    histogram_t *lmtp_transmission_delays;
    c_avl_tree_t *pipe_before_queue_manager_delays;
    c_avl_tree_t *pipe_queue_manager_delays;
    c_avl_tree_t *pipe_connection_setup_delays;
    c_avl_tree_t *pipe_transmission_delays;
    histogram_t *smtp_before_queue_manager_delays;
    histogram_t *smtp_queue_manager_delays;
    histogram_t *smtp_connection_setup_delays;
    histogram_t *smtp_transmission_delays;
    histogram_t *qmgr_inserts_nrcpt;
    histogram_t *qmgr_inserts_size;
    uint64_t cleanup_processes;
    uint64_t cleanup_rejects;
    uint64_t cleanup_not_accepted;
    uint64_t qmgr_removes;
    uint64_t qmgr_expires;
    uint64_t smtp_tls_connects;
    uint64_t smtp_connection_timedout;
    c_avl_tree_t *smtp_processed;
    uint64_t smtpd_connects;
    uint64_t smtpd_disconnects;
    uint64_t smtpd_fcr_dns_errors;
    c_avl_tree_t *smtpd_lost_connections;
    uint64_t smtpd_msg_processed;
    c_avl_tree_t *smtpd_sasl_msg_processed;
    c_avl_tree_t *smtpd_rejects;
    uint64_t smtpd_sasl_auth_failures;
    uint64_t smtpd_tls_connects;
    uint64_t bounce_non_delivery;
    uint64_t virtual_delivered;
} postfix_stats_t;

typedef struct {
    double *buckets;
    size_t size;
} postfix_buckets_t;

typedef struct {
    char *name;
    char *log_path;
    char *showq_path;
    char *unit;
    tail_t tail;
#ifdef HAVE_SD_JOURNAL
    sd_journal *journal;
#endif
    postfix_log_from_t log_from;
    postfix_buckets_t buckets_time;
    postfix_buckets_t buckets_queue_size;
    postfix_buckets_t buckets_queue_age;
    postfix_buckets_t buckets_qmgr_inserts_nrcpt;
    postfix_buckets_t buckets_qmgr_inserts_size;
    label_set_t labels;
    plugin_filter_t *filter;
    cdtime_t timeout;
    regex_t preg[POSTFIX_REGEX_MAX];
    postfix_stats_t stats;
    metric_family_t fams[FAM_POSTFIX_MAX];
} postfix_ctx_t;

static postfix_buckets_t default_buckets_time = {
    .buckets = (double[]){0.001, 0.01, 0.1, 1, 10, 60, 3600, 86400, 172800},
    .size = 9,
};
static postfix_buckets_t default_buckets_queue_size = {
    .buckets = (double[]){1024, 4096, 65536, 262144, 524288, 1048576, 4194304, 8388608, 16777216,
                          20971520},
    .size = 10,
};
static postfix_buckets_t default_buckets_queue_age = {
    .buckets = (double[]){10, 30, 60, 300, 900, 1800, 3600, 10800, 21600, 86400, 259200, 604800},
    .size = 12,
};
static postfix_buckets_t default_buckets_qmgr_inserts_nrcpt = {
    .buckets = (double[]){1, 2, 4, 8, 16, 32, 64, 128},
    .size = 8,
};
static postfix_buckets_t default_buckets_qmgr_inserts_size = {
    .buckets = (double[]){1024, 4096, 65536, 262144, 524288, 1048576, 4194304, 8388608, 16777216,
                          20971520},
    .size = 10,
};

static const char *postfix_regex[POSTFIX_REGEX_MAX] = {
    [POSTFIX_REGEX_LOG] =
        " ?postfix(/([a-zA-Z0-9_]+))?\\[[0-9]+\\]: ((warning|error|fatal|panic): )?(.*)",
    [POSTFIX_REGEX_LMTP_PIPE_SMTP] =
        ", relay=([^ \\t]+), .*, delays=([0-9\\.]+)/([0-9\\.]+)/([0-9\\.]+)/([0-9\\.]+)(,.*)",
    [POSTFIX_REGEX_QMGR_INSERT] =
        ":.*, size=([0-9]+), nrcpt=([0-9]+) ",
    [POSTFIX_REGEX_QMGR_EXPIRED] =
        ":.*, status=(expired|force-expired), returned to sender",
    [POSTFIX_REGEX_SMTP_STATUS] =
        ", status=([a-zA-Z0-9_]+) ",
    [POSTFIX_REGEX_SMTP_TLS] =
        "^([^ \\t]+) TLS connection established to [^ \\t]+: ([^ \\t]+) with cipher ([^ \\t]+) \\(([0-9]+)/([0-9]+) bits\\)",
    [POSTFIX_REGEX_SMTP_CONNECTION_TIMEDOUT] =
        "^connect[ \\t]+to[ \\t]+(.*)\\[(.*)\\]:[0-9]+):[ \\t]+(Connection timed out)$",
    [POSTFIX_REGEX_SMTPD_FCRDNS_ERRORS] =
        "^warning: hostname [^ \\t]+ does not resolve to address ",
    [POSTFIX_REGEX_SMTPD_PROCESSES_SASL] =
        ": client=.*, sasl_method=([^ \\t]+)",
    [POSTFIX_REGEX_SMTPD_REJECTS] =
        "^NOQUEUE: reject: RCPT from [^ \\t]+: ([0-9]+) ",
    [POSTFIX_REGEX_SMTPD_LOST_CONNECTION] =
        "^lost connection after ([a-zA-Z0-9_]+) from ",
    [POSTFIX_REGEX_SMTPD_SASL_AUTH_FAILURES] =
        "^warning: [^ \\t]+: SASL [^ \\t]+ authentication failed: ",
    [POSTFIX_REGEX_SMTPD_TLS] =
        "^([^ \\t]+) TLS connection established from [^ \\t]+: ([^ \\t]+) with cipher ([^ \\t]+) \\(([0-9]+)/([0-9]+) bits\\)",
};

static metric_family_t fams[FAM_POSTFIX_MAX] = {
    [FAM_POSTFIX_CLEANUP_MESSAGES_PROCESSED] = {
        .name = "postfix_cleanup_messages_processed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of messages processed by cleanup.",
    },
    [FAM_POSTFIX_CLEANUP_MESSAGES_REJECTED] = {
        .name = "postfix_cleanup_messages_rejected",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of messages rejected by cleanup.",
    },
    [FAM_POSTFIX_CLEANUP_MESSAGES_NOT_ACCEPTED] = {
        .name = "postfix_cleanup_messages_not_accepted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of messages not accepted by cleanup.",
    },
    [FAM_POSTFIX_LMTP_DELIVERY_DELAY_SECONDS] = {
        .name = "postfix_lmtp_delivery_delay_seconds",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "LMTP message processing time in seconds.",
    },
    [FAM_POSTFIX_PIPE_DELIVERY_DELAY_SECONDS] = {
        .name = "postfix_pipe_delivery_delay_seconds",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "Pipe message processing time in seconds.",
    },
    [FAM_POSTFIX_QMGR_MESSAGES_INSERTED_RECEIPIENTS] = {
        .name = "postfix_qmgr_messages_inserted_receipients",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "Number of receipients per message inserted into the mail queues.",
    },
    [FAM_POSTFIX_QMGR_MESSAGES_INSERTED_SIZE_BYTES] = {
        .name = "postfix_qmgr_messages_inserted_size_bytes",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "Size of messages inserted into the mail queues in bytes.",
    },
    [FAM_POSTFIX_QMGR_MESSAGES_REMOVED] = {
        .name = "postfix_qmgr_messages_removed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of messages removed from mail queues.",
    },
    [FAM_POSTFIX_QMGR_MESSAGES_EXPIRED] = {
        .name = "postfix_qmgr_messages_expired",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of messages expired from mail queues.",
    },
    [FAM_POSTFIX_SMTP_DELIVERY_DELAY_SECONDS] = {
        .name = "postfix_smtp_delivery_delay_seconds",
        .type = METRIC_TYPE_HISTOGRAM,
        .help = "SMTP message processing time in seconds.",
    },
    [FAM_POSTFIX_SMTP_TLS_CONNECTIONS] = {
        .name = "postfix_smtp_tls_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of outgoing TLS connections.",
    },
    [FAM_POSTFIX_SMTP_MESSAGES_PROCESSED] = {
        .name = "postfix_smtp_messages_processed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of messages that have been processed by the smtp process.",
    },
    [FAM_POSTFIX_SMTP_SASL_MESSAGES_PROCESSED] = {
        .name = "postfix_smtp_sasl_messages_processed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of messages that have been processed by the smtp process "
                "with SASL auth.",
    },
    [FAM_POSTFIX_SMTP_CONNECTION_TIMED_OUT] = {
        .name = "postfix_smtp_connection_timed_out",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of messages that have been deferred on SMTP.", // FIXME
    },
    [FAM_POSTFIX_SMTPD_CONNECTS] = {
        .name = "postfix_smtpd_connects",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of incoming connections.",
    },
    [FAM_POSTFIX_SMTPD_DISCONNECTS] = {
        .name = "postfix_smtpd_disconnects",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of incoming disconnections.",
    },
    [FAM_POSTFIX_SMTPD_FORWARD_CONFIRMED_REVERSE_DNS_ERRORS] = {
        .name = "postfix_smtpd_forward_confirmed_reverse_dns_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of connections for which forward-confirmed DNS cannot be resolved.",
    },
    [FAM_POSTFIX_SMTPD_CONNECTIONS_LOST] = {
        .name = "postfix_smtpd_connections_lost",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of connections lost.",
    },
    [FAM_POSTFIX_SMTPD_MESSAGES_PROCESSED] = {
        .name = "postfix_smtpd_messages_processed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of messages processed.",
    },
    [FAM_POSTFIX_SMTPD_MESSAGES_REJECTED] = {
        .name = "postfix_smtpd_messages_rejected",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of NOQUEUE rejects.",
    },
    [FAM_POSTFIX_SMTPD_SASL_AUTHENTICATION_FAILURES] = {
        .name = "postfix_smtpd_sasl_authentication_failures",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of SASL authentication failures.",
    },
    [FAM_POSTFIX_SMTPD_TLS_CONNECTIONS] = {
        .name = "postfix_smtpd_tls_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of incoming TLS connections.",
    },
    [FAM_POSTFIX_BOUNCE_NON_DELIVERY_NOTIFICATION] = {
        .name = "postfix_bounce_non_delivery_notification",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of non delivery notification sent by bounce.",
    },
    [FAM_POSTFIX_VIRTUAL_DELIVERED] = {
        .name = "postfix_virtual_delivered",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of mail delivered to a virtual mailbox.",
    },
    [FAM_POSTFIX_QUEUE_SIZE] = {
        .name = "postfix_queue_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of messages in Postfix's queue.",
    },
    [FAM_POSTFIX_QUEUE_MESSAGE_SIZE_BYTES] = {
        .name = "postfix_queue_message_size_bytes",
        .type = METRIC_TYPE_GAUGE_HISTOGRAM,
        .help = "Size of messages in Postfix's message queue, in bytes.",
    },
    [FAM_POSTFIX_QUEUE_MESSAGE_AGE_SECONDS] = {
        .name = "postfix_queue_message_age_seconds",
        .type = METRIC_TYPE_GAUGE_HISTOGRAM,
        .help = "Age of messages in Postfix's message queue, in seconds.",
    }
};

static void postfix_counter_tree_free(c_avl_tree_t *tree)
{
    while (true) {
        char *name = NULL;
        uint64_t *counter = NULL;
        int status = c_avl_pick(tree, (void *)&name, (void *)&counter);
        if (status != 0)
            break;
        free(name);
        free(counter);
    }

    c_avl_destroy(tree);
}

static void postfix_counter_tree_metric_append(c_avl_tree_t *tree, metric_family_t *fam,
                                               label_set_t *labels, char *key)
{
    char *name = NULL;
    uint64_t *value;

    c_avl_iterator_t *iter = c_avl_get_iterator(tree);
    if (iter != NULL) {
        while (c_avl_iterator_next(iter, (void *)&name, (void *)&value) == 0) {
            metric_family_append(fam, VALUE_COUNTER((*value)), labels,
                                 &(label_pair_const_t){.name=key, .value=name}, NULL);
        }
        c_avl_iterator_destroy(iter);
    }
}

static int postfix_counter_tree_inc(c_avl_tree_t *tree, char *name)
{
    uint64_t *counter;

    int status = c_avl_get(tree, name, (void *)&counter);
    if (status == 0) {
        assert(counter != NULL);
        (*counter)++;
        return 0;
    }

    char *dup_name = strdup(name);
    if (dup_name == NULL) {
        PLUGIN_ERROR("strdup failed");
        return -1;
    }

    counter = calloc(1, sizeof(*counter));
    if (counter == NULL) {
        PLUGIN_ERROR("calloc failed");
        free(dup_name);
        return -1;
    }

    *counter = 1;

    status = c_avl_insert(tree, dup_name, counter);
    if (status != 0) {
        free(dup_name);
        free(counter);
        return -1;
    }

    return 0;
}

static void postfix_histogram_tree_free(c_avl_tree_t *tree)
{
    while (true) {
        char *name = NULL;
        histogram_t *histogram = NULL;
        int status = c_avl_pick(tree, (void *)&name, (void *)&histogram);
        if (status != 0)
            break;
        free(name);
        histogram_reset(histogram);
        free(histogram);
    }

    c_avl_destroy(tree);
}

static void postfix_histogram_tree_metric_append(c_avl_tree_t *tree, metric_family_t *fam,
                                                label_set_t *labels, char *key1, char *key2,
                                                char *name2)
{
    char *name = NULL;
    histogram_t *histogram;

    c_avl_iterator_t *iter = c_avl_get_iterator(tree);
    if (iter != NULL) {
        while (c_avl_iterator_next(iter, (void *)&name, (void *)&histogram) == 0) {
            metric_family_append(fam, VALUE_HISTOGRAM(histogram), labels,
                                 &(label_pair_const_t){.name=key1, .value=name},
                                 &(label_pair_const_t){.name=key2, .value=name2}, NULL);
        }
        c_avl_iterator_destroy(iter);
    }
}

static int postfix_histogram_tree_add(c_avl_tree_t *tree, char *name,
                                      postfix_buckets_t *buckets, char *str)
{
    histogram_t *histogram;

    double value = 0;
    if (strtodouble(str, &value) != 0)
        return -1;

    int status = c_avl_get(tree, name, (void *)&histogram);
    if (status == 0) {
        assert(histogram != NULL);
        status = histogram_update(histogram, value);
        if (status != 0)
            PLUGIN_WARNING("histogram_update failed");
        return 0;
    }

    char *dup_name = strdup(name);
    if (dup_name == NULL) {
        PLUGIN_ERROR("strdup failed");
        return -1;
    }

    histogram = histogram_new_custom(buckets->size, buckets->buckets);
    if (histogram == NULL) {
        PLUGIN_ERROR("histogram_new_custom failed");
        free(dup_name);
        return -1;
    }

    status = histogram_update(histogram, value);
    if (status != 0)
        PLUGIN_WARNING("histogram_update failed");

    status = c_avl_insert(tree, dup_name, histogram);
    if (status != 0) {
        free(dup_name);
        histogram_reset(histogram);
        free(histogram);
        return -1;
    }

    return 0;
}

static int postfix_showq_parse(postfix_ctx_t *ctx)
{
    typedef enum {
        QUEUE_UNKNOW   = -1,
        QUEUE_ACTIVE   = 0,
        QUEUE_DEFERRED = 1,
        QUEUE_HOLD     = 2,
        QUEUE_INCOMING = 3,
        QUEUE_MAILDROP = 4,
        QUEUE_MAX      = 5,
    } queue_t;

    static char *queue_name[QUEUE_MAX] = {
        [QUEUE_ACTIVE] = "active",
        [QUEUE_DEFERRED] ="deferred",
        [QUEUE_HOLD] = "hold",
        [QUEUE_INCOMING] = "incoming",
        [QUEUE_MAILDROP] = "maildrop",
    };

    char *key = NULL;
    size_t key_size = 256;
    char *value = NULL;
    size_t value_size = 4096;
    int status = 0;

    histogram_t *queue_msg_size[QUEUE_MAX] = { 0 };
    histogram_t *queue_msg_age[QUEUE_MAX] = { 0 };
    uint64_t queue_size[QUEUE_MAX] = { 0 };

    int fd = socket_connect_unix_stream(ctx->showq_path, ctx->timeout);
    if (fd < 0)
        return -1;

    FILE *fh = fdopen(fd, "r");
    if (fh == NULL) {
        close(fd);
        return -1;
    }

    for (size_t i = 0; i < QUEUE_MAX; i++) {
        queue_msg_size[i] = histogram_new_custom(ctx->buckets_queue_size.size,
                                                 ctx->buckets_queue_size.buckets);
        queue_msg_age[i] = histogram_new_custom(ctx->buckets_queue_age.size,
                                                ctx->buckets_queue_age.buckets);
        if ((queue_msg_size[i] == NULL) || (queue_msg_age[i] == NULL)) {
            status = -1;
            goto error;
        }
    }

    key = malloc(sizeof(*key)*key_size);
    if (key == NULL) {
        PLUGIN_ERROR("malloc failed.");
        status = -1;
        goto error;
    }

    value = malloc(sizeof(*value)*value_size);
    if (value == NULL) {
        PLUGIN_ERROR("malloc failed.");
        status = -1;
        goto error;
    }

    double now = CDTIME_T_TO_DOUBLE(cdtime());
    queue_t queue = QUEUE_UNKNOW;

    while (true) {
        if (feof(fh))
            break;
        ssize_t key_len = getdelim(&key, &key_size, '\0', fh);
        if ((key_len == 0) || (key_len == 1)) {
            queue = QUEUE_UNKNOW;
            continue;
        }

        if (feof(fh))
            break;

        ssize_t value_len = getdelim(&value, &value_size, '\0', fh);
        if ((value_len == 0) || (value_len == 1))
            continue;

        if (strcmp(key, "queue_name") == 0) {
            if (strcmp(value, queue_name[QUEUE_ACTIVE]) == 0) {
                queue = QUEUE_ACTIVE;
                queue_size[queue]++;
            } else if (strcmp(value, queue_name[QUEUE_DEFERRED]) == 0) {
                queue = QUEUE_DEFERRED;
                queue_size[queue]++;
            } else if (strcmp(value, queue_name[QUEUE_HOLD]) == 0) {
                queue = QUEUE_HOLD;
                queue_size[queue]++;
            } else if (strcmp(value, queue_name[QUEUE_INCOMING]) == 0) {
                queue = QUEUE_INCOMING;
                queue_size[queue]++;
            } else if (strcmp(value, queue_name[QUEUE_MAILDROP]) == 0) {
                queue = QUEUE_MAILDROP;
                queue_size[queue]++;
            } else {
                queue = QUEUE_UNKNOW;
            }
        } else if (strcmp(key, "size") == 0) {
            double gauge = 0;
            if (strtodouble(value, &gauge) == 0)
                histogram_update(queue_msg_size[queue], gauge);
        } else if (strcmp(key, "time") == 0) {
            double gauge = 0;
            if (strtodouble(value, &gauge) == 0) {
                double wait = now - gauge;
                if (wait < 0)
                    wait = 0;
                histogram_update(queue_msg_age[queue], wait);
            }
        }
    }

    for (size_t i = 0; i < QUEUE_MAX; i++) {
        metric_family_append(&ctx->fams[FAM_POSTFIX_QUEUE_SIZE],
                             VALUE_GAUGE(queue_size[i]), &ctx->labels,
                             &(label_pair_const_t){.name="queue", .value=queue_name[i]},
                             NULL);
        metric_family_append(&ctx->fams[FAM_POSTFIX_QUEUE_MESSAGE_SIZE_BYTES],
                             VALUE_HISTOGRAM(queue_msg_size[i]), &ctx->labels,
                             &(label_pair_const_t){.name="queue", .value=queue_name[i]},
                             NULL);
        metric_family_append(&ctx->fams[FAM_POSTFIX_QUEUE_MESSAGE_AGE_SECONDS],
                             VALUE_HISTOGRAM(queue_msg_age[i]), &ctx->labels,
                             &(label_pair_const_t){.name="queue", .value=queue_name[i]},
                             NULL);
    }

error:
    free(key);
    free(value);
    fclose(fh);
    for (size_t i = 0; i < QUEUE_MAX; i++) {
        histogram_destroy(queue_msg_size[i]);
        histogram_destroy(queue_msg_age[i]);
    }
    return status;
}

static char *postfix_regmatch(regmatch_t *match, char *line, int len)
{
    if ((match->rm_so < 0) || (match->rm_eo < 0))
        return NULL;
    if (match->rm_so >= match->rm_eo)
        return NULL;
    if (match->rm_eo > (len + 1))
        return NULL;

    char *str = line + match->rm_so;
    str[match->rm_eo-match->rm_so] = '\0';
    return str;
}

static int postfix_log_parse_cleanup(postfix_ctx_t *ctx, char *message,
                                     __attribute__((unused)) size_t message_len)
{

    if (strstr(message, ": message-id=<") != NULL) {
        ctx->stats.cleanup_processes++;
    } else if (strstr(message, ": reject: ") != NULL) {
        ctx->stats.cleanup_rejects++;
    } else if (strstr(message, "message not accepted") != NULL) {
        ctx->stats.cleanup_not_accepted++;
    }

    return 0;
}

static int postfix_log_parse_lmtp(postfix_ctx_t *ctx, char *message, size_t message_len)
{
    regmatch_t match[7] = {0};
    size_t match_size = STATIC_ARRAY_SIZE(match);

    int status = regexec(&ctx->preg[POSTFIX_REGEX_LMTP_PIPE_SMTP], message, match_size, match, 0);
    if (status != 0)
        return 0;

    char *before_queue_manager = postfix_regmatch(&match[2], message, message_len);
    if (before_queue_manager != NULL) {
        double value = 0;
        if (strtodouble(before_queue_manager, &value) == 0)
            histogram_update(ctx->stats.lmtp_before_queue_manager_delays, value);
    }

    char *queue_manager = postfix_regmatch(&match[3], message, message_len);
    if (queue_manager != NULL) {
        double value = 0;
        if (strtodouble(queue_manager, &value) == 0)
            histogram_update(ctx->stats.lmtp_queue_manager_delays, value);
    }

    char *connection_setup = postfix_regmatch(&match[4], message, message_len);
    if (connection_setup != NULL) {
        double value = 0;
        if (strtodouble(connection_setup, &value) == 0)
            histogram_update(ctx->stats.lmtp_connection_setup_delays, value);
    }

    char *transmission = postfix_regmatch(&match[5], message, message_len);
    if (transmission != NULL) {
        double value = 0;
        if (strtodouble(transmission, &value) == 0)
            histogram_update(ctx->stats.lmtp_transmission_delays, value);
    }
    return 0;
}

static int postfix_log_parse_pipe(postfix_ctx_t *ctx, char *message, size_t message_len)
{
    regmatch_t match[7] = {0};
    size_t match_size = STATIC_ARRAY_SIZE(match);

    int status = regexec(&ctx->preg[POSTFIX_REGEX_LMTP_PIPE_SMTP], message, match_size, match, 0);
    if (status != 0)
        return 0;

    char *relay = postfix_regmatch(&match[1], message, message_len);
    if (relay == NULL)
        return -1;

    char *before_queue_manager = postfix_regmatch(&match[2], message, message_len);
    if (before_queue_manager != NULL)
        postfix_histogram_tree_add(ctx->stats.pipe_before_queue_manager_delays, relay,
                                   &ctx->buckets_time, before_queue_manager);

    char *queue_manager = postfix_regmatch(&match[3], message, message_len);
    if (queue_manager != NULL)
        postfix_histogram_tree_add(ctx->stats.pipe_queue_manager_delays, relay,
                                   &ctx->buckets_time, queue_manager);

    char *connection_setup = postfix_regmatch(&match[4], message, message_len);
    if (connection_setup != NULL)
        postfix_histogram_tree_add(ctx->stats.pipe_connection_setup_delays, relay,
                                   &ctx->buckets_time, connection_setup);

    char *transmission = postfix_regmatch(&match[5], message, message_len);
    if (transmission != NULL)
        postfix_histogram_tree_add(ctx->stats.pipe_transmission_delays, relay,
                                   &ctx->buckets_time, transmission);

    return 0;
}

static int postfix_log_parse_qmgr(postfix_ctx_t *ctx, char *message, size_t message_len)
{
    regmatch_t match[3] = {0};
    size_t match_size = STATIC_ARRAY_SIZE(match);

    if (regexec(&ctx->preg[POSTFIX_REGEX_QMGR_INSERT], message, match_size, match, 0) == 0) {
        char *size = postfix_regmatch(&match[1], message, message_len);
        if (size == NULL) {
            double value = 0;
            if (strtodouble(size, &value) == 0)
                histogram_update(ctx->stats.qmgr_inserts_size, value);
        }
        char *nrcpt = postfix_regmatch(&match[2], message, message_len);
        if (nrcpt == NULL) {
            double value = 0;
            if (strtodouble(nrcpt, &value) == 0)
                histogram_update(ctx->stats.qmgr_inserts_nrcpt, value);
        }

    } else if (strstr(message, ": removed") != NULL) {
         ctx->stats.qmgr_removes++;
    } else if (regexec(&ctx->preg[POSTFIX_REGEX_QMGR_EXPIRED], message, match_size, match, 0) == 0) {
         ctx->stats.qmgr_expires++;
    }

    return 0;
}

static int postfix_log_parse_smtp(postfix_ctx_t *ctx, char *message, size_t message_len)
{
    regmatch_t match[7] = {0};
    size_t match_size = STATIC_ARRAY_SIZE(match);

    if (regexec(&ctx->preg[POSTFIX_REGEX_LMTP_PIPE_SMTP], message, match_size, match, 0) == 0) {
        char *before_queue_manager = postfix_regmatch(&match[2], message, message_len);
        if (before_queue_manager != NULL) {
            double value = 0;
            if (strtodouble(before_queue_manager, &value) == 0)
                histogram_update(ctx->stats.smtp_before_queue_manager_delays, value);
        }

        char *queue_manager = postfix_regmatch(&match[3], message, message_len);
        if (queue_manager != NULL) {
            double value = 0;
            if (strtodouble(queue_manager, &value) == 0)
                histogram_update(ctx->stats.smtp_queue_manager_delays, value);
        }

        char *connection_setup = postfix_regmatch(&match[4], message, message_len);
        if (connection_setup != NULL) {
            double value = 0;
            if (strtodouble(connection_setup, &value) == 0)
                histogram_update(ctx->stats.smtp_connection_setup_delays, value);
        }

        char *transmission = postfix_regmatch(&match[5], message, message_len);
        if (transmission != NULL) {
            double value = 0;
            if (strtodouble(transmission, &value) == 0)
                histogram_update(ctx->stats.smtp_transmission_delays, value);
        }

        char *remain = postfix_regmatch(&match[6], message, message_len);
        if (remain == NULL)
            return 0;

        size_t remain_len = strlen(remain);

        if (regexec(&ctx->preg[POSTFIX_REGEX_SMTP_STATUS], remain, 1, match, 0) == 0) {
            char *status = postfix_regmatch(&match[1], remain, remain_len);
            if (status != 0)
                postfix_counter_tree_inc(ctx->stats.smtp_processed, status);
        }
    } else if (regexec(&ctx->preg[POSTFIX_REGEX_SMTP_TLS], message, match_size, match, 0) == 0) {
        ctx->stats.smtp_tls_connects++;
    } else if (regexec(&ctx->preg[POSTFIX_REGEX_SMTP_CONNECTION_TIMEDOUT], message,
                       match_size, match, 0) == 0) {
        ctx->stats.smtp_connection_timedout++;
    }

    return 0;
}

static int postfix_log_parse_smtpd(postfix_ctx_t *ctx, char *message, size_t message_len)
{
    regmatch_t match[2] = {0};
    size_t match_size = STATIC_ARRAY_SIZE(match);

    if (strstr(message, "connect from ") != NULL) {
        ctx->stats.smtpd_connects++;
    } else if (strstr(message, "disconnect from ") != NULL) {
        ctx->stats.smtpd_disconnects++;
    } else if (regexec(&ctx->preg[POSTFIX_REGEX_SMTPD_FCRDNS_ERRORS] , message,
                       match_size, match, 0) == 0) {
        ctx->stats.smtpd_fcr_dns_errors++;
    } else if (regexec(&ctx->preg[POSTFIX_REGEX_SMTPD_LOST_CONNECTION] , message,
                       match_size, match, 0) == 0) {
        char *after_stage = postfix_regmatch(&match[1], message, message_len);
        if (after_stage != NULL)
            postfix_counter_tree_inc(ctx->stats.smtpd_lost_connections, after_stage);
    } else if (regexec(&ctx->preg[POSTFIX_REGEX_SMTPD_PROCESSES_SASL] , message,
                       match_size, match, 0) == 0) {
        ctx->stats.smtpd_msg_processed++;
        char *sasl_method = postfix_regmatch(&match[1], message, message_len);
        if (sasl_method != NULL)
            postfix_counter_tree_inc(ctx->stats.smtpd_sasl_msg_processed, sasl_method);
    } else if (strstr(message, ": client=") != NULL) {
        ctx->stats.smtpd_msg_processed++;
    } else if (regexec(&ctx->preg[POSTFIX_REGEX_SMTPD_REJECTS], message,
                       match_size, match, 0) == 0) {
        char *code = postfix_regmatch(&match[1], message, message_len);
        if (code != NULL)
            postfix_counter_tree_inc(ctx->stats.smtpd_rejects, code);
    } else if (regexec(&ctx->preg[POSTFIX_REGEX_SMTPD_SASL_AUTH_FAILURES] , message,
                       match_size, match, 0) == 0) {
        ctx->stats.smtpd_sasl_auth_failures++;
    } else if (regexec(&ctx->preg[POSTFIX_REGEX_SMTPD_TLS] , message, match_size, match, 0) == 0) {
        ctx->stats.smtpd_tls_connects++;
    }
    return 0;
}

static int postfix_log_parse_bounce(postfix_ctx_t *ctx, char *message,
                                    __attribute__((unused)) size_t message_len)
{
    if (strstr(message, ": sender non-delivery notification: ") == 0) {
        ctx->stats.bounce_non_delivery++;
    }

    return 0;
}

static int postfix_log_parse_virtual(postfix_ctx_t *ctx, char *message,
                                     __attribute__((unused)) size_t message_len)
{
    if (strstr(message, ", status=sent (delivered to maildir)") != NULL) {
        ctx->stats.virtual_delivered++;
    }

    return 0;
}

static int postfix_parse_log_line(postfix_ctx_t *ctx, char *subprocess,
                                  char *message, size_t message_len)
{
    int status = 0;

    if (strcmp(subprocess, "cleanup") == 0) {
        status = postfix_log_parse_cleanup(ctx, message, message_len);
    } else if (strcmp(subprocess, "lmtp") == 0) {
        status = postfix_log_parse_lmtp(ctx, message, message_len);
    } else if (strcmp(subprocess, "pipe") == 0) {
        status = postfix_log_parse_pipe(ctx, message, message_len);
    } else if (strcmp(subprocess, "qmgr") == 0) {
        status = postfix_log_parse_qmgr(ctx, message, message_len);
    } else if (strcmp(subprocess, "smtp") == 0) {
        status = postfix_log_parse_smtp(ctx, message, message_len);
    } else if (strcmp(subprocess, "smtpd") == 0) {
        status = postfix_log_parse_smtpd(ctx, message, message_len);
    } else if (strcmp(subprocess, "bounce") == 0) {
        status = postfix_log_parse_bounce(ctx, message, message_len);
    } else if (strcmp(subprocess, "virtual") == 0) {
        status = postfix_log_parse_virtual(ctx, message, message_len);
    }

    return status;
}

#ifdef HAVE_SD_JOURNAL
static size_t postfix_journal_get_data(sd_journal *j, char *label,
                                                      char *prefix, size_t prefix_len,
                                                      char *buffer, size_t buffer_len)
{
    const char *data;
    size_t data_len;

    int status = sd_journal_get_data(j, label, (const void **)&data, &data_len);
    if (status != 0)
        return 0;

    if (strncmp(prefix, data, prefix_len) != 0)
        return 0;
    data += prefix_len;
    data_len -= prefix_len;

    if (data_len > (buffer_len - 1))
        data_len = buffer_len - 1;
    memcpy(buffer, data, data_len);
    buffer[data_len] = '\0';

    return data_len;
}

static int postfix_read_journal(postfix_ctx_t *ctx)
{
    if (ctx->journal == NULL) {
        int status = sd_journal_open(&ctx->journal, SD_JOURNAL_LOCAL_ONLY);
        if (status != 0) {

        }

        char filter[256];
        ssnprintf(filter, sizeof(filter), "_SYSTEMD_UNIT=%s", ctx->unit);
        sd_journal_add_match(ctx->journal, filter, 0);

        status = sd_journal_seek_tail(ctx->journal);
        if (status != 0) {
            PLUGIN_ERROR("Failed to seek to the tail of the journal.");
        }

        sd_journal_previous(ctx->journal);
    }

    while (true) {
        int status = sd_journal_next(ctx->journal);
        if (status == 0)
            break;

        if (status < 0) {
            PLUGIN_ERROR("Failed to read the next message in the journal.");
            break;
        }

        char message[4096];
        size_t message_len = postfix_journal_get_data(ctx->journal, "MESSAGE",
                                                      "MESSAGE=", strlen("MESSAGE="),
                                                       message, sizeof(message));
        if (message_len == 0)
            continue;

        char subprocess[256];
        size_t subprocess_len = postfix_journal_get_data(ctx->journal, "SYSLOG_IDENTIFIER",
                                                         "SYSLOG_IDENTIFIER=postfix/",
                                                         strlen("SYSLOG_IDENTIFIER=postfix/"),
                                                         subprocess, sizeof(subprocess));
        if (subprocess_len == 0)
            continue;

        postfix_parse_log_line(ctx, subprocess, message, message_len);
    }

    return 0;
}
#endif

static int postfix_read_log(postfix_ctx_t *ctx)
{
    while (true) {
        char buf[8192];
        int status = tail_readline(&ctx->tail, buf, sizeof(buf));
        if (status != 0) {
            PLUGIN_ERROR("File '%s': tail_readline failed with status %i.",
                         ctx->tail.file, status);
            return -1;
        }

        /* check for EOF */
        if (buf[0] == 0)
          break;

        size_t len = strlen(buf);
        while (len > 0) {
            if (buf[len - 1] != '\n')
                break;
            buf[len - 1] = '\0';
            len--;
        }

        if (len == 0)
            continue;

        regmatch_t match[6] = {0};
        size_t match_size = STATIC_ARRAY_SIZE(match);

        status = regexec(&ctx->preg[POSTFIX_REGEX_LOG], buf, match_size, match, 0);
        if (status != 0)
            return 0;

        char *subprocess = postfix_regmatch(&match[2], buf, len);
        if (subprocess == NULL)
            return -1;

        char *message = postfix_regmatch(&match[5], buf, len);
        if (message == NULL)
            return -1;

        size_t message_len = strlen(message);

        postfix_parse_log_line(ctx, subprocess, message, message_len);
    }

    return 0;
}

static int postfix_read(user_data_t *user_data)
{
    postfix_ctx_t *ctx = user_data->data;

    cdtime_t submit = cdtime();

    postfix_showq_parse(ctx);

    if (ctx->log_from == POSTFIX_LOG_FROM_FILE) {
        postfix_read_log(ctx);
    } else {
#ifdef HAVE_SD_JOURNAL
        postfix_read_journal(ctx);
#endif
    }

    metric_family_append(&ctx->fams[FAM_POSTFIX_LMTP_DELIVERY_DELAY_SECONDS],
                         VALUE_HISTOGRAM(ctx->stats.lmtp_before_queue_manager_delays), &ctx->labels,
                         &(label_pair_const_t){.name="stage", .value="before_queue_manager"}, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_LMTP_DELIVERY_DELAY_SECONDS],
                         VALUE_HISTOGRAM(ctx->stats.lmtp_queue_manager_delays), &ctx->labels,
                         &(label_pair_const_t){.name="stage", .value="queue_manager"}, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_LMTP_DELIVERY_DELAY_SECONDS],
                         VALUE_HISTOGRAM(ctx->stats.lmtp_connection_setup_delays), &ctx->labels,
                         &(label_pair_const_t){.name="stage", .value="connection_setup"}, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_LMTP_DELIVERY_DELAY_SECONDS],
                         VALUE_HISTOGRAM(ctx->stats.lmtp_transmission_delays), &ctx->labels,
                         &(label_pair_const_t){.name="stage", .value="transmission"}, NULL);

    postfix_histogram_tree_metric_append(ctx->stats.pipe_before_queue_manager_delays,
                                         &ctx->fams[FAM_POSTFIX_PIPE_DELIVERY_DELAY_SECONDS],
                                         &ctx->labels, "relay", "stage", "before_queue_manager");
    postfix_histogram_tree_metric_append(ctx->stats.pipe_queue_manager_delays,
                                         &ctx->fams[FAM_POSTFIX_PIPE_DELIVERY_DELAY_SECONDS],
                                         &ctx->labels, "relay", "stage", "queue_manager");
    postfix_histogram_tree_metric_append(ctx->stats.pipe_connection_setup_delays,
                                         &ctx->fams[FAM_POSTFIX_PIPE_DELIVERY_DELAY_SECONDS],
                                         &ctx->labels, "relay", "stage", "connection_setup");
    postfix_histogram_tree_metric_append(ctx->stats.pipe_transmission_delays,
                                         &ctx->fams[FAM_POSTFIX_PIPE_DELIVERY_DELAY_SECONDS],
                                         &ctx->labels, "relay", "stage", "transmission");

    metric_family_append(&ctx->fams[FAM_POSTFIX_SMTP_DELIVERY_DELAY_SECONDS],
                         VALUE_HISTOGRAM(ctx->stats.smtp_before_queue_manager_delays), &ctx->labels,
                         &(label_pair_const_t){.name="stage", .value="before_queue_manager"}, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_SMTP_DELIVERY_DELAY_SECONDS],
                         VALUE_HISTOGRAM(ctx->stats.smtp_queue_manager_delays), &ctx->labels,
                         &(label_pair_const_t){.name="stage", .value="queue_manager"}, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_SMTP_DELIVERY_DELAY_SECONDS],
                         VALUE_HISTOGRAM(ctx->stats.smtp_connection_setup_delays), &ctx->labels,
                         &(label_pair_const_t){.name="stage", .value="connection_setup"}, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_SMTP_DELIVERY_DELAY_SECONDS],
                         VALUE_HISTOGRAM(ctx->stats.smtp_transmission_delays), &ctx->labels,
                         &(label_pair_const_t){.name="stage", .value="transmission"}, NULL);

    postfix_counter_tree_metric_append(ctx->stats.smtp_processed,
                                       &ctx->fams[FAM_POSTFIX_SMTP_MESSAGES_PROCESSED],
                                       &ctx->labels, "status");
    postfix_counter_tree_metric_append(ctx->stats.smtpd_sasl_msg_processed,
                                       &ctx->fams[FAM_POSTFIX_SMTP_SASL_MESSAGES_PROCESSED],
                                       &ctx->labels, "sasl_method");
    postfix_counter_tree_metric_append(ctx->stats.smtpd_lost_connections,
                                       &ctx->fams[FAM_POSTFIX_SMTPD_CONNECTIONS_LOST],
                                       &ctx->labels, "after_stage");
    postfix_counter_tree_metric_append(ctx->stats.smtpd_rejects,
                                       &ctx->fams[FAM_POSTFIX_SMTPD_MESSAGES_REJECTED],
                                       &ctx->labels, "code");

    metric_family_append(&ctx->fams[FAM_POSTFIX_QMGR_MESSAGES_INSERTED_RECEIPIENTS],
                         VALUE_HISTOGRAM(ctx->stats.qmgr_inserts_nrcpt), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_QMGR_MESSAGES_INSERTED_SIZE_BYTES],
                         VALUE_HISTOGRAM(ctx->stats.qmgr_inserts_size), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_CLEANUP_MESSAGES_PROCESSED],
                         VALUE_COUNTER(ctx->stats.cleanup_processes), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_CLEANUP_MESSAGES_REJECTED],
                         VALUE_COUNTER(ctx->stats.cleanup_rejects), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_CLEANUP_MESSAGES_NOT_ACCEPTED],
                         VALUE_COUNTER(ctx->stats.cleanup_not_accepted), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_QMGR_MESSAGES_REMOVED],
                         VALUE_COUNTER(ctx->stats.qmgr_removes), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_QMGR_MESSAGES_EXPIRED],
                         VALUE_COUNTER(ctx->stats.qmgr_expires), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_SMTP_TLS_CONNECTIONS],
                         VALUE_COUNTER(ctx->stats.smtp_tls_connects), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_SMTP_CONNECTION_TIMED_OUT],
                         VALUE_COUNTER(ctx->stats.smtp_connection_timedout), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_SMTPD_CONNECTS],
                         VALUE_COUNTER(ctx->stats.smtpd_connects), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_SMTPD_DISCONNECTS],
                         VALUE_COUNTER(ctx->stats.smtpd_disconnects), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_SMTPD_FORWARD_CONFIRMED_REVERSE_DNS_ERRORS],
                         VALUE_COUNTER(ctx->stats.smtpd_fcr_dns_errors), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_SMTPD_MESSAGES_PROCESSED],
                         VALUE_COUNTER(ctx->stats.smtpd_msg_processed), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_SMTPD_SASL_AUTHENTICATION_FAILURES],
                         VALUE_COUNTER(ctx->stats.smtpd_sasl_auth_failures), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_SMTPD_TLS_CONNECTIONS],
                         VALUE_COUNTER(ctx->stats.smtpd_tls_connects), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_BOUNCE_NON_DELIVERY_NOTIFICATION],
                         VALUE_COUNTER(ctx->stats.bounce_non_delivery), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_POSTFIX_VIRTUAL_DELIVERED],
                         VALUE_COUNTER(ctx->stats.virtual_delivered), &ctx->labels, NULL);

    plugin_dispatch_metric_family_array_filtered(ctx->fams, FAM_POSTFIX_MAX, ctx->filter, submit);

    return 0;
}

static void postfix_ctx_free(void *arg)
{
    postfix_ctx_t *ctx = arg;

    if (ctx == NULL)
        return;

    free(ctx->name);
    free(ctx->log_path);
    free(ctx->showq_path);
    free(ctx->unit);

    tail_close(&ctx->tail);
#ifdef HAVE_SD_JOURNAL
    if (ctx->journal != NULL)
        sd_journal_close(ctx->journal);
#endif

    label_set_reset(&ctx->labels);
    plugin_filter_free(ctx->filter);

    regex_t preg_zero = {0};

    for (size_t i = 0; i < POSTFIX_REGEX_MAX; i++) {
        if (memcmp(&ctx->preg[i], &preg_zero, sizeof(regex_t)) != 0)
            regfree(&ctx->preg[i]);
    }

    postfix_counter_tree_free(ctx->stats.smtpd_rejects);
    postfix_counter_tree_free(ctx->stats.smtpd_lost_connections);
    postfix_counter_tree_free(ctx->stats.smtpd_sasl_msg_processed);
    postfix_counter_tree_free(ctx->stats.smtp_processed);
    histogram_destroy(ctx->stats.lmtp_before_queue_manager_delays);
    histogram_destroy(ctx->stats.lmtp_queue_manager_delays);
    histogram_destroy(ctx->stats.lmtp_connection_setup_delays);
    histogram_destroy(ctx->stats.lmtp_transmission_delays);
    postfix_histogram_tree_free(ctx->stats.pipe_before_queue_manager_delays);
    postfix_histogram_tree_free(ctx->stats.pipe_queue_manager_delays);
    postfix_histogram_tree_free(ctx->stats.pipe_connection_setup_delays);
    postfix_histogram_tree_free(ctx->stats.pipe_transmission_delays);
    histogram_destroy(ctx->stats.smtp_before_queue_manager_delays);
    histogram_destroy(ctx->stats.smtp_queue_manager_delays);
    histogram_destroy(ctx->stats.smtp_connection_setup_delays);
    histogram_destroy(ctx->stats.smtp_transmission_delays);

    histogram_destroy(ctx->stats.qmgr_inserts_nrcpt);
    histogram_destroy(ctx->stats.qmgr_inserts_size);

    free(ctx);
}

static postfix_ctx_t *postfix_ctx_alloc(void)
{
    postfix_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return NULL;
    }

    for (size_t i = 0; i < POSTFIX_REGEX_MAX; i++) {
        int status = regcomp(&ctx->preg[i], postfix_regex[i], REG_EXTENDED | REG_NEWLINE);
        if (status != 0) {
            PLUGIN_ERROR("Compiling the regular expression '%s' failed.", postfix_regex[i]);
            goto error;
        }
    }

    memcpy(ctx->fams, fams, sizeof(ctx->fams[0])*FAM_POSTFIX_MAX);

    c_avl_tree_t **trees[] = {
        &ctx->stats.smtpd_rejects,                    &ctx->stats.smtpd_lost_connections,
        &ctx->stats.smtpd_sasl_msg_processed,         &ctx->stats.smtp_processed,
        &ctx->stats.pipe_before_queue_manager_delays, &ctx->stats.pipe_queue_manager_delays,
        &ctx->stats.pipe_connection_setup_delays,     &ctx->stats.pipe_transmission_delays,
        NULL
    };

    for (size_t i = 0; trees[i] != NULL; i++) {
        *(trees[i]) = c_avl_create((int (*)(const void *, const void *))strcmp);
        if (*(trees[i]) == NULL)
            goto error;
    }

    return ctx;

error:
    postfix_ctx_free(ctx);
    return NULL;
}

static int postfix_buckets_copy(postfix_buckets_t *buckets, postfix_buckets_t *default_buckets)
{
    if (buckets->size != 0)
        return 0;

    buckets->buckets = calloc(default_buckets->size, sizeof(double));
    if (buckets->buckets == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }
    memcpy(buckets->buckets, default_buckets->buckets, default_buckets->size * sizeof(double));
    buckets->size = default_buckets->size;

    return 0;
}

static int postfix_config_instance (config_item_t *ci)
{
    postfix_ctx_t *ctx = postfix_ctx_alloc();
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }


    int status = cf_util_get_string(ci, &ctx->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        postfix_ctx_free(ctx);
        return status;
    }
    assert(ctx->name != NULL);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("unit", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->unit);
            ctx->log_from = POSTFIX_LOG_FROM_SD_JOURNAL;
        } else if (strcasecmp("log-path", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->log_path);
            ctx->log_from = POSTFIX_LOG_FROM_FILE;
        } else if (strcasecmp("log-from", child->key) == 0) {
            char *kind = NULL;
            status = cf_util_get_string(child, &kind);
            if (status == 0) {
                if (strcmp(kind, "file") == 0) {
                    ctx->log_from = POSTFIX_LOG_FROM_FILE;
                } else if (strcmp(kind, "systemd") == 0) {
                    ctx->log_from = POSTFIX_LOG_FROM_SD_JOURNAL;
                }
                free(kind);
            }
        } else if (strcasecmp("showq-path", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->showq_path);

        } else if (strcasecmp("histogram-time-buckets", child->key) == 0) {
            status = cf_util_get_double_array(child, &ctx->buckets_time.size,
                                                     &ctx->buckets_time.buckets);
        } else if (strcasecmp("histogram-queue-size-buckets", child->key) == 0) {
            status = cf_util_get_double_array(child, &ctx->buckets_queue_size.size,
                                                     &ctx->buckets_queue_size.buckets);
        } else if (strcasecmp("histogram-queue-age-buckets", child->key) == 0) {
            status = cf_util_get_double_array(child, &ctx->buckets_queue_age.size,
                                                     &ctx->buckets_queue_age.buckets);
        } else if (strcasecmp("histogram-qmgr-inserts-nrcpt-buckets", child->key) == 0) {
            status = cf_util_get_double_array(child, &ctx->buckets_qmgr_inserts_nrcpt.size,
                                                     &ctx->buckets_qmgr_inserts_nrcpt.buckets);
        } else if (strcasecmp("histogram-qmgr-inserts-size-buckets", child->key) == 0) {
            status = cf_util_get_double_array(child, &ctx->buckets_qmgr_inserts_size.size,
                                                     &ctx->buckets_qmgr_inserts_size.buckets);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &ctx->timeout);
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
        postfix_ctx_free(ctx);
        return -1;
    }

    if (ctx->unit == NULL) {
        ctx->unit = strdup(DEFAULT_SERVICE);
        if (ctx->unit == NULL) {
            PLUGIN_ERROR("strdup failed.");
            postfix_ctx_free(ctx);
            return -1;
        }
    }

    if (ctx->log_path == NULL) {
        ctx->log_path = strdup(DEFAULT_LOG_PATH);
        if (ctx->log_path == NULL) {
            PLUGIN_ERROR("strdup failed.");
            postfix_ctx_free(ctx);
            return -1;
        }
    }

    ctx->tail.file = ctx->log_path;

    if (ctx->showq_path == NULL) {
        ctx->showq_path = strdup(DEFAULT_SHOWQ_PATH);
        if (ctx->showq_path == NULL) {
            PLUGIN_ERROR("strdup failed.");
            postfix_ctx_free(ctx);
            return -1;
        }
    }

    struct {
        postfix_buckets_t *buckets;
        postfix_buckets_t *default_buckets;
    } buckets[] = {
        { &ctx->buckets_time,               &default_buckets_time               },
        { &ctx->buckets_queue_size,         &default_buckets_queue_size         },
        { &ctx->buckets_queue_age,          &default_buckets_queue_age          },
        { &ctx->buckets_qmgr_inserts_nrcpt, &default_buckets_qmgr_inserts_nrcpt },
        { &ctx->buckets_qmgr_inserts_size,  &default_buckets_qmgr_inserts_size  },
        { NULL,                             NULL                                }
    };

    for (size_t i = 0; buckets[i].buckets != 0; i++) {
        if (postfix_buckets_copy(buckets[i].buckets, buckets[i].default_buckets) != 0) {
            postfix_ctx_free(ctx);
            return -1;
        }
    }

    ctx->stats.qmgr_inserts_nrcpt = histogram_new_custom(ctx->buckets_qmgr_inserts_nrcpt.size,
                                                         ctx->buckets_qmgr_inserts_nrcpt.buckets);
    ctx->stats.qmgr_inserts_size = histogram_new_custom(ctx->buckets_qmgr_inserts_size.size,
                                                        ctx->buckets_qmgr_inserts_size.buckets);
    if ((ctx->stats.qmgr_inserts_nrcpt == NULL) || (ctx->stats.qmgr_inserts_size == NULL)) {
        postfix_ctx_free(ctx);
        return -1;
    }

    histogram_t **histograms[] = {
        &ctx->stats.lmtp_before_queue_manager_delays, &ctx->stats.lmtp_queue_manager_delays,
        &ctx->stats.lmtp_connection_setup_delays, &ctx->stats.lmtp_transmission_delays,
        &ctx->stats.smtp_before_queue_manager_delays, &ctx->stats.smtp_queue_manager_delays,
        &ctx->stats.smtp_connection_setup_delays, &ctx->stats.smtp_transmission_delays,
        NULL
    };

    for (size_t i = 0; histograms[i] != NULL; i++) {
        *(histograms[i]) = histogram_new_custom(ctx->buckets_time.size, ctx->buckets_time.buckets);
        if (*(histograms[i]) == NULL) {
            postfix_ctx_free(ctx);
            return -1;
        }
    }

    if (ctx->log_from == POSTFIX_LOG_FROM_NONE) {
        if (access("/run/systemd/system/", R_OK|X_OK) == 0) {
            ctx->log_from = POSTFIX_LOG_FROM_SD_JOURNAL;
        } else {
            ctx->log_from = POSTFIX_LOG_FROM_FILE;
        }
    }

    label_set_add(&ctx->labels, true, "instance", ctx->name);

    return plugin_register_complex_read("postfix", ctx->name, postfix_read, interval,
                                        &(user_data_t){.data = ctx, .free_func = postfix_ctx_free});
}

static int postfix_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = postfix_config_instance(child);
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
    plugin_register_config("postfix", postfix_config);
}
