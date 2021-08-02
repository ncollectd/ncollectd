/**
 *
 * collectd - src/fhcount.c
 * Copyright (c) 2015, Jiri Tyr <jiri.tyr at gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 **/

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

static const char *config_keys[] = {"ValuesAbsolute", "ValuesPercentage"};
static int config_keys_num = STATIC_ARRAY_SIZE(config_keys);

static bool values_absolute = true;
static bool values_percentage;

static int fhcount_config(const char *key, const char *value)
{
  int ret = -1;

  if (strcasecmp(key, "ValuesAbsolute") == 0) {
    if (IS_TRUE(value)) {
      values_absolute = true;
    } else {
      values_absolute = false;
    }

    ret = 0;
  } else if (strcasecmp(key, "ValuesPercentage") == 0) {
    if (IS_TRUE(value)) {
      values_percentage = true;
    } else {
      values_percentage = false;
    }

    ret = 0;
  }

  return ret;
}

static void fhcount_submit(char *fam_name, double value)
{
  metric_family_t fam = {
      .name = fam_name,
      .type = METRIC_TYPE_GAUGE,
  };

  metric_family_metric_append(&fam, (metric_t){ .value.gauge.real = value, });

  int status = plugin_dispatch_metric_family(&fam);
  if (status != 0) {
    ERROR("fhcount plugin: plugin_dispatch_metric_family failed: %s",
          STRERROR(status));
  }

  metric_family_metric_reset(&fam);
}

static int fhcount_read(void)
{
  int numfields = 0;
  int buffer_len = 60;
  double used, unused, max;
  int prc_used, prc_unused;
  char *fields[3];
  char buffer[buffer_len];
  FILE *fp;

  // Open file
  fp = fopen("/proc/sys/fs/file-nr", "r");
  if (fp == NULL) {
    ERROR("fhcount: fopen: %s", STRERRNO);
    return EXIT_FAILURE;
  }
  if (fgets(buffer, buffer_len, fp) == NULL) {
    ERROR("fhcount: fgets: %s", STRERRNO);
    fclose(fp);
    return EXIT_FAILURE;
  }
  fclose(fp);

  // Tokenize string
  numfields = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

  if (numfields != 3) {
    ERROR("fhcount: Line doesn't contain 3 fields");
    return EXIT_FAILURE;
  }

  // Define the values
  strtodouble(fields[0], &used);
  strtodouble(fields[1], &unused);
  strtodouble(fields[2], &max);
  prc_used = used / max * 100;
  prc_unused = unused / max * 100;

  // Submit values
  if (values_absolute) {
    fhcount_submit("host_file_handles_used", used);
    fhcount_submit("host_file_handles_unused", unused);
    fhcount_submit("host_file_handles_max", max);
  }
  if (values_percentage) {
    fhcount_submit("host_file_handles_used_percent", prc_used);
    fhcount_submit("host_file_handles_unused_percent", prc_unused);
  }

  return 0;
}

void module_register(void)
{
  plugin_register_config("fhcount", fhcount_config, config_keys, config_keys_num);
  plugin_register_read("fhcount", fhcount_read);
}
