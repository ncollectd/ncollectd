// SPDX-License-Identifier: GPL-2.0
// SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz
// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>
// SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/mount.h"
#include "libutils/avltree.h"

#include <sys/resource.h>
#include <sys/utsname.h>

#include "internal.h"
#include "ply.h"

typedef enum {
    KERNEL_CONFIG_NONE           = 0,
    KERNEL_CONFIG_BPF_SYSCALL    = (1 << 0),
    KERNEL_CONFIG_KPROBES        = (1 << 1),
    KERNEL_CONFIG_UPROBES        = (1 << 2),
    KERNEL_CONFIG_TRACEPOINTS    = (1 << 3),
    KERNEL_CONFIG_FTRACE         = (1 << 4),
    KERNEL_CONFIG_DYNAMIC_FTRACE = (1 << 5),
    KERNEL_CONFIG_PERF_EVENTS    = (1 << 6)
} kernel_config_t;

typedef struct {
    char *key;
    int from;
} ebpf_label_from_pair_t;

typedef struct {
    size_t size;
    ebpf_label_from_pair_t *ptr;
} ebpf_label_from_t;

typedef struct {
    char *map;
    char *metric;
    char *help;
    metric_type_t type;
    label_set_t labels;
    ebpf_label_from_t labels_from;
} ebpf_metric_t;

typedef struct ctx {
    char *instance;
    char *script;
    char *filename;
    label_set_t labels;
    c_avl_tree_t *metrics;
    struct ply *ply;
    nfds_t nfds;
    struct pollfd *fds;
    plugin_filter_t *filter;
} ebpf_ctx_t;

static char *path_proc;
static char *path_sys;

static void ebpf_label_from_reset(ebpf_label_from_t *label_from)
{
    for (size_t i = 0; i < label_from->size; i++) {
        free(label_from->ptr[i].key);
    }

    free(label_from->ptr);

    label_from->size = 0;
    label_from->ptr = NULL;
}

static void ebpf_metric_free(ebpf_metric_t *ebpf_metric)
{
    if (ebpf_metric == NULL)
        return;

    free(ebpf_metric->map);
    free(ebpf_metric->metric);
    free(ebpf_metric->help);

    label_set_reset(&ebpf_metric->labels);
    ebpf_label_from_reset(&ebpf_metric->labels_from);

    free(ebpf_metric);
}

static void ebpf_free(void *data)
{
    if (data == NULL)
        return;

    ebpf_ctx_t *ctx = (ebpf_ctx_t *)data;

    free(ctx->instance);
    free(ctx->script);
    free(ctx->filename);
    label_set_reset(&ctx->labels);

    if (ctx->ply) {
        ply_stop(ctx->ply);
#if 0
        /* END probes may generate events, so do a final poll for them
         * before shutting down.
         */
        int ready = poll(&fds[1], nfds, 0);
        if (ready > 0)
            ply_return_fold(&ret, ply_service(ctx->ply, ready, &fds[1]));

        free(fds);
#endif

        ply_unload(ctx->ply);
        ply_free(ctx->ply);
    }

    free(ctx->fds);

    while (true) {
        ebpf_metric_t *ebpf_metric = NULL;
        char *name = NULL;
        int status = c_avl_pick(ctx->metrics, (void *)&name, (void *)&ebpf_metric);
        if (status != 0)
            break;

        ebpf_metric_free(ebpf_metric);
    }
    c_avl_destroy(ctx->metrics);

    plugin_filter_free(ctx->filter);

    free(ctx);
}

