/* SPDX-License-Identifier: GPL-2.0-only */

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "libutils/time.h"

#ifndef LOG_ERR
#define LOG_ERR 3
#endif
#ifndef LOG_WARNING
#define LOG_WARNING 4
#endif
#ifndef LOG_NOTICE
#define LOG_NOTICE 5
#endif
#ifndef LOG_INFO
#define LOG_INFO 6
#endif
#ifndef LOG_DEBUG
#define LOG_DEBUG 7
#endif

typedef struct {
    int severity;
    cdtime_t time;
    char const *plugin;
    char const *file;
    int line;
    char const *func;
    char const *msg;
} log_msg_t;

void plugin_log(int level, const char *file, int line, const char *func, const char *format, ...)
    __attribute__((format(printf, 5, 6)));

#define PLUGIN_ERROR(...) plugin_log(LOG_ERR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define PLUGIN_WARNING(...) plugin_log(LOG_WARNING, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define PLUGIN_NOTICE(...) plugin_log(LOG_NOTICE, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define PLUGIN_INFO(...) plugin_log(LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#ifdef NCOLLECTD_DEBUG
#define PLUGIN_DEBUG(...) plugin_log(LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
#define PLUGIN_DEBUG(...) /* noop */
#endif

#define ERROR(...) plugin_log(LOG_ERR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define WARNING(...) plugin_log(LOG_WARNING, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define NOTICE(...) plugin_log(LOG_NOTICE, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define INFO(...) plugin_log(LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#ifdef NCOLLECTD_DEBUG
#define DEBUG(...) plugin_log(LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
#define DEBUG(...) /* noop */
#endif
