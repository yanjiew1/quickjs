/* Shared Temporal/Intl duration record conversion, independent of constructors.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "duration-value.h"
#include <math.h>

static const char *const duration_fields[] = {
    "years", "months", "weeks", "days", "hours", "minutes", "seconds",
    "milliseconds", "microseconds", "nanoseconds",
};

int js_duration_to_integral_field(JSContext *ctx, JSValueConst value,
                                  double *result)
{
    double number;

    if (JS_ToFloat64(ctx, &number, value))
        return -1;
    if (!isfinite(number) || trunc(number) != number) {
        JS_ThrowRangeError(ctx, "Temporal.Duration fields must be integral");
        return -1;
    }
    *result = number == 0 ? 0 : number;
    return 0;
}

/* Fields starts with either all zeros or the receiver's existing fields.
   Undefined properties leave it unchanged. Every recognized property is
   read in the spec's alphabetical order, including after a nonzero field. */
int js_duration_partial_fields(JSContext *ctx, JSValueConst object,
                               QJSTemporalDuration *fields)
{
    static const unsigned order[] = { 3, 4, 8, 7, 5, 1, 9, 6, 2, 0 };
    JSValue value;
    int i, index, any = 0, ret;

    if (!JS_IsObject(object)) {
        JS_ThrowTypeError(ctx, "Temporal duration input must be an object");
        return -1;
    }
    for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++) {
        index = order[i];
        value = JS_GetPropertyStr(ctx, object, duration_fields[index]);
        if (JS_IsException(value))
            return -1;
        if (!JS_IsUndefined(value)) {
            any = 1;
            ret = js_duration_to_integral_field(ctx, value,
                                                &fields->fields[index]);
            JS_FreeValue(ctx, value);
            if (ret)
                return -1;
        } else {
            JS_FreeValue(ctx, value);
        }
    }
    if (!any) {
        JS_ThrowTypeError(ctx, "Temporal duration input has no duration fields");
        return -1;
    }
    return 0;
}

int js_duration_to_record(JSContext *ctx, JSValueConst value,
                          QJSTemporalDuration *out)
{
    QJSTemporalDuration result = { { 0 } };
    const char *text;
    size_t length;
    int ret;

    if (JS_IsObject(value)) {
#ifdef CONFIG_TEMPORAL
        const QJSTemporalDuration *data = JS_GetOpaque(value, JS_CLASS_TEMPORAL_DURATION);
        if (data) {
            *out = *data;
            return 0;
        }
#endif
        if (js_duration_partial_fields(ctx, value, &result))
            return -1;
        if (!qjs_temporal_duration_is_valid(&result)) {
            JS_ThrowRangeError(ctx, "invalid Temporal.Duration fields");
            return -1;
        }
    } else {
        if (!JS_IsString(value)) {
            JS_ThrowTypeError(ctx, "Temporal duration input must be a string");
            return -1;
        }
        text = JS_ToCStringLen(ctx, &length, value);
        if (!text)
            return -1;
        ret = qjs_temporal_parse_duration(&result, text, length);
        JS_FreeCString(ctx, text);
        if (ret) {
            JS_ThrowRangeError(ctx, "invalid Temporal duration string");
            return -1;
        }
    }
    *out = result;
    return 0;
}
