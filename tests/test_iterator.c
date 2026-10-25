/*
 * QuickJS Iterator Number counter boundary tests
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
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

/* Use the actual owner and helper state without adding an engine test API. */
#include "../src/quickjs/builtins/iterator.c"

static void check_iterator_counter(JSContext *ctx, int kind, int64_t start)
{
    static const char *sources[] = {
        "globalThis.iteratorCounterIndices = [];"
        "[10, 20, 30, 40].values().map((value, index) => {"
        "  iteratorCounterIndices.push(index); return value;"
        "});",
        "globalThis.iteratorCounterIndices = [];"
        "[10, 20, 30, 40].values().filter((value, index) => {"
        "  iteratorCounterIndices.push(index);"
        "  return iteratorCounterIndices.length === 4;"
        "});",
        "globalThis.iteratorCounterIndices = [];"
        "[10, 20, 30, 40].values().flatMap((value, index) => {"
        "  iteratorCounterIndices.push(index);"
        "  return iteratorCounterIndices.length === 4 ? [value] : [];"
        "});",
    };
    static const JSIteratorHelperKindEnum kinds[] = {
        JS_ITERATOR_HELPER_KIND_MAP,
        JS_ITERATOR_HELPER_KIND_FILTER,
        JS_ITERATOR_HELPER_KIND_FLAT_MAP,
    };
    JSIteratorHelperData *state;
    JSValue helper, result, value, global, indices;
    const double limit = 9007199254740992.0;
    double index;
    int32_t length, item;
    int yields = kind == 0 ? 4 : 1;
    int i;

    helper = JS_Eval(ctx, sources[kind], strlen(sources[kind]),
                     "test_iterator_counter", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(helper));
    state = JS_GetOpaque(helper, JS_CLASS_ITERATOR_HELPER);
    assert(state && state->kind == kinds[kind] && state->count == 0);
    /* Seed the legitimate long-running state instead of looping 2^53 times. */
    state->count = start;
    for (i = 0; i < yields; i++) {
        result = JS_Invoke(ctx, helper, JS_ATOM_next, 0, NULL);
        assert(!JS_IsException(result));
        value = JS_GetProperty(ctx, result, JS_ATOM_done);
        assert(JS_IsBool(value) && !JS_ToBool(ctx, value));
        JS_FreeValue(ctx, value);
        value = JS_GetProperty(ctx, result, JS_ATOM_value);
        assert(JS_ToInt32(ctx, &item, value) == 0);
        assert(item == (kind == 0 ? 10 * (i + 1) : 40));
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, result);
    }
    global = JS_GetGlobalObject(ctx);
    indices = JS_GetPropertyStr(ctx, global, "iteratorCounterIndices");
    assert(!JS_IsException(indices));
    value = JS_GetProperty(ctx, indices, JS_ATOM_length);
    assert(JS_ToInt32(ctx, &length, value) == 0 && length == 4);
    JS_FreeValue(ctx, value);
    for (i = 0; i < 4; i++) {
        value = JS_GetPropertyUint32(ctx, indices, i);
        assert(JS_IsNumber(value) && JS_ToFloat64(ctx, &index, value) == 0);
        assert(index == (i == 0 ? (double)start : limit));
        JS_FreeValue(ctx, value);
    }
    assert(state->count == INT64_C(9007199254740992));
    result = JS_Invoke(ctx, helper, JS_ATOM_next, 0, NULL);
    assert(!JS_IsException(result));
    value = JS_GetProperty(ctx, result, JS_ATOM_done);
    assert(JS_IsBool(value) && JS_ToBool(ctx, value));
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, indices);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, helper);
}

static void check_counter_arithmetic(JSContext *ctx)
{
    const int64_t starts[] = { 0, 1, INT32_MAX - 1LL, INT32_MAX,
        INT64_C(9007199254740990), INT64_C(9007199254740991),
        INT64_C(9007199254740992) };
    for (unsigned i = 0; i < countof(starts); i++) {
        int64_t counter = starts[i];
        double expected = starts[i];
        for (int j = 0; j < 4; j++) {
            JSValue value = JS_NewInt64(ctx, counter);
            double actual;
            assert(JS_ToFloat64(ctx, &actual, value) == 0 && actual == expected);
            if (expected <= INT32_MAX)
                assert(JS_VALUE_GET_TAG(value) == JS_TAG_INT);
            JS_FreeValue(ctx, value);
            counter = js_iterator_increment_counter(counter);
            expected += 1;
        }
    }
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    int kind;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    for (kind = 0; kind < 3; kind++) {
        check_iterator_counter(ctx, kind, INT64_C(9007199254740991));
        check_iterator_counter(ctx, kind, INT64_C(9007199254740992));
    }
    check_counter_arithmetic(ctx);
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
