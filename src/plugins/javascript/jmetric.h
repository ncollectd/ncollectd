/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín       */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

int metric_to_string(strbuf_t *buf, metric_t *m, metric_type_t type);

JSValue qjs_metric_new(JSContext *ctx, metric_t *m, metric_type_t type);

metric_type_t qjs_metric_get_metric_type(JSContext *ctx, JSValueConst v);

metric_t *qjs_metric_get_metric(JSContext *ctx, JSValueConst v);

int qjs_metric_all_init(JSContext *ctx, JSModuleDef *m);
