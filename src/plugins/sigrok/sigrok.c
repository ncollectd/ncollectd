// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2013 Bert Vermeulen
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Bert Vermeulen <bert at biot.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <glib.h>
#include <libsigrok/libsigrok.h>

/* Minimum interval between dispatches coming from this plugin. The RRD
 * plugin, at least, complains when written to with sub-second intervals.*/
#define DEFAULT_MIN_DISPATCH_INTERVAL TIME_T_TO_CDTIME_T(0)

static pthread_t sr_thread;
static int sr_thread_running = FALSE;
GSList *config_devices;
static int num_devices;
static int loglevel = SR_LOG_WARN;
static struct sr_context *sr_ctx;

struct config_device {
    char *name;
    char *metric_prefix;
    label_set_t labels;
    char *driver;
    char *conn;
    char *serialcomm;
    size_t channel_num;
    char **channel;
    struct sr_dev_inst *sdi;
    cdtime_t min_dispatch_interval;
    cdtime_t last_dispatch;
};

static int sigrok_log_callback(void *cb_data __attribute__((unused)),
                               int msg_loglevel, const char *format, va_list args)
{
    char s[512];

    if (msg_loglevel <= loglevel) {
        vsnprintf(s, 512, format, args);
        PLUGIN_INFO("%s", s);
    }

    return 0;
}

static const char *sigrok_type(const struct sr_datafeed_analog *analog)
{
    switch (analog->meaning->mq) {
    case SR_MQ_VOLTAGE:
        return "voltage";
    case SR_MQ_CURRENT:
        return "current";
    case SR_MQ_RESISTANCE:
        return "resistance";
    case SR_MQ_CAPACITANCE:
        return "capacitance";
    case SR_MQ_TEMPERATURE:
        return "temperature";
    case SR_MQ_FREQUENCY:
        return "frequency";
    case SR_MQ_DUTY_CYCLE:
        return "duty_cycle";
    case SR_MQ_CONTINUITY:
        return "continuity";
    case SR_MQ_PULSE_WIDTH:
        return "pulse_witdh";
    case SR_MQ_CONDUCTANCE:
        return "conductance";
    case SR_MQ_POWER:
        return "power";
    case SR_MQ_GAIN:
        return "gain";
    case SR_MQ_SOUND_PRESSURE_LEVEL:
        return "sound_pressure_level";
    case SR_MQ_CARBON_MONOXIDE:
        return "carbon_monoxide";
    case SR_MQ_RELATIVE_HUMIDITY:
        return "relative_humidity";
    case SR_MQ_TIME:
        return "time";
    case SR_MQ_WIND_SPEED:
        return "wind_speed";
    case SR_MQ_PRESSURE:
        return "pressure";
    case SR_MQ_PARALLEL_INDUCTANCE:
        return "parallel_inductance";
    case SR_MQ_PARALLEL_CAPACITANCE:
        return "parallel_capacitance";
    case SR_MQ_PARALLEL_RESISTANCE:
        return "parallel_resistance";
    case SR_MQ_SERIES_INDUCTANCE:
        return "series_inductance";
    case SR_MQ_SERIES_CAPACITANCE:
        return "series_capacitance";
    case SR_MQ_SERIES_RESISTANCE:
        return "series_resistance";
    case SR_MQ_DISSIPATION_FACTOR:
        return "dissipation_factor";
    case SR_MQ_QUALITY_FACTOR:
        return "quality_factor";
    case SR_MQ_PHASE_ANGLE:
        return "phase_angle";
    case SR_MQ_DIFFERENCE:
        return "difference";
    case SR_MQ_COUNT:
        return "count";
    case SR_MQ_POWER_FACTOR:
        return "power_factor";
    case SR_MQ_APPARENT_POWER:
        return "apparent_power";
    case SR_MQ_MASS:
        return "mass";
    case SR_MQ_HARMONIC_RATIO:
        return "harmonic_ratio";
    default:
        return NULL;
    }
}

