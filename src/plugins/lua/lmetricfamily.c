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

static metric_t *lmetric_family_check_metric(metric_family_t *fam, lua_State *L, int idx)
{
    static const struct {
        const char *mt;
        metric_type_t type;
    } lmetric_family_metric_types[] = {
        { MT_METRIC_UNKNOWN_DOUBLE,  METRIC_TYPE_UNKNOWN         },
        { MT_METRIC_UNKNOWN_INTEGER, METRIC_TYPE_UNKNOWN         },
        { MT_METRIC_GAUGE_DOUBLE,    METRIC_TYPE_GAUGE           },
        { MT_METRIC_GAUGE_INTEGER,   METRIC_TYPE_GAUGE           },
        { MT_METRIC_COUNTER_DOUBLE,  METRIC_TYPE_COUNTER         },
        { MT_METRIC_COUNTER_INTEGER, METRIC_TYPE_COUNTER         },
        { MT_METRIC_INFO,            METRIC_TYPE_INFO            },
        { MT_METRIC_STATE_SET,       METRIC_TYPE_STATE_SET       },
        { MT_METRIC_SUMMARY,         METRIC_TYPE_SUMMARY         },
        { MT_METRIC_HISTOGRAM,       METRIC_TYPE_HISTOGRAM       },
        { MT_METRIC_GAUGE_HISTOGRAM, METRIC_TYPE_GAUGE_HISTOGRAM },
    };

    for (size_t i = 0; i < STATIC_ARRAY_SIZE(lmetric_family_metric_types); i++) {
        metric_t *metric = luaL_testudata(L, idx, lmetric_family_metric_types[i].mt);
        if (metric == NULL)
            continue;

        if (lmetric_family_metric_types[i].type != fam->type)
            luaL_error(L, "Mismatch metric type, the metric family type is %s.",
                          metric_type_str(fam->type));

        return metric;
    }

    luaL_error(L, "Metrics must be a metric or an array of metrics.");
    return NULL;
}

static int lmetric_family_append_metrics(metric_family_t *fam, lua_State *L, int idx, bool reset)
{
    if (lua_isuserdata(L, idx)) {
        metric_t *metric = lmetric_family_check_metric(fam, L, idx);
        if (reset)
            metric_list_reset(&fam->metric, fam->type);
        if (metric_family_metric_append(fam, *metric) != 0)
            return luaL_error(L, "Metric family: unable to append the metric.");
        return 0;
    }

    if (!luac_isarray(L, idx))
        return luaL_error(L, "Metrics must be a metric or an array of metrics.");

    size_t size = lua_rawlen(L, idx);
    for (size_t i = 0; i < size; i++) {
        lua_rawgeti(L, idx, i + 1);
        lmetric_family_check_metric(fam, L, -1);
        lua_pop(L, 1);
    }

    if (reset)
        metric_list_reset(&fam->metric, fam->type);

    for (size_t i = 0; i < size; i++) {
        lua_rawgeti(L, idx, i + 1);
        metric_t *metric = lua_touserdata(L, -1);
        if (metric_family_metric_append(fam, *metric) != 0)
            return luaL_error(L, "Metric family: unable to append the metric.");
        lua_pop(L, 1);
    }

    return 0;
}

