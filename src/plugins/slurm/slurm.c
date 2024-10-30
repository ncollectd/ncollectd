// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2018 Pablo Llopis
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Pablo Llopis <pablo.llopis at gmail.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include "plugin.h"
#include "libutils/common.h"

#include <slurm/slurm.h>
#include <slurm/slurm_errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define PART_NAME_SIZE 128

/* this function declaration is missing in slurm.h */
extern void slurm_free_stats_response_msg(stats_info_response_msg_t *msg);

enum {
    FAM_SLURM_SERVER_THREADS,
    FAM_SLURM_AGENT_QUEUE,
    FAM_SLURM_AGENTS,
    FAM_SLURM_AGENT_THREADS,
    FAM_SLURM_DBD_AGENT_QUEUE,
    FAM_SLURM_SCHEDULE_CYCLE_MAX_SECONDS,
    FAM_SLURM_SCHEDULE_CYCLE_LAST_SECONDS,
    FAM_SLURM_SCHEDULE_CYCLE_SUM_SECONDS,
    FAM_SLURM_SCHEDULE_CYCLES,
    FAM_SLURM_SCHEDULE_CYCLE_DEPTH,
    FAM_SLURM_SCHEDULE_QUEUE_LEN,
    FAM_SLURM_JOBS_SUBMITTED,
    FAM_SLURM_JOBS_STARTED,
    FAM_SLURM_JOBS_COMPLETED,
    FAM_SLURM_JOBS_CANCELED,
    FAM_SLURM_JOBS_FAILED,
    FAM_SLURM_JOBS_PENDING,
    FAM_SLURM_JOBS_RUNNING,
    FAM_SLURM_BACKFILLED_JOBS,
    FAM_SLURM_BACKFILLED_LAST_JOBS,
    FAM_SLURM_BACKFILLED_HETEROGENEOUS_JOBS,
    FAM_SLURM_BACKFILL_CYCLES,
    FAM_SLURM_BACKFILL_CYCLE_SECONDS,
    FAM_SLURM_BACKFILL_CYCLE_LAST_SECONDS,
    FAM_SLURM_BACKFILL_CYCLE_MAX_SECONDS,
    FAM_SLURM_BACKFILL_LAST_DEPTH,
    FAM_SLURM_BACKFILL_LAST_DEPTH_TRY,
    FAM_SLURM_BACKFILL_DEPTH_SUM,
    FAM_SLURM_BACKFILL_DEPTH_TRY_SUM,
    FAM_SLURM_BACKFILL_QUEUE_LEN,
    FAM_SLURM_BACKFILL_QUEUE_LEN_SUM,
    FAM_SLURM_BACKFILL_TABLE_SIZE,
    FAM_SLURM_BACKFILL_TABLE_SIZE_SUM,
    FAM_SLURM_JOB_STATE,
    FAM_SLURM_NODE_STATE,
    FAM_SLURM_MAX,
};

