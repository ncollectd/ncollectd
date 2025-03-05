/* SPDX-License-Identifier: GPL-2.0-only OR MIT                     */
/* SPDX-FileCopyrightText: Copyright (C) 2006-2009  Florian Forster */
/* SPDX-FileContributor: Florian Forster <octo at collectd.org>     */

#pragma once

#define CONFIG_ERROR_SIZE 256

typedef struct {
    char error[CONFIG_ERROR_SIZE];
    config_item_t *root;
} config_status_t;

typedef struct {
    config_item_t *statement;
    int statement_num;
} statement_list_t;

typedef struct {
    config_value_t *argument;
    int argument_num;
} argument_list_t;

typedef struct {
    int openbrac;
    int closebrac;
    int eof;
} config_yy_extra_t;

