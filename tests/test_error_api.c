/*
 * Owned Error constructor embedding contracts
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
#include "oom-allocator.h"
#include "pool-padding.h"

typedef JSValue ErrorFactory(JSContext *, const char *, ...);

static const struct {
    const char *name;
    ErrorFactory *create;
} factories[] = {
    { "Error", JS_NewPlainError },
    { "TypeError", JS_NewTypeError },
    { "RangeError", JS_NewRangeError },
    { "ReferenceError", JS_NewReferenceError },
    { "SyntaxError", JS_NewSyntaxError },
    { "InternalError", JS_NewInternalError },
};

static void check_string(JSContext *ctx, JSValueConst object,
                         const char *property, const char *expected)
{
    JSValue value = JS_GetPropertyStr(ctx, object, property);
    const char *text = JS_ToCString(ctx, value);

    assert(text && !strcmp(text, expected));
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, value);
}

static void test_realms_and_pending_exception(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *contexts[2];
    JSValue prototypes[2][6];
    unsigned int realm, i;

    assert(rt);
    for (realm = 0; realm < 2; realm++) {
        JSContext *ctx = contexts[realm] = JS_NewContext(rt);
        JSValue global;

        assert(ctx);
        global = JS_GetGlobalObject(ctx);
        for (i = 0; i < 6; i++) {
            JSValue constructor = JS_GetPropertyStr(ctx, global, factories[i].name);

            prototypes[realm][i] = JS_GetPropertyStr(ctx, constructor, "prototype");
            assert(JS_IsObject(prototypes[realm][i]));
            JS_FreeValue(ctx, constructor);
            /* Factories must use retained intrinsics, not mutable globals. */
            assert(JS_SetPropertyStr(ctx, global, factories[i].name, JS_NULL) >= 0);
        }
        JS_FreeValue(ctx, global);
    }
    for (realm = 0; realm < 2; realm++) {
        JSContext *ctx = contexts[realm];

        for (i = 0; i < 6; i++) {
            JSValue pending = JS_NewString(ctx, "old exception");
            JSValue error, prototype, stack, saved;

            assert(!JS_IsException(pending));
            JS_Throw(ctx, JS_DupValue(ctx, pending));
            error = factories[i].create(ctx, "%s %d", "message", 42);
            assert(!JS_IsException(error) && JS_IsError(ctx, error));
            saved = JS_GetException(ctx);
            assert(JS_StrictEq(ctx, pending, saved));
            JS_FreeValue(ctx, saved);
            JS_FreeValue(ctx, pending);
            prototype = JS_GetPrototype(ctx, error);
            assert(JS_StrictEq(ctx, prototype, prototypes[realm][i]));
            assert(!JS_StrictEq(ctx, prototype, prototypes[1 - realm][i]));
            check_string(ctx, error, "message", "message 42");
            check_string(ctx, error, "name", factories[i].name);
            stack = JS_GetPropertyStr(ctx, error, "stack");
            assert(JS_IsString(stack));
            JS_FreeValue(ctx, stack);
            JS_FreeValue(ctx, prototype);
            JS_FreeValue(ctx, error);
            assert(!JS_HasException(ctx));
        }
    }
    for (realm = 0; realm < 2; realm++) {
        for (i = 0; i < 6; i++)
            JS_FreeValue(contexts[realm], prototypes[realm][i]);
        JS_FreeContext(contexts[realm]);
    }
    JS_FreeRuntime(rt);
}

static JSValue preserve_uncatchable(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    JSValue error;

    JS_Throw(ctx, JS_NewInt32(ctx, 42));
    JS_SetUncatchableException(ctx, 1);
    error = JS_NewTypeError(ctx, "constructed, not thrown");
    assert(!JS_IsException(error));
    JS_FreeValue(ctx, error);
    return JS_EXCEPTION;
}

static JSValue make_internal(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    return JS_NewInternalError(ctx, "catchable");
}

static void test_exception_flags_and_raw_context(void)
{
    const char *source = "globalThis.caught = false;"
        "try { preserve(); } catch (e) { caught = true; }";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt), *raw;
    JSValue global, value, error;
    int32_t number;

    assert(rt && ctx);
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "preserve",
        JS_NewCFunction(ctx, preserve_uncatchable, "preserve", 0)) >= 0);
    value = JS_Eval(ctx, source, strlen(source), "<error-flags>", JS_EVAL_TYPE_GLOBAL);
    assert(JS_IsException(value));
    error = JS_GetException(ctx);
    assert(JS_ToInt32(ctx, &number, error) == 0 && number == 42);
    JS_FreeValue(ctx, error);
    JS_SetUncatchableException(ctx, 0);
    value = JS_GetPropertyStr(ctx, global, "caught");
    assert(JS_ToBool(ctx, value) == 0);
    JS_FreeValue(ctx, value);
    assert(JS_SetPropertyStr(ctx, global, "makeInternal",
        JS_NewCFunction(ctx, make_internal, "makeInternal", 0)) >= 0);
    source = "try { throw makeInternal(); } catch(e) { e.message === 'catchable'; }";
    value = JS_Eval(ctx, source, strlen(source), "<internal-error>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(value) && JS_ToBool(ctx, value) == 1);
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, global);
    raw = JS_NewContextRaw(rt);
    assert(raw);
    error = JS_NewTypeError(raw, "raw context");
    assert(!JS_IsException(error) && JS_IsError(raw, error));
    check_string(raw, error, "name", "TypeError");
    JS_FreeValue(raw, error);
    JS_FreeContext(raw);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static JSValue make_type(JSContext *ctx, JSValueConst this_val,
                         int argc, JSValueConst *argv)
{
    return JS_NewTypeError(ctx, "allocation %d", 42);
}

