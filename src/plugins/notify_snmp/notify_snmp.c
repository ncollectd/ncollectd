// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2014-2022 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include <pthread.h>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

static oid objid_snmptrap[] = {1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0};
static oid objid_sysuptime[] = {1, 3, 6, 1, 2, 1, 1, 3, 0};

typedef enum {
    NOTIFY_SNMP_NULL_OID,
    NOTIFY_SNMP_ENTERPRISE_OID,
    NOTIFY_SNMP_TRAP_OID,
    NOTIFY_SNMP_NAME_OID,
    NOTIFY_SNMP_SEVERITY_OID,
    NOTIFY_SNMP_TIMESTAMP_OID,
    NOTIFY_SNMP_LABELS_OID,
    NOTIFY_SNMP_ANNOTATIONS_OID,
} notify_snmp_enum_oids_t;

static struct {
    notify_snmp_enum_oids_t id;
    char *name;
} notify_snmp_oids_map[] = {
    { NOTIFY_SNMP_ENTERPRISE_OID,  "enterprise-oid"  },
    { NOTIFY_SNMP_TRAP_OID,        "trap-oid"        },
    { NOTIFY_SNMP_NAME_OID,        "name-oid"        },
    { NOTIFY_SNMP_SEVERITY_OID,    "severity-oid"    },
    { NOTIFY_SNMP_TIMESTAMP_OID,   "timestamp-oid"   },
    { NOTIFY_SNMP_LABELS_OID,      "labels-oid"      },
    { NOTIFY_SNMP_ANNOTATIONS_OID, "annotations-oid" },
    { NOTIFY_SNMP_NULL_OID,        NULL              },
};

struct notify_snmp_oid_s {
    notify_snmp_enum_oids_t id;
    char *string;
    oid objid[MAX_OID_LEN];
    size_t len;
};
typedef struct notify_snmp_oid_s notify_snmp_oid_t;

struct notify_snmp_oids_s {
    char *name;
    notify_snmp_oid_t *list;
    int len;

    struct notify_snmp_oids_s *next;
};
typedef struct notify_snmp_oids_s notify_snmp_oids_t;

struct notify_snmp_target_s {
    char *name;
    char *address;
    char *community;
    int version;
    notify_snmp_oids_t *oids;
    void *sess_handle;
    bool sess_reuse;
    pthread_mutex_t session_lock;

    struct notify_snmp_target_s *next;
};
typedef struct notify_snmp_target_s notify_snmp_target_t;

static notify_snmp_target_t *notify_snmp_targets;
static notify_snmp_oids_t *notify_snmp_oids;

static struct {
    notify_snmp_enum_oids_t id;
    char *string;
} notify_snmp_default_oids[] = {
    { NOTIFY_SNMP_ENTERPRISE_OID,  "SNMPv2-SMI::experimental.101"     },
    { NOTIFY_SNMP_TRAP_OID,        "SNMPv2-SMI::experimental.101.1"   },
    { NOTIFY_SNMP_NAME_OID,        "SNMPv2-SMI::experimental.101.2.1" },
    { NOTIFY_SNMP_SEVERITY_OID,    "SNMPv2-SMI::experimental.101.2.2" },
    { NOTIFY_SNMP_TIMESTAMP_OID,   "SNMPv2-SMI::experimental.101.2.3" },
    { NOTIFY_SNMP_LABELS_OID,      "SNMPv2-SMI::experimental.101.2.4" },
    { NOTIFY_SNMP_ANNOTATIONS_OID, "SNMPv2-SMI::experimental.101.2.5" },
    { NOTIFY_SNMP_NULL_OID,        NULL}
};

static char *notify_snmp_oids_map_id2name(notify_snmp_enum_oids_t id)
{
    for (int i = 0; notify_snmp_oids_map[i].name != NULL; i++) {
        if (notify_snmp_oids_map[i].id == id)
            return notify_snmp_oids_map[i].name;
    }

    return NULL;
}

static notify_snmp_oids_t *notify_snmp_get_oids(char *name)
{
    notify_snmp_oids_t *oids = notify_snmp_oids;
    while (oids != NULL) {
        if ((name == NULL) && (oids->name == NULL)) {
            return oids;
        } else if ((name != NULL) && (oids->name != NULL)) {
            if (strcasecmp(oids->name, name) == 0)
                return oids;
        }
        oids = oids->next;
    }

    return NULL;
}

