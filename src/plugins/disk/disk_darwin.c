// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2012 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#ifdef HAVE_MACH_MACH_TYPES_H
#    include <mach/mach_types.h>
#endif
#ifdef HAVE_MACH_MACH_INIT_H
#    include <mach/mach_init.h>
#endif
#ifdef HAVE_MACH_MACH_ERROR_H
#    include <mach/mach_error.h>
#endif
#ifdef HAVE_MACH_MACH_PORT_H
#    include <mach/mach_port.h>
#endif
#ifdef HAVE_COREFOUNDATION_COREFOUNDATION_H
#    include <CoreFoundation/CoreFoundation.h>
#endif
#ifdef HAVE_IOKIT_IOKITLIB_H
#    include <IOKit/IOKitLib.h>
#endif
#ifdef HAVE_IOKIT_IOTYPES_H
#    include <IOKit/IOTypes.h>
#endif
#ifdef HAVE_IOKIT_STORAGE_IOBLOCKSTORAGEDRIVER_H
#    include <IOKit/storage/IOBlockStorageDriver.h>
#endif
#ifdef HAVE_IOKIT_IOBSD_H
#    include <IOKit/IOBSD.h>
#endif

#if !defined(MAC_OS_VERSION_12_0) || (MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_VERSION_12_0)
#define IOMainPort IOMasterPort
#endif

#include "disk.h"

extern exclist_t excl_disk;
extern char *conf_udev_name_attr;
extern metric_family_t fams[];

static mach_port_t io_master_port = MACH_PORT_NULL;
/* This defaults to false for backwards compatibility. Please fix in the next
 * major version. */
extern bool use_bsd_name;

static signed long long dict_get_value(CFDictionaryRef dict, const char *key)
{
    /* `key_obj' needs to be released. */
    CFStringRef key_obj = CFStringCreateWithCString(kCFAllocatorDefault, key,
                                                    kCFStringEncodingASCII);
    if (key_obj == NULL) {
        PLUGIN_DEBUG("CFStringCreateWithCString (%s) failed.", key);
        return -1LL;
    }

    /* get => we don't need to release (== free) the object */
    CFNumberRef val_obj = (CFNumberRef)CFDictionaryGetValue(dict, key_obj);

    CFRelease(key_obj);

    if (val_obj == NULL) {
        PLUGIN_DEBUG("CFDictionaryGetValue (%s) failed.", key);
        return -1LL;
    }

    signed long long val_int;
    if (!CFNumberGetValue(val_obj, kCFNumberSInt64Type, &val_int)) {
        PLUGIN_DEBUG("CFNumberGetValue (%s) failed.", key);
        return -1LL;
    }

    return val_int;
}

