/* Native Temporal Plain civil objects.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, as in instant.c.
 */
#include "calendar-fields.h"
#include "../../internal/object.h"
#include "../../../temporal/civil.h"
#include "../../../temporal/relative.h"
#include "../../../temporal/time.h"
#include "../../../temporal/duration.h"
#include <limits.h>
#include <string.h>
#include <stdio.h>

enum { PLAIN_DATE, PLAIN_DATETIME, PLAIN_YEAR_MONTH, PLAIN_MONTH_DAY };
static const JSClassID plain_classes[] = {
    JS_CLASS_TEMPORAL_PLAIN_DATE, JS_CLASS_TEMPORAL_PLAIN_DATE_TIME,
    JS_CLASS_TEMPORAL_PLAIN_YEAR_MONTH, JS_CLASS_TEMPORAL_PLAIN_MONTH_DAY
};
static const char *const plain_names[] = {
    "PlainDate", "PlainDateTime", "PlainYearMonth", "PlainMonthDay"
};
static const char *const overflow_names[] = { "constrain", "reject" };
static const char *const calendar_names[] = {
    "auto", "always", "never", "critical"
};

typedef struct PlainValue {
    QJSTemporalISODateTime datetime;
    QJSTemporalCalendar calendar;
} PlainValue;

static JSValueConst argument(int argc, JSValueConst *argv, int index)
{
    return index < argc ? argv[index] : JS_UNDEFINED;
}

static int get_plain(JSContext *ctx, JSValueConst value, int kind,
                       PlainValue *result, BOOL throwing)
{
    void *opaque = throwing ? JS_GetOpaque2(ctx, value, plain_classes[kind]) :
                              JS_GetOpaque(value, plain_classes[kind]);
    if (!opaque)
        return -1;
    if (kind == PLAIN_DATETIME) {
        JSTemporalPlainDateTimeData *data = opaque;
        result->datetime = data->datetime;
        result->calendar = data->calendar;
    } else {
        JSTemporalPlainDateData *data = opaque;
        result->datetime.date = data->date;
        result->datetime.time = (QJSTemporalISOTime){ 0, 0, 0, 0, 0, 0 };
        result->calendar = data->calendar;
    }
    return 0;
}

static JSValue create_plain(JSContext *ctx, JSValueConst new_target,
                             int kind, PlainValue value)
{
    JSValue object;
    void *opaque;
    BOOL valid;
    if (kind == PLAIN_DATETIME)
        valid = qjs_temporal_iso_datetime_within_limits(value.datetime);
    else if (kind == PLAIN_YEAR_MONTH)
        valid = qjs_temporal_iso_year_month_within_limits(value.datetime.date);
    else
        valid = qjs_temporal_iso_date_within_limits(value.datetime.date);
    if (!valid)
        return JS_ThrowRangeError(ctx, "Temporal civil value outside range");
    object = js_temporal_create_from_ctor(ctx, new_target, plain_classes[kind]);
    if (JS_IsException(object))
        return object;
    if (kind == PLAIN_DATETIME) {
        JSTemporalPlainDateTimeData *data = js_malloc(ctx, sizeof(*data));
        opaque = data;
        if (data) {
            data->datetime = value.datetime;
            data->calendar = value.calendar;
        }
    } else {
        JSTemporalPlainDateData *data = js_malloc(ctx, sizeof(*data));
        opaque = data;
        if (data) {
            data->date = value.datetime.date;
            data->calendar = value.calendar;
        }
    }
    if (!opaque) {
        JS_FreeValue(ctx, object);
        return JS_EXCEPTION;
    }
    JS_SetOpaque(object, opaque);
    return object;
}

JSValue js_temporal_create_plain_date(JSContext *ctx, JSValueConst target,
                                      QJSTemporalISODate date,
                                      QJSTemporalCalendar calendar)
{
    PlainValue value = { { date, { 0, 0, 0, 0, 0, 0 } }, calendar };
    return create_plain(ctx, target, PLAIN_DATE, value);
}

JSValue js_temporal_create_plain_date_time(JSContext *ctx, JSValueConst target,
                                          QJSTemporalISODateTime datetime,
                                          QJSTemporalCalendar calendar)
{
    PlainValue value = { datetime, calendar };
    return create_plain(ctx, target, PLAIN_DATETIME, value);
}

JSValue js_temporal_create_plain_year_month(JSContext *ctx, JSValueConst target,
                                           QJSTemporalISODate date,
                                           QJSTemporalCalendar calendar)
{
    PlainValue value = { { date, { 0, 0, 0, 0, 0, 0 } }, calendar };
    return create_plain(ctx, target, PLAIN_YEAR_MONTH, value);
}

JSValue js_temporal_create_plain_month_day(JSContext *ctx, JSValueConst target,
                                          QJSTemporalISODate date,
                                          QJSTemporalCalendar calendar)
{
    PlainValue value = { { date, { 0, 0, 0, 0, 0, 0 } }, calendar };
    return create_plain(ctx, target, PLAIN_MONTH_DAY, value);
}

static int get_overflow(JSContext *ctx, JSValueConst options,
                         QJSTemporalOverflow *result)
{
    JSValue resolved = js_temporal_get_options(ctx, options);
    int value, error;
    if (JS_IsException(resolved))
        return -1;
    error = js_temporal_get_string_option(ctx, resolved, "overflow",
                                    overflow_names, 2, 0, &value);
    JS_FreeValue(ctx, resolved);
    if (error)
        return -1;
    *result = (QJSTemporalOverflow)value;
    return 0;
}

static int regulate_time(JSContext *ctx, const double values[6],
                           QJSTemporalOverflow overflow,
                           QJSTemporalISOTime *result)
{
    const int maxima[] = { 23, 59, 59, 999, 999, 999 };
    int32_t out[6];
    int i;
    for (i = 0; i < 6; i++) {
        double value = values[i];
        if (overflow == QJS_TEMPORAL_OVERFLOW_CONSTRAIN) {
            if (value < 0) value = 0;
            if (value > maxima[i]) value = maxima[i];
        } else if (value < 0 || value > maxima[i]) {
            JS_ThrowRangeError(ctx, "invalid Temporal clock field");
            return -1;
        }
        out[i] = (int32_t)value;
    }
    *result = (QJSTemporalISOTime){ out[0], out[1], out[2], out[3],
                                  out[4], out[5] };
    return 0;
}

static unsigned date_field_names(int kind)
{
    return kind == PLAIN_YEAR_MONTH ?
           QJS_TEMPORAL_DATE_FIELDS & ~QJS_TEMPORAL_FIELD_DAY :
           QJS_TEMPORAL_DATE_FIELDS;
}

static void restrict_fields(JSTemporalFields *fields, int kind)
{
    /* ISODateToFields deliberately drops ordinal month for all modes. */
    fields->calendar.present &= ~(QJS_TEMPORAL_FIELD_MONTH |
        QJS_TEMPORAL_FIELD_ERA | QJS_TEMPORAL_FIELD_ERA_YEAR);
    fields->present &= ~(QJS_TEMPORAL_FIELD_MONTH |
        QJS_TEMPORAL_FIELD_ERA | QJS_TEMPORAL_FIELD_ERA_YEAR);
    if (kind == PLAIN_YEAR_MONTH) {
        fields->calendar.present &= ~QJS_TEMPORAL_FIELD_DAY;
        fields->present &= ~QJS_TEMPORAL_FIELD_DAY;
    } else if (kind == PLAIN_MONTH_DAY) {
        fields->calendar.present &= ~(QJS_TEMPORAL_FIELD_YEAR |
            QJS_TEMPORAL_FIELD_ERA | QJS_TEMPORAL_FIELD_ERA_YEAR);
        fields->present &= ~(QJS_TEMPORAL_FIELD_YEAR |
            QJS_TEMPORAL_FIELD_ERA | QJS_TEMPORAL_FIELD_ERA_YEAR);
    }
}

