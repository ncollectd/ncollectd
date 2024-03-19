// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2006-2014  Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008 Michał Mirosław
// SPDX-FileCopyrightText: Copyright (C) 2014 Andy Parkins
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Michał Mirosław <mirq-linux at rere.qmqm.pl>
// SPDX-FileContributor: Andy Parkins <andyp at fussylogic.co.uk>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifdef HAVE_MACH_MACH_TYPES_H
#include <mach/mach_types.h>
#endif
#ifdef HAVE_MACH_MACH_INIT_H
#include <mach/mach_init.h>
#endif
#ifdef HAVE_MACH_MACH_ERROR_H
#include <mach/mach_error.h>
#endif
#ifdef HAVE_COREFOUNDATION_COREFOUNDATION_H
#include <CoreFoundation/CoreFoundation.h>
#endif
#ifdef HAVE_IOKIT_IOKITLIB_H
#include <IOKit/IOKitLib.h>
#endif
#ifdef HAVE_IOKIT_IOTYPES_H
#include <IOKit/IOTypes.h>
#endif
#ifdef HAVE_IOKIT_PS_IOPOWERSOURCES_H
#include <IOKit/ps/IOPowerSources.h>
#endif
#ifdef HAVE_IOKIT_PS_IOPSKEYS_H
#include <IOKit/ps/IOPSKeys.h>
#endif

#if !defined(HAVE_IOKIT_IOKITLIB_H) && !defined(HAVE_IOKIT_PS_IOPOWERSOURCES_H) && !defined(KERNEL_LINUX)
#error "No applicable input method."
#endif

#if defined(HAVE_IOKIT_IOKITLIB_H) || defined(HAVE_IOKIT_PS_IOPOWERSOURCES_H)
/* No global variables */
/* #endif HAVE_IOKIT_IOKITLIB_H || HAVE_IOKIT_PS_IOPOWERSOURCES_H */

#elif defined(KERNEL_LINUX)
static char *path_proc;
static char *path_proc_acpi;
static char *path_sys_power_supply;
#define PROC_ACPI_FACTOR 0.001
#define SYSFS_FACTOR 0.000001
#endif /* KERNEL_LINUX */

static bool report_percent;
static bool report_degraded;

enum {
    FAM_BATTERY_POWER,
    FAM_BATTERY_CURRENT,
    FAM_BATTERY_VOLTAGE,
    FAM_BATTERY_CHARGED_RATIO,
    FAM_BATTERY_DISCHARGED_RATIO,
    FAM_BATTERY_DEGRADED_RATIO,
    FAM_BATTERY_CHARGED,
    FAM_BATTERY_DISCHARGED,
    FAM_BATTERY_DEGRADED,
    FAM_BATTERY_CAPACITY,
    FAM_BATTERY_MAX,
};

