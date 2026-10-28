/* TypedArray sort allocation failure and 32-bit size overflow.
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
#include "../src/quickjs/builtins/typed-array.h"
#include "../src/quickjs/builtins/array-buffer.h"

static void *sort_malloc(JSContext *ctx, size_t size);
#define js_malloc sort_malloc
#include "../src/quickjs/builtins/typed-array.c"
#undef js_malloc

static int fail_next, reject_large, failures, comparisons;
static int bigint_gets, bigint_fail_at, bigint_comparisons;

static void *sort_malloc(JSContext *ctx, size_t size)
{
    /* A fabricated external length must never reach allocation or memcpy. */
    assert(!reject_large || size < (size_t)0x40000000);
    if (fail_next) {
        fail_next = 0;
        failures++;
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return js_malloc(ctx, size);
}

static JSValue compare(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv)
{
    int32_t a, b;
    assert(argc == 2);
    assert(JS_ToInt32(ctx, &a, argv[0]) == 0);
    assert(JS_ToInt32(ctx, &b, argv[1]) == 0);
    comparisons++;
    return JS_NewInt32(ctx, a % 10 - b % 10);
}

static JSValue compare_bigint(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    assert(argc == 2);
    assert(!JS_IsException(argv[0]) && !JS_IsException(argv[1]));
    bigint_comparisons++;
    return JS_NewInt32(ctx, 0);
}

static JSValue get_bigint_or_fail(JSContext *ctx, const void *a)
{
    if (++bigint_gets == bigint_fail_at)
        return JS_ThrowOutOfMemory(ctx);
    return js_TA_get_uint64(ctx, a);
}

static JSValue view_of(JSContext *ctx, uint8_t *storage, size_t length)
{
    JSValue buffer, view;
    JSValueConst args[3];
    buffer = JS_NewArrayBuffer(ctx, storage, length, NULL, NULL, FALSE);
    assert(!JS_IsException(buffer));
    args[0] = buffer;
    args[1] = JS_NewInt32(ctx, 0);
    args[2] = JS_NewInt64(ctx, length);
    view = JS_NewTypedArray(ctx, 3, args, JS_TYPED_ARRAY_UINT8);
    JS_FreeValue(ctx, buffer);
    assert(!JS_IsException(view));
    return view;
}

static void clear_error(JSContext *ctx)
{
    JSValue error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
}

static void test_bigint_comparator_oom(JSContext *ctx)
{
    uint64_t values[2] = { UINT64_MAX, UINT64_MAX - 1 };
    uint32_t first = 0, second = 1;
    struct TA_sort_context sort_ctx;
    JSValue callback;
    int result;

    callback = JS_NewCFunction(ctx, compare_bigint, "compareBigInt", 2);
    assert(!JS_IsException(callback));
    sort_ctx.ctx = ctx;
    sort_ctx.array = (uint8_t *)values;
    sort_ctx.cmp = callback;
    sort_ctx.getfun = get_bigint_or_fail;
    sort_ctx.elt_size = sizeof(values[0]);

    /* Fail each argument conversion in turn, independent of allocator pools. */
    for (bigint_fail_at = 1; bigint_fail_at <= 2; bigint_fail_at++) {
        bigint_gets = bigint_comparisons = 0;
        sort_ctx.exception = 0;
        result = js_TA_cmp_generic(&first, &second, &sort_ctx);
        assert(result == 0 && sort_ctx.exception == 1);
        assert(bigint_gets == bigint_fail_at && bigint_comparisons == 0);
        clear_error(ctx);
    }
    JS_FreeValue(ctx, callback);
}

int main(void)
{
    uint8_t bytes[] = { 22, 11, 12, 21 };
    static const uint8_t sorted[] = { 11, 21, 22, 12 };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue view, callback, result;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    callback = JS_NewCFunction(ctx, compare, "compare", 2);
    assert(!JS_IsException(callback));
    view = view_of(ctx, bytes, sizeof(bytes));
    fail_next = 1;
    result = js_typed_array_sort(ctx, view, 1, (JSValueConst *)&callback);
    assert(JS_IsException(result) && failures == 1 && comparisons == 0);
    clear_error(ctx);
    assert(bytes[0] == 22 && bytes[1] == 11 && bytes[2] == 12 && bytes[3] == 21);
    result = js_typed_array_sort(ctx, view, 1, (JSValueConst *)&callback);
    assert(!JS_IsException(result) && comparisons > 0);
    JS_FreeValue(ctx, result);
    assert(memcmp(bytes, sorted, sizeof(bytes)) == 0);
    JS_FreeValue(ctx, view);
#if SIZE_MAX == UINT32_MAX
    view = view_of(ctx, bytes, (size_t)0x40000001);
    comparisons = 0;
    reject_large = 1;
    result = js_typed_array_sort(ctx, view, 1, (JSValueConst *)&callback);
    reject_large = 0;
    assert(JS_IsException(result) && comparisons == 0);
    clear_error(ctx);
    JS_FreeValue(ctx, view);
#endif
    test_bigint_comparator_oom(ctx);
    JS_FreeValue(ctx, callback);
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
