#include "config.h"

#if STRPTIME_NEEDS_STANDARDS
#ifndef _ISOC99_SOURCE
#define _ISOC99_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#endif

#include "collectd.h"
#include "plugin.h"
#include "utils/common/common.h"

#include <time.h>

#include <lber.h>
#include <ldap.h>

#include "ds389_fams.h"

typedef struct {
  char *name;

  char *binddn;
  char *password;
  char *cacert;
  bool starttls;
  int timeout;
  char *url;
  bool verifyhost;
  int version;
  label_set_t labels;

  LDAP *ld;

  metric_family_t fams[FAM_DS389_MAX];
} ds389_t;

typedef struct {
  char *attr;
  int fam;
} ds389_metric_t;

static ds389_metric_t ds389_metrics_monitor[] = {
  {"version",                        FAM_DS389_VERSION_INFO},
  {"starttime",                      FAM_DS389_START_TIME_SECONDS},
  {"threads",                        FAM_DS389_THREADS}, 
  {"currentconnections",             FAM_DS389_CONNECTIONS},
  {"totalconnections",               FAM_DS389_CONNECTIONS_TOTAL},
  {"currentconnectionsatmaxthreads", FAM_DS389_CONNECTIONS_MAXTHREADS},
  {"maxthreadsperconnhits",          FAM_DS389_CONNECTIONS_MAXTHREADS_TOTAL},
  {"dtablesize",                     FAM_DS389_DTABLESIZE},
  {"readwaiters",                    FAM_DS389_READWAITERS},
  {"opsinitiated",                   FAM_DS389_OPS_INITIATED_TOTAL},
  {"opscompleted",                   FAM_DS389_OPS_COMPLETED_TOTAL},
  {"entriessent",                    FAM_DS389_SEND_ENTRIES_TOTAL},
  {"nbackends",                      FAM_DS389_NBACKENDS},
};
static size_t ds389_metrics_monitor_size = STATIC_ARRAY_SIZE(ds389_metrics_monitor);

static ds389_metric_t ds389_metrics_snmp[] = {
  {"anonymousbinds",        FAM_DS389_BINDS_ANONYMOUS_TOTAL},
  {"unauthbinds",           FAM_DS389_BINDS_UNAUTH_TOTAL},
  {"simpleauthbinds",       FAM_DS389_BINDS_SIMPLEAUTH_TOTAL},
  {"strongauthbinds",       FAM_DS389_BINDS_STRONGAUTH_TOTAL},
  {"bindsecurityerrors",    FAM_DS389_BINDS_SECURITYERRORS_TOTAL},
  {"inops",                 FAM_DS389_OPS_IN_TOTAL},
  {"compareops",            FAM_DS389_OPS_COMPARE_TOTAL},
  {"addentryops",           FAM_DS389_OPS_ADDENTRY_TOTAL},
  {"removeentryops",        FAM_DS389_OPS_REMOVEENTRY_TOTAL}, 
  {"modifyentryops",        FAM_DS389_OPS_MODIFYENTRY_TOTAL}, 
  {"modifyrdnops",          FAM_DS389_OPS_MODIFYRDN_TOTAL},
  {"searchops",             FAM_DS389_OPS_SEARCH_TOTAL},
  {"onelevelsearchops",     FAM_DS389_OPS_ONELEVELSEARCH_TOTAL},
  {"wholesubtreesearchops", FAM_DS389_OPS_WHOLESUBTREESEARCH_TOTAL},
  {"referrals",             FAM_DS389_REFERRALS_TOTAL},
  {"securityerrors",        FAM_DS389_SECURITYERRORS_TOTAL},
  {"errors",                FAM_DS389_ERRORS_TOTAL},
  {"bytesrecv",             FAM_DS389_RECV_BYTES_TOTAL},
  {"bytessent",             FAM_DS389_SENT_BYTES_TOTAL},
  {"entriesreturned",       FAM_DS389_ENTRIES_RETURNED_TOTAL},
  {"referralsreturned",     FAM_DS389_REFERRALS_RETURNED_TOTAL},
};
static size_t ds389_metrics_snmp_size = STATIC_ARRAY_SIZE(ds389_metrics_snmp);

