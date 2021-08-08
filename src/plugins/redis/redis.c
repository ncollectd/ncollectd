/**
 * collectd - src/redis.c, based on src/memcached.c
 * Copyright (C) 2010       Andrés J. Díaz <ajdiaz@connectical.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Authors:
 *   Andrés J. Díaz <ajdiaz@connectical.com>
 **/

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#include <hiredis/hiredis.h>
#include <sys/time.h>

#define REDIS_DEF_HOST "localhost"
#define REDIS_DEF_PASSWD ""
#define REDIS_DEF_PORT 6379
#define REDIS_DEF_TIMEOUT_SEC 2
#define REDIS_DEF_DB_COUNT 256
#define MAX_REDIS_VAL_SIZE 256

#include "redis_fams.h"
#include "redis_info.h"

/* Redis plugin configuration example:
 *
 * <Plugin redis>
 *   <Instance "mynode">
 *     Host "localhost"
 *     Port "6379"
 *     Timeout 2
 *     Password "foobar"
 *   </Instance>
 * </Plugin>
 */

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

  redisContext *redisContext;

  redis_query_t *queries;

  metric_family_t fams[FAM_REDIS_MAX];
} redis_node_t;

static int redis_read(user_data_t *user_data);

static void redis_instance_free(void *arg)
{
  redis_node_t *rn = arg;
  if (rn == NULL)
    return;

  redis_query_t *rq = rn->queries;
  while (rq != NULL) {
    redis_query_t *next = rq->next;
    sfree(rq->query);
    sfree(rq->metric);
    label_set_reset(&rq->labels);
    sfree(rq);
    rq = next;
  }

  if (rn->redisContext)
    redisFree(rn->redisContext);
  sfree(rn->name);
  sfree(rn->host);
  sfree(rn->socket);
  sfree(rn->passwd);
  label_set_reset(&rn->labels);
  sfree(rn);
}

static redis_query_t *redis_config_query(oconfig_item_t *ci)
{
  redis_query_t *rq = calloc(1, sizeof(*rq));
  if (rq == NULL) {
    ERROR("redis plugin: calloc failed adding redis_query.");
    return NULL;
  }
  int status = cf_util_get_string(ci, &rq->query);
  if (status != 0) {
    ERROR("redis plugin: query missing");
    sfree(rq);
    return NULL;
  }

  rq->database = 0;
  rq->type = METRIC_TYPE_UNKNOWN;

  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *option = ci->children + i;

    if (strcasecmp("Metric", option->key) == 0) {
      status = cf_util_get_string(option, &rq->metric);
    } else if (strcasecmp("Type", option->key) == 0) {
      status = cf_util_get_metric_type(option, &rq->type);
    } else if (strcasecmp("Label", option->key) == 0) {
      status = cf_util_get_label(option, &rq->labels);
    } else if (strcasecmp("Database", option->key) == 0) {
      status = cf_util_get_int(option, &rq->database);
      if (rq->database < 0) {
        WARNING("redis plugin: The \"Database\" option must be positive "
                "integer or zero");
        status = -1;
      }
    } else {
      WARNING("redis plugin: unknown configuration option: %s", option->key);
      status = -1;
    }
    if (status != 0)
      goto err;
  }
  return rq;

err:
  sfree(rq->query);
  sfree(rq->metric);
  label_set_reset(&rq->labels);
  free(rq);
  return NULL;
}

