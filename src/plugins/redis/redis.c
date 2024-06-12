// SPDX-License-Identifier: GPL-2.0-or-late
// SPDX-FileCopyrightText: Copyright (C) 2010 Andrés J. Díaz
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Andrés J. Díaz <ajdiaz at connectical.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <hiredis/hiredis.h>
#include <sys/time.h>

#include "plugins/redis/redis_fams.h"
#include "plugins/redis/redis_info.h"

#define REDIS_DEF_HOST "localhost"
#define REDIS_DEF_PASSWD ""
#define REDIS_DEF_PORT 6379
#define REDIS_DEF_TIMEOUT_SEC 2
#define REDIS_DEF_DB_COUNT 256
#define MAX_REDIS_VAL_SIZE 256

struct redis_query_s;
typedef struct redis_query_s redis_query_t;
struct redis_query_s {
    char *query;
    char *metric;
    metric_type_t type;
    label_set_t labels;
    int database;
    redis_query_t *next;
};

typedef struct {
    char *name;
    char *host;
    int port;
    char *socket;
    char *passwd;
    struct timeval timeout;
    label_set_t labels;
    plugin_filter_t *filter;
    redisContext *redisContext;
    redis_query_t *queries;
    metric_family_t fams[FAM_REDIS_MAX];
} redis_node_t;

static void redis_instance_free(void *arg)
{
    redis_node_t *rn = arg;
    if (rn == NULL)
        return;

    redis_query_t *rq = rn->queries;
    while (rq != NULL) {
        redis_query_t *next = rq->next;
        free(rq->query);
        free(rq->metric);
        label_set_reset(&rq->labels);
        free(rq);
        rq = next;
    }

    if (rn->redisContext)
        redisFree(rn->redisContext);
    free(rn->name);
    free(rn->host);
    free(rn->socket);
    free(rn->passwd);
    label_set_reset(&rn->labels);
    plugin_filter_free(rn->filter);
    free(rn);
}

static void *c_redisCommand(redis_node_t *rn, const char *format, ...)
{
    redisContext *c = rn->redisContext;

    if (unlikely(c == NULL))
        return NULL;

    va_list ap;
    va_start(ap, format);
    void *reply = redisvCommand(c, format, ap);
    va_end(ap);

    if (unlikely(reply == NULL)) {
        PLUGIN_ERROR("Connection error: %s", c->errstr);
        redisFree(rn->redisContext);
        rn->redisContext = NULL;
    }

    return reply;
}

static int redis_handle_query(redis_node_t *rn, redis_query_t *rq)
{
    redisReply *rr;
    rr = c_redisCommand(rn, "SELECT %d", rq->database);
    if (unlikely(rr == NULL)) {
        PLUGIN_WARNING("unable to switch to database '%d' on node '%s'.",
                        rq->database, rn->name);
        return -1;
    }

    rr = c_redisCommand(rn, rq->query);
    if (unlikely(rr == NULL)) {
        PLUGIN_WARNING("unable to carry out query '%s'.", rq->query);
        return -1;
    }

    value_t value = {0};
    switch (rr->type) {
    case REDIS_REPLY_INTEGER:
        if (rq->type == METRIC_TYPE_COUNTER) {
            value = VALUE_COUNTER(rr->integer);
        } else {
            value = VALUE_GAUGE(rr->integer);
        }
        break;
    case REDIS_REPLY_STRING:
        if (rq->type == METRIC_TYPE_GAUGE) {
            double ret_value = 0;
            if (parse_double(rr->str, &ret_value) == -1) {
                PLUGIN_WARNING("Query '%s': Unable to parse value.", rq->query);
                freeReplyObject(rr);
                return -1;
            }
            value = VALUE_GAUGE(ret_value);
        } else if (rq->type == METRIC_TYPE_COUNTER) {
            uint64_t ret_value = 0;
            if (parse_uinteger(rr->str, &ret_value) == -1) {
                PLUGIN_WARNING("Query '%s': Unable to parse value.", rq->query);
                freeReplyObject(rr);
                return -1;
            }
            value = VALUE_COUNTER(ret_value);
        }
        break;
    case REDIS_REPLY_ERROR:
        PLUGIN_WARNING("Query '%s' failed: %s.", rq->query, rr->str);
        freeReplyObject(rr);
        return -1;
    case REDIS_REPLY_ARRAY:
        PLUGIN_WARNING("Query '%s' should return string or integer. Arrays are not supported.",
                        rq->query);
        freeReplyObject(rr);
        return -1;
    default:
        PLUGIN_WARNING("Query '%s': Cannot coerce redis type (%i).", rq->query, rr->type);
        freeReplyObject(rr);
        return -1;
    }

    metric_family_t fam = {
        .name = rq->metric,
        .type = rq->type,
    };
    metric_family_append(&fam, value, &rq->labels, NULL);

    plugin_dispatch_metric_family_filtered(&fam, rn->filter, 0);

    freeReplyObject(rr);
    return 0;
}

