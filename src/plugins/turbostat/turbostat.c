// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (c) 2013 Intel Corporation.
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Len Brown <len.brown at intel.com>
// SPDX-FileContributor: Vincent Brillault <git at lerya.net>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

// Based on the 'turbostat' tool of the Linux kernel, found at
// linux/tools/power/x86/turbostat/turbostat.c:
// ----
// Copyright (c) 2013 Intel Corporation.
// Len Brown <len.brown@intel.com>
// ----
// Ported to collectd by Vincent Brillault <git@lerya.net>

/*
 * _GNU_SOURCE is required because of the following functions:
 * - CPU_ISSET_S
 * - CPU_ZERO_S
 * - CPU_SET_S
 * - CPU_FREE
 * - CPU_ALLOC
 * - CPU_ALLOC_SIZE
 */
#define _GNU_SOURCE

#include "plugin.h"
#include "libutils/common.h"

#include "msr-index.h"
#include <cpuid.h>
#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#endif /* HAVE_SYS_CAPABILITY_H */

enum {
    FAM_TURBOSTAT_CPU_C0_RATIO,
    FAM_TURBOSTAT_CPU_C1_RATIO,
    FAM_TURBOSTAT_CPU_FREQUENCY_AVERAGE,
    FAM_TURBOSTAT_CPU_FREQUENCY_BUSY,
    FAM_TURBOSTAT_CPU_TSC,
    FAM_TURBOSTAT_CPU_SMI,
    FAM_TURBOSTAT_CORE_C3_RATIO,
    FAM_TURBOSTAT_CORE_C6_RATIO,
    FAM_TURBOSTAT_CORE_C7_RATIO,
    FAM_TURBOSTAT_CORE_TEMPERATURE_CELSIUS,
    FAM_TURBOSTAT_PKG_TEMPERATURE_CELSIUS,
    FAM_TURBOSTAT_PKG_PC2_RATIO,
    FAM_TURBOSTAT_PKG_PC3_RATIO,
    FAM_TURBOSTAT_PKG_PC6_RATIO,
    FAM_TURBOSTAT_PKG_PC7_RATIO,
    FAM_TURBOSTAT_PKG_PC8_RATIO,
    FAM_TURBOSTAT_PKG_PC9_RATIO,
    FAM_TURBOSTAT_PKG_PC10_RATIO,
    FAM_TURBOSTAT_PKG_POWER_WATTS,
    FAM_TURBOSTAT_PKG_TDP_WATTS,
    FAM_TURBOSTAT_PKG_CORES_POWER_WATTS,
    FAM_TURBOSTAT_PKG_GFX_POWER_WATTS,
    FAM_TURBOSTAT_PKG_DRAM_POWER_WATTS,
    FAM_TURBOSTAT_PKG_TURBO_ENABLED,
    FAM_TURBOSTAT_PKG_PSTATES_ENABLED,
    FAM_TURBOSTAT_PKG_UNCORE_RATIO,
    FAM_TURBOSTAT_PKG_TCC_ACTIVATION_CELSIUS,
    FAM_TURBOSTAT_MAX,
};

