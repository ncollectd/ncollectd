// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2008-2009 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008 Peter Holik
// SPDX-FileCopyrightText: Copyright (C) 2009 Bruno Prémont
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Peter Holik <peter at holik.at>
// SPDX-FileContributor: Bruno Prémont <bonbons at linux-vserver.org>
// SPDX-FileContributor: Pavel Rochnyak <pavel2000 ngs.ru>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include <OpenIPMI/ipmi_auth.h>
#include <OpenIPMI/ipmi_conn.h>
#include <OpenIPMI/ipmi_err.h>
#include <OpenIPMI/ipmi_lan.h>
#include <OpenIPMI/ipmi_posix.h>
#include <OpenIPMI/ipmi_smi.h>
#include <OpenIPMI/ipmiif.h>

#define ERR_BUF_SIZE 1024

enum {
    FAM_IPMI_TEMPERATURE_CELSIUS,
    FAM_IPMI_TEMPERATURE_STATE,
    FAM_IPMI_FAN_SPEED_RPM,
    FAM_IPMI_FAN_SPEED_STATE,
    FAM_IPMI_VOLTAGE_VOLTS,
    FAM_IPMI_VOLTAGE_STATE,
    FAM_IPMI_CURRENT_AMPERES,
    FAM_IPMI_CURRENT_STATE,
    FAM_IPMI_POWER_WATTS,
    FAM_IPMI_POWER_STATE,
    FAM_IPMI_SENSOR_VALUE,
    FAM_IPMI_SENSOR_STATE,
    FAM_IPMI_MAX,
};

static metric_family_t fams[FAM_IPMI_MAX] = {
    [FAM_IPMI_TEMPERATURE_CELSIUS] = {
        .name = "ipmi_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = "Temperature reading in degree Celsius.",
    },
    [FAM_IPMI_TEMPERATURE_STATE] = {
        .name = "ipmi_temperature_state",
        .type = METRIC_TYPE_GAUGE,
        .help = "Reported state of a temperature sensor (0=nominal, 1=warning, 2=critical).",
    },
    [FAM_IPMI_VOLTAGE_VOLTS] = {
        .name = "ipmi_voltage_volts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Voltage reading in Volts.",
    },
    [FAM_IPMI_VOLTAGE_STATE] = {
        .name = "ipmi_voltage_state",
        .type = METRIC_TYPE_GAUGE,
        .help = "Reported state of a voltage sensor (0=nominal, 1=warning, 2=critical).",
    },
    [FAM_IPMI_CURRENT_AMPERES] = {
        .name = "ipmi_current_amperes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current reading in Amperes.",
    },
    [FAM_IPMI_CURRENT_STATE] = {
        .name = "ipmi_current_state",
        .type = METRIC_TYPE_GAUGE,
        .help = "Reported state of a current sensor (0=nominal, 1=warning, 2=critical).",
    },
    [FAM_IPMI_FAN_SPEED_RPM] = {
        .name = "ipmi_fan_speed_rpm",
        .type = METRIC_TYPE_GAUGE,
        .help = "Fan speed in rotations per minute.",
    },
    [FAM_IPMI_FAN_SPEED_STATE] = {
        .name = "ipmi_fan_speed_state",
        .type = METRIC_TYPE_GAUGE,
        .help = "Reported state of a fan speed sensor (0=nominal, 1=warning, 2=critical).",
    },
    [FAM_IPMI_POWER_WATTS] = {
        .name = "ipmi_power_watts",
        .type = METRIC_TYPE_GAUGE,
        .help = "Power reading in Watts.",
    },
    [FAM_IPMI_POWER_STATE] = {
        .name = "ipmi_power_state",
        .type = METRIC_TYPE_GAUGE,
        .help = "Reported state of a power sensor (0=nominal, 1=warning, 2=critical).",
    },
    [FAM_IPMI_SENSOR_VALUE] = {
        .name = "ipmi_sensor_value",
        .type = METRIC_TYPE_GAUGE,
        .help = "Generic data read from an IPMI sensor of unknown type, relying on labels for context.",
    },
    [FAM_IPMI_SENSOR_STATE] = {
        .name = "ipmi_sensor_state",
        .type = METRIC_TYPE_GAUGE,
        .help = "Indicates the severity of the state reported by an IPMI sensor (0=nominal, 1=warning, 2=critical).",
    },
};

#if 0
 IPMI_SENSOR_TYPE_TEMPERATURE
 IPMI_SENSOR_TYPE_VOLTAGE
 IPMI_SENSOR_TYPE_CURRENT
 IPMI_SENSOR_TYPE_FAN

 IPMI_SENSOR_TYPE_MEMORY

 IPMI_SENSOR_TYPE_POWER_SUPPLY
 IPMI_SENSOR_TYPE_POWER_UNIT

 IPMI_SENSOR_TYPE_PHYSICAL_SECURITY
 IPMI_SENSOR_TYPE_PLATFORM_SECURITY
 IPMI_SENSOR_TYPE_PROCESSOR
 IPMI_SENSOR_TYPE_COOLING_DEVICE
 IPMI_SENSOR_TYPE_OTHER_UNITS_BASED_SENSOR
 IPMI_SENSOR_TYPE_DRIVE_SLOT
 IPMI_SENSOR_TYPE_POWER_MEMORY_RESIZE
 IPMI_SENSOR_TYPE_SYSTEM_FIRMWARE_PROGRESS
 IPMI_SENSOR_TYPE_EVENT_LOGGING_DISABLED
 IPMI_SENSOR_TYPE_WATCHDOG_1
 IPMI_SENSOR_TYPE_SYSTEM_EVENT
 IPMI_SENSOR_TYPE_CRITICAL_INTERRUPT
 IPMI_SENSOR_TYPE_BUTTON
 IPMI_SENSOR_TYPE_MODULE_BOARD
 IPMI_SENSOR_TYPE_MICROCONTROLLER_COPROCESSOR
 IPMI_SENSOR_TYPE_ADD_IN_CARD
 IPMI_SENSOR_TYPE_CHASSIS
 IPMI_SENSOR_TYPE_CHIP_SET
 IPMI_SENSOR_TYPE_OTHER_FRU
 IPMI_SENSOR_TYPE_CABLE_INTERCONNECT
 IPMI_SENSOR_TYPE_TERMINATOR
 IPMI_SENSOR_TYPE_SYSTEM_BOOT_INITIATED
 IPMI_SENSOR_TYPE_BOOT_ERROR
 IPMI_SENSOR_TYPE_OS_BOOT
 IPMI_SENSOR_TYPE_OS_CRITICAL_STOP
 IPMI_SENSOR_TYPE_SLOT_CONNECTOR
 IPMI_SENSOR_TYPE_SYSTEM_ACPI_POWER_STATE
 IPMI_SENSOR_TYPE_WATCHDOG_2
 IPMI_SENSOR_TYPE_PLATFORM_ALERT
 IPMI_SENSOR_TYPE_ENTITY_PRESENCE
 IPMI_SENSOR_TYPE_MONITOR_ASIC_IC
 IPMI_SENSOR_TYPE_LAN
 IPMI_SENSOR_TYPE_MANAGEMENT_SUBSYSTEM_HEALTH
 IPMI_SENSOR_TYPE_BATTERY
 IPMI_SENSOR_TYPE_SESSION_AUDIT
 IPMI_SENSOR_TYPE_VERSION_CHANGE
 IPMI_SENSOR_TYPE_FRU_STATE
