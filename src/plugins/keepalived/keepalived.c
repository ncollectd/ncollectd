// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#define KEEPALIVED_STATS_FILE "/tmp/keepalived.stats"
#define KEEPALIVED_DATA_FILE "/tmp/keepalived.data"
#define KEEPALIVED_PID_FILE "/var/run/keepalived.pid"

enum {
    FAM_KEEPALIVED_UP,
    FAM_KEEPALIVED_VRRP_STATE,
    FAM_KEEPALIVED_GRATUITOUS_ARP_DELAY,
    FAM_KEEPALIVED_ADVERTISEMENTS_RECEIVE,
    FAM_KEEPALIVED_ADVERTISEMENTS_SENT,
    FAM_KEEPALIVED_BECOME_MASTER,
    FAM_KEEPALIVED_RELEASE_MASTER,
    FAM_KEEPALIVED_PACKET_LENGTH_ERRORS,
    FAM_KEEPALIVED_ADVERTISEMENTS_INTERVAL_ERRORS,
    FAM_KEEPALIVED_IP_TTL_ERRORS,
    FAM_KEEPALIVED_INVALID_TYPE_RECEIVED,
    FAM_KEEPALIVED_ADDRESS_LIST_ERRORS,
    FAM_KEEPALIVED_AUTHENTICATION_INVALID,
    FAM_KEEPALIVED_AUTHENTICATION_MISMATCH,
    FAM_KEEPALIVED_AUTHENTICATION_FAILURE,
    FAM_KEEPALIVED_PRIORITY_ZERO_RECEIVED,
    FAM_KEEPALIVED_PRIORITY_ZERO_SENT,
    FAM_KEEPALIVED_SCRIPT_STATUS,
    FAM_KEEPALIVED_SCRIPT_STATE,
    FAM_KEEPALIVED_MAX,
};

static metric_family_t fams[FAM_KEEPALIVED_MAX] = {
    [FAM_KEEPALIVED_UP] = {
        .name = "keepalived_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the keepalived server be reached."
    },
    [FAM_KEEPALIVED_VRRP_STATE] = {
        .name = "keepalived_vrrp_state",
        .type = METRIC_TYPE_STATE_SET,
        .help =  "State of vrrp",
    },
    [FAM_KEEPALIVED_GRATUITOUS_ARP_DELAY] = {
        .name = "keepalived_gratuitous_arp_delay",
        .type = METRIC_TYPE_COUNTER,
        .help = "Gratuitous ARP delay",
    },
    [FAM_KEEPALIVED_ADVERTISEMENTS_RECEIVE] = {
        .name = "keepalived_advertisements_received",
        .type = METRIC_TYPE_COUNTER,
        .help = "Advertisements received",
    },
    [FAM_KEEPALIVED_ADVERTISEMENTS_SENT] = {
        .name = "keepalived_advertisements_sent",
        .help = "Advertisements sent",
        .type = METRIC_TYPE_COUNTER,
    },
    [FAM_KEEPALIVED_BECOME_MASTER] = {
        .name = "keepalived_become_master",
        .type = METRIC_TYPE_COUNTER,
        .help = "Became master",
    },
    [FAM_KEEPALIVED_RELEASE_MASTER] = {
        .name = "keepalived_release_master",
        .type = METRIC_TYPE_COUNTER,
        .help = "Released master",
    },
    [FAM_KEEPALIVED_PACKET_LENGTH_ERRORS] = {
        .name = "keepalived_packet_length_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Packet length errors",
    },
    [FAM_KEEPALIVED_ADVERTISEMENTS_INTERVAL_ERRORS] = {
        .name = "keepalived_advertisements_interval_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Advertisement interval errors",
    },
    [FAM_KEEPALIVED_IP_TTL_ERRORS] = {
        .name = "keepalived_ip_ttl_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "TTL errors",
    },
    [FAM_KEEPALIVED_INVALID_TYPE_RECEIVED] = {
        .name = "keepalived_invalid_type_received",
        .type = METRIC_TYPE_COUNTER,
        .help = "Invalid type errors",
    },
    [FAM_KEEPALIVED_ADDRESS_LIST_ERRORS] = {
        .name = "keepalived_address_list_errors",
        .type = METRIC_TYPE_COUNTER,
        .help = "Address list errors",
    },
    [FAM_KEEPALIVED_AUTHENTICATION_INVALID] = {
        .name = "keepalived_authentication_invalid",
        .type = METRIC_TYPE_COUNTER,
        .help = "Authentication invalid",
    },
    [FAM_KEEPALIVED_AUTHENTICATION_MISMATCH] = {
        .name = "keepalived_authentication_mismatch",
        .type = METRIC_TYPE_COUNTER,
        .help = "Authentication mismatch",
    },
    [FAM_KEEPALIVED_AUTHENTICATION_FAILURE] = {
        .name = "keepalived_authentication_failure",
        .type = METRIC_TYPE_COUNTER,
        .help = "Authentication failure",
    },
    [FAM_KEEPALIVED_PRIORITY_ZERO_RECEIVED] = {
        .name = "keepalived_priority_zero_received",
        .type = METRIC_TYPE_COUNTER,
        .help = "Priority zero received",
    },
    [FAM_KEEPALIVED_PRIORITY_ZERO_SENT] = {
        .name = "keepalived_priority_zero_sent",
        .type = METRIC_TYPE_COUNTER,
        .help = "Priority zero sent",
    },
    [FAM_KEEPALIVED_SCRIPT_STATUS] = {
        .name = "keepalived_script_status",
        .type = METRIC_TYPE_STATE_SET,
        .help = "Tracker Script Status",
    },
    [FAM_KEEPALIVED_SCRIPT_STATE] = {
        .name = "keepalived_script_state",
        .type = METRIC_TYPE_STATE_SET,
        .help = "Tracker Script State",
    },
};

