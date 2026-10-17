/* Native callbacks must retain every captured value.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#include <assert.h>
#include <limits.h>
#include <string.h>
#include "quickjs.h"

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
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(finalized == 2);
    return 0;
}
