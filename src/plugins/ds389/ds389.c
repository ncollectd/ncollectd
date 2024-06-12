// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "config.h"

#if defined(STRPTIME_NEEDS_STANDARDS) || defined(HAVE_STRPTIME_STANDARDS)
#ifndef _ISOC99_SOURCE
#define _ISOC99_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#endif

#include "plugin.h"
#include "libutils/common.h"

#include <lber.h>
#include <ldap.h>

#include <time.h>

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
    plugin_filter_t *filter;
    LDAP *ld;
    metric_family_t fams[FAM_DS389_MAX];
} ds389_ctx_t;

typedef struct {
    char *attr;
    int fam;
} ds389_metric_t;

static ds389_metric_t ds389_metrics_monitor[] = {
    { "version",                        FAM_DS389_VERSION_INFO                   },
    { "starttime",                      FAM_DS389_START_TIME_SECONDS             },
    { "threads",                        FAM_DS389_THREADS                        },
    { "currentconnections",             FAM_DS389_CURRENT_CONNECTIONS            },
    { "totalconnections",               FAM_DS389_CONNECTIONS                    },
    { "currentconnectionsatmaxthreads", FAM_DS389_CURRENT_CONNECTIONS_MAXTHREADS },
    { "maxthreadsperconnhits",          FAM_DS389_CONNECTIONS_MAXTHREADS         },
    { "dtablesize",                     FAM_DS389_DTABLESIZE                     },
    { "readwaiters",                    FAM_DS389_READWAITERS                    },
    { "opsinitiated",                   FAM_DS389_OPS_INITIATED                  },
    { "opscompleted",                   FAM_DS389_OPS_COMPLETED                  },
    { "entriessent",                    FAM_DS389_SEND_ENTRIES                   },
    { "nbackends",                      FAM_DS389_NBACKENDS                      },
};
static size_t ds389_metrics_monitor_size = STATIC_ARRAY_SIZE(ds389_metrics_monitor);

static ds389_metric_t ds389_metrics_snmp[] = {
    { "anonymousbinds",        FAM_DS389_BINDS_ANONYMOUS        },
    { "unauthbinds",           FAM_DS389_BINDS_UNAUTH           },
    { "simpleauthbinds",       FAM_DS389_BINDS_SIMPLEAUTH       },
    { "strongauthbinds",       FAM_DS389_BINDS_STRONGAUTH       },
    { "bindsecurityerrors",    FAM_DS389_BINDS_SECURITYERRORS   },
    { "inops",                 FAM_DS389_OPS_IN                 },
    { "compareops",            FAM_DS389_OPS_COMPARE            },
    { "addentryops",           FAM_DS389_OPS_ADDENTRY           },
    { "removeentryops",        FAM_DS389_OPS_REMOVEENTRY        },
    { "modifyentryops",        FAM_DS389_OPS_MODIFYENTRY        },
    { "modifyrdnops",          FAM_DS389_OPS_MODIFYRDN          },
    { "searchops",             FAM_DS389_OPS_SEARCH             },
    { "onelevelsearchops",     FAM_DS389_OPS_ONELEVELSEARCH     },
    { "wholesubtreesearchops", FAM_DS389_OPS_WHOLESUBTREESEARCH },
    { "referrals",             FAM_DS389_REFERRALS              },
    { "securityerrors",        FAM_DS389_SECURITYERRORS         },
    { "errors",                FAM_DS389_ERRORS                 },
    { "bytesrecv",             FAM_DS389_RECV_BYTES             },
    { "bytessent",             FAM_DS389_SENT_BYTES             },
    { "entriesreturned",       FAM_DS389_ENTRIES_RETURNED       },
    { "referralsreturned",     FAM_DS389_REFERRALS_RETURNED     },
};
static size_t ds389_metrics_snmp_size = STATIC_ARRAY_SIZE(ds389_metrics_snmp);

