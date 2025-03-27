// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libxson/json_parse.h"

#include <curl/curl.h>

enum {
    FAM_PODMAN_CONTAINER,
    FAM_PODMAN_CONTAINER_STATE,
    FAM_PODMAN_CONTAINER_CREATED_SECONDS,
    FAM_PODMAN_CONTAINER_STARTED_SECONDS,
    FAM_PODMAN_CONTAINER_EXIT_CODE,
    FAM_PODMAN_CONTAINER_EXITED_SECONDS,
    FAM_PODMAN_CONTAINER_PID,
    FAM_PODMAN_CONTAINER_BLOCK_INPUT,
    FAM_PODMAN_CONTAINER_BLOCK_OUTPUT,
    FAM_PODMAN_CONTAINER_CPU_SECONDS,
    FAM_PODMAN_CONTAINER_CPU_SYSTEM_SECONDS,
    FAM_PODMAN_CONTAINER_MEM_LIMIT_BYTES,
    FAM_PODMAN_CONTAINER_MEM_USAGE_BYTES,
    FAM_PODMAN_CONTAINER_NET_INPUT,
    FAM_PODMAN_CONTAINER_NET_OUTPUT,
    FAM_PODMAN_MAX,
};

static metric_family_t fams_podman[FAM_PODMAN_MAX] = {
    [FAM_PODMAN_CONTAINER] = {
        .name = "podman_container",
        .type = METRIC_TYPE_INFO,
        .help = "Container information.",
    },
    [FAM_PODMAN_CONTAINER_STATE] = {
        .name = "podman_container_state",
        .type = METRIC_TYPE_STATE_SET,
        .help = "Container current state",
    },
    [FAM_PODMAN_CONTAINER_BLOCK_INPUT] = {
        .name = "podman_container_block_input",
        .type = METRIC_TYPE_COUNTER,
        .help = "Container block input.",
    },
    [FAM_PODMAN_CONTAINER_BLOCK_OUTPUT] = {
        .name = "podman_container_block_output",
        .type = METRIC_TYPE_COUNTER,
        .help = "Container block output.",
    },
    [FAM_PODMAN_CONTAINER_CPU_SECONDS] = {
        .name = "podman_container_cpu_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total CPU time spent for container in seconds.",
    },
    [FAM_PODMAN_CONTAINER_CPU_SYSTEM_SECONDS] = {
        .name = "podman_container_cpu_system_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total system CPU time spent for container in seconds."
    },
    [FAM_PODMAN_CONTAINER_CREATED_SECONDS] = {
        .name = "podman_container_created_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Container creation time in unixtime.",
    },
    [FAM_PODMAN_CONTAINER_STARTED_SECONDS] = {
        .name = "podman_container_started_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Container started time in unixtime.",
    },
    [FAM_PODMAN_CONTAINER_EXIT_CODE] = {
        .name = "podman_container_exit_code",
        .type = METRIC_TYPE_GAUGE,
        .help = "Container exit code, if the container has not exited or restarted "
                "then the exit code will be 0.",
    },
    [FAM_PODMAN_CONTAINER_EXITED_SECONDS] = {
        .name = "podman_container_exited_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Container exited time in unixtime.",
    },
    [FAM_PODMAN_CONTAINER_MEM_LIMIT_BYTES] = {
        .name = "podman_container_mem_limit_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Container memory limit.",
    },
    [FAM_PODMAN_CONTAINER_MEM_USAGE_BYTES] = {
        .name = "podman_container_mem_usage_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Container memory usage.",
    },
    [FAM_PODMAN_CONTAINER_NET_INPUT] = {
        .name = "podman_container_net_input",
        .type = METRIC_TYPE_COUNTER,
        .help = "Container network input.",
    },
    [FAM_PODMAN_CONTAINER_NET_OUTPUT] = {
        .name = "podman_container_net_output",
        .type = METRIC_TYPE_COUNTER,
        .help = "Container network output.",
    },
    [FAM_PODMAN_CONTAINER_PID] = {
        .name = "podman_container_pid",
        .type = METRIC_TYPE_GAUGE,
        .help = "Container pid number.",
    }
};

