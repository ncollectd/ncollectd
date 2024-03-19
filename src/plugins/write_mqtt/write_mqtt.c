// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2014 Marc Falzon
// SPDX-FileCopyrightText: Copyright (C) 2014,2015 Florian octo Forster
// SPDX-FileContributor: Marc Falzon <marc at baha dot mu>
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Jan-Piet Mens <jpmens at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/complain.h"
#include "libformat/format.h"

#include <mosquitto.h>

#define MQTT_MAX_TOPIC_SIZE 1024
#define MQTT_MAX_MESSAGE_SIZE MQTT_MAX_TOPIC_SIZE + 1024
#define MQTT_DEFAULT_HOST "localhost"
#define MQTT_DEFAULT_PORT 1883
#define MQTT_DEFAULT_TOPIC_PREFIX "ncollectd"
#define MQTT_DEFAULT_TOPIC "ncollectd/#"
#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE 60
#endif
#ifndef SSL_VERIFY_PEER
#define SSL_VERIFY_PEER 1
#endif

typedef struct {
    char *name;

    struct mosquitto *mosq;
    bool connected;

    char *host;
    int port;
    char *topic;

    char *client_id;
    char *username;
    char *password;
    int qos;
    char *cacertificatefile;
    char *certificatefile;
    char *certificatekeyfile;
    char *tlsprotocol;
    char *ciphersuite;

    /* For publishing */
    bool store_rates;
    bool retain;

    format_stream_metric_t format_metric;
    format_notification_t format_notification;

    c_complain_t complaint_cantpublish;
} mqtt_client_conf_t;

#if LIBMOSQUITTO_MAJOR == 0
static char const *mosquitto_strerror(int code) {
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

static void mqtt_free(void *arg)
{
    mqtt_client_conf_t *conf = arg;

    if (conf == NULL)
        return;

    if (conf->connected)
        (void)mosquitto_disconnect(conf->mosq);
    conf->connected = false;
    (void)mosquitto_destroy(conf->mosq);

    free(conf->host);
    free(conf->username);
    free(conf->topic);
    free(conf->password);
    free(conf->client_id);
    free(conf);
}

static int mqtt_reconnect(mqtt_client_conf_t *conf)
{
    int status;

    if (conf->connected)
        return 0;

    status = mosquitto_reconnect(conf->mosq);
    if (status != MOSQ_ERR_SUCCESS) {
        ERROR("mqtt_connect_broker: mosquitto_connect failed: %s",
                    (status == MOSQ_ERR_ERRNO) ? STRERRNO : mosquitto_strerror(status));
        return -1;
    }

    conf->connected = true;

    c_release(LOG_INFO, &conf->complaint_cantpublish,
                        "mqtt plugin: successfully reconnected to broker \"%s:%d\"",
                        conf->host, conf->port);

    return 0;
}

static int mqtt_connect(mqtt_client_conf_t *conf)
{
    char const *client_id;
    int status;

    if (conf->mosq != NULL)
        return mqtt_reconnect(conf);

    if (conf->client_id)
        client_id = conf->client_id;
    else
        client_id = plugin_get_hostname();

#if LIBMOSQUITTO_MAJOR == 0
    conf->mosq = mosquitto_new(client_id, /* user data = */ conf);
#else
    conf->mosq = mosquitto_new(client_id, true, /* user data = */ conf);
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
            PLUGIN_ERROR("cannot mosquitto_tls_set: %s",
                        mosquitto_strerror(status));
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
        status =
                mosquitto_username_pw_set(conf->mosq, conf->username, conf->password);
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
                               MQTT_KEEPALIVE, conf->clean_session);
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

    conf->connected = true;
    return 0;
}

static int publish(mqtt_client_conf_t *conf, void const *payload, size_t payload_len)
{

    int status = mqtt_connect(conf);
    if (status != 0) {
        PLUGIN_ERROR("unable to reconnect to broker");
        return status;
    }

    status = mosquitto_publish(conf->mosq, /* message_id */ NULL, conf->topic,
#if LIBMOSQUITTO_MAJOR == 0
                               (uint32_t)payload_len, payload,
#else
                               (int)payload_len, payload,
#endif
                               conf->qos, conf->retain);
    if (status != MOSQ_ERR_SUCCESS) {
        c_complain(LOG_ERR, &conf->complaint_cantpublish,
                             "write_mqtt plugin: mosquitto_publish failed: %s",
                             (status == MOSQ_ERR_ERRNO) ? STRERRNO
                             : mosquitto_strerror(status));
        /* Mark our connection "down" regardless of the error as a safety
         * measure; we will try to reconnect the next time we have to publish a
         * message */
        conf->connected = false;
        mosquitto_disconnect(conf->mosq);
        return -1;
    }

#if LIBMOSQUITTO_MAJOR == 0
    status = mosquitto_loop(conf->mosq, /* timeout = */ 1000 /* ms */);
#else
    status = mosquitto_loop(conf->mosq, /* timeout[ms] = */ 1000,
                                        /* max_packets = */ 1);
#endif

    if (status != MOSQ_ERR_SUCCESS) {
        c_complain(LOG_ERR, &conf->complaint_cantpublish,
                            "mqtt plugin: mosquitto_loop failed: %s",
                             (status == MOSQ_ERR_ERRNO) ? STRERRNO
                                                        : mosquitto_strerror(status));
        /* Mark our connection "down" regardless of the error as a safety
         * measure; we will try to reconnect the next time we have to publish a
         * message */
        conf->connected = 0;
        mosquitto_disconnect(conf->mosq);
        return -1;
    }

    return 0;
}

