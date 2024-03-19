// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2007 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Pavel Rochnyak <pavel2000 ngs.ru>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <upsclient.h>

#ifdef HAVE_UPSCONN_T
typedef UPSCONN_t ncollectd_upsconn_t;
#elif defined(HAVE_UPSCONN)
typedef UPSCONN ncollectd_upsconn_t;
#else
#error "Unable to determine the UPS connection type."
#endif

enum {
    FAM_UPS_POWER_VOLTS_AMPS,
    FAM_UPS_REALPOWER_WATTS,
    FAM_UPS_TEMPERATURE_CELSIUS,
    FAM_UPS_LOAD_RATIO,
    FAM_UPS_INPUT_VOLTAGE_VOLTS,
    FAM_UPS_INPUT_CURRENT_AMPS,
    FAM_UPS_INPUT_FREQUENCY_HZ,
    FAM_UPS_INPUT_LOAD_RATIO,
    FAM_UPS_INPUT_REALPOWER_WATTS,
    FAM_UPS_INPUT_POWER_VOLTS_AMPS,
    FAM_UPS_OUTPUT_VOLTAGE_VOLTS,
    FAM_UPS_OUTPUT_CURRENT_AMPS,
    FAM_UPS_OUTPUT_FREQUENCY_HZ,
    FAM_UPS_OUTPUT_REALPOWER_WATTS,
    FAM_UPS_OUTPUT_POWER_VOLTS_AMPS,
    FAM_UPS_BATTERY_CHARGE_RATIO,
    FAM_UPS_BATTERY_VOLTAGE_VOLTS,
    FAM_UPS_BATTERY_CAPACITY_AMPS_HOUR,
    FAM_UPS_BATTERY_CURRENT_AMPS,
    FAM_UPS_BATTERY_TEMPERATURE_CELSIUS,
    FAM_UPS_BATTERY_RUNTIME_SECONDS,
    FAM_UPS_AMBIENT_TEMPERATURE_CELSIUS,
    FAM_UPS_AMBIENT_HUMIDITY_RATIO,
    FAM_UPS_MAX,
};

