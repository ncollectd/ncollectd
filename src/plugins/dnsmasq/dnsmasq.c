// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>

static const char *dnsmsq_leases_file = "/var/lib/misc/dnsmasq.leases";

enum {
    FAM_DNSMASQ_UP,
    FAM_DNSMASQ_CACHESIZE,
    FAM_DNSMASQ_INSERTIONS,
    FAM_DNSMASQ_EVICTION,
    FAM_DNSMASQ_MISSES,
    FAM_DNSMASQ_HITS,
    FAM_DNSMASQ_AUTH,
    FAM_DNSMASQ_SERVERS_QUERIES,
    FAM_DNSMASQ_SERVERS_QUERIES_FAILED,
    FAM_DNSMASQ_MAX,
};

static metric_family_t fams[FAM_DNSMASQ_MAX] = {
    [FAM_DNSMASQ_UP] = {
        .name = "dnsmasq_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the dnsmasq server be reached.",
    },
    [FAM_DNSMASQ_CACHESIZE] = {
        .name = "dnsmasq_cachesize",
        .type = METRIC_TYPE_GAUGE,
        .help = "Configured size of the DNS cache.",
    },
    [FAM_DNSMASQ_INSERTIONS] = {
        .name = "dnsmasq_insertions",
        .type = METRIC_TYPE_COUNTER,
        .help = "DNS cache insertions.",
    },
    [FAM_DNSMASQ_EVICTION] = {
        .name = "dnsmasq_evictions",
        .type = METRIC_TYPE_COUNTER,
        .help = "DNS cache exictions: numbers of entries which replaced an unexpired cache entry.",
    },
    [FAM_DNSMASQ_MISSES] = {
        .name = "dnsmasq_misses",
        .type = METRIC_TYPE_COUNTER,
        .help = "DNS cache misses: queries which had to be forwarded.",
    },
    [FAM_DNSMASQ_HITS] = {
        .name = "dnsmasq_hits",
        .type = METRIC_TYPE_COUNTER,
        .help = "DNS queries answered locally (cache hits).",
    },
    [FAM_DNSMASQ_AUTH] = {
        .name = "dnsmasq_auth",
        .type = METRIC_TYPE_COUNTER,
        .help = "DNS queries for authoritative zones.",
    },
    [FAM_DNSMASQ_SERVERS_QUERIES] = {
        .name = "dnsmasq_servers_queries",
        .type = METRIC_TYPE_COUNTER,
        .help = "DNS queries on upstream server.",
    },
    [FAM_DNSMASQ_SERVERS_QUERIES_FAILED] = {
        .name = "dnsmasq_servers_queries_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "DNS queries failed on upstream server.",
    },
};

typedef struct {
    char *name;
    char *host;
    int port;
    char *leases;
    label_set_t labels;
    struct timeval timeout;
    struct sockaddr_in dst;
    int sd;
    metric_family_t fams[FAM_DNSMASQ_MAX];
} dnsmasq_t;

#define DNS_OPCODE_QUERY   0
#define DNS_RCODE_NOERROR  0
#define DNS_CLASS_CHAOS    3
#define DNS_TYPE_TXT       16

#define DNS_NAME_MAX_SIZE  255

typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed)) dns_header_t;

typedef struct {
    uint16_t qtype;
    uint16_t qclass;
} __attribute__((packed)) dns_query_t;

typedef struct {
    uint16_t name;
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t rdlength;
} __attribute__((packed)) dns_answer_t;

static int dns_split_txt (uint8_t *buffer, size_t size, char **fields, size_t nfields)
{
    if (size == 0)
        return -1;

    size_t n = 0;
    uint8_t *ptr = buffer;
    uint8_t len = *(ptr++);
    while(true) {
        if (((ptr - buffer) + 1 ) > (long int)size)
            return n;
        if (len == 0)
            return n;
        if (((ptr - buffer) + len) > (long int)size)
            return n;
        fields[n++] = (char *)ptr;
        ptr += len;
        len = *ptr;
        *(ptr++) = '\0';
        if (n >= nfields)
            return n;
    }

    return n;
}

