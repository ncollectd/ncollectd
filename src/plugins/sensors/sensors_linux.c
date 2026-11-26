// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2008 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2006 Luboš Staněk
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Lubos Stanek <lubek at users.sourceforge.net> Wed Oct 27, 2006
// SPDX-FileContributor: Henrique de Moraes Holschuh <hmh at debian.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <sensors/sensors.h>

#include "sensors.h"

#if !defined(SENSORS_API_VERSION)
#define SENSORS_API_VERSION 0x000
#endif

#if SENSORS_API_VERSION < 0x400
#error "Invalid libsensors API version."
#endif

typedef struct featurelist {
    const sensors_chip_name *chip;
    const sensors_feature *feature;
    const sensors_subfeature *subfeature;
    struct featurelist *next;
} featurelist_t;

static featurelist_t *first_feature;
static featurelist_t *last_feature;

static bool sensors_loaded;
static complain_t sensors_complaint;

static void ncsensors_free_features(void)
{
    featurelist_t *nextft;

    if (first_feature == NULL)
        return;

    sensors_cleanup();
    sensors_loaded = false;

    for (featurelist_t *thisft = first_feature; thisft != NULL; thisft = nextft) {
        nextft = thisft->next;
        free(thisft);
    }
    first_feature = NULL;
}

static int ncsensors_load_conf(void)
{
    if (sensors_loaded)
        return 0;

    FILE *fh = NULL;
    if (sensors_conffile != NULL) {
        fh = fopen(sensors_conffile, "r");
        if (fh == NULL) {
            PLUGIN_COMPLAIN_ERROR(&sensors_complaint, "fopen(%s) failed: %s",
                                  sensors_conffile, STRERRNO);
            return -1;
        }
    }

    int status = sensors_init(fh);
    if (fh)
        fclose(fh);

    if (status != 0) {
        PLUGIN_COMPLAIN_ERROR(&sensors_complaint,
                              "Cannot initialize sensors. Data will not be collected.");
        return -1;
    }

    first_feature = NULL;
    last_feature = NULL;
    const sensors_chip_name *chip;
    int chip_num = 0;
    while ((chip = sensors_get_detected_chips(NULL, &chip_num)) != NULL) {
        const sensors_feature *feature;
        int feature_num = 0;

        while ((feature = sensors_get_features(chip, &feature_num)) != NULL) {
            const sensors_subfeature *subfeature;
            int subfeature_num = 0;

            /* Only handle voltage, fanspeeds and temperatures */
            if ((feature->type != SENSORS_FEATURE_IN) &&
                (feature->type != SENSORS_FEATURE_FAN) &&
#if SENSORS_API_VERSION >= 0x401
                (feature->type != SENSORS_FEATURE_POWER) &&
#endif
#if SENSORS_API_VERSION >= 0x410
                (feature->type != SENSORS_FEATURE_CURR) &&
#endif
#if SENSORS_API_VERSION >= 0x431
                (feature->type != SENSORS_FEATURE_HUMIDITY) &&
                (feature->type != SENSORS_FEATURE_INTRUSION) &&
#endif
                (feature->type != SENSORS_FEATURE_TEMP)
            ) {
                PLUGIN_DEBUG("Ignoring feature `%s', "
                             "because its type is not supported.", feature->name);
                continue;
            }

            while ((subfeature = sensors_get_all_subfeatures(
                                 chip, feature, &subfeature_num)) != NULL) {
                if ((subfeature->type != SENSORS_SUBFEATURE_IN_INPUT) &&
                    (subfeature->type != SENSORS_SUBFEATURE_FAN_INPUT) &&
#if SENSORS_API_VERSION >= 0x401
                    (subfeature->type != SENSORS_SUBFEATURE_POWER_AVERAGE) &&
#endif
#if SENSORS_API_VERSION >= 0x410
                    (subfeature->type != SENSORS_SUBFEATURE_POWER_INPUT) &&
                    (subfeature->type != SENSORS_SUBFEATURE_CURR_INPUT) &&
#endif
#if SENSORS_API_VERSION >= 0x431
                    (subfeature->type != SENSORS_SUBFEATURE_HUMIDITY_INPUT) &&
                    (subfeature->type != SENSORS_SUBFEATURE_INTRUSION_ALARM) &&
#endif
                    (subfeature->type != SENSORS_SUBFEATURE_TEMP_INPUT)
                    )
                    continue;

                featurelist_t *fl = calloc(1, sizeof(*fl));
                if (fl == NULL) {
                    PLUGIN_ERROR("calloc failed.");
                    continue;
                }

                fl->chip = chip;
                fl->feature = feature;
                fl->subfeature = subfeature;

                if (first_feature == NULL)
                    first_feature = fl;

                if (last_feature != NULL)
                    last_feature->next = fl;
                last_feature = fl;
            }
        }
    }

    if (first_feature == NULL) {
        sensors_cleanup();
        PLUGIN_COMPLAIN_INFO(&sensors_complaint,
                             "lm_sensors reports no features. Data will not be collected.");
        return 1;
    }

    PLUGIN_RELEASE_INFO(&sensors_complaint, "sensors initialized.");
    sensors_loaded = true;

    return 0;
}

