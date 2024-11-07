// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2006-2015 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2006 Anthony Gialluca
// SPDX-FileCopyrightText: Copyright (C) 2000-2004 Kern Sibbald
// SPDX-FileCopyrightText: Copyright (C) 1996-1999 Andre M. Hedrick
// SPDX-FileContributor: Anthony Gialluca <tonyabg at charter.net>
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

#include "plugin.h"
#include "libutils/common.h"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#define APCUPS_SERVER_TIMEOUT 15.0
#define APCUPS_DEFAULT_HOST "localhost"
#define APCUPS_DEFAULT_SERVICE "3551"

enum {
    FAM_APCUPS_INPUT_VOLTAGE_VOLTS,
    FAM_APCUPS_OUTPUT_VOLTAGE_VOLTS,
    FAM_APCUPS_BATTERY_VOLTAGE_VOLTS,
    FAM_APCUPS_BATTERY_CHARGE_RATIO,
    FAM_APCUPS_LOAD_RATIO,
    FAM_APCUPS_BATTERY_TIMELEFT_SECONDS,
    FAM_APCUPS_TEMPERATURE_CELSIUS,
    FAM_APCUPS_INPUT_FREQUENCY_HZ,
    FAM_APCUPS_MAX,
};

static metric_family_t fams[FAM_APCUPS_MAX] = {
    [FAM_APCUPS_INPUT_VOLTAGE_VOLTS] = {
        .name = "apcups_input_voltage_volts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Input voltage (V)",
    },
    [FAM_APCUPS_OUTPUT_VOLTAGE_VOLTS] = {
        .name = "apcups_output_voltage_volts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Output voltage (V)",
    },
    [FAM_APCUPS_BATTERY_VOLTAGE_VOLTS] = {
        .name = "apcups_battery_voltage_volts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Battery voltage (V)",
    },
    [FAM_APCUPS_BATTERY_CHARGE_RATIO] = {
        .name = "apcups_battery_charge_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Battery charge (percent)",
    },
    [FAM_APCUPS_LOAD_RATIO] = {
        .name = "apcups_load_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Load on UPS (percent)",
    },
    [FAM_APCUPS_BATTERY_TIMELEFT_SECONDS] = {
        .name = "apcups_battery_timeleft_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Battery time left in seconds.",
    },
    [FAM_APCUPS_TEMPERATURE_CELSIUS] = {
        .name = "apcups_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "UPS temperature (degrees C)",
    },
    [FAM_APCUPS_INPUT_FREQUENCY_HZ] = {
        .name = "apcups_input_frequency_hz",
        .type = METRIC_TYPE_GAUGE,
        .help = "Input line frequency (Hz)",
    }
};

typedef struct {
    double linev;
    double loadpct;
    double bcharge;
    double timeleft;
    double outputv;
    double itemp;
    double battv;
    double linefreq;
} apc_detail_t;

typedef struct {
    char *name;
    char *host;
    char *service;
    bool persistent_conn;
    int sockfd;
    int retries;
    int iterations;
    label_set_t labels;
    plugin_filter_t *filter;
    metric_family_t fams[FAM_APCUPS_MAX];
} apc_ctx_t;

static int net_shutdown(int *fd)
{
    if ((fd == NULL) || (*fd < 0))
        return EINVAL;

    uint16_t packet_size = 0;
    (void)swrite(*fd, (void *)&packet_size, sizeof(packet_size));
    close(*fd);
    *fd = -1;

    return 0;
}

/*
 * Open a TCP connection to the UPS network server
 * Returns -1 on error
 * Returns socket file descriptor otherwise
 */
static int net_open(apc_ctx_t *ctx)
{
    /* TODO: Change this to `AF_UNSPEC' if apcupsd can handle IPv6 */
    struct addrinfo ai_hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM};
    struct addrinfo *ai_return;
    struct addrinfo *ai_list;
    int status = getaddrinfo(ctx->host, ctx->service, &ai_hints, &ai_return);
    if (status != 0) {
        PLUGIN_INFO("getaddrinfo failed: %s",
                 (status == EAI_SYSTEM) ? STRERRNO : gai_strerror(status));
        return -1;
    }

    /* Create socket */
    int sd = -1;
    for (ai_list = ai_return; ai_list != NULL; ai_list = ai_list->ai_next) {
        sd = socket(ai_list->ai_family, ai_list->ai_socktype, ai_list->ai_protocol);
        if (sd >= 0)
            break;
    }
    /* `ai_list' still holds the current description of the socket.. */

    if (sd < 0) {
        PLUGIN_DEBUG("Unable to open a socket");
        freeaddrinfo(ai_return);
        return -1;
    }

    status = connect(sd, ai_list->ai_addr, ai_list->ai_addrlen);

    freeaddrinfo(ai_return);

    if (status != 0) { /* `connect(2)' failed */
        PLUGIN_INFO("connect failed: %s", STRERRNO);
        close(sd);
        return -1;
    }

    PLUGIN_DEBUG("Done opening a socket %i", sd);

    return sd;
}

