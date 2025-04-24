// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2007-2012 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"
#include "libutils/complain.h"
#include "libutils/itoa.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <fnmatch.h>

/* SHA512 plus a trailing nul */
#define MAX_DIGEST_NAME_LEN 7

#define BUFFER_DATA_SIZE 512

typedef struct {
    oid oid[MAX_OID_LEN];
    size_t len;
} oid_t;

typedef struct {
    char *label;
    oid_t oid;
} label_oid_t;

typedef struct {
    size_t num;
    label_oid_t *ptr;
} label_oid_set_t;

struct data_definition_s {
    char *name; /* used to reference this from the 'collect' option */
    bool is_table;

    char *metric;
    metric_type_t type;
    char *help;

    label_set_t labels;
    label_oid_set_t labels_from;

    oid_t filter_oid;
    exclist_t exclist;

    oid_t value_oid;

    double scale;
    double shift;

    bool count;

    struct data_definition_s *next;
};
typedef struct data_definition_s data_definition_t;

typedef struct {
    char *name;
    char *address;
    int version;
    cdtime_t timeout;
    int retries;

    char *metric_prefix;
    label_set_t labels;

    /* snmpv1/2 options */
    char *community;

    /* snmpv3 security options */
    char *username;
    oid *auth_protocol;
    size_t auth_protocol_len;
    char *auth_passphrase;
    oid *priv_protocol;
    size_t priv_protocol_len;
    char *priv_passphrase;
    int security_level;
    char *local_cert;
    char *peer_cert;
    char *peer_hostname;
    char *trust_cert;
    char *context;

    void *sess_handle;
    c_complain_t complaint;
    data_definition_t **data_list;
    int data_list_len;
    int bulk_size;
} host_definition_t;

/* These two types are used to cache values in 'csnmp_read_table' to handle gaps in tables. */
struct csnmp_cell_char_s {
    oid_t suffix;
    char value[BUFFER_DATA_SIZE];
    struct csnmp_cell_char_s *next;
};
typedef struct csnmp_cell_char_s csnmp_cell_char_t;

struct csnmp_cell_value_s {
    oid_t suffix;
    value_t value;
    struct csnmp_cell_value_s *next;
};
typedef struct csnmp_cell_value_s csnmp_cell_value_t;

typedef enum {
    OID_TYPE_SKIP = 0,
    OID_TYPE_VARIABLE,
    OID_TYPE_LABEL,
    OID_TYPE_FILTER,
} csnmp_oid_type_t;

static data_definition_t *data_head;

static int csnmp_read_host(user_data_t *ud);

static void csnmp_oid_init(oid_t *dst, oid const *src, size_t n)
{
    assert(n <= STATIC_ARRAY_SIZE(dst->oid));
    memcpy(dst->oid, src, sizeof(*src) * n);
    dst->len = n;
}

static int csnmp_oid_compare(oid_t const *left, oid_t const *right)
{
    return snmp_oid_compare(left->oid, left->len, right->oid, right->len);
}

static int csnmp_oid_suffix(oid_t *dst, oid_t const *src, oid_t const *root)
{
    /* Make sure "src" is in "root"s subtree. */
    if (src->len <= root->len)
        return EINVAL;
    if (snmp_oid_ncompare(root->oid, root->len, src->oid, src->len, root->len) != 0)
        return EINVAL;

    memset(dst, 0, sizeof(*dst));
    dst->len = src->len - root->len;
    memcpy(dst->oid, &src->oid[root->len], dst->len * sizeof(dst->oid[0]));
    return 0;
}

static void csnmp_host_close_session(host_definition_t *host)
{
    if (host->sess_handle == NULL)
        return;

    snmp_sess_close(host->sess_handle);
    host->sess_handle = NULL;
}

static void csnmp_host_definition_destroy(void *arg)
{
    host_definition_t *hd =  arg;
    if (hd == NULL)
        return;

    csnmp_host_close_session(hd);

    free(hd->name);
    free(hd->address);
    free(hd->community);
    free(hd->username);
    free(hd->auth_passphrase);
    free(hd->priv_passphrase);
    free(hd->local_cert);
    free(hd->peer_cert);
    free(hd->peer_hostname);
    free(hd->trust_cert);
    free(hd->context);
    free(hd->data_list);

    free(hd->metric_prefix);
    label_set_reset(&hd->labels);

    free(hd);
}

static void csnmp_data_definition_destroy(data_definition_t *dd)
{
    free(dd->name);
    free(dd->metric);
    free(dd->help);
    label_set_reset(&dd->labels);

    for (size_t i = 0; i < dd->labels_from.num; i++) {
        free(dd->labels_from.ptr[i].label);
    }
    free(dd->labels_from.ptr);

    exclist_reset(&dd->exclist);

    free(dd);
}

static void csnmp_host_open_session(host_definition_t *host)
{
    struct snmp_session sess;
    int error;

    if (host->sess_handle != NULL)
        csnmp_host_close_session(host);

    snmp_sess_init(&sess);
    sess.peername = host->address;
    switch (host->version) {
    case 1:
        sess.version = SNMP_VERSION_1;
        break;
    case 3:
        sess.version = SNMP_VERSION_3;
        break;
    default:
        sess.version = SNMP_VERSION_2c;
        break;
    }

    if (host->version == 3) {
        /* use TLS/DTLS... */
        if (host->local_cert != NULL) {
            if (sess.transport_configuration == NULL) {
                netsnmp_container_init_list();
                sess.transport_configuration = netsnmp_container_find("transport_configuration:fifo");
                if (sess.transport_configuration == NULL) {
                    PLUGIN_ERROR("Host %s: Failed to initialize the transport "
                                 "configuration container.", host->name);
                    return;
                }
                sess.transport_configuration->compare =
                    (netsnmp_container_compare *)netsnmp_transport_config_compare;
            }

            CONTAINER_INSERT(sess.transport_configuration,
                             netsnmp_transport_create_config("localCert", host->local_cert));

            if (host->peer_cert != NULL) {
                CONTAINER_INSERT(sess.transport_configuration,
                    netsnmp_transport_create_config("peerCert", host->peer_cert));
            }
            if (host->peer_hostname != NULL) {
                CONTAINER_INSERT(sess.transport_configuration,
                    netsnmp_transport_create_config("peerHostname", host->peer_hostname));
            }
            if (host->trust_cert != NULL) {
                CONTAINER_INSERT(sess.transport_configuration,
                    netsnmp_transport_create_config("trustCert", host->trust_cert));
            }
        } else { /* ...otherwise use shared secrets */
            sess.securityName = host->username;
            sess.securityNameLen = strlen(host->username);
            sess.securityLevel = host->security_level;

            if ((sess.securityLevel == SNMP_SEC_LEVEL_AUTHNOPRIV) ||
                (sess.securityLevel == SNMP_SEC_LEVEL_AUTHPRIV)) {
                sess.securityAuthProto = host->auth_protocol;
                sess.securityAuthProtoLen = host->auth_protocol_len;
                sess.securityAuthKeyLen = USM_AUTH_KU_LEN;
                error = generate_Ku(sess.securityAuthProto, sess.securityAuthProtoLen,
                                    (u_char *)host->auth_passphrase,
                                    strlen(host->auth_passphrase), sess.securityAuthKey,
                                    &sess.securityAuthKeyLen);
                if (error != SNMPERR_SUCCESS) {
                    PLUGIN_ERROR("host %s: Error generating Ku from auth_passphrase. (Error %d)",
                                 host->name, error);
                }
            }

            if (sess.securityLevel == SNMP_SEC_LEVEL_AUTHPRIV) {
                sess.securityPrivProto = host->priv_protocol;
                sess.securityPrivProtoLen = host->priv_protocol_len;
                sess.securityPrivKeyLen = USM_PRIV_KU_LEN;
                error = generate_Ku(sess.securityAuthProto, sess.securityAuthProtoLen,
                                    (u_char *)host->priv_passphrase,
                                    strlen(host->priv_passphrase), sess.securityPrivKey,
                                    &sess.securityPrivKeyLen);
                if (error != SNMPERR_SUCCESS) {
                    PLUGIN_ERROR("host %s: Error generating Ku from priv_passphrase. (Error %d)",
                                 host->name, error);
                }
            }
        }

        if (host->context != NULL) {
            sess.contextName = host->context;
            sess.contextNameLen = strlen(host->context);
        }
    } else { /* SNMPv1/2 "authenticates" with community string */
        sess.community = (u_char *)host->community;
        sess.community_len = strlen(host->community);
    }

    /* Set timeout & retries, if they have been changed from the default */
    if (host->timeout != 0) {
        /* net-snmp expects microseconds */
        sess.timeout = CDTIME_T_TO_US(host->timeout);
    }

    if (host->retries >= 0)
        sess.retries = host->retries;

    /* snmp_sess_open will copy the 'struct snmp_session *'. */
    host->sess_handle = snmp_sess_open(&sess);

    if (host->sess_handle == NULL) {
        char *errstr = NULL;

        snmp_error(&sess, NULL, NULL, &errstr);

        PLUGIN_ERROR("host %s: snmp_sess_open failed: %s", host->name,
                     (errstr == NULL) ? "Unknown problem" : errstr);
        free(errstr);
    }
}

