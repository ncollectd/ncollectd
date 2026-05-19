// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz
// SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com>

// From https://github.com/wkz/ply

#include "plugin.h"
#include "libutils/strbuf.h"

#include "ply.h"
#include "internal.h"


static void __sgr(FILE *fp, int sgr, const char *s)
{
    if (!s)
        return;

    fprintf(fp, "\033[%dm%s\033[0m", sgr, s);
}

static void __bold(FILE *fp, const char *s)
{
    __sgr(fp, 1, s);
}

static void __faint(FILE *fp, const char *s)
{
    __sgr(fp, 2, s);
}

static void type_dump_func(struct type *t, const char *name, FILE *fp)
{
    type_dump(t->func.type, NULL, fp);
    fprintf(fp, " (*\033[1m%s\033[0m)(", name ? name : "");

    if (!t->func.args) {
        __faint(fp, t->func.vargs ? "..." : "void");
        fputc(')', fp);
        return;
    }

    tfields_foreach(arg, t->func.args) {
        if (arg != t->func.args)
            fputs(", ", fp);

        type_dump(arg->type, NULL, fp);
    }

    if (t->func.vargs)
        __faint(fp, ", ...");

    fputc(')', fp);
}

void type_dump(struct type *t, const char *name, FILE *fp)
{
    if (!t) {
        __faint(fp, "(none)");
        goto print_name;
    }

    switch (t->ttype){
    case T_VOID:
        __faint(fp, "void");
        break;
    case T_TYPEDEF:
        __faint(fp, t->tdef.name);
        break;
    case T_SCALAR:
        __faint(fp, t->scalar.name);
        break;
    case T_POINTER:
        type_dump(t->ptr.type, NULL, fp);

        if (t->ptr.bpf)
            fputs(" __bpf", fp);

        fputs(" *", fp);
        __bold(fp, name);
        return;
    case T_ARRAY:
        type_dump(t->array.type, NULL, fp);
        fputs(name ? " " : "", fp);
        __bold(fp, name);
        fprintf(fp, "[%zu]", t->array.len);
        return;
    case T_STRUCT:
        fputs("struct ", fp);
        __faint(fp, t->sou.name);
        break;
    case T_FUNC:
        type_dump_func(t, name, fp);
        return;
    case T_MAP:
        type_dump(t->map.vtype, NULL, fp);
        fputs(name ? " " : "", fp);        
        __bold(fp, name);
        fputc('{', fp);
        type_dump(t->map.ktype, NULL, fp);
        fputc('}', fp);
        return;
    }
print_name:
    fputs(name ? " " : "", fp);
    __bold(fp, name);
}

static void type_dump_decl_sou(struct type *t, FILE *fp)
{
    struct tfield *f;

    type_dump(t, NULL, fp);
    fputs(" {\n", fp);
    for (f = t->sou.fields; f->type->ttype != T_VOID; f++) {
        fputc('\t', fp);
        type_dump(f->type, NULL, fp);
        fprintf(fp, " %s;\n", f->name);
    }
    fputs("}", fp);
}

void type_dump_decl(struct type *t, FILE *fp)
{
    switch (t->ttype) {
    case T_TYPEDEF:
        fputs("typedef ", fp);
        type_dump(t->tdef.type, NULL, fp);
        fprintf(fp, " %s", t->tdef.name);
        break;

    case T_STRUCT:
        type_dump_decl_sou(t, fp);
        break;

    case T_VOID:
    case T_SCALAR:
    case T_POINTER:
    case T_ARRAY:
    case T_MAP:
    case T_FUNC:
        type_dump(t, NULL, fp);
        break;
    }
}

void type_free(struct type *t)
{
    if (t->stype)
        return;

    switch (t->ttype) {
    case T_TYPEDEF:
        free(t->tdef.name);
        break;
    case T_STRUCT:
        tfields_foreach(f, t->sou.fields) {
            free(f->name);
        }
        free(t->sou.fields);
        free(t->sou.name);
        break;
    case T_VOID:
    case T_SCALAR:
    case T_POINTER:
    case T_ARRAY:
    case T_MAP:
    case T_FUNC:
        break;
    }

    free(t);
}

