// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright 1998 by the Massachusetts Institute of Technology.
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

// Based on adig

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"
#include "libexpr/expr.h"

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif

#include <poll.h>

#include <ares.h>
#include <ares_dns.h>

#define QFIXEDSZ  4
#define HFIXEDSZ  12  /* #/bytes of fixed data in header */
#define RRFIXEDSZ 10  /* #/bytes of fixed data in r record */

#ifndef MAX_IP6_RR
#define MAX_IP6_RR  (16*sizeof(".x.x") + sizeof(".IP6.ARPA") + 1)
#endif

#ifdef HAVE_INET_PTON
 #define ACCEPTED_RETVAL4 1
 #define ACCEPTED_RETVAL6 1
#else
 #define ACCEPTED_RETVAL4 32
 #define ACCEPTED_RETVAL6 128
#endif

enum {
    FAM_DNS_QUERY_TIME_SECONDS,
    FAM_DNS_QUERY_SUCCESS,
    FAM_DNS_QUERY_VALIDATION,
    FAM_DNS_MAX,
};

static metric_family_t fams[FAM_DNS_MAX] = {
    [FAM_DNS_QUERY_TIME_SECONDS] = {
        .name = "dns_query_time_seconds",
        .type = METRIC_TYPE_GAUGE,
        .help = "DNS lookup time in seconds.",
    },
    [FAM_DNS_QUERY_SUCCESS] = {
        .name = "dns_query_success",
        .type = METRIC_TYPE_GAUGE,
        .help = "Was this DNS query successful or not, 1 for success or 0 for failure.",
    },
    [FAM_DNS_QUERY_VALIDATION] = {
        .name = "dns_query_validation",
        .type = METRIC_TYPE_GAUGE,
        .help = "Was the validation for this DNS query successful or not, "
                "1 for success or 0 for failure.",
    },
};

struct dns_ctx_s;
typedef struct dns_ctx_s dns_ctx_t;

struct nv {
//    const char *name;
    char *name;
    int value;
};

typedef struct {
    char *host;
    uint32_t ttl;
    uint16_t class;
    uint16_t type;
    union {
        struct {
            char *name;
        } cname;
        struct {
            char *hardware;
            char *os;
        } hinfo;
        struct {
            char *mailbox;
            char *error_mailbox;
        } minfo;
        struct {
            uint16_t priority;
            char *mailserver;
        } mx;
        struct {
            char *master;
            char *responsible;
            uint32_t serial;
            uint32_t refresh_interval;
            uint32_t retry_interval;
            uint32_t expire_interval;
            uint32_t negative_caching_ttl;
        } soa;
        struct {
            char *data;
        } txt;
        struct {
            uint8_t flags;
            char *flag;
            char *value;
        } caa;
        struct {
            char address[46];
        } a;
        struct {
            uint16_t priority;
            uint16_t weight;
            uint16_t port;
            char *target;
        } srv;
        struct {
            uint16_t priority;
            uint16_t weight;
            char *target;
        } uri;
        struct {
            uint16_t order;
            uint16_t preference;
            char *flags;
            char *service;
            char *regex;
            char *replacement;
        } naptr;
        struct {
            uint8_t algorithm;
            uint8_t type;
            char *fingerprint;
        } sshfp;
        struct {
            uint16_t flags;
            uint8_t protocol;
            uint8_t algorithm;
            char *public_key;
        } dnskey;
    };
} dns_rr_t;

typedef struct {
    char *name;
    uint16_t class;
    uint16_t type;
} dns_qd_t;

typedef struct {
    cdtime_t query_time;
    uint16_t id;
    uint8_t rcode;
    uint8_t opcode;
    struct {
        uint8_t qr;
        uint8_t aa;
        uint8_t tc;
        uint8_t rd;
        uint8_t ra;
    } flags;
    size_t question_qd_len;
    dns_qd_t *question_qd;
    size_t answer_rr_len;
    dns_rr_t *answer_rr;
    size_t authority_rr_len;
    dns_rr_t *authority_rr;
    size_t additional_rr_len;
    dns_rr_t *additional_rr;
} dns_response_t;

typedef struct {
    size_t num;
    char **domains;
} dns_domains_t;

typedef struct {
    char *query;
    cdtime_t start;
    dns_ctx_t *ctx;
    int type;
    int class;
    label_set_t labels;

    dns_response_t response;
    expr_symtab_t *symtab;
    expr_node_t *ast;
} dns_query_t;

struct dns_ctx_s {
    char *instance;

    dns_query_t **queries;
    size_t queries_num;

    strbuf_t servers;
    dns_domains_t domains;

    int udp_port;
    int tcp_port;

    cdtime_t timeout;
    int tries;
    int ndots;

    int flags;
    label_set_t labels;
    plugin_filter_t *filter;

    ares_channel *channelptr;
    int optmask;
    struct ares_options options;

    metric_family_t fams[FAM_DNS_MAX];
};

enum {
    T_A        = 1,
    T_NS       = 2,
    T_MD       = 3,
    T_MF       = 4,
    T_CNAME    = 5,
    T_SOA      = 6,
    T_MB       = 7,
    T_MG       = 8,
    T_MR       = 9,
    T_NULL     = 10,
    T_WKS      = 11,
    T_PTR      = 12,
    T_HINFO    = 13,
    T_MINFO    = 14,
    T_MX       = 15,
    T_TXT      = 16,
    T_RP       = 17,
    T_AFSDB    = 18,
    T_X25      = 19,
    T_ISDN     = 20,
    T_RT       = 21,
    T_NSAP     = 22,
    T_NSAP_PTR = 23,
    T_SIG      = 24,
    T_KEY      = 25,
    T_PX       = 26,
    T_GPOS     = 27,
    T_AAAA     = 28,
    T_LOC      = 29,
    T_NXT      = 30,
    T_EID      = 31,
    T_NIMLOC   = 32,
    T_SRV      = 33,
    T_ATMA     = 34,
    T_NAPTR    = 35,
    T_KX       = 36,
    T_CERT     = 37,
    T_A6       = 38,
    T_DNAME    = 39,
    T_SINK     = 40,
    T_OPT      = 41,
    T_APL      = 42,
    T_DS       = 43,
    T_SSHFP    = 44,
    T_RRSIG    = 46,
    T_NSEC     = 47,
    T_DNSKEY   = 48,
    T_TKEY     = 249,
    T_TSIG     = 250,
    T_IXFR     = 251,
    T_AXFR     = 252,
    T_MAILB    = 253,
    T_MAILA    = 254,
    T_ANY      = 255,
    T_URI      = 256,
    T_CAA      = 257
};

enum {
    C_IN    = 1,
    C_CHAOS = 3,
    C_HS    = 4,
    C_NONE  = 254,
    C_ANY   = 255,
};
#if 0
enum {
    NOERROR      = 0,
    FORMERR      = 1,
    SERVFAIL     = 2,
    NXDOMAIN     = 3,
    NOTIMP       = 4,
    REFUSED      = 5,
    YXDOMAIN     = 6,
    YXRRSET      = 7,
    NXRRSET      = 8,
    NOTAUTH      = 9,
    NOTZONE      = 10,
    TSIG_BADSIG  = 16,
    TSIG_BADKEY  = 17,
    TSIG_BADTIME = 18,
};
#endif
static const struct nv dns_types[] = {
    { "A",        T_A        },
    { "NS",       T_NS       },
    { "MD",       T_MD       },
    { "MF",       T_MF       },
    { "CNAME",    T_CNAME    },
    { "SOA",      T_SOA      },
    { "MB",       T_MB       },
    { "MG",       T_MG       },
    { "MR",       T_MR       },
    { "NULL",     T_NULL     },
    { "WKS",      T_WKS      },
    { "PTR",      T_PTR      },
    { "HINFO",    T_HINFO    },
    { "MINFO",    T_MINFO    },
    { "MX",       T_MX       },
    { "TXT",      T_TXT      },
    { "RP",       T_RP       },
    { "AFSDB",    T_AFSDB    },
    { "X25",      T_X25      },
    { "ISDN",     T_ISDN     },
    { "RT",       T_RT       },
    { "NSAP",     T_NSAP     },
    { "NSAP_PTR", T_NSAP_PTR },
    { "SIG",      T_SIG      },
    { "KEY",      T_KEY      },
    { "PX",       T_PX       },
    { "GPOS",     T_GPOS     },
    { "AAAA",     T_AAAA     },
    { "LOC",      T_LOC      },
    { "SRV",      T_SRV      },
    { "AXFR",     T_AXFR     },
    { "MAILB",    T_MAILB    },
    { "MAILA",    T_MAILA    },
    { "NAPTR",    T_NAPTR    },
    { "DS",       T_DS       },
    { "SSHFP",    T_SSHFP    },
    { "RRSIG",    T_RRSIG    },
    { "NSEC",     T_NSEC     },
    { "DNSKEY",   T_DNSKEY   },
    { "CAA",      T_CAA      },
    { "URI",      T_URI      },
    { "ANY",      T_ANY      }
};
static size_t dns_types_size = STATIC_ARRAY_SIZE(dns_types);

