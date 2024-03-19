// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright(c) 2016-2019 Intel Corporation.
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Serhiy Pshyk <serhiyx.pshyk at intel.com>
// SPDX-FileContributor: Starzyk, Mateusz <mateuszx.starzyk at intel.com>
// SPDX-FileContributor: Wojciech Andralojc <wojciechx.andralojc at intel.com>
// SPDX-FileContributor: Michał Aleksiński <michalx.aleksinski at intel.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "plugins/intel_rdt/config_cores.h"
#include "plugins/intel_rdt/proc_pids.h"

#include <pqos.h>

/* libpqos v2.0 or newer is required for process monitoring*/
#undef LIBPQOS2
#if defined(PQOS_VERSION) && PQOS_VERSION >= 20000
#define LIBPQOS2
#endif

#define RDT_MAX_SOCKETS 8
#define RDT_MAX_SOCKET_CORES 64
#define RDT_MAX_CORES (RDT_MAX_SOCKET_CORES * RDT_MAX_SOCKETS)

#ifdef LIBPQOS2
/*
 * Process name inside comm file is limited to 16 chars.
 * More info here: http://man7.org/linux/man-pages/man5/proc.5.html
 */
#define RDT_MAX_NAMES_GROUPS 64
#define RDT_PROC_PATH "/proc"
#endif

enum {
    FAM_INTEL_RDT_LOCAL_MEMORY_BANDWIDTH,
    FAM_INTEL_RDT_REMOTE_MEMORY_BANDWIDTH,
    FAM_INTEL_RDT_TOTAL_MEMORY_BANDWIDTH,
    FAM_INTEL_RDT_L3_CACHE_OCCUPANCY_BYTES,
    FAM_INTEL_RDT_INSTRUCTIONS_PER_CYCLE,
    FAM_INTEL_RDT_LLC_REFERENCES,
    FAM_INTEL_RDT_MAX,
};

static metric_family_t fams[FAM_INTEL_RDT_MAX] = {
    [FAM_INTEL_RDT_LOCAL_MEMORY_BANDWIDTH] = {
        .name = "system_intel_rdt_local_memory_bandwidth",
        .type = METRIC_TYPE_COUNTER,
        .help = "Memory bandwidth utilization by the relevant CPU core "
                "on the local NUMA memory channel",
    },
    [FAM_INTEL_RDT_REMOTE_MEMORY_BANDWIDTH] = {
        .name = "system_intel_rdt_remote_memory_bandwidth",
        .type = METRIC_TYPE_COUNTER,
        .help = "Memory bandwidth utilization by the relevant CPU core "
                "on the remote NUMA memory channel",
    },
    [FAM_INTEL_RDT_TOTAL_MEMORY_BANDWIDTH] = {
        .name = "system_intel_rdt_total_memory_bandwidth",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total memory bandwidth utilized by a CPU core "
                "on local and remote NUMA memory channels",
    },
    [FAM_INTEL_RDT_L3_CACHE_OCCUPANCY_BYTES] = {
        .name = "system_intel_rdt_l3_cache_occupancy_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Last Level Cache occupancy by a process",
    },
    [FAM_INTEL_RDT_INSTRUCTIONS_PER_CYCLE] = {
        .name = "system_intel_rdt_instructions_per_cycle",
        .type = METRIC_TYPE_GAUGE,
        .help = "Instructions per cycle executed by a process",
    },
    [FAM_INTEL_RDT_LLC_REFERENCES] = {
        .name = "system_intel_rdt_llc_references",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total Last Level Cache references.",
    },
};

typedef enum {
    UNKNOWN = 0,
    CONFIGURATION_ERROR,
} rdt_config_status;

#ifdef LIBPQOS2
struct rdt_name_group_s {
    char *desc;
    size_t num_names;
    char **names;
    proc_pids_t **proc_pids;
    size_t monitored_pids_count;
    enum pqos_mon_event events;
};
typedef struct rdt_name_group_s rdt_name_group_t;
#endif

struct rdt_ctx_s {
    bool mon_ipc_enabled;
#if PQOS_VERSION >= 40400
    bool mon_llc_ref_enabled;
#endif
    core_groups_list_t cores;
    enum pqos_mon_event events[RDT_MAX_CORES];
    struct pqos_mon_data *pcgroups[RDT_MAX_CORES];
#ifdef LIBPQOS2
    rdt_name_group_t ngroups[RDT_MAX_NAMES_GROUPS];
    struct pqos_mon_data *pngroups[RDT_MAX_NAMES_GROUPS];
    size_t num_ngroups;
    proc_pids_t **proc_pids;
    size_t num_proc_pids;
#endif
    const struct pqos_cpuinfo *pqos_cpu;
    const struct pqos_cap *pqos_cap;
    const struct pqos_capability *cap_mon;
};
typedef struct rdt_ctx_s rdt_ctx_t;

static rdt_ctx_t *g_rdt;

static rdt_config_status g_state = UNKNOWN;

static int g_interface = -1;

