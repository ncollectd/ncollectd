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
#include "lmetricfamily.h"

static int lmetric_family_append_metric(metric_family_t *fam, lua_State *L, int idx)
{
    if (!lua_isuserdata(L, idx))
        return -1;

    metric_t *metric = NULL;

    if ((metric = luaL_testudata(L, idx, MT_METRIC_UNKNOWN_DOUBLE)) != NULL) {
        if (fam->type != METRIC_TYPE_UNKNOWN)
            return luaL_error(L, "Missmatch metric type, must be METRIC_UNKNOWN.");
        metric_family_metric_append(fam, *metric);
    } else if ((metric = luaL_testudata(L, idx, MT_METRIC_UNKNOWN_INTEGER)) != NULL) {
        if (fam->type != METRIC_TYPE_UNKNOWN)
            return luaL_error(L, "Missmatch metric type, must be METRIC_UNKNOWN.");
        metric_family_metric_append(fam, *metric);
    } else if ((metric = luaL_testudata(L, idx, MT_METRIC_GAUGE_DOUBLE)) != NULL) {
        if (fam->type != METRIC_TYPE_GAUGE)
            return luaL_error(L, "Missmatch metric type, must be METRIC_GAUGE.");
        metric_family_metric_append(fam, *metric);
    } else if ((metric = luaL_testudata(L, idx, MT_METRIC_GAUGE_INTEGER)) != NULL) {
        if (fam->type != METRIC_TYPE_GAUGE)
            return luaL_error(L, "Missmatch metric type, must be METRIC_GAUGE.");
        metric_family_metric_append(fam, *metric);
    } else if ((metric = luaL_testudata(L, idx, MT_METRIC_COUNTER_DOUBLE)) != NULL) {
        if (fam->type != METRIC_TYPE_COUNTER)
            return luaL_error(L, "Missmatch metric type, must be METRIC_COUNTER.");
        metric_family_metric_append(fam, *metric);
    } else if ((metric = luaL_testudata(L, idx, MT_METRIC_COUNTER_INTEGER)) != NULL) {
        if (fam->type != METRIC_TYPE_COUNTER)
            return luaL_error(L, "Missmatch metric type, must be METRIC_COUNTER.");
        metric_family_metric_append(fam, *metric);
    } else if ((metric = luaL_testudata(L, idx, MT_METRIC_INFO)) != NULL) {
        if (fam->type != METRIC_TYPE_INFO)
            return luaL_error(L, "Missmatch metric type, must be METRIC_INFO.");
        metric_family_metric_append(fam, *metric);
    } else if ((metric = luaL_testudata(L, idx, MT_METRIC_STATE_SET)) != NULL) {
        if (fam->type != METRIC_TYPE_STATE_SET)
            return luaL_error(L, "Missmatch metric type, must be METRIC_STATE_SET.");
        metric_family_metric_append(fam, *metric);
    } else if ((metric = luaL_testudata(L, idx, MT_METRIC_SUMMARY)) != NULL) {
        if (fam->type != METRIC_TYPE_SUMMARY)
            return luaL_error(L, "Missmatch metric type, must be METRIC_SUMMARY.");
        metric_family_metric_append(fam, *metric);
    } else if ((metric = luaL_testudata(L, idx, MT_METRIC_HISTOGRAM)) != NULL) {
        if (fam->type != METRIC_TYPE_HISTOGRAM)
            return luaL_error(L, "Missmatch metric type, must be METRIC_HISTOGRAM.");
        metric_family_metric_append(fam, *metric);
    } else if ((metric = luaL_testudata(L, idx, MT_METRIC_GAUGE_HISTOGRAM)) != NULL) {
        if (fam->type != METRIC_TYPE_GAUGE_HISTOGRAM)
            return luaL_error(L, "Missmatch metric type, must be METRIC_GAUGE_HISTOGRAM.");
        metric_family_metric_append(fam, *metric);
    }

    return 0;
}

