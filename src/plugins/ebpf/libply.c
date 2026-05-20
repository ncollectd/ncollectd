// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz
// SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com>

// From https://github.com/wkz/ply

#pragma GCC diagnostic ignored "-Wunused-result"

#define _GNU_SOURCE
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>

#include "internal.h"
#include "ply.h"

#include "plugins/ebpf/grammar.h"
#include "plugins/ebpf/lexer.h"

#if 0
struct ply_config ply_config = {
    .map_elems   =  0x400,
    .string_size =   0x80,
    .buf_pages   =      1,
    .stack_depth =   0x20,

    .sort = 1,
    .ksyms = 1,
    .strict = 1,
};
#endif

void ply_map_print(struct ply *ply, struct sym *sym, FILE *fp)
{
    struct type *t = sym->type;
    int err;

    size_t key_size = type_sizeof(t->map.ktype);
    size_t val_size = type_sizeof(t->map.vtype);
    size_t row_size = key_size + val_size;

    /* TODO: if (!ply_config.sort) => call printers directly from
     * bpf_map_next loop. In that case we only need space for two
     * keys and one value. This means we can get unsorted output
     * in low memory environments. */
    char *data = calloc(ply->map_elems, row_size);
    if (!data) {
        PLUGIN_ERROR("not enough memory to dump '%s'.", sym->name);
        return;
    }

    char *key = data;
    char *val = data + key_size;

    size_t n_elems;
    for (n_elems = 0, err = bpf_map_next(sym->mapfd, NULL, key); !err;
         err = bpf_map_next(sym->mapfd, key - row_size, key)) {
        err = bpf_map_lookup(sym->mapfd, key, val);
        if (err)
            goto err_free;

        key += row_size;
        val += row_size;
        n_elems++;
    }

    if (n_elems == 0)
        goto err_free;

    if (ply->sort)
        qsort_r(data, n_elems, row_size, type_cmp, t);

    fprintf(fp, "\n%s:\n", sym->name);
    for (char *row = data; n_elems > 0; row += row_size, n_elems--) {
        type_fprint(t->map.ktype, fp, row);
        fputs(": ", fp);

        type_fprint(t->map.vtype, fp, row + type_sizeof(t->map.ktype));
        fputc('\n', fp);
    }

err_free:
    free(data);
}

void ply_maps_print(struct ply *ply)
{
    struct sym **symp, *sym;

    symtab_foreach(&ply->globals, symp) {
        sym = *symp;

        if ((sym->type->ttype == T_MAP)
            && (sym->type->map.mtype != BPF_MAP_TYPE_PERF_EVENT_ARRAY)
            && (sym->type->map.mtype != BPF_MAP_TYPE_STACK_TRACE))
            ply_map_print(ply, sym, stdout);
    }    
}

void ply_map_clear(struct ply *ply, struct sym *sym)
{
    struct type *t = sym->type;
    size_t n_elems;
    int err;

    size_t key_size = type_sizeof(t->map.ktype);

    char *data = calloc(ply->map_elems, key_size);
    if (!data) {
        PLUGIN_ERROR("Not enough memory to clear '%s'.", sym->name);
        return;
    }

    char *key = data;

    for (n_elems = 0, err = bpf_map_next(sym->mapfd, NULL, key); !err;
         err = bpf_map_next(sym->mapfd, key - key_size, key)) {
        key += key_size;
        n_elems++;
    }

    for (key = data; n_elems > 0; key += key_size, n_elems--) {
        bpf_map_delete(sym->mapfd, key);
    }

    free(data);
}

void ply_probe_free(__attribute__((unused)) struct ply *ply, struct ply_probe *pb)
{
    free(pb->probe);
    node_free(pb->ast);
    if (pb->ir != NULL) {
        for (size_t i = 0; i < pb->ir->len; i++) {
            struct vinsn *vi = &pb->ir->vi[i];
            if (vi->vitype == VI_COMMENT)
                free(vi->comment);
        }
        free(pb->ir->vi);
        free(pb->ir);  
    }
    
    symtab_reset(&pb->locals);

    /* TODO */
    free(pb);
}

