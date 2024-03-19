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

#include <pthread.h>

typedef enum {
    LUA_CB_INIT         = 0,
    LUA_CB_READ         = 1,
    LUA_CB_WRITE        = 2,
    LUA_CB_SHUTDOWN     = 3,
    LUA_CB_CONFIG       = 4,
    LUA_CB_NOTIFICATION = 5,
} lua_cb_type_t;

typedef struct lua_script_s {
    lua_State *lua_state;
    struct lua_script_s *next;
} lua_script_t;

typedef struct {
    lua_State *lua_state;
    char *lua_function_name;
    pthread_mutex_t lock;
    int callback_id;
} clua_callback_data_t;

static char base_path[PATH_MAX];
static lua_script_t *scripts;

static size_t lua_init_callbacks_num = 0;
static clua_callback_data_t **lua_init_callbacks = NULL;

static size_t lua_shutdown_callbacks_num = 0;
static clua_callback_data_t **lua_shutdown_callbacks = NULL;

static size_t lua_config_callbacks_num = 0;
static clua_callback_data_t **lua_config_callbacks = NULL;

static void clua_callback_data_free(clua_callback_data_t **cb, size_t num)
{
    for (size_t i = 0 ; i < num; i++) {
        if (cb[i] != NULL) {
            free(cb[i]->lua_function_name);
            pthread_mutex_destroy(&cb[i]->lock);
            free(cb[i]);
        }
    }
    free(cb);
}