static metric_family_t fams[FAM_SLURM_MAX] = {
    [FAM_SLURM_SERVER_THREADS] = {
        .name = "slurm_server_threads",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of current active slurmctld threads.",
    },
    [FAM_SLURM_AGENT_QUEUE] = {
        .name = "slurm_agent_queue",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of enqueued outgoing RPC requests in an internal retry list.",
    },
    [FAM_SLURM_AGENTS] = {
        .name = "slurm_agents",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of agent thread.",
    },
    [FAM_SLURM_AGENT_THREADS] = {
        .name = "slurm_agent_threads",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total count of active threads created by all the agent threads.",
    },
    [FAM_SLURM_DBD_AGENT_QUEUE] = {
        .name = "slurm_dbd_agent_queue",
        .type = METRIC_TYPE_GAUGE,
        .help = "Slurm queues up the messages intended for the SlurmDBD and processes them in a "
                "separate thread.",
    },
    [FAM_SLURM_SCHEDULE_CYCLE_MAX_SECONDS] = {
        .name = "slurm_schedule_cycle_max_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Maximum time in seconds for any scheduling cycle since last reset.",
    },
    [FAM_SLURM_SCHEDULE_CYCLE_LAST_SECONDS] = {
        .name = "slurm_schedule_cycle_last_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Time in seconds for last scheduling cycle.",
    },
    [FAM_SLURM_SCHEDULE_CYCLE_SUM_SECONDS] =    {
        .name = "slurm_schedule_cycle_sum_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total time in seconds of scheduling cycles.",
    },
    [FAM_SLURM_SCHEDULE_CYCLES] =  {
        .name = "slurm_schedule_cycles",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of scheduling cycles since last reset."
    },
    [FAM_SLURM_SCHEDULE_CYCLE_DEPTH] =  {
        .name = "slurm_schedule_cycle_depth",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of jobs processed in the scheduling cycles.",
    },
    [FAM_SLURM_SCHEDULE_QUEUE_LEN] =    {
        .name = "slurm_schedule_queue_len",
        .type = METRIC_TYPE_GAUGE,
        .help = "Length of jobs pending queue."
    },
    [FAM_SLURM_JOBS_SUBMITTED] = {
        .name = "slurm_jobs_submitted",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of jobs submitted since last reset.",
    },
    [FAM_SLURM_JOBS_STARTED] = {
        .name = "slurm_jobs_started",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of jobs started since last reset. This includes backfilled jobs.",
    },
    [FAM_SLURM_JOBS_COMPLETED] = {
        .name = "slurm_jobs_completed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of jobs completed since last reset.",
    },
    [FAM_SLURM_JOBS_CANCELED] = {
        .name = "slurm_jobs_canceled",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of jobs canceled since last reset.",
    },
    [FAM_SLURM_JOBS_FAILED] = {
        .name = "slurm_jobs_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of jobs failed due to slurmd or other internal issues since last reset.",
    },
    [FAM_SLURM_JOBS_PENDING] = {
        .name = "slurm_jobs_pending",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of jobs pending at the given time.",
    },
    [FAM_SLURM_JOBS_RUNNING] = {
        .name = "slurm_jobs_running",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of jobs running at the given time.",
    },
    [FAM_SLURM_BACKFILLED_JOBS] = {
        .name = "slurm_backfilled_jobs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of jobs started thanks to backfilling since last slurm start.",
    },
    [FAM_SLURM_BACKFILLED_LAST_JOBS] = {
        .name = "slurm_backfilled_last_jobs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of jobs started thanks to backfilling since last time stats where reset.",
    },
    [FAM_SLURM_BACKFILLED_HETEROGENEOUS_JOBS] = {
        .name = "slurm_backfilled_heterogeneous_jobs",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of heterogeneous job components started thanks to backfilling since last "
                "Slurm start.",
    },
    [FAM_SLURM_BACKFILL_CYCLES] = {
        .name = "slurm_backfill_cycles",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of backfill scheduling cycles since last reset.",
    },
    [FAM_SLURM_BACKFILL_CYCLE_SECONDS] = {
        .name = "slurm_backfill_cycle_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Time in seconds    of backfilling scheduling cycles since last reset.",
    },
    [FAM_SLURM_BACKFILL_CYCLE_LAST_SECONDS] = {
        .name = "slurm_backfill_cycle_last_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Time in seconds of last backfill scheduling cycle.",
    },
    [FAM_SLURM_BACKFILL_CYCLE_MAX_SECONDS] = {
        .name = "slurm_backfill_cycle_max_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Time in seconds of maximum backfill scheduling cycle execution since last reset.",
    },
    [FAM_SLURM_BACKFILL_LAST_DEPTH] = {
        .name = "slurm_backfill_last_depth",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of processed jobs during last backfilling scheduling cycle. "
                "It counts every job even if that job can not be started due "
                "to dependencies or limits.",
    },
    [FAM_SLURM_BACKFILL_LAST_DEPTH_TRY] = {
        .name = "slurm_backfill_last_depth_try",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of processed jobs during last backfilling scheduling cycle. "
                "It counts only jobs with a chance to start using available resources.",
    },
    [FAM_SLURM_BACKFILL_DEPTH_SUM] = {
        .name = "slurm_backfill_depth_sum",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of processed jobs during backfilling scheduling cycles. "
                "It counts every job even if that job can not be started due "
                "to dependencies or limits.",
    },
    [FAM_SLURM_BACKFILL_DEPTH_TRY_SUM] = {
        .name = "slurm_backfill_depth_try_sum",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of processed jobs during backfilling scheduling cycles. "
                "It counts only jobs with a chance to start using available resources.",
    },
    [FAM_SLURM_BACKFILL_QUEUE_LEN] =    {
        .name = "slurm_backfill_queue_len",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of jobs pending to be processed by backfilling algorithm.",
    },
    [FAM_SLURM_BACKFILL_QUEUE_LEN_SUM] = {
        .name = "slurm_backfill_queue_len_sum",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of jobs pending to be processed by backfilling algorithm.",
    },
    [FAM_SLURM_BACKFILL_TABLE_SIZE] = {
        .name = "slurm_backfill_table_size",
        .type = METRIC_TYPE_GAUGE,
        .help = "Count of different time slots tested by the backfill scheduler "
                "in its last iteration.",
    },
    [FAM_SLURM_BACKFILL_TABLE_SIZE_SUM] = {
        .name = "slurm_backfill_table_size_sum",
        .type = METRIC_TYPE_COUNTER,
        .help = "Count of different time slots tested by the backfill scheduler.",
    },
    [FAM_SLURM_JOB_STATE] = {
        .name = "slurm_job_state",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_SLURM_NODE_STATE] = {
        .name = "slurm_node_state",
        .type = METRIC_TYPE_GAUGE,
    },
};

