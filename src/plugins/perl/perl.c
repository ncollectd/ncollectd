// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2007-2009 Sebastian Harl
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Pavel Rochnyak <pavel2000 at ngs.ru>

/*
 * This plugin embeds a Perl interpreter into ncollectd and provides an
 * interface for ncollectd plugins written in perl.
 */

/* do not automatically get the thread specific Perl interpreter */
#define PERL_NO_GET_CONTEXT

#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wcast-qual"

#include <EXTERN.h>
#include <perl.h>

#include <XSUB.h>

/* Some versions of Perl define their own version of DEBUG... :-/ */
#ifdef DEBUG
#undef DEBUG
#endif

/* ... while we want the definition found in plugin.h. */
#include "plugin.h"
#include "libutils/common.h"
#include "libutils/itoa.h"
#include "libutils/dtoa.h"

#if !defined(USE_ITHREADS)
#error "Perl does not support ithreads!"
#endif

/* clear the Perl sub's stack frame
 * (this should only be used inside an XSUB) */
#define CLEAR_STACK_FRAME PL_stack_sp = PL_stack_base + *PL_markstack_ptr

#define PLUGIN_INIT      0
#define PLUGIN_READ      1
#define PLUGIN_WRITE     2
#define PLUGIN_SHUTDOWN  3
#define PLUGIN_LOG       4
#define PLUGIN_NOTIF     5
#define PLUGIN_FLUSH     6
#define PLUGIN_TYPES     8
#define PLUGIN_CONFIG    254

/* this is defined in DynaLoader.a */
void boot_DynaLoader(PerlInterpreter *, CV *);

static XS(ncollectd_plugin_register_read);
static XS(ncollectd_plugin_register_write);
static XS(ncollectd_plugin_register_log);
static XS(ncollectd_plugin_register_notification);
static XS(ncollectd_plugin_unregister_read);
static XS(ncollectd_plugin_unregister_write);
static XS(ncollectd_plugin_unregister_log);
static XS(ncollectd_plugin_unregister_notification);
static XS(ncollectd_plugin_dispatch_metric_family);
static XS(ncollectd_plugin_get_interval);
static XS(ncollectd_plugin_dispatch_notification);
static XS(ncollectd_plugin_log);
static XS(ncollectd_call_by_name);

static int perl_read(user_data_t *ud);
static int perl_write(metric_family_t const *fam, user_data_t *user_data);
static void perl_log(const log_msg_t *msg, user_data_t *user_data);
static int perl_notify(const notification_t *notif, user_data_t *user_data);

typedef struct c_ithread_s {
    /* the thread's Perl interpreter */
    PerlInterpreter *interp;
    bool running; /* thread is inside Perl interpreter */
    bool shutdown;
    pthread_t pthread;

    /* double linked list of threads */
    struct c_ithread_s *prev;
    struct c_ithread_s *next;
} c_ithread_t;

typedef struct {
    c_ithread_t *head;
    c_ithread_t *tail;

#ifdef NCOLLECTD_DEBUG
    /* some usage stats */
    int number_of_threads;
#endif

    pthread_mutex_t mutex;
    pthread_mutexattr_t mutexattr;
} c_ithread_list_t;

/* name / user_data for Perl matches / targets */
typedef struct {
    char *name;
    SV *user_data;
} pfc_user_data_t;

#define PFC_USER_DATA_FREE(data)        \
    do {                                \
        free((data)->name);             \
        if (NULL != (data)->user_data)  \
            sv_free((data)->user_data); \
        free(data);                     \
    } while (0)

extern char **environ;

/* if perl_threads != NULL perl_threads->head must
 * point to the "base" thread */
static c_ithread_list_t *perl_threads;

/* the key used to store each pthread's ithread */
static pthread_key_t perl_thr_key;

static int perl_argc;
static char **perl_argv;

static char base_name[DATA_MAX_NAME_LEN] = "";

static struct {
    char name[64];
    XS((*f));
} api[] = {
    { "NCollectd::plugin_register_read",           ncollectd_plugin_register_read           },
    { "NCollectd::plugin_register_write",          ncollectd_plugin_register_write          },
    { "NCollectd::plugin_register_log",            ncollectd_plugin_register_log            },
    { "NCollectd::plugin_register_notification",   ncollectd_plugin_register_notification   },
    { "NCollectd::plugin_unregister_read",         ncollectd_plugin_unregister_read         },
    { "NCollectd::plugin_unregister_write",        ncollectd_plugin_unregister_write        },
    { "NCollectd::plugin_unregister_log",          ncollectd_plugin_unregister_log          },
    { "NCollectd::plugin_unregister_notification", ncollectd_plugin_unregister_notification },
    { "NCollectd::plugin_dispatch_metric_family",  ncollectd_plugin_dispatch_metric_family  },
    { "NCollectd::plugin_get_interval",            ncollectd_plugin_get_interval            },
    { "NCollectd::plugin_dispatch_notification",   ncollectd_plugin_dispatch_notification   },
    { "NCollectd::plugin_log",                     ncollectd_plugin_log                     },
    { "NCollectd::call_by_name",                   ncollectd_call_by_name                   },
    { "",                                          NULL                                     }
};

struct {
    char name[64];
    int value;
} constants[] = {
    { "NCollectd::TYPE_INIT",                   PLUGIN_INIT                 },
    { "NCollectd::TYPE_READ",                   PLUGIN_READ                 },
    { "NCollectd::TYPE_WRITE",                  PLUGIN_WRITE                },
    { "NCollectd::TYPE_SHUTDOWN",               PLUGIN_SHUTDOWN             },
    { "NCollectd::TYPE_LOG",                    PLUGIN_LOG                  },
    { "NCollectd::TYPE_NOTIF",                  PLUGIN_NOTIF                },
    { "NCollectd::TYPE_FLUSH",                  PLUGIN_FLUSH                },
    { "NCollectd::TYPE_CONFIG",                 PLUGIN_CONFIG               },
    { "NCollectd::METRIC_TYPE_UNKNOWN",         METRIC_TYPE_UNKNOWN         },
    { "NCollectd::METRIC_TYPE_GAUGE",           METRIC_TYPE_GAUGE           },
    { "NCollectd::METRIC_TYPE_COUNTER",         METRIC_TYPE_COUNTER         },
    { "NCollectd::METRIC_TYPE_STATE_SET",       METRIC_TYPE_STATE_SET       },
    { "NCollectd::METRIC_TYPE_INFO",            METRIC_TYPE_INFO            },
    { "NCollectd::METRIC_TYPE_SUMMARY",         METRIC_TYPE_SUMMARY         },
    { "NCollectd::METRIC_TYPE_HISTOGRAM",       METRIC_TYPE_HISTOGRAM       },
    { "NCollectd::METRIC_TYPE_GAUGE_HISTOGRAM", METRIC_TYPE_GAUGE_HISTOGRAM },
    { "NCollectd::UNKNOWN_FLOAT64",             UNKNOWN_FLOAT64             },
    { "NCollectd::UNKNOWN_INT64",               UNKNOWN_INT64               },
    { "NCollectd::GAUGE_FLOAT64",               GAUGE_FLOAT64               },
    { "NCollectd::GAUGE_INT64",                 GAUGE_INT64                 },
    { "NCollectd::COUNTER_UINT64",              COUNTER_UINT64              },
    { "NCollectd::COUNTER_FLOAT64",             COUNTER_FLOAT64             },
    { "NCollectd::LOG_ERR",                     LOG_ERR                     },
    { "NCollectd::LOG_WARNING",                 LOG_WARNING                 },
    { "NCollectd::LOG_NOTICE",                  LOG_NOTICE                  },
    { "NCollectd::LOG_INFO",                    LOG_INFO                    },
    { "NCollectd::LOG_DEBUG",                   LOG_DEBUG                   },
    { "NCollectd::NOTIF_FAILURE",               NOTIF_FAILURE               },
    { "NCollectd::NOTIF_WARNING",               NOTIF_WARNING               },
    { "NCollectd::NOTIF_OKAY",                  NOTIF_OKAY                  },
    { "",                                      0                            }
};

/*
 * Helper functions for data type conversion.
 */

/*
 * data source:
 * [
 *   {
 *       name => $ds_name,
 *       type => $ds_type,
 *       min    => $ds_min,
 *       max    => $ds_max
 *   },
 *   ...
 * ]
 */
#if 0
static int hv2data_source(pTHX_ HV *hash, data_source_t *ds)
{
    SV **tmp = NULL;

    if ((hash == NULL) || (ds == NULL))
        return -1;

    if ((tmp = hv_fetch(hash, "name", 4, 0)) != NULL) {
        sstrncpy(ds->name, SvPV_nolen(*tmp), sizeof(ds->name));
    } else {
        PLUGIN_ERROR("hv2data_source: No DS name given.");
        return -1;
    }

    if ((tmp = hv_fetch(hash, "type", 4, 0)) != NULL) {
        ds->type = SvIV(*tmp);

        if ((DS_TYPE_COUNTER != ds->type) && (DS_TYPE_GAUGE != ds->type) && (DS_TYPE_DERIVE != ds->type)) {
            PLUGIN_ERROR("hv2data_source: Invalid DS type.");
            return -1;
        }
    } else {
        ds->type = DS_TYPE_COUNTER;
    }

    if ((tmp = hv_fetch(hash, "min", 3, 0)) != NULL)
        ds->min = SvNV(*tmp);
    else
        ds->min = NAN;

    if ((tmp = hv_fetch(hash, "max", 3, 0)) != NULL)
        ds->max = SvNV(*tmp);
    else
        ds->max = NAN;
    return 0;
}
#endif

static unsigned int sv2int(pTHX_  SV *value)
{
    int rvalue = 0;

    if (SvNOK(value)) {
        rvalue = (int)SvNVX(value);
    } else if (SvUOK(value)) {
        rvalue = (int)SvUVX(value);
    } else if (SvIOK(value)) {
        rvalue = (int)SvIVX(value);
    } else {
        const char *str = SvPV_nolen(value);
        if (str != NULL)
            rvalue = (int)strtoul(str, NULL, 10);
    }

    return rvalue;
}