static int redis_config_instance(oconfig_item_t *ci)
{
  redis_node_t *rn = calloc(1, sizeof(*rn));
  if (rn == NULL) {
    ERROR("redis plugin: calloc failed adding instance.");
    return ENOMEM;
  }

  memcpy(rn->fams, fams_redis, FAM_REDIS_MAX * sizeof(fams_redis[0]));

  rn->port = REDIS_DEF_PORT;
  rn->timeout.tv_sec = REDIS_DEF_TIMEOUT_SEC;

  rn->host = strdup(REDIS_DEF_HOST);
  if (rn->host == NULL) {
    ERROR("redis plugin: strdup failed adding instance.");
    sfree(rn);
    return ENOMEM;
  }

  int status = cf_util_get_string(ci, &rn->name);
  if (status != 0) {
    sfree(rn->host);
    sfree(rn);
    return status;
  }

  cdtime_t interval = 0;
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *option = ci->children + i;

    if (strcasecmp("Host", option->key) == 0)
      status = cf_util_get_string(option, &rn->host);
    else if (strcasecmp("Port", option->key) == 0) {
      status = cf_util_get_port_number(option);
      if (status > 0) {
        rn->port = status;
        status = 0;
      }
    } else if (strcasecmp("Socket", option->key) == 0) {
      status = cf_util_get_string(option, &rn->socket);
    } else if (strcasecmp("Query", option->key) == 0) {
      redis_query_t *rq = redis_config_query(option);
      if (rq == NULL) {
        status = 1;
      } else {
        rq->next = rn->queries;
        rn->queries = rq;
      }
    } else if (strcasecmp("Timeout", option->key) == 0) {
      int timeout;
      status = cf_util_get_int(option, &timeout);
      if (status == 0) {
        rn->timeout.tv_usec = timeout * 1000;
        rn->timeout.tv_sec = rn->timeout.tv_usec / 1000000L;
        rn->timeout.tv_usec %= 1000000L;
      }
    } else if (strcasecmp("Password", option->key) == 0)
      status = cf_util_get_string(option, &rn->passwd);
    else if (strcasecmp("Label", option->key) == 0)
      status = cf_util_get_label(option, &rn->labels);
    else
      WARNING("redis plugin: Option `%s' not allowed inside a `Instance' "
              "block. I'll ignore this option.",
              option->key);

    if (status != 0)
      break;
  }

  if (status != 0) {
    redis_instance_free(rn);
    return status;
  }

  char *cb_name = ssnprintf_alloc("redis/%s", rn->name);

  status = plugin_register_complex_read(
      /* group     = */ "redis",
      /* name      = */ cb_name,
      /* callback  = */ redis_read,
      /* interval  = */ interval,
      &(user_data_t){
          .data = rn,
          .free_func = redis_instance_free,
      });

  sfree(cb_name);
  return status;
}

static int redis_config(oconfig_item_t *ci)
{
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *option = ci->children + i;

    if (strcasecmp("Instance", option->key) == 0)
      redis_config_instance(option);
    else
      WARNING("redis plugin: Option `%s' not allowed in redis"
              " configuration. It will be ignored.",
              option->key);
  }

  return 0;
}

static void *c_redisCommand(redis_node_t *rn, const char *format, ...)
{
  redisContext *c = rn->redisContext;

  if (c == NULL)
    return NULL;

  va_list ap;
  va_start(ap, format);
  void *reply = redisvCommand(c, format, ap);
  va_end(ap);

  if (reply == NULL) {
    ERROR("redis plugin: Connection error: %s", c->errstr);
    redisFree(rn->redisContext);
    rn->redisContext = NULL;
  }

  return reply;
}

