/* SPDX-License-Identifier: GPL-2.0-only OR MIT                      */
/* SPDX-FileCopyrightText: Copyright (C) 2007-2008 C-Ware, Inc.      */
/* SPDX-FileCopyrightText: Copyright (C) 2008-2013 Florian Forster   */
/* SPDX-FileCopyrightText: Copyright (C) 2013 Kris Nielander         */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Kris Nielander <nielander at fox-it.com>    */
/* SPDX-FileContributor: Florian Forster <octo at collectd.org>      */
/* SPDX-FileContributor: Luke Heberling <lukeh at c-ware.com>        */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include <stdio.h>
#include <sys/stat.h>

typedef struct {
    char *file;
    FILE *fh;
    struct stat stat;
    bool force_rewind;
} tail_t;

void tail_free(tail_t *tail);

void tail_reset(tail_t *tail);

tail_t *tail_alloc(char *file, bool force_rewind);

int tail_close(tail_t *tail);

int tail_reopen(tail_t *tail);

int tail_readline(tail_t *tail, char *buf, size_t buflen);