static int lmetric_family_append_metric_array(metric_family_t *fam, lua_State *L, int idx)
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
        lmetric_family_append_metric(fam, L, -1);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    return 0;
}

static void lmetric_family_reset(metric_family_t *fam)
{
    free(fam->name);
    free(fam->help);
    free(fam->unit);
}

static int lmetric_family_new(lua_State *L)
{
    metric_family_t fam = {0};
    fam.type = METRIC_TYPE_UNKNOWN;

    int argc = lua_gettop(L);

    if (argc < 2)
        return luaL_error(L, "The name and metric type are required arguments.");

    if ((argc > 0) && !lua_isnil(L, 1)) {
        const char *name = luaL_checkstring(L, 1);
        if (name == NULL)
            return luaL_error(L, "Metric Family name must be a string.");

        char *name_dup = strdup(name);
        if (name_dup == NULL)
            return luaL_error(L, "Metric Family: strdup failed.");
        fam.name = name_dup;
    }

    if ((argc > 1) && !lua_isnil(L, 2)) {
        if (!lua_isnumber(L, 2)) {
            lmetric_family_reset(&fam);
            return luaL_error(L, "Metric Family type must be a number.");
        }

        int type = luaL_checkinteger(L, 2);
        if ((type != METRIC_TYPE_UNKNOWN) && (type != METRIC_TYPE_GAUGE) &&
            (type != METRIC_TYPE_COUNTER) && (type != METRIC_TYPE_STATE_SET) &&
            (type != METRIC_TYPE_INFO) && (type != METRIC_TYPE_SUMMARY) &&
            (type != METRIC_TYPE_HISTOGRAM) && (type != METRIC_TYPE_GAUGE_HISTOGRAM)) {
            lmetric_family_reset(&fam);
            return luaL_error(L, "Invalid metric type.");
        }

        fam.type = type;
    }

    if ((argc > 2) && !lua_isnil(L, 3)) {
        if (!lua_isstring(L, 3)) {
            lmetric_family_reset(&fam);
            return luaL_error(L, "Metric Family help must be a string.");
        }

        const char *help = luaL_checkstring(L, 3);
        if (help == NULL) {
            lmetric_family_reset(&fam);
            return luaL_error(L, "Metric Family help must be a string.");
        }

        char *help_dup = strdup(help);
        if (help_dup == NULL) {
            lmetric_family_reset(&fam);
            return luaL_error(L, "Metric Family: strdup failed.");
        }

        fam.help = help_dup;
    }

    if ((argc > 3) && !lua_isnil(L, 4)) {
        if (!lua_isstring(L, 4)) {
            lmetric_family_reset(&fam);
            return luaL_error(L, "Metric Family unit must be a string.");
        }

        const char *unit = luaL_checkstring(L, 4);
        if (unit == NULL) {
            lmetric_family_reset(&fam);
            return luaL_error(L, "Metric Family unit must be a string.");
        }

        char *unit_dup = strdup(unit);
        if (unit_dup == NULL) {
            lmetric_family_reset(&fam);
            return luaL_error(L, "Metric Family: strdup failed.");
        }
        fam.unit = unit_dup;
    }

    metric_family_t *new_fam = lua_newuserdata(L, sizeof(metric_family_t));

    memcpy(new_fam, &fam, sizeof(metric_family_t));

    luaL_setmetatable(L, MT_METRIC_FAMILY);

    return 1;
}

static int lmetric_family_delete(lua_State *L)
{
    metric_family_t *fam = luaL_checkudata(L, 1, MT_METRIC_FAMILY);
    if (fam == NULL)
        return 0;

    free(fam->name);
    free(fam->help);
    free(fam->unit);
    metric_list_reset(&fam->metric, fam->type);

    return 0;
}

