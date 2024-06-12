// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2014 Marc Falzon
// SPDX-FileCopyrightText: Copyright (C) 2014,2015 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Marc Falzon <marc at baha dot mu>
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Jan-Piet Mens <jpmens at gmail.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

// Reference: http://mosquitto.org/api/files/mosquitto-h.html
// https://github.com/mqtt/mqtt.org/wiki/SYS-Topics

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/complain.h"

#include <mosquitto.h>

#define MQTT_DEFAULT_HOST "localhost"
#define MQTT_DEFAULT_PORT 1883
#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE 60
#endif
#ifndef SSL_VERIFY_PEER
#define SSL_VERIFY_PEER 1
#endif

enum {
    FAM_MOSQUITTO_UP,
    FAM_MOSQUITTO_RECEIVED_BYTES,
    FAM_MOSQUITTO_SENT_BYTES,
    FAM_MOSQUITTO_CLIENTS_CONNECTED,
    FAM_MOSQUITTO_CLIENTS_EXPIRED,
    FAM_MOSQUITTO_CLIENTS_DISCONNECTED,
    FAM_MOSQUITTO_CLIENTS_MAXIMUM,
    FAM_MOSQUITTO_CLIENTS,
    FAM_MOSQUITTO_HEAP_SIZE,
    FAM_MOSQUITTO_HEAP_MAXIMUM_SIZE,
    FAM_MOSQUITTO_MESSAGES_INFLIGHT,
    FAM_MOSQUITTO_MESSAGES_RECEIVED,
    FAM_MOSQUITTO_MESSAGES_SENT,
    FAM_MOSQUITTO_PUBLISH_MESSAGES_DROPPED,
    FAM_MOSQUITTO_PUBLISH_MESSAGES_RECEIVED,
    FAM_MOSQUITTO_PUBLISH_MESSAGES_SENT,
    FAM_MOSQUITTO_MESSAGES_RETAINED,
    FAM_MOSQUITTO_STORE_MESSAGES,
    FAM_MOSQUITTO_STORE_MESSAGES_BYTES,
    FAM_MOSQUITTO_SUBSCRIPTIONS,
    FAM_MOSQUITTO_MAX
};

static metric_family_t fams[FAM_MOSQUITTO_MAX] = {
    [FAM_MOSQUITTO_UP] = {
        .name = "mosquitto_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the mosquitto server be reached.",
    },
    [FAM_MOSQUITTO_RECEIVED_BYTES] = {
        .name = "mosquitto_received_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes received since the broker started.",
    },
    [FAM_MOSQUITTO_SENT_BYTES] = {
        .name = "mosquitto_sent_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of bytes sent since the broker started.",
    },
    [FAM_MOSQUITTO_CLIENTS_CONNECTED] = {
        .name = "mosquitto_clients_connected",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of currently connected clients.",
    },
    [FAM_MOSQUITTO_CLIENTS_EXPIRED] = {
        .name = "mosquitto_clients_expired",
        .type = METRIC_TYPE_COUNTER,
        .help = "The number of disconnected persistent clients that have been expired and removed."
    },
    [FAM_MOSQUITTO_CLIENTS_DISCONNECTED] = {
        .name = "mosquitto_clients_disconnected",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of persistent clients (with clean session disabled) "
                "that are registered at the broker but are currently disconnected.",
    },
    [FAM_MOSQUITTO_CLIENTS_MAXIMUM] = {
        .name = "mosquitto_clients_maximum",
        .type = METRIC_TYPE_GAUGE,
        .help = "The maximum number of clients that have been connected "
                "to the broker at the same time.",
    },
    [FAM_MOSQUITTO_CLIENTS] = {
        .name = "mosquitto_clients",
        .type = METRIC_TYPE_GAUGE,
        .help = "The total number of active and inactive clients currently connected "
                "and registered on the broker.",
    },
    [FAM_MOSQUITTO_HEAP_SIZE] = {
        .name = "mosquitto_heap_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current size of the heap memory in use by mosquitto.",
    },
    [FAM_MOSQUITTO_HEAP_MAXIMUM_SIZE] = {
        .name = "mosquitto_heap_maximum_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "The largest amount of heap memory used by mosquitto.",
    },
    [FAM_MOSQUITTO_MESSAGES_INFLIGHT] = {
        .name = "mosquitto_messages_inflight",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of messages with QoS>0 that are awaiting acknowledgments.",
    },
    [FAM_MOSQUITTO_MESSAGES_RECEIVED] = {
        .name = "mosquitto_messages_received",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of messages of any type received since the broker started.",
    },
    [FAM_MOSQUITTO_MESSAGES_SENT] = {
        .name = "mosquitto_messages_sent",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of messages of any type sent since the broker started.",
    },
    [FAM_MOSQUITTO_PUBLISH_MESSAGES_DROPPED] = {
        .name = "mosquitto_publish_messages_dropped",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of PUBLISH messages that have been dropped "
                "due to inflight/queuing limits.",
    },
    [FAM_MOSQUITTO_PUBLISH_MESSAGES_RECEIVED] = {
        .name = "mosquitto_publish_messages_received",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of PUBLISH messages received since the broker started.",
    },
    [FAM_MOSQUITTO_PUBLISH_MESSAGES_SENT] = {
        .name = "mosquitto_publish_messages_sent",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of PUBLISH messages sent since the broker started.",
    },
    [FAM_MOSQUITTO_MESSAGES_RETAINED] = {
        .name = "mosquitto_messages_retained",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of retained messages active on the broker.",
    },
    [FAM_MOSQUITTO_STORE_MESSAGES] = {
        .name = "mosquitto_store_messages",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of messages currently held in the message store.",
    },
    [FAM_MOSQUITTO_STORE_MESSAGES_BYTES] = {
        .name = "mosquitto_store_messages_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes currently held by message payloads in the message store.",
    },
    [FAM_MOSQUITTO_SUBSCRIPTIONS] = {
        .name = "mosquitto_subscriptions",
        .type = METRIC_TYPE_COUNTER,
        .help = "The total number of subscriptions active on the broker.",
    },
};