static notify_snmp_oid_t *notify_snmp_oids_get_oid(notify_snmp_oids_t *oids,
                                                   notify_snmp_enum_oids_t id)
{
    for (int i = 0; i < oids->len; i++) {
        if (oids->list[i].id == id)
            return &oids->list[i];
    }

    return NULL;
}

static notify_snmp_target_t *notify_snmp_malloc_target(void)
{
    notify_snmp_target_t *target = calloc(1, sizeof(notify_snmp_target_t));
    if (target == NULL) {
        PLUGIN_ERROR("notify_snmp_malloc_target: calloc failed.");
        return NULL;
    }

    pthread_mutex_init(&target->session_lock, /* attr = */ NULL);
    return target;
}

static void notify_snmp_free_target(notify_snmp_target_t *target)
{
    free(target->name);
    free(target->address);
    free(target->community);

    if (target->sess_handle != NULL)
        snmp_sess_close(target->sess_handle);

    free(target);
}

static notify_snmp_oids_t *notify_snmp_malloc_oids(void)
{
    notify_snmp_oids_t *oids = calloc(1, sizeof(notify_snmp_oids_t));
    if (oids == NULL) {
        PLUGIN_ERROR("notify_snmp_malloc_oids: calloc failed.");
        return NULL;
    }
    return oids;
}

static void notify_snmp_free_oids(notify_snmp_oids_t *oids)
{
    if (oids->list != NULL) {
        for (int i = 0; i < oids->len; i++) {
            free(oids->list[i].string);
        }
        free(oids->list);
    }

    free(oids->name);
    free(oids);
}

static int notify_snmp_config_set_target_oids(notify_snmp_oids_t **oids, config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The `%s' config option needs exactly one string argument.", ci->key);
        return -1;
    }

    char *string = ci->values[0].value.string;
    notify_snmp_oids_t *ret_oids = notify_snmp_get_oids(string);
    if (ret_oids == NULL) {
        PLUGIN_WARNING("OIDs '%s' not found.", string);
        return -1;
    }

    *oids = ret_oids;
    return 0;
}

static int notify_snmp_oids_append_oid(notify_snmp_oids_t **oids, notify_snmp_enum_oids_t id,
                                       char *string_oid)
{
    notify_snmp_oid_t *oids_list = realloc((*oids)->list,
                                           sizeof(notify_snmp_oid_t) * ((*oids)->len + 1));
    if (oids_list == NULL) {
        PLUGIN_ERROR("realloc failed.");
        return -1;
    }

    (*oids)->list = oids_list;
    int n = (*oids)->len;

    oids_list[n].string = strdup(string_oid);
    if (oids_list[n].string == NULL) {
        PLUGIN_ERROR("strdup failed.");
        return -1;
    }

    oids_list[n].len = MAX_OID_LEN;
    if (snmp_parse_oid(oids_list[n].string, oids_list[n].objid, &oids_list[n].len) == NULL) {
        PLUGIN_ERROR("OIDs %s: snmp_parse_oid %s (%s) failed.",
                    ((*oids)->name == NULL) ? "default" : (*oids)->name,
                    notify_snmp_oids_map_id2name(oids_list[n].id), oids_list[n].string);
        return -1;
    }

    oids_list[n].id = id;
    (*oids)->len++;
    return 0;
}

static int notify_snmp_config_oids_append_oid(notify_snmp_oids_t **oids, notify_snmp_enum_oids_t id,
                                              config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The `%s' config option needs exactly one string argument.",
                        ci->key);
        return -1;
    }

    return notify_snmp_oids_append_oid(oids, id, ci->values[0].value.string);
}

static notify_snmp_oids_t *notify_snmp_get_default_oids(void)
{
    notify_snmp_oids_t *oids = notify_snmp_get_oids(NULL);
    if (oids != NULL)
        return oids;

    oids = notify_snmp_malloc_oids();
    if (oids == NULL)
        return NULL;

    oids->name = NULL;

    for (int i = 0; notify_snmp_default_oids[i].string != NULL; i++) {
        int status = notify_snmp_oids_append_oid(&oids, notify_snmp_default_oids[i].id,
                                                  notify_snmp_default_oids[i].string);
        if (status != 0) {
            notify_snmp_free_oids(oids);
            return NULL;
        }
    }

    if (notify_snmp_oids != NULL)
        oids->next = notify_snmp_oids->next;

    notify_snmp_oids = oids;

    return oids;
}

