// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "libmdb/mql.h"
#include "libmdb/value.h"
#include "libmdb/functions.h"

typedef enum {
    CALL_DAYS_IN_MONTH,
    CALL_DAY_OF_MONTH,
    CALL_DAY_OF_WEEK,
    CALL_HOUR,
    CALL_MINUTE,
    CALL_MONTH,
    CALL_YEAR,
} call_date_e;

static mql_value_t *mql_eval_date_vector(__attribute__((unused)) void *ctx,
                                           size_t argc, mql_value_t *argv, call_date_e call)
{
    mql_value_t *value = NULL;

    if (argc == 0) {
        value = mql_value_samples_dup(&argv[0], true);
        if (value == NULL)
            return NULL;
        // time()

    } else {
        if ((argc != 1) || (argv[0].kind != MQL_VALUE_SAMPLES)) {
            return NULL;
        }

        value = mql_value_samples_dup(&argv[0], true);
        if (value == NULL)
            return NULL;

        if (value->samples.size == 0) {
            // FIXME
        }
    }

    for (size_t i = 0; i < value->samples.size ; i++) {
        mql_point_t *src = &argv[0].samples.ptr[i].point;
        mql_point_t *dst = &value->samples.ptr[i].point;

        time_t timestamp = src->timestamp;
        struct tm tm;

        gmtime_r(&timestamp, &tm);

        switch(call) {
            case CALL_DAYS_IN_MONTH:
                switch(tm.tm_mon) {
                case 1: {
                    int year =  tm.tm_year + 1900;
                    dst->value = (((year % 4 == 0) && (year % 400 == 0)) || (year % 100 != 0)) ? 29 : 28;
                    break;
                }
                case 0:
                case 2:
                case 4:
                case 6:
                case 7:
                case 9:
                case 11:
                    dst->value = 31;
                    break;
                default:
                    dst->value = 30;
                    break;
                }
                break;
            case CALL_DAY_OF_MONTH:
                dst->value = tm.tm_mday;
                break;
            case CALL_DAY_OF_WEEK:
                dst->value = tm.tm_wday;
                break;
            case CALL_HOUR:
                dst->value = tm.tm_hour;
                break;
            case CALL_MINUTE:
                dst->value = tm.tm_min;
                break;
            case CALL_MONTH:
                dst->value = tm.tm_mon + 1;
                break;
            case CALL_YEAR:
                dst->value = tm.tm_year + 1900;
                break;
        }
        dst->timestamp = src->timestamp;
    }

    return value;
}

static mql_value_t *mql_eval_double_vector(__attribute__((unused)) void *ctx,
                                             size_t argc, mql_value_t *argv, double (*call)(double))
{
    if (argc != 1) {
        return NULL;
    }

    if (argv[0].kind != MQL_VALUE_SAMPLES) {
        return NULL;
    }

    mql_value_t *value = mql_value_samples_dup(&argv[0], true);
    if (value == NULL)
        return NULL;

    for (size_t i = 0; i < value->samples.size ; i++) {
        mql_point_t *src = &argv[0].samples.ptr[i].point;
        mql_point_t *dst = &value->samples.ptr[i].point;
        dst->value = call(src->value);
        dst->timestamp = src->timestamp;
    }

    return value;
}

#if 0
static mql_value_t *mql_eval_aggregate_over_time(__attribute__((unused)) void *ctx,
                                                   __attribute__((unused)) size_t argc,
                                                   __attribute__((unused)) mql_value_t *argv,
                                                   __attribute__((unused)) double (*call)(mql_point_t *, size_t))
{
#if 0
    if ((argc != 1) || (argv[0].kind != MQL_VALUE_SERIES)) {
        return NULL;
    }
#endif
    return 0;
}
#endif

static mql_value_t *mql_eval_abs(void *ctx, size_t argc, mql_value_t *argv)
{
// "abs",                                MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return mql_eval_double_vector(ctx, argc, argv, &fabs);
}

