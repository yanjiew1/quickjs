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
static void *assimilation_padding;
static int assimilation_armed;
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

static JSModuleDef *reject_import_loader(JSContext *ctx, const char *name,
                                         void *opaque)
{
    JS_Throw(ctx, JS_NewInt32(ctx, 42));
    return NULL;
}

static void test_dynamic_import_reject_job_oom(void)
{
    static const char source[] =
        "globalThis.p = import('missing'); p.catch(() => {});";
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue result;
        void *padding;
        int ret;

        assert(ctx);
        JS_SetModuleLoaderFunc(rt, NULL, reject_import_loader, NULL);
        result = JS_Eval(ctx, source, sizeof(source) - 1,
                         "import-reject-oom.js", JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(result));
        JS_FreeValue(ctx, result);
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        ret = JS_ExecutePendingJob(rt, NULL);
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(calls > 0 && calls < 16);
        } else {
            assert(allocator.failures == 1);
        }
        assert(ret == 1 || (step && ret == -1));
        assert((ret == -1) == !!JS_HasException(ctx));
        if (ret == -1)
            JS_FreeValue(ctx, JS_GetException(ctx));
        while (JS_IsJobPending(rt)) {
            assert(JS_ExecutePendingJob(rt, NULL) == 1);
            assert(!JS_HasException(ctx));
        }
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
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

static void test_reaction_scheduling_no_allocation(int mode)
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
    assert(JS_IsUndefined(result) && !JS_HasException(ctx));
    assert(JS_PromiseState(ctx, promise) ==
           (mode ? JS_PROMISE_REJECTED : JS_PROMISE_FULFILLED));
    assert(JS_IsJobPending(rt));
    /* Settlement is already published; retrying must not settle it again. */
    result = JS_Call(ctx, funcs[0], JS_UNDEFINED, 0, NULL);
    assert(JS_IsUndefined(result) && !JS_HasException(ctx));
    for (i = 0; JS_IsJobPending(rt); i++) {
        assert(i < 8 && JS_ExecutePendingJob(rt, NULL) == 1);
    }
    assert(i == 2);
    for (i = 0; i < 2; i++) {
        assert(JS_PromiseState(ctx, children[i]) ==
               (mode ? JS_PROMISE_REJECTED : JS_PROMISE_FULFILLED));
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

static void test_with_resolvers_oom(void)
{
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue global, ctor, method, result;
        void *padding;

        assert(ctx);
        global = JS_GetGlobalObject(ctx);
        ctor = JS_GetPropertyStr(ctx, global, "Promise");
        method = JS_GetPropertyStr(ctx, ctor, "withResolvers");
        assert(!JS_IsException(global) && !JS_IsException(ctor));
        assert(!JS_IsException(method));
        padding = test_pool_padding(rt);

        oom_test_begin(&allocator, step);
        result = JS_Call(ctx, method, ctor, 0, NULL);
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
            assert(JS_IsObject(result) && !JS_HasException(ctx));
        }

        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, method);
        JS_FreeValue(ctx, ctor);
        JS_FreeValue(ctx, global);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

static void test_async_from_sync_return_oom(void)
{
    static const char source[] =
        "({ [Symbol.iterator]() { return this; },"
        "   next() { return { done: true }; } })";
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue sync, async, method, result;
        void *padding;

        assert(ctx);
        sync = JS_Eval(ctx, source, sizeof(source) - 1,
                       "async-from-sync-oom.js", JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(sync));
        async = JS_GetIterator(ctx, sync, 1);
        assert(!JS_IsException(async));
        method = JS_GetPropertyStr(ctx, async, "return");
        assert(!JS_IsException(method));
        padding = test_pool_padding(rt);

        oom_test_begin(&allocator, step);
        result = JS_Call(ctx, method, async, 0, NULL);
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
        }

        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, method);
        JS_FreeValue(ctx, async);
        JS_FreeValue(ctx, sync);
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

static void *module_stack_padding;
static int module_freeze_calls, module_graph_mode;

static JSValue exhaust_module_stack(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    module_freeze_calls++;
    module_stack_padding = test_pool_padding(rt);
    JS_SetMaxStackSize(rt, 1);
    JS_SetMemoryLimit(rt, 0);
    return JS_UNDEFINED;
}

