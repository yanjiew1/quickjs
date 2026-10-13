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
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
#endif
    return 0;
}
