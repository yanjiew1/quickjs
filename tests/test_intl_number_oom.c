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

/* NumberFormat binding allocation failures, cached callbacks and GC teardown. */
#include "oom-allocator.h"
#ifdef CONFIG_INTL
#include "pool-padding.h"

static JSValue invoke(JSContext *ctx, int operation, JSValueConst constructor,
                        JSValueConst method, JSValueConst instance, JSValueConst receiver,
                        JSValueConst *constructor_args, JSValueConst *values)
{
    if (!operation)
        return JS_CallConstructor(ctx, constructor, 2, constructor_args);
    if (operation == 7)
        return JS_Call(ctx, constructor, receiver, 2, constructor_args);
    if (operation == 8)
        return JS_Call(ctx, method, constructor, 1, constructor_args);
    return JS_Call(ctx, method, instance, operation == 1 || operation == 6 ? 0
                                          : operation == 4 || operation == 5 ? 2 : 1, values);
}

static void test_operation(int operation)
{
    static const char *const names[] = {NULL, NULL, NULL, "formatToParts", "formatRange",
                                          "formatRangeToParts", "resolvedOptions", NULL,
                                          "supportedLocalesOf"};
    size_t step, calls = 0, failures = 0;
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *warm = JS_NewContext(rt);
    JSValue warmed;
    static const char warm_script[] = "new Intl.NumberFormat('en-u-nu-latn')";
    /* Reuse only runtime-owned immutable services. Each injected operation has
       fresh realm objects; reopening/validating full CLDR for every allocation
       point is covered by service tests and would dominate this binding test. */
    assert(warm);
    warmed = JS_Eval(warm, warm_script, sizeof(warm_script) - 1,
                     "<warm-number-service>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(warmed));
    JS_FreeValue(warm, warmed);
    for (step = 0; step <= calls; step++) {
        JSContext *ctx = JS_NewContextRaw(rt);
        JSValue global, intl, constructor, prototype, instance, method, getter, receiver;
        JSValue locale, options, first, last, result;
        JSPropertyDescriptor descriptor;
        JSAtom key;
        void *padding;
        int error;
        assert(ctx && !JS_AddIntrinsicBaseObjects(ctx) && !JS_AddIntrinsicIntl(ctx));
        global = JS_GetGlobalObject(ctx);
        intl = JS_GetPropertyStr(ctx, global, "Intl");
        constructor = JS_GetPropertyStr(ctx, intl, "NumberFormat");
        prototype = JS_GetPropertyStr(ctx, constructor, "prototype");
        key = JS_NewAtom(ctx, "format");
        assert(key && JS_GetOwnProperty(ctx, &descriptor, prototype, key) == 1);
        JS_FreeAtom(ctx, key);
        getter = descriptor.getter;
        JS_FreeValue(ctx, descriptor.setter);
        JS_FreeValue(ctx, descriptor.value);
        locale = JS_NewString(ctx, "en-u-nu-latn");
        options = JS_NewObject(ctx);
        assert(!JS_IsException(options));
        assert(JS_SetPropertyStr(ctx, options, "style", JS_NewString(ctx, "unit")) >= 0);
        assert(JS_SetPropertyStr(ctx, options, "unit", JS_NewString(ctx, "meter-per-second")) >= 0);
        JSValueConst arguments[] = {locale, options};
        instance = JS_CallConstructor(ctx, constructor, 2, arguments);
        assert(!JS_IsException(instance)); /* Warm immutable data before injection. */
        receiver = JS_NewObjectProto(ctx, prototype);
        assert(!JS_IsException(receiver));
        if (operation == 1)
            method = JS_DupValue(ctx, getter);
        else if (operation == 2)
            method = JS_GetPropertyStr(ctx, instance, "format");
        else if (names[operation])
            method = JS_GetPropertyStr(ctx, operation == 8 ? constructor : instance, names[operation]);
        else
            method = JS_UNDEFINED;
        assert(!JS_IsException(method));
        first = JS_NewString(ctx, "1.125");
        last = JS_NewString(ctx, "2.250");
        assert(!JS_IsException(first) && !JS_IsException(last));
        JSValueConst values[] = {first, last};
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        result = invoke(ctx, operation, constructor, method, instance, receiver, arguments, values);
        oom_test_end(&allocator);
        test_pool_padding_free(rt, padding);
        error = JS_IsException(result);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 512 && !error);
        } else if (!allocator.failures) {
            /* Runtime caches can shorten later physical-allocation paths.
               Pool padding still ensures every reached index is injected. */
            assert(allocator.calls < step && !error);
        } else {
            assert(allocator.failures == 1);
        }
        if (error) {
            failures++;
            oom_test_check_exception(ctx);
            result = invoke(ctx, operation, constructor, method, instance, receiver, arguments, values);
        }
        assert(!JS_IsException(result) && !JS_HasException(ctx));
        if (operation == 1) {
            JSValue again = JS_Call(ctx, getter, instance, 0, NULL);
            assert(JS_VALUE_GET_PTR(result) == JS_VALUE_GET_PTR(again));
            JS_FreeValue(ctx, again);
        } else if (!operation || operation == 7) {
            JSValue format = JS_Call(ctx, getter, result, 0, NULL);
            JSValue text = JS_Call(ctx, format, JS_UNDEFINED, 1, values);
            assert(!JS_IsException(format) && !JS_IsException(text));
            JS_FreeValue(ctx, text);
            JS_FreeValue(ctx, format);
        } else if (operation == 3 || operation == 5 || operation == 8) {
            assert(JS_IsArray(ctx, result));
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, first);
        JS_FreeValue(ctx, last);
        JS_FreeValue(ctx, receiver);
        JS_FreeValue(ctx, method);
        JS_FreeValue(ctx, getter);
        JS_FreeValue(ctx, instance);
        JS_FreeValue(ctx, locale);
        JS_FreeValue(ctx, options);
        JS_FreeValue(ctx, prototype);
        JS_FreeValue(ctx, constructor);
        JS_FreeValue(ctx, intl);
        JS_FreeValue(ctx, global);
        JS_RunGC(rt); /* Includes unrooted formatter/cached-function cycles. */
        JS_FreeContext(ctx);
        JS_RunGC(rt);
    }
    JS_FreeContext(warm);
    JS_FreeRuntime(rt);
    assert(!allocator.live);
    assert(failures);
}
#endif

int main(void)
{
#ifdef CONFIG_INTL
    for (int operation = 0; operation < 9; operation++)
        test_operation(operation);
#endif
    return 0;
}
