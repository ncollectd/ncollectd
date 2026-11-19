// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz
// SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com>

// From https://github.com/wkz/ply

/* Keeps an in-memory copy of the text symbols in /proc/kallsyms, with
 * each symbol using a fixed amount of space. This let's us lookup
 * addresses using bsearch(3), making stacktrace resolution much
 * faster.
 *
 * The first and last symbols are the special `nullsym` and `endsym`
 * symbols defined below. These are always present but never included
 * in the range given to bsearch(3) thus making it safe to always look
 * at the previous and next record.
 */

#define _POSIX_C_SOURCE 200809L /* strndup */
#define _XOPEN_SOURCE        /* strptime */
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <linux/version.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <linux/capability.h>

#include "ply.h"
#include "kallsyms.h"
#include "perf_event.h"


struct kfunc_list {
    int size;
    char **kfunc;
};

static struct ksym nullsym = {
    .addr = 0,
    .sym = "NULL",
};

static struct ksym endsym = {
    .addr = UINTPTR_MAX,
    .sym = "END",
};

static int ksym_cmp(const void *_key, const void *_member)
{
    const struct ksym *key = _key, *member = _member;

    if (key->addr < member->addr)
        return -1;
    else if (key->addr >= (member + 1)->addr)
        return 1;

    return 0;
}

int ksym_fprint(struct ksyms *ks, FILE *fp, uintptr_t addr)
{
    const struct ksym *sym;

    if (ks && (sym = ksym_get(ks, addr))) {
        if (sym->addr == addr)
            return fputs(sym->sym, fp);
        else
            return fprintf(fp, "%s+%"PRIuPTR, sym->sym, addr - sym->addr);
    } else {
        int w = (int)(sizeof(addr) * 2);

        return fprintf(fp, "<%*.*"PRIxPTR">", w, w, addr);
    }
}

const struct ksym *ksym_get(struct ksyms *ks, uintptr_t addr)
{
    struct ksym key = { .addr = addr };

    if (!ks)
        return NULL;

    return bsearch(&key, ks->cache->sym,
               ks->cache->hdr.n_syms - 1, sizeof(key), ksym_cmp);
}

static int kfunc_list_cmp(const void *_key, const void *_member)
{
    const char * const *key = _key, * const *member = _member;

    return strcmp(*key, *member);
}

static void kfunc_list_sort(struct kfunc_list *kfuncs)
{
    if (!kfuncs->kfunc)
        return;

    qsort(kfuncs->kfunc, kfuncs->size, sizeof(char *), kfunc_list_cmp);
}

static int kfunc_list_find(struct kfunc_list *kfuncs, const char *name)
{
    if (!kfuncs->kfunc)
        return 1;

    return !!bsearch(&name, kfuncs->kfunc, kfuncs->size, sizeof(char *), kfunc_list_cmp);
}

static int kfunc_list_build(struct kfunc_list *kfuncs)
{
    char *line = NULL;
    size_t len = 0;
    char **list = NULL;
    int size = 0;
    int count = 0;

    FILE *fp = fopenf("r", "%savailable_filter_functions", tracepath());
    if (!fp) {
        PLUGIN_WARNING("cannot open %savailable_filter_functions: %s.", tracepath(), strerror(errno));
        return -1;
    }

    while (getline(&line, &len, fp) >= 0) {
        struct ksym *ksym;

        char *saveptr = NULL;
        char *p = strtok_r(line, " \t\n", &saveptr);
        if (!p)
            continue;

        if (count >= size) {
            size = size ? (size * 2) : 1024;
            list = realloc(list, size * sizeof(*list));
            assert(list);
        }

        list[count] = strndup(p, sizeof(ksym->sym) - 1);
        assert(list[count]);

        count++;
    }
    free(line);
    kfuncs->size = count;
    kfuncs->kfunc = list;

    kfunc_list_sort(kfuncs);

    fclose(fp);
    return 0;
}

static void kfunc_list_free(struct kfunc_list *kfuncs)
{
    int i;

    for (i = 0; i < kfuncs->size; i++)
        free(kfuncs->kfunc[i]);
    free(kfuncs->kfunc);

    kfuncs->size = 0;
    kfuncs->kfunc = NULL;
}

