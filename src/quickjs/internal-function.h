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

static inline BOOL qjs_class_has_bytecode(JSClassID class_id)
{
    return class_id == JS_CLASS_BYTECODE_FUNCTION ||
           class_id == JS_CLASS_GENERATOR_FUNCTION ||
           class_id == JS_CLASS_ASYNC_FUNCTION ||
           class_id == JS_CLASS_ASYNC_GENERATOR_FUNCTION;
}

QJS_INTERNAL JSVarRef *qjs_create_var_ref(JSContext *ctx, BOOL is_lexical);
QJS_INTERNAL JSValue qjs_closure2(JSContext *ctx, JSValue func_obj,
                                 JSFunctionBytecode *bytecode,
                                 JSVarRef **cur_var_refs,
                                 JSStackFrame *stack_frame, BOOL is_eval,
                                 JSModuleDef *module);
QJS_INTERNAL JSValue qjs_async_function_call(JSContext *ctx,
                                             JSValueConst func_obj,
                                             JSValueConst this_obj,
                                             int argc, JSValueConst *argv,
                                             int flags);
QJS_INTERNAL JSValue qjs_promise_then(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv);
QJS_INTERNAL JSVarRef *qjs_get_var_ref(JSContext *ctx, JSStackFrame *frame,
                                      int var_idx, BOOL is_arg);
QJS_INTERNAL JSValue qjs_closure(JSContext *ctx, JSValue bytecode_func,
                                JSVarRef **cur_var_refs,
                                JSStackFrame *stack_frame, BOOL is_eval);
QJS_INTERNAL JSValue qjs_call_free(JSContext *ctx, JSValue func_obj,
                                   JSValueConst this_obj, int argc,
                                   JSValueConst *argv);


#endif /* QUICKJS_INTERNAL_FUNCTION_H */
