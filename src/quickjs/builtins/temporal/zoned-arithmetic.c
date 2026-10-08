/* Exact zoned duration arithmetic and relative rounding adapters.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE. */
#include "calendar-fields.h"
#include "../../../temporal/relative.h"

static int time_sign(QJSTemporalEpochNs value)
{
    QJSTemporalEpochNs zero = {0};
    return qjs_temporal_epoch_ns_compare(value, zero);
}
int js_temporal_zoned_add_duration(JSContext *ctx,
    const JSTemporalZonedDateTimeData *zoned,
    QJSTemporalInternalDuration duration, QJSTemporalOverflow overflow,
    QJSTemporalEpochNs *result)
{
    QJSTemporalISODateTime local;
    QJSTemporalEpochNs intermediate = zoned->epoch_nanoseconds, epoch;
    if (duration.date.years || duration.date.months || duration.date.weeks || duration.date.days) {
        if (js_temporal_time_zone_datetime(ctx, &zoned->time_zone,
                zoned->epoch_nanoseconds, &local) ||
            js_temporal_calendar_date_add(ctx, zoned->calendar, local.date,
                duration.date, overflow, &local.date) ||
            js_temporal_time_zone_epoch(ctx, &zoned->time_zone, local,
                QJS_TEMPORAL_COMPATIBLE, &intermediate)) return -1;
    }
    if (qjs_temporal_epoch_ns_add(&epoch, intermediate, duration.time) ||
        !qjs_temporal_epoch_ns_is_valid(epoch))
        return js_temporal_calendar_error(ctx, QJS_TEMPORAL_ERROR_RANGE);
    *result = epoch;
    return 0;
}
int js_temporal_zoned_difference_raw(JSContext *ctx,
    QJSTemporalEpochNs start, QJSTemporalEpochNs end,
    const JSTemporalTimeZone *zone, QJSTemporalCalendar calendar,
    QJSTemporalUnit largest, QJSTemporalInternalDuration *result)
{
    QJSTemporalInternalDuration duration = {0};
    QJSTemporalISODateTime one, two, intermediate;
    QJSTemporalEpochNs one_time, two_time, difference, anchor;
    int64_t one_days, two_days;
    int sign, maximum_correction, correction, succeeded = 0;
    if (qjs_temporal_epoch_ns_subtract(&difference, end, start))
        return js_temporal_calendar_error(ctx, QJS_TEMPORAL_ERROR_RANGE);
    if (largest >= QJS_TEMPORAL_HOUR || !time_sign(difference)) {
        duration.time = difference; *result = duration; return 0;
    }
    if (js_temporal_time_zone_datetime(ctx, zone, start, &one) ||
        js_temporal_time_zone_datetime(ctx, zone, end, &two)) return -1;
    qjs_temporal_iso_date_to_days(&one_days, one.date);
    qjs_temporal_iso_date_to_days(&two_days, two.date);
    if (one_days == two_days) {
        duration.time = difference; *result = duration; return 0;
    }
    sign = time_sign(difference) < 0 ? 1 : -1;
    maximum_correction = sign < 0 ? 2 : 1;
    /* Compare wall clocks without subtracting their civil dates. */
    intermediate = one; intermediate.date = (QJSTemporalISODate){1970,1,1};
    qjs_temporal_iso_datetime_to_epoch_ns(&one_time, intermediate);
    intermediate.time = two.time;
    qjs_temporal_iso_datetime_to_epoch_ns(&two_time, intermediate);
    qjs_temporal_epoch_ns_subtract(&duration.time, two_time, one_time);
    correction = time_sign(duration.time) == sign ? 1 : 0;
    for (; correction <= maximum_correction && !succeeded; correction++) {
        intermediate.time = one.time;
        if (qjs_temporal_iso_date_from_days(&intermediate.date,
                                            two_days + (int64_t)correction * sign))
            return js_temporal_calendar_error(ctx, QJS_TEMPORAL_ERROR_RANGE);
        if (js_temporal_time_zone_epoch(ctx, zone, intermediate,
                                         QJS_TEMPORAL_COMPATIBLE, &anchor)) return -1;
        qjs_temporal_epoch_ns_subtract(&duration.time, end, anchor);
        succeeded = time_sign(duration.time) != sign;
    }
    if (!succeeded)
        return js_temporal_calendar_error(ctx, QJS_TEMPORAL_ERROR_BACKEND);
    if (js_temporal_calendar_error(ctx, qjs_temporal_calendar_date_until(calendar,
            one.date, intermediate.date, largest, &duration.date))) return -1;
    *result = duration;
    return 0;
}
int js_temporal_zoned_difference_round(JSContext *ctx,
    QJSTemporalEpochNs start, QJSTemporalEpochNs end,
    const JSTemporalTimeZone *zone, QJSTemporalCalendar calendar,
    const QJSTemporalDifferenceSettings *settings,
    QJSTemporalInternalDuration *result)
{
    QJSTemporalInternalDuration duration = {0};
    QJSTemporalISODateTime origin;
    QJSTemporalZone native;
    uint64_t increment;
    if (!qjs_temporal_epoch_ns_compare(start, end)) { *result = duration; return 0; }
    if (settings->largest_unit >= QJS_TEMPORAL_HOUR) {
        increment = qjs_temporal_unit_nanoseconds(settings->smallest_unit) *
            settings->rounding_increment;
        if (qjs_temporal_epoch_ns_subtract(&duration.time, end, start) ||
            qjs_temporal_epoch_ns_round(&duration.time, duration.time, increment,
                                        settings->rounding_mode))
            return js_temporal_calendar_error(ctx, QJS_TEMPORAL_ERROR_RANGE);
        *result = duration; return 0;
    }
    if (js_temporal_zoned_difference_raw(ctx, start, end, zone, calendar,
                                     settings->largest_unit, &duration)) return -1;
    if (settings->smallest_unit == QJS_TEMPORAL_NANOSECOND &&
        settings->rounding_increment == 1) { *result = duration; return 0; }
    if (js_temporal_time_zone_to_native(ctx, zone, &native) ||
        js_temporal_time_zone_datetime(ctx, zone, start, &origin)) return -1;
    return js_temporal_calendar_error(ctx, qjs_temporal_relative_round(duration,
             start, end, origin, &native, calendar, settings, result));
}
int js_temporal_zoned_difference_total(JSContext *ctx,
    QJSTemporalEpochNs start, QJSTemporalEpochNs end,
    const JSTemporalTimeZone *zone, QJSTemporalCalendar calendar,
    QJSTemporalUnit unit, double *result)
{
    QJSTemporalInternalDuration duration = {0};
    QJSTemporalISODateTime origin;
    QJSTemporalZone native;
    if (!qjs_temporal_epoch_ns_compare(start, end)) { *result = 0; return 0; }
    if (js_temporal_zoned_difference_raw(ctx, start, end, zone, calendar,
                                     unit, &duration)) return -1;
    if (js_temporal_time_zone_to_native(ctx, zone, &native) ||
        js_temporal_time_zone_datetime(ctx, zone, start, &origin)) return -1;
    return js_temporal_calendar_error(ctx, qjs_temporal_relative_total(duration,
             start, end, origin, &native, calendar, unit, result));
}
