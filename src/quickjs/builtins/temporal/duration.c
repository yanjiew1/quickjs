/*
 * QuickJS Temporal.Duration native implementation
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
#include "temporal-internal.h"
#include "calendar-fields.h"
#include "../../../temporal/duration.h"
#include "../../../temporal/time.h"

static const char *const js_temporal_duration_fields[] = {
    "years", "months", "weeks", "days", "hours", "minutes", "seconds",
    "milliseconds", "microseconds", "nanoseconds",
};

static void js_temporal_duration_finalizer(JSRuntime *rt, JSValue value)
{
    js_free_rt(rt, JS_GetOpaque(value, JS_CLASS_TEMPORAL_DURATION));
}

static const JSClassDef js_temporal_duration_class = {
    .class_name = "Temporal.Duration",
    .finalizer = js_temporal_duration_finalizer,
};

JSValue js_temporal_create_duration(JSContext *ctx, JSValueConst new_target,
                                    const QJSTemporalDuration *duration)
{
    JSTemporalDurationData *data;
    JSValue result;
    int i;

    if (!qjs_temporal_duration_is_valid(duration))
        return JS_ThrowRangeError(ctx, "invalid Temporal.Duration fields");
    result = js_temporal_create_from_ctor(ctx, new_target, JS_CLASS_TEMPORAL_DURATION);
    if (JS_IsException(result))
        return result;
    data = js_malloc(ctx, sizeof(*data));
    if (!data) {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }
    *data = *duration;
    for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++) {
        if (!data->fields[i])
            data->fields[i] = 0;
    }
    JS_SetOpaque(result, data);
    return result;
}

static int js_temporal_to_integral_duration_field(JSContext *ctx,
                                                  JSValueConst value,
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
static int js_temporal_duration_partial_fields(JSContext *ctx,
                                               JSValueConst object,
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
        value = JS_GetPropertyStr(ctx, object, js_temporal_duration_fields[index]);
        if (JS_IsException(value))
            return -1;
        if (!JS_IsUndefined(value)) {
            any = 1;
            ret = js_temporal_to_integral_duration_field(ctx, value,
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

JSValue js_temporal_to_duration(JSContext *ctx, JSValueConst value)
{
    JSTemporalDurationData *data;
    QJSTemporalDuration result = { { 0 } };
    const char *text;
    size_t length;
    int ret;

    if (JS_IsObject(value)) {
        data = JS_GetOpaque(value, JS_CLASS_TEMPORAL_DURATION);
        if (data)
            return js_temporal_create_duration(ctx, JS_UNDEFINED, data);
        if (js_temporal_duration_partial_fields(ctx, value, &result))
            return JS_EXCEPTION;
    } else {
        if (!JS_IsString(value))
            return JS_ThrowTypeError(ctx, "Temporal duration input must be a string");
        text = JS_ToCStringLen(ctx, &length, value);
        if (!text)
            return JS_EXCEPTION;
        ret = qjs_temporal_parse_duration(&result, text, length);
        JS_FreeCString(ctx, text);
        if (ret)
            return JS_ThrowRangeError(ctx, "invalid Temporal duration string");
    }
    return js_temporal_create_duration(ctx, JS_UNDEFINED, &result);
}

static JSValue js_temporal_duration_constructor(JSContext *ctx,
                                                JSValueConst new_target,
                                                int argc,
                                                JSValueConst *argv)
{
    QJSTemporalDuration fields = { { 0 } };
    int i;

    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Temporal.Duration requires new");
    for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++) {
        if (i < argc && !JS_IsUndefined(argv[i]) &&
            js_temporal_to_integral_duration_field(ctx, argv[i],
                                                    &fields.fields[i]))
            return JS_EXCEPTION;
    }
    return js_temporal_create_duration(ctx, new_target, &fields);
}

static JSValue js_temporal_duration_from(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    return js_temporal_to_duration(ctx, argv[0]);
}

static JSValue js_temporal_duration_get(JSContext *ctx, JSValueConst this_val,
                                        int magic)
{
    JSTemporalDurationData *data;
    int sign;

    data = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_DURATION);
    if (!data)
        return JS_EXCEPTION;
    if (magic < QJS_TEMPORAL_UNIT_COUNT)
        return JS_NewFloat64(ctx, data->fields[magic]);
    sign = qjs_temporal_duration_sign(data);
    if (magic == QJS_TEMPORAL_UNIT_COUNT)
        return JS_NewInt32(ctx, sign);
    return JS_NewBool(ctx, sign == 0);
}

static JSValue js_temporal_duration_with(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    JSTemporalDurationData *data;
    QJSTemporalDuration result;

    data = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_DURATION);
    if (!data)
        return JS_EXCEPTION;
    result = *data;
    if (js_temporal_duration_partial_fields(ctx, argv[0], &result))
        return JS_EXCEPTION;
    return js_temporal_create_duration(ctx, JS_UNDEFINED, &result);
}

static JSValue js_temporal_duration_negated(JSContext *ctx,
                                            JSValueConst this_val,
                                            int argc, JSValueConst *argv,
                                            int absolute)
{
    JSTemporalDurationData *data;
    QJSTemporalDuration result;
    int i;

    data = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_DURATION);
    if (!data)
        return JS_EXCEPTION;
    for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++)
        result.fields[i] = absolute ? fabs(data->fields[i]) : -data->fields[i];
    return js_temporal_create_duration(ctx, JS_UNDEFINED, &result);
}

static JSValue js_temporal_duration_add(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv,
                                        int subtract)
{
    JSTemporalDurationData *data, *other;
    QJSTemporalInternalDuration one, two;
    QJSTemporalDuration result;
    QJSTemporalUnit largest, other_largest;
    JSValue converted;
    int ret;

    data = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_DURATION);
    if (!data)
        return JS_EXCEPTION;
    converted = js_temporal_to_duration(ctx, argv[0]);
    if (JS_IsException(converted))
        return converted;
    other = JS_GetOpaque(converted, JS_CLASS_TEMPORAL_DURATION);
    largest = qjs_temporal_duration_largest_unit(data);
    other_largest = qjs_temporal_duration_largest_unit(other);
    if (other_largest < largest)
        largest = other_largest;
    if (largest < QJS_TEMPORAL_DAY) {
        JS_FreeValue(ctx, converted);
        return JS_ThrowRangeError(ctx, "calendar duration addition requires a date");
    }
    ret = qjs_temporal_duration_normalize(&one, data, 1) ||
        qjs_temporal_duration_normalize(&two, other, 1);
    JS_FreeValue(ctx, converted);
    if (!ret) {
        if (subtract)
            ret = qjs_temporal_epoch_ns_subtract(&one.time, one.time, two.time);
        else
            ret = qjs_temporal_epoch_ns_add(&one.time, one.time, two.time);
    }
    if (ret || !qjs_temporal_time_duration_is_valid(one.time) ||
        qjs_temporal_duration_from_internal(&result, one, largest))
        return JS_ThrowRangeError(ctx, "Temporal duration addition overflow");
    return js_temporal_create_duration(ctx, JS_UNDEFINED, &result);
}

static int js_temporal_duration_calendar_days(JSContext *ctx,
                                              QJSTemporalDateDuration duration,
                                              JSTemporalPlainDateData *relative,
                                              int64_t *result)
{
    QJSTemporalISODate after;
    QJSTemporalDateDuration date = duration;
    int64_t one, two;

    if (!duration.years && !duration.months && !duration.weeks) {
        *result = duration.days;
        return 0;
    }
    date.days = 0;
    if (js_temporal_calendar_date_add(ctx, relative->calendar, relative->date,
                                      date, QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &after))
        return -1;
    if (qjs_temporal_iso_date_to_days(&one, relative->date) ||
        qjs_temporal_iso_date_to_days(&two, after)) {
        JS_ThrowRangeError(ctx, "Temporal date difference overflow");
        return -1;
    }
    *result = duration.days + two - one;
    return 0;
}

static JSValue js_temporal_duration_compare(JSContext *ctx,
                                            JSValueConst this_val,
                                            int argc, JSValueConst *argv)
{
    JSValue one = JS_UNDEFINED, two = JS_UNDEFINED, options;
    JSTemporalDurationData *a, *b;
    JSTemporalPlainDateData *plain;
    JSTemporalZonedDateTimeData *zoned;
    JSTemporalRelativeTo relative = { JS_UNDEFINED, JS_UNDEFINED };
    QJSTemporalInternalDuration first, second;
    QJSTemporalEpochNs after_one, after_two;
    QJSTemporalUnit first_unit, second_unit;
    int64_t days_one, days_two;
    int i, comparison = 0, ret;
    JSValue result = JS_EXCEPTION;

    one = js_temporal_to_duration(ctx, argv[0]);
    if (JS_IsException(one))
        goto done;
    two = js_temporal_to_duration(ctx, argv[1]);
    if (JS_IsException(two))
        goto done;
    options = js_temporal_get_options(ctx, argc > 2 ? argv[2] : JS_UNDEFINED);
    if (JS_IsException(options))
        goto done;
    ret = js_temporal_get_relative_to(ctx, options, &relative);
    JS_FreeValue(ctx, options);
    if (ret)
        goto done;
    a = JS_GetOpaque(one, JS_CLASS_TEMPORAL_DURATION);
    b = JS_GetOpaque(two, JS_CLASS_TEMPORAL_DURATION);
    for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++) {
        if (a->fields[i] != b->fields[i])
            break;
    }
    if (i == QJS_TEMPORAL_UNIT_COUNT) {
        result = JS_NewInt32(ctx, 0);
        goto done;
    }
    first_unit = qjs_temporal_duration_largest_unit(a);
    second_unit = qjs_temporal_duration_largest_unit(b);
    if (qjs_temporal_duration_normalize(&first, a, 0) ||
        qjs_temporal_duration_normalize(&second, b, 0))
        goto overflow;
    zoned = JS_GetOpaque(relative.zoned, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (zoned && (first_unit <= QJS_TEMPORAL_DAY ||
                  second_unit <= QJS_TEMPORAL_DAY)) {
        if (js_temporal_zoned_add_duration(ctx, zoned, first,
                                           QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &after_one) ||
            js_temporal_zoned_add_duration(ctx, zoned, second,
                                           QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &after_two))
            goto done;
        comparison = qjs_temporal_epoch_ns_compare(after_one, after_two);
    } else {
        days_one = first.date.days;
        days_two = second.date.days;
        if (first_unit < QJS_TEMPORAL_DAY || second_unit < QJS_TEMPORAL_DAY) {
            plain = JS_GetOpaque(relative.plain, JS_CLASS_TEMPORAL_PLAIN_DATE);
            if (!plain) {
                JS_ThrowRangeError(ctx, "calendar duration comparison needs relativeTo");
                goto done;
            }
            if (js_temporal_duration_calendar_days(ctx, first.date, plain,
                                                    &days_one) ||
                js_temporal_duration_calendar_days(ctx, second.date, plain,
                                                    &days_two))
                goto done;
        }
        if (qjs_temporal_time_duration_add_days(&first.time, first.time,
                                                 days_one) ||
            qjs_temporal_time_duration_add_days(&second.time, second.time,
                                                 days_two))
            goto overflow;
        comparison = qjs_temporal_epoch_ns_compare(first.time, second.time);
    }
    result = JS_NewInt32(ctx, comparison);
    goto done;
 overflow:
    JS_ThrowRangeError(ctx, "Temporal duration comparison overflow");
 done:
    JS_FreeValue(ctx, one);
    JS_FreeValue(ctx, two);
    js_temporal_free_relative_to(ctx, &relative);
    return result;
}

static JSValue js_temporal_duration_string_options(JSContext *ctx,
                                                   JSValueConst value,
                                                   const char *property)
{
    JSValue options;

    if (JS_IsUndefined(value))
        return JS_ThrowTypeError(ctx, "Temporal duration options required");
    if (!JS_IsString(value))
        return js_temporal_get_options(ctx, value);
    options = JS_NewObjectProto(ctx, JS_NULL);
    if (JS_IsException(options))
        return options;
    if (JS_DefinePropertyValueStr(ctx, options, property,
                                  JS_DupValue(ctx, value), JS_PROP_C_W_E) < 0) {
        JS_FreeValue(ctx, options);
        return JS_EXCEPTION;
    }
    return options;
}

/* A plain relative date is midnight in its calendar. Move fixed time into
   days first, then use calendar date addition; never treat a zoned day as
   24 hours. The calendar and zone owners provide the complete differences. */
