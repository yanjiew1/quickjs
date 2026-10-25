/* Native Promise integration contracts.
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

static JSValue eval(JSContext *ctx, const char *source)
{
    JSValue result = JS_Eval(ctx, source, strlen(source), "<promise-api>",
                             JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(result));
    return result;
}

static void drain_jobs(JSRuntime *rt)
{
    int remaining = 1000;

    while (JS_IsJobPending(rt)) {
        assert(remaining-- > 0);
        assert(JS_ExecutePendingJob(rt, NULL) > 0);
    }
}

typedef struct TrackerState {
    JSAtom then_atom;
    JSValue handler;
    int rejected, handled, reentered;
} TrackerState;

static void rejection_tracker(JSContext *ctx, JSValueConst promise,
                              JSValueConst reason, JS_BOOL handled, void *opaque)
{
    TrackerState *state = opaque;

    if (!handled) {
        state->rejected++;
    } else {
        state->handled++;
        if (!state->reentered) {
            JSValueConst args[2] = { JS_UNDEFINED, state->handler };
            JSValue nested;

            state->reentered = 1;
            nested = JS_Invoke(ctx, promise, state->then_atom, 2, args);
            assert(!JS_IsException(nested));
            JS_FreeValue(ctx, nested);
        }
    }
}

static void test_reentrant_rejection_tracker(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    TrackerState state = { 0 };
    JSValue source, result;
    JSValueConst args[2];

    state.then_atom = JS_NewAtom(ctx, "then");
    assert(state.then_atom != JS_ATOM_NULL);
    state.handler = eval(ctx, "(reason) => undefined");
    JS_SetHostPromiseRejectionTracker(rt, rejection_tracker, &state);
    source = eval(ctx, "Promise.reject(42)");
    assert(state.rejected == 1);
    args[0] = JS_UNDEFINED;
    args[1] = state.handler;
    result = JS_Invoke(ctx, source, state.then_atom, 2, args);
    assert(!JS_IsException(result));
    assert(state.handled == 1 && state.reentered == 1);
    drain_jobs(rt);
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_FULFILLED);
    assert(state.rejected == 1 && state.handled == 1);
    JS_SetHostPromiseRejectionTracker(rt, NULL, NULL);
    JS_FreeValue(ctx, source);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, state.handler);
    JS_FreeAtom(ctx, state.then_atom);
}

static void assert_promise_int(JSContext *ctx, JSValueConst promise,
                               JSPromiseStateEnum state, int expected)
{
    JSValue result;
    int32_t number;

    assert(JS_PromiseState(ctx, promise) == state);
    result = JS_PromiseResult(ctx, promise);
    assert(JS_ToInt32(ctx, &number, result) == 0 && number == expected);
    JS_FreeValue(ctx, result);
}

static void test_native_then(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSValue source, handler, result, forwarded, value, funcs[2];
    const char *invalid[] = {
        "undefined", "null", "42",
        "({ get then() { throw 1; }, get constructor() { throw 2; } })",
        "new Proxy(Promise.resolve(1), { get() { throw 3; } })",
        ("(() => { let r = Proxy.revocable(Promise.resolve(1), {});"
         " r.revoke(); return r.proxy; })()"),
    };
    size_t i;

    source = eval(ctx,
        "(() => { let p = Promise.resolve(41);"
        " Object.defineProperties(p, {"
        "   then: { get() { throw 10; } },"
        "   constructor: { get() { throw 11; } } });"
        " return p; })()");
    handler = eval(ctx, "value => value + 1");
    result = JS_PromiseThen(ctx, source, handler, JS_UNDEFINED);
    assert(!JS_IsException(result));
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_PENDING);
    forwarded = JS_PromiseThen(ctx, source, JS_NULL, JS_NULL);
    assert(!JS_IsException(forwarded));
    drain_jobs(rt);
    assert_promise_int(ctx, result, JS_PROMISE_FULFILLED, 42);
    assert_promise_int(ctx, forwarded, JS_PROMISE_FULFILLED, 41);
    assert_promise_int(ctx, source, JS_PROMISE_FULFILLED, 41);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, forwarded);
    JS_FreeValue(ctx, handler);

    handler = eval(ctx, "value => { throw value + 2; }");
    result = JS_PromiseThen(ctx, source, handler, JS_UNDEFINED);
    assert(!JS_IsException(result));
    drain_jobs(rt);
    assert_promise_int(ctx, result, JS_PROMISE_REJECTED, 43);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, handler);
    JS_FreeValue(ctx, source);

    source = eval(ctx, "Promise.reject(17)");
    result = JS_PromiseThen(ctx, source, JS_UNDEFINED, JS_UNDEFINED);
    handler = eval(ctx, "reason => reason + 1");
    forwarded = JS_PromiseThen(ctx, source, JS_UNDEFINED, handler);
    assert(!JS_IsException(result) && !JS_IsException(forwarded));
    drain_jobs(rt);
    assert_promise_int(ctx, result, JS_PROMISE_REJECTED, 17);
    assert_promise_int(ctx, forwarded, JS_PROMISE_FULFILLED, 18);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, forwarded);
    JS_FreeValue(ctx, handler);
    JS_FreeValue(ctx, source);

    source = JS_NewPromiseCapability(ctx, funcs);
    assert(!JS_IsException(source));
    result = JS_PromiseThen(ctx, source, JS_UNDEFINED, JS_UNDEFINED);
    assert(!JS_IsException(result));
    value = JS_NewInt32(ctx, 19);
    forwarded = JS_Call(ctx, funcs[1], JS_UNDEFINED, 1, (JSValueConst *)&value);
    assert(!JS_IsException(forwarded));
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_PENDING);
    drain_jobs(rt);
    assert_promise_int(ctx, result, JS_PROMISE_REJECTED, 19);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, forwarded);
    JS_FreeValue(ctx, funcs[0]);
    JS_FreeValue(ctx, funcs[1]);
    JS_FreeValue(ctx, source);

    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        const char *name;

        source = eval(ctx, invalid[i]);
        result = JS_PromiseThen(ctx, source, JS_UNDEFINED, JS_UNDEFINED);
        assert(JS_IsException(result));
        value = JS_GetException(ctx);
        handler = JS_GetPropertyStr(ctx, value, "name");
        name = JS_ToCString(ctx, handler);
        assert(name && !strcmp(name, "TypeError"));
        JS_FreeCString(ctx, name);
        JS_FreeValue(ctx, handler);
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, source);
    }
}

static void test_native_then_realms(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSContext *other = JS_NewContext(rt);
    JSValue source, handler, result, prototype, expected, global, saved, wrapped;

    assert(other);
    source = eval(other, "Promise.resolve(41)");
    handler = eval(other,
        "value => { if (globalThis.foreignMarker !== 17) throw 0;"
        " return value + 1; }");
    saved = eval(other, "globalThis.foreignMarker = 17");
    JS_FreeValue(other, saved);
    expected = eval(ctx, "Promise.prototype");
    saved = eval(ctx, "Promise");
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "Promise", JS_NULL) >= 0);
    result = JS_PromiseThen(ctx, source, handler, JS_UNDEFINED);
    assert(!JS_IsException(result));
    assert(JS_SetPropertyStr(ctx, global, "Promise", saved) >= 0);
    prototype = JS_GetPrototype(ctx, result);
    assert(JS_IsObject(prototype) && JS_IsObject(expected));
    assert(JS_VALUE_GET_PTR(prototype) == JS_VALUE_GET_PTR(expected));
    wrapped = JS_NewSettledPromise(ctx, 0, source);
    assert(!JS_IsException(wrapped));
    assert(JS_VALUE_GET_PTR(wrapped) != JS_VALUE_GET_PTR(source));
    drain_jobs(rt);
    assert_promise_int(ctx, result, JS_PROMISE_FULFILLED, 42);
    assert_promise_int(ctx, wrapped, JS_PROMISE_FULFILLED, 41);
    JS_FreeValue(ctx, wrapped);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, prototype);
    JS_FreeValue(ctx, expected);
    JS_FreeValue(ctx, result);
    JS_FreeValue(other, source);
    JS_FreeValue(other, handler);
    JS_FreeContext(other);
}

static void counting_tracker(JSContext *ctx, JSValueConst promise,
                             JSValueConst reason, JS_BOOL handled, void *opaque)
{
    int *counts = opaque;

    counts[!!handled]++;
    if (handled)
        JS_PromiseMarkAsHandled(ctx, promise);
}

static void test_mark_as_handled(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    int counts[2] = { 0 };
    JSValue promise, funcs[2], result, invalid;

    JS_SetHostPromiseRejectionTracker(rt, counting_tracker, counts);
    promise = JS_NewPromiseCapability(ctx, funcs);
    assert(!JS_IsException(promise));
    JS_PromiseMarkAsHandled(ctx, promise);
    result = JS_Call(ctx, funcs[1], JS_UNDEFINED, 0, NULL);
    assert(!JS_IsException(result));
    assert(counts[0] == 0 && counts[1] == 0);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, funcs[0]);
    JS_FreeValue(ctx, funcs[1]);
    JS_FreeValue(ctx, promise);

    promise = eval(ctx, "Promise.reject(23)");
    assert(counts[0] == 1 && counts[1] == 0);
    result = JS_PromiseResult(ctx, promise);
    JS_FreeValue(ctx, result);
    assert(counts[1] == 0);
    JS_PromiseMarkAsHandled(ctx, promise);
    JS_PromiseMarkAsHandled(ctx, promise);
    assert(counts[0] == 1 && counts[1] == 1);
    assert_promise_int(ctx, promise, JS_PROMISE_REJECTED, 23);
    JS_FreeValue(ctx, promise);

    invalid = eval(ctx, "new Proxy({}, { get() { throw 1; } })");
    JS_PromiseMarkAsHandled(ctx, invalid);
    JS_PromiseMarkAsHandled(ctx, JS_NULL);
    assert(!JS_HasException(ctx));
    assert(counts[0] == 1 && counts[1] == 1);
    JS_FreeValue(ctx, invalid);
    JS_SetHostPromiseRejectionTracker(rt, NULL, NULL);
}

static void test_settled_promise(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSValue input, result, value;

    input = eval(ctx, "Promise.resolve(7)");
    result = JS_NewSettledPromise(ctx, 0, input);
    assert(!JS_IsException(result));
    assert(JS_VALUE_GET_PTR(result) == JS_VALUE_GET_PTR(input));
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, input);

    input = eval(ctx,
        "({ get then() { throw 29; }, get constructor() { throw 30; } })");
    result = JS_NewSettledPromise(ctx, 0, input);
    assert_promise_int(ctx, result, JS_PROMISE_REJECTED, 29);
    JS_FreeValue(ctx, result);
    result = JS_NewSettledPromise(ctx, 7, input);
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_REJECTED);
    value = JS_PromiseResult(ctx, result);
    assert(JS_VALUE_GET_PTR(value) == JS_VALUE_GET_PTR(input));
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, input);

    input = eval(ctx, "({ then(resolve) { resolve(31); } })");
    result = JS_NewSettledPromise(ctx, 0, input);
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_PENDING);
    drain_jobs(rt);
    assert_promise_int(ctx, result, JS_PROMISE_FULFILLED, 31);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, input);

    input = eval(ctx,
        "Object.defineProperty(Promise.resolve(1), 'constructor',"
        " { get() { throw 32; } })");
    result = JS_NewSettledPromise(ctx, 0, input);
    assert(JS_IsException(result));
    value = JS_GetException(ctx);
    assert(JS_IsNumber(value) && JS_VALUE_GET_INT(value) == 32);
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, input);
}

/* Keep spare arena blocks live so memory limits also exercise pooled builds. */
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