/* TODO: Check if negative values wrap around. Problem: negative temperatures. */
static value_t csnmp_value_list_to_value(const struct variable_list *vl,
                                         metric_type_t type, double scale, double shift,
                                         const char *host_name, const char *data_name)
{
    value_t ret = VALUE_UNKNOWN(NAN);

    uint64_t tmp_unsigned = 0;
    int64_t tmp_signed = 0;
    bool defined = 1;
    /* Set to true when the original SNMP type appears to have been signed. */
    bool prefer_signed = 0;

    if ((vl->type == ASN_INTEGER) || (vl->type == ASN_UINTEGER) || (vl->type == ASN_COUNTER)
#ifdef ASN_TIMETICKS
            || (vl->type == ASN_TIMETICKS)
#endif
            || (vl->type == ASN_GAUGE)) {
        tmp_unsigned = (uint32_t)*vl->val.integer;
        tmp_signed = (int32_t)*vl->val.integer;

        if (vl->type == ASN_INTEGER)
            prefer_signed = 1;

        PLUGIN_DEBUG("Parsed int32 value is %" PRIu64 ".", tmp_unsigned);
    } else if (vl->type == ASN_COUNTER64) {
        tmp_unsigned = (uint32_t)vl->val.counter64->high;
        tmp_unsigned = tmp_unsigned << 32;
        tmp_unsigned += (uint32_t)vl->val.counter64->low;
        tmp_signed = (int64_t)tmp_unsigned;
        PLUGIN_DEBUG("Parsed int64 value is %" PRIu64 ".", tmp_unsigned);
    } else if (vl->type == ASN_OCTET_STR) {
        /* We'll handle this later.. */
    } else {
        char oid_buffer[1024] = {0};

        snprint_objid(oid_buffer, sizeof(oid_buffer) - 1, vl->name, vl->name_length);
#ifdef ASN_NULL
        if (vl->type == ASN_NULL)
            PLUGIN_INFO("OID \"%s\" is undefined (type ASN_NULL)", oid_buffer);
        else
#endif
            PLUGIN_WARNING("I don't know the ASN type #%i "
                           "(OID: \"%s\", data block \"%s\", host block \"%s\")",
                           (int)vl->type, oid_buffer,
                           (data_name != NULL) ? data_name : "UNKNOWN",
                           (host_name != NULL) ? host_name : "UNKNOWN");

        defined = 0;
    }

    if (vl->type == ASN_OCTET_STR) {
        int status = -1;

        if (vl->val.string != NULL) {
            char string[64];
            size_t string_length;

            string_length = sizeof(string) - 1;
            if (vl->val_len < string_length)
                string_length = vl->val_len;

            /* The strings we get from the Net-SNMP library may not be null
             * terminated. That is why we're using 'memcpy' here and not 'strcpy'.
             * 'string_length' is set to 'vl->val_len' which holds the length of the
             * string.  -octo */
            memcpy(string, vl->val.string, string_length);
            string[string_length] = 0;

            if (type == METRIC_TYPE_COUNTER) {
                uint64_t raw = 0;
                status = parse_uinteger(string, &raw);
                ret = VALUE_COUNTER(raw);
            } else {
                double raw = 0;
                status = parse_double(string, &raw);
                ret = VALUE_GAUGE(raw);
            }
            if (status != 0) {
                PLUGIN_ERROR("host %s: csnmp_value_list_to_value: Parsing string failed: %s",
                              host_name, string);
            }
        }

        if (status != 0) {
            if (type == METRIC_TYPE_COUNTER) {
                ret = VALUE_COUNTER(0);
            } else if (type == METRIC_TYPE_GAUGE) {
                ret = VALUE_GAUGE(NAN);
            } else {
                PLUGIN_ERROR("Unknown data source type: %u.", type);
                ret = VALUE_UNKNOWN(NAN);
            }
        }
    } else if (type == METRIC_TYPE_COUNTER) {
        ret = VALUE_COUNTER(tmp_unsigned);
    } else if (type == METRIC_TYPE_GAUGE) {
        if (!defined) {
            ret = VALUE_GAUGE(NAN);
        } else if (prefer_signed) {
            ret = VALUE_GAUGE((scale * tmp_signed) + shift);
        } else {
            ret = VALUE_GAUGE((scale * tmp_unsigned) + shift);
        }
    } else {
        PLUGIN_ERROR("csnmp_value_list_to_value: Unknown data source type: %u.", type);
        ret = VALUE_UNKNOWN(NAN);
    }

    return ret;
}

/* csnmp_strvbcopy_hexstring converts the bit string contained in "vb" to a hex
 * representation and writes it to dst. Returns zero on success and ENOMEM if
 * dst is not large enough to hold the string. dst is guaranteed to be
 * nul-terminated. */
static int csnmp_strvbcopy_hexstring(char *dst, const struct variable_list *vb, size_t dst_size)
{
    char *buffer_ptr;
    size_t buffer_free;

    dst[0] = 0;

    buffer_ptr = dst;
    buffer_free = dst_size;

    for (size_t i = 0; i < vb->val_len; i++) {
        int status;

        status = ssnprintf(buffer_ptr, buffer_free, (i == 0) ? "%02x" : ":%02x",
                                             (unsigned int)vb->val.bitstring[i]);
        assert(status >= 0);

        if (((size_t)status) >= buffer_free) { /* truncated */
            dst[dst_size - 1] = '\0';
            return ENOMEM;
        } else { /* if (status < buffer_free) */
            buffer_ptr += (size_t)status;
            buffer_free -= (size_t)status;
        }
    }

    return 0;
}

/* csnmp_strvbcopy copies the octet string or bit string contained in vb to
 * dst. If non-printable characters are detected, it will switch to a hex
 * representation of the string. Returns zero on success, EINVAL if vb does not
 * contain a string and ENOMEM if dst is not large enough to contain the
 * string. */
