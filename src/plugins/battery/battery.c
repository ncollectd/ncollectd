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

enum {
    FAM_BATTERY_POWER,
    FAM_BATTERY_CURRENT,
    FAM_BATTERY_VOLTAGE,
    FAM_BATTERY_CAPACITY_CHARGED,
    FAM_BATTERY_CAPACITY_FULL,
    FAM_BATTERY_CAPACITY_DESIGN,
    FAM_BATTERY_MAX,
};

static metric_family_t fams[FAM_BATTERY_MAX] = {
    [FAM_BATTERY_POWER] = {
        .name = "system_battery_power",
        .type = METRIC_TYPE_GAUGE,
        .help = "Power drawn from (negative) or supplied to the battery in watts.",
    },
    [FAM_BATTERY_CURRENT] = {
        .name = "system_battery_current",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current drawn from (negative) or supplied to the battery in amperes.",
    },
    [FAM_BATTERY_VOLTAGE] = {
        .name = "system_battery_voltage",
        .type = METRIC_TYPE_GAUGE,
        .help = "Voltage of the battery in volts.",
    },
    [FAM_BATTERY_CAPACITY_CHARGED] = {
        .name = "system_battery_capacity_charged",
        .type = METRIC_TYPE_GAUGE,
        .help = "Charged capacity of the battery in watt-hours.",
    },
    [FAM_BATTERY_CAPACITY_FULL] = {
        .name = "system_battery_capacity_full",
        .type = METRIC_TYPE_GAUGE,
        .help = "Capacity of the battery when fully charged in watt-hours.",
    },
    [FAM_BATTERY_CAPACITY_DESIGN] = {
        .name = "system_battery_capacity_design",
        .type = METRIC_TYPE_GAUGE,
        .help = "Design capacity of the battery in watt-hours.",
    },
};

static void submit_capacity(const char *device, double capacity_charged, double capacity_full,
                                                double capacity_design)
{
    /* Submit each value independently: the full capacity can be greater than
     * the design capacity and the charged capacity can be greater than the
     * full capacity. Only reject unknown or negative values. */
    if (!isnan(capacity_charged) && (capacity_charged >= 0.0)) {
        metric_family_append(&fams[FAM_BATTERY_CAPACITY_CHARGED], VALUE_GAUGE(capacity_charged),
                             NULL, &LABEL_PAIR_CONST("battery", device), NULL);
    } else {
        PLUGIN_DEBUG("Ignoring charged capacity %g of battery '%s'.", capacity_charged, device);
    }

    if (!isnan(capacity_full) && (capacity_full >= 0.0)) {
        metric_family_append(&fams[FAM_BATTERY_CAPACITY_FULL], VALUE_GAUGE(capacity_full),
                             NULL, &LABEL_PAIR_CONST("battery", device), NULL);
    } else {
        PLUGIN_DEBUG("Ignoring full capacity %g of battery '%s'.", capacity_full, device);
    }

    if (!isnan(capacity_design) && (capacity_design >= 0.0)) {
        metric_family_append(&fams[FAM_BATTERY_CAPACITY_DESIGN], VALUE_GAUGE(capacity_design),
                             NULL, &LABEL_PAIR_CONST("battery", device), NULL);
    } else {
        PLUGIN_DEBUG("Ignoring design capacity %g of battery '%s'.", capacity_design, device);
    }
}

