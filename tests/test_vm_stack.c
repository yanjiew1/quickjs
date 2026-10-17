/* Operand stack roots must survive calls and suspension, then be released.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"

static JSClassID probe_class;
static int created, finalized;

static void probe_finalizer(JSRuntime *rt, JSValue value)
{
    finalized++;
}

static JSValue make_probe(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    JSValue value = JS_NewObjectClass(ctx, probe_class);
    if (JS_IsException(value))
        return value;
    created++;
    if (argc && JS_ToBool(ctx, argv[0]) &&
        JS_SetPropertyStr(ctx, value, "self", JS_DupValue(ctx, value)) < 0) {
        JS_FreeValue(ctx, value);
        return JS_EXCEPTION;
    }
    return value;
}

static JSValue collect(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv)
{
    JS_RunGC(JS_GetRuntime(ctx));
    return JS_UNDEFINED;
}

static JSValue evaluate(JSContext *ctx, const char *source)
{
    JSValue result = JS_Eval(ctx, source, strlen(source), "vm-stack",
                             JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue error = JS_GetException(ctx);
        const char *message = JS_ToCString(ctx, error);
        fprintf(stderr, "vm-stack: %s\n", message ? message : "exception");
        JS_FreeCString(ctx, message);
        JS_FreeValue(ctx, error);
        abort();
    }
    return result;
}

static void evaluate_discard(JSContext *ctx, const char *source)
{
    JS_FreeValue(ctx, evaluate(ctx, source));
}

static JSValue reenter(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv)
{
    int previous = finalized;
    assert(argc == 1);
    evaluate_discard(ctx, "(function() { collect(); return 42; })()");
    assert(finalized == previous);
    return JS_DupValue(ctx, argv[0]);
}

static void check_released(JSRuntime *rt)
{
    JS_RunGC(rt);
    assert(finalized == created);
}

static void test_drop_return_and_calls(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSValue value;
    int previous;

    evaluate_discard(ctx, "(function() { makeProbe(); makeProbe(true); })()");
    check_released(rt);

    previous = created;
    value = evaluate(ctx, "(function() { return makeProbe(true); })()");
    assert(created == previous + 1);
    JS_RunGC(rt);
    assert(finalized == previous);
    JS_FreeValue(ctx, value);
    check_released(rt);

    previous = created;
    value = evaluate(ctx, "reenter(makeProbe(true))");
    JS_RunGC(rt);
    assert(created == previous + 1 && finalized == previous);
    JS_FreeValue(ctx, value);
    check_released(rt);

    /* Assignments in the callee replace the caller's last argument. The
       caller must not free its stale cached value on either exit path. */
    value = evaluate(ctx,
        "(function() {"
        "  function replace(a) { a = makeProbe(); collect(); return a; }"
        "  return replace(makeProbe());"
        "})()");
    assert(created == finalized + 1);
    JS_FreeValue(ctx, value);
    check_released(rt);

    evaluate_discard(ctx,
        "(function() {"
        "  function fail(a) { arguments[0] = makeProbe(); throw 1; }"
        "  try { fail(makeProbe()); } catch (e) { if (e !== 1) throw e; }"
        "  function failEval(a) { eval('a = makeProbe()'); throw 2; }"
        "  try { failEval(makeProbe()); } catch (e) { if (e !== 2) throw e; }"
        "  function failTail(a) { return fail(a); }"
        "  try { failTail(makeProbe()); } catch (e) { if (e !== 1) throw e; }"
        "  try {"
        "    (function(a, b) {})(makeProbe(true), {"
        "      [Symbol.toPrimitive]() { collect(); throw 3; }"
        "    } + 1);"
        "  } catch (e) { if (e !== 3) throw e; }"
        "})()");
    check_released(rt);
}

static JSValue invoke(JSContext *ctx, JSValueConst object, const char *name)
{
    JSAtom atom = JS_NewAtom(ctx, name);
    JSValue result;
    assert(atom != JS_ATOM_NULL);
    result = JS_Invoke(ctx, object, atom, 0, NULL);
    JS_FreeAtom(ctx, atom);
    return result;
}

static void test_generator_roots(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    int mode;

    for (mode = 0; mode < 3; mode++) {
        int previous = created;
        JSValue generator = evaluate(ctx,
            "(function*() {"
            "  try { return [makeProbe(true), yield 1]; }"
            "  finally { collect(); }"
            "})()");
        JSValue result = invoke(ctx, generator, "next");
        assert(!JS_IsException(result));
        JS_FreeValue(ctx, result);
        JS_RunGC(rt);
        assert(created == previous + 1 && finalized == previous);
        result = invoke(ctx, generator, mode == 0 ? "next" :
                                         mode == 1 ? "return" : "throw");
        if (mode == 2) {
            assert(JS_IsException(result));
            JS_FreeValue(ctx, JS_GetException(ctx));
        } else {
            assert(!JS_IsException(result));
            if (mode == 0) {
                JS_RunGC(rt);
                assert(finalized == previous);
            }
            JS_FreeValue(ctx, result);
        }
        JS_FreeValue(ctx, generator);
        check_released(rt);
    }
    /* An unstarted frame can already own an argument and reserved varrefs. */
    evaluate_discard(ctx,
        "(function* (value) { yield () => value; })(makeProbe(true)).return()");
    check_released(rt);
}

static void test_async_roots(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSContext *job_ctx;
    int previous = created, jobs = 0;

    evaluate_discard(ctx,
        "globalThis.pendingProbe = (async function() {"
        "  const gate = new Promise(resolve => globalThis.releaseProbe = resolve);"
        "  return [makeProbe(true), await gate];"
        "})(); void 0;");
    JS_RunGC(rt);
    assert(created == previous + 1 && finalized == previous);
    evaluate_discard(ctx, "releaseProbe(0); releaseProbe = undefined;");
    while (JS_IsJobPending(rt)) {
        assert(++jobs < 100);
        assert(JS_ExecutePendingJob(rt, &job_ctx) >= 0);
    }
    JS_RunGC(rt);
    assert(finalized == previous);
    evaluate_discard(ctx, "pendingProbe = undefined;");
    check_released(rt);
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue global;
    JSClassDef definition = {
        .class_name = "StackProbe",
        .finalizer = probe_finalizer,
    };
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_NewClassID(&probe_class);
    assert(JS_NewClass(rt, probe_class, &definition) == 0);
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "makeProbe",
                             JS_NewCFunction(ctx, make_probe, "makeProbe", 1)) >= 0);
    assert(JS_SetPropertyStr(ctx, global, "collect",
                             JS_NewCFunction(ctx, collect, "collect", 0)) >= 0);
    assert(JS_SetPropertyStr(ctx, global, "reenter",
                             JS_NewCFunction(ctx, reenter, "reenter", 1)) >= 0);
    JS_FreeValue(ctx, global);
    test_drop_return_and_calls(ctx);
    test_generator_roots(ctx);
    test_async_roots(ctx);
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(finalized == created);
    return 0;
}