static int js_temporal_duration_plain_target(JSContext *ctx,
                                             JSTemporalPlainDateData *relative,
                                             QJSTemporalInternalDuration value,
                                             QJSTemporalISODateTime *start,
                                             QJSTemporalISODateTime *target)
{
    QJSTemporalISOTime midnight = { 0 };
    int64_t days;

    start->date = relative->date;
    start->time = midnight;
    if (qjs_temporal_time_add(&target->time, &days, midnight, value.time)) {
        JS_ThrowRangeError(ctx, "Temporal duration target overflow");
        return -1;
    }
    value.date.days = days;
    return js_temporal_calendar_date_add(ctx, relative->calendar, relative->date,
                                          value.date, QJS_TEMPORAL_OVERFLOW_CONSTRAIN,
                                          &target->date);
}

static int js_temporal_duration_relative_round(
                                               JSContext *ctx, const QJSTemporalDuration *duration,
                                               JSTemporalRelativeTo *relative, QJSTemporalDifferenceSettings *settings,
                                               QJSTemporalInternalDuration *result)
{
    JSTemporalZonedDateTimeData *zoned;
    JSTemporalPlainDateData *plain;
    QJSTemporalEpochNs target_epoch;
    QJSTemporalISODateTime start, target;
    QJSTemporalInternalDuration value;

    zoned = JS_GetOpaque(relative->zoned, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (zoned) {
        if (qjs_temporal_duration_normalize(&value, duration, 0))
            goto overflow;
        if (js_temporal_zoned_add_duration(ctx, zoned, value,
                                           QJS_TEMPORAL_OVERFLOW_CONSTRAIN,
                                           &target_epoch) ||
            js_temporal_zoned_difference_round(ctx, zoned->epoch_nanoseconds,
                target_epoch, &zoned->time_zone, zoned->calendar, settings,
                result))
            return -1;
        if (settings->largest_unit <= QJS_TEMPORAL_DAY)
            settings->largest_unit = QJS_TEMPORAL_HOUR;
        return 0;
    }
    plain = JS_GetOpaque(relative->plain, JS_CLASS_TEMPORAL_PLAIN_DATE);
    if (qjs_temporal_duration_normalize(&value, duration, 1))
        goto overflow;
    if (plain) {
        if (js_temporal_duration_plain_target(ctx, plain, value, &start, &target))
            return -1;
        return js_temporal_plain_datetime_difference_round(ctx, start, target,
            plain->calendar, settings, result);
    }
    if (qjs_temporal_duration_largest_unit(duration) < QJS_TEMPORAL_DAY ||
        settings->largest_unit < QJS_TEMPORAL_DAY) {
        JS_ThrowRangeError(ctx, "calendar duration rounding needs relativeTo");
        return -1;
    }
    if (qjs_temporal_time_duration_round(&value.time, value.time,
        settings->rounding_increment, settings->smallest_unit,
        settings->rounding_mode))
        goto overflow;
    *result = value;
    return 0;
 overflow:
    JS_ThrowRangeError(ctx, "Temporal duration rounding overflow");
    return -1;
}

static JSValue js_temporal_duration_round(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv)
{
    JSTemporalDurationData *data;
    JSTemporalRelativeTo relative = { JS_UNDEFINED, JS_UNDEFINED };
    QJSTemporalDifferenceSettings settings;
    QJSTemporalInternalDuration internal;
    QJSTemporalDuration result;
    QJSTemporalUnit existing, fallback;
    JSValue options, output = JS_EXCEPTION;
    int largest_present, smallest_present;
    uint64_t maximum;

    data = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_DURATION);
    if (!data)
        return JS_EXCEPTION;
    options = js_temporal_duration_string_options(ctx, argv[0], "smallestUnit");
    if (JS_IsException(options))
        return options;
    if (js_temporal_get_unit_option(ctx, options, "largestUnit", FALSE,
                                    &settings.largest_unit) ||
        js_temporal_get_relative_to(ctx, options, &relative) ||
        js_temporal_get_rounding_increment(ctx, options,
                                           &settings.rounding_increment) ||
        js_temporal_get_rounding_mode(ctx, options,
                                      QJS_TEMPORAL_ROUND_HALF_EXPAND,
                                      &settings.rounding_mode) ||
        js_temporal_get_unit_option(ctx, options, "smallestUnit", FALSE,
                                    &settings.smallest_unit))
        goto done;
    if (settings.smallest_unit != QJS_TEMPORAL_UNIT_UNSET &&
        (unsigned)settings.smallest_unit > QJS_TEMPORAL_NANOSECOND)
        goto invalid;
    largest_present = settings.largest_unit != QJS_TEMPORAL_UNIT_UNSET;
    smallest_present = settings.smallest_unit != QJS_TEMPORAL_UNIT_UNSET;
    if (!smallest_present)
        settings.smallest_unit = QJS_TEMPORAL_NANOSECOND;
    existing = qjs_temporal_duration_largest_unit(data);
    fallback = existing < settings.smallest_unit ? existing : settings.smallest_unit;
    if (!largest_present || settings.largest_unit == QJS_TEMPORAL_UNIT_AUTO)
        settings.largest_unit = fallback;
    if ((!largest_present && !smallest_present) ||
        settings.largest_unit > settings.smallest_unit)
        goto invalid;
    maximum = settings.smallest_unit == QJS_TEMPORAL_HOUR ? 24 :
        settings.smallest_unit <= QJS_TEMPORAL_SECOND ? 60 : 1000;
    if (settings.smallest_unit >= QJS_TEMPORAL_HOUR &&
        js_temporal_validate_increment(ctx, settings.rounding_increment,
                                       maximum, FALSE))
        goto done;
    if (settings.rounding_increment > 1 &&
        settings.largest_unit != settings.smallest_unit &&
        settings.smallest_unit <= QJS_TEMPORAL_DAY)
        goto invalid;
    if (js_temporal_duration_relative_round(ctx, data, &relative, &settings,
                                            &internal))
        goto done;
    if (qjs_temporal_duration_from_internal(&result, internal,
                                             settings.largest_unit)) {
        JS_ThrowRangeError(ctx, "rounded Temporal duration is out of range");
        goto done;
    }
    output = js_temporal_create_duration(ctx, JS_UNDEFINED, &result);
    goto done;
 invalid:
    JS_ThrowRangeError(ctx, "invalid Temporal duration rounding options");
 done:
    JS_FreeValue(ctx, options);
    js_temporal_free_relative_to(ctx, &relative);
    return output;
}

