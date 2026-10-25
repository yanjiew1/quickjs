/* Calendar-relative date arithmetic with checked, bounded unit searches.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include "noniso-calendar.h"
#include "civil.h"
#include "../calendar/calendar.h"
#include <limits.h>
#include <string.h>

#define RANGE QJS_TEMPORAL_ERROR_RANGE

static int add_checked(int64_t *result, int64_t a, int64_t b)
{
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b))
        return RANGE;
    *result = a + b;
    return 0;
}

typedef struct TargetMonth {
    int32_t year;
    int month, maximum_day;
} TargetMonth;

static int year_month(QJSCalendarId calendar, QJSTemporalCalendarDate record,
                      int64_t years, int64_t months, QJSTemporalOverflow overflow,
                      TargetMonth *result)
{
    int64_t year;
    int32_t target_year;
    int month, target_month, count, maximum, error;
    char code[5];

    if (add_checked(&year, record.year, years) ||
        year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR)
        return RANGE;
    error = qjs_calendar_month_ordinal(calendar, (int32_t)year, record.month_code,
                                       overflow == QJS_TEMPORAL_OVERFLOW_CONSTRAIN,
                                       &month);
    if (error)
        return error;
    error = qjs_calendar_add_months(calendar, (int32_t)year, month, months,
                                    &target_year, &target_month);
    if (error)
        return error;
    error = qjs_calendar_month_info(calendar, target_year, target_month,
                                    &count, &maximum, code);
    if (error)
        return error;
    *result = (TargetMonth){target_year, target_month, maximum};
    return 0;
}

int qjs_temporal_noniso_date_add(QJSCalendarId calendar, QJSTemporalISODate date,
                                 QJSTemporalDateDuration duration,
                                 QJSTemporalOverflow overflow,
                                 QJSTemporalISODate *result)
{
    QJSTemporalCalendarDate record;
    TargetMonth month;
    QJSTemporalISODate target;
    int64_t days, extra, first_day;
    int day, error;

    if (overflow != QJS_TEMPORAL_OVERFLOW_CONSTRAIN &&
        overflow != QJS_TEMPORAL_OVERFLOW_REJECT)
        return RANGE;
    error = qjs_temporal_noniso_fields(calendar, date, &record);
    if (error)
        return error;
    error = year_month(calendar, record, duration.years, duration.months,
                        overflow, &month);
    if (error)
        return error;
    /* Comparisons need only the checked month; addition also needs its origin. */
    error = qjs_calendar_to_epoch_day(calendar, month.year, month.month, 1, &first_day);
    if (error)
        return error;
    day = record.day;
    if (day > month.maximum_day) {
        if (overflow == QJS_TEMPORAL_OVERFLOW_REJECT)
            return RANGE;
        day = month.maximum_day;
    }
    /* A month start may precede Temporal bounds; constrain only the final date. */
    if (duration.weeks < INT64_MIN / 7 || duration.weeks > INT64_MAX / 7 ||
        add_checked(&extra, duration.weeks * 7, duration.days) ||
        add_checked(&extra, extra, day - 1) ||
        add_checked(&days, first_day, extra) ||
        qjs_iso_date_from_days(&target, days) ||
        !qjs_temporal_iso_date_within_limits(target))
        return RANGE;
    *result = target;
    return 0;
}

