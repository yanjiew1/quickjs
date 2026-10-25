/* Native opaque closures retain their creator realm and native owner.
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
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"
#include "oom-allocator.h"

typedef struct Stats {
    int calls, finalized, expected_argc, throw_next;
} Stats;

typedef struct State {
    Stats *stats;
    JSContext *creator; /* identity only; the closure owns the actual reference */
} State;

static State *new_state(Stats *stats, JSContext *creator)
{
    State *state = malloc(sizeof(*state));
    assert(state);
    state->stats = stats;
    state->creator = creator;
    return state;
}

static void finalize_state(void *opaque)
{
    State *state = opaque;
    state->stats->finalized++;
    free(state);
}

static JSValue eval(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source), "<c-closure>",
                            JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(value));
    return value;
}

static void assert_int(JSContext *ctx, JSValueConst value, int expected)
{
    int32_t number;
    assert(JS_ToInt32(ctx, &number, value) == 0 && number == expected);
}

static JSValue callback(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv, int magic, void *opaque)
{
    State *state = opaque;
    JSValue marker;
    int32_t argument = 1;
    int i;

    assert(ctx == state->creator && magic == -98765);
    assert(argc == state->stats->expected_argc);
    for (i = argc; i < 2; i++)
        assert(JS_IsUndefined(argv[i]));
    marker = eval(ctx, "globalThis.nativeRealmMarker");
    assert_int(ctx, marker, 17);
    JS_FreeValue(ctx, marker);
    state->stats->calls++;
    if (state->stats->throw_next) {
        state->stats->throw_next = 0;
        return JS_ThrowTypeError(ctx, "native closure failure");
    }
    if (argc)
        assert(JS_ToInt32(ctx, &argument, argv[0]) == 0);
    return JS_NewInt32(ctx, magic + argument);
}

static void test_calls_and_realm(void)
{
    Stats stats = { 0 };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *caller, *creator, *job_ctx = NULL;
    JSValue function, global, bound, proxy, result, error, stack;
    JSValue source, chained, value;
    const char *text;
    State *state;

    assert(rt);
    caller = JS_NewContext(rt);
    creator = JS_NewContext(rt);
    assert(caller && creator);
    value = eval(creator, "globalThis.nativeRealmMarker = 17");
    JS_FreeValue(creator, value);
    state = new_state(&stats, creator);
    function = JS_NewCClosure(creator, callback, "nativeClosure", finalize_state,
                              2, -98765, state);
    assert(!JS_IsException(function) && JS_IsFunction(caller, function));
    assert(!JS_IsConstructor(caller, function));
    result = JS_Call(caller, function, JS_UNDEFINED, 0, NULL);
    assert_int(caller, result, -98764);
    JS_FreeValue(caller, result);
    global = JS_GetGlobalObject(caller);
    assert(JS_SetPropertyStr(caller, global, "native",
                             JS_DupValue(caller, function)) >= 0);
    bound = eval(caller, "native.bind(null)");
    proxy = eval(caller, "new Proxy(native, {})");
    result = JS_Call(caller, bound, JS_UNDEFINED, 0, NULL);
    assert_int(caller, result, -98764);
    JS_FreeValue(caller, result);
    result = JS_Call(caller, proxy, JS_UNDEFINED, 0, NULL);
    assert_int(caller, result, -98764);
    JS_FreeValue(caller, result);

    stats.expected_argc = 1;
    source = JS_NewSettledPromise(caller, 0, JS_NewInt32(caller, 41));
    assert(!JS_IsException(source));
    chained = JS_PromiseThen(caller, source, function, JS_UNDEFINED);
    assert(!JS_IsException(chained));
    /* Promise reaction jobs capture their handler's realm when enqueued. */
    assert(JS_ExecutePendingJob(rt, &job_ctx) == 1 && job_ctx == creator);
    assert(!JS_IsJobPending(rt));
    assert(JS_PromiseState(caller, chained) == JS_PROMISE_FULFILLED);
    value = JS_PromiseResult(caller, chained);
    assert_int(caller, value, -98724);
    JS_FreeValue(caller, value);
    JS_FreeValue(caller, chained);
    JS_FreeValue(caller, source);

    stats.expected_argc = 0;
    stats.throw_next = 1;
    result = JS_Call(caller, function, JS_UNDEFINED, 0, NULL);
    assert(JS_IsException(result));
    error = JS_GetException(caller);
    stack = JS_GetPropertyStr(caller, error, "stack");
    text = JS_ToCString(caller, stack);
    assert(text && strstr(text, "nativeClosure"));
    JS_FreeCString(caller, text);
    JS_FreeValue(caller, stack);
    JS_FreeValue(caller, error);
    JS_ThrowTypeError(caller, "outside the callback");
    error = JS_GetException(caller);
    stack = JS_GetPropertyStr(caller, error, "stack");
    text = JS_ToCString(caller, stack);
    assert(text && !strstr(text, "nativeClosure"));
    JS_FreeCString(caller, text);
    JS_FreeValue(caller, stack);
    JS_FreeValue(caller, error);

    JS_FreeContext(creator);
    JS_RunGC(rt);
    result = JS_Call(caller, function, JS_UNDEFINED, 0, NULL);
    assert_int(caller, result, -98764);
    JS_FreeValue(caller, result);
    assert(stats.calls == 6);
    assert(JS_SetPropertyStr(caller, global, "native", JS_UNDEFINED) >= 0);
    JS_FreeValue(caller, global);
    JS_FreeValue(caller, proxy);
    JS_FreeValue(caller, bound);
    JS_FreeValue(caller, function);
    assert(stats.finalized == 1);
    JS_FreeContext(caller);
    JS_FreeRuntime(rt);
}

