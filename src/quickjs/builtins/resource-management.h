/*
 * QuickJS explicit resource management builtins
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

#ifndef QUICKJS_BUILTINS_RESOURCE_MANAGEMENT_H
#define QUICKJS_BUILTINS_RESOURCE_MANAGEMENT_H

#include "../internal/base.h"

typedef struct JSDisposableStackData {
    JSValue resources;
    BOOL disposed;
} JSDisposableStackData;

int js_init_disposable_stack(JSContext *ctx);
int js_init_async_disposable_stack(JSContext *ctx);
JSValue js_new_disposable_stack(JSContext *ctx, JSValueConst new_target,
                                int class_id, size_t data_size);
void js_disposable_stack_clear(JSRuntime *rt, JSDisposableStackData *stack);
void js_disposable_stack_mark_data(JSRuntime *rt,
                                   const JSDisposableStackData *stack,
                                   JS_MarkFunc *mark_func);
JSValue js_disposable_stack_get_disposed(JSContext *ctx, JSValueConst value,
                                         int class_id);
JSValue js_disposable_stack_use(JSContext *ctx, JSValueConst value,
                                int argc, JSValueConst *argv, int class_id);
JSValue js_disposable_stack_adopt(JSContext *ctx, JSValueConst value,
                                  int argc, JSValueConst *argv, int class_id);
JSValue js_disposable_stack_defer(JSContext *ctx, JSValueConst value,
                                  int argc, JSValueConst *argv, int class_id);
JSValue js_disposable_stack_move(JSContext *ctx, JSValueConst value,
                                 int class_id, size_t data_size);

#endif
