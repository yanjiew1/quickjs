/*
 * QuickJS Temporal.PlainTime native implementation
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
#include "temporal-internal.h"
#include "../../../temporal/duration.h"
#include "../../../temporal/time.h"

static const char *const js_temporal_time_fields[] = {
    "hour", "minute", "second", "millisecond", "microsecond", "nanosecond",
};

static void js_temporal_plain_time_finalizer(JSRuntime *rt, JSValue value)
{
    js_free_rt(rt, JS_GetOpaque(value, JS_CLASS_TEMPORAL_PLAIN_TIME));
}

static const JSClassDef js_temporal_plain_time_class = {
    .class_name = "Temporal.PlainTime",
    .finalizer = js_temporal_plain_time_finalizer,
};

JSValue js_temporal_create_plain_time(JSContext *ctx, JSValueConst new_target,
                                      QJSTemporalISOTime time)
{
    JSTemporalPlainTimeData *data;
    JSValue result;

    result = js_temporal_create_from_ctor(ctx, new_target, JS_CLASS_TEMPORAL_PLAIN_TIME);
    if (JS_IsException(result))
        return result;
    data = js_malloc(ctx, sizeof(*data));
    if (!data) {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }
    *data = time;
    JS_SetOpaque(result, data);
    return result;
}

static int js_temporal_time_get_overflow(JSContext *ctx, JSValueConst options,
                                         int *overflow)
{
    static const char *const values[] = { "constrain", "reject" };
    JSValue resolved;
    int ret;

    resolved = js_temporal_get_options(ctx, options);
    if (JS_IsException(resolved))
        return -1;
    ret = js_temporal_get_string_option(ctx, resolved, "overflow", values,
                                        2, 0, overflow);
    JS_FreeValue(ctx, resolved);
    return ret;
}

static void js_temporal_time_copy_fields(double *fields,
                                         QJSTemporalISOTime time)
{
    fields[0] = time.hour;
    fields[1] = time.minute;
    fields[2] = time.second;
    fields[3] = time.millisecond;
    fields[4] = time.microsecond;
    fields[5] = time.nanosecond;
}

static int js_temporal_time_regulate(JSContext *ctx,
                                     QJSTemporalISOTime *result,
                                     const double *fields, int overflow)
{
    static const int maximum[] = { 23, 59, 59, 999, 999, 999 };
    int32_t values[6];
    double value;
    int i;

    for (i = 0; i < 6; i++) {
        value = fields[i];
        if (value < 0 || value > maximum[i]) {
            if (overflow) {
                JS_ThrowRangeError(ctx, "Temporal.PlainTime field out of range");
                return -1;
            }
            value = value < 0 ? 0 : maximum[i];
        }
        values[i] = (int32_t)value;
    }
    result->hour = values[0];
    result->minute = values[1];
    result->second = values[2];
    result->millisecond = values[3];
    result->microsecond = values[4];
    result->nanosecond = values[5];
    return 0;
}

/* In a partial record existing values are already in fields. The six
   Gets and conversions occur in alphabetical property order. */
static int js_temporal_to_time_fields(JSContext *ctx, JSValueConst object,
                                      double *fields)
{
    static const unsigned order[] = { 0, 4, 3, 1, 5, 2 };
    JSValue value;
    int i, index, any = 0, ret;

    for (i = 0; i < 6; i++) {
        index = order[i];
        value = JS_GetPropertyStr(ctx, object, js_temporal_time_fields[index]);
        if (JS_IsException(value))
            return -1;
        if (!JS_IsUndefined(value)) {
            any = 1;
            ret = js_temporal_to_integer(ctx, value, &fields[index]);
            JS_FreeValue(ctx, value);
            if (ret)
                return -1;
        } else {
            JS_FreeValue(ctx, value);
        }
    }
    if (!any) {
        JS_ThrowTypeError(ctx, "Temporal time input has no time fields");
        return -1;
    }
    return 0;
}