static ds389_metric_t ds389_metrics_cache[] = {
  {"dbcachehits",                   FAM_DS389_DB_CACHE_HITS_TOTAL},
  {"dbcachetries",                  FAM_DS389_DB_CACHE_TRIES_TOTAL},
  {"dbcachehitratio",               FAM_DS389_DB_CACHE_HIT_RATIO},
  {"dbcachepagein",                 FAM_DS389_DB_CACHE_PAGEIN_TOTAL},
  {"dbcachepageout",                FAM_DS389_DB_CACHE_PAGEOUT_TOTAL},
  {"dbcacheroevict",                FAM_DS389_DB_CACHE_ROEVICT_TOTAL},
  {"dbcacherwevict",                FAM_DS389_DB_CACHE_RWEVICT_TOTAL},
  {"normalizeddncachetries",        FAM_DS389_NDN_CACHE_TRIES_TOTAL},
  {"normalizeddncachehits",         FAM_DS389_NDN_CACHE_HITS_TOTAL},
  {"normalizeddncachemisses",       FAM_DS389_NDN_CACHE_MISSES_TOTAL},
  {"normalizeddncachehitratio",     FAM_DS389_NDN_CACHE_HIT_RATIO},
  {"normalizeddncacheevictions",    FAM_DS389_NDN_CACHE_EVICTIONS_TOTAL},
  {"currentnormalizeddncachesize",  FAM_DS389_NDN_CACHE_SIZE_BYTES},
  {"maxnormalizeddncachesize",      FAM_DS389_NDN_CACHE_MAX_SIZE},
  {"currentnormalizeddncachecount", FAM_DS389_NDN_CACHE_COUNT},
  {"normalizeddncachethreadsize",   FAM_DS389_NDN_CACHE_THREADSIZE},
  {"normalizeddncachethreadslots",  FAM_DS389_NDN_CACHE_THREADSLOTS},
};
static size_t ds389_metrics_cache_size = STATIC_ARRAY_SIZE(ds389_metrics_cache);

static ds389_metric_t ds389_metrics_db[] = {
  {"nsslapd-db-abort-rate",                 FAM_DS389_DB_ABORT_RATE_TOTAL},
  {"nsslapd-db-active-txns",                FAM_DS389_DB_ACTIVE_TXNS},
  {"nsslapd-db-cache-region-wait-rate",     FAM_DS389_DB_CACHE_REGION_WAIT_TOTAL},
  {"nsslapd-db-cache-size-bytes",           FAM_DS389_DB_CACHE_SIZE_BYTES},
  {"nsslapd-db-clean-pages",                FAM_DS389_DB_CLEAN_PAGES},
  {"nsslapd-db-commit-rate",                FAM_DS389_DB_COMMIT_TOTAL},
  {"nsslapd-db-deadlock-rate",              FAM_DS389_DB_DEADLOCK_TOTAL},
  {"nsslapd-db-dirty-pages",                FAM_DS389_DB_DIRTY_PAGES},
  {"nsslapd-db-hash-buckets",               FAM_DS389_DB_HASH_BUCKETS},
  {"nsslapd-db-hash-elements-examine-rate", FAM_DS389_DB_HASH_ELEMENTS_EXAMINE_TOTAL},
  {"nsslapd-db-hash-search-rate",           FAM_DS389_DB_HASH_SEARCH_TOTAL},
  {"nsslapd-db-lock-conflicts",             FAM_DS389_DB_LOCK_CONFLICTS_TOTAL},
  {"nsslapd-db-lock-region-wait-rate",      FAM_DS389_DB_LOCK_REGION_WAIT_TOTAL},
  {"nsslapd-db-lock-request-rate",          FAM_DS389_DB_LOCK_REQUEST_TOTAL},
  {"nsslapd-db-lockers",                    FAM_DS389_DB_LOCKERS},
  {"nsslapd-db-configured-locks",           FAM_DS389_DB_CONFIGURED_LOCKS},
  {"nsslapd-db-current-locks",              FAM_DS389_DB_CURRENT_LOCKS},
  {"nsslapd-db-max-locks",                  FAM_DS389_DB_MAX_LOCKS},
  {"nsslapd-db-current-lock-objects",       FAM_DS389_DB_CURRENT_LOCK_OBJECTS},
  {"nsslapd-db-max-lock-objects",           FAM_DS389_DB_MAX_LOCK_OBJECTS},
  {"nsslapd-db-log-bytes-since-checkpoint", FAM_DS389_DB_LOG_BYTES_SINCE_CHECKPOINT},
  {"nsslapd-db-log-region-wait-rate",       FAM_DS389_DB_LOG_REGION_WAIT_TOTAL},
  {"nsslapd-db-log-write-rate",             FAM_DS389_DB_LOG_WRITE_RATE},
  {"nsslapd-db-longest-chain-length",       FAM_DS389_DB_LONGEST_CHAIN_LENGTH},
  {"nsslapd-db-page-create-rate",           FAM_DS389_DB_PAGE_CREATE_TOTAL},
  {"nsslapd-db-page-read-rate",             FAM_DS389_DB_PAGE_READ_TOTAL},
  {"nsslapd-db-page-ro-evict-rate",         FAM_DS389_DB_PAGE_RO_EVICT_TOTAL},
  {"nsslapd-db-page-rw-evict-rate",         FAM_DS389_DB_PAGE_RW_EVICT_TOTAL},
  {"nsslapd-db-page-trickle-rate",          FAM_DS389_DB_PAGE_TRICKLE_TOTAL},
  {"nsslapd-db-page-write-rate",            FAM_DS389_DB_PAGE_WRITE_TOTAL},
  {"nsslapd-db-pages-in-use",               FAM_DS389_DB_PAGES_IN_USE},
  {"nsslapd-db-txn-region-wait-rate",       FAM_DS389_DB_TXN_REGION_WAIT_TOTAL},     
};
static size_t ds389_metrics_db_size = STATIC_ARRAY_SIZE(ds389_metrics_db);