static int mqtt_notify(notification_t const *n, user_data_t *user_data)
{
    if ((n == NULL) || (user_data == NULL) || (user_data->data == NULL))
        return EINVAL;

    mqtt_client_conf_t *conf = user_data->data;

//  char payload[MQTT_MAX_MESSAGE_SIZE];
    strbuf_t buf = STRBUF_CREATE;
    int status = format_notification(conf->format_notification, &buf, n);
    if (status != 0) {
        PLUGIN_ERROR("format notification failed.");
        return status;
    }
    size_t size = strbuf_len(&buf);

    status = publish(conf, buf.ptr, size);
    if (status != 0) {
        PLUGIN_ERROR("publish failed: %s", mosquitto_strerror(status));
        return status;
    }

    return status;
}

static int mqtt_write(metric_family_t const *fam, user_data_t *user_data)
{
    if ((fam == NULL) || (user_data == NULL) || (user_data->data == NULL))
        return EINVAL;
    mqtt_client_conf_t *conf = user_data->data;

//  char payload[MQTT_MAX_MESSAGE_SIZE];
    strbuf_t buf = STRBUF_CREATE;
    format_stream_metric_ctx_t ctx = {0};

    int status = format_stream_metric_begin(&ctx, conf->format_metric, &buf);
    status |= format_stream_metric_family(&ctx, fam);
    status |= format_stream_metric_end(&ctx);
    if (status != 0) {
        PLUGIN_ERROR("format metric failed.");
        return status;
    }
    size_t size = strbuf_len(&buf);

    status = publish(conf, buf.ptr, size);
    if (status != 0) {
        PLUGIN_ERROR("publish failed: %s", mosquitto_strerror(status));
        return status;
    }

    return status;
}

static int mqtt_config_instance(config_item_t *ci)
{
    mqtt_client_conf_t *conf = calloc(1, sizeof(*conf));
    if (conf == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &conf->name);
    if (status != 0) {
        mqtt_free(conf);
        return status;
    }

    conf->host = strdup(MQTT_DEFAULT_HOST);
    conf->port = MQTT_DEFAULT_PORT;
    conf->client_id = NULL;
    conf->qos = 0;
    conf->store_rates = true;
    conf->format_metric = FORMAT_STREAM_METRIC_OPENMETRICS_TEXT;
    conf->format_notification = FORMAT_NOTIFICATION_JSON;
    C_COMPLAIN_INIT(&conf->complaint_cantpublish);

    cf_send_t send = SEND_METRICS;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &conf->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_port_number(child, &conf->port);
        } else if (strcasecmp("topic", child->key) == 0) {
            status = cf_util_get_string(child, &conf->topic);
        } else if (strcasecmp("client-id", child->key) == 0) {
            status = cf_util_get_string(child, &conf->client_id);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &conf->username);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &conf->password);
        } else if (strcasecmp("qos", child->key) == 0) {
            int tmp = -1;
            status = cf_util_get_int(child, &tmp);
            if ((status != 0) || (tmp < 0) || (tmp > 2)) {
                PLUGIN_ERROR("Not a valid QoS setting.");
                status = -1;
            } else {
                conf->qos = tmp;
            }
        } else if (strcasecmp("store-rates", child->key) == 0) {
            status = cf_util_get_boolean(child, &conf->store_rates);
        } else if (strcasecmp("retain", child->key) == 0) {
            status = cf_util_get_boolean(child, &conf->retain);
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
        } else if (strcasecmp("format-metric", child->key) == 0) {
            status = config_format_stream_metric(child, &conf->format_metric);
        } else if (strcasecmp("format-notification", child->key) == 0) {
            status = config_format_notification(child, &conf->format_notification);
        } else if (strcasecmp("write", child->key) == 0) {
            status = cf_uti_get_send(child, &send);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        mqtt_free(conf);
        return -1;
    }

    if(conf->topic == NULL) {
        PLUGIN_ERROR("Missing topic key.");
        mqtt_free(conf);
        return -1;
    }

    user_data_t user_data = (user_data_t){ .data = conf, .free_func = mqtt_free };

    if (send == SEND_NOTIFICATIONS)
        return plugin_register_notification("write_mqtt", conf->name, mqtt_notify, &user_data);

    return plugin_register_write("write_mqtt", conf->name, mqtt_write, NULL, 0, 0, &user_data);
}

static int mqtt_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = mqtt_config_instance(child);
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

static int mqtt_init(void)
{
    mosquitto_lib_init();
    return 0;
}

void module_register(void)
{
    plugin_register_config("write_mqtt", mqtt_config);
    plugin_register_init("write_mqtt", mqtt_init);
}
