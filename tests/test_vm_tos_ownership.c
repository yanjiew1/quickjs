/*
 * Cached VM stack ownership across native reentry and abrupt completion.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "quickjs.h"

static JSClassID tracked_class;
static int created, finalized, ticks;

static void tracked_finalizer(JSRuntime *rt, JSValue value)
{
    (void)rt;
    (void)value;
    finalized++;
}

static JSValue make_tracked(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    JSValue value = JS_NewObjectClass(ctx, tracked_class);
    (void)this_val;
    (void)argc;
    (void)argv;
    if (!JS_IsException(value))
        created++;
    return value;
}

static JSValue reenter(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv)
{
    const char *source = "(() => { let x = 1; return (x + 2) * 3; })()";
    JSValue result;
    (void)this_val;
    JS_RunGC(JS_GetRuntime(ctx));
    result = JS_Eval(ctx, source, strlen(source), "reentry",
                     JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result))
        return result;
    JS_FreeValue(ctx, result);
    return argc ? JS_DupValue(ctx, argv[0]) : JS_UNDEFINED;
}

static int interrupt(JSRuntime *rt, void *opaque)
{
    (void)rt;
    (void)opaque;
    return ++ticks >= 3;
}

static void check_case(JSRuntime *rt, JSContext *ctx, const char *source,
                        int expected_exception)
{
    JSValue result;
    int n = created;
    result = JS_Eval(ctx, source, strlen(source), "tos-ownership",
                     JS_EVAL_TYPE_GLOBAL);
    assert(JS_IsException(result) == expected_exception);
    JS_FreeValue(ctx, result);
    if (expected_exception)
        JS_FreeValue(ctx, JS_GetException(ctx));
    JS_RunGC(rt);
    assert(created > n);
    assert(created == finalized);
}

int main(void)
{
    static const JSClassDef def = {
        "TosTracked", .finalizer = tracked_finalizer,
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue global;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_NewClassID(&tracked_class);
    assert(JS_NewClass(rt, tracked_class, &def) == 0);
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "makeTracked",
                            JS_NewCFunction(ctx, make_tracked, "makeTracked", 0)) >= 0);
    assert(JS_SetPropertyStr(ctx, global, "reenter",
                            JS_NewCFunction(ctx, reenter, "reenter", 1)) >= 0);
    JS_FreeValue(ctx, global);

    check_case(rt, ctx, "(() => { for(let i=0;i<100;i++) { makeTracked(); reenter(makeTracked()); } })()", 0);
    check_case(rt, ctx, "(() => { let a=makeTracked(); a.self=a; return a; })()", 0);
    check_case(rt, ctx, "(() => { throw makeTracked(); })()", 1);
    check_case(rt, ctx, "(() => { try { throw makeTracked(); } catch(e) { return reenter(e); } })()", 0);
    check_case(rt, ctx, "(() => { let a=makeTracked(); a.valueOf=()=>{reenter(); throw makeTracked()}; return a+1; })()", 1);
    check_case(rt, ctx, "(() => { let a=makeTracked(); a.valueOf=()=>{reenter(); return 3}; return 1+a; })()", 0);
    check_case(rt, ctx, "(() => { let a=makeTracked(); return [a,reenter(a),makeTracked()]; })()", 0);
    check_case(rt, ctx, "(() => { function f(a,b) { a=makeTracked(); b=undefined; return reenter(a); } return f(makeTracked(),makeTracked()); })()", 0);
    check_case(rt, ctx, "(() => { function* g() { const x=makeTracked(); yield x; return x; } let it=g(); it.next(); reenter(); it.next(); })()", 0);
    check_case(rt, ctx, "(() => { function* g() { const x=makeTracked(); yield x; return x; } let it=g(); it.next(); reenter(); it.throw(makeTracked()); })()", 1);
    check_case(rt, ctx, "(() => { function* g() { const x=makeTracked(); yield x; return x; } let it=g(); it.next(); reenter(); it.return(makeTracked()); })()", 0);
    check_case(rt, ctx, "(() => { function* g() { const x=makeTracked(); yield x; } let it=g(); it.next(); })()", 0);
    check_case(rt, ctx, "(() => { const x=makeTracked(); try { return reenter(x); } finally { makeTracked(); reenter(); } })()", 0);

    ticks = 0;
    JS_SetInterruptHandler(rt, interrupt, NULL);
    check_case(rt, ctx, "(() => { let x=makeTracked(); for (;;) { reenter(x); } })()", 1);
    JS_SetInterruptHandler(rt, NULL, NULL);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(created == finalized);
    puts("tos ownership: all tracked objects finalized exactly once");
    return 0;
}