static ds389_metric_t ds389_metrics_link[] = {
  {"nsAddCount",                 FAM_DS389_LINK_ADD},
  {"nsDeleteCount",              FAM_DS389_LINK_DELETE},
  {"nsModifyCount",              FAM_DS389_LINK_MODIFY},
  {"nsRenameCount",              FAM_DS389_LINK_RENAME},
  {"nsSearchBaseCount",          FAM_DS389_LINK_SEARCH_BASE},
  {"nsSearchOneLevelCount",      FAM_DS389_LINK_SEARCH_ONELEVEL},
  {"nsSearchSubtreeCount",       FAM_DS389_LINK_SEARCH_SUBTREE},
  {"nsAbandonCount",             FAM_DS389_LINK_ABANDON},
  {"nsBindCount",                FAM_DS389_LINK_BIND},
  {"nsUnbindCount",              FAM_DS389_LINK_UNBIND},
  {"nsCompareCount",             FAM_DS389_LINK_COMPARE},
  {"nsOperationConnectionCount", FAM_DS389_LINK_CONNECTION_OPERATION},
  {"nsBindConnectionCount",      FAM_DS389_LINK_CONNECTION_BIND},
};
static size_t ds389_metrics_link_size = STATIC_ARRAY_SIZE(ds389_metrics_link);

static ds389_metric_t ds389_metrics_backend[] = {
  {"readonly",               FAM_DS389_BACKEND_READONLY},
  {"entrycachehits",         FAM_DS389_BACKEND_ENTRY_CACHE_HITS},
  {"entrycachetries",        FAM_DS389_BACKEND_ENTRY_CACHE_TRIES},
  {"entrycachehitratio",     FAM_DS389_BACKEND_ENTRY_CACHE_HIT_RATIO},
  {"currententrycachesize",  FAM_DS389_BACKEND_ENTRY_CACHE_SIZE},
  {"maxentrycachesize",      FAM_DS389_BACKEND_ENTRY_CACHE_SIZE_MAX},
  {"currententrycachecount", FAM_DS389_BACKEND_ENTRY_CACHE_COUNT},
  {"maxentrycachecount",     FAM_DS389_BACKEND_ENTRY_CACHE_COUNT_MAX},
  {"dncachehits",            FAM_DS389_BACKEND_DN_CACHE_HITS},
  {"dncachetries",           FAM_DS389_BACKEND_DN_CACHE_TRIES},
  {"dncachehitratio",        FAM_DS389_BACKEND_DN_CACHE_HIT_RATIO},
  {"currentdncachesize",     FAM_DS389_BACKEND_DN_CACHE_SIZE},
  {"maxdncachesize",         FAM_DS389_BACKEND_DN_CACHE_SIZE_MAX},
  {"currentdncachecount",    FAM_DS389_BACKEND_DN_CACHE_COUNT},
  {"maxdncachecount",        FAM_DS389_BACKEND_DN_CACHE_COUNT_MAX},
};
static size_t ds389_metrics_backend_size = STATIC_ARRAY_SIZE(ds389_metrics_backend);

static void ds389_free(void *arg)
{
  ds389_t *st = arg;
  if (st == NULL)
    return;

  sfree(st->binddn);
  sfree(st->password);
  sfree(st->cacert);
  sfree(st->name);
  sfree(st->url);
  if (st->ld != NULL)
    ldap_unbind_ext_s(st->ld, NULL, NULL);

  for (int i = 0; i < FAM_DS389_MAX; i++) {
    if (st->fams[i].metric.num > 0) {
      metric_family_metric_reset(&st->fams[i]);
    }
  }

  sfree(st);
}

