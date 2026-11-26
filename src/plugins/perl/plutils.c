// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "cperl.h"

#include "libutils/common.h"
#include "libutils/itoa.h"
#include "libutils/dtoa.h"


#define POW_2_63 9223372036854775808.0
#define POW_2_64 18446744073709551616.0

#define CPL_REPR_MAX_DEPTH 16

typedef enum {
    CPL_NUMBER_INT,
    CPL_NUMBER_UINT,
    CPL_NUMBER_DOUBLE,
} cpl_number_kind_t;

typedef struct {
    cpl_number_kind_t kind;
    union {
        int64_t i;
        uint64_t u;
        double d;
    };
} cpl_number_t;

static const cpl_class_t *cpl_classes[32];
static size_t cpl_classes_num;

static bool cpl_sv_to_number(pTHX_ SV *sv, cpl_number_t *num)
{
    SvGETMAGIC(sv);

    if (!SvOK(sv) || SvROK(sv))
        return false;

    if (SvIOK(sv)) {
        if (SvIsUV(sv)) {
            num->kind = CPL_NUMBER_UINT;
            num->u = SvUVX(sv);
        } else {
            num->kind = CPL_NUMBER_INT;
            num->i = SvIVX(sv);
        }
        return true;
    }

    if (SvNOK(sv)) {
        num->kind = CPL_NUMBER_DOUBLE;
        num->d = SvNVX(sv);
        return true;
    }

    if (SvPOKp(sv)) {
        STRLEN len = 0;
        const char *str = SvPV_nomg_const(sv, len);
        UV value = 0;
        int flags = grok_number(str, len, &value);
        if ((flags == 0) || (flags & IS_NUMBER_TRAILING))
            return false;

        if ((flags & IS_NUMBER_IN_UV) && !(flags & IS_NUMBER_NOT_INT)) {
            if (!(flags & IS_NUMBER_NEG)) {
                num->kind = CPL_NUMBER_UINT;
                num->u = value;
                return true;
            }
            if (value <= ((UV)INT64_MAX + 1)) {
                num->kind = CPL_NUMBER_INT;
                num->i = value == ((UV)INT64_MAX + 1) ? INT64_MIN : -(int64_t)value;
                return true;
            }
        }

        num->kind = CPL_NUMBER_DOUBLE;
        num->d = SvNV_nomg(sv);
        return true;
    }

    if (SvIOKp(sv)) {
        num->kind = CPL_NUMBER_INT;
        num->i = SvIV_nomg(sv);
        return true;
    }

    if (SvNOKp(sv)) {
        num->kind = CPL_NUMBER_DOUBLE;
        num->d = SvNV_nomg(sv);
        return true;
    }

    return false;
}

double cpl_sv_to_double(pTHX_ SV *sv, const char *what)
{
    cpl_number_t num = {0};
    if (!cpl_sv_to_number(aTHX_ sv, &num))
        croak("%s must be a number", what);

    switch (num.kind) {
    case CPL_NUMBER_INT:
        return (double)num.i;
    case CPL_NUMBER_UINT:
        return (double)num.u;
    case CPL_NUMBER_DOUBLE:
        break;
    }

    return num.d;
}

int64_t cpl_sv_to_int64(pTHX_ SV *sv, const char *what)
{
    cpl_number_t num = {0};
    if (!cpl_sv_to_number(aTHX_ sv, &num))
        croak("%s must be a number", what);

    switch (num.kind) {
    case CPL_NUMBER_INT:
        return num.i;
    case CPL_NUMBER_UINT:
        if (num.u > (uint64_t)INT64_MAX)
            croak("%s must be a number between -2^63 and 2^63", what);
        return (int64_t)num.u;
    case CPL_NUMBER_DOUBLE:
        break;
    }

    /* Also false for NaN. */
    if (!((num.d >= -POW_2_63) && (num.d < POW_2_63)))
        croak("%s must be a number between -2^63 and 2^63", what);

    return (int64_t)num.d;
}

