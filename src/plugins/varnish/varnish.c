// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2010 Jérôme Renard
// SPDX-FileCopyrightText: Copyright (C) 2010 Marc Fournier
// SPDX-FileCopyrightText: Copyright (C) 2010-2012 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Jérôme Renard <jerome.renard at gmail.com>
// SPDX-FileContributor: Marc Fournier <marc.fournier at camptocamp.com>
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Denes Matetelki <dmatetelki at varnish-software.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#if defined(HAVE_VARNISH_V4) || defined(HAVE_VARNISH_V5) || defined(HAVE_VARNISH_V6)
#include <vapi/vsc.h>
#include <vapi/vsm.h>
typedef struct VSC_C_main c_varnish_stats_t;
#endif

#ifdef HAVE_VARNISH_V3
#include <varnishapi.h>
#include <vsc.h>
typedef struct VSC_C_main c_varnish_stats_t;
#endif

#include "plugins/varnish/varnish_flags.h"
#include "plugins/varnish/varnish_fam.h"

struct varnish_stats_metric {
    char *key;
    uint64_t flag;
    int fam;
    char *lkey;
    char *lvalue;
    char *tag1;
    char *tag2;
    char *tag3;
};

const struct varnish_stats_metric *
    varnish_stats_get_key (register const char *str, register size_t len);

extern metric_family_t fams[FAM_VARNISH_MAX];

static cf_flags_t cvarnish_flags[] = {
    { "backend",     COLLECT_BACKEND     },
    { "cache",       COLLECT_CACHE       },
    { "connections", COLLECT_CONNECTIONS },
    { "dirdns",      COLLECT_DIRDNS      },
    { "esi",         COLLECT_ESI         },
    { "fetch",       COLLECT_FETCH       },
    { "hcb",         COLLECT_HCB         },
    { "objects",     COLLECT_OBJECTS     },
    { "ban",         COLLECT_BANS        },
    { "session",     COLLECT_SESSION     },
    { "shm",         COLLECT_SHM         },
    { "sma",         COLLECT_SMA         },
    { "sms",         COLLECT_SMS         },
    { "struct",      COLLECT_STRUCT      },
    { "totals",      COLLECT_TOTALS      },
    { "uptime",      COLLECT_UPTIME      },
    { "vcl",         COLLECT_VCL         },
    { "workers",     COLLECT_WORKERS     },
    { "vsm",         COLLECT_VSM         },
    { "lck",         COLLECT_LCK         },
    { "mempool",     COLLECT_MEMPOOL     },
    { "mgt",         COLLECT_MGT         },
    { "smf",         COLLECT_SMF         },
    { "vbe",         COLLECT_VBE         },
    { "mse",         COLLECT_MSE         },
    { "goto",        COLLECT_GOTO        },
    { "smu",         COLLECT_SMU         },
    { "brotli",      COLLECT_BROTLI      },
    { "accg_diag",   COLLECT_ACCG_DIAG   },
    { "accg",        COLLECT_ACCG        },
    { "workspace",   COLLECT_WORKSPACE   },
};
static size_t cvarnish_flags_size = STATIC_ARRAY_SIZE(cvarnish_flags);

typedef struct {
    char *instance;
    char *vsh_instance;
    label_set_t labels;
    plugin_filter_t *filter;
    uint64_t flags;
    metric_family_t fams[FAM_VARNISH_MAX];
} varnish_instance_t;