typedef struct {
    char id[13];
    uint64_t pids;
    uint64_t cpu;
    uint64_t cpu_system;
    uint64_t mem_usage;
    uint64_t mem_limit;
    uint64_t block_input;
    uint64_t block_output;
    uint64_t net_input;
    uint64_t net_output;
} podman_container_stats_t;

typedef enum {
    PODMAN_CONTAINER_STATS_JSON_NONE,
    PODMAN_CONTAINER_STATS_JSON_STATS,
    PODMAN_CONTAINER_STATS_JSON_STATS_PIDS,
    PODMAN_CONTAINER_STATS_JSON_STATS_CPU_NANO,
    PODMAN_CONTAINER_STATS_JSON_STATS_MEM_USAGE,
    PODMAN_CONTAINER_STATS_JSON_STATS_MEM_LIMIT,
    PODMAN_CONTAINER_STATS_JSON_STATS_NET_INPUT,
    PODMAN_CONTAINER_STATS_JSON_STATS_NET_OUTPUT,
    PODMAN_CONTAINER_STATS_JSON_STATS_BLOCK_INPUT,
    PODMAN_CONTAINER_STATS_JSON_STATS_BLOCK_OUTPUT,
    PODMAN_CONTAINER_STATS_JSON_STATS_CONTAINER_ID,
    PODMAN_CONTAINER_STATS_JSON_STATS_CPU_SYSTEM_NANO
} podman_container_stats_json_t;

typedef struct {
    podman_container_stats_json_t stack[JSON_MAX_DEPTH];
    size_t depth;
    podman_container_stats_t stats;
    metric_family_t *fams;
    label_set_t *labels;
} podman_container_stats_json_ctx_t;

typedef struct {
    char id[13];
    char image[512];
    char name[256];
    char state[16];
    uint64_t pid;
    uint64_t created;
    uint64_t started;
    uint64_t exit_at;
    uint64_t exit_code;
} podman_container_info_t;

typedef enum {
    PODMAN_CONTAINER_INFO_JSON_NONE,
    PODMAN_CONTAINER_INFO_JSON_ID,
    PODMAN_CONTAINER_INFO_JSON_PID,
    PODMAN_CONTAINER_INFO_JSON_IMAGE,
    PODMAN_CONTAINER_INFO_JSON_NAME,
    PODMAN_CONTAINER_INFO_JSON_STATE,
    PODMAN_CONTAINER_INFO_JSON_CREATED,
    PODMAN_CONTAINER_INFO_JSON_EXITED_AT,
    PODMAN_CONTAINER_INFO_JSON_EXIT_CODE,
    PODMAN_CONTAINER_INFO_JSON_STARTED_AT,
} podman_container_info_json_t;

typedef struct {
    podman_container_info_json_t stack[JSON_MAX_DEPTH];
    size_t depth;
    podman_container_info_t info;
    metric_family_t *fams;
    label_set_t *labels;
} podman_container_info_json_ctx_t;

typedef struct {
    char *instance;
    char *url;
    char *url_stats;
    char *url_info;
    int timeout;
    label_set_t labels;
    plugin_filter_t *filter;
    CURL *curl;
    char curl_error[CURL_ERROR_SIZE];
    metric_family_t fams[FAM_PODMAN_MAX];
    json_parser_t handle;
} podman_instance_t;

static bool podman_container_stats_json_string(void *ctx, const char *string, size_t string_len)
{
    podman_container_stats_json_ctx_t *sctx = ctx;

    if ((sctx->depth == 2) && (sctx->stack[1] == PODMAN_CONTAINER_STATS_JSON_STATS_CONTAINER_ID))
        sstrnncpy(sctx->stats.id, sizeof(sctx->stats.id), string, string_len);

    return true;
}

