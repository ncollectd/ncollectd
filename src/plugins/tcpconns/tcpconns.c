// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2007,2008 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2008 Michael Stapelberg
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Michael Stapelberg <michael+git at stapelberg.de>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/itoa.h"

#define TCPCONNS_CFG_PORT 0x01
#define TCPCONNS_CFG_ADDR 0x02

typedef struct {
    uint16_t start;
    uint16_t end;
} inet_port_t;

typedef struct {
    size_t num;
    inet_port_t *ptr;
} inet_ports_t;

typedef struct {
    int flags;
    char *str_port;
    char *str_addr;
    sa_family_t family;
    inet_ports_t ports;
    union {
        struct in_addr in_addr;
        struct in6_addr in6_addr;
    };
    union {
        struct in_addr in_mask;
        struct in6_addr in6_mask;
    };
} inet_addr_t;

typedef struct tcp_counter_s {
    char *name;
    inet_addr_t local;
    inet_addr_t remote;
    uint64_t count[12];
    struct tcp_counter_s *next;
} tcp_counter_t;

static tcp_counter_t *tcp_counter_list_head;
static uint32_t count_total[12];

enum {
    FAM_TCP_ALL_CONNECTIONS,
    FAM_TCP_CONNECTIONS,
    FAM_TCP_MAX
};

static metric_family_t fams[FAM_TCP_MAX] = {
    [FAM_TCP_ALL_CONNECTIONS] = {
        .name = "system_tcp_all_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of TCP connections in the system broken down by state",
    },
    [FAM_TCP_CONNECTIONS] = {
        .name = "system_tcp_connections",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of TCP connections broken down by state",
    }
};

void conn_submit_all(const char *tcp_state[], int tcp_state_min, int tcp_state_max)
{
    for (int i = 1; i <= tcp_state_max; i++) {
        metric_family_append(&fams[FAM_TCP_ALL_CONNECTIONS], VALUE_GAUGE(count_total[i]), NULL,
                             &(label_pair_const_t){.name="state", .value=tcp_state[i]},
                             NULL);
    }

    for (tcp_counter_t *counter = tcp_counter_list_head; counter != NULL; counter = counter->next) {
        for (int i = tcp_state_min; i <= tcp_state_max; i++) {
            label_pair_const_t label_pairs[5];
            size_t label_num = 0;

            if (counter->local.str_port != NULL)
                label_pairs[label_num++] = (label_pair_const_t){.name="local_port",
                                                                .value=counter->local.str_port};
            if (counter->local.str_addr != NULL)
                label_pairs[label_num++] = (label_pair_const_t){.name="local_addr",
                                                                .value=counter->local.str_addr};
            if (counter->remote.str_port != NULL)
                label_pairs[label_num++] = (label_pair_const_t){.name="remote_port",
                                                                .value=counter->remote.str_port};
            if (counter->remote.str_addr != NULL)
                label_pairs[label_num++] = (label_pair_const_t){.name="remote_addr",
                                                                .value=counter->remote.str_addr};

            label_pairs[label_num++] = (label_pair_const_t){.name="state", .value=tcp_state[i]};

            label_set_t labels = {
                .num = label_num,
                .ptr = (label_pair_t *)label_pairs
            };

            metric_family_append(&fams[FAM_TCP_CONNECTIONS], VALUE_GAUGE(counter->count[i]),
                                 &labels, NULL);
        }
    }

    plugin_dispatch_metric_family_array(fams, FAM_TCP_MAX, 0);

    memset(&count_total, 0, sizeof(count_total));

    for (tcp_counter_t *counter = tcp_counter_list_head; counter != NULL; counter = counter->next) {
        memset(counter->count, 0, sizeof(counter->count));
    }
}

