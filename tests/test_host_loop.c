/* Host loop stepping must preserve every event source and pending exception.
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
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "quickjs-libc.h"
#ifndef _WIN32
#include <signal.h>

static volatile sig_atomic_t embedder_signal_count;

static void embedder_signal_handler(int signal_number)
{
    (void)signal_number;
    embedder_signal_count++;
}
#endif

static JSValue eval(JSContext *ctx, const char *source, int flags)
{
    JSValue value = JS_Eval(ctx, source, strlen(source), "tests/host-loop.js", flags);
    assert(!JS_IsException(value));
    return value;
}

static void run(JSContext *ctx, const char *source)
{
    JS_FreeValue(ctx, eval(ctx, source, JS_EVAL_TYPE_GLOBAL));
}

static int number(JSContext *ctx, const char *source)
{
    JSValue value = eval(ctx, source, JS_EVAL_TYPE_GLOBAL);
    int32_t n;
    assert(JS_ToInt32(ctx, &n, value) == 0);
    JS_FreeValue(ctx, value);
    return n;
}

static double now_ms(void)
{
    struct timespec ts;
    assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
    return (double)ts.tv_sec * 1000 + ts.tv_nsec / 1e6;
}

static void wait_for(JSContext *ctx, const char *condition)
{
    double deadline = now_ms() + 5000;
    while (!number(ctx, condition)) {
        assert(now_ms() < deadline);
        assert(js_std_poll_io(ctx, 20) >= 0);
        assert(js_std_loop_once(ctx) >= 0);
    }
}

static JSValue job(JSContext *ctx, int argc, JSValueConst *argv)
{
    if (argc)
        return JS_ThrowTypeError(ctx, "native job failure");
    run(ctx, "order.push(order.length)");
    return JS_UNDEFINED;
}

static JSValue checkpoint_error_job(JSContext *ctx, int argc, JSValueConst *argv)
{
    run(ctx, "var failedJobWeak = new WeakRef({}); var failedJobLive = false;"
             "Promise.resolve().then(() => {"
             " failedJobLive = failedJobWeak.deref() !== undefined; });");
    return JS_ThrowTypeError(ctx, "checkpoint test job failure");
}

static void clear_exception(JSContext *ctx)
{
    JSValue error;
    assert(JS_HasException(ctx));
    error = JS_GetException(ctx);
    JS_FreeValue(ctx, error);
}

static JSContext *worker_context(JSRuntime *rt)
{
    JSContext *ctx = JS_NewContext(rt);
    if (ctx && !js_init_module_os(ctx, "os")) {
        JS_FreeContext(ctx);
        return NULL;
    }
    return ctx;
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx, *temporary;
    JSValue value, promise, funcs[2];
    double start;
    int ret;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    assert(js_std_loop_once(ctx) == 0 && js_std_poll_io(ctx, -1) == 0);
    /* A checkpoint follows the complete microtask queue, not each job. */
    run(ctx, "var weak = new WeakRef({}); var weakLive = false;"
             "Promise.resolve().then(() => { weakLive = weak.deref() !== undefined; });");
    assert(js_std_loop_once(ctx) == 1);
    assert(number(ctx, "weakLive && weak.deref() === undefined") == 1);
    run(ctx, "var order = []");
    assert(JS_EnqueueJob(ctx, job, 0, NULL) == 0);
    assert(JS_EnqueueJob(ctx, job, 0, NULL) == 0);
    assert(js_std_poll_io(ctx, -1) == 0); /* Does not execute JS jobs. */
    assert(number(ctx, "order.length") == 0);
    assert(js_std_loop_once(ctx) == 1);
    assert(number(ctx, "order.join('') === '01'") == 1);
    temporary = JS_NewContext(rt);
    assert(temporary);
    value = JS_UNDEFINED;
    assert(JS_EnqueueJob(temporary, job, 1, &value) == 0);
    JS_FreeContext(temporary); /* Only the queued job retains this realm. */
    assert(js_std_loop_once(ctx) == -1);
    clear_exception(ctx);

    /* Native completions work without host handlers or an os import. */
    run(ctx, "var done = false; var shared = new Int32Array(new SharedArrayBuffer(4));"
             "Atomics.waitAsync(shared, 0, 0, 1).value.then(() => done = true)");
    wait_for(ctx, "done");
    assert(js_std_loop_once(ctx) == 0);

    js_std_init_handlers(rt);
    promise = JS_NewPromiseCapability(ctx, funcs);
    assert(!JS_IsException(promise));
    value = js_std_await(ctx, promise); /* Consumes the unsettled promise. */
    assert(JS_IsException(value));
    clear_exception(ctx);
    JS_FreeValue(ctx, funcs[0]);
    JS_FreeValue(ctx, funcs[1]);
    /* Retain targets when a job error leaves microtasks for a later step. */
    assert(JS_EnqueueJob(ctx, checkpoint_error_job, 0, NULL) == 0);
    js_std_loop(ctx);
    while ((ret = JS_ExecutePendingJob(rt, NULL)) > 0)
        ;
    assert(ret == 0 && number(ctx, "failedJobLive") == 1);
    JS_ClearKeptObjects(rt);
    assert(number(ctx, "failedJobWeak.deref() === undefined") == 1);

    assert(js_init_module_os(ctx, "os"));
    value = eval(ctx, "import * as os from 'os'; globalThis.os = os;",
                 JS_EVAL_TYPE_MODULE);
    assert(js_std_loop_once(ctx) >= 0);
    JS_FreeValue(ctx, value);
    run(ctx, "var ticks = 0; os.setTimeout(() => ticks++, 0);"
             "os.setTimeout(() => ticks++, 0)");
    assert(js_std_poll_io(ctx, 0) == 1 && number(ctx, "ticks") == 1);
    assert(js_std_poll_io(ctx, 0) == 1 && number(ctx, "ticks") == 2);
    assert(js_std_loop_once(ctx) == 0);
    run(ctx, "var timer = os.setTimeout(() => ticks++, 60000)");
    start = now_ms();
    assert(js_std_loop_once(ctx) == 1);
    assert(now_ms() - start < 1000);
    assert(number(ctx, "ticks") == 2);
    assert(JS_EnqueueJob(ctx, job, 0, NULL) == 0);
    start = now_ms();
    assert(js_std_poll_io(ctx, -1) >= 0);
    assert(now_ms() - start < 1000);
    assert(number(ctx, "order.length") == 2);
    assert(js_std_loop_once(ctx) == 1);
    assert(number(ctx, "order.length") == 3);
    run(ctx, "os.clearTimeout(timer); os.setTimeout(() => { throw 123 }, 0)");
    assert(js_std_poll_io(ctx, 0) == -1);
    value = JS_GetException(ctx);
    assert(JS_VALUE_GET_INT(value) == 123);
    JS_FreeValue(ctx, value);
    assert(js_std_loop_once(ctx) >= 0);
    run(ctx, "os.setTimeout(() => { throw 'legacy loop callback' }, 0);"
             "os.setTimeout(() => ticks++, 0)");
    js_std_loop(ctx);
    assert(number(ctx, "ticks") == 3 && !JS_HasException(ctx));
    run(ctx, "os.setTimeout(() => { throw 'legacy await callback' }, 0)");
    value = eval(ctx, "os.sleepAsync(1)", JS_EVAL_TYPE_GLOBAL);
    value = js_std_await(ctx, value);
    assert(!JS_IsException(value) && !JS_HasException(ctx));
    JS_FreeValue(ctx, value);