static metric_family_t fams[FAM_BATTERY_MAX] = {
    [FAM_BATTERY_POWER] = {
        .name = "system_battery_power",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_BATTERY_CURRENT] = {
        .name = "system_battery_current",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_BATTERY_VOLTAGE] = {
        .name = "system_battery_voltage",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_BATTERY_CHARGED_RATIO] = {
        .name = "system_battery_charged_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_BATTERY_DISCHARGED_RATIO] = {
        .name = "system_battery_discharged_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_BATTERY_DEGRADED_RATIO] = {
        .name = "system_battery_degraded_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_BATTERY_CHARGED] = {
        .name = "system_battery_charged",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_BATTERY_DISCHARGED] = {
        .name = "system_battery_discharged",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_BATTERY_DEGRADED] = {
        .name = "system_battery_degraded",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_BATTERY_CAPACITY] = {
        .name = "system_battery_capacity",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
};

static void submit_capacity(const char *device, double capacity_charged, double capacity_full,
                                                double capacity_design)
{
    if (report_percent && (capacity_charged > capacity_full))
        return;
    if (report_degraded && (capacity_full > capacity_design))
        return;

    if (report_percent) {
        double capacity_max;

        if (report_degraded)
            capacity_max = capacity_design;
        else
            capacity_max = capacity_full;

        metric_family_append(&fams[FAM_BATTERY_CHARGED_RATIO],
                             VALUE_GAUGE(100.0 * capacity_charged / capacity_max), NULL,
                             &(label_pair_const_t){.name="battery", .value=device}, NULL);
        metric_family_append(&fams[FAM_BATTERY_DISCHARGED_RATIO],
                             VALUE_GAUGE(100.0 * (capacity_full - capacity_charged) / capacity_max),
                             NULL,
                             &(label_pair_const_t){.name="battery", .value=device}, NULL);

        if (report_degraded) {
            metric_family_append(&fams[FAM_BATTERY_DEGRADED_RATIO],
                                 VALUE_GAUGE(100.0 * (capacity_design - capacity_full) / capacity_max),
                                 NULL,
                                 &(label_pair_const_t){.name="battery", .value=device}, NULL);
        }
    } else if (report_degraded) {
        metric_family_append(&fams[FAM_BATTERY_CHARGED], VALUE_GAUGE(capacity_charged), NULL,
                             &(label_pair_const_t){.name="battery", .value=device}, NULL);
        metric_family_append(&fams[FAM_BATTERY_DISCHARGED],
                             VALUE_GAUGE(capacity_full - capacity_charged), NULL,
                             &(label_pair_const_t){.name="battery", .value=device}, NULL);
        metric_family_append(&fams[FAM_BATTERY_DEGRADED],
                             VALUE_GAUGE(capacity_design - capacity_full), NULL,
                             &(label_pair_const_t){.name="battery", .value=device}, NULL);
    } else {
        metric_family_append(&fams[FAM_BATTERY_CAPACITY], VALUE_GAUGE(capacity_charged), NULL,
                             &(label_pair_const_t){.name="battery", .value=device}, NULL);
    }
}

#if defined(HAVE_IOKIT_PS_IOPOWERSOURCES_H) || defined(HAVE_IOKIT_IOKITLIB_H)
static double dict_get_double(CFDictionaryRef dict, const char *key_string)
{
    double val_double;
    long long val_int;
    CFNumberRef val_obj;
    CFStringRef key_obj;

    key_obj = CFStringCreateWithCString(kCFAllocatorDefault, key_string, kCFStringEncodingASCII);
    if (key_obj == NULL) {
        PLUGIN_DEBUG("CFStringCreateWithCString (%s) failed.\n", key_string);
        return NAN;
    }

    if ((val_obj = CFDictionaryGetValue(dict, key_obj)) == NULL) {
        PLUGIN_DEBUG("CFDictionaryGetValue (%s) failed.", key_string);
        CFRelease(key_obj);
        return NAN;
    }
    CFRelease(key_obj);

    if (CFGetTypeID(val_obj) == CFNumberGetTypeID()) {
        if (CFNumberIsFloatType(val_obj)) {
            CFNumberGetValue(val_obj, kCFNumberDoubleType, &val_double);
        } else {
            CFNumberGetValue(val_obj, kCFNumberLongLongType, &val_int);
            val_double = val_int;
        }
    } else {
        PLUGIN_DEBUG("CFGetTypeID (val_obj) = %i", (int)CFGetTypeID(val_obj));
        return NAN;
    }

    return val_double;
}

#ifdef HAVE_IOKIT_PS_IOPOWERSOURCES_H
static void get_via_io_power_sources(double *ret_charge, double *ret_current, double *ret_voltage)
{
    CFTypeRef ps_raw;
    CFArrayRef ps_array;
    int ps_array_len;
    CFDictionaryRef ps_dict;
    CFTypeRef ps_obj;

    double temp_double;

    ps_raw = IOPSCopyPowerSourcesInfo();
    ps_array = IOPSCopyPowerSourcesList(ps_raw);
    ps_array_len = CFArrayGetCount(ps_array);

    PLUGIN_DEBUG("ps_array_len == %i", ps_array_len);

    for (int i = 0; i < ps_array_len; i++) {
        ps_obj = CFArrayGetValueAtIndex(ps_array, i);
        ps_dict = IOPSGetPowerSourceDescription(ps_raw, ps_obj);

        if (ps_dict == NULL) {
            PLUGIN_DEBUG("IOPSGetPowerSourceDescription failed.");
            continue;
        }

        if (CFGetTypeID(ps_dict) != CFDictionaryGetTypeID()) {
            PLUGIN_DEBUG("IOPSGetPowerSourceDescription did not return a CFDictionaryRef");
            continue;
        }

        /* FIXME: Check if this is really an internal battery */

        if (isnan(*ret_charge)) {
            /* This is the charge in percent. */
            temp_double = dict_get_double(ps_dict, kIOPSCurrentCapacityKey);
            if (!isnan((temp_double)) && (temp_double >= 0.0) && (temp_double <= 100.0))
                *ret_charge = temp_double;
        }

        if (isnan(*ret_current)) {
            temp_double = dict_get_double(ps_dict, kIOPSCurrentKey);
            if (!isnan(temp_double))
                *ret_current = temp_double / 1000.0;
        }

        if (isnan(*ret_voltage)) {
            temp_double = dict_get_double(ps_dict, kIOPSVoltageKey);
            if (!isnan(temp_double))
                *ret_voltage = temp_double / 1000.0;
        }
    }

    CFRelease(ps_array);
    CFRelease(ps_raw);
}
#endif

#ifdef HAVE_IOKIT_IOKITLIB_H
static void get_via_generic_iokit(double *ret_capacity_full, double *ret_capacity_design,
                                  double *ret_current, double *ret_voltage)
{
    kern_return_t status;
    io_iterator_t iterator;
    io_object_t io_obj;

    CFDictionaryRef bat_root_dict;
    CFArrayRef bat_info_arry;
    CFIndex bat_info_arry_len;
    CFDictionaryRef bat_info_dict;

    double temp_double;

    status = IOServiceGetMatchingServices((mach_port_t) 0, IOServiceNameMatching("battery"),
                                                           &iterator);
    if (status != kIOReturnSuccess) {
        DEBUG("IOServiceGetMatchingServices failed.");
        return;
    }

    while ((io_obj = IOIteratorNext(iterator))) {
        status = IORegistryEntryCreateCFProperties(io_obj, (CFMutableDictionaryRef *)&bat_root_dict,
                                                           kCFAllocatorDefault, kNilOptions);
        if (status != kIOReturnSuccess) {
            DEBUG("IORegistryEntryCreateCFProperties failed.");
            continue;
        }

        bat_info_arry = (CFArrayRef)CFDictionaryGetValue(bat_root_dict, CFSTR("IOBatteryInfo"));
        if (bat_info_arry == NULL) {
            CFRelease(bat_root_dict);
            continue;
        }
        bat_info_arry_len = CFArrayGetCount(bat_info_arry);

        for (CFIndex bat_info_arry_pos = 0; bat_info_arry_pos < bat_info_arry_len;
                 bat_info_arry_pos++) {
            bat_info_dict = (CFDictionaryRef)CFArrayGetValueAtIndex(bat_info_arry,
                                                                    bat_info_arry_pos);

            if (isnan(*ret_capacity_full)) {
                temp_double = dict_get_double(bat_info_dict, "Capacity");
                *ret_capacity_full = temp_double / 1000.0;
            }

            if (isnan(*ret_capacity_design)) {
                temp_double = dict_get_double(bat_info_dict, "AbsoluteMaxCapacity");
                *ret_capacity_design = temp_double / 1000.0;
            }

            if (isnan(*ret_current)) {
                temp_double = dict_get_double(bat_info_dict, "Current");
                *ret_current = temp_double / 1000.0;
            }

            if (isnan(*ret_voltage)) {
                temp_double = dict_get_double(bat_info_dict, "Voltage");
                *ret_voltage = temp_double / 1000.0;
            }
        }

        CFRelease(bat_root_dict);
    }

    IOObjectRelease(iterator);
}
#endif

static int battery_read_metrics(void)
{
    double current = NAN; /* Current in A */
    double voltage = NAN; /* Voltage in V */

    /* We only get the charged capacity as a percentage from
     * IOPowerSources. IOKit, on the other hand, only reports the full
     * capacity. We use the two to calculate the current charged capacity. */
    double charge_rel = NAN;      /* Current charge in percent */
    double capacity_charged;      /* Charged capacity */
    double capacity_full = NAN;   /* Total capacity */
    double capacity_design = NAN; /* Full design capacity */

#ifdef HAVE_IOKIT_PS_IOPOWERSOURCES_H
    get_via_io_power_sources(&charge_rel, &current, &voltage);
#endif
#ifdef HAVE_IOKIT_IOKITLIB_H
    get_via_generic_iokit(&capacity_full, &capacity_design, &current, &voltage);
#endif

    capacity_charged = charge_rel * capacity_full / 100.0;
    submit_capacity("0", capacity_charged, capacity_full, capacity_design);

    if (!isnan(current))
        metric_family_append(&fams[FAM_BATTERY_CURRENT], VALUE_GAUGE(current), NULL,
                             &(label_pair_const_t){.name="battery", .value="0"}, NULL);
    if (!isnan(voltage))
        metric_family_append(&fams[FAM_BATTERY_VOLTAGE], VALUE_GAUGE(voltage), NULL,
                             &(label_pair_const_t){.name="battery", .value="0"}, NULL);

    return 0;
}
/* #endif HAVE_IOKIT_IOKITLIB_H || HAVE_IOKIT_PS_IOPOWERSOURCES_H */

#elif defined(KERNEL_LINUX)
/* Reads a file which contains only a number (and optionally a trailing
 * newline) and parses that number. */
static int sysfs_file_to_buffer(char const *dir, char const *power_supply, char const *basename,
                                                 char *buffer, size_t buffer_size)
{
    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "%s/%s/%s", dir, power_supply, basename);

    ssize_t status = read_file(filename, buffer, buffer_size);
    if (status < 0)
        return status;

    strstripnewline(buffer);
    return 0;
}

