// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "config.h"

#if defined(STRPTIME_NEEDS_STANDARDS) || defined(HAVE_STRPTIME_STANDARDS)
#ifndef _ISOC99_SOURCE
#define _ISOC99_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#endif

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"

#include <cmqc.h>
#include <cmqbc.h>
#include <cmqcfc.h>
#include <cmqstrc.h>
#include <cmqxc.h>

enum {
    FAM_MQ_UP,
    FAM_MQ_QUEUE_DEPTH,
    FAM_MQ_QUEUE_MAX_DEPTH,
    FAM_MQ_QUEUE_OPEN_INPUT,
    FAM_MQ_QUEUE_OPEN_OUTPUT,
    FAM_MQ_QUEUE_DEQUEUE,
    FAM_MQ_QUEUE_ENQUEUE,
    FAM_MQ_QUEUE_OLDEST_MSG_AGE_SECONDS,
    FAM_MQ_QUEUE_LATEST_GET_SECONDS,
    FAM_MQ_QUEUE_LATEST_PUT_SECONDS,
    FAM_MQ_MAX,
};

static metric_family_t fams[FAM_MQ_MAX] = {
    [FAM_MQ_UP] = {
        .name = "mq_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the mq broker be reached.",
    },
    [FAM_MQ_QUEUE_DEPTH] = {
        .name = "mq_queue_depth",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of messages on queue.",
    },
    [FAM_MQ_QUEUE_MAX_DEPTH] = {
        .name = "mq_queue_max_depth",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum number of messages allowed on queue.",
    },
    [FAM_MQ_QUEUE_OPEN_INPUT] = {
        .name = "mq_queue_open_input",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of MQOPEN calls that have the queue open for input.",
    },
    [FAM_MQ_QUEUE_OPEN_OUTPUT] = {
        .name = "mq_queue_open_output",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of MQOPEN calls that have the queue open for output.",
    },
    [FAM_MQ_QUEUE_DEQUEUE] = {
        .name = "mq_queue_dequeue",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of messages that have been successfully retrieved from the queue, "
                "even though the MQGET has not yet been committed.",
    },
    [FAM_MQ_QUEUE_ENQUEUE] = {
        .name = "mq_queue_enqueue",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of messages that were put on the queue, but have not yet been committed.",
    },
    [FAM_MQ_QUEUE_OLDEST_MSG_AGE_SECONDS] = {
        .name = "mq_queue_oldest_msg_age_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Age, in seconds, of the oldest message on the queue.",
    },
    [FAM_MQ_QUEUE_LATEST_GET_SECONDS] = {
        .name = "mq_queue_latest_get_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "The time, in seconds, at which the last message was successfully "
                "read from the queue.",
    },
    [FAM_MQ_QUEUE_LATEST_PUT_SECONDS] = {
        .name = "mq_queue_latest_put_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "The time, in seconds, at which the last message was successfully "
                "put to the queue",
    },
};

// In WebSphere MQ, names can have a maximum of 48 characters, with the
// exception of channels which have a maximum of 20 characters.
typedef struct {
    char *name;
    char *username;
    char *password;
    char *host;
    char *port;
    char *connection;
    char *qmanager;
    char *cchannel;
    label_set_t labels;
    metric_family_t fams[FAM_MQ_MAX];
    MQHCONN hdl;
} cmq_instance_t;

static const char *cmq_mqccstr(MQLONG mqcc)
{
    switch (mqcc) {
    case MQCC_OK:
        return "Ok";
    case MQCC_WARNING:
        return "Warning";
    case MQCC_FAILED:
        return "Failed";
    case MQCC_UNKNOWN:
        return "Unknow MQCC";
    }
    return "Unknow MQCC";
}

static void cmq_reason(char *msg, MQLONG mqcc, MQLONG mqrc)
{
    switch (mqcc) {
    case MQCC_OK:
        PLUGIN_INFO("%s %s: (%d) %s", msg, cmq_mqccstr(mqcc), mqrc, MQRC_STR(mqrc));
        break;
    case MQCC_WARNING:
        PLUGIN_WARNING("%s %s: (%d) %s", msg, cmq_mqccstr(mqcc), mqrc, MQRC_STR(mqrc));
        break;
    default:
        PLUGIN_ERROR("%s %s: (%d) %s", msg, cmq_mqccstr(mqcc), mqrc, MQRC_STR(mqrc));
        break;
    }
}

