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
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
