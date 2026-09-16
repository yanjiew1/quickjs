/*
 * QuickJS Javascript Engine
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
#ifndef QUICKJS_INTERNAL_FUNCTION_H
#define QUICKJS_INTERNAL_FUNCTION_H

#include "internal-property.h"

#define GLOBAL_VAR_OFFSET 0x40000000
#define ARGUMENT_VAR_OFFSET 0x20000000
#define JS_DEFINE_CLASS_HAS_HERITAGE       (1 << 0)
#define JS_THROW_VAR_RO                    0
#define JS_THROW_VAR_REDECL                1
#define JS_THROW_VAR_UNINITIALIZED         2
#define JS_THROW_ERROR_DELETE_SUPER        3
#define JS_THROW_ERROR_ITERATOR_THROW      4
#define OP_DEFINE_METHOD_METHOD            0
#define OP_DEFINE_METHOD_GETTER            1
#define OP_DEFINE_METHOD_SETTER            2
#define OP_DEFINE_METHOD_ENUMERABLE        4

typedef enum {
    OP_SPECIAL_OBJECT_ARGUMENTS,
    OP_SPECIAL_OBJECT_MAPPED_ARGUMENTS,
    OP_SPECIAL_OBJECT_THIS_FUNC,
    OP_SPECIAL_OBJECT_NEW_TARGET,
    OP_SPECIAL_OBJECT_HOME_OBJECT,
    OP_SPECIAL_OBJECT_VAR_OBJECT,
    OP_SPECIAL_OBJECT_IMPORT_META,
} OPSpecialObjectEnum;

QJS_INTERNAL BOOL js_class_has_bytecode(JSClassID class_id);

QJS_INTERNAL BOOL is_strict_mode(JSContext *ctx);

QJS_INTERNAL void __async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
QJS_INTERNAL JSValue js_instantiate_prototype(JSContext *ctx, JSObject *p, JSAtom atom, void *opaque);
QJS_INTERNAL JSValueConst JS_GetActiveFunction(JSContext *ctx);
QJS_INTERNAL JSVarRef *js_global_object_find_uninitialized_var(JSContext *ctx, JSObject *p,
                                                         JSAtom atom, BOOL is_lexical);
QJS_INTERNAL JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                                   int class_id);

QJS_INTERNAL JSVarRef *js_create_var_ref(JSContext *ctx, BOOL is_lexical);
QJS_INTERNAL JSValue js_closure2(JSContext *ctx, JSValue func_obj,
                           JSFunctionBytecode *b,
                           JSVarRef **cur_var_refs,
                           JSStackFrame *sf,
                           BOOL is_eval, JSModuleDef *m);
QJS_INTERNAL JSValue js_async_function_call(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst this_obj,
                                      int argc, JSValueConst *argv, int flags);
QJS_INTERNAL void js_function_set_properties(JSContext *ctx, JSValueConst func_obj,
                                       JSAtom name, int len);
/* Promise implementation is owned by builtin-async.c. */
QJS_INTERNAL JSValue js_promise_then(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv);
QJS_INTERNAL JSVarRef *get_var_ref(JSContext *ctx, JSStackFrame *sf, int var_idx,
                             BOOL is_arg);
QJS_INTERNAL JSValue js_closure(JSContext *ctx, JSValue bfunc,
                          JSVarRef **cur_var_refs,
                          JSStackFrame *sf, BOOL is_eval);
QJS_INTERNAL JSValue JS_CallFree(JSContext *ctx, JSValue func_obj, JSValueConst this_obj,
                           int argc, JSValueConst *argv);
QJS_INTERNAL int JS_OrdinaryIsInstanceOf(JSContext *ctx, JSValueConst val,
                                   JSValueConst obj);
QJS_INTERNAL JSContext *JS_GetFunctionRealm(JSContext *ctx, JSValueConst func_obj);
QJS_INTERNAL JSValue JS_InvokeFree(JSContext *ctx, JSValue this_val, JSAtom atom,
                             int argc, JSValueConst *argv);