static void rdt_submit(const struct pqos_mon_data *group)
{
    const struct pqos_event_values *values = &group->values;
    const char *desc = group->context;
    const enum pqos_mon_event events = group->event;

    if (events & PQOS_MON_EVENT_L3_OCCUP)
        metric_family_append(&fams[FAM_INTEL_RDT_L3_CACHE_OCCUPANCY_BYTES],
                             VALUE_GAUGE(values->llc), NULL,
                             &(label_pair_const_t){.name="context", .value=desc}, NULL);

    if (events & PQOS_PERF_EVENT_IPC)
        metric_family_append(&fams[FAM_INTEL_RDT_INSTRUCTIONS_PER_CYCLE],
                             VALUE_GAUGE(values->ipc), NULL,
                             &(label_pair_const_t){.name="context", .value=desc}, NULL);

#if PQOS_VERSION >= 40400
    if (events & PQOS_PERF_EVENT_LLC_REF) {
        uint64_t value = 0;
        int ret = pqos_mon_get_value(group, PQOS_PERF_EVENT_LLC_REF, &value, NULL);
        if (ret == PQOS_RETVAL_OK)
            metric_family_append(&fams[FAM_INTEL_RDT_LLC_REFERENCES],
                                 VALUE_COUNTER(value), NULL,
                                 &(label_pair_const_t){.name="context", .value=desc}, NULL);
        }
#endif

    if (events & PQOS_MON_EVENT_LMEM_BW) {
        const struct pqos_monitor *mon = NULL;

        int retval = pqos_cap_get_event(g_rdt->pqos_cap, PQOS_MON_EVENT_LMEM_BW, &mon);
        if (retval == PQOS_RETVAL_OK) {
            uint64_t value = values->mbm_local;

            if (mon->scale_factor != 0)
                value = value * mon->scale_factor;

            metric_family_append(&fams[FAM_INTEL_RDT_LOCAL_MEMORY_BANDWIDTH],
                                 VALUE_COUNTER(value), NULL,
                                 &(label_pair_const_t){.name="context", .value=desc}, NULL);
        }
    }

    if (events & PQOS_MON_EVENT_TMEM_BW) {
        const struct pqos_monitor *mon = NULL;

        int retval = pqos_cap_get_event(g_rdt->pqos_cap, PQOS_MON_EVENT_TMEM_BW, &mon);
        if (retval == PQOS_RETVAL_OK) {
            uint64_t value = values->mbm_total;

            if (mon->scale_factor != 0)
                value = value * mon->scale_factor;

            metric_family_append(&fams[FAM_INTEL_RDT_TOTAL_MEMORY_BANDWIDTH],
                                 VALUE_COUNTER(value), NULL,
                                 &(label_pair_const_t){.name="context", .value=desc}, NULL);
        }
    }

    if (events & PQOS_MON_EVENT_RMEM_BW) {
        const struct pqos_monitor *mon = NULL;

        int retval = pqos_cap_get_event(g_rdt->pqos_cap, PQOS_MON_EVENT_RMEM_BW, &mon);
        if (retval == PQOS_RETVAL_OK) {
            uint64_t value = values->mbm_remote;

#if PQOS_VERSION < 40000
            if (events & (PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_LMEM_BW))
                value = values->mbm_total - values->mbm_local;
#endif

            if (mon->scale_factor != 0)
                value = value * mon->scale_factor;

            metric_family_append(&fams[FAM_INTEL_RDT_REMOTE_MEMORY_BANDWIDTH],
                                 VALUE_COUNTER(value), NULL,
                                 &(label_pair_const_t){.name="context", .value=desc}, NULL);
        }
    }

    plugin_dispatch_metric_family_array(fams, FAM_INTEL_RDT_MAX, 0);
}

#ifdef NCOLLECTD_DEBUG
static void rdt_dump_cgroups(void)
{
    char cores[RDT_MAX_CORES * 4];

    if (g_rdt == NULL)
        return;
    if (g_rdt->cores.num_cgroups == 0)
        return;

    PLUGIN_DEBUG("Core Groups Dump");
    PLUGIN_DEBUG(" groups count: %" PRIsz, g_rdt->cores.num_cgroups);

    for (size_t i = 0; i < g_rdt->cores.num_cgroups; i++) {
        core_group_t *cgroup = g_rdt->cores.cgroups + i;

        memset(cores, 0, sizeof(cores));
        for (size_t j = 0; j < cgroup->num_cores; j++) {
            ssnprintf(cores + strlen(cores), sizeof(cores) - strlen(cores) - 1, " %u",
                                cgroup->cores[j]);
        }

        PLUGIN_DEBUG(" group[%zu]:", i);
        PLUGIN_DEBUG("     description: %s", cgroup->desc);
        PLUGIN_DEBUG("     cores: %s", cores);
        PLUGIN_DEBUG("     events: 0x%X", g_rdt->events[i]);
    }

    return;
}

#ifdef LIBPQOS2
static void rdt_dump_ngroups(void)
{
    char names[DATA_MAX_NAME_LEN];

    if (g_rdt == NULL)
        return;
    if (g_rdt->num_ngroups == 0)
        return;

    PLUGIN_DEBUG("Process Names Groups Dump");
    PLUGIN_DEBUG(" groups count: %" PRIsz, g_rdt->num_ngroups);

    for (size_t i = 0; i < g_rdt->num_ngroups; i++) {
        memset(names, 0, sizeof(names));
        for (size_t j = 0; j < g_rdt->ngroups[i].num_names; j++)
            ssnprintf(names + strlen(names), sizeof(names) - strlen(names) - 1, " %s",
                                g_rdt->ngroups[i].names[j]);

        PLUGIN_DEBUG(" group[%d]:", (int)i);
        PLUGIN_DEBUG("     description: %s", g_rdt->ngroups[i].desc);
        PLUGIN_DEBUG("     process names:%s", names);
        PLUGIN_DEBUG("     events: 0x%X", g_rdt->ngroups[i].events);
    }

    return;
}
#endif

static inline double bytes_to_kb(const double bytes)
{
    return bytes / 1024.0;
}

static inline double bytes_to_mb(const double bytes)
{
    return bytes / (1024.0 * 1024.0);
}

static void rdt_dump_cores_data(void)
{
    /*
     * CORE - monitored group of cores
     * RMID - Resource Monitoring ID associated with the monitored group
     *              This is not available for monitoring with resource control
     * LLC - last level cache occupancy
     * MBL - local memory bandwidth
     * MBR - remote memory bandwidth
     */
    PLUGIN_DEBUG(" CORE           LLC[KB]     MBL[MB]      MBR[MB]");

    for (size_t i = 0; i < g_rdt->cores.num_cgroups; i++) {
        const struct pqos_event_values *pv = &g_rdt->pcgroups[i]->values;

        double llc = bytes_to_kb(pv->llc);
        double mbr = bytes_to_mb(pv->mbm_remote_delta);
        double mbl = bytes_to_mb(pv->mbm_local_delta);

        PLUGIN_DEBUG("[%s] %10.1f %10.1f %10.1f", g_rdt->cores.cgroups[i].desc, llc, mbl, mbr);
    }
}