static const char *sigrok_units(const struct sr_datafeed_analog *analog)
{
    switch (analog->meaning->unit) {
    case SR_UNIT_VOLT:
        return "_volts";
    case SR_UNIT_AMPERE:
        return "_amps";
    case SR_UNIT_OHM:
        return "_ohms";
    case SR_UNIT_FARAD:
        return "_farads";
    case SR_UNIT_KELVIN:
        return "_kelvin";
    case SR_UNIT_CELSIUS:
        return "_celsius";
    case SR_UNIT_FAHRENHEIT:
        return "_fahrenheit";
    case SR_UNIT_HERTZ:
        return "_hertz";
    case SR_UNIT_PERCENTAGE:
        return "_percentage";
    case SR_UNIT_BOOLEAN:
        return "_boolean";
    case SR_UNIT_SECOND:
        return "_seconds";
    case SR_UNIT_SIEMENS:
        return "_siemens";
    case SR_UNIT_DECIBEL_MW:
        return "_decibels_milliwatts";
    case SR_UNIT_DECIBEL_VOLT:
        return "_decibels_volts";
    case SR_UNIT_UNITLESS:
        return NULL;
    case SR_UNIT_DECIBEL_SPL:
        return "_sound_presure_level";
    case SR_UNIT_CONCENTRATION:
        return "_concentration";
    case SR_UNIT_REVOLUTIONS_PER_MINUTE:
        return "_revolutions_per_minute";
    case SR_UNIT_VOLT_AMPERE:
        return "_volts_amps";
    case SR_UNIT_WATT:
        return "_watts";
    case SR_UNIT_WATT_HOUR:
        return "_watts_per_hour";
    case SR_UNIT_METER_SECOND:
        return "_meters_per_second";
    case SR_UNIT_HECTOPASCAL:
        return "_hectopascals";
    case SR_UNIT_HUMIDITY_293K:
        return "_relative_humidity_293K";
    case SR_UNIT_DEGREE:
        return "_degrees";
    case SR_UNIT_HENRY:
        return "_henries";
    case SR_UNIT_GRAM:
        return "_grams";
    case SR_UNIT_CARAT:
        return "_carats";
    case SR_UNIT_OUNCE:
        return "_ounces";
    case SR_UNIT_TROY_OUNCE:
        return "_troy_ounces";
    case SR_UNIT_POUND:
        return "_pounds";
    case SR_UNIT_PENNYWEIGHT:
        return "_pennyweights";
    case SR_UNIT_GRAIN:
        return "_grains";
    case SR_UNIT_TAEL:
        return "_taels";
    case SR_UNIT_MOMME:
        return "_mommes";
    case SR_UNIT_TOLA:
        return "_tolas";
    case SR_UNIT_PIECE:
        return "_pieces";
    default:
        return NULL;
    }
}

