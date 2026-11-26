/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín       */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

/* Do not automatically get the thread specific Perl interpreter. */
#define PERL_NO_GET_CONTEXT
/* Do not use the GCC brace groups, which are not ISO C, in the perl macros. */
#define PERL_GCC_BRACE_GROUPS_FORBIDDEN

/* The perl headers have code that triggers these warnings. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wcast-qual"
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#pragma GCC diagnostic pop

/* Some versions of Perl define their own version of DEBUG... :-/ */
#ifdef DEBUG
#undef DEBUG
#endif

/* ... while we want the definition found in plugin.h. */
#include "plugin.h"

#if !defined(USE_ITHREADS)
#error "Perl does not support ithreads!"
#endif

#if IVSIZE < 8
#error "The perl plugin requires a perl with 64 bit integers."
#endif

#define CPL_PACKAGE                      "NCollectd"
#define CPL_CLASS_ERROR                  "NCollectd::Error"
#define CPL_CLASS_CONFIG                 "NCollectd::Config"
#define CPL_CLASS_METRIC                 "NCollectd::Metric"
#define CPL_CLASS_METRIC_UNKNOWN_DOUBLE  "NCollectd::MetricUnknownDouble"
#define CPL_CLASS_METRIC_UNKNOWN_LONG    "NCollectd::MetricUnknownLong"
#define CPL_CLASS_METRIC_GAUGE_DOUBLE    "NCollectd::MetricGaugeDouble"
#define CPL_CLASS_METRIC_GAUGE_LONG      "NCollectd::MetricGaugeLong"
#define CPL_CLASS_METRIC_COUNTER_ULONG   "NCollectd::MetricCounterULong"
#define CPL_CLASS_METRIC_COUNTER_DOUBLE  "NCollectd::MetricCounterDouble"
#define CPL_CLASS_METRIC_STATE_SET       "NCollectd::MetricStateSet"
#define CPL_CLASS_METRIC_INFO            "NCollectd::MetricInfo"
#define CPL_CLASS_METRIC_SUMMARY         "NCollectd::MetricSummary"
#define CPL_CLASS_METRIC_HISTOGRAM       "NCollectd::MetricHistogram"
#define CPL_CLASS_METRIC_GAUGE_HISTOGRAM "NCollectd::MetricGaugeHistogram"
#define CPL_CLASS_METRIC_FAMILY          "NCollectd::MetricFamily"
#define CPL_CLASS_NOTIFICATION           "NCollectd::Notification"

/* The objects of the NCollectd classes are blessed hash references, each
 * attribute (member) is a key of the hash. The classes are described with the
 * following structures, which play the role of PyMemberDef/PyTypeObject in the
 * python plugin: the constructor, the accessors and the string conversion of
 * every class are generic XSUBs driven by these descriptions. */

typedef enum {
    CPL_MEMBER_ANY,
    CPL_MEMBER_STRING,
    CPL_MEMBER_DOUBLE,
    CPL_MEMBER_INT64,
    CPL_MEMBER_UINT64,
    CPL_MEMBER_HASH,
    CPL_MEMBER_ARRAY,
    CPL_MEMBER_METRIC_TYPE,
    CPL_MEMBER_SEVERITY,
} cpl_member_type_t;

/* The member must be given to the constructor and can not be undef. */
#define CPL_MEMBER_REQUIRED     0x01
/* The accessor of the member can not modify it. */
#define CPL_MEMBER_READONLY     0x02
/* The member is not shown when the object is converted to a string. */
#define CPL_MEMBER_NOREPR       0x04
/* The member is not shown when the object is converted to a string if it is zero. */
#define CPL_MEMBER_REPR_NONZERO 0x08

typedef struct {
    const char *name;
    cpl_member_type_t type;
    int flags;
} cpl_member_t;

typedef struct cpl_class_s cpl_class_t;
struct cpl_class_s {
    const char *name;
    const char *base;
    /* Arguments of the constructor, used in the error messages. */
    const char *usage;
    /* Members of the class in the order of the constructor arguments. */
    const cpl_member_t *members;
    size_t members_num;
    /* Accessors are created in this class for the first own_members members,
     * the remaining ones are inherited from the base class. */
    size_t own_members;
    /* Conversion to string, NULL for the generic one. */
    SV *(*to_string)(pTHX_ SV *self);
};

void cpl_class_boot(pTHX_ const cpl_class_t *class);

const cpl_class_t *cpl_class_find(pTHX_ SV *sv);

SV *cpl_object_new(pTHX_ const char *class, HV **ret_hv);

HV *cpl_object_hv(pTHX_ SV *sv, const char *class);

SV *cpl_object_get(pTHX_ HV *hv, const char *key);

void cpl_object_set(pTHX_ HV *hv, const char *key, SV *value);

SV *cpl_object_to_string(pTHX_ SV *sv);

double cpl_sv_to_double(pTHX_ SV *sv, const char *what);

int64_t cpl_sv_to_int64(pTHX_ SV *sv, const char *what);

uint64_t cpl_sv_to_uint64(pTHX_ SV *sv, const char *what);

const char *cpl_sv_to_string(pTHX_ SV *sv, const char *what);

bool cpl_metric_type_valid(int64_t type);

const char *cpl_metric_type_name(metric_type_t type);

bool cpl_severity_valid(int64_t severity);

SV *cpl_labels_to_sv(pTHX_ const label_set_t *labels);

void cpl_build_labels(pTHX_ SV *sv, label_set_t *labels, const char *what);

void cpl_build_state_set(pTHX_ SV *sv, state_set_t *set);

void cpl_build_histogram(pTHX_ SV *sv, histogram_t **ret);

void cpl_build_summary(pTHX_ SV *sv, summary_t **ret);

void cpl_config_boot(pTHX);

SV *cpl_config_to_sv(pTHX_ const config_item_t *ci, SV *parent);

void cpl_metric_boot(pTHX);

SV *cpl_metric_to_sv(pTHX_ metric_type_t type, const metric_t *m);

bool cpl_metric_check_type(pTHX_ metric_type_t type, SV *sv);

void cpl_metric_build(pTHX_ metric_type_t type, SV *sv, metric_t *m);

void cpl_metric_family_boot(pTHX);

SV *cpl_metric_family_to_sv(pTHX_ const metric_family_t *fam);

void cpl_notification_boot(pTHX);

SV *cpl_notification_to_sv(pTHX_ const notification_t *n);