static int notify_snmp_config_add_oids(config_item_t *ci)
{
    int status;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("The `Target' block needs exactly one string argument.");
        return -1;
    }

    notify_snmp_oids_t *oids = notify_snmp_malloc_oids();
    if (oids == NULL)
        return -1;

    status = cf_util_get_string(ci, &oids->name);
    if (status != 0) {
        notify_snmp_free_oids(oids);
        return -1;
    }

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        int found = 0;

        for (int n = 0; notify_snmp_oids_map[n].name != NULL; n++) {
            if (strcasecmp(notify_snmp_oids_map[n].name, child->key) == 0) {
                status = notify_snmp_config_oids_append_oid(&oids, notify_snmp_oids_map[n].id,
                                                            child);
                found = 1;
                break;
            }
        }

        if (found == 0) {
            PLUGIN_WARNING("Option `%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0) {
            notify_snmp_free_oids(oids);
            return -1;
        }
    }

    if (notify_snmp_oids != NULL)
        oids->next = notify_snmp_oids->next;

    notify_snmp_oids = oids;

    return 0;
}

static int notify_snmp_config_add_target(config_item_t *ci)
{
    notify_snmp_target_t *target = notify_snmp_malloc_target();
    if (target == NULL)
        return -1;

    int status = cf_util_get_string(ci, &target->name);
    if (status != 0) {
        PLUGIN_WARNING("The 'target' block needs exactly one string argument.");
        notify_snmp_free_target(target);
        return -1;
    }

    target->version = 1;
    target->sess_reuse = false;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("address", child->key) == 0) {
            status = cf_util_get_string(child, &target->address);
        } else if (strcasecmp("version", child->key) == 0) {
            status = cf_util_get_int(child, &target->version);
            if ((status == 0) && (target->version < 0 || target->version > 2))
                target->version = 1;
        } else if (strcasecmp("community", child->key) == 0) {
            status = cf_util_get_string(child, &target->community);
        } else if (strcasecmp("oids", child->key) == 0) {
            status = notify_snmp_config_set_target_oids(&target->oids, child);
        } else if (strcasecmp("session-reuse", child->key) == 0) {
            status = cf_util_get_boolean(child, &target->sess_reuse);
        } else {
            PLUGIN_WARNING("Option `%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        notify_snmp_free_target(target);
        return -1;
    }

    if (target->oids == NULL) {
        target->oids = notify_snmp_get_default_oids();
        if (target->oids == NULL) {
            PLUGIN_WARNING("cannot get default OIDs.");
            notify_snmp_free_target(target);
            return -1;
        }
    }

    if ((target->version == 1) &&
        (notify_snmp_oids_get_oid(target->oids, NOTIFY_SNMP_ENTERPRISE_OID) == NULL)) {
        PLUGIN_WARNING("With SNMP Version 1 need an Enterprise OID.");
        notify_snmp_free_target(target);
        return -1;
    }

    if ((target->version == 2) &&
        (notify_snmp_oids_get_oid(target->oids, NOTIFY_SNMP_TRAP_OID) == NULL)) {
        PLUGIN_WARNING("With SNMP Version 2 need a Trap OID.");
        notify_snmp_free_target(target);
        return -1;
    }

    if (target->address == NULL)
        target->address = strdup("localhost");

    if (target->community == NULL)
        target->community = strdup("public");

    if (notify_snmp_targets != NULL) {
        notify_snmp_target_t *targets = notify_snmp_targets;
        while (targets->next != NULL) {
            targets = target->next;
        }
        targets->next = target;
    } else {
        notify_snmp_targets = target;
    }

    return 0;
}

/*
plugin notify_snmp {
    oids ncollectd {
        EnterpriseOID  "SNMPv2-SMI::experimental.101"
        TrapOID        "SNMPv2-SMI::experimental.101.1"
        NameOID        "SNMPv2-SMI::experimental.101.2.1"
        TimeStampOID   "SNMPv2-SMI::experimental.101.2.2"
        SeverityOID    "SNMPv2-SMI::experimental.101.2.3"
        LabelsOID      "SNMPv2-SMI::experimental.101.2.4"
        AnnotationsOID "SNMPv2-SMI::experimental.101.2.5"
    }
    target localhost {
        Address "localhost:162"
        Version 2
        Community "public"
        SessionReuse true
        OIDs ncollectd
    }
}
*/

static int notify_snmp_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp("target", child->key) == 0) {
            status = notify_snmp_config_add_target(child);
        } else if (strcasecmp("oids", child->key) == 0) {
            status = notify_snmp_config_add_oids(child);
        } else {
            PLUGIN_ERROR("Unknown config option '%s'.", child->key);
            status = -1;
        }

        if (status < 0)
            return -1;
    }

    return 0;
}