static int csnmp_strvbcopy(char *dst, const struct variable_list *vb, size_t dst_size)
{
    char *src;
    size_t num_chars;

    if (vb->type == ASN_OCTET_STR)
        src = (char *)vb->val.string;
    else if (vb->type == ASN_BIT_STR)
        src = (char *)vb->val.bitstring;
    else if (vb->type == ASN_IPADDRESS) {
        return ssnprintf(dst, dst_size, "%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 "",
                              (uint8_t)vb->val.string[0], (uint8_t)vb->val.string[1],
                              (uint8_t)vb->val.string[2], (uint8_t)vb->val.string[3]);
    } else {
        dst[0] = 0;
        return EINVAL;
    }

    num_chars = dst_size - 1;
    if (num_chars > vb->val_len)
        num_chars = vb->val_len;

    for (size_t i = 0; i < num_chars; i++) {
        /* Check for control characters. */
        if ((unsigned char)src[i] < 32)
            return csnmp_strvbcopy_hexstring(dst, vb, dst_size);
        dst[i] = src[i];
    }
    dst[num_chars] = 0;
    dst[dst_size - 1] = '\0';

    if (dst_size <= vb->val_len)
        return ENOMEM;

    return 0;
}

static int csnmp_variable_list_to_str(char *dst, const struct variable_list *vb, size_t dst_size)
{
    switch(vb->type){
    case ASN_OCTET_STR:
    case ASN_BIT_STR:
    case ASN_IPADDRESS:
        return csnmp_strvbcopy(dst, vb, dst_size);
        break;
    case ASN_INTEGER:
        if (dst_size <= ITOA_MAX) {
            dst[0] = '\0';
            return -1;
        } else {
            int64_t tmp_signed = (int32_t)*vb->val.integer;
            itoa(tmp_signed, dst);
        }
        break;
    case ASN_UINTEGER:
    case ASN_COUNTER:
#ifdef ASN_TIMETICKS
    case ASN_TIMETICKS:
#endif
    case ASN_GAUGE:
        if (dst_size <= ITOA_MAX) {
            dst[0] = '\0';
            return -1;
        } else {
            uint64_t tmp_unsigned = (uint32_t)*vb->val.integer;
            uitoa(tmp_unsigned, dst);
        }
        break;
    case ASN_COUNTER64:
        if (dst_size <= ITOA_MAX) {
            dst[0] = '\0';
            return -1;
        } else {
            uint64_t tmp_unsigned = (uint32_t)vb->val.counter64->high;
            tmp_unsigned = tmp_unsigned << 32;
            tmp_unsigned += (uint32_t)vb->val.counter64->low;
            uitoa(tmp_unsigned, dst);
        }
        break;
    default:
        dst[0] = '\0';
        return -1;
        break;
    }

    return 0;
}

static csnmp_cell_char_t *csnmp_get_char_cell(const struct variable_list *vb, const oid_t *root_oid)
{
    if (vb == NULL)
        return NULL;

    csnmp_cell_char_t *il = calloc(1, sizeof(*il));
    if (il == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return NULL;
    }
    il->next = NULL;

    oid_t vb_name;
    csnmp_oid_init(&vb_name, vb->name, vb->name_length);

    int status = csnmp_oid_suffix(&il->suffix, &vb_name, root_oid);
    if (status != 0) {
        free(il);
        return NULL;
    }

    /* Get value */
    status = csnmp_variable_list_to_str(il->value, vb, sizeof(il->value));
    if (status != 0) {
        free(il);
        return NULL;
    }

    return il;
}

static void csnmp_cells_append(csnmp_cell_char_t **head, csnmp_cell_char_t **tail,
                                                         csnmp_cell_char_t *il)
{
    if (*head == NULL)
        *head = il;
    else
        (*tail)->next = il;
    *tail = il;
}

static int csnmp_dispatch_table(host_definition_t *host, data_definition_t *data,
                                csnmp_cell_char_t **label_from_cells,
                                csnmp_cell_char_t *filter_cells,
                                csnmp_cell_value_t *value_cells,
                                bool count_values)
{
    csnmp_cell_char_t *label_cell_ptr[data->labels_from.num+1];
    csnmp_cell_char_t *filter_cell_ptr = filter_cells;
    csnmp_cell_value_t *value_cell_ptr = value_cells;

    oid_t current_suffix;
    uint64_t count = 0;

    for (size_t i = 0; i < data->labels_from.num; i++)
        label_cell_ptr[i] = label_from_cells[i];

    bool have_more = 1;
    while (have_more) {
        bool suffix_skipped = 0;
        csnmp_cell_value_t *ptr = value_cell_ptr;
        if (ptr == NULL) {
            have_more = 0;
            continue;
        }

        memcpy(&current_suffix, &ptr->suffix, sizeof(current_suffix));

        /* Update filter_cell_ptr to point expected suffix */
        if (filter_cells != NULL) {
            while ((filter_cell_ptr != NULL) &&
                         (csnmp_oid_compare(&filter_cell_ptr->suffix, &current_suffix) < 0))
                filter_cell_ptr = filter_cell_ptr->next;

            if (filter_cell_ptr == NULL) {
                have_more = 0;
                continue;
            }

            if (csnmp_oid_compare(&filter_cell_ptr->suffix, &current_suffix) > 0) {
                /* This suffix is missing in the subtree. Indicate this with the
                 * "suffix_skipped" flag and try the next instance / suffix. */
                suffix_skipped = 1;
            }
        }

        if (value_cells != NULL) {
            while ((value_cell_ptr != NULL) &&
                         (csnmp_oid_compare(&value_cell_ptr->suffix, &current_suffix) < 0))
                value_cell_ptr = value_cell_ptr->next;

            if (value_cell_ptr == NULL) {
                have_more = 0;
                continue;
            }

            if (csnmp_oid_compare(&value_cell_ptr->suffix, &current_suffix) > 0) {
                /* This suffix is missing in the subtree. Indicate this with the
                 * "suffix_skipped" flag and try the next instance / suffix. */
                suffix_skipped = 1;
            }
        }

        /* Update all the value_cell_ptr to point at the entry with the same
         * trailing partial OID */
        for (size_t i = 0; i < data->labels_from.num; i++) {
            while ((label_cell_ptr[i] != NULL) &&
                         (csnmp_oid_compare(&label_cell_ptr[i]->suffix, &current_suffix) < 0))
                label_cell_ptr[i] = label_cell_ptr[i]->next;

            if (label_cell_ptr[i] == NULL) {
                have_more = 0;
                break;
            } else if (csnmp_oid_compare(&label_cell_ptr[i]->suffix, &current_suffix) > 0) {
                /* This suffix is missing in the subtree. Indicate this with the
                 * "suffix_skipped" flag and try the next instance / suffix. */
                suffix_skipped = 1;
                break;
            }
        }

        if (!have_more)
            break;

        /* Matching the values failed. Start from the beginning again. */
        if (suffix_skipped) {
            value_cell_ptr = value_cell_ptr->next;
            continue;
        }

       /* if we reach this line, all value_cell_ptr[i] are non-NULL and are set
        * to the same subid. type_instance_cell_ptr is either NULL or points to the
        * same subid, too. */

        if (filter_cell_ptr != NULL) {
            if(!exclist_match(&data->exclist, filter_cell_ptr->value)) {
                value_cell_ptr = value_cell_ptr->next;
                continue;
            }
        }

        if (count_values) {
            count++;
        } else {
            metric_family_t fam = {0};

            strbuf_t buf = STRBUF_CREATE;
            if (host->metric_prefix != NULL)
                strbuf_print(&buf, host->metric_prefix);
            strbuf_print(&buf, data->metric);
            fam.name = buf.ptr;
            fam.type = data->type;
            fam.help = data->help;

            metric_t m = {0};

            for (size_t i = 0; i < host->labels.num; i++) {
                metric_label_set(&m, host->labels.ptr[i].name, host->labels.ptr[i].value);
            }

            for (size_t i = 0; i < data->labels.num; i++) {
                metric_label_set(&m, data->labels.ptr[i].name, data->labels.ptr[i].value);
            }

            for (size_t i = 0; i < data->labels_from.num; i++) {
                if (label_cell_ptr[i] != NULL) {
                    label_oid_t *loid = &data->labels_from.ptr[i];
                    metric_label_set(&m, loid->label, label_cell_ptr[i]->value);
                }
            }

            m.value = value_cell_ptr->value;

            metric_family_metric_append(&fam, m);
            metric_reset(&m, fam.type);
            plugin_dispatch_metric_family(&fam, 0);

            strbuf_destroy(&buf);
        }

        value_cell_ptr = value_cell_ptr->next;
    }

    if (count_values) {
        metric_family_t fam = {0};

        strbuf_t buf = STRBUF_CREATE;
        if (host->metric_prefix != NULL)
            strbuf_print(&buf, host->metric_prefix);
        strbuf_print(&buf, data->metric);
        fam.name = buf.ptr;
        fam.type = data->type;
        fam.help = data->help;

        metric_t m = {0};

        for (size_t i = 0; i < host->labels.num; i++) {
            metric_label_set(&m, host->labels.ptr[i].name, host->labels.ptr[i].value);
        }

        for (size_t i = 0; i < data->labels.num; i++) {
            metric_label_set(&m, data->labels.ptr[i].name, data->labels.ptr[i].value);
        }

        if (data->type == METRIC_TYPE_COUNTER) {
            m.value = VALUE_COUNTER(count);
        } else {
            m.value = VALUE_GAUGE(count);
        }

        metric_family_metric_append(&fam, m);
        metric_reset(&m, fam.type);
        plugin_dispatch_metric_family(&fam, 0);
        strbuf_destroy(&buf);
    }

    return 0;
}

