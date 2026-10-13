/* Native Temporal calendar resolution and arithmetic.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 * Era/MonthCode 5833eae6, 2026-03-19 supplies the non-ISO algorithms.
 */
#include "calendar.h"
#include "civil.h"
#include "../calendar/calendar.h"
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define HAS(f, name) ((f)->present & QJS_TEMPORAL_FIELD_##name)
#define RANGE QJS_TEMPORAL_ERROR_RANGE
#define MISSING QJS_TEMPORAL_ERROR_MISSING
static int has_era(QJSTemporalCalendar calendar)
{
    return calendar != QJS_TEMPORAL_CAL_ISO8601 &&
        calendar != QJS_TEMPORAL_CAL_CHINESE && calendar != QJS_TEMPORAL_CAL_DANGI;
}
const char *qjs_temporal_calendar_identifier(QJSTemporalCalendar calendar)
{
    return qjs_calendar_identifier((QJSCalendarId)calendar);
}
int qjs_temporal_calendar_from_identifier(QJSTemporalCalendar *result,
                                         const char *text, size_t length)
{
    QJSCalendarId id;
    int error = qjs_calendar_from_identifier(&id, text, length);
    if (!error) *result = (QJSTemporalCalendar)id;
    return error;
}
unsigned qjs_temporal_calendar_extra_fields(QJSTemporalCalendar calendar,
                                             unsigned requested)
{
    return has_era(calendar) && (requested & QJS_TEMPORAL_FIELD_YEAR) ?
        QJS_TEMPORAL_FIELD_ERA | QJS_TEMPORAL_FIELD_ERA_YEAR : 0;
}
unsigned qjs_temporal_calendar_fields_to_ignore(QJSTemporalCalendar calendar,
                                                 unsigned additional)
{
    unsigned result = additional;
    if (additional & (QJS_TEMPORAL_FIELD_MONTH | QJS_TEMPORAL_FIELD_MONTH_CODE))
        result |= QJS_TEMPORAL_FIELD_MONTH | QJS_TEMPORAL_FIELD_MONTH_CODE;
    if (has_era(calendar) && (additional & (QJS_TEMPORAL_FIELD_YEAR |
                    QJS_TEMPORAL_FIELD_ERA | QJS_TEMPORAL_FIELD_ERA_YEAR)))
        result |= QJS_TEMPORAL_FIELD_YEAR | QJS_TEMPORAL_FIELD_ERA |
            QJS_TEMPORAL_FIELD_ERA_YEAR;
    if (calendar == QJS_TEMPORAL_CAL_JAPANESE &&
        (additional & (QJS_TEMPORAL_FIELD_DAY | QJS_TEMPORAL_FIELD_MONTH |
                       QJS_TEMPORAL_FIELD_MONTH_CODE)))
        result |= QJS_TEMPORAL_FIELD_ERA | QJS_TEMPORAL_FIELD_ERA_YEAR;
    return result;
}
static int checked_i32(double value, int32_t *result)
{
    if (!isfinite(value) || trunc(value) != value ||
        value < INT32_MIN || value > INT32_MAX) return RANGE;
    *result = (int32_t)value;
    return 0;
}
static int month_code_number(const char *code, int *number, int *is_leap)
{
    size_t length = strlen(code);
    if ((length != 3 && length != 4) || code[0] != 'M' ||
        code[1] < '0' || code[1] > '9' || code[2] < '0' || code[2] > '9' ||
        (length == 4 && code[3] != 'L')) return RANGE;
    *number = (code[1] - '0') * 10 + code[2] - '0';
    *is_leap = length == 4;
    return 0;
}
static int valid_month_code(QJSTemporalCalendar calendar, const char *code)
{
    int month, is_leap;
    if (month_code_number(code, &month, &is_leap)) return 0;
    if (!is_leap) return month >= 1 && (month <= 12 || (month == 13 &&
        (calendar == QJS_TEMPORAL_CAL_COPTIC || calendar == QJS_TEMPORAL_CAL_ETHIOAA ||
         calendar == QJS_TEMPORAL_CAL_ETHIOPIC)));
    return (month >= 1 && month <= 12 &&
        (calendar == QJS_TEMPORAL_CAL_CHINESE || calendar == QJS_TEMPORAL_CAL_DANGI)) ||
        (calendar == QJS_TEMPORAL_CAL_HEBREW && month == 5);
}
static void make_month_code(char result[5], int month, int is_leap)
{
    result[0] = 'M'; result[1] = (char)('0' + month / 10);
    result[2] = (char)('0' + month % 10); result[3] = is_leap ? 'L' : 0;
    result[4] = 0;
}
static int resolve_era(QJSTemporalCalendar calendar,
                       QJSTemporalCalendarFields *fields)
{
    int32_t era_year, year;
    int error;
    if (!has_era(calendar)) return 0;
    if (!!HAS(fields, ERA) != !!HAS(fields, ERA_YEAR)) return MISSING;
    if (!HAS(fields, ERA_YEAR)) return 0;
    if (checked_i32(fields->era_year, &era_year)) return RANGE;
    error = qjs_calendar_year_from_era((QJSCalendarId)calendar, fields->era,
                                        era_year, &year);
    if (error) return error;
    if (HAS(fields, YEAR) && fields->year != year) return RANGE;
    fields->year = year;
    fields->present |= QJS_TEMPORAL_FIELD_YEAR;
    fields->present &= ~(QJS_TEMPORAL_FIELD_ERA | QJS_TEMPORAL_FIELD_ERA_YEAR);
    return 0;
}
static int month_info(QJSTemporalCalendar calendar, int32_t year, int month,
                       int *months, int *days, char code[5])
{
    return qjs_calendar_month_info((QJSCalendarId)calendar, year, month,
                                    months, days, code);
}
static int month_ordinal(QJSTemporalCalendar calendar, int32_t year,
                          const char *code, QJSTemporalOverflow overflow,
                          int *result)
{
    return qjs_calendar_month_ordinal((QJSCalendarId)calendar, year, code,
                         overflow == QJS_TEMPORAL_OVERFLOW_CONSTRAIN, result);
}
static int calendar_integers_to_iso(QJSTemporalCalendar calendar, int32_t year,
                                     int month, int day, QJSTemporalISODate *result)
{
    int64_t days;
    int error = qjs_calendar_to_epoch_day_unbounded((QJSCalendarId)calendar,
                                                    year, month, day, &days);
    if (error) return error;
    return qjs_temporal_iso_date_from_days(result, days) ? RANGE : 0;
}
static int resolve(QJSTemporalCalendar calendar,
                    const QJSTemporalCalendarFields *input, int type,
                    QJSTemporalOverflow overflow, QJSTemporalISODate *result)
{
    QJSTemporalCalendarFields fields = *input;
    int32_t year;
    int month, day, max_day, error, needs_year;
    double numeric;
    needs_year = type != 2 || (calendar != QJS_TEMPORAL_CAL_ISO8601 &&
        (!HAS(&fields, MONTH_CODE) || HAS(&fields, MONTH)));
    if (needs_year && !HAS(&fields, YEAR) &&
        (!has_era(calendar) || !HAS(&fields, ERA) || !HAS(&fields, ERA_YEAR)))
        return MISSING;
    if (type != 1 && !HAS(&fields, DAY)) return MISSING;
    if (!HAS(&fields, MONTH) && !HAS(&fields, MONTH_CODE)) return MISSING;
    error = resolve_era(calendar, &fields);
    if (error) return error;
    if (HAS(&fields, MONTH_CODE) && !valid_month_code(calendar, fields.month_code))
        return RANGE;
    if (calendar == QJS_TEMPORAL_CAL_ISO8601) {
        if (HAS(&fields, MONTH_CODE)) {
            int is_leap;
            if (month_code_number(fields.month_code, &month, &is_leap)) return RANGE;
            if (HAS(&fields, MONTH) && fields.month != month) return RANGE;
        } else {
            numeric = fields.month;
            if (numeric < 1 || !isfinite(numeric)) return RANGE;
            if (numeric > 12) {
                if (overflow == QJS_TEMPORAL_OVERFLOW_REJECT) return RANGE;
                numeric = 12;
            }
            month = (int)numeric;
        }
        if (type == 2) {
            /* MonthDay's supplied year determines leap-day regulation but
               does not bound the reference year. Retain integral Numbers
               beyond int32_t and use their exact remainder modulo 400. */
            int leap_year = HAS(&fields, YEAR) ?
                (fmod(fields.year, 4) == 0 &&
                 (fmod(fields.year, 100) != 0 || fmod(fields.year, 400) == 0)) : 1;
            year = 1972;
            max_day = qjs_temporal_iso_days_in_month(year, month) - (month == 2 && !leap_year);
        } else {
            if (checked_i32(fields.year, &year)) return RANGE;
            max_day = qjs_temporal_iso_days_in_month(year, month);
        }
        numeric = type == 1 ? 1 : fields.day;
        if (!isfinite(numeric) || numeric < 1) return RANGE;
        if (numeric > max_day) {
            if (overflow == QJS_TEMPORAL_OVERFLOW_REJECT) return RANGE;
            numeric = max_day;
        }
        day = (int)numeric;
        *result = (QJSTemporalISODate){year, month, day};
        return 0;
    }
    if (!HAS(&fields, YEAR)) return MISSING; /* MonthDay has its own reference search. */
    if (checked_i32(fields.year, &year) || year < -1000000 || year > 1000000) return RANGE;
    if (HAS(&fields, MONTH_CODE)) {
        error = month_ordinal(calendar, year, fields.month_code,
                              QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &month);
        if (error) return error;
        if (HAS(&fields, MONTH) && fields.month != month) return RANGE;
        if (overflow == QJS_TEMPORAL_OVERFLOW_REJECT) {
            error = month_ordinal(calendar, year, fields.month_code, overflow, &month);
            if (error) return error;
        }
    } else {
        int months, days;
        char code[5];
        error = month_info(calendar, year, 1, &months, &days, code);
        if (error) return error;
        numeric = fields.month;
        if (!isfinite(numeric) || numeric < 1) return RANGE;
        if (numeric > months) {
            if (overflow == QJS_TEMPORAL_OVERFLOW_REJECT) return RANGE;
            numeric = months;
        }
        month = (int)numeric;
    }
    {
        int months;
        char code[5];
        error = month_info(calendar, year, month, &months, &max_day, code);
        if (error) return error;
    }
    numeric = type == 1 ? 1 : fields.day;
    if (!isfinite(numeric) || numeric < 1) return RANGE;
    if (numeric > max_day) {
        if (overflow == QJS_TEMPORAL_OVERFLOW_REJECT) return RANGE;
        numeric = max_day;
    }
    return calendar_integers_to_iso(calendar, year, month, (int)numeric, result);
}
int qjs_temporal_calendar_date_from_fields(QJSTemporalCalendar calendar,
                    const QJSTemporalCalendarFields *fields,
                    QJSTemporalOverflow overflow, QJSTemporalISODate *result)
{
    int error = resolve(calendar, fields, 0, overflow, result);

    if (!error && !qjs_temporal_iso_date_within_limits(*result))
        return RANGE;
    return error;
}
int qjs_temporal_calendar_year_month_from_fields(QJSTemporalCalendar calendar,
                    const QJSTemporalCalendarFields *fields,
                    QJSTemporalOverflow overflow, QJSTemporalISODate *result)
{
    return resolve(calendar, fields, 1, overflow, result);
}
int qjs_temporal_calendar_fields(QJSTemporalCalendar calendar,
                                QJSTemporalISODate date,
                                QJSTemporalCalendarDate *result)
{
    QJSTemporalCalendarDate fields = {0};
    int64_t days;
    if (qjs_temporal_iso_date_to_days(&days, date)) return RANGE;
    fields.day_of_week = qjs_temporal_iso_day_of_week(date);
    fields.days_in_week = 7;
    if (calendar == QJS_TEMPORAL_CAL_ISO8601) {
        fields.year = date.year; fields.month = date.month; fields.day = date.day;
        make_month_code(fields.month_code, date.month, 0);
        fields.day_of_year = qjs_temporal_iso_day_of_year(date);
        fields.days_in_month = qjs_temporal_iso_days_in_month(date.year, date.month);
        fields.days_in_year = 365 + qjs_temporal_iso_leap_year(date.year);
        fields.months_in_year = 12; fields.in_leap_year = qjs_temporal_iso_leap_year(date.year);
        fields.week_of_year = qjs_temporal_iso_week_of_year(date, &fields.year_of_week);
        fields.has_week = 1;
    } else {
        QJSCalendarDate native;
        int error = qjs_calendar_from_epoch_day_unbounded((QJSCalendarId)calendar,
                                                           days, &native);
        if (error) return error;
        fields.year = native.year; fields.month = native.month; fields.day = native.day;
        fields.era_year = native.era_year;
        memcpy(fields.month_code, native.month_code, sizeof(fields.month_code));
        memcpy(fields.era, native.era, sizeof(fields.era));
        fields.day_of_year = native.day_of_year;
        fields.days_in_month = native.days_in_month;
        fields.days_in_year = native.days_in_year;
        fields.months_in_year = native.months_in_year;
        fields.has_era = native.has_era; fields.in_leap_year = native.in_leap_year;
    }
    *result = fields;
    return 0;
}
int qjs_temporal_calendar_month_day_from_fields(QJSTemporalCalendar calendar,
                    const QJSTemporalCalendarFields *input,
                    QJSTemporalOverflow overflow, QJSTemporalISODate *result)
{
    if (calendar == QJS_TEMPORAL_CAL_ISO8601)
        return resolve(calendar, input, 2, overflow, result);
    {
        QJSTemporalCalendarFields fields = *input;
        QJSTemporalCalendarDate record;
        QJSTemporalISODate source, candidate;
        int64_t limit, days, low, high;
        int error, desired_day, maximum = 0, reference_year = 0, number, is_leap;
        char code[5];
        if (!HAS(&fields, DAY)) return MISSING;
        if (!HAS(&fields, MONTH) && !HAS(&fields, MONTH_CODE)) return MISSING;
        error = resolve_era(calendar, &fields);
        if (error) return error;
        if (HAS(&fields, MONTH_CODE) && !valid_month_code(calendar, fields.month_code)) return RANGE;
        if (HAS(&fields, YEAR)) {
            int32_t arithmetic_year;
            if (checked_i32(fields.year, &arithmetic_year)) return RANGE;
            /* Chinese/Dangi arithmetic years are Gregorian years. Reject
               an impossible year before asking the calendar backend to calculate its months. */
            if ((calendar == QJS_TEMPORAL_CAL_CHINESE ||
                 calendar == QJS_TEMPORAL_CAL_DANGI) &&
                (arithmetic_year < -271821 || arithmetic_year > 275760))
                return RANGE;
            int supported;
            /* Guard the supplied year before lunar month lookup, as the
             * reference-date operation requires. An edge year may include
             * dates outside the public construction domain. */
            error = qjs_calendar_year_has_supported_date((QJSCalendarId)calendar,
                                                          arithmetic_year, &supported);
            if (error) return error;
            if (!supported) return RANGE;
            error = resolve(calendar, &fields, 0, overflow, &source);
            if (error) return error;
            error = qjs_temporal_calendar_fields(calendar, source, &record);
            if (error) return error;
            strcpy(code, record.month_code); desired_day = record.day;
        } else {
            if (HAS(&fields, MONTH) || !HAS(&fields, MONTH_CODE)) return MISSING;
            strcpy(code, fields.month_code);
            /* Obtain the maximum day count over the mandated reference
               interval; Chinese/Dangi are explicitly bounded to 30. */
            if (calendar == QJS_TEMPORAL_CAL_CHINESE || calendar == QJS_TEMPORAL_CAL_DANGI)
                maximum = 30;
            else {
                QJSTemporalISODate begin = {1900, 1, 1}, finish = {2035, 12, 31};
                qjs_temporal_iso_date_to_days(&low, begin);
                qjs_temporal_iso_date_to_days(&high, finish);
                for (days = low; days <= high;) {
                    qjs_temporal_iso_date_from_days(&candidate, days);
                    error = qjs_temporal_calendar_fields(calendar, candidate, &record);
                    if (error) return error;
                    if (!strcmp(record.month_code, code) && record.days_in_month > maximum)
                        maximum = record.days_in_month;
                    if (record.days_in_month < record.day) return QJS_TEMPORAL_ERROR_BACKEND;
                    days += record.days_in_month - record.day + 1;
                }
            }
            if (!maximum || !isfinite(fields.day) || fields.day < 1) return RANGE;
            if (fields.day > maximum && overflow == QJS_TEMPORAL_OVERFLOW_REJECT) return RANGE;
            desired_day = fields.day > maximum ? maximum : (int)fields.day;
        }
        if (calendar == QJS_TEMPORAL_CAL_CHINESE || calendar == QJS_TEMPORAL_CAL_DANGI) {
            static const int common_30[] = {1970,1972,1966,1970,1972,1971,1972,1971,1972,1972,1970,1972};
            static const int leap_29[] = {0,1947,1966,1963,1971,1960,1968,1957,2014,1984,2033,0};
            static const int leap_30[] = {0,0,1955,1944,1952,1941,1938,0,0,0,0,0};
            month_code_number(code, &number, &is_leap);
            reference_year = is_leap ? (desired_day == 30 ? leap_30[number - 1] : leap_29[number - 1]) :
                desired_day == 30 ? common_30[number - 1] : 1972;
            if (!is_leap && desired_day == 30 && number == 3 && calendar == QJS_TEMPORAL_CAL_DANGI)
                reference_year = 1968;
            if (is_leap && number == 11 && desired_day > 10 && desired_day < 30)
                reference_year = 2034;
            if (!reference_year) {
                if (overflow == QJS_TEMPORAL_OVERFLOW_REJECT) return RANGE;
                make_month_code(code, number, 0);
                reference_year = desired_day == 30 ? common_30[number - 1] : 1972;
                if (desired_day == 30 && number == 3 && calendar == QJS_TEMPORAL_CAL_DANGI)
                    reference_year = 1968;
            }
        }
        /* Latest matching date in 1900..1972, then earliest in 1973..2035.
           The Chinese/Dangi normative table restricts this to one year. */
        for (int pass = 0; pass < 2; pass++) {
            QJSTemporalISODate begin = {reference_year ? reference_year : pass ? 1973 : 1900, 1, 1};
            QJSTemporalISODate finish = {reference_year ? reference_year : pass ? 2035 : 1972, 12, 31};
            qjs_temporal_iso_date_to_days(&low, begin);
            qjs_temporal_iso_date_to_days(&high, finish);
            days = pass && !reference_year ? low : high;
            limit = pass && !reference_year ? high : low;
            for (;;) {
                qjs_temporal_iso_date_from_days(&candidate, days);
                error = qjs_temporal_calendar_fields(calendar, candidate, &record);
                if (error) return error;
                if (!strcmp(record.month_code, code) && desired_day <= record.days_in_month) {
                    int64_t match = days + desired_day - record.day;
                    if (match >= low && match <= high &&
                        !qjs_temporal_iso_date_from_days(&candidate, match)) {
                        *result = candidate; return 0;
                    }
                }
                if (record.day < 1 || record.days_in_month < record.day)
                    return QJS_TEMPORAL_ERROR_BACKEND;
                if (pass && !reference_year) {
                    days += record.days_in_month - record.day + 1;
                    if (days > limit) break;
                } else {
                    days -= record.day;
                    if (days < limit) break;
                }
            }
            if (reference_year) break;
        }
        return QJS_TEMPORAL_ERROR_BACKEND;
    }

}
static int add_checked(int64_t *result, int64_t a, int64_t b)
{
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return RANGE;
    *result = a + b; return 0;
}
static int calendar_year_month(QJSTemporalCalendar calendar,
                    QJSTemporalCalendarDate record, int64_t years, int64_t months,
                    QJSTemporalOverflow overflow, QJSTemporalISODate *first,
                    int *maximum_day)
{
    int64_t year, ordinal;
    int month;
    if (add_checked(&year, record.year, years) || year < -1000000 || year > 1000000)
        return RANGE;
    if (calendar == QJS_TEMPORAL_CAL_ISO8601) {
        if (add_checked(&ordinal, (year * 12 + record.month - 1), months)) return RANGE;
        year = ordinal / 12;
        month = (int)(ordinal % 12);
        if (month < 0) { month += 12; year--; }
        month++;
        if (year < INT32_MIN || year > INT32_MAX) return RANGE;
        *first = (QJSTemporalISODate){(int32_t)year, month, 1};
        *maximum_day = qjs_temporal_iso_days_in_month((int32_t)year, month);
        return 0;
    }
    {
        int32_t target_year;
        int target_month, count, error;
        char code[5];
        error = month_ordinal(calendar, (int32_t)year, record.month_code,
                              overflow, &month);
        if (error) return error;
        error = qjs_calendar_add_months((QJSCalendarId)calendar, (int32_t)year,
                                         month, months, &target_year, &target_month);
        if (error) return error;
        error = month_info(calendar, target_year, target_month, &count,
                            maximum_day, code);
        if (error) return error;
        return calendar_integers_to_iso(calendar, target_year, target_month, 1, first);
    }
}
int qjs_temporal_calendar_date_add(QJSTemporalCalendar calendar,
                    QJSTemporalISODate date, QJSTemporalDateDuration duration,
                    QJSTemporalOverflow overflow, QJSTemporalISODate *result)
{
    QJSTemporalCalendarDate record;
    QJSTemporalISODate first, target;
    int64_t days, extra;
    int maximum, day, error;
    if (calendar == QJS_TEMPORAL_CAL_ISO8601) {
        if (qjs_temporal_iso_date_add(&target, date, duration, overflow) ||
            !qjs_temporal_iso_date_within_limits(target)) return RANGE;
        *result = target; return 0;
    }
    error = qjs_temporal_calendar_fields(calendar, date, &record);
    if (error) return error;
    error = calendar_year_month(calendar, record, duration.years, duration.months,
                                overflow, &first, &maximum);
    if (error) return error;
    day = record.day;
    if (day > maximum) {
        if (overflow == QJS_TEMPORAL_OVERFLOW_REJECT) return RANGE;
        day = maximum;
    }
    if (duration.weeks < INT64_MIN / 7 || duration.weeks > INT64_MAX / 7 ||
        add_checked(&extra, duration.weeks * 7, duration.days) ||
        add_checked(&extra, extra, day - 1) ||
        qjs_temporal_iso_date_to_days(&days, first) || add_checked(&days, days, extra) ||
        qjs_temporal_iso_date_from_days(&target, days) || !qjs_temporal_iso_date_within_limits(target)) return RANGE;
    *result = target;
    return 0;
}
static int record_compare(QJSTemporalCalendarDate a, QJSTemporalCalendarDate b)
{
    if (a.year != b.year) return a.year < b.year ? -1 : 1;
    if (a.month != b.month) return a.month < b.month ? -1 : 1;
    return a.day < b.day ? -1 : a.day > b.day;
}
static int surpasses(QJSTemporalCalendar calendar, int sign,
                     QJSTemporalCalendarDate from, QJSTemporalCalendarDate to,
                     int64_t years, int64_t months, int *result)
{
    QJSTemporalISODate first;
    QJSTemporalCalendarDate current;
    int64_t year;
    int maximum, error;
    if (add_checked(&year, from.year, years) || year < -1000000 || year > 1000000) {
        *result = 1; return 0;
    }
    /* Compare the original month code before constraining a missing leap
       month in the target year, as NonISODateSurpasses requires. */
    if ((year > to.year && sign > 0) || (year < to.year && sign < 0)) {
        *result = 1; return 0;
    }
    if (year == to.year) {
        int comparison = strcmp(from.month_code, to.month_code);
        if ((comparison > 0 && sign > 0) || (comparison < 0 && sign < 0) ||
            (!comparison && (from.day - to.day) * sign > 0)) {
            *result = 1; return 0;
        }
    }
    error = calendar_year_month(calendar, from, years, months,
                                QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &first, &maximum);
    if (error) { if (error == RANGE) { *result = 1; return 0; } return error; }
    error = qjs_temporal_calendar_fields(calendar, first, &current);
    if (error) return error;
    current.day = from.day;
    *result = record_compare(current, to) * sign > 0;
    return 0;
}
static int count_units(QJSTemporalCalendar calendar, int sign,
                       QJSTemporalCalendarDate from, QJSTemporalCalendarDate to,
                       int years_unit, int64_t years, int64_t upper, int64_t *result)
{
    int64_t lo = 0, hi = upper, mid;
    int error, surpassed;
    while (lo < hi) {
        mid = lo + (hi - lo + 1) / 2;
        error = surpasses(calendar, sign, from, to,
                           years_unit ? mid * sign : years,
                           years_unit ? 0 : mid * sign, &surpassed);
        if (error) return error;
        if (surpassed) hi = mid - 1;
        else lo = mid;
    }
    *result = lo * sign;
    return 0;
}
int qjs_temporal_calendar_date_until(QJSTemporalCalendar calendar,
                    QJSTemporalISODate start, QJSTemporalISODate end,
                    QJSTemporalUnit largest, QJSTemporalDateDuration *result)
{
    QJSTemporalDateDuration duration = {0};
    QJSTemporalCalendarDate from, to;
    QJSTemporalISODate anchor;
    int64_t start_days, end_days, anchor_days, magnitude;
    int sign, error;
    if (calendar == QJS_TEMPORAL_CAL_ISO8601)
        return qjs_temporal_iso_date_until(result, start, end, largest);
    if (largest > QJS_TEMPORAL_DAY || qjs_temporal_iso_date_to_days(&start_days, start) ||
        qjs_temporal_iso_date_to_days(&end_days, end)) return RANGE;
    sign = end_days > start_days ? 1 : end_days < start_days ? -1 : 0;
    if (!sign) { *result = duration; return 0; }
    if (largest == QJS_TEMPORAL_DAY || largest == QJS_TEMPORAL_WEEK) {
        duration.days = end_days - start_days;
        if (largest == QJS_TEMPORAL_WEEK) {
            duration.weeks = duration.days / 7; duration.days %= 7;
        }
        *result = duration; return 0;
    }
    error = qjs_temporal_calendar_fields(calendar, start, &from);
    if (error) return error;
    error = qjs_temporal_calendar_fields(calendar, end, &to);
    if (error) return error;
    magnitude = (int64_t)to.year - from.year;
    if (magnitude < 0) magnitude = -magnitude;
    if (largest == QJS_TEMPORAL_YEAR) {
        error = count_units(calendar, sign, from, to, 1, 0, magnitude + 1, &duration.years);
        if (error) return error;
    }
    /* Every supported calendar month has at least five days (the Coptic
       intercalary month); this is a safe bound without linear scanning. */
    magnitude = (end_days - start_days) * sign / 5 + 14;
    error = count_units(calendar, sign, from, to, 0, duration.years, magnitude, &duration.months);
    if (error) return error;
    error = qjs_temporal_calendar_date_add(calendar, start, duration,
                                          QJS_TEMPORAL_OVERFLOW_CONSTRAIN, &anchor);
    if (error) return error;
    qjs_temporal_iso_date_to_days(&anchor_days, anchor);
    duration.days = end_days - anchor_days;
    *result = duration;
    return 0;
}
