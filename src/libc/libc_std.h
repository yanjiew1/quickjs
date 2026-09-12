/*
 * QuickJS C library: Standard Subsystem Header
 *
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
 */
#ifndef QUICKJS_LIBC_STD_H
#define QUICKJS_LIBC_STD_H

#include "libc_internal.h"

JSModuleDef *js_init_module_std(JSContext *ctx, const char *module_name);
void js_std_add_helpers(JSContext *ctx, int argc, char **argv);
void js_std_dump_error(JSContext *ctx);
void js_std_dump_error1(JSContext *ctx, JSValueConst exp);
int get_bool_option(JSContext *ctx, BOOL *pbool, JSValueConst obj, const char *option);
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

#endif /* QUICKJS_LIBC_STD_H */
