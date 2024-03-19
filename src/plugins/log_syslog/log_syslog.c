// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2007 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/socket.h"

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

/* AIX doesn't have MSG_DONTWAIT */
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT MSG_NONBLOCK
#endif

typedef enum {
    SYSLOG_LOCAL,
    SYSLOG_RFC3164,
    SYSLOG_RFC5424
} syslog_kind_t;

static const char *rfc3164_mon[] = {
    "Jan", "Feb", "Mar", "Apr",
    "May", "Jun", "Jul", "Aug",
    "Sep", "Oct", "Nov", "Dec"
};

typedef struct {
    const char *name;
    int value;
} syslog_facility_t;

static syslog_facility_t syslog_facility[] = {
    { "kern",     0  },
    { "user",     1  },
    { "mail",     2  },
    { "daemon",   3  },
    { "auth",     4  },
    { "syslog",   5  },
    { "lpr",      6  },
    { "news",     7  },
    { "uucp",     8  },
    { "cron",     9  },
    { "authpriv", 10 },
    { "ftp",      11 },
    { "ntp",      12 },
    { "security", 13 },
    { "console",  14 },
    { "local0",   16 },
    { "local1",   17 },
    { "local2",   18 },
    { "local3",   19 },
    { "local4",   20 },
    { "local5",   21 },
    { "local6",   22 },
    { "local7",   23 },
};

static size_t syslog_facility_size = STATIC_ARRAY_SIZE(syslog_facility);

typedef struct {
    char *instance;
    syslog_kind_t kind;
    char *host;
    int port;
    int fd;
    int facility;
    int log_level;
    severity_t notif_severity;
    int rc;
} syslog_ctx_t;

static int sl_fmt_msg(strbuf_t *buf, const log_msg_t *msg)
{
    int status = 0;

    if (msg->plugin != NULL) {
        status |= strbuf_putstrn(buf, "plugin ", strlen("plugin "));
        status |= strbuf_putstr(buf, msg->plugin);
        status |= strbuf_putchar(buf, ' ');
    }

    if (msg->func != NULL) {
        status |= strbuf_putstr(buf, msg->func);
        if (msg->file != NULL)
            status |= strbuf_putchar(buf, '(');
    }

    if (msg->file != NULL) {
        status |= strbuf_putstr(buf, msg->file);
        if (msg->line != 0) {
            status |= strbuf_putchar(buf, ':');
            status |= strbuf_putint(buf, msg->line);
        }
        if (msg->func != NULL)
            status |= strbuf_putchar(buf, ')');
    }

    if ((msg->plugin != NULL) || (msg->func != NULL) || (msg->file != NULL))
        status |= strbuf_putstrn(buf, ": ", strlen(": "));

    status |= strbuf_putstr(buf, msg->msg);
    return status;
}

static int sl_rfc5424(int fd, cdtime_t time, int facility, int severity, const char *msg)
{
    char message[1024] = "";
    strbuf_t buf = STRBUF_CREATE_STATIC(message);

    struct tm tm;
    struct timespec t_spec = CDTIME_T_TO_TIMESPEC(time);
    NORMALIZE_TIMESPEC(t_spec);
    if (gmtime_r(&t_spec.tv_sec, &tm) == NULL) {
        PLUGIN_ERROR("gmtime_r failed: %s", STRERRNO);
        return -1;
    }

    uint8_t prival =  (facility << 3) + severity;
    int status = strbuf_printf(&buf, "<%i>%i %d-%02d-%02dT%02d:%02d:%02d.%06"PRIu64"Z ",
                               prival, 1, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                               tm.tm_hour, tm.tm_min, tm.tm_sec, (uint64_t) t_spec.tv_nsec/1000);

    const char *hostname = plugin_get_hostname();
    if (hostname != NULL) {
        size_t len = strlen(hostname);
        status |= strbuf_putstrn(&buf, hostname, len > 255 ? 255 : len);
        status |= strbuf_putchar(&buf, ' ');
    } else {
        status |= strbuf_putstr(&buf, "- ");
    }

    size_t len = strlen(PACKAGE_NAME " ");
    status |= strbuf_putstrn(&buf, PACKAGE_NAME " ", len > 49 ? 49 : len);

    pid_t pid = getpid();
    status |= strbuf_putuint(&buf, pid);
    status |= strbuf_putstr(&buf, " - - \xef\xbb\xbf");

    status |= strbuf_putstr(&buf, msg);

    if (status != 0)
        PLUGIN_WARNING("Failed to format message.");

    ssize_t ret = send(fd, buf.ptr, strbuf_len(&buf), MSG_DONTWAIT);
    if (ret < 0)
        PLUGIN_WARNING("Failed to write message: %s.", STRERRNO);

    return 0;
}

