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
#ifndef QUICKJS_INTERNAL_ARRAY_H
#define QUICKJS_INTERNAL_ARRAY_H

#include "internal-module.h"

static force_inline BOOL qjs_can_extend_fast_array(JSObject *obj)
{
    JSObject *proto;
    if (!obj->extensible)
        return FALSE;
    proto = obj->shape->proto;
    if (!proto)
        return TRUE;
    return proto->is_std_array_prototype;
}

QJS_INTERNAL JSValue qjs_allocate_fast_array(JSContext *ctx, int64_t len);
QJS_INTERNAL int qjs_try_get_property_int64(JSContext *ctx, JSValueConst obj,
                                            int64_t index, JSValue *value);
QJS_INTERNAL int qjs_expand_fast_array(JSContext *ctx, JSObject *obj,
                                       uint32_t new_len);
QJS_INTERNAL JSValue qjs_create_array(JSContext *ctx, int len,
                                      JSValueConst *values);
QJS_INTERNAL int qjs_set_property_value(JSContext *ctx, JSValueConst obj,
                                        JSValue property, JSValue value,
                                        int flags);
QJS_INTERNAL JSValue qjs_primitive_create_array_iterator(
    JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv,
    int magic);

/* Neutral TypedArray interoperability used by generic Array algorithms. */
QJS_INTERNAL BOOL qjs_typed_array_is_oob(JSObject *obj);
QJS_INTERNAL int qjs_typed_array_get_length_unsafe(JSContext *ctx,
                                                   JSValueConst obj);
QJS_INTERNAL JSValue qjs_typed_array_species_create(
    JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
QJS_INTERNAL JSValue qjs_throw_array_buffer_oob(JSContext *ctx);

QJS_INTERNAL JSValue qjs_throw_detached_array_buffer(JSContext *ctx);
QJS_INTERNAL JSValue qjs_array_buffer_constructor(
    JSContext *ctx, JSValueConst new_target, uint64_t len, uint64_t *max_len,
    JSClassID class_id, uint8_t *buf,
    JSFreeArrayBufferDataFunc *free_func, void *opaque, BOOL alloc_flag);
QJS_INTERNAL void qjs_array_buffer_free(JSRuntime *rt, void *opaque, void *ptr);
QJS_INTERNAL JSArrayBuffer *qjs_get_array_buffer(JSContext *ctx,
                                                JSValueConst obj);
QJS_INTERNAL JSValue qjs_typed_array_constructor(JSContext *ctx,
                                                JSValueConst new_target,
                                                int argc,
                                                JSValueConst *argv,
                                                int class_id);

#endif /* QUICKJS_INTERNAL_ARRAY_H */
