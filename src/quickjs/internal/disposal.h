/*
 * QuickJS disposable resource traversal
 *
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
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
#ifndef QUICKJS_DISPOSAL_H
#define QUICKJS_DISPOSAL_H

#include "base.h"

typedef enum {
    JS_DISPOSAL_SYNC,
    JS_DISPOSAL_ASYNC,
} JSDisposalKind;

JSValue js_new_disposable_resource_list(JSContext *ctx);
int js_add_disposable_resource(JSContext *ctx, JSValueConst resources,
                               JSValueConst value, JSDisposalKind kind);
/* First input seeds the completion; subsequent input resumes Await.
   Return 0 for normal completion, 1 for an owned Await operand, and
   -1 with the final throw completion in the context exception slot. */
int js_dispose_resources_step(JSContext *ctx, JSValueConst resources,
                              JSValueConst input, BOOL is_throw,
                              JSValue *await_value);

/* Consume method even when inserting the resource fails. */
int js_adopt_disposable_resource(JSContext *ctx, JSValueConst resources,
                                 JSValueConst value, JSValue method,
                                 JSDisposalKind kind);
int js_defer_disposable_resource(JSContext *ctx, JSValueConst resources,
                                 JSValue method, JSDisposalKind kind);

void js_async_dispose_settle(JSContext *ctx, JSValueConst *funcs,
                            JSValueConst value, BOOL reject);

#endif
