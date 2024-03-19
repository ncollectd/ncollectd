// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2014 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008 Oleg King
// SPDX-FileCopyrightText: Copyright (C) 2009 Simon Kuhnle
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileCopyrightText: Copyright (C) 2013-2014 Pierre-Yves Ritschard
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Oleg King <king2 at kaluga.ru>
// SPDX-FileContributor: Simon Kuhnle <simon at blarzwurst.de>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>
// SPDX-FileContributor: Pierre-Yves Ritschard <pyr at spootnik.org>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/itoa.h"

#include "cpu.h"

extern metric_family_t fams[];
extern bool cpu_report_topology;
extern bool cpu_report_guest;
extern bool cpu_subtract_guest;

static char *path_proc_stat;
static char *path_sys_system_cpu;
static char *path_sys_system_node;

typedef struct {
    char node[8];
    char socket[8];
    char core[8];
    char drawer[8];
    char book[8];
} cpu_topology_t;

static cpu_topology_t *cpu_topology;
static size_t cpu_topology_num;
static double user_hz = 100.0;

static int cpu_topology_alloc(size_t cpu_num)
{
    cpu_topology_t *tmp;

    size_t size = cpu_num + 1;
    assert(size > 0);

    if (cpu_topology_num >= size)
        return 0;

    tmp = realloc(cpu_topology, size * sizeof(*cpu_topology));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        return ENOMEM;
    }
    cpu_topology = tmp;

    memset(cpu_topology + cpu_topology_num, 0, (size - cpu_topology_num) * sizeof(*cpu_topology));
    cpu_topology_num = size;
    return 0;
}

static cpu_topology_t *get_cpu_topology(size_t cpu_num, bool alloc)
{
    if (cpu_num >= cpu_topology_num) {
        if (alloc) {
            cpu_topology_alloc(cpu_num);
            if (cpu_num >= cpu_topology_num)
                return NULL;
        } else {
            return NULL;
        }
    }
    return &cpu_topology[cpu_num];
}

static int cpu_topology_id (char const *cpu, char const *id, char *buffer, int size)
{
    char path[PATH_MAX];
    ssnprintf(path, sizeof(path), "%s/%s/topology/%s", path_sys_system_cpu, cpu, id);
    int len = read_text_file_contents(path, buffer, size);
    if (len <= 0) {
        buffer[0] = '\0';
        return -1;
    }

    if ((len > 0) && (buffer[len -1] == '\n'))
        buffer[len-1] = '\0';

    return 0;
}

static int cpu_topology_id_callback (__attribute__((unused)) int dirfd,
                                     __attribute__((unused)) char const *dir, char const *cpu,
                                     __attribute__((unused)) void *user_data)
{
    if (strncmp(cpu, "cpu", 3) != 0)
        return 0;
    if (!isdigit(cpu[3]))
        return 0;

    int cpu_num = atoi(cpu + 3);

    int status = cpu_topology_alloc(cpu_num);
    if (status != 0)
        return status;

    cpu_topology_t *t = get_cpu_topology(cpu_num, true);
    if (t == NULL)
        return 0;

    cpu_topology_id(cpu, "core_id", t->core, sizeof(t->core));
    cpu_topology_id(cpu, "physical_package_id", t->socket, sizeof(t->socket));
    cpu_topology_id(cpu, "book_id", t->book, sizeof(t->book));
    cpu_topology_id(cpu, "drawer_id", t->drawer, sizeof(t->drawer));
    return 0;
}

static void cpu_topology_set_node(int ncpu, char const *node)
{
    cpu_topology_t *t = get_cpu_topology(ncpu, false);
    if (t == NULL)
        return;
     sstrncpy(t->node, node, sizeof(t->node));
}

static inline int hex_to_int(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    c = tolower(c);
    if (c >= 'a' && c <= 'f')
        return (c - 'a') + 10;
    return -1;
}

static int cpu_topology_node_callback (__attribute__((unused)) int dirfd,
                                       __attribute__((unused)) char const *dir, char const *node,
                                       __attribute__((unused)) void *user_data)
{
    if (strncmp(node, "node", 4) != 0)
        return 0;
    if (!isdigit(node[4]))
        return 0;

    char path[PATH_MAX];
    ssnprintf(path, sizeof(path), "%s/%s/cpumap", path_sys_system_node, node);

    char buffer[8096];
    ssize_t len = read_text_file_contents(path, buffer, sizeof(buffer));
    if (len <= 0)
        return -1;

    if ((len > 0) && (buffer[len -1] == '\n')) {
        buffer[len -1] = '\0';
        len--;
    }

    int ncpu = 0;
    for (char *c = buffer + len - 1;  c >= buffer; c--) {
        if (*c == 'x')
            break;
        if (*c == ',')
            continue;
//            c--;

        int set = hex_to_int(*c);
        if (set <= 0) {
            ncpu += 4;
            continue;
        }

        if (set & 1)
            cpu_topology_set_node(ncpu, node + 4);
        if (set & 2)
            cpu_topology_set_node(ncpu+1, node + 4);
        if (set & 4)
            cpu_topology_set_node(ncpu+2, node + 4);
        if (set & 8)
            cpu_topology_set_node(ncpu+3, node + 4);
        ncpu += 4;
    }

    return 0;
}

static int cpu_topology_scan(void)
{
    if (cpu_topology_num > 0) {
        free(cpu_topology);
        cpu_topology = NULL;
        cpu_topology_num = 0;
    }

    walk_directory(path_sys_system_cpu, cpu_topology_id_callback, NULL, 0);

    walk_directory(path_sys_system_node, cpu_topology_node_callback, NULL, 0);

    return 0;
}

