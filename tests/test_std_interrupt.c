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

#ifndef _WIN32
static int cross_thread_signal_calls;

static JSValue cross_thread_signal_handler(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv)
{
    cross_thread_signal_calls++;
    return JS_UNDEFINED;
}

static JSValue signal_wake_timeout_handler(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv)
{
    return JS_UNDEFINED;
}

static void *send_signal_from_worker(void *opaque)
{
    struct timespec delay = { 0, 20000000 };

    nanosleep(&delay, NULL);
    pthread_kill(pthread_self(), SIGUSR1);
    return NULL;
}

static void test_cross_thread_signal_wake(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue args[2], value, timer_args[2], timer_id;
    pthread_t worker;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    js_std_init_handlers(rt);
    args[0] = JS_NewInt32(ctx, SIGUSR1);
    args[1] = JS_NewCFunction(ctx, cross_thread_signal_handler,
                              "crossThreadSignal", 0);
    value = js_os_signal(ctx, JS_UNDEFINED, 2, args);
    assert(!JS_IsException(value));
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, args[1]);
    timer_args[0] = JS_NewCFunction(ctx, signal_wake_timeout_handler,
                                    "signalWakeTimeout", 0);
    timer_args[1] = JS_NewInt32(ctx, 200);
    timer_id = js_os_setTimeout(ctx, JS_UNDEFINED, 2, timer_args);
    assert(!JS_IsException(timer_id));
    JS_FreeValue(ctx, timer_args[0]);
    cross_thread_signal_calls = 0;
    assert(pthread_create(&worker, NULL, send_signal_from_worker, NULL) == 0);
    assert(js_os_poll(ctx, 1000) >= 0);
    assert(pthread_join(worker, NULL) == 0);
    assert(cross_thread_signal_calls == 1);
    value = js_os_clearTimeout(ctx, JS_UNDEFINED, 1, &timer_id);
    assert(!JS_IsException(value));
    JS_FreeValue(ctx, value);
    args[1] = JS_NULL;
    value = js_os_signal(ctx, JS_UNDEFINED, 2, args);
    assert(!JS_IsException(value));
    JS_FreeValue(ctx, value);
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}
#endif

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
#ifndef _WIN32
    assert(js_os_poll(signal_ctx, 0) >= 0);
    assert(foreign_sigint_calls == 1 && !os_pending_signals[SIGINT]);
#else
    /* Windows polling does not dispatch os.signal callbacks. */
    assert(foreign_sigint_calls == 0);
#endif

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
#ifndef _WIN32
    test_cross_thread_signal_wake();
#endif
    return 0;
}