static int strtype_strbuf(struct type *t, char *name, strbuf_t *sbuf)
{
    if (!t) {
        strbuf_putstr(sbuf, "(none)");
        goto print_name;
    }

    switch (t->ttype){
    case T_VOID:
        strbuf_putstr(sbuf, "void");
        break;
    case T_TYPEDEF:
        strbuf_putstr(sbuf, t->tdef.name);
        break;
    case T_SCALAR:
        strbuf_putstr(sbuf, t->scalar.name);
        break;
    case T_POINTER:
        strtype_strbuf(t->ptr.type, NULL, sbuf);
        if (t->ptr.bpf)
            strbuf_putstr(sbuf, " __bpf");
        strbuf_putstr(sbuf, " *");
        if (name != NULL)
            strbuf_putstr(sbuf, name);
        return 0;
    case T_ARRAY:
        strtype_strbuf(t->array.type, NULL, sbuf);
        if (name != NULL) {
            strbuf_putchar(sbuf, ' ');
            strbuf_putstr(sbuf, name);
        }
        strbuf_putchar(sbuf, '[');
        strbuf_putint(sbuf, t->array.len);
        strbuf_putchar(sbuf, ']');
        return 0;
    case T_STRUCT:
        strbuf_putstr(sbuf, "struct ");
        strbuf_putstr(sbuf, t->sou.name);
        break;
    case T_FUNC:
        strtype_strbuf(t->func.type, NULL, sbuf);
        strbuf_putstr(sbuf, "(*");
        if (name != NULL)
            strbuf_putstr(sbuf, name);
        strbuf_putstr(sbuf, ")(");
        if (!t->func.args) {
            strbuf_putstr(sbuf, t->func.vargs ? "..." : "void");
        } else {
            tfields_foreach(arg, t->func.args) {
                if (arg != t->func.args)
                    strbuf_putstr(sbuf, ", ");

                strtype_strbuf(arg->type, NULL, sbuf);
            }

            if (t->func.vargs)
                strbuf_putstr(sbuf, ", ...");

        }
        strbuf_putchar(sbuf, ')');
        return 0;
    case T_MAP:
        strtype_strbuf(t->map.vtype, NULL, sbuf);
        if (name != NULL) {
            strbuf_putchar(sbuf, ' ');
            strbuf_putstr(sbuf, name);
        }
        strbuf_putchar(sbuf, '{');
        strtype_strbuf(t->map.ktype, NULL, sbuf);
        strbuf_putchar(sbuf, '}');
        return 0;
    }
print_name:
    if (name != NULL) {
        strbuf_putchar(sbuf, ' ');
        strbuf_putstr(sbuf, name);
    }

    return 0;
}

char *strtype(struct type *t, char *buf, size_t buflen)
{
    strbuf_t sbuf = STRBUF_CREATE_FIXED(buf, buflen);

    strtype_strbuf(t, NULL, &sbuf);

    return buf;
}

int type_vfprintxf(__attribute__((unused)) struct printxf *pxf, FILE *fp,
                   __attribute__((unused)) const char *spec, va_list ap)
{
    struct type *t;

    t = va_arg(ap, struct type *);
    type_dump(t, NULL, fp);
    return 0;
}

