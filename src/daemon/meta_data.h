/**
 * collectd - src/meta_data.h
 * Copyright (C) 2008-2011  Florian octo Forster
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
 **/

#ifndef META_DATA_H
#define META_DATA_H

#include "collectd.h"

typedef enum {
  MD_TYPE_STRING  = 1,
  MD_TYPE_INT64   = 2,
  MD_TYPE_UINT64  = 3,
  MD_TYPE_FLOAT64 = 4,
  MD_TYPE_BOOLEAN = 5,
} md_type_t;

typedef union {
  char *string;
  int64_t int64;
  uint64_t uint64;
  double float64;
  bool boolean;
} md_value_t;

typedef struct {
  char *name;
  md_type_t type;
  md_value_t value;
} md_pair_t;

typedef struct {
  md_pair_t *ptr;
  size_t num;
} meta_data_t;

int meta_data_add(meta_data_t *md, const char *name, md_type_t type, md_value_t value);

static inline int meta_data_add_string(meta_data_t *md, const char *name, const char *value)
{
  return meta_data_add(md, name, MD_TYPE_STRING, (md_value_t){.string = (char *)value});
}

static inline int meta_data_add_signed_int(meta_data_t *md, const char *name, int64_t value)
{
  return meta_data_add(md, name, MD_TYPE_INT64, (md_value_t){.int64 = value});
}

static inline int meta_data_add_unsigned_int(meta_data_t *md, const char *name, uint64_t value)
{
  return meta_data_add(md, name, MD_TYPE_UINT64, (md_value_t){.uint64 = value});
}

static inline int meta_data_add_double(meta_data_t *md, const char *name, double value)
{
  return meta_data_add(md, name, MD_TYPE_FLOAT64, (md_value_t){.float64 = value});
}

static inline int meta_data_add_boolean(meta_data_t *md, const char *name, bool value)
{ 
  return meta_data_add(md, name, MD_TYPE_BOOLEAN, (md_value_t){.boolean = value});
}

md_pair_t *meta_data_read(meta_data_t md, char const *name);

static inline int meta_data_exists(meta_data_t *md, const char *name)
{
  md_pair_t *elem = meta_data_read(*md, name);
  if (elem == NULL)
    return ENOENT;
  return 0;
}

static inline md_type_t meta_data_type(meta_data_t *md, const char *name)
{
  md_pair_t *elem = meta_data_read(*md, name);
  if (elem == NULL)
    return ENOENT;

  return elem->type;
}

static inline int meta_data_get_string(meta_data_t *md, const char *name, char **value)
{
  md_pair_t *elem = meta_data_read(*md, name);
  if (elem == NULL)
    return ENOENT;

  if (elem->type != MD_TYPE_STRING)
    return EINVAL;

  *value = elem->value.string;
  return 0;
}

static inline int meta_data_get_int(meta_data_t *md, const char *name, int64_t *value)
{
  md_pair_t *elem = meta_data_read(*md, name);
  if (elem == NULL)
    return ENOENT;

  if (elem->type != MD_TYPE_INT64)
    return EINVAL;

  *value = elem->value.int64;
  return 0;
}

static inline int meta_data_get_uint(meta_data_t *md, const char *name, uint64_t *value)
{
  md_pair_t *elem = meta_data_read(*md, name);
  if (elem == NULL)
    return ENOENT;

  if (elem->type != MD_TYPE_UINT64)
    return EINVAL;

  *value = elem->value.uint64;
  return 0;
}

static inline int meta_data_get_double(meta_data_t *md, const char *name, double *value)
{
  md_pair_t *elem = meta_data_read(*md, name);
  if (elem == NULL)
    return ENOENT;

  if (elem->type != MD_TYPE_FLOAT64)
    return EINVAL;

  *value = elem->value.float64;
  return 0;
}

static inline int meta_data_get_boolean(meta_data_t *md, const char *name, bool *value)
{
  md_pair_t *elem = meta_data_read(*md, name);
  if (elem == NULL)
    return ENOENT;

  if (elem->type != MD_TYPE_BOOLEAN)
    return EINVAL;

  *value = elem->value.boolean;
  return 0;
}

int meta_data_create(meta_data_t *md, char const *name, md_type_t type, md_value_t value);

int meta_data_delete(meta_data_t *md, char const *name);

void meta_data_reset(meta_data_t *md);

int meta_data_clone(meta_data_t *dest, meta_data_t src);

#endif /* META_DATA_H */
