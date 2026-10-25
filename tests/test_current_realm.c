/*
 * Current realm embedding contracts
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
#include <assert.h>
#include <string.h>
#include "quickjs.h"

typedef struct RealmTest {
    JSRuntime *rt, *other;
    JSContext *creator, *caller, *retained;
    JSValue nested, native;
    int observations, finalized, expect_idle, retain;
    int recurse, depth, overflows;
} RealmTest;

static JSClassID observer_class, then_class, resource_class;

static void check_current(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    RealmTest *test = JS_GetRuntimeOpaque(rt);
    int i;

    for (i = 0; i < 16; i++)
        assert(JS_GetCurrentRealm(rt) == ctx);
    assert(JS_GetCurrentRealm(test->other) == NULL);
    test->observations++;
}

static void check_idle(RealmTest *test)
{
    assert(JS_GetCurrentRealm(test->rt) == NULL);
    assert(JS_GetCurrentRealm(test->other) == NULL);
}

static JSValue eval(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source), "<current-realm>",
                            JS_EVAL_TYPE_GLOBAL);

    assert(!JS_IsException(value));
    return value;
}

/* A host callable has no execution frame of its own. This observes the
   bytecode/generator frame rather than hiding it behind a native function. */
static JSValue observe(JSContext *ctx, JSValueConst function,
                       JSValueConst this_val, int argc,
                       JSValueConst *argv, int flags)
{
    RealmTest *test = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));

    if (test->expect_idle)
        check_idle(test);
    else
        check_current(ctx);
    return JS_UNDEFINED;
}

static JSValue native_probe(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    RealmTest *test = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    JSValue result;

    check_current(ctx);
    if (!argc)
        assert(JS_IsUndefined(argv[0]) && JS_IsUndefined(argv[1]));
    if (test->retain) {
        test->retain = 0;
        test->retained = JS_DupContext(JS_GetCurrentRealm(test->rt));
    }
    if (argc) {
        result = JS_Call(ctx, test->nested, JS_UNDEFINED, 1, argv);
        /* This also covers restoration after a nested throw. */
        check_current(ctx);
        if (JS_IsException(result))
            return result;
        JS_FreeValue(ctx, result);
    }
    return JS_NewObject(ctx);
}

static JSValue data_probe(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv, int magic, JSValue *data)
{
    return native_probe(ctx, this_val, argc, argv);
}

static JSValue closure_probe(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int magic, void *opaque)
{
    return native_probe(ctx, this_val, argc, argv);
}

static JSValue strict_write(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv, int magic, JSValue *data)
{
    JSAtom atom;
    int ret;

    check_current(ctx);
    atom = JS_NewAtom(ctx, "fixed");
    assert(atom != JS_ATOM_NULL);
    ret = JS_SetPropertyInternal(ctx, argv[0], atom, JS_NewInt32(ctx, 2),
                                argv[0], JS_PROP_THROW_STRICT);
    JS_FreeAtom(ctx, atom);
    return ret < 0 ? JS_EXCEPTION : JS_NewBool(ctx, ret);
}

/* Promise resolving functions must have their own active realm while reading
   a thenable, even when they are called directly by the embedder. */
static JSValue get_then(JSContext *ctx, JSValueConst object, JSAtom atom,
                        JSValueConst receiver)
{
    RealmTest *test = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));

    check_current(ctx);
    if (test->recurse) {
        JSValue promise, resolvers[2], result;

        assert(++test->depth < 256);
        promise = JS_NewPromiseCapability(ctx, resolvers);
        assert(!JS_IsException(promise));
        result = JS_Call(ctx, resolvers[0], JS_UNDEFINED, 1, &object);
        if (JS_IsException(result)) {
            JSValue error = JS_GetException(ctx);

            assert(JS_IsError(ctx, error));
            JS_FreeValue(ctx, error);
            test->overflows++;
        } else {
            JS_FreeValue(ctx, result);
        }
        JS_FreeValue(ctx, resolvers[0]);
        JS_FreeValue(ctx, resolvers[1]);
        JS_FreeValue(ctx, promise);
        test->depth--;
        check_current(ctx);
    }
    return JS_UNDEFINED;
}

static void resource_finalizer(JSRuntime *rt, JSValue value)
{
    RealmTest *test = JS_GetRuntimeOpaque(rt);

    /* Async completion frees frame arguments after the bytecode frame has
       returned. Its native await continuation must still expose its realm. */
    assert(JS_GetCurrentRealm(rt) == test->caller);
    test->finalized++;
}

static void drain(RealmTest *test)
{
    JSContext *job_ctx;
    unsigned int jobs = 0;

    while (JS_IsJobPending(test->rt)) {
        assert(++jobs < 64);
        assert(JS_ExecutePendingJob(test->rt, &job_ctx) >= 0);
        check_idle(test);
    }
}