static bool podman_container_stats_json_number(void *ctx, const char *number_val,
                                                          size_t number_len)
{
    podman_container_stats_json_ctx_t *sctx = ctx;

    char number[64];
    sstrnncpy(number, sizeof(number), number_val, number_len);

    switch (sctx->depth) {
    case 2:
        switch (sctx->stack[1]) {
        case PODMAN_CONTAINER_STATS_JSON_STATS_PIDS:
            strtouint(number, &sctx->stats.pids);
            break;
        case PODMAN_CONTAINER_STATS_JSON_STATS_CPU_NANO:
            strtouint(number, &sctx->stats.cpu);
            break;
        case PODMAN_CONTAINER_STATS_JSON_STATS_CPU_SYSTEM_NANO:
            strtouint(number, &sctx->stats.cpu_system);
            break;
        case PODMAN_CONTAINER_STATS_JSON_STATS_MEM_USAGE:
            strtouint(number, &sctx->stats.mem_usage);
            break;
        case PODMAN_CONTAINER_STATS_JSON_STATS_MEM_LIMIT:
            strtouint(number, &sctx->stats.mem_limit);
            break;
        case PODMAN_CONTAINER_STATS_JSON_STATS_NET_INPUT:
            strtouint(number, &sctx->stats.net_input);
            break;
        case PODMAN_CONTAINER_STATS_JSON_STATS_NET_OUTPUT:
            strtouint(number, &sctx->stats.net_output);
            break;
        case PODMAN_CONTAINER_STATS_JSON_STATS_BLOCK_INPUT:
            strtouint(number, &sctx->stats.block_input);
            break;
        case PODMAN_CONTAINER_STATS_JSON_STATS_BLOCK_OUTPUT:
            strtouint(number, &sctx->stats.block_output);
            break;
        default:
            break;
        }
        break;
    }

    return true;
}

static bool podman_container_stats_json_start_map(void *ctx)
{
    podman_container_stats_json_ctx_t *sctx = ctx;
    sctx->depth++;
    sctx->stack[sctx->depth] = PODMAN_CONTAINER_STATS_JSON_NONE;

    if ((sctx->depth == 2) && (sctx->stack[0] == PODMAN_CONTAINER_STATS_JSON_STATS))
        memset(&sctx->stats, 0, sizeof(sctx->stats));

    return true;
}

static bool podman_container_stats_json_map_key(void * ctx, const char * key, size_t key_len)
{
    podman_container_stats_json_ctx_t *sctx = ctx;

    switch (sctx->depth) {
    case 1:
        if ((key_len == 5) && (strncmp("stats", key, key_len) == 0)) {
            sctx->stack[0] = PODMAN_CONTAINER_STATS_JSON_STATS;
            return 1;
        }
        sctx->stack[0] = PODMAN_CONTAINER_STATS_JSON_NONE;
        break;
    case 2:
        switch (sctx->stack[0]) {
        case PODMAN_CONTAINER_STATS_JSON_STATS:
            switch(key_len) {
            case 4:
                if (strncmp("PIDs", key, key_len) == 0) {
                    sctx->stack[1] = PODMAN_CONTAINER_STATS_JSON_STATS_PIDS;
                    return 1;
                }
                break;
            case 7:
                if (strncmp("CPUNano", key, key_len) == 0) {
                    sctx->stack[1] = PODMAN_CONTAINER_STATS_JSON_STATS_CPU_NANO;
                    return 1;
                }
                break;
            case 8:
                if (strncmp("MemUsage", key, key_len) == 0) {
                    sctx->stack[1] = PODMAN_CONTAINER_STATS_JSON_STATS_MEM_USAGE;
                    return 1;
                } else if (strncmp("MemLimit", key, key_len) == 0) {
                    sctx->stack[1] = PODMAN_CONTAINER_STATS_JSON_STATS_MEM_LIMIT;
                    return 1;
                } else if (strncmp("NetInput", key, key_len) == 0) {
                    sctx->stack[1] = PODMAN_CONTAINER_STATS_JSON_STATS_NET_INPUT;
                    return 1;
                }
                break;
            case 9:
                if (strncmp("NetOutput", key, key_len) == 0) {
                    sctx->stack[1] = PODMAN_CONTAINER_STATS_JSON_STATS_NET_OUTPUT;
                    return 1;
                }
                break;
            case 10:
                if (strncmp("BlockInput", key, key_len) == 0) {
                    sctx->stack[1] = PODMAN_CONTAINER_STATS_JSON_STATS_BLOCK_INPUT;
                    return 1;
                }
                break;
            case 11:
                if (strncmp("BlockOutput", key, key_len) == 0) {
                    sctx->stack[1] = PODMAN_CONTAINER_STATS_JSON_STATS_BLOCK_OUTPUT;
                    return 1;
                } else if (strncmp("ContainerID", key, key_len) == 0) {
                    sctx->stack[1] = PODMAN_CONTAINER_STATS_JSON_STATS_CONTAINER_ID;
                    return 1;
                }
                break;
            case 13:
                if (strncmp("CPUSystemNano", key, key_len) == 0) {
                    sctx->stack[1] = PODMAN_CONTAINER_STATS_JSON_STATS_CPU_SYSTEM_NANO;
                    return 1;
                }
                break;
            }
            sctx->stack[1] = PODMAN_CONTAINER_STATS_JSON_NONE;
            break;
        default:
            break;
        }
        break;
    }

    return true;
}

