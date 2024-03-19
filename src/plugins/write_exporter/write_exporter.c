// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2016 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/avltree.h"
#include "libutils/common.h"
#include "libutils/complain.h"
#include "libformat/format.h"

#include <microhttpd.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifndef EXPORTER_DEFAULT_STALENESS_DELTA
#define EXPORTER_DEFAULT_STALENESS_DELTA TIME_T_TO_CDTIME_T_STATIC(300)
#endif

#define ACCESS_DENIED "<html><head><title>Access denied</title></head>"\
                      "<body>Access denied</body></html>"

#define DIGEST_OPAQUE "af845c05130244b1b0204c8fe3f194d7"

#if MHD_VERSION >= 0x00097002
#define MHD_RESULT enum MHD_Result
#else
#define MHD_RESULT int
#endif

enum {
    AUTH_BASIC,
    AUTH_DIGEST
};

typedef struct {
    char *name;
    char *host;
    int port;
    char *private_key;
    char *private_key_pass;
    char *certificate;
    char *tls_priority;
    char *realm;
    char *user;
    char *password;
    int  authmethod;
    cdtime_t staleness_delta;
    struct MHD_Daemon *httpd;
    format_stream_metric_t format;
    c_avl_tree_t *metrics;
    pthread_mutex_t metrics_lock;
} exporter_t;

static int exporter_metric_cmp(void const *a, void const *b)
{
    metric_t const *m_a = (metric_t const *)a;
    metric_t const *m_b = (metric_t const *)b;

    if (m_a->label.num < m_b->label.num)
        return -1;
    else if (m_a->label.num > m_b->label.num)
        return 1;

    for (size_t i = 0; i < m_a->label.num; i++) {
        int status = strcmp(m_a->label.ptr[i].value, m_b->label.ptr[i].value);
        if (status != 0)
            return status;

#ifdef NCOLLECTD_DEBUG
        assert(strcmp(m_a->label.ptr[i].name, m_b->label.ptr[i].name) == 0);
#endif
    }
    return 0;
}

static MHD_RESULT exporter_auth_fail(exporter_t *exporter, struct MHD_Connection *connection)
{
    struct MHD_Response *res = NULL;

#if defined(MHD_VERSION) && MHD_VERSION >= 0x00090500
    res = MHD_create_response_from_buffer(strlen(ACCESS_DENIED),
                                          (void *)ACCESS_DENIED, MHD_RESPMEM_PERSISTENT);
#else
    res = MHD_create_response_from_data(strlen(ACCESS_DENIED), (void *)ACCESS_DENIED, 0, 1);
#endif
    if (res == NULL) {
        return MHD_NO;
    }

    MHD_RESULT result = 0;
    if (exporter->authmethod == AUTH_BASIC) {
        result = MHD_queue_basic_auth_fail_response(connection, exporter->realm, res);
    } else {
#if defined(MHD_VERSION) && MHD_VERSION >= 0x00096200
        result = MHD_queue_auth_fail_response2(connection, exporter->realm,
                                               DIGEST_OPAQUE, res, MHD_NO, MHD_DIGEST_ALG_AUTO);
#else
        result = MHD_queue_auth_fail_response(connection, exporter->realm,
                                              DIGEST_OPAQUE, res, MHD_NO);
#endif
    }

    MHD_destroy_response(res);
    return result;
}

static bool exporter_auth(exporter_t *exporter, struct MHD_Connection *connection)
{
    if (exporter->authmethod == AUTH_BASIC) {
        char *username = NULL;
        char *password = NULL;

        username = MHD_basic_auth_get_username_password (connection, &password);
        if ((username == NULL) || (password == NULL)) {
            free(username);
            free(password);
            return false;
        }
        if ((strcmp(username, exporter->user) != 0) ||
            (strcmp(password, exporter->password) != 0)) {
            free(username);
            free(password);
            return false;
        }
        free(username);
        free(password);
        return true;
    } else {
        char *username = MHD_digest_auth_get_username(connection);
        if (username == NULL) {
            free(username);
            return false;
        }
        if (strcmp(username, exporter->user) != 0) {
            free(username);
            return false;
        }
        free(username);
        int status = 0;
#if defined(MHD_VERSION) && MHD_VERSION >= 0x00096200
        status = MHD_digest_auth_check2(connection, exporter->realm,
                                        exporter->user, exporter->password,
                                        300, MHD_DIGEST_ALG_AUTO);
#else
        status = MHD_digest_auth_check(connection, exporter->realm,
                                       exporter->user, exporter->password, 300);
#endif
        if ( (status == MHD_INVALID_NONCE) || (status == MHD_NO) )
            return false;
        return true;
    }

    return true;
}

