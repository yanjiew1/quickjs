/* ListFormat binding failures with warmed native services and real allocation. */
#include "oom-allocator.h"
#ifdef CONFIG_INTL
#include "../src/quickjs/internal/runtime.h"
#include "../src/quickjs/internal/string.h"
#include "pool-padding.h"
#include "../src/quickjs/builtins/intl.h"

static void test_shared_admission_failures(void)
{
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContextRaw(rt);
    const QJSIntlData *data;
    QJSIntlLocaleService *locale;
    void *padding;
    int valid;

    assert(ctx && !JS_AddIntrinsicBaseObjects(ctx));
    assert(!rt->intl_data.pack.bytes);
    padding = test_pool_padding(rt);
    oom_test_begin(&allocator, 1);
    data = js_intl_data(ctx);
    oom_test_end(&allocator);
    test_pool_padding_free(rt, padding);
    assert(!data && allocator.failures == 1 && !rt->intl_data.pack.bytes);
    oom_test_check_exception(ctx);
    data = js_intl_data(ctx);
    assert(data == &rt->intl_data && data->pack.bytes);
    locale = js_intl_locale_service(ctx);
    assert(locale && !rt->intl_number);

    /* Exhaust pools again so failure reaches the next service's real allocator,
       rather than succeeding from an existing arena. Keep shared admission. */
    padding = test_pool_padding(rt);
    oom_test_begin(&allocator, 1);
    assert(!js_intl_number_service(ctx));
    oom_test_end(&allocator);
    test_pool_padding_free(rt, padding);
    assert(allocator.failures == 1 && !rt->intl_number);
    assert(js_intl_data(ctx) == data && rt->intl_locale == locale);
    oom_test_check_exception(ctx);
    assert(!qjs_intl_locale_validate(locale, (QJSIntlBytes){"en", 2}, &valid) && valid);
    assert(js_intl_number_service(ctx) && !JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(!allocator.live);
}

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

static void test_format(int parts)
{
    size_t step, calls = 0, failures = 0;
    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContextRaw(rt);
        JSValue global, intl, constructor, locale, instance, method;
        JSValue input, expected, result, rope;
        JSValueConst argument;
        uint16_t wide[] = {0x100, 0};
        char chunk[513];
        void *padding;
        int error;
        uint32_t index;
        assert(ctx && !JS_AddIntrinsicBaseObjects(ctx) && !JS_AddIntrinsicIntl(ctx));
        global = JS_GetGlobalObject(ctx);
        intl = JS_GetPropertyStr(ctx, global, "Intl");
        constructor = JS_GetPropertyStr(ctx, intl, "ListFormat");
        locale = JS_NewString(ctx, "en");
        argument = locale;
        instance = JS_CallConstructor(ctx, constructor, 1, &argument);
        method = JS_GetPropertyStr(ctx, instance, parts ? "formatToParts" : "format");
        input = JS_NewArray(ctx);
        memset(chunk, 'a', sizeof(chunk));
        rope = JS_ConcatString(ctx, JS_NewStringLen(ctx, chunk, sizeof(chunk)),
                               JS_NewStringLen(ctx, chunk, sizeof(chunk)));
        assert(JS_IsFunction(ctx, method) && !JS_IsException(input) &&
               !JS_IsException(rope));
        assert(JS_IsStringRope(rope));
        assert(JS_SetPropertyUint32(ctx, input, 0, rope) > 0);
        assert(JS_SetPropertyUint32(ctx, input, 1, JS_NewStringUTF16(ctx, wide, 2)) >
               0);
        assert(JS_SetPropertyUint32(ctx, input, 2, JS_NewString(ctx, "last")) > 0);
        /* More than eight entries also exercises retained-list growth. */
        for (index = 3; index < 9; index++)
            assert(JS_SetPropertyUint32(ctx, input, index, JS_NewString(ctx, "tail")) >
                   0);
        argument = input;
        expected = JS_Call(ctx, method, instance, 1, &argument);
        assert(!JS_IsException(expected));
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        result = JS_Call(ctx, method, instance, 1, &argument);
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
            result = JS_Call(ctx, method, instance, 1, &argument);
        }
        assert(!JS_IsException(result) && !JS_HasException(ctx));
        if (parts) {
            uint32_t length, expected_length, i;
            JSValue value = JS_GetPropertyStr(ctx, result, "length");
            JSValue reference = JS_GetPropertyStr(ctx, expected, "length");
            assert(!JS_ToUint32(ctx, &length, value) &&
                   !JS_ToUint32(ctx, &expected_length, reference));
            JS_FreeValue(ctx, value);
            JS_FreeValue(ctx, reference);
            assert(length == expected_length && length == 17);
            for (i = 0; i < length; i++) {
                const char *keys[] = {"type", "value"};
                JSValue object = JS_GetPropertyUint32(ctx, result, i);
                JSValue other = JS_GetPropertyUint32(ctx, expected, i);
                size_t key;
                for (key = 0; key < 2; key++) {
                    value = JS_GetPropertyStr(ctx, object, keys[key]);
                    reference = JS_GetPropertyStr(ctx, other, keys[key]);
                    assert(JS_StrictEq(ctx, value, reference));
                    JS_FreeValue(ctx, value);
                    JS_FreeValue(ctx, reference);
                }
                JS_FreeValue(ctx, object);
                JS_FreeValue(ctx, other);
            }
        } else {
            assert(JS_StrictEq(ctx, result, expected));
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, expected);
        JS_FreeValue(ctx, input);
        JS_FreeValue(ctx, method);
        JS_FreeValue(ctx, instance);
        JS_FreeValue(ctx, locale);
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
    test_shared_admission_failures();
    test_operation(0);
    test_operation(1);
    test_operation(2);
    test_format(0);
    test_format(1);
#endif
    return 0;
}
