// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"

#include <lua.h>
#include <lauxlib.h>

#include "utils.h"
#include "lnotification.h"

static int lnotification_new(lua_State *L)
{
    notification_t n = {
        .severity = NOTIF_FAILURE,
        .time = cdtime()
    };

    int argc = lua_gettop(L);

    if (argc < 1)
        return luaL_error(L, "The name is a required argument.");

    if ((argc > 0) && !lua_isnil(L, 1)) {
        const char *name = luaL_checkstring(L, 1);
        if (name == NULL)
            return luaL_error(L, "Notification name must be a string.");

        char *name_dup = strdup(name);
        if (name_dup == NULL)
            return luaL_error(L, "Notification: strdup failed.");
        n.name = name_dup;
    }

    if ((argc > 1) && !lua_isnil(L, 2)) {
        if (!lua_isnumber(L, 2)) {
            notification_reset(&n);
            return luaL_error(L, "Notification severity must be a number.");
        }

        int severity = luaL_checkinteger(L, 2);
        if ((severity != NOTIF_FAILURE) && (severity != NOTIF_WARNING) &&
            (severity != NOTIF_OKAY)) {
            notification_reset(&n);
            return luaL_error(L, "Notification severity must be FAILURE(%d), "
                                 "WARNING(%d) or OKAY(%d).",
                                 NOTIF_FAILURE, NOTIF_WARNING, NOTIF_OKAY);
        }
        n.severity = severity;
    }

    if ((argc > 2) && !lua_isnil(L, 3)) {
        if (!lua_isnumber(L, 3)) {
            notification_reset(&n);
            return luaL_error(L, "Notification time must be a timestamp.");
        }
        n.time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
    }

    if ((argc > 3) && !lua_isnil(L, 4)) {
        if (!lua_istable(L, 4)) {
            notification_reset(&n);
            return luaL_error(L, "Notification labels must be a table.");
        }
        luac_to_labels(L, 4, &n.label);
    }

    if ((argc > 4) && !lua_isnil(L, 5)) {
        if (!lua_istable(L, 5)) {
            notification_reset(&n);
            return luaL_error(L, "Notification annotations must be a table.");
        }
        luac_to_labels(L, 5, &n.annotation);
    }

    notification_t *new_n = lua_newuserdata(L, sizeof(notification_t));

    memcpy(new_n, &n, sizeof(notification_t));

    luaL_setmetatable(L, MT_NOTIFICATION);

    return 1;
}

static int lnotification_delete(lua_State *L)
{
    notification_t *n = luaL_checkudata(L, 1, MT_NOTIFICATION);
    if (n == NULL)
        return 0;

    notification_reset(n);

    return 0;
}

static int lnotification_add_label(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc != 3)
        return luaL_error(L, "Notification.add_label requires two string arguments.");

    notification_t *n = luaL_checkudata(L, 1, MT_NOTIFICATION);
    if (n == NULL)
        return 0;

    if (lua_isnil(L, 1) || !lua_isstring(L, 3))
        return luaL_error(L, "Notification.add_label requires two string arguments.");

    if (lua_isnil(L, 2) || !lua_isstring(L, 3))
        return luaL_error(L, "Notification.add_label requires two string arguments.");

    const char *name = luaL_checkstring(L, 2);
    const char *value = luaL_checkstring(L, 3);
    if ((name == NULL) || (value == NULL))
        return luaL_error(L, "Notification.add_label requires two string arguments.");

    label_set_add(&n->label, true, name, value);

    return 0;
}

static int lnotification_add_annotation(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc != 3)
        return luaL_error(L, "Notification.add_annotation requires two string arguments.");

    notification_t *n = luaL_checkudata(L, 1, MT_NOTIFICATION);
    if (n == NULL)
        return 0;

    if (lua_isnil(L, 1) || !lua_isstring(L, 2))
        return luaL_error(L, "Notification.add_annotation requires two string arguments.");

    if (lua_isnil(L, 2) || !lua_isstring(L, 3))
        return luaL_error(L, "Notification.add_annotation requires two string arguments.");

    const char *name = luaL_checkstring(L, 2);
    const char *value = luaL_checkstring(L, 3);
    if ((name == NULL) || (value == NULL))
        return luaL_error(L, "Notification.add_annotation requires two string arguments.");

    label_set_add(&n->annotation, true, name, value);

    return 0;
}

static int lnotification_dispatch(lua_State *L)
{
    notification_t *n = luaL_checkudata(L, 1, MT_NOTIFICATION);
    if (n == NULL)
        return 0;

    plugin_dispatch_notification(n);

    return 0;
}

