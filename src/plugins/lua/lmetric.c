// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"

#include <lua.h>
#include <lauxlib.h>

#include "utils.h"
#include "lmetric.h"

static int lmetric_unknown_double_new(lua_State *L)
{
    metric_t m = {0};

    m.time = 0;
    m.interval = 0;
    m.value = VALUE_UNKNOWN_FLOAT64(0);

    int argc = lua_gettop(L);

    if (argc < 1)
        return luaL_error(L, "The value is a required argument.");

    if ((argc > 0) && !lua_isnil(L, 1)) {
        if (!lua_isnumber(L, 1))
            return luaL_error(L, "Value must be a number.");
        m.value.unknown.float64 = luaL_checknumber(L, 1);
    }

    if ((argc > 1) && !lua_isnil(L, 2)) {
        if (!lua_istable(L, 2))
            return luaL_error(L, "Labels must be a table.");
        luac_to_labels(L, 2, &m.label);
    }

    if ((argc > 2) && !lua_isnil(L, 3)) {
        if (!lua_isnumber(L, 3)) {
            metric_reset(&m, METRIC_TYPE_UNKNOWN);
            return luaL_error(L, "Interval time must be a timestamp.");
        }
        m.time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
    }

    if ((argc > 3) && !lua_isnil(L, 4)) {
        if (!lua_isnumber(L, 4)) {
            metric_reset(&m, METRIC_TYPE_UNKNOWN);
            return luaL_error(L, "Interval must be a timestamp.");
        }
        m.interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 4));
    }

    metric_t *new_m = lua_newuserdata(L, sizeof(metric_t));

    memcpy(new_m, &m, sizeof(metric_t));

    luaL_setmetatable(L, MT_METRIC_UNKNOWN_DOUBLE);

    return 1;
}

static int lmetric_unknown_double_delete(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_UNKNOWN_DOUBLE);
    if (m == NULL)
        return 0;

    metric_reset(m, METRIC_TYPE_UNKNOWN);

    return 0;
}

static int lmetric_unknown_double_tostring_buffer(strbuf_t *buf, metric_t *m)
{
    int state = strbuf_putstr(buf, "MetricUnknownDouble{ ");
    state |= strbuf_putstr(buf, "value = ");
    state |= strbuf_putdouble(buf, m->value.unknown.float64);
    state |= strbuf_putstr(buf, ", time = ");
    state |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->time));
    state |= strbuf_putstr(buf, ", interval = ");
    state |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->interval));
    state |= strbuf_putstr(buf, ", labels = {");

    for (size_t i = 0; i < m->label.num; i++) {
        label_pair_t *pair = &m->label.ptr[i];
        if (i > 0)
            state |= strbuf_putchar(buf, ',');
        state |= strbuf_putstr(buf, " \"");
        state |= strbuf_putstr(buf, pair->name);
        state |= strbuf_putstr(buf, "\" = \"");
        state |= strbuf_putstr(buf, pair->value);
        state |= strbuf_putstr(buf, "\"");
    }

    return state | strbuf_putstr(buf, "} }");
}

static int lmetric_unknown_double_tostring(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_UNKNOWN_DOUBLE);
    if (m == NULL)
        return 0;

    strbuf_t buf = STRBUF_CREATE;

    lmetric_unknown_double_tostring_buffer(&buf, m);

    lua_pushstring(L, buf.ptr);

    strbuf_destroy(&buf);

    return 1;
}

static int lmetric_unknown_double_get(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_UNKNOWN_DOUBLE);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->time));
                return 1;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->interval));
                return 1;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                lua_createtable(L, 0, m->label.num);

                for (size_t i = 0; i < m->label.num; i++) {
                    label_pair_t *pair = &m->label.ptr[i];
                    lua_pushstring(L, pair->value);
                    lua_setfield(L, -2, pair->name);
                }

                return 1;
            }
            break;
        case 'v':
            if (strcmp(key, "value") == 0) {
                lua_pushnumber(L, (lua_Number)m->value.unknown.float64);
                return 1;
            }
            break;
        }
    }

    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {
        lua_getuservalue(L, 1);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
    }

    return 1;
}

static int lmetric_unknown_double_set(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_UNKNOWN_DOUBLE);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                m->time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                m->interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                if (!lua_istable(L, 3))
                    return luaL_error(L, "Labels must be a table.");
                label_set_reset(&m->label);
                luac_to_labels(L, 3, &m->label);
                return 0;
            }
            break;
        case 'v':
            if (strcmp(key, "value") == 0) {
                m->value.unknown.float64 = luaL_checknumber(L, 3);
                return 0;
            }
            break;
        }
    }

    lua_getuservalue(L, 1);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);

    return 0;
}

static luaL_Reg metric_unknown_double_reg[] = {
    { "new",         lmetric_unknown_double_new      },
    { "__gc",        lmetric_unknown_double_delete   },
    { "__tostring",  lmetric_unknown_double_tostring },
    { "__newindex",  lmetric_unknown_double_set      },
    { "__index",     lmetric_unknown_double_get      },
    { NULL,          NULL                            }
};

int register_metric_unknown_double(lua_State *L)
{
    luaL_newmetatable(L, MT_METRIC_UNKNOWN_DOUBLE);
    luaL_setfuncs(L, metric_unknown_double_reg, 0);

    return 1;
}

static int lmetric_unknown_integer_new(lua_State *L)
{
    metric_t m = {0};

    m.time = 0;
    m.interval = 0;
    m.value = VALUE_UNKNOWN_INT64(0);

    int argc = lua_gettop(L);

    if (argc < 1)
        return luaL_error(L, "The value is a required argument.");

    if ((argc > 0) && !lua_isnil(L, 1)) {
        if (!lua_isnumber(L, 1))
            return luaL_error(L, "Value must be a number.");
        m.value.unknown.int64 = luaL_checknumber(L, 1);
    }

    if ((argc > 1) && !lua_isnil(L, 2)) {
        if (!lua_istable(L, 2))
            return luaL_error(L, "Labels must be a table.");
        luac_to_labels(L, 2, &m.label);
    }

    if ((argc > 2) && !lua_isnil(L, 3)) {
        if (!lua_isnumber(L, 3)) {
            metric_reset(&m, METRIC_TYPE_UNKNOWN);
            return luaL_error(L, "Interval time must be a timestamp.");
        }
        m.time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
    }

    if ((argc > 3) && !lua_isnil(L, 4)) {
        if (!lua_isnumber(L, 4)) {
            metric_reset(&m, METRIC_TYPE_UNKNOWN);
            return luaL_error(L, "Interval must be a timestamp.");
        }
        m.interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 4));
    }

    metric_t *new_m = lua_newuserdata(L, sizeof(metric_t));

    memcpy(new_m, &m, sizeof(metric_t));

    luaL_setmetatable(L, MT_METRIC_UNKNOWN_INTEGER);

    return 1;
}

static int lmetric_unknown_integer_delete(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_UNKNOWN_INTEGER);
    if (m == NULL)
        return 0;

    metric_reset(m, METRIC_TYPE_UNKNOWN);

    return 0;
}

static int lmetric_unknown_integer_tostring_buffer(strbuf_t *buf, metric_t *m)
{
    int status = strbuf_putstr(buf, "MetricUnknownInteger{ ");
    status |= strbuf_putstr(buf, "value = ");
    status |= strbuf_putint(buf, m->value.unknown.int64);
    status |= strbuf_putstr(buf, ", time = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->time));
    status |= strbuf_putstr(buf, ", interval = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->interval));
    status |= strbuf_putstr(buf, ", labels = {");

    for (size_t i = 0; i < m->label.num; i++) {
        label_pair_t *pair = &m->label.ptr[i];
        if (i > 0)
            status |= strbuf_putchar(buf, ',');
        status |= strbuf_putstr(buf, " \"");
        status |= strbuf_putstr(buf, pair->name);
        status |= strbuf_putstr(buf, "\" = \"");
        status |= strbuf_putstr(buf, pair->value);
        status |= strbuf_putstr(buf, "\"");
    }

    return status | strbuf_putstr(buf, "} }");
}

static int lmetric_unknown_integer_tostring(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_UNKNOWN_INTEGER);
    if (m == NULL)
        return 0;

    strbuf_t buf = STRBUF_CREATE;

    lmetric_unknown_integer_tostring_buffer(&buf, m);

    lua_pushstring(L, buf.ptr);

    strbuf_destroy(&buf);

    return 1;
}

static int lmetric_unknown_integer_get(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_UNKNOWN_INTEGER);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->time));
                return 1;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->interval));
                return 1;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                lua_createtable(L, 0, m->label.num);

                for (size_t i = 0; i < m->label.num; i++) {
                    label_pair_t *pair = &m->label.ptr[i];
                    lua_pushstring(L, pair->value);
                    lua_setfield(L, -2, pair->name);
                }

                return 1;
            }
            break;
        case 'v':
            if (strcmp(key, "value") == 0) {
                lua_pushnumber(L, (lua_Number)m->value.unknown.int64);
                return 1;
            }
            break;
        }
    }

    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {
        lua_getuservalue(L, 1);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
    }

    return 1;
}

