/*
 * QuickJS Javascript Engine - Function Frames, Calls & Closures
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

#ifndef QUICKJS_JS_FUNC_H
#define QUICKJS_JS_FUNC_H

#include "quickjs.h"
#include "quickjs-internal.h"

void free_var_ref(JSRuntime *rt, JSVarRef *var_ref);
JSVarRef *get_var_ref(JSContext *ctx, JSStackFrame *sf, int var_idx,
                      BOOL is_arg);

JSValue js_build_arguments(JSContext *ctx, int argc, JSValueConst *argv);
JSValue js_build_mapped_arguments(JSContext *ctx, int argc,
                                  JSValueConst *argv,
                                  JSStackFrame *sf, int arg_count);

JSValue js_closure2(JSContext *ctx, JSValue func_obj,
                    JSFunctionBytecode *b, JSVarRef **cur_var_refs,
                    JSStackFrame *sf, BOOL is_eval, JSModuleDef *home_module);
JSValue js_closure(JSContext *ctx, JSValue bfunc,
                   JSVarRef **cur_var_refs, JSStackFrame *sf,
                   BOOL is_eval);
int js_op_define_class(JSContext *ctx, JSValue *sp, JSAtom class_name,
                       int class_flags, JSVarRef **cur_var_refs,
                       JSStackFrame *sf, BOOL is_computed_name);
void close_var_refs(JSRuntime *rt, JSFunctionBytecode *b, JSStackFrame *sf);
void close_lexical_var(JSContext *ctx, JSFunctionBytecode *b,
                       JSStackFrame *sf, int idx);

JSValue JS_CallConstructorInternal(JSContext *ctx, JSValueConst func_obj,
                                   JSValueConst new_target,
                                   int argc, JSValue *argv, int flags);

void free_function_bytecode(JSRuntime *rt, JSFunctionBytecode *b);

#endif /* QUICKJS_JS_FUNC_H */