typedef struct {
    int fam;
    char *topic;
    union {
        volatile uint64_t uint64;
        volatile double float64;
    };
    volatile bool updated;
} ncmosquitto_subs_t;

static ncmosquitto_subs_t ncmosquitto_subs[] = {
    { FAM_MOSQUITTO_RECEIVED_BYTES,             "$SYS/broker/bytes/received"            },
    { FAM_MOSQUITTO_SENT_BYTES,                 "$SYS/broker/bytes/sent"                },
    { FAM_MOSQUITTO_CLIENTS_CONNECTED,          "$SYS/broker/clients/connected"         },
    { FAM_MOSQUITTO_CLIENTS_EXPIRED,            "$SYS/broker/clients/expired"           },
    { FAM_MOSQUITTO_CLIENTS_DISCONNECTED,       "$SYS/broker/clients/disconnected"      },
    { FAM_MOSQUITTO_CLIENTS_MAXIMUM,            "$SYS/broker/clients/maximum"           },
    { FAM_MOSQUITTO_CLIENTS,                    "$SYS/broker/clients/total"             },
    { FAM_MOSQUITTO_HEAP_SIZE,                  "$SYS/broker/heap/current size"         },
    { FAM_MOSQUITTO_HEAP_MAXIMUM_SIZE,          "$SYS/broker/heap/maximum size"         },
    { FAM_MOSQUITTO_MESSAGES_INFLIGHT,          "$SYS/broker/messages/inflight"         },
    { FAM_MOSQUITTO_MESSAGES_RECEIVED,          "$SYS/broker/messages/received"         },
    { FAM_MOSQUITTO_MESSAGES_SENT,              "$SYS/broker/messages/sent"             },
    { FAM_MOSQUITTO_PUBLISH_MESSAGES_DROPPED,   "$SYS/broker/publish/messages/dropped"  },
    { FAM_MOSQUITTO_PUBLISH_MESSAGES_RECEIVED,  "$SYS/broker/publish/messages/received" },
    { FAM_MOSQUITTO_PUBLISH_MESSAGES_SENT,      "$SYS/broker/publish/messages/sent"     },
    { FAM_MOSQUITTO_MESSAGES_RETAINED,          "$SYS/broker/retained messages/count"   },
    { FAM_MOSQUITTO_STORE_MESSAGES,             "$SYS/broker/store/messages/count"      },
    { FAM_MOSQUITTO_STORE_MESSAGES_BYTES,       "$SYS/broker/store/messages/bytes"      },
    { FAM_MOSQUITTO_SUBSCRIPTIONS,              "$SYS/broker/subscriptions/count"       },
};
static size_t ncmosquitto_subs_size = STATIC_ARRAY_SIZE(ncmosquitto_subs);