static int lmetric_family_new(lua_State *L)
{
    int argc = lua_gettop(L);

    if (argc < 2)
        return luaL_error(L, "The name and metric type are required arguments.");

    metric_family_t *fam = lua_newuserdata(L, sizeof(*fam));
    memset(fam, 0, sizeof(*fam));
    fam->type = METRIC_TYPE_UNKNOWN;
    luaL_setmetatable(L, MT_METRIC_FAMILY);

    if (!lua_isstring(L, 1))
        return luaL_error(L, "Metric Family name must be a string.");

    fam->name = strdup(lua_tostring(L, 1));
    if (fam->name == NULL)
        return luaL_error(L, "Metric Family: strdup failed.");

    if ((argc > 1) && !lua_isnil(L, 2)) {
        if (!lua_isnumber(L, 2))
            return luaL_error(L, "Metric Family type must be a number.");

        int type = luaL_checkinteger(L, 2);
        if ((type != METRIC_TYPE_UNKNOWN) && (type != METRIC_TYPE_GAUGE) &&
            (type != METRIC_TYPE_COUNTER) && (type != METRIC_TYPE_STATE_SET) &&
            (type != METRIC_TYPE_INFO) && (type != METRIC_TYPE_SUMMARY) &&
            (type != METRIC_TYPE_HISTOGRAM) && (type != METRIC_TYPE_GAUGE_HISTOGRAM))
            return luaL_error(L, "Invalid metric type.");

        fam->type = type;
    }

    if ((argc > 2) && !lua_isnil(L, 3)) {
        if (!lua_isstring(L, 3))
            return luaL_error(L, "Metric Family help must be a string.");

        fam->help = strdup(lua_tostring(L, 3));
        if (fam->help == NULL)
            return luaL_error(L, "Metric Family: strdup failed.");
    }

    if ((argc > 3) && !lua_isnil(L, 4)) {
        if (!lua_isstring(L, 4))
            return luaL_error(L, "Metric Family unit must be a string.");

        fam->unit = strdup(lua_tostring(L, 4));
        if (fam->unit == NULL)
            return luaL_error(L, "Metric Family: strdup failed.");
    }

    return 1;
}

static int lmetric_family_delete(lua_State *L)
{
    metric_family_t *fam = luaL_checkudata(L, 1, MT_METRIC_FAMILY);
    if (fam == NULL)
        return 0;

    free(fam->name);
    fam->name = NULL;
    free(fam->help);
    fam->help = NULL;
    free(fam->unit);
    fam->unit = NULL;
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
    strbuf_putstr(&buf, fam->name == NULL ? "" : fam->name);
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
    if ((argc > 1) && !lua_isnil(L, 2)) {
        if (lua_isnumber(L, 2))
            ts = luac_checkcdtime(L, 2);
    }

    plugin_dispatch_metric_family(fam, ts);

    return 0;
}

static int lmetric_family_append(lua_State *L)
{
    metric_family_t *fam = luaL_checkudata(L, 1, MT_METRIC_FAMILY);
    if (fam == NULL)
        return 0;

    return lmetric_family_append_metrics(fam, L, 2, false);
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
                    if (luac_push_metric(L, fam->type, &fam->metric.ptr[i]) != 0)
                        return luaL_error(L, "Metric family: unable to copy the metric.");
                    lua_rawseti (L, -2, i+1);
                }
                return 1;
            }
            break;
        }
    }

    return luac_index_metatable(L);
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
                if (!lua_isstring(L, 3))
                    return luaL_error(L, "Metric family name must be a string.");

                char *name_dup = strdup(lua_tostring(L, 3));
                if (name_dup == NULL)
                    return luaL_error(L, "Metric family strdup failed.");

                free(fam->name);
                fam->name = name_dup;
                return 0;
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
                        return luaL_error(L, "Metric family unit must be a string.");

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
            if (strcmp(key, "metrics") == 0)
                return lmetric_family_append_metrics(fam, L, 3, true);
            break;
        }
    }

    return luac_newindex_error(L, "MetricFamily");
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
    new_fam->type = fam->type;
    luaL_setmetatable(L, MT_METRIC_FAMILY);

    if (fam->name != NULL) {
        new_fam->name = strdup(fam->name);
        if (new_fam->name == NULL)
            return -1;
    }
    if (fam->help != NULL) {
        new_fam->help = strdup(fam->help);
        if (new_fam->help == NULL)
            return -1;
    }
    if (fam->unit != NULL) {
        new_fam->unit = strdup(fam->unit);
        if (new_fam->unit == NULL)
            return -1;
    }

    new_fam->type = fam->type;

    if (metric_list_clone(&new_fam->metric, fam->metric, fam->type) != 0)
        return -1;

    return 0;
}