uint64_t cpl_sv_to_uint64(pTHX_ SV *sv, const char *what)
{
    cpl_number_t num = {0};
    if (!cpl_sv_to_number(aTHX_ sv, &num))
        croak("%s must be a number", what);

    switch (num.kind) {
    case CPL_NUMBER_INT:
        if (num.i < 0)
            croak("%s must be a number between 0 and 2^64", what);
        return (uint64_t)num.i;
    case CPL_NUMBER_UINT:
        return num.u;
    case CPL_NUMBER_DOUBLE:
        break;
    }

    /* Also false for NaN. */
    if (!((num.d >= 0.0) && (num.d < POW_2_64)))
        croak("%s must be a number between 0 and 2^64", what);

    return (uint64_t)num.d;
}

const char *cpl_sv_to_string(pTHX_ SV *sv, const char *what)
{
    SvGETMAGIC(sv);

    if (!SvOK(sv))
        croak("%s must be a string", what);

    if (SvROK(sv) && !SvAMAGIC(sv))
        croak("%s must be a string, not a reference", what);

    STRLEN len = 0;
    const char *str = SvPV_nomg_const(sv, len);
    if (memchr(str, '\0', len) != NULL)
        croak("%s must not contain NUL characters", what);

    return str;
}

bool cpl_metric_type_valid(int64_t type)
{
    switch (type) {
    case METRIC_TYPE_UNKNOWN:
    case METRIC_TYPE_GAUGE:
    case METRIC_TYPE_COUNTER:
    case METRIC_TYPE_STATE_SET:
    case METRIC_TYPE_INFO:
    case METRIC_TYPE_SUMMARY:
    case METRIC_TYPE_HISTOGRAM:
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        return true;
    }
    return false;
}

const char *cpl_metric_type_name(metric_type_t type)
{
    switch (type) {
    case METRIC_TYPE_UNKNOWN:
        return "METRIC_UNKNOWN";
    case METRIC_TYPE_GAUGE:
        return "METRIC_GAUGE";
    case METRIC_TYPE_COUNTER:
        return "METRIC_COUNTER";
    case METRIC_TYPE_STATE_SET:
        return "METRIC_STATE_SET";
    case METRIC_TYPE_INFO:
        return "METRIC_INFO";
    case METRIC_TYPE_SUMMARY:
        return "METRIC_SUMMARY";
    case METRIC_TYPE_HISTOGRAM:
        return "METRIC_HISTOGRAM";
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        return "METRIC_GAUGE_HISTOGRAM";
    }
    return "METRIC_INVALID";
}

bool cpl_severity_valid(int64_t severity)
{
    switch (severity) {
    case NOTIF_FAILURE:
    case NOTIF_WARNING:
    case NOTIF_OKAY:
        return true;
    }
    return false;
}

SV *cpl_labels_to_sv(pTHX_ const label_set_t *labels)
{
    HV *hv = newHV();

    for (size_t i = 0; i < labels->num; i++) {
        const label_pair_t *pair = &labels->ptr[i];
        (void)hv_store(hv, pair->name, (I32)strlen(pair->name), newSVpv(pair->value, 0), 0);
    }

    return newRV_noinc((SV *)hv);
}

static HV *cpl_sv_to_hv(pTHX_ SV *sv, const char *what)
{
    SvGETMAGIC(sv);
    if (!SvROK(sv) || (SvTYPE(SvRV(sv)) != SVt_PVHV))
        croak("%s must be a hash reference", what);
    return (HV *)SvRV(sv);
}

static AV *cpl_sv_to_av(pTHX_ SV *sv, const char *what)
{
    SvGETMAGIC(sv);
    if (!SvROK(sv) || (SvTYPE(SvRV(sv)) != SVt_PVAV))
        croak("%s must be an array reference", what);
    return (AV *)SvRV(sv);
}

/* Labels are a hash of numbers, strings or booleans. Like in the python plugin
 * the values are converted to strings, undefined values are ignored. */