typedef struct {
    char *name;

    struct mosquitto *mosq;
    volatile bool connected;

    char *host;
    int port;
    char *client_id;
    char *username;
    char *password;
    char *cacertificatefile;
    char *certificatefile;
    char *certificatekeyfile;
    char *tlsprotocol;
    char *ciphersuite;
    int qos;
    label_set_t labels;
    plugin_filter_t *filter;

    metric_family_t fams[FAM_MOSQUITTO_MAX];
    ncmosquitto_subs_t *subs;
    size_t subs_size;

    /* For subscribing */
    pthread_t thread;
    volatile bool loop;
    bool clean_session;

    c_complain_t complaint_cantpublish;
} ncmosquitto_instance_t;

#if LIBMOSQUITTO_MAJOR == 0
static char const *mosquitto_strerror(int code)
{
    switch (code) {
    case MOSQ_ERR_SUCCESS:
        return "MOSQ_ERR_SUCCESS";
    case MOSQ_ERR_NOMEM:
        return "MOSQ_ERR_NOMEM";
    case MOSQ_ERR_PROTOCOL:
        return "MOSQ_ERR_PROTOCOL";
    case MOSQ_ERR_INVAL:
        return "MOSQ_ERR_INVAL";
    case MOSQ_ERR_NO_CONN:
        return "MOSQ_ERR_NO_CONN";
    case MOSQ_ERR_CONN_REFUSED:
        return "MOSQ_ERR_CONN_REFUSED";
    case MOSQ_ERR_NOT_FOUND:
        return "MOSQ_ERR_NOT_FOUND";
    case MOSQ_ERR_CONN_LOST:
        return "MOSQ_ERR_CONN_LOST";
    case MOSQ_ERR_SSL:
        return "MOSQ_ERR_SSL";
    case MOSQ_ERR_PAYLOAD_SIZE:
        return "MOSQ_ERR_PAYLOAD_SIZE";
    case MOSQ_ERR_NOT_SUPPORTED:
        return "MOSQ_ERR_NOT_SUPPORTED";
    case MOSQ_ERR_AUTH:
        return "MOSQ_ERR_AUTH";
    case MOSQ_ERR_ACL_DENIED:
        return "MOSQ_ERR_ACL_DENIED";
    case MOSQ_ERR_UNKNOWN:
        return "MOSQ_ERR_UNKNOWN";
    case MOSQ_ERR_ERRNO:
        return "MOSQ_ERR_ERRNO";
    }

    return "UNKNOWN ERROR CODE";
}
#else
/* provided by libmosquitto */
#endif

static void ncmosquitto_free(void *arg)
{
    ncmosquitto_instance_t *conf = arg;
    if (conf == NULL)
        return;

    conf->loop = 0;
    int status = pthread_join(conf->thread, /* retval = */ NULL);
    if (status != 0) {
        PLUGIN_ERROR("pthread_join failed: %s", STRERROR(status));
    }

    if (conf->connected)
        (void)mosquitto_disconnect(conf->mosq);
    conf->connected = false;
    (void)mosquitto_destroy(conf->mosq);


    free(conf->name);
    free(conf->host);
    free(conf->client_id);
    free(conf->username);
    free(conf->password);
    free(conf->cacertificatefile);
    free(conf->certificatefile);
    free(conf->certificatekeyfile);
    free(conf->tlsprotocol);
    free(conf->ciphersuite);
    free(conf->subs);
    label_set_reset(&conf->labels);
    plugin_filter_free(conf->filter);

    free(conf);
}