static MHD_RESULT http_handler(void *cls, struct MHD_Connection *connection,
                               __attribute__((unused)) const char *url, const char *method,
                               __attribute__((unused)) const char *version,
                               __attribute__((unused)) const char *upload_data,
                               __attribute__((unused)) size_t *upload_data_size,
                               void **connection_state)
{
    exporter_t *exporter = cls;

    if (strcmp(method, MHD_HTTP_METHOD_GET) != 0)
        return MHD_NO;

    /* On the first call for each connection, return without anything further.
     * Apparently not everything has been initialized yet or so; the docs are not
     * very specific on the issue. */
    if (*connection_state == NULL) {
        /* set to a random non-NULL pointer. */
        *connection_state = (void *)42;
        return MHD_YES;
    }

    if (exporter->user != NULL) {
        if (!exporter_auth(exporter, connection))
            return exporter_auth_fail(exporter, connection);
    }

    format_stream_metric_ctx_t ctx = {0};
    strbuf_t buf = STRBUF_CREATE;

    format_stream_metric_begin(&ctx, exporter->format, &buf);

    pthread_mutex_lock(&exporter->metrics_lock);
    char *unused_name;
    metric_family_t *fam;
    cdtime_t last = cdtime() - EXPORTER_DEFAULT_STALENESS_DELTA;
    c_avl_iterator_t *iter = c_avl_get_iterator(exporter->metrics);
    while (c_avl_iterator_next(iter, (void *)&unused_name, (void *)&fam) == 0) {
        if (fam->metric.num == 0)
            continue;

        for (size_t i=0; i < fam->metric.num; i++) {
            if ((fam->metric.ptr[i].time > 0) && (fam->metric.ptr[i].time < last)) {
                // delete
            }
        }

        format_stream_metric_family(&ctx, fam);
    }
    c_avl_iterator_destroy(iter);
    pthread_mutex_unlock(&exporter->metrics_lock);

    format_stream_metric_end(&ctx);

    if (exporter->format == FORMAT_STREAM_METRIC_OPENMETRICS_TEXT) {
        int status = strbuf_printf(&buf, "# ncollectd/write_exporter %s at %s\n# EOF\n", 
                                   PACKAGE_VERSION, plugin_get_hostname());
        if (status != 0)
            PLUGIN_WARNING("Cannot append comment to response.");
    }

    struct MHD_Response *res = NULL;
#if defined(MHD_VERSION) && MHD_VERSION >= 0x00090500
    res = MHD_create_response_from_buffer(buf.pos, buf.ptr, MHD_RESPMEM_MUST_COPY);
#else
    res = MHD_create_response_from_data(buf.pos, buf.ptr, 0, 1);
#endif
    strbuf_destroy(&buf);

    const char *content_type = format_stream_metric_content_type(exporter->format);
    if (content_type != NULL) {
        MHD_RESULT status = MHD_add_response_header(res, MHD_HTTP_HEADER_CONTENT_TYPE,
                                                         content_type);
        if (status == MHD_NO)
            PLUGIN_WARNING("Failed to add header content-type to response.");
    }

    MHD_RESULT status = MHD_queue_response(connection, MHD_HTTP_OK, res);

    MHD_destroy_response(res);

    return status;
}

static void exporter_logger(__attribute__((unused)) void *arg, char const *fmt, va_list ap)
{
    char errbuf[1024];
    vsnprintf(errbuf, sizeof(errbuf), fmt, ap);

    PLUGIN_ERROR("%s", errbuf);
}

#if MHD_VERSION >= 0x00090000
static int exporter_open_socket(exporter_t *exporter, int addrfamily)
{
    char service[NI_MAXSERV];
    ssnprintf(service, sizeof(service), "%hu", exporter->port);

    struct addrinfo *res;
    int status = getaddrinfo(exporter->host, service,
                             &(struct addrinfo){ .ai_flags = AI_PASSIVE | AI_ADDRCONFIG,
                                                 .ai_family = addrfamily,
                                                 .ai_socktype = SOCK_STREAM, }, &res);
    if (status != 0) {
        return -1;
    }

    int fd = -1;
    for (struct addrinfo *ai = res; ai != NULL; ai = ai->ai_next) {
        int flags = ai->ai_socktype;
#ifdef SOCK_CLOEXEC
        flags |= SOCK_CLOEXEC;
#endif

        fd = socket(ai->ai_family, flags, 0);
        if (fd == -1)
            continue;

        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) != 0) {
            PLUGIN_WARNING("setsockopt(SO_REUSEADDR) failed: %s", STRERRNO);
            close(fd);
            fd = -1;
            continue;
        }

        if (bind(fd, ai->ai_addr, ai->ai_addrlen) != 0) {
            close(fd);
            fd = -1;
            continue;
        }

        if (listen(fd, /* backlog = */ 16) != 0) {
            close(fd);
            fd = -1;
            continue;
        }

        char str_node[NI_MAXHOST];
        char str_service[NI_MAXSERV];

        getnameinfo(ai->ai_addr, ai->ai_addrlen, str_node, sizeof(str_node),
                    str_service, sizeof(str_service), NI_NUMERICHOST | NI_NUMERICSERV);

        PLUGIN_INFO("Listening on [%s]:%s.", str_node, str_service);
        break;
    }

    freeaddrinfo(res);

    return fd;
}
#endif