static ds389_metric_t ds389_metrics_cache[] = {
    { "dbcachehits",                   FAM_DS389_DB_CACHE_HITS         },
    { "dbcachetries",                  FAM_DS389_DB_CACHE_TRIES        },
    { "dbcachehitratio",               FAM_DS389_DB_CACHE_HIT_RATIO    },
    { "dbcachepagein",                 FAM_DS389_DB_CACHE_PAGEIN       },
    { "dbcachepageout",                FAM_DS389_DB_CACHE_PAGEOUT      },
    { "dbcacheroevict",                FAM_DS389_DB_CACHE_ROEVICT      },
    { "dbcacherwevict",                FAM_DS389_DB_CACHE_RWEVICT      },
    { "normalizeddncachetries",        FAM_DS389_NDN_CACHE_TRIES       },
    { "normalizeddncachehits",         FAM_DS389_NDN_CACHE_HITS        },
    { "normalizeddncachemisses",       FAM_DS389_NDN_CACHE_MISSES      },
    { "normalizeddncachehitratio",     FAM_DS389_NDN_CACHE_HIT_RATIO   },
    { "normalizeddncacheevictions",    FAM_DS389_NDN_CACHE_EVICTIONS   },
    { "currentnormalizeddncachesize",  FAM_DS389_NDN_CACHE_SIZE_BYTES  },
    { "maxnormalizeddncachesize",      FAM_DS389_NDN_CACHE_MAX_SIZE    },
    { "currentnormalizeddncachecount", FAM_DS389_NDN_CACHE_COUNT       },
    { "normalizeddncachethreadsize",   FAM_DS389_NDN_CACHE_THREADSIZE  },
    { "normalizeddncachethreadslots",  FAM_DS389_NDN_CACHE_THREADSLOTS },
};
static size_t ds389_metrics_cache_size = STATIC_ARRAY_SIZE(ds389_metrics_cache);

static ds389_metric_t ds389_metrics_db[] = {
    { "nsslapd-db-abort-rate",                 FAM_DS389_DB_ABORT_RATE                 },
    { "nsslapd-db-active-txns",                FAM_DS389_DB_ACTIVE_TXNS                },
    { "nsslapd-db-cache-region-wait-rate",     FAM_DS389_DB_CACHE_REGION_WAIT          },
    { "nsslapd-db-cache-size-bytes",           FAM_DS389_DB_CACHE_SIZE_BYTES           },
    { "nsslapd-db-clean-pages",                FAM_DS389_DB_CLEAN_PAGES                },
    { "nsslapd-db-commit-rate",                FAM_DS389_DB_COMMIT                     },
    { "nsslapd-db-deadlock-rate",              FAM_DS389_DB_DEADLOCK                   },
    { "nsslapd-db-dirty-pages",                FAM_DS389_DB_DIRTY_PAGES                },
    { "nsslapd-db-hash-buckets",               FAM_DS389_DB_HASH_BUCKETS               },
    { "nsslapd-db-hash-elements-examine-rate", FAM_DS389_DB_HASH_ELEMENTS_EXAMINE      },
    { "nsslapd-db-hash-search-rate",           FAM_DS389_DB_HASH_SEARCH                },
    { "nsslapd-db-lock-conflicts",             FAM_DS389_DB_LOCK_CONFLICTS             },
    { "nsslapd-db-lock-region-wait-rate",      FAM_DS389_DB_LOCK_REGION_WAIT           },
    { "nsslapd-db-lock-request-rate",          FAM_DS389_DB_LOCK_REQUEST               },
    { "nsslapd-db-lockers",                    FAM_DS389_DB_LOCKERS                    },
    { "nsslapd-db-configured-locks",           FAM_DS389_DB_CONFIGURED_LOCKS           },
    { "nsslapd-db-current-locks",              FAM_DS389_DB_CURRENT_LOCKS              },
    { "nsslapd-db-max-locks",                  FAM_DS389_DB_MAX_LOCKS                  },
    { "nsslapd-db-current-lock-objects",       FAM_DS389_DB_CURRENT_LOCK_OBJECTS       },
    { "nsslapd-db-max-lock-objects",           FAM_DS389_DB_MAX_LOCK_OBJECTS           },
    { "nsslapd-db-log-bytes-since-checkpoint", FAM_DS389_DB_LOG_BYTES_SINCE_CHECKPOINT },
    { "nsslapd-db-log-region-wait-rate",       FAM_DS389_DB_LOG_REGION_WAIT            },
    { "nsslapd-db-log-write-rate",             FAM_DS389_DB_LOG_WRITE_RATE             },
    { "nsslapd-db-longest-chain-length",       FAM_DS389_DB_LONGEST_CHAIN_LENGTH       },
    { "nsslapd-db-page-create-rate",           FAM_DS389_DB_PAGE_CREATE                },
    { "nsslapd-db-page-read-rate",             FAM_DS389_DB_PAGE_READ                  },
    { "nsslapd-db-page-ro-evict-rate",         FAM_DS389_DB_PAGE_RO_EVICT              },
    { "nsslapd-db-page-rw-evict-rate",         FAM_DS389_DB_PAGE_RW_EVICT              },
    { "nsslapd-db-page-trickle-rate",          FAM_DS389_DB_PAGE_TRICKLE               },
    { "nsslapd-db-page-write-rate",            FAM_DS389_DB_PAGE_WRITE                 },
    { "nsslapd-db-pages-in-use",               FAM_DS389_DB_PAGES_IN_USE               },
    { "nsslapd-db-txn-region-wait-rate",       FAM_DS389_DB_TXN_REGION_WAIT            },
};
static size_t ds389_metrics_db_size = STATIC_ARRAY_SIZE(ds389_metrics_db);