QJS_INTERNAL void js_method_set_home_object(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst home_obj);
QJS_INTERNAL int js_method_set_properties(JSContext *ctx, JSValueConst func_obj,
                                    JSAtom name, int flags, JSValueConst home_obj);
BOOL JS_IsCFunction(JSContext *ctx, JSValueConst val, JSCFunction *func, int magic);


/* Cross-TU declarations owned by this subsystem. */
extern QJS_INTERNAL const JSClassExoticMethods js_arguments_exotic_methods;

extern QJS_INTERNAL const uint16_t func_kind_to_class_id[4];

QJS_INTERNAL JSValue JS_CallConstructorInternal(JSContext *ctx,
                                          JSValueConst func_obj,
                                          JSValueConst new_target,
                                          int argc, JSValue *argv, int flags);

QJS_INTERNAL JSValue JS_CallInternal(JSContext *caller_ctx, JSValueConst func_obj,
                               JSValueConst this_obj, JSValueConst new_target,
                               int argc, JSValue *argv, int flags);

QJS_INTERNAL JSFunctionBytecode *JS_GetFunctionBytecode(JSValueConst val);

QJS_INTERNAL void async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);

QJS_INTERNAL void close_lexical_var(JSContext *ctx, JSFunctionBytecode *b,
                              JSStackFrame *sf, int var_idx);

QJS_INTERNAL void close_var_refs(JSRuntime *rt, JSFunctionBytecode *b, JSStackFrame *sf);

QJS_INTERNAL void js_bound_function_finalizer(JSRuntime *rt, JSValue val);

QJS_INTERNAL void js_bound_function_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);

QJS_INTERNAL JSValue js_build_arguments(JSContext *ctx, int argc, JSValueConst *argv);

QJS_INTERNAL JSValue js_build_mapped_arguments(JSContext *ctx, int argc,
                                         JSValueConst *argv,
                                         JSStackFrame *sf, int arg_count);

QJS_INTERNAL JSValue js_c_function_data_call(JSContext *ctx, JSValueConst func_obj,
                                       JSValueConst this_val,
                                       int argc, JSValueConst *argv, int flags);

QJS_INTERNAL void js_c_function_data_finalizer(JSRuntime *rt, JSValue val);

QJS_INTERNAL void js_c_function_data_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);

QJS_INTERNAL void js_c_function_finalizer(JSRuntime *rt, JSValue val);

QJS_INTERNAL void js_c_function_mark(JSRuntime *rt, JSValueConst val,
                               JS_MarkFunc *mark_func);

QJS_INTERNAL JSValue js_call_bound_function(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst this_obj,
                                      int argc, JSValueConst *argv, int flags);

QJS_INTERNAL JSValue js_call_c_function(JSContext *ctx, JSValueConst func_obj,
                                  JSValueConst this_obj,
                                  int argc, JSValueConst *argv, int flags);

QJS_INTERNAL void js_generator_finalizer(JSRuntime *rt, JSValue obj);

QJS_INTERNAL JSValue js_generator_function_call(JSContext *ctx, JSValueConst func_obj,
                                          JSValueConst this_obj,
                                          int argc, JSValueConst *argv,
                                          int flags);

QJS_INTERNAL void js_generator_mark(JSRuntime *rt, JSValueConst val,
                              JS_MarkFunc *mark_func);

QJS_INTERNAL JSValue js_get_function_name(JSContext *ctx, JSAtom name);

QJS_INTERNAL void js_global_object_finalizer(JSRuntime *rt, JSValue obj);

QJS_INTERNAL void js_global_object_mark(JSRuntime *rt, JSValueConst val,
                                  JS_MarkFunc *mark_func);

QJS_INTERNAL void js_mapped_arguments_finalizer(JSRuntime *rt, JSValue val);

QJS_INTERNAL void js_mapped_arguments_mark(JSRuntime *rt, JSValueConst val,
                                     JS_MarkFunc *mark_func);

#endif /* QUICKJS_INTERNAL_FUNCTION_H */