static int resolve_fields(JSContext *ctx, int kind,
                            QJSTemporalCalendar calendar,
                            const JSTemporalFields *fields,
                            QJSTemporalOverflow overflow, PlainValue *result)
{
    int error;
    if (kind == PLAIN_YEAR_MONTH)
        error = qjs_temporal_calendar_year_month_from_fields(calendar,
                      &fields->calendar, overflow, &result->datetime.date);
    else if (kind == PLAIN_MONTH_DAY)
        error = qjs_temporal_calendar_month_day_from_fields(calendar,
                      &fields->calendar, overflow, &result->datetime.date);
    else
        error = qjs_temporal_calendar_date_from_fields(calendar,
                      &fields->calendar, overflow, &result->datetime.date);
    if (js_temporal_calendar_error(ctx, error))
        return -1;
    result->datetime.time = (QJSTemporalISOTime){ 0, 0, 0, 0, 0, 0 };
    if (kind == PLAIN_DATETIME &&
        regulate_time(ctx, fields->time, overflow, &result->datetime.time))
        return -1;
    result->calendar = calendar;
    return 0;
}

int js_temporal_interpret_datetime_fields(JSContext *ctx,
    QJSTemporalCalendar calendar, const JSTemporalFields *fields,
    QJSTemporalOverflow overflow, QJSTemporalISODateTime *result)
{
    PlainValue value;
    if (resolve_fields(ctx, PLAIN_DATETIME, calendar, fields, overflow, &value))
        return -1;
    *result = value.datetime;
    return 0;
}

