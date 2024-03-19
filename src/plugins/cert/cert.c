// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/itoa.h"

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/bn.h>
#include <openssl/asn1.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>

enum {
    FAM_CERT_EXPIRE_NOT_AFTER_SECONDS,
    FAM_CERT_EXPIRE_NOT_BEFORE_SECONDS,
    FAM_CERT_MAX,
};

static metric_family_t fams[FAM_CERT_MAX] = {
    [FAM_CERT_EXPIRE_NOT_AFTER_SECONDS] = {
        .name = "cert_expire_not_after_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "The date after which a peer certificate expires.",
    },
    [FAM_CERT_EXPIRE_NOT_BEFORE_SECONDS] = {
        .name = "cert_expire_not_before_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "The date before which a peer certificate is not valid.",
    },
};

typedef struct cert_cb {
    char *instance;
    char *host;
    int port;
    char *server_name;
    label_set_t labels;
    metric_family_t fams[FAM_CERT_MAX];
} cert_cb_t;

static time_t ASN1_timestamp(const ASN1_TIME *s)
{
    struct tm tm = {0};
    ASN1_TIME_to_tm(s, &tm);
    return mktime(&tm);
}

static int create_socket(char *hostname, int port)
{
    struct hostent * host = gethostbyname(hostname);
    if (host == NULL) {
        PLUGIN_ERROR("Cannot resolve hostname %s.",  hostname);
        return -1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        PLUGIN_ERROR("Error open socket: %s.",  STRERRNO);
        return -1;
    }

    struct sockaddr_in dest_addr = {0};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = *(long*)(host->h_addr);

    char *tmp_ptr = inet_ntoa(dest_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *) &dest_addr, sizeof(struct sockaddr)) == -1 ) {
        PLUGIN_ERROR("Cannot connect to host %s [%s] on port %d.", hostname, tmp_ptr, port);
        close(sockfd);
        return -1;
    }

    return sockfd;
}

static void cert_free(void *data)
{
    cert_cb_t *cert_cb = data;

    if (cert_cb == NULL)
        return;

    free(cert_cb->instance);
    free(cert_cb->host);
    free(cert_cb->server_name);

    label_set_reset(&cert_cb->labels);

    free(cert_cb);
}