enum slurm_node_states {
    MAINT_NONRESP,
    MAINT,
    REBOOT_NONRESP,
    REBOOT,
    DRAINING_MAINT,
    DRAINING_REBOOT,
    DRAINING_POWERUP,
    DRAINING_POWERDOWN,
    DRAINING_NONRESP,
    DRAINING,
    DRAINED_MAINT,
    DRAINED_REBOOT,
    DRAINED_POWERUP,
    DRAINED_POWERDOWN,
    DRAINED_NONRESP,
    DRAINED,
    FAILING_NONRESP,
    FAILING,
    FAIL_NONRESP,
    FAIL,
    CANCEL_REBOOT,
    POWER_DOWN,
    POWER_UP,
    DOWN_MAINT,
    DOWN_REBOOT,
    DOWN_POWERUP,
    DOWN_POWERDOWN,
    DOWN_NONRESP,
    DOWN,
    ALLOCATED_MAINT,
    ALLOCATED_REBOOT,
    ALLOCATED_POWERUP,
    ALLOCATED_POWERDOWN,
    ALLOCATED_NONRESP,
    ALLOCATED_COMP,
    ALLOCATED,
    COMPLETING_MAINT,
    COMPLETING_REBOOT,
    COMPLETING_POWERUP,
    COMPLETING_POWERDOWN,
    COMPLETING_NONRESP,
    COMPLETING,
    IDLE_MAINT,
    IDLE_REBOOT,
    IDLE_POWERUP,
    IDLE_POWERDOWN,
    IDLE_NONRESP,
    PERFCTRS,
    RESERVED,
    IDLE,
    MIXED_MAINT,
    MIXED_REBOOT,
    MIXED_POWERUP,
    MIXED_POWERDOWN,
    MIXED_NONRESP,
    MIXED,
    FUTURE_MAINT,
    FUTURE_REBOOT,
    FUTURE_POWERUP,
    FUTURE_POWERDOWN,
    FUTURE_NONRESP,
    FUTURE,
    RESUME,
    UNKNOWN_NONRESP,
    UNKNOWN,
    UNKNOWN2
};