static metric_family_t fams[FAM_TURBOSTAT_MAX] = {
    [FAM_TURBOSTAT_CPU_C0_RATIO] = {
        .name = "system_turbostat_cpu_c0_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_CPU_C1_RATIO] = {
        .name = "system_turbostat_cpu_c1_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_CPU_FREQUENCY_AVERAGE] = {
        .name = "system_turbostat_cpu_frequency_average",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_CPU_FREQUENCY_BUSY] = {
        .name = "system_turbostat_cpu_frequency_busy",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_CPU_TSC] = {
        .name = "system_turbostat_cpu_tsc",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_CPU_SMI] = {
        .name = "system_turbostat_cpu_smi",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_CORE_C3_RATIO] = {
        .name = "system_turbostat_core_c3_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_CORE_C6_RATIO] = {
        .name = "system_turbostat_core_c6_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_CORE_C7_RATIO] = {
        .name = "system_turbostat_core_c7_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_CORE_TEMPERATURE_CELSIUS] = {
        .name = "system_turbostat_core_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_TEMPERATURE_CELSIUS] = {
        .name = "system_turbostat_pkg_temperature_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_PC2_RATIO] = {
        .name = "system_turbostat_pkg_pc2_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_PC3_RATIO] = {
        .name = "system_turbostat_pkg_pc3_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_PC6_RATIO] = {
        .name = "system_turbostat_pkg_pc6_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_PC7_RATIO] = {
        .name = "system_turbostat_pkg_pc7_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_PC8_RATIO] = {
        .name = "system_turbostat_pkg_pc8_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_PC9_RATIO] = {
        .name = "system_turbostat_pkg_pc9_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_PC10_RATIO] = {
        .name = "system_turbostat_pkg_pc10_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_POWER_WATTS] = {
        .name = "system_turbostat_pkg_power_watts",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_TDP_WATTS] = {
        .name = "system_turbostat_pkg_tdp_watts",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_CORES_POWER_WATTS] = {
        .name = "system_turbostat_pkg_cores_power_watts",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_GFX_POWER_WATTS] = {
        .name = "system_turbostat_pkg_gfx_power_watts",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_DRAM_POWER_WATTS] = {
        .name = "system_turbostat_pkg_dram_power_watts",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_TURBO_ENABLED] = {
        .name = "system_turbostat_pkg_turbo_enabled",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_PSTATES_ENABLED] = {
        .name = "system_turbostat_pkg_pstates_enabled",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_UNCORE_RATIO] = {
        .name = "system_turbostat_pkg_uncore_ratio",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_TURBOSTAT_PKG_TCC_ACTIVATION_CELSIUS] = {
        .name = "system_turbostat_pkg_tcc_activation_celsius",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
};

typedef enum affinity_policy_enum {
    policy_restore_affinity, /* restore cpu affinity to whatever it was before */
    policy_allcpus_affinity  /* do not restore affinity, set to all cpus */
} affinity_policy_t;

/* the default is to set cpu affinity to all cpus */
static affinity_policy_t affinity_policy = policy_allcpus_affinity;

/*
 * This tool uses the Model-Specific Registers (MSRs) present on Intel
 * processors.
 * The general description each of these registers, depending on the
 * architecture,
 * can be found in the Intel® 64 and IA-32 Architectures Software Developer
 * Manual,
 * Volume 3 Chapter 35.
 */

/*
 * If set, aperf_mperf_unstable disables a/mperf based stats.
 * This includes: C0 & C1 states, frequency
 *
 * This value is automatically set if mperf or aperf go backward
 */
static bool aperf_mperf_unstable;

/*
 * Bitmask of the list of core C states supported by the processor.
 * Currently supported C-states (by this plugin): 3, 6, 7
 */
static unsigned int do_core_cstate;
static unsigned int config_core_cstate;
static bool apply_config_core_cstate;

/*
 * Bitmask of the list of packages C states supported by the processor.
 * Currently supported C-states (by this plugin): 2, 3, 6, 7, 8, 9, 10
 */
static unsigned int do_pkg_cstate;
static unsigned int config_pkg_cstate;
static bool apply_config_pkg_cstate;

/*
 * Boolean indicating if the processor supports 'I/O System-Management Interrupt
 * counter'
 */
static bool do_smi;
static bool config_smi;
static bool apply_config_smi;

/*
 * Boolean indicating if the processor supports 'Digital temperature sensor'
 * This feature enables the monitoring of the temperature of each core
 *
 * This feature has two limitations:
 *  - if MSR_IA32_TEMPERATURE_TARGET is not supported, the absolute temperature
 * might be wrong
 *  - Temperatures above the tcc_activation_temp are not recorded
 */
static bool do_dts;
static bool config_dts;
static bool apply_config_dts;

/*
 * Boolean indicating if the processor supports 'Package thermal management'
 * This feature allows the monitoring of the temperature of each package
 *
 * This feature has two limitations:
 *  - if MSR_IA32_TEMPERATURE_TARGET is not supported, the absolute temperature
 * might be wrong
 *  - Temperatures above the tcc_activation_temp are not recorded
 */
static bool do_ptm;
static bool config_ptm;
static bool apply_config_ptm;

/*
 * Thermal Control Circuit Activation Temperature as configured by the user.
 * This override the automated detection via MSR_IA32_TEMPERATURE_TARGET
 * and should only be used if the automated detection fails.
 */
static unsigned int tcc_activation_temp;

static unsigned int do_power_fields;
#define UFS_PLATFORM (1 << 0)
#define TURBO_PLATFORM (1 << 1)
#define PSTATES_PLATFORM (1 << 2)

static unsigned int do_rapl;
static unsigned int config_rapl;
static bool apply_config_rapl;
static double rapl_energy_units;
static double rapl_power_units;

#define RAPL_PKG (1 << 0)
/* 0x610 MSR_PKG_POWER_LIMIT */
/* 0x611 MSR_PKG_ENERGY_STATUS */
/* 0x614 MSR_PKG_POWER_INFO */
#define RAPL_DRAM (1 << 1)
/* 0x618 MSR_DRAM_POWER_LIMIT */
/* 0x619 MSR_DRAM_ENERGY_STATUS */
/* 0x61c MSR_DRAM_POWER_INFO */
#define RAPL_CORES (1 << 2)
/* 0x638 MSR_PP0_POWER_LIMIT */
/* 0x639 MSR_PP0_ENERGY_STATUS */

#define RAPL_GFX (1 << 3)
/* 0x640 MSR_PP1_POWER_LIMIT */
/* 0x641 MSR_PP1_ENERGY_STATUS */
/* 0x642 MSR_PP1_POLICY */
#define TJMAX_DEFAULT 100

static cpu_set_t *cpu_present_set, *cpu_affinity_set, *cpu_saved_affinity_set;
static size_t cpu_present_setsize, cpu_affinity_setsize,
        cpu_saved_affinity_setsize;

static struct thread_data {
    unsigned long long tsc;
    unsigned long long aperf;
    unsigned long long mperf;
    unsigned long long c1;
    unsigned int smi_count;
    unsigned int cpu_id;
    unsigned int flags;
#define CPU_IS_FIRST_THREAD_IN_CORE 0x2
#define CPU_IS_FIRST_CORE_IN_PACKAGE 0x4
} *thread_delta, *thread_even, *thread_odd;

static struct core_data {
    unsigned long long c3;
    unsigned long long c6;
    unsigned long long c7;
    unsigned int core_temp_c;
    unsigned int core_id;
} *core_delta, *core_even, *core_odd;

static struct pkg_data {
    unsigned long long pc2;
    unsigned long long pc3;
    unsigned long long pc6;
    unsigned long long pc7;
    unsigned long long pc8;
    unsigned long long pc9;
    unsigned long long pc10;
    unsigned int package_id;
    uint32_t energy_pkg;     /* MSR_PKG_ENERGY_STATUS */
    uint32_t energy_dram;  /* MSR_DRAM_ENERGY_STATUS */
    uint32_t energy_cores; /* MSR_PP0_ENERGY_STATUS */
    uint32_t energy_gfx;     /* MSR_PP1_ENERGY_STATUS */
    uint32_t tdp;
    uint8_t turbo_enabled;
    uint8_t pstates_enabled;
    uint32_t uncore;
    unsigned int tcc_activation_temp;
    unsigned int pkg_temp_c;
} *package_delta, *package_even, *package_odd;

#define DELTA_COUNTERS thread_delta, core_delta, package_delta
#define ODD_COUNTERS thread_odd, core_odd, package_odd
#define EVEN_COUNTERS thread_even, core_even, package_even
static bool is_even = true;

static bool allocated;
static bool initialized;

#define GET_THREAD(thread_base, thread_no, core_no, pkg_no)                \
    (thread_base + (pkg_no)*topology.num_cores * topology.num_threads +    \
     (core_no)*topology.num_threads + (thread_no))
#define GET_CORE(core_base, core_no, pkg_no)                               \
    (core_base + (pkg_no)*topology.num_cores + (core_no))
#define GET_PKG(pkg_base, pkg_no) (pkg_base + pkg_no)

struct cpu_topology {
    unsigned int package_id;
    unsigned int core_id;
    bool first_core_in_package;
    bool first_thread_in_core;
};

static struct topology {
    unsigned int max_cpu_id;
    unsigned int num_packages;
    unsigned int num_cores;
    unsigned int num_threads;
    struct cpu_topology *cpus;
} topology;

static cdtime_t time_even, time_odd, time_delta;

/*****************************
 *  MSR Manipulation helpers *
 *****************************/

/*
 * Open a MSR device for reading
 * Can change the scheduling affinity of the current process if multiple_read is
 * true
 */
static int __attribute__((warn_unused_result)) open_msr(unsigned int cpu, bool multiple_read)
{
    char pathname[32];
    int fd;

    /*
     * If we need to do multiple read, let's migrate to the CPU
     * Otherwise, we would lose time calling functions on another CPU
     *
     * If we are not yet initialized (cpu_affinity_setsize = 0),
     * we need to skip this optimisation.
     */
    if (multiple_read && cpu_affinity_setsize) {
        CPU_ZERO_S(cpu_affinity_setsize, cpu_affinity_set);
        CPU_SET_S(cpu, cpu_affinity_setsize, cpu_affinity_set);
        if (sched_setaffinity(0, cpu_affinity_setsize, cpu_affinity_set) == -1) {
            PLUGIN_ERROR("Could not migrate to CPU %u", cpu);
            return -1;
        }
    }

    snprintf(pathname, sizeof(pathname), "/dev/cpu/%u/msr", cpu);
    fd = open(pathname, O_RDONLY);
    if (fd < 0) {
        PLUGIN_ERROR("failed to open %s", pathname);
        return -1;
    }
    return fd;
}

/*
 * Read a single MSR from an open file descriptor
 */
static int __attribute__((warn_unused_result)) read_msr(int fd, off_t offset, unsigned long long *msr)
{
    ssize_t retval;

    retval = pread(fd, msr, sizeof *msr, offset);

    if (retval != sizeof *msr) {
        PLUGIN_ERROR("MSR offset 0x%llx read failed",
                    (unsigned long long)offset);
        return -1;
    }
    return 0;
}

/*
 * Open a MSR device for reading, read the value asked for and close it.
 * This call will not affect the scheduling affinity of this thread.
 */
static ssize_t __attribute__((warn_unused_result)) get_msr(unsigned int cpu, off_t offset,
                                                           unsigned long long *msr)
{
    ssize_t retval;
    int fd;

    fd = open_msr(cpu, 0);
    if (fd < 0)
        return fd;
    retval = read_msr(fd, offset, msr);
    close(fd);
    return retval;
}

/********************************
 * Raw data acquisition (1 CPU) *
 ********************************/

/*
 * Read every data available for a single CPU
 *
 * Core data is shared for all threads in one core: extracted only for the first
 * thread
 * Package data is shared for all core in one package: extracted only for the
 * first thread of the first core
 *
 * Side effect: migrates to the targeted CPU
 */
static int __attribute__((warn_unused_result)) get_counters(struct thread_data *t,
                                                            struct core_data *c,
                                                            struct pkg_data *p)
{
    unsigned int cpu = t->cpu_id;
    unsigned long long msr;
    int msr_fd;
    int retval = 0;

    msr_fd = open_msr(cpu, 1);
    if (msr_fd < 0)
        return msr_fd;

#define READ_MSR(msr, dst)                                   \
    do {                                                     \
        if (read_msr(msr_fd, msr, dst)) {                    \
            PLUGIN_ERROR("Unable to read " #msr); \
            retval = -1;                                     \
            goto out;                                        \
        }                                                    \
    } while (0)

    READ_MSR(MSR_IA32_TSC, &t->tsc);

    READ_MSR(MSR_IA32_APERF, &t->aperf);
    READ_MSR(MSR_IA32_MPERF, &t->mperf);

    if (do_smi) {
        READ_MSR(MSR_SMI_COUNT, &msr);
        t->smi_count = msr & 0xFFFFFFFF;
    }

    /* collect core counters only for 1st thread in core */
    if (!(t->flags & CPU_IS_FIRST_THREAD_IN_CORE)) {
        retval = 0;
        goto out;
    }

    if (do_core_cstate & (1 << 3))
        READ_MSR(MSR_CORE_C3_RESIDENCY, &c->c3);
    if (do_core_cstate & (1 << 6))
        READ_MSR(MSR_CORE_C6_RESIDENCY, &c->c6);
    if (do_core_cstate & (1 << 7))
        READ_MSR(MSR_CORE_C7_RESIDENCY, &c->c7);

    if (do_dts) {
        READ_MSR(MSR_IA32_THERM_STATUS, &msr);
        c->core_temp_c = p->tcc_activation_temp - ((msr >> 16) & 0x7F);
    }

    /* collect package counters only for 1st core in package */
    if (!(t->flags & CPU_IS_FIRST_CORE_IN_PACKAGE)) {
        retval = 0;
        goto out;
    }

    if (do_pkg_cstate & (1 << 2))
        READ_MSR(MSR_PKG_C2_RESIDENCY, &p->pc2);
    if (do_pkg_cstate & (1 << 3))
        READ_MSR(MSR_PKG_C3_RESIDENCY, &p->pc3);
    if (do_pkg_cstate & (1 << 6))
        READ_MSR(MSR_PKG_C6_RESIDENCY, &p->pc6);
    if (do_pkg_cstate & (1 << 7))
        READ_MSR(MSR_PKG_C7_RESIDENCY, &p->pc7);
    if (do_pkg_cstate & (1 << 8))
        READ_MSR(MSR_PKG_C8_RESIDENCY, &p->pc8);
    if (do_pkg_cstate & (1 << 9))
        READ_MSR(MSR_PKG_C9_RESIDENCY, &p->pc9);
    if (do_pkg_cstate & (1 << 10))
        READ_MSR(MSR_PKG_C10_RESIDENCY, &p->pc10);

    if (do_rapl & RAPL_PKG) {
        READ_MSR(MSR_PKG_ENERGY_STATUS, &msr);
        p->energy_pkg = msr & 0xFFFFFFFF;
        READ_MSR(MSR_PKG_POWER_INFO, &msr);
        p->tdp = msr & 0x7FFF;
    }
    if (do_rapl & RAPL_CORES) {
        READ_MSR(MSR_PP0_ENERGY_STATUS, &msr);
        p->energy_cores = msr & 0xFFFFFFFF;
    }
    if (do_rapl & RAPL_DRAM) {
        READ_MSR(MSR_DRAM_ENERGY_STATUS, &msr);
        p->energy_dram = msr & 0xFFFFFFFF;
    }
    if (do_rapl & RAPL_GFX) {
        READ_MSR(MSR_PP1_ENERGY_STATUS, &msr);
        p->energy_gfx = msr & 0xFFFFFFFF;
    }
    if (do_ptm) {
        READ_MSR(MSR_IA32_PACKAGE_THERM_STATUS, &msr);
        p->pkg_temp_c = p->tcc_activation_temp - ((msr >> 16) & 0x7F);
    }
    if (do_power_fields & TURBO_PLATFORM) {
        READ_MSR(MSR_IA32_MISC_ENABLE, &msr);
        p->turbo_enabled = !((msr >> 38) & 0x1);
    }
    if (do_power_fields & PSTATES_PLATFORM) {
        READ_MSR(MSR_IA32_MISC_ENABLE, &msr);
        p->pstates_enabled = (msr >> 16) & 0x1;
    }
    if (do_power_fields & UFS_PLATFORM) {
        READ_MSR(MSR_UNCORE_FREQ_SCALING, &msr);
        p->uncore = msr & 0x1F;
    }

out:
    close(msr_fd);
    return retval;
}

/**********************************
 * Evaluating the changes (1 CPU) *
 **********************************/

/*
 * Extract the evolution old->new in delta at a package level
 * (some are not new-delta, e.g. temperature)
 */
static inline void delta_package(struct pkg_data *delta,
                                                                 const struct pkg_data *new,
                                                                 const struct pkg_data *old)
{
    delta->pc2 = new->pc2 - old->pc2;
    delta->pc3 = new->pc3 - old->pc3;
    delta->pc6 = new->pc6 - old->pc6;
    delta->pc7 = new->pc7 - old->pc7;
    delta->pc8 = new->pc8 - old->pc8;
    delta->pc9 = new->pc9 - old->pc9;
    delta->pc10 = new->pc10 - old->pc10;
    delta->pkg_temp_c = new->pkg_temp_c;

    delta->energy_pkg = new->energy_pkg - old->energy_pkg;
    delta->energy_cores = new->energy_cores - old->energy_cores;
    delta->energy_gfx = new->energy_gfx - old->energy_gfx;
    delta->energy_dram = new->energy_dram - old->energy_dram;
    delta->tdp = new->tdp;
    delta->turbo_enabled = new->turbo_enabled;
    delta->pstates_enabled = new->pstates_enabled;
    delta->tcc_activation_temp = new->tcc_activation_temp;
    delta->uncore = new->uncore;
}

/*
 * Extract the evolution old->new in delta at a core level
 * (some are not new-delta, e.g. temperature)
 */
static inline void delta_core(struct core_data *delta,
                                                            const struct core_data *new,
                                                            const struct core_data *old)
{
    delta->c3 = new->c3 - old->c3;
    delta->c6 = new->c6 - old->c6;
    delta->c7 = new->c7 - old->c7;
    delta->core_temp_c = new->core_temp_c;
}

/*
 * Extract the evolution old->new in delta at a package level
 * core_delta is required for c1 estimation (tsc - c0 - all core cstates)
 */
static inline int __attribute__((warn_unused_result))
delta_thread(struct thread_data *delta, const struct thread_data *new,
                         const struct thread_data *old, const struct core_data *cdelta)
{
    delta->tsc = new->tsc - old->tsc;

    /* check for TSC < 1 Mcycles over interval */
    if (delta->tsc < (1000 * 1000)) {
        PLUGIN_WARNING("Insanely slow TSC rate, TSC stops "
                        "in idle? You can disable all c-states by booting with"
                        " 'idle=poll' or just the deep ones with"
                        " 'processor.max_cstate=1'");
        return -1;
    }

    delta->c1 = new->c1 - old->c1;

    if ((new->aperf > old->aperf) && (new->mperf > old->mperf)) {
        delta->aperf = new->aperf - old->aperf;
        delta->mperf = new->mperf - old->mperf;
    } else {
        if (!aperf_mperf_unstable) {
            PLUGIN_WARNING("APERF or MPERF went "
                            "backwards. Frequency results do not cover "
                            "the entire interval. Fix this by running "
                            "Linux-2.6.30 or later.");

            aperf_mperf_unstable = true;
        }
    }

    /*
     * As counter collection is not atomic,
     * it is possible for mperf's non-halted cycles + idle states
     * to exceed TSC's all cycles: show c1 = 0% in that case.
     */
    if ((delta->mperf + cdelta->c3 + cdelta->c6 + cdelta->c7) > delta->tsc)
        delta->c1 = 0;
    else {
        /* normal case, derive c1 */
        delta->c1 =
                delta->tsc - delta->mperf - cdelta->c3 - cdelta->c6 - cdelta->c7;
    }

    if (delta->mperf == 0) {
        PLUGIN_WARNING("cpu%u MPERF 0!", old->cpu_id);
        delta->mperf = 1; /* divide by 0 protection */
    }

    if (do_smi)
        delta->smi_count = new->smi_count - old->smi_count;

    return 0;
}

/**********************************
 * Submitting the results (1 CPU) *
 **********************************/

/*
 * Submit every data for a single CPU
 *
 * Core data is shared for all threads in one core: submitted only for the first
 * thread
 * Package data is shared for all core in one package: submitted only for the
 * first thread of the first core
 */
static int submit_counters(struct thread_data *t, struct core_data *c,
                                                     struct pkg_data *p)
{
    PLUGIN_DEBUG("submit stats for cpu: %u, core: %u, pkg: %u",
                t->cpu_id, c->core_id, p->package_id);

    double interval_float = CDTIME_T_TO_DOUBLE(time_delta);

    metric_t m = {0};

    char cpu[24];
    ssnprintf(cpu, sizeof(cpu), "%u", t->cpu_id);
    metric_label_set(&m, "cpu", cpu);

    char core[24];
    ssnprintf(core, sizeof(core), "%u", c->core_id);
    metric_label_set(&m, "core", core);

    char pkg[24];
    ssnprintf(pkg, sizeof(pkg), "%u", p->package_id);
    metric_label_set(&m, "package", pkg);

    if (!aperf_mperf_unstable) {
        m.value = VALUE_GAUGE(100.0 * t->mperf / t->tsc);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_CPU_C0_RATIO], m);
    }
    if (!aperf_mperf_unstable) {
        m.value = VALUE_GAUGE(100.0 * t->c1 / t->tsc);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_CPU_C1_RATIO], m);
    }

    m.value = VALUE_GAUGE(1.0 / 1000000 * t->aperf / interval_float);
    metric_family_metric_append(&fams[FAM_TURBOSTAT_CPU_FREQUENCY_AVERAGE], m);

    if ((!aperf_mperf_unstable) || (!(t->aperf > t->tsc || t->mperf > t->tsc))) {
        m.value = VALUE_GAUGE(1.0 * t->tsc / 1000000 * t->aperf / t->mperf / interval_float);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_CPU_FREQUENCY_BUSY], m);
    }

    /* Sanity check (should stay stable) */
    m.value = VALUE_GAUGE(1.0 * t->tsc / 1000000 / interval_float);
    metric_family_metric_append(&fams[FAM_TURBOSTAT_CPU_TSC], m);

    /* SMI */
    if (do_smi) {
        m.value = VALUE_GAUGE(t->smi_count);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_CPU_SMI], m);
    }

    /* submit per-core data only for 1st thread in core */
    if (!(t->flags & CPU_IS_FIRST_THREAD_IN_CORE))
        goto done;

    metric_label_set(&m, "cpu", NULL);

    if (do_core_cstate & (1 << 3)) {
        m.value = VALUE_GAUGE(100.0 * c->c3 / t->tsc);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_CORE_C3_RATIO], m);
    }
    if (do_core_cstate & (1 << 6)) {
        m.value = VALUE_GAUGE(100.0 * c->c6 / t->tsc);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_CORE_C6_RATIO], m);
    }
    if (do_core_cstate & (1 << 7)) {
        m.value = VALUE_GAUGE(100.0 * c->c7 / t->tsc);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_CORE_C7_RATIO], m);
    }

    if (do_dts) {
        m.value = VALUE_GAUGE(c->core_temp_c);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_CORE_TEMPERATURE_CELSIUS], m);
    }

    /* submit per-package data only for 1st core in package */
    if (!(t->flags & CPU_IS_FIRST_CORE_IN_PACKAGE))
        goto done;

    metric_label_set(&m, "core", NULL);

    if (do_ptm) {
        m.value = VALUE_GAUGE(p->pkg_temp_c);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_TEMPERATURE_CELSIUS], m);
    }

    if (do_pkg_cstate & (1 << 2)) {
        m.value = VALUE_GAUGE(100.0 * p->pc2 / t->tsc);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_PC2_RATIO], m);
    }
    if (do_pkg_cstate & (1 << 3)) {
        m.value = VALUE_GAUGE(100.0 * p->pc3 / t->tsc);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_PC3_RATIO], m);
    }
    if (do_pkg_cstate & (1 << 6)) {
        m.value = VALUE_GAUGE(100.0 * p->pc6 / t->tsc);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_PC6_RATIO], m);
    }
    if (do_pkg_cstate & (1 << 7)) {
        m.value = VALUE_GAUGE(100.0 * p->pc7 / t->tsc);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_PC7_RATIO], m);
    }
    if (do_pkg_cstate & (1 << 8)) {
        m.value = VALUE_GAUGE(100.0 * p->pc8 / t->tsc);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_PC8_RATIO], m);
    }
    if (do_pkg_cstate & (1 << 9)) {
        m.value = VALUE_GAUGE(100.0 * p->pc9 / t->tsc);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_PC9_RATIO], m);
    }
    if (do_pkg_cstate & (1 << 10)) {
        m.value = VALUE_GAUGE(100.0 * p->pc10 / t->tsc);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_PC10_RATIO], m);
    }

    if (do_rapl) {
        if (do_rapl & RAPL_PKG) {
            m.value = VALUE_GAUGE(p->energy_pkg * rapl_energy_units / interval_float);
            metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_POWER_WATTS], m);
            m.value = VALUE_GAUGE(p->tdp * rapl_power_units);
            metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_TDP_WATTS], m);
        }
        if (do_rapl & RAPL_CORES) {
            m.value = VALUE_GAUGE(p->energy_cores * rapl_energy_units / interval_float);
            metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_CORES_POWER_WATTS], m);
        }
        if (do_rapl & RAPL_GFX) {
            m.value = VALUE_GAUGE(p->energy_gfx * rapl_energy_units / interval_float);
            metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_GFX_POWER_WATTS], m);
        }
        if (do_rapl & RAPL_DRAM) {
            m.value = VALUE_GAUGE(p->energy_dram * rapl_energy_units / interval_float);
            metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_DRAM_POWER_WATTS], m);
        }
    }

    if (do_power_fields & TURBO_PLATFORM) {
        m.value = VALUE_GAUGE(p->turbo_enabled);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_TURBO_ENABLED], m);
    }
    if (do_power_fields & PSTATES_PLATFORM) {
        m.value = VALUE_GAUGE(p->pstates_enabled);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_PSTATES_ENABLED], m);
    }
    if (do_power_fields & UFS_PLATFORM) {
        m.value = VALUE_GAUGE(p->uncore);
        metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_UNCORE_RATIO], m);
    }
    m.value = VALUE_GAUGE(p->tcc_activation_temp);
    metric_family_metric_append(&fams[FAM_TURBOSTAT_PKG_TCC_ACTIVATION_CELSIUS], m);

