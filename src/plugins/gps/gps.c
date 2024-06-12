// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2015  Nicolas JOURDEN
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Nicolas JOURDEN <nicolas.jourden at laposte.net>
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Marc Fournier <marc.fournier at camptocamp.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/avltree.h"

#include <gps.h>
#include <pthread.h>

#define CGPS_DEFAULT_HOST "localhost"
#define CGPS_DEFAULT_PORT "2947"
#define CGPS_DEFAULT_TIMEOUT MS_TO_CDTIME_T(15)
#define CGPS_DEFAULT_PAUSE_CONNECT TIME_T_TO_CDTIME_T(5)
#define CGPS_MAX_ERROR 100
#define CGPS_CONFIG "?WATCH={\"enable\":true,\"json\":true,\"nmea\":false}\r\n"

enum {
    FAM_GPSD_SATELLITES_VISIBLE,
    FAM_GPSD_SATELLITES_USED,
    FAM_GPSD_HDOP,
    FAM_GPSD_VDOP,
    FAM_GPSD_PDOP,
    FAM_GPSD_MODE,
    FAM_GPSD_LATITUDE_DEGREES,
    FAM_GPSD_EPY_METERS,
    FAM_GPSD_LONGITUDE_DEGREES,
    FAM_GPSD_EPX_METERS,
    FAM_GPSD_ALTITUDE,
    FAM_GPSD_EPV,
    FAM_GPSD_SPEED_METERS_PER_SECOND,
    FAM_GPSD_EPS_METERS_PER_SECOND,
    FAM_GPSD_CLIMB_METERS_PER_SECOND,
    FAM_GPSD_EPC_METERS_PER_SECOND,
    FAM_GPSD_MAX,
};

static metric_family_t fams[FAM_GPSD_MAX] = {
    [FAM_GPSD_SATELLITES_VISIBLE] = {
        .name = "gpsd_satellites_visible",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of satellites in view."
    },
    [FAM_GPSD_SATELLITES_USED] = {
        .name = "gpsd_satellites_used",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of satellites used in solution.",
    },
    [FAM_GPSD_HDOP] = {
        .name = "gpsd_hdop",
        .type = METRIC_TYPE_GAUGE,
        .help = "Horizontal dilution of precision.",
    },
    [FAM_GPSD_VDOP] = {
        .name = "gpsd_vdop",
        .type = METRIC_TYPE_GAUGE,
        .help = "Vertical dilution of precision.",
    },
    [FAM_GPSD_PDOP] = {
        .name = "gpsd_pdop",
        .type = METRIC_TYPE_GAUGE,
        .help = "Position (3D) dilution of precision.",
    },
    [FAM_GPSD_MODE] = {
        .name = "gpsd_mode",
        .type = METRIC_TYPE_STATE_SET,
        .help = "Mode of fix: \"NO FIX\", \"2D FIX\" or \"3D FIX\".",
    },
    [FAM_GPSD_LATITUDE_DEGREES] = {
        .name = "gpsd_latitude_degrees",
        .type = METRIC_TYPE_GAUGE,
        .help = "Latitude in degrees.",
    },
    [FAM_GPSD_EPY_METERS] = {
        .name = "gpsd_epy_meters",
        .type = METRIC_TYPE_GAUGE,
        .help = "Latitude position uncertainty in meters.",
    },
    [FAM_GPSD_LONGITUDE_DEGREES] = {
        .name = "gpsd_longitude_degrees",
        .type = METRIC_TYPE_GAUGE,
        .help = "Longitude in degrees.",
    },
    [FAM_GPSD_EPX_METERS] = {
        .name = "gpsd_epx_meters",
        .type = METRIC_TYPE_GAUGE,
        .help = "Longitude position uncertainty in meters.",
    },
    [FAM_GPSD_ALTITUDE] = {
        .name = "gpsd_altitude",
        .type = METRIC_TYPE_GAUGE,
        .help = "Altitude in meters.",
    },
    [FAM_GPSD_EPV] = {
        .name = "gpsd_epv",
        .type = METRIC_TYPE_GAUGE,
        .help = "Vertical position uncertainty in meters.",
    },
    [FAM_GPSD_SPEED_METERS_PER_SECOND] = {
        .name = "gpsd_speed_meters_per_second",
        .type = METRIC_TYPE_GAUGE,
        .help = "Speed over ground, meters/sec.",
    },
    [FAM_GPSD_EPS_METERS_PER_SECOND] = {
        .name = "gpsd_eps_meters_per_second",
        .type = METRIC_TYPE_GAUGE,
        .help = "Speed uncertainty, meters/sec.",
    },
    [FAM_GPSD_CLIMB_METERS_PER_SECOND] = {
        .name = "gpsd_climb_meters_per_second",
        .type = METRIC_TYPE_GAUGE,
        .help = "Vertical speed, meters/sec.",
    },
    [FAM_GPSD_EPC_METERS_PER_SECOND] = {
        .name = "gpsd_epc_meters_per_second",
        .type = METRIC_TYPE_GAUGE,
        .help = "Vertical speed uncertainty.",
    },
};

