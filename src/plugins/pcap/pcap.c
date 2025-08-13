// SPDX-License-Identifier: GPL-2.0-only OR BSD-3-Clause
// SPDX-FileCopyrightText: Copyright (C) 2002 The Measurement Factory, Inc.
// SPDX-FileCopyrightText: Copyright (C) 2006-2011 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009 Mirko Buffon
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Mirko Buffoni <briareos at eswat.org>
// SPDX-FileContributor: The Measurement Factory, Inc. <http://www.measurement-factory.com/>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#define _DEFAULT_SOURCE
// #define _BSD_SOURCE

#include "plugin.h"
#include "libutils/common.h"

#include <poll.h>
#ifdef HAVE_NET_BPF_H
#    include <net/bpf.h>
#endif
#include <pcap.h>

#ifdef HAVE_SYS_CAPABILITY_H
#    include <sys/capability.h>
#endif

#ifdef HAVE_NET_IF_ARP_H
#    include <net/if_arp.h>
#endif
#ifdef HAVE_NET_IF_H
#    include <net/if.h>
#endif
#ifdef HAVE_NET_PPP_DEFS_H
#    include <net/ppp_defs.h>
#endif
#ifdef HAVE_NET_IF_PPP_H
#    include <net/if_ppp.h>
#endif

#ifdef HAVE_NETINET_IN_SYSTM_H
#    include <netinet/in_systm.h>
#endif
#ifdef HAVE_NETINET_IN_H
#    include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IP_H
#    include <netinet/ip.h>
#endif
#ifdef HAVE_NETINET_IP6_H
#    include <netinet/ip6.h>
#endif
#ifdef HAVE_NETINET_IF_ETHER_H
#    include <netinet/if_ether.h>
#endif
#ifdef HAVE_NETINET_IP_VAR_H
#    include <netinet/ip_var.h>
#endif
#ifdef HAVE_NETINET_UDP_H
#    include <netinet/udp.h>
#endif

#ifdef HAVE_ARPA_INET_H
#    include <arpa/inet.h>
#endif

#ifdef HAVE_NETDB_H
#    include <netdb.h>
#endif

#define PCAP_SNAPLEN 1460
#ifndef ETHER_HDR_LEN
#    define ETHER_ADDR_LEN 6
#    define ETHER_TYPE_LEN 2
#    define ETHER_HDR_LEN (ETHER_ADDR_LEN * 2 + ETHER_TYPE_LEN)
#endif
#ifndef ETHERTYPE_8021Q
#    define ETHERTYPE_8021Q 0x8100
#endif
#ifndef ETHERTYPE_IPV6
#    define ETHERTYPE_IPV6 0x86DD
#endif

#ifndef PPP_ADDRESS_VAL
#    define PPP_ADDRESS_VAL 0xff /* The address byte value */
#endif
#ifndef PPP_CONTROL_VAL
#    define PPP_CONTROL_VAL 0x03 /* The control byte value */
#endif

#if defined(HAVE_STRUCT_UDPHDR_UH_DPORT) && defined(HAVE_STRUCT_UDPHDR_UH_SPORT)
#    define UDP_DEST uh_dport
#    define UDP_SRC uh_sport
#elif defined(HAVE_STRUCT_UDPHDR_DEST) && defined(HAVE_STRUCT_UDPHDR_SOURCE)
#    define UDP_DEST dest
#    define UDP_SRC source
#else
#    error "'struct udphdr' is unusable."
#endif

#if defined(HAVE_NETINET_IP6_H) && defined(HAVE_STRUCT_IP6_EXT)
#    define HAVE_IPV6 1
#endif

#include "plugins/pcap/dns.h"

#pragma GCC diagnostic ignored "-Wcast-align"

struct ip_list_s {
    struct in6_addr addr;
    void *data;
    struct ip_list_s *next;
};
typedef struct ip_list_s ip_list_t;

typedef struct {
    char *name;
    char *interface;
    bool promiscuous;
    char *filter;
    cdtime_t interval;
    label_set_t labels;
    ip_list_t *ignore_src;
    ip_list_t *ignore_dst;
    pcap_t *pcap_obj;
    pthread_t listen_thread;
    int listen_thread_init;
    nc_dns_ctx_t dns;
} nc_pcap_ctx_t;

