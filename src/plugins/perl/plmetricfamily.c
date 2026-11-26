// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "cperl.h"

#include "libutils/common.h"


static const cpl_member_t cpl_metric_family_members[] = {
    { "name",    CPL_MEMBER_STRING,      CPL_MEMBER_REQUIRED                       },
    { "type",    CPL_MEMBER_METRIC_TYPE, CPL_MEMBER_REQUIRED | CPL_MEMBER_READONLY },
    { "help",    CPL_MEMBER_STRING,      0                                         },
    { "unit",    CPL_MEMBER_STRING,      0                                         },
    { "metrics", CPL_MEMBER_ARRAY,       0                                         },
};

static const cpl_class_t cpl_metric_family_class = {
    .name        = CPL_CLASS_METRIC_FAMILY,
    .base        = NULL,
    .usage       = "name, type[, help][, unit][, metrics]",
    .members     = cpl_metric_family_members,
    .members_num = STATIC_ARRAY_SIZE(cpl_metric_family_members),
    .own_members = STATIC_ARRAY_SIZE(cpl_metric_family_members),
};

static metric_type_t cpl_metric_family_type(pTHX_ HV *hv)
{
    SV *sv = cpl_object_get(aTHX_ hv, "type");
    if (sv == NULL)
        croak("type is required");

    int64_t type = cpl_sv_to_int64(aTHX_ sv, "type");
    if (!cpl_metric_type_valid(type))
        croak("invalid metric type: %" PRId64, type);

    return (metric_type_t)type;
}

static AV *cpl_metric_family_metrics(pTHX_ HV *hv, bool create)
{
    SV *sv = cpl_object_get(aTHX_ hv, "metrics");
    if (sv != NULL) {
        if (!SvROK(sv) || (SvTYPE(SvRV(sv)) != SVt_PVAV))
            croak("metrics must be an array reference");
        return (AV *)SvRV(sv);
    }

    if (!create)
        return NULL;

    AV *av = newAV();
    cpl_object_set(aTHX_ hv, "metrics", newRV_noinc((SV *)av));
    return av;
}

static bool cpl_metric_family_is_list(SV *sv)
{
    return SvROK(sv) && !SvOBJECT(SvRV(sv)) && (SvTYPE(SvRV(sv)) == SVt_PVAV);
}

static void cpl_metric_family_check(pTHX_ metric_type_t type, SV *sv)
{
    if (cpl_metric_check_type(aTHX_ type, sv))
        return;

    const char *class = sv_isobject(sv) ? HvNAME(SvSTASH(SvRV(sv))) : NULL;
    croak("metric type mismatch: %s can not be added to a metric family of type %s",
          class != NULL ? class : "a value which is not a metric",
          cpl_metric_type_name(type));
}

/* $fam->append($metric | [$metric, ...], ...) */
XS_INTERNAL(cpl_xs_metric_family_append)
{
    dXSARGS;

    if (items < 1)
        croak_xs_usage(cv, "self, metrics...");

    HV *hv = cpl_object_hv(aTHX_ ST(0), CPL_CLASS_METRIC_FAMILY);
    metric_type_t type = cpl_metric_family_type(aTHX_ hv);

    /* Check all the metrics before appending any of them. */
    for (I32 i = 1; i < items; i++) {
        if (!cpl_metric_family_is_list(ST(i))) {
            cpl_metric_family_check(aTHX_ type, ST(i));
            continue;
        }

        AV *list = (AV *)SvRV(ST(i));
        for (SSize_t j = 0; j <= av_top_index(list); j++) {
            SV **item = av_fetch(list, j, 0);
            cpl_metric_family_check(aTHX_ type, item != NULL ? *item : &PL_sv_undef);
        }
    }

    AV *metrics = cpl_metric_family_metrics(aTHX_ hv, true);

    for (I32 i = 1; i < items; i++) {
        if (!cpl_metric_family_is_list(ST(i))) {
            av_push(metrics, newSVsv(ST(i)));
            continue;
        }

        AV *list = (AV *)SvRV(ST(i));
        for (SSize_t j = 0; j <= av_top_index(list); j++) {
            SV **item = av_fetch(list, j, 0);
            if (item != NULL)
                av_push(metrics, newSVsv(*item));
        }
    }

    XSRETURN_EMPTY;
}