done:
    metric_reset(&m, METRIC_TYPE_GAUGE);
    return 0;
}

/**********************************
 * Looping function over all CPUs *
 **********************************/

/*
 * Check if a given cpu id is in our compiled list of existing CPUs
 */
static int cpu_is_not_present(unsigned int cpu)
{
    return !CPU_ISSET_S(cpu, cpu_present_setsize, cpu_present_set);
}

/*
 * Loop on all CPUs in topological order
 *
 * Skip non-present cpus
 * Return the error code at the first error or 0
 */
static int __attribute__((warn_unused_result))
for_all_cpus(int(func)(struct thread_data *, struct core_data *, struct pkg_data *),
                       struct thread_data *thread_base, struct core_data *core_base,
                       struct pkg_data *pkg_base)
{
    int retval;

    for (unsigned int pkg_no = 0; pkg_no < topology.num_packages; ++pkg_no) {
        for (unsigned int core_no = 0; core_no < topology.num_cores; ++core_no) {
            for (unsigned int thread_no = 0; thread_no < topology.num_threads;
                     ++thread_no) {
                struct thread_data *t;
                struct core_data *c;
                struct pkg_data *p;

                t = GET_THREAD(thread_base, thread_no, core_no, pkg_no);

                if (cpu_is_not_present(t->cpu_id))
                    continue;

                c = GET_CORE(core_base, core_no, pkg_no);
                p = GET_PKG(pkg_base, pkg_no);

                retval = func(t, c, p);
                if (retval)
                    return retval;
            }
        }
    }
    return 0;
}