/*
 * Receive a message from the other end. Each message consists of
 * two packets. The first is a header that contains the size
 * of the data that follows in the second packet.
 * Returns number of bytes read
 * Returns 0 on end of file
 * Returns -1 on hard end of file (i.e. network connection close)
 * Returns -2 on error
 */
static int net_recv(int *sockfd, char *buf, int buflen)
{
    /* get data size -- in short */
    uint16_t packet_size = 0;
    if (sread(*sockfd, (void *)&packet_size, sizeof(packet_size)) != 0) {
        close(*sockfd);
        *sockfd = -1;
        return -1;
    }

    packet_size = ntohs(packet_size);
    if (packet_size > buflen) {
        PLUGIN_ERROR("Received %" PRIu16 " bytes of payload "
                    "but have only %i bytes of buffer available.",
                    packet_size, buflen);
        close(*sockfd);
        *sockfd = -1;
        return -2;
    }

    if (packet_size == 0)
        return 0;

    /* now read the actual data */
    if (sread(*sockfd, (void *)buf, packet_size) != 0) {
        close(*sockfd);
        *sockfd = -1;
        return -1;
    }

    return (int)packet_size;
}

/*
 * Send a message over the network. The send consists of
 * two network packets. The first is sends a short containing
 * the length of the data packet which follows.
 * Returns zero on success
 * Returns non-zero on error
 */
static int net_send(int *sockfd, char *buff, int len)
{
    assert(len > 0);
    assert(*sockfd >= 0);

    /* send short containing size of data packet */
    uint16_t packet_size = htons((uint16_t)len);

    if (swrite(*sockfd, (void *)&packet_size, sizeof(packet_size)) != 0) {
        close(*sockfd);
        *sockfd = -1;
        return -1;
    }

    /* send data packet */
    if (swrite(*sockfd, (void *)buff, len) != 0) {
        close(*sockfd);
        *sockfd = -1;
        return -2;
    }

    return 0;
}

/* Get and print status from apcupsd NIS server */
static int apc_query_server(apc_ctx_t *ctx, apc_detail_t *apcups_detail)
{

    int status = 0;
    int try = 0;
    while (true) {
        if (ctx->sockfd < 0) {
            ctx->sockfd = net_open(ctx);
            if (ctx->sockfd < 0) {
                PLUGIN_ERROR("Connecting to the "
                            "apcupsd failed.");
                return -1;
            }
        }

        status = net_send(&ctx->sockfd, "status", strlen("status"));
        if (status != 0) {
            /* net_send closes the socket on error. */
            assert(ctx->sockfd < 0);
            if (try == 0) {
                try++;
                ctx->retries++;
                continue;
            }

            PLUGIN_ERROR("Writing to the socket failed.");
            return -1;
        }

        break;
    }

    /* When collectd's collection interval is larger than apcupsd's
     * timeout, we would have to retry / re-connect each iteration. Try to
     * detect this situation and shut down the socket gracefully in that
     * case. Otherwise, keep the socket open to avoid overhead. */
    ctx->iterations++;
    if ((ctx->iterations == 10) && (ctx->retries > 2)) {
        PLUGIN_NOTICE("There have been %i retries in the first %i iterations. "
                      "Will close the socket in future iterations.",
                      ctx->retries, ctx->iterations);
        ctx->persistent_conn = false;
    }

    int n;
    char recvline[1024];
    while ((n = net_recv(&ctx->sockfd, recvline, sizeof(recvline) - 1)) > 0) {
        assert((size_t)n < sizeof(recvline));
        recvline[n] = 0;
#ifdef APCMAIN
        printf("net_recv = '%s';\n", recvline);
#endif

        char *toksaveptr = NULL;
        char *tokptr = strtok_r(recvline, " :\t", &toksaveptr);
        while (tokptr != NULL) {
            char *key = tokptr;
            if ((tokptr = strtok_r(NULL, " :\t", &toksaveptr)) == NULL)
                continue;

            double value = 0;
            if (strtodouble(tokptr, &value) != 0)
                continue;

#ifdef APCMAIN
            printf("    Found property: name = %s; value = %f;\n", key, value)
#endif

            if (strcmp("LINEV", key) == 0) {
                apcups_detail->linev = value;
            } else if (strcmp("BATTV", key) == 0) {
                apcups_detail->battv = value;
            } else if (strcmp("ITEMP", key) == 0) {
                apcups_detail->itemp = value;
            } else if (strcmp("LOADPCT", key) == 0) {
                apcups_detail->loadpct = value / 100.0;
            } else if (strcmp("BCHARGE", key) == 0) {
                apcups_detail->bcharge = value / 100.0;
            } else if (strcmp("OUTPUTV", key) == 0) {
                apcups_detail->outputv = value;
            } else if (strcmp("LINEFREQ", key) == 0) {
                apcups_detail->linefreq = value;
            } else if (strcmp("TIMELEFT", key) == 0) {
                /* Convert minutes to seconds */
                apcups_detail->timeleft = value * 60.0;
            }

            tokptr = strtok_r(NULL, ":", &toksaveptr);
        }
    }
    status = errno; /* save errno, net_shutdown() may re-set it. */

    if (!ctx->persistent_conn)
        net_shutdown(&ctx->sockfd);

    if (n < 0) {
        PLUGIN_ERROR("Reading from socket failed: %s", STRERROR(status));
        return -1;
    }

    return 0;
}