static void test_allocation_failures(void)
{
    size_t fail_at, calls = 0;

    for (fail_at = 0; fail_at <= calls; fail_at++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue function, error;
        void *padding;

        assert(ctx);
        /* A wide function name also exercises backtrace string conversion. */
        function = JS_NewCFunction(ctx, make_type, "error-\xc4\x80", 0);
        assert(!JS_IsException(function));
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, fail_at ? fail_at : (size_t)-1);
        error = JS_Call(ctx, function, JS_UNDEFINED, 0, NULL);
        oom_test_end(&allocator);
        if (!fail_at)
            calls = allocator.calls;
        if (fail_at)
            assert(allocator.failures == 1);
        if (JS_IsException(error)) {
            assert(allocator.failures && JS_HasException(ctx));
            JS_FreeValue(ctx, JS_GetException(ctx));
        } else {
            JSValue stack = JS_GetPropertyStr(ctx, error, "stack");
            const char *text = JS_ToCString(ctx, stack);

            /* Optional buffer shrinking may fail without failing construction. */
            assert(JS_IsError(ctx, error));
            check_string(ctx, error, "message", "allocation 42");
            assert(text && strstr(text, "error-\xc4\x80"));
            JS_FreeCString(ctx, text);
            JS_FreeValue(ctx, stack);
            assert(!JS_HasException(ctx));
        }
        JS_FreeValue(ctx, error);
        JS_FreeValue(ctx, function);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
    assert(calls > 0);
}

static OOMTestAllocator *property_allocator;
static void *property_padding;

static JSValue fail_property_allocation(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    property_padding = test_pool_padding(JS_GetRuntime(ctx));
    oom_test_begin(property_allocator, 1);
    return JS_TRUE;
}

static void test_constructor_property_failure(int aggregate)
{
    static const char iterator_source[] =
        "(record => ({ [Symbol.iterator]() { return this; },"
        "next() { return record; } }))";
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    JSValue global, constructor, record, getter, prototype, anchor;
    JSValue factory = JS_UNDEFINED;
    JSValue input = JS_UNDEFINED, result, args[2];
    JSAtom key;

    assert(ctx);
    global = JS_GetGlobalObject(ctx);
    constructor = JS_GetPropertyStr(ctx, global, aggregate ? "AggregateError" : "Error");
    prototype = JS_GetPropertyStr(ctx, constructor, "prototype");
    /* Retain the empty shape so adding the error's first property must clone it. */
    anchor = JS_NewObjectProto(ctx, prototype);
    JS_FreeValue(ctx, prototype);
    record = JS_NewObject(ctx);
    getter = JS_NewCFunction(ctx, fail_property_allocation, "armFailure", 0);
    assert(!JS_IsException(constructor) && !JS_IsException(anchor) && !JS_IsException(record) &&
           !JS_IsException(getter));
    key = JS_NewAtom(ctx, aggregate ? "done" : "cause");
    assert(key != JS_ATOM_NULL);
    assert(JS_DefinePropertyGetSet(ctx, record, key, getter, JS_UNDEFINED,
                                  JS_PROP_CONFIGURABLE) >= 0);
    JS_FreeAtom(ctx, key);
    if (aggregate) {
        factory = JS_Eval(ctx, iterator_source, sizeof(iterator_source) - 1,
                          "<error-iterator>", JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(factory));
        input = JS_Call(ctx, factory, JS_UNDEFINED, 1, (JSValueConst *)&record);
        assert(!JS_IsException(input));
        args[0] = input;
    } else {
        args[0] = JS_UNDEFINED;
        args[1] = record;
    }
    /* Arm only after the cause getter or final iterator step. This targets
       required property creation rather than optional backtrace allocation. */
    property_allocator = &allocator;
    property_padding = NULL;
    result = JS_CallConstructor(ctx, constructor, aggregate ? 1 : 2,
                                (JSValueConst *)args);
    oom_test_end(&allocator);
    assert(allocator.failures == 1 && JS_IsException(result));
    oom_test_check_exception(ctx);
    test_pool_padding_free(rt, property_padding);
    property_padding = NULL;
    property_allocator = NULL;
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, input);
    JS_FreeValue(ctx, factory);
    JS_FreeValue(ctx, record);
    JS_FreeValue(ctx, anchor);
    JS_FreeValue(ctx, constructor);
    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(!allocator.live);
}

static void test_throwing_allocation_failures(void)
{
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue result, error;
        void *padding;

        assert(ctx);
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        result = JS_ThrowTypeError(ctx, "allocation %d", 42);
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 64);
        } else {
            assert(allocator.failures == 1);
        }
        assert(JS_IsException(result) && JS_HasException(ctx));
        error = JS_GetException(ctx);
        /* Throwing helpers deliberately fall back to null or an incomplete
           Error under OOM, but must not publish JS_EXCEPTION as its message. */
        assert(JS_IsNull(error) || JS_IsError(ctx, error));
        if (JS_IsObject(error)) {
            JSValue message = JS_GetPropertyStr(ctx, error, "message");

            assert(JS_IsString(message));
            JS_FreeValue(ctx, message);
            if (!step)
                check_string(ctx, error, "message", "allocation 42");
        }
        assert(!JS_HasException(ctx));
        JS_FreeValue(ctx, error);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

int main(void)
{
    test_throwing_allocation_failures();
    test_constructor_property_failure(0);
    test_constructor_property_failure(1);
    test_realms_and_pending_exception();
    test_exception_flags_and_raw_context();
    test_allocation_failures();
    return 0;
}