static void memlock_uncap(void)
{
    struct rlimit limit;
    int err = getrlimit(RLIMIT_MEMLOCK, &limit);
    if (err) {
        PLUGIN_ERROR("Unable to retrieve memlock limit, maps are likely limited in size.");
        return;
    }

    rlim_t current = limit.rlim_cur;

    /* The total size of all maps that ply is allowed to create is
     * limited by the amount of memory that can be locked into
     * RAM. By default, this limit can be quite low (64kB on a
     * standard x86_64 box running a recent kernel). So this
     * simply tells the kernel to allow ply to use as much as it
     * needs. */
    limit.rlim_cur = limit.rlim_max = RLIM_INFINITY;
    err = setrlimit(RLIMIT_MEMLOCK, &limit);
    if (err) {
        const char *suffix = "B";

        if (!(current & 0xfffff)) {
            suffix = "MB";
            current >>= 20;
        } else if (!(current & 0x3ff)) {
            suffix = "kB";
            current >>= 10;
        }

        PLUGIN_WARNING("Could not remove memlock size restriction. "
                       "Total map size is limited to %lu%s.", current, suffix);
        return;
    }

    PLUGIN_DEBUG("Unlimited memlock.");
}

static bool check_mount_debugfs(void)
{
    char path[PATH_MAX];

    ssnprintf(path, sizeof(path), "%s/mounts", path_proc);

    FILE *file_mounts = setmntent("/proc/mounts", "r");
    if (file_mounts == NULL)
        return false;

    ssnprintf(path, sizeof(path), "%s/kernel/debug", path_sys);

    struct mntent *mount_entry = getmntent(file_mounts);
    while (mount_entry != NULL) {
        if (strncmp(mount_entry->mnt_type, "debugfs", 7) == 0) {
            if (strcmp(mount_entry->mnt_dir, path) == 0) {
                endmntent(file_mounts);
                return true;
            }
        }
        mount_entry = getmntent(file_mounts);
    }

    endmntent(file_mounts);

    return false;
}

static int check_kernel_config(void)
{
    struct utsname uts = {0};
    int status = uname(&uts);
    if (status != 0)
        return -1;

//     "/proc/config.gz"

    char path[PATH_MAX];
    ssnprintf(path, sizeof(path), "/boot/config-%s", uts.release);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        // FIXME
        return 0;
    }

    kernel_config_t found = KERNEL_CONFIG_NONE;

    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        strnrtrim(line, strlen(line));

    if (strcmp(line, "CONFIG_BPF_SYSCALL=y") == 0) {
        found |= KERNEL_CONFIG_BPF_SYSCALL;
        } else if (strcmp(line, "CONFIG_KPROBES=y") == 0) {
            found |= KERNEL_CONFIG_KPROBES;
        } else if (strcmp(line, "CONFIG_UPROBES=y") == 0) {
            found |= KERNEL_CONFIG_UPROBES;
        } else if (strcmp(line, "CONFIG_TRACEPOINTS=y") == 0) {
            found |= KERNEL_CONFIG_TRACEPOINTS;
        } else if (strcmp(line, "CONFIG_FTRACE=y") == 0) {
            found |= KERNEL_CONFIG_FTRACE;
        } else if (strcmp(line, "CONFIG_DYNAMIC_FTRACE=y") == 0) {
            found |= KERNEL_CONFIG_DYNAMIC_FTRACE;
        } else if (strcmp(line, "CONFIG_PERF_EVENTS=y") == 0) {
            found |= KERNEL_CONFIG_PERF_EVENTS;
        }
    }

    fclose(fp);

    if ((found & KERNEL_CONFIG_BPF_SYSCALL) &&
        ((found & KERNEL_CONFIG_KPROBES) || (found & KERNEL_CONFIG_TRACEPOINTS)) &&
        (found & KERNEL_CONFIG_DYNAMIC_FTRACE)) {
        PLUGIN_INFO("kernel config (%s): OK.", path);
    } else {
        PLUGIN_ERROR("kernel config (%s): MISSING.", path);
    }

    if (!(found & KERNEL_CONFIG_BPF_SYSCALL))
       PLUGIN_WARNING("Kernel config CONFIG_BPF_SYSCALL is not set.");
    if (!(found & KERNEL_CONFIG_KPROBES))
       PLUGIN_WARNING("Kernel config CONFIG_KPROBES is not set.");
    if (!(found & KERNEL_CONFIG_UPROBES))
       PLUGIN_WARNING("Kernel config CONFIG_UPROBES is not set.");
    if (!(found & KERNEL_CONFIG_TRACEPOINTS))
       PLUGIN_WARNING("Kernel config CONFIG_TRACEPOINTS is not set.");
    if (!(found & KERNEL_CONFIG_FTRACE))
       PLUGIN_WARNING("Kernel config CONFIG_FTRACE is not set.");
    if (!(found & KERNEL_CONFIG_DYNAMIC_FTRACE))
       PLUGIN_WARNING("Kernel config CONFIG_DYNAMIC_FTRACE is not set.");
    if (!(found & KERNEL_CONFIG_PERF_EVENTS))
       PLUGIN_WARNING("Kernel config CONFIG_PERF_EVENTS is not set.");

    return 0;
}

