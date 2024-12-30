// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/avltree.h"
#include "libutils/socket.h"
#include "libxson/json_parse.h"

enum {
    FAM_KEA_UP,
    FAM_KEA_DHCP4_PACKETS_SENT,
    FAM_KEA_DHCP4_PACKETS_RECEIVED,
    FAM_KEA_DHCP6_PACKETS_SENT,
    FAM_KEA_DHCP6_PACKETS_RECEIVED,
    FAM_KEA_DHCP6_PACKETS_SENT_DHCP4,
    FAM_KEA_DHCP6_PACKETS_RECEIVED_DHCP4,
    FAM_KEA_DHCP4_GLOBAL4_CUMULATIVE_ADDRESSES_ASSIGNED,
    FAM_KEA_DHCP4_GLOBAL4_ADDRESSES_DECLINED,
    FAM_KEA_DHCP4_GLOBAL4_ADDRESSES_DECLINED_RECLAIMED,
    FAM_KEA_DHCP4_GLOBAL4_ADDRESSES_RECLAIMED,
    FAM_KEA_DHCP6_GLOBAL6_ADDRESSES_DECLINED,
    FAM_KEA_DHCP6_GLOBAL6_CUMULATIVE_NAS_ASSIGNED,
    FAM_KEA_DHCP6_GLOBAL6_CUMULATIVE_PDS_ASSIGNED,
    FAM_KEA_DHCP6_GLOBAL6_ADDRESSES_DECLINED_RECLAIMED,
    FAM_KEA_DHCP6_GLOBAL6_ADDRESSES_RECLAIMED,
    FAM_KEA_DHCP4_ADDRESSES_ASSIGNED,
    FAM_KEA_DHCP4_ADDRESSES_DECLINED,
    FAM_KEA_DHCP4_ADDRESSES_DECLINED_RECLAIMED,
    FAM_KEA_DHCP4_ADDRESSES_RECLAIMED,
    FAM_KEA_DHCP4_ADDRESSES,
    FAM_KEA_DHCP4_CUMULATIVE_ADDRESSES_ASSIGNED,
    FAM_KEA_DHCP4_ADDRESSES_ALLOCATION_FAIL,
    FAM_KEA_DHCP4_RESERVATION_CONFLICTS,
    FAM_KEA_DHCP4_LEASES_REUSED,
    FAM_KEA_DHCP6_NA,
    FAM_KEA_DHCP6_NA_ASSIGNED,
    FAM_KEA_DHCP6_PD,
    FAM_KEA_DHCP6_PD_ASSIGNED,
    FAM_KEA_DHCP6_ADDRESSES_RECLAIMED,
    FAM_KEA_DHCP6_ADDRESSES_DECLINED,
    FAM_KEA_DHCP6_ADDRESSES_DECLINED_RECLAIMED,
    FAM_KEA_DHCP6_CUMULATIVE_NAS_ASSIGNED,
    FAM_KEA_DHCP6_CUMULATIVE_PDS_ASSIGNED,
    FAM_KEA_DHCP6_ALLOCATIONS_FAILED,
    FAM_KEA_DHCP6_RESERVATION_CONFLICTS,
    FAM_KEA_DHCP6_NA_REUSES,
    FAM_KEA_DHCP6_PD_REUSES,
    FAM_KEA_MAX,
};

