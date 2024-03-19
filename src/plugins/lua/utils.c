// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2010 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
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

static int luac_push_labels(lua_State *L, const label_set_t *labels)
{
    lua_createtable(L, 0, labels->num);

    for (size_t i = 0; i < labels->num; i++) {
        lua_pushstring(L, labels->ptr[i].value);
        lua_setfield(L, -2, labels->ptr[i].name);
    }

    return 0;
}

static int luac_to_labels(lua_State *L, int idx, label_set_t *labels)
{
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if(!lua_isstring(L, -1 + idx)) {
            PLUGIN_WARNING("Label key for '%s' must be LUA_TSTRING, LUA_TNUMBER o "
                           "LUA_TBOOLEAN.", lua_tostring(L, -1 + idx));
            // FIXME -1
            lua_pop(L, 1);
            continue;
        }
        int type = lua_type(L, 0 + idx);
        switch (type) {
        case LUA_TSTRING:
            label_set_add(labels, true, lua_tostring(L, -1 + idx), lua_tostring(L, 0 + idx));
            break;
        case LUA_TNUMBER: {
            double value = lua_tonumber(L, 0 + idx);
            char buffer[DTOA_MAX];
            dtoa(value, buffer, sizeof(buffer));
            label_set_add(labels, true, lua_tostring(L, -1 + idx), buffer);
        }   break;
        case LUA_TBOOLEAN: {
            bool value = lua_toboolean(L, 0 + idx);
            label_set_add(labels, true, lua_tostring(L, -1 + idx), value ? "true" : "false");
        }   break;
        default:
            PLUGIN_WARNING("Label value for '%s' must be LUA_TSTRING, LUA_TNUMBER o "
                           "LUA_TBOOLEAN.", lua_tostring(L, 0 + idx));
            // FIXME
            break;
        }

        lua_pop(L, 1);
    }

    return 0;
}

static int luac_push_value_state_set(lua_State *L, value_t *value)
{
    lua_createtable(L, 0, value->state_set.num);

    for (size_t i = 0; i < value->state_set.num; i++) {
        lua_pushboolean(L, value->state_set.ptr[i].enabled);
        lua_setfield(L, -2, value->state_set.ptr[i].name);
    }

    return 0;
}