static int cert_read(user_data_t *user_data)
{
    SSL_CTX *ctx = NULL;
    int fd = -1;
    SSL *ssl = NULL;
    X509 *cert = NULL;
    char *subj = NULL;
    char *issuer = NULL;
    BIGNUM *bn = NULL;
    char *serial = NULL;

    cert_cb_t *cert_cb = user_data->data;
    if (cert_cb == NULL)
        return  -1;

    const SSL_METHOD *method = SSLv23_client_method();
    ctx = SSL_CTX_new(method);
    if (ctx == NULL) {
        PLUGIN_ERROR("Unable to create a new SSL context structure.");
        return -1;
    }

    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);

    ssl = SSL_new(ctx);
    const char *server_name = cert_cb->server_name != NULL ? cert_cb->server_name
                                                           : cert_cb->host;
    SSL_set_tlsext_host_name(ssl, server_name);
    fd = create_socket(cert_cb->host, cert_cb->port);
    if (fd < 0) {
        goto error;
    }

    SSL_set_fd(ssl, fd);
    if (SSL_connect(ssl) != 1) {
        PLUGIN_ERROR("Error: Could not build a SSL session.");
        goto error;
    }

    cert = SSL_get_peer_certificate(ssl);
    if (cert == NULL) {
        PLUGIN_ERROR("Error: Could not get a certificate.");
        goto error;
    }

    subj = X509_NAME_oneline(X509_get_subject_name(cert), NULL, 0);
    issuer = X509_NAME_oneline(X509_get_issuer_name(cert), NULL, 0);

    ASN1_INTEGER *serialnum = X509_get_serialNumber(cert);
    bn = ASN1_INTEGER_to_BN(serialnum, NULL);
    if (bn == NULL) {
        PLUGIN_ERROR("Unable to convert ASN1INTEGER to BN.");
        goto error;
    }

    serial = BN_bn2dec(bn);
    if (serial == NULL) {
        PLUGIN_ERROR("Unable to convert BN to decimal string.");
        goto error;
    }

    char port[ITOA_MAX];
    itoa(cert_cb->port, port);

    ASN1_TIME *not_after = X509_get_notAfter(cert);
    if (not_after != NULL) {
        metric_family_append(&cert_cb->fams[FAM_CERT_EXPIRE_NOT_AFTER_SECONDS],
                             VALUE_GAUGE(ASN1_timestamp(not_after)), &cert_cb->labels,
                             &(label_pair_const_t){.name="host", .value= cert_cb->host },
                             &(label_pair_const_t){.name="port", .value= port },
                             &(label_pair_const_t){.name="servername", .value= server_name },
                             &(label_pair_const_t){.name="subject", .value= subj },
                             &(label_pair_const_t){.name="issuer", .value= issuer },
                             &(label_pair_const_t){.name="serial", .value= serial },
                             NULL);
    }

    ASN1_TIME *not_before = X509_get_notBefore(cert);
    if (not_before != NULL) {
        metric_family_append(&cert_cb->fams[FAM_CERT_EXPIRE_NOT_BEFORE_SECONDS],
                             VALUE_GAUGE(ASN1_timestamp(not_before)), &cert_cb->labels,
                             &(label_pair_const_t){.name="host", .value= cert_cb->host },
                             &(label_pair_const_t){.name="port", .value= port },
                             &(label_pair_const_t){.name="servername", .value= server_name },
                             &(label_pair_const_t){.name="subject", .value= subj },
                             &(label_pair_const_t){.name="issuer", .value= issuer },
                             &(label_pair_const_t){.name="serial", .value= serial },
                             NULL);
    }

    plugin_dispatch_metric_family_array(cert_cb->fams, FAM_CERT_MAX, 0);

error:
    if (ssl != NULL)
        SSL_free(ssl);
    if (fd >= 0)
        close(fd);
    if (cert != NULL)
        X509_free(cert);
    if (ctx != NULL)
        SSL_CTX_free(ctx);
    if (subj != NULL)
        OPENSSL_free(subj);
    if (issuer != NULL)
        OPENSSL_free(issuer);
    if (bn != NULL)
        BN_free(bn);
    if (serial != NULL)
        OPENSSL_free(serial);

    return 0;
}

static int cert_config_instance(config_item_t *ci)
{
    cert_cb_t *cert_cb = calloc(1, sizeof(*cert_cb));
    if (cert_cb == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    memcpy(cert_cb->fams, fams, FAM_CERT_MAX * sizeof(fams[0]));

    int status = cf_util_get_string(ci, &cert_cb->instance);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        cert_free(cert_cb);
        return -1;
    }

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &cert_cb->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_port_number(child, &cert_cb->port);
        } else if (strcasecmp("server-name", child->key) == 0) {
            status = cf_util_get_string(child, &cert_cb->server_name);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &cert_cb->labels);
        } else {
            PLUGIN_WARNING("Option `%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        cert_free(cert_cb);
        return status;
    }

    if (cert_cb->host == NULL) {
        cert_cb->host = strdup("localhost");
        if (cert_cb->host == NULL) {
            PLUGIN_ERROR("strdup failed.");
            cert_free(cert_cb);
            return -1;
        }
    }

    if (cert_cb->port == 0)
        cert_cb->port = 443;

    label_set_add(&cert_cb->labels, false, "instance", cert_cb->instance);

    return plugin_register_complex_read("cert", cert_cb->instance, cert_read, interval,
                                        &(user_data_t){.data = cert_cb, .free_func = cert_free});
}

static int cert_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = cert_config_instance(child);
        } else {
            PLUGIN_ERROR("Option `%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int cert_init(void)
{
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
    SSL_load_error_strings();

    if (SSL_library_init() < 0) {
        PLUGIN_ERROR("Could not initialize the OpenSSL library.");
        return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_init("cert", cert_init);
    plugin_register_config("cert", cert_config);
}