#ifdef LIBPQOS2
static void rdt_dump_pids_data(void)
{
    /*
     * NAME - monitored group of processes
     * PIDs - list of PID numbers in the NAME group
     * LLC - last level cache occupancy
     * MBL - local memory bandwidth
     * MBR - remote memory bandwidth
     */

    PLUGIN_DEBUG(" NAME           PIDs");
    char pids[DATA_MAX_NAME_LEN];
    for (size_t i = 0; i < g_rdt->num_ngroups; ++i) {
        memset(pids, 0, sizeof(pids));
        for (size_t j = 0; j < g_rdt->ngroups[i].num_names; ++j) {
            pids_list_t *list = g_rdt->ngroups[i].proc_pids[j]->curr;
            for (size_t k = 0; k < list->size; k++)
                ssnprintf(pids + strlen(pids), sizeof(pids) - strlen(pids) - 1, " %d",
                                    list->pids[k]);
        }
        PLUGIN_DEBUG(" [%s] %s", g_rdt->ngroups[i].desc, pids);
    }

    PLUGIN_DEBUG(" NAME        LLC[KB]     MBL[MB]        MBR[MB]");
    for (size_t i = 0; i < g_rdt->num_ngroups; i++) {

        const struct pqos_event_values *pv = &g_rdt->pngroups[i]->values;

        double llc = bytes_to_kb(pv->llc);
        double mbr = bytes_to_mb(pv->mbm_remote_delta);
        double mbl = bytes_to_mb(pv->mbm_local_delta);

        PLUGIN_DEBUG(" [%s] %10.1f %10.1f %10.1f", g_rdt->ngroups[i].desc,
                    llc, mbl, mbr);
    }
}
#endif
#endif

#ifdef LIBPQOS2
static int isdupstr(char *names[], const size_t size, const char *name)
{
    for (size_t i = 0; i < size; i++)
        if (strncmp(names[i], name, (size_t)MAX_PROC_NAME_LEN) == 0)
            return 1;

    return 0;
}

/*
 * NAME
 *   strlisttoarray
 *
 * DESCRIPTION
 *   Converts string representing list of strings into array of strings.
 *   Allowed format is:
 *       name,name1,name2,name3
 *
 * PARAMETERS
 *   `str_list'  String representing list of strings.
 *   `names'         Array to put extracted strings into.
 *   `names_num' Variable to put number of extracted strings.
 *
 * RETURN VALUE
 *      Number of elements placed into names.
 */
static int strlisttoarray(char *str_list, char ***names, size_t *names_num)
{
    char *saveptr = NULL;

    if (str_list == NULL || names == NULL)
        return -EINVAL;

    if (strstr(str_list, ",,")) {
        /* strtok ignores empty words between separators.
         * This condition handles that by rejecting strings
         * with consecutive seprators */
        PLUGIN_ERROR("Empty process name");
        return -EINVAL;
    }

    for (;;) {
        char *token = strtok_r(str_list, ",", &saveptr);
        if (token == NULL)
            break;

        str_list = NULL;

        while (isspace(*token))
            token++;

        if (*token == '\0')
            continue;

        if ((isdupstr(*names, *names_num, token))) {
            PLUGIN_ERROR("Duplicated process name \'%s\'", token);
            return -EINVAL;
        } else {
            if (strarray_add(names, names_num, token) != 0) {
                PLUGIN_ERROR("Error allocating process name string");
                return -ENOMEM;
            }
        }
    }

    return 0;
}

/*
 * NAME
 *   ngroup_cmp
 *
 * DESCRIPTION
 *   Function to compare names in two name groups.
 *
 * PARAMETERS
 *   `ng_a'          Pointer to name group a.
 *   `ng_b'          Pointer to name group b.
 *
 * RETURN VALUE
 *      1 if both groups contain the same names
 *      0 if none of their names match
 *      -1 if some but not all names match
 */
static int ngroup_cmp(const rdt_name_group_t *ng_a, const rdt_name_group_t *ng_b)
{
    unsigned found = 0;

    assert(ng_a != NULL);
    assert(ng_b != NULL);

    const size_t sz_a = (unsigned)ng_a->num_names;
    const size_t sz_b = (unsigned)ng_b->num_names;
    char **tab_a = ng_a->names;
    char **tab_b = ng_b->names;

    for (size_t i = 0; i < sz_a; i++) {
        for (size_t j = 0; j < sz_b; j++) {
            if (strncmp(tab_a[i], tab_b[j], (size_t)MAX_PROC_NAME_LEN) == 0)
                found++;
        }
    }
    /* if no names are the same */
    if (!found)
        return 0;
    /* if group contains same names */
    if (sz_a == sz_b && sz_b == (size_t)found)
        return 1;
    /* if not all names are the same */
    return -1;
}

/*
 * NAME
 *   config_to_ngroups
 *
 * DESCRIPTION
 *   Function to set the descriptions and names for each process names group.
 *   Takes a config option containing list of strings that are used to set
 *   process group values.
 *
 * PARAMETERS
 *   `item'              Config option containing process names groups.
 *   `groups'            Table of process name groups to set values in.
 *   `max_groups'  Maximum number of process name groups allowed.
 *
 * RETURN VALUE
 *   On success, the number of name groups set up. On error, appropriate
 *   negative error value.
 */
static int config_to_ngroups(const config_item_t *item,
                             rdt_name_group_t *groups, const size_t max_groups)
{
    int index = 0;

    assert(groups != NULL);
    assert(max_groups > 0);
    assert(item != NULL);

    for (int j = 0; j < item->values_num; j++) {
        char value[DATA_MAX_NAME_LEN];

        if ((item->values[j].value.string == NULL) || (strlen(item->values[j].value.string) == 0)) {
            PLUGIN_ERROR("Error - empty group");
            return -EINVAL;
        }

        sstrncpy(value, item->values[j].value.string, sizeof(value));

        int ret = strlisttoarray(value, &groups[index].names, &groups[index].num_names);
        if (ret != 0 || groups[index].num_names == 0) {
            PLUGIN_ERROR("Error parsing process names group (%s)",
                        item->values[j].value.string);
            return -EINVAL;
        }

        /* set group description info */
        groups[index].desc = sstrdup(item->values[j].value.string);
        if (groups[index].desc == NULL) {
            PLUGIN_ERROR("Error allocating name group description");
            return -ENOMEM;
        }

        groups[index].proc_pids = NULL;
        groups[index].monitored_pids_count = 0;

        index++;

        if (index >= (const int)max_groups) {
            PLUGIN_WARNING("Too many process names groups configured");
            return index;
        }
    }

    return index;
}