static void sigrok_feed_callback(const struct sr_dev_inst *sdi,
                                 const struct sr_datafeed_packet *packet,
                                 __attribute__((unused)) void *cb_data)
{
    const struct sr_datafeed_analog *analog;
    struct config_device *cfdev;

    /* Find this device's configuration. */
    cfdev = NULL;
    for (GSList *l = config_devices; l; l = l->next) {
        cfdev = l->data;
        if (cfdev->sdi == sdi) {
            /* Found it. */
            break;
        }
        cfdev = NULL;
    }

    if (!cfdev) {
        struct sr_dev_driver *driver = sr_dev_inst_driver_get(sdi);
        const char *name = driver == NULL ? "" : driver->name;
        PLUGIN_ERROR("Received data from driver '%s' but "
                     "can't find a configuration / device matching it.", name);
        return;
    }

    if (packet->type == SR_DF_END) {
        /* TODO: try to restart acquisition after a delay? */
        PLUGIN_WARNING("acquisition for \"%s\" ended.", cfdev->name);
        return;
    }

    if (packet->type != SR_DF_ANALOG)
        return;

    if ((cfdev->min_dispatch_interval != 0) &&
            ((cdtime() - cfdev->last_dispatch) < cfdev->min_dispatch_interval))
        return;

    metric_t m = {0};

    /* Ignore all but the first sample on the first probe. */
    analog = packet->payload;
    if (analog == NULL)
        return;
    if (analog->meaning == NULL)
        return;

    if ((analog->meaning->channels != NULL) && (g_slist_length(analog->meaning->channels) > 0)) {
        struct sr_channel *channel = g_slist_nth_data(analog->meaning->channels, 0);
        if (channel != NULL) {
            if (cfdev->channel_num > 0) {
                bool channel_found = false;
                for (size_t i = 0; i < cfdev->channel_num; i++) {
                    if (strcasecmp(cfdev->channel[i], channel->name) == 0) {
                        channel_found = true;
                        break;
                    }
                }
                if (!channel_found)
                    return;
            }
            metric_label_set(&m, "channel", channel->name);
        }
    }

    if (analog->num_samples > 1) {
        float *data = malloc(analog->num_samples * sizeof(float));
        if (data == NULL) {
            PLUGIN_ERROR("malloc failed.");
            return;
        }
        if (sr_analog_to_float(analog, data) != SR_OK) {
            free(data);
            PLUGIN_ERROR("sr_analog_to_float failed.");
            return;
        }
        m.value = VALUE_GAUGE(data[0]);
        free(data);
    } else {
        float data;
        if (sr_analog_to_float(analog, &data) != SR_OK) {
            PLUGIN_ERROR("sr_analog_to_float failed.");
            return;
        }
        m.value = VALUE_GAUGE(data);
    }

    strbuf_t buf = STRBUF_CREATE;

    metric_family_t fam = {0};
    fam.type = METRIC_TYPE_GAUGE;

    if (cfdev->metric_prefix != NULL)
        strbuf_print(&buf, cfdev->metric_prefix);
    else
        strbuf_print(&buf, "sigrok_");

    const char *type = sigrok_type(analog);
    if (type != NULL)
        strbuf_print(&buf, type);

    const char *units = sigrok_units(analog);
    if (units != NULL)
        strbuf_print(&buf, units);

    fam.name = buf.ptr;

    metric_label_set(&m, "device", cfdev->name);

    if ((analog->meaning->mqflags & SR_MQFLAG_AC) && (analog->meaning->mqflags & SR_MQFLAG_DC))
        metric_label_set(&m, "voltage", "DC+AC");
    if ((analog->meaning->mqflags & SR_MQFLAG_AC) && !(analog->meaning->mqflags & SR_MQFLAG_DC))
        metric_label_set(&m, "voltage", "AC");
    if ((analog->meaning->mqflags & SR_MQFLAG_DC) && !(analog->meaning->mqflags & SR_MQFLAG_AC))
        metric_label_set(&m, "voltage", "DC");

    if (analog->meaning->mqflags & SR_MQFLAG_RMS)
        metric_label_set(&m, "rms", "true");
    if (analog->meaning->mqflags & SR_MQFLAG_DIODE)
        metric_label_set(&m, "diode", "on");
    if (analog->meaning->mqflags & SR_MQFLAG_HOLD)
        metric_label_set(&m, "hold", "on");
    if (analog->meaning->mqflags & SR_MQFLAG_MAX)
        metric_label_set(&m, "max", "on");
    if (analog->meaning->mqflags & SR_MQFLAG_MIN)
        metric_label_set(&m, "min", "on");
    if (analog->meaning->mqflags & SR_MQFLAG_AUTORANGE)
        metric_label_set(&m, "autorange", "on");
    if (analog->meaning->mqflags & SR_MQFLAG_RELATIVE)
        metric_label_set(&m, "relative", "on");
    if (analog->meaning->mqflags & SR_MQFLAG_AVG)
        metric_label_set(&m, "avg", "on");
    if (analog->meaning->mqflags & SR_MQFLAG_REFERENCE)
        metric_label_set(&m, "reference", "on");
    if (analog->meaning->mqflags & SR_MQFLAG_FOUR_WIRE)
        metric_label_set(&m, "four_wires", "true");
    if (analog->meaning->mqflags & SR_MQFLAG_UNSTABLE)
        metric_label_set(&m, "unstable", "true");

    for (size_t i = 0; i < cfdev->labels.num; i++) {
        metric_label_set(&m, cfdev->labels.ptr[i].name, cfdev->labels.ptr[i].value);
    }

    metric_family_metric_append(&fam, m);
    metric_reset(&m, fam.type);

    plugin_dispatch_metric_family(&fam, 0);
    cfdev->last_dispatch = cdtime();
    strbuf_destroy(&buf);
}

static void sigrok_free_drvopts(struct sr_config *src)
{
    g_variant_unref(src->data);
    g_free(src);
}

static int sigrok_init_driver(struct sr_session *sr_sess, struct config_device *cfdev,
                              struct sr_dev_driver *drv)
{
    struct sr_config *src;
    GSList *devlist, *drvopts;
    char hwident[512];

