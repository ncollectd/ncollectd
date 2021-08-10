// SPDX-License-Identifier: GPL-2.0-only

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"
#include "utils_time.h"

#include "squid_fams.h"
#include "squid_counters.h"

#include <curl/curl.h>

typedef struct {
  char *instance;
  label_set_t labels;

  char *url;

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

  metric_family_t fams[FAM_SQUID_MAX];
} squid_t;

static size_t squid_curl_callback(void *buf, size_t size, size_t nmemb, void *user_data)
{
  size_t len = size * nmemb;
  if (len == 0)
    return len;

  squid_t *sq = user_data;
  if (sq == NULL)
    return 0;

  if ((sq->buffer_fill + len) >= sq->buffer_size) {
    char *temp;
    size_t temp_size;

    temp_size = sq->buffer_fill + len + 1;
    temp = realloc(sq->buffer, temp_size);
    if (temp == NULL) {
      ERROR("squid plugin: realloc failed.");
      return 0;
    }
    sq->buffer = temp;
    sq->buffer_size = temp_size;
  }

  memcpy(sq->buffer + sq->buffer_fill, (char *)buf, len);
  sq->buffer_fill += len;
  sq->buffer[sq->buffer_fill] = 0;

  return len;
}

static void squid_free(void *arg)
{
  squid_t *sq = (squid_t *)arg;
  if (sq == NULL)
    return;

  if (sq->curl != NULL)
    curl_easy_cleanup(sq->curl);
  sq->curl = NULL;

  label_set_reset(&sq->labels);
  sfree(sq->url);
  sfree(sq->user);
  sfree(sq->pass);
  sfree(sq->credentials);
  sfree(sq->cacert);
  curl_slist_free_all(sq->headers);

  sfree(sq->buffer);

  sfree(sq);
}

static int squid_config_append_string(oconfig_item_t *ci, const char *name,
                                      struct curl_slist **dest)
{
  struct curl_slist *temp = NULL;
  if ((ci->values_num != 1) || (ci->values[0].type != OCONFIG_TYPE_STRING)) {
    WARNING("squid plugin: `%s' needs exactly one string argument.", name);
    return -1;
  }

  temp = curl_slist_append(*dest, ci->values[0].value.string);
  if (temp == NULL)
    return -1;

  *dest = temp;

  return 0;
}