static metric_family_t fams_kea[FAM_KEA_MAX] = {
    [FAM_KEA_UP] = {
        .name = "kea_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the kea server be reached."
    },
    [FAM_KEA_DHCP4_PACKETS_SENT] = {
        .name = "kea_dhcp4_packets_sent",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total DHCPv4 packets sent by operation."
    },
    [FAM_KEA_DHCP4_PACKETS_RECEIVED] = {
        .name = "kea_dhcp4_packets_received",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total DHCPv4 packets received by operation."
    },

    [FAM_KEA_DHCP6_PACKETS_SENT] = {
        .name = "kea_dhcp6_packets_sent",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total DHCPv6 packets sent by operation."
    },
    [FAM_KEA_DHCP6_PACKETS_RECEIVED] = {
        .name = "kea_dhcp6_packets_received",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total DHCPv6 packets received by operation."
    },
    [FAM_KEA_DHCP6_PACKETS_SENT_DHCP4] = {
        .name = "kea_dhcp6_packets_sent_dhcp4",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total DHCPv4 over DHCPv6 packets sent by operation."
    },
    [FAM_KEA_DHCP6_PACKETS_RECEIVED_DHCP4] = {
        .name = "kea_dhcp6_packets_received_dhcp4",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total DHCPv4 over DHCPv6 packets received by operation."
    },
    [FAM_KEA_DHCP4_GLOBAL4_CUMULATIVE_ADDRESSES_ASSIGNED] = {
        .name = "kea_dhcp4_global4_cumulative_addresses_assigned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cumulative number of assigned addresses since server startup from all subnets",
    },
    [FAM_KEA_DHCP4_GLOBAL4_ADDRESSES_DECLINED] = {
        .name = "kea_dhcp4_global4_addresses_declined",
        .type = METRIC_TYPE_COUNTER,
        .help = "Declined counts from all subnets",
    },
    [FAM_KEA_DHCP4_GLOBAL4_ADDRESSES_DECLINED_RECLAIMED] = {
        .name = "kea_dhcp4_global4_addresses_declined_reclaimed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Declined addresses that were reclaimed for all subnets",
    },
    [FAM_KEA_DHCP4_GLOBAL4_ADDRESSES_RECLAIMED] = {
        .name = "kea_dhcp4_global4_addresses_reclaimed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Expired addresses that were reclaimed for all subnets",
    },
    [FAM_KEA_DHCP6_GLOBAL6_ADDRESSES_DECLINED] = {
        .name = "kea_dhcp6_global6_addresses_declined",
        .type = METRIC_TYPE_COUNTER,
        .help = "Declined counts from all subnets",
    },
    [FAM_KEA_DHCP6_GLOBAL6_CUMULATIVE_NAS_ASSIGNED] = {
        .name = "kea_dhcp6_global6_cumulative_nas_assigned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cumulative number of assigned NA addresses since server startup from all subnets",
    },
    [FAM_KEA_DHCP6_GLOBAL6_CUMULATIVE_PDS_ASSIGNED] = {
        .name = "kea_dhcp6_global6_cumulative_pds_assigned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cumulative number of assigned PD prefixes since server startup",
    },
    [FAM_KEA_DHCP6_GLOBAL6_ADDRESSES_DECLINED_RECLAIMED] = {
        .name = "kea_dhcp6_global6_addresses_declined_reclaimed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Declined addresses that were reclaimed for all subnets",
    },
    [FAM_KEA_DHCP6_GLOBAL6_ADDRESSES_RECLAIMED] = {
        .name = "kea_dhcp6_global6_addresses_reclaimed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Expired addresses that were reclaimed for all subnets",
    },
    [FAM_KEA_DHCP4_ADDRESSES_ASSIGNED] = {
        .name = "kea_dhcp4_addresses_assigned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Assigned addresses",
    },
    [FAM_KEA_DHCP4_ADDRESSES_DECLINED] = {
        .name = "kea_dhcp4_addresses_declined",
        .type = METRIC_TYPE_COUNTER,
        .help = "Declined counts",
    },
    [FAM_KEA_DHCP4_ADDRESSES_DECLINED_RECLAIMED] = {
        .name = "kea_dhcp4_addresses_declined_reclaimed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Declined addresses that were reclaimed",
    },
    [FAM_KEA_DHCP4_ADDRESSES_RECLAIMED] = {
        .name = "kea_dhcp4_addresses_reclaimed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Expired addresses that were reclaimed",
    },
    [FAM_KEA_DHCP4_ADDRESSES] = {
        .name = "kea_dhcp4_addresses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Size of subnet address pool",
    },
    [FAM_KEA_DHCP4_CUMULATIVE_ADDRESSES_ASSIGNED] = {  // FIXME ¿?
        .name = "kea_dhcp4_cumulative_addresses_assigned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cumulative number of assigned addresses since server startup",
    },
    [FAM_KEA_DHCP4_ADDRESSES_ALLOCATION_FAIL] = {
        .name = "kea_dhcp4_addresses_allocation_fail",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total allocation fails.",
    },
    [FAM_KEA_DHCP4_RESERVATION_CONFLICTS] = {
        .name = "kea_dhcp4_reservation_conflicts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total reservation conflict.",
    },
    [FAM_KEA_DHCP4_LEASES_REUSED] = {
        .name = "kea_dhcp4_leases_reused",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of times an IPv4 lease has been renewed in memory.",
    },
    [FAM_KEA_DHCP6_NA] = {
        .name = "kea_dhcp6_na",
        .type = METRIC_TYPE_COUNTER,
        .help = "Size of non-temporary address pool",
    },
    [FAM_KEA_DHCP6_NA_ASSIGNED] = {
        .name = "kea_dhcp6_na_assigned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Assigned non-temporary addresses (IA_NA)",
    },
    [FAM_KEA_DHCP6_PD] = {
        .name = "kea_dhcp6_pd",
        .type = METRIC_TYPE_COUNTER,
        .help = "Size of prefix delegation pool",
    },
    [FAM_KEA_DHCP6_PD_ASSIGNED] = {
        .name = "kea_dhcp6_pd_assigned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Assigned prefix delegations (IA_PD)",
    },
    [FAM_KEA_DHCP6_ADDRESSES_RECLAIMED] = {
        .name = "kea_dhcp6_addresses_reclaimed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Expired addresses that were reclaimed",
    },
    [FAM_KEA_DHCP6_ADDRESSES_DECLINED] = {
        .name = "kea_dhcp6_addresses_declined",
        .type = METRIC_TYPE_COUNTER,
        .help = "Declined counts",
    },
    [FAM_KEA_DHCP6_ADDRESSES_DECLINED_RECLAIMED] = {
        .name = "kea_dhcp6_addresses_declined_reclaimed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Declined addresses that were reclaimed",
    },
    [FAM_KEA_DHCP6_CUMULATIVE_NAS_ASSIGNED] = {
        .name = "kea_dhcp6_cumulative_nas_assigned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cumulative number of assigned NA addresses since server startup",
    },
    [FAM_KEA_DHCP6_CUMULATIVE_PDS_ASSIGNED] = {  // FIXME ¿?
        .name = "kea_dhcp6_cumulative_pds_assigned",
        .type = METRIC_TYPE_COUNTER,
        .help = "Cumulative number of assigned PD prefixes since server startup",
    },
    [FAM_KEA_DHCP6_ALLOCATIONS_FAILED] = {
        .name = "kea_dhcp6_allocations_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Allocation fail count",
    },
    [FAM_KEA_DHCP6_RESERVATION_CONFLICTS] = {
        .name = "kea_dhcp6_reservation_conflicts",
        .type = METRIC_TYPE_COUNTER,
        .help = "Reservation conflict count",
    },
    [FAM_KEA_DHCP6_NA_REUSES] = {
        .name = "kea_dhcp6_na_reuses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of IA_NA lease reuses",
    },
    [FAM_KEA_DHCP6_PD_REUSES] = {
        .name = "kea_dhcp6_pd_reuses",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of IA_PD lease reuses",
    },
};

