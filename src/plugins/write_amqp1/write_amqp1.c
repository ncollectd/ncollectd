// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright(c) 2017 Red Hat Inc.
// SPDX-FileContributor: Andy Smith <ansmith at redhat.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/random.h"
#include "libformat/format.h"

#include <proton/condition.h>
#include <proton/connection.h>
#include <proton/delivery.h>
#include <proton/link.h>
#include <proton/message.h>
#include <proton/proactor.h>
#include <proton/sasl.h>
#include <proton/session.h>
#include <proton/transport.h>

#define BUFSIZE 8192

typedef struct amqp1_config_inst_s {
    char *name;
    char *host;
    char *port;
    char *user;
    char *password;
    char *address;
    int retry_delay;
    format_stream_metric_t format_metric;
    format_notification_t format_notification;
    bool pre_settle;
    uint64_t dtag;
    pn_connection_t *conn;
    pn_link_t *sender;
    bool stopping;
    bool event_thread_running;
    pn_rwbytes_t mbuf;
    pthread_cond_t send_cond;
    pthread_mutex_t send_lock;
    pthread_t event_thread_id;
} amqp1_instance_t;

static int amqp1_send_out_messages(amqp1_instance_t *inst, pn_link_t *link)
{
    if (inst->stopping)
        return 0;

    int link_credit = pn_link_credit(link);

    if (link_credit > 0) {
        pthread_mutex_lock(&inst->send_lock);

        if (inst->mbuf.size == 0) {
            pthread_cond_signal(&inst->send_cond);
            pthread_mutex_unlock(&inst->send_lock);
            return 0;
        }

        inst->dtag++;

        pn_delivery_tag_t dtag = pn_dtag((const char *)&inst->dtag, sizeof(inst->dtag));
        pn_delivery_t *dlv = pn_delivery(link, dtag);
        pn_link_send(link, inst->mbuf.start, inst->mbuf.size);
        pn_link_advance(link);
        if (inst->pre_settle == true)
            pn_delivery_settle(dlv);

        free(inst->mbuf.start);
        inst->mbuf.start = NULL;
        inst->mbuf.size = 0;

        pthread_cond_signal(&inst->send_cond);
        pthread_mutex_unlock(&inst->send_lock);
    }

    return 0;
}

static void check_condition(amqp1_instance_t *inst, pn_event_t *e, pn_condition_t *cond)
{
    if (pn_condition_is_set(cond)) {
        PLUGIN_ERROR("%s: %s: %s", pn_event_type_name(pn_event_type(e)),
                     pn_condition_get_name(cond), pn_condition_get_description(cond));
        pn_connection_close(pn_event_connection(e));
        inst->conn = NULL;
    }
}

static bool handle(amqp1_instance_t *inst, pn_event_t *event)
{
    switch (pn_event_type(event)) {
    case PN_CONNECTION_INIT: {
        inst->conn = pn_event_connection(event);
        pn_connection_set_container(inst->conn, inst->name);
        pn_connection_open(inst->conn);
        pn_session_t *ssn = pn_session(inst->conn);
        pn_session_open(ssn);
        inst->sender = pn_sender(ssn, PACKAGE_NAME);
        pn_link_set_snd_settle_mode(inst->sender, PN_SND_MIXED);
        pn_link_open(inst->sender);
    }   break;
    case PN_LINK_FLOW:
        /* peer has given us credit, send outbound messages */
        amqp1_send_out_messages(inst, inst->sender);
        break;
    case PN_DELIVERY: {
        /* acknowledgement from peer that a message was delivered */
        pn_delivery_t *dlv = pn_event_delivery(event);
        if (pn_delivery_remote_state(dlv) == PN_ACCEPTED)
            pn_delivery_settle(dlv);
    }   break;
    case PN_CONNECTION_WAKE:
        if (!inst->stopping)
            amqp1_send_out_messages(inst, inst->sender);
        break;
    case PN_TRANSPORT_CLOSED: {
        pn_transport_t *t = pn_event_transport(event);
        if (t != NULL) {
            pn_condition_t *condition = pn_transport_condition(t);
            if (condition != NULL)
                check_condition(inst, event, condition);
        }
    }   break;
    case PN_CONNECTION_REMOTE_CLOSE: {
        pn_session_t *s = pn_event_session(event);
        if (s != NULL) {
            pn_condition_t *condition = pn_session_remote_condition(s);
            if (condition != NULL)
                check_condition(inst, event, condition);
        }
        pn_connection_close(pn_event_connection(event));
    }   break;
    case PN_SESSION_REMOTE_CLOSE: {
        pn_session_t *s = pn_event_session(event);
        if (s != NULL) {
            pn_condition_t *condition = pn_session_remote_condition(s);
            if (condition != NULL)
                check_condition(inst, event, condition);
        }
        pn_connection_close(pn_event_connection(event));
    }   break;
    case PN_LINK_REMOTE_CLOSE:
    case PN_LINK_REMOTE_DETACH: {
        pn_link_t *link = pn_event_link(event);
        if (event != NULL) {
            pn_condition_t *condition = pn_link_remote_condition (link);
            if (condition != NULL) {
                check_condition(inst, event, condition);
            }
        }
        pn_connection_close(pn_event_connection(event));
    }   break;
    case PN_PROACTOR_INACTIVE:
        return false;
    default:
        break;
    }
    return true;
}