static JSValue to_plain(JSContext *ctx, JSValueConst item,
                         JSValueConst options, int kind)
{
    PlainValue value;
    QJSTemporalOverflow overflow;
    QJSTemporalParsedISO parsed;
    JSTemporalFields fields;
    JSValue calendar_string;
    const char *text;
    size_t length;
    int error, copied = 0;
    if (JS_IsObject(item)) {
        if (get_plain(ctx, item, kind, &value, FALSE) == 0) {
            copied = 1;
        } else if (kind == PLAIN_DATE || kind == PLAIN_DATETIME) {
            if (get_plain(ctx, item,
                    kind == PLAIN_DATE ? PLAIN_DATETIME : PLAIN_DATE,
                    &value, FALSE) == 0) {
                copied = 1;
            } else {
                JSTemporalZonedDateTimeData *zoned = JS_GetOpaque(item,
                                         JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
                if (zoned) {
                    if (js_temporal_time_zone_datetime(ctx, &zoned->time_zone,
                           zoned->epoch_nanoseconds, &value.datetime))
                        return JS_EXCEPTION;
                    value.calendar = zoned->calendar;
                    copied = 1;
                }
            }
        }
        if (copied) {
            if (get_overflow(ctx, options, &overflow))
                return JS_EXCEPTION;
            return create_plain(ctx, JS_UNDEFINED, kind, value);
        }
        if (js_temporal_get_calendar(ctx, item, &value.calendar))
            return JS_EXCEPTION;
        error = js_temporal_prepare_calendar_fields(ctx, value.calendar,
                     item, date_field_names(kind),
                     kind == PLAIN_DATETIME ? QJS_TEMPORAL_TIME_FIELDS : 0,
                     0, FALSE, &fields);
        if (!error)
            error = get_overflow(ctx, options, &overflow);
        if (!error)
            error = resolve_fields(ctx, kind, value.calendar,
                                     &fields, overflow, &value);
        js_temporal_fields_free(ctx, &fields);
        if (error)
            return JS_EXCEPTION;
        return create_plain(ctx, JS_UNDEFINED, kind, value);
    }
    if (!JS_IsString(item))
        return JS_ThrowTypeError(ctx, "Temporal civil input must be a string");
    text = JS_ToCStringLen(ctx, &length, item);
    if (!text)
        return JS_EXCEPTION;
    error = qjs_temporal_parse_iso_datetime(&parsed, text, length,
               kind == PLAIN_YEAR_MONTH ? QJS_TEMPORAL_PARSE_YEAR_MONTH :
               kind == PLAIN_MONTH_DAY ? QJS_TEMPORAL_PARSE_MONTH_DAY :
                                        QJS_TEMPORAL_PARSE_DATE_TIME);
    if (error) {
        JS_FreeCString(ctx, text);
        return JS_ThrowRangeError(ctx, "invalid Temporal civil string");
    }
    value.datetime = parsed.datetime;
    calendar_string = parsed.calendar ?
        JS_NewStringLen(ctx, parsed.calendar, parsed.calendar_length) :
        JS_NewString(ctx, "iso8601");
    JS_FreeCString(ctx, text);
    if (JS_IsException(calendar_string))
        return JS_EXCEPTION;
    error = js_temporal_to_calendar(ctx, calendar_string, &value.calendar);
    JS_FreeValue(ctx, calendar_string);
    if (error || get_overflow(ctx, options, &overflow))
        return JS_EXCEPTION;
    if (kind == PLAIN_MONTH_DAY &&
        value.calendar == QJS_TEMPORAL_CAL_ISO8601) {
        value.datetime.date.year = 1972;
    } else if (kind == PLAIN_YEAR_MONTH || kind == PLAIN_MONTH_DAY) {
        BOOL valid = kind == PLAIN_YEAR_MONTH ?
            qjs_temporal_iso_year_month_within_limits(value.datetime.date) :
            qjs_temporal_iso_date_within_limits(value.datetime.date);
        if (!valid)
            return JS_ThrowRangeError(ctx, "Temporal reference outside range");
        error = js_temporal_iso_date_to_fields(ctx, value.calendar,
                                              value.datetime.date, &fields);
        if (!error) {
            restrict_fields(&fields, kind);
            error = resolve_fields(ctx, kind, value.calendar, &fields,
                         QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &value);
        }
        js_temporal_fields_free(ctx, &fields);
        if (error)
            return JS_EXCEPTION;
    }
    return create_plain(ctx, JS_UNDEFINED, kind, value);
}

JSValue js_temporal_to_plain_date(JSContext *ctx, JSValueConst value,
                                 JSValueConst options)
{ return to_plain(ctx, value, options, PLAIN_DATE); }
JSValue js_temporal_to_plain_date_time(JSContext *ctx, JSValueConst value,
                                      JSValueConst options)
{ return to_plain(ctx, value, options, PLAIN_DATETIME); }
JSValue js_temporal_to_plain_year_month(JSContext *ctx, JSValueConst value,
                                       JSValueConst options)
{ return to_plain(ctx, value, options, PLAIN_YEAR_MONTH); }
JSValue js_temporal_to_plain_month_day(JSContext *ctx, JSValueConst value,
                                      JSValueConst options)
{ return to_plain(ctx, value, options, PLAIN_MONTH_DAY); }

static JSValue plain_constructor(JSContext *ctx, JSValueConst new_target,
                                  int argc, JSValueConst *argv, int kind)
{
    PlainValue value = { { { 1972, 1, 1 }, { 0, 0, 0, 0, 0, 0 } },
                          QJS_TEMPORAL_CAL_ISO8601 };
    double date[3] = { 1972, 1, 1 }, time[6] = { 0, 0, 0, 0, 0, 0 };
    JSValueConst calendar;
    int i, count = kind == PLAIN_DATETIME || kind == PLAIN_DATE ? 3 : 2;
    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Temporal constructor requires new");
    for (i = 0; i < count; i++) {
        int slot = kind == PLAIN_MONTH_DAY ? i + 1 : i;
        if (js_temporal_to_integer(ctx, argument(argc, argv, i), &date[slot]))
            return JS_EXCEPTION;
    }
    if (kind == PLAIN_DATETIME) {
        for (i = 0; i < 6; i++) {
            JSValueConst input = argument(argc, argv, i + 3);
            if (!JS_IsUndefined(input) &&
                js_temporal_to_integer(ctx, input, &time[i]))
                return JS_EXCEPTION;
        }
    }
    calendar = argument(argc, argv, kind == PLAIN_DATETIME ? 9 :
                                   kind == PLAIN_DATE ? 3 : 2);
    if (!JS_IsUndefined(calendar)) {
        if (!JS_IsString(calendar))
            return JS_ThrowTypeError(ctx, "calendar must be a string");
        if (js_temporal_to_calendar(ctx, calendar, &value.calendar))
            return JS_EXCEPTION;
    }
    if (kind == PLAIN_YEAR_MONTH || kind == PLAIN_MONTH_DAY) {
        JSValueConst reference = argument(argc, argv, 3);
        if (kind == PLAIN_YEAR_MONTH)
            date[2] = 1;
        if (!JS_IsUndefined(reference) &&
            js_temporal_to_integer(ctx, reference,
                 &date[kind == PLAIN_YEAR_MONTH ? 2 : 0]))
            return JS_EXCEPTION;
    }
    if (date[0] < INT32_MIN || date[0] > INT32_MAX ||
        date[1] < 1 || date[1] > 12 || date[2] < 1 || date[2] > 31)
        return JS_ThrowRangeError(ctx, "invalid Temporal ISO date");
    value.datetime.date = (QJSTemporalISODate){
        (int32_t)date[0], (int32_t)date[1], (int32_t)date[2] };
    if (!qjs_temporal_iso_date_is_valid(value.datetime.date))
        return JS_ThrowRangeError(ctx, "invalid Temporal ISO date");
    if (regulate_time(ctx, time, QJS_TEMPORAL_OVERFLOW_REJECT,
                       &value.datetime.time))
        return JS_EXCEPTION;
    return create_plain(ctx, new_target, kind, value);
}

static JSValue plain_from(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv, int kind)
{
    return to_plain(ctx, argument(argc, argv, 0), argument(argc, argv, 1), kind);
}

static JSValue plain_compare(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int kind)
{
    JSValue one, two;
    PlainValue a, b;
    int comparison;
    one = to_plain(ctx, argument(argc, argv, 0), JS_UNDEFINED, kind);
    if (JS_IsException(one))
        return one;
    two = to_plain(ctx, argument(argc, argv, 1), JS_UNDEFINED, kind);
    if (JS_IsException(two)) {
        JS_FreeValue(ctx, one);
        return two;
    }
    if (get_plain(ctx, one, kind, &a, TRUE) ||
        get_plain(ctx, two, kind, &b, TRUE)) {
        JS_FreeValue(ctx, one);
        JS_FreeValue(ctx, two);
        return JS_EXCEPTION;
    }
    comparison = kind == PLAIN_DATETIME ?
        qjs_temporal_iso_datetime_compare(a.datetime, b.datetime) :
        qjs_temporal_iso_date_compare(a.datetime.date, b.datetime.date);
    JS_FreeValue(ctx, one);
    JS_FreeValue(ctx, two);
    return JS_NewInt32(ctx, comparison);
}

enum { GET_CALENDAR, GET_ERA, GET_ERA_YEAR, GET_YEAR, GET_MONTH,
    GET_MONTH_CODE, GET_DAY, GET_DAY_OF_WEEK, GET_DAY_OF_YEAR, GET_WEEK,
    GET_WEEK_YEAR, GET_DAYS_IN_WEEK, GET_DAYS_IN_MONTH, GET_DAYS_IN_YEAR,
    GET_MONTHS_IN_YEAR, GET_LEAP, GET_HOUR, GET_MINUTE, GET_SECOND,
    GET_MILLISECOND, GET_MICROSECOND, GET_NANOSECOND };

static JSValue plain_getter(JSContext *ctx, JSValueConst this_val, int magic)
{
    PlainValue value;
    QJSTemporalCalendarDate fields;
    int kind = magic >> 8, field = magic & 255;
    if (get_plain(ctx, this_val, kind, &value, TRUE))
        return JS_EXCEPTION;
    if (field == GET_CALENDAR)
        return JS_NewString(ctx,
                             qjs_temporal_calendar_identifier(value.calendar));
    if (field >= GET_HOUR) {
        const int32_t time[] = { value.datetime.time.hour,
            value.datetime.time.minute, value.datetime.time.second,
            value.datetime.time.millisecond, value.datetime.time.microsecond,
            value.datetime.time.nanosecond };
        return JS_NewInt32(ctx, time[field - GET_HOUR]);
    }
    if (js_temporal_calendar_error(ctx, qjs_temporal_calendar_fields(
                               value.calendar, value.datetime.date, &fields)))
        return JS_EXCEPTION;
    switch (field) {
    case GET_ERA: return fields.has_era ? JS_NewString(ctx, fields.era) :
                                        JS_UNDEFINED;
    case GET_ERA_YEAR: return fields.has_era ? JS_NewInt32(ctx, fields.era_year) :
                                             JS_UNDEFINED;
    case GET_YEAR: return JS_NewInt32(ctx, fields.year);
    case GET_MONTH: return JS_NewInt32(ctx, fields.month);
    case GET_MONTH_CODE: return JS_NewString(ctx, fields.month_code);
    case GET_DAY: return JS_NewInt32(ctx, fields.day);
    case GET_DAY_OF_WEEK: return JS_NewInt32(ctx, fields.day_of_week);
    case GET_DAY_OF_YEAR: return JS_NewInt32(ctx, fields.day_of_year);
    case GET_WEEK: return fields.has_week ? JS_NewInt32(ctx, fields.week_of_year) :
                                           JS_UNDEFINED;
    case GET_WEEK_YEAR: return fields.has_week ?
                        JS_NewInt32(ctx, fields.year_of_week) : JS_UNDEFINED;
    case GET_DAYS_IN_WEEK: return JS_NewInt32(ctx, fields.days_in_week);
    case GET_DAYS_IN_MONTH: return JS_NewInt32(ctx, fields.days_in_month);
    case GET_DAYS_IN_YEAR: return JS_NewInt32(ctx, fields.days_in_year);
    case GET_MONTHS_IN_YEAR: return JS_NewInt32(ctx, fields.months_in_year);
    default: return JS_NewBool(ctx, fields.in_leap_year);
    }
}

static JSValue plain_equals(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int kind)
{
    PlainValue a, b;
    JSValue other;
    int comparison;
    if (get_plain(ctx, this_val, kind, &a, TRUE))
        return JS_EXCEPTION;
    other = to_plain(ctx, argument(argc, argv, 0), JS_UNDEFINED, kind);
    if (JS_IsException(other))
        return other;
    if (get_plain(ctx, other, kind, &b, TRUE)) {
        JS_FreeValue(ctx, other);
        return JS_EXCEPTION;
    }
    comparison = kind == PLAIN_DATETIME ?
        qjs_temporal_iso_datetime_compare(a.datetime, b.datetime) :
        qjs_temporal_iso_date_compare(a.datetime.date, b.datetime.date);
    JS_FreeValue(ctx, other);
    return JS_NewBool(ctx, !comparison && a.calendar == b.calendar);
}

static JSValue plain_with(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv, int kind)
{
    PlainValue value;
    JSTemporalFields original, partial, merged;
    QJSTemporalOverflow overflow;
    int error, i;
    JSValueConst input = argument(argc, argv, 0);
    if (get_plain(ctx, this_val, kind, &value, TRUE))
        return JS_EXCEPTION;
    error = js_temporal_is_partial_object(ctx, input);
    if (error < 0)
        return JS_EXCEPTION;
    if (!error)
        return JS_ThrowTypeError(ctx, "invalid partial Temporal object");
    error = js_temporal_iso_date_to_fields(ctx, value.calendar,
                                           value.datetime.date, &original);
    if (error) {
        js_temporal_fields_free(ctx, &original);
        return JS_EXCEPTION;
    }
    restrict_fields(&original, kind);
    if (kind == PLAIN_DATETIME) {
        const int32_t time[] = { value.datetime.time.hour,
            value.datetime.time.minute, value.datetime.time.second,
            value.datetime.time.millisecond, value.datetime.time.microsecond,
            value.datetime.time.nanosecond };
        for (i = 0; i < 6; i++) original.time[i] = time[i];
        original.present |= QJS_TEMPORAL_TIME_FIELDS;
    }
    error = js_temporal_prepare_calendar_fields(ctx, value.calendar, input,
                date_field_names(kind), kind == PLAIN_DATETIME ?
                QJS_TEMPORAL_TIME_FIELDS : 0, 0, TRUE, &partial);
    if (error) {
        js_temporal_fields_free(ctx, &original);
        js_temporal_fields_free(ctx, &partial);
        return JS_EXCEPTION;
    }
    js_temporal_calendar_merge_fields(ctx, value.calendar,
                                       &original, &partial, &merged);
    error = get_overflow(ctx, argument(argc, argv, 1), &overflow);
    if (!error)
        error = resolve_fields(ctx, kind, value.calendar,
                                 &merged, overflow, &value);
    js_temporal_fields_free(ctx, &original);
    js_temporal_fields_free(ctx, &partial);
    js_temporal_fields_free(ctx, &merged);
    return error ? JS_EXCEPTION : create_plain(ctx, JS_UNDEFINED, kind, value);
}

static JSValue plain_with_calendar(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv, int kind)
{
    PlainValue value;
    if (get_plain(ctx, this_val, kind, &value, TRUE) ||
        js_temporal_to_calendar(ctx, argument(argc, argv, 0), &value.calendar))
        return JS_EXCEPTION;
    return create_plain(ctx, JS_UNDEFINED, kind, value);
}

static int get_time_or_midnight(JSContext *ctx, JSValueConst input,
                                 QJSTemporalISOTime *result)
{
    JSValue time;
    JSTemporalPlainTimeData *data;
    if (JS_IsUndefined(input)) {
        *result = (QJSTemporalISOTime){ 0, 0, 0, 0, 0, 0 };
        return 0;
    }
    time = js_temporal_to_plain_time(ctx, input, JS_UNDEFINED);
    if (JS_IsException(time))
        return -1;
    data = JS_GetOpaque(time, JS_CLASS_TEMPORAL_PLAIN_TIME);
    *result = *data;
    JS_FreeValue(ctx, time);
    return 0;
}

static JSValue plain_with_time(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv, int kind)
{
    PlainValue value;
    if (get_plain(ctx, this_val, kind, &value, TRUE) ||
        get_time_or_midnight(ctx, argument(argc, argv, 0), &value.datetime.time))
        return JS_EXCEPTION;
    return create_plain(ctx, JS_UNDEFINED, PLAIN_DATETIME, value);
}

static JSValue plain_convert(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int magic)
{
    int kind = magic >> 8, target = magic & 255;
    PlainValue value;
    JSTemporalFields fields;
    int error;
    if (get_plain(ctx, this_val, kind, &value, TRUE))
        return JS_EXCEPTION;
    if (target == PLAIN_DATE)
        return create_plain(ctx, JS_UNDEFINED, target, value);
    if (target == 4)
        return js_temporal_create_plain_time(ctx, JS_UNDEFINED,
                                              value.datetime.time);
    error = js_temporal_iso_date_to_fields(ctx, value.calendar,
                                           value.datetime.date, &fields);
    if (!error) {
        restrict_fields(&fields, target);
        error = resolve_fields(ctx, target, value.calendar, &fields,
                                 QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &value);
    }
    js_temporal_fields_free(ctx, &fields);
    return error ? JS_EXCEPTION : create_plain(ctx, JS_UNDEFINED, target, value);
}

static JSValue plain_partial_to_date(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int kind)
{
    PlainValue value;
    JSTemporalFields original, input, merged;
    int error;
    if (get_plain(ctx, this_val, kind, &value, TRUE))
        return JS_EXCEPTION;
    if (!JS_IsObject(argument(argc, argv, 0)))
        return JS_ThrowTypeError(ctx, "toPlainDate needs an object");
    error = js_temporal_iso_date_to_fields(ctx, value.calendar,
                                           value.datetime.date, &original);
    if (error) {
        js_temporal_fields_free(ctx, &original);
        return JS_EXCEPTION;
    }
    restrict_fields(&original, kind);
    error = js_temporal_prepare_calendar_fields(ctx, value.calendar,
                     argument(argc, argv, 0), kind == PLAIN_YEAR_MONTH ?
                     QJS_TEMPORAL_FIELD_DAY : QJS_TEMPORAL_FIELD_YEAR,
                     0, 0, FALSE, &input);
    if (error) {
        js_temporal_fields_free(ctx, &original);
        js_temporal_fields_free(ctx, &input);
        return JS_EXCEPTION;
    }
    js_temporal_calendar_merge_fields(ctx, value.calendar,
                                       &original, &input, &merged);
    error = resolve_fields(ctx, PLAIN_DATE, value.calendar, &merged,
                             QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &value);
    js_temporal_fields_free(ctx, &original);
    js_temporal_fields_free(ctx, &input);
    js_temporal_fields_free(ctx, &merged);
    return error ? JS_EXCEPTION : create_plain(ctx, JS_UNDEFINED,
                                               PLAIN_DATE, value);
}

static JSValue plain_add(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv, int magic)
{
    int kind = magic >> 8, subtract = magic & 255, error, i;
    PlainValue value;
    JSValue duration;
    QJSTemporalDuration input;
    QJSTemporalInternalDuration normalized;
    QJSTemporalOverflow overflow;
    int64_t overflow_days;
    JSTemporalFields fields;
    if (get_plain(ctx, this_val, kind, &value, TRUE))
        return JS_EXCEPTION;
    duration = js_temporal_to_duration(ctx, argument(argc, argv, 0));
    if (JS_IsException(duration))
        return duration;
    input = *(JSTemporalDurationData *)JS_GetOpaque(duration,
                                                    JS_CLASS_TEMPORAL_DURATION);
    JS_FreeValue(ctx, duration);
    if (subtract)
        for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++)
            input.fields[i] = -input.fields[i];
    if (kind != PLAIN_DATETIME &&
        qjs_temporal_duration_normalize(&normalized, &input,
                                        kind == PLAIN_DATE))
        return JS_ThrowRangeError(ctx, "Temporal duration outside range");
    if (kind == PLAIN_DATE) {
        QJSTemporalEpochNs whole_days;
        uint64_t remainder;

        if (qjs_temporal_epoch_ns_divide(&whole_days, &remainder,
                  normalized.time, UINT64_C(86400000000000)) ||
            qjs_temporal_epoch_ns_to_int64(&overflow_days, whole_days))
            return JS_ThrowRangeError(ctx, "Temporal duration outside range");
        /* Division floors; date-only durations truncate towards zero. */
        if ((normalized.time.high >> 63) && remainder)
            overflow_days++;
        normalized.date.days = overflow_days;
    }
    if (get_overflow(ctx, argument(argc, argv, 1), &overflow))
        return JS_EXCEPTION;
    if (kind == PLAIN_DATETIME &&
        qjs_temporal_duration_normalize(&normalized, &input, TRUE))
        return JS_ThrowRangeError(ctx, "Temporal duration outside range");
    if (kind == PLAIN_YEAR_MONTH) {
        if (normalized.date.weeks || normalized.date.days ||
            normalized.time.low || normalized.time.high)
            return JS_ThrowRangeError(ctx,
                                      "YearMonth accepts only years/months");
        error = js_temporal_iso_date_to_fields(ctx, value.calendar,
                                               value.datetime.date, &fields);
        if (!error) {
            restrict_fields(&fields, PLAIN_YEAR_MONTH);
            fields.calendar.day = 1;
            fields.calendar.present |= QJS_TEMPORAL_FIELD_DAY;
            error = resolve_fields(ctx, PLAIN_DATE, value.calendar, &fields,
                                     QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &value);
        }
        js_temporal_fields_free(ctx, &fields);
        if (error)
            return JS_EXCEPTION;
    } else if (kind == PLAIN_DATETIME) {
        if (qjs_temporal_time_add(&value.datetime.time, &overflow_days,
                                  value.datetime.time, normalized.time) ||
            (overflow_days > 0 && normalized.date.days >
                                  INT64_MAX - overflow_days) ||
            (overflow_days < 0 && normalized.date.days <
                                  INT64_MIN - overflow_days))
            return JS_ThrowRangeError(ctx, "Temporal time addition overflow");
        normalized.date.days += overflow_days;
    }
    error = js_temporal_calendar_date_add(ctx, value.calendar,
                  value.datetime.date, normalized.date, overflow,
                  &value.datetime.date);
    if (error)
        return JS_EXCEPTION;
    if (kind == PLAIN_YEAR_MONTH) {
        error = js_temporal_iso_date_to_fields(ctx, value.calendar,
                                               value.datetime.date, &fields);
        if (!error) {
            restrict_fields(&fields, kind);
            error = resolve_fields(ctx, kind, value.calendar,
                                     &fields, overflow, &value);
        }
        js_temporal_fields_free(ctx, &fields);
        if (error)
            return JS_EXCEPTION;
    }
    return create_plain(ctx, JS_UNDEFINED, kind, value);
}

