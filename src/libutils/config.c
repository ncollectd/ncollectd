// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2005-2011 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Sebastian tokkee Harl <sh at tokkee.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "log.h"
#include "libutils/config.h"
#include "libutils/common.h"
#include "libmetric/notification.h"

/* Assures the config option is a string, duplicates it and returns the copy in
 * "ret_string". If necessary "*ret_string" is freed first. Returns zero upon
 * success. */
int cf_util_get_string(const config_item_t *ci, char **ret_string)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    char *string = strdup(ci->values[0].value.string);
    if (string == NULL)
        return -1;

    if (*ret_string != NULL)
        free(*ret_string);
    *ret_string = string;

    return 0;
}

/* Assures the config option is a string and copies it to the provided buffer.
 * Assures NUL-termination. */
int cf_util_get_string_buffer(const config_item_t *ci, char *buffer, size_t buffer_size)
{
    if ((ci == NULL) || (buffer == NULL) || (buffer_size < 1))
        return EINVAL;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    strncpy(buffer, ci->values[0].value.string, buffer_size);
    buffer[buffer_size - 1] = '\0';

    return 0;
}

/* Assures the config option is a number and returns it as an int. */
int cf_util_get_int(const config_item_t *ci, int *ret_value)
{
    if ((ci == NULL) || (ret_value == NULL))
        return EINVAL;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_NUMBER)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one numeric argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    *ret_value = (int)ci->values[0].value.number;

    return 0;
}

int cf_util_get_unsigned_int(const config_item_t *ci, unsigned int *ret_value)
{
    if ((ci == NULL) || (ret_value == NULL))
        return EINVAL;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_NUMBER)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one numeric argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    *ret_value = (unsigned int)ci->values[0].value.number;

    return 0;
}

int cf_util_get_double(const config_item_t *ci, double *ret_value)
{
    if ((ci == NULL) || (ret_value == NULL))
        return EINVAL;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_NUMBER)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one numeric argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    *ret_value = ci->values[0].value.number;

    return 0;
}

int cf_util_get_double_array(const config_item_t *ci, size_t *ret_size, double **ret_values)
{
    if ((ci == NULL) || (ret_size == NULL) || (ret_values == NULL))
        return EINVAL;

    if (ci->values_num <= 0) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires a list of numbers.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    for (int i=0 ; i < ci->values_num; i++) {
        if (ci->values[i].type != CONFIG_TYPE_NUMBER) {
            PLUGIN_ERROR("The argument %d in option '%s' at %s:%d must be a number.",
                         i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }
    }

    double *values = malloc(sizeof(double) * ci->values_num);
    if (values == NULL) {
        PLUGIN_ERROR("malloc failed: %s.", STRERRNO);
        return -1;
    }

    for (int i=0 ; i < ci->values_num; i++) {
        values[i] = ci->values[i].value.number;
    }

    *ret_values = values;
    *ret_size = ci->values_num;

    return 0;
}

int cf_util_get_boolean(const config_item_t *ci, bool *ret_bool)
{
    if ((ci == NULL) || (ret_bool == NULL))
        return EINVAL;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_BOOLEAN)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one boolean argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    *ret_bool = ci->values[0].value.boolean ? true : false;

    return 0;
}

int cf_util_get_flag(const config_item_t *ci, unsigned int *ret_value, unsigned int flag)
{
    if (ret_value == NULL)
        return EINVAL;

    bool b = false;
    int status = cf_util_get_boolean(ci, &b);
    if (status != 0)
        return status;

    if (b) {
        *ret_value |= flag;
    } else {
        *ret_value &= ~flag;
    }

    return 0;
}

/* Assures that the config option is a string or a number if the correct range
 * of 1-65535. The string is then converted to a port number using
 * 'service_name_to_port_number' and returned.
 * Returns the port number in the range [1-65535] or less than zero upon
 * failure. */