static int lmetric_unknown_integer_set(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_UNKNOWN_INTEGER);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                m->time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                m->interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                if (!lua_istable(L, 3))
                    return luaL_error(L, "Labels must be a table.");
                label_set_reset(&m->label);
                luac_to_labels(L, 3, &m->label);
                return 0;
            }
            break;
        case 'v':
            if (strcmp(key, "value") == 0) {
                m->value.unknown.int64 = luaL_checknumber(L, 3);
                return 0;
            }
            break;
        }
    }

    lua_getuservalue(L, 1);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);

    return 0;
}

static luaL_Reg metric_unknown_integer_reg[] = {
    { "new",         lmetric_unknown_integer_new      },
    { "__gc",        lmetric_unknown_integer_delete   },
    { "__tostring",  lmetric_unknown_integer_tostring },
    { "__newindex",  lmetric_unknown_integer_set      },
    { "__index",     lmetric_unknown_integer_get      },
    { NULL,          NULL                             }
};

int register_metric_unknown_interger(lua_State *L)
{
    luaL_newmetatable(L, MT_METRIC_UNKNOWN_INTEGER);
    luaL_setfuncs(L, metric_unknown_integer_reg, 0);

    return 1;
}

static int lmetric_gauge_double_new(lua_State *L)
{
    metric_t m = {0};

    m.time = 0;
    m.interval = 0;
    m.value = VALUE_GAUGE_FLOAT64(0);

    int argc = lua_gettop(L);

    if (argc < 1)
        return luaL_error(L, "The value is a required argument.");

    if ((argc > 0) && !lua_isnil(L, 1)) {
        if (!lua_isnumber(L, 1)) {
            return luaL_error(L, "Value must be a number.");
        }
        m.value.gauge.float64 = luaL_checknumber(L, 1);
    }

    if ((argc > 1) && !lua_isnil(L, 2)) {
        if (!lua_istable(L, 2))
            return luaL_error(L, "Labels must be a table.");
        luac_to_labels(L, 2, &m.label);
    }

    if ((argc > 2) && !lua_isnil(L, 3)) {
        if (!lua_isnumber(L, 3)) {
            metric_reset(&m, METRIC_TYPE_GAUGE);
            return luaL_error(L, "Interval time must be a timestamp.");
        }
        m.time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
    }

    if ((argc > 3) && !lua_isnil(L, 4)) {
        if (!lua_isnumber(L, 4)) {
            metric_reset(&m, METRIC_TYPE_GAUGE);
            return luaL_error(L, "Interval must be a timestamp.");
        }
        m.interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 4));
    }

    metric_t *new_m = lua_newuserdata(L, sizeof(metric_t));

    memcpy(new_m, &m, sizeof(metric_t));

    luaL_setmetatable(L, MT_METRIC_GAUGE_DOUBLE);

    return 1;
}

static int lmetric_gauge_double_delete(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_GAUGE_DOUBLE);
    if (m == NULL)
        return 0;

    metric_reset(m, METRIC_TYPE_GAUGE);

    return 0;
}

static int lmetric_gauge_double_tostring_buffer(strbuf_t *buf, metric_t *m)
{
    int status = strbuf_putstr(buf, "MetricGaugeDouble{ ");
    status |= strbuf_putstr(buf, "value = ");
    status |= strbuf_putdouble(buf, m->value.gauge.float64);
    status |= strbuf_putstr(buf, ", time = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->time));
    status |= strbuf_putstr(buf, ", interval = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->interval));
    status |= strbuf_putstr(buf, ", labels = {");

    for (size_t i = 0; i < m->label.num; i++) {
        label_pair_t *pair = &m->label.ptr[i];
        if (i > 0)
            status |= strbuf_putchar(buf, ',');
        status |= strbuf_putstr(buf, " \"");
        status |= strbuf_putstr(buf, pair->name);
        status |= strbuf_putstr(buf, "\" = \"");
        status |= strbuf_putstr(buf, pair->value);
        status |= strbuf_putstr(buf, "\"");
    }

    return status | strbuf_putstr(buf, "} }");
}

static int lmetric_gauge_double_tostring(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_GAUGE_DOUBLE);
    if (m == NULL)
        return 0;

    strbuf_t buf = STRBUF_CREATE;

    lmetric_gauge_double_tostring_buffer(&buf, m);

    lua_pushstring(L, buf.ptr);

    strbuf_destroy(&buf);

    return 1;
}

static int lmetric_gauge_double_get(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_GAUGE_DOUBLE);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->time));
                return 1;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->interval));
                return 1;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                lua_createtable(L, 0, m->label.num);

                for (size_t i = 0; i < m->label.num; i++) {
                    label_pair_t *pair = &m->label.ptr[i];
                    lua_pushstring(L, pair->value);
                    lua_setfield(L, -2, pair->name);
                }

                return 1;
            }
            break;
        case 'v':
            if (strcmp(key, "value") == 0) {
                lua_pushnumber(L, (lua_Number)m->value.gauge.float64);
                return 1;
            }
            break;
        }
    }

    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {
        lua_getuservalue(L, 1);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
    }

    return 1;
}

static int lmetric_gauge_double_set(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_GAUGE_DOUBLE);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                m->time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                m->interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                if (!lua_istable(L, 3))
                    return luaL_error(L, "Labels must be a table.");
                label_set_reset(&m->label);
                luac_to_labels(L, 3, &m->label);
                return 0;
            }
            break;
        case 'v':
            if (strcmp(key, "value") == 0) {
                m->value.gauge.float64 = luaL_checknumber(L, 3);
                return 0;
            }
            break;
        }
    }

    lua_getuservalue(L, 1);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);

    return 0;
}

static luaL_Reg metric_gauge_double_reg[] = {
    { "new",         lmetric_gauge_double_new      },
    { "__gc",        lmetric_gauge_double_delete   },
    { "__tostring",  lmetric_gauge_double_tostring },
    { "__newindex",  lmetric_gauge_double_set      },
    { "__index",     lmetric_gauge_double_get      },
    { NULL,          NULL                            }
};

int register_metric_gauge_double(lua_State *L)
{
    luaL_newmetatable(L, MT_METRIC_GAUGE_DOUBLE);
    luaL_setfuncs(L, metric_gauge_double_reg, 0);

    return 1;
}

static int lmetric_gauge_integer_new(lua_State *L)
{
    metric_t m = {0};

    m.time = 0;
    m.interval = 0;
    m.value = VALUE_GAUGE_INT64(0);

    int argc = lua_gettop(L);

    if (argc < 1)
        return luaL_error(L, "The value is a required argument.");

    if ((argc > 0) && !lua_isnil(L, 1)) {
        if (!lua_isnumber(L, 1))
            return luaL_error(L, "Value must be a number.");
        m.value.gauge.int64 = luaL_checknumber(L, 1);
    }

    if ((argc > 1) && !lua_isnil(L, 2)) {
        if (!lua_istable(L, 2))
            return luaL_error(L, "Labels must be a table.");
        luac_to_labels(L, 2, &m.label);
    }

    if ((argc > 2) && !lua_isnil(L, 3)) {
        if (!lua_isnumber(L, 3)) {
            metric_reset(&m, METRIC_TYPE_GAUGE);
            return luaL_error(L, "Interval time must be a timestamp.");
        }
        m.time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
    }

    if ((argc > 3) && !lua_isnil(L, 4)) {
        if (!lua_isnumber(L, 4)) {
            metric_reset(&m, METRIC_TYPE_GAUGE);
            return luaL_error(L, "Interval must be a timestamp.");
        }
        m.interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 4));
    }

    metric_t *new_m = lua_newuserdata(L, sizeof(metric_t));

    memcpy(new_m, &m, sizeof(metric_t));

    luaL_setmetatable(L, MT_METRIC_GAUGE_INTEGER);

    return 1;
}

static int lmetric_gauge_integer_delete(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_GAUGE_INTEGER);
    if (m == NULL)
        return 0;

    metric_reset(m, METRIC_TYPE_GAUGE);

    return 0;
}