static void cmq_disconnect(cmq_instance_t *mq)
{
    MQLONG mqcc, mqrc;

    if (mq == NULL)
        return;

    if (mq->hdl == MQHC_UNUSABLE_HCONN)
        return;

    MQDISC(&(mq->hdl), &mqcc, &mqrc);
    if (mqcc != MQCC_OK)
        cmq_reason("MQDISC", mqcc, mqrc);

    mq->hdl = MQHC_UNUSABLE_HCONN;
}

static void cmq_instance_free(void *arg)
{
    cmq_instance_t *mq = arg;

    if (mq == NULL)
        return;

    cmq_disconnect(mq);

    free(mq->name);
    free(mq->username);
    free(mq->password);
    free(mq->host);
    free(mq->port);
    free(mq->connection);
    free(mq->qmanager);
    free(mq->cchannel);
    label_set_reset(&mq->labels);
    free(mq);
}

static int cmq_connect(cmq_instance_t *mq)
{
    MQLONG mqcc, mqrc;

    if (mq == NULL)
        return -1;

    MQCHAR qmgr[MQ_Q_MGR_NAME_LENGTH];
    if (mq->qmanager != NULL) {
        strncpy(qmgr, mq->qmanager, MQ_Q_MGR_NAME_LENGTH);
        qmgr[MQ_Q_MGR_NAME_LENGTH - 1] = '\0';
    } else {
        qmgr[0] = '\0'; /* default */
    }

    MQCNO mqco = {MQCNO_DEFAULT};
    MQCD mqcd = {MQCD_CLIENT_CONN_DEFAULT};
    MQCSP mqcsp = {MQCSP_DEFAULT};

    if (mq->connection != NULL) {
        strncpy(mqcd.ConnectionName, mq->connection, MQ_CONN_NAME_LENGTH);
        mqcd.ConnectionName[MQ_Q_MGR_NAME_LENGTH - 1] = '\0';

        char *cchannel = mq->cchannel;
        if (cchannel == NULL) {
            cchannel = "SYSTEM.DEF.SVRCONN";
        }

        strncpy(mqcd.ChannelName, cchannel, MQ_CHANNEL_NAME_LENGTH);
        mqcd.ChannelName[MQ_CHANNEL_NAME_LENGTH - 1] = '\0';

        mqco.ClientConnPtr = &mqcd;
        mqco.Version = MQCNO_VERSION_2;
    }

    mqco.Options = MQCNO_RECONNECT;

    if (mq->username != NULL) {
        mqco.SecurityParmsPtr = &mqcsp;
        mqco.Version = MQCNO_VERSION_5;
        mqcsp.AuthenticationType = MQCSP_AUTH_USER_ID_AND_PWD;
        mqcsp.CSPUserIdPtr = mq->username;
        mqcsp.CSPUserIdLength = strlen(mq->username);
        mqcsp.CSPPasswordPtr = mq->password;
        mqcsp.CSPPasswordLength = strlen(mq->password);
    }

    MQCONNX(qmgr, &mqco, &(mq->hdl), &mqcc, &mqrc);
    if (mqcc != MQCC_OK) {
        cmq_reason("MQCONNX", mqcc, mqrc);
        return -1;
    }

    return 0;
}

time_t cmq_queue_time(MQHBAG bag, MQLONG mq_date, MQLONG mq_time)
{
    MQLONG mqcc, mqrc;
    MQCHAR qdate[MQ_DATE_LENGTH+1] = {0};
    MQLONG qdate_len = 0;
    mqInquireString(bag, mq_date, 0, MQ_DATE_LENGTH, qdate, &qdate_len, NULL, &mqcc, &mqrc);
    if (mqcc == MQCC_OK) {
        mqTrim(MQ_DATE_LENGTH, qdate, qdate, &mqcc, &mqrc);
        MQCHAR qtime[MQ_TIME_LENGTH + 1] = {0};
        MQLONG qtime_len = 0;
        mqInquireString(bag, mq_time, 0, MQ_TIME_LENGTH, qtime, &qtime_len, NULL, &mqcc, &mqrc);
        if (mqcc == MQCC_OK) {
            mqTrim(MQ_TIME_LENGTH, qtime, qtime, &mqcc, &mqrc);

            char buffer[256];
            strbuf_t buf = STRBUF_CREATE_STATIC(buffer);
            strbuf_putstr(&buf, qdate);
            strbuf_putchar(&buf, ' ');
            strbuf_putstr(&buf, qtime);

            struct tm tm = {0};
            char *ret = strptime("%Y-%m-%d %H.%M.%S", buf.ptr, &tm);
            if (ret != NULL)
                return mktime(&tm);
       }
    }

    return 0;
}

