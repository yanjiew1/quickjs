/* Small public embedding helpers.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#include <assert.h>
#include <math.h>
#include <string.h>
#include "quickjs.h"

static JSValue eval(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source), "<embedding-helpers>",
                            JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(value));
    return value;
}

static void test_class_names(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSClassID hole = 0, registered = 0;
    JSClassDef def = { .class_name = "EmbeddingSample" };
    JSAtom atom, retained;
    const char *name;

    assert(JS_GetClassName(rt, JS_INVALID_CLASS_ID) == JS_ATOM_NULL);
    assert(JS_GetClassName(rt, (JSClassID)-1) == JS_ATOM_NULL);
    JS_NewClassID(&hole);
    JS_NewClassID(&registered);
    assert(JS_GetClassName(rt, registered) == JS_ATOM_NULL);
    assert(JS_NewClass(rt, registered, &def) == 0);
    assert(JS_GetClassName(rt, hole) == JS_ATOM_NULL);
    atom = JS_GetClassName(rt, registered);
    assert(atom != JS_ATOM_NULL);
    retained = JS_DupAtomRT(rt, atom);
    assert(retained == atom);
    JS_FreeAtomRT(rt, atom);
    name = JS_AtomToCString(ctx, retained);
    assert(name && !strcmp(name, "EmbeddingSample"));
    JS_FreeCString(ctx, name);
    JS_FreeAtomRT(rt, retained);
    atom = JS_NewAtom(ctx, "retained without a context");
    assert(atom != JS_ATOM_NULL);
    retained = JS_DupAtomRT(rt, atom);
    JS_FreeAtom(ctx, atom);
    name = JS_AtomToCString(ctx, retained);
    assert(name && !strcmp(name, "retained without a context"));
    JS_FreeCString(ctx, name);
    JS_FreeAtomRT(rt, retained);
}

static void test_to_number(JSContext *ctx)
{
    const char *input[] = { "null", "true", "' 12.5 '", "'-0'", "undefined" };
    const double expected[] = { 0, 1, 12.5, -0.0, NAN };
    const char *errors[] = { "Symbol('x')", "1n", "({ valueOf() { throw 71; } })" };
    size_t i;
    JSValue value, number, check;
    double d;

    for (i = 0; i < sizeof(input) / sizeof(input[0]); i++) {
        value = eval(ctx, input[i]);
        number = JS_ToNumber(ctx, value);
        assert(JS_IsNumber(number));
        assert(JS_ToFloat64(ctx, &d, number) == 0);
        if (isnan(expected[i]))
            assert(isnan(d));
        else
            assert(d == expected[i] && !!signbit(d) == !!signbit(expected[i]));
        JS_FreeValue(ctx, number);
        JS_FreeValue(ctx, value);
    }
    value = eval(ctx,
        "globalThis.coercions = 0; ({"
        " [Symbol.toPrimitive](hint) { coercions++;"
        "   if (hint !== 'number') throw 1; return '42'; },"
        " valueOf() { throw 2; } })");
    number = JS_ToNumber(ctx, value);
    assert(JS_IsNumber(number));
    assert(JS_ToFloat64(ctx, &d, number) == 0 && d == 42);
    check = eval(ctx, "coercions === 1");
    assert(JS_ToBool(ctx, check) == 1);
    JS_FreeValue(ctx, check);
    JS_FreeValue(ctx, number);
    JS_FreeValue(ctx, value);
    for (i = 0; i < sizeof(errors) / sizeof(errors[0]); i++) {
        value = eval(ctx, errors[i]);
        number = JS_ToNumber(ctx, value);
        assert(JS_IsException(number) && JS_HasException(ctx));
        check = JS_GetException(ctx);
        if (i == 2)
            assert(JS_IsNumber(check) && JS_VALUE_GET_INT(check) == 71);
        else {
            JSValue type;
            const char *name;

            assert(JS_IsError(ctx, check));
            type = JS_GetPropertyStr(ctx, check, "name");
            name = JS_ToCString(ctx, type);
            assert(name && !strcmp(name, "TypeError"));
            JS_FreeCString(ctx, name);
            JS_FreeValue(ctx, type);
        }
        JS_FreeValue(ctx, check);
        JS_FreeValue(ctx, value);
    }
}

static void test_compiled_values(JSContext *ctx)
{
    const char *script = "globalThis.compile_count++; 42";
    const char *module = "globalThis.compile_count++; export const answer = 42;";
    const char *ordinary[] = { "null", "undefined", "0", "0.5", "'text'",
                               "1n", "Symbol()", "({})", "(() => 0)" };
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSContext *job_ctx;
    JSValue compiled, result, namespace, value, exception;
    JSModuleDef *m;
    size_t i;
    int jobs = 0, ret;

    value = eval(ctx, "globalThis.compile_count = 0");
    JS_FreeValue(ctx, value);
    compiled = JS_Compile(ctx, script, strlen(script), "<compile-script>",
                          JS_EVAL_TYPE_GLOBAL);
    assert(JS_IsFunctionBytecode(compiled) && !JS_IsModule(compiled));
    assert(!JS_IsObject(compiled) && !JS_IsFunction(ctx, compiled));
    assert(JS_GetModuleDef(compiled) == NULL);
    value = eval(ctx, "compile_count");
    assert(JS_VALUE_GET_INT(value) == 0);
    JS_FreeValue(ctx, value);
    result = JS_EvalFunction(ctx, JS_DupValue(ctx, compiled));
    assert(JS_IsNumber(result) && JS_VALUE_GET_INT(result) == 42);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, compiled);
    value = eval(ctx, "compile_count");
    assert(JS_VALUE_GET_INT(value) == 1);
    JS_FreeValue(ctx, value);

    compiled = JS_Compile(ctx, module, strlen(module), "<compile-module>",
                          JS_EVAL_TYPE_MODULE);
    assert(!JS_IsException(compiled));
    assert(JS_IsModule(compiled) && !JS_IsFunctionBytecode(compiled));
    assert(!JS_IsObject(compiled) && !JS_IsFunction(ctx, compiled));
    m = JS_GetModuleDef(compiled);
    assert(m);
    value = eval(ctx, "compile_count");
    assert(JS_VALUE_GET_INT(value) == 1);
    JS_FreeValue(ctx, value);
    JS_RunGC(rt);
    result = JS_EvalFunction(ctx, JS_DupValue(ctx, compiled));
    assert(!JS_IsException(result));
    while ((ret = JS_ExecutePendingJob(rt, &job_ctx)) > 0)
        assert(++jobs < 100);
    assert(ret == 0 && JS_PromiseState(ctx, result) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, result);
    value = eval(ctx, "compile_count");
    assert(JS_VALUE_GET_INT(value) == 2);
    JS_FreeValue(ctx, value);
    namespace = JS_GetModuleNamespace(ctx, m);
    assert(JS_IsObject(namespace) && !JS_IsModule(namespace));
    assert(!JS_IsFunctionBytecode(namespace));
    assert(JS_GetModuleDef(namespace) == NULL);
    value = JS_GetPropertyStr(ctx, namespace, "answer");
    assert(JS_IsNumber(value) && JS_VALUE_GET_INT(value) == 42);
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, namespace);
    JS_FreeValue(ctx, compiled);

    for (i = 0; i < sizeof(ordinary) / sizeof(ordinary[0]); i++) {
        value = eval(ctx, ordinary[i]);
        assert(!JS_IsModule(value) && !JS_IsFunctionBytecode(value));
        assert(JS_GetModuleDef(value) == NULL);
        JS_FreeValue(ctx, value);
    }
    /* Queries preserve a pending exception and accept result sentinels. */
    value = JS_Throw(ctx, JS_NewInt32(ctx, 17));
    assert(!JS_IsModule(value) && !JS_IsFunctionBytecode(value));
    assert(JS_GetModuleDef(value) == NULL && JS_HasException(ctx));
    exception = JS_GetException(ctx);
    assert(JS_IsNumber(exception) && JS_VALUE_GET_INT(exception) == 17);
    JS_FreeValue(ctx, exception);

    compiled = JS_Compile(ctx, "let = ;", 7, "<compile-error>",
                          JS_EVAL_TYPE_GLOBAL);
    assert(JS_IsException(compiled) && JS_HasException(ctx));
    exception = JS_GetException(ctx);
    assert(JS_IsError(ctx, exception));
    JS_FreeValue(ctx, exception);
    script = "compile_missing = 1";
    compiled = JS_Compile(ctx, script, strlen(script), "<compile-strict>",
                          JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_STRICT);
    assert(JS_IsFunctionBytecode(compiled));
    result = JS_EvalFunction(ctx, compiled);
    assert(JS_IsException(result) && JS_HasException(ctx));
    exception = JS_GetException(ctx);
    assert(JS_IsError(ctx, exception));
    JS_FreeValue(ctx, exception);
    script = "await 1; 42";
    compiled = JS_Compile(ctx, script, strlen(script), "<compile-async>",
                          JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_ASYNC);
    assert(JS_IsFunctionBytecode(compiled));
    result = JS_EvalFunction(ctx, compiled);
    assert(!JS_IsException(result));
    while ((ret = JS_ExecutePendingJob(rt, &job_ctx)) > 0)
        assert(++jobs < 100);
    assert(ret == 0 && JS_PromiseState(ctx, result) == JS_PROMISE_FULFILLED);
    value = JS_PromiseResult(ctx, result);
    assert(JS_IsObject(value));
    exception = JS_GetPropertyStr(ctx, value, "value");
    assert(JS_IsNumber(exception) && JS_VALUE_GET_INT(exception) == 42);
    JS_FreeValue(ctx, exception);
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, result);
}

static void test_compile_without_eval(JSRuntime *rt)
{
    JSContext *ctx = JS_NewContextRaw(rt);
    JSValue compiled, exception;

    assert(ctx);
    compiled = JS_Compile(ctx, "42", 2, "<compile-disabled>", JS_EVAL_TYPE_GLOBAL);
    assert(JS_IsException(compiled) && JS_HasException(ctx));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
    JS_FreeContext(ctx);
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    const char *version = JS_GetVersion();

    assert(rt && version && version[0] && !strcmp(version, JS_GetVersion()));
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_SetGCThreshold(rt, 123456789);
    assert(JS_GetGCThreshold(rt) == 123456789);
    JS_SetGCThreshold(rt, (size_t)-1);
    assert(JS_GetGCThreshold(rt) == (size_t)-1);
    test_class_names(ctx);
    test_to_number(ctx);
    test_compiled_values(ctx);
    test_compile_without_eval(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