static ds389_metric_t ds389_metrics_link[] = {
    { "nsAddCount",                 FAM_DS389_LINK_ADD                  },
    { "nsDeleteCount",              FAM_DS389_LINK_DELETE               },
    { "nsModifyCount",              FAM_DS389_LINK_MODIFY               },
    { "nsRenameCount",              FAM_DS389_LINK_RENAME               },
    { "nsSearchBaseCount",          FAM_DS389_LINK_SEARCH_BASE          },
    { "nsSearchOneLevelCount",      FAM_DS389_LINK_SEARCH_ONELEVEL      },
    { "nsSearchSubtreeCount",       FAM_DS389_LINK_SEARCH_SUBTREE       },
    { "nsAbandonCount",             FAM_DS389_LINK_ABANDON              },
    { "nsBindCount",                FAM_DS389_LINK_BIND                 },
    { "nsUnbindCount",              FAM_DS389_LINK_UNBIND               },
    { "nsCompareCount",             FAM_DS389_LINK_COMPARE              },
    { "nsOperationConnectionCount", FAM_DS389_LINK_CONNECTION_OPERATION },
    { "nsBindConnectionCount",      FAM_DS389_LINK_CONNECTION_BIND      },
};
static size_t ds389_metrics_link_size = STATIC_ARRAY_SIZE(ds389_metrics_link);

static ds389_metric_t ds389_metrics_backend[] = {
    { "readonly",               FAM_DS389_BACKEND_READONLY              },
    { "entrycachehits",         FAM_DS389_BACKEND_ENTRY_CACHE_HITS      },
    { "entrycachetries",        FAM_DS389_BACKEND_ENTRY_CACHE_TRIES     },
    { "entrycachehitratio",     FAM_DS389_BACKEND_ENTRY_CACHE_HIT_RATIO },
    { "currententrycachesize",  FAM_DS389_BACKEND_ENTRY_CACHE_SIZE      },
    { "maxentrycachesize",      FAM_DS389_BACKEND_ENTRY_CACHE_SIZE_MAX  },
    { "currententrycachecount", FAM_DS389_BACKEND_ENTRY_CACHE_COUNT     },
    { "maxentrycachecount",     FAM_DS389_BACKEND_ENTRY_CACHE_COUNT_MAX },
    { "dncachehits",            FAM_DS389_BACKEND_DN_CACHE_HITS         },
    { "dncachetries",           FAM_DS389_BACKEND_DN_CACHE_TRIES        },
    { "dncachehitratio",        FAM_DS389_BACKEND_DN_CACHE_HIT_RATIO    },
    { "currentdncachesize",     FAM_DS389_BACKEND_DN_CACHE_SIZE         },
    { "maxdncachesize",         FAM_DS389_BACKEND_DN_CACHE_SIZE_MAX     },
    { "currentdncachecount",    FAM_DS389_BACKEND_DN_CACHE_COUNT        },
    { "maxdncachecount",        FAM_DS389_BACKEND_DN_CACHE_COUNT_MAX    },
};
static size_t ds389_metrics_backend_size = STATIC_ARRAY_SIZE(ds389_metrics_backend);

static void ds389_free(void *arg)
{
    ds389_ctx_t *ctx = arg;
    if (ctx == NULL)
        return;

    free(ctx->binddn);
    free(ctx->password);
    free(ctx->cacert);
    free(ctx->name);
    free(ctx->url);
    label_set_reset(&ctx->labels);
    plugin_filter_free(ctx->filter);

    if (ctx->ld != NULL)
        ldap_unbind_ext_s(ctx->ld, NULL, NULL);

    free(ctx);
}

