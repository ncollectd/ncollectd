// SPDX-License-Identifier: GPL-2.0-only or ISC
// SPDX-FileCopyrightText: Copyright (c) 2003, 2004 Henning Brauer
// SPDX-FileCopyrightText: Copyright (c) 2012 Mike Miller
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Henning Brauer <henning at openbsd.org>
// SPDX-FileContributor: Mike Miller <mmiller at mgm51.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

/* Based on ntpd.c from  openntpd */

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/socket.h"

#include <sys/queue.h>
#include "imsg.h"

#define OPENNTP_CTLSOCKET  "/var/run/ntpd.sock"
#define	TRUSTLEVEL_BADPEER 6
#define	MAX_DISPLAY_WIDTH  80

enum {
    FAM_OPENNTPD_UP,
    FAM_OPENNTPD_CLOCK_STRATUM,
    FAM_OPENNTPD_CLOCK_SYNCED,
    FAM_OPENNTPD_CLOCK_OFFSET,
    FAM_OPENNTPD_CONSTRAINT_ERRORS,
    FAM_OPENNTPD_AVAILABLE_PEERS,
    FAM_OPENNTPD_VALID_PEERS,
    FAM_OPENNTPD_AVAILABLE_SENSORS,
    FAM_OPENNTPD_VALID_SENSORS,
    FAM_OPENNTPD_PEER_STRATUM,
    FAM_OPENNTPD_PEER_SYNCEDTO,
    FAM_OPENNTPD_PEER_WEIGHT,
    FAM_OPENNTPD_PEER_TRUSTLEVEL,
    FAM_OPENNTPD_PEER_NEXT,
    FAM_OPENNTPD_PEER_POLL,
    FAM_OPENNTPD_PEER_OFFSET,
    FAM_OPENNTPD_PEER_DELAY,
    FAM_OPENNTPD_PEER_JITTER,
    FAM_OPENNTPD_SENSOR_SYNCEDTO,
    FAM_OPENNTPD_SENSOR_WEIGHT,
    FAM_OPENNTPD_SENSOR_GOOD,
    FAM_OPENNTPD_SENSOR_STRATUM,
    FAM_OPENNTPD_SENSOR_NEXT,
    FAM_OPENNTPD_SENSOR_POLL,
    FAM_OPENNTPD_SENSOR_OFFSET,
    FAM_OPENNTPD_SENSOR_CORRECTION,
    FAM_OPENNTPD_MAX
};

