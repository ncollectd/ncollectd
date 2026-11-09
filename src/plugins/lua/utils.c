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

#if defined(LUA_VERSION_NUM) && (LUA_VERSION_NUM >= 503)
#  if LUA_MAXINTEGER >= INT64_MAX
#    define LUAC_INTEGER_INT64 1
#  endif
#endif

#define EXP2_30 17179869184.0
#define EXP2_63 9223372036854775808.0
#define EXP2_64 1.8446744073709552e+19

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

cdtime_t luac_checkcdtime(lua_State *L, int idx)
{
    lua_Number d = luaL_checknumber(L, idx);
    /* cdtime_t is an uint64_t with 2^30 units per second, this also rejects NaN. */
    if (!((d >= 0) && (d < EXP2_30)))
        luaL_argerror(L, idx, "time must be a non-negative number of seconds");

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
            PLUGIN_WARNING("Label key must be a LUA_TSTRING.");
            lua_pop(L, 1);
            continue;
        }

        const char *key = lua_tostring(L, -2);
        if(key == NULL) {
            PLUGIN_WARNING("Label key must be not null.");
            lua_pop(L, 1);
            continue;
        }

        int status = 0;
        int type = lua_type(L, -1);
        switch (type) {
        case LUA_TSTRING:
            status = label_set_add(labels, true, key, lua_tostring(L, -1));
            break;
        case LUA_TNUMBER: {
            double value = lua_tonumber(L, -1);
            char buffer[DTOA_MAX];
            dtoa(value, buffer, sizeof(buffer));
            status = label_set_add(labels, true, key, buffer);
        }   break;
        case LUA_TBOOLEAN: {
            bool value = lua_toboolean(L, -1);
            status = label_set_add(labels, true, key, value ? "true" : "false");
        }   break;
        default:
            PLUGIN_WARNING("Label value for '%s' must be LUA_TSTRING, LUA_TNUMBER o "
                           "LUA_TBOOLEAN.", key);
            break;
        }

        if (status != 0)
            PLUGIN_WARNING("Cannot add the label '%s', the name may be invalid.", key);

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
    if (!lua_checkstack(L, 3))
        return -1;

    lua_createtable(L, ci->children_num, 0);

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (luac_push_config_item(L, child) != 0)
            return -1;
        lua_rawseti(L, -2, i + 1);
    }

    return 0;
}

int luac_push_config_item(lua_State *L, const config_item_t *ci)
{
    if (!lua_checkstack(L, 3))
        return -1;

    lua_createtable(L, 0, 3);

    lua_pushstring(L, ci->key);
    lua_setfield(L, -2, "key");

    luac_push_config_values(L, ci);
    lua_setfield(L, -2, "values");

    if (luac_push_config_item_children(L, ci) != 0)
        return -1;
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
        return false;
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

bool luac_toint64(lua_State *L, int idx, int64_t *ret)
{
#ifdef LUAC_INTEGER_INT64
    int isnum = 0;
    lua_Integer i = lua_tointegerx(L, idx, &isnum);
    if (isnum) {
        *ret = (int64_t)i;
        return true;
    }
#endif
    if (!lua_isnumber(L, idx))
        return false;

    lua_Number n = lua_tonumber(L, idx);
    /* Also rejects NaN: the integral part must fit in an int64_t. */
    if (!((n >= -EXP2_63) && (n < EXP2_63)))
        return false;

    *ret = (int64_t)n;
    return true;
}

bool luac_touint64(lua_State *L, int idx, uint64_t *ret)
{
#ifdef LUAC_INTEGER_INT64
    int isnum = 0;
    lua_Integer i = lua_tointegerx(L, idx, &isnum);
    if (isnum) {
        if (i < 0)
            return false;
        *ret = (uint64_t)i;
        return true;
    }
#endif
    if (!lua_isnumber(L, idx))
        return false;

    lua_Number n = lua_tonumber(L, idx);
    /* Also rejects NaN: the integral part must fit in an uint64_t. */
    if (!((n > -1.0) && (n < EXP2_64)))
        return false;

    *ret = (uint64_t)n;
    return true;
}

int64_t luac_checkint64(lua_State *L, int idx)
{
    int64_t value = 0;
    if (!luac_toint64(L, idx, &value)) {
        luaL_checknumber(L, idx);
        luaL_argerror(L, idx, "number out of the int64 range");
    }
    return value;
}

uint64_t luac_checkuint64(lua_State *L, int idx)
{
    uint64_t value = 0;
    if (!luac_touint64(L, idx, &value)) {
        luaL_checknumber(L, idx);
        luaL_argerror(L, idx, "number out of the uint64 range");
    }
    return value;
}

void luac_pushint64(lua_State *L, int64_t value)
{
#ifdef LUAC_INTEGER_INT64
    lua_pushinteger(L, (lua_Integer)value);
#else
    lua_pushnumber(L, (lua_Number)value);
#endif
}

void luac_pushuint64(lua_State *L, uint64_t value)
{
#ifdef LUAC_INTEGER_INT64
    if (value <= (uint64_t)LUA_MAXINTEGER) {
        lua_pushinteger(L, (lua_Integer)value);
        return;
    }
#endif
    lua_pushnumber(L, (lua_Number)value);
}

int luac_index_metatable(lua_State *L)
{
    /* Methods are looked up in the metatable, unknown keys are nil. */
    if (!lua_getmetatable(L, 1)) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushvalue(L, 2);
    lua_rawget(L, -2);

    return 1;
}

int luac_newindex_error(lua_State *L, const char *type)
{
    if (lua_type(L, 2) == LUA_TSTRING)
        return luaL_error(L, "%s: unknown or read-only field '%s'.", type, lua_tostring(L, 2));

    return luaL_error(L, "%s: invalid key of type '%s'.", type, luaL_typename(L, 2));
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

#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM == 501

void luac_setmetatable (lua_State *L, const char *tname)
{
	luaL_checkstack(L, 1, "not enough stack slots");
	luaL_getmetatable(L, tname);
	lua_setmetatable(L, -2);
}

void luac_setfuncs (lua_State *L, const luaL_Reg *l, int nup)
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

void *luac_testudata (lua_State *L, int i, const char *tname)
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
