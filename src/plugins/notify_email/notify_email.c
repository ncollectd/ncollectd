// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2008 Oleg King
// SPDX-FileCopyrightText: Copyright (C) 2010 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Oleg King <king2 at kaluga.ru>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <auth-client.h>
#include <libesmtp.h>

#define MAXSTRING 256

static char **recipients;
static int recipients_len;

static bool initialized;
static pthread_mutex_t session_lock = PTHREAD_MUTEX_INITIALIZER;
static auth_context_t authctx;

static int smtp_port = 25;
static char *smtp_host;
static char *smtp_user;
static char *smtp_password;
static char *email_from;
static char *email_subject;
static enum starttls_option smtp_starttls = Starttls_DISABLED;
static cdtime_t smtp_timeout;

#define DEFAULT_SMTP_HOST "localhost"
#define DEFAULT_SMTP_FROM "root@localhost"
#define DEFAULT_SMTP_SUBJECT "NCollectd notify: "

/* Callback to get username and password */
static int authinteract(auth_client_request_t request, char **result,
                        int fields, void __attribute__((unused)) * arg)
{
    for (int i = 0; i < fields; i++) {
        if (request[i].flags & AUTH_USER)
            result[i] = smtp_user;
        else if (request[i].flags & AUTH_PASS)
            result[i] = smtp_password;
        else
            return 0;
    }
    return 1;
}

/* Callback to print the recipient status */
static void print_recipient_status(smtp_recipient_t recipient, const char *mailbox,
                                   void __attribute__((unused)) * arg)
{
    const smtp_status_t *status = smtp_recipient_status(recipient);
    if (status == NULL) {
        PLUGIN_INFO("notify sent to %s: unknown status", mailbox);
        return;
    }

    if (status->text == NULL) {
        PLUGIN_INFO("notify sent to %s: %d", mailbox, status->code);
        return;
    }

    /* copy the text to strip the trailing \r\n without modifying libesmtp data */
    char text[MAXSTRING];
    sstrncpy(text, status->text, sizeof(text));
    strstripnewline(text);
    PLUGIN_INFO("notify sent to %s: %d %s", mailbox, status->code, text);
}

#ifdef NCOLLECTD_DEBUG
/* Callback to monitor SMTP activity */
static void monitor_cb(const char *buf, int buflen, int writing,
                       void __attribute__((unused)) * arg)
{
    char log_str[MAXSTRING];

    /* buf is not NUL-terminated and buflen can be larger than log_str */
    size_t len = 0;
    if (buflen > 0)
        len = MIN((size_t)buflen, sizeof(log_str) - 1);
    memcpy(log_str, buf, len);
    log_str[len] = '\0';
    strstripnewline(log_str);

    if (writing == SMTP_CB_HEADERS) {
        PLUGIN_DEBUG("SMTP --- H: %s", log_str);
        return;
    }
    PLUGIN_DEBUG(writing ? "SMTP >>> C: %s" : "SMTP <<< S: %s", log_str);
}
#endif

static int notify_email_init(void)
{
    pthread_mutex_lock(&session_lock);

    auth_client_init();

    if (smtp_user && smtp_password) {
        authctx = auth_create_context();
        if (authctx == NULL) {
            pthread_mutex_unlock(&session_lock);
            PLUGIN_ERROR("cannot create SMTP auth context");
            return -1;
        }
        auth_set_mechanism_flags(authctx, AUTH_PLUGIN_PLAIN, 0);
        auth_set_interact_cb(authctx, authinteract, NULL);
    }

    initialized = true;

    pthread_mutex_unlock(&session_lock);
    return 0;
}

static smtp_session_t notify_email_session_create(void)
{
    char server[MAXSTRING];

    ssnprintf(server, sizeof(server), "%s:%i",
                      (smtp_host == NULL) ? DEFAULT_SMTP_HOST : smtp_host, smtp_port);

    smtp_session_t session = smtp_create_session();
    if (session == NULL) {
        PLUGIN_ERROR("cannot create SMTP session");
        return NULL;
    }

#ifdef NCOLLECTD_DEBUG
    smtp_set_monitorcb(session, monitor_cb, NULL, 1);
#endif
    smtp_set_hostname(session, plugin_get_hostname());
    smtp_set_server(session, server);

    if (!smtp_auth_set_context(session, authctx)) {
        PLUGIN_ERROR("cannot set SMTP auth context");
        smtp_destroy_session(session);
        return NULL;
    }

    return session;
}