static JSValue js_temporal_duration_total(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv)
{
    JSTemporalDurationData *data;
    JSTemporalZonedDateTimeData *zoned;
    JSTemporalPlainDateData *plain;
    JSTemporalRelativeTo relative = { JS_UNDEFINED, JS_UNDEFINED };
    QJSTemporalInternalDuration value;
    QJSTemporalEpochNs target_epoch;
    QJSTemporalISODateTime start, target;
    QJSTemporalUnit unit;
    JSValue options, output = JS_EXCEPTION;
    double total;

    data = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_DURATION);
    if (!data)
        return JS_EXCEPTION;
    options = js_temporal_duration_string_options(ctx, argv[0], "unit");
    if (JS_IsException(options))
        return options;
    if (js_temporal_get_relative_to(ctx, options, &relative) ||
        js_temporal_get_unit_option(ctx, options, "unit", TRUE, &unit))
        goto done;
    if ((unsigned)unit > QJS_TEMPORAL_NANOSECOND) {
        JS_ThrowRangeError(ctx, "invalid Temporal duration total unit");
        goto done;
    }
    zoned = JS_GetOpaque(relative.zoned, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    plain = JS_GetOpaque(relative.plain, JS_CLASS_TEMPORAL_PLAIN_DATE);
    if (qjs_temporal_duration_normalize(&value, data, zoned ? 0 : 1))
        goto overflow;
    if (zoned) {
        if (js_temporal_zoned_add_duration(ctx, zoned, value,
                                           QJS_TEMPORAL_OVERFLOW_CONSTRAIN,
                                           &target_epoch) ||
            js_temporal_zoned_difference_total(ctx, zoned->epoch_nanoseconds,
                target_epoch, &zoned->time_zone, zoned->calendar, unit, &total))
            goto done;
    } else if (plain) {
        if (js_temporal_duration_plain_target(ctx, plain, value, &start, &target) ||
            js_temporal_plain_datetime_difference_total(ctx, start, target,
                                                         plain->calendar,
                                                         unit, &total))
            goto done;
    } else {
        if (qjs_temporal_duration_largest_unit(data) < QJS_TEMPORAL_DAY ||
            unit < QJS_TEMPORAL_DAY) {
            JS_ThrowRangeError(ctx, "calendar duration total needs relativeTo");
            goto done;
        }
        total = qjs_temporal_ratio_to_double(value.time,
            qjs_temporal_epoch_ns_from_int64(
                (int64_t)qjs_temporal_unit_nanoseconds(unit)));
    }
    output = JS_NewFloat64(ctx, total);
    goto done;
 overflow:
    JS_ThrowRangeError(ctx, "Temporal duration total overflow");
 done:
    JS_FreeValue(ctx, options);
    js_temporal_free_relative_to(ctx, &relative);
    return output;
}

