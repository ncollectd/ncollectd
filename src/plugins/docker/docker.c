// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/llist.h"
#include "libutils/avltree.h"
#include "libxson/json_parse.h"

#include <curl/curl.h>

#define DOCKER_MIN_VERSION "1.17"

enum {
    FAM_DOCKER_CONTAINER,
    FAM_DOCKER_CONTAINER_STATE,
    FAM_DOCKER_CONTAINER_CREATED_SECONDS,
    FAM_DOCKER_CONTAINER_CPU_USER_SECONDS,
    FAM_DOCKER_CONTAINER_CPU_SYSTEM_SECONDS,
    FAM_DOCKER_CONTAINER_CPU_USAGE_SECONDS,
    FAM_DOCKER_CONTAINER_ONLINE_CPUS,
    FAM_DOCKER_CONTAINER_PROCESSES,
    FAM_DOCKER_CONTAINER_PROCESSES_LIMITS,
    FAM_DOCKER_CONTAINER_MEMORY_USAGE_BYTES,
    FAM_DOCKER_CONTAINER_MEMORY_LIMIT_BYTES,
    FAM_DOCKER_CONTAINER_NETWORK_RECEIVE_BYTES,
    FAM_DOCKER_CONTAINER_NETWORK_RECEIVE_PACKETS,
    FAM_DOCKER_CONTAINER_NETWORK_TRANSMIT_BYTES,
    FAM_DOCKER_CONTAINER_NETWORK_TRANSMIT_PACKETS,
    FAM_DOCKER_MAX,
};

static metric_family_t fams_docker[FAM_DOCKER_MAX] = {
    [FAM_DOCKER_CONTAINER] = {
        .name = "docker_container",
        .type = METRIC_TYPE_INFO,
        .help = "Container information.",
    },
    [FAM_DOCKER_CONTAINER_STATE] = {
        .name = "docker_container_state",
        .type = METRIC_TYPE_STATE_SET,
        .help = "Container current state",
    },
    [FAM_DOCKER_CONTAINER_CREATED_SECONDS] = {
        .name = "docker_container_created_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "Container creation time in unixtime.",
    },
    [FAM_DOCKER_CONTAINER_CPU_USER_SECONDS] = {
        .name = "docker_container_cpu_user_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help =  "Cumulative user cpu time consumed in seconds.",
    },
    [FAM_DOCKER_CONTAINER_CPU_SYSTEM_SECONDS] = {
        .name = "docker_container_cpu_system_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cumulative system cpu time consumed in seconds.",
    },
    [FAM_DOCKER_CONTAINER_CPU_USAGE_SECONDS] = {
        .name = "docker_container_cpu_usage_seconds",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cumulative cpu time consumed in seconds.",
    },
    [FAM_DOCKER_CONTAINER_ONLINE_CPUS] = {
        .name = "docker_container_online_cpus",
        .type = METRIC_TYPE_GAUGE,
        .help = "Cpus avilable inside the container.",
    },
    [FAM_DOCKER_CONTAINER_PROCESSES] = {
        .name = "docker_container_processes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of processes running inside the container.",
    },
    [FAM_DOCKER_CONTAINER_PROCESSES_LIMITS] = {
        .name = "docker_container_processes_limits",
        .type = METRIC_TYPE_GAUGE,
        .help = "Max allowed processes running inside the container."
    },
    [FAM_DOCKER_CONTAINER_MEMORY_USAGE_BYTES] = {
        .name = "docker_container_memory_usage_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of bytes used by the conainter.",
    },
    [FAM_DOCKER_CONTAINER_MEMORY_LIMIT_BYTES] = {
        .name = "docker_container_memory_limit_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Max allowed memory for the the container."
    },
    [FAM_DOCKER_CONTAINER_NETWORK_RECEIVE_BYTES] = {
        .name = "docker_container_network_receive_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes received by the network interfaces."
    },
    [FAM_DOCKER_CONTAINER_NETWORK_RECEIVE_PACKETS] = {
        .name = "docker_container_network_receive_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets received by the network interfaces."
    },
    [FAM_DOCKER_CONTAINER_NETWORK_TRANSMIT_BYTES] = {
        .name = "docker_container_network_transmit_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of bytes transmitted by the network interfaces."
    },
    [FAM_DOCKER_CONTAINER_NETWORK_TRANSMIT_PACKETS] = {
        .name = "docker_container_network_transmit_packets",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of packets transmitted by the network interfaces."
    },
};

typedef struct {
    uint64_t pids_current;
    uint64_t pids_limit;
    uint64_t cpu_usage;
    uint64_t cpu_system;
    uint64_t online_cpus;
    uint64_t mem_usage;
    uint64_t mem_limit;
    uint64_t net_rx_bytes;
    uint64_t net_rx_packets;
    uint64_t net_tx_bytes;
    uint64_t net_tx_packets;
} docker_container_stats_t;

typedef enum {
    DOCKER_STATS_JSON_NONE,
    DOCKER_STATS_JSON_ID,
    DOCKER_STATS_JSON_PIDS_STATS,
    DOCKER_STATS_JSON_PIDS_STATS_CURRENT,
    DOCKER_STATS_JSON_PIDS_STATS_LIMIT,
    DOCKER_STATS_JSON_CPU_STATS,
    DOCKER_STATS_JSON_CPU_STATS_CPU_USAGE,
    DOCKER_STATS_JSON_CPU_STATS_CPU_USAGE_TOTAL_USAGE,
    DOCKER_STATS_JSON_CPU_STATS_SYSTEM_CPU_USAGE,
    DOCKER_STATS_JSON_CPU_STATS_ONLINE_CPUS,
    DOCKER_STATS_JSON_MEMORY_STATS,
    DOCKER_STATS_JSON_MEMORY_STATS_USAGE,
    DOCKER_STATS_JSON_MEMORY_STATS_LIMIT,
    DOCKER_STATS_JSON_NETWORKS,
    DOCKER_STATS_JSON_NETWORKS_RX_BYTES,
    DOCKER_STATS_JSON_NETWORKS_RX_PACKETS,
    DOCKER_STATS_JSON_NETWORKS_TX_BYTES,
    DOCKER_STATS_JSON_NETWORKS_TX_PACKETS,
} docker_stats_json_t;

