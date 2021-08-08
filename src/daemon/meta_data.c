/**
 * collectd - src/meta_data.c
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

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"
#include "meta_data.h"

static int md_pair_compare(void const *a, void const *b)
{
  return strcmp(((md_pair_t const *)a)->name,
                ((md_pair_t const *)b)->name);
}

md_pair_t *meta_data_read(meta_data_t md, char const *name)
{
  if (name == NULL) {
    errno = EINVAL;
    return NULL;
  }

  md_pair_t pair = (md_pair_t){.name = (char *)name};

  md_pair_t *ret = bsearch(&pair, md.ptr, md.num, sizeof(*md.ptr), md_pair_compare);
  if (ret == NULL) {
    errno = ENOENT;
    return NULL;
  }

  return ret;
}

int meta_data_create(meta_data_t *md, char const *name, md_type_t type, md_value_t value)
{
  if ((md == NULL) || (name == NULL) || (name[0] == '\0'))
    return EINVAL;

  if (meta_data_read(*md, name) != NULL)
    return EEXIST;

  errno = 0;

  md_pair_t *tmp = realloc(md->ptr, sizeof(*md->ptr) * (md->num + 1));
  if (tmp == NULL)
    return errno;
  md->ptr = tmp;

  md_pair_t pair = {
      .name = strdup(name),
      .type = type
  };
  if (pair.name == NULL)
    return ENOMEM;

  if (type == MD_TYPE_STRING) {
    pair.value.string = strdup(value.string);
    if (pair.value.string == NULL) {
      free(pair.name);
      return ENOMEM;
    }
  }

  md->ptr[md->num] = pair;
  md->num++;

  qsort(md->ptr, md->num, sizeof(*md->ptr), md_pair_compare);
  return 0;
}

int meta_data_delete(meta_data_t *md, char const *name)
{
  if ((md == NULL) || (name == NULL))
    return EINVAL;

  md_pair_t elem = (md_pair_t){.name = (char *)name};

  md_pair_t *ret = bsearch(&elem, md->ptr, md->num, sizeof(*md->ptr), md_pair_compare);
  if (ret == NULL)
    return ENOENT;

  if ((ret < md->ptr) || (ret > md->ptr + (md->num - 1)))
    return ERANGE;

  size_t index = ret - md->ptr;
  assert(md->ptr + index == ret);

  free(ret->name);
  if (ret->type == MD_TYPE_STRING)
    free(ret->value.string);

  if (index != (md->num - 1))
    memmove(md->ptr + index, md->ptr + (index + 1),
            sizeof(*md->ptr) * (md->num - (index + 1)));
  md->num--;

  if (md->num == 0) {
    free(md->ptr);
    md->ptr = NULL;
    return 0;
  }

  md_pair_t *tmp = realloc(md->ptr, sizeof(*md->ptr) * md->num);
  if (tmp == NULL)
    return errno;
  md->ptr = tmp;

  return 0;
}

int meta_data_add(meta_data_t *md, char const *name, md_type_t type, md_value_t value)
{
  if ((md == NULL) || (name == NULL))
    return EINVAL;

  md_pair_t *pair= meta_data_read(*md, name);
  if ((pair == NULL) && (errno != ENOENT))
    return errno;
  errno = 0;

  if (pair == NULL)
    return meta_data_create(md, name, type, value);

  if (pair->type == MD_TYPE_STRING)
    free(pair->value.string);

  pair->type = type;
  if (pair->type == MD_TYPE_STRING) {
    if (value.string == NULL)
      return EINVAL;
    pair->value.string = strdup(value.string);
    if (pair->value.string == NULL)
      return errno;
  } else {
    pair->value = value;
  }

  return 0;
}

void meta_data_reset(meta_data_t *md)
{
  if (md == NULL)
    return;

  for (size_t i = 0; i < md->num; i++) {
    free(md->ptr[i].name);
    if (md->ptr[i].type == MD_TYPE_STRING)
      free(md->ptr[i].value.string);
  }
  free(md->ptr);

  md->ptr = NULL;
  md->num = 0;
}

int meta_data_clone(meta_data_t *dest, meta_data_t src)
{
  if (src.num == 0)
    return 0;

  meta_data_t ret = {
    .ptr = calloc(src.num, sizeof(*ret.ptr)),
    .num = src.num,
  };
  if (ret.ptr == NULL)
    return ENOMEM;

  for (size_t i = 0; i < src.num; i++) {
    ret.ptr[i].name = strdup(src.ptr[i].name);
    if (ret.ptr[i].name == NULL) {
      meta_data_reset(&ret);
      return ENOMEM;
    }
    ret.ptr[i].type = src.ptr[i].type;
    if (ret.ptr[i].type == MD_TYPE_STRING) {
      ret.ptr[i].value.string = strdup(src.ptr[i].value.string);
      if (ret.ptr[i].value.string == NULL) {
        meta_data_reset(&ret);
        return ENOMEM;
      }
    } else {
      ret.ptr[i].value = src.ptr[i].value;
    }
  }

  *dest = ret;
  return 0;
}