/* Reads a file which contains only a number (and optionally a trailing
 * newline) and parses that number. */
static int sysfs_file_to_gauge(char const *dir, char const *power_supply, char const *basename,
                                                double *ret_value)
{
    int status;
    char buffer[32];

    status = sysfs_file_to_buffer(dir, power_supply, basename, buffer, sizeof(buffer));
    if (status != 0)
        return status;

    return strtodouble(buffer, ret_value);
}

static int read_sysfs_capacity(char const *dir, char const *power_supply,
                                                char const *device)
{
    double capacity_charged = NAN;
    double capacity_full = NAN;
    double capacity_design = NAN;
    int status;

    status = sysfs_file_to_gauge(dir, power_supply, "energy_now", &capacity_charged);
    if (status != 0)
        return status;

    status = sysfs_file_to_gauge(dir, power_supply, "energy_full", &capacity_full);
    if (status != 0)
        return status;

    status = sysfs_file_to_gauge(dir, power_supply, "energy_full_design", &capacity_design);
    if (status != 0)
        return status;

    submit_capacity(device, capacity_charged * SYSFS_FACTOR,
                            capacity_full * SYSFS_FACTOR, capacity_design * SYSFS_FACTOR);
    return 0;
}

static int read_sysfs_capacity_from_charge(char const *dir, char const *power_supply,
                                                            char const *device)
{
    double capacity_charged = NAN;
    double capacity_full = NAN;
    double capacity_design = NAN;
    double voltage_min_design = NAN;
    int status;

    status = sysfs_file_to_gauge(dir, power_supply, "voltage_min_design", &voltage_min_design);
    if (status != 0)
        return status;
    voltage_min_design *= SYSFS_FACTOR;

    status = sysfs_file_to_gauge(dir, power_supply, "charge_now", &capacity_charged);
    if (status != 0)
        return status;
    capacity_charged *= voltage_min_design;

    status = sysfs_file_to_gauge(dir, power_supply, "charge_full", &capacity_full);
    if (status != 0)
        return status;
    capacity_full *= voltage_min_design;

    status = sysfs_file_to_gauge(dir, power_supply, "charge_full_design", &capacity_design);
    if (status != 0)
        return status;
    capacity_design *= voltage_min_design;

    submit_capacity(device, capacity_charged * SYSFS_FACTOR,
                            capacity_full * SYSFS_FACTOR, capacity_design * SYSFS_FACTOR);
    return 0;
}

