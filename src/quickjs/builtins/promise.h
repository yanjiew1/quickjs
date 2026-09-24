/*
 * QuickJS Promise builtin interface
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
#ifndef QUICKJS_BUILTINS_PROMISE_H
#define QUICKJS_BUILTINS_PROMISE_H

#include "../internal/base.h"

void js_promise_finalizer(JSRuntime *rt, JSValue val);
void js_promise_mark(JSRuntime *rt, JSValueConst val,
                            JS_MarkFunc *mark_func);
void js_promise_resolve_function_finalizer(JSRuntime *rt, JSValue val);
void js_promise_resolve_function_mark(JSRuntime *rt, JSValueConst val,
                                             JS_MarkFunc *mark_func);
JSValue js_new_promise_capability(JSContext *ctx,
                                         JSValue *resolving_funcs,
                                         JSValueConst ctor);
JSValue js_promise_resolve(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic);
__exception int perform_promise_then(JSContext *ctx,
                                            JSValueConst promise,
                                            JSValueConst *resolve_reject,
                                            JSValueConst *cap_resolving_funcs);
JSValue JS_CreateAsyncFromSyncIterator(JSContext *ctx,
                                              JSValueConst sync_iter);
void js_async_from_sync_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_async_from_sync_iterator_mark(JSRuntime *rt, JSValueConst val,
                                             JS_MarkFunc *mark_func);
JSValue js_promise_then(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv);

#endif
