/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín       */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

JSValue qjs_array_get_length(JSContext *ctx, JSValueConst array, uint32_t *len);

JSValue qjs_from_label_set(JSContext *ctx, const label_set_t *set);

JSValue qjs_to_label_set(JSContext *ctx, JSValueConst jset, label_set_t *set);

int label_set_to_string(strbuf_t *buf, label_set_t *set);