#include "plugins/kea/kea_stats.h"

#define CONFIG_HASH_SIZE 256

typedef enum {
    KEA_NONE,
    KEA_DHCP4,
    KEA_DHCP6,
} kea_server_t;

typedef struct {
    char *id;
    char *subnet;
    size_t pool_size;
    char **pools;
} kea_subnet_t;

typedef struct {
    char *instance;
    char *socketpath;
    cdtime_t timeout;
    char config_hash[CONFIG_HASH_SIZE];
    kea_server_t kind;
    c_avl_tree_t *subnets;
    metric_family_t fams[FAM_KEA_MAX];
    label_set_t labels;
    plugin_filter_t *filter;
} kea_t;

static void kea_subnet_free(kea_subnet_t *subnet)
{
    if (subnet == NULL)
        return;

    free(subnet->id);
    free(subnet->subnet);
    if (subnet->pools != NULL) {
        for (size_t i = 0; i < subnet->pool_size; i++) {
            free(subnet->pools[i]);
        }
        free(subnet->pools);
    }

    free(subnet);
}

static int kea_subnet_set_id(kea_subnet_t *subnet, const char *id, size_t id_len)
{
    if (subnet == NULL)
        return -1;

    if (subnet->id != NULL)
        free(subnet->id);
    subnet->id = sstrndup(id, id_len);
    return 0;
}

static int kea_subnet_set_subnet(kea_subnet_t *subnet, const char *subnet_name, size_t subnet_name_len)
{
    if (subnet == NULL)
        return -1;

    if (subnet->subnet != NULL)
        free(subnet->subnet);
    subnet->subnet = sstrndup(subnet_name, subnet_name_len);
    return 0;
}

static int kea_subnet_add_pool(kea_subnet_t *subnet, const char *pool, size_t pool_len)
{
    if (subnet == NULL)
        return -1;

    char *apool = sstrndup(pool, pool_len);
    if (apool == NULL) {
        PLUGIN_ERROR("sstrndup failed.");
        return -1;
    }

    char **tmp = realloc(subnet->pools, sizeof(*subnet->pools) * (subnet->pool_size + 1));
    if (tmp == NULL) {
        free(apool);
        PLUGIN_ERROR("realloc failed.");
        return -1;
    }

    subnet->pools = tmp;
    subnet->pools[subnet->pool_size] = apool;
    subnet->pool_size++;

    return 0;
}

static char *kea_subnet_get_pool(kea_subnet_t *subnet, const char *pool_id)
{
    if (subnet == NULL)
        return NULL;

    if (subnet->pools == NULL)
        return NULL;

    size_t n = atoi(pool_id);
    if (n >= subnet->pool_size)
        return NULL;

    return subnet->pools[n];
}

static int kea_config_add_subnet(kea_t *kea, kea_subnet_t *subnet)
{
    if (subnet == NULL)
        return -1;

    if (kea->subnets == NULL) {
        kea->subnets = c_avl_create((int (*)(const void *, const void *))strcmp);
        if (kea->subnets == NULL) {
            PLUGIN_ERROR("cannot create avl tree.");
            return -1;
        }
    }

    int status = c_avl_insert(kea->subnets, subnet->id, subnet);
    if (status != 0) {

    }

    return 0;
}

static kea_subnet_t *kea_config_get_subnet(kea_t *kea, char *id)
{
    if (kea->subnets == NULL)
        return NULL;

    kea_subnet_t *subnet = NULL;
    int status = c_avl_get(kea->subnets, id, (void *)&subnet);
    if (status == 0)
        return subnet;

    return NULL;
}

static void kea_config_reset(c_avl_tree_t *tree)
{
    while (true) {
        char *id = NULL;
        kea_subnet_t *subnet = NULL;
        int status = c_avl_pick(tree, (void *)&id, (void *)&subnet);
        if (status != 0)
            break;
        kea_subnet_free(subnet);
    }
}

static void kea_free(void *arg)
{
    kea_t *kea = (kea_t *)arg;
    if (kea == NULL)
        return;

    free(kea->instance);
    free(kea->socketpath);
    label_set_reset(&kea->labels);

    if (kea->subnets != NULL) {
        kea_config_reset(kea->subnets);
        c_avl_destroy(kea->subnets);
    }

    free(kea);
}

static int kea_cmd(kea_t *kea, char *cmd)
{
    int fd = socket_connect_unix_stream(kea->socketpath, kea->timeout);
    if (fd < 0) {
        PLUGIN_ERROR("socket failed: %s", STRERRNO);
        return -1;
    }

    ssize_t cmd_len = strlen(cmd);

    if (send(fd, cmd, cmd_len, 0) < cmd_len) {
        PLUGIN_ERROR("unix socket send command failed");
        close(fd);
        return -1;
    }

    return fd;
}

typedef enum {
    KEA_STATS_JSON_NONE,
    KEA_STATS_JSON_ARGS,
    KEA_STATS_JSON_ARGS_METRIC,
    KEA_STATS_JSON_ARGS_METRIC_ARRAY,
    KEA_STATS_JSON_ARGS_METRIC_ARRAY_ARRAY,
} kea_json_stats_key_t;

typedef struct {
    kea_json_stats_key_t stack[JSON_MAX_DEPTH];
    size_t depth;
    kea_t *kea;
    char key[256];
} kea_json_stats_ctx_t;