static JSValue js_temporal_duration_to_string(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv,
                                              int kind)
{
    JSTemporalDurationData *data;
    QJSTemporalDuration rounded;
    QJSTemporalInternalDuration internal;
    QJSTemporalStringPrecision precision;
    QJSTemporalRoundingMode mode;
    QJSTemporalUnit unit, largest;
    JSValue options;
    char buffer[512];
    int digits, ret;

    data = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_DURATION);
    if (!data)
        return JS_EXCEPTION;
#ifdef CONFIG_INTL
    if (kind == 2)
        return js_intl_temporal_to_locale_string(ctx, this_val,
            argc > 0 ? argv[0] : JS_UNDEFINED,
            argc > 1 ? argv[1] : JS_UNDEFINED);
#endif
    rounded = *data;
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
            (unit < QJS_TEMPORAL_SECOND || unit > QJS_TEMPORAL_NANOSECOND))
            return JS_ThrowRangeError(ctx, "invalid Temporal duration string unit");
        if (qjs_temporal_string_precision(&precision, unit, digits))
            goto overflow;
        if (precision.unit != QJS_TEMPORAL_NANOSECOND || precision.increment != 1) {
            largest = qjs_temporal_duration_largest_unit(data);
            if (largest > QJS_TEMPORAL_SECOND)
                largest = QJS_TEMPORAL_SECOND;
            if (qjs_temporal_duration_normalize(&internal, data, 0) ||
                qjs_temporal_time_duration_round(&internal.time, internal.time,
                    precision.increment, precision.unit, mode) ||
                qjs_temporal_duration_from_internal(&rounded, internal, largest))
                goto overflow;
        }
    }
    ret = qjs_temporal_format_duration(buffer, sizeof(buffer), &rounded,
                                        precision.precision);
    if (ret < 0)
        goto overflow;
    return JS_NewStringLen(ctx, buffer, ret);
 overflow:
    return JS_ThrowRangeError(ctx, "Temporal duration formatting overflow");
}

