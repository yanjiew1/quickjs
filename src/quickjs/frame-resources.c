/*
 * QuickJS lexical frame resources
 *
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */

#include "internal/frame-resources.h"
#include "internal/disposal.h"

/* State belongs to a scope activation; resource ordering belongs to the
   compiler. No runtime resource list or storage callbacks are needed. */
enum {
    JS_DISPOSAL_FRAME_STARTED = 1 << 0,
    JS_DISPOSAL_FRAME_DONE = 1 << 1,
    JS_DISPOSAL_FRAME_HAS_ERROR = 1 << 2,
    JS_DISPOSAL_FRAME_NEEDS_AWAIT = 1 << 3,
    JS_DISPOSAL_FRAME_HAS_AWAITED = 1 << 4,
    JS_DISPOSAL_FRAME_AWAIT_METHOD = 1 << 5,
    JS_DISPOSAL_FRAME_AWAIT_EMPTY = 1 << 6,
};

int js_init_resource_loc(JSContext *ctx, JSValue *record,
                         JSValueConst value, JSDisposalKind kind)
{
    JSValue method;

    if (kind == JS_DISPOSAL_SYNC &&
        (JS_IsNull(value) || JS_IsUndefined(value)))
        return 0;
    method = js_disposable_resource_method(ctx, value, kind);
    if (JS_IsException(method))
        return -1;
    /* A failed lookup must leave this declaration inactive. Null distinguishes
       a registered nullish async resource from an unexecuted declaration. */
    assert(JS_IsUndefined(record[JS_DISPOSAL_RECORD_VALUE]));
    assert(JS_IsUndefined(record[JS_DISPOSAL_RECORD_METHOD]));
    record[JS_DISPOSAL_RECORD_VALUE] = JS_IsUndefined(method) ?
        JS_UNDEFINED : JS_DupValue(ctx, value);
    record[JS_DISPOSAL_RECORD_METHOD] = JS_IsUndefined(method) ? JS_NULL : method;
    return 0;
}

static int js_resume_frame_disposal(JSContext *ctx, JSValue *state,
                                    JSValueConst input, BOOL is_throw)
{
    int flags = JS_VALUE_GET_INT(state[JS_DISPOSAL_FRAME_FLAGS]);
    BOOL has_error = (flags & JS_DISPOSAL_FRAME_HAS_ERROR) != 0;
    JSValue *error = state + JS_DISPOSAL_FRAME_ERROR;

    if (flags & JS_DISPOSAL_FRAME_DONE)
        return flags;
    if (!(flags & JS_DISPOSAL_FRAME_STARTED)) {
        flags |= JS_DISPOSAL_FRAME_STARTED;
        if (is_throw) {
            *error = JS_DupValue(ctx, input);
            has_error = TRUE;
        }
    } else if (flags & (JS_DISPOSAL_FRAME_AWAIT_METHOD |
                        JS_DISPOSAL_FRAME_AWAIT_EMPTY)) {
        if (flags & JS_DISPOSAL_FRAME_AWAIT_METHOD)
            flags |= JS_DISPOSAL_FRAME_HAS_AWAITED;
        flags &= ~(JS_DISPOSAL_FRAME_AWAIT_METHOD | JS_DISPOSAL_FRAME_AWAIT_EMPTY);
        if (is_throw)
            js_disposable_resource_add_error(ctx, error, &has_error,
                                             JS_DupValue(ctx, input));
    }
    if (has_error)
        flags |= JS_DISPOSAL_FRAME_HAS_ERROR;
    state[JS_DISPOSAL_FRAME_FLAGS] = JS_NewInt32(ctx, flags);
    return flags;
}

