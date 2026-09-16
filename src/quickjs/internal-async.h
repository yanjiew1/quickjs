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

QJS_INTERNAL JSValue JS_InvokeFree(JSContext *ctx, JSValue this_val, JSAtom atom,
                             int argc, JSValueConst *argv);
QJS_INTERNAL JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                                   int class_id);
QJS_INTERNAL JSValue JS_SpeciesConstructor(JSContext *ctx, JSValueConst obj,
                                     JSValueConst defaultConstructor);
QJS_INTERNAL JSValue js_aggregate_error_constructor(JSContext *ctx,
                                              JSValueConst errors);
QJS_INTERNAL __maybe_unused void JS_DumpValue(JSContext *ctx, const char *str, JSValueConst val);

QJS_INTERNAL JSValue js_generator_next(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv,
                                 BOOL *pdone, int magic);
QJS_INTERNAL void js_bytecode_function_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_bytecode_function_mark(JSRuntime *rt, JSValueConst val,
                                      JS_MarkFunc *mark_func);
QJS_INTERNAL void js_async_function_resolve_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_async_function_resolve_mark(JSRuntime *rt, JSValueConst val,
                                           JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_async_function_resolve_call(JSContext *ctx,
                                              JSValueConst func_obj,
                                              JSValueConst this_obj,
                                              int argc, JSValueConst *argv,
                                              int flags);
QJS_INTERNAL void js_async_generator_finalizer(JSRuntime *rt, JSValue obj);
QJS_INTERNAL void js_async_generator_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_async_generator_function_call(JSContext *ctx, JSValueConst func_obj,
                                                JSValueConst this_obj,
                                                int argc, JSValueConst *argv,
                                                int flags);
QJS_INTERNAL JSValue js_async_generator_next(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv,
                                       int magic);
QJS_INTERNAL JSValue js_function_constructor(JSContext *ctx, JSValueConst new_target,
                                       int argc, JSValueConst *argv, int magic);

QJS_INTERNAL __exception int perform_promise_then(JSContext *ctx,
                                            JSValueConst promise,
                                            JSValueConst *resolve_reject,
                                            JSValueConst *cap_resolving_funcs);
QJS_INTERNAL JSValue js_promise_resolve(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic);
QJS_INTERNAL JSValue JS_CreateAsyncFromSyncIterator(JSContext *ctx,
                                              JSValueConst sync_iter);
QJS_INTERNAL JSValue js_promise_then(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv);

#endif