static int ds389_init_host(ds389_t *st)
{
  if (st->ld != NULL) {
    DEBUG("ds389 plugin: Already connected to %s", st->url);
    return 0;
  }

  int rc = ldap_initialize(&st->ld, st->url);
  if (rc != LDAP_SUCCESS) {
    ERROR("ds389 plugin: ldap_initialize failed: %s", ldap_err2string(rc));
    if (st->ld != NULL)
      ldap_unbind_ext_s(st->ld, NULL, NULL);
    st->ld = NULL;
    return -1;
  }

  ldap_set_option(st->ld, LDAP_OPT_PROTOCOL_VERSION, &st->version);

  ldap_set_option(st->ld, LDAP_OPT_TIMEOUT, &(const struct timeval){st->timeout, 0});

  ldap_set_option(st->ld, LDAP_OPT_RESTART, LDAP_OPT_ON);

  if (st->cacert != NULL)
    ldap_set_option(st->ld, LDAP_OPT_X_TLS_CACERTFILE, st->cacert);

  if (st->verifyhost == false) {
    int never = LDAP_OPT_X_TLS_NEVER;
    ldap_set_option(st->ld, LDAP_OPT_X_TLS_REQUIRE_CERT, &never);
  }

  if (st->starttls) {
    rc = ldap_start_tls_s(st->ld, NULL, NULL);
    if (rc != LDAP_SUCCESS) {
      ERROR("ds389 plugin: Failed to start tls on %s: %s", st->url, ldap_err2string(rc));
      ldap_unbind_ext_s(st->ld, NULL, NULL);
      st->ld = NULL;
      return -1;
    }
  }

  struct berval cred;
  if (st->password != NULL) {
    cred.bv_val = st->password;
    cred.bv_len = strlen(st->password);
  } else {
    cred.bv_val = "";
    cred.bv_len = 0;
  }

  rc = ldap_sasl_bind_s(st->ld, st->binddn, LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL);
  if (rc != LDAP_SUCCESS) {
    ERROR("ds389 plugin: Failed to bind to %s: %s", st->url, ldap_err2string(rc));
    ldap_unbind_ext_s(st->ld, NULL, NULL);
    st->ld = NULL;
    return -1;
  }

  DEBUG("ds389 plugin: Successfully connected to %s", st->url);
  return 0;
}

static int ds389_read_metrics(ds389_t *st, char *filter,
                              ds389_metric_t *metrics, size_t metrics_size,
                              metric_t *tmpl)
{
  LDAPMessage *result = NULL;
  int rc = ldap_search_ext_s(st->ld, filter, LDAP_SCOPE_BASE, "(objectClass=*)",
                             NULL, 0, NULL, NULL, NULL, 0, &result);

  if (rc != LDAP_SUCCESS) {
    ERROR("ds389 plugin: Failed to execute search: %s", ldap_err2string(rc));
    ldap_msgfree(result);
    return -1;
  }

  LDAPMessage *e = ldap_first_entry(st->ld, result);
  if (e == NULL) {
    ERROR("ds389 plugin: Failed to get first entry");
    ldap_msgfree(result);
    return -1;
  }

  for (size_t i = 0; i < metrics_size; i++) {
    value_t value;
    struct berval **value_list =
        ldap_get_values_len(st->ld, e, metrics[i].attr);
    if (value_list != NULL) {
      struct berval value_data = *value_list[0];
      metric_family_t *fam = &st->fams[metrics[i].fam];

      if (metrics[i].fam == FAM_DS389_VERSION_INFO) {
        value.gauge = 1;
        metric_family_append(fam, "version", value_data.bv_val, value, tmpl);
      } else if (metrics[i].fam == FAM_DS389_START_TIME_SECONDS) {
        struct tm tm = {0};
        if (strptime(value_data.bv_val,"%Y%m%d%H%M%S", &tm) != NULL) {
          time_t start = mktime(&tm);
          value.gauge = start;
          metric_family_append(fam, NULL, NULL, value, tmpl);
        }
      } else {
        if (fam->type == METRIC_TYPE_GAUGE)
          value.gauge = atoll(value_data.bv_val);
        else
          value.counter = atoll(value_data.bv_val);

        metric_family_append(fam, NULL, NULL, value, tmpl);
      }
      ldap_value_free_len(value_list);
    }
  }

  ldap_msgfree(result);

  return 0;
}