static int read_sysfs_callback(__attribute__((unused)) int dir_fd, char const *dir,
                               char const *power_supply, void *user_data)
{
    int *battery_index = user_data;

    char const *device;
    char buffer[32];
    double v = NAN;
    bool discharging = false;
    int status;

    /* Ignore non-battery directories, such as AC power. */
    status = sysfs_file_to_buffer(dir, power_supply, "type", buffer, sizeof(buffer));
    if (status != 0)
        return 0;
    if (strcasecmp("Battery", buffer) != 0)
        return 0;

    (void)sysfs_file_to_buffer(dir, power_supply, "status", buffer, sizeof(buffer));
    if (strcasecmp("Discharging", buffer) == 0)
        discharging = true;

    /* FIXME: This is a dirty hack for backwards compatibility: The battery
     * plugin, for a very long time, has had the plugin_instance
     * hard-coded to "0". So, to keep backwards compatibility, we'll use
     * "0" for the first battery we find and the power_supply name for all
     * following. This should be reverted in a future major version. */
    device = (*battery_index == 0) ? "0" : power_supply;
    (*battery_index)++;

    if (sysfs_file_to_gauge(dir, power_supply, "energy_now", &v) == 0)
        read_sysfs_capacity(dir, power_supply, device);
    else
        read_sysfs_capacity_from_charge(dir, power_supply, device);

    if (sysfs_file_to_gauge(dir, power_supply, "power_now", &v) == 0) {
        if (discharging)
            v *= -1.0;
        metric_family_append(&fams[FAM_BATTERY_POWER], VALUE_GAUGE(v * SYSFS_FACTOR), NULL,
                             &(label_pair_const_t){.name="battery", .value=device}, NULL);
    }
    if (sysfs_file_to_gauge(dir, power_supply, "current_now", &v) == 0) {
        if (discharging)
            v *= -1.0;
        metric_family_append(&fams[FAM_BATTERY_CURRENT], VALUE_GAUGE(v * SYSFS_FACTOR), NULL,
                             &(label_pair_const_t){.name="battery", .value=device}, NULL);
    }

    if (sysfs_file_to_gauge(dir, power_supply, "voltage_now", &v) == 0)
        metric_family_append(&fams[FAM_BATTERY_VOLTAGE], VALUE_GAUGE(v * SYSFS_FACTOR), NULL,
                             &(label_pair_const_t){.name="battery", .value=device}, NULL);

    return 0;
}

