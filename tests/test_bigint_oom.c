/* BigInt allocation-failure regression tests.
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
#include "oom-allocator.h"
#include "../src/quickjs/internal/bigint.h"

static void test_power_failure(void)
{
    size_t fail_at, allocations = 0;

    for (fail_at = 0; fail_at <= allocations; fail_at++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSBigIntBuf exponent_buf;
        JSBigInt *base, *exponent, *result;
        int len = 600 / sizeof(js_limb_t) + 1;

        assert(ctx);
        /* All products exceed allocator pools; exponent 3 exercises both
           squaring and the subsequent multiplication by the base. */
        base = js_bigint_new(ctx, len);
        assert(base);
        memset(base->tab, 0, len * sizeof(js_limb_t));
        base->tab[0] = 3;
        base->tab[len - 1] = 1;
        exponent = js_bigint_set_si(&exponent_buf, 3);
        oom_test_begin(&allocator, fail_at);
        result = js_bigint_pow(ctx, base, exponent);
        oom_test_end(&allocator);
        if (!fail_at) {
            allocations = allocator.calls;
            assert(allocations >= 3 && allocations < 32);
        } else {
            assert(allocator.failures == 1);
        }
        if (result) {
            assert(!JS_HasException(ctx));
            assert(result->tab[0] == 27);
            js_free_rt(rt, result);
        } else {
            assert(fail_at);
            oom_test_check_exception(ctx);
        }
        js_free_rt(rt, base);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
}

int main(void)
{
    test_power_failure();
    return 0;
}