static int varnish_monitor(void *priv, const struct VSC_point *const pt)
{
    const char *tokens[12] = {NULL};
    size_t tokens_num = 0;

    if (unlikely(pt == NULL))
        return 0;

#if defined(HAVE_VARNISH_V6) | defined(HAVE_VARNISH_V5)
    char buffer[1024];
    sstrncpy(buffer, pt->name, sizeof(buffer));

    char *ptr = buffer;

    tokens[tokens_num] = ptr;
    tokens_num++;

    char *end = NULL;
    int sep = '.';
    while ((end = strchr(ptr, sep)) != NULL) {
        *(end++) = '\0';
        if ((sep == ')') && (*end == '.')) end++;
        ptr = end;
        if (*ptr == '(') {
            sep = ')';
            if (tokens_num < STATIC_ARRAY_SIZE(tokens)) {
                tokens[tokens_num] = ++ptr;
                tokens_num++;
            }
        } else {
            sep = '.';
            if (tokens_num < STATIC_ARRAY_SIZE(tokens)) {
                 tokens[tokens_num] = ptr;
                    tokens_num++;
            }
        }
    }

    if ((tokens_num < 2) || (tokens_num > STATIC_ARRAY_SIZE(tokens))) {
        return 0;
    }

#elif defined(HAVE_VARNISH_V4)
    if (pt->section->fantom->type[0] == 0)
        return 0;

    tokens[0] = pt->section->fantom->type;
    tokens_num++;

    if (pt->section->fantom->ident[0] != 0) {
        tokens[tokens_num] = pt->section->fantom->ident;
        tokens_num++;
    }

    tokens[tokens_num] = pt->desc->name;
    tokens_num++;
#elif defined(HAVE_VARNISH_V3)
    if (pt->class[0] == 0)
        return 0;

    tokens[0] = pt->class;
    tokens_num++;

    if (pt->ident[0] != 0) {
        tokens[tokens_num] = pt->ident;
        tokens_num++;
    }

    tokens[tokens_num] = pt->name;
    tokens_num++;
#endif

    varnish_instance_t *conf = priv;
    uint64_t val = *(const volatile uint64_t *)pt->ptr;

    char mname[256];
    int size = ssnprintf(mname, sizeof(mname), "%s.%s", tokens[0], tokens[tokens_num-1]);
    if (size >= (int)sizeof(mname)) {
        PLUGIN_ERROR("overflow in the concatenation \"%s.%s\"", tokens[0], tokens[tokens_num-1]);
        return -1;
    }

    const struct varnish_stats_metric *vsh_metric = varnish_stats_get_key (mname, strlen(mname));
    if (unlikely(vsh_metric != NULL)) {
        metric_family_t *fam = &(conf->fams[vsh_metric->fam]);

        /* special cases */
        if ((tokens_num >= 7) &&
            (strcmp(tokens[0], "VBE") == 0) && (strcmp(tokens[2], "goto") == 0)) {
            tokens[2] = tokens[4];
            tokens[3] = tokens[5];
        } else if ((tokens_num >= 4) && (strcmp(tokens[0], "LCK") == 0)) {
            const char *swap = tokens[1];
            tokens[1] = tokens[2];
            tokens[2] = swap;
        }

        metric_t m = {0};

        if (vsh_metric->fam == FAM_VARNISH_VBE_UP) {
            m.value = VALUE_GAUGE(val & 1);
        } else {
            if (fam->type == METRIC_TYPE_GAUGE)
                m.value = VALUE_GAUGE(val);
            else
                m.value = VALUE_COUNTER(val);
        }

        label_set_clone(&m.label, conf->labels);

        if (vsh_metric->lkey != NULL)
            metric_label_set(&m, vsh_metric->lkey, vsh_metric->lvalue);
        if ((vsh_metric->tag1 != NULL) && (tokens_num > 2))
            metric_label_set(&m, vsh_metric->tag1, tokens[1]);
        if ((vsh_metric->tag2 != NULL) && (tokens_num > 3))
            metric_label_set(&m, vsh_metric->tag2, tokens[2]);
        if ((vsh_metric->tag3 != NULL) && (tokens_num > 4))
            metric_label_set(&m, vsh_metric->tag3, tokens[3]);

        metric_family_metric_append(fam, m);

        metric_reset(&m, fam->type);
    }

    return 0;
}