static int lnotification_tostring(lua_State *L)
{
    notification_t *n = luaL_checkudata(L, 1, MT_NOTIFICATION);
    if (n == NULL)
        return 0;

    strbuf_t buf = STRBUF_CREATE;

    strbuf_putstr(&buf, "notification{ ");
    strbuf_putstr(&buf, "name = \"");
    strbuf_putstr(&buf, n->name);
    strbuf_putstr(&buf, "\", severity = ");
    switch (n->severity) {
    case NOTIF_FAILURE:
        strbuf_putstr(&buf, "NOTIF_FAILURE");
        break;
    case NOTIF_WARNING:
        strbuf_putstr(&buf, "NOTIF_WARNING");
        break;
    case NOTIF_OKAY:
        strbuf_putstr(&buf, "NOTIF_OKAY");
        break;
    }
    strbuf_putstr(&buf, ", time = ");
    strbuf_putdouble(&buf, CDTIME_T_TO_DOUBLE(n->time));
    strbuf_putstr(&buf, ", labels = {");
    for (size_t i = 0; i < n->label.num; i++) {
        label_pair_t *pair = &n->label.ptr[i];
        if (i > 0)
            strbuf_putchar(&buf, ',');
        strbuf_putstr(&buf, " \"");
        strbuf_putstr(&buf, pair->name);
        strbuf_putstr(&buf, "\" = \"");
        strbuf_putstr(&buf, pair->value);
        strbuf_putstr(&buf, "\"");
    }
    strbuf_putstr(&buf, "}, annotations = {");
    for (size_t i = 0; i < n->annotation.num; i++) {
        label_pair_t *pair = &n->annotation.ptr[i];
        if (i > 0)
            strbuf_putchar(&buf, ',');
        strbuf_putstr(&buf, " \"");
        strbuf_putstr(&buf, pair->name);
        strbuf_putstr(&buf, "\" = \"");
        strbuf_putstr(&buf, pair->value);
        strbuf_putstr(&buf, "\"");
    }
    strbuf_putstr(&buf, "} }");

    lua_pushstring(L, buf.ptr);

    strbuf_destroy(&buf);

    return 1;
}

static int lnotification_get(lua_State *L)
{
    notification_t *n = luaL_checkudata(L, 1, MT_NOTIFICATION);
    if (n == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 's':
            if (strcmp(key, "severity") == 0) {
                lua_pushinteger(L, n->severity);
                return 1;
            }
            break;
        case 't':
            if (strcmp(key, "time") == 0) {
                lua_pushnumber(L, (lua_Number)CDTIME_T_TO_DOUBLE(n->time));
                return 1;
            }
            break;
        case 'n':
            if (strcmp(key, "name") == 0) {
                lua_pushstring(L, n->name);
                return 1;
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                lua_createtable(L, 0, n->label.num);
                for (size_t i = 0; i < n->label.num; i++) {
                    label_pair_t *pair = &n->label.ptr[i];
                    lua_pushstring(L, pair->value);
                    lua_setfield(L, -2, pair->name);
                }
                return 1;
            }
            break;
        case 'a':
            if (strcmp(key, "annotations") == 0) {
                lua_createtable(L, 0, n->annotation.num);
                for (size_t i = 0; i < n->annotation.num; i++) {
                    label_pair_t *pair = &n->annotation.ptr[i];
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

static int lnotification_set(lua_State *L)
{
    notification_t *n = luaL_checkudata(L, 1, MT_NOTIFICATION);
    if (n == NULL)
        return 0;

    if (lua_isstring(L, 2)) {
        const char *key = lua_tostring(L, 2);
        switch(key[0]) {
        case 's':
            if (strcmp(key, "severity") == 0) {
                int severity = (int)luaL_checknumber(L, 3);
                if ((severity == NOTIF_FAILURE) ||
                    (severity == NOTIF_WARNING) ||
                    (severity == NOTIF_OKAY)) {
                    n->severity = severity;
                    return 0;
                }
            }
            break;
        case 't':
            if (strcmp(key, "time") == 0) {
                n->time = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 3));
                return 0;
            }
            break;
        case 'n':
            if (strcmp(key, "name") == 0) {
                if (!lua_isnil(L, 3)) {
                    const char *name = luaL_checkstring(L, 3);
                    if (name == NULL)
                        return luaL_error(L, "Notification: name must be a string.");

                    char *name_dup = strdup(name);
                    if (name_dup == NULL)
                        return luaL_error(L, "Notification: strdup failed.");

                    free(n->name);
                    n->name = name_dup;
                    return 0;
                }
            }
            break;
        case 'l':
            if (strcmp(key, "labels") == 0) {
                if (!lua_istable(L, 3))
                    return luaL_error(L, "Notification: labels must be a string.");
                label_set_reset(&n->label);
                luac_to_labels(L, 3, &n->label);
                return 0;
            }
            break;
        case 'a':
            if (strcmp(key, "annotations") == 0) {
                if (!lua_istable(L, 3))
                    return luaL_error(L, "Notification: annotations must be a string.");
                label_set_reset(&n->annotation);
                luac_to_labels(L, 3, &n->annotation);
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

static luaL_Reg notification_reg[] = {
    { "new",             lnotification_new            },
    { "__gc",            lnotification_delete         },
    { "add_label",       lnotification_add_label      },
    { "add_annotation",  lnotification_add_annotation },
    { "dispatch",        lnotification_dispatch       },
    { "__tostring",      lnotification_tostring       },
    { "__newindex",      lnotification_set            },
    { "__index",         lnotification_get            },
    { NULL,              NULL                         }
};

int register_notification(lua_State *L)
{
    luaL_newmetatable(L, MT_NOTIFICATION);
    luaL_setfuncs(L, notification_reg, 0);
    return 1;
}

int luac_push_notification(lua_State *L, const notification_t *n)
{
    notification_t *new_n = lua_newuserdata(L, sizeof(notification_t));
    if (new_n == NULL)
        return -1;

    memset(new_n, 0, sizeof(notification_t));

    new_n->time = n->time;
    new_n->severity = n->severity;
    if (new_n->name != NULL)
        new_n->name = strdup(n->name);
    label_set_clone(&new_n->annotation, new_n->annotation);
    label_set_clone(&new_n->label, new_n->label);

    luaL_setmetatable(L, MT_NOTIFICATION);

    return 0;
}
