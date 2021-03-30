/**
 * collectd - src/openldap.c
 * Copyright (C) 2011       Kimo Rosenbaum
 * Copyright (C) 2014-2015  Marc Fournier
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Kimo Rosenbaum <kimor79 at yahoo.com>
 *   Marc Fournier <marc.fournier at camptocamp.com>
 **/

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#include <lber.h>
#include <ldap.h>

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
} openldap_t;

enum {
  FAM_OPENLDAP_CONNECTIONS_TOTAL = 0,
  FAM_OPENLDAP_CONNECTIONS,
  FAM_OPENLDAP_OPERATIONS_INITIATED_TOTAL,
  FAM_OPENLDAP_OPERATIONS_COMPLETED_TOTAL,
  FAM_OPENLDAP_THREADS,
  FAM_OPENLDAP_WAITERS_READ,
  FAM_OPENLDAP_WAITERS_WRITE,
  FAM_OPENLDAP_BDB_ENTRY_CACHE_SIZE,
  FAM_OPENLDAP_BDB_DN_CACHE_SIZE,
  FAM_OPENLDAP_BDB_IDL_CACHE_SIZE,
  FAM_OPENLDAP_MDB_ENTRIES,
  FAM_OPENLDAP_MDB_PAGES_MAX,
  FAM_OPENLDAP_MDB_PAGES_USED,
  FAM_OPENLDAP_MDB_PAGES_FREE,
  FAM_OPENLDAP_MDB_READERS_MAX,
  FAM_OPENLDAP_MDB_READERS_USED,
  FAM_OPENLDAP_SEND_BYTES_TOTAL,
  FAM_OPENLDAP_SEND_PDUS_TOTAL,
  FAM_OPENLDAP_SEND_ENTRIES_TOTAL,
  FAM_OPENLDAP_SEND_REFERRALS_TOTAL,
  FAM_OPENLDAP_MAX,
};

static void openldap_free(void *arg)
{
  openldap_t *st = arg;

  if (st == NULL)
    return;

  sfree(st->binddn);
  sfree(st->password);
  sfree(st->cacert);
  sfree(st->name);
  sfree(st->url);
  if (st->ld)
    ldap_unbind_ext_s(st->ld, NULL, NULL);

  sfree(st);
}