static const struct nv dns_classes[] = {
    { "IN",    C_IN    },
    { "CHAOS", C_CHAOS },
    { "HS",    C_HS    },
    { "ANY",   C_ANY   }
};
static size_t dns_classes_size = STATIC_ARRAY_SIZE(dns_classes);

#if 0
static char *dns_opcodes[] = {
    "QUERY",
    "IQUERY",
    "STATUS",
    "(reserved)",
    "NOTIFY",
    "UPDATE",
    "(unknown)",
    "(unknown)",
    "(unknown)",
    "UPDATEA",
    "UPDATED",
    "UPDATEDA",
    "UPDATEM",
    "UPDATEMA",
    "ZONEINIT",
    "ZONEREF"
};
static size_t dns_opcodes_size = STATIC_ARRAY_SIZE(dns_opcodes);

static char *dns_rcodes[] = {
    "NOERROR",
    "FORMERR",
    "SERVFAIL",
    "NXDOMAIN",
    "NOTIMP",
    "REFUSED",
    "YXDOMAIN",
    "YXRRSET",
    "NXRRSET",
    "NOTAUTH",
    "NOTZONE",
    "DSOTYPENI",
    "(unknown)",
    "(unknown)",
    "(unknown)",
    "(unknown)",
    "BADSIG",
    "BADKEY",
    "BADTIME",
    "BADMODE",
    "BADNAME",
    "BADALG",
    "BADTRUNC",
    "BADCOOKIE",
};
static size_t dns_rcodes_size = STATIC_ARRAY_SIZE(dns_rcodes);
#endif

static const struct nv dns_constants[] = {
    { "A",           T_A        },
    { "NS",          T_NS       },
    { "MD",          T_MD       },
    { "MF",          T_MF       },
    { "CNAME",       T_CNAME    },
    { "SOA",         T_SOA      },
    { "MB",          T_MB       },
    { "MG",          T_MG       },
    { "MR",          T_MR       },
    { "NULL",        T_NULL     },
    { "WKS",         T_WKS      },
    { "PTR",         T_PTR      },
    { "HINFO",       T_HINFO    },
    { "MINFO",       T_MINFO    },
    { "MX",          T_MX       },
    { "TXT",         T_TXT      },
    { "RP",          T_RP       },
    { "AFSDB",       T_AFSDB    },
    { "X25",         T_X25      },
    { "ISDN",        T_ISDN     },
    { "RT",          T_RT       },
    { "NSAP",        T_NSAP     },
    { "NSAP_PTR",    T_NSAP_PTR },
    { "SIG",         T_SIG      },
    { "KEY",         T_KEY      },
    { "PX",          T_PX       },
    { "GPOS",        T_GPOS     },
    { "AAAA",        T_AAAA     },
    { "LOC",         T_LOC      },
    { "SRV",         T_SRV      },
    { "AXFR",        T_AXFR     },
    { "MAILB",       T_MAILB    },
    { "MAILA",       T_MAILA    },
    { "NAPTR",       T_NAPTR    },
    { "DS",          T_DS       },
    { "SSHFP",       T_SSHFP    },
    { "RRSIG",       T_RRSIG    },
    { "NSEC",        T_NSEC     },
    { "DNSKEY",      T_DNSKEY   },
    { "CAA",         T_CAA      },
    { "URI",         T_URI      },
    { "ANY",         T_ANY      },
    { "IN",          C_IN       },
    { "CHAOS",       C_CHAOS    },
    { "HS",          C_HS       },
    { "QUERY",       0          },
    { "IQUERY",      1          },
    { "STATUS",      2          },
    { "NOTIFY",      4          },
    { "UPDATE",      5          },
    { "UPDATEA",     9          },
    { "UPDATED",    10          },
    { "UPDATEDA",   11          },
    { "UPDATEM",    12          },
    { "UPDATEMA",   13          },
    { "ZONEINIT",   14          },
    { "ZONEREF",    15          },
    { "NOERROR",     0          },
    { "FORMERR",     1          },
    { "SERVFAIL",    2          },
    { "NXDOMAIN",    3          },
    { "NOTIMP",      4          },
    { "REFUSED",     5          },
    { "YXDOMAIN",    6          },
    { "YXRRSET",     7          },
    { "NXRRSET",     8          },
    { "NOTAUTH",     9          },
    { "NOTZONE",    10          },
    { "DSOTYPENI",  11          },
    { "BADSIG",     16          },
    { "BADKEY",     17          },
    { "BADTIME",    18          },
    { "BADMODE",    19          },
    { "BADNAME",    20          },
    { "BADALG",     21          },
    { "BADTRUNC",   22          },
    { "BADCOOKIE",  23          },
};
static size_t dns_constants_size = STATIC_ARRAY_SIZE(dns_constants);

static void dns_rr_reset(dns_rr_t *rr)
{
    if (rr == NULL)
        return;

    if (rr->host)
        free(rr->host);

    switch (rr->type) {
    case T_CNAME:
    case T_MB:
    case T_MD:
    case T_MF:
    case T_MG:
    case T_MR:
    case T_NS:
    case T_PTR:
        if (rr->cname.name !=NULL)
            ares_free_string(rr->cname.name);
        break;
    case T_HINFO:
        if (rr->hinfo.hardware != NULL)
            ares_free_string(rr->hinfo.hardware);
        if (rr->hinfo.os != NULL)
            ares_free_string(rr->hinfo.os);
        break;
    case T_MINFO:
        if (rr->minfo.mailbox != NULL)
            ares_free_string(rr->minfo.mailbox);
        if (rr->minfo.error_mailbox != NULL)
            ares_free_string(rr->minfo.error_mailbox);
        break;
    case T_MX:
        if (rr->mx.mailserver != NULL)
            ares_free_string(rr->mx.mailserver);
        break;
    case T_SOA:
        if (rr->soa.master != NULL)
            ares_free_string(rr->soa.master);
        if (rr->soa.responsible != NULL)
            ares_free_string(rr->soa.responsible);
        break;
    case T_TXT:
        if (rr->txt.data)
            free(rr->txt.data);
        break;
    case T_CAA:
        if (rr->caa.flag != NULL)
            ares_free_string(rr->caa.flag);
        if (rr->caa.value != NULL)
            free(rr->caa.value);
        break;
    case T_A:
    case T_AAAA:
        break;
    case T_WKS:
        /* Not implemented yet */
        break;
    case T_SRV:
        if (rr->srv.target != NULL)
            ares_free_string(rr->srv.target);
        break;
    case T_URI:
        if (rr->uri.target != NULL)
            free(rr->uri.target);
        break;
    case T_NAPTR:
        if (rr->naptr.flags != NULL)
            ares_free_string(rr->naptr.flags);
        if (rr->naptr.service != NULL)
            ares_free_string(rr->naptr.service);
        if (rr->naptr.regex != NULL)
            ares_free_string(rr->naptr.regex);
        if (rr->naptr.replacement != NULL)
            ares_free_string(rr->naptr.replacement);
        break;
    case T_DS:
    case T_SSHFP:
#if 0
        struct {
            uint8_t algorithm;
            uint8_t type;
            char *fingerprint;
        } sshfp;
#endif
    case T_RRSIG:
    case T_NSEC:
        break;
    case T_DNSKEY:
#if 0
        struct {
            uint16_t flags;
            uint8_t protocol;
            uint8_t algorithm;
            char *public_key;
        } dnskey;
#endif
        break;
    default:
        break;
    }
}

