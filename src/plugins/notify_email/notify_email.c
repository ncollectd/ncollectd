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

static smtp_session_t session;
static pthread_mutex_t session_lock = PTHREAD_MUTEX_INITIALIZER;
static smtp_message_t message;
static auth_context_t authctx;

static int smtp_port = 25;
static char *smtp_host;
static char *smtp_user;
static char *smtp_password;
static char *email_from;
static char *email_subject;

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
    if (status->text[strlen(status->text) - 2] == '\r')
        status->text[strlen(status->text) - 2] = 0;
    PLUGIN_INFO("notify sent to %s: %d %s", mailbox, status->code, status->text);
}

/* Callback to monitor SMTP activity */
static void monitor_cb(const char *buf, int buflen, int writing,
                       void __attribute__((unused)) * arg)
{
    char log_str[MAXSTRING];

    sstrncpy(log_str, buf, sizeof(log_str));
    if (buflen > 2)
        log_str[buflen - 2] = 0; /* replace \n with \0 */

    if (writing == SMTP_CB_HEADERS) {
        PLUGIN_DEBUG("SMTP --- H: %s", log_str);
        return;
    }
    PLUGIN_DEBUG(writing ? "SMTP >>> C: %s" : "SMTP <<< S: %s", log_str);
}

static int notify_email_init(void)
{
    char server[MAXSTRING];

    ssnprintf(server, sizeof(server), "%s:%i",
                      (smtp_host == NULL) ? DEFAULT_SMTP_HOST : smtp_host, smtp_port);

    pthread_mutex_lock(&session_lock);

    auth_client_init();

    session = smtp_create_session();
    if (session == NULL) {
        pthread_mutex_unlock(&session_lock);
        PLUGIN_ERROR("cannot create SMTP session");
        return -1;
    }

    smtp_set_monitorcb(session, monitor_cb, NULL, 1);
    smtp_set_hostname(session, plugin_get_hostname());
    smtp_set_server(session, server);

    if (smtp_user && smtp_password) {
        authctx = auth_create_context();
        auth_set_mechanism_flags(authctx, AUTH_PLUGIN_PLAIN, 0);
        auth_set_interact_cb(authctx, authinteract, NULL);
    }

    if (!smtp_auth_set_context(session, authctx)) {
        pthread_mutex_unlock(&session_lock);
        PLUGIN_ERROR("cannot set SMTP auth context");
        return -1;
    }

    pthread_mutex_unlock(&session_lock);
    return 0;
}

static int notify_email_shutdown(void)
{
    pthread_mutex_lock(&session_lock);

    if (session != NULL)
        smtp_destroy_session(session);
    session = NULL;

    if (authctx != NULL)
        auth_destroy_context(authctx);
    authctx = NULL;

    auth_client_exit();

    pthread_mutex_unlock(&session_lock);
    return 0;
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
                                     "Content-Type: text/plain; charset=\"US-ASCII\"\r\n"
                                     "Content-Transfer-Encoding: 8bit\r\n"
                                     "Subject: ");
    status |= strbuf_putstr(&buf, email_subject == NULL ? DEFAULT_SMTP_SUBJECT : email_subject);
    status |= strbuf_putstr(&buf, n->name);
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

    pthread_mutex_lock(&session_lock);

    if (session == NULL) {
        /* Initialization failed or we're in the process of shutting down. */
        strbuf_destroy(&buf);
        pthread_mutex_unlock(&session_lock);
        return -1;
    }

    if (!(message = smtp_add_message(session))) {
        strbuf_destroy(&buf);
        pthread_mutex_unlock(&session_lock);
        PLUGIN_ERROR("cannot set SMTP message");
        return -1;
    }
    smtp_set_reverse_path(message, email_from);
    smtp_set_header(message, "To", NULL, NULL);
    smtp_set_message_str(message, buf.ptr);

    for (int i = 0; i < recipients_len; i++)
        smtp_add_recipient(message, recipients[i]);

    /* Initiate a connection to the SMTP server and transfer the message. */
    if (!smtp_start_session(session)) {
        char berror[1024];
        PLUGIN_ERROR("SMTP server problem: %s",
                     smtp_strerror(smtp_errno(), berror, sizeof(berror)));
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
                    return -1;
                }
                recipients = tmp;
                recipients[recipients_len] = recipient;
                recipients_len++;
            }
        } else if (strcasecmp(child->key, "smtp-server") == 0) {
            status = cf_util_get_string(child, &smtp_host);
        } else if (strcasecmp(child->key, "smtp-port") == 0) {
            status = cf_util_get_int(child, &smtp_port);
        } else if (strcasecmp(child->key, "smtp-user") == 0) {
            status = cf_util_get_string(child, &smtp_user);
        } else if (strcasecmp(child->key, "smtp-password") == 0) {
            status = cf_util_get_string(child, &smtp_password);
        } else if (strcasecmp(child->key, "from") == 0) {
            status = cf_util_get_string(child, &email_from);
        } else if (strcasecmp(child->key, "subject") == 0) {
            status = cf_util_get_string(child, &email_subject);
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
    plugin_register_notification(NULL, "notify_email", notify_email_notification, NULL);
}