/* initialize ldap for each host */
static int openldap_init_host(openldap_t *st)
{
  int rc;

  if (st->ld) {
    DEBUG("openldap plugin: Already connected to %s", st->url);
    return 0;
  }

  rc = ldap_initialize(&st->ld, st->url);
  if (rc != LDAP_SUCCESS) {
    ERROR("openldap plugin: ldap_initialize failed: %s", ldap_err2string(rc));
    if (st->ld != NULL)
      ldap_unbind_ext_s(st->ld, NULL, NULL);
    st->ld = NULL;
    return (-1);
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
      ERROR("openldap plugin: Failed to start tls on %s: %s", st->url, ldap_err2string(rc));
      ldap_unbind_ext_s(st->ld, NULL, NULL);
      st->ld = NULL;
      return (-1);
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

  rc = ldap_sasl_bind_s(st->ld, st->binddn, LDAP_SASL_SIMPLE, &cred, NULL, NULL,
                        NULL);
  if (rc != LDAP_SUCCESS) {
    ERROR("openldap plugin: Failed to bind to %s: %s", st->url, ldap_err2string(rc));
    ldap_unbind_ext_s(st->ld, NULL, NULL);
    st->ld = NULL;
    return (-1);
  } else {
    DEBUG("openldap plugin: Successfully connected to %s", st->url);
    return 0;
  }
}

static int openldap_read_host(user_data_t *ud)
{
  metric_family_t fams[FAM_OPENLDAP_MAX] = {
    [FAM_OPENLDAP_CONNECTIONS_TOTAL] = {
      .name = "openldap_connections_total",
      .type = METRIC_TYPE_COUNTER,
      .help = "",
    },
    [FAM_OPENLDAP_CONNECTIONS] = {
      .name = "openldap_connections",
      .type = METRIC_TYPE_GAUGE,
      .help = "",
    },
    [FAM_OPENLDAP_OPERATIONS_INITIATED_TOTAL] = {
      .name = "openldap_operations_initiated_total",
      .type = METRIC_TYPE_COUNTER,
      .help = "",
    },
    [FAM_OPENLDAP_OPERATIONS_COMPLETED_TOTAL] = {
      .name = "openldap_operations_completed_total",
      .type = METRIC_TYPE_COUNTER,
      .help = "",
    },
    [FAM_OPENLDAP_THREADS] = {
      .name = "openldap_threads",
      .type = METRIC_TYPE_GAUGE,
      .help = "",
    },
    [FAM_OPENLDAP_WAITERS_READ] = {
      .name = "openldap_waiters_read",
      .type = METRIC_TYPE_GAUGE,
      .help = "",
    },
    [FAM_OPENLDAP_WAITERS_WRITE] = {
      .name = "openldap_waiters_write",
      .type = METRIC_TYPE_GAUGE,
      .help = "",
    },
    [FAM_OPENLDAP_BDB_ENTRY_CACHE_SIZE] = {
      .name = "openldap_bdb_entry_cache_size",
      .type = METRIC_TYPE_GAUGE,
      .help = "",
    },
    [FAM_OPENLDAP_BDB_DN_CACHE_SIZE] = {
      .name = "openldap_bdb_dn_cache_size",
      .type = METRIC_TYPE_GAUGE,
      .help = "",
    },
    [FAM_OPENLDAP_BDB_IDL_CACHE_SIZE] = {
      .name = "openldap_bdb_idl_cache_size",
      .type = METRIC_TYPE_GAUGE,
      .help = "",
    },
    [FAM_OPENLDAP_MDB_ENTRIES] = {
      .name = "openldap_mdb_entries",
      .type = METRIC_TYPE_GAUGE,
      .help = "",
    },
    [FAM_OPENLDAP_MDB_PAGES_MAX] = {
      .name = "openldap_mdb_pages_max",
      .type = METRIC_TYPE_GAUGE,
      .help = "",
    },
    [FAM_OPENLDAP_MDB_PAGES_USED] = {
      .name = "openldap_mdb_pages_used",
      .type = METRIC_TYPE_GAUGE,
      .help = "",
    },
    [FAM_OPENLDAP_MDB_PAGES_FREE] = {
      .name = "openldap_mdb_pages_free",
      .type = METRIC_TYPE_GAUGE,
      .help = "",
    },
    [FAM_OPENLDAP_MDB_READERS_MAX] = {
      .name = "openldap_mdb_reader_max",
      .type = METRIC_TYPE_GAUGE,
      .help = "",
    },
    [FAM_OPENLDAP_MDB_READERS_USED] = {
      .name = "openldap_mdb_readers_used",
      .type = METRIC_TYPE_GAUGE,
      .help = "",
    },
    [FAM_OPENLDAP_SEND_BYTES_TOTAL] = {
      .name = "openldap_send_bytes_total",
      .type = METRIC_TYPE_COUNTER,
      .help = "",
    },
    [FAM_OPENLDAP_SEND_PDUS_TOTAL] = {
      .name = "openldap_send_pdus_total",
      .type = METRIC_TYPE_COUNTER,
      .help = "",
    },
    [FAM_OPENLDAP_SEND_ENTRIES_TOTAL] = {
      .name = "openldap_send_entries_total",
      .type = METRIC_TYPE_COUNTER,
      .help = "",
    },
    [FAM_OPENLDAP_SEND_REFERRALS_TOTAL] = {
      .name = "openldap_send_referrals_total",
      .type = METRIC_TYPE_COUNTER,
      .help = "",
    },
  };

  char *attrs[] = {
      "monitorCounter", "monitorOpCompleted", "monitorOpInitiated",
      "monitoredInfo",  "olmBDBEntryCache",   "olmBDBDNCache",
      "olmBDBIDLCache", "namingContexts", "olmMDBPagesMax",
      "olmMDBPagesUsed", "olmMDBPagesFree", "olmMDBReadersMax",
      "olmMDBReadersUsed", "olmMDBEntries", NULL};

  if ((ud == NULL) || (ud->data == NULL)) {
    ERROR("openldap plugin: cldap_read_host: Invalid user data.");
    return -1;
  }

  openldap_t *st = (openldap_t *)ud->data;

  int status = openldap_init_host(st);
  if (status != 0)
    return -1;

  LDAPMessage *result;
  int rc = ldap_search_ext_s(st->ld, "cn=Monitor", LDAP_SCOPE_SUBTREE,
                             "(|(!(cn=* *))(cn=Database*))", attrs, 0, NULL, NULL,
                             NULL, 0, &result);

  if (rc != LDAP_SUCCESS) {
    ERROR("openldap plugin: Failed to execute search: %s", ldap_err2string(rc));
    ldap_msgfree(result);
    ldap_unbind_ext_s(st->ld, NULL, NULL);
    st->ld = NULL;
    return (-1);
  }

  metric_t m = {0};
  if (st->name != NULL)
    metric_label_set(&m, "instance", st->name);

  for (size_t i = 0; i < st->labels.num; i++) {
    metric_label_set(&m, st->labels.ptr[i].name, st->labels.ptr[i].value);
  }


  for (LDAPMessage *e = ldap_first_entry(st->ld, result); e != NULL;
       e = ldap_next_entry(st->ld, e)) {
    char *dn =  ldap_get_dn(st->ld, e);
    if (dn != NULL) {
      unsigned long long counter = 0;
      unsigned long long opc = 0;
      unsigned long long opi = 0;
      unsigned long long info = 0;

      struct berval counter_data;
      struct berval opc_data;
      struct berval opi_data;
      struct berval info_data;
      struct berval olmbdb_data;
      struct berval nc_data;

      struct berval **counter_list;
      struct berval **opc_list;
      struct berval **opi_list;
      struct berval **info_list;
      struct berval **olmbdb_list;
      struct berval **nc_list;

      if ((counter_list = ldap_get_values_len(st->ld, e, "monitorCounter")) != NULL) {
        counter_data = *counter_list[0];
        counter = atoll(counter_data.bv_val);
      }

      if ((opc_list = ldap_get_values_len(st->ld, e, "monitorOpCompleted")) != NULL) {
        opc_data = *opc_list[0];
        opc = atoll(opc_data.bv_val);
      }

      if ((opi_list = ldap_get_values_len(st->ld, e, "monitorOpInitiated")) != NULL) {
        opi_data = *opi_list[0];
        opi = atoll(opi_data.bv_val);
      }

      if ((info_list = ldap_get_values_len(st->ld, e, "monitoredInfo")) != NULL) {
        info_data = *info_list[0];
        info = atoll(info_data.bv_val);
      }

      if (strcmp(dn, "cn=Total,cn=Connections,cn=Monitor") == 0) {
        m.value.counter = counter;
        metric_family_metric_append(&fams[FAM_OPENLDAP_CONNECTIONS_TOTAL], m);
      } else if (strcmp(dn, "cn=Current,cn=Connections,cn=Monitor") == 0) {
        m.value.gauge = counter;
        metric_family_metric_append(&fams[FAM_OPENLDAP_CONNECTIONS], m);
      } else if (strcmp(dn, "cn=Operations,cn=Monitor") == 0) {
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_COMPLETED_TOTAL],
                             "operation", "all", (value_t){.counter = opc}, &m);
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_INITIATED_TOTAL],
                             "operation", "all", (value_t){.counter = opi}, &m);
      } else if (strcmp(dn, "cn=Bind,cn=Operations,cn=Monitor") == 0) {
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_COMPLETED_TOTAL],
                             "operation", "bind", (value_t){.counter = opc}, &m);
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_INITIATED_TOTAL],
                             "operation", "bind", (value_t){.counter = opi}, &m);
      } else if (strcmp(dn, "cn=UnBind,cn=Operations,cn=Monitor") == 0) {
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_COMPLETED_TOTAL],
                             "operation", "unbind", (value_t){.counter = opc}, &m);
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_INITIATED_TOTAL],
                             "operation", "unbind", (value_t){.counter = opi}, &m);
      } else if (strcmp(dn, "cn=Search,cn=Operations,cn=Monitor") == 0) {
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_COMPLETED_TOTAL],
                             "operation", "search", (value_t){.counter = opc}, &m);
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_INITIATED_TOTAL],
                             "operation", "search", (value_t){.counter = opi}, &m);
      } else if (strcmp(dn, "cn=Compare,cn=Operations,cn=Monitor") == 0) {
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_COMPLETED_TOTAL],
                             "operation", "compare", (value_t){.counter = opc}, &m);
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_INITIATED_TOTAL],
                             "operation", "compare", (value_t){.counter = opi}, &m);
      } else if (strcmp(dn, "cn=Modify,cn=Operations,cn=Monitor") == 0) {
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_COMPLETED_TOTAL],
                             "operation", "modify", (value_t){.counter = opc}, &m);
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_INITIATED_TOTAL],
                             "operation", "modify", (value_t){.counter = opi}, &m);
      } else if (strcmp(dn, "cn=Modrdn,cn=Operations,cn=Monitor") == 0) {
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_COMPLETED_TOTAL],
                             "operation", "modrdn", (value_t){.counter = opc}, &m);
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_INITIATED_TOTAL],
                             "operation", "modrdn", (value_t){.counter = opi}, &m);
      } else if (strcmp(dn, "cn=Add,cn=Operations,cn=Monitor") == 0) {
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_COMPLETED_TOTAL],
                             "operation", "add", (value_t){.counter = opc}, &m);
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_INITIATED_TOTAL],
                             "operation", "add", (value_t){.counter = opi}, &m);
      } else if (strcmp(dn, "cn=Delete,cn=Operations,cn=Monitor") == 0) {
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_COMPLETED_TOTAL],
                             "operation", "delete", (value_t){.counter = opc}, &m);
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_INITIATED_TOTAL],
                             "operation", "delete", (value_t){.counter = opi}, &m);
      } else if (strcmp(dn, "cn=Abandon,cn=Operations,cn=Monitor") == 0) {
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_COMPLETED_TOTAL],
                             "operation", "abandon", (value_t){.counter = opc}, &m);
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_INITIATED_TOTAL],
                             "operation", "abandon", (value_t){.counter = opi}, &m);
      } else if (strcmp(dn, "cn=Extended,cn=Operations,cn=Monitor") == 0) {
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_COMPLETED_TOTAL],
                             "operation", "extended", (value_t){.counter = opc}, &m);
        metric_family_append(&fams[FAM_OPENLDAP_OPERATIONS_INITIATED_TOTAL],
                             "operation", "extended", (value_t){.counter = opi}, &m);
      } else if ((strncmp(dn, "cn=Database", 11) == 0) &&
                 ((nc_list = ldap_get_values_len(st->ld, e, "namingContexts")) != NULL)) {
        nc_data = *nc_list[0];

        if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmBDBEntryCache")) != NULL) {
          olmbdb_data = *olmbdb_list[0];
          metric_family_append(&fams[FAM_OPENLDAP_BDB_ENTRY_CACHE_SIZE],
                               "database", nc_data.bv_val,
                               (value_t){.gauge = atoll(olmbdb_data.bv_val)}, &m);
          ldap_value_free_len(olmbdb_list);
        }

        if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmBDBDNCache")) != NULL) {
          olmbdb_data = *olmbdb_list[0];
          metric_family_append(&fams[FAM_OPENLDAP_BDB_DN_CACHE_SIZE],
                               "database", nc_data.bv_val,
                               (value_t){.gauge = atoll(olmbdb_data.bv_val)}, &m);
          ldap_value_free_len(olmbdb_list);
        }

        if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmBDBIDLCache")) != NULL) {
          olmbdb_data = *olmbdb_list[0];
          metric_family_append(&fams[FAM_OPENLDAP_BDB_IDL_CACHE_SIZE],
                               "database", nc_data.bv_val,
                               (value_t){.gauge = atoll(olmbdb_data.bv_val)}, &m);
          ldap_value_free_len(olmbdb_list);
        }

        if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmMDBEntries")) != NULL) {
          olmbdb_data = *olmbdb_list[0];
          metric_family_append(&fams[FAM_OPENLDAP_MDB_ENTRIES],
                               "database", nc_data.bv_val,
                               (value_t){.gauge = atoll(olmbdb_data.bv_val)}, &m);
          ldap_value_free_len(olmbdb_list);
        }

        if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmMDBPagesMax")) != NULL) {
          olmbdb_data = *olmbdb_list[0];
          metric_family_append(&fams[FAM_OPENLDAP_MDB_PAGES_MAX],
                               "database", nc_data.bv_val,
                               (value_t){.gauge = atoll(olmbdb_data.bv_val)}, &m);
          ldap_value_free_len(olmbdb_list);
        }

        if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmMDBPagesUsed")) != NULL) {
          olmbdb_data = *olmbdb_list[0];
          metric_family_append(&fams[FAM_OPENLDAP_MDB_PAGES_USED],
                               "database", nc_data.bv_val,
                               (value_t){.gauge = atoll(olmbdb_data.bv_val)}, &m);
          ldap_value_free_len(olmbdb_list);
        }

        if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmMDBPagesFree")) != NULL) {
          olmbdb_data = *olmbdb_list[0];
          metric_family_append(&fams[FAM_OPENLDAP_MDB_PAGES_FREE],
                               "database", nc_data.bv_val,
                               (value_t){.gauge = atoll(olmbdb_data.bv_val)}, &m);
          ldap_value_free_len(olmbdb_list);
        }

        if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmMDBReadersMax")) != NULL) {
          olmbdb_data = *olmbdb_list[0];
          metric_family_append(&fams[FAM_OPENLDAP_MDB_READERS_MAX],
                               "database", nc_data.bv_val,
                               (value_t){.gauge = atoll(olmbdb_data.bv_val)}, &m);
          ldap_value_free_len(olmbdb_list);
        }

        if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmMDBReadersUsed")) != NULL) {
          olmbdb_data = *olmbdb_list[0];
          metric_family_append(&fams[FAM_OPENLDAP_MDB_READERS_USED],
                               "database", nc_data.bv_val,
                               (value_t){.gauge = atoll(olmbdb_data.bv_val)}, &m);
          ldap_value_free_len(olmbdb_list);
        }

        ldap_value_free_len(nc_list);
      } else if (strcmp(dn, "cn=Bytes,cn=Statistics,cn=Monitor") == 0) {
        m.value.counter = counter;
        metric_family_metric_append(&fams[FAM_OPENLDAP_SEND_BYTES_TOTAL], m);
      } else if (strcmp(dn, "cn=PDU,cn=Statistics,cn=Monitor") == 0) {
        m.value.counter = counter;
        metric_family_metric_append(&fams[FAM_OPENLDAP_SEND_PDUS_TOTAL], m);
      } else if (strcmp(dn, "cn=Entries,cn=Statistics,cn=Monitor") == 0) {
        m.value.counter = counter;
        metric_family_metric_append(&fams[FAM_OPENLDAP_SEND_ENTRIES_TOTAL], m);
      } else if (strcmp(dn, "cn=Referrals,cn=Statistics,cn=Monitor") == 0) {
        m.value.counter = counter;
        metric_family_metric_append(&fams[FAM_OPENLDAP_SEND_REFERRALS_TOTAL], m);
      } else if (strcmp(dn, "cn=Open,cn=Threads,cn=Monitor") == 0) {
        metric_family_append(&fams[FAM_OPENLDAP_THREADS],
                            "status", "open", (value_t){.gauge = info}, &m);
      } else if (strcmp(dn, "cn=Starting,cn=Threads,cn=Monitor") == 0) {
        metric_family_append(&fams[FAM_OPENLDAP_THREADS],
                            "status", "stating", (value_t){.gauge = info}, &m);
      } else if (strcmp(dn, "cn=Active,cn=Threads,cn=Monitor") == 0) {
        metric_family_append(&fams[FAM_OPENLDAP_THREADS],
                             "status", "active", (value_t){.gauge = info}, &m);
      } else if (strcmp(dn, "cn=Pending,cn=Threads,cn=Monitor") == 0) {
        metric_family_append(&fams[FAM_OPENLDAP_THREADS],
                             "status", "pending", (value_t){.gauge = info}, &m);
      } else if (strcmp(dn, "cn=Backload,cn=Threads,cn=Monitor") == 0) {
        metric_family_append(&fams[FAM_OPENLDAP_THREADS],
                             "status", "backload", (value_t){.gauge = info}, &m);
      } else if (strcmp(dn, "cn=Read,cn=Waiters,cn=Monitor") == 0) {
        m.value.gauge = counter;
        metric_family_metric_append(&fams[FAM_OPENLDAP_WAITERS_READ], m);
      } else if (strcmp(dn, "cn=Write,cn=Waiters,cn=Monitor") == 0) {
        m.value.gauge = counter;
        metric_family_metric_append(&fams[FAM_OPENLDAP_WAITERS_WRITE], m);
      }

      ldap_value_free_len(counter_list);
      ldap_value_free_len(opc_list);
      ldap_value_free_len(opi_list);
      ldap_value_free_len(info_list);
    }

    ldap_memfree(dn);
  }

  for (size_t i = 0; i < FAM_OPENLDAP_MAX; i++) {
    if (fams[i].metric.num > 0) {
      int status = plugin_dispatch_metric_family(&fams[i]);
      if (status != 0) {
        ERROR("openldap plugin: plugin_dispatch_metric_family failed: %s", STRERROR(status));
      }
      metric_family_metric_reset(&fams[i]);
    }
  }

  ldap_msgfree(result);
  return 0;
}