static void dns_response_reset(dns_response_t *response)
{
    if (response == NULL)
        return;

    if (response->question_qd != NULL) {
        for (size_t i = 0; i < response->question_qd_len; i++) {
            if (response->question_qd[i].name !=NULL)
                ares_free_string(response->question_qd[i].name);
        }
        free(response->question_qd);
    }

    if (response->answer_rr != NULL) {
        for (size_t i = 0; i < response->answer_rr_len; i++) {
            dns_rr_reset(&response->answer_rr[i]);
        }
        free(response->answer_rr);
    }

    if (response->authority_rr != NULL) {
        for (size_t i = 0; i < response->authority_rr_len; i++) {
            dns_rr_reset(&response->authority_rr[i]);
        }
        free(response->authority_rr);
    }

    if (response->additional_rr != NULL) {
        for (size_t i = 0; i < response->additional_rr_len; i++) {
            dns_rr_reset(&response->additional_rr[i]);
        }
        free(response->additional_rr);
    }

    memset(response, 0, sizeof(*response));
}
#if 0
static char *dns_type_name(int type)
{
    for (size_t i = 0; i < dns_types_size; i++) {
        if (dns_types[i].value == type)
            return dns_types[i].name;
    }

    return "(unknown)";
}

static char *dns_class_name(int class)
{
    for (size_t i = 0; i < dns_classes_size; i++) {
        if (dns_classes[i].value == class)
            return dns_classes[i].name;
    }

    return "(unknown)";
}

static char *dns_opcode_name(int opcode)
{
    if ((opcode >= 0) && (opcode < (int)dns_opcodes_size))
        return dns_opcodes[opcode];

    return "(unknown)";
}

static char *dns_rcode_name(int rcode)
{
    if ((rcode >= 0) && (rcode < (int)dns_rcodes_size))
        return dns_rcodes[rcode];

    return "(unknown)";
}
#endif

static expr_value_t *dns_value_string(char *str)
{
    if (str == NULL)
        return expr_value_alloc_number(NAN);

    return expr_value_alloc_string(str);
}

static expr_value_t *dns_value_rr(expr_id_t *id, dns_rr_t *rr)
{
    if (id->num == 1) {
        if (id->ptr[0].type != EXPR_ID_NAME)
            goto error;

        if (strcmp(id->ptr[0].name, "name") == 0) {
            return dns_value_string(rr->host);
        } else if (strcmp(id->ptr[0].name, "type") == 0) {
//            return expr_value_alloc_string(dns_type_name(rr->type));
            return expr_value_alloc_number(rr->type);
        } else if (strcmp(id->ptr[0].name, "class") == 0) {
//            return expr_value_alloc_string(dns_class_name(rr->class));
            return expr_value_alloc_number(rr->class);
        } else if (strcmp(id->ptr[0].name, "ttl") == 0) {
            return expr_value_alloc_number(rr->ttl);
        }
   } else if (id->num == 2) {
        if (id->ptr[0].type != EXPR_ID_NAME)
            goto error;
        if (id->ptr[1].type != EXPR_ID_NAME)
            goto error;

        switch (rr->type) {
        case T_CNAME:
            if (strcmp(id->ptr[0].name, "cname") == 0) {
                if (strcmp(id->ptr[1].name, "name") == 0)
                    return dns_value_string(rr->cname.name);
            }
            break;
        case T_MB:
            if (strcmp(id->ptr[0].name, "mb") == 0) {
                if (strcmp(id->ptr[1].name, "name") == 0)
                    return dns_value_string(rr->cname.name);
            }
            break;
        case T_MD:
            if (strcmp(id->ptr[0].name, "md") == 0) {
                if (strcmp(id->ptr[1].name, "name") == 0)
                    return dns_value_string(rr->cname.name);
            }
            break;
        case T_MF:
            if (strcmp(id->ptr[0].name, "mf") == 0) {
                if (strcmp(id->ptr[1].name, "name") == 0)
                    return dns_value_string(rr->cname.name);
            }
            break;
        case T_MG:
            if (strcmp(id->ptr[0].name, "mg") == 0) {
                if (strcmp(id->ptr[1].name, "name") == 0)
                    return dns_value_string(rr->cname.name);
            }
            break;
        case T_MR:
            if (strcmp(id->ptr[0].name, "mr") == 0) {
                if (strcmp(id->ptr[1].name, "name") == 0)
                    return dns_value_string(rr->cname.name);
            }
            break;
        case T_NS:
            if (strcmp(id->ptr[0].name, "ns") == 0) {
                if (strcmp(id->ptr[1].name, "name") == 0)
                    return dns_value_string(rr->cname.name);
            }
            break;
        case T_PTR:
            if (strcmp(id->ptr[0].name, "ptr") == 0) {
                if (strcmp(id->ptr[1].name, "name") == 0)
                    return dns_value_string(rr->cname.name);
            }
            break;
        case T_HINFO:
            if (strcmp(id->ptr[0].name, "hinfo") == 0) {
                if (strcmp(id->ptr[1].name, "hardware") == 0) {
                    return dns_value_string(rr->hinfo.hardware);
                } else if (strcmp(id->ptr[1].name, "os") == 0) {
                    return dns_value_string(rr->hinfo.os);
                }
            }
            break;
        case T_MINFO:
            if (strcmp(id->ptr[0].name, "minfo") == 0) {
                if (strcmp(id->ptr[1].name, "mailbox") == 0) {
                    return dns_value_string(rr->minfo.mailbox);
                } else if (strcmp(id->ptr[1].name, "error_mailbox") == 0) {
                    return dns_value_string(rr->minfo.error_mailbox);
                }
            }
            break;
        case T_MX:
            if (strcmp(id->ptr[0].name, "mx") == 0) {
                if (strcmp(id->ptr[1].name, "priority") == 0) {
                    return expr_value_alloc_number(rr->mx.priority);
                } else if (strcmp(id->ptr[1].name, "mailserver") == 0) {
                    return dns_value_string(rr->mx.mailserver);
                }
            }
            break;
        case T_SOA:
            if (strcmp(id->ptr[0].name, "soa") == 0) {
                if (strcmp(id->ptr[1].name, "master") == 0) {
                    return dns_value_string(rr->soa.master);
                } else if (strcmp(id->ptr[1].name, "responsible") == 0) {
                    return dns_value_string(rr->soa.responsible);
                } else if (strcmp(id->ptr[1].name, "serial") == 0) {
                    return expr_value_alloc_number(rr->soa.serial);
                } else if (strcmp(id->ptr[1].name, "refresh_interval") == 0) {
                    return expr_value_alloc_number(rr->soa.refresh_interval);
                } else if (strcmp(id->ptr[1].name, "retry_interval") == 0) {
                    return expr_value_alloc_number(rr->soa.retry_interval);
                } else if (strcmp(id->ptr[1].name, "expire") == 0) {
                    return expr_value_alloc_number(rr->soa.expire_interval);
                } else if (strcmp(id->ptr[1].name, "negative_caching_ttl") == 0) {
                    return expr_value_alloc_number(rr->soa.negative_caching_ttl);
                }
            }
            break;
        case T_TXT:
            if (strcmp(id->ptr[0].name, "txt") == 0) {
                if (strcmp(id->ptr[1].name, "data") == 0)
                    return dns_value_string(rr->txt.data);
            }
            break;
        case T_CAA:
            if (strcmp(id->ptr[0].name, "caa") == 0) {
                if (strcmp(id->ptr[1].name, "flags") == 0) {
                    return expr_value_alloc_number(rr->caa.flags);
                } else if (strcmp(id->ptr[1].name, "tag") == 0) {
                    return dns_value_string(rr->caa.flag);
                } else if (strcmp(id->ptr[1].name, "value") == 0) {
                    return dns_value_string(rr->caa.value);
                }
            }
            break;
        case T_A:
            if (strcmp(id->ptr[0].name, "a") == 0) {
                if (strcmp(id->ptr[1].name, "address") == 0) {
                    return expr_value_alloc_string(rr->a.address);
                }
            }
            break;
        case T_AAAA:
            if (strcmp(id->ptr[0].name, "aaaa") == 0) {
                if (strcmp(id->ptr[1].name, "address") == 0) {
                    return expr_value_alloc_string(rr->a.address);
                }
            }
            break;
        case T_WKS:
            /* Not implemented yet */
            break;
        case T_SRV:
            if (strcmp(id->ptr[0].name, "srv") == 0) {
                if (strcmp(id->ptr[1].name, "priority") == 0) {
                    return expr_value_alloc_number(rr->srv.priority);
                } else if (strcmp(id->ptr[1].name, "weight") == 0) {
                    return expr_value_alloc_number(rr->srv.weight);
                } else if (strcmp(id->ptr[1].name, "port") == 0) {
                    return expr_value_alloc_number(rr->srv.port);
                } else if (strcmp(id->ptr[1].name, "target") == 0) {
                    return dns_value_string(rr->srv.target);
                }
            }
            break;
        case T_URI:
            if (strcmp(id->ptr[0].name, "uri") == 0) {
                if (strcmp(id->ptr[1].name, "priority") == 0) {
                    return expr_value_alloc_number(rr->uri.priority);
                } else if (strcmp(id->ptr[1].name, "weight") == 0) {
                    return expr_value_alloc_number(rr->uri.weight);
                } else if (strcmp(id->ptr[1].name, "target") == 0) {
                    return dns_value_string(rr->uri.target);
                }
            }
            break;
        case T_NAPTR:
            if (strcmp(id->ptr[0].name, "naptr") == 0) {
                if (strcmp(id->ptr[1].name, "order") == 0) {
                    return expr_value_alloc_number(rr->naptr.order);
                } else if (strcmp(id->ptr[1].name, "preference") == 0) {
                    return expr_value_alloc_number(rr->naptr.preference);
                } else if (strcmp(id->ptr[1].name, "flags") == 0) {
                    return dns_value_string(rr->naptr.flags);
                } else if (strcmp(id->ptr[1].name, "service") == 0) {
                    return dns_value_string(rr->naptr.service);
                } else if (strcmp(id->ptr[1].name, "regex") == 0) {
                    return dns_value_string(rr->naptr.regex);
                } else if (strcmp(id->ptr[1].name, "replacement") == 0) {
                    return dns_value_string(rr->naptr.replacement);
                }
            }
            break;

        case T_DS:
        case T_SSHFP:
        case T_RRSIG:
        case T_NSEC:
        case T_DNSKEY:
            break;
        default:
            break;
        }
    }

error:
    return expr_value_alloc_number(NAN);
}