char *node_state_names[] = {
    [MAINT_NONRESP]        =  "MAINT_NONRESP",
    [MAINT]                =  "MAINT",
    [REBOOT_NONRESP]       =  "REBOOT_NONRESP",
    [REBOOT]               =  "REBOOT",
    [DRAINING_MAINT]       =  "DRAINING_MAINT",
    [DRAINING_REBOOT]      =  "DRAINING_REBOOT",
    [DRAINING_POWERUP]     =  "DRAINING_POWERUP",
    [DRAINING_POWERDOWN]   =  "DRAINING_POWERDOWN",
    [DRAINING_NONRESP]     =  "DRAINING_NONRESP",
    [DRAINING]             =  "DRAINING",
    [DRAINED_MAINT]        =  "DRAINED_MAINT",
    [DRAINED_REBOOT]       =  "DRAINED_REBOOT",
    [DRAINED_POWERUP]      =  "DRAINED_POWERUP",
    [DRAINED_POWERDOWN]    =  "DRAINED_POWERDOWN",
    [DRAINED_NONRESP]      =  "DRAINED_NONRESP",
    [DRAINED]              =  "DRAINED",
    [FAILING_NONRESP]      =  "FAILING_NONRESP",
    [FAILING]              =  "FAILING",
    [FAIL_NONRESP]         =  "FAIL_NONRESP",
    [FAIL]                 =  "FAIL",
    [CANCEL_REBOOT]        =  "CANCEL_REBOOT",
    [POWER_DOWN]           =  "POWER_DOWN",
    [POWER_UP]             =  "POWER_UP",
    [DOWN_MAINT]           =  "DOWN_MAINT",
    [DOWN_REBOOT]          =  "DOWN_REBOOT",
    [DOWN_POWERUP]         =  "DOWN_POWERUP",
    [DOWN_POWERDOWN]       =  "DOWN_POWERDOWN",
    [DOWN_NONRESP]         =  "DOWN_NONRESP",
    [DOWN]                 =  "DOWN",
    [ALLOCATED_MAINT]      =  "ALLOCATED_MAINT",
    [ALLOCATED_REBOOT]     =  "ALLOCATED_REBOOT",
    [ALLOCATED_POWERUP]    =  "ALLOCATED_POWERUP",
    [ALLOCATED_POWERDOWN]  =  "ALLOCATED_POWERDOWN",
    [ALLOCATED_NONRESP]    =  "ALLOCATED_NONRESP",
    [ALLOCATED_COMP]       =  "ALLOCATED_COMP",
    [ALLOCATED]            =  "ALLOCATED",
    [COMPLETING_MAINT]     =  "COMPLETING_MAINT",
    [COMPLETING_REBOOT]    =  "COMPLETING_REBOOT",
    [COMPLETING_POWERUP]   =  "COMPLETING_POWERUP",
    [COMPLETING_POWERDOWN] =  "COMPLETING_POWERDOWN",
    [COMPLETING_NONRESP]   =  "COMPLETING_NONRESP",
    [COMPLETING]           =  "COMPLETING",
    [IDLE_MAINT]           =  "IDLE_MAINT",
    [IDLE_REBOOT]          =  "IDLE_REBOOT",
    [IDLE_POWERUP]         =  "IDLE_POWERUP",
    [IDLE_POWERDOWN]       =  "IDLE_POWERDOWN",
    [IDLE_NONRESP]         =  "IDLE_NONRESP",
    [PERFCTRS]             =  "PERFCTRS",
    [RESERVED]             =  "RESERVED",
    [IDLE]                 =  "IDLE",
    [MIXED_MAINT]          =  "MIXED_MAINT",
    [MIXED_REBOOT]         =  "MIXED_REBOOT",
    [MIXED_POWERUP]        =  "MIXED_POWERUP",
    [MIXED_POWERDOWN]      =  "MIXED_POWERDOWN",
    [MIXED_NONRESP]        =  "MIXED_NONRESP",
    [MIXED]                =  "MIXED",
    [FUTURE_MAINT]         =  "FUTURE_MAINT",
    [FUTURE_REBOOT]        =  "FUTURE_REBOOT",
    [FUTURE_POWERUP]       =  "FUTURE_POWERUP",
    [FUTURE_POWERDOWN]     =  "FUTURE_POWERDOWN",
    [FUTURE_NONRESP]       =  "FUTURE_NONRESP",
    [FUTURE]               =  "FUTURE",
    [RESUME]               =  "RESUME",
    [UNKNOWN_NONRESP]      =  "UNKNOWN_NONRESP",
    [UNKNOWN]              =  "UNKNOWN",
    [UNKNOWN2]             =  "?"
};

