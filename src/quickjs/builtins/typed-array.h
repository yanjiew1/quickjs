/*
 * QuickJS typed array and ArrayBuffer builtin interface
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
#ifndef QUICKJS_BUILTINS_TYPED_ARRAY_H
#define QUICKJS_BUILTINS_TYPED_ARRAY_H

#include "../internal/object.h"

struct JSTypedArray {
    struct list_head link; /* link to arraybuffer */
    JSObject *obj; /* back pointer to the TypedArray/DataView object */
    JSObject *buffer; /* based array buffer */
    uint32_t offset; /* byte offset in the array buffer */
    uint32_t length; /* byte length in the array buffer */
    BOOL track_rab; /* auto-track length of backing array buffer */
};

extern uint8_t const typed_array_size_log2[JS_TYPED_ARRAY_COUNT];
#define typed_array_size_log2(classid)  (typed_array_size_log2[(classid)- JS_CLASS_UINT8C_ARRAY])

JSValue js_typed_array_constructor(JSContext *ctx, JSValueConst new_target,
                                   int argc, JSValueConst *argv, int classid);

void js_typed_array_finalizer(JSRuntime *rt, JSValue val);
void js_typed_array_mark(JSRuntime *rt, JSValueConst val,
                         JS_MarkFunc *mark_func);
int js_typed_array_get_length_unsafe(JSContext *ctx, JSValueConst obj);
JSValue js_typed_array___speciesCreate(JSContext *ctx,
                                       JSValueConst this_val,
                                       int argc, JSValueConst *argv);
BOOL typed_array_is_oob(JSObject *p);

#endif
