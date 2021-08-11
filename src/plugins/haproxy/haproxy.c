// SPDX-License-Identifier: GPL-2.0-only

#include "collectd.h"
#include "plugin.h"
#include "utils/common/common.h"

#include <sys/stat.h>
#include <sys/un.h>

#include <curl/curl.h>

#include "haproxy_process_fams.h"
#include "haproxy_stat_fams.h"
#include "haproxy_stat.h"
#include "haproxy_info.h"

enum {
  FAM_HAPROXY_STICKTABLE_SIZE = 0,
  FAM_HAPROXY_STICKTABLE_USED,
  FAM_HAPROXY_STICKTABLE_MAX,
};

typedef enum {
  HA_TYPE_FRONTEND,
  HA_TYPE_BACKEND,
  HA_TYPE_SERVER,
  HA_TYPE_LISTENER,
} ha_type_t;

typedef enum {
  HA_PROXY_MODE_TCP,
  HA_PROXY_MODE_HTTP,
  HA_PROXY_MODE_HEALTH,
  HA_PROXY_MODE_UNKNOW,
} ha_proxy_mode_t ;

typedef struct {
  char *instance;
  label_set_t labels;

  char *socketpath;

  char *url;
  int address_family;
  char *user;
  char *pass;
  char *credentials;
  bool digest;
  bool verify_peer;
  bool verify_host;
  char *cacert;
  struct curl_slist *headers;

  CURL *curl;
  char curl_errbuf[CURL_ERROR_SIZE];
  char *buffer;
  size_t buffer_size;
  size_t buffer_fill;

  metric_family_t fams_process[FAM_HAPROXY_PROCESS_MAX];
  metric_family_t fams_stat[FAM_HAPROXY_STAT_MAX];
  metric_family_t fams_sticktable[FAM_HAPROXY_STICKTABLE_MAX];
} haproxy_t;

typedef struct {
  int field;
  int fam;
} haproxy_field_t;

static haproxy_field_t haproxy_frontend_fields[] = {
  {HA_STAT_SCUR, FAM_HAPROXY_FRONTEND_CURRENT_SESSIONS},
  {HA_STAT_SMAX, FAM_HAPROXY_FRONTEND_MAX_SESSIONS},
  {HA_STAT_SLIM, FAM_HAPROXY_FRONTEND_LIMIT_SESSION},
  {HA_STAT_STOT, FAM_HAPROXY_FRONTEND_SESSIONS},
  {HA_STAT_BIN, FAM_HAPROXY_FRONTEND_BYTES_IN},
  {HA_STAT_BOUT, FAM_HAPROXY_FRONTEND_BYTES_OUT},
  {HA_STAT_DREQ, FAM_HAPROXY_FRONTEND_REQUESTS_DENIED},
  {HA_STAT_DRESP, FAM_HAPROXY_FRONTEND_RESPONSES_DENIED},
  {HA_STAT_EREQ, FAM_HAPROXY_FRONTEND_REQUEST_ERRORS},
  {HA_STAT_STATUS, FAM_HAPROXY_FRONTEND_STATUS},
  {HA_STAT_RATE_LIM, FAM_HAPROXY_FRONTEND_LIMIT_SESSION_RATE},
  {HA_STAT_RATE_MAX, FAM_HAPROXY_FRONTEND_MAX_SESSION_RATE},
  {HA_STAT_HRSP_1XX, FAM_HAPROXY_FRONTEND_HTTP_RESPONSES},
  {HA_STAT_HRSP_2XX, FAM_HAPROXY_FRONTEND_HTTP_RESPONSES},
  {HA_STAT_HRSP_3XX, FAM_HAPROXY_FRONTEND_HTTP_RESPONSES},
  {HA_STAT_HRSP_4XX, FAM_HAPROXY_FRONTEND_HTTP_RESPONSES},
  {HA_STAT_HRSP_5XX, FAM_HAPROXY_FRONTEND_HTTP_RESPONSES},
  {HA_STAT_HRSP_OTHER, FAM_HAPROXY_FRONTEND_HTTP_RESPONSES},
  {HA_STAT_REQ_RATE_MAX, FAM_HAPROXY_FRONTEND_HTTP_REQUESTS_RATE_MAX},
  {HA_STAT_REQ_TOT, FAM_HAPROXY_FRONTEND_HTTP_REQUESTS},
  {HA_STAT_COMP_IN, FAM_HAPROXY_FRONTEND_HTTP_COMP_BYTES_IN},
  {HA_STAT_COMP_OUT, FAM_HAPROXY_FRONTEND_HTTP_COMP_BYTES_OUT},
  {HA_STAT_COMP_BYP, FAM_HAPROXY_FRONTEND_HTTP_COMP_BYTES_BYPASSED},
  {HA_STAT_COMP_RSP, FAM_HAPROXY_FRONTEND_HTTP_COMP_RESPONSES},
  {HA_STAT_CONN_RATE_MAX, FAM_HAPROXY_FRONTEND_CONNECTIONS_RATE_MAX},
  {HA_STAT_CONN_TOT, FAM_HAPROXY_FRONTEND_CONNECTIONS},
  {HA_STAT_INTERCEPTED, FAM_HAPROXY_FRONTEND_INTERCEPTED_REQUESTS},
  {HA_STAT_DCON, FAM_HAPROXY_FRONTEND_DENIED_CONNECTIONS},
  {HA_STAT_DSES, FAM_HAPROXY_FRONTEND_DENIED_SESSIONS},
  {HA_STAT_WREW, FAM_HAPROXY_FRONTEND_FAILED_HEADER_REWRITING},
  {HA_STAT_CACHE_LOOKUPS, FAM_HAPROXY_FRONTEND_HTTP_CACHE_LOOKUPS},
  {HA_STAT_CACHE_HITS, FAM_HAPROXY_FRONTEND_HTTP_CACHE_HITS},
  {HA_STAT_EINT, FAM_HAPROXY_FRONTEND_INTERNAL_ERRORS},
};
static size_t haproxy_frontend_fields_size = STATIC_ARRAY_SIZE(haproxy_frontend_fields);