static int ds389_read_backend_metrics(ds389_t *st, char *filter, metric_t *tmpl)
{ 
  LDAPMessage *result = NULL;
  int rc = ldap_search_ext_s(st->ld, filter, LDAP_SCOPE_BASE, "(objectClass=*)",
                             NULL, 0, NULL, NULL, NULL, 0, &result);

  if (rc != LDAP_SUCCESS) {
    ERROR("ds389 plugin: Failed to execute search: %s", ldap_err2string(rc));
    ldap_msgfree(result);
    return -1;
  }

  LDAPMessage *e = ldap_first_entry(st->ld, result);
  if (e == NULL) {
    ERROR("ds389 plugin: Failed to get first entry");
    ldap_msgfree(result);
    return -1;
  }

  for (size_t i = 0; i < ds389_metrics_backend_size; i++) {
    metric_family_t *fam = &st->fams[ds389_metrics_backend[i].fam];
    char *attr = ds389_metrics_backend[i].attr;
    struct berval **value_list = ldap_get_values_len(st->ld, e, attr);
    if (value_list != NULL) {
      value_t value;
      struct berval value_data = *value_list[0];

      if (fam->type == METRIC_TYPE_GAUGE)
        value.gauge = atoll(value_data.bv_val);
      else
        value.counter = atoll(value_data.bv_val);

      metric_family_append(fam, NULL, NULL, value, tmpl);

      ldap_value_free_len(value_list);
    }
  }

  int current_num = -1;
  char *current_filename = NULL;

  BerElement *ber = NULL;
  char *attr = NULL;
  for (attr = ldap_first_attribute(st->ld, e, &ber); attr != NULL;
       attr = ldap_next_attribute(st->ld, e, ber)) {
    struct berval **value_list = ldap_get_values_len(st->ld, e, attr);
    if (value_list != NULL) {
      struct berval value_data = *value_list[0];

      if (strncmp(attr, "dbfilename-", strlen("dbfilename-")) == 0) {
        int num = atoll(attr + strlen("dbfilename-"));
        current_num = num;
        sfree(current_filename);
        current_filename = strdup(value_data.bv_val);
        if (current_filename == NULL)
          current_num = -1;
      } else if (strncmp(attr, "dbfilecachehit-", strlen("dbfilecachehit-")) == 0) {
        int num = atoll(attr + strlen("dbfilecachehit-"));
        if (num == current_num) {
          counter_t value = atoll(value_data.bv_val);
          metric_family_append(&st->fams[FAM_DS389_BACKEND_DBFILE_CACHE_HIT],
                               "filename", current_filename,
                               (value_t){.counter = value}, tmpl);
        }
      } else if (strncmp(attr, "dbfilecachemiss-", strlen("dbfilecachemiss-")) == 0) {
        int num = atoll(attr + strlen("dbfilecachemiss-"));
        if (num == current_num) {
          counter_t value = atoll(value_data.bv_val);
          metric_family_append(&st->fams[FAM_DS389_BACKEND_DBFILE_CACHE_MISS],
                               "filename", current_filename,
                               (value_t){.counter = value}, tmpl);
        }
      } else if (strncmp(attr, "dbfilepagein-", strlen("dbfilepagein-")) == 0) {
        int num = atoll(attr + strlen("dbfilepagein-"));
        if (num == current_num) {
          counter_t value = atoll(value_data.bv_val);
          metric_family_append(&st->fams[FAM_DS389_BACKEND_DBFILE_PAGEIN],
                               "filename", current_filename,
                               (value_t){.counter = value}, tmpl);
        }
      } else if (strncmp(attr, "dbfilepageout-", strlen("dbfilepageout-")) == 0) {
        int num = atoll(attr + strlen("dbfilepageout-"));
        if (num == current_num) {
          counter_t value = atoll(value_data.bv_val);
          metric_family_append(&st->fams[FAM_DS389_BACKEND_DBFILE_PAGEOUT],
                               "filename", current_filename,
                               (value_t){.counter = value}, tmpl);
        }
      }

      ldap_value_free_len(value_list);
    }
    if (attr != NULL)
      ldap_memfree(attr);
  }
  if (ber != NULL)
    ber_free(ber, 0);

  sfree(current_filename);
  ldap_msgfree(result);

  return 0;
}

static bool ds389_cmp_rdn(LDAPRDN rdn, char *rdn_attr, char *rdn_value)
{
  LDAPAVA *attr = rdn[0];

  if ((attr->la_flags & LDAP_AVA_STRING) == 0)
    return false;

  if (strcmp(rdn_attr, attr->la_attr.bv_val))
    return false;

  if (strcmp(rdn_value, attr->la_value.bv_val))
    return false;

  return true;
}

static char *ds389_get_2rdn(char *dn, char *rdn0, char *rdn2)
{
  LDAPDN ldn;

  int ret = ldap_str2dn(dn, &ldn, LDAP_DN_FORMAT_LDAPV3);
  if (ret != LDAP_SUCCESS) {
    ERROR("ds389 plugin: ldap_str2dn failed");
    return NULL;
  }

  if (ldn == NULL)
    return NULL;

  int rdnc = 0;
  for (; ldn != NULL && ldn[rdnc]; rdnc++) ;
  
  if (rdnc < 4) {
    ldap_dnfree(ldn);
    return NULL;
  }

  if ((rdn0 != NULL) && !ds389_cmp_rdn(ldn[0], "cn", rdn0)) {
    ldap_dnfree(ldn);
    return NULL;
  }

  if ((rdn2 != NULL) && !ds389_cmp_rdn(ldn[2], "cn", rdn2)) {
    ldap_dnfree(ldn);
    return NULL;
  }

  LDAPRDN rdn = ldn[1];
  LDAPAVA *attr = rdn[0];

  char *str = strdup(attr->la_value.bv_val);
  if (str == NULL) {
    ERROR("ds389 plugin: strdup failed");
  }

  ldap_dnfree(ldn);
  return str;
}