static bool podman_container_stats_json_end_map(void *ctx)
{
    podman_container_stats_json_ctx_t *sctx = ctx;

    if ((sctx->depth == 2) && (sctx->stack[0] == PODMAN_CONTAINER_STATS_JSON_STATS)) {
        if ((sctx->stats.id[0] != '\0') && (sctx->stats.pids > 0)) {
            metric_family_append(&sctx->fams[FAM_PODMAN_CONTAINER_CPU_SECONDS],
                                 VALUE_COUNTER_FLOAT64((double)sctx->stats.cpu / 1000000000.0),
                                 sctx->labels,
                                 &(label_pair_const_t){.name="id", .value=sctx->stats.id}, NULL);
            metric_family_append(&sctx->fams[FAM_PODMAN_CONTAINER_CPU_SYSTEM_SECONDS],
                                 VALUE_COUNTER_FLOAT64((double)sctx->stats.cpu_system / 1000000000.0),
                                 sctx->labels,
                                 &(label_pair_const_t){.name="id", .value=sctx->stats.id}, NULL);
            metric_family_append(&sctx->fams[FAM_PODMAN_CONTAINER_MEM_USAGE_BYTES],
                                 VALUE_GAUGE(sctx->stats.mem_usage), sctx->labels,
                                 &(label_pair_const_t){.name="id", .value=sctx->stats.id}, NULL);
            metric_family_append(&sctx->fams[FAM_PODMAN_CONTAINER_MEM_LIMIT_BYTES],
                                 VALUE_GAUGE(sctx->stats.mem_limit), sctx->labels,
                                 &(label_pair_const_t){.name="id", .value=sctx->stats.id}, NULL);
            metric_family_append(&sctx->fams[FAM_PODMAN_CONTAINER_BLOCK_INPUT],
                                 VALUE_COUNTER(sctx->stats.block_input), sctx->labels,
                                 &(label_pair_const_t){.name="id", .value=sctx->stats.id}, NULL);
            metric_family_append(&sctx->fams[FAM_PODMAN_CONTAINER_BLOCK_OUTPUT],
                                 VALUE_COUNTER(sctx->stats.block_output), sctx->labels,
                                 &(label_pair_const_t){.name="id", .value=sctx->stats.id}, NULL);
            metric_family_append(&sctx->fams[FAM_PODMAN_CONTAINER_NET_INPUT],
                                 VALUE_COUNTER(sctx->stats.net_input), sctx->labels,
                                 &(label_pair_const_t){.name="id", .value=sctx->stats.id}, NULL);
            metric_family_append(&sctx->fams[FAM_PODMAN_CONTAINER_NET_OUTPUT],
                                 VALUE_COUNTER(sctx->stats.net_output), sctx->labels,
                                 &(label_pair_const_t){.name="id", .value=sctx->stats.id}, NULL);

        }
    }

    sctx->depth--;
    if (sctx->depth > 0) {
        sctx->stack[sctx->depth-1] = PODMAN_CONTAINER_STATS_JSON_NONE;
    } else {
        sctx->depth = 0;
    }

    return true;
}