int __ply_probe_alloc(struct ply *ply, struct node *pspec, struct node *ast)
{
    struct ply_probe *last;
    int err;

    struct ply_probe *pb = xcalloc(1, sizeof(*pb));

    pb->ply = ply;
    pb->ast = ast;
    pb->probe = strdup(pspec->string.data);

    pb->provider = provider_get(pb->probe);
    if (!pb->provider) {
        PLUGIN_ERROR("%s: No provider found for %s.", STRNODELOC(pspec), STRNODE(pspec));
        err = -EINVAL;
        goto err_free_probe;
    }

    pb->ir = ir_new();

    err = pb->provider->probe(pb);
    if (err)
        goto err_free_ir;

    node_free(pspec);

    if (!ply->probes) {
        ply->probes = pb;
        return 0;
    }

    for (last = ply->probes; last->next; last = last->next);
    pb->prev = last;
    last->next = pb;
    return 0;

err_free_ir:
    free(pb->ir);
err_free_probe:
    free(pb->probe);
    free(pb);
    node_free(pspec);
    return err;
}

int ply_fparse(struct ply *ply, FILE *fp)
{
    yyscan_t scanner;
    
    if (yylex_init(&scanner))
        return -EINVAL;

    yyset_in(fp, scanner);
    if (yyparse(scanner, ply))
        return -EINVAL;
 
    yylex_destroy(scanner); 
    return 0;
}

int ply_parsef(struct ply *ply, const char *fmt, ...)
{
    va_list ap;
    size_t bufsz;
    char *buf;
    FILE *fp;
    int err;

    fp = open_memstream(&buf, &bufsz);

    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);

    rewind(fp);
    err = ply_fparse(ply, fp);
    fclose(fp);
    free(buf);
    return err;
}

static int ply_unload_map(struct ply *ply)
{
    struct sym **symp, *sym;

    symtab_foreach(&ply->globals, symp) {
        sym = *symp;

        if (sym->type->ttype != T_MAP)
            continue;

        if (sym->mapfd >= 0)
            close(sym->mapfd);
    }
    
    return 0;
}

static int ply_unload_bpf(struct ply *ply)
{
    struct ply_probe *pb;

    ply_probe_foreach(ply, pb) {
        close(pb->bpf_fd);
    }

    return 0;
}

static int ply_unload_detach(struct ply *ply)
{
    struct ply_probe *pb;
    int err;

    ply_probe_foreach(ply, pb) {
        if (!pb->provider->detach)
            continue;

        err = pb->provider->detach(pb);
        if (err)
            return err;
    }

    return 0;
}

int ply_unload(struct ply *ply)
{
    int err;

    err  = ply_unload_detach(ply);
    err |= ply_unload_bpf(ply);
    err |= ply_unload_map(ply);
    return err;
}

static int ply_load_map(struct ply *ply)
{
    struct sym **symp, *sym;
    struct type *t;

    symtab_foreach(&ply->globals, symp) {
        sym = *symp;
        t = sym->type;

        if (t->ttype != T_MAP)
            continue;

        sym->mapfd = bpf_map_create(t->map.mtype,
                        type_sizeof(t->map.ktype),
                        type_sizeof(t->map.vtype),
                        t->map.len ? t->map.len : ply->map_elems);
        if (sym->mapfd < 0) {
            PLUGIN_ERROR("Unable to create map '%s', errno:%d.", sym->name, errno);
            return -errno;
        }

        if (t->map.mtype == BPF_MAP_TYPE_PERF_EVENT_ARRAY) {
            sym->priv = buffer_new(ply, sym->mapfd);
            if (!sym->priv) {
                PLUGIN_ERROR("Unable to create buffer '%s'.", sym->name);
                return -EINVAL;
            }

            if (!strcmp("stdbuf", sym->name))
                ply->stdbuf = sym;
        }
    }

    return 0;
}

static int ply_load_bpf(struct ply *ply)
{
    struct ply_probe *pb;
    size_t vlog_sz = 0;
    char *vlog = NULL;
    int err = 0;

    if (ply->verify) {
        /* According to libbpf, the recommended buffer size is
         * 16MB (!) */
        vlog = malloc(16 << 20);
        if (vlog) {
            vlog_sz = 16 << 20;
            goto load;
        }

        PLUGIN_WARNING("Not enough memory for the recommended 16M verifier "
                       "buffer, trying 1M.");

        vlog = malloc(1 << 20);
        if (vlog) {
            vlog_sz = 1 << 20;
            goto load;
        }

        PLUGIN_WARNING("Not enough memory to enable the kernel verifier output.");
    }

load:
    ply_probe_foreach(ply, pb) {
        struct bpf_insn *insns;
        int n_insns;

        err = ir_bpf_extract(pb->ir, &insns, &n_insns);
        if (err)
            break;

        pb->bpf_fd = bpf_prog_load(pb->provider->prog_type, insns, n_insns, vlog, vlog_sz);
        free(insns);
        if (pb->bpf_fd < 0) {
            PLUGIN_ERROR("Unable to load %s, errno:%d", pb->probe, errno);
            if ((errno == EINVAL) && vlog && !vlog[0])
                PLUGIN_WARNING("Was ply built against the running kernel?");
            else if (vlog && vlog[0])
                PLUGIN_ERROR("Output from kernel bpf verifier: %s", vlog);
            else
                PLUGIN_ERROR("No output from kernel bpf verifier.");

            err = -errno;
            break;
        }
    }

    if (vlog)
        free(vlog);

    return err;
}