/*
 * NAME
 *   rdt_free_ngroups
 *
 * DESCRIPTION
 *   Function to deallocate memory allocated for name groups.
 *
 * PARAMETERS
 *   `rdt'           Pointer to rdt context
 */
static void rdt_free_ngroups(rdt_ctx_t *rdt)
{
    for (int i = 0; i < RDT_MAX_NAMES_GROUPS; i++) {
        if (rdt->ngroups[i].desc) {
            PLUGIN_DEBUG("Freeing pids \'%s\' group\'s data...", rdt->ngroups[i].desc);
        }
        free(rdt->ngroups[i].desc);
        strarray_free(rdt->ngroups[i].names, rdt->ngroups[i].num_names);

        if (rdt->ngroups[i].proc_pids)
            proc_pids_free(rdt->ngroups[i].proc_pids, rdt->ngroups[i].num_names);

        rdt->ngroups[i].num_names = 0;
#if PQOS_VERSION < 40600
        free(rdt->pngroups[i]);
#endif
    }
    if (rdt->proc_pids)
        free(rdt->proc_pids);

    rdt->num_ngroups = 0;
}
#endif

/*
 * NAME
 *   rdt_config_events
 *
 * DESCRIPTION
 *   Configure available monitoring events
 *
 * PARAMETERS
 *   `rdt`       Pointer to rdt context
 *
 * RETURN VALUE
 *  0 on success. Negative number on error.
 */
static int rdt_config_events(rdt_ctx_t *rdt)
{
    enum pqos_mon_event events = 0;

    /* Get all available events on this platform */
    for (unsigned i = 0; i < rdt->cap_mon->u.mon->num_events; i++)
        events |= rdt->cap_mon->u.mon->events[i].type;

#if PQOS_VERSION >= 40400
    events &= (PQOS_MON_EVENT_L3_OCCUP | PQOS_PERF_EVENT_IPC | PQOS_MON_EVENT_LMEM_BW |
               PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW | PQOS_PERF_EVENT_LLC_REF);
#else
    events &= (PQOS_MON_EVENT_L3_OCCUP | PQOS_PERF_EVENT_IPC | PQOS_MON_EVENT_LMEM_BW |
               PQOS_MON_EVENT_TMEM_BW | PQOS_MON_EVENT_RMEM_BW);
#endif

    /* IPC monitoring is disabled */
    if (!rdt->mon_ipc_enabled)
        events &= ~(PQOS_PERF_EVENT_IPC);

#if PQOS_VERSION >= 40400
  /* LLC references monitoring is disabled */
    if (!rdt->mon_llc_ref_enabled)
        events &= ~(PQOS_PERF_EVENT_LLC_REF);
#endif

    PLUGIN_DEBUG("Available events to monitor: %#x", events);

    for (size_t i = 0; i < rdt->cores.num_cgroups; i++) {
        rdt->events[i] = events;
    }
#ifdef LIBPQOS2
    for (size_t i = 0; i < rdt->num_ngroups; i++) {
        rdt->ngroups[i].events = events;
    }
#endif
    return 0;
}

#ifdef LIBPQOS2
/*
 * NAME
 *   rdt_config_ngroups
 *
 * DESCRIPTION
 *   Reads name groups configuration.
 *
 * PARAMETERS
 *   `rdt`           Pointer to rdt context
 *   `item'          Config option containing process names groups.
 *
 * RETURN VALUE
 *  0 on success. Negative number on error.
 */
static int rdt_config_ngroups(rdt_ctx_t *rdt, const config_item_t *item)
{
    if (item == NULL) {
        PLUGIN_DEBUG("Invalid argument.");
        return -EINVAL;
    }

    PLUGIN_DEBUG("Process names groups [%d]:", item->values_num);
    for (int j = 0; j < item->values_num; j++) {
        if (item->values[j].type != CONFIG_TYPE_STRING) {
            PLUGIN_ERROR("given process names group value is not a string [idx=%d]", j);
            return -EINVAL;
        }
        PLUGIN_DEBUG(" [%d]: %s", j, item->values[j].value.string);
    }

    int n = config_to_ngroups(item, rdt->ngroups, RDT_MAX_NAMES_GROUPS);
    if (n < 0) {
        rdt_free_ngroups(rdt);
        PLUGIN_ERROR("Error parsing process name groups configuration.");
        return -EINVAL;
    }

    /* validate configured process name values */
    for (int group_idx = 0; group_idx < n; group_idx++) {
        PLUGIN_DEBUG(" checking group [%d]: %s", group_idx, rdt->ngroups[group_idx].desc);
        for (size_t name_idx = 0; name_idx < rdt->ngroups[group_idx].num_names; name_idx++) {
            PLUGIN_DEBUG("     checking process name [%zu]: %s", name_idx,
                        rdt->ngroups[group_idx].names[name_idx]);
            if (!proc_pids_is_name_valid(rdt->ngroups[group_idx].names[name_idx])) {
                PLUGIN_ERROR("Process name group '%s' contains invalid name '%s'",
                            rdt->ngroups[group_idx].desc,
                            rdt->ngroups[group_idx].names[name_idx]);
                rdt_free_ngroups(rdt);
                return -EINVAL;
            }
        }
    }

    if (n == 0) {
        PLUGIN_ERROR("Empty process name groups configured.");
        return -EINVAL;
    }

    rdt->num_ngroups = n;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < i; j++) {
            int found = ngroup_cmp(&rdt->ngroups[j], &rdt->ngroups[i]);
            if (found != 0) {
                rdt_free_ngroups(rdt);
                PLUGIN_ERROR("Cannot monitor same process name in different groups.");
                return -EINVAL;
            }
        }
