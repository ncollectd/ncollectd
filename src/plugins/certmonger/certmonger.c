// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include <systemd/sd-daemon.h>
#include <systemd/sd-bus.h>

enum {
    FAM_CERTMONGER_UP,
    FAM_CERTMONGER_REQUEST_STATUS,
    FAM_CERTMONGER_REQUEST_CA_ERROR,
    FAM_CERTMONGER_REQUEST_KEY_GENERATED_DATE_SECONDS,
    FAM_CERTMONGER_REQUEST_KEY_ISSUED_COUNT,
    FAM_CERTMONGER_REQUEST_LAST_CHECKED_DATE_SECONDS,
    FAM_CERTMONGER_REQUEST_NOT_VALID_AFTER_DATE_SECONDS,
    FAM_CERTMONGER_REQUEST_NOT_VALID_BEFORE_DATE_SECONDS,
    FAM_CERTMONGER_REQUEST_STUCK,
    FAM_CERTMONGER_MAX
};

static metric_family_t fams[FAM_CERTMONGER_MAX] = {
    [FAM_CERTMONGER_UP] = {
        .name = "certmonger_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "1 if certmonger was contactable via D-Bus.",
    },
    [FAM_CERTMONGER_REQUEST_STATUS] = {
        .name = "certmonger_request_status",
        .type = METRIC_TYPE_GAUGE,
        .help = "State of each tracking request.",
    },
    [FAM_CERTMONGER_REQUEST_CA_ERROR] = {
        .name = "certmonger_request_ca_error",
        .type = METRIC_TYPE_GAUGE,
        .help = "1 if the CA returned an error when certificate signing was requested.",
    },
    [FAM_CERTMONGER_REQUEST_KEY_GENERATED_DATE_SECONDS] = {
        .name = "certmonger_request_key_generated_date_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Timestamp the private key was generated.",
    },
    [FAM_CERTMONGER_REQUEST_KEY_ISSUED_COUNT] = {
        .name = "certmonger_request_key_issued_count",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of times a certificate was issued for the private key."
    },
    [FAM_CERTMONGER_REQUEST_LAST_CHECKED_DATE_SECONDS] = {
        .name = "certmonger_request_last_checked_date_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Timestamp of last check for expiration."
    },
    [FAM_CERTMONGER_REQUEST_NOT_VALID_AFTER_DATE_SECONDS] = {
        .name = "certmonger_request_not_valid_after_date_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Timestamp of certificate expiry."
    },
    [FAM_CERTMONGER_REQUEST_NOT_VALID_BEFORE_DATE_SECONDS] = {
        .name = "certmonger_request_not_valid_before_date_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Timestamp after which certificate is valid."
    },
    [FAM_CERTMONGER_REQUEST_STUCK] = {
        .name = "certmonger_request_stuck",
        .type = METRIC_TYPE_GAUGE,
        .help = "1 if request is stuck."
    }
};

static plugin_filter_t *filter;
static exclist_t excl_request;

static int get_property_bool(sd_bus *bus, const char *destination, const char *path,
                             const char *interface, const char *member, int *value)
{
    sd_bus_message *reply = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;

    int status = sd_bus_call_method(bus, destination, path, "org.freedesktop.DBus.Properties",
                                         "Get", &error, &reply, "ss", interface, member);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    sd_bus_error_free(&error);

    status = sd_bus_message_enter_container(reply, 'v', "b");
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    int boolean = 0;
    status = sd_bus_message_read_basic(reply, 'b', &boolean);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    *value = boolean;

    sd_bus_message_unref(reply);

    return 0;
}

static int get_property_integer(sd_bus *bus, const char *destination, const char *path,
                                const char *interface, const char *member, int64_t *value)
{
    sd_bus_message *reply = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;

    int status = sd_bus_call_method(bus, destination, path, "org.freedesktop.DBus.Properties",
                                         "Get", &error, &reply, "ss", interface, member);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    sd_bus_error_free(&error);

    status = sd_bus_message_enter_container(reply, 'v', "x");
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    int64_t integer = 0;
    status = sd_bus_message_read_basic(reply, 'x', &integer);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    *value = integer;

    sd_bus_message_unref(reply);

    return 0;
}

