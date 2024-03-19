/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

int graphite_line_metric(strbuf_t *buf, metric_family_t const *fam, metric_t const *m);

int graphite_line_metric_family(strbuf_t *buf, metric_family_t const *fam);
