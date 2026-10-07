/*
 * QuickJS Atomics wait native tests
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <string.h>
#include "../src/quickjs/internal/base.h"
#include "quickjs-libc.h"

#ifdef CONFIG_ATOMICS
static int test_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
static int test_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                              const struct timespec *deadline);

/* Compile the actual owner with its condition waits intercepted. */
#define pthread_cond_wait test_cond_wait
#define pthread_cond_timedwait test_cond_timedwait
#include "../src/quickjs/builtins/atomics.c"
#undef pthread_cond_wait
#undef pthread_cond_timedwait
#endif

#ifdef CONFIG_ATOMICS
#include <sched.h>

typedef struct {
    _Atomic(int32_t) *word;
    _Atomic(int64_t) *big_word;
    _Atomic(uint32_t) writes;
    _Atomic(int) stop;
} TestAtomicWriter;

static void *test_atomic_writer(void *opaque)
{
    TestAtomicWriter *writer = opaque;
    uint32_t n = 0;

    while (!atomic_load(&writer->stop)) {
        atomic_store(writer->word, n & 1);
        atomic_store(writer->big_word, n & 1);
        atomic_store(&writer->writes, ++n);
    }
    return NULL;
}

static JSValue test_eval(JSContext *ctx, const char *source)
{
    JSValue result = JS_Eval(ctx, source, strlen(source),
                             "test_atomics_wait", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(result));
    return result;
}

static void test_concurrent_expected_value(JSContext *ctx)
{
    const char *source =
        "for (let i = 0; i < 10000; i++) {"
        "  for (const view of [testWords, testBigWords]) {"
        "    const expected = view === testWords ? 0 : 0n;"
        "    const result = Atomics.wait(view, 0, expected, 0);"
        "    if (result !== 'not-equal' && result !== 'timed-out')"
        "      throw Error('unexpected wait result: ' + result);"
        "  }"
        "}";
    TestAtomicWriter writer;
    pthread_t thread;
    JSValue buffer, result;
    uint8_t *data;
    size_t size;

    buffer = test_eval(ctx,
        "globalThis.testShared = new SharedArrayBuffer(16);"
        "globalThis.testWords = new Int32Array(testShared, 0, 1);"
        "globalThis.testBigWords = new BigInt64Array(testShared, 8, 1);"
        "testShared;");
    data = JS_GetArrayBuffer(ctx, &size, buffer);
    assert(data && size == 16);
    writer.word = (_Atomic(int32_t) *)data;
    writer.big_word = (_Atomic(int64_t) *)(data + 8);
    atomic_init(&writer.writes, 0);
    atomic_init(&writer.stop, 0);
    assert(pthread_create(&thread, NULL, test_atomic_writer, &writer) == 0);
    while (atomic_load(&writer.writes) == 0)
        sched_yield();
    result = test_eval(ctx, source);
    JS_FreeValue(ctx, result);
    atomic_store(&writer.stop, 1);
    assert(pthread_join(thread, NULL) == 0);
    assert(atomic_load(&writer.writes) > 0);
    JS_FreeValue(ctx, buffer);
}
typedef enum {
    TEST_WAIT_NORMAL,
    TEST_WAIT_SPURIOUS_TIMEOUT,
    TEST_WAIT_NOTIFY_TIMEOUT_RACE,
    TEST_WAIT_SPURIOUS_NOTIFY,
} TestWaitMode;

static TestWaitMode test_wait_mode;
static JSContext *test_wait_context;
static int test_wait_calls;
static struct timespec test_wait_deadline;

static void test_notify_while_unlocked(pthread_mutex_t *mutex)
{
    JSValue result;
    int notified;

    assert(pthread_mutex_unlock(mutex) == 0);
    result = test_eval(test_wait_context, "Atomics.notify(testWords, 0, 1)");
    assert(JS_ToInt32(test_wait_context, &notified, result) == 0);
    assert(notified == 1);
    JS_FreeValue(test_wait_context, result);
    assert(pthread_mutex_lock(mutex) == 0);
}

