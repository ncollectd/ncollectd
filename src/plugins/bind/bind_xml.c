// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "plugin.h"
#include "libutils/common.h"
#include "plugins/bind/bind_fams.h"
#include "plugins/bind/bind_xml.h"

#include <libxml/parser.h>

int bind_append_metric(metric_family_t *fams, label_set_t *labels,
                       const char *prefix, const char *name,
                       const char *lkey, const char *lvalue, const char *value);

time_t bind_get_timestamp(const char *str, size_t len);

int bind_traffic_histogram_append(histogram_t **rh, const char *maximum, const char *counter);

static void bind_xml_characters(void *ctx, const xmlChar *ch, int len)
{
    bind_xml_ctx_t *sctx = (bind_xml_ctx_t *)ctx;

    if (!sctx->data)
        return;

    char number[256];
    sstrnncpy(number, sizeof(number), (const char *)ch, len);

    switch (sctx->depth) {
    case 4:
        switch (sctx->stack[2]) {
        case BIND_XML_STATISTICS_SERVER_OPCODE:
            metric_family_append(&sctx->fams[FAM_BIND_INCOMING_REQUESTS],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="opcode", .value=sctx->value1},
                                 NULL);
            break;
        case BIND_XML_STATISTICS_SERVER_RCODE:
            metric_family_append(&sctx->fams[FAM_BIND_RESPONSE_RCODES],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="rcode", .value=sctx->value1},
                                 NULL);
            break;
        case BIND_XML_STATISTICS_SERVER_QTYPE:
            metric_family_append(&sctx->fams[FAM_BIND_INCOMING_QUERIES],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="qtype", .value=sctx->value1},
                                 NULL);
            break;
        case BIND_XML_STATISTICS_SERVER_NSSTAT:
            bind_append_metric(sctx->fams, sctx->labels, "nsstats:", sctx->value1,
                                           NULL,  NULL, number);
            break;
        case BIND_XML_STATISTICS_SERVER_ZONESTAT:
            bind_append_metric(sctx->fams, sctx->labels, "zonestat:", sctx->value1,
                                           NULL,  NULL, number);
            break;
        case BIND_XML_STATISTICS_SERVER_SOCKSTAT:
            bind_append_metric(sctx->fams, sctx->labels, "sockstat:", sctx->value1,
                                           NULL,  NULL, number);
            break;
        case BIND_XML_STATISTICS_MEMORY_SUMMARY:
            bind_append_metric(sctx->fams, sctx->labels, "memory:", sctx->value1,
                                           NULL,  NULL, number);
            break;
        default:
            break;
        }
        break;
    case 5:
        switch (sctx->stack[3]) {
        case BIND_XML_STATISTICS_VIEW_RESSTATS:
            bind_append_metric(sctx->fams, sctx->labels, "resstat:", sctx->value2,
                                           "view", sctx->value1, number);
            break;
        case BIND_XML_STATISTICS_VIEW_RESQTYPE:
            metric_family_append(&sctx->fams[FAM_BIND_RESOLVER_QUERIES],
                                 VALUE_COUNTER(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="view", .value=sctx->value1},
                                 &(label_pair_const_t){.name="type", .value=sctx->value2},
                                 NULL);
            break;
        case BIND_XML_STATISTICS_VIEW_CACHESTATS:
            bind_append_metric(sctx->fams, sctx->labels, "cachestats:", sctx->value2,
                                           "view", sctx->value1, number);
            break;
        case BIND_XML_STATISTICS_VIEW_ADBSTAT:
            break;
        default:
            break;
        }
        break;
    case 6:
        switch (sctx->stack[5]) {
        case BIND_XML_STATISTICS_VIEW_CACHE_RRSET_NAME:
            sstrnncpy(sctx->value2, sizeof(sctx->value2),  (const char *)ch, len);
            break;
        case BIND_XML_STATISTICS_VIEW_CACHE_RRSET_COUNTER:
            metric_family_append(&sctx->fams[FAM_BIND_RESOLVER_CACHE_RRSETS],
                                 VALUE_GAUGE(atol(number)), sctx->labels,
                                 &(label_pair_const_t){.name="view", .value=sctx->value1},
                                 &(label_pair_const_t){.name="type", .value=sctx->value2},
                                 NULL);
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV4_UDP_REQUEST_SIZE_COUNTER:
            bind_traffic_histogram_append(&sctx->traffic[BIND_TRAFFIC_INCOMING_REQUESTS_UDP4_SIZE],
                                          sctx->value1, number);
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV4_UDP_RESPONSE_SIZE_COUNTER:
            bind_traffic_histogram_append(&sctx->traffic[BIND_TRAFFIC_RESPONSES_UDP4_SIZE],
                                          sctx->value1, number);
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV4_TCP_REQUEST_SIZE_COUNTER:
            bind_traffic_histogram_append(&sctx->traffic[BIND_TRAFFIC_INCOMING_REQUESTS_TCP4_SIZE],
                                          sctx->value1, number);
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV4_TCP_RESPONSE_SIZE_COUNTER:
            bind_traffic_histogram_append(&sctx->traffic[BIND_TRAFFIC_RESPONSES_TCP4_SIZE],
                                          sctx->value1, number);
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV6_UDP_REQUEST_SIZE_COUNTER:
            bind_traffic_histogram_append(&sctx->traffic[BIND_TRAFFIC_INCOMING_REQUESTS_UDP6_SIZE],
                                          sctx->value1, number);
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV6_UDP_RESPONSE_SIZE_COUNTER:
            bind_traffic_histogram_append(&sctx->traffic[BIND_TRAFFIC_RESPONSES_UDP6_SIZE],
                                          sctx->value1, number);
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV6_TCP_REQUEST_SIZE_COUNTER:
            bind_traffic_histogram_append(&sctx->traffic[BIND_TRAFFIC_INCOMING_REQUESTS_TCP6_SIZE],
                                          sctx->value1, number);
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV6_TCP_RESPONSE_SIZE_COUNTER:
            bind_traffic_histogram_append(&sctx->traffic[BIND_TRAFFIC_RESPONSES_TCP6_SIZE],
                                          sctx->value1, number);
            break;
        default:
            break;
        }
        break;
    }

}