static int read_sysfs(void)
{
    int status;
    int battery_counter = 0;

    if (access(path_sys_power_supply, R_OK) != 0)
        return ENOENT;

    status = walk_directory(path_sys_power_supply, read_sysfs_callback,
                                                   /* user_data = */ &battery_counter,
                                                   /* include hidden */ 0);
    return status;
}

static int read_acpi_full_capacity(char const *dir, char const *power_supply,
                                                    double *ret_capacity_full,
                                                    double *ret_capacity_design)

{
    char filename[PATH_MAX];
    char buffer[1024];

    FILE *fh;

    snprintf(filename, sizeof(filename), "%s/%s/info", dir, power_supply);
    fh = fopen(filename, "r");
    if (fh == NULL)
        return errno;

    /* last full capacity: 40090 mWh */
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        double *value_ptr;
        int fields_num;
        char *fields[8];
        int index;

        if (strncmp("last full capacity:", buffer, strlen("last full capacity:")) == 0) {
            value_ptr = ret_capacity_full;
            index = 3;
        } else if (strncmp("design capacity:", buffer, strlen("design capacity:")) == 0) {
            value_ptr = ret_capacity_design;
            index = 2;
        } else {
            continue;
        }

        fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (fields_num <= index)
            continue;

        strtodouble(fields[index], value_ptr);
    }

    fclose(fh);
    return 0;
}