void cpl_build_labels(pTHX_ SV *sv, label_set_t *labels, const char *what)
{
    if (sv == NULL)
        return;

    SvGETMAGIC(sv);
    if (!SvOK(sv))
        return;

    HV *hv = cpl_sv_to_hv(aTHX_ sv, what);

    hv_iterinit(hv);
    HE *he;
    while ((he = hv_iternext(hv)) != NULL) {
        STRLEN name_len = 0;
        const char *name = HePV(he, name_len);
        SV *value = hv_iterval(hv, he);

        SvGETMAGIC(value);
        if (!SvOK(value))
            continue;

        char buffer[DTOA_MAX > ITOA_MAX ? DTOA_MAX : ITOA_MAX];
        const char *str = NULL;
        STRLEN str_len = 0;

#ifdef SvIsBOOL
        if (SvIsBOOL(value)) {
            str = SvTRUE_nomg(value) ? "true" : "false";
            str_len = strlen(str);
        } else
#endif
        if (!SvPOK(value) && SvIOK(value)) {
            if (SvIsUV(value))
                str_len = uitoa(SvUVX(value), buffer);
            else
                str_len = itoa(SvIVX(value), buffer);
            str = buffer;
        } else if (!SvPOK(value) && SvNOK(value)) {
            str_len = dtoa(SvNVX(value), buffer, sizeof(buffer));
            str = buffer;
        } else {
            str = SvPV_nomg_const(value, str_len);
        }

        if ((memchr(name, '\0', name_len) != NULL) || (memchr(str, '\0', str_len) != NULL))
            croak("the names and values of %s must not contain NUL characters", what);

        int status = _label_set_add(labels, true, false, name, name_len, str, str_len);
        if (status != 0)
            croak("unable to add '%s' to %s", name, what);
    }
}

void cpl_build_state_set(pTHX_ SV *sv, state_set_t *set)
{
    if (sv == NULL)
        return;

    SvGETMAGIC(sv);
    if (!SvOK(sv))
        return;

    HV *hv = cpl_sv_to_hv(aTHX_ sv, "set");

    hv_iterinit(hv);
    HE *he;
    while ((he = hv_iternext(hv)) != NULL) {
        STRLEN name_len = 0;
        const char *name = HePV(he, name_len);
        SV *value = hv_iterval(hv, he);

        if (memchr(name, '\0', name_len) != NULL)
            croak("the state names must not contain NUL characters");

        int status = state_set_add(set, name, SvTRUE(value));
        if (status != 0)
            croak("unable to add the state '%s' to the set", name);
    }
}

static void cpl_pair(pTHX_ AV *av, SSize_t idx, const char *what, SV **first, SV **second)
{
    SV **item = av_fetch(av, idx, 0);
    if ((item == NULL) || !SvROK(*item) || (SvTYPE(SvRV(*item)) != SVt_PVAV))
        croak("%s %zd must be an array reference with two elements", what, idx);

    AV *pair = (AV *)SvRV(*item);
    if (av_top_index(pair) != 1)
        croak("%s %zd must be an array reference with two elements", what, idx);

    SV **a = av_fetch(pair, 0, 0);
    SV **b = av_fetch(pair, 1, 0);
    *first = (a != NULL) ? *a : &PL_sv_undef;
    *second = (b != NULL) ? *b : &PL_sv_undef;
}

/* Buckets are an array of [counter, maximum] pairs, from the bucket with the
 * highest maximum (+Inf) to the lowest one. The histogram is stored in "ret"
 * before converting the buckets so the caller can free it if this croaks. */
void cpl_build_histogram(pTHX_ SV *sv, histogram_t **ret)
{
    AV *av = cpl_sv_to_av(aTHX_ sv, "buckets");
    SSize_t num = av_top_index(av) + 1;

    histogram_t *h = calloc(1, sizeof(*h) + sizeof(h->buckets[0]) * (size_t)num);
    if (h == NULL)
        croak("calloc failed");
    *ret = h;

    h->num = (size_t)num;
    for (SSize_t i = 0; i < num; i++) {
        SV *counter = NULL;
        SV *maximum = NULL;
        cpl_pair(aTHX_ av, i, "bucket", &counter, &maximum);
        h->buckets[i].counter = cpl_sv_to_uint64(aTHX_ counter, "bucket counter");
        h->buckets[i].maximum = cpl_sv_to_double(aTHX_ maximum, "bucket maximum");
    }
}

