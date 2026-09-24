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

#include "../internal/object.h"

int JS_AddIntrinsicArrayBuffers(JSContext *ctx);
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

#endif