static bool kea_json_stats_number(void *ctx, const char *num, size_t num_len)
{
    kea_json_stats_ctx_t *sctx = ctx;

    if (sctx->depth != 4)
        return true;
    if (sctx->stack[3] != KEA_STATS_JSON_ARGS_METRIC_ARRAY_ARRAY)
        return true;

    sctx->stack[3] = KEA_STATS_JSON_NONE;

    char *key = sctx->key;
    char *subnet_id = NULL;
    char *pool_id = NULL;
    if (strncmp(sctx->key, "subnet[", strlen("subnet[")) == 0) {
        subnet_id = key + strlen("subnet[");
        key = strchr(subnet_id, ']');
        if (key == NULL)
            return true;
        *key = '\0';
        key++;
        if (strncmp(key, ".pool[", strlen(".pool[")) == 0) {
            pool_id = key + strlen(".pool[");
            key = strchr(pool_id, ']');
            if (key == NULL)
                return true;
            *key = '\0';
            key++;
        }

        if (*key == '.')
            key++;
    }

    char skey[256];

    switch(sctx->kea->kind) {
    case KEA_NONE:
        return true;
        break;
    case KEA_DHCP4:
        if ((strncmp(key, "pkt4-", strlen("pkt4-")) == 0) ||
            (strncmp(key, "v4-", strlen("v4-")) == 0)) {
            sstrncpy(skey, key, sizeof(skey));
        } else if (subnet_id != NULL) {
            sstrncpy(skey, "subnet4::", sizeof(skey));
            sstrncpy(skey + strlen("subnet4::"), key, sizeof(skey) - strlen("subnet4::"));
        } else {
            sstrncpy(skey, "global4::", sizeof(skey));
            sstrncpy(skey + strlen("global4::"), key, sizeof(skey) - strlen("global4::"));
        }
        break;
    case KEA_DHCP6:
        if ((strncmp(key, "pkt6-", strlen("pkt6-")) == 0) ||
            (strncmp(key, "v6-", strlen("v6-")) == 0)) {
            sstrncpy(skey, key, sizeof(skey));
        } else if (subnet_id != NULL) {
            sstrncpy(skey, "subnet6::", sizeof(skey));
            sstrncpy(skey + strlen("subnet6::"), key, sizeof(skey) - strlen("subnet6::"));
        } else {
            sstrncpy(skey, "global6::", sizeof(skey));
            sstrncpy(skey + strlen("global6::"), key, sizeof(skey) - strlen("global6::"));
        }
        break;
    }


    const struct kea_stats *ks = kea_stats_get_key(skey, strlen(skey));
    if (ks == NULL) {
        PLUGIN_DEBUG("Unknow key: '%s'", skey);
        return true;
    }

    if (ks->fam < 0)
        return true;

    metric_family_t *fam = &sctx->kea->fams[ks->fam];

    char number[256];
    sstrnncpy(number, sizeof(number), (const char *)num, num_len);

    value_t value = {0};
    if (fam->type == METRIC_TYPE_COUNTER) {
        value = VALUE_COUNTER(atol(number));
    } else if (fam->type == METRIC_TYPE_GAUGE) {
        value = VALUE_GAUGE(atof(number));
    } else {
        return true;
    }

    size_t n = 0;
    label_pair_t labels[4] = {0};
    label_pair_t *label0 = NULL;
    label_pair_t *label1 = NULL;
    label_pair_t *label2 = NULL;
    label_pair_t *label3 = NULL;

    if (subnet_id != NULL) {
        kea_subnet_t *subnet = kea_config_get_subnet(sctx->kea, subnet_id);
        if (subnet != NULL) {
            labels[n].name = "id";
            labels[n].value = subnet->id;
            n++;
            if (pool_id != NULL) {
                char *pool = kea_subnet_get_pool(subnet, pool_id);
                if (pool != NULL) {
                    labels[n].name = "pool";
                    labels[n].value = pool;
                    n++;
                }
            }
            labels[n].name = "subnet";
            labels[n].value = subnet->subnet;
            n++;
        }
    }

    if (ks->lkey != NULL) {
        labels[n].name = ks->lkey;
        labels[n].value = ks->lvalue;
        n++;
    }

    if (n > 0)
        label0 = &labels[0];
    if (n > 1)
        label1 = &labels[1];
    if (n > 2)
        label2 = &labels[2];
    if (n > 3)
        label3 = &labels[3];

    metric_family_append(fam, value, &sctx->kea->labels, label0, label1, label2, label3, NULL);

    return true;
}

static bool kea_json_stats_map_key(void * ctx, const char *ukey, size_t key_len)
{
    kea_json_stats_ctx_t *sctx = ctx;
    if (sctx == NULL)
        return false;

    switch(sctx->depth) {
    case 1:
        if ((key_len == 9) && (strncmp(ukey, "arguments", key_len) == 0)) {
            sctx->stack[0] = KEA_STATS_JSON_ARGS;
        } else {
            sctx->stack[0] = KEA_STATS_JSON_NONE;
        }
        break;
    case 2:
        if (sctx->stack[0] == KEA_STATS_JSON_ARGS) {
            sctx->stack[1] = KEA_STATS_JSON_ARGS_METRIC;
            sstrnncpy(sctx->key, sizeof(sctx->key), ukey, key_len);
        } else {
            sctx->stack[1] = KEA_STATS_JSON_NONE;
        }
        break;
    }

    return true;
}

