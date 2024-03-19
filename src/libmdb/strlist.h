/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libutils/strbuf.h"
#include "libutils/strlist.h"
#include "libmdb/table.h"

strlist_t *mdb_strlist_parse(const char *data, size_t len);

int mdb_strlist_to_json(strlist_t *list, strbuf_t *buf, bool pretty);

int mdb_strlist_to_yaml(strlist_t *list, strbuf_t *buf);

int mdb_strlist_to_text(strlist_t *list, strbuf_t *buf);

int mdb_strlist_to_table(strlist_t *list, table_style_type_t style, strbuf_t *buf, char *header);
