// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2013 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/avltree.h"
#include "libutils/common.h"

#include <netdb.h>
#include <poll.h>
#include <sys/types.h>

/* AIX doesn't have MSG_DONTWAIT */
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT MSG_NONBLOCK
#endif

#ifndef STATSD_DEFAULT_SERVICE
#define STATSD_DEFAULT_SERVICE "8125"
#endif

typedef enum {
    STATSD_COUNTER,
    STATSD_TIMER,
    STATSD_GAUGE,
    STATSD_SET,
} statsd_metric_type_t;

typedef struct {
    statsd_metric_type_t type;
    double value;
    uint64_t counter;
    histogram_t *histogram;
    c_avl_tree_t *set;
    unsigned long updates_num;
} statsd_metric_t;

typedef struct {
    pthread_t network_thread;
    bool network_thread_running;
    bool network_thread_shutdown;

    c_avl_tree_t *metrics_tree;
    pthread_mutex_t metrics_lock;

    char *instance;
    char *node;
    char *service;

    bool delete_counters;
    bool delete_timers;
    bool delete_gauges;
    bool delete_sets;

    double *timer_buckets;
    size_t timer_buckets_num;

    char *metric_prefix;
    label_set_t labels;
} statsd_instance_t;

/* Must hold metrics_lock when calling this function. */
static statsd_metric_t *statsd_metric_lookup_unsafe(statsd_instance_t *si,
                                                    char const *name, char const *tags,
                                                    statsd_metric_type_t type)
{
    char key[2048];

    switch (type) {
    case STATSD_COUNTER:
        key[0] = 'c';
        break;
    case STATSD_TIMER:
        key[0] = 't';
        break;
    case STATSD_GAUGE:
        key[0] = 'g';
        break;
    case STATSD_SET:
        key[0] = 's';
        break;
    default:
        return NULL;
    }

    key[1] = ':';
    sstrncpy(&key[2], name, sizeof(key) - 2);
    if (tags != NULL) {
        size_t len = strlen(key);
        if ((len + strlen(tags)) >= sizeof(key))
            return NULL;
        key[len] = ',';
        sstrncpy(&key[len+1], tags, sizeof(key) - len - 1);
    }

    statsd_metric_t *metric;
    int status = c_avl_get(si->metrics_tree, key, (void *)&metric);
    if (status == 0)
        return metric;

    char *key_copy = strdup(key);
    if (key_copy == NULL) {
        PLUGIN_ERROR("strdup failed.");
        return NULL;
    }

    metric = calloc(1, sizeof(*metric));
    if (metric == NULL) {
        PLUGIN_ERROR("calloc failed.");
        free(key_copy);
        return NULL;
    }

    metric->type = type;
    metric->histogram = NULL;
    metric->set = NULL;

    status = c_avl_insert(si->metrics_tree, key_copy, metric);
    if (status != 0) {
        PLUGIN_ERROR("c_avl_insert failed.");
        free(key_copy);
        free(metric);
        return NULL;
    }

    return metric;
}

static int statsd_metric_set(statsd_instance_t *si, char const *name, char const *tags,
                                                    double value, statsd_metric_type_t type)
{
    pthread_mutex_lock(&si->metrics_lock);

    statsd_metric_t *metric = statsd_metric_lookup_unsafe(si, name, tags, type);
    if (metric == NULL) {
        pthread_mutex_unlock(&si->metrics_lock);
        return -1;
    }

    metric->value = value;
    metric->updates_num++;

    pthread_mutex_unlock(&si->metrics_lock);
    return 0;
}

static int statsd_metric_add(statsd_instance_t *si, char const *name, char const *tags,
                                                    double delta, statsd_metric_type_t type)
{
    pthread_mutex_lock(&si->metrics_lock);

    statsd_metric_t *metric = statsd_metric_lookup_unsafe(si, name, tags, type);
    if (metric == NULL) {
        pthread_mutex_unlock(&si->metrics_lock);
        return -1;
    }

    metric->value += delta;
    metric->updates_num++;

    pthread_mutex_unlock(&si->metrics_lock);
    return 0;
}

