/*
 * QuickJS TypedArray internal state tests
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
#include <stdio.h>
#include <string.h>

#include "../src/quickjs/internal/object.h"
#include "../src/quickjs/builtins/array-buffer.h"
#include "../src/quickjs/builtins/typed-array.h"

static JSValue new_view(JSContext *ctx, JSValueConst buffer,
                        JSTypedArrayEnum type, int offset, int length)
{
    JSValueConst args[3] = {
        buffer, JS_NewInt32(ctx, offset),
        length < 0 ? JS_UNDEFINED : JS_NewInt32(ctx, length),
    };
    JSValue view = JS_NewTypedArray(ctx, 3, args, type);
    assert(!JS_IsException(view));
    return view;
}

static JSValue new_buffer(JSContext *ctx, const char *source)
{
    JSValue buffer = JS_Eval(ctx, source, strlen(source),
                             "typed-array-state", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(buffer));
    return buffer;
}

static void change_length(JSContext *ctx, JSValueConst buffer,
                           const char *method, int length)
{
    JSValue func = JS_GetPropertyStr(ctx, buffer, method);
    JSValueConst arg = JS_NewInt32(ctx, length);
    JSValue result;
    assert(!JS_IsException(func));
    result = JS_Call(ctx, func, buffer, 1, &arg);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, func);
}

static void check_view(JSValueConst view, BOOL oob, uint32_t count)
{
    JSObject *p = JS_VALUE_GET_OBJ(view);
    JSTypedArray *ta = p->u.typed_array;
    JSArrayBuffer *abuf = ta->buffer->u.array_buffer;

    assert(typed_array_is_oob(p) == oob);
    assert(p->u.array.count == count);
    if (oob) {
        assert(p->u.array.u.ptr == NULL);
    } else {
        assert(abuf->data != NULL);
        assert(p->u.array.u.ptr == abuf->data + ta->offset);
    }
}

static void check_views(JSValueConst *views, const BOOL *oob,
                         const uint32_t *counts, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        check_view(views[i], oob[i], counts[i]);
}

static void test_resizable_views(JSContext *ctx, JSTypedArrayEnum type,
                                 int element_size)
{
    static const BOOL all_valid[] = { FALSE, FALSE, FALSE, FALSE, FALSE, FALSE };
    static const BOOL one_element[] = { FALSE, FALSE, FALSE, TRUE, TRUE, TRUE };
    static const BOOL less_than_one[] = { FALSE, TRUE, FALSE, TRUE, TRUE, TRUE };
    static const uint32_t full_counts[] = { 4, 3, 0, 0, 1, 0 };
    static const uint32_t one_count[] = { 1, 0, 0, 0, 0, 0 };
    static const uint32_t zero_counts[] = { 0, 0, 0, 0, 0, 0 };
    JSValue views[6], buffer;
    char source[128];
    size_t i;

    snprintf(source, sizeof(source), "new ArrayBuffer(%d, { maxByteLength: %d })",
              4 * element_size, 4 * element_size);
    buffer = new_buffer(ctx, source);
    views[0] = new_view(ctx, buffer, type, 0, -1);
    views[1] = new_view(ctx, buffer, type, element_size, -1);
    views[2] = new_view(ctx, buffer, type, 0, 0);
    views[3] = new_view(ctx, buffer, type, 4 * element_size, 0);
    views[4] = new_view(ctx, buffer, type, element_size, 1);
    views[5] = new_view(ctx, buffer, type, 4 * element_size, -1);
    check_views(views, all_valid, full_counts, countof(views));
    change_length(ctx, buffer, "resize", element_size);
    check_views(views, one_element, one_count, countof(views));
    change_length(ctx, buffer, "resize", element_size - 1);
    check_views(views, less_than_one, zero_counts, countof(views));
    change_length(ctx, buffer, "resize", 0);
    check_views(views, less_than_one, zero_counts, countof(views));
    change_length(ctx, buffer, "resize", 0);
    check_views(views, less_than_one, zero_counts, countof(views));
    change_length(ctx, buffer, "resize", 4 * element_size);
    check_views(views, all_valid, full_counts, countof(views));
    JS_DetachArrayBuffer(ctx, buffer);
    for (i = 0; i < countof(views); i++) {
        check_view(views[i], TRUE, 0);
        JS_FreeValue(ctx, views[i]);
    }
    JS_FreeValue(ctx, buffer);
}

static void test_empty_views(JSContext *ctx, JSTypedArrayEnum type,
                             int element_size)
{
    static const char *sources[] = {
        "new ArrayBuffer(0)",
        "new ArrayBuffer(0, { maxByteLength: 32 })",
        "new SharedArrayBuffer(0)",
        "new SharedArrayBuffer(0, { maxByteLength: 32 })",
    };
    JSValue buffer, tracking, fixed;
    size_t i;

    for (i = 0; i < countof(sources); i++) {
        buffer = new_buffer(ctx, sources[i]);
        tracking = new_view(ctx, buffer, type, 0, -1);
        fixed = new_view(ctx, buffer, type, 0, 0);
        check_view(tracking, FALSE, 0);
        check_view(fixed, FALSE, 0);
        if (i == 1 || i == 3) {
            const char *method = i == 1 ? "resize" : "grow";
            change_length(ctx, buffer, method, element_size - 1);
            check_view(tracking, FALSE, 0);
            check_view(fixed, FALSE, 0);
            change_length(ctx, buffer, method, 4 * element_size);
            check_view(tracking, FALSE, 4);
            check_view(fixed, FALSE, 0);
        }
        JS_FreeValue(ctx, fixed);
        JS_FreeValue(ctx, tracking);
        JS_FreeValue(ctx, buffer);
    }
}

struct UnalignedBuffer {
    uint8_t *ptr;
    int freed;
};

static void free_unaligned_buffer(JSRuntime *rt, void *opaque, void *ptr)
{
    struct UnalignedBuffer *buffer = opaque;
    assert(ptr == buffer->ptr);
    buffer->freed++;
}

static void test_unaligned_views(JSContext *ctx, JSTypedArrayEnum type,
                                 int element_size)
{
    static const char source[] =
        "(function(a) {"
        "  const C = a.constructor;"
        "  const bigint = typeof a[0] === 'bigint';"
        "  const floating = C === Float16Array || C === Float32Array ||"
        "                   C === Float64Array;"
        "  const scalar = bigint ? BigInt : x => x;"
        "  const values = [4, 1, 7, 2, 6, 3, 5, 0].map(scalar);"
        "  function check(actual, expected, label) {"
        "    if (actual.length !== expected.length) throw Error(label);"
        "    for (let i = 0; i < expected.length; i++)"
        "      if (!Object.is(actual[i], expected[i])) throw Error(label);"
        "  }"
        "  function search(value, first, last) {"
        "    if (!a.includes(value) || a.indexOf(value) !== first ||"
        "        a.lastIndexOf(value) !== last) throw Error('search');"
        "  }"
        "  for (let i = 0; i < values.length; i++) a[i] = values[i];"
        "  check(a, values, 'indexed access');"
        "  Object.defineProperty(a, '2', { value: scalar(6) });"
        "  if (a['2'] !== scalar(6)) throw Error('defineProperty');"
        "  Reflect.set(a, '2', scalar(7));"
        "  check(a, values, 'Reflect.set');"
        "  check(Array.from(a), values, 'iterator');"
        "  if (a.at(-2) !== scalar(5)) throw Error('at');"
        "  if (a.find(x => x === scalar(6)) !== scalar(6) ||"
        "      a.findLastIndex(x => x === scalar(1)) !== 1)"
        "    throw Error('find');"
        "  a.fill(scalar(3));"
        "  check(a, new Array(8).fill(scalar(3)), 'fill');"
        "  search(scalar(3), 0, 7);"
        "  a.set(values);"
        "  search(scalar(2), 3, 3);"
        "  a.sort();"
        "  check(a, [0, 1, 2, 3, 4, 5, 6, 7].map(scalar), 'sort');"
        "  a.reverse();"
        "  check(a, [7, 6, 5, 4, 3, 2, 1, 0].map(scalar), 'reverse');"
        "  a.sort((x, y) => x < y ? -1 : x > y ? 1 : 0);"
        "  check(a, [0, 1, 2, 3, 4, 5, 6, 7].map(scalar), 'custom sort');"
        "  check(a.slice(2, 5), [2, 3, 4].map(scalar), 'slice');"
        "  check(a.subarray(2, 5), [2, 3, 4].map(scalar), 'subarray');"
        "  check(a.toReversed(), [7, 6, 5, 4, 3, 2, 1, 0].map(scalar),"
        "        'toReversed');"
        "  check(a.toSorted((x, y) => x < y ? 1 : x > y ? -1 : 0),"
        "        [7, 6, 5, 4, 3, 2, 1, 0].map(scalar), 'toSorted');"
        "  check(a.with(0, scalar(7)), [7, 1, 2, 3, 4, 5, 6, 7].map(scalar),"
        "        'with');"
        "  a.copyWithin(2, 0, 2);"
        "  check(a, [0, 1, 0, 1, 4, 5, 6, 7].map(scalar), 'copyWithin');"
        "  if (floating) {"
        "    a.set([-0, 0, NaN, -Infinity, Infinity, 1.5, -2.5, NaN]);"
        "    search(0, 0, 1);"
        "    if (!a.includes(NaN) || a.indexOf(NaN) !== -1 ||"
        "        a.lastIndexOf(NaN) !== -1) throw Error('NaN search');"
        "    a.sort();"
        "    check(a, [-Infinity, -2.5, -0, 0, 1.5, Infinity, NaN, NaN],"
        "          'floating sort');"
        "    a.reverse();"
        "    check(a, [NaN, NaN, Infinity, 1.5, 0, -0, -2.5, -Infinity],"
        "          'floating reverse');"
        "    a.fill(-2.5);"
        "    search(-2.5, 0, 7);"
        "  } else if (C === Int8Array || C === Int16Array ||"
        "             C === Int32Array || C === BigInt64Array) {"
        "    a.fill(scalar(-7));"
        "    search(scalar(-7), 0, 7);"
        "    a[0] = scalar(-3);"
        "    if (a[0] !== scalar(-3)) throw Error('signed indexed access');"
        "    a.sort();"
        "    if (a[7] !== scalar(-3)) throw Error('signed sort');"
        "  }"
        "  if (C === BigInt64Array || C === BigUint64Array) {"
        "    a.fill(0xffffffffffffffffn);"
        "    search(C === BigInt64Array ? -1n : 0xffffffffffffffffn, 0, 7);"
        "  } else if (C === Uint16Array || C === Uint32Array) {"
        "    const high = C === Uint16Array ? 65535 : 4294967295;"
        "    a.fill(high);"
        "    search(high, 0, 7);"
        "    a[0] = 0; a.sort();"
        "    if (a[0] !== 0 || a[7] !== high) throw Error('unsigned sort');"
        "  }"
        "  a.set(values);"
        "  const dv = new DataView(a.buffer, a.byteOffset, a.byteLength);"
        "  const little = new Uint8Array(new Uint16Array([1]).buffer)[0] === 1;"
        "  const getter = C === Uint8ClampedArray ? 'getUint8' :"
        "                 'get' + C.name.slice(0, -5);"
        "  if (dv[getter](0, little) !== scalar(4)) throw Error('native bytes');"
        "})";
    union {
        uint64_t alignment;
        double double_alignment;
        uint8_t bytes[9 * 8 + 16];
    } storage;
    JSValue function, buffer, view, result;
    struct UnalignedBuffer owner;
    uint8_t *data;
    size_t length, i;
    int misalignment;

    function = JS_Eval(ctx, source, strlen(source), "unaligned-typed-array",
                       JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(function));
    for (misalignment = 1; misalignment < 8; misalignment++) {
        memset(storage.bytes, 0xa5, sizeof(storage.bytes));
        data = storage.bytes + misalignment;
        owner.ptr = data;
        owner.freed = 0;
        buffer = JS_NewArrayBuffer(ctx, data, 9 * element_size,
                                   free_unaligned_buffer, &owner, FALSE);
        assert(!JS_IsException(buffer));
        assert(JS_GetArrayBuffer(ctx, &length, buffer) == data);
        assert(length == (size_t)9 * element_size);
        view = new_view(ctx, buffer, type, element_size, 8);
        result = JS_Call(ctx, function, JS_UNDEFINED, 1, &view);
        if (JS_IsException(result)) {
            JSValue exception = JS_GetException(ctx);
            const char *message = JS_ToCString(ctx, exception);
            fprintf(stderr, "type %d, offset %d: %s\n", type, misalignment,
                     message ? message : "exception");
            JS_FreeCString(ctx, message);
            JS_FreeValue(ctx, exception);
            assert(!JS_IsException(result));
        }
        JS_FreeValue(ctx, result);
        for (i = 0; i < (size_t)misalignment + element_size; i++)
            assert(storage.bytes[i] == 0xa5);
        for (i = misalignment + 9 * element_size;
             i < sizeof(storage.bytes); i++)
            assert(storage.bytes[i] == 0xa5);
        JS_FreeValue(ctx, view);
        JS_FreeValue(ctx, buffer);
        assert(owner.freed == 1);
    }
    JS_FreeValue(ctx, function);
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    int type;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    for (type = JS_TYPED_ARRAY_UINT8C; type <= JS_TYPED_ARRAY_FLOAT64; type++) {
        int element_size = 1 << typed_array_size_log2[type];
        test_resizable_views(ctx, type, element_size);
        test_empty_views(ctx, type, element_size);
        test_unaligned_views(ctx, type, element_size);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
