// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

static char *path_proc_softirqs;

static exclist_t excl_softirq;

static metric_family_t fam = {
    .name = "system_softirq",
    .type = METRIC_TYPE_COUNTER,
    .help = "Counts of softirq handlers serviced since boot time, for each CPU."
};

static int softirq_read(void)
{
    /*
     * Example content:
     *                    CPU0       CPU1       CPU2       CPU3
     *          HI:          0          0          0          0
     *       TIMER:     472857     485158     495586     959256
     *      NET_TX:       1024        843        952      50626
     *      NET_RX:      11825      12586      11671      32979
     *       BLOCK:      36247      45217      32037      31845
     *    IRQ_POLL:          0          0          0          0
     *     TASKLET:          1          1          1          1
     *       SCHED:    9109146    3315427    2641233    4153576
     *     HRTIMER:          0          0          2         76
     *         RCU:    3282442    3150050    3131744    4257753
     */

    FILE *fh = fopen(path_proc_softirqs, "r");
    if (unlikely(fh == NULL)) {
        PLUGIN_ERROR("Cannot open '%s': %s", path_proc_softirqs, STRERRNO);
        return -1;
    }

    /* Get CPU count from the first line */
    char cpu_buffer[1024];
    char *cpu_fields[256];
    int cpu_count;

    if (unlikely(fgets(cpu_buffer, sizeof(cpu_buffer), fh) != NULL)) {
        cpu_count = strsplit(cpu_buffer, cpu_fields, STATIC_ARRAY_SIZE(cpu_fields));
        for (int i = 0; i < cpu_count; i++) {
            if (strncmp(cpu_fields[i], "CPU", 3) == 0)
                cpu_fields[i] += 3;
        }
    } else {
        PLUGIN_ERROR("Unable to get CPU count from first line of '%s'", path_proc_softirqs);
        fclose(fh);
        return -1;
    }

    char buffer[1024];
    char *fields[256];

    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (unlikely(fields_num < 2))
            continue;

        int softirq_values_to_parse = fields_num - 1;
        if (fields_num >= cpu_count + 1)
            softirq_values_to_parse = cpu_count;

        /* First field is softirq name and colon */
        char *softirq_name = fields[0];
        size_t softirq_name_len = strlen(softirq_name);
        if (softirq_name_len < 2)
            continue;

        if (softirq_name[softirq_name_len - 1] != ':')
            continue;

        softirq_name[softirq_name_len - 1] = '\0';
        softirq_name_len--;

        if (!exclist_match(&excl_softirq, softirq_name))
            continue;

        for (int i = 1; i <= softirq_values_to_parse; i++) {
            /* Per-CPU value */
            uint64_t v;
            int status = parse_uinteger(fields[i], &v);
            if (status != 0)
                break;
            metric_family_append(&fam, VALUE_COUNTER(v), NULL,
                                 &(label_pair_const_t){.name="cpu", .value=cpu_fields[i-1]},
                                 &(label_pair_const_t){.name="softirq", .value=softirq_name},
                                 NULL);
        }
    }
    fclose(fh);

    plugin_dispatch_metric_family(&fam, 0);

    return 0;
}

static int softirq_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "soft-irq") == 0) {
            status = cf_util_exclist(child, &excl_softirq);
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

static int softirq_init(void)
{
    path_proc_softirqs = plugin_procpath("softirqs");
    if (path_proc_softirqs == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int softirq_shutdown(void)
{
    free(path_proc_softirqs);
    exclist_reset(&excl_softirq);
    return 0;
}

void module_register(void)
{
    plugin_register_init("softirq", softirq_init);
    plugin_register_config("softirq", softirq_config);
    plugin_register_read("softirq", softirq_read);
    plugin_register_shutdown("softirq", softirq_shutdown);
}
