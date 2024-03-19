// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/socket.h"

#include <netdb.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/bn.h>
#include <openssl/asn1.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#define NSD_SERVER_CERT_FILE "/etc/nsd/nsd_server.pem"
#define NSD_CONTROL_KEY_FILE "/etc/nsd/nsd_control.key"
#define NSD_CONTROL_CERT_FILE "/etc/nsd/nsd_control.pem"
#define NSD_CONTROL_PORT 8952

enum {
    FAM_NSD_UP,
    FAM_NSD_QUERIES,
    FAM_NSD_SERVER_QUERIES,
    FAM_NSD_UPTIME_SECONDS,
    FAM_NSD_DB_DISK_BYTES,
    FAM_NSD_DB_MEMORY_BYTES,
    FAM_NSD_XFRD_MEMORY_BYTES,
    FAM_NSD_CONFIG_DISK_BYTES,
    FAM_NSD_CONFIG_MEMORY_BYTES,
    FAM_NSD_QUERY_TYPE,
    FAM_NSD_QUERY_OPCODE,
    FAM_NSD_QUERY_CLASS,
    FAM_NSD_ANSWER_RCODE,
    FAM_NSD_QUERY_EDNS,
    FAM_NSD_QUERY_EDNS_ERROR,
    FAM_NSD_QUERY_PROTOCOL,
    FAM_NSD_ANSWER_WITHOUT_AA,
    FAM_NSD_QUERY_RX_ERROR,
    FAM_NSD_QUERY_TX_ERROR,
    FAM_NSD_AXFR_REQUEST,
    FAM_NSD_IXFR_REQUEST,
    FAM_NSD_ANSWER_TRUNCATED,
    FAM_NSD_QUERY_DROPPED,
    FAM_NSD_ZONE_MASTER,
    FAM_NSD_ZONE_SLAVE,
    FAM_NSD_MAX,
};

static metric_family_t fams[FAM_NSD_MAX] = {
    [FAM_NSD_UP] = {
        .name = "nsd_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the nsd server be reached.",
    },
    [FAM_NSD_QUERIES] = {
        .name = "nsd_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries received (the tls, tcp and udp queries added up).",
    },
    [FAM_NSD_SERVER_QUERIES] = {
        .name = "nsd_server_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries handled by the server process.",
    },
    [FAM_NSD_UPTIME_SECONDS] = {
        .name = "nsd_uptime_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Uptime in seconds since the server was started. With fractional seconds.",
    },
    [FAM_NSD_DB_DISK_BYTES] = {
        .name = "nsd_db_disk_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size of nsd.db on disk, in bytes.",
    },
    [FAM_NSD_DB_MEMORY_BYTES] = {
        .name = "nsd_db_memory_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size of the DNS database in memory, in bytes.",
    },
    [FAM_NSD_XFRD_MEMORY_BYTES] = {
        .name = "nsd_xfed_memory_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size of memory for zone transfers and notifies in xfrd process, "
                "excludes TSIG data, in bytes.",
    },
    [FAM_NSD_CONFIG_DISK_BYTES] = {
        .name = "nsd_config_disk_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size of zonelist file on disk, excludes the nsd.conf size, in bytes.",
    },
    [FAM_NSD_CONFIG_MEMORY_BYTES] = {
        .name = "nsd_config_memory_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Size of config data in memory, kept twice in server and xfrd process, in bytes.",
    },
    [FAM_NSD_QUERY_TYPE] = {
        .name = "nsd_query_type",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries with this query type.",
    },
    [FAM_NSD_QUERY_OPCODE] = {
        .name = "nsd_query_opcode",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries with this opcode.",
    },
    [FAM_NSD_QUERY_CLASS] = {
        .name = "nsd_query_class",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries with this query class.",
    },
    [FAM_NSD_ANSWER_RCODE] = {
        .name = "nsd_answer_rcode",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of answers that carried this return code.",
    },
    [FAM_NSD_QUERY_EDNS] = {
        .name = "nsd_query_edns",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries with EDNS OPT.",
    },
    [FAM_NSD_QUERY_EDNS_ERROR] = {
        .name = "nsd_query_edns_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries which failed EDNS parse.",
    },
    [FAM_NSD_QUERY_PROTOCOL] = {
        .name = "nsd_query_protocol",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries per protocol.",
    },
    [FAM_NSD_ANSWER_WITHOUT_AA] = {
        .name = "nsd_answer_without_aa",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of answers with NOERROR rcode and without AA flag, "
                "this includes the referrals.",
    },
    [FAM_NSD_QUERY_RX_ERROR] = {
        .name = "nsd_query_rx_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries for which the receive failed.",
    },
    [FAM_NSD_QUERY_TX_ERROR] = {
        .name = "nsd_query_tx_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of answers for which the transmit failed.",
    },
    [FAM_NSD_AXFR_REQUEST] = {
        .name = "nsd_axfr_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of AXFR requests from clients (that got served with reply).",
    },
    [FAM_NSD_IXFR_REQUEST] = {
        .name = "nsd_ixfr_request",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of IXFR requests from clients (that got served with reply).",
    },
    [FAM_NSD_ANSWER_TRUNCATED] = {
        .name = "nsd_answer_truncated",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of answers with TC flag set.",
    },
    [FAM_NSD_QUERY_DROPPED] = {
        .name = "nsd_query_dropped",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of queries that were dropped because they failed sanity check.",
    },
    [FAM_NSD_ZONE_MASTER] = {
        .name = "nsd_zone_master",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of master zones served. These are zones with no ‘request-xfr:’ entries.",
    },
    [FAM_NSD_ZONE_SLAVE] = {
        .name = "nsd_zone_slave",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of slave zones served. These are zones with ‘request-xfr’ entries.",
    },
};