static int read_acpi_callback(__attribute__((unused)) int dirfd, char const *dir,
                              char const *power_supply, void *user_data)
{
    int *battery_index = user_data;

    double power = NAN;
    double voltage = NAN;
    double capacity_charged = NAN;
    double capacity_full = NAN;
    double capacity_design = NAN;
    bool charging = false;
    bool is_current = false;

    char const *device;
    char filename[PATH_MAX];
    char buffer[1024];

    FILE *fh;

    snprintf(filename, sizeof(filename), "%s/%s/state", dir, power_supply);
    fh = fopen(filename, "r");
    if (fh == NULL) {
        if ((errno == EAGAIN) || (errno == EINTR) || (errno == ENOENT))
            return 0;
        else
            return errno;
    }

    /*
     * [11:00] <@tokkee> $ cat /proc/acpi/battery/BAT1/state
     * [11:00] <@tokkee> present:             yes
     * [11:00] <@tokkee> capacity state:      ok
     * [11:00] <@tokkee> charging state:      charging
     * [11:00] <@tokkee> present rate:        1724 mA
     * [11:00] <@tokkee> remaining capacity:  4136 mAh
     * [11:00] <@tokkee> present voltage:     12428 mV
     */
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[8];
        int numfields;

        numfields = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields < 3)
            continue;

        if ((strcmp(fields[0], "charging") == 0) && (strcmp(fields[1], "state:") == 0)) {
            if (strcmp(fields[2], "charging") == 0)
                charging = true;
            else
                charging = false;
            continue;
        }

        /* The unit of "present rate" depends on the battery. Modern
         * batteries export power (watts), older batteries (used to)
         * export current (amperes). We check the fourth column and try
         * to find old batteries this way. */
        if ((strcmp(fields[0], "present") == 0) && (strcmp(fields[1], "rate:") == 0)) {
            strtodouble(fields[2], &power);
            if ((numfields >= 4) && (strcmp("mA", fields[3]) == 0))
                is_current = true;
        } else if ((strcmp(fields[0], "remaining") == 0) && (strcmp(fields[1], "capacity:") == 0))
            strtodouble(fields[2], &capacity_charged);
        else if ((strcmp(fields[0], "present") == 0) && (strcmp(fields[1], "voltage:") == 0))
            strtodouble(fields[2], &voltage);
    } /* while (fgets (buffer, sizeof (buffer), fh) != NULL) */

    fclose(fh);

    if (!charging)
        power *= -1.0;

    /* FIXME: This is a dirty hack for backwards compatibility: The battery
     * plugin, for a very long time, has had the plugin_instance
     * hard-coded to "0". So, to keep backwards compatibility, we'll use
     * "0" for the first battery we find and the power_supply name for all
     * following. This should be reverted in a future major version. */
    device = (*battery_index == 0) ? "0" : power_supply;
    (*battery_index)++;

    read_acpi_full_capacity(dir, power_supply, &capacity_full, &capacity_design);

    submit_capacity(device, capacity_charged * PROC_ACPI_FACTOR,
                            capacity_full * PROC_ACPI_FACTOR,
                            capacity_design * PROC_ACPI_FACTOR);

    if (is_current) {
        metric_family_append(&fams[FAM_BATTERY_CURRENT], VALUE_GAUGE(power * PROC_ACPI_FACTOR), NULL,
                             &(label_pair_const_t){.name="battery", .value=device}, NULL);

    } else {
        metric_family_append(&fams[FAM_BATTERY_POWER], VALUE_GAUGE(power * PROC_ACPI_FACTOR), NULL,
                             &(label_pair_const_t){.name="battery", .value=device}, NULL);
    }

    metric_family_append(&fams[FAM_BATTERY_VOLTAGE], VALUE_GAUGE(voltage * PROC_ACPI_FACTOR), NULL,
                         &(label_pair_const_t){.name="battery", .value=device}, NULL);

    return 0;
}

static int read_acpi(void)
{
    int status;
    int battery_counter = 0;

    if (access(path_proc_acpi, R_OK) != 0)
        return ENOENT;

    status = walk_directory(path_proc_acpi, read_acpi_callback,
                                            /* user_data = */ &battery_counter,
                                            /* include hidden */ 0);
    return status;
}