#if 0
"kprobe:schedule { exit(0); }"
echo -n "Verifying kprobe... "

"tracepoint:sched/sched_switch { exit(0); }"
echo -n "Verifying tracepoint... "

"BEGIN { exit(0); }"
echo -n "Verifying special... "

"interval:1s { exit(0); }"
echo -n "Verifying interval... "
#endif

static histogram_t *ebpf_histogram(struct type *t, const void *data)
{
    const unsigned int *bucket = data;
    struct type *arg_type = t->priv;
    int len = type_base(t)->array.len;

    histogram_t *h = histogram_new_linear(len, 1);
    if (h == NULL)
        return NULL;

    int alen = len;
    /* signed argument => last bucket holds count of negative values */
    if (!type_base(arg_type)->scalar.unsignd) {
        alen--;
        h->buckets[len].maximum = 0;
        h->buckets[len].counter = bucket[alen];
    }

    for (int i = 0; i < alen; i++) {
        int n = alen - i;

        double maximum = 0;

        if (arg_type->fprint_log2) {
            if (type_base(arg_type)->scalar.unsignd) {
                maximum = (double)((1ULL << (i + 1)) - 1) * 1e-9;
            } else {
                maximum = (double)((1LL  << (i + 1)) - 1) * 1e-9;
            }
        } else {
            maximum = i + 1;
        }
        
        h->buckets[n].maximum = maximum;
        h->buckets[n].counter = bucket[i];
    }

    for (int i = len - 1; i >= 0; i--) {
        h->buckets[i].counter += h->buckets[i+1].counter;
    }

    return h;
}

static uint64_t ebpf_scalar_unsigned(struct type *t, const void *data)
{
    switch ((t->scalar.size << 1) | t->scalar.unsignd) {
    case (1 << 1) | 1:
        return *((const uint8_t *)data);
    case (1 << 1) | 0:
        return *((const int8_t *)data);
    case (2 << 1) | 1:
        return *((const uint16_t *)data);
    case (2 << 1) | 0:
        return *((const int16_t *)data);
    case (4 << 1) | 1:
        return *((const uint32_t *)data);
    case (4 << 1) | 0:
        return *((const int32_t *)data);
    case (8 << 1) | 1:
        return *((const uint64_t *)data);
    case (8 << 1) | 0:
        return *((const int64_t *)data);
    }

    assert(0);
    return 0;
}

static int64_t ebpf_scalar_signed(struct type *t, const void *data)
{
    switch ((t->scalar.size << 1) | t->scalar.unsignd) {
    case (1 << 1) | 1:
        return *((const uint8_t *)data);
    case (1 << 1) | 0:
        return *((const int8_t *)data);
    case (2 << 1) | 1:
        return *((const uint16_t *)data);
    case (2 << 1) | 0:
        return *((const int16_t *)data);
    case (4 << 1) | 1:
        return *((const uint32_t *)data);
    case (4 << 1) | 0:
        return *((const int32_t *)data);
    case (8 << 1) | 1:
        return *((const uint64_t *)data);
    case (8 << 1) | 0:
        return *((const int64_t *)data);
    }

    assert(0);
    return 0;
}

