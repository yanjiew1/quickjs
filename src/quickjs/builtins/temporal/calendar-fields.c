/* Observable Temporal field coercion, calendar and zone identifiers.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE. */
#include "calendar-fields.h"
#include <string.h>

static void fields_init(JSTemporalFields *fields)
{
    memset(fields, 0, sizeof(*fields));
    fields->offset = JS_UNDEFINED;
    fields->zone.identifier = JS_UNDEFINED;
}
void js_temporal_fields_free(JSContext *ctx, JSTemporalFields *fields)
{
    JS_FreeValue(ctx, fields->offset);
    js_temporal_free_time_zone(ctx, &fields->zone);
    fields_init(fields);
}
int js_temporal_calendar_error(JSContext *ctx, int error)
{
    if (!error) return 0;
    if (error == QJS_TEMPORAL_ERROR_MEMORY) JS_ThrowOutOfMemory(ctx);
    else if (error == QJS_TEMPORAL_ERROR_MISSING)
        JS_ThrowTypeError(ctx, "missing Temporal calendar field");
    else if (error == QJS_TEMPORAL_ERROR_BACKEND)
        JS_ThrowInternalError(ctx, "Temporal calendar or time-zone backend failed");
    else JS_ThrowRangeError(ctx, "invalid Temporal calendar or time-zone value");
    return -1;
}
int js_temporal_calendar_date_add(
    JSContext *ctx, QJSTemporalCalendar calendar, QJSTemporalISODate date,
    QJSTemporalDateDuration duration, QJSTemporalOverflow overflow,
    QJSTemporalISODate *result)
{
    return js_temporal_calendar_error(ctx,
        qjs_temporal_calendar_date_add(calendar, date, duration, overflow,
                                      result));
}
static int primitive_string(JSContext *ctx, JSValueConst value, JSValue *result)
{
    JSValue string = JS_ToPrimitive(ctx, value, HINT_STRING);
    if (JS_IsException(string)) return -1;
    if (!JS_IsString(string)) {
        JS_FreeValue(ctx, string);
        JS_ThrowTypeError(ctx, "Temporal field must convert to a string");
        return -1;
    }
    *result = string;
    return 0;
}
static int month_code(JSContext *ctx, JSValueConst value, char result[5])
{
    JSValue string;
    const char *text;
    size_t length;
    int valid;
    if (primitive_string(ctx, value, &string)) return -1;
    text = JS_ToCStringLen(ctx, &length, string);
    JS_FreeValue(ctx, string);
    if (!text) return -1;
    valid = (length == 3 || length == 4) && text[0] == 'M' &&
        text[1] >= '0' && text[1] <= '9' && text[2] >= '0' && text[2] <= '9' &&
        (length == 4 ? text[3] == 'L' : text[1] != '0' || text[2] != '0');
    if (valid) { memcpy(result, text, length); result[length] = 0; }
    JS_FreeCString(ctx, text);
    if (!valid) { JS_ThrowRangeError(ctx, "invalid Temporal monthCode"); return -1; }
    return 0;
}
static int offset_string(JSContext *ctx, JSValueConst value, JSValue *result)
{
    JSValue string;
    const char *text;
    size_t length, consumed;
    QJSTemporalUTCOffset offset;
    int valid;
    if (primitive_string(ctx, value, &string)) return -1;
    text = JS_ToCStringLen(ctx, &length, string);
    if (!text) { JS_FreeValue(ctx, string); return -1; }
    valid = !qjs_temporal_parse_utc_offset_prefix(&offset, &consumed,
                     text, length, QJS_TEMPORAL_OFFSET_ALLOW_SECONDS) && consumed == length;
    JS_FreeCString(ctx, text);
    if (!valid) {
        JS_FreeValue(ctx, string);
        JS_ThrowRangeError(ctx, "invalid Temporal offset"); return -1;
    }
    *result = string;
    return 0;
}
int js_temporal_prepare_calendar_fields(JSContext *ctx,
    QJSTemporalCalendar calendar, JSValueConst object,
    unsigned calendar_names, unsigned non_calendar_names,
    unsigned required_names, BOOL partial, JSTemporalFields *result)
{
    static const char *const names[] = {
        "day", "era", "eraYear", "hour", "microsecond", "millisecond",
        "minute", "month", "monthCode", "nanosecond", "offset", "second",
        "timeZone", "year"
    };
    static const int time_index[] = {-1,-1,-1,0,4,3,1,-1,-1,5,-1,2,-1,-1};
    unsigned requested = calendar_names | non_calendar_names |
        qjs_temporal_calendar_extra_fields(calendar, calendar_names);
    int i, any = 0;
    JSValue value, string;
    const char *text;
    size_t length;
    double number;
    fields_init(result);
    for (i = 0; i < (int)countof(names); i++) {
        unsigned bit = 1U << i;
        if (!(requested & bit)) continue;
        value = JS_GetPropertyStr(ctx, object, names[i]);
        if (JS_IsException(value)) return -1;
        if (JS_IsUndefined(value)) {
            JS_FreeValue(ctx, value);
            if (!partial && (required_names & bit)) {
                JS_ThrowTypeError(ctx, "missing Temporal %s field", names[i]); return -1;
            }
            if (!partial && time_index[i] >= 0) result->present |= bit;
            continue;
        }
        any = 1;
        switch (i) {
        case 1:
            string = JS_ToString(ctx, value);
            if (JS_IsException(string)) goto failed;
            text = JS_ToCStringLen(ctx, &length, string);
            JS_FreeValue(ctx, string);
            if (!text) goto failed;
            /* Unknown long / embedded-NUL eras fail only at resolution,
               after every sorted Get. They must not be truncated to a
               valid era or cause an early validation exception. */
            if (length < sizeof(result->calendar.era) && !memchr(text, 0, length)) {
                memcpy(result->calendar.era, text, length);
                result->calendar.era[length] = 0;
            }
            JS_FreeCString(ctx, text);
            break;
        case 8:
            if (month_code(ctx, value, result->calendar.month_code)) goto failed;
            break;
        case 10:
            if (offset_string(ctx, value, &result->offset)) goto failed;
            break;
        case 12:
            if (js_temporal_to_time_zone(ctx, value, &result->zone)) goto failed;
            break;
        default:
            if (js_temporal_to_integer(ctx, value, &number)) goto failed;
            if ((i == 0 || i == 7) && number <= 0) {
                JS_ThrowRangeError(ctx, "Temporal %s must be positive", names[i]); goto failed;
            }
            if (i == 0) result->calendar.day = number;
            else if (i == 2) result->calendar.era_year = number;
            else if (i == 7) result->calendar.month = number;
            else if (i == 13) result->calendar.year = number;
            else result->time[time_index[i]] = number;
            break;
        }
        result->present |= bit;
        result->calendar.present = result->present &
            (QJS_TEMPORAL_DATE_FIELDS | QJS_TEMPORAL_FIELD_ERA | QJS_TEMPORAL_FIELD_ERA_YEAR);
        JS_FreeValue(ctx, value);
    }
    if (partial && !any) {
        JS_ThrowTypeError(ctx, "Temporal fields must contain a defined property"); return -1;
    }
    return 0;
 failed:
    JS_FreeValue(ctx, value);
    return -1;
}
int js_temporal_iso_date_to_fields(JSContext *ctx,
    QJSTemporalCalendar calendar, QJSTemporalISODate date,
    JSTemporalFields *result)
{
    QJSTemporalCalendarDate record;
    int error;
    fields_init(result);
    error = qjs_temporal_calendar_fields(calendar, date, &record);
    if (js_temporal_calendar_error(ctx, error)) return -1;
    result->calendar.year = record.year;
    result->calendar.day = record.day;
    strcpy(result->calendar.month_code, record.month_code);
    result->present = result->calendar.present = QJS_TEMPORAL_FIELD_YEAR |
        QJS_TEMPORAL_FIELD_MONTH_CODE | QJS_TEMPORAL_FIELD_DAY;
    return 0;
}
void js_temporal_calendar_merge_fields(JSContext *ctx,
    QJSTemporalCalendar calendar, const JSTemporalFields *original,
    const JSTemporalFields *additional, JSTemporalFields *result)
{
    unsigned kept = original->present &
        ~qjs_temporal_calendar_fields_to_ignore(calendar, additional->present);
    static const unsigned time_bits[] = {QJS_TEMPORAL_FIELD_HOUR,
        QJS_TEMPORAL_FIELD_MINUTE, QJS_TEMPORAL_FIELD_SECOND,
        QJS_TEMPORAL_FIELD_MILLISECOND, QJS_TEMPORAL_FIELD_MICROSECOND,
        QJS_TEMPORAL_FIELD_NANOSECOND};
    fields_init(result);
    result->present = kept | additional->present;
#define COPY_NUMBER(member, bit) do { \
    if (additional->present & (bit)) result->calendar.member = additional->calendar.member; \
    else if (kept & (bit)) result->calendar.member = original->calendar.member; \
} while (0)
    COPY_NUMBER(year, QJS_TEMPORAL_FIELD_YEAR);
    COPY_NUMBER(month, QJS_TEMPORAL_FIELD_MONTH);
    COPY_NUMBER(day, QJS_TEMPORAL_FIELD_DAY);
    COPY_NUMBER(era_year, QJS_TEMPORAL_FIELD_ERA_YEAR);
