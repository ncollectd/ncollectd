// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "cperl.h"

#include "libutils/common.h"


static const cpl_member_t cpl_config_members[] = {
    { "key",      CPL_MEMBER_STRING, CPL_MEMBER_REQUIRED },
    { "parent",   CPL_MEMBER_ANY,    CPL_MEMBER_NOREPR   },
    { "values",   CPL_MEMBER_ARRAY,  0                   },
    { "children", CPL_MEMBER_ARRAY,  0                   },
};

static const cpl_class_t cpl_config_class = {
    .name        = CPL_CLASS_CONFIG,
    .base        = NULL,
    .usage       = "key[, parent][, values][, children]",
    .members     = cpl_config_members,
    .members_num = STATIC_ARRAY_SIZE(cpl_config_members),
    .own_members = STATIC_ARRAY_SIZE(cpl_config_members),
};

void cpl_config_boot(pTHX)
{
    cpl_class_boot(aTHX_ &cpl_config_class);
}

SV *cpl_config_to_sv(pTHX_ const config_item_t *ci, SV *parent)
{
    HV *hv = NULL;
    SV *self = cpl_object_new(aTHX_ CPL_CLASS_CONFIG, &hv);

    cpl_object_set(aTHX_ hv, "key", newSVpv(ci->key, 0));

    SV *parent_ref = newSV(0);
    if (parent != NULL) {
        sv_setsv(parent_ref, parent);
        /* The parent has references to its children. */
        sv_rvweaken(parent_ref);
    }
    cpl_object_set(aTHX_ hv, "parent", parent_ref);

    AV *values = newAV();
    for (int i = 0; i < ci->values_num; i++) {
        const config_value_t *value = &ci->values[i];
        switch (value->type) {
        case CONFIG_TYPE_STRING:
        case CONFIG_TYPE_REGEX:
            av_push(values, newSVpv(value->value.string, 0));
            break;
        case CONFIG_TYPE_NUMBER:
            av_push(values, newSVnv(value->value.number));
            break;
        case CONFIG_TYPE_BOOLEAN:
            av_push(values, newSVsv(boolSV(value->value.boolean)));
            break;
        }
    }
    cpl_object_set(aTHX_ hv, "values", newRV_noinc((SV *)values));

    AV *children = newAV();
    for (int i = 0; i < ci->children_num; i++)
        av_push(children, cpl_config_to_sv(aTHX_ ci->children + i, self));
    cpl_object_set(aTHX_ hv, "children", newRV_noinc((SV *)children));

    return self;
}
