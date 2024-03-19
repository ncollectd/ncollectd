// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2011 Kimo Rosenbaum
// SPDX-FileCopyrightText: Copyright (C) 2014-2015 Marc Fournier
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Kimo Rosenbaum <kimor79 at yahoo.com>
// SPDX-FileContributor: Marc Fournier <marc.fournier at camptocamp.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <lber.h>
#include <ldap.h>

enum {
    FAM_OPENLDAP_UP,
    FAM_OPENLDAP_CONNECTIONS,
    FAM_OPENLDAP_CURRENT_CONNECTIONS,
    FAM_OPENLDAP_OPERATIONS_INITIATED,
    FAM_OPENLDAP_OPERATIONS_COMPLETED,
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
    FAM_OPENLDAP_SEND_BYTES,
    FAM_OPENLDAP_SEND_PDUS,
    FAM_OPENLDAP_SEND_ENTRIES,
    FAM_OPENLDAP_SEND_REFERRALS,
    FAM_OPENLDAP_MAX,
};

static metric_family_t fams[FAM_OPENLDAP_MAX] = {
    [FAM_OPENLDAP_UP] = {
        .name = "openldap_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the openldap server be reached.",
    },
    [FAM_OPENLDAP_CONNECTIONS] = {
        .name = "openldap_connections",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of connections",
    },
    [FAM_OPENLDAP_CURRENT_CONNECTIONS] = {
        .name = "openldap_current_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of current connections",
    },
    [FAM_OPENLDAP_OPERATIONS_INITIATED] = {
        .name = "openldap_operations_initiated",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of initiated operations",
    },
    [FAM_OPENLDAP_OPERATIONS_COMPLETED] = {
        .name = "openldap_operations_completed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Total number of completed operations",
    },
    [FAM_OPENLDAP_THREADS] = {
        .name = "openldap_threads",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of threads by type",
    },
    [FAM_OPENLDAP_WAITERS_READ] = {
        .name = "openldap_waiters_read",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of read waiters",
    },
    [FAM_OPENLDAP_WAITERS_WRITE] = {
        .name = "openldap_waiters_write",
        .type = METRIC_TYPE_GAUGE,
        .help = "Current number of write waiters",
    },
    [FAM_OPENLDAP_BDB_ENTRY_CACHE_SIZE] = {
        .name = "openldap_bdb_entry_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENLDAP_BDB_DN_CACHE_SIZE] = {
        .name = "openldap_bdb_dn_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENLDAP_BDB_IDL_CACHE_SIZE] = {
        .name = "openldap_bdb_idl_cache_size",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENLDAP_MDB_ENTRIES] = {
        .name = "openldap_mdb_entries",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENLDAP_MDB_PAGES_MAX] = {
        .name = "openldap_mdb_pages_max",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENLDAP_MDB_PAGES_USED] = {
        .name = "openldap_mdb_pages_used",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENLDAP_MDB_PAGES_FREE] = {
        .name = "openldap_mdb_pages_free",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENLDAP_MDB_READERS_MAX] = {
        .name = "openldap_mdb_reader_max",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENLDAP_MDB_READERS_USED] = {
        .name = "openldap_mdb_readers_used",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_OPENLDAP_SEND_BYTES] = {
        .name = "openldap_send_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_OPENLDAP_SEND_PDUS] = {
        .name = "openldap_send_pdus",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_OPENLDAP_SEND_ENTRIES] = {
        .name = "openldap_send_entries",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_OPENLDAP_SEND_REFERRALS] = {
        .name = "openldap_send_referrals",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
};

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
    metric_family_t fams[FAM_OPENLDAP_MAX];
} openldap_t;

static void openldap_free(void *arg)
{
    openldap_t *st = arg;

    if (st == NULL)
        return;

    free(st->binddn);
    free(st->password);
    free(st->cacert);
    free(st->name);
    free(st->url);
    if (st->ld)
        ldap_unbind_ext_s(st->ld, NULL, NULL);

    free(st);
}

