/* Intl initialization, disabled API and real allocator-failure contracts.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT. */
#include "oom-allocator.h"

#ifdef CONFIG_INTL
#include "pool-padding.h"
static void test_failures(int operation)
{
    size_t step, allocations = 0, samples = 0, failed = 0;

    for (step = 0; step <= samples; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContextRaw(rt);
        JSValue global, object = JS_UNDEFINED, function = JS_UNDEFINED;
        JSValue argument = JS_UNDEFINED, result = JS_UNDEFINED;
        void *padding;
        size_t fail_at = 0;
        int status;

        assert(ctx && JS_AddIntrinsicBaseObjects(ctx) == 0);
        global = JS_GetGlobalObject(ctx);
        if (operation) {
            assert(JS_AddIntrinsicIntl(ctx) == 0);
            object = JS_GetPropertyStr(ctx, global, "Intl");
            function = JS_GetPropertyStr(ctx, object, "getCanonicalLocales");
            argument = JS_NewString(ctx, "iw-Qaai-BU");
            assert(!JS_IsException(argument) && JS_IsFunction(ctx, function));
            if (operation == 2) {
                result = JS_Call(ctx, function, JS_UNDEFINED, 1, &argument);
                assert(!JS_IsException(result));
                JS_FreeValue(ctx, result);
                result = JS_UNDEFINED;
            }
        }
        padding = test_pool_padding(rt);
        if (step)
            fail_at = samples == 1 ? 1 :
                1 + (allocations - 1) * (step - 1) / (samples - 1);
        oom_test_begin(&allocator, fail_at);
        if (operation) {
            result = JS_Call(ctx, function, JS_UNDEFINED, 1, &argument);
            status = JS_IsException(result) ? -1 : 0;
        } else {
            status = JS_AddIntrinsicIntl(ctx);
        }
        oom_test_end(&allocator);
        test_pool_padding_free(rt, padding);
        if (!step) {
            allocations = allocator.calls;
            assert(allocations && allocations < 4096 && status == 0);
            /* The C service tests exhaust every data-reader allocation.
               Sample that cold path here; exhaust the much smaller binding
               paths with the service already open and during publication. */
            samples = operation == 1 && allocations > 16 ? 16 : allocations;
        } else {
            assert(allocator.failures == 1);
        }
        if (status < 0) {
            failed++;
            oom_test_check_exception(ctx);
            if (operation) {
                result = JS_Call(ctx, function, JS_UNDEFINED, 1, &argument);
                assert(!JS_IsException(result));
            } else {
                JSValue absent = JS_GetPropertyStr(ctx, global, "Intl");
                assert(JS_IsUndefined(absent));
                JS_FreeValue(ctx, absent);
                assert(JS_AddIntrinsicIntl(ctx) == 0);
            }
        } else {
            /* Optional engine cache allocations may fail without an error. */
            assert(!JS_HasException(ctx));
        }
        assert(JS_AddIntrinsicIntl(ctx) == 0);
        if (operation) {
            JSValue length = JS_GetPropertyStr(ctx, result, "length");
            JSValue first = JS_GetPropertyUint32(ctx, result, 0);
            const char *text = JS_ToCString(ctx, first);
            int32_t count;

            assert(JS_IsArray(ctx, result) == 1);
            assert(JS_ToInt32(ctx, &count, length) == 0 && count == 1);
            assert(text && !strcmp(text, "he-Zinh-MM"));
            JS_FreeCString(ctx, text);
            JS_FreeValue(ctx, first);
            JS_FreeValue(ctx, length);
        } else {
            object = JS_GetPropertyStr(ctx, global, "Intl");
            function = JS_GetPropertyStr(ctx, object, "getCanonicalLocales");
            assert(JS_IsObject(object) && JS_IsFunction(ctx, function));
        }
        assert(!JS_HasException(ctx));
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, argument);
        JS_FreeValue(ctx, function);
        JS_FreeValue(ctx, object);
        JS_FreeValue(ctx, global);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
    assert(failed);
}
#endif

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue global, value;

    assert(rt);
    ctx = JS_NewContextRaw(rt);
    assert(ctx && JS_AddIntrinsicBaseObjects(ctx) == 0);
    global = JS_GetGlobalObject(ctx);
#ifdef CONFIG_INTL
    assert(JS_AddIntrinsicIntl(ctx) == 0);
    value = JS_GetPropertyStr(ctx, global, "Intl");
    assert(JS_IsObject(value));
    JS_FreeValue(ctx, value);
    assert(JS_DefinePropertyValueStr(ctx, global, "Intl", JS_NULL,
                                     JS_PROP_C_W_E | JS_PROP_THROW) == 1);
    JS_RunGC(rt);
    assert(JS_AddIntrinsicIntl(ctx) == 0);
    value = JS_GetPropertyStr(ctx, global, "Intl");
    assert(JS_IsNull(value));
    JS_FreeValue(ctx, value);
#else
    assert(JS_AddIntrinsicIntl(ctx) < 0 && JS_HasException(ctx));
    value = JS_GetException(ctx);
    assert(JS_IsError(ctx, value));
    JS_FreeValue(ctx, value);
    value = JS_GetPropertyStr(ctx, global, "Intl");
    assert(JS_IsUndefined(value));
    JS_FreeValue(ctx, value);
#endif
    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
#ifdef CONFIG_INTL
    test_failures(0);
    test_failures(1);
    test_failures(2);
#endif
    return 0;
}