static int lmetric_family_tostring(lua_State *L)
{
    metric_family_t *fam = luaL_checkudata(L, 1, MT_METRIC_FAMILY);
    if (fam == NULL)
        return 0;

    strbuf_t buf = STRBUF_CREATE;

    strbuf_putstr(&buf, "metric_family{ ");
    strbuf_putstr(&buf, "name = \"");
    strbuf_putstr(&buf, fam->name);
    strbuf_putstr(&buf, "\", type = ");
    switch (fam->type) {
    case METRIC_TYPE_UNKNOWN:
        strbuf_putstr(&buf, "METRIC_UNKNOWN");
        break;
    case METRIC_TYPE_GAUGE:
        strbuf_putstr(&buf, "METRIC_GAUGE");
        break;
    case METRIC_TYPE_COUNTER:
        strbuf_putstr(&buf, "METRIC_COUNTER");
        break;
    case METRIC_TYPE_STATE_SET:
        strbuf_putstr(&buf, "METRIC_STATE_SET");
        break;
    case METRIC_TYPE_INFO:
        strbuf_putstr(&buf, "METRIC_INFO");
        break;
    case METRIC_TYPE_SUMMARY:
        strbuf_putstr(&buf, "METRIC_SUMMARY");
        break;
    case METRIC_TYPE_HISTOGRAM:
        strbuf_putstr(&buf, "METRIC_HISTOGRAM");
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        strbuf_putstr(&buf, "METRIC_GAUGE_HISTOGRAM");
        break;
    }

    if (fam->help != NULL) {
        strbuf_putstr(&buf, ", help = \"");
        strbuf_putstr(&buf, fam->help);
        strbuf_putstr(&buf, "\"");
    }

    if (fam->unit != NULL) {
        strbuf_putstr(&buf, ", unit = \"");
        strbuf_putstr(&buf, fam->unit);
        strbuf_putstr(&buf, "\"");
    }

    strbuf_putstr(&buf, ", metrics = [");
    for (size_t i = 0; i < fam->metric.num; i++) {
        metric_t *m = &fam->metric.ptr[i];
        if (i != 0)
            strbuf_putstr(&buf, ", ");
        lmetric_tostring_buffer(&buf, fam->type, m);
    }

    strbuf_putstr(&buf, "] }");

    lua_pushstring(L, buf.ptr);

    strbuf_destroy(&buf);

    return 1;
}

static int lmetric_family_dispatch(lua_State *L)
{
    metric_family_t *fam = luaL_checkudata(L, 1, MT_METRIC_FAMILY);
    if (fam == NULL)
        return 0;

    cdtime_t ts = 0;

    int argc = lua_gettop(L);
    if ((argc > 1) && !lua_isnil(L, 1)) {
        if (lua_isnumber(L, 1))
            ts = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 1));
    }

    plugin_dispatch_metric_family(fam, ts);

    return 0;
}

static int lmetric_family_append(lua_State *L)
{
    metric_family_t *fam = luaL_checkudata(L, 1, MT_METRIC_FAMILY);
    if (fam == NULL)
        return 0;

    if (lua_isuserdata(L, 2)) {
        lmetric_family_append_metric(fam, L, 2);
    } else if (luac_isarray(L, 2)) {
        lmetric_family_append_metric_array(fam, L, 2);
    } else {
        return luaL_error(L, "Metrics must be a metric or an array of metrics.");
    }

    return 0;
}