#if PQOS_VERSION < 40600
        rdt->pngroups[i] = calloc(1, sizeof(*rdt->pngroups[i]));
        if (rdt->pngroups[i] == NULL) {
            rdt_free_ngroups(rdt);
            PLUGIN_ERROR("Failed to allocate memory for process name monitoring data.");
            return -ENOMEM;
        }
#endif
    }

    return 0;
}

/*
 * NAME
 *   rdt_refresh_ngroup
 *
 * DESCRIPTION
 *   Refresh pids monitored by name group.
 *
 * PARAMETERS
 *   `ngroup`                   Pointer to name group.
 *   `group_mon_data' PQoS monitoring context.
 *
 * RETURN VALUE
 *  0 on success. Negative number on error.
 */
static int rdt_refresh_ngroup(rdt_name_group_t *ngroup, struct pqos_mon_data **group_mon_data)
{
    int result = 0;

    if (ngroup == NULL)
        return -1;

    if (ngroup->proc_pids == NULL) {
        PLUGIN_ERROR("\'%s\' uninitialized process pids array.", ngroup->desc);
        return -1;
    }

    PLUGIN_DEBUG("\'%s\' process names group.", ngroup->desc);

    proc_pids_t **proc_pids = ngroup->proc_pids;
    pids_list_t added_pids;
    pids_list_t removed_pids;

    memset(&added_pids, 0, sizeof(added_pids));
    memset(&removed_pids, 0, sizeof(removed_pids));

    for (size_t i = 0; i < ngroup->num_names; ++i) {
        int diff_result = pids_list_diff(proc_pids[i], &added_pids, &removed_pids);
        if (diff_result != 0) {
            PLUGIN_ERROR("\'%s\'. Error [%d] during PID diff.", ngroup->desc, diff_result);
            result = -1;
            goto cleanup;
        }
    }

    PLUGIN_DEBUG("\'%s\' process names group, added: %u, removed: %u.",
                 ngroup->desc, (unsigned)added_pids.size, (unsigned)removed_pids.size);

    if (added_pids.size > 0) {
        /* no pids are monitored for this group yet: start monitoring */
        if (ngroup->monitored_pids_count == 0) {
#if PQOS_VERSION >= 40600
            int start_result = pqos_mon_start_pids2(added_pids.size, added_pids.pids, ngroup->events,
                                                    (void *)ngroup->desc, group_mon_data);
#else
            int start_result = pqos_mon_start_pids(added_pids.size, added_pids.pids, ngroup->events,
                                                   (void *)ngroup->desc, *group_mon_data);
#endif
            if (start_result == PQOS_RETVAL_OK) {
                ngroup->monitored_pids_count = added_pids.size;
            } else {
                PLUGIN_ERROR("\'%s\'. Error [%d] while STARTING pids monitoring",
                             ngroup->desc, start_result);
                result = -1;
                goto pqos_error_recovery;
            }

        } else {
            int add_result = pqos_mon_add_pids(added_pids.size, added_pids.pids, *group_mon_data);
            if (add_result == PQOS_RETVAL_OK) {
                ngroup->monitored_pids_count += added_pids.size;
            } else {
                PLUGIN_ERROR("\'%s\'. Error [%d] while ADDING pids.", ngroup->desc, add_result);
                result = -1;
                goto pqos_error_recovery;
            }
        }
    }

    if (removed_pids.size > 0) {

        /* all pids are removed: stop monitoring */
        if (removed_pids.size == ngroup->monitored_pids_count) {
            /* all pids for this group are lost: stop monitoring */
            int stop_result = pqos_mon_stop(*group_mon_data);
            if (stop_result != PQOS_RETVAL_OK) {
                PLUGIN_ERROR("\'%s\'. Error [%d] while STOPPING monitoring",
                             ngroup->desc, stop_result);
                result = -1;
                goto pqos_error_recovery;
            }
            ngroup->monitored_pids_count = 0;
        } else {
            int remove_result = pqos_mon_remove_pids(removed_pids.size, removed_pids.pids,
                                                     *group_mon_data);
            if (remove_result == PQOS_RETVAL_OK) {
                ngroup->monitored_pids_count -= removed_pids.size;
            } else {
                PLUGIN_ERROR("\'%s\'. Error [%d] while REMOVING pids.",
                             ngroup->desc, remove_result);
                result = -1;
                goto pqos_error_recovery;
            }
        }
    }

    goto cleanup;

pqos_error_recovery:
    /* Why?
     * Resources might be temporary unavailable.
     *
     * How?
     * Collectd will halt the reading thread for this
     * plugin if it returns an error.
     * Consecutive errors will be increasing the read period
     * up to 1 day interval.
     * On pqos error stop monitoring current group
     * and reset the proc_pids array
     * monitoring will be restarted on next ncollectd read cycle
     */
    PLUGIN_DEBUG("\'%s\' group RESET after error.", ngroup->desc);
    pqos_mon_stop(*group_mon_data);
    for (size_t i = 0; i < ngroup->num_names; ++i) {
        if (ngroup->proc_pids[i]->curr)
            ngroup->proc_pids[i]->curr->size = 0;
    }

    ngroup->monitored_pids_count = 0;

cleanup:
    pids_list_clear(&added_pids);
    pids_list_clear(&removed_pids);

    return result;
}

/*
 * NAME
 *   read_pids_data
 *
 * DESCRIPTION
 *   Poll monitoring statistics for name groups
 *
 * RETURN VALUE
 *  0 on success. Negative number on error.
 */