static void ncmosquitto_on_message(
#if LIBMOSQUITTO_MAJOR == 0
#else
        __attribute__((unused)) struct mosquitto *m,
#endif
        __attribute__((unused)) void *arg, const struct mosquitto_message *msg)
{
    ncmosquitto_instance_t *conf = arg;

    if (msg->payloadlen <= 0) {
        PLUGIN_DEBUG("message has empty payload");
        return;
    }

    for (size_t i = 0; i < ncmosquitto_subs_size ; i++) {
        if (strcmp(msg->topic, conf->subs[i].topic) == 0) {
            char playload[256];
            size_t playload_size =  sizeof(playload)-1;
            if (msg->payloadlen < (int)(sizeof(playload)-1))
                playload_size = msg->payloadlen;
            memcpy(playload, msg->payload, playload_size);
            playload[playload_size] = '\0';

            uint64_t value = 0;
            int status = strtouint(playload, &value);
            if (status != 0) {
                PLUGIN_ERROR("Failed to convert \"%s\" to integer", playload);
                break;
            }

            int type = conf->fams[conf->subs[i].fam].type;
            if (type == METRIC_TYPE_COUNTER) {
                conf->subs[i].uint64 = value;
                conf->subs[i].updated = true;
            } else if (type == METRIC_TYPE_GAUGE) {
                conf->subs[i].float64 = value;
                conf->subs[i].updated = true;
            }
            break;
        }
    }
}

static int ncmosquitto_subscribe(ncmosquitto_instance_t *conf)
{
    for (size_t i = 0; i < conf->subs_size ; i++) {
        char *topic = conf->subs[i].topic;
        conf->subs[i].updated = false;
        int status = mosquitto_subscribe(conf->mosq, /* message_id = */ NULL, topic, conf->qos);
        if (status != MOSQ_ERR_SUCCESS) {
            PLUGIN_ERROR("Subscribing to \"%s\" failed: %s", topic, mosquitto_strerror(status));
            mosquitto_disconnect(conf->mosq);
            return -1;
        }
    }

    return 0;
}

static int ncmosquitto_reconnect(ncmosquitto_instance_t *conf)
{
    int status;

    if (conf->connected)
        return 0;

    status = mosquitto_reconnect(conf->mosq);
    if (status != MOSQ_ERR_SUCCESS) {
        PLUGIN_ERROR("mqtt_connect_broker: mosquitto_connect failed: %s",
                     (status == MOSQ_ERR_ERRNO) ? STRERRNO : mosquitto_strerror(status));
        return -1;
    }

    status = ncmosquitto_subscribe(conf);
    if (status != 0)
        return status;

    conf->connected = true;

    c_release(LOG_INFO, &conf->complaint_cantpublish,
                        "mqtt plugin: successfully reconnected to broker \"%s:%d\"",
                        conf->host, conf->port);

    return 0;
}

static int ncmosquitto_connect(ncmosquitto_instance_t *conf)
{
    char const *client_id;
    int status;

    if (conf->mosq != NULL)
        return ncmosquitto_reconnect(conf);

    if (conf->client_id)
        client_id = conf->client_id;
    else
        client_id = plugin_get_hostname();

#if LIBMOSQUITTO_MAJOR == 0
    conf->mosq = mosquitto_new(client_id, /* user data = */ conf);
#else
    conf->mosq = mosquitto_new(client_id, conf->clean_session, /* user data = */ conf);
#endif
    if (conf->mosq == NULL) {
        PLUGIN_ERROR("mosquitto_new failed");
        return -1;
    }

#if LIBMOSQUITTO_MAJOR != 0
    if (conf->cacertificatefile) {
        status = mosquitto_tls_set(conf->mosq, conf->cacertificatefile, NULL,
                                               conf->certificatefile, conf->certificatekeyfile,
                                               /* pw_callback */ NULL);
        if (status != MOSQ_ERR_SUCCESS) {
            PLUGIN_ERROR("cannot mosquitto_tls_set: %s", mosquitto_strerror(status));
            mosquitto_destroy(conf->mosq);
            conf->mosq = NULL;
            return -1;
        }

        status = mosquitto_tls_opts_set(conf->mosq, SSL_VERIFY_PEER,
                                                    conf->tlsprotocol, conf->ciphersuite);
        if (status != MOSQ_ERR_SUCCESS) {
            PLUGIN_ERROR("cannot mosquitto_tls_opts_set: %s", mosquitto_strerror(status));
            mosquitto_destroy(conf->mosq);
            conf->mosq = NULL;
            return -1;
        }

        status = mosquitto_tls_insecure_set(conf->mosq, false);
        if (status != MOSQ_ERR_SUCCESS) {
            PLUGIN_ERROR("cannot mosquitto_tls_insecure_set: %s", mosquitto_strerror(status));
            mosquitto_destroy(conf->mosq);
            conf->mosq = NULL;
            return -1;
        }
    }
#endif

    if (conf->username && conf->password) {
        status = mosquitto_username_pw_set(conf->mosq, conf->username, conf->password);
        if (status != MOSQ_ERR_SUCCESS) {
            PLUGIN_ERROR("mosquitto_username_pw_set failed: %s",
                         (status == MOSQ_ERR_ERRNO) ? STRERRNO : mosquitto_strerror(status));
            mosquitto_destroy(conf->mosq);
            conf->mosq = NULL;
            return -1;
        }
    }

#if LIBMOSQUITTO_MAJOR == 0
    status = mosquitto_connect(conf->mosq, conf->host, conf->port,
                                           /* keepalive = */ MQTT_KEEPALIVE,
                                           /* clean session = */ conf->clean_session);
#else
    status = mosquitto_connect(conf->mosq, conf->host, conf->port, MQTT_KEEPALIVE);
#endif
    if (status != MOSQ_ERR_SUCCESS) {
        PLUGIN_ERROR("mosquitto_connect failed: %s",
                     (status == MOSQ_ERR_ERRNO) ? STRERRNO : mosquitto_strerror(status));
        mosquitto_destroy(conf->mosq);
        conf->mosq = NULL;
        return -1;
    }

    mosquitto_message_callback_set(conf->mosq, ncmosquitto_on_message);

    status = ncmosquitto_subscribe(conf);
    if (status != 0) {
        mosquitto_destroy(conf->mosq);
        conf->mosq = NULL;
        return status;
    }

    conf->connected = true;
    return 0;
}

