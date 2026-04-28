/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín       */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#define MT_METRIC_FAMILY "mt_metric_family"

int register_metric_family(lua_State *L);

int luac_push_metric_family(lua_State *L, metric_family_t const *fam);