#define PCAP_SNAPLEN 1460
// static int select_numeric_qtype = 1;


#ifndef PCAP_ERROR
#    define PCAP_ERROR                         -1  /* generic error code */
#endif
#ifndef PCAP_ERROR_BREAK
#    define PCAP_ERROR_BREAK                   -2  /* loop terminated by pcap_breakloop */
#endif
#ifndef PCAP_ERROR_NOT_ACTIVATED
#    define PCAP_ERROR_NOT_ACTIVATED           -3  /* the capture needs to be activated */
#endif
#ifndef PCAP_ERROR_ACTIVATED
#    define PCAP_ERROR_ACTIVATED               -4  /* the operation can't be performed on already activated captures */
#endif
#ifndef PCAP_ERROR_NO_SUCH_DEVICE
#    define PCAP_ERROR_NO_SUCH_DEVICE          -5  /* no such device exists */
#endif
#ifndef PCAP_ERROR_RFMON_NOTSUP
#    define PCAP_ERROR_RFMON_NOTSUP            -6  /* this device doesn't support rfmon (monitor) mode */
#endif
#ifndef PCAP_ERROR_NOT_RFMON
#    define PCAP_ERROR_NOT_RFMON               -7  /* operation supported only in monitor mode */
#endif
#ifndef PCAP_ERROR_PERM_DENIED
#    define PCAP_ERROR_PERM_DENIED             -8  /* no permission to open the device */
#endif
#ifndef PCAP_ERROR_IFACE_NOT_UP
#    define PCAP_ERROR_IFACE_NOT_UP            -9  /* interface isn't up */
#endif
#ifndef PCAP_ERROR_CANTSET_TSTAMP_TYPE
#    define PCAP_ERROR_CANTSET_TSTAMP_TYPE     -10 /* this device doesn't support setting the time stamp type */
#endif
#ifndef PCAP_ERROR_PROMISC_PERM_DENIED
#    define PCAP_ERROR_PROMISC_PERM_DENIED     -11 /* you don't have permission to capture in promiscuous mode */
#endif
#ifndef PCAP_ERROR_TSTAMP_PRECISION_NOTSUP
#    define PCAP_ERROR_TSTAMP_PRECISION_NOTSUP -12 /* the requested time stamp precision is not supported */
#endif

const char *get_pcap_error(pcap_t *p, int status)
{
#ifdef HAVE_PCAP_STATUSTOSTR
    (void)p;
    return pcap_statustostr(status);
#else
    (void)status;
    if(p != NULL)
        return pcap_geterr(p);
#endif
    /* coverity[UNREACHABLE] */
    return "unknown error";
}

static int cmp_in6_addr(const struct in6_addr *a, const struct in6_addr *b)
{
    int i;

    assert(sizeof(struct in6_addr) == 16);

    for (i = 0; i < 16; i++)
        if (a->s6_addr[i] != b->s6_addr[i])
            break;

    if (i >= 16)
        return 0;

    return a->s6_addr[i] > b->s6_addr[i] ? 1 : -1;
}

static inline int ignore_list_match(ip_list_t *list, const struct in6_addr *addr)
{
    for (ip_list_t *ptr = list; ptr != NULL; ptr = ptr->next) {
        if (cmp_in6_addr(addr, &ptr->addr) == 0)
            return 1;
    }

    return 0;
}

static void ignore_list_add(ip_list_t **list, const struct in6_addr *addr)
{
    if (ignore_list_match(*list, addr) != 0)
        return;

    ip_list_t *new = malloc(sizeof(*new));
    if (new == NULL) {
        PLUGIN_ERROR("malloc failed");
        return;
    }

    memcpy(&new->addr, addr, sizeof(struct in6_addr));
    new->next = *list;

    *list = new;
}

static void ignore_list_add_name(ip_list_t **list, const char *name)
{
    struct addrinfo *ai_list;
    struct in6_addr addr;

    int status = getaddrinfo(name, NULL, NULL, &ai_list);
    if (status != 0)
        return;

    for (struct addrinfo *ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next) {
        if (ai_ptr->ai_family == AF_INET) {
            memset(&addr, '\0', sizeof(addr));
            addr.s6_addr[10] = 0xFF;
            addr.s6_addr[11] = 0xFF;
            memcpy(addr.s6_addr + 12, &((struct sockaddr_in *)ai_ptr->ai_addr)->sin_addr, 4);

            ignore_list_add(list, &addr);
        } else {
            ignore_list_add(list, &((struct sockaddr_in6 *)ai_ptr->ai_addr)->sin6_addr);
        }
    }

    freeaddrinfo(ai_list);
}

