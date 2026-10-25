/* Native callbacks must retain every captured value.
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
#include <limits.h>
#include <string.h>
#include "quickjs.h"
#include "oom-allocator.h"

static int finalized;
static JSClassID resource_class;

static void resource_finalizer(JSRuntime *rt, JSValue value)
{
    finalized++;
}

static JSValue last_capture(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv, int magic,
                            JSValue *data)
{
    return JS_DupValue(ctx, data[magic]);
}

static void test_large_capture_set(JSContext *ctx, int cycle)
{
    JSValue resource, function, result;
    JSValueConst data[256];
    int previous = finalized, i;

    resource = JS_NewObjectClass(ctx, resource_class);
    assert(!JS_IsException(resource));
    for (i = 0; i < 256; i++)
        data[i] = resource;
    function = JS_NewCFunctionData(ctx, last_capture, 0, 255, 256, data);
    assert(!JS_IsException(function));
    if (cycle) {
        assert(JS_SetPropertyStr(ctx, resource, "callback",
                                 JS_DupValue(ctx, function)) >= 0);
    } else {
        result = JS_Call(ctx, function, JS_UNDEFINED, 0, NULL);
        assert(JS_IsObject(result) && JS_StrictEq(ctx, result, resource));
        JS_FreeValue(ctx, result);
    }
    JS_FreeValue(ctx, resource);
    assert(finalized == previous);
    JS_FreeValue(ctx, function);
    if (cycle)
        JS_RunGC(JS_GetRuntime(ctx));
    assert(finalized == previous + 1);
}

static void check_error(JSContext *ctx, JSValue result, const char *type)
{
    JSValue error, name;
    const char *text;

    assert(JS_IsException(result) && JS_HasException(ctx));
    error = JS_GetException(ctx);
    name = JS_GetPropertyStr(ctx, error, "name");
    text = JS_ToCString(ctx, name);
    assert(text && !strcmp(text, type));
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, name);
    JS_FreeValue(ctx, error);
}

static void test_invalid_metadata(JSContext *ctx)
{
    JSValue data = JS_UNDEFINED;

    check_error(ctx, JS_NewCFunctionData(ctx, last_capture, 0, 0, -1, NULL),
                "RangeError");
    check_error(ctx, JS_NewCFunctionData(ctx, last_capture, -1, 0, 0, NULL),
                "RangeError");
    check_error(ctx, JS_NewCFunctionData(ctx, last_capture, 256, 0, 0, NULL),
                "RangeError");
    check_error(ctx, JS_NewCFunctionData(ctx, NULL, 0, 0, 0, NULL), "TypeError");
    check_error(ctx, JS_NewCFunctionData(ctx, last_capture, 0, 0, 1, NULL),
                "TypeError");
    if (SIZE_MAX / sizeof(JSValue) < INT_MAX)
        check_error(ctx, JS_NewCFunctionData(ctx, last_capture, 0, 0, INT_MAX,
                                            (JSValueConst *)&data), "RangeError");
}

static JSValue padded_capture(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int magic,
                              JSValue *data)
{
    assert(argc == 0 && magic == 123);
    assert(JS_IsUndefined(argv[0]) && JS_IsUndefined(argv[1]));
    return JS_DupValue(ctx, data[0]);
}

static void check_property(JSContext *ctx, JSValueConst function,
                           const char *key, const char *expected, int length)
{
    JSAtom atom = JS_NewAtom(ctx, key);
    JSPropertyDescriptor desc;
    const char *text;
    int32_t number;

    assert(atom != JS_ATOM_NULL);
    assert(JS_GetOwnProperty(ctx, &desc, function, atom) == 1);
    assert(desc.flags & JS_PROP_CONFIGURABLE);
    assert(!(desc.flags & (JS_PROP_WRITABLE | JS_PROP_ENUMERABLE)));
    if (expected) {
        text = JS_ToCString(ctx, desc.value);
        assert(text && !strcmp(text, expected));
        JS_FreeCString(ctx, text);
    } else {
        assert(JS_ToInt32(ctx, &number, desc.value) == 0 && number == length);
    }
    JS_FreeValue(ctx, desc.value);
    JS_FreeValue(ctx, desc.getter);
    JS_FreeValue(ctx, desc.setter);
    JS_FreeAtom(ctx, atom);
}

static void test_named_callback(JSContext *ctx)
{
    JSValue data = JS_NewInt32(ctx, 42), function, result;
    int32_t number;

    function = JS_NewCFunctionData2(ctx, padded_capture, "native λ callback",
                                    2, 123, 1, (JSValueConst *)&data);
    assert(!JS_IsException(function));
    check_property(ctx, function, "name", "native λ callback", 0);
    check_property(ctx, function, "length", NULL, 2);
    result = JS_Call(ctx, function, JS_UNDEFINED, 0, NULL);
    assert(JS_ToInt32(ctx, &number, result) == 0 && number == 42);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, function);
    function = JS_NewCFunctionData2(ctx, last_capture, NULL, 0, 0, 1,
                                    (JSValueConst *)&data);
    assert(!JS_IsException(function));
    check_property(ctx, function, "name", "", 0);
    JS_FreeValue(ctx, function);
    function = JS_NewCFunctionData(ctx, last_capture, 0, 0, 1,
                                   (JSValueConst *)&data);
    assert(!JS_IsException(function));
    check_property(ctx, function, "name", "", 0);
    JS_FreeValue(ctx, function);
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

static void test_named_callback_oom(void)
{
    JSClassDef def = {
        .class_name = "CapturedResource",
        .finalizer = resource_finalizer,
    };
    size_t fail_at;
    int failures = 0, successes = 0;

    for (fail_at = 1; fail_at <= 64; fail_at++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue resource, function;
        int previous = finalized;
        void *padding;

        assert(ctx);
        assert(JS_NewClass(rt, resource_class, &def) == 0);
        resource = JS_NewObjectClass(ctx, resource_class);
        assert(!JS_IsException(resource));
        padding = exhaust_pooled_blocks(rt);
        oom_test_begin(&allocator, fail_at);
        function = JS_NewCFunctionData2(ctx, last_capture,
                                        "a fresh native callback name",
                                        0, 0, 1, (JSValueConst *)&resource);
        oom_test_end(&allocator);
        if (JS_IsException(function)) {
            JSValue error;

            failures++;
            assert(allocator.failures == 1 && JS_HasException(ctx));
            error = JS_GetException(ctx);
            JS_FreeValue(ctx, error);
        } else {
            successes++;
            assert(!JS_HasException(ctx));
        }
        JS_FreeValue(ctx, resource);
        if (!JS_IsException(function)) {
            assert(finalized == previous);
            JS_FreeValue(ctx, function);
        }
        assert(finalized == previous + 1);
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

/* Data callbacks implement standard builtin closures too. Keep their creation
   realm even when only a callback from another context remains reachable. */