static expr_value_t *dns_value_qd(expr_id_t *id, dns_qd_t *qd)
{
    if (id->num == 1) {
        if (id->ptr[0].type != EXPR_ID_NAME)
            goto error;

        if (strcmp(id->ptr[0].name, "name") == 0) {
            return dns_value_string(qd->name);
        } else if (strcmp(id->ptr[0].name, "type") == 0) {
  //          return expr_value_alloc_string(dns_type_name(qd->type));
            return expr_value_alloc_number(qd->type);
        } else if (strcmp(id->ptr[0].name, "class") == 0) {
//            return expr_value_alloc_string(dns_class_name(qd->class));
            return expr_value_alloc_number(qd->class);
        }
    }
error:
    return expr_value_alloc_number(NAN);
}

static expr_value_t *dns_value_response(expr_id_t *id, void *data)
{
    dns_response_t *response = data;

    if (id->num < 2)
        goto error;

    if (id->ptr[1].type != EXPR_ID_NAME)
        goto error;

    if (strcmp(id->ptr[1].name, "id") == 0) {
        return expr_value_alloc_number(response->id);
    } else if (strcmp(id->ptr[1].name, "flags") == 0) {
        if (id->num != 3)
            goto error;
        if (id->ptr[2].type != EXPR_ID_NAME)
            goto error;

        if (strcmp(id->ptr[2].name, "qr") == 0) {
            return expr_value_alloc_bool(response->flags.qr);
        } else if (strcmp(id->ptr[2].name, "aa") == 0) {
            return expr_value_alloc_bool(response->flags.aa);
        } else if (strcmp(id->ptr[2].name, "tc") == 0) {
            return expr_value_alloc_bool(response->flags.tc);
        } else if (strcmp(id->ptr[2].name, "rd") == 0) {
            return expr_value_alloc_bool(response->flags.rd);
        } else if (strcmp(id->ptr[2].name, "ra") == 0) {
            return expr_value_alloc_bool(response->flags.ra);
        }
    } else if (strcmp(id->ptr[1].name, "rtime") == 0) {
        return expr_value_alloc_number(CDTIME_T_TO_DOUBLE(response->query_time));
    } else if (strcmp(id->ptr[1].name, "rcode") == 0) {
//        return expr_value_alloc_string(dns_rcode_name(response->rcode));
        return expr_value_alloc_number(response->rcode);
    } else if (strcmp(id->ptr[1].name, "opcode") == 0) {
  //      return expr_value_alloc_string(dns_opcode_name(response->opcode));
        return expr_value_alloc_number(response->opcode);
    } else if (strcmp(id->ptr[1].name, "question") == 0) {
        if (id->num < 3)
            goto error;

        if (id->ptr[2].type == EXPR_ID_NAME) {
            if (strcmp(id->ptr[2].name, "length") == 0)
                return expr_value_alloc_number(response->question_qd_len);
        } else {
            size_t idx = id->ptr[2].idx;
            if (idx >= response->question_qd_len)
                goto error;
            expr_id_t qd_id = {.num = id->num - 3, id->ptr + 3};
            return dns_value_qd(&qd_id, &response->question_qd[idx]);
        }
    } else if (strcmp(id->ptr[1].name, "answer") == 0) {
        if (id->num < 3)
            goto error;

        if (id->ptr[2].type == EXPR_ID_NAME) {
            if (strcmp(id->ptr[2].name, "length") == 0)
                return expr_value_alloc_number(response->answer_rr_len);
        } else {
            size_t idx = id->ptr[2].idx;
            if (idx >= response->answer_rr_len)
                goto error;
            expr_id_t rr_id = {.num = id->num - 3, id->ptr + 3};
            return dns_value_rr(&rr_id, &response->answer_rr[idx]);
        }
    } else if (strcmp(id->ptr[1].name, "authority") == 0) {
        if (id->num < 3)
            goto error;

        if (id->ptr[2].type == EXPR_ID_NAME) {
            if (strcmp(id->ptr[2].name, "length") == 0)
                return expr_value_alloc_number(response->authority_rr_len);
        } else {
            size_t idx = id->ptr[2].idx;
            if (idx >= response->authority_rr_len)
                goto error;
            expr_id_t rr_id = {.num = id->num - 3, id->ptr + 3};
            return dns_value_rr(&rr_id, &response->authority_rr[idx]);
        }
    } else if (strcmp(id->ptr[1].name, "additional") == 0) {
        if (id->num < 3)
            goto error;

        if (id->ptr[2].type == EXPR_ID_NAME) {
            if (strcmp(id->ptr[2].name, "length") == 0)
                return expr_value_alloc_number(response->additional_rr_len);
        } else {
            size_t idx = id->ptr[2].idx;
            if (idx >= response->additional_rr_len)
                goto error;
            expr_id_t rr_id = {.num = id->num - 3, id->ptr + 3};
            return dns_value_rr(&rr_id, &response->additional_rr[idx]);
        }
    }

error:
    return expr_value_alloc_number(NAN);
}

static const unsigned char *parse_qd(dns_qd_t *qd, const unsigned char *aptr,
                                                   const unsigned char *abuf, int alen)
{
    /* Parse the question name. */
    char *name;
    long len;
    int status = ares_expand_name(aptr, abuf, alen, &name, &len);
    if (status != ARES_SUCCESS)
        return NULL;
    aptr += len;

    /* Make sure there's enough data after the name for the fixed part of the question. */
    if (aptr + QFIXEDSZ > abuf + alen) {
        ares_free_string(name);
        return NULL;
    }

    qd->name = name;
    qd->type = DNS_QUESTION_TYPE(aptr);
    qd->class = DNS_QUESTION_CLASS(aptr);

    aptr += QFIXEDSZ;

    return aptr;
}