static void redis_check_connection(redis_node_t *rn)
{
    if (rn->redisContext)
        return;

    redisContext *rh;
    if (rn->socket != NULL)
        rh = redisConnectUnixWithTimeout(rn->socket, rn->timeout);
    else
        rh = redisConnectWithTimeout(rn->host, rn->port, rn->timeout);

    if (unlikely(rh == NULL)) {
        PLUGIN_ERROR("can't allocate redis context");
        return;
    }
    if (unlikely(rh->err)) {
        if (rn->socket)
            PLUGIN_ERROR("unable to connect to node '%s' (%s): %s.", rn->name,
                        rn->socket, rh->errstr);
        else
            PLUGIN_ERROR("unable to connect to node '%s' (%s:%d): %s.",
                        rn->name, rn->host, rn->port, rh->errstr);
        redisFree(rh);
        return;
    }

    rn->redisContext = rh;

    if (rn->passwd) {
        PLUGIN_DEBUG("authenticating node '%s' passwd(%s).", rn->name, rn->passwd);

        redisReply *rr = c_redisCommand(rn, "AUTH %s", rn->passwd);
        if (unlikely(rr == NULL)) {
            PLUGIN_WARNING("unable to authenticate on node '%s'.", rn->name);
            return;
        }

        if (unlikely(rr->type != REDIS_REPLY_STATUS)) {
            PLUGIN_WARNING("invalid authentication on node '%s'.", rn->name);
            freeReplyObject(rr);
            redisFree(rn->redisContext);
            rn->redisContext = NULL;
            return;
        }

        freeReplyObject(rr);
    }
    return;
}

static int redis_read_info_errorstat(redis_node_t *rn, char *key, char *value)
{
    char *error = key + strlen("errorstat_");

    if (strncmp(value, "count=", strlen("count=")) != 0)
        return -1;
    char *counts = value + strlen("count=");

    metric_family_append(&rn->fams[FAM_REDIS_ERRORS], VALUE_COUNTER(atoll(counts)), &rn->labels,
                         &(label_pair_const_t){.name="error", .value=error}, NULL);

    return 0;
}

static int redis_read_info_master(redis_node_t *rn, char *value)
{
/* "master%d:name=%s,status=%s,address=%s:%d,slaves=%lu,sentinels=%lu\r\n" */
    char *master_name = value;
    if (unlikely(strncmp(master_name, "name=", strlen("name=")) != 0))
        return -1;
    master_name += strlen("name=");

    char *master_status = strchr(master_name, ',');
    if (unlikely(master_status == NULL))
        return -1;
    *(master_status++) = '\0';

    if (unlikely(strncmp(master_status, "status=", strlen("status=")) != 0))
        return -1;
    master_status += strlen("status=");

    char *master_address = strchr(master_status, ',');
    if (unlikely(master_address == NULL))
        return -1;
    *(master_address++) = '\0';

    if (unlikely(strncmp(master_address, "address=", strlen("address=")) != 0))
        return -1;
    master_address += strlen("address=");

    char *master_slaves = strchr(master_address, ',');
    if (unlikely(master_slaves == NULL))
        return -1;
    *(master_slaves++) = '\0';

    if (unlikely(strncmp(master_slaves, "slaves=", strlen("slaves=")) != 0))
        return -1;
    master_slaves += strlen("slaves=");

    char *master_sentinels = strchr(master_slaves, ',');
    if (unlikely(master_sentinels == NULL))
        return -1;
    *(master_sentinels++) = '\0';

    if (unlikely(strncmp(master_sentinels, "sentinels=", strlen("sentinels=")) != 0))
        return -1;
    master_sentinels += strlen("sentinels=");

    state_t states[] = {
        { .name = "ok",     .enabled = false },
        { .name = "odown",  .enabled = false },
        { .name = "sdown",  .enabled = false },
    };
    state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
    state_set_enable(set, master_status);

    metric_family_append(&rn->fams[FAM_REDIS_SENTINEL_MASTER_STATUS],
                         VALUE_STATE_SET(set), &rn->labels,
                         &(label_pair_const_t){.name="master_address", .value=master_address},
                         &(label_pair_const_t){.name="master_name", .value=master_name},
                         NULL);

    metric_family_append(&rn->fams[FAM_REDIS_SENTINEL_MASTER_SLAVES],
                         VALUE_GAUGE(atoll(master_slaves)), &rn->labels,
                         &(label_pair_const_t){.name="master_address", .value=master_address},
                         &(label_pair_const_t){.name="master_name", .value=master_name},
                         NULL);

    metric_family_append(&rn->fams[FAM_REDIS_SENTINEL_MASTER_SENTINELS],
                         VALUE_GAUGE(atoll(master_sentinels)), &rn->labels,
                         &(label_pair_const_t){.name="master_address", .value=master_address},
                         &(label_pair_const_t){.name="master_name", .value=master_name},
                         NULL);

    return 0;
}

