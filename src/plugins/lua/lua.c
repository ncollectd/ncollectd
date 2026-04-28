// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2010 Julien Ammous
// SPDX-FileCopyrightText: Copyright (C) 2010 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2016 Ruben Kerkhof
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Julien Ammous
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Ruben Kerkhof <ruben at rubenkerkhof.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "utils.h"

/* Include the Lua API header files. */
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include "lmetric.h"
#include "lmetricfamily.h"
#include "lnotification.h"

#include <pthread.h>

typedef enum {
    LUA_CB_INIT,
    LUA_CB_READ,
    LUA_CB_WRITE,
    LUA_CB_SHUTDOWN,
    LUA_CB_CONFIG,
    LUA_CB_NOTIFICATION,
} clua_cb_type_t;

struct clua_script_s;
typedef struct clua_script_s clua_script_t;

struct clua_cb_data_s;
typedef struct clua_cb_data_s clua_cb_data_t;

struct clua_cb_data_s {
    lua_State *lua_state;
    int callback_id;
    int data_id;
    char *name;
    char *plugin_name;
    char *source;
    int line;
    clua_script_t *script;
    clua_cb_data_t *next;
};

struct clua_script_s {
    lua_State *lua_state;
    pthread_mutex_t lock;
    clua_cb_data_t *init_callbacks;
    clua_cb_data_t *shutdown_callbacks;
    clua_cb_data_t *config_callbacks;
    clua_script_t *next;
};

static char base_path[PATH_MAX];

static clua_script_t *scripts;

static config_item_t *config_block;

static clua_script_t *clua_get_context(lua_State *L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "ncollectd:context");
    clua_script_t *script = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return script;
}

static void clua_cb_data_free(clua_cb_data_t *cb)
{
    if (cb == NULL)
        return;

    free(cb->name);
    free(cb->plugin_name);
    free(cb->source);
    free(cb);
}

static void clua_cb_data_list_free(clua_cb_data_t *cb)
{
    while(cb != NULL) {
        clua_cb_data_t *next = cb->next;
        clua_cb_data_free(cb);
        cb = next;
    }
}

static int clua_store_callback(lua_State *L, int idx)
{
    lua_pushvalue(L, idx);
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

static int clua_load_callback(lua_State *L, int callback_ref)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, callback_ref);

    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return -1;
    }

    return 0;
}

static int clua_store_thread(lua_State *L, int idx)
{
    if (!lua_isthread(L, idx))
        return -1;

    lua_pushvalue(L, idx);
    luaL_ref(L, LUA_REGISTRYINDEX);

    return 0;
}

static int clua_cb_config(clua_cb_data_t *cb, const config_item_t *ci)
{
    pthread_mutex_lock(&cb->script->lock);

    lua_State *L = cb->lua_state;

    int status = clua_load_callback(L, cb->callback_id);
    if (status != 0) {
        PLUGIN_ERROR("Unable to load callback at '%s':%d.", cb->source, cb->line);
        pthread_mutex_unlock(&cb->script->lock);
        return -1;
    }

    status = luac_push_config_item(L, ci);
    if (status != 0) {
        lua_pop(L, 1);
        pthread_mutex_unlock(&cb->script->lock);
        PLUGIN_ERROR("luac_push_config failed.");
        return -1;
    }

    int argc = 1;

    if (cb->data_id >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, cb->data_id);
        argc++;
    }

    status = lua_pcall(L, argc, 1, 0);
    if (status != 0) {
        const char *errmsg = lua_tostring(L, -1);
        if (errmsg == NULL)
            PLUGIN_ERROR("Calling a read callback failed. "
                         "In addition, retrieving the error message failed.");
        else
            PLUGIN_ERROR("Calling a read callback failed: %s", errmsg);
        lua_pop(L, 1);
        pthread_mutex_unlock(&cb->script->lock);
        return -1;
    }

    if (!lua_isnumber(L, -1)) {
        PLUGIN_ERROR("Read function at '%s':%d did not return a numeric status.",
                     cb->source, cb->line);
        status = -1;
    } else {
        status = (int)lua_tointeger(L, -1);
    }

    lua_pop(L, 1);

    pthread_mutex_unlock(&cb->script->lock);
    return status;
}

