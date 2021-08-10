/* SPDX-License-Identifier: GPL-2.0-only OR MIT  */
/* Copyright (C) 2019-2020  Google LLC           */
/* Authors:                                      */
/*   Florian octo Forster <octo at collectd.org> */
/*   Manoj Srivastava <srivasta at google.com>   */

#ifndef LABEL_SET_H
#define LABEL_SET_H 1

#include "utils/strbuf/strbuf.h"

/* label_pair_t represents a label, i.e. a key/value pair. */
typedef struct {
  char *name;
  char *value;
} label_pair_t;

/* label_set_t is a sorted set of labels. */
typedef struct {
  label_pair_t *ptr;
  size_t num;
} label_set_t;

int label_set_add(label_set_t *labels, char const *name, char const *value);

label_pair_t *label_set_read(label_set_t labels, char const *name);

int label_set_create(label_set_t *labels, char const *name, char const *value);

int label_set_delete(label_set_t *labels, label_pair_t *elem);

void label_set_reset(label_set_t *labels);

int label_set_clone(label_set_t *dest, label_set_t src);

int label_set_unmarshal(label_set_t *labels, char const **inout);

int label_set_marshal(strbuf_t *buf, label_set_t labels);

#endif