/*
 * Dedicated loop: Extract every data evolution for all CPU
 *
 * Skip non-present cpus
 * Return the error code at the first error or 0
 *
 * Core data is shared for all threads in one core: extracted only for the first
 * thread
 * Package data is shared for all core in one package: extracted only for the
 * first thread of the first core
 */
static int __attribute__((warn_unused_result))
for_all_cpus_delta(const struct thread_data *thread_new_base,
                   const struct core_data *core_new_base,
                   const struct pkg_data *pkg_new_base,
                   const struct thread_data *thread_old_base,
                   const struct core_data *core_old_base,
                   const struct pkg_data *pkg_old_base)
{
    int retval;

    for (unsigned int pkg_no = 0; pkg_no < topology.num_packages; ++pkg_no) {
        for (unsigned int core_no = 0; core_no < topology.num_cores; ++core_no) {
            for (unsigned int thread_no = 0; thread_no < topology.num_threads;
                     ++thread_no) {
                struct thread_data *t_delta;
                const struct thread_data *t_old, *t_new;
                struct core_data *c_delta;

                /* Get correct pointers for threads */
                t_delta = GET_THREAD(thread_delta, thread_no, core_no, pkg_no);
                t_new = GET_THREAD(thread_new_base, thread_no, core_no, pkg_no);
                t_old = GET_THREAD(thread_old_base, thread_no, core_no, pkg_no);

                /* Skip threads that disappeared */
                if (cpu_is_not_present(t_delta->cpu_id))
                    continue;

                /* c_delta is always required for delta_thread */
                c_delta = GET_CORE(core_delta, core_no, pkg_no);

                /* calculate core delta only for 1st thread in core */
                if (t_new->flags & CPU_IS_FIRST_THREAD_IN_CORE) {
                    const struct core_data *c_old, *c_new;

                    c_new = GET_CORE(core_new_base, core_no, pkg_no);
                    c_old = GET_CORE(core_old_base, core_no, pkg_no);

                    delta_core(c_delta, c_new, c_old);
                }

                /* Always calculate thread delta */
                retval = delta_thread(t_delta, t_new, t_old, c_delta);
                if (retval)
                    return retval;

                /* calculate package delta only for 1st core in package */
                if (t_new->flags & CPU_IS_FIRST_CORE_IN_PACKAGE) {
                    struct pkg_data *p_delta;
                    const struct pkg_data *p_old, *p_new;

                    p_delta = GET_PKG(package_delta, pkg_no);
                    p_new = GET_PKG(pkg_new_base, pkg_no);
                    p_old = GET_PKG(pkg_old_base, pkg_no);

                    delta_package(p_delta, p_new, p_old);
                }
            }
        }
    }
    return 0;
}