static void statsd_metric_free(statsd_metric_t *metric)
{
    if (metric == NULL)
        return;

    if (metric->histogram != NULL) {
        histogram_destroy(metric->histogram);
        metric->histogram= NULL;
    }

    if (metric->set != NULL) {
        void *key;
        void *value;

        while (c_avl_pick(metric->set, &key, &value) == 0) {
            free(key);
            assert(value == NULL);
        }

        c_avl_destroy(metric->set);
        metric->set = NULL;
    }

    free(metric);
}

static int statsd_parse_value(char const *str, double *ret_value)
{
    char *endptr = NULL;

    *ret_value = strtod(str, &endptr);
    if ((str == endptr) || ((endptr != NULL) && (*endptr != 0)))
        return -1;

    return 0;
}

static int statsd_handle_counter(statsd_instance_t *si, char const *name, char const *tags,
                                                        char const *value_str, char const *extra)
{
    if ((extra != NULL) && (extra[0] != '@'))
        return -1;

    double scale = 1.0;
    if (extra != NULL) {
        int status = statsd_parse_value(extra + 1, &scale);
        if (status != 0)
            return status;

        if (!isfinite(scale) || (scale <= 0.0) || (scale > 1.0))
            return -1;
    }

    double value = 1.0;
    int status = statsd_parse_value(value_str, &value);
    if (status != 0)
        return status;

    /* Changes to the counter are added to (statsd_metric_t*)->value. ->counter is
     * only updated in statsd_metric_submit_unsafe(). */
    return statsd_metric_add(si, name, tags, value / scale, STATSD_COUNTER);
}

static int statsd_handle_gauge(statsd_instance_t *si, char const *name, char const *tags,
                                                      char const *value_str)
{
    double value = 0;
    int status = statsd_parse_value(value_str, &value);
    if (status != 0)
        return status;

    if ((value_str[0] == '+') || (value_str[0] == '-'))
        return statsd_metric_add(si, name, tags, value, STATSD_GAUGE);
    else
        return statsd_metric_set(si, name, tags, value, STATSD_GAUGE);
}

static int statsd_handle_timer(statsd_instance_t *si, char const *name, char const *tags,
                                                      char const *value_str, char const *extra)
{
    if ((extra != NULL) && (extra[0] != '@'))
        return -1;

    double scale = 1.0;
    if (extra != NULL) {
        int status = statsd_parse_value(extra + 1, &scale);
        if (status != 0)
            return status;

        if (!isfinite(scale) || (scale <= 0.0) || (scale > 1.0))
            return -1;
    }

    double value_ms = 0.0;
    int status = statsd_parse_value(value_str, &value_ms);
    if (status != 0)
        return status;

    cdtime_t value = MS_TO_CDTIME_T(value_ms / scale);

    pthread_mutex_lock(&si->metrics_lock);

    statsd_metric_t *metric = statsd_metric_lookup_unsafe(si, name, tags, STATSD_TIMER);
    if (metric == NULL) {
        pthread_mutex_unlock(&si->metrics_lock);
        return -1;
    }

    if (metric->histogram == NULL) {
        if (si->timer_buckets_num == 0) {
            metric->histogram = histogram_new_custom(11,
                                    (double[]){.005, .01, .025, .05, .1, .25, .5, 1, 2.5, 5, 10});
        } else {
            metric->histogram = histogram_new_custom(si->timer_buckets_num, si->timer_buckets);
        }
    }
    if (metric->histogram == NULL) {
        pthread_mutex_unlock(&si->metrics_lock);
        return -1;
    }

    histogram_update(metric->histogram, value);
    metric->updates_num++;

    pthread_mutex_unlock(&si->metrics_lock);
    return 0;
}