static int read_pids_data(void)
{
    if (g_rdt->num_ngroups == 0) {
        PLUGIN_DEBUG("not configured - PIDs read skipped");
        return 0;
    }

    PLUGIN_DEBUG("Scanning active groups");
    struct pqos_mon_data *active_groups[RDT_MAX_NAMES_GROUPS] = {0};
    size_t active_group_idx = 0;
    for (size_t pngroups_idx = 0;
             pngroups_idx < STATIC_ARRAY_SIZE(g_rdt->pngroups); ++pngroups_idx)
        if (0 != g_rdt->ngroups[pngroups_idx].monitored_pids_count)
            active_groups[active_group_idx++] = g_rdt->pngroups[pngroups_idx];

    int ret = 0;

    if (active_group_idx == 0) {
        PLUGIN_DEBUG("no active groups - PIDs read skipped");
        goto groups_refresh;
    }

    PLUGIN_DEBUG("PIDs data polling");

    int poll_result = pqos_mon_poll(active_groups, active_group_idx);
    if (poll_result != PQOS_RETVAL_OK) {
        PLUGIN_ERROR("Failed to poll monitoring data for pids. Error [%d].", poll_result);
        // ret = -poll_result; // FIXME
        goto groups_refresh;
    }

    for (size_t i = 0; i < g_rdt->num_ngroups; i++) {
        if (g_rdt->pngroups[i] == NULL || g_rdt->ngroups[i].monitored_pids_count == 0)
            continue;

        /* Submit monitoring data */
        rdt_submit(g_rdt->pngroups[i]);
    }

#ifdef NCOLLECTD_DEBUG
    rdt_dump_pids_data();
#endif

groups_refresh:
    ret = proc_pids_update(RDT_PROC_PATH, g_rdt->proc_pids, g_rdt->num_proc_pids);
    if (ret != 0) {
        PLUGIN_ERROR("Initial update of proc pids failed");
        return ret;
    }

    for (size_t i = 0; i < g_rdt->num_ngroups; i++) {
        int refresh_result = rdt_refresh_ngroup(&(g_rdt->ngroups[i]), &(g_rdt->pngroups[i]));

        if (refresh_result != 0) {
            PLUGIN_ERROR("NGroup %zu refresh failed. Error: %d", i, refresh_result);
            if (ret == 0) {
                /* refresh error will be escalated only if there were no
                 * errors before.
                 */
                ret = refresh_result;
            }
        }
    }

    assert(ret <= 0);
    return ret;
}

static void rdt_init_pids_monitoring(void)
{
    for (size_t group_idx = 0; group_idx < g_rdt->num_ngroups; group_idx++) {
        /*
         * Each group must have not-null proc_pids array.
         * Initial refresh is not mandatory for proper
         * PIDs statistics detection.
         */
        rdt_name_group_t *ng = &g_rdt->ngroups[group_idx];
        int init_result = proc_pids_init(ng->names, ng->num_names, &ng->proc_pids);
        if (init_result != 0) {
            PLUGIN_ERROR("Initialization of proc_pids for group %zu failed. Error: %d",
                        group_idx, init_result);
            continue;
        }

        /* update global proc_pids table */
        proc_pids_t **proc_pids = realloc(g_rdt->proc_pids,
                               (g_rdt->num_proc_pids + ng->num_names) * sizeof(*g_rdt->proc_pids));
        if (proc_pids == NULL) {
            PLUGIN_ERROR("Alloc error\n");
            continue;
        }

        for (size_t i = 0; i < ng->num_names; i++)
            proc_pids[g_rdt->num_proc_pids + i] = ng->proc_pids[i];

        g_rdt->proc_pids = proc_pids;
        g_rdt->num_proc_pids += ng->num_names;
    }

    if (g_rdt->num_ngroups > 0) {
        int update_result = proc_pids_update(RDT_PROC_PATH, g_rdt->proc_pids, g_rdt->num_proc_pids);
        if (update_result != 0)
            PLUGIN_ERROR("Initial update of proc pids failed");
    }

    for (size_t group_idx = 0; group_idx < g_rdt->num_ngroups; group_idx++) {
        int refresh_result = rdt_refresh_ngroup(&(g_rdt->ngroups[group_idx]),
                                                &(g_rdt->pngroups[group_idx]));
        if (refresh_result != 0)
            PLUGIN_ERROR("Initial refresh of group %zu failed. Error: %d",
                          group_idx, refresh_result);
    }
}
#endif

static void rdt_free_cgroups(void)
{
    config_cores_cleanup(&g_rdt->cores);
#if PQOS_VERSION < 40600
    for (int i = 0; i < RDT_MAX_CORES; i++) {
        free(g_rdt->pcgroups[i]);
    }
#endif
    g_rdt->cores.num_cgroups = 0;
}

static int rdt_default_cgroups(void)
{
    unsigned num_cores = g_rdt->pqos_cpu->num_cores;

    g_rdt->cores.cgroups = calloc(num_cores, sizeof(*(g_rdt->cores.cgroups)));
    if (g_rdt->cores.cgroups == NULL) {
        PLUGIN_ERROR("Error allocating core groups array");
        return -ENOMEM;
    }
    g_rdt->cores.num_cgroups = num_cores;

    /* configure each core in separate group */
    for (unsigned i = 0; i < num_cores; i++) {
        core_group_t *cgroup = g_rdt->cores.cgroups + i;
        char desc[DATA_MAX_NAME_LEN];

        /* set core group info */
        cgroup->cores = calloc(1, sizeof(*cgroup->cores));
        if (cgroup->cores == NULL) {
            PLUGIN_ERROR("Error allocating cores array");
            rdt_free_cgroups();
            return -ENOMEM;
        }
        cgroup->num_cores = 1;
        cgroup->cores[0] = i;

        ssnprintf(desc, sizeof(desc), "%u", g_rdt->pqos_cpu->cores[i].lcore);
        cgroup->desc = strdup(desc);
        if (cgroup->desc == NULL) {
            PLUGIN_ERROR("Error allocating core group description");
            rdt_free_cgroups();
            return -ENOMEM;
        }
    }

    return num_cores;
}

static int rdt_is_core_id_valid(unsigned int core_id)
{
    for (unsigned int i = 0; i < g_rdt->pqos_cpu->num_cores; i++)
        if (core_id == g_rdt->pqos_cpu->cores[i].lcore)
            return 1;

    return 0;
}