#include "plugins/nsd/nsd.h"

typedef struct {
    char *name;
    char *host;
    char *socketpath;
    int port;
    char *server_cert_file;
    char *control_key_file;
    char *control_cert_file;
    cdtime_t timeout;
    label_set_t labels;
    metric_family_t fams[FAM_NSD_MAX];
} nsd_t;

static int nsd_parse_metric(nsd_t *nsd, char *line)
{
    char *value = strchr(line, '=');
    if (value == NULL)
        return -1;
    *value = '\0';
    value++;
    if (*value == '\0')
        return -1;

    char *key = line;
    size_t key_len = strlen(key);

    char buffer[64];
    char *lkey = NULL;
    char *lvalue = NULL;

    if (key[0] == 's') {
        if ((key_len > 6) && (memcmp("server", key, 6) == 0)) {
            lkey = "server";
            lvalue = key + 6;
            char *key1 = strchr(key, '.');
            if (key1 == NULL)
                return 0;
            *key1 = '\0';
            key1++;
            memcpy(buffer, "server.", 7);
            sstrncpy(buffer + 7, key1, sizeof(buffer) - 7);
            key_len = strlen(buffer);
            key =  buffer;
        }
    } else if (key[0] == 'n') {
        if ((key_len > 8) && (memcmp("num.type", key, 8) == 0)) {
            key[8] = '\0';
            key_len = 8;
            lkey = "type";
            lvalue = key + 9;
        } else if ((key_len > 9) && (memcmp("num.class", key, 9) == 0)) {
            key[9] = '\0';
            key_len = 9;
            lkey = "class";
            lvalue = key + 10;
        } else if ((key_len > 9) && (memcmp("num.rcode", key, 9) == 0)) {
            key[9] = '\0';
            key_len = 9;
            lkey = "rcode";
            lvalue = key + 10;
        } else if ((key_len > 10) && (memcmp("num.opcode", key, 10) == 0)) {
            key[10] = '\0';
            key_len = 10;
            lkey = "opcode";
            lvalue = key + 11;
        }
    }

    const struct nsd_metric *nm = nsd_get_key(key, key_len);
    if (nm == NULL)
        return 0;

    if (nm->fam < 0)
        return 0;

    value_t mvalue = {0};

    if (nsd->fams[nm->fam].type == METRIC_TYPE_COUNTER) {
        mvalue = VALUE_COUNTER(atoll(value));
    } else if (nsd->fams[nm->fam].type == METRIC_TYPE_GAUGE) {
        mvalue = VALUE_GAUGE(atof(value));
    } else {
        return 0;
    }

    if ((nm->lkey != NULL) && (nm->lvalue != NULL)) {
        if ((lkey != NULL) && (lvalue != NULL)) {
            metric_family_append(&nsd->fams[nm->fam], mvalue, &nsd->labels,
                                 &(label_pair_const_t){.name=lkey, .value=lvalue},
                                 &(label_pair_const_t){.name=nm->lkey, .value=nm->lvalue}, NULL);
        } else {
            metric_family_append(&nsd->fams[nm->fam], mvalue, &nsd->labels,
                                 &(label_pair_const_t){.name=nm->lkey, .value=nm->lvalue}, NULL);
        }
    } else {
        if ((lkey != NULL) && (lvalue != NULL)) {
            metric_family_append(&nsd->fams[nm->fam], mvalue, &nsd->labels,
                                 &(label_pair_const_t){.name=lkey, .value=lvalue}, NULL);
        } else {
            metric_family_append(&nsd->fams[nm->fam], mvalue, &nsd->labels, NULL);
        }
    }

    return 0;
}