static int openldap_init_host(openldap_t *st)
{
    int rc;

    if (unlikely(st->ld)) {
        PLUGIN_DEBUG("Already connected to %s", st->url);
        return 0;
    }

    rc = ldap_initialize(&st->ld, st->url);
    if (unlikely(rc != LDAP_SUCCESS)) {
        PLUGIN_ERROR("ldap_initialize failed: %s", ldap_err2string(rc));
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
            PLUGIN_ERROR("Failed to start tls on %s: %s", st->url, ldap_err2string(rc));
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
    if (unlikely(rc != LDAP_SUCCESS)) {
        PLUGIN_ERROR("Failed to bind to %s: %s", st->url, ldap_err2string(rc));
        ldap_unbind_ext_s(st->ld, NULL, NULL);
        st->ld = NULL;
        return -1;
    }

    PLUGIN_DEBUG("Successfully connected to %s", st->url);
    return 0;
}

static int openldap_read_host(user_data_t *ud)
{
    char *attrs[] = {
        "monitorCounter",    "monitorOpCompleted", "monitorOpInitiated",
        "monitoredInfo",     "olmBDBEntryCache",   "olmBDBDNCache",
        "olmBDBIDLCache",    "namingContexts",     "olmMDBPagesMax",
        "olmMDBPagesUsed",   "olmMDBPagesFree",    "olmMDBReadersMax",
        "olmMDBReadersUsed", "olmMDBEntries",      NULL
    };

    if (unlikely((ud == NULL) || (ud->data == NULL))) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }
    openldap_t *st = (openldap_t *)ud->data;

    int status = openldap_init_host(st);
    if (unlikely(status != 0)) {
        metric_family_append(&st->fams[FAM_OPENLDAP_UP],
                             VALUE_GAUGE(0), &st->labels, NULL);
        plugin_dispatch_metric_family(&st->fams[FAM_OPENLDAP_UP], 0);
        return -1;
    }

    LDAPMessage *result;
    int rc = ldap_search_ext_s(st->ld, "cn=Monitor", LDAP_SCOPE_SUBTREE,
                               "(|(!(cn=* *))(cn=Database*))", attrs, 0, NULL, NULL,
                               NULL, 0, &result);

    if (unlikely(rc != LDAP_SUCCESS)) {
        PLUGIN_ERROR("Failed to execute search: %s", ldap_err2string(rc));
        ldap_msgfree(result);
        ldap_unbind_ext_s(st->ld, NULL, NULL);
        st->ld = NULL;
        metric_family_append(&st->fams[FAM_OPENLDAP_UP],
                             VALUE_GAUGE(0), &st->labels, NULL);
        plugin_dispatch_metric_family(&st->fams[FAM_OPENLDAP_UP], 0);
        return -1;
    }

    metric_family_append(&st->fams[FAM_OPENLDAP_UP],
                         VALUE_GAUGE(1), &st->labels, NULL);

    for (LDAPMessage *e = ldap_first_entry(st->ld, result);
         e != NULL;
         e = ldap_next_entry(st->ld, e)) {
        char *dn =  ldap_get_dn(st->ld, e);
        if (dn == NULL)
            continue;

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
            metric_family_append(&st->fams[FAM_OPENLDAP_CONNECTIONS],
                                 VALUE_COUNTER(counter), &st->labels, NULL);
        } else if (strcmp(dn, "cn=Current,cn=Connections,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_CURRENT_CONNECTIONS],
                                  VALUE_GAUGE(counter), &st->labels, NULL);
        } else if (strcmp(dn, "cn=Operations,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_COMPLETED],
                                 VALUE_COUNTER(opc), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="all"}, NULL);
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_INITIATED],
                                 VALUE_COUNTER(opi), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="all"}, NULL);
        } else if (strcmp(dn, "cn=Bind,cn=Operations,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_COMPLETED],
                                 VALUE_COUNTER(opc), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="bind"}, NULL);
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_INITIATED],
                                 VALUE_COUNTER(opi), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="bind"}, NULL);
        } else if (strcmp(dn, "cn=UnBind,cn=Operations,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_COMPLETED],
                                 VALUE_COUNTER(opc), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="unbind"}, NULL);
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_INITIATED],
                                 VALUE_COUNTER(opi), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="unbind"}, NULL);
        } else if (strcmp(dn, "cn=Search,cn=Operations,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_COMPLETED],
                                 VALUE_COUNTER(opc), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="search"}, NULL);
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_INITIATED],
                                 VALUE_COUNTER(opi), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="search"}, NULL);
        } else if (strcmp(dn, "cn=Compare,cn=Operations,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_COMPLETED],
                                 VALUE_COUNTER(opc), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="compare"}, NULL);
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_INITIATED],
                                 VALUE_COUNTER(opi), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="compare"}, NULL);
        } else if (strcmp(dn, "cn=Modify,cn=Operations,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_COMPLETED],
                                 VALUE_COUNTER(opc), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="modify"}, NULL);
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_INITIATED],
                                 VALUE_COUNTER(opi), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="modify"}, NULL);
        } else if (strcmp(dn, "cn=Modrdn,cn=Operations,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_COMPLETED],
                                 VALUE_COUNTER(opc), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="modrdn"}, NULL);
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_INITIATED],
                                 VALUE_COUNTER(opi), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="modrdn"}, NULL);
        } else if (strcmp(dn, "cn=Add,cn=Operations,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_COMPLETED],
                                 VALUE_COUNTER(opc), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="add"}, NULL);
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_INITIATED],
                                 VALUE_COUNTER(opi), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="add"}, NULL);
        } else if (strcmp(dn, "cn=Delete,cn=Operations,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_COMPLETED],
                                 VALUE_COUNTER(opc), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="delete"}, NULL);
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_INITIATED],
                                 VALUE_COUNTER(opi), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="delete"}, NULL);
        } else if (strcmp(dn, "cn=Abandon,cn=Operations,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_COMPLETED],
                                 VALUE_COUNTER(opc), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="abandon"}, NULL);
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_INITIATED],
                                 VALUE_COUNTER(opi), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="abandon"}, NULL);
        } else if (strcmp(dn, "cn=Extended,cn=Operations,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_COMPLETED],
                                 VALUE_COUNTER(opc), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="extended"}, NULL);
            metric_family_append(&st->fams[FAM_OPENLDAP_OPERATIONS_INITIATED],
                                 VALUE_COUNTER(opi), &st->labels,
                                 &(label_pair_const_t){.name="operation", .value="extended"}, NULL);
        } else if ((strncmp(dn, "cn=Database", 11) == 0) &&
                   ((nc_list = ldap_get_values_len(st->ld, e, "namingContexts")) != NULL)) {
            nc_data = *nc_list[0];

            if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmBDBEntryCache")) != NULL) {
                olmbdb_data = *olmbdb_list[0];
                metric_family_append(&st->fams[FAM_OPENLDAP_BDB_ENTRY_CACHE_SIZE],
                            VALUE_GAUGE(atoll(olmbdb_data.bv_val)), &st->labels,
                            &(label_pair_const_t){.name="database", .value=nc_data.bv_val}, NULL);
                ldap_value_free_len(olmbdb_list);
            }

            if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmBDBDNCache")) != NULL) {
                olmbdb_data = *olmbdb_list[0];
                metric_family_append(&st->fams[FAM_OPENLDAP_BDB_DN_CACHE_SIZE],
                            VALUE_GAUGE(atoll(olmbdb_data.bv_val)), &st->labels,
                            &(label_pair_const_t){.name="database", .value=nc_data.bv_val}, NULL);
                ldap_value_free_len(olmbdb_list);
            }

            if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmBDBIDLCache")) != NULL) {
                olmbdb_data = *olmbdb_list[0];
                metric_family_append(&st->fams[FAM_OPENLDAP_BDB_IDL_CACHE_SIZE],
                            VALUE_GAUGE(atoll(olmbdb_data.bv_val)), &st->labels,
                            &(label_pair_const_t){.name="database", .value=nc_data.bv_val}, NULL);
                ldap_value_free_len(olmbdb_list);
            }

            if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmMDBEntries")) != NULL) {
                olmbdb_data = *olmbdb_list[0];
                metric_family_append(&st->fams[FAM_OPENLDAP_MDB_ENTRIES],
                            VALUE_GAUGE(atoll(olmbdb_data.bv_val)), &st->labels,
                            &(label_pair_const_t){.name="database", .value=nc_data.bv_val}, NULL);
                ldap_value_free_len(olmbdb_list);
            }

            if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmMDBPagesMax")) != NULL) {
                olmbdb_data = *olmbdb_list[0];
                metric_family_append(&st->fams[FAM_OPENLDAP_MDB_PAGES_MAX],
                            VALUE_GAUGE(atoll(olmbdb_data.bv_val)), &st->labels,
                            &(label_pair_const_t){.name="database", .value=nc_data.bv_val}, NULL);
                ldap_value_free_len(olmbdb_list);
            }

            if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmMDBPagesUsed")) != NULL) {
                olmbdb_data = *olmbdb_list[0];
                metric_family_append(&st->fams[FAM_OPENLDAP_MDB_PAGES_USED],
                            VALUE_GAUGE(atoll(olmbdb_data.bv_val)), &st->labels,
                            &(label_pair_const_t){.name="database", .value=nc_data.bv_val}, NULL);
                ldap_value_free_len(olmbdb_list);
            }

            if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmMDBPagesFree")) != NULL) {
                olmbdb_data = *olmbdb_list[0];
                metric_family_append(&st->fams[FAM_OPENLDAP_MDB_PAGES_FREE],
                            VALUE_GAUGE(atoll(olmbdb_data.bv_val)), &st->labels,
                            &(label_pair_const_t){.name="database", .value=nc_data.bv_val}, NULL);
                ldap_value_free_len(olmbdb_list);
            }

            if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmMDBReadersMax")) != NULL) {
                olmbdb_data = *olmbdb_list[0];
                metric_family_append(&st->fams[FAM_OPENLDAP_MDB_READERS_MAX],
                            VALUE_GAUGE(atoll(olmbdb_data.bv_val)), &st->labels,
                            &(label_pair_const_t){.name="database", .value=nc_data.bv_val}, NULL);
                ldap_value_free_len(olmbdb_list);
            }

            if ((olmbdb_list = ldap_get_values_len(st->ld, e, "olmMDBReadersUsed")) != NULL) {
                olmbdb_data = *olmbdb_list[0];
                metric_family_append(&st->fams[FAM_OPENLDAP_MDB_READERS_USED],
                            VALUE_GAUGE(atoll(olmbdb_data.bv_val)), &st->labels,
                            &(label_pair_const_t){.name="database", .value=nc_data.bv_val}, NULL);
                ldap_value_free_len(olmbdb_list);
            }

            ldap_value_free_len(nc_list);
        } else if (strcmp(dn, "cn=Bytes,cn=Statistics,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_SEND_BYTES],
                                 VALUE_COUNTER(counter), &st->labels, NULL);
        } else if (strcmp(dn, "cn=PDU,cn=Statistics,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_SEND_PDUS],
                                 VALUE_COUNTER(counter), &st->labels, NULL);
        } else if (strcmp(dn, "cn=Entries,cn=Statistics,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_SEND_ENTRIES],
                                 VALUE_COUNTER(counter), &st->labels, NULL);
        } else if (strcmp(dn, "cn=Referrals,cn=Statistics,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_SEND_REFERRALS],
                                 VALUE_COUNTER(counter), &st->labels, NULL);
        } else if (strcmp(dn, "cn=Open,cn=Threads,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_THREADS],
                                 VALUE_GAUGE(info), &st->labels,
                                 &(label_pair_const_t){.name="status", .value="open"}, NULL);
        } else if (strcmp(dn, "cn=Starting,cn=Threads,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_THREADS],
                                 VALUE_GAUGE(info), &st->labels,
                                 &(label_pair_const_t){.name="status", .value="stating"}, NULL);
        } else if (strcmp(dn, "cn=Active,cn=Threads,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_THREADS],
                                 VALUE_GAUGE(info), &st->labels,
                                 &(label_pair_const_t){.name="status", .value="active"}, NULL);
        } else if (strcmp(dn, "cn=Pending,cn=Threads,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_THREADS],
                                 VALUE_GAUGE(info), &st->labels,
                                 &(label_pair_const_t){.name="status", .value="pending"}, NULL);
        } else if (strcmp(dn, "cn=Backload,cn=Threads,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_THREADS],
                                 VALUE_GAUGE(info), &st->labels,
                                 &(label_pair_const_t){.name="status", .value="backload"}, NULL);
        } else if (strcmp(dn, "cn=Read,cn=Waiters,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_WAITERS_READ],
                                 VALUE_GAUGE(counter), &st->labels, NULL);
        } else if (strcmp(dn, "cn=Write,cn=Waiters,cn=Monitor") == 0) {
            metric_family_append(&st->fams[FAM_OPENLDAP_WAITERS_WRITE],
                                 VALUE_GAUGE(counter), &st->labels, NULL);
        }

        ldap_value_free_len(counter_list);
        ldap_value_free_len(opc_list);
        ldap_value_free_len(opi_list);
        ldap_value_free_len(info_list);
        ldap_memfree(dn);
    }
    ldap_msgfree(result);

    plugin_dispatch_metric_family_array(st->fams, FAM_OPENLDAP_MAX, 0);
    return 0;
}