static int test_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    if (test_wait_mode == TEST_WAIT_NORMAL)
        return pthread_cond_wait(cond, mutex);
    assert(test_wait_mode == TEST_WAIT_SPURIOUS_NOTIFY);
    assert(++test_wait_calls <= 3);
    if (test_wait_calls < 3)
        return 0;
    test_notify_while_unlocked(mutex);
    return 0;
}

static int test_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                              const struct timespec *deadline)
{
    if (test_wait_mode == TEST_WAIT_NORMAL)
        return pthread_cond_timedwait(cond, mutex, deadline);
    assert(++test_wait_calls <= 4);
    if (test_wait_mode == TEST_WAIT_NOTIFY_TIMEOUT_RACE) {
        assert(test_wait_calls == 1);
        test_notify_while_unlocked(mutex);
        return ETIMEDOUT;
    }
    assert(test_wait_mode == TEST_WAIT_SPURIOUS_TIMEOUT);
    if (test_wait_calls == 1) {
        test_wait_deadline = *deadline;
    } else {
        assert(deadline->tv_sec == test_wait_deadline.tv_sec);
        assert(deadline->tv_nsec == test_wait_deadline.tv_nsec);
    }
    if (test_wait_calls < 4)
        return 0;
    return pthread_cond_timedwait(cond, mutex, deadline);
}

static void test_wait_result(JSContext *ctx, TestWaitMode mode,
                             const char *source, const char *expected,
                             int expected_calls)
{
    JSValue result;
    const char *string;

    result = test_eval(ctx, "Atomics.store(testWords, 0, 0)");
    JS_FreeValue(ctx, result);
    test_wait_context = ctx;
    test_wait_mode = mode;
    test_wait_calls = 0;
    result = test_eval(ctx, source);
    test_wait_mode = TEST_WAIT_NORMAL;
    string = JS_ToCString(ctx, result);
    assert(string && !strcmp(string, expected));
    assert(test_wait_calls == expected_calls);
    assert(!js_native_jobs_wait_queue()->first);
    JS_FreeCString(ctx, string);
    JS_FreeValue(ctx, result);
    test_wait_context = NULL;
}

static void test_wait_notification_predicate(JSContext *ctx)
{
    test_wait_result(ctx, TEST_WAIT_SPURIOUS_TIMEOUT,
        "Atomics.wait(testWords, 0, 0, 0)", "timed-out", 4);
    test_wait_result(ctx, TEST_WAIT_NOTIFY_TIMEOUT_RACE,
        "Atomics.wait(testWords, 0, 0, 0)", "ok", 1);
    test_wait_result(ctx, TEST_WAIT_SPURIOUS_NOTIFY,
        "Atomics.wait(testWords, 0, 0)", "ok", 3);
}

#endif

#ifdef CONFIG_ATOMICS
static void test_javascript_wait_cases(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    size_t size;
    uint8_t *source;
    JSValue result;

    JS_SetModuleLoaderFunc2(rt, NULL, js_module_loader,
                           js_module_check_attributes, NULL);
    source = js_load_file(ctx, &size, "tests/test_atomics.js");
    assert(source);
    result = JS_Eval(ctx, (const char *)source, size,
                     "tests/test_atomics.js", JS_EVAL_TYPE_MODULE);
    js_free(ctx, source);
    assert(!JS_IsException(result));
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, result);
}
#endif

int main(void)
{
#ifdef CONFIG_ATOMICS
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;

    assert(rt);
    JS_SetCanBlock(rt, TRUE);
    ctx = JS_NewContext(rt);
    assert(ctx);
    test_javascript_wait_cases(ctx);
    test_concurrent_expected_value(ctx);
    test_wait_notification_predicate(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
#endif
    return 0;
}