typedef struct {
    int satellites_used;
    int satellites_visible;
    double hdop;
    double vdop;
    double pdop;
    int mode;
    double latitude;
    double epy;
    double longitude;
    double epx;
    double altitude;
    double epv;
    double track;
    double epd;
    double speed;
    double eps;
    double climb;
    double epc;
} cgps_data_t;

typedef struct {
    char *instance;
    label_set_t labels;
    plugin_filter_t *filter;

    char *host;
    char *port;
    cdtime_t timeout;
    cdtime_t pause_connect;

    struct gps_data_t gpsdata;
    cgps_data_t cgps_data;
    metric_family_t fams[FAM_GPSD_MAX];

    pthread_t thread_id;
    pthread_mutex_t data_lock;
    pthread_mutex_t thread_lock;
    pthread_cond_t thread_cond;
    volatile int thread_shutdown;
    volatile int thread_running;
} cgps_instance_t;

static void *cgps_thread(void *data)
{
    cgps_instance_t *cgps = data;
    unsigned int err_count;

    cgps->thread_running = true;

    while (true) {
        pthread_mutex_lock(&cgps->thread_lock);
        if (cgps->thread_shutdown)
            goto quit;
        pthread_mutex_unlock(&cgps->thread_lock);

        err_count = 0;

#if GPSD_API_MAJOR_VERSION > 4
        int status = gps_open(cgps->host, cgps->port, &cgps->gpsdata);
#else
        int status = gps_open_r(cgps->host, cgps->port, &cgps->gpsdata);

#endif
        if (status < 0) {
            PLUGIN_WARNING("connecting to %s:%s failed: %s",
                           cgps->host, cgps->port, gps_errstr(status));
            // Here we make a pause until a new tentative to connect, we check also if
            // the thread does not need to stop.
            cdtime_t until = cdtime() + cgps->pause_connect;

            pthread_mutex_lock(&cgps->thread_lock);
            pthread_cond_timedwait(&cgps->thread_cond,
                                   &cgps->thread_lock, &CDTIME_T_TO_TIMESPEC(until));
            if (cgps->thread_shutdown)
                goto quit;
            pthread_mutex_unlock(&cgps->thread_lock);
            continue;
        }

        gps_stream(&cgps->gpsdata, WATCH_ENABLE | WATCH_JSON | WATCH_NEWSTYLE, NULL);
        gps_send(&cgps->gpsdata, CGPS_CONFIG);

        while (true) {
            pthread_mutex_lock(&cgps->thread_lock);
            if (cgps->thread_shutdown)
                goto stop;
            pthread_mutex_unlock(&cgps->thread_lock);

#if GPSD_API_MAJOR_VERSION > 4
            long timeout_us = CDTIME_T_TO_US(cgps->timeout);
            if (!gps_waiting(&cgps->gpsdata, (int)timeout_us))
                continue;
#else
            if (!gps_waiting(&cgps->gpsdata))
                continue;
#endif

#if GPSD_API_MAJOR_VERSION > 6
            status = gps_read(&cgps->gpsdata, NULL, 0);
#else
            status = gps_read(&cgps->gpsdata);
#endif
            if (status == -1) {
                PLUGIN_WARNING("incorrect data! (err_count: %u)", err_count);
                err_count++;

                if (err_count > CGPS_MAX_ERROR) {
                    // Server is not responding ...
                    if (gps_send(&cgps->gpsdata, CGPS_CONFIG) == -1) {
                        PLUGIN_WARNING("gpsd seems to be down, reconnecting");
                        gps_close(&cgps->gpsdata);
                        break;
                    } else { // Server is responding ...
                        err_count = 0;
                    }
                }

                continue;
            }

            pthread_mutex_lock(&cgps->data_lock);

            cgps->cgps_data.satellites_used = cgps->gpsdata.satellites_used;
            cgps->cgps_data.satellites_visible= cgps->gpsdata.satellites_visible;
            cgps->cgps_data.hdop = cgps->gpsdata.dop.hdop;
            cgps->cgps_data.vdop = cgps->gpsdata.dop.vdop;
            cgps->cgps_data.pdop = cgps->gpsdata.dop.pdop;
            cgps->cgps_data.mode = cgps->gpsdata.fix.mode;

            if (cgps->cgps_data.satellites_used > 0) {
                cgps->cgps_data.latitude = cgps->gpsdata.fix.latitude;
                cgps->cgps_data.epy = cgps->gpsdata.fix.epy;
                cgps->cgps_data.longitude = cgps->gpsdata.fix.longitude;
                cgps->cgps_data.epx = cgps->gpsdata.fix.epx;
#if GPSD_API_MAJOR_VERSION >= 9
                cgps->cgps_data.altitude = cgps->gpsdata.fix.altMSL;
#else
                cgps->cgps_data.altitude = NAN;
#endif
                cgps->cgps_data.epv = cgps->gpsdata.fix.epv;
                cgps->cgps_data.track = cgps->gpsdata.fix.track;
                cgps->cgps_data.epd = cgps->gpsdata.fix.epd;
                cgps->cgps_data.speed = cgps->gpsdata.fix.speed;
                cgps->cgps_data.eps = cgps->gpsdata.fix.eps;
                cgps->cgps_data.climb = cgps->gpsdata.fix.climb;
                cgps->cgps_data.epc = cgps->gpsdata.fix.epc;
            } else {
                cgps->cgps_data.latitude = NAN;
                cgps->cgps_data.epy = NAN;
                cgps->cgps_data.longitude = NAN;
                cgps->cgps_data.epx = NAN;
                cgps->cgps_data.altitude =NAN;
                cgps->cgps_data.epv = NAN;
                cgps->cgps_data.track = NAN;
                cgps->cgps_data.epd = NAN;
                cgps->cgps_data.speed = NAN;
                cgps->cgps_data.eps = NAN;
                cgps->cgps_data.climb = NAN;
                cgps->cgps_data.epc = NAN;
            }

            pthread_mutex_unlock(&cgps->data_lock);
        }
    }

stop:
    PLUGIN_DEBUG("thread closing gpsd connection.");
    gps_stream(&cgps->gpsdata, WATCH_DISABLE, NULL);
    gps_close(&cgps->gpsdata);
quit:
    PLUGIN_DEBUG("thread shutting down.");
    cgps->thread_running = false;
    pthread_mutex_unlock(&cgps->thread_lock);
    pthread_exit(NULL);
}