#ifndef _WIN32
    {
        int fds[2];
        char source[256];
        assert(pipe(fds) == 0 && write(fds[1], "x", 1) == 1);
        snprintf(source, sizeof(source),
                 "os.setReadHandler(%d, () => { os.setReadHandler(%d, null); ticks++ })",
                 fds[0], fds[0]);
        run(ctx, source);
        assert(js_std_poll_io(ctx, 100) == 1 && number(ctx, "ticks") == 4);
        assert(close(fds[0]) == 0 && close(fds[1]) == 0);
        assert(js_std_loop_once(ctx) == 0);
        assert(pipe(fds) == 0);
        snprintf(source, sizeof(source), "os.setReadHandler(%d, () => ticks++)", fds[0]);
        run(ctx, source);
        assert(close(fds[0]) == 0);
        assert(js_std_poll_io(ctx, 0) == -1);
        clear_exception(ctx);
        snprintf(source, sizeof(source), "os.setReadHandler(%d, null)", fds[0]);
        run(ctx, source);
        assert(close(fds[1]) == 0);
        snprintf(source, sizeof(source),
                 "os.signal(%d, () => { ticks++; os.signal(%d, null) })", SIGUSR1, SIGUSR1);
        run(ctx, source);
        assert(raise(SIGUSR1) == 0);
        assert(js_std_loop_once(ctx) == 1 && number(ctx, "ticks") == 5);
        assert(js_std_loop_once(ctx) == 0);
        snprintf(source, sizeof(source),
                 "os.signal(%d, () => { ticks++; os.signal(%d, null) });"
                 "os.signal(%d, () => { ticks++; os.signal(%d, null) })",
                 SIGUSR1, SIGUSR1, SIGUSR2, SIGUSR2);
        run(ctx, source);
        assert(raise(SIGUSR1) == 0 && raise(SIGUSR2) == 0);
        assert(js_std_loop_once(ctx) == 1 && number(ctx, "ticks") == 6);
        assert(js_std_loop_once(ctx) == 1 && number(ctx, "ticks") == 7);
        assert(js_std_loop_once(ctx) == 0);
    }
