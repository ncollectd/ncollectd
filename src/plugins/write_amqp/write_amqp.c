// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009 Sebastien Pahl
// SPDX-FileCopyrightText: Copyright (C) 2010-2012 Florian Forster
// SPDX-FileContributor: Sebastien Pahl <sebastien.pahl at dotcloud.com>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>

#include "plugin.h"
#include "libutils/avltree.h"
#include "libutils/common.h"
#include "libutils/random.h"
#include "libformat/format.h"

#ifdef HAVE_RABBITMQ_C_INCLUDE_PATH
#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/framing.h>
#else
#include <amqp.h>
#include <amqp_framing.h>
#endif

#ifdef HAVE_AMQP_TCP_SOCKET_H
#ifdef HAVE_RABBITMQ_C_INCLUDE_PATH
#include <rabbitmq-c/ssl_socket.h>
#include <rabbitmq-c/tcp_socket.h>
#else
#include <amqp_ssl_socket.h>
#include <amqp_tcp_socket.h>
#endif
#endif
#ifdef HAVE_AMQP_SOCKET_H
#ifdef HAVE_RABBITMQ_C_INCLUDE_PATH
#include <rabbitmq-c/socket.h>
#else
#include <amqp_socket.h>
#endif
#endif

#ifdef HAVE_AMQP_TCP_SOCKET
#if defined HAVE_DECL_AMQP_SOCKET_CLOSE && !HAVE_DECL_AMQP_SOCKET_CLOSE
/* rabbitmq-c does not currently ship amqp_socket.h
 * and, thus, does not define this function. */
int amqp_socket_close(amqp_socket_t *);
#endif
#endif

/* Defines for the delivery mode. I have no idea why they're not defined by the
 * library.. */
#define CAMQP_DM_VOLATILE 1
#define CAMQP_DM_PERSISTENT 2

#define CAMQP_CHANNEL 1

typedef struct camqp_config_s {
    char *name;
    char **hosts;
    size_t hosts_count;
    int port;
    char *vhost;
    char *user;
    char *password;
    bool tls_enabled;
    bool tls_verify_peer;
    bool tls_verify_hostname;
    char *tls_cacert;
    char *tls_client_cert;
    char *tls_client_key;
    char *exchange;
    char *exchange_type;
    char *routing_key;

    /* Number of seconds to wait before connection is retried */
    int connection_retry_delay;

    uint8_t delivery_mode;
    bool store_rates;
    format_stream_metric_t format_metric;
    format_notification_t format_notification;

    amqp_connection_state_t connection;
    pthread_mutex_t lock;
} camqp_config_t;

