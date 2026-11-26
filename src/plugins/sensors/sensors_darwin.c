// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2006-2007 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "sensors.h"

#ifdef HAVE_MACH_MACH_TYPES_H
#        include <mach/mach_types.h>
#endif
#ifdef HAVE_MACH_MACH_INIT_H
#        include <mach/mach_init.h>
#endif
#ifdef HAVE_MACH_MACH_ERROR_H
#        include <mach/mach_error.h>
#endif
#ifdef HAVE_MACH_MACH_PORT_H
#        include <mach/mach_port.h>
#endif
#ifdef HAVE_COREFOUNDATION_COREFOUNDATION_H
#        include <CoreFoundation/CoreFoundation.h>
#endif
#ifdef HAVE_IOKIT_IOKITLIB_H
#        include <IOKit/IOKitLib.h>
#endif
#ifdef HAVE_IOKIT_IOTYPES_H
#        include <IOKit/IOTypes.h>
#endif

#if (MAC_OS_X_VERSION_MIN_REQUIRED < 120000) // Before macOS 12 Monterey
#define IOMainPort IOMasterPort
#endif

static mach_port_t io_main_port = MACH_PORT_NULL;

int ncsensors_read(void)
{
    if (!io_main_port || (io_main_port == MACH_PORT_NULL))
        return -1;

    io_iterator_t iterator;
    kern_return_t status = IOServiceGetMatchingServices(io_main_port,
                                                        IOServiceNameMatching("IOHWSensor"),
                                                        &iterator);
    if (status != kIOReturnSuccess) {
        PLUGIN_ERROR("IOServiceGetMatchingServices failed: %s", mach_error_string(status));
        return -1;
    }

    io_object_t io_obj;

    while ((io_obj = IOIteratorNext(iterator))) {
        CFMutableDictionaryRef prop_dict = NULL;
        CFTypeRef property = NULL;
        char type[256];
        char location[256];
        int value_int = 0;
        int fam = -1;
        double value_double = 0;

        status = IORegistryEntryCreateCFProperties(io_obj, &prop_dict, kCFAllocatorDefault,
                                                   kNilOptions);
        if (status != kIOReturnSuccess) {
            PLUGIN_DEBUG("IORegistryEntryCreateCFProperties failed: %s", mach_error_string(status));
            IOObjectRelease(io_obj);
            continue;
        }

        /* Copy the sensor type. */
        if (!CFDictionaryGetValueIfPresent(prop_dict, CFSTR("type"), &property))
            goto next;
        if (CFGetTypeID(property) != CFStringGetTypeID())
            goto next;
        if (!CFStringGetCString(property, type, sizeof(type), kCFStringEncodingASCII))
            goto next;
        type[sizeof(type) - 1] = '\0';

        /* Copy the sensor location. This will be used as `instance'. */
        property = NULL;
        if (!CFDictionaryGetValueIfPresent(prop_dict, CFSTR("location"), &property))
            goto next;
        if (CFGetTypeID(property) != CFStringGetTypeID())
            goto next;
        if (!CFStringGetCString(property, location, sizeof(location), kCFStringEncodingASCII))
            goto next;

        /* Get the actual value. Some computation, based on the `type' is necessary. */
        property = NULL;
        if (!CFDictionaryGetValueIfPresent(prop_dict, CFSTR("current-value"), &property))
            goto next;
        if (CFGetTypeID(property) != CFNumberGetTypeID())
            goto next;
        if (!CFNumberGetValue(property, kCFNumberIntType, &value_int))
            goto next;

        if (strcmp(type, "temperature") == 0) {
            value_double = ((double)value_int) / 65536.0;
            fam = FAM_SENSOR_TEMPERATURE_CELSIUS;
        } else if (strcmp(type, "temp") == 0) {
            value_double = ((double)value_int) / 10.0;
            fam = FAM_SENSOR_TEMPERATURE_CELSIUS;
        } else if (strcmp(type, "fanspeed") == 0) {
            value_double = ((double)value_int) / 65536.0;
            fam = FAM_SENSOR_FAN_SPEED_RPM;
        } else if (strcmp(type, "voltage") == 0) {
            value_double = ((double)value_int) / 65536.0;
            fam = FAM_SENSOR_VOLTAGE_VOLTS;
        } else {
            PLUGIN_DEBUG("Read unknown sensor type: %s", type);
        }

        if (fam >= 0)
            metric_family_append(&sensors_fams[fam], VALUE_GAUGE(value_double), NULL,
                                 &LABEL_PAIR_CONST("location", location), NULL);
next:
        CFRelease(prop_dict);
        IOObjectRelease(io_obj);
    }

    IOObjectRelease(iterator);

    plugin_dispatch_metric_family_array_filtered(sensors_fams, FAM_SENSOR_MAX, sensors_filter, 0);

    return 0;
}

int ncsensors_init(void)
{
    if (io_main_port != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), io_main_port);
        io_main_port = MACH_PORT_NULL;
    }

    kern_return_t status = IOMainPort(MACH_PORT_NULL, &io_main_port);
    if (status != kIOReturnSuccess) {
        PLUGIN_ERROR("IOMainPort failed: %s", mach_error_string(status));
        io_main_port = MACH_PORT_NULL;
        return -1;
    }

    return 0;
}

int ncsensors_shutdown(void)
{
    if (io_main_port != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), io_main_port);
        io_main_port = MACH_PORT_NULL;
    }

    plugin_filter_free(sensors_filter);
    return 0;
}