static int ds389_init_host(ds389_ctx_t *ctx)
{
    if (ctx->ld != NULL) {
        PLUGIN_DEBUG("Already connected to %s", ctx->url);
        return 0;
    }

    int rc = ldap_initialize(&ctx->ld, ctx->url);
    if (rc != LDAP_SUCCESS) {
        PLUGIN_ERROR("ldap_initialize failed: %s", ldap_err2string(rc));
        if (ctx->ld != NULL)
            ldap_unbind_ext_s(ctx->ld, NULL, NULL);
        ctx->ld = NULL;
        return -1;
    }

    ldap_set_option(ctx->ld, LDAP_OPT_PROTOCOL_VERSION, &ctx->version);
    ldap_set_option(ctx->ld, LDAP_OPT_TIMEOUT, &(const struct timeval){ctx->timeout, 0});
    ldap_set_option(ctx->ld, LDAP_OPT_RESTART, LDAP_OPT_ON);

    if (ctx->cacert != NULL)
        ldap_set_option(ctx->ld, LDAP_OPT_X_TLS_CACERTFILE, ctx->cacert);

    if (ctx->verifyhost == false) {
        int never = LDAP_OPT_X_TLS_NEVER;
        ldap_set_option(ctx->ld, LDAP_OPT_X_TLS_REQUIRE_CERT, &never);
    }

    if (ctx->starttls) {
        rc = ldap_start_tls_s(ctx->ld, NULL, NULL);
        if (rc != LDAP_SUCCESS) {
            PLUGIN_ERROR("Failed to start tls on %s: %s", ctx->url, ldap_err2string(rc));
            ldap_unbind_ext_s(ctx->ld, NULL, NULL);
            ctx->ld = NULL;
            return -1;
        }
    }

    struct berval cred;
    if (ctx->password != NULL) {
        cred.bv_val = ctx->password;
        cred.bv_len = strlen(ctx->password);
    } else {
        cred.bv_val = "";
        cred.bv_len = 0;
    }

    rc = ldap_sasl_bind_s(ctx->ld, ctx->binddn, LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL);
    if (rc != LDAP_SUCCESS) {
        PLUGIN_ERROR("Failed to bind to %s: %s", ctx->url, ldap_err2string(rc));
        ldap_unbind_ext_s(ctx->ld, NULL, NULL);
        ctx->ld = NULL;
        return -1;
    }

    PLUGIN_DEBUG("Successfully connected to %s", ctx->url);
    return 0;
}

static int ds389_read_metrics(ds389_ctx_t *ctx, char *filter,
                              ds389_metric_t *metrics, size_t metrics_size)
{
    LDAPMessage *result = NULL;
    int rc = ldap_search_ext_s(ctx->ld, filter, LDAP_SCOPE_BASE, "(objectClass=*)",
                               NULL, 0, NULL, NULL, NULL, 0, &result);

    if (rc != LDAP_SUCCESS) {
        PLUGIN_ERROR("Failed to execute search: %s", ldap_err2string(rc));
        ldap_msgfree(result);
        return -1;
    }

    LDAPMessage *e = ldap_first_entry(ctx->ld, result);
    if (e == NULL) {
        PLUGIN_ERROR("Failed to get first entry");
        ldap_msgfree(result);
        return -1;
    }

    for (size_t i = 0; i < metrics_size; i++) {
        value_t value = {0};
        struct berval **value_list = ldap_get_values_len(ctx->ld, e, metrics[i].attr);
        if (value_list != NULL) {
            struct berval value_data = *value_list[0];
            metric_family_t *fam = &ctx->fams[metrics[i].fam];

            if (metrics[i].fam == FAM_DS389_VERSION_INFO) {
                value = VALUE_GAUGE(1);
                metric_family_append(fam, value, &ctx->labels,
                                     &(label_pair_const_t){.name="version",
                                                           .value=value_data.bv_val},
                                     NULL);
            } else if (metrics[i].fam == FAM_DS389_START_TIME_SECONDS) {
                struct tm tm = {0};
                if (strptime(value_data.bv_val,"%Y%m%d%H%M%S", &tm) != NULL) {
                    time_t start = mktime(&tm);
                    value = VALUE_GAUGE(start);
                    metric_family_append(fam, value, &ctx->labels, NULL);
                }
            } else {
                if (fam->type == METRIC_TYPE_GAUGE)
                    value = VALUE_GAUGE((double)atoll(value_data.bv_val));
                else
                    value = VALUE_COUNTER((uint64_t)atoll(value_data.bv_val));

                metric_family_append(fam, value, &ctx->labels, NULL);
            }
            ldap_value_free_len(value_list);
        }
    }

    ldap_msgfree(result);

    return 0;
}