int js_temporal_plain_datetime_difference_round(
    JSContext *ctx, QJSTemporalISODateTime one, QJSTemporalISODateTime two,
    QJSTemporalCalendar calendar,
    const QJSTemporalDifferenceSettings *settings,
    QJSTemporalInternalDuration *result)
{
    return js_temporal_calendar_error(ctx,
        qjs_temporal_plain_datetime_difference_round(one, two, calendar,
                                                      settings, result));
}

int js_temporal_plain_datetime_difference_total(
    JSContext *ctx, QJSTemporalISODateTime one, QJSTemporalISODateTime two,
    QJSTemporalCalendar calendar, QJSTemporalUnit unit, double *result)
{
    return js_temporal_calendar_error(ctx,
        qjs_temporal_plain_datetime_difference_total(one, two, calendar,
                                                      unit, result));
}

static int year_month_first_day(JSContext *ctx, PlainValue *value)
{
    JSTemporalFields fields;
    int error = js_temporal_iso_date_to_fields(ctx, value->calendar,
                                               value->datetime.date, &fields);
    if (!error) {
        restrict_fields(&fields, PLAIN_YEAR_MONTH);
        fields.calendar.day = 1;
        fields.calendar.present |= QJS_TEMPORAL_FIELD_DAY;
        error = resolve_fields(ctx, PLAIN_DATE, value->calendar, &fields,
                                 QJS_TEMPORAL_OVERFLOW_CONSTRAIN, value);
    }
    js_temporal_fields_free(ctx, &fields);
    return error;
}

