/* ISO Temporal calendar resolution and arithmetic.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include "calendar.h"
#include "civil.h"
#include <limits.h>
#include <math.h>
#include <string.h>

#define HAS(f, name) ((f)->present & QJS_TEMPORAL_FIELD_##name)
#define RANGE QJS_TEMPORAL_ERROR_RANGE
#define MISSING QJS_TEMPORAL_ERROR_MISSING

const char *qjs_temporal_calendar_identifier(QJSCalendarId calendar)
{
    return calendar == QJS_CAL_ISO8601 ? "iso8601" : NULL;
}

int qjs_temporal_calendar_from_identifier(QJSCalendarId *result,
                                          const char *text, size_t length)
{
    static const char identifier[] = "iso8601";
    size_t i;

    if (length != sizeof(identifier) - 1)
        return RANGE;
    for (i = 0; i < length; i++) {
        unsigned char c = text[i];
        if (c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
        if (c != (unsigned char)identifier[i])
            return RANGE;
    }
    *result = QJS_CAL_ISO8601;
    return 0;
}

unsigned qjs_temporal_calendar_extra_fields(QJSCalendarId calendar,
                                           unsigned requested)
{
    return 0;
}

unsigned qjs_temporal_calendar_fields_to_ignore(QJSCalendarId calendar,
                                               unsigned additional)
{
    if (additional & (QJS_TEMPORAL_FIELD_MONTH | QJS_TEMPORAL_FIELD_MONTH_CODE))
        additional |= QJS_TEMPORAL_FIELD_MONTH | QJS_TEMPORAL_FIELD_MONTH_CODE;
    return additional;
}

static int resolve(const QJSTemporalCalendarFields *fields, int type,
                   QJSTemporalOverflow overflow, QJSTemporalISODate *result)
{
    QJSTemporalISODate date;
    double month, day;
    int maximum;

    if (overflow != QJS_TEMPORAL_OVERFLOW_CONSTRAIN &&
        overflow != QJS_TEMPORAL_OVERFLOW_REJECT)
        return RANGE;
    if ((type != 2 && !HAS(fields, YEAR)) ||
        (type != 1 && !HAS(fields, DAY)) ||
        (!HAS(fields, MONTH) && !HAS(fields, MONTH_CODE)))
        return MISSING;
    if (HAS(fields, YEAR) && (!isfinite(fields->year) ||
                             trunc(fields->year) != fields->year))
        return RANGE;
    if (HAS(fields, MONTH_CODE)) {
        const char *code = fields->month_code;
        if (code[0] != 'M' || code[1] < '0' || code[1] > '9' ||
            code[2] < '0' || code[2] > '9' || code[3])
            return RANGE;
        month = (code[1] - '0') * 10 + code[2] - '0';
        if (month < 1 || month > 12 ||
            (HAS(fields, MONTH) && fields->month != month))
            return RANGE;
    } else {
        month = fields->month;
        if (!isfinite(month) || month < 1 || trunc(month) != month)
            return RANGE;
        if (month > 12) {
            if (overflow == QJS_TEMPORAL_OVERFLOW_REJECT)
                return RANGE;
            month = 12;
        }
    }
    date.month = (int32_t)month;
    if (type == 2) {
        /* A supplied MonthDay year regulates February but does not bound
           its leap reference year. Integral Numbers may exceed int32_t. */
        int leap = !HAS(fields, YEAR) || (fmod(fields->year, 4) == 0 &&
            (fmod(fields->year, 100) != 0 || fmod(fields->year, 400) == 0));
        date.year = 1972;
        maximum = qjs_iso_days_in_month(date.year, date.month) -
                  (date.month == 2 && !leap);
    } else {
        if (fields->year < INT32_MIN || fields->year > INT32_MAX)
            return RANGE;
        date.year = (int32_t)fields->year;
        maximum = qjs_iso_days_in_month(date.year, date.month);
    }
    day = type == 1 ? 1 : fields->day;
    if (!isfinite(day) || day < 1 || trunc(day) != day)
        return RANGE;
    if (day > maximum) {
        if (overflow == QJS_TEMPORAL_OVERFLOW_REJECT)
            return RANGE;
        day = maximum;
    }
    date.day = (int32_t)day;
    *result = date;
    return 0;
}

