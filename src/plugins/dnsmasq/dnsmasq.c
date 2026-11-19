// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/random.h"
#include "libutils/socket.h"

#include <netdb.h>
#include <netinet/in.h>

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
    FAM_DNSMASQ_DHCP_LEASES,
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
    [FAM_DNSMASQ_DHCP_LEASES] = {
        .name = "dnsmasq_dhcp_leases",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of DHCP leases present in the leases file.",
    },
};

typedef struct {
    char *name;
    char *host;
    int port;
    char *leases;
    label_set_t labels;
    plugin_filter_t *filter;
    struct timeval timeout;
    int sd;
    metric_family_t fams[FAM_DNSMASQ_MAX];
} dnsmasq_t;

#define DNS_OPCODE_QUERY   0
#define DNS_RCODE_NOERROR  0
#define DNS_CLASS_CHAOS    3
#define DNS_TYPE_TXT       16
#define DNS_TYPE_OPT       41

#define DNS_EDNS0_SIZE     4096
#define DNS_OPT_RR_SIZE    11

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

/* Split the TXT rdata in buffer into NUL terminated strings.
 * The NUL terminator of the last string is written at buffer[size], so the
 * caller must provide a buffer of at least size + 1 bytes. */
static int dns_split_txt (uint8_t *buffer, size_t size, char **fields, size_t nfields)
{
    if (size <= 1)
        return -1;

    size_t n = 0;
    uint8_t *ptr = buffer;
    uint8_t len = *(ptr++);
    while (true) {
        if (len == 0)
            return n;
        if (((ptr - buffer) + len) > (long int)size)
            return n;
        fields[n++] = (char *)ptr;
        ptr += len;
        len = *ptr;
        if (((ptr - buffer) + 1 ) > (long int)size)
            return n;
        *(ptr++) = '\0';
        if (n >= nfields)
            return n;
    }

    return n;
}

static int dns_response(uint8_t *buffer, size_t size, uint16_t id,
                        const uint8_t *question, size_t question_size)
{
    if (sizeof(dns_header_t) > size)
        return -1;

    dns_header_t *hdr = (dns_header_t*)buffer;
    if (ntohs(hdr->id) != id)
        return -1;
    /* Must be a response (QR=1) with RCODE=NOERROR */
    if (((ntohs(hdr->flags) & 0x8000) == 0) || ((ntohs(hdr->flags) & 0xf) != 0) ||
        (ntohs(hdr->qdcount) != 1) || (ntohs(hdr->ancount) < 1))
        return -1;

    /* The question section must match the one sent */
    if ((sizeof(dns_header_t) + question_size) > size)
        return -1;
    if (memcmp(buffer + sizeof(dns_header_t), question, question_size) != 0)
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
    /* The answer may be followed by the OPT RR (EDNS0) in the additional section. */
    if ((rdlength == 0) || ((ptr - buffer) + rdlength) > (long int)size)
        return -1;

    memmove(buffer, ptr, rdlength);
    return rdlength;
}

static int dns_bind_query(uint8_t *buffer, size_t size, uint64_t id, char *txt_query)
{
    size_t txt_len = strlen(txt_query);
    size_t bind_len = strlen("bind");
    size_t hdr_size = sizeof(dns_header_t) + txt_len + bind_len + 3 + sizeof(dns_query_t)
                      + DNS_OPT_RR_SIZE;
    if (hdr_size > size)
        return -1;

    dns_header_t *hdr = (dns_header_t*)buffer;
    memset(hdr, 0, sizeof(*hdr));
    hdr->id = htons(id);
    hdr->flags = htons (0x0100); /* QR=0, RD=1 */
    hdr->qdcount = htons(1);
    hdr->arcount = htons(1);

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
    query->qtype = htons(DNS_TYPE_TXT);
    ptr += sizeof(dns_query_t);

    /* OPT RR (EDNS0) to announce a larger UDP payload size and avoid truncation */
    uint16_t opt_type = htons(DNS_TYPE_OPT);
    uint16_t opt_class = htons(DNS_EDNS0_SIZE);
    *(ptr++) = 0; /* root name */
    memcpy(ptr, &opt_type, sizeof(opt_type));
    ptr += sizeof(opt_type);
    memcpy(ptr, &opt_class, sizeof(opt_class));
    ptr += sizeof(opt_class);
    memset(ptr, 0, 4 + 2); /* ttl (extended rcode and flags) and rdlength */

    return hdr_size;
}