static int statsd_handle_set(statsd_instance_t *si, char const *name, char const *tags,
                                                    char const *set_key_orig)
{
    pthread_mutex_lock(&si->metrics_lock);

    statsd_metric_t *metric = statsd_metric_lookup_unsafe(si, name, tags, STATSD_SET);
    if (metric == NULL) {
        pthread_mutex_unlock(&si->metrics_lock);
        return -1;
    }

    /* Make sure metric->set exists. */
    if (metric->set == NULL)
        metric->set = c_avl_create((int (*)(const void *, const void *))strcmp);

    if (metric->set == NULL) {
        pthread_mutex_unlock(&si->metrics_lock);
        PLUGIN_ERROR("c_avl_create failed.");
        return -1;
    }

    char *set_key = strdup(set_key_orig);
    if (set_key == NULL) {
        pthread_mutex_unlock(&si->metrics_lock);
        PLUGIN_ERROR("strdup failed.");
        return -1;
    }

    int status = c_avl_insert(metric->set, set_key, /* value = */ NULL);
    if (status < 0) {
        pthread_mutex_unlock(&si->metrics_lock);
        PLUGIN_ERROR("c_avl_insert (\"%s\") failed with status %i.", set_key, status);
        free(set_key);
        return -1;
    } else if (status > 0) { /* key already exists */
        free(set_key);
    }

    metric->updates_num++;

    pthread_mutex_unlock(&si->metrics_lock);
    return 0;
}

static int statsd_parse_line(statsd_instance_t *si, char *buffer)
{
    char *name = buffer;

    char *type = strchr(name, '|');
    if (type == NULL)
        return -1;
    *(type++) = 0;

    char *value = strrchr(name, ':');
    if (value == NULL)
        return -1;
    *(value++) = 0;

    char *extra = strchr(type, '|');
    if (extra != NULL)
        *(extra++) = 0;

    char *tags = NULL;
    /* signalfx: metric.name[tagName=val,tag2Name=val2]:0|c */
    if ((tags = strchr(name, '[')) != NULL) {
        char *end = strrchr(tags+1, ']');
        if (end != NULL) {
            *end = 0;
            *(tags++) = 0;
        } else {
            tags = NULL;
        }
    /* librato: metric.name#tagName=val,tag2Name=val2:0|c */
    } else if ((tags = strchr(name, '#')) != NULL) {
        *(tags++) = 0;
    /* influxdb: metric.name,tagName=val,tag2Name=val2:0|c */
    } else if ((tags = strchr(name, ',')) != NULL) {
        *(tags++) = 0;
    /* dogstatsd: metric.name:0|c|#tagName:val,tag2Name:val2 */
    } else if (extra != NULL && ((tags = strchr(extra, '#')) != NULL)) {
        *(tags++) = 0;
        char *ptr = tags;
        while((ptr = strchr(ptr, ':')) != NULL)
            *(ptr++) = '=';
        if (*extra == '\0')
            extra = NULL;
    }

    if (strcmp("c", type) == 0)
        return statsd_handle_counter(si, name, tags, value, extra);
    else if (strcmp("ms", type) == 0)
        return statsd_handle_timer(si, name, tags, value, extra);

    /* extra is only valid for counters and timers */
    if (extra != NULL)
        return -1;

    if (strcmp("g", type) == 0)
        return statsd_handle_gauge(si, name, tags, value);
    else if (strcmp("s", type) == 0)
        return statsd_handle_set(si, name, tags, value);
    else
        return -1;
}

static void statsd_parse_buffer(statsd_instance_t *si, char *buffer)
{
    while (buffer != NULL) {
        char *next = strchr(buffer, '\n');
        if (next != NULL)
            *(next++) = 0;

        if (*buffer == 0) {
            buffer = next;
            continue;
        }

        char orig[512];
        sstrncpy(orig, buffer, sizeof(orig));

        int status = statsd_parse_line(si, buffer);
        if (status != 0)
            PLUGIN_ERROR("Unable to parse line: \"%s\"", orig);

        buffer = next;
    }
}

static void statsd_network_read(statsd_instance_t *si, int fd)
{
    char buffer[4096];
    ssize_t status = recv(fd, buffer, sizeof(buffer), /* flags = */ MSG_DONTWAIT);
    if (status < 0) {
#if EAGAIN != EWOULDBLOCK
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            return;
#else
        if (errno == EAGAIN)
            return;
#endif

        PLUGIN_ERROR("recv(2) failed: %s", STRERRNO);
        return;
    }

    size_t buffer_size = (size_t)status;
    if (buffer_size >= sizeof(buffer))
        buffer_size = sizeof(buffer) - 1;
    buffer[buffer_size] = 0;

    statsd_parse_buffer(si, buffer);
}