int cf_util_get_port_number(const config_item_t *ci, int *ret_port)
{
    if ((ci->values_num != 1) || ((ci->values[0].type != CONFIG_TYPE_STRING) &&
                                  (ci->values[0].type != CONFIG_TYPE_NUMBER))) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                      ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (ci->values[0].type == CONFIG_TYPE_STRING) {
        int tmp = service_name_to_port_number(ci->values[0].value.string);
        if (tmp < 0)
            return -1;
        *ret_port = tmp;
        return 0;
    }

    assert(ci->values[0].type == CONFIG_TYPE_NUMBER);
    int tmp = (int)(ci->values[0].value.number + 0.5);
    if ((tmp < 1) || (tmp > 65535)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires a service name or a port number. The "
                     "number you specified, %i, is not in the valid range of 1-65535.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci), tmp);
        return -1;
    }

    *ret_port = tmp;

    return 0;
}

int cf_util_get_service(const config_item_t *ci, char **ret_string)
{
    if (ci->values_num != 1) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (ci->values[0].type == CONFIG_TYPE_STRING)
        return cf_util_get_string(ci, ret_string);

    if (ci->values[0].type != CONFIG_TYPE_NUMBER) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string or numeric argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    int port = 0;
    int status = cf_util_get_int(ci, &port);
    if (status != 0) {
        return status;
    } else if ((port < 1) || (port > 65535)) {
        PLUGIN_ERROR("The port number given for the '%s' option in %s:%d is out of range (%i).",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci), port);
        return -1;
    }

    char *service = malloc(6);
    if (service == NULL) {
        PLUGIN_ERROR("cf_util_get_service: Out of memory.");
        return -1;
    }
    ssnprintf(service, 6, "%i", port);

    free(*ret_string);
    *ret_string = service;

    return 0;
}

