// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "cperl.h"

#include "libutils/common.h"


static const cpl_member_t cpl_notification_members[] = {
    { "name",        CPL_MEMBER_STRING,   CPL_MEMBER_REQUIRED     },
    { "severity",    CPL_MEMBER_SEVERITY, 0                       },
    { "time",        CPL_MEMBER_DOUBLE,   CPL_MEMBER_REPR_NONZERO },
    { "labels",      CPL_MEMBER_HASH,     0                       },
    { "annotations", CPL_MEMBER_HASH,     0                       },
};

static const cpl_class_t cpl_notification_class = {
    .name        = CPL_CLASS_NOTIFICATION,
    .base        = NULL,
    .usage       = "name[, severity][, time][, labels][, annotations]",
    .members     = cpl_notification_members,
    .members_num = STATIC_ARRAY_SIZE(cpl_notification_members),
    .own_members = STATIC_ARRAY_SIZE(cpl_notification_members),
};

static void cpl_notification_reset(pTHX_ void *arg)
{
    notification_t *n = arg;
    label_set_reset(&n->label);
    label_set_reset(&n->annotation);
}

/* $notification->dispatch() */
XS_INTERNAL(cpl_xs_notification_dispatch)
{
    dXSARGS;

    if (items != 1)
        croak_xs_usage(cv, "self");

    HV *hv = cpl_object_hv(aTHX_ ST(0), CPL_CLASS_NOTIFICATION);

    SV *name = cpl_object_get(aTHX_ hv, "name");
    if (name == NULL)
        croak("name is required");

    notification_t n = {0};
    /* The name is not copied: plugin_dispatch_notification does it. */
    n.name = discard_const(cpl_sv_to_string(aTHX_ name, "name"));

    n.severity = NOTIF_FAILURE;
    SV *severity = cpl_object_get(aTHX_ hv, "severity");
    if (severity != NULL) {
        int64_t value = cpl_sv_to_int64(aTHX_ severity, "severity");
        if (!cpl_severity_valid(value))
            croak("invalid severity: %" PRId64, value);
        n.severity = (severity_t)value;
    }

    SV *time = cpl_object_get(aTHX_ hv, "time");
    if (time != NULL) {
        double value = cpl_sv_to_double(aTHX_ time, "time");
        if (value < 0)
            croak("time must be a positive number");
        n.time = DOUBLE_TO_CDTIME_T(value);
    }
    if (n.time == 0)
        n.time = cdtime();

    ENTER;
    /* Free the labels if any conversion croaks. */
    SAVEDESTRUCTOR_X(cpl_notification_reset, &n);

    cpl_build_labels(aTHX_ cpl_object_get(aTHX_ hv, "labels"), &n.label, "labels");
    cpl_build_labels(aTHX_ cpl_object_get(aTHX_ hv, "annotations"), &n.annotation, "annotations");

    int status = plugin_dispatch_notification(&n);

    LEAVE;

    if (status != 0)
        croak("error dispatching notification, read the logs");

    XSRETURN_EMPTY;
}

void cpl_notification_boot(pTHX)
{
    static const char file[] = __FILE__;

    cpl_class_boot(aTHX_ &cpl_notification_class);

    newXS(CPL_CLASS_NOTIFICATION "::dispatch", cpl_xs_notification_dispatch, file);
}

SV *cpl_notification_to_sv(pTHX_ const notification_t *n)
{
    HV *hv = NULL;
    SV *self = cpl_object_new(aTHX_ CPL_CLASS_NOTIFICATION, &hv);

    cpl_object_set(aTHX_ hv, "name", n->name != NULL ? newSVpv(n->name, 0) : newSV(0));
    cpl_object_set(aTHX_ hv, "severity", newSViv(n->severity));
    cpl_object_set(aTHX_ hv, "time", newSVnv(CDTIME_T_TO_DOUBLE(n->time)));
    cpl_object_set(aTHX_ hv, "labels", cpl_labels_to_sv(aTHX_ &n->label));
    cpl_object_set(aTHX_ hv, "annotations", cpl_labels_to_sv(aTHX_ &n->annotation));

    return self;
}