int qjs_temporal_calendar_date_from_fields(QJSCalendarId calendar,
                                          const QJSTemporalCalendarFields *fields,
                                          QJSTemporalOverflow overflow,
                                          QJSTemporalISODate *result)
{
    QJSTemporalISODate date;
    int error;

    if (calendar != QJS_CAL_ISO8601)
        return QJS_TEMPORAL_ERROR_UNSUPPORTED;
    error = resolve(fields, 0, overflow, &date);
    if (error)
        return error;
    if (!qjs_temporal_iso_date_within_limits(date))
        return RANGE;
    *result = date;
    return 0;
}

int qjs_temporal_calendar_year_month_from_fields(
    QJSCalendarId calendar, const QJSTemporalCalendarFields *fields,
    QJSTemporalOverflow overflow, QJSTemporalISODate *result)
{
    if (calendar != QJS_CAL_ISO8601)
        return QJS_TEMPORAL_ERROR_UNSUPPORTED;
    return resolve(fields, 1, overflow, result);
}

int qjs_temporal_calendar_month_day_from_fields(
    QJSCalendarId calendar, const QJSTemporalCalendarFields *fields,
    QJSTemporalOverflow overflow, QJSTemporalISODate *result)
{
    if (calendar != QJS_CAL_ISO8601)
        return QJS_TEMPORAL_ERROR_UNSUPPORTED;
    return resolve(fields, 2, overflow, result);
}

int qjs_temporal_calendar_fields(QJSCalendarId calendar,
                                 QJSTemporalISODate date,
                                 QJSTemporalCalendarDate *result)
{
    QJSTemporalCalendarDate fields = { 0 };

    if (calendar != QJS_CAL_ISO8601)
        return QJS_TEMPORAL_ERROR_UNSUPPORTED;
    if (!qjs_iso_date_is_valid(date))
        return RANGE;
    fields.year = date.year;
    fields.month = date.month;
    fields.day = date.day;
    fields.month_code[0] = 'M';
    fields.month_code[1] = '0' + date.month / 10;
    fields.month_code[2] = '0' + date.month % 10;
    fields.day_of_week = qjs_temporal_iso_day_of_week(date);
    fields.day_of_year = qjs_temporal_iso_day_of_year(date);
    fields.days_in_week = 7;
    fields.days_in_month = qjs_iso_days_in_month(date.year, date.month);
    fields.in_leap_year = qjs_iso_leap_year(date.year);
    fields.days_in_year = 365 + fields.in_leap_year;
    fields.months_in_year = 12;
    fields.week_of_year = qjs_temporal_iso_week_of_year(date, &fields.year_of_week);
    fields.has_week = 1;
    *result = fields;
    return 0;
}

int qjs_temporal_calendar_date_add(QJSCalendarId calendar,
                                   QJSTemporalISODate date,
                                   QJSTemporalDateDuration duration,
                                   QJSTemporalOverflow overflow,
                                   QJSTemporalISODate *result)
{
    QJSTemporalISODate target;

    if (calendar != QJS_CAL_ISO8601)
        return QJS_TEMPORAL_ERROR_UNSUPPORTED;
    if (qjs_temporal_iso_date_add(&target, date, duration, overflow) ||
        !qjs_temporal_iso_date_within_limits(target))
        return RANGE;
    *result = target;
    return 0;
}

int qjs_temporal_calendar_date_until(QJSCalendarId calendar,
                                     QJSTemporalISODate start,
                                     QJSTemporalISODate end,
                                     QJSTemporalUnit largest,
                                     QJSTemporalDateDuration *result)
{
    if (calendar != QJS_CAL_ISO8601)
        return QJS_TEMPORAL_ERROR_UNSUPPORTED;
    return qjs_temporal_iso_date_until(result, start, end, largest);
}
