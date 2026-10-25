/* Exercise staged NumberFormat constructor option operations from JavaScript.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "../include/quickjs-libc.h"
#include "../src/quickjs/builtins/intl/number-format-options.h"
#include "../src/quickjs/builtins/intl.h"
#include "oom-allocator.h"
#include "pool-padding.h"

static QJSIntlNumberService *fixture_service;

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

static JSValue read_full(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSIntlNumberFormatOptions out;
    JSValue result;
    (void)this_val;
    if (!argc || !JS_IsObject(argv[0]))
        return JS_ThrowTypeError(ctx, "test operation requires an options object");
    memset(&out, 0xff, sizeof(out));
    if (js_intl_number_format_options(ctx, argv[0], fixture_service, &out)) {
        const unsigned char *bytes = (const unsigned char *)&out;
        for (size_t i = 0; i < sizeof(out); i++)
            assert(!bytes[i]);
        return JS_EXCEPTION;
    }
    const struct {
        const char *name;
        int value;
    } fields[] = {
        {"style", out.unit.style},
        {"notation", out.notation},
        {"compactDisplay", out.compact_long},
        {"useGrouping", out.grouping},
        {"signDisplay", out.sign_display},
        {"minimumIntegerDigits", out.digits.minimum_integer_digits},
        {"minimumFractionDigits", out.digits.minimum_fraction_digits},
        {"maximumFractionDigits", out.digits.maximum_fraction_digits},
        {"minimumSignificantDigits", out.digits.minimum_significant_digits},
        {"maximumSignificantDigits", out.digits.maximum_significant_digits},
        {"roundingIncrement", out.digits.rounding_increment},
        {"roundingMode", out.digits.rounding_mode},
        {"roundingType", out.digits.rounding_type},
        {"trailingZeroDisplay", out.digits.trailing_zero_display},
        {"roundingPriority", out.digits.computed_rounding_priority},
    };
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        goto done;
    for (size_t i = 0; i < countof(fields); i++) {
        if (fields[i].value >= 0 &&
            property(ctx, result, fields[i].name, JS_NewInt32(ctx, fields[i].value)) < 0) {
            JS_FreeValue(ctx, result);
            result = JS_EXCEPTION;
            break;
        }
    }
done:
    js_free(ctx, out.unit.unit);
    return result;
}

static JSValue option_string(JSContext *ctx, JSValueConst this_val, int argc,
                              JSValueConst *argv, int magic)
{
    static const char *const strings[] = {"uSd", "standard", "always"};
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewString(ctx, strings[magic]);
}

static void test_allocation_failures(int unit_style, int full)
{
    size_t step, calls = 0, failures = 0;
    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContextRaw(rt);
        JSIntlNumberUnitOptions out;
        JSIntlNumberFormatOptions complete;
        QJSIntlNumberService *service = NULL;
        JSValue options, currency;
        void *padding;
        int error;
        assert(ctx && !JS_AddIntrinsicBaseObjects(ctx));
        if (full) {
            QJSIntlAllocator native_allocator = js_intl_allocator(rt);
            assert(!qjs_intl_number_open(&native_allocator, NULL, &service));
        }
        options = JS_NewObject(ctx);
        assert(!JS_IsException(options));
        assert(property(ctx, options, "style", JS_NewString(ctx, unit_style ? "unit" : "currency")) >= 0);
        currency = JS_NewObject(ctx);
        assert(!JS_IsException(currency));
        assert(property(ctx, currency, "toString",
                         JS_NewCFunctionMagic(ctx, option_string, "toString", 0,
                                               JS_CFUNC_generic_magic, 0)) >= 0);
        assert(JS_SetPropertyStr(ctx, options, "currency", currency) >= 0);
        assert(property(ctx, options, "unit", JS_NewString(ctx, "kilometer-per-hour")) >= 0);
        if (full) {
            const char *names[] = {"notation", "signDisplay"};
            for (int i = 0; i < 2; i++) {
                JSValue value = JS_NewObject(ctx);
                assert(!JS_IsException(value));
                assert(property(ctx, value, "toString",
                                 JS_NewCFunctionMagic(ctx, option_string, "toString", 0,
                                                       JS_CFUNC_generic_magic, i + 1)) >= 0);
                assert(JS_SetPropertyStr(ctx, options, names[i], value) >= 0);
            }
        }
        padding = test_pool_padding(rt);
        memset(&out, 0xff, sizeof(out));
        oom_test_begin(&allocator, step);
        memset(&complete, 0xff, sizeof(complete));
        error = full ? js_intl_number_format_options(ctx, options, service, &complete)
                     : js_intl_number_unit_options(ctx, options, &out);
        oom_test_end(&allocator);
        test_pool_padding_free(rt, padding);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 64 && !error);
        } else {
            assert(allocator.failures == 1);
        }
        if (error) {
            const unsigned char *bytes = full ? (const unsigned char *)&complete
                                               : (const unsigned char *)&out;
            failures++;
            for (size_t i = 0; i < (full ? sizeof(complete) : sizeof(out)); i++)
                assert(!bytes[i]);
            oom_test_check_exception(ctx);
            if (full)
                assert(!js_intl_number_format_options(ctx, options, service, &complete));
            else
                assert(!js_intl_number_unit_options(ctx, options, &out));
        }
        if (full) {
            out = complete.unit;
            assert(complete.notation == QJS_INTL_NUMBER_STANDARD &&
                   complete.sign_display == QJS_INTL_NUMBER_SIGN_ALWAYS);
        }
        assert(!JS_HasException(ctx));
        if (unit_style)
            assert(out.unit && !strcmp(out.unit, "kilometer-per-hour") && !out.currency[0]);
        else
            assert(!out.unit && !strcmp(out.currency, "USD"));
        js_free(ctx, out.unit);
        JS_FreeValue(ctx, options);
        qjs_intl_number_close(service);
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
    {
        QJSIntlAllocator allocator = js_intl_allocator(rt);
        assert(!qjs_intl_number_open(&allocator, NULL, &fixture_service));
    }
    global = JS_GetGlobalObject(ctx);
    assert(property(ctx, global, "readNumberUnitOptions",
                     JS_NewCFunction(ctx, read_units, "readNumberUnitOptions", 1)) >= 0);
    assert(property(ctx, global, "readNumberFormatOptions",
                     JS_NewCFunction(ctx, read_full, "readNumberFormatOptions", 1)) >= 0);
    JS_FreeValue(ctx, global);
    script = js_load_file(ctx, &length, argv[1]);
    assert(script);
    result = JS_Eval(ctx, (const char *)script, length, argv[1], JS_EVAL_TYPE_GLOBAL);
    js_free(ctx, script);
    if (JS_IsException(result))
        js_std_dump_error(ctx);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, result);
    qjs_intl_number_close(fixture_service);
    fixture_service = NULL;
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    test_allocation_failures(0, 0);
    test_allocation_failures(1, 0);
    test_allocation_failures(0, 1);
    test_allocation_failures(1, 1);
    return 0;
}