static xson_callbacks_t podman_container_stats_json_callbacks = {
    .xson_null        = NULL,
    .xson_boolean     = NULL,
    .xson_integer     = NULL,
    .xson_double      = NULL,
    .xson_number      = podman_container_stats_json_number,
    .xson_string      = podman_container_stats_json_string,
    .xson_start_map   = podman_container_stats_json_start_map,
    .xson_map_key     = podman_container_stats_json_map_key,
    .xson_end_map     = podman_container_stats_json_end_map,
    .xson_start_array = NULL,
    .xson_end_array   = NULL,
};

static bool podman_container_info_json_string(void *ctx, const char *string, size_t string_len)
{
    podman_container_info_json_ctx_t *sctx = ctx;

    if (sctx->depth == 1) {
        switch (sctx->stack[0]) {
        case PODMAN_CONTAINER_INFO_JSON_ID:
            sstrnncpy(sctx->info.id, sizeof(sctx->info.id), string, string_len);
            break;
        case PODMAN_CONTAINER_INFO_JSON_IMAGE:
            sstrnncpy(sctx->info.image, sizeof(sctx->info.image), string, string_len);
            break;
        case PODMAN_CONTAINER_INFO_JSON_NAME:
            sstrnncpy(sctx->info.name, sizeof(sctx->info.name), string, string_len);
            break;
        case PODMAN_CONTAINER_INFO_JSON_STATE:
            sstrnncpy(sctx->info.state, sizeof(sctx->info.state), string, string_len);
            break;
        case PODMAN_CONTAINER_INFO_JSON_CREATED:
            // FIXME "Created": "2023-07-04T23:09:50.817461235+02:00",
            break;
        default:
            break;
        }
    }

    return true;
}