#endif
#if 0
The state of a sensor can be one of nominal, warning, critical, or N/A, reflected by the metric values 0, 1, 2, and NaN respectively. Think of this as a kind of severity.
"nominal"
"warning"
"critical"
"n/a"

ipmi_temperature_celsius{id="18",name="Inlet Temp"} 24
ipmi_temperature_state{id="18",name="Inlet Temp"} 0

ipmi_fan_speed_rpm{id="12",name="Fan1A"} 4560
ipmi_fan_speed_state{id="12",name="Fan1A"} 0

ipmi_voltage_state{id="2416",name="12V"} 0
ipmi_voltage_volts{id="2416",name="12V"} 12

ipmi_current_state{id="83",name="Current 1"} 0
ipmi_current_amperes{id="83",name="Current 1"} 0

ipmi_power_state{id="90",name="Pwr Consumption"} 0
ipmi_power_watts{id="90",name="Pwr Consumption"} 70

ipmi_sensor_state{id="139",name="Power Cable",type="Cable/Interconnect"} 0
ipmi_sensor_value{id="139",name="Power Cable",type="Cable/Interconnect"} NaN
#endif

struct c_ipmi_sensor_list_s;
typedef struct c_ipmi_sensor_list_s c_ipmi_sensor_list_t;

struct c_ipmi_instance_s {
    char *name;

    exclist_t excl_sensor;
    exclist_t excl_sel_sensor;

    bool notify_add;
    bool notify_remove;
    bool notify_notpresent;
    bool notify_conn;
    bool sel_enabled;
    bool sel_clear_event;

    char *host;
    char *connaddr;
    char *username;
    char *password;
    unsigned int authtype;

    bool connected;
    ipmi_con_t *connection;
    pthread_mutex_t sensor_list_lock;
    c_ipmi_sensor_list_t *sensor_list;

    int init_in_progress;

    label_set_t labels;
    metric_family_t fams[FAM_IPMI_MAX];
};
typedef struct c_ipmi_instance_s c_ipmi_instance_t;

struct c_ipmi_sensor_list_s {
    ipmi_sensor_id_t sensor_id;
    char sensor_name[DATA_MAX_NAME_LEN];
    int sensor_type;
    char type_instance[DATA_MAX_NAME_LEN];
    int sensor_not_present;
    c_ipmi_instance_t *instance;
    unsigned int use;
    c_ipmi_sensor_list_t *next;
};

struct c_ipmi_db_type_map_s {
    enum ipmi_unit_type_e type;
    const char *type_name;
};
typedef struct c_ipmi_db_type_map_s c_ipmi_db_type_map_t;

static os_handler_t *os_handler;
static bool os_handler_active;
static pthread_t os_handler_thread_id;

#define STRERRIPMI(x) strerripmi((x), (char[ERRBUF_SIZE]){0}, ERRBUF_SIZE)

static const char *strerripmi(unsigned int err, char *errbuf, size_t errbuf_size)
{
    if (IPMI_IS_OS_ERR(err) || IPMI_IS_RMCPP_ERR(err) || IPMI_IS_IPMI_ERR(err))
        ipmi_get_error_string(err, errbuf, errbuf_size);

    if (errbuf[0] == 0)
        ssnprintf(errbuf, errbuf_size, "Unknown error %#x", err);

    errbuf[errbuf_size - 1] = '\0';

    return errbuf;
}

static void c_ipmi_log(__attribute__((unused)) os_handler_t *handler, const char *format,
                       enum ipmi_log_type_e log_type, va_list ap)
{
    char msg[ERR_BUF_SIZE];

    vsnprintf(msg, sizeof(msg), format, ap);

    switch (log_type) {
        case IPMI_LOG_INFO:
            PLUGIN_INFO("%s", msg);
            break;
        case IPMI_LOG_WARNING:
            PLUGIN_NOTICE("%s", msg);
            break;
        case IPMI_LOG_SEVERE:
            PLUGIN_WARNING("%s", msg);
            break;
        case IPMI_LOG_FATAL:
            PLUGIN_ERROR("%s", msg);
            break;
        case IPMI_LOG_ERR_INFO:
            PLUGIN_ERROR("%s", msg);
            break;
#ifdef NCOLLECTD_DEBUG
        case IPMI_LOG_DEBUG_START:
        case IPMI_LOG_DEBUG:
            PLUGIN_DEBUG("%s", msg);
            break;
        case IPMI_LOG_DEBUG_CONT:
        case IPMI_LOG_DEBUG_END:
            PLUGIN_DEBUG("%s", msg);
            break;
#else
        case IPMI_LOG_DEBUG_START:
        case IPMI_LOG_DEBUG:
        case IPMI_LOG_DEBUG_CONT:
        case IPMI_LOG_DEBUG_END:
            break;
#endif
    }
}

/* Prototype for sensor_list_remove, so sensor_read_handler can call it. */
static int sensor_list_remove(c_ipmi_instance_t *st, ipmi_sensor_t *sensor);

