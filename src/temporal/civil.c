/* Portable Temporal civil arithmetic.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, as in epoch.c.
 */
#include "civil.h"
#include <limits.h>
#include <stdio.h>

#define NS_PER_DAY UINT64_C(86400000000000)

static int add64(int64_t *result, int64_t a, int64_t b)
{
    if ((b > 0 && a > INT64_MAX - b) ||
        (b < 0 && a < INT64_MIN - b))
        return -1;
    *result = a + b;
    return 0;
}

static int64_t floor_div(int64_t a, int64_t b)
{
    int64_t q = a / b;
    return q - (a % b < 0);
}

int qjs_temporal_iso_leap_year(int32_t year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int qjs_temporal_iso_days_in_month(int32_t year, int32_t month)
{
    static const unsigned char lengths[] =
        { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12)
        return 0;
    return lengths[month - 1] +
           (month == 2 && qjs_temporal_iso_leap_year(year));
}

int qjs_temporal_iso_date_compare(QJSTemporalISODate a,
                                   QJSTemporalISODate b)
{
    if (a.year != b.year)
        return a.year < b.year ? -1 : 1;
    if (a.month != b.month)
        return a.month < b.month ? -1 : 1;
    return (a.day > b.day) - (a.day < b.day);
}

int qjs_temporal_iso_datetime_compare(QJSTemporalISODateTime a,
                                       QJSTemporalISODateTime b)
{
    int r = qjs_temporal_iso_date_compare(a.date, b.date);
    const int32_t av[] = { a.time.hour, a.time.minute, a.time.second,
        a.time.millisecond, a.time.microsecond, a.time.nanosecond };
    const int32_t bv[] = { b.time.hour, b.time.minute, b.time.second,
        b.time.millisecond, b.time.microsecond, b.time.nanosecond };
    int i;
    if (r)
        return r;
    for (i = 0; i < 6; i++) {
        if (av[i] != bv[i])
            return av[i] < bv[i] ? -1 : 1;
    }
    return 0;
}

int qjs_temporal_iso_date_within_limits(QJSTemporalISODate date)
{
    int64_t days;
    return qjs_temporal_iso_date_to_days(&days, date) == 0 &&
           days >= -INT64_C(100000001) && days <= INT64_C(100000000);
}

int qjs_temporal_iso_datetime_within_limits(QJSTemporalISODateTime value)
{
    int64_t days;
    if (!qjs_temporal_iso_time_is_valid(value.time) ||
        qjs_temporal_iso_date_to_days(&days, value.date) < 0)
        return 0;
    if (days < -INT64_C(100000001) || days > INT64_C(100000000))
        return 0;
    /* The endpoints are strict; only the earliest midnight is excluded. */
    return days != -INT64_C(100000001) || value.time.hour ||
           value.time.minute || value.time.second || value.time.millisecond ||
           value.time.microsecond || value.time.nanosecond;
}

int qjs_temporal_iso_year_month_within_limits(QJSTemporalISODate date)
{
    return qjs_temporal_iso_date_is_valid(date) &&
           date.year >= -271821 && date.year <= 275760 &&
           (date.year != -271821 || date.month >= 4) &&
           (date.year != 275760 || date.month <= 9);
}

int qjs_temporal_iso_regulate_date(QJSTemporalISODate *result,
                                  int64_t year, int64_t month, int64_t day,
                                  QJSTemporalOverflow overflow)
{
    QJSTemporalISODate date;
    int maximum;
    if (year < INT32_MIN || year > INT32_MAX ||
        (overflow != QJS_TEMPORAL_OVERFLOW_CONSTRAIN &&
         overflow != QJS_TEMPORAL_OVERFLOW_REJECT))
        return -1;
    if (overflow == QJS_TEMPORAL_OVERFLOW_CONSTRAIN) {
        if (month < 1) month = 1;
        if (month > 12) month = 12;
    } else if (month < 1 || month > 12) {
        return -1;
    }
    maximum = qjs_temporal_iso_days_in_month((int32_t)year, (int32_t)month);
    if (overflow == QJS_TEMPORAL_OVERFLOW_CONSTRAIN) {
        if (day < 1) day = 1;
        if (day > maximum) day = maximum;
    } else if (day < 1 || day > maximum) {
        return -1;
    }
    date.year = (int32_t)year;
    date.month = (int32_t)month;
    date.day = (int32_t)day;
    *result = date;
    return 0;
}

int qjs_temporal_iso_add_days(QJSTemporalISODate *result,
                              QJSTemporalISODate date, int64_t days)
{
    int64_t base, sum;
    if (qjs_temporal_iso_date_to_days(&base, date) < 0 ||
        add64(&sum, base, days) < 0)
        return -1;
    return qjs_temporal_iso_date_from_days(result, sum);
}

int qjs_temporal_iso_date_add(QJSTemporalISODate *result,
                              QJSTemporalISODate date,
                              QJSTemporalDateDuration duration,
                              QJSTemporalOverflow overflow)
{
    int64_t year, months, year_month, days;
    QJSTemporalISODate intermediate;
    if (!qjs_temporal_iso_date_is_valid(date) ||
        add64(&year, date.year, duration.years) < 0 ||
        year < INT64_MIN / 12 || year > INT64_MAX / 12 ||
        add64(&months, (int64_t)date.month - 1, duration.months) < 0 ||
        add64(&year_month, year * 12, months) < 0)
        return -1;
    year = floor_div(year_month, 12);
    if (year < INT32_MIN || year > INT32_MAX)
        return -1;
    months = year_month - year * 12 + 1;
    if (qjs_temporal_iso_regulate_date(&intermediate, year, months,
                                      date.day, overflow) < 0 ||
        duration.weeks < INT64_MIN / 7 || duration.weeks > INT64_MAX / 7 ||
        add64(&days, duration.days, duration.weeks * 7) < 0)
        return -1;
    return qjs_temporal_iso_add_days(result, intermediate, days);
}

static int overshoots(QJSTemporalISODate candidate,
                      QJSTemporalISODate target, int sign)
{
    return qjs_temporal_iso_date_compare(candidate, target) == sign;
}

int qjs_temporal_iso_date_until(QJSTemporalDateDuration *result,
                                QJSTemporalISODate one,
                                QJSTemporalISODate two,
                                QJSTemporalUnit largest_unit)
{
    QJSTemporalDateDuration d = { 0, 0, 0, 0 };
    QJSTemporalISODate candidate;
    int64_t start_days, end_days, ordinal, year;
    int sign = -qjs_temporal_iso_date_compare(one, two);
    if (!qjs_temporal_iso_date_is_valid(one) ||
        !qjs_temporal_iso_date_is_valid(two) ||
        largest_unit < QJS_TEMPORAL_YEAR || largest_unit > QJS_TEMPORAL_DAY)
        return -1;
    if (sign && largest_unit == QJS_TEMPORAL_YEAR) {
        d.years = (int64_t)two.year - one.year;
        candidate = one;
        candidate.year = two.year;
        /* ISODateSurpasses compares the original, possibly invalid day.
           A leap-day year probe must not be constrained to February 28. */
        if (overshoots(candidate, two, sign))
            d.years -= sign;
    }
    if (sign && largest_unit <= QJS_TEMPORAL_MONTH) {
        d.months = ((int64_t)two.year - one.year - d.years) * 12 +
                   two.month - one.month;
        ordinal = ((int64_t)one.year + d.years) * 12 +
                  one.month - 1 + d.months;
        year = floor_div(ordinal, 12);
        if (year < INT32_MIN || year > INT32_MAX)
            return -1;
        candidate.year = (int32_t)year;
        candidate.month = (int32_t)(ordinal - year * 12 + 1);
        candidate.day = one.day;
        /* January 31 to February 28 is 28 days, not a constrained month. */
        if (overshoots(candidate, two, sign))
            d.months -= sign;
    }
    if (qjs_temporal_iso_date_add(&candidate, one, d,
                                  QJS_TEMPORAL_OVERFLOW_CONSTRAIN) < 0 ||
        qjs_temporal_iso_date_to_days(&start_days, candidate) < 0 ||
        qjs_temporal_iso_date_to_days(&end_days, two) < 0)
        return -1;
    d.days = end_days - start_days;
    if (largest_unit == QJS_TEMPORAL_WEEK) {
        d.weeks = d.days / 7;
        d.days %= 7;
    }
    *result = d;
    return 0;
}

int qjs_temporal_iso_day_of_week(QJSTemporalISODate date)
{
    int64_t days;
    if (qjs_temporal_iso_date_to_days(&days, date) < 0)
        return 0;
    days = (days + 3) % 7;
    if (days < 0) days += 7;
    return (int)days + 1;
}

int qjs_temporal_iso_day_of_year(QJSTemporalISODate date)
{
    int64_t days, january;
    QJSTemporalISODate start = { date.year, 1, 1 };
    if (qjs_temporal_iso_date_to_days(&days, date) < 0 ||
        qjs_temporal_iso_date_to_days(&january, start) < 0)
        return 0;
    return (int)(days - january + 1);
}

int qjs_temporal_iso_week_of_year(QJSTemporalISODate date,
                                 int32_t *year_of_week)
{
    QJSTemporalISODate thursday, fourth;
    int64_t days, january;
    int weekday = qjs_temporal_iso_day_of_week(date);
    if (!weekday || qjs_temporal_iso_add_days(&thursday, date,
                                             4 - weekday) < 0)
        return 0;
    fourth.year = thursday.year;
    fourth.month = 1;
    fourth.day = 4;
    if (qjs_temporal_iso_date_to_days(&days, thursday) < 0 ||
        qjs_temporal_iso_date_to_days(&january, fourth) < 0)
        return 0;
    january -= qjs_temporal_iso_day_of_week(fourth) - 1;
    *year_of_week = thursday.year;
    return (int)((days - january) / 7 + 1);
}

int qjs_temporal_iso_datetime_add_time(QJSTemporalISODateTime *result,
                                       QJSTemporalISODateTime value,
                                       QJSTemporalEpochNs nanoseconds)
{
    QJSTemporalEpochNs epoch, sum;
    QJSTemporalISODateTime out;
    if (qjs_temporal_iso_datetime_to_epoch_ns(&epoch, value) < 0 ||
        qjs_temporal_epoch_ns_add(&sum, epoch, nanoseconds) < 0 ||
        qjs_temporal_iso_datetime_from_epoch_ns(&out, sum) < 0)
        return -1;
    *result = out;
    return 0;
}

int qjs_temporal_iso_datetime_round(QJSTemporalISODateTime *result,
                                    QJSTemporalISODateTime value,
                                    uint64_t increment_nanoseconds,
                                    QJSTemporalRoundingMode mode)
{
    QJSTemporalISODateTime midnight = value, out;
    QJSTemporalEpochNs epoch, start, time, rounded;
    if (!increment_nanoseconds || increment_nanoseconds > NS_PER_DAY)
        return -1;
    midnight.time = (QJSTemporalISOTime){ 0, 0, 0, 0, 0, 0 };
    if (qjs_temporal_iso_datetime_to_epoch_ns(&epoch, value) < 0 ||
        qjs_temporal_iso_datetime_to_epoch_ns(&start, midnight) < 0 ||
        qjs_temporal_epoch_ns_subtract(&time, epoch, start) < 0 ||
        qjs_temporal_epoch_ns_round(&rounded, time,
                                   increment_nanoseconds, mode) < 0 ||
        qjs_temporal_iso_datetime_add_time(&out, midnight, rounded) < 0)
        return -1;
    *result = out;
    return 0;
}

int qjs_temporal_iso_date_format(char *buffer, size_t capacity,
                                 QJSTemporalISODate date)
{
    int length;
    if (!qjs_temporal_iso_date_is_valid(date))
        return -1;
    if (date.year >= 0 && date.year <= 9999)
        length = snprintf(buffer, capacity, "%04d-%02d-%02d",
                          date.year, date.month, date.day);
    else
        length = snprintf(buffer, capacity, "%c%06lld-%02d-%02d",
                          date.year < 0 ? '-' : '+',
                          date.year < 0 ? -(long long)date.year :
                                          (long long)date.year,
                          date.month, date.day);
    return length >= 0 && (size_t)length < capacity ? length : -1;
}
