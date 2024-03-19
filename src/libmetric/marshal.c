// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2019-2020 Google LLC
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>

#include "ncollectd.h"
#include "libutils/strbuf.h"
#include "libmetric/metric.h"

int label_set_marshal(strbuf_t *buf, label_set_t labels)
{
    int status = strbuf_print(buf, "{");

    for (size_t i = 0; i < labels.num; i++) {
        if (i != 0) {
            status = status || strbuf_print(buf, ",");
        }

        status = status || strbuf_print(buf, labels.ptr[i].name);
        status = status || strbuf_print(buf, "=\"");
        status = status || strbuf_putescape_label(buf, labels.ptr[i].value);
        status = status || strbuf_print(buf, "\"");
    }

    return status || strbuf_print(buf, "}");
}

int metric_identity(strbuf_t *buf, metric_family_t const *fam, metric_t const *m)
{
    if ((buf == NULL) || (m == NULL) || (fam == NULL))
        return EINVAL;

    int status = strbuf_print(buf, fam->name);
    if (m->label.num == 0)
        return status;

    status = status || label_set_marshal(buf, m->label);

    return status;
}