static int redis_read_info_slave(redis_node_t *rn, char *value)
{
    char *slave_ip = value;
    if (unlikely(strncmp(slave_ip, "ip=", strlen("ip=")) != 0))
        return -1;
    slave_ip += strlen("ip=");

    char *slave_port = strchr(slave_ip, ',');
    if (unlikely(slave_port == NULL))
        return -1;
    *(slave_port++) = '\0';

    if (unlikely(strncmp(slave_port, "port=", strlen("port=")) != 0))
        return -1;
    slave_port += strlen("port=");

    char *slave_state = strchr(slave_port, ',');
    if (unlikely(slave_state == NULL))
        return -1;
    *(slave_state++) = '\0';

    if (unlikely(strncmp(slave_state, "state=", strlen("state=")) != 0))
        return -1;
    slave_state += strlen("state=");

    char *slave_offset = strchr(slave_state, ',');
    if (unlikely(slave_offset == NULL))
        return -1;
    *(slave_offset++) = '\0';

    if (unlikely(strncmp(slave_offset, "offset=", strlen("offset=")) != 0))
        return -1;
    slave_offset += strlen("offset=");

    char *slave_lag = strchr(slave_offset, ',');
    if (unlikely(slave_lag == NULL))
        return -1;
    *(slave_lag++) = '\0';

    if (unlikely(strncmp(slave_lag, "lag=", strlen("lag=")) != 0))
        return -1;
    slave_lag += strlen("lag=");

    char address[256];
    ssnprintf(address, sizeof(address), "%s:%s", slave_ip, slave_port);

    state_t states[] = {
        { .name = "wait_bgsave", .enabled = false },
        { .name = "send_bulk",   .enabled = false },
        { .name = "online",      .enabled = false },
    };
    state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
    state_set_enable(set, slave_state);

    metric_family_append(&rn->fams[FAM_REDIS_SLAVE_STATE], VALUE_STATE_SET(set), &rn->labels,
                         &(label_pair_const_t){.name="slave_address", .value=address}, NULL);

    metric_family_append(&rn->fams[FAM_REDIS_SLAVE_LAG],
                         VALUE_GAUGE(atoll(slave_lag)), &rn->labels,
                         &(label_pair_const_t){.name="slave_address", .value=address}, NULL);

    metric_family_append(&rn->fams[FAM_REDIS_SLAVE_OFFSET],
                         VALUE_GAUGE(atoll(slave_offset)), &rn->labels,
                         &(label_pair_const_t){.name="slave_address", .value=address}, NULL);
    return 0;
}

static int redis_read_info_cmdstat(redis_node_t *rn, char *key, char *value)
{
    char *command = key + strlen("cmdstat_");

    char *command_calls = value;
    if (unlikely(strncmp(command_calls, "calls=", strlen("calls=")) != 0))
        return -1;
    command_calls += strlen("calls=");

    char *command_usec = strchr(command_calls, ',');
    if (unlikely(command_usec == NULL))
        return -1;
    *(command_usec++) = '\0';

    if (unlikely(strncmp(command_usec, "usec=", strlen("usec=")) != 0))
        return -1;
    command_usec += strlen("usec=");

    char *end= strchr(command_usec, ',');
    if (unlikely(end == NULL))
        return -1;
    *end = '\0';

    metric_family_append(&rn->fams[FAM_REDIS_COMMANDS],
                         VALUE_COUNTER(atoll(command_calls)), &rn->labels,
                         &(label_pair_const_t){.name="cmd", .value=command}, NULL);

    metric_family_append(&rn->fams[FAM_REDIS_COMMANDS_DURATION_SECONDS],
                         VALUE_COUNTER_FLOAT64(((double)atoll(command_usec)) / 1000000.0),
                         &rn->labels,
                         &(label_pair_const_t){.name="cmd", .value=command}, NULL);
    return 0;
}

