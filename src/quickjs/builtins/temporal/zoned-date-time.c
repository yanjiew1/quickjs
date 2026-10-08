/*
 * Native Temporal.ZonedDateTime
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
#include <stdio.h>
#include <string.h>
#include "calendar-fields.h"
#include "../temporal.h"
#include "../../../temporal/format.h"

/* Temporal proposal e8cc03fc970a65a3359e8870e3b35e687ac94e55,
   dated 2026-07-27; pinned algorithms reviewed 2026-10-08.
   Engine coercion, branding, realms and owned identifiers live here;
   calendar arithmetic and zone data stay behind native backend contracts. */

static JSValueConst zdt_argument(int argc, JSValueConst *argv, int index)
{
    return argc > index ? argv[index] : JS_UNDEFINED;
}

static void js_temporal_zoned_finalizer(JSRuntime *rt, JSValue object)
{
    JSTemporalZonedDateTimeData *s;
    s = JS_GetOpaque(object, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (s) {
        JS_FreeValueRT(rt, s->time_zone.identifier);
        js_free_rt(rt, s);
    }
}

static void js_temporal_zoned_mark(JSRuntime *rt, JSValueConst object,
                                    JS_MarkFunc *mark_func)
{
    JSTemporalZonedDateTimeData *s;
    s = JS_GetOpaque(object, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (s)
        JS_MarkValue(rt, s->time_zone.identifier, mark_func);
}

static const JSClassDef js_temporal_zoned_class = {
    .class_name = "Temporal.ZonedDateTime",
    .finalizer = js_temporal_zoned_finalizer,
    .gc_mark = js_temporal_zoned_mark,
};

JSValue js_temporal_create_zoned_date_time(JSContext *ctx,
    JSValueConst new_target, QJSTemporalEpochNs epoch,
    const JSTemporalTimeZone *zone, QJSTemporalCalendar calendar)
{
    JSTemporalZonedDateTimeData *s;
    JSValue object;

    if (!qjs_temporal_epoch_ns_is_valid(epoch))
        return JS_ThrowRangeError(ctx, "Temporal epoch is outside the valid range");
    object = js_temporal_create_from_ctor(ctx, new_target, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (JS_IsException(object))
        return object;
    s = js_malloc(ctx, sizeof(*s));
    if (!s) {
        JS_FreeValue(ctx, object);
        return JS_EXCEPTION;
    }
    s->epoch_nanoseconds = epoch;
    s->time_zone = *zone;
    s->time_zone.identifier = JS_DupValue(ctx, zone->identifier);
    s->calendar = calendar;
    JS_SetOpaque(object, s);
    return object;
}

/* Constructors use identifier grammar, not Temporal string conversion. */
static int zdt_zone_identifier(JSContext *ctx, const char *text, size_t length,
                                JSTemporalTimeZone *result)
{
    QJSTemporalZone native;
    int error;

    memset(result, 0, sizeof(*result));
    result->identifier = JS_UNDEFINED;
    error = qjs_temporal_zone_parse(&native, text, length);
    if (js_temporal_calendar_error(ctx, error))
        return -1;
    result->identifier = JS_NewString(ctx, native.identifier);
    if (JS_IsException(result->identifier)) {
        result->identifier = JS_UNDEFINED;
        return -1;
    }
    result->is_offset = native.is_offset;
    result->offset_nanoseconds = native.offset_nanoseconds;
    return 0;
}

static int zdt_calendar_identifier(JSContext *ctx, const char *text,
                                    size_t length, QJSTemporalCalendar *result)
{
    return js_temporal_calendar_error(ctx,
        qjs_temporal_calendar_from_identifier(result, text, length));
}

static int zdt_conversion_options(JSContext *ctx, JSValueConst value,
    JSTemporalOffsetOption fallback_offset, QJSTemporalDisambiguation *disambiguation,
    JSTemporalOffsetOption *offset, QJSTemporalOverflow *overflow)
{
    static const char *const disambiguations[] = {
        "compatible", "earlier", "later", "reject"
    };
    static const char *const offsets[] = { "ignore", "use", "prefer", "reject" };
    static const char *const overflows[] = { "constrain", "reject" };
    JSValue options;
    int d, o, v, ret;

    options = js_temporal_get_options(ctx, value);
    if (JS_IsException(options))
        return -1;
    ret = js_temporal_get_string_option(ctx, options, "disambiguation",
                  disambiguations, countof(disambiguations), QJS_TEMPORAL_COMPATIBLE, &d) ||
          js_temporal_get_string_option(ctx, options, "offset", offsets,
                  countof(offsets), fallback_offset, &o) ||
          js_temporal_get_string_option(ctx, options, "overflow", overflows,
                  countof(overflows), QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &v);
    JS_FreeValue(ctx, options);
    if (ret)
        return -1;
    *disambiguation = d;
    *offset = o;
    *overflow = v;
    return 0;
}

static int zdt_offset_nanoseconds(JSContext *ctx, JSValueConst string,
                                   int64_t *result)
{
    QJSTemporalUTCOffset offset;
    const char *text;
    size_t length, consumed;
    int ret;

    text = JS_ToCStringLen(ctx, &length, string);
    if (!text)
        return -1;
    ret = qjs_temporal_parse_utc_offset_prefix(&offset, &consumed, text,
                    length, QJS_TEMPORAL_OFFSET_ALLOW_SECONDS);
    JS_FreeCString(ctx, text);
    if (ret || consumed != length) {
        JS_ThrowRangeError(ctx, "invalid Temporal offset");
        return -1;
    }
    *result = offset.nanoseconds;
    return 0;
}

JSValue js_temporal_to_zoned_date_time(JSContext *ctx, JSValueConst item,
                                       JSValueConst options)
{
    JSTemporalZonedDateTimeData *existing;
    JSTemporalFields fields;
    JSTemporalTimeZone zone;
    QJSTemporalParsedISO parsed;
    QJSTemporalISODateTime datetime;
    QJSTemporalCalendar calendar;
    QJSTemporalDisambiguation disambiguation;
    JSTemporalOffsetOption offset_option;
    QJSTemporalOverflow overflow;
    JSTemporalOffsetBehaviour behaviour;
    QJSTemporalEpochNs epoch;
    const char *text;
    size_t length;
    int64_t offset = 0;
    BOOL start_of_day = FALSE, match_minutes = FALSE;
    JSValue result = JS_EXCEPTION;

    if (JS_IsObject(item)) {
        existing = JS_GetOpaque(item, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
        if (existing) {
            if (zdt_conversion_options(ctx, options, JS_TEMPORAL_OFFSET_REJECT,
                                       &disambiguation, &offset_option, &overflow))
                return JS_EXCEPTION;
            return js_temporal_create_zoned_date_time(ctx, JS_UNDEFINED,
                existing->epoch_nanoseconds, &existing->time_zone, existing->calendar);
        }
        if (js_temporal_get_calendar(ctx, item, &calendar))
            return JS_EXCEPTION;
        if (js_temporal_prepare_calendar_fields(ctx, calendar, item,
                QJS_TEMPORAL_DATE_FIELDS,
                QJS_TEMPORAL_TIME_FIELDS | QJS_TEMPORAL_FIELD_OFFSET |
                QJS_TEMPORAL_FIELD_TIME_ZONE, QJS_TEMPORAL_FIELD_TIME_ZONE,
                FALSE, &fields)) {
            js_temporal_fields_free(ctx, &fields);
            return JS_EXCEPTION;
        }
        if (zdt_conversion_options(ctx, options, JS_TEMPORAL_OFFSET_REJECT,
                &disambiguation, &offset_option, &overflow) ||
            js_temporal_interpret_datetime_fields(ctx, calendar, &fields, overflow,
                                                 &datetime))
            goto object_done;
        behaviour = JS_IsUndefined(fields.offset) ?
            JS_TEMPORAL_OFFSET_WALL : JS_TEMPORAL_OFFSET_OPTION;
        if (behaviour == JS_TEMPORAL_OFFSET_OPTION &&
            zdt_offset_nanoseconds(ctx, fields.offset, &offset))
            goto object_done;
        if (!js_temporal_interpret_offset(ctx, datetime, FALSE, behaviour, offset,
                &fields.zone, disambiguation, offset_option, FALSE, &epoch))
            result = js_temporal_create_zoned_date_time(ctx, JS_UNDEFINED, epoch,
                                                        &fields.zone, calendar);
 object_done:
        js_temporal_fields_free(ctx, &fields);
        return result;
    }
    if (!JS_IsString(item))
        return JS_ThrowTypeError(ctx, "Temporal.ZonedDateTime input must be a string or object");
    text = JS_ToCStringLen(ctx, &length, item);
    if (!text)
        return JS_EXCEPTION;
    if (qjs_temporal_parse_iso_datetime(&parsed, text, length, QJS_TEMPORAL_PARSE_ZONED)) {
        JS_FreeCString(ctx, text);
        return JS_ThrowRangeError(ctx, "invalid Temporal.ZonedDateTime string");
    }
    if (zdt_zone_identifier(ctx, parsed.time_zone, parsed.time_zone_length, &zone)) {
        JS_FreeCString(ctx, text);
        return JS_EXCEPTION;
    }
    if (zdt_calendar_identifier(ctx, parsed.calendar ? parsed.calendar : "iso8601",
                          parsed.calendar ? parsed.calendar_length : 7, &calendar)) {
        JS_FreeCString(ctx, text);
        js_temporal_free_time_zone(ctx, &zone);
        return JS_EXCEPTION;
    }
    datetime = parsed.datetime;
    start_of_day = !parsed.has_time;
    behaviour = parsed.offset_present ?
        (parsed.offset.is_z ? JS_TEMPORAL_OFFSET_EXACT : JS_TEMPORAL_OFFSET_OPTION) :
        JS_TEMPORAL_OFFSET_WALL;
    offset = parsed.offset.nanoseconds;
    match_minutes = !parsed.offset.has_seconds;
    JS_FreeCString(ctx, text);
    if (!zdt_conversion_options(ctx, options, JS_TEMPORAL_OFFSET_REJECT,
            &disambiguation, &offset_option, &overflow) &&
        !js_temporal_interpret_offset(ctx, datetime, start_of_day, behaviour,
              offset, &zone, disambiguation, offset_option, match_minutes, &epoch))
        result = js_temporal_create_zoned_date_time(ctx, JS_UNDEFINED, epoch,
                                                    &zone, calendar);
    js_temporal_free_time_zone(ctx, &zone);
    return result;
}

static JSValue js_temporal_zoned_constructor(JSContext *ctx, JSValueConst new_target,
                                              int argc, JSValueConst *argv)
{
    QJSTemporalEpochNs epoch;
    QJSTemporalCalendar calendar = QJS_TEMPORAL_CAL_ISO8601;
    JSTemporalTimeZone zone;
    JSValueConst zone_value, calendar_value;
    JSValue result;
    const char *text;
    size_t length;
    int ret;

    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Temporal.ZonedDateTime requires new");
    if (js_temporal_to_epoch_ns(ctx, zdt_argument(argc, argv, 0), &epoch))
        return JS_EXCEPTION;
    zone_value = zdt_argument(argc, argv, 1);
    if (!JS_IsString(zone_value))
        return JS_ThrowTypeError(ctx, "Temporal time-zone identifier must be a string");
    text = JS_ToCStringLen(ctx, &length, zone_value);
    if (!text)
        return JS_EXCEPTION;
    ret = zdt_zone_identifier(ctx, text, length, &zone);
    JS_FreeCString(ctx, text);
    if (ret)
        return JS_EXCEPTION;
    calendar_value = zdt_argument(argc, argv, 2);
    if (!JS_IsUndefined(calendar_value)) {
        if (!JS_IsString(calendar_value)) {
            js_temporal_free_time_zone(ctx, &zone);
            return JS_ThrowTypeError(ctx, "Temporal calendar identifier must be a string");
        }
        text = JS_ToCStringLen(ctx, &length, calendar_value);
        if (!text) {
            js_temporal_free_time_zone(ctx, &zone);
            return JS_EXCEPTION;
        }
        ret = zdt_calendar_identifier(ctx, text, length, &calendar);
        JS_FreeCString(ctx, text);
        if (ret) {
            js_temporal_free_time_zone(ctx, &zone);
            return JS_EXCEPTION;
        }
    }
    result = js_temporal_create_zoned_date_time(ctx, new_target, epoch, &zone, calendar);
    js_temporal_free_time_zone(ctx, &zone);
    return result;
}

static JSValue js_temporal_zoned_from(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    return js_temporal_to_zoned_date_time(ctx, zdt_argument(argc, argv, 0),
                                          zdt_argument(argc, argv, 1));
}

static JSValue js_temporal_zoned_compare(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv)
{
    JSValue one, two;
    JSTemporalZonedDateTimeData *a, *b;
    int result;

    one = js_temporal_to_zoned_date_time(ctx, zdt_argument(argc, argv, 0), JS_UNDEFINED);
    if (JS_IsException(one))
        return one;
    two = js_temporal_to_zoned_date_time(ctx, zdt_argument(argc, argv, 1), JS_UNDEFINED);
    if (JS_IsException(two)) {
        JS_FreeValue(ctx, one);
        return two;
    }
    a = JS_GetOpaque(one, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    b = JS_GetOpaque(two, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    result = qjs_temporal_epoch_ns_compare(a->epoch_nanoseconds, b->epoch_nanoseconds);
    JS_FreeValue(ctx, one);
    JS_FreeValue(ctx, two);
    return JS_NewInt32(ctx, result);
}

enum {
    ZDT_CALENDAR, ZDT_ZONE, ZDT_ERA, ZDT_ERA_YEAR, ZDT_YEAR, ZDT_MONTH,
    ZDT_MONTH_CODE, ZDT_DAY, ZDT_HOUR, ZDT_MINUTE, ZDT_SECOND, ZDT_MILLISECOND,
    ZDT_MICROSECOND, ZDT_NANOSECOND, ZDT_EPOCH_MS, ZDT_EPOCH_NS,
    ZDT_DAY_OF_WEEK, ZDT_DAY_OF_YEAR, ZDT_WEEK_OF_YEAR, ZDT_YEAR_OF_WEEK,
    ZDT_HOURS_IN_DAY, ZDT_DAYS_IN_WEEK, ZDT_DAYS_IN_MONTH, ZDT_DAYS_IN_YEAR,
    ZDT_MONTHS_IN_YEAR, ZDT_IN_LEAP_YEAR, ZDT_OFFSET_NS, ZDT_OFFSET
};

static int zdt_day_bounds(JSContext *ctx, const JSTemporalTimeZone *zone,
                            QJSTemporalISODate date, QJSTemporalEpochNs *start,
                            QJSTemporalEpochNs *end)
{
    QJSTemporalISODate tomorrow;
    int64_t days;

    if (qjs_temporal_iso_date_to_days(&days, date) ||
        qjs_temporal_iso_date_from_days(&tomorrow, days + 1))
        return js_temporal_calendar_error(ctx, QJS_TEMPORAL_ERROR_RANGE);
    return js_temporal_time_zone_start_of_day(ctx, zone, date, start) ||
           js_temporal_time_zone_start_of_day(ctx, zone, tomorrow, end) ? -1 : 0;
}

static JSValue js_temporal_zoned_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSTemporalZonedDateTimeData *s;
    QJSTemporalISODateTime dt;
    QJSTemporalCalendarDate date;
    QJSTemporalEpochNs start, end, difference;
    int64_t integer;
    char offset_string[26];

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (!s)
        return JS_EXCEPTION;
    if (magic == ZDT_CALENDAR)
        return JS_NewString(ctx, qjs_temporal_calendar_identifier(s->calendar));
    if (magic == ZDT_ZONE)
        return JS_DupValue(ctx, s->time_zone.identifier);
    if (magic == ZDT_EPOCH_NS)
        return JS_NewBigInt128(ctx, s->epoch_nanoseconds.low, s->epoch_nanoseconds.high);
    if (magic == ZDT_EPOCH_MS) {
        if (qjs_temporal_epoch_ns_to_milliseconds(&integer, s->epoch_nanoseconds))
            return JS_ThrowRangeError(ctx, "Temporal epoch is outside the valid range");
        return JS_NewInt64(ctx, integer);
    }
    if (magic == ZDT_OFFSET_NS || magic == ZDT_OFFSET) {
        if (js_temporal_time_zone_offset(ctx, &s->time_zone, s->epoch_nanoseconds, &integer))
            return JS_EXCEPTION;
        if (magic == ZDT_OFFSET_NS)
            return JS_NewInt64(ctx, integer);
        if (qjs_temporal_format_utc_offset(offset_string,
                                           sizeof(offset_string), integer, 0) < 0)
            return JS_ThrowRangeError(ctx, "invalid Temporal offset");
        return JS_NewString(ctx, offset_string);
    }
    if (js_temporal_time_zone_datetime(ctx, &s->time_zone, s->epoch_nanoseconds, &dt))
        return JS_EXCEPTION;
    switch (magic) {
    case ZDT_HOUR: return JS_NewInt32(ctx, dt.time.hour);
    case ZDT_MINUTE: return JS_NewInt32(ctx, dt.time.minute);
    case ZDT_SECOND: return JS_NewInt32(ctx, dt.time.second);
    case ZDT_MILLISECOND: return JS_NewInt32(ctx, dt.time.millisecond);
    case ZDT_MICROSECOND: return JS_NewInt32(ctx, dt.time.microsecond);
    case ZDT_NANOSECOND: return JS_NewInt32(ctx, dt.time.nanosecond);
    case ZDT_HOURS_IN_DAY:
        if (zdt_day_bounds(ctx, &s->time_zone, dt.date, &start, &end) ||
            qjs_temporal_epoch_ns_subtract(&difference, end, start) ||
            qjs_temporal_epoch_ns_to_int64(&integer, difference)) {
            if (!JS_HasException(ctx))
                JS_ThrowRangeError(ctx, "invalid Temporal day length");
            return JS_EXCEPTION;
        }
        return JS_NewFloat64(ctx, (double)integer / 3600000000000.0);
    }
    if (js_temporal_calendar_error(ctx,
            qjs_temporal_calendar_fields(s->calendar, dt.date, &date)))
        return JS_EXCEPTION;
    switch (magic) {
    case ZDT_ERA: return date.has_era ? JS_NewString(ctx, date.era) : JS_UNDEFINED;
    case ZDT_ERA_YEAR: return date.has_era ? JS_NewInt32(ctx, date.era_year) : JS_UNDEFINED;
    case ZDT_YEAR: integer = date.year; break;
    case ZDT_MONTH: integer = date.month; break;
    case ZDT_MONTH_CODE: return JS_NewString(ctx, date.month_code);
    case ZDT_DAY: integer = date.day; break;
    case ZDT_DAY_OF_WEEK: integer = date.day_of_week; break;
    case ZDT_DAY_OF_YEAR: integer = date.day_of_year; break;
    case ZDT_WEEK_OF_YEAR: return date.has_week ? JS_NewInt32(ctx, date.week_of_year) : JS_UNDEFINED;
    case ZDT_YEAR_OF_WEEK: return date.has_week ? JS_NewInt32(ctx, date.year_of_week) : JS_UNDEFINED;
    case ZDT_DAYS_IN_WEEK: integer = date.days_in_week; break;
    case ZDT_DAYS_IN_MONTH: integer = date.days_in_month; break;
    case ZDT_DAYS_IN_YEAR: integer = date.days_in_year; break;
    case ZDT_MONTHS_IN_YEAR: integer = date.months_in_year; break;
    default: return JS_NewBool(ctx, date.in_leap_year);
    }
    return JS_NewInt64(ctx, integer);
}

static void zdt_fields_set_time(JSTemporalFields *fields, QJSTemporalISOTime time)
{
    fields->time[0] = time.hour;
    fields->time[1] = time.minute;
    fields->time[2] = time.second;
    fields->time[3] = time.millisecond;
    fields->time[4] = time.microsecond;
    fields->time[5] = time.nanosecond;
    fields->present |= QJS_TEMPORAL_TIME_FIELDS;
}

static JSValue js_temporal_zoned_with(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    JSTemporalZonedDateTimeData *s;
    JSTemporalFields original, additional, merged;
    QJSTemporalISODateTime datetime;
    QJSTemporalEpochNs epoch;
    QJSTemporalDisambiguation disambiguation;
    JSTemporalOffsetOption offset_option;
    QJSTemporalOverflow overflow;
    int64_t offset;
    char offset_string[26];
    int partial;
    JSValue result = JS_EXCEPTION;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (!s)
        return JS_EXCEPTION;
    partial = js_temporal_is_partial_object(ctx, zdt_argument(argc, argv, 0));
    if (partial < 0)
        return JS_EXCEPTION;
    if (!partial)
        return JS_ThrowTypeError(ctx, "ZonedDateTime.with requires a partial Temporal object");
    if (js_temporal_time_zone_offset(ctx, &s->time_zone, s->epoch_nanoseconds, &offset) ||
        js_temporal_time_zone_datetime(ctx, &s->time_zone, s->epoch_nanoseconds, &datetime))
        return JS_EXCEPTION;
    if (js_temporal_iso_date_to_fields(ctx, s->calendar, datetime.date, &original)) {
        js_temporal_fields_free(ctx, &original);
        return JS_EXCEPTION;
    }
    zdt_fields_set_time(&original, datetime.time);
    if (qjs_temporal_format_utc_offset(offset_string,
                                       sizeof(offset_string), offset, 0) < 0) {
        JS_ThrowRangeError(ctx, "invalid Temporal offset");
        goto original_done;
    }
    original.offset = JS_NewString(ctx, offset_string);
    if (JS_IsException(original.offset)) {
        original.offset = JS_UNDEFINED;
        goto original_done;
    }
    original.present |= QJS_TEMPORAL_FIELD_OFFSET;
    if (js_temporal_prepare_calendar_fields(ctx, s->calendar, zdt_argument(argc, argv, 0),
            QJS_TEMPORAL_DATE_FIELDS, QJS_TEMPORAL_TIME_FIELDS | QJS_TEMPORAL_FIELD_OFFSET,
            0, TRUE, &additional)) {
        js_temporal_fields_free(ctx, &additional);
        goto original_done;
    }
    js_temporal_calendar_merge_fields(ctx, s->calendar, &original, &additional, &merged);
    if (!zdt_conversion_options(ctx, zdt_argument(argc, argv, 1), JS_TEMPORAL_OFFSET_PREFER,
            &disambiguation, &offset_option, &overflow) &&
        !js_temporal_interpret_datetime_fields(ctx, s->calendar, &merged, overflow, &datetime) &&
        !zdt_offset_nanoseconds(ctx, merged.offset, &offset) &&
        !js_temporal_interpret_offset(ctx, datetime, FALSE, JS_TEMPORAL_OFFSET_OPTION,
            offset, &s->time_zone, disambiguation, offset_option, FALSE, &epoch))
        result = js_temporal_create_zoned_date_time(ctx, JS_UNDEFINED, epoch,
                                                   &s->time_zone, s->calendar);
    js_temporal_fields_free(ctx, &merged);
    js_temporal_fields_free(ctx, &additional);
 original_done:
    js_temporal_fields_free(ctx, &original);
    return result;
}

static JSValue js_temporal_zoned_with_plain_time(JSContext *ctx, JSValueConst this_val,
                                                  int argc, JSValueConst *argv)
{
    JSTemporalZonedDateTimeData *s;
    JSTemporalPlainTimeData *time;
    QJSTemporalISODateTime datetime;
    QJSTemporalEpochNs epoch;
    JSValue converted, result;
    int ret;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (!s)
        return JS_EXCEPTION;
    if (js_temporal_time_zone_datetime(ctx, &s->time_zone, s->epoch_nanoseconds, &datetime))
        return JS_EXCEPTION;
    if (JS_IsUndefined(zdt_argument(argc, argv, 0))) {
        if (js_temporal_time_zone_start_of_day(ctx, &s->time_zone, datetime.date, &epoch))
            return JS_EXCEPTION;
    } else {
        converted = js_temporal_to_plain_time(ctx, argv[0], JS_UNDEFINED);
        if (JS_IsException(converted))
            return converted;
        time = JS_GetOpaque(converted, JS_CLASS_TEMPORAL_PLAIN_TIME);
        datetime.time = *time;
        ret = js_temporal_time_zone_epoch(ctx, &s->time_zone, datetime,
                                          QJS_TEMPORAL_COMPATIBLE, &epoch);
        JS_FreeValue(ctx, converted);
        if (ret)
            return JS_EXCEPTION;
    }
    result = js_temporal_create_zoned_date_time(ctx, JS_UNDEFINED, epoch,
                                               &s->time_zone, s->calendar);
    return result;
}

static JSValue js_temporal_zoned_with_zone(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv)
{
    JSTemporalZonedDateTimeData *s;
    JSTemporalTimeZone zone;
    JSValue result;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (!s)
        return JS_EXCEPTION;
    if (js_temporal_to_time_zone(ctx, zdt_argument(argc, argv, 0), &zone))
        return JS_EXCEPTION;
    result = js_temporal_create_zoned_date_time(ctx, JS_UNDEFINED, s->epoch_nanoseconds,
                                               &zone, s->calendar);
    js_temporal_free_time_zone(ctx, &zone);
    return result;
}

static JSValue js_temporal_zoned_with_calendar(JSContext *ctx, JSValueConst this_val,
                                               int argc, JSValueConst *argv)
{
    JSTemporalZonedDateTimeData *s;
    QJSTemporalCalendar calendar;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (!s)
        return JS_EXCEPTION;
    if (js_temporal_to_calendar(ctx, zdt_argument(argc, argv, 0), &calendar))
        return JS_EXCEPTION;
    return js_temporal_create_zoned_date_time(ctx, JS_UNDEFINED, s->epoch_nanoseconds,
                                              &s->time_zone, calendar);
}

static JSValue js_temporal_zoned_add(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int subtract)
{
    static const char *const overflows[] = { "constrain", "reject" };
    JSTemporalZonedDateTimeData *s;
    JSTemporalDurationData *duration;
    QJSTemporalDuration copy;
    QJSTemporalInternalDuration internal;
    QJSTemporalEpochNs epoch;
    JSValue converted, options;
    int overflow, i, ret;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (!s)
        return JS_EXCEPTION;
    converted = js_temporal_to_duration(ctx, zdt_argument(argc, argv, 0));
    if (JS_IsException(converted))
        return converted;
    duration = JS_GetOpaque(converted, JS_CLASS_TEMPORAL_DURATION);
    copy = *duration;
    JS_FreeValue(ctx, converted);
    if (subtract) {
        for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++)
            copy.fields[i] = -copy.fields[i];
    }
    options = js_temporal_get_options(ctx, zdt_argument(argc, argv, 1));
    if (JS_IsException(options))
        return options;
    ret = js_temporal_get_string_option(ctx, options, "overflow", overflows,
            countof(overflows), QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &overflow);
    JS_FreeValue(ctx, options);
    if (ret)
        return JS_EXCEPTION;
    if (qjs_temporal_duration_normalize(&internal, &copy, 0))
        return JS_ThrowRangeError(ctx, "Temporal duration is outside the valid range");
    if (js_temporal_zoned_add_duration(ctx, s, internal, overflow, &epoch))
        return JS_EXCEPTION;
    return js_temporal_create_zoned_date_time(ctx, JS_UNDEFINED, epoch,
                                               &s->time_zone, s->calendar);
}

static JSValue js_temporal_zoned_difference(JSContext *ctx, JSValueConst this_val,
                                             int argc, JSValueConst *argv, int since)
{
    JSTemporalZonedDateTimeData *s, *other;
    QJSTemporalDifferenceSettings settings;
    QJSTemporalInternalDuration internal = { 0 };
    QJSTemporalDuration duration;
    QJSTemporalEpochNs difference;
    QJSTemporalUnit balance_unit;
    JSValue converted, options, result = JS_EXCEPTION;
    uint64_t increment;
    int equal, i, ret;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (!s)
        return JS_EXCEPTION;
    converted = js_temporal_to_zoned_date_time(ctx, zdt_argument(argc, argv, 0), JS_UNDEFINED);
    if (JS_IsException(converted))
        return converted;
    other = JS_GetOpaque(converted, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (s->calendar != other->calendar) {
        JS_ThrowRangeError(ctx, "ZonedDateTime calendars must match for a difference");
        goto done;
    }
    options = js_temporal_get_options(ctx, zdt_argument(argc, argv, 1));
    if (JS_IsException(options))
        goto done;
    ret = js_temporal_get_difference_settings(ctx, options, since,
            QJS_TEMPORAL_YEAR, QJS_TEMPORAL_NANOSECOND,
            QJS_TEMPORAL_NANOSECOND, QJS_TEMPORAL_HOUR, &settings);
    JS_FreeValue(ctx, options);
    if (ret)
        goto done;
    if (settings.largest_unit >= QJS_TEMPORAL_HOUR) {
        increment = qjs_temporal_unit_nanoseconds(settings.smallest_unit) *
                    settings.rounding_increment;
        if (qjs_temporal_epoch_ns_subtract(&difference, other->epoch_nanoseconds,
                                           s->epoch_nanoseconds) ||
            qjs_temporal_epoch_ns_round(&internal.time, difference, increment,
                                         settings.rounding_mode)) {
            JS_ThrowRangeError(ctx, "Temporal difference is outside the valid range");
            goto done;
        }
        balance_unit = settings.largest_unit;
    } else {
        if (js_temporal_time_zones_equal(ctx, &s->time_zone, &other->time_zone, &equal))
            goto done;
        if (!equal) {
            JS_ThrowRangeError(ctx, "ZonedDateTime zones must match for a date difference");
            goto done;
        }
        if (qjs_temporal_epoch_ns_compare(s->epoch_nanoseconds, other->epoch_nanoseconds) &&
            js_temporal_zoned_difference_round(ctx, s->epoch_nanoseconds,
                other->epoch_nanoseconds, &s->time_zone, s->calendar, &settings, &internal))
            goto done;
        balance_unit = QJS_TEMPORAL_HOUR;
    }
    if (qjs_temporal_duration_from_internal(&duration, internal, balance_unit)) {
        JS_ThrowRangeError(ctx, "Temporal duration is outside the valid range");
        goto done;
    }
    if (since) {
        for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++)
            duration.fields[i] = -duration.fields[i];
    }
    result = js_temporal_create_duration(ctx, JS_UNDEFINED, &duration);
 done:
    JS_FreeValue(ctx, converted);
    return result;
}

static JSValue zdt_shorthand_options(JSContext *ctx, JSValueConst value,
                                      const char *property)
{
    JSValue options;
    if (!JS_IsString(value))
        return js_temporal_get_options(ctx, value);
    options = JS_NewObjectProto(ctx, JS_NULL);
    if (JS_IsException(options))
        return options;
    if (JS_DefinePropertyValueStr(ctx, options, property, JS_DupValue(ctx, value),
                                   JS_PROP_C_W_E) < 0) {
        JS_FreeValue(ctx, options);
        return JS_EXCEPTION;
    }
    return options;
}

static JSValue js_temporal_zoned_round(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    JSTemporalZonedDateTimeData *s;
    QJSTemporalISODateTime datetime, midnight;
    QJSTemporalEpochNs epoch, start, end, difference, rounded, wall;
    QJSTemporalUnit unit;
    QJSTemporalRoundingMode mode;
    uint32_t increment;
    uint64_t maximum, quantum;
    int64_t length, offset, time_ns;
    JSValue options;
    int ret;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (!s)
        return JS_EXCEPTION;
    if (JS_IsUndefined(zdt_argument(argc, argv, 0)))
        return JS_ThrowTypeError(ctx, "ZonedDateTime.round requires options");
    options = zdt_shorthand_options(ctx, argv[0], "smallestUnit");
    if (JS_IsException(options))
        return options;
    ret = js_temporal_get_rounding_increment(ctx, options, &increment) ||
          js_temporal_get_rounding_mode(ctx, options, QJS_TEMPORAL_ROUND_HALF_EXPAND, &mode) ||
          js_temporal_get_unit_option(ctx, options, "smallestUnit", TRUE, &unit);
    JS_FreeValue(ctx, options);
    if (ret)
        return JS_EXCEPTION;
    if (unit < QJS_TEMPORAL_DAY || unit > QJS_TEMPORAL_NANOSECOND)
        return JS_ThrowRangeError(ctx, "invalid ZonedDateTime rounding unit");
    maximum = unit == QJS_TEMPORAL_DAY ? 1 :
              unit == QJS_TEMPORAL_HOUR ? 24 :
              unit <= QJS_TEMPORAL_SECOND ? 60 : 1000;
    if (js_temporal_validate_increment(ctx, increment, maximum, unit == QJS_TEMPORAL_DAY))
        return JS_EXCEPTION;
    if (unit == QJS_TEMPORAL_NANOSECOND && increment == 1)
        return js_temporal_create_zoned_date_time(ctx, JS_UNDEFINED, s->epoch_nanoseconds,
                                                   &s->time_zone, s->calendar);
    if (js_temporal_time_zone_datetime(ctx, &s->time_zone, s->epoch_nanoseconds, &datetime))
        return JS_EXCEPTION;
    if (unit == QJS_TEMPORAL_DAY) {
        if (zdt_day_bounds(ctx, &s->time_zone, datetime.date, &start, &end))
            return JS_EXCEPTION;
        if (qjs_temporal_epoch_ns_subtract(&difference, end, start) ||
            qjs_temporal_epoch_ns_to_int64(&length, difference) || length <= 0 ||
            qjs_temporal_epoch_ns_subtract(&difference, s->epoch_nanoseconds, start) ||
            qjs_temporal_epoch_ns_round(&rounded, difference, length, mode) ||
            qjs_temporal_epoch_ns_add(&epoch, start, rounded))
            return JS_ThrowRangeError(ctx, "invalid Temporal day rounding");
    } else {
        /* Round the nonnegative wall-clock time, then balance its day carry.
           Rounding a negative civil epoch directly changes trunc/half modes. */
        time_ns = (((int64_t)datetime.time.hour * 60 + datetime.time.minute) * 60 +
                   datetime.time.second) * INT64_C(1000000000) +
                  (int64_t)datetime.time.millisecond * 1000000 +
                  (int64_t)datetime.time.microsecond * 1000 + datetime.time.nanosecond;
        quantum = qjs_temporal_unit_nanoseconds(unit) * increment;
        midnight = datetime;
        memset(&midnight.time, 0, sizeof(midnight.time));
        if (qjs_temporal_epoch_ns_round(&rounded, qjs_temporal_epoch_ns_from_int64(time_ns),
                                         quantum, mode) ||
            qjs_temporal_iso_datetime_to_epoch_ns(&wall, midnight) ||
            qjs_temporal_epoch_ns_add(&wall, wall, rounded) ||
            qjs_temporal_iso_datetime_from_epoch_ns(&datetime, wall))
            return JS_ThrowRangeError(ctx, "invalid Temporal civil rounding");
        if (js_temporal_time_zone_offset(ctx, &s->time_zone, s->epoch_nanoseconds, &offset) ||
            js_temporal_interpret_offset(ctx, datetime, FALSE, JS_TEMPORAL_OFFSET_OPTION,
                offset, &s->time_zone, QJS_TEMPORAL_COMPATIBLE, JS_TEMPORAL_OFFSET_PREFER,
                FALSE, &epoch))
            return JS_EXCEPTION;
    }
    return js_temporal_create_zoned_date_time(ctx, JS_UNDEFINED, epoch,
                                               &s->time_zone, s->calendar);
}

static JSValue js_temporal_zoned_equals(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    JSTemporalZonedDateTimeData *s, *other;
    JSValue converted;
    int equal, ret = 0;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (!s)
        return JS_EXCEPTION;
    converted = js_temporal_to_zoned_date_time(ctx, zdt_argument(argc, argv, 0), JS_UNDEFINED);
    if (JS_IsException(converted))
        return converted;
    other = JS_GetOpaque(converted, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    equal = !qjs_temporal_epoch_ns_compare(s->epoch_nanoseconds, other->epoch_nanoseconds);
    if (equal)
        ret = js_temporal_time_zones_equal(ctx, &s->time_zone, &other->time_zone, &equal);
    if (equal)
        equal = s->calendar == other->calendar;
    JS_FreeValue(ctx, converted);
    return ret ? JS_EXCEPTION : JS_NewBool(ctx, equal);
}

static JSValue zdt_format(JSContext *ctx, const JSTemporalZonedDateTimeData *s,
    int precision, int show_calendar, int show_zone, int show_offset,
    uint64_t quantum, QJSTemporalRoundingMode mode)
{
    QJSTemporalEpochNs epoch;
    QJSTemporalISODateTime datetime;
    int64_t offset;
    const char *zone = NULL, *calendar;
    char date_text[64], offset_text[26], zone_text[300], calendar_text[64], output[512];
    int length;
    JSValue result;

    if (qjs_temporal_epoch_ns_round_as_if_positive(&epoch, s->epoch_nanoseconds,
                                                   quantum, mode) ||
        !qjs_temporal_epoch_ns_is_valid(epoch))
        return JS_ThrowRangeError(ctx, "Temporal rounded epoch is outside the valid range");
    if (js_temporal_time_zone_offset(ctx, &s->time_zone, epoch, &offset) ||
        js_temporal_time_zone_datetime(ctx, &s->time_zone, epoch, &datetime))
        return JS_EXCEPTION;
    if (qjs_temporal_format_date_time(date_text, sizeof(date_text), datetime, precision) < 0)
        return JS_ThrowRangeError(ctx, "invalid Temporal date-time formatting");
    offset_text[0] = zone_text[0] = calendar_text[0] = 0;
    if (!show_offset && qjs_temporal_format_utc_offset(offset_text,
                                                       sizeof(offset_text), offset, 1) < 0)
        return JS_ThrowRangeError(ctx, "invalid Temporal offset formatting");
    if (show_zone != 1) {
        zone = JS_ToCString(ctx, s->time_zone.identifier);
        if (!zone)
            return JS_EXCEPTION;
        length = snprintf(zone_text, sizeof(zone_text), "[%s%s]", show_zone == 2 ? "!" : "", zone);
        JS_FreeCString(ctx, zone);
        if (length < 0 || length >= (int)sizeof(zone_text))
            return JS_ThrowRangeError(ctx, "Temporal zone identifier is too long");
    }
    if (show_calendar != 2 && (show_calendar || s->calendar != QJS_TEMPORAL_CAL_ISO8601)) {
        calendar = qjs_temporal_calendar_identifier(s->calendar);
        length = snprintf(calendar_text, sizeof(calendar_text), "[%su-ca=%s]",
                            show_calendar == 3 ? "!" : "", calendar);
        if (length < 0 || length >= (int)sizeof(calendar_text))
            return JS_ThrowRangeError(ctx, "Temporal calendar identifier is too long");
    }
    length = snprintf(output, sizeof(output), "%s%s%s%s", date_text, offset_text,
                                                         zone_text, calendar_text);
    if (length < 0 || length >= (int)sizeof(output))
        return JS_ThrowRangeError(ctx, "Temporal string is too long");
    result = JS_NewStringLen(ctx, output, length);
    return result;
}

static JSValue js_temporal_zoned_to_string(JSContext *ctx, JSValueConst this_val,
                                            int argc, JSValueConst *argv)
{
    static const char *const calendars[] = { "auto", "always", "never", "critical" };
    static const char *const offsets[] = { "auto", "never" };
    static const char *const zones[] = { "auto", "never", "critical" };
    JSTemporalZonedDateTimeData *s;
    QJSTemporalStringPrecision precision;
    QJSTemporalUnit unit;
    QJSTemporalRoundingMode mode;
    JSValue options;
    int show_calendar, digits, show_offset, show_zone, ret;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (!s)
        return JS_EXCEPTION;
    options = js_temporal_get_options(ctx, zdt_argument(argc, argv, 0));
    if (JS_IsException(options))
        return options;
    ret = js_temporal_get_string_option(ctx, options, "calendarName", calendars,
                countof(calendars), 0, &show_calendar) ||
          js_temporal_get_fractional_digits(ctx, options, &digits) ||
          js_temporal_get_string_option(ctx, options, "offset", offsets,
                countof(offsets), 0, &show_offset) ||
          js_temporal_get_rounding_mode(ctx, options, QJS_TEMPORAL_ROUND_TRUNC, &mode) ||
          js_temporal_get_unit_option(ctx, options, "smallestUnit", FALSE, &unit) ||
          js_temporal_get_string_option(ctx, options, "timeZoneName", zones,
                countof(zones), 0, &show_zone);
    JS_FreeValue(ctx, options);
    if (ret)
        return JS_EXCEPTION;
    if ((unit != QJS_TEMPORAL_UNIT_UNSET &&
        (unit < QJS_TEMPORAL_MINUTE || unit > QJS_TEMPORAL_NANOSECOND)) ||
        qjs_temporal_string_precision(&precision, unit, digits))
        return JS_ThrowRangeError(ctx, "invalid ZonedDateTime string precision");
    return zdt_format(ctx, s, precision.precision, show_calendar, show_zone, show_offset,
            qjs_temporal_unit_nanoseconds(precision.unit) * precision.increment, mode);
}

static JSValue js_temporal_zoned_to_json(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv)
{
    JSTemporalZonedDateTimeData *s;
    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (!s)
        return JS_EXCEPTION;
    return zdt_format(ctx, s, -1, 0, 0, 0, 1, QJS_TEMPORAL_ROUND_TRUNC);
}

static JSValue js_temporal_zoned_to_locale_string(JSContext *ctx, JSValueConst this_val,
                                                   int argc, JSValueConst *argv)
{
    JSTemporalZonedDateTimeData *s;
    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (!s)
        return JS_EXCEPTION;
#ifdef CONFIG_INTL
    return js_intl_temporal_to_locale_string(ctx, this_val,
        zdt_argument(argc, argv, 0), zdt_argument(argc, argv, 1));
#else
    return zdt_format(ctx, s, -1, 0, 0, 0, 1, QJS_TEMPORAL_ROUND_TRUNC);
#endif
}

static JSValue js_temporal_zoned_value_of(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv)
{
    return JS_ThrowTypeError(ctx, "Temporal.ZonedDateTime cannot be converted to a number");
}

static JSValue js_temporal_zoned_start_of_day(JSContext *ctx, JSValueConst this_val,
                                               int argc, JSValueConst *argv)
{
    JSTemporalZonedDateTimeData *s;
    QJSTemporalISODateTime datetime;
    QJSTemporalEpochNs epoch;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (!s)
        return JS_EXCEPTION;
    if (js_temporal_time_zone_datetime(ctx, &s->time_zone, s->epoch_nanoseconds, &datetime) ||
        js_temporal_time_zone_start_of_day(ctx, &s->time_zone, datetime.date, &epoch))
        return JS_EXCEPTION;
    return js_temporal_create_zoned_date_time(ctx, JS_UNDEFINED, epoch,
                                               &s->time_zone, s->calendar);
}

static JSValue js_temporal_zoned_transition(JSContext *ctx, JSValueConst this_val,
                                             int argc, JSValueConst *argv)
{
    static const char *const directions[] = { "next", "previous" };
    JSTemporalZonedDateTimeData *s;
    QJSTemporalEpochNs epoch;
    JSValue options;
    int direction, found, ret;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (!s)
        return JS_EXCEPTION;
    if (JS_IsUndefined(zdt_argument(argc, argv, 0)))
        return JS_ThrowTypeError(ctx, "getTimeZoneTransition requires a direction");
    options = zdt_shorthand_options(ctx, argv[0], "direction");
    if (JS_IsException(options))
        return options;
    ret = js_temporal_get_string_option(ctx, options, "direction", directions,
                                          countof(directions), -1, &direction);
    JS_FreeValue(ctx, options);
    if (ret)
        return JS_EXCEPTION;
    if (direction < 0)
        return JS_ThrowRangeError(ctx, "Temporal transition direction is required");
    if (js_temporal_time_zone_transition(ctx, &s->time_zone, s->epoch_nanoseconds,
                                          direction == 0, &epoch, &found))
        return JS_EXCEPTION;
    if (!found)
        return JS_NULL;
    return js_temporal_create_zoned_date_time(ctx, JS_UNDEFINED, epoch,
                                               &s->time_zone, s->calendar);
}

static JSValue js_temporal_zoned_to_plain(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv, int target)
{
    JSTemporalZonedDateTimeData *s;
    QJSTemporalISODateTime datetime;

    s = JS_GetOpaque2(ctx, this_val, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
    if (!s)
        return JS_EXCEPTION;
    if (target == 0)
        return js_temporal_create_instant(ctx, JS_UNDEFINED, s->epoch_nanoseconds);
    if (js_temporal_time_zone_datetime(ctx, &s->time_zone, s->epoch_nanoseconds, &datetime))
        return JS_EXCEPTION;
    if (target == 1)
        return js_temporal_create_plain_date(ctx, JS_UNDEFINED, datetime.date, s->calendar);
    if (target == 2)
        return js_temporal_create_plain_time(ctx, JS_UNDEFINED, datetime.time);
    return js_temporal_create_plain_date_time(ctx, JS_UNDEFINED, datetime, s->calendar);
}

static const JSCFunctionListEntry js_temporal_zoned_funcs[] = {
    JS_CFUNC_DEF("from", 1, js_temporal_zoned_from),
    JS_CFUNC_DEF("compare", 2, js_temporal_zoned_compare),
};

static const JSCFunctionListEntry js_temporal_zoned_proto_funcs[] = {
#define ZDT_GET(name, magic) JS_CGETSET_MAGIC_DEF(name, js_temporal_zoned_get, NULL, magic)
    ZDT_GET("calendarId", ZDT_CALENDAR),
    ZDT_GET("timeZoneId", ZDT_ZONE),
    ZDT_GET("era", ZDT_ERA),
    ZDT_GET("eraYear", ZDT_ERA_YEAR),
    ZDT_GET("year", ZDT_YEAR),
    ZDT_GET("month", ZDT_MONTH),
    ZDT_GET("monthCode", ZDT_MONTH_CODE),
    ZDT_GET("day", ZDT_DAY),
    ZDT_GET("hour", ZDT_HOUR),
    ZDT_GET("minute", ZDT_MINUTE),
    ZDT_GET("second", ZDT_SECOND),
    ZDT_GET("millisecond", ZDT_MILLISECOND),
    ZDT_GET("microsecond", ZDT_MICROSECOND),
    ZDT_GET("nanosecond", ZDT_NANOSECOND),
    ZDT_GET("epochMilliseconds", ZDT_EPOCH_MS),
    ZDT_GET("epochNanoseconds", ZDT_EPOCH_NS),
    ZDT_GET("dayOfWeek", ZDT_DAY_OF_WEEK),
    ZDT_GET("dayOfYear", ZDT_DAY_OF_YEAR),
    ZDT_GET("weekOfYear", ZDT_WEEK_OF_YEAR),
    ZDT_GET("yearOfWeek", ZDT_YEAR_OF_WEEK),
    ZDT_GET("hoursInDay", ZDT_HOURS_IN_DAY),
    ZDT_GET("daysInWeek", ZDT_DAYS_IN_WEEK),
    ZDT_GET("daysInMonth", ZDT_DAYS_IN_MONTH),
    ZDT_GET("daysInYear", ZDT_DAYS_IN_YEAR),
    ZDT_GET("monthsInYear", ZDT_MONTHS_IN_YEAR),
    ZDT_GET("inLeapYear", ZDT_IN_LEAP_YEAR),
    ZDT_GET("offsetNanoseconds", ZDT_OFFSET_NS),
    ZDT_GET("offset", ZDT_OFFSET),
#undef ZDT_GET
    JS_CFUNC_DEF("with", 1, js_temporal_zoned_with),
    JS_CFUNC_DEF("withPlainTime", 0, js_temporal_zoned_with_plain_time),
    JS_CFUNC_DEF("withTimeZone", 1, js_temporal_zoned_with_zone),
    JS_CFUNC_DEF("withCalendar", 1, js_temporal_zoned_with_calendar),
    JS_CFUNC_MAGIC_DEF("add", 1, js_temporal_zoned_add, 0),
    JS_CFUNC_MAGIC_DEF("subtract", 1, js_temporal_zoned_add, 1),
    JS_CFUNC_MAGIC_DEF("until", 1, js_temporal_zoned_difference, 0),
    JS_CFUNC_MAGIC_DEF("since", 1, js_temporal_zoned_difference, 1),
    JS_CFUNC_DEF("round", 1, js_temporal_zoned_round),
    JS_CFUNC_DEF("equals", 1, js_temporal_zoned_equals),
    JS_CFUNC_DEF("toString", 0, js_temporal_zoned_to_string),
    JS_CFUNC_DEF("toLocaleString", 0, js_temporal_zoned_to_locale_string),
    JS_CFUNC_DEF("toJSON", 0, js_temporal_zoned_to_json),
    JS_CFUNC_DEF("valueOf", 0, js_temporal_zoned_value_of),
    JS_CFUNC_DEF("startOfDay", 0, js_temporal_zoned_start_of_day),
    JS_CFUNC_DEF("getTimeZoneTransition", 1, js_temporal_zoned_transition),
    JS_CFUNC_MAGIC_DEF("toInstant", 0, js_temporal_zoned_to_plain, 0),
    JS_CFUNC_MAGIC_DEF("toPlainDate", 0, js_temporal_zoned_to_plain, 1),
    JS_CFUNC_MAGIC_DEF("toPlainTime", 0, js_temporal_zoned_to_plain, 2),
    JS_CFUNC_MAGIC_DEF("toPlainDateTime", 0, js_temporal_zoned_to_plain, 3),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Temporal.ZonedDateTime", JS_PROP_CONFIGURABLE),
};

int js_temporal_init_zoned_date_time(JSContext *ctx, JSValueConst namespace_object)
{
    JSValue ctor;

    if (!JS_IsRegisteredClass(ctx->rt, JS_CLASS_TEMPORAL_ZONED_DATE_TIME) &&
        JS_NewClass(ctx->rt, JS_CLASS_TEMPORAL_ZONED_DATE_TIME, &js_temporal_zoned_class) < 0) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    ctor = JS_NewCConstructor(ctx, JS_CLASS_TEMPORAL_ZONED_DATE_TIME, "ZonedDateTime",
        js_temporal_zoned_constructor, 2, JS_CFUNC_constructor, 0, JS_UNDEFINED,
        js_temporal_zoned_funcs, countof(js_temporal_zoned_funcs),
        js_temporal_zoned_proto_funcs, countof(js_temporal_zoned_proto_funcs),
        JS_NEW_CTOR_NO_GLOBAL);
    if (JS_IsException(ctor))
        return -1;
    if (JS_HasException(ctx)) {
        JS_FreeValue(ctx, ctor);
        return -1;
    }
    return JS_DefinePropertyValueStr(ctx, namespace_object, "ZonedDateTime", ctor,
        JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE | JS_PROP_THROW) < 0 ? -1 : 0;
}