static const unsigned char *parse_rr(dns_rr_t *rr, const unsigned char *aptr,
                                                   const unsigned char *abuf, int alen)
{
    const unsigned char *p;

    union {
        unsigned char *as_uchar;
        char *as_char;
    } name;

    /* Parse the RR name. */
    long len;
    int status = ares_expand_name(aptr, abuf, alen, &name.as_char, &len);
    if (status != ARES_SUCCESS)
        return NULL;
    aptr += len;

    /* Make sure there is enough data after the RR name for the fixed part of the RR. */
    if (aptr + RRFIXEDSZ > abuf + alen) {
        ares_free_string(name.as_char);
        return NULL;
    }

    rr->host = name.as_char;
    rr->type = DNS_RR_TYPE(aptr);
    rr->class = DNS_RR_CLASS(aptr);
    rr->ttl = DNS_RR_TTL(aptr);

    int dlen = DNS_RR_LEN(aptr);

    aptr += RRFIXEDSZ;
    if (aptr + dlen > abuf + alen)
        return NULL;

    const unsigned char *end = aptr + dlen;

    /* Display the RR data.  Don't touch aptr. */
    switch (rr->type) {
    case T_CNAME:
    case T_MB:
    case T_MD:
    case T_MF:
    case T_MG:
    case T_MR:
    case T_NS:
    case T_PTR:
        /* For these types, the RR data is just a domain name. */
        status = ares_expand_name(aptr, abuf, alen, &name.as_char, &len);
        if (status != ARES_SUCCESS)
            return NULL;
        rr->cname.name = name.as_char;
        if (len > dlen)
            return NULL;
        break;
    case T_HINFO:
        /* The RR data is two length-counted character strings. */
        p = aptr;
        if ((p >= end) || (p + *p + 1 > end))
            return NULL;
        status = ares_expand_string(p, abuf, alen, &name.as_uchar, &len);
        if (status != ARES_SUCCESS)
            return NULL;
        rr->hinfo.hardware = name.as_char;
        p += len;
        if ((p >= end) || (p + *p + 1 > end))
            return NULL;
        status = ares_expand_string(p, abuf, alen, &name.as_uchar, &len);
        if (status != ARES_SUCCESS)
            return NULL;
        rr->hinfo.os = name.as_char;
        break;
    case T_MINFO:
        /* The RR data is two domain names. */
        p = aptr;
        status = ares_expand_name(p, abuf, alen, &name.as_char, &len);
        if (status != ARES_SUCCESS)
            return NULL;
        rr->minfo.mailbox = name.as_char;
        p += len;
        if (p >= end)
            return NULL;
        status = ares_expand_name(p, abuf, alen, &name.as_char, &len);
        if (status != ARES_SUCCESS)
            return NULL;
        rr->minfo.error_mailbox = name.as_char;
        if (p + len > end)
            return NULL;
        break;
    case T_MX:
        /* The RR data is two bytes giving a preference ordering, and then a domain name.  */
        if (dlen < 2)
            return NULL;
        rr->mx.priority = DNS__16BIT(aptr);
        status = ares_expand_name(aptr + 2, abuf, alen, &name.as_char, &len);
        if (status != ARES_SUCCESS)
            return NULL;
        rr->mx.mailserver = name.as_char;
        if (aptr + 2 + len > end)
            return NULL;
        break;

    case T_SOA:
        /* The RR data is two domain names and then five four-byte
         * numbers giving the serial number and some timeouts.
         */
        p = aptr;
        status = ares_expand_name(p, abuf, alen, &name.as_char, &len);
        if (status != ARES_SUCCESS)
            return NULL;
        rr->soa.master = name.as_char;
        p += len;
        status = ares_expand_name(p, abuf, alen, &name.as_char, &len);
        if (status != ARES_SUCCESS)
            return NULL;
        rr->soa.responsible = name.as_char;
        p += len;
        if (p + 20 > aptr + dlen)
            return NULL;

        rr->soa.serial = DNS__32BIT(p);
        rr->soa.refresh_interval = DNS__32BIT(p+4);
        rr->soa.retry_interval = DNS__32BIT(p+8);
        rr->soa.expire_interval = DNS__32BIT(p+12);
        rr->soa.negative_caching_ttl = DNS__32BIT(p+16);
        break;
    case T_TXT:
        /* The RR data is one or more length-counted character
         * strings, they are concatenated. */
        rr->txt.data = malloc(dlen + 1);
        if (rr->txt.data == NULL)
            return NULL;
        len = 0;
        p = aptr;
        while (p < end) {
            if (p + *p + 1 > end)
                return NULL;
            memcpy(rr->txt.data + len, p + 1, *p);
            len += *p;
            p += *p + 1;
        }
        rr->txt.data[len] = '\0';
        break;
    case T_CAA:
        if (dlen < 2)
            return NULL;
        p = aptr;
        rr->caa.flags = *p;
        p += 1;
        /* Remainder of record */
        int vlen = dlen - (int)*p - 2;
        if (vlen < 0)
            return NULL;
        /* The Property identifier, one of: - "issue", - "iodef", or - "issuewild" */
        status = ares_expand_string(p, abuf, alen, &name.as_uchar, &len);
        if (status != ARES_SUCCESS)
            return NULL;
        rr->caa.flag = name.as_char;
        p += len;

        if (p + vlen > end)
            return NULL;

        /* A sequence of octets representing the Property Value */
        rr->caa.value = sstrndup((const char *)p, vlen);
        if (rr->caa.value == NULL)
            return NULL;
        break;
    case T_A:
        /* The RR data is a four-byte Internet address. */
        if (dlen != 4)
            return NULL;
        ares_inet_ntop(AF_INET, aptr, rr->a.address, sizeof(rr->a.address));
        break;
    case T_AAAA:
        /* The RR data is a 16-byte IPv6 address. */
        if (dlen != 16)
            return NULL;
        ares_inet_ntop(AF_INET6, aptr, rr->a.address, sizeof(rr->a.address));
        break;
    case T_WKS:
        /* Not implemented yet */
        break;
    case T_SRV:
        /* The RR data is three two-byte numbers representing the
         * priority, weight, and port, followed by a domain name. */
        if (dlen < 6)
            return NULL;
        rr->srv.priority = DNS__16BIT(aptr);
        rr->srv.weight = DNS__16BIT(aptr + 2);
        rr->srv.port = DNS__16BIT(aptr + 4);

        status = ares_expand_name(aptr + 6, abuf, alen, &name.as_char, &len);
        if (status != ARES_SUCCESS)
            return NULL;
        rr->srv.target = name.as_char;
        if (aptr + 6 + len > end)
            return NULL;
        break;
    case T_URI:
        /* The RR data is two two-byte numbers representing the
         * priority and weight, followed by a target. */
        if (dlen < 4)
            return NULL;
        rr->uri.priority = DNS__16BIT(aptr);
        rr->uri.weight = DNS__16BIT(aptr+2);
        rr->uri.target = sstrndup((const char *)aptr + 4, dlen - 4);
        if (rr->uri.target == NULL)
            return NULL;
        break;

    case T_NAPTR:
        if (dlen < 4)
            return NULL;
        rr->naptr.order = DNS__16BIT(aptr);
        rr->naptr.preference = DNS__16BIT(aptr + 2);

        p = aptr + 4;
        if ((p >= end) || (p + *p + 1 > end))
            return NULL;
        status = ares_expand_string(p, abuf, alen, &name.as_uchar, &len);
        if (status != ARES_SUCCESS)
            return NULL;
        rr->naptr.flags = name.as_char;
        p += len;

        if ((p >= end) || (p + *p + 1 > end))
            return NULL;
        status = ares_expand_string(p, abuf, alen, &name.as_uchar, &len);
        if (status != ARES_SUCCESS)
            return NULL;
        rr->naptr.service = name.as_char;
        p += len;

        if ((p >= end) || (p + *p + 1 > end))
            return NULL;
        status = ares_expand_string(p, abuf, alen, &name.as_uchar, &len);
        if (status != ARES_SUCCESS)
            return NULL;
        rr->naptr.regex = name.as_char;
        p += len;

        if (p >= end)
            return NULL;
        status = ares_expand_name(p, abuf, alen, &name.as_char, &len);
        if (status != ARES_SUCCESS)
            return NULL;
        rr->naptr.replacement = name.as_char;
        if (p + len > end)
            return NULL;
        break;

    case T_DS:
    case T_SSHFP:
    case T_RRSIG:
    case T_NSEC:
    case T_DNSKEY:
        PLUGIN_DEBUG("Parsing for RR type %hu unavailable.", rr->type);
        break;

    default:
        PLUGIN_DEBUG("Unknown RR type %hu, parsing unavailable.", rr->type);
        break;
    }

    return aptr + dlen;
}