static int type_fprint_scalar(struct type *t, FILE *fp, bool hex, const void *data)
{
    switch ((t->scalar.size << 1) | t->scalar.unsignd) {
    case (1 << 1) | 1:
        return fprintf(fp, hex ? "%#"PRIx8 : "%"PRIu8, *((const uint8_t *)data));
    case (1 << 1) | 0:
        return fprintf(fp, "%"PRId8, *((const int8_t *)data));

    case (2 << 1) | 1:
        return fprintf(fp, hex ? "%#"PRIx16 : "%"PRIu16, *((const uint16_t *)data));
    case (2 << 1) | 0:
        return fprintf(fp, "%"PRId16, *((const int16_t *)data));

    case (4 << 1) | 1:
        return fprintf(fp, hex ? "%#"PRIx32 : "%"PRIu32, *((const uint32_t *)data));
    case (4 << 1) | 0:
        return fprintf(fp, "%"PRId32, *((const int32_t *)data));

    case (8 << 1) | 1:
        return fprintf(fp, hex ? "%#"PRIx64 : "%"PRIu64, *((const uint64_t *)data));
    case (8 << 1) | 0:
        return fprintf(fp, "%"PRId64, *((const int64_t *)data));
    }

    assert(0);
    return 0;
}

static int type_fprint_pointer(struct type *t, FILE *fp, const void *data)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wduplicated-branches"
    if (t->ptr.bpf)
        return fprintf(fp, "<%"PRIx64">", *((const uint64_t *)data));
    else
        return fprintf(fp, "<%"PRIxPTR">", *((const uintptr_t *)data));
#pragma GCC diagnostic pop
}

static void __hexdump_line(FILE *fp, size_t offset, const unsigned char *data, size_t n)
{
    size_t i;
    int pad;

    fprintf(fp, "%03zx: ", offset);

    for (i = 0; i < n; i++) {
        if (i == 8)
            fputc(' ', fp);

        fprintf(fp, " %2.2x", data[i]);
    }

    pad = (0x10 - i) * 3 + 3;
    if (i < 8)
        pad++;

    fprintf(fp, "%*c", pad, ' ');

    for (i = 0; i < n; i++) {
        if (i == 8)
            fputc(' ', fp);

        fputc(isprint(data[i]) ? data[i] : '.', fp);
    }
}

static int type_fprint_char_array(struct type *t, FILE *fp, const void *data)
{
    const unsigned char *d = data;
    size_t i;

    if (isstring(data, t->array.len))
        return fprintf(fp, "%-*s", (int)t->array.len - 1, (const char *)data);

    fputc('\n', fp);

    for (i = 0; (i + 0xf) < t->array.len; i += 0x10) {
        __hexdump_line(fp, i, &d[i], 0x10);
        fputc('\n', fp);
    }

    if (i < t->array.len) {
        __hexdump_line(fp, i, &d[i], t->array.len - i);
        fputc('\n', fp);
    }
    return 0;
}

static int type_fprint_array(struct type *t, FILE *fp, const void *data)
{
    size_t i;
    int ret, total = 0;

    if (t->array.type == &t_char)
        return type_fprint_char_array(t, fp, data);

    fputc('[', fp);
    total++;
    const uint8_t *udata = (const uint8_t *)data;
    for (i = 0; i < t->array.len; i++) {
        if (i) {
            fputs(", ", fp);
            total += 2;
        }

        ret = type_fprint(t->array.type, fp, udata);
        if (ret < 0)
            return ret;

        total += ret;
        udata += type_sizeof(t->array.type);
    }
    fputc(']', fp);
    total++;

    return total;
}

static int type_fprint_map(struct type *t, FILE *fp, const void *data)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
    struct ply *ply = (const struct ply *)data;
    struct sym **symp;

    symtab_foreach(&ply->globals, symp) {
        if ((*symp)->type == t) {
            ply_map_print(ply, *symp, fp);
            break;
        }
    }
    return 0;
#pragma GCC diagnostic pop
}

int type_fprint_struct(struct type *t, FILE *fp, const void *data)
{
    size_t offs;
    int anon, ret;

    anon = !strncmp(t->sou.name, ":anon_", strlen(":anon_"));

    fputs(anon ? "{ " : "{\n\t", fp);

    tfields_foreach(f, t->sou.fields) {
        offs = type_offsetof(t, f->name);
        if (offs)
            fputs(anon ? ", " : ",\n\t", fp);

        if (!anon)
            fprintf(fp, "%s = ", f->name);

        ret = type_fprint(f->type, fp, ((const uint8_t *)data) + offs);
        if (ret < 0)
            return ret;
    }

    fputs(anon ? " }" : "\n}", fp);

    return 0;
}