static JSValue plain_difference(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic)
{
    int kind = magic >> 8, since = magic & 255, error, i;
    PlainValue one, two;
    JSValue other, options;
    QJSTemporalDifferenceSettings settings;
    QJSTemporalInternalDuration difference;
    QJSTemporalDuration result;
    if (get_plain(ctx, this_val, kind, &one, TRUE))
        return JS_EXCEPTION;
    other = to_plain(ctx, argument(argc, argv, 0), JS_UNDEFINED, kind);
    if (JS_IsException(other))
        return other;
    if (get_plain(ctx, other, kind, &two, TRUE)) {
        JS_FreeValue(ctx, other);
        return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, other);
    if (one.calendar != two.calendar)
        return JS_ThrowRangeError(ctx, "Temporal calendars differ");
    options = js_temporal_get_options(ctx, argument(argc, argv, 1));
    if (JS_IsException(options))
        return options;
    error = js_temporal_get_difference_settings(ctx, options, since,
               QJS_TEMPORAL_YEAR, kind == PLAIN_DATETIME ?
               QJS_TEMPORAL_NANOSECOND : kind == PLAIN_YEAR_MONTH ?
               QJS_TEMPORAL_MONTH : QJS_TEMPORAL_DAY,
               kind == PLAIN_DATETIME ? QJS_TEMPORAL_NANOSECOND :
               kind == PLAIN_YEAR_MONTH ? QJS_TEMPORAL_MONTH :
                                         QJS_TEMPORAL_DAY,
               kind == PLAIN_YEAR_MONTH ? QJS_TEMPORAL_YEAR :
                                         QJS_TEMPORAL_DAY, &settings);
    JS_FreeValue(ctx, options);
    if (error)
        return JS_EXCEPTION;
    if (qjs_temporal_iso_datetime_compare(one.datetime, two.datetime) == 0) {
        result = (QJSTemporalDuration){ { 0 } };
        return js_temporal_create_duration(ctx, JS_UNDEFINED, &result);
    }
    if (kind == PLAIN_YEAR_MONTH &&
        (year_month_first_day(ctx, &one) || year_month_first_day(ctx, &two)))
        return JS_EXCEPTION;
    if (kind != PLAIN_DATETIME &&
        settings.smallest_unit == (kind == PLAIN_YEAR_MONTH ?
                                QJS_TEMPORAL_MONTH : QJS_TEMPORAL_DAY) &&
        settings.rounding_increment == 1) {
        difference.time = (QJSTemporalEpochNs){ 0, 0 };
        error = js_temporal_calendar_error(ctx,
          qjs_temporal_calendar_date_until(one.calendar, one.datetime.date,
                      two.datetime.date, settings.largest_unit,
                      &difference.date));
        if (kind == PLAIN_YEAR_MONTH)
            difference.date.weeks = difference.date.days = 0;
    } else if (kind != PLAIN_DATETIME) {
        difference.time = (QJSTemporalEpochNs){ 0, 0 };
        error = qjs_temporal_calendar_date_until(one.calendar,
                  one.datetime.date, two.datetime.date,
                  settings.largest_unit, &difference.date);
        if (!error) {
            if (kind == PLAIN_YEAR_MONTH)
                difference.date.weeks = difference.date.days = 0;
            error = qjs_temporal_plain_relative_round(one.datetime,
                         two.datetime, one.calendar, difference,
                         &settings, &difference);
        }
        error = js_temporal_calendar_error(ctx, error);
    } else {
        error = js_temporal_plain_datetime_difference_round(ctx,
                    one.datetime, two.datetime, one.calendar,
                    &settings, &difference);
    }
    if (error)
        return JS_EXCEPTION;
    if (qjs_temporal_duration_from_internal(&result, difference,
                   kind == PLAIN_DATETIME ? settings.largest_unit :
                                           QJS_TEMPORAL_DAY))
        return JS_ThrowRangeError(ctx, "Temporal difference outside range");
    if (since)
        for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++)
            result.fields[i] = -result.fields[i];
    return js_temporal_create_duration(ctx, JS_UNDEFINED, &result);
}

static JSValue plain_round(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    PlainValue value;
    JSValue options;
    JSValueConst input = argument(argc, argv, 0);
    QJSTemporalUnit unit;
    QJSTemporalRoundingMode mode;
    uint32_t increment;
    uint64_t length, maximum;
    int error;
    if (get_plain(ctx, this_val, PLAIN_DATETIME, &value, TRUE))
        return JS_EXCEPTION;
    if (JS_IsUndefined(input))
        return JS_ThrowTypeError(ctx, "round requires an argument");
    if (JS_IsString(input)) {
        options = JS_NewObjectProto(ctx, JS_NULL);
        if (!JS_IsException(options) &&
            JS_DefinePropertyValueStr(ctx, options, "smallestUnit",
                   JS_DupValue(ctx, input), JS_PROP_C_W_E) < 0) {
            JS_FreeValue(ctx, options);
            return JS_EXCEPTION;
        }
    } else {
        options = js_temporal_get_options(ctx, input);
    }
    if (JS_IsException(options))
        return options;
    error = js_temporal_get_rounding_increment(ctx, options, &increment) ||
            js_temporal_get_rounding_mode(ctx, options,
                           QJS_TEMPORAL_ROUND_HALF_EXPAND, &mode) ||
            js_temporal_get_unit_option(ctx, options, "smallestUnit", TRUE,
                                          &unit);
    JS_FreeValue(ctx, options);
    if (error)
        return JS_EXCEPTION;
    if (unit < QJS_TEMPORAL_DAY || unit > QJS_TEMPORAL_NANOSECOND)
        return JS_ThrowRangeError(ctx, "invalid PlainDateTime round unit");
    length = qjs_temporal_unit_nanoseconds(unit);
    maximum = unit == QJS_TEMPORAL_DAY ? 1 : unit == QJS_TEMPORAL_HOUR ? 24 :
              unit <= QJS_TEMPORAL_SECOND ? 60 : 1000;
    if (js_temporal_validate_increment(ctx, increment, maximum,
                                       unit == QJS_TEMPORAL_DAY))
        return JS_EXCEPTION;
    if (qjs_temporal_iso_datetime_round(&value.datetime, value.datetime,
                                      length * increment, mode))
        return JS_ThrowRangeError(ctx, "Temporal rounding overflow");
    return create_plain(ctx, JS_UNDEFINED, PLAIN_DATETIME, value);
}

