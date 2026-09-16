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
#include "internal-array-algorithm.h"

QJS_INTERNAL BOOL can_extend_fast_array(JSObject *p);

QJS_INTERNAL BOOL array_buffer_is_resizable(const JSArrayBuffer *abuf);

QJS_INTERNAL JSValue js_allocate_fast_array(JSContext *ctx, int64_t len);
QJS_INTERNAL int JS_TryGetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, JSValue *pval);
QJS_INTERNAL int expand_fast_array(JSContext *ctx, JSObject *p, uint32_t new_len);
QJS_INTERNAL JSValue js_create_array(JSContext *ctx, int len, JSValueConst *tab);
QJS_INTERNAL JSValue js_object_toString(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv);
QJS_INTERNAL JSValue js_array_push(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int unshift);
QJS_INTERNAL JSValue js_array_iterator_next(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv,
                                      BOOL *pdone, int magic);
QJS_INTERNAL JSValue js_create_array_iterator(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv, int magic);

/* Neutral TypedArray interoperability used by generic Array algorithms. */
QJS_INTERNAL BOOL typed_array_is_oob(JSObject *p);
QJS_INTERNAL int js_typed_array_get_length_unsafe(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL JSValue js_typed_array___speciesCreate(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv);
QJS_INTERNAL JSValue JS_ThrowTypeErrorArrayBufferOOB(JSContext *ctx);
QJS_INTERNAL void js_array_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_array_mark(JSRuntime *rt, JSValueConst val,
                          JS_MarkFunc *mark_func);
QJS_INTERNAL void js_array_iterator_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_array_iterator_mark(JSRuntime *rt, JSValueConst val,
                                   JS_MarkFunc *mark_func);
QJS_INTERNAL void js_iterator_concat_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_iterator_concat_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);
QJS_INTERNAL void js_iterator_helper_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_iterator_helper_mark(JSRuntime *rt, JSValueConst val,
                                   JS_MarkFunc *mark_func);
QJS_INTERNAL void js_iterator_wrap_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_iterator_wrap_mark(JSRuntime *rt, JSValueConst val,
                                  JS_MarkFunc *mark_func);

QJS_INTERNAL JSValue JS_ThrowTypeErrorDetachedArrayBuffer(JSContext *ctx);
QJS_INTERNAL JSValue js_array_buffer_constructor3(JSContext *ctx,
                                            JSValueConst new_target,
                                            uint64_t len, uint64_t *max_len,
                                            JSClassID class_id,
                                            uint8_t *buf,
                                            JSFreeArrayBufferDataFunc *free_func,
                                            void *opaque, BOOL alloc_flag);
QJS_INTERNAL void js_array_buffer_free(JSRuntime *rt, void *opaque, void *ptr);
QJS_INTERNAL JSArrayBuffer *js_get_array_buffer(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL JSValue js_typed_array_constructor(JSContext *ctx,
                                          JSValueConst new_target,
                                          int argc, JSValueConst *argv,
                                          int classid);

#endif /* QUICKJS_INTERNAL_ARRAY_H */