static int ds389_read_backend_metrics(ds389_ctx_t *ctx, char *filter)
{
    LDAPMessage *result = NULL;
    int rc = ldap_search_ext_s(ctx->ld, filter, LDAP_SCOPE_BASE, "(objectClass=*)",
                               NULL, 0, NULL, NULL, NULL, 0, &result);

    if (rc != LDAP_SUCCESS) {
        PLUGIN_ERROR("Failed to execute search: %s", ldap_err2string(rc));
        ldap_msgfree(result);
        return -1;
    }

    LDAPMessage *e = ldap_first_entry(ctx->ld, result);
    if (e == NULL) {
        PLUGIN_ERROR("Failed to get first entry");
        ldap_msgfree(result);
        return -1;
    }

    for (size_t i = 0; i < ds389_metrics_backend_size; i++) {
        metric_family_t *fam = &ctx->fams[ds389_metrics_backend[i].fam];
        char *attr = ds389_metrics_backend[i].attr;
        struct berval **value_list = ldap_get_values_len(ctx->ld, e, attr);
        if (value_list != NULL) {
            value_t value = {0};
            struct berval value_data = *value_list[0];

            if (fam->type == METRIC_TYPE_GAUGE)
                value = VALUE_GAUGE((double)atoll(value_data.bv_val));
            else
                value = VALUE_COUNTER((uint64_t)atoll(value_data.bv_val));

            metric_family_append(fam, value, &ctx->labels, NULL);

            ldap_value_free_len(value_list);
        }
    }

    int current_num = -1;
    char *current_filename = NULL;

    BerElement *ber = NULL;
    char *attr = NULL;
    for (attr = ldap_first_attribute(ctx->ld, e, &ber); attr != NULL;
             attr = ldap_next_attribute(ctx->ld, e, ber)) {
        struct berval **value_list = ldap_get_values_len(ctx->ld, e, attr);
        if (value_list != NULL) {
            struct berval value_data = *value_list[0];

            if (strncmp(attr, "dbfilename-", strlen("dbfilename-")) == 0) {
                int num = atoll(attr + strlen("dbfilename-"));
                current_num = num;
                free(current_filename);
                current_filename = strdup(value_data.bv_val);
                if (current_filename == NULL)
                    current_num = -1;
            } else if (strncmp(attr, "dbfilecachehit-", strlen("dbfilecachehit-")) == 0) {
                int num = atoll(attr + strlen("dbfilecachehit-"));
                if (num == current_num) {
                    uint64_t value = (uint64_t)atoll(value_data.bv_val);
                    metric_family_append(&ctx->fams[FAM_DS389_BACKEND_DBFILE_CACHE_HIT],
                                         VALUE_COUNTER(value), &ctx->labels,
                                         &(label_pair_const_t){.name="filename",
                                                               .value=current_filename},
                                         NULL);
                }
            } else if (strncmp(attr, "dbfilecachemiss-", strlen("dbfilecachemiss-")) == 0) {
                int num = atoll(attr + strlen("dbfilecachemiss-"));
                if (num == current_num) {
                    uint64_t value = (uint64_t)atoll(value_data.bv_val);
                    metric_family_append(&ctx->fams[FAM_DS389_BACKEND_DBFILE_CACHE_MISS],
                                         VALUE_COUNTER(value), &ctx->labels,
                                         &(label_pair_const_t){.name="filename",
                                                               .value=current_filename},
                                         NULL);
                }
            } else if (strncmp(attr, "dbfilepagein-", strlen("dbfilepagein-")) == 0) {
                int num = atoll(attr + strlen("dbfilepagein-"));
                if (num == current_num) {
                    uint64_t value = (uint64_t)atoll(value_data.bv_val);
                    metric_family_append(&ctx->fams[FAM_DS389_BACKEND_DBFILE_PAGEIN],
                                         VALUE_COUNTER(value), &ctx->labels,
                                         &(label_pair_const_t){.name="filename",
                                                               .value=current_filename},
                                         NULL);
                }
            } else if (strncmp(attr, "dbfilepageout-", strlen("dbfilepageout-")) == 0) {
                int num = atoll(attr + strlen("dbfilepageout-"));
                if (num == current_num) {
                    uint64_t value = (uint64_t)atoll(value_data.bv_val);
                    metric_family_append(&ctx->fams[FAM_DS389_BACKEND_DBFILE_PAGEOUT],
                                         VALUE_COUNTER(value), &ctx->labels,
                                         &(label_pair_const_t){.name="filename",
                                                               .value=current_filename},
                                         NULL);
                }
            }

            ldap_value_free_len(value_list);
        }
        if (attr != NULL)
            ldap_memfree(attr);
    }
    if (ber != NULL)
        ber_free(ber, 0);

    free(current_filename);
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
        PLUGIN_ERROR("ldap_str2dn failed");
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
        PLUGIN_ERROR("strdup failed");
    }

    ldap_dnfree(ldn);
    return str;
}

