// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2005-2012 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/complain.h"

#include <netinet/in.h>
#ifdef HAVE_NETDB_H
#include <netdb.h> /* NI_MAXHOST */
#endif

#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#endif

#include "oping.h"

enum {
    FAM_PING_DROP_RATIO,
    FAM_PING_LATENCY_AVG_SECONDS,
    FAM_PING_LATENCY_STDDEV_SECONDS,
    FAM_PING_LATENCY_SECONDS,
    FAM_PING_MAX
};

static metric_family_t fams[FAM_PING_MAX] = {
    [FAM_PING_DROP_RATIO] = {
        .name = "ping_drop_ratio",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_PING_LATENCY_AVG_SECONDS] = {
        .name = "ping_latency_avg_seconds",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_PING_LATENCY_STDDEV_SECONDS] = {
        .name = "ping_latency_stddev_seconds",
        .type = METRIC_TYPE_GAUGE,
    },
    [FAM_PING_LATENCY_SECONDS] = {
        .name = "ping_latency_seconds",
        .type = METRIC_TYPE_HISTOGRAM,
    }
};

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

struct hostlist_s {
    char *host;
    uint32_t pkg_sent;
    uint32_t pkg_recv;
    uint32_t pkg_missed;
    double latency_total;
    double latency_squared;
    histogram_t *latency;
    double *buckets;
    size_t buckets_size;
    label_set_t labels;
    struct hostlist_s *next;
};
typedef struct hostlist_s hostlist_t;

typedef struct  {
    char *name;
    int ping_af;
    char *ping_source;
    char *ping_device;
    int ping_data_size;
    char *ping_data;
    int ping_ttl;
    cdtime_t ping_interval;
    cdtime_t ping_timeout;
    int ping_max_missed;
    double *buckets;
    size_t buckets_size;
    pthread_mutex_t ping_lock;
    pthread_cond_t ping_cond;
    int ping_thread_loop;
    int ping_thread_error;
    pthread_t ping_thread_id;
    hostlist_t *hostlist_head;
    label_set_t labels;
    plugin_filter_t *filter;
    metric_family_t fams[FAM_PING_MAX];
} ping_inst_t;

static int ping_dispatch_all(pingobj_t *pingobj, ping_inst_t *inst)
{
    for (pingobj_iter_t *iter = ping_iterator_get(pingobj);
           iter != NULL; iter = ping_iterator_next(iter)) {
        char userhost[NI_MAXHOST];
        size_t param_size = sizeof(userhost);
        int status = ping_iterator_get_info(iter, PING_INFO_USERNAME, userhost, &param_size);
        if (status != 0) {
            PLUGIN_WARNING("ping_iterator_get_info failed: %s", ping_get_error(pingobj));
            continue;
        }

        hostlist_t *hl;
        for (hl = inst->hostlist_head; hl != NULL; hl = hl->next) {
            if (strcmp(userhost, hl->host) == 0)
                break;
        }

        if (hl == NULL) {
            PLUGIN_WARNING("Cannot find host %s.", userhost);
            continue;
        }

        double latency;
        param_size = sizeof(latency);
        status = ping_iterator_get_info(iter, PING_INFO_LATENCY, (void *)&latency, &param_size);
        if (status != 0) {
            PLUGIN_WARNING("ping_iterator_get_info failed: %s", ping_get_error(pingobj));
            continue;
        }
        hl->pkg_sent++;
        if (latency >= 0.0) {
            hl->pkg_recv++;
            latency = latency / 1000.0;
            hl->latency_total += latency;
            hl->latency_squared += (latency * latency);

            status = histogram_update(hl->latency, latency);
            if (status != 0)
                PLUGIN_WARNING("histogram_update failed: %s",STRERROR(status));

            /* reset missed packages counter */
            hl->pkg_missed = 0;
        } else {
            hl->pkg_missed++;
        }

        /* if the host did not answer our last N packages, trigger a resolv. */
        if ((inst->ping_max_missed >= 0) && (hl->pkg_missed >= ((uint32_t)inst->ping_max_missed))) {
            /* we reset the missed package counter here, since we only want to
             * trigger a resolv every N packages and not every package _AFTER_ N
             * missed packages */
            hl->pkg_missed = 0;

            PLUGIN_WARNING("host %s has not answered %d PING requests, triggering resolve",
                            hl->host, inst->ping_max_missed);

            /* we trigger the resolv simply be removing and adding the host to our
             * ping object */
            status = ping_host_remove(pingobj, hl->host);
            if (status != 0) {
                PLUGIN_WARNING("ping_host_remove (%s) failed.", hl->host);
            } else {
                status = ping_host_add(pingobj, hl->host);
                if (status != 0)
                    PLUGIN_ERROR("ping_host_add (%s) failed.", hl->host);
            }
        }
    }

    return 0;
}

