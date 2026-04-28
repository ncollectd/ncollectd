/* SPDX-License-Identifier: GPL-2.0-only OR MIT                 */
/* SPDX-FileCopyrightText: Copyright (C) 2010 Florian Forster   */
/* SPDX-FileContributor: Florian Forster <octo at collectd.org> */

#pragma once

#include "plugin.h"

#include <lua.h>

cdtime_t luac_to_cdtime(lua_State *L, int idx);

int luac_push_cdtime(lua_State *L, cdtime_t t);

int luac_push_config_item(lua_State *L, const config_item_t *ci);

int luac_push_labels(lua_State *L, const label_set_t *labels);

int luac_to_labels(lua_State *L, int idx, label_set_t *labels);

bool luac_isarray(lua_State *L, int idx);

void luac_stack_dump(lua_State *L, FILE *fp);