struct docker_stats_s;
typedef struct docker_stats_s docker_stats_t;

struct docker_instance_s;
typedef struct docker_instance_s docker_instance_t;

typedef struct {
    docker_stats_json_t stack[JSON_MAX_DEPTH];
    size_t depth;
    docker_container_stats_t stats;
    docker_stats_t *docker_stats;
    docker_instance_t *docker;
} docker_container_stats_json_ctx_t;

struct docker_stats_s {
    char id[65];
    CURL *curl;
    char curl_error[CURL_ERROR_SIZE];
    json_parser_t handle;
    cdtime_t last;
    docker_container_stats_t stats;
    docker_container_stats_json_ctx_t ctx;
};

typedef struct {
    char id[65];
    char name[512];
    char image[512];
    char imageid[512];
    char state[16];
    uint64_t created;
} docker_container_info_t;

typedef enum {
    DOCKER_INFO_JSON_NONE,
    DOCKER_INFO_JSON_ID,
    DOCKER_INFO_JSON_IMAGE,
    DOCKER_INFO_JSON_STATE,
    DOCKER_INFO_JSON_IMAGEID,
    DOCKER_INFO_JSON_CREATED,
} docker_container_info_json_t;

typedef struct {
    docker_container_info_json_t stack[JSON_MAX_DEPTH];
    size_t depth;
    docker_container_info_t info;
    metric_family_t *fams;
    label_set_t *labels;
    docker_instance_t *docker;
} docker_container_info_json_ctx_t;

struct docker_instance_s {
    char *instance;
    char *socket_path;
    char *url;
    char *url_info;
    int timeout;
    label_set_t labels;
    plugin_filter_t *filter;

    pthread_t thread_id;
    bool thread_running;

    CURL *curl;
    char curl_error[CURL_ERROR_SIZE];
    CURLM *curl_multi;

    pthread_mutex_t lock;
    llist_t *ladd;
    llist_t *ldel;
    c_avl_tree_t *ids;

    metric_family_t fams[FAM_DOCKER_MAX];
    json_parser_t handle;
};

static bool docker_container_stats_json_number(void *ctx, const char *number_val, size_t number_len)
{
    docker_container_stats_json_ctx_t *sctx = ctx;

    char number[64];
    sstrnncpy(number, sizeof(number), number_val, number_len);

    switch (sctx->depth) {
    case 2:
        switch (sctx->stack[1]) {
        case DOCKER_STATS_JSON_CPU_STATS_ONLINE_CPUS:
            strtouint(number, &sctx->stats.online_cpus);
            break;
        case DOCKER_STATS_JSON_CPU_STATS_SYSTEM_CPU_USAGE:
            strtouint(number, &sctx->stats.cpu_system);
            break;
        case DOCKER_STATS_JSON_PIDS_STATS_LIMIT:
            strtouint(number, &sctx->stats.pids_limit);
            break;
        case DOCKER_STATS_JSON_PIDS_STATS_CURRENT:
            strtouint(number, &sctx->stats.pids_current);
            break;
        case DOCKER_STATS_JSON_MEMORY_STATS_USAGE:
            strtouint(number, &sctx->stats.mem_usage);
            break;
        case DOCKER_STATS_JSON_MEMORY_STATS_LIMIT:
            strtouint(number, &sctx->stats.mem_limit);
            break;
        default:
            break;
        }
        break;
    case 3:
        switch (sctx->stack[1]) {
        case DOCKER_STATS_JSON_CPU_STATS_CPU_USAGE_TOTAL_USAGE:
            strtouint(number, &sctx->stats.cpu_usage);
            break;
        case DOCKER_STATS_JSON_NETWORKS_RX_BYTES: {
            uint64_t value = 0;
            strtouint(number, &value);
            sctx->stats.net_rx_bytes += value;
        }   break;
        case DOCKER_STATS_JSON_NETWORKS_TX_BYTES: {
            uint64_t value = 0;
            strtouint(number, &value);
            sctx->stats.net_tx_bytes += value;
        }   break;
        case DOCKER_STATS_JSON_NETWORKS_RX_PACKETS: {
            uint64_t value = 0;
            strtouint(number, &value);
            sctx->stats.net_rx_packets += value;
        }   break;
        case DOCKER_STATS_JSON_NETWORKS_TX_PACKETS: {
            uint64_t value = 0;
            strtouint(number, &value);
            sctx->stats.net_tx_packets += value;
        }   break;
        default:
            break;
        }
        break;
    }

    return true;
}

static bool docker_container_stats_json_start_map(void *ctx)
{
    docker_container_stats_json_ctx_t *sctx = ctx;
    sctx->depth++;
    sctx->stack[sctx->depth] = DOCKER_STATS_JSON_NONE;

    if (sctx->depth == 1)
        memset(&sctx->stats, 0, sizeof(sctx->stats));

    return true;
}