static void nsd_free(void *data)
{
    nsd_t *nsd = data;

    if (nsd == NULL)
        return;

    free(nsd->name);
    free(nsd->host);
    free(nsd->socketpath);
    free(nsd->server_cert_file);
    free(nsd->control_key_file);
    free(nsd->control_cert_file);
    label_set_reset(&nsd->labels);

    free(nsd);
}

static int nsd_read_ssl(nsd_t *nsd)
{
    int fd = -1;
    SSL *ssl = NULL;

    const SSL_METHOD *method = SSLv23_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (ctx == NULL) {
        PLUGIN_ERROR("Unable to create a new SSL context structure.");
        return -1;
    }

    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);

    int status = SSL_CTX_use_certificate_file(ctx, nsd->control_cert_file, SSL_FILETYPE_PEM);
    if(status != 1) {
        PLUGIN_ERROR("Error setting up SSL_CTX client cert for '%s'.", nsd->control_cert_file);
        goto error;
    }

    status = SSL_CTX_use_PrivateKey_file(ctx, nsd->control_key_file, SSL_FILETYPE_PEM);
    if(status != 1) {
        PLUGIN_ERROR("Error setting up SSL_CTX client key for '%s'", nsd->control_key_file);
        goto error;
    }

    status = SSL_CTX_check_private_key(ctx);
    if(status != 1) {
        PLUGIN_ERROR("Error setting up SSL_CTX client key");
        goto error;
    }

    status = SSL_CTX_load_verify_locations(ctx, nsd->server_cert_file, NULL);
    if (status != 1) {
        PLUGIN_ERROR("Error setting up SSL_CTX verify, server cert for '%s'", nsd->server_cert_file);
        goto error;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    ssl = SSL_new(ctx);
    if (ssl == NULL) {
        PLUGIN_ERROR("SSL_new failed.");
        goto error;
    }
    SSL_set_connect_state(ssl);

    fd = socket_connect_tcp(nsd->host, nsd->port, 0, 0);
    if (fd < 0)
        goto error;

    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
    SSL_set_fd(ssl, fd);
    status = SSL_do_handshake(ssl);
    if (status != 1) {
        PLUGIN_ERROR("SSL handshake failed.");
        goto error;
    }

    if (SSL_get_verify_result(ssl) != X509_V_OK) {
        PLUGIN_ERROR("SSL handshake failed.");
        goto error;
    }

    X509 *cert = SSL_get_peer_certificate(ssl);
    if (cert == NULL) {
        PLUGIN_ERROR("SSL server presented no peer certificate");
        goto error;
    }
    X509_free(cert);

    const char *command = "NSDCT1 stats_noreset\n";
    int command_len = strlen(command);

    status = SSL_write(ssl, command, command_len);
    if (status <= 0) {
        unsigned long  err = ERR_peek_error();
        PLUGIN_ERROR("could not SSL_write %s", ERR_reason_error_string(err) );
        goto error;
    }
#if 0
    BIO *rbio = SSL_get_rbio(ssl);
    if (rbio == NULL) {
        PLUGIN_ERROR("SSL_get_rbio failed");
        goto error;
    }

    BIO *bbio = BIO_new(BIO_f_buffer());
    if (bbio == NULL) {
        PLUGIN_ERROR("BIO_new failed");
        goto error;
    }

    BIO_push(bbio, rbio);

    while (true) {
        char buffer[256];
        status = BIO_gets(bbio, buffer, sizeof(buffer));
        if (status == 0)
            break;
        if (status < 0) {
            fprintf(stderr,  "*** %d\n", status);
            break;
        }
        fprintf(stderr, "[%s]\n", buffer);
    }
#endif

    char buffer[1024];
    while (true) {
        int buffer_size = sizeof(buffer);
        int rsize = SSL_read(ssl, buffer, buffer_size - 1);
        if (rsize == 0)
            break;
        if (rsize <= 0) {
            unsigned long err = ERR_peek_error();
            PLUGIN_ERROR("could not SSL_read %d %s", rsize,  ERR_reason_error_string(err) );
            break;
        }
        buffer[rsize] = '\0';

        nsd_parse_metric(nsd, buffer);
    }


error:
    if (ssl != NULL)
        SSL_free(ssl);
    if (fd >= 0)
        close(fd);
    if (ctx != NULL)
        SSL_CTX_free(ctx);

    return 0;
}