#if defined(HAVE_IOKIT_PS_IOPOWERSOURCES_H) || defined(HAVE_IOKIT_IOKITLIB_H)
static double dict_get_double(CFDictionaryRef dict, const char *key_string)
{
    double val_double;

    CFStringRef key_obj = CFStringCreateWithCString(kCFAllocatorDefault, key_string,
                                                    kCFStringEncodingASCII);
    if (key_obj == NULL) {
        PLUGIN_DEBUG("CFStringCreateWithCString (%s) failed.", key_string);
        return NAN;
    }

    CFNumberRef val_obj = CFDictionaryGetValue(dict, key_obj);
    if (val_obj == NULL) {
        PLUGIN_DEBUG("CFDictionaryGetValue (%s) failed.", key_string);
        CFRelease(key_obj);
        return NAN;
    }
    CFRelease(key_obj);

    if (CFGetTypeID(val_obj) == CFNumberGetTypeID()) {
        if (CFNumberIsFloatType(val_obj)) {
            CFNumberGetValue(val_obj, kCFNumberDoubleType, &val_double);
        } else {
            long long val_int;
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
    CFTypeRef ps_raw = IOPSCopyPowerSourcesInfo();
    if (ps_raw == NULL)
        return;
    CFArrayRef ps_array = IOPSCopyPowerSourcesList(ps_raw);
    if (ps_array == NULL) {
        CFRelease(ps_raw);
        return;
    }
    int ps_array_len = CFArrayGetCount(ps_array);

    PLUGIN_DEBUG("ps_array_len == %i", ps_array_len);

    for (int i = 0; i < ps_array_len; i++) {
        CFTypeRef ps_obj = CFArrayGetValueAtIndex(ps_array, i);
        CFDictionaryRef ps_dict = IOPSGetPowerSourceDescription(ps_raw, ps_obj);
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
            double temp_double = dict_get_double(ps_dict, kIOPSCurrentCapacityKey);
            if (!isnan((temp_double)) && (temp_double >= 0.0) && (temp_double <= 100.0))
                *ret_charge = temp_double;
        }

        if (isnan(*ret_current)) {
            double temp_double = dict_get_double(ps_dict, kIOPSCurrentKey);
            if (!isnan(temp_double))
                *ret_current = temp_double / 1000.0;
        }

        if (isnan(*ret_voltage)) {
            double temp_double = dict_get_double(ps_dict, kIOPSVoltageKey);
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
    io_iterator_t iterator;
    kern_return_t status = IOServiceGetMatchingServices((mach_port_t) 0,
                                                        IOServiceNameMatching("battery"),
                                                        &iterator);
    if (status != kIOReturnSuccess) {
        PLUGIN_DEBUG("IOServiceGetMatchingServices failed.");
        return;
    }

    io_object_t io_obj;
    while ((io_obj = IOIteratorNext(iterator))) {
        CFDictionaryRef bat_root_dict;
        status = IORegistryEntryCreateCFProperties(io_obj, (CFMutableDictionaryRef *)&bat_root_dict,
                                                           kCFAllocatorDefault, kNilOptions);
        if (status != kIOReturnSuccess) {
            PLUGIN_DEBUG("IORegistryEntryCreateCFProperties failed.");
            continue;
        }

        CFArrayRef bat_info_arry = (CFArrayRef)CFDictionaryGetValue(bat_root_dict,
                                                                    CFSTR("IOBatteryInfo"));
        if (bat_info_arry == NULL) {
            CFRelease(bat_root_dict);
            continue;
        }

        CFIndex bat_info_arry_len = CFArrayGetCount(bat_info_arry);

        for (CFIndex bat_info_arry_pos = 0; bat_info_arry_pos < bat_info_arry_len;
                 bat_info_arry_pos++) {
            CFDictionaryRef bat_info_dict = (CFDictionaryRef)CFArrayGetValueAtIndex(bat_info_arry,
                                            bat_info_arry_pos);
            if (bat_info_dict == NULL)
                continue;

            if (isnan(*ret_capacity_full)) {
                double temp_double = dict_get_double(bat_info_dict, "Capacity");
                *ret_capacity_full = temp_double / 1000.0;
            }

            if (isnan(*ret_capacity_design)) {
                double temp_double = dict_get_double(bat_info_dict, "AbsoluteMaxCapacity");
                *ret_capacity_design = temp_double / 1000.0;
            }

            if (isnan(*ret_current)) {
                double temp_double = dict_get_double(bat_info_dict, "Current");
                *ret_current = temp_double / 1000.0;
            }

            if (isnan(*ret_voltage)) {
                double temp_double = dict_get_double(bat_info_dict, "Voltage");
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
                             &LABEL_PAIR_CONST("battery", "0"), NULL);
    if (!isnan(voltage))
        metric_family_append(&fams[FAM_BATTERY_VOLTAGE], VALUE_GAUGE(voltage), NULL,
                             &LABEL_PAIR_CONST("battery", "0"), NULL);

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
    /* Some drivers return an empty string instead of an error. */
    if (buffer[0] == '\0')
        return ENODATA;

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

    /* Read each value independently, the missing ones remain NAN. */
    (void)sysfs_file_to_gauge(dir, power_supply, "energy_now", &capacity_charged);
    (void)sysfs_file_to_gauge(dir, power_supply, "energy_full", &capacity_full);
    (void)sysfs_file_to_gauge(dir, power_supply, "energy_full_design", &capacity_design);

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
    double voltage = NAN;

    /* Not all drivers export voltage_min_design, fall back to other voltages. */
    if ((sysfs_file_to_gauge(dir, power_supply, "voltage_min_design", &voltage) != 0) &&
        (sysfs_file_to_gauge(dir, power_supply, "voltage_max_design", &voltage) != 0) &&
        (sysfs_file_to_gauge(dir, power_supply, "voltage_now", &voltage) != 0)) {
        PLUGIN_DEBUG("Cannot get the voltage of battery '%s'.", power_supply);
        return ENOENT;
    }
    voltage *= SYSFS_FACTOR;

    /* Read each value independently, the missing ones remain NAN. */
    (void)sysfs_file_to_gauge(dir, power_supply, "charge_now", &capacity_charged);
    capacity_charged *= voltage;

    (void)sysfs_file_to_gauge(dir, power_supply, "charge_full", &capacity_full);
    capacity_full *= voltage;

    (void)sysfs_file_to_gauge(dir, power_supply, "charge_full_design", &capacity_design);
    capacity_design *= voltage;

    submit_capacity(device, capacity_charged * SYSFS_FACTOR,
                            capacity_full * SYSFS_FACTOR, capacity_design * SYSFS_FACTOR);
    return 0;
}

static int read_sysfs_callback(__attribute__((unused)) int dir_fd, char const *dir,
                               char const *power_supply, __attribute__((unused)) void *user_data)
{
    char const *device = power_supply;
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

    /* Ignore batteries of peripheral devices (mouses, keyboards, ...). */
    status = sysfs_file_to_buffer(dir, power_supply, "scope", buffer, sizeof(buffer));
    if ((status == 0) && (strcasecmp("Device", buffer) == 0))
        return 0;

    (void)sysfs_file_to_buffer(dir, power_supply, "status", buffer, sizeof(buffer));
    if (strcasecmp("Discharging", buffer) == 0)
        discharging = true;

    /* read_sysfs_capacity fails if energy_now is not available. */
    if (read_sysfs_capacity(dir, power_supply, device) != 0)
        read_sysfs_capacity_from_charge(dir, power_supply, device);

    if (sysfs_file_to_gauge(dir, power_supply, "power_now", &v) == 0) {
        /* Some drivers already report negative values when discharging. */
        if (discharging && (v > 0))
            v *= -1.0;
        metric_family_append(&fams[FAM_BATTERY_POWER], VALUE_GAUGE(v * SYSFS_FACTOR), NULL,
                             &LABEL_PAIR_CONST("battery", device), NULL);
    }
    if (sysfs_file_to_gauge(dir, power_supply, "current_now", &v) == 0) {
        /* Some drivers already report negative values when discharging. */
        if (discharging && (v > 0))
            v *= -1.0;
        metric_family_append(&fams[FAM_BATTERY_CURRENT], VALUE_GAUGE(v * SYSFS_FACTOR), NULL,
                             &LABEL_PAIR_CONST("battery", device), NULL);
    }

    if (sysfs_file_to_gauge(dir, power_supply, "voltage_now", &v) == 0)
        metric_family_append(&fams[FAM_BATTERY_VOLTAGE], VALUE_GAUGE(v * SYSFS_FACTOR), NULL,
                             &LABEL_PAIR_CONST("battery", device), NULL);

    return 0;
}

static int read_sysfs(void)
{
    int status;

    if (access(path_sys_power_supply, R_OK) != 0)
        return ENOENT;

    status = walk_directory(path_sys_power_supply, read_sysfs_callback,
                                                   /* user_data = */ NULL,
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
                              char const *power_supply, __attribute__((unused)) void *user_data)
{
    double power = NAN;
    double voltage = NAN;
    double capacity_charged = NAN;
    double capacity_full = NAN;
    double capacity_design = NAN;
    bool charging = false;
    bool is_current = false;

    char const *device = power_supply;
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

    read_acpi_full_capacity(dir, power_supply, &capacity_full, &capacity_design);

    submit_capacity(device, capacity_charged * PROC_ACPI_FACTOR,
                            capacity_full * PROC_ACPI_FACTOR,
                            capacity_design * PROC_ACPI_FACTOR);

    if (!isnan(power)) {
        if (is_current) {
            metric_family_append(&fams[FAM_BATTERY_CURRENT], VALUE_GAUGE(power * PROC_ACPI_FACTOR),
                                 NULL, &LABEL_PAIR_CONST("battery", device), NULL);
        } else {
            metric_family_append(&fams[FAM_BATTERY_POWER], VALUE_GAUGE(power * PROC_ACPI_FACTOR),
                                 NULL, &LABEL_PAIR_CONST("battery", device), NULL);
        }
    }

    if (!isnan(voltage))
        metric_family_append(&fams[FAM_BATTERY_VOLTAGE], VALUE_GAUGE(voltage * PROC_ACPI_FACTOR),
                             NULL, &LABEL_PAIR_CONST("battery", device), NULL);

    return 0;
}

static int read_acpi(void)
{
    int status;

    if (access(path_proc_acpi, R_OK) != 0)
        return ENOENT;

    status = walk_directory(path_proc_acpi, read_acpi_callback,
                                            /* user_data = */ NULL,
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

        if (!isnan(charge))
            metric_family_append(&fams[FAM_BATTERY_CAPACITY_CHARGED], VALUE_GAUGE(charge / 1000.0),
                                 NULL, &LABEL_PAIR_CONST("battery", device), NULL);
        if (!isnan(current))
            metric_family_append(&fams[FAM_BATTERY_CURRENT], VALUE_GAUGE(current / 1000.0),
                                 NULL, &LABEL_PAIR_CONST("battery", device), NULL);
        if (!isnan(voltage))
            metric_family_append(&fams[FAM_BATTERY_VOLTAGE], VALUE_GAUGE(voltage / 1000.0),
                                 NULL, &LABEL_PAIR_CONST("battery", device), NULL);
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
    plugin_register_read("battery", battery_read);
}