static JSValue host_job(JSContext *ctx, int argc, JSValueConst *argv)
{
    RealmTest *test = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    JSValue result;

    /* A raw C job carries a context but is not itself a function frame. */
    check_idle(test);
    result = JS_Call(ctx, test->native, JS_UNDEFINED, 0, NULL);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, result);
    check_idle(test);
    return JS_UNDEFINED;
}

static void test_native_calls(RealmTest *test)
{
    JSContext *a = test->creator, *b = test->caller;
    JSValue functions[3], global, value, flag;
    int i;

    functions[0] = JS_NewCFunction2(a, native_probe, "probe", 2,
                                    JS_CFUNC_constructor_or_func, 0);
    functions[1] = JS_NewCFunctionData(a, data_probe, 2, 0, 0, NULL);
    functions[2] = JS_NewCClosure(a, closure_probe, "closure", NULL, 2, 0, NULL);
    test->native = functions[0];
    test->nested = eval(b, "(function(fail) { observe(); if (fail) throw 123; })");
    for (i = 0; i < 3; i++) {
        assert(!JS_IsException(functions[i]));
        value = JS_Call(b, functions[i], JS_UNDEFINED, 0, NULL);
        assert(!JS_IsException(value));
        JS_FreeValue(b, value);
        check_idle(test);
        for (int fail = 0; fail < 2; fail++) {
            flag = JS_NewBool(b, fail);
            value = JS_Call(b, functions[i], JS_UNDEFINED, 1, &flag);
            assert(JS_IsException(value) == fail);
            if (fail)
                JS_FreeValue(b, JS_GetException(b));
            else
                JS_FreeValue(b, value);
            check_idle(test);
        }
        assert(JS_SetConstructorBit(a, functions[i], 1));
        value = JS_CallConstructor(b, functions[i], 0, NULL);
        assert(!JS_IsException(value));
        JS_FreeValue(b, value);
        check_idle(test);
    }
    global = JS_GetGlobalObject(b);
    assert(JS_SetPropertyStr(b, global, "nativeProbe", JS_DupValue(b, functions[0])) >= 0);
    assert(JS_SetPropertyStr(b, global, "strictWrite",
           JS_NewCFunctionData(a, strict_write, 1, 0, 0, NULL)) >= 0);
    value = eval(b, "nativeProbe.bind(null)(); new Proxy(nativeProbe, {})();"
                   "const frozen = Object.freeze({fixed: 1});"
                   "if (strictWrite(frozen) !== false) throw Error('sloppy write');"
                   "(function() { 'use strict'; let caught = false;"
                   " try { strictWrite(frozen); } catch(e) { caught = true; }"
                   " if (!caught) throw Error('strict write'); })();");
    JS_FreeValue(b, value);
    check_idle(test);
    assert(JS_EnqueueJob(b, host_job, 0, NULL) == 0);
    drain(test);
    /* Retaining the borrowed result is explicit and survives frame exit. */
    test->retain = 1;
    value = JS_Call(b, functions[1], JS_UNDEFINED, 0, NULL);
    assert(!JS_IsException(value) && test->retained == a);
    JS_FreeValue(b, value);
    JS_FreeValue(b, global);
    JS_FreeValue(b, test->nested);
    for (i = 0; i < 3; i++)
        JS_FreeValue(b, functions[i]);
}

static void test_suspended_calls(RealmTest *test)
{
    JSContext *a = test->creator, *b = test->caller;
    JSValue generator, method, result, function, resource, resolvers[2], promise;
    const char *methods[] = { "next", "throw", "return" };
    int i;

    generator = eval(b, "(function*() { try { observe(); yield 1; }"
                        " catch(e) { observe(); yield 2; }"
                        " finally { observe(); } })()");
    for (i = 0; i < 3; i++) {
        method = JS_GetPropertyStr(a, generator, methods[i]);
        result = JS_Call(a, method, generator, 0, NULL);
        assert(!JS_IsException(result));
        JS_FreeValue(a, result);
        JS_FreeValue(a, method);
        check_idle(test);
        JS_RunGC(test->rt);
    }
    JS_FreeValue(a, generator);

    function = eval(b, "(async function(resource) { observe(); await 0; observe(); return 42; })");
    resource = JS_NewObjectClass(a, resource_class);
    assert(!JS_IsException(resource));
    result = JS_Call(a, function, JS_UNDEFINED, 1, &resource);
    assert(!JS_IsException(result));
    JS_FreeValue(a, resource);
    check_idle(test);
    drain(test);
    assert(test->finalized == 1 && JS_PromiseState(a, result) == JS_PROMISE_FULFILLED);
    JS_FreeValue(a, result);
    JS_FreeValue(a, function);

    result = eval(b, "(async function() { const g = (async function*() {"
                     " observe(); await 0; yield 1; observe(); })();"
                     " await g.next(); await g.next(); })()");
    drain(test);
    assert(JS_PromiseState(a, result) == JS_PROMISE_FULFILLED);
    JS_FreeValue(a, result);

    promise = JS_NewPromiseCapability(b, resolvers);
    resource = JS_NewObjectClass(a, then_class);
    result = JS_Call(a, resolvers[0], JS_UNDEFINED, 1, &resource);
    assert(!JS_IsException(result));
    JS_FreeValue(a, result);
    JS_FreeValue(a, resource);
    JS_FreeValue(a, promise);
    JS_FreeValue(a, resolvers[0]);
    JS_FreeValue(a, resolvers[1]);
    check_idle(test);
}