static int handle_udp(nc_pcap_ctx_t *ctx, const struct udphdr *udp, int len)
{
    char buf[PCAP_SNAPLEN];
    if ((ntohs(udp->UDP_DEST) != 53) && (ntohs(udp->UDP_SRC) != 53))
        return 0;
    memcpy(buf, udp + 1, len - sizeof(*udp));
    if (handle_dns(&ctx->dns, buf, len - sizeof(*udp)) == 0)
        return 0;
    return 1;
}

#ifdef HAVE_IPV6
static int handle_ipv6(nc_pcap_ctx_t *ctx, const struct ip6_hdr *ipv6, int len)
{
    char buf[PCAP_SNAPLEN];
    unsigned int offset;
    int nexthdr;

    struct in6_addr c_src_addr;
    uint16_t payload_len;

    if (0 > len)
        return 0;

    offset = sizeof(struct ip6_hdr);
    nexthdr = ipv6->ip6_nxt;
    c_src_addr = ipv6->ip6_src;
    payload_len = ntohs(ipv6->ip6_plen);

    if (ignore_list_match(ctx->ignore_src, &c_src_addr))
        return 0;

    /* Parse extension headers. This only handles the standard headers, as
     * defined in RFC 2460, correctly. Fragments are discarded. */
    while ((IPPROTO_ROUTING == nexthdr)  || /* routing header */
           (IPPROTO_HOPOPTS == nexthdr)  || /* Hop-by-Hop options. */
           (IPPROTO_FRAGMENT == nexthdr) || /* fragmentation header. */
           (IPPROTO_DSTOPTS == nexthdr)  || /* destination options. */
           (IPPROTO_AH == nexthdr)       || /* destination options. */
           (IPPROTO_ESP == nexthdr)) {      /* encapsulating security payload. */
        struct ip6_ext ext_hdr;
        uint16_t ext_hdr_len;

        /* Catch broken packets */
        if ((offset + sizeof(struct ip6_ext)) > (unsigned int)len)
            return 0;

        /* Cannot handle fragments. */
        if (nexthdr == IPPROTO_FRAGMENT)
            return 0;

        memcpy(&ext_hdr, (const char *)ipv6 + offset, sizeof(struct ip6_ext));
        nexthdr = ext_hdr.ip6e_nxt;
        ext_hdr_len = (8 * (ntohs(ext_hdr.ip6e_len) + 1));

        /* This header is longer than the packets payload.. WTF? */
        if (ext_hdr_len > payload_len)
            return 0;

        offset += ext_hdr_len;
        payload_len -= ext_hdr_len;
    }

    /* Catch broken and empty packets */
    if (((offset + payload_len) > (unsigned int)len) || (payload_len == 0) ||
        (payload_len > PCAP_SNAPLEN))
        return 0;

    if (nexthdr != IPPROTO_UDP)
        return 0;

    memcpy(buf, (const char *)ipv6 + offset, payload_len);
    if (handle_udp(ctx, (struct udphdr *)buf, payload_len) == 0)
        return 0;

    return 1;
}
#else
static int handle_ipv6(__attribute__((unused)) nc_pcap_ctx_t *ctx,
                       __attribute__((unused)) const void *pkg, __attribute__((unused)) int len)
{
    return 0;
}
#endif

static void in6_addr_from_buffer(struct in6_addr *ia, const void *buf, size_t buf_len, int family)
{
    memset(ia, 0, sizeof(struct in6_addr));
    if ((AF_INET == family) && (sizeof(uint32_t) == buf_len)) {
        ia->s6_addr[10] = 0xFF;
        ia->s6_addr[11] = 0xFF;
        memcpy(ia->s6_addr + 12, buf, buf_len);
    } else if ((AF_INET6 == family) && (sizeof(struct in6_addr) == buf_len)) {
        memcpy(ia, buf, buf_len);
    }
}

