// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/avltree.h"

#include <systemd/sd-daemon.h>
#include <systemd/sd-bus.h>

enum {
    FAM_LOGIND_SESSIONS,
    FAM_LOGIND_MAX,
};

static metric_family_t fams[FAM_LOGIND_MAX] = {
    [FAM_LOGIND_SESSIONS] = {
        .name = "logind_sessions",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of sessions registered in logind.",
    },
};

typedef enum {
    LOGIND_GROUP_BY_SEAT   = (1 <<  0),
    LOGIND_GROUP_BY_REMOTE = (1 <<  1),
    LOGIND_GROUP_BY_TYPE   = (1 <<  2),
    LOGIND_GROUP_BY_CLASS  = (1 <<  3),
} logind_group_by_flag_t;

static cf_flags_t logind_flags[] = {
    { "seat",   LOGIND_GROUP_BY_SEAT   },
    { "remote", LOGIND_GROUP_BY_REMOTE },
    { "type",   LOGIND_GROUP_BY_TYPE   },
    { "class",  LOGIND_GROUP_BY_CLASS  },
};
static size_t logind_flags_size = STATIC_ARRAY_SIZE(logind_flags);

static uint64_t logind_group_by = 0;

static uint64_t sessions_total = 0;

static size_t session_remote_size = 2;

static char *session_types[] = {"other", "unspecified", "tty", "x11", "wayland", "mir", "web"};
static size_t session_types_size = STATIC_ARRAY_SIZE(session_types);

static char *session_classes[] = {"other", "user", "greeter", "lock-screen", "background"};
static size_t session_classes_size = STATIC_ARRAY_SIZE(session_classes);

typedef struct {
    char *seat;
    int remote;
    char *type;
    char *class;
    uint64_t cnt;
} logind_session_t;

static void logind_session_free(logind_session_t *ls)
{
    if (ls == NULL)
        return;

    free(ls->seat);
    free(ls->type);
    free(ls->class);
    free(ls);
}

static char *get_session_type(char *type)
{
    for (size_t i = 1; i < session_types_size; i++) {
        if (strcmp(type, session_types[i]) == 0)
            return type;
    }

    return session_types[0];
}

static char *get_session_class(char *class)
{
    for (size_t i = 1; i < session_classes_size; i++) {
        if (strcmp(class, session_classes[i]) == 0)
            return class;
    }

    return session_classes[0];
}

static int logind_session_cmp(void *a, void *b)
{
    logind_session_t *sa = a;
    logind_session_t *sb = b;

    int cmp;

    if (logind_group_by & LOGIND_GROUP_BY_SEAT) {
        if (sa->seat == NULL)
            return -1;
        if (sb->seat == NULL)
            return 1;
        if ((cmp = strcmp(sa->seat, sb->seat)) != 0)
            return cmp;
    }

    if (logind_group_by & LOGIND_GROUP_BY_REMOTE) {
        if (sa->remote != sb->remote) {
            if (sa->remote > sb->remote)
                return 1;
            return -1;
        }
    }

    if (logind_group_by & LOGIND_GROUP_BY_TYPE) {
        if (sa->type == NULL)
            return -1;
        if (sb->type == NULL)
            return 1;
        if ((cmp = strcmp(sa->type, sb->type)) != 0)
            return cmp;
    }

    if (logind_group_by & LOGIND_GROUP_BY_CLASS) {
        if (sa->class == NULL)
            return -1;
        if (sb->class == NULL)
            return 1;
        if ((cmp = strcmp(sa->class, sb->class)) != 0)
            return cmp;
    }

    return 0;
}