static void *event_thread(void *arg)
{
    char addr[PN_MAX_ADDR];

    if (arg == NULL)
        return NULL;

    amqp1_instance_t *inst = arg;

    pn_proactor_t *proactor = pn_proactor();
    if (proactor == NULL) {
        PLUGIN_ERROR("Creating proactor failed.");
        return NULL;
    }

    pn_proactor_addr(addr, sizeof(addr), inst->host, inst->port);

    while (!inst->stopping) {
        /* make connection */
        inst->conn = pn_connection();
        if (inst->user != NULL) {
            pn_connection_set_user(inst->conn, inst->user);
            pn_connection_set_password(inst->conn, inst->password);
        }
        pn_proactor_connect(proactor, inst->conn, addr);

        bool engine_running = true;
        while (engine_running && !inst->stopping) {
            pn_event_batch_t *events = pn_proactor_wait(proactor);
            pn_event_t *e;
            while ((e = pn_event_batch_next(events))) {
                engine_running = handle(inst, e);
                if (!engine_running)
                    break;
            }
            pn_proactor_done(proactor, events);
        }
#if 0
// FIXME
        if (inst->conn != NULL) {
            pn_proactor_release_connection(inst->conn);
            pn_connection_free(inst->conn);
            inst->conn = NULL;
        }
#endif
        PLUGIN_DEBUG("retrying connection");
        int delay = inst->retry_delay;
        while (delay-- > 0 && !inst->stopping) {
            sleep(1);
        }
    }

    pn_proactor_free(proactor);

    inst->event_thread_running = false;

    return NULL;
}

static int encqueue(amqp1_instance_t *inst, char *buf, size_t size)
{
    pn_rwbytes_t mbuf = {.size = BUFSIZE };
    mbuf.start = malloc(mbuf.size);
    if (mbuf.start == NULL) {
        PLUGIN_ERROR("malloc failed");
        return -1;
    }

    /* encode message */
    pn_message_t *message = pn_message();
    if (message == NULL) {
        PLUGIN_ERROR("pn_message failed");
        free(mbuf.start);
        return -1;
    }
    pn_message_set_address(message, inst->address);
    pn_data_t *body = pn_message_body(message);
    if (body == NULL) {
        PLUGIN_ERROR("pn_message_body failed");
        free(mbuf.start);
        return -1;
    }
    pn_data_clear(body);
    pn_data_put_binary(body, pn_bytes(size, buf));
    pn_data_exit(body);

    /* put_binary copies and stores so ok to use mbuf */

    int status = 0;
    while ((status = pn_message_encode(message, mbuf.start, &mbuf.size)) == PN_OVERFLOW) {
        PLUGIN_DEBUG("increasing message buffer size %zu", mbuf.size);
        mbuf.size *= 2;
        char *start = realloc(mbuf.start, mbuf.size);
        if (start == NULL) {
            status = -1;
            break;
        }
        mbuf.start = start;
    }

    if (status != 0) {
        PLUGIN_ERROR("error encoding message: %s", pn_error_text(pn_message_error(message)));
        pn_message_free(message);
        free(mbuf.start);
        return -1;
    }

    pn_message_free(message);

    pthread_mutex_lock(&inst->send_lock);

    while (inst->mbuf.size != 0)
        pthread_cond_wait(&inst->send_cond, &inst->send_lock);

    inst->mbuf = mbuf;

    pthread_mutex_unlock(&inst->send_lock);

    /* activate the sender */
    if (inst->conn)
        pn_connection_wake(inst->conn);

    return 0;
}

static int amqp1_notify(notification_t const *n, user_data_t *user_data)
{
    if ((n == NULL) || (user_data == NULL))
        return EINVAL;

    amqp1_instance_t *inst = user_data->data;

    strbuf_t buf = STRBUF_CREATE;
    int status = format_notification(inst->format_notification, &buf, n);
    if (status != 0) {
        PLUGIN_ERROR("Failed to format notification.");
        strbuf_destroy(&buf);
        return 0;
    }
//  const char *content_type = format_notification_content_type(instance->format_notification);

    status = encqueue(inst, buf.ptr, strbuf_len(&buf));
    if (status != 0)
        PLUGIN_ERROR("notify enqueue failed");

    strbuf_destroy(&buf);

    return status;
}