    if (sr_driver_init(sr_ctx, drv) != SR_OK)
        /* Error was logged by libsigrok. */
        return -1;

    drvopts = NULL;
    if (cfdev->conn) {
        if (!(src = malloc(sizeof(*src))))
            return -1;
        src->key = SR_CONF_CONN;
        src->data = g_variant_new_string(cfdev->conn);
        drvopts = g_slist_append(drvopts, src);
    }
    if (cfdev->serialcomm) {
        if (!(src = malloc(sizeof(*src))))
            return -1;
        src->key = SR_CONF_SERIALCOMM;
        src->data = g_variant_new_string(cfdev->serialcomm);
        drvopts = g_slist_append(drvopts, src);
    }
    devlist = sr_driver_scan(drv, drvopts);
    g_slist_free_full(drvopts, (GDestroyNotify)sigrok_free_drvopts);
    if (!devlist) {
        /* Not an error, but the user should know about it. */
        PLUGIN_WARNING("No device found for \"%s\".", cfdev->name);
        return 0;
    }

    if (g_slist_length(devlist) > 1) {
        PLUGIN_INFO("%u sigrok devices for device entry \"%s\": must be 1.",
                    g_slist_length(devlist), cfdev->name);
        return -1;
    }
    cfdev->sdi = devlist->data;
    g_slist_free(devlist);

    const char *vendor = sr_dev_inst_vendor_get(cfdev->sdi);
    const char *model = sr_dev_inst_model_get(cfdev->sdi);
    const char *version = sr_dev_inst_version_get(cfdev->sdi);

    ssnprintf(hwident, sizeof(hwident), "%s %s %s", vendor ? vendor : "",
                        model ? model : "", version ? version : "");
    PLUGIN_INFO("Device \"%s\" is a %s", cfdev->name, hwident);

    if (sr_dev_open(cfdev->sdi) != SR_OK)
        return -1;

    if (sr_session_dev_add(sr_sess, cfdev->sdi) != SR_OK)
        return -1;

    return 1;
}

static void *sigrok_read_thread(void *arg __attribute__((unused)))
{
    struct sr_dev_driver *drv, **drvlist;
    struct sr_session *sr_sess = NULL;
    GSList *l;
    struct config_device *cfdev;
    int ret, i;

    sr_log_callback_set(sigrok_log_callback, NULL);
    sr_log_loglevel_set(loglevel);

    if ((ret = sr_init(&sr_ctx)) != SR_OK) {
        PLUGIN_ERROR("Failed to initialize libsigrok: %s.", sr_strerror(ret));
        return NULL;
    }

    if ((ret = sr_session_new(sr_ctx, &sr_sess)) != SR_OK) {
        PLUGIN_ERROR("Failed to create session: %s.", sr_strerror(ret));
        return NULL;
    }

    num_devices = 0;
    drvlist = sr_driver_list(sr_ctx);
    for (l = config_devices; l; l = l->next) {
        cfdev = l->data;
        drv = NULL;
        for (i = 0; drvlist[i]; i++) {
            if (!strcmp(drvlist[i]->name, cfdev->driver)) {
                drv = drvlist[i];
                break;
            }
        }

        if (!drv) {
            PLUGIN_ERROR("Unknown driver \"%s\".", cfdev->driver);
            return NULL;
        }

        if ((ret = sigrok_init_driver(sr_sess, cfdev, drv)) < 0)
            /* Error was already logged. */
            return NULL;

        num_devices += ret;
    }

    if (num_devices > 0) {
        /* Do this only when we're sure there's hardware to talk to. */
        if (sr_session_datafeed_callback_add(sr_sess, sigrok_feed_callback, NULL) != SR_OK)
            return NULL;

        /* Start acquisition on all devices. */
        if (sr_session_start(sr_sess) != SR_OK)
            return NULL;

        /* Main loop, runs forever. */
        sr_session_run(sr_sess);

        sr_session_stop(sr_sess);
        sr_session_dev_remove_all(sr_sess);
    }

    sr_session_destroy(sr_sess);

    sr_exit(sr_ctx);

    pthread_exit(NULL);
    sr_thread_running = FALSE;

    return NULL;
}