static int ds389_list_backends(ds389_ctx_t *ctx)
{
    char *attrs[] = {"dn", NULL};
    LDAPMessage *result;
    int rc = ldap_search_ext_s(ctx->ld, "cn=ldbm database,cn=plugins,cn=config",
                               LDAP_SCOPE_SUBTREE, "(cn=monitor)", attrs, 0, NULL,
                               NULL, NULL, 0, &result);

    if (rc != LDAP_SUCCESS) {
        PLUGIN_ERROR("Failed to execute search: %s", ldap_err2string(rc));
        ldap_msgfree(result);
        return -1;
    }

    for (LDAPMessage *e = ldap_first_entry(ctx->ld, result); e != NULL;
         e = ldap_next_entry(ctx->ld, e)) {
        char *dn = ldap_get_dn(ctx->ld, e);
        if (dn == NULL)
            continue;

        char *backend = ds389_get_2rdn(dn, "monitor", "ldbm database");
        if (backend != NULL) {
            label_set_add(&ctx->labels, true, "backend", backend);
            ds389_read_backend_metrics(ctx, dn);
        }

        free(backend);
        ldap_memfree(dn);
    }

    label_set_add(&ctx->labels, true, "backend", NULL);

    ldap_msgfree(result);
    return 0;
}

static int ds389_read_replica(ds389_ctx_t *ctx, char *dn)
{
    LDAPMessage *result = NULL;
    int rc = ldap_search_ext_s(ctx->ld, dn, LDAP_SCOPE_BASE, "(objectClass=*)",
                               NULL, 0, NULL, NULL, NULL, 0, &result);

    if (rc != LDAP_SUCCESS) {
        PLUGIN_ERROR("Failed to execute search: %s", ldap_err2string(rc));
        ldap_msgfree(result);
        return -1;
    }

    LDAPMessage *e = ldap_first_entry(ctx->ld, result);
    if (e == NULL) {
        PLUGIN_ERROR("Failed to get first entry");
        ldap_msgfree(result);
        return -1;
    }

    struct berval **value_list;
    value_list = ldap_get_values_len(ctx->ld, e, "cn");
    if (value_list != NULL) {
        struct berval value_data = *value_list[0];
        label_set_add(&ctx->labels, true, "replica", value_data.bv_val);
        ldap_value_free_len(value_list);
    }

    value_list = ldap_get_values_len(ctx->ld, e, "nsds5replicahost");
    if (value_list != NULL) {
        struct berval value_data = *value_list[0];
        label_set_add(&ctx->labels, true, "host", value_data.bv_val);
        ldap_value_free_len(value_list);
    }

    value_list = ldap_get_values_len(ctx->ld, e, "nsds5replicaport");
    if (value_list != NULL) {
        struct berval value_data = *value_list[0];
        label_set_add(&ctx->labels, true, "port", value_data.bv_val);
        ldap_value_free_len(value_list);
    }

    value_list = ldap_get_values_len(ctx->ld, e, "nsds5replicaroot");
    if (value_list != NULL) {
        struct berval value_data = *value_list[0];
        label_set_add(&ctx->labels, true, "root", value_data.bv_val);
        ldap_value_free_len(value_list);
    }

    value_list = ldap_get_values_len(ctx->ld, e, "nsds5replicaLastUpdateStatus");
    if (value_list != NULL) {
        struct berval value_data = *value_list[0];
        double status = strtoll(value_data.bv_val, NULL, 0);
        metric_family_append(&ctx->fams[FAM_DS389_REPLICA_LAST_UPDATE_STATUS],
                             VALUE_GAUGE(status), &ctx->labels, NULL);
        ldap_value_free_len(value_list);
    }

    value_list = ldap_get_values_len(ctx->ld, e, "nsds5replicaLastUpdateStart");
    if (value_list != NULL) {
        struct berval value_data = *value_list[0];
        struct tm tm = {0};
        if (strptime(value_data.bv_val,"%Y%m%d%H%M%S", &tm) != NULL) {
            time_t start = mktime(&tm);
            metric_family_append(&ctx->fams[FAM_DS389_REPLICA_LAST_UPDATE_START_SECONDS],
                                 VALUE_GAUGE(start), &ctx->labels, NULL);
        }
        ldap_value_free_len(value_list);
    }

    value_list = ldap_get_values_len(ctx->ld, e, "nsds5replicaLastUpdateEnd");
    if (value_list != NULL) {
        struct berval value_data = *value_list[0];
        struct tm tm = {0};
        if (strptime(value_data.bv_val,"%Y%m%d%H%M%S", &tm) != NULL) {
            time_t end = mktime(&tm);
            metric_family_append(&ctx->fams[FAM_DS389_REPLICA_LAST_UPDATE_END_SECONDS],
                                 VALUE_GAUGE(end), &ctx->labels, NULL);
        }
        ldap_value_free_len(value_list);
    }

    label_set_add(&ctx->labels, true, "replica", NULL);
    label_set_add(&ctx->labels, true, "host", NULL);
    label_set_add(&ctx->labels, true, "port", NULL);
    label_set_add(&ctx->labels, true, "root", NULL);

    ldap_msgfree(result);

    return 0;
}