static int statsd_network_init(statsd_instance_t *si, struct pollfd **ret_fds, size_t *ret_fds_num)
{
    struct pollfd *fds = NULL;
    size_t fds_num = 0;

    struct addrinfo *ai_list;

    struct addrinfo ai_hints = {.ai_family = AF_UNSPEC,
                                .ai_flags = AI_PASSIVE | AI_ADDRCONFIG,
                                .ai_socktype = SOCK_DGRAM};

    int status = getaddrinfo(si->node, si->service, &ai_hints, &ai_list);
    if (status != 0) {
        PLUGIN_ERROR("getaddrinfo (\"%s\", \"%s\") failed: %s", si->node, si->service,
                     gai_strerror(status));
        return status;
    }

    for (struct addrinfo *ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next) {
        char str_node[NI_MAXHOST];
        char str_service[NI_MAXSERV];

        int fd = socket(ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol);
        if (fd < 0) {
            PLUGIN_ERROR("socket(2) failed: %s", STRERRNO);
            continue;
        }

        /* allow multiple sockets to use the same PORT number */
        int yes = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            PLUGIN_ERROR("setsockopt (reuseaddr): %s", STRERRNO);
            close(fd);
            continue;
        }

        getnameinfo(ai_ptr->ai_addr, ai_ptr->ai_addrlen, str_node, sizeof(str_node),
                    str_service, sizeof(str_service),
                    NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV);
        PLUGIN_DEBUG("Trying to bind to [%s]:%s ...", str_node, str_service);

        status = bind(fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
        if (status != 0) {
            PLUGIN_ERROR("bind(2) to [%s]:%s failed: %s", str_node, str_service, STRERRNO);
            close(fd);
            continue;
        }

        struct pollfd *tmp = realloc(fds, sizeof(*fds) * (fds_num + 1));
        if (tmp == NULL) {
            PLUGIN_ERROR("realloc failed.");
            close(fd);
            continue;
        }
        fds = tmp;
        tmp = fds + fds_num;
        fds_num++;

        memset(tmp, 0, sizeof(*tmp));
        tmp->fd = fd;
        tmp->events = POLLIN | POLLPRI;
        PLUGIN_INFO("Listening on [%s]:%s.", str_node, str_service);
    }

    freeaddrinfo(ai_list);

    if (fds_num == 0) {
        PLUGIN_ERROR("Unable to create listening socket for [%s]:%s.",
                     (si->node != NULL) ? si->node : "::", si->service);
        return ENOENT;
    }

    *ret_fds = fds;
    *ret_fds_num = fds_num;
    return 0;
}

static void *statsd_network_thread(void *args)
{
    statsd_instance_t *si = args;
    if (si == NULL)
        pthread_exit((void *)0);

    struct pollfd *fds = NULL;
    size_t fds_num = 0;
    int status = statsd_network_init(si, &fds, &fds_num);
    if (status != 0) {
        PLUGIN_ERROR("Unable to open listening sockets.");
        pthread_exit((void *)0);
    }

    while (!si->network_thread_shutdown) {
        status = poll(fds, (nfds_t)fds_num, /* timeout = */ -1);
        if (status < 0) {
            if ((errno == EINTR) || (errno == EAGAIN))
                continue;

            PLUGIN_ERROR("poll(2) failed: %s", STRERRNO);
            break;
        }

        for (size_t i = 0; i < fds_num; i++) {
            if ((fds[i].revents & (POLLIN | POLLPRI)) == 0)
                continue;

            statsd_network_read(si, fds[i].fd);
            fds[i].revents = 0;
        }
    }

    /* Clean up */
    for (size_t i = 0; i < fds_num; i++)
        close(fds[i].fd);
    free(fds);
    return (void *)0;
}

/* Must hold metrics_lock when calling this function. */
static int statsd_metric_clear_set_unsafe(statsd_metric_t *metric)
{
    if ((metric == NULL) || (metric->type != STATSD_SET))
        return EINVAL;

    if (metric->set == NULL)
        return 0;

    void *key, *value;
    while (c_avl_pick(metric->set, &key, &value) == 0) {
        free(key);
        free(value);
    }

    return 0;
}

