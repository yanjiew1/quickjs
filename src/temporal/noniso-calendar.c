/* Non-ISO Temporal field resolution over the independent calendar facade.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include "noniso-calendar.h"
#include "civil.h"
#include "../calendar/calendar.h"
#include <limits.h>
#include <math.h>
#include <string.h>

#define HAS(f, name) ((f)->present & QJS_TEMPORAL_FIELD_##name)
#define RANGE QJS_TEMPORAL_ERROR_RANGE
#define MISSING QJS_TEMPORAL_ERROR_MISSING

int qjs_temporal_noniso_has_era(QJSCalendarId calendar)
{
    return calendar > QJS_CAL_ISO8601 && calendar < QJS_CAL_COUNT &&
           calendar != QJS_CAL_CHINESE && calendar != QJS_CAL_DANGI;
}

static int checked_i32(double value, int32_t *result)
{
    if (!isfinite(value) || trunc(value) != value ||
        value < INT32_MIN || value > INT32_MAX)
        return RANGE;
    *result = (int32_t)value;
    return 0;
}

static int parse_month_code(const char *code, int *nominal, int *leap)
{
    size_t length = strlen(code);

    if ((length != 3 && length != 4) || code[0] != 'M' ||
        code[1] < '0' || code[1] > '9' || code[2] < '0' || code[2] > '9' ||
        (length == 4 && code[3] != 'L'))
        return RANGE;
    *nominal = (code[1] - '0') * 10 + code[2] - '0';
    *leap = length == 4;
    return 0;
}

static int valid_month_code(QJSCalendarId calendar, const char *code)
{
    int nominal, leap;

    if (parse_month_code(code, &nominal, &leap) || nominal < 1)
        return 0;
    if (!leap)
        return nominal <= 12 || (nominal == 13 &&
               (calendar == QJS_CAL_COPTIC || calendar == QJS_CAL_ETHIOAA ||
                calendar == QJS_CAL_ETHIOPIC));
    return (nominal <= 12 && (calendar == QJS_CAL_CHINESE || calendar == QJS_CAL_DANGI)) ||
           (nominal == 5 && calendar == QJS_CAL_HEBREW);
}

static int resolve_era(QJSCalendarId calendar, QJSTemporalCalendarFields *fields)
{
    int32_t era_year, year;
    int error;

    if (!qjs_temporal_noniso_has_era(calendar))
        return 0;
    if (!!HAS(fields, ERA) != !!HAS(fields, ERA_YEAR))
        return MISSING;
    if (!HAS(fields, ERA_YEAR))
        return 0;
    if (checked_i32(fields->era_year, &era_year))
        return RANGE;
    error = qjs_calendar_year_from_era(calendar, fields->era, era_year, &year);
    if (error)
        return error;
    if (HAS(fields, YEAR) && fields->year != year)
        return RANGE;
    fields->year = year;
    fields->present |= QJS_TEMPORAL_FIELD_YEAR;
    fields->present &= ~(QJS_TEMPORAL_FIELD_ERA | QJS_TEMPORAL_FIELD_ERA_YEAR);
    return 0;
}

static int integers_to_iso(QJSCalendarId calendar, int32_t year, int month,
                            int day, QJSTemporalISODate *result)
{
    int64_t epoch;
    int error = qjs_calendar_to_epoch_day(calendar, year, month, day, &epoch);

    if (error)
        return error;
    return qjs_iso_date_from_days(result, epoch) ? RANGE : 0;
}

int qjs_temporal_noniso_resolve(QJSCalendarId calendar,
                                const QJSTemporalCalendarFields *input, int type,
                                QJSTemporalOverflow overflow,
                                QJSTemporalISODate *result)
{
    QJSTemporalCalendarFields fields = *input;
    int32_t year;
    int month, count, maximum, error;
    double numeric;
    char code[5];

    if (!qjs_calendar_is_supported(calendar))
        return QJS_TEMPORAL_ERROR_UNSUPPORTED;
    if ((overflow != QJS_TEMPORAL_OVERFLOW_CONSTRAIN &&
         overflow != QJS_TEMPORAL_OVERFLOW_REJECT) || (type != 0 && type != 1))
        return RANGE;
    if (!HAS(&fields, YEAR) && (!qjs_temporal_noniso_has_era(calendar) ||
        !HAS(&fields, ERA) || !HAS(&fields, ERA_YEAR)))
        return MISSING;
    if ((type == 0 && !HAS(&fields, DAY)) ||
        (!HAS(&fields, MONTH) && !HAS(&fields, MONTH_CODE)))
        return MISSING;
    error = resolve_era(calendar, &fields);
    if (error)
        return error;
    if (checked_i32(fields.year, &year) ||
        year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR)
        return RANGE;
    if (HAS(&fields, MONTH_CODE)) {
        if (!valid_month_code(calendar, fields.month_code))
            return RANGE;
        /* Check consistency with the constrained ordinal before rejecting
           a missing leap month, as NonISOResolveFields requires. */
        error = qjs_calendar_month_ordinal(calendar, year, fields.month_code, 1, &month);
        if (error)
            return error;
        if (HAS(&fields, MONTH) && fields.month != month)
            return RANGE;
        if (overflow == QJS_TEMPORAL_OVERFLOW_REJECT) {
            error = qjs_calendar_month_ordinal(calendar, year, fields.month_code, 0, &month);
            if (error)
                return error;
        }
    } else {
        error = qjs_calendar_month_info(calendar, year, 1, &count, &maximum, code);
        if (error)
            return error;
        numeric = fields.month;
        if (!isfinite(numeric) || numeric < 1 || trunc(numeric) != numeric)
            return RANGE;
        if (numeric > count) {
            if (overflow == QJS_TEMPORAL_OVERFLOW_REJECT)
                return RANGE;
            numeric = count;
        }
        month = (int)numeric;
    }
    error = qjs_calendar_month_info(calendar, year, month, &count, &maximum, code);
    if (error)
        return error;
    numeric = type == 1 ? 1 : fields.day;
    if (!isfinite(numeric) || numeric < 1 || trunc(numeric) != numeric)
        return RANGE;
    if (numeric > maximum) {
        if (overflow == QJS_TEMPORAL_OVERFLOW_REJECT)
            return RANGE;
        numeric = maximum;
    }
    return integers_to_iso(calendar, year, month, (int)numeric, result);
}

