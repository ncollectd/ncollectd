/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín       */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

int qjs_metric_family_init(JSContext *ctx, JSModuleDef *m);

JSValue qjs_metric_family_new(JSContext *ctx, const metric_family_t *fam);