static int squid_init_curl(squid_t *sq) 
{
  sq->curl = curl_easy_init();
  if (sq->curl == NULL) {
    ERROR("squid plugin: curl_easy_init failed.");
    return -1;
  }

  curl_easy_setopt(sq->curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(sq->curl, CURLOPT_WRITEFUNCTION, squid_curl_callback);
  curl_easy_setopt(sq->curl, CURLOPT_WRITEDATA, sq);
  curl_easy_setopt(sq->curl, CURLOPT_USERAGENT, COLLECTD_USERAGENT);
  curl_easy_setopt(sq->curl, CURLOPT_ERRORBUFFER, sq->curl_errbuf);
  curl_easy_setopt(sq->curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(sq->curl, CURLOPT_MAXREDIRS, 50L);
  curl_easy_setopt(sq->curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);

  if (sq->user != NULL) {
#ifdef HAVE_CURLOPT_USERNAME
    curl_easy_setopt(sq->curl, CURLOPT_USERNAME, sq->user);
    curl_easy_setopt(sq->curl, CURLOPT_PASSWORD, (sq->pass == NULL) ? "" : sq->pass);
#else
    size_t credentials_size;

    credentials_size = strlen(sq->user) + 2; if (sq->pass != NULL)
    credentials_size += strlen(sq->pass);

    sq->credentials = malloc(credentials_size);
    if (sq->credentials == NULL) {
      ERROR("squid plugin: malloc failed.");
      return -1;
    }

    snprintf(sq->credentials, credentials_size, "%s:%s", sq->user,
             (sq->pass == NULL) ? "" : sq->pass);
    curl_easy_setopt(sq->curl, CURLOPT_USERPWD, sq->credentials);
#endif
    if (sq->digest)
      curl_easy_setopt(sq->curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
  }

  curl_easy_setopt(sq->curl, CURLOPT_SSL_VERIFYPEER, (long)sq->verify_peer);
  curl_easy_setopt(sq->curl, CURLOPT_SSL_VERIFYHOST, sq->verify_host ? 2L : 0L);
  if (sq->cacert != NULL)
    curl_easy_setopt(sq->curl, CURLOPT_CAINFO, sq->cacert);
  if (sq->headers != NULL)
    curl_easy_setopt(sq->curl, CURLOPT_HTTPHEADER, sq->headers);

#ifdef HAVE_CURLOPT_TIMEOUT_MS
    curl_easy_setopt(sq->curl, CURLOPT_TIMEOUT_MS,
                     (long)CDTIME_T_TO_MS(plugin_get_interval()));
#endif

  return 0;
}

static int squid_read_counters(squid_t *sq, metric_t *tmpl)
{
  size_t url_len = strlen(sq->url);
  if (url_len <= 0)
    return -1;
  
  char url[1024];
  ssnprintf(url, sizeof(url), "%s%ssquid-internal-mgr/counters",
            sq->url, sq->url[url_len-1] == '/' ? "" : "/");
  curl_easy_setopt(sq->curl, CURLOPT_URL, url);

  sq->buffer_fill = 0;

  int status = curl_easy_perform(sq->curl);
  if (status != CURLE_OK) {
    ERROR("squid plugin: curl_easy_perform failed with status %i: %s", status,
          sq->curl_errbuf);
    return -1;
  }

  long rcode = 0;
  status = curl_easy_getinfo(sq->curl, CURLINFO_RESPONSE_CODE, &rcode);
  if (status != CURLE_OK) {
    ERROR("squid plugin: Fetching response code failed with status %i: %s",
          status, sq->curl_errbuf);
    return -1;
  }

  if (rcode != 200) {
    ERROR("squid plugin: response code for %s was %ld", url, rcode);
    return -1;
  }

  if (sq->buffer_fill == 0) {
    ERROR("squid plugin: empty response for %s", url);
    return -1;
  }

  char *ptr = sq->buffer;
  char *saveptr = NULL;
  char *line;
  while ((line = strtok_r(ptr, "\n", &saveptr)) != NULL) {
    ptr = NULL;

    char *key = line;
    char *val = strchr(key, '=');
    if (val == NULL)
      continue;
    *val = '\0';
    val++;
    while (*val == ' ') val++;

    size_t key_len = strlen(key);
    while ((key_len > 0) && key[key_len - 1] == ' ') {
      key[key_len - 1] = '\0';
      key_len--;
    }

    const struct squid_counter *sc;
    if ((sc = squid_counter_get_key(key, key_len))!= NULL) {
      value_t value = {0};

      switch(sc->fam) {
        case FAM_SQUID_CLIENT_HTTP_IN_BYTES_TOTAL:
        case FAM_SQUID_CLIENT_HTTP_OUT_BYTES_TOTAL:
        case FAM_SQUID_CLIENT_HTTP_HIT_OUT_BYTES_TOTAL:
        case FAM_SQUID_SERVER_ALL_IN_BYTES_TOTAL:
        case FAM_SQUID_SERVER_ALL_OUT_BYTES_TOTAL:
        case FAM_SQUID_SERVER_HTTP_IN_BYTES_TOTAL:
        case FAM_SQUID_SERVER_HTTP_OUT_BYTES_TOTAL:
        case FAM_SQUID_SERVER_FTP_IN_BYTES_TOTAL:
        case FAM_SQUID_SERVER_FTP_OUT_BYTES_TOTAL:
        case FAM_SQUID_SERVER_OTHER_IN_BYTES_TOTAL:
        case FAM_SQUID_SERVER_OTHER_OUT_BYTES_TOTAL:
        case FAM_SQUID_ICP_SENT_BYTES_TOTAL:
        case FAM_SQUID_ICP_RECV_BYTES_TOTAL:
        case FAM_SQUID_ICP_Q_SENT_BYTES_TOTAL:
        case FAM_SQUID_ICP_R_SENT_BYTES_TOTAL:
        case FAM_SQUID_ICP_Q_RECV_BYTES_TOTAL:
        case FAM_SQUID_ICP_R_RECV_BYTES_TOTAL:
        case FAM_SQUID_CD_SENT_BYTES_TOTAL:
        case FAM_SQUID_CD_RECV_BYTES_TOTAL: {
          uint64_t counter; 
          if (strtouint(val, &counter) < 0) 
            continue;
          value.counter.uint64 = counter * 1024;
          break;
        }
        case FAM_SQUID_CPU_TIME_TOTAL:
        case FAM_SQUID_WALL_TIME_TOTAL: {
          double gauge;
          if (strtodouble(val, &gauge) < 0) 
            continue;
          value.counter.uint64 = gauge * 1000000; // FIXME
          break;
        }
        default:
          if (sq->fams[sc->fam].type == METRIC_TYPE_GAUGE) {
            if (parse_uinteger(val, &value.counter.uint64) != 0) {
              WARNING("squid plugin: Unable to parse field `%s'.", key);
              continue;
            }
          } else if (sq->fams[sc->fam].type == METRIC_TYPE_COUNTER) {
            if (parse_double(val, &value.gauge.float64) != 0) {
              WARNING("squid plugin: Unable to parse field `%s'.", key);
              continue;
            }
          }
          break;
      }

      metric_family_append(&sq->fams[sc->fam], NULL, NULL, value, tmpl);
    }
  }

  for (int i = 0; i < FAM_SQUID_MAX; i++) {
    if (sq->fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&sq->fams[i]);
      if (status != 0) {
        ERROR("squid plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      }
      metric_family_metric_reset(&sq->fams[i]);
    }
  }

  return 0;
}

static int squid_read(user_data_t *ud)
{
  if ((ud == NULL) || (ud->data == NULL)) {
    ERROR("squid plugin: squid_read: Invalid user data.");
    return -1;
  }

  squid_t *sq = (squid_t *)ud->data;

  metric_family_t fam_up = {
    .name = "squid_up",
    .type = METRIC_TYPE_GAUGE,
    .help = "Could the squid server be reached"
  };

  metric_t tmpl = {0};

  metric_label_set(&tmpl, "instance", sq->instance);
  for (size_t i = 0; i < sq->labels.num; i++) {
    metric_label_set(&tmpl, sq->labels.ptr[i].name, sq->labels.ptr[i].value);
  }

  int status = squid_read_counters(sq, &tmpl);

  metric_family_append(&fam_up, NULL, NULL,
                       (value_t){.gauge.float64 = status == 0 ? 1 : 0},
                       &tmpl);

  status = plugin_dispatch_metric_family(&fam_up);
  if (status != 0) {
    ERROR("squid plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
  }
  metric_family_metric_reset(&fam_up);

  metric_reset(&tmpl);

  return 0;
}

static int squid_config_instance(oconfig_item_t *ci) 
{
  if ((ci->values_num != 1) || (ci->values[0].type != OCONFIG_TYPE_STRING)) {
    WARNING("squid plugin: `Page' blocks need exactly one string argument.");
    return -1;
  }

  squid_t *sq = calloc(1, sizeof(*sq));
  if (sq == NULL) {
    ERROR("squid plugin: calloc failed.");
    return -1;
  }
  sq->digest = false;
  sq->verify_peer = true;
  sq->verify_host = true;

  memcpy(sq->fams, fams_squid,
         FAM_SQUID_MAX * sizeof(fams_squid[0]));

  sq->instance = strdup(ci->values[0].value.string);
  if (sq->instance == NULL) {
    ERROR("squid plugin: strdup failed.");
    sfree(sq);
    return -1;
  }

  int status = 0;
  cdtime_t interval = 0;
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;

    if (strcasecmp("URL", child->key) == 0)
      status = cf_util_get_string(child, &sq->url);
    else if (strcasecmp("Label", child->key) == 0)
      status = cf_util_get_label(child, &sq->labels);
    else if (strcasecmp("User", child->key) == 0)
      status = cf_util_get_string(child, &sq->user);
    else if (strcasecmp("Password", child->key) == 0)
      status = cf_util_get_string(child, &sq->pass);
    else if (strcasecmp("Digest", child->key) == 0)
      status = cf_util_get_boolean(child, &sq->digest);
    else if (strcasecmp("VerifyPeer", child->key) == 0)
      status = cf_util_get_boolean(child, &sq->verify_peer);
    else if (strcasecmp("VerifyHost", child->key) == 0)
      status = cf_util_get_boolean(child, &sq->verify_host);
    else if (strcasecmp("CACert", child->key) == 0)
      status = cf_util_get_string(child, &sq->cacert);
    else if (strcasecmp("Header", child->key) == 0)
      status = squid_config_append_string(child, "Header", &sq->headers);
    else if (strcasecmp("Interval", child->key) == 0)
      status = cf_util_get_cdtime(child, &interval);
    else {
      WARNING("squid plugin: Option `%s' not allowed here.", child->key);
      status = -1;
    }

    if (status != 0)
      break;
  }

  if (status == 0) {
    if (sq->url == NULL) {
      WARNING("squid plugin: `URL' missing in `Page' block.");
      status = -1;
    }
  }

  if (status == 0)
    status = squid_init_curl(sq);

  if (status != 0) {
    squid_free(sq);
    return status;
  }

  char *cb_name = ssnprintf_alloc("squid/%s", sq->instance);

  status = plugin_register_complex_read(/* group = */ NULL,
                               cb_name, squid_read,
                               interval,
                               &(user_data_t){
                                   .data = sq,
                                   .free_func = squid_free,
                               });
  sfree(cb_name);

  return status;
} 

static int squid_config(oconfig_item_t *ci)
{
  int success = 0;
  int errors = 0;

  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;

    if (strcasecmp("Instance", child->key) == 0) {
      int status = squid_config_instance(child);
      if (status == 0)
        success++;
      else
        errors++;
    } else {
      WARNING("squid plugin: Option `%s' not allowed here.", child->key);
      errors++;
    }
  }

  if ((success == 0) && (errors > 0)) {
    ERROR("squid plugin: All statements failed.");
    return -1;
  }

  return 0;
}

static int squid_init(void)
{
  curl_global_init(CURL_GLOBAL_SSL);
  return 0;
}

void module_register(void)
{
  plugin_register_complex_config("squid", squid_config);
  plugin_register_init("squid", squid_init);
} 