static JSValue plain_to_zoned(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int kind)
{
    PlainValue value;
    JSTemporalTimeZone zone;
    QJSTemporalEpochNs epoch;
    JSValueConst input = argument(argc, argv, 0);
    JSValue zone_input, time_input;
    JSValue options, result;
    QJSTemporalDisambiguation disambiguation = QJS_TEMPORAL_COMPATIBLE;
    int error, choice;
    static const char *const disambiguations[] = {
        "compatible", "earlier", "later", "reject"
    };
    if (get_plain(ctx, this_val, kind, &value, TRUE))
        return JS_EXCEPTION;
    time_input = JS_UNDEFINED;
    if (kind == PLAIN_DATE && JS_IsObject(input)) {
        zone_input = JS_GetPropertyStr(ctx, input, "timeZone");
        if (JS_IsException(zone_input))
            return zone_input;
        if (JS_IsUndefined(zone_input)) {
            JS_FreeValue(ctx, zone_input);
            zone_input = JS_DupValue(ctx, input);
            error = js_temporal_to_time_zone(ctx, zone_input, &zone);
        } else {
            error = js_temporal_to_time_zone(ctx, zone_input, &zone);
            if (!error)
                time_input = JS_GetPropertyStr(ctx, input, "plainTime");
        }
        JS_FreeValue(ctx, zone_input);
    } else {
        error = js_temporal_to_time_zone(ctx, input, &zone);
    }
    if (error)
        return JS_EXCEPTION;
    if (JS_IsException(time_input)) {
        js_temporal_free_time_zone(ctx, &zone);
        return JS_EXCEPTION;
    }
    if (kind == PLAIN_DATETIME) {
        options = js_temporal_get_options(ctx, argument(argc, argv, 1));
        if (JS_IsException(options)) {
            error = -1;
        } else {
            error = js_temporal_get_string_option(ctx, options,
                      "disambiguation", disambiguations, 4, 0, &choice);
            JS_FreeValue(ctx, options);
            if (!error)
                disambiguation = (QJSTemporalDisambiguation)choice;
        }
    } else if (!JS_IsUndefined(time_input)) {
        error = get_time_or_midnight(ctx, time_input, &value.datetime.time);
    }
    if (!error) {
        if (kind == PLAIN_DATE && JS_IsUndefined(time_input))
            error = js_temporal_time_zone_start_of_day(ctx, &zone,
                                                       value.datetime.date,
                                                       &epoch);
        else
            error = js_temporal_time_zone_epoch(ctx, &zone, value.datetime,
                                                 disambiguation, &epoch);
    }
    JS_FreeValue(ctx, time_input);
    result = error ? JS_EXCEPTION : js_temporal_create_zoned_date_time(ctx,
                        JS_UNDEFINED, epoch, &zone, value.calendar);
    js_temporal_free_time_zone(ctx, &zone);
    return result;
}

static JSValue plain_string(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int magic)
{
    int kind = magic >> 8, json = magic & 255, show = 0, digits = -1;
    PlainValue value;
    JSValue options;
    QJSTemporalStringPrecision precision;
    QJSTemporalRoundingMode mode = QJS_TEMPORAL_ROUND_TRUNC;
    QJSTemporalUnit unit = QJS_TEMPORAL_UNIT_UNSET;
    char buffer[160], time[48];
    int length, time_length, error = 0;
    if (get_plain(ctx, this_val, kind, &value, TRUE))
        return JS_EXCEPTION;
    if (!json) {
        options = js_temporal_get_options(ctx, argument(argc, argv, 0));
        if (JS_IsException(options))
            return options;
        error = js_temporal_get_string_option(ctx, options, "calendarName",
                                               calendar_names, 4, 0, &show);
        if (!error && kind == PLAIN_DATETIME)
            error = js_temporal_get_fractional_digits(ctx, options, &digits) ||
                    js_temporal_get_rounding_mode(ctx, options,
                                 QJS_TEMPORAL_ROUND_TRUNC, &mode) ||
                    js_temporal_get_unit_option(ctx, options, "smallestUnit",
                                                 FALSE, &unit);
        JS_FreeValue(ctx, options);
        if (error)
            return JS_EXCEPTION;
    }
    if (kind == PLAIN_DATETIME) {
        if (unit != QJS_TEMPORAL_UNIT_UNSET &&
            (unit < QJS_TEMPORAL_MINUTE || unit > QJS_TEMPORAL_NANOSECOND))
            return JS_ThrowRangeError(ctx, "invalid string precision unit");
        if (qjs_temporal_string_precision(&precision, unit, digits) ||
            qjs_temporal_iso_datetime_round(&value.datetime, value.datetime,
                qjs_temporal_unit_nanoseconds(precision.unit) *
                precision.increment, mode) ||
            !qjs_temporal_iso_datetime_within_limits(value.datetime))
            return JS_ThrowRangeError(ctx, "Temporal string rounding overflow");
    }
    length = qjs_temporal_iso_date_format(buffer, sizeof(buffer),
                                          value.datetime.date);
    if (length < 0)
        return JS_ThrowRangeError(ctx, "invalid Temporal ISO date");
    if (kind == PLAIN_YEAR_MONTH && show != 1 && show != 3 &&
        value.calendar == QJS_TEMPORAL_CAL_ISO8601) {
        length -= 3;
        buffer[length] = '\0';
    } else if (kind == PLAIN_MONTH_DAY && show != 1 && show != 3 &&
               value.calendar == QJS_TEMPORAL_CAL_ISO8601) {
        memmove(buffer, buffer + length - 5, 5);
        length = 5;
        buffer[length] = '\0';
    } else if (kind == PLAIN_DATETIME) {
        time_length = qjs_temporal_format_time(time, sizeof(time),
                            value.datetime.time, precision.precision);
        if (time_length < 0)
            return JS_ThrowRangeError(ctx, "invalid Temporal time format");
        buffer[length++] = 'T';
        memcpy(buffer + length, time, time_length);
        length += time_length;
        buffer[length] = '\0';
    }
    if (show != 2 && (show != 0 ||
                     value.calendar != QJS_TEMPORAL_CAL_ISO8601)) {
        int written = snprintf(buffer + length, sizeof(buffer) - length,
                    "[%su-ca=%s]", show == 3 ? "!" : "",
                    qjs_temporal_calendar_identifier(value.calendar));
        if (written < 0 || (size_t)written >= sizeof(buffer) - length)
            return JS_ThrowInternalError(ctx, "Temporal annotation overflow");
        length += written;
    }
    return JS_NewStringLen(ctx, buffer, length);
}

static JSValue plain_locale_string(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv, int kind)
{
    PlainValue value;
    if (get_plain(ctx, this_val, kind, &value, TRUE))
        return JS_EXCEPTION;
#ifdef CONFIG_INTL
    return js_intl_temporal_to_locale_string(ctx, this_val,
                         argument(argc, argv, 0), argument(argc, argv, 1));
#else
    return plain_string(ctx, this_val, 0, NULL, (kind << 8) | 1);
#endif
}

static JSValue plain_value_of(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv)
{
    return JS_ThrowTypeError(ctx, "Temporal objects cannot convert to numbers");
}

static void plain_finalizer(JSRuntime *rt, JSValue value)
{
    int i;
    for (i = 0; i < 4; i++) {
        void *data = JS_GetOpaque(value, plain_classes[i]);
        if (data) {
            js_free_rt(rt, data);
            return;
        }
    }
}

static const JSCFunctionListEntry plain_date_static[] = {
    JS_CFUNC_MAGIC_DEF("from", 1, plain_from, PLAIN_DATE),
    JS_CFUNC_MAGIC_DEF("compare", 2, plain_compare, PLAIN_DATE),
};