static int lmetric_gauge_integer_tostring_buffer(strbuf_t *buf, metric_t *m)
{
    int status = strbuf_putstr(buf, "MetricGaugeInteger{ ");
    status |= strbuf_putstr(buf, "value = ");
    status |= strbuf_putint(buf, m->value.gauge.int64);
    status |= strbuf_putstr(buf, ", time = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->time));
    status |= strbuf_putstr(buf, ", interval = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->interval));
    status |= strbuf_putstr(buf, ", labels = {");

    for (size_t i = 0; i < m->label.num; i++) {
        label_pair_t *pair = &m->label.ptr[i];
        if (i > 0)
            status |= strbuf_putchar(buf, ',');
        status |= strbuf_putstr(buf, " \"");
        status |= strbuf_putstr(buf, pair->name);
        status |= strbuf_putstr(buf, "\" = \"");
        status |= strbuf_putstr(buf, pair->value);
        status |= strbuf_putstr(buf, "\"");
    }

    return status | strbuf_putstr(buf, "} }");
}

static int lmetric_gauge_integer_tostring(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_GAUGE_INTEGER);
    if (m == NULL)
        return 0;

    strbuf_t buf = STRBUF_CREATE;

    lmetric_gauge_integer_tostring_buffer(&buf, m);

    lua_pushstring(L, buf.ptr);

    strbuf_destroy(&buf);

    return 1;
}

static int lmetric_gauge_integer_get(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_GAUGE_INTEGER);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->time));
                return 1;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->interval));
                return 1;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                lua_createtable(L, 0, m->label.num);

                for (size_t i = 0; i < m->label.num; i++) {
                    label_pair_t *pair = &m->label.ptr[i];
                    lua_pushstring(L, pair->value);
                    lua_setfield(L, -2, pair->name);
                }

                return 1;
            }
            break;
        case 'v':
            if (strcmp(key, "value") == 0) {
                lua_pushnumber(L, (lua_Number)m->value.gauge.int64);
                return 1;
            }
            break;
        }
    }

    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {
        lua_getuservalue(L, 1);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
    }

    return 1;
}

static int lmetric_gauge_integer_set(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_GAUGE_INTEGER);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                m->time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                m->interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                if (!lua_istable(L, 3))
                    return luaL_error(L, "Labels must be a table.");
                label_set_reset(&m->label);
                luac_to_labels(L, 3, &m->label);
                return 0;
            }
            break;
        case 'v':
            if (strcmp(key, "value") == 0) {
                m->value.gauge.int64 = luaL_checknumber(L, 3);
                return 0;
            }
            break;
        }
    }

    lua_getuservalue(L, 1);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);

    return 0;
}

static luaL_Reg metric_gauge_integer_reg[] = {
    { "new",         lmetric_gauge_integer_new      },
    { "__gc",        lmetric_gauge_integer_delete   },
    { "__tostring",  lmetric_gauge_integer_tostring },
    { "__newindex",  lmetric_gauge_integer_set      },
    { "__index",     lmetric_gauge_integer_get      },
    { NULL,          NULL                           }
};

int register_metric_gauge_interger(lua_State *L)
{
    luaL_newmetatable(L, MT_METRIC_GAUGE_INTEGER);
    luaL_setfuncs(L, metric_gauge_integer_reg, 0);

    return 1;
}

static int lmetric_counter_double_new(lua_State *L)
{
    metric_t m = {0};

    m.time = 0;
    m.interval = 0;
    m.value = VALUE_COUNTER_FLOAT64(0);

    int argc = lua_gettop(L);

    if (argc < 1)
        return luaL_error(L, "The value is a required argument.");

    if ((argc > 0) && !lua_isnil(L, 1)) {
        if (!lua_isnumber(L, 1))
            return luaL_error(L, "Value must be a number.");
        m.value.counter.float64 = luaL_checknumber(L, 1);
    }

    if ((argc > 1) && !lua_isnil(L, 2)) {
        if (!lua_istable(L, 2))
            return luaL_error(L, "Labels must be a table.");
        luac_to_labels(L, 2, &m.label);
    }

    if ((argc > 2) && !lua_isnil(L, 3)) {
        if (!lua_isnumber(L, 3)) {
            metric_reset(&m, METRIC_TYPE_COUNTER);
            return luaL_error(L, "Interval time must be a timestamp.");
        }
        m.time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
    }

    if ((argc > 3) && !lua_isnil(L, 4)) {
        if (!lua_isnumber(L, 4)) {
            metric_reset(&m, METRIC_TYPE_COUNTER);
            return luaL_error(L, "Interval must be a timestamp.");
        }
        m.interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 4));
    }

    metric_t *new_m = lua_newuserdata(L, sizeof(metric_t));

    memcpy(new_m, &m, sizeof(metric_t));

    luaL_setmetatable(L, MT_METRIC_COUNTER_DOUBLE);

    return 1;
}

static int lmetric_counter_double_delete(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_COUNTER_DOUBLE);
    if (m == NULL)
        return 0;

    metric_reset(m, METRIC_TYPE_COUNTER);

    return 0;
}

static int lmetric_counter_double_tostring_buffer(strbuf_t *buf, metric_t *m)
{
    int status = strbuf_putstr(buf, "MetricCounterDouble{ ");
    status |= strbuf_putstr(buf, "value = ");
    status |= strbuf_putdouble(buf, m->value.counter.float64);
    status |= strbuf_putstr(buf, ", time = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->time));
    status |= strbuf_putstr(buf, ", interval = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->interval));
    status |= strbuf_putstr(buf, ", labels = {");

    for (size_t i = 0; i < m->label.num; i++) {
        label_pair_t *pair = &m->label.ptr[i];
        if (i > 0)
            status |= strbuf_putchar(buf, ',');
        status |= strbuf_putstr(buf, " \"");
        status |= strbuf_putstr(buf, pair->name);
        status |= strbuf_putstr(buf, "\" = \"");
        status |= strbuf_putstr(buf, pair->value);
        status |= strbuf_putstr(buf, "\"");
    }

    return status | strbuf_putstr(buf, "} }");
}

static int lmetric_counter_double_tostring(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_COUNTER_DOUBLE);
    if (m == NULL)
        return 0;

    strbuf_t buf = STRBUF_CREATE;

    lmetric_counter_double_tostring_buffer(&buf, m);

    lua_pushstring(L, buf.ptr);

    strbuf_destroy(&buf);

    return 1;
}

static int lmetric_counter_double_get(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_COUNTER_DOUBLE);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->time));
                return 1;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->interval));
                return 1;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                lua_createtable(L, 0, m->label.num);

                for (size_t i = 0; i < m->label.num; i++) {
                    label_pair_t *pair = &m->label.ptr[i];
                    lua_pushstring(L, pair->value);
                    lua_setfield(L, -2, pair->name);
                }

                return 1;
            }
            break;
        case 'v':
            if (strcmp(key, "value") == 0) {
                lua_pushnumber(L, (lua_Number)m->value.counter.float64);
                return 1;
            }
            break;
        }
    }

    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {
        lua_getuservalue(L, 1);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
    }

    return 1;
}

static int lmetric_counter_double_set(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_COUNTER_DOUBLE);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                m->time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                m->interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                if (!lua_istable(L, 3))
                    return luaL_error(L, "Labels must be a table.");
                label_set_reset(&m->label);
                luac_to_labels(L, 3, &m->label);
                return 0;
            }
            break;
        case 'v':
            if (strcmp(key, "value") == 0) {
                m->value.counter.float64 = luaL_checknumber(L, 3);
                return 0;
            }
            break;
        }
    }

    lua_getuservalue(L, 1);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);

    return 0;
}

static luaL_Reg metric_counter_double_reg[] = {
    { "new",        lmetric_counter_double_new      },
    { "__gc",       lmetric_counter_double_delete   },
    { "__tostring", lmetric_counter_double_tostring },
    { "__newindex", lmetric_counter_double_set      },
    { "__index",    lmetric_counter_double_get      },
    { NULL,         NULL                            }
};

int register_metric_counter_double(lua_State *L)
{
    luaL_newmetatable(L, MT_METRIC_COUNTER_DOUBLE);
    luaL_setfuncs(L, metric_counter_double_reg, 0);

    return 1;
}

static int lmetric_counter_integer_new(lua_State *L)
{
    metric_t m = {0};

    m.time = 0;
    m.interval = 0;
    m.value = VALUE_COUNTER_UINT64(0);

    int argc = lua_gettop(L);

    if (argc < 1)
        return luaL_error(L, "The value is a required argument.");

    if ((argc > 0) && !lua_isnil(L, 1)) {
        if (!lua_isnumber(L, 1))
            return luaL_error(L, "Value must be a number.");
        m.value.counter.uint64 = luaL_checknumber(L, 1);
    }

    if ((argc > 1) && !lua_isnil(L, 2)) {
        if (!lua_istable(L, 2))
            return luaL_error(L, "Labels must be a table.");
        luac_to_labels(L, 2, &m.label);
    }

    if ((argc > 2) && !lua_isnil(L, 3)) {
        if (!lua_isnumber(L, 3)) {
            metric_reset(&m, METRIC_TYPE_COUNTER);
            return luaL_error(L, "Interval time must be a timestamp.");
        }
        m.time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
    }

    if ((argc > 3) && !lua_isnil(L, 4)) {
        if (!lua_isnumber(L, 4)) {
            metric_reset(&m, METRIC_TYPE_COUNTER);
            return luaL_error(L, "Interval must be a timestamp.");
        }
        m.interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 4));
    }

    metric_t *new_m = lua_newuserdata(L, sizeof(metric_t));

    memcpy(new_m, &m, sizeof(metric_t));

    luaL_setmetatable(L, MT_METRIC_COUNTER_INTEGER);

    return 1;
}