static unsigned int sv2uint(pTHX_  SV *value)
{
    unsigned int rvalue = 0;

    if (SvNOK(value)) {
        rvalue = (unsigned int)SvNVX(value);
    } else if (SvUOK(value)) {
        rvalue = (unsigned int)SvUVX(value);
    } else if (SvIOK(value)) {
        rvalue = (unsigned int)SvIVX(value);
    } else {
        const char *str = SvPV_nolen(value);
        if (str != NULL)
            rvalue = (unsigned int)strtoul(str, NULL, 10);
    }

    return rvalue;
}

static uint64_t sv2uint64(pTHX_  SV *value)
{
    uint64_t rvalue = 0;

    if (SvNOK(value)) {
        rvalue = SvNVX(value);
    } else if (SvUOK(value)) {
        rvalue = SvUVX(value);
    } else if (SvIOK(value)) {
        rvalue = SvIVX(value);
    } else {
        const char *str = SvPV_nolen(value);
        if (str != NULL)
            rvalue = (uint64_t)strtoull(str, NULL, 10);
    }

    return rvalue;
}

static int64_t sv2int64(pTHX_  SV *value)
{
    int64_t rvalue = 0;

    if (SvNOK(value)) {
        rvalue = SvNVX(value);
    } else if (SvUOK(value)) {
        rvalue = SvUVX(value);
    } else if (SvIOK(value)) {
        rvalue = SvIVX(value);
    } else {
        const char *str = SvPV_nolen(value);
        if (str != NULL)
            rvalue = (int64_t)strtoll(str, NULL, 10);
    }

    return rvalue;
}

static double sv2double(pTHX_  SV *value)
{
    double rvalue = 0.0;

    if (SvNOK(value)) {
        rvalue = SvNVX(value);
    } else if (SvUOK(value)) {
        rvalue = SvUVX(value);
    } else if (SvIOK(value)) {
        rvalue = SvIVX(value);
    } else {
        const char *str = SvPV_nolen(value);
        if (str != NULL)
            rvalue = (double)strtod(str, NULL);
    }

    return rvalue;
}

static int av2label(pTHX_ HV *hash, label_set_t *label)
{
    if (hash == NULL)
        return 0;

    I32 keys = hv_iterinit(hash);
    for (I32 i = 0; i < keys; i++) {
        char *key = NULL;
        I32 key_len = 0;
        SV *value = hv_iternextsv(hash, &key, &key_len);
        if (!SvROK(value)) {
            char buffer[DTOA_MAX > ITOA_MAX ? DTOA_MAX : ITOA_MAX];
            char *lvalue = buffer;
            size_t lvalue_len = 0;
            if (SvNOK(value)) {
                lvalue_len = dtoa(SvNVX(value), buffer, sizeof(buffer));
            } else if (SvUOK(value)) {
                lvalue_len = uitoa(SvUVX(value), buffer);
            } else if (SvIOK(value)) {
                lvalue_len = itoa(SvIVX(value), buffer);
            } else {
                lvalue = SvPV_nolen(value);
                if (lvalue != NULL)
                    lvalue_len = strlen(lvalue);
            }

            if ((lvalue != NULL) && (lvalue_len > 0) && (key != NULL) && (key_len > 0))
                _label_set_add(label, true, false, key, key_len, lvalue, lvalue_len); // FIXME escape
        }
    }

    return 0;
}

