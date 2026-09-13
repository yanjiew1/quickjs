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

#define QJS_TYPED_HINT_NONE 2

static inline int qjs_typed_bigint_sign(const JSBigInt *value)
{
    return (js_slimb_t)value->tab[value->len - 1] < 0;
}

QJS_INTERNAL void qjs_array_buffer_finalizer(JSRuntime *rt, JSValue value);
QJS_INTERNAL void qjs_typed_array_finalizer(JSRuntime *rt, JSValue value);
QJS_INTERNAL void qjs_typed_array_mark(JSRuntime *rt, JSValueConst value,
                                       JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue qjs_typed_throw_invalid_class(JSContext *ctx,
                                                   int class_id);
QJS_INTERNAL JSValue qjs_typed_to_primitive(JSContext *ctx,
                                            JSValueConst value, int hint);
QJS_INTERNAL int qjs_to_uint8_clamp_free(JSContext *ctx, int32_t *result,
                                         JSValue value);
QJS_INTERNAL JSValue qjs_to_bigint_free(JSContext *ctx, JSValue value);
QJS_INTERNAL JSValue qjs_typed_create_from_ctor(JSContext *ctx,
                                                JSValueConst ctor,
                                                JSClassID class_id);
QJS_INTERNAL JSValue qjs_typed_species_constructor(JSContext *ctx,
                                                   JSValueConst obj,
                                                   JSValueConst default_ctor);

#endif /* QUICKJS_INTERNAL_TYPED_ARRAY_H */
