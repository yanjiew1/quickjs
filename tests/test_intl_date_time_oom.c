/* DateTimeFormat binding allocation failures, cached callbacks and GC teardown. */
#include "oom-allocator.h"
#ifdef CONFIG_INTL
#include "pool-padding.h"

static JSValue invoke(JSContext *ctx, int operation, JSValueConst constructor,
                        JSValueConst method, JSValueConst instance, JSValueConst receiver,
                        JSValueConst *constructor_args, JSValueConst *values)
{
    if (!operation)
        return JS_CallConstructor(ctx, constructor, 2, constructor_args);
    if (operation == 5)
        return JS_Call(ctx, constructor, receiver, 2, constructor_args);
    if (operation == 6)
        return JS_Call(ctx, method, constructor, 1, constructor_args);
    return JS_Call(ctx, method, instance, operation == 1 || operation == 4 ? 0 : 1, values);
}

static void test_operation(int operation, OOMTestAllocator *allocator, JSRuntime *rt)
{
    static const char *const names[] = {NULL, NULL, NULL, "formatToParts", "resolvedOptions",
                                          NULL, "supportedLocalesOf"};
    size_t step, calls = 0, failures = 0;
    for (step = 0; step <= calls; step++) {
        JSContext *ctx = JS_NewContextRaw(rt);
        JSValue global, intl, constructor, prototype, instance, method, getter, receiver;
        JSValue locale, options, first, result;
        JSPropertyDescriptor descriptor;
        JSAtom key;
        void *padding;
        int error;
        assert(ctx && !JS_AddIntrinsicBaseObjects(ctx) && !JS_AddIntrinsicIntl(ctx));
        global = JS_GetGlobalObject(ctx);
        intl = JS_GetPropertyStr(ctx, global, "Intl");
        constructor = JS_GetPropertyStr(ctx, intl, "DateTimeFormat");
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
        assert(JS_SetPropertyStr(ctx, options, "dateStyle", JS_NewString(ctx, "full")) >= 0);
        assert(JS_SetPropertyStr(ctx, options, "timeZone", JS_NewString(ctx, "UTC")) >= 0);
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
            method = JS_GetPropertyStr(ctx, operation == 6 ? constructor : instance, names[operation]);
        else
            method = JS_UNDEFINED;
        assert(!JS_IsException(method));
        first = JS_NewString(ctx, "1719835200123");
        assert(!JS_IsException(first));
        JSValueConst values[] = {first};
        padding = test_pool_padding(rt);
        oom_test_begin(allocator, step);
        result = invoke(ctx, operation, constructor, method, instance, receiver, arguments, values);
        oom_test_end(allocator);
        test_pool_padding_free(rt, padding);
        error = JS_IsException(result);
        if (!step) {
            calls = allocator->calls;
            assert(calls && calls < 4096 && !error);
        } else if (!allocator->failures) {
            /* Runtime caches can shorten later physical-allocation paths.
               Pool padding still ensures every reached index is injected. */
            assert(allocator->calls < step && !error);
        } else {
            assert(allocator->failures == 1);
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
        } else if (!operation || operation == 5) {
            JSValue format = JS_Call(ctx, getter, result, 0, NULL);
            JSValue text = JS_Call(ctx, format, JS_UNDEFINED, 1, values);
            assert(!JS_IsException(format) && !JS_IsException(text));
            JS_FreeValue(ctx, text);
            JS_FreeValue(ctx, format);
        } else if (operation == 3 || operation == 6) {
            assert(JS_IsArray(ctx, result));
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, first);
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
    assert(failures);
}
#endif

int main(void)
{
#ifdef CONFIG_INTL
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *warm = JS_NewContext(rt);
    const char script[] = "new Intl.DateTimeFormat('en-u-nu-latn', {timeZone: 'UTC'})";
    JSValue formatter;
    assert(warm);
    /* Share immutable runtime services across operations. Every failure point
       still gets fresh realm objects and pool padding; the C service suite
       independently sweeps formatter construction without JS realm overhead. */
    formatter = JS_Eval(warm, script, sizeof(script) - 1, "<warm-date-time-service>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(formatter));
    JS_FreeValue(warm, formatter);
    for (int operation = 0; operation < 7; operation++)
        test_operation(operation, &allocator, rt);
    JS_FreeContext(warm);
    JS_FreeRuntime(rt);
    assert(!allocator.live);
#endif
    return 0;
}