static metric_family_t fams[FAM_OPENNTPD_MAX] = {
    [FAM_OPENNTPD_UP] = {
        .name = "openntpd_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the OpenNTPD server be reached.",
    },
    [FAM_OPENNTPD_CLOCK_STRATUM] = {
        .name = "openntpd_clock_stratum",
        .type = METRIC_TYPE_GAUGE,
        .help = "The distance from the reference clock."
    },
    [FAM_OPENNTPD_CLOCK_SYNCED] = {
        .name = "openntpd_clock_synced",
        .type = METRIC_TYPE_GAUGE,
        .help = "Is the clock synced.",
    },
    [FAM_OPENNTPD_CLOCK_OFFSET] = {
        .name = "openntpd_clock_offset_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_CONSTRAINT_ERRORS] = {
        .name = "openntpd_constraint_errors",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_AVAILABLE_PEERS] = {
        .name = "openntpd_available_peers",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_VALID_PEERS] = {
        .name = "openntpd_valid_peers",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_AVAILABLE_SENSORS] = {
        .name = "openntpd_available_sensors",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_VALID_SENSORS] = {
        .name = "openntpd_valid_sensors",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_PEER_STRATUM] = {
        .name = "openntpd_peer_stratum",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_PEER_SYNCEDTO] = {
        .name = "openntpd_peer_syncedto",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_PEER_WEIGHT] = {
        .name = "openntpd_peer_weight",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_PEER_TRUSTLEVEL] = {
        .name = "openntpd_peer_trustlevel",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_PEER_NEXT] = {
        .name = "openntpd_peer_next",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_PEER_POLL] = {
        .name = "openntpd_peer_poll",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_PEER_OFFSET] = {
        .name = "openntpd_peer_offset",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_PEER_DELAY] = {
        .name = "openntpd_peer_delay",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_PEER_JITTER] = {
        .name = "openntpd_peer_jitter",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_SENSOR_SYNCEDTO] = {
        .name = "openntpd_sensor_syncedto",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_SENSOR_WEIGHT] = {
        .name = "openntpd_sensor_weight",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_SENSOR_GOOD] = {
        .name = "openntpd_sensor_good",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_SENSOR_STRATUM] = {
        .name = "openntpd_sensor_stratum",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_SENSOR_NEXT] = {
        .name = "openntpd_sensor_next",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_SENSOR_POLL] = {
        .name = "openntpd_sensor_poll",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_SENSOR_OFFSET] = {
        .name = "openntpd_sensor_offset",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENNTPD_SENSOR_CORRECTION] = {
        .name = "openntpd_sensor_correction",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
};

typedef struct {
    char *name;
    char *path;
    cdtime_t timeout;
    label_set_t labels;
    plugin_filter_t *filter;
    metric_family_t fams[FAM_OPENNTPD_MAX];
} openntpd_ctx_t;

enum imsg_type {
    IMSG_NONE,
    IMSG_ADJTIME,
    IMSG_ADJFREQ,
    IMSG_SETTIME,
    IMSG_HOST_DNS,
    IMSG_CONSTRAINT_DNS,
    IMSG_CONSTRAINT_QUERY,
    IMSG_CONSTRAINT_RESULT,
    IMSG_CONSTRAINT_CLOSE,
    IMSG_CONSTRAINT_KILL,
    IMSG_CTL_SHOW_STATUS,
    IMSG_CTL_SHOW_PEERS,
    IMSG_CTL_SHOW_PEERS_END,
    IMSG_CTL_SHOW_SENSORS,
    IMSG_CTL_SHOW_SENSORS_END,
    IMSG_CTL_SHOW_ALL,
    IMSG_CTL_SHOW_ALL_END,
    IMSG_SYNCED,
    IMSG_UNSYNCED,
    IMSG_PROBE_ROOT
};

struct ctl_show_status {
    time_t  constraint_median;
    time_t  constraint_last;
    double  clock_offset;
    u_int   peercnt;
    u_int   sensorcnt;
    u_int   valid_peers;
    u_int   valid_sensors;
    u_int   constraint_errors;
    uint8_t synced;
    uint8_t stratum;
    uint8_t constraints;
};

struct ctl_show_peer {
    char    peer_desc[MAX_DISPLAY_WIDTH];
    uint8_t syncedto;
    uint8_t weight;
    uint8_t trustlevel;
    uint8_t stratum;
    time_t  next;
    time_t  poll;
    double  offset;
    double  delay;
    double  jitter;
};

struct ctl_show_sensor {
    char    sensor_desc[MAX_DISPLAY_WIDTH];
    uint8_t syncedto;
    uint8_t weight;
    uint8_t good;
    uint8_t stratum;
    time_t  next;
    time_t  poll;
    double  offset;
    double  correction;
};

static int read_status_msg(openntpd_ctx_t *ctx, struct imsg *imsg)
{
    if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(struct ctl_show_status)) {
        PLUGIN_ERROR("Invalid IMSG_CTL_SHOW_STATUS received");
        return -1;
    }

    struct ctl_show_status *cstatus = (struct ctl_show_status *)imsg->data;

    metric_family_append(&ctx->fams[FAM_OPENNTPD_AVAILABLE_PEERS],
                         VALUE_GAUGE(cstatus->peercnt), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_VALID_PEERS],
                         VALUE_GAUGE(cstatus->valid_peers), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_AVAILABLE_SENSORS],
                         VALUE_GAUGE(cstatus->sensorcnt), &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_VALID_SENSORS],
                         VALUE_GAUGE(cstatus->valid_sensors), &ctx->labels, NULL);

    metric_family_append(&ctx->fams[FAM_OPENNTPD_CONSTRAINT_ERRORS],
                         VALUE_GAUGE(cstatus->constraint_errors), &ctx->labels, NULL);

    metric_family_append(&ctx->fams[FAM_OPENNTPD_CLOCK_STRATUM], VALUE_GAUGE(cstatus->stratum),
                         &ctx->labels, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_CLOCK_SYNCED], VALUE_GAUGE(cstatus->synced),
                         &ctx->labels, NULL);

    double clock_offset = cstatus->clock_offset < 0 ?
                          -1.0 * cstatus->clock_offset : cstatus->clock_offset;
    metric_family_append(&ctx->fams[FAM_OPENNTPD_CLOCK_OFFSET], VALUE_GAUGE(clock_offset * 0.001),
                         &ctx->labels, NULL);
    
    return 0;
}