typedef enum {
    KEEPALIVED_STATS_SECTION_NONE,
    KEEPALIVED_STATS_SECTION_ADVERTISEMENTS,
    KEEPALIVED_STATS_SECTION_PACKET_ERRORS,
    KEEPALIVED_STATS_SECTION_AUTHENTICATION_ERRORS,
    KEEPALIVED_STATS_SECTION_PRIORITY_ZERO,
} keepalived_stats_section_t;

typedef enum {
    KEEPALIVED_DATA_SECTION_NONE,
    KEEPALIVED_DATA_SECTION_INSTANCE,
    KEEPALIVED_DATA_SECTION_SCRIPT,
} keepalived_data_section_t;

typedef struct {
    char *instance;
    char *stats_path;
    char *data_path;
    char *pid_path;
    label_set_t labels;
    metric_family_t fams[FAM_KEEPALIVED_MAX];
} keepalived_t;

static int dump_file(pid_t pid, int signal, const char *filename)
{
    bool found = true;
    struct stat sb1 = {0};
    int status = stat(filename, &sb1);
    if (status != 0)
        found = false;

    status = kill(pid, SIGUSR1);
    if (status != 0)  {
        PLUGIN_ERROR("Cannot send signal %d to %d", signal, pid);
        return -1;
    }

    for (int i = 0; i < 100 ; i++) {
        usleep(10000);

        struct stat sb2 = {0};
        status = stat(filename, &sb2);
        if (status == 0) {
            if (!found)
                return 0;
            if (sb1.st_mtime != sb2.st_mtime)
                return 0;
        }
    }

    PLUGIN_ERROR("Cannot get a new data for: \"%s\"", filename);

    return -1;
}

static void split_kv(char *line, char **rkey, char **rval, char sep)
{
    char *key = line;
    *rkey = key;

    *rval = NULL;
    char *end = strchr(key, sep);
    if (end == NULL)
       return;

    char *val = end+1;

    do {
        *end = '\0';
        end--;
    } while ((*end == ' ') && (end != line));

    while ((*val == ' ') || (*val == '\n')) {
        val++;
    }

    if (*val == '\0')
      return;

    *rval = val;


}