/* based on src/common/slurm_protocol_defs.c node_state_string function */
static uint8_t slurm_node_state(uint32_t inx)
{
    int base = (inx & NODE_STATE_BASE);
    bool comp_flag = (inx & NODE_STATE_COMPLETING);
    bool drain_flag = (inx & NODE_STATE_DRAIN);
    bool fail_flag = (inx & NODE_STATE_FAIL);
    bool maint_flag = (inx & NODE_STATE_MAINT);
#if SLURM_VERSION_NUMBER < SLURM_VERSION_NUM(24,5,0)
    bool net_flag = (inx & NODE_STATE_NET);
#endif
#if SLURM_VERSION_NUMBER >= SLURM_VERSION_NUM(21,8,0)
    bool reboot_flag = (inx & NODE_STATE_REBOOT_ISSUED);
#else
    bool reboot_flag = (inx & NODE_STATE_REBOOT);
#endif
    bool res_flag = (inx & NODE_STATE_RES);
    bool resume_flag = (inx & NODE_RESUME);
    bool no_resp_flag = (inx & NODE_STATE_NO_RESPOND);
#if SLURM_VERSION_NUMBER >= SLURM_VERSION_NUM(21,8,0)
    bool power_down_flag = (inx & NODE_STATE_POWERED_DOWN);
#else
    bool power_down_flag = (inx & NODE_STATE_POWER_SAVE);
#endif
    bool power_up_flag = (inx & NODE_STATE_POWER_UP);

    if (maint_flag) {
        if (drain_flag || (base == NODE_STATE_ALLOCATED) ||
            (base == NODE_STATE_DOWN) || (base == NODE_STATE_MIXED))
            ;
        else if (no_resp_flag)
            return MAINT_NONRESP;
        else
            return MAINT;
    }
    if (reboot_flag) {
        if ((base == NODE_STATE_ALLOCATED) || (base == NODE_STATE_MIXED))
            ;
        else if (no_resp_flag)
            return REBOOT_NONRESP;
        else
            return REBOOT;
    }
    if (drain_flag) {
        if (comp_flag || (base == NODE_STATE_ALLOCATED) || (base == NODE_STATE_MIXED)) {
            if (maint_flag)
                return DRAINING_MAINT;
            if (reboot_flag)
                return DRAINING_REBOOT;
            if (power_up_flag)
                return DRAINING_POWERUP;
            if (power_down_flag)
                return DRAINING_POWERDOWN;
            if (no_resp_flag)
                return DRAINING_NONRESP;
            return DRAINING;
        } else {
            if (maint_flag)
                return DRAINED_MAINT;
            if (reboot_flag)
                return DRAINED_REBOOT;
            if (power_up_flag)
                return DRAINED_POWERUP;
            if (power_down_flag)
                return DRAINED_POWERDOWN;
            if (no_resp_flag)
                return DRAINED_NONRESP;
            return DRAINED;
        }
    }
    if (fail_flag) {
        if (comp_flag || (base == NODE_STATE_ALLOCATED)) {
            if (no_resp_flag)
                return FAILING_NONRESP;
            return FAILING;
        } else {
            if (no_resp_flag)
                return FAIL_NONRESP;
            return FAIL;
        }
    }

#if SLURM_VERSION_NUMBER >= SLURM_VERSION_NUM(21,8,0)
    if (inx == NODE_STATE_REBOOT_CANCEL)
#else
    if (inx == NODE_STATE_CANCEL_REBOOT)
#endif
        return CANCEL_REBOOT;
#if SLURM_VERSION_NUMBER >= SLURM_VERSION_NUM(21,8,0)
    if (inx == NODE_STATE_POWERED_DOWN)
#else
    if (inx == NODE_STATE_POWER_SAVE)
#endif
        return POWER_DOWN;
    if (inx == NODE_STATE_POWER_UP)
        return POWER_UP;
    if (base == NODE_STATE_DOWN) {
        if (maint_flag)
            return DOWN_MAINT;
        if (reboot_flag)
            return DOWN_REBOOT;
        if (power_up_flag)
            return DOWN_POWERUP;
        if (power_down_flag)
            return DOWN_POWERDOWN;
        if (no_resp_flag)
            return DOWN_NONRESP;
        return DOWN;
    }

    if (base == NODE_STATE_ALLOCATED) {
        if (maint_flag)
            return ALLOCATED_MAINT;
        if (reboot_flag)
            return ALLOCATED_REBOOT;
        if (power_up_flag)
            return ALLOCATED_POWERUP;
        if (power_down_flag)
            return ALLOCATED_POWERDOWN;
        if (no_resp_flag)
            return ALLOCATED_NONRESP;
        if (comp_flag)
            return ALLOCATED_COMP;
        return ALLOCATED;
    }
    if (comp_flag) {
        if (maint_flag)
            return COMPLETING_MAINT;
        if (reboot_flag)
            return COMPLETING_REBOOT;
        if (power_up_flag)
            return COMPLETING_POWERUP;
        if (power_down_flag)
            return COMPLETING_POWERDOWN;
        if (no_resp_flag)
            return COMPLETING_NONRESP;
        return COMPLETING;
    }
    if (base == NODE_STATE_IDLE) {
        if (maint_flag)
            return IDLE_MAINT;
        if (reboot_flag)
            return IDLE_REBOOT;
        if (power_up_flag)
            return IDLE_POWERUP;
        if (power_down_flag)
            return IDLE_POWERDOWN;
        if (no_resp_flag)
            return IDLE_NONRESP;
#if SLURM_VERSION_NUMBER < SLURM_VERSION_NUM(24,5,0)
        if (net_flag)
            return PERFCTRS;
#endif
        if (res_flag)
            return RESERVED;
        return IDLE;
    }
    if (base == NODE_STATE_MIXED) {
        if (maint_flag)
            return MIXED_MAINT;
        if (reboot_flag)
            return MIXED_REBOOT;
        if (power_up_flag)
            return MIXED_POWERUP;
        if (power_down_flag)
            return MIXED_POWERDOWN;
        if (no_resp_flag)
            return MIXED_NONRESP;
        return MIXED;
    }
    if (base == NODE_STATE_FUTURE) {
        if (maint_flag)
            return FUTURE_MAINT;
        if (reboot_flag)
            return FUTURE_REBOOT;
        if (power_up_flag)
            return FUTURE_POWERUP;
        if (power_down_flag)
            return FUTURE_POWERDOWN;
        if (no_resp_flag)
            return FUTURE_NONRESP;
        return FUTURE;
    }
    if (resume_flag)
        return RESUME;
    if (base == NODE_STATE_UNKNOWN) {
        if (no_resp_flag)
            return UNKNOWN_NONRESP;
        return UNKNOWN;
    }
    return UNKNOWN2;
}

