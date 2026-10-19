/* ListFormat binding failures with warmed native services and real allocation. */
#include "oom-allocator.h"
#ifdef CONFIG_INTL
#include "pool-padding.h"

static void test_operation(int operation)
{
    size_t step, calls = 0, failures = 0;
    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContextRaw(rt);
        JSValue global, intl, constructor, argument, instance, method, result;
        JSValueConst input;
        void *padding;
        int error;
        assert(ctx && !JS_AddIntrinsicBaseObjects(ctx) && !JS_AddIntrinsicIntl(ctx));
        global = JS_GetGlobalObject(ctx);
        intl = JS_GetPropertyStr(ctx, global, "Intl");
        constructor = JS_GetPropertyStr(ctx, intl, "ListFormat");
        argument = JS_NewString(ctx, "en-u-kn");
        input = argument;
        instance = JS_CallConstructor(ctx, constructor, 1, &input);
        assert(!JS_IsException(instance));
        method = operation == 1
                     ? JS_GetPropertyStr(ctx, constructor, "supportedLocalesOf")
                     : JS_GetPropertyStr(ctx, instance, "resolvedOptions");
        assert(JS_IsFunction(ctx, method));
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        if (operation == 0)
            result = JS_CallConstructor(ctx, constructor, 1, &input);
        else if (operation == 1)
            result = JS_Call(ctx, method, constructor, 1, &input);
        else
            result = JS_Call(ctx, method, instance, 0, NULL);
        oom_test_end(&allocator);
        test_pool_padding_free(rt, padding);
        error = JS_IsException(result);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 256 && !error);
        } else {
            assert(allocator.failures == 1);
        }
        if (error) {
            failures++;
            oom_test_check_exception(ctx);
            if (operation == 0)
                result = JS_CallConstructor(ctx, constructor, 1, &input);
            else if (operation == 1)
                result = JS_Call(ctx, method, constructor, 1, &input);
            else
                result = JS_Call(ctx, method, instance, 0, NULL);
        }
        assert(!JS_IsException(result) && !JS_HasException(ctx));
        if (operation == 0) {
            JSValue resolved = JS_Call(ctx, method, result, 0, NULL);
            JS_FreeValue(ctx, result);
            result = resolved;
        }
        {
            JSValue value = operation == 1 ? JS_GetPropertyUint32(ctx, result, 0)
                                           : JS_GetPropertyStr(ctx, result, "locale");
            const char *text = JS_ToCString(ctx, value);
            assert(text && !strcmp(text, operation == 1 ? "en-u-kn" : "en"));
            JS_FreeCString(ctx, text);
            JS_FreeValue(ctx, value);
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, method);
        JS_FreeValue(ctx, instance);
        JS_FreeValue(ctx, argument);
        JS_FreeValue(ctx, constructor);
        JS_FreeValue(ctx, intl);
        JS_FreeValue(ctx, global);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
    assert(failures);
}
#endif

int main(void)
{
#ifdef CONFIG_INTL
    test_operation(0);
    test_operation(1);
    test_operation(2);
#endif
    return 0;
}