static int redis_handle_query(redis_node_t *rn, redis_query_t *rq)
{
  redisReply *rr;
  if ((rr = c_redisCommand(rn, "SELECT %d", rq->database)) == NULL) {
    WARNING("redis plugin: unable to switch to database `%d' on node `%s'.",
            rq->database, rn->name);
    return -1;
  }

  if ((rr = c_redisCommand(rn, rq->query)) == NULL) {
    WARNING("redis plugin: unable to carry out query `%s'.", rq->query);
    return -1;
  }

  value_t val = {0};
  switch (rr->type) {
  case REDIS_REPLY_INTEGER:
    if (rr->type == METRIC_TYPE_COUNTER) {
      val.counter.uinteger = (uint64_t)rr->integer;
    } else {
      val.gauge.real = (double)rr->integer;
    }
    break;
  case REDIS_REPLY_STRING:
    if (rr->type == METRIC_TYPE_GAUGE) {
      if (parse_double(rr->str, &val.gauge.real) == -1) {
        WARNING("redis plugin: Query `%s': Unable to parse value.", rq->query);
        freeReplyObject(rr);
        return -1;
      }
    } else if (rr->type == METRIC_TYPE_COUNTER) {
      if (parse_uinteger(rr->str, &val.counter.uinteger) == -1) {
        WARNING("redis plugin: Query `%s': Unable to parse value.", rq->query);
        freeReplyObject(rr);
        return -1;
      }
    }
    break;
  case REDIS_REPLY_ERROR:
    WARNING("redis plugin: Query `%s' failed: %s.", rq->query, rr->str);
    freeReplyObject(rr);
    return -1;
  case REDIS_REPLY_ARRAY:
    WARNING("redis plugin: Query `%s' should return string or integer. Arrays "
            "are not supported.",
            rq->query);
    freeReplyObject(rr);
    return -1;
  default:
    WARNING("redis plugin: Query `%s': Cannot coerce redis type (%i).",
            rq->query, rr->type);
    freeReplyObject(rr);
    return -1;
  }

//  redis_submit(rn->name, rq->type, (strlen(rq->instance) > 0) ? rq->instance : NULL, val);

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

  if (rh == NULL) {
    ERROR("redis plugin: can't allocate redis context");
    return;
  }
  if (rh->err) {
    if (rn->socket)
      ERROR("redis plugin: unable to connect to node `%s' (%s): %s.", rn->name,
            rn->socket, rh->errstr);
    else
      ERROR("redis plugin: unable to connect to node `%s' (%s:%d): %s.",
            rn->name, rn->host, rn->port, rh->errstr);
    redisFree(rh);
    return;
  }

  rn->redisContext = rh;

  if (rn->passwd) {
    redisReply *rr;

    DEBUG("redis plugin: authenticating node `%s' passwd(%s).", rn->name,
          rn->passwd);

    if ((rr = c_redisCommand(rn, "AUTH %s", rn->passwd)) == NULL) {
      WARNING("redis plugin: unable to authenticate on node `%s'.", rn->name);
      return;
    }

    if (rr->type != REDIS_REPLY_STATUS) {
      WARNING("redis plugin: invalid authentication on node `%s'.", rn->name);
      freeReplyObject(rr);
      redisFree(rn->redisContext);
      rn->redisContext = NULL;
      return;
    }

    freeReplyObject(rr);
  }
  return;
}

static int redis_read_info_errorstat(redis_node_t *rn, metric_t *tmpl, char *key, char *value)
{
  char *error = key + strlen("errorstat_");

  if (strncmp(value, "count=", strlen("count=")) != 0)
    return -1;
  char *counts = value + strlen("count=");

  value_t val = {0};
  val.counter.uinteger = atoll(counts);
  metric_family_append(&rn->fams[FAM_REDIS_ERRORS_TOTAL], "error", error, val, tmpl);

  return 0;
}

