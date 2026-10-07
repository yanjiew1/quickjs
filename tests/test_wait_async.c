/*
 * Asynchronous Atomics wait embedding and lifetime regressions.
 *
 * Copyright (c) 2026 Yan-Jie Wang
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
#include "quickjs.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

#if !defined(__EMSCRIPTEN__)
typedef struct TestLog {
    char text[128];
    size_t length;
} TestLog;

typedef struct TestNotifier {
    _Atomic(int32_t) *words;
    int expected;
} TestNotifier;

static JSValue eval(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source), "<waitAsync test>",
                            JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(value));
    return value;
}

static void eval_void(JSContext *ctx, const char *source)
{
    JS_FreeValue(ctx, eval(ctx, source));
}

static JSValue log_job(JSContext *ctx, int argc, JSValueConst *argv)
{
    TestLog *log = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    const char *text = JS_ToCString(ctx, argv[0]);
    size_t length;
    assert(text);
    length = strlen(text);
    assert(log->length + length + 2 < sizeof(log->text));
    if (log->length)
        log->text[log->length++] = ',';
    memcpy(log->text + log->length, text, length + 1);
    log->length += length;
    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

static JSValue log_call(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv)
{
    return log_job(ctx, argc, argv);
}

static void install_shared(JSContext *ctx, _Atomic(int32_t) *words,
                            JSFreeArrayBufferDataFunc *free_func, void *opaque)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue shared = JS_NewArrayBuffer(ctx, (uint8_t *)words,
                                       2 * sizeof(*words), free_func, opaque, 1);
    assert(!JS_IsException(shared));
    assert(JS_SetPropertyStr(ctx, global, "shared", shared) >= 0);
    assert(JS_SetPropertyStr(ctx, global, "pushLog",
                             JS_NewCFunction(ctx, log_call, "pushLog", 1)) >= 0);
    JS_FreeValue(ctx, global);
}

static void *foreign_notify(void *opaque)
{
    TestNotifier *test = opaque;
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue value;
    int32_t count;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    install_shared(ctx, test->words, NULL, NULL);
    /* This thread owns its own runtime. It never calls a JS API with the
       waiting runtime, context, capability or ordinary job entry. */
    value = eval(ctx, "Atomics.notify(new Int32Array(shared), 0, 1)");
    assert(!JS_ToInt32(ctx, &count, value) && count == test->expected);
    JS_FreeValue(ctx, value);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return NULL;
}

static void notify_join(_Atomic(int32_t) *words, int expected)
{
    TestNotifier test = { words, expected };
    pthread_t thread;
    assert(!pthread_create(&thread, NULL, foreign_notify, &test));
    assert(!pthread_join(thread, NULL));
}

static void drain(JSRuntime *rt)
{
    int ret;
    while ((ret = JS_ExecutePendingJob(rt, NULL)) > 0) {}
    assert(ret == 0);
}

static void enqueue_text(JSContext *ctx, const char *text)
{
    JSValue value = JS_NewString(ctx, text);
    assert(!JS_IsException(value));
    assert(!JS_EnqueueJob(ctx, log_job, 1, (JSValueConst *)&value));
    JS_FreeValue(ctx, value);
}

static void test_foreign_fifo_and_context_root(void)
{
    _Atomic(int32_t) words[2] = { 0, 0 };
    TestLog log = { { 0 }, 0 };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    assert(rt && ctx);
    JS_SetRuntimeOpaque(rt, &log);
    JS_SetCanBlock(rt, 0);
    install_shared(ctx, words, NULL, NULL);
    enqueue_text(ctx, "initial");
    eval_void(ctx, "Atomics.waitAsync(new Int32Array(shared), 0, 0).value.then(pushLog)");
    enqueue_text(ctx, "before");
    assert(JS_IsNativeJobPending(rt));
    assert(JS_GetNativeJobWakeHandle(rt) != -1);
    notify_join(words, 1);
    enqueue_text(ctx, "after");
    JS_RunGC(rt);
    /* The native wait and queued jobs, rather than the embedder reference,
       retain the creator realm until completion and the final reaction. */
    JS_FreeContext(ctx);
    assert(JS_PollNativeJobs(rt, 0) == 1);
    drain(rt);
    assert(!strcmp(log.text, "initial,before,after,ok"));
    assert(!JS_IsNativeJobPending(rt));
    assert(JS_GetNativeJobTimeout(rt) == -1);
    JS_FreeRuntime(rt);
}

