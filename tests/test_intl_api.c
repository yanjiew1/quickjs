/* Intl initialization, disabled API and real allocator-failure contracts.
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
static JSValue call_operation(JSContext *ctx, JSValueConst function,
                               JSValueConst argument, int operation)
{
    return operation == 3 ? JS_CallConstructor(ctx, function, 1, &argument) :
                            JS_Call(ctx, function, JS_UNDEFINED, 1, &argument);
}

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
            function = JS_GetPropertyStr(ctx, object,
                                          operation == 3 ? "Locale" : "getCanonicalLocales");
            argument = JS_NewString(ctx, operation == 3 ?
                "iw-Qaai-BU-u-ca-islamicc-co-foobar-fw-sun-hc-h23-kf-upper-kn-nu-arab" :
                "iw-Qaai-BU");
            assert(!JS_IsException(argument) && JS_IsFunction(ctx, function));
            if (operation >= 2) {
                result = call_operation(ctx, function, argument, operation);
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
            result = call_operation(ctx, function, argument, operation);
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
                result = call_operation(ctx, function, argument, operation);
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
        if (operation == 3) {
            const char *text = JS_ToCString(ctx, result);
            assert(text && !strcmp(text,
                "he-Zinh-MM-u-ca-islamic-civil-co-foobar-fw-sun-hc-h23-kf-upper-kn-nu-arab"));
            JS_FreeCString(ctx, text);
        } else if (operation) {
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
static void test_preference_failures(const char *method, const char *tag, int kind)
{
    size_t step, calls = 0, failed = 0;
    OOMTestAllocator allocator;
    /* Reuse the large immutable DateTimeFormat reader across calendar-binding
       samples. Fresh contexts still isolate each injected failure. */
    JSRuntime *shared_rt = kind == 3 ? oom_test_runtime(&allocator) : NULL;

    for (step = 0; step <= calls; step++) {
        JSRuntime *rt = shared_rt ? shared_rt : oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContextRaw(rt);
        JSValue global, object, constructor, argument, locale, function, result;
        JSValueConst borrowed;
        void *padding;

        assert(ctx && JS_AddIntrinsicBaseObjects(ctx) == 0);
        assert(JS_AddIntrinsicIntl(ctx) == 0);
        global = JS_GetGlobalObject(ctx);
        object = JS_GetPropertyStr(ctx, global, "Intl");
        constructor = JS_GetPropertyStr(ctx, object, "Locale");
        argument = JS_NewString(ctx, tag);
        borrowed = argument;
        locale = JS_CallConstructor(ctx, constructor, 1, &borrowed);
        assert(!JS_IsException(locale));
        function = JS_GetPropertyStr(ctx, locale, method);
        assert(JS_IsFunction(ctx, function));
        if (kind == 3) {
            /* Exhaust the small binding path after DateTimeFormat's immutable
               source owner is open; its cold reader has separate OOM tests. */
            result = JS_Call(ctx, function, locale, 0, NULL);
            assert(!JS_IsException(result));
            JS_FreeValue(ctx, result);
        }
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        result = JS_Call(ctx, function, locale, 0, NULL);
        oom_test_end(&allocator);
        test_pool_padding_free(rt, padding);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 1024);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_IsException(result)) {
            failed++;
            oom_test_check_exception(ctx);
            result = JS_Call(ctx, function, locale, 0, NULL);
            assert(!JS_IsException(result));
        }
        assert(!JS_HasException(ctx));
        if (kind == 0) {
            JSValue first = JS_GetPropertyStr(ctx, result, "firstDay");
            JSValue weekend = JS_GetPropertyStr(ctx, result, "weekend");
            int32_t number;
            assert(!JS_ToInt32(ctx, &number, first) && number == 6);
            assert(JS_IsArray(ctx, weekend) == 1);
            JS_FreeValue(ctx, first);
            JS_FreeValue(ctx, weekend);
        } else {
            JSValue value = kind == 1 ? JS_GetPropertyStr(ctx, result, "direction")
                                     : JS_GetPropertyUint32(ctx, result, 0);
            const char *text = JS_ToCString(ctx, value);
            assert(text && !strcmp(text, kind == 1 ? "rtl" : kind == 3 ? "gregory" :
                                         kind == 4 ? "Asia/Singapore" : "h23"));
            JS_FreeCString(ctx, text);
            JS_FreeValue(ctx, value);
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, function);
        JS_FreeValue(ctx, locale);
        JS_FreeValue(ctx, argument);
        JS_FreeValue(ctx, constructor);
        JS_FreeValue(ctx, object);
        JS_FreeValue(ctx, global);
        JS_FreeContext(ctx);
        if (!shared_rt) {
            JS_FreeRuntime(rt);
            assert(!allocator.live);
        }
    }
    if (shared_rt) {
        JS_FreeRuntime(shared_rt);
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
    test_failures(3);
    test_preference_failures("getWeekInfo", "fa-u-sd-inka-rg-afzzzz", 0);
    test_preference_failures("getTextInfo", "ar", 1);
    test_preference_failures("getHourCycles", "en-u-rg-jpzzzz", 2);
    test_preference_failures("getCalendars", "en", 3);
#ifdef CONFIG_TIMEZONE_NATIVE
    test_preference_failures("getTimeZones", "en-SG", 4);
#endif
#endif
    return 0;
}