static int clua_cb_call(clua_cb_data_t *cb)
{
    pthread_mutex_lock(&cb->script->lock);

    lua_State *L = cb->lua_state;

    int status = clua_load_callback(L, cb->callback_id);
    if (status != 0) {
        PLUGIN_ERROR("Unable to load callback at '%s':%d.", cb->source, cb->line);
        pthread_mutex_unlock(&cb->script->lock);
        return -1;
    }

    int argc = 0;

    if (cb->data_id >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, cb->data_id);
        argc++;
    }

    status = lua_pcall(L, argc, 1, 0);
    if (status != 0) {
        const char *errmsg = lua_tostring(L, -1);
        if (errmsg == NULL)
            PLUGIN_ERROR("Calling a read callback failed. "
                         "In addition, retrieving the error message failed.");
        else
            PLUGIN_ERROR("Calling a read callback failed: %s", errmsg);
        lua_pop(L, 1);
        pthread_mutex_unlock(&cb->script->lock);
        return -1;
    }

    if (!lua_isnumber(L, -1)) {
        PLUGIN_ERROR("Read function at '%s':%d did not return a numeric status.",
                     cb->source, cb->line);
        status = -1;
    } else {
        status = (int)lua_tointeger(L, -1);
    }

    lua_pop(L, 1);

    pthread_mutex_unlock(&cb->script->lock);
    return status;
}

static int clua_read(user_data_t *ud)
{
    clua_cb_data_t *cb = ud->data;

    pthread_mutex_lock(&cb->script->lock);

    lua_State *L = cb->lua_state;

    int status = clua_load_callback(L, cb->callback_id);
    if (status != 0) {
        PLUGIN_ERROR("Unable to load callback at '%s':%d.", cb->source, cb->line);
        pthread_mutex_unlock(&cb->script->lock);
        return -1;
    }

    int argc = 0;

    if (cb->data_id >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, cb->data_id);
        argc++;
    }

    status = lua_pcall(L, argc, 1, 0);
    if (status != 0) {
        const char *errmsg = lua_tostring(L, -1);
        if (errmsg == NULL)
            PLUGIN_ERROR("Calling a read callback failed. "
                         "In addition, retrieving the error message failed.");
        else
            PLUGIN_ERROR("Calling a read callback failed: %s", errmsg);
        lua_pop(L, 1);
        pthread_mutex_unlock(&cb->script->lock);
        return -1;
    }

    if (!lua_isnumber(L, -1)) {
        PLUGIN_ERROR("Read function at '%s':%d did not return a numeric status.",
                     cb->source, cb->line);
        status = -1;
    } else {
        status = (int)lua_tointeger(L, -1);
    }

    lua_pop(L, 1);

    pthread_mutex_unlock(&cb->script->lock);
    return status;
}

static int clua_write(metric_family_t const *fam, user_data_t *ud)
{
    clua_cb_data_t *cb = ud->data;

    pthread_mutex_lock(&cb->script->lock);

    lua_State *L = cb->lua_state;

    int status = clua_load_callback(L, cb->callback_id);
    if (status != 0) {
        PLUGIN_ERROR("Unable to load callback at '%s':%d.", cb->source, cb->line);
        pthread_mutex_unlock(&cb->script->lock);
        return -1;
    }

    status = luac_push_metric_family(L, fam);
    if (status != 0) {
        lua_pop(L, 1);
        pthread_mutex_unlock(&cb->script->lock);
        PLUGIN_ERROR("luac_push_metric_family failed.");
        return -1;
    }

    int argc = 1;

    if (cb->data_id >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, cb->data_id);
        argc++;
    }

    status = lua_pcall(L, argc, 1, 0);
    if (status != 0) {
        const char *errmsg = lua_tostring(L, -1);
        if (errmsg == NULL)
            PLUGIN_ERROR("Calling the write callback failed. "
                         "In addition, retrieving the error message failed.");
        else
            PLUGIN_ERROR("Calling the write callback failed:\n%s", errmsg);
        lua_pop(L, 1);
        pthread_mutex_unlock(&cb->script->lock);
        return -1;
    }

    if (!lua_isnumber(L, -1)) {
        PLUGIN_ERROR("Write function at '%s':%d did not return a numeric value.",
                     cb->source, cb->line);
        status = -1;
    } else {
        status = (int)lua_tointeger(L, -1);
    }

    lua_pop(L, 1);

    pthread_mutex_unlock(&cb->script->lock);
    return status;
}