static int redis_read_info_master(redis_node_t *rn, metric_t *tmpl, char *key, char *value)
{
  char *master_name = value;
  if (strncmp(master_name, "name=", strlen("name=")) != 0)
    return -1;
  master_name += strlen("name=");

  char *master_status = strchr(master_name, ',');
  if (master_status == NULL)
    return -1;
  *(master_status++) = '\0';

  if (strncmp(master_status, "status=", strlen("status=")) != 0)
    return -1;
  master_status += strlen("status=");

  char *master_address = strchr(master_status, ',');
  if (master_address == NULL)
    return -1;
  *(master_address++) = '\0';

  if (strncmp(master_address, "address=", strlen("address=")) != 0)
    return -1;
  master_address += strlen("address=");

  char *master_slaves = strchr(master_address, ',');
  if (master_slaves == NULL)
    return -1;
  *(master_slaves++) = '\0';

  if (strncmp(master_slaves, "slaves=", strlen("slaves=")) != 0)
    return -1;
  master_slaves += strlen("slaves=");

  char *master_sentinels = strchr(master_slaves, ',');
  if (master_sentinels == NULL)
    return -1;
  *(master_sentinels++) = '\0';

  if (strncmp(master_sentinels, "sentinels=", strlen("sentinels=")) != 0)
    return -1;
  master_sentinels += strlen("sentinels=");

  metric_label_set(tmpl, "master_name", master_name);
  metric_label_set(tmpl, "master_address", master_address);

  value_t val = {0};
  val.gauge.real = strcmp(master_status, "ok") == 0 ? 1 : 0;
  metric_family_append(&rn->fams[FAM_REDIS_SENTINEL_MASTER_STATUS], "master_status", "ok", val, tmpl);

  val.gauge.real = strcmp(master_status, "odown") == 0 ? 1 : 0;
  metric_family_append(&rn->fams[FAM_REDIS_SENTINEL_MASTER_STATUS], "master_status", "odown", val, tmpl);

  val.gauge.real = strcmp(master_status, "sdown") == 0 ? 1 : 0;
  metric_family_append(&rn->fams[FAM_REDIS_SENTINEL_MASTER_STATUS], "master_status", "sdown", val, tmpl);

  val.gauge.real = atoll(master_slaves);
  metric_family_append(&rn->fams[FAM_REDIS_SENTINEL_MASTER_SLAVES], NULL, NULL, val, tmpl);

  val.gauge.real = atoll(master_sentinels);
  metric_family_append(&rn->fams[FAM_REDIS_SENTINEL_MASTER_SENTINELS], NULL, NULL, val, tmpl);

  metric_label_set(tmpl, "master_name", NULL);
  metric_label_set(tmpl, "master_address", NULL);
// "master%d:name=%s,status=%s,address=%s:%d,slaves=%lu,sentinels=%lu\r\n"
  return 0;
}

static int redis_read_info_slave(redis_node_t *rn, metric_t *tmpl, char *key, char *value)
{
  char *slave_ip = value;
  if (strncmp(slave_ip, "ip=", strlen("ip=")) != 0)
    return -1;
  slave_ip += strlen("ip=");

  char *slave_port = strchr(slave_ip, ',');
  if (slave_port == NULL)
    return -1;
  *(slave_port++) = '\0';

  if (strncmp(slave_port, "port=", strlen("port=")) != 0)
    return -1;
  slave_port += strlen("port=");

  char *slave_state = strchr(slave_port, ',');
  if (slave_state == NULL)
    return -1;
  *(slave_state++) = '\0';

  if (strncmp(slave_state, "state=", strlen("state=")) != 0)
    return -1;
  slave_state += strlen("state=");

  char *slave_offset = strchr(slave_state, ',');
  if (slave_offset == NULL)
    return -1;
  *(slave_offset++) = '\0';

  if (strncmp(slave_offset, "offset=", strlen("offset=")) != 0)
    return -1;
  slave_offset += strlen("offset=");

  char *slave_lag = strchr(slave_offset, ',');
  if (slave_lag == NULL)
    return -1;
  *(slave_lag++) = '\0';

  if (strncmp(slave_lag, "lag=", strlen("lag=")) != 0)
    return -1;
  slave_lag += strlen("lag=");

  char address[256];
  ssnprintf(address, sizeof(address), "%s:%s", slave_ip, slave_port);
  metric_label_set(tmpl, "slave_address", address);
 
  value_t val = {0};
  val.gauge.real = strcmp(slave_state, "wait_bgsave") == 0 ? 1 : 0;
  metric_family_append(&rn->fams[FAM_REDIS_SLAVE_STATE], "slave_state", "wait_bgsave", val, tmpl);

  val.gauge.real = strcmp(slave_state, "send_bulk") == 0 ? 1 : 0;
  metric_family_append(&rn->fams[FAM_REDIS_SLAVE_STATE], "slave_state", "send_bulk", val, tmpl);

  val.gauge.real = strcmp(slave_state, "online") == 0 ? 1 : 0;
  metric_family_append(&rn->fams[FAM_REDIS_SLAVE_STATE], "slave_state", "online", val, tmpl);

  val.gauge.real = atoll(slave_lag);
  metric_family_append(&rn->fams[FAM_REDIS_SLAVE_LAG], NULL, NULL, val, tmpl);

  val.gauge.real = atoll(slave_offset);
  metric_family_append(&rn->fams[FAM_REDIS_SLAVE_OFFSET], NULL, NULL, val, tmpl);

  metric_label_set(tmpl, "slave_address", NULL);
  return 0;
}