int ncsensors_read(void)
{
    int load_status = ncsensors_load_conf();
    if (load_status < 0)
        return -1;
    if (load_status > 0)
        return 0;

    for (featurelist_t *fl = first_feature; fl != NULL; fl = fl->next) {
        double value;

        int status = sensors_get_value(fl->chip, fl->subfeature->number, &value);
        if (status < 0)
            continue;

        char chip[DATA_MAX_NAME_LEN];
        status = sensors_snprintf_chip_name(chip, sizeof(chip), fl->chip);
        if (status < 0)
            continue;

        metric_family_t *fam = NULL;
        if (fl->feature->type == SENSORS_FEATURE_IN) {
            fam = &sensors_fams[FAM_SENSOR_VOLTAGE_VOLTS];
        } else if (fl->feature->type == SENSORS_FEATURE_FAN) {
            fam = &sensors_fams[FAM_SENSOR_FAN_SPEED_RPM];
        } else if (fl->feature->type == SENSORS_FEATURE_TEMP) {
            fam = &sensors_fams[FAM_SENSOR_TEMPERATURE_CELSIUS];
#if SENSORS_API_VERSION >= 0x401
        } else if (fl->feature->type == SENSORS_FEATURE_POWER) {
#if SENSORS_API_VERSION >= 0x410
            if (fl->subfeature->type == SENSORS_SUBFEATURE_POWER_INPUT) {
                fam = &sensors_fams[FAM_SENSOR_POWER_WATTS];
            }
#endif
            if (fl->subfeature->type == SENSORS_SUBFEATURE_POWER_AVERAGE) {
                fam = &sensors_fams[FAM_SENSOR_POWER_AVERAGE_WATTS];
            }
#endif
#if SENSORS_API_VERSION >= 0x410
        } else if (fl->feature->type == SENSORS_FEATURE_CURR) {
            fam = &sensors_fams[FAM_SENSOR_CURRENT_AMPS];
#endif
#if SENSORS_API_VERSION >= 0x431
        } else if (fl->feature->type == SENSORS_FEATURE_HUMIDITY) {
            fam = &sensors_fams[FAM_SENSOR_HUMIDITY];
            value = value / 100.0;
        } else if (fl->feature->type == SENSORS_FEATURE_INTRUSION) {
            fam = &sensors_fams[FAM_SENSOR_INTRUSION_ALARM];
#endif
        }

        if (fam == NULL)
            continue;

        metric_t tmpl = {0};
        tmpl.value = VALUE_GAUGE(value);
        metric_label_set(&tmpl, "chip", chip);

        if (sensors_use_labels) {
            char *sensor_label = sensors_get_label(fl->chip, fl->feature);
            metric_label_set(&tmpl, "name", sensor_label != NULL ? sensor_label : fl->feature->name);
            free(sensor_label);
        } else {
            metric_label_set(&tmpl, "name", fl->feature->name);
        }

        const char *adapter = sensors_get_adapter_name(&fl->chip->bus);
        if (adapter != NULL)
            metric_label_set(&tmpl, "adapter", adapter);

        metric_family_metric_append(fam, tmpl);
        metric_reset(&tmpl, METRIC_TYPE_GAUGE);
    }

    plugin_dispatch_metric_family_array_filtered(sensors_fams, FAM_SENSOR_MAX, sensors_filter, 0);

    return 0;
}

int ncsensors_shutdown(void)
{
    ncsensors_free_features();
    plugin_filter_free(sensors_filter);
    sensors_filter = NULL;
    free(sensors_conffile);
    sensors_conffile = NULL;

    return 0;
}
