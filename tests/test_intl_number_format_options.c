/* Exercise staged NumberFormat constructor option operations from JavaScript.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../include/quickjs-libc.h"
#include "../src/quickjs/builtins/intl/number-format-options.h"
#include "oom-allocator.h"
#include "pool-padding.h"

static int property(JSContext *ctx, JSValueConst object, const char *name, JSValue value)
{
    if (JS_IsException(value))
        return -1;
    return JS_SetPropertyStr(ctx, object, name, value);
}

static JSValue read_units(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    static const char *const styles[] = {"decimal", "percent", "currency", "unit"};
    static const char *const currency_displays[] = {"symbol", "narrowSymbol", "code", "name"};
    static const char *const currency_signs[] = {"standard", "accounting"};
    static const char *const unit_displays[] = {"short", "narrow", "long"};
    JSIntlNumberUnitOptions out;
    JSValue result;
    (void)this_val;
    if (!argc || !JS_IsObject(argv[0]))
        return JS_ThrowTypeError(ctx, "test operation requires an options object");
    memset(&out, 0xff, sizeof(out));
    if (js_intl_number_unit_options(ctx, argv[0], &out)) {
        const unsigned char *bytes = (const unsigned char *)&out;
        for (size_t i = 0; i < sizeof(out); i++)
            assert(!bytes[i]);
        return JS_EXCEPTION;
    }
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        goto done;
    if (property(ctx, result, "style", JS_NewString(ctx, styles[out.style])) < 0 ||
        (out.style == QJS_INTL_NUMBER_CURRENCY &&
         (property(ctx, result, "currency", JS_NewString(ctx, out.currency)) < 0 ||
          property(ctx, result, "currencyDisplay",
                    JS_NewString(ctx, currency_displays[out.currency_display])) < 0 ||
          property(ctx, result, "currencySign",
                    JS_NewString(ctx, currency_signs[out.currency_sign])) < 0)) ||
        (out.style == QJS_INTL_NUMBER_UNIT &&
         (property(ctx, result, "unit", JS_NewString(ctx, out.unit)) < 0 ||
          property(ctx, result, "unitDisplay",
                    JS_NewString(ctx, unit_displays[out.unit_display])) < 0))) {
        JS_FreeValue(ctx, result);
        result = JS_EXCEPTION;
    }
done:
    js_free(ctx, out.unit);
    return result;
}

static JSValue currency_string(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewString(ctx, "uSd");
}

static void test_allocation_failures(int unit_style)
{
    size_t step, calls = 0, failures = 0;
    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContextRaw(rt);
        JSIntlNumberUnitOptions out;
        JSValue options, currency;
        void *padding;
        int error;
        assert(ctx && !JS_AddIntrinsicBaseObjects(ctx));
        options = JS_NewObject(ctx);
        assert(!JS_IsException(options));
        assert(property(ctx, options, "style", JS_NewString(ctx, unit_style ? "unit" : "currency")) >= 0);
        currency = JS_NewObject(ctx);
        assert(!JS_IsException(currency));
        assert(property(ctx, currency, "toString",
                         JS_NewCFunction(ctx, currency_string, "toString", 0)) >= 0);
        assert(JS_SetPropertyStr(ctx, options, "currency", currency) >= 0);
        assert(property(ctx, options, "unit", JS_NewString(ctx, "kilometer-per-hour")) >= 0);
        padding = test_pool_padding(rt);
        memset(&out, 0xff, sizeof(out));
        oom_test_begin(&allocator, step);
        error = js_intl_number_unit_options(ctx, options, &out);
        oom_test_end(&allocator);
        test_pool_padding_free(rt, padding);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 64 && !error);
        } else {
            assert(allocator.failures == 1);
        }
        if (error) {
            const unsigned char *bytes = (const unsigned char *)&out;
            failures++;
            for (size_t i = 0; i < sizeof(out); i++)
                assert(!bytes[i]);
            oom_test_check_exception(ctx);
            assert(!js_intl_number_unit_options(ctx, options, &out));
        }
        assert(!JS_HasException(ctx));
        if (unit_style)
            assert(out.unit && !strcmp(out.unit, "kilometer-per-hour") && !out.currency[0]);
        else
            assert(!out.unit && !strcmp(out.currency, "USD"));
        js_free(ctx, out.unit);
        JS_FreeValue(ctx, options);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
    assert(failures);
}

int main(int argc, char **argv)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue global, result;
    size_t length;
    uint8_t *script;
    assert(argc == 2 && rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    global = JS_GetGlobalObject(ctx);
    assert(property(ctx, global, "readNumberUnitOptions",
                     JS_NewCFunction(ctx, read_units, "readNumberUnitOptions", 1)) >= 0);
    JS_FreeValue(ctx, global);
    script = js_load_file(ctx, &length, argv[1]);
    assert(script);
    result = JS_Eval(ctx, (const char *)script, length, argv[1], JS_EVAL_TYPE_GLOBAL);
    js_free(ctx, script);
    if (JS_IsException(result))
        js_std_dump_error(ctx);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, result);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    test_allocation_failures(0);
    test_allocation_failures(1);
    return 0;
}