static void sensor_read_handler(ipmi_sensor_t *sensor, int err,
                                enum ipmi_value_present_e value_present,
                                unsigned int __attribute__((unused)) raw_value,
                                double value, ipmi_states_t *states,
                                void *user_data)
{
    c_ipmi_sensor_list_t *list_item = user_data;
    c_ipmi_instance_t *st = list_item->instance;

    list_item->use--;

    if (err != 0) {
        if (IPMI_IS_IPMI_ERR(err) && IPMI_GET_IPMI_ERR(err) == IPMI_NOT_PRESENT_CC) {
            if (list_item->sensor_not_present == 0) {
                list_item->sensor_not_present = 1;

                PLUGIN_INFO("sensor_read_handler: sensor `%s` of `%s` not present.",
                         list_item->sensor_name, st->name);

                if (st->notify_notpresent) {
                    notification_t n = {
                        .severity = NOTIF_WARNING,
                        .time = cdtime(),
                        .name = "ipmi_sensor",
                    };

                    if (st->host != NULL)
                        notification_label_set(&n, "host", st->host);

//                  sstrncpy(n.type_instance, list_item->type_instance, sizeof(n.type_instance)); // FIXME
//                  sstrncpy(n.type, list_item->sensor_type, sizeof(n.type));
//                  notification_label_set(&n, "sensor_type", list_item->sensor_type);

                    char message[1024];
                    ssnprintf(message, sizeof(message), "sensor %s not present",
                                                        list_item->sensor_name);
                    notification_annotation_set(&n, "summary", message);

                    plugin_dispatch_notification(&n);
                }
            }
        } else if (IPMI_IS_IPMI_ERR(err) &&
                   (IPMI_GET_IPMI_ERR(err) == IPMI_NOT_SUPPORTED_IN_PRESENT_STATE_CC)) {
            PLUGIN_INFO("sensor_read_handler: Sensor `%s` of `%s` not ready.",
                        list_item->sensor_name, st->name);
        } else if (IPMI_IS_IPMI_ERR(err) && (IPMI_GET_IPMI_ERR(err) == IPMI_TIMEOUT_CC)) {
            PLUGIN_INFO("sensor_read_handler: Sensor `%s` of `%s` timed out.",
                        list_item->sensor_name, st->name);
        } else {
            char errbuf[ERR_BUF_SIZE] = {0};
            ipmi_get_error_string(err, errbuf, sizeof(errbuf) - 1);

            if (IPMI_IS_IPMI_ERR(err)) {
                PLUGIN_INFO("sensor_read_handler: Sensor `%s` of `%s` failed: %s.",
                            list_item->sensor_name, st->name, errbuf);
            } else if (IPMI_IS_OS_ERR(err)) {
                PLUGIN_INFO("sensor_read_handler: Sensor `%s` of `%s` failed: %s (%#x).",
                            list_item->sensor_name, st->name,
                            errbuf, (unsigned int)IPMI_GET_OS_ERR(err));
            } else if (IPMI_IS_RMCPP_ERR(err)) {
                PLUGIN_INFO("sensor_read_handler: Sensor `%s` of `%s` failed: %s.",
                            list_item->sensor_name, st->name, errbuf);
            } else if (IPMI_IS_SOL_ERR(err)) {
                PLUGIN_INFO("sensor_read_handler: Sensor `%s` of `%s` failed: %s (%#x).",
                            list_item->sensor_name, st->name,
                            errbuf, (unsigned int)IPMI_GET_SOL_ERR(err));
            } else {
                PLUGIN_INFO("sensor_read_handler: Sensor `%s` of `%s` failed "
                            "with error %#x. of class %#x",
                            list_item->sensor_name, st->name,
                            (unsigned int)err & 0xff, (unsigned int)err & 0xffffff00);
            }
        }
        return;
    } else if (list_item->sensor_not_present == 1) {
        list_item->sensor_not_present = 0;

        PLUGIN_INFO("sensor_read_handler: sensor `%s` of `%s` present.",
                    list_item->sensor_name, st->name);

        if (st->notify_notpresent) {
            notification_t n = {
                .severity = NOTIF_OKAY,
                .time = cdtime(),
                .name = "ipmi_sensor",
            };

            if (st->host != NULL)
                notification_label_set(&n, "host", st->host);

//          sstrncpy(n.type_instance, list_item->type_instance, sizeof(n.type_instance)); FIXME
//          sstrncpy(n.type, list_item->sensor_type, sizeof(n.type));
            char message[1024];
            ssnprintf(message, sizeof(message), "sensor %s present", list_item->sensor_name);
            notification_annotation_set(&n, "summary", message);

            plugin_dispatch_notification(&n);
        }
    }

    if (value_present != IPMI_BOTH_VALUES_PRESENT) {
        PLUGIN_INFO("sensor_read_handler: Removing sensor `%s` of `%s`, "
                    "because it provides %s. If you need this sensor, "
                    "please file a bug report.",
                    list_item->sensor_name, st->name,
                    (value_present == IPMI_RAW_VALUE_PRESENT) ? "only the raw value" : "no value");
        sensor_list_remove(st, sensor);
        return;
    }

    if (!ipmi_is_sensor_scanning_enabled(states)) {
        PLUGIN_DEBUG("sensor_read_handler: Skipping sensor `%s` of `%s`, "
                     "it is in 'scanning disabled' state.",
                     list_item->sensor_name, st->name);
        return;
    }

    if (ipmi_is_initial_update_in_progress(states)) {
        PLUGIN_DEBUG("sensor_read_handler: Skipping sensor `%s` of `%s`, "
                     "it is in 'initial update in progress' state.",
                     list_item->sensor_name, st->name);
        return;
    }

    int sensor_type = ipmi_sensor_get_sensor_type(sensor);

    metric_family_t *fam = NULL;

    switch (sensor_type) {
        case IPMI_SENSOR_TYPE_TEMPERATURE:
            fam = &st->fams[FAM_IPMI_TEMPERATURE_CELSIUS];
// FAM_IPMI_TEMPERATURE_STATE,
            break;
        case IPMI_SENSOR_TYPE_VOLTAGE:
            fam = &st->fams[FAM_IPMI_VOLTAGE_VOLTS];
// FAM_IPMI_VOLTAGE_STATE,
            break;
        case IPMI_SENSOR_TYPE_CURRENT:
            fam = &st->fams[FAM_IPMI_CURRENT_AMPERES];
// FAM_IPMI_CURRENT_STATE,
            break;
        case IPMI_SENSOR_TYPE_FAN:
            fam = &st->fams[FAM_IPMI_FAN_SPEED_RPM];
// FAM_IPMI_FAN_SPEED_STATE,
            break;
#if 0
        case IPMI_SENSOR_TYPE_MEMORY:
            type = "memory";
            fam = &st->fams[FAM_IPMI_SENSOR_VALUE];
// FAM_IPMI_SENSOR_STATE,
            break;
#endif
        default:
            fam = &st->fams[FAM_IPMI_SENSOR_VALUE];
            break;
    }

    metric_t m = {0};

    m.value = VALUE_GAUGE(value);

    if (st->host != NULL)
        metric_label_set(&m, "host", st->host);

    metric_family_metric_append(fams, m);
    metric_reset(&m, METRIC_TYPE_GAUGE);
//  sstrncpy(vl.plugin, "ipmi", sizeof(vl.plugin));
//  sstrncpy(vl.type, list_item->sensor_type, sizeof(vl.type));
//  sstrncpy(vl.type_instance, list_item->type_instance, sizeof(vl.type_instance));
    plugin_dispatch_metric_family(fam, 0);
}