static bool podman_container_info_json_number(void *ctx, const char *number_val,
                                                        size_t number_len)
{
    podman_container_info_json_ctx_t *sctx = ctx;

    char number[64];
    sstrnncpy(number, sizeof(number), number_val, number_len);

    switch (sctx->depth) {
    case 1:
        switch (sctx->stack[0]) {
        case PODMAN_CONTAINER_INFO_JSON_PID:
            strtouint(number, &sctx->info.pid);
            break;
        case PODMAN_CONTAINER_INFO_JSON_EXITED_AT:
            if (number_val[0] != '-')
                strtouint(number, &sctx->info.exit_at);
            break;
        case PODMAN_CONTAINER_INFO_JSON_EXIT_CODE:
            strtouint(number, &sctx->info.exit_code);
            break;
        case PODMAN_CONTAINER_INFO_JSON_STARTED_AT:
            strtouint(number, &sctx->info.started);
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    return true;
}

static bool podman_container_info_json_start_map(void *ctx)
{
    podman_container_info_json_ctx_t *sctx = ctx;
    sctx->depth++;
    sctx->stack[sctx->depth] = PODMAN_CONTAINER_INFO_JSON_NONE;

    if (sctx->depth == 1)
        memset(&sctx->info, 0, sizeof(sctx->info));

    return true;
}

static bool podman_container_info_json_map_key(void * ctx, const char * key, size_t key_len)
{
    podman_container_info_json_ctx_t *sctx = ctx;

    switch (sctx->depth) {
    case 1:
        switch(key_len) {
        case 2:
            if (strncmp("Id", (const char *)key, key_len) == 0) {
                sctx->stack[0] = PODMAN_CONTAINER_INFO_JSON_ID;
                return true;
            }
            break;
        case 3:
            if (strncmp("Pid", (const char *)key, key_len) == 0) {
                sctx->stack[0] = PODMAN_CONTAINER_INFO_JSON_PID;
                return true;
            }
            break;
        case 5:
            if (strncmp("Image", (const char *)key, key_len) == 0) {
                sctx->stack[0] = PODMAN_CONTAINER_INFO_JSON_IMAGE;
                return true;
            } else if (strncmp("State", (const char *)key, key_len) == 0) {
                sctx->stack[0] = PODMAN_CONTAINER_INFO_JSON_STATE;
                return true;
            } else if (strncmp("Names", (const char *)key, key_len) == 0) { // FIXME is array?
                sctx->stack[0] = PODMAN_CONTAINER_INFO_JSON_NAME;
                return true;
            }
            break;
        case 7:
            if (strncmp("Created", (const char *)key, key_len) == 0) {
                sctx->stack[0] = PODMAN_CONTAINER_INFO_JSON_CREATED;
                return true;
            }
            break;
        case 8:
            if (strncmp("ExitedAt", (const char *)key, key_len) == 0) {
                sctx->stack[0] = PODMAN_CONTAINER_INFO_JSON_EXITED_AT;
                return true;
            } else if (strncmp("ExitCode", (const char *)key, key_len) == 0) {
                sctx->stack[0] = PODMAN_CONTAINER_INFO_JSON_EXIT_CODE;
                return true;
            }
            break;
        case 9:
            if (strncmp("StartedAt", (const char *)key, key_len) == 0) {
                sctx->stack[0] = PODMAN_CONTAINER_INFO_JSON_STARTED_AT;
                return true;
            }
            break;
        }
        sctx->stack[0] = PODMAN_CONTAINER_INFO_JSON_NONE;
        break;
    }

    return true;
}

static bool podman_container_info_json_end_map(void *ctx)
{
    podman_container_info_json_ctx_t *sctx = ctx;

    if ((sctx->depth == 1) && (sctx->info.id[0] != '\0')) {
        if ((sctx->info.image[0] != '\0') && (sctx->info.name[0] != '\0')) {
            label_set_t info = {
                .num = 2,
                .ptr = (label_pair_t[]){
                    {.name = "image", .value = sctx->info.image},
                    {.name = "name", .value = sctx->info.name},
                }
            };
            metric_family_append(&sctx->fams[FAM_PODMAN_CONTAINER], VALUE_INFO(info), sctx->labels,
                                 &(label_pair_const_t){.name="id", .value=sctx->info.id}, NULL);
        }

        if (sctx->info.state[0] != '\0') {
            state_t states[] = {
                { .name = "unknown",     .enabled = false },
                { .name = "created",     .enabled = false },
                { .name = "initialized", .enabled = false },
                { .name = "running",     .enabled = false },
                { .name = "stopped",     .enabled = false },
                { .name = "paused",      .enabled = false },
                { .name = "exited",      .enabled = false },
                { .name = "removing",    .enabled = false },
                { .name = "stopping",    .enabled = false },
            };
            state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
            for (size_t j = 0; j < set.num ; j++) {
                if (strcmp(set.ptr[j].name, sctx->info.state) == 0) {
                    set.ptr[j].enabled = true;
                    break;
                }
            }
            metric_family_append(&sctx->fams[FAM_PODMAN_CONTAINER_STATE],
                                 VALUE_STATE_SET(set), sctx->labels,
                                 &(label_pair_const_t){.name="id", .value=sctx->info.id}, NULL);
        }

        metric_family_append(&sctx->fams[FAM_PODMAN_CONTAINER_CREATED_SECONDS],
                             VALUE_GAUGE(sctx->info.created), sctx->labels,
                             &(label_pair_const_t){.name="id", .value=sctx->info.id}, NULL);
        metric_family_append(&sctx->fams[FAM_PODMAN_CONTAINER_STARTED_SECONDS],
                             VALUE_GAUGE(sctx->info.started), sctx->labels,
                             &(label_pair_const_t){.name="id", .value=sctx->info.id}, NULL);
        metric_family_append(&sctx->fams[FAM_PODMAN_CONTAINER_EXIT_CODE],
                             VALUE_GAUGE(sctx->info.exit_code), sctx->labels,
                             &(label_pair_const_t){.name="id", .value=sctx->info.id}, NULL);
        metric_family_append(&sctx->fams[FAM_PODMAN_CONTAINER_EXITED_SECONDS],
                             VALUE_GAUGE(sctx->info.exit_at), sctx->labels,
                             &(label_pair_const_t){.name="id", .value=sctx->info.id}, NULL);
        metric_family_append(&sctx->fams[FAM_PODMAN_CONTAINER_PID],
                             VALUE_GAUGE(sctx->info.pid), sctx->labels,
                             &(label_pair_const_t){.name="id", .value=sctx->info.id}, NULL);

    }

    sctx->depth--;
    if (sctx->depth > 0) {
        sctx->stack[sctx->depth-1] = PODMAN_CONTAINER_INFO_JSON_NONE;
    } else {
        sctx->depth = 0;
    }

    return true;
}

static xson_callbacks_t podman_container_info_json_callbacks = {
    .xson_null        = NULL,
    .xson_boolean     = NULL,
    .xson_integer     = NULL,
    .xson_double      = NULL,
    .xson_number      = podman_container_info_json_number,
    .xson_string      = podman_container_info_json_string,
    .xson_start_map   = podman_container_info_json_start_map,
    .xson_map_key     = podman_container_info_json_map_key,
    .xson_end_map     = podman_container_info_json_end_map,
    .xson_start_array = NULL,
    .xson_end_array   = NULL,
};

static size_t podman_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    podman_instance_t *podman = user_data;
    if (podman == NULL) {
        PLUGIN_ERROR("user_data pointer is NULL.");
        return 0;
    }

    size_t len = size * nmemb;
    if (len == 0)
        return 0;

    json_status_t status = json_parser_parse(&podman->handle, buf, len);
    if (status != JSON_STATUS_OK)
        return 0;

    return len;
}

static int podman_curl_read(podman_instance_t *podman, const char *url,
                            xson_callbacks_t callbacks, void *ctx)
{
    if (podman->curl == NULL) {
        podman->curl = curl_easy_init();
        if (podman->curl == NULL) {
            PLUGIN_ERROR("curl_easy_init failed.");
            return -1;
        }

        CURLcode rcode = 0;

        rcode = curl_easy_setopt(podman->curl, CURLOPT_NOSIGNAL, 1L);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_NOSIGNAL failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(podman->curl, CURLOPT_WRITEFUNCTION, podman_curl_callback);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEFUNCTION failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(podman->curl, CURLOPT_WRITEDATA, podman);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEDATA failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(podman->curl, CURLOPT_USERAGENT, NCOLLECTD_USERAGENT);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERAGENT failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(podman->curl, CURLOPT_ERRORBUFFER, podman->curl_error);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(podman->curl, CURLOPT_FOLLOWLOCATION, 1L);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_FOLLOWLOCATION failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(podman->curl, CURLOPT_MAXREDIRS, 50L);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_MAXREDIRS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

#ifdef HAVE_CURLOPT_TIMEOUT_MS
        rcode = curl_easy_setopt(podman->curl, CURLOPT_TIMEOUT_MS,
                                   (podman->timeout >= 0) ? (long)podman->timeout
                                   : (long)CDTIME_T_TO_MS(plugin_get_interval()));
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

#endif
    }

    json_parser_init(&podman->handle, 0, &callbacks, &ctx);

    CURLcode rcode = curl_easy_setopt(podman->curl, CURLOPT_URL, url);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_URL failed: %s",
                     curl_easy_strerror(rcode));
        return -1;
    }


    if (curl_easy_perform(podman->curl) != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_perform failed: %s", podman->curl_error);
        json_parser_free(&podman->handle);
        return -1;
    }


    json_status_t status = json_parser_complete(&podman->handle);
    if (status != JSON_STATUS_OK) {
        unsigned char *errmsg = json_parser_get_error(&podman->handle, 0, NULL, 0);
        PLUGIN_ERROR("json_parse_complete failed: %s", (const char *)errmsg);
        json_parser_free_error(errmsg);
        json_parser_free (&podman->handle);
        return -1;
    }

    json_parser_free(&podman->handle);

    return 0;
}

