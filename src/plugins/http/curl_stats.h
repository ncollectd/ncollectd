/* SPDX-License-Identifier: GPL-2.0-only OR MIT                       */
/* SPDX-FileCopyrightText: Copyright (C) 2015 Sebastian 'tokkee' Harl */
/* SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>            */

#pragma once

#include "plugin.h"

#include <curl/curl.h>

struct curl_stats_s;
typedef struct curl_stats_s curl_stats_t;

/*
 * curl_stats_from_config allocates and constructs a cURL statistics object
 * from the specified configuration which is expected to be a single block of
 * boolean options named after cURL information fields. The boolean value
 * indicates whether to collect the respective information.
 *
 * See http://curl.haxx.se/libcurl/c/curl_easy_getinfo.html
 */
int curl_stats_from_config(config_item_t *ci, char *prefix, curl_stats_t **curl_stats);

void curl_stats_destroy(curl_stats_t *s);

/*
 * curl_stats_dispatch dispatches performance values from the the specified
 * cURL session to the daemon.
 */
int curl_stats_dispatch(curl_stats_t *s, CURL *curl, label_set_t *labels);
