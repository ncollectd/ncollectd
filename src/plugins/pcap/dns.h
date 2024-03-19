/* SPDX-License-Identifier: GPL-2.0-only or BSD-3-Clause                                      */
/* SPDX-FileCopyrightText: Copyright (C) 2002 The Measurement Factory, Inc.                   */
/* SPDX-FileCopyrightText: Copyright (C) 2006-2011 Florian octo Forster                       */
/* SPDX-FileCopyrightText: Copyright (C) 2009 Mirko Buffoni                                   */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org>                          */
/* SPDX-FileContributor: Mirko Buffoni <briareos at eswat.org>                                */
/* SPDX-FileContributor: The Measurement Factory, Inc. <http://www.measurement-factory.com/>  */

#pragma once

enum {
    FAM_PCAP_DNS_QUERIES,
    FAM_PCAP_DNS_RESPONSES,
    FAM_PCAP_DNS_QUERY_TYPES,
    FAM_PCAP_DNS_OPERATION_CODES,
    FAM_PCAP_DNS_RESPONSE_CODES,
    FAM_PCAP_DNS_MAX,
};

struct counter_list_s {
    unsigned int key;
    unsigned int value;
    struct counter_list_s *next;
};
typedef struct counter_list_s counter_list_t;

typedef struct {
    uint64_t tr_queries;
    uint64_t tr_responses;
    counter_list_t *qtype_list;
    counter_list_t *opcode_list;
    counter_list_t *rcode_list;
    /* The `traffic' mutex if for `tr_queries' and `tr_responses' */
    pthread_mutex_t traffic_mutex;
    pthread_mutex_t qtype_mutex;
    pthread_mutex_t opcode_mutex;
    pthread_mutex_t rcode_mutex;
    metric_family_t fams[FAM_PCAP_DNS_MAX];
} nc_dns_ctx_t;

int handle_dns(nc_dns_ctx_t *ctx, const char *buf, int len);

int nc_dns_read(nc_dns_ctx_t *ctx, label_set_t *labels);

int nc_dns_init(nc_dns_ctx_t *ctx);