#define NUM_NODE_STATES (sizeof(node_state_names) / sizeof(node_state_names[0]))

typedef struct partition_state_st {
    char name[PART_NAME_SIZE];
    uint32_t nodes_states_count[NUM_NODE_STATES];
    /* counts jobs states indexed by enum job_states in slurm.h */
    uint32_t jobs_states_count[JOB_END];
} partition_state_t;

/* based on enum job_states from slurm.h */
static const char *job_state_names[JOB_END] = {
    [JOB_PENDING]   = "pending",
    [JOB_RUNNING]   = "running",
    [JOB_SUSPENDED] = "suspended",
    [JOB_COMPLETE]  = "complete",
    [JOB_CANCELLED] = "cancelled",
    [JOB_FAILED]    = "failed",
    [JOB_TIMEOUT]   = "timeout",
    [JOB_NODE_FAIL] = "node_fail",
    [JOB_PREEMPTED] = "preempted",
    [JOB_BOOT_FAIL] = "boot_fail",
    [JOB_DEADLINE]  = "deadline",
    [JOB_OOM]       = "oom",
};

static partition_state_t *alloc_partition_states(uint32_t num_partitions,
                                                 partition_info_t *partitions)
{
    partition_state_t *partition_states;

    partition_states = (partition_state_t *)calloc(num_partitions, sizeof(partition_state_t));
    if (!partition_states) {
        return NULL;
    }

    for (uint32_t i = 0; i < num_partitions; i++)
        sstrncpy(partition_states[i].name, partitions[i].name, PART_NAME_SIZE);

    return partition_states;
}

static partition_state_t *find_partition(partition_state_t *partitions, uint32_t num_partitions,
                                         char *name)
{
    partition_state_t *part = NULL;

    for (uint32_t i = 0; i < num_partitions; i++) {
        if (strncmp(name, partitions[i].name, PART_NAME_SIZE) == 0)
            part = &partitions[i];
    }

    return part;
}

static void slurm_submit_partition(partition_state_t *partition)
{
    for (int i = 0; i < JOB_END; i++) {
        metric_family_append(&fams[FAM_SLURM_JOB_STATE],
                             VALUE_GAUGE(partition->jobs_states_count[i]), NULL,
                             &(label_pair_const_t){.name="partition", .value=partition->name},
                             &(label_pair_const_t){.name="state", .value=job_state_names[i]},
                             NULL);
    }
    for (size_t i = 0; i < NUM_NODE_STATES; i++) {
        metric_family_append(&fams[FAM_SLURM_NODE_STATE],
                             VALUE_GAUGE(partition->nodes_states_count[i]), NULL,
                             &(label_pair_const_t){.name="partition", .value=partition->name},
                             &(label_pair_const_t){.name="state", .value=node_state_names[i]},
                             NULL);
    }
}

static void slurm_submit_stats(stats_info_response_msg_t *stats_resp)
{
    metric_family_append(&fams[FAM_SLURM_SERVER_THREADS],
                         VALUE_GAUGE(stats_resp->server_thread_count), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_AGENT_QUEUE],
                         VALUE_GAUGE(stats_resp->agent_queue_size), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_AGENTS],
                         VALUE_GAUGE(stats_resp->agent_count), NULL, NULL);
#if SLURM_VERSION_NUMBER >= SLURM_VERSION_NUM(20,2,0)
    metric_family_append(&fams[FAM_SLURM_AGENT_THREADS],
                         VALUE_GAUGE(stats_resp->agent_thread_count), NULL, NULL);