static void notify_snmp_exit_session(notify_snmp_target_t *target)
{

    if (target->sess_handle != NULL && !target->sess_reuse) {
        int status = snmp_sess_close(target->sess_handle);
        if (status == 0) {
            char *errstr = NULL;
            snmp_sess_error(target->sess_handle, NULL, NULL, &errstr);
            PLUGIN_WARNING("snmp_sess_close error: '%s'.", errstr);
        }
        target->sess_handle = NULL;
    }
}

static int notify_snmp_init_session(notify_snmp_target_t *target)
{
    netsnmp_session session;

    if (target->sess_handle != NULL)
        notify_snmp_exit_session(target);

    snmp_sess_init(&session);
    session.version = target->version == 1 ? SNMP_VERSION_1 : SNMP_VERSION_2c;
    session.callback = NULL;
    session.callback_magic = NULL;
    session.peername = target->address;

    session.community = (u_char *)target->community;
    session.community_len = strlen(target->community);

    target->sess_handle = snmp_sess_open(&session);
    if (target->sess_handle == NULL) {
        char *errstr = NULL;
        snmp_error(&session, NULL, NULL, &errstr);
        PLUGIN_ERROR("target %s: snmp_sess_open failed: %s",
                    target->name, (errstr == NULL) ? "Unknown problem" : errstr);
        return -1;
    }

    return 0;
}

static netsnmp_pdu *notify_snmp_create_pdu(notify_snmp_target_t *target)
{
    netsnmp_pdu *pdu = NULL;
    int status;

    netsnmp_session *session = snmp_sess_session(target->sess_handle);
    if (session->version == SNMP_VERSION_1) {
        notify_snmp_oid_t *oid_enterprise =
                notify_snmp_oids_get_oid(target->oids, NOTIFY_SNMP_ENTERPRISE_OID);

        if (oid_enterprise == NULL) {
            PLUGIN_ERROR("cannot find Enterprise OID for Target %s.", target->name);
            return NULL;
        }

        pdu = snmp_pdu_create(SNMP_MSG_TRAP);
        if (pdu == NULL) {
            PLUGIN_ERROR("Target %s: snmp_pdu_create failed.", target->name);
            return NULL;
        }

        pdu->enterprise = malloc(oid_enterprise->len * sizeof(oid));
        if (pdu->enterprise == NULL) {
            PLUGIN_ERROR("malloc failed.");
            snmp_free_pdu(pdu);
            return NULL;
        }

        memcpy(pdu->enterprise, oid_enterprise->objid, oid_enterprise->len * sizeof(oid));
        pdu->enterprise_length = oid_enterprise->len;

        pdu->trap_type = SNMP_TRAP_ENTERPRISESPECIFIC;
        pdu->specific_type = 0;
        pdu->time = get_uptime();
    } else if (session->version == SNMP_VERSION_2c) {
        char sysuptime[20];

        notify_snmp_oid_t *oid_trap = notify_snmp_oids_get_oid(target->oids, NOTIFY_SNMP_TRAP_OID);

        if (oid_trap == NULL) {
            PLUGIN_ERROR("cannot find Trap OID for Target %s.", target->name);
            return NULL;
        }

        pdu = snmp_pdu_create(SNMP_MSG_TRAP2);
        if (pdu == NULL) {
            PLUGIN_ERROR("Target %s: snmp_pdu_create failed.",
                        target->name);
            return NULL;
        }

        status = ssnprintf(sysuptime, sizeof(sysuptime), "%ld", get_uptime());
        if ((status < 0) || ((size_t)status >= sizeof(sysuptime)))
            PLUGIN_WARNING("notify_snmp_create_pdu snprintf truncated sysuptime");

        status = snmp_add_var(pdu, objid_sysuptime,
                                   sizeof(objid_sysuptime) / sizeof(oid), 't', sysuptime);
        if (status) {
            PLUGIN_ERROR("Target %s: snmp_add_var oid sysuptime failed", target->name);
            snmp_free_pdu(pdu);
            return NULL;
        }

        status = snmp_add_var(pdu, objid_snmptrap, sizeof(objid_snmptrap) / sizeof(oid),
                                   'o', oid_trap->string);
        if (status) {
            PLUGIN_ERROR("Target %s: snmp_add_var oid trap (%s) failed.",
                        target->name, oid_trap->string);
            snmp_free_pdu(pdu);
            return NULL;
        }
    }

    return pdu;
}

