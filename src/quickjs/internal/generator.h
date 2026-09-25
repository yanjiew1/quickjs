/*
 * QuickJS generator and async function lifecycle
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
#ifndef QUICKJS_GENERATOR_H
#define QUICKJS_GENERATOR_H

#include "runtime.h"

typedef struct JSAsyncFunctionState {
    JSGCObjectHeader header;
    JSValue this_val; /* 'this' argument */
    int argc; /* number of function arguments */
    BOOL throw_flag; /* used to throw an exception in JS_CallInternal() */
    BOOL is_completed; /* TRUE if the function has returned. The stack
                          frame is no longer valid */
    JSValue resolving_funcs[2]; /* only used in JS async functions */
    JSStackFrame frame;
    /* arg_buf, var_buf, stack_buf and var_refs follow */
} JSAsyncFunctionState;

#define GEN_MAGIC_THROW  2
#define GEN_MAGIC_RETURN 1
#define GEN_MAGIC_NEXT   0

void __async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
void async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
JSValue js_async_function_call(JSContext *ctx, JSValueConst func_obj,
                               JSValueConst this_obj, int argc,
                               JSValueConst *argv, int flags);
void js_async_function_resolve_finalizer(JSRuntime *rt, JSValue val);
void js_async_function_resolve_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);
JSValue js_async_function_resolve_call(JSContext *ctx,
                                       JSValueConst func_obj,
                                       JSValueConst this_obj, int argc,
                                       JSValueConst *argv, int flags);
void js_async_generator_finalizer(JSRuntime *rt, JSValue obj);
void js_async_generator_mark(JSRuntime *rt, JSValueConst val,
                             JS_MarkFunc *mark_func);
JSValue js_async_generator_next(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv, int magic);
JSValue js_async_generator_function_call(JSContext *ctx, JSValueConst func_obj,
                                         JSValueConst this_obj, int argc,
                                         JSValueConst *argv, int flags);
JSValue js_generator_next(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv, BOOL *pdone,
                          int magic);
void js_generator_finalizer(JSRuntime *rt, JSValue obj);
void js_generator_mark(JSRuntime *rt, JSValueConst val,
                       JS_MarkFunc *mark_func);
JSValue js_generator_function_call(JSContext *ctx, JSValueConst func_obj,
                                   JSValueConst this_obj, int argc,
                                   JSValueConst *argv, int flags);

#endif
