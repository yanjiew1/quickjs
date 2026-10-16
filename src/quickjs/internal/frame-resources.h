/*
 * QuickJS lexical resource disposal
 *
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
#ifndef QUICKJS_FRAME_RESOURCES_H
#define QUICKJS_FRAME_RESOURCES_H

#include "runtime.h"
#include "disposal.h"

enum {
    JS_DISPOSE_NEEDS_AWAIT = 1 << 0,
    JS_DISPOSE_HAS_AWAITED = 1 << 1,
    JS_DISPOSE_WAIT_METHOD = 1 << 2,
    JS_DISPOSE_WAIT_BEFORE_SYNC = 1 << 3,
    JS_DISPOSE_WAIT_END = 1 << 4,
    JS_DISPOSE_WAIT_MASK = JS_DISPOSE_WAIT_METHOD |
        JS_DISPOSE_WAIT_BEFORE_SYNC | JS_DISPOSE_WAIT_END,
};

int js_using_add(JSContext *ctx, JSStackFrame *sf, int var_idx,
                 int method_idx, JSDisposalKind kind, JSValueConst value);
/* Consume next_error, combining it with the owned pending completion. */
void js_frame_disposal_error(JSContext *ctx, JSValue *error, JSValue next_error);
/* Return 1 with an owned Await operand, or 0 after advancing the record. */
int js_dispose_resource(JSContext *ctx, JSStackFrame *sf, int var_idx,
                        int method_idx, JSDisposalKind kind, JSValue *error,
                        JSValue *await_value);

#endif
