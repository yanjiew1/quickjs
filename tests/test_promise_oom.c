/* Promise and async-generator allocation failures.
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
#include "quickjs.h"
#include "pool-padding.h"
#include "oom-allocator.h"

static void *nested_padding;
static JSValue nested_error;
static void *import_padding;
static JSValue import_attributes;
static int import_armed;

typedef struct AsyncGeneratorReentry {
    JSValue generator, next;
    int calls;
} AsyncGeneratorReentry;

static void reenter_start_throw(JSContext *ctx, JSValueConst promise,
                                JSValueConst reason, JS_BOOL is_handled,
                                void *opaque)
{
    AsyncGeneratorReentry *test = opaque;
    JSValue result;

    if (is_handled)
        return;
    test->calls++;
    result = JS_Call(ctx, test->next, test->generator, 0, NULL);
    /* Throw on an unstarted generator marks it completed before rejection;
       a host rejection tracker can observe the immediate next result. */
    assert(!JS_IsException(result));
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, result);
}

static void test_async_generator_start_throw(void)
{
    static const char source[] = "(async function*() {})()";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    AsyncGeneratorReentry test;
    JSValue method, result;
    JSValueConst reason = JS_NewInt32(ctx, 42);

    assert(rt && ctx);
    test.generator = JS_Eval(ctx, source, sizeof(source) - 1, "start-throw.js",
                             JS_EVAL_TYPE_GLOBAL);
    test.next = JS_GetPropertyStr(ctx, test.generator, "next");
    test.calls = 0;
    method = JS_GetPropertyStr(ctx, test.generator, "throw");
    assert(!JS_IsException(test.generator) && !JS_IsException(test.next));
    assert(!JS_IsException(method));
    JS_SetHostPromiseRejectionTracker(rt, reenter_start_throw, &test);
    result = JS_Call(ctx, method, test.generator, 1, &reason);
    assert(!JS_IsException(result) && !JS_HasException(ctx));
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_REJECTED);
    assert(test.calls == 1);
    JS_SetHostPromiseRejectionTracker(rt, NULL, NULL);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, method);
    JS_FreeValue(ctx, test.next);
    JS_FreeValue(ctx, test.generator);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static JSValue fail_import_allocation(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    import_armed = 1;
    import_padding = test_pool_padding(rt);
    JS_SetMemoryLimit(rt, 0);
    return JS_DupValue(ctx, import_attributes);
}

static void test_dynamic_import_oom(int with_attributes)
{
    static const char source[] =
        "import('./absent.js', { get with() { return failImport(); } })";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue global, result, error;

    assert(rt && ctx);
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "failImport",
               JS_NewCFunction(ctx, fail_import_allocation, "failImport", 0)) > 0);
    import_attributes = with_attributes ? JS_NewObject(ctx) : JS_UNDEFINED;
    assert(!JS_IsException(import_attributes));
    if (with_attributes)
        assert(JS_SetPropertyStr(ctx, import_attributes, "type",
                                 JS_NewString(ctx, "json")) > 0);
    import_padding = NULL;
    import_armed = 0;
    result = JS_Eval(ctx, source, sizeof(source) - 1,
                     "<import-oom>", JS_EVAL_TYPE_GLOBAL);
    JS_SetMemoryLimit(rt, (size_t)-1);
    assert(!JS_IsException(result) && !JS_HasException(ctx));
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_REJECTED);
    error = JS_PromiseResult(ctx, result);
    assert(!JS_IsJobPending(rt) && import_armed);
    assert(JS_IsError(ctx, error) || JS_IsNull(error));
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, import_attributes);
    import_attributes = JS_UNDEFINED;
    JS_FreeValue(ctx, global);
    test_pool_padding_free(rt, import_padding);
    import_padding = NULL;
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static JSValue resolve_inner(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int magic,
                              JSValue *data)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSValue result;

    nested_padding = test_pool_padding(rt);
    JS_SetMemoryLimit(rt, 0);
    result = JS_Call(ctx, argv[0], JS_UNDEFINED, 1, (JSValueConst *)data);
    JS_SetMemoryLimit(rt, (size_t)-1);
    assert(JS_IsException(result));
    assert(JS_HasException(ctx));
    nested_error = JS_GetException(ctx);
    return JS_Throw(ctx, JS_DupValue(ctx, nested_error));
}

