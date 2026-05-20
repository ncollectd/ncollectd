// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2010 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileCopyrightText: Copyright (C) 1994-2020 Lua.org, PUC-Rio
// SPDX-FileCopyrightText: Copyright (C) 2013-2023 The Lua-Compat-5.3 authors
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/dtoa.h"
#include "utils.h"

int luac_push_cdtime(lua_State *L, cdtime_t t)
{
    double d = CDTIME_T_TO_DOUBLE(t);

    lua_pushnumber(L, (lua_Number)d);

    return 0;
}

cdtime_t luac_to_cdtime(lua_State *L, int idx)
{
    if (!lua_isnumber(L, idx))
        return 0;

    double d = lua_tonumber(L, idx);

    return DOUBLE_TO_CDTIME_T(d);
}

int luac_push_labels(lua_State *L, const label_set_t *labels)
{
    lua_createtable(L, 0, labels->num);

    for (size_t i = 0; i < labels->num; i++) {
        lua_pushstring(L, labels->ptr[i].value);
        lua_setfield(L, -2, labels->ptr[i].name);
    }

    return 0;
}

int luac_to_labels(lua_State *L, int idx, label_set_t *labels)
{
    if (idx < 1)
        idx += lua_gettop(L) + 1;

    /* Check that idx is in the valid range */
    if ((idx < 1) || (idx > lua_gettop(L))) {
        PLUGIN_DEBUG("idx(%d), top(%d)", idx, lua_gettop(L));
        return -1;
    }

    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        int ktype = lua_type(L, -2);
        if (ktype != LUA_TSTRING) {
            PLUGIN_WARNING("Label key '%s' must be LUA_TSTRING.", lua_tostring(L, -2));
            lua_pop(L, 1);
            continue;
        }

        const char *key = lua_tostring(L, -2);
        if(key == NULL) {
            PLUGIN_WARNING("Label key must be not null.");
            lua_pop(L, 1);
            continue;
        }

        int type = lua_type(L, -1);
        switch (type) {
        case LUA_TSTRING:
            label_set_add(labels, true, key, lua_tostring(L, -1));
            break;
        case LUA_TNUMBER: {
            double value = lua_tonumber(L, -1);
            char buffer[DTOA_MAX];
            dtoa(value, buffer, sizeof(buffer));
            label_set_add(labels, true, key, buffer);
        }   break;
        case LUA_TBOOLEAN: {
            bool value = lua_toboolean(L, -1);
            label_set_add(labels, true, key, value ? "true" : "false");
        }   break;
        default:
            PLUGIN_WARNING("Label value for '%s' must be LUA_TSTRING, LUA_TNUMBER o "
                           "LUA_TBOOLEAN.", key);
            break;
        }

        lua_pop(L, 1);
    }

    return 0;
}

static int luac_push_config_values(lua_State *L, const config_item_t *ci)
{
    lua_createtable(L, ci->values_num, 0);

    for (int i = 0; i < ci->values_num; i++) {
        config_value_t *value = ci->values + i;
        switch (value->type) {
        case CONFIG_TYPE_STRING:
        case CONFIG_TYPE_REGEX:
            lua_pushstring(L, value->value.string);
            break;
        case CONFIG_TYPE_NUMBER:
            lua_pushnumber(L, value->value.number);
            break;
        case CONFIG_TYPE_BOOLEAN:
            lua_pushboolean(L, value->value.boolean);
            break;
        }
        lua_rawseti(L, -2, i+1);
    }

    return 0;
}

static int luac_push_config_item_children(lua_State *L, const config_item_t *ci)
{
    lua_createtable(L, ci->children_num, 0);

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        lua_createtable(L, 0, 3);

        lua_pushstring(L, child->key);
        lua_setfield(L, -2, "key");

        luac_push_config_values(L, child);
        lua_setfield(L, -2, "values");

        luac_push_config_item(L, child->children);
        lua_setfield(L, -2, "children");
    }

    return 0;
}

