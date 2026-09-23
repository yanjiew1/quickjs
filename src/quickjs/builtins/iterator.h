/*
 * QuickJS Iterator Builtins Internal Interface
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
#ifndef QJS_BUILTIN_ITERATOR_H
#define QJS_BUILTIN_ITERATOR_H

#include "../internal/base.h"

QJS_INTERNAL void js_iterator_concat_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_iterator_concat_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
QJS_INTERNAL void js_iterator_helper_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_iterator_helper_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
QJS_INTERNAL void js_iterator_wrap_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_iterator_wrap_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
extern QJS_INTERNAL const JSCFunctionListEntry js_iterator_wrap_proto_funcs[2];
extern QJS_INTERNAL const JSCFunctionListEntry js_iterator_concat_proto_funcs[3];
extern QJS_INTERNAL const JSCFunctionListEntry js_iterator_funcs[2];
extern QJS_INTERNAL const JSCFunctionListEntry js_iterator_proto_funcs[13];
extern QJS_INTERNAL const JSCFunctionListEntry js_iterator_helper_proto_funcs[3];
extern QJS_INTERNAL const JSCFunctionListEntry js_generator_function_proto_funcs[1];
extern QJS_INTERNAL const JSCFunctionListEntry js_generator_proto_funcs[4];

QJS_INTERNAL JSValue js_iterator_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
QJS_INTERNAL JSValue js_iterator_constructor_getset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *func_data);

#endif /* QJS_BUILTIN_ITERATOR_H */