int disk_read(void)
{
    io_registry_entry_t disk;
    io_registry_entry_t disk_child;
    io_iterator_t disk_list;
    CFMutableDictionaryRef props_dict, child_dict;
    CFDictionaryRef stats_dict;
    CFStringRef tmp_cf_string_ref;
    kern_return_t status;

    signed long long read_ops, read_byt, read_tme;
    signed long long write_ops, write_byt, write_tme;

    int disk_major, disk_minor;
    char disk_name[DATA_MAX_NAME_LEN];
    char child_disk_name_bsd[DATA_MAX_NAME_LEN],
         props_disk_name_bsd[DATA_MAX_NAME_LEN];

    /* Get the list of all disk objects. */
    if (IOServiceGetMatchingServices(io_master_port, IOServiceMatching(kIOBlockStorageDriverClass),
                    &disk_list) != kIOReturnSuccess) {
        PLUGIN_ERROR("IOServiceGetMatchingServices failed.");
        return -1;
    }

    while ((disk = IOIteratorNext(disk_list)) != 0) {
        props_dict = NULL;
        stats_dict = NULL;
        child_dict = NULL;

        /* get child of disk entry and corresponding property dictionary */
        if ((status = IORegistryEntryGetChildEntry(
                         disk, kIOServicePlane, &disk_child)) != kIOReturnSuccess) {
            /* This fails for example for DVD/CD drives, which we want to ignore
             * anyway */
            PLUGIN_DEBUG("IORegistryEntryGetChildEntry (disk) failed: 0x%08x", status);
            IOObjectRelease(disk);
            continue;
        }
        if (IORegistryEntryCreateCFProperties(disk_child, (CFMutableDictionaryRef *)&child_dict,
            kCFAllocatorDefault, kNilOptions) != kIOReturnSuccess || child_dict == NULL) {
            PLUGIN_ERROR("IORegistryEntryCreateCFProperties (disk_child) failed.");
            IOObjectRelease(disk_child);
            IOObjectRelease(disk);
            continue;
        }

        /* extract name and major/minor numbers */
        memset(child_disk_name_bsd, 0, sizeof(child_disk_name_bsd));
        tmp_cf_string_ref = (CFStringRef)CFDictionaryGetValue(child_dict, CFSTR(kIOBSDNameKey));
        if (tmp_cf_string_ref) {
            assert(CFGetTypeID(tmp_cf_string_ref) == CFStringGetTypeID());
            CFStringGetCString(tmp_cf_string_ref, child_disk_name_bsd,
                               sizeof(child_disk_name_bsd), kCFStringEncodingUTF8);
        }
        disk_major = (int)dict_get_value(child_dict, kIOBSDMajorKey);
        disk_minor = (int)dict_get_value(child_dict, kIOBSDMinorKey);
        PLUGIN_DEBUG("child_disk_name_bsd=\"%s\" major=%d minor=%d",
                    child_disk_name_bsd, disk_major, disk_minor);
        CFRelease(child_dict);
        IOObjectRelease(disk_child);

        /* get property dictionary of the disk entry itself */
        if (IORegistryEntryCreateCFProperties(
                        disk, (CFMutableDictionaryRef *)&props_dict, kCFAllocatorDefault,
                        kNilOptions) != kIOReturnSuccess ||
                props_dict == NULL) {
            PLUGIN_ERROR("IORegistryEntryCreateCFProperties failed.");
            IOObjectRelease(disk);
            continue;
        }

        /* extract name and stats dictionary */
        memset(props_disk_name_bsd, 0, sizeof(props_disk_name_bsd));
        tmp_cf_string_ref =
                (CFStringRef)CFDictionaryGetValue(props_dict, CFSTR(kIOBSDNameKey));
        if (tmp_cf_string_ref) {
            assert(CFGetTypeID(tmp_cf_string_ref) == CFStringGetTypeID());
            CFStringGetCString(tmp_cf_string_ref, props_disk_name_bsd, sizeof(props_disk_name_bsd),
                               kCFStringEncodingUTF8);
        }
        stats_dict = (CFDictionaryRef)CFDictionaryGetValue(
                props_dict, CFSTR(kIOBlockStorageDriverStatisticsKey));
        if (stats_dict == NULL) {
            PLUGIN_ERROR("CFDictionaryGetValue (%s) failed.", kIOBlockStorageDriverStatisticsKey);
            CFRelease(props_dict);
            IOObjectRelease(disk);
            continue;
        }
        PLUGIN_DEBUG("props_disk_name_bsd=\"%s\"", props_disk_name_bsd);

        /* choose name */
        if (use_bsd_name) {
            if (child_disk_name_bsd[0] != 0)
                sstrncpy(disk_name, child_disk_name_bsd, sizeof(disk_name));
            else if (props_disk_name_bsd[0] != 0)
                sstrncpy(disk_name, props_disk_name_bsd, sizeof(disk_name));
            else {
                PLUGIN_ERROR("can't find bsd disk name.");
                ssnprintf(disk_name, sizeof(disk_name), "%i-%i", disk_major, disk_minor);
            }
        } else
            ssnprintf(disk_name, sizeof(disk_name), "%i-%i", disk_major, disk_minor);

        PLUGIN_DEBUG("disk_name = \"%s\"", disk_name);

        if (!exclist_match(&excl_disk, disk_name)) {
            CFRelease(props_dict);
            IOObjectRelease(disk);
            continue;
        }

        /* extract the stats */
        read_ops = dict_get_value(stats_dict, kIOBlockStorageDriverStatisticsReadsKey);
        read_byt = dict_get_value(stats_dict, kIOBlockStorageDriverStatisticsBytesReadKey);
        read_tme = dict_get_value(stats_dict, kIOBlockStorageDriverStatisticsTotalReadTimeKey);
        write_ops = dict_get_value(stats_dict, kIOBlockStorageDriverStatisticsWritesKey);
        write_byt = dict_get_value(stats_dict, kIOBlockStorageDriverStatisticsBytesWrittenKey);
        write_tme = dict_get_value(stats_dict, kIOBlockStorageDriverStatisticsTotalWriteTimeKey);
        CFRelease(props_dict);
        IOObjectRelease(disk);

        /* and submit */
        if ((read_byt != -1LL) || (write_byt != -1LL)) {
            metric_family_append(&fams[FAM_DISK_READ_BYTES], VALUE_COUNTER(read_byt), NULL,
                                 &(label_pair_const_t){.name="device", .value=disk_name}, NULL);
            metric_family_append(&fams[FAM_DISK_WRITE_BYTES], VALUE_COUNTER(write_byt), NULL,
                                 &(label_pair_const_t){.name="device", .value=disk_name}, NULL);
        }
        if ((read_ops != -1LL) || (write_ops != -1LL)) {
            metric_family_append(&fams[FAM_DISK_READ_OPS], VALUE_COUNTER(read_ops), NULL,
                                 &(label_pair_const_t){.name="device", .value=disk_name}, NULL);

            metric_family_append(&fams[FAM_DISK_WRITE_OPS], VALUE_COUNTER(write_ops), NULL,
                                 &(label_pair_const_t){.name="device", .value=disk_name}, NULL);
        }
        if ((read_tme != -1LL) || (write_tme != -1LL)) {
            metric_family_append(&fams[FAM_DISK_READ_TIME],
                                 VALUE_COUNTER_FLOAT64((double)read_tme * 1e-9), NULL,
                                 &(label_pair_const_t){.name="device", .value=disk_name}, NULL);

            metric_family_append(&fams[FAM_DISK_WRITE_TIME],
                                 VALUE_COUNTER_FLOAT64((double)write_tme * 1e-9), NULL,
                                 &(label_pair_const_t){.name="device", .value=disk_name}, NULL);
        }
    }

    IOObjectRelease(disk_list);

    plugin_dispatch_metric_family_array(fams, FAM_DISK_MAX, 0);
    return 0;
}

int disk_init(void)
{
    if (io_master_port != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), io_master_port);
        io_master_port = MACH_PORT_NULL;
    }

    kern_return_t status = IOMainPort(MACH_PORT_NULL, &io_master_port);
    if (status != kIOReturnSuccess) {
        PLUGIN_ERROR("IOMainPort failed: %s", mach_error_string(status));
        io_master_port = MACH_PORT_NULL;
        return -1;
    }

    return 0;
}

int disk_shutdown(void)
{
    if (io_master_port != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), io_master_port);
        io_master_port = MACH_PORT_NULL;
    }

    exclist_reset(&excl_disk);

    return 0;
}