static mql_value_t *mql_eval_absent(__attribute__((unused)) void *ctx,
                                      size_t argc, mql_value_t *argv)
{
// "absent",                         MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    if ((argc != 1) || (argv[0].kind != MQL_VALUE_SAMPLES)) {
        return NULL;
    }
 // createLabelsForAbsentFunction
    return NULL;
}

static mql_value_t *mql_eval_absent_over_time(__attribute__((unused)) void *ctx,
                                                __attribute__((unused)) size_t argc,
                                                __attribute__((unused)) mql_value_t *argv)
{
// "absent_over_time",   MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_avg_over_time(__attribute__((unused)) void *ctx,
                                             __attribute__((unused)) size_t argc,
                                             __attribute__((unused)) mql_value_t *argv)
{
// "avg_over_time",          MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE



 return NULL;
}

static mql_value_t *mql_eval_ceil(void *ctx, size_t argc, mql_value_t *argv)
{
// "ceil",                           MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return mql_eval_double_vector(ctx, argc, argv, &ceil);
}

static mql_value_t *mql_eval_changes(__attribute__((unused)) void *ctx,
                                       __attribute__((unused)) size_t argc,
                                       __attribute__((unused)) mql_value_t *argv)
{
// "changes",                        MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NON
    return NULL;
}

static mql_value_t *mql_eval_clamp(__attribute__((unused)) void *ctx,
                                     size_t argc, mql_value_t *argv)
{
// "clamp",                          MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_SCALAR, MQL_VALUE_SCALAR, MQL_VALUE_NONE, MQL_VALUE_NONE
    if ((argc != 3) || (argv[0].kind != MQL_VALUE_SAMPLES) || (argv[1].kind != MQL_VALUE_SCALAR) || (argv[2].kind != MQL_VALUE_SCALAR)) {
        return NULL;
    }
    double min = argv[1].scalar.value;
    double max = argv[2].scalar.value;
    if (max < min ) {
        return NULL;
    }

    mql_value_t *value = mql_value_samples_dup(&argv[0], true);
    if (value == NULL)
        return NULL;

    for (size_t i = 0; i < value->samples.size ; i++) {
        mql_point_t *src = &argv[0].samples.ptr[i].point;
        mql_point_t *dst = &value->samples.ptr[i].point;
        dst->value = fmax(min, fmin(max, src->value));
        dst->timestamp = src->timestamp;
    }

    return value;
}

static mql_value_t *mql_eval_clamp_max(__attribute__((unused)) void *ctx,
                                         size_t argc, mql_value_t *argv)
{
// "clamp_max",                  MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_SCALAR, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    if ((argc != 2) || (argv[0].kind != MQL_VALUE_SAMPLES) || (argv[1].kind != MQL_VALUE_SCALAR)) {
        return NULL;
    }
    double max = argv[1].scalar.value;

    mql_value_t *value = mql_value_samples_dup(&argv[0], true);
    if (value == NULL)
        return NULL;

    for (size_t i = 0; i < value->samples.size ; i++) {
        mql_point_t *src = &argv[0].samples.ptr[i].point;
        mql_point_t *dst = &value->samples.ptr[i].point;
        dst->value = fmax(max, src->value);
        dst->timestamp = src->timestamp;
    }

    return value;
}

static mql_value_t *mql_eval_clamp_min(__attribute__((unused)) void *ctx,
                                         size_t argc, mql_value_t *argv)
{
// "clamp_min",                  MQL_VALUE_SAMPLES, 0, MQL_VALUE_SCALAR, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    if ((argc != 2) || (argv[0].kind != MQL_VALUE_SAMPLES) || (argv[1].kind != MQL_VALUE_SCALAR)) {
        return NULL;
    }
    double min = argv[1].scalar.value;

    mql_value_t *value = mql_value_samples_dup(&argv[0], true);
    if (value == NULL)
        return NULL;

    for (size_t i = 0; i < value->samples.size ; i++) {
        mql_point_t *src = &argv[0].samples.ptr[i].point;
        mql_point_t *dst = &value->samples.ptr[i].point;
        dst->value = fmin(min, src->value);
        dst->timestamp = src->timestamp;
    }

    return value;
}

