/* SPDX-License-Identifier: GPL-2.0-only OR MIT                    */
/* SPDX-FileCopyrightText: Copyright (c) 2017-2018 Fabrice Bellard */
/* SPDX-FileContributor: Fabrice Bellard                           */

/* QuickJS C library */

#ifndef QUICKJS_LIBC_H
#define QUICKJS_LIBC_H

#include <stdio.h>
#include <stdlib.h>

#include "quickjs.h"

#ifdef __cplusplus
extern "C" {
#endif

JSModuleDef *js_init_module_std(JSContext *ctx, const char *module_name);
JSModuleDef *js_init_module_os(JSContext *ctx, const char *module_name);
void js_std_add_helpers(JSContext *ctx, int argc, char **argv);
void js_std_loop(JSContext *ctx);
JSValue js_std_await(JSContext *ctx, JSValue obj);
void js_std_init_handlers(JSRuntime *rt);
void js_std_free_handlers(JSRuntime *rt);
void js_std_dump_error(JSContext *ctx);
uint8_t *js_load_file(JSContext *ctx, size_t *pbuf_len, const char *filename);
int js_module_set_import_meta(JSContext *ctx, JSValueConst func_val,
                              JS_BOOL use_realpath, JS_BOOL is_main);
int js_module_test_json(JSContext *ctx, JSValueConst attributes);
int js_module_check_attributes(JSContext *ctx, void *opaque, JSValueConst attributes);
JSModuleDef *js_module_loader(JSContext *ctx,
                              const char *module_name, void *opaque,
                              JSValueConst attributes);
void js_std_eval_binary(JSContext *ctx, const uint8_t *buf, size_t buf_len,
                        int flags);
void js_std_eval_binary_json_module(JSContext *ctx,
                                    const uint8_t *buf, size_t buf_len,
                                    const char *module_name);
void js_std_promise_rejection_tracker(JSContext *ctx, JSValueConst promise,
                                      JSValueConst reason,
                                      JS_BOOL is_handled, void *opaque);
void js_std_set_worker_new_context_func(JSContext *(*func)(JSRuntime *rt));

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* QUICKJS_LIBC_H */
