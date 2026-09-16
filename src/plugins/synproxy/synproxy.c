// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2017 Marek Becka
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Marek Becka <https://github.com/marekbecka>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

#define SYNPROXY_FIELDS 6

static char *path_proc_synproxy;

enum {
    FAM_SYNPROXY_CONNECTIONS_SYN_RECEIVED,
    FAM_SYNPROXY_COOKIES_INVALID,
    FAM_SYNPROXY_COOKIES_VALID,
    FAM_SYNPROXY_COOKIES_RETRANSMISSION,
    FAM_SYNPROXY_CONNECTIONS_REOPENED,
    FAM_SYNPROXY_MAX,
};

static metric_family_t fams[FAM_SYNPROXY_MAX] = {
    [FAM_SYNPROXY_CONNECTIONS_SYN_RECEIVED] = {
        .name = "system_synproxy_connections_syn_received",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of SYN received.",
    },
    [FAM_SYNPROXY_COOKIES_INVALID] = {
        .name = "system_synproxy_cookies_invalid",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of invalid cookies.",
    },
    [FAM_SYNPROXY_COOKIES_VALID] = {
        .name = "system_synproxy_cookies_valid",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of valid cookies.",
    },
    [FAM_SYNPROXY_COOKIES_RETRANSMISSION] = {
        .name = "system_synproxy_cookies_retransmission",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of cookies retransmitted.",
    },
    [FAM_SYNPROXY_CONNECTIONS_REOPENED] = {
        .name = "system_synproxy_connections_reopened",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of connections reopened.",
    },
};

static int synproxy_read(void)
{
    FILE *fh = fopen(path_proc_synproxy, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Unable to open '%s': %s", path_proc_synproxy, STRERRNO);
        return -1;
    }

    uint64_t results[SYNPROXY_FIELDS] = {0};

    bool is_header = true;
    char buf[1024];
    while (fgets(buf, sizeof(buf), fh) != NULL) {
        if (is_header) {
            is_header = false;
            continue;
        }

        char *fields[SYNPROXY_FIELDS];
        int numfields = strsplit(buf, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields != SYNPROXY_FIELDS) {
            PLUGIN_ERROR("Unexpected number of columns in '%s'", path_proc_synproxy);
            fclose(fh);
            return -1;
        }

        /* 1st column (entries) is hardcoded to 0 in kernel code */
        for (size_t n = 1; n < SYNPROXY_FIELDS; n++) {
            char *endptr = NULL;
            errno = 0;
            uint64_t value = (uint64_t)strtoull(fields[n], &endptr, 16);
            if ((endptr == fields[n]) || (errno != 0)) {
                PLUGIN_ERROR("Unable to parse value: '%s'", fields[n]);
                fclose(fh);
                return -1;
            }

            results[n] += value;
        }
    }

    fclose(fh);

    metric_family_append(&fams[FAM_SYNPROXY_CONNECTIONS_SYN_RECEIVED],
                         VALUE_COUNTER(results[1]), NULL, NULL);
    metric_family_append(&fams[FAM_SYNPROXY_COOKIES_INVALID],
                         VALUE_COUNTER(results[2]), NULL, NULL);
    metric_family_append(&fams[FAM_SYNPROXY_COOKIES_VALID],
                         VALUE_COUNTER(results[3]), NULL, NULL);
    metric_family_append(&fams[FAM_SYNPROXY_COOKIES_RETRANSMISSION],
                         VALUE_COUNTER(results[4]), NULL, NULL);
    metric_family_append(&fams[FAM_SYNPROXY_CONNECTIONS_REOPENED],
                         VALUE_COUNTER(results[5]), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_SYNPROXY_MAX, 0);

    return 0;
}

static int synproxy_init(void)
{
    path_proc_synproxy = plugin_procpath("net/stat/synproxy");
    if (path_proc_synproxy == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int synproxy_shutdown(void)
{
    free(path_proc_synproxy);
    return 0;
}

void module_register(void)
{
    plugin_register_init("synproxy", synproxy_init);
    plugin_register_read("synproxy", synproxy_read);
    plugin_register_shutdown("synproxy", synproxy_shutdown);
}
