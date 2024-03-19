/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libutils/htable.h"
#include "libmetric/metric.h"
#include "libmdb/rindex.h"
#include "libmdb/family_metric_list.h"

typedef struct {
    char *name;
    char *help;
    char *unit;
    metric_type_t type;

    uint32_t metric_name_len;
    rindex_name_t *metric_name;
} family_t;

typedef struct {
    htable_t family_table; /* family_t */
} mdb_family_t;

int family_init(mdb_family_t *mdbfam);

void family_destroy(mdb_family_t *mdbfam);

int family_getsert(mdb_family_t *mdbfam, metric_family_t *mfam);

mdb_family_metric_list_t *family_get_list(mdb_family_t *mdbfam);