static int notify_snmp_sendsnmp(notify_snmp_target_t *target, const notification_t *n)
{
    int status;

    if (target->sess_handle == NULL) {
        if (notify_snmp_init_session(target) < 0)
            return -1;
    }

    netsnmp_pdu *pdu = notify_snmp_create_pdu(target);
    if (pdu == NULL)
        return -1;

    notify_snmp_oid_t *oids_list = target->oids->list;
    int oids_len = target->oids->len;

    for (int i = 0; i < oids_len; i++) {
        char *value = NULL;
        char buffer[32];
        int add_value = 1;

        switch (oids_list[i].id) {
        case NOTIFY_SNMP_NAME_OID:
            value = n->name;
            break;
        case NOTIFY_SNMP_SEVERITY_OID:
            switch(n->severity) {
            case NOTIF_FAILURE:
                value = "FAILURE";
                break;
            case NOTIF_WARNING:
                value = "WARNING";
                break;
            case NOTIF_OKAY:
                value = "OKAY";
                break;
            default:
                value = "UNKNOWN";
                break;
            }
            break;
        case NOTIFY_SNMP_TIMESTAMP_OID:
            status = ssnprintf(buffer, sizeof(buffer), "%zu", (size_t)CDTIME_T_TO_TIME_T(n->time));
            if ((status < 0) || ((size_t)status >= sizeof(buffer)))
                PLUGIN_WARNING("truncate notification time.");
            value = buffer;
            break;
        case NOTIFY_SNMP_LABELS_OID:
            value = NULL;
            break;
        case NOTIFY_SNMP_ANNOTATIONS_OID:
            value = NULL;
            break;
        default:
            add_value = 0;
            break;
        }

        if (add_value) {
            status = snmp_add_var(pdu, oids_list[i].objid, oids_list[i].len, 's',
                                                        value == NULL ? "" : value);

            if (status) {
                char *errstr = NULL;

                snmp_sess_error(target->sess_handle, NULL, NULL, &errstr);
                PLUGIN_ERROR("target %s: snmp_add_var for %s (%s) failed: %s",
                             target->name, notify_snmp_oids_map_id2name(oids_list[i].id),
                             oids_list[i].string,
                             (errstr == NULL) ? "Unknown problem" : errstr);
                snmp_free_pdu(pdu);
                notify_snmp_exit_session(target);
                return -1;
            }
        }
    }

    status = snmp_sess_send(target->sess_handle, pdu);
    if (status == 0) {
        char *errstr = NULL;

        snmp_sess_error(target->sess_handle, NULL, NULL, &errstr);
        PLUGIN_ERROR("target %s: snmp_sess_send failed: %s.",
                      target->name, (errstr == NULL) ? "Unknown problem" : errstr);
        snmp_free_pdu(pdu);
        notify_snmp_exit_session(target);
        return -1;
    }

    notify_snmp_exit_session(target);

    return 0;
}

static int notify_snmp_notification(const notification_t *n,
                                    user_data_t __attribute__((unused)) * user_data)
{
    notify_snmp_target_t *target = notify_snmp_targets;
    int ok = 0;
    int fail = 0;

    while (target != NULL) {
        pthread_mutex_lock(&target->session_lock);
        int status = notify_snmp_sendsnmp(target, n);
        pthread_mutex_unlock(&target->session_lock);

        if (status < 0) {
            fail++;
        } else {
            ok++;
        }
        target = target->next;
    }

    if (ok == 0 && fail > 0) {
        return -1;
    }

    return 0;
}

static int notify_snmp_init(void)
{
    static int have_init = 0;

    if (have_init == 0) {
        init_snmp(PACKAGE_NAME);
    }

    have_init = 1;
    return 0;
}

static int notify_snmp_shutdown(void)
{
    notify_snmp_target_t *target = notify_snmp_targets;
    notify_snmp_oids_t *oid = notify_snmp_oids;

    notify_snmp_targets = NULL;
    while (target != NULL) {
        notify_snmp_target_t *next;

        next = target->next;
        pthread_mutex_lock(&target->session_lock);
        pthread_mutex_unlock(&target->session_lock);
        pthread_mutex_destroy(&target->session_lock);
        notify_snmp_free_target(target);
        target = next;
    }

    notify_snmp_oids = NULL;
    while (oid != NULL) {
        notify_snmp_oids_t *next;

        next = oid->next;
        notify_snmp_free_oids(oid);
        oid = next;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_init("notify_snmp", notify_snmp_init);
    plugin_register_shutdown("notify_snmp", notify_snmp_shutdown);
    plugin_register_config("notify_snmp", notify_snmp_config);
    plugin_register_notification(NULL, "notify_snmp", notify_snmp_notification, NULL);
}