static int read_keepalived_data(keepalived_t *kpd, int pid)
{
    int status = dump_file(pid, SIGUSR1, kpd->data_path);
    if (status != 0)
        return -1;

    FILE *fh = fopen(kpd->data_path, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("fopen (%s): %s", kpd->data_path, STRERRNO);
        return -1;
    }

    keepalived_data_section_t section = KEEPALIVED_DATA_SECTION_NONE;

    char buffer[1024];
    char iname[256] = "";
    char sname[256] = "";
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *key, *val;
        split_kv(buffer, &key, &val, '=');
        if ((key == NULL) || (val == NULL))
            continue;

        if ((key[0] == ' ') && (key[1] != ' ')) {
            key += 1;
            if (strcmp("VRRP Instance", key) == 0) {
                section = KEEPALIVED_DATA_SECTION_INSTANCE;
                iname[0] = '\0';
                if (val != NULL)
                    sstrncpy(iname, val, sizeof(iname));
            } else if (strcmp("VRRP Script", key) == 0) {
                section = KEEPALIVED_DATA_SECTION_SCRIPT;
                sname[0] = '\0';
                if (val != NULL)
                    sstrncpy(sname, val, sizeof(sname));
            }
        } else if ((key[0] == ' ') && (key[1] == ' ') && (key[2] != ' ')) {
            key += 2;

            switch (section) {
            case KEEPALIVED_DATA_SECTION_INSTANCE:
                if (strcmp("State", key) == 0) {
                    state_t states[] = {
                        { .name = "INIT",   .enabled = false },
                        { .name = "BACKUP", .enabled = false },
                        { .name = "MASTER", .enabled = false },
                        { .name = "FAULT",  .enabled = false },
                    };
                    state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
                    for (size_t j = 0; j < set.num ; j++) {
                        if (strcmp(set.ptr[j].name, val) == 0) {
                            set.ptr[j].enabled = true;
                            break;
                        }
                    }
                    metric_family_append(&kpd->fams[FAM_KEEPALIVED_VRRP_STATE],
                                         VALUE_STATE_SET(set), &kpd->labels,
                                         &(label_pair_const_t){.name="iname", .value=iname },
                                         NULL);
                } else if (strcmp("Gratuitous ARP delay", key) == 0) {
                    uint64_t value = 0;
                    if (strtouint(val, &value) == 0)
                        metric_family_append(&kpd->fams[FAM_KEEPALIVED_GRATUITOUS_ARP_DELAY],
                                             VALUE_COUNTER(value), &kpd->labels,
                                             &(label_pair_const_t){.name="iname", .value=iname },
                                             NULL);

                }
                break;
            case KEEPALIVED_DATA_SECTION_SCRIPT:
                if (strcmp("Status", key) == 0) {
                    state_t states[] = {
                        { .name = "BAD",  .enabled = false },
                        { .name = "GOOD", .enabled = false },
                    };
                    state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
                    for (size_t j = 0; j < set.num ; j++) {
                        if (strcmp(set.ptr[j].name, val) == 0) {
                            set.ptr[j].enabled = true;
                            break;
                        }
                    }
                    metric_family_append(&kpd->fams[FAM_KEEPALIVED_SCRIPT_STATUS],
                                         VALUE_STATE_SET(set), &kpd->labels,
                                         &(label_pair_const_t){.name="script", .value=sname },
                                         NULL);
                } else if (strcmp("State", key) == 0) {
                    state_t states[] = {
                        { .name = "idle",                  .enabled = false },
                        { .name = "running",               .enabled = false },
                        { .name = "requested termination", .enabled = false },
                        { .name = "forcing termination",   .enabled = false },
                    };
                    state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };
                    for (size_t j = 0; j < set.num ; j++) {
                        if (strcmp(set.ptr[j].name, val) == 0) {
                            set.ptr[j].enabled = true;
                            break;
                        }
                    }
                    metric_family_append(&kpd->fams[FAM_KEEPALIVED_SCRIPT_STATE],
                                         VALUE_STATE_SET(set), &kpd->labels,
                                         &(label_pair_const_t){.name="script", .value=sname },
                                         NULL);

                }
                break;
            default:
                break;
            }
        }
    }

    fclose(fh);
    return 0;
}