static int sl_rfc3164(int fd, cdtime_t time, int facility, int severity, const char *msg)
{
    char message[2048] = "";
    strbuf_t buf = STRBUF_CREATE_STATIC(message);

    struct tm tm;
    struct timespec t_spec = CDTIME_T_TO_TIMESPEC(time);
    NORMALIZE_TIMESPEC(t_spec);
    if (gmtime_r(&t_spec.tv_sec, &tm) == NULL) {
        PLUGIN_ERROR("gmtime_r failed: %s", STRERRNO);
        return -1;
    }

    uint8_t prival =  (facility << 3) + severity;

    int status = strbuf_printf(&buf, "<%i>%s %2d %02d:%02d:%02d ",
                               prival, rfc3164_mon[tm.tm_mon],
                               tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    const char *hostname = plugin_get_hostname();
    if (hostname != NULL) {
        status |= strbuf_putstr(&buf, hostname);
        status |= strbuf_putchar(&buf, ' ');
    }

    status |= strbuf_putstr(&buf, PACKAGE_NAME "[");
    pid_t pid = getpid();
    status |= strbuf_putuint(&buf, pid);
    status |= strbuf_putstr(&buf, "]: ");

    status |= strbuf_putstr(&buf, msg);
    if (status != 0)
        PLUGIN_WARNING("Failed to format message.");

    ssize_t ret = send(fd, buf.ptr, strbuf_len(&buf), MSG_DONTWAIT);
    if (ret < 0)
        PLUGIN_WARNING("Failed to write message: %s.", STRERRNO);

    return 0;
}

static void sl_log(const log_msg_t *msg, user_data_t *ud)
{
    syslog_ctx_t *ctx = ud->data;

    if (msg == NULL)
        return;

    if (msg->severity > ctx->log_level)
        return;

    char message[1024] = "";
    strbuf_t buf = STRBUF_CREATE_STATIC(message);
    sl_fmt_msg(&buf, msg);

    switch(ctx->kind) {
    case SYSLOG_LOCAL:
        syslog(msg->severity, "%s", buf.ptr);
        break;
    case SYSLOG_RFC3164:
        sl_rfc3164(ctx->fd, msg->time, ctx->facility, msg->severity, buf.ptr);
        break;
    case SYSLOG_RFC5424:
        sl_rfc5424(ctx->fd, msg->time, ctx->facility, msg->severity, buf.ptr);
        break;
    }
}

static int sl_notification(const notification_t *n, user_data_t *ud)
{
    syslog_ctx_t *ctx = ud->data;

    if (n == NULL)
        return 0;

    if (n->severity > ctx->notif_severity)
        return 0;

    int log_severity;
    switch (n->severity) {
        case NOTIF_FAILURE:
            log_severity = LOG_ERR;
            break;
        case NOTIF_WARNING:
            log_severity = LOG_WARNING;
            break;
        case NOTIF_OKAY:
            log_severity = LOG_NOTICE;
            break;
        default:
            log_severity = LOG_ERR;
    }

    char message[1024] = "";
    strbuf_t buf = STRBUF_CREATE_STATIC(message);
    strbuf_putstr(&buf, "Notification: ");
    notification_marshal(&buf, n);

    switch(ctx->kind) {
    case SYSLOG_LOCAL:
        syslog(log_severity, "%s", buf.ptr);
        break;
    case SYSLOG_RFC3164:
        sl_rfc3164(ctx->fd, n->time, ctx->facility, log_severity, buf.ptr);
        break;
    case SYSLOG_RFC5424:
        sl_rfc5424(ctx->fd, n->time, ctx->facility, log_severity, buf.ptr);
        break;
    }

    return 0;
}

static void sl_free(void *arg)
{
    syslog_ctx_t *ctx = arg;

    if (ctx == NULL)
        return;

    ctx->rc--;
    if (ctx->rc > 0)
        return;

    if (ctx->fd > 0)
        close(ctx->fd);

    free(ctx->instance);
    free(ctx->host);

    free(ctx);
}

static int sl_config_facility(config_item_t *ci, int *ret_facility)
{
    if ((ci == NULL) || (ret_facility == NULL))
        return EINVAL;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    int facility = -1;
    for (size_t i = 0; i < syslog_facility_size ; i++) {
        if (strcasecmp(ci->values[0].value.string, syslog_facility[i].name) == 0) {
            facility = syslog_facility[i].value;
            break;
        }
    }

    if (facility == -1) {
        PLUGIN_ERROR("The '%s' option in %s:%d must be a valid syslog facility name.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    *ret_facility = facility;

    return 0;
}

static int sl_config_kind(config_item_t *ci, syslog_kind_t *ret_kind)
{
    if ((ci == NULL) || (ret_kind == NULL))
        return EINVAL;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (strcasecmp(ci->values[0].value.string, "local") == 0)  {
        *ret_kind = SYSLOG_LOCAL;
    } else if (strcasecmp(ci->values[0].value.string, "rfc3164") == 0)  {
        *ret_kind = SYSLOG_RFC3164;
    } else if (strcasecmp(ci->values[0].value.string, "rfc5424") == 0)  {
        *ret_kind = SYSLOG_RFC5424;
    } else {
        PLUGIN_ERROR("The '%s' option in %s:%d must be: 'local', 'rfc3164' or 'rfc5424'",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    return 0;
}

static int sl_config_instance(config_item_t *ci)
{
    syslog_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &ctx->instance);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(ctx);
        return status;
    }

    ctx->port = 514;
    ctx->fd = -1;
    ctx->kind = SYSLOG_LOCAL;
    ctx->facility = 3;
#ifdef NCOLLECTD_DEBUG
    ctx->log_level = LOG_DEBUG;
#else
    ctx->log_level = LOG_INFO;
#endif
    ctx->notif_severity = NOTIF_FAILURE;
    int ttl = -1;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "host") == 0) {
            status = cf_util_get_string(child, &ctx->host);
        } else if (strcasecmp(child->key, "port") == 0) {
            status = cf_util_get_port_number(child, &ctx->port);
        } else if (strcasecmp(child->key, "log-level") == 0) {
            status = cf_util_get_log_level(child, &ctx->log_level);
        } else if (strcasecmp(child->key, "notify-level") == 0) {
            status = cf_util_get_severity(child, &ctx->notif_severity);
        } else if (strcasecmp(child->key, "facility") == 0) {
            status = sl_config_facility(child, &ctx->facility);
        } else if (strcasecmp(child->key, "type") == 0) {
            status = sl_config_kind(child, &ctx->kind);
        } else if (strcasecmp(child->key, "ttl") == 0) {
            status = cf_util_get_int(child, &ttl);
        } else {
            PLUGIN_ERROR("Invalid configuration option: %s.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        sl_free(ctx);
        return status;
    }

    if ((ctx->host != NULL) && (ctx->kind == SYSLOG_LOCAL))
        ctx->kind = SYSLOG_RFC3164;

    if (ctx->host != NULL) {
        ctx->fd = socket_connect_udp(ctx->host, ctx->port, ttl);
        if (ctx->fd < 0) {
            sl_free(ctx);
            return -1;
        }
    }

    user_data_t user_data = { .data = ctx, .free_func = sl_free };

    ctx->rc++;
    plugin_register_log("log_syslog", ctx->instance, sl_log, &user_data);
    ctx->rc++;
    plugin_register_notification("log_syslog", ctx->instance, sl_notification, &user_data);

    return 0;
}

static int sl_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "instance") == 0) {
            status = sl_config_instance(child);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int sl_init(void)
{
    openlog("ncollectd", LOG_CONS | LOG_PID, LOG_DAEMON);
    return 0;
}

static int sl_shutdown(void)
{
    closelog();
    return 0;
}

void module_register(void)
{
    plugin_register_config("log_syslog", sl_config);
    plugin_register_init("log_syslog", sl_init);
    plugin_register_shutdown("log_syslog", sl_shutdown);
}