/* Quantiles are an array of [quantile, value] pairs. */
void cpl_build_summary(pTHX_ SV *sv, summary_t **ret)
{
    AV *av = cpl_sv_to_av(aTHX_ sv, "quantiles");
    SSize_t num = av_top_index(av) + 1;

    summary_t *s = calloc(1, sizeof(*s) + sizeof(s->quantiles[0]) * (size_t)num);
    if (s == NULL)
        croak("calloc failed");
    *ret = s;

    s->num = (size_t)num;
    for (SSize_t i = 0; i < num; i++) {
        SV *quantile = NULL;
        SV *value = NULL;
        cpl_pair(aTHX_ av, i, "quantile", &quantile, &value);
        s->quantiles[i].quantile = cpl_sv_to_double(aTHX_ quantile, "quantile");
        s->quantiles[i].value = cpl_sv_to_double(aTHX_ value, "quantile value");
    }
}

SV *cpl_object_new(pTHX_ const char *class, HV **ret_hv)
{
    HV *hv = newHV();
    SV *self = newRV_noinc((SV *)hv);
    sv_bless(self, gv_stashpv(class, GV_ADD));

    if (ret_hv != NULL)
        *ret_hv = hv;

    return self;
}

HV *cpl_object_hv(pTHX_ SV *sv, const char *class)
{
    if ((sv == NULL) || !sv_isobject(sv) || (SvTYPE(SvRV(sv)) != SVt_PVHV) ||
        ((class != NULL) && !sv_derived_from(sv, class)))
        croak("Expected a %s object", class == NULL ? CPL_PACKAGE : class);

    return (HV *)SvRV(sv);
}

/* Returns the member "key" of the object, or NULL if it does not exist or is undef. */
SV *cpl_object_get(pTHX_ HV *hv, const char *key)
{
    SV **svp = hv_fetch(hv, key, (I32)strlen(key), 0);
    if (svp == NULL)
        return NULL;

    SvGETMAGIC(*svp);
    if (!SvOK(*svp))
        return NULL;

    return *svp;
}

/* Stores "value" in the member "key" of the object, taking its reference. */
void cpl_object_set(pTHX_ HV *hv, const char *key, SV *value)
{
    if (hv_store(hv, key, (I32)strlen(key), value, 0) == NULL)
        SvREFCNT_dec(value);
}

/* Converts the value given to the constructor or to an accessor to the type
 * of the member, returns a new SV. */