static bool docker_container_stats_json_map_key(void * ctx, const char * key, size_t key_len)
{
    docker_container_stats_json_ctx_t *sctx = ctx;

    switch (sctx->depth) {
    case 1:
        switch (key_len) {
        case 2:
            if (strncmp("id", (const char *)key, key_len) == 0) {
                sctx->stack[0] = DOCKER_STATS_JSON_ID;
                return true;
            }
            break;
        case 8:
            if (strncmp("networks", (const char *)key, key_len) == 0) {
                sctx->stack[0] = DOCKER_STATS_JSON_NETWORKS;
                return true;
            }
            break;
        case 9:
            if (strncmp("cpu_stats", (const char *)key, key_len) == 0) {
                sctx->stack[0] = DOCKER_STATS_JSON_CPU_STATS;
                return true;
            }
            break;
        case 10:
            if (strncmp("pids_stats", (const char *)key, key_len) == 0) {
                sctx->stack[0] = DOCKER_STATS_JSON_PIDS_STATS;
                return true;
            }
            break;
        case 12:
            if (strncmp("memory_stats", (const char *)key, key_len) == 0) {
                sctx->stack[0] = DOCKER_STATS_JSON_MEMORY_STATS;
                return true;
            }
            break;
        }
        sctx->stack[0] = DOCKER_STATS_JSON_NONE;
        break;
    case 2:
        switch (sctx->stack[0]) {
        case DOCKER_STATS_JSON_CPU_STATS:
            switch (key_len) {
            case 9:
                if (strncmp("cpu_usage", (const char *)key, key_len) == 0)
                    sctx->stack[1] = DOCKER_STATS_JSON_CPU_STATS_CPU_USAGE;
                else
                    sctx->stack[1] = DOCKER_STATS_JSON_NONE;
                break;
            case 11:
                if (strncmp("online_cpus", (const char *)key, key_len) == 0)
                    sctx->stack[1] = DOCKER_STATS_JSON_CPU_STATS_ONLINE_CPUS;
                else
                    sctx->stack[1] = DOCKER_STATS_JSON_NONE;
                break;
            case 16:
                if (strncmp("system_cpu_usage", (const char *)key, key_len) == 0)
                    sctx->stack[1] = DOCKER_STATS_JSON_CPU_STATS_SYSTEM_CPU_USAGE;
                else
                    sctx->stack[1] = DOCKER_STATS_JSON_NONE;
                break;
            default:
                sctx->stack[1] = DOCKER_STATS_JSON_NONE;
                break;
            }
            break;
        case DOCKER_STATS_JSON_PIDS_STATS:
            switch (key_len) {
            case 5:
                if (strncmp("limit", (const char *)key, key_len) == 0)
                    sctx->stack[1] = DOCKER_STATS_JSON_PIDS_STATS_LIMIT;
                else
                    sctx->stack[1] = DOCKER_STATS_JSON_NONE;
                break;
            case 7:
                if (strncmp("current", (const char *)key, key_len) == 0)
                    sctx->stack[1] = DOCKER_STATS_JSON_PIDS_STATS_CURRENT;
                else
                    sctx->stack[1] = DOCKER_STATS_JSON_NONE;
                break;
            default:
                sctx->stack[1] = DOCKER_STATS_JSON_NONE;
                break;
            }
            break;
        case DOCKER_STATS_JSON_MEMORY_STATS:
            if (key_len == 5) {
                if (strncmp("usage", (const char *)key, key_len) == 0) {
                    sctx->stack[1] = DOCKER_STATS_JSON_MEMORY_STATS_USAGE;
                } else if (strncmp("limit", (const char *)key, key_len) == 0) {
                    sctx->stack[1] = DOCKER_STATS_JSON_MEMORY_STATS_LIMIT;
                } else {
                    sctx->stack[1] = DOCKER_STATS_JSON_NONE;
                }
            }
            break;
        default:
            break;
        }
        break;
    case 3:
        if (sctx->stack[1] == DOCKER_STATS_JSON_CPU_STATS_CPU_USAGE) {
            if ((key_len == 11) && (strncmp("total_usage", (const char *)key, key_len) == 0)) {
                sctx->stack[2] = DOCKER_STATS_JSON_CPU_STATS_CPU_USAGE_TOTAL_USAGE;
            } else {
                sctx->stack[2] = DOCKER_STATS_JSON_NONE;
            }
        } else if (sctx->stack[0] == DOCKER_STATS_JSON_NETWORKS) {
            switch (key_len) {
            case 8:
                if (strncmp("rx_bytes", (const char *)key, key_len) == 0) {
                    sctx->stack[2] = DOCKER_STATS_JSON_NETWORKS_RX_BYTES;
                } else if (strncmp("tx_bytes", (const char *)key, key_len) == 0) {
                    sctx->stack[2] = DOCKER_STATS_JSON_NETWORKS_TX_BYTES;
                } else {
                    sctx->stack[2] = DOCKER_STATS_JSON_NONE;
                }
                break;
            case 10:
                if (strncmp("rx_packets", (const char *)key, key_len) == 0) {
                    sctx->stack[2] = DOCKER_STATS_JSON_NETWORKS_RX_PACKETS;
                } else if (strncmp("tx_packets", (const char *)key, key_len) == 0) {
                    sctx->stack[2] = DOCKER_STATS_JSON_NETWORKS_TX_PACKETS;
                } else {
                    sctx->stack[2] = DOCKER_STATS_JSON_NONE;
                }
                break;
            default:
                    sctx->stack[2] = DOCKER_STATS_JSON_NONE;
                break;
            }
        }
        break;
    }

    return true;
}

static bool docker_container_stats_json_end_map(void *ctx)
{
    docker_container_stats_json_ctx_t *sctx = ctx;

    if (sctx->depth == 1) {
        pthread_mutex_lock(&sctx->docker->lock);
        memcpy(&sctx->docker_stats->stats, &sctx->stats, sizeof(sctx->stats));
        pthread_mutex_unlock(&sctx->docker->lock);
    }

    sctx->depth--;
    if (sctx->depth > 0) {
        sctx->stack[sctx->depth-1] = DOCKER_STATS_JSON_NONE;
    } else {
        sctx->depth = 0;
    }

    return false;
}

json_callbacks_t docker_container_stats_json_callbacks = {
    .json_null        =  NULL,
    .json_boolean     =  NULL,
    .json_integer     =  NULL,
    .json_double      =  NULL,
    .json_number      =  docker_container_stats_json_number,
    .json_string      =  NULL,
    .json_start_map   =  docker_container_stats_json_start_map,
    .json_map_key     =  docker_container_stats_json_map_key,
    .json_end_map     =  docker_container_stats_json_end_map,
    .json_start_array =  NULL,
    .json_end_array   =  NULL,
};