JSValue js_temporal_to_plain_time(JSContext *ctx, JSValueConst value,
                                  JSValueConst options)
{
    JSTemporalPlainTimeData *time;
    JSTemporalPlainDateTimeData *date_time;
    JSTemporalZonedDateTimeData *zoned;
    QJSTemporalISOTime result;
    QJSTemporalISODateTime local;
    QJSTemporalParsedISO parsed;
    double fields[6] = { 0 };
    const char *text;
    size_t length;
    int overflow, ret;

    if (JS_IsObject(value)) {
        time = JS_GetOpaque(value, JS_CLASS_TEMPORAL_PLAIN_TIME);
        date_time = JS_GetOpaque(value, JS_CLASS_TEMPORAL_PLAIN_DATE_TIME);
        zoned = JS_GetOpaque(value, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
        if (time) {
            result = *time;
        } else if (date_time) {
            result = date_time->datetime.time;
        } else if (zoned) {
            if (js_temporal_time_zone_datetime(ctx, &zoned->time_zone,
                                               zoned->epoch_nanoseconds,
                                               &local))
                return JS_EXCEPTION;
            result = local.time;
        } else {
            if (js_temporal_to_time_fields(ctx, value, fields) ||
                js_temporal_time_get_overflow(ctx, options, &overflow) ||
                js_temporal_time_regulate(ctx, &result, fields, overflow))
                return JS_EXCEPTION;
            return js_temporal_create_plain_time(ctx, JS_UNDEFINED, result);
        }
    } else {
        if (!JS_IsString(value))
            return JS_ThrowTypeError(ctx, "Temporal time input must be a string");
        text = JS_ToCStringLen(ctx, &length, value);
        if (!text)
            return JS_EXCEPTION;
        ret = qjs_temporal_parse_iso_datetime(&parsed, text, length,
                                              QJS_TEMPORAL_PARSE_TIME);
        JS_FreeCString(ctx, text);
        if (ret)
            return JS_ThrowRangeError(ctx, "invalid Temporal time string");
        result = parsed.datetime.time;
    }
    if (js_temporal_time_get_overflow(ctx, options, &overflow))
        return JS_EXCEPTION;
    return js_temporal_create_plain_time(ctx, JS_UNDEFINED, result);
}

static JSValue js_temporal_plain_time_constructor(JSContext *ctx,
                                                  JSValueConst new_target,
                                                  int argc,
                                                  JSValueConst *argv)
{
    double fields[6] = { 0 };
    QJSTemporalISOTime time;
    int i;

    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Temporal.PlainTime requires new");
    for (i = 0; i < 6; i++) {
        if (i < argc && !JS_IsUndefined(argv[i]) &&
            js_temporal_to_integer(ctx, argv[i], &fields[i]))
            return JS_EXCEPTION;
    }
    if (js_temporal_time_regulate(ctx, &time, fields, 1))
        return JS_EXCEPTION;
    return js_temporal_create_plain_time(ctx, new_target, time);
}

static JSValue js_temporal_plain_time_from(JSContext *ctx,
                                           JSValueConst this_val,
                                           int argc, JSValueConst *argv)
{
    return js_temporal_to_plain_time(ctx, argv[0],
                                     argc > 1 ? argv[1] : JS_UNDEFINED);
}

static JSValue js_temporal_plain_time_compare(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv)
{
    JSValue one, two;
    int64_t a, b;

    one = js_temporal_to_plain_time(ctx, argv[0], JS_UNDEFINED);
    if (JS_IsException(one))
        return one;
    two = js_temporal_to_plain_time(ctx, argv[1], JS_UNDEFINED);
    if (JS_IsException(two)) {
        JS_FreeValue(ctx, one);
        return two;
    }
    a = qjs_temporal_time_nanoseconds(
        *(JSTemporalPlainTimeData *)JS_GetOpaque(one, JS_CLASS_TEMPORAL_PLAIN_TIME));
    b = qjs_temporal_time_nanoseconds(
        *(JSTemporalPlainTimeData *)JS_GetOpaque(two, JS_CLASS_TEMPORAL_PLAIN_TIME));
    JS_FreeValue(ctx, one);
    JS_FreeValue(ctx, two);
    return JS_NewInt32(ctx, (a > b) - (a < b));
}

static JSValue js_temporal_plain_time_get(JSContext *ctx,
                                          JSValueConst this_val, int magic)
{
    JSTemporalPlainTimeData *time;
    double fields[6];

    time = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_PLAIN_TIME);
    if (!time)
        return JS_EXCEPTION;
    js_temporal_time_copy_fields(fields, *time);
    return JS_NewInt32(ctx, (int32_t)fields[magic]);
}