static void sensor_get_name(ipmi_sensor_t *sensor, char *buffer, int buf_len)
{
    char temp[DATA_MAX_NAME_LEN] = {0};
    ipmi_entity_t *ent = ipmi_sensor_get_entity(sensor);
    const char *entity_id_string = ipmi_entity_get_entity_id_string(ent);
    char sensor_name[DATA_MAX_NAME_LEN] = "";
    char *sensor_name_ptr;

    if ((buffer == NULL) || (buf_len == 0))
        return;

    ipmi_sensor_get_name(sensor, temp, sizeof(temp));
    temp[sizeof(temp) - 1] = '\0';

    if (entity_id_string != NULL && strlen(temp))
        ssnprintf(sensor_name, sizeof(sensor_name), "%s %s", temp, entity_id_string);
    else if (entity_id_string != NULL)
        sstrncpy(sensor_name, entity_id_string, sizeof(sensor_name));
    else
        sstrncpy(sensor_name, temp, sizeof(sensor_name));

    if (strlen(temp)) {
        sstrncpy(temp, sensor_name, sizeof(temp));
        sensor_name_ptr = strstr(temp, ").");
        if (sensor_name_ptr != NULL) {
            /* If name is something like "foo (123).bar",
             * change that to "bar (123)".
             * Both, sensor_name_ptr and sensor_id_ptr point to memory within the
             * `temp' array, which holds a copy of the current `sensor_name'. */
            char *sensor_id_ptr;

            /* `sensor_name_ptr' points to ").bar". */
            sensor_name_ptr[1] = 0;
            /* `temp' holds "foo (123)\0bar\0". */
            sensor_name_ptr += 2;
            /* `sensor_name_ptr' now points to "bar". */

            sensor_id_ptr = strstr(temp, "(");
            if (sensor_id_ptr != NULL) {
                /* `sensor_id_ptr' now points to "(123)". */
                ssnprintf(sensor_name, sizeof(sensor_name), "%s %s", sensor_name_ptr,
                                    sensor_id_ptr);
            }
            /* else: don't touch sensor_name. */
        }
    }
    sstrncpy(buffer, sensor_name, buf_len);
}

#if 0
static const char *sensor_unit_to_type(ipmi_sensor_t *sensor)
{
    static const c_ipmi_db_type_map_t ipmi_db_type_map[] = {
            {IPMI_UNIT_TYPE_WATTS, "power"}, {IPMI_UNIT_TYPE_CFM, "flow"}};

    /* check the modifier and rate of the sensor value */
    if ((ipmi_sensor_get_modifier_unit_use(sensor) != IPMI_MODIFIER_UNIT_NONE) ||
            (ipmi_sensor_get_rate_unit(sensor) != IPMI_RATE_UNIT_NONE))
        return NULL;

    /* find the db type by using sensor base unit type */
    enum ipmi_unit_type_e ipmi_type = ipmi_sensor_get_base_unit(sensor);
    for (size_t i = 0; i < STATIC_ARRAY_SIZE(ipmi_db_type_map); i++)
        if (ipmi_db_type_map[i].type == ipmi_type)
            return ipmi_db_type_map[i].type_name;

    return NULL;
}
#endif

static int sensor_list_add(c_ipmi_instance_t *st, ipmi_sensor_t *sensor)
{
    ipmi_sensor_id_t sensor_id;
    c_ipmi_sensor_list_t *list_item;
    c_ipmi_sensor_list_t *list_prev;

    char buffer[DATA_MAX_NAME_LEN] = {0};
    char *sensor_name_ptr = buffer;
    int sensor_type;

    sensor_id = ipmi_sensor_convert_to_id(sensor);
    sensor_get_name(sensor, buffer, sizeof(buffer));

    PLUGIN_DEBUG("sensor_list_add: Found sensor `%s` of `%s`,"
                " Type: %#x"
                " Event reading type: %#x"
                " Direction: %#x"
                " Event support: %#x",
                sensor_name_ptr, st->name, (unsigned int)ipmi_sensor_get_sensor_type(sensor),
                (unsigned int)ipmi_sensor_get_event_reading_type(sensor),
                (unsigned int)ipmi_sensor_get_sensor_direction(sensor),
                (unsigned int)ipmi_sensor_get_event_support(sensor));

    /* Both `ignorelist' and `sensor_name_ptr' may be NULL. */
    if (!exclist_match(&st->excl_sensor, sensor_name_ptr))
        return 0;

    /* FIXME: Use rate unit or base unit to scale the value */

    sensor_type = ipmi_sensor_get_sensor_type(sensor);

    /*
     * ipmitool/lib/ipmi_sdr.c sdr_sensor_has_analog_reading() has a notice
     * about 'Threshold sensors' and 'analog readings'. Discrete sensor may
     * have analog data, but discrete sensors support is not implemented
     * in Collectd yet.
     *
     * ipmi_sensor_id_get_reading() supports only 'Threshold' sensors.
     * See lib/sensor.c:4842, stand_ipmi_sensor_get_reading() for details.
     */
    if (!ipmi_sensor_get_is_readable(sensor)) {
        PLUGIN_INFO("sensor_list_add: Ignore sensor `%s` of `%s`, "
                 "because it isn't readable! Its type: (%#x, %s). ",
                 sensor_name_ptr, st->name, (unsigned int)sensor_type,
                 ipmi_sensor_get_sensor_type_string(sensor));
        return -1;
    }

    if (ipmi_sensor_get_event_reading_type(sensor) != IPMI_EVENT_READING_TYPE_THRESHOLD) {
        PLUGIN_INFO("sensor_list_add: Ignore sensor `%s` of `%s`, "
                 "because it is discrete (%#x)! Its type: (%#x, %s). ",
                 sensor_name_ptr, st->name, (unsigned int)sensor_type,
                 (unsigned int)ipmi_sensor_get_event_reading_type(sensor),
                 ipmi_sensor_get_sensor_type_string(sensor));
        return -1;
    }

    pthread_mutex_lock(&st->sensor_list_lock);

    list_prev = NULL;
    for (list_item = st->sensor_list; list_item != NULL; list_item = list_item->next) {
        if (ipmi_cmp_sensor_id(sensor_id, list_item->sensor_id) == 0)
            break;
        list_prev = list_item;
    }

    if (list_item != NULL) {
        pthread_mutex_unlock(&st->sensor_list_lock);
        return 0;
    }

    list_item = calloc(1, sizeof(*list_item));
    if (list_item == NULL) {
        pthread_mutex_unlock(&st->sensor_list_lock);
        return -1;
    }

    list_item->instance = st;
    list_item->sensor_id = ipmi_sensor_convert_to_id(sensor);

    if (list_prev != NULL)
        list_prev->next = list_item;
    else
        st->sensor_list = list_item;

//  sstrncpy(list_item->sensor_name, sensor_name_ptr, sizeof(list_item->sensor_name)); FIXME
    list_item->sensor_type = sensor_type;

    pthread_mutex_unlock(&st->sensor_list_lock);

    if (st->notify_add && (st->init_in_progress == 0)) {
        notification_t n = {
            .severity = NOTIF_OKAY,
            .time = cdtime(),
            .name = "ipmi_",
        };

        if (st->host != NULL)
            notification_label_set(&n, "host", st->host);

//      sstrncpy(n.type_instance, list_item->type_instance, sizeof(n.type_instance)); FIXME
//      sstrncpy(n.type, list_item->sensor_type, sizeof(n.type));
        char message[1024];
        ssnprintf(message, sizeof(message), "sensor %s added", list_item->sensor_name);
        notification_annotation_set(&n, "summary", message);

        plugin_dispatch_notification(&n);
    }

    return 0;
}