static int handle_ip(nc_pcap_ctx_t *ctx, const struct ip *ip, int len)
{
    char buf[PCAP_SNAPLEN];
    int offset = ip->ip_hl << 2;
    struct in6_addr c_src_addr;
    struct in6_addr c_dst_addr;

    if (ip->ip_v == 6)
        return handle_ipv6(ctx, (const void *)ip, len);

    in6_addr_from_buffer(&c_src_addr, &ip->ip_src.s_addr, sizeof(ip->ip_src.s_addr), AF_INET);
    in6_addr_from_buffer(&c_dst_addr, &ip->ip_dst.s_addr, sizeof(ip->ip_dst.s_addr), AF_INET);
    if (ignore_list_match(ctx->ignore_src, &c_src_addr))
        return 0;
    if (IPPROTO_UDP != ip->ip_p)
        return 0;
    memcpy(buf, ((const char *)ip) + offset, len - offset);
    if (handle_udp(ctx, (struct udphdr *)buf, len - offset) == 0)
        return 0;
    return 1;
}

#ifdef HAVE_NET_IF_PPP_H
static int handle_ppp(nc_pcap_ctx_t *ctx, const u_char *pkt, int len)
{
    char buf[PCAP_SNAPLEN];
    unsigned short us;
    unsigned short proto;
    if (len < 2)
        return 0;
    if ((*pkt == PPP_ADDRESS_VAL) && (*(pkt + 1) == PPP_CONTROL_VAL)) {
        pkt += 2; /* ACFC not used */
        len -= 2;
    }
    if (len < 2)
        return 0;
    if (*pkt % 2) {
        proto = *pkt; /* PFC is used */
        pkt++;
        len--;
    } else {
        memcpy(&us, pkt, sizeof(us));
        proto = ntohs(us);
        pkt += 2;
        len -= 2;
    }
    if (ETHERTYPE_IP != proto && PPP_IP != proto)
        return 0;
    memcpy(buf, pkt, len);
    return handle_ip(ctx, (struct ip *)buf, len);
}
#endif

static int handle_null(nc_pcap_ctx_t *ctx, const u_char *pkt, int len)
{
    unsigned int family;
    memcpy(&family, pkt, sizeof(family));
    if (AF_INET != family)
        return 0;
    return handle_ip(ctx, (const struct ip *)(pkt + 4), len - 4);
}

#ifdef DLT_LOOP
static int handle_loop(nc_pcap_ctx_t *ctx, const u_char *pkt, int len)
{
    unsigned int family;
    memcpy(&family, pkt, sizeof(family));
    if (AF_INET != ntohl(family))
        return 0;
    return handle_ip(ctx, (const struct ip *)(pkt + 4), len - 4);
}

#endif

#ifdef DLT_RAW
static int handle_raw(nc_pcap_ctx_t *ctx, const u_char *pkt, int len)
{
    return handle_ip(ctx, (const struct ip *)pkt, len);
}

#endif

static int handle_ether(nc_pcap_ctx_t *ctx, const u_char *pkt, int len)
{
    char buf[PCAP_SNAPLEN];
    const struct ether_header *e = (const void *)pkt;
    unsigned short etype = ntohs(e->ether_type);
    if (len < ETHER_HDR_LEN)
        return 0;
    pkt += ETHER_HDR_LEN;
    len -= ETHER_HDR_LEN;
    if (ETHERTYPE_8021Q == etype) {
        etype = ntohs(*(const unsigned short *)(pkt + 2));
        pkt += 4;
        len -= 4;
    }
    if ((ETHERTYPE_IP != etype) && (ETHERTYPE_IPV6 != etype))
        return 0;
    memcpy(buf, pkt, len);
    if (ETHERTYPE_IPV6 == etype)
        return handle_ipv6(ctx, (void *)buf, len);
    else
        return handle_ip(ctx, (struct ip *)buf, len);
}

