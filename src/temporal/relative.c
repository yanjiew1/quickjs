/* Plain civil relative duration arithmetic.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, as in epoch.c.
 */
#include "relative.h"
#include "civil.h"
#include "duration.h"
#include "options.h"
#include <limits.h>

#define NS_PER_DAY UINT64_C(86400000000000)
static const QJSTemporalEpochNs zero = { 0, 0 };

static int pair_sign(QJSTemporalEpochNs value)
{
    return qjs_temporal_epoch_ns_compare(value, zero);
}

static int time_of_day(QJSTemporalEpochNs *result, QJSTemporalISOTime t)
{
    int64_t ns;
    if (!qjs_temporal_iso_time_is_valid(t))
        return QJS_TEMPORAL_ERROR_RANGE;
    ns = (((((int64_t)t.hour * 60 + t.minute) * 60 + t.second) * 1000 +
           t.millisecond) * 1000 + t.microsecond) * 1000 + t.nanosecond;
    *result = qjs_temporal_epoch_ns_from_int64(ns);
    return 0;
}

static int add_days(QJSTemporalEpochNs *result,
                     QJSTemporalEpochNs value, int64_t days)
{
    QJSTemporalEpochNs p = qjs_temporal_epoch_ns_from_int64(days), product;
    /* Split the multiplier; both factors fit int32. */
    if (qjs_temporal_epoch_ns_multiply(&product, p, 86400) < 0 ||
        qjs_temporal_epoch_ns_multiply(&product, product, 1000000000) < 0 ||
        qjs_temporal_epoch_ns_add(result, value, product) < 0)
        return QJS_TEMPORAL_ERROR_RANGE;
    return 0;
}

static int truncate_days(QJSTemporalEpochNs value, int64_t *result)
{
    QJSTemporalEpochNs quotient;
    uint64_t remainder;
    int64_t days;
    if (qjs_temporal_epoch_ns_divide(&quotient, &remainder,
                                     value, NS_PER_DAY) < 0 ||
        qjs_temporal_epoch_ns_to_int64(&days, quotient) < 0)
        return QJS_TEMPORAL_ERROR_RANGE;
    if (pair_sign(value) < 0 && remainder)
        days++;
    *result = days;
    return 0;
}

int qjs_temporal_plain_datetime_difference(
    QJSTemporalISODateTime one, QJSTemporalISODateTime two,
    QJSTemporalCalendar calendar, QJSTemporalUnit largest,
    QJSTemporalInternalDuration *result)
{
    QJSTemporalEpochNs a, b;
    QJSTemporalInternalDuration out;
    QJSTemporalISODate adjusted = two.date;
    QJSTemporalUnit date_largest;
    int time_sign, date_sign, error;
    if (largest < QJS_TEMPORAL_YEAR || largest > QJS_TEMPORAL_NANOSECOND ||
        !qjs_temporal_iso_datetime_within_limits(one) ||
        !qjs_temporal_iso_datetime_within_limits(two) ||
        time_of_day(&a, one.time) < 0 || time_of_day(&b, two.time) < 0 ||
        qjs_temporal_epoch_ns_subtract(&out.time, b, a) < 0)
        return QJS_TEMPORAL_ERROR_RANGE;
    time_sign = pair_sign(out.time);
    date_sign = qjs_temporal_iso_date_compare(one.date, two.date);
    if (time_sign && time_sign == date_sign) {
        if (qjs_temporal_iso_add_days(&adjusted, adjusted, time_sign) < 0 ||
            add_days(&out.time, out.time, -time_sign) < 0)
            return QJS_TEMPORAL_ERROR_RANGE;
    }
    date_largest = largest < QJS_TEMPORAL_DAY ? largest : QJS_TEMPORAL_DAY;
    error = qjs_temporal_calendar_date_until(calendar, one.date, adjusted,
                                             date_largest, &out.date);
    if (error)
        return error;
    if (largest != date_largest) {
        if (add_days(&out.time, out.time, out.date.days) < 0)
            return QJS_TEMPORAL_ERROR_RANGE;
        out.date.days = 0;
    }
    *result = out;
    return 0;
}

typedef struct NudgeWindow {
    int64_t r1, r2;
    QJSTemporalEpochNs start, end;
    QJSTemporalDateDuration start_duration, end_duration;
} NudgeWindow;

