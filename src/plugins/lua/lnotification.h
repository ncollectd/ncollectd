/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín       */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#define MT_NOTIFICATION "mt_notification"

int register_notification(lua_State *L);

int luac_push_notification(lua_State *L, const notification_t *n);