static bool kea_json_stats_end_map(void *ctx)
{
    kea_json_stats_ctx_t *sctx = ctx;

    sctx->depth--;
    if (sctx->depth > 0) {
        sctx->stack[sctx->depth-1] = KEA_STATS_JSON_NONE;
    } else {
        sctx->depth = 0;
    }

    return true;
}

static bool kea_json_stats_start_map(void *ctx)
{
    kea_json_stats_ctx_t *sctx = ctx;
    sctx->depth++;
    if (sctx->depth >= JSON_MAX_DEPTH)
        return false;
    sctx->stack[sctx->depth-1] = KEA_STATS_JSON_NONE;
    return true;
}

static bool kea_json_stats_start_array(void *ctx)
{
    kea_json_stats_ctx_t *sctx = ctx;

    sctx->depth++;
    if (sctx->depth >= JSON_MAX_DEPTH)
        return false;

    if (sctx->depth == 3) {
        if (sctx->stack[1] == KEA_STATS_JSON_ARGS_METRIC) {
            sctx->stack[2] = KEA_STATS_JSON_ARGS_METRIC_ARRAY;
        } else {
            sctx->stack[2] = KEA_STATS_JSON_NONE;
        }
    } else if (sctx->depth == 4) {
        if (sctx->stack[2] == KEA_STATS_JSON_ARGS_METRIC_ARRAY) {
            sctx->stack[3] = KEA_STATS_JSON_ARGS_METRIC_ARRAY_ARRAY;
        } else {
            sctx->stack[3] = KEA_STATS_JSON_NONE;
        }
    } else {
        sctx->stack[sctx->depth-1] = KEA_STATS_JSON_NONE;
    }

    return true;
}

static bool kea_json_stats_end_array(void *ctx)
{
    kea_json_stats_ctx_t *sctx = ctx;

    sctx->depth--;
    if (sctx->depth > 0) {
        sctx->stack[sctx->depth-1] = KEA_STATS_JSON_NONE;
    } else {
        sctx->depth = 0;
    }

    return true;
}

static json_callbacks_t kea_json_stats_callbacks = {
    .json_null        = NULL,
    .json_boolean     = NULL,
    .json_integer     = NULL,
    .json_double      = NULL,
    .json_number      = kea_json_stats_number,
    .json_string      = NULL,
    .json_start_map   = kea_json_stats_start_map,
    .json_map_key     = kea_json_stats_map_key,
    .json_end_map     = kea_json_stats_end_map,
    .json_start_array = kea_json_stats_start_array,
    .json_end_array   = kea_json_stats_end_array,
};

static int kea_read_stats(kea_t *kea)
{
    int fd = kea_cmd (kea, "{\"command\": \"statistic-get-all\"}");
    if (fd < 0)
        return -1;

    kea_json_stats_ctx_t ctx = {0};
    ctx.kea = kea;

    json_parser_t handle;
    json_parser_init(&handle, 0, &kea_json_stats_callbacks, &ctx);

    char buffer[4096];
    ssize_t len = 0;
    while ((len = read(fd, buffer, sizeof(buffer))) > 0) {
        json_status_t status = json_parser_parse(&handle, (unsigned char *)buffer, len);
        if (status != JSON_STATUS_OK)
            break;
    }

    json_status_t status = json_parser_complete(&handle);
    if (status != JSON_STATUS_OK) {
        unsigned char *errmsg = json_parser_get_error(&handle, 0, NULL, 0);
        PLUGIN_ERROR("json_parse_complete failed: %s", (const char *)errmsg);
        json_parser_free_error(errmsg);
        json_parser_free (&handle);
        return -1;
    }
    json_parser_free(&handle);

    close(fd);

    return 0;
}

typedef enum {
    KEA_CONFIG_JSON_NONE,
    KEA_CONFIG_JSON_ARGS,
    KEA_CONFIG_JSON_ARGS_DHCP4,
    KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4,
    KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY,
    KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_ID,
    KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_SUBNET,
    KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_POOLS,
    KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_POOLS_ARRAY,
    KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_POOLS_ARRAY_POOL,
    KEA_CONFIG_JSON_ARGS_DHCP6,
    KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6,
    KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY,
    KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_ID,
    KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_SUBNET,
    KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_POOLS,
    KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_POOLS_ARRAY,
    KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_POOLS_ARRAY_POOL,
    KEA_CONFIG_JSON_ARGS_HASH,
} kea_json_config_key_t;

typedef struct {
    kea_json_config_key_t stack[JSON_MAX_DEPTH];
    size_t depth;
    kea_server_t kind;
    kea_subnet_t *subnet;
    kea_t *kea;
} kea_json_config_ctx_t;

static bool kea_json_config_number(void *ctx, const char *str, size_t str_len)
{
    kea_json_config_ctx_t *sctx = ctx;

    if (sctx->depth == 5) {
        if (sctx->stack[4] == KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_ID) {
            kea_subnet_set_id(sctx->subnet, str, str_len);
        } else if (sctx->stack[4] == KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_ID) {
            kea_subnet_set_id(sctx->subnet, str, str_len);
        }
    }

    return true;
}

