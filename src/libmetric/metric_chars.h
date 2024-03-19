/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

extern const char valid_label_chars[256];
extern const char valid_metric_chars[256];

static inline bool label_check_name (const char *str, size_t sn)
{
  unsigned const char *s = (unsigned const char *)str;

  if (valid_label_chars[s[0]] != 1)
    return false;

  for (size_t n = 1; n < sn; n++) {
    if (valid_label_chars[s[n]] == 0)
      return false;
  }

  return true;
}

static inline size_t label_valid_name_len (const char *str)
{
  unsigned const char *s = (unsigned const char *)str;

  if (valid_label_chars[s[0]] != 1)
    return 0;

  size_t len = 1;
  for (unsigned const char *c = s+1; *c != '\0' ; c++, len++) {
    if (valid_label_chars[*c] == 0)
      return len;
  }

  return len;
}

static inline size_t metric_valid_len (const char *str)
{
  unsigned const char *s = (unsigned const char *)str;

  if (valid_metric_chars[s[0]] != 1)
    return 0;

  size_t len = 1;
  for (unsigned const char *c = s+1; *c != '\0' ; c++, len++) {
    if (valid_metric_chars[*c] == 0)
      return len;
  }

  return len;
}