static int csnmp_read_table(host_definition_t *host, data_definition_t *data)
{
    struct snmp_pdu *req;
    struct snmp_pdu *res = NULL;
    struct variable_list *vb;

    size_t oid_list_len = 1 + data->labels_from.num + (data->filter_oid.len > 0 ? 1 : 0);

    /* Holds the last OID returned by the device. We use this in the GETNEXT request to proceed. */
    oid_t oid_list[oid_list_len];
    /* Set to false when an OID has left its subtree so we don't re-request it again. */
    csnmp_oid_type_t oid_list_todo[oid_list_len];

    int status;

    csnmp_cell_char_t **label_from_cells_head = NULL;
    csnmp_cell_char_t **label_from_cells_tail = NULL;
    if (data->labels_from.num > 0) {
        label_from_cells_head = calloc(data->labels_from.num, sizeof(*label_from_cells_head));
        label_from_cells_tail = calloc(data->labels_from.num, sizeof(*label_from_cells_tail));
        if ((label_from_cells_head == NULL) || (label_from_cells_tail == NULL)) {
            PLUGIN_ERROR("calloc failed.");
            free(label_from_cells_head);
            free(label_from_cells_tail);
            return -1;
        }
    }

    csnmp_cell_char_t *filter_cells_head = NULL;
    csnmp_cell_char_t *filter_cells_tail = NULL;

    csnmp_cell_value_t *value_cells_head = NULL;
    csnmp_cell_value_t *value_cells_tail = NULL;

    PLUGIN_DEBUG("(host = %s, data = %s)", host->name, data->name);

    if (host->sess_handle == NULL) {
        PLUGIN_DEBUG("host->sess_handle == NULL");
        free(label_from_cells_head);
        free(label_from_cells_tail);
        return -1;
    }

    size_t n = 0;
    for (size_t i = 0; i < data->labels_from.num; i++) {
        memcpy(oid_list + n, &data->labels_from.ptr[i].oid, sizeof(oid_t));
        oid_list_todo[n] = OID_TYPE_LABEL;
        n++;
    }

    if (data->filter_oid.len > 0) {
        memcpy(oid_list + n, &data->filter_oid, sizeof(oid_t));
        oid_list_todo[n] = OID_TYPE_FILTER;
        n++;
    }

    memcpy(oid_list + n, &data->value_oid, sizeof(oid_t));
    oid_list_todo[n] = OID_TYPE_VARIABLE;

    status = 0;
    while (status == 0) {
        /* If SNMP v2 and later and bulk transfers enabled, use GETBULK PDU */
        if (host->version > 1 && host->bulk_size > 0) {
            req = snmp_pdu_create(SNMP_MSG_GETBULK);
            req->non_repeaters = 0;
            req->max_repetitions = host->bulk_size;
        } else {
            req = snmp_pdu_create(SNMP_MSG_GETNEXT);
        }

        if (req == NULL) {
            PLUGIN_ERROR("snmp_pdu_create failed.");
            status = -1;
            break;
        }

        size_t var_idx[oid_list_len];
        memset(var_idx, 0, sizeof(var_idx));

        int oid_list_todo_num = 0;
        for (size_t i = 0; i < oid_list_len; i++) {
            /* Do not rerequest already finished OIDs */
            if (!oid_list_todo[i])
                continue;
            snmp_add_null_var(req, oid_list[i].oid, oid_list[i].len);
            var_idx[oid_list_todo_num] = i;
            oid_list_todo_num++;
        }

        if (oid_list_todo_num == 0) {
            /* The request is still empty - so we are finished */
            PLUGIN_DEBUG("all variables have left their subtree");
            snmp_free_pdu(req);
            status = 0;
            break;
        }

        if (req->command == SNMP_MSG_GETBULK) {
            /* In bulk mode the host will send 'max_repetitions' values per
                 requested variable, so we need to split it per number of variable
                 to stay 'in budget' */
            req->max_repetitions = floor(host->bulk_size / oid_list_todo_num);
        }

        res = NULL;
        status = snmp_sess_synch_response(host->sess_handle, req, &res);
        /* snmp_sess_synch_response always frees our req PDU */
        /* req = NULL; */

        if ((status != STAT_SUCCESS) || (res == NULL)) {
            char *errstr = NULL;

            snmp_sess_error(host->sess_handle, NULL, NULL, &errstr);

            c_complain(LOG_ERR, &host->complaint, "host %s: snmp_sess_synch_response failed: %s",
                                 host->name, (errstr == NULL) ? "Unknown problem" : errstr);

            if (res != NULL)
                snmp_free_pdu(res);
            res = NULL;

            free(errstr);
            csnmp_host_close_session(host);

            status = -1;
            break;
        }

        status = 0;
        assert(res != NULL);
        c_release(LOG_INFO, &host->complaint, "host %s: snmp_sess_synch_response successful.",
                            host->name);

        vb = res->variables;
        if (vb == NULL) {
            status = -1;
            break;
        }

        if (res->errstat != SNMP_ERR_NOERROR) {
            if (res->errindex != 0) {
                /* Find the OID which caused error */
                int i;
                for (i = 1, vb = res->variables; vb != NULL && i != res->errindex;
                         vb = vb->next_variable, i++)
                    /* do nothing */;
            }

            if ((res->errindex == 0) || (vb == NULL)) {
                PLUGIN_ERROR("host %s; data %s: response error: %s (%li) ",
                            host->name, data->name, snmp_errstring(res->errstat),
                            res->errstat);
                status = -1;
                break;
            }

            char oid_buffer[1024] = {0};
            snprint_objid(oid_buffer, sizeof(oid_buffer) - 1, vb->name, vb->name_length);
            PLUGIN_NOTICE("host %s; data %s: OID '%s' failed: %s", host->name,
                          data->name, oid_buffer, snmp_errstring(res->errstat));

            /* Get value index from todo list and skip OID found */
            assert(res->errindex <= oid_list_todo_num);
            size_t i = var_idx[res->errindex - 1];
            assert(i < oid_list_len);
            oid_list_todo[i] = 0;

            snmp_free_pdu(res);
            res = NULL;
            continue;
        }

        size_t j;
        for (vb = res->variables, j = 0; (vb != NULL); vb = vb->next_variable, j++) {
            size_t i = j;
            /* If bulk request is active convert value index of the extra value */
            if (host->version > 1 && host->bulk_size > 0)
                i %= oid_list_todo_num;
            /* Calculate value index from todo list */
            while ((i < oid_list_len) && !oid_list_todo[i]) {
                i++;
                j++;
            }
            if (i >= oid_list_len)
                break;

            if (oid_list_todo[i] == OID_TYPE_SKIP)
                continue;

            if (oid_list_todo[i] == OID_TYPE_LABEL) {
                if (i >= data->labels_from.num) {
                    oid_list_todo[i] = 0;
                    continue;
                }

                label_oid_t *loid = &data->labels_from.ptr[i];

                if ((vb->type == SNMP_ENDOFMIBVIEW) ||
                    (snmp_oid_ncompare(loid->oid.oid, loid->oid.len,
                                        vb->name, vb->name_length, loid->oid.len) != 0)) {
                    PLUGIN_DEBUG("host = %s; data = %s; Host left its subtree.",
                                 host->name, data->name);
                    oid_list_todo[i] = 0;
                    continue;
                }

                csnmp_cell_char_t *cell = csnmp_get_char_cell(vb, &loid->oid);
                if (cell == NULL) {
                    PLUGIN_ERROR("host %s: csnmp_get_char_cell() failed.", host->name);
                    status = -1;
                    break;
                }

                PLUGIN_DEBUG("il->plugin_instance = '%s';", cell->value);
                csnmp_cells_append(&label_from_cells_head[i], &label_from_cells_tail[i], cell);
            } else if (oid_list_todo[i] == OID_TYPE_FILTER) {
                if ((vb->type == SNMP_ENDOFMIBVIEW) ||
                    (snmp_oid_ncompare(data->filter_oid.oid, data->filter_oid.len,
                                        vb->name, vb->name_length, data->filter_oid.len) != 0)) {
                    PLUGIN_DEBUG("host = %s; data = %s; Host left its subtree.",
                                host->name, data->name);
                    oid_list_todo[i] = 0;
                    continue;
                }

                /* Allocate a new 'csnmp_cell_char_t', insert the instance name and
                 * add it to the list */
                csnmp_cell_char_t *cell = csnmp_get_char_cell(vb, &data->filter_oid);
                if (cell == NULL) {
                    PLUGIN_ERROR("host %s: csnmp_get_char_cell() failed.", host->name);
                    status = -1;
                    break;
                }

                PLUGIN_DEBUG("il->filter = '%s';", cell->value);
                csnmp_cells_append(&filter_cells_head, &filter_cells_tail, cell);
            } else if (oid_list_todo[i] == OID_TYPE_VARIABLE) {
                oid_t vb_name;
                oid_t suffix;

                csnmp_oid_init(&vb_name, vb->name, vb->name_length);
                /* Calculate the current suffix. This is later used to check that the
                 * suffix is increasing. This also checks if we left the subtree */
                int ret = csnmp_oid_suffix(&suffix, &vb_name, &data->value_oid);
                if (ret != 0) {
                    PLUGIN_DEBUG("host = %s; data = %s; i = %" PRIsz "; "
                                 "Value probably left its subtree.", host->name, data->name, i);
                    oid_list_todo[i] = 0;
                    continue;
                }

                /* Make sure the OIDs returned by the agent are increasing. Otherwise
                 * our table matching algorithm will get confused. */
                if ((value_cells_tail != NULL) &&
                        (csnmp_oid_compare(&suffix, &value_cells_tail->suffix) <= 0)) {
                    PLUGIN_DEBUG("host = %s; data = %s; i = %" PRIsz "; "
                                 "Suffix is not increasing.", host->name, data->name, i);
                    oid_list_todo[i] = 0;
                    continue;
                }

                csnmp_cell_value_t *vt = calloc(1, sizeof(*vt));
                if (vt == NULL) {
                    PLUGIN_ERROR("calloc failed.");
                    status = -1;
                    break;
                }

                vt->value = csnmp_value_list_to_value(vb, data->type, data->scale, data->shift,
                                                          host->name, data->name);
                memcpy(&vt->suffix, &suffix, sizeof(vt->suffix));
                vt->next = NULL;

                if (value_cells_tail == NULL)
                    value_cells_head = vt;
                else
                    value_cells_tail->next = vt;
                value_cells_tail = vt;
            }

            /* Copy OID to oid_list[i] */
            memcpy(oid_list[i].oid, vb->name, sizeof(oid) * vb->name_length);
            oid_list[i].len = vb->name_length;
        }

        if (res != NULL)
            snmp_free_pdu(res);
        res = NULL;
    }

    if (res != NULL)
        snmp_free_pdu(res);
    res = NULL;

    if (status == 0)
        csnmp_dispatch_table(host, data, label_from_cells_head, filter_cells_head,
                                         value_cells_head, data->count);

    /* Free all allocated variables here */
    for (size_t i = 0; i < data->labels_from.num; i++) {
        if (label_from_cells_head != NULL) {
            while (label_from_cells_head[i] != NULL) {
                csnmp_cell_char_t *next = label_from_cells_head[i]->next;
                free(label_from_cells_head[i]);
                label_from_cells_head[i] = next;
            }
        }
    }
    free(label_from_cells_head);
    free(label_from_cells_tail);


    while (filter_cells_head != NULL) {
        csnmp_cell_char_t *next = filter_cells_head->next;
        free(filter_cells_head);
        filter_cells_head = next;
    }

    while (value_cells_head != NULL) {
        csnmp_cell_value_t *next = value_cells_head->next;
        free(value_cells_head);
        value_cells_head = next;
    }

    return 0;
}

