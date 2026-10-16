/*
 * QuickJS shared disposal operations
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

/* Return an owned method, undefined for nullish resources, or an exception. */
JSValue js_disposable_resource_method(JSContext *ctx, JSValueConst value,
                                      JSDisposalKind kind);
/* Consume next_error and update the owned accumulated completion. */
void js_disposable_resource_add_error(JSContext *ctx, JSValue *error,
                                      BOOL *has_error, JSValue next_error);

void js_async_dispose_settle(JSContext *ctx, JSValueConst *funcs,
                            JSValueConst value, BOOL reject);

#endif