/* Configuration handling functions
 *
 * <Plugin ldap>
 *   <Instance "plugin_instance1">
 *     URL "ldap://localhost"
 *     ...
 *   </Instance>
 * </Plugin>
 */

static int openldap_config_add(oconfig_item_t *ci)
{
  openldap_t *st = calloc(1, sizeof(*st));
  if (st == NULL) {
    ERROR("openldap plugin: calloc failed.");
    return -1;
  }

  int status = cf_util_get_string(ci, &st->name);
  if (status != 0) {
    sfree(st);
    return status;
  }

  st->starttls = false;
  st->timeout = (long)CDTIME_T_TO_TIME_T(plugin_get_interval());
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
      WARNING("openldap plugin: Option `%s' not allowed here.", child->key);
      status = -1;
    }

    if (status != 0)
      break;
  }

  /* Check if struct is complete.. */
  if ((status == 0) && (st->url == NULL)) {
    ERROR("openldap plugin: Instance `%s': No URL has been configured.",
          st->name);
    status = -1;
  }

  /* Check if URL is valid */
  if ((status == 0) && (st->url != NULL)) {
    LDAPURLDesc *ludpp;

    if (ldap_url_parse(st->url, &ludpp) != 0) {
      ERROR("openldap plugin: Instance `%s': Invalid URL: `%s'",
            st->name, st->url);
      status = -1;
    }

    ldap_free_urldesc(ludpp);
  }

  if (status != 0) {
    openldap_free(st);
    return -1;
  }

  char callback_name[3 * DATA_MAX_NAME_LEN] = {0};
  ssnprintf(callback_name, sizeof(callback_name), "openldap/%s",
            (st->name != NULL) ? st->name : "default");

  return plugin_register_complex_read(/* group = */ NULL,
                                      /* name      = */ callback_name,
                                      /* callback  = */ openldap_read_host,
                                      /* interval  = */ interval,
                                      &(user_data_t){
                                          .data = st,
                                          .free_func = openldap_free,
                                      });
}

static int openldap_config(oconfig_item_t *ci)
{
  int status = 0;

  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;

    if (strcasecmp("Instance", child->key) == 0)
      openldap_config_add(child);
    else
      WARNING("openldap plugin: The configuration option "
              "\"%s\" is not allowed here. Did you "
              "forget to add an <Instance /> block "
              "around the configuration?",
              child->key);
  }

  return status;
}

static int openldap_init(void)
{
  /* Initialize LDAP library while still single-threaded as recommended in
   * ldap_initialize(3) */
  int debug_level;
  ldap_get_option(NULL, LDAP_OPT_DEBUG_LEVEL, &debug_level);
  return 0;
}

void module_register(void)
{
  plugin_register_complex_config("openldap", openldap_config);
  plugin_register_init("openldap", openldap_init);
}
