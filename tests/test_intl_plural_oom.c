/*
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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

/* Plural binding allocation failures, retry and complete runtime teardown. */
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
        JSValue global, intl, constructor, locale, instance, method, result;
        JSValue arguments[2];
        void *padding;
        int error, argc = operation == 3 ? 2 : operation == 1 ? 0 : 1;
        const char *name = operation == 1   ? "resolvedOptions"
                           : operation == 2 ? "select"
                           : operation == 3 ? "selectRange"
                                            : "supportedLocalesOf";
        assert(ctx && !JS_AddIntrinsicBaseObjects(ctx) && !JS_AddIntrinsicIntl(ctx));
        global = JS_GetGlobalObject(ctx);
        intl = JS_GetPropertyStr(ctx, global, "Intl");
        constructor = JS_GetPropertyStr(ctx, intl, "PluralRules");
        locale = JS_NewString(ctx, "en");
        instance = JS_CallConstructor(ctx, constructor, 1, &locale);
        assert(!JS_IsException(instance));
        method = JS_GetPropertyStr(ctx, operation == 4 ? constructor : instance,
                                   operation == 0 ? "resolvedOptions" : name);
        assert(JS_IsFunction(ctx, method));
        arguments[0] = operation == 0 || operation == 4 ? JS_DupValue(ctx, locale)
                                                        : JS_NewString(ctx, "1.125");
        arguments[1] = JS_NewString(ctx, "2.250");
        assert(!JS_IsException(arguments[0]) && !JS_IsException(arguments[1]));
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        if (operation == 0)
            result = JS_CallConstructor(ctx, constructor, argc, arguments);
        else
            result = JS_Call(ctx, method, operation == 4 ? constructor : instance, argc, arguments);
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
                result = JS_CallConstructor(ctx, constructor, argc, arguments);
            else
                result =
                    JS_Call(ctx, method, operation == 4 ? constructor : instance, argc, arguments);
        }
        assert(!JS_IsException(result) && !JS_HasException(ctx));
        if (operation == 0) {
            JSValue resolved = JS_Call(ctx, method, result, 0, NULL);
            JS_FreeValue(ctx, result);
            result = resolved;
        }
        if (operation == 0 || operation == 1) {
            JSValue tag = JS_GetPropertyStr(ctx, result, "locale");
            const char *text = JS_ToCString(ctx, tag);
            assert(text && !strcmp(text, "en"));
            JS_FreeCString(ctx, text);
            JS_FreeValue(ctx, tag);
        } else if (operation == 2 || operation == 3) {
            const char *text = JS_ToCString(ctx, result);
            assert(text && !strcmp(text, "other"));
            JS_FreeCString(ctx, text);
        } else {
            assert(JS_IsArray(ctx, result));
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, arguments[0]);
        JS_FreeValue(ctx, arguments[1]);
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
    int i;
    for (i = 0; i < 5; i++)
        test_operation(i);
#endif
    return 0;
}