static SV *cpl_member_value(pTHX_ const cpl_member_t *member, SV *sv)
{
    if (sv != NULL)
        SvGETMAGIC(sv);

    if ((sv == NULL) || !SvOK(sv)) {
        if (member->flags & CPL_MEMBER_REQUIRED)
            croak("%s is required", member->name);

        switch (member->type) {
        case CPL_MEMBER_HASH:
            return newRV_noinc((SV *)newHV());
        case CPL_MEMBER_ARRAY:
            return newRV_noinc((SV *)newAV());
        case CPL_MEMBER_DOUBLE:
            return newSVnv(0.0);
        case CPL_MEMBER_INT64:
            return newSViv(0);
        case CPL_MEMBER_UINT64:
            return newSVuv(0);
        case CPL_MEMBER_METRIC_TYPE:
            return newSViv(METRIC_TYPE_UNKNOWN);
        case CPL_MEMBER_SEVERITY:
            return newSViv(NOTIF_FAILURE);
        case CPL_MEMBER_STRING:
        case CPL_MEMBER_ANY:
            break;
        }
        return newSV(0);
    }

    switch (member->type) {
    case CPL_MEMBER_ANY:
        return newSVsv(sv);
    case CPL_MEMBER_STRING: {
        (void)cpl_sv_to_string(aTHX_ sv, member->name);
        SV *str = newSV(0);
        sv_copypv_nomg(str, sv);
        return str;
    }
    case CPL_MEMBER_DOUBLE:
        return newSVnv(cpl_sv_to_double(aTHX_ sv, member->name));
    case CPL_MEMBER_INT64:
        return newSViv((IV)cpl_sv_to_int64(aTHX_ sv, member->name));
    case CPL_MEMBER_UINT64:
        return newSVuv((UV)cpl_sv_to_uint64(aTHX_ sv, member->name));
    case CPL_MEMBER_HASH:
        (void)cpl_sv_to_hv(aTHX_ sv, member->name);
        return newSVsv(sv);
    case CPL_MEMBER_ARRAY:
        (void)cpl_sv_to_av(aTHX_ sv, member->name);
        return newSVsv(sv);
    case CPL_MEMBER_METRIC_TYPE: {
        int64_t type = cpl_sv_to_int64(aTHX_ sv, member->name);
        if (!cpl_metric_type_valid(type))
            croak("invalid metric type: %" PRId64, type);
        return newSViv((IV)type);
    }
    case CPL_MEMBER_SEVERITY: {
        int64_t severity = cpl_sv_to_int64(aTHX_ sv, member->name);
        if (!cpl_severity_valid(severity))
            croak("invalid severity: %" PRId64, severity);
        return newSViv((IV)severity);
    }
    }

    return newSVsv(sv);
}

/* Class->new(...): the arguments are the members of the class in order. */
static XS(cpl_xs_object_new)
{
    dXSARGS;
    const cpl_class_t *class = XSANY.any_ptr;

    if (items < 1)
        croak_xs_usage(cv, "class, ...");

    HV *stash = NULL;
    if (sv_isobject(ST(0)))
        stash = SvSTASH(SvRV(ST(0)));
    else
        stash = gv_stashsv(ST(0), GV_ADD);

    size_t required = 0;
    for (size_t i = 0; i < class->members_num; i++) {
        if (class->members[i].flags & CPL_MEMBER_REQUIRED)
            required++;
    }

    size_t argc = (size_t)items - 1;
    if ((argc < required) || (argc > class->members_num))
        croak("Usage: %s->new(%s)", class->name, class->usage);

    HV *hv = newHV();
    SV *self = sv_2mortal(newRV_noinc((SV *)hv));
    sv_bless(self, stash);

    for (size_t i = 0; i < class->members_num; i++) {
        const cpl_member_t *member = &class->members[i];
        SV *value = cpl_member_value(aTHX_ member, i < argc ? ST(i + 1) : NULL);
        cpl_object_set(aTHX_ hv, member->name, value);
    }

    ST(0) = self;
    XSRETURN(1);
}

/* $object->member() returns the member, $object->member($value) sets it. */
static XS(cpl_xs_object_member)
{
    dXSARGS;
    const cpl_member_t *member = XSANY.any_ptr;

    if ((items < 1) || (items > 2))
        croak_xs_usage(cv, "self, [value]");

    HV *hv = cpl_object_hv(aTHX_ ST(0), NULL);
    I32 len = (I32)strlen(member->name);

    if (items == 1) {
        SV **svp = hv_fetch(hv, member->name, len, 0);
        ST(0) = (svp != NULL) ? sv_mortalcopy(*svp) : &PL_sv_undef;
        XSRETURN(1);
    }

    if (member->flags & CPL_MEMBER_READONLY)
        croak("%s is a read-only member", member->name);

    SV *value = cpl_member_value(aTHX_ member, ST(1));
    cpl_object_set(aTHX_ hv, member->name, value);

    ST(0) = sv_mortalcopy(value);
    XSRETURN(1);
}