static JSModuleDef *load_stack_module(JSContext *ctx, const char *name,
                                      void *opaque)
{
    static const char dependency[] = "await 0;";
    static const char parent[] = "import 'dep'; exhaustModuleStack(); await 0;";
    static const char diamond_parent[] = "import 'mid'; await 0;";
    static const char diamond_middle[] = "import 'leaf'; exhaustModuleStack(); await 0;";
    const char *source;

    if (module_graph_mode == 2) {
        if (!strcmp(name, "leaf"))
            source = dependency;
        else if (!strcmp(name, "mid"))
            source = diamond_middle;
        else
            source = diamond_parent;
    } else {
        source = !strcmp(name, "dep") ? dependency : parent;
    }
    JSValue compiled = JS_Eval(ctx, source, strlen(source), name,
                               JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    JSModuleDef *module;

    if (JS_IsException(compiled))
        return NULL;
    module = JS_VALUE_GET_PTR(compiled);
    JS_FreeValue(ctx, compiled);
    return module;
}

static void test_async_module_late_setup_failure(int graph_mode)
{
    static const char root[] = "import 'dep'; exhaustModuleStack(); await 0;";
    static const char rooted_parent[] = "import 'parent'; await 0;";
    static const char diamond[] = "import 'left'; import 'right'; await 0;";
    const char *source = graph_mode == 2 ? diamond :
                         graph_mode == 1 ? rooted_parent : root;
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue global, method, compiled, result, error;
    int jobs = 0, ret;

    assert(rt && ctx);
    JS_SetModuleLoaderFunc(rt, NULL, load_stack_module, NULL);
    global = JS_GetGlobalObject(ctx);
    method = JS_NewCFunction(ctx, exhaust_module_stack, "exhaustModuleStack", 0);
    assert(!JS_IsException(method));
    assert(JS_SetPropertyStr(ctx, global, "exhaustModuleStack", method) >= 0);
    JS_FreeValue(ctx, global);
    module_stack_padding = NULL;
    module_freeze_calls = 0;
    module_graph_mode = graph_mode;
    compiled = JS_Eval(ctx, source, strlen(source), "stack-root",
                       JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    result = JS_EvalFunction(ctx, compiled);
    assert(!JS_IsException(result));
    while (JS_IsJobPending(rt)) {
        assert(++jobs < 24);
        ret = JS_ExecutePendingJob(rt, NULL);
        if (module_freeze_calls) {
            JS_SetMemoryLimit(rt, (size_t)-1);
            JS_SetMaxStackSize(rt, 0);
            test_pool_padding_free(rt, module_stack_padding);
            module_stack_padding = NULL;
        }
        if (ret < 0)
            JS_FreeValue(ctx, JS_GetException(ctx));
    }
    assert(module_freeze_calls == 1);
    /* Every async ancestor must settle even when no new job can be made. */
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_REJECTED);
    error = JS_PromiseResult(ctx, result);
    assert(JS_IsNull(error) || JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    if (graph_mode == 2) {
        const char *names[] = { "left", "right", "mid" };
        int i;

        for (i = 0; i < 3; i++) {
            JSValue parent = JS_LoadModule(ctx, "stack-root", names[i]);

            assert(!JS_IsException(parent));
            while (JS_IsJobPending(rt)) {
                assert(++jobs < 32);
                assert(JS_ExecutePendingJob(rt, NULL) > 0);
            }
            assert(JS_PromiseState(ctx, parent) == JS_PROMISE_REJECTED);
            JS_FreeValue(ctx, parent);
        }
        assert(module_freeze_calls == 1);
    }
    JS_FreeValue(ctx, result);
    JS_FreeContext(ctx);
    JS_RunGC(rt);
    JS_FreeRuntime(rt);
}

static void test_capability_failure_outputs(void)
{
    int fail_at, failures = 0;

    for (fail_at = 1; fail_at <= 16; fail_at++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue funcs[2] = { JS_UNDEFINED, JS_UNDEFINED }, promise;
        void *padding;

        assert(ctx);
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, fail_at);
        promise = JS_NewPromiseCapability(ctx, funcs);
        oom_test_end(&allocator);
        if (JS_IsException(promise)) {
            failures++;
            /* A caller may embed these slots in a marked object. A failed
               capability must not publish partially constructed resolvers. */
            assert(JS_IsUndefined(funcs[0]) && JS_IsUndefined(funcs[1]));
            JS_FreeValue(ctx, JS_GetException(ctx));
        }
        JS_FreeValue(ctx, funcs[0]);
        JS_FreeValue(ctx, funcs[1]);
        JS_FreeValue(ctx, promise);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
    assert(failures > 0);
}

static void test_thenable_job_resolver_oom(void)
{
    static const char source[] = "Promise.resolve({ then(resolve) { resolve(1); } })";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue promise, reason;
    void *padding;

    assert(rt && ctx);
    promise = JS_Eval(ctx, source, sizeof(source) - 1,
                      "<thenable-job-oom>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(promise));
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_PENDING);
    assert(JS_IsJobPending(rt));
    padding = test_pool_padding(rt);
    JS_SetMemoryLimit(rt, 0);
    assert(JS_ExecutePendingJob(rt, NULL) == 1);
    JS_SetMemoryLimit(rt, (size_t)-1);
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_REJECTED);
    reason = JS_PromiseResult(ctx, promise);
    assert(JS_IsError(ctx, reason) || JS_IsNull(reason));
    JS_FreeValue(ctx, reason);
    JS_FreeValue(ctx, promise);
    test_pool_padding_free(rt, padding);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_combinator_reaction_oom(void)
{
    static const char *const sources[] = {
        "Promise.all([Promise.resolve(1)])",
        "Promise.allSettled([Promise.resolve(1)])",
        "Promise.any([Promise.reject(1)])",
    };
    size_t i;

    for (i = 0; i < sizeof(sources) / sizeof(sources[0]); i++) {
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx = JS_NewContext(rt);
        JSValue promise, reason;
        void *padding;
        int ret;

        assert(rt && ctx);
        promise = JS_Eval(ctx, sources[i], strlen(sources[i]),
                          "<combinator-reaction-oom>", JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(promise));
        assert(JS_PromiseState(ctx, promise) == JS_PROMISE_PENDING);
        padding = test_pool_padding(rt);
        JS_SetMemoryLimit(rt, 0);
        ret = JS_ExecutePendingJob(rt, NULL);
        JS_SetMemoryLimit(rt, (size_t)-1);
        assert(ret == 1);
        assert(JS_PromiseState(ctx, promise) == JS_PROMISE_REJECTED);
        reason = JS_PromiseResult(ctx, promise);
        assert(JS_IsError(ctx, reason) || JS_IsNull(reason));
        JS_FreeValue(ctx, reason);
        JS_FreeValue(ctx, promise);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
}

static JSValue arm_assimilation_oom(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    assimilation_armed = 1;
    assimilation_padding = test_pool_padding(rt);
    JS_SetMemoryLimit(rt, 0);
    return JS_UNDEFINED;
}

static void test_combinator_final_resolve_oom(void)
{
    static const char source[] =
        "var cachedThen = function(resolve) { resolve(1); };"
        "Object.defineProperty(Object.prototype, 'then', { configurable: true,"
        "  get() { if (Array.isArray(this)) {"
        "    armAssimilationOOM(); return cachedThen; } } });"
        "Promise.all([Promise.resolve(1)])";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue global, promise, reason;

    assert(rt && ctx);
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "armAssimilationOOM",
               JS_NewCFunction(ctx, arm_assimilation_oom, "armAssimilationOOM", 0)) > 0);
    assimilation_padding = NULL;
    assimilation_armed = 0;
    promise = JS_Eval(ctx, source, sizeof(source) - 1,
                      "<combinator-resolve-oom>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(promise) && JS_IsJobPending(rt));
    assert(JS_ExecutePendingJob(rt, NULL) == 1);
    JS_SetMemoryLimit(rt, (size_t)-1);
    assert(assimilation_armed);
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_REJECTED);
    reason = JS_PromiseResult(ctx, promise);
    assert(JS_IsError(ctx, reason) || JS_IsNull(reason));
    JS_FreeValue(ctx, reason);
    JS_FreeValue(ctx, promise);
    JS_FreeValue(ctx, global);
    test_pool_padding_free(rt, assimilation_padding);
    assimilation_padding = NULL;
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_custom_combinator_resolve_throw(void)
{
    static const char source[] =
        "globalThis.rejectCalls = 0;"
        "class P extends Promise {"
        "  constructor(executor) {"
        "    super((resolve, reject) => executor("
        "      () => { throw 42; },"
        "      reason => { rejectCalls++; reject(reason); }));"
        "  }"
        "  static resolve(value) { return Promise.resolve(value); }"
        "}"
        "P.all([1])";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue promise, count;

    assert(rt && ctx);
    promise = JS_Eval(ctx, source, sizeof(source) - 1,
                      "<custom-combinator>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(promise) && JS_IsJobPending(rt));
    assert(JS_ExecutePendingJob(rt, NULL) == 1);
    count = JS_Eval(ctx, "rejectCalls", 11, "<custom-combinator>",
                    JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(count) && JS_VALUE_GET_INT(count) == 0);
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_PENDING);
    JS_FreeValue(ctx, count);
    JS_FreeValue(ctx, promise);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_async_function_return_oom(void)
{
    static const char source[] =
        "var cachedThen = function(resolve) { resolve(1); };"
        "var thenable = { get then() { armAssimilationOOM(); return cachedThen; } };"
        "(async function() { return thenable; })()";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue global, promise, reason;

    assert(rt && ctx);
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "armAssimilationOOM",
               JS_NewCFunction(ctx, arm_assimilation_oom, "armAssimilationOOM", 0)) > 0);
    assimilation_padding = NULL;
    assimilation_armed = 0;
    promise = JS_Eval(ctx, source, sizeof(source) - 1,
                      "<async-return-oom>", JS_EVAL_TYPE_GLOBAL);
    JS_SetMemoryLimit(rt, (size_t)-1);
    assert(assimilation_armed);
    assert(!JS_IsException(promise));
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_REJECTED);
    reason = JS_PromiseResult(ctx, promise);
    assert(JS_IsError(ctx, reason) || JS_IsNull(reason));
    JS_FreeValue(ctx, reason);
    JS_FreeValue(ctx, promise);
    JS_FreeValue(ctx, global);
    test_pool_padding_free(rt, assimilation_padding);
    assimilation_padding = NULL;
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_async_generator_result_oom(void)
{
    static const char source[] =
        "var cachedThen = function(resolve) { resolve(1); };"
        "Object.defineProperty(Object.prototype, 'then', { configurable: true,"
        "  get() { if (Object.hasOwn(this, 'value') && Object.hasOwn(this, 'done')) {"
        "    armAssimilationOOM(); return cachedThen; } } });"
        "(async function*() { yield 1; })().next()";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue global, promise, reason;
    int ret, i;

    assert(rt && ctx);
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "armAssimilationOOM",
               JS_NewCFunction(ctx, arm_assimilation_oom, "armAssimilationOOM", 0)) > 0);
    assimilation_padding = NULL;
    assimilation_armed = 0;
    promise = JS_Eval(ctx, source, sizeof(source) - 1,
                      "<async-generator-result-oom>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(promise));
    for (i = 0; i < 8 && !assimilation_armed && JS_IsJobPending(rt); i++) {
        ret = JS_ExecutePendingJob(rt, NULL);
        assert(ret == 1);
    }
    JS_SetMemoryLimit(rt, (size_t)-1);
    assert(assimilation_armed);
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_REJECTED);
    reason = JS_PromiseResult(ctx, promise);
    assert(JS_IsError(ctx, reason) || JS_IsNull(reason));
    JS_FreeValue(ctx, reason);
    JS_FreeValue(ctx, promise);
    JS_FreeValue(ctx, global);
    test_pool_padding_free(rt, assimilation_padding);
    assimilation_padding = NULL;
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main(void)
{
    const char *source = "({ then(resolve) { resolve(1); } })";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue promise, funcs[2], thenable, result, error;
    void *padding;

    test_capability_failure_outputs();
    test_combinator_oom("(function() { return Promise.all([1]); })");
    test_combinator_oom("(function() { return Promise.allSettled([1]); })");
    test_combinator_oom("(function() { return Promise.any([1]); })");
    test_with_resolvers_oom();
    test_async_from_sync_return_oom();
    test_async_module_late_setup_failure(0);
    test_async_module_late_setup_failure(1);
    test_async_module_late_setup_failure(2);
    test_async_generator_start_throw();
    test_async_generator_oom(0);
    test_async_generator_oom(1);
    test_dynamic_import_oom(0);
    test_dynamic_import_oom(1);
    test_dynamic_import_reject_job_oom();
    test_reaction_scheduling_no_allocation(0);
    test_reaction_scheduling_no_allocation(1);
    test_reaction_scheduling_no_allocation(2);
    test_thenable_job_resolver_oom();
    test_combinator_reaction_oom();
    test_combinator_final_resolve_oom();
    test_custom_combinator_resolve_throw();
    test_async_function_return_oom();
    test_async_generator_result_oom();
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