int qjs_temporal_noniso_fields(QJSCalendarId calendar, QJSTemporalISODate date,
                               QJSTemporalCalendarDate *result)
{
    QJSTemporalCalendarDate fields = {0};
    QJSCalendarDate native;
    int64_t epoch;
    int error;

    if (qjs_iso_date_to_days(&epoch, date))
        return RANGE;
    error = qjs_calendar_from_epoch_day(calendar, epoch, &native);
    if (error)
        return error;
    fields.year = native.year;
    fields.month = native.month;
    fields.day = native.day;
    fields.era_year = native.era_year;
    memcpy(fields.month_code, native.month_code, sizeof(fields.month_code));
    memcpy(fields.era, native.era, sizeof(fields.era));
    fields.day_of_week = qjs_temporal_iso_day_of_week(date);
    fields.days_in_week = 7;
    fields.day_of_year = native.day_of_year;
    fields.days_in_month = native.days_in_month;
    fields.days_in_year = native.days_in_year;
    fields.months_in_year = native.months_in_year;
    fields.has_era = native.has_era;
    fields.in_leap_year = native.in_leap_year;
    /* Non-ISO calendars have no locale-independent week numbering. */
    *result = fields;
    return 0;
}

static int maximum_month_days(QJSCalendarId calendar, int nominal, int leap)
{
    static const unsigned char hebrew[12] =
        {30, 30, 30, 29, 30, 29, 30, 29, 30, 29, 30, 29};

    switch (calendar) {
    case QJS_CAL_CHINESE: case QJS_CAL_DANGI: case QJS_CAL_ISLAMIC_UMALQURA:
        return 30;
    case QJS_CAL_COPTIC: case QJS_CAL_ETHIOAA: case QJS_CAL_ETHIOPIC:
        return nominal == 13 ? 6 : 30;
    case QJS_CAL_HEBREW:
        return leap ? 30 : hebrew[nominal - 1];
    case QJS_CAL_ISLAMIC_CIVIL: case QJS_CAL_ISLAMIC_TBLA:
        return nominal == 12 || (nominal & 1) ? 30 : 29;
    case QJS_CAL_INDIAN: case QJS_CAL_PERSIAN:
        return nominal <= 6 ? 31 : 30;
    default:
        return qjs_iso_days_in_month(1972, nominal);
    }
}

