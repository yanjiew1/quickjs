/* Native Promise integration contracts.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "quickjs.h"

static JSValue eval(JSContext *ctx, const char *source)
{
    JSValue result = JS_Eval(ctx, source, strlen(source), "<promise-api>",
                             JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(result));
    return result;
}

static void drain_jobs(JSRuntime *rt)
{
    int remaining = 1000;

    while (JS_IsJobPending(rt)) {
        assert(remaining-- > 0);
        assert(JS_ExecutePendingJob(rt, NULL) > 0);
    }
}

typedef struct TrackerState {
    JSAtom then_atom;
    JSValue handler;
    int rejected, handled, reentered;
} TrackerState;

static void rejection_tracker(JSContext *ctx, JSValueConst promise,
                              JSValueConst reason, JS_BOOL handled, void *opaque)
{
    TrackerState *state = opaque;

    if (!handled) {
        state->rejected++;
    } else {
        state->handled++;
        if (!state->reentered) {
            JSValueConst args[2] = { JS_UNDEFINED, state->handler };
            JSValue nested;

            state->reentered = 1;
            nested = JS_Invoke(ctx, promise, state->then_atom, 2, args);
            assert(!JS_IsException(nested));
            JS_FreeValue(ctx, nested);
        }
    }
}

static void test_reentrant_rejection_tracker(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    TrackerState state = { 0 };
    JSValue source, result;
    JSValueConst args[2];

    state.then_atom = JS_NewAtom(ctx, "then");
    assert(state.then_atom != JS_ATOM_NULL);
    state.handler = eval(ctx, "(reason) => undefined");
    JS_SetHostPromiseRejectionTracker(rt, rejection_tracker, &state);
    source = eval(ctx, "Promise.reject(42)");
    assert(state.rejected == 1);
    args[0] = JS_UNDEFINED;
    args[1] = state.handler;
    result = JS_Invoke(ctx, source, state.then_atom, 2, args);
    assert(!JS_IsException(result));
    assert(state.handled == 1 && state.reentered == 1);
    drain_jobs(rt);
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_FULFILLED);
    assert(state.rejected == 1 && state.handled == 1);
    JS_SetHostPromiseRejectionTracker(rt, NULL, NULL);
    JS_FreeValue(ctx, source);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, state.handler);
    JS_FreeAtom(ctx, state.then_atom);
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    test_reentrant_rejection_tracker(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
