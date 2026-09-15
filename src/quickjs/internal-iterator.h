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
#ifndef QUICKJS_INTERNAL_ITERATOR_H
#define QUICKJS_INTERNAL_ITERATOR_H

#include "internal-function.h"

QJS_INTERNAL JSValue JS_GetIterator2(JSContext *ctx, JSValueConst obj,
                                       JSValueConst method);
QJS_INTERNAL JSValue JS_GetIterator(JSContext *ctx, JSValueConst obj,
                                      BOOL is_async);
QJS_INTERNAL JSValue JS_IteratorNext2(JSContext *ctx,
                                        JSValueConst iterator,
                                        JSValueConst method, int argc,
                                        JSValueConst *argv, int *done);
/* Note: always return JS_UNDEFINED when *pdone = TRUE. */
static inline JSValue JS_IteratorNext(JSContext *ctx, JSValueConst enum_obj,
                                     JSValueConst method,
                                     int argc, JSValueConst *argv, BOOL *pdone)
{
    JSValue obj, value, done_val;
    int done;

    obj = JS_IteratorNext2(ctx, enum_obj, method, argc, argv, &done);
    if (JS_IsException(obj))
        goto fail;
    if (likely(done == 0)) {
        *pdone = FALSE;
        return obj;
    } else if (done != 2) {
        JS_FreeValue(ctx, obj);
        *pdone = TRUE;
        return JS_UNDEFINED;
    } else {
        done_val = JS_GetProperty(ctx, obj, JS_ATOM_done);
        if (JS_IsException(done_val))
            goto fail;
        *pdone = JS_ToBoolFree(ctx, done_val);
        value = JS_UNDEFINED;
        if (!*pdone) {
            value = JS_GetProperty(ctx, obj, JS_ATOM_value);
        }
        JS_FreeValue(ctx, obj);
        return value;
    }
 fail:
    JS_FreeValue(ctx, obj);
    *pdone = FALSE;
    return JS_EXCEPTION;
}
QJS_INTERNAL int JS_IteratorClose(JSContext *ctx, JSValueConst iterator,
                                    BOOL is_exception_pending);
QJS_INTERNAL JSValue JS_IteratorGetCompleteValue(JSContext *ctx,
                                                     JSValueConst obj,
                                                     BOOL *done);
QJS_INTERNAL JSValue js_create_iterator_result(JSContext *ctx,
                                                JSValue value, BOOL done);
QJS_INTERNAL JSValue js_iterator_proto_iterator(JSContext *ctx,
                                                 JSValueConst this_val,
                                                 int argc,
                                                 JSValueConst *argv);
/* This representation check was directly visible to every monolithic caller.
   Keep it inline across the split so array algorithms do not gain a runtime
   forwarding boundary. */
static inline BOOL js_get_fast_array(JSContext *ctx, JSValueConst obj,
                                      JSValue **values, uint32_t *count)
{
    (void)ctx;
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_OBJECT) {
        JSObject *p = JS_VALUE_GET_OBJ(obj);
        if (p->class_id == JS_CLASS_ARRAY && p->fast_array) {
            *count = p->u.array.count;
            *values = p->u.array.u.values;
            return TRUE;
        }
    }
    return FALSE;
}
QJS_INTERNAL int JS_CopyDataProperties(JSContext *ctx,
                                          JSValueConst target,
                                          JSValueConst source,
                                          JSValueConst excluded,
                                          BOOL set_property);

#endif /* QUICKJS_INTERNAL_ITERATOR_H */