static void cpl_metric_family_reset(pTHX_ void *arg)
{
    metric_family_t *fam = arg;
    metric_list_reset(&fam->metric, fam->type);
}

/* $fam->dispatch([time]) */
XS_INTERNAL(cpl_xs_metric_family_dispatch)
{
    dXSARGS;

    if ((items < 1) || (items > 2))
        croak_xs_usage(cv, "self, [time]");

    HV *hv = cpl_object_hv(aTHX_ ST(0), CPL_CLASS_METRIC_FAMILY);

    double time = 0;
    if ((items > 1) && SvOK(ST(1))) {
        time = cpl_sv_to_double(aTHX_ ST(1), "time");
        if (time < 0)
            croak("time must be greater than zero");
    }

    SV *name = cpl_object_get(aTHX_ hv, "name");
    if (name == NULL)
        croak("name is required");

    SV *help = cpl_object_get(aTHX_ hv, "help");
    SV *unit = cpl_object_get(aTHX_ hv, "unit");

    /* The strings are not copied: plugin_dispatch_metric_family does it. */
    metric_family_t fam = {0};
    fam.type = cpl_metric_family_type(aTHX_ hv);
    fam.name = discard_const(cpl_sv_to_string(aTHX_ name, "name"));
    if (help != NULL)
        fam.help = discard_const(cpl_sv_to_string(aTHX_ help, "help"));
    if (unit != NULL)
        fam.unit = discard_const(cpl_sv_to_string(aTHX_ unit, "unit"));

    AV *metrics = cpl_metric_family_metrics(aTHX_ hv, false);
    SSize_t num = (metrics == NULL) ? 0 : av_top_index(metrics) + 1;
    if (num == 0)
        XSRETURN_EMPTY;

    ENTER;
    /* Free the metrics if any conversion croaks. */
    SAVEDESTRUCTOR_X(cpl_metric_family_reset, &fam);

    fam.metric.ptr = calloc((size_t)num, sizeof(*fam.metric.ptr));
    if (fam.metric.ptr == NULL)
        croak("calloc failed");
    fam.metric.num = (size_t)num;

    for (SSize_t i = 0; i < num; i++) {
        SV **item = av_fetch(metrics, i, 0);
        cpl_metric_build(aTHX_ fam.type, item != NULL ? *item : &PL_sv_undef, &fam.metric.ptr[i]);
    }

    /* The dispatch takes the metrics from the family. */
    int status = plugin_dispatch_metric_family(&fam, DOUBLE_TO_CDTIME_T(time));

    /* Like in the python plugin the dispatched metrics are removed from the family. */
    cpl_object_set(aTHX_ hv, "metrics", newRV_noinc((SV *)newAV()));

    LEAVE;

    if (status != 0)
        croak("error dispatching metric family, read the logs");

    XSRETURN_EMPTY;
}

void cpl_metric_family_boot(pTHX)
{
    static const char file[] = __FILE__;

    cpl_class_boot(aTHX_ &cpl_metric_family_class);

    newXS(CPL_CLASS_METRIC_FAMILY "::append", cpl_xs_metric_family_append, file);
    newXS(CPL_CLASS_METRIC_FAMILY "::dispatch", cpl_xs_metric_family_dispatch, file);
}

SV *cpl_metric_family_to_sv(pTHX_ const metric_family_t *fam)
{
    HV *hv = NULL;
    SV *self = cpl_object_new(aTHX_ CPL_CLASS_METRIC_FAMILY, &hv);

    cpl_object_set(aTHX_ hv, "name", fam->name != NULL ? newSVpv(fam->name, 0) : newSV(0));
    cpl_object_set(aTHX_ hv, "type", newSViv(fam->type));
    cpl_object_set(aTHX_ hv, "help", fam->help != NULL ? newSVpv(fam->help, 0) : newSV(0));
    cpl_object_set(aTHX_ hv, "unit", fam->unit != NULL ? newSVpv(fam->unit, 0) : newSV(0));

    AV *metrics = newAV();
    if (fam->metric.num > 0)
        av_extend(metrics, (SSize_t)fam->metric.num - 1);

    for (size_t i = 0; i < fam->metric.num; i++)
        av_push(metrics, cpl_metric_to_sv(aTHX_ fam->type, &fam->metric.ptr[i]));

    cpl_object_set(aTHX_ hv, "metrics", newRV_noinc((SV *)metrics));

    return self;
}