static int dns_response(uint8_t *buffer, size_t size)
{
    if (sizeof(dns_header_t) > size)
        return -1;

    dns_header_t *hdr = (dns_header_t*)buffer;
    if (((ntohs(hdr->flags) & 0xf) != 0) || (ntohs(hdr->qdcount) != 1))
        return -1;

    uint8_t *ptr = buffer + sizeof(dns_header_t);
    while (true) {
        if (((ptr - buffer) + 1 ) > (long int)size)
            return -1;
        uint8_t len = *(ptr++);
        if (((ptr - buffer) + len) > (long int)size)
            return -1;
        if (len == 0)
            break;
        ptr += len;
    }

    if (((ptr - buffer) + sizeof(dns_query_t) + sizeof(dns_answer_t))  > size)
        return -1;
    ptr += sizeof(dns_query_t);
    dns_answer_t *answer = (dns_answer_t *)ptr;

    if ((ntohs(answer->type) != DNS_TYPE_TXT) || (ntohs(answer->class) != DNS_CLASS_CHAOS))
        return -1;

    ptr += sizeof(dns_answer_t);
    int rdlength = ntohs(answer->rdlength);
    if ((rdlength == 0) || ((ptr - buffer) + rdlength) != (long int)size)
        return -1;

    memmove(buffer, ptr, rdlength);
    return rdlength;
}

static int dns_bind_query(uint8_t *buffer, size_t size, uint64_t id, char *txt_query)
{
    size_t txt_len = strlen(txt_query);
    size_t bind_len = strlen("bind");
    size_t hdr_size = sizeof(dns_header_t) + txt_len + bind_len + 3 + sizeof(dns_query_t);
    if (hdr_size > size)
        return -1;

    dns_header_t *hdr = (dns_header_t*)buffer;
    memset(hdr, 0, sizeof(*hdr));
    hdr->id = htons(id);
    hdr->flags = htons (0x0100); /* QR=0, RD=1 */
    hdr->qdcount = htons(1);

    uint8_t *ptr = buffer + sizeof(*hdr);

    *(ptr++) = (uint8_t) txt_len;
    memcpy(ptr, txt_query, txt_len);
    ptr += txt_len;
    *(ptr++) = (uint8_t) bind_len;
    memcpy(ptr, "bind", bind_len);
    ptr += bind_len;
    *(ptr++) = 0;

    dns_query_t *query = (dns_query_t *)ptr;
    query->qclass = htons(DNS_CLASS_CHAOS);
    query->qtype    = htons(DNS_TYPE_TXT);

    return hdr_size;
}

int dns_query(dnsmasq_t *dns, char *buffer, size_t buffer_size, int id,
              char *txt_query, char **fields, size_t nfields)
{
    int size = dns_bind_query((uint8_t *)buffer, buffer_size, id, txt_query);
    if (size < 0)
        return 0;

    socklen_t addrlen = sizeof(dns->dst);
    int send_size = sendto(dns->sd, buffer, size, 0, (struct sockaddr *)&dns->dst, addrlen);
    if (size != send_size)
        return 0;

    addrlen = 0;
    int recv_size = recvfrom(dns->sd, buffer, buffer_size, 0,
                             (struct sockaddr *)&dns->dst, &addrlen);
    if (recv_size < 0)
     return 0;

    size = dns_response((uint8_t *)buffer, recv_size);
    if (size < 0)
        return 0;

    return dns_split_txt((uint8_t *)buffer, size, fields, nfields);
}

static int dnsmasq_leases(char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
        return -1;

    int lines = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        lines++;
    }

    fclose(fp);
    return lines;
}

// dig +short chaos txt cachesize.bind insertions.bind evictions.bind misses.bind hits.bind auth.bind servers.bind
typedef struct {
    char *query;
    int fam;
} dns_query_fam_t;

static dns_query_fam_t dns_query_fam[] = {
    { "cachesize" , FAM_DNSMASQ_CACHESIZE  },
    { "insertions", FAM_DNSMASQ_INSERTIONS },
    { "evictions" , FAM_DNSMASQ_EVICTION   },
    { "misses"    , FAM_DNSMASQ_MISSES     },
    { "hits"      , FAM_DNSMASQ_HITS       },
    { "auth"      , FAM_DNSMASQ_AUTH       },
};

