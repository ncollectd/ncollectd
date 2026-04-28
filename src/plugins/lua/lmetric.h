/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín       */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#define MT_METRIC_UNKNOWN_DOUBLE  "mt_metric_unknown_double"
#define MT_METRIC_UNKNOWN_INTEGER "mt_metric_unknown_integer"
#define MT_METRIC_GAUGE_DOUBLE    "mt_metric_gauge_double"
#define MT_METRIC_GAUGE_INTEGER   "mt_metric_gauge_integer"
#define MT_METRIC_COUNTER_DOUBLE  "mt_metric_counter_double"
#define MT_METRIC_COUNTER_INTEGER "mt_metric_counter_integer"
#define MT_METRIC_INFO            "mt_metric_info"
#define MT_METRIC_STATE_SET       "mt_metric_state_set"
#define MT_METRIC_SUMMARY         "mt_metric_summary"
#define MT_METRIC_HISTOGRAM       "mt_metric_histogram"
#define MT_METRIC_GAUGE_HISTOGRAM "mt_metric_gauge_histogram"

int register_metric_unknown_double(lua_State *L);

int register_metric_unknown_interger(lua_State *L);

int register_metric_gauge_double(lua_State *L);

int register_metric_gauge_interger(lua_State *L);

int register_metric_counter_double(lua_State *L);

int register_metric_counter_interger(lua_State *L);

int register_metric_info(lua_State *L);

int register_metric_state_set(lua_State *L);

int register_metric_summary(lua_State *L);

int register_metric_histogram(lua_State *L);

int register_metric_gauge_histogram(lua_State *L);

int lmetric_tostring_buffer(strbuf_t *buf, metric_type_t type, metric_t *m);

int luac_push_metric(lua_State *L, metric_type_t type, metric_t *m);