static int sigrok_config_channel(config_item_t *ci, struct config_device *cfdev)
{
    char *channel = NULL;

    int status = cf_util_get_string(ci, &channel);
    if (status != 0)
        return -1;

    char **tmp = realloc(cfdev->channel, sizeof(char *) * (cfdev->channel_num + 1));
    if (tmp == NULL) {
        PLUGIN_ERROR("recalloc failed.");
        free(channel);
        return -1;
    }

    cfdev->channel = tmp;
    cfdev->channel[cfdev->channel_num] = channel;
    cfdev->channel_num++;

    return 0;
}

static void sigrok_device_free(struct config_device *cfdev)
{
    if (cfdev == NULL)
        return;

    free(cfdev->name);
    free(cfdev->metric_prefix);
    label_set_reset(&cfdev->labels);
    free(cfdev->driver);
    free(cfdev->conn);
    free(cfdev->serialcomm);
    for (size_t i = 0; i < cfdev->channel_num; i++) {
        free(cfdev->channel[i]);
    }
    free(cfdev->channel);

    free(cfdev);
}

static int sigrok_config_device(config_item_t *ci)
{
    struct config_device *cfdev = calloc(1, sizeof(*cfdev));
    if (cfdev == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }
    if (cf_util_get_string(ci, &cfdev->name)) {
        free(cfdev);
        PLUGIN_WARNING("Invalid device name.");
        return -1;
    }
    cfdev->min_dispatch_interval = DEFAULT_MIN_DISPATCH_INTERVAL;

    int status = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *item = ci->children + i;
        if (!strcasecmp(item->key, "driver")) {
            status = cf_util_get_string(item, &cfdev->driver);
        } else if (!strcasecmp(item->key, "conn")) {
            status = cf_util_get_string(item, &cfdev->conn);
        } else if (!strcasecmp(item->key, "serial-comm")) {
            status = cf_util_get_string(item, &cfdev->serialcomm);
        } else if (!strcasecmp(item->key, "minimum-interval")) {
            status = cf_util_get_cdtime(item, &cfdev->min_dispatch_interval);
        } else if (strcasecmp(item->key, "metric-prefix") == 0) {
            status = cf_util_get_string(item, &cfdev->metric_prefix);
        } else if (strcasecmp(item->key, "channel") == 0) {
            status = sigrok_config_channel(item, cfdev);
        } else if (strcasecmp(item->key, "label") == 0) {
            status = cf_util_get_label(item, &cfdev->labels);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          item->key, cf_get_file(item), cf_get_lineno(item));
            status = -1;
        }

        if (status != 0) {
            sigrok_device_free(cfdev);
            return -1;
        }
    }

    config_devices = g_slist_append(config_devices, cfdev);

    return 0;
}

static int sigrok_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *item = ci->children + i;
        if (strcasecmp("log-level", item->key) == 0) {
            int tmp = -1;
            status = cf_util_get_int(item, &tmp);
            if (status != 0) {
                continue;
            } else if ((tmp < 0) || (tmp > 5)) {
                PLUGIN_ERROR("The 'log-level' configuration option in %s:%d expects "
                             "an integer between 0 and 5 (inclusive); you provided %i.",
                             cf_get_file(item), cf_get_lineno(item), tmp);
                status = -1;
            }
            loglevel = tmp;
        } else if (!strcasecmp(item->key, "device")) {
            status = sigrok_config_device(item);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         item->key, cf_get_file(item), cf_get_lineno(item));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int sigrok_init(void)
{
    if (sr_thread_running) {
        PLUGIN_ERROR("Thread already running.");
        return -1;
    }

    int status = plugin_thread_create(&sr_thread, sigrok_read_thread, NULL, "sigrok read");
    if (status != 0) {
        PLUGIN_ERROR("Failed to create thread: %s.", STRERRNO);
        return -1;
    }
    sr_thread_running = TRUE;

    return 0;
}

static int sigrok_shutdown(void)
{
    if (sr_thread_running) {
        pthread_cancel(sr_thread);
        pthread_join(sr_thread, NULL);
    }

    for (GSList *l = config_devices; l; l = l->next) {
        sigrok_device_free(l->data);
    }
    g_slist_free(config_devices);

    return 0;
}

void module_register(void)
{
    plugin_register_config("sigrok", sigrok_config);
    plugin_register_init("sigrok", sigrok_init);
    plugin_register_shutdown("sigrok", sigrok_shutdown);
}
