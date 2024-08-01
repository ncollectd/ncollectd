// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <systemd/sd-daemon.h>
#include <systemd/sd-bus.h>

static char **units;
static size_t units_size;

enum {
    FAM_SYSTEMD_UNIT_LOAD_STATE,
    FAM_SYSTEMD_UNIT_ACTIVE_STATE,
    FAM_SYSTEMD_UNIT_SUB_STATE,
    FAM_SYSTEMD_UNIT_START_TIME_SECONDS,
    FAM_SYSTEMD_UNIT_TASKS_CURRENT,
    FAM_SYSTEMD_UNIT_TASKS_MAX,
    FAM_SYSTEMD_SERVICE_RESTART,
    FAM_SYSTEMD_TIMER_LAST_TRIGGER_SECONDS,
    FAM_SYSTEMD_SOCKET_ACCEPTED_CONNECTIONS,
    FAM_SYSTEMD_SOCKET_CURRENT_CONNECTIONS,
    FAM_SYSTEMD_SOCKET_REFUSED_CONNECTIONS,
    FAM_SYSTEMD_MAX,
};

static metric_family_t fam[FAM_SYSTEMD_MAX] = {
    [FAM_SYSTEMD_UNIT_LOAD_STATE] = {
        .name = "systemd_unit_load_state",
        .type = METRIC_TYPE_STATE_SET,
        .help = "Reflects whether the unit definition was properly loaded.",
    },
    [FAM_SYSTEMD_UNIT_ACTIVE_STATE] = {
        .name = "systemd_unit_active_state",
        .type = METRIC_TYPE_STATE_SET,
        .help = "The high-level unit activation state.",
    },
    [FAM_SYSTEMD_UNIT_SUB_STATE] = {
        .name = "systemd_unit_sub_state",
        .type = METRIC_TYPE_STATE_SET,
        .help = "The low-level unit activation state, values depend on unit type.",
    },
    [FAM_SYSTEMD_UNIT_START_TIME_SECONDS] = {
        .name = "systemd_unit_start_time_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Start time of the unit since unix epoch in seconds.",
    },
    [FAM_SYSTEMD_UNIT_TASKS_CURRENT] = {
        .name = "systemd_unit_tasks_current",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of tasks per Systemd unit.",
    },
    [FAM_SYSTEMD_UNIT_TASKS_MAX] = {
        .name = "systemd_unit_tasks_max",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum number of tasks per Systemd unit.",
    },
    [FAM_SYSTEMD_SERVICE_RESTART] = {
        .name = "systemd_service_restart",
        .type = METRIC_TYPE_COUNTER,
        .help = "Service unit count of restart triggers.",
    },
    [FAM_SYSTEMD_TIMER_LAST_TRIGGER_SECONDS] = {
        .name = "systemd_timer_last_trigger_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Seconds since epoch of last trigger.",
    },
    [FAM_SYSTEMD_SOCKET_ACCEPTED_CONNECTIONS] = {
        .name = "systemd_socket_accepted_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of accepted socket connections.",
    },
    [FAM_SYSTEMD_SOCKET_CURRENT_CONNECTIONS] = {
        .name = "systemd_socket_current_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of socket connections.",
    },
    [FAM_SYSTEMD_SOCKET_REFUSED_CONNECTIONS] = {
        .name = "systemd_socket_refused_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of refused socket connections.",
    },
};

static bool strendswith(const char *s, const char *suffix)
{
    size_t sl = strlen(s);
    size_t pl = strlen(suffix);

    if (pl == 0)
        return false;

    if (sl < pl)
        return false;

    if (strcmp(s + sl - pl, suffix) == 0)
        return true;

    return false;
}

static int get_property_uint32(sd_bus *bus, const char *destination, const char *path,
                               const char *interface, const char *member, uint32_t *value)
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

    status = sd_bus_message_enter_container(reply, 'v', "u");
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    uint32_t number = 0;
    status = sd_bus_message_read_basic(reply, 'u', &number);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    *value = number;

    sd_bus_message_unref(reply);

    return 0;
}

static int get_property_uint64(sd_bus *bus, const char *destination, const char *path,
                               const char *interface, const char *member, uint64_t *value)
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

    status = sd_bus_message_enter_container(reply, 'v', "t");
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    uint64_t number = 0;
    status = sd_bus_message_read_basic(reply, 't', &number);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    *value = number;

    sd_bus_message_unref(reply);

    return 0;
}