static int clua_notification(const notification_t *notify, user_data_t *ud)
{
    clua_cb_data_t *cb = ud->data;

    pthread_mutex_lock(&cb->script->lock);

    lua_State *L = cb->lua_state;

    int status = clua_load_callback(L, cb->callback_id);
    if (status != 0) {
        PLUGIN_ERROR("Unable to load callback at '%s':%d.",
                     cb->source, cb->line);
        pthread_mutex_unlock(&cb->script->lock);
        return -1;
    }

    status = luac_push_notification(L, notify);
    if (status != 0) {
        lua_pop(L, 1);
        PLUGIN_ERROR("Lua plugin: luaC_pushNotification failed.");
        pthread_mutex_unlock(&cb->script->lock);
        return -1;
    }

    int argc = 1;

    if (cb->data_id >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, cb->data_id);
        argc++;
    }

    status = lua_pcall(L, argc, 1, 0); /* -2+1 = 1 */
    if (status != 0) {
        const char *errmsg = lua_tostring(L, -1);
        if (errmsg == NULL) {
            PLUGIN_ERROR("Calling the notification callback failed. "
                         "In addition, retrieving the error message failed.");
        } else {
            PLUGIN_ERROR("Calling the notification callback failed: %s.", errmsg);
        }
        lua_pop(L, 1); /* -1 = 0 */
        pthread_mutex_unlock(&cb->script->lock);
        return -1;
    }

    if (!lua_isnumber(L, -1)) {
        PLUGIN_ERROR("Notification function at '%s':%d did not return a numeric value.",
                      cb->source, cb->line);
        status = -1;
    } else {
        status = (int)lua_tointeger(L, -1);
    }

    lua_pop(L, 1);

    pthread_mutex_unlock(&cb->script->lock);

    return status;
}

static int clua_cb_log(lua_State *L, int level)
{
    const char *msg = luaL_checkstring(L, 1);

    lua_Debug ar;
    lua_getstack(L, 1, &ar);
    lua_getinfo(L, "nSl", &ar);

    plugin_log(level, ar.short_src, ar.currentline, ar.name, "%s", msg);

    return 0;
}

static int clua_cb_log_debug(lua_State *L)
{
    return clua_cb_log(L, LOG_DEBUG);
}

static int clua_cb_log_error(lua_State *L)
{
    return clua_cb_log(L, LOG_ERR);
}

static int clua_cb_log_info(lua_State *L)
{
    return clua_cb_log(L, LOG_INFO);
}

static int clua_cb_log_notice(lua_State *L)
{
    return clua_cb_log(L, LOG_NOTICE);
}

static int clua_cb_log_warning(lua_State *L)
{
    return clua_cb_log(L, LOG_WARNING);
}

static void clua_cb_free(void *data)
{
    clua_cb_data_free(data);
}