static void test_nested(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    const char *source = "({ then(resolve) { resolve(2); } })";
    JSValue inner, outer, then, promise, funcs[2], result;
    int jobs = 0;

    inner = JS_Eval(ctx, source, strlen(source), "<nested-promise-oom>",
                    JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(inner));
    outer = JS_NewObject(ctx);
    assert(!JS_IsException(outer));
    then = JS_NewCFunctionData(ctx, resolve_inner, 1, 0, 1,
                              (JSValueConst *)&inner);
    assert(!JS_IsException(then));
    assert(JS_SetPropertyStr(ctx, outer, "then", then) >= 0);
    promise = JS_NewPromiseCapability(ctx, funcs);
    assert(!JS_IsException(promise));
    result = JS_Call(ctx, funcs[0], JS_UNDEFINED, 1, (JSValueConst *)&outer);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, result);
    while (JS_IsJobPending(rt)) {
        assert(++jobs < 10);
        assert(JS_ExecutePendingJob(rt, NULL) > 0);
    }
    assert(jobs == 1);
    assert(!JS_HasException(ctx));
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_REJECTED);
    result = JS_PromiseResult(ctx, promise);
    /* Under complete exhaustion even allocating the OOM Error can fail. */
    if (JS_IsNull(nested_error)) {
        assert(JS_IsNull(result));
    } else {
        assert(JS_IsObject(nested_error) && JS_IsObject(result));
        assert(JS_VALUE_GET_PTR(result) == JS_VALUE_GET_PTR(nested_error));
    }
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, nested_error);
    nested_error = JS_UNDEFINED;
    JS_FreeValue(ctx, funcs[0]);
    JS_FreeValue(ctx, funcs[1]);
    JS_FreeValue(ctx, promise);
    JS_FreeValue(ctx, outer);
    JS_FreeValue(ctx, inner);
    test_pool_padding_free(rt, nested_padding);
    nested_padding = NULL;
}

static void test_reaction_enqueue_failure(int mode)
{
    static const char source[] = "({ get then() { throw 42; } })";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue promise, funcs[2], children[2], resolution, result;
    void *padding;
    int i;

    assert(rt && ctx);
    promise = JS_NewPromiseCapability(ctx, funcs);
    assert(!JS_IsException(promise));
    for (i = 0; i < 2; i++) {
        children[i] = JS_PromiseThen(ctx, promise, JS_UNDEFINED, JS_UNDEFINED);
        assert(!JS_IsException(children[i]));
    }
    resolution = mode == 2 ?
        JS_Eval(ctx, source, sizeof(source) - 1, "<reaction-oom>", JS_EVAL_TYPE_GLOBAL) :
        JS_NewInt32(ctx, 42);
    assert(!JS_IsException(resolution));
    padding = test_pool_padding(rt);
    JS_SetMemoryLimit(rt, 0);
    result = JS_Call(ctx, funcs[mode == 1], JS_UNDEFINED, 1, (JSValueConst *)&resolution);
    JS_SetMemoryLimit(rt, (size_t)-1);
    assert(JS_IsException(result) && JS_HasException(ctx));
    JS_FreeValue(ctx, JS_GetException(ctx));
    assert(JS_PromiseState(ctx, promise) ==
           (mode ? JS_PROMISE_REJECTED : JS_PROMISE_FULFILLED));
    assert(!JS_IsJobPending(rt));
    /* Settlement is already published; retrying must not settle it again. */
    result = JS_Call(ctx, funcs[0], JS_UNDEFINED, 0, NULL);
    assert(JS_IsUndefined(result) && !JS_HasException(ctx));
    for (i = 0; i < 2; i++) {
        assert(JS_PromiseState(ctx, children[i]) == JS_PROMISE_PENDING);
        JS_FreeValue(ctx, children[i]);
    }
    JS_FreeValue(ctx, resolution);
    JS_FreeValue(ctx, promise);
    JS_FreeValue(ctx, funcs[0]);
    JS_FreeValue(ctx, funcs[1]);
    test_pool_padding_free(rt, padding);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_combinator_oom(const char *source)
{
    size_t step, calls = 0;

    /* Sweep construction allocations separately from the later promise jobs. */
    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue function, result;
        void *padding;
        int jobs = 0;

        assert(ctx);
        function = JS_Eval(ctx, source, strlen(source), "combinator-oom.js",
                           JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(function));
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        result = JS_Call(ctx, function, JS_UNDEFINED, 0, NULL);
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 256);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_IsException(result)) {
            assert(step);
            oom_test_check_exception(ctx);
        } else {
            assert(!JS_HasException(ctx));
            while (JS_IsJobPending(rt)) {
                assert(++jobs < 16);
                assert(JS_ExecutePendingJob(rt, NULL) > 0);
            }
            assert(JS_PromiseState(ctx, result) != JS_PROMISE_PENDING);
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, function);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