static int get_property_string(sd_bus *bus, const char *destination, const char *path,
                               const char *interface, const char *member, char *type, char **value)
{
    sd_bus_message *reply = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;

    int status = sd_bus_call_method(bus, destination, path, "org.freedesktop.DBus.Properties",
                                         "Get", &error, &reply, "ss", interface, member);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    sd_bus_error_free(&error);

    status = sd_bus_message_enter_container(reply, 'v', type);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    char *str = NULL;
    status = sd_bus_message_read_basic(reply, type[0], &str);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    *value = strdup(str);
    if (*value == NULL) {
        sd_bus_message_unref(reply);
        return -1;
    }

    sd_bus_message_unref(reply);

    return 0;
}

static int ca_property_string(sd_bus *bus, const char *path, const char *member, char **value)
{
    return get_property_string(bus, "org.fedorahosted.certmonger", path,
                                    "org.fedorahosted.certmonger.ca", member, "s", value);
}

static int request_property_string(sd_bus *bus, const char *path, const char *member, char **value)
{
    return get_property_string(bus, "org.fedorahosted.certmonger", path,
                                    "org.fedorahosted.certmonger.request", member, "s", value);
}

static int request_property_object_path(sd_bus *bus, const char *path, const char *member,
                                        char **value)
{
    return get_property_string(bus, "org.fedorahosted.certmonger", path,
                                    "org.fedorahosted.certmonger.request", member, "o", value);
}

static int request_property_bool(sd_bus *bus, const char *path, const char *member, int *value)
{
    return get_property_bool(bus, "org.fedorahosted.certmonger", path,
                                  "org.fedorahosted.certmonger.request", member, value);
}

static int request_property_integer(sd_bus *bus, const char *path, const char *member,
                                    int64_t *value)
{
    return get_property_integer(bus, "org.fedorahosted.certmonger", path,
                                     "org.fedorahosted.certmonger.request", member, value);
}