/* ECMA-402's ISO Reference Years table, not an astronomical search policy. */
static int reference_year(QJSCalendarId calendar, int nominal, int leap, int day)
{
    static const int common_30[12] =
        {1970, 1972, 1966, 1970, 1972, 1971, 1972, 1971, 1972, 1972, 1970, 1972};
    static const int leap_29[12] =
        {0, 1947, 1966, 1963, 1971, 1960, 1968, 1957, 2014, 1984, 2033, 0};
    static const int leap_30[12] =
        {0, 0, 1955, 1944, 1952, 1941, 1938, 0, 0, 0, 0, 0};
    static const int umm_30[12] =
        {1972, 1970, 1971, 1972, 1971, 1972, 1969, 1972, 1972, 1970, 1972, 1971};

    if (calendar == QJS_CAL_CHINESE || calendar == QJS_CAL_DANGI) {
        if (leap) {
            if (nominal == 11 && day > 10 && day < 30)
                return 2034;
            return day == 30 ? leap_30[nominal - 1] : leap_29[nominal - 1];
        }
        if (day == 30) {
            if (calendar == QJS_CAL_DANGI && nominal == 3)
                return 1968;
            return common_30[nominal - 1];
        }
    } else if (calendar == QJS_CAL_HEBREW) {
        if (leap)
            return 1970;
        if (day == 30 && (nominal == 2 || nominal == 3))
            return 1971;
    } else if (calendar == QJS_CAL_ISLAMIC_UMALQURA) {
        if (day == 30)
            return umm_30[nominal - 1];
    } else if (calendar == QJS_CAL_ISLAMIC_CIVIL || calendar == QJS_CAL_ISLAMIC_TBLA) {
        if (nominal == 12 && day == 30)
            return 1971;
    } else if (calendar == QJS_CAL_COPTIC || calendar == QJS_CAL_ETHIOAA ||
               calendar == QJS_CAL_ETHIOPIC) {
        if (nominal == 13 && day == 6)
            return 1971;
    }
    return 1972;
}

static int year_intersects_limits(QJSCalendarId calendar, int32_t year)
{
    int64_t first, next;
    int error;

    /* Reject impossible lunar years before constructing astronomical metadata.
       M01 falls in the ISO year named by the arithmetic year. */
    if ((calendar == QJS_CAL_CHINESE || calendar == QJS_CAL_DANGI) &&
        (year < -271821 || year > 275760))
        return RANGE;
    error = qjs_calendar_to_epoch_day(calendar, year, 1, 1, &first);
    if (error)
        return error;
    if (first > INT64_C(100000000) || year == QJS_CAL_MAX_YEAR)
        return RANGE;
    error = qjs_calendar_to_epoch_day(calendar, year + 1, 1, 1, &next);
    if (error)
        return error;
    return next <= -INT64_C(100000001) ? RANGE : 0;
}

