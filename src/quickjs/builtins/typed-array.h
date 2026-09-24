/*
 * QuickJS internal typed-array builtin interfaces
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
#ifndef QUICKJS_PRIVATE_BUILTIN_TYPED_ARRAY_H
#define QUICKJS_PRIVATE_BUILTIN_TYPED_ARRAY_H

/* Internal implementation details; not part of the public QuickJS API. */
JSArrayBuffer *js_get_array_buffer(JSContext *ctx, JSValueConst obj);
JSValue js_typed_array_constructor(JSContext *ctx,
                                   JSValueConst new_target,
                                   int argc, JSValueConst *argv,
                                   int classid);
JSValue js_array_buffer_constructor3(JSContext *ctx,
                                     JSValueConst new_target,
                                     uint64_t len, uint64_t *max_len,
                                     JSClassID class_id,
                                     uint8_t *buf,
                                     JSFreeArrayBufferDataFunc *free_func,
                                     void *opaque, BOOL alloc_flag);
void js_array_buffer_free(JSRuntime *rt, void *opaque, void *ptr);
JSValue JS_ThrowTypeErrorDetachedArrayBuffer(JSContext *ctx);

/* Internal implementation detail; not part of the public QuickJS API. */
extern const uint8_t typed_array_size_log2[JS_TYPED_ARRAY_COUNT];
#define typed_array_size_log2(classid) (typed_array_size_log2[(classid) - JS_CLASS_UINT8C_ARRAY])

/* Internal implementation detail; not part of the public QuickJS API. */
BOOL array_buffer_is_resizable(const JSArrayBuffer *abuf);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_array_buffer_finalizer(JSRuntime *rt, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_typed_array_finalizer(JSRuntime *rt, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_typed_array_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);

/* Internal implementation detail; not part of the public QuickJS API. */
int js_typed_array_get_length_unsafe(JSContext *ctx, JSValueConst obj);

/* Internal implementation detail; not part of the public QuickJS API. */
BOOL typed_array_is_oob(JSObject *p);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_typed_array___speciesCreate(JSContext *ctx,
                                       JSValueConst this_val,
                                       int argc, JSValueConst *argv);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_ThrowTypeErrorArrayBufferOOB(JSContext *ctx);

#endif /* QUICKJS_PRIVATE_BUILTIN_TYPED_ARRAY_H */