static void test_queued_timeout_can_lose_to_notify(void)
{
    _Atomic(int32_t) words[2] = { 0, 0 };
    TestLog log = { { 0 }, 0 };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    struct timespec delay = { 0, 5000000 };
    assert(rt && ctx);
    JS_SetRuntimeOpaque(rt, &log);
    install_shared(ctx, words, NULL, NULL);
    eval_void(ctx, "Atomics.waitAsync(new Int32Array(shared), 0, 0, 1).value.then(pushLog)");
    enqueue_text(ctx, "before");
    nanosleep(&delay, NULL);
    assert(JS_PollNativeJobs(rt, 0) == 1); /* preallocated timeout token */
    notify_join(words, 1); /* waiter still linked until its timeout job executes */
    enqueue_text(ctx, "after");
    drain(rt);
    assert(!strcmp(log.text, "before,after,ok"));
    assert(!JS_IsNativeJobPending(rt));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void count_backing_free(JSRuntime *rt, void *opaque, void *ptr)
{
    int *count = opaque;
    ++*count; /* native identity remains live for the post-teardown notifier */
}

static void test_teardown_cancels_and_releases_backing(void)
{
    _Atomic(int32_t) words[2] = { 0, 0 };
    int freed = 0;
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    assert(rt && ctx);
    install_shared(ctx, words, count_backing_free, &freed);
    eval_void(ctx, "Atomics.waitAsync(new Int32Array(shared), 0, 0)");
    assert(JS_IsNativeJobPending(rt));
    JS_FreeContext(ctx);
    JS_RunGC(rt);
    assert(freed == 0);
    JS_FreeRuntime(rt);
    assert(freed == 1);
    notify_join(words, 0);
}

static void test_realm_and_nonblocking_owner(void)
{
    _Atomic(int32_t) words[2] = { 0, 0 };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *creator = JS_NewContext(rt), *caller = JS_NewContext(rt);
    JSValue global, caller_global, method, prototype, promise_prototype;
    assert(rt && creator && caller);
    JS_SetCanBlock(rt, 0);
    install_shared(caller, words, NULL, NULL);
    global = JS_GetGlobalObject(creator);
    caller_global = JS_GetGlobalObject(caller);
    method = eval(creator, "Atomics.waitAsync");
    prototype = eval(creator, "Object.prototype");
    promise_prototype = eval(creator, "Promise.prototype");
    assert(JS_SetPropertyStr(caller, caller_global, "foreignWait", method) >= 0);
    assert(JS_SetPropertyStr(caller, caller_global, "creatorObject", prototype) >= 0);
    assert(JS_SetPropertyStr(caller, caller_global, "creatorPromise", promise_prototype) >= 0);
    eval_void(caller,
        "var words = new Int32Array(shared);"
        "var result = foreignWait(words, 0, 0);"
        "if (!result.async || Object.getPrototypeOf(result) !== creatorObject ||"
        "Object.getPrototypeOf(result.value) !== creatorPromise) throw Error('realm');"
        "if (Atomics.notify(words, 0, 1) !== 1) throw Error('notify');");
    drain(rt);
    JS_FreeValue(creator, global);
    JS_FreeValue(caller, caller_global);
    JS_FreeContext(caller);
    JS_FreeContext(creator);
    JS_FreeRuntime(rt);
}

static void test_oom_does_not_publish_a_wait(void)
{
    size_t budget;
    int failures = 0, successes = 0;
    for (budget = 0; budget <= 4096; budget += 32) {
        _Atomic(int32_t) words[2] = { 0, 0 };
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx = JS_NewContext(rt);
        JSMemoryUsage usage;
        JSValue function, result, value;
        int32_t count;
        assert(rt && ctx);
        install_shared(ctx, words, NULL, NULL);
        /* Keep intrinsic autoinitialization outside the calibrated operation. */
        eval_void(ctx, "void Atomics.waitAsync; var words = new Int32Array(shared)");
        function = eval(ctx, "(function () { return Atomics.waitAsync(words, 0, 0) })");
        JS_ComputeMemoryUsage(rt, &usage);
        JS_SetMemoryLimit(rt, usage.malloc_size + budget);
        result = JS_Call(ctx, function, JS_UNDEFINED, 0, NULL);
        JS_SetMemoryLimit(rt, (size_t)-1);
        if (JS_IsException(result)) {
            failures++;
            value = JS_GetException(ctx);
            JS_FreeValue(ctx, value);
            assert(!JS_IsNativeJobPending(rt));
            value = eval(ctx, "Atomics.notify(words, 0, 1)");
            assert(!JS_ToInt32(ctx, &count, value) && count == 0);
            JS_FreeValue(ctx, value);
        } else {
            successes++;
            assert(JS_IsNativeJobPending(rt));
            value = eval(ctx, "Atomics.notify(words, 0, 1)");
            assert(!JS_ToInt32(ctx, &count, value) && count == 1);
            JS_FreeValue(ctx, value);
            drain(rt);
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, function);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
    assert(failures > 0 && successes > 0);
}
#endif

int main(void)
{
#if !defined(__EMSCRIPTEN__)
    test_foreign_fifo_and_context_root();
    test_queued_timeout_can_lose_to_notify();
    test_teardown_cancels_and_releases_backing();
    test_realm_and_nonblocking_owner();
    test_oom_does_not_publish_a_wait();
#endif
    return 0;
}
