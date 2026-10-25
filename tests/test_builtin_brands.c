/* Nonobservable builtin brand queries through the embedding API.
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
#include "quickjs.h"

typedef JS_BOOL BrandQuery(JSValueConst value);

typedef struct BrandCase {
    BrandQuery *query;
    const char *instance;
    const char *subclass;
} BrandCase;

static const BrandCase cases[] = {
    { JS_IsMap, "new Map()", "new (class extends Map {})()" },
    { JS_IsSet, "new Set()", "new (class extends Set {})()" },
    { JS_IsWeakMap, "new WeakMap()", "new (class extends WeakMap {})()" },
    { JS_IsWeakSet, "new WeakSet()", "new (class extends WeakSet {})()" },
    { JS_IsWeakRef, "new WeakRef({})", "new (class extends WeakRef {})({})" },
    { JS_IsRegExp, "/x/", "new (class extends RegExp {})('x')" },
    { JS_IsDate, "new Date()", "new (class extends Date {})()" },
    { JS_IsProxy, "new Proxy({}, {})", NULL },
    { JS_IsArrayBuffer, "new ArrayBuffer(8)",
      "new (class extends ArrayBuffer {})(8)" },
    { JS_IsSharedArrayBuffer, "new SharedArrayBuffer(8)",
      "new (class extends SharedArrayBuffer {})(8)" },
    { JS_IsDataView, "new DataView(new ArrayBuffer(8))",
      "new (class extends DataView {})(new ArrayBuffer(8))" },
    { JS_IsPromise, "Promise.resolve(1)",
      "new (class extends Promise {})(resolve => resolve(1))" },
    { JS_IsAsyncFunction, "(async function () {})", NULL },
};

static JSValue eval(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source), "<builtin-brands>",
                            JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(value));
    return value;
}

static void check_brand(JSValueConst value, BrandQuery *expected)
{
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        assert(!!cases[i].query(value) == (cases[i].query == expected));
}

static void test_instances(JSContext *ctx, JSContext *other)
{
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        JSValue value = eval(ctx, cases[i].instance);
        JSValue foreign = eval(other, cases[i].instance);

        check_brand(value, cases[i].query);
        check_brand(foreign, cases[i].query);
        JS_Throw(ctx, JS_NewInt32(ctx, 57));
        check_brand(value, cases[i].query);
        assert(JS_HasException(ctx));
        {
            JSValue error = JS_GetException(ctx);
            assert(JS_IsNumber(error) && JS_VALUE_GET_INT(error) == 57);
            JS_FreeValue(ctx, error);
        }
        JS_FreeValue(ctx, value);
        JS_FreeValue(other, foreign);
        if (cases[i].subclass) {
            value = eval(ctx, cases[i].subclass);
            check_brand(value, cases[i].query);
            JS_FreeValue(ctx, value);
        }
    }
}

static void test_noninstances(JSContext *ctx)
{
    const char *sources[] = {
        "null", "undefined", "true", "1", "'x'", "1n", "Symbol('x')",
        "({})", "[]", "Object.create(Map.prototype)",
        "({ [Symbol.toStringTag]: 'Promise' })", "(function () {})",
        "(async function* () {})", "(async function () {}).bind(null)",
    };
    size_t i;
    JSValue value, error;

    for (i = 0; i < sizeof(sources) / sizeof(sources[0]); i++) {
        value = eval(ctx, sources[i]);
        check_brand(value, NULL);
        JS_FreeValue(ctx, value);
    }
    value = eval(ctx,
        "new Proxy(new Map(), { get() { throw 1; },"
        " getPrototypeOf() { throw 2; } })");
    check_brand(value, JS_IsProxy);
    JS_FreeValue(ctx, value);
    value = eval(ctx,
        "(() => { let r = Proxy.revocable([], {});"
        " r.revoke(); return r.proxy; })()");
    check_brand(value, JS_IsProxy);
    assert(!JS_HasException(ctx));
    assert(JS_IsArray(ctx, value) == -1);
    error = JS_GetException(ctx);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, value);
    value = eval(ctx, "new Proxy([], {})");
    assert(JS_IsArray(ctx, value) == 1);
    JS_FreeValue(ctx, value);
}

static void test_buffer_states(JSContext *ctx)
{
    const char *views[] = {
        ("(() => { let b = new ArrayBuffer(8); let v = new DataView(b);"
         " b.transfer(); return v; })()"),
        ("(() => { let b = new ArrayBuffer(8, { maxByteLength: 16 });"
         " let v = new DataView(b, 4); b.resize(2); return v; })()"),
    };
    size_t i;
    JSValue value;

    for (i = 0; i < sizeof(views) / sizeof(views[0]); i++) {
        value = eval(ctx, views[i]);
        check_brand(value, JS_IsDataView);
        JS_FreeValue(ctx, value);
    }
    value = eval(ctx,
        "(() => { let b = new ArrayBuffer(8); b.transfer(); return b; })()");
    check_brand(value, JS_IsArrayBuffer);
    JS_FreeValue(ctx, value);
    value = eval(ctx,
        "(() => { let b = new ArrayBuffer(8); let v = new Uint8Array(b);"
        " b.transfer(); return v; })()");
    assert(JS_GetTypedArrayType(value) == JS_TYPED_ARRAY_UINT8);
    JS_FreeValue(ctx, value);
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx, *other;

    assert(rt);
    ctx = JS_NewContext(rt);
    other = JS_NewContext(rt);
    assert(ctx && other);
    test_instances(ctx, other);
    test_noninstances(ctx);
    test_buffer_states(ctx);
    JS_FreeContext(other);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