static int date_epoch(QJSTemporalEpochNs *result,
                       QJSTemporalISODateTime origin,
                       QJSTemporalCalendar calendar, const QJSTemporalZone *zone,
                       QJSTemporalDateDuration duration)
{
    QJSTemporalISODateTime value = origin;
    int error = qjs_temporal_calendar_date_add(calendar, origin.date,
                     duration, QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &value.date);
    if (error)
        return error;
    return zone ? qjs_temporal_zone_epoch(zone, value,
                         QJS_TEMPORAL_COMPATIBLE, result) :
                  qjs_temporal_iso_datetime_to_epoch_ns(result, value);
}

static int nudge_window(NudgeWindow *result, int sign,
                         QJSTemporalInternalDuration duration,
                         QJSTemporalEpochNs origin_ns,
                         QJSTemporalISODateTime origin,
                         QJSTemporalCalendar calendar, const QJSTemporalZone *zone,
                         uint32_t increment, QJSTemporalUnit unit, int shift)
{
    NudgeWindow w;
    int64_t count;
    int error;
    QJSTemporalDateDuration base = duration.date;
    if (unit == QJS_TEMPORAL_YEAR) {
        count = base.years;
        base = (QJSTemporalDateDuration){ 0, 0, 0, 0 };
    } else if (unit == QJS_TEMPORAL_MONTH) {
        count = base.months;
        base.months = base.weeks = base.days = 0;
    } else if (unit == QJS_TEMPORAL_WEEK) {
        QJSTemporalISODate start, end;
        QJSTemporalDateDuration difference;
        base.weeks = base.days = 0;
        error = qjs_temporal_calendar_date_add(calendar, origin.date, base,
                             QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &start);
        if (error) return error;
        if (qjs_temporal_iso_add_days(&end, start, duration.date.days) < 0)
            return QJS_TEMPORAL_ERROR_RANGE;
        error = qjs_temporal_calendar_date_until(calendar, start, end,
                                          QJS_TEMPORAL_WEEK, &difference);
        if (error) return error;
        count = duration.date.weeks + difference.weeks;
    } else {
        count = base.days;
        base.days = 0;
    }
    w.r1 = count / increment * increment;
    if (shift && unit <= QJS_TEMPORAL_MONTH)
        w.r1 += (int64_t)increment * sign;
    w.r2 = w.r1 + (int64_t)increment * sign;
    w.start_duration = w.end_duration = base;
    if (unit == QJS_TEMPORAL_YEAR) {
        w.start_duration.years = w.r1;
        w.end_duration.years = w.r2;
    } else if (unit == QJS_TEMPORAL_MONTH) {
        w.start_duration.months = w.r1;
        w.end_duration.months = w.r2;
    } else if (unit == QJS_TEMPORAL_WEEK) {
        w.start_duration.weeks = w.r1;
        w.end_duration.weeks = w.r2;
    } else {
        w.start_duration.days = w.r1;
        w.end_duration.days = w.r2;
    }
    if (!w.start_duration.years && !w.start_duration.months &&
        !w.start_duration.weeks && !w.start_duration.days) {
        w.start = origin_ns;
    } else {
        error = date_epoch(&w.start, origin, calendar, zone, w.start_duration);
        if (error) return error;
    }
    error = date_epoch(&w.end, origin, calendar, zone, w.end_duration);
    if (error) return error;
    *result = w;
    return 0;
}

static int window_contains(NudgeWindow w, QJSTemporalEpochNs dest, int sign)
{
    return qjs_temporal_epoch_ns_compare(dest, w.start) != -sign &&
           qjs_temporal_epoch_ns_compare(dest, w.end) != sign;
}