static int read_pmu(void)
{
    int i = 0;
    /* The upper limit here is just a safeguard. If there is a system with
     * more than 100 batteries, this can easily be increased. */
    for (; i < 100; i++) {
        char buffer[1024];
        char filename[PATH_MAX];
        char device[DATA_MAX_NAME_LEN];

        double current = NAN;
        double voltage = NAN;
        double charge = NAN;

        snprintf(filename, sizeof(filename), "%s/pmu/battery_%i", path_proc, i);
        /* coverity[TOCTOU] */
        if (access(filename, R_OK) != 0)
            break;

        snprintf(device, sizeof(device), "%i", i);

        FILE *fh = fopen(filename, "r");
        if (fh == NULL) {
            if (errno == ENOENT)
                break;
            else if ((errno == EAGAIN) || (errno == EINTR))
                continue;
            else
                return errno;
        }

        while (fgets(buffer, sizeof(buffer), fh) != NULL) {
            char *fields[8];
            int numfields;

            numfields = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
            if (numfields < 3)
                continue;

            if (strcmp("current", fields[0]) == 0)
                strtodouble(fields[2], &current);
            else if (strcmp("voltage", fields[0]) == 0)
                strtodouble(fields[2], &voltage);
            else if (strcmp("charge", fields[0]) == 0)
                strtodouble(fields[2], &charge);
        }

        fclose(fh);
        fh = NULL;

        metric_family_append(&fams[FAM_BATTERY_CHARGED], VALUE_GAUGE(charge / 1000.0), NULL,
                             &(label_pair_const_t){.name="battery", .value=device}, NULL);
        metric_family_append(&fams[FAM_BATTERY_CURRENT], VALUE_GAUGE(current / 1000.0), NULL,
                             &(label_pair_const_t){.name="battery", .value=device}, NULL);
        metric_family_append(&fams[FAM_BATTERY_VOLTAGE], VALUE_GAUGE(voltage / 1000.0), NULL,
                             &(label_pair_const_t){.name="battery", .value=device}, NULL);
    }

    if (i == 0)
        return ENOENT;
    return 0;
}

static int battery_read_metrics(void)
{
    int status;

    PLUGIN_DEBUG("Trying sysfs ...");
    status = read_sysfs();
    if (status == 0)
        return 0;

    PLUGIN_DEBUG("Trying acpi ...");
    status = read_acpi();
    if (status == 0)
        return 0;

    PLUGIN_DEBUG("Trying pmu ...");
    status = read_pmu();
    if (status == 0)
        return 0;

    PLUGIN_ERROR("All available input methods failed.");
    return -1;
}
#endif

static int battery_read(void)
{
    int status = battery_read_metrics();

    plugin_dispatch_metric_family_array(fams, FAM_BATTERY_MAX, 0);

    return status;
}

static int battery_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("values-percentage", child->key) == 0) {
            status = cf_util_get_boolean(child, &report_percent);
        } else if (strcasecmp("report-degraded", child->key) == 0) {
            status = cf_util_get_boolean(child, &report_degraded);
        } else {
            PLUGIN_WARNING("Ignoring unknown configuration option \"%s\".", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

#ifdef KERNEL_LINUX
static int battery_init(void)
{
    path_proc = plugin_procpath(NULL);
    if (path_proc == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    path_proc_acpi = plugin_procpath("acpi/battery");
    if (path_proc_acpi == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    path_sys_power_supply = plugin_syspath("class/power_supply");
    if (path_sys_power_supply == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    return 0;
}

static int battery_shutdown(void)
{
    free(path_proc);
    free(path_proc_acpi);
    free(path_sys_power_supply);

    return 0;
}
#endif

void module_register(void)
{
#ifdef KERNEL_LINUX
    plugin_register_init("battery", battery_init);
    plugin_register_shutdown("battery", battery_shutdown);
#endif
    plugin_register_config("battery", battery_config);
    plugin_register_read("battery", battery_read);
}