#endif
    metric_family_append(&fams[FAM_SLURM_DBD_AGENT_QUEUE],
                         VALUE_GAUGE(stats_resp->dbd_agent_queue_size), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_SCHEDULE_CYCLE_MAX_SECONDS],
                         VALUE_GAUGE((double)stats_resp->schedule_cycle_max/100000.0),
                         NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_SCHEDULE_CYCLE_LAST_SECONDS],
                         VALUE_GAUGE((double)stats_resp->schedule_cycle_last/100000.0),
                         NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_SCHEDULE_CYCLE_SUM_SECONDS],
                         VALUE_COUNTER_FLOAT64((double)stats_resp->schedule_cycle_sum/100000.0),
                         NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_SCHEDULE_CYCLES],
                         VALUE_COUNTER(stats_resp->schedule_cycle_counter), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_SCHEDULE_CYCLE_DEPTH],
                         VALUE_COUNTER(stats_resp->schedule_cycle_depth), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_SCHEDULE_QUEUE_LEN],
                         VALUE_GAUGE(stats_resp->schedule_queue_len), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_JOBS_SUBMITTED],
                         VALUE_COUNTER(stats_resp->jobs_submitted), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_JOBS_STARTED],
                         VALUE_COUNTER(stats_resp->jobs_started), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_JOBS_COMPLETED],
                         VALUE_COUNTER(stats_resp->jobs_completed), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_JOBS_CANCELED],
                         VALUE_COUNTER(stats_resp->jobs_canceled), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_JOBS_FAILED],
                         VALUE_COUNTER(stats_resp->jobs_failed), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_JOBS_PENDING],
                         VALUE_GAUGE(stats_resp->jobs_pending), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_JOBS_RUNNING],
                         VALUE_GAUGE(stats_resp->jobs_running), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_BACKFILLED_JOBS],
                         VALUE_COUNTER(stats_resp->bf_backfilled_jobs), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_BACKFILLED_LAST_JOBS],
                         VALUE_COUNTER(stats_resp->bf_last_backfilled_jobs), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_BACKFILLED_HETEROGENEOUS_JOBS],
#if SLURM_VERSION_NUMBER >= SLURM_VERSION_NUM(20,2,0)
                         VALUE_COUNTER(stats_resp->bf_backfilled_het_jobs), NULL, NULL);
#else
                         VALUE_COUNTER(stats_resp->bf_backfilled_pack_jobs), NULL, NULL);
#endif
    metric_family_append(&fams[FAM_SLURM_BACKFILL_CYCLES],
                         VALUE_COUNTER(stats_resp->bf_cycle_counter), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_BACKFILL_CYCLE_SECONDS],
                         VALUE_COUNTER_FLOAT64((double)stats_resp->bf_cycle_sum/100000.0),
                         NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_BACKFILL_CYCLE_LAST_SECONDS],
                         VALUE_GAUGE((double)stats_resp->bf_cycle_last/100000.0),
                         NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_BACKFILL_CYCLE_MAX_SECONDS],
                         VALUE_GAUGE((double)stats_resp->bf_cycle_max/100000.0),
                         NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_BACKFILL_LAST_DEPTH],
                         VALUE_GAUGE(stats_resp->bf_last_depth), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_BACKFILL_LAST_DEPTH_TRY],
                         VALUE_GAUGE(stats_resp->bf_last_depth_try), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_BACKFILL_DEPTH_SUM],
                         VALUE_COUNTER(stats_resp->bf_depth_sum), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_BACKFILL_DEPTH_TRY_SUM],
                         VALUE_COUNTER(stats_resp->bf_depth_try_sum), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_BACKFILL_QUEUE_LEN],
                         VALUE_GAUGE(stats_resp->bf_queue_len), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_BACKFILL_QUEUE_LEN_SUM],
                         VALUE_COUNTER(stats_resp->bf_queue_len_sum), NULL, NULL);
#if SLURM_VERSION_NUMBER >= SLURM_VERSION_NUM(20,2,0)
    metric_family_append(&fams[FAM_SLURM_BACKFILL_TABLE_SIZE],
                         VALUE_GAUGE(stats_resp->bf_table_size), NULL, NULL);
    metric_family_append(&fams[FAM_SLURM_BACKFILL_TABLE_SIZE_SUM],
                         VALUE_COUNTER(stats_resp->bf_table_size_sum), NULL, NULL);
#endif
}