static int podman_read(user_data_t *user_data)
{
    podman_instance_t *podman = user_data->data;

    if (podman == NULL) {
        PLUGIN_ERROR("podman instance is NULL.");
        return -1;
    }

    podman_container_info_json_ctx_t ictx = {0};
    ictx.fams = podman->fams;
    ictx.labels = &podman->labels;

    int status = podman_curl_read(podman, podman->url_info,
                                  podman_container_info_json_callbacks, &ictx);
    if (status != 0) {

    }

    podman_container_stats_json_ctx_t sctx = {0};
    sctx.fams = podman->fams;
    sctx.labels = &podman->labels;

    status = podman_curl_read(podman, podman->url_stats,
                              podman_container_stats_json_callbacks, &sctx);
    if (status != 0) {

    }

    plugin_dispatch_metric_family_array_filtered(podman->fams, FAM_PODMAN_MAX, podman->filter, 0);
    return 0;
}

static void podman_instance_free(void *arg)
{
    podman_instance_t *podman = arg;

    if (podman == NULL)
        return;

    if (podman->curl != NULL) {
        curl_easy_cleanup(podman->curl);
        podman->curl = NULL;
    }

    free(podman->instance);
    free(podman->url);
    // FIXME
    label_set_reset(&podman->labels);
    plugin_filter_free(podman->filter);

    free(podman);
}