static struct ply_return ply_load_attach(struct ply *ply)
{
    struct ply_probe *pb;
    struct ply_return ret;
    int err;

    ply_probe_foreach(ply, pb) {
        if (!pb->provider->attach)
            continue;

        err = pb->provider->attach(pb);
        if (err)
            goto err;
    }

    return (struct ply_return){0};

err:
    ret.err = 1;
    ret.val = err;
    return ret;
}

struct ply_return ply_load(struct ply *ply)
{
    int err;
    struct ply_return ret = { .err = 1, };

    /* Maps has to be allocated first, since we need those fds
     * before calling ir_bpf_extract. */
    err = ply_load_map(ply);
    if (err)
        goto err_free_evp;

    /* Load programs in to the kernel. */
    err = ply_load_bpf(ply);
    if (err)
        goto err_free_map;

    ret = ply_load_attach(ply);
    if (ret.err || ret.exit)
        goto err_free_prog;

    return ret;
err_free_prog:
    ply_unload_bpf(ply);
err_free_map:
    ply_unload_map(ply);
err_free_evp:
    /* TODO evpipe_free(&ply->evp); */
    if (err)
        ret.val = err;
    return ret;

}

struct ply_return ply_service(struct ply *ply, int ready, struct pollfd *fds)
{
    if (ply->stdbuf)
        return buffer_service(ply, ply->stdbuf->priv, ready, fds);

    return (struct ply_return){ .err = 0, .val = 0 };
}

nfds_t ply_get_nfds(struct ply *ply)
{
    if (ply->stdbuf)
        return buffer_get_nfds(ply->stdbuf->priv);

    return 0;
}

void ply_fill_pollset(struct ply *ply, struct pollfd *fds)
{
    if (ply->stdbuf)
        buffer_fill_pollset(ply->stdbuf->priv, fds);
}

int ply_stop(struct ply *ply)
{
    struct ply_probe *pb;
    int err;

    if (ply->group_fd >= 0) {
        err = perf_event_disable(ply->group_fd);
        if (err)
            return err;
    }

    ply_probe_foreach(ply, pb) {
        if (!pb->provider->stop)
            continue;

        err = pb->provider->stop(pb);
        if (err)
            return err;
    }

    return 0;
}

int ply_start(struct ply *ply)
{
    struct ply_probe *pb;
    int err;

    ply_probe_foreach(ply, pb) {
        if (!pb->provider->start)
            continue;

        err = pb->provider->start(pb);
        if (err)
            return err;
    }

    if (ply->group_fd < 0)
        return 0;

    return perf_event_enable(ply->group_fd);
}

void ply_free(struct ply *ply)
{
    struct ply_probe *pb, *next;

    for (pb = ply->probes; pb;) {
        next = pb->next;
        ply_probe_free(ply, pb);
        pb = next;
    }

    if (ply->ksyms)
        ksyms_free(ply->ksyms);
    
    symtab_reset(&ply->globals);

    free(ply->group);

    /* TODO: evpipe_free(&ply->evp); */

    free(ply);
}

struct ply *ply_alloc(char *group, bool ksyms_cache)
{
    struct ply *ply = calloc(1, sizeof(*ply));
    if (!ply)
        return NULL;

    ply->globals.global = 1;
    asprintf(&ply->group, "ply-%s-%d", group, getpid());
    ply->group_fd = -1;

    ply->map_elems = 0x400;
    ply->string_size = 0x80;
    ply->buf_pages = 1;
    ply->stack_depth = 0x20;
    ply->sort = true;
    ply->ksyms_cache = ksyms_cache;
    ply->strict = true;
    ply->verify = false;

    if (ply->ksyms_cache) //  FIXME
        ply->ksyms = ksyms_new();

    return ply;
}

void ply_init(void)
{
    static int init_done = 0;

    if (init_done)
        return;

    provider_init();
    built_in_init();

    init_done = 1;
}