static int ebpf_dispatch_metric_family(ebpf_ctx_t *ctx, ebpf_metric_t *ebpf_metric, struct sym *sym)
{
    struct type *t = sym->type;

    size_t key_size = type_sizeof(t->map.ktype);
    size_t val_size = type_sizeof(t->map.vtype);
    size_t row_size = key_size + val_size;

    char *data = calloc(1, row_size);
    if (data == NULL)
        return -1;

    char *prev_key = NULL;
    char *key = data;
    char *val = data + key_size;

    metric_family_t fam = {0};
    fam.name = ebpf_metric->metric;
    fam.help = ebpf_metric->help;
    fam.type = ebpf_metric->type;

    while (bpf_map_next(sym->mapfd, prev_key, key) == 0) {
        int err = bpf_map_lookup(sym->mapfd, key, val);
        if (err)
            break;

        struct type *kt = t->map.ktype;
        if (kt->ttype != T_STRUCT)
            break;

        if (strncmp(kt->sou.name, ":anon_", strlen(":anon_")) != 0)
            break;

        struct type *vt = t->map.vtype;
        label_set_t labels = {0};
            
        label_set_add_set(&labels, true, ctx->labels);
        label_set_add_set(&labels, true, ebpf_metric->labels);

        int n = 0;

        tfields_foreach(f, kt->sou.fields) {
            size_t offs = type_offsetof(kt, f->name);

            if (!type_is_string(f->type))
                break;
            
            const char *label = ((const char *)key) + offs;

            if (!isstring(label, f->type->array.len))
                break;

            for (size_t i = 0; i < ebpf_metric->labels_from.size ; i++) {
                ebpf_label_from_pair_t *pair = &ebpf_metric->labels_from.ptr[i];
                if (pair->from == n) {
                    _label_set_add(&labels, true, false, pair->key, strlen(pair->key),
                                            label, f->type->array.len-1);
                    break;
                }
            }
            n++;
        }
 

        if (vt->ttype == T_SCALAR) {
            if (fam.type == METRIC_TYPE_GAUGE) {
                metric_family_append(&fam, VALUE_GAUGE_INT64(ebpf_scalar_signed(vt, val)),
                                     &labels, NULL);
            } else if (fam.type == METRIC_TYPE_COUNTER) {
                metric_family_append(&fam, VALUE_COUNTER_UINT64(ebpf_scalar_unsigned(vt, val)),
                                     &labels, NULL);
            }
        } else if (vt->ttype == T_TYPEDEF) {
            if ((vt->tdef.name != NULL) && 
                (strncmp(vt->tdef.name, "quantize_", strlen("quantize_")) == 0))  {
                if ((fam.type == METRIC_TYPE_HISTOGRAM) || 
                    (fam.type == METRIC_TYPE_GAUGE_HISTOGRAM)) {
                    histogram_t *h = ebpf_histogram(vt, val);   
                    if (h != NULL) {
                        metric_family_append(&fam, VALUE_HISTOGRAM(h), &labels, NULL);
                        histogram_destroy(h);
                    }
                }
            }

        }

        label_set_reset(&labels);

        prev_key = key;
    }

    free(data);

    plugin_dispatch_metric_family_filtered(&fam, ctx->filter, 0);

    return 0;
}

static int ebpf_read(user_data_t *user_data)
{
    ebpf_ctx_t *ctx = user_data->data;
    if (ctx == NULL)
        return  -1;
    
    struct ply_return ret = { .err = 1 };

    int ready = poll(ctx->fds, ctx->nfds, 0);
    ply_return_fold(&ret, ply_service(ctx->ply, ready, ctx->fds));
    
//   ebpf_maps_print(ctx->ply);

    struct sym **symp, *sym;
    symtab_foreach(&ctx->ply->globals, symp) {
        sym = *symp;

        if ((sym->type->ttype == T_MAP) &&
            (sym->type->map.mtype != BPF_MAP_TYPE_PERF_EVENT_ARRAY) &&
            (sym->type->map.mtype != BPF_MAP_TYPE_STACK_TRACE)) {

            ebpf_metric_t *found = NULL;
            int status = c_avl_get(ctx->metrics, sym->name, (void *)&found);
            if (status == 0) {
                ebpf_dispatch_metric_family(ctx, found, sym);
            }
        }
    }    

    return 0;
}