static int clua_store_callback(lua_State *L, int idx)
{
    /* Copy the function pointer */
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

/* Store the threads in a global variable so they are not cleaned up by the
 * garbage collector. */
static int clua_store_thread(lua_State *L, int idx)
{
    if (!lua_isthread(L, idx))
        return -1;

    /* Copy the thread pointer */
    lua_pushvalue(L, idx);

    luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

static int clua_read(user_data_t *ud)
{
    clua_callback_data_t *cb = ud->data;

    pthread_mutex_lock(&cb->lock);

    lua_State *L = cb->lua_state;

    int status = clua_load_callback(L, cb->callback_id);
    if (status != 0) {
        PLUGIN_ERROR("Unable to load callback \"%s\" (id %i).",
                     cb->lua_function_name, cb->callback_id);
        pthread_mutex_unlock(&cb->lock);
        return -1;
    }
    /* +1 = 1 */

    status = lua_pcall(L, 0, 1, 0);
    if (status != 0) {
        const char *errmsg = lua_tostring(L, -1);
        if (errmsg == NULL)
            PLUGIN_ERROR("Calling a read callback failed. "
                         "In addition, retrieving the error message failed.");
        else
            PLUGIN_ERROR("Calling a read callback failed: %s", errmsg);
        lua_pop(L, 1);
        pthread_mutex_unlock(&cb->lock);
        return -1;
    }

    if (!lua_isnumber(L, -1)) {
        PLUGIN_ERROR("Read function \"%s\" (id %i) did not return a numeric status.",
                     cb->lua_function_name, cb->callback_id);
        status = -1;
    } else {
        status = (int)lua_tointeger(L, -1);
    }

    /* pop return value and function */
    lua_pop(L, 1); /* -1 = 0 */

    pthread_mutex_unlock(&cb->lock);
    return status;
}

static int clua_write(metric_family_t const *fam, user_data_t *ud)
{
    clua_callback_data_t *cb = ud->data;

    pthread_mutex_lock(&cb->lock);

    lua_State *L = cb->lua_state;

    int status = clua_load_callback(L, cb->callback_id);
    if (status != 0) {
        PLUGIN_ERROR("Unable to load callback \"%s\" (id %i).",
                     cb->lua_function_name, cb->callback_id);
        pthread_mutex_unlock(&cb->lock);
        return -1;
    }
    /* +1 = 1 */

    status = luac_push_metric_family(L, fam);
    if (status != 0) {
        lua_pop(L, 1); /* -1 = 0 */
        pthread_mutex_unlock(&cb->lock);
        PLUGIN_ERROR("luac_push_metric_family failed.");
        return -1;
    }
    /* +1 = 2 */

    status = lua_pcall(L, 1, 1, 0); /* -2+1 = 1 */
    if (status != 0) {
        const char *errmsg = lua_tostring(L, -1);
        if (errmsg == NULL)
            PLUGIN_ERROR("Calling the write callback failed. "
                         "In addition, retrieving the error message failed.");
        else
            PLUGIN_ERROR("Calling the write callback failed:\n%s", errmsg);
        lua_pop(L, 1); /* -1 = 0 */
        pthread_mutex_unlock(&cb->lock);
        return -1;
    }

    if (!lua_isnumber(L, -1)) {
        PLUGIN_ERROR("Write function \"%s\" (id %i) did not return a numeric value.",
                     cb->lua_function_name, cb->callback_id);
        status = -1;
    } else {
        status = (int)lua_tointeger(L, -1);
    }

    lua_pop(L, 1); /* -1 = 0 */
    pthread_mutex_unlock(&cb->lock);
    return status;
}

static int clua_notification(const notification_t *notify, user_data_t *ud)
{
    clua_callback_data_t *cb = ud->data;

    pthread_mutex_lock(&cb->lock);

    lua_State *L = cb->lua_state;

    int status = clua_load_callback(L, cb->callback_id);
    if (status != 0) {
        PLUGIN_ERROR("Unable to load callback '%s' (id %i).",
                     cb->lua_function_name, cb->callback_id);
        pthread_mutex_unlock(&cb->lock);
        return -1;
    }
    /* +1 = 1 */

    DEBUG("Lua plugin: Convert notification_t to table on stack");
    status = luac_push_notification(L, notify);
    if (status != 0) {
        lua_pop(L, 1); /* -1 = 0 */
        PLUGIN_ERROR("Lua plugin: luaC_pushNotification failed.");
        pthread_mutex_unlock(&cb->lock);
        return -1;
    }
    /* +1 = 2 */

    status = lua_pcall(L, 1, 1, 0); /* -2+1 = 1 */
    if (status != 0) {
        const char *errmsg = lua_tostring(L, -1);
        if (errmsg == NULL) {
            PLUGIN_ERROR("Calling the notification callback failed. "
                         "In addition, retrieving the error message failed.");
        } else {
            PLUGIN_ERROR("Calling the notification callback failed: %s.", errmsg);
        }
        lua_pop(L, 1); /* -1 = 0 */
        pthread_mutex_unlock(&cb->lock);
        return -1;
    }

    if (!lua_isnumber(L, -1)) {
        PLUGIN_ERROR("Notification function '%s' (id %i) did not return a numeric value.",
                      cb->lua_function_name, cb->callback_id);
        status = -1;
    } else {
        status = (int)lua_tointeger(L, -1);
    }

    lua_pop(L, 1); /* -1 = 0 */

    pthread_mutex_unlock(&cb->lock);

    return status;
}

static int lua_cb_dispatch_notification(lua_State *L)
{
    PLUGIN_DEBUG("lua_cb_dispatch_notification called");

    int nargs = lua_gettop(L);

    if (nargs != 1)
      return luaL_error(L, "Invalid number of arguments (%d != 1)", nargs);

    luaL_checktype(L, 1, LUA_TTABLE);

    PLUGIN_DEBUG("Check whether table is passed to dispatch_notification.");

    notification_t *notif = luac_to_notification(L, 0);
    if (notif == NULL)
        return luaL_error(L, "Failed to convert table into notification_t");

    plugin_dispatch_notification(notif);
    free(notif);

    PLUGIN_DEBUG("lua_cb_dispatch_notification successfully called.");

    return 0;
}

/* Exported functions */

static int lua_cb_log_debug(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    plugin_log(LOG_DEBUG, NULL, 0, NULL, "%s", msg);
    return 0;
}

static int lua_cb_log_error(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    plugin_log(LOG_ERR, NULL, 0, NULL, "%s", msg);
    return 0;
}

static int lua_cb_log_info(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    plugin_log(LOG_INFO, NULL, 0, NULL, "%s", msg);
    return 0;
}

static int lua_cb_log_notice(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    plugin_log(LOG_NOTICE, NULL, 0, NULL, "%s", msg);
    return 0;
}

static int lua_cb_log_warning(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    plugin_log(LOG_WARNING, NULL, 0, NULL, "%s", msg);
    return 0;
}

static int lua_cb_dispatch_metric_family(lua_State *L)
{
    int nargs = lua_gettop(L);

    if (nargs != 1)
        return luaL_error(L, "Invalid number of arguments (%d != 1)", nargs);

    luaL_checktype(L, 1, LUA_TTABLE);

    metric_family_t *fam = luac_to_metric_family(L, -1);
    if (fam == NULL)
        return luaL_error(L, "%s", "luac_to_metric_family failed");

    plugin_dispatch_metric_family(fam, 0);

    metric_family_free(fam);

    return 0;
}

static void lua_cb_free(void *data)
{
    clua_callback_data_t *cb = data;
    free(cb->lua_function_name);
    pthread_mutex_destroy(&cb->lock);
    free(cb);
}

static int lua_cb_register_plugin_callbacks(lua_State *L,
                                            clua_callback_data_t ***callbacks,
                                            size_t *callbacks_num,
                                            clua_callback_data_t *cb)
{
  //pthread_mutex_lock(&lua_lock);
  clua_callback_data_t **new_callbacks = NULL;
  new_callbacks = realloc(*callbacks, (*callbacks_num + 1) * sizeof(clua_callback_data_t *));
  if (new_callbacks == NULL) {
    //pthread_mutex_unlock(&lua_lock);
    return luaL_error(L, "Reallocate callback stack failed");
  }
  new_callbacks[*callbacks_num] = cb;
  *callbacks = new_callbacks;
  *callbacks_num = *callbacks_num + 1;
  // pthread_mutex_unlock(&lua_lock);

  return 0;
}

static int lua_cb_register_generic(lua_State *L, lua_cb_type_t type)
{
    int nargs = lua_gettop(L);

    if (nargs != 1)
        return luaL_error(L, "Invalid number of arguments (%d != 1)", nargs);

    char subname[DATA_MAX_NAME_LEN];
    if (!lua_isfunction(L, 1) && lua_isstring(L, 1)) {
        const char *fname = lua_tostring(L, 1);
        ssnprintf(subname, sizeof(subname), "%s()", fname);

        lua_getglobal(L, fname); // Push function into stack
        lua_remove(L, 1);        // Remove string from stack
        if (!lua_isfunction(L, -1))
            return luaL_error(L, "Unable to find function '%s'", fname);
    } else {
        lua_getfield(L, LUA_REGISTRYINDEX, "ncollectd:callback_num");
        int tmp = lua_tointeger(L, -1);
        ssnprintf(subname, sizeof(subname), "ncallback_%d", tmp);
        lua_pop(L, 1); // Remove old value from stack
        lua_pushinteger(L, tmp + 1);
        lua_setfield(L, LUA_REGISTRYINDEX, "ncollectd:callback_num"); // pops value
    }

    luaL_checktype(L, 1, LUA_TFUNCTION);

    lua_getfield(L, LUA_REGISTRYINDEX, "ncollectd:script_path");
    char function_name[DATA_MAX_NAME_LEN];
    ssnprintf(function_name, sizeof(function_name), "lua/%s/%s", lua_tostring(L, -1), subname);
    lua_pop(L, 1);

    int callback_id = clua_store_callback(L, 1);
    if (callback_id < 0)
        return luaL_error(L, "%s", "Storing callback function failed");

    lua_State *thread = lua_newthread(L);
    if (thread == NULL)
        return luaL_error(L, "%s", "lua_newthread failed");
    clua_store_thread(L, -1);
    lua_pop(L, 1);

    clua_callback_data_t *cb = calloc(1, sizeof(*cb));
    if (cb == NULL)
        return luaL_error(L, "%s", "calloc failed");

    cb->lua_state = thread;
    cb->callback_id = callback_id;
    cb->lua_function_name = strdup(function_name);
    pthread_mutex_init(&cb->lock, NULL);

    switch(type) {
    case LUA_CB_INIT: {
        int status = lua_cb_register_plugin_callbacks(L, &lua_init_callbacks,
                                                         &lua_init_callbacks_num, cb);
        if (status != 0)
            return luaL_error(L, "lua_cb_register_plugin_callbacks(init) failed");
        return 0;
    }   break;
    case LUA_CB_READ: {
        int status = plugin_register_complex_read("lua", function_name, clua_read, 0,
                                   &(user_data_t){ .data = cb, .free_func = lua_cb_free });

        if (status != 0)
            return luaL_error(L, "%s", "plugin_register_complex_read failed");
        return 0;
    }   break;
    case LUA_CB_WRITE: {
// FIXME
        int status = plugin_register_write("lua", function_name, clua_write, NULL, 0, 0,
                                           &(user_data_t){ .data = cb, .free_func = lua_cb_free });

        if (status != 0)
            return luaL_error(L, "%s", "plugin_register_write failed");
        return 0;
    }   break;
    case LUA_CB_SHUTDOWN: {
        int status = lua_cb_register_plugin_callbacks(L, &lua_shutdown_callbacks,
                                                         &lua_shutdown_callbacks_num, cb);
        if (status != 0)
            return luaL_error(L, "lua_cb_register_plugin_callbacks(shutdown) failed");
        return 0;
    }   break;
    case LUA_CB_CONFIG: {
        int status = lua_cb_register_plugin_callbacks(L, &lua_config_callbacks,
                                                         &lua_config_callbacks_num, cb);
        if (status != 0)
            return luaL_error(L, "lua_cb_register_plugin_callbacks(config) failed");
        return 0;
    }   break;
    case LUA_CB_NOTIFICATION: {
        int status = plugin_register_notification("lua", function_name, clua_notification,
                                            &(user_data_t){ .data = cb, .free_func = lua_cb_free });
        if (status != 0)
            return luaL_error(L, "plugin_register_notification failed");
        return 0;
    }   break;
    }

    return luaL_error(L, "%s", "lua_cb_register_generic unsupported type");
}

static int lua_cb_register_read(lua_State *L)
{
    return lua_cb_register_generic(L, LUA_CB_READ);
}

static int lua_cb_register_write(lua_State *L)
{
    return lua_cb_register_generic(L, LUA_CB_WRITE);
}

static int lua_cb_register_init(lua_State *L)
{
    return lua_cb_register_generic(L, LUA_CB_INIT);
}

static int lua_cb_register_shutdown(lua_State *L)
{
    return lua_cb_register_generic(L, LUA_CB_SHUTDOWN);
}

static int lua_cb_register_config(lua_State *L)
{
    return lua_cb_register_generic(L, LUA_CB_CONFIG);
}

static int lua_cb_register_notification(lua_State *L)
{
    return lua_cb_register_generic(L, LUA_CB_NOTIFICATION);
}

static const luaL_Reg ncollectdlib[] = {
    { "log_debug",             lua_cb_log_debug              },
    { "log_error",             lua_cb_log_error              },
    { "log_info",              lua_cb_log_info               },
    { "log_notice",            lua_cb_log_notice             },
    { "log_warning",           lua_cb_log_warning            },
    { "dispatch_metric_family",lua_cb_dispatch_metric_family },
    { "dispatch_notification", lua_cb_dispatch_notification  },
    { "register_read",         lua_cb_register_read          },
    { "register_init",         lua_cb_register_init          },
    { "register_write",        lua_cb_register_write         },
    { "register_config",       lua_cb_register_config        },
    { "register_shutdown",     lua_cb_register_shutdown      },
    { "register_notification", lua_cb_register_notification  },
    { NULL,                    NULL                          }
};

static int open_ncollectd(lua_State *L)
{
#if LUA_VERSION_NUM < 502
    luaL_register(L, "ncollectd", ncollectdlib);
#else
    luaL_newlib(L, ncollectdlib);
#endif

    lua_pushstring(L, "METRIC_TYPE_UNKNOWN");
    lua_pushnumber(L, METRIC_TYPE_UNKNOWN);
    lua_settable(L, -3);
    lua_pushstring(L, "METRIC_TYPE_GAUGE");
    lua_pushnumber(L, METRIC_TYPE_GAUGE);
    lua_settable(L, -3);
    lua_pushstring(L, "METRIC_TYPE_COUNTER");
    lua_pushnumber(L, METRIC_TYPE_COUNTER);
    lua_settable(L, -3);
    lua_pushstring(L, "METRIC_TYPE_STATE_SET");
    lua_pushnumber(L, METRIC_TYPE_STATE_SET);
    lua_settable(L, -3);
    lua_pushstring(L, "METRIC_TYPE_INFO");
    lua_pushnumber(L, METRIC_TYPE_INFO);
    lua_settable(L, -3);
    lua_pushstring(L, "METRIC_TYPE_SUMMARY");
    lua_pushnumber(L, METRIC_TYPE_SUMMARY);
    lua_settable(L, -3);
    lua_pushstring(L, "METRIC_TYPE_HISTOGRAM");
    lua_pushnumber(L, METRIC_TYPE_HISTOGRAM);
    lua_settable(L, -3);
    lua_pushstring(L, "METRIC_TYPE_GAUGE_HISTOGRAM");
    lua_pushnumber(L, METRIC_TYPE_GAUGE_HISTOGRAM);
    lua_settable(L, -3);
    lua_pushstring(L, "UNKNOWN_FLOAT64");
    lua_pushnumber(L, UNKNOWN_FLOAT64);
    lua_settable(L, -3);
    lua_pushstring(L, "UNKNOWN_INT64");
    lua_pushnumber(L, UNKNOWN_INT64);
    lua_settable(L, -3);
    lua_pushstring(L, "GAUGE_FLOAT64");
    lua_pushnumber(L, GAUGE_FLOAT64);
    lua_settable(L, -3);
    lua_pushstring(L, "GAUGE_INT64");
    lua_pushnumber(L, GAUGE_INT64);
    lua_settable(L, -3);
    lua_pushstring(L, "COUNTER_UINT64");
    lua_pushnumber(L, COUNTER_UINT64);
    lua_settable(L, -3);
    lua_pushstring(L, "COUNTER_FLOAT64");
    lua_pushnumber(L, COUNTER_FLOAT64);
    lua_settable(L, -3);
    lua_pushstring(L, "LOG_ERR");
    lua_pushnumber(L, LOG_ERR);
    lua_settable(L, -3);
    lua_pushstring(L, "LOG_WARNING");
    lua_pushnumber(L, LOG_WARNING);
    lua_settable(L, -3);
    lua_pushstring(L, "LOG_NOTICE");
    lua_pushnumber(L, LOG_NOTICE);
    lua_settable(L, -3);
    lua_pushstring(L, "LOG_INFO");
    lua_pushnumber(L, LOG_INFO);
    lua_settable(L, -3);
    lua_pushstring(L, "LOG_DEBUG");
    lua_pushnumber(L, LOG_DEBUG);
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

static void lua_script_free(lua_script_t *script)
{
    if (script == NULL)
        return;

    lua_script_t *next = script->next;

    if (script->lua_state != NULL) {
        lua_close(script->lua_state);
        script->lua_state = NULL;
    }

    free(script);

    lua_script_free(next);
}

static int lua_script_init(lua_script_t *script)
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

static int lua_script_load(const char *script_path)
{
    lua_script_t *script = malloc(sizeof(*script));
    if (script == NULL) {
        PLUGIN_ERROR("malloc failed.");
        return -1;
    }

    int status = lua_script_init(script);
    if (status != 0) {
        lua_script_free(script);
        return status;
    }

    status = luaL_loadfile(script->lua_state, script_path);
    if (status != 0) {
        PLUGIN_ERROR("luaL_loadfile failed: %s", lua_tostring(script->lua_state, -1));
        lua_pop(script->lua_state, 1);
        lua_script_free(script);
        return -1;
    }

    lua_pushstring(script->lua_state, script_path);
    lua_setfield(script->lua_state, LUA_REGISTRYINDEX, "ncollectd:script_path");
    lua_pushinteger(script->lua_state, 0);
    lua_setfield(script->lua_state, LUA_REGISTRYINDEX, "ncollectd:callback_num");

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
        lua_script_t *last = scripts;
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

static int lua_config_base_path(const config_item_t *ci)
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

static int lua_config_script(const config_item_t *ci)
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

    status = lua_script_load(abs_path);
    if (status != 0)
        return status;

    PLUGIN_INFO("File '%s' loaded successfully", abs_path);

    return 0;
}

/*
 * plugin lua {
 *   base-path "/"
 *   script "script1.lua"
 *   script "script2.lua"
 * }
 */
static int lua_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("base-path", child->key) == 0) {
            status = lua_config_base_path(child);
        } else if (strcasecmp("script", child->key) == 0) {
            status = lua_config_script(child);
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
static int lua_shutdown(void)
{
    lua_script_free(scripts);

    clua_callback_data_free(lua_init_callbacks, lua_init_callbacks_num);
    clua_callback_data_free(lua_shutdown_callbacks, lua_shutdown_callbacks_num);
    clua_callback_data_free(lua_config_callbacks, lua_config_callbacks_num);

    return 0;
}

void module_register(void)
{
    plugin_register_config("lua", lua_config);
    plugin_register_shutdown("lua", lua_shutdown);
}