/* Must hold metrics_lock when calling this function. */
static int statsd_metric_submit_unsafe(statsd_instance_t *si, char const *name,
                                                              statsd_metric_t *metric)
{
    metric_family_t fam = {0};

    switch (metric->type) {
        case STATSD_GAUGE:
            fam.type = METRIC_TYPE_GAUGE;
            break;
        case STATSD_TIMER:
            fam.type = METRIC_TYPE_HISTOGRAM;
            break;
        case STATSD_SET:
            fam.type = METRIC_TYPE_GAUGE;
            break;
        case STATSD_COUNTER:
            fam.type = METRIC_TYPE_COUNTER;
            break;
    }

    char buffer[2048];
    sstrncpy(buffer, name, sizeof(buffer));

    metric_t m = {0};
    metric_label_set(&m, "instance", si->instance);
    for (size_t i = 0; i < si->labels.num; i++) {
        metric_label_set(&m, si->labels.ptr[i].name, si->labels.ptr[i].value);
    }

    char *metric_name = buffer;
    char *tags = strchr(metric_name, ',');
    if (tags != NULL) {
        *(tags++) = 0;
        char *saveptr = NULL;
        for (char *key = strtok_r(tags, ",", &saveptr);
                 key != NULL;
                 key = strtok_r(NULL, ",", &saveptr)) {
            char *value = strchr(key, '=');
            if (value != NULL) {
                *(value++) = 0;
                metric_label_set(&m, key, value);
            }
        }
    }

    int status = 0;
    strbuf_t buf = STRBUF_CREATE;
    if (si->metric_prefix != NULL)
        status |= strbuf_putstr(&buf, si->metric_prefix);
    status |= strbuf_putstr(&buf, metric_name);

    if (status != 0) {
        PLUGIN_ERROR("Cannot format metric name.");
        label_set_reset(&m.label);
        strbuf_destroy(&buf);
        return 0;
    }

    fam.name = buf.ptr;

    m.value = VALUE_GAUGE(NAN);

    switch (metric->type) {
    case STATSD_GAUGE:
        m.value = VALUE_GAUGE(metric->value);
        break;
    case STATSD_TIMER:
        m.value.histogram = histogram_clone(metric->histogram);
        histogram_reset(metric->histogram);
        break;
    case STATSD_SET:
        if (metric->set == NULL)
            m.value = VALUE_GAUGE(0.0);
        else
            m.value = VALUE_GAUGE(c_avl_size(metric->set));
        break;
    case STATSD_COUNTER: {
        double delta = nearbyint(metric->value);
        /* Rather than resetting value to zero, subtract delta so we correctly keep
         * track of residuals. */
        metric->value -= delta;
        metric->counter += (uint64_t)delta;
        m.value = VALUE_COUNTER(metric->counter);
    }   break;
    }

    metric_family_metric_append(&fam, m);
    metric_reset(&m, fam.type);

    plugin_dispatch_metric_family(&fam, 0);

    strbuf_destroy(&buf);
    
    return 0;
}

static int statsd_instance_read(user_data_t *user_data)
{
    statsd_instance_t *si = user_data->data;
    if (si == NULL)
        return EINVAL;

    pthread_mutex_lock(&si->metrics_lock);

    if (si->metrics_tree == NULL) {
        pthread_mutex_unlock(&si->metrics_lock);
        return 0;
    }

    char **to_be_deleted = NULL;
    size_t to_be_deleted_num = 0;
    char *name;
    statsd_metric_t *metric;
    c_avl_iterator_t *iter = c_avl_get_iterator(si->metrics_tree);
    while (c_avl_iterator_next(iter, (void *)&name, (void *)&metric) == 0) {
        if ((metric->updates_num == 0) &&
                ((si->delete_counters && (metric->type == STATSD_COUNTER)) ||
                 (si->delete_timers && (metric->type == STATSD_TIMER)) ||
                 (si->delete_gauges && (metric->type == STATSD_GAUGE)) ||
                 (si->delete_sets && (metric->type == STATSD_SET)))) {
            PLUGIN_DEBUG("statsd plugin: Deleting metric \"%s\".", name);
            strarray_add(&to_be_deleted, &to_be_deleted_num, name);
            continue;
        }

        /* Names have a prefix, e.g. "c:", which determines the (statsd) type.
         * Remove this here. */
        statsd_metric_submit_unsafe(si, name + 2, metric);

        /* Reset the metric. */
        metric->updates_num = 0;
        if (metric->type == STATSD_SET)
            statsd_metric_clear_set_unsafe(metric);
    }
    c_avl_iterator_destroy(iter);

    for (size_t i = 0; i < to_be_deleted_num; i++) {
        int status;

        status = c_avl_remove(si->metrics_tree, to_be_deleted[i], (void *)&name, (void *)&metric);
        if (status != 0) {
            PLUGIN_ERROR("c_avl_remove (\"%s\") failed with status %i.", to_be_deleted[i], status);
            continue;
        }

        free(name);
        statsd_metric_free(metric);
    }

    pthread_mutex_unlock(&si->metrics_lock);

    strarray_free(to_be_deleted, to_be_deleted_num);
    return 0;
}