static int lmetric_counter_integer_delete(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_COUNTER_INTEGER);
    if (m == NULL)
        return 0;

    metric_reset(m, METRIC_TYPE_COUNTER);

    return 0;
}

static int lmetric_counter_integer_tostring_buffer(strbuf_t *buf, metric_t *m)
{
    int status = strbuf_putstr(buf, "MetricCounterInteger{ ");
    status |= strbuf_putstr(buf, "value = ");
    status |= strbuf_putuint(buf, m->value.counter.uint64);
    status |= strbuf_putstr(buf, ", time = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->time));
    status |= strbuf_putstr(buf, ", interval = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->interval));
    status |= strbuf_putstr(buf, ", labels = {");

    for (size_t i = 0; i < m->label.num; i++) {
        label_pair_t *pair = &m->label.ptr[i];
        if (i > 0)
            status |= strbuf_putchar(buf, ',');
        status |= strbuf_putstr(buf, " \"");
        status |= strbuf_putstr(buf, pair->name);
        status |= strbuf_putstr(buf, "\" = \"");
        status |= strbuf_putstr(buf, pair->value);
        status |= strbuf_putstr(buf, "\"");
    }

    return status | strbuf_putstr(buf, "} }");
}

static int lmetric_counter_integer_tostring(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_COUNTER_INTEGER);
    if (m == NULL)
        return 0;

    strbuf_t buf = STRBUF_CREATE;

    lmetric_counter_integer_tostring_buffer(&buf, m);

    lua_pushstring(L, buf.ptr);

    strbuf_destroy(&buf);

    return 1;
}

static int lmetric_counter_integer_get(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_COUNTER_INTEGER);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->time));
                return 1;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->interval));
                return 1;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                lua_createtable(L, 0, m->label.num);

                for (size_t i = 0; i < m->label.num; i++) {
                    label_pair_t *pair = &m->label.ptr[i];
                    lua_pushstring(L, pair->value);
                    lua_setfield(L, -2, pair->name);
                }

                return 1;
            }
            break;
        case 'v':
            if (strcmp(key, "value") == 0) {
                lua_pushnumber(L, (lua_Number)m->value.counter.uint64);
                return 1;
            }
            break;
        }
    }

    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {
        lua_getuservalue(L, 1);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
    }

    return 1;
}

static int lmetric_counter_integer_set(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_COUNTER_INTEGER);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                m->time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                m->interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                if (!lua_istable(L, 3))
                    return luaL_error(L, "Labels must be a table.");
                label_set_reset(&m->label);
                luac_to_labels(L, 3, &m->label);
                return 0;
            }
            break;
        case 'v':
            if (strcmp(key, "value") == 0) {
                m->value.counter.uint64 = luaL_checknumber(L, 3);
                return 0;
            }
            break;
        }
    }

    lua_getuservalue(L, 1);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);

    return 0;
}

static luaL_Reg metric_counter_integer_reg[] = {
    { "new",         lmetric_counter_integer_new      },
    { "__gc",        lmetric_counter_integer_delete   },
    { "__tostring",  lmetric_counter_integer_tostring },
    { "__newindex",  lmetric_counter_integer_set      },
    { "__index",     lmetric_counter_integer_get      },
    { NULL,          NULL                             }
};

int register_metric_counter_interger(lua_State *L)
{
    luaL_newmetatable(L, MT_METRIC_COUNTER_INTEGER);
    luaL_setfuncs(L, metric_counter_integer_reg, 0);

    return 1;
}

static int lmetric_info_new(lua_State *L)
{
    metric_t m = {0};

    m.time = 0;
    m.interval = 0;

    int argc = lua_gettop(L);

    if (argc < 1)
        return luaL_error(L, "The info is a required argument.");

    if ((argc > 0) && !lua_isnil(L, 1)) {
        if (!lua_istable(L, 1))
            return luaL_error(L, "Info must be a table.");
        luac_to_labels(L, 1, &m.value.info);
    }

    if ((argc > 1) && !lua_isnil(L, 2)) {
        if (!lua_istable(L, 2))
            return luaL_error(L, "Labels must be a table.");
        luac_to_labels(L, 2, &m.label);
    }

    if ((argc > 2) && !lua_isnil(L, 3)) {
        if (!lua_isnumber(L, 3)) {
            metric_reset(&m, METRIC_TYPE_INFO);
            return luaL_error(L, "Interval time must be a timestamp.");
        }
        m.time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
    }

    if ((argc > 3) && !lua_isnil(L, 4)) {
        if (!lua_isnumber(L, 4)) {
            metric_reset(&m, METRIC_TYPE_INFO);
            return luaL_error(L, "Interval must be a timestamp.");
        }
        m.interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 4));
    }

    metric_t *new_m = lua_newuserdata(L, sizeof(metric_t));

    memcpy(new_m, &m, sizeof(metric_t));

    luaL_setmetatable(L, MT_METRIC_INFO);

    return 1;
}

static int lmetric_info_delete(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_INFO);
    if (m == NULL)
        return 0;

    metric_reset(m, METRIC_TYPE_INFO);

    return 0;
}

static int lmetric_info_tostring_buffer(strbuf_t *buf, metric_t *m)
{
    int status = strbuf_putstr(buf, "MetricInfo{ ");
    status |= strbuf_putstr(buf, "value = {");

    for (size_t i = 0; i < m->value.info.num; i++) {
        label_pair_t *pair = &m->value.info.ptr[i];
        if (i > 0)
            status |= strbuf_putchar(buf, ',');
        status |= strbuf_putstr(buf, " \"");
        status |= strbuf_putstr(buf, pair->name);
        status |= strbuf_putstr(buf, "\" = \"");
        status |= strbuf_putstr(buf, pair->value);
        status |= strbuf_putstr(buf, "\"");
    }

    status |= strbuf_putstr(buf, "}, time = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->time));
    status |= strbuf_putstr(buf, ", interval = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->interval));
    status |= strbuf_putstr(buf, ", labels = {");

    for (size_t i = 0; i < m->label.num; i++) {
        label_pair_t *pair = &m->label.ptr[i];
        if (i > 0)
            status |= strbuf_putchar(buf, ',');
        status |= strbuf_putstr(buf, " \"");
        status |= strbuf_putstr(buf, pair->name);
        status |= strbuf_putstr(buf, "\" = \"");
        status |= strbuf_putstr(buf, pair->value);
        status |= strbuf_putstr(buf, "\"");
    }

    return status | strbuf_putstr(buf, "} }");
}

static int lmetric_info_tostring(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_INFO);
    if (m == NULL)
        return 0;

    strbuf_t buf = STRBUF_CREATE;

    lmetric_info_tostring_buffer(&buf, m);

    lua_pushstring(L, buf.ptr);

    strbuf_destroy(&buf);

    return 1;
}

static int lmetric_info_get(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_INFO);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->time));
                return 1;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->interval));
                return 1;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                lua_createtable(L, 0, m->label.num);

                for (size_t i = 0; i < m->label.num; i++) {
                    label_pair_t *pair = &m->label.ptr[i];
                    lua_pushstring(L, pair->value);
                    lua_setfield(L, -2, pair->name);
                }

                return 1;
            }
            break;
        case 'v':
            if (strcmp(key, "value") == 0) {
                lua_createtable(L, 0, m->value.info.num);

                for (size_t i = 0; i < m->value.info.num; i++) {
                    label_pair_t *pair = &m->value.info.ptr[i];
                    lua_pushstring(L, pair->value);
                    lua_setfield(L, -2, pair->name);
                }

                return 1;
            }
            break;
        }
    }

    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {
        lua_getuservalue(L, 1);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
    }

    return 1;
}

static int lmetric_info_set(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_INFO);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                m->time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                m->interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                if (!lua_istable(L, 3))
                    return luaL_error(L, "Labels must be a table.");
                label_set_reset(&m->label);
                luac_to_labels(L, 3, &m->label);
                return 0;
            }
            break;
        case 'v':
            if (strcmp(key, "value") == 0) {
                if (!lua_istable(L, 3))
                    return luaL_error(L, "Value must be a table.");
                label_set_reset(&m->value.info);
                luac_to_labels(L, 3, &m->value.info);
                return 0;
            }
            break;
        }
    }

    lua_getuservalue(L, 1);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);

    return 0;
}

