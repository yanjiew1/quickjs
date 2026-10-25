/* Allocation failures during locale resolution, UTF-16 conversion and casing.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT.
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
#include "oom-allocator.h"

#ifdef CONFIG_INTL
#include "pool-padding.h"

static void test_method(JSRuntime *rt, OOMTestAllocator *allocator,
                        const char *name, const char *tag)
{
    static const uint16_t units[] = {
        'I', 0x323, 0x307, 'J', 0x301, 'i', 0x323, 0x307, 0xcc, 0xd801, 0xdc28
    };
    JSContext *ctx = JS_NewContextRaw(rt);
    JSValue global, constructor, prototype, method, input, locale, expected, result;
    size_t step, calls = 0, failures = 0;

    assert(ctx && !JS_AddIntrinsicBaseObjects(ctx));
    global = JS_GetGlobalObject(ctx);
    constructor = JS_GetPropertyStr(ctx, global, "String");
    prototype = JS_GetPropertyStr(ctx, constructor, "prototype");
    method = JS_GetPropertyStr(ctx, prototype, name);
    input = JS_NewStringUTF16(ctx, units, sizeof(units) / sizeof(*units));
    locale = JS_NewString(ctx, tag);
    assert(JS_IsFunction(ctx, method) && !JS_IsException(input) && !JS_IsException(locale));
    /* Locale-reader failures are exhausted by service tests. Retain a warm
       reader here and exhaust the small, observable String operation. */
    expected = JS_Call(ctx, method, input, 1, &locale);
    assert(!JS_IsException(expected));
    for (step = 0; step <= calls; step++) {
        void *padding = test_pool_padding(rt);
        oom_test_begin(allocator, step);
        result = JS_Call(ctx, method, input, 1, &locale);
        oom_test_end(allocator);
        test_pool_padding_free(rt, padding);
        if (!step) {
            calls = allocator->calls;
            assert(calls && calls < 128);
        } else {
            assert(allocator->failures == 1);
        }
        if (JS_IsException(result)) {
            failures++;
            oom_test_check_exception(ctx);
            result = JS_Call(ctx, method, input, 1, &locale);
            assert(!JS_IsException(result));
        }
        assert(JS_StrictEq(ctx, result, expected) && !JS_HasException(ctx));
        JS_FreeValue(ctx, result);
    }
    assert(failures);
    /* The explicit default override rejects locales absent from a reduced
       profile. Use English there; native also exercises Turkish/Lithuanian
       defaults. Omitted locales must use the runtime's resolved value. */
    if (JS_SetIntlDefaultLocale(ctx, tag) < 0) {
        JSValue error = JS_GetException(ctx);
        JSValue range_error = JS_GetPropertyStr(ctx, global, "RangeError");
        assert(JS_IsInstanceOf(ctx, error, range_error) == 1);
        JS_FreeValue(ctx, range_error);
        JS_FreeValue(ctx, error);
        assert(JS_SetIntlDefaultLocale(ctx, "en") == 0);
    }
    result = JS_GetIntlDefaultLocale(ctx);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, expected);
    expected = JS_Call(ctx, method, input, 1, &result);
    JS_FreeValue(ctx, result);
    assert(!JS_IsException(expected));
    result = JS_Call(ctx, method, input, 0, NULL);
    assert(!JS_IsException(result) && JS_StrictEq(ctx, result, expected));
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, expected);
    JS_FreeValue(ctx, locale);
    JS_FreeValue(ctx, input);
    JS_FreeValue(ctx, method);
    JS_FreeValue(ctx, prototype);
    JS_FreeValue(ctx, constructor);
    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
}
#endif

int main(void)
{
#ifdef CONFIG_INTL
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    test_method(rt, &allocator, "toLocaleLowerCase", "tr");
    test_method(rt, &allocator, "toLocaleUpperCase", "tr");
    test_method(rt, &allocator, "toLocaleLowerCase", "lt");
    test_method(rt, &allocator, "toLocaleUpperCase", "lt");
    JS_FreeRuntime(rt);
    assert(!allocator.live);
#endif
    return 0;
}
