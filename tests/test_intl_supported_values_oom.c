/* Inventory result construction must unwind partial arrays after allocation failure.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT. */
#include "oom-allocator.h"

#ifdef CONFIG_INTL
#include "pool-padding.h"

static void test_inventory(JSRuntime *rt, OOMTestAllocator *allocator, const char *key)
{
    JSContext *ctx = JS_NewContextRaw(rt);
    JSValue global, intl, function, argument, result;
    size_t step, samples = 0, calls = 0, failed = 0;

    assert(ctx && JS_AddIntrinsicBaseObjects(ctx) == 0);
    assert(JS_AddIntrinsicIntl(ctx) == 0);
    global = JS_GetGlobalObject(ctx);
    intl = JS_GetPropertyStr(ctx, global, "Intl");
    function = JS_GetPropertyStr(ctx, intl, "supportedValuesOf");
    argument = JS_NewString(ctx, key);
    assert(JS_IsFunction(ctx, function) && !JS_IsException(argument));
    /* Service tests exhaust their cold readers. Keep those immutable runtime
       caches warm here so bounded samples exercise the binding and its arrays. */
    result = JS_Call(ctx, function, JS_UNDEFINED, 1, &argument);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, result);
    for (step = 0; step <= samples; step++) {
        void *padding = test_pool_padding(rt);
        /* A reduced inventory may need just one real allocation. */
        size_t fail_at = !step ? 0 : samples == 1 ? 1 :
                        1 + (calls - 1) * (step - 1) / (samples - 1);

        oom_test_begin(allocator, fail_at);
        result = JS_Call(ctx, function, JS_UNDEFINED, 1, &argument);
        oom_test_end(allocator);
        test_pool_padding_free(rt, padding);
        if (!step) {
            assert(!JS_IsException(result));
            calls = allocator->calls;
            assert(calls && calls < 4096);
            /* Large catalogs repeat the same append operation hundreds of
               times. Sample across the entire array, including its last step. */
            samples = calls > 16 ? 16 : calls;
        } else {
            assert(allocator->failures == 1);
        }
        if (JS_IsException(result)) {
            failed++;
            oom_test_check_exception(ctx);
            result = JS_Call(ctx, function, JS_UNDEFINED, 1, &argument);
            assert(!JS_IsException(result));
        }
        assert(JS_IsArray(ctx, result) == 1 && !JS_HasException(ctx));
        JS_FreeValue(ctx, result);
    }
    assert(failed);
    JS_FreeValue(ctx, argument);
    JS_FreeValue(ctx, function);
    JS_FreeValue(ctx, intl);
    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
}
#endif

int main(void)
{
#ifdef CONFIG_INTL
    static const char *const keys[] = {
        "calendar", "collation", "currency", "numberingSystem", "timeZone", "unit"
    };
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    size_t i;

    for (i = 0; i < sizeof(keys) / sizeof(*keys); i++)
        test_inventory(rt, &allocator, keys[i]);
    JS_FreeRuntime(rt);
    assert(!allocator.live);
#endif
    return 0;
}
