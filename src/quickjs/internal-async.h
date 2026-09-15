/*
 * QuickJS Javascript Engine
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef QUICKJS_INTERNAL_ASYNC_H
#define QUICKJS_INTERNAL_ASYNC_H

#include "internal-builtin.h"
#include "internal-array-algorithm.h"

typedef enum QJSGeneratorMagic {
    QJS_GEN_MAGIC_NEXT,
    QJS_GEN_MAGIC_RETURN,
    QJS_GEN_MAGIC_THROW,
} QJSGeneratorMagic;

QJS_INTERNAL int qjs_add_intrinsic_generator(JSContext *ctx);
QJS_INTERNAL int qjs_async_to_int32_free(JSContext *ctx, int32_t *result,
                                         JSValue value);
QJS_INTERNAL JSValue qjs_async_invoke_free(JSContext *ctx, JSValue value,
                                           JSAtom atom, int argc,
                                           JSValueConst *argv);
QJS_INTERNAL JSValue qjs_async_create_from_ctor(JSContext *ctx,
                                                JSValueConst ctor,
                                                int class_id);
QJS_INTERNAL JSValue qjs_async_species_constructor(JSContext *ctx,
                                                   JSValueConst obj,
                                                   JSValueConst default_ctor);
QJS_INTERNAL JSValue qjs_async_aggregate_error_constructor(
    JSContext *ctx, JSValueConst errors);
QJS_INTERNAL void qjs_async_dump_value(JSContext *ctx, const char *name,
                                       JSValueConst value);
QJS_INTERNAL JSValueConst qjs_async_c_function_data(
    JSValueConst function, int index);

QJS_INTERNAL JSValue qjs_generator_next(JSContext *ctx,
                                        JSValueConst this_val,
                                        int argc, JSValueConst *argv,
                                        BOOL *done, int magic);
QJS_INTERNAL void qjs_async_bytecode_finalizer(JSRuntime *rt, JSValue value);
QJS_INTERNAL void qjs_async_bytecode_mark(JSRuntime *rt, JSValueConst value,
                                          JS_MarkFunc *mark_func);
QJS_INTERNAL void qjs_async_resolve_finalizer(JSRuntime *rt, JSValue value);
QJS_INTERNAL void qjs_async_resolve_mark(JSRuntime *rt, JSValueConst value,
                                         JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue qjs_async_resolve_call(JSContext *ctx,
                                            JSValueConst func_obj,
                                            JSValueConst this_obj,
                                            int argc, JSValueConst *argv,
                                            int flags);
QJS_INTERNAL void qjs_async_generator_finalizer(JSRuntime *rt, JSValue value);
QJS_INTERNAL void qjs_async_generator_mark(JSRuntime *rt, JSValueConst value,
                                           JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue qjs_async_generator_function_call(
    JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj,
    int argc, JSValueConst *argv, int flags);
QJS_INTERNAL JSValue qjs_async_generator_next(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv,
                                              int magic);
QJS_INTERNAL JSValue qjs_async_function_constructor(
    JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv,
    int magic);

QJS_INTERNAL int qjs_async_perform_promise_then(
    JSContext *ctx, JSValueConst promise, JSValueConst *resolve_reject,
    JSValueConst *cap_resolving_funcs);
QJS_INTERNAL JSValue qjs_async_promise_resolve(
    JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv,
    int magic);
QJS_INTERNAL JSValue qjs_async_create_from_sync_iterator(
    JSContext *ctx, JSValueConst sync_iterator);
QJS_INTERNAL JSValue qjs_promise_then(JSContext *ctx,
                                      JSValueConst this_val,
                                      int argc, JSValueConst *argv);

#endif
