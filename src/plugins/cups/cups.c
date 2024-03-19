// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/avltree.h"
#include "libutils/complain.h"

#include <cups/cups.h>

enum {
    FAM_CUPS_UP,
    FAM_CUPS_PRINTER_STATUS,
    FAM_CUPS_PRINTER_ACCEPTING_JOBS,
    FAM_CUPS_PRINTER_JOBS_PENDING,
    FAM_CUPS_PRINTER_JOBS_HELD,
    FAM_CUPS_PRINTER_JOBS_PROCESSING,
    FAM_CUPS_MAX,
};

static metric_family_t fams[FAM_CUPS_MAX] = {
    [FAM_CUPS_UP] =  {
        .name = "cups_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the cups server be reached.",
    },
    [FAM_CUPS_PRINTER_ACCEPTING_JOBS] = {
        .name = "cups_printer_accepting_jobs",
        .type = METRIC_TYPE_GAUGE,
        .help = "Printer accepting jobs.",
    },
    [FAM_CUPS_PRINTER_STATUS] = {
        .name = "cups_printer_status",
        .type = METRIC_TYPE_STATE_SET,
        .help = "Printer status.",
    },
    [FAM_CUPS_PRINTER_JOBS_PENDING] = {
        .name = "cups_printer_jobs_pending",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of jobs in pending state.",
    },
    [FAM_CUPS_PRINTER_JOBS_HELD] = {
        .name = "cups_printer_jobs_held",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of jobs in held state.",
    },
    [FAM_CUPS_PRINTER_JOBS_PROCESSING] = {
        .name = "cups_printer_jobs_processing",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of jobs in processing state.",
    }
};

typedef struct {
    char *name;
    char *host;
    int port;
    cdtime_t timeout;
    http_t *http;
    label_set_t labels;
    metric_family_t fams[FAM_CUPS_MAX];
} cups_instance_t;

typedef struct {
    uint64_t pending;
    uint64_t held;
    uint64_t processing;
    char dest[];
} jstate_t;