static int redis_read_info_db(redis_node_t *rn, char *key, char *value)
{
    char *db = key + strlen("db");

    char *db_keys = value;
    if (unlikely(strncmp(db_keys, "keys=", strlen("keys=")) != 0))
        return -1;
    db_keys += strlen("keys=");

    char *db_expires = strchr(db_keys, ',');
    if (unlikely(db_expires == NULL))
        return -1;
    *(db_expires ++) = '\0';

    if (unlikely(strncmp(db_expires, "expires=", strlen("expires=")) != 0))
        return -1;
    db_expires += strlen("expires=");

    char *end= strchr(db_expires, ',');
    if (unlikely(end == NULL))
        return -1;
    *end = '\0';

    metric_family_append(&rn->fams[FAM_REDIS_DB_KEYS],
                         VALUE_GAUGE((double)atoll(db_keys)), &rn->labels,
                         &(label_pair_const_t){.name="db", .value=db}, NULL);

    metric_family_append(&rn->fams[FAM_REDIS_DB_KEYS_EXPIRING],
                         VALUE_GAUGE((double)atoll(db_expires)), &rn->labels,
                         &(label_pair_const_t){.name="db", .value=db}, NULL);

    return 0;
}

static void redis_read_info(redis_node_t *rn)
{
    redisReply *rr = c_redisCommand(rn, "INFO ALL");
    if (unlikely(rr == NULL)) {
        PLUGIN_WARNING("unable to get INFO from node '%s'.", rn->name);
        return;
    }

    char *saveptr = NULL;
    for (char *key = strtok_r(rr->str, "\r\n", &saveptr);
             key != NULL;
             key = strtok_r(NULL, "\r\n", &saveptr)) {

        if (*key == '#') continue;

        char *val = strchr(key, ':');
        if (val == NULL)
            continue;
        *val = '\0';
        size_t key_len = val - key;
        val++;

        const struct redis_info *ri = redis_info_get_key(key, key_len);
        if (ri != NULL) {
            value_t value = {0};

            switch (ri->fam) {
            case FAM_REDIS_MODE: {
                state_set_t set = (state_set_t) {
                    .num = 3,
                    .ptr = (state_t []){
                        {.enabled = false, .name = "cluster"   },
                        {.enabled = false, .name = "sentinel"  },
                        {.enabled = false, .name = "standalone"},
                    }
                };
                state_set_enable(set, val);
                metric_family_append(&rn->fams[ri->fam], VALUE_STATE_SET(set), &rn->labels, NULL);
                continue;
            }   break;
            case FAM_REDIS_ROLE: {
                state_set_t set = (state_set_t) {
                    .num = 2,
                    .ptr = (state_t []){
                        {.enabled = false, .name = "master"},
                        {.enabled = false, .name = "slave" },
                    }
                };
                state_set_enable(set, val);
                metric_family_append(&rn->fams[ri->fam], VALUE_STATE_SET(set), &rn->labels, NULL);
                continue;
            }   break;
            case FAM_REDIS_MASTER_FAILOVER_STATE: {
                state_set_t set = (state_set_t) {
                    .num = 3,
                    .ptr = (state_t []){
                        {.enabled = false, .name = "no-failover"         },
                        {.enabled = false, .name = "failover-in-progress"},
                        {.enabled = false, .name = "waiting-for-sync"    },
                    }
                };
                state_set_enable(set, val);
                metric_family_append(&rn->fams[ri->fam], VALUE_STATE_SET(set), &rn->labels, NULL);
                continue;
            }   break;
            case FAM_REDIS_MEMORY_USED_DATASET_RATIO:
            case FAM_REDIS_MEMORY_USED_PEAK_RATIO:
            {
                size_t val_len = strlen(val);
                if (val_len > 0) {
                    if (val[val_len-1] == '%')
                        val[val_len-1] = '\0';
                    double raw = 0;
                    if (parse_double(val, &raw) != 0) {
                        PLUGIN_WARNING("Unable to parse field '%s'.", key);
                        continue;
                    }
                    value = VALUE_GAUGE(raw);
                } else {
                    continue;
                }
                break;
            }
            case FAM_REDIS_RDB_LAST_BGSAVE_STATUS:
            case FAM_REDIS_AOF_LAST_BGREWRITE_STATUS:
            case FAM_REDIS_AOF_LAST_WRITE_STATUS:
                if (strcmp(val, "ok") == 0)
                    value = VALUE_GAUGE(1);
                else
                    value = VALUE_GAUGE(0);
                break;
            case FAM_REDIS_CPU_SYS_SECONDS:
            case FAM_REDIS_CPU_USER_SECONDS:
            case FAM_REDIS_CPU_SYS_CHILDREN_SECONDS:
            case FAM_REDIS_CPU_USER_CHILDREN_SECONDS:
            case FAM_REDIS_CPU_SYS_MAIN_THREAD_SECONDS:
            case FAM_REDIS_CPU_USER_MAIN_THREAD_SECONDS: {
                double raw = 0;
                if (parse_double(val, &raw) != 0) {
                    PLUGIN_WARNING("Unable to parse field '%s'.", key);
                    continue;
                }
                value = VALUE_COUNTER_FLOAT64(raw);
            }   break;
            default:
                if (rn->fams[ri->fam].type == METRIC_TYPE_GAUGE) {
                    double raw = 0;
                    if (parse_double(val, &raw) != 0) {
                        PLUGIN_WARNING("Unable to parse field '%s'.", key);
                        continue;
                    }
                    value = VALUE_GAUGE(raw);
                } else if (rn->fams[ri->fam].type == METRIC_TYPE_COUNTER) {
                    uint64_t raw = 0;
                    if (parse_uinteger(val, &raw) != 0) {
                        PLUGIN_WARNING("Unable to parse field '%s'.", key);
                        continue;
                    }
                    value = VALUE_COUNTER(raw);
                }
            break;
            }
            metric_family_append(&rn->fams[ri->fam], value, &rn->labels, NULL);
        } else {
            if ((key_len > 10) && (strncmp(key, "errorstat_", strlen("errorstat_")) == 0)) {
                redis_read_info_errorstat(rn, key, val);
            } else if ((key_len > 8) && (strncmp(key, "cmdstat_", strlen("cmdstat_")) == 0)) {
                redis_read_info_cmdstat(rn, key, val);
            } else if ((key_len > 6) && (strncmp(key, "master", strlen("master")) == 0)) {
                redis_read_info_master(rn, val);
            } else if ((key_len > 5) && (strncmp(key, "slave", strlen("slave")) == 0)) {
                redis_read_info_slave(rn, val);
            } else if ((key_len > 2) && (strncmp(key, "db", strlen("db")) == 0)) {
                redis_read_info_db(rn, key, val);
            /* "cluster_"  */
            }
        }
    }

    freeReplyObject(rr);

    plugin_dispatch_metric_family_array_filtered(rn->fams, FAM_REDIS_MAX, rn->filter, 0);
}