static int rdt_config_cgroups(config_item_t *item)
{
    if (config_cores_parse(item, &g_rdt->cores) < 0) {
        rdt_free_cgroups();
        PLUGIN_ERROR("Error parsing core groups configuration.");
        return -EINVAL;
    }
    size_t n = g_rdt->cores.num_cgroups;

    /* validate configured core id values */
    for (size_t group_idx = 0; group_idx < n; group_idx++) {
        core_group_t *cgroup = g_rdt->cores.cgroups + group_idx;
        for (size_t core_idx = 0; core_idx < cgroup->num_cores; core_idx++) {
            if (!rdt_is_core_id_valid(cgroup->cores[core_idx])) {
                PLUGIN_ERROR("Core group '%s' contains invalid core id '%u'",
                            cgroup->desc, cgroup->cores[core_idx]);
                rdt_free_cgroups();
                return -EINVAL;
            }
        }
    }

    if (n == 0) {
        /* create default core groups if "Cores" config option is empty */
        int ret = rdt_default_cgroups();
        if (ret < 0) {
            rdt_free_cgroups();
            PLUGIN_ERROR("Error creating default core groups configuration.");
            return ret;
        }
        n = (size_t)ret;
        PLUGIN_INFO("No core groups configured. Default core groups created.");
    }

    PLUGIN_DEBUG("Number of cores in the system: %u", g_rdt->pqos_cpu->num_cores);

    g_rdt->cores.num_cgroups = n;
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < i; j++) {
            int found = config_cores_cmp_cgroups(&g_rdt->cores.cgroups[j],
                                                 &g_rdt->cores.cgroups[i]);
            if (found != 0) {
                rdt_free_cgroups();
                PLUGIN_ERROR("Cannot monitor same cores in different groups.");
                return -EINVAL;
            }
        }
#if PQOS_VERSION < 40600
        g_rdt->pcgroups[i] = calloc(1, sizeof(*g_rdt->pcgroups[i]));
        if (g_rdt->pcgroups[i] == NULL) {
            rdt_free_cgroups();
            PLUGIN_ERROR("Failed to allocate memory for monitoring data.");
            return -ENOMEM;
        }
#endif
    }

    return 0;
}

static void rdt_pqos_log(__attribute__((unused)) void *context,
                         __attribute__((unused)) const size_t size,
                         __attribute__((unused)) const char *msg)
{
    PLUGIN_DEBUG("%s", msg);
}

static int rdt_preinit(void)
{
    int ret;

    if (g_rdt != NULL) {
        /* already initialized if config callback was called before init callback */
        return 0;
    }

    g_rdt = calloc(1, sizeof(*g_rdt));
    if (g_rdt == NULL) {
        PLUGIN_ERROR("Failed to allocate memory for rdt context.");
        return -ENOMEM;
    }

    /* IPC monitoring is enabled by default */
    g_rdt->mon_ipc_enabled = true;

    struct pqos_config pqos = {.fd_log = -1,
                               .callback_log = rdt_pqos_log,
                               .context_log = NULL,
                               .verbose = 0,
#ifdef LIBPQOS2
                               .interface = PQOS_INTER_OS_RESCTRL_MON};
    PLUGIN_DEBUG("Initializing PQoS with RESCTRL interface");
#else
                               .interface = PQOS_INTER_MSR};
    PLUGIN_DEBUG("Initializing PQoS with MSR interface");
#endif

    ret = pqos_init(&pqos);
    PLUGIN_DEBUG("PQoS initialization result: [%d]", ret);

#ifdef LIBPQOS2
    if (ret == PQOS_RETVAL_INTER) {
        pqos.interface = PQOS_INTER_MSR;
        PLUGIN_DEBUG("Initializing PQoS with MSR interface");
        ret = pqos_init(&pqos);
        PLUGIN_DEBUG("PQoS initialization result: [%d]", ret);
    }
#endif

    if (ret != PQOS_RETVAL_OK) {
        PLUGIN_ERROR("Error initializing PQoS library!");
        goto rdt_preinit_error1;
    }

    g_interface = pqos.interface;

    ret = pqos_cap_get(&g_rdt->pqos_cap, &g_rdt->pqos_cpu);
    if (ret != PQOS_RETVAL_OK) {
        PLUGIN_ERROR("Error retrieving PQoS capabilities.");
        goto rdt_preinit_error2;
    }

    ret = pqos_cap_get_type(g_rdt->pqos_cap, PQOS_CAP_TYPE_MON, &g_rdt->cap_mon);
    if (ret == PQOS_RETVAL_PARAM) {
        PLUGIN_ERROR("Error retrieving monitoring capabilities.");
        goto rdt_preinit_error2;
    }

    if (g_rdt->cap_mon == NULL) {
        PLUGIN_ERROR("Monitoring capability not detected. Nothing to do for the plugin.");
        goto rdt_preinit_error2;
    }

    /* Reset pqos monitoring groups registers */
    pqos_mon_reset();

    return 0;

rdt_preinit_error2:
    pqos_fini();

rdt_preinit_error1:
    free(g_rdt);
    g_rdt = NULL;

    return -1;
}