static void apcups_free(void *arg)
{
    apc_ctx_t *ctx = arg;

    if (ctx == NULL)
        return;

    net_shutdown(&ctx->sockfd);

    free(ctx->name);
    free(ctx->host);
    free(ctx->service);
    label_set_reset(&ctx->labels);
    plugin_filter_free(ctx->filter);
    free(ctx);
}

static int apcups_read(user_data_t *user_data)
{
    apc_ctx_t *ctx = user_data->data;

    apc_detail_t apcups_detail = {
        .linev = NAN,
        .outputv = NAN,
        .battv = NAN,
        .loadpct = NAN,
        .bcharge = NAN,
        .timeleft = NAN,
        .itemp = NAN,
        .linefreq = NAN,
    };

    int status = apc_query_server(ctx, &apcups_detail);
    if (status != 0) {
        PLUGIN_DEBUG("apc_query_server (\"%s\", \"%s\") = %d", ctx->host, ctx->service, status);
        return status;
    }

    metric_family_append(&ctx->fams[FAM_APCUPS_INPUT_VOLTAGE_VOLTS],
                         VALUE_GAUGE(apcups_detail.linev), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_APCUPS_OUTPUT_VOLTAGE_VOLTS],
                         VALUE_GAUGE(apcups_detail.outputv), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_APCUPS_BATTERY_VOLTAGE_VOLTS],
                         VALUE_GAUGE(apcups_detail.battv), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_APCUPS_BATTERY_CHARGE_RATIO],
                         VALUE_GAUGE(apcups_detail.bcharge), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_APCUPS_LOAD_RATIO],
                         VALUE_GAUGE(apcups_detail.loadpct), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_APCUPS_BATTERY_TIMELEFT_SECONDS],
                         VALUE_GAUGE(apcups_detail.timeleft), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_APCUPS_TEMPERATURE_CELSIUS],
                         VALUE_GAUGE(apcups_detail.itemp), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_APCUPS_INPUT_FREQUENCY_HZ],
                         VALUE_GAUGE(apcups_detail.linefreq), &ctx->labels, NULL);

    plugin_dispatch_metric_family_array_filtered(ctx->fams, FAM_APCUPS_MAX, ctx->filter, 0);

    return 0;
}

static int apcups_config_instance(config_item_t *ci)
{
    apc_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    memcpy(ctx->fams, fams, sizeof(ctx->fams[0])*FAM_APCUPS_MAX);

    int status = cf_util_get_string(ci, &ctx->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(ctx);
        return status;
    }
    assert(ctx->name != NULL);

    ctx->persistent_conn = true;
    ctx->sockfd = -1;

    cdtime_t interval = plugin_get_interval();

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "host") == 0) {
            status = cf_util_get_string(child, &ctx->host);
        } else if (strcasecmp(child->key, "port") == 0) {
            status = cf_util_get_service(child, &ctx->service);
        } else if (strcasecmp(child->key, "persistent-connection") == 0) {
            status = cf_util_get_boolean(child, &ctx->persistent_conn);
        } else if (strcasecmp(child->key, "label") == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp(child->key, "interval") == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = plugin_filter_configure(child, &ctx->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        apcups_free(ctx);
        return -1;
    }

    if (ctx->persistent_conn) {
        if (CDTIME_T_TO_DOUBLE(interval) > APCUPS_SERVER_TIMEOUT) {
            PLUGIN_NOTICE("Plugin poll interval set to %.3f seconds. "
                          "Apcupsd NIS socket timeout is %.3f seconds, "
                          "PersistentConnection disabled by default.",
                          CDTIME_T_TO_DOUBLE(interval), APCUPS_SERVER_TIMEOUT);
            ctx->persistent_conn = false;
        }
    }

    if (ctx->host == NULL) {
        ctx->host = strdup(APCUPS_DEFAULT_HOST);
        if (ctx->host == NULL) {
            PLUGIN_ERROR("strdup failed.");
            apcups_free(ctx);
            return -1;
        }
    }

    if (ctx->service == NULL) {
        ctx->service = strdup(APCUPS_DEFAULT_SERVICE);
        if (ctx->service == NULL) {
            PLUGIN_ERROR("strdup failed.");
            apcups_free(ctx);
            return -1;
        }
    }

    label_set_add(&ctx->labels, true, "instance", ctx->name);

    return plugin_register_complex_read("apcups", ctx->name, apcups_read, interval,
                                        &(user_data_t){.data = ctx, .free_func = apcups_free});

    return 0;
}

static int apcups_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = apcups_config_instance(child);
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

void module_register(void)
{
    plugin_register_config("apcups", apcups_config);
}