static int varnish_read_instance(varnish_instance_t *conf)
{
#if defined(HAVE_VARNISH_V3) || defined(HAVE_VARNISH_V4)
    struct VSM_data *vd;
    bool ok;
    const c_varnish_stats_t *stats;
#elif defined(HAVE_VARNISH_V5) || defined(HAVE_VARNISH_V6)
    struct vsm *vd;
    struct vsc *vsc;
    int vsm_status;
#endif

    vd = VSM_New();
#if defined(HAVE_VARNISH_V5) || defined(HAVE_VARNISH_V6)
    vsc = VSC_New();
#endif
#ifdef HAVE_VARNISH_V3
    VSC_Setup(vd);
#endif

    if (conf->vsh_instance != NULL) {
        int status;

#if defined(HAVE_VARNISH_V3) || defined(HAVE_VARNISH_V4)
        status = VSM_n_Arg(vd, conf->vsh_instance);
#elif defined(HAVE_VARNISH_V5) || defined(HAVE_VARNISH_V6)
        status = VSM_Arg(vd, 'n', conf->vsh_instance);
#endif

        if (unlikely(status < 0)) {
#if defined(HAVE_VARNISH_V3) || defined(HAVE_VARNISH_V4)
            VSM_Delete(vd);
#elif defined(HAVE_VARNISH_V5) || defined(HAVE_VARNISH_V6)
            VSC_Destroy(&vsc, vd);
            VSM_Destroy(&vd);
#endif
            PLUGIN_ERROR("VSM_Arg (\"%s\") failed with status %i.", conf->vsh_instance, status);
            return -1;
        }
    }

#ifdef HAVE_VARNISH_V3
    ok = (VSC_Open(vd, /* diag = */ 1) == 0);
#elif defined(HAVE_VARNISH_V4)
    ok = (VSM_Open(vd) == 0);
#endif
#if defined(HAVE_VARNISH_V3) || defined(HAVE_VARNISH_V4)
    if (unlikely(!ok)) {
        VSM_Delete(vd);
        PLUGIN_ERROR("Unable to open connection.");
        return -1;
    }
#endif

#ifdef HAVE_VARNISH_V3
    stats = VSC_Main(vd);
#elif defined(HAVE_VARNISH_V4)
    stats = VSC_Main(vd, NULL);
#endif
#if defined(HAVE_VARNISH_V3) || defined(HAVE_VARNISH_V4)
    if (unlikely(!stats)) {
        VSM_Delete(vd);
        PLUGIN_ERROR("Unable to get statistics.");
        return -1;
    }
#endif

#if defined(HAVE_VARNISH_V5) || defined(HAVE_VARNISH_V6)
    if (unlikely(VSM_Attach(vd, STDERR_FILENO))) {
        PLUGIN_ERROR("Cannot attach to varnish. %s", VSM_Error(vd));
        VSC_Destroy(&vsc, vd);
        VSM_Destroy(&vd);
        return -1;
    }

    vsm_status = VSM_Status(vd);
    if (unlikely(vsm_status & ~(VSM_MGT_RUNNING | VSM_WRK_RUNNING))) {
        PLUGIN_ERROR("Unable to get statistics.");
        VSC_Destroy(&vsc, vd);
        VSM_Destroy(&vd);
        return -1;
    }
#endif

#ifdef HAVE_VARNISH_V3
    VSC_Iter(vd, varnish_monitor, conf);
#elif defined(HAVE_VARNISH_V4)
    VSC_Iter(vd, NULL, varnish_monitor, conf);
#elif defined(HAVE_VARNISH_V5) || defined(HAVE_VARNISH_V6)
    VSC_Iter(vsc, vd, varnish_monitor, conf);
#endif

#if defined(HAVE_VARNISH_V3) || defined(HAVE_VARNISH_V4)
    VSM_Delete(vd);
#elif defined(HAVE_VARNISH_V5) || defined(HAVE_VARNISH_V6)
    VSC_Destroy(&vsc, vd);
    VSM_Destroy(&vd);
#endif

    return 0;
}

static int varnish_read(user_data_t *ud)
{
    if (unlikely((ud == NULL) || (ud->data == NULL)))
        return EINVAL;

    varnish_instance_t *conf = ud->data;

    int status = varnish_read_instance(conf);

    metric_family_append(&conf->fams[FAM_VARNISH_UP],
                         VALUE_GAUGE(status == 0 ? 1 : 0), &conf->labels, NULL);

    plugin_dispatch_metric_family_array_filtered(conf->fams, FAM_VARNISH_MAX, conf->filter, 0);
    return 0;
}

static void varnish_config_free(void *ptr)
{
    varnish_instance_t *conf = ptr;

    if (conf == NULL)
        return;

    if (conf->instance != NULL)
        free(conf->instance);
    if (conf->vsh_instance != NULL)
        free(conf->vsh_instance);

    label_set_reset(&conf->labels);
    plugin_filter_free(conf->filter);

    free(conf);
}

static int varnish_config_instance(const config_item_t *ci)
{
    varnish_instance_t *conf = calloc(1, sizeof(*conf));
    if (conf == NULL) {
        PLUGIN_ERROR("calloc failed: %s.", STRERRNO);
        return -1;
    }
    conf->instance = NULL;

    memcpy(conf->fams, fams, FAM_VARNISH_MAX * sizeof(conf->fams[0]));

    int status = cf_util_get_string(ci, &conf->instance);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        free(conf);
        return -1;
    }

    conf->flags = COLLECT_BACKEND | COLLECT_CACHE | COLLECT_CONNECTIONS | COLLECT_SHM;

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("vsh-instance", child->key) == 0) {
            status = cf_util_get_string(child, &conf->vsh_instance);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &conf->labels);
        } else if (strcasecmp("collect", child->key) == 0) {
            status = cf_util_get_flags(child, cvarnish_flags, cvarnish_flags_size, &conf->flags);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &conf->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        varnish_config_free(conf);
        return status;
    }

    if (conf->vsh_instance != NULL) {
        if (strcmp("localhost", conf->vsh_instance) == 0) {
            free(conf->vsh_instance);
            conf->vsh_instance = NULL;
        }
    }

    label_set_add(&conf->labels, true, "instance", conf->instance);

    return plugin_register_complex_read("varnish", conf->vsh_instance, varnish_read, interval,
                                        &(user_data_t){.data=conf, .free_func=varnish_config_free});
}

static int varnish_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = varnish_config_instance(child);
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
    plugin_register_config("varnish", varnish_config);
}
