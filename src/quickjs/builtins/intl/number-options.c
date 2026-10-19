/* SetNumberFormatDigitOptions, ECMA-402
 * 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "number-options.h"
#include "../intl.h"
#include <assert.h>
#include <math.h>

int js_intl_set_digit_options(JSContext *ctx, JSValueConst options, int minimum_fraction_default,
                              int maximum_fraction_default, BOOL compact, JSIntlDigitOptions *out)
{
    static const int increments[] = {1,   2,   5,   10,   20,   25,   50,  100,
                                     200, 250, 500, 1000, 2000, 2500, 5000};
    static const char *const modes[] = {"ceil",       "floor",     "expand",
                                        "trunc",      "halfCeil",  "halfFloor",
                                        "halfExpand", "halfTrunc", "halfEven"};
    static const char *const priorities[] = {"auto", "morePrecision", "lessPrecision"};
    static const char *const trailing[] = {"auto", "stripIfInteger"};
    JSIntlDigitOptions result = {0};
    JSValue mnfd = JS_UNDEFINED, mxfd = JS_UNDEFINED;
    JSValue mnsd = JS_UNDEFINED, mxsd = JS_UNDEFINED;
    int has_sd, has_fd, need_sd = 1, need_fd = 1, mode, priority, zero, ret = -1;
    size_t i;
    assert(out && JS_IsObject(options));
    assert(minimum_fraction_default >= 0 && minimum_fraction_default <= maximum_fraction_default &&
           maximum_fraction_default <= 100);
    memset(out, 0, sizeof(*out));
    result.minimum_fraction_digits = result.maximum_fraction_digits = -1;
    result.minimum_significant_digits = result.maximum_significant_digits = -1;
    if (js_intl_get_number_option(ctx, options, "minimumIntegerDigits", 1, 21, 1,
                                  &result.minimum_integer_digits))
        goto done;
    /* Get all four raw values before reading rounding options or coercing
       fraction/significant values. Even unused options still have getters. */
    mnfd = JS_GetPropertyStr(ctx, options, "minimumFractionDigits");
    if (JS_IsException(mnfd))
        goto done;
    mxfd = JS_GetPropertyStr(ctx, options, "maximumFractionDigits");
    if (JS_IsException(mxfd))
        goto done;
    mnsd = JS_GetPropertyStr(ctx, options, "minimumSignificantDigits");
    if (JS_IsException(mnsd))
        goto done;
    mxsd = JS_GetPropertyStr(ctx, options, "maximumSignificantDigits");
    if (JS_IsException(mxsd))
        goto done;
    if (js_intl_get_number_option(ctx, options, "roundingIncrement", 1, 5000, 1,
                                  &result.rounding_increment))
        goto done;
    for (i = 0; i < countof(increments); i++) {
        if (increments[i] == result.rounding_increment)
            break;
    }
    if (i == countof(increments)) {
        JS_ThrowRangeError(ctx, "invalid roundingIncrement");
        goto done;
    }
    mode = js_intl_string_option(ctx, options, "roundingMode", modes, countof(modes),
                                 QJS_INTL_ROUND_HALF_EXPAND);
    if (mode < 0)
        goto done;
    priority =
        js_intl_string_option(ctx, options, "roundingPriority", priorities, countof(priorities), 0);
    if (priority < 0)
        goto done;
    zero = js_intl_string_option(ctx, options, "trailingZeroDisplay", trailing, countof(trailing),
                                 QJS_INTL_TRAILING_ZERO_AUTO);
    if (zero < 0)
        goto done;
    result.rounding_mode = mode;
    result.trailing_zero_display = zero;
    result.computed_rounding_priority = priority;
    if (result.rounding_increment != 1)
        maximum_fraction_default = minimum_fraction_default;
    has_sd = !JS_IsUndefined(mnsd) || !JS_IsUndefined(mxsd);
    has_fd = !JS_IsUndefined(mnfd) || !JS_IsUndefined(mxfd);
    if (!priority) {
        need_sd = has_sd;
        if (need_sd || (!has_fd && compact))
            need_fd = 0;
    }
    if (need_sd) {
        if (js_intl_default_number_option(ctx, mnsd, 1, 21, 1, &result.minimum_significant_digits) ||
            js_intl_default_number_option(ctx, mxsd, result.minimum_significant_digits, 21, 21,
                                          &result.maximum_significant_digits))
            goto done;
    }
    if (need_fd) {
        if (has_fd) {
            if (js_intl_default_number_option(ctx, mnfd, 0, 100, -1, &result.minimum_fraction_digits) ||
                js_intl_default_number_option(ctx, mxfd, 0, 100, -1, &result.maximum_fraction_digits))
                goto done;
            if (result.minimum_fraction_digits < 0)
                result.minimum_fraction_digits =
                    min_int(minimum_fraction_default, result.maximum_fraction_digits);
            else if (result.maximum_fraction_digits < 0)
                result.maximum_fraction_digits =
                    max_int(maximum_fraction_default, result.minimum_fraction_digits);
            else if (result.minimum_fraction_digits > result.maximum_fraction_digits) {
                JS_ThrowRangeError(ctx, "minimumFractionDigits exceeds maximumFractionDigits");
                goto done;
            }
        } else {
            result.minimum_fraction_digits = minimum_fraction_default;
            result.maximum_fraction_digits = maximum_fraction_default;
        }
    }
    if (!need_sd && !need_fd) {
        result.minimum_fraction_digits = result.maximum_fraction_digits = 0;
        result.minimum_significant_digits = 1;
        result.maximum_significant_digits = 2;
        result.rounding_type = QJS_INTL_ROUND_MORE_PRECISION;
        result.computed_rounding_priority = 1;
    } else if (priority == 1) {
        result.rounding_type = QJS_INTL_ROUND_MORE_PRECISION;
    } else if (priority == 2) {
        result.rounding_type = QJS_INTL_ROUND_LESS_PRECISION;
    } else {
        result.rounding_type = has_sd ? QJS_INTL_ROUND_SIGNIFICANT : QJS_INTL_ROUND_FRACTION;
    }
    if (result.rounding_increment != 1) {
        if (result.rounding_type != QJS_INTL_ROUND_FRACTION) {
            JS_ThrowTypeError(ctx, "roundingIncrement requires fraction precision");
            goto done;
        }
        if (result.minimum_fraction_digits != result.maximum_fraction_digits) {
            JS_ThrowRangeError(ctx, "roundingIncrement requires fixed fraction precision");
            goto done;
        }
    }
    *out = result;
    ret = 0;
done:
    JS_FreeValue(ctx, mnfd);
    JS_FreeValue(ctx, mxfd);
    JS_FreeValue(ctx, mnsd);
    JS_FreeValue(ctx, mxsd);
    return ret;
}