static const JSCFunctionListEntry plain_date_prototype[] = {
    JS_CGETSET_MAGIC_DEF("calendarId", plain_getter, NULL,
                         (PLAIN_DATE << 8) | GET_CALENDAR),
    JS_CGETSET_MAGIC_DEF("era", plain_getter, NULL,
                         (PLAIN_DATE << 8) | GET_ERA),
    JS_CGETSET_MAGIC_DEF("eraYear", plain_getter, NULL,
                         (PLAIN_DATE << 8) | GET_ERA_YEAR),
    JS_CGETSET_MAGIC_DEF("year", plain_getter, NULL,
                         (PLAIN_DATE << 8) | GET_YEAR),
    JS_CGETSET_MAGIC_DEF("month", plain_getter, NULL,
                         (PLAIN_DATE << 8) | GET_MONTH),
    JS_CGETSET_MAGIC_DEF("monthCode", plain_getter, NULL,
                         (PLAIN_DATE << 8) | GET_MONTH_CODE),
    JS_CGETSET_MAGIC_DEF("day", plain_getter, NULL,
                         (PLAIN_DATE << 8) | GET_DAY),
    JS_CGETSET_MAGIC_DEF("dayOfWeek", plain_getter, NULL,
                         (PLAIN_DATE << 8) | GET_DAY_OF_WEEK),
    JS_CGETSET_MAGIC_DEF("dayOfYear", plain_getter, NULL,
                         (PLAIN_DATE << 8) | GET_DAY_OF_YEAR),
    JS_CGETSET_MAGIC_DEF("weekOfYear", plain_getter, NULL,
                         (PLAIN_DATE << 8) | GET_WEEK),
    JS_CGETSET_MAGIC_DEF("yearOfWeek", plain_getter, NULL,
                         (PLAIN_DATE << 8) | GET_WEEK_YEAR),
    JS_CGETSET_MAGIC_DEF("daysInWeek", plain_getter, NULL,
                         (PLAIN_DATE << 8) | GET_DAYS_IN_WEEK),
    JS_CGETSET_MAGIC_DEF("daysInMonth", plain_getter, NULL,
                         (PLAIN_DATE << 8) | GET_DAYS_IN_MONTH),
    JS_CGETSET_MAGIC_DEF("daysInYear", plain_getter, NULL,
                         (PLAIN_DATE << 8) | GET_DAYS_IN_YEAR),
    JS_CGETSET_MAGIC_DEF("monthsInYear", plain_getter, NULL,
                         (PLAIN_DATE << 8) | GET_MONTHS_IN_YEAR),
    JS_CGETSET_MAGIC_DEF("inLeapYear", plain_getter, NULL,
                         (PLAIN_DATE << 8) | GET_LEAP),
    JS_CFUNC_MAGIC_DEF("with", 1, plain_with, PLAIN_DATE),
    JS_CFUNC_MAGIC_DEF("equals", 1, plain_equals, PLAIN_DATE),
    JS_CFUNC_MAGIC_DEF("toString", 0, plain_string, PLAIN_DATE << 8),
    JS_CFUNC_MAGIC_DEF("toJSON", 0, plain_string, (PLAIN_DATE << 8) | 1),
    JS_CFUNC_MAGIC_DEF("toLocaleString", 0, plain_locale_string, PLAIN_DATE),
    JS_CFUNC_DEF("valueOf", 0, plain_value_of),
    JS_CFUNC_MAGIC_DEF("add", 1, plain_add, (PLAIN_DATE << 8) | 0),
    JS_CFUNC_MAGIC_DEF("subtract", 1, plain_add, (PLAIN_DATE << 8) | 1),
    JS_CFUNC_MAGIC_DEF("until", 1, plain_difference, (PLAIN_DATE << 8) | 0),
    JS_CFUNC_MAGIC_DEF("since", 1, plain_difference, (PLAIN_DATE << 8) | 1),
    JS_CFUNC_MAGIC_DEF("withCalendar", 1, plain_with_calendar, PLAIN_DATE),
    JS_CFUNC_MAGIC_DEF("toZonedDateTime", 1, plain_to_zoned, PLAIN_DATE),
    JS_CFUNC_MAGIC_DEF("toPlainDateTime", 0, plain_with_time, PLAIN_DATE),
    JS_CFUNC_MAGIC_DEF("toPlainYearMonth", 0, plain_convert,
                        (PLAIN_DATE << 8) | PLAIN_YEAR_MONTH),
    JS_CFUNC_MAGIC_DEF("toPlainMonthDay", 0, plain_convert,
                        (PLAIN_DATE << 8) | PLAIN_MONTH_DAY),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Temporal.PlainDate",
                       JS_PROP_CONFIGURABLE),
};

static const JSCFunctionListEntry plain_datetime_static[] = {
    JS_CFUNC_MAGIC_DEF("from", 1, plain_from, PLAIN_DATETIME),
    JS_CFUNC_MAGIC_DEF("compare", 2, plain_compare, PLAIN_DATETIME),
};

static const JSCFunctionListEntry plain_datetime_prototype[] = {
    JS_CGETSET_MAGIC_DEF("calendarId", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_CALENDAR),
    JS_CGETSET_MAGIC_DEF("era", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_ERA),
    JS_CGETSET_MAGIC_DEF("eraYear", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_ERA_YEAR),
    JS_CGETSET_MAGIC_DEF("year", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_YEAR),
    JS_CGETSET_MAGIC_DEF("month", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_MONTH),
    JS_CGETSET_MAGIC_DEF("monthCode", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_MONTH_CODE),
    JS_CGETSET_MAGIC_DEF("day", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_DAY),
    JS_CGETSET_MAGIC_DEF("dayOfWeek", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_DAY_OF_WEEK),
    JS_CGETSET_MAGIC_DEF("dayOfYear", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_DAY_OF_YEAR),
    JS_CGETSET_MAGIC_DEF("weekOfYear", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_WEEK),
    JS_CGETSET_MAGIC_DEF("yearOfWeek", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_WEEK_YEAR),
    JS_CGETSET_MAGIC_DEF("daysInWeek", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_DAYS_IN_WEEK),
    JS_CGETSET_MAGIC_DEF("daysInMonth", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_DAYS_IN_MONTH),
    JS_CGETSET_MAGIC_DEF("daysInYear", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_DAYS_IN_YEAR),
    JS_CGETSET_MAGIC_DEF("monthsInYear", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_MONTHS_IN_YEAR),
    JS_CGETSET_MAGIC_DEF("inLeapYear", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_LEAP),
    JS_CGETSET_MAGIC_DEF("hour", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_HOUR),
    JS_CGETSET_MAGIC_DEF("minute", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_MINUTE),
    JS_CGETSET_MAGIC_DEF("second", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_SECOND),
    JS_CGETSET_MAGIC_DEF("millisecond", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_MILLISECOND),
    JS_CGETSET_MAGIC_DEF("microsecond", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_MICROSECOND),
    JS_CGETSET_MAGIC_DEF("nanosecond", plain_getter, NULL,
                         (PLAIN_DATETIME << 8) | GET_NANOSECOND),
    JS_CFUNC_MAGIC_DEF("with", 1, plain_with, PLAIN_DATETIME),
    JS_CFUNC_MAGIC_DEF("equals", 1, plain_equals, PLAIN_DATETIME),
    JS_CFUNC_MAGIC_DEF("toString", 0, plain_string, PLAIN_DATETIME << 8),
    JS_CFUNC_MAGIC_DEF("toJSON", 0, plain_string, (PLAIN_DATETIME << 8) | 1),
    JS_CFUNC_MAGIC_DEF("toLocaleString", 0, plain_locale_string, PLAIN_DATETIME),
    JS_CFUNC_DEF("valueOf", 0, plain_value_of),
    JS_CFUNC_MAGIC_DEF("add", 1, plain_add, (PLAIN_DATETIME << 8) | 0),
    JS_CFUNC_MAGIC_DEF("subtract", 1, plain_add, (PLAIN_DATETIME << 8) | 1),
    JS_CFUNC_MAGIC_DEF("until", 1, plain_difference, (PLAIN_DATETIME << 8) | 0),
    JS_CFUNC_MAGIC_DEF("since", 1, plain_difference, (PLAIN_DATETIME << 8) | 1),
    JS_CFUNC_MAGIC_DEF("withCalendar", 1, plain_with_calendar, PLAIN_DATETIME),
    JS_CFUNC_MAGIC_DEF("toZonedDateTime", 1, plain_to_zoned, PLAIN_DATETIME),
    JS_CFUNC_MAGIC_DEF("withPlainTime", 0, plain_with_time, PLAIN_DATETIME),
    JS_CFUNC_DEF("round", 1, plain_round),
    JS_CFUNC_MAGIC_DEF("toPlainDate", 0, plain_convert,
                        (PLAIN_DATETIME << 8) | PLAIN_DATE),
    JS_CFUNC_MAGIC_DEF("toPlainTime", 0, plain_convert,
                        (PLAIN_DATETIME << 8) | 4),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Temporal.PlainDateTime",
                       JS_PROP_CONFIGURABLE),
};