int cf_util_get_cdtime(const config_item_t *ci, cdtime_t *ret_value)
{
    if ((ci == NULL) || (ret_value == NULL))
        return EINVAL;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_NUMBER)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one numeric argument.",
                      ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (ci->values[0].value.number < 0.0) {
        PLUGIN_ERROR("The numeric argument of the '%s' option in %s:%d must not be negative.",
                      ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    *ret_value = DOUBLE_TO_CDTIME_T(ci->values[0].value.number);

    return 0;
}

int cf_util_get_label(const config_item_t *ci, label_set_t *labels)
{
    if ((ci->values_num != 2) ||
        ((ci->values_num == 2) && ((ci->values[0].type != CONFIG_TYPE_STRING) ||
                                   (ci->values[1].type != CONFIG_TYPE_STRING)))) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly two string arguments.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    return label_set_add(labels, true, ci->values[0].value.string, ci->values[1].value.string);
}

int cf_util_get_metric_type(const config_item_t *ci, metric_type_t *ret_metric)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (!strcasecmp(ci->values[0].value.string, "gauge")) {
        *ret_metric = METRIC_TYPE_GAUGE;
    } else if (!strcasecmp(ci->values[0].value.string, "unknow")) {
        *ret_metric = METRIC_TYPE_UNKNOWN;
    } else if (!strcasecmp(ci->values[0].value.string, "counter")) {
        *ret_metric = METRIC_TYPE_COUNTER;
    } else if (!strcasecmp(ci->values[0].value.string, "info")) {
        *ret_metric = METRIC_TYPE_INFO;
    } else {
        PLUGIN_ERROR("The '%s' option in %s:%d must be: 'gauge', 'unknow', 'info' or 'counter'.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    return 0;
}

int cf_util_get_log_level(const config_item_t *ci, int *ret_log_level)
{
    if ((ci == NULL) || (ret_log_level == NULL))
        return EINVAL;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (strcasecmp(ci->values[0].value.string, "emerg") == 0)  {
        *ret_log_level = LOG_ERR;
    } else if (strcasecmp(ci->values[0].value.string, "alert") == 0) {
        *ret_log_level = LOG_ERR;
    } else if (strcasecmp(ci->values[0].value.string, "crit") == 0) {
        *ret_log_level = LOG_ERR;
    } else if (strcasecmp(ci->values[0].value.string, "err") == 0) {
        *ret_log_level = LOG_ERR;
    } else if (strcasecmp(ci->values[0].value.string, "warning") == 0) {
        *ret_log_level = LOG_WARNING;
    } else if (strcasecmp(ci->values[0].value.string, "notice") == 0) {
        *ret_log_level = LOG_NOTICE;
    } else if (strcasecmp(ci->values[0].value.string, "info") == 0) {
        *ret_log_level = LOG_INFO;
#ifdef NCOLLECTD_DEBUG
    } else if (strcasecmp(ci->values[0].value.string, "debug") == 0) {
        *ret_log_level = LOG_DEBUG;
#else
    } else if (strcasecmp(ci->values[0].value.string, "debug") == 0) {
        *ret_log_level = LOG_INFO;
#endif
    } else {
        PLUGIN_ERROR("The '%s' option in %s:%d must be: "
                     "'emerg', 'alert', 'crit', 'err', 'warning', 'notice', 'info' or 'debug'.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    return 0;
}

int cf_util_get_severity(const config_item_t *ci, severity_t *ret_severity)
{
    if ((ci == NULL) || (ret_severity == NULL))
        return EINVAL;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (strcasecmp("OK", ci->values[0].value.string) == 0) {
        *ret_severity = NOTIF_OKAY;
    } else if (strcasecmp("WARNING", ci->values[0].value.string) == 0) {
        *ret_severity = NOTIF_WARNING;
    } else if (strcasecmp("WARN", ci->values[0].value.string) == 0) {
        *ret_severity = NOTIF_WARNING;
    } else if (strcasecmp("FAILURE", ci->values[0].value.string) == 0) {
        *ret_severity = NOTIF_FAILURE;
    } else {
        PLUGIN_ERROR("The '%s' option in %s:%d must be: 'ok', 'warning' or 'failure' ",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    return 0;
}

int cf_util_get_flags(const config_item_t *ci, cf_flags_t *flags_set, size_t flags_set_size,
                                                uint64_t *flags)
{
    if (ci->values_num == 0) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires one or more arguments.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    for (int i = 0; i < ci->values_num; i++) {
        if (ci->values[i].type != CONFIG_TYPE_STRING) {
            PLUGIN_ERROR("The %d argument of '%s' option in %s:%d must be a string.",
                         i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }

        char *option = ci->values[i].value.string;

        bool negate = false;
        if (option[0] == '!') {
            negate = true;
            option++;
        }

        if (!strcasecmp("ALL", option)) {
            if (negate) {
                *flags = 0ULL;
            } else {
                *flags = ~0ULL;
            }
            continue;
        }

        for (size_t j = 0; j < flags_set_size; j++)  {
            if (!strcasecmp(flags_set[j].option, option)) {
                if (negate) {
                    *flags &= ~flags_set[j].flag;
                } else {
                    *flags |= flags_set[j].flag;
                }
            }
        }
    }

    return 0;
}

int cf_uti_get_send(const config_item_t *ci, cf_send_t *send)
{
    if ((ci == NULL) || (send == NULL))
        return EINVAL;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (strcasecmp("metric", ci->values[0].value.string) == 0) {
        *send = SEND_METRICS;
    } else if (strcasecmp("metrics", ci->values[0].value.string) == 0) {
        *send = SEND_METRICS;
    } else if (strcasecmp("notif", ci->values[0].value.string) == 0) {
        *send = SEND_NOTIFICATIONS;
    } else if (strcasecmp("notification", ci->values[0].value.string) == 0) {
        *send = SEND_NOTIFICATIONS;
    } else if (strcasecmp("notifications", ci->values[0].value.string) == 0) {
        *send = SEND_NOTIFICATIONS;
    } else {
        PLUGIN_ERROR("The '%s' option in %s:%d must be: 'metrics' or 'notifications' ",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    return 0;
}
