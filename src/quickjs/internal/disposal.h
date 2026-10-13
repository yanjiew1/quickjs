/*
 * QuickJS disposable resource traversal
 *
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
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