static metric_family_t fams[FAM_UPS_MAX] = {
    [FAM_UPS_POWER_VOLTS_AMPS] = {
        .name = "ups_power_volts_amps",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current value of apparent power (Volt-Amps)",
    },
    [FAM_UPS_REALPOWER_WATTS] = {
        .name = "ups_realpower_watts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current value of real power (Watts)",
    },
    [FAM_UPS_TEMPERATURE_CELSIUS] = {
        .name = "ups_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "UPS temperature (degrees C)",
    },
    [FAM_UPS_LOAD_RATIO] = {
        .name = "ups_load_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Load on UPS (percent)",
    },
    [FAM_UPS_INPUT_VOLTAGE_VOLTS] = {
        .name = "ups_input_voltage_volts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Input voltage (V)",
    },
    [FAM_UPS_INPUT_CURRENT_AMPS] = {
        .name = "ups_input_current_amps",
        .type = METRIC_TYPE_GAUGE,
        .help = "Input current (A)",
    },
    [FAM_UPS_INPUT_FREQUENCY_HZ] = {
        .name = "ups_input_frequency_hz",
        .type = METRIC_TYPE_GAUGE,
        .help = "Input line frequency (Hz)",
    },
    [FAM_UPS_INPUT_LOAD_RATIO] = {
        .name = "ups_input_load_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Load on (ePDU) input (percent of full)",
    },
    [FAM_UPS_INPUT_REALPOWER_WATTS] = {
        .name = "ups_input_realpower_watts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current sum value of all (ePDU) phases real power (W)",
    },
    [FAM_UPS_INPUT_POWER_VOLTS_AMPS] = {
        .name = "ups_input_power_volts_amps",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current sum value of all (ePDU) phases apparent power (VA)",
    },
    [FAM_UPS_OUTPUT_VOLTAGE_VOLTS] = {
        .name = "ups_output_voltage_volts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Output voltage (V)",
    },
    [FAM_UPS_OUTPUT_CURRENT_AMPS] = {
        .name = "ups_output_current_amps",
        .type = METRIC_TYPE_GAUGE,
        .help = "Input current (A)",
    },
    [FAM_UPS_OUTPUT_FREQUENCY_HZ] = {
        .name = "ups_output_frequency_hz",
        .type = METRIC_TYPE_GAUGE,
        .help = "Output frequency (Hz)",
    },
    [FAM_UPS_OUTPUT_REALPOWER_WATTS] = {
        .name = "ups_output_realpower_watts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Output real power (Watts)",
    },
    [FAM_UPS_OUTPUT_POWER_VOLTS_AMPS] = {
        .name = "ups_output_power_volts_amps",
        .type = METRIC_TYPE_GAUGE,
        .help = "Output apparent power (Volt-Amps)",
    },
    [FAM_UPS_BATTERY_CHARGE_RATIO] = {
        .name = "ups_battery_charge_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Battery charge (percent)",
    },
    [FAM_UPS_BATTERY_VOLTAGE_VOLTS] = {
        .name = "ups_battery_voltage_volts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Battery voltage (V)",
    },
    [FAM_UPS_BATTERY_CAPACITY_AMPS_HOUR] = {
        .name = "ups_battery_capacity_amps_hour",
        .type = METRIC_TYPE_GAUGE,
        .help = "Battery capacity (Ah)",
    },
    [FAM_UPS_BATTERY_CURRENT_AMPS] = {
        .name = "ups_battery_current_amps",
        .type = METRIC_TYPE_GAUGE,
        .help = "Battery current (A)",
    },
    [FAM_UPS_BATTERY_TEMPERATURE_CELSIUS] = {
        .name = "ups_battery_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "Battery temperature (degrees C)",
    },
    [FAM_UPS_BATTERY_RUNTIME_SECONDS] = {
        .name = "ups_battery_runtime_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Battery runtime (seconds)",
    },
    [FAM_UPS_AMBIENT_TEMPERATURE_CELSIUS] = {
        .name = "ups_ambient_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "Ambient temperature (degrees C)",
    },
    [FAM_UPS_AMBIENT_HUMIDITY_RATIO] = {
        .name = "ups_ambient_humidity_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = "Ambient relative humidity (percent)",
    },
};

typedef struct nut_ups_s {
    char *instance;
    char *name;
    char *upsname;
    char *hostname;
    NUT_PORT_TYPE port;
    ncollectd_upsconn_t *conn;
    label_set_t labels;
    bool force_ssl;
    bool verify_peer;
    cdtime_t connect_timeout;
    metric_family_t fams[FAM_UPS_MAX];
} nut_ups_t;

static char *ca_path;
static bool can_verify_peer;

static void free_nut_ups(void *arg)
{
    nut_ups_t *ups = arg;

    if (ups == NULL)
        return;

    if (ups->conn != NULL) {
        upscli_disconnect(ups->conn);
        free(ups->conn);
    }

    free(ups->instance);
    free(ups->name);
    free(ups->hostname);
    free(ups->upsname);

    label_set_reset(&ups->labels);

    free(ups);
}

static int nut_connect(nut_ups_t *ups)
{
    int ssl_flags = UPSCLI_CONN_TRYSSL;
    if (ups->force_ssl)
        ssl_flags |= UPSCLI_CONN_REQSSL;
    if (can_verify_peer && ups->verify_peer)
        ssl_flags |= UPSCLI_CONN_CERTVERIF;

    int status = 0;

#ifdef HAVE_UPSCLI_TRYCONNECT
    struct timeval tv = CDTIME_T_TO_TIMEVAL(ups->connect_timeout);
    status = upscli_tryconnect(ups->conn, ups->hostname, ups->port, ssl_flags, &tv);
#else
    status = upscli_connect(ups->conn, ups->hostname, ups->port, ssl_flags);
#endif

    if (status != 0) {
        PLUGIN_ERROR("upscli_connect (%s, %i) failed: %s",
                     ups->hostname, ups->port, upscli_strerror(ups->conn));
        free(ups->conn);
        return -1;
    }

    PLUGIN_INFO("Connection to (%s, %i) established.", ups->hostname, ups->port);

    // Output INFO or WARNING based on SSL and VERIFICATION
    int ssl_status = upscli_ssl(ups->conn); // 1 for SSL, 0 for not, -1 for error
    if (ssl_status == 1 && ups->verify_peer) {
        PLUGIN_INFO("Connection is secured with SSL and certificate has been verified.");
    } else if (ssl_status == 1) {
        PLUGIN_INFO("Connection is secured with SSL with no verification of server SSL certificate.");
    } else if (ssl_status == 0) {
        PLUGIN_WARNING("Connection is unsecured (no SSL).");
    } else {
        PLUGIN_ERROR("upscli_ssl failed: %s", upscli_strerror(ups->conn));
        free(ups->conn);
        return -1;
    }
    return 0;
}