static void *ping_thread(void *arg)
{
    ping_inst_t *inst = arg;

    c_complain_t complaint = C_COMPLAIN_INIT_STATIC;

    pingobj_t *pingobj = ping_construct();
    if (pingobj == NULL) {
        PLUGIN_ERROR("ping_construct failed.");
        pthread_mutex_lock(&inst->ping_lock);
        inst->ping_thread_error = true;
        pthread_mutex_unlock(&inst->ping_lock);
        return (void *)-1;
    }

    if (inst->ping_af != PING_DEF_AF) {
        if (ping_setopt(pingobj, PING_OPT_AF, &inst->ping_af) != 0)
            PLUGIN_WARNING("Failed to set address family: %s", ping_get_error(pingobj));
    }

    if (inst->ping_source != NULL) {
        if (ping_setopt(pingobj, PING_OPT_SOURCE, (void *)inst->ping_source) != 0)
            PLUGIN_WARNING("Failed to set source address: %s", ping_get_error(pingobj));
    }

    if (inst->ping_device != NULL) {
        if (ping_setopt(pingobj, PING_OPT_DEVICE, (void *)inst->ping_device) != 0)
            PLUGIN_WARNING("Failed to set device: %s", ping_get_error(pingobj));
    }

    double ping_timeout = CDTIME_T_TO_DOUBLE(inst->ping_timeout);
    ping_setopt(pingobj, PING_OPT_TIMEOUT, (void *)&ping_timeout);
    ping_setopt(pingobj, PING_OPT_TTL, (void *)&inst->ping_ttl);

    if (inst->ping_data != NULL)
        ping_setopt(pingobj, PING_OPT_DATA, (void *)inst->ping_data);

    /* Add all the hosts to the ping object. */
    int count = 0;
    for (hostlist_t *hl = inst->hostlist_head; hl != NULL; hl = hl->next) {
        int tmp_status = ping_host_add(pingobj, hl->host);
        if (tmp_status != 0)
            PLUGIN_WARNING("ping_host_add (%s) failed: %s", hl->host, ping_get_error(pingobj));
        else
            count++;
    }

    if (count == 0) {
        PLUGIN_ERROR("No host could be added to ping object. Giving up.");
        pthread_mutex_lock(&inst->ping_lock);
        inst->ping_thread_error = true;
        pthread_mutex_unlock(&inst->ping_lock);
        ping_destroy(pingobj);
        return (void *)-1;
    }

    pthread_mutex_lock(&inst->ping_lock);
    while (inst->ping_thread_loop) {
        bool send_successful = false;

        cdtime_t begin = cdtime();

        pthread_mutex_unlock(&inst->ping_lock);

        int status = ping_send(pingobj);
        if (status < 0) {
            c_complain(LOG_ERR, &complaint, "ping_send failed: %s", ping_get_error(pingobj));
        } else {
            c_release(LOG_NOTICE, &complaint, "ping_send succeeded.");
            send_successful = true;
        }

        pthread_mutex_lock(&inst->ping_lock);

        if (!inst->ping_thread_loop)
            break;

        if (send_successful)
            (void)ping_dispatch_all(pingobj, inst);

        cdtime_t end = cdtime();

        /* Calculate the absolute time until which to wait. */
        cdtime_t wait = begin + inst->ping_interval;
        if (end > wait)
            wait = end;
        struct timespec ts_wait = CDTIME_T_TO_TIMESPEC(wait);

        pthread_cond_timedwait(&inst->ping_cond, &inst->ping_lock, &ts_wait);
        if (!inst->ping_thread_loop)
            break;
    }

    pthread_mutex_unlock(&inst->ping_lock);
    ping_destroy(pingobj);

    return (void *)0;
}

static int ping_start_thread(ping_inst_t *inst)
{
    pthread_mutex_lock(&inst->ping_lock);

    if (inst->ping_thread_loop) {
        pthread_mutex_unlock(&inst->ping_lock);
        return 0;
    }

    inst->ping_thread_loop = true;
    inst->ping_thread_error = false;
    int status = plugin_thread_create(&inst->ping_thread_id, ping_thread, inst, "ping");
    if (status != 0) {
        inst->ping_thread_loop = false;
        PLUGIN_ERROR("Starting thread failed.");
        pthread_mutex_unlock(&inst->ping_lock);
        return -1;
    }

    pthread_mutex_unlock(&inst->ping_lock);
    return 0;
}

