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

/* Collator binding failures, cached functions and intrinsic String cleanup. */
#include "oom-allocator.h"
#ifdef CONFIG_INTL
#include "pool-padding.h"

enum { CONSTRUCT, GET_COMPARE, COMPARE, RESOLVED, SUPPORTED, LOCALE_COMPARE, OPERATION_COUNT };

static JSValue invoke(JSContext *ctx, int operation, JSValueConst constructor,
                      JSValueConst instance, JSValueConst method,
                      JSValueConst *arguments, JSValueConst *values)
{
    if (operation == CONSTRUCT)
        return JS_CallConstructor(ctx, constructor, 2, arguments);
    if (operation == GET_COMPARE)
        return JS_Call(ctx, method, instance, 0, NULL);
    if (operation == COMPARE)
        return JS_Call(ctx, method, JS_UNDEFINED, 2, values);
    if (operation == SUPPORTED)
        return JS_Call(ctx, method, constructor, 1, arguments);
    if (operation == LOCALE_COMPARE) {
        JSValueConst options[] = {values[1], arguments[0], arguments[1]};

        return JS_Call(ctx, method, values[0], 3, options);
    }
    return JS_Call(ctx, method, instance, 0, NULL);
}

static void test_operation(int operation)
{
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *warm = JS_NewContext(rt);
    static const char script[] = "new Intl.Collator('en-u-co-emoji-kf-upper-kn')";
    JSValue warmed;
    size_t step, calls = 0, failures = 0;

    /* Warm only immutable runtime data, then use fresh realm objects for each
       failure point. Deep cold-reader failures have their own bounded tests. */
    assert(warm);
    warmed = JS_Eval(warm, script, sizeof(script) - 1, "<warm-collation>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(warmed));
    JS_FreeValue(warm, warmed);
    for (step = 0; step <= calls; step++) {
        JSContext *ctx = JS_NewContextRaw(rt);
        JSValue global, intl, constructor, prototype, instance, getter, method = JS_UNDEFINED;
        JSValue locale, options, first, last, result;
        JSPropertyDescriptor descriptor;
        JSAtom key;
        void *padding;
        int error;

        assert(ctx && !JS_AddIntrinsicBaseObjects(ctx) && !JS_AddIntrinsicStringNormalize(ctx) &&
               !JS_AddIntrinsicIntl(ctx));
        global = JS_GetGlobalObject(ctx);
        intl = JS_GetPropertyStr(ctx, global, "Intl");
        constructor = JS_GetPropertyStr(ctx, intl, "Collator");
        prototype = JS_GetPropertyStr(ctx, constructor, "prototype");
        key = JS_NewAtom(ctx, "compare");
        assert(key && JS_GetOwnProperty(ctx, &descriptor, prototype, key) == 1);
        JS_FreeAtom(ctx, key);
        getter = descriptor.getter;
        JS_FreeValue(ctx, descriptor.setter);
        JS_FreeValue(ctx, descriptor.value);
        locale = JS_NewString(ctx, "en-u-co-emoji-kf-upper-kn");
        options = JS_NewObject(ctx);
        assert(!JS_IsException(locale) && !JS_IsException(options));
        assert(JS_SetPropertyStr(ctx, options, "collation", JS_NewString(ctx, "eor")) >= 0);
        assert(JS_SetPropertyStr(ctx, options, "numeric", JS_FALSE) >= 0);
        assert(JS_SetPropertyStr(ctx, options, "caseFirst", JS_NewString(ctx, "lower")) >= 0);
        JSValueConst arguments[] = {locale, options};
        instance = JS_CallConstructor(ctx, constructor, 2, arguments);
        assert(!JS_IsException(instance));
        if (operation == GET_COMPARE)
            method = JS_DupValue(ctx, getter);
        else if (operation == COMPARE)
            method = JS_GetPropertyStr(ctx, instance, "compare");
        else if (operation == RESOLVED)
            method = JS_GetPropertyStr(ctx, instance, "resolvedOptions");
        else if (operation == SUPPORTED)
            method = JS_GetPropertyStr(ctx, constructor, "supportedLocalesOf");
        else if (operation == LOCALE_COMPARE) {
            JSValue string = JS_GetPropertyStr(ctx, global, "String");
            JSValue string_prototype = JS_GetPropertyStr(ctx, string, "prototype");

            method = JS_GetPropertyStr(ctx, string_prototype, "localeCompare");
            JS_FreeValue(ctx, string_prototype);
            JS_FreeValue(ctx, string);
        }
        assert(!JS_IsException(method));
        first = JS_NewString(ctx, "é");
        last = JS_NewString(ctx, "e\xcc\x81");
        assert(!JS_IsException(first) && !JS_IsException(last));
        JSValueConst values[] = {first, last};
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        result = invoke(ctx, operation, constructor, instance, method, arguments, values);
        oom_test_end(&allocator);
        test_pool_padding_free(rt, padding);
        error = JS_IsException(result);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 256 && !error);
        } else if (!allocator.failures) {
            assert(allocator.calls < step && !error);
        } else {
            assert(allocator.failures == 1);
        }
        if (error) {
            failures++;
            oom_test_check_exception(ctx);
            result = invoke(ctx, operation, constructor, instance, method, arguments, values);
        }
        assert(!JS_IsException(result) && !JS_HasException(ctx));
        if (operation == GET_COMPARE) {
            JSValue again = JS_Call(ctx, getter, instance, 0, NULL);

            assert(JS_StrictEq(ctx, result, again));
            JS_FreeValue(ctx, again);
        } else if (operation == COMPARE || operation == LOCALE_COMPARE) {
            assert(JS_StrictEq(ctx, result, JS_NewInt32(ctx, 0)));
        } else if (operation == SUPPORTED) {
            assert(JS_IsArray(ctx, result));
        } else if (operation == CONSTRUCT) {
            JSValue function = JS_Call(ctx, getter, result, 0, NULL);
            JSValue order = JS_Call(ctx, function, JS_UNDEFINED, 2, values);

            assert(!JS_IsException(function) && JS_StrictEq(ctx, order, JS_NewInt32(ctx, 0)));
            JS_FreeValue(ctx, order);
            JS_FreeValue(ctx, function);
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, first);
        JS_FreeValue(ctx, last);
        JS_FreeValue(ctx, method);
        JS_FreeValue(ctx, getter);
        JS_FreeValue(ctx, instance);
        JS_FreeValue(ctx, locale);
        JS_FreeValue(ctx, options);
        JS_FreeValue(ctx, prototype);
        JS_FreeValue(ctx, constructor);
        JS_FreeValue(ctx, intl);
        JS_FreeValue(ctx, global);
        JS_RunGC(rt);
        JS_FreeContext(ctx);
        JS_RunGC(rt);
    }
    JS_FreeContext(warm);
    JS_FreeRuntime(rt);
    assert(!allocator.live && failures);
}
#endif

int main(void)
{
#ifdef CONFIG_INTL
    for (int operation = 0; operation < OPERATION_COUNT; operation++)
        test_operation(operation);
#endif
    return 0;
}