static bool tcpconn_port_cmp(inet_ports_t *ports, uint16_t port)
{
    for (size_t i = 0; i < ports->num; i++) {
        if (ports->ptr[i].end == 0) {
            if (ports->ptr[i].start == port)
                return true;
        } else {
            if ((port >= ports->ptr[i].start) && (port <= ports->ptr[i].end))
                return true;
        }
    }

    return false;
}

static bool tcpconn_cmp(inet_addr_t *iaddr, struct sockaddr *saddr)
{
    if (iaddr->family == AF_INET) {
        struct sockaddr_in *in_addr = (struct sockaddr_in *)saddr;
        if (iaddr->flags & TCPCONNS_CFG_ADDR) {
            if ((iaddr->in_addr.s_addr & iaddr->in_mask.s_addr) !=
                (in_addr->sin_addr.s_addr & iaddr->in_mask.s_addr))
                return false;
        }

        if (iaddr->ports.num > 0) {
            if (!tcpconn_port_cmp(&iaddr->ports, in_addr->sin_port))
                return false;
        }
    } else {
        struct sockaddr_in6 *in6_addr = (struct sockaddr_in6 *)saddr;
        if (iaddr->flags & TCPCONNS_CFG_ADDR) {
            for (size_t i = 0; i < 16; i++) {
                if ((iaddr->in6_addr.s6_addr[i] & iaddr->in6_mask.s6_addr[i]) !=
                    (in6_addr->sin6_addr.s6_addr[i] & iaddr->in6_mask.s6_addr[i]))
                    return false;
            }
        }
        if (iaddr->ports.num > 0) {
            if (!tcpconn_port_cmp(&iaddr->ports, in6_addr->sin6_port))
                return false;
        }
    }

    return true;
}

int conn_handle_ports(struct sockaddr *local, struct sockaddr *remote, uint8_t state,
                      int tcp_state_min, int tcp_state_max)
{
    if ((state > tcp_state_max) || ((tcp_state_min > 0) && (state < tcp_state_min))) {
        PLUGIN_NOTICE("Ignoring connection with unknown state 0x%02" PRIx8 ".", state);
        return -1;
    }

    count_total[state]++;

    for (tcp_counter_t *counter = tcp_counter_list_head; counter != NULL; counter = counter->next) {
        if ((counter->local.flags == 0) && (counter->remote.flags == 0))
            continue;

        if (counter->local.flags != 0) {
            if (!tcpconn_cmp(&counter->local, local))
                continue;
        }

        if (counter->remote.flags != 0) {
            if (!tcpconn_cmp(&counter->remote, remote))
                continue;
        }

        counter->count[state]++;
    }

    return 0;
}

static void tcpconn_free_counter(tcp_counter_t *counter)
{
    if (counter == NULL)
        return;

    free(counter->name);

    free(counter->local.str_port);
    free(counter->local.str_addr);
    free(counter->local.ports.ptr);

    free(counter->remote.str_port);
    free(counter->remote.str_addr);
    free(counter->remote.ports.ptr);

    free(counter);
}