static int redis_read(user_data_t *user_data)
{
    redis_node_t *rn = user_data->data;

#ifdef NCOLLECTD_DEBUG
    if (rn->socket)
        PLUGIN_DEBUG("querying info from node '%s' (%s).", rn->name, rn->socket);
    else
        PLUGIN_DEBUG("querying info from node '%s' (%s:%d).", rn->name, rn->host, rn->port);
#endif

    redis_check_connection(rn);
    if (!rn->redisContext) { /* no connection */
        metric_family_append(&rn->fams[FAM_REDIS_UP], VALUE_GAUGE(0), &rn->labels, NULL);
        plugin_dispatch_metric_family_filtered(&rn->fams[FAM_REDIS_UP], rn->filter,  0);
        return 0;
    }

    metric_family_append(&rn->fams[FAM_REDIS_UP], VALUE_GAUGE(1), &rn->labels, NULL);
    redis_read_info(rn);

    if (!rn->redisContext) /* connection lost */
        return 0;

    for (redis_query_t *rq = rn->queries; rq != NULL; rq = rq->next) {
        redis_handle_query(rn, rq);
        if (!rn->redisContext) /* connection lost */
            return 0;
    }

    return 0;
}

static redis_query_t *redis_config_query(config_item_t *ci)
{
    redis_query_t *rq = calloc(1, sizeof(*rq));
    if (rq == NULL) {
        PLUGIN_ERROR("calloc failed adding redis_query.");
        return NULL;
    }
    int status = cf_util_get_string(ci, &rq->query);
    if (status != 0) {
        PLUGIN_ERROR("query missing");
        free(rq);
        return NULL;
    }

    rq->database = 0;
    rq->type = METRIC_TYPE_UNKNOWN;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *option = ci->children + i;

        if (strcasecmp("metric", option->key) == 0) {
            status = cf_util_get_string(option, &rq->metric);
        } else if (strcasecmp("Type", option->key) == 0) {
            status = cf_util_get_metric_type(option, &rq->type);
        } else if (strcasecmp("label", option->key) == 0) {
            status = cf_util_get_label(option, &rq->labels);
        } else if (strcasecmp("database", option->key) == 0) {
            status = cf_util_get_int(option, &rq->database);
            if (rq->database < 0) {
                PLUGIN_WARNING("The 'Database' option must be positive "
                                "integer or zero");
                status = -1;
            }
        } else {
            PLUGIN_WARNING("unknown configuration option: %s", option->key);
            status = -1;
        }
        if (status != 0)
            goto err;
    }
    return rq;