static int notify_email_shutdown(void)
{
    pthread_mutex_lock(&session_lock);

    initialized = false;

    if (authctx != NULL)
        auth_destroy_context(authctx);
    authctx = NULL;

    auth_client_exit();

    pthread_mutex_unlock(&session_lock);

    for (int i = 0; i < recipients_len; i++)
        free(recipients[i]);
    free(recipients);
    recipients = NULL;
    recipients_len = 0;

    free(smtp_host);
    smtp_host = NULL;
    free(smtp_user);
    smtp_user = NULL;
    free(smtp_password);
    smtp_password = NULL;
    free(email_from);
    email_from = NULL;
    free(email_subject);
    email_subject = NULL;

    return 0;
}

/* Append a string to a header replacing CR and LF to avoid header injection. */
static int notify_email_putheader(strbuf_t *buf, const char *str)
{
    static char rset[256] = {
        ['\r'] = ' ',
        ['\n'] = ' ',
    };

    return strbuf_putnreplace_set(buf, str, strlen(str), rset, ' ');
}

static int notify_email_notification(const notification_t *n,
                                     user_data_t __attribute__((unused)) * user_data)
{
    char *severity = NULL;
    switch (n->severity) {
    case NOTIF_FAILURE:
        severity = "FAILURE";
        break;
    case NOTIF_WARNING:
        severity = "WARNING";
        break;
    case NOTIF_OKAY:
        severity = "OKAY";
        break;
    default:
        severity = "UNKNOWN";
        break;
    }

    struct tm timestamp_tm;
    char timestamp_str[64];
    localtime_r(&CDTIME_T_TO_TIME_T(n->time), &timestamp_tm);
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S", &timestamp_tm);
    timestamp_str[sizeof(timestamp_str) - 1] = '\0';

    strbuf_t buf = STRBUF_CREATE;

    /* Let's make RFC822 message text with \r\n EOLs */
    int status = strbuf_putstr(&buf, "MIME-Version: 1.0\r\n"
                                     "Content-Type: text/plain; charset=\"UTF-8\"\r\n"
                                     "Content-Transfer-Encoding: 8bit\r\n"
                                     "Subject: ");
    status |= notify_email_putheader(&buf, email_subject == NULL ? DEFAULT_SMTP_SUBJECT
                                                                 : email_subject);
    status |= notify_email_putheader(&buf, n->name);
    status |= strbuf_putstr(&buf, " - ");
    status |= strbuf_putstr(&buf, severity);
    status |= strbuf_putstr(&buf, "\r\n");
    status |= strbuf_putstr(&buf, "\r\n\r\n");

    status |= strbuf_putstr(&buf, n->name);
    status |= strbuf_putstr(&buf, " - ");
    status |= strbuf_putstr(&buf, severity);
    status |= strbuf_putstr(&buf, " at ");
    status |= strbuf_putstr(&buf, timestamp_str);
    status |= strbuf_putstr(&buf, "\r\n");

    status |= strbuf_putstr(&buf, "Labels:\r\n");
    for (size_t i=0; i < n->label.num ; i++) {
        status |= strbuf_putstr(&buf,  n->label.ptr[i].name);
        status |= strbuf_putstr(&buf, " = ");
        status |= strbuf_putstr(&buf,  n->label.ptr[i].value);
        status |= strbuf_putstr(&buf, "\r\n");
    }
    status |= strbuf_putstr(&buf, "\r\nAnnotations:\r\n");
    for (size_t i=0; i < n->annotation.num ; i++) {
        status |= strbuf_putstr(&buf,  n->annotation.ptr[i].name);
        status |= strbuf_putstr(&buf, " = ");
        status |= strbuf_putstr(&buf,  n->annotation.ptr[i].value);
        status |= strbuf_putstr(&buf, "\r\n");
    }

    if (status != 0) {
        strbuf_destroy(&buf);
        PLUGIN_ERROR("failed to build message");
        return -1;
    }

    pthread_mutex_lock(&session_lock);

    if (!initialized) {
        /* Initialization failed or we're in the process of shutting down. */
        strbuf_destroy(&buf);
        pthread_mutex_unlock(&session_lock);
        return -1;
    }

    smtp_session_t session = notify_email_session_create();
    if (session == NULL) {
        strbuf_destroy(&buf);
        pthread_mutex_unlock(&session_lock);
        return -1;
    }

    smtp_message_t message = smtp_add_message(session);
    if (message == NULL) {
        smtp_destroy_session(session);
        strbuf_destroy(&buf);
        pthread_mutex_unlock(&session_lock);
        PLUGIN_ERROR("cannot set SMTP message");
        return -1;
    }
    smtp_set_reverse_path(message, email_from == NULL ? DEFAULT_SMTP_FROM : email_from);
    smtp_set_header(message, "To", NULL, NULL);
    smtp_set_message_str(message, buf.ptr);

    for (int i = 0; i < recipients_len; i++)
        smtp_add_recipient(message, recipients[i]);

    /* Initiate a connection to the SMTP server and transfer the message. */
    if (!smtp_start_session(session)) {
        char berror[1024];
        PLUGIN_ERROR("SMTP server problem: %s",
                     smtp_strerror(smtp_errno(), berror, sizeof(berror)));
        smtp_destroy_session(session);
        strbuf_destroy(&buf);
        pthread_mutex_unlock(&session_lock);
        return -1;
    } else {
#ifdef NCOLLECTD_DEBUG
        const smtp_status_t *smtp_status;
        /* Report on the success or otherwise of the mail transfer. */
        smtp_status = smtp_message_transfer_status(message);
        PLUGIN_DEBUG("SMTP server report: %d %s", smtp_status->code,
                    (smtp_status->text != NULL) ? smtp_status->text : "\n");
#endif
        smtp_enumerate_recipients(message, print_recipient_status, NULL);
    }

    smtp_destroy_session(session);
    strbuf_destroy(&buf);
    pthread_mutex_unlock(&session_lock);
    return 0;
}