static void docker_stats_free(docker_stats_t *docker_stats)
{
    if (docker_stats == NULL)
        return;

    if (docker_stats->curl != NULL) {
        curl_easy_cleanup(docker_stats->curl);
        docker_stats->curl = NULL;
    }

    free(docker_stats);
}

static void *docker_thread(void *data)
{
    docker_instance_t *docker = data;

    if (docker->curl_multi == NULL) {
        docker->curl_multi = curl_multi_init();
        if (docker->curl_multi == NULL) {
            PLUGIN_ERROR("curl_multi_init failed.");
            pthread_exit(NULL);
        }
    }

    docker->thread_running = true;

    while (true) {
        int still_running = 0;

        docker_stats_t *docker_stats = NULL;
        c_avl_iterator_t *iter = NULL;
        llentry_t *entry = NULL;

        pthread_mutex_lock(&docker->lock);

        while ((entry = llist_head(docker->ladd)) != NULL) {
            llist_remove(docker->ladd, entry);
            free(entry);
        }

        iter = c_avl_get_iterator(docker->ids);
        char *id = NULL;
        if (iter != NULL) {
            while (c_avl_iterator_next(iter, (void *)&id, (void *)&docker_stats) == 0) {
                curl_multi_add_handle(docker->curl_multi, docker_stats->curl);
            }
            c_avl_iterator_destroy(iter);
        }

        pthread_mutex_unlock(&docker->lock);

        do {
            CURLMcode mc;
            int numfds;

            pthread_mutex_lock(&docker->lock);

            while ((entry = llist_head(docker->ladd)) != NULL) {
                llist_remove(docker->ladd, entry);
                docker_stats = entry->value;
                curl_multi_add_handle(docker->curl_multi, docker_stats->curl);
                free(entry);
            }

            while ((entry = llist_head(docker->ldel)) != NULL) {
                llist_remove(docker->ldel, entry);
                docker_stats = entry->value;
                curl_multi_remove_handle(docker->curl_multi, docker_stats->curl);
                docker_stats_free(docker_stats);
                free(entry);
            }

            pthread_mutex_unlock(&docker->lock);

            mc = curl_multi_perform(docker->curl_multi, &still_running);
// fprintf(stderr, "curl_multi_perform: %d %d\n", (int)mc, still_running);
            if (mc != CURLM_OK) {

                break;
            }
                /* wait for activity, timeout or wakeup */
            mc = curl_multi_poll(docker->curl_multi, NULL, 0, 1000, &numfds);
            if (mc != CURLM_OK) {

            }

        } while(still_running);

        pthread_mutex_lock(&docker->lock);

        iter = c_avl_get_iterator(docker->ids);
        if (iter != NULL) {
            while (c_avl_iterator_next(iter, (void *)&id, (void *)&docker_stats) == 0) {
                curl_multi_remove_handle(docker->curl_multi, docker_stats->curl);
            }
            c_avl_iterator_destroy(iter);
        }

        pthread_mutex_unlock(&docker->lock);

    }

    docker->thread_running = false;
    pthread_exit(NULL);
}

static size_t docker_stats_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    docker_stats_t *docker_stats = user_data;
    if (docker_stats == NULL) {
        PLUGIN_ERROR("user_data pointer is NULL.");
        return 0;
    }

    size_t len = size * nmemb;
    if (len == 0)
        return 0;

    json_status_t status = json_parser_parse(&docker_stats->handle, buf, len);
    if (status != JSON_STATUS_OK) {
        unsigned char *errmsg = json_parser_get_error(&docker_stats->handle, 0, NULL, 0);
        PLUGIN_ERROR("json_parse_complete failed: %s", (const char *)errmsg);
        json_parser_free_error(errmsg);
        return 0;
    }

    return len;
}

static docker_stats_t *docker_stats_add(docker_instance_t *docker, char *id, cdtime_t now)
{
    docker_stats_t *docker_stats = NULL;

    pthread_mutex_lock(&docker->lock);

    int status = c_avl_get(docker->ids, id, (void *)&docker_stats);
    if (status == 0) {
        docker_stats->last = now;
        pthread_mutex_unlock(&docker->lock);
        return docker_stats;
    }

    pthread_mutex_unlock(&docker->lock);

    docker_stats = calloc(1, sizeof(*docker_stats));
    if (docker_stats == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return NULL;
    }

    sstrncpy(docker_stats->id, id, sizeof(docker_stats->id));
    docker_stats->last = now;

    docker_stats->curl = curl_easy_init();
    if (docker_stats->curl == NULL) {
        PLUGIN_ERROR("curl_easy_init failed.");
        free(docker_stats);
        return NULL;
    }

    CURLcode rcode = 0;

    rcode = curl_easy_setopt(docker_stats->curl, CURLOPT_NOSIGNAL, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_NOSIGNAL failed: %s",
                     curl_easy_strerror(rcode));
        free(docker_stats);
        return NULL;
    }

    rcode = curl_easy_setopt(docker_stats->curl, CURLOPT_WRITEFUNCTION, docker_stats_curl_callback);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEFUNCTION failed: %s",
                     curl_easy_strerror(rcode));
        free(docker_stats);
        return NULL;
    }

    rcode = curl_easy_setopt(docker_stats->curl, CURLOPT_WRITEDATA, docker_stats);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEDATA failed: %s",
                     curl_easy_strerror(rcode));
        free(docker_stats);
        return NULL;
    }

    rcode = curl_easy_setopt(docker_stats->curl, CURLOPT_USERAGENT, NCOLLECTD_USERAGENT);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERAGENT failed: %s",
                     curl_easy_strerror(rcode));
        free(docker_stats);
        return NULL;
    }

    rcode = curl_easy_setopt(docker_stats->curl, CURLOPT_ERRORBUFFER, docker_stats->curl_error);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s",
                     curl_easy_strerror(rcode));
        free(docker_stats);
        return NULL;
    }

    rcode = curl_easy_setopt(docker_stats->curl, CURLOPT_FOLLOWLOCATION, 1L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_FOLLOWLOCATION failed: %s",
                     curl_easy_strerror(rcode));
        free(docker_stats);
        return NULL;
    }

    rcode = curl_easy_setopt(docker_stats->curl, CURLOPT_MAXREDIRS, 50L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_MAXREDIRS failed: %s",
                     curl_easy_strerror(rcode));
        free(docker_stats);
        return NULL;
    }