#if 0
    hv_iterinit(hash);
    while (value = hv_iternextsv(hash, &key, &key_length)) {

        if (!(SvROK(value) && (SVt_PVHV == SvTYPE(SvRV(value))))) {

      if (SvROK(value)) {
        SV * const referenced = SvRV(value);
        if (SvTYPE(referenced) == SVt_PVAV) {
          printf("The value at key %s is reference to an array\n", key);
        }
        else if (SvTYPE(referenced) == SVt_PVHV) {
           printf("The value at key %s is a reference to a hash\n", key);
        }
        else {
           printf("The value at key %s is a reference\n", key);
        }
      } else {
        printf("The value at key %s is not a reference\n", key);
      }



    int len = av_len(array);

    for (int i = 0; i <= len; ++i) {
        SV **tmp = av_fetch(array, i, 0);

        if (tmp == NULL)
            return -1;

        if (!(SvROK(*tmp) && (SVt_PVHV == SvTYPE(SvRV(*tmp))))) {
            PLUGIN_WARNING("Skipping invalid label information.");
            continue;
        }

        HV *hash = (HV *)SvRV(*tmp);

        notification_meta_t *m = calloc(1, sizeof(*m));
        if (m == NULL)
            return ENOMEM;

        SV **name = hv_fetch(hash, "name", strlen("name"), 0);
        if (name == NULL) {
            PLUGIN_WARNING("Skipping invalid meta information.");
            free(m);
            continue;
        }
        sstrncpy(m->name, SvPV_nolen(*name), sizeof(m->name));

        SV **value = hv_fetch(hash, "value", strlen("value"), 0);
        if (value == NULL) {
            PLUGIN_WARNING("Skipping invalid meta information.");
            free(m);
            continue;
        }
#if 0
        if (SvNOK(*value)) {
            m->nm_value.nm_double = SvNVX(*value);
            m->type = NM_TYPE_DOUBLE;
        } else if (SvUOK(*value)) {
            m->nm_value.nm_unsigned_int = SvUVX(*value);
            m->type = NM_TYPE_UNSIGNED_INT;
        } else if (SvIOK(*value)) {
            m->nm_value.nm_signed_int = SvIVX(*value);
            m->type = NM_TYPE_SIGNED_INT;
        } else {
            m->nm_value.nm_string = sstrdup(SvPV_nolen(*value));
            m->type = NM_TYPE_STRING;
        }
#endif
    }

    return 0;
}
#endif

/* av2metric converts at most "len" elements from "array" to "metric".
 * Returns the number of elements converted or zero on error. */
static size_t av2metric(pTHX_ HV *hash, metric_type_t type, metric_t *m)
{
    if (hash == NULL)
        return 0;

    SV **tmp = NULL;

    if ((tmp = hv_fetch(hash, "time", 4, 0)) != NULL) {
        double t = sv2double(aTHX_ *tmp);
        m->time = DOUBLE_TO_CDTIME_T(t);
    }

    if ((tmp = hv_fetch(hash, "interval", 8, 0)) != NULL) {
        double t = sv2double(aTHX_ *tmp);
        m->interval = DOUBLE_TO_CDTIME_T(t);
    }

    if ((tmp = hv_fetch(hash, "labels", 6, 0)) != NULL) {
        if (!(SvROK(*tmp) && (SVt_PVHV == SvTYPE(SvRV(*tmp))))) {
            PLUGIN_ERROR("No valid labels given.");
            return -1;
        }

        int status = av2label(aTHX_ (HV *)SvRV(*tmp), &m->label);
        if (status != 0)
            return -1;
    }

    switch (type) {
    case METRIC_TYPE_UNKNOWN:
        m->value.unknown.type = UNKNOWN_FLOAT64;
        if ((tmp = hv_fetch(hash, "type", 4, 0)) != NULL) {
            unsigned int type = sv2uint(aTHX_ *tmp);
            if ((type != UNKNOWN_FLOAT64) && (type != UNKNOWN_INT64)) {
                return -1;
            }
            m->value.unknown.type = type;
        }
        if ((tmp = hv_fetch(hash, "value", 5, 0)) != NULL) {
            if (m->value.unknown.type == UNKNOWN_FLOAT64)
                m->value.unknown.float64 = sv2double(aTHX_ *tmp);
//                m->value.unknown.float64 = SvNVX(*tmp);
            else
                m->value.unknown.int64 = sv2int64(aTHX_ *tmp);
//                m->value.unknown.int64 = SvIVX(*tmp);
        }
        break;
    case METRIC_TYPE_GAUGE:
        m->value.gauge.type = GAUGE_FLOAT64;
        if ((tmp = hv_fetch(hash, "type", 4, 0)) != NULL) {
            unsigned int type = sv2uint(aTHX_ *tmp);
            if ((type != GAUGE_FLOAT64) && (type != GAUGE_INT64)) {
                return -1;
            }
            m->value.gauge.type = type;
        }
        if ((tmp = hv_fetch(hash, "value", 5, 0)) != NULL) {
            if (m->value.gauge.type == GAUGE_FLOAT64)
                m->value.gauge.float64 = sv2double(aTHX_ *tmp);
//                m->value.gauge.float64 = SvNVX(*tmp);
            else
                m->value.gauge.int64 = sv2int64(aTHX_ *tmp);
//                m->value.gauge.int64 = SvIVX(*tmp);
        }
        break;
    case METRIC_TYPE_COUNTER:
        m->value.counter.type = COUNTER_UINT64;
        if ((tmp = hv_fetch(hash, "type", 4, 0)) != NULL) {
            unsigned int type = sv2uint(aTHX_ *tmp);
            if ((type != COUNTER_UINT64) && (type != COUNTER_FLOAT64)) {
                return -1;
            }
            m->value.counter.type = type;
        }
        if ((tmp = hv_fetch(hash, "value", 5, 0)) != NULL) {
            if (m->value.counter.type == COUNTER_UINT64) {
                m->value.counter.uint64 = sv2uint64(aTHX_ *tmp);
//                m->value.counter.uint64 = SvUVX(*tmp);
            } else
                m->value.counter.float64 = sv2double(aTHX_ *tmp);
//                m->value.counter.float64 = SvNVX(*tmp);
        }
        break;
    case METRIC_TYPE_STATE_SET:
        if ((tmp = hv_fetch(hash, "stateset", 8, 0)) != NULL) {
        }
        break;
    case METRIC_TYPE_INFO:
        if ((tmp = hv_fetch(hash, "info", 4, 0)) != NULL) {
            if (!(SvROK(*tmp) && (SVt_PVHV == SvTYPE(SvRV(*tmp))))) {
                PLUGIN_ERROR("No valid metric info given.");
                return -1;
            }

            int status = av2label(aTHX_ (HV *)SvRV(*tmp), &m->value.info);
            if (status != 0)
                return -1;
        }
        break;
    case METRIC_TYPE_SUMMARY: {
        summary_t *summary = summary_new();
        if (summary == NULL) {
            PLUGIN_ERROR("Cannot alloc summary value.");
            return -1;
        }

        if ((tmp = hv_fetch(hash, "quantiles", 9, 0)) != NULL) {
        }
        if ((tmp = hv_fetch(hash, "count", 5, 0)) != NULL) {
        }
        if ((tmp = hv_fetch(hash, "sum", 3, 0)) != NULL) {
        }
    }   break;
    case METRIC_TYPE_HISTOGRAM: {
        histogram_t *histogram = histogram_new();
        if (histogram == NULL) {
            PLUGIN_ERROR("Cannot alloc summary value.");
            return -1;
        }

        if ((tmp = hv_fetch(hash, "buckets", 7, 0)) != NULL) {
        }
        if ((tmp = hv_fetch(hash, "count", 5, 0)) != NULL) {
        }
        if ((tmp = hv_fetch(hash, "sum", 3, 0)) != NULL) {
        }
    }   break;
    case METRIC_TYPE_GAUGE_HISTOGRAM: {
        histogram_t *histogram = histogram_new();
        if (histogram == NULL) {
            PLUGIN_ERROR("Cannot alloc summary value.");
            return -1;
        }

        if ((tmp = hv_fetch(hash, "buckets", 7, 0)) != NULL) {
        }
        if ((tmp = hv_fetch(hash, "gcount", 6, 0)) != NULL) {
        }
        if ((tmp = hv_fetch(hash, "gsum", 4, 0)) != NULL) {
        }
    }   break;
    }

    return 0;
}

#if 0
static int hv2metric_list(pTHX_ HV *hash, metric_single_t *m)
{
    if ((hash == NULL) || (m == NULL))
        return -1;
    SV **tmp = av_fetch(array, 0, 0);

    if (tmp != NULL) {
#if 0
        if (DS_TYPE_COUNTER == m->value_type)
            m->value.counter = SvIV(*tmp);
        else if (DS_TYPE_GAUGE == m->value_type)
            m->value.gauge = SvNV(*tmp);
        else if (DS_TYPE_DERIVE == m->value_type)
            m->value.derive = SvIV(*tmp);
        else if (DS_TYPE_ABSOLUTE == m->value_type)
            m->value.absolute = SvIV(*tmp);
#endif
    } else {
        return 0;
    }
    return 1;
}
#endif

static int hv2metric_family(pTHX_ HV *hash, metric_family_t *fam)
{
    if ((hash == NULL) || (fam == NULL))
        return -1;

    SV **tmp = NULL;

    fam->type = METRIC_TYPE_UNKNOWN;

    if ((tmp = hv_fetch(hash, "name", 4, 0)) != NULL) {
        fam->name = strdup(SvPV_nolen(*tmp));
        if (fam->name  == NULL) {
            PLUGIN_ERROR("strdup failed.");
            return -1;
        }
    }

    if (fam->name == NULL) {
        PLUGIN_ERROR("Missing name in metric family.");
        return -1;
    }

    if ((tmp = hv_fetch(hash, "help", 4, 0)) != NULL) {
        fam->help = strdup(SvPV_nolen(*tmp));
        if (fam->help == NULL) {
            PLUGIN_ERROR("strdup failed.");
            return -1;
        }
    }

    if ((tmp = hv_fetch(hash, "unit", 4, 0)) != NULL) {
        fam->unit = strdup(SvPV_nolen(*tmp));
        if (fam->unit == NULL) {
            PLUGIN_ERROR("strdup failed.");
            return -1;
        }
    }

    if ((tmp = hv_fetch(hash, "type", 4, 0)) != NULL) {
        int type = sv2int(aTHX_ *tmp);
        switch (type) {
        case METRIC_TYPE_UNKNOWN:
        case METRIC_TYPE_GAUGE:
        case METRIC_TYPE_COUNTER:
        case METRIC_TYPE_STATE_SET:
        case METRIC_TYPE_INFO:
        case METRIC_TYPE_SUMMARY:
        case METRIC_TYPE_HISTOGRAM:
        case METRIC_TYPE_GAUGE_HISTOGRAM:
            break;
        default:
            PLUGIN_ERROR("Unknow metric type: %d", type);
            return -1;
            break;
        }
        fam->type = type;
    }

    if (((tmp = hv_fetch(hash, "metrics", 7, 0)) == NULL) ||
        (!(SvROK(*tmp) && (SVt_PVAV == SvTYPE(SvRV(*tmp)))))) {
        PLUGIN_ERROR("No valid metrics given.");
        return -1;
    }

    AV *array = (AV *)SvRV(*tmp);
    /* av_len returns the highest index, not the actual length. */
    size_t len = (size_t)(av_len(array) + 1);
    if (len == 0)
        return -1;

    fam->metric.ptr = calloc(len, sizeof(*fam->metric.ptr));
    if (fam->metric.ptr == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    size_t n = 0;
    for (size_t i = 0; i <= len; ++i) {
        SV **elem = av_fetch(array, i, 0);
        if (elem == NULL)
            continue;
        if (!(SvROK(*elem) && (SvTYPE(SvRV(*elem)) == SVt_PVHV)))
            continue;

        int status = av2metric(aTHX_ (HV *)SvRV(*elem), fam->type, &fam->metric.ptr[n]);
        if (status != 0)
            break;
        n++;
    }
    fam->metric.num = n;

    return 0;
}

#if 0
static int av2data_set(pTHX_ AV *array, char *name, data_set_t *ds)
{
    int len;

    if ((array == NULL) || (name == NULL) || (ds == NULL))
        return -1;

    len = av_len(array);

    if (len == -1) {
        PLUGIN_ERROR("Invalid data set.");
        return -1;
    }

    ds->ds = smalloc((len + 1) * sizeof(*ds->ds));
    ds->ds_num = len + 1;

    for (int i = 0; i <= len; ++i) {
        SV **elem = av_fetch(array, i, 0);

        if (elem == NULL) {
            PLUGIN_ERROR("Failed to fetch data source %i.", i);
            return -1;
        }

        if (!(SvROK(*elem) && (SVt_PVHV == SvTYPE(SvRV(*elem))))) {
            PLUGIN_ERROR("Invalid data source.");
            return -1;
        }

        if (hv2data_source(aTHX_(HV *) SvRV(*elem), &ds->ds[i]) == -1)
            return -1;

        PLUGIN_DEBUG("DS.name = \"%s\", DS.type = %i, DS.min = %f, DS.max = %f",
                      ds->ds[i].name, ds->ds[i].type, ds->ds[i].min, ds->ds[i].max);
    }

    sstrncpy(ds->type, name, sizeof(ds->type));
    return 0;
}
#endif
#if 0
/*
 * notification:
 * {
 *   severity => $severity,
 *   time           => $time,
 *   message    => $msg,
 *   host           => $host,
 *   plugin     => $plugin,
 *   type           => $type,
 *   plugin_instance => $instance,
 *   type_instance   => $type_instance,
 *   meta           => [ { name => <name>, value => <value> }, ... ]
 * }
 */
static int av2notification_meta(pTHX_ AV *array, notification_meta_t **ret_meta)
{
    notification_meta_t *tail = NULL;

    int len = av_len(array);

    for (int i = 0; i <= len; ++i) {
        SV **tmp = av_fetch(array, i, 0);

        if (tmp == NULL)
            return -1;

        if (!(SvROK(*tmp) && (SVt_PVHV == SvTYPE(SvRV(*tmp))))) {
            PLUGIN_WARNING("Skipping invalid meta information.");
            continue;
        }

        HV *hash = (HV *)SvRV(*tmp);

        notification_meta_t *m = calloc(1, sizeof(*m));
        if (m == NULL)
            return ENOMEM;

        SV **name = hv_fetch(hash, "name", strlen("name"), 0);
        if (name == NULL) {
            PLUGIN_WARNING("Skipping invalid meta information.");
            free(m);
            continue;
        }
        sstrncpy(m->name, SvPV_nolen(*name), sizeof(m->name));

        SV **value = hv_fetch(hash, "value", strlen("value"), 0);
        if (value == NULL) {
            PLUGIN_WARNING("Skipping invalid meta information.");
            free(m);
            continue;
        }
#if 0
        if (SvNOK(*value)) {
            m->nm_value.nm_double = SvNVX(*value);
            m->type = NM_TYPE_DOUBLE;
        } else if (SvUOK(*value)) {
            m->nm_value.nm_unsigned_int = SvUVX(*value);
            m->type = NM_TYPE_UNSIGNED_INT;
        } else if (SvIOK(*value)) {
            m->nm_value.nm_signed_int = SvIVX(*value);
            m->type = NM_TYPE_SIGNED_INT;
        } else {
            m->nm_value.nm_string = sstrdup(SvPV_nolen(*value));
            m->type = NM_TYPE_STRING;
        }
#endif
        m->next = NULL;
        if (tail == NULL)
            *ret_meta = m;
        else
            tail->next = m;
        tail = m;
    }

    return 0;
}
#endif

static int hv2notification(pTHX_ HV *hash, notification_t *n)
{
    SV **tmp = NULL;

    if ((hash == NULL) || (n == NULL))
        return -1;

    if ((tmp = hv_fetch(hash, "name", 4, 0)) != NULL) {
        n->name = strdup(SvPV_nolen(*tmp));
        if (n->name  == NULL) {
            PLUGIN_ERROR("strdup failed.");
            return -1;
        }
    }

    if (n->name == NULL) {
        PLUGIN_ERROR("Missing name in notification.");
        return -1;
    }

    n->severity = NOTIF_FAILURE;
    if ((tmp = hv_fetch(hash, "severity", 8, 0)) != NULL) {
        unsigned int severity = SvIV(*tmp);
        if ((severity != NOTIF_FAILURE) && (severity != NOTIF_WARNING) && (severity != NOTIF_OKAY)) {
            PLUGIN_ERROR("Invalid notification sevirity.");
            return -1;
        }
        n->severity = severity;
    }

    if ((tmp = hv_fetch(hash, "time", 4, 0)) != NULL) {
        double t = SvNV(*tmp);
        n->time = DOUBLE_TO_CDTIME_T(t);
    } else {
        n->time = cdtime();
    }

    if ((tmp = hv_fetch(hash, "labels", 6, 0)) != NULL) {
        if (!(SvROK(*tmp) && (SVt_PVHV == SvTYPE(SvRV(*tmp))))) {
            PLUGIN_ERROR("No valid labels given.");
            return -1;
        }

        int status = av2label(aTHX_ (HV *)SvRV(*tmp), &n->label);
        if (status != 0)
            return -1;
    }

    if ((tmp = hv_fetch(hash, "annotations", 11, 0)) != NULL) {
        if (!(SvROK(*tmp) && (SVt_PVHV == SvTYPE(SvRV(*tmp))))) {
            PLUGIN_ERROR("No valid labels given.");
            return -1;
        }

        int status = av2label(aTHX_ (HV *)SvRV(*tmp), &n->annotation);
        if (status != 0)
            return -1;
    }



#if 0
    if ((tmp = hv_fetch(hash, "message", 7, 0)) != NULL)
        sstrncpy(n->message, SvPV_nolen(*tmp), sizeof(n->message));

    if ((tmp = hv_fetch(hash, "host", 4, 0)) != NULL)
        sstrncpy(n->host, SvPV_nolen(*tmp), sizeof(n->host));
    else
        sstrncpy(n->host, hostname_g, sizeof(n->host));

    if ((tmp = hv_fetch(hash, "plugin", 6, 0)) != NULL)
        sstrncpy(n->plugin, SvPV_nolen(*tmp), sizeof(n->plugin));

    if ((tmp = hv_fetch(hash, "plugin_instance", 15, 0)) != NULL)
        sstrncpy(n->plugin_instance, SvPV_nolen(*tmp), sizeof(n->plugin_instance));

    if ((tmp = hv_fetch(hash, "type", 4, 0)) != NULL)
        sstrncpy(n->type, SvPV_nolen(*tmp), sizeof(n->type));

    if ((tmp = hv_fetch(hash, "type_instance", 13, 0)) != NULL)
        sstrncpy(n->type_instance, SvPV_nolen(*tmp), sizeof(n->type_instance));
#endif
#if 0
    n->meta = NULL;
    while ((tmp = hv_fetch(hash, "meta", 4, 0)) != NULL) {
        if (!(SvROK(*tmp) && (SVt_PVAV == SvTYPE(SvRV(*tmp))))) {
            PLUGIN_WARNING("Ignoring invalid meta information.");
            break;
        }

        if (av2notification_meta(aTHX_(AV *) SvRV(*tmp), &n->meta) != 0) {
            plugin_notification_meta_free(n->meta);
            n->meta = NULL;
            return -1;
        }
        break;
    }
#endif
    return 0;
}
#if 0
static int data_set2av(pTHX_ data_set_t *ds, AV *array)
{
    if ((ds == NULL) || (array == NULL))
        return -1;

    av_extend(array, ds->ds_num);

    for (size_t i = 0; i < ds->ds_num; ++i) {
        HV *source = newHV();

        if (NULL == hv_store(source, "name", 4, newSVpv(ds->ds[i].name, 0), 0))
            return -1;

        if (NULL == hv_store(source, "type", 4, newSViv(ds->ds[i].type), 0))
            return -1;

        if (!isnan(ds->ds[i].min))
            if (NULL == hv_store(source, "min", 3, newSVnv(ds->ds[i].min), 0))
                return -1;

        if (!isnan(ds->ds[i].max))
            if (NULL == hv_store(source, "max", 3, newSVnv(ds->ds[i].max), 0))
                return -1;

        if (av_store(array, i, newRV_noinc((SV *)source)) == NULL)
            return -1;
    }
    return 0;
}
#endif

static int label2hv(pTHX_ label_set_t *label, HV *hash)
{
    if ((label  == NULL) || (hash == NULL))
        return -1;

    for (size_t i = 0; i < label->num; i++) {
        label_pair_t *pair = &label->ptr[i];
        size_t name_len = strlen(pair->name);
        if (hv_store(hash, pair->name, name_len, newSVpv(pair->value, 0), 0) == NULL)
            return -1;
    }

    return 0;
}

static int metric2hv(pTHX_ metric_type_t type, metric_t *m, HV *hash)
{
    if ((m == NULL) || (hash == NULL))
        return -1;

    if (m->time != 0) {
        double t = CDTIME_T_TO_DOUBLE(m->time);
        if (hv_store(hash, "time", 4, newSVnv(t), 0) == NULL)
            return -1;
    }

    if (m->interval != 0) {
        double t = CDTIME_T_TO_DOUBLE(m->interval);
        if (hv_store(hash, "interval", 8, newSVnv(t), 0) == NULL)
            return -1;
    }

    HV *label = newHV();
    if (label== NULL)
        return -1;

    label2hv(aTHX_ &m->label, label);
    if (hv_store(hash, "unknown", 7, newRV_noinc((SV *)label), 0) == NULL) {
        // FIXME
        return -1;
    }

    HV *value = newHV();
    if (value == NULL)
        return -1;

    switch (type) {
    case METRIC_TYPE_UNKNOWN: {
        if (hv_store(hash, "value", 5, newRV_noinc((SV *)value), 0) == NULL)
            return -1;
    }   break;
    case METRIC_TYPE_GAUGE: {
        if (hv_store(hash, "value", 5, newRV_noinc((SV *)value), 0) == NULL)
            return -1;
    }   break;
    case METRIC_TYPE_COUNTER: {
        if (hv_store(hash, "value", 5, newRV_noinc((SV *)value), 0) == NULL)
            return -1;
    }   break;
    case METRIC_TYPE_STATE_SET: {
        if (hv_store(hash, "stateset", 9, newRV_noinc((SV *)value), 0) == NULL)
            return -1;
    }   break;
    case METRIC_TYPE_INFO: {
        if (hv_store(hash, "info", 4, newRV_noinc((SV *)value), 0) == NULL)
            return -1;
    }   break;
    case METRIC_TYPE_SUMMARY: {
        if (hv_store(hash, "summary", 7, newRV_noinc((SV *)value), 0) == NULL)
            return -1;
    }   break;
    case METRIC_TYPE_HISTOGRAM: {
        if (hv_store(hash, "histogram", 9, newRV_noinc((SV *)value), 0) == NULL)
            return -1;
    }   break;
    case METRIC_TYPE_GAUGE_HISTOGRAM: {
        if (hv_store(hash, "gauge_histogram", 15, newRV_noinc((SV *)value), 0) == NULL)
            return -1;
    }   break;
    }

    return 0;
}

static int metric_family2hv(pTHX_ metric_family_t *fam, HV *hash)
{
    if ((fam == NULL) || (hash == NULL))
        return -1;

#if 0
    AV *values = newAV();
    /* av_extend takes the last *index* to which the array should be extended. */
    av_extend(values, vl->values_len - 1);

    assert(ds->ds_num == vl->values_len);
    for (i = 0; i < vl->values_len; ++i) {
        SV *val = NULL;

        if (DS_TYPE_COUNTER == ds->ds[i].type)
            val = newSViv(vl->values[i].counter);
        else if (DS_TYPE_GAUGE == ds->ds[i].type)
            val = newSVnv(vl->values[i].gauge);
        else if (DS_TYPE_DERIVE == ds->ds[i].type)
            val = newSViv(vl->values[i].derive);

        if (NULL == av_store(values, i, val)) {
            av_undef(values);
            return -1;
        }
    }
#endif

    if (fam->name != NULL) {
        if (hv_store(hash, "name", 4, newSVpv(fam->name, 0), 0) == NULL)
            return -1;
    }

    if (fam->help != NULL) {
        if (hv_store(hash, "help", 4, newSVpv(fam->help, 0), 0) == NULL)
            return -1;
    }

    if (fam->unit != NULL) {
        if (hv_store(hash, "unit", 4, newSVpv(fam->unit, 0), 0) == NULL)
            return -1;
    }

    if (hv_store(hash, "type", 4, newSVuv(fam->type), 0) == NULL)
        return -1;

#if 0
    if (hv_store(hash, "values", 6, newRV_noinc((SV *)values), 0) == NULL)
        return -1;
#endif

    AV *metrics = newAV();
    /* av_extend takes the last *index* to which the array should be extended. */
    av_extend(metrics, fam->metric.num - 1);

    for (size_t i = 0; i < fam->metric.num; ++i) {
        HV *metric = newHV();
        if (metric == NULL)
            return -1;

        int status = metric2hv(aTHX_ fam->type, &fam->metric.ptr[i], metric);
        if (status != 0) {
            hv_clear(metric);
            hv_undef(metric);
            av_clear(metrics);
            av_undef(metrics);
            return -1;
        }

        if (av_store(metrics, i, newRV_noinc((SV *)metric)) == NULL) {
            hv_clear(metric);
            hv_undef(metric);
            av_clear(metrics);
            av_undef(metrics);
            return -1;
        }
    }

    if (hv_store(hash, "metrics", 7, newRV_noinc((SV *)metrics), 0) == NULL)
        return -1;


#if 0
    {
        double t = CDTIME_T_TO_DOUBLE(vl->interval);
        if (NULL == hv_store(hash, "interval", 8, newSVnv(t), 0))
            return -1;
    }
#endif
    return 0;
}

#if 0
static int notification_meta2av(pTHX_ notification_meta_t *meta, AV *array)
{
    int meta_num = 0;
    for (notification_meta_t *m = meta; m != NULL; m = m->next) {
        ++meta_num;
    }

    av_extend(array, meta_num);

    for (int i = 0; NULL != meta; meta = meta->next, ++i) {
        HV *m = newHV();
        SV *value;

        if (hv_store(m, "name", 4, newSVpv(meta->name, 0), 0) == NULL)
            return -1;

        if (NM_TYPE_STRING == meta->type)
            value = newSVpv(meta->nm_value.nm_string, 0);
        else if (NM_TYPE_SIGNED_INT == meta->type)
            value = newSViv(meta->nm_value.nm_signed_int);
        else if (NM_TYPE_UNSIGNED_INT == meta->type)
            value = newSVuv(meta->nm_value.nm_unsigned_int);
        else if (NM_TYPE_DOUBLE == meta->type)
            value = newSVnv(meta->nm_value.nm_double);
        else if (NM_TYPE_BOOLEAN == meta->type)
            value = meta->nm_value.nm_boolean ? &PL_sv_yes : &PL_sv_no;
        else
            return -1;

        if (hv_store(m, "value", 5, value, 0) == NULL) {
            sv_free(value);
            return -1;
        }

        if (av_store(array, i, newRV_noinc((SV *)m)) == NULL) {
            hv_clear(m);
            hv_undef(m);
            return -1;
        }
    }
    return 0;
}
#endif

static int notification2hv(pTHX_ notification_t *n, HV *hash)
{
    if (hv_store(hash, "severity", 8, newSViv(n->severity), 0) == NULL)
        return -1;

    if (n->time != 0) {
        double t = CDTIME_T_TO_DOUBLE(n->time);
        if (hv_store(hash, "time", 4, newSVnv(t), 0) == NULL)
            return -1;
    }

    if (n->name != NULL)
        if (hv_store(hash, "name", 4, newSVpv(n->name, 0), 0) == NULL)
            return -1;

    HV *labels = newHV();
    if (labels == NULL) {
        PLUGIN_ERROR("newHV failed.");
        return -1;
    }

    label2hv(aTHX_ &n->label, labels);
    if (hv_store(hash, "labels", 6, newRV_noinc((SV *)labels), 0) == NULL) {
        hv_clear(labels);
        hv_undef(labels);
        return -1;
    }

    HV *annotations = newHV();
    if (annotations == NULL) {
        PLUGIN_ERROR("newHV failed.");
        return -1;
    }

    label2hv(aTHX_ &n->annotation, annotations);
    if (hv_store(hash, "labels", 6, newRV_noinc((SV *)annotations), 0) == NULL) {
        hv_clear(annotations);
        hv_undef(annotations);
        return -1;
    }

#if 0
    if (*n->message != '\0')
        if (hv_store(hash, "message", 7, newSVpv(n->message, 0), 0) == NULL)
            return -1;

    if (*n->host != '\0')
        if (hv_store(hash, "host", 4, newSVpv(n->host, 0), 0) == NULL)
            return -1;

    if (*n->plugin != '\0')
        if (hv_store(hash, "plugin", 6, newSVpv(n->plugin, 0), 0) == NULL)
            return -1;

    if (*n->plugin_instance != '\0')
        if (hv_store(hash, "plugin_instance", 15, newSVpv(n->plugin_instance, 0), 0) == NULL)
            return -1;

    if (*n->type != '\0')
        if (hv_store(hash, "type", 4, newSVpv(n->type, 0), 0) == NULL)
            return -1;

    if (*n->type_instance != '\0')
        if (hv_store(hash, "type_instance", 13, newSVpv(n->type_instance, 0), 0) == NULL)
            return -1;
#endif
#if 0
    if (n->meta != NULL) {
        AV *meta = newAV();
        if ((notification_meta2av(aTHX_ n->meta, meta) != 0) ||
            (hv_store(hash, "meta", 4, newRV_noinc((SV *)meta), 0) == NULL)) {
            av_clear(meta);
            av_undef(meta);
            return -1;
        }
    }
#endif
    return 0;
}

static int config_item2hv(pTHX_ config_item_t *ci, HV *hash)
{
    AV *values;
    AV *children;

    if (hv_store(hash, "key", 3, newSVpv(ci->key, 0), 0) == NULL)
        return -1;

    values = newAV();
    if (0 < ci->values_num)
        av_extend(values, ci->values_num);

    if (hv_store(hash, "values", 6, newRV_noinc((SV *)values), 0) == NULL) {
        av_clear(values);
        av_undef(values);
        return -1;
    }

    for (int i = 0; i < ci->values_num; ++i) {
        SV *value;

        switch (ci->values[i].type) {
        case CONFIG_TYPE_STRING:
            value = newSVpv(ci->values[i].value.string, 0);
            break;
        case CONFIG_TYPE_NUMBER:
            value = newSVnv((NV)ci->values[i].value.number);
            break;
        case CONFIG_TYPE_BOOLEAN:
            value = ci->values[i].value.boolean ? &PL_sv_yes : &PL_sv_no;
            break;
        default:
            PLUGIN_ERROR("config_item2hv: Invalid value type %u.", ci->values[i].type);
            value = &PL_sv_undef;
        }

        if (NULL == av_store(values, i, value)) {
            sv_free(value);
            return -1;
        }
    }

    /* ignoring 'parent' member which is uninteresting in this case */

    children = newAV();
    if (0 < ci->children_num)
        av_extend(children, ci->children_num);

    if (NULL == hv_store(hash, "children", 8, newRV_noinc((SV *)children), 0)) {
        av_clear(children);
        av_undef(children);
        return -1;
    }

    for (int i = 0; i < ci->children_num; ++i) {
        HV *child = newHV();

        if (0 != config_item2hv(aTHX_ ci->children + i, child)) {
            hv_clear(child);
            hv_undef(child);
            return -1;
        }

        if (NULL == av_store(children, i, newRV_noinc((SV *)child))) {
            hv_clear(child);
            hv_undef(child);
            return -1;
        }
    }
    return 0;
}

/*
 * Internal functions.
 */

static char *get_module_name(char *buf, size_t buf_len, const char *module)
{
    int status = 0;
    if (base_name[0] == '\0')
        status = ssnprintf(buf, buf_len, "%s", module);
    else
        status = ssnprintf(buf, buf_len, "%s::%s", base_name, module);
    if ((status < 0) || ((unsigned int)status >= buf_len))
        return NULL;
    return buf;
}

/*
 * Submit the values to the write functions.
 */
static int pplugin_dispatch_metric_family(pTHX_ HV *values)
{
    if (values == NULL)
        return -1;

    int ret = -1;
    metric_family_t fam = {0};

    if (hv2metric_family(aTHX_ values, &fam) != 0)
        goto error;

    ret = plugin_dispatch_metric_family(&fam, 0);

error:
    metric_family_metric_reset(&fam);
    free(fam.name);
    free(fam.help);
    free(fam.unit);

    return ret;
}

/*
 * Dispatch a notification.
 */
static int pplugin_dispatch_notification(pTHX_ HV *notif)
{
    if (notif == NULL)
        return -1;

    notification_t n = {0};
    if (hv2notification(aTHX_ notif, &n) != 0)
        return -1;

    int ret = plugin_dispatch_notification(&n);
//    plugin_notification_meta_free(n.meta);
    return ret;
}

/*
 * Call perl sub with thread locking flags handled.
 */
static int call_pv_locked(pTHX_ const char *sub_name)
{
    c_ithread_t *t = (c_ithread_t *)pthread_getspecific(perl_thr_key);
    if (t == NULL) /* thread destroyed */
        return 0;

    bool old_running = t->running;
    t->running = true;

    if (t->shutdown) {
        t->running = old_running;
        return 0;
    }

    int ret = call_pv(sub_name, G_SCALAR | G_EVAL);

    t->running = old_running;
    return ret;
}

/*
 * Call all working functions of the given type.
 */
static int pplugin_call(pTHX_ int type, ...)
{
    int retvals = 0;

    va_list ap;
    int ret = 0;
    char *subname;

    dSP;

    if ((type < 0) || (type >= PLUGIN_TYPES))
        return -1;

    va_start(ap, type);

    ENTER;
    SAVETMPS;

    PUSHMARK(SP);

    switch (type) {
    case PLUGIN_READ:
        subname = va_arg(ap, char *);
        break;
    case PLUGIN_WRITE: {
        HV *pfam = newHV();
        subname = va_arg(ap, char *);
        /*
         * $_[0] = $plugin_type;
         *
         * $_[2] =
         * {
         *   values => [ $v1, ... ],
         *   time       => $time,
         *   host       => $hostname,
         *   plugin => $plugin,
         *   type       => $type,
         *   plugin_instance => $instance,
         *   type_instance   => $type_instance
         * };
         */
        metric_family_t *fam = va_arg(ap, metric_family_t *);

        if (metric_family2hv(aTHX_ fam, pfam) == -1) {
            hv_clear(pfam);
            hv_undef(pfam);
            pfam = (HV *)&PL_sv_undef;
            ret = -1;
        }

//        XPUSHs(sv_2mortal(newSVpv(ds->type, 0))); // FIXME
        XPUSHs(sv_2mortal(newRV_noinc((SV *)pfam)));
    }   break;
    case PLUGIN_LOG:
        subname = va_arg(ap, char *);
        /*
         * $_[0] = $level;
         *
         * $_[1] = $message;
         */
        XPUSHs(sv_2mortal(newSViv(va_arg(ap, int))));
        XPUSHs(sv_2mortal(newSVpv(va_arg(ap, char *), 0)));
        break;
    case PLUGIN_NOTIF: {
        notification_t *n;
        HV *notif = newHV();

        subname = va_arg(ap, char *);
        /*
         * $_[0] =
         * {
         *   severity => $severity,
         *   time           => $time,
         *   message    => $msg,
         *   host           => $host,
         *   plugin     => $plugin,
         *   type           => $type,
         *   plugin_instance => $instance,
         *   type_instance   => $type_instance
         * };
         */
        n = va_arg(ap, notification_t *);

        if (notification2hv(aTHX_ n, notif) == -1) {
            hv_clear(notif);
            hv_undef(notif);
            notif = (HV *)&PL_sv_undef;
            ret = -1;
        }

        XPUSHs(sv_2mortal(newRV_noinc((SV *)notif)));
    }   break;
    case PLUGIN_FLUSH: {
        subname = va_arg(ap, char *);
        /*
         * $_[0] = $timeout;
         * $_[1] = $identifier;
         */
        cdtime_t timeout = va_arg(ap, cdtime_t);

        XPUSHs(sv_2mortal(newSVnv(CDTIME_T_TO_DOUBLE(timeout))));
        XPUSHs(sv_2mortal(newSVpv(va_arg(ap, char *), 0)));
    }   break;
    case PLUGIN_INIT:
        subname = "NCollectd::plugin_call_all";
        XPUSHs(sv_2mortal(newSViv((IV)type)));
        break;
    case PLUGIN_SHUTDOWN:
        subname = "NCollectd::plugin_call_all";
        XPUSHs(sv_2mortal(newSViv((IV)type)));
        break;
    default:
        /* Unknown type. Run 'plugin_call_all' and make compiler happy */
        subname = "NCollectd::plugin_call_all";
        XPUSHs(sv_2mortal(newSViv((IV)type)));
        break;
    }

    PUTBACK;

    retvals = call_pv_locked(aTHX_ subname);

    SPAGAIN;
    if (SvTRUE(ERRSV)) {
        if (PLUGIN_LOG != type)
            PLUGIN_ERROR("perl: %s error: %s", subname, SvPV_nolen(ERRSV));
        ret = -1;
    } else if (0 < retvals) {
        SV *tmp = POPs;
        if (!SvTRUE(tmp))
            ret = -1;
    }

    PUTBACK;
    FREETMPS;
    LEAVE;

    va_end(ap);
    return ret;
}

/*
 * ncollectd's Perl interpreter based thread implementation.
 *
 * This has been inspired by Perl's ithreads introduced in version 5.6.0.
 */

/* must be called with perl_threads->mutex locked */
static void c_ithread_destroy(c_ithread_t *ithread)
{
    dTHXa(ithread->interp);

    assert(NULL != perl_threads);

    PERL_SET_CONTEXT(aTHX);
    /* Mark as running to avoid deadlock:
         c_ithread_destroy -> log_debug -> perl_log()
    */
    ithread->running = true;
    PLUGIN_DEBUG("Shutting down Perl interpreter %p...", (void *)aTHX);

#ifdef NCOLLECTD_DEBUG
    sv_report_used();

    --perl_threads->number_of_threads;
#endif

    perl_destruct(aTHX);
    perl_free(aTHX);

    if (NULL == ithread->prev)
        perl_threads->head = ithread->next;
    else
        ithread->prev->next = ithread->next;

    if (NULL == ithread->next)
        perl_threads->tail = ithread->prev;
    else
        ithread->next->prev = ithread->prev;

    free(ithread);
    return;
}

static void c_ithread_destructor(void *arg)
{
    c_ithread_t *ithread = (c_ithread_t *)arg;
    c_ithread_t *t = NULL;

    if (perl_threads == NULL)
        return;

    pthread_mutex_lock(&perl_threads->mutex);

    for (t = perl_threads->head; NULL != t; t = t->next) {
        if (t == ithread)
            break;
    }

    /* the ithread no longer exists */
    if (t == NULL) {
        pthread_mutex_unlock(&perl_threads->mutex);
        return;
    }

    c_ithread_destroy(ithread);

    pthread_mutex_unlock(&perl_threads->mutex);
    return;
}

/* must be called with perl_threads->mutex locked */
static c_ithread_t *c_ithread_create(PerlInterpreter *base)
{
    dTHXa(NULL);

    assert(perl_threads != NULL);

    c_ithread_t *t = malloc(sizeof(*t));
    if (t == NULL) {
        PLUGIN_ERROR("malloc failed.");
        return NULL;
    }
    memset(t, 0, sizeof(c_ithread_t));

    t->interp = (NULL == base) ? NULL : perl_clone(base, CLONEf_KEEP_PTR_TABLE);

    aTHX = t->interp;

    if ((base != NULL) && (PL_endav != NULL)) {
        av_clear(PL_endav);
        av_undef(PL_endav);
        PL_endav = Nullav;
    }

#ifdef NCOLLECTD_DEBUG
    ++perl_threads->number_of_threads;
#endif

    t->next = NULL;

    if (NULL == perl_threads->tail) {
        perl_threads->head = t;
        t->prev = NULL;
    } else {
        perl_threads->tail->next = t;
        t->prev = perl_threads->tail;
    }

    t->pthread = pthread_self();
    t->running = false;
    t->shutdown = false;
    perl_threads->tail = t;

    pthread_setspecific(perl_thr_key, (const void *)t);
    return t;
}

/*
 * Exported Perl API.
 */

static void _plugin_register_generic_userdata(pTHX, int type, const char *desc)
{
    int ret = 0;

    dXSARGS;

    if (items != 2) {
        PLUGIN_ERROR("Usage: NCollectd::plugin_register_%s(pluginname, subname)", desc);
        XSRETURN_EMPTY;
    }

    if (!SvOK(ST(0))) {
        PLUGIN_ERROR("NCollectd::plugin_register_%s(pluginname, subname): "
                        "Invalid pluginname",
                        desc);
        XSRETURN_EMPTY;
    }
    if (!SvOK(ST(1))) {
        PLUGIN_ERROR("NCollectd::plugin_register_%s(pluginname, subname): "
                        "Invalid subname",
                        desc);
        XSRETURN_EMPTY;
    }

    /* Use pluginname as-is to allow flush a single perl plugin */
    char *pluginname = SvPV_nolen(ST(0));

    PLUGIN_DEBUG("NCollectd::plugin_register_%s: plugin = \"perl/%s\", sub = \"%s\"",
                 desc, pluginname, SvPV_nolen(ST(1)));

    user_data_t userdata;
    memset(&userdata, 0, sizeof(userdata));
    userdata.data = strdup(SvPV_nolen(ST(1)));
    userdata.free_func = free;

    if (PLUGIN_READ == type) {
        ret = plugin_register_complex_read("perl", pluginname, perl_read, plugin_get_interval(),
                                           &userdata);
    } else if (PLUGIN_WRITE == type) {
        // FIXME
        ret = plugin_register_write("perl", pluginname, perl_write, NULL, 0, 0, &userdata);
    } else if (PLUGIN_LOG == type) {
        ret = plugin_register_log("perl", pluginname, perl_log, &userdata); // FIXME
    } else if (PLUGIN_NOTIF == type) {
        ret = plugin_register_notification("perl", pluginname, perl_notify, &userdata);
#if 0
    } else if (PLUGIN_FLUSH == type) {
        ret = plugin_register_flush("perl", pluginname, perl_flush, plugin_get_interval(), &userdata);
#endif
    } else {
        ret = -1;
    }

    if (ret == 0)
        XSRETURN_YES;
    else
        XSRETURN_EMPTY;
}

/*
 * NCollectd::plugin_register_TYPE (pluginname, subname).
 *
 * pluginname:
 *   name of the perl plugin
 *
 * subname:
 *   name of the plugin's subroutine that does the work
 */

static XS(ncollectd_plugin_register_read)
{
    _plugin_register_generic_userdata(aTHX, PLUGIN_READ, "read");
}

static XS(ncollectd_plugin_register_write)
{
    _plugin_register_generic_userdata(aTHX, PLUGIN_WRITE, "write");
}

static XS(ncollectd_plugin_register_log)
{
    _plugin_register_generic_userdata(aTHX, PLUGIN_LOG, "log");
}

static XS(ncollectd_plugin_register_notification)
{
    _plugin_register_generic_userdata(aTHX, PLUGIN_NOTIF, "notification");
}

typedef int perl_unregister_function_t(const char *name);

static void _plugin_unregister_generic(pTHX, perl_unregister_function_t *unreg, const char *desc)
{
    dXSARGS;

    if (items != 1) {
        PLUGIN_ERROR("Usage: NCollectd::plugin_unregister_%s(pluginname)", desc);
        XSRETURN_EMPTY;
    }

    if (!SvOK(ST(0))) {
        PLUGIN_ERROR("NCollectd::plugin_unregister_%s(pluginname): Invalid pluginname",
                        desc);
        XSRETURN_EMPTY;
    }

    PLUGIN_DEBUG("NCollectd::plugin_unregister_%s: plugin = \"%s\"", desc, SvPV_nolen(ST(0)));

    unreg(SvPV_nolen(ST(0)));

    XSRETURN_EMPTY;
}

/*
 * NCollectd::plugin_unregister_TYPE (pluginname).
 *
 * TYPE:
 *   type of callback to be unregistered: read, write, log, notification, flush
 *
 * pluginname:
 *   name of the perl plugin
 */

static XS(ncollectd_plugin_unregister_read)
{
    _plugin_unregister_generic(aTHX, plugin_unregister_read, "read");
}

static XS(ncollectd_plugin_unregister_write)
{
    _plugin_unregister_generic(aTHX, plugin_unregister_write, "write");
}

static XS(ncollectd_plugin_unregister_log)
{
    _plugin_unregister_generic(aTHX, plugin_unregister_log, "log");
}

static XS(ncollectd_plugin_unregister_notification)
{
    _plugin_unregister_generic(aTHX, plugin_unregister_notification, "notification");
}

/*
 * NCollectd::plugin_dispatch_metric_family (name, values).
 *
 * name:
 *   name of the plugin
 *
 * values:
 *   value list to submit
 */
static XS(ncollectd_plugin_dispatch_metric_family)
{
    dXSARGS;

    if (1 != items) {
        PLUGIN_ERROR("Usage: NCollectd::plugin_dispatch_metric_family(values)");
        XSRETURN_EMPTY;
    }

    PLUGIN_DEBUG("NCollectd::plugin_dispatch_metric_family: values=\"%s\"",
                  SvPV_nolen(ST(/* stack index = */ 0)));

    SV *values = ST(/* stack index = */ 0);

    if (values == NULL)
        XSRETURN_EMPTY;

    /* Make sure the argument is a hash reference. */
    if (!(SvROK(values) && (SVt_PVHV == SvTYPE(SvRV(values))))) {
        PLUGIN_ERROR("NCollectd::plugin_dispatch_metric_family: Invalid values.");
        XSRETURN_EMPTY;
    }

    int ret = pplugin_dispatch_metric_family(aTHX_(HV *) SvRV(values));
    if (ret == 0)
        XSRETURN_YES;
    else
        XSRETURN_EMPTY;
}

/*
 * NCollectd::plugin_get_interval ().
 */
static XS(ncollectd_plugin_get_interval)
{
    dXSARGS;

    /* make sure we don't get any unused variable warnings for 'items';
     * don't abort, though */
    if (items)
        PLUGIN_ERROR("Usage: NCollectd::plugin_get_interval()");

    XSRETURN_NV((NV)CDTIME_T_TO_DOUBLE(plugin_get_interval()));
}

/*
 * NCollectd::plugin_dispatch_notification (notif).
 *
 * notif:
 *   notification to dispatch
 */
static XS(ncollectd_plugin_dispatch_notification)
{
    dXSARGS;

    if (items != 1) {
        PLUGIN_ERROR("Usage: NCollectd::plugin_dispatch_notification(notif)");
        XSRETURN_EMPTY;
    }

    PLUGIN_DEBUG("NCollectd::plugin_dispatch_notification: notif = \"%s\"", SvPV_nolen(ST(0)));

    SV *notif = ST(0);
    if (!(SvROK(notif) && (SVt_PVHV == SvTYPE(SvRV(notif))))) {
        PLUGIN_ERROR("NCollectd::plugin_dispatch_notification: Invalid notif.");
        XSRETURN_EMPTY;
    }

    int ret = pplugin_dispatch_notification(aTHX_(HV *) SvRV(notif));
    if (ret == 0)
        XSRETURN_YES;
    else
        XSRETURN_EMPTY;
}

/*
 * NCollectd::plugin_log (level, message).
 *
 * level:
 *   log level (LOG_DEBUG, ... LOG_ERR)
 *
 * message:
 *   log message
 */
static XS(ncollectd_plugin_log)
{
    dXSARGS;

    if (items != 2) {
        PLUGIN_ERROR("Usage: NCollectd::plugin_log(level, message)");
        XSRETURN_EMPTY;
    }

    plugin_log(SvIV(ST(0)), NULL, 0, NULL, "%s", SvPV_nolen(ST(1)));
    XSRETURN_YES;
}

/*
 * NCollectd::call_by_name (...).
 *
 * Call a Perl sub identified by its name passed through $NCollectd::cb_name.
 */
static XS(ncollectd_call_by_name)
{
    SV *tmp = get_sv("NCollectd::cb_name", 0);
    if (tmp == NULL) {
        sv_setpv(get_sv("@", 1), "cb_name has not been set");
        CLEAR_STACK_FRAME;
        return;
    }

    char *name = SvPV_nolen(tmp);
    if (get_cv(name, 0) == NULL) {
        sv_setpvf(get_sv("@", 1), "unknown callback \"%s\"", name);
        CLEAR_STACK_FRAME;
        return;
    }

    /* simply pass on the subroutine call without touching the stack,
     * thus leaving any arguments and return values in place */
    call_pv(name, 0);
}

/*
 * Interface to ncollectd.
 */

static int perl_init(void)
{
    dTHX;

    if (perl_threads == NULL)
        return 0;

    if (aTHX == NULL) {
        c_ithread_t *t = NULL;

        pthread_mutex_lock(&perl_threads->mutex);
        t = c_ithread_create(perl_threads->head->interp);
        pthread_mutex_unlock(&perl_threads->mutex);
        if (t == NULL)
            return 0;

        aTHX = t->interp;
    }

    PLUGIN_DEBUG("c_ithread: interp = %p (active threads: %i)", (void *)aTHX,
                 perl_threads->number_of_threads);

    /* Lock the base thread to avoid race conditions with c_ithread_create().
     * See https://github.com/collectd/collectd/issues/9 and
     *       https://github.com/collectd/collectd/issues/1706 for details.
     */
    assert(perl_threads->head->interp == aTHX);
    pthread_mutex_lock(&perl_threads->mutex);

    int status = pplugin_call(aTHX_ PLUGIN_INIT);

    pthread_mutex_unlock(&perl_threads->mutex);

    return status;
}

static int perl_read(user_data_t *user_data)
{
    dTHX;

    if (perl_threads == NULL)
        return 0;

    if (aTHX == NULL) {
        c_ithread_t *t = NULL;

        pthread_mutex_lock(&perl_threads->mutex);
        t = c_ithread_create(perl_threads->head->interp);
        pthread_mutex_unlock(&perl_threads->mutex);
        if (t == NULL)
            return 0;

        aTHX = t->interp;
    }

    /* Assert that we're not running as the base thread. Otherwise, we might
     * run into concurrency issues with c_ithread_create(). See
     * https://github.com/collectd/collectd/issues/9 for details. */
    assert(perl_threads->head->interp != aTHX);
    PLUGIN_DEBUG("perl_read: c_ithread: interp = %p (active threads: %i)", (void *)aTHX,
                 perl_threads->number_of_threads);

    return pplugin_call(aTHX_ PLUGIN_READ, user_data->data);
}

static int perl_write(metric_family_t const *fam, user_data_t *user_data)
{
    dTHX;

    if (perl_threads == NULL)
        return 0;

    if (aTHX == NULL) {
        c_ithread_t *t = NULL;

        pthread_mutex_lock(&perl_threads->mutex);
        t = c_ithread_create(perl_threads->head->interp);
        pthread_mutex_unlock(&perl_threads->mutex);
        if (t == NULL)
            return 0;

        aTHX = t->interp;
    }

    /* Lock the base thread if this is not called from one of the read threads
     * to avoid race conditions with c_ithread_create(). See
     * https://github.com/collectd/collectd/issues/9 for details. */
    if (perl_threads->head->interp == aTHX)
        pthread_mutex_lock(&perl_threads->mutex);

    PLUGIN_DEBUG("perl_write: c_ithread: interp = %p (active threads: %i)", (void *)aTHX,
                  perl_threads->number_of_threads);
    int status = pplugin_call(aTHX_ PLUGIN_WRITE, user_data->data, fam);

    if (perl_threads->head->interp == aTHX)
        pthread_mutex_unlock(&perl_threads->mutex);

    return status;
}

static void perl_log(const log_msg_t *msg, user_data_t *user_data)
{
    dTHX;

    if (perl_threads == NULL)
        return;

    if (aTHX == NULL) {
        c_ithread_t *t = NULL;

        pthread_mutex_lock(&perl_threads->mutex);
        t = c_ithread_create(perl_threads->head->interp);
        pthread_mutex_unlock(&perl_threads->mutex);
        if (t == NULL)
            return;

        aTHX = t->interp;
    }

    /* Lock the base thread if this is not called from one of the read threads
     * to avoid race conditions with c_ithread_create(). See
     * https://github.com/collectd/collectd/issues/9 for details.
     */

    if (perl_threads->head->interp == aTHX)
        pthread_mutex_lock(&perl_threads->mutex);

    pplugin_call(aTHX_ PLUGIN_LOG, user_data->data, msg->severity, msg->msg);

    if (perl_threads->head->interp == aTHX)
        pthread_mutex_unlock(&perl_threads->mutex);

    return;
}

static int perl_notify(const notification_t *notif, user_data_t *user_data)
{
    dTHX;

    if (perl_threads == NULL)
        return 0;

    if (aTHX == NULL) {
        c_ithread_t *t = NULL;

        pthread_mutex_lock(&perl_threads->mutex);
        t = c_ithread_create(perl_threads->head->interp);
        pthread_mutex_unlock(&perl_threads->mutex);
        if (t == NULL)
            return 0;

        aTHX = t->interp;
    }
    return pplugin_call(aTHX_ PLUGIN_NOTIF, user_data->data, notif);
}

static int perl_shutdown(void)
{
    dTHX;

    plugin_unregister_config("perl");
//  plugin_unregister_read_group("perl"); // FIXME

    if (perl_threads == NULL)
        return 0;

    c_ithread_t *t = NULL;

    if (aTHX == NULL) {
        pthread_mutex_lock(&perl_threads->mutex);
        t = c_ithread_create(perl_threads->head->interp);
        pthread_mutex_unlock(&perl_threads->mutex);
        if (t == NULL)
            return 0;

        aTHX = t->interp;
    }

    PLUGIN_DEBUG("c_ithread: interp = %p (active threads: %i)", (void *)aTHX,
                 perl_threads->number_of_threads);

    plugin_unregister_init("perl");

    int ret = pplugin_call(aTHX_ PLUGIN_SHUTDOWN);

    pthread_mutex_lock(&perl_threads->mutex);
    t = perl_threads->tail;

    while (t != NULL) {
        struct timespec ts_wait;
        c_ithread_t *thr = t;

        /* the pointer has to be advanced before destroying
         * the thread as this will free the memory */
        t = t->prev;

        thr->shutdown = true;
        if (thr->running) {
            /* Give some time to thread to exit from Perl interpreter */
            PLUGIN_WARNING("Thread is running inside Perl. Waiting.");
            ts_wait.tv_sec = 0;
            ts_wait.tv_nsec = 500000;
            nanosleep(&ts_wait, NULL);
        }
        if (thr->running) {
            pthread_kill(thr->pthread, SIGTERM);
            PLUGIN_ERROR("Thread hangs inside Perl. Thread killed.");
        }
        c_ithread_destroy(thr);
    }

    pthread_mutex_unlock(&perl_threads->mutex);
    pthread_mutex_destroy(&perl_threads->mutex);
    pthread_mutexattr_destroy(&perl_threads->mutexattr);

    free(perl_threads);

    pthread_key_delete(perl_thr_key);

    PERL_SYS_TERM();

    plugin_unregister_shutdown("perl");
    return ret;
}

/*
 * Access functions for global variables.
 *
 * These functions implement the "magic" used to access
 * the global variables from Perl.
 */
#if 0
static int g_pv_get(pTHX_ SV *var, MAGIC *mg)
{
    char *pv = mg->mg_ptr;
    sv_setpv(var, pv);
    return 0;
}

static int g_pv_set(pTHX_ SV *var, MAGIC *mg)
{
    char *pv = mg->mg_ptr;
    sstrncpy(pv, SvPV_nolen(var), DATA_MAX_NAME_LEN);
    return 0;
}
#endif

static int g_interval_get(pTHX_ SV *var, __attribute__((unused)) MAGIC *mg)
{
    PLUGIN_WARNING("Accessing $interval_g is deprecated (and might not "
                   "give the desired results) - plugin_get_interval() should be used instead.");
    cdtime_t interval = plugin_get_interval();
    sv_setnv(var, CDTIME_T_TO_DOUBLE(interval));
    return 0;
}

static int g_interval_set(pTHX_ SV *var, __attribute__((unused)) MAGIC *mg)
{
    double nv = (double)SvNV(var);
    PLUGIN_WARNING("Accessing $interval_g is deprecated (and might not "
                   "give the desired results) - plugin_get_interval() should be used instead.");

    cdtime_t interval = DOUBLE_TO_CDTIME_T(nv); // FIXME
    (void)interval;
    return 0;
}
#if 0
static MGVTBL g_pv_vtbl = {g_pv_get, g_pv_set, NULL, NULL, NULL, NULL, NULL };
#endif
static MGVTBL g_interval_vtbl = {g_interval_get, g_interval_set, NULL, NULL, NULL, NULL, NULL};

/* bootstrap the Collectd module */
static void xs_init(pTHX)
{
    char *file = __FILE__;

    dXSUB_SYS;

    /* enable usage of Perl modules using shared libraries */
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);

    /* register API */
    for (int i = 0; NULL != api[i].f; ++i) {
        newXS(api[i].name, api[i].f, file);
    }

    HV *stash = gv_stashpv("Collectd", 1);

    /* export "constants" */
    for (int i = 0; constants[i].name[0] != '\0'; ++i) {
        newCONSTSUB(stash, constants[i].name, newSViv(constants[i].value));
    }

    /* export global variables
     * by adding "magic" to the SV's representing the globale variables
     * perl is able to automagically call the get/set function when
     * accessing any such variable (this is basically the same as using
     * tie() in Perl) */
    /* global strings */
#if 0
    struct {
        char name[64];
        char *var;
    } g_strings[] = {{"NCollectd::hostname_g", hostname_g}, {"", NULL}};

    SV *tmp = NULL;
    for (int i = 0; '\0' != g_strings[i].name[0]; ++i) {
        tmp = get_sv(g_strings[i].name, 1);
        sv_magicext(tmp, NULL, PERL_MAGIC_ext, &g_pv_vtbl, g_strings[i].var, 0);
    }
#endif

    SV *tmp = get_sv("NCollectd::interval_g", /* create = */ 1);
    sv_magicext(tmp, NULL, /* how = */ PERL_MAGIC_ext,
                            /* vtbl = */ &g_interval_vtbl,
                            /* name = */ NULL, /* namelen = */ 0);
    return;
}

/* Initialize the global Perl interpreter. */
static int init_pi(int argc, char **argv)
{
    dTHXa(NULL);

    if (perl_threads != NULL)
        return 0;

    PLUGIN_INFO("Initializing Perl interpreter...");
#ifdef NCOLLECTD_DEBUG
    {
        for (int i = 0; i < argc; ++i)
            PLUGIN_DEBUG("argv[%i] = \"%s\"", i, argv[i]);
    }
#endif

    if (pthread_key_create(&perl_thr_key, c_ithread_destructor) != 0) {
        PLUGIN_ERROR("init_pi: pthread_key_create failed");

        /* this must not happen - cowardly giving up if it does */
        return -1;
    }

#ifdef __FreeBSD__
    /* On FreeBSD, PERL_SYS_INIT3 expands to some expression which
     * triggers a "value computed is not used" warning by gcc. */
    (void)
#endif
            PERL_SYS_INIT3(&argc, &argv, &environ);

    perl_threads = malloc(sizeof(*perl_threads));
    if (perl_threads == NULL)
        return 0;
    memset(perl_threads, 0, sizeof(c_ithread_list_t));

    pthread_mutexattr_init(&perl_threads->mutexattr);
    pthread_mutexattr_settype(&perl_threads->mutexattr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&perl_threads->mutex, &perl_threads->mutexattr);
    /* locking the mutex should not be necessary at this point
     * but let's just do it for the sake of completeness */
    pthread_mutex_lock(&perl_threads->mutex);

    perl_threads->head = c_ithread_create(NULL);
    if (perl_threads->head == NULL) {
        pthread_mutex_unlock(&perl_threads->mutex);
        return -1;
    }
    perl_threads->tail = perl_threads->head;

    if ((perl_threads->head->interp = perl_alloc()) == NULL) {
        PLUGIN_ERROR("init_pi: Not enough memory.");
        exit(3);
    }

    aTHX = perl_threads->head->interp;
    pthread_mutex_unlock(&perl_threads->mutex);

    perl_construct(aTHX);

    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;

    if (perl_parse(aTHX_ xs_init, argc, argv, NULL) != 0) {
        SV *err = get_sv("@", 1);
        PLUGIN_ERROR("init_pi: Unable to bootstrap NCollectd: %s", SvPV_nolen(err));

        perl_destruct(perl_threads->head->interp);
        perl_free(perl_threads->head->interp);
        free(perl_threads);

        pthread_key_delete(perl_thr_key);
        return -1;
    }

    /* Set $0 to "ncollectd" because perl_parse() has to set it to "-e". */
    sv_setpv(get_sv("0", 0), "ncollectd");

    perl_run(aTHX);

    plugin_register_init("perl", perl_init);
    plugin_register_shutdown("perl", perl_shutdown);
    return 0;
}

/* load-plugin "plugin" */
static int perl_config_loadplugin(pTHX_ config_item_t *ci)
{
    char module_name[DATA_MAX_NAME_LEN];

    if ((ci->children_num != 0) || (ci->values_num != 1) ||
        (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("load-plugin expects a single string argument.");
        return 1;
    }

    char *value = ci->values[0].value.string;

    if (get_module_name(module_name, sizeof(module_name), value) == NULL) {
        PLUGIN_ERROR("Invalid module name %s", value);
        return 1;
    }

    if (init_pi(perl_argc, perl_argv) != 0)
        return -1;

    if (perl_threads == NULL) {
        PLUGIN_ERROR("perl_threads is NULL.");
        return -1;
    }

    if (perl_threads->head == NULL) {
        PLUGIN_ERROR("perl_threads->head is NULL.");
        return -1;
    }

    aTHX = perl_threads->head->interp;

    PLUGIN_DEBUG("perl_config: Loading Perl plugin \"%s\"", value);
    load_module(PERL_LOADMOD_NOIMPORT, newSVpv(module_name, strlen(module_name)), Nullsv);
    return 0;
}

/* base-name "name" */
static int perl_config_basename(pTHX_ config_item_t *ci)
{
    if ((ci->children_num != 0) || (ci->values_num != 1) ||
        (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("base-name expects a single string argument.");
        return 1;
    }

    char *value = ci->values[0].value.string;

    PLUGIN_DEBUG("perl_config: Setting plugin basename to \"%s\"", value);
    sstrncpy(base_name, value, sizeof(base_name));
    return 0;
}

/* enable-debugger "package"|"" */
static int perl_config_enabledebugger(pTHX_ config_item_t *ci)
{

    if ((ci->children_num != 0) || (ci->values_num != 1) ||
        (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("enable-debugger expects a single string argument.");
        return 1;
    }

    if (perl_threads != NULL) {
        PLUGIN_WARNING("enable-debugger has no effects if used after LoadPlugin.");
        return 1;
    }

    char *value = ci->values[0].value.string;

    perl_argv = realloc(perl_argv, (++perl_argc + 1) * sizeof(char *));
    if (perl_argv == NULL) {
        PLUGIN_ERROR("perl_config: Not enough memory.");
        exit(3); // FIXME
    }

    if (value[0] == '\0') {
        perl_argv[perl_argc - 1] = "-d";
    } else {
        perl_argv[perl_argc - 1] = malloc(strlen(value) + 5);
        if (perl_argv[perl_argc - 1] == NULL) {
            PLUGIN_ERROR("perl_config: Not enough memory.");
            exit(3); // FIXME
        }
        sstrncpy(perl_argv[perl_argc - 1], "-d:", 4);
        sstrncpy(perl_argv[perl_argc - 1] + 3, value, strlen(value) + 1);
    }

    perl_argv[perl_argc] = NULL;
    return 0;
}

static int perl_config_includedir(pTHX_ config_item_t *ci)
{
    if ((ci->children_num != 0) || (ci->values_num != 1) ||
        (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("include-dir expects a single string argument.");
        return 1;
    }

    char *value = ci->values[0].value.string;

    if (aTHX == NULL) {
        perl_argv = realloc(perl_argv, (++perl_argc + 1) * sizeof(char *));
        if (perl_argv == NULL) {
            PLUGIN_ERROR("perl_config: Not enough memory.");
            exit(3); // FIXME
        }

        perl_argv[perl_argc - 1] = malloc(strlen(value) + 3);
        if (perl_argv[perl_argc - 1] == NULL) {
            PLUGIN_ERROR("perl_config: Not enough memory.");
            exit(3); // FIXME
        }
        sstrncpy(perl_argv[perl_argc - 1], "-I", 3);
        sstrncpy(perl_argv[perl_argc - 1] + 2, value, strlen(value) + 1);

        perl_argv[perl_argc] = NULL;
    } else {
        /* prepend the directory to @INC */
        av_unshift(GvAVn(PL_incgv), 1);
        av_store(GvAVn(PL_incgv), 0, newSVpv(value, strlen(value)));
    }
    return 0;
}

static int perl_config_plugin(pTHX_ config_item_t *ci)
{
    int retvals = 0;
    int ret = 0;

    char *plugin;
    HV *config;

    if (perl_threads == NULL) {
        PLUGIN_ERROR("A `Plugin' block was encountered but no plugin was loaded yet. "
                "Put the appropriate `LoadPlugin' option in front of it.");
        return -1;
    }

    dSP;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("LoadPlugin expects a single string argument.");
        return 1;
    }

    plugin = ci->values[0].value.string;
    config = newHV();

    if (config_item2hv(aTHX_ ci, config) != 0) {
        hv_clear(config);
        hv_undef(config);

        PLUGIN_ERROR("Unable to convert configuration to a Perl hash value.");
        config = (HV *)&PL_sv_undef;
    }

    ENTER;
    SAVETMPS;

    PUSHMARK(SP);

    XPUSHs(sv_2mortal(newSVpv(plugin, 0)));
    XPUSHs(sv_2mortal(newRV_noinc((SV *)config)));

    PUTBACK;

    retvals = call_pv("NCollectd::_plugin_dispatch_config", G_SCALAR);

    SPAGAIN;
    if (retvals > 0) {
        SV *tmp = POPs;
        if (!SvTRUE(tmp))
            ret = 1;
    } else {
        ret = 1;
    }

    PUTBACK;
    FREETMPS;
    LEAVE;
    return ret;
}

static int perl_config(config_item_t *ci)
{
    int status = 0;
    dTHXa(NULL);

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (perl_threads != NULL) {
            if ((aTHX = PERL_GET_CONTEXT) == NULL)
                return -1;
        }

        if (strcasecmp(child->key, "load-plugin") == 0) {
            status = perl_config_loadplugin(aTHX_ child);
        } else if (strcasecmp(child->key, "base-name") == 0) {
            status = perl_config_basename(aTHX_ child);
        } else if (strcasecmp(child->key, "enable-debugger") == 0) {
            status = perl_config_enabledebugger(aTHX_ child);
        } else if (strcasecmp(child->key, "include-dir") == 0) {
            status = perl_config_includedir(aTHX_ child);
        } else if (strcasecmp(child->key, "plugin") == 0) {
            status = perl_config_plugin(aTHX_ child);
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

void module_register(void)
{
    perl_argc = 4;
    perl_argv = malloc((perl_argc + 1) * sizeof(*perl_argv));
    if (perl_argv == NULL) {
        PLUGIN_ERROR("malloc failed.");
        return;
    }

    /* default options for the Perl interpreter */
    perl_argv[0] = "";
    perl_argv[1] = "-MNCollectd";
    perl_argv[2] = "-e";
    perl_argv[3] = "1";
    perl_argv[4] = NULL;

    plugin_register_config("perl", perl_config);
}