static int cmq_queue_stats(cmq_instance_t *mq, MQHBAG bag)
{
    MQLONG mqcc, mqrc;

    MQCHAR qname[MQ_Q_NAME_LENGTH + 1];
    memset(qname, '\0', MQ_Q_NAME_LENGTH + 1);
    MQLONG qname_len;
    mqInquireString(bag, MQCA_Q_NAME, 0, MQ_Q_NAME_LENGTH, qname, &qname_len, NULL, &mqcc, &mqrc);
    if (mqcc != MQCC_OK) {
        cmq_reason("MQCA_Q_NAME", mqcc, mqrc);
        return -1;
    }
    qname[MQ_Q_NAME_LENGTH] = '\0';

    mqTrim(MQ_Q_NAME_LENGTH, qname, qname, &mqcc, &mqrc);

    if (strstr(qname, "SYSTEM") || strstr(qname, "MQAI"))
        return 0;

    MQLONG qtype;
    mqInquireInteger(bag, MQIA_Q_TYPE, MQIND_NONE, &qtype, &mqcc, &mqrc);
    if (mqcc != MQCC_OK) {
        cmq_reason("MQIA_Q_TYPE", mqcc, mqrc);
        return -1;
    }

    /* only local queues */
    if (qtype != 1)
        return 0;

    MQLONG depth = 0;
    mqInquireInteger(bag, MQIA_CURRENT_Q_DEPTH, MQIND_NONE, &depth, &mqcc, &mqrc);
    if (mqcc == MQCC_OK) {
        metric_family_append(&mq->fams[FAM_MQ_QUEUE_DEPTH],
                             VALUE_GAUGE(depth), &mq->labels,
                             &(label_pair_const_t){.name="queue", .value=qname}, NULL);
    } else {
        // cmq_reason("MQIA_CURRENT_Q_DEPTH", mqcc, mqrc);
    }

    MQLONG maxdepth = 0;
    mqInquireInteger(bag, MQIA_MAX_Q_DEPTH, MQIND_NONE, &maxdepth, &mqcc, &mqrc);
    if (mqcc == MQCC_OK) {
        metric_family_append(&mq->fams[FAM_MQ_QUEUE_MAX_DEPTH],
                             VALUE_GAUGE(maxdepth), &mq->labels,
                             &(label_pair_const_t){.name="queue", .value=qname}, NULL);
    } else {
        // cmq_reason("MQIA_MAX_Q_DEPTH", mqcc, mqrc);
    }

    MQLONG openinput = 0;
    mqInquireInteger(bag, MQIA_OPEN_INPUT_COUNT, MQIND_NONE, &openinput, &mqcc, &mqrc);
    if (mqcc == MQCC_OK) {
        metric_family_append(&mq->fams[FAM_MQ_QUEUE_OPEN_INPUT],
                             VALUE_COUNTER(openinput), &mq->labels,
                             &(label_pair_const_t){.name="queue", .value=qname}, NULL);
    } else {
        // cmq_reason("MQIA_OPEN_INPUT_COUNT", mqcc, mqrc);
    }

    MQLONG openoutput = 0;
    mqInquireInteger(bag, MQIA_OPEN_OUTPUT_COUNT, MQIND_NONE, &openoutput, &mqcc, &mqrc);
    if (mqcc == MQCC_OK) {
        metric_family_append(&mq->fams[FAM_MQ_QUEUE_OPEN_OUTPUT],
                             VALUE_COUNTER(openoutput), &mq->labels,
                             &(label_pair_const_t){.name="queue", .value=qname}, NULL);
    } else {
        // cmq_reason("MQIA_OPEN_OUTPUT_COUNT", mqcc, mqrc);
    }

    MQLONG dequeue = 0;
    mqInquireInteger(bag, MQIA_MSG_DEQ_COUNT, MQIND_NONE, &dequeue, &mqcc, &mqrc);
    if (mqcc == MQCC_OK) {
        metric_family_append(&mq->fams[FAM_MQ_QUEUE_DEQUEUE],
                             VALUE_COUNTER(dequeue), &mq->labels,
                             &(label_pair_const_t){.name="queue", .value=qname}, NULL);
    } else {
        // cmq_reason("MQIA_MSG_DEQ_COUNT", mqcc, mqrc);
    }

    MQLONG enqueue = 0;
    mqInquireInteger(bag, MQIA_MSG_ENQ_COUNT, MQIND_NONE, &enqueue, &mqcc, &mqrc);
    if (mqcc == MQCC_OK) {
        metric_family_append(&mq->fams[FAM_MQ_QUEUE_ENQUEUE],
                             VALUE_COUNTER(enqueue), &mq->labels,
                             &(label_pair_const_t){.name="queue", .value=qname}, NULL);
    } else {
        // cmq_reason("MQIA_MSG_ENQ_COUNT", mqcc, mqrc);
    }

    time_t lastget = cmq_queue_time(bag, MQCACF_LAST_GET_DATE, MQCACF_LAST_GET_TIME);
    metric_family_append(&mq->fams[FAM_MQ_QUEUE_LATEST_GET_SECONDS],
                         VALUE_GAUGE(lastget), &mq->labels,
                         &(label_pair_const_t){.name="queue", .value=qname}, NULL);

    time_t lastput = cmq_queue_time(bag, MQCACF_LAST_PUT_TIME, MQCACF_LAST_PUT_TIME);
    metric_family_append(&mq->fams[FAM_MQ_QUEUE_LATEST_PUT_SECONDS],
                         VALUE_GAUGE(lastput), &mq->labels,
                         &(label_pair_const_t){.name="queue", .value=qname}, NULL);

    MQLONG oldest_msg_age = 0;
    mqInquireInteger(bag, MQIACF_OLDEST_MSG_AGE, MQIND_NONE, &enqueue, &mqcc, &mqrc);
    if (mqcc == MQCC_OK) {
        metric_family_append(&mq->fams[FAM_MQ_QUEUE_OLDEST_MSG_AGE_SECONDS],
                             VALUE_GAUGE(oldest_msg_age), &mq->labels,
                             &(label_pair_const_t){.name="queue", .value=qname}, NULL);
    } else {
        // cmq_reason("MQIACF_OLDEST_MSG_AGE", mqcc, mqrc);
    }

    return 0;
}