static int ksym_parse(FILE *fp, struct ksym *ksym, char **line, size_t *len)
{
    char *p;

    while (getline(line, len, fp) >= 0) {
        ksym->addr = strtoul(*line, &p, 16);
        if ((p == *line) || (ksym->addr == ULONG_MAX) || (*p != ' '))
            continue;

        p++;
        if (*p != 't' && *p != 'T')
            continue;

        p++;
        if (*p != ' ')
            continue;

        char *saveptr = NULL;
        p = strtok_r(p, " \t\n", &saveptr);
        if (!p)
            continue;

        strncpy(ksym->sym, p, sizeof(ksym->sym) - 1);
        return 0;
    }

    return EOF;
}

static int ksym_sort_cmp(const void *_a, const void *_b)
{
    const struct ksym *a = _a, *b = _b;

    if (a->addr > b->addr)
        return 1;
    else if (a->addr < b->addr)
        return -1;
    else
        return 0;
}

/* Anyone may read /proc/kallsyms, but if the reader does not posses
 * the CAP_SYSLOG capability, all symbol addresses are set to zero. So
 * we make sure that we have it to avoid creating a useless cache. If
 * there is some less hacky way of getting this information without
 * depending on libcap, please refactor. */
static int ksyms_cache_cap(void)
{
    FILE *fp;
    char line[0x100];
    int err = -EPERM;
    uint64_t caps = 0;

    fp = fopen("/proc/self/status", "r");
    if (!fp)
        return err;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "CapEff:") != line)
            continue;

        caps = strtoull(&line[7], NULL, 16);
        err = 0;
        break;
    }

    fclose(fp);
    if (err)
        return err;

    return (caps & (1ULL << CAP_SYSLOG)) ? 0 : -EPERM;
}

static int ksyms_append(struct ksyms *ks, size_t *size, const struct ksym *ksym)
{
    if (ks->cache->hdr.n_syms >= *size) {
        size_t nsize = *size * 2;
        struct ksym_cache *tmp = realloc(ks->cache, sizeof(*tmp) + nsize * sizeof(tmp->sym[0]));
        if (!tmp)
            return -ENOMEM;

        ks->cache = tmp;
        *size = nsize;
    }

    ks->cache->sym[ks->cache->hdr.n_syms++] = *ksym;
    return 0;
}

static int ksyms_build(struct ksyms *ks)
{
    struct kfunc_list kfuncs = {0};
    struct ksym ksym = {0};
    size_t size = 0x1000;
    char *line = NULL;
    size_t len = 0;

    int err = ksyms_cache_cap();
    if (err)
        goto out;

    char *path = plugin_procpath("kallsyms");
    if (!path) {
        err = -ENOMEM;
        goto out;
    }

    FILE *kfp = fopen(path, "r");
    if (!kfp) {
        err = -errno;
        free(path);
        goto out;
    }
    free(path);

    ks->cache = calloc(1, sizeof(*ks->cache) + size * sizeof(ks->cache->sym[0]));
    if (!ks->cache) {
        err = -ENOMEM;
        goto close_kfp;
    }

    /* Ignore failures due to missing filter functions. */
    kfunc_list_build(&kfuncs);

    err = ksyms_append(ks, &size, &nullsym);
    if (err)
        goto close_kfp;

    while (!(err = ksym_parse(kfp, &ksym, &line, &len))) {
        if (!kfunc_list_find(&kfuncs, ksym.sym))
            continue;

        err = ksyms_append(ks, &size, &ksym);
        if (err)
            goto close_kfp;
    }

    err = ksyms_append(ks, &size, &endsym);
    if (err)
        goto close_kfp;

    /* Sort everything between NULL and END */
    qsort(&ks->cache->sym[1], ks->cache->hdr.n_syms - 2,
          sizeof(struct ksym), ksym_sort_cmp);

close_kfp:
    free(line);
    kfunc_list_free(&kfuncs);
    fclose(kfp);
out:
    if (err)
        PLUGIN_WARNING("Unable to load kernel symbols: %s.", STRERROR(-err));

    return err;
}

void ksyms_free(struct ksyms *ks)
{
    free(ks->cache);
    free(ks);
}

struct ksyms *ksyms_new(void)
{
    struct ksyms *ks = calloc(1, sizeof(*ks));
    if (!ks)
        return NULL;

    if (ksyms_build(ks)) {
        ksyms_free(ks);
        return NULL;
    }

    return ks;
}