static int amqp1_write(metric_family_t const *fam, user_data_t *user_data)
{
    if (fam == NULL || user_data == NULL)
        return EINVAL;

    amqp1_instance_t *inst = user_data->data;

    strbuf_t buf = STRBUF_CREATE;
    format_stream_metric_ctx_t ctx = {0};

    int status = format_stream_metric_begin(&ctx, inst->format_metric, &buf);
    status |= format_stream_metric_family(&ctx, fam);
    status |= format_stream_metric_end(&ctx);
    if (status != 0) {
        PLUGIN_ERROR("Failed to format metric.");
        strbuf_destroy(&buf);
        return 0;
    }
//  const char *content_type = format_metric_content_type(instance->format);

    /* encode message and place on outbound queue */
    status = encqueue(inst, buf.ptr, strbuf_len(&buf));
    if (status != 0)
        PLUGIN_ERROR("write enqueue failed");
    strbuf_destroy(&buf);

    return status;
}

static void amqp1_config_instance_free(void *ptr)
{
    amqp1_instance_t *inst = ptr;

    if (inst == NULL)
        return;

    inst->stopping = true;

    /* Stop the proactor thread */
    if (inst->event_thread_running) {
        PLUGIN_DEBUG("Shutting down proactor thread.");
        pn_connection_wake(inst->conn);
    }
    pthread_join(inst->event_thread_id, NULL /* no return value */);

    pthread_mutex_destroy(&inst->send_lock);
    pthread_cond_destroy(&inst->send_cond);

    free(inst->name);
    free(inst->host);
    free(inst->port);
    free(inst->user);
    free(inst->password);
    free(inst->address);

    free(inst->mbuf.start);

    free(inst);
}

static int amqp1_config_instance(config_item_t *ci)
{
    amqp1_instance_t *inst = calloc(1, sizeof(*inst));
    if (inst == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return ENOMEM;
    }

    inst->retry_delay = 1;

    int status = cf_util_get_string(ci, &inst->name);
    if (status != 0) {
        free(inst);
        return status;
    }

    inst->format_metric = FORMAT_STREAM_METRIC_OPENMETRICS_TEXT;
    inst->format_notification = FORMAT_NOTIFICATION_JSON;

    cf_send_t send = SEND_METRICS;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &inst->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_string(child, &inst->port);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &inst->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &inst->password);
        } else if (strcasecmp("address", child->key) == 0) {
            status = cf_util_get_string(child, &inst->address);
        } else if (strcasecmp("retry-delay", child->key) == 0) {
            status = cf_util_get_int(child, &inst->retry_delay);
        } else if (strcasecmp("pre-settle", child->key) == 0) {
            status = cf_util_get_boolean(child, &inst->pre_settle);
        } else if (strcasecmp("format-metric", child->key) == 0) {
            status = config_format_stream_metric(child, &inst->format_metric);
        } else if (strcasecmp("format-notification", child->key) == 0) {
            status = config_format_notification(child, &inst->format_notification);
        } else if (strcasecmp("write", child->key) == 0) {
            status = cf_uti_get_send(child, &send);
        } else {
            PLUGIN_ERROR("Ignoring unknown inst configuration option '%s'.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        amqp1_config_instance_free(inst);
        return -1;
    }

    pthread_mutex_init(&inst->send_lock, /* attr = */ NULL);
    pthread_cond_init(&inst->send_cond, /* attr = */ NULL);
    /* start_thread */
    status = plugin_thread_create(&inst->event_thread_id, event_thread, inst, "write_amqp1");
    if (status != 0) {
        PLUGIN_ERROR("pthread_create failed: %s", STRERRNO);
        amqp1_config_instance_free(inst);
        return -1;
    } else {
        inst->event_thread_running = true;
    }

    user_data_t user_data = (user_data_t){ .data = inst, .free_func = amqp1_config_instance_free};

    if (send == SEND_NOTIFICATIONS)
        return plugin_register_notification("write_amqp1", inst->name, amqp1_notify, &user_data);

    return plugin_register_write("write_amqp1", inst->name, amqp1_write, NULL, 0, 0, &user_data);
}

static int amqp1_config(config_item_t *ci)
{
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            int status = amqp1_config_instance(child);
            if (status != 0)
                return -1;
        } else {
            PLUGIN_WARNING("Unknown config option '%s'.", child->key);
            return -1;
        }
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("write_amqp1", amqp1_config);
}