int type_fprint(struct type *t, FILE *fp, const void *data)
{
    if (t->fprint)
        return t->fprint(t, fp, data);

    switch (t->ttype) {
    case T_VOID:
        return fprintf(fp, "void");
    case T_TYPEDEF:
        return type_fprint(t->tdef.type, fp, data);
    case T_SCALAR:
        return type_fprint_scalar(t, fp, false, data);
    case T_POINTER:
        return type_fprint_pointer(t, fp, data);
    case T_FUNC:
        return type_fprint_pointer(type_ptr_of(&t_void, 0), fp, data);
    case T_ARRAY:
        return type_fprint_array(t, fp, data);
    case T_MAP:
        return type_fprint_map(t, fp, data);
    case T_STRUCT:
        return type_fprint_struct(t, fp, data);
    }

    assert(0);
    return 0;
}


static int type_cmp_scalar(const void *a, const void *b, struct type *t)
{
    switch ((t->scalar.size << 1) | t->scalar.unsignd) {
    case (1 << 1) | 1:
        return *((const uint8_t *)a) - *((const uint8_t *)b);
    case (1 << 1) | 0:
        return *((const int8_t *)a) - *((const int8_t *)b);
    case (2 << 1) | 1:
        return *((const uint16_t *)a) - *((const uint16_t *)b);
    case (2 << 1) | 0:
        return *((const int16_t *)a) - *((const int16_t *)b);
    case (4 << 1) | 1:
        return *((const uint32_t *)a) - *((const uint32_t *)b);
    case (4 << 1) | 0:
        return *((const int32_t *)a) - *((const int32_t *)b);
    case (8 << 1) | 1:
        return *((const uint64_t *)a) - *((const uint64_t *)b);
    case (8 << 1) | 0:
        return *((const int64_t *)a) - *((const int64_t *)b);
    }

    assert(0);
    return 0;
}

static int type_cmp_pointer(const void *a, const void *b, struct type *t)
{
    if (t->ptr.bpf)
        return *((const uint64_t *)a) - *((const uint64_t *)b);

    return *((const uintptr_t *)a) - *((const uintptr_t *)b);
}

static int type_cmp_array(const void *a, const void *b, struct type *t)
{
    const uint8_t *ua = (const uint8_t *)a;
    const uint8_t *ub = (const uint8_t *)b;

    for (size_t i = 0; i < t->array.len; i++) {
        int cmp = type_cmp(ua, ub, t->array.type);
        if (cmp)
            return cmp;

        ua += type_sizeof(t->array.type);
        ub += type_sizeof(t->array.type);
    }

    return 0;
}

static int type_cmp_map(const void *a, const void *b, struct type *t)
{
    size_t key_size = type_sizeof(t->map.ktype);

    int cmp = type_cmp(((const uint8_t *)a) + key_size, ((const uint8_t *)b) + key_size,
                       t->map.vtype);
    if (cmp)
        return cmp;

    return type_cmp(a, b, t->map.ktype);
}

static int type_cmp_struct(const void *a, const void *b, struct type *t)
{
    tfields_foreach(f, t->sou.fields) {
        size_t offs = type_offsetof(t, f->name);
        int cmp = type_cmp(((const uint8_t *)a) + offs, ((const uint8_t *)b) + offs, f->type);
        if (cmp)
            return cmp;
    }

    return 0;
}