static int luac_push_metrics(lua_State *L, metric_family_t const *fam)
{
    lua_createtable(L, fam->metric.num, 0);

    for (size_t i = 0; i < fam->metric.num; i++) {
        metric_t *m = &fam->metric.ptr[i];

        int size = 3;

        switch (fam->type) {
        case METRIC_TYPE_UNKNOWN:
            size += 2;
            break;
        case METRIC_TYPE_GAUGE:
            size += 2;
            break;
        case METRIC_TYPE_COUNTER:
            size += 2;
            break;
        case METRIC_TYPE_STATE_SET:
            size += 1;
            break;
        case METRIC_TYPE_INFO:
            size += 1;
            break;
        case METRIC_TYPE_SUMMARY:
            size += 3;
            break;
        case METRIC_TYPE_HISTOGRAM:
            size += 3;
            break;
        case METRIC_TYPE_GAUGE_HISTOGRAM:
            size += 3;
            break;
        }

        lua_createtable(L, 0, size);

        luac_push_cdtime(L, m->time);
        lua_setfield(L, -2, "time");

        luac_push_cdtime(L, m->interval);
        lua_setfield(L, -2, "interval");

        luac_push_labels(L, &m->label);
        lua_setfield(L, -2, "labels");

        switch (fam->type) {
        case METRIC_TYPE_UNKNOWN:
            lua_pushinteger(L, (lua_Integer)m->value.unknown.type);
            lua_setfield(L, -2, "type");
            if (m->value.unknown.type == UNKNOWN_FLOAT64)
                lua_pushnumber(L, m->value.unknown.float64);
            else
                lua_pushinteger(L, (lua_Integer)m->value.unknown.int64);
            lua_setfield(L, -2, "value");
            break;
        case METRIC_TYPE_GAUGE:
            lua_pushinteger(L, (lua_Integer)m->value.gauge.type);
            lua_setfield(L, -2, "type");
            if (m->value.gauge.type == GAUGE_FLOAT64)
                lua_pushnumber(L, m->value.gauge.float64);
            else
                lua_pushinteger(L, (lua_Integer)m->value.gauge.int64);
            lua_setfield(L, -2, "value");
            break;
        case METRIC_TYPE_COUNTER:
            lua_pushinteger(L, (lua_Integer)m->value.counter.type);
            lua_setfield(L, -2, "type");
            if (m->value.counter.type == COUNTER_UINT64)
                lua_pushinteger(L, (lua_Integer)m->value.counter.uint64);
            else
                lua_pushnumber(L, m->value.gauge.float64);
            lua_setfield(L, -2, "value");
            break;
        case METRIC_TYPE_STATE_SET:
            luac_push_value_state_set(L, &m->value);
            lua_setfield(L, -2, "stateset");
            break;
        case METRIC_TYPE_INFO:
            luac_push_labels(L, &m->value.info);
            lua_setfield(L, -2, "info");
            break;
        case METRIC_TYPE_SUMMARY:
            lua_pushnil(L);
            lua_setfield(L, -2, "quantiles");
            lua_pushnil(L);
            lua_setfield(L, -2, "count");
            lua_pushnil(L);
            lua_setfield(L, -2, "sum");
            break;
        case METRIC_TYPE_HISTOGRAM:
            lua_pushnil(L);
            lua_setfield(L, -2, "buckets");
            lua_pushnil(L);
            lua_setfield(L, -2, "count");
            lua_pushnil(L);
            lua_setfield(L, -2, "sum");
            break;
        case METRIC_TYPE_GAUGE_HISTOGRAM:
            lua_pushnil(L);
            lua_setfield(L, -2, "buckets");
            lua_pushnil(L);
            lua_setfield(L, -2, "gcount");
            lua_pushnil(L);
            lua_setfield(L, -2, "gsum");
            break;
        }

        lua_rawseti (L, -2, i+1);
    }

    return 0;
}

int luac_push_metric_family(lua_State *L, metric_family_t const *fam)
{
    PLUGIN_DEBUG("luaC_pushmetricfamily called");

    lua_createtable(L, 0, 5);

    lua_pushstring(L, fam->name);
    lua_setfield(L, -2, "name");

    lua_pushstring(L, fam->help);
    lua_setfield(L, -2, "help");

    lua_pushstring(L, fam->unit);
    lua_setfield(L, -2, "unit");

    lua_pushinteger(L, (lua_Integer)fam->type);
    lua_setfield(L, -2, "type");

    luac_push_metrics(L, fam);
    lua_setfield(L, -2, "metrics");

    PLUGIN_DEBUG("luaC_pushmetricfamily successfully called.");
    return 0;
}