static int logind_session_inc (c_avl_tree_t *tree, char *seat, int remote, char *type, char *class)
{
    logind_session_t lk = {0};
    logind_session_t *lv = NULL;

    if (logind_group_by & LOGIND_GROUP_BY_SEAT) {
        if (seat[0] == '\0')
            lk.seat = "none";
        else
            lk.seat = seat;
    }

    if (logind_group_by & LOGIND_GROUP_BY_REMOTE)
        lk.remote = remote == 0 ? 0 : 1;

    if (logind_group_by & LOGIND_GROUP_BY_TYPE) {
        if (type == NULL)
            return -1;
        lk.type = get_session_type(type);
    }

    if (logind_group_by & LOGIND_GROUP_BY_CLASS) {
        if (class == NULL)
            return -1;
        lk.class = get_session_class(class);
    }

    int status = c_avl_get(tree, &lk, (void *)&lv);
    if (status == 0) {
        lv->cnt++;
        return 0;
    }

    logind_session_t *ls = calloc (1,  sizeof(*ls));
    if (ls == NULL)
        return -1;

    if (logind_group_by & LOGIND_GROUP_BY_SEAT) {
        if (seat == NULL) {
            logind_session_free(ls);
            return -1;
        }

        if (seat[0] == '\0')
            ls->seat = strdup("none");
        else
            ls->seat = strdup(seat);

        if (ls->seat == NULL) {
            logind_session_free(ls);
            return -1;
        }
    }

    if (logind_group_by & LOGIND_GROUP_BY_REMOTE)
        ls->remote = remote == 0 ? 0 : 1;

    if (logind_group_by & LOGIND_GROUP_BY_TYPE) {
        if (ls->type == NULL) {
            logind_session_free(ls);
            return -1;
        }

        ls->type = strdup(get_session_type(type));
        if (ls->type == NULL) {
            logind_session_free(ls);
            return -1;
        }
    }

    if (logind_group_by & LOGIND_GROUP_BY_CLASS) {
        if (ls->class == NULL) {
            logind_session_free(ls);
            return -1;
        }

        ls->class = strdup(get_session_class(class));
        if (ls->class == NULL) {
            logind_session_free(ls);
            return -1;
        }
    }

    ls->cnt++;

    status = c_avl_insert(tree, ls, ls);
    if (status != 0) {
        logind_session_free(ls);
        return -1;
    }

    return 0;
}

static int get_property_bool(sd_bus *bus, const char *destination, const char *path,
                             const char *interface, const char *member, int *value)
{
    sd_bus_message *reply = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;

    int status = sd_bus_call_method(bus, destination, path, "org.freedesktop.DBus.Properties",
                                         "Get", &error, &reply, "ss", interface, member);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    sd_bus_error_free(&error);

    status = sd_bus_message_enter_container(reply, 'v', "b");
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    int boolean = 0;
    status = sd_bus_message_read_basic(reply, 'b', &boolean);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    *value = boolean;

    sd_bus_message_unref(reply);

    return 0;
}

static int get_property_string(sd_bus *bus, const char *destination, const char *path,
                               const char *interface, const char *member, char **value)
{
    sd_bus_message *reply = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;

    int status = sd_bus_call_method(bus, destination, path, "org.freedesktop.DBus.Properties",
                                         "Get", &error, &reply, "ss", interface, member);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    sd_bus_error_free(&error);

    status = sd_bus_message_enter_container(reply, 'v', "s");
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    char *str = NULL;
    status = sd_bus_message_read_basic(reply, 's', &str);
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    *value = strdup(str);
    if (*value == NULL) {
        sd_bus_message_unref(reply);
        return -1;
    }

    sd_bus_message_unref(reply);

    return 0;
}

static int logind_submit(metric_family_t *fam,  c_avl_tree_t *sessions,
                         char *seat, char *type, char *class, int remote)
{
    size_t n = 0;
    label_pair_t labels[4] = {0};

    if (logind_group_by & LOGIND_GROUP_BY_SEAT) {
        labels[n].name = "seat";
        labels[n].value = seat;
        n++;
    }

    if (logind_group_by & LOGIND_GROUP_BY_REMOTE) {
        labels[n].name = "remote";
        labels[n].value = remote == 0 ? "false" : "true";
        n++;
    }

    if (logind_group_by & LOGIND_GROUP_BY_TYPE) {
        labels[n].name = "type";
        labels[n].value = type;
        n++;
    }

    if (logind_group_by & LOGIND_GROUP_BY_CLASS) {
        labels[n].name = "class";
        labels[n].value = class;
        n++;
    }

    uint64_t value = 0;

    logind_session_t lk = {0};
    logind_session_t *lv = NULL;

    if (logind_group_by & LOGIND_GROUP_BY_SEAT)
        lk.seat = seat;

    if (logind_group_by & LOGIND_GROUP_BY_REMOTE)
        lk.remote = remote;

    if (logind_group_by & LOGIND_GROUP_BY_TYPE)
        lk.type = type;

    if (logind_group_by & LOGIND_GROUP_BY_CLASS)
        lk.class = class;

    int status = c_avl_get(sessions, &lk, (void *)&lv);
    if (status == 0)
        value = lv->cnt;

    metric_family_append(fam, VALUE_GAUGE(value),
                         &(label_set_t){.ptr = labels, .num = n}, NULL);

    return 0;
}