/***************
 * CPU Probing *
 ***************/

/*
 * MSR_IA32_TEMPERATURE_TARGET indicates the temperature where
 * the Thermal Control Circuit (TCC) activates.
 * This is usually equal to tjMax.
 *
 * Older processors do not have this MSR, so there we guess,
 * but also allow conficuration over-ride with "TCCActivationTemp".
 *
 * Several MSR temperature values are in units of degrees-C
 * below this value, including the Digital Thermal Sensor (DTS),
 * Package Thermal Management Sensor (PTM), and thermal event thresholds.
 */
static int __attribute__((warn_unused_result))
set_temperature_target(struct thread_data *t,
                       __attribute__((unused)) struct core_data *c,
                       struct pkg_data *p)
{
    unsigned long long msr;
    unsigned int target_c_local;

    /* tcc_activation_temp is used only for dts or ptm */
    if (!(do_dts || do_ptm))
        return 0;

    /* this is a per-package concept */
    if (!(t->flags & CPU_IS_FIRST_THREAD_IN_CORE) ||
            !(t->flags & CPU_IS_FIRST_CORE_IN_PACKAGE))
        return 0;

    if (tcc_activation_temp != 0) {
        p->tcc_activation_temp = tcc_activation_temp;
        return 0;
    }

    if (get_msr(t->cpu_id, MSR_IA32_TEMPERATURE_TARGET, &msr))
        goto guess;

    target_c_local = (msr >> 16) & 0xFF;

    if (!target_c_local)
        goto guess;

    p->tcc_activation_temp = target_c_local;

    return 0;

guess:
    p->tcc_activation_temp = TJMAX_DEFAULT;
    PLUGIN_WARNING("cpu%u: Guessing tjMax %u C,"
                    " Please use TCCActivationTemp to specify it.",
                    t->cpu_id, p->tcc_activation_temp);

    return 0;
}

/*
 * Identify the functionality of the CPU
 */
