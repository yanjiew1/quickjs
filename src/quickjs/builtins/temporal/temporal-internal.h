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
#ifndef QUICKJS_BUILTINS_TEMPORAL_INTERNAL_H
#define QUICKJS_BUILTINS_TEMPORAL_INTERNAL_H

#include "../../internal/allocator.h"
#include "../../internal/class.h"
#include "../../internal/function.h"
#include "../../internal/function-list.h"
#include "../../internal/runtime.h"
#include "../../value/conversion.h"
#include "../../../temporal/types.h"
#include "../../../temporal/duration.h"
#include "../../../temporal/options.h"
#include "../../../temporal/parse.h"

typedef QJSTemporalEpochNs JSTemporalInstantData;
typedef QJSTemporalDuration JSTemporalDurationData;
typedef QJSTemporalISOTime JSTemporalPlainTimeData;

typedef struct JSTemporalPlainDateData {
    QJSTemporalISODate date;
    QJSTemporalCalendar calendar;
} JSTemporalPlainDateData;

typedef struct JSTemporalPlainDateTimeData {
    QJSTemporalISODateTime datetime;
    QJSTemporalCalendar calendar;
} JSTemporalPlainDateTimeData;

/* Reference ISO dates are retained for YearMonth and MonthDay. */
typedef JSTemporalPlainDateData JSTemporalPlainYearMonthData;
typedef JSTemporalPlainDateData JSTemporalPlainMonthDayData;

/* Identifier is a canonical String retained as a JSValue. The offset
   discriminator and integer are native metadata, never user properties. */
typedef struct JSTemporalTimeZone {
    JSValue identifier;
    int64_t offset_nanoseconds;
    BOOL is_offset;
} JSTemporalTimeZone;

typedef struct JSTemporalZonedDateTimeData {
    QJSTemporalEpochNs epoch_nanoseconds;
    JSTemporalTimeZone time_zone;
    QJSTemporalCalendar calendar;
} JSTemporalZonedDateTimeData;

/* Options results are owned values. String / unit conversions perform
   exactly one Get followed by the standard observable coercion. */
int js_temporal_to_epoch_ns(JSContext *ctx, JSValueConst value,
                            QJSTemporalEpochNs *result);
JSValue js_temporal_get_options(JSContext *ctx, JSValueConst value);
int js_temporal_get_string_option(JSContext *ctx, JSValueConst options,
                                 const char *name,
                                 const char *const *values, int count,
                                 int fallback, int *result);
int js_temporal_get_unit_option(JSContext *ctx, JSValueConst options,
                               const char *name, BOOL required,
                               QJSTemporalUnit *result);
int js_temporal_get_rounding_increment(JSContext *ctx, JSValueConst options,
                                      uint32_t *result);
int js_temporal_get_rounding_mode(JSContext *ctx, JSValueConst options,
                                 QJSTemporalRoundingMode fallback,
                                 QJSTemporalRoundingMode *result);
int js_temporal_get_fractional_digits(JSContext *ctx, JSValueConst options,
                                     int *result);
int js_temporal_to_integer(JSContext *ctx, JSValueConst value,
                           double *result);
/* Return 1 / 0 for the standard Boolean result, or -1 with exception. */
int js_temporal_is_partial_object(JSContext *ctx, JSValueConst value);
int js_temporal_validate_increment(JSContext *ctx, uint32_t increment,
                                  uint64_t maximum, BOOL inclusive);
int js_temporal_get_difference_settings(
    JSContext *ctx, JSValueConst options, BOOL since,
    QJSTemporalUnit minimum, QJSTemporalUnit maximum,
    QJSTemporalUnit fallback_smallest, QJSTemporalUnit fallback_largest,
    QJSTemporalDifferenceSettings *result);
/* Perform one prototype Get, then ensure a required fallback intrinsic
   in the constructor's realm without publishing its global namespace. */
JSValue js_temporal_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                                    int class_id);

/* Allocation creates an intrinsic-realm result when new_target is
   undefined. Each owner implements and tests its own conversion order. */
JSValue js_temporal_create_instant(JSContext *ctx, JSValueConst new_target,
                                  QJSTemporalEpochNs epoch_nanoseconds);
JSValue js_temporal_to_instant(JSContext *ctx, JSValueConst value);
JSValue js_temporal_create_duration(JSContext *ctx, JSValueConst new_target,
                                   const QJSTemporalDuration *duration);
JSValue js_temporal_to_duration(JSContext *ctx, JSValueConst value);

JSValue js_temporal_create_plain_time(JSContext *ctx, JSValueConst new_target,
                                     QJSTemporalISOTime time);
JSValue js_temporal_to_plain_time(JSContext *ctx, JSValueConst value,
                                 JSValueConst options);
JSValue js_temporal_create_plain_date(JSContext *ctx, JSValueConst new_target,
                                     QJSTemporalISODate date,
                                     QJSTemporalCalendar calendar);