static int ping_stop_thread(ping_inst_t *inst)
{
    pthread_mutex_lock(&inst->ping_lock);

    if (!inst->ping_thread_loop) {
        pthread_mutex_unlock(&inst->ping_lock);
        return -1;
    }

    inst->ping_thread_loop = false;
    pthread_cond_broadcast(&inst->ping_cond);
    pthread_mutex_unlock(&inst->ping_lock);

    /* coverity[MISSING_LOCK] */
    int status = pthread_join(inst->ping_thread_id, /* return = */ NULL);
    if (status != 0) {
        PLUGIN_ERROR("Stopping thread failed.");
        status = -1;
    }

    pthread_mutex_lock(&inst->ping_lock);
    memset(&inst->ping_thread_id, 0, sizeof(inst->ping_thread_id));
    inst->ping_thread_error = false;
    pthread_mutex_unlock(&inst->ping_lock);

    return status;
}

static int ping_read(user_data_t *user_data)
{
    ping_inst_t *inst = user_data->data;

    pthread_mutex_lock(&inst->ping_lock);
    bool ping_thread_error = inst->ping_thread_error;
    pthread_mutex_unlock(&inst->ping_lock);

    if (ping_thread_error) {
        PLUGIN_ERROR("The ping thread had a problem. Restarting it.");

        ping_stop_thread(inst);

        for (hostlist_t *hl = inst->hostlist_head; hl != NULL; hl = hl->next) {
            hl->pkg_sent = 0;
            hl->pkg_recv = 0;
            hl->latency_total = 0.0;
            hl->latency_squared = 0.0;
            histogram_reset(hl->latency);
        }

        ping_start_thread(inst);

        return -1;
    }

    const char *hostname = plugin_get_hostname();

    for (hostlist_t *hl = inst->hostlist_head; hl != NULL; hl = hl->next) {
        /* Locking here works, because the structure of the linked list is only
         * changed during configure and shutdown. */

        pthread_mutex_lock(&inst->ping_lock);

        uint32_t pkg_sent = hl->pkg_sent;
        uint32_t pkg_recv = hl->pkg_recv;
        double latency_total = hl->latency_total;
        double latency_squared = hl->latency_squared;
        histogram_t *latency = histogram_clone(hl->latency);

        hl->pkg_sent = 0;
        hl->pkg_recv = 0;
        hl->latency_total = 0.0;
        hl->latency_squared = 0.0;

        pthread_mutex_unlock(&inst->ping_lock);

        /* This e. g. happens when starting up. */
        if (pkg_sent == 0) {
            PLUGIN_DEBUG("No packages for host %s have been sent.", hl->host);
            free(latency);
            continue;
        }

        if (latency != NULL) {
            metric_family_append(&inst->fams[FAM_PING_LATENCY_SECONDS],
                                 VALUE_HISTOGRAM(latency), &hl->labels,
                                 &(label_pair_const_t){.name="destination", .value=hl->host},
                                 &(label_pair_const_t){.name="source", .value=hostname},
                                 NULL);
            free(latency);
        }

        /* Calculate average. Beware of division by zero. */
        double latency_average;
        if (pkg_recv == 0)
            latency_average = NAN;
        else
            latency_average = latency_total / ((double)pkg_recv);

        metric_family_append(&inst->fams[FAM_PING_LATENCY_AVG_SECONDS],
                             VALUE_GAUGE(latency_average), &hl->labels,
                             &(label_pair_const_t){.name="destination", .value=hl->host},
                             &(label_pair_const_t){.name="source", .value=hostname}, NULL);

        /* Calculate standard deviation. Beware even more of division by zero. */
        double latency_stddev;
        if (pkg_recv == 0)
            latency_stddev = NAN;
        else if (pkg_recv == 1)
            latency_stddev = 0.0;
        else
            latency_stddev = sqrt(((((double)pkg_recv) * latency_squared) -
                                   (latency_total * latency_total)) /
                                  ((double)(pkg_recv * (pkg_recv - 1))));

        metric_family_append(&inst->fams[FAM_PING_LATENCY_STDDEV_SECONDS],
                             VALUE_GAUGE(latency_stddev), &hl->labels,
                             &(label_pair_const_t){.name="destination", .value=hl->host},
                             &(label_pair_const_t){.name="source", .value=hostname}, NULL);

        /* Calculate drop ratio. */
        double droprate = ((double)(pkg_sent - pkg_recv)) / ((double)pkg_sent);

        metric_family_append(&inst->fams[FAM_PING_DROP_RATIO],
                             VALUE_GAUGE(droprate), &hl->labels,
                             &(label_pair_const_t){.name="destination", .value=hl->host},
                             &(label_pair_const_t){.name="source", .value=hostname}, NULL);
    }

    plugin_dispatch_metric_family_array_filtered(inst->fams, FAM_PING_MAX, inst->filter, 0);

    return 0;
}