static int nut_read(user_data_t *user_data)
{
    nut_ups_t *ups = user_data->data;

    /* (Re-)Connect if we have no connection */
    if (ups->conn == NULL) {
        ups->conn = malloc(sizeof(*ups->conn));
        if (ups->conn == NULL) {
            PLUGIN_ERROR("malloc failed.");
            return -1;
        }

        int status = nut_connect(ups);
        if (status == -1)
            return -1;

    }

    const char *query[3] = {"VAR", ups->upsname, NULL};
    unsigned int query_num = 2;
    /* nut plugin: nut_read_one: upscli_list_start (adpos) failed: Protocol error */
    int status = upscli_list_start(ups->conn, query_num, query);
    if (status != 0) {
        PLUGIN_ERROR("upscli_list_start (%s) failed: %s", ups->upsname, upscli_strerror(ups->conn));
        upscli_disconnect(ups->conn);
        free(ups->conn);
        return -1;
    }

    char **answer;
    NUT_SIZE_TYPE answer_num;
    while (upscli_list_next(ups->conn, query_num, query, &answer_num, &answer) == 1) {
        if (answer_num < 4)
            continue;

        char buffer[1024];
        sstrncpy(buffer, answer[2], sizeof(buffer));
        char *ptr = buffer;
        char *tokens[8] = {NULL};
        size_t tokens_num = 0;
        char *saveptr = NULL;
        char *token = NULL;
        while ((token = strtok_r(ptr, ".", &saveptr)) != NULL) {
            ptr = NULL;
            if (tokens_num < STATIC_ARRAY_SIZE(tokens)) {
                tokens[tokens_num] = token;
            }
            tokens_num++;
        }

        if ((tokens_num < 2) || (tokens_num > STATIC_ARRAY_SIZE(tokens)))
            continue;

        metric_family_t *fam = NULL;
        if (strcmp("ambient", tokens[0]) == 0) {
            if (strcmp("humidity", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_AMBIENT_HUMIDITY_RATIO];
            else if (strcmp("temperature", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_AMBIENT_TEMPERATURE_CELSIUS];
        } else if (strcmp("battery", tokens[0]) == 0) {
            if (strcmp("charge", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_BATTERY_CHARGE_RATIO];
            else if (strcmp("voltage", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_BATTERY_VOLTAGE_VOLTS];
            else if (strcmp("capacity", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_BATTERY_CAPACITY_AMPS_HOUR];
            else if (strcmp("current", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_BATTERY_CURRENT_AMPS];
            else if (strcmp("temperature", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_BATTERY_TEMPERATURE_CELSIUS];
            else if (strcmp("runtime", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_BATTERY_RUNTIME_SECONDS];
        } else if (strcmp("input", tokens[0]) == 0) {
            if (strcmp("voltage", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_INPUT_VOLTAGE_VOLTS];
            else if (strcmp("current", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_INPUT_CURRENT_AMPS];
            else if (strcmp("frequency", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_INPUT_FREQUENCY_HZ];
            else if (strcmp("load", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_INPUT_LOAD_RATIO];
            else if (strcmp("realpower", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_INPUT_REALPOWER_WATTS];
            else if (strcmp("power", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_INPUT_POWER_VOLTS_AMPS];
        } else if (strcmp("output", tokens[0]) == 0) {
            if (strcmp("voltage", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_OUTPUT_VOLTAGE_VOLTS];
            else if (strcmp("current", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_OUTPUT_CURRENT_AMPS];
            else if (strcmp("frequency", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_OUTPUT_FREQUENCY_HZ];
            else if (strcmp("realpower", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_OUTPUT_REALPOWER_WATTS];
            else if (strcmp("power", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_OUTPUT_POWER_VOLTS_AMPS];
        } else if (strcmp("ups", tokens[0]) == 0) {
            if (strcmp("power", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_POWER_VOLTS_AMPS];
            else if (strcmp("realpower", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_REALPOWER_WATTS];
            else if (strcmp("temperature", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_TEMPERATURE_CELSIUS];
            else if (strcmp("load", tokens[tokens_num-1]) == 0)
                fam = &ups->fams[FAM_UPS_LOAD_RATIO];
        }

        if (fam != NULL) {
            double value = atof(answer[3]);
            if (tokens_num > 2) {
                metric_family_append(fam, VALUE_GAUGE(value), NULL,
                                     &(label_pair_const_t){.name="context", .value=tokens[1]},
                                     &(label_pair_const_t){.name="instance", .value=ups->upsname},
                                     NULL);
            } else {
                metric_family_append(fam, VALUE_GAUGE(value), NULL,
                                     &(label_pair_const_t){.name="instance", .value=ups->upsname},
                                     NULL);
            }
        }
    }

    plugin_dispatch_metric_family_array(ups->fams, FAM_UPS_MAX, 0);
    return 0;
}

static int nut_config_instance(config_item_t *ci)
{
    nut_ups_t *ups = calloc(1, sizeof(*ups));
    if (ups == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }
    memcpy(ups->fams, fams, FAM_UPS_MAX * sizeof(*fams));

    int status = cf_util_get_string(ci, &ups->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name in %s:%d.",cf_get_file(ci), cf_get_lineno(ci));
        free_nut_ups(ups);
        return -1;
    }

    ups->connect_timeout = plugin_get_interval();

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "ups") == 0) {
            status = cf_util_get_string(child, &ups->name);
        } else if (strcasecmp(child->key, "force-ssl") == 0) {
            status = cf_util_get_boolean(child, &ups->force_ssl);
        } else if (strcasecmp(child->key, "verify-peer") == 0) {
            status = cf_util_get_boolean(child, &ups->verify_peer);
        } else if (strcasecmp(child->key, "connect-timeout") == 0) {
            status = cf_util_get_cdtime(child, &ups->connect_timeout);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ups->labels);
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
        free_nut_ups(ups);
        return -1;
    }

    status = upscli_splitname(ups->name, &ups->upsname, &ups->hostname, &ups->port);
    if (status != 0) {
        PLUGIN_ERROR("nut_add_ups: upscli_splitname (%s) failed.", ups->name);
        free_nut_ups(ups);
        return 1;
    }

    PLUGIN_DEBUG("nut_add_ups (name = %s);", ups->instance);

    return plugin_register_complex_read("nut", ups->instance, nut_read, interval,
                                        &(user_data_t){.data = ups,.free_func = free_nut_ups});
}

static int nut_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = nut_config_instance(child);
        } else if (strcasecmp(child->key, "ca-path") == 0) {
            status = cf_util_get_string(child, &ca_path);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0)
        return -1;

#ifdef HAVE_UPSCLI_INIT
    if (ca_path != NULL) {
        status = upscli_init(0, ca_path, NULL, NULL);
        if (status != 1) {
            PLUGIN_ERROR("upscli_init '%s' failed", ca_path);
            upscli_cleanup();
            return -1;
        }
        can_verify_peer = true;
    }
#else
    if (ca_path != NULL) {
        PLUGIN_WARNING("nut_connect: Dependency libupsclient version insufficient (<2.7) "
                       "for 'verify-peer support. Ignoring 'verify-peer' and 'ca-path'.");
        verify_peer = false;
    }
#endif

    return 0;
}

static int nut_shutdown(void)
{
#ifdef HAVE_UPSCLI_INIT
    upscli_cleanup();
#endif
    return 0;
}

void module_register(void)
{
    plugin_register_config("nut", nut_config);
    plugin_register_shutdown("nut", nut_shutdown);
}