static void camqp_close_connection(camqp_config_t *conf)
{
    if ((conf == NULL) || (conf->connection == NULL))
        return;

    int sockfd = amqp_get_sockfd(conf->connection);
    amqp_channel_close(conf->connection, CAMQP_CHANNEL, AMQP_REPLY_SUCCESS);
    amqp_connection_close(conf->connection, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(conf->connection);
    close(sockfd);
    conf->connection = NULL;
}

static void camqp_config_free(void *arg)
{
    if (arg == NULL)
        return;

    camqp_config_t *conf = arg;

    camqp_close_connection(conf);

    free(conf->name);
    strarray_free(conf->hosts, conf->hosts_count);
    free(conf->vhost);
    free(conf->user);
    free(conf->password);
    free(conf->tls_cacert);
    free(conf->tls_client_cert);
    free(conf->tls_client_key);
    free(conf->exchange);
    free(conf->exchange_type);
    free(conf->routing_key);

    free(conf);
}

static char *camqp_bytes_cstring(amqp_bytes_t *in)
{
    char *ret;

    if ((in == NULL) || (in->bytes == NULL))
        return NULL;

    ret = malloc(in->len + 1);
    if (ret == NULL)
        return NULL;

    memcpy(ret, in->bytes, in->len);
    ret[in->len] = 0;

    return ret;
}

static bool camqp_is_error(camqp_config_t *conf)
{
    amqp_rpc_reply_t r;

    r = amqp_get_rpc_reply(conf->connection);
    if (r.reply_type == AMQP_RESPONSE_NORMAL)
        return false;

    return true;
}

static char *camqp_strerror(camqp_config_t *conf, char *buffer, size_t buffer_size)
{
    amqp_rpc_reply_t r = amqp_get_rpc_reply(conf->connection);

    switch (r.reply_type) {
    case AMQP_RESPONSE_NORMAL:
        sstrncpy(buffer, "Success", buffer_size);
        break;

    case AMQP_RESPONSE_NONE:
        sstrncpy(buffer, "Missing RPC reply type", buffer_size);
        break;

    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
#ifdef HAVE_AMQP_RPC_REPLY_T_LIBRARY_ERRNO
        if (r.library_errno)
            return sstrerror(r.library_errno, buffer, buffer_size);
#else
        if (r.library_error)
            return sstrerror(r.library_error, buffer, buffer_size);
#endif
        else
            sstrncpy(buffer, "End of stream", buffer_size);
        break;

    case AMQP_RESPONSE_SERVER_EXCEPTION:
        if (r.reply.id == AMQP_CONNECTION_CLOSE_METHOD) {
            amqp_connection_close_t *m = r.reply.decoded;
            char *tmp = camqp_bytes_cstring(&m->reply_text);
            ssnprintf(buffer, buffer_size, "Server connection error %d: %s", m->reply_code, tmp);
            free(tmp);
        } else if (r.reply.id == AMQP_CHANNEL_CLOSE_METHOD) {
            amqp_channel_close_t *m = r.reply.decoded;
            char *tmp = camqp_bytes_cstring(&m->reply_text);
            ssnprintf(buffer, buffer_size, "Server channel error %d: %s", m->reply_code, tmp);
            free(tmp);
        } else {
            ssnprintf(buffer, buffer_size, "Server error method %#" PRIx32, r.reply.id);
        }
        break;

    default:
        ssnprintf(buffer, buffer_size, "Unknown reply type %i", (int)r.reply_type);
    }

    return buffer;
}

#ifdef HAVE_AMQP_RPC_REPLY_T_LIBRARY_ERRNO
static int camqp_create_exchange(camqp_config_t *conf)
{
    amqp_exchange_declare_ok_t *ed_ret;

    if (conf->exchange_type == NULL)
        return 0;

    ed_ret = amqp_exchange_declare(conf->connection,
               /* channel     = */ CAMQP_CHANNEL,
               /* exchange    = */ amqp_cstring_bytes(conf->exchange),
               /* type        = */ amqp_cstring_bytes(conf->exchange_type),
               /* passive     = */ 0,
               /* durable     = */ 0,
               /* auto_delete = */ 1,
               /* arguments   = */ AMQP_EMPTY_TABLE);
    if ((ed_ret == NULL) && camqp_is_error(conf)) {
        char errbuf[1024];
        PLUGIN_ERROR("amqp_exchange_declare failed: %s",
                     camqp_strerror(conf, errbuf, sizeof(errbuf)));
        camqp_close_connection(conf);
        return -1;
    }

    PLUGIN_INFO("Successfully created exchange \"%s\" with type \"%s\".",
                conf->exchange, conf->exchange_type);
    return 0;
}
#else
static int camqp_create_exchange(camqp_config_t *conf)
{
    amqp_exchange_declare_ok_t *ed_ret;
    amqp_table_t argument_table;
    struct amqp_table_entry_t_ argument_table_entries[1];

    if (conf->exchange_type == NULL)
        return 0;

    /* Valid arguments: "auto_delete", "internal" */
    argument_table.num_entries = STATIC_ARRAY_SIZE(argument_table_entries);
    argument_table.entries = argument_table_entries;
    argument_table_entries[0].key = amqp_cstring_bytes("auto_delete");
    argument_table_entries[0].value.kind = AMQP_FIELD_KIND_BOOLEAN;
    argument_table_entries[0].value.value.boolean = 1;

    ed_ret = amqp_exchange_declare(conf->connection,
               /* channel     = */ CAMQP_CHANNEL,
               /* exchange    = */ amqp_cstring_bytes(conf->exchange),
               /* type        = */ amqp_cstring_bytes(conf->exchange_type),
               /* passive     = */ 0,
               /* durable     = */ 0,
#if defined(AMQP_VERSION) && AMQP_VERSION >= 0x00060000
               /* auto delete = */ 0,
               /* internal    = */ 0,
#endif
               /* arguments   = */ argument_table);
    if ((ed_ret == NULL) && camqp_is_error(conf)) {
        char errbuf[1024];
        PLUGIN_ERROR("amqp_exchange_declare failed: %s",
                     camqp_strerror(conf, errbuf, sizeof(errbuf)));
        camqp_close_connection(conf);
        return -1;
    }

    PLUGIN_INFO("Successfully created exchange \"%s\" with type \"%s\".",
                conf->exchange, conf->exchange_type);
    return 0;
}
#endif

static int camqp_connect(camqp_config_t *conf)
{
    static time_t last_connect_time;

    amqp_rpc_reply_t reply;
    int status;
#ifdef HAVE_AMQP_TCP_SOCKET
    amqp_socket_t *socket;
#else
    int sockfd;
#endif

    if (conf->connection != NULL)
        return 0;

    time_t now = time(NULL);
    if (now < (last_connect_time + conf->connection_retry_delay)) {
        PLUGIN_DEBUG("skipping connection retry, ConnectionRetryDelay: %d",
                     conf->connection_retry_delay);
        return 1;
    } else {
        PLUGIN_DEBUG("retrying connection");
        last_connect_time = now;
    }

    conf->connection = amqp_new_connection();
    if (conf->connection == NULL) {
        PLUGIN_ERROR("amqp_new_connection failed.");
        return ENOMEM;
    }

    char *host = conf->hosts[cdrand_u() % conf->hosts_count];
    PLUGIN_INFO("Connecting to %s", host);

#ifdef HAVE_AMQP_TCP_SOCKET
#define CLOSE_SOCKET() /* amqp_destroy_connection() closes the socket for us  */

    if (conf->tls_enabled) {
        socket = amqp_ssl_socket_new(conf->connection);
        if (!socket) {
            PLUGIN_ERROR("amqp_ssl_socket_new failed.");
            amqp_destroy_connection(conf->connection);
            conf->connection = NULL;
            return ENOMEM;
        }

#if AMQP_VERSION >= 0x00080000
        amqp_ssl_socket_set_verify_peer(socket, conf->tls_verify_peer);
        amqp_ssl_socket_set_verify_hostname(socket, conf->tls_verify_hostname);
#endif

        if (conf->tls_cacert) {
            status = amqp_ssl_socket_set_cacert(socket, conf->tls_cacert);
            if (status < 0) {
                PLUGIN_ERROR("amqp_ssl_socket_set_cacert failed: %s",
                             amqp_error_string2(status));
                amqp_destroy_connection(conf->connection);
                conf->connection = NULL;
                return status;
            }
        }
        if (conf->tls_client_cert && conf->tls_client_key) {
            status = amqp_ssl_socket_set_key(socket, conf->tls_client_cert, conf->tls_client_key);
            if (status < 0) {
                PLUGIN_ERROR("amqp_ssl_socket_set_key failed: %s",
                             amqp_error_string2(status));
                amqp_destroy_connection(conf->connection);
                conf->connection = NULL;
                return status;
            }
        }
    } else {
        socket = amqp_tcp_socket_new(conf->connection);
        if (!socket) {
            PLUGIN_ERROR("amqp_tcp_socket_new failed.");
            amqp_destroy_connection(conf->connection);
            conf->connection = NULL;
            return ENOMEM;
        }
    }

    status = amqp_socket_open(socket, host, conf->port);
    if (status < 0) {
        PLUGIN_ERROR("amqp_socket_open failed: %s", amqp_error_string2(status));
        amqp_destroy_connection(conf->connection);
        conf->connection = NULL;
        return status;
    }
#else /* HAVE_AMQP_TCP_SOCKET */
#define CLOSE_SOCKET() close(sockfd)
    /* this interface is deprecated as of rabbitmq-c 0.4 */
    sockfd = amqp_open_socket(host, conf->port);
    if (sockfd < 0) {
        status = (-1) * sockfd;
        PLUGIN_ERROR("amqp_open_socket failed: %s", STRERROR(status));
        amqp_destroy_connection(conf->connection);
        conf->connection = NULL;
        return status;
    }
    amqp_set_sockfd(conf->connection, sockfd);
#endif

    reply = amqp_login(conf->connection, conf->vhost,
/* channel max    = */ 0,
/* frame max      = */ 131072,
/* heartbeat      = */ 0,
/* authentication = */ AMQP_SASL_METHOD_PLAIN,
                       conf->user, conf->password);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        PLUGIN_ERROR("amqp_login (vhost = %s, user = %s) failed.",
                     conf->vhost, conf->user);
        amqp_destroy_connection(conf->connection);
        CLOSE_SOCKET();
        conf->connection = NULL;
        return 1;
    }

    amqp_channel_open(conf->connection, /* channel = */ 1);
    /* FIXME: Is checking "reply.reply_type" really correct here? How does
     * it get set? --octo */
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        PLUGIN_ERROR("amqp_channel_open failed.");
        amqp_connection_close(conf->connection, AMQP_REPLY_SUCCESS);
        amqp_destroy_connection(conf->connection);
        CLOSE_SOCKET();
        conf->connection = NULL;
        return 1;
    }

    PLUGIN_INFO("Successfully opened connection to vhost \"%s\" on %s:%i.",
                conf->vhost, host, conf->port);

    status = camqp_create_exchange(conf);
    if (status != 0)
        return status;
    return 0;
}