/* Exact midpoint comparisons; no floating point determines rounding. */
static int choose_end(NudgeWindow w, QJSTemporalEpochNs dest,
                        int sign, uint32_t increment,
                        QJSTemporalRoundingMode mode)
{
    QJSTemporalEpochNs progress, span, twice;
    int compare;
    if (qjs_temporal_epoch_ns_compare(dest, w.start) == 0)
        return 0;
    if (qjs_temporal_epoch_ns_compare(dest, w.end) == 0)
        return 1;
    if (mode == QJS_TEMPORAL_ROUND_CEIL) return sign > 0;
    if (mode == QJS_TEMPORAL_ROUND_FLOOR) return sign < 0;
    if (mode == QJS_TEMPORAL_ROUND_EXPAND) return 1;
    if (mode == QJS_TEMPORAL_ROUND_TRUNC) return 0;
    qjs_temporal_epoch_ns_subtract(&progress, dest, w.start);
    qjs_temporal_epoch_ns_subtract(&span, w.end, w.start);
    if (sign < 0) {
        qjs_temporal_epoch_ns_subtract(&progress, zero, progress);
        qjs_temporal_epoch_ns_subtract(&span, zero, span);
    }
    qjs_temporal_epoch_ns_multiply(&twice, progress, 2);
    compare = qjs_temporal_epoch_ns_compare(twice, span);
    if (compare) return compare > 0;
    if (mode == QJS_TEMPORAL_ROUND_HALF_CEIL) return sign > 0;
    if (mode == QJS_TEMPORAL_ROUND_HALF_FLOOR) return sign < 0;
    if (mode == QJS_TEMPORAL_ROUND_HALF_EXPAND) return 1;
    if (mode == QJS_TEMPORAL_ROUND_HALF_TRUNC) return 0;
    return (w.r1 / increment) % 2 != 0;
}

static int bubble(QJSTemporalInternalDuration *duration,
                    QJSTemporalEpochNs nudged,
                    QJSTemporalISODateTime origin,
                    QJSTemporalCalendar calendar, const QJSTemporalZone *zone,
                    QJSTemporalUnit largest, QJSTemporalUnit smallest, int sign)
{
    int unit, error;
    QJSTemporalDateDuration trial;
    QJSTemporalEpochNs end;
    for (unit = (int)smallest - 1; unit >= (int)largest; unit--) {
        if (unit == QJS_TEMPORAL_WEEK && largest != QJS_TEMPORAL_WEEK)
            continue;
        trial = duration->date;
        trial.days = 0;
        if (unit == QJS_TEMPORAL_YEAR) {
            trial.years += sign;
            trial.months = trial.weeks = 0;
        } else if (unit == QJS_TEMPORAL_MONTH) {
            trial.months += sign;
            trial.weeks = 0;
        } else if (unit == QJS_TEMPORAL_WEEK) {
            trial.weeks += sign;
        } else {
            continue;
        }
        error = date_epoch(&end, origin, calendar, zone, trial);
        if (error) return error;
        if (qjs_temporal_epoch_ns_compare(nudged, end) == -sign)
            break;
        duration->date = trial;
        duration->time = zero;
    }
    return 0;
}