int js_dispose_resource_loc(JSContext *ctx, JSValue *state, JSValue *record,
                            JSDisposalKind kind, JSValueConst input,
                            BOOL is_throw, JSValue *await_value)
{
    int flags = js_resume_frame_disposal(ctx, state, input, is_throw);
    BOOL has_error = (flags & JS_DISPOSAL_FRAME_HAS_ERROR) != 0;
    JSValue method, value, result;

    *await_value = JS_UNDEFINED;
    /* Consume a resumed rejection before inspecting the cleared method. An
       inactive sync declaration must not introduce a synthetic Await. */
    if ((flags & JS_DISPOSAL_FRAME_DONE) ||
        JS_IsUndefined(record[JS_DISPOSAL_RECORD_METHOD]))
        return 0;
    if (kind == JS_DISPOSAL_SYNC &&
        (flags & JS_DISPOSAL_FRAME_NEEDS_AWAIT) &&
        !(flags & JS_DISPOSAL_FRAME_HAS_AWAITED)) {
        flags &= ~JS_DISPOSAL_FRAME_NEEDS_AWAIT;
        flags |= JS_DISPOSAL_FRAME_AWAIT_EMPTY;
        goto await;
    }
    value = record[JS_DISPOSAL_RECORD_VALUE];
    method = record[JS_DISPOSAL_RECORD_METHOD];
    record[JS_DISPOSAL_RECORD_VALUE] = JS_UNDEFINED;
    record[JS_DISPOSAL_RECORD_METHOD] = JS_UNDEFINED;
    if (JS_IsNull(method)) {
        assert(kind == JS_DISPOSAL_ASYNC);
        flags |= JS_DISPOSAL_FRAME_NEEDS_AWAIT;
        JS_FreeValue(ctx, value);
    } else {
        /* Detach ownership before calling user code, so a retry after Await
           or another exit through this cleanup cannot call the method twice. */
        result = JS_Call(ctx, method, value, 0, NULL);
        JS_FreeValue(ctx, method);
        JS_FreeValue(ctx, value);
        if (JS_IsException(result)) {
            js_disposable_resource_add_error(ctx,
                state + JS_DISPOSAL_FRAME_ERROR, &has_error,
                JS_GetException(ctx));
        } else if (kind == JS_DISPOSAL_SYNC) {
            JS_FreeValue(ctx, result);
        } else {
            flags |= JS_DISPOSAL_FRAME_AWAIT_METHOD;
            *await_value = result;
            goto await;
        }
    }
    if (has_error)
        flags |= JS_DISPOSAL_FRAME_HAS_ERROR;
    state[JS_DISPOSAL_FRAME_FLAGS] = JS_NewInt32(ctx, flags);
    return 0;

 await:
    state[JS_DISPOSAL_FRAME_FLAGS] = JS_NewInt32(ctx, flags);
    return 1;
}

int js_finish_resource_scope(JSContext *ctx, JSValue *state,
                             JSValueConst input, BOOL is_throw,
                             JSValue *await_value)
{
    int flags = js_resume_frame_disposal(ctx, state, input, is_throw);

    *await_value = JS_UNDEFINED;
    if (flags & JS_DISPOSAL_FRAME_DONE)
        return 0;
    if ((flags & JS_DISPOSAL_FRAME_NEEDS_AWAIT) &&
        !(flags & JS_DISPOSAL_FRAME_HAS_AWAITED)) {
        flags &= ~JS_DISPOSAL_FRAME_NEEDS_AWAIT;
        flags |= JS_DISPOSAL_FRAME_AWAIT_EMPTY;
        state[JS_DISPOSAL_FRAME_FLAGS] = JS_NewInt32(ctx, flags);
        return 1;
    }
    state[JS_DISPOSAL_FRAME_FLAGS] = JS_NewInt32(ctx, JS_DISPOSAL_FRAME_DONE);
    if (flags & JS_DISPOSAL_FRAME_HAS_ERROR) {
        JSValue error = state[JS_DISPOSAL_FRAME_ERROR];
        state[JS_DISPOSAL_FRAME_ERROR] = JS_UNDEFINED;
        JS_Throw(ctx, error);
        return -1;
    }
    return 0;
}
