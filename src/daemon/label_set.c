/**
 * collectd - src/daemon/label_set.c
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

#include "collectd.h"

#include "label_set.h"

static int label_pair_compare(void const *a, void const *b) {
  return strcmp(((label_pair_t const *)a)->name,
                ((label_pair_t const *)b)->name);
}

label_pair_t *label_set_read(label_set_t labels, char const *name) {
  if (name == NULL) {
    errno = EINVAL;
    return NULL;
  }

  struct {
    char const *name;
    char const *value;
  } label = {
      .name = name,
  };

  label_pair_t *ret = bsearch(&label, labels.ptr, labels.num,
                              sizeof(*labels.ptr), label_pair_compare);
  if (ret == NULL) {
    errno = ENOENT;
    return NULL;
  }

  return ret;
}

int label_set_create(label_set_t *labels, char const *name, char const *value) {
  if ((labels == NULL) || (name == NULL) || (value == NULL)) {
    return EINVAL;
  }

  size_t name_len = strlen(name);
  if (name_len == 0) {
    return EINVAL;
  }

  size_t valid_len = strspn(name, VALID_LABEL_CHARS);
  if ((valid_len != name_len) || isdigit((int)name[0])) {
    return EINVAL;
  }

  if (label_set_read(*labels, name) != NULL) {
    return EEXIST;
  }
  errno = 0;

  if (strlen(value) == 0) {
    return 0;
  }

  label_pair_t *tmp =
      realloc(labels->ptr, sizeof(*labels->ptr) * (labels->num + 1));
  if (tmp == NULL) {
    return errno;
  }
  labels->ptr = tmp;

  label_pair_t pair = {
      .name = strdup(name),
      .value = strdup(value),
  };
  if ((pair.name == NULL) || (pair.value == NULL)) {
    free(pair.name);
    free(pair.value);
    return ENOMEM;
  }

  labels->ptr[labels->num] = pair;
  labels->num++;

  qsort(labels->ptr, labels->num, sizeof(*labels->ptr), label_pair_compare);
  return 0;
}

int label_set_delete(label_set_t *labels, label_pair_t *elem) {
  if ((labels == NULL) || (elem == NULL)) {
    return EINVAL;
  }

  if ((elem < labels->ptr) || (elem > labels->ptr + (labels->num - 1))) {
    return ERANGE;
  }

  size_t index = elem - labels->ptr;
  assert(labels->ptr + index == elem);

  free(elem->name);
  free(elem->value);

  if (index != (labels->num - 1)) {
    memmove(labels->ptr + index, labels->ptr + (index + 1),
            labels->num - (index + 1));
  }
  labels->num--;

  if (labels->num == 0) {
    free(labels->ptr);
    labels->ptr = NULL;
  }

  return 0;
}

int label_set_add(label_set_t *labels, char const *name, char const *value) {
  if ((labels == NULL) || (name == NULL)) {
    return EINVAL;
  }

  label_pair_t *label = label_set_read(*labels, name);
  if ((label == NULL) && (errno != ENOENT)) {
    return errno;
  }
  errno = 0;

  if (label == NULL) {
    if ((value == NULL) || strlen(value) == 0) {
      return 0;
    }
    return label_set_create(labels, name, value);
  }

  if ((value == NULL) || strlen(value) == 0) {
    return label_set_delete(labels, label);
  }

  char *new_value = strdup(value);
  if (new_value == NULL) {
    return errno;
  }

  free(label->value);
  label->value = new_value;

  return 0;
}

void label_set_reset(label_set_t *labels) {
  if (labels == NULL) {
    return;
  }
  for (size_t i = 0; i < labels->num; i++) {
    free(labels->ptr[i].name);
    free(labels->ptr[i].value);
  }
  free(labels->ptr);

  labels->ptr = NULL;
  labels->num = 0;
}

int label_set_clone(label_set_t *dest, label_set_t src) {
  if (src.num == 0) {
    return 0;
  }

  label_set_t ret = {
      .ptr = calloc(src.num, sizeof(*ret.ptr)),
      .num = src.num,
  };
  if (ret.ptr == NULL) {
    return ENOMEM;
  }

  for (size_t i = 0; i < src.num; i++) {
    ret.ptr[i].name = strdup(src.ptr[i].name);
    ret.ptr[i].value = strdup(src.ptr[i].value);
    if ((ret.ptr[i].name == NULL) || (ret.ptr[i].value == NULL)) {
      label_set_reset(&ret);
      return ENOMEM;
    }
  }

  *dest = ret;
  return 0;
}

/* parse_label_value reads a label value, unescapes it and prints it to buf. On
 * success, inout is updated to point to the character just *after* the label
 * value, i.e. the character *following* the ending quotes - either a comma or
 * closing curlies. */
static int parse_label_value(strbuf_t *buf, char const **inout) {
  char const *ptr = *inout;

  if (ptr[0] != '"') {
    return EINVAL;
  }
  ptr++;

  while (ptr[0] != '"') {
    size_t valid_len = strcspn(ptr, "\\\"\n");
    if (valid_len != 0) {
      strbuf_printn(buf, ptr, valid_len);
      ptr += valid_len;
      continue;
    }

    if ((ptr[0] == 0) || (ptr[0] == '\n')) {
      return EINVAL;
    }

    assert(ptr[0] == '\\');
    if (ptr[1] == 0) {
      return EINVAL;
    }

    char tmp[2] = {ptr[1], 0};
    if (tmp[0] == 'n') {
      tmp[0] = '\n';
    } else if (tmp[0] == 'r') {
      tmp[0] = '\r';
    } else if (tmp[0] == 't') {
      tmp[0] = '\t';
    }

    strbuf_print(buf, tmp);

    ptr += 2;
  }

  assert(ptr[0] == '"');
  ptr++;
  *inout = ptr;
  return 0;
}

int label_set_unmarshal(label_set_t *labels, char const **inout)
{
  int ret = 0;
  char const *ptr = *inout;

  if (ptr[0] != '{') {
    return EINVAL;
  }

  strbuf_t value = STRBUF_CREATE;
  while ((ptr[0] == '{') || (ptr[0] == ',')) {
    ptr++;

    size_t key_len = strspn(ptr, VALID_LABEL_CHARS);
    if (key_len == 0) {
      ret = EINVAL;
      break;
    }
    char key[key_len + 1];
    strncpy(key, ptr, key_len);
    key[key_len] = 0;
    ptr += key_len;

    if (ptr[0] != '=') {
      ret = EINVAL;
      break;
    }
    ptr++;
    
    strbuf_reset(&value);   
    int status = parse_label_value(&value, &ptr);
    if (status != 0) {
      ret = status;
      break;
    }

    status = label_set_add(labels, key, value.ptr);
    if (status != 0) {
      ret = status;
      break;
    }
  }
  STRBUF_DESTROY(value);

  if (ret != 0) {
    return ret;
  }

  if (ptr[0] != '}') {
    return EINVAL;
  }

  *inout = &ptr[1];

  return 0;
}

int label_set_marshal(strbuf_t *buf, label_set_t labels)
{
  int status = strbuf_print(buf, "{");
  for (size_t i = 0; i < labels.num; i++) {
    if (i != 0) {
      status = status || strbuf_print(buf, ",");
    }

    status = status || strbuf_print(buf, labels.ptr[i].name);
    status = status || strbuf_print(buf, "=\"");
    status = status || strbuf_print_escaped(buf, labels.ptr[i].value,
                                            "\\\"\n\r\t", '\\');
    status = status || strbuf_print(buf, "\"");
  }

  return status || strbuf_print(buf, "}");
}