static void statsd_instance_shutdown(void *arg)
{
    statsd_instance_t *si = arg;
    if (si == NULL)
        return;

    if (si->network_thread_running) {
        si->network_thread_shutdown = true;
        pthread_kill(si->network_thread, SIGTERM);
        pthread_join(si->network_thread, /* retval = */ NULL);
    }
    si->network_thread_running = false;

    pthread_mutex_lock(&si->metrics_lock);

    void *key, *value;
    while (c_avl_pick(si->metrics_tree, &key, &value) == 0) {
        free(key);
        statsd_metric_free(value);
    }
    c_avl_destroy(si->metrics_tree);
    si->metrics_tree = NULL;

    free(si->node);
    free(si->service);
    free(si->metric_prefix);
    label_set_reset(&si->labels);
    free(si->timer_buckets);

    pthread_mutex_unlock(&si->metrics_lock);

    free(si);
}

static int statsd_instance_config(config_item_t *ci)
{
    statsd_instance_t *si = calloc(1, sizeof(*si));
    if (si == NULL)
        return ENOMEM;

    int status = cf_util_get_string(ci, &si->instance);
    if (status != 0) {
        free(si);
        return status;
    }

    pthread_mutex_init(&si->metrics_lock, NULL);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &si->node);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_service(child, &si->service);
        } else if (strcasecmp("delete-counters", child->key) == 0) {
            status = cf_util_get_boolean(child, &si->delete_counters);
        } else if (strcasecmp("delete-timers", child->key) == 0) {
            status = cf_util_get_boolean(child, &si->delete_timers);
        } else if (strcasecmp("delete-gauges", child->key) == 0) {
            status = cf_util_get_boolean(child, &si->delete_gauges);
        } else if (strcasecmp("delete-sets", child->key) == 0) {
            status = cf_util_get_boolean(child, &si->delete_sets);
        } else if (strcasecmp("timer-buckets", child->key) == 0) {
            status = cf_util_get_double_array(child, &si->timer_buckets_num, &si->timer_buckets);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &si->metric_prefix);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &si->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else {
            PLUGIN_ERROR("The '%s' config option is not valid.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if ((status == 0) && (si->service == NULL)) {
        si->service = strdup(STATSD_DEFAULT_SERVICE);
        if (si->service == NULL) {
            PLUGIN_ERROR("strdup failed.");
            status = ENOMEM;
        }
    }

    if (status == 0) {
        si->metrics_tree = c_avl_create((int (*)(const void *, const void *))strcmp);
        if (status != 0) {
            PLUGIN_ERROR("c_avl_create failed.");
            status = ENOMEM;
        }
    }

    if (status != 0) {
        statsd_instance_shutdown(si);
        return status;
    }

    status = plugin_thread_create(&si->network_thread, statsd_network_thread, si, "statsd");
    if (status != 0) {
        PLUGIN_ERROR("pthread_create failed: %s", STRERRNO);
        statsd_instance_shutdown(si);
        return status;
    }
    si->network_thread_running = true;

    return plugin_register_complex_read("statsd", si->instance, statsd_instance_read, interval,
                                &(user_data_t){.data=si, .free_func=statsd_instance_shutdown});
}

static int statsd_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = statsd_instance_config(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' is not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("statsd", statsd_config);
}