static int read_keepalived_stats(keepalived_t *kpd, int pid)
{
    int status = dump_file(pid, SIGUSR2, kpd->stats_path);
    if (status != 0)
        return -1;

    FILE *fh = fopen(kpd->stats_path, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("fopen (%s): %s", kpd->stats_path, STRERRNO);
        return -1;
    }

    keepalived_stats_section_t section = KEEPALIVED_STATS_SECTION_NONE;

    char buffer[1024];
    char iname[256] = "";
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *key, *val;
        split_kv(buffer, &key, &val, ':');
        if (key == NULL)
            continue;

        int fam = -1;

        if (key[0] != ' ') {
            section = KEEPALIVED_STATS_SECTION_NONE;
            if (strcmp("VRRP Instance", key) == 0) {
                iname[0] = '\0';
                if (val != NULL)
                    sstrncpy(iname, val, sizeof(iname));
            }
        } else if ((key[0] == ' ') && (key[1] == ' ') && (key[2] != ' ')) {
            key += 2;
            section = KEEPALIVED_STATS_SECTION_NONE;

            if (strcmp("Advertisements", key) == 0) {
                section = KEEPALIVED_STATS_SECTION_ADVERTISEMENTS;
            } else if (strcmp("Became master", key) == 0) {
                fam = FAM_KEEPALIVED_BECOME_MASTER;
            } else if (strcmp("Released master", key) == 0) {
                fam = FAM_KEEPALIVED_RELEASE_MASTER;
            } else if (strcmp("Packet Errors", key) == 0) {
                section = KEEPALIVED_STATS_SECTION_PACKET_ERRORS;
            } else if (strcmp("Authentication Errors", key) == 0) {
                section = KEEPALIVED_STATS_SECTION_AUTHENTICATION_ERRORS;
            } else if (strcmp("Priority Zero", key) == 0) {
                section = KEEPALIVED_STATS_SECTION_PRIORITY_ZERO;
            }
        } else if ((key[0] == ' ') && (key[1] == ' ') && (key[2] == ' ') && (key[3] == ' ')) {
            key += 4;

            switch(section) {
            case KEEPALIVED_STATS_SECTION_ADVERTISEMENTS:
                if (strcmp("Received", key) == 0) {
                    fam = FAM_KEEPALIVED_ADVERTISEMENTS_RECEIVE;
                } else if (strcmp("Sent", key) == 0) {
                    fam = FAM_KEEPALIVED_ADVERTISEMENTS_SENT;
                }
                break;
            case KEEPALIVED_STATS_SECTION_PACKET_ERRORS:
                if (strcmp("Length", key) == 0) {
                    fam = FAM_KEEPALIVED_PACKET_LENGTH_ERRORS;
                } else if (strcmp("TTL", key) == 0) {
                    fam = FAM_KEEPALIVED_IP_TTL_ERRORS;
                } else if (strcmp("Invalid Type", key) == 0) {
                    fam = FAM_KEEPALIVED_INVALID_TYPE_RECEIVED;
                } else if (strcmp("Advertisement Interval", key) == 0) {
                    fam = FAM_KEEPALIVED_ADVERTISEMENTS_INTERVAL_ERRORS;
                } else if (strcmp("Address List", key) == 0) {
                    fam = FAM_KEEPALIVED_ADDRESS_LIST_ERRORS;
                }
                break;
            case KEEPALIVED_STATS_SECTION_AUTHENTICATION_ERRORS:
                if (strcmp("Invalid Type", key) == 0) {
                    fam = FAM_KEEPALIVED_AUTHENTICATION_INVALID;
                } else if (strcmp("Type Mismatch", key) == 0) {
                    fam = FAM_KEEPALIVED_AUTHENTICATION_MISMATCH;
                } else if (strcmp("Failure", key) == 0) {
                    fam = FAM_KEEPALIVED_AUTHENTICATION_FAILURE;
                }
                break;
            case KEEPALIVED_STATS_SECTION_PRIORITY_ZERO:
                if (strcmp("Received", key) == 0) {
                    fam = FAM_KEEPALIVED_PRIORITY_ZERO_RECEIVED;
                } else if (strcmp("Sent", key) == 0) {
                    fam = FAM_KEEPALIVED_PRIORITY_ZERO_SENT;
                }
                break;
            default:
                break;
            }
        }

        if ((fam >= 0) && (val != NULL)) {
            if (kpd->fams[fam].type == METRIC_TYPE_COUNTER) {
                uint64_t value = 0;
                if (strtouint(val, &value) == 0)
                    metric_family_append(&kpd->fams[fam], VALUE_COUNTER(value), &kpd->labels,
                                         &(label_pair_const_t){.name="iname", .value=iname },
                                         NULL);
            }
        }
    }

    fclose(fh);
    return 0;
}

