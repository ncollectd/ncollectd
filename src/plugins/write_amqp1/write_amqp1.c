// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright(c) 2017 Red Hat Inc.
// SPDX-FileContributor: Andy Smith <ansmith at redhat.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/random.h"
#include "libformat/format.h"

#include <stdatomic.h>

#include <proton/condition.h>
#include <proton/connection.h>
#include <proton/delivery.h>
#include <proton/link.h>
#include <proton/message.h>
#include <proton/proactor.h>
#include <proton/sasl.h>
#include <proton/session.h>
#include <proton/ssl.h>
#include <proton/transport.h>

#define BUFSIZE 8192
/* Maximum time, in seconds, to wait for the previous message to be sent */
#define SEND_TIMEOUT 10

typedef struct amqp1_config_inst_s {
    char *name;
    char *host;
    char *port;
    char *user;
    char *password;
    char *address;
    bool tls_enabled;
    bool tls_verify_peer;
    char *tls_cacert;
    bool allow_insecure_mechs;
    int retry_delay;
    format_stream_metric_t format_metric;
    format_notification_t format_notification;
    bool pre_settle;
    uint64_t dtag;
    /* conn and proactor are protected by send_lock */
    pn_proactor_t *proactor;
    pn_connection_t *conn;
    pn_link_t *sender;
    atomic_bool stopping;
    atomic_bool event_thread_running;
    bool event_thread_created;
    pn_rwbytes_t mbuf;
    complain_t complaint;
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
        ssize_t sent = pn_link_send(link, inst->mbuf.start, inst->mbuf.size);
        if (sent < 0)
            PLUGIN_ERROR("pn_link_send failed: %zd", sent);
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
        pthread_mutex_lock(&inst->send_lock);
        inst->conn = NULL;
        pthread_mutex_unlock(&inst->send_lock);
    }
}