static int read_peer_msg(openntpd_ctx_t *ctx, struct imsg *imsg)
{
    if (imsg->hdr.type == IMSG_CTL_SHOW_PEERS_END) {
        if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(int)) {
            PLUGIN_ERROR("Invalid IMSG_CTL_SHOW_PEERS_END received");
            return -1;
        }
        return 0;
    }

    if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(struct ctl_show_peer)) {
        PLUGIN_ERROR("Invalid IMSG_CTL_SHOW_PEERS received");
        return -1;
    }

    struct ctl_show_peer *cpeer = (struct ctl_show_peer *)imsg->data;

    if (strlen(cpeer->peer_desc) > MAX_DISPLAY_WIDTH - 1) {
        PLUGIN_ERROR("Peer_desc is too long");
        return -1;
    }

    metric_family_append(&ctx->fams[FAM_OPENNTPD_PEER_STRATUM],
                         VALUE_GAUGE(cpeer->stratum), &ctx->labels, 
                         &(label_pair_const_t){.name="peer", .value=cpeer->peer_desc}, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_PEER_SYNCEDTO],
                         VALUE_GAUGE(cpeer->syncedto), &ctx->labels, 
                         &(label_pair_const_t){.name="peer", .value=cpeer->peer_desc}, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_PEER_WEIGHT],
                         VALUE_GAUGE(cpeer->weight), &ctx->labels, 
                         &(label_pair_const_t){.name="peer", .value=cpeer->peer_desc}, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_PEER_TRUSTLEVEL],
                         VALUE_GAUGE(cpeer->trustlevel), &ctx->labels, 
                         &(label_pair_const_t){.name="peer", .value=cpeer->peer_desc}, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_PEER_NEXT],
                         VALUE_GAUGE(cpeer->next), &ctx->labels, 
                         &(label_pair_const_t){.name="peer", .value=cpeer->peer_desc}, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_PEER_POLL],
                         VALUE_GAUGE(cpeer->poll), &ctx->labels, 
                         &(label_pair_const_t){.name="peer", .value=cpeer->peer_desc}, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_PEER_OFFSET],
                         VALUE_GAUGE(cpeer->offset * 0.001), &ctx->labels, 
                         &(label_pair_const_t){.name="peer", .value=cpeer->peer_desc}, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_PEER_DELAY],
                         VALUE_GAUGE(cpeer->delay * 0.001), &ctx->labels, 
                         &(label_pair_const_t){.name="peer", .value=cpeer->peer_desc}, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_PEER_JITTER],
                         VALUE_GAUGE(cpeer->jitter * 0.001), &ctx->labels, 
                         &(label_pair_const_t){.name="peer", .value=cpeer->peer_desc}, NULL);
 
    return 0;
}

static int read_sensor_msg(openntpd_ctx_t *ctx, struct imsg *imsg)
{
    if (imsg->hdr.type == IMSG_CTL_SHOW_SENSORS_END) {
        if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(int)) {
            PLUGIN_ERROR("Invalid IMSG_CTL_SHOW_SENSORS_END received");
            return -1;
        }
        return 0;
    }

    if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(struct ctl_show_sensor)) {
        PLUGIN_ERROR("Invalid IMSG_CTL_SHOW_SENSORS received");
        return -1;
    }

    struct ctl_show_sensor *csensor = (struct ctl_show_sensor *)imsg->data;

    if (strlen(csensor->sensor_desc) > MAX_DISPLAY_WIDTH - 1) {
        PLUGIN_ERROR("Sensor_desc is too long");
        return -1;
    }

    metric_family_append(&ctx->fams[FAM_OPENNTPD_SENSOR_SYNCEDTO],
                         VALUE_GAUGE(csensor->syncedto), &ctx->labels,
                         &(label_pair_const_t){.name="sensor", .value=csensor->sensor_desc}, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_SENSOR_WEIGHT],
                         VALUE_GAUGE(csensor->weight), &ctx->labels,
                         &(label_pair_const_t){.name="sensor", .value=csensor->sensor_desc}, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_SENSOR_GOOD],
                         VALUE_GAUGE(csensor->good), &ctx->labels,
                         &(label_pair_const_t){.name="sensor", .value=csensor->sensor_desc}, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_SENSOR_STRATUM],
                         VALUE_GAUGE(csensor->stratum), &ctx->labels,
                         &(label_pair_const_t){.name="sensor", .value=csensor->sensor_desc}, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_SENSOR_NEXT],
                         VALUE_GAUGE(csensor->next), &ctx->labels,
                         &(label_pair_const_t){.name="sensor", .value=csensor->sensor_desc}, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_SENSOR_POLL],
                         VALUE_GAUGE(csensor->poll), &ctx->labels,
                         &(label_pair_const_t){.name="sensor", .value=csensor->sensor_desc}, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_SENSOR_OFFSET],
                         VALUE_GAUGE(csensor->offset * 0.001), &ctx->labels,
                         &(label_pair_const_t){.name="sensor", .value=csensor->sensor_desc}, NULL);
    metric_family_append(&ctx->fams[FAM_OPENNTPD_SENSOR_CORRECTION],
                         VALUE_GAUGE(csensor->correction * 0.001), &ctx->labels,
                         &(label_pair_const_t){.name="sensor", .value=csensor->sensor_desc}, NULL);

    return 0;
}