#if ARES_VERSION == 74247 /* 1.34.7 */
static void dns_callback(void *arg, int status, int timeouts, const unsigned char *abuf, int alen)
#else
static void dns_callback(void *arg, int status, int timeouts, unsigned char *abuf, int alen)
#endif
{
    dns_query_t *query = arg;

    if (query == NULL)
        return;

    dns_ctx_t *ctx = query->ctx;

    (void) timeouts;

    if (status != ARES_SUCCESS) {
        PLUGIN_DEBUG("Query '%s' failed: %s", query->query, ares_strerror(status));
    }

    if ((abuf == NULL) || (alen < HFIXEDSZ)) {
        metric_family_append(&ctx->fams[FAM_DNS_QUERY_SUCCESS], VALUE_GAUGE(0), &query->labels,
                             &LABEL_PAIR_CONST("query", query->query), NULL);
        if (query->ast != NULL)
            metric_family_append(&ctx->fams[FAM_DNS_QUERY_VALIDATION], VALUE_GAUGE(0),
                                 &query->labels, &LABEL_PAIR_CONST("query", query->query), NULL);
        return;
    }

    bool validation = false;
    dns_response_t *response = &query->response; // FIXME ¿?

    response->query_time = cdtime() - query->start;

    metric_family_append(&ctx->fams[FAM_DNS_QUERY_TIME_SECONDS],
                         VALUE_GAUGE(CDTIME_T_TO_DOUBLE(response->query_time)), &query->labels,
                         &LABEL_PAIR_CONST("query", query->query), NULL);

    /* Parse the answer header. */
    response->id = DNS_HEADER_QID(abuf);
    response->flags.qr = DNS_HEADER_QR(abuf);
    response->flags.aa = DNS_HEADER_AA(abuf);
    response->flags.tc = DNS_HEADER_TC(abuf);
    response->flags.rd = DNS_HEADER_RD(abuf);
    response->flags.ra = DNS_HEADER_RA(abuf);
    response->opcode = DNS_HEADER_OPCODE(abuf);
    response->rcode = DNS_HEADER_RCODE(abuf);

    metric_family_append(&ctx->fams[FAM_DNS_QUERY_SUCCESS],
                         VALUE_GAUGE(response->rcode == 0 ? 1 : 0), &query->labels,
                         &LABEL_PAIR_CONST("query", query->query), NULL);

    response->question_qd_len = DNS_HEADER_QDCOUNT(abuf);
    unsigned int qdcount = DNS_HEADER_QDCOUNT(abuf);
    response->answer_rr_len = DNS_HEADER_ANCOUNT(abuf);
    unsigned int ancount = DNS_HEADER_ANCOUNT(abuf);
    response->authority_rr_len = DNS_HEADER_NSCOUNT(abuf);
    unsigned int nscount = DNS_HEADER_NSCOUNT(abuf);
    response->additional_rr_len = DNS_HEADER_ARCOUNT(abuf);
    unsigned int arcount = DNS_HEADER_ARCOUNT(abuf);

    const unsigned char *aptr = abuf + HFIXEDSZ;

    if (qdcount > 0) {
        response->question_qd = calloc(qdcount, sizeof(*(response->question_qd)));
        if (response->question_qd == NULL) {
            PLUGIN_ERROR("calloc failed.");
            goto exit;
        }
        for (unsigned int i = 0; i < qdcount; i++) {
            aptr = parse_qd(&response->question_qd[i], aptr, abuf, alen);
            if (aptr == NULL) {
                PLUGIN_ERROR("Parsing question records failed.");
                goto exit;
            }
        }
    }

    if (ancount > 0) {
        response->answer_rr = calloc(ancount, sizeof(*(response->answer_rr)));
        if (response->answer_rr == NULL) {
            PLUGIN_ERROR("calloc failed.");
            goto exit;
        }
        for (unsigned int i = 0; i < ancount; i++) {
            aptr = parse_rr(&response->answer_rr[i], aptr, abuf, alen);
            if (aptr == NULL) {
                PLUGIN_ERROR("Parsing answers records failed.");
                goto exit;
            }
        }
    }

    if (nscount > 0) {
        response->authority_rr = calloc(nscount, sizeof(*(response->authority_rr)));
        if (response->authority_rr == NULL) {
            PLUGIN_ERROR("calloc failed.");
            goto exit;
        }
        for (unsigned int i = 0; i < nscount; i++) {
            aptr = parse_rr(&response->authority_rr[i], aptr, abuf, alen);
            if (aptr == NULL) {
                PLUGIN_ERROR("Parsing NS records failed.");
                goto exit;
            }
        }
    }

    if (arcount > 0) {
        response->additional_rr = calloc(arcount, sizeof(*(response->additional_rr)));
        if (response->additional_rr == NULL) {
            PLUGIN_ERROR("calloc failed.");
            goto exit;
        }
        for (unsigned int i = 0; i < arcount; i++) {
            aptr = parse_rr(&response->additional_rr[i], aptr, abuf, alen);
            if (aptr == NULL) {
                PLUGIN_ERROR("Parsing additional records failed.");
                goto exit;
            }
        }
    }

    if (query->ast != NULL) {
        expr_value_t *value = expr_eval(query->ast);
        if (value != NULL) {
            switch(value->type) {
            case EXPR_VALUE_NUMBER:
//fprintf(stderr, "number: %f\n", value->number);
                validation = value->number == 0.0 ? false : true;
                break;
            case EXPR_VALUE_STRING:
//fprintf(stderr, "string: [%s]\n", value->string);
                validation = (value->string != NULL) && (strlen(value->string) > 0) ? true : false;
                break;
            case EXPR_VALUE_BOOLEAN:
//fprintf(stderr, "bool: %s\n", value->boolean ? "true" : "false");
                validation = value->boolean;
                break;
            }
            expr_value_free(value);
        } else {
//fprintf(stderr, "null\n");
        }
    } else {
//fprintf(stderr, "query->ast  is null\n");
    }

exit:
    if (query->ast != NULL)
        metric_family_append(&ctx->fams[FAM_DNS_QUERY_VALIDATION],
                             VALUE_GAUGE(validation), &query->labels,
                             &LABEL_PAIR_CONST("query", query->query), NULL);

    dns_response_reset(response);
}