static int ds389_list_backends(ds389_t *st, metric_t *tmpl)
{
  char *attrs[] = {"dn", NULL};
  LDAPMessage *result;
  int rc = ldap_search_ext_s(st->ld, "cn=ldbm database,cn=plugins,cn=config",
                             LDAP_SCOPE_SUBTREE, "(cn=monitor)", attrs, 0, NULL,
                             NULL, NULL, 0, &result);

  if (rc != LDAP_SUCCESS) {
    ERROR("ds389 plugin: Failed to execute search: %s", ldap_err2string(rc));
    ldap_msgfree(result);
    return -1;
  }

  for (LDAPMessage *e = ldap_first_entry(st->ld, result); e != NULL;
       e = ldap_next_entry(st->ld, e)) {
    char *dn = ldap_get_dn(st->ld, e);
    if (dn == NULL)
      continue;

    char *backend = ds389_get_2rdn(dn, "monitor", "ldbm database");
    if (backend != NULL) {
      metric_label_set(tmpl, "backend", backend);
      ds389_read_backend_metrics(st, dn, tmpl);
    }

    sfree(backend);
    ldap_memfree(dn);
  }

  metric_label_set(tmpl, "backend", NULL);

  ldap_msgfree(result);
  return 0;
}

static int ds389_read_replica(ds389_t *st, char *dn, metric_t *tmpl)
{
  LDAPMessage *result = NULL;
  int rc = ldap_search_ext_s(st->ld, dn, LDAP_SCOPE_BASE, "(objectClass=*)",
                             NULL, 0, NULL, NULL, NULL, 0, &result);

  if (rc != LDAP_SUCCESS) {
    ERROR("ds389 plugin: Failed to execute search: %s", ldap_err2string(rc));
    ldap_msgfree(result);
    return -1;
  }

  LDAPMessage *e = ldap_first_entry(st->ld, result);
  if (e == NULL) {
    ERROR("ds389 plugin: Failed to get first entry");
    ldap_msgfree(result);
    return -1;
  }

  struct berval **value_list;
  value_list = ldap_get_values_len(st->ld, e, "cn");
  if (value_list != NULL) {
    struct berval value_data = *value_list[0];
    metric_label_set(tmpl, "replica", value_data.bv_val);
    ldap_value_free_len(value_list);
  }

  value_list = ldap_get_values_len(st->ld, e, "nsds5replicahost");
  if (value_list != NULL) {
    struct berval value_data = *value_list[0];
    metric_label_set(tmpl, "host", value_data.bv_val);
    ldap_value_free_len(value_list);
  }

  value_list = ldap_get_values_len(st->ld, e, "nsds5replicaport");
  if (value_list != NULL) {
    struct berval value_data = *value_list[0];
    metric_label_set(tmpl, "port", value_data.bv_val);
    ldap_value_free_len(value_list);
  }

  value_list = ldap_get_values_len(st->ld, e, "nsds5replicaroot");
  if (value_list != NULL) {
    struct berval value_data = *value_list[0];
    metric_label_set(tmpl, "root", value_data.bv_val);
    ldap_value_free_len(value_list);
  }

  value_list = ldap_get_values_len(st->ld, e, "nsds5replicaLastUpdateStatus");
  if (value_list != NULL) {
    struct berval value_data = *value_list[0];
    gauge_t status = strtoll(value_data.bv_val, NULL, 0);
    metric_family_append(&st->fams[FAM_DS389_REPLICA_LAST_UPDATE_STATUS],
                         NULL, NULL, (value_t){.gauge = status}, tmpl);
    ldap_value_free_len(value_list);
  }

  value_list = ldap_get_values_len(st->ld, e, "nsds5replicaLastUpdateStart");
  if (value_list != NULL) {
    struct berval value_data = *value_list[0];
    struct tm tm = {0};
    if (strptime(value_data.bv_val,"%Y%m%d%H%M%S", &tm) != NULL) {
      time_t start = mktime(&tm);
      metric_family_append(&st->fams[FAM_DS389_REPLICA_LAST_UPDATE_START_SECONDS],
                           NULL, NULL, (value_t){.gauge = start}, tmpl);
    }
    ldap_value_free_len(value_list);
  }

  value_list = ldap_get_values_len(st->ld, e, "nsds5replicaLastUpdateEnd");
  if (value_list != NULL) {
    struct berval value_data = *value_list[0];
    struct tm tm = {0};
    if (strptime(value_data.bv_val,"%Y%m%d%H%M%S", &tm) != NULL) {
      time_t end = mktime(&tm);
      metric_family_append(&st->fams[FAM_DS389_REPLICA_LAST_UPDATE_END_SECONDS],
                           NULL, NULL, (value_t){.gauge = end}, tmpl);
    }
    ldap_value_free_len(value_list);
  }

  metric_label_set(tmpl, "replica", NULL);
  metric_label_set(tmpl, "host", NULL);
  metric_label_set(tmpl, "port", NULL);
  metric_label_set(tmpl, "root", NULL);

  ldap_msgfree(result);

  return 0;
}