static int csnmp_read_value(host_definition_t *host, data_definition_t *data)
{

    PLUGIN_DEBUG("csnmp_read_value (host = %s, data = %s)", host->name, data->name);

    if (host->sess_handle == NULL) {
        PLUGIN_DEBUG("csnmp_read_value: host->sess_handle == NULL");
        return -1;
    }

    struct snmp_pdu *req = snmp_pdu_create(SNMP_MSG_GET);
    if (req == NULL) {
        PLUGIN_ERROR("snmp_pdu_create failed.");
        return -1;
    }

    snmp_add_null_var(req, data->value_oid.oid, data->value_oid.len);

    for (size_t i = 0; i < data->labels_from.num; i++) {
        label_oid_t *loid = &data->labels_from.ptr[i];
        snmp_add_null_var(req, loid->oid.oid, loid->oid.len);
    }

    struct snmp_pdu *res = NULL;
    int status = snmp_sess_synch_response(host->sess_handle, req, &res);
    if ((status != STAT_SUCCESS) || (res == NULL)) {
        char *errstr = NULL;

        snmp_sess_error(host->sess_handle, NULL, NULL, &errstr);
        PLUGIN_ERROR("host %s: snmp_sess_synch_response failed: %s",
                                host->name, (errstr == NULL) ? "Unknown problem" : errstr);

        if (res != NULL)
            snmp_free_pdu(res);

        free(errstr);
        csnmp_host_close_session(host);
        return -1;
    }

    struct variable_list *vb = res->variables;
    if (vb == NULL) {
        PLUGIN_ERROR("snmp_sess_synch_response null variables.");
        snmp_free_pdu(res);
        return -1;
    }

#ifdef NCOLLECTD_DEBUG
    char buffer[1024];
    snprint_variable(buffer, sizeof(buffer), vb->name, vb->name_length, vb);
    PLUGIN_DEBUG("Got this variable: %s", buffer);
#endif

    metric_t m = {0};

    for (size_t i = 0; i < host->labels.num; i++) {
        metric_label_set(&m, host->labels.ptr[i].name, host->labels.ptr[i].value);
    }
    for (size_t i = 0; i < data->labels.num; i++) {
        metric_label_set(&m, data->labels.ptr[i].name, data->labels.ptr[i].value);
    }

    for (vb = res->variables; vb != NULL; vb = vb->next_variable) {
        if (snmp_oid_compare(data->value_oid.oid, data->value_oid.len,
                             vb->name, vb->name_length) == 0) {
            m.value = csnmp_value_list_to_value(vb, data->type, data->scale, data->shift,
                                                    host->name, data->name);

        } else  {
            for (size_t i = 0; i < data->labels_from.num; i++) {
                label_oid_t *loid = &data->labels_from.ptr[i];
                if (snmp_oid_compare(loid->oid.oid, loid->oid.len,
                                     vb->name, vb->name_length) == 0) {

                    char lvalue[BUFFER_DATA_SIZE];
                    status = csnmp_variable_list_to_str(lvalue, vb, sizeof(lvalue));
                    if (status != 0)
                        continue;

                    metric_label_set(&m, loid->label, lvalue);
                }
            }
        }
    }

    snmp_free_pdu(res);

    metric_family_t fam = {0};

    strbuf_t buf = STRBUF_CREATE;
    if (host->metric_prefix != NULL)
        strbuf_print(&buf, host->metric_prefix);
    strbuf_print(&buf, data->metric);
    fam.name = buf.ptr;
    fam.type = data->type;
    fam.help = data->help;


    metric_family_metric_append(&fam, m);
    metric_reset(&m, fam.type);
    plugin_dispatch_metric_family(&fam, 0);
    strbuf_destroy(&buf);
    return 0;
}