static int ds389_list_replications(ds389_ctx_t *ctx)
{
    char *attrs[] = {"dn", NULL};
    LDAPMessage *result;
    int rc = ldap_search_ext_s(ctx->ld, "cn=config", LDAP_SCOPE_SUBTREE,
                               "(objectClass=nsDS5ReplicationAgreement)", attrs,
                               0, NULL, NULL, NULL, 0, &result);
    if (rc != LDAP_SUCCESS) {
        PLUGIN_ERROR("Failed to execute search: %s", ldap_err2string(rc));
        ldap_msgfree(result);
        return -1;
    }

    for (LDAPMessage *e = ldap_first_entry(ctx->ld, result); e != NULL;
         e = ldap_next_entry(ctx->ld, e)) {
        char *dn = ldap_get_dn(ctx->ld, e);
        if (dn == NULL)
            continue;
        ds389_read_replica(ctx, dn);
        ldap_memfree(dn);
    }
    ldap_msgfree(result);


    return 0;
}

static int ds389_list_links(ds389_ctx_t *ctx)
{
    char *attrs[] = {"dn", NULL};
    LDAPMessage *result;
    int rc = ldap_search_ext_s(ctx->ld, "cn=chaining database,cn=plugins,cn=config",
                               LDAP_SCOPE_SUBTREE, "(cn=monitor)", attrs, 0, NULL, NULL,
                               NULL, 0, &result);
    if (rc != LDAP_SUCCESS) {
        PLUGIN_ERROR("Failed to execute search: %s", ldap_err2string(rc));
        ldap_msgfree(result);
        return -1;
    }

    for (LDAPMessage *e = ldap_first_entry(ctx->ld, result); e != NULL;
         e = ldap_next_entry(ctx->ld, e)) {
        char *dn = ldap_get_dn(ctx->ld, e);
        if (dn == NULL)
            continue;

        char *link= ds389_get_2rdn(dn, "monitor", "chaining database");
        if (link != NULL) {
            label_set_add(&ctx->labels, true, "link", link);
            int status = ds389_read_metrics(ctx, dn, ds389_metrics_link, ds389_metrics_link_size);
            if (status < 0)
                PLUGIN_ERROR("error reading \"%s\" metrics", dn);
        }

        free(link);
        ldap_memfree(dn);
    }

    label_set_add(&ctx->labels, true, "link", NULL);

    ldap_msgfree(result);
    return 0;
}

static int ds389_ping(ds389_ctx_t *ctx)
{
    char *attrs[] = {"dn", NULL};
    LDAPMessage *result;
    int rc = ldap_search_ext_s(ctx->ld, "", LDAP_SCOPE_BASE,
                               "(objectClass=top)", attrs, 0, NULL, NULL, NULL, 0, &result);
    if (rc != LDAP_SUCCESS) {
        PLUGIN_ERROR("Failed to execute search: %s", ldap_err2string(rc));
        ldap_msgfree(result);
        return -1;
    }

    ldap_msgfree(result);

    return 0;
}

