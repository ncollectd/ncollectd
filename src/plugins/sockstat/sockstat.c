// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

enum {
    FAM_SOCKETS_USED,
    FAM_SOCKETS_TCP_INUSE,
    FAM_SOCKETS_TCP_ORPHAN,
    FAM_SOCKETS_TCP_TIME_WAIT,
    FAM_SOCKETS_TCP_ALLOC,
    FAM_SOCKETS_TCP_MEM,
    FAM_SOCKETS_UDP_INUSE,
    FAM_SOCKETS_UDP_MEM,
    FAM_SOCKETS_UDPLITE_INUSE,
    FAM_SOCKETS_RAW_INUSE,
    FAM_SOCKETS_FRAG_INUSE,
    FAM_SOCKETS_FRAG_MEMORY,
    FAM_SOCKETS_TCP6_INUSE,
    FAM_SOCKETS_UDP6_INUSE,
    FAM_SOCKETS_UDPLITE6_INUSE,
    FAM_SOCKETS_RAW6_INUSE,
    FAM_SOCKETS_FRAG6_INUSE,
    FAM_SOCKETS_FRAG6_MEMORY,
    FAM_SOCKETS_MAX
};

static metric_family_t fams_sockstat[FAM_SOCKETS_MAX] = {
    [FAM_SOCKETS_USED] = {
        .name = "system_sockets_used",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_TCP_INUSE] = {
        .name = "system_sockets_tcp_inuse",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_TCP_ORPHAN] = {
        .name = "system_sockets_tcp_orphan",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_TCP_TIME_WAIT] = {
        .name = "system_sockets_tcp_time_wait",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_TCP_ALLOC] = {
        .name = "system_sockets_tcp_alloc",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_TCP_MEM] = {
        .name = "system_sockets_tcp_mem",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_UDP_INUSE] = {
        .name = "system_sockets_udp_inuse",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_UDP_MEM] = {
        .name = "system_sockets_udp_mem",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_UDPLITE_INUSE] = {
        .name = "system_sockets_udplite_inuse",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_RAW_INUSE] = {
        .name = "system_sockets_raw_inuse",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_FRAG_INUSE] = {
        .name = "system_sockets_frag_inuse",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_FRAG_MEMORY] = {
        .name = "system_sockets_frag_memory",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_TCP6_INUSE] = {
        .name = "system_sockets_tcp6_inuse",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_UDP6_INUSE] = {
        .name = "system_sockets_udp6_inuse",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_UDPLITE6_INUSE] = {
        .name = "system_sockets_udplite6_inuse",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_RAW6_INUSE] = {
        .name = "system_sockets_raw6_inuse",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_FRAG6_INUSE] = {
        .name = "system_sockets_frag6_inuse",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SOCKETS_FRAG6_MEMORY] = {
        .name = "system_sockets_frag6_memory",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
};

static char *path_proc_sockstat;
static bool path_proc_sockstat_found = false;
static char *path_proc_sockstat6;
static bool path_proc_sockstat6_found = false;

static int sockstat4_read(void)
{
    FILE *fh = fopen(path_proc_sockstat, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Unable to open '%s': %s", path_proc_sockstat, STRERRNO);
        return -1;
    }

    char buffer[256];
    char *fields[16];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

        if (fields_num < 3)
            continue;

        switch (fields[0][0]) {
        case 's':
            if ((strcmp(fields[0], "sockets:") == 0) && (fields_num == 3)) {
                if (strcmp(fields[1], "used") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[2], &value) == 0)
                        metric_family_append(&fams_sockstat[FAM_SOCKETS_USED],
                                             VALUE_GAUGE(value), NULL, NULL);
                }
            }
            break;
        case 'T':
            if ((strcmp(fields[0], "TCP:") == 0) && (fields_num == 11)) {
                if (strcmp(fields[1], "inuse") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[2], &value) == 0)
                         metric_family_append(&fams_sockstat[FAM_SOCKETS_TCP_INUSE],
                                              VALUE_GAUGE(value), NULL, NULL);
                }
                if (strcmp(fields[3], "orphan") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[4], &value) == 0)
                         metric_family_append(&fams_sockstat[FAM_SOCKETS_TCP_ORPHAN],
                                              VALUE_GAUGE(value), NULL, NULL);
                }
                if (strcmp(fields[5], "tw") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[6], &value) == 0)
                         metric_family_append(&fams_sockstat[FAM_SOCKETS_TCP_TIME_WAIT],
                                              VALUE_GAUGE(value), NULL, NULL);
                }
                if (strcmp(fields[7], "alloc") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[8], &value) == 0)
                         metric_family_append(&fams_sockstat[FAM_SOCKETS_TCP_ALLOC],
                                              VALUE_GAUGE(value), NULL, NULL);
                }
                if (strcmp(fields[9], "mem") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[10], &value) == 0)
                         metric_family_append(&fams_sockstat[FAM_SOCKETS_TCP_MEM],
                                              VALUE_GAUGE(value), NULL, NULL);
                }
            }
            break;
        case 'U':
            if ((strcmp(fields[0], "UDP:") == 0) && (fields_num == 5)) {
                if (strcmp(fields[1], "inuse") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[2], &value) == 0)
                         metric_family_append(&fams_sockstat[FAM_SOCKETS_UDP_INUSE],
                                              VALUE_GAUGE(value), NULL, NULL);
                }
                if (strcmp(fields[3], "mem") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[3], &value) == 0)
                         metric_family_append(&fams_sockstat[FAM_SOCKETS_UDP_MEM],
                                              VALUE_GAUGE(value), NULL, NULL);
                }
            } else if ((strcmp(fields[0], "UDPLITE:") == 0) && (fields_num == 3)) {
                if (strcmp(fields[1], "inuse") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[2], &value) == 0)
                         metric_family_append(&fams_sockstat[FAM_SOCKETS_UDPLITE_INUSE],
                                              VALUE_GAUGE(value), NULL, NULL);
                }
            }
            break;
        case 'R':
            if ((strcmp(fields[0], "RAW:") == 0) && (fields_num == 3)) {
                if (strcmp(fields[1], "inuse") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[2], &value) == 0)
                         metric_family_append(&fams_sockstat[FAM_SOCKETS_RAW_INUSE],
                                              VALUE_GAUGE(value), NULL, NULL);
                }
            }
            break;
        case 'F':
            if ((strcmp(fields[0], "FRAG:") == 0) && (fields_num == 5)) {
                if (strcmp(fields[1], "inuse") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[2], &value) == 0)
                         metric_family_append(&fams_sockstat[FAM_SOCKETS_FRAG_INUSE],
                                              VALUE_GAUGE(value), NULL, NULL);
                }
                if (strcmp(fields[3], "memory") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[4], &value) == 0)
                         metric_family_append(&fams_sockstat[FAM_SOCKETS_FRAG_MEMORY],
                                              VALUE_GAUGE(value), NULL, NULL);
                }
            }
            break;
        }

    }

    fclose(fh);

    return 0;
}