static int dns_read(user_data_t *user_data)
{
    dns_ctx_t *ctx = user_data->data;

    ares_channel channel = {0};

    int status = ares_init_options(&channel, &ctx->options, ctx->optmask);
    if (status != ARES_SUCCESS) {
        PLUGIN_ERROR("ares_init_options: %s", ares_strerror(status));
        return -1;
    }

    if(strbuf_len(&ctx->servers) > 0) {
        status = ares_set_servers_csv(channel, ctx->servers.ptr);
        if (status != ARES_SUCCESS) {
            ares_destroy(channel);
            PLUGIN_ERROR("ares_init_options: %s", ares_strerror(status));
            return -1;
        }
    }

    for (size_t i = 0; i < ctx->queries_num; i++) {
        dns_query_t *query = ctx->queries[i];

        query->start = cdtime();

        if (ctx->options.flags & ARES_FLAG_NOSEARCH)
            ares_query(channel, query->query, query->class, query->type,
                                dns_callback, (void*)query);
        else
            ares_search(channel, query->query, query->class, query->type,
                                 dns_callback, (void*)query);
    }

    for (;;) {
        ares_socket_t socks[ARES_GETSOCK_MAXNUM];
        /* Not using ARES_GETSOCK_WRITABLE(): it shifts a signed 1 into the sign bit. */
        unsigned int bitmask = (unsigned int)ares_getsock(channel, socks, ARES_GETSOCK_MAXNUM);

        struct pollfd pfds[ARES_GETSOCK_MAXNUM];
        nfds_t nfds = 0;
        for (int i = 0; i < ARES_GETSOCK_MAXNUM; i++) {
            short events = 0;
            if (bitmask & (1U << i))
                events |= POLLIN;
            if (bitmask & (1U << (i + ARES_GETSOCK_MAXNUM)))
                events |= POLLOUT;
            if (events == 0)
                continue;
            pfds[nfds] = (struct pollfd){.fd = socks[i], .events = events};
            nfds++;
        }

        if (nfds == 0)
            break;

        struct timeval tv;
        struct timeval *tvp = ares_timeout(channel, NULL, &tv);
        int timeout = -1;
        if (tvp != NULL)
            timeout = (int)(tvp->tv_sec * 1000 + (tvp->tv_usec + 999) / 1000);

        int count = poll(pfds, nfds, timeout);
        if (count < 0) {
            if (errno == EINTR)
                continue;
            PLUGIN_ERROR("poll fail: %s", STRERRNO );
            ares_destroy(channel);
            for (size_t i = 0; i < FAM_DNS_MAX; i++)
                metric_family_metric_reset(&ctx->fams[i]);
            return 1;
        }

        if (count == 0) {
            ares_process_fd(channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
            continue;
        }

        for (nfds_t i = 0; i < nfds; i++) {
            ares_socket_t rfd = ARES_SOCKET_BAD;
            ares_socket_t wfd = ARES_SOCKET_BAD;
            if (pfds[i].revents & (POLLIN | POLLERR | POLLHUP))
                rfd = pfds[i].fd;
            if (pfds[i].revents & POLLOUT)
                wfd = pfds[i].fd;
            if ((rfd != ARES_SOCKET_BAD) || (wfd != ARES_SOCKET_BAD))
                ares_process_fd(channel, rfd, wfd);
        }
    }

    ares_destroy(channel);

    plugin_dispatch_metric_family_array_filtered(ctx->fams, FAM_DNS_MAX, ctx->filter, 0);

    return 0;
}

static int dns_config_query_class(config_item_t *ci, dns_query_t *query)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    char *class = ci->values[0].value.string;
    int class_value = -1;

    for (size_t i = 0; i < dns_classes_size; i++) {
        if (strcasecmp(dns_classes[i].name, class) == 0) {
            class_value = dns_classes[i].value;
            break;
        }
    }

    if (class_value < 0) {
        PLUGIN_ERROR("Unknown dns class '%s' in %s:%d.", class, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    query->class = class_value;

    return 0;
}

static int dns_config_query_type(config_item_t *ci, dns_query_t *query)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    char *type = ci->values[0].value.string;
    int type_value = -1;

    for (size_t i = 0; i < dns_types_size; i++) {
        if (strcasecmp(dns_types[i].name, type) == 0) {
            type_value = dns_types[i].value;
            break;
        }
    }

    if (type_value < 0) {
        PLUGIN_ERROR("Unknown dns type '%s' in %s:%d.", type, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    query->type = type_value;

    return 0;
}

static int dns_convert_query (dns_query_t *query, int use_bitstring)
{
    static const char hex_chars[] = "0123456789ABCDEF";
    char new_name [MAX_IP6_RR];

    union {
        struct in_addr       addr4;
        struct ares_in6_addr addr6;
    } addr;

    if (ares_inet_pton (AF_INET, query->query, &addr.addr4) == 1) {
        unsigned long laddr = ntohl(addr.addr4.s_addr);
        unsigned long a1 = (laddr >> 24UL) & 0xFFUL;
        unsigned long a2 = (laddr >> 16UL) & 0xFFUL;
        unsigned long a3 = (laddr >>  8UL) & 0xFFUL;
        unsigned long a4 = laddr & 0xFFUL;

        snprintf(new_name, sizeof(new_name), "%lu.%lu.%lu.%lu.in-addr.arpa", a4, a3, a2, a1);
    } else if (ares_inet_pton(AF_INET6, query->query, &addr.addr6) == 1) {
        char *c = new_name;
        const unsigned char *ip = (const unsigned char*) &addr.addr6;
        int max_i = (int)sizeof(addr.addr6) - 1;
        int i, hi, lo;

        /* Use the more compact RFC-2673 notation?
           * Currently doesn't work or unsupported by the DNS-servers I've tested against.
           */
        if (use_bitstring) {
            *c++ = '\\';
            *c++ = '[';
            *c++ = 'x';
            for (i = max_i; i >= 0; i--) {
                hi = ip[i] >> 4;
                lo = ip[i] & 15;
                *c++ = hex_chars [lo];
                *c++ = hex_chars [hi];
            }
            strcpy (c, "].IP6.ARPA");
        } else {
            for (i = max_i; i >= 0; i--) {
                hi = ip[i] >> 4;
                lo = ip[i] & 15;
                *c++ = hex_chars [lo];
                *c++ = '.';
                *c++ = hex_chars [hi];
                *c++ = '.';
            }
            strcpy (c, "IP6.ARPA");
        }
    } else {
        return 0;
    }

    free(query->query);
    query->query = strdup(new_name);
    if (query->query == NULL) {
        PLUGIN_ERROR("strdup failed.");
        return -1;
    }

    return 0;
}

#if 0
DIG response header:

Flags:
AA = Authoritative Answer
TC = Truncation
RD = Recursion Desired (set in a query and copied into the response if recursion is supported)
RA = Recursion Available (if set, denotes recursive query support is available)
AD = Authenticated Data (for DNSSEC only; indicates that the data was authenticated)
CD = Checking Disabled (DNSSEC only; disables checking at the receiving server)


QR specifies whether this message is a query (0), or a response (1)
OPCODE A four bit field, only valid values: 0,1,2
AA Authoritative Answer
TC TrunCation (truncated due to length greater than that permitted on the transmission channel)
RD Recursion Desired
RA Recursion Available

There were two more DNSSEC-related flags introduced in RFC 4035:

CD (Checking Disabled): indicates a security-aware resolver should disable signature validation (that is, not check DNSSEC records)
AD (Authentic Data): indicates the resolver believes the responses to be authentic - that is, validated by DNSSEC

Response code:

0 = NOERR, no error
1 = FORMERR, format error (unable to understand the query)
2 = SERVFAIL, name server problem
3 = NXDOMAIN, domain name does not exist
4 = NOTIMPL, not implemented
5 = REFUSED (e.g., refused zone transfer requests)


AA
TC
RD
RA
AD
CD

rcode

int type     = DNS_RR_TYPE(aptr);
int dnsclass = DNS_RR_CLASS(aptr);
int ttl      = DNS_RR_TTL(aptr);

rr

#endif

static int dns_config_query_expr(config_item_t *ci, dns_query_t *query)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if ((query->symtab != NULL) || (query->ast != NULL)) {
        PLUGIN_ERROR("The '%s' option is already configured in %s:%d.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    char *expr = ci->values[0].value.string;

    query->symtab = expr_symtab_alloc();
    if (query->symtab == NULL) {
        return -1;
    }

    int status = 0;
    for (size_t i = 0; i < dns_constants_size; i++) {
        expr_id_item_t item = {.type = EXPR_ID_NAME, .name = dns_constants[i].name };
        expr_id_t id = {.num = 1, .ptr = &item };
        status = expr_symtab_append_number(query->symtab, &id, dns_constants[i].value);
        if (status != 0) {
            PLUGIN_ERROR("Failed to add '%s' to the symbol table.", dns_constants[i].name);
            return -1;
        }
    }

    expr_id_item_t ritem = {.type = EXPR_ID_NAME, .name = "response" };
    expr_id_t rid = {.num = 1, .ptr = &ritem };
    status = expr_symtab_append_callback(query->symtab, &rid, dns_value_response,
                                         &query->response);
    if (status != 0) {
        PLUGIN_ERROR("Failed to add 'response' to the symbol table.");
        return -1;
    }

    query->ast = expr_parse(expr, query->symtab);
    if (query->ast == NULL) {
        return -1;
    }

    return 0;
}

static int dns_config_query(config_item_t *ci, dns_ctx_t *ctx)
{
    dns_query_t **tmp = realloc(ctx->queries, sizeof(*ctx->queries) * (ctx->queries_num + 1));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed");
        return -1;
    }
    ctx->queries = tmp;

    dns_query_t *query = calloc(1, sizeof(*query));
    if (query == NULL) {
        PLUGIN_ERROR("calloc failed");
        return -1;
    }

    ctx->queries[ctx->queries_num] = query;
    ctx->queries_num++;

    int status = cf_util_get_string(ci, &query->query);
    if (status != 0) {
        PLUGIN_ERROR("Missing query argument in  %s:%d.", cf_get_file(ci), cf_get_lineno(ci));
        return status;
    }

    bool convert_ptr = true;
    bool convert_ptr_bit_string = false;

    query->ctx = ctx;
    query->class = C_IN;
    query->type = T_A;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("class", child->key) == 0) {
            status = dns_config_query_class(child, query);
        } else if (strcasecmp("type", child->key) == 0) {
            status = dns_config_query_type(child, query);
        } else if (strcasecmp("convert-ptr", child->key) == 0) {
            status = cf_util_get_boolean(child, &convert_ptr);
        } else if (strcasecmp("convert-ptr-bit-string", child->key) == 0) {
            status = cf_util_get_boolean(child, &convert_ptr_bit_string);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &query->labels);
        } else if (strcasecmp("validate", child->key) == 0) {
            status = dns_config_query_expr(child, query);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0)
        return -1;

    if ((query->type == T_PTR) && (query->class == C_IN) &&
        (convert_ptr || convert_ptr_bit_string)) {
        status = dns_convert_query(query, convert_ptr_bit_string);
        if (status != 0)
            return -1;
    }

    return 0;
}

static void dns_free(void *arg)
{
    dns_ctx_t *ctx = arg;

    if (ctx == NULL)
        return;

    strbuf_destroy(&ctx->servers);

    for (size_t i = 0; i < ctx->queries_num; i++) {
        dns_query_t *query = ctx->queries[i];
        label_set_reset(&query->labels);
        if (query->symtab != NULL)
            expr_symtab_free(query->symtab);
        if (query->ast != NULL)
            expr_node_free(query->ast);
        free(query->query);
        free(query);
    }
    free(ctx->queries);

    for (int i = 0; i < ctx->options.ndomains; i++)
        free(ctx->options.domains[i]);
    free(ctx->options.domains);
#ifdef ARES_OPT_RESOLVCONF
    free(ctx->options.resolvconf_path);
#endif
    free(ctx->instance);
    label_set_reset(&ctx->labels);
    plugin_filter_free(ctx->filter);

    free(ctx);
}

static int dns_config_server(config_item_t *ci, dns_ctx_t *ctx)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    char *server = ci->values[0].value.string;

    int status = 0;
    if (strbuf_len(&ctx->servers) > 0)
        status = strbuf_putchar(&ctx->servers, ',');

    status |= strbuf_putstr(&ctx->servers, server);
    if (status != 0) {
        PLUGIN_ERROR("Failed to add server '%s'.", server);
        return -1;
    }

    ctx->optmask |= ARES_OPT_SERVERS;

    return 0;
}