static luaL_Reg metric_info_reg[] = {
    { "new",         lmetric_info_new      },
    { "__gc",        lmetric_info_delete   },
    { "__tostring",  lmetric_info_tostring },
    { "__newindex",  lmetric_info_set      },
    { "__index",     lmetric_info_get      },
    { NULL,          NULL                  }
};

int register_metric_info(lua_State *L)
{
    luaL_newmetatable(L, MT_METRIC_INFO);
    luaL_setfuncs(L, metric_info_reg, 0);

    return 1;
}

int luac_to_state_set(lua_State *L, int idx, state_set_t *set)
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
        if (ktype != LUA_TSTRING)
            return luaL_error(L, "State set key '%s' must be LUA_TSTRING.", lua_tostring(L, -2));

        const char *key = lua_tostring(L, -2);
        if(key == NULL) {
            PLUGIN_WARNING("State set key must be not null.");
            lua_pop(L, 1);
            continue;
        }

        if (lua_type(L, -1) != LUA_TBOOLEAN)
            return luaL_error(L, "State set value ''%s' must be LUA_TBOOLEAN.", lua_tostring(L, -2));

        state_set_add(set, key, lua_toboolean(L, -1));

        lua_pop(L, 1);
    }

    return 0;
}

static int lmetric_state_set_new(lua_State *L)
{
    metric_t m = {0};

    m.time = 0;
    m.interval = 0;

    int argc = lua_gettop(L);

    if (argc < 1)
        return luaL_error(L, "The stateset is a required argument.");

    if ((argc > 0) && !lua_isnil(L, 1)) {
        if (!lua_istable(L, 1))
            return luaL_error(L, "State set must be a table.");
        luac_to_state_set(L, 1, &m.value.state_set);
    }

    if ((argc > 1) && !lua_isnil(L, 2)) {
        if (!lua_istable(L, 2))
            return luaL_error(L, "Labels must be a table.");
        luac_to_labels(L, 2, &m.label);
    }

    if ((argc > 2) && !lua_isnil(L, 3)) {
        if (!lua_isnumber(L, 3)) {
            metric_reset(&m, METRIC_TYPE_STATE_SET);
            return luaL_error(L, "Interval time must be a timestamp.");
        }
        m.time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
    }

    if ((argc > 3) && !lua_isnil(L, 4)) {
        if (!lua_isnumber(L, 4)) {
            metric_reset(&m, METRIC_TYPE_STATE_SET);
            return luaL_error(L, "Interval must be a timestamp.");
        }
        m.interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 4));
    }

    metric_t *new_m = lua_newuserdata(L, sizeof(metric_t));

    memcpy(new_m, &m, sizeof(metric_t));

    luaL_setmetatable(L, MT_METRIC_STATE_SET);

    return 1;
}

static int lmetric_state_set_delete(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_STATE_SET);
    if (m == NULL)
        return 0;

    metric_reset(m, METRIC_TYPE_STATE_SET);

    return 0;
}

static int lmetric_state_set_tostring_buffer(strbuf_t *buf, metric_t *m)
{
    int status = strbuf_putstr(buf, "MetricStateSet{ ");
    status |= strbuf_putstr(buf, "value = {");
    for (size_t i = 0; i < m->value.info.num; i++) {
        label_pair_t *pair = &m->value.info.ptr[i];
        if (i > 0)
            status |= strbuf_putchar(buf, ',');
        status |=  strbuf_putstr(buf, " \"");
        status |= strbuf_putstr(buf, pair->name);
        status |= strbuf_putstr(buf, "\" = \"");
        status |= strbuf_putstr(buf, pair->value);
        status |= strbuf_putstr(buf, "\"");
    }
    status |= strbuf_putstr(buf, "}, time = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->time));
    status |= strbuf_putstr(buf, ", interval = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->interval));
    status |= strbuf_putstr(buf, ", labels = {");

    for (size_t i = 0; i < m->label.num; i++) {
        label_pair_t *pair = &m->label.ptr[i];
        if (i > 0)
            status |= strbuf_putchar(buf, ',');
        status |= strbuf_putstr(buf, " \"");
        status |= strbuf_putstr(buf, pair->name);
        status |= strbuf_putstr(buf, "\" = \"");
        status |= strbuf_putstr(buf, pair->value);
        status |= strbuf_putstr(buf, "\"");
    }

    return status | strbuf_putstr(buf, "} }");
}

static int lmetric_state_set_tostring(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_STATE_SET);
    if (m == NULL)
        return 0;

    strbuf_t buf = STRBUF_CREATE;

    lmetric_state_set_tostring_buffer(&buf, m);

    lua_pushstring(L, buf.ptr);

    strbuf_destroy(&buf);

    return 1;
}

static int lmetric_state_set_get(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_STATE_SET);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->time));
                return 1;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->interval));
                return 1;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                lua_createtable(L, 0, m->label.num);

                for (size_t i = 0; i < m->label.num; i++) {
                    label_pair_t *pair = &m->label.ptr[i];
                    lua_pushstring(L, pair->value);
                    lua_setfield(L, -2, pair->name);
                }

                return 1;
            }
            break;
        case 'v':
            if (strcmp(key, "value") == 0) {
                lua_createtable(L, 0, m->value.state_set.num);

                for (size_t i = 0; i < m->value.state_set.num; i++) {
                    lua_pushboolean(L, m->value.state_set.ptr[i].enabled);
                    lua_setfield(L, -2, m->value.state_set.ptr[i].name);
                }

                return 1;
            }
            break;
        }
    }

    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {
        lua_getuservalue(L, 1);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
    }

    return 1;
}

static int lmetric_state_set_set(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_STATE_SET);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                m->time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                m->interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                if (!lua_istable(L, 3))
                    return luaL_error(L, "Labels must be a table.");
                label_set_reset(&m->label);
                luac_to_labels(L, 3, &m->label);
                return 0;
            }
            break;
        case 'v':
            if (strcmp(key, "value") == 0) {
                if (!lua_istable(L, 3))
                    return luaL_error(L, "Value must be a table.");
                state_set_reset(&m->value.state_set);
                luac_to_state_set(L, 3, &m->value.state_set);
                return 0;
            }
            break;
        }
    }

    lua_getuservalue(L, 1);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);

    return 0;
}

static luaL_Reg metric_state_set_reg[] = {
    { "new",        lmetric_state_set_new      },
    { "__gc",       lmetric_state_set_delete   },
    { "__tostring", lmetric_state_set_tostring },
    { "__newindex", lmetric_state_set_set      },
    { "__index",    lmetric_state_set_get      },
    { NULL,         NULL                  }
};

int register_metric_state_set(lua_State *L)
{
    luaL_newmetatable(L, MT_METRIC_STATE_SET);
    luaL_setfuncs(L, metric_state_set_reg, 0);

    return 1;
}

static int luac_to_quantiles(lua_State *L, int idx, double *quantile, double *value)
{
    if (idx < 1)
        idx += lua_gettop(L) + 1;

    /* Check that idx is in the valid range */
    if ((idx < 1) || (idx > lua_gettop(L))) {
        PLUGIN_DEBUG("idx(%d), top(%d)", idx, lua_gettop(L));
        return -1;
    }
    
    size_t size = lua_rawlen(L, idx);
    if (size != 2) {
        return luaL_error(L, "Quantile must be an array of two: the quantile and the value.");
    }

    lua_rawgeti(L, idx, 1);
    *quantile = luaL_checknumber(L, -1);
    lua_pop(L, 1);

    lua_rawgeti(L, idx, 2);
    *value = luaL_checknumber(L, -1);
    lua_pop(L, 1);
    
    return 0;
}

static summary_t *luac_to_summary_quantiles(lua_State *L, int idx, summary_t *summary)
{
    if (idx < 1)
        idx += lua_gettop(L) + 1;

    /* Check that idx is in the valid range */
    if ((idx < 1) || (idx > lua_gettop(L))) {
        PLUGIN_DEBUG("idx(%d), top(%d)", idx, lua_gettop(L));
        return summary;
    }

    size_t size = lua_rawlen(L, idx);
    for (size_t i = 0; i < size; i++) {
        lua_rawgeti(L, idx, i + 1);
        double quantile = 0;
        double value = 0;
        if (luac_to_quantiles(L, -1, &quantile, &value) == 0)
            summary = summary_quantile_append(summary, quantile, value);
        lua_pop(L, 1);
    }

    return summary;
}

