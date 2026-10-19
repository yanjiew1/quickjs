/* Exercise the internal numeric option operation with JS getters/coercions.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../include/quickjs-libc.h"
#include "../src/quickjs/builtins/intl/number-options.h"
#include <assert.h>
#include <string.h>

static JSValue read_options(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSIntlDigitOptions digits;
    int minimum, maximum;
    JSValue result;
    size_t i;
    (void)this_val;
    if (argc != 4 || !JS_IsObject(argv[0]))
        return JS_ThrowTypeError(ctx, "test operation requires options and defaults");
    if (JS_ToInt32(ctx, &minimum, argv[1]) || JS_ToInt32(ctx, &maximum, argv[2]))
        return JS_EXCEPTION;
    if (minimum < 0 || minimum > maximum || maximum > 100)
        return JS_ThrowRangeError(ctx, "invalid test defaults");
    memset(&digits, 0xff, sizeof(digits));
    if (js_intl_set_digit_options(ctx, argv[0], minimum, maximum, JS_ToBool(ctx, argv[3]),
                                  &digits)) {
        const unsigned char *bytes = (const unsigned char *)&digits;
        for (i = 0; i < sizeof(digits); i++)
            assert(!bytes[i]);
        return JS_EXCEPTION;
    }
    const struct {
        const char *name;
        int value;
    } properties[] = {
        {"minimumIntegerDigits", digits.minimum_integer_digits},
        {"minimumFractionDigits", digits.minimum_fraction_digits},
        {"maximumFractionDigits", digits.maximum_fraction_digits},
        {"minimumSignificantDigits", digits.minimum_significant_digits},
        {"maximumSignificantDigits", digits.maximum_significant_digits},
        {"roundingIncrement", digits.rounding_increment},
        {"roundingMode", digits.rounding_mode},
        {"roundingType", digits.rounding_type},
        {"trailingZeroDisplay", digits.trailing_zero_display},
        {"computedRoundingPriority", digits.computed_rounding_priority},
    };
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        return result;
    for (i = 0; i < sizeof(properties) / sizeof(properties[0]); i++) {
        if (properties[i].value >= 0 &&
            JS_DefinePropertyValueStr(ctx, result, properties[i].name,
                                      JS_NewInt32(ctx, properties[i].value), JS_PROP_C_W_E) < 0) {
            JS_FreeValue(ctx, result);
            return JS_EXCEPTION;
        }
    }
    return result;
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
    assert(JS_SetPropertyStr(ctx, global, "readDigitOptions",
                             JS_NewCFunction(ctx, read_options, "readDigitOptions", 4)) >= 0);
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
    return 0;
}
