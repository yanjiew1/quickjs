/* JSON array-element ownership when its index conversion fails.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "../src/quickjs/value/conversion.h"

static JSValue failing_index_string(JSContext *ctx, JSValue value);
#define JS_ToStringFree failing_index_string
#include "../src/quickjs/builtins/json-stringify.c"
#undef JS_ToStringFree

static int fail_index, armed, failures, finalized;
static JSClassID element_class;

static JSValue failing_index_string(JSContext *ctx, JSValue value)
{
    if (armed && fail_index) {
        assert(JS_VALUE_GET_TAG(value) == JS_TAG_INT);
        assert(JS_VALUE_GET_INT(value) == 0);
        armed = 0;
        failures++;
        JS_FreeValue(ctx, value);
        return JS_ThrowOutOfMemory(ctx);
    }
    return JS_ToStringFree(ctx, value);
}

static void element_finalizer(JSRuntime *rt, JSValue value)
{
    finalized++;
}

static JSValue get_element(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
    JSValue value = JS_NewObjectClass(ctx, element_class);
    assert(!JS_IsException(value));
    armed = 1;
    return value;
}

int main(void)
{
    static const JSClassDef element_def = {
        "StringifyElement", .finalizer = element_finalizer,
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue array, result, error, getter;
    JSAtom index;
    const char *text;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_NewClassID(&element_class);
    assert(JS_NewClass(rt, element_class, &element_def) == 0);
    array = JS_NewArray(ctx);
    assert(!JS_IsException(array));
    getter = JS_NewCFunction(ctx, get_element, "get element", 0);
    assert(!JS_IsException(getter));
    index = JS_NewAtom(ctx, "0");
    assert(index != JS_ATOM_NULL);
    assert(JS_DefinePropertyGetSet(ctx, array, index, getter, JS_UNDEFINED,
                                  JS_PROP_CONFIGURABLE) >= 0);
    JS_FreeAtom(ctx, index);
    assert(JS_SetPropertyStr(ctx, array, "length", JS_NewInt32(ctx, 1)) >= 0);
    fail_index = 1;
    result = JS_JSONStringify(ctx, array, JS_UNDEFINED, JS_UNDEFINED);
    assert(JS_IsException(result) && failures == 1);
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    JS_RunGC(rt);
    assert(finalized == 1);

    fail_index = 0;
    result = JS_JSONStringify(ctx, array, JS_UNDEFINED, JS_UNDEFINED);
    assert(!JS_IsException(result));
    text = JS_ToCString(ctx, result);
    assert(text && strcmp(text, "[{}]") == 0);
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, array);
    JS_RunGC(rt);
    assert(finalized == 2 && !JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
