/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libutils/strbuf.h"
#include "libmetric/metric.h"

int label_set_unmarshal(label_set_t *labels, char const **inout);

/* value_marshal_text prints a text representation of v to buf. */
int value_marshal_text(strbuf_t *buf, value_t v, metric_type_t type);