static haproxy_field_t haproxy_listener_fields[] = {
  {HA_STAT_SCUR, FAM_HAPROXY_LISTENER_CURRENT_SESSIONS},
  {HA_STAT_SMAX, FAM_HAPROXY_LISTENER_MAX_SESSIONS},
  {HA_STAT_SLIM, FAM_HAPROXY_LISTENER_LIMIT_SESSIONS},
  {HA_STAT_STOT, FAM_HAPROXY_LISTENER_SESSIONS},
  {HA_STAT_BIN, FAM_HAPROXY_LISTENER_BYTES_IN},
  {HA_STAT_BOUT, FAM_HAPROXY_LISTENER_BYTES_OUT},
  {HA_STAT_DREQ, FAM_HAPROXY_LISTENER_REQUESTS_DENIED},
  {HA_STAT_DRESP, FAM_HAPROXY_LISTENER_RESPONSES_DENIED},
  {HA_STAT_EREQ, FAM_HAPROXY_LISTENER_REQUEST_ERRORS},
  {HA_STAT_STATUS, FAM_HAPROXY_LISTENER_STATUS},
  {HA_STAT_DCON, FAM_HAPROXY_LISTENER_DENIED_CONNECTIONS},
  {HA_STAT_DSES, FAM_HAPROXY_LISTENER_DENIED_SESSIONS},
  {HA_STAT_WREW, FAM_HAPROXY_LISTENER_FAILED_HEADER_REWRITING},
  {HA_STAT_EINT, FAM_HAPROXY_LISTENER_INTERNAL_ERRORS},           
};
static size_t haproxy_listener_fields_size = STATIC_ARRAY_SIZE(haproxy_listener_fields);

static haproxy_field_t haproxy_backend_fields[] = {
  {HA_STAT_QCUR, FAM_HAPROXY_BACKEND_CURRENT_QUEUE},
  {HA_STAT_QMAX, FAM_HAPROXY_BACKEND_MAX_QUEUE},
  {HA_STAT_SCUR, FAM_HAPROXY_BACKEND_CURRENT_SESSIONS},
  {HA_STAT_SMAX, FAM_HAPROXY_BACKEND_MAX_SESSIONS},
  {HA_STAT_SLIM, FAM_HAPROXY_BACKEND_LIMIT_SESSIONS},
  {HA_STAT_STOT, FAM_HAPROXY_BACKEND_SESSIONS},
  {HA_STAT_BIN, FAM_HAPROXY_BACKEND_BYTES_IN},
  {HA_STAT_BOUT, FAM_HAPROXY_BACKEND_BYTES_OUT},
  {HA_STAT_DREQ, FAM_HAPROXY_BACKEND_REQUESTS_DENIED},
  {HA_STAT_DRESP, FAM_HAPROXY_BACKEND_RESPONSES_DENIED},
  {HA_STAT_ECON, FAM_HAPROXY_BACKEND_CONNECTION_ERRORS},
  {HA_STAT_ERESP, FAM_HAPROXY_BACKEND_RESPONSE_ERRORS},
  {HA_STAT_WRETR, FAM_HAPROXY_BACKEND_RETRY_WARNINGS},
  {HA_STAT_WREDIS, FAM_HAPROXY_BACKEND_REDISPATCH_WARNINGS},
  {HA_STAT_STATUS, FAM_HAPROXY_BACKEND_STATUS},
  {HA_STAT_WEIGHT, FAM_HAPROXY_BACKEND_WEIGHT},
  {HA_STAT_ACT, FAM_HAPROXY_BACKEND_ACTIVE_SERVERS},
  {HA_STAT_BCK, FAM_HAPROXY_BACKEND_BACKUP_SERVERS},
  {HA_STAT_CHKFAIL, FAM_HAPROXY_BACKEND_CHECK_FAILURES},
  {HA_STAT_CHKDOWN, FAM_HAPROXY_BACKEND_CHECK_UP_DOWN},
  {HA_STAT_LASTCHG, FAM_HAPROXY_BACKEND_CHECK_LAST_CHANGE_SECONDS},
  {HA_STAT_DOWNTIME, FAM_HAPROXY_BACKEND_DOWNTIME_SECONDS},
  {HA_STAT_LBTOT, FAM_HAPROXY_BACKEND_LOADBALANCED},
  {HA_STAT_RATE_MAX, FAM_HAPROXY_BACKEND_MAX_SESSION_RATE},
  {HA_STAT_HRSP_1XX, FAM_HAPROXY_BACKEND_HTTP_RESPONSES},
  {HA_STAT_HRSP_2XX, FAM_HAPROXY_BACKEND_HTTP_RESPONSES},
  {HA_STAT_HRSP_3XX, FAM_HAPROXY_BACKEND_HTTP_RESPONSES},
  {HA_STAT_HRSP_4XX, FAM_HAPROXY_BACKEND_HTTP_RESPONSES},
  {HA_STAT_HRSP_5XX, FAM_HAPROXY_BACKEND_HTTP_RESPONSES},
  {HA_STAT_HRSP_OTHER, FAM_HAPROXY_BACKEND_HTTP_RESPONSES},
  {HA_STAT_REQ_TOT, FAM_HAPROXY_BACKEND_HTTP_REQUESTS},
  {HA_STAT_CLI_ABRT, FAM_HAPROXY_BACKEND_CLIENT_ABORTS},
  {HA_STAT_SRV_ABRT, FAM_HAPROXY_BACKEND_SERVER_ABORTS},
  {HA_STAT_COMP_IN, FAM_HAPROXY_BACKEND_HTTP_COMP_BYTES_IN},
  {HA_STAT_COMP_OUT, FAM_HAPROXY_BACKEND_HTTP_COMP_BYTES_OUT},
  {HA_STAT_COMP_BYP, FAM_HAPROXY_BACKEND_HTTP_COMP_BYTES_BYPASSED},
  {HA_STAT_COMP_RSP, FAM_HAPROXY_BACKEND_HTTP_COMP_RESPONSES},
  {HA_STAT_LASTSESS, FAM_HAPROXY_BACKEND_LAST_SESSION_SECONDS},
  {HA_STAT_QTIME, FAM_HAPROXY_BACKEND_QUEUE_TIME_AVERAGE_SECONDS},
  {HA_STAT_CTIME, FAM_HAPROXY_BACKEND_CONNECT_TIME_AVERAGE_SECONDS},
  {HA_STAT_RTIME, FAM_HAPROXY_BACKEND_RESPONSE_TIME_AVERAGE_SECONDS},
  {HA_STAT_TTIME, FAM_HAPROXY_BACKEND_TOTAL_TIME_AVERAGE_SECONDS},
  {HA_STAT_WREW, FAM_HAPROXY_BACKEND_FAILED_HEADER_REWRITING},
  {HA_STAT_CONNECT, FAM_HAPROXY_BACKEND_CONNECTION_ATTEMPTS},
  {HA_STAT_REUSE, FAM_HAPROXY_BACKEND_CONNECTION_REUSES},
  {HA_STAT_CACHE_LOOKUPS, FAM_HAPROXY_BACKEND_HTTP_CACHE_LOOKUPS},
  {HA_STAT_CACHE_HITS, FAM_HAPROXY_BACKEND_HTTP_CACHE_HITS},
  {HA_STAT_QT_MAX, FAM_HAPROXY_BACKEND_MAX_QUEUE_TIME_SECONDS},
  {HA_STAT_CT_MAX, FAM_HAPROXY_BACKEND_MAX_CONNECT_TIME_SECONDS},
  {HA_STAT_RT_MAX, FAM_HAPROXY_BACKEND_MAX_RESPONSE_TIME_SECONDS},
  {HA_STAT_TT_MAX, FAM_HAPROXY_BACKEND_MAX_TOTAL_TIME_SECONDS},
  {HA_STAT_EINT, FAM_HAPROXY_BACKEND_INTERNAL_ERRORS},
  {HA_STAT_UWEIGHT, FAM_HAPROXY_BACKEND_UWEIGHT},
};
static size_t haproxy_backend_fields_size = STATIC_ARRAY_SIZE(haproxy_backend_fields);
                                                                                                                                                                                                                    
