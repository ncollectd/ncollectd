// SPDX-License-Identifier: GPL-2.0-only OR MIT
// Copyright (C) 2008       Sebastian Harl
// Authors:
//   Sebastian "tokkee" Harl <sh at tokkee.org>

/*
 * This module provides functions for string substitution.
 */

#include "collectd.h"

#include "utils/common/common.h"
#include "utils_subst.h"

char *subst(char *buf, size_t buflen, const char *string, size_t off1,
            size_t off2, const char *replacement) {
  char *out = buf;

  char const *front;
  char const *back;
  size_t front_len;
  size_t replacement_len;
  size_t back_len;

  if ((NULL == buf) || (0 == buflen) || (NULL == string) ||
      (NULL == replacement))
    return NULL;

  size_t string_len = strlen(string);
  if ((off1 > string_len) || (off2 > string_len) || (off1 > off2))
    return NULL;

  front = string;
  back = string + off2;
  front_len = off1;
  replacement_len = strlen(replacement);
  back_len = strlen(back);

  if (front_len >= buflen) {
    front_len = buflen - 1;
    replacement_len = 0;
    back_len = 0;
  } else if ((front_len + replacement_len) >= buflen) {
    replacement_len = buflen - (front_len + 1);
    back_len = 0;
  } else if ((front_len + replacement_len + back_len) >= buflen) {
    back_len = buflen - (front_len + replacement_len + 1);
  } else {
    buflen = front_len + replacement_len + back_len + 1;
  }
  assert((front_len + replacement_len + back_len) == (buflen - 1));

  if (front_len != 0) {
    sstrncpy(out, front, front_len + 1);
    out += front_len;
  }

  if (replacement_len != 0) {
    sstrncpy(out, replacement, replacement_len + 1);
    out += replacement_len;
  }

  if (back_len != 0) {
    sstrncpy(out, back, back_len + 1);
    out += back_len;
  }

  out[0] = 0;
  return buf;
} /* subst */

char *subst_string(char *buf, size_t buflen, const char *string,
                   const char *needle, const char *replacement) {
  size_t needle_len;
  size_t i;

  if ((buf == NULL) || (string == NULL) || (needle == NULL) ||
      (replacement == NULL))
    return NULL;

  needle_len = strlen(needle);
  sstrncpy(buf, string, buflen);

  /* Limit the loop to prevent endless loops. */
  for (i = 0; i < buflen; i++) {
    char temp[buflen];
    char *begin_ptr;
    size_t begin;

    /* Find `needle' in `buf'. */
    begin_ptr = strstr(buf, needle);
    if (begin_ptr == NULL)
      break;

    /* Calculate the start offset. */
    begin = begin_ptr - buf;

    /* Substitute the region using `subst'. The result is stored in
     * `temp'. */
    begin_ptr =
        subst(temp, buflen, buf, begin, begin + needle_len, replacement);
    if (begin_ptr == NULL) {
      WARNING("subst_string: subst failed.");
      break;
    }

    /* Copy the new string in `temp' to `buf' for the next round. */
    strncpy(buf, temp, buflen);
  }

  if (i >= buflen) {
    WARNING("subst_string: Loop exited after %" PRIsz " iterations: "
            "string = %s; needle = %s; replacement = %s;",
            i, string, needle, replacement);
  }

  return buf;
} /* char *subst_string */
