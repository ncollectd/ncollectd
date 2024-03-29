// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2013-2015  Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libconfig/config.h"
#include "libmetric/metric.h"
#include "libmetric/notification.h"

static int fail_count__;
static int check_count__;

#ifndef DBL_PRECISION
#define DBL_PRECISION 1e-12
#endif

#define DEF_TEST(func) static int test_##func(void)

#define RUN_TEST(func)                                                                   \
    do {                                                                                 \
        int status;                                                                      \
        printf("Testing %s ...\n", #func);                                               \
        status = test_##func();                                                          \
        printf("%s.\n", (status == 0) ? "Success" : "FAILURE");                          \
        if (status != 0) {                                                               \
            fail_count__++;                                                              \
        }                                                                                \
    } while (0)

#define END_TEST exit((fail_count__ == 0) ? 0 : 1);

#define LOG(result, text)                                                                \
    printf("%s %i - %s\n", result ? "ok" : "not ok", ++check_count__, text)

#define OK1(cond, text)                                                                  \
    do {                                                                                 \
        bool result = (cond);                                                            \
        LOG(result, text);                                                               \
        if (!result) {                                                                   \
            return -1;                                                                   \
        }                                                                                \
    } while (0)
#define OK(cond) OK1(cond, #cond)

#define EXPECT_EQ_STR(expect, actual)                                                    \
    do {                                                                                 \
        /* Evaluate 'actual' only once. */                                               \
        const char *got__ = actual;                                                      \
        if ((expect == NULL) != (got__ == NULL)) {                                       \
            printf("not ok %i - %s = \"%s\", want \"%s\"\n", ++check_count__,            \
                         #actual, got__ ? got__ : "(null)", expect ? expect : "(null)"); \
            return -1;                                                                   \
        }                                                                                \
        if ((expect != NULL) && (strcmp(expect, got__) != 0)) {                          \
            printf("not ok %i - %s = \"%s\", want \"%s\"\n", ++check_count__,            \
                         #actual, got__, expect);                                        \
            return -1;                                                                   \
        }                                                                                \
        printf("ok %i - %s = \"%s\"\n", ++check_count__, #actual,                        \
                     got__ ? got__ : "(null)");                                          \
    } while (0)

#define EXPECT_EQ_INT_STR(expect, actual, str)                                           \
    do {                                                                                 \
        int want__ = (int)expect;                                                        \
        int got__ = (int)actual;                                                         \
        if (got__ != want__) {                                                           \
            printf("not ok %i - %s = %d, want %d\n", ++check_count__, str,               \
                         got__, want__);                                                 \
            return -1;                                                                   \
        }                                                                                \
        printf("ok %i - %s = %d\n", ++check_count__, str, got__);                        \
    } while (0)

#define EXPECT_EQ_INT(expect, actual)                                                    \
    EXPECT_EQ_INT_STR(expect, actual, #actual)

#define EXPECT_EQ_UINT64(expect, actual)                                                 \
    do {                                                                                 \
        uint64_t want__ = (uint64_t)expect;                                              \
        uint64_t got__ = (uint64_t)actual;                                               \
        if (got__ != want__) {                                                           \
            printf("not ok %i - %s = %" PRIu64 ", want %" PRIu64 "\n",                   \
                         ++check_count__, #actual, got__, want__);                       \
            return -1;                                                                   \
        }                                                                                \
        printf("ok %i - %s = %" PRIu64 "\n", ++check_count__, #actual, got__);           \
    } while (0)

#define EXPECT_EQ_PTR(expect, actual)                                                    \
    do {                                                                                 \
        void *want__ = expect;                                                           \
        void *got__ = actual;                                                            \
        if (got__ != want__) {                                                           \
            printf("not ok %i - %s = %p, want %p\n", ++check_count__, #actual,           \
                         got__, want__);                                                 \
            return -1;                                                                   \
        }                                                                                \
        printf("ok %i - %s = %p\n", ++check_count__, #actual, got__);                    \
    } while (0)

#define EXPECT_EQ_DOUBLE_STR(expect, actual, str)                                        \
    do {                                                                                 \
        double want__ = (double)expect;                                                  \
        double got__ = (double)actual;                                                   \
        if ((isnan(want__) && !isnan(got__)) ||                                          \
                (!isnan(want__) && isnan(got__))) {                                      \
            printf("not ok %i - %s = %.15g, want %.15g\n", ++check_count__, str,         \
                                                           got__, want__);               \
            return -1;                                                                   \
        } else if (!isnan(want__) && (((want__ - got__) < -DBL_PRECISION) ||             \
                                      ((want__ - got__) > DBL_PRECISION))) {             \
            printf("not ok %i - %s = %.15g, want %.15g\n", ++check_count__, str,         \
                                                           got__, want__);               \
            return -1;                                                                   \
        }                                                                                \
        printf("ok %i - %s = %.15g\n", ++check_count__, str, got__);                     \
    } while (0)

#define EXPECT_EQ_DOUBLE(expect, actual)                                                 \
    EXPECT_EQ_DOUBLE_STR(expect, actual, #actual)

#define CHECK_NOT_NULL(expr)                                                             \
    do {                                                                                 \
        void const *ptr_;                                                                \
        ptr_ = (expr);                                                                   \
        OK1(ptr_ != NULL, #expr);                                                        \
    } while (0)

#define CHECK_ZERO(expr)                                                                 \
    do {                                                                                 \
        long status_;                                                                    \
        status_ = (long)(expr);                                                          \
        OK1(status_ == 0L, #expr);                                                       \
    } while (0)


int plugin_test_set_procpath(const char *path);
int plugin_test_set_syspath(const char *path);

int plugin_test_config(config_item_t *ci);
int plugin_test_init(void);
int plugin_test_read(void);
int plugin_test_write(metric_family_t const *fam);
int plugin_test_notification(const notification_t *n);
int plugin_test_flush(cdtime_t timeout);
int plugin_test_shutdown(void);

int plugin_test_metrics_cmp(const char *filename);

void plugin_test_metrics_reset(void);
void plugin_test_reset(void);

int plugin_test_do_read(char *proc_path, char *sys_path, config_item_t *ci, char *expect);