static int cgps_read(user_data_t *user_data)
{
    cgps_instance_t *cgps = user_data->data;

    pthread_mutex_lock(&cgps->data_lock);
    cgps_data_t cgps_data = cgps->cgps_data;
    pthread_mutex_unlock(&cgps->data_lock);

    metric_t m = {0};
    if (cgps->instance != NULL)
        metric_label_set(&m, "instance", cgps->instance);

    for (size_t i = 0; i < cgps->labels.num; i++) {
        metric_label_set(&m, cgps->labels.ptr[i].name, cgps->labels.ptr[i].value);
    }

    metric_family_append(&cgps->fams[FAM_GPSD_SATELLITES_VISIBLE],
                         VALUE_GAUGE((double) cgps_data.satellites_visible), &cgps->labels, NULL);
    metric_family_append(&cgps->fams[FAM_GPSD_SATELLITES_USED],
                         VALUE_GAUGE((double) cgps_data.satellites_used), &cgps->labels, NULL);
    metric_family_append(&cgps->fams[FAM_GPSD_HDOP],
                         VALUE_GAUGE(cgps_data.hdop), &cgps->labels, NULL);
    metric_family_append(&cgps->fams[FAM_GPSD_VDOP],
                         VALUE_GAUGE(cgps_data.vdop), &cgps->labels, NULL);
    metric_family_append(&cgps->fams[FAM_GPSD_PDOP],
                         VALUE_GAUGE(cgps_data.pdop), &cgps->labels, NULL);

    enum {
        GPS_MODE_STATE_NO_FIX,
        GPS_MODE_STATE_2D_FIX,
        GPS_MODE_STATE_3D_FIX,
        GPS_MODE_STATE_MAX,
    };

    state_t gps_mode_state[GPS_MODE_STATE_MAX] = {
        [GPS_MODE_STATE_NO_FIX] = {.name="NO FIX", .enabled = false},
        [GPS_MODE_STATE_2D_FIX] = {.name="2D FIX", .enabled = false},
        [GPS_MODE_STATE_3D_FIX] = {.name="3D FIX", .enabled = false},
    };
    state_set_t set = {
        .num = GPS_MODE_STATE_MAX,
        .ptr = gps_mode_state,
    };

    switch(cgps_data.mode) {
    case MODE_2D:
        gps_mode_state[GPS_MODE_STATE_2D_FIX].enabled = true;
        break;
    case MODE_3D:
        gps_mode_state[GPS_MODE_STATE_2D_FIX].enabled = true;
        break;
    case MODE_NOT_SEEN:
    case MODE_NO_FIX:
    default:
        gps_mode_state[GPS_MODE_STATE_NO_FIX].enabled = true;
        break;
    }

    metric_family_append(&cgps->fams[FAM_GPSD_MODE], VALUE_STATE_SET(set), &cgps->labels, NULL);

    memset(&m.value, 0, sizeof(m.value));

    metric_family_append(&cgps->fams[FAM_GPSD_LATITUDE_DEGREES],
                         VALUE_GAUGE(cgps_data.latitude), &cgps->labels, NULL);
    metric_family_append(&cgps->fams[FAM_GPSD_EPY_METERS],
                         VALUE_GAUGE(cgps_data.epy), &cgps->labels, NULL);
    metric_family_append(&cgps->fams[FAM_GPSD_LONGITUDE_DEGREES],
                         VALUE_GAUGE(cgps_data.longitude), &cgps->labels, NULL);
    metric_family_append(&cgps->fams[FAM_GPSD_EPX_METERS],
                         VALUE_GAUGE(cgps_data.epx), &cgps->labels, NULL);
    metric_family_append(&cgps->fams[FAM_GPSD_ALTITUDE],
                         VALUE_GAUGE(cgps_data.altitude), &cgps->labels, NULL);
    metric_family_append(&cgps->fams[FAM_GPSD_EPV],
                         VALUE_GAUGE(cgps_data.epv), &cgps->labels, NULL);
    metric_family_append(&cgps->fams[FAM_GPSD_SPEED_METERS_PER_SECOND],
                         VALUE_GAUGE(cgps_data.speed), &cgps->labels, NULL);
    metric_family_append(&cgps->fams[FAM_GPSD_EPS_METERS_PER_SECOND],
                         VALUE_GAUGE(cgps_data.eps), &cgps->labels, NULL);
    metric_family_append(&cgps->fams[FAM_GPSD_CLIMB_METERS_PER_SECOND],
                         VALUE_GAUGE(cgps_data.climb), &cgps->labels, NULL);
    metric_family_append(&cgps->fams[FAM_GPSD_EPC_METERS_PER_SECOND],
                         VALUE_GAUGE(cgps_data.epc), &cgps->labels, NULL);

    plugin_dispatch_metric_family_array_filtered(cgps->fams, FAM_GPSD_MAX, cgps->filter, 0);

    metric_reset(&m, METRIC_TYPE_GAUGE);

    return 0;
}