static int cmq_queue_list(cmq_instance_t *mq)
{
    MQHBAG reqbag = MQHB_UNUSABLE_HBAG;
    MQHBAG respbag = MQHB_UNUSABLE_HBAG;
    MQLONG mqcc, mqrc;
    int rcode = -1;

    /* create an admin bag */
    mqCreateBag(MQCBO_ADMIN_BAG, &reqbag, &mqcc, &mqrc);
    if (mqcc != MQCC_OK) {
        cmq_reason("mqCreateBag MQCBO_ADMIN_BAG", mqcc, mqrc);
        goto error;
    }
    /* create ain admin response bag */
    mqCreateBag(MQCBO_ADMIN_BAG, &respbag, &mqcc, &mqrc);
    if (mqcc != MQCC_OK) {
        cmq_reason("mqCreateBag MQCBO_ADMIN_BAG", mqcc, mqrc);
        goto error;
    }
    /* get queue name */
    mqAddString(reqbag, MQCA_Q_NAME, MQBL_NULL_TERMINATED, "*", &mqcc, &mqrc);
    if (mqcc != MQCC_OK) {
        cmq_reason("mqAddString MQCA_Q_NAME", mqcc, mqrc);
        goto error;
    }
    /* get queue type */
    mqAddInteger(reqbag, MQIA_Q_TYPE, MQQT_ALL, &mqcc, &mqrc);
    if (mqcc != MQCC_OK) {
        cmq_reason("mqAddInteger MQIA_Q_TYPE", mqcc, mqrc);
        goto error;
    }
    // MQCMD_INQUIRE_Q_STATUS
    mqExecute(mq->hdl, MQCMD_INQUIRE_Q, MQHB_NONE, reqbag, respbag, MQHO_NONE,
                       MQHO_NONE, &mqcc, &mqrc);
    if (mqrc == MQRC_CMD_SERVER_NOT_AVAILABLE) {
        PLUGIN_ERROR("server %s not available", mq->name);
        cmq_disconnect(mq);
        goto error;
    }

    if (mqcc == MQCC_OK) {
        MQLONG nbags;
        MQHBAG attrsbag = MQHB_UNUSABLE_HBAG;

        mqCountItems(respbag, MQHA_BAG_HANDLE, &nbags, &mqcc, &mqrc);
        if (mqcc != MQCC_OK) {
            cmq_reason("mqCountItems", mqcc, mqrc);
            goto error;
        }

        for (int i = 0; i < nbags; i++) {
            mqInquireBag(respbag, MQHA_BAG_HANDLE, i, &attrsbag, &mqcc, &mqrc);
            if (mqcc != MQCC_OK) {
                cmq_reason("mqInquireBag MQHA_BAG_HANDLE", mqcc, mqrc);
                goto error;
            }
            cmq_queue_stats(mq, attrsbag);
        }
    } else {
        if (mqrc == MQRCCF_COMMAND_FAILED) {
            MQLONG mqexeccc, mqexecrc;
            MQHBAG errbag;

            mqInquireBag(respbag, MQHA_BAG_HANDLE, 0, &errbag, &mqcc, &mqrc);
            if (mqcc != MQCC_OK) {
                cmq_reason("MQHA_BAG_HANDLE", mqcc, mqrc);
                goto error;
            }

            mqInquireInteger(errbag, MQIASY_COMP_CODE, MQIND_NONE, &mqexeccc, &mqcc, &mqrc);
            if (mqcc != MQCC_OK) {
                cmq_reason("MQIASY_COMP_CODE", mqcc, mqrc);
                goto error;
            }

            mqInquireInteger(errbag, MQIASY_REASON, MQIND_NONE, &mqexecrc, &mqcc, &mqrc);
            if (mqcc != MQCC_OK) {
                cmq_reason("MQIASY_REASON", mqcc, mqrc);
                goto error;
            }

            PLUGIN_ERROR("mqExecute failed reason: (%d) %s: (%d) %s\n", mqexeccc,
                         cmq_mqccstr(mqexeccc), mqexecrc, MQRC_STR(mqexecrc));
        } else {
            cmq_reason("mqExecute", mqcc, mqrc);
        }
    }

    rcode = 0;

    plugin_dispatch_metric_family_array(mq->fams, FAM_MQ_MAX, 0);

error:

    if (reqbag != MQHB_UNUSABLE_HBAG) {
        mqDeleteBag(&reqbag, &mqcc, &mqrc);
        if (mqcc != MQCC_OK) {
            cmq_reason("mqDeleteBag", mqcc, mqrc);
        }
    }

    if (respbag != MQHB_UNUSABLE_HBAG) {
        mqDeleteBag(&respbag, &mqcc, &mqrc);
        if (mqcc != MQCC_OK) {
            cmq_reason("mqDeleteBag", mqcc, mqrc);
        }
    }

    return rcode;
}