int luac_to_metric(lua_State *L, int idx, metric_t *m, metric_type_t type)
{
    (void)idx;

    lua_getfield(L, -1, "interval");
    if (lua_isnumber(L, -1))
        m->interval = lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "time");
    if (lua_isnumber(L, -1))
        m->time = luac_to_cdtime(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "labels");
    if (lua_istable(L, -1))
        luac_to_labels(L, -1, &m->label);
    lua_pop(L, 1);

    switch(type) {
    case METRIC_TYPE_UNKNOWN: {
        m->value.unknown.type = UNKNOWN_FLOAT64;
        m->value.unknown.float64 = 0;

        lua_getfield(L, -1, "type");
        if (lua_isnumber(L, -1))
            m->value.unknown.type = lua_tonumber(L, -1);
        lua_pop(L, 1);

        if ((m->value.unknown.type != UNKNOWN_FLOAT64) &&
            (m->value.unknown.type != UNKNOWN_INT64))
            m->value.unknown.type = UNKNOWN_FLOAT64;

        lua_getfield(L, -1, "value");
        if (lua_isnumber(L, -1)) {
            if (m->value.unknown.type == UNKNOWN_FLOAT64)
                m->value.unknown.float64 = lua_tonumber(L, -1);
            else
                m->value.unknown.int64 = lua_tonumber(L, -1); // FIXME
            lua_pop(L, 1);
        }
    }   break;
    case METRIC_TYPE_GAUGE: {
        m->value.gauge.type = GAUGE_FLOAT64;
        m->value.gauge.float64 = 0;

        lua_getfield(L, -1, "type");
        if (lua_isnumber(L, -1))
            m->value.gauge.type = lua_tonumber(L, -1);
        lua_pop(L, 1);

        if ((m->value.gauge.type != GAUGE_FLOAT64) &&
            (m->value.gauge.type != GAUGE_INT64))
            m->value.gauge.type = GAUGE_FLOAT64;

        lua_getfield(L, -1, "value");
        if (lua_isnumber(L, -1)) {
            if (m->value.gauge.type == GAUGE_FLOAT64)
                m->value.gauge.float64 = lua_tonumber(L, -1);
            else
                m->value.gauge.int64 = lua_tonumber(L, -1); // FIXME
            lua_pop(L, 1);
        }
    }   break;
    case METRIC_TYPE_COUNTER: {
        m->value.counter.type = COUNTER_UINT64;
        m->value.counter.uint64 = 0;

        lua_getfield(L, -1, "type");
        if (lua_isnumber(L, -1))
            m->value.counter.type = lua_tonumber(L, -1);
        lua_pop(L, 1);

        if ((m->value.counter.type != COUNTER_FLOAT64) &&
            (m->value.counter.type != COUNTER_UINT64))
            m->value.counter.type = COUNTER_UINT64;

        lua_getfield(L, -1, "value");
        if (lua_isnumber(L, -1)) {
            if (m->value.counter.type == COUNTER_UINT64)
                m->value.counter.uint64 = lua_tonumber(L, -1); // FIXME
            else
                m->value.counter.float64 = lua_tonumber(L, -1);
            lua_pop(L, 1);
        }
    }   break;
    case METRIC_TYPE_STATE_SET: {
    }   break;
    case METRIC_TYPE_INFO: {
    }   break;
    case METRIC_TYPE_SUMMARY: {
    }   break;
    case METRIC_TYPE_HISTOGRAM: {
    }   break;
    case METRIC_TYPE_GAUGE_HISTOGRAM: {
    }   break;
    }


    return 0;
}

int luac_to_metric_list(lua_State *L, int idx, metric_family_t *fam)
{
#if LUA_VERSION_NUM < 502
    int len = lua_objlen(L, -2 + idx);
#else
    int len = lua_rawlen(L, -2 + idx);
#endif

    if (len == 0)
        return -1;

    metric_t *tmp = calloc(len, sizeof(*tmp));
    if (tmp == NULL)
        return -1;

    fam->metric.ptr = tmp;
    fam->metric.num = len;

    for (int i = 0; i < len; i++) {
         lua_pushinteger(L, i + 1);
         lua_gettable(L, -2);
         if (lua_type(L, -1) == LUA_TNIL)
             break;
         luac_to_metric(L, -1, &fam->metric.ptr[i], fam->type);
         lua_pop(L, 1);
    }

    return 0;
}