static int clua_cb_register_generic(lua_State *L, clua_cb_type_t type)
{
    clua_script_t *script = clua_get_context(L);
    if (script == NULL)
        return luaL_error(L, "Missing script context.");

    int data_id = -1;
    cdtime_t interval = 0;
    const char *lname = NULL;
    int callback_id = -1;

    int nargs = lua_gettop(L);
    if (nargs < 1)
        return luaL_error(L, "A callback function is required.");

    if (!lua_isfunction(L, 1) && lua_isstring(L, 1)) {
        const char *fname = lua_tostring(L, 1);
        lua_getglobal(L, fname); // Push function into stack
        if (!lua_isfunction(L, -1))
            return luaL_error(L, "Unable to find function '%s'", fname);
        callback_id = clua_store_callback(L, -1);
        lua_pop(L, 1);
    } else {
        luaL_checktype(L, 1, LUA_TFUNCTION);
        callback_id = clua_store_callback(L, 1);
    }

    if (callback_id < 0)
        return luaL_error(L, "%s", "Storing callback function failed");

    lua_Debug ar;
    lua_pushvalue(L, 1);
    lua_getinfo(L, ">S", &ar);

    if (type == LUA_CB_READ) {
        if ((nargs > 1) && !lua_isnil(L, 2))
            interval = DOUBLE_TO_CDTIME_T(luaL_checknumber(L, 2));
        if ((nargs > 2) && !lua_isnil(L, 3)) {
            lua_pushvalue(L, 3);
            data_id = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        if ((nargs > 3) && !lua_isnil(L, 4))
            lname = luaL_checkstring(L, 4);
    } else {
        if ((nargs > 1) && !lua_isnil(L, 2)) {
            lua_pushvalue(L, 2);
            data_id = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        if ((nargs > 2) && !lua_isnil(L, 3))
            lname = luaL_checkstring(L, 3);
    }

    lua_State *thread = lua_newthread(L);
    if (thread == NULL)
        return luaL_error(L, "%s", "lua_newthread failed");
    clua_store_thread(L, -1);
    lua_pop(L, 1);

    clua_cb_data_t *cb = calloc(1, sizeof(*cb));
    if (cb == NULL)
        return luaL_error(L, "%s", "calloc failed");

    char full_plugin_name[PATH_MAX];
    lua_getfield(L, LUA_REGISTRYINDEX, "ncollectd:script_path");
    const char *script_path = lua_tostring(L, -1);
    if (lname != NULL)
        ssnprintf(full_plugin_name, sizeof(full_plugin_name), "lua/%s/%s", script_path, lname);
    else
        ssnprintf(full_plugin_name, sizeof(full_plugin_name), "lua/%s/%p", script_path, (void *)cb);
    char *plugin_name = full_plugin_name + strlen("lua/");
    lua_pop(L, 1);

    cb->lua_state = thread;
    cb->callback_id = callback_id;
    cb->script = script;
    if (lname != NULL)
        cb->name = strdup(lname);
    cb->data_id = data_id;
    cb->source = strdup(ar.short_src);
    cb->line = ar.linedefined;

    switch(type) {
    case LUA_CB_INIT:
        cb->next = cb->script->init_callbacks;
        cb->script->init_callbacks = cb;
        lua_pushstring(L, full_plugin_name);
        return 1;
        break;
    case LUA_CB_READ: {
        int status = plugin_register_complex_read("lua", plugin_name, clua_read, interval,
                                   &(user_data_t){ .data = cb, .free_func = clua_cb_free });
        if (status != 0)
            return luaL_error(L, "%s", "plugin_register_complex_read failed");
        lua_pushstring(L, full_plugin_name);
        return 1;
    }   break;
    case LUA_CB_WRITE: {
        int status = plugin_register_write("lua", plugin_name, clua_write, NULL, 0, 0,
                                           &(user_data_t){ .data = cb, .free_func = clua_cb_free });
        if (status != 0)
            return luaL_error(L, "%s", "plugin_register_write failed");
        lua_pushstring(L, full_plugin_name);
        return 1;
    }   break;
    case LUA_CB_SHUTDOWN:
        cb->next = cb->script->shutdown_callbacks;
        cb->script->shutdown_callbacks = cb;
        lua_pushstring(L, full_plugin_name);
        return 1;
        break;
    case LUA_CB_CONFIG:
        cb->next = cb->script->config_callbacks;
        cb->script->config_callbacks = cb;
        lua_pushstring(L, full_plugin_name);
        return 1;
        break;
    case LUA_CB_NOTIFICATION: {
        int status = plugin_register_notification("lua", plugin_name, clua_notification,
                                            &(user_data_t){ .data = cb, .free_func = clua_cb_free });
        if (status != 0)
            return luaL_error(L, "plugin_register_notification failed");

        lua_pushstring(L, full_plugin_name);
        return 1;
    }   break;
    }

    return luaL_error(L, "lua_cb_register_generic unsupported type");
}

static int clua_cb_register_read(lua_State *L)
{
    return clua_cb_register_generic(L, LUA_CB_READ);
}

static int clua_cb_register_write(lua_State *L)
{
    return clua_cb_register_generic(L, LUA_CB_WRITE);
}

static int clua_cb_register_init(lua_State *L)
{
    return clua_cb_register_generic(L, LUA_CB_INIT);
}

static int clua_cb_register_shutdown(lua_State *L)
{
    return clua_cb_register_generic(L, LUA_CB_SHUTDOWN);
}

static int clua_cb_register_config(lua_State *L)
{
    return clua_cb_register_generic(L, LUA_CB_CONFIG);
}

static int clua_cb_register_notification(lua_State *L)
{
    return clua_cb_register_generic(L, LUA_CB_NOTIFICATION);
}

static int clua_cb_unregister_generic(lua_State *L, clua_cb_data_t **head)
{
    int nargs = lua_gettop(L);
    if (nargs < 1)
        return luaL_error(L, "A callback name is required.");

    const char *name = luaL_checkstring(L, 1);

    clua_cb_data_t *cb = *head;
    clua_cb_data_t *prev = NULL;
    while (cb != NULL) {
        if (strcmp(cb->plugin_name, name) == 0)
            break;
        prev = cb;
        cb = cb->next;
    }

    if (cb == NULL)
        return luaL_error(L, "Callback not found.");


    if (prev == NULL)
        *head = cb->next;
    else
        prev->next = cb->next;

    clua_cb_data_free(0);

    return 0;
}

static int clua_cb_unregister_read(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    plugin_unregister_read(name);
    return 0;
}

static int clua_cb_unregister_init(lua_State *L)
{
    clua_script_t *script = clua_get_context(L);
    if (script == NULL)
        return luaL_error(L, "Missing script context.");
    return clua_cb_unregister_generic(L, &script->init_callbacks);
}

static int clua_cb_unregister_write(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    plugin_unregister_write(name);
    return 0;
}

static int clua_cb_unregister_config(lua_State *L)
{
    clua_script_t *script = clua_get_context(L);
    if (script == NULL)
        return luaL_error(L, "Missing script context.");

    return clua_cb_unregister_generic(L, &script->shutdown_callbacks);
}

static int clua_cb_unregister_shutdown(lua_State *L)
{
    clua_script_t *script = clua_get_context(L);
    if (script == NULL)
        return luaL_error(L, "Missing script context.");

    return clua_cb_unregister_generic(L, &script->config_callbacks);
}

static int clua_cb_unregister_notification(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    plugin_unregister_notification(name);
    return 0;
}

static const luaL_Reg ncollectdlib[] = {
    { "debug",                   clua_cb_log_debug               },
    { "error",                   clua_cb_log_error               },
    { "info",                    clua_cb_log_info                },
    { "notice",                  clua_cb_log_notice              },
    { "warning",                 clua_cb_log_warning             },
    { "register_read",           clua_cb_register_read           },
    { "register_init",           clua_cb_register_init           },
    { "register_write",          clua_cb_register_write          },
    { "register_config",         clua_cb_register_config         },
    { "register_shutdown",       clua_cb_register_shutdown       },
    { "register_notification",   clua_cb_register_notification   },
    { "unregister_read",         clua_cb_unregister_read         },
    { "unregister_init",         clua_cb_unregister_init         },
    { "unregister_write",        clua_cb_unregister_write        },
    { "unregister_config",       clua_cb_unregister_config       },
    { "unregister_shutdown",     clua_cb_unregister_shutdown     },
    { "unregister_notification", clua_cb_unregister_notification },
    { NULL,                      NULL                            }
};

static int open_ncollectd(lua_State *L)
{
#if LUA_VERSION_NUM < 502
    luaL_register(L, "ncollectd", ncollectdlib);
#else
    luaL_newlib(L, ncollectdlib);
#endif

    register_metric_unknown_double(L);
    lua_setfield(L, -2, "MetricUnknownDouble");

    register_metric_unknown_interger(L);
    lua_setfield(L, -2, "MetricUnknownInteger");

    register_metric_gauge_double(L);
    lua_setfield(L, -2, "MetricGaugeDouble");

    register_metric_gauge_interger(L);
    lua_setfield(L, -2, "MetricGaugeInteger");

    register_metric_counter_double(L);
    lua_setfield(L, -2, "MetricCounterDouble");

    register_metric_counter_interger(L);
    lua_setfield(L, -2, "MetricCounterInteger");

    register_metric_info(L);
    lua_setfield(L, -2, "MetricInfo");

    register_metric_state_set(L);
    lua_setfield(L, -2, "MetricStateSet");

    register_metric_summary(L);
    lua_setfield(L, -2, "MetricSummary");

    register_metric_histogram(L);
    lua_setfield(L, -2, "MetricHistogram");

    register_metric_gauge_histogram(L);
    lua_setfield(L, -2, "MetricGaugeHistogram");

    register_metric_family(L);
    lua_setfield(L, -2, "MetricFamily");

    register_notification(L);
    lua_setfield(L, -2, "Notification");

    lua_pushnumber(L, LOG_ERR);
    lua_setfield(L, -2, "LOG_ERR");

    lua_pushnumber(L, LOG_WARNING);
    lua_setfield(L, -2, "LOG_WARNING");

    lua_pushnumber(L, LOG_NOTICE);
    lua_setfield(L, -2, "LOG_NOTICE");

    lua_pushnumber(L, LOG_INFO);
    lua_setfield(L, -2, "LOG_INFO");

    lua_pushnumber(L, LOG_DEBUG);
    lua_setfield(L, -2, "LOG_DEBUG");

    lua_pushstring(L, "METRIC_UNKNOWN");
    lua_pushnumber(L, METRIC_TYPE_UNKNOWN);
    lua_settable(L, -3);

    lua_pushstring(L, "METRIC_GAUGE");
    lua_pushnumber(L, METRIC_TYPE_GAUGE);
    lua_settable(L, -3);

    lua_pushstring(L, "METRIC_COUNTER");
    lua_pushnumber(L, METRIC_TYPE_COUNTER);
    lua_settable(L, -3);

    lua_pushstring(L, "METRIC_STATE_SET");
    lua_pushnumber(L, METRIC_TYPE_STATE_SET);
    lua_settable(L, -3);

    lua_pushstring(L, "METRIC_INFO");
    lua_pushnumber(L, METRIC_TYPE_INFO);
    lua_settable(L, -3);

    lua_pushstring(L, "METRIC_SUMMARY");
    lua_pushnumber(L, METRIC_TYPE_SUMMARY);
    lua_settable(L, -3);

    lua_pushstring(L, "METRIC_HISTOGRAM");
    lua_pushnumber(L, METRIC_TYPE_HISTOGRAM);
    lua_settable(L, -3);

    lua_pushstring(L, "METRIC_GAUGE_HISTOGRAM");
    lua_pushnumber(L, METRIC_TYPE_GAUGE_HISTOGRAM);
    lua_settable(L, -3);

    lua_pushstring(L, "NOTIF_FAILURE");
    lua_pushnumber(L, NOTIF_FAILURE);
    lua_settable(L, -3);

    lua_pushstring(L, "NOTIF_WARNING");
    lua_pushnumber(L, NOTIF_WARNING);
    lua_settable(L, -3);

    lua_pushstring(L, "NOTIF_OKAY");
    lua_pushnumber(L, NOTIF_OKAY);
    lua_settable(L, -3);

    return 1;
}

static void clua_script_free(clua_script_t *script)
{
    if (script == NULL)
        return;

    if (script->lua_state != NULL) {
        clua_cb_data_list_free(script->init_callbacks);
        clua_cb_data_list_free(script->shutdown_callbacks);
        clua_cb_data_list_free(script->config_callbacks);

        lua_close(script->lua_state);
        script->lua_state = NULL;
    }

    free(script);
}

static int clua_script_init(clua_script_t *script)
{
    memset(script, 0, sizeof(*script));

    /* initialize the lua context */
    script->lua_state = luaL_newstate();
    if (script->lua_state == NULL) {
        PLUGIN_ERROR("luaL_newstate() failed.");
        return -1;
    }

    /* Open up all the standard Lua libraries. */
    luaL_openlibs(script->lua_state);

    /* Load the 'ncollectd' library */
#if LUA_VERSION_NUM < 502
    lua_pushcfunction(script->lua_state, open_ncollectd);
    lua_pushstring(script->lua_state, "ncollectd");
    lua_call(script->lua_state, 1, 0);
#else
    luaL_requiref(script->lua_state, "ncollectd", open_ncollectd, 1);
    lua_pop(script->lua_state, 1);
#endif

    /* Prepend BasePath to package.path */
    if (base_path[0] != '\0') {
        lua_getglobal(script->lua_state, "package");
        lua_getfield(script->lua_state, -1, "path");

        const char *cur_path = lua_tostring(script->lua_state, -1);
        char *new_path = ssnprintf_alloc("%s/?.lua;%s", base_path, cur_path);

        lua_pop(script->lua_state, 1);
        lua_pushstring(script->lua_state, new_path);

        free(new_path);

        lua_setfield(script->lua_state, -2, "path");
        lua_pop(script->lua_state, 1);
    }

    return 0;
}

static int clua_script_load(const char *script_path)
{
    clua_script_t *script = calloc(1, sizeof(*script));
    if (script == NULL) {
        PLUGIN_ERROR("malloc failed.");
        return -1;
    }

    pthread_mutex_init(&script->lock, NULL);

    int status = clua_script_init(script);
    if (status != 0) {
        clua_script_free(script);
        return status;
    }

    status = luaL_loadfile(script->lua_state, script_path);
    if (status != 0) {
        PLUGIN_ERROR("luaL_loadfile failed: %s", lua_tostring(script->lua_state, -1));
        lua_pop(script->lua_state, 1);
        clua_script_free(script);
        return -1;
    }

    lua_pushstring(script->lua_state, script_path);
    lua_setfield(script->lua_state, LUA_REGISTRYINDEX, "ncollectd:script_path");
    lua_pushlightuserdata(script->lua_state, script);
    lua_setfield(script->lua_state, LUA_REGISTRYINDEX, "ncollectd:context");

    status = lua_pcall(script->lua_state, /* nargs    = */ 0, LUA_MULTRET, /* errfunc  = */ 0);
    if (status != 0) {
        const char *errmsg = lua_tostring(script->lua_state, -1);
        if (errmsg == NULL)
            PLUGIN_ERROR("lua_pcall failed with status %i. "
                         "In addition, no error message could be retrieved from the stack.",
                         status);
        else
            PLUGIN_ERROR("Executing script '%s' failed: %s", script_path, errmsg);
    }

    /* Append this script to the global list of scripts. */
    if (scripts) {
        clua_script_t *last = scripts;
        while (last->next)
            last = last->next;

        last->next = script;
    } else {
        scripts = script;
    }

    if (status != 0)
        return -1;

    return 0;
}

static int clua_config_base_path(const config_item_t *ci)
{
    int status = cf_util_get_string_buffer(ci, base_path, sizeof(base_path));
    if (status != 0)
        return status;

    size_t len = strlen(base_path);
    while ((len > 0) && (base_path[len - 1] == '/')) {
        len--;
        base_path[len] = '\0';
    }

    PLUGIN_DEBUG("base_path = '%s';", base_path);

    return 0;
}

static int clua_config_load_plugin(const config_item_t *ci)
{
    char rel_path[PATH_MAX];

    int status = cf_util_get_string_buffer(ci, rel_path, sizeof(rel_path));
    if (status != 0)
        return status;

    char abs_path[PATH_MAX];

    if (base_path[0] == '\0')
        sstrncpy(abs_path, rel_path, sizeof(abs_path));
    else
        ssnprintf(abs_path, sizeof(abs_path), "%s/%s", base_path, rel_path);

    PLUGIN_DEBUG("abs_path = '%s';", abs_path);

    status = clua_script_load(abs_path);
    if (status != 0)
        return status;

    PLUGIN_INFO("File '%s' loaded successfully", abs_path);

    return 0;
}

static int clua_config_plugin(const config_item_t *ci)
{
    if (ci->children_num == 0)
        return 0;

    config_item_t *ci_copy = config_clone(ci);
    if (ci_copy == NULL) {
        PLUGIN_ERROR("config_clone failed.");
        return -1;
    }

    if (config_block == NULL) {
        config_block = ci_copy;
        return 0;
    }

    config_item_t *tmp = realloc(config_block->children,
                  (config_block->children_num + ci_copy->children_num) * sizeof(*tmp));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        config_free(ci_copy);
        return -1;
    }
    config_block->children = tmp;

    /* Copy the pointers */
    memcpy(config_block->children + config_block->children_num, ci_copy->children,
           ci_copy->children_num * sizeof(*ci_copy->children));
    config_block->children_num += ci_copy->children_num;

    /* Delete the pointers from the copy, so `config_free' can't free them. */
    memset(ci_copy->children, 0,
           ci_copy->children_num * sizeof(*ci_copy->children));
    ci_copy->children_num = 0;

    config_free(ci_copy);

    return 0;
}

static int clua_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("base-path", child->key) == 0) {
            status = clua_config_base_path(child);
        } else if (strcasecmp("load-plugin", child->key) == 0) {
            status = clua_config_load_plugin(child);
        } else if (strcasecmp("plugin", child->key) == 0) {
            status = clua_config_plugin(child);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int clua_init(void)
{
    clua_script_t *script = scripts;

    while (script != NULL) {
        clua_script_t *next = script->next;

        clua_cb_data_t *cb = script->config_callbacks;
        while (cb != NULL) {
            for (int i = 0; i < config_block->children_num; i++) {
                config_item_t *child = config_block->children + i;
                if (cb->name != NULL) {
                    if (strcmp(cb->name, child->key) == 0) {
                        clua_cb_config(cb, child);
                        break;
                    }
                }
            }
            cb = cb->next;
        }

        cb = script->init_callbacks;
        while (cb != NULL) {
            clua_cb_call(cb);
            cb = cb->next;
        }

        script = next;
    }

    return 0;
}

static int clua_shutdown(void)
{
    clua_script_t *script = scripts;

    while (script != NULL) {
        clua_script_t *next = script->next;
        clua_cb_data_t *cb = script->shutdown_callbacks;
        while (cb != NULL) {
            clua_cb_call(cb);
            cb = cb->next;
        }
        clua_script_free(script);
        script = next;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("lua", clua_config);
    plugin_register_init("lua", clua_init);
    plugin_register_shutdown("lua", clua_shutdown);
}
