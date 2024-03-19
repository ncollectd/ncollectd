// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2007 Peter Holik
// SPDX-FileCopyrightText: Copyright (C) 2011 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Peter Holik <peter at holik.at>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#if !defined(KERNEL_LINUX) && !defined(KERNEL_NETBSD)
#error "No applicable input method."
#endif

#ifdef KERNEL_NETBSD
#include <malloc.h>
#include <sys/evcnt.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

static metric_family_t fam = {
    .name = "system_interrupts",
    .type = METRIC_TYPE_COUNTER,
    .help = "The total number of interrupts per CPU per IO device.",
};

#ifdef KERNEL_LINUX
static char *path_proc_interrupts;
#endif

static exclist_t excl_irq;

static int irq_read(void)
{
#ifdef KERNEL_LINUX
   /* Example content:
    *         CPU0       CPU1       CPU2       CPU3
    * 0:       2574          1          3          2   IO-APIC-edge      timer
    * 1:     102553     158669     218062      70587   IO-APIC-edge      i8042
    * 8:          0          0          0          1   IO-APIC-edge      rtc0
    */
    FILE *fh = fopen(path_proc_interrupts, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Cannot fopen '%s': %s", path_proc_interrupts, STRERRNO);
        return -1;
    }

    /* Get CPU count from the first line */
    char cpu_buffer[1024];
    char *cpu_fields[256];
    int cpu_count;

    if (fgets(cpu_buffer, sizeof(cpu_buffer), fh) != NULL) {
        cpu_count = strsplit(cpu_buffer, cpu_fields, STATIC_ARRAY_SIZE(cpu_fields));
        for (int i = 0; i < cpu_count; i++) {
            if (strncmp(cpu_fields[i], "CPU", 3) == 0)
                cpu_fields[i] += 3;
        }
    } else {
        PLUGIN_ERROR("unable to get CPU count from first line of '%s'.", path_proc_interrupts);
        fclose(fh);
        return -1;
    }

    char buffer[1024];
    char *fields[256];

    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (fields_num < 2)
            continue;

        /* Parse this many numeric fields, skip the rest
         * (+1 because first there is a name of irq in each line) */
        int irq_values_to_parse;
        if (fields_num >= cpu_count + 1)
            irq_values_to_parse = cpu_count;
        else
            irq_values_to_parse = fields_num - 1;

        /* First field is irq name and colon */
        char *irq_name = fields[0];
        size_t irq_name_len = strlen(irq_name);
        if (irq_name_len < 2)
            continue;

        /* Check if irq name ends with colon.
         * Otherwise it's a header. */
        if (irq_name[irq_name_len - 1] != ':')
            continue;

        /* Is it the the ARM fast interrupt (FIQ)? */
        if (irq_name_len == 4 && (strncmp(irq_name, "FIQ:", 4) == 0))
            continue;

        irq_name[irq_name_len - 1] = '\0';
        irq_name_len--;

        if (!exclist_match(&excl_irq, irq_name))
            continue;

        for (int i = 1; i <= irq_values_to_parse; i++) {
            /* Per-CPU value */
            uint64_t value;
            int status = parse_uinteger(fields[i], &value);
            if (status != 0)
                break;
            metric_family_append(&fam, VALUE_COUNTER(value), NULL,
                                 &(label_pair_const_t){.name="irq", .value=irq_name},
                                 &(label_pair_const_t){.name="cpu", .value=cpu_fields[i - 1]},
                                 NULL);
        }
    }

    fclose(fh);
#elif defined(KERNEL_NETBSD)

    const int mib[4] = {CTL_KERN, KERN_EVCNT, EVCNT_TYPE_INTR, KERN_EVCNT_COUNT_NONZERO};
    size_t buflen = 0;
    void *buf = NULL;
    const struct evcnt_sysctl *evs, *last_evs;

    for (;;) {
        size_t newlen;
        int error;

        newlen = buflen;
        if (buflen)
            buf = malloc(buflen);
        error = sysctl(mib, __arraycount(mib), buf, &newlen, NULL, 0);
        if (error) {
            PLUGIN_ERROR("failed to get event count");
            return -1;
        }
        if (newlen <= buflen) {
            buflen = newlen;
            break;
        }
        if (buf)
            free(buf);
        buflen = newlen;
    }
    evs = buf;
    last_evs = (void *)((char *)buf + buflen);
    buflen /= sizeof(uint64_t);
    while (evs < last_evs && buflen > sizeof(*evs) / sizeof(uint64_t) && buflen >= evs->ev_len) {
        char irqname[80];

        snprintf(irqname, 80, "%s-%s", evs->ev_strings, evs->ev_strings + evs->ev_grouplen + 1);

        if (exclist_match(&excl_irq, irqname)) {
            metric_family_append(&fam, VALUE_COUNTER(evs->ev_count), NULL,
                                 &(label_pair_const_t){.name="irq", .value=irqname}, NULL);
        }

        buflen -= evs->ev_len;
        evs = (const void *)((const uint64_t *)evs + evs->ev_len);
    }
    free(buf);
#endif

    plugin_dispatch_metric_family(&fam, 0);
    return 0;
}

static int irq_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "irq") == 0) {
            status = cf_util_exclist(child, &excl_irq);
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

#ifdef KERNEL_LINUX
static int irq_init(void)
{
    path_proc_interrupts = plugin_procpath("interrupts");
    if (path_proc_interrupts == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}
#endif

static int irq_shutdown(void)
{
#ifdef KERNEL_LINUX
    free(path_proc_interrupts);
#endif
    exclist_reset(&excl_irq);
    return 0;
}

void module_register(void)
{
#ifdef KERNEL_LINUX
    plugin_register_init("irq", irq_init);
#endif
    plugin_register_shutdown("irq", irq_shutdown);
    plugin_register_config("irq", irq_config);
    plugin_register_read("irq", irq_read);
}
