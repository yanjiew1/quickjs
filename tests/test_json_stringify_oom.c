/* JSON array-element ownership when its index conversion fails.
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
#include "../src/quickjs/value/conversion.h"
#include "oom-allocator.h"
#include "pool-padding.h"

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

static void test_replacer_list_oom(void)
{
    static const char source[] =
        "Array.from({ length: 80 }, (_, i) => 'k' + i)";
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue object, replacer, result;
        void *padding;
        const char *text;

        assert(ctx);
        object = JS_NewObject(ctx);
        assert(!JS_IsException(object));
        assert(JS_SetPropertyStr(ctx, object, "k0", JS_NewInt32(ctx, 1)) >= 0);
        assert(JS_SetPropertyStr(ctx, object, "k79", JS_NewInt32(ctx, 2)) >= 0);
        replacer = JS_Eval(ctx, source, sizeof(source) - 1, "replacer-oom",
                           JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(replacer));
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        result = JS_JSONStringify(ctx, object, replacer, JS_UNDEFINED);
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 128);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_IsException(result)) {
            assert(step);
            oom_test_check_exception(ctx);
        } else {
            assert(!JS_HasException(ctx));
            text = JS_ToCString(ctx, result);
            assert(text && !strcmp(text, "{\"k0\":1,\"k79\":2}"));
            JS_FreeCString(ctx, text);
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, replacer);
        JS_FreeValue(ctx, object);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

int main(void)
{
    test_replacer_list_oom();
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
