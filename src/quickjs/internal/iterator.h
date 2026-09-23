/*
 * QuickJS Iterator Internal Interface
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
#ifndef QJS_ITERATOR_H
#define QJS_ITERATOR_H

#include "base.h"

typedef enum JSIteratorKindEnum {
    JS_ITERATOR_KIND_KEY,
    JS_ITERATOR_KIND_VALUE,
    JS_ITERATOR_KIND_KEY_AND_VALUE,
} JSIteratorKindEnum;

typedef struct JSArrayIteratorData {
    JSValue obj;
    JSIteratorKindEnum kind;
    uint32_t idx;
} JSArrayIteratorData;

/* XXX: use enum */
#define GEN_MAGIC_NEXT   0
#define GEN_MAGIC_RETURN 1
#define GEN_MAGIC_THROW  2


QJS_INTERNAL JSValue JS_GetIterator(JSContext *ctx, JSValueConst obj, BOOL is_async);
QJS_INTERNAL JSValue JS_IteratorNext(JSContext *ctx, JSValueConst enum_obj,
                                     JSValueConst method, int argc,
                                     JSValueConst *argv, BOOL *pdone);
QJS_INTERNAL int JS_IteratorClose(JSContext *ctx, JSValueConst enum_obj,
                                  BOOL is_exception_pending);
QJS_INTERNAL JSValue js_create_array_iterator(JSContext *ctx,
                                               JSValueConst this_val,
                                               int argc, JSValueConst *argv,
                                               int magic);

QJS_INTERNAL JSValue js_iterator_proto_iterator(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
QJS_INTERNAL JSValue js_create_iterator_result(JSContext *ctx, JSValue val, BOOL done);
QJS_INTERNAL JSValue JS_IteratorNext2(JSContext *ctx, JSValueConst enum_obj, JSValueConst method, int argc, JSValueConst *argv, int *pdone);
QJS_INTERNAL JSValue JS_IteratorGetCompleteValue(JSContext *ctx, JSValueConst obj, BOOL *pdone);

#endif /* QJS_ITERATOR_H */