static haproxy_field_t haproxy_server_fields[] = {
  {HA_STAT_QCUR, FAM_HAPROXY_SERVER_CURRENT_QUEUE},
  {HA_STAT_QMAX, FAM_HAPROXY_SERVER_MAX_QUEUE},
  {HA_STAT_SCUR, FAM_HAPROXY_SERVER_CURRENT_SESSIONS},
  {HA_STAT_SMAX, FAM_HAPROXY_SERVER_MAX_SESSIONS},
  {HA_STAT_SLIM, FAM_HAPROXY_SERVER_LIMIT_SESSIONS},
  {HA_STAT_STOT, FAM_HAPROXY_SERVER_SESSIONS},
  {HA_STAT_BIN, FAM_HAPROXY_SERVER_BYTES_IN},
  {HA_STAT_BOUT, FAM_HAPROXY_SERVER_BYTES_OUT},
  {HA_STAT_DRESP, FAM_HAPROXY_SERVER_RESPONSES_DENIED},
  {HA_STAT_ECON, FAM_HAPROXY_SERVER_CONNECTION_ERRORS},
  {HA_STAT_ERESP, FAM_HAPROXY_SERVER_RESPONSE_ERRORS},
  {HA_STAT_WRETR, FAM_HAPROXY_SERVER_RETRY_WARNINGS},
  {HA_STAT_WREDIS, FAM_HAPROXY_SERVER_REDISPATCH_WARNINGS},
  {HA_STAT_STATUS, FAM_HAPROXY_SERVER_STATUS},
  {HA_STAT_WEIGHT, FAM_HAPROXY_SERVER_WEIGHT},
  {HA_STAT_CHKFAIL, FAM_HAPROXY_SERVER_CHECK_FAILURES},
  {HA_STAT_CHKDOWN, FAM_HAPROXY_SERVER_CHECK_UP_DOWN},
  {HA_STAT_LASTCHG, FAM_HAPROXY_SERVER_CHECK_LAST_CHANGE_SECONDS},
  {HA_STAT_DOWNTIME, FAM_HAPROXY_SERVER_DOWNTIME_SECONDS},
  {HA_STAT_QLIMIT, FAM_HAPROXY_SERVER_QUEUE_LIMIT},
  {HA_STAT_THROTTLE, FAM_HAPROXY_SERVER_CURRENT_THROTTLE},
  {HA_STAT_LBTOT, FAM_HAPROXY_SERVER_LOADBALANCED},
  {HA_STAT_RATE_MAX, FAM_HAPROXY_SERVER_MAX_SESSION_RATE},
  {HA_STAT_CHECK_STATUS, FAM_HAPROXY_SERVER_CHECK_STATUS},
  {HA_STAT_CHECK_CODE, FAM_HAPROXY_SERVER_CHECK_CODE},
  {HA_STAT_CHECK_DURATION, FAM_HAPROXY_SERVER_CHECK_DURATION_SECONDS},
  {HA_STAT_HRSP_1XX, FAM_HAPROXY_SERVER_HTTP_RESPONSES},
  {HA_STAT_HRSP_2XX, FAM_HAPROXY_SERVER_HTTP_RESPONSES},
  {HA_STAT_HRSP_3XX, FAM_HAPROXY_SERVER_HTTP_RESPONSES},
  {HA_STAT_HRSP_4XX, FAM_HAPROXY_SERVER_HTTP_RESPONSES},
  {HA_STAT_HRSP_5XX, FAM_HAPROXY_SERVER_HTTP_RESPONSES},
  {HA_STAT_HRSP_OTHER, FAM_HAPROXY_SERVER_HTTP_RESPONSES},
  {HA_STAT_CLI_ABRT, FAM_HAPROXY_SERVER_CLIENT_ABORTS},
  {HA_STAT_SRV_ABRT, FAM_HAPROXY_SERVER_SERVER_ABORTS},
  {HA_STAT_LASTSESS, FAM_HAPROXY_SERVER_LAST_SESSION_SECONDS},
  {HA_STAT_QTIME, FAM_HAPROXY_SERVER_QUEUE_TIME_AVERAGE_SECONDS},
  {HA_STAT_CTIME, FAM_HAPROXY_SERVER_CONNECT_TIME_AVERAGE_SECONDS},
  {HA_STAT_RTIME, FAM_HAPROXY_SERVER_RESPONSE_TIME_AVERAGE_SECONDS},
  {HA_STAT_TTIME, FAM_HAPROXY_SERVER_TOTAL_TIME_AVERAGE_SECONDS},
  {HA_STAT_WREW, FAM_HAPROXY_SERVER_FAILED_HEADER_REWRITING},
  {HA_STAT_CONNECT, FAM_HAPROXY_SERVER_CONNECTION_ATTEMPTS},
  {HA_STAT_REUSE, FAM_HAPROXY_SERVER_CONNECTION_REUSES},
  {HA_STAT_SRV_ICUR, FAM_HAPROXY_SERVER_IDLE_CONNECTIONS_CURRENT},
  {HA_STAT_SRV_ILIM, FAM_HAPROXY_SERVER_IDLE_CONNECTIONS_LIMIT},
  {HA_STAT_QT_MAX, FAM_HAPROXY_SERVER_MAX_QUEUE_TIME_SECONDS},
  {HA_STAT_CT_MAX, FAM_HAPROXY_SERVER_MAX_CONNECT_TIME_SECONDS},
  {HA_STAT_RT_MAX, FAM_HAPROXY_SERVER_MAX_RESPONSE_TIME_SECONDS},
  {HA_STAT_TT_MAX, FAM_HAPROXY_SERVER_MAX_TOTAL_TIME_SECONDS},
  {HA_STAT_EINT, FAM_HAPROXY_SERVER_INTERNAL_ERRORS},
  {HA_STAT_IDLE_CONN_CUR, FAM_HAPROXY_SERVER_UNSAFE_IDLE_CONNECTIONS_CURRENT},
  {HA_STAT_SAFE_CONN_CUR, FAM_HAPROXY_SERVER_SAFE_IDLE_CONNECTIONS_CURRENT},
  {HA_STAT_USED_CONN_CUR, FAM_HAPROXY_SERVER_USED_CONNECTIONS_CURRENT},
  {HA_STAT_NEED_CONN_EST, FAM_HAPROXY_SERVER_NEED_CONNECTIONS_CURRENT},
  {HA_STAT_UWEIGHT, FAM_HAPROXY_SERVER_UWEIGHT},
};
static size_t haproxy_server_fields_size = STATIC_ARRAY_SIZE(haproxy_server_fields);

