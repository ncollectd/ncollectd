/* SPDX-License-Identifier: GPL-2.0-only OR MIT                 */
/* SPDX-FileCopyrightText: Copyright (C) 2010 Florian Forster   */
/* SPDX-FileContributor: Florian Forster <octo at collectd.org> */

#pragma once

#include "plugin.h"

#include <lua.h>

cdtime_t luac_to_cdtime(lua_State *L, int idx);

int luac_push_cdtime(lua_State *L, cdtime_t t);

metric_family_t *luac_to_metric_family(lua_State *L, int idx);

int luac_push_metric_family(lua_State *L, metric_family_t const *fam);

notification_t *luac_to_notification(lua_State *L, int idx);

int luac_push_notification(lua_State *L, const notification_t *notif);

int luac_push_config_item(lua_State *L, const config_item_t *ci);