static int logind_list_seats(sd_bus *bus, char **seats, size_t seats_size)
{
    sd_bus_message *reply = NULL;

    sd_bus_error error = SD_BUS_ERROR_NULL;
    int status = sd_bus_call_method(bus, "org.freedesktop.login1",
                                         "/org/freedesktop/login1",
                                         "org.freedesktop.login1.Manager",
                                         "ListSeats", &error, &reply, NULL);
    if (status < 0)
        return -1;

    sd_bus_error_free(&error);

    status = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(so)");
    if (status < 0) {
        sd_bus_message_unref(reply);
        return -1;
    }

    size_t n = 0;

    while (true) {
        char *seat_id = NULL;
        char *seat_object_path = NULL;

        status = sd_bus_message_read(reply, "(so)", &seat_id, &seat_object_path);
        if (status <= 0)
          break;

        if (n >= seats_size)
            break;

        seats[n] = strdup(seat_id);
        if (seats[n] == NULL)
            break;
        n++;
    }

    sd_bus_message_exit_container(reply);
    sd_bus_message_unref(reply);

    return n;
}

static int logind_list_sessions(sd_bus *bus, c_avl_tree_t *sessions)
{
    sd_bus_message *reply = NULL;

    sd_bus_error error = SD_BUS_ERROR_NULL;
    int status = sd_bus_call_method(bus, "org.freedesktop.login1",
                                         "/org/freedesktop/login1",
                                         "org.freedesktop.login1.Manager",
                                         "ListSessions", &error, &reply, NULL);
    if (status < 0)
        return -1;

    sd_bus_error_free(&error);

    status = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(susso)");
    if (status < 0) {
        sd_bus_message_unref(reply);
        sd_bus_unref(bus);
        return -1;
    }

    sessions_total = 0;

    while (true) {
        char *seat_id = NULL;
        char *session_object_path = NULL;

        status = sd_bus_message_read(reply, "(susso)", NULL, NULL, NULL,
                                                       &seat_id, &session_object_path);
        if (status <= 0)
          break;

        sessions_total++;

        int remote = 0;
        if (logind_group_by & LOGIND_GROUP_BY_REMOTE) {
            status = get_property_bool(bus, "org.freedesktop.login1", session_object_path,
                                        "org.freedesktop.login1.Session", "Remote", &remote);
            if (status != 0)
                break;
        }

        char *type = NULL;
        if (logind_group_by & LOGIND_GROUP_BY_TYPE) {
            status = get_property_string(bus, "org.freedesktop.login1", session_object_path,
                                              "org.freedesktop.login1.Session", "Type", &type);
            if (status != 0)
                break;
        }

        char *class = NULL;
        if (logind_group_by & LOGIND_GROUP_BY_CLASS) {
            status = get_property_string(bus, "org.freedesktop.login1", session_object_path,
                                              "org.freedesktop.login1.Session", "Class", &class);
            if (status != 0) {
                free(type);
                break;
            }
        }

        logind_session_inc(sessions, seat_id, remote, type, class);

        free(type);
        free(class);
    }

    sd_bus_message_exit_container(reply);
    sd_bus_message_unref(reply);

    return 0;
}