static int unit_service(sd_bus *bus, char *name, char *unit_path)
{
    uint32_t r = 0;
    int status = get_property_uint32(bus, "org.freedesktop.systemd1", unit_path,
                                          "org.freedesktop.systemd1.Service", "NRestarts", &r);
    if (status == 0)
        metric_family_append(&fam[FAM_SYSTEMD_SERVICE_RESTART], VALUE_COUNTER(r), NULL,
                             &(label_pair_const_t){.name="unit", .value = name}, NULL);

    uint64_t tasks = 0;
    status = get_property_uint64(bus, "org.freedesktop.systemd1", unit_path,
                                      "org.freedesktop.systemd1.Service", "TasksCurrent", &tasks);
    if (status == 0)
        metric_family_append(&fam[FAM_SYSTEMD_UNIT_TASKS_CURRENT], VALUE_GAUGE(tasks), NULL,
                             &(label_pair_const_t){.name="unit", .value = name}, NULL);

    uint64_t tasks_max = 0;
    status = get_property_uint64(bus, "org.freedesktop.systemd1", unit_path,
                                      "org.freedesktop.systemd1.Service", "TasksMax", &tasks_max);
    if (status == 0)
        metric_family_append(&fam[FAM_SYSTEMD_UNIT_TASKS_MAX], VALUE_GAUGE(tasks_max), NULL,
                             &(label_pair_const_t){.name="unit", .value = name}, NULL);

    return 0;
}

static int unit_socket(sd_bus *bus, char *name, char *unit_path)
{
    uint32_t accepted = 0;
    int status = get_property_uint32(bus, "org.freedesktop.systemd1", unit_path,
                                          "org.freedesktop.systemd1.Socket",
                                          "NAccepted", &accepted);
    if (status == 0)
        metric_family_append(&fam[FAM_SYSTEMD_SOCKET_ACCEPTED_CONNECTIONS],
                             VALUE_COUNTER(accepted), NULL,
                             &(label_pair_const_t){.name="unit", .value = name}, NULL);

    uint32_t connections = 0;
    status = get_property_uint32(bus, "org.freedesktop.systemd1", unit_path,
                                      "org.freedesktop.systemd1.Socket",
                                      "NConnections", &connections);
    if (status == 0)
        metric_family_append(&fam[FAM_SYSTEMD_SOCKET_CURRENT_CONNECTIONS],
                             VALUE_GAUGE(connections), NULL,
                             &(label_pair_const_t){.name="unit", .value = name}, NULL);

    uint32_t refused = 0;
    status = get_property_uint32(bus, "org.freedesktop.systemd1", unit_path,
                                      "org.freedesktop.systemd1.Socket", "NRefused", &refused);
    if (status == 0)
        metric_family_append(&fam[FAM_SYSTEMD_SOCKET_REFUSED_CONNECTIONS],
                             VALUE_COUNTER(refused), NULL,
                             &(label_pair_const_t){.name="unit", .value = name}, NULL);

    return 0;
}

static int unit_timer(sd_bus *bus, char *name, char *unit_path)
{
    uint64_t last = 0;
    int status = get_property_uint64(bus, "org.freedesktop.systemd1", unit_path,
                                          "org.freedesktop.systemd1.Timer",
                                          "LastTriggerUSec", &last);
    if (status == 0) {
        double last_trigger = last/1e6;
        metric_family_append(&fam[FAM_SYSTEMD_TIMER_LAST_TRIGGER_SECONDS],
                             VALUE_GAUGE(last_trigger), NULL,
                             &(label_pair_const_t){.name="unit", .value = name}, NULL);
    }

    return 0;
}

