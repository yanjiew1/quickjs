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

#include "base.h"

typedef enum JSIteratorKindEnum {
    JS_ITERATOR_KIND_KEY,
    JS_ITERATOR_KIND_VALUE,
    JS_ITERATOR_KIND_KEY_AND_VALUE,
} JSIteratorKindEnum;

JSValue JS_GetIterator2(JSContext *ctx, JSValueConst obj, JSValueConst method);
JSValue JS_GetIterator(JSContext *ctx, JSValueConst obj, BOOL is_async);
JSValue JS_IteratorNext2(JSContext *ctx, JSValueConst enum_obj,
                         JSValueConst method, int argc, JSValueConst *argv,
                         int *pdone);
JSValue JS_IteratorNext(JSContext *ctx, JSValueConst enum_obj,
                        JSValueConst method, int argc, JSValueConst *argv,
                        BOOL *pdone);
int JS_IteratorClose(JSContext *ctx, JSValueConst enum_obj,
                     BOOL is_exception_pending);
JSValue JS_IteratorGetCompleteValue(JSContext *ctx, JSValueConst obj,
                                    BOOL *pdone);
JSValue js_create_iterator_result(JSContext *ctx, JSValue val, BOOL done);

#endif