int qjs_temporal_relative_round(
    QJSTemporalInternalDuration duration,
    QJSTemporalEpochNs origin, QJSTemporalEpochNs dest,
    QJSTemporalISODateTime one, const QJSTemporalZone *zone,
    QJSTemporalCalendar calendar,
    const QJSTemporalDifferenceSettings *settings,
    QJSTemporalInternalDuration *result)
{
    QJSTemporalInternalDuration out = duration;
    QJSTemporalEpochNs nudged;
    int error, sign, expanded = 0;
    QJSTemporalUnit smallest = settings->smallest_unit;
    if (qjs_temporal_epoch_ns_compare(origin, dest) == 0) {
        *result = (QJSTemporalInternalDuration){ {0}, {0} };
        return 0;
    }
    if (smallest == QJS_TEMPORAL_NANOSECOND &&
        settings->rounding_increment == 1) {
        *result = out;
        return 0;
    }
    if (smallest < QJS_TEMPORAL_YEAR || smallest > QJS_TEMPORAL_NANOSECOND ||
        !settings->rounding_increment ||
        settings->rounding_mode > QJS_TEMPORAL_ROUND_HALF_EVEN)
        return QJS_TEMPORAL_ERROR_RANGE;
    if (settings->largest_unit > QJS_TEMPORAL_DAY)
        zone = NULL;
    sign = qjs_temporal_epoch_ns_compare(dest, origin) < 0 ? -1 : 1;
    if (smallest <= QJS_TEMPORAL_WEEK ||
        (zone && smallest == QJS_TEMPORAL_DAY)) {
        NudgeWindow w;
        int end;
        error = nudge_window(&w, sign, out, origin, one, calendar, zone,
                              settings->rounding_increment, smallest, 0);
        if (error) return error;
        if (!window_contains(w, dest, sign)) {
            error = nudge_window(&w, sign, out, origin, one, calendar, zone,
                                  settings->rounding_increment, smallest, 1);
            if (error) return error;
            if (!window_contains(w, dest, sign))
                return QJS_TEMPORAL_ERROR_BACKEND;
            expanded = 1;
        }
        end = choose_end(w, dest, sign, settings->rounding_increment,
                           settings->rounding_mode);
        out.date = end ? w.end_duration : w.start_duration;
        out.time = zero;
        nudged = end ? w.end : w.start;
        expanded |= end;
    } else if (zone) {
        QJSTemporalISODateTime start = one, end = one;
        QJSTemporalEpochNs start_ns, end_ns, span, rounded, beyond;
        error = qjs_temporal_calendar_date_add(calendar, one.date,
                    out.date, QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &start.date);
        if (error) return error;
        if (qjs_temporal_iso_add_days(&end.date, start.date, sign))
            return QJS_TEMPORAL_ERROR_RANGE;
        error = qjs_temporal_zone_epoch(zone, start, QJS_TEMPORAL_COMPATIBLE,
                                          &start_ns);
        if (error) return error;
        error = qjs_temporal_zone_epoch(zone, end, QJS_TEMPORAL_COMPATIBLE,
                                          &end_ns);
        if (error) return error;
        if (qjs_temporal_epoch_ns_subtract(&span, end_ns, start_ns) ||
            pair_sign(span) != sign ||
            qjs_temporal_time_duration_round(&rounded, out.time,
                         settings->rounding_increment, smallest,
                         settings->rounding_mode) ||
            qjs_temporal_epoch_ns_subtract(&beyond, rounded, span))
            return QJS_TEMPORAL_ERROR_RANGE;
        expanded = pair_sign(beyond) != -sign;
        if (expanded) {
            out.date.days += sign;
            if (qjs_temporal_time_duration_round(&rounded, beyond,
                         settings->rounding_increment, smallest,
                         settings->rounding_mode) ||
                qjs_temporal_epoch_ns_add(&nudged, end_ns, rounded))
                return QJS_TEMPORAL_ERROR_RANGE;
        } else if (qjs_temporal_epoch_ns_add(&nudged, start_ns, rounded)) {
            return QJS_TEMPORAL_ERROR_RANGE;
        }
        out.time = rounded;
    } else {
        QJSTemporalEpochNs time, rounded, delta;
        int64_t days, rounded_days;
        if (add_days(&time, out.time, out.date.days) < 0 ||
            qjs_temporal_time_duration_round(&rounded, time,
                      settings->rounding_increment, smallest,
                      settings->rounding_mode) < 0 ||
            truncate_days(time, &days) < 0 ||
            truncate_days(rounded, &rounded_days) < 0 ||
            qjs_temporal_epoch_ns_subtract(&delta, rounded, time) < 0 ||
            qjs_temporal_epoch_ns_add(&nudged, dest, delta) < 0)
            return QJS_TEMPORAL_ERROR_RANGE;
        expanded = (rounded_days > days ? 1 : rounded_days < days ? -1 : 0)
                    == pair_sign(time);
        out.date.days = 0;
        out.time = rounded;
        if (settings->largest_unit <= QJS_TEMPORAL_DAY) {
            out.date.days = rounded_days;
            if (add_days(&out.time, rounded, -rounded_days) < 0)
                return QJS_TEMPORAL_ERROR_RANGE;
        }
    }
    if (expanded && smallest != QJS_TEMPORAL_YEAR &&
        smallest != QJS_TEMPORAL_WEEK) {
        QJSTemporalUnit start = smallest < QJS_TEMPORAL_DAY ?
                               smallest : QJS_TEMPORAL_DAY;
        error = bubble(&out, nudged, one, calendar, zone, settings->largest_unit,
                         start, sign);
        if (error) return error;
    }
    *result = out;
    return 0;
}

int qjs_temporal_plain_relative_round(
    QJSTemporalISODateTime one, QJSTemporalISODateTime two,
    QJSTemporalCalendar calendar, QJSTemporalInternalDuration duration,
    const QJSTemporalDifferenceSettings *settings,
    QJSTemporalInternalDuration *result)
{
    QJSTemporalEpochNs origin, dest;
    if (qjs_temporal_iso_datetime_to_epoch_ns(&origin, one) ||
        qjs_temporal_iso_datetime_to_epoch_ns(&dest, two))
        return QJS_TEMPORAL_ERROR_RANGE;
    return qjs_temporal_relative_round(duration, origin, dest, one, NULL,
                                         calendar, settings, result);
}