static void cpl_class_register(const cpl_class_t *class)
{
    for (size_t i = 0; i < cpl_classes_num; i++) {
        if (cpl_classes[i] == class)
            return;
    }

    if (cpl_classes_num < STATIC_ARRAY_SIZE(cpl_classes))
        cpl_classes[cpl_classes_num++] = class;
}

static const cpl_class_t *cpl_class_by_name(const char *name)
{
    for (size_t i = 0; i < cpl_classes_num; i++) {
        if (strcmp(cpl_classes[i]->name, name) == 0)
            return cpl_classes[i];
    }
    return NULL;
}

static bool cpl_class_isa(const cpl_class_t *class, const cpl_class_t *base)
{
    while (class != NULL) {
        if (class == base)
            return true;
        class = (class->base == NULL) ? NULL : cpl_class_by_name(class->base);
    }
    return false;
}

/* Returns the most specific NCollectd class of the object. */
const cpl_class_t *cpl_class_find(pTHX_ SV *sv)
{
    if (!sv_isobject(sv))
        return NULL;

    const char *name = HvNAME(SvSTASH(SvRV(sv)));
    if (name != NULL) {
        const cpl_class_t *class = cpl_class_by_name(name);
        if (class != NULL)
            return class;
    }

    /* A class derived in perl from one of the NCollectd classes. */
    const cpl_class_t *found = NULL;
    for (size_t i = 0; i < cpl_classes_num; i++) {
        if (!sv_derived_from(sv, cpl_classes[i]->name))
            continue;
        if ((found == NULL) || cpl_class_isa(cpl_classes[i], found))
            found = cpl_classes[i];
    }

    return found;
}

static void cpl_repr_value(pTHX_ SV *out, SV *value, int depth);

static void cpl_repr_string(pTHX_ SV *out, SV *value)
{
    STRLEN len = 0;
    const char *str = SvPV_nomg_const(value, len);
    I32 flags = SvUTF8(value) ? SV_CATUTF8 : SV_CATBYTES;

    sv_catpvs(out, "\"");

    const char *start = str;
    for (const char *ptr = str; ptr < (str + len); ptr++) {
        char hex[8];
        const char *escape = NULL;
        switch (*ptr) {
        case '"':
            escape = "\\\"";
            break;
        case '\\':
            escape = "\\\\";
            break;
        case '\n':
            escape = "\\n";
            break;
        case '\r':
            escape = "\\r";
            break;
        case '\t':
            escape = "\\t";
            break;
        default:
            if (((unsigned char)*ptr < 0x20) || ((unsigned char)*ptr == 0x7f)) {
                ssnprintf(hex, sizeof(hex), "\\x%02x", (unsigned int)(unsigned char)*ptr);
                escape = hex;
            }
            break;
        }

        if (escape == NULL)
            continue;

        if (ptr > start)
            sv_catpvn_flags(out, start, (STRLEN)(ptr - start), flags);
        sv_catpv(out, escape);
        start = ptr + 1;
    }

    if ((str + len) > start)
        sv_catpvn_flags(out, start, (STRLEN)((str + len) - start), flags);

    sv_catpvs(out, "\"");
}

static void cpl_repr_number(pTHX_ SV *out, SV *value)
{
    if (SvIOK(value)) {
        if (SvIsUV(value))
            sv_catpvf(out, "%" UVuf, SvUVX(value));
        else
            sv_catpvf(out, "%" IVdf, SvIVX(value));
        return;
    }

    char buffer[DTOA_MAX];
    dtoa(SvNV_nomg(value), buffer, sizeof(buffer));
    sv_catpv(out, buffer);
}

static bool cpl_repr_is_identifier(const char *str, STRLEN len)
{
    if ((len == 0) || isDIGIT(str[0]))
        return false;

    for (STRLEN i = 0; i < len; i++) {
        if (!isWORDCHAR_A(str[i]))
            return false;
    }

    return true;
}