static bool handle(amqp1_instance_t *inst, pn_event_t *event)
{
    switch (pn_event_type(event)) {
    case PN_CONNECTION_INIT: {
        pn_connection_t *conn = pn_event_connection(event);
        pthread_mutex_lock(&inst->send_lock);
        inst->conn = conn;
        pthread_mutex_unlock(&inst->send_lock);
        pn_connection_set_container(conn, inst->name);
        pn_connection_open(conn);
        pn_session_t *ssn = pn_session(conn);
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
        /* The proactor frees the connection after this event is handled. */
        pthread_mutex_lock(&inst->send_lock);
        inst->conn = NULL;
        inst->sender = NULL;
        pthread_mutex_unlock(&inst->send_lock);
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
        if (link != NULL) {
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
        inst->event_thread_running = false;
        return NULL;
    }

    pthread_mutex_lock(&inst->send_lock);
    inst->proactor = proactor;
    pthread_mutex_unlock(&inst->send_lock);

    pn_ssl_domain_t *ssl_domain = NULL;
    if (inst->tls_enabled) {
        ssl_domain = pn_ssl_domain(PN_SSL_MODE_CLIENT);
        if (ssl_domain == NULL) {
            PLUGIN_ERROR("Creating ssl domain failed.");
            pn_proactor_free(proactor);
            inst->event_thread_running = false;
            return NULL;
        }

        if (inst->tls_cacert != NULL) {
            int status = pn_ssl_domain_set_trusted_ca_db(ssl_domain, inst->tls_cacert);
            if (status != 0) {
                PLUGIN_ERROR("Setting trusted CA database '%s' failed: %d.",
                             inst->tls_cacert, status);
                pn_ssl_domain_free(ssl_domain);
                pn_proactor_free(proactor);
                inst->event_thread_running = false;
                return NULL;
            }
        }

        int status = pn_ssl_domain_set_peer_authentication(ssl_domain,
                         inst->tls_verify_peer ? PN_SSL_VERIFY_PEER_NAME : PN_SSL_ANONYMOUS_PEER,
                         NULL);
        if (status != 0) {
            PLUGIN_ERROR("Setting peer authentication failed: %d.", status);
            pn_ssl_domain_free(ssl_domain);
            pn_proactor_free(proactor);
            inst->event_thread_running = false;
            return NULL;
        }
    }

    pn_proactor_addr(addr, sizeof(addr), inst->host, inst->port);

    while (!inst->stopping) {
        /* make connection */
        pn_connection_t *conn = pn_connection();
        if (conn == NULL) {
            PLUGIN_ERROR("Creating connection failed.");
            break;
        }
        if (inst->user != NULL) {
            pn_connection_set_user(conn, inst->user);
            pn_connection_set_password(conn, inst->password);
        }
        pthread_mutex_lock(&inst->send_lock);
        inst->conn = conn;
        pthread_mutex_unlock(&inst->send_lock);

        pn_transport_t *transport = pn_transport();
        if (transport == NULL) {
            PLUGIN_ERROR("Creating transport failed.");
            pn_connection_free(inst->conn);
            inst->conn = NULL;
            break;
        }

        if (ssl_domain != NULL) {
            int status = pn_ssl_init(pn_ssl(transport), ssl_domain,
                                     inst->host != NULL ? inst->host : "localhost");
            if (status != 0) {
                PLUGIN_ERROR("Initializing ssl failed: %d.", status);
                pn_transport_free(transport);
                pn_connection_free(inst->conn);
                inst->conn = NULL;
                break;
            }
        }

        if (inst->allow_insecure_mechs)
            pn_sasl_set_allow_insecure_mechs(pn_sasl(transport), true);

        pn_proactor_connect2(proactor, inst->conn, transport, addr);

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
        PLUGIN_DEBUG("retrying connection");
        int delay = inst->retry_delay;
        while (delay-- > 0 && !inst->stopping) {
            sleep(1);
        }
    }

    pthread_mutex_lock(&inst->send_lock);
    inst->proactor = NULL;
    inst->conn = NULL;
    pthread_mutex_unlock(&inst->send_lock);

    pn_proactor_free(proactor);

    if (ssl_domain != NULL)
        pn_ssl_domain_free(ssl_domain);

    inst->event_thread_running = false;

    return NULL;
}

static int message_encode(pn_message_t *msg, pn_rwbytes_t *buffer)
{
    if (buffer->start == NULL) {
        buffer->start = (char*)malloc(BUFSIZE);
        if (buffer->start == NULL) {
            PLUGIN_ERROR("malloc failed");
            return -1;
        }
        buffer->size = BUFSIZE;
    }

    size_t size = buffer->size;
    int err = 0;
    while ((err = pn_message_encode(msg, buffer->start, &size)) == PN_OVERFLOW) {
        size_t nsize = buffer->size * 2;
        PLUGIN_DEBUG("increasing message buffer size to %zu", nsize);
        char *start = realloc(buffer->start, nsize);
        if (start == NULL) {
            PLUGIN_ERROR("realloc failed");
            free(buffer->start);
            buffer->start = NULL;
            return -1;
        }
        buffer->start = start;
        buffer->size = nsize;
        size = nsize;
    }

    if (err != 0) {
        free(buffer->start);
        return -1;
    }

    buffer->size = size;

    return 0;
}

static int encqueue(amqp1_instance_t *inst, char *buf, size_t size, const char *content_type)
{
    /* encode message */
    pn_message_t *message = pn_message();
    if (message == NULL) {
        PLUGIN_ERROR("pn_message failed");
        return -1;
    }
    pn_message_set_address(message, inst->address);
    if (content_type != NULL)
        pn_message_set_content_type(message, content_type);
    pn_data_t *body = pn_message_body(message);
    if (body == NULL) {
        PLUGIN_ERROR("pn_message_body failed");
        pn_message_free(message);
        return -1;
    }
    pn_data_clear(body);
    pn_data_put_binary(body, pn_bytes(size, buf));
    pn_data_exit(body);

    /* put_binary copies and stores so ok to use mbuf */
    pn_rwbytes_t mbuf = {0};
    int status  = message_encode(message, &mbuf);
    if (status < 0) {
        PLUGIN_ERROR("error encoding message: %s", pn_error_text(pn_message_error(message)));
        pn_message_free(message);
        return -1;
    }

    pn_message_free(message);

    pthread_mutex_lock(&inst->send_lock);

    struct timespec timeout = CDTIME_T_TO_TIMESPEC(cdtime() + TIME_T_TO_CDTIME_T(SEND_TIMEOUT));
    while ((inst->mbuf.size != 0) && !inst->stopping) {
        if (pthread_cond_timedwait(&inst->send_cond, &inst->send_lock, &timeout) == ETIMEDOUT)
            break;
    }

    if (inst->stopping || (inst->mbuf.size != 0)) {
        if (!inst->stopping)
            PLUGIN_COMPLAIN_WARNING(&inst->complaint,
                       "write_amqp1 plugin: timeout sending message to the broker, "
                       "dropping messages.");
        pthread_mutex_unlock(&inst->send_lock);
        free(mbuf.start);
        return -1;
    }

    PLUGIN_RELEASE_INFO(&inst->complaint,
                        "write_amqp1 plugin: sending messages to the broker again.");

    inst->mbuf = mbuf;

    /* activate the sender */
    if (inst->conn)
        pn_connection_wake(inst->conn);

    pthread_mutex_unlock(&inst->send_lock);

    return 0;
}

static void amqp1_config_instance_free(void *ptr)
{
    amqp1_instance_t *inst = ptr;

    if (inst == NULL)
        return;

    pthread_mutex_lock(&inst->send_lock);
    inst->stopping = true;
    /* wake up any writer waiting to enqueue a message */
    pthread_cond_broadcast(&inst->send_cond);
    pthread_mutex_unlock(&inst->send_lock);

    /* Stop the proactor thread */
    if (inst->event_thread_running) {
        PLUGIN_DEBUG("Shutting down proactor thread.");
        pthread_mutex_lock(&inst->send_lock);
        if (inst->proactor != NULL)
            pn_proactor_interrupt(inst->proactor);
        pthread_mutex_unlock(&inst->send_lock);
    }
    if (inst->event_thread_created)
        pthread_join(inst->event_thread_id, NULL /* no return value */);

    pthread_mutex_destroy(&inst->send_lock);
    pthread_cond_destroy(&inst->send_cond);

    free(inst->name);
    free(inst->host);
    free(inst->port);
    free(inst->user);
    free(inst->password);
    free(inst->address);
    free(inst->tls_cacert);

    free(inst->mbuf.start);

    free(inst);
}

static int amqp1_event_thread(amqp1_instance_t *inst)
{
    if (inst->event_thread_running)
        return 0;

    /* start_thread */
    inst->event_thread_running = true;
    int status = plugin_thread_create(&inst->event_thread_id, event_thread, inst, "write_amqp1");
    if (status != 0) {
        PLUGIN_ERROR("pthread_create failed: %s", STRERRNO);
        inst->event_thread_running = false;
        amqp1_config_instance_free(inst);
        return -1;
    }
    inst->event_thread_created = true;

    return 0;
}

static int amqp1_notify(notification_t const *n, user_data_t *user_data)
{
    if ((n == NULL) || (user_data == NULL))
        return EINVAL;

    amqp1_instance_t *inst = user_data->data;

    if (amqp1_event_thread(inst) != 0)
        return -1;

    strbuf_t buf = STRBUF_CREATE;
    int status = format_notification(inst->format_notification, &buf, n);
    if (status != 0) {
        PLUGIN_ERROR("Failed to format notification.");
        strbuf_destroy(&buf);
        return 0;
    }
    const char *content_type = format_notification_content_type(inst->format_notification);

    status = encqueue(inst, buf.ptr, strbuf_len(&buf), content_type);
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

    if (amqp1_event_thread(inst) != 0)
        return -1;

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
    const char *content_type = format_stream_metric_content_type(inst->format_metric);

    /* encode message and place on outbound queue */
    status = encqueue(inst, buf.ptr, strbuf_len(&buf), content_type);
    if (status != 0)
        PLUGIN_ERROR("write enqueue failed");
    strbuf_destroy(&buf);

    return status;
}

static int amqp1_config_instance(config_item_t *ci)
{
    amqp1_instance_t *inst = calloc(1, sizeof(*inst));
    if (inst == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return ENOMEM;
    }

    inst->retry_delay = 1;
    inst->tls_verify_peer = true;

    pthread_mutex_init(&inst->send_lock, /* attr = */ NULL);
    pthread_cond_init(&inst->send_cond, /* attr = */ NULL);

    pthread_mutex_init(&inst->send_lock, /* attr = */ NULL);
    pthread_cond_init(&inst->send_cond, /* attr = */ NULL);

    int status = cf_util_get_string(ci, &inst->name);
    if (status != 0) {
        amqp1_config_instance_free(inst);
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
        } else if (strcasecmp("user-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &inst->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &inst->password);
        } else if (strcasecmp("password-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &inst->password);
        } else if (strcasecmp("address", child->key) == 0) {
            status = cf_util_get_string(child, &inst->address);
        } else if (strcasecmp("tls-enabled", child->key) == 0) {
            status = cf_util_get_boolean(child, &inst->tls_enabled);
        } else if (strcasecmp("tls-verify-peer", child->key) == 0) {
            status = cf_util_get_boolean(child, &inst->tls_verify_peer);
        } else if (strcasecmp("tls-ca-cert", child->key) == 0) {
            status = cf_util_get_string(child, &inst->tls_cacert);
        } else if (strcasecmp("allow-insecure-mechs", child->key) == 0) {
            status = cf_util_get_boolean(child, &inst->allow_insecure_mechs);
        } else if (strcasecmp("retry-delay", child->key) == 0) {
            status = cf_util_get_int(child, &inst->retry_delay);
            if ((status == 0) && (inst->retry_delay < 1)) {
                PLUGIN_ERROR("'retry-delay' must be greater than 0.");
                status = -1;
            }
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

    if ((status == 0) && inst->allow_insecure_mechs && !inst->tls_enabled)
        PLUGIN_WARNING("'allow-insecure-mechs' is enabled without TLS, "
                       "credentials may be sent in clear text.");

    if (status != 0) {
        amqp1_config_instance_free(inst);
        return -1;
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