static void test_native_promise_oom(void)
{
    int mode;

    for (mode = 0; mode < 4; mode++) {
        int failures = 0, successes = 0;
        size_t budget;

        for (budget = 0; budget <= 4096 + 128; budget += 128) {
            JSRuntime *rt = JS_NewRuntime();
            JSContext *ctx;
            JSMemoryUsage usage;
            JSValue source, funcs[2], result, value;
            void *padding;

            assert(rt);
            ctx = JS_NewContext(rt);
            assert(ctx);
            funcs[0] = funcs[1] = JS_UNDEFINED;
            if (mode < 2) {
                source = JS_NewPromiseCapability(ctx, funcs);
                assert(!JS_IsException(source));
                if (mode == 1) {
                    result = JS_Call(ctx, funcs[0], JS_UNDEFINED, 0, NULL);
                    assert(!JS_IsException(result));
                    JS_FreeValue(ctx, result);
                }
            } else {
                source = eval(ctx, "({ then(resolve) { resolve(37); } })");
            }
            padding = exhaust_pooled_blocks(rt);
            JS_ComputeMemoryUsage(rt, &usage);
            JS_SetMemoryLimit(rt, budget <= 4096 ?
                              usage.malloc_size + budget : (size_t)-1);
            if (mode < 2)
                result = JS_PromiseThen(ctx, source, JS_UNDEFINED, JS_UNDEFINED);
            else
                result = JS_NewSettledPromise(ctx, mode == 3, source);
            JS_SetMemoryLimit(rt, (size_t)-1);
            if (JS_IsException(result)) {
                failures++;
                assert(JS_HasException(ctx));
                value = JS_GetException(ctx);
                JS_FreeValue(ctx, value);
            } else {
                successes++;
            }
            assert(!JS_HasException(ctx));
            if (mode == 0) {
                value = JS_Call(ctx, funcs[0], JS_UNDEFINED, 0, NULL);
                assert(!JS_IsException(value));
                JS_FreeValue(ctx, value);
            }
            drain_jobs(rt);
            if (!JS_IsException(result))
                assert(JS_PromiseState(ctx, result) ==
                       (mode == 3 ? JS_PROMISE_REJECTED : JS_PROMISE_FULFILLED));
            JS_FreeValue(ctx, result);
            JS_FreeValue(ctx, funcs[0]);
            JS_FreeValue(ctx, funcs[1]);
            JS_FreeValue(ctx, source);
            while (padding) {
                void *next;

                memcpy(&next, padding, sizeof(next));
                js_free_rt(rt, padding);
                padding = next;
            }
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);
        }
        assert(failures > 0 && successes > 0);
    }
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    test_reentrant_rejection_tracker(ctx);
    test_native_then(ctx);
    test_native_then_realms(ctx);
    test_mark_as_handled(ctx);
    test_settled_promise(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    test_native_promise_oom();
    return 0;
}