static int openntpd_read(user_data_t *user_data)
{
    openntpd_ctx_t *ctx = user_data->data;

    int fd = socket_connect_unix_stream(ctx->path, ctx->timeout);
    if (fd < 0) {
        metric_family_append(&ctx->fams[FAM_OPENNTPD_UP], VALUE_GAUGE(0), &ctx->labels, NULL);
        plugin_dispatch_metric_family(&ctx->fams[FAM_OPENNTPD_UP], 0);
        return 0;
    }

    struct imsgbuf ibuf_ctl;
    imsg_init(&ibuf_ctl, fd);

    imsg_compose(&ibuf_ctl, IMSG_CTL_SHOW_ALL, 0, 0, -1, NULL, 0);

    while (ibuf_ctl.w.queued) {
        if ((msgbuf_write(&ibuf_ctl.w) <= 0) && (errno != EAGAIN)) {
            PLUGIN_ERROR("msgbuf_write error");
            close(fd);
            metric_family_append(&ctx->fams[FAM_OPENNTPD_UP], VALUE_GAUGE(0), &ctx->labels, NULL);
            plugin_dispatch_metric_family(&ctx->fams[FAM_OPENNTPD_UP], 0);
            return 0;
        }
    }

    metric_family_append(&ctx->fams[FAM_OPENNTPD_UP], VALUE_GAUGE(1), &ctx->labels, NULL);

    bool done = false;
    while (!done) {
        int n = imsg_read(&ibuf_ctl);
        if ((n == -1) && (errno != EAGAIN)) {
            PLUGIN_ERROR("imsg_read error");
            break;
        }
        if (n == 0) {
            PLUGIN_ERROR("Pipe closed");
            break;
        }

        while (!done) {
            struct imsg imsg = {0};
            n = imsg_get(&ibuf_ctl, &imsg);
            if (n == -1) {
                PLUGIN_ERROR("ibuf_ctl: imsg_get error");
                imsg_free(&imsg);
                done = true;
                break;
            }
            if (n == 0)
                break;

            int status = 0;
            switch (imsg.hdr.type) {
            case IMSG_CTL_SHOW_STATUS:
                status = read_status_msg(ctx, &imsg);
                break;
            case IMSG_CTL_SHOW_PEERS:
                status = read_peer_msg(ctx, &imsg);
                break;
            case IMSG_CTL_SHOW_SENSORS:
                status = read_sensor_msg(ctx, &imsg);
                break;
            case IMSG_CTL_SHOW_PEERS_END:
            case IMSG_CTL_SHOW_SENSORS_END:
                /* do nothing */
                break;
            case IMSG_CTL_SHOW_ALL_END:
                done = true;
                break;
            default:
                /* no action taken */
                break;
            }

            imsg_free(&imsg);

            if (status != 0)
                break;
        }
    }

    close(fd);

    plugin_dispatch_metric_family_array_filtered(ctx->fams, FAM_OPENNTPD_MAX, ctx->filter, 0);

    return 0;
}

static void openntpd_free(void *arg)
{
    openntpd_ctx_t *ctx = arg;

    if (ctx == NULL)
        return;

    free(ctx->name);
    free(ctx->path);

    label_set_reset(&ctx->labels);
    plugin_filter_free(ctx->filter);

    free(ctx);
}

static int openntpd_config_instance(config_item_t *ci)
{
    openntpd_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    memcpy(ctx->fams, fams, sizeof(ctx->fams[0])*FAM_OPENNTPD_MAX);

    int status = cf_util_get_string(ci, &ctx->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(ctx);
        return status;
    }

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("socket-path", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->path);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &ctx->timeout);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &ctx->filter);
        } else {
            PLUGIN_ERROR("Option `%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        openntpd_free(ctx);
        return -1;
    }

    if (ctx->path== NULL) {
        ctx->path = strdup(OPENNTP_CTLSOCKET);
        if (ctx->path == NULL) {
            openntpd_free(ctx);
            return -1;
        }
    }

    if (ctx->timeout == 0)
        ctx->timeout = interval == 0 ? plugin_get_interval()/2 : interval/2;

    label_set_add(&ctx->labels, true, "instance", ctx->name);

    return plugin_register_complex_read("openntpd", ctx->name, openntpd_read, interval,
                                        &(user_data_t){.data = ctx, .free_func = openntpd_free});
}

static int openntpd_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = openntpd_config_instance(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' is not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("openntpd", openntpd_config);
}