static const char *bind_xml_get_attr(char *attr, int nb_attributes, const xmlChar **attributes,
                                                 int *len)
{
    if (attributes != NULL) {
        for (int i = 0; i < nb_attributes * 5; i += 5) {
          if (strcmp(attr, (const char *)attributes[i]) == 0) {
            const xmlChar *begin = attributes[i + 3];
            const xmlChar *end = attributes[i + 4];
            *len = (int)(end-begin);
            return (const char *)begin;
          }
        }
    }
    return NULL;
}

static void bind_xml_start_element_ns(void *ctx, const xmlChar *localname,
                                      __attribute__((unused)) const xmlChar *prefix,
                                      __attribute__((unused)) const xmlChar *URI,
                                      __attribute__((unused)) int nb_namespaces,
                                      __attribute__((unused)) const xmlChar **namespaces,
                                      int nb_attributes,
                                      __attribute__((unused)) int nb_defaulted,
                                      const xmlChar **attributes)
{
    bind_xml_ctx_t *sctx = (bind_xml_ctx_t *)ctx;
    sctx->depth++;

    switch(sctx->depth) {
    case 1:
        if (strcmp("statistics", (const char *)localname) == 0) {
            sctx->stack[0] = BIND_XML_STATISTICS;
            // get version
        } else {
            sctx->stack[0] = BIND_XML_NONE;
        }
        break;
    case 2:
        if (sctx->stack[0] == BIND_XML_STATISTICS) {
            if (strcmp("server", (const char *)localname) == 0) {
                sctx->stack[1] = BIND_XML_STATISTICS_SERVER;
            } else if (strcmp("views", (const char *)localname) == 0) {
                sctx->stack[1] = BIND_XML_STATISTICS_VIEWS;
            } else if (strcmp("memory", (const char *)localname) == 0) {
                sctx->stack[1] = BIND_XML_STATISTICS_MEMORY;
            } else if (strcmp("traffic", (const char *)localname) == 0) {
                sctx->stack[1] = BIND_XML_STATISTICS_TRAFFIC;
            } else {
                sctx->stack[1] = BIND_XML_NONE;
            }
        }
        break;
    case 3:
        switch(sctx->stack[1]) {
        case BIND_XML_STATISTICS_SERVER:
            if (strcmp("counters", (const char *)localname) == 0) {
                int type_len = 0;
                const char *type = bind_xml_get_attr("type", nb_attributes, attributes, &type_len);
                if (type != NULL) {
                    if ((type_len == strlen("opcode")) &&
                        (strncmp("opcode", type, type_len) == 0)) {
                        sctx->stack[2] = BIND_XML_STATISTICS_SERVER_OPCODE;
                    } else if ((type_len == strlen("rcode")) &&
                               (strncmp("rcode", type, type_len) == 0)) {
                        sctx->stack[2] = BIND_XML_STATISTICS_SERVER_RCODE;
                    } else if ((type_len == strlen("qtype")) &&
                               (strncmp("qtype", type, type_len) == 0)) {
                        sctx->stack[2] = BIND_XML_STATISTICS_SERVER_QTYPE;
                    } else if ((type_len == strlen("nsstat")) &&
                               (strncmp("nsstat", type, type_len) == 0)) {
                        sctx->stack[2] = BIND_XML_STATISTICS_SERVER_NSSTAT;
                    } else if ((type_len == strlen("zonestat")) &&
                                (strncmp("zonestat", type, type_len) == 0)) {
                        sctx->stack[2] = BIND_XML_STATISTICS_SERVER_ZONESTAT;
                    } else if ((type_len == strlen("sockstat")) &&
                               (strncmp("sockstat", type, type_len) == 0)) {
                        sctx->stack[2] = BIND_XML_STATISTICS_SERVER_SOCKSTAT;
                    } else {
                        sctx->stack[2] = BIND_XML_NONE;
                    }
                }
            }
            break;
        case BIND_XML_STATISTICS_VIEWS:
            if (strcmp("view", (const char *)localname) == 0) {
                int name_len = 0;
                const char *name = bind_xml_get_attr("name", nb_attributes, attributes, &name_len);
                if (name != NULL) {
                    sctx->stack[2] = BIND_XML_STATISTICS_VIEW;
                    sstrnncpy(sctx->value1, sizeof(sctx->value1), name, name_len);
                } else {
                    sctx->stack[2] = BIND_XML_NONE;
                }
            } else {
                sctx->stack[2] = BIND_XML_NONE;
            }
            break;
        case BIND_XML_STATISTICS_MEMORY:
            if (strcmp("summary", (const char *)localname) == 0) {
                sctx->stack[2] = BIND_XML_STATISTICS_MEMORY_SUMMARY;
            } else {
                sctx->stack[2] = BIND_XML_NONE;
            }
            break;
        case BIND_XML_STATISTICS_TRAFFIC:
            if (strcmp("ipv4", (const char *)localname) == 0) {
                sctx->stack[2] = BIND_XML_STATISTICS_TRAFFIC_IPV4;
            } else if (strcmp("ipv6", (const char *)localname) == 0) {
                sctx->stack[2] = BIND_XML_STATISTICS_TRAFFIC_IPV6;
            } else {
                sctx->stack[2] = BIND_XML_NONE;
            }
        default:
            break;
        }
        break;
    case 4:
        switch (sctx->stack[2]) {
        case BIND_XML_STATISTICS_SERVER_OPCODE:
        case BIND_XML_STATISTICS_SERVER_RCODE:
        case BIND_XML_STATISTICS_SERVER_QTYPE:
        case BIND_XML_STATISTICS_SERVER_NSSTAT:
        case BIND_XML_STATISTICS_SERVER_ZONESTAT:
        case BIND_XML_STATISTICS_SERVER_SOCKSTAT:
            if (strcmp("counter", (const char *)localname) == 0) {
                int name_len = 0;
                const char *name = bind_xml_get_attr("name", nb_attributes, attributes, &name_len);
                if (name != NULL) {
                    sstrnncpy(sctx->value1, sizeof(sctx->value1), name, name_len);
                    sctx->data = true;
                }
            }
            break;
        case BIND_XML_STATISTICS_VIEW:
            if (strcmp("counters", (const char *)localname) == 0) {
                int type_len = 0;
                const char *type = bind_xml_get_attr("type", nb_attributes, attributes, &type_len);
                if (type != NULL) {
                    if ((type_len == strlen("resqtype")) &&
                        (strncmp("resqtype", type, type_len) == 0)) {
                       sctx->stack[3] = BIND_XML_STATISTICS_VIEW_RESQTYPE;
                    } else if ((type_len == strlen("resstats")) &&
                        (strncmp("resstats", type, type_len) == 0)) {
                       sctx->stack[3] = BIND_XML_STATISTICS_VIEW_RESSTATS;
                    } else if ((type_len == strlen("adbstat")) &&
                        (strncmp("adbstat", type, type_len) == 0)) {
                       sctx->stack[3] = BIND_XML_STATISTICS_VIEW_ADBSTAT;
                    } else if ((type_len == strlen("cachestats")) &&
                        (strncmp("cachestats", type, type_len) == 0)) {
                       sctx->stack[3] = BIND_XML_STATISTICS_VIEW_CACHESTATS;
                    } else {
                       sctx->stack[3] = BIND_XML_NONE;
                    }
                } else {
                    sctx->stack[3] = BIND_XML_NONE;
                }
            } else if (strcmp("cache", (const char *)localname) == 0) {
                sctx->stack[3] = BIND_XML_STATISTICS_VIEW_CACHE;
            } else {
                sctx->stack[3] = BIND_XML_NONE;
            }
            break;
        case BIND_XML_STATISTICS_MEMORY_SUMMARY:
            sstrncpy(sctx->value1, (const char *)localname, sizeof(sctx->value1));
            sctx->data = true;
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV4:
            if (strcmp("udp", (const char *)localname) == 0) {
                sctx->stack[3] = BIND_XML_STATISTICS_TRAFFIC_IPV4_UDP;
            } else if (strcmp("tcp", (const char *)localname) == 0) {
                sctx->stack[3] = BIND_XML_STATISTICS_TRAFFIC_IPV4_TCP;
            } else {
                sctx->stack[3] = BIND_XML_NONE;
            }
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV6:
            if (strcmp("udp", (const char *)localname) == 0) {
                sctx->stack[3] = BIND_XML_STATISTICS_TRAFFIC_IPV6_UDP;
            } else if (strcmp("tcp", (const char *)localname) == 0) {
                sctx->stack[3] = BIND_XML_STATISTICS_TRAFFIC_IPV6_TCP;
            } else {
                sctx->stack[3] = BIND_XML_NONE;
            }
            break;
        default:
            break;
        }
        break;
    case 5:
        switch (sctx->stack[3]) {
        case BIND_XML_STATISTICS_VIEW_RESQTYPE:
        case BIND_XML_STATISTICS_VIEW_RESSTATS:
        case BIND_XML_STATISTICS_VIEW_ADBSTAT:
        case BIND_XML_STATISTICS_VIEW_CACHESTATS:
            if (strcmp("counter", (const char *)localname) == 0) {
                int name_len = 0;
                const char *name = bind_xml_get_attr("name", nb_attributes, attributes, &name_len);
                if (name != NULL) {
                  sstrnncpy(sctx->value2, sizeof(sctx->value2), name, name_len);
                  sctx->data = true;
                }
            }
            break;
        case BIND_XML_STATISTICS_VIEW_CACHE:
            if (strcmp("rrset", (const char *)localname) == 0) {
                sctx->stack[4] = BIND_XML_STATISTICS_VIEW_CACHE_RRSET;
            } else {
                sctx->stack[4] = BIND_XML_NONE;
            }
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV4_UDP:
            if (strcmp("counters", (const char *)localname) == 0) {
                int type_len = 0;
                const char *type = bind_xml_get_attr("type", nb_attributes, attributes, &type_len);
                if (type != NULL) {
                    if (strncmp("request-size", type, type_len) == 0) {
                        sctx->stack[4] = BIND_XML_STATISTICS_TRAFFIC_IPV4_UDP_REQUEST_SIZE;
                    } else if (strncmp("response-size", type, type_len) == 0) {
                        sctx->stack[4] = BIND_XML_STATISTICS_TRAFFIC_IPV4_UDP_RESPONSE_SIZE;
                    } else {
                        sctx->stack[4] = BIND_XML_NONE;
                    }
                } else {
                    sctx->stack[4] = BIND_XML_NONE;
                }
            } else {
                sctx->stack[4] = BIND_XML_NONE;
            }
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV4_TCP:
            if (strcmp("counters", (const char *)localname) == 0) {
                int type_len = 0;
                const char *type = bind_xml_get_attr("type", nb_attributes, attributes, &type_len);
                if (type != NULL) {
                    if (strncmp("request-size", type, type_len) == 0) {
                        sctx->stack[4] = BIND_XML_STATISTICS_TRAFFIC_IPV4_TCP_REQUEST_SIZE;
                    } else if (strncmp("response-size", type, type_len) == 0) {
                        sctx->stack[4] = BIND_XML_STATISTICS_TRAFFIC_IPV4_TCP_RESPONSE_SIZE;
                    } else {
                        sctx->stack[4] = BIND_XML_NONE;
                    }
                } else {
                    sctx->stack[4] = BIND_XML_NONE;
                }
            } else {
                sctx->stack[4] = BIND_XML_NONE;
            }
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV6_UDP:
            if (strcmp("counters", (const char *)localname) == 0) {
                int type_len = 0;
                const char *type = bind_xml_get_attr("type", nb_attributes, attributes, &type_len);
                if (type != NULL) {
                    if (strncmp("request-size", type, type_len) == 0) {
                        sctx->stack[4] = BIND_XML_STATISTICS_TRAFFIC_IPV6_UDP_REQUEST_SIZE;
                    } else if (strncmp("response-size", type, type_len) == 0) {
                        sctx->stack[4] = BIND_XML_STATISTICS_TRAFFIC_IPV6_UDP_RESPONSE_SIZE;
                    } else {
                        sctx->stack[4] = BIND_XML_NONE;
                    }
                } else {
                    sctx->stack[4] = BIND_XML_NONE;
                }
            } else {
                sctx->stack[4] = BIND_XML_NONE;
            }
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV6_TCP:
            if (strcmp("counters", (const char *)localname) == 0) {
                int type_len = 0;
                const char *type = bind_xml_get_attr("type", nb_attributes, attributes, &type_len);
                if (type != NULL) {
                    if (strncmp("request-size", type, type_len) == 0) {
                        sctx->stack[4] = BIND_XML_STATISTICS_TRAFFIC_IPV6_TCP_REQUEST_SIZE;
                    } else if (strncmp("response-size", type, type_len) == 0) {
                        sctx->stack[4] = BIND_XML_STATISTICS_TRAFFIC_IPV6_TCP_RESPONSE_SIZE;
                    } else {
                        sctx->stack[4] = BIND_XML_NONE;
                    }
                } else {
                    sctx->stack[4] = BIND_XML_NONE;
                }
            } else {
                sctx->stack[4] = BIND_XML_NONE;
            }
            break;
        default:
            break;
        }
        break;
    case 6:
        switch (sctx->stack[4]) {
        case BIND_XML_STATISTICS_VIEW_CACHE_RRSET:
            if (strcmp("name", (const char *)localname) == 0) {
                sctx->stack[5] = BIND_XML_STATISTICS_VIEW_CACHE_RRSET_NAME;
                sctx->data = true;
            } else if (strcmp("counter", (const char *)localname) == 0) {
                sctx->stack[5] = BIND_XML_STATISTICS_VIEW_CACHE_RRSET_COUNTER;
                sctx->data = true;
            } else {
                sctx->stack[5] = BIND_XML_NONE;
            }
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV4_UDP_REQUEST_SIZE:
            if (strcmp("counter", (const char *)localname) == 0) {
                int name_len = 0;
                const char *name = bind_xml_get_attr("name", nb_attributes, attributes, &name_len);
                if (name != NULL) {
                    sstrnncpy(sctx->value1, sizeof(sctx->value1), name, name_len);
                    sctx->stack[5] = BIND_XML_STATISTICS_TRAFFIC_IPV4_UDP_REQUEST_SIZE_COUNTER;
                    sctx->data = true;
                }
            } else {
                sctx->stack[5] = BIND_XML_NONE;
            }
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV4_UDP_RESPONSE_SIZE:
            if (strcmp("counter", (const char *)localname) == 0) {
                int name_len = 0;
                const char *name = bind_xml_get_attr("name", nb_attributes, attributes, &name_len);
                if (name != NULL) {
                    sstrnncpy(sctx->value1, sizeof(sctx->value1), name, name_len);
                    sctx->stack[5] = BIND_XML_STATISTICS_TRAFFIC_IPV4_UDP_RESPONSE_SIZE_COUNTER;
                    sctx->data = true;
                }
            } else {
                sctx->stack[5] = BIND_XML_NONE;
            }
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV4_TCP_REQUEST_SIZE:
            if (strcmp("counter", (const char *)localname) == 0) {
                int name_len = 0;
                const char *name = bind_xml_get_attr("name", nb_attributes, attributes, &name_len);
                if (name != NULL) {
                    sstrnncpy(sctx->value1, sizeof(sctx->value1), name, name_len);
                    sctx->stack[5] = BIND_XML_STATISTICS_TRAFFIC_IPV4_TCP_REQUEST_SIZE_COUNTER;
                    sctx->data = true;
                }
            } else {
                sctx->stack[5] = BIND_XML_NONE;
            }
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV4_TCP_RESPONSE_SIZE:
            if (strcmp("counter", (const char *)localname) == 0) {
                int name_len = 0;
                const char *name = bind_xml_get_attr("name", nb_attributes, attributes, &name_len);
                if (name != NULL) {
                    sstrnncpy(sctx->value1, sizeof(sctx->value1), name, name_len);
                    sctx->stack[5] = BIND_XML_STATISTICS_TRAFFIC_IPV4_TCP_RESPONSE_SIZE_COUNTER;
                    sctx->data = true;
                }
            } else {
                sctx->stack[5] = BIND_XML_NONE;
            }
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV6_UDP_REQUEST_SIZE:
            if (strcmp("counter", (const char *)localname) == 0) {
                int name_len = 0;
                const char *name = bind_xml_get_attr("name", nb_attributes, attributes, &name_len);
                if (name != NULL) {
                    sstrnncpy(sctx->value1, sizeof(sctx->value1), name, name_len);
                     sctx->stack[5] = BIND_XML_STATISTICS_TRAFFIC_IPV6_UDP_REQUEST_SIZE_COUNTER;
                    sctx->data = true;
                }
            } else {
                sctx->stack[5] = BIND_XML_NONE;
            }
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV6_UDP_RESPONSE_SIZE:
            if (strcmp("counter", (const char *)localname) == 0) {
                int name_len = 0;
                const char *name = bind_xml_get_attr("name", nb_attributes, attributes, &name_len);
                if (name != NULL) {
                    sstrnncpy(sctx->value1, sizeof(sctx->value1), name, name_len);
                    sctx->stack[5] = BIND_XML_STATISTICS_TRAFFIC_IPV6_UDP_RESPONSE_SIZE_COUNTER;
                    sctx->data = true;
                }
            } else {
                sctx->stack[5] = BIND_XML_NONE;
            }
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV6_TCP_REQUEST_SIZE:
            if (strcmp("counter", (const char *)localname) == 0) {
                int name_len = 0;
                const char *name = bind_xml_get_attr("name", nb_attributes, attributes, &name_len);
                if (name != NULL) {
                   sstrnncpy(sctx->value1, sizeof(sctx->value1), name, name_len);
                   sctx->stack[5] = BIND_XML_STATISTICS_TRAFFIC_IPV6_TCP_REQUEST_SIZE_COUNTER;
                   sctx->data = true;
                }
            } else {
                sctx->stack[5] = BIND_XML_NONE;
            }
            break;
        case BIND_XML_STATISTICS_TRAFFIC_IPV6_TCP_RESPONSE_SIZE:
            if (strcmp("counter", (const char *)localname) == 0) {
                int name_len = 0;
                const char *name = bind_xml_get_attr("name", nb_attributes, attributes, &name_len);
                if (name != NULL) {
                   sstrnncpy(sctx->value1, sizeof(sctx->value1), name, name_len);
                   sctx->stack[5] = BIND_XML_STATISTICS_TRAFFIC_IPV6_TCP_RESPONSE_SIZE_COUNTER;
                   sctx->data = true;
                }
            } else {
                sctx->stack[5] = BIND_XML_NONE;
            }
            break;
        default:
            break;
        }
        break;
    }
}


