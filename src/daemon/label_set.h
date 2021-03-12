/**
 * collectd - src/daemon/label_set.h
 * Copyright (C) 2019-2020  Google LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Florian octo Forster <octo at collectd.org>
 *   Manoj Srivastava <srivasta at google.com>
 **/

#ifndef LABEL_SET_H
#define LABEL_SET_H 1

#include "utils/strbuf/strbuf.h"

/* Label names must match the regex `[a-zA-Z_][a-zA-Z0-9_]*`. Label names
 *
 * beginning with __ are reserved for internal use.
 *
 * Source:
 * https://prometheus.io/docs/concepts/data_model/#metric-names-and-labels */
#define VALID_LABEL_CHARS                                                      \
  "abcdefghijklmnopqrstuvwxyz"                                                 \
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"                                                 \
  "0123456789_"
/* Metric names must match the regex `[a-zA-Z_:][a-zA-Z0-9_:]*` */
#define VALID_NAME_CHARS VALID_LABEL_CHARS ":"

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
