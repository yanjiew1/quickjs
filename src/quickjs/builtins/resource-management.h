/*
 * QuickJS explicit resource management builtins
 *
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */

#ifndef QUICKJS_BUILTINS_RESOURCE_MANAGEMENT_H
#define QUICKJS_BUILTINS_RESOURCE_MANAGEMENT_H

#include "../internal/base.h"

typedef enum {
    JS_DISPOSABLE_USE,
    JS_DISPOSABLE_ADOPT,
    JS_DISPOSABLE_DEFER,
} JSDisposableInvocation;

typedef struct JSDisposableResource {
    struct JSDisposableResource *next;
    JSValue value;
    JSValue method;
    JSDisposableInvocation invocation;
} JSDisposableResource;

typedef struct JSDisposableResourceList {
    JSDisposableResource *head;
    JSDisposableResource *active;
} JSDisposableResourceList;

typedef struct JSDisposableStackData {
    JSValue resources;
    BOOL disposed;
} JSDisposableStackData;

int js_init_disposable_stack(JSContext *ctx);
int js_init_disposable_resource_list(JSContext *ctx);
int js_init_async_disposable_stack(JSContext *ctx);
JSValue js_get_async_dispose_method(JSContext *ctx, JSValueConst value);
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
JSValue js_disposable_resource_call(JSContext *ctx,
                                    const JSDisposableResource *resource);
void js_disposable_resource_free(JSRuntime *rt, JSDisposableResource *resource);
void js_disposable_resource_add_error(JSContext *ctx, JSValue *error,
                                      BOOL *has_error, JSValue next_error);

#endif