static int dns_config_option(config_item_t *ci, int *flags, int flag)
{
    bool value = false;
    int status = cf_util_get_boolean(ci, &value);
    if (status == 0) {
        if (value)
            *flags |= flag;
        else
            *flags &= ~flag;
    }
    return status;
}

static int dns_config_option_inverse(config_item_t *ci, int *flags, int flag)
{
    bool value= false;
    int status = cf_util_get_boolean(ci, &value);
    if (status == 0) {
        if (value)
            *flags &= ~flag;
        else
            *flags |= flag;
    }
    return status;
}

static int dns_config_domain(config_item_t *ci, dns_ctx_t *ctx)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    char **tmp = realloc(ctx->options.domains,
                         sizeof(*ctx->options.domains)*(ctx->options.ndomains + 1));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed");
        return -1;
    }

    ctx->options.domains = tmp;
    ctx->options.domains[ctx->options.ndomains] = strdup(ci->values[0].value.string);
    if (ctx->options.domains[ctx->options.ndomains] == NULL) {
        PLUGIN_ERROR("strdup failed");
        return -1;
    }
    ctx->options.ndomains++;

    ctx->optmask |= ARES_OPT_DOMAINS;

    return 0;
}

static int dns_config_instance(config_item_t *ci)
{
    dns_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

//    memcpy(ctx->fams, fams, sizeof(st->fams[0])*FAM_DNS_MAX);
    ctx->timeout = -1;

    ctx->optmask = ARES_OPT_FLAGS;
    ctx->options.flags = ARES_FLAG_NOCHECKRESP | ARES_FLAG_EDNS | ARES_FLAG_NOSEARCH;
    ctx->options.servers = NULL;
    ctx->options.nservers = 0;
    memcpy(ctx->fams, fams, sizeof(ctx->fams[0])*FAM_DNS_MAX);

    int status = cf_util_get_string(ci, &ctx->instance);
    if (status != 0) {
        dns_free(ctx);
        return status;
    }

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("server", child->key) == 0) {
            status = dns_config_server(child, ctx);
        } else if (strcasecmp("domain", child->key) == 0) {
            status = dns_config_domain(child, ctx);
        } else if (strcasecmp("udp-port", child->key) == 0) {
            int port = 0;
            status = cf_util_get_port_number(child, &port);
            ctx->options.udp_port = port;
            ctx->optmask |= ARES_OPT_UDP_PORT;
        } else if (strcasecmp("tcp-port", child->key) == 0) {
            int port = 0;
            status = cf_util_get_port_number(child, &port);
            ctx->options.tcp_port = port;
            ctx->optmask |= ARES_OPT_TCP_PORT;
        } else if (strcasecmp("query", child->key) == 0) {
            status = dns_config_query(child, ctx);
        } else if (strcasecmp("use-vc", child->key) == 0) {
            status = dns_config_option(child, &ctx->options.flags, ARES_FLAG_USEVC);
        } else if (strcasecmp("use-tcp", child->key) == 0) {
            status = dns_config_option(child, &ctx->options.flags, ARES_FLAG_USEVC);
        } else if (strcasecmp("primary", child->key) == 0) {
            status = dns_config_option(child, &ctx->options.flags, ARES_FLAG_PRIMARY);
        } else if (strcasecmp("ignore-truncated", child->key) == 0) {
            status = dns_config_option(child, &ctx->options.flags, ARES_FLAG_IGNTC);
        } else if (strcasecmp("recurse", child->key) == 0) {
            status = dns_config_option_inverse(child, &ctx->options.flags, ARES_FLAG_NORECURSE);
        } else if (strcasecmp("search", child->key) == 0) {
            status = dns_config_option_inverse(child, &ctx->options.flags, ARES_FLAG_NOSEARCH);
        } else if (strcasecmp("aliases", child->key) == 0) {
            status = dns_config_option_inverse(child, &ctx->options.flags, ARES_FLAG_NOALIASES);
        } else if (strcasecmp("edns", child->key) == 0) {
            status = dns_config_option(child, &ctx->options.flags, ARES_FLAG_EDNS);
        } else if (strcasecmp("edns-size", child->key) == 0) {
            status = cf_util_get_int(child, &ctx->options.ednspsz);
            ctx->optmask |= ARES_OPT_EDNSPSZ;
            ctx->options.flags |= ARES_FLAG_EDNS;
        } else if (strcasecmp("resolvconf-path", child->key) == 0) {
#ifdef ARES_OPT_RESOLVCONF
            status = cf_util_get_string(child, &ctx->options.resolvconf_path);
            ctx->optmask |= ARES_OPT_RESOLVCONF;
#else
            PLUGIN_WARNING("Option 'resolvconf-path' is not supported in this version of c-ares");
#endif
        } else if (strcasecmp("timeout", child->key) == 0) {
            cdtime_t timeout = 0;
            status = cf_util_get_cdtime(child, &timeout);
            ctx->optmask |= ARES_OPT_TIMEOUTMS;
            ctx->options.timeout = CDTIME_T_TO_MS(timeout);
        } else if (strcasecmp("tries", child->key) == 0) {
            status = cf_util_get_int(child, &ctx->options.tries);
            ctx->optmask |= ARES_OPT_TRIES;
        } else if (strcasecmp("ndots", child->key) == 0) {
            status = cf_util_get_int(child, &ctx->options.ndots);
            ctx->optmask |= ARES_OPT_NDOTS;
        } else if (strcasecmp("rotate", child->key) == 0) {
            bool flag = false;
            status = cf_util_get_boolean(child, &flag);
#ifdef ARES_OPT_NOROTATE
            ctx->optmask &= ~(ARES_OPT_ROTATE | ARES_OPT_NOROTATE);
            ctx->optmask |= flag ? ARES_OPT_ROTATE : ARES_OPT_NOROTATE;
#else
            if (flag)
                ctx->optmask |= ARES_OPT_ROTATE;
            else
                ctx->optmask &= ~ARES_OPT_ROTATE;
#endif
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = plugin_filter_configure(child, &ctx->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        dns_free(ctx);
        return -1;
    }

    status = label_set_add(&ctx->labels, true, "instance", ctx->instance);
    if (status != 0) {
        PLUGIN_ERROR("Failed to add the 'instance' label.");
        dns_free(ctx);
        return -1;
    }

    if (ctx->labels.num > 0) {
        for (size_t i = 0; i < ctx->queries_num; i++) {
            status = label_set_add_set(&ctx->queries[i]->labels, false, ctx->labels);
            if (status != 0) {
                PLUGIN_ERROR("Failed to add the instance labels to query '%s'.",
                             ctx->queries[i]->query);
                dns_free(ctx);
                return -1;
            }
        }
    }

    return plugin_register_complex_read("dns", ctx->instance, dns_read, interval,
                                        &(user_data_t){.data = ctx, .free_func = dns_free});
}

static int dns_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = dns_config_instance(child);
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

static int dns_init(void)
{
    int status = ares_library_init(ARES_LIB_INIT_ALL);
    if (status != ARES_SUCCESS) {
        PLUGIN_ERROR("ares_library_init: %s", ares_strerror(status));
        return -1;
    }
    return 0;
}

static int dns_shutdown(void)
{
    ares_library_cleanup();
    return 0;
}

void module_register(void)
{
    plugin_register_config("dns", dns_config);
    plugin_register_init("dns", dns_init);
    plugin_register_shutdown("dns", dns_shutdown);
}