static struct MHD_Daemon *exporter_start_daemon(exporter_t *exporter)
{
    unsigned int flags = MHD_USE_DEBUG;
#if MHD_VERSION >= 0x00095300
    flags |= MHD_USE_INTERNAL_POLLING_THREAD;
#endif

    size_t nops = 0;
    struct MHD_OptionItem ops[8];
    ops[nops++] = (struct MHD_OptionItem){ MHD_OPTION_EXTERNAL_LOGGER,
                                           (intptr_t)exporter_logger, NULL };

#if MHD_VERSION >= 0x00090000
    int fd = exporter_open_socket(exporter, PF_INET6);
    if (fd == -1)
        fd = exporter_open_socket(exporter, PF_INET);

    if (fd == -1) {
        PLUGIN_ERROR("Opening a listening socket for [%s]:%hu failed.",
                     (exporter->host != NULL) ? exporter->host : "::", exporter->port);
        return NULL;
    }
    ops[nops++] = (struct MHD_OptionItem){ MHD_OPTION_LISTEN_SOCKET, fd, NULL };
#endif

    if (exporter->private_key != NULL) {
        flags |= MHD_USE_SSL;
        if (exporter->private_key != NULL) {
            ops[nops++] = (struct MHD_OptionItem){ MHD_OPTION_HTTPS_MEM_KEY, 0,
                                                   exporter->private_key };
            if (exporter->private_key_pass != NULL) {
#if defined(MHD_VERSION) && MHD_VERSION >= 0x00094000
                ops[nops++] = (struct MHD_OptionItem){ MHD_OPTION_HTTPS_KEY_PASSWORD, 0,
                                                       exporter->private_key_pass };
#else
                PLUGIN_WARNING("This version of libmicrohttpd "
                               "doesn't support MHD_OPTION_HTTPS_KEY_PASSWORD");
#endif
            }
        }
        if (exporter->certificate != NULL)
            ops[nops++] = (struct MHD_OptionItem){ MHD_OPTION_HTTPS_MEM_CERT, 0,
                                                   exporter->certificate };
        if (exporter->tls_priority != NULL)
            ops[nops++] = (struct MHD_OptionItem){ MHD_OPTION_HTTPS_PRIORITIES, 0,
                                                   exporter->tls_priority };
    }

    ops[nops++] = (struct MHD_OptionItem){ MHD_OPTION_END, 0, NULL };

    struct MHD_Daemon *d = MHD_start_daemon(flags, exporter->port, NULL, NULL,
                                            http_handler, exporter,
                                            MHD_OPTION_ARRAY, ops,
                                            MHD_OPTION_END);
    if (d == NULL) {
        PLUGIN_ERROR("MHD_start_daemon() failed.");
        close(fd);
        return NULL;
    }

    return d;
}