static int ds389_read(user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    ds389_ctx_t *ctx = ud->data;

    int status = ds389_init_host(ctx);
    if (status >= 0)
        status = ds389_ping(ctx);

    if (status < 0) {
        if (ctx->ld != NULL) {
            ldap_unbind_ext_s(ctx->ld, NULL, NULL);
            ctx->ld = NULL;
        }

        metric_family_append(&ctx->fams[FAM_DS389_UP], VALUE_GAUGE(0), &ctx->labels, NULL);

        plugin_dispatch_metric_family_filtered(&ctx->fams[FAM_DS389_UP], ctx->filter, 0);
        return 0;
    }

    metric_family_append(&ctx->fams[FAM_DS389_UP], VALUE_GAUGE(1), &ctx->labels, NULL);

    status = ds389_read_metrics(ctx, "cn=monitor",
                                ds389_metrics_monitor, ds389_metrics_monitor_size);
    if (status < 0)
        PLUGIN_ERROR("error reading \"cn=monitor\" metrics");

    status = ds389_read_metrics(ctx, "cn=snmp,cn=monitor",
                                ds389_metrics_snmp, ds389_metrics_snmp_size);
    if (status < 0)
        PLUGIN_ERROR("error reading \"cn=snmp,cn=monitor\" metrics");

    status = ds389_read_metrics(ctx,"cn=monitor,cn=ldbm database,cn=plugins,cn=config",
                                ds389_metrics_cache, ds389_metrics_cache_size);
    if (status < 0)
        PLUGIN_ERROR("error reading \"cn=monitor,cn=ldbm database,cn=plugins,cn=config\" metrics");

    status = ds389_read_metrics(ctx,"cn=database,cn=monitor,cn=ldbm database,cn=plugins,cn=config",
                                ds389_metrics_db, ds389_metrics_db_size);
    if (status < 0)
        PLUGIN_ERROR("error reading "
                     "\"cn=database,cn=monitor,cn=ldbm database,cn=plugins,cn=config\" metrics");

    status = ds389_list_backends(ctx);
    if (status < 0)
        PLUGIN_ERROR("error reading bakend metrics");

    status = ds389_list_links(ctx);
    if (status < 0)
        PLUGIN_ERROR("error reading links metrics");

    status = ds389_list_replications(ctx);
    if (status < 0)
        PLUGIN_ERROR("error reading replication metrics");

    plugin_dispatch_metric_family_array_filtered(ctx->fams, FAM_DS389_MAX, ctx->filter, 0);
    return 0;
}

static int ds389_config_add(config_item_t *ci)
{
    ds389_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed: %s.", STRERRNO);
        return -1;
    }

    memcpy(ctx->fams, fams_ds389, FAM_DS389_MAX * sizeof(ctx->fams[0]));

    int status = cf_util_get_string(ci, &ctx->name);
    if (status != 0) {
        free(ctx);
        return status;
    }

    ctx->starttls = false;
    ctx->verifyhost = true;
    ctx->version = LDAP_VERSION3;

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("bind-dn", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->binddn);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->password);
        } else if (strcasecmp("ca-cert", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->cacert);
        } else if (strcasecmp("start-tls", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctx->starttls);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_int(child, &ctx->timeout);
        } else if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->url);
        } else if (strcasecmp("verify-host", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctx->verifyhost);
        } else if (strcasecmp("version", child->key) == 0) {
            status = cf_util_get_int(child, &ctx->version);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &ctx->filter);
        } else {
            PLUGIN_WARNING("Option '%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    /* Check if struct is complete.. */
    if ((status == 0) && (ctx->url == NULL)) {
        PLUGIN_ERROR("Instance '%s': No url has been configured.", ctx->name);
        status = -1;
    }

    /* Check if URL is valid */
    if ((status == 0) && (ctx->url != NULL)) {
        LDAPURLDesc *ludpp;

        if (ldap_url_parse(ctx->url, &ludpp) != 0) {
            PLUGIN_ERROR("Instance '%s': Invalid url: '%s'", ctx->name, ctx->url);
            status = -1;
        }

        ldap_free_urldesc(ludpp);
    }

    if (status != 0) {
        ds389_free(ctx);
        return -1;
    }

    if (ctx->timeout == 0)
        ctx->timeout = (long)CDTIME_T_TO_TIME_T(interval);

    label_set_add(&ctx->labels, true, "instance",  ctx->name);

    return plugin_register_complex_read("ds389", ctx->name, ds389_read, interval,
                                        &(user_data_t){.data = ctx, .free_func = ds389_free});
}

static int ds389_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = ds389_config_add(child);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("ds389", ds389_config);
}