metric_family_t *luac_to_metric_family(lua_State *L, int idx)
{
#ifdef NCOLLECTD_DEBUG
    int stack_top_before = lua_gettop(L);
#endif
    /* Convert relative indexes to absolute indexes, so it doesn't change when we
     * push / pop stuff. */
    if (idx < 1)
        idx += lua_gettop(L) + 1;

    /* Check that idx is in the valid range */
    if ((idx < 1) || (idx > lua_gettop(L))) {
        PLUGIN_DEBUG("idx(%d), top(%d)", idx, stack_top_before);
        return NULL;
    }

    metric_family_t *fam = calloc(1, sizeof(*fam));
    if (fam == NULL) {
        PLUGIN_ERROR("calloc failed");
        return NULL;
    }

    fam->type = METRIC_TYPE_UNKNOWN;

    lua_getfield(L, -1, "name");
    if (lua_isstring(L, -1)) {
        const char *str = lua_tostring(L, -1);
        if (str != NULL)
            fam->name = strdup(str);
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "help");
    if (lua_isstring(L, -1)) {
        const char *str = lua_tostring(L, -1);
        if (str != NULL)
            fam->help = strdup(str);
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "unit");
    if (lua_isstring(L, -1)) {
        const char *str = lua_tostring(L, -1);
        if (str != NULL)
            fam->unit = strdup(str);
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "type");
    if (lua_isnumber(L, -1))
        fam->type = lua_tonumber(L, -1);
    lua_pop(L, 1);

    switch(fam->type) {
    case METRIC_TYPE_UNKNOWN:
    case METRIC_TYPE_GAUGE:
    case METRIC_TYPE_COUNTER:
    case METRIC_TYPE_STATE_SET:
    case METRIC_TYPE_INFO:
    case METRIC_TYPE_SUMMARY:
    case METRIC_TYPE_HISTOGRAM:
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        break;
    default:
        fam->type = METRIC_TYPE_UNKNOWN;
        break;
    }

    lua_getfield(L, -1, "metrics");
    if (lua_istable(L, -1))
        luac_to_metric_list(L, idx, fam);
    lua_pop(L, 1);

#ifdef NCOLLECTD_DEBUG
    assert(stack_top_before == lua_gettop(L));
#endif

    return fam;
}

notification_t *luac_to_notification(lua_State *L, __attribute__((unused))  int idx)
{
    notification_t *notif = calloc(1, sizeof(*notif));
    if (notif == NULL)
        return NULL;

    lua_getfield(L, -1, "severity");
    if (lua_isnumber(L, -1))
        notif->severity = lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "time");
    if (lua_isnumber(L, -1))
        notif->time = luac_to_cdtime(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "name");
    if (lua_isstring(L, -1)) {
        const char *str = lua_tostring(L, -1);
        if (str != NULL)
            notif->name = strdup(str);
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "labels");
    if (lua_istable(L, -1))
        luac_to_labels(L, -1, &notif->label);
    lua_pop(L, 1);

    lua_getfield(L, -1, "annotations");
    if (lua_istable(L, -1))
        luac_to_labels(L, -1, &notif->annotation);
    lua_pop(L, 1);

    return notif;
}

int luac_push_notification(lua_State *L, const notification_t *notif)
{
    PLUGIN_DEBUG("luaC_pushNotification called.");

    lua_createtable(L, 0, 5);

    lua_pushstring(L, notif->name);
    lua_setfield(L, -2, "name");

    lua_pushinteger(L, notif->severity);
    lua_setfield(L, -2, "severity");

    luac_push_cdtime(L, notif->time);
    lua_setfield(L, -2, "time");

    luac_push_labels(L, &notif->label);
    lua_setfield(L, -2, "labels");

    luac_push_labels(L, &notif->annotation);
    lua_setfield(L, -2, "annotations");

    return 0;
}

static int luac_push_config_values(lua_State *L, const config_item_t *ci)
{
    lua_createtable(L, ci->values_num, 0);

    for (int i = 0; i < ci->values_num; i++) {
        config_value_t *value = ci->values + i;
        switch (value->type) {
        case CONFIG_TYPE_STRING:
        case CONFIG_TYPE_REGEX: // FIXME put /str/ ¿?
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