static int openldap_config_add(config_item_t *ci)
{
    openldap_t *st = calloc(1, sizeof(*st));
    if (st == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &st->name);
    if (status != 0) {
        free(st);
        return status;
    }

    st->starttls = false;
    st->verifyhost = true;
    st->version = LDAP_VERSION3;

    memcpy(st->fams, fams, sizeof(st->fams[0])*FAM_OPENLDAP_MAX);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("bind-dn", child->key) == 0) {
            status = cf_util_get_string(child, &st->binddn);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &st->password);
        } else if (strcasecmp("ca-cert", child->key) == 0) {
            status = cf_util_get_string(child, &st->cacert);
        } else if (strcasecmp("start-tls", child->key) == 0) {
            status = cf_util_get_boolean(child, &st->starttls);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_int(child, &st->timeout);
        } else if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &st->url);
        } else if (strcasecmp("verify-host", child->key) == 0) {
            status = cf_util_get_boolean(child, &st->verifyhost);
        } else if (strcasecmp("version", child->key) == 0) {
            status = cf_util_get_int(child, &st->version);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &st->labels);
        } else {
            PLUGIN_WARNING("Option '%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    /* Check if struct is complete.. */
    if ((status == 0) && (st->url == NULL)) {
        PLUGIN_ERROR("Instance '%s': No 'url' has been configured.", st->name);
        status = -1;
    }

    /* Check if URL is valid */
    if ((status == 0) && (st->url != NULL)) {
        LDAPURLDesc *ludpp;

        if (ldap_url_parse(st->url, &ludpp) != 0) {
            PLUGIN_ERROR("Instance '%s': Invalid 'url': `%s'", st->name, st->url);
            status = -1;
        }

        ldap_free_urldesc(ludpp);
    }

    if (status == 0)
        status = label_set_add(&st->labels, false, "instance", st->name);

    if (status != 0) {
        openldap_free(st);
        return -1;
    }

    st->timeout = (long)CDTIME_T_TO_TIME_T(interval);

    return plugin_register_complex_read("openldap", st->name, openldap_read_host, interval,
                                        &(user_data_t){ .data = st, .free_func = openldap_free });
}

static int openldap_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = openldap_config_add(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' is not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
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
    plugin_register_config("openldap", openldap_config);
    plugin_register_init("openldap", openldap_init);
}