/* XXX: You must hold "conf->lock" when calling this function! */
static int camqp_write_locked(camqp_config_t *conf, const char *buffer,
                              const char *content_type)
{
    int status;

    status = camqp_connect(conf);
    if (status != 0)
        return status;

    amqp_basic_properties_t props = {._flags = AMQP_BASIC_CONTENT_TYPE_FLAG |
                                               AMQP_BASIC_DELIVERY_MODE_FLAG |
                                               AMQP_BASIC_APP_ID_FLAG,
                                      .delivery_mode = conf->delivery_mode,
                                      .app_id = amqp_cstring_bytes(PACKAGE_NAME)};


    props.content_type = amqp_cstring_bytes(content_type);

    status = amqp_basic_publish(conf->connection,
              /* channel   = */ 1,
                                amqp_cstring_bytes(conf->exchange),
                                (conf->routing_key != NULL) ? amqp_cstring_bytes(conf->routing_key)
                                                            : AMQP_EMPTY_BYTES,
              /* mandatory = */ 0,
              /* immediate = */ 0, &props, amqp_cstring_bytes(buffer));
    if (status != 0) {
        PLUGIN_ERROR("amqp_basic_publish failed with status %i.", status);
        camqp_close_connection(conf);
    }

    return status;
}

static int camqp_notify(notification_t const *n, user_data_t *user_data)
{
    if ((n == NULL) || (user_data == NULL))
        return -1;

    camqp_config_t *conf = user_data->data;

    strbuf_t buf = STRBUF_CREATE;

    int status = format_notification(conf->format_notification, &buf, n);
    if (status != 0) {
        PLUGIN_ERROR("Failed to format notification.");
        strbuf_destroy(&buf);
        return 0;
    }

    const char *content_type = format_notification_content_type(conf->format_notification);
    pthread_mutex_lock(&conf->lock);
    status = camqp_write_locked(conf, buf.ptr, content_type);
    pthread_mutex_unlock(&conf->lock);

    if (status != 0) {
        // FIXME ERROR
    }

    strbuf_destroy(&buf);

    return 0;
}

