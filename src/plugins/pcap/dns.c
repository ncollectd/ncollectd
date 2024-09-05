// SPDX-License-Identifier: GPL-2.0-only OR BSD-3-Clause
// SPDX-FileCopyrightText: Copyright (C) 2002 The Measurement Factory, Inc.
// SPDX-FileCopyrightText: Copyright (C) 2006-2011 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009 Mirko Buffoni
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Mirko Buffoni <briareos at eswat.org>
// SPDX-FileContributor: The Measurement Factory, Inc. <http://www.measurement-factory.com/>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include "plugins/pcap/dns.h"

#define DNS_MSG_HDR_SZ 12
#define T_MAX 65536
#define MAX_QNAME_SZ 512

typedef struct {
    uint16_t id;
    unsigned int qr : 1;
    unsigned int opcode : 4;
    unsigned int aa : 1;
    unsigned int tc : 1;
    unsigned int rd : 1;
    unsigned int ra : 1;
    unsigned int z : 1;
    unsigned int ad : 1;
    unsigned int cd : 1;
    unsigned int rcode : 4;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
    uint16_t qtype;
    uint16_t qclass;
    char qname[MAX_QNAME_SZ];
    uint16_t length;
} rfc1035_header_t;

static metric_family_t fams_dns[FAM_PCAP_DNS_MAX] = {
    [FAM_PCAP_DNS_QUERIES] = {
        .name = "pcap_dns_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PCAP_DNS_RESPONSES] = {
        .name = "pcap_dns_responses",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PCAP_DNS_QUERY_TYPES] = {
        .name = "pcap_dns_query_types",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PCAP_DNS_OPERATION_CODES] = {
        .name = "pcap_dns_operation_codes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
    [FAM_PCAP_DNS_RESPONSE_CODES] = {
        .name = "pcap_dns_response_codes",
        .type = METRIC_TYPE_COUNTER,
        .help = NULL,
    },
};

/*
 * flags/features for non-interactive mode
 */

#ifndef T_A6
#define T_A6 38
#endif
#ifndef T_SRV
#define T_SRV 33
#endif

static counter_list_t *counter_list_search(counter_list_t **list, unsigned int key)
{
    counter_list_t *entry;

    for (entry = *list; entry != NULL; entry = entry->next)
        if (entry->key == key)
            break;

    return entry;
}

static counter_list_t *counter_list_create(counter_list_t **list, unsigned int key, unsigned int value)
{
    counter_list_t *entry;

    entry = calloc(1, sizeof(*entry));
    if (entry == NULL)
        return NULL;

    entry->key = key;
    entry->value = value;

    if (*list == NULL) {
        *list = entry;
    } else {
        counter_list_t *last;

        last = *list;
        while (last->next != NULL)
            last = last->next;

        last->next = entry;
    }

    return entry;
}

static void counter_list_add(counter_list_t **list, unsigned int key, unsigned int increment)
{
    counter_list_t *entry;

    entry = counter_list_search(list, key);

    if (entry != NULL) {
        entry->value += increment;
    } else {
        counter_list_create(list, key, increment);
    }
}


static void dns_child_callback(nc_dns_ctx_t *ctx, const rfc1035_header_t *dns)
{
    if (dns->qr == 0) {
        /* This is a query */
        int skip = 0;
#if 0
        if (!select_numeric_qtype) {
            const char *str = qtype_str(dns->qtype);
            if ((str == NULL) || (str[0] == '#'))
                skip = 1;
        }
#endif

        pthread_mutex_lock(&ctx->traffic_mutex);
        ctx->tr_queries += dns->length;
        pthread_mutex_unlock(&ctx->traffic_mutex);

        if (skip == 0) {
            pthread_mutex_lock(&ctx->qtype_mutex);
            counter_list_add(&ctx->qtype_list, dns->qtype, 1);
            pthread_mutex_unlock(&ctx->qtype_mutex);
        }
    } else {
        /* This is a reply */
        pthread_mutex_lock(&ctx->traffic_mutex);
        ctx->tr_responses += dns->length;
        pthread_mutex_unlock(&ctx->traffic_mutex);

        pthread_mutex_lock(&ctx->rcode_mutex);
        counter_list_add(&ctx->rcode_list, dns->rcode, 1);
        pthread_mutex_unlock(&ctx->rcode_mutex);
    }

    /* FIXME: Are queries, replies or both interesting? */
    pthread_mutex_lock(&ctx->opcode_mutex);
    counter_list_add(&ctx->opcode_list, dns->opcode, 1);
    pthread_mutex_unlock(&ctx->opcode_mutex);
}

#define RFC1035_MAXLABELSZ 63

static int rfc1035_name_unpack(const char *buf, size_t sz, off_t *off, char *name, size_t ns)
{
    off_t no = 0;
    unsigned char c;
    size_t len;
    static int loop_detect;

    if (loop_detect > 2)
        return 4; /* compression loop */
    if (ns == 0)
        return 4; /* probably compression loop */

    do {
        if ((*off) >= ((off_t)sz))
            break;
        c = *(buf + (*off));
        if (c > 191) {
            /* blasted compression */
            int rc;
            unsigned short s;
            off_t ptr;
            memcpy(&s, buf + (*off), sizeof(s));
            s = ntohs(s);
            (*off) += sizeof(s);
            /* Sanity check */
            if ((*off) >= ((off_t)sz))
                return 1; /* message too short */
            ptr = s & 0x3FFF;
            /* Make sure the pointer is inside this message */
            if (ptr >= ((off_t)sz))
                return 2; /* bad compression ptr */
            if (ptr < DNS_MSG_HDR_SZ)
                return 2; /* bad compression ptr */
            loop_detect++;
            rc = rfc1035_name_unpack(buf, sz, &ptr, name + no, ns - no);
            loop_detect--;
            return rc;
        } else if (c > RFC1035_MAXLABELSZ) {
            /* The 10 and 01 combinations are reserved for future use. */
            return 3; /* reserved label/compression flags */
        } else {
            (*off)++;
            len = (size_t)c;
            if (len == 0)
                break;
            if (len > (ns - 1))
                len = ns - 1;
            if ((*off) + len > sz)
                return 4; /* message is too short */
            if (no + len + 1 > ns)
                return 5; /* qname would overflow name buffer */
            memcpy(name + no, buf + (*off), len);
            (*off) += len;
            no += len;
            *(name + (no++)) = '.';
        }
    } while (c > 0);

    if (no > 0)
        *(name + no - 1) = '\0';
    /* make sure we didn't allow someone to overflow the name buffer */
    assert(no <= ((off_t)ns));

    return 0;
}

int handle_dns(nc_dns_ctx_t *ctx, const char *buf, int len)
{
    rfc1035_header_t qh;
    uint16_t us;
    off_t offset;
    char *t;
    int status;

    /* The DNS header is 12 bytes long */
    if (len < DNS_MSG_HDR_SZ)
        return 0;

    memcpy(&us, buf + 0, 2);
    qh.id = ntohs(us);

    memcpy(&us, buf + 2, 2);
    us = ntohs(us);
    qh.qr = (us >> 15) & 0x01;
    qh.opcode = (us >> 11) & 0x0F;
    qh.aa = (us >> 10) & 0x01;
    qh.tc = (us >> 9) & 0x01;
    qh.rd = (us >> 8) & 0x01;
    qh.ra = (us >> 7) & 0x01;
    qh.z = (us >> 6) & 0x01;
    qh.ad = (us >> 5) & 0x01;
    qh.cd = (us >> 4) & 0x01;
    qh.rcode = us & 0x0F;

    memcpy(&us, buf + 4, 2);
    qh.qdcount = ntohs(us);

    memcpy(&us, buf + 6, 2);
    qh.ancount = ntohs(us);

    memcpy(&us, buf + 8, 2);
    qh.nscount = ntohs(us);

    memcpy(&us, buf + 10, 2);
    qh.arcount = ntohs(us);

    offset = DNS_MSG_HDR_SZ;
    memset(qh.qname, '\0', MAX_QNAME_SZ);
    status = rfc1035_name_unpack(buf, len, &offset, qh.qname, MAX_QNAME_SZ);
    if (status != 0) {
        PLUGIN_INFO("rfc1035_name_unpack failed with status %i.", status);
        return 0;
    }
    if ('\0' == qh.qname[0])
        sstrncpy(qh.qname, ".", sizeof(qh.qname));
    while ((t = strchr(qh.qname, '\n')))
        *t = ' ';
    while ((t = strchr(qh.qname, '\r')))
        *t = ' ';
    for (t = qh.qname; *t; t++)
        *t = tolower((int)*t);

    memcpy(&us, buf + offset, 2);
    qh.qtype = ntohs(us);
    memcpy(&us, buf + offset + 2, 2);
    qh.qclass = ntohs(us);

    qh.length = (uint16_t)len;

    dns_child_callback(ctx, &qh);

    return 1;
}

static char *qtype_str(int t, char *buf, size_t size)
{
    /*
    Built (with minor cleanup) from glibc-2.29 by
        cat resolv/arpa/nameser.h | grep "ns_t_" | \
        perl -ne '/ns_t_(\S+)\ =\ (\d+)/; print "  case $2:\n        return \"".uc($1)."\";\n";'
    */
    switch (t) {
    case 1:
        return "A";
    case 2:
        return "NS";
    case 3:
        return "MD";
    case 4:
        return "MF";
    case 5:
        return "CNAME";
    case 6:
        return "SOA";
    case 7:
        return "MB";
    case 8:
        return "MG";
    case 9:
        return "MR";
    case 10:
        return "NULL";
    case 11:
        return "WKS";
    case 12:
        return "PTR";
    case 13:
        return "HINFO";
    case 14:
        return "MINFO";
    case 15:
        return "MX";
    case 16:
        return "TXT";
    case 17:
        return "RP";
    case 18:
        return "AFSDB";
    case 19:
        return "X25";
    case 20:
        return "ISDN";
    case 21:
        return "RT";
    case 22:
        return "NSAP";
    case 23:
        return "NSAP-PTR";
    case 24:
        return "SIG";
    case 25:
        return "KEY";
    case 26:
        return "PX";
    case 27:
        return "GPOS";
    case 28:
        return "AAAA";
    case 29:
        return "LOC";
    case 30:
        return "NXT";
    case 31:
        return "EID";
    case 32:
        return "NIMLOC";
    case 33:
        return "SRV";
    case 34:
        return "ATMA";
    case 35:
        return "NAPTR";
    case 36:
        return "KX";
    case 37:
        return "CERT";
    case 38:
        return "A6";
    case 39:
        return "DNAME";
    case 40:
        return "SINK";
    case 41:
        return "OPT";
    case 42:
        return "APL";
    case 43:
        return "DS";
    case 44:
        return "SSHFP";
    case 45:
        return "IPSECKEY";
    case 46:
        return "RRSIG";
    case 47:
        return "NSEC";
    case 48:
        return "DNSKEY";
    case 49:
        return "DHCID";
    case 50:
        return "NSEC3";
    case 51:
        return "NSEC3PARAM";
    case 52:
        return "TLSA";
    case 53:
        return "SMIMEA";
    case 55:
        return "HIP";
    case 56:
        return "NINFO";
    case 57:
        return "RKEY";
    case 58:
        return "TALINK";
    case 59:
        return "CDS";
    case 60:
        return "CDNSKEY";
    case 61:
        return "OPENPGPKEY";
    case 62:
        return "CSYNC";
    case 99:
        return "SPF";
    case 100:
        return "UINFO";
    case 101:
        return "UID";
    case 102:
        return "GID";
    case 103:
        return "UNSPEC";
    case 104:
        return "NID";
    case 105:
        return "L32";
    case 106:
        return "L64";
    case 107:
        return "LP";
    case 108:
        return "EUI48";
    case 109:
        return "EUI64";
    case 249:
        return "TKEY";
    case 250:
        return "TSIG";
    case 251:
        return "IXFR";
    case 252:
        return "AXFR";
    case 253:
        return "MAILB";
    case 254:
        return "MAILA";
    case 255:
        return "ANY";
    case 256:
        return "URI";
    case 257:
        return "CAA";
    case 258:
        return "AVC";
    case 32768:
        return "TA";
    case 32769:
        return "DLV";
    default:
        ssnprintf(buf, size, "#%i", t);
        return buf;
    }
}

static char *opcode_str(int o, char *buf, size_t size)
{
    switch (o) {
    case 0:
        return "Query";
    case 1:
        return "Iquery";
    case 2:
        return "Status";
    case 4:
        return "Notify";
    case 5:
        return "Update";
    default:
        ssnprintf(buf, size, "Opcode%d", o);
        return buf;
    }
}

static char *rcode_str(int rcode, char *buf, size_t size)
{
    /* RFC2136 rcodes */
    /*
    Built (with minor cleanup) from glibc-2.29 by
        cat resolv/arpa/nameser.h | grep "ns_r_" | \
        perl -ne '/ns_r_(\S+)\ =\ (\d+)/; print "  case $2:\n        return \"".uc($1)."\";\n";'

    https://tools.ietf.org/html/rfc2671 assigns EDNS Extended RCODE "16" to "BADVERS"
    https://tools.ietf.org/html/rfc2845 declares 0..15 as DNS RCODE and 16 is BADSIG.
    */
    switch (rcode) {
    case 1:
        return "FORMERR";
    case 2:
        return "SERVFAIL";
    case 3:
        return "NXDOMAIN";
    case 4:
        return "NOTIMPL";
    case 5:
        return "REFUSED";
    case 6:
        return "YXDOMAIN";
    case 7:
        return "YXRRSET";
    case 8:
        return "NXRRSET";
    case 9:
        return "NOTAUTH";
    case 10:
        return "NOTZONE";
    case 11:
        return "MAX";
    case 16:
        return "BADSIG";
    case 17:
        return "BADKEY";
    case 18:
        return "BADTIME";
    default:
        ssnprintf(buf, size, "RCode%i", rcode);
        return buf;
    }
}

int nc_dns_read(nc_dns_ctx_t *ctx, label_set_t *labels)
{
    unsigned int keys[T_MAX];
    unsigned int values[T_MAX];
    int len;

    counter_list_t *ptr;

    int64_t queries = 0;
    int64_t responses = 0;
    pthread_mutex_lock(&ctx->traffic_mutex);
    queries = ctx->tr_queries;
    responses = ctx->tr_responses;
    pthread_mutex_unlock(&ctx->traffic_mutex);

    metric_family_append(&ctx->fams[FAM_PCAP_DNS_QUERIES], VALUE_COUNTER(queries),
                         labels, NULL);
    metric_family_append(&ctx->fams[FAM_PCAP_DNS_RESPONSES], VALUE_COUNTER(responses),
                         labels, NULL);


    pthread_mutex_lock(&ctx->qtype_mutex);
    for (ptr = ctx->qtype_list, len = 0; (ptr != NULL) && (len < T_MAX); ptr = ptr->next, len++) {
        keys[len] = ptr->key;
        values[len] = ptr->value;
    }
    pthread_mutex_unlock(&ctx->qtype_mutex);

    for (int i = 0; i < len; i++) {
        char buf[32];
        char *qtype = qtype_str(keys[i], buf, sizeof(buf));
        metric_family_append(&ctx->fams[FAM_PCAP_DNS_QUERY_TYPES], VALUE_COUNTER(values[i]),
                             labels,
                             &(label_pair_const_t){.name="qtype", .value=qtype}, NULL);
        PLUGIN_DEBUG("qtype = %u; counter = %u;", keys[i], values[i]);
    }

    pthread_mutex_lock(&ctx->opcode_mutex);
    for (ptr = ctx->opcode_list, len = 0; (ptr != NULL) && (len < T_MAX); ptr = ptr->next, len++) {
        keys[len] = ptr->key;
        values[len] = ptr->value;
    }
    pthread_mutex_unlock(&ctx->opcode_mutex);

    for (int i = 0; i < len; i++) {
        char buf[32];
        char *opcode = opcode_str(keys[i], buf, sizeof(buf));
        metric_family_append(&ctx->fams[FAM_PCAP_DNS_OPERATION_CODES], VALUE_COUNTER(values[i]),
                             labels,
                             &(label_pair_const_t){.name="opcode", .value=opcode}, NULL);
        PLUGIN_DEBUG("opcode = %u; counter = %u;", keys[i], values[i]);
    }

    pthread_mutex_lock(&ctx->rcode_mutex);
    for (ptr = ctx->rcode_list, len = 0; (ptr != NULL) && (len < T_MAX); ptr = ptr->next, len++) {
        keys[len] = ptr->key;
        values[len] = ptr->value;
    }
    pthread_mutex_unlock(&ctx->rcode_mutex);

    for (int i = 0; i < len; i++) {
        char buf[32];
        char *rcode = rcode_str(keys[i], buf, sizeof(buf));
        metric_family_append(&ctx->fams[FAM_PCAP_DNS_RESPONSE_CODES], VALUE_COUNTER(values[i]),
                             labels,
                             &(label_pair_const_t){.name="rcode", .value=rcode}, NULL);
        PLUGIN_DEBUG("rcode = %u; counter = %u;", keys[i], values[i]);
    }

    plugin_dispatch_metric_family_array(ctx->fams, FAM_PCAP_DNS_MAX, 0);

    return 0;
}

int nc_dns_init(nc_dns_ctx_t *ctx)
{
    memcpy(ctx->fams, fams_dns, sizeof(ctx->fams[0])*FAM_PCAP_DNS_MAX);

    pthread_mutex_init(&ctx->traffic_mutex, NULL);
    pthread_mutex_init(&ctx->qtype_mutex, NULL);
    pthread_mutex_init(&ctx->opcode_mutex, NULL);
    pthread_mutex_init(&ctx->rcode_mutex, NULL);
    return 0;
}
