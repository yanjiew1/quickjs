/*
 * QuickJS Function and Generator builtin interfaces
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef QUICKJS_BUILTINS_FUNCTION_H
#define QUICKJS_BUILTINS_FUNCTION_H

#include "../internal/base.h"

JSValue js_throw_type_error(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv);
JSValue js_function_proto(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv);
JSValue js_function_constructor(JSContext *ctx, JSValueConst new_target,
                                int argc, JSValueConst *argv, int magic);
JSValue js_function_apply(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv, int magic);
JSValue *build_arg_list(JSContext *ctx, uint32_t *plen, JSValueConst array_arg);
void free_arg_list(JSContext *ctx, JSValue *tab, uint32_t len);
extern const JSCFunctionListEntry js_function_proto_funcs[8];
extern const JSCFunctionListEntry js_generator_function_proto_funcs[1];
extern const JSCFunctionListEntry js_generator_proto_funcs[4];

#endif