static int sensor_list_remove(c_ipmi_instance_t *st, ipmi_sensor_t *sensor)
{
    ipmi_sensor_id_t sensor_id = ipmi_sensor_convert_to_id(sensor);

    pthread_mutex_lock(&st->sensor_list_lock);

    c_ipmi_sensor_list_t *list_prev = NULL;
    c_ipmi_sensor_list_t *list_item = NULL;
    for (list_item = st->sensor_list; list_item != NULL; list_item = list_item->next) {
        if (ipmi_cmp_sensor_id(sensor_id, list_item->sensor_id) == 0)
            break;
        list_prev = list_item;
    }

    if (list_item == NULL) {
        pthread_mutex_unlock(&st->sensor_list_lock);
        return -1;
    }

    if (list_prev == NULL)
        st->sensor_list = list_item->next;
    else
        list_prev->next = list_item->next;

    list_prev = NULL;
    list_item->next = NULL;

    pthread_mutex_unlock(&st->sensor_list_lock);

    if (st->notify_remove && os_handler_active) {
        notification_t n = {
            .severity = NOTIF_WARNING,
            .time = cdtime(),
            .name = "ipmi_sensor",
        };

//      sstrncpy(n.type_instance, list_item->type_instance, sizeof(n.type_instance)); FIXME
//      sstrncpy(n.type, list_item->sensor_type, sizeof(n.type));
//      ssnprintf(n.message, sizeof(n.message), "sensor %s removed", list_item->sensor_name);

        char message[1024];
        ssnprintf(message, sizeof(message), "sensor %s removed", list_item->sensor_name);
        notification_annotation_set(&n, "summary", message);

        plugin_dispatch_notification(&n);
    }

    free(list_item);
    return 0;
}

static int sensor_list_read_all(c_ipmi_instance_t *st)
{
    pthread_mutex_lock(&st->sensor_list_lock);

    for (c_ipmi_sensor_list_t *list_item = st->sensor_list; list_item != NULL;
             list_item = list_item->next) {
        PLUGIN_DEBUG("try read sensor `%s` of `%s`, use: %u",
                     list_item->sensor_name, st->name, list_item->use);

        /* Reading already initiated */
        if (list_item->use)
            continue;

        list_item->use++;
        ipmi_sensor_id_get_reading(list_item->sensor_id, sensor_read_handler,
                                                             /* user data = */ (void *)list_item);
    }

    pthread_mutex_unlock(&st->sensor_list_lock);

    return 0;
}

static int sensor_list_remove_all(c_ipmi_instance_t *st)
{
    c_ipmi_sensor_list_t *list_item;

    pthread_mutex_lock(&st->sensor_list_lock);

    list_item = st->sensor_list;
    st->sensor_list = NULL;

    pthread_mutex_unlock(&st->sensor_list_lock);

    while (list_item != NULL) {
        c_ipmi_sensor_list_t *list_next = list_item->next;

        free(list_item);

        list_item = list_next;
    }

    return 0;
}

static int sensor_convert_threshold_severity(enum ipmi_thresh_e severity)
{
    switch (severity) {
        case IPMI_LOWER_NON_CRITICAL:
        case IPMI_UPPER_NON_CRITICAL:
            return NOTIF_OKAY;
        case IPMI_LOWER_CRITICAL:
        case IPMI_UPPER_CRITICAL:
            return NOTIF_WARNING;
        case IPMI_LOWER_NON_RECOVERABLE:
        case IPMI_UPPER_NON_RECOVERABLE:
            return NOTIF_FAILURE;
        default:
            return NOTIF_OKAY;
    }
}

static void add_event_common_data(__attribute__((unused)) notification_t *n,
                                  __attribute__((unused)) ipmi_sensor_t *sensor,
                                  __attribute__((unused)) enum ipmi_event_dir_e dir,
                                  __attribute__((unused)) ipmi_event_t *event)
{
#if 0
    ipmi_entity_t *ent = ipmi_sensor_get_entity(sensor);

    plugin_notification_meta_add_string(n, "entity_name", ipmi_entity_get_entity_id_string(ent));
    plugin_notification_meta_add_signed_int(n, "entity_id", ipmi_entity_get_entity_id(ent));
    plugin_notification_meta_add_signed_int(n, "entity_instance", ipmi_entity_get_entity_instance(ent));
    plugin_notification_meta_add_boolean(n, "assert", dir == IPMI_ASSERTION);

    if (event)
        plugin_notification_meta_add_signed_int(n, "event_type", ipmi_event_get_type(event));
#endif
}

