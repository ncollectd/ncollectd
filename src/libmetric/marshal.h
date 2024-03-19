/* SPDX-License-Identifier: GPL-2.0-only OR MIT                      */
/* SPDX-FileCopyrightText: Copyright (C) 2019-2020 Google LLC        */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org> */
/* SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>   */

#pragma once

#include "libutils/strbuf.h"
#include "libmetric/metric.h"

int label_set_marshal(strbuf_t *buf, label_set_t labels);

int metric_identity(strbuf_t *buf, metric_family_t const *fam, metric_t const *m);
