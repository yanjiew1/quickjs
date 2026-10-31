/* Preserve embedder cancellation across std.evalScript calls.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT
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
        os_pending_signals[SIGINT] = 1;
    }
    return state->cancel;
}

static int foreign_sigint_calls;

static JSValue foreign_sigint_handler(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    foreign_sigint_calls++;
    return JS_UNDEFINED;
}

static JSValue evaluate(JSContext *ctx, const char *source)
{
    ctx->interrupt_counter = 100;
    return JS_Eval(ctx, source, strlen(source), "std-interrupt", JS_EVAL_TYPE_GLOBAL);
}

static void check_restored(JSRuntime *rt, InterruptState *state)
{
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    void *opaque;

    assert(JS_GetInterruptHandler(rt, &opaque) == host_interrupt);
    assert(opaque == state);
    assert(ts->eval_script_recurse == 0);
}

static void test_foreign_sigint_owner(void)
{
    JSRuntime *eval_rt = JS_NewRuntime();
    JSRuntime *signal_rt = JS_NewRuntime();
    JSContext *eval_ctx, *signal_ctx;
    JSValue global, func, value, args[2];
    InterruptState state = { 0 };

    assert(eval_rt && signal_rt);
    eval_ctx = JS_NewContext(eval_rt);
    signal_ctx = JS_NewContext(signal_rt);
    assert(eval_ctx && signal_ctx);
    js_std_init_handlers(eval_rt);
    js_std_init_handlers(signal_rt);
    global = JS_GetGlobalObject(eval_ctx);
    func = JS_NewCFunction(eval_ctx, js_evalScript, "evalScript", 1);
    assert(JS_SetPropertyStr(eval_ctx, global, "evalScript", func) >= 0);
    JS_FreeValue(eval_ctx, global);

    args[0] = JS_NewInt32(signal_ctx, SIGINT);
    args[1] = JS_NewCFunction(signal_ctx, foreign_sigint_handler,
                              "foreignSigint", 0);
    value = js_os_signal(signal_ctx, JS_UNDEFINED, 2, args);
    assert(!JS_IsException(value));
    JS_FreeValue(signal_ctx, value);
    JS_FreeValue(signal_ctx, args[1]);

    JS_SetInterruptHandler(eval_rt, host_interrupt, &state);
    state.sigint = 1;
    foreign_sigint_calls = 0;
    value = evaluate(eval_ctx, "evalScript('for (let i = 0; i < 100000; i++); 42')");
    assert(JS_IsNumber(value) && JS_VALUE_GET_INT(value) == 42);
    JS_FreeValue(eval_ctx, value);
    assert(os_pending_signals[SIGINT]);
    assert(js_os_poll(signal_ctx, 0) >= 0);
    assert(foreign_sigint_calls == 1 && !os_pending_signals[SIGINT]);

    args[1] = JS_NULL;
    value = js_os_signal(signal_ctx, JS_UNDEFINED, 2, args);
    assert(!JS_IsException(value));
    JS_FreeValue(signal_ctx, value);
    JS_SetInterruptHandler(eval_rt, NULL, NULL);
    js_std_free_handlers(signal_rt);
    js_std_free_handlers(eval_rt);
    JS_FreeContext(signal_ctx);
    JS_FreeContext(eval_ctx);
    JS_FreeRuntime(signal_rt);
    JS_FreeRuntime(eval_rt);
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
    assert(!os_pending_signals[SIGINT]);
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
    test_foreign_sigint_owner();
    return 0;
}