static void test_async_generator_oom(int is_return)
{
    static const char source[] = "(async function*() {})()";
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue generator, next, method, promise, value;
        JSValueConst arg = JS_NewInt32(ctx, 42);
        void *padding;
        int jobs = 0;

        assert(ctx);
        generator = JS_Eval(ctx, source, sizeof(source) - 1, "generator-oom.js",
                            JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(generator));
        next = JS_GetPropertyStr(ctx, generator, "next");
        method = JS_GetPropertyStr(ctx, generator, is_return ? "return" : "next");
        assert(!JS_IsException(next) && !JS_IsException(method));
        promise = JS_Call(ctx, next, generator, 0, NULL);
        assert(!JS_IsException(promise));
        while (JS_IsJobPending(rt)) {
            assert(++jobs < 16);
            assert(JS_ExecutePendingJob(rt, NULL) > 0);
        }
        JS_FreeValue(ctx, promise);
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        promise = JS_Call(ctx, method, generator, 1, &arg);
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 64);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_IsException(promise)) {
            assert(step);
            oom_test_check_exception(ctx);
        } else {
            assert(!JS_HasException(ctx));
            while (JS_IsJobPending(rt)) {
                assert(++jobs < 16);
                assert(JS_ExecutePendingJob(rt, NULL) > 0);
            }
            /* Failed return setup must reject, not strand a request without
               a scheduled continuation or fulfill it with JS_EXCEPTION. */
            assert(JS_PromiseState(ctx, promise) != JS_PROMISE_PENDING);
            value = JS_PromiseResult(ctx, promise);
            assert(!JS_IsException(value));
            if (JS_PromiseState(ctx, promise) == JS_PROMISE_REJECTED)
                assert(step && JS_IsError(ctx, value));
            else
                assert(JS_IsObject(value));
            JS_FreeValue(ctx, value);
        }
        JS_FreeValue(ctx, promise);
        assert(!JS_HasException(ctx));
        /* Recovery must also leave the completed generator usable. */
        promise = JS_Call(ctx, next, generator, 0, NULL);
        assert(!JS_IsException(promise));
        while (JS_IsJobPending(rt)) {
            assert(++jobs < 16);
            assert(JS_ExecutePendingJob(rt, NULL) > 0);
        }
        assert(JS_PromiseState(ctx, promise) == JS_PROMISE_FULFILLED);
        assert(!JS_HasException(ctx));
        JS_FreeValue(ctx, promise);
        JS_FreeValue(ctx, method);
        JS_FreeValue(ctx, next);
        JS_FreeValue(ctx, generator);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_RunGC(rt);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

int main(void)
{
    const char *source = "({ then(resolve) { resolve(1); } })";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue promise, funcs[2], thenable, result, error;
    void *padding;

    test_combinator_oom("(function() { return Promise.all([1]); })");
    test_combinator_oom("(function() { return Promise.allSettled([1]); })");
    test_combinator_oom("(function() { return Promise.any([1]); })");
    test_async_generator_start_throw();
    test_async_generator_oom(0);
    test_async_generator_oom(1);
    test_dynamic_import_oom(0);
    test_dynamic_import_oom(1);
    test_reaction_enqueue_failure(0);
    test_reaction_enqueue_failure(1);
    test_reaction_enqueue_failure(2);
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    promise = JS_NewPromiseCapability(ctx, funcs);
    assert(!JS_IsException(promise));
    thenable = JS_Eval(ctx, source, strlen(source), "<promise-oom>",
                       JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(thenable));
    padding = test_pool_padding(rt);
    JS_SetMemoryLimit(rt, 0);
    result = JS_Call(ctx, funcs[0], JS_UNDEFINED, 1,
                     (JSValueConst *)&thenable);
    JS_SetMemoryLimit(rt, (size_t)-1);
    assert(JS_IsException(result));
    assert(JS_HasException(ctx));
    error = JS_GetException(ctx);
    JS_FreeValue(ctx, error);
    assert(!JS_IsJobPending(rt));
    /* The failed attempt must not permanently disable this resolving pair. */
    result = JS_Call(ctx, funcs[0], JS_UNDEFINED, 1,
                     (JSValueConst *)&thenable);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, result);
    assert(JS_ExecutePendingJob(rt, NULL) == 1);
    assert(!JS_IsJobPending(rt));
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, thenable);
    JS_FreeValue(ctx, funcs[0]);
    JS_FreeValue(ctx, funcs[1]);
    JS_FreeValue(ctx, promise);
    test_pool_padding_free(rt, padding);
    test_nested(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