static int ebpf_ply_init(ebpf_ctx_t *ctx)
{

    ply_init();

    ctx->ply = ply_alloc(ctx->instance, true);

    FILE *fh = NULL;

    if (ctx->filename != NULL) {
        fh = fopen(ctx->filename, "r");     
        if (fh == NULL) {
            PLUGIN_ERROR("Cannot open '%s' failed: %s", ctx->filename, STRERRNO);
            return -1;
        }
        
    } else {
        fh = fmemopen(ctx->script, strlen(ctx->script), "r");     
        if (fh == NULL) {
            PLUGIN_ERROR("Cannot open memory stream: %s", STRERRNO);
            return -1;
        }
    }

    int status = ply_fparse(ctx->ply, fh);
    if (status != 0) {
        PLUGIN_ERROR("Failed to parse ply script.");
        fclose(fh);
        return -1;
    }

    fclose(fh);

    status = ply_compile(ctx->ply);
    if (status != 0) {
        PLUGIN_ERROR("Failed to compile ply script.");
        return -1;
    }

    struct ply_return ret = {0};
    ret = ply_load(ctx->ply);
    if (ret.exit || ret.err) {
        PLUGIN_ERROR("Failed to load ply script.");
        return -1;
    }

    ctx->nfds = ply_get_nfds(ctx->ply);
    if (ctx->nfds) {
        ctx->fds = malloc(ctx->nfds * sizeof(*(ctx->fds)));
        if (ctx->fds == NULL) {
            PLUGIN_ERROR("malloc failed.");
            return -1;
        }

        ply_fill_pollset(ctx->ply, ctx->fds);
    }

    ply_start(ctx->ply);

    return 0;
}

static int ebpf_config_append_label_from(config_item_t *ci, ebpf_label_from_t *label_from)
{
    if ((ci->values_num != 2) ||
        ((ci->values_num == 2) && ((ci->values[0].type != CONFIG_TYPE_STRING) ||
                                   (ci->values[1].type != CONFIG_TYPE_NUMBER)))) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly a string and a number.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    ebpf_label_from_pair_t *tmp = realloc(label_from->ptr, sizeof(*tmp) * (label_from->size + 1));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        return -1;
    }

    label_from->ptr = tmp;

    char *str = sstrdup(ci->values[0].value.string);
    if (str == NULL) {
        PLUGIN_ERROR("strdup failed.");
        return -1;
    }
    
    label_from->ptr[label_from->size].key = str;
    label_from->ptr[label_from->size].from = (int)ci->values[1].value.number;
    label_from->size++;

    return 0;
}

static int ebpf_metric_type(const config_item_t *ci, metric_type_t *ret_metric)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (!strcasecmp(ci->values[0].value.string, "gauge")) {
        *ret_metric = METRIC_TYPE_GAUGE;
    } else if (!strcasecmp(ci->values[0].value.string, "counter")) {
        *ret_metric = METRIC_TYPE_COUNTER;
    } else if (!strcasecmp(ci->values[0].value.string, "histogram")) {
        *ret_metric = METRIC_TYPE_HISTOGRAM;
    } else if (!strcasecmp(ci->values[0].value.string, "gaugehistogram")) {
        *ret_metric = METRIC_TYPE_GAUGE_HISTOGRAM;
    } else {
        PLUGIN_ERROR("The '%s' option in %s:%d must be: 'gauge', 'counter', "
                     "'histogram' or 'gaugehistogram'.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    return 0;
}

static int ebpf_config_metric(config_item_t *ci, ebpf_ctx_t *ctx)
{
    ebpf_metric_t *ebpf_metric = calloc(1, sizeof(*ebpf_metric));
    if (ebpf_metric == NULL)
        return -1;

    int status = cf_util_get_string(ci, &ebpf_metric->map);
    if (status != 0) {
        PLUGIN_ERROR("Missing metric map name.");
        free(ebpf_metric);
        return -1;
    }

    ebpf_metric->type = METRIC_TYPE_COUNTER;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("name", child->key) == 0) {
            status = cf_util_get_string(child, &ebpf_metric->metric);
        } else if (strcasecmp("help", child->key) == 0) {
            status = cf_util_get_string(child, &ebpf_metric->help);
        } else if (strcasecmp("type", child->key) == 0) {
            status = ebpf_metric_type(child, &ebpf_metric->type);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ebpf_metric->labels);
        } else if (strcasecmp("label-from", child->key) == 0) {
            status = ebpf_config_append_label_from(child, &ebpf_metric->labels_from);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        ebpf_metric_free(ebpf_metric);
        return -1;
    }

    if (ebpf_metric->metric == NULL) {
        PLUGIN_ERROR("Missing metric name in metric definition.");
        ebpf_metric_free(ebpf_metric);
        return -1;
    }

    ebpf_metric_t *found = NULL;
    status = c_avl_get(ctx->metrics, ebpf_metric->map, (void *)&found);
    if (status == 0) {
        PLUGIN_ERROR("Duplicate metric name: '%s'.", ebpf_metric->map);
        ebpf_metric_free(ebpf_metric);
        return -1;
    }

    status = c_avl_insert(ctx->metrics, ebpf_metric->map, ebpf_metric);
    if (status != 0) {
        ebpf_metric_free(ebpf_metric);
        return -1;
    }

    return 0;
}