static int dns_query(dnsmasq_t *dns, char *buffer, size_t buffer_size, int16_t id,
                     char *txt_query, char **fields, size_t nfields)
{
    int size = dns_bind_query((uint8_t *)buffer, buffer_size, id, txt_query);
    if (size < 0)
        return 0;

    uint8_t question[DNS_NAME_MAX_SIZE + sizeof(dns_query_t)];
    size_t question_size = size - sizeof(dns_header_t);
    if (question_size > sizeof(question))
        return 0;
    memcpy(question, buffer + sizeof(dns_header_t), question_size);

    int send_size = send(dns->sd, buffer, size, 0);
    if (size != send_size)
        return 0;

    int recv_size = recv(dns->sd, buffer, buffer_size, 0);
    if (recv_size < 0)
        return 0;

    size = dns_response((uint8_t *)buffer, recv_size, id, question, question_size);
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
    char buffer[4096];
    size_t len;
    while ((len = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        for (size_t i = 0; i < len; i++) {
            if (buffer[i] == '\n')
                lines++;
        }
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

    if (dns->leases != NULL) {
        int leases = dnsmasq_leases(dns->leases);
        if (leases >= 0)
            metric_family_append(&dns->fams[FAM_DNSMASQ_DHCP_LEASES], VALUE_GAUGE(leases),
                                 &dns->labels, NULL);
    }

    char buffer[DNS_EDNS0_SIZE + 1];
    char *fields[256];
    int failed = 0;

    for (size_t i = 0; i < STATIC_ARRAY_SIZE(dns_query_fam); i++) {
        ssize_t size = dns_query(dns, buffer, sizeof(buffer), (uint16_t)cdrand_u(),
                                 dns_query_fam[i].query, fields, STATIC_ARRAY_SIZE(fields));
        if (size < 0)
            failed++;
        if (size <= 0)
            continue;

        uint64_t value = 0;
        if (parse_uinteger(fields[0], &value) != 0)
            continue;

        if (dns->fams[dns_query_fam[i].fam].type == METRIC_TYPE_COUNTER) {
            metric_family_append(&dns->fams[dns_query_fam[i].fam],
                                 VALUE_COUNTER(value), &dns->labels, NULL);
        } else if (dns->fams[dns_query_fam[i].fam].type == METRIC_TYPE_GAUGE) {
            metric_family_append(&dns->fams[dns_query_fam[i].fam],
                                 VALUE_GAUGE(value), &dns->labels, NULL);
        }
    }

    ssize_t size = dns_query(dns, buffer, sizeof(buffer), (uint16_t)cdrand_u(),
                             "servers", fields, STATIC_ARRAY_SIZE(fields));
    if (size < 0)
        failed++;

    char *sfields[3];
    for (ssize_t i = 0; i < size; i++) {
        int numfields = strsplit(fields[i], sfields, STATIC_ARRAY_SIZE(sfields));
        if (numfields != 3)
            continue;
        uint64_t queries = 0;
        uint64_t queries_failed = 0;
        if ((parse_uinteger(sfields[1], &queries) != 0) ||
            (parse_uinteger(sfields[2], &queries_failed) != 0))
            continue;
        metric_family_append(&dns->fams[FAM_DNSMASQ_SERVERS_QUERIES],
                             VALUE_COUNTER(queries), &dns->labels,
                             &LABEL_PAIR_CONST("server", sfields[0]), NULL);
        metric_family_append(&dns->fams[FAM_DNSMASQ_SERVERS_QUERIES_FAILED],
                             VALUE_COUNTER(queries_failed), &dns->labels,
                             &LABEL_PAIR_CONST("server", sfields[0]), NULL);
    }

    metric_family_append(&dns->fams[FAM_DNSMASQ_UP], VALUE_GAUGE(failed == 0 ? 1 : 0),
                         &dns->labels, NULL);

    plugin_dispatch_metric_family_array_filtered(dns->fams, FAM_DNSMASQ_MAX, dns->filter, 0);

    return 0;
}

static void dnsmasq_free(void *data)
{
    dnsmasq_t *dnsi = data;
    if (dnsi == NULL)
        return;

    free(dnsi->name);
    free(dnsi->host);
    free(dnsi->leases);
    label_set_reset(&dnsi->labels);
    plugin_filter_free(dnsi->filter);

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

    dnsi->port = 53;
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
        } else if (strcasecmp("filter", option->key) == 0) {
            status = plugin_filter_configure(option, &dnsi->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed inside a 'instance' block.",
                         option->key, cf_get_file(option), cf_get_lineno(option));
            status = 1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        dnsmasq_free(dnsi);
        return status;
    }

    if (dnsi->leases == NULL) {
        dnsi->leases = strdup(dnsmsq_leases_file);
        if (dnsi->leases == NULL) {
            PLUGIN_ERROR("strdup failed adding instance.");
            dnsmasq_free(dnsi);
            return -1;
        }
    }

    dnsi->sd = socket_connect_udp(dnsi->host, dnsi->port, 0);
    if (dnsi->sd < 0) {
        PLUGIN_ERROR("Failed to connect to '%s' port %d in instance '%s'.",
                     dnsi->host, dnsi->port, dnsi->name);
        dnsmasq_free(dnsi);
        return -1;
    }

    if ((dnsi->timeout.tv_sec == 0) && (dnsi->timeout.tv_usec == 0))
        dnsi->timeout.tv_sec = 1;

    if (setsockopt(dnsi->sd, SOL_SOCKET, SO_RCVTIMEO, &dnsi->timeout, sizeof(dnsi->timeout)) != 0) {
        PLUGIN_ERROR("setsockopt(SO_RCVTIMEO) failed: %s", STRERRNO);
        dnsmasq_free(dnsi);
        return -1;
    }

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