static JSValue js_temporal_plain_time_with(JSContext *ctx,
                                           JSValueConst this_val,
                                           int argc, JSValueConst *argv)
{
    JSTemporalPlainTimeData *time;
    QJSTemporalISOTime result;
    double fields[6];
    int partial, overflow;

    time = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_PLAIN_TIME);
    if (!time)
        return JS_EXCEPTION;
    partial = js_temporal_is_partial_object(ctx, argv[0]);
    if (partial < 0)
        return JS_EXCEPTION;
    if (!partial)
        return JS_ThrowTypeError(ctx, "invalid partial Temporal time object");
    js_temporal_time_copy_fields(fields, *time);
    if (js_temporal_to_time_fields(ctx, argv[0], fields) ||
        js_temporal_time_get_overflow(ctx, argc > 1 ? argv[1] : JS_UNDEFINED,
                                       &overflow) ||
        js_temporal_time_regulate(ctx, &result, fields, overflow))
        return JS_EXCEPTION;
    return js_temporal_create_plain_time(ctx, JS_UNDEFINED, result);
}

static JSValue js_temporal_plain_time_add(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv,
                                          int subtract)
{
    JSTemporalPlainTimeData *time;
    QJSTemporalInternalDuration normalized;
    QJSTemporalISOTime result;
    QJSTemporalEpochNs zero = { 0, 0 };
    JSValue duration;
    int64_t days;
    int ret;

    time = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_PLAIN_TIME);
    if (!time)
        return JS_EXCEPTION;
    duration = js_temporal_to_duration(ctx, argv[0]);
    if (JS_IsException(duration))
        return duration;
    ret = qjs_temporal_duration_normalize(&normalized,
        JS_GetOpaque(duration, JS_CLASS_TEMPORAL_DURATION), 0);
    JS_FreeValue(ctx, duration);
    if (!ret && subtract)
        ret = qjs_temporal_epoch_ns_subtract(&normalized.time, zero,
                                              normalized.time);
    if (ret || qjs_temporal_time_add(&result, &days, *time, normalized.time))
        return JS_ThrowRangeError(ctx, "Temporal time arithmetic overflow");
    return js_temporal_create_plain_time(ctx, JS_UNDEFINED, result);
}

static JSValue js_temporal_plain_time_difference(JSContext *ctx,
                                                 JSValueConst this_val,
                                                 int argc,
                                                 JSValueConst *argv,
                                                 int since)
{
    JSTemporalPlainTimeData *time, *other;
    QJSTemporalDifferenceSettings settings;
    QJSTemporalInternalDuration internal = { { 0 }, { 0, 0 } };
    QJSTemporalDuration duration;
    JSValue converted, options;
    int ret, i;

    time = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_PLAIN_TIME);
    if (!time)
        return JS_EXCEPTION;
    converted = js_temporal_to_plain_time(ctx, argv[0], JS_UNDEFINED);
    if (JS_IsException(converted))
        return converted;
    other = JS_GetOpaque(converted, JS_CLASS_TEMPORAL_PLAIN_TIME);
    internal.time = qjs_temporal_epoch_ns_from_int64(
        qjs_temporal_time_nanoseconds(*other) -
        qjs_temporal_time_nanoseconds(*time));
    JS_FreeValue(ctx, converted);
    options = js_temporal_get_options(ctx, argc > 1 ? argv[1] : JS_UNDEFINED);
    if (JS_IsException(options))
        return options;
    ret = js_temporal_get_difference_settings(ctx, options, since,
        QJS_TEMPORAL_HOUR, QJS_TEMPORAL_NANOSECOND,
        QJS_TEMPORAL_NANOSECOND, QJS_TEMPORAL_HOUR, &settings);
    JS_FreeValue(ctx, options);
    if (ret)
        return JS_EXCEPTION;
    if (qjs_temporal_epoch_ns_round(&internal.time, internal.time,
            qjs_temporal_unit_nanoseconds(settings.smallest_unit) *
                settings.rounding_increment,
            settings.rounding_mode) ||
        qjs_temporal_duration_from_internal(&duration, internal,
                                             settings.largest_unit))
        return JS_ThrowRangeError(ctx, "Temporal time difference overflow");
    if (since) {
        for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++) {
            if (duration.fields[i])
                duration.fields[i] = -duration.fields[i];
        }
    }
    return js_temporal_create_duration(ctx, JS_UNDEFINED, &duration);
}