#ifdef HAVE_CURLOPT_TIMEOUT_MS
    rcode = curl_easy_setopt(docker_stats->curl, CURLOPT_TIMEOUT_MS, 0L);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                     curl_easy_strerror(rcode));
        free(docker_stats);
        return NULL;
    }
/*
    curl_easy_setopt(docker_stats->curl, CURLOPT_TIMEOUT_MS,
                                   (docker->timeout >= 0) ? (long)docker->timeout
                                   : (long)CDTIME_T_TO_MS(plugin_get_interval()));
*/
#endif
#ifdef HAVE_CURLOPT_UNIX_SOCKET_PATH
    if (docker->socket_path != NULL) {
        rcode = curl_easy_setopt(docker_stats->curl, CURLOPT_UNIX_SOCKET_PATH, docker->socket_path);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_UNIX_SOCKET_PATH failed: %s",
                         curl_easy_strerror(rcode));
            free(docker_stats);
            return NULL;
        }
    }
#endif
    // curl_easy_setopt(docker_stats->curl, CURLOPT_VERBOSE, (long)1);

    char url[512];

    bool slash = false;
    if (docker->url != NULL) {
        size_t url_len = strlen(docker->url);
        if (docker->url[url_len-1] == '/')
            slash = true;
    }

    ssnprintf(url, sizeof(url), "%s%s%s%s/stats?stream=1"
                              , docker->url != NULL ? docker->url : "http://127.0.0.1"
                              , slash ? "" : "/"
                              , "v" DOCKER_MIN_VERSION "/containers/"
                              , id);

    rcode = curl_easy_setopt(docker_stats->curl, CURLOPT_URL, url);
    if (rcode != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_setopt CURLOPT_URL failed: %s",
                     curl_easy_strerror(rcode));
        free(docker_stats);
        return NULL;
    }


    docker_stats->ctx.docker_stats = docker_stats;
    docker_stats->ctx.docker = docker;

    json_parser_init(&docker_stats->handle, JSON_ALLOW_MULTIPLE_VALUES,
                     &docker_container_stats_json_callbacks, &docker_stats->ctx);

    pthread_mutex_lock(&docker->lock);

    llentry_t *entry = llentry_create(NULL, docker_stats);
    if (entry == NULL) {
        pthread_mutex_unlock(&docker->lock);
        docker_stats_free(docker_stats);
        PLUGIN_ERROR("llentry_create failed.");
        return NULL;
    }

    llist_append(docker->ladd, entry);

    status = c_avl_insert(docker->ids, docker_stats->id, docker_stats);
    if (status != 0) {
        llist_remove(docker->ladd, entry);
        pthread_mutex_unlock(&docker->lock);
        docker_stats_free(docker_stats);
        PLUGIN_ERROR("c_avl_insert failed.");
        return NULL;
    }

    pthread_mutex_unlock(&docker->lock);

    curl_multi_wakeup(&docker->curl_multi);

    return docker_stats;
}

static int docker_stats_purge(docker_instance_t *docker, cdtime_t last)
{
    char **to_be_deleted = NULL;
    size_t to_be_deleted_num = 0;

    docker_stats_t *docker_stats = NULL;
    char *id = NULL;

    pthread_mutex_lock(&docker->lock);

    c_avl_iterator_t *iter = c_avl_get_iterator(docker->ids);
    while (c_avl_iterator_next(iter, (void *)&id, (void *)&docker_stats) == 0) {
        if (last > docker_stats->last)
            strarray_add(&to_be_deleted, &to_be_deleted_num, id);
    }
    c_avl_iterator_destroy(iter);


    for (size_t i = 0; i < to_be_deleted_num; i++) {
        llentry_t *entry = llentry_create(NULL, docker_stats);
        if (entry == NULL) {
            pthread_mutex_unlock(&docker->lock);
            docker_stats_free(docker_stats);
            PLUGIN_ERROR("llentry_create failed.");
            return 0;
        }

        llist_append(docker->ldel, entry);

        c_avl_remove(docker->ids, to_be_deleted[i], (void *)&id, (void *)&docker_stats);
    }

    pthread_mutex_unlock(&docker->lock);

    if (to_be_deleted_num > 0)
        curl_multi_wakeup(&docker->curl_multi);

    strarray_free(to_be_deleted, to_be_deleted_num);

    return 0;
}

static bool docker_container_info_json_string(void *ctx, const char *string, size_t string_len)
{
    docker_container_info_json_ctx_t *sctx = ctx;

    if (sctx->depth == 1) {
        switch (sctx->stack[0]) {
        case DOCKER_INFO_JSON_ID:
            sstrnncpy(sctx->info.id, sizeof(sctx->info.id), string, string_len);
            break;
        case DOCKER_INFO_JSON_IMAGE:
            sstrnncpy(sctx->info.image, sizeof(sctx->info.image), string, string_len);
            break;
        case DOCKER_INFO_JSON_STATE:
            sstrnncpy(sctx->info.state, sizeof(sctx->info.state), string, string_len);
            break;
        case DOCKER_INFO_JSON_IMAGEID:
            sstrnncpy(sctx->info.imageid, sizeof(sctx->info.imageid), string, string_len);
            break;
        default:
            break;
        }
    }

    return true;
}

