/* Preserve embedder cancellation across std.evalScript calls.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#include "../src/quickjs/internal/runtime.h"
#include "../src/quickjs-libc/std.c"

typedef struct {
    int calls, cancel, sigint;
} InterruptState;

static int host_interrupt(JSRuntime *rt, void *opaque)
{
    InterruptState *state = opaque;
    assert(state);
    state->calls++;
    if (state->sigint) {
        state->sigint = 0;
        os_pending_signals |= (uint64_t)1 << SIGINT;
    }
    return state->cancel;
}

static JSValue evaluate(JSContext *ctx, const char *source)
{
    ctx->interrupt_counter = 100;
    return JS_Eval(ctx, source, strlen(source), "std-interrupt", JS_EVAL_TYPE_GLOBAL);
}

static void check_restored(JSRuntime *rt, InterruptState *state)
{
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    assert(rt->interrupt_handler == host_interrupt);
    assert(rt->interrupt_opaque == state);
    assert(ts->eval_script_recurse == 0);
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue global, func, value, error;
    InterruptState state = { 0 };

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    js_std_init_handlers(rt);
    global = JS_GetGlobalObject(ctx);
    func = JS_NewCFunction(ctx, js_evalScript, "evalScript", 1);
    assert(!JS_IsException(func));
    assert(JS_SetPropertyStr(ctx, global, "evalScript", func) >= 0);
    JS_SetInterruptHandler(rt, host_interrupt, &state);
    value = evaluate(ctx, "evalScript(\"evalScript('for(var i=0;i<100000;i++); 41')\")");
    assert(!JS_IsException(value));
    assert(JS_StrictEq(ctx, value, JS_NewInt32(ctx, 41)));
    JS_FreeValue(ctx, value);
    assert(state.calls > 0);
    check_restored(rt, &state);

    value = evaluate(ctx, "evalScript('throw 19')");
    assert(JS_IsException(value));
    error = JS_GetException(ctx);
    assert(JS_StrictEq(ctx, error, JS_NewInt32(ctx, 19)));
    JS_FreeValue(ctx, error);
    check_restored(rt, &state);

    state.cancel = 1;
    value = evaluate(ctx, "globalThis.caught = false; try { evalScript('for(var i=0;i<100000;i++);') } catch(e) { caught = true; }");
    assert(JS_IsException(value));
    assert(rt->current_exception_is_uncatchable);
    error = JS_GetException(ctx);
    JS_FreeValue(ctx, error);
    JS_SetUncatchableException(ctx, FALSE);
    value = JS_GetPropertyStr(ctx, global, "caught");
    assert(JS_IsBool(value) && !JS_ToBool(ctx, value));
    JS_FreeValue(ctx, value);
    check_restored(rt, &state);

    state.cancel = 0;
    state.sigint = 1;
    value = evaluate(ctx, "try { evalScript('for(var i=0;i<100000;i++);') } catch(e) { caught = true; }");
    assert(!JS_IsException(value));
    JS_FreeValue(ctx, value);
    value = JS_GetPropertyStr(ctx, global, "caught");
    assert(JS_IsBool(value) && JS_ToBool(ctx, value));
    JS_FreeValue(ctx, value);
    assert(!(os_pending_signals & ((uint64_t)1 << SIGINT)));
    check_restored(rt, &state);

    JS_SetInterruptHandler(rt, NULL, &state);
    value = evaluate(ctx, "evalScript('42')");
    assert(JS_StrictEq(ctx, value, JS_NewInt32(ctx, 42)));
    JS_FreeValue(ctx, value);
    assert(!rt->interrupt_handler && rt->interrupt_opaque == &state);
    JS_FreeValue(ctx, global);
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