static int submit_unit(sd_bus *bus, char *unit, char *unit_path, char *load_state,
                                    char *active_state, char *sub_state)
{
    state_t load_states[] = {
        { .name = "stub",        .enabled = false },
        { .name = "loaded",      .enabled = false },
        { .name = "not-found",   .enabled = false },
        { .name = "bad-setting", .enabled = false },
        { .name = "error",       .enabled = false },
        { .name = "merged",      .enabled = false },
        { .name = "masked",      .enabled = false },
    };
    state_set_t load_set = { .num = STATIC_ARRAY_SIZE(load_states), .ptr = load_states };
    for (size_t j = 0; j < load_set.num ; j++) {
        if (strcmp(load_set.ptr[j].name, load_state) == 0) {
            load_set.ptr[j].enabled = true;
            break;
        }
    }
    metric_family_append(&fam[FAM_SYSTEMD_UNIT_LOAD_STATE], VALUE_STATE_SET(load_set), NULL,
                         &(label_pair_const_t){.name="unit", .value=unit}, NULL);

    state_t active_states[] = {
        { .name = "active",       .enabled = false },
        { .name = "reloading",    .enabled = false },
        { .name = "inactive",     .enabled = false },
        { .name = "failed",       .enabled = false },
        { .name = "activating",   .enabled = false },
        { .name = "deactivating", .enabled = false },
        { .name = "maintenance",  .enabled = false },
    };
    state_set_t active_set = { .num = STATIC_ARRAY_SIZE(active_states), .ptr = active_states };
    for (size_t j = 0; j < active_set.num ; j++) {
        if (strcmp(active_set.ptr[j].name, active_state) == 0) {
            active_set.ptr[j].enabled = true;
            break;
        }
    }
    metric_family_append(&fam[FAM_SYSTEMD_UNIT_ACTIVE_STATE], VALUE_STATE_SET(active_set), NULL,
                         &(label_pair_const_t){.name="unit", .value=unit}, NULL);

    state_t sub_states[] = {
        { .name = "dead",                       .enabled = false },
        { .name = "condition",                  .enabled = false },
        { .name = "start-pre",                  .enabled = false },
        { .name = "start",                      .enabled = false },
        { .name = "start-post",                 .enabled = false },
        { .name = "running",                    .enabled = false },
        { .name = "exited",                     .enabled = false },
        { .name = "reload",                     .enabled = false },
        { .name = "reload-signal",              .enabled = false },
        { .name = "reload-notify",              .enabled = false },
        { .name = "stop",                       .enabled = false },
        { .name = "stop-watchdog",              .enabled = false },
        { .name = "stop-sigterm",               .enabled = false },
        { .name = "stop-sigkill",               .enabled = false },
        { .name = "stop-post",                  .enabled = false },
        { .name = "final-watchdog",             .enabled = false },
        { .name = "final-sigterm",              .enabled = false },
        { .name = "final-sigkill",              .enabled = false },
        { .name = "failed",                     .enabled = false },
        { .name = "dead-before-auto-restart",   .enabled = false },
        { .name = "failed-before-auto-restart", .enabled = false },
        { .name = "dead-resources-pinned",      .enabled = false },
        { .name = "auto-restart",               .enabled = false },
        { .name = "auto-restart-queued",        .enabled = false },
        { .name = "cleaning",                   .enabled = false },
    };
    state_set_t sub_set = { .num = STATIC_ARRAY_SIZE(sub_states), .ptr = sub_states };
    for (size_t j = 0; j < sub_set.num ; j++) {
        if (strcmp(sub_set.ptr[j].name, sub_state) == 0) {
            sub_set.ptr[j].enabled = true;
            break;
        }
    }
    metric_family_append(&fam[FAM_SYSTEMD_UNIT_SUB_STATE], VALUE_STATE_SET(sub_set), NULL,
                         &(label_pair_const_t){.name="unit", .value=unit}, NULL);


    double start_time = 0.0;
    if (strcmp(active_state, "active") == 0) {
        uint64_t timestamp = 0;
        int status = get_property_uint64(bus, "org.freedesktop.systemd1", unit_path,
                                              "org.freedesktop.systemd1.Unit",
                                              "ActiveEnterTimestamp", &timestamp);
        if (status == 0)
            start_time = (double)timestamp/1e6;
    }

    metric_family_append(&fam[FAM_SYSTEMD_UNIT_START_TIME_SECONDS], VALUE_GAUGE(start_time), NULL,
                         &(label_pair_const_t){.name="unit", .value=unit}, NULL);


    if (strendswith(unit, ".service")) {
        unit_service(bus, unit, unit_path);
    } else if (strendswith(unit, ".socket")) {
        unit_socket(bus, unit, unit_path);
    } else if (strendswith(unit, ".timer")) {
        unit_timer(bus, unit, unit_path);
    }

    return 0;
}