static int sensor_threshold_event_handler(
        ipmi_sensor_t *sensor, enum ipmi_event_dir_e dir,
        enum ipmi_thresh_e threshold, enum ipmi_event_value_dir_e high_low,
        enum ipmi_value_present_e value_present, unsigned int raw_value,
        double value, void *cb_data, ipmi_event_t *event)
{

    c_ipmi_instance_t *st = cb_data;

    /* From the IPMI specification Chapter 2: Events.
     * If a callback handles the event, then all future callbacks called due to
     * the event will receive a NULL for the event. So be ready to handle a NULL
     * event in all your event handlers. A NULL may also be passed to an event
     * handler if the callback was not due to an event. */
    if (event == NULL)
        return IPMI_EVENT_NOT_HANDLED;

    notification_t n = {
        .severity = NOTIF_OKAY,
        .time = cdtime(),
        .name = "ipmi_", // FIXME
    };

    if (st->host != NULL)
        notification_label_set(&n, "host", st->host);

    /* offset is a table index and it's represented as enum of strings that are
         organized in the way - high and low for each threshold severity level */
    unsigned int offset = (2 * threshold) + high_low;
    unsigned int event_type = ipmi_sensor_get_event_reading_type(sensor);
    unsigned int sensor_type = ipmi_sensor_get_sensor_type(sensor);
    const char *event_state = ipmi_get_reading_name(event_type, sensor_type, offset);

    char sensor_name[IPMI_SENSOR_NAME_LEN];
    ipmi_sensor_get_name(sensor, sensor_name, sizeof(sensor_name));
    notification_label_set(&n, "sensor_name", sensor_name);

    char message[1024];
    if (value_present != IPMI_NO_VALUES_PRESENT)
        ssnprintf(message, sizeof(message), "sensor %s received event: %s, value is %f",
                           sensor_name, event_state, value);
    else
        ssnprintf(message, sizeof(message), "sensor %s received event: %s, value not provided",
                           sensor_name, event_state);
    notification_annotation_set(&n, "summary", message);
#if 0
    DEBUG("Threshold event received for sensor %s", n.type_instance);
#endif
    notification_label_set(&n, "sensor_type", ipmi_sensor_get_sensor_type_string(sensor));
// sstrncpy(n.type, ipmi_sensor_get_sensor_type_string(sensor), sizeof(n.type));

    n.severity = sensor_convert_threshold_severity(threshold);
    n.time = NS_TO_CDTIME_T(ipmi_event_get_timestamp(event));

    notification_annotation_set(&n, "severity", ipmi_get_threshold_string(threshold));
    notification_annotation_set(&n, "direction", ipmi_get_value_dir_string(high_low));

    switch (value_present) {
        case IPMI_BOTH_VALUES_PRESENT: {
                char buf[DATA_MAX_NAME_LEN] = {0};
                ssnprintf(buf, sizeof(buf), "%f", value);
                notification_annotation_set(&n, "value", buf);
            }
            /* FALLTHRU */
        /* both values present, so fall-through to add raw value too */
        case IPMI_RAW_VALUE_PRESENT: {
                char buf[DATA_MAX_NAME_LEN] = {0};
                ssnprintf(buf, sizeof(buf), "0x%2.2x", raw_value);
                notification_annotation_set(&n, "value_raw", buf);
            }
            break;
        default:
            break;
    } /* switch (value_present) */

    add_event_common_data(&n, sensor, dir, event);

    plugin_dispatch_notification(&n);

    /* Delete handled ipmi event from the list */
    if (st->sel_clear_event) {
        ipmi_event_delete(event, NULL, NULL);
        return IPMI_EVENT_HANDLED;
    }

    return IPMI_EVENT_NOT_HANDLED;
}

static int sensor_discrete_event_handler(ipmi_sensor_t *sensor,
                                         __attribute__((unused)) enum ipmi_event_dir_e dir,
                                         int offset,
                                         __attribute__((unused)) int severity,
                                         __attribute__((unused)) int prev_severity,
                                         void *cb_data, ipmi_event_t *event)
{
    c_ipmi_instance_t *st = cb_data;

    /* From the IPMI specification Chapter 2: Events.
     * If a callback handles the event, then all future callbacks called due to
     * the event will receive a NULL for the event. So be ready to handle a NULL
     * event in all your event handlers. A NULL may also be passed to an event
     * handler if the callback was not due to an event. */
    if (event == NULL)
        return IPMI_EVENT_NOT_HANDLED;

    notification_t n = {
        .severity = NOTIF_OKAY,
        .time = 0,
        .name = "ipmi_sensor",
    };

    unsigned int event_type = ipmi_sensor_get_event_reading_type(sensor);
    unsigned int sensor_type = ipmi_sensor_get_sensor_type(sensor);
    const char *event_state = ipmi_get_reading_name(event_type, sensor_type, offset);

    char sensor_name[IPMI_SENSOR_NAME_LEN];
    ipmi_sensor_get_name(sensor, sensor_name, sizeof(sensor_name));
    notification_label_set(&n, "sensor_name", sensor_name);

    char message[1024];
    ssnprintf(message, sizeof(message), "sensor %s received event: %s",
                        sensor_name, event_state);
    notification_annotation_set(&n, "summary", message);
#if 0
    DEBUG("Discrete event received for sensor %s", n.type_instance);
#endif
//  sstrncpy(n.type, ipmi_sensor_get_sensor_type_string(sensor), sizeof(n.type));
    n.time = NS_TO_CDTIME_T(ipmi_event_get_timestamp(event));

#if 0
    plugin_notification_meta_add_signed_int(&n, "offset", offset);

    if (severity != -1)
        plugin_notification_meta_add_signed_int(&n, "severity", severity);

    if (prev_severity != -1)
        plugin_notification_meta_add_signed_int(&n, "prevseverity", prev_severity);

    add_event_common_data(&n, sensor, dir, event);
#endif

    plugin_dispatch_notification(&n);

    /* Delete handled ipmi event from the list */
    if (st->sel_clear_event) {
        ipmi_event_delete(event, NULL, NULL);
        return IPMI_EVENT_HANDLED;
    }

    return IPMI_EVENT_NOT_HANDLED;
}

static int sel_list_add(c_ipmi_instance_t *st, ipmi_sensor_t *sensor)
{
    char sensor_name[DATA_MAX_NAME_LEN] = {0};

    /* Check if sensor on sel_ignorelist */
    sensor_get_name(sensor, sensor_name, sizeof(sensor_name));
    if (!exclist_match(&st->excl_sel_sensor, sensor_name) != 0)
        return 0;

    int status = 0;
    /* register threshold event if threshold sensor support events */
    if (ipmi_sensor_get_event_reading_type(sensor) == IPMI_EVENT_READING_TYPE_THRESHOLD)
        status = ipmi_sensor_add_threshold_event_handler(sensor, sensor_threshold_event_handler, st);
    /* register discrete handler if discrete/specific sensor support events */
    else if (ipmi_sensor_get_event_support(sensor) != IPMI_EVENT_SUPPORT_NONE)
        status = ipmi_sensor_add_discrete_event_handler(sensor, sensor_discrete_event_handler, st);

    if (status)
        PLUGIN_ERROR("Unable to add sensor %s event handler, status: %d", sensor_name, status);

    return status;
}