static void cpl_repr_hash(pTHX_ SV *out, HV *hv, int depth)
{
    AV *keys = (AV *)sv_2mortal((SV *)newAV());

    hv_iterinit(hv);
    HE *he;
    while ((he = hv_iternext(hv)) != NULL)
        av_push(keys, newSVsv(hv_iterkeysv(he)));

    SSize_t num = av_top_index(keys) + 1;
    if (num > 1)
        sortsv(AvARRAY(keys), (size_t)num, Perl_sv_cmp);

    sv_catpvs(out, "{");
    for (SSize_t i = 0; i < num; i++) {
        SV *key = AvARRAY(keys)[i];

        if (i > 0)
            sv_catpvs(out, ", ");

        STRLEN len = 0;
        const char *str = SvPV_const(key, len);
        if (cpl_repr_is_identifier(str, len))
            sv_catsv(out, key);
        else
            cpl_repr_string(aTHX_ out, key);

        sv_catpvs(out, " => ");

        HE *entry = hv_fetch_ent(hv, key, 0, 0);
        cpl_repr_value(aTHX_ out, entry != NULL ? HeVAL(entry) : &PL_sv_undef, depth + 1);
    }
    sv_catpvs(out, "}");
}

static void cpl_repr_array(pTHX_ SV *out, AV *av, int depth)
{
    SSize_t num = av_top_index(av) + 1;

    sv_catpvs(out, "[");
    for (SSize_t i = 0; i < num; i++) {
        if (i > 0)
            sv_catpvs(out, ", ");
        SV **item = av_fetch(av, i, 0);
        cpl_repr_value(aTHX_ out, item != NULL ? *item : &PL_sv_undef, depth + 1);
    }
    sv_catpvs(out, "]");
}

static bool cpl_repr_is_empty(pTHX_ SV *value)
{
    if (!SvROK(value) || sv_isobject(value))
        return false;

    SV *ref = SvRV(value);
    if (SvTYPE(ref) == SVt_PVHV)
        return HvUSEDKEYS((HV *)ref) == 0;
    if (SvTYPE(ref) == SVt_PVAV)
        return av_top_index((AV *)ref) < 0;

    return false;
}

/* Like __repr__ in the python plugin: Class(member => value, ...) */
static void cpl_repr_object(pTHX_ SV *out, SV *self, const cpl_class_t *class, int depth)
{
    HV *hv = (HV *)SvRV(self);
    const char *name = HvNAME(SvSTASH((SV *)hv));

    sv_catpv(out, name != NULL ? name : class->name);
    sv_catpvs(out, "(");

    bool first = true;
    for (size_t i = 0; i < class->members_num; i++) {
        const cpl_member_t *member = &class->members[i];
        if (member->flags & CPL_MEMBER_NOREPR)
            continue;

        SV **svp = hv_fetch(hv, member->name, (I32)strlen(member->name), 0);
        if ((svp == NULL) || !SvOK(*svp) || cpl_repr_is_empty(aTHX_ *svp))
            continue;

        if ((member->flags & CPL_MEMBER_REPR_NONZERO) && !SvROK(*svp) && (SvNV(*svp) == 0.0))
            continue;

        if (!first)
            sv_catpvs(out, ", ");
        first = false;

        sv_catpv(out, member->name);
        sv_catpvs(out, " => ");
        cpl_repr_value(aTHX_ out, *svp, depth + 1);
    }

    sv_catpvs(out, ")");
}

