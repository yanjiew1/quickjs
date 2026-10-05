/*
 * QuickJS iterator protocol interfaces
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
#ifndef QUICKJS_ITERATOR_H
#define QUICKJS_ITERATOR_H

#include "object.h"

typedef enum JSIteratorKindEnum {
    JS_ITERATOR_KIND_KEY,
    JS_ITERATOR_KIND_VALUE,
    JS_ITERATOR_KIND_KEY_AND_VALUE,
} JSIteratorKindEnum;

JSValue JS_GetIterator2(JSContext *ctx, JSValueConst obj, JSValueConst method);
JSValue JS_GetIterator(JSContext *ctx, JSValueConst obj, BOOL is_async);

/* return *pdone = 2 if the iterator object is not parsed */
static inline JSValue JS_IteratorNext2(JSContext *ctx, JSValueConst enum_obj,
                                       JSValueConst method,
                                       int argc, JSValueConst *argv, int *pdone)
{
    JSValue obj;

    /* fast path for the built-in iterators (avoid creating the
       intermediate result object) */
    if (JS_IsObject(method)) {
        JSObject *p = JS_VALUE_GET_OBJ(method);
        if (p->class_id == JS_CLASS_C_FUNCTION &&
            p->u.cfunc.cproto == JS_CFUNC_iterator_next) {
            JSCFunctionType func;
            JSValueConst args[1];

            /* in case the function expects one argument */
            if (argc == 0) {
                args[0] = JS_UNDEFINED;
                argv = args;
            }
            func = p->u.cfunc.c_function;
            return func.iterator_next(ctx, enum_obj, argc, argv,
                                      pdone, p->u.cfunc.magic);
        }
    }
    obj = JS_Call(ctx, method, enum_obj, argc, argv);
    if (JS_IsException(obj))
        goto fail;
    if (!JS_IsObject(obj)) {
        JS_FreeValue(ctx, obj);
        JS_ThrowTypeError(ctx, "iterator must return an object");
        goto fail;
    }
    *pdone = 2;
    return obj;
 fail:
    *pdone = FALSE;
    return JS_EXCEPTION;
}

JSValue JS_IteratorNext(JSContext *ctx, JSValueConst enum_obj,
                        JSValueConst method, int argc, JSValueConst *argv,
                        BOOL *pdone);
int JS_IteratorClose(JSContext *ctx, JSValueConst enum_obj,
                     BOOL is_exception_pending);
JSValue JS_IteratorGetCompleteValue(JSContext *ctx, JSValueConst obj,
                                    BOOL *pdone);
JSValue js_create_iterator_result(JSContext *ctx, JSValue val, BOOL done);

/* XXX: use enum */
#define GEN_MAGIC_THROW  2
#define GEN_MAGIC_RETURN 1
#define GEN_MAGIC_NEXT   0


#endif
