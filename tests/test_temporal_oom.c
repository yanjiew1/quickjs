/* Partial Temporal intrinsic initialization must roll back and permit retry.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT.
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