static mql_value_t *mql_eval_count_over_time(__attribute__((unused)) void *ctx,
                                               __attribute__((unused)) size_t argc,
                                               __attribute__((unused)) mql_value_t *argv)
{
// "count_over_time",        MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_days_in_month(void *ctx, size_t argc, mql_value_t *argv)
{
// "days_in_month",          MQL_VALUE_SAMPLES, 1, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return mql_eval_date_vector(ctx, argc, argv, CALL_DAYS_IN_MONTH);
}

static mql_value_t *mql_eval_day_of_month(__attribute__((unused)) void *ctx,
                                            __attribute__((unused)) size_t argc,
                                            __attribute__((unused)) mql_value_t *argv)
{
// "day_of_month",           MQL_VALUE_SAMPLES, 1, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return mql_eval_date_vector(ctx, argc, argv, CALL_DAY_OF_MONTH);
}

static mql_value_t *mql_eval_day_of_week(__attribute__((unused)) void *ctx,
                                           __attribute__((unused)) size_t argc,
                                           __attribute__((unused)) mql_value_t *argv)
{
// "day_of_week",                MQL_VALUE_SAMPLES, 1, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return mql_eval_date_vector(ctx, argc, argv, CALL_DAY_OF_WEEK);
}

static mql_value_t *mql_eval_delta(__attribute__((unused)) void *ctx,
                                     __attribute__((unused)) size_t argc,
                                     __attribute__((unused)) mql_value_t *argv)
{
// "delta",                          MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_deriv(__attribute__((unused)) void *ctx,
                                     __attribute__((unused)) size_t argc,
                                     __attribute__((unused)) mql_value_t *argv)
{
// "deriv",                          MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_exp(void *ctx, size_t argc, mql_value_t *argv)
{
// "exp",                                MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return mql_eval_double_vector(ctx, argc, argv, &exp);
}

static mql_value_t *mql_eval_floor(void *ctx, size_t argc, mql_value_t *argv)
{
// "floor",                          MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return mql_eval_double_vector(ctx, argc, argv, &floor);
}

static mql_value_t *mql_eval_histogram_quantile(__attribute__((unused)) void *ctx,
                                                  __attribute__((unused)) size_t argc,
                                                  __attribute__((unused))  mql_value_t *argv)
{
// "histogram_quantile", MQL_VALUE_SAMPLES, 0, MQL_VALUE_SCALAR, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_holt_winters(__attribute__((unused)) void *ctx,
                                            __attribute__((unused)) size_t argc,
                                            __attribute__((unused)) mql_value_t *argv)
{
// "holt_winters",           MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_SCALAR, MQL_VALUE_SCALAR, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_hour(void *ctx, size_t argc, mql_value_t *argv)
{
// "hour",                           MQL_VALUE_SAMPLES, 1, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return mql_eval_date_vector(ctx, argc, argv, CALL_HOUR);
}

static mql_value_t *mql_eval_idelta(__attribute__((unused)) void *ctx,
                                      __attribute__((unused)) size_t argc,
                                      __attribute__((unused)) mql_value_t *argv)
{
// "idelta",                         MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_increase(__attribute__((unused)) void *ctx,
                                      __attribute__((unused)) size_t argc,
                                      __attribute__((unused)) mql_value_t *argv)
{
// "increase",                   MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_irate(__attribute__((unused)) void *ctx,
                                   __attribute__((unused)) size_t argc,
                                   __attribute__((unused)) mql_value_t *argv)
{
// "irate",                          MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_label_replace(__attribute__((unused)) void *ctx,
                                           __attribute__((unused)) size_t argc,
                                           __attribute__((unused)) mql_value_t *argv)
{
// "label_replace",          MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_SCALAR, MQL_VALUE_SCALAR, MQL_VALUE_SCALAR, MQL_VALUE_SCALAR
    return NULL;
}

#if 0
static mql_value_t *mql_eval_label_join(__attribute__((unused)) void *ctx,
                                        __attribute__((unused)) size_t argc,
                                        __attribute__((unused)) mql_value_t *argv)
{
// "label_join",                 MQL_VALUE_SAMPLES -1, MQL_VALUE_SAMPLES, MQL_VALUE_STRING, MQL_VALUE_STRING, MQL_VALUE_STRING, MQL_VALUE_NONE
    return NULL;
}
#endif

static mql_value_t *mql_eval_ln(void *ctx, size_t argc, mql_value_t *argv)
{
// "ln",                                 MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return mql_eval_double_vector(ctx, argc, argv, &log);
}

static mql_value_t *mql_eval_log10(void *ctx, size_t argc, mql_value_t *argv)
{
// "log10",                          MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return mql_eval_double_vector(ctx, argc, argv, &log10);
}

static mql_value_t *mql_eval_log2(void *ctx, size_t argc, mql_value_t *argv)
{
// "log2",                           MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return mql_eval_double_vector(ctx, argc, argv, &log2);
}

static mql_value_t *mql_eval_last_over_time(__attribute__((unused)) void *ctx,
                                              __attribute__((unused)) size_t argc,
                                              __attribute__((unused)) mql_value_t *argv)
{
// "last_over_time",         MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_max_over_time(__attribute__((unused)) void *ctx,
                                             __attribute__((unused)) size_t argc,
                                             __attribute__((unused)) mql_value_t *argv)
{
// "max_over_time",          MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_min_over_time(__attribute__((unused)) void *ctx,
                                             __attribute__((unused)) size_t argc,
                                             __attribute__((unused)) mql_value_t *argv)
{
// "min_over_time",          MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_minute(void *ctx, size_t argc, mql_value_t *argv)
{
// "minute",                         MQL_VALUE_SAMPLES, 1, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return mql_eval_date_vector(ctx, argc, argv, CALL_MINUTE);
    return NULL;
}

static mql_value_t *mql_eval_month(void *ctx, size_t argc, mql_value_t *argv)
{
// "month",                          MQL_VALUE_SAMPLES, 1, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return mql_eval_date_vector(ctx, argc, argv, CALL_MONTH);
    return NULL;
}

static mql_value_t *mql_eval_predict_linear(__attribute__((unused)) void *ctx,
                                              __attribute__((unused)) size_t argc,
                                              __attribute__((unused)) mql_value_t *argv)
{
// "predict_linear",         MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_SCALAR, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_quantile_over_time(__attribute__((unused)) void *ctx,
                                                  __attribute__((unused)) size_t argc,
                                                  __attribute__((unused)) mql_value_t *argv)
{
// "quantile_over_time", MQL_VALUE_SAMPLES, 0, MQL_VALUE_SCALAR, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_rate(__attribute__((unused)) void *ctx,
                                    __attribute__((unused)) size_t argc,
                                    __attribute__((unused)) mql_value_t *argv)
{
// "rate",                           MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_resets(__attribute__((unused)) void *ctx,
                                      __attribute__((unused)) size_t argc,
                                      __attribute__((unused)) mql_value_t *argv)
{
// "resets",                         MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_round(__attribute__((unused)) void *ctx,
                                     __attribute__((unused)) size_t argc,
                                     __attribute__((unused)) mql_value_t *argv)
{
// "round",                          MQL_VALUE_SAMPLES, 1, MQL_VALUE_SAMPLES, MQL_VALUE_SCALAR, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_scalar(__attribute__((unused)) void *ctx,
                                      __attribute__((unused)) size_t argc,
                                      __attribute__((unused)) mql_value_t *argv)
{
// "scalar",                         MQL_VALUE_SCALAR, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}


static mql_value_t *mql_eval_sgn(__attribute__((unused)) void *ctx,
                                   __attribute__((unused)) size_t argc,
                                   __attribute__((unused)) mql_value_t *argv)
{
// "sgn",                                MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
//    double sgn (double val) { return val < 0 ? -1 : 0; }
//    return mql_eval_double_vector(ctx, argc, argv, &sgn);
    return NULL;
}

static mql_value_t *mql_eval_sort(__attribute__((unused)) void *ctx,
                                    __attribute__((unused)) size_t argc,
                                    __attribute__((unused)) mql_value_t *argv)
{
// "sort",                           MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
 return NULL;
}

static mql_value_t *mql_eval_sort_desc(__attribute__((unused)) void *ctx,
                                         __attribute__((unused)) size_t argc,
                                         __attribute__((unused)) mql_value_t *argv)
{
// "sort_desc",                  MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE

    return NULL;
}

static mql_value_t *mql_eval_sqrt(void *ctx, size_t argc, mql_value_t *argv)
{
// "sqrt",                           MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return mql_eval_double_vector(ctx, argc, argv, &sqrt);
}

static mql_value_t *mql_eval_stddev_over_time(__attribute__((unused)) void *ctx,
                                                __attribute__((unused)) size_t argc,
                                                __attribute__((unused)) mql_value_t *argv)
{
// "stddev_over_time",   MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_stdvar_over_time(__attribute__((unused)) void *ctx,
                                                __attribute__((unused)) size_t argc,
                                                __attribute__((unused)) mql_value_t *argv)
{
// "stdvar_over_time",   MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_sum_over_time(__attribute__((unused)) void *ctx,
                                             __attribute__((unused)) size_t argc,
                                             __attribute__((unused)) mql_value_t *argv)
{
// "sum_over_time",          MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return NULL;
}

static mql_value_t *mql_eval_time(__attribute__((unused)) void *ctx,
                                    size_t argc,
                                    __attribute__((unused)) mql_value_t *argv)
{
// "time",                           MQL_VALUE_SCALAR, 0, MQL_VALUE_NONE,     MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    if (argc != 0) {
        return NULL;
    }

    mql_value_t *value = mql_value_scalar(0, time(NULL)); // FIXME
    if (value == NULL) {
        return NULL;
    }
    return value;
}

static mql_value_t *mql_eval_timestamp(__attribute__((unused)) void *ctx,
                                         size_t argc, mql_value_t *argv)
{
// "timestamp",                  MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    if ((argc != 1) || (argv[0].kind != MQL_VALUE_SAMPLES)) {
        return NULL;
    }

    mql_value_t *value = mql_value_samples_dup(&argv[0], true);
    if (value == NULL)
        return NULL;

    for (size_t i = 0; i < value->samples.size ; i++) {
        mql_point_t *src = &argv[0].samples.ptr[i].point;
        mql_point_t *dst = &value->samples.ptr[i].point;
        dst->value = (double)src->timestamp / 1000.0;
        dst->timestamp = src->timestamp;
    }

    return value;
    return NULL;
}

static mql_value_t *mql_eval_vector(__attribute__((unused)) void *ctx,
                                      size_t argc, mql_value_t *argv)
{
// "vector",                         MQL_VALUE_SAMPLES, 0, MQL_VALUE_SCALAR, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    if (argc != 1) {
        return NULL;
    }
    if (argv[0].kind != MQL_VALUE_SCALAR) {
        return NULL;
    }

    mql_value_t *value = mql_value_samples();
    if (value == NULL) {
        return NULL;
    }

    mql_sample_t sample = {0};
    sample.point.value = argv[0].scalar.value;

    mql_value_samples_add(value, &sample);
    return value;
}

static mql_value_t *mql_eval_year(void *ctx, size_t argc, mql_value_t *argv)
{
// "year",                           MQL_VALUE_SAMPLES, 1, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE
    return mql_eval_date_vector(ctx, argc, argv, CALL_YEAR);
}


static mql_function_t mql_functions[] = {
    { "abs",                mql_eval_abs,                MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "absent",             mql_eval_absent,             MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "absent_over_time",   mql_eval_absent_over_time,   MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "avg_over_time",      mql_eval_avg_over_time,      MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "ceil",               mql_eval_ceil,               MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "changes",            mql_eval_changes,            MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "clamp",              mql_eval_clamp,              MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_SCALAR, MQL_VALUE_SCALAR, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "clamp_max",          mql_eval_clamp_max,          MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_SCALAR, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "clamp_min",          mql_eval_clamp_min,          MQL_VALUE_SAMPLES, 0, MQL_VALUE_SCALAR, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "count_over_time",    mql_eval_count_over_time,    MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "days_in_month",      mql_eval_days_in_month,      MQL_VALUE_SAMPLES, 1, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "day_of_month",       mql_eval_day_of_month,       MQL_VALUE_SAMPLES, 1, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "day_of_week",        mql_eval_day_of_week,        MQL_VALUE_SAMPLES, 1, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "delta",              mql_eval_delta,              MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "deriv",              mql_eval_deriv,              MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "exp",                mql_eval_exp,                MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "floor",              mql_eval_floor,              MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "histogram_quantile", mql_eval_histogram_quantile, MQL_VALUE_SAMPLES, 0, MQL_VALUE_SCALAR, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "holt_winters",       mql_eval_holt_winters,       MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_SCALAR, MQL_VALUE_SCALAR, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "hour",               mql_eval_hour,               MQL_VALUE_SAMPLES, 1, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "idelta",             mql_eval_idelta,             MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "increase",           mql_eval_increase,           MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "irate",              mql_eval_irate,              MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "label_replace",      mql_eval_label_replace,      MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_SCALAR, MQL_VALUE_SCALAR, MQL_VALUE_SCALAR, MQL_VALUE_SCALAR },
//    { "label_join",         mql_eval_label_join,         MQL_VALUE_SAMPLES -1, MQL_VALUE_SAMPLES, MQL_VALUE_STRING, MQL_VALUE_STRING, MQL_VALUE_STRING, MQL_VALUE_NONE },
    { "ln",                 mql_eval_ln,                 MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "log10",              mql_eval_log10,              MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "log2",               mql_eval_log2,               MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "last_over_time",     mql_eval_last_over_time,     MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "max_over_time",      mql_eval_max_over_time,      MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "min_over_time",      mql_eval_min_over_time,      MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "minute",             mql_eval_minute,             MQL_VALUE_SAMPLES, 1, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "month",              mql_eval_month,              MQL_VALUE_SAMPLES, 1, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "predict_linear",     mql_eval_predict_linear,     MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_SCALAR, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "quantile_over_time", mql_eval_quantile_over_time, MQL_VALUE_SAMPLES, 0, MQL_VALUE_SCALAR, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "rate",               mql_eval_rate,               MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "resets",             mql_eval_resets,             MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "round",              mql_eval_round,              MQL_VALUE_SAMPLES, 1, MQL_VALUE_SAMPLES, MQL_VALUE_SCALAR, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "scalar",             mql_eval_scalar,             MQL_VALUE_SCALAR, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "sgn",                mql_eval_sgn,                MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "sort",               mql_eval_sort,               MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "sort_desc",          mql_eval_sort_desc,          MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "sqrt",               mql_eval_sqrt,               MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "stddev_over_time",   mql_eval_stddev_over_time,   MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "stdvar_over_time",   mql_eval_stdvar_over_time,   MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "sum_over_time",      mql_eval_sum_over_time,      MQL_VALUE_SAMPLES, 0, MQL_VALUE_SERIES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "time",               mql_eval_time,               MQL_VALUE_SCALAR, 0, MQL_VALUE_NONE,   MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "timestamp",          mql_eval_timestamp,          MQL_VALUE_SAMPLES, 0, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "vector",             mql_eval_vector,             MQL_VALUE_SAMPLES, 0, MQL_VALUE_SCALAR, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE },
    { "year",               mql_eval_year,               MQL_VALUE_SAMPLES, 1, MQL_VALUE_SAMPLES, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE, MQL_VALUE_NONE }
};
static size_t mql_functions_size = sizeof(mql_functions) / sizeof(mql_function_t);


mql_function_t *mql_function_get (char *name)
{
    for (size_t i=0 ; i < mql_functions_size; i++) {
        if (!strcmp(mql_functions[i].name, name)) {
            return &mql_functions[i];
        }
    }
    return NULL;
}
