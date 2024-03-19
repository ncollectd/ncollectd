/* SPDX-License-Identifier: GPL-2.0-only OR MIT                     */
/* SPDX-FileCopyrightText: Copyright (C) 2006-2009  Florian Forster */
/* SPDX-FileContributor: Florian Forster <octo at collectd.org>     */

#pragma once

#include <stdio.h>

typedef struct {
    int refcnt;
    char name[];
} config_file_t;

typedef enum {
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_NUMBER,
    CONFIG_TYPE_BOOLEAN,
    CONFIG_TYPE_REGEX
} config_type_t;

typedef struct {
    union {
        char *string;
        double number;
        int boolean;
    } value;
    config_type_t type;
} config_value_t;

struct config_item_s;
typedef struct config_item_s config_item_t;
struct config_item_s {
    char *key;
    config_value_t *values;
    int values_num;

    int lineno;
    config_file_t *file;

    config_item_t *parent;
    config_item_t *children;
    int children_num;
};

config_item_t *config_parse_file(const char *file);

config_item_t *config_clone(const config_item_t *ci);

void config_dump(FILE *fh, config_item_t *ci);

void config_free(config_item_t *ci);
