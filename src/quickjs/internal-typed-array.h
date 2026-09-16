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
#ifndef QUICKJS_INTERNAL_TYPED_ARRAY_H
#define QUICKJS_INTERNAL_TYPED_ARRAY_H

#include "internal-array.h"
#include "internal-builtin.h"

QJS_INTERNAL void js_array_buffer_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_typed_array_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_typed_array_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue JS_ThrowTypeErrorInvalidClass(JSContext *ctx, int class_id);
QJS_INTERNAL JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst val, int hint);
QJS_INTERNAL int JS_ToUint8ClampFree(JSContext *ctx, int32_t *pres, JSValue val);
QJS_INTERNAL JSValue JS_ToBigIntFree(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                                   int class_id);
QJS_INTERNAL JSValue JS_SpeciesConstructor(JSContext *ctx, JSValueConst obj,
                                     JSValueConst defaultConstructor);

#endif /* QUICKJS_INTERNAL_TYPED_ARRAY_H */