static int exporter_write(metric_family_t const *fam, user_data_t *ud)
{
    exporter_t *exporter = ud->data;

    pthread_mutex_lock(&exporter->metrics_lock);

    metric_family_t *exporter_fam = NULL;
    if (c_avl_get(exporter->metrics, fam->name, (void *)&exporter_fam) != 0) {
        exporter_fam = metric_family_clone(fam);
        if (exporter_fam == NULL) {
            PLUGIN_ERROR("Clone metric '%s' failed.", fam->name);
            pthread_mutex_unlock(&exporter->metrics_lock);
            return -1;
        }
        /* Sort the metrics so that lookup is fast. */
        qsort(exporter_fam->metric.ptr, exporter_fam->metric.num,
              sizeof(*exporter_fam->metric.ptr), exporter_metric_cmp);

        int status = c_avl_insert(exporter->metrics, exporter_fam->name, exporter_fam);
        if (status != 0) {
            PLUGIN_ERROR("Adding '%s' failed.", exporter_fam->name);
            metric_family_free(exporter_fam);
            pthread_mutex_unlock(&exporter->metrics_lock);
            return -1;
        }

        pthread_mutex_unlock(&exporter->metrics_lock);
        return 0;
    }

    for (size_t i = 0; i < fam->metric.num; i++) {
        metric_t const *m = &fam->metric.ptr[i];

        metric_t *mmatch = bsearch(m, exporter_fam->metric.ptr, exporter_fam->metric.num,
                                      sizeof(*exporter_fam->metric.ptr), exporter_metric_cmp);
        if (mmatch == NULL) {
            metric_family_metric_append(exporter_fam, *m);
            /* Sort the metrics so that lookup is fast. */
            qsort(exporter_fam->metric.ptr, exporter_fam->metric.num,
                        sizeof(*exporter_fam->metric.ptr), exporter_metric_cmp);
            continue;
        }

        metric_value_clone(&mmatch->value, m->value, exporter_fam->type);

        /* Prometheus has a globally configured timeout after which metrics are
         * considered stale. This causes problems when metrics have an interval
         * exceeding that limit. We emulate the behavior of "pushgateway" and *not*
         * send a timestamp value – Prometheus will fill in the current time. */
        if (m->interval > exporter->staleness_delta) {
            static c_complain_t long_metric = C_COMPLAIN_INIT_STATIC;
            c_complain(LOG_NOTICE, &long_metric,
                                 "You have metrics with an interval exceeding "
                                 "'staleness-delta' setting (%.3fs). This is "
                                 "suboptimal, please check the ncollectd.conf(5) manual page to "
                                 "understand what's going on.",
                                 CDTIME_T_TO_DOUBLE(exporter->staleness_delta));

            mmatch->time = 0;
        } else {
            mmatch->time = m->time;
        }
    }

    pthread_mutex_unlock(&exporter->metrics_lock);
    return 0;
}

static void exporter_free(void *arg)
{
    if (arg == NULL)
        return;
    exporter_t *exporter = arg;

    if (exporter->httpd != NULL) {
        MHD_stop_daemon(exporter->httpd);
        exporter->httpd = NULL;
    }

    pthread_mutex_lock(&exporter->metrics_lock);
    if (exporter->metrics != NULL) {
        char *name;
        metric_family_t *exporter_fam;
        while (c_avl_pick(exporter->metrics, (void *)&name, (void *)&exporter_fam) == 0) {
            assert(name == exporter_fam->name);
            name = NULL;
            metric_family_free(exporter_fam);
        }
        c_avl_destroy(exporter->metrics);
        exporter->metrics = NULL;
    }
    pthread_mutex_unlock(&exporter->metrics_lock);
    pthread_mutex_destroy(&exporter->metrics_lock);

    free(exporter->name);
    free(exporter->host);
    free(exporter->private_key);
    free(exporter->private_key_pass);
    free(exporter->certificate);
    free(exporter->tls_priority);
    free(exporter->realm);
    free(exporter->user);
    free(exporter->password);
    free(exporter);
}

static int config_exporter_read_file(const config_item_t *ci, char **ret_string)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option requires exactly one string argument.", ci->key);
        return -1;
    }

    int fd = open (ci->values[0].value.string, O_RDONLY);
    if (fd < 0) {
        PLUGIN_ERROR("open %s failed: %s", ci->values[0].value.string, STRERRNO);
        return -1;
    }

    struct stat statbuf;
    int status = fstat(fd, &statbuf);
    if (status < 0) {
        PLUGIN_ERROR("fstat %s failed: %s", ci->values[0].value.string, STRERRNO);
        close(fd);
        return -1;
    }

    char *buf = malloc(statbuf.st_size + 1);
    if (buf == NULL) {
        PLUGIN_ERROR("malloc failed");
        close(fd);
        return -1;
    }

    ssize_t size = read(fd, buf, statbuf.st_size);
    if ((size <= 0) || (size != statbuf.st_size)) {
        PLUGIN_ERROR("read %s failed: %s", ci->values[0].value.string, STRERRNO);
        free(buf);
        close(fd);
        return -1;
    }
    close(fd);

    buf[statbuf.st_size] = '\0';

    if (*ret_string != NULL)
        free(*ret_string);
   *ret_string = buf;

    return 0;
}