static int cmq_read(user_data_t *ud)
{
    cmq_instance_t *mq = (cmq_instance_t *)ud->data;
    if (mq == NULL)
        return -1;

    int status = 0;
    if (mq->hdl == MQHC_UNUSABLE_HCONN) {
        status = cmq_connect(mq);
        if (status != 0) {
            metric_family_append(&mq->fams[FAM_MQ_UP], VALUE_GAUGE(0), &mq->labels, NULL);
            plugin_dispatch_metric_family(&mq->fams[FAM_MQ_UP], 0);
            return 0;
        }
    }

    cdtime_t submit = cdtime();
    double up = 1;

    status = cmq_queue_list(mq);
    if (status < 0) {
        PLUGIN_ERROR("query stats failed for `%s'.", mq->name);
        up = 0;
    }

    metric_family_append(&mq->fams[FAM_MQ_UP], VALUE_GAUGE(up), &mq->labels, NULL);

    plugin_dispatch_metric_family_array(mq->fams, FAM_MQ_MAX, submit);
    return 0;
}

static int cmq_config_instance(config_item_t *ci)
{
    cmq_instance_t *mq = calloc(1, sizeof(*mq));
    if (mq == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    mq->hdl = MQHC_UNUSABLE_HCONN;

    int status = cf_util_get_string(ci, &mq->name);
    if (status != 0) {
        cmq_instance_free(mq);
        return status;
    }

    memcpy(mq->fams, fams, sizeof(mq->fams[0]) * FAM_MQ_MAX);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &mq->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_string(child, &mq->port);
        } else if (strcasecmp("username", child->key) == 0) {
            status = cf_util_get_string(child, &mq->username);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &mq->password);
        } else if (strcasecmp("queue-manager", child->key) == 0) {
            status = cf_util_get_string(child, &mq->qmanager);
        } else if (strcasecmp("connection-channel", child->key) == 0) {
            status = cf_util_get_string(child, &mq->cchannel);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &mq->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        cmq_instance_free(mq);
        return -1;
    }

    if (mq->host != NULL) {
        mq->connection = calloc(1, MQ_Q_MGR_NAME_LENGTH);
        if (mq->connection == NULL) {
            PLUGIN_ERROR("calloc failed");
            cmq_instance_free(mq);
            return -1;
        }
        if (mq->port == NULL) {
            mq->port = strdup("1414");
        }
        snprintf(mq->connection, MQ_Q_MGR_NAME_LENGTH - 1, "%s(%s)", mq->host, mq->port);
    }

    label_set_add(&mq->labels, true, "instance", mq->name);

    return plugin_register_complex_read("mq", mq->name, cmq_read, interval,
                                        &(user_data_t){.data=mq, .free_func=cmq_instance_free});
}

static int cmq_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = cmq_config_instance(child);
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
    plugin_register_config("mq", cmq_config);
}