err:
    free(rq->query);
    free(rq->metric);
    label_set_reset(&rq->labels);
    free(rq);
    return NULL;
}

static int redis_config_instance(config_item_t *ci)
{
    redis_node_t *rn = calloc(1, sizeof(*rn));
    if (rn == NULL) {
        PLUGIN_ERROR("calloc failed adding instance.");
        return ENOMEM;
    }

    memcpy(rn->fams, fams_redis, FAM_REDIS_MAX * sizeof(fams_redis[0]));

    rn->port = REDIS_DEF_PORT;
    rn->timeout.tv_sec = REDIS_DEF_TIMEOUT_SEC;

    rn->host = strdup(REDIS_DEF_HOST);
    if (rn->host == NULL) {
        PLUGIN_ERROR("strdup failed adding instance.");
        free(rn);
        return ENOMEM;
    }

    int status = cf_util_get_string(ci, &rn->name);
    if (status != 0) {
        free(rn->host);
        free(rn);
        return status;
    }

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *option = ci->children + i;

        if (strcasecmp("host", option->key) == 0) {
            status = cf_util_get_string(option, &rn->host);
        } else if (strcasecmp("port", option->key) == 0) {
            status = cf_util_get_port_number(option, &rn->port);
        } else if (strcasecmp("socket", option->key) == 0) {
            status = cf_util_get_string(option, &rn->socket);
        } else if (strcasecmp("query", option->key) == 0) {
            redis_query_t *rq = redis_config_query(option);
            if (rq == NULL) {
                status = 1;
            } else {
                rq->next = rn->queries;
                rn->queries = rq;
            }
        } else if (strcasecmp("timeout", option->key) == 0) {
            cdtime_t timeout;
            status = cf_util_get_cdtime(option, &timeout);
            if (status == 0)
                rn->timeout = CDTIME_T_TO_TIMEVAL(timeout);
        } else if (strcasecmp("password", option->key) == 0) {
            status = cf_util_get_string(option, &rn->passwd);
        } else if (strcasecmp("interval", option->key) == 0) {
            status = cf_util_get_cdtime(option, &interval);
        } else if (strcasecmp("label", option->key) == 0) {
            status = cf_util_get_label(option, &rn->labels);
        } else if (strcasecmp("filter", option->key) == 0) {
            status = plugin_filter_configure(option, &rn->filter);
        }  else {
            PLUGIN_WARNING("Option '%s' not allowed inside a 'instance' block.", option->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status == 0) {
        redis_query_t *rq = rn->queries;
        while (rq != NULL) {
            for (size_t i = 0; i < rn->labels.num; i++) {
                label_set_add(&rq->labels, false, rn->labels.ptr[i].name, rn->labels.ptr[i].value);
            }
            rq = rq->next;
        }
    }

    if (status != 0) {
        redis_instance_free(rn);
        return status;
    }

    label_set_add(&rn->labels, true, "instance", rn->name);

    return plugin_register_complex_read("redis", rn->name, redis_read, interval,
                                        &(user_data_t){.data=rn, .free_func=redis_instance_free});
}

static int redis_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *option = ci->children + i;

        if (strcasecmp("instance", option->key) == 0) {
            status = redis_config_instance(option);
        } else {
            PLUGIN_ERROR("Option '%s' not allowed in redis configuration.", option->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("redis", redis_config);
}