static JSValue js_temporal_plain_time_round(JSContext *ctx,
                                            JSValueConst this_val,
                                            int argc, JSValueConst *argv)
{
    JSTemporalPlainTimeData *time;
    QJSTemporalISOTime result;
    QJSTemporalRoundingMode mode;
    QJSTemporalUnit unit;
    JSValue options;
    uint32_t increment;
    uint64_t maximum;
    int ret;

    time = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_PLAIN_TIME);
    if (!time)
        return JS_EXCEPTION;
    if (JS_IsUndefined(argv[0]))
        return JS_ThrowTypeError(ctx, "Temporal rounding options required");
    if (JS_IsString(argv[0])) {
        options = JS_NewObjectProto(ctx, JS_NULL);
        if (JS_IsException(options))
            return options;
        if (JS_DefinePropertyValueStr(ctx, options, "smallestUnit",
                                      JS_DupValue(ctx, argv[0]),
                                      JS_PROP_C_W_E) < 0) {
            JS_FreeValue(ctx, options);
            return JS_EXCEPTION;
        }
    } else {
        options = js_temporal_get_options(ctx, argv[0]);
        if (JS_IsException(options))
            return options;
    }
    ret = js_temporal_get_rounding_increment(ctx, options, &increment) ||
        js_temporal_get_rounding_mode(ctx, options,
                                      QJS_TEMPORAL_ROUND_HALF_EXPAND, &mode) ||
        js_temporal_get_unit_option(ctx, options, "smallestUnit", TRUE, &unit);
    JS_FreeValue(ctx, options);
    if (ret)
        return JS_EXCEPTION;
    if (unit < QJS_TEMPORAL_HOUR || unit > QJS_TEMPORAL_NANOSECOND)
        return JS_ThrowRangeError(ctx, "Temporal time requires a time unit");
    maximum = unit == QJS_TEMPORAL_HOUR ? 24 :
        unit <= QJS_TEMPORAL_SECOND ? 60 : 1000;
    if (js_temporal_validate_increment(ctx, increment, maximum, FALSE))
        return JS_EXCEPTION;
    if (qjs_temporal_time_round(&result, *time, increment, unit, mode))
        return JS_ThrowRangeError(ctx, "Temporal time rounding overflow");
    return js_temporal_create_plain_time(ctx, JS_UNDEFINED, result);
}

static JSValue js_temporal_plain_time_equals(JSContext *ctx,
                                             JSValueConst this_val,
                                             int argc, JSValueConst *argv)
{
    JSTemporalPlainTimeData *time, *other;
    JSValue converted;
    int equal;

    time = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_PLAIN_TIME);
    if (!time)
        return JS_EXCEPTION;
    converted = js_temporal_to_plain_time(ctx, argv[0], JS_UNDEFINED);
    if (JS_IsException(converted))
        return converted;
    other = JS_GetOpaque(converted, JS_CLASS_TEMPORAL_PLAIN_TIME);
    equal = qjs_temporal_time_nanoseconds(*time) ==
        qjs_temporal_time_nanoseconds(*other);
    JS_FreeValue(ctx, converted);
    return JS_NewBool(ctx, equal);
}

static JSValue js_temporal_plain_time_to_string(JSContext *ctx,
                                                JSValueConst this_val,
                                                int argc,
                                                JSValueConst *argv, int kind)
{
    JSTemporalPlainTimeData *time;
    QJSTemporalISOTime result;
    QJSTemporalStringPrecision precision;
    QJSTemporalRoundingMode mode;
    QJSTemporalUnit unit;
    JSValue options;
    char buffer[32];
    int digits, ret;

    time = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_PLAIN_TIME);
    if (!time)
        return JS_EXCEPTION;
#ifdef CONFIG_INTL
    if (kind == 2)
        return js_intl_temporal_to_locale_string(ctx, this_val,
            argc > 0 ? argv[0] : JS_UNDEFINED,
            argc > 1 ? argv[1] : JS_UNDEFINED);
