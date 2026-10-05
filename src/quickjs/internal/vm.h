/*
 * QuickJS bytecode VM interfaces
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
#ifndef QUICKJS_VM_H
#define QUICKJS_VM_H

#include "function.h"

typedef struct JSForInIterator {
    JSValue obj;
    uint32_t idx;
    uint32_t atom_count;
    uint8_t in_prototype_chain;
    uint8_t is_array;
    JSPropertyEnum *tab_atom; /* is_array = FALSE */
} JSForInIterator;

#define JS_CALL_FLAG_GENERATOR   (1 << 2)

#define FUNC_RET_AWAIT         0
#define FUNC_RET_YIELD         1
#define FUNC_RET_YIELD_STAR    2
#define FUNC_RET_INITIAL_YIELD 3

/* A generator resumes its detached stack frame through this VM entry point. */
JSValue JS_CallInternal(JSContext *ctx, JSValueConst func_obj,
                        JSValueConst this_obj, JSValueConst new_target,
                        int argc, JSValue *argv, int flags);
void close_var_refs(JSRuntime *rt, JSFunctionBytecode *b, JSStackFrame *sf);
void js_for_in_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_for_in_iterator_mark(JSRuntime *rt, JSValueConst val,
                             JS_MarkFunc *mark_func);

static inline BOOL is_strict_mode(JSContext *ctx)
{
    JSStackFrame *sf = ctx->rt->current_stack_frame;
    return (sf && (sf->js_mode & JS_MODE_STRICT));
}

JSValue JS_CallFree(JSContext *ctx, JSValue func_obj, JSValueConst this_obj,
                    int argc, JSValueConst *argv);

JSValueConst JS_GetActiveFunction(JSContext *ctx);

JSValue js_closure2(JSContext *ctx, JSValue func_obj,
                    JSFunctionBytecode *b,
                    JSVarRef **cur_var_refs,
                    JSStackFrame *sf,
                    BOOL is_eval, JSModuleDef *m);

JSValue js_closure(JSContext *ctx, JSValue bfunc,
                   JSVarRef **cur_var_refs,
                   JSStackFrame *sf, BOOL is_eval);

JSValue JS_InvokeFree(JSContext *ctx, JSValue this_val, JSAtom atom,
                      int argc, JSValueConst *argv);



void js_mapped_arguments_finalizer(JSRuntime *rt, JSValue val);

void js_mapped_arguments_mark(JSRuntime *rt, JSValueConst val,
                              JS_MarkFunc *mark_func);

extern const JSClassExoticMethods js_arguments_exotic_methods;

#endif