int type_cmp(const void *a, const void *b, void *_type)
{
    struct type *t = _type;

    switch (t->ttype) {
    case T_VOID:
        return 0;
    case T_TYPEDEF:
        return type_cmp(a, b, t->tdef.type);
    case T_SCALAR:
        return type_cmp_scalar(a, b, t);
    case T_POINTER:
        return type_cmp_pointer(a, b, t);
    case T_FUNC:
        return type_cmp_pointer(a, b, type_ptr_of(&t_void, 0));
    case T_ARRAY:
        return type_cmp_array(a, b, t);
    case T_MAP:
        return type_cmp_map(a, b, t);
    case T_STRUCT:
        return type_cmp_struct(a, b, t);
    }

    assert(0);
    return 0;
}

struct type *type_scalar_promote(struct type *t)
{
    assert(type_base(t)->ttype == T_SCALAR);

    if (type_sizeof(t) < type_sizeof(&t_int))
        return &t_int;

    return t;
}

struct type *type_scalar_convert(struct type *a, struct type *b)
{
    a = type_scalar_promote(a);
    b = type_scalar_promote(b);

    if (type_equal(a, b))
        return a;

    a = type_base(a);
    b = type_base(b);

    if (a->scalar.unsignd == b->scalar.unsignd)
        return (type_sizeof(a) > type_sizeof(b)) ? a : b;

    if (a->scalar.unsignd && (type_sizeof(a) >= type_sizeof(b)))
        return a;
    else if (type_sizeof(b) >= type_sizeof(a))
        return b;

    return a->scalar.unsignd ? b : a;
}

int type_equal(struct type *a, struct type *b)
{
    /* TODO */
    return a == b;
}

int type_compatible(struct type *a, struct type *b)
{

    a = type_base(type_return(a));
    b = type_base(type_return(b));

    if (a->ttype != b->ttype)
        return 0;

    switch (a->ttype){
    case T_SCALAR:
        return a->scalar.size == b->scalar.size;
    case T_VOID:
        return 1;
    case T_POINTER:
        return a->ptr.bpf == b->ptr.bpf;
    case T_ARRAY:
        if (a->array.len != b->array.len)
            return 0;

        return type_compatible(a->array.type, b->array.type);
    case T_STRUCT:
    /* case T_UNION: */
        return !strcmp(a->sou.name, b->sou.name);
    case T_FUNC:
        return type_compatible(a->func.type, b->func.type);
    case T_MAP:
        return type_compatible(a->map.vtype, b->map.vtype);

    case T_TYPEDEF:
        assert(0);
    }

    assert(0);
    return 0;
}

static ssize_t type_alignof_struct(struct type *t)
{
    ssize_t falign, align = -EINVAL;

    if (t->sou.packed)
        return 1;

    tfields_foreach(f, t->sou.fields) {
        falign = type_alignof(f->type);

        if (falign < 0)
            return falign;

        if (falign > align)
            align = falign;
    }

    return align;
}

ssize_t type_alignof(struct type *t)
{
    if (!t)
        return -EINVAL;

    switch (t->ttype){
    case T_VOID:
    case T_SCALAR:
    case T_POINTER:
    case T_FUNC:
    case T_MAP:
        return type_sizeof(t);
    case T_TYPEDEF:
        return type_alignof(t->tdef.type);
    case T_ARRAY:
        return type_alignof(t->array.type);
    case T_STRUCT:
        return type_alignof_struct(t);
    }

    return -EINVAL;
}

static size_t __padding(size_t offset, size_t align)
{
    size_t pad = align - (offset & (align - 1));

    return (pad == align) ? 0 : pad;
}

void type_struct_layout(struct type *t)
{
    size_t offset = 0;
    ssize_t fsize, falign;

    assert(t->ttype == T_STRUCT);

    tfields_foreach(f, t->sou.fields) {
        fsize = type_sizeof(f->type);
        falign = type_alignof(f->type);

        assert(fsize >= 0);
        assert(falign >= 0);

        if (!t->sou.packed)
            offset += __padding(offset, falign);

        f->offset = offset;
        offset += fsize;
    }

    if (!t->sou.packed)
        offset += __padding(offset, type_alignof(t));

    t->sou.size = offset;
}

