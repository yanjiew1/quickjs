/*
 * Native Temporal support
 *
 * Copyright (c) 2026 Yan-Jie Wang
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
#include <math.h>
#include <string.h>
#include "temporal-internal.h"

int js_temporal_to_epoch_ns(JSContext *ctx, JSValueConst val,
                            QJSTemporalEpochNs *epoch_ns)
{
    int ret;

    ret = JS_ToBigInt128Sat(ctx, &epoch_ns->low, &epoch_ns->high, val);
    if (ret < 0)
        return -1;
    if (ret || !qjs_temporal_epoch_ns_is_valid(*epoch_ns)) {
        JS_ThrowRangeError(ctx, "Temporal epoch nanoseconds are outside the valid range");
        return -1;
    }
    return 0;
}

int js_temporal_is_partial_object(JSContext *ctx, JSValueConst value)
{
    JSValue property;
    int valid;

    if (!JS_IsObject(value))
        return 0;
    if (JS_GetOpaque(value, JS_CLASS_TEMPORAL_PLAIN_DATE) ||
        JS_GetOpaque(value, JS_CLASS_TEMPORAL_PLAIN_DATE_TIME) ||
        JS_GetOpaque(value, JS_CLASS_TEMPORAL_PLAIN_MONTH_DAY) ||
        JS_GetOpaque(value, JS_CLASS_TEMPORAL_PLAIN_TIME) ||
        JS_GetOpaque(value, JS_CLASS_TEMPORAL_PLAIN_YEAR_MONTH) ||
        JS_GetOpaque(value, JS_CLASS_TEMPORAL_ZONED_DATE_TIME))
        return 0;
    property = JS_GetPropertyStr(ctx, value, "calendar");
    if (JS_IsException(property))
        return -1;
    valid = JS_IsUndefined(property);
    JS_FreeValue(ctx, property);
    if (!valid)
        return 0;
    property = JS_GetPropertyStr(ctx, value, "timeZone");
    if (JS_IsException(property))
        return -1;
    valid = JS_IsUndefined(property);
    JS_FreeValue(ctx, property);
    return valid;
}

JSValue js_temporal_get_options(JSContext *ctx, JSValueConst value)
{
    if (JS_IsUndefined(value))
        return JS_NewObjectProto(ctx, JS_NULL);
    if (!JS_IsObject(value))
        return JS_ThrowTypeError(ctx, "Temporal options must be an object");
    return JS_DupValue(ctx, value);
}

int js_temporal_get_string_option(JSContext *ctx, JSValueConst options,
                                 const char *name,
                                 const char *const *values, int count,
                                 int fallback, int *result)
{
    JSValue value, string;
    const char *text;
    size_t length;
    int i;

    value = JS_GetPropertyStr(ctx, options, name);
    if (JS_IsException(value))
        return -1;
    if (JS_IsUndefined(value)) {
        JS_FreeValue(ctx, value);
        *result = fallback;
        return 0;
    }
    string = JS_ToString(ctx, value);
    JS_FreeValue(ctx, value);
    if (JS_IsException(string))
        return -1;
    text = JS_ToCStringLen(ctx, &length, string);
    JS_FreeValue(ctx, string);
    if (!text)
        return -1;
    for (i = 0; i < count; i++) {
        if (strlen(values[i]) == length &&
            !memcmp(text, values[i], length))
            break;
    }
    JS_FreeCString(ctx, text);
    if (i == count) {
        JS_ThrowRangeError(ctx, "invalid Temporal %s option", name);
        return -1;
    }
    *result = i;
    return 0;
}

int js_temporal_get_unit_option(JSContext *ctx, JSValueConst options,
                               const char *name, BOOL required,
                               QJSTemporalUnit *result)
{
    static const char *const names[] = {
        "year", "years", "month", "months", "week", "weeks", "day", "days",
        "hour", "hours", "minute", "minutes", "second", "seconds",
        "millisecond", "milliseconds", "microsecond", "microseconds",
        "nanosecond", "nanoseconds", "auto",
    };
    int value;

    if (js_temporal_get_string_option(ctx, options, name, names,
                                      countof(names), -1, &value))
        return -1;
    if (value == -1) {
        if (required) {
            JS_ThrowRangeError(ctx, "Temporal %s is required", name);
            return -1;
        }
        *result = QJS_TEMPORAL_UNIT_UNSET;
    } else {
        *result = value == 20 ? QJS_TEMPORAL_UNIT_AUTO : value / 2;
    }
    return 0;
}

int js_temporal_to_integer(JSContext *ctx, JSValueConst value, double *result)
{
    double number;

    if (JS_ToFloat64(ctx, &number, value))
        return -1;
    if (!isfinite(number)) {
        JS_ThrowRangeError(ctx, "Temporal integer must be finite");
        return -1;
    }
    *result = trunc(number);
    return 0;
}

int js_temporal_get_rounding_increment(JSContext *ctx, JSValueConst options,
                                      uint32_t *result)
{
    JSValue value;
    double number;
    int ret;

    value = JS_GetPropertyStr(ctx, options, "roundingIncrement");
    if (JS_IsException(value))
        return -1;
    if (JS_IsUndefined(value)) {
        JS_FreeValue(ctx, value);
        *result = 1;
        return 0;
    }
    ret = js_temporal_to_integer(ctx, value, &number);
    JS_FreeValue(ctx, value);
    if (ret)
        return -1;
    if (number < 1 || number > 1000000000) {
        JS_ThrowRangeError(ctx, "invalid Temporal roundingIncrement");
        return -1;
    }
    *result = (uint32_t)number;
    return 0;
}

int js_temporal_get_rounding_mode(JSContext *ctx, JSValueConst options,
                                 QJSTemporalRoundingMode fallback,
                                 QJSTemporalRoundingMode *result)
{
    static const char *const names[] = {
        "ceil", "floor", "expand", "trunc", "halfCeil", "halfFloor",
        "halfExpand", "halfTrunc", "halfEven",
    };
    int value;

    if (js_temporal_get_string_option(ctx, options, "roundingMode", names,
                                      countof(names), fallback, &value))
        return -1;
    *result = value;
    return 0;
}

int js_temporal_get_fractional_digits(JSContext *ctx, JSValueConst options,
                                     int *result)
{
    JSValue value, string;
    double digits;
    const char *text;
    size_t length;
    int valid;

    value = JS_GetPropertyStr(ctx, options, "fractionalSecondDigits");
    if (JS_IsException(value))
        return -1;
    if (JS_IsUndefined(value)) {
        JS_FreeValue(ctx, value);
        *result = -1;
        return 0;
    }
    if (!JS_IsNumber(value)) {
        string = JS_ToString(ctx, value);
        JS_FreeValue(ctx, value);
        if (JS_IsException(string))
            return -1;
        text = JS_ToCStringLen(ctx, &length, string);
        JS_FreeValue(ctx, string);
        if (!text)
            return -1;
        valid = length == 4 && !memcmp(text, "auto", 4);
        JS_FreeCString(ctx, text);
        if (!valid) {
            JS_ThrowRangeError(ctx, "invalid Temporal fractionalSecondDigits");
            return -1;
        }
        *result = -1;
        return 0;
    }
    valid = JS_ToFloat64(ctx, &digits, value);
    JS_FreeValue(ctx, value);
    if (valid)
        return -1;
    digits = floor(digits);
    if (!isfinite(digits) || digits < 0 || digits > 9) {
        JS_ThrowRangeError(ctx, "invalid Temporal fractionalSecondDigits");
        return -1;
    }
    *result = (int)digits;
    return 0;
}

int js_temporal_validate_increment(JSContext *ctx, uint32_t increment,
                                  uint64_t maximum, BOOL inclusive)
{
    if (!increment || increment > maximum ||
        (!inclusive && increment == maximum) || maximum % increment) {
        JS_ThrowRangeError(ctx, "invalid Temporal roundingIncrement");
        return -1;
    }
    return 0;
}

int js_temporal_get_difference_settings(
    JSContext *ctx, JSValueConst options, BOOL since,
    QJSTemporalUnit minimum, QJSTemporalUnit maximum,
    QJSTemporalUnit fallback_smallest, QJSTemporalUnit fallback_largest,
    QJSTemporalDifferenceSettings *result)
{
    QJSTemporalDifferenceSettings settings;
    uint64_t increment_maximum;

    if (js_temporal_get_unit_option(ctx, options, "largestUnit", FALSE,
                                   &settings.largest_unit) ||
        js_temporal_get_rounding_increment(ctx, options,
                                          &settings.rounding_increment) ||
        js_temporal_get_rounding_mode(ctx, options, QJS_TEMPORAL_ROUND_TRUNC,
                                     &settings.rounding_mode) ||
        js_temporal_get_unit_option(ctx, options, "smallestUnit", FALSE,
                                   &settings.smallest_unit))
        return -1;
    if (settings.largest_unit == QJS_TEMPORAL_UNIT_UNSET)
        settings.largest_unit = QJS_TEMPORAL_UNIT_AUTO;
    if (settings.largest_unit != QJS_TEMPORAL_UNIT_AUTO &&
        (settings.largest_unit < minimum || settings.largest_unit > maximum))
        goto invalid;
    if (settings.smallest_unit == QJS_TEMPORAL_UNIT_UNSET)
        settings.smallest_unit = fallback_smallest;
    if (settings.smallest_unit < minimum || settings.smallest_unit > maximum)
        goto invalid;
    if (settings.largest_unit == QJS_TEMPORAL_UNIT_AUTO)
        settings.largest_unit = settings.smallest_unit < fallback_largest ?
            settings.smallest_unit : fallback_largest;
    if (settings.largest_unit > settings.smallest_unit)
        goto invalid;
    switch (settings.smallest_unit) {
    case QJS_TEMPORAL_HOUR:
        increment_maximum = 24;
        break;
    case QJS_TEMPORAL_MINUTE:
    case QJS_TEMPORAL_SECOND:
        increment_maximum = 60;
        break;
    case QJS_TEMPORAL_MILLISECOND:
    case QJS_TEMPORAL_MICROSECOND:
    case QJS_TEMPORAL_NANOSECOND:
        increment_maximum = 1000;
        break;
    default:
        increment_maximum = 0;
        break;
    }
    if (increment_maximum &&
        js_temporal_validate_increment(ctx, settings.rounding_increment,
                                       increment_maximum, FALSE))
        return -1;
    if (since)
        settings.rounding_mode =
            qjs_temporal_negate_rounding_mode(settings.rounding_mode);
    *result = settings;
    return 0;
 invalid:
    JS_ThrowRangeError(ctx, "invalid Temporal difference unit");
    return -1;
}