static bool kea_json_config_string(void *ctx, const char *str, size_t str_len)
{
    kea_json_config_ctx_t *sctx = ctx;

    switch(sctx->depth) {
    case 2:
        if (sctx->stack[1] == KEA_CONFIG_JSON_ARGS_HASH) {
            sstrnncpy(sctx->kea->config_hash, CONFIG_HASH_SIZE, str, str_len);
        }
        break;
    case 5:
        if (sctx->stack[4] == KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_ID) {
            kea_subnet_set_id(sctx->subnet, str, str_len);
        } else if (sctx->stack[4] == KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_SUBNET) {
            kea_subnet_set_subnet(sctx->subnet, str, str_len);
        } else if (sctx->stack[4] == KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_ID) {
            kea_subnet_set_id(sctx->subnet, str, str_len);
        } else if (sctx->stack[4] == KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_SUBNET) {
            kea_subnet_set_subnet(sctx->subnet, str, str_len);
        }
        break;
    case 7:
        if (sctx->stack[5] == KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_POOLS_ARRAY) {
            if (sctx->stack[6] == KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_POOLS_ARRAY_POOL) {
                kea_subnet_add_pool(sctx->subnet, str, str_len);
            }
        } else if (sctx->stack[5] == KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_POOLS_ARRAY) {
            if (sctx->stack[6] == KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_POOLS_ARRAY_POOL) {
                kea_subnet_add_pool(sctx->subnet, str, str_len);
            }
        }
        break;
    }

    return true;
}

static bool kea_json_config_map_key(void * ctx, const char *ukey, size_t key_len)
{
    kea_json_config_ctx_t *sctx = ctx;

    switch(sctx->depth) {
    case 1:
        if ((key_len == 9) && (strncmp(ukey, "arguments", key_len) == 0)) {
            sctx->stack[0] = KEA_CONFIG_JSON_ARGS;
        } else {
            sctx->stack[1] = KEA_CONFIG_JSON_NONE;
        }
        break;
    case 2:
        if (sctx->stack[0] == KEA_CONFIG_JSON_ARGS) {
            if ((key_len == 5) && (strncmp(ukey, "Dhcp4", key_len) == 0)) {
                sctx->stack[1] = KEA_CONFIG_JSON_ARGS_DHCP4;
                sctx->kind = KEA_DHCP4;
            } else if ((key_len == 5) && (strncmp(ukey, "Dhcp6", key_len) == 0)) {
                sctx->stack[1] = KEA_CONFIG_JSON_ARGS_DHCP6;
                sctx->kind = KEA_DHCP6;
            } else if ((key_len == 4) && (strncmp(ukey, "hash", key_len) == 0)) {
                sctx->stack[1] = KEA_CONFIG_JSON_ARGS_HASH;
            } else {
                sctx->stack[1] = KEA_CONFIG_JSON_NONE;
            }
        } else {
            sctx->stack[1] = KEA_CONFIG_JSON_NONE;
        }
        break;
    case 3:
        switch(sctx->stack[1]) {
        case KEA_CONFIG_JSON_ARGS_DHCP4:
            if ((key_len == 7) && (strncmp(ukey, "subnet4", key_len) == 0)) {
                sctx->stack[2] = KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4;
            } else {
                sctx->stack[2] = KEA_CONFIG_JSON_NONE;
            }
            break;
        case KEA_CONFIG_JSON_ARGS_DHCP6:
            if ((key_len == 7) && (strncmp(ukey, "subnet6", key_len) == 0)) {
                sctx->stack[2] = KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6;
            } else {
                sctx->stack[2] = KEA_CONFIG_JSON_NONE;
            }
            break;
        default:
            sctx->stack[2] = KEA_CONFIG_JSON_NONE;
            break;
        }
        break;
    case 5:
        switch(sctx->stack[3]) {
        case KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY:
            if ((key_len == 2) && (strncmp(ukey, "id", key_len) == 0)) {
                sctx->stack[4] = KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_ID;
            } else if ((key_len == 6) && (strncmp(ukey, "subnet", key_len) == 0)) {
                sctx->stack[4] = KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_SUBNET;
            } else if ((key_len == 5) && (strncmp(ukey, "pools", key_len) == 0)) {
                sctx->stack[4] = KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_POOLS;
            } else {
                sctx->stack[4] = KEA_CONFIG_JSON_NONE;
            }
            break;
        case KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY:
            if ((key_len == 2) && (strncmp(ukey, "id", key_len) == 0)) {
                sctx->stack[4] = KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_ID;
            } else if ((key_len == 6) && (strncmp(ukey, "subnet", key_len) == 0)) {
                sctx->stack[4] = KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_SUBNET;
            } else if ((key_len == 5) && (strncmp(ukey, "pools", key_len) == 0)) {
                sctx->stack[4] = KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_POOLS;
            } else {
                sctx->stack[4] = KEA_CONFIG_JSON_NONE;
            }
            break;
        default:
            sctx->stack[4] = KEA_CONFIG_JSON_NONE;
            break;
        }
        break;
    case 7:
        switch(sctx->stack[5]) {
        case KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_POOLS_ARRAY:
            if ((key_len == 4) && (strncmp(ukey, "pool", key_len) == 0)) {
                sctx->stack[6] = KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_POOLS_ARRAY_POOL;
            } else {
                sctx->stack[6] = KEA_CONFIG_JSON_NONE;
            }
            break;
        case KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_POOLS_ARRAY:
            if ((key_len == 4) && (strncmp(ukey, "pool", key_len) == 0)) {
                sctx->stack[6] = KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_POOLS_ARRAY_POOL;
            } else {
                sctx->stack[6] = KEA_CONFIG_JSON_NONE;
            }
            break;
        default:
            sctx->stack[6] = KEA_CONFIG_JSON_NONE;
            break;
        }
        break;
    }

    return true;
}