static void ping_hostlist_free(hostlist_t *hl)
{

    while (hl != NULL) {
        hostlist_t *hl_next = hl->next;

        free(hl->host);
        free(hl->buckets);
        histogram_destroy(hl->latency);
        label_set_reset(&hl->labels);
        free(hl);

        hl = hl_next;
    }
}

static void ping_instance_free(void *arg)
{
    ping_inst_t *inst = arg;
    if (inst == NULL)
        return;

    PLUGIN_INFO("Shutting down thread.");
    if (ping_stop_thread(inst) < 0)
        return;

    free(inst->name);
    free(inst->ping_source);
    free(inst->ping_device);
    free(inst->ping_data);
    free(inst->buckets);

    label_set_reset(&inst->labels);
    plugin_filter_free(inst->filter);

    ping_hostlist_free(inst->hostlist_head);

    free(inst);
}

static int ping_config_instance_host(config_item_t *ci, ping_inst_t *inst)
{
    hostlist_t *hl = calloc(1, sizeof(*hl));
    if (hl == NULL) {
        PLUGIN_ERROR("Cannot create a histogram for latency: calloc failed: %s", STRERRNO);
        return -1;
    }

    char *host = NULL;
    int status = cf_util_get_string(ci, &host);
    if (status != 0) {
        ping_hostlist_free(hl);
        return -1;
    }

    hl->host = host;
    hl->pkg_sent = 0;
    hl->pkg_recv = 0;
    hl->pkg_missed = 0;
    hl->latency_total = 0.0;
    hl->latency_squared = 0.0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "label") == 0) {
            status = cf_util_get_label(child, &hl->labels);
        } else if (strcasecmp("histogram-buckets", child->key) == 0) {
            status = cf_util_get_double_array(child, &hl->buckets_size, &hl->buckets);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        ping_hostlist_free(hl);
        return -1;
    }

    hl->next = inst->hostlist_head;
    inst->hostlist_head = hl;

    return 0;
}