static int csnmp_read_host(user_data_t *ud)
{
    host_definition_t *host = ud->data;

    if (host->sess_handle == NULL)
        csnmp_host_open_session(host);

    if (host->sess_handle == NULL)
        return -1;

    int success = 0;
    for (int i = 0; i < host->data_list_len; i++) {
        data_definition_t *data = host->data_list[i];

        int status = 0;

        if (data->is_table)
            status = csnmp_read_table(host, data);
        else
            status = csnmp_read_value(host, data);

        if (status == 0)
            success++;
    }

    if (success == 0)
        return -1;

    return 0;
}

static int csnmp_config_get_label_oid(config_item_t *ci, label_oid_set_t *set)
{
    if ((ci->values_num != 2) ||
        ((ci->values_num == 2) && ((ci->values[0].type != CONFIG_TYPE_STRING) ||
         (ci->values[1].type != CONFIG_TYPE_STRING)))) {
        PLUGIN_ERROR("The '%s' option requires exactly two string arguments.", ci->key);
        return -1;
    }

    char *label = strdup(ci->values[0].value.string);
    if (label == NULL)
        return -1;

    oid_t oid = {.len = MAX_OID_LEN};

    if (snmp_parse_oid(ci->values[1].value.string, oid.oid, &oid.len) == NULL) {
        PLUGIN_ERROR("snmp_parse_oid (%s) failed.", ci->values[1].value.string);
        free(label);
        return -1;
    }

    label_oid_t *tmp = realloc(set->ptr, sizeof(*set->ptr) * (set->num + 1));
    if (tmp == NULL) {
        free(label);
        return ENOMEM;
    }

    set->ptr = tmp;
    set->ptr[set->num].label =  label;
    set->ptr[set->num].oid = oid;
    set->num++;

    return 0;
}

static int csnmp_config_get_oid(config_item_t *ci, oid_t *oid)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("the '%s' option requires exactly one string argument.", ci->key);
        return -1;
    }

    oid->len = MAX_OID_LEN;
    if (snmp_parse_oid(ci->values[0].value.string, oid->oid, &(oid->len)) == NULL) {
        PLUGIN_ERROR("snmp_parse_oid (%s) failed.", ci->values[0].value.string);
        return -1;
    }

    return 0;
}

static int csnmp_config_add_data_filter_oid(data_definition_t *data, config_item_t *ci)
{
    char buffer[BUFFER_DATA_SIZE];
    int status = cf_util_get_string_buffer(ci, buffer, sizeof(buffer));
    if (status != 0)
        return status;

    data->filter_oid.len = MAX_OID_LEN;

    if (!read_objid(buffer, data->filter_oid.oid, &data->filter_oid.len)) {
        PLUGIN_ERROR("read_objid (%s) failed.", buffer);
        return -1;
    }
    return 0;
}

static int csnmp_config_add_data(config_item_t *ci)
{
    data_definition_t *dd = calloc(1, sizeof(*dd));
    if (dd == NULL)
        return -1;

    int status = cf_util_get_string(ci, &dd->name);
    if (status != 0) {
        free(dd);
        return -1;
    }

    dd->scale = 1.0;
    dd->shift = 0.0;
    dd->type = METRIC_TYPE_GAUGE;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *option = ci->children + i;

        if (strcasecmp("table", option->key) == 0) {
            status = cf_util_get_boolean(option, &dd->is_table);
        } else if (strcasecmp("type", option->key) == 0) {
            status = cf_util_get_metric_type(option, &dd->type);
        } else if (strcasecmp("help", option->key) == 0) {
            status = cf_util_get_string(option, &dd->help);
        } else if (strcasecmp("metric", option->key) == 0) {
            status = cf_util_get_string(option, &dd->metric);
        } else if (strcasecmp("label", option->key) == 0) {
            status = cf_util_get_label(option, &dd->labels);
        } else if (strcasecmp("label-from", option->key) == 0) {
            status = csnmp_config_get_label_oid(option, &dd->labels_from);
        } else if (strcasecmp("value", option->key) == 0) {
            status = csnmp_config_get_oid(option, &dd->value_oid);
        } else if (strcasecmp("shift", option->key) == 0) {
            status = cf_util_get_double(option, &dd->shift);
        } else if (strcasecmp("scale", option->key) == 0) {
            status = cf_util_get_double(option, &dd->scale);
        } else if (strcasecmp("filter-oid", option->key) == 0) {
            status = csnmp_config_add_data_filter_oid(dd, option);
        } else if (strcasecmp("filter-value", option->key) == 0) {
            status = cf_util_exclist(option, &dd->exclist);
        } else if (strcasecmp("count", option->key) == 0) {
            status = cf_util_get_boolean(option, &dd->count);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          option->key, cf_get_file(option), cf_get_lineno(option));
            status = -1;
        }

        if (status != 0)
            break;
    }

    while (status == 0) {
        if (!dd->is_table && dd->count) {
            PLUGIN_ERROR("'count' is set to 'true' and 'table' set to 'false' for data '%s'",
                         dd->name);
            status = -1;
            break;
        }

        if (dd->metric == NULL) {
            PLUGIN_ERROR("No 'metric' name given for data '%s'", dd->name);
            status = -1;
            break;
        }

        if (dd->value_oid.len == 0) {
            PLUGIN_ERROR("No 'value' oid given for data '%s'", dd->name);
            status = -1;
            break;
        }

        if ((dd->type != METRIC_TYPE_COUNTER) && (dd->type != METRIC_TYPE_GAUGE)) {
            PLUGIN_ERROR("type is not 'counter' or 'gauge'.");
            status = -1;
            break;
        }

        break;
    }

    if (status != 0) {
        csnmp_data_definition_destroy(dd);
        return -1;
    }

    if (data_head == NULL) {
        data_head = dd;
    } else {
        data_definition_t *last;
        last = data_head;
        while (last->next != NULL)
            last = last->next;
        last->next = dd;
    }

    return 0;
}

