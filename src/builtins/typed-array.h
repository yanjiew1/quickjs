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
/* Array-buffer/typed-array storage and serialization interfaces. */
#ifndef QUICKJS_BUILTINS_TYPED_ARRAY_H
#define QUICKJS_BUILTINS_TYPED_ARRAY_H

#include "runtime.h"

typedef struct JSArrayBuffer {
    int byte_length; /* 0 if detached */
    int max_byte_length; /* -1 if not resizable; >= byte_length otherwise */
    uint8_t detached;
    uint8_t shared; /* if shared, the array buffer cannot be detached */
    uint8_t *data; /* NULL if detached */
    struct list_head array_list;
    void *opaque;
    JSFreeArrayBufferDataFunc *free_func;
} JSArrayBuffer;

typedef struct JSTypedArray {
    struct list_head link; /* link to arraybuffer */
    JSObject *obj; /* back pointer to the TypedArray/DataView object */
    JSObject *buffer; /* based array buffer */
    uint32_t offset; /* byte offset in the array buffer */
    uint32_t length; /* byte length in the array buffer */
    BOOL track_rab; /* auto-track length of backing array buffer */
} JSTypedArray;


extern JS_INTERNAL uint8_t const typed_array_size_log2[JS_TYPED_ARRAY_COUNT];
#define typed_array_size_log2(classid)  (typed_array_size_log2[(classid)- JS_CLASS_UINT8C_ARRAY])

/* Construct an owned buffer. With alloc_flag=false, adopt buf on success;
   a shared buffer instead retains it through sab_dup when provided. */
JS_INTERNAL JSValue js_array_buffer_constructor3(JSContext *ctx,
                                            JSValueConst new_target,
                                            uint64_t len, uint64_t *max_len,
                                            JSClassID class_id,
                                            uint8_t *buf,
                                            JSFreeArrayBufferDataFunc *free_func,
                                            void *opaque, BOOL alloc_flag);

/* Default ArrayBuffer backing-store finalizer. */
JS_INTERNAL void js_array_buffer_free(JSRuntime *rt, void *opaque, void *ptr);

/* Return borrowed buffer storage, or NULL with an exception. */
JS_INTERNAL JSArrayBuffer *js_get_array_buffer(JSContext *ctx, JSValueConst obj);

/* Construct an owned typed array from borrowed arguments and new_target. */
JS_INTERNAL JSValue js_typed_array_constructor(JSContext *ctx,
                                          JSValueConst new_target,
                                          int argc, JSValueConst *argv,
                                          int classid);

/* Throw the canonical detached-buffer exception. */
JS_INTERNAL JSValue JS_ThrowTypeErrorDetachedArrayBuffer(JSContext *ctx);

#endif /* QUICKJS_BUILTINS_TYPED_ARRAY_H */