static void sel_list_remove(c_ipmi_instance_t *st, ipmi_sensor_t *sensor)
{
    if (ipmi_sensor_get_event_reading_type(sensor) == IPMI_EVENT_READING_TYPE_THRESHOLD)
        ipmi_sensor_remove_threshold_event_handler(sensor, sensor_threshold_event_handler, st);
    else
        ipmi_sensor_remove_discrete_event_handler(sensor, sensor_discrete_event_handler, st);
}

static void entity_sensor_update_handler(enum ipmi_update_e op,
                                         ipmi_entity_t __attribute__((unused)) * entity,
                                         ipmi_sensor_t *sensor, void *user_data)
{
    c_ipmi_instance_t *st = user_data;

    if ((op == IPMI_ADDED) || (op == IPMI_CHANGED)) {
        /* Will check for duplicate entries.. */
        sensor_list_add(st, sensor);
        if (st->sel_enabled)
            sel_list_add(st, sensor);
    } else if (op == IPMI_DELETED) {
        sensor_list_remove(st, sensor);
        if (st->sel_enabled)
            sel_list_remove(st, sensor);
    }
}

static void domain_entity_update_handler(enum ipmi_update_e op,
                                         ipmi_domain_t __attribute__((unused)) * domain,
                                         ipmi_entity_t *entity, void *user_data)
{
    c_ipmi_instance_t *st = user_data;

    if (op == IPMI_ADDED) {
        int status = ipmi_entity_add_sensor_update_handler(entity, entity_sensor_update_handler,
                                                               (void *)st);
        if (status != 0)
            PLUGIN_ERROR("ipmi_entity_add_sensor_update_handler failed for '%s': %s",
                          st->name, STRERRIPMI(status));
    } else if (op == IPMI_DELETED) {
        int status = ipmi_entity_remove_sensor_update_handler(entity, entity_sensor_update_handler,
                                                                  (void *)st);
        if (status != 0)
            PLUGIN_ERROR("ipmi_entity_remove_sensor_update_handler failed for '%s': %s",
                         st->name, STRERRIPMI(status));
    }
}

static void smi_event_handler(ipmi_con_t __attribute__((unused)) * ipmi,
                              const ipmi_addr_t __attribute__((unused)) * addr,
                              unsigned int __attribute__((unused)) addr_len,
                              ipmi_event_t *event, void *cb_data)
{
    unsigned int type = ipmi_event_get_type(event);
    ipmi_domain_t *domain = cb_data;

    PLUGIN_DEBUG("%s: Event received: type %u", __func__, type);

    if (type != 0x02)
        /* It's not a standard IPMI event. */
        return;

    /* force domain to reread SELs */
    ipmi_domain_reread_sels(domain, NULL, NULL);
}

static void domain_connection_change_handler(ipmi_domain_t *domain, int err,
                                             __attribute__((unused)) unsigned int conn_num,
                                             __attribute__((unused)) unsigned int port_num,
                                             int still_connected,
                                             void *user_data)
{
    PLUGIN_DEBUG("domain_connection_change_handler (domain = %p, err = %i, "
                 "conn_num = %u, port_num = %u, still_connected = %i, "
                 "user_data = %p);",
                 (void *)domain, err, conn_num, port_num, still_connected, user_data);

    c_ipmi_instance_t *st = user_data;

    if (err != 0)
        PLUGIN_ERROR("domain_connection_change_handler failed for '%s': %s",
                      st->name, STRERRIPMI(err));

    if (!still_connected) {
        if (st->notify_conn && st->connected && (st->init_in_progress == 0)) {
            notification_t n = {
                .severity = NOTIF_FAILURE,
                .time = cdtime(),
                .name = "ipmi",
            };

            if (st->host != NULL)
                notification_label_set(&n, "host", st->host);

            char message[1024];
            snprintf(message, sizeof(message), "IPMI connection lost");
            notification_annotation_set(&n, "summary", message);

            plugin_dispatch_notification(&n);
        }

        st->connected = false;
        return;
    }

    if (st->notify_conn && !st->connected && (st->init_in_progress == 0)) {
        notification_t n = {
            .severity = NOTIF_OKAY,
            .time = cdtime(),
            .name = "ipmi",
        };

        if (st->host != NULL)
            notification_label_set(&n, "host", st->host);

        char message[1024];
        snprintf(message, sizeof(message), "IPMI connection restored");
        notification_annotation_set(&n, "summary", message);

        plugin_dispatch_notification(&n);
    }

    st->connected = true;

    int status = ipmi_domain_add_entity_update_handler(domain, domain_entity_update_handler, st);
    if (status != 0)
        PLUGIN_ERROR("ipmi_domain_add_entity_update_handler failed for '%s': %s",
                     st->name, STRERRIPMI(status));

    status = st->connection->add_event_handler(st->connection, smi_event_handler, (void *)domain);

    if (status != 0)
        PLUGIN_ERROR("Failed to register smi event handler for '%s': %s",
                     st->name, STRERRIPMI(status));
}

static int c_ipmi_read(user_data_t *user_data)
{
    c_ipmi_instance_t *st = user_data->data;

    if (os_handler_active == false) {
        PLUGIN_INFO("c_ipmi_read: I'm not active, returning false.");
        return 0;
    }

    if (os_handler == NULL)
        return 0;

    ipmi_domain_id_t domain_id;
    int status = 0;

    if (st->connection == NULL) {
        if (st->connaddr != NULL) {
            status = ipmi_ip_setup_con(&st->connaddr, &(char *){IPMI_LAN_STD_PORT_STR}, 1,
                                       st->authtype, (unsigned int)IPMI_PRIVILEGE_USER,
                                       st->username, strlen(st->username),
                                       st->password, strlen(st->password),
                                       os_handler, NULL, &st->connection);
            if (status != 0) {
                PLUGIN_ERROR("ipmi_ip_setup_con failed for '%s': %s", st->name, STRERRIPMI(status));
                return -1;
            }
        } else {
            status = ipmi_smi_setup_con(/* if_num = */ 0, os_handler, NULL, &st->connection);
            if (status != 0) {
                PLUGIN_ERROR("ipmi_smi_setup_con failed for '%s': %s", st->name, STRERRIPMI(status));
                return -1;
            }
        }

        ipmi_open_option_t opts[] = {
                {.option = IPMI_OPEN_OPTION_ALL, {.ival = 1}},
#ifdef IPMI_OPEN_OPTION_USE_CACHE
                /* OpenIPMI-2.0.17 and later: Disable SDR cache in local file */
                {.option = IPMI_OPEN_OPTION_USE_CACHE, {.ival = 0}},
#endif
        };

        /*
         * NOTE: Domain names must be unique. There is static `domains_list` common
         * to all threads inside lib/domain.c and some ops are done by name.
         */
        status = ipmi_open_domain(st->name, &st->connection, /* num_con = */ 1,
                                  domain_connection_change_handler, /* user data = */ (void *)st,
                                  /* domain_fully_up_handler = */ NULL, /* user data = */ NULL, opts,
                                  STATIC_ARRAY_SIZE(opts), &domain_id);
        if (status != 0) {
            PLUGIN_ERROR("ipmi_open_domain failed for '%s': %s", st->name, STRERRIPMI(status));
            return -1;
        }
    }

    if (st->connected == false)
        return 0;

    sensor_list_read_all(st);

    if (st->init_in_progress > 0)
        st->init_in_progress--;
    else
        st->init_in_progress = 0;

    return 0;
}