static int slurm_read(void)
{
    job_info_msg_t *job_buffer_ptr = NULL;
    if (slurm_load_jobs((time_t)NULL, &job_buffer_ptr, SHOW_ALL)) {
        PLUGIN_ERROR("slurm_load_jobs error");
        return -1;
    }

    node_info_msg_t *node_buffer_ptr = NULL;
    if (slurm_load_node((time_t)NULL, &node_buffer_ptr, SHOW_ALL)) {
        slurm_free_job_info_msg(job_buffer_ptr);
        PLUGIN_ERROR("slurm_load_node error");
        return -1;
    }

    partition_info_msg_t *part_buffer_ptr = NULL;
    if (slurm_load_partitions((time_t)NULL, &part_buffer_ptr, 0)) {
        slurm_free_job_info_msg(job_buffer_ptr);
        slurm_free_node_info_msg(node_buffer_ptr);
        PLUGIN_ERROR("slurm_load_partitions error");
        return -1;
    }

    stats_info_response_msg_t *stats_resp;
    stats_info_request_msg_t stats_req;
    stats_req.command_id = STAT_COMMAND_GET;
    if (slurm_get_statistics(&stats_resp, &stats_req)) {
        slurm_free_job_info_msg(job_buffer_ptr);
        slurm_free_node_info_msg(node_buffer_ptr);
        slurm_free_partition_info_msg(part_buffer_ptr);
        PLUGIN_ERROR("slurm_get_statistics error");
    }

    /* SLURM APIs provide *non-relational* data about nodes, partitions and jobs.
     * We allocate a data structure that relates all three together, and the following
     * two for loops fill this data structure. The data structure is an array
     * of partition_state_t that holds job and node states. */
    uint32_t num_partitions = part_buffer_ptr->record_count;
    partition_state_t *partition_states = alloc_partition_states(num_partitions,
                                                                 part_buffer_ptr->partition_array);
    if (!partition_states) {
        slurm_free_job_info_msg(job_buffer_ptr);
        slurm_free_node_info_msg(node_buffer_ptr);
        slurm_free_partition_info_msg(part_buffer_ptr);
        PLUGIN_ERROR("alloc_partition_states");
        return -1;
    }

    /* fill partition_states array with per-partition job state information */
    for (uint32_t i = 0; i < job_buffer_ptr->record_count; i++) {
        job_info_t *job_ptr = &job_buffer_ptr->job_array[i];
        partition_state_t *partition_state = find_partition(partition_states, num_partitions,
                                                                              job_ptr->partition);
        if (!partition_state) {
            PLUGIN_ERROR("slurm_read: cannot find partition %s from jobid %u"
                          "in partition list returned by slurm_load_partitions",
                          job_ptr->partition, job_ptr->job_id);
            continue;
        }

        uint8_t job_state = job_ptr->job_state & JOB_STATE_BASE;
        partition_state->jobs_states_count[job_state]++;
    }

    /* fill partition_states array with per-partition node state information */
    for (uint32_t i = 0; i < part_buffer_ptr->record_count; i++) {
        partition_info_t *part_ptr = &part_buffer_ptr->partition_array[i];

        partition_state_t *partition_state = find_partition(partition_states, num_partitions,
                                                                              part_ptr->name);
        if (!partition_state) {
            PLUGIN_ERROR("slurm_read: cannot find partition %s"
                         " in partition list returned by slurm_load_partitions", part_ptr->name);
            continue;
        }

        for (int j = 0; part_ptr->node_inx; j += 2) {
            if (part_ptr->node_inx[j] == -1)
                break;
            for (int k = part_ptr->node_inx[j]; k <= part_ptr->node_inx[j + 1]; k++) {
                node_info_t *node_ptr = &node_buffer_ptr->node_array[k];
                /* some non-existant nodes (name is NULL) may show up as node_state FUTURE */
                uint8_t node_state = slurm_node_state(node_ptr->node_state);
                partition_state->nodes_states_count[node_state]++;
            }
        }
    }

    for (uint32_t i = 0; i < num_partitions; i++)
        slurm_submit_partition(&partition_states[i]);

    slurm_submit_stats(stats_resp);

    plugin_dispatch_metric_family_array(fams, FAM_SLURM_MAX, 0);

    slurm_free_job_info_msg(job_buffer_ptr);
    slurm_free_node_info_msg(node_buffer_ptr);
    slurm_free_partition_info_msg(part_buffer_ptr);
    slurm_free_stats_response_msg(stats_resp);
    free(partition_states);
    return 0;
}

void module_register(void)
{
    plugin_register_read("slurm", slurm_read);
}