static metric_family_t fams_haproxy_sticktable[FAM_HAPROXY_STICKTABLE_MAX] = {
  [FAM_HAPROXY_STICKTABLE_SIZE] = {
    .name = "haproxy_sticktable_size",
    .type = METRIC_TYPE_GAUGE,
    .help = "Stick table size"
  },
  [FAM_HAPROXY_STICKTABLE_USED] = {
    .name = "haproxy_sticktable_used",
    .type = METRIC_TYPE_GAUGE,
    .help = "Number of entries used in this stick table"
  },
};

static void haproxy_free(void *arg)
{
  haproxy_t *ha = (haproxy_t *)arg;
  if (ha == NULL)
    return;

  if (ha->curl != NULL)
    curl_easy_cleanup(ha->curl);
  ha->curl = NULL;

  label_set_reset(&ha->labels);
  sfree(ha->url);
  sfree(ha->socketpath);
  sfree(ha->user);
  sfree(ha->pass);
  sfree(ha->credentials);
  sfree(ha->cacert);
  curl_slist_free_all(ha->headers);

  sfree(ha->buffer);

  sfree(ha);
}

static FILE *haproxy_cmd(haproxy_t *ha, char *cmd)
{
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    ERROR("haproxy plugin: socket failed: %s", STRERRNO);
    return NULL;
  }

  struct sockaddr_un sa = {0};
  sa.sun_family = AF_UNIX;
  strncpy(sa.sun_path, ha->socketpath, sizeof(sa.sun_path)-1);
  sa.sun_path[sizeof(sa.sun_path) - 1] = 0;

  int status = connect(fd, (const struct sockaddr *) &sa, sizeof(sa));
  if (status < 0) {
    ERROR("haproxy plugin: unix socket connect failed: %s", STRERRNO);
    close(fd);
    return NULL;
  }

  size_t cmd_len = strlen(cmd);

  if (send(fd, cmd, cmd_len, 0) < cmd_len) {
    ERROR("haproxy plugin: unix socket send command failed");
    close(fd);
    return NULL;
  }
 
  FILE *fp = fdopen(fd, "r");
  if (fp == NULL) {
    close(fd);
    return NULL;
  }

  return fp;
}