static int dnsmasq_read(user_data_t *user_data)
{
    dnsmasq_t *dns = user_data->data;

    if (0)
        dnsmasq_leases(dns->leases);

    char buffer[512];
    char *fields[32];

    uint16_t id = 4561;
    for (size_t i = 0; i < STATIC_ARRAY_SIZE(dns_query_fam); i++) {
        ssize_t size = dns_query(dns, buffer, sizeof(buffer),
                                 id, dns_query_fam[i].query, fields, STATIC_ARRAY_SIZE(fields));
        id++;
        if (size <= 0)
            continue;

        if (fams[dns_query_fam[i].fam].type == METRIC_TYPE_COUNTER) {
            metric_family_append(&fams[dns_query_fam[i].fam],
                                 VALUE_COUNTER(atoll(fields[0])), &dns->labels, NULL);
        } else if (fams[dns_query_fam[i].fam].type == METRIC_TYPE_GAUGE) {
            metric_family_append(&fams[dns_query_fam[i].fam],
                                 VALUE_GAUGE(atoll(fields[0])), &dns->labels, NULL);
        }
    }

    ssize_t size = dns_query(dns, buffer, sizeof(buffer),
                             id, "servers", fields, STATIC_ARRAY_SIZE(fields));
    char *sfields[3];
    for (ssize_t i = 0; i < size; i++) {
        int numfields = strsplit(fields[i], sfields, STATIC_ARRAY_SIZE(sfields));
        if (numfields != 3)
            continue;
        metric_family_append(&fams[FAM_DNSMASQ_SERVERS_QUERIES],
                             VALUE_COUNTER(atoll(sfields[1])), &dns->labels,
                             &(label_pair_const_t){.name="server", .value=sfields[0]}, NULL);
        metric_family_append(&fams[FAM_DNSMASQ_SERVERS_QUERIES_FAILED],
                             VALUE_COUNTER(atoll(sfields[2])), &dns->labels,
                             &(label_pair_const_t){.name="server", .value=sfields[0]}, NULL);
    }

    plugin_dispatch_metric_family_array(fams, FAM_DNSMASQ_MAX, 0);
    return 0;
}

static void dnsmasq_free(void *data)
{
    dnsmasq_t *dnsi = data;
    if (dnsi == NULL)
        return;

    free(dnsi->name);
    free(dnsi->host);
    label_set_reset(&dnsi->labels);
    if (dnsi->sd >= 0)
        close(dnsi->sd);

    free(dnsi);
}

static int dnsmasq_config_instance(config_item_t *ci)
{
    dnsmasq_t *dnsi = calloc(1, sizeof(*dnsi));
    if (dnsi == NULL) {
        PLUGIN_ERROR("calloc failed adding instance.");
        return -1;
    }
    dnsi->sd = -1;
    memcpy(dnsi->fams, fams, FAM_DNSMASQ_MAX * sizeof(*fams));

    dnsi->port = 43;
    dnsi->host = strdup("127.0.0.1");
    if (dnsi->host == NULL) {
        PLUGIN_ERROR("strdup failed adding instance.");
        dnsmasq_free(dnsi);
        return -1;
    }

    int status = cf_util_get_string(ci, &dnsi->name);
    if (status != 0) {
        dnsmasq_free(dnsi);
        return status;
    }

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *option = ci->children + i;

        if (strcasecmp("host", option->key) == 0) {
            status = cf_util_get_string(option, &dnsi->host);
        } else if (strcasecmp("port", option->key) == 0) {
            status = cf_util_get_port_number(option, &dnsi->port);
        } else if (strcasecmp("timeout", option->key) == 0) {
            cdtime_t timeout;
            status = cf_util_get_cdtime(option, &timeout);
            if (status == 0)
                dnsi->timeout = CDTIME_T_TO_TIMEVAL(timeout);
        } else if (strcasecmp("leases", option->key) == 0) {
            status = cf_util_get_string(option, &dnsi->leases);
        } else if (strcasecmp("interval", option->key) == 0) {
            status = cf_util_get_cdtime(option, &interval);
        }  else if (strcasecmp("label", option->key) == 0) {
            status = cf_util_get_label(option, &dnsi->labels);
        } else {
            PLUGIN_ERROR("Option `%s' not allowed inside a 'instance' block. "
                         "I'll ignore this option.", option->key);
            status = 1;
        }

        if (status != 0)
            break;
    }


    if (status != 0) {
        dnsmasq_free(dnsi);
        return status;
    }

    if (dnsi->leases == NULL)
        dnsi->leases = strdup(dnsmsq_leases_file);

    dnsi->sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (dnsi->sd < 0) {
        dnsmasq_free(dnsi);
        return -1;
    }

    dnsi->dst.sin_family = AF_INET;
    dnsi->dst.sin_port = htons(dnsi->port);
    dnsi->dst.sin_addr.s_addr = inet_addr(dnsi->host);

    label_set_add(&dnsi->labels, true, "instance", dnsi->name);

    return plugin_register_complex_read("dnsmasq", dnsi->name, dnsmasq_read, interval,
                                        &(user_data_t){.data = dnsi, .free_func = dnsmasq_free});
}

static int dnsmasq_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = dnsmasq_config_instance(child);
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

void module_register(void)
{
    plugin_register_config("dnsmasq", dnsmasq_config);
}