static int lmetric_family_get(lua_State *L)
{
    metric_family_t *fam = luaL_checkudata(L, 1, MT_METRIC_FAMILY);
    if (fam == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 'n':
            if (strcmp(key, "name") == 0) {
                lua_pushstring(L, fam->name);
                return 1;
            }
            break;
        case 'h':
            if (strcmp(key, "help") == 0) {
                if (fam->help != NULL)
                    lua_pushstring(L, fam->help);
                else
                    lua_pushnil(L);
                return 1;
            }
            break;
        case 'u':
            if (strcmp(key, "unit") == 0) {
                if (fam->unit != NULL)
                    lua_pushstring(L, fam->unit);
                else
                    lua_pushnil(L);

                return 1;
            }
            break;
        case 't':
            if (strcmp(key, "type") == 0) {
                lua_pushinteger(L, fam->type);
                return 1;
            }
            break;
        case 'm':
            if (strcmp(key, "metrics") == 0) {
                lua_createtable(L, fam->metric.num, 0);
                for (size_t i = 0; i < fam->metric.num; i++) {
                    luac_push_metric(L, fam->type, &fam->metric.ptr[i]);
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

static int lmetric_family_set(lua_State *L)
{
    metric_family_t *fam = luaL_checkudata(L, 1, MT_METRIC_FAMILY);
    if (fam == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 'n':
            if (strcmp(key, "name") == 0) {
                if (!lua_isnil(L, 3)) {
                    const char *name = luaL_checkstring(L, 3);
                    if (name == NULL)
                        return luaL_error(L, "Metric family name must be a string.");

                    char *name_dup = strdup(name);
                    if (name_dup == NULL)
                        return luaL_error(L, "Metric family strdup failed.");

                    free(fam->name);
                    fam->name = name_dup;
                    return 0;
                }
            }
            break;
        case 'h':
            if (strcmp(key, "help") == 0) {
                if (lua_isnil(L, 3)) {
                    free(fam->help);
                    fam->help = NULL;
                } else {
                    const char *help = luaL_checkstring(L, 3);
                    if (help == NULL)
                        return luaL_error(L, "Metric family help must be a string.");

                    char *help_dup = strdup(help);
                    if (help_dup == NULL)
                        return luaL_error(L, "Metric family strdup failed.");

                    free(fam->help);
                    fam->help = help_dup;
                }
                return 0;
            }
            break;
        case 'u':
            if (strcmp(key, "unit") == 0) {
                if (lua_isnil(L, 3)) {
                    free(fam->unit);
                    fam->unit = NULL;
                } else {
                    const char *unit = luaL_checkstring(L, 3);
                    if (unit == NULL)
                        return luaL_error(L, "Metric family help must be a string.");

                    char *unit_dup = strdup(unit);
                    if (unit_dup == NULL)
                        return luaL_error(L, "Metric family strdup failed.");

                    free(fam->unit);
                    fam->unit = unit_dup;
                }
                return 0;
            }
            break;
        case 'm':
            if (strcmp(key, "metrics") == 0) {
                metric_list_reset(&fam->metric, fam->type);
                if (lua_isuserdata(L, 3)) {
                    lmetric_family_append_metric(fam, L, 3);
                    return 0;
                } else if (luac_isarray(L, 3)) {
                    lmetric_family_append_metric_array(fam, L, 3);
                    return 0;
                } else {
                    return luaL_error(L, "Metric family metrics must be a metric or "
                                         "an array of metrics.");
                }
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

static luaL_Reg metric_family_reg[] = {
    { "new",         lmetric_family_new      },
    { "__gc",        lmetric_family_delete   },
    { "__tostring",  lmetric_family_tostring },
    { "dispatch",    lmetric_family_dispatch },
    { "append",      lmetric_family_append   },
    { "add_metric",  lmetric_family_append   },
    { "__newindex",  lmetric_family_set      },
    { "__index",     lmetric_family_get      },
    { NULL,          NULL                    }
};

int register_metric_family(lua_State *L)
{
    luaL_newmetatable(L, MT_METRIC_FAMILY);
    luaL_setfuncs(L, metric_family_reg, 0);
    return 1;
}

int luac_push_metric_family(lua_State *L, metric_family_t const *fam)
{
    metric_family_t *new_fam = lua_newuserdata(L, sizeof(metric_family_t));
    if (new_fam == NULL)
        return -1;

    memset(new_fam, 0, sizeof(metric_family_t));

    if (fam->name != NULL)
        new_fam->name = strdup(fam->name);
    if (fam->help != NULL)
        new_fam->help = strdup(fam->help);
    if (fam->unit != NULL)
        new_fam->unit = strdup(fam->unit);

    new_fam->type = fam->type;

    metric_list_clone(&new_fam->metric, fam->metric, fam->type);

    luaL_setmetatable(L, MT_METRIC_FAMILY);

    return 0;
}