static JSValue callback_global(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int magic,
                               JSValue *data)
{
    return JS_GetGlobalObject(ctx);
}

static void test_creation_realm(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *creator = JS_NewContext(rt), *caller = JS_NewContext(rt);
    JSValue function, expected_global, constructor, object, prototype, expected;

    assert(creator && caller);
    expected_global = JS_GetGlobalObject(creator);
    function = JS_NewCFunctionData(creator, callback_global, 0, 0, 0, NULL);
    assert(!JS_IsException(function));
    assert(JS_SetConstructorBit(creator, function, 1));
    assert(JS_SetPropertyStr(creator, function, "prototype", JS_NULL) >= 0);
    expected = JS_Eval(creator, "Object.prototype", 16, "<realm>",
                       JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(expected));
    JS_FreeContext(creator);
    JS_RunGC(rt);

    object = JS_Call(caller, function, JS_UNDEFINED, 0, NULL);
    assert(JS_StrictEq(caller, object, expected_global));
    JS_FreeValue(caller, object);
    constructor = JS_Eval(caller, "Object", 6, "<realm>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(constructor));
    /* A non-object prototype uses GetFunctionRealm(newTarget), independently
       of the callback's ordinary call path. */
    object = JS_CallConstructor2(caller, constructor, function, 0, NULL);
    assert(!JS_IsException(object));
    prototype = JS_GetPrototype(caller, object);
    assert(JS_StrictEq(caller, prototype, expected));
    JS_FreeValue(caller, prototype);
    JS_FreeValue(caller, object);
    JS_FreeValue(caller, constructor);
    JS_FreeValue(caller, expected);
    JS_FreeValue(caller, expected_global);
    JS_FreeValue(caller, function);
    JS_FreeContext(caller);
    JS_RunGC(rt);
    JS_FreeRuntime(rt);
}

int main(void)
{
    JSClassDef def = {
        .class_name = "CapturedResource",
        .finalizer = resource_finalizer,
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_NewClassID(&resource_class);
    assert(JS_NewClass(rt, resource_class, &def) == 0);
    test_large_capture_set(ctx, 0);
    test_large_capture_set(ctx, 1);
    test_invalid_metadata(ctx);
    test_named_callback(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(finalized == 2);
    test_named_callback_oom();
    test_creation_realm();
    return 0;
}