static int rdt_config(config_item_t *ci)
{
    if (rdt_preinit() != 0) {
        g_state = CONFIGURATION_ERROR;
        /* if we return -1 at this point ncollectd
            reports a failure in configuration and
            aborts
        */
        return 0;
    }

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("cores", child->key) == 0) {
            if (g_rdt->cores.num_cgroups > 0) {
                PLUGIN_ERROR("Configuration parameter \"%s\" can be used only once.", child->key);
                g_state = CONFIGURATION_ERROR;
            } else if (rdt_config_cgroups(child) != 0) {
                g_state = CONFIGURATION_ERROR;
            }

            if (g_state == CONFIGURATION_ERROR) {
                /* if we return -1 at this point ncollectd reports a failure in configuration and aborts */
                return 0;
            }
        } else if (strcasecmp("processes", child->key) == 0) {
#ifdef LIBPQOS2
            if (g_interface != PQOS_INTER_OS_RESCTRL_MON) {
                PLUGIN_ERROR("Configuration parameter \"%s\" not supported. "
                             "Resctrl monitoring is needed for PIDs monitoring.", child->key);
                g_state = CONFIGURATION_ERROR;
            } else if (g_rdt->num_ngroups > 0) {
                PLUGIN_ERROR("Configuration parameter \"%s\" can be used only once.", child->key);
                g_state = CONFIGURATION_ERROR;
            } else if (rdt_config_ngroups(g_rdt, child) != 0) {
                g_state = CONFIGURATION_ERROR;
            }

            if (g_state == CONFIGURATION_ERROR) {
                /* if we return -1 at this point ncollectd reports a failure in configuration and aborts */
                return 0;
            }
#else
            PLUGIN_ERROR("Configuration parameter \"%s\" not supported, please "
                         "recompile ncollectd with libpqos version 2.0 or newer.",
                         child->key);
#endif
        } else if (strcasecmp("mon-ipc-enabled", child->key) == 0) {
            int status = cf_util_get_boolean(child, &g_rdt->mon_ipc_enabled);
            if (status != 0)
                g_state = CONFIGURATION_ERROR;
        } else if (strcasecmp("mon-llc-ref-enabled", child->key) == 0) {
#if PQOS_VERSION >= 40400
            int status = cf_util_get_boolean(child, &g_rdt->mon_llc_ref_enabled);
            if (status != 0)
                g_state = CONFIGURATION_ERROR;
#else
            PLUGIN_ERROR("Configuration parameter \"%s\" not supported, please "
                         "recompile ncollectd with libpqos version 4.4 or newer.",
                         child->key);
#endif
        } else {
            PLUGIN_ERROR("Unknown configuration parameter \"%s\".", child->key);
        }
    }

    rdt_config_events(g_rdt);

#ifdef NCOLLECTD_DEBUG
    rdt_dump_cgroups();
#ifdef LIBPQOS2
    rdt_dump_ngroups();
#endif
#endif
    return 0;
}

static int read_cores_data(void)
{
    if (g_rdt->cores.num_cgroups == 0) {
        PLUGIN_DEBUG("not configured - Cores read skipped");
        return 0;
    }
    PLUGIN_DEBUG("Cores data poll");

    int ret = pqos_mon_poll(g_rdt->pcgroups, (unsigned)g_rdt->cores.num_cgroups);
    if (ret != PQOS_RETVAL_OK) {
        PLUGIN_ERROR("Failed to poll monitoring data for cores. Error [%d].", ret);
        return -1;
    }

    /* Submit monitoring data */
    for (size_t i = 0; i < g_rdt->cores.num_cgroups; i++)
        rdt_submit(g_rdt->pcgroups[i]);

#ifdef NCOLLECTD_DEBUG
    rdt_dump_cores_data();
#endif

    return 0;
}

static int rdt_read(void)
{
    if (g_rdt == NULL) {
        PLUGIN_ERROR("plugin not initialized.");
        return -EINVAL;
    }

    int cores_read_result = read_cores_data();

#ifdef LIBPQOS2
    int pids_read_result = read_pids_data();
#endif

    if (cores_read_result != 0)
        return cores_read_result;

#ifdef LIBPQOS2
    if (pids_read_result != 0)
        return pids_read_result;
#endif

    return 0;
}

static void rdt_init_cores_monitoring(void)
{
    for (size_t i = 0; i < g_rdt->cores.num_cgroups; i++) {
        core_group_t *cg = g_rdt->cores.cgroups + i;
#if PQOS_VERSION >= 40600
        int mon_start_result = pqos_mon_start_cores(cg->num_cores, cg->cores, g_rdt->events[i],
                                                    (void *)cg->desc, &g_rdt->pcgroups[i]);
#else
        int mon_start_result = pqos_mon_start(cg->num_cores, cg->cores, g_rdt->events[i],
                                              (void *)cg->desc, g_rdt->pcgroups[i]);
#endif

        if (mon_start_result != PQOS_RETVAL_OK)
            PLUGIN_ERROR("Error starting cores monitoring group %s (pqos status=%d)",
                         cg->desc, mon_start_result);
    }
}

static int rdt_init(void)
{
    if (g_state == CONFIGURATION_ERROR) {
        if (g_rdt != NULL) {
            if (g_rdt->cores.num_cgroups > 0)
                rdt_free_cgroups();
#ifdef LIBPQOS2
            if (g_rdt->num_ngroups > 0)
                rdt_free_ngroups(g_rdt);
#endif
        }
        return -1;
    }

    int rdt_preinint_result = rdt_preinit();
    if (rdt_preinint_result != 0)
        return rdt_preinint_result;

    rdt_init_cores_monitoring();
#ifdef LIBPQOS2
    rdt_init_pids_monitoring();
#endif

    return 0;
}

static int rdt_shutdown(void)
{
    if (g_rdt == NULL)
        return 0;

    /* Stop monitoring cores */
    for (size_t i = 0; i < g_rdt->cores.num_cgroups; i++) {
        /* In pqos 4.6.0 and later this frees memory */
        pqos_mon_stop(g_rdt->pcgroups[i]);
    }

    /* Stop pids monitoring */
#ifdef LIBPQOS2
    for (size_t i = 0; i < g_rdt->num_ngroups; i++) {
        /* In pqos 4.6.0 and later this frees memory */
        pqos_mon_stop(g_rdt->pngroups[i]);
    }
#endif

    int ret = pqos_fini();
    if (ret != PQOS_RETVAL_OK)
        PLUGIN_ERROR("Error shutting down PQoS library.");
    rdt_free_cgroups();
#ifdef LIBPQOS2
    rdt_free_ngroups(g_rdt);
#endif
    free(g_rdt);

    return 0;
}

void module_register(void)
{
    plugin_register_init("intel_rdt", rdt_init);
    plugin_register_config("intel_rdt", rdt_config);
    plugin_register_read("intel_rdt", rdt_read);
    plugin_register_shutdown("intel_rdt", rdt_shutdown);
}