#endif
    run(ctx, "done = false; Atomics.waitAsync(shared, 0, 0, 1).value.then(() => done = true)");
    wait_for(ctx, "done");
    js_std_set_worker_new_context_func(worker_context);
    JS_SetModuleLoaderFunc2(rt, NULL, js_module_loader, js_module_check_attributes, NULL);
    value = eval(ctx,
                 "import { Worker } from 'os';"
                 "globalThis.messages = 0; globalThis.worker = new Worker('./test_host_loop_worker.js');"
                 "worker.onmessage = e => { messages++; if (messages === 2) {"
                 "worker.onmessage = null; throw 456; } };", JS_EVAL_TYPE_MODULE);
    JS_FreeValue(ctx, value);
    start = now_ms();
    while (number(ctx, "messages") < 2) {
        int status;
        assert(now_ms() - start < 5000);
        status = js_std_poll_io(ctx, 20);
        if (status < 0) {
            value = JS_GetException(ctx);
            assert(JS_VALUE_GET_INT(value) == 456 && number(ctx, "messages") == 2);
            JS_FreeValue(ctx, value);
        }
    }
    assert(js_std_loop_once(ctx) == 0);
    js_std_set_worker_new_context_func(NULL);
    {
        int embedder_state = 42;
        JSRuntime *bare_rt = JS_NewRuntime();
        JSContext *bare;
        assert(bare_rt);
        bare = JS_NewContext(bare_rt);
        assert(bare);
        JS_SetRuntimeOpaque(bare_rt, &embedder_state);
        assert(js_std_loop_once(bare) == 0);
        run(bare, "var done = false; var shared = new Int32Array(new SharedArrayBuffer(4));"
                  "Atomics.waitAsync(shared, 0, 0, 1).value.then(() => done = true)");
        wait_for(bare, "done");
        assert(JS_GetRuntimeOpaque(bare_rt) == &embedder_state);
        JS_FreeContext(bare);
        JS_FreeRuntime(bare_rt);
    }
#ifndef _WIN32
    {
        void (*previous)(int);

        previous = signal(SIGUSR1, embedder_signal_handler);
        assert(previous != SIG_ERR);
        run(ctx, "os.signal(os.SIGUSR1, () => {})");
        js_std_free_handlers(rt);
        /* Sanitizers may wrap signal handlers, so test the restored behavior. */
        assert(raise(SIGUSR1) == 0);
        assert(embedder_signal_count == 1);
        assert(signal(SIGUSR1, previous) != SIG_ERR);
    }
#else
    js_std_free_handlers(rt);
#endif
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
