/*
 * QuickJS Array Builtins Internal Interface
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
#ifndef QJS_BUILTIN_ARRAY_H
#define QJS_BUILTIN_ARRAY_H

#include "../internal/base.h"

QJS_INTERNAL void js_array_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_array_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
QJS_INTERNAL void js_array_iterator_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_array_iterator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
extern QJS_INTERNAL const JSCFunctionListEntry js_array_funcs[4];
extern QJS_INTERNAL const JSCFunctionListEntry js_array_proto_funcs[40];
extern QJS_INTERNAL const JSCFunctionListEntry js_array_iterator_proto_funcs[2];

QJS_INTERNAL JSValue js_array_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
QJS_INTERNAL JSValue js_array_iterator_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, BOOL *pdone, int magic);

QJS_INTERNAL JSValue js_array_push(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int unshift);
QJS_INTERNAL JSValue js_array_pop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int shift);
QJS_INTERNAL JSValue js_array_includes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

#endif /* QJS_BUILTIN_ARRAY_H */
