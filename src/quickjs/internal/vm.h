/*
 * QuickJS Vm Internal Interface
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
#ifndef QJS_VM_H
#define QJS_VM_H

#include "runtime.h"
#include "function.h"

QJS_INTERNAL JSValue JS_EvalObject(JSContext *ctx, JSValueConst this_obj,
                                    JSValueConst val, int flags, int scope_idx);

QJS_INTERNAL void js_async_function_resolve_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_async_function_resolve_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
QJS_INTERNAL void js_async_generator_finalizer(JSRuntime *rt, JSValue obj);
QJS_INTERNAL void js_async_generator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_async_generator_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
QJS_INTERNAL JSValue js_async_function_call(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj, int argc, JSValueConst *argv, int flags);
QJS_INTERNAL JSValue js_async_function_resolve_call(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj, int argc, JSValueConst *argv, int flags);
QJS_INTERNAL JSValue js_async_generator_function_call(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj, int argc, JSValueConst *argv, int flags);

QJS_INTERNAL int find_line_num(JSContext *ctx, JSFunctionBytecode *b, uint32_t pc_value, int *pcol_num);

QJS_INTERNAL JSValue js_generator_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, BOOL *pdone, int magic);

QJS_INTERNAL JSValue __JS_EvalInternal(JSContext *ctx, JSValueConst this_obj, const char *input, size_t input_len, const char *filename, int flags, int scope_idx);

#define GLOBAL_VAR_OFFSET 0x40000000
#define ARGUMENT_VAR_OFFSET 0x20000000

#define JS_DEFINE_CLASS_HAS_HERITAGE     (1 << 0)

#define JS_THROW_VAR_RO             0
#define JS_THROW_VAR_REDECL         1
#define JS_THROW_VAR_UNINITIALIZED  2
#define JS_THROW_ERROR_DELETE_SUPER   3
#define JS_THROW_ERROR_ITERATOR_THROW 4
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

QJS_INTERNAL JSValue js_call_c_function(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj, int argc, JSValueConst *argv, int flags);
QJS_INTERNAL JSValue js_call_bound_function(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj, int argc, JSValueConst *argv, int flags);
QJS_INTERNAL JSValue js_generator_function_call(JSContext *ctx, JSValueConst func_obj, JSValueConst this_obj, int argc, JSValueConst *argv, int flags);
QJS_INTERNAL void __async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
QJS_INTERNAL void async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);

QJS_INTERNAL void js_global_object_finalizer(JSRuntime *rt, JSValue obj);
QJS_INTERNAL void js_global_object_mark(JSRuntime *rt, JSValueConst val,
                                  JS_MarkFunc *mark_func);
QJS_INTERNAL void js_generator_finalizer(JSRuntime *rt, JSValue obj);
QJS_INTERNAL void js_generator_mark(JSRuntime *rt, JSValueConst val,
                              JS_MarkFunc *mark_func);
QJS_INTERNAL JSVarRef *get_var_ref(JSContext *ctx, JSStackFrame *sf, int var_idx,
                             BOOL is_arg);

#endif /* QJS_VM_H */