static int notify_email_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "recipient") == 0) {
            char *recipient = NULL;
            status = cf_util_get_string(child, &recipient);
            if (status == 0) {
                char **tmp = realloc(recipients, (recipients_len + 1) * sizeof(char *));
                if (tmp == NULL) {
                    PLUGIN_ERROR("realloc failed.");
                    free(recipient);
                    return -1;
                }
                recipients = tmp;
                recipients[recipients_len] = recipient;
                recipients_len++;
            }
        } else if (strcasecmp(child->key, "smtp-server") == 0) {
            status = cf_util_get_string(child, &smtp_host);
        } else if (strcasecmp(child->key, "smtp-port") == 0) {
            status = cf_util_get_port_number(child, &smtp_port);
        } else if (strcasecmp(child->key, "smtp-user") == 0) {
            status = cf_util_get_string(child, &smtp_user);
        } else if (strcasecmp(child->key, "smtp-password") == 0) {
            status = cf_util_get_string(child, &smtp_password);
        } else if (strcasecmp(child->key, "from") == 0) {
            status = cf_util_get_string(child, &email_from);
        } else if (strcasecmp(child->key, "subject") == 0) {
            status = cf_util_get_string(child, &email_subject);
        } else if (strcasecmp(child->key, "starttls") == 0) {
            char *starttls = NULL;
            status = cf_util_get_string(child, &starttls);
            if (status == 0) {
                if (strcasecmp(starttls, "disable") == 0) {
                    smtp_starttls = Starttls_DISABLED;
                } else if (strcasecmp(starttls, "enable") == 0) {
                    smtp_starttls = Starttls_ENABLED;
                } else if (strcasecmp(starttls, "require") == 0) {
                    smtp_starttls = Starttls_REQUIRED;
                } else {
                    PLUGIN_ERROR("Invalid value '%s' for option '%s' in %s:%d, "
                                 "must be 'disable', 'enable' or 'require'.", starttls,
                                 child->key, cf_get_file(child), cf_get_lineno(child));
                    status = -1;
                }
                free(starttls);
            }
        } else if (strcasecmp(child->key, "timeout") == 0) {
            status = cf_util_get_cdtime(child, &smtp_timeout);
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

void module_register(void)
{
    plugin_register_init("notify_email", notify_email_init);
    plugin_register_shutdown("notify_email", notify_email_shutdown);
    plugin_register_config("notify_email", notify_email_config);
    plugin_register_notification("notify_email", NULL, notify_email_notification, NULL);
}
