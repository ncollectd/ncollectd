/* SPDX-License-Identifier: GPL-2.0-only OR MIT                                */
/* SPDX-FileCopyrightText: Copyright (C) 2010 Florian Forster                  */
/* SPDX-FileCopyrightText: Copyright (C) 1994-2020 Lua.org, PUC-Rio            */
/* SPDX-FileCopyrightText: Copyright (C) 2013-2023 The Lua-Compat-5.3 authors  */
/* SPDX-FileContributor: Florian Forster <octo at collectd.org>                */

#pragma once

#include "plugin.h"

#include <lua.h>
#include <lauxlib.h>

cdtime_t luac_to_cdtime(lua_State *L, int idx);

int luac_push_cdtime(lua_State *L, cdtime_t t);

int luac_push_config_item(lua_State *L, const config_item_t *ci);

int luac_push_labels(lua_State *L, const label_set_t *labels);

int luac_to_labels(lua_State *L, int idx, label_set_t *labels);

bool luac_isarray(lua_State *L, int idx);

void luac_stack_dump(lua_State *L, FILE *fp);

#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM == 501 && !defined(LUA_JITLIBNAME)
void luaL_setmetatable (lua_State *L, const char *tname);
void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup);
void *luaL_testudata (lua_State *L, int i, const char *tname);
#endif

#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM == 501
    #define lua_getuservalue(L, i) (lua_getfenv((L), (i)), lua_type((L), -1))
    #define lua_rawlen(L, i) lua_objlen((L), (i))
#endif

#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM == 502
    #define lua_getuservalue(L, i) (lua_getuservalue((L), (i)), lua_type((L), -1))
#endif