static int cups_read_instance(user_data_t *user_data)
{
    cups_instance_t *ins = user_data->data;

    cdtime_t submit = cdtime();

    if (ins->http == NULL) {
        ins->http = httpConnect2(ins->host, ins->port, NULL, AF_UNSPEC, cupsEncryption(),
                                 0, CDTIME_T_TO_MS(ins->timeout), NULL);
        if (ins->http == NULL) {
            metric_family_append(&ins->fams[FAM_CUPS_UP], VALUE_GAUGE(0), &ins->labels, NULL);
            int status = plugin_dispatch_metric_family(&ins->fams[FAM_CUPS_UP], 0);
            if (status != 0)
                PLUGIN_ERROR("plugin_dispatch_metric_family failed: %s", STRERROR(status));
            return 0;
        }
    }

    metric_family_append(&ins->fams[FAM_CUPS_UP], VALUE_GAUGE(1), &ins->labels, NULL);

    c_avl_tree_t *tree = c_avl_create((int (*)(const void *, const void *))strcmp);
    if (unlikely(tree == NULL)) {
        PLUGIN_ERROR("failed to create avl tree");
        return 0;
    }

    cups_dest_t *dest = NULL;
    int n_dest = cupsGetDests2(ins->http, &dest);
    if (n_dest > 0) {
        for (int n = 0; n < n_dest ; n++) {
            cups_option_t *options = dest[n].options;
            int n_options = dest[n].num_options;
            char *dest_name = dest[n].name;

            //if (dest_name ==

            if (cupsGetOption("printer-uri-supported", n_options, options) == NULL)
                continue;

            size_t len = strlen(dest_name);
            jstate_t *jstate = calloc(1, sizeof(*jstate)+len+1);
            if (unlikely(jstate == NULL))
                continue;

            memcpy(jstate->dest, dest_name, len+1);
            int status = c_avl_insert(tree, jstate->dest, jstate);
            if (unlikely(status != 0)) {
                free(jstate);
                continue;
            }

            const char *option;
            option = cupsGetOption("printer-is-accepting-jobs", n_options, options);
            if ((option != NULL) && !strcmp(option, "true")) {
                metric_family_append(&ins->fams[FAM_CUPS_PRINTER_ACCEPTING_JOBS],
                                     VALUE_GAUGE(1), &ins->labels,
                                     &(label_pair_const_t){.name="printer", .value=dest_name}, NULL);
            } else {
                metric_family_append(&ins->fams[FAM_CUPS_PRINTER_ACCEPTING_JOBS],
                                     VALUE_GAUGE(0), &ins->labels,
                                     &(label_pair_const_t){.name="printer", .value=dest_name}, NULL);
            }


            state_t states[] = {
                { .name = "IDLE",       .enabled = false },
                { .name = "PROCESSING", .enabled = false },
                { .name = "STOPPED",    .enabled = false },
            };
            state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };

            option = cupsGetOption("printer-state", n_options, options);
            if (option != NULL) {
                int pstate = atoi(option);
                switch (pstate) {
                case IPP_PSTATE_IDLE:
                    states[0].enabled = true;
                    break;
                case IPP_PSTATE_PROCESSING:
                    states[1].enabled = true;
                    break;
                case IPP_PSTATE_STOPPED:
                    states[2].enabled = true;
                    break;
                }
            }

            metric_family_append(&ins->fams[FAM_CUPS_PRINTER_STATUS],
                                 VALUE_STATE_SET(set), &ins->labels,
                                 &(label_pair_const_t){.name="printer", .value=dest_name}, NULL);

        }
        cupsFreeDests(n_dest, dest);
    }

    cups_job_t *jobs = NULL;
    int n_jobs = cupsGetJobs2(ins->http, &jobs, NULL, 0, CUPS_WHICHJOBS_ACTIVE);
    if (n_jobs > 0) {
        for (int n = 0; n < n_jobs ; n++) {
            char *dest_name = jobs[n].dest;

            jstate_t *jstate = NULL;
            int status = c_avl_get(tree, dest_name, (void *)&jstate);
            if (status != 0)
                continue;
            if (jstate == NULL)
                continue;

            switch (jobs[n].state) {
            case IPP_JSTATE_PENDING:
                jstate->pending++;
                break;
            case IPP_JSTATE_HELD:
                jstate->held++;
                break;
            case IPP_JSTATE_PROCESSING:
                jstate->processing++;
                break;
            default:
                break;
            }
        }
        cupsFreeJobs(n_jobs, jobs);
    }

    while (true) {
        jstate_t *jstate = NULL;
        char *key = NULL;
        int status = c_avl_pick(tree, (void *)&key, (void *)&jstate);
        if (status != 0)
            break;
        metric_family_append(&ins->fams[FAM_CUPS_PRINTER_JOBS_PENDING],
                             VALUE_GAUGE(jstate->pending), &ins->labels,
                             &(label_pair_const_t){.name="printer", .value=key}, NULL);
        metric_family_append(&ins->fams[FAM_CUPS_PRINTER_JOBS_HELD],
                             VALUE_GAUGE(jstate->held), &ins->labels,
                             &(label_pair_const_t){.name="printer", .value=key}, NULL);
        metric_family_append(&ins->fams[FAM_CUPS_PRINTER_JOBS_PROCESSING],
                             VALUE_GAUGE(jstate->processing), &ins->labels,
                             &(label_pair_const_t){.name="printer", .value=key}, NULL);
        free(jstate);
    }

    c_avl_destroy(tree);

    plugin_dispatch_metric_family_array(ins->fams, FAM_CUPS_MAX, submit);

    return 0;
}

static void cups_instance_free(void *data)
{
    cups_instance_t *ins = data;

    if (ins == NULL)
        return;

    free(ins->name);
    free(ins->host);

    if (ins->http != NULL)
        httpClose(ins->http);

    free(ins);
}

static int cups_config_instance(config_item_t *ci)
{
    cups_instance_t *ins = calloc(1, sizeof(*ins));
    if (ins == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    ins->name = NULL;
    int status = cf_util_get_string(ci, &ins->name);
    if (status != 0) {
        cups_instance_free(ins);
        return status;
    }
    ins->timeout = TIME_T_TO_CDTIME_T(1);

    memcpy(ins->fams, fams, sizeof(ins->fams[0])*FAM_CUPS_MAX);

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp("host", child->key) == 0) {
            status = cf_util_get_string(child, &ins->host);
        } else if (strcasecmp("port", child->key) == 0) {
            status = cf_util_get_port_number(child, &ins->port);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ins->labels);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &ins->timeout);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else {
            PLUGIN_ERROR("Unknown config option: %s", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }
    if (status == 0)
        status = label_set_add(&ins->labels, true, "instance", ins->name);

    if (status != 0) {
        cups_instance_free(ins);
        return status;
    }

    if (ins->host == NULL)
        ins->host = strdup(cupsServer());
    if (ins->port == 0)
        ins->port = ippPort();

    return plugin_register_complex_read("cups", ins->name, cups_read_instance, interval,
                                     &(user_data_t){.data = ins, .free_func = cups_instance_free});
}

static int cups_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = cups_config_instance(child);
        } else {
            PLUGIN_ERROR("Unknown config option: %s", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("cups", cups_config);
}
