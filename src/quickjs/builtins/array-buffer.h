/*
 * QuickJS ArrayBuffer and SharedArrayBuffer builtin interface
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
#ifndef QUICKJS_BUILTINS_ARRAY_BUFFER_H
#define QUICKJS_BUILTINS_ARRAY_BUFFER_H

#include "../internal/base.h"

#ifdef CONFIG_ATOMICS
typedef _Atomic(uint32_t) JSSharedArrayBufferLength;
#else
typedef uint32_t JSSharedArrayBufferLength;
#endif

typedef struct JSArrayBuffer {
    union {
        int byte_length; /* ordinary or fixed buffer; 0 if detached */
        JSSharedArrayBufferLength *shared_length; /* growable shared buffer */
    };
    int max_byte_length; /* -1 if fixed; reserved payload capacity otherwise */
    uint8_t detached;
    uint8_t shared; /* if shared, the array buffer cannot be detached */
    uint8_t uses_shared_callbacks; /* backing owns one shared callback reference */
    uint8_t *data; /* NULL if detached */
    struct list_head array_list;
    void *opaque;
    union {
        JSFreeArrayBufferDataFunc *free_func;
        void (*shared_free_func)(void *opaque, void *ptr);
    };
} JSArrayBuffer;

static inline size_t js_shared_array_buffer_allocation_size(size_t maximum)
{
    size_t payload = maximum ? maximum : 1;
    return payload + _Alignof(JSSharedArrayBufferLength) - 1 +
        sizeof(JSSharedArrayBufferLength);
}

/* Growable shared lengths belong to the backing allocation, not a runtime. */
static inline uint32_t js_array_buffer_byte_length(const JSArrayBuffer *abuf)
{
    if (abuf->shared && abuf->max_byte_length >= 0) {
#ifdef CONFIG_ATOMICS
        return atomic_load_explicit(abuf->shared_length, memory_order_seq_cst);
#else
        return *abuf->shared_length;
#endif
    }
    return abuf->byte_length;
}

JSValue js_clone_shared_array_buffer(JSContext *ctx, uint32_t len,
                                     uint64_t *max_len, uint8_t *data);

JSValue js_array_buffer_constructor3(JSContext *ctx, JSValueConst new_target,
                                     uint64_t len, uint64_t *max_len,
                                     JSClassID class_id, uint8_t *buf,
                                     JSFreeArrayBufferDataFunc *free_func,
                                     void *opaque, BOOL alloc_flag);
JSValue js_array_buffer_constructor1(JSContext *ctx, JSValueConst new_target,
                                     uint64_t len, uint64_t *max_len);
void js_array_buffer_free(JSRuntime *rt, void *opaque, void *ptr);
void js_array_buffer_finalizer(JSRuntime *rt, JSValue val);
JSArrayBuffer *js_get_array_buffer(JSContext *ctx, JSValueConst obj);
BOOL array_buffer_is_resizable(const JSArrayBuffer *abuf);
JSValue JS_ThrowTypeErrorDetachedArrayBuffer(JSContext *ctx);
JSValue JS_ThrowTypeErrorArrayBufferOOB(JSContext *ctx);

extern const JSCFunctionListEntry js_array_buffer_funcs[2];
extern const JSCFunctionListEntry js_array_buffer_proto_funcs[9];
extern const JSCFunctionListEntry js_shared_array_buffer_funcs[1];
extern const JSCFunctionListEntry js_shared_array_buffer_proto_funcs[6];
JSValue js_array_buffer_constructor(JSContext *ctx,
                                           JSValueConst new_target,
                                           int argc, JSValueConst *argv);
JSValue js_shared_array_buffer_constructor(JSContext *ctx,
                                                  JSValueConst new_target,
                                                  int argc, JSValueConst *argv);

#endif