static int ds389_list_replications(ds389_t *st, metric_t *tmpl)
{
  char *attrs[] = {"dn", NULL};
  LDAPMessage *result;
  int rc = ldap_search_ext_s(st->ld, "cn=config", LDAP_SCOPE_SUBTREE, 
                             "(objectClass=nsDS5ReplicationAgreement)", attrs,
                             0, NULL, NULL, NULL, 0, &result); 
  if (rc != LDAP_SUCCESS) {
    ERROR("ds389 plugin: Failed to execute search: %s", ldap_err2string(rc));
    ldap_msgfree(result);
    return -1;
  }

  for (LDAPMessage *e = ldap_first_entry(st->ld, result); e != NULL;
       e = ldap_next_entry(st->ld, e)) {
    char *dn = ldap_get_dn(st->ld, e);
    if (dn == NULL)
      continue;
    ds389_read_replica(st, dn, tmpl);
    ldap_memfree(dn);
  }
  ldap_msgfree(result);


  return 0;
}

static int ds389_list_links(ds389_t *st, metric_t *tmpl)
{
  char *attrs[] = {"dn", NULL};
  LDAPMessage *result;
  int rc = ldap_search_ext_s(st->ld, "cn=chaining database,cn=plugins,cn=config",
                        LDAP_SCOPE_SUBTREE, "(cn=monitor)", attrs, 0, NULL, NULL,
                        NULL, 0, &result);
  if (rc != LDAP_SUCCESS) {
    ERROR("ds389 plugin: Failed to execute search: %s", ldap_err2string(rc));
    ldap_msgfree(result);
    return -1;
  }

  for (LDAPMessage *e = ldap_first_entry(st->ld, result); e != NULL;
       e = ldap_next_entry(st->ld, e)) {
    char *dn = ldap_get_dn(st->ld, e);
    if (dn == NULL)
      continue;

    char *link= ds389_get_2rdn(dn, "monitor", "chaining database");
    if (link != NULL) {
      metric_label_set(tmpl, "link", link);
      ds389_read_metrics(st, dn, ds389_metrics_link,
                         ds389_metrics_link_size, tmpl);
    }

    sfree(link);
    ldap_memfree(dn);
  }

  metric_label_set(tmpl, "link", NULL);

  ldap_msgfree(result);
  return 0;
}

static int ds389_ping(ds389_t *st)
{
  char *attrs[] = {"dn", NULL};
  LDAPMessage *result;
  int rc = ldap_search_ext_s(st->ld, "", LDAP_SCOPE_BASE, 
                             "(objectClass=top)", attrs,
                             0, NULL, NULL, NULL, 0, &result); 
  if (rc != LDAP_SUCCESS) {
    ERROR("ds389 plugin: Failed to execute search: %s", ldap_err2string(rc));
    ldap_msgfree(result);
    return -1;
  }

  ldap_msgfree(result);

  return 0;
}