static bool docker_container_info_json_number(void *ctx, const char *number_val, size_t number_len)
{
    docker_container_info_json_ctx_t *sctx = ctx;

    char number[64];
    sstrnncpy(number, sizeof(number), number_val, number_len);

    switch (sctx->depth) {
    case 1:
        switch (sctx->stack[0]) {
        case DOCKER_INFO_JSON_CREATED:
            strtouint(number, &sctx->info.created);
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

static bool docker_container_info_json_start_map(void *ctx)
{
    docker_container_info_json_ctx_t *sctx = ctx;
    sctx->depth++;
    sctx->stack[sctx->depth] = DOCKER_INFO_JSON_NONE;

    if (sctx->depth == 1)
        memset(&sctx->info, 0, sizeof(sctx->info));
    return true;
}

static bool docker_container_info_json_map_key(void * ctx, const char * key, size_t key_len)
{
    docker_container_info_json_ctx_t *sctx = ctx;

    if (sctx->depth != 1)
        return true;

    switch (sctx->depth) {
    case 1:
        switch (key_len) {
        case 2:
            if (strncmp("Id", (const char *)key, key_len) == 0) {
                sctx->stack[0] = DOCKER_INFO_JSON_ID;
            } else {
                sctx->stack[0] = DOCKER_INFO_JSON_NONE;
            }
            break;
        case 5:
            if (strncmp("Image", (const char *)key, key_len) == 0) {
                sctx->stack[0] = DOCKER_INFO_JSON_IMAGE;
            } else if (strncmp("State", (const char *)key, key_len) == 0) {
                sctx->stack[0] = DOCKER_INFO_JSON_STATE;
            } else {
                sctx->stack[0] = DOCKER_INFO_JSON_NONE;
            }
            break;
        case 7:
            if (strncmp("ImageID", (const char *)key, key_len) == 0) {
                sctx->stack[0] = DOCKER_INFO_JSON_IMAGEID;
            } else if (strncmp("Created", (const char *)key, key_len) == 0) {
                sctx->stack[0] = DOCKER_INFO_JSON_CREATED;
            } else {
                sctx->stack[0] = DOCKER_INFO_JSON_NONE;
            }
            break;
        default:
            sctx->stack[0] = DOCKER_INFO_JSON_NONE;
            break;
        }
        break;
    }

    return true;
}

static bool docker_container_info_json_end_map(void *ctx)
{
    docker_container_info_json_ctx_t *sctx = ctx;

    if ((sctx->depth == 1) && (sctx->info.id[0] != '\0')) {

        docker_stats_t *docker_stats = docker_stats_add(sctx->docker, sctx->info.id, cdtime());
        if (docker_stats == NULL) {
            // FIXME
        }

        if ((sctx->info.image[0] != '\0') && (sctx->info.name[0] != '\0')) {
            label_set_t info = {
                .num = 2,
                .ptr = (label_pair_t[]){
                    {.name = "image", .value = sctx->info.image},
                    {.name = "name", .value = sctx->info.name},
                }
            };
            metric_family_append(&sctx->fams[FAM_DOCKER_CONTAINER], VALUE_INFO(info), sctx->labels,
                                 &(label_pair_const_t){.name="id", .value=sctx->info.id}, NULL);
        }

        if (sctx->info.state[0] != '\0') {
            state_t states[] = {
                { .name = "unknown",    .enabled = false },
                { .name = "created",    .enabled = false },
                { .name = "running",    .enabled = false },
                { .name = "paused",     .enabled = false },
                { .name = "exited",     .enabled = false },
                { .name = "restarting", .enabled = false },
                { .name = "dead",       .enabled = false },
            };
            state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
            size_t j;
            for (j = 0; j < set.num ; j++) {
                if (strcmp(set.ptr[j].name, sctx->info.state) == 0) {
                    set.ptr[j].enabled = true;
                    break;
                }
            }
            if (j == set.num)
                set.ptr[0].enabled = true;

            metric_family_append(&sctx->fams[FAM_DOCKER_CONTAINER_STATE],
                                 VALUE_STATE_SET(set), sctx->labels,
                                 &(label_pair_const_t){.name="id", .value=sctx->info.id}, NULL);
        }

        metric_family_append(&sctx->fams[FAM_DOCKER_CONTAINER_CREATED_SECONDS],
                             VALUE_GAUGE(sctx->info.created), sctx->labels,
                             &(label_pair_const_t){.name="id", .value=sctx->info.id}, NULL);
    }

    sctx->depth--;
    if (sctx->depth > 0) {
        sctx->stack[sctx->depth-1] = DOCKER_INFO_JSON_NONE;
    } else {
        sctx->depth = 0;
    }

    return true;
}

static json_callbacks_t docker_container_info_json_callbacks = {
   .json_null        =  NULL,
   .json_boolean     =  NULL,
   .json_integer     =  NULL,
   .json_double      =  NULL,
   .json_number      =  docker_container_info_json_number,
   .json_string      =  docker_container_info_json_string,
   .json_start_map   =  docker_container_info_json_start_map,
   .json_map_key     =  docker_container_info_json_map_key,
   .json_end_map     =  docker_container_info_json_end_map,
   .json_start_array =  NULL,
   .json_end_array   =  NULL,
};

static size_t docker_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
    docker_instance_t *docker = user_data;
    if (docker == NULL) {
        PLUGIN_ERROR("user_data pointer is NULL.");
        return 0;
    }

    size_t len = size * nmemb;
    if (len == 0)
        return 0;

    json_status_t status = json_parser_parse(&docker->handle, buf, len);
    if (status != JSON_STATUS_OK)
        return 0;

    return len;
}

static int docker_read(user_data_t *user_data)
{
    docker_instance_t *docker = user_data->data;

    if (docker == NULL) {
        PLUGIN_ERROR("docker instance is NULL.");
        return -1;
    }

    if (docker->curl == NULL) {
        docker->curl = curl_easy_init();
        if (docker->curl == NULL) {
            PLUGIN_ERROR("curl_easy_init failed.");
            return -1;
        }

        CURLcode rcode = 0;

        rcode = curl_easy_setopt(docker->curl, CURLOPT_NOSIGNAL, 1L);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_NOSIGNAL failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(docker->curl, CURLOPT_WRITEFUNCTION, docker_curl_callback);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEFUNCTION failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(docker->curl, CURLOPT_WRITEDATA, docker);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_WRITEDATA failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(docker->curl, CURLOPT_USERAGENT, NCOLLECTD_USERAGENT);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_USERAGENT failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(docker->curl, CURLOPT_ERRORBUFFER, docker->curl_error);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_ERRORBUFFER failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(docker->curl, CURLOPT_FOLLOWLOCATION, 1L);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_FOLLOWLOCATION failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }

        rcode = curl_easy_setopt(docker->curl, CURLOPT_MAXREDIRS, 50L);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_MAXREDIRS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
#ifdef HAVE_CURLOPT_TIMEOUT_MS
        rcode = curl_easy_setopt(docker->curl, CURLOPT_TIMEOUT_MS,
                                       (docker->timeout >= 0) ? (long)docker->timeout
                                       : (long)CDTIME_T_TO_MS(plugin_get_interval()));
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_TIMEOUT_MS failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
#endif
#ifdef HAVE_CURLOPT_UNIX_SOCKET_PATH
        if (docker->socket_path != NULL) {
            rcode = curl_easy_setopt(docker->curl, CURLOPT_UNIX_SOCKET_PATH, docker->socket_path);
            if (rcode != CURLE_OK) {
                PLUGIN_ERROR("curl_easy_setopt CURLOPT_UNIX_SOCKET_PATH failed: %s",
                             curl_easy_strerror(rcode));
                return -1;
            }
        }
#endif
        rcode = curl_easy_setopt(docker->curl, CURLOPT_URL, docker->url_info);
        if (rcode != CURLE_OK) {
            PLUGIN_ERROR("curl_easy_setopt CURLOPT_URL failed: %s",
                         curl_easy_strerror(rcode));
            return -1;
        }
    }

    docker_container_info_json_ctx_t ictx = {0};
    ictx.fams = docker->fams;
    ictx.labels = &docker->labels;
    ictx.docker = docker;

    json_parser_init(&docker->handle, 0, &docker_container_info_json_callbacks, &ictx);

    CURLcode code = curl_easy_perform(docker->curl);
    if (code != CURLE_OK) {
        PLUGIN_ERROR("curl_easy_perform failed: (%d) %s", (int)code, docker->curl_error);
        json_parser_free(&docker->handle);
        return -1;
    }

    json_status_t status = json_parser_complete(&docker->handle);
    if (status != JSON_STATUS_OK) {
        unsigned char *errmsg = json_parser_get_error(&docker->handle, 0, NULL, 0);
        PLUGIN_ERROR("json_parse_complete failed: %s", (const char *)errmsg);
        json_parser_free_error(errmsg);
        json_parser_free(&docker->handle);
        return -1;
    }

    json_parser_free(&docker->handle);

    docker_stats_purge(docker, cdtime() - plugin_get_interval());

    pthread_mutex_lock(&docker->lock);

    c_avl_iterator_t *iter = c_avl_get_iterator(docker->ids);
    docker_stats_t *docker_stats = NULL;
    char *id = NULL;
    while (c_avl_iterator_next(iter, (void*)&id, (void *)&docker_stats) == 0) {

#if 0
        metric_family_append(&docker->fams[FAM_DOCKER_CONTAINER_CPU_USER_SECONDS],
                             VALUE_COUNTER(
                             &docker->labels,
                             &(label_pair_const_t){.name="id", .value=docker_stats->id}, NULL);
#endif
        metric_family_append(&docker->fams[FAM_DOCKER_CONTAINER_CPU_SYSTEM_SECONDS],
                             VALUE_COUNTER(docker_stats->stats.cpu_system), // FIXME
                             &docker->labels,
                             &(label_pair_const_t){.name="id", .value=docker_stats->id}, NULL);
        metric_family_append(&docker->fams[FAM_DOCKER_CONTAINER_CPU_USAGE_SECONDS],
                             VALUE_COUNTER(docker_stats->stats.cpu_usage), // FIXME
                             &docker->labels,
                             &(label_pair_const_t){.name="id", .value=docker_stats->id}, NULL);
        metric_family_append(&docker->fams[FAM_DOCKER_CONTAINER_ONLINE_CPUS],
                             VALUE_GAUGE(docker_stats->stats.online_cpus), &docker->labels,
                             &(label_pair_const_t){.name="id", .value=docker_stats->id}, NULL);
        metric_family_append(&docker->fams[FAM_DOCKER_CONTAINER_PROCESSES],
                             VALUE_GAUGE(docker_stats->stats.pids_current), &docker->labels,
                             &(label_pair_const_t){.name="id", .value=docker_stats->id}, NULL);
        metric_family_append(&docker->fams[FAM_DOCKER_CONTAINER_PROCESSES_LIMITS],
                             VALUE_GAUGE(docker_stats->stats.pids_limit), &docker->labels,
                             &(label_pair_const_t){.name="id", .value=docker_stats->id}, NULL);
        metric_family_append(&docker->fams[FAM_DOCKER_CONTAINER_MEMORY_USAGE_BYTES],
                             VALUE_GAUGE(docker_stats->stats.mem_usage), &docker->labels,
                             &(label_pair_const_t){.name="id", .value=docker_stats->id}, NULL);
        metric_family_append(&docker->fams[FAM_DOCKER_CONTAINER_MEMORY_LIMIT_BYTES],
                             VALUE_GAUGE(docker_stats->stats.mem_limit), &docker->labels,
                             &(label_pair_const_t){.name="id", .value=docker_stats->id}, NULL);
        metric_family_append(&docker->fams[FAM_DOCKER_CONTAINER_NETWORK_RECEIVE_BYTES],
                             VALUE_COUNTER(docker_stats->stats.net_rx_bytes), &docker->labels,
                             &(label_pair_const_t){.name="id", .value=docker_stats->id}, NULL);
        metric_family_append(&docker->fams[FAM_DOCKER_CONTAINER_NETWORK_RECEIVE_PACKETS],
                             VALUE_COUNTER(docker_stats->stats.net_rx_packets), &docker->labels,
                             &(label_pair_const_t){.name="id", .value=docker_stats->id}, NULL);
        metric_family_append(&docker->fams[FAM_DOCKER_CONTAINER_NETWORK_TRANSMIT_BYTES],
                             VALUE_COUNTER(docker_stats->stats.net_tx_bytes), &docker->labels,
                             &(label_pair_const_t){.name="id", .value=docker_stats->id}, NULL);
        metric_family_append(&docker->fams[FAM_DOCKER_CONTAINER_NETWORK_TRANSMIT_PACKETS],
                             VALUE_COUNTER(docker_stats->stats.net_tx_packets), &docker->labels,
                             &(label_pair_const_t){.name="id", .value=docker_stats->id}, NULL);

    }
    c_avl_iterator_destroy(iter);

    pthread_mutex_unlock(&docker->lock);


    plugin_dispatch_metric_family_array_filtered(docker->fams, FAM_DOCKER_MAX, docker->filter, 0);
    return 0;
}

static void docker_instance_free(void *arg)
{
    docker_instance_t *docker = arg;

    if (docker == NULL)
        return;

    if (docker->curl != NULL) {
        curl_easy_cleanup(docker->curl);
        docker->curl = NULL;
    }

    if (docker->curl_multi != NULL) {
        curl_multi_cleanup(docker->curl_multi);
        docker->curl_multi = NULL;
    }

    pthread_mutex_destroy(&docker->lock);

    free(docker->instance);
    free(docker->socket_path);
    free(docker->url);
    free(docker->url_info);
    label_set_reset(&docker->labels);
    plugin_filter_free(docker->filter);

    if (docker->ladd != NULL) {
        for (llentry_t *e = llist_head(docker->ladd); e != NULL; e = e->next) {
            docker_stats_free(e->value);
        }
        llist_destroy(docker->ladd);
    }

    if (docker->ldel != NULL) {
        for (llentry_t *e = llist_head(docker->ldel); e != NULL; e = e->next) {
            docker_stats_free(e->value);
        }
        llist_destroy(docker->ldel);
    }

    if (docker->ids != NULL) {
        while (true) {
            char *id = NULL;
            docker_stats_t *docker_stats = NULL;
            int status = c_avl_pick(docker->ids, (void *)&id, (void *)&docker_stats);
            if (status != 0)
                break;
            docker_stats_free(docker_stats);
        }
        c_avl_destroy(docker->ids);
    }

    free(docker);
}

static int docker_config_instance(config_item_t *ci)
{
    docker_instance_t *docker = calloc(1, sizeof(*docker));
    if (docker == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    docker->ladd = llist_create();
    if (docker->ladd == NULL) {
        docker_instance_free(docker);
        PLUGIN_ERROR("llist_create failed.");
        return -1;
    }

    docker->ldel = llist_create();
    if (docker->ldel == NULL) {
        docker_instance_free(docker);
        PLUGIN_ERROR("llist_create failed.");
        return -1;
    }

    docker->ids = c_avl_create((int (*)(const void *, const void *))strcmp);
    if (docker->ids == NULL) {
        docker_instance_free(docker);
        PLUGIN_ERROR("c_avl_create failed.");
        return -1;
    }

    pthread_mutex_init(&docker->lock, NULL);

    docker->timeout = -1;

    memcpy(docker->fams, fams_docker, FAM_DOCKER_MAX * sizeof(docker->fams[0]));

    int status = cf_util_get_string(ci, &docker->instance);
    if (status != 0) {
        free(docker);
        return status;
    }
    assert(docker->instance != NULL);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &docker->url);
        } else if (strcasecmp("socket-path", child->key) == 0) {
            status = cf_util_get_string(child, &docker->socket_path);
        } else if (strcasecmp("labels", child->key) == 0) {
            status = cf_util_get_label(child, &docker->labels);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_int(child, &docker->timeout);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = plugin_filter_configure(child, &docker->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        docker_instance_free(docker);
        return -1;
    }

    if ((docker->url == NULL) && (docker->socket_path == NULL)) {
        PLUGIN_ERROR("missing ... FIXME"); // FIXME
        docker_instance_free(docker);
        return -1;
    }

    char url[512];

    bool slash = false;
    if (docker->url != NULL) {
        size_t url_len = strlen(docker->url);
        if (docker->url[url_len-1] == '/')
            slash = true;
    }

    ssnprintf(url, sizeof(url), "%s%s%s", docker->url != NULL ? docker->url : "http://127.0.0.1"
                                        , slash ? "" : "/"
                                        , "v" DOCKER_MIN_VERSION "/containers/json?limit=0");
    docker->url_info = strdup(url);
    if (docker->url_info == NULL) {
        PLUGIN_ERROR("strdup failed");
        docker_instance_free(docker);
        return -1;
    }


    status = plugin_thread_create(&docker->thread_id, docker_thread, docker, "docker");
    if (status != 0) {
        PLUGIN_ERROR("pthread_create() failed.");
        docker_instance_free(docker);
        return -1;
    }

    return plugin_register_complex_read("docker", docker->instance, docker_read, interval,
                    &(user_data_t){.data = docker, .free_func = docker_instance_free});
}

static int docker_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = docker_config_instance(child);
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

static int docker_init(void)
{
    curl_global_init(CURL_GLOBAL_SSL);
    return 0;
}

void module_register(void)
{
    plugin_register_init("docker", docker_init);
    plugin_register_config("docker", docker_config);
}
