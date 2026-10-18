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
