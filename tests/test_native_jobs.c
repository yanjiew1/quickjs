/*
 * Owner-thread native job bridge regression tests.
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
#include "../src/quickjs/internal/native-jobs.h"
#include "../src/quickjs/internal/runtime.h"
#include "../src/quickjs/internal/allocator.h"
#include <assert.h>
#include <string.h>
#include <float.h>
#include <limits.h>

#ifdef CONFIG_ATOMICS
typedef struct TestLog {
    char text[64];
    int released;
} TestLog;

typedef struct TestRecord {
    JSNativeAsyncRecord record;
    TestLog *log;
} TestRecord;

typedef struct TestNotify {
    JSNativeWaitQueue *queue;
    const void *location;
} TestNotify;

static JSValue log_job(JSContext *ctx, int argc, JSValueConst *argv)
{
    TestLog *log = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    const char *text = JS_ToCString(ctx, argv[0]);
    assert(text);
    assert(strlen(log->text) + strlen(text) + 2 < sizeof(log->text));
    if (log->text[0])
        strcat(log->text, ",");
    strcat(log->text, text);
    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

static int enqueue_text(JSContext *ctx, const char *text)
{
    JSValue value = JS_NewString(ctx, text);
    int ret;
    if (JS_IsException(value))
        return -1;
    ret = JS_EnqueueJob(ctx, log_job, 1, (JSValueConst *)&value);
    JS_FreeValue(ctx, value);
    return ret;
}

static JSValue resolve_record(JSNativeAsyncRecord *record,
                              JSNativeWaitResult result)
{
    int ret = enqueue_text(record->ctx,
                           result == JS_NATIVE_WAIT_OK ? "ok" : "timeout");
    return ret ? JS_EXCEPTION : JS_UNDEFINED;
}

static void release_record(JSNativeAsyncRecord *record)
{
    TestRecord *test = (TestRecord *)record;
    JSContext *ctx = record->ctx;
    ++test->log->released;
    js_free(ctx, test);
    JS_FreeContext(ctx);
}

static TestRecord *add_record(JSContext *ctx, JSNativeJobOwner *owner,
                              const void *location, double timeout_ms)
{
    TestRecord *test = js_mallocz(ctx, sizeof(*test));
    assert(test);
    test->log = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    test->record.ctx = JS_DupContext(ctx);
    test->record.resolve = resolve_record;
    test->record.release = release_record;
    js_native_waiter_init(&test->record.native, &test->record);
    js_native_wait_queue_lock(owner->native.queue);
    js_native_wait_add_relative_locked(owner->native.queue, &test->record.native,
                                       location, &owner->native,
                                       js_native_jobs_now(), timeout_ms);
    js_native_wait_queue_unlock(owner->native.queue);
    return test;
}

static void *native_notify(void *opaque)
{
    TestNotify *test = opaque;
    JSNativeWaiter *inline_waiters;
    assert(js_native_wait_notify(test->queue, test->location, 1, NULL,
                                  &inline_waiters) == 1);
    assert(!inline_waiters);
    return NULL;
}

static void drain(JSRuntime *rt)
{
    int ret;
    while ((ret = JS_ExecutePendingJob(rt, NULL)) > 0) {}
    assert(ret == 0);
}

static void test_job_fifo_and_public_poll_contract(void)
{
    TestLog log = { { 0 }, 0 };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSNativeJobOwner *owner;
    TestNotify notifier;
    pthread_t thread;
    int location;
    assert(rt && ctx);
    JS_SetRuntimeOpaque(rt, &log);
    assert(!JS_IsNativeJobPending(rt));
    assert(JS_GetNativeJobWakeHandle(rt) == -1);
    assert(JS_GetNativeJobTimeout(rt) == -1);
    assert(JS_PollNativeJobs(rt, -1) == 0);
    assert(!enqueue_text(ctx, "initial"));
    owner = js_native_jobs_get_owner(ctx);
    assert(owner && JS_IsJobPending(rt));
    add_record(ctx, owner, &location, JS_NATIVE_WAIT_FOREVER);
    assert(!enqueue_text(ctx, "before"));
    notifier.queue = owner->native.queue;
    notifier.location = &location;
    assert(!pthread_create(&thread, NULL, native_notify, &notifier));
    assert(!pthread_join(thread, NULL));
    assert(!enqueue_text(ctx, "after"));
    JS_FreeContext(ctx);
    JS_RunGC(rt);
    assert(JS_PollNativeJobs(rt, 0) == 1);
    assert(JS_GetNativeJobTimeout(rt) == 0);
    drain(rt);
    assert(!strcmp(log.text, "initial,before,after,ok"));
    assert(log.released == 1 && !JS_IsNativeJobPending(rt));
    JS_FreeRuntime(rt);
}

static void test_public_poll_timeout_and_cancel(void)
{
    TestLog log = { { 0 }, 0 };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSNativeJobOwner *owner;
    int locations[2], timeout, ret;
    long double retry_end;
    assert(rt && ctx);
    JS_SetRuntimeOpaque(rt, &log);
    owner = js_native_jobs_get_owner(ctx);
    assert(owner);
    add_record(ctx, owner, &locations[0], 0.5);
    add_record(ctx, owner, &locations[1], JS_NATIVE_WAIT_FOREVER);
    timeout = JS_GetNativeJobTimeout(rt);
    assert(timeout == 0 || timeout == 1);
    retry_end = js_native_jobs_now() + 10000000000.0L;
    do {
        ret = JS_PollNativeJobs(rt, -1);
        assert(ret >= 0);
        assert(js_native_jobs_now() < retry_end);
    } while (ret != 1);
    drain(rt);
    assert(!strcmp(log.text, "timeout") && log.released == 1);
    assert(JS_IsNativeJobPending(rt) && JS_GetNativeJobTimeout(rt) == -1);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(log.released == 2);
}
static void test_finite_huge_timeout_identity(void)
{
    TestLog log = { { 0 }, 0 };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSNativeJobOwner *owner;
    int locations[2];
    assert(rt && ctx);
    JS_SetRuntimeOpaque(rt, &log);
    owner = js_native_jobs_get_owner(ctx);
    assert(owner);
    add_record(ctx, owner, &locations[0], DBL_MAX);
    assert(JS_GetNativeJobTimeout(rt) == INT_MAX);
    add_record(ctx, owner, &locations[1], INFINITY);
    assert(JS_GetNativeJobTimeout(rt) == INT_MAX);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(log.released == 2);
    rt = JS_NewRuntime();
    ctx = JS_NewContext(rt);
    assert(rt && ctx);
    JS_SetRuntimeOpaque(rt, &log);
    owner = js_native_jobs_get_owner(ctx);
    add_record(ctx, owner, &locations[0], INFINITY);
    assert(JS_GetNativeJobTimeout(rt) == -1);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(log.released == 3);
}
#endif

int main(void)
{
#ifdef CONFIG_ATOMICS
    test_finite_huge_timeout_identity();
    test_job_fifo_and_public_poll_contract();
    test_public_poll_timeout_and_cancel();
#endif
    return 0;
}
