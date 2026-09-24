/*
 * QuickJS Error Builtin Interface
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
#ifndef QUICKJS_BUILTINS_ERROR_H
#define QUICKJS_BUILTINS_ERROR_H

#include "internal/object.h"

JSValue js_aggregate_error_constructor(JSContext *ctx,
                                              JSValueConst errors);

JSValue js_error_constructor(JSContext *ctx, JSValueConst new_target,
                                    int argc, JSValueConst *argv, int magic);

extern const JSCFunctionListEntry js_error_proto_funcs[3];
extern const JSCFunctionListEntry js_native_error_proto_funcs[16];
extern const JSCFunctionListEntry js_error_funcs[1];

#endif /* QUICKJS_BUILTINS_ERROR_H */