static int csnmp_config_add_host_version(host_definition_t *hd, config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_NUMBER)) {
        PLUGIN_WARNING("The 'version' config option needs exactly one number argument.");
        return -1;
    }

    int version = (int)ci->values[0].value.number;
    if ((version < 1) || (version > 3)) {
        PLUGIN_WARNING("'version' must either be '1', '2', or '3'.");
        return -1;
    }

    hd->version = version;

    return 0;
}

static int csnmp_config_add_host_collect(host_definition_t *host, config_item_t *ci)
{
    if (ci->values_num < 1) {
        PLUGIN_WARNING("'collect' needs at least one argument.");
        return -1;
    }

    for (int i = 0; i < ci->values_num; i++) {
        if (ci->values[i].type != CONFIG_TYPE_STRING) {
            PLUGIN_WARNING("All arguments to 'collect' must be strings.");
            return -1;
        }
    }

    int data_list_len = host->data_list_len + ci->values_num;
    data_definition_t **data_list = realloc(host->data_list,
                                            sizeof(data_definition_t *) * data_list_len);
    if (data_list == NULL)
        return -1;

    host->data_list = data_list;
    for (int i = 0; i < ci->values_num; i++) {
        data_definition_t *data = NULL;
        for (data = data_head; data != NULL; data = data->next) {
            if (strcasecmp(ci->values[i].value.string, data->name) == 0)
                break;
        }

        if (data == NULL) {
            PLUGIN_WARNING("No such data configured: '%s'", ci->values[i].value.string);
            continue;
        }

        PLUGIN_DEBUG("collect: host = %s, data[%i] = %s;", host->name,
                                                           host->data_list_len, data->name);

        host->data_list[host->data_list_len] = data;
        host->data_list_len++;
    }

    return 0;
}

static int csnmp_config_add_host_auth_protocol(host_definition_t *hd, config_item_t *ci)
{
    char buffer[MAX_DIGEST_NAME_LEN];
    int status;

    status = cf_util_get_string_buffer(ci, buffer, sizeof(buffer));
    if (status != 0)
        return status;

#ifndef NETSNMP_DISABLE_MD5
    if (strcasecmp("MD5", buffer) == 0) {
        hd->auth_protocol = usmHMACMD5AuthProtocol;
        hd->auth_protocol_len = sizeof(usmHMACMD5AuthProtocol) / sizeof(oid);
        PLUGIN_DEBUG("host = %s; host->auth_protocol = %s;", hd->name, "MD5");
        return 0;
    }
#endif

#ifdef NETSNMP_USMAUTH_HMACSHA1
    if (strcasecmp("SHA", buffer) == 0) {
        hd->auth_protocol = usmHMACSHA1AuthProtocol;
        hd->auth_protocol_len = sizeof(usmHMACSHA1AuthProtocol) / sizeof(oid);
        PLUGIN_DEBUG("host = %s; host->auth_protocol = %s;", hd->name, "SHA");
        return 0;
    }
#endif

#ifdef NETSNMP_USMAUTH_HMAC128SHA224
    if (strcasecmp("SHA224", buffer) == 0) {
        hd->auth_protocol = usmHMAC128SHA224AuthProtocol;
        hd->auth_protocol_len = sizeof(usmHMAC128SHA224AuthProtocol) / sizeof(oid);
        PLUGIN_DEBUG("host = %s; host->auth_protocol = %s;", hd->name, "SHA224");
        return 0;
    }
#endif

#ifdef NETSNMP_USMAUTH_HMAC192SHA256
  if (strcasecmp("SHA256", buffer) == 0) {
        hd->auth_protocol = usmHMAC192SHA256AuthProtocol;
        hd->auth_protocol_len = sizeof(usmHMAC192SHA256AuthProtocol) / sizeof(oid);
        PLUGIN_DEBUG("host = %s; host->auth_protocol = %s;", hd->name, "SHA256");
        return 0;
    }
#endif

#ifdef NETSNMP_USMAUTH_HMAC256SHA384
    if (strcasecmp("SHA384", buffer) == 0) {
        hd->auth_protocol = usmHMAC256SHA384AuthProtocol;
        hd->auth_protocol_len = sizeof(usmHMAC256SHA384AuthProtocol) / sizeof(oid);
        PLUGIN_DEBUG("host = %s; host->auth_protocol = %s;", hd->name, "SHA384");
        return 0;
    }
#endif

#ifdef NETSNMP_USMAUTH_HMAC384SHA512
    if (strcasecmp("SHA512", buffer) == 0) {
        hd->auth_protocol = usmHMAC384SHA512AuthProtocol;
        hd->auth_protocol_len = sizeof(usmHMAC384SHA512AuthProtocol) / sizeof(oid);
        PLUGIN_DEBUG("host = %s; host->auth_protocol = %s;", hd->name, "SHA512");
        return 0;
    }
#endif

    const char *auth_protocol =  ":"
#ifdef NETSNMP_USMAUTH_HMACMD5
            " MD5"
#endif
#ifdef NETSNMP_USMAUTH_HMACSHA1
            " SHA"
#endif
#ifdef NETSNMP_USMAUTH_HMAC128SHA224
            " SHA224"
#endif
#ifdef NETSNMP_USMAUTH_HMAC192SHA256
            " SHA256"
#endif
#ifdef NETSNMP_USMAUTH_HMAC256SHA384
            " SHA384"
#endif
#ifdef NETSNMP_USMAUTH_HMAC384SHA512
            " SHA512"
#endif
    ;

    PLUGIN_WARNING("The 'AuthProtocol' config option must be%s", auth_protocol);
    return -1;
}

static int csnmp_config_add_host_priv_protocol(host_definition_t *hd, config_item_t *ci)
{
    char buffer[4];
    int status;

    status = cf_util_get_string_buffer(ci, buffer, sizeof(buffer));
    if (status != 0)
        return status;

    if (strcasecmp("AES", buffer) == 0) {
        hd->priv_protocol = usmAESPrivProtocol;
        hd->priv_protocol_len = sizeof(usmAESPrivProtocol) / sizeof(oid);
    } else if (strcasecmp("DES", buffer) == 0) {
#ifndef NETSNMP_DISABLE_DES
        hd->priv_protocol = usmDESPrivProtocol;
        hd->priv_protocol_len = sizeof(usmDESPrivProtocol) / sizeof(oid);
#else
        PLUGIN_WARNING("The 'priv-protocol' config DES is not supported.");
        return -1;
#endif
    } else {
        PLUGIN_WARNING("The 'priv-protocol' config option must be 'AES' or 'DES'.");
        return -1;
    }

    PLUGIN_DEBUG("host = %s; host->priv_protocol = %s;", hd->name,
                 hd->priv_protocol == usmAESPrivProtocol ? "AES" : "DES");

    return 0;
}

static int csnmp_config_add_host_security_level(host_definition_t *hd, config_item_t *ci)
{
    char buffer[16];
    int status;

    status = cf_util_get_string_buffer(ci, buffer, sizeof(buffer));
    if (status != 0)
        return status;

    if (strcasecmp("noAuthNoPriv", buffer) == 0)
        hd->security_level = SNMP_SEC_LEVEL_NOAUTH;
    else if (strcasecmp("authNoPriv", buffer) == 0)
        hd->security_level = SNMP_SEC_LEVEL_AUTHNOPRIV;
    else if (strcasecmp("authPriv", buffer) == 0)
        hd->security_level = SNMP_SEC_LEVEL_AUTHPRIV;
    else {
        PLUGIN_WARNING("The 'security-level' config option must be "
                        "'noAuthNoPriv', 'authNoPriv', or 'authPriv'.");
        return -1;
    }

    PLUGIN_DEBUG("host = %s; host->security_level = %d;", hd->name, hd->security_level);

    return 0;
}