int qjs_temporal_plain_datetime_difference_round(
    QJSTemporalISODateTime one, QJSTemporalISODateTime two,
    QJSTemporalCalendar calendar,
    const QJSTemporalDifferenceSettings *settings,
    QJSTemporalInternalDuration *result)
{
    QJSTemporalInternalDuration difference;
    int error;
    if (qjs_temporal_iso_datetime_compare(one, two) == 0) {
        *result = (QJSTemporalInternalDuration){ { 0, 0, 0, 0 }, { 0, 0 } };
        return 0;
    }
    error = qjs_temporal_plain_datetime_difference(one, two, calendar,
                                       settings->largest_unit, &difference);
    if (error)
        return error;
    return qjs_temporal_plain_relative_round(one, two, calendar, difference,
                                               settings, result);
}

int qjs_temporal_relative_total(
    QJSTemporalInternalDuration diff,
    QJSTemporalEpochNs origin, QJSTemporalEpochNs dest,
    QJSTemporalISODateTime one, const QJSTemporalZone *zone,
    QJSTemporalCalendar calendar, QJSTemporalUnit unit, double *result)
{
    QJSTemporalEpochNs numerator, denominator;
    int error, sign;
    double value;
    if (unit < QJS_TEMPORAL_YEAR || unit > QJS_TEMPORAL_NANOSECOND)
        return QJS_TEMPORAL_ERROR_RANGE;
    if (qjs_temporal_epoch_ns_compare(origin, dest) == 0) {
        *result = 0;
        return 0;
    }
    if (unit <= QJS_TEMPORAL_WEEK || (zone && unit == QJS_TEMPORAL_DAY)) {
        NudgeWindow w;
        QJSTemporalEpochNs progress, product, partial;
        sign = qjs_temporal_epoch_ns_compare(dest, origin) < 0 ? -1 : 1;
        error = nudge_window(&w, sign, diff, origin, one, calendar, zone, 1, unit, 0);
        if (error) return error;
        if (!window_contains(w, dest, sign)) {
            error = nudge_window(&w, sign, diff, origin, one, calendar, zone,
                                  1, unit, 1);
            if (error) return error;
            if (!window_contains(w, dest, sign))
                return QJS_TEMPORAL_ERROR_BACKEND;
        }
        qjs_temporal_epoch_ns_subtract(&denominator, w.end, w.start);
        qjs_temporal_epoch_ns_subtract(&progress, dest, w.start);
        /* All Plain range calendar counts fit int32. */
        if (w.r1 < INT32_MIN || w.r1 > INT32_MAX ||
            qjs_temporal_epoch_ns_multiply(&product, denominator,
                                            (int32_t)w.r1) < 0 ||
            qjs_temporal_epoch_ns_multiply(&partial, progress, sign) < 0 ||
            qjs_temporal_epoch_ns_add(&numerator, product, partial) < 0)
            return QJS_TEMPORAL_ERROR_RANGE;
    } else {
        if (qjs_temporal_epoch_ns_subtract(&numerator, dest, origin) < 0)
            return QJS_TEMPORAL_ERROR_RANGE;
        denominator = qjs_temporal_epoch_ns_from_int64(
                         (int64_t)qjs_temporal_unit_nanoseconds(unit));
    }
    if (pair_sign(denominator) < 0) {
        qjs_temporal_epoch_ns_subtract(&numerator, zero, numerator);
        qjs_temporal_epoch_ns_subtract(&denominator, zero, denominator);
    }
    value = qjs_temporal_ratio_to_double(numerator, denominator);
    *result = value;
    return 0;
}

int qjs_temporal_plain_datetime_difference_total(
    QJSTemporalISODateTime one, QJSTemporalISODateTime two,
    QJSTemporalCalendar calendar, QJSTemporalUnit unit, double *result)
{
    QJSTemporalInternalDuration difference;
    QJSTemporalEpochNs origin, dest;
    int error;
    if (qjs_temporal_iso_datetime_compare(one, two) == 0) {
        *result = 0;
        return 0;
    }
    error = qjs_temporal_plain_datetime_difference(one, two, calendar,
                                                    unit, &difference);
    if (error) return error;
    if (qjs_temporal_iso_datetime_to_epoch_ns(&origin, one) ||
        qjs_temporal_iso_datetime_to_epoch_ns(&dest, two))
        return QJS_TEMPORAL_ERROR_RANGE;
    return qjs_temporal_relative_total(difference, origin, dest, one, NULL,
                                         calendar, unit, result);
}
