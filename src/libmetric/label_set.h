/* SPDX-License-Identifier: GPL-2.0-only OR MIT                      */
/* SPDX-FileCopyrightText: Copyright (C) 2019-2020 Google LLC        */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org> */
/* SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>   */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include <stdbool.h>
#include <string.h>

/* label_pair_t represents a label, i.e. a key/value pair. */
typedef struct {
  const char *name;
  const char *value;
} label_pair_const_t;

typedef struct {
  char *name;
  char *value;
} label_pair_t;

/* label_set_t is a sorted set of labels. */
typedef struct {
  label_pair_t *ptr;
  size_t num;
} label_set_t;

label_pair_t *label_set_read(label_set_t labels, char const *name);

int _label_set_add(label_set_t *labels, bool overwrite, bool unescape,
                   const char *name, size_t sn, const char *value, size_t sv);

static inline int label_set_add(label_set_t *labels, bool overwrite,
                                char const *name, char const *value)
{
    return _label_set_add(labels, overwrite, false,
                          name, strlen(name),
                          value, value == NULL ? 0 : strlen(value));
}

static inline int label_set_add_escape(label_set_t *labels, bool overwrite, bool unescape,
                                       char const *name, char const *value)
{
    return _label_set_add(labels, overwrite, unescape,
                          name, strlen(name),
                          value, value == NULL ? 0 : strlen(value));
}

int label_set_add_set(label_set_t *labels, bool overwrite, label_set_t set);

void label_set_reset(label_set_t *labels);

int label_set_clone(label_set_t *dest, label_set_t src);

size_t label_set_strlen(label_set_t *labels);

int label_set_cmp(label_set_t *l1, label_set_t *l2);

int label_set_rename(label_set_t *labels, char *from, char *to);

int label_set_qsort(label_set_t *labels);

int label_set_unmarshal(label_set_t *labels, char const **inout);
