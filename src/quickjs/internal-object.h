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
#ifndef QUICKJS_INTERNAL_OBJECT_H
#define QUICKJS_INTERNAL_OBJECT_H

#include "internal-number.h"

QJS_INTERNAL int JS_SetObjectData(JSContext *ctx, JSValueConst obj, JSValue val);
QJS_INTERNAL JSValue JS_ToObject(JSContext *ctx, JSValueConst val);
QJS_INTERNAL JSValue JS_ToObjectFree(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue JS_ToStringFree(JSContext *ctx, JSValue val);
QJS_INTERNAL __exception int js_get_length32(JSContext *ctx, uint32_t *pres,
                                       JSValueConst obj);
QJS_INTERNAL __exception int js_get_length64(JSContext *ctx, int64_t *pres,
                                       JSValueConst obj);
QJS_INTERNAL void free_var_ref(JSRuntime *rt, JSVarRef *var_ref);

typedef struct JSArrayIteratorData {
    JSValue obj;
    JSIteratorKindEnum kind;
    uint32_t idx;
} JSArrayIteratorData;

#endif /* QUICKJS_INTERNAL_OBJECT_H */