static int systemd_read(void)
{
    sd_bus *bus = NULL;
    sd_bus_message *reply = NULL;

    if (sd_booted() <= 0)
        return -1;

    if (geteuid() != 0) {
        sd_bus_default_system(&bus);
    } else {
        int status = sd_bus_new(&bus);
        if (status < 0)
          return -1;

        status = sd_bus_set_address(bus, "unix:path=/run/systemd/private");
        if (status < 0)
           return -1;

        status = sd_bus_start(bus);
        if (status < 0)
           sd_bus_default_system(&bus);
    }

    sd_bus_error error = SD_BUS_ERROR_NULL;

    if (units_size == 0) {
        int status = sd_bus_call_method(bus, "org.freedesktop.systemd1",
                                             "/org/freedesktop/systemd1",
                                             "org.freedesktop.systemd1.Manager",
                                             "ListUnits", &error, &reply, NULL);
        if (status < 0)
           return -1;
    } else {
        sd_bus_message *m = NULL;
        int status = sd_bus_message_new_method_call(bus, &m, "org.freedesktop.systemd1",
                                                             "/org/freedesktop/systemd1",
                                                             "org.freedesktop.systemd1.Manager",
                                                             "ListUnitsByNames");
        if (status < 0)
            return -1;

        status = sd_bus_message_append_strv(m, units);
        if (status < 0) {
            sd_bus_message_unref(m);
            return -1;
        }

        status = sd_bus_call(bus, m, 0, &error, &reply);
        if (status < 0) {
            sd_bus_message_unref(m);
            return -1;
        }

        sd_bus_message_unref(m);
    }

    sd_bus_error_free(&error);

    int status = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(ssssssouso)");
    if (status < 0) {
        sd_bus_message_unref(reply);
        sd_bus_unref(bus);
        return -1;
    }

    while (true) {
        char *unit = NULL;
        char *load_state = NULL;
        char *active_state = NULL;
        char *sub_state = NULL;
        char *unit_path = NULL;

        status = sd_bus_message_read(reply, "(ssssssouso)", &unit, NULL, &load_state,
                                                            &active_state, &sub_state, NULL,
                                                            &unit_path, NULL, NULL, NULL);
        if (status <= 0)
          break;

        submit_unit(bus, unit, unit_path, load_state, active_state, sub_state);
    }

    sd_bus_message_exit_container(reply);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);

    plugin_dispatch_metric_family_array(fam, FAM_SYSTEMD_MAX, 0);

    return 0;
}

static int systemd_config_append_unit(config_item_t *ci, char *suffix)
{
    if (ci == NULL)
        return -1;

    if (ci->values_num <= 0) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires a list of strings.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    for (int i=0 ; i < ci->values_num; i++) {
        if (ci->values[i].type != CONFIG_TYPE_STRING) {
            PLUGIN_ERROR("The argument %d in option '%s' at %s:%d must be a string.",
                         i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }
    }

    size_t new_size = units_size + ci->values_num + 1;

    char **tmp = realloc(units, sizeof(char *)*new_size);
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed");
        return -1;
    }

    units = tmp;

    for (int i=0 ; i < ci->values_num; i++) {
        units[units_size + i] = NULL;
    }

    size_t old_units_size = units_size;
    units_size = units_size + ci->values_num;

    for (int i=0 ; i < ci->values_num; i++) {
        if (suffix != NULL) {
            if (strendswith(ci->values[i].value.string, suffix)) {
                char *str = strdup(ci->values[i].value.string);
                if (str == NULL) {
                    PLUGIN_ERROR("strdup failed");
                    return -1;
                }
                units[old_units_size + i] = str;
            } else {
                size_t len = strlen(ci->values[i].value.string);
                size_t suffix_len = strlen(suffix);
                char *str = malloc(len + suffix_len + 1);
                if (str == NULL) {
                    PLUGIN_ERROR("strdup failed");
                    return -1;
                }
                ssnprintf(str, len + suffix_len + 1, "%s%s", ci->values[i].value.string, suffix);
                units[old_units_size + i] = str;
            }
        } else {
            char *str = strdup(ci->values[i].value.string);
            if (str == NULL) {
                PLUGIN_ERROR("strdup failed");
                return -1;
            }
            units[old_units_size + i] = str;
        }
    }

    units[units_size] = NULL;

    return 0;
}

static int systemd_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "unit") == 0) {
            status = systemd_config_append_unit(child, NULL);
        } else if (strcasecmp(child->key, "service") == 0) {
            status = systemd_config_append_unit(child, ".service");
        } else if (strcasecmp(child->key, "socket") == 0) {
            status = systemd_config_append_unit(child, ".socket");
        } else if (strcasecmp(child->key, "timer") == 0) {
            status = systemd_config_append_unit(child, ".timer");
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0)
        return -1;

    return 0;
}

static int systemd_shutdown(void)
{
    if (units == NULL)
        return 0;

    for (size_t i = 0; i < units_size; i++) {
        free(units[i]);
    }

    free(units);

    units_size = 0;
    units = NULL;

    return 0;
}

void module_register(void)
{
    plugin_register_config("systemd", systemd_config);
    plugin_register_read("systemd", systemd_read);
    plugin_register_shutdown("systemd", systemd_shutdown);
}