static int lmetric_summary_new(lua_State *L)
{
    metric_t m = {0};

    m.time = 0;
    m.interval = 0;
    m.value.summary = summary_new();
    if (m.value.summary == NULL)
        return luaL_error(L, "Fail to create summary.");

    int argc = lua_gettop(L);

    if (argc < 3)
        return luaL_error(L, "The sum, count and quantiles are required arguments.");

    if ((argc > 0) && !lua_isnil(L, 1)) {
        if (!lua_isnumber(L, 1)) {
            metric_reset(&m, METRIC_TYPE_SUMMARY);
            return luaL_error(L, "Sum must be a number.");
        }
        m.value.summary->sum = luaL_checknumber(L, 1);
    }

    if ((argc > 1) && !lua_isnil(L, 2)) {
        if (!lua_isnumber(L, 2)) {
            metric_reset(&m, METRIC_TYPE_SUMMARY);
            return luaL_error(L, "Count must be a number.");
        }
        m.value.summary->count = luaL_checknumber(L, 2);
    }

    if ((argc > 2) && !lua_isnil(L, 3)) {
        if (!luac_isarray(L, 3)) {
            metric_reset(&m, METRIC_TYPE_SUMMARY);
            return luaL_error(L, "Quantiles must be an array.");
        }
        m.value.summary = luac_to_summary_quantiles(L, 3, m.value.summary);
    }

    if ((argc > 3) && !lua_isnil(L, 4)) {
        if (!lua_istable(L, 4))
            return luaL_error(L, "Labels must be a table.");
        luac_to_labels(L, 4, &m.label);
    }

    if ((argc > 4) && !lua_isnil(L, 5)) {
        if (!lua_isnumber(L, 5)) {
            metric_reset(&m, METRIC_TYPE_SUMMARY);
            return luaL_error(L, "Time must be a timestamp.");
        }
        m.time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 5));
    }

    if ((argc > 5) && !lua_isnil(L, 6)) {
        if (!lua_isnumber(L, 6)) {
            metric_reset(&m, METRIC_TYPE_SUMMARY);
            return luaL_error(L, "Interval must be a timestamp.");
        }
        m.interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 6));
    }

    metric_t *new_m = lua_newuserdata(L, sizeof(metric_t));

    memcpy(new_m, &m, sizeof(metric_t));

    luaL_setmetatable(L, MT_METRIC_SUMMARY);

    return 1;
}

static int lmetric_summary_delete(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_SUMMARY);
    if (m == NULL)
        return 0;

    metric_reset(m, METRIC_TYPE_SUMMARY);

    return 0;
}

static int lmetric_summary_tostring_buffer(strbuf_t *buf, metric_t *m)
{
    int status = strbuf_putstr(buf, "MetricSummary{ ");
    status |= strbuf_putstr(buf, "sum = ");
    status |= strbuf_putdouble(buf, m->value.summary->sum);
    status |= strbuf_putstr(buf, "count = ");
    status |= strbuf_putuint(buf, m->value.summary->count);
    status |= strbuf_putstr(buf, "quantiles = [");
    for (size_t i = 0; i < m->value.summary->num; i++) {
        summary_quantile_t *q = &m->value.summary->quantiles[i];
        if (i > 0)
            status |= strbuf_putchar(buf, ',');
        status |= strbuf_putstr(buf, " [");
        status |= strbuf_putdouble(buf, q->quantile);
        status |= strbuf_putstr(buf, ", ");
        status |= strbuf_putdouble(buf, q->value);
        status |= strbuf_putstr(buf, "]");
    }
    status |= strbuf_putstr(buf, "], time = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->time));
    status |= strbuf_putstr(buf, ", interval = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->interval));
    status |= strbuf_putstr(buf, ", labels = {");

    for (size_t i = 0; i < m->label.num; i++) {
        label_pair_t *pair = &m->label.ptr[i];
        if (i > 0)
            status |= strbuf_putchar(buf, ',');
        status |= strbuf_putstr(buf, " \"");
        status |= strbuf_putstr(buf, pair->name);
        status |= strbuf_putstr(buf, "\" = \"");
        status |= strbuf_putstr(buf, pair->value);
        status |= strbuf_putstr(buf, "\"");
    }

    return status | strbuf_putstr(buf, "} }");
}

static int lmetric_summary_tostring(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_SUMMARY);
    if (m == NULL)
        return 0;

    strbuf_t buf = STRBUF_CREATE;

    lmetric_summary_tostring_buffer(&buf, m);

    lua_pushstring(L, buf.ptr);

    strbuf_destroy(&buf);

    return 1;
}

static int lmetric_summary_get(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_SUMMARY);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->time));
                return 1;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->interval));
                return 1;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                lua_createtable(L, 0, m->label.num);

                for (size_t i = 0; i < m->label.num; i++) {
                    label_pair_t *pair = &m->label.ptr[i];
                    lua_pushstring(L, pair->value);
                    lua_setfield(L, -2, pair->name);
                }

                return 1;
            }
            break;
        case 's':
            if (strcmp(key, "sum") == 0) {
                lua_pushnumber(L, (lua_Number)m->value.summary->sum);
                return 1;
            }
            break;
        case 'c':
            if (strcmp(key, "count") == 0) {
                lua_pushnumber(L, (lua_Number)m->value.summary->count);
                return 1;
            }
            break;
        case 'q':
            if (strcmp(key, "quantiles") == 0) {
                lua_createtable(L, m->value.summary->num, 0);
                for (size_t i = 0; i < m->value.summary->num; i++) {
                    summary_quantile_t *q = &m->value.summary->quantiles[i];

                    lua_createtable(L, 2, 0);
                    lua_pushnumber(L, (lua_Number)q->quantile);
                    lua_rawseti (L, -2, 1);
                    lua_pushnumber(L, (lua_Number)q->value);
                    lua_rawseti (L, -2, 2);

                    lua_rawseti (L, -2, i+1);
                }
                return 1;
            }
            break;
        }
    }

    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {
        lua_getuservalue(L, 1);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
    }

    return 1;
}

static int lmetric_summary_set(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_SUMMARY);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                m->time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                m->interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                if (!lua_istable(L, 3))
                    return luaL_error(L, "Labels must be a table.");
                label_set_reset(&m->label);
                luac_to_labels(L, 3, &m->label);
                return 0;
            }
            break;
        case 's':
            if (strcmp(key, "sum") == 0) {
                m->value.summary->sum = luaL_checknumber(L, 3);
                return 0;
            }
            break;
        case 'c':
            if (strcmp(key, "count") == 0) {
                m->value.summary->count = luaL_checknumber(L, 3);
                return 0;
            }
            break;
        case 'q':
            if (strcmp(key, "quantiles") == 0) {
                m->value.summary = luac_to_summary_quantiles(L, 3, m->value.summary);
                return 0;
            }
            break;
        }
    }

    lua_getuservalue(L, 1);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);

    return 0;
}

static luaL_Reg metric_summary_reg[] = {
    { "new",        lmetric_summary_new      },
    { "__gc",       lmetric_summary_delete   },
    { "__tostring", lmetric_summary_tostring },
    { "__newindex", lmetric_summary_set      },
    { "__index",    lmetric_summary_get      },
    { NULL,         NULL                     }
};

int register_metric_summary(lua_State *L)
{
    luaL_newmetatable(L, MT_METRIC_SUMMARY);
    luaL_setfuncs(L, metric_summary_reg, 0);

    return 1;
}

static int luac_to_bucket(lua_State *L, int idx, uint64_t *counter, double *maximum)
{
    if (idx < 1)
        idx += lua_gettop(L) + 1;

    /* Check that idx is in the valid range */
    if ((idx < 1) || (idx > lua_gettop(L))) {
        PLUGIN_DEBUG("idx(%d), top(%d)", idx, lua_gettop(L));
        return -1;
    }

    size_t size = lua_rawlen(L, idx);
    if (size != 2) {
        return luaL_error(L, "Bucket must be an array of two: the counter and the maximum.");
    }

    lua_rawgeti(L, idx, 1);
    *counter = luaL_checknumber(L, -1);
    lua_pop(L, 1);

    lua_rawgeti(L, idx, 2);
    *maximum = luaL_checknumber(L, -1);
    lua_pop(L, 1);

    return 0;
}

static histogram_t *luac_to_histogram_buckets(lua_State *L, int idx, histogram_t *histogram)
{
    if (idx < 1)
        idx += lua_gettop(L) + 1;

    /* Check that idx is in the valid range */
    if ((idx < 1) || (idx > lua_gettop(L))) {
        PLUGIN_DEBUG("idx(%d), top(%d)", idx, lua_gettop(L));
        return histogram;
    }

    size_t size = lua_rawlen(L, idx);
    for (size_t i = 0; i < size; i++) {
        lua_rawgeti(L, idx, i + 1);
        uint64_t counter = 0;
        double maximum = 0;
        if (luac_to_bucket(L, -1, &counter, &maximum) == 0)
            histogram = histogram_bucket_append(histogram, counter, maximum);
        lua_pop(L, 1);
    }

    return histogram;
}