static void test_native_recursion(RealmTest *test)
{
    JSContext *ctx = test->caller;
    JSValue promise, resolvers[2], object, result;

    /* Recursion through host getters has no intervening bytecode/native-C
       function guard. Each resolving-function frame must check the limit. */
    JS_SetMaxStackSize(test->rt, 32 * 1024);
    test->recurse = 1;
    promise = JS_NewPromiseCapability(ctx, resolvers);
    object = JS_NewObjectClass(ctx, then_class);
    assert(!JS_IsException(promise) && !JS_IsException(object));
    result = JS_Call(ctx, resolvers[0], JS_UNDEFINED, 1, &object);
    assert(!JS_IsException(result));
    assert(test->overflows == 1 && test->depth == 0);
    test->recurse = 0;
    JS_SetMaxStackSize(test->rt, 0);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, object);
    JS_FreeValue(ctx, promise);
    JS_FreeValue(ctx, resolvers[0]);
    JS_FreeValue(ctx, resolvers[1]);
    check_idle(test);
}

typedef struct PollState {
    JSContext *realm;
    int calls, abort;
} PollState;

static int interrupt_probe(JSRuntime *rt, void *opaque)
{
    PollState *state = opaque;
    JSContext *realm = JS_GetCurrentRealm(rt);

    if (realm) {
        assert(realm == state->realm);
        state->calls++;
        return state->abort;
    }
    return 0; /* Entry checks may run before the first frame is installed. */
}

static void test_interrupts(RealmTest *test)
{
    PollState state = { test->caller, 0, 0 };
    const char *loop = "for (let i = 0; i < 100000; i++) {}";
    JSValue value;

    JS_SetInterruptHandler(test->rt, interrupt_probe, &state);
    value = eval(test->caller, loop);
    JS_FreeValue(test->caller, value);
    assert(state.calls > 0);
    check_idle(test);
    state.calls = 0;
    state.abort = 1;
    value = JS_Eval(test->caller, loop, strlen(loop), "<interrupt-realm>",
                    JS_EVAL_TYPE_GLOBAL);
    assert(JS_IsException(value) && state.calls > 0);
    JS_FreeValue(test->caller, JS_GetException(test->caller));
    JS_SetUncatchableException(test->caller, 0);
    JS_SetInterruptHandler(test->rt, NULL, NULL);
    check_idle(test);
}

int main(void)
{
    RealmTest test = { 0 };
    JSValue observer, global, value;
    JSClassDef observer_def = { .class_name = "RealmObserver", .call = observe };
    JSClassDef resource_def = { .class_name = "RealmResource", .finalizer = resource_finalizer };
    JSClassExoticMethods then_exotic = { .get_property = get_then };
    JSClassDef then_def = { .class_name = "RealmThenable", .exotic = &then_exotic };

    test.rt = JS_NewRuntime();
    test.other = JS_NewRuntime();
    assert(test.rt && test.other);
    JS_SetRuntimeOpaque(test.rt, &test);
    check_idle(&test);
    test.creator = JS_NewContext(test.rt);
    test.caller = JS_NewContext(test.rt);
    assert(test.creator && test.caller);
    JS_NewClassID(&observer_class);
    JS_NewClassID(&resource_class);
    JS_NewClassID(&then_class);
    assert(!JS_NewClass(test.rt, observer_class, &observer_def));
    assert(!JS_NewClass(test.rt, resource_class, &resource_def));
    assert(!JS_NewClass(test.rt, then_class, &then_def));
    observer = JS_NewObjectClass(test.creator, observer_class);
    assert(!JS_IsException(observer));
    global = JS_GetGlobalObject(test.caller);
    assert(JS_SetPropertyStr(test.caller, global, "observe", JS_DupValue(test.caller, observer)) >= 0);
    JS_FreeValue(test.caller, global);
    test.expect_idle = 1;
    value = JS_Call(test.caller, observer, JS_UNDEFINED, 0, NULL);
    assert(!JS_IsException(value));
    test.expect_idle = 0;
    JS_FreeValue(test.caller, value);
    JS_FreeValue(test.caller, observer);
    test_native_calls(&test);
    test_suspended_calls(&test);
    test_interrupts(&test);
    test_native_recursion(&test);
    assert(test.observations >= 20);
    JS_FreeContext(test.creator);
    JS_FreeContext(test.caller);
    JS_RunGC(test.rt);
    value = eval(test.retained, "40 + 2");
    JS_FreeValue(test.retained, value);
    JS_FreeContext(test.retained);
    JS_RunGC(test.rt);
    check_idle(&test);
    JS_FreeRuntime(test.rt);
    JS_FreeRuntime(test.other);
    return 0;
}
