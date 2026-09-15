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

#include "internal-object.h"

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

#endif /* QUICKJS_INTERNAL_FUNCTION_H */
