// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2005-2014 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "log.h"
#include "libutils/common.h"
#include "libmetric/metric.h"
#include "libmetric/metric_chars.h"
#include "libmetric/notification.h"
#include "libmetric/marshal.h"
#include "libmetric/parser.h"

int notification_marshal(strbuf_t *buf, notification_t const *n)
{
    if ((buf == NULL) || (n == NULL))
        return EINVAL;

    int status = strbuf_print(buf, n->name);

    status = status || label_set_marshal(buf, n->label);
    status = status || label_set_marshal(buf, n->annotation);

    const char *severity = " FAILURE ";
    if (n->severity == NOTIF_WARNING)
        severity = " WARNING ";
    else if (n->severity == NOTIF_OKAY)
        severity = " OKAY ";

    status = status || strbuf_print(buf, severity);
    status = status || strbuf_printf(buf, "%.3f", CDTIME_T_TO_DOUBLE(n->time));

    return status;
}

int notification_init_metric(notification_t *n, severity_t severity,
                             metric_family_t const *fam, metric_t const *m)
{
    if ((n == NULL) || (m == NULL))
        return EINVAL;

    n->name = fam->name;
    n->severity = severity;
    n->time = m->time;

    int status = label_set_clone(&n->label, m->label);
    if (status != 0) {
        return status;
    }

    return 0;
}

char const *notification_label_get(notification_t const *n, char const *name)
{
    if ((n == NULL) || (name == NULL)) {
        errno = EINVAL;
        return NULL;
    }

    label_pair_t *set = label_set_read(n->label, name);
    if (set == NULL)
        return NULL;

    return set->value;
}

int notification_label_set(notification_t *n, char const *name, char const *value)
{
    if ((n == NULL) || (name == NULL))
        return EINVAL;

    return label_set_add(&n->label, true, name, value);
}

char const *notification_annotation_get(notification_t const *n, char const *name)
{
    if ((n == NULL) || (name == NULL)) {
        errno = EINVAL;
        return NULL;
    }

    label_pair_t *set = label_set_read(n->annotation, name);
    if (set == NULL)
        return NULL;

    return set->value;
}

int notification_annotation_set(notification_t *n, char const *name, char const *value)
{
    if ((n == NULL) || (name == NULL))
        return EINVAL;

    return label_set_add(&n->annotation, true, name, value);
}

int notification_reset(notification_t *n)
{
    if (n == NULL)
        return EINVAL;

    label_set_reset(&n->label);
    label_set_reset(&n->annotation);
    free(n->name);

    memset(n, 0, sizeof(*n));

    return 0;
}

void notification_free(notification_t *n)
{
    if (n == NULL)
        return;

    label_set_reset(&n->label);
    label_set_reset(&n->annotation);
    free(n->name);
    free(n);
}

notification_t *notification_clone(notification_t const *src)
{
    if (src == NULL) {
        //return EINVAL;
        return NULL;
    }

    notification_t *dest = calloc(1, sizeof(*dest));
    if (dest == NULL) {
        //return ENOMEM;
        return NULL;
    }

    int status = label_set_clone(&dest->label, src->label);
    if (status != 0) {
        notification_free(dest);
        return NULL;
    }

    status = label_set_clone(&dest->annotation, src->annotation);
    if (status != 0) {
        notification_free(dest);
        return NULL;
    }

    dest->time = src->time;
    dest->severity = src->severity;
    dest->name = sstrdup(src->name);
    if (dest->name == NULL) {
        ERROR("notification_clone: strdup failed.");
        notification_free(dest);
        return NULL;
    }

    return dest;
}

int notitication_unmarshal(notification_t *n, char const *buf)
{
    if ((n == NULL) || (buf == NULL))
        return EINVAL;

    char const *ptr = buf;
    size_t name_len = metric_valid_len(ptr);
    if (name_len == 0)
        return EINVAL;

    char name[name_len + 1];
    strncpy(name, ptr, name_len);
    name[name_len] = 0;
    ptr += name_len;

    n->name = strdup(name);
    if (n->name == NULL)
        return ENOMEM;

    char const *end = ptr;
    int ret = label_set_unmarshal(&n->label, &end);
    if (ret != 0)
        return ret;
    ptr = end;

    end = ptr;
    ret = label_set_unmarshal(&n->annotation, &end);
    if (ret != 0)
        return ret;
    ptr = end;

    if ((ptr[0] != '}') || ((ptr[1] != 0) && (ptr[1] != ' ')))
        return EINVAL;

    return 0;
}
