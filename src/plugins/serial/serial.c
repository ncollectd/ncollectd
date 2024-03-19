// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2005,2006 David Bacher
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: David Bacher <drbacher at gmail.com>
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

static char *path_proc_serial;

enum {
    FAM_SERIAL_READ,
    FAM_SERIAL_WRITE,
    FAM_SERIAL_FRAMING_ERRORS,
    FAM_SERIAL_PARITY_ERRORS,
    FAM_SERIAL_BREAK_CONDITIONS,
    FAM_SERIAL_OVERRUN_ERRORS,
    FAM_SERIAL_MAX,
};

static metric_family_t fams[FAM_SERIAL_MAX] = {
    [FAM_SERIAL_READ] = {
        .name = "system_serial_read_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes read in serial port",
    },
    [FAM_SERIAL_WRITE] = {
        .name = "system_serial_write_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total bytes written in serial port",
    },
    [FAM_SERIAL_FRAMING_ERRORS] = {
        .name = "system_serial_framing_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total framing errors (stop bit not found) in serial port",
    },
    [FAM_SERIAL_PARITY_ERRORS] = {
        .name = "system_serial_parity_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total parity errors in serial port",
    },
    [FAM_SERIAL_BREAK_CONDITIONS] = {
        .name = "system_serial_break_conditions",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total break conditions in serial port",
    },
    [FAM_SERIAL_OVERRUN_ERRORS] = {
        .name = "system_serial_overrun_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total receiver overrun errors in serial port",
    },
};

static int serial_read(void)
{
    FILE *fh = fopen(path_proc_serial, "r");
    if (unlikely(fh == NULL)) {
        PLUGIN_WARNING("Cannot open '%s': %s", path_proc_serial, STRERRNO);
        return -1;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[16];
        int numfields = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields < 6)
            continue;

        /*
         * 0: uart:16550A port:000003F8 irq:4 tx:0 rx:0
         * 1: uart:16550A port:000002F8 irq:3 tx:0 rx:0
         */
        size_t len = strlen(fields[0]);
        if (len < 2)
            continue;
        if (fields[0][len - 1] != ':')
            continue;
        fields[0][len - 1] = '\0';

        metric_t m = {0};
        metric_label_set(&m, "line", fields[0]);

        uint64_t value = 0;

        for (int i = 1; i < numfields; i++) {
            len = strlen(fields[i]);
            if (len < 4)
                continue;

            switch (fields[i][0]) {
            case 'i':
                if (strncmp(fields[i], "irq:", 4) == 0)
                    metric_label_set(&m, "irq", fields[i]+4);
                break;
            case 't':
                if (strncmp(fields[i], "tx:", 3) == 0) {
                    if (strtouint(fields[i] + 3, &value) == 0) {
                        m.value = VALUE_COUNTER(value);
                        metric_family_metric_append(&fams[FAM_SERIAL_READ], m);
                    }
                }
                break;
            case 'r':
                if (strncmp(fields[i], "rx:", 3) == 0) {
                    if (strtouint(fields[i] + 3, &value) == 0) {
                         m.value = VALUE_COUNTER(value);
                         metric_family_metric_append(&fams[FAM_SERIAL_WRITE], m);
                    }
                }
                break;
            case 'f':
                if (strncmp(fields[i], "fe:", 3) == 0) {
                    if (strtouint(fields[i] + 3, &value) == 0) {
                        m.value = VALUE_COUNTER(value);
                        metric_family_metric_append(&fams[FAM_SERIAL_FRAMING_ERRORS], m);
                    }
                }
                break;
            case 'p':
                if (strncmp(fields[i], "pe:", 3) == 0) {
                    if (strtouint(fields[i] + 3, &value) == 0) {
                        m.value = VALUE_COUNTER(value);
                        metric_family_metric_append(&fams[FAM_SERIAL_PARITY_ERRORS], m);
                    }
                }
                break;
            case 'b':
                if (strncmp(fields[i], "brk:", 4) == 0) {
                    if (strtouint(fields[i] + 4, &value) == 0) {
                        m.value = VALUE_COUNTER(value);
                        metric_family_metric_append(&fams[FAM_SERIAL_BREAK_CONDITIONS], m);
                    }
                }
                break;
            case 'o':
                if (strncmp(fields[i], "oe:", 3) == 0) {
                    if (strtouint(fields[i] + 3, &value) == 0) {
                        m.value = VALUE_COUNTER(value);
                        metric_family_metric_append(&fams[FAM_SERIAL_OVERRUN_ERRORS], m);
                    }
                }
                break;
            }
        }

        metric_reset(&m, METRIC_TYPE_COUNTER);
    }
    fclose(fh);

    plugin_dispatch_metric_family_array(fams, FAM_SERIAL_MAX, 0);
    return 0;
}

static int serial_init(void)
{
    path_proc_serial = plugin_procpath("tty/driver/serial");
    if (path_proc_serial == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int serial_shutdown(void)
{
    free(path_proc_serial);
    return 0;
}

void module_register(void)
{
    plugin_register_init("serial", serial_init);
    plugin_register_read("serial", serial_read);
    plugin_register_shutdown("serial", serial_shutdown);
}