static const JSCFunctionListEntry plain_year_month_static[] = {
    JS_CFUNC_MAGIC_DEF("from", 1, plain_from, PLAIN_YEAR_MONTH),
    JS_CFUNC_MAGIC_DEF("compare", 2, plain_compare, PLAIN_YEAR_MONTH),
};

static const JSCFunctionListEntry plain_year_month_prototype[] = {
    JS_CGETSET_MAGIC_DEF("calendarId", plain_getter, NULL,
                         (PLAIN_YEAR_MONTH << 8) | GET_CALENDAR),
    JS_CGETSET_MAGIC_DEF("era", plain_getter, NULL,
                         (PLAIN_YEAR_MONTH << 8) | GET_ERA),
    JS_CGETSET_MAGIC_DEF("eraYear", plain_getter, NULL,
                         (PLAIN_YEAR_MONTH << 8) | GET_ERA_YEAR),
    JS_CGETSET_MAGIC_DEF("year", plain_getter, NULL,
                         (PLAIN_YEAR_MONTH << 8) | GET_YEAR),
    JS_CGETSET_MAGIC_DEF("month", plain_getter, NULL,
                         (PLAIN_YEAR_MONTH << 8) | GET_MONTH),
    JS_CGETSET_MAGIC_DEF("monthCode", plain_getter, NULL,
                         (PLAIN_YEAR_MONTH << 8) | GET_MONTH_CODE),
    JS_CGETSET_MAGIC_DEF("daysInMonth", plain_getter, NULL,
                         (PLAIN_YEAR_MONTH << 8) | GET_DAYS_IN_MONTH),
    JS_CGETSET_MAGIC_DEF("daysInYear", plain_getter, NULL,
                         (PLAIN_YEAR_MONTH << 8) | GET_DAYS_IN_YEAR),
    JS_CGETSET_MAGIC_DEF("monthsInYear", plain_getter, NULL,
                         (PLAIN_YEAR_MONTH << 8) | GET_MONTHS_IN_YEAR),
    JS_CGETSET_MAGIC_DEF("inLeapYear", plain_getter, NULL,
                         (PLAIN_YEAR_MONTH << 8) | GET_LEAP),
    JS_CFUNC_MAGIC_DEF("with", 1, plain_with, PLAIN_YEAR_MONTH),
    JS_CFUNC_MAGIC_DEF("equals", 1, plain_equals, PLAIN_YEAR_MONTH),
    JS_CFUNC_MAGIC_DEF("toString", 0, plain_string, PLAIN_YEAR_MONTH << 8),
    JS_CFUNC_MAGIC_DEF("toJSON", 0, plain_string, (PLAIN_YEAR_MONTH << 8) | 1),
    JS_CFUNC_MAGIC_DEF("toLocaleString", 0, plain_locale_string, PLAIN_YEAR_MONTH),
    JS_CFUNC_DEF("valueOf", 0, plain_value_of),
    JS_CFUNC_MAGIC_DEF("add", 1, plain_add, (PLAIN_YEAR_MONTH << 8) | 0),
    JS_CFUNC_MAGIC_DEF("subtract", 1, plain_add, (PLAIN_YEAR_MONTH << 8) | 1),
    JS_CFUNC_MAGIC_DEF("until", 1, plain_difference, (PLAIN_YEAR_MONTH << 8) | 0),
    JS_CFUNC_MAGIC_DEF("since", 1, plain_difference, (PLAIN_YEAR_MONTH << 8) | 1),
    JS_CFUNC_MAGIC_DEF("toPlainDate", 1, plain_partial_to_date, PLAIN_YEAR_MONTH),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Temporal.PlainYearMonth",
                       JS_PROP_CONFIGURABLE),
};

static const JSCFunctionListEntry plain_month_day_static[] = {
    JS_CFUNC_MAGIC_DEF("from", 1, plain_from, PLAIN_MONTH_DAY),
};

static const JSCFunctionListEntry plain_month_day_prototype[] = {
    JS_CGETSET_MAGIC_DEF("calendarId", plain_getter, NULL,
                         (PLAIN_MONTH_DAY << 8) | GET_CALENDAR),
    JS_CGETSET_MAGIC_DEF("monthCode", plain_getter, NULL,
                         (PLAIN_MONTH_DAY << 8) | GET_MONTH_CODE),
    JS_CGETSET_MAGIC_DEF("day", plain_getter, NULL,
                         (PLAIN_MONTH_DAY << 8) | GET_DAY),
    JS_CFUNC_MAGIC_DEF("with", 1, plain_with, PLAIN_MONTH_DAY),
    JS_CFUNC_MAGIC_DEF("equals", 1, plain_equals, PLAIN_MONTH_DAY),
    JS_CFUNC_MAGIC_DEF("toString", 0, plain_string, PLAIN_MONTH_DAY << 8),
    JS_CFUNC_MAGIC_DEF("toJSON", 0, plain_string, (PLAIN_MONTH_DAY << 8) | 1),
    JS_CFUNC_MAGIC_DEF("toLocaleString", 0, plain_locale_string, PLAIN_MONTH_DAY),
    JS_CFUNC_DEF("valueOf", 0, plain_value_of),
    JS_CFUNC_MAGIC_DEF("toPlainDate", 1, plain_partial_to_date, PLAIN_MONTH_DAY),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Temporal.PlainMonthDay",
                       JS_PROP_CONFIGURABLE),
};

static int init_plain(JSContext *ctx, JSValueConst namespace_object, int kind)
{
    static const JSClassDef classes[] = {
        { .class_name = "PlainDate", .finalizer = plain_finalizer },
        { .class_name = "PlainDateTime", .finalizer = plain_finalizer },
        { .class_name = "PlainYearMonth", .finalizer = plain_finalizer },
        { .class_name = "PlainMonthDay", .finalizer = plain_finalizer },
    };
    static const JSCFunctionListEntry *const statics[] = {
        plain_date_static, plain_datetime_static,
        plain_year_month_static, plain_month_day_static
    };
    static const JSCFunctionListEntry *const prototypes[] = {
        plain_date_prototype, plain_datetime_prototype,
        plain_year_month_prototype, plain_month_day_prototype
    };
    static const int static_counts[] = {
        countof(plain_date_static), countof(plain_datetime_static),
        countof(plain_year_month_static), countof(plain_month_day_static)
    };
    static const int prototype_counts[] = {
        countof(plain_date_prototype), countof(plain_datetime_prototype),
        countof(plain_year_month_prototype), countof(plain_month_day_prototype)
    };
    JSValue constructor;
    JSCFunctionType ft = { .generic_magic = plain_constructor };
    if (!JS_IsRegisteredClass(ctx->rt, plain_classes[kind]) &&
        JS_NewClass(ctx->rt, plain_classes[kind], &classes[kind]) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    constructor = JS_NewCConstructor(ctx, plain_classes[kind],
                    plain_names[kind], ft.generic,
                    kind <= PLAIN_DATETIME ? 3 : 2,
                    JS_CFUNC_constructor_magic, kind, JS_UNDEFINED,
                    statics[kind], static_counts[kind],
                    prototypes[kind], prototype_counts[kind],
                    JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(constructor))
        return -1;
    if (JS_HasException(ctx)) {
        JS_FreeValue(ctx, constructor);
        return -1;
    }
    return JS_DefinePropertyValueStr(ctx, namespace_object,
                   plain_names[kind], constructor,
                   JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE) < 0 ? -1 : 0;
}

int js_temporal_init_plain_date(JSContext *ctx, JSValueConst object)
{ return init_plain(ctx, object, PLAIN_DATE); }
int js_temporal_init_plain_date_time(JSContext *ctx, JSValueConst object)
{ return init_plain(ctx, object, PLAIN_DATETIME); }
int js_temporal_init_plain_year_month(JSContext *ctx, JSValueConst object)
{ return init_plain(ctx, object, PLAIN_YEAR_MONTH); }
int js_temporal_init_plain_month_day(JSContext *ctx, JSValueConst object)
{ return init_plain(ctx, object, PLAIN_MONTH_DAY); }
