/* DurationFormat binding allocation failures and realm teardown. */
#include "oom-allocator.h"
#ifdef CONFIG_INTL
#include "pool-padding.h"

static JSValue invoke(JSContext *ctx, int operation, JSValueConst constructor,
                        JSValueConst method, JSValueConst instance,
                        JSValueConst *arguments, JSValueConst *values)
{
    if (!operation)
        return JS_CallConstructor(ctx, constructor, 2, arguments);
    if (operation == 4)
        return JS_Call(ctx, method, constructor, 1, arguments);
    return JS_Call(ctx, method, instance, operation == 3 ? 0 : 1, values);
}

static void test_operation(int operation)
{
    static const char *const names[] = {NULL, "format", "formatToParts", "resolvedOptions",
                                          "supportedLocalesOf"};
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *warm = JS_NewContext(rt);
    JSValue warmed;
    size_t step, calls = 0, failures = 0;
    const char warm_script[] = "new Intl.DurationFormat('en-u-nu-latn')";
    assert(warm);
    /* Cache only immutable runtime services. Every operation uses fresh realm
       objects, without reopening all CLDR data at each allocation point. */
    warmed = JS_Eval(warm, warm_script, sizeof(warm_script) - 1, "<warm-duration>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(warmed));
    JS_FreeValue(warm, warmed);
    for (step = 0; step <= calls; step++) {
        JSContext *ctx = JS_NewContextRaw(rt);
        JSValue global, intl, constructor, instance, method, locale, options, unit, number, result;
        void *padding;
        int error;
        assert(ctx && !JS_AddIntrinsicBaseObjects(ctx) && !JS_AddIntrinsicIntl(ctx));
        global = JS_GetGlobalObject(ctx);
        intl = JS_GetPropertyStr(ctx, global, "Intl");
        constructor = JS_GetPropertyStr(ctx, intl, "DurationFormat");
        locale = JS_NewString(ctx, "en-u-nu-latn");
        options = JS_NewObject(ctx);
        assert(!JS_IsException(options));
        assert(JS_SetPropertyStr(ctx, options, "style", JS_NewString(ctx, "digital")) >= 0);
        JSValueConst arguments[] = {locale, options};
        instance = JS_CallConstructor(ctx, constructor, 2, arguments);
        assert(!JS_IsException(instance));
        method = names[operation] ? JS_GetPropertyStr(ctx, operation == 4 ? constructor : instance, names[operation])
                                  : JS_UNDEFINED;
        assert(!JS_IsException(method));
        unit = JS_UNDEFINED;
        number = JS_NewObject(ctx);
        assert(JS_SetPropertyStr(ctx, number, "seconds", JS_NewInt32(ctx, 1)) >= 0);
        assert(JS_SetPropertyStr(ctx, number, "nanoseconds", JS_NewInt32(ctx, 1)) >= 0);
        assert(!JS_IsException(unit) && !JS_IsException(number));
        JSValueConst values[] = {number, unit};
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        result = invoke(ctx, operation, constructor, method, instance, arguments, values);
        oom_test_end(&allocator);
        test_pool_padding_free(rt, padding);
        error = JS_IsException(result);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 2048 && !error);
        } else if (!allocator.failures) {
            assert(allocator.calls < step && !error);
        } else {
            assert(allocator.failures == 1);
        }
        if (error) {
            failures++;
            oom_test_check_exception(ctx);
            result = invoke(ctx, operation, constructor, method, instance, arguments, values);
        }
        assert(!JS_IsException(result) && !JS_HasException(ctx));
        if (operation == 2 || operation == 4)
            assert(JS_IsArray(ctx, result));
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, number);
        JS_FreeValue(ctx, unit);
        JS_FreeValue(ctx, method);
        JS_FreeValue(ctx, instance);
        JS_FreeValue(ctx, options);
        JS_FreeValue(ctx, locale);
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
    for (int operation = 0; operation < 5; operation++)
        test_operation(operation);
#endif
    return 0;
}