static int nsd_read_stream(nsd_t *nsd)
{
    int fd = -1;

    if (nsd->socketpath != NULL)
        fd = socket_connect_unix_stream(nsd->socketpath, nsd->timeout);
    else
        fd = socket_connect_tcp(nsd->host, nsd->port, 0, 0);

    if (fd < 0)
        return -1;

    const char *command = "NSDCT1 stats_noreset\n";
    int command_len = strlen(command);
    if (send(fd, command, command_len, 0) < command_len) {
        PLUGIN_ERROR("unix socket send command failed");
        close(fd);
        return -1;
    }

    FILE *fh = fdopen(fd, "r");
    if (fh == NULL) {
        close(fd);
        return -1;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fh) != NULL)
        nsd_parse_metric(nsd, buffer);

    fclose(fh);

    return 0;
}

static int nsd_read(user_data_t *user_data)
{
    nsd_t *nsd = user_data->data;
    if (nsd == NULL)
        return  -1;

    int status = 0;
    if (nsd->socketpath != NULL)
        status = nsd_read_stream(nsd);
    else if (nsd->control_key_file == NULL)
        status = nsd_read_stream(nsd);
    else
        status = nsd_read_ssl(nsd);

    metric_family_append(&nsd->fams[FAM_NSD_UP], VALUE_GAUGE(status == 0 ? 1 : 0),
                         &nsd->labels, NULL);

    plugin_dispatch_metric_family_array(nsd->fams, FAM_NSD_MAX, 0);

    return 0;
}

static int nsd_config_instance (config_item_t *ci)
{
    nsd_t *nsd = calloc(1, sizeof(*nsd));
    if (nsd == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    memcpy(nsd->fams, fams, sizeof(nsd->fams[0])*FAM_NSD_MAX);

    int status = cf_util_get_string(ci, &nsd->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(nsd);
        return status;
    }

    nsd->port = NSD_CONTROL_PORT;

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &nsd->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_port_number(child, &nsd->port);
        } else if (strcasecmp("socket-path", child->key) == 0) {
            status = cf_util_get_string(child, &nsd->socketpath);
        } else if (strcasecmp("server-cert", child->key) == 0) {
            status = cf_util_get_string(child, &nsd->server_cert_file);
        } else if (strcasecmp("control-key", child->key) == 0) {
            status = cf_util_get_string(child, &nsd->control_key_file);
        } else if (strcasecmp("control-cert", child->key) == 0) {
            status = cf_util_get_string(child, &nsd->control_cert_file);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &nsd->timeout);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &nsd->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        nsd_free(nsd);
        return -1;
    }

    if ((nsd->host == NULL) && (nsd->socketpath == NULL)) {
        PLUGIN_ERROR("Missing 'host' or 'socket-path' option.");
        nsd_free(nsd);
        return -1;
    }

    if ((nsd->host != NULL) && (strcmp(nsd->host, "::1") != 0) &&
        (strcmp(nsd->host, "127.0.0.1") != 0) && (strcmp(nsd->host, "localhost") != 0)) {

        if (nsd->server_cert_file == NULL) {
            PLUGIN_ERROR("Missing 'server-cert' option.");
            nsd_free(nsd);
            return -1;
        }

        if (nsd->control_key_file == NULL) {
            PLUGIN_ERROR("Missing 'control-key' option.");
            nsd_free(nsd);
            return -1;
        }

        if (nsd->control_cert_file == NULL) {
            PLUGIN_ERROR("Missing 'control-cert' option");
            nsd_free(nsd);
            return -1;
        }
    }

    label_set_add(&nsd->labels, true, "instance", nsd->name);

    return plugin_register_complex_read("nsd", nsd->name, nsd_read, interval,
                                        &(user_data_t){.data = nsd, .free_func = nsd_free});
}

static int nsd_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = nsd_config_instance(child);
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

static int nsd_init(void)
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
    plugin_register_config("nsd", nsd_config);
    plugin_register_init("nsd", nsd_init);
}
