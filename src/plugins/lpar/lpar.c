// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2010  Aurélien Reynaud
// Authors:
//   Aurélien Reynaud <collectd at wattapower.net>

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#include <libperfstat.h>
#include <sys/protosw.h>
#include <sys/utsname.h>

/* XINTFRAC was defined in libperfstat.h somewhere between AIX 5.3 and 6.1 */
#ifndef XINTFRAC
#include <sys/systemcfg.h>
#define XINTFRAC                                                               \
  ((double)(_system_configuration.Xint) / (double)(_system_configuration.Xfrac))
#endif

#define CLOCKTICKS_TO_TICKS(cticks) ((cticks) / XINTFRAC)

static const char *config_keys[] = {"CpuPoolStats"};
static int config_keys_num = STATIC_ARRAY_SIZE(config_keys);

static bool pool_stats;
#if PERFSTAT_SUPPORTS_DONATION
static bool donate_flag;
#endif

static perfstat_partition_total_t lparstats_old;

enum {
  FAM_LPAR_ENTITLED,
  FAM_LPAR_USER,
  FAM_LPAR_SYSTEM,
  FAM_LPAR_WAIT,
  FAM_LPAR_IDLE,
  FAM_LPAR_CONSUMED,
  FAM_LPAR_IDLE_DONATED,
  FAM_LPAR_BUSY_DONATED,
  FAM_LPAR_IDLE_STOLEN,
  FAM_LPAR_BUSY_STOLEN,
  FAM_LPAR_POOL_IDLE,
  FAM_LPAR_POOL_BUSY,
  FAM_LPAR_MAX,
};

static metric_family_t fams[FAM_LPAR_MAX] = {
  [FAM_LPAR_ENTITLED] = {
    .name = "lpar_entitled",
    .type = METRIC_TYPE_GAUGE,
  },
  [FAM_LPAR_USER] = {
    .name = "lpar_user",
    .type = METRIC_TYPE_GAUGE,
  },
  [FAM_LPAR_SYSTEM] = {
    .name = "lpar_system",
    .type = METRIC_TYPE_GAUGE,
  },
  [FAM_LPAR_WAIT] = {
    .name = "lpar_wait",
    .type = METRIC_TYPE_GAUGE,
  },
  [FAM_LPAR_IDLE] = {
    .name = "lpar_idle",
    .type = METRIC_TYPE_GAUGE,
  },
  [FAM_LPAR_CONSUMED] = {
    .name = "lpar_consumed",
    .type = METRIC_TYPE_GAUGE,
  },
  [FAM_LPAR_IDLE_DONATED] = {
    .name = "lpar_idle_donated",
    .type = METRIC_TYPE_GAUGE,
  },
  [FAM_LPAR_BUSY_DONATED] = {
    .name = "lpar_busy_donated",
    .type = METRIC_TYPE_GAUGE,
  },
  [FAM_LPAR_IDLE_STOLEN] = {
    .name = "lpar_idle_stolen",
    .type = METRIC_TYPE_GAUGE,
  },
  [FAM_LPAR_BUSY_STOLEN] = {
    .name = "lpar_busy_stolen",
    .type = METRIC_TYPE_GAUGE,
  },
  [FAM_LPAR_POOL_IDLE] = {
    .name = "lpar_pool_busy",
    .type = METRIC_TYPE_GAUGE,
  },
  [FAM_LPAR_POOL_BUSY] = {
    .name = "lpar_pool_idle",
    .type = METRIC_TYPE_GAUGE,
  },
};

static int lpar_config(const char *key, const char *value)
{
  if (strcasecmp("CpuPoolStats", key) == 0) {
    if (IS_TRUE(value))
      pool_stats = true;
    else
      pool_stats = false;
    return -1;
  }

  return 0;
}