static bool kea_json_config_start_map(void *ctx)
{
    kea_json_config_ctx_t *sctx = ctx;
    sctx->depth++;
    if (sctx->depth >= JSON_MAX_DEPTH)
        return false;

    if (sctx->depth == 5) {
        switch(sctx->stack[3]) {
        case KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY:
            sctx->subnet = calloc(1, sizeof(*sctx->subnet));
            if (sctx->subnet == NULL) {
                PLUGIN_ERROR("calloc failed.");
                return false;
            }
            break;
        case KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY:
            sctx->subnet = calloc(1, sizeof(*sctx->subnet));
            if (sctx->subnet == NULL) {
                PLUGIN_ERROR("calloc failed.");
                return false;
            }
            break;
        default:
            sctx->stack[4] = KEA_CONFIG_JSON_NONE;
            break;
        }
    } else {
        sctx->stack[sctx->depth-1] = KEA_CONFIG_JSON_NONE;
    }

    return true;
}

static bool kea_json_config_end_map(void *ctx)
{
    kea_json_config_ctx_t *sctx = ctx;

    if (sctx->depth == 5) {
        if ((sctx->stack[3] == KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY) ||
            (sctx->stack[3] == KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY)) {
            kea_config_add_subnet(sctx->kea, sctx->subnet);
            sctx->subnet = NULL;
        }
    }

    sctx->depth--;
    if (sctx->depth > 0) {
        sctx->stack[sctx->depth-1] = KEA_CONFIG_JSON_NONE;
    } else {
        sctx->depth = 0;
    }

    return true;
}

static bool kea_json_config_start_array(void *ctx)
{
    kea_json_config_ctx_t *sctx = ctx;
    sctx->depth++;
    if (sctx->depth >= JSON_MAX_DEPTH)
        return false;

    if (sctx->depth == 4) {
        if (sctx->stack[2] == KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4) {
            sctx->stack[3] = KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY;
        } else if (sctx->stack[2] == KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6) {
            sctx->stack[3] = KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY;
        } else {
            sctx->stack[3] = KEA_CONFIG_JSON_NONE;
        }
    } else if (sctx->depth == 6) {
        if (sctx->stack[4] == KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_POOLS) {
            sctx->stack[5] = KEA_CONFIG_JSON_ARGS_DHCP4_SUBNET4_ARRAY_POOLS_ARRAY;
        } else if (sctx->stack[4] == KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_POOLS) {
            sctx->stack[5] = KEA_CONFIG_JSON_ARGS_DHCP6_SUBNET6_ARRAY_POOLS_ARRAY;
        } else {
            sctx->stack[5] = KEA_CONFIG_JSON_NONE;
        }
    } else {
        sctx->stack[sctx->depth-1] = KEA_CONFIG_JSON_NONE;
    }

    return true;
}

static bool kea_json_config_end_array(void *ctx)
{
    kea_json_config_ctx_t *sctx = ctx;

    sctx->depth--;
    if (sctx->depth > 0) {
        sctx->stack[sctx->depth-1] = KEA_CONFIG_JSON_NONE;
    } else {
        sctx->depth = 0;
    }

    return true;
}

static json_callbacks_t kea_json_config_callbacks = {
    .json_null        = NULL,
    .json_boolean     = NULL,
    .json_integer     = NULL,
    .json_double      = NULL,
    .json_number      = kea_json_config_number,
    .json_string      = kea_json_config_string,
    .json_start_map   = kea_json_config_start_map,
    .json_map_key     = kea_json_config_map_key,
    .json_end_map     = kea_json_config_end_map,
    .json_start_array = kea_json_config_start_array,
    .json_end_array   = kea_json_config_end_array,
};

static int kea_read_config(kea_t *kea)
{
    int fd = kea_cmd (kea, "{\"command\": \"config-get\"}");
    if (fd < 0)
        return -1;

    kea_json_config_ctx_t ctx = {0};
    ctx.kea = kea;

    json_parser_t handle;
    json_parser_init(&handle, 0, &kea_json_config_callbacks, &ctx);

    char buffer[4096];
    ssize_t len = 0;
    while ((len = read(fd, buffer, sizeof(buffer))) > 0) {
        json_status_t status = json_parser_parse(&handle, (unsigned char *)buffer, len);
        if (status != JSON_STATUS_OK)
            break;
    }

    json_status_t status = json_parser_complete(&handle);
    if (status != JSON_STATUS_OK) {
        unsigned char *errmsg = json_parser_get_error(&handle, 0, NULL, 0);
        PLUGIN_ERROR("json_parse_complete failed: %s", (const char *)errmsg);
        free(ctx.subnet);
        json_parser_free_error(errmsg);
        json_parser_free (&handle);
        return -1;
    }
    json_parser_free(&handle);

    close(fd);

    kea->kind = ctx.kind;

    return 0;
}

typedef enum {
    KEA_CONFIG_HASH_JSON_NONE,
    KEA_CONFIG_HASH_JSON_ARGS,
    KEA_CONFIG_HASH_JSON_ARGS_HASH,
} kea_json_config_hash_key_t;

typedef struct {
    kea_json_config_hash_key_t stack[JSON_MAX_DEPTH];
    size_t depth;
    char *hash;
} kea_json_config_hash_ctx_t;

static bool kea_json_config_hash_string(void *ctx, const char *str, size_t str_len)
{
    kea_json_config_hash_ctx_t *sctx = ctx;
    if (sctx == NULL)
        return false;

    if ((sctx->depth == 2) && (sctx->stack[1] == KEA_CONFIG_HASH_JSON_ARGS_HASH))
        sstrnncpy(sctx->hash, CONFIG_HASH_SIZE, str, str_len);

    return true;
}