static void cpl_repr_value(pTHX_ SV *out, SV *value, int depth)
{
    if (depth > CPL_REPR_MAX_DEPTH) {
        sv_catpvs(out, "...");
        return;
    }

    SvGETMAGIC(value);

    if (!SvOK(value)) {
        sv_catpvs(out, "undef");
        return;
    }

    if (SvROK(value)) {
        SV *ref = SvRV(value);
        if (SvOBJECT(ref)) {
            const cpl_class_t *class = cpl_class_find(aTHX_ value);
            if (class == NULL) {
                sv_catsv(out, value);
            } else if (SvTYPE(ref) != SVt_PVHV) {
                sv_catpvf(out, "%s=%s(0x%" UVxf ")", HvNAME(SvSTASH(ref)),
                          sv_reftype(ref, 0), PTR2UV(ref));
            } else if (class->to_string != NULL) {
                SV *str = class->to_string(aTHX_ value);
                sv_catsv(out, str);
                SvREFCNT_dec(str);
            } else {
                cpl_repr_object(aTHX_ out, value, class, depth);
            }
            return;
        }

        if (SvTYPE(ref) == SVt_PVHV)
            cpl_repr_hash(aTHX_ out, (HV *)ref, depth);
        else if (SvTYPE(ref) == SVt_PVAV)
            cpl_repr_array(aTHX_ out, (AV *)ref, depth);
        else
            sv_catsv(out, value);
        return;
    }

#ifdef SvIsBOOL
    if (SvIsBOOL(value)) {
        sv_catpv(out, SvTRUE_nomg(value) ? "true" : "false");
        return;
    }
#endif

    if (!SvPOK(value) && (SvIOK(value) || SvNOK(value))) {
        cpl_repr_number(aTHX_ out, value);
        return;
    }

    cpl_repr_string(aTHX_ out, value);
}

SV *cpl_object_to_string(pTHX_ SV *sv)
{
    SV *out = newSVpvs("");
    cpl_repr_value(aTHX_ out, sv, 0);
    return out;
}

/* The overloaded operators of the NCollectd classes. */

static XS(cpl_xs_object_nil)
{
    dXSARGS;
    PERL_UNUSED_VAR(items);
    XSRETURN_EMPTY;
}

static XS(cpl_xs_object_string)
{
    dXSARGS;
    if (items < 1)
        croak_xs_usage(cv, "self, ...");

    ST(0) = sv_2mortal(cpl_object_to_string(aTHX_ ST(0)));
    XSRETURN(1);
}

static XS(cpl_xs_object_bool)
{
    dXSARGS;
    PERL_UNUSED_VAR(items);
    XSRETURN_YES;
}

static XS(cpl_xs_object_number)
{
    dXSARGS;
    if ((items < 1) || !SvROK(ST(0)))
        croak_xs_usage(cv, "self, ...");

    ST(0) = sv_2mortal(newSVuv(PTR2UV(SvRV(ST(0)))));
    XSRETURN(1);
}

/* Creates the class in the perl interpreter: @ISA, the constructor, the
 * accessors and, for the base classes, the overloaded string conversion,
 * which is what "use overload" does in perl, but without the overload module. */
void cpl_class_boot(pTHX_ const cpl_class_t *class)
{
    static const char file[] = __FILE__;
    char name[256];

    cpl_class_register(class);

    if (class->base != NULL) {
        ssnprintf(name, sizeof(name), "%s::ISA", class->name);
        av_push(get_av(name, GV_ADD), newSVpv(class->base, 0));
    }

    ssnprintf(name, sizeof(name), "%s::new", class->name);
    CV *cv = newXS(name, cpl_xs_object_new, file);
    CvXSUBANY(cv).any_ptr = discard_const(class);

    for (size_t i = 0; i < class->own_members; i++) {
        ssnprintf(name, sizeof(name), "%s::%s", class->name, class->members[i].name);
        cv = newXS(name, cpl_xs_object_member, file);
        CvXSUBANY(cv).any_ptr = discard_const(&class->members[i]);
    }

    if (class->base != NULL)
        return;

    /* fallback => 1 */
    ssnprintf(name, sizeof(name), "%s::()", class->name);
    sv_setsv(get_sv(name, GV_ADD), &PL_sv_yes);
    newXS(name, cpl_xs_object_nil, file);

    ssnprintf(name, sizeof(name), "%s::(\"\"", class->name);
    newXS(name, cpl_xs_object_string, file);

    ssnprintf(name, sizeof(name), "%s::(bool", class->name);
    newXS(name, cpl_xs_object_bool, file);

    ssnprintf(name, sizeof(name), "%s::(0+", class->name);
    newXS(name, cpl_xs_object_number, file);
}