static int redis_read_info_cmdstat(redis_node_t *rn, metric_t *tmpl, char *key, char *value)
{
  char *command = key + strlen("cmdstat_");

  char *command_calls = value;
  if (strncmp(command_calls, "calls=", strlen("calls=")) != 0)
    return -1;
  command_calls += strlen("calls=");

  char *command_usec = strchr(command_calls, ',');
  if (command_usec == NULL)
    return -1;
  *(command_usec++) = '\0';

  if (strncmp(command_usec, "usec=", strlen("usec=")) != 0)
    return -1;
  command_usec += strlen("usec=");

  char *end= strchr(command_usec, ',');
  if (end == NULL)
    return -1;
  *end = '\0';

  value_t val = {0};
  val.counter.uinteger = atoll(command_calls);
  metric_family_append(&rn->fams[FAM_REDIS_COMMANDS_TOTAL], "cmd", command, val, tmpl);

  val.counter.uinteger = atoll(command_usec);
  metric_family_append(&rn->fams[FAM_REDIS_COMMANDS_DURATION_USECONDS_TOTAL], "cmd", command, val, tmpl);

  return 0;
}

static int redis_read_info_db(redis_node_t *rn, metric_t *tmpl, char *key, char *value)
{
  char *db = key + strlen("db");

  char *db_keys = value;
  if (strncmp(db_keys, "keys=", strlen("keys=")) != 0)
    return -1;
  db_keys += strlen("keys=");

  char *db_expires = strchr(db_keys, ',');
  if (db_expires == NULL)
    return -1;
  *(db_expires ++) = '\0';

  if (strncmp(db_expires, "expires=", strlen("expires=")) != 0)
    return -1;
  db_expires += strlen("expires=");

  char *end= strchr(db_expires, ',');
  if (end == NULL)
    return -1;
  *end = '\0';

  value_t val = {0};
  val.gauge.real = (double)atoll(db_keys);
  metric_family_append(&rn->fams[FAM_REDIS_DB_KEYS], "db", db, val, tmpl);

  val.gauge.real = (double)atoll(db_expires);
  metric_family_append(&rn->fams[FAM_REDIS_DB_KEYS_EXPIRING], "db", db, val, tmpl);

  return 0;
}