static int haproxy_read_stat_line(haproxy_t *ha, metric_t *tmpl, char *line)
{
  char *fields[256];
  size_t fields_len = 0;

  char *ptr = line;
  while (*ptr != '\0') {
    fields[fields_len] = ptr;
    fields_len++;
    while ((*ptr != '\0') && (*ptr != ',')) ptr++;
    if (*ptr == '\0')
      break;
    *ptr = '\0';
    ptr++;
    if (fields_len >= STATIC_ARRAY_SIZE(fields))
      break;
  }
 
  if (fields_len < HA_STAT_TYPE)
    return -1;

  // 32. type [LFBS]: (0=frontend, 1=backend, 2=server, 3=socket/listener)
  char *type = fields[HA_STAT_TYPE];
  ha_type_t ha_type;
  switch (*type) {
    case '0':
      ha_type = HA_TYPE_FRONTEND;
      break;
    case '1':
      ha_type = HA_TYPE_BACKEND;
      break;
    case '2':
      ha_type = HA_TYPE_SERVER;
      break;
    case '3':
      ha_type = HA_TYPE_LISTENER;
      break;
    default:
      return -1;
      break;
  }
  
  ha_proxy_mode_t ha_proxy_mode = HA_PROXY_MODE_HTTP;
  if (fields_len > HA_STAT_MODE) {
    // 75: mode [LFBS]: proxy mode (tcp, http, health, unknown)
    char *mode = fields[HA_STAT_MODE];
    if (*mode == '\0') {
    }
    
    if (strcmp(mode, "tcp") == 0) {
      ha_proxy_mode = HA_PROXY_MODE_TCP;
    } else if (strcmp(mode, "http") == 0) {
      ha_proxy_mode = HA_PROXY_MODE_HTTP;
    } else if (strcmp(mode, "health") == 0) {
      ha_proxy_mode = HA_PROXY_MODE_HEALTH;
    } else {
      ha_proxy_mode = HA_PROXY_MODE_UNKNOW;
    }
  }

  metric_label_set(tmpl, "proxy", fields[HA_STAT_PXNAME]);
 
  haproxy_field_t *ha_fields;
  size_t ha_fields_size = 0;

  switch(ha_type) {
    case HA_TYPE_FRONTEND:
      ha_fields = haproxy_frontend_fields;
      ha_fields_size = haproxy_frontend_fields_size;
      break;
    case HA_TYPE_BACKEND:
      ha_fields = haproxy_backend_fields;
      ha_fields_size = haproxy_backend_fields_size;
      break;
    case HA_TYPE_SERVER:
      ha_fields = haproxy_server_fields;
      ha_fields_size = haproxy_server_fields_size;
      metric_label_set(tmpl, "server", fields[HA_STAT_SVNAME]);
      break;
    case HA_TYPE_LISTENER:
      ha_fields = haproxy_listener_fields;
      ha_fields_size = haproxy_listener_fields_size;
      metric_label_set(tmpl, "listener", fields[HA_STAT_SVNAME]);
      break;
  }
  
  for(size_t i=0 ; i < ha_fields_size ; i++) {
    size_t n = ha_fields[i].field;
    if (n >= fields_len)
      continue;
    
    char *str = fields[n];
    if (*str == '\0')
      continue;    

    metric_family_t *fam = &ha->fams_stat[ha_fields[i].fam];

    value_t value = {0};
    switch (fam->type) {
      case METRIC_TYPE_COUNTER:
        value.counter.uint64 = (uint64_t)atoll(str);
        break;
      case METRIC_TYPE_GAUGE:
        value.gauge.float64 = (double)atoll(str);
        break;
      default:
        continue;
        break;
    }

    switch (n) {
      case HA_STAT_STATUS:
        switch(ha_type) {
          case HA_TYPE_FRONTEND:
            value.gauge.float64 = strncmp("STOP", str, strlen("STOP")) == 0 ? 1 : 0;
            metric_family_append(fam, "state", "DOWN", value, tmpl);
            value.gauge.float64 = strncmp("OPEN", str, strlen("OPEN")) == 0 ? 1 : 0;
            metric_family_append(fam, "state", "UP", value, tmpl);
            break;
          case HA_TYPE_BACKEND:
            value.gauge.float64 = strncmp("DOWN", str, strlen("DOWN")) == 0 ? 1 : 0;
            metric_family_append(fam, "state", "DOWN", value, tmpl);
            value.gauge.float64 = strncmp("UP", str, strlen("UP")) == 0 ? 1 : 0;
            metric_family_append(fam, "state", "UP", value, tmpl);
            break;
          case HA_TYPE_SERVER:
            value.gauge.float64 = strncmp("DOWN", str, strlen("DOWN")) == 0 ? 1 : 0;
            metric_family_append(fam, "state", "DOWN", value, tmpl);
            value.gauge.float64 = strncmp("UP", str, strlen("UP")) == 0 ? 1 : 0;
            metric_family_append(fam, "state", "UP", value, tmpl);
            value.gauge.float64 = strncmp("MAINT", str, strlen("MAINT")) == 0 ? 1 : 0;
            metric_family_append(fam, "state", "MAINT", value, tmpl);
            value.gauge.float64 = strncmp("DRAIN", str, strlen("DRAIN")) == 0 ? 1 : 0;
            metric_family_append(fam, "state", "DRAIN", value, tmpl);
            value.gauge.float64 = strncmp("NOLB", str, strlen("NOLB")) == 0 ? 1 : 0;
            metric_family_append(fam, "state", "NOLB", value, tmpl);
            // "no check" FIXME
            break;
          case HA_TYPE_LISTENER:
            value.gauge.float64 = strncmp("WAITING", str, strlen("WAITING")) == 0 ? 1 : 0;
            metric_family_append(fam, "state", "WAITING", value, tmpl);
            value.gauge.float64 = strncmp("OPEN", str, strlen("OPEN")) == 0 ? 1 : 0;
            metric_family_append(fam, "state", "OPEN", value, tmpl);
            value.gauge.float64 = strncmp("FULL", str, strlen("FULL")) == 0 ? 1 : 0;
            metric_family_append(fam, "state", "FULL", value, tmpl);
            break;
        }
        break;
      case HA_STAT_CHECK_STATUS:
        value.gauge.float64 = strncmp("HANA", str, strlen("HANA")) == 0 ? 1 : 0;
        metric_family_append(fam, "state", "HANA", value, tmpl);
        value.gauge.float64 = strncmp("SOCKERR", str, strlen("SOCKERR")) == 0 ? 1 : 0;
        metric_family_append(fam, "state", "SOCKERR",  value, tmpl);
        value.gauge.float64 = strncmp("L4OK", str, strlen("L4OK")) == 0 ? 1 : 0;
        metric_family_append(fam, "state", "L4OK",  value, tmpl);
        value.gauge.float64 = strncmp("L4TOUT", str, strlen("L4TOUT")) == 0 ? 1 : 0;
        metric_family_append(fam, "state", "L4TOUT",  value, tmpl);
        value.gauge.float64 = strncmp("L4CON", str, strlen("L4CON")) == 0 ? 1 : 0;
        metric_family_append(fam, "state", "L4CON", value, tmpl);
        value.gauge.float64 = strncmp("L6OK", str, strlen("L6OK")) == 0 ? 1 : 0;
        metric_family_append(fam, "state", "L6OK",  value, tmpl);
        value.gauge.float64 = strncmp("L6TOUT", str, strlen("L6TOUT")) == 0 ? 1 : 0;
        metric_family_append(fam, "state", "L6TOUT", value, tmpl);
        value.gauge.float64 = strncmp("L6RSP", str, strlen("L6RSP")) == 0 ? 1 : 0;
        metric_family_append(fam, "state", "L6RSP", value, tmpl);
        value.gauge.float64 = strncmp("L7TOUT", str, strlen("L7TOUT")) == 0 ? 1 : 0;
        metric_family_append(fam, "state", "L7TOUT",  value, tmpl);
        value.gauge.float64 = strncmp("L7RSP", str, strlen("L7RSP")) == 0 ? 1 : 0;
        metric_family_append(fam, "state", "L7RSP",  value, tmpl);
        value.gauge.float64 = strncmp("L7OK", str, strlen("L7OK")) == 0 ? 1 : 0;
        metric_family_append(fam, "state", "L7OK",  value, tmpl);
        value.gauge.float64 = strncmp("L7OKC", str, strlen("L7OKC")) == 0 ? 1 : 0;
        metric_family_append(fam, "state", "L7OKC", value, tmpl);
        value.gauge.float64 = strncmp("L7STS", str, strlen("L7STS")) == 0 ? 1 : 0;
        metric_family_append(fam, "state", "L7STS",  value, tmpl);
        value.gauge.float64 = strncmp("PROCERR", str, strlen("PROCERR")) == 0 ? 1 : 0;
        metric_family_append(fam, "state", "PROCERR", value, tmpl);
        value.gauge.float64 = strncmp("PROCTOUT", str, strlen("PROCTOUT")) == 0 ? 1 : 0;
        metric_family_append(fam, "state", "PROCTOUT",  value, tmpl);
        value.gauge.float64 = strncmp("PROCOK", str, strlen("PROCOK")) == 0 ? 1 : 0;
        metric_family_append(fam, "state", "PROCOK",  value, tmpl);
        break;
      case HA_STAT_HRSP_1XX:
        if (ha_proxy_mode == HA_PROXY_MODE_HTTP)
          metric_family_append(fam, "code", "1xx", value, tmpl);
        break;
      case HA_STAT_HRSP_2XX:
        if (ha_proxy_mode == HA_PROXY_MODE_HTTP)
          metric_family_append(fam, "code", "2xx", value, tmpl);
        break;
      case HA_STAT_HRSP_3XX:
        if (ha_proxy_mode == HA_PROXY_MODE_HTTP)
          metric_family_append(fam, "code", "3xx", value, tmpl);
        break;
      case HA_STAT_HRSP_4XX:
        if (ha_proxy_mode == HA_PROXY_MODE_HTTP)
          metric_family_append(fam, "code", "4xx", value, tmpl);
        break;
      case HA_STAT_HRSP_5XX:
        if (ha_proxy_mode == HA_PROXY_MODE_HTTP)
          metric_family_append(fam, "code", "5xx", value, tmpl);
        break;
      case HA_STAT_HRSP_OTHER:
        if (ha_proxy_mode == HA_PROXY_MODE_HTTP)
          metric_family_append(fam, "code", "other", value, tmpl);
        break;
      case HA_STAT_REQ_RATE_MAX:
      case HA_STAT_REQ_TOT:
      case HA_STAT_INTERCEPTED:
      case HA_STAT_CACHE_LOOKUPS:
      case HA_STAT_CACHE_HITS:
      case HA_STAT_COMP_IN:
      case HA_STAT_COMP_OUT:
      case HA_STAT_COMP_BYP:
      case HA_STAT_COMP_RSP:
        if (ha_proxy_mode == HA_PROXY_MODE_HTTP)
          metric_family_append(fam, NULL, NULL, value, tmpl);
        break;
      case HA_STAT_CHECK_DURATION:
      case HA_STAT_QTIME:
      case HA_STAT_CTIME:
      case HA_STAT_RTIME:
      case HA_STAT_TTIME:
      case HA_STAT_QT_MAX:
      case HA_STAT_CT_MAX:
      case HA_STAT_RT_MAX:
      case HA_STAT_TT_MAX:
        value.gauge.float64 /= 1000.0;
        metric_family_append(fam, NULL, NULL, value, tmpl);
        break;
      default:
        metric_family_append(fam, NULL, NULL, value, tmpl);
        break;
    }
  }

  metric_label_set(tmpl, "server", NULL);
  metric_label_set(tmpl, "listener", NULL);
  metric_label_set(tmpl, "proxy", NULL);

  return 0;
}