static int inet_port_append(config_item_t *ci, inet_ports_t *ports, int start, int end)
{
    if ((start < 1) || (start > 65535)) {
        PLUGIN_ERROR("Invalid port: %i in %s:%d.", start, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if ((end != 0) && ((end < 1) || (end > 65535))) {
        PLUGIN_ERROR("Invalid port: %i in %s:%d.", end, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if ((end != 0) && (start > end)) {
        PLUGIN_ERROR("End port must be larger than start port in %s:%d.",
                     cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    inet_port_t *tmp = realloc(ports->ptr, sizeof(ports->ptr[0]) * (ports->num + 1 ));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        return -1;
    }

    ports->ptr = tmp;
    ports->ptr[ports->num].start = start;
    ports->ptr[ports->num].end = end;
    ports->num++;

    return 0;
}

static int tcpconn_config_port(config_item_t *ci, inet_addr_t *addr)
{
    if ((ci->values_num != 1) ||
        ((ci->values[0].type != CONFIG_TYPE_STRING) &&
         (ci->values[0].type != CONFIG_TYPE_NUMBER))) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string or numeric argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    inet_ports_t *ports = &addr->ports;

    if (ci->values[0].type == CONFIG_TYPE_NUMBER) {
        int status = inet_port_append(ci, ports, ci->values[0].value.number, 0);
        if (status != 0)
            return -1;
        char buffer[ITOA_MAX];
        itoa(ci->values[0].value.number, buffer);
        addr->str_port = strdup(buffer);
        if (addr->str_port == NULL) {
            PLUGIN_ERROR("strdup failed.");
            return -1;
        }
    } else if (ci->values[0].type == CONFIG_TYPE_STRING) {
        addr->str_port = strdup(ci->values[0].value.string);
        if (addr->str_port == NULL) {
            PLUGIN_ERROR("strdup failed.");
            return -1;
        }

        char str_ports[256];
        sstrncpy(str_ports, ci->values[0].value.string, sizeof(str_ports));

        char *start = NULL;
        char *ptr = str_ports;
        char *saveptr = NULL;
        while ((start = strtok_r(ptr, ",", &saveptr)) != NULL) {
            ptr = NULL;

            while(*start == ' ') start++;

            char *end = strchr(start, '-');
            if (end == NULL) {
                char *endptr = NULL;
                int nstart = strtol(start, &endptr, 10);
                if ((endptr == NULL) || (*endptr != '\0')) {
                    PLUGIN_ERROR("Cannot parse number '%s' at %s:%d.",
                                 start, cf_get_file(ci), cf_get_lineno(ci));
                    return -1;
                }
                int status = inet_port_append(ci, ports, nstart, 0);
                if (status != 0)
                    return -1;
            } else {
                *end = '\0';
                end++;
                char *endptr = NULL;
                int nstart = strtol(start, &endptr, 10);
                if ((endptr == NULL) || (*endptr != '\0')) {
                    PLUGIN_ERROR("Cannot parse number '%s' at %s:%d.",
                                 start, cf_get_file(ci), cf_get_lineno(ci));
                    return -1;
                }

                endptr = NULL;
                int nend = strtol(end, &endptr, 10);
                if ((endptr == NULL) || (*endptr != '\0')) {
                    PLUGIN_ERROR("Cannot parse number '%s' at %s:%d.",
                                 end, cf_get_file(ci), cf_get_lineno(ci));
                    return -1;
                }
                int status = inet_port_append(ci, ports, nstart, nend);
                if (status != 0)
                    return -1;
            }
        }
    }

    addr->flags |= TCPCONNS_CFG_PORT;

    return 0;
}

static int tcpconn_config_addr(config_item_t *ci, inet_addr_t *addr)
{
    char ip_str[256];
    int status = cf_util_get_string_buffer(ci, ip_str, sizeof(ip_str));
    if (status != 0)
        return status;

    if (addr->str_addr != NULL)
        free(addr->str_addr);
    addr->str_addr = strdup(ip_str);
    if (addr->str_addr == NULL) {
        PLUGIN_ERROR("strdup failed.");
        return -1;
    }

    addr->family = AF_INET;
    if (strchr(ip_str, ':') != NULL)
        addr->family = AF_INET6;

    int prefix = 0;
    char *prefix_str = strchr(ip_str, '/');
    if (prefix_str != NULL) {
        *prefix_str = '\0';
        prefix_str++;
        char *c = prefix_str;
        while (*c != '\0') {
            if (!isdigit(*c)) {
                PLUGIN_ERROR("Invalid address prefix: '%s' in %s:%d.", prefix_str,
                             cf_get_file(ci), cf_get_lineno(ci));
                return -1;
            }
            c++;
        }
        prefix = atoi(prefix_str);
    }

    if (addr->family == AF_INET) {
        memset(&addr->in_mask.s_addr, 0, sizeof(addr->in_mask.s_addr));
        if (prefix != 0) {
            if (prefix > 32) {
                PLUGIN_ERROR("Invalid address prefix: '%s' in %s:%d.", prefix_str,
                             cf_get_file(ci), cf_get_lineno(ci));
                return -1;
            }
            addr->in_mask.s_addr = htonl((1 << (32 - prefix)) - 1);
        }
        addr->in_mask.s_addr = ~addr->in_mask.s_addr;
    } else if (addr->family == AF_INET6) {
        memset(&addr->in6_mask, 0, sizeof(addr->in6_mask));
        if (prefix != 0)  {
            if (prefix > 128) {
                PLUGIN_ERROR("Invalid address prefix: '%s' in %s:%d.", prefix_str,
                             cf_get_file(ci), cf_get_lineno(ci));
                return -1;
            }
            for (int i = prefix, j = 0; i > 0; i -= 8, j++) {
                if (i > 8)  {
                    addr->in6_mask.s6_addr[j] = 0xff;
                } else {
                    addr->in6_mask.s6_addr[j] = (unsigned long)(0xffU << (8 - i));
                }
            }
        } else {
            for (int i = 0; i < 16; i++) {
                addr->in6_mask.s6_addr[i] = 0xff;
            }
        }
    }

    if (addr->family == AF_INET) {
        status = inet_pton(addr->family, ip_str, &addr->in_addr);
    } else if (addr->family == AF_INET6) {
        status = inet_pton(addr->family, ip_str, &addr->in6_addr);
    }

    if (status != 1) {
        PLUGIN_ERROR("Cannot convert address: '%s'.", ip_str);
        return -1;
    }

    addr->flags |= TCPCONNS_CFG_ADDR;

    return 0;
}

static int tcpconn_config_counter(config_item_t *ci)
{
    tcp_counter_t *counter = calloc(1, sizeof(*counter));
    if (counter == NULL) {
        return -1;
    }

    int status = cf_util_get_string(ci, &counter->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing counter name.");
        free(counter);
        return status;
    }

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcmp(child->key, "local-ip") == 0) {
            status = tcpconn_config_addr(child, &counter->local);
        } else if (strcmp(child->key, "local-port") == 0) {
            status = tcpconn_config_port(child, &counter->local);
        } else if (strcmp(child->key, "remote-ip") == 0) {
            status = tcpconn_config_addr(child, &counter->remote);
        } else if (strcmp(child->key, "remote-port") == 0) {
            status = tcpconn_config_port(child, &counter->remote);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    if (status != 0) {
        tcpconn_free_counter(counter);
        return -1;
    }

    if ((counter->local.family != 0) && (counter->remote.family !=0)) {
        if (counter->local.family != counter->remote.family) {
            PLUGIN_ERROR("Mixing IPv4 and IPv6 in counter.");
            tcpconn_free_counter(counter);
            return -1;
        }
    }

    counter->next = tcp_counter_list_head;
    tcp_counter_list_head = counter;

    return 0;
}

static int conn_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "counter") == 0) {
            status = tcpconn_config_counter(child);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

__attribute__(( weak ))
int conn_read(void)
{
    return 0;
}

__attribute__(( weak ))
int conn_init(void)
{
    return 0;
}

__attribute__(( weak ))
int conn_shutdown(void)
{
    return 0;
}

static int conn_generic_shutdown(void)
{
    tcp_counter_t *counter = tcp_counter_list_head;
    while (counter != NULL) {
        tcp_counter_t *next = counter->next;
        tcpconn_free_counter(counter);
        counter = next;
    }
    tcp_counter_list_head = NULL;

    return conn_shutdown();
}

void module_register(void)
{
    plugin_register_config("tcpconns", conn_config);
    plugin_register_init("tcpconns", conn_init);
    plugin_register_read("tcpconns", conn_read);
    plugin_register_shutdown("tcpconns", conn_generic_shutdown);
}