static void bind_xml_end_element_ns(void *ctx,
                                    __attribute__((unused)) const xmlChar *localname,
                                    __attribute__((unused)) const xmlChar *prefix,
                                    __attribute__((unused)) const xmlChar *URI)
{
    bind_xml_ctx_t *sctx = (bind_xml_ctx_t *)ctx;
    sctx->stack[sctx->depth-1] = BIND_XML_NONE;
    sctx->depth--;
    sctx->data = false;
}

static xmlSAXHandler bind_xml_handler = {
    .internalSubset        = NULL,
    .isStandalone          = NULL,
    .hasInternalSubset     = NULL,
    .hasExternalSubset     = NULL,
    .resolveEntity         = NULL,
    .getEntity             = NULL,
    .entityDecl            = NULL,
    .notationDecl          = NULL,
    .attributeDecl         = NULL,
    .elementDecl           = NULL,
    .unparsedEntityDecl    = NULL,
    .setDocumentLocator    = NULL,
    .startDocument         = NULL,
    .endDocument           = NULL,
    .startElement          = NULL,
    .endElement            = NULL,
    .reference             = NULL,
    .characters            = bind_xml_characters,
    .ignorableWhitespace   = NULL,
    .processingInstruction = NULL,
    .comment               = NULL,
    .warning               = NULL,
    .error                 = NULL,
    .fatalError            = NULL,
    .getParameterEntity    = NULL,
    .cdataBlock            = NULL,
    .externalSubset        = NULL,
    .initialized           = XML_SAX2_MAGIC,
    ._private              = NULL,
    .startElementNs        = bind_xml_start_element_ns,
    .endElementNs          = bind_xml_end_element_ns,
    .serror                = NULL
};

int bind_xml_parse(bind_xml_ctx_t *ctx)
{
    xmlParserCtxtPtr ctxt;

    ctxt = xmlCreatePushParserCtxt(&bind_xml_handler, (void *)ctx, NULL, 0, NULL);
    if (ctxt == NULL)
        return -1;
    ctx->ctxt = ctxt;
    return 0;
}

int bind_xml_parse_chunk(bind_xml_ctx_t *ctx, const char *data, int size)
{
    xmlParserCtxtPtr ctxt = ctx->ctxt;
    return xmlParseChunk(ctxt, data, size, 0);
}

int bind_xml_parse_end(bind_xml_ctx_t *ctx)
{
    xmlParserCtxtPtr ctxt = ctx->ctxt;

    int status = xmlParseChunk(ctxt, NULL, 0, 1);
    if (status != 0)
        PLUGIN_ERROR("xmlSAXUserParseFile returned error %d\n", status);

    xmlFreeParserCtxt(ctxt);
    ctx->ctxt = NULL;

    return 0;
}