static int logind_read(void)
{
    sd_bus *bus = NULL;

    if (sd_booted() <= 0)
        return -1;

    c_avl_tree_t *sessions = c_avl_create((int (*)(const void *, const void *))logind_session_cmp);
    if (sessions == NULL)
        return -1;

    sd_bus_default_system(&bus);

    char *seats[1024];
    size_t seats_size = 1;
    seats[0] = "none";

    if (logind_group_by & LOGIND_GROUP_BY_SEAT)
        seats_size += logind_list_seats(bus, seats + 1,  STATIC_ARRAY_SIZE(seats) - 1);

    logind_list_sessions(bus, sessions);

    sd_bus_unref(bus);

    logind_group_by &= (LOGIND_GROUP_BY_SEAT | LOGIND_GROUP_BY_REMOTE |
                        LOGIND_GROUP_BY_TYPE | LOGIND_GROUP_BY_CLASS);

    if (logind_group_by == (LOGIND_GROUP_BY_SEAT | LOGIND_GROUP_BY_REMOTE |
                            LOGIND_GROUP_BY_TYPE | LOGIND_GROUP_BY_CLASS)) {
        for (size_t i = 0; i < seats_size; i++) {
            for (size_t j = 0; j < session_types_size; j++) {
                for (size_t k = 0; k < session_classes_size; k++) {
                    for (size_t m = 0; m < session_remote_size; m++) {
                        logind_submit(&fams[FAM_LOGIND_SESSIONS], sessions, seats[i],
                                      session_types[j], session_classes[k], m);
                    }
                }
            }
        }
    } else if (logind_group_by == (LOGIND_GROUP_BY_REMOTE | LOGIND_GROUP_BY_TYPE |
                                   LOGIND_GROUP_BY_CLASS)) {
        for (size_t j = 0; j < session_types_size; j++) {
            for (size_t k = 0; k < session_classes_size; k++) {
                for (size_t m = 0; m < session_remote_size; m++) {
                    logind_submit(&fams[FAM_LOGIND_SESSIONS], sessions, NULL, session_types[j],
                                  session_classes[k], m);
                }
            }
        }
    } else if (logind_group_by == (LOGIND_GROUP_BY_SEAT | LOGIND_GROUP_BY_REMOTE |
                                   LOGIND_GROUP_BY_TYPE)) {
        for (size_t i = 0; i < seats_size; i++) {
            for (size_t j = 0; j < session_types_size; j++) {
                for (size_t m = 0; m < session_remote_size; m++) {
                    logind_submit(&fams[FAM_LOGIND_SESSIONS], sessions, seats[i],
                                  session_types[j], NULL, m);
                }
            }
        }
    } else if (logind_group_by == (LOGIND_GROUP_BY_SEAT | LOGIND_GROUP_BY_REMOTE |
                                   LOGIND_GROUP_BY_CLASS)) {
        for (size_t i = 0; i < seats_size; i++) {
            for (size_t k = 0; k < session_classes_size; k++) {
                for (size_t m = 0; m < session_remote_size; m++) {
                    logind_submit(&fams[FAM_LOGIND_SESSIONS], sessions, seats[i],
                                  NULL, session_classes[k], m);
                }
            }
        }
    } else if (logind_group_by == (LOGIND_GROUP_BY_SEAT | LOGIND_GROUP_BY_TYPE |
                                   LOGIND_GROUP_BY_CLASS)) {
        for (size_t i = 0; i < seats_size; i++) {
            for (size_t j = 0; j < session_types_size; j++) {
                for (size_t k = 0; k < session_classes_size; k++) {
                    logind_submit(&fams[FAM_LOGIND_SESSIONS], sessions, seats[i],
                                  session_types[j], session_classes[k], 0);
                }
            }
        }
    } else if (logind_group_by == (LOGIND_GROUP_BY_TYPE | LOGIND_GROUP_BY_CLASS)) {
        for (size_t j = 0; j < session_types_size; j++) {
            for (size_t k = 0; k < session_classes_size; k++) {
                logind_submit(&fams[FAM_LOGIND_SESSIONS], sessions, NULL,
                              session_types[j], session_classes[k], 0);
            }
        }
    } else if (logind_group_by == (LOGIND_GROUP_BY_TYPE | LOGIND_GROUP_BY_REMOTE)) {
        for (size_t j = 0; j < session_types_size; j++) {
            for (size_t m = 0; m < session_remote_size; m++) {
                logind_submit(&fams[FAM_LOGIND_SESSIONS], sessions, NULL,
                              session_types[j], NULL, m);
            }
        }
    } else if (logind_group_by == (LOGIND_GROUP_BY_SEAT | LOGIND_GROUP_BY_REMOTE )) {
        for (size_t i = 0; i < seats_size; i++) {
            for (size_t m = 0; m < session_remote_size; m++) {
                logind_submit(&fams[FAM_LOGIND_SESSIONS], sessions, seats[i], NULL, NULL, m);
            }
        }
    } else if (logind_group_by == (LOGIND_GROUP_BY_SEAT | LOGIND_GROUP_BY_TYPE)) {
        for (size_t i = 0; i < seats_size; i++) {
            for (size_t j = 0; j < session_types_size; j++) {
                logind_submit(&fams[FAM_LOGIND_SESSIONS], sessions, seats[i], session_types[j],
                              NULL, 0);
            }
        }
    } else if (logind_group_by == (LOGIND_GROUP_BY_SEAT | LOGIND_GROUP_BY_CLASS)) {
        for (size_t i = 0; i < seats_size; i++) {
            for (size_t k = 0; k < session_classes_size; k++) {
                logind_submit(&fams[FAM_LOGIND_SESSIONS], sessions, seats[i],
                              NULL, session_classes[k], 0);
            }
        }
    } else if (logind_group_by == (LOGIND_GROUP_BY_CLASS | LOGIND_GROUP_BY_REMOTE)) {
        for (size_t k = 0; k < session_classes_size; k++) {
            for (size_t m = 0; m < session_remote_size; m++) {
                logind_submit(&fams[FAM_LOGIND_SESSIONS], sessions, NULL, NULL,
                              session_classes[k], m);
            }
        }
    } else if (logind_group_by == LOGIND_GROUP_BY_SEAT) {
        for (size_t i = 0; i < seats_size; i++) {
            logind_submit(&fams[FAM_LOGIND_SESSIONS], sessions, seats[i], NULL, NULL, 0);
        }
    } else if (logind_group_by == LOGIND_GROUP_BY_REMOTE) {
        for (size_t m = 0; m < session_remote_size; m++) {
            logind_submit(&fams[FAM_LOGIND_SESSIONS], sessions, NULL, NULL, NULL, m);
        }
    } else if (logind_group_by == LOGIND_GROUP_BY_TYPE) {
        for (size_t j = 0; j < session_types_size; j++) {
            logind_submit(&fams[FAM_LOGIND_SESSIONS], sessions, NULL, session_types[j], NULL, 0);
        }
    } else if (logind_group_by == LOGIND_GROUP_BY_CLASS) {
        for (size_t k = 0; k < session_classes_size; k++) {
            logind_submit(&fams[FAM_LOGIND_SESSIONS], sessions, NULL, NULL, session_classes[k], 0);
        }
    } else {
        metric_family_append(&fams[FAM_LOGIND_SESSIONS], VALUE_GAUGE(sessions_total), NULL, NULL);
    }

    while (true) {
        logind_session_t *lk = NULL;
        logind_session_t *lv = NULL;
        int status = c_avl_pick(sessions, (void *)&lk, (void *)&lv);
        if (status != 0)
            break;
        logind_session_free(lk);
    }

    c_avl_destroy(sessions);

    for (size_t i = 1; i < seats_size; i++) {
        if (seats[i] == NULL)
            break;
        free(seats[i]);
    }

    plugin_dispatch_metric_family_array(fams, FAM_LOGIND_MAX, 0);

    return 0;
}

static int logind_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "group-by") == 0) {
            status = cf_util_get_flags(child, logind_flags, logind_flags_size, &logind_group_by);
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

void module_register(void)
{
    plugin_register_config("logind", logind_config);
    plugin_register_read("logind", logind_read);
}