static void *ncmosquitto_subscribers_thread(void *arg)
{
    ncmosquitto_instance_t *conf = arg;
    int status;

    conf->loop = 1;

    while (conf->loop) {
        status = ncmosquitto_connect(conf);
        if (status != 0) {
            sleep(1);
            continue;
        }

/* The documentation says "0" would map to the default (1000ms), but
 * that does not work on some versions. */
#if LIBMOSQUITTO_MAJOR == 0
        status = mosquitto_loop(conf->mosq, /* timeout = */ 1000 /* ms */);
#else
        status = mosquitto_loop(conf->mosq, /* timeout[ms] = */ 1000,
                                            /* max_packets = */ 100);
#endif
        if (status == MOSQ_ERR_CONN_LOST) {
            conf->connected = false;
            continue;
        } else if (status != MOSQ_ERR_SUCCESS) {
            PLUGIN_ERROR("mosquitto_loop failed: %s", mosquitto_strerror(status));
            mosquitto_destroy(conf->mosq);
            conf->mosq = NULL;
            conf->connected = false;
            continue;
        }

        PLUGIN_DEBUG("mosquitto_loop succeeded.");
    }

    pthread_exit(0);
}

static int ncmosquitto_read(user_data_t *user_data)
{
    ncmosquitto_instance_t *conf = user_data->data;

    if (!conf->connected) {
        metric_family_append(&conf->fams[FAM_MOSQUITTO_UP], VALUE_GAUGE(0), &conf->labels, NULL);
        plugin_dispatch_metric_family_filtered(&conf->fams[FAM_MOSQUITTO_UP], conf->filter, 0);
        return 0;
    }

    metric_family_append(&conf->fams[FAM_MOSQUITTO_UP], VALUE_GAUGE(1), &conf->labels, NULL);

    for (size_t i = 0; i < ncmosquitto_subs_size ; i++) {
        if (conf->subs[i].updated) {
            int type = conf->fams[conf->subs[i].fam].type;
            if (type == METRIC_TYPE_COUNTER) {
                metric_family_append(&conf->fams[conf->subs[i].fam],
                                     VALUE_COUNTER(conf->subs[i].uint64), &conf->labels, NULL);
            } else if (type == METRIC_TYPE_GAUGE) {
                metric_family_append(&conf->fams[conf->subs[i].fam],
                                     VALUE_GAUGE(conf->subs[i].float64), &conf->labels, NULL);
            }
        }
    }

    plugin_dispatch_metric_family_array_filtered(conf->fams, FAM_MOSQUITTO_MAX, conf->filter, 0);

    return 0;
}