static int ebpf_config_instance(config_item_t *ci)
{
    ebpf_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &ctx->instance);
    if (status != 0) {
        PLUGIN_ERROR("Missing instance name.");
        ebpf_free(ctx);
        return -1;
    }

    ctx->metrics = c_avl_create((int (*)(const void *, const void *))strcmp);
    if (ctx->metrics == NULL) {
        ebpf_free(ctx);
        return -1;
    }

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp("script", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->script);
        } else if (strcasecmp("file", child->key) == 0) {
            status = cf_util_get_string(child, &ctx->filename);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &ctx->labels);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("metric", child->key) == 0) {
            status = ebpf_config_metric(child, ctx);
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = plugin_filter_configure(child, &ctx->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        ebpf_free(ctx);
        return -1;
    }

    if ((ctx->script == NULL) && (ctx->filename == NULL)) {
        PLUGIN_ERROR("None of 'script' or 'file' are set.");
        ebpf_free(ctx);
        return -1;
    }

    if ((ctx->script != NULL) && (ctx->filename != NULL)) {
        PLUGIN_ERROR("Only one of 'script' or 'file' can be set.");
        ebpf_free(ctx);
        return -1;
    }

    if (c_avl_size(ctx->metrics) == 0)
        PLUGIN_WARNING("No metrics configured.");

    status = ebpf_ply_init(ctx);
    if (status != 0) {
        ebpf_free(ctx);
        return -1;
    }

    label_set_add(&ctx->labels, true, "instance", ctx->instance);

    return plugin_register_complex_read("ebf", ctx->instance, ebpf_read, interval,
                                        &(user_data_t){.data = ctx, .free_func = ebpf_free});
}

static int ebpf_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = ebpf_config_instance(child);
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

static int ebpf_init(void)
{
//    ply_init();

    path_sys = plugin_syspath(NULL);
    if (path_sys == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    path_proc = plugin_procpath(NULL);
    if (path_proc == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    memlock_uncap();

#if defined(HAVE_SYS_CAPABILITY_H) && defined(CAP_SYS_ADMIN)
    if (plugin_check_capability(CAP_SYS_ADMIN) != 0) {
        if (getuid() == 0)
            PLUGIN_WARNING("Running ncollectd as root, but the "
                           "CAP_SYS_ADMIN capability is missing. The plugin's read "
                           "function will probably fail. Is your init system dropping "
                           "capabilities?");
        else
            PLUGIN_WARNING("ncollectd doesn't have the CAP_SYS_ADMIN "
                           "capability. If you don't want to run collectd as root, try "
                           "running \"setcap cap_sys_admin=ep\" on the ncollectd binary.");
    }
#endif

    check_mount_debugfs();

    check_kernel_config();

    return 0;
}

static int ebpf_shutdown(void)
{
    free(path_sys);
    free(path_proc);

    return 0;
}

void module_register(void)
{
    plugin_register_init("ebpf", ebpf_init);
    plugin_register_shutdown("ebpf", ebpf_shutdown);
    plugin_register_config("ebpf", ebpf_config);
}