static int lmetric_histogram_new(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc < 3)
        return luaL_error(L, "The sum and buckets are required arguments.");

    metric_t m = {0};
    m.time = 0;
    m.interval = 0;
    m.value.histogram = histogram_new();
    if (m.value.histogram == NULL)
        return luaL_error(L, "Fail to create Histogram.");

    if ((argc > 0) && !lua_isnil(L, 1)) {
        if (!lua_isnumber(L, 1)) {
            metric_reset(&m, METRIC_TYPE_HISTOGRAM);
            return luaL_error(L, "Sum must be a number.");
        }
        m.value.histogram->sum = luaL_checknumber(L, 1);
    }

    if ((argc > 1) && !lua_isnil(L, 2)) {
        if (!luac_isarray(L, 2)) {
            metric_reset(&m, METRIC_TYPE_HISTOGRAM);
            return luaL_error(L, "Buckets must be an array.");
        }
        m.value.histogram = luac_to_histogram_buckets(L, 2, m.value.histogram);
    }

    if ((argc > 2) && !lua_isnil(L, 3)) {
        if (!lua_istable(L, 3)) {
            metric_reset(&m, METRIC_TYPE_HISTOGRAM);
            return luaL_error(L, "Labels must be a table.");
        }
        luac_to_labels(L, 3, &m.label);
    }

    if ((argc > 3) && !lua_isnil(L, 4)) {
        if (!lua_isnumber(L, 4)) {
            metric_reset(&m, METRIC_TYPE_HISTOGRAM);
            return luaL_error(L, "Time must be a timestamp.");
        }
        m.time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 4));
    }

    if ((argc > 4) && !lua_isnil(L, 5)) {
        if (!lua_isnumber(L, 5)) {
            metric_reset(&m, METRIC_TYPE_HISTOGRAM);
            return luaL_error(L, "Interval must be a timestamp.");
        }
        m.interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 5));
    }

    metric_t *new_m = lua_newuserdata(L, sizeof(metric_t));

    memcpy(new_m, &m, sizeof(metric_t));

    luaL_setmetatable(L, MT_METRIC_HISTOGRAM);

    return 1;
}

static int lmetric_histogram_delete(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_HISTOGRAM);
    if (m == NULL)
        return 0;

    metric_reset(m, METRIC_TYPE_HISTOGRAM);

    return 0;
}

static int lmetric_histogram_tostring_buffer(strbuf_t *buf, metric_t *m)
{
    int status = strbuf_putstr(buf, "MetricHistogram{ ");
    status |= strbuf_putstr(buf, "sum = ");
    status |= strbuf_putdouble(buf, m->value.histogram->sum);
    status |= strbuf_putstr(buf, "bucket = [");
    for (size_t i = 0; i < m->value.histogram->num; i++) {
        histogram_bucket_t *b = &m->value.histogram->buckets[i];
        if (i > 0)
            status |= strbuf_putchar(buf, ',');
        status |= strbuf_putstr(buf, " [");
        status |= strbuf_putuint(buf, b->counter);
        status |= strbuf_putstr(buf, ", ");
        status |= strbuf_putdouble(buf, b->maximum);
        status |= strbuf_putstr(buf, "]");
    }
    status |= strbuf_putstr(buf, "], time = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->time));
    status |= strbuf_putstr(buf, ", interval = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->interval));
    status |= strbuf_putstr(buf, ", labels = {");

    for (size_t i = 0; i < m->label.num; i++) {
        label_pair_t *pair = &m->label.ptr[i];
        if (i > 0)
            status |= strbuf_putchar(buf, ',');
        status |= strbuf_putstr(buf, " \"");
        status |= strbuf_putstr(buf, pair->name);
        status |= strbuf_putstr(buf, "\" = \"");
        status |= strbuf_putstr(buf, pair->value);
        status |= strbuf_putstr(buf, "\"");
    }

    return status | strbuf_putstr(buf, "} }");
}

static int lmetric_histogram_tostring(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_HISTOGRAM);
    if (m == NULL)
        return 0;

    strbuf_t buf = STRBUF_CREATE;

    lmetric_histogram_tostring_buffer(&buf, m);

    lua_pushstring(L, buf.ptr);

    strbuf_destroy(&buf);

    return 1;
}

static int lmetric_histogram_get(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_HISTOGRAM);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->time));
                return 1;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->interval));
                return 1;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                lua_createtable(L, 0, m->label.num);

                for (size_t i = 0; i < m->label.num; i++) {
                    label_pair_t *pair = &m->label.ptr[i];
                    lua_pushstring(L, pair->value);
                    lua_setfield(L, -2, pair->name);
                }

                return 1;
            }
            break;
        case 's':
            if (strcmp(key, "sum") == 0) {
                lua_pushnumber(L, (lua_Number)m->value.histogram->sum);
                return 1;
            }
            break;
        case 'q':
            if (strcmp(key, "quantiles") == 0) {
                lua_createtable(L, m->value.histogram->num, 0);
                for (size_t i = 0; i < m->value.histogram->num; i++) {
                    histogram_bucket_t *b = &m->value.histogram->buckets[i];

                    lua_createtable(L, 2, 0);
                    lua_pushnumber(L, (lua_Number)b->counter);
                    lua_rawseti (L, -2, 1);
                    lua_pushnumber(L, (lua_Number)b->maximum);
                    lua_rawseti (L, -2, 2);

                    lua_rawseti (L, -2, i+1);
                }
                return 1;
            }
            break;
        }
    }

    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {
        lua_getuservalue(L, 1);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
    }

    return 1;
}

static int lmetric_histogram_set(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_HISTOGRAM);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                m->time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                m->interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                if (!lua_istable(L, 3))
                    return luaL_error(L, "Labels must be a table.");
                label_set_reset(&m->label);
                luac_to_labels(L, 3, &m->label);
                return 0;
            }
            break;
        case 's':
            if (strcmp(key, "sum") == 0) {
                m->value.histogram->sum = luaL_checknumber(L, 3);
                return 0;
            }
            break;
        case 'q':
            if (strcmp(key, "bucket") == 0) {
                m->value.histogram = luac_to_histogram_buckets(L, 3, m->value.histogram);
                return 0;
            }
            break;
        }
    }

    lua_getuservalue(L, 1);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);

    return 0;
}

static luaL_Reg metric_histogram_reg[] = {
    { "new",        lmetric_histogram_new      },
    { "__gc",       lmetric_histogram_delete   },
    { "__tostring", lmetric_histogram_tostring },
    { "__newindex", lmetric_histogram_set      },
    { "__index",    lmetric_histogram_get      },
    { NULL,         NULL                       }
};

int register_metric_histogram(lua_State *L)
{
    luaL_newmetatable(L, MT_METRIC_HISTOGRAM);
    luaL_setfuncs(L, metric_histogram_reg, 0);

    return 1;
}

static int lmetric_gauge_histogram_new(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc < 3)
        return luaL_error(L, "The sum and buckets are required arguments.");

    metric_t m = {0};

    m.time = 0;
    m.interval = 0;
    m.value.histogram = histogram_new();
    if (m.value.histogram == NULL)
        return luaL_error(L, "Fail to create Histogram.");

    if ((argc > 0) && !lua_isnil(L, 1)) {
        if (!lua_isnumber(L, 1)) {
            metric_reset(&m, METRIC_TYPE_GAUGE_HISTOGRAM);
            return luaL_error(L, "Sum must be a number.");
        }
        m.value.histogram->sum = luaL_checknumber(L, 1);
    }

    if ((argc > 1) && !lua_isnil(L, 2)) {
        if (!luac_isarray(L, 2)) {
            metric_reset(&m, METRIC_TYPE_GAUGE_HISTOGRAM);
            return luaL_error(L, "Buckets must be an array.");
        }
        m.value.histogram = luac_to_histogram_buckets(L, 2, m.value.histogram);
    }

    if ((argc > 2) && !lua_isnil(L, 3)) {
        if (!lua_istable(L, 3)) {
            metric_reset(&m, METRIC_TYPE_GAUGE_HISTOGRAM);
            return luaL_error(L, "Labels must be a table.");
        }
        luac_to_labels(L, 3, &m.label);
    }

    if ((argc > 3) && !lua_isnil(L, 4)) {
        if (!lua_isnumber(L, 4)) {
            metric_reset(&m, METRIC_TYPE_GAUGE_HISTOGRAM);
            return luaL_error(L, "Time must be a timestamp.");
        }
        m.time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 4));
    }

    if ((argc > 4) && !lua_isnil(L, 5)) {
        if (!lua_isnumber(L, 5)) {
            metric_reset(&m, METRIC_TYPE_GAUGE_HISTOGRAM);
            return luaL_error(L, "Interval must be a timestamp.");
        }
        m.interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 5));
    }

    metric_t *new_m = lua_newuserdata(L, sizeof(metric_t));

    memcpy(new_m, &m, sizeof(metric_t));

    luaL_setmetatable(L, MT_METRIC_GAUGE_HISTOGRAM);

    return 1;
}

