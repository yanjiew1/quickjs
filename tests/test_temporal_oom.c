/* Partial Temporal intrinsic initialization must roll back and permit retry.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT.
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
#ifdef CONFIG_TEMPORAL
#include "oom-allocator.h"

int main(void)
{
    size_t allocations = 0, samples = 0, step;
    unsigned failed = 0;

    /* Sample actual allocator requests across the complete operation. Fresh
       runtimes keep pool state reproducible; failure injection counts only
       requests that reach the allocator, including when pools are enabled. */
    for (step = 0; step <= samples; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContextRaw(rt);
        JSValue value, global;
        uint64_t low, high;
        size_t fail_at = step ? (samples == 1 ? 1 :
            1 + (allocations - 1) * (step - 1) / (samples - 1)) : 0;
        int result;

        assert(ctx && JS_AddIntrinsicBaseObjects(ctx) == 0);
        oom_test_begin(&allocator, fail_at);
        result = JS_AddIntrinsicTemporal(ctx);
        oom_test_end(&allocator);
        if (!step) {
            allocations = allocator.calls;
            assert(allocations > 0 && allocations < 10000);
            samples = allocations < 32 ? allocations : 32;
            assert(result == 0 && !JS_HasException(ctx));
        } else {
            assert(allocator.failures == 1);
        }
        if (result) {
            failed++;
            oom_test_check_exception(ctx);
            global = JS_GetGlobalObject(ctx);
            value = JS_GetPropertyStr(ctx, global, "Temporal");
            assert(JS_IsUndefined(value));
            JS_FreeValue(ctx, value);
            JS_FreeValue(ctx, global);
            assert(JS_AddIntrinsicTemporal(ctx) == 0);
        } else {
            assert(!JS_HasException(ctx));
        }
        value = JS_NewTemporalInstant(ctx, 42, 0);
        assert(!JS_IsException(value));
        assert(!JS_GetTemporalInstantEpochNanoseconds(ctx, value, &low, &high));
        assert(low == 42 && high == 0);
        JS_FreeValue(ctx, value);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
    assert(failed > 0);
    return 0;
}
#else
int main(void)
{
    return 0;
}
#endif
