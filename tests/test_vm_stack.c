/*
 * VM local-store ownership tests
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <string.h>

#include "../src/quickjs/internal/runtime.h"
#include "../src/quickjs/internal/function.h"
#include "../src/quickjs/internal/object.h"

static JSClassID probe_class;
static JSAtom local_name;
static int finalized;

static void probe_finalizer(JSRuntime *rt, JSValue value)
{
    JSStackFrame *sf = rt->current_stack_frame;
    JSValue stored;
    intptr_t kind = (intptr_t)JS_GetOpaque(value, probe_class);

    assert(sf);
    if (kind == 1) {
        stored = sf->arg_buf[0];
    } else {
        JSFunctionBytecode *b = JS_VALUE_GET_OBJ(sf->cur_func)->u.func.function_bytecode;
        int i;
        for (i = 0; i < b->var_count; i++) {
            if (b->vardefs[b->arg_count + i].var_name == local_name)
                break;
        }
        assert(i < b->var_count);
        stored = sf->var_buf[i];
    }
    /* Replacing the slot must publish the new value before releasing the
       previous object. Inspect only; do not execute JS from a finalizer. */
    assert(JS_VALUE_IS_INT(stored));
    assert(JS_VALUE_GET_INT(stored) == 42);
    finalized++;
}

static JSValue make_probe(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv)
{
    JSValue result;
    assert(argc == 1 && JS_VALUE_IS_INT(argv[0]));
    result = JS_NewObjectClass(ctx, probe_class);
    if (!JS_IsException(result))
        JS_SetOpaque(result, (void *)(intptr_t)JS_VALUE_GET_INT(argv[0]));
    return result;
}

int main(void)
{
    static const JSClassDef definition = {
        "StackStoreProbe", .finalizer = probe_finalizer,
    };
    static const char source[] =
        "(function(x) { x = makeProbe(1); x = 42; return x; })(null);"
        "(function(x) { x = makeProbe(1); return x = 42; })(null);"
        "(function() { var x = makeProbe(2); x = 42; return x; })();"
        "(function() { var x = makeProbe(2); return x = 42; })();";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue global, result;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_NewClassID(&probe_class);
    assert(JS_NewClass(rt, probe_class, &definition) == 0);
    local_name = JS_NewAtom(ctx, "x");
    assert(local_name != JS_ATOM_NULL);
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "makeProbe",
                            JS_NewCFunction(ctx, make_probe, "makeProbe", 1)) >= 0);
    JS_FreeValue(ctx, global);
    result = JS_Eval(ctx, source, strlen(source), "<stack-store>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(result));
    assert(finalized == 4);
    JS_FreeValue(ctx, result);
    JS_FreeAtom(ctx, local_name);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