static void redis_read_info(redis_node_t *rn)
{
  redisReply *rr;
  if ((rr = c_redisCommand(rn, "INFO ALL")) == NULL) {
    WARNING("redis plugin: unable to get INFO from node `%s'.", rn->name);
    return;
  }

  metric_t tmpl = {0};
  metric_label_set(&tmpl, "instance", rn->name);

  for (size_t i = 0; i < rn->labels.num; i++) {
    metric_label_set(&tmpl, rn->labels.ptr[i].name, rn->labels.ptr[i].value);
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

    const struct redis_info *ri;
    if ((ri = redis_info_get_key(key, key_len))!= NULL) {
      value_t value = {0};

      if (strcmp(val, "ok") == 0) {
        value.gauge.real = 1;
      } else if (strcmp(val, "true") == 0) {
        value.gauge.real = 1;
      } else if (strcmp(val, "err") == 0) {
        value.gauge.real = 0;
      } else if ((strcmp(val, "fail") == 0) ||
                 (strcmp(val, "false") == 0)) {
        value.gauge.real = 0;
      } else if (strcmp(val, "up") == 0) {
        value.gauge.real = 1;
      } else if (strcmp(val, "down") == 0) {
        value.gauge.real = 0;
      } else {

//used_cpu_sys_children,  RTYPE_SECTOUSEC, FAM_REDIS_CPU_SYS_CHILDREN_USECONDS_TOTAL
//used_cpu_sys,           RTYPE_SECTOUSEC, FAM_REDIS_CPU_SYS_USECONDS_TOTAL
//used_cpu_user_children, RTYPE_SECTOUSEC, FAM_REDIS_CPU_USER_CHILDREN_USECONDS_TOTAL
//used_cpu_user,          RTYPE_SECTOUSEC, FAM_REDIS_CPU_USER_USECONDS_TOTAL
        if (rn->fams[ri->fam].type == METRIC_TYPE_GAUGE) {
          if (parse_double(val, &value.gauge.real) != 0) {
            WARNING("redis plugin: Unable to parse field `%s'.", key);
            fprintf(stderr,"### [%s] [%s]\n", key, val);
            continue;
          }
        } else if (rn->fams[ri->fam].type == METRIC_TYPE_COUNTER) {
          if (parse_uinteger(val, &value.counter.uinteger) != 0) {
            WARNING("redis plugin: Unable to parse field `%s'.", key);
            fprintf(stderr,"### [%s] [%s]\n", key, val);
            continue;
          }
        }
      }
      metric_family_append(&rn->fams[ri->fam], NULL, NULL, value, &tmpl);
    } else {
      if ((key_len > 10) && (strncmp(key, "errorstat_", strlen("errorstat_")) == 0)) {
        redis_read_info_errorstat(rn, &tmpl, key, val);
      } else if ((key_len > 8) && (strncmp(key, "cmdstat_", strlen("cmdstat_")) == 0)) {
        redis_read_info_cmdstat(rn, &tmpl, key, val);
      } else if ((key_len > 6) && (strncmp(key, "master", strlen("master")) == 0)) {
        redis_read_info_master(rn, &tmpl, key, val);
      } else if ((key_len > 5) && (strncmp(key, "slave", strlen("slave")) == 0)) {
        redis_read_info_slave(rn, &tmpl, key, val);
      } else if ((key_len > 2) && (strncmp(key, "db", strlen("db")) == 0)) {
        redis_read_info_db(rn, &tmpl, key, val);
      } else {
//  "cluster_"
fprintf(stderr,"*** [%s] [%s]\n", key, val);
      }
    }
  }

  freeReplyObject(rr);

  for (size_t i = 0; i < FAM_REDIS_MAX; i++) {
    if (rn->fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&rn->fams[i]);
      if (status != 0) {
        ERROR("redis plugin: plugin_dispatch_metric_family failed: %s",
              STRERROR(status));
      }
      metric_family_metric_reset(&rn->fams[i]);
    }
  }
}

static int redis_read(user_data_t *user_data)
{
  redis_node_t *rn = user_data->data;

#if COLLECT_DEBUG
  if (rn->socket)
    DEBUG("redis plugin: querying info from node `%s' (%s).", rn->name,
          rn->socket);
  else
    DEBUG("redis plugin: querying info from node `%s' (%s:%d).", rn->name,
          rn->host, rn->port);
#endif

  redis_check_connection(rn);

  if (!rn->redisContext) /* no connection */
    return -1;

  redis_read_info(rn);

  if (!rn->redisContext) /* connection lost */
    return -1;

  for (redis_query_t *rq = rn->queries; rq != NULL; rq = rq->next) {
    redis_handle_query(rn, rq);
    if (!rn->redisContext) /* connection lost */
      return -1;
  }

  return 0;
}

void module_register(void)
{
  plugin_register_complex_config("redis", redis_config);
}