/*
 * <Instance "name">
 *   Host "example.com"
 *   Port 1883
 *   ClientId "ncollectd"
 *   User "guest"
 *   Password "secret"
 *   CACert "ca.pem"                     Enables TLS if set
 *   CertificateFile "client-cert.pem"   optional
 *   CertificateKeyFile "client-key.pem" optional
 *   TLSProtocol "tlsv1.2"               optional
 * </Instance>
 */
static int ncmosquitto_config_instance(config_item_t *ci)
{
    ncmosquitto_instance_t *conf = calloc(1, sizeof(*conf));
    if (conf == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    conf->name = NULL;
    int status = cf_util_get_string(ci, &conf->name);
    if (status != 0) {
        ncmosquitto_free(conf);
        return status;
    }

    memcpy(conf->fams, fams, sizeof(conf->fams[0])*FAM_MOSQUITTO_MAX);

    conf->host = strdup(MQTT_DEFAULT_HOST);
    conf->port = MQTT_DEFAULT_PORT;
    conf->client_id = NULL;
    conf->clean_session = true;
    conf->qos = 2;

    conf->subs = malloc(sizeof(*conf->subs)*ncmosquitto_subs_size);
    if (conf->subs == NULL) {
        PLUGIN_ERROR("malloc failed.");
        ncmosquitto_free(conf);
        return -1;
    }
    conf->subs_size = ncmosquitto_subs_size;
    memcpy(conf->subs, ncmosquitto_subs, sizeof(*conf->subs)*ncmosquitto_subs_size);

    C_COMPLAIN_INIT(&conf->complaint_cantpublish);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &conf->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_port_number(child, &conf->port);
        } else if (strcasecmp("qos", child->key) == 0) {
            int qos = -1;
            status = cf_util_get_int(child, &qos);
            if ((status != 0) || (qos < 0) || (qos > 2)) {
                PLUGIN_ERROR("Not a valid QoS setting.");
                status = -1;
            } else {
                conf->qos = qos;
            }
        } else if (strcasecmp("client-id", child->key) == 0) {
            status = cf_util_get_string(child, &conf->client_id);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &conf->username);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &conf->password);
        } else if (strcasecmp("clean-session", child->key) == 0) {
            status = cf_util_get_boolean(child, &conf->clean_session);
        } else if (strcasecmp("ca-cert", child->key) == 0) {
            status = cf_util_get_string(child, &conf->cacertificatefile);
        } else if (strcasecmp("certificate-file", child->key) == 0) {
            status = cf_util_get_string(child, &conf->certificatefile);
        } else if (strcasecmp("certificate-key-file", child->key) == 0) {
            status = cf_util_get_string(child, &conf->certificatekeyfile);
        } else if (strcasecmp("tls-protocol", child->key) == 0) {
            status = cf_util_get_string(child, &conf->tlsprotocol);
        } else if (strcasecmp("cipher-suite", child->key) == 0) {
            status = cf_util_get_string(child, &conf->ciphersuite);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &conf->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &conf->filter);
        } else {
            PLUGIN_ERROR("Unknown config option: %s", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        ncmosquitto_free(conf);
        return status;
    }

    label_set_add(&conf->labels, true, "instance", conf->name);

    status = plugin_thread_create(&conf->thread,
                                   /* func  = */ ncmosquitto_subscribers_thread,
                                   /* args  = */ conf,
                                   /* name  = */ "mosquitto");
    if (status != 0) {
        PLUGIN_ERROR("pthread_create failed: %s", STRERRNO);
        ncmosquitto_free(conf);
        return -1;
    }

    return plugin_register_complex_read("mosquitto", conf->name, ncmosquitto_read, interval,
                                        &(user_data_t){.data=conf, .free_func=ncmosquitto_free, });
}

/*
 * <Plugin mosquitto>
 *   <Instance  "name">
 *       # ...
 *   </Instance>
 * </Plugin>
 */
static int ncmosquitto_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = ncmosquitto_config_instance(child);
        } else {
            PLUGIN_ERROR("Unknown config option: %s", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int ncmosquitto_init(void)
{
    mosquitto_lib_init();
    return 0;
}

void module_register(void)
{
    plugin_register_config("mosquitto", ncmosquitto_config);
    plugin_register_init("mosquitto", ncmosquitto_init);
}
