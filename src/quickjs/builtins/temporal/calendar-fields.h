/* Engine Temporal field records, conversion and ownership. */
#ifndef QUICKJS_TEMPORAL_CALENDAR_FIELDS_H
#define QUICKJS_TEMPORAL_CALENDAR_FIELDS_H
#include "temporal-internal.h"
#include "../../../temporal/calendar.h"
#include "../../../temporal/time-zone.h"
typedef struct JSTemporalFields {
    QJSTemporalCalendarFields calendar;
    double time[6]; /* hour, minute, second, millisecond, microsecond, nanosecond */
    unsigned present;
    JSValue offset; /* owned validated OffsetString or undefined */
    JSTemporalTimeZone zone; /* owned identifier or undefined */
} JSTemporalFields;
/* Always initialize result internally, including failure. Free after either
   success or failure; no input is consumed. Nonpartial undefined time fields
   become present defaults. Range narrowing happens after all field Gets. */
int js_temporal_prepare_calendar_fields(JSContext *ctx,
    QJSTemporalCalendar calendar, JSValueConst object,
    unsigned calendar_names, unsigned non_calendar_names,
    unsigned required_names, BOOL partial, JSTemporalFields *result);
int js_temporal_iso_date_to_fields(JSContext *ctx,
    QJSTemporalCalendar calendar, QJSTemporalISODate date,
    JSTemporalFields *result);
/* Implemented by the Plain civil owner without observable property rereads. */
int js_temporal_interpret_datetime_fields(JSContext *ctx,
    QJSTemporalCalendar calendar, const JSTemporalFields *fields,
    QJSTemporalOverflow overflow, QJSTemporalISODateTime *result);
void js_temporal_calendar_merge_fields(JSContext *ctx,
    QJSTemporalCalendar calendar, const JSTemporalFields *original,
    const JSTemporalFields *additional, JSTemporalFields *result);
void js_temporal_fields_free(JSContext *ctx, JSTemporalFields *fields);
int js_temporal_calendar_error(JSContext *ctx, int error);
/* Shared IsPartialTemporalObject remains coordinator-owned:
   js_temporal_is_partial_object. */
int js_temporal_time_zone_epoch(JSContext *ctx,
    const JSTemporalTimeZone *zone, QJSTemporalISODateTime datetime,
    QJSTemporalDisambiguation disambiguation, QJSTemporalEpochNs *result);
int js_temporal_time_zone_start_of_day(JSContext *ctx,
    const JSTemporalTimeZone *zone, QJSTemporalISODate date,
    QJSTemporalEpochNs *result);
int js_temporal_time_zone_possible_epochs(JSContext *ctx,
    const JSTemporalTimeZone *zone, QJSTemporalISODateTime datetime,
    QJSTemporalEpochNs result[2], int *count);
int js_temporal_time_zone_transition(JSContext *ctx,
    const JSTemporalTimeZone *zone, QJSTemporalEpochNs epoch, BOOL next,
    QJSTemporalEpochNs *result, int *found);
int js_temporal_time_zones_equal(JSContext *ctx,
    const JSTemporalTimeZone *one, const JSTemporalTimeZone *two, int *result);
int js_temporal_time_zone_to_native(JSContext *ctx,
    const JSTemporalTimeZone *zone, QJSTemporalZone *result);
typedef enum JSTemporalOffsetBehaviour {
    JS_TEMPORAL_OFFSET_WALL, JS_TEMPORAL_OFFSET_EXACT, JS_TEMPORAL_OFFSET_OPTION
} JSTemporalOffsetBehaviour;
typedef enum JSTemporalOffsetOption {
    JS_TEMPORAL_OFFSET_IGNORE, JS_TEMPORAL_OFFSET_USE,
    JS_TEMPORAL_OFFSET_PREFER, JS_TEMPORAL_OFFSET_REJECT
} JSTemporalOffsetOption;
int js_temporal_interpret_offset(JSContext *ctx,
    QJSTemporalISODateTime datetime, BOOL start_of_day,
    JSTemporalOffsetBehaviour behaviour, int64_t offset,
    const JSTemporalTimeZone *zone, QJSTemporalDisambiguation disambiguation,
    JSTemporalOffsetOption option, BOOL match_minutes, QJSTemporalEpochNs *result);
typedef struct JSTemporalRelativeTo { JSValue plain, zoned; } JSTemporalRelativeTo;
int js_temporal_get_relative_to(JSContext *ctx, JSValueConst options,
                               JSTemporalRelativeTo *result);
void js_temporal_free_relative_to(JSContext *ctx, JSTemporalRelativeTo *relative);
int js_temporal_zoned_add_duration(JSContext *ctx,
    const JSTemporalZonedDateTimeData *zoned,
    QJSTemporalInternalDuration duration, QJSTemporalOverflow overflow,
    QJSTemporalEpochNs *result);
int js_temporal_zoned_difference_raw(JSContext *ctx,
    QJSTemporalEpochNs start, QJSTemporalEpochNs end,
    const JSTemporalTimeZone *zone, QJSTemporalCalendar calendar,
    QJSTemporalUnit largest, QJSTemporalInternalDuration *result);
int js_temporal_zoned_difference_round(JSContext *ctx,
    QJSTemporalEpochNs start, QJSTemporalEpochNs end,
    const JSTemporalTimeZone *zone, QJSTemporalCalendar calendar,
    const QJSTemporalDifferenceSettings *settings,
    QJSTemporalInternalDuration *result);
int js_temporal_zoned_difference_total(JSContext *ctx,
    QJSTemporalEpochNs start, QJSTemporalEpochNs end,
    const JSTemporalTimeZone *zone, QJSTemporalCalendar calendar,
    QJSTemporalUnit unit, double *result);
#endif
