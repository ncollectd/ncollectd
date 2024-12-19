/* SPDX-License-Identifier: GPL-2.0-only OR MIT                         */
/* SPDX-FileCopyrightText: Copyright (C) 2005-2011 Florian octo Forster */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org>    */

#pragma once

#include "libconfig/config.h"
#include "libutils/time.h"
#include "libmetric/metric.h"
#include "libmetric/notification.h"

static inline char *cf_get_file(const config_item_t *ci)
{
    return ci->file == NULL ? NULL : ci->file->name;
}

static inline int cf_get_lineno(const config_item_t *ci)
{
    return ci->lineno - 1;
}

/* Assures the config option is a string, duplicates it and returns the copy in
 * "ret_string". If necessary "*ret_string" is freed first. Returns zero upon
 * success. */
int cf_util_get_string(const config_item_t *ci, char **ret_string);

int cf_util_get_string_env(const config_item_t *ci, char **ret_string);

int cf_util_get_string_file(const config_item_t *ci, char **ret_string);

/* Assures the config option is a string and copies it to the provided buffer.
 * Assures null-termination. */
int cf_util_get_string_buffer(const config_item_t *ci, char *buffer,
                              size_t buffer_size);

/* Assures the config option is a number and returns it as an int. */
int cf_util_get_int(const config_item_t *ci, int *ret_value);
int cf_util_get_unsigned_int(const config_item_t *ci, unsigned int *ret_value);

/* Assures the config option is a number and returns it as a double. */
int cf_util_get_double(const config_item_t *ci, double *ret_value);
int cf_util_get_double_array(const config_item_t *ci, size_t *ret_size, double **ret_values);

/* Assures the config option is a boolean and assignes it to `ret_bool'.
 * Otherwise, `ret_bool' is not changed and non-zero is returned. */
int cf_util_get_boolean(const config_item_t *ci, bool *ret_bool);

/* Assures the config option is a boolean and set or unset the given flag in
 * `ret_value' as appropriate. Returns non-zero on error. */
int cf_util_get_flag(const config_item_t *ci, unsigned int *ret_value, unsigned int flag);

/* Assures that the config option is a string or a number if the correct range
 * of 1-65535. The string is then converted to a port number using
 * `service_name_to_port_number' and returned.
 * Returns the port number in the range [1-65535] or less than zero upon
 * failure. */
int cf_util_get_port_number(const config_item_t *ci, int *ret_port);

/* Assures that the config option is either a service name (a string) or a port
 * number (an integer in the appropriate range) and returns a newly allocated
 * string. If ret_string points to a non-NULL pointer, it is freed before
 * assigning a new value. */
int cf_util_get_service(const config_item_t *ci, char **ret_string);

int cf_util_get_cdtime(const config_item_t *ci, cdtime_t *ret_value);

int cf_util_get_label(const config_item_t *ci, label_set_t *labels);

int cf_util_get_metric_type(const config_item_t *ci, metric_type_t *ret_metric);

int cf_util_get_severity(const config_item_t *ci, severity_t *ret_severity);

int cf_util_get_log_level(const config_item_t *ci, int *ret_log_level);

typedef struct {
    char *option;
    uint64_t flag;
} cf_flags_t;

int cf_util_get_flags(const config_item_t *ci, cf_flags_t *flags_set,
                      size_t flags_set_size, uint64_t *flags);

typedef enum {
    SEND_METRICS,
    SEND_NOTIFICATIONS,
} cf_send_t;

int cf_uti_get_send(const config_item_t *ci, cf_send_t *send);