#undef COPY_NUMBER
    if (additional->present & QJS_TEMPORAL_FIELD_MONTH_CODE)
        strcpy(result->calendar.month_code, additional->calendar.month_code);
    else if (kept & QJS_TEMPORAL_FIELD_MONTH_CODE)
        strcpy(result->calendar.month_code, original->calendar.month_code);
    if (additional->present & QJS_TEMPORAL_FIELD_ERA)
        strcpy(result->calendar.era, additional->calendar.era);
    else if (kept & QJS_TEMPORAL_FIELD_ERA)
        strcpy(result->calendar.era, original->calendar.era);
    for (int i = 0; i < 6; i++)
        result->time[i] = additional->present & time_bits[i] ? additional->time[i] :
            kept & time_bits[i] ? original->time[i] : 0;
    if (result->present & QJS_TEMPORAL_FIELD_OFFSET)
        result->offset = JS_DupValue(ctx, additional->present & QJS_TEMPORAL_FIELD_OFFSET ?
                                    additional->offset : original->offset);
    if (result->present & QJS_TEMPORAL_FIELD_TIME_ZONE) {
        const JSTemporalTimeZone *zone = additional->present & QJS_TEMPORAL_FIELD_TIME_ZONE ?
            &additional->zone : &original->zone;
        result->zone = *zone;
        result->zone.identifier = JS_DupValue(ctx, zone->identifier);
    }
    result->calendar.present = result->present &
        (QJS_TEMPORAL_DATE_FIELDS | QJS_TEMPORAL_FIELD_ERA | QJS_TEMPORAL_FIELD_ERA_YEAR);
}
static int calendar_from_slot(JSValueConst value, QJSTemporalCalendar *result)
{
    if (JS_IsObject(value)) {
        JSTemporalPlainDateData *date;
        JSTemporalPlainDateTimeData *datetime;
        JSTemporalZonedDateTimeData *zoned;
        date = JS_GetOpaque(value, JS_CLASS_TEMPORAL_PLAIN_DATE);
        if (!date) date = JS_GetOpaque(value, JS_CLASS_TEMPORAL_PLAIN_YEAR_MONTH);
        if (!date) date = JS_GetOpaque(value, JS_CLASS_TEMPORAL_PLAIN_MONTH_DAY);
        if (date) { *result = date->calendar; return 1; }
        datetime = JS_GetOpaque(value, JS_CLASS_TEMPORAL_PLAIN_DATE_TIME);
        if (datetime) { *result = datetime->calendar; return 1; }
        zoned = JS_GetOpaque(value, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
        if (zoned) { *result = zoned->calendar; return 1; }
    }
    return 0;
}
int js_temporal_to_calendar(JSContext *ctx, JSValueConst value,
                            QJSTemporalCalendar *result)
{
    JSValueConst identifier = value;
    const char *text;
    size_t length;
    QJSTemporalParsedISO parsed;
    int error;
    if (calendar_from_slot(value, result)) return 0;
    if (!JS_IsString(identifier)) {
        JS_ThrowTypeError(ctx, "Temporal calendar identifier must be a string"); return -1;
    }
    text = JS_ToCStringLen(ctx, &length, identifier);
    if (!text) return -1;
    error = qjs_temporal_calendar_from_identifier(result, text, length);
    if (error && !qjs_temporal_parse_iso_datetime(&parsed, text, length,
            QJS_TEMPORAL_PARSE_DATE_TIME | QJS_TEMPORAL_PARSE_ZONED |
            QJS_TEMPORAL_PARSE_INSTANT | QJS_TEMPORAL_PARSE_TIME | QJS_TEMPORAL_PARSE_YEAR_MONTH | QJS_TEMPORAL_PARSE_MONTH_DAY))
        error = qjs_temporal_calendar_from_identifier(result,
                parsed.calendar ? parsed.calendar : "iso8601",
                parsed.calendar ? parsed.calendar_length : 7);
    JS_FreeCString(ctx, text);
    return js_temporal_calendar_error(ctx, error);
}
int js_temporal_get_calendar(JSContext *ctx, JSValueConst object,
                             QJSTemporalCalendar *result)
{
    JSValue value;
    int error;
    if (calendar_from_slot(object, result)) return 0;
    value = JS_GetPropertyStr(ctx, object, "calendar");
    if (JS_IsException(value)) return -1;
    if (JS_IsUndefined(value)) { JS_FreeValue(ctx, value); *result = QJS_TEMPORAL_CAL_ISO8601; return 0; }
    error = js_temporal_to_calendar(ctx, value, result);
    JS_FreeValue(ctx, value);
    return error;
}
void js_temporal_free_time_zone(JSContext *ctx, JSTemporalTimeZone *zone)
{
    JS_FreeValue(ctx, zone->identifier);
    zone->identifier = JS_UNDEFINED;
    zone->offset_nanoseconds = 0;
    zone->is_offset = FALSE;
}
int js_temporal_to_time_zone(JSContext *ctx, JSValueConst value,
                             JSTemporalTimeZone *result)
{
    QJSTemporalZone zone;
    QJSTemporalParsedISO parsed;
    const char *text;
    size_t length;
    int error;
    memset(result, 0, sizeof(*result)); result->identifier = JS_UNDEFINED;
    if (JS_IsObject(value)) {
        JSTemporalZonedDateTimeData *zoned = JS_GetOpaque(value, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
        if (zoned) {
            *result = zoned->time_zone;
            result->identifier = JS_DupValue(ctx, zoned->time_zone.identifier);
            return 0;
        }
    }
    if (!JS_IsString(value)) {
        JS_ThrowTypeError(ctx, "Temporal time-zone identifier must be a string"); return -1;
    }
    text = JS_ToCStringLen(ctx, &length, value);
    if (!text) return -1;
    error = qjs_temporal_zone_parse(&zone, text, length);
    if (error && !qjs_temporal_parse_iso_datetime(&parsed, text, length,
            QJS_TEMPORAL_PARSE_DATE_TIME | QJS_TEMPORAL_PARSE_ZONED |
            QJS_TEMPORAL_PARSE_INSTANT | QJS_TEMPORAL_PARSE_TIME | QJS_TEMPORAL_PARSE_YEAR_MONTH | QJS_TEMPORAL_PARSE_MONTH_DAY)) {
        if (parsed.time_zone) error = qjs_temporal_zone_parse(&zone,
                                   parsed.time_zone, parsed.time_zone_length);
        else if (parsed.offset_present && parsed.offset.is_z)
            error = qjs_temporal_zone_parse(&zone, "UTC", 3);
        else if (parsed.offset_present)
            error = qjs_temporal_zone_parse(&zone, parsed.offset_text, parsed.offset_length);
    }
    JS_FreeCString(ctx, text);
    if (js_temporal_calendar_error(ctx, error)) return -1;
    result->identifier = JS_NewString(ctx, zone.identifier);
    if (JS_IsException(result->identifier)) { result->identifier = JS_UNDEFINED; return -1; }
    result->is_offset = zone.is_offset;
    result->offset_nanoseconds = zone.offset_nanoseconds;
    return 0;
}
int js_temporal_time_zone_to_native(JSContext *ctx, const JSTemporalTimeZone *source,
                                    QJSTemporalZone *result)
{
    const char *text;
    size_t length;
    text = JS_ToCStringLen(ctx, &length, source->identifier);
    if (!text) return -1;
    if (length >= sizeof(result->identifier)) {
        JS_FreeCString(ctx, text);
        return js_temporal_calendar_error(ctx, QJS_TEMPORAL_ERROR_BACKEND);
    }
    memcpy(result->identifier, text, length); result->identifier[length] = 0;
    JS_FreeCString(ctx, text);
    result->is_offset = source->is_offset;
    result->offset_nanoseconds = source->offset_nanoseconds;
    return 0;
}
int js_temporal_time_zone_offset(JSContext *ctx,
    const JSTemporalTimeZone *zone, QJSTemporalEpochNs epoch, int64_t *result)
{
    QJSTemporalZone backend;
    if (js_temporal_time_zone_to_native(ctx, zone, &backend)) return -1;
    return js_temporal_calendar_error(ctx, qjs_temporal_zone_offset(&backend, epoch, result));
}
int js_temporal_time_zone_datetime(JSContext *ctx,
    const JSTemporalTimeZone *zone, QJSTemporalEpochNs epoch,
    QJSTemporalISODateTime *result)
{
    QJSTemporalZone backend;
    if (js_temporal_time_zone_to_native(ctx, zone, &backend)) return -1;
    return js_temporal_calendar_error(ctx, qjs_temporal_zone_datetime(&backend, epoch, result));
}
int js_temporal_time_zone_epoch(JSContext *ctx,
    const JSTemporalTimeZone *zone, QJSTemporalISODateTime datetime,
    QJSTemporalDisambiguation disambiguation, QJSTemporalEpochNs *result)
{
    QJSTemporalZone backend;
    if (js_temporal_time_zone_to_native(ctx, zone, &backend)) return -1;
    return js_temporal_calendar_error(ctx,
        qjs_temporal_zone_epoch(&backend, datetime, disambiguation, result));
}
int js_temporal_time_zone_start_of_day(JSContext *ctx,
    const JSTemporalTimeZone *zone, QJSTemporalISODate date, QJSTemporalEpochNs *result)
{
    QJSTemporalZone backend;
    if (js_temporal_time_zone_to_native(ctx, zone, &backend)) return -1;
    return js_temporal_calendar_error(ctx, qjs_temporal_zone_start_of_day(&backend, date, result));
}
int js_temporal_time_zone_possible_epochs(JSContext *ctx,
    const JSTemporalTimeZone *zone, QJSTemporalISODateTime datetime,
    QJSTemporalEpochNs result[2], int *count)
{
    QJSTemporalZone backend;
    if (js_temporal_time_zone_to_native(ctx, zone, &backend)) return -1;
    return js_temporal_calendar_error(ctx,
        qjs_temporal_zone_possible_epochs(&backend, datetime, result, count));
}
int js_temporal_time_zone_transition(JSContext *ctx,
    const JSTemporalTimeZone *zone, QJSTemporalEpochNs epoch, BOOL next,
    QJSTemporalEpochNs *result, int *found)
{
    QJSTemporalZone backend;
    if (js_temporal_time_zone_to_native(ctx, zone, &backend)) return -1;
    return js_temporal_calendar_error(ctx,
        qjs_temporal_zone_transition(&backend, epoch, next, result, found));
}
int js_temporal_time_zones_equal(JSContext *ctx,
    const JSTemporalTimeZone *one, const JSTemporalTimeZone *two, int *result)
{
    QJSTemporalZone first, second;
    if (js_temporal_time_zone_to_native(ctx, one, &first) ||
        js_temporal_time_zone_to_native(ctx, two, &second)) return -1;
    return js_temporal_calendar_error(ctx, qjs_temporal_zones_equal(&first, &second, result));
}
int js_temporal_interpret_offset(JSContext *ctx,
    QJSTemporalISODateTime datetime, BOOL start_of_day,
    JSTemporalOffsetBehaviour behaviour, int64_t offset,
    const JSTemporalTimeZone *zone, QJSTemporalDisambiguation disambiguation,
    JSTemporalOffsetOption option, BOOL match_minutes, QJSTemporalEpochNs *result)
{
    QJSTemporalEpochNs wall, epoch, choices[2], difference;
    int n;
    int64_t actual, magnitude, rounded, days;
    if (start_of_day)
        return js_temporal_time_zone_start_of_day(ctx, zone, datetime.date, result);
    if (behaviour == JS_TEMPORAL_OFFSET_WALL ||
        (behaviour == JS_TEMPORAL_OFFSET_OPTION && option == JS_TEMPORAL_OFFSET_IGNORE))
        return js_temporal_time_zone_epoch(ctx, zone, datetime, disambiguation, result);
    if (qjs_temporal_iso_datetime_to_epoch_ns(&wall, datetime))
        return js_temporal_calendar_error(ctx, QJS_TEMPORAL_ERROR_RANGE);
    if (behaviour == JS_TEMPORAL_OFFSET_EXACT || option == JS_TEMPORAL_OFFSET_USE) {
        if (qjs_temporal_epoch_ns_subtract(&epoch, wall,
                qjs_temporal_epoch_ns_from_int64(offset)) ||
            !qjs_temporal_epoch_ns_is_valid(epoch))
            return js_temporal_calendar_error(ctx, QJS_TEMPORAL_ERROR_RANGE);
        *result = epoch; return 0;
    }
    if (qjs_temporal_iso_date_to_days(&days, datetime.date) ||
        days < -INT64_C(100000000) || days > INT64_C(100000000))
        return js_temporal_calendar_error(ctx, QJS_TEMPORAL_ERROR_RANGE);
    if (js_temporal_time_zone_possible_epochs(ctx, zone, datetime, choices, &n)) return -1;
    for (int i = 0; i < n; i++) {
        if (qjs_temporal_epoch_ns_subtract(&difference, wall, choices[i]) ||
            qjs_temporal_epoch_ns_to_int64(&actual, difference))
            return js_temporal_calendar_error(ctx, QJS_TEMPORAL_ERROR_BACKEND);
        if (actual == offset) { *result = choices[i]; return 0; }
        if (match_minutes) {
            magnitude = actual < 0 ? -actual : actual;
            rounded = (magnitude + INT64_C(30000000000)) / INT64_C(60000000000) *
                INT64_C(60000000000);
            if (actual < 0) rounded = -rounded;
            if (rounded == offset) { *result = choices[i]; return 0; }
        }
    }
    if (option == JS_TEMPORAL_OFFSET_REJECT)
        return js_temporal_calendar_error(ctx, QJS_TEMPORAL_ERROR_RANGE);
    return js_temporal_time_zone_epoch(ctx, zone, datetime, disambiguation, result);
}
void js_temporal_free_relative_to(JSContext *ctx, JSTemporalRelativeTo *relative)
{
    JS_FreeValue(ctx, relative->plain); JS_FreeValue(ctx, relative->zoned);
    relative->plain = relative->zoned = JS_UNDEFINED;
}
int js_temporal_get_relative_to(JSContext *ctx, JSValueConst options,
                               JSTemporalRelativeTo *result)
{
    JSValue value, identifier;
    JSTemporalPlainDateData *plain;
    JSTemporalPlainDateTimeData *datetime_slot;
    JSTemporalZonedDateTimeData *zoned;
    QJSTemporalISODateTime datetime;
    QJSTemporalCalendar calendar;
    QJSTemporalEpochNs epoch;
    JSTemporalFields fields;
    QJSTemporalParsedISO parsed;
    const char *text;
    size_t length, consumed;
    QJSTemporalUTCOffset offset;
    JSTemporalOffsetBehaviour behaviour = JS_TEMPORAL_OFFSET_OPTION;
    BOOL match_minutes = FALSE, start_of_day = FALSE;
    int error = -1;
    result->plain = result->zoned = JS_UNDEFINED;
    fields_init(&fields);
    value = JS_GetPropertyStr(ctx, options, "relativeTo");
    if (JS_IsException(value)) return -1;
    if (JS_IsUndefined(value)) { JS_FreeValue(ctx, value); return 0; }
    if (JS_IsObject(value)) {
        zoned = JS_GetOpaque(value, JS_CLASS_TEMPORAL_ZONED_DATE_TIME);
        if (zoned) { result->zoned = value; return 0; }
        plain = JS_GetOpaque(value, JS_CLASS_TEMPORAL_PLAIN_DATE);
        if (plain) { result->plain = value; return 0; }
        datetime_slot = JS_GetOpaque(value, JS_CLASS_TEMPORAL_PLAIN_DATE_TIME);
        if (datetime_slot) {
            result->plain = js_temporal_create_plain_date(ctx, JS_UNDEFINED,
                                  datetime_slot->datetime.date, datetime_slot->calendar);
            JS_FreeValue(ctx, value);
            return JS_IsException(result->plain) ? -1 : 0;
        }
        if (js_temporal_get_calendar(ctx, value, &calendar) ||
            js_temporal_prepare_calendar_fields(ctx, calendar, value,
                QJS_TEMPORAL_DATE_FIELDS, QJS_TEMPORAL_TIME_FIELDS |
                QJS_TEMPORAL_FIELD_OFFSET | QJS_TEMPORAL_FIELD_TIME_ZONE, 0, FALSE, &fields))
            goto done;
        if (js_temporal_interpret_datetime_fields(ctx, calendar, &fields,
                QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &datetime)) goto done;
        if (JS_IsUndefined(fields.offset)) behaviour = JS_TEMPORAL_OFFSET_WALL;
        else {
            text = JS_ToCStringLen(ctx, &length, fields.offset);
            if (!text) goto done;
            error = qjs_temporal_parse_utc_offset_prefix(&offset, &consumed,
                       text, length, QJS_TEMPORAL_OFFSET_ALLOW_SECONDS);
            JS_FreeCString(ctx, text);
            if (error) { js_temporal_calendar_error(ctx, QJS_TEMPORAL_ERROR_RANGE); goto done; }
        }
    } else {
        if (!JS_IsString(value)) {
            JS_ThrowTypeError(ctx, "Temporal relativeTo must be a string or object"); goto done;
        }
        text = JS_ToCStringLen(ctx, &length, value);
        if (!text) goto done;
        error = qjs_temporal_parse_iso_datetime(&parsed, text, length,
                    QJS_TEMPORAL_PARSE_DATE_TIME | QJS_TEMPORAL_PARSE_ZONED);
        if (!error) {
            error = qjs_temporal_calendar_from_identifier(&calendar,
                parsed.calendar ? parsed.calendar : "iso8601",
                parsed.calendar ? parsed.calendar_length : 7);
            if (!error && parsed.time_zone) {
                identifier = JS_NewStringLen(ctx, parsed.time_zone, parsed.time_zone_length);
                if (JS_IsException(identifier)) error = -100;
                else {
                    error = js_temporal_to_time_zone(ctx, identifier, &fields.zone) ? -100 : 0;
                    JS_FreeValue(ctx, identifier);
                }
            }
        }
        JS_FreeCString(ctx, text);
        if (error) { if (error != -100) js_temporal_calendar_error(ctx, error); error = -1; goto done; }
        datetime = parsed.datetime;
        offset = parsed.offset;
        start_of_day = !parsed.has_time;
        behaviour = parsed.offset_present ? parsed.offset.is_z ?
            JS_TEMPORAL_OFFSET_EXACT : JS_TEMPORAL_OFFSET_OPTION : JS_TEMPORAL_OFFSET_WALL;
        match_minutes = !parsed.offset.has_seconds;
    }
    if (JS_IsUndefined(fields.zone.identifier)) {
        result->plain = js_temporal_create_plain_date(ctx, JS_UNDEFINED, datetime.date, calendar);
        error = JS_IsException(result->plain) ? -1 : 0;
    } else {
        if (js_temporal_interpret_offset(ctx, datetime, start_of_day, behaviour,
               behaviour == JS_TEMPORAL_OFFSET_OPTION ? offset.nanoseconds : 0,
               &fields.zone, QJS_TEMPORAL_COMPATIBLE, JS_TEMPORAL_OFFSET_REJECT,
               match_minutes, &epoch)) { error = -1; goto done; }
        result->zoned = js_temporal_create_zoned_date_time(ctx, JS_UNDEFINED, epoch,
                                                         &fields.zone, calendar);
        error = JS_IsException(result->zoned) ? -1 : 0;
    }
 done:
    js_temporal_fields_free(ctx, &fields); JS_FreeValue(ctx, value);
    return error;
}
