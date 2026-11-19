// SPDX-License-Identifier: GPL-2.0
// SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz
// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>
// SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com>

/* _GNU_SOURCE is needed for qsort_r */
#define _GNU_SOURCE

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/avltree.h"
#include "libutils/strbuf.h"

#include <sys/resource.h>
#include <sys/utsname.h>

#include <linux/capability.h>

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
    cdtime_t interval;
    unsigned int buf_pages;
    struct ctx *next;
} ebpf_ctx_t;

/* Instances compiled in the configuration, loaded in the init callback. */
static ebpf_ctx_t *ebpf_pending;

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
                       "Total map size is limited to %" PRIu64 "%s.", (uint64_t)current, suffix);
        return;
    }

    PLUGIN_DEBUG("Unlimited memlock.");
}

static void check_capabilities(void)
{
    /* Since Linux 5.8 CAP_BPF and CAP_PERFMON are enough to load the
     * programs, before CAP_SYS_ADMIN is required. */
#if defined(CAP_BPF) && defined(CAP_PERFMON)
    bool bpf_caps = (plugin_check_capability(CAP_BPF) == 0) &&
                    (plugin_check_capability(CAP_PERFMON) == 0);
#else
    bool bpf_caps = false;
#endif

    if (!bpf_caps && (plugin_check_capability(CAP_SYS_ADMIN) != 0)) {
        if (getuid() == 0)
            PLUGIN_WARNING("Running ncollectd as root, but the CAP_BPF and CAP_PERFMON "
                           "(Linux >= 5.8) or the CAP_SYS_ADMIN capabilities are missing. "
                           "Loading the scripts will probably fail. Is your init system "
                           "dropping capabilities?");
        else
            PLUGIN_WARNING("ncollectd doesn't have the CAP_BPF and CAP_PERFMON (Linux >= 5.8) "
                           "or the CAP_SYS_ADMIN capabilities. If you don't want to run "
                           "ncollectd as root, try running \"setcap "
                           "cap_bpf,cap_perfmon,cap_syslog,cap_dac_override=ep\" on the "
                           "ncollectd binary.");
    }

    if (plugin_check_capability(CAP_SYSLOG) != 0)
        PLUGIN_WARNING("ncollectd doesn't have the CAP_SYSLOG capability needed to read the "
                       "addresses of /proc/kallsyms (see the sysctl kernel.kptr_restrict): "
                       "kprobe wildcards, 'caller' and 'stack' can not be resolved.");
}