static int ping_config_instance(config_item_t *ci)
{
    ping_inst_t *inst = calloc(1, sizeof(*inst));
    if (inst == NULL) {
        PLUGIN_ERROR("calloc failed: %s.", STRERRNO);
        return -1;
    }

    int status = cf_util_get_string(ci, &inst->name);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name in %s:%d", cf_get_file(ci), cf_get_lineno(ci));
        free(inst);
        return status;
    }

    inst->ping_af = PING_DEF_AF;
    inst->ping_ttl = PING_DEF_TTL;

    inst->ping_interval = DOUBLE_TO_CDTIME_T(1.0);
    inst->ping_timeout = DOUBLE_TO_CDTIME_T(0.9);
    inst->ping_max_missed = -1;

    memcpy(inst->fams, fams, sizeof(inst->fams[0])*FAM_PING_MAX);

    pthread_mutex_init(&inst->ping_lock, NULL);
    pthread_cond_init(&inst->ping_cond, NULL);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "host") == 0) {
            status = ping_config_instance_host(child, inst);
        } else if (strcasecmp(child->key, "address-family") == 0) {
            char *af = NULL;
            status = cf_util_get_string(child, &af);
            if (status == 0) {
                if (strncmp(af, "any", 3) == 0) {
                    inst->ping_af = AF_UNSPEC;
                } else if (strncmp(af, "ipv4", 4) == 0) {
                    inst->ping_af = AF_INET;
                } else if (strncmp(af, "ipv6", 4) == 0) {
                    inst->ping_af = AF_INET6;
                } else {
                    PLUGIN_WARNING("Ignoring invalid address-family value '%s'", af);
                }
                free(af);
            }
        } else if (strcasecmp(child->key, "source-address") == 0) {
            status = cf_util_get_string(child, &inst->ping_source);
        } else if (strcasecmp(child->key, "device") == 0) {
            status = cf_util_get_string(child, &inst->ping_device);
        } else if (strcasecmp(child->key, "ttl") == 0) {
            int ttl = 0;
            status = cf_util_get_int(child, &ttl);
            if (status == 0) {
                if ((ttl > 0) && (ttl <= 255))
                    inst->ping_ttl = ttl;
                else
                    PLUGIN_WARNING("Ignoring invalid ttl %i.", ttl);
            }
        } else if (strcasecmp(child->key, "ping-interval") == 0) {
            status = cf_util_get_cdtime(child, &inst->ping_interval);
        } else if (strcasecmp(child->key, "interval") == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp(child->key, "size") == 0) {
            int size = 0;
            status = cf_util_get_int(child, &size);
            if (status == 0) {
                /* Max IP packet size - (IPv6 + ICMP) = 65535 - (40 + 8) = 65487 */
                if (size <= 65487) {
                    free(inst->ping_data);
                    inst->ping_data = malloc(size + 1);
                    if (inst->ping_data == NULL) {
                        PLUGIN_ERROR("malloc failed.");
                        status = -1;
                        break;
                    }

                    /* Note: By default oping is using constant string
                     * "liboping -- ICMP ping library <http://octo.it/liboping/>"
                     * which is exactly 56 bytes.
                     *
                     * Optimally we would follow the ping(1) behaviour, but we
                     * cannot use byte 00 or start data payload at exactly same
                     * location, due to oping library limitations. */
                    for (int j = 0; j < size; j++) {
                        /* This restricts data pattern to be only composed of easily
                         * printable characters, and not NUL character. */
                        inst->ping_data[j] = ('0' + j % 64);
                    }
                    inst->ping_data[size] = 0;
                    inst->ping_data_size = size;
                } else {
                    PLUGIN_WARNING("Ignoring invalid 'size' %d.", size);
                }
            }
        } else if (strcasecmp(child->key, "timeout") == 0) {
            status = cf_util_get_cdtime(child, &inst->ping_timeout);
        } else if (strcasecmp(child->key, "max-missed") == 0) {
            status = cf_util_get_int(child, &inst->ping_max_missed);
            if (status == 0) {
                if (inst->ping_max_missed < 0)
                    PLUGIN_INFO("max-missed < 0, disabled re-resolving of hosts");
            }
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &inst->labels);
        } else if (strcasecmp("histogram-buckets", child->key) == 0) {
            status = cf_util_get_double_array(child, &inst->buckets_size, &inst->buckets);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &inst->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        ping_instance_free(inst);
        return -1;
    }

    if (inst->hostlist_head == NULL) {
        PLUGIN_NOTICE("No hosts have been configured in ping instance '%s' at %s:%d.",
                      inst->name, cf_get_file(ci), cf_get_lineno(ci));
        ping_instance_free(inst);
        return -1;
    }

    if (inst->ping_timeout > inst->ping_interval) {
        inst->ping_timeout = 0.9 * inst->ping_interval;
        PLUGIN_WARNING("Timeout is greater than interval. Will use a timeout of %gs.",
                       CDTIME_T_TO_DOUBLE(inst->ping_timeout));
    }

    label_set_add(&inst->labels, true, "instance", inst->name);

    for (hostlist_t *hl = inst->hostlist_head; hl != NULL; hl = hl->next) {
        label_set_add_set(&hl->labels, false, inst->labels);

        if (hl->buckets_size > 0) {
            hl->latency = histogram_new_custom(hl->buckets_size, hl->buckets);
        } else if (inst->buckets_size > 0) {
            hl->latency = histogram_new_custom(inst->buckets_size, inst->buckets);
        } else {
            hl->latency = histogram_new_exp(15, 2, 0.00005);
        }

        if (hl->latency == NULL) {
            PLUGIN_ERROR("Cannot create a histogram for latency in ping instance '%s' at %s:%d",
                         inst->name, cf_get_file(ci), cf_get_lineno(ci));
            ping_instance_free(inst);
            return -1;
        }
    }

    ping_start_thread(inst);

    return plugin_register_complex_read("ping", inst->name, ping_read, interval,
                        &(user_data_t){.data = inst, .free_func = ping_instance_free});
}

static int ping_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "instance") == 0) {
            status = ping_config_instance(child);
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

static int ping_init(void)
{
#if defined(HAVE_SYS_CAPABILITY_H) && defined(CAP_NET_RAW)
    if (plugin_check_capability(CAP_NET_RAW) != 0) {
        if (getuid() == 0)
            PLUGIN_WARNING("Running ncollectd as root, but the CAP_NET_RAW "
                           "capability is missing. The plugin's read function will probably "
                           "fail. Is your init system dropping capabilities?");
        else
            PLUGIN_WARNING("ncollectd doesn't have the CAP_NET_RAW capability. "
                           "If you don't want to run ncollectd as root, try running "
                           "'setcap cap_net_raw=ep' on the ncollectd binary.");
    }
#endif
    return 0;
}

void module_register(void)
{
    plugin_register_config("ping", ping_config);
    plugin_register_init("ping", ping_init);
}