static int surpasses(QJSCalendarId calendar, int sign,
                      QJSTemporalCalendarDate from, QJSTemporalCalendarDate to,
                      int64_t years, int64_t months, int *result)
{
    TargetMonth month;
    int64_t year;
    int error, order;

    if (add_checked(&year, from.year, years) ||
        year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR) {
        *result = 1;
        return 0;
    }
    /* Compare the original month code before constraining a missing leap
       month in the target year, as NonISODateSurpasses requires. */
    if ((year > to.year && sign > 0) || (year < to.year && sign < 0)) {
        *result = 1;
        return 0;
    }
    if (year == to.year) {
        int comparison = strcmp(from.month_code, to.month_code);

        if ((comparison > 0 && sign > 0) || (comparison < 0 && sign < 0) ||
            (!comparison && (from.day - to.day) * sign > 0)) {
            *result = 1;
            return 0;
        }
    }
    error = year_month(calendar, from, years, months,
                        QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &month);
    if (error) {
        if (error != RANGE)
            return error;
        *result = 1;
        return 0;
    }
    /* Without weeks/days, retain an impossible day such as February 30
       instead of clamping it or rebuilding a full calendar field record. */
    if (month.year != to.year)
        order = month.year < to.year ? -1 : 1;
    else if (month.month != to.month)
        order = month.month < to.month ? -1 : 1;
    else
        order = from.day < to.day ? -1 : from.day > to.day;
    *result = order * sign > 0;
    return 0;
}

static int count_units(QJSCalendarId calendar, int sign,
                        QJSTemporalCalendarDate from, QJSTemporalCalendarDate to,
                        int years_unit, int64_t years, int64_t upper,
                        int64_t *result)
{
    int64_t low = 0, high = upper;
    int error, passed;

    /* Equivalent to the specification's monotone incremental search, bounded
       logarithmically even when the dates are hundreds of millennia apart. */
    while (low < high) {
        int64_t middle = low + (high - low + 1) / 2;

        error = surpasses(calendar, sign, from, to,
                           years_unit ? middle * sign : years,
                           years_unit ? 0 : middle * sign, &passed);
        if (error)
            return error;
        if (passed)
            high = middle - 1;
        else
            low = middle;
    }
    *result = low * sign;
    return 0;
}

int qjs_temporal_noniso_date_until(QJSCalendarId calendar,
                                   QJSTemporalISODate start,
                                   QJSTemporalISODate end,
                                   QJSTemporalUnit largest,
                                   QJSTemporalDateDuration *result)
{
    QJSTemporalDateDuration duration = {0};
    QJSTemporalCalendarDate from, to;
    QJSTemporalISODate anchor;
    int64_t start_days, end_days, anchor_days, magnitude;
    int sign, error;

    if (!qjs_calendar_is_supported(calendar))
        return QJS_TEMPORAL_ERROR_UNSUPPORTED;
    if ((unsigned)largest > QJS_TEMPORAL_DAY ||
        qjs_iso_date_to_days(&start_days, start) || qjs_iso_date_to_days(&end_days, end))
        return RANGE;
    sign = end_days > start_days ? 1 : end_days < start_days ? -1 : 0;
    if (!sign) {
        *result = duration;
        return 0;
    }
    if (largest == QJS_TEMPORAL_DAY || largest == QJS_TEMPORAL_WEEK) {
        duration.days = end_days - start_days;
        if (largest == QJS_TEMPORAL_WEEK) {
            duration.weeks = duration.days / 7;
            duration.days %= 7;
        }
        *result = duration;
        return 0;
    }
    error = qjs_temporal_noniso_fields(calendar, start, &from);
    if (error)
        return error;
    error = qjs_temporal_noniso_fields(calendar, end, &to);
    if (error)
        return error;
    magnitude = (int64_t)to.year - from.year;
    if (magnitude < 0)
        magnitude = -magnitude;
    if (largest == QJS_TEMPORAL_YEAR) {
        error = count_units(calendar, sign, from, to, 1, 0, magnitude + 1, &duration.years);
        if (error)
            return error;
    }
    /* Five days (the shortest Coptic intercalary month) is a safe lower
       bound on every supported month, including years near public limits. */
    magnitude = (end_days - start_days) * sign / 5 + 14;
    error = count_units(calendar, sign, from, to, 0, duration.years, magnitude,
                         &duration.months);
    if (error)
        return error;
    error = qjs_temporal_noniso_date_add(calendar, start, duration,
                                        QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &anchor);
    if (error)
        return error;
    if (qjs_iso_date_to_days(&anchor_days, anchor))
        return RANGE;
    duration.days = end_days - anchor_days;
    *result = duration;
    return 0;
}