static int ds389_read_host(user_data_t *ud)
{
  if ((ud == NULL) || (ud->data == NULL)) {
    ERROR("ds389 plugin: ds389_read_host: Invalid user data.");
    return -1;
  }

  ds389_t *st = (ds389_t *)ud->data;

  metric_t tmpl = {0};

  metric_label_set(&tmpl, "instance", st->name);
  for (size_t i = 0; i < st->labels.num; i++) {
    metric_label_set(&tmpl, st->labels.ptr[i].name, st->labels.ptr[i].value);
  }

  int status = ds389_init_host(st);
  if (status >= 0) 
    status = ds389_ping(st);

  if (status < 0) {
    if (st->ld != NULL) {
      ldap_unbind_ext_s(st->ld, NULL, NULL);
      st->ld = NULL;
    }
   
    tmpl.value.gauge = 0;
    metric_family_metric_append(&st->fams[FAM_DS389_UP], tmpl);
    status = plugin_dispatch_metric_family(&st->fams[FAM_DS389_UP]);
    if (status != 0)
      ERROR("ds389 plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
    metric_family_metric_reset(&st->fams[FAM_DS389_UP]);
    metric_reset(&tmpl);
    return 0;
  }

  tmpl.value.gauge = 1;
  metric_family_metric_append(&st->fams[FAM_DS389_UP], tmpl);

  status = ds389_read_metrics(st, "cn=monitor",
                              ds389_metrics_monitor, ds389_metrics_monitor_size, &tmpl); 
  if (status < 0)
    ERROR("ds389 plugin: error reading \"cn=monitor\" metrics");

  status = ds389_read_metrics(st, "cn=snmp,cn=monitor",
                              ds389_metrics_snmp, ds389_metrics_snmp_size, &tmpl); 
  if (status < 0)
    ERROR("ds389 plugin: error reading \"cn=snmp,cn=monitor\" metrics");

  status = ds389_read_metrics(st,"cn=monitor,cn=ldbm database,cn=plugins,cn=config",
                              ds389_metrics_cache, ds389_metrics_cache_size, &tmpl);
  if (status < 0)
    ERROR("ds389 plugin: error reading \"cn=monitor,cn=ldbm database,cn=plugins,cn=config\" metrics");

  status = ds389_read_metrics(st,"cn=database,cn=monitor,cn=ldbm database,cn=plugins,cn=config",
                              ds389_metrics_db, ds389_metrics_db_size, &tmpl);
  if (status < 0)
    ERROR("ds389 plugin: error reading \"cn=database,cn=monitor,cn=ldbm database,cn=plugins,cn=config\" metrics");

  status = ds389_list_backends(st, &tmpl);
  if (status < 0)
    ERROR("ds389 plugin: error reading bakend metrics");

  status = ds389_list_links(st, &tmpl);
  if (status < 0)
    ERROR("ds389 plugin: error reading links metrics");

  status = ds389_list_replications(st, &tmpl);
  if (status < 0)
    ERROR("ds389 plugin: error reading replication metrics");

  metric_reset(&tmpl);

  for (int i = 0; i < FAM_DS389_MAX; i++) {
    if (st->fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&st->fams[i]);
      if (status != 0) {
        ERROR("ds389 plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      }
      metric_family_metric_reset(&st->fams[i]);
    }
  }

  return 0;
}

/* Configuration handling functions
 *
 * <Plugin ds389>
 *   <Instance "instance1">
 *     URL "ldap://localhost"
 *     ...
 *   </Instance>
 * </Plugin>
 */

static int ds389_config_add(oconfig_item_t *ci)
{
  ds389_t *st;
  int status;

  st = calloc(1, sizeof(*st));
  if (st == NULL) {
    ERROR("ds389 plugin: calloc failed.");
    return -1;
  }

  memcpy(st->fams, fams_ds389, FAM_DS389_MAX * sizeof(st->fams[0]));

  status = cf_util_get_string(ci, &st->name);
  if (status != 0) {
    sfree(st);
    return status;
  }

  st->starttls = false;
  st->verifyhost = true;
  st->version = LDAP_VERSION3;

  cdtime_t interval = 0;
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;

    if (strcasecmp("BindDN", child->key) == 0)
      status = cf_util_get_string(child, &st->binddn);
    else if (strcasecmp("Password", child->key) == 0)
      status = cf_util_get_string(child, &st->password);
    else if (strcasecmp("CACert", child->key) == 0)
      status = cf_util_get_string(child, &st->cacert);
    else if (strcasecmp("StartTLS", child->key) == 0)
      status = cf_util_get_boolean(child, &st->starttls);
    else if (strcasecmp("Timeout", child->key) == 0)
      status = cf_util_get_int(child, &st->timeout);
    else if (strcasecmp("URL", child->key) == 0)
      status = cf_util_get_string(child, &st->url);
    else if (strcasecmp("VerifyHost", child->key) == 0)
      status = cf_util_get_boolean(child, &st->verifyhost);
    else if (strcasecmp("Version", child->key) == 0)
      status = cf_util_get_int(child, &st->version);
    else if (strcasecmp("Interval", child->key) == 0)
      status = cf_util_get_cdtime(child, &interval);
    else if (strcasecmp("Label", child->key) == 0)
      status = cf_util_get_label(child, &st->labels);
    else {
      WARNING("ds389 plugin: Option `%s' not allowed here.", child->key);
      status = -1;
    }

    if (status != 0)
      break;
  }

  /* Check if struct is complete.. */
  if ((status == 0) && (st->url == NULL)) {
    ERROR("ds389 plugin: Instance `%s': No URL has been configured.", st->name);
    status = -1;
  }

  /* Check if URL is valid */
  if ((status == 0) && (st->url != NULL)) {
    LDAPURLDesc *ludpp;

    if (ldap_url_parse(st->url, &ludpp) != 0) {
      ERROR("ds389 plugin: Instance `%s': Invalid URL: `%s'", st->name, st->url);
      status = -1;
    }

    ldap_free_urldesc(ludpp);
  }

  if (status != 0) {
    ds389_free(st);
    return -1;
  }

  st->timeout = (long)CDTIME_T_TO_TIME_T(interval);

  char callback_name[2 * DATA_MAX_NAME_LEN] = {0};
  ssnprintf(callback_name, sizeof(callback_name), "ds389/%s",
            (st->name != NULL) ? st->name : "default");

  return plugin_register_complex_read(/* group = */ NULL,
                                      /* name      = */ callback_name,
                                      /* callback  = */ ds389_read_host,
                                      /* interval  = */ interval,
                                      &(user_data_t){
                                          .data = st,
                                          .free_func = ds389_free,
                                      });
}

static int ds389_config(oconfig_item_t *ci)
{
  int status = 0;

  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;

    if (strcasecmp("Instance", child->key) == 0)
      ds389_config_add(child);
    else
      WARNING("ds389 plugin: The configuration option \"%s\" is not allowed here. "
              "Did you forget to add an <Instance /> blocki around the configuration?",
              child->key);
  }

  return status;
}

void module_register(void) {
  plugin_register_complex_config("ds389", ds389_config);
}