static int __attribute__((warn_unused_result)) probe_cpu(void)
{
    unsigned int eax, ebx, ecx, edx, max_level;
    unsigned int fms, family, model;

    /* CPUID(0):
     * - EAX: Maximum Input Value for Basic CPUID Information
     * - EBX: "Genu" (0x756e6547)
     * - EDX: "ineI" (0x49656e69)
     * - ECX: "ntel" (0x6c65746e)
     */
    max_level = ebx = ecx = edx = 0;
    __get_cpuid(0, &max_level, &ebx, &ecx, &edx);
    if (ebx != 0x756e6547 && edx != 0x49656e69 && ecx != 0x6c65746e) {
        PLUGIN_ERROR("Unsupported CPU (not Intel)");
        return -1;
    }

    /* CPUID(1):
     * - EAX: Version Information: Type, Family, Model, and Stepping ID
     *  + 4-7:   Model ID
     *  + 8-11:  Family ID
     *  + 12-13: Processor type
     *  + 16-19: Extended Model ID
     *  + 20-27: Extended Family ID
     * - EDX: Feature Information:
     *  + 5: Support for MSR read/write operations
     */
    fms = ebx = ecx = edx = 0;
    __get_cpuid(1, &fms, &ebx, &ecx, &edx);
    family = (fms >> 8) & 0xf;
    model = (fms >> 4) & 0xf;
    if (family == 0xf)
        family += (fms >> 20) & 0xf;
    if (family == 6 || family == 0xf)
        model += ((fms >> 16) & 0xf) << 4;
    if (!(edx & (1 << 5))) {
        PLUGIN_ERROR("Unsupported CPU (no MSR support)");
        return -1;
    }

    /*
     * CPUID(6):
     * - EAX:
     *  + 0: Digital temperature sensor is supported if set
     *  + 6: Package thermal management is supported if set
     * - ECX:
     *  + 0: Hardware Coordination Feedback Capability (Presence of IA32_MPERF and
     * IA32_APERF).
     *  + 3: The processor supports performance-energy bias preference if set.
     *           It also implies the presence of a new architectural MSR called
     * IA32_ENERGY_PERF_BIAS
     *
     * This check is valid for both Intel and AMD
     */
    eax = ebx = ecx = edx = 0;
    __get_cpuid(0x6, &eax, &ebx, &ecx, &edx);
    do_dts = eax & (1 << 0);
    do_ptm = eax & (1 << 6);
    if (!(ecx & (1 << 0))) {
        PLUGIN_ERROR("Unsupported CPU (No APERF)");
        return -1;
    }

    /*
     * Enable or disable C states depending on the model and family
     */
    if (family == 6) {
        switch (model) {
        /* Atom (partial) */
        case 0x27:
            do_smi = false;
            do_core_cstate = 0;
            do_pkg_cstate = (1 << 2) | (1 << 4) | (1 << 6);
            break;
        /* Silvermont */
        case 0x37: /* BYT */
        case 0x4D: /* AVN */
            do_smi = true;
            do_core_cstate = (1 << 1) | (1 << 6);
            do_pkg_cstate = (1 << 6);
            break;
        /* Nehalem */
        case 0x1A: /* Core i7, Xeon 5500 series - Bloomfield, Gainstown NHM-EP */
        case 0x1E: /* Core i7 and i5 Processor - Clarksfield, Lynnfield, Jasper
                                    Forest */
        case 0x1F: /* Core i7 and i5 Processor - Nehalem */
        case 0x2E: /* Nehalem-EX Xeon - Beckton */
            do_smi = true;
            do_core_cstate = (1 << 3) | (1 << 6);
            do_pkg_cstate = (1 << 3) | (1 << 6) | (1 << 7);
            break;
        /* Westmere */
        case 0x25: /* Westmere Client - Clarkdale, Arrandale */
        case 0x2C: /* Westmere EP - Gulftown */
        case 0x2F: /* Westmere-EX Xeon - Eagleton */
            do_smi = true;
            do_core_cstate = (1 << 3) | (1 << 6);
            do_pkg_cstate = (1 << 3) | (1 << 6) | (1 << 7);
            break;
        /* Sandy Bridge */
        case 0x2A: /* SNB */
        case 0x2D: /* SNB Xeon */
            do_smi = true;
            do_core_cstate = (1 << 3) | (1 << 6) | (1 << 7);
            do_pkg_cstate = (1 << 2) | (1 << 3) | (1 << 6) | (1 << 7);
            break;
        /* Ivy Bridge */
        case 0x3A: /* IVB */
        case 0x3E: /* IVB Xeon */
        case 0x55: /* SKX,CLX Xeon */
        case 0x6A: /* ICX Xeon */
            do_smi = true;
            do_core_cstate = (1 << 3) | (1 << 6) | (1 << 7);
            do_pkg_cstate = (1 << 2) | (1 << 3) | (1 << 6) | (1 << 7);
            break;
        /* Haswell Bridge */
        case 0x3C: /* HSW */
        case 0x3F: /* HSW */
        case 0x46: /* HSW */
            do_smi = true;
            do_core_cstate = (1 << 3) | (1 << 6) | (1 << 7);
            do_pkg_cstate = (1 << 2) | (1 << 3) | (1 << 6) | (1 << 7);
            break;
        case 0x45: /* HSW */
            do_smi = true;
            do_core_cstate = (1 << 3) | (1 << 6) | (1 << 7);
            do_pkg_cstate = (1 << 2) | (1 << 3) | (1 << 6) | (1 << 7) | (1 << 8) |
                                            (1 << 9) | (1 << 10);
            break;
        /* Broadwell */
        case 0x4F: /* BDW */
        case 0x56: /* BDX-DE */
            do_smi = true;
            do_core_cstate = (1 << 3) | (1 << 6) | (1 << 7);
            do_pkg_cstate = (1 << 2) | (1 << 3) | (1 << 6) | (1 << 7);
            break;
        case 0x3D: /* BDW */
            do_smi = true;
            do_core_cstate = (1 << 3) | (1 << 6) | (1 << 7);
            do_pkg_cstate = (1 << 2) | (1 << 3) | (1 << 6) | (1 << 7) | (1 << 8) |
                                            (1 << 9) | (1 << 10);
            break;
        default:
            do_smi = false;
            do_core_cstate = 0;
            do_pkg_cstate = 0;
            break;
        }
        switch (model) {
        case 0x2A: /* SNB */
        case 0x3A: /* IVB */
        case 0x3C: /* HSW */
        case 0x45: /* HSW */
        case 0x46: /* HSW */
        case 0x3D: /* BDW */
        case 0x5E: /* SKL */
            do_rapl = RAPL_PKG | RAPL_CORES | RAPL_GFX;
            break;
        case 0x3F: /* HSX */
        case 0x4F: /* BDX */
        case 0x56: /* BDX-DE */
            do_rapl = RAPL_PKG | RAPL_DRAM;
            do_power_fields = TURBO_PLATFORM | UFS_PLATFORM | PSTATES_PLATFORM;
            break;
        case 0x2D: /* SNB Xeon */
        case 0x3E: /* IVB Xeon */
        case 0x55: /* SKX,CLX Xeon */
        case 0x6A: /* ICX Xeon */
            do_rapl = RAPL_PKG | RAPL_CORES | RAPL_DRAM;
            do_power_fields = TURBO_PLATFORM | PSTATES_PLATFORM;
            break;
        case 0x37: /* BYT */
        case 0x4D: /* AVN */
            do_rapl = RAPL_PKG | RAPL_CORES;
            break;
        default:
            do_rapl = 0;
        }
    } else {
        PLUGIN_ERROR("Unsupported CPU (family: %#x, "
                    "model: %#x)",
                    family, model);
        return -1;
    }

    /* Override detected values with configuration */
    if (apply_config_core_cstate)
        do_core_cstate = config_core_cstate;
    if (apply_config_pkg_cstate)
        do_pkg_cstate = config_pkg_cstate;
    if (apply_config_smi)
        do_smi = config_smi;
    if (apply_config_dts)
        do_dts = config_dts;
    if (apply_config_ptm)
        do_ptm = config_ptm;
    if (apply_config_rapl)
        do_rapl = config_rapl;

    if (do_rapl) {
        unsigned long long msr;
        if (get_msr(0, MSR_RAPL_POWER_UNIT, &msr))
            return 0;

        rapl_power_units = 1.0 / (1 << (msr & 0xF));
        if (model == 0x37)
            rapl_energy_units = 1.0 * (1 << (msr >> 8 & 0x1F)) / 1000000;
        else
            rapl_energy_units = 1.0 / (1 << (msr >> 8 & 0x1F));
    }

    return 0;
}

/********************
 * Topology Probing *
 ********************/

/*
 * Read a single int from a file.
 */
static int __attribute__((format(printf, 1, 2))) parse_int_file(const char *fmt, ...)
{
    va_list args;
    char path[PATH_MAX];
    char buf[256];
    int len;
    uint64_t value;
    char *c;
    FILE *fp;

    va_start(args, fmt);
    len = vsnprintf(path, sizeof(path), fmt, args);
    va_end(args);
    if (len < 0 || len >= PATH_MAX) {
        PLUGIN_ERROR("path truncated: '%s'", path);
        return -1;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        PLUGIN_ERROR("unable to open: '%s': %s", path, strerror(errno));
        return -1;
    }

    if (fgets(buf, sizeof(buf), fp) == NULL) {
        PLUGIN_ERROR("unable to read: '%s': %s", path, strerror(errno));
        fclose(fp);
        return -1;
    }
    fclose(fp);

    /* We only care about the first integer in the range */
    c = strchr(buf, '-');
    if (c != NULL)
        *c = '\0';
    c = strchr(buf, ',');
    if (c != NULL)
        *c = '\0';
    strstripnewline(buf);

    if (parse_uinteger(buf, &value) != 0) {
        PLUGIN_ERROR("Parsing \"%s\" failed.", path);
        return -1;
    }

    return (int)value;
}

static int get_threads_on_core(unsigned int cpu)
{
    char path[80];
    FILE *filep;
    int sib1, sib2;
    int matches;
    char character;

    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%u/topology/thread_siblings_list", cpu);
    filep = fopen(path, "r");
    if (!filep) {
        PLUGIN_ERROR("Failed to open '%s'", path);
        return -1;
    }
    /*
     * file format:
     * if a pair of number with a character between: 2 siblings (eg. 1-2, or 1,4)
     * otherwinse 1 sibling (self).
     */
    matches = fscanf(filep, "%d%c%d\n", &sib1, &character, &sib2);

    fclose(filep);

    if (matches == 3)
        return 2;
    else
        return 1;
}

/*
 * run func(cpu) on every cpu in /proc/stat
 * return max_cpu number
 */
static int __attribute__((warn_unused_result)) for_all_proc_cpus(int(func)(unsigned int))
{
    FILE *fp;
    unsigned int cpu_num;
    int retval;

    fp = fopen("/proc/stat", "r");
    if (!fp) {
        PLUGIN_ERROR("Failed to open /proc/stat");
        return -1;
    }

    retval = fscanf(fp, "cpu %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d\n");
    if (retval != 0) {
        PLUGIN_ERROR("Failed to parse /proc/stat");
        fclose(fp);
        return -1;
    }

    while (1) {
        retval =
                fscanf(fp, "cpu%u %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d\n", &cpu_num);
        if (retval != 1)
            break;

        retval = func(cpu_num);
        if (retval) {
            fclose(fp);
            return retval;
        }
    }
    fclose(fp);
    return 0;
}

/*
 * Update the stored topology.max_cpu_id
 */