int luac_push_config_item(lua_State *L, const config_item_t *ci)
{
    lua_createtable(L, 0, 3);

    lua_pushstring(L, ci->key);
    lua_setfield(L, -2, "key");

    luac_push_config_values(L, ci);
    lua_setfield(L, -2, "values");

    luac_push_config_item_children(L, ci->children);
    lua_setfield(L, -2, "children");

    return 0;
}

bool luac_isarray(lua_State *L, int idx)
{
    if (!lua_istable(L, idx))
        return false;

    if (idx < 1)
        idx += lua_gettop(L) + 1;

    /* Check that idx is in the valid range */
    if ((idx < 1) || (idx > lua_gettop(L))) {
        PLUGIN_DEBUG("idx(%d), top(%d)", idx, lua_gettop(L));
        return -1;
    }

    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        if (lua_type(L, -2) != LUA_TNUMBER) {
            lua_pop(L, 2);
            return false;
        }
        lua_pop(L, 1);
    }

    return true;
}

static void luac_stack_dump_entry(lua_State *L, FILE *fp, int i)
{
    int t = lua_type(L, i);
    switch (t) {
    case LUA_TNIL:
        fprintf(fp, "nil");
        break;
    case LUA_TBOOLEAN:
        fprintf(fp, lua_toboolean(L, i) ? "true" : "false");
        break;
    case LUA_TLIGHTUSERDATA:
        fprintf(fp, "lightuserdata(%p)", lua_touserdata(L, i));
        break;
    case LUA_TNUMBER:
        fprintf(fp, "%g", lua_tonumber(L, i));
        break;
    case LUA_TSTRING:
        fprintf(fp, "'%s'", lua_tostring(L, i));
        break;
    case LUA_TTABLE:
        fprintf(fp, "table[");
        lua_pushnil(L);
        while (lua_next(L, i > 0 ? i : i - 1)) {
            luac_stack_dump_entry(L, fp, -2);
            fprintf(fp, "=");
            luac_stack_dump_entry(L, fp, -1);
            fprintf(fp, ",");
            lua_pop(L, 1);
        }
        fprintf(fp, "]");
        break;
    case LUA_TUSERDATA:
        fprintf(fp, "userdata(%p)", lua_touserdata(L, i));
        break;
    default:
        fprintf(fp, "%s", lua_typename(L, t));
        break;
    }
}

void luac_stack_dump(lua_State *L, FILE *fp)
{
    int top = lua_gettop(L);
    for (int i = 1; i <= top; i++) {
        fprintf(fp, "%d: ", i);
        luac_stack_dump_entry(L, fp, i);
        fprintf(fp, "\n");
    }
}

#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM == 501 && !defined(LUA_JITLIBNAME)

void luaL_setmetatable (lua_State *L, const char *tname)
{
	luaL_checkstack(L, 1, "not enough stack slots");
	luaL_getmetatable(L, tname);
	lua_setmetatable(L, -2);
}

void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup)
{
    luaL_checkstack(L, nup+1, "too many upvalues");
    for (; l->name != NULL; l++) {
        lua_pushstring(L, l->name);
        for (int i = 0; i < nup; i++)
            lua_pushvalue(L, -(nup + 1));
        lua_pushcclosure(L, l->func, nup);
        lua_settable(L, -(nup + 3));
    }
    lua_pop(L, nup);
}

void *luaL_testudata (lua_State *L, int i, const char *tname)
{
    void *p = lua_touserdata(L, i);
    luaL_checkstack(L, 2, "not enough stack slots");
    if (p == NULL || !lua_getmetatable(L, i)) {
        return NULL;
    }  else {
        luaL_getmetatable(L, tname);
        int res = lua_rawequal(L, -1, -2);
        lua_pop(L, 2);
        if (!res)
            p = NULL;
    }
    return p;
}

#endif