static void cgps_free(void *data)
{
    if (data == NULL)
        return;

    cgps_instance_t *cgps = data;

    if (cgps->thread_running) {
        cgps->thread_shutdown = true;
        pthread_cond_broadcast(&cgps->thread_cond);
        void *res;
        pthread_join(cgps->thread_id, &res);
        free(res);
    }

    pthread_mutex_lock(&cgps->thread_lock);
    pthread_mutex_unlock(&cgps->thread_lock);
    pthread_mutex_destroy(&cgps->thread_lock);

    pthread_mutex_lock(&cgps->data_lock);
    pthread_mutex_unlock(&cgps->data_lock);
    pthread_mutex_destroy(&cgps->data_lock);

    free(cgps->port);
    free(cgps->host);
    label_set_reset(&cgps->labels);
    plugin_filter_free(cgps->filter);
    free(cgps);
}

static int cgps_config_instance(config_item_t *ci)
{
    cgps_instance_t *cgps = calloc(1, sizeof(*cgps));
    if (cgps == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &cgps->instance);
    if (status != 0) {
        free(cgps);
        return -1;
    }

    pthread_mutex_init(&cgps->data_lock, NULL);
    pthread_mutex_init(&cgps->thread_lock, NULL);
    pthread_cond_init(&cgps->thread_cond, NULL);

    cgps->thread_shutdown = false;
    cgps->thread_running = false;
    cgps->timeout = CGPS_DEFAULT_TIMEOUT;
    cgps->pause_connect = CGPS_DEFAULT_PAUSE_CONNECT;

    memcpy(cgps->fams, fams, FAM_GPSD_MAX * sizeof(cgps->fams[0]));

    cgps->cgps_data.satellites_used = 0;
    cgps->cgps_data.satellites_visible= 0;
    cgps->cgps_data.hdop = NAN;
    cgps->cgps_data.vdop = NAN;
    cgps->cgps_data.pdop = NAN;
    cgps->cgps_data.mode = 0;
    cgps->cgps_data.latitude = NAN;
    cgps->cgps_data.epy = NAN;
    cgps->cgps_data.longitude = NAN;
    cgps->cgps_data.epx = NAN;
    cgps->cgps_data.altitude =NAN;
    cgps->cgps_data.epv = NAN;
    cgps->cgps_data.track = NAN;
    cgps->cgps_data.epd = NAN;
    cgps->cgps_data.speed = NAN;
    cgps->cgps_data.eps = NAN;
    cgps->cgps_data.climb = NAN;
    cgps->cgps_data.epc = NAN;

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &cgps->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_service(child, &cgps->port);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &cgps->timeout);
        } else if (strcasecmp("pause-connect", child->key) == 0) {
            status = cf_util_get_cdtime(child, &cgps->pause_connect);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &cgps->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &cgps->filter);
        } else {
            PLUGIN_WARNING("Option `%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        cgps_free(cgps);
        return -1;
    }

    // Controlling the value for timeout:
    // If set too high it blocks the reading (> 5 s), too low it gets not reading
    // (< 500 us).
    // To avoid any issues we replace "out of range" value by the default value.
    if (cgps->timeout > TIME_T_TO_CDTIME_T(5) ||
            cgps->timeout < US_TO_CDTIME_T(500)) {
        PLUGIN_WARNING("timeout set to %.6f sec. setting to default (%.6f).",
                       CDTIME_T_TO_DOUBLE(cgps->timeout), CDTIME_T_TO_DOUBLE(CGPS_DEFAULT_TIMEOUT));
        cgps->timeout = CGPS_DEFAULT_TIMEOUT;
    }

    if (cgps->host == NULL) {
        cgps->host = sstrdup(CGPS_DEFAULT_HOST);
        if (cgps->host == NULL) {
            cgps_free(cgps);
            return -1;
        }
    }
    if (cgps->port == NULL) {
        cgps->port = sstrdup(CGPS_DEFAULT_PORT);
        if (cgps->port == NULL) {
            cgps_free(cgps);
            return -1;
        }
    }

    label_set_add(&cgps->labels, true, "instance", cgps->instance);

    PLUGIN_DEBUG("config{host: \"%s\", port: \"%s\", timeout: %.6f sec., pause connect: %.3f sec.}",
                 cgps->host, cgps->port,
                 CDTIME_T_TO_DOUBLE(cgps->timeout), CDTIME_T_TO_DOUBLE(cgps->pause_connect));

    status = plugin_thread_create(&cgps->thread_id, cgps_thread, cgps, "gps");
    if (status != 0) {
        PLUGIN_ERROR("pthread_create() failed.");
        cgps_free(cgps);
        return -1;
    }

    return plugin_register_complex_read("gps", cgps->instance, cgps_read, interval,
                                        &(user_data_t){.data = cgps, .free_func = cgps_free});
}

static int cgps_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = cgps_config_instance(child);
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
    plugin_register_config("gps", cgps_config);
}