static int lpar_init(void)
{
  /* Retrieve the initial metrics. Returns the number of structures filled. */
  int status = perfstat_partition_total(/* name = */ NULL, /* (must be NULL) */
                                        &lparstats_old,
                                        sizeof(perfstat_partition_total_t),
                                        /* number = */ 1 /* (must be 1) */);
  if (status != 1) {
    ERROR("lpar plugin: perfstat_partition_total failed: %s (%i)", STRERRNO,
          status);
    return -1;
  }

#if PERFSTAT_SUPPORTS_DONATION
  if (!lparstats_old.type.b.shared_enabled &&
      lparstats_old.type.b.donate_enabled) {
    donate_flag = true;
  }
#endif

  if (pool_stats && !lparstats_old.type.b.pool_util_authority) {
    WARNING("lpar plugin: This partition does not have pool authority. "
            "Disabling CPU pool statistics collection.");
    pool_stats = false;
  }

  return 0;
}

static int lpar_read(void)
{
  perfstat_partition_total_t lparstats;
  struct utsname name;

  /* An LPAR has the same serial number as the physical system it is currently
     running on. It is a convenient way of tracking LPARs as they are moved
     from chassis to chassis through Live Partition Mobility (LPM). */
  if (uname(&name) != 0) {
    ERROR("lpar plugin: uname failed.");
    return -1;
  }

  /* Retrieve the current metrics. Returns the number of structures filled. */
  int status = perfstat_partition_total(/* name = */ NULL, /* (must be NULL) */
                                        &lparstats, sizeof(perfstat_partition_total_t),
                                       /* number = */ 1 /* (must be 1) */);
  if (status != 1) {
    ERROR("lpar plugin: perfstat_partition_total failed: %s (%i)", STRERRNO, status);
    return -1;
  }

  /* Number of ticks since we last run. */
  u_longlong_t ticks = lparstats.timebase_last - lparstats_old.timebase_last;
  if (ticks == 0) {
    /* The stats have not been updated. Return now to avoid
     * dividing by zero */
    return 0;
  }

  metric_t tmpl = {0};
  metric_label_set(&tmpl, "serial", name.machine);

  /*
   * On a shared partition, we're "entitled" to a certain amount of
   * processing power, for example 250/100 of a physical CPU. Processing
   * capacity not used by the partition may be assigned to a different
   * partition by the hypervisor, so "idle" is hopefully a very small
   * number.
   *
   * A dedicated partition may donate its CPUs to another partition and
   * may steal ticks from somewhere else (another partition or maybe the
   * shared pool, I don't know --octo).
   */

  /* entitled_proc_capacity is in 1/100th of a CPU */
  double entitled_proc_capacity = 0.01 * ((double)lparstats.entitled_proc_capacity);
  metric_family_append(&fams[FAM_LPAR_ENTITLED], NULL, NULL,
                       (value_t){.gauge.float64 = entitled_proc_capacity }, &tmpl);

  /* The number of ticks actually spent in the various states */
  u_longlong_t user_ticks = lparstats.puser - lparstats_old.puser;
  u_longlong_t syst_ticks = lparstats.psys - lparstats_old.psys;
  u_longlong_t wait_ticks = lparstats.pwait - lparstats_old.pwait;
  u_longlong_t idle_ticks = lparstats.pidle - lparstats_old.pidle;
  u_longlong_t consumed_ticks = user_ticks + syst_ticks + wait_ticks + idle_ticks;

  metric_family_append(&fams[FAM_LPAR_USER], NULL, NULL,
                       (value_t){.gauge.float64 = (double)user_ticks / (double)ticks }, &tmpl);
  metric_family_append(&fams[FAM_LPAR_SYSTEM], NULL, NULL,
                       (value_t){.gauge.float64 = (double)syst_ticks / (double)ticks }, &tmpl);
  metric_family_append(&fams[FAM_LPAR_WAIT], NULL, NULL,
                       (value_t){.gauge.float64 = (double)wait_ticks / (double)ticks }, &tmpl);
  metric_family_append(&fams[FAM_LPAR_IDLE], NULL, NULL,
                       (value_t){.gauge.float64 = (double)idle_ticks / (double)ticks}, &tmpl);

#if PERFSTAT_SUPPORTS_DONATION
  if (donate_flag) {
    /* donated => ticks given to another partition
     * stolen  => ticks received from another partition */
    u_longlong_t idle_donated_ticks, busy_donated_ticks;
    u_longlong_t idle_stolen_ticks, busy_stolen_ticks;

    /* FYI:  PURR == Processor Utilization of Resources Register
     *      SPURR == Scaled PURR */
    idle_donated_ticks = lparstats.idle_donated_purr - lparstats_old.idle_donated_purr;
    busy_donated_ticks = lparstats.busy_donated_purr - lparstats_old.busy_donated_purr;
    idle_stolen_ticks = lparstats.idle_stolen_purr - lparstats_old.idle_stolen_purr;
    busy_stolen_ticks = lparstats.busy_stolen_purr - lparstats_old.busy_stolen_purr;

    metric_family_append(&fams[FAM_LPAR_IDLE_DONATED], NULL, NULL,
                         (value_t){.gauge.float64 = (double)idle_donated_ticks / (double)ticks }, &tmpl);
    metric_family_append(&fams[FAM_LPAR_BUSY_DONATED], NULL, NULL,
                         (value_t){.gauge.float64 = (double)busy_donated_ticks / (double)ticks }, &tmpl);
    metric_family_append(&fams[FAM_LPAR_IDLE_STOLEN], NULL, NULL,
                         (value_t){.gauge.float64 = (double)idle_stolen_ticks / (double)ticks }, &tmpl);
    metric_family_append(&fams[FAM_LPAR_BUSY_STOLEN], NULL, NULL,
                         (value_t){.gauge.float64 = (double)busy_stolen_ticks / (double)ticks }, &tmpl);

    /* Donated ticks will be accounted for as stolen ticks in other LPARs */
    consumed_ticks += idle_stolen_ticks + busy_stolen_ticks;
  }
#endif

  metric_family_append(&fams[FAM_LPAR_CONSUMED], NULL, NULL,
                       (value_t){.gauge.float64 = (double)consumed_ticks / (double)ticks) }, &tmpl);

  if (pool_stats) {
    /* We're calculating "busy" from "idle" and the total number of
     * CPUs, because the "busy" member didn't exist in early versions
     * of libperfstat. It was added somewhere between AIX 5.3 ML5 and ML9. */
    u_longlong_t pool_idle_cticks = lparstats.pool_idle_time - lparstats_old.pool_idle_time;
    double pool_idle_cpus = CLOCKTICKS_TO_TICKS((double)pool_idle_cticks) / (double)ticks;
    double pool_busy_cpus = ((double)lparstats.phys_cpus_pool) - pool_idle_cpus;
    if (pool_busy_cpus < 0.0)
      pool_busy_cpus = 0.0;

    char poolid[24];
    ssnprintf(poolid, sizeof(poolid), "%X", lparstats.pool_id);

    metric_family_append(&fams[FAM_LPAR_POOL_BUSY], "pool_id", poolid,
                         (value_t){.gauge.float64 = pool_busy_cpus }, &tmpl);
    metric_family_append(&fams[FAM_LPAR_POOL_IDLE], "pool_id", poolid,
                         (value_t){.gauge.float64 = pool_idle_cpus }, &tmpl);
  }

  memcpy(&lparstats_old, &lparstats, sizeof(lparstats_old));

  metric_reset(&tmpl);
  for (size_t i = 0; i < FAM_LPAR_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("lpar plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      }
      metric_family_metric_reset(&fams[i]);
    }
  }

  return 0;
}

void module_register(void)
{
  plugin_register_config("lpar", lpar_config, config_keys, config_keys_num);
  plugin_register_init("lpar", lpar_init);
  plugin_register_read("lpar", lpar_read);
}