static void check_tracefs(void)
{
    char path[PATH_MAX];

    ssnprintf(path, sizeof(path), "%sevents", tracepath());

    if (access(path, F_OK) != 0)
        PLUGIN_WARNING("Cannot access '%s': %s. tracefs must be mounted on "
                       "/sys/kernel/tracing (or debugfs on /sys/kernel/debug) "
                       "for kprobe, kretprobe and tracepoint probes.", path, STRERRNO);
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
    /* time values are in nanoseconds, report them in seconds */
    double scale = arg_type->fprint_log2 ? 1e-9 : 1.0;

    /* the last bucket of the values is the +Inf bucket */
    histogram_t *h = histogram_new_linear(len - 1, 1);
    if (h == NULL)
        return NULL;

    int alen = len;
    /* signed argument => last bucket holds count of negative values */
    if (!type_base(arg_type)->scalar.unsignd) {
        alen--;
        h->buckets[len - 1].maximum = 0;
        h->buckets[len - 1].counter = bucket[alen];
    }

    /* bucket i holds the values in [2^i, 2^(i+1) - 1], bucket 0 also 0 */
    double bound = 1.0;
    for (int i = 0; i < alen; i++) {
        int n = alen - 1 - i;

        bound *= 2.0;
        if (n > 0)
            h->buckets[n].maximum = (bound - 1.0) * scale;
        h->buckets[n].counter = bucket[i];
    }

    for (int i = len - 2; i >= 0; i--) {
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

static bool ebpf_type_is_quantize(struct type *t)
{
    return (t->ttype == T_TYPEDEF) && (t->tdef.name != NULL) &&
           (strncmp(t->tdef.name, "quantize_", strlen("quantize_")) == 0);
}

static struct sym *ebpf_find_map(struct ply *ply, const char *name)
{
    struct sym **symp;

    symtab_foreach(&ply->globals, symp) {
        struct sym *sym = *symp;

        if ((sym->name != NULL) && (strcmp(sym->name, name) == 0) &&
            (sym->type != NULL) && (sym->type->ttype == T_MAP))
            return sym;
    }

    return NULL;
}

static void ebpf_label_addr(ebpf_ctx_t *ctx, uintptr_t addr, strbuf_t *buf)
{
    const struct ksym *sym = ksym_get(ctx->ply->ksyms, addr);

    if (sym == NULL)
        strbuf_printf(buf, "0x%" PRIxPTR, addr);
    else if (sym->addr == addr)
        strbuf_print(buf, sym->sym);
    else
        strbuf_printf(buf, "%s+%" PRIuPTR, sym->sym, addr - sym->addr);
}

static void ebpf_label_stack(ebpf_ctx_t *ctx, uint32_t stackid, strbuf_t *buf)
{
    struct sym *map = ebpf_find_map(ctx->ply, ":stackmap");
    size_t depth = ctx->ply->stack_depth;

    uint64_t *bt = calloc(depth, sizeof(*bt));
    if ((map == NULL) || (bt == NULL) || (bpf_map_lookup(map->mapfd, &stackid, bt) != 0)) {
        free(bt);
        strbuf_printf(buf, "%" PRIu32, stackid);
        return;
    }

    for (size_t i = 0; (i < depth) && (bt[i] != 0); i++) {
        if (i > 0)
            strbuf_putchar(buf, ';');
        ebpf_label_addr(ctx, (uintptr_t)bt[i], buf);
    }

    free(bt);
}

/* Convert a field of a map key to a label value. */
static int ebpf_key_label(ebpf_ctx_t *ctx, struct type *t, const char *data, strbuf_t *buf)
{
    if ((t->ttype == T_TYPEDEF) && (t->tdef.name != NULL)) {
        if (strcmp(t->tdef.name, "caller_t") == 0) {
            ebpf_label_addr(ctx, *((const uintptr_t *)data), buf);
            return 0;
        } else if (strcmp(t->tdef.name, ":stackid") == 0) {
            ebpf_label_stack(ctx, *((const uint32_t *)data), buf);
            return 0;
        }
    }

    struct type *bt = type_base(t);

    switch (bt->ttype) {
    case T_ARRAY:
        if (!type_is_string(bt) || !isstring(data, bt->array.len))
            return -1;
        return strbuf_putstrn(buf, data, strnlen(data, bt->array.len));
    case T_SCALAR:
        if (bt->scalar.unsignd)
            return strbuf_printf(buf, "%" PRIu64, ebpf_scalar_unsigned(bt, data));
        return strbuf_printf(buf, "%" PRId64, ebpf_scalar_signed(bt, data));
    case T_POINTER:
        if (bt->ptr.bpf)
            return strbuf_printf(buf, "0x%" PRIx64, *((const uint64_t *)data));
        return strbuf_printf(buf, "0x%" PRIxPTR, *((const uintptr_t *)data));
    default:
        break;
    }

    return -1;
}

static int ebpf_key_cmp(const void *a, const void *b, void *size)
{
    return memcmp(a, b, *((size_t *)size));
}

static int ebpf_dispatch_metric_family(ebpf_ctx_t *ctx, ebpf_metric_t *ebpf_metric, struct sym *sym)
{
    struct type *t = sym->type;

    size_t key_size = type_sizeof(t->map.ktype);
    size_t val_size = type_sizeof(t->map.vtype);
    size_t max_elems = t->map.len ? t->map.len : ctx->ply->map_elems;

    char *keys = calloc(max_elems, key_size);
    char *val = calloc(1, val_size);
    if ((keys == NULL) || (val == NULL)) {
        free(keys);
        free(val);
        return -1;
    }

    /* The BPF programs update the map while it is walked, and the walk
     * restarts from the first key when the previous one was deleted:
     * collect at most max_elems keys and skip the duplicates. */
    size_t n_keys = 0;
    char *prev_key = NULL;
    while (n_keys < max_elems) {
        char *key = keys + n_keys * key_size;
        if (bpf_map_next(sym->mapfd, prev_key, key) != 0)
            break;

        prev_key = key;
        n_keys++;
    }

    qsort_r(keys, n_keys, key_size, ebpf_key_cmp, &key_size);

    strbuf_t buf = STRBUF_CREATE;

    metric_family_t fam = {0};
    fam.name = ebpf_metric->metric;
    fam.help = ebpf_metric->help;
    fam.type = ebpf_metric->type;

    for (size_t k = 0; k < n_keys; k++) {
        char *key = keys + k * key_size;
        if ((k > 0) && (memcmp(key, key - key_size, key_size) == 0))
            continue;

        int err = bpf_map_lookup(sym->mapfd, key, val);
        if (err)
            continue;

        struct type *kt = t->map.ktype;
        if (kt->ttype != T_STRUCT)
            break;

        if (strncmp(kt->sou.name, ":anon_", strlen(":anon_")) != 0)
            break;

        struct type *vt = t->map.vtype;
        label_set_t labels = {0};
            
        label_set_add_set(&labels, true, ctx->labels);
        label_set_add_set(&labels, true, ebpf_metric->labels);

        int n = -1;

        tfields_foreach(f, kt->sou.fields) {
            n++;

            ebpf_label_from_pair_t *pair = NULL;
            for (size_t i = 0; i < ebpf_metric->labels_from.size ; i++) {
                if (ebpf_metric->labels_from.ptr[i].from == n) {
                    pair = &ebpf_metric->labels_from.ptr[i];
                    break;
                }
            }

            if (pair == NULL)
                continue;

            strbuf_reset(&buf);
            if (ebpf_key_label(ctx, f->type, key + type_offsetof(kt, f->name), &buf) == 0)
                label_set_add(&labels, true, pair->key, buf.ptr);
        }

        if (ebpf_type_is_quantize(vt)) {
            if ((fam.type == METRIC_TYPE_HISTOGRAM) ||
                (fam.type == METRIC_TYPE_GAUGE_HISTOGRAM)) {
                histogram_t *h = ebpf_histogram(vt, val);
                if (h != NULL) {
                    metric_family_append(&fam, VALUE_HISTOGRAM(h), &labels, NULL);
                    histogram_destroy(h);
                }
            }
        } else if (type_base(vt)->ttype == T_SCALAR) {
            struct type *st = type_base(vt);
            if (fam.type == METRIC_TYPE_GAUGE) {
                metric_family_append(&fam, VALUE_GAUGE_INT64(ebpf_scalar_signed(st, val)),
                                     &labels, NULL);
            } else if (fam.type == METRIC_TYPE_COUNTER) {
                metric_family_append(&fam, VALUE_COUNTER_UINT64(ebpf_scalar_unsigned(st, val)),
                                     &labels, NULL);
            }
        }

        label_set_reset(&labels);
    }

    strbuf_destroy(&buf);
    free(keys);
    free(val);

    plugin_dispatch_metric_family_filtered(&fam, ctx->filter, 0);

    return 0;
}

static int ebpf_read(user_data_t *user_data)
{
    ebpf_ctx_t *ctx = user_data->data;
    if (ctx == NULL)
        return  -1;
    
    if (ctx->nfds > 0) {
        int ready = poll(ctx->fds, ctx->nfds, 0);
        if (ready < 0) {
            if (errno != EINTR)
                PLUGIN_ERROR("Instance '%s': poll failed: %s.", ctx->instance, STRERRNO);
        } else if (ready > 0) {
            struct ply_return ret = ply_service(ctx->ply, ready, ctx->fds);
            if (ret.err)
                PLUGIN_ERROR("Instance '%s': failed to process the events of the script: %s.",
                             ctx->instance, STRERROR(ret.val));
            else if (ret.exit)
                PLUGIN_INFO("Instance '%s': the script called exit(%d).",
                            ctx->instance, ret.val);
        }
    }

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

static int ebpf_check_metrics(ebpf_ctx_t *ctx)
{
    c_avl_iterator_t *iter = c_avl_get_iterator(ctx->metrics);
    if (iter == NULL) {
        PLUGIN_ERROR("Cannot create avl iterator.");
        return -1;
    }

    int status = 0;
    char *name = NULL;
    ebpf_metric_t *ebpf_metric = NULL;
    while (c_avl_iterator_next(iter, (void *)&name, (void *)&ebpf_metric) == 0) {
        struct sym *sym = ebpf_find_map(ctx->ply, ebpf_metric->map);
        if ((sym == NULL) || (sym->type->map.mtype == BPF_MAP_TYPE_PERF_EVENT_ARRAY) ||
                            (sym->type->map.mtype == BPF_MAP_TYPE_STACK_TRACE)) {
            PLUGIN_ERROR("Instance '%s': map '%s' of metric '%s' not found in the script.",
                         ctx->instance, ebpf_metric->map, ebpf_metric->metric);
            status = -1;
            break;
        }

        struct type *kt = sym->type->map.ktype;
        if ((kt->ttype != T_STRUCT) ||
            (strncmp(kt->sou.name, ":anon_", strlen(":anon_")) != 0)) {
            PLUGIN_ERROR("Instance '%s': the key of map '%s' is not supported.",
                         ctx->instance, ebpf_metric->map);
            status = -1;
            break;
        }

        struct type *vt = sym->type->map.vtype;
        bool compatible = false;
        switch (ebpf_metric->type) {
        case METRIC_TYPE_GAUGE:
        case METRIC_TYPE_COUNTER:
            compatible = !ebpf_type_is_quantize(vt) && (type_base(vt)->ttype == T_SCALAR);
            break;
        case METRIC_TYPE_HISTOGRAM:
        case METRIC_TYPE_GAUGE_HISTOGRAM:
            compatible = ebpf_type_is_quantize(vt);
            break;
        default:
            break;
        }

        if (!compatible) {
            PLUGIN_ERROR("Instance '%s': the type of metric '%s' is not compatible "
                         "with the values of map '%s'.",
                         ctx->instance, ebpf_metric->metric, ebpf_metric->map);
            status = -1;
            break;
        }

        int nfields = 0;
        tfields_foreach(f, kt->sou.fields)
            nfields++;

        for (size_t i = 0; i < ebpf_metric->labels_from.size; i++) {
            ebpf_label_from_pair_t *pair = &ebpf_metric->labels_from.ptr[i];
            if ((pair->from < 0) || (pair->from >= nfields)) {
                PLUGIN_ERROR("Instance '%s': index %d of 'label-from' '%s' is out of range, "
                             "the key of map '%s' has %d fields.", ctx->instance,
                             pair->from, pair->key, ebpf_metric->map, nfields);
                status = -1;
                break;
            }
        }

        if (status != 0)
            break;
    }

    c_avl_iterator_destroy(iter);

    return status;
}

static int ebpf_ply_compile(ebpf_ctx_t *ctx)
{

    ply_init();

    ctx->ply = ply_alloc(ctx->instance, true);
    if (ctx->ply == NULL) {
        PLUGIN_ERROR("Failed to allocate ply.");
        return -1;
    }

    if (ctx->buf_pages > 0)
        ctx->ply->buf_pages = ctx->buf_pages;

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

    status = ebpf_check_metrics(ctx);
    if (status != 0)
        return -1;

    return 0;
}

static int ebpf_ply_load(ebpf_ctx_t *ctx)
{
    struct ply_return ret = {0};
    ret = ply_load(ctx->ply);
    if (ret.exit || ret.err) {
        PLUGIN_ERROR("Failed to load ply script of instance '%s'.", ctx->instance);
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

    ebpf_metric->type = METRIC_TYPE_GAUGE;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("name", child->key) == 0) {
            status = cf_util_get_string(child, &ebpf_metric->metric);
        } else if (strcasecmp("help", child->key) == 0) {
            status = cf_util_get_string(child, &ebpf_metric->help);
        } else if (strcasecmp("type", child->key) == 0) {
            int allow = CONFIG_METRIC_GAUGE | CONFIG_METRIC_COUNTER |
                        CONFIG_METRIC_HISTOGRAM | CONFIG_METRIC_GAUGE_HISTOGRAM;
            status = cf_util_get_metric_type(child, allow, &ebpf_metric->type);
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
        } else if (strcasecmp("buffer-pages", child->key) == 0) {
            status = cf_util_get_unsigned_int(child, &ctx->buf_pages);
            if ((status == 0) &&
                ((ctx->buf_pages == 0) || ((ctx->buf_pages & (ctx->buf_pages - 1)) != 0))) {
                PLUGIN_ERROR("The '%s' option in %s:%d must be a power of two.",
                             child->key, cf_get_file(child), cf_get_lineno(child));
                status = -1;
            }
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

    status = ebpf_ply_compile(ctx);
    if (status != 0) {
        ebpf_free(ctx);
        return -1;
    }

    label_set_add(&ctx->labels, true, "instance", ctx->instance);

    ctx->interval = interval;

    ebpf_ctx_t **last = &ebpf_pending;
    while (*last != NULL)
        last = &(*last)->next;
    *last = ctx;

    return 0;
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
    if (ebpf_pending == NULL)
        return 0;

    memlock_uncap();

    check_capabilities();

    check_tracefs();

    check_kernel_config();

    int ret = 0;
    while (ebpf_pending != NULL) {
        ebpf_ctx_t *ctx = ebpf_pending;
        ebpf_pending = ctx->next;
        ctx->next = NULL;

        if (ebpf_ply_load(ctx) != 0) {
            ebpf_free(ctx);
            ret = -1;
            continue;
        }

        int status = plugin_register_complex_read("ebpf", ctx->instance, ebpf_read, ctx->interval,
                                         &(user_data_t){.data = ctx, .free_func = ebpf_free});
        if (status != 0)
            ret = -1;
    }

    return ret;
}

static int ebpf_shutdown(void)
{
    while (ebpf_pending != NULL) {
        ebpf_ctx_t *ctx = ebpf_pending;
        ebpf_pending = ctx->next;
        ebpf_free(ctx);
    }

    return 0;
}

void module_register(void)
{
    plugin_register_init("ebpf", ebpf_init);
    plugin_register_shutdown("ebpf", ebpf_shutdown);
    plugin_register_config("ebpf", ebpf_config);
}
