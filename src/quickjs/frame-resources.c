/*
 * QuickJS lexical resource disposal
 *
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
#include "internal/frame-resources.h"

int js_using_add(JSContext *ctx, JSStackFrame *sf, int var_idx,
                 int method_idx, JSDisposalKind kind, JSValueConst value)
{
    JSValue method;

    if (kind == JS_DISPOSAL_SYNC &&
        (JS_IsNull(value) || JS_IsUndefined(value)))
        return 0;
    method = js_disposable_resource_method(ctx, value, kind);
    if (JS_IsException(method))
        return -1;
    /* Publish only after lookup succeeds. Keep an independent resource root:
       the user binding can belong to a module or an eval environment. */
    assert(JS_IsUndefined(sf->dispose_buf[method_idx]));
    JS_FreeValue(ctx, sf->var_buf[var_idx]);
    sf->var_buf[var_idx] = JS_DupValue(ctx, value);
    /* Undefined is inactive; null records an executed nullish await using. */
    sf->dispose_buf[method_idx] = JS_IsUndefined(method) ? JS_NULL : method;
    return 0;
}

void js_frame_disposal_error(JSContext *ctx, JSValue *error, JSValue next_error)
{
    BOOL has_error = !JS_IsUninitialized(*error);

    js_disposable_resource_add_error(ctx, error, &has_error, next_error);
}

int js_dispose_resource(JSContext *ctx, JSStackFrame *sf, int var_idx,
                        int method_idx, JSDisposalKind kind, JSValue *error,
                        JSValue *await_value)
{
    JSValue method = sf->dispose_buf[method_idx];
    JSValue value, result;

    *await_value = JS_UNDEFINED;
    if (JS_IsUndefined(method))
        return 0;
    if (kind == JS_DISPOSAL_SYNC &&
        (sf->dispose_state & JS_DISPOSE_NEEDS_AWAIT) &&
        !(sf->dispose_state & JS_DISPOSE_HAS_AWAITED)) {
        sf->dispose_state &= ~JS_DISPOSE_NEEDS_AWAIT;
        sf->dispose_state |= JS_DISPOSE_WAIT_BEFORE_SYNC;
        return 1;
    }
    value = sf->var_buf[var_idx];
    sf->var_buf[var_idx] = JS_UNDEFINED;
    sf->dispose_buf[method_idx] = JS_UNDEFINED;
    /* Detach before calling user code or suspending. Repeated exit markers
       and resumption of this opcode cannot dispose the record twice. */
    if (JS_IsNull(method)) {
        assert(kind == JS_DISPOSAL_ASYNC);
        sf->dispose_state |= JS_DISPOSE_NEEDS_AWAIT;
        JS_FreeValue(ctx, value);
        return 0;
    }
    result = JS_Call(ctx, method, value, 0, NULL);
    JS_FreeValue(ctx, method);
    JS_FreeValue(ctx, value);
    if (JS_IsException(result)) {
        js_frame_disposal_error(ctx, error, JS_GetException(ctx));
    } else if (kind == JS_DISPOSAL_SYNC) {
        JS_FreeValue(ctx, result);
    } else {
        sf->dispose_state |= JS_DISPOSE_WAIT_METHOD;
        *await_value = result;
        return 1;
    }
    return 0;
}