static int podman_config_instance(config_item_t *ci)
{
    podman_instance_t *podman = calloc(1, sizeof(*podman));
    if (podman == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    podman->timeout = -1;

    memcpy(podman->fams, fams_podman, FAM_PODMAN_MAX * sizeof(podman->fams[0]));

    int status = cf_util_get_string(ci, &podman->instance);
    if (status != 0) {
        free(podman);
        return status;
    }
    assert(podman->instance != NULL);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &podman->url);
        } else if (strcasecmp("labels", child->key) == 0) {
            status = cf_util_get_label(child, &podman->labels);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_int(child, &podman->timeout);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &podman->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        podman_instance_free(podman);
        return -1;
    }

    if (podman->url != NULL) {
        char url[512] = {0};

        size_t url_len = strlen(podman->url);
        bool slash = podman->url[url_len-1] == '/' ? true : false;

        ssnprintf(url, sizeof(url), "%s%s%s", podman->url, slash ? "" : "/",
                                    "v1.0.0/libpod/containers/json");
        podman->url_info = strdup(url);
        if (podman->url_info == NULL) {
            podman_instance_free(podman);
            return -1;
        }

        ssnprintf(url, sizeof(url), "%s%s%s", podman->url, slash ? "" : "/",
                                    "v1.0.0/libpod/containers/stats?stream=false");
        podman->url_stats = strdup(url);
        if (podman->url_stats == NULL) {
            podman_instance_free(podman);
            return -1;
        }
    }

    return plugin_register_complex_read("podman", podman->instance, podman_read, interval,
                    &(user_data_t){.data = podman, .free_func = podman_instance_free});
}

static int podman_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = podman_config_instance(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int podman_init(void)
{
    curl_global_init(CURL_GLOBAL_SSL);
    return 0;
}

void module_register(void)
{
    plugin_register_init("podman", podman_init);
    plugin_register_config("podman", podman_config);
}