struct tfield *tfields_get(struct tfield *fields, const char *name)
{
    tfields_foreach(f, fields) {
        if (!strcmp(f->name, name))
            return f;
    }

    return NULL;
}

ssize_t type_offsetof(struct type *t, const char *field)
{
    struct tfield *f;

    if (!t || t->ttype != T_STRUCT)
        return -EINVAL;

    f = tfields_get(t->sou.fields, field);
    if (f)
        return f->offset;

    return -ENOENT;
}

ssize_t type_sizeof(struct type *t)
{
    if (!t)
        return -EINVAL;

    switch (t->ttype){
    case T_VOID:
        return sizeof(char);
    case T_SCALAR:
        return t->scalar.size;
    case T_TYPEDEF:
        return type_sizeof(t->tdef.type);
    case T_POINTER:
        if (t->ptr.bpf)
            return sizeof(uint64_t);
        /* fall-through */
    case T_FUNC:
        return sizeof(void *);
    case T_ARRAY:
        return t->array.len * type_sizeof(t->array.type);
    case T_STRUCT:
        return t->sou.size;
    case T_MAP:
        return sizeof(int);
    }

    return -EINVAL;
}

int all_types_cmp(const void *_a, const void *_b)
{
    const struct type *a = *((struct type * const *)_a);
    const struct type *b = *((struct type * const *)_b);

    return a - b;
}

struct type_list {
    struct type **types;
    size_t len;
} all_types;

#define types_foreach(_t) \
    for ((_t) = all_types.types; (_t) < &all_types.types[all_types.len]; (_t)++)

void type_dump_decls(FILE *fp)
{
    struct type **ti, *t;

    types_foreach(ti) {
        t = *ti;
        if (t->ttype == T_SCALAR)
            continue;

        type_dump_decl(t, fp);
        fputc('\n', fp);
    }
}

int type_add(struct type *t)
{
    if (bsearch(t, all_types.types, all_types.len, sizeof(*all_types.types), all_types_cmp))
        return 0;

    /* type_size_set(t); */
    all_types.types = realloc(all_types.types, ++all_types.len * sizeof(*all_types.types));
    all_types.types[all_types.len - 1] = t;
    qsort(all_types.types, all_types.len, sizeof(*all_types.types), all_types_cmp);

    return 0;
}

int type_add_list(struct type **ts)
{
    for (; *ts; ts++) {
        int err = type_add(*ts);
        if (err)
            return err;
    }

    return 0;
}

struct type *type_typedef(struct type *type, const char *name)
{
    struct type **ti, *t;

    types_foreach(ti) {
        t = *ti;
        if ((t->ttype == T_TYPEDEF)
            && (t->tdef.type == type)
            && (!strcmp(t->tdef.name, name)))
            return t;
    }

    t = xcalloc(1, sizeof(*t));
    t->ttype = T_TYPEDEF;
    t->tdef.type = type;
    t->tdef.name = strdup(name);
    type_add(t);
    return t;
}

struct type *type_array_of(struct type *type, size_t len)
{
    struct type **ti, *t;

    types_foreach(ti) {
        t = *ti;
        if ((t->ttype == T_ARRAY)
            && (t->array.type == type)
            && (t->array.len == len))
            return t;
    }

    t = xcalloc(1, sizeof(*t));
    t->ttype = T_ARRAY;
    t->array.type = type;
    t->array.len = len;
    type_add(t);
    return t;
}

struct type *type_map_of(struct type *ktype, struct type *vtype,
             enum bpf_map_type mtype, size_t len)
{
    struct type **ti, *t;

    types_foreach(ti) {
        t = *ti;
        if ((t->ttype == T_MAP)
            && (t->map.ktype == ktype)
            && (t->map.vtype == vtype)
            && (t->map.mtype == mtype)
            && (t->map.len   == len))
            return t;
    }

    t = xcalloc(1, sizeof(*t));
    t->ttype = T_MAP;
    t->map.ktype = ktype;
    t->map.vtype = vtype;
    t->map.mtype = mtype;
    t->map.len   = len;
    type_add(t);
    return t;
}