JSValue js_temporal_to_plain_date(JSContext *ctx, JSValueConst value,
                                 JSValueConst options);
JSValue js_temporal_create_plain_date_time(
    JSContext *ctx, JSValueConst new_target, QJSTemporalISODateTime datetime,
    QJSTemporalCalendar calendar);
JSValue js_temporal_to_plain_date_time(JSContext *ctx, JSValueConst value,
                                      JSValueConst options);
JSValue js_temporal_create_plain_year_month(
    JSContext *ctx, JSValueConst new_target, QJSTemporalISODate reference_date,
    QJSTemporalCalendar calendar);
JSValue js_temporal_create_plain_month_day(
    JSContext *ctx, JSValueConst new_target, QJSTemporalISODate reference_date,
    QJSTemporalCalendar calendar);
JSValue js_temporal_to_plain_year_month(JSContext *ctx, JSValueConst value,
                                      JSValueConst options);
JSValue js_temporal_to_plain_month_day(JSContext *ctx, JSValueConst value,
                                     JSValueConst options);
int js_temporal_calendar_date_add(
    JSContext *ctx, QJSTemporalCalendar calendar, QJSTemporalISODate date,
    QJSTemporalDateDuration duration, QJSTemporalOverflow overflow,
    QJSTemporalISODate *result);
int js_temporal_plain_datetime_difference_round(
    JSContext *ctx, QJSTemporalISODateTime one, QJSTemporalISODateTime two,
    QJSTemporalCalendar calendar,
    const QJSTemporalDifferenceSettings *settings,
    QJSTemporalInternalDuration *result);
int js_temporal_plain_datetime_difference_total(
    JSContext *ctx, QJSTemporalISODateTime one, QJSTemporalISODateTime two,
    QJSTemporalCalendar calendar, QJSTemporalUnit unit, double *result);

/* Calendar identifiers and fields are handled by the calendar owner;
   unknown calendars throw rather than silently becoming ISO. */
int js_temporal_to_calendar(JSContext *ctx, JSValueConst value,
                            QJSTemporalCalendar *result);
int js_temporal_get_calendar(JSContext *ctx, JSValueConst object,
                             QJSTemporalCalendar *result);
const char *qjs_temporal_calendar_identifier(QJSTemporalCalendar calendar);

/* A converted zone owns identifier. Every successful conversion must be
   matched by js_temporal_free_time_zone, including abrupt completions. */
int js_temporal_to_time_zone(JSContext *ctx, JSValueConst value,
                             JSTemporalTimeZone *result);
void js_temporal_free_time_zone(JSContext *ctx, JSTemporalTimeZone *zone);
int js_temporal_time_zone_offset(JSContext *ctx,
                                const JSTemporalTimeZone *zone,
                                QJSTemporalEpochNs epoch_nanoseconds,
                                int64_t *result);
int js_temporal_time_zone_datetime(JSContext *ctx,
                                  const JSTemporalTimeZone *zone,
                                  QJSTemporalEpochNs epoch_nanoseconds,
                                  QJSTemporalISODateTime *result);
JSValue js_temporal_create_zoned_date_time(
    JSContext *ctx, JSValueConst new_target, QJSTemporalEpochNs epoch,
    const JSTemporalTimeZone *zone, QJSTemporalCalendar calendar);
JSValue js_temporal_to_zoned_date_time(JSContext *ctx, JSValueConst value,
                                      JSValueConst options);

/* Initializers add complete constructors to an unpublished namespace.
   The common owner publishes Temporal only after all initializers pass. */
int js_temporal_init_instant(JSContext *ctx, JSValueConst namespace_object);
int js_temporal_init_duration(JSContext *ctx, JSValueConst namespace_object);
int js_temporal_init_plain_time(JSContext *ctx, JSValueConst namespace_object);
int js_temporal_init_plain_date(JSContext *ctx, JSValueConst namespace_object);
int js_temporal_init_plain_date_time(JSContext *ctx,
                                   JSValueConst namespace_object);
int js_temporal_init_plain_year_month(JSContext *ctx,
                                    JSValueConst namespace_object);
int js_temporal_init_plain_month_day(JSContext *ctx,
                                   JSValueConst namespace_object);
int js_temporal_init_zoned_date_time(JSContext *ctx,
                                   JSValueConst namespace_object);
int js_temporal_init_now(JSContext *ctx, JSValueConst namespace_object);

#ifdef CONFIG_INTL
/* Intl owner defines the formatter without exposing private slots through
   JavaScript getters. this_val is borrowed and already brand-checked. */
JSValue js_intl_temporal_to_locale_string(JSContext *ctx,
                                        JSValueConst this_val,
                                        JSValueConst locales,
                                        JSValueConst options);
#endif

#endif /* QUICKJS_BUILTINS_TEMPORAL_INTERNAL_H */