static int sockstat6_read(void)
{
    FILE *fh = fopen(path_proc_sockstat6, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Unable to open '%s': %s", path_proc_sockstat6, STRERRNO);
        return -1;
    }

    char buffer[256];
    char *fields[16];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

        if (fields_num < 3)
            continue;

        switch (fields[0][0]) {
        case 'T':
            if ((strcmp(fields[0], "TCP6:") == 0) && (fields_num == 3)) {
                if (strcmp(fields[1], "inuse") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[2], &value) == 0)
                        metric_family_append(&fams_sockstat[FAM_SOCKETS_TCP6_INUSE],
                                             VALUE_GAUGE(value), NULL, NULL);
                }
            }
            break;
        case 'U':
            if ((strcmp(fields[0], "UDP6:") == 0) && (fields_num == 3)) {
                if (strcmp(fields[1], "inuse") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[2], &value) == 0)
                        metric_family_append(&fams_sockstat[FAM_SOCKETS_UDP6_INUSE],
                                             VALUE_GAUGE(value), NULL, NULL);
                }
            } else if ((strcmp(fields[0], "UDPLITE:") == 0) && (fields_num == 3)) {
                if (strcmp(fields[1], "inuse") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[2], &value) == 0)
                        metric_family_append(&fams_sockstat[FAM_SOCKETS_UDPLITE6_INUSE],
                                             VALUE_GAUGE(value), NULL, NULL);
                }
            }
            break;
        case 'R':
            if ((strcmp(fields[0], "RAW6:") == 0) && (fields_num == 3)) {
                if (strcmp(fields[1], "inuse") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[2], &value) == 0)
                        metric_family_append(&fams_sockstat[FAM_SOCKETS_RAW6_INUSE],
                                             VALUE_GAUGE(value), NULL, NULL);
                }
            }
            break;
        case 'F':
            if ((strcmp(fields[0], "FRAG6:") == 0) && (fields_num == 5)) {
                if (strcmp(fields[1], "inuse") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[2], &value) == 0)
                        metric_family_append(&fams_sockstat[FAM_SOCKETS_FRAG6_INUSE],
                                             VALUE_GAUGE(value), NULL, NULL);
                }
                if (strcmp(fields[3], "memory") == 0) {
                    uint64_t value = 0;
                    if (strtouint(fields[4], &value) == 0)
                        metric_family_append(&fams_sockstat[FAM_SOCKETS_FRAG6_MEMORY],
                                             VALUE_GAUGE(value), NULL, NULL);
                }
            }
            break;
        }
    }

    fclose(fh);

    return 0;
}

static int sockstat_read(void)
{
    if (path_proc_sockstat_found)
        sockstat4_read();

    if (path_proc_sockstat6_found)
        sockstat6_read();

    plugin_dispatch_metric_family_array(fams_sockstat, FAM_SOCKETS_MAX, 0);
    return 0;
}

static int sockstat_init(void)
{
    path_proc_sockstat = plugin_procpath("net/sockstat");
    if (path_proc_sockstat == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    int status = access(path_proc_sockstat, R_OK);
    if (status == 0)
        path_proc_sockstat_found = true;

    path_proc_sockstat6 = plugin_procpath("net/sockstat6");
    if (path_proc_sockstat6 == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    status = access(path_proc_sockstat6, R_OK);
    if (status == 0)
        path_proc_sockstat6_found = true;

    return 0;
}

static int sockstat_shutdown(void)
{
    free(path_proc_sockstat);
    free(path_proc_sockstat6);
    return 0;
}

void module_register(void)
{
    plugin_register_init("sockstat", sockstat_init);
    plugin_register_read("sockstat", sockstat_read);
    plugin_register_shutdown("sockstat", sockstat_shutdown);
}