static void c_ipmi_instance_free(void *arg)
{
    c_ipmi_instance_t *st = arg;
    if (st == NULL)
        return;

    sensor_list_remove_all(st);

    free(st->name);
    free(st->host);
    free(st->connaddr);
    free(st->username);
    free(st->password);

    label_set_reset(&st->labels);

    exclist_reset(&st->excl_sensor);
    exclist_reset(&st->excl_sel_sensor);
    pthread_mutex_destroy(&st->sensor_list_lock);

    free(st);
}

static int c_ipmi_config_instance(config_item_t *ci)
{
    c_ipmi_instance_t *st = calloc(1, sizeof(*st));
    if (st == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    pthread_mutex_init(&st->sensor_list_lock, NULL);
    st->authtype = IPMI_AUTHTYPE_DEFAULT;

    int status = cf_util_get_string(ci, &st->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        c_ipmi_instance_free(st);
        return status;
    }

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("sensor", child->key) == 0) {
            status = cf_util_exclist(child, &st->excl_sensor);
        } else if (strcasecmp("notify-ipmi-connection-state", child->key) == 0) {
            status = cf_util_get_boolean(child, &st->notify_conn);
        } else if (strcasecmp("notify-sensor-add", child->key) == 0) {
            status = cf_util_get_boolean(child, &st->notify_add);
        } else if (strcasecmp("notify-sensor-remove", child->key) == 0) {
            status = cf_util_get_boolean(child, &st->notify_remove);
        } else if (strcasecmp("notify-sensor-not-present", child->key) == 0) {
            status = cf_util_get_boolean(child, &st->notify_notpresent);
        } else if (strcasecmp("sel-sensor", child->key) == 0) {
            status = cf_util_exclist(child, &st->excl_sel_sensor);
        } else if (strcasecmp("sel-enable", child->key) == 0) {
            status = cf_util_get_boolean(child, &st->sel_enabled);
        } else if (strcasecmp("sel-celear-event", child->key) == 0) {
            status = cf_util_get_boolean(child, &st->sel_clear_event);
        } else if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &st->host);
        } else if (strcasecmp("address", child->key) == 0) {
            status = cf_util_get_string(child, &st->connaddr);
        } else if (strcasecmp("username", child->key) == 0) {
            status = cf_util_get_string(child, &st->username);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &st->password);
        } else if (strcasecmp("auth-type", child->key) == 0) {
            char tmp[8];
            status = cf_util_get_string_buffer(child, tmp, sizeof(tmp));
            if (status != 0)
                break;

            if (strcasecmp("MD5", tmp) == 0) {
                st->authtype = IPMI_AUTHTYPE_MD5;
            } else if (strcasecmp("rmcp+", tmp) == 0) {
                st->authtype = IPMI_AUTHTYPE_RMCP_PLUS;
            } else {
                PLUGIN_ERROR("The value '%s' is not valid for the 'auth-type' option.", tmp);
                status = -1;
            }
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &st->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else {
            PLUGIN_WARNING("Option '%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        c_ipmi_instance_free(st);
        return status;
    }

    label_set_add(&st->labels, true, "instance", st->name);

    /* Don't send `ADD' notifications during startup (~ 1 minute) */
    if (interval != 0)
        st->init_in_progress = 1 + (int)(TIME_T_TO_CDTIME_T(60) / interval);
    else
        st->init_in_progress = 1 + (int)(TIME_T_TO_CDTIME_T(60) / plugin_get_interval());

    return plugin_register_complex_read("ipmi", st->name, c_ipmi_read, interval,
                                    &(user_data_t){.data = st, .free_func = c_ipmi_instance_free});
}

static int c_ipmi_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = c_ipmi_config_instance(child);
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

static void *c_ipmi_thread_main(__attribute__((unused)) void *user_data)
{
    while (os_handler_active) {
        struct timeval tv = {1, 0};
        os_handler->perform_one_op(os_handler, &tv);
    }

    return (void *)0;
}

static int c_ipmi_init(void)
{
    if (os_handler != NULL)
        return 0;

    os_handler = ipmi_posix_thread_setup_os_handler(SIGIO);
    if (os_handler == NULL) {
        PLUGIN_ERROR("ipmi_posix_thread_setup_os_handler failed.");
        return -1;
    }

    os_handler->set_log_handler(os_handler, c_ipmi_log);

    int status = ipmi_init(os_handler);
    if (status != 0) {
        PLUGIN_ERROR("ipmi_init() failed.");
        os_handler->free_os_handler(os_handler);
        return -1;
    };

    os_handler_active = true;

    status = plugin_thread_create(&os_handler_thread_id, c_ipmi_thread_main, NULL, "ipmi");
    if (status != 0) {
        os_handler_active = false;
        os_handler_thread_id = (pthread_t){0};
        PLUGIN_ERROR("pthread_create failed for ipmi.");
        return -1;
    }

    return 0;
}

static int c_ipmi_shutdown(void)
{
    os_handler_active = false;

    if (!pthread_equal(os_handler_thread_id, (pthread_t){0})) {
        pthread_join(os_handler_thread_id, NULL);
        os_handler_thread_id = (pthread_t){0};
    }

    os_handler->free_os_handler(os_handler);
    os_handler = NULL;
    return 0;
}

void module_register(void)
{
    plugin_register_config("ipmi", c_ipmi_config);
    plugin_register_init("ipmi", c_ipmi_init);
    plugin_register_shutdown("ipmi", c_ipmi_shutdown);
}