struct type *type_ptr_of(struct type *type, unsigned bpf)
{
    struct type **ti, *t;

    types_foreach(ti) {
        t = *ti;
        if ((t->ttype == T_POINTER)
            && (t->ptr.type == type)
            && (t->ptr.bpf  == bpf))
            return t;
    }

    t = xcalloc(1, sizeof(*t));
    t->ttype = T_POINTER;
    t->ptr.type = type;
    t->ptr.bpf = bpf;
    type_add(t);
    return t;
}

#define is_signed(_t) (((_t)(-1)) < 0)

#define builtin_scalar(_t) {           \
        .stype = true,                 \
        .ttype = T_SCALAR,             \
        .scalar = {                    \
            .name = #_t,               \
            .size = sizeof(_t),        \
            .unsignd = !is_signed(_t), \
        },                             \
    }

struct type t_void = {
    .stype = true,
    .ttype = T_VOID
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
/* is_signed will generate a warning for unsigned types since the
 * expression can never be true. this is exactly what we're interested
 * in here though. it gets us out of having to specify scalar
 * signedness per architecture. */

struct type t_char  = builtin_scalar(char);
struct type t_schar = builtin_scalar(signed char);
struct type t_uchar = builtin_scalar(unsigned char);

struct type t_short  = builtin_scalar(short);
struct type t_sshort = builtin_scalar(signed short);
struct type t_ushort = builtin_scalar(unsigned short);

struct type t_int  = builtin_scalar(int);
struct type t_sint = builtin_scalar(signed int);
struct type t_uint = builtin_scalar(unsigned int);

struct type t_long  = builtin_scalar(long);
struct type t_slong = builtin_scalar(signed long);
struct type t_ulong = builtin_scalar(unsigned long);

struct type t_llong  = builtin_scalar(long long);
struct type t_sllong = builtin_scalar(signed long long);
struct type t_ullong = builtin_scalar(unsigned long long);

#pragma GCC diagnostic pop

static struct tfield f_2args[] = {
    { .type = &t_void },
    { .type = &t_void },
    { .type = NULL }
};

struct type t_binop_func = {
    .stype = true,
    .ttype = T_FUNC,
    .func = { .type = &t_void, .args = f_2args },
};

static struct tfield f_1arg[] = {
    { .type = &t_void },
    { .type = NULL }
};

struct type t_unary_func = {
    .stype = true,
    .ttype = T_FUNC,
    .func = { .type = &t_void, .args = f_1arg },
};

struct type t_vargs_func = {
    .stype = true,
    .ttype = T_FUNC,
    .func = { .type = &t_void, .vargs = 1 },
};

struct type t_buffer = {
    .stype = true,
    .ttype = T_MAP,
    .map = {
        .mtype = BPF_MAP_TYPE_PERF_EVENT_ARRAY,
        .ktype = &t_u32,
        .vtype = &t_int,
    },
};

struct type *builtin_types[] = {
    &t_void,
    &t_char,  &t_schar,  &t_uchar,
    &t_short, &t_sshort, &t_ushort,
    &t_int,   &t_sint,   &t_uint,
    &t_long,  &t_slong,  &t_ulong,
    &t_llong, &t_sllong, &t_ullong,
    &t_binop_func, &t_unary_func, &t_vargs_func,
    &t_buffer,
    NULL
};

__attribute__((constructor))
static void type_init(void)
{
    int ncpus;

    /* perf event array maps's length has to equal the number of
     * CPUs, which we won't know until we're actually running. */
    ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    assert(ncpus > 0);
    t_buffer.map.len = ncpus;

    type_add_list(builtin_types);

    printxf_default.vfprintxf['T'] = type_vfprintxf;
}


__attribute__((destructor))
static void all_types_free(void)
{
    struct type **ti;
    types_foreach(ti) {
        type_free(*ti);
    }
    free(all_types.types);
    all_types.len = 0;
}