#ifdef DLT_LINUX_SLL
static int handle_linux_sll(nc_pcap_ctx_t *ctx, const u_char *pkt, int len)
{
    const struct sll_header {
        uint16_t pkt_type;
        uint16_t dev_type;
        uint16_t addr_len;
        uint8_t addr[8];
        uint16_t proto_type;
    } * hdr;
    uint16_t etype;

    if ((0 > len) || ((unsigned int)len < sizeof(struct sll_header)))
        return 0;

    hdr = (const struct sll_header *)pkt;
    pkt = (const u_char *)(hdr + 1);
    len -= sizeof(struct sll_header);

    etype = ntohs(hdr->proto_type);

    if ((ETHERTYPE_IP != etype) && (ETHERTYPE_IPV6 != etype))
        return 0;

    if (ETHERTYPE_IPV6 == etype)
        return handle_ipv6(ctx, (const void *)pkt, len);
    else
        return handle_ip(ctx, (const struct ip *)pkt, len);
}
#endif

static void handle_pcap(u_char *udata, const struct pcap_pkthdr *hdr, const u_char *pkt)
{
    int status;
    nc_pcap_ctx_t *ctx = (nc_pcap_ctx_t *)udata;

    if (hdr->caplen < ETHER_HDR_LEN)
        return;

    switch (pcap_datalink(ctx->pcap_obj)) {
    case DLT_EN10MB:
        status = handle_ether(ctx, pkt, hdr->caplen);
        break;
#ifdef HAVE_NET_IF_PPP_H
    case DLT_PPP:
        status = handle_ppp(ctx, pkt, hdr->caplen);
        break;
#endif
#ifdef DLT_LOOP
    case DLT_LOOP:
        status = handle_loop(ctx, pkt, hdr->caplen);
        break;
#endif
#ifdef DLT_RAW
    case DLT_RAW:
        status = handle_raw(ctx, pkt, hdr->caplen);
        break;
#endif
#ifdef DLT_LINUX_SLL
    case DLT_LINUX_SLL:
        status = handle_linux_sll(ctx, pkt, hdr->caplen);
        break;
#endif
    case DLT_NULL:
        status = handle_null(ctx, pkt, hdr->caplen);
        break;

    default:
        PLUGIN_ERROR("unsupported data link type %d", pcap_datalink(ctx->pcap_obj));
        status = 0;
        break;
    }

    if (status == 0)
        return;
#if 0
    query_count_intvl++;
    query_count_total++;
    last_ts = hdr->ts;
#endif
}

static int nc_pcap_loop(nc_pcap_ctx_t *ctx)
{
    /* Don't block any signals */
    sigset_t sigmask;
    sigemptyset(&sigmask);
    pthread_sigmask(SIG_SETMASK, &sigmask, NULL);

    /* Passing `pcap_device == NULL' is okay and the same as passign "any" */
    PLUGIN_DEBUG("Creating PCAP object..");
    char pcap_error[PCAP_ERRBUF_SIZE];
    ctx->pcap_obj = pcap_open_live(ctx->interface,
                                      PCAP_SNAPLEN, 0 /* Not promiscuous */,
                                      (int)CDTIME_T_TO_MS(ctx->interval / 2),
                                      pcap_error);
    if (ctx->pcap_obj == NULL) {
        PLUGIN_ERROR("Opening interface `%s' failed: %s", ctx->interface, pcap_error);
        return PCAP_ERROR;
    }

    struct bpf_program fp = {0};
    int status = pcap_compile(ctx->pcap_obj, &fp, "udp port 53", 1, 0);
    if (status < 0) {
        PLUGIN_ERROR("pcap_compile failed: %s", get_pcap_error(ctx->pcap_obj, status));
        return status;
    }

    status = pcap_setfilter(ctx->pcap_obj, &fp);
    if (status < 0) {
        PLUGIN_ERROR("pcap_setfilter failed: %s", get_pcap_error(ctx->pcap_obj, status));
        return status;
    }

    PLUGIN_DEBUG("PCAP object created.");

#if 0
    dnstop_set_pcap_obj(pcap_obj);
    dnstop_set_callback(dns_child_callback);
#endif

    status = pcap_loop(ctx->pcap_obj, -1, handle_pcap, (void *)ctx);
    PLUGIN_INFO("pcap_loop exited with status %i.", status);
    /* We need to handle "PCAP_ERROR" specially because libpcap currently
     * doesn't return PCAP_ERROR_IFACE_NOT_UP for compatibility reasons. */
    if (status == PCAP_ERROR)
        status = PCAP_ERROR_IFACE_NOT_UP;

    pcap_close(ctx->pcap_obj);
    return status;
}