static bool kea_json_config_hash_map_key(void * ctx, const char *ukey, size_t key_len)
{
    kea_json_config_hash_ctx_t *sctx = ctx;
    if (sctx == NULL)
        return false;

    switch(sctx->depth) {
    case 1:
        if ((key_len == 9) && (strncmp(ukey, "arguments", key_len) == 0)) {
            sctx->stack[0] = KEA_CONFIG_HASH_JSON_ARGS;
        } else {
            sctx->stack[0] = KEA_CONFIG_HASH_JSON_NONE;
        }
        break;
    case 2:
        if ((key_len == 4) && (strncmp(ukey, "hash", key_len) == 0)) {
            sctx->stack[1] = KEA_CONFIG_HASH_JSON_ARGS_HASH;
        } else {
            sctx->stack[1] = KEA_CONFIG_HASH_JSON_NONE;
        }
        break;
    }

    return true;
}

static bool kea_json_config_hash_start_map(void *ctx)
{
    kea_json_config_hash_ctx_t *sctx = ctx;
    sctx->depth++;
    if (sctx->depth >= JSON_MAX_DEPTH)
        return false;
    sctx->stack[sctx->depth] = KEA_CONFIG_HASH_JSON_NONE;
    return true;
}

static bool kea_json_config_hash_end_map(void *ctx)
{
    kea_json_config_hash_ctx_t *sctx = ctx;

    sctx->depth--;
    if (sctx->depth > 0) {
        sctx->stack[sctx->depth-1] = KEA_CONFIG_HASH_JSON_NONE;
    } else {
        sctx->depth = 0;
    }

    return true;
}

static json_callbacks_t kea_json_config_hash_callbacks = {
    .json_null        = NULL,
    .json_boolean     = NULL,
    .json_integer     = NULL,
    .json_double      = NULL,
    .json_number      = NULL,
    .json_string      = kea_json_config_hash_string,
    .json_start_map   = kea_json_config_hash_start_map,
    .json_map_key     = kea_json_config_hash_map_key,
    .json_end_map     = kea_json_config_hash_end_map,
    .json_start_array = NULL,
    .json_end_array   = NULL,
};

static int kea_read_config_hash(kea_t *kea, char hash[CONFIG_HASH_SIZE])
{
    int fd = kea_cmd (kea, "{\"command\": \"config-hash-get\"}");
    if (fd < 0)
        return -1;

    kea_json_config_hash_ctx_t ctx = {0};
    ctx.hash = hash;

    json_parser_t handle;
    json_parser_init(&handle, 0, &kea_json_config_hash_callbacks, &ctx);

    char buffer[4096];
    ssize_t len = 0;
    while ((len = read(fd, buffer, sizeof(buffer))) > 0) {
        json_status_t status = json_parser_parse(&handle, (unsigned char *)buffer, len);
        if (status != JSON_STATUS_OK)
            break;
    }

    json_status_t status = json_parser_complete(&handle);
    if (status != JSON_STATUS_OK) {
        unsigned char *errmsg = json_parser_get_error(&handle, 0, NULL, 0);
        PLUGIN_ERROR("json_parse_complete failed: %s", (const char *)errmsg);
        json_parser_free_error(errmsg);
        json_parser_free (&handle);
        return -1;
    }
    json_parser_free(&handle);

    close(fd);

    return 0;
}

static int kea_read (user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    kea_t *kea = ud->data;

    int status = 0;

    bool read_config = true;
    if (kea->config_hash[0] != '\0') {
        char hash[CONFIG_HASH_SIZE];
        hash[0] = '\0';
        kea_read_config_hash(kea, hash);
        if (strcmp(hash, kea->config_hash) == 0)
            read_config = false;
    }

    if (read_config) {
        kea_config_reset(kea->subnets);
        kea_read_config(kea);
    }

    cdtime_t when = cdtime();

    kea_read_stats(kea);

    metric_family_append(&kea->fams[FAM_KEA_UP], VALUE_GAUGE(status == 0 ? 1 : 0),
                         &kea->labels, NULL);

    plugin_dispatch_metric_family_array_filtered(kea->fams, FAM_KEA_MAX, kea->filter, when);

    return 0;
}

static int kea_config_instance(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("'instance' blocks need exactly one string argument.");
        return -1;
    }

    kea_t *kea = calloc(1, sizeof(*kea));
    if (kea == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    memcpy(kea->fams, fams_kea, FAM_KEA_MAX * sizeof(fams_kea[0]));

    kea->instance = strdup(ci->values[0].value.string);
    if (kea->instance == NULL) {
        PLUGIN_ERROR("strdup failed.");
        free(kea);
        return -1;
    }

    cdtime_t interval = 0;
    int status = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("socket-path", child->key) == 0) {
            status = cf_util_get_string(child, &kea->socketpath);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &kea->timeout);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &kea->labels);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &kea->filter);
        } else {
            PLUGIN_WARNING("Option '%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (kea->socketpath == NULL) {
        PLUGIN_WARNING("'socket-path' missing in 'Instance' block.");
        status = -1;
    }

    if (interval == 0)
        interval = plugin_get_interval();
    if (kea->timeout == 0)
        kea->timeout = interval;

    if (status != 0) {
        kea_free(kea);
        return status;
    }

    label_set_add(&kea->labels, true, "instance", kea->instance);

    return plugin_register_complex_read("kea", kea->instance, kea_read, interval,
                                        &(user_data_t){.data = kea, .free_func = kea_free});
}

static int kea_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = kea_config_instance(child);
        } else {
            PLUGIN_WARNING("Option '%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("kea", kea_config);
}