static int haproxy_read_curl_stat(haproxy_t *ha, metric_t *tmpl)
{
  if (ha->buffer_fill == 0)
    return 0;

  char *ptr = ha->buffer;
  char *saveptr = NULL;
  char *line;
  while ((line = strtok_r(ptr, "\n", &saveptr)) != NULL) {
    ptr = NULL;
    if (line[0] != '#')
      haproxy_read_stat_line(ha, tmpl, line);
  }

  for (int i = 0; i < FAM_HAPROXY_STAT_MAX; i++) {
    if (ha->fams_stat[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&ha->fams_stat[i]);
      if (status != 0) {
        ERROR("haproxy plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      }
      metric_family_metric_reset(&ha->fams_stat[i]);
    }
  }

  return 0;
}

static int haproxy_read_cmd_stat(haproxy_t *ha, metric_t *tmpl)
{
  FILE *fp = haproxy_cmd (ha, "show stat\n");
  if (fp == NULL) {
    return -1;
  }

  char buffer[4096];
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {

    size_t len = strlen(buffer);
    while ((len > 0) && (buffer[len-1] == '\n')) {
      buffer[len-1] = '\0';
      len--;
    }

    if (*buffer == '\0')
      continue;

    if (buffer[0] != '#')
      haproxy_read_stat_line(ha, tmpl, buffer);
  }

  for (int i = 0; i < FAM_HAPROXY_STAT_MAX; i++) {
    if (ha->fams_stat[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&ha->fams_stat[i]);
      if (status != 0) {
        ERROR("haproxy plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      }
      metric_family_metric_reset(&ha->fams_stat[i]);
    }
  }

  fclose(fp);
  return 0;
}

static int haproxy_read_cmd_info(haproxy_t *ha, metric_t *tmpl)
{
  FILE *fp = haproxy_cmd (ha, "show info\n");
  if (fp == NULL) {
    return -1;
  }

  char buffer[1024];
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    char *key = buffer;
    char *val = key;

    while ((*val != ':') && (*val != '\0')) val++;
    if (*val == '\0')
      continue;  
    *val = '\0';
    val++;

    while (*val == ' ') val++;
    if (*val == '\0')
      continue;  

    size_t len = strlen(val);
    while ((len > 0) && (val[len-1] == '\n')) {
      val[len-1] = '\0';
      len--;
    }
   
    const struct hainfo_metric *hm = hainfo_get_key (key, strlen(key));
    if (hm == NULL) {
      continue;
    }
 
    metric_family_t *fam = &ha->fams_process[hm->fam];
    value_t value = {0};
    if (hm->fam == FAM_HAPROXY_PROCESS_BUILD_INFO) {
      value.gauge.float64 = 1.0;
      metric_family_append(fam, "version", val, value, tmpl);
    } else {
      if (fam->type == METRIC_TYPE_GAUGE)
        value.gauge.float64 = (double)atoll(val);
      else
        value.counter.uint64 = (uint64_t)atoll(val);
     
      metric_family_append(fam, NULL, NULL, value, tmpl);
    }
  }

  for (int i = 0; i < FAM_HAPROXY_PROCESS_MAX; i++) {
    if (ha->fams_process[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&ha->fams_process[i]);
      if (status != 0) {
        ERROR("haproxy plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      }
      metric_family_metric_reset(&ha->fams_process[i]);
    }
  }

  fclose(fp);
  return 0;
}

static int haproxy_read_cmd_table(haproxy_t *ha, metric_t *tmpl)
{
  FILE *fp = haproxy_cmd (ha, "show table\n");
  if (fp == NULL) {
    return -1;
  }

  char *fields[16];
  char buffer[1024];
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    if (buffer[0] != '#')
      continue;

    int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
    if (fields_num < 7)
      continue;
    
    char *table = fields[2];
    size_t table_len = strlen(table);
    if (table_len == 0)
      continue;

    if (table[table_len - 1] == ',')
      table[table_len-1] = '\0';

    if (strncmp(fields[5], "size:", strlen("size:")) == 0) {
      size_t size_len = strlen(fields[5]);
      if (fields[5][size_len-1] == ',')
        fields[5][size_len-1] = '\0';

      value_t value = {0};
      if (strtodouble(fields[5] + strlen("size:"), &value.gauge.float64) == 0)
        metric_family_append(&ha->fams_sticktable[FAM_HAPROXY_STICKTABLE_SIZE],
                             "table", table, value, tmpl);
    }

    if (strncmp(fields[6], "used:", strlen("used:")) == 0) {
      value_t value = {0};
      if (strtodouble(fields[6] + strlen("used:"), &value.gauge.float64) == 0)
        metric_family_append(&ha->fams_sticktable[FAM_HAPROXY_STICKTABLE_USED],
                             "table", table, value, tmpl);
    }
  }

  for (int i = 0; i < FAM_HAPROXY_STICKTABLE_MAX; i++) {
    if (ha->fams_sticktable[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&ha->fams_sticktable[i]);
      if (status != 0) {
        ERROR("haproxy plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      }
      metric_family_metric_reset(&ha->fams_sticktable[i]);
    }
  }

  fclose(fp);
  return 0;
}

static int haproxy_read (user_data_t *ud)
{
  if ((ud == NULL) || (ud->data == NULL)) {
    ERROR("haproxy plugin: haproxy_read: Invalid user data.");
    return -1;
  }

  haproxy_t *ha = ud->data;

  metric_family_t fam_up = {
    .name = "haproxy_up",
    .type = METRIC_TYPE_GAUGE,
    .help = "Could the haproxy server be reached"
  };

  metric_t tmpl = {0};

  metric_label_set(&tmpl, "instance", ha->instance);
  for (size_t i = 0; i < ha->labels.num; i++) {
    metric_label_set(&tmpl, ha->labels.ptr[i].name, ha->labels.ptr[i].value);
  }
  
  int status = 0;

  if (ha->socketpath != NULL) {
    while (status == 0) {
      status = haproxy_read_cmd_info(ha, &tmpl); 
      if (status < 0)
        break;
      status = haproxy_read_cmd_stat(ha, &tmpl);
      if (status < 0)
        break;
      status = haproxy_read_cmd_table(ha, &tmpl);

      break;
    }
  } else {
    ha->buffer_fill = 0;

    while (status == 0) {
      curl_easy_setopt(ha->curl, CURLOPT_URL, ha->url);

      status = curl_easy_perform(ha->curl);
      if (status != CURLE_OK) {
        ERROR("haproxy plugin: curl_easy_perform failed with status %i: %s",
              status, ha->curl_errbuf);
        status = -1;
        break;
      }

      int rcode = 0;
      status = curl_easy_getinfo(ha->curl, CURLINFO_RESPONSE_CODE, &rcode);
      if (status != CURLE_OK) {
        ERROR("haproxy plugin: Fetching response code failed with status %i: %s",
              status, ha->curl_errbuf);
        status = -1;
        break;
      }

      if (rcode == 200)
        haproxy_read_curl_stat(ha, &tmpl);

      break;
    }
  }

  metric_family_append(&fam_up, NULL, NULL,
                       (value_t){.gauge.float64 = status == 0 ? 1 : 0},
                       &tmpl);
  status = plugin_dispatch_metric_family(&fam_up);
  if (status != 0)
    ERROR("haproxy plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
  metric_family_metric_reset(&fam_up);

  metric_reset(&tmpl);

  return 0;
}

static size_t haproxy_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
  size_t len = size * nmemb;
  if (len == 0)
    return len;

  haproxy_t *ha = user_data;
  if (ha == NULL)
    return 0;

  if ((ha->buffer_fill + len) >= ha->buffer_size) {
    size_t temp_size = ha->buffer_fill + len + 1;
    char * temp = realloc(ha->buffer, temp_size);
    if (temp == NULL) {
      ERROR("curl plugin: realloc failed.");
      return 0;
    }
    ha->buffer = temp;
    ha->buffer_size = temp_size;
  }

  memcpy(ha->buffer + ha->buffer_fill, (char *)buf, len);
  ha->buffer_fill += len;
  ha->buffer[ha->buffer_fill] = 0;

  return len;
}

static int haproxy_init_curl(haproxy_t *ha)
{
  ha->curl = curl_easy_init();
  if (ha->curl == NULL) {
    ERROR("haproxy plugin: curl_easy_init failed.");
    return -1;
  }

  curl_easy_setopt(ha->curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(ha->curl, CURLOPT_WRITEFUNCTION, haproxy_curl_callback);
  curl_easy_setopt(ha->curl, CURLOPT_WRITEDATA, ha);
  curl_easy_setopt(ha->curl, CURLOPT_USERAGENT, COLLECTD_USERAGENT);
  curl_easy_setopt(ha->curl, CURLOPT_ERRORBUFFER, ha->curl_errbuf);
  curl_easy_setopt(ha->curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(ha->curl, CURLOPT_MAXREDIRS, 50L);
  curl_easy_setopt(ha->curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);

  if (ha->user != NULL) {
#ifdef HAVE_CURLOPT_USERNAME
    curl_easy_setopt(ha->curl, CURLOPT_USERNAME, ha->user);
    curl_easy_setopt(ha->curl, CURLOPT_PASSWORD, (ha->pass == NULL) ? "" : ha->pass);
#else
    size_t credentials_size = strlen(ha->user) + 2;
    if (ha->pass != NULL)
      credentials_size += strlen(ha->pass);

    ha->credentials = malloc(credentials_size);
    if (ha->credentials == NULL) {
      ERROR("curl plugin: malloc failed.");
      return -1;
    }

    snprintf(ha->credentials, credentials_size, "%s:%s", ha->user,
             (ha->pass == NULL) ? "" : ha->pass);
    curl_easy_setopt(ha->curl, CURLOPT_USERPWD, ha->credentials);
#endif

    if (ha->digest)
      curl_easy_setopt(ha->curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
  }

  curl_easy_setopt(ha->curl, CURLOPT_SSL_VERIFYPEER, (long)ha->verify_peer);
  curl_easy_setopt(ha->curl, CURLOPT_SSL_VERIFYHOST, ha->verify_host ? 2L : 0L);
  if (ha->cacert != NULL)
    curl_easy_setopt(ha->curl, CURLOPT_CAINFO, ha->cacert);
  if (ha->headers != NULL)
    curl_easy_setopt(ha->curl, CURLOPT_HTTPHEADER, ha->headers);

  curl_easy_setopt(ha->curl, CURLOPT_TIMEOUT_MS, (long)CDTIME_T_TO_MS(plugin_get_interval()));

  return 0;
}

static int haproxy_config_append_string(oconfig_item_t *ci, const char *name,
                                        struct curl_slist **dest)
{
  struct curl_slist *temp = NULL;
  if ((ci->values_num != 1) || (ci->values[0].type != OCONFIG_TYPE_STRING)) {
    WARNING("haproxy plugin: `%s' needs exactly one string argument.", name);
    return -1;
  }

  temp = curl_slist_append(*dest, ci->values[0].value.string);
  if (temp == NULL)
    return -1;

  *dest = temp;

  return 0;
} 

static int haproxy_config_instance(oconfig_item_t *ci)
{
  if ((ci->values_num != 1) || (ci->values[0].type != OCONFIG_TYPE_STRING)) {
    WARNING("haproxy plugin: `Instance' blocks need exactly one string argument.");
    return -1;
  }

  haproxy_t *ha = calloc(1, sizeof(*ha));
  if (ha == NULL) {
    ERROR("haproxy plugin: calloc failed.");
    return -1;
  }

  memcpy(ha->fams_process, fams_haproxy_process,
         FAM_HAPROXY_PROCESS_MAX * sizeof(fams_haproxy_process[0]));
  memcpy(ha->fams_stat, fams_haproxy_stat,
         FAM_HAPROXY_STAT_MAX * sizeof(fams_haproxy_stat[0]));
  memcpy(ha->fams_sticktable, fams_haproxy_sticktable,
         FAM_HAPROXY_STICKTABLE_MAX* sizeof(fams_haproxy_sticktable[0]));

  ha->digest = false;
  ha->verify_peer = true;
  ha->verify_host = true;

  ha->instance = strdup(ci->values[0].value.string);
  if (ha->instance == NULL) {
    ERROR("haproxy plugin: strdup failed.");
    sfree(ha);
    return -1;
  }

  cdtime_t interval = 0;
  int status = 0;
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;

    if (strcasecmp("URL", child->key) == 0)
      status = cf_util_get_string(child, &ha->url);
    else if (strcasecmp("SocketPath", child->key) == 0)
      status = cf_util_get_string(child, &ha->socketpath);
    else if (strcasecmp("User", child->key) == 0)
      status = cf_util_get_string(child, &ha->user);
    else if (strcasecmp("Password", child->key) == 0)
      status = cf_util_get_string(child, &ha->pass);
    else if (strcasecmp("Digest", child->key) == 0)
      status = cf_util_get_boolean(child, &ha->digest);
    else if (strcasecmp("VerifyPeer", child->key) == 0)
      status = cf_util_get_boolean(child, &ha->verify_peer);
    else if (strcasecmp("VerifyHost", child->key) == 0)
      status = cf_util_get_boolean(child, &ha->verify_host);
    else if (strcasecmp("CACert", child->key) == 0)
      status = cf_util_get_string(child, &ha->cacert);
    else if (strcasecmp("Header", child->key) == 0)
      status = haproxy_config_append_string(child, "Header", &ha->headers);
    else if (strcasecmp("Interval", child->key) == 0)
      status = cf_util_get_cdtime(child, &interval);
    else if (strcasecmp("Label", child->key) == 0)
      status = cf_util_get_label(child, &ha->labels);
    else {
      WARNING("haproxy plugin: Option `%s' not allowed here.", child->key);
      status = -1;
    }

    if (status != 0)
      break;
  }

  if ((ha->url == NULL) && (ha->socketpath == NULL)) {
    WARNING("haproxy plugin: `URL' or `SocketPath` missing in `Instance' block.");
    status = -1;
  }

  if ((status == 0) && (ha->url != NULL))
    status = haproxy_init_curl(ha);

  if (status != 0) {
    haproxy_free(ha);
    return status;
  }

  char *cb_name = ssnprintf_alloc("haproxy/%s", ha->instance);

  plugin_register_complex_read(/* group = */ NULL, cb_name, haproxy_read,
                               interval,
                               &(user_data_t){
                                   .data = ha,
                                   .free_func = haproxy_free,
                               });
  sfree(cb_name);
  return 0;
}

static int haproxy_config(oconfig_item_t *ci)
{
  int success = 0;
  int errors = 0;

  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;

    if (strcasecmp("Instance", child->key) == 0) {
      int status = haproxy_config_instance(child);
      if (status == 0)
        success++;
      else
        errors++;
    } else {
      WARNING("haproxy plugin: Option `%s' not allowed here.", child->key);
      errors++;
    }
  }

  if ((success == 0) && (errors > 0)) {
    ERROR("haproxy plugin: All statements failed.");
    return -1;
  }

  return 0;
}

static int haproxy_init(void)
{
  curl_global_init(CURL_GLOBAL_SSL);
  return 0;
}

void module_register(void)
{
  plugin_register_complex_config("haproxy", haproxy_config);
  plugin_register_init("haproxy", haproxy_init);
} 