int qjs_temporal_noniso_month_day(QJSCalendarId calendar,
                                  const QJSTemporalCalendarFields *input,
                                  QJSTemporalOverflow overflow,
                                  QJSTemporalISODate *result)
{
    QJSTemporalCalendarFields fields = *input;
    QJSTemporalCalendarDate record;
    QJSTemporalISODate source, candidate;
    int32_t year;
    int desired_day, maximum, nominal, leap, ref, error, count;
    int64_t low, days;
    double numeric;
    char code[5];

    if (!qjs_calendar_is_supported(calendar))
        return QJS_TEMPORAL_ERROR_UNSUPPORTED;
    if (overflow != QJS_TEMPORAL_OVERFLOW_CONSTRAIN &&
        overflow != QJS_TEMPORAL_OVERFLOW_REJECT)
        return RANGE;
    if (!HAS(&fields, DAY) || (!HAS(&fields, MONTH) && !HAS(&fields, MONTH_CODE)))
        return MISSING;
    error = resolve_era(calendar, &fields);
    if (error)
        return error;
    if (HAS(&fields, MONTH) && !HAS(&fields, YEAR))
        return MISSING;
    if (HAS(&fields, MONTH_CODE) && !valid_month_code(calendar, fields.month_code))
        return RANGE;
    if (HAS(&fields, YEAR)) {
        if (checked_i32(fields.year, &year))
            return RANGE;
        error = year_intersects_limits(calendar, year);
        if (error)
            return error;
        error = qjs_temporal_noniso_resolve(calendar, &fields, 0, overflow, &source);
        if (error)
            return error;
        /* The coarse year guard permits early rejection, but a partial edge
           year still requires the actual constrained month/day to be valid
           before selecting its unrelated reference year. */
        if (!qjs_temporal_iso_date_within_limits(source))
            return RANGE;
        error = qjs_temporal_noniso_fields(calendar, source, &record);
        if (error)
            return error;
        memcpy(code, record.month_code, sizeof(code));
        desired_day = record.day;
    } else {
        if (HAS(&fields, MONTH) || !HAS(&fields, MONTH_CODE))
            return MISSING;
        memcpy(code, fields.month_code, sizeof(code));
        error = parse_month_code(code, &nominal, &leap);
        if (error)
            return error;
        maximum = maximum_month_days(calendar, nominal, leap);
        numeric = fields.day;
        if (!isfinite(numeric) || numeric < 1 || trunc(numeric) != numeric)
            return RANGE;
        if (numeric > maximum) {
            if (overflow == QJS_TEMPORAL_OVERFLOW_REJECT)
                return RANGE;
            numeric = maximum;
        }
        desired_day = (int)numeric;
    }
    error = parse_month_code(code, &nominal, &leap);
    if (error)
        return error;
    ref = reference_year(calendar, nominal, leap, desired_day);
    if (!ref) {
        if (overflow == QJS_TEMPORAL_OVERFLOW_REJECT)
            return RANGE;
        /* Unknown historical lunar combinations constrain to the common
           month, retaining the regulated day, as the normative table says. */
        code[3] = 0;
        ref = reference_year(calendar, nominal, 0, desired_day);
    }
    if (qjs_iso_date_to_days(&low, (QJSTemporalISODate){ref, 1, 1}) ||
        qjs_iso_date_to_days(&days, (QJSTemporalISODate){ref, 12, 31}))
        return RANGE;
    /* Select the latest occurrence in the one prescribed ISO year. At most
       14 calendar-month portions intersect it; keep an explicit safe bound. */
    for (count = 0; days >= low && count < 16; count++) {
        int64_t match;

        if (qjs_iso_date_from_days(&candidate, days))
            return RANGE;
        error = qjs_temporal_noniso_fields(calendar, candidate, &record);
        if (error)
            return error;
        if (!strcmp(record.month_code, code) && desired_day <= record.days_in_month) {
            match = days + desired_day - record.day;
            if (match >= low && match <= days &&
                !qjs_iso_date_from_days(&candidate, match)) {
                *result = candidate;
                return 0;
            }
        }
        days -= record.day;
    }
    return QJS_TEMPORAL_ERROR_BACKEND;
}