static int exporter_config_instance(config_item_t *ci)
{
    char *name = NULL;
    int status = cf_util_get_string(ci, &name);
    if (status != 0) {
        return status;
    }
    assert(name != NULL);

    exporter_t *exporter = calloc(1, sizeof(*exporter));
    if (exporter == NULL) {
        PLUGIN_ERROR("calloc failed");
        return -1;
    }

    exporter->name = name;
    exporter->port = 9103;
    exporter->format = FORMAT_STREAM_METRIC_OPENMETRICS_TEXT;
    exporter->authmethod = AUTH_BASIC;
    exporter->staleness_delta = EXPORTER_DEFAULT_STALENESS_DELTA;

    exporter->realm = strdup(PACKAGE_NAME);
    if (exporter->realm == NULL) {
        exporter_free(exporter);
        PLUGIN_ERROR("strdup failed");
        return -1;
    }

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
#if MHD_VERSION >= 0x00090000
            status = cf_util_get_string(child, &exporter->host);
#else
            PLUGIN_ERROR("Option 'host' not supported. "
                         "Please upgrade libmicrohttpd to at least 0.9.0");
            exporter_free(exporter);
            return -1;
#endif
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_port_number(child, &exporter->port);
        } else if (strcasecmp("staleness-delta", child->key) == 0) {
            status = cf_util_get_cdtime(child, &exporter->staleness_delta);
        } else if (strcasecmp("private-key", child->key) == 0) {
            status = config_exporter_read_file(child, &exporter->private_key);
        } else if (strcasecmp("private-key-password", child->key) == 0) {
            status = cf_util_get_string(child, &exporter->private_key_pass);
        } else if (strcasecmp("certificate", child->key) == 0) {
            status = config_exporter_read_file(child, &exporter->certificate);
        } else if (strcasecmp("tls-priority", child->key) == 0) {
            status = cf_util_get_string(child, &exporter->tls_priority);
        } else if (strcasecmp("auth-method", child->key) == 0) {
            char *auth_method = NULL;
            status = cf_util_get_string(child, &auth_method);
            if (status == 0) {
                if (strcasecmp("basic", auth_method) == 0) {
                    free(auth_method);
                    exporter->authmethod = AUTH_BASIC;
                } else if (strcasecmp("digest", auth_method) == 0) {
                    free(auth_method);
                    exporter->authmethod = AUTH_DIGEST;
                } else {
                    free(auth_method);
                    exporter_free(exporter);
                    PLUGIN_ERROR("Invalid AuthMethod, must be 'basic' or 'digest'.");
                    return -1;
                }
            }
        } else if (strcasecmp("user", child->key) == 0) {
            status = cf_util_get_string(child, &exporter->user);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &exporter->password);
        } else if (strcasecmp("realm", child->key) == 0) {
            status = cf_util_get_string(child, &exporter->realm);
        } else if (strcasecmp("format", child->key) == 0) {
            status = config_format_stream_metric(child, &exporter->format);
        } else {
            PLUGIN_WARNING("Ignoring unknown configuration option '%s'.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        exporter_free(exporter);
        return -1;
    }

    if ((exporter->private_key == NULL) && (exporter->certificate !=NULL)) {
        exporter_free(exporter);
        PLUGIN_ERROR("Missing 'private-key' option");
        return -1;
    }

    if ((exporter->private_key != NULL) && (exporter->certificate ==NULL)) {
        exporter_free(exporter);
        PLUGIN_ERROR("Missing 'certificate' option");
        return -1;
    }

    if ((exporter->user != NULL) && (exporter->password == NULL)) {
        exporter_free(exporter);
        PLUGIN_ERROR("Missing 'password' option");
        return -1;
    }

    if ((exporter->user == NULL) && (exporter->password != NULL)) {
        exporter_free(exporter);
        PLUGIN_ERROR("Missing 'user' option");
        return -1;
    }


    exporter->metrics = c_avl_create((int (*)(const void *, const void *))strcmp);
    if (exporter->metrics == NULL) {
        exporter_free(exporter);
        PLUGIN_ERROR("c_avl_create() failed.");
        return -1;
    }

    pthread_mutex_init(&exporter->metrics_lock, NULL);

    exporter->httpd = exporter_start_daemon(exporter);
    if (exporter->httpd == NULL) {
        exporter_free(exporter);
        return -1;
    }

    PLUGIN_DEBUG("Successfully started microhttpd %s", MHD_get_version());

    plugin_register_write("write_exporter",  exporter->name, exporter_write, NULL, 0, 0,
                          &(user_data_t){ .data = exporter, .free_func = exporter_free });
    return 0;
}

static int exporter_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = exporter_config_instance(child);
        } else {
            PLUGIN_WARNING("The configuration option '%s' is not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("write_exporter", exporter_config);
}