#endif
    result = *time;
    precision.precision = -1;
    if (kind == 0) {
        options = js_temporal_get_options(ctx, argc ? argv[0] : JS_UNDEFINED);
        if (JS_IsException(options))
            return options;
        ret = js_temporal_get_fractional_digits(ctx, options, &digits) ||
            js_temporal_get_rounding_mode(ctx, options,
                                          QJS_TEMPORAL_ROUND_TRUNC, &mode) ||
            js_temporal_get_unit_option(ctx, options, "smallestUnit", FALSE,
                                        &unit);
        JS_FreeValue(ctx, options);
        if (ret)
            return JS_EXCEPTION;
        if (unit != QJS_TEMPORAL_UNIT_UNSET &&
            (unit < QJS_TEMPORAL_MINUTE || unit > QJS_TEMPORAL_NANOSECOND))
            return JS_ThrowRangeError(ctx, "invalid Temporal time string unit");
        if (qjs_temporal_string_precision(&precision, unit, digits) ||
            qjs_temporal_time_round(&result, *time, precision.increment,
                                     precision.unit, mode))
            return JS_ThrowRangeError(ctx, "Temporal time formatting overflow");
    }
    ret = qjs_temporal_format_time(buffer, sizeof(buffer), result,
                                    precision.precision);
    if (ret < 0)
        return JS_ThrowRangeError(ctx, "Temporal time formatting overflow");
    return JS_NewStringLen(ctx, buffer, ret);
}

static JSValue js_temporal_plain_time_value_of(JSContext *ctx,
                                               JSValueConst this_val,
                                               int argc, JSValueConst *argv)
{
    return JS_ThrowTypeError(ctx, "Temporal.PlainTime cannot become a number");
}

static const JSCFunctionListEntry js_temporal_plain_time_funcs[] = {
    JS_CFUNC_DEF("from", 1, js_temporal_plain_time_from),
    JS_CFUNC_DEF("compare", 2, js_temporal_plain_time_compare),
};

static const JSCFunctionListEntry js_temporal_plain_time_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("hour", js_temporal_plain_time_get, NULL, 0),
    JS_CGETSET_MAGIC_DEF("minute", js_temporal_plain_time_get, NULL, 1),
    JS_CGETSET_MAGIC_DEF("second", js_temporal_plain_time_get, NULL, 2),
    JS_CGETSET_MAGIC_DEF("millisecond", js_temporal_plain_time_get, NULL, 3),
    JS_CGETSET_MAGIC_DEF("microsecond", js_temporal_plain_time_get, NULL, 4),
    JS_CGETSET_MAGIC_DEF("nanosecond", js_temporal_plain_time_get, NULL, 5),
    JS_CFUNC_DEF("with", 1, js_temporal_plain_time_with),
    JS_CFUNC_MAGIC_DEF("add", 1, js_temporal_plain_time_add, 0),
    JS_CFUNC_MAGIC_DEF("subtract", 1, js_temporal_plain_time_add, 1),
    JS_CFUNC_MAGIC_DEF("until", 1, js_temporal_plain_time_difference, 0),
    JS_CFUNC_MAGIC_DEF("since", 1, js_temporal_plain_time_difference, 1),
    JS_CFUNC_DEF("round", 1, js_temporal_plain_time_round),
    JS_CFUNC_DEF("equals", 1, js_temporal_plain_time_equals),
    JS_CFUNC_MAGIC_DEF("toString", 0, js_temporal_plain_time_to_string, 0),
    JS_CFUNC_MAGIC_DEF("toJSON", 0, js_temporal_plain_time_to_string, 1),
    JS_CFUNC_MAGIC_DEF("toLocaleString", 0, js_temporal_plain_time_to_string, 2),
    JS_CFUNC_DEF("valueOf", 0, js_temporal_plain_time_value_of),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Temporal.PlainTime",
                       JS_PROP_CONFIGURABLE),
};

int js_temporal_init_plain_time(JSContext *ctx, JSValueConst namespace_object)
{
    JSValue constructor;
    int ret;

    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_TEMPORAL_PLAIN_TIME) &&
        JS_NewClass(ctx->rt, JS_CLASS_TEMPORAL_PLAIN_TIME,
                     &js_temporal_plain_time_class) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    constructor = JS_NewCConstructor(ctx, JS_CLASS_TEMPORAL_PLAIN_TIME,
        "PlainTime", js_temporal_plain_time_constructor, 0,
        JS_CFUNC_constructor, 0, JS_UNDEFINED,
        js_temporal_plain_time_funcs, countof(js_temporal_plain_time_funcs),
        js_temporal_plain_time_proto_funcs,
        countof(js_temporal_plain_time_proto_funcs), JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(constructor) || JS_HasException(ctx)) {
        JS_FreeValue(ctx, constructor);
        return -1;
    }
    ret = JS_DefinePropertyValueStr(ctx, namespace_object, "PlainTime",
                                     constructor,
                                     JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    return ret < 0 ? -1 : 0;
}