static int certmonger_get_requests(sd_bus *bus)
{
    sd_bus_message *reply = NULL;

    sd_bus_error error = SD_BUS_ERROR_NULL;
    int status = sd_bus_call_method(bus, "org.fedorahosted.certmonger",
                                         "/org/fedorahosted/certmonger",
                                         "org.fedorahosted.certmonger",
                                         "get_requests", &error, &reply, NULL);
    if (status < 0)
        return -1;

    sd_bus_error_free(&error);

    status = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "o");
    if (status < 0) {
        PLUGIN_ERROR("sd_bus_message_enter_container failed.");
        sd_bus_message_unref(reply);
        return -1;
    }

    while (true) {
        char *object_path;
        status = sd_bus_message_read(reply, "o", &object_path);
        if (status <= 0)
          break;

        char *nickname = NULL;
        status = request_property_string(bus, object_path, "nickname", &nickname);
        if (status != 0)
            continue;
        if (nickname == NULL)
            continue;

        if (!exclist_match(&excl_request, nickname)) {
            free(nickname);
            continue;
        }

        char *ca_nickname = NULL;
        char *ca_object_path = NULL;
        status = request_property_object_path(bus, object_path, "ca", &ca_object_path);
        if ((status == 0) && (ca_object_path != NULL)) {
            ca_property_string(bus, ca_object_path, "nickname", &ca_nickname);
            free(ca_object_path);
        }

        char *storage_type = NULL;
        char *storage_location = NULL;

        status = request_property_string(bus, object_path, "cert-storage", &storage_type); 
        if ((status == 0) && (storage_type != NULL)) {
            if (strcmp(storage_type, "FILE") == 0) {
                request_property_string(bus, object_path, "cert-file", &storage_location);
            } else if (strcmp(storage_type, "NSSDB") == 0) {
                request_property_string(bus, object_path, "cert-database", &storage_location);
            } else {
                PLUGIN_WARNING("The certificate request '%s' has an unknown storage type '%s'",
                               nickname, storage_type);
            }
        }
    
        label_set_t labels = {0};

        if (nickname != NULL)
            label_set_add(&labels, true, "request", nickname);
        if (ca_nickname != NULL)
            label_set_add(&labels, true, "ca", ca_nickname);
        if (storage_location != NULL)
            label_set_add(&labels, true, "location", storage_location);

        free(nickname);
        free(ca_nickname);
        free(storage_type);
        free(storage_location);

        char *request_status = NULL;
        status = request_property_string(bus, object_path, "status", &request_status);
        if ((status == 0) && (request_status != NULL)) {
            metric_family_append(&fams[FAM_CERTMONGER_REQUEST_STATUS], VALUE_GAUGE(1), &labels, 
                                 &LABEL_PAIR_CONST("status", request_status), NULL);
            free(request_status);
        }

        char *ca_error = NULL;
        status = request_property_string(bus, object_path, "ca-error", &ca_error);
        metric_family_append(&fams[FAM_CERTMONGER_REQUEST_CA_ERROR],
                             VALUE_GAUGE((status != 0) && (ca_error != NULL) ? 1 : 0),
                             &labels, NULL);
        free(ca_error);
        
        
        int64_t key_generated_date = 0;
        request_property_integer(bus, object_path, "key-generated-date", &key_generated_date);
        if (key_generated_date > 0)
            metric_family_append(&fams[FAM_CERTMONGER_REQUEST_KEY_GENERATED_DATE_SECONDS],
                                 VALUE_GAUGE(key_generated_date), &labels, NULL);
            
        int64_t key_issued_count = 0;
        request_property_integer(bus, object_path, "key-issued-count", &key_issued_count);
        metric_family_append(&fams[FAM_CERTMONGER_REQUEST_KEY_ISSUED_COUNT],
                             VALUE_GAUGE(key_issued_count), &labels, NULL);
        
        int64_t last_checked = 0;
        request_property_integer(bus, object_path, "last-checked", &last_checked);
        metric_family_append(&fams[FAM_CERTMONGER_REQUEST_LAST_CHECKED_DATE_SECONDS],
                             VALUE_GAUGE(last_checked), &labels, NULL);
        
        int64_t not_valid_after = 0;
        request_property_integer(bus, object_path, "not-valid-after",  &not_valid_after);
        metric_family_append(&fams[FAM_CERTMONGER_REQUEST_NOT_VALID_AFTER_DATE_SECONDS],
                             VALUE_GAUGE(not_valid_after), &labels, NULL);
 

        int64_t not_valid_before = 0;
        request_property_integer(bus, object_path, "not-valid-before", &not_valid_before);
        metric_family_append(&fams[FAM_CERTMONGER_REQUEST_NOT_VALID_BEFORE_DATE_SECONDS],
                             VALUE_GAUGE(not_valid_before), &labels, NULL);

        int stuck = 0;
        request_property_bool(bus, object_path, "stuck", &stuck);
        metric_family_append(&fams[FAM_CERTMONGER_REQUEST_STUCK], VALUE_GAUGE(stuck),
                             &labels, NULL);

        label_set_reset(&labels);
    }

    sd_bus_message_exit_container(reply);
    sd_bus_message_unref(reply);

    return 0; 
}

static int certmonger_read(void)
{
    sd_bus *bus = NULL;

    if (sd_booted() <= 0)
        return -1;

    sd_bus_default_system(&bus);

    int requests = certmonger_get_requests(bus);
    metric_family_append(&fams[FAM_CERTMONGER_UP],
                         VALUE_GAUGE(requests >= 0 ? 1 : 0), NULL, NULL);

    sd_bus_unref(bus);

    plugin_dispatch_metric_family_array_filtered(fams, FAM_CERTMONGER_MAX, filter, 0);

    return 0;
}

static int certmonger_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "request") == 0) {
            status = cf_util_exclist(child, &excl_request);
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = plugin_filter_configure(child, &filter);
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

static int certmonger_shutdown(void)
{
    exclist_reset(&excl_request);
    plugin_filter_free(filter);

    return 0;
}

void module_register(void)
{
    plugin_register_config("certmonger", certmonger_config);
    plugin_register_read("certmonger", certmonger_read);
    plugin_register_shutdown("certmonger", certmonger_shutdown);
}
