/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín       */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

int qjs_notification_init(JSContext *ctx, JSModuleDef *m);

JSValue qjs_notification_new(JSContext *ctx, const notification_t *n);