static pid_t keepalived_get_pid(keepalived_t *kpd)
{
    int status = access(kpd->pid_path, R_OK);
    if (status != 0) {
        PLUGIN_ERROR("Cannot access '%s': %s", kpd->pid_path, STRERRNO);
        return -1;
    }

    uint64_t rpid = 0;
    status = filetouint(kpd->pid_path, &rpid);
    if (status != 0) {
        PLUGIN_ERROR("Cannot read pid from '%s'", kpd->pid_path);
        return -1;
    }

    if (rpid == 0) {
        PLUGIN_ERROR("Pid in '%s' must be > 0", kpd->pid_path);
        return -1;
    }

    pid_t pid = rpid;

    status = kill(pid, 0);
    if (status != 0)  {
        PLUGIN_ERROR("Cannot send signals to %d", pid);
        return -1;
    }

    return pid;
}

static int keepalived_read(user_data_t *ud)
{
    if ((ud == NULL) || (ud->data == NULL)) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    keepalived_t *kpd = ud->data;

    pid_t pid = keepalived_get_pid(kpd);
    if (pid <= 0) {
        metric_family_append(&kpd->fams[FAM_KEEPALIVED_UP], VALUE_GAUGE(0), &kpd->labels, NULL);
        plugin_dispatch_metric_family(&kpd->fams[FAM_KEEPALIVED_UP], 0);
        return 0;
    }

    double up = 1;

    int status = read_keepalived_data(kpd, pid);
    if (status != 0)
        up = 0;

    status = read_keepalived_stats(kpd, pid);
    if (status != 0)
        up = 0;

    metric_family_append(&kpd->fams[FAM_KEEPALIVED_UP], VALUE_GAUGE(up), &kpd->labels, NULL);

    plugin_dispatch_metric_family_array(kpd->fams, FAM_KEEPALIVED_MAX, 0);

    return 0;
}

static void keepalived_free(void *arg)
{
    if (arg == NULL)
        return;

    keepalived_t *kpd = arg;

    free(kpd->instance);
    free(kpd->stats_path);
    free(kpd->data_path);
    free(kpd->pid_path);

    label_set_reset(&kpd->labels);

    free(kpd);
}

static int keepalived_config_instance(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_WARNING("'instance' blocks need exactly one string argument.");
        return -1;
    }

    keepalived_t *kpd = calloc(1, sizeof(*kpd));
    if (kpd == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    memcpy(kpd->fams, fams, FAM_KEEPALIVED_MAX * sizeof(fams[0]));

    kpd->instance = strdup(ci->values[0].value.string);
    if (kpd->instance == NULL) {
        PLUGIN_ERROR("strdup failed.");
        free(kpd);
        return -1;
    }

    cdtime_t interval = 0;
    int status = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("stats-path", child->key) == 0) {
            status = cf_util_get_string(child, &kpd->stats_path);
        } else if (strcasecmp("data-path", child->key) == 0) {
            status = cf_util_get_string(child, &kpd->data_path);
        } else if (strcasecmp("pid-path", child->key) == 0) {
            status = cf_util_get_string(child, &kpd->pid_path);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &kpd->labels);
        } else {
            PLUGIN_WARNING("Option '%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        keepalived_free(kpd);
        return -1;
    }

    if (kpd->stats_path == NULL) {
        kpd->stats_path = strdup(KEEPALIVED_STATS_FILE);
        if (kpd->stats_path == NULL) {
            PLUGIN_ERROR("strdup failed.");
            keepalived_free(kpd);
            return -1;
        }
    }

    if (kpd->data_path == NULL) {
        kpd->data_path = strdup(KEEPALIVED_DATA_FILE);
        if (kpd->data_path == NULL) {
            PLUGIN_ERROR("strdup failed.");
            keepalived_free(kpd);
            return -1;
        }
    }

    if (kpd->pid_path == NULL) {
        kpd->pid_path = strdup(KEEPALIVED_PID_FILE);
        if (kpd->pid_path == NULL) {
            PLUGIN_ERROR("strdup failed.");
            keepalived_free(kpd);
            return -1;
        }
    }

    label_set_add(&kpd->labels, false, "instance", kpd->instance);

    return plugin_register_complex_read("keepalived", kpd->instance, keepalived_read, interval,
                                        &(user_data_t){.data = kpd, .free_func = keepalived_free});
}

static int keepalived_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = keepalived_config_instance(child);
        } else {
            PLUGIN_WARNING("Option '%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("keepalived", keepalived_config);
}