static int update_max_cpu_id(unsigned int cpu)
{
    if (topology.max_cpu_id < cpu)
        topology.max_cpu_id = cpu;
    return 0;
}

static int mark_cpu_present(unsigned int cpu)
{
    CPU_SET_S(cpu, cpu_present_setsize, cpu_present_set);
    return 0;
}

static int __attribute__((warn_unused_result)) allocate_cpu_set(cpu_set_t **set, size_t *size)
{
    *set = CPU_ALLOC(topology.max_cpu_id + 1);
    if (*set == NULL) {
        PLUGIN_ERROR("Unable to allocate CPU state");
        return -1;
    }
    *size = CPU_ALLOC_SIZE(topology.max_cpu_id + 1);
    CPU_ZERO_S(*size, *set);
    return 0;
}

/*
 * Build a local representation of the cpu distribution
 */
static int __attribute__((warn_unused_result)) topology_probe(void)
{
    int ret;
    unsigned int max_package_id, max_core_id, max_threads;
    max_package_id = max_core_id = max_threads = 0;

    /* Clean topology */
    free(topology.cpus);
    memset(&topology, 0, sizeof(topology));

    ret = for_all_proc_cpus(update_max_cpu_id);
    if (ret != 0)
        goto err;

    topology.cpus =
            calloc(1, (topology.max_cpu_id + 1) * sizeof(struct cpu_topology));
    if (topology.cpus == NULL) {
        PLUGIN_ERROR("Unable to allocate memory for CPU topology");
        return -1;
    }

    ret = allocate_cpu_set(&cpu_present_set, &cpu_present_setsize);
    if (ret != 0)
        goto err;
    ret = allocate_cpu_set(&cpu_affinity_set, &cpu_affinity_setsize);
    if (ret != 0)
        goto err;
    ret = allocate_cpu_set(&cpu_saved_affinity_set, &cpu_saved_affinity_setsize);
    if (ret != 0)
        goto err;

    ret = for_all_proc_cpus(mark_cpu_present);
    if (ret != 0)
        goto err;

    /*
     * For online cpus
     * find max_core_id, max_package_id
     */
    for (unsigned int i = 0; i <= topology.max_cpu_id; ++i) {
        unsigned int num_threads;
        struct cpu_topology *cpu = &topology.cpus[i];

        if (cpu_is_not_present(i)) {
            PLUGIN_WARNING("cpu%u NOT PRESENT", i);
            continue;
        }

        ret = parse_int_file("/sys/devices/system/cpu/cpu%u/topology/physical_package_id", i);
        if (ret < 0)
            goto err;
        else
            cpu->package_id = (unsigned int)ret;
        if (cpu->package_id > max_package_id)
            max_package_id = cpu->package_id;

        ret = parse_int_file("/sys/devices/system/cpu/cpu%u/topology/core_id", i);
        if (ret < 0)
            goto err;
        else
            cpu->core_id = (unsigned int)ret;
        if (cpu->core_id > max_core_id)
            max_core_id = cpu->core_id;
        ret = parse_int_file("/sys/devices/system/cpu/cpu%u/topology/core_siblings_list", i);
        if (ret < 0)
            goto err;
        else if ((unsigned int)ret == i)
            cpu->first_core_in_package = true;

        ret = get_threads_on_core(i);
        if (ret < 0)
            goto err;
        else
            num_threads = (unsigned int)ret;
        if (num_threads > max_threads)
            max_threads = num_threads;
        ret = parse_int_file("/sys/devices/system/cpu/cpu%u/topology/thread_siblings_list", i);
        if (ret < 0)
            goto err;
        else if ((unsigned int)ret == i)
            cpu->first_thread_in_core = true;

        PLUGIN_DEBUG("cpu %u pkg %u core %u\n", i, cpu->package_id, cpu->core_id);
    }
    /* Num is max + 1 (need to count 0) */
    topology.num_packages = max_package_id + 1;
    topology.num_cores = max_core_id + 1;
    topology.num_threads = max_threads;

    return 0;
err:
    free(topology.cpus);
    return ret;
}

/************************
 * Main alloc/init/free *
 ************************/

static int allocate_counters(struct thread_data **threads,
                             struct core_data **cores,
                             struct pkg_data **packages)
{
    if ((topology.num_threads == 0) || (topology.num_cores == 0) || (topology.num_packages == 0)) {
        PLUGIN_ERROR("Invalid topology: %u threads, %u cores, %u packages",
                     topology.num_threads, topology.num_cores, topology.num_packages);
        return -1;
    }

    unsigned int total_threads = topology.num_threads * topology.num_cores * topology.num_packages;
    *threads = calloc(total_threads, sizeof(struct thread_data));
    if (*threads == NULL) {
        PLUGIN_ERROR("calloc failed");
        return -1;
    }

    for (unsigned int i = 0; i < total_threads; ++i)
        (*threads)[i].cpu_id = topology.max_cpu_id + 1;

    unsigned int total_cores = topology.num_cores * topology.num_packages;
    *cores = calloc(total_cores, sizeof(struct core_data));
    if (*cores == NULL) {
        PLUGIN_ERROR("calloc failed");
        free(*threads);
        return -1;
    }

    *packages = calloc(topology.num_packages, sizeof(struct pkg_data));
    if (*packages == NULL) {
        PLUGIN_ERROR("calloc failed");
        free(*cores);
        free(*threads);
        return -1;
    }

    return 0;
}

static void init_counter(struct thread_data *thread_base,
                                                 struct core_data *core_base, struct pkg_data *pkg_base,
                                                 unsigned int cpu_id)
{
    struct thread_data *t;
    struct core_data *c;
    struct pkg_data *p;
    struct cpu_topology *cpu = &topology.cpus[cpu_id];

    t = GET_THREAD(thread_base, !(cpu->first_thread_in_core), cpu->core_id,
                                 cpu->package_id);
    c = GET_CORE(core_base, cpu->core_id, cpu->package_id);
    p = GET_PKG(pkg_base, cpu->package_id);

    t->cpu_id = cpu_id;
    if (cpu->first_thread_in_core)
        t->flags |= CPU_IS_FIRST_THREAD_IN_CORE;
    if (cpu->first_core_in_package)
        t->flags |= CPU_IS_FIRST_CORE_IN_PACKAGE;

    c->core_id = cpu->core_id;
    p->package_id = cpu->package_id;
}

static void initialize_counters(void)
{
    for (unsigned int cpu_id = 0; cpu_id <= topology.max_cpu_id; ++cpu_id) {
        if (cpu_is_not_present(cpu_id))
            continue;
        init_counter(EVEN_COUNTERS, cpu_id);
        init_counter(ODD_COUNTERS, cpu_id);
        init_counter(DELTA_COUNTERS, cpu_id);
    }
}

static void free_all_buffers(void)
{
    allocated = false;
    initialized = false;

    if (cpu_present_set != NULL) {
        CPU_FREE(cpu_present_set);
        cpu_present_set = NULL;
        cpu_present_setsize = 0;
    }

    if (cpu_affinity_set != NULL) {
        CPU_FREE(cpu_affinity_set);
        cpu_affinity_set = NULL;
        cpu_affinity_setsize = 0;
    }

    if (cpu_saved_affinity_set != NULL) {
        CPU_FREE(cpu_saved_affinity_set);
        cpu_saved_affinity_set = NULL;
        cpu_saved_affinity_setsize = 0;
    }

    if (thread_even != NULL) {
        free(thread_even);
        thread_even = NULL;
    }
    if (core_even != NULL) {
        free(core_even);
        core_even = NULL;
    }
    if (package_even != NULL) {
        free(package_even);
        package_even = NULL;
    }

    if (thread_odd != NULL) {
        free(thread_odd);
        thread_odd = NULL;
    }
    if (core_odd != NULL) {
        free(core_odd);
        core_odd = NULL;
    }
    if (package_odd != NULL) {
        free(package_odd);
        package_odd = NULL;
    }

    if (thread_delta != NULL) {
        free(thread_delta);
        thread_delta = NULL;
    }
    if (core_delta != NULL) {
        free(core_delta);
        core_delta = NULL;
    }
    if (package_delta != NULL) {
        free(package_delta);
        package_delta = NULL;
    }
}

    /**********************
     * Collectd functions *
     **********************/