static int camqp_write(metric_family_t const *fam, user_data_t *user_data)
{
    camqp_config_t *conf = user_data->data;

    strbuf_t buf = STRBUF_CREATE;
    format_stream_metric_ctx_t ctx = {0};

    int status = format_stream_metric_begin(&ctx, conf->format_metric, &buf);
    status |= format_stream_metric_family(&ctx, fam);
    status |= format_stream_metric_end(&ctx);
    if (status != 0) {
        PLUGIN_ERROR("Failed to format metric.");
        strbuf_destroy(&buf);
        return 0;
    }

    const char *content_type = format_stream_metric_content_type(conf->format_metric);
    pthread_mutex_lock(&conf->lock);
    status = camqp_write_locked(conf, buf.ptr, content_type);
    pthread_mutex_unlock(&conf->lock);

    if (status != 0) {
        // FIXME ERROR
    }

    strbuf_destroy(&buf);

    return 0;
}

static int camqp_config_instance(config_item_t *ci)
{
    camqp_config_t *conf = calloc(1, sizeof(*conf));
    if (conf == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return ENOMEM;
    }

    int status = cf_util_get_string(ci, &conf->name);
    if (status != 0) {
        free(conf);
        return status;
    }

    conf->format_metric = FORMAT_STREAM_METRIC_OPENMETRICS_TEXT;
    conf->format_notification = FORMAT_NOTIFICATION_JSON;
    conf->hosts = NULL;
    conf->hosts_count = 0;
    conf->port = 5672;
    conf->vhost = strdup("/");
    if (conf->vhost == NULL) {
        PLUGIN_ERROR("strdup failed.");
        camqp_config_free(conf);
        return -1;
    }

    conf->user = strdup("guest");
    if (conf->user == NULL) {
        PLUGIN_ERROR("strdup failed.");
        camqp_config_free(conf);
        return -1;
    }

    conf->password = strdup("guest");
    if (conf->password == NULL) {
        PLUGIN_ERROR("strdup failed.");
        camqp_config_free(conf);
        return -1;
    }

    conf->tls_enabled = false;
    conf->tls_verify_peer = true;
    conf->tls_verify_hostname = true;
    conf->tls_cacert = NULL;
    conf->tls_client_cert = NULL;
    conf->tls_client_key = NULL;
    conf->exchange = strdup("amq.fanout");
    if (conf->exchange == NULL) {
        PLUGIN_ERROR("strdup failed.");
        camqp_config_free(conf);
        return -1;
    }

    conf->routing_key = NULL;
    conf->connection_retry_delay = 0;

    conf->delivery_mode = CAMQP_DM_VOLATILE;
    conf->store_rates = false;

    conf->connection = NULL;

    pthread_mutex_init(&conf->lock, NULL);

    cf_send_t send = SEND_METRICS;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            for (int j = 0; j < child->values_num; j++) {
                if (child->values[j].type != CONFIG_TYPE_STRING) {
                    status = EINVAL;
                    PLUGIN_ERROR("'host' arguments must be strings");
                    break;
                }

                status = strarray_add(&conf->hosts, &conf->hosts_count,
                                                    child->values[j].value.string);
                if (status) {
                    PLUGIN_ERROR("'host' configuration failed: %d", status);
                    break;
                }
            }
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_port_number(child, &conf->port);
        } else if (strcasecmp("vhost", child->key) == 0) {
            status = cf_util_get_string(child, &conf->vhost);
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &conf->user);
        } else if (strcasecmp("user-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &conf->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &conf->password);
        } else if (strcasecmp("password_env", child->key) == 0) {
            status = cf_util_get_string_env(child, &conf->password);
        } else if (strcasecmp("tls-enabled", child->key) == 0) {
            status = cf_util_get_boolean(child, &conf->tls_enabled);
        } else if (strcasecmp("tls-verify-peer", child->key) == 0) {
            status = cf_util_get_boolean(child, &conf->tls_verify_peer);
        } else if (strcasecmp("tls-verify-hostname", child->key) == 0) {
            status = cf_util_get_boolean(child, &conf->tls_verify_hostname);
        } else if (strcasecmp("tls-ca-cert", child->key) == 0) {
            status = cf_util_get_string(child, &conf->tls_cacert);
        } else if (strcasecmp("tls-client-cert", child->key) == 0) {
            status = cf_util_get_string(child, &conf->tls_client_cert);
        } else if (strcasecmp("tls-client-key", child->key) == 0) {
            status = cf_util_get_string(child, &conf->tls_client_key);
        } else if (strcasecmp("exchange", child->key) == 0) {
            status = cf_util_get_string(child, &conf->exchange);
        } else if (strcasecmp("exchange-type", child->key) == 0) {
            status = cf_util_get_string(child, &conf->exchange_type);
        } else if (strcasecmp("routing-key", child->key) == 0) {
            status = cf_util_get_string(child, &conf->routing_key);
        } else if ((strcasecmp("persistent", child->key) == 0)) {
            bool tmp = 0;
            status = cf_util_get_boolean(child, &tmp);
            if (tmp)
                conf->delivery_mode = CAMQP_DM_PERSISTENT;
            else
                conf->delivery_mode = CAMQP_DM_VOLATILE;
#if 0
        } else if (strcasecmp("StoreRates", child->key) == 0) {
            status = cf_util_get_boolean(child, &conf->store_rates);
            (void)cf_util_get_flag(child, &conf->graphite_flags, GRAPHITE_STORE_RATES);
#endif
        } else if (strcasecmp("write", child->key) == 0) {
            status = cf_uti_get_send(child, &send);
        } else if (strcasecmp("format-metric", child->key) == 0) {
            status = config_format_stream_metric(child, &conf->format_metric);
        } else if (strcasecmp("format-notification", child->key) == 0) {
            status = config_format_notification(child, &conf->format_notification);

        } else if (strcasecmp("connection-retry-delay", child->key) == 0) {
            status = cf_util_get_int(child, &conf->connection_retry_delay);
        } else {
            PLUGIN_ERROR("Ignoring unknown configuration option \"%s\".", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status == 0 && conf->hosts_count == 0) {
        status = strarray_add(&conf->hosts, &conf->hosts_count, "localhost");
        if (status)
            PLUGIN_ERROR("Host configuration failed: %d", status);
    }

#if !defined(AMQP_VERSION) || AMQP_VERSION < 0x00040000
    if (status == 0 && conf->tls_enabled) {
        PLUGIN_ERROR("'tls-enabled' is set but not supported. "
                     "Rebuild ncollectd with rabbitmq-c >= 0.4");
        status = -1;
    }
#endif
#if !defined(AMQP_VERSION) || AMQP_VERSION < 0x00080000
    if (status == 0 && (!conf->tls_verify_peer || !conf->tls_verify_hostname)) {
        PLUGIN_ERROR("Disabling 'tls-verify-*' is not supported. "
                     "Rebuild ncollectd with rabbitmq-c >= 0.8");
        status = -1;
    }
#endif
    if (status == 0 && (conf->tls_client_cert != NULL || conf->tls_client_key != NULL)) {
        if (conf->tls_client_cert == NULL || conf->tls_client_key == NULL) {
            PLUGIN_ERROR("Only one of tls-client-cert/tls-client-key is "
                         "configured. need both or neither.");
            status = -1;
        }
    }

    if (status != 0) {
        camqp_config_free(conf);
        return status;
    }

    if (conf->exchange != NULL) {
        PLUGIN_DEBUG("camqp_config_connection: exchange = %s;", conf->exchange);
    }

    user_data_t user_data = (user_data_t){ .data = conf, .free_func = camqp_config_free };

    if (send == SEND_NOTIFICATIONS)
        return plugin_register_notification("write_amqp", conf->name, camqp_notify, &user_data);

    return plugin_register_write("write_amqp", conf->name, camqp_write, NULL, 0, 0, &user_data);
}

static int camqp_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = camqp_config_instance(child);
        } else {
            PLUGIN_WARNING("Unknown config option '%s'.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("write_amqp", camqp_config);
}