void cpu_state_append(int cpu, char *state, uint64_t value)
{
    char buffer_cpu[ITOA_MAX];

    metric_family_t *fam = cpu < 0 ?  &fams[FAM_CPU_ALL_USAGE] : &fams[FAM_CPU_USAGE];

    label_pair_t pairs[7];
    size_t n=0;

    if (cpu >= 0) {
        cpu_topology_t *t = NULL;
        if (cpu_report_topology)
            t = get_cpu_topology(cpu, false);

        if (cpu_report_topology && (t != NULL)) {
            if (t->book[0] != '\0')
                pairs[n++] = (label_pair_t){ .name = "book", .value = t->book};
            if (t->core[0] != '\0')
                pairs[n++] = (label_pair_t){ .name = "core", .value = t->core};
        }

        itoa(cpu, buffer_cpu);
        pairs[n++] = (label_pair_t){ .name = "cpu", .value = buffer_cpu};

        if (cpu_report_topology && (t != NULL)) {
            if (t->drawer[0] != '\0')
                pairs[n++] = (label_pair_t){ .name = "drawer", .value = t->drawer};
            if (t->node[0] != '\0')
                pairs[n++] = (label_pair_t){ .name = "node", .value = t->node};
            if (t->socket[0] != '\0')
                pairs[n++] = (label_pair_t){ .name = "socket", .value = t->socket};
        }
    }

    pairs[n++] = (label_pair_t){ .name = "state", .value = state};

    label_set_t labels = {.num = n, .ptr = pairs};

    metric_family_append(fam, VALUE_COUNTER_FLOAT64((double)value/user_hz), &labels, NULL);
}

int cpu_read(void)
{
    cdtime_t now = cdtime();

    FILE * fh = fopen(path_proc_stat, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("fopen '%s' failed: %s", path_proc_stat, STRERRNO);
        return -1;
    }

    size_t cpu_num = 0;

    char buf[1024];
    while (fgets(buf, 1024, fh) != NULL) {
        if (strncmp(buf, "cpu", 3))
            continue;

        int cpu = 0;

        if (buf[3] == ' ') {
            cpu = -1;
        } else if ((buf[3] < '0') || (buf[3] > '9')) {
            continue;
        }

        char *fields[11];
        int numfields = strsplit(buf, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields < 5)
            continue;

        if (cpu == 0) {
            cpu = atoi(fields[0] + 3);
            cpu_num++;
        }

        /* Do not stage User and Nice immediately: we may need to alter them later: */
        long long user_value = atoll(fields[1]);
        long long nice_value = atoll(fields[2]);
        cpu_state_append(cpu, "system", (uint64_t)atoll(fields[3]));
        cpu_state_append(cpu, "idle", (uint64_t)atoll(fields[4]));

        if (numfields >= 8) {
            cpu_state_append(cpu, "wait", (uint64_t)atoll(fields[5]));
            cpu_state_append(cpu, "interrupt", (uint64_t)atoll(fields[6]));
            cpu_state_append(cpu, "softirq", (uint64_t)atoll(fields[7]));
        }

        if (numfields >= 9) { /* Steal (since Linux 2.6.11) */
            cpu_state_append(cpu, "steal", (uint64_t)atoll(fields[8]));
        }

        if (numfields >= 10) { /* Guest (since Linux 2.6.24) */
            if (cpu_report_guest) {
                long long value = atoll(fields[9]);
                cpu_state_append(cpu, "guest" , (uint64_t)value);
                /* Guest is included in User; optionally subtract Guest from User: */
                if (cpu_subtract_guest) {
                    user_value -= value;
                    if (user_value < 0)
                        user_value = 0;
                }
            }
        }

        if (numfields >= 11) { /* Guest_nice (since Linux 2.6.33) */
            if (cpu_report_guest) {
                long long value = atoll(fields[10]);
                cpu_state_append(cpu, "guest_nice" , (uint64_t)value);
                /* Guest_nice is included in Nice; optionally subtract Guest_nice from Nice: */
                if (cpu_subtract_guest) {
                    nice_value -= value;
                    if (nice_value < 0)
                        nice_value = 0;
                }
            }
        }

        /* Eventually stage User and Nice: */
        cpu_state_append(cpu, "user", (uint64_t)user_value);
        cpu_state_append(cpu, "nice", (uint64_t)nice_value);
    }

    fclose(fh);

    if (cpu_num > 0)
        metric_family_append(&fams[FAM_CPU_COUNT], VALUE_GAUGE(cpu_num), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_CPU_MAX, now);

    if (cpu_report_topology && (cpu_num > 0) && (cpu_num != cpu_topology_num))
        cpu_topology_scan();

    return 0;
}

int cpu_init(void)
{
    path_proc_stat = plugin_procpath("stat");
    if (path_proc_stat == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    path_sys_system_cpu = plugin_syspath("devices/system/cpu");
    if (path_sys_system_cpu == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    path_sys_system_node = plugin_syspath("devices/system/node");
    if (path_sys_system_node == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    long ticks = sysconf(_SC_CLK_TCK);
    if (ticks <= 0) {
        PLUGIN_ERROR("sysconf(_SC_CLK_TCK) failed: %s.", STRERRNO);
        return -1;
    }

    user_hz = ticks;

    if (cpu_report_topology)
        cpu_topology_scan();

    return 0;
}

int cpu_shutdown(void)
{
    free(path_proc_stat);
    free(path_sys_system_cpu);
    free(path_sys_system_node);
    free(cpu_topology);
    return 0;
}