static JSValue js_temporal_duration_value_of(JSContext *ctx,
                                             JSValueConst this_val,
                                             int argc, JSValueConst *argv)
{
    return JS_ThrowTypeError(ctx, "Temporal.Duration cannot become a number");
}

static const JSCFunctionListEntry js_temporal_duration_funcs[] = {
    JS_CFUNC_DEF("from", 1, js_temporal_duration_from),
    JS_CFUNC_DEF("compare", 2, js_temporal_duration_compare),
};

static const JSCFunctionListEntry js_temporal_duration_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("years", js_temporal_duration_get, NULL, 0),
    JS_CGETSET_MAGIC_DEF("months", js_temporal_duration_get, NULL, 1),
    JS_CGETSET_MAGIC_DEF("weeks", js_temporal_duration_get, NULL, 2),
    JS_CGETSET_MAGIC_DEF("days", js_temporal_duration_get, NULL, 3),
    JS_CGETSET_MAGIC_DEF("hours", js_temporal_duration_get, NULL, 4),
    JS_CGETSET_MAGIC_DEF("minutes", js_temporal_duration_get, NULL, 5),
    JS_CGETSET_MAGIC_DEF("seconds", js_temporal_duration_get, NULL, 6),
    JS_CGETSET_MAGIC_DEF("milliseconds", js_temporal_duration_get, NULL, 7),
    JS_CGETSET_MAGIC_DEF("microseconds", js_temporal_duration_get, NULL, 8),
    JS_CGETSET_MAGIC_DEF("nanoseconds", js_temporal_duration_get, NULL, 9),
    JS_CGETSET_MAGIC_DEF("sign", js_temporal_duration_get, NULL,
                        QJS_TEMPORAL_UNIT_COUNT),
    JS_CGETSET_MAGIC_DEF("blank", js_temporal_duration_get, NULL,
                        QJS_TEMPORAL_UNIT_COUNT + 1),
    JS_CFUNC_DEF("with", 1, js_temporal_duration_with),
    JS_CFUNC_MAGIC_DEF("negated", 0, js_temporal_duration_negated, 0),
    JS_CFUNC_MAGIC_DEF("abs", 0, js_temporal_duration_negated, 1),
    JS_CFUNC_MAGIC_DEF("add", 1, js_temporal_duration_add, 0),
    JS_CFUNC_MAGIC_DEF("subtract", 1, js_temporal_duration_add, 1),
    JS_CFUNC_DEF("round", 1, js_temporal_duration_round),
    JS_CFUNC_DEF("total", 1, js_temporal_duration_total),
    JS_CFUNC_MAGIC_DEF("toString", 0, js_temporal_duration_to_string, 0),
    JS_CFUNC_MAGIC_DEF("toJSON", 0, js_temporal_duration_to_string, 1),
    JS_CFUNC_MAGIC_DEF("toLocaleString", 0, js_temporal_duration_to_string, 2),
    JS_CFUNC_DEF("valueOf", 0, js_temporal_duration_value_of),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Temporal.Duration",
                       JS_PROP_CONFIGURABLE),
};

int js_temporal_init_duration(JSContext *ctx, JSValueConst namespace_object)
{
    JSValue constructor;
    int ret;

    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_TEMPORAL_DURATION) &&
        JS_NewClass(ctx->rt, JS_CLASS_TEMPORAL_DURATION,
                     &js_temporal_duration_class) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    constructor = JS_NewCConstructor(ctx, JS_CLASS_TEMPORAL_DURATION,
        "Duration", js_temporal_duration_constructor, 0,
        JS_CFUNC_constructor, 0, JS_UNDEFINED,
        js_temporal_duration_funcs, countof(js_temporal_duration_funcs),
        js_temporal_duration_proto_funcs,
        countof(js_temporal_duration_proto_funcs), JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(constructor) || JS_HasException(ctx)) {
        JS_FreeValue(ctx, constructor);
        return -1;
    }
    ret = JS_DefinePropertyValueStr(ctx, namespace_object, "Duration", constructor,
                                     JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    return ret < 0 ? -1 : 0;
}