#define DO_OR_GOTO_ERR(something)     \
    do {                              \
        ret = (something);            \
        if (ret < 0)                  \
            goto err;                 \
    } while (0)

static int setup_all_buffers(void)
{
    int ret;

    DO_OR_GOTO_ERR(topology_probe());
    DO_OR_GOTO_ERR(allocate_counters(&thread_even, &core_even, &package_even));
    DO_OR_GOTO_ERR(allocate_counters(&thread_odd, &core_odd, &package_odd));
    DO_OR_GOTO_ERR(allocate_counters(&thread_delta, &core_delta, &package_delta));
    initialize_counters();
    DO_OR_GOTO_ERR(for_all_cpus(set_temperature_target, EVEN_COUNTERS));
    DO_OR_GOTO_ERR(for_all_cpus(set_temperature_target, ODD_COUNTERS));

    allocated = true;
    return 0;
err:
    free_all_buffers();
    return ret;
}

static int save_affinity(void)
{
    if (affinity_policy == policy_restore_affinity) {
        /* Try to save the scheduling affinity, as it will be modified by
         * get_counters().
         */
        if (sched_getaffinity(0, cpu_saved_affinity_setsize, cpu_saved_affinity_set) != 0)
            return -1;
    }

    return 0;
}

static void restore_affinity(void)
{
    /* Let's restore the affinity to the value saved in save_affinity */
    if (affinity_policy == policy_restore_affinity)
        (void)sched_setaffinity(0, cpu_saved_affinity_setsize, cpu_saved_affinity_set);
    else {
        /* reset the affinity to all present cpus */
        (void)sched_setaffinity(0, cpu_present_setsize, cpu_present_set);
    }
}

static int turbostat_read(void)
{
    int ret;

    if (!allocated) {
        if ((ret = setup_all_buffers()) < 0)
            return ret;
    }

    if (for_all_proc_cpus(cpu_is_not_present)) {
        free_all_buffers();
        if ((ret = setup_all_buffers()) < 0)
            return ret;
        if (for_all_proc_cpus(cpu_is_not_present)) {
            PLUGIN_ERROR("CPU appeared just after initialization");
            return -1;
        }
    }

    if (save_affinity() != 0) {
        PLUGIN_ERROR("Unable to save the CPU affinity. Please read the "
                    "docs about RestoreAffinityPolicy option.");
        return -1;
    }

    if (!initialized) {
        if ((ret = for_all_cpus(get_counters, EVEN_COUNTERS)) < 0)
            goto out;
        time_even = cdtime();
        is_even = true;
        initialized = true;
        ret = 0;
        goto out;
    }

    if (is_even) {
        if ((ret = for_all_cpus(get_counters, ODD_COUNTERS)) < 0)
            goto out;
        time_odd = cdtime();
        is_even = false;
        time_delta = time_odd - time_even;
        if ((ret = for_all_cpus_delta(ODD_COUNTERS, EVEN_COUNTERS)) < 0)
            goto out;
        if ((ret = for_all_cpus(submit_counters, DELTA_COUNTERS)) < 0)
            goto out;
    } else {
        if ((ret = for_all_cpus(get_counters, EVEN_COUNTERS)) < 0)
            goto out;
        time_even = cdtime();
        is_even = true;
        time_delta = time_even - time_odd;
        if ((ret = for_all_cpus_delta(EVEN_COUNTERS, ODD_COUNTERS)) < 0)
            goto out;
        if ((ret = for_all_cpus(submit_counters, DELTA_COUNTERS)) < 0)
            goto out;
    }
    ret = 0;
out:
    restore_affinity();

    return ret;
}

static int check_permissions(void)
{
    if (getuid() == 0) {
        /* We have everything we need */
        return 0;
#if !defined(HAVE_SYS_CAPABILITY_H) && !defined(CAP_SYS_RAWIO)
    } else {
        PLUGIN_ERROR("Initialization failed: this plugin requires ncollectd to run as root");
        return -1;
    }
#else  /* HAVE_SYS_CAPABILITY_H && CAP_SYS_RAWIO */
    }

    int ret = 0;

    if (plugin_check_capability(CAP_SYS_RAWIO) != 0) {
        PLUGIN_WARNING("ncollectd doesn't have the CAP_SYS_RAWIO capability. "
                       "If you don't want to run ncollectd as root, try running "
                       "\"setcap cap_sys_rawio=ep\" on ncollectd binary");
        ret = -1;
    }

    if (euidaccess("/dev/cpu/0/msr", R_OK)) {
        PLUGIN_WARNING("ncollectd cannot open /dev/cpu/0/msr. "
                       "If you don't want to run ncollectd as root, you need to change the "
                       "ownership (chown) and permissions on /dev/cpu/*/msr to allow such access");
        ret = -1;
    }

    if (ret != 0)
        PLUGIN_ERROR("Initialization failed: this plugin "
                     "requires ncollectd to either to run as root or give "
                     "ncollectd a special capability (CAP_SYS_RAWIO) and read "
                     "access to /dev/cpu/*/msr (see previous warnings)");
    return ret;
#endif /* HAVE_SYS_CAPABILITY_H && CAP_SYS_RAWIO */
}

static int turbostat_init(void)
{
    struct stat sb;
    int ret;

    if (stat("/dev/cpu/0/msr", &sb)) {
        PLUGIN_ERROR("Initialization failed: /dev/cpu/0/msr "
                    "does not exist while the CPU supports MSR. You may be "
                    "missing the corresponding kernel module, please try '# "
                    "modprobe msr'");
        return -1;
    }

    DO_OR_GOTO_ERR(check_permissions());

    DO_OR_GOTO_ERR(probe_cpu());

    DO_OR_GOTO_ERR(setup_all_buffers());

    plugin_register_read("turbostat", turbostat_read);

    return 0;
err:
    free_all_buffers();
    return ret;
}

static int turbostat_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "core-cstates") == 0) {
            status = cf_util_get_unsigned_int(child, &config_core_cstate);
            apply_config_core_cstate = true;
        } else if (strcasecmp(child->key, "package-cstates") == 0) {
            status = cf_util_get_unsigned_int(child, &config_pkg_cstate);
            apply_config_pkg_cstate = true;
        } else if (strcasecmp(child->key, "system-management-interrupt") == 0) {
            status = cf_util_get_boolean(child, &config_smi);
            apply_config_smi = true;
        } else if (strcasecmp(child->key, "digital-temperature-sensor") == 0) {
            status = cf_util_get_boolean(child, &config_dts);
            apply_config_dts = true;
        } else if (strcasecmp(child->key, "package-thermal-management") == 0) {
            status = cf_util_get_boolean(child, &config_ptm);
            apply_config_ptm = true;
        } else if (strcasecmp(child->key, "running-average-power-limit") == 0) {
            status = cf_util_get_unsigned_int(child, &config_rapl);
            apply_config_rapl = true;
        } else if (strcasecmp(child->key, "tcc-activation-temp") == 0) {
            status = cf_util_get_unsigned_int(child, &tcc_activation_temp);
        } else if (strcasecmp(child->key, "restore-affinity-policy") == 0) {
            char *restore = NULL;
            status = cf_util_get_string(child, &restore);
            if (status == 0) {
                if (strcasecmp("restore", restore) == 0)
                    affinity_policy = policy_restore_affinity;
                else if (strcasecmp("all-cpus", restore) == 0)
                    affinity_policy = policy_allcpus_affinity;
                else {
                    PLUGIN_ERROR("Invalid 'restore-affinity-policy' '%s' in %s:%d.",
                                 restore, cf_get_file(child), cf_get_lineno(child));
                    status = -1;
                }
                free(restore);
            }
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

void module_register(void)
{
    plugin_register_init("turbostat", turbostat_init);
    plugin_register_config("turbostat", turbostat_config);
}