static void test_creator_cycle(void)
{
    Stats stats = { 0 };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue function, global;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    function = JS_NewCClosure(ctx, callback, "cycle", finalize_state,
                              2, -98765, new_state(&stats, ctx));
    assert(!JS_IsException(function));
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "cycle", function) >= 0);
    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_RunGC(rt);
    assert(stats.finalized == 1);
    JS_FreeRuntime(rt);
}

static void *exhaust_pooled_blocks(JSRuntime *rt)
{
    void *padding = NULL, *ptr;
    size_t size;

    JS_SetMemoryLimit(rt, 0);
    for (size = sizeof(void *); size <= 512; size += sizeof(void *)) {
        while ((ptr = js_malloc_rt(rt, size)) != NULL) {
            memcpy(ptr, &padding, sizeof(padding));
            padding = ptr;
        }
    }
    JS_SetMemoryLimit(rt, (size_t)-1);
    return padding;
}

static void test_construction_oom(void)
{
    size_t fail_at;
    int failures = 0, successes = 0;

    for (fail_at = 1; fail_at <= 64; fail_at++) {
        OOMTestAllocator allocator;
        Stats stats = { 0 };
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        State *state;
        JSValue function;
        void *padding;

        assert(ctx);
        state = new_state(&stats, ctx);
        padding = exhaust_pooled_blocks(rt);
        oom_test_begin(&allocator, fail_at);
        function = JS_NewCClosure(ctx, callback, "fresh opaque callback name",
                                  finalize_state, 2, -98765, state);
        oom_test_end(&allocator);
        if (JS_IsException(function)) {
            JSValue error;

            failures++;
            assert(allocator.failures == 1 && JS_HasException(ctx));
            error = JS_GetException(ctx);
            JS_FreeValue(ctx, error);
            assert(stats.finalized == 0);
            free(state);
        } else {
            successes++;
            assert(!JS_HasException(ctx) && stats.finalized == 0);
            JS_FreeValue(ctx, function);
            assert(stats.finalized == 1);
        }
        while (padding) {
            void *next;

            memcpy(&next, padding, sizeof(next));
            js_free_rt(rt, padding);
            padding = next;
        }
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
    assert(failures > 0 && successes > 0);
}

static void test_validation_and_stack_limit(void)
{
    Stats stats = { 0 };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    State *state;
    JSValue function, error, result;
    int lengths[] = { -1, 65536 };
    size_t i;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    state = new_state(&stats, ctx);
    for (i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        function = JS_NewCClosure(ctx, callback, NULL, finalize_state,
                                  lengths[i], -98765, state);
        assert(JS_IsException(function) && stats.finalized == 0);
        error = JS_GetException(ctx);
        JS_FreeValue(ctx, error);
    }
    function = JS_NewCClosure(ctx, NULL, NULL, finalize_state, 0, 0, state);
    assert(JS_IsException(function) && stats.finalized == 0);
    error = JS_GetException(ctx);
    JS_FreeValue(ctx, error);
    function = JS_NewCClosure(ctx, callback, NULL, finalize_state,
                              65535, -98765, state);
    assert(!JS_IsException(function));
    JS_SetMaxStackSize(rt, 1024);
    result = JS_Call(ctx, function, JS_UNDEFINED, 0, NULL);
    assert(JS_IsException(result) && stats.calls == 0);
    JS_SetMaxStackSize(rt, 0);
    error = JS_GetException(ctx);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, function);
    assert(stats.finalized == 1);
    state = new_state(&stats, ctx);
    function = JS_NewCClosure(ctx, callback, NULL, NULL, 0, 0, state);
    assert(!JS_IsException(function));
    JS_FreeValue(ctx, function);
    assert(state->stats == &stats && stats.finalized == 1);
    free(state);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main(void)
{
    test_calls_and_realm();
    test_creator_cycle();
    test_construction_oom();
    test_validation_and_stack_limit();
    return 0;
}