static int csnmp_config_add_host(config_item_t *ci)
{
    host_definition_t *hd;
    int status = 0;

    /* Registration stuff. */
    cdtime_t interval = 0;

    hd = calloc(1, sizeof(*hd));
    if (hd == NULL)
        return -1;
    hd->version = 2;
    C_COMPLAIN_INIT(&hd->complaint);

    status = cf_util_get_string(ci, &hd->name);
    if (status != 0) {
        free(hd);
        return status;
    }

    hd->sess_handle = NULL;

    /* These mean that we have not set a timeout or retry value */
    hd->timeout = 0;
    hd->retries = -1;
    hd->bulk_size = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *option = ci->children + i;

        if (strcasecmp("address", option->key) == 0) {
            status = cf_util_get_string(option, &hd->address);
        } else if (strcasecmp("community", option->key) == 0) {
            status = cf_util_get_string(option, &hd->community);
        } else if (strcasecmp("version", option->key) == 0) {
            status = csnmp_config_add_host_version(hd, option);
        } else if (strcasecmp("timeout", option->key) == 0) {
            status = cf_util_get_cdtime(option, &hd->timeout);
        } else if (strcasecmp("retries", option->key) == 0) {
            status = cf_util_get_int(option, &hd->retries);
        } else if (strcasecmp("collect", option->key) == 0) {
            status = csnmp_config_add_host_collect(hd, option);
        } else if (strcasecmp("interval", option->key) == 0) {
            status = cf_util_get_cdtime(option, &interval);
        } else if (strcasecmp("username", option->key) == 0) {
            status = cf_util_get_string(option, &hd->username);
        } else if (strcasecmp("auth-protocol", option->key) == 0) {
            status = csnmp_config_add_host_auth_protocol(hd, option);
        } else if (strcasecmp("privacy-protocol", option->key) == 0) {
            status = csnmp_config_add_host_priv_protocol(hd, option);
        } else if (strcasecmp("auth-passphrase", option->key) == 0) {
            status = cf_util_get_string(option, &hd->auth_passphrase);
        } else if (strcasecmp("privacy-passphrase", option->key) == 0) {
            status = cf_util_get_string(option, &hd->priv_passphrase);
        } else if (strcasecmp("security-level", option->key) == 0) {
            status = csnmp_config_add_host_security_level(hd, option);
        } else if (strcasecmp("local-cert", option->key) == 0) {
            status = cf_util_get_string(option, &hd->local_cert);
        } else if (strcasecmp("peer-cert", option->key) == 0) {
            status = cf_util_get_string(option, &hd->peer_cert);
        } else if (strcasecmp("peer-hostname", option->key) == 0) {
            status = cf_util_get_string(option, &hd->peer_hostname);
        } else if (strcasecmp("trust-cert", option->key) == 0) {
            status = cf_util_get_string(option, &hd->trust_cert);
        } else if (strcasecmp("context", option->key) == 0) {
            status = cf_util_get_string(option, &hd->context);
        } else if (strcasecmp("bulk-size", option->key) == 0) {
            status = cf_util_get_int(option, &hd->bulk_size);
        } else if (strcasecmp("metric-prefix", option->key) == 0) {
            status = cf_util_get_string(option, &hd->metric_prefix);
        } else if (strcasecmp("label", option->key) == 0) {
            status = cf_util_get_label(option, &hd->labels);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          option->key, cf_get_file(option), cf_get_lineno(option));
            status = -1;
        }

        if (status != 0)
            break;
    }

    while (status == 0) {
        if (hd->address == NULL) {
            PLUGIN_WARNING("'address' not given for host '%s'", hd->name);
            status = -1;
            break;
        }
        if (hd->community == NULL && hd->version < 3) {
            PLUGIN_WARNING("'community' not given for host '%s'", hd->name);
            status = -1;
            break;
        }
        if (hd->bulk_size > 0 && hd->version < 2) {
            PLUGIN_WARNING("Bulk transfers is only available for SNMP v2 and "
                           "later, host '%s' is configured as version '%d'",
                           hd->name, hd->version);
        }
        if (hd->version == 3) {
            if (hd->local_cert != NULL) {
                if (hd->peer_cert == NULL && hd->trust_cert == NULL) {
                    PLUGIN_WARNING("'local-cert' present but neither 'peer-cert'"
                                   " nor 'trust-cert' present for host `%s'", hd->name);
                    status = -1;
                    break;
                }
            } else {
                if (hd->username == NULL) {
                    PLUGIN_WARNING("'Username' not given for host '%s'", hd->name);
                    status = -1;
                    break;
                }

                if (hd->security_level == 0) {
                    PLUGIN_WARNING("'security-level' not given for host '%s'", hd->name);
                    status = -1;
                    break;
                }
                if ((hd->security_level == SNMP_SEC_LEVEL_AUTHNOPRIV) ||
                    (hd->security_level == SNMP_SEC_LEVEL_AUTHPRIV)) {
                    if (hd->auth_protocol == NULL) {
                        PLUGIN_WARNING("'auth-protocol' not given for host '%s'", hd->name);
                        status = -1;
                        break;
                    }
                    if (hd->auth_passphrase == NULL) {
                        PLUGIN_WARNING("'auth-passphrase' not given for host '%s'", hd->name);
                        status = -1;
                        break;
                    }
                }
                if (hd->security_level == SNMP_SEC_LEVEL_AUTHPRIV) {
                    if (hd->priv_protocol == NULL) {
                        PLUGIN_WARNING("'privacy-protocol' not given for host '%s'", hd->name);
                        status = -1;
                        break;
                    }
                    if (hd->priv_passphrase == NULL) {
                        PLUGIN_WARNING("'privacy-passphrase' not given for host '%s'", hd->name);
                        status = -1;
                        break;
                    }
                }
            }
        }

        break;
    }

    if (status != 0) {
        csnmp_host_definition_destroy(hd);
        return -1;
    }

    PLUGIN_DEBUG("hd = { name = %s, address = %s, community = %s, version = %i }",
                 hd->name, hd->address, hd->community, hd->version);

    return plugin_register_complex_read("snmp", hd->name, csnmp_read_host, interval,
                            &(user_data_t){.data=hd, .free_func=csnmp_host_definition_destroy});
}

static int csnmp_init(void)
{
    static int have_init;

    if (have_init == 0)
        init_snmp(PACKAGE_NAME);
    have_init = 1;

    return 0;
}

static int csnmp_config(config_item_t *ci)
{
    int status = 0;

    csnmp_init();

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("data", child->key) == 0) {
            status = csnmp_config_add_data(child);
        } else if (strcasecmp("host", child->key) == 0) {
            status = csnmp_config_add_host(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int csnmp_shutdown(void)
{
    data_definition_t *data_this;
    data_definition_t *data_next;

    data_this = data_head;
    data_head = NULL;
    while (data_this != NULL) {
        data_next = data_this->next;
        csnmp_data_definition_destroy(data_this);
        data_this = data_next;
    }

    snmp_shutdown(PACKAGE_NAME);

    return 0;
}

void module_register(void)
{
    plugin_register_config("snmp", csnmp_config);
    plugin_register_init("snmp", csnmp_init);
    plugin_register_shutdown("snmp", csnmp_shutdown);
}