static int sleep_one_interval(cdtime_t interval)
{
    struct timespec ts = CDTIME_T_TO_TIMESPEC(interval);
    while (nanosleep(&ts, &ts) != 0) {
        if ((errno == EINTR) || (errno == EAGAIN))
            continue;

        return errno;
    }

    return 0;
}

static void *child_loop(__attribute__((unused)) void *dummy)
{
    nc_pcap_ctx_t *ctx = dummy;
    if (ctx == NULL)
        return NULL;

    int status = 0;
    while (true) {
        status = nc_pcap_loop(ctx);
        if (status != PCAP_ERROR_IFACE_NOT_UP)
            break;

        sleep_one_interval(ctx->interval);
    }

    if (status != PCAP_ERROR_BREAK)
        PLUGIN_ERROR("PCAP returned error %s.", get_pcap_error(ctx->pcap_obj, status));

    ctx->listen_thread_init = 0;
    return NULL;
}

static int nc_pcap_read(user_data_t *ud)
{
    if (ud == NULL)
        return 0;

    nc_pcap_ctx_t *ctx = ud->data;
    if (ctx == NULL)
        return 0;

    nc_dns_read(&ctx->dns, &ctx->labels);

    return 0;
}

static void nc_pcap_free(void *arg)
{
    if (arg == NULL)
        return;
    nc_pcap_ctx_t *ctx = arg;

    free(ctx);
}

static int nc_pcap_ignore_list_add(const config_item_t *ci, ip_list_t **list)
{
    if (ci == NULL)
        return EINVAL;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    ignore_list_add_name(list, ci->values[0].value.string);

    return 0;
}

static int nc_pcap_config_instance(config_item_t *ci)
{
    nc_pcap_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

//  memcpy(ctx->fams, fams, sizeof(ctx->fams[0])*FAM_PCAP_MAX);

    int status = cf_util_get_string(ci, &ctx->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(ctx);
        return status;
    }
    assert(ctx->name != NULL);

    ctx->interval = plugin_get_interval();
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("interface", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->interface);
        } else if (strcasecmp("promiscuous", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctx->promiscuous);
        } else if (strcasecmp("ignore-source", child->key) == 0) {
            status = nc_pcap_ignore_list_add(child, &ctx->ignore_src);
        } else if (strcasecmp("ignore-destination", child->key) == 0) {
            status = nc_pcap_ignore_list_add(child, &ctx->ignore_dst);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->filter);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &ctx->interval);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        nc_pcap_free(ctx);
        return -1;
    }

// (pcap_device != NULL) ? pcap_device : "any"

    label_set_add(&ctx->labels, true, "instance", ctx->name);

    nc_dns_init(&ctx->dns);

    status = plugin_thread_create(&ctx->listen_thread, child_loop, (void *)ctx, "pcap listen");
    if (status != 0) {
        PLUGIN_ERROR("pthread_create failed: %s", STRERRNO);
        nc_pcap_free(ctx);
        return -1;
    }

    ctx->listen_thread_init = 1;

    return plugin_register_complex_read("pcap", ctx->name, nc_pcap_read, ctx->interval,
                                        &(user_data_t){.data = ctx, .free_func = nc_pcap_free});
}

static int nc_pcap_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = nc_pcap_config_instance(child);
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

static int nc_pcap_init(void)
{
#if defined(HAVE_SYS_CAPABILITY_H) && defined(CAP_NET_RAW)
    if (plugin_check_capability(CAP_NET_RAW) != 0) {
        if (getuid() == 0)
            PLUGIN_WARNING("Running ncollectd as root, but the CAP_NET_RAW "
                           "capability is missing. The plugin's read function will probably "
                           "fail. Is your init system dropping capabilities?");
        else
            PLUGIN_WARNING("ncollectd doesn't have the CAP_NET_RAW capability. "
                           "If you don't want to run ncollectd as root, try running \"setcap "
                           "cap_net_raw=ep\" on the ncollectd binary.");
    }
#endif

    return 0;
}

void module_register(void)
{
    plugin_register_config("pcap", nc_pcap_config);
    plugin_register_init("pcap", nc_pcap_init);
}