static int lmetric_gauge_histogram_delete(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_GAUGE_HISTOGRAM);
    if (m == NULL)
        return 0;

    metric_reset(m, METRIC_TYPE_GAUGE_HISTOGRAM);

    return 0;
}

static int lmetric_gauge_histogram_tostring_buffer(strbuf_t *buf, metric_t *m)
{
    int status = strbuf_putstr(buf, "MetricGaugeHistogram{ ");
    status |= strbuf_putstr(buf, "sum = ");
    status |= strbuf_putdouble(buf, m->value.histogram->sum);
    status |= strbuf_putstr(buf, "bucket = [");
    for (size_t i = 0; i < m->value.histogram->num; i++) {
        histogram_bucket_t *b = &m->value.histogram->buckets[i];
        if (i > 0)
            status |= strbuf_putchar(buf, ',');
        status |= strbuf_putstr(buf, " [");
        status |= strbuf_putuint(buf, b->counter);
        status |= strbuf_putstr(buf, ", ");
        status |= strbuf_putdouble(buf, b->maximum);
        status |= strbuf_putstr(buf, "]");
    }
    status |= strbuf_putstr(buf, "], time = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->time));
    status |= strbuf_putstr(buf, ", interval = ");
    status |= strbuf_putdouble(buf, CDTIME_T_TO_DOUBLE(m->interval));
    status |= strbuf_putstr(buf, ", labels = {");

    for (size_t i = 0; i < m->label.num; i++) {
        label_pair_t *pair = &m->label.ptr[i];
        if (i > 0)
            status |= strbuf_putchar(buf, ',');
        status |= strbuf_putstr(buf, " \"");
        status |= strbuf_putstr(buf, pair->name);
        status |= strbuf_putstr(buf, "\" = \"");
        status |= strbuf_putstr(buf, pair->value);
        status |= strbuf_putstr(buf, "\"");
    }

    return status | strbuf_putstr(buf, "} }");
}

static int lmetric_gauge_histogram_tostring(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_GAUGE_HISTOGRAM);
    if (m == NULL)
        return 0;

    strbuf_t buf = STRBUF_CREATE;

    lmetric_gauge_histogram_tostring_buffer(&buf, m);

    lua_pushstring(L, buf.ptr);

    strbuf_destroy(&buf);

    return 1;
}

static int lmetric_gauge_histogram_get(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_GAUGE_HISTOGRAM);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->time));
                return 1;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(m->interval));
                return 1;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                lua_createtable(L, 0, m->label.num);

                for (size_t i = 0; i < m->label.num; i++) {
                    label_pair_t *pair = &m->label.ptr[i];
                    lua_pushstring(L, pair->value);
                    lua_setfield(L, -2, pair->name);
                }

                return 1;
            }
            break;
        case 's':
            if (strcmp(key, "sum") == 0) {
                lua_pushnumber(L, (lua_Number)m->value.histogram->sum);
                return 1;
            }
            break;
        case 'q':
            if (strcmp(key, "quantiles") == 0) {
                lua_createtable(L, m->value.histogram->num, 0);
                for (size_t i = 0; i < m->value.histogram->num; i++) {
                    histogram_bucket_t *b = &m->value.histogram->buckets[i];

                    lua_createtable(L, 2, 0);
                    lua_pushnumber(L, (lua_Number)b->counter);
                    lua_rawseti (L, -2, 1);
                    lua_pushnumber(L, (lua_Number)b->maximum);
                    lua_rawseti (L, -2, 2);

                    lua_rawseti (L, -2, i+1);
                }
                return 1;
            }
            break;
        }
    }

    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {
        lua_getuservalue(L, 1);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
    }

    return 1;
}

static int lmetric_gauge_histogram_set(lua_State *L)
{
    metric_t *m = luaL_checkudata(L, 1, MT_METRIC_GAUGE_HISTOGRAM);
    if (m == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 't':
            if (strcmp(key, "time") == 0) {
                m->time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'i':
            if (strcmp(key, "interval") == 0) {
                m->interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                if (!lua_istable(L, 3))
                    return luaL_error(L, "Labels must be a table.");
                label_set_reset(&m->label);
                luac_to_labels(L, 3, &m->label);
                return 0;
            }
            break;
        case 's':
            if (strcmp(key, "sum") == 0) {
                m->value.histogram->sum = luaL_checknumber(L, 3);
                return 0;
            }
            break;
        case 'q':
            if (strcmp(key, "bucket") == 0) {
                m->value.histogram = luac_to_histogram_buckets(L, 3, m->value.histogram);
                return 0;
            }
            break;
        }
    }

    lua_getuservalue(L, 1);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);

    return 0;
}

static luaL_Reg metric_gauge_histogram_reg[] = {
    { "new",        lmetric_gauge_histogram_new      },
    { "__gc",       lmetric_gauge_histogram_delete   },
    { "__tostring", lmetric_gauge_histogram_tostring },
    { "__newindex", lmetric_gauge_histogram_set      },
    { "__index",    lmetric_gauge_histogram_get      },
    { NULL,         NULL                             }
};

int register_metric_gauge_histogram(lua_State *L)
{
    luaL_newmetatable(L, MT_METRIC_GAUGE_HISTOGRAM);
    luaL_setfuncs(L, metric_gauge_histogram_reg, 0);

    return 1;
}

int lmetric_tostring_buffer(strbuf_t *buf, metric_type_t type, metric_t *m)
{
    switch(type) {
    case METRIC_TYPE_UNKNOWN:
        if (m->value.unknown.type == UNKNOWN_FLOAT64)
            return lmetric_unknown_double_tostring_buffer(buf, m);
        else
            return lmetric_unknown_integer_tostring_buffer(buf, m);
        break;
    case METRIC_TYPE_GAUGE:
        if (m->value.gauge.type == GAUGE_FLOAT64)
            return lmetric_gauge_double_tostring_buffer(buf, m);
        else
            return lmetric_gauge_integer_tostring_buffer(buf, m);
        break;
    case METRIC_TYPE_COUNTER:
        if (m->value.counter.type == COUNTER_FLOAT64)
            return lmetric_counter_double_tostring_buffer(buf, m);
        else
            return lmetric_counter_integer_tostring_buffer(buf, m);
        break;
    case METRIC_TYPE_STATE_SET:
        return lmetric_state_set_tostring_buffer(buf, m);
        break;
    case METRIC_TYPE_INFO:
        return lmetric_info_tostring_buffer(buf, m);
        break;
    case METRIC_TYPE_SUMMARY:
        return lmetric_summary_tostring_buffer(buf, m);
        break;
    case METRIC_TYPE_HISTOGRAM:
        return lmetric_histogram_tostring_buffer(buf, m);
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        return lmetric_gauge_histogram_tostring_buffer(buf, m);
        break;
    }

    return 0;
}

int luac_push_metric(lua_State *L, metric_type_t type, metric_t *m)
{
    metric_t *new_m = lua_newuserdata(L, sizeof(metric_t));
    if (new_m == NULL)
        return -1;

    memset(new_m, 0, sizeof(metric_t));

    new_m->time = m->time;
    new_m->interval = m->interval;

    label_set_clone(&new_m->label, m->label);

    metric_value_clone(&new_m->value, m->value, type);

    switch(type) {
    case METRIC_TYPE_UNKNOWN:
        if (m->value.unknown.type == UNKNOWN_FLOAT64)
            luaL_setmetatable(L, MT_METRIC_UNKNOWN_DOUBLE);
        else
            luaL_setmetatable(L, MT_METRIC_UNKNOWN_INTEGER);
        break;
    case METRIC_TYPE_GAUGE:
        if (m->value.gauge.type == GAUGE_FLOAT64)
            luaL_setmetatable(L, MT_METRIC_GAUGE_DOUBLE);
        else
            luaL_setmetatable(L, MT_METRIC_GAUGE_INTEGER);
        break;
    case METRIC_TYPE_COUNTER:
        if (m->value.counter.type == COUNTER_FLOAT64)
            luaL_setmetatable(L, MT_METRIC_COUNTER_DOUBLE);
        else
            luaL_setmetatable(L, MT_METRIC_COUNTER_INTEGER);
        break;
    case METRIC_TYPE_STATE_SET:
        luaL_setmetatable(L, MT_METRIC_STATE_SET);
        break;
    case METRIC_TYPE_INFO:
        luaL_setmetatable(L, MT_METRIC_INFO);
        break;
    case METRIC_TYPE_SUMMARY:
        luaL_setmetatable(L, MT_METRIC_SUMMARY);
        break;
    case METRIC_TYPE_HISTOGRAM:
        luaL_setmetatable(L, MT_METRIC_HISTOGRAM);
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        luaL_setmetatable(L, MT_METRIC_GAUGE_HISTOGRAM);
        break;
    }

    return 0;
}
