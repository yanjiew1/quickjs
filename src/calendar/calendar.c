/* Common calendar identities and mandatory ISO behavior.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include "calendar-backend.h"
#include "arithmetic-internal.h"
#include "iso.h"
#include <limits.h>
#include <string.h>

static const char *const identifiers[QJS_CAL_COUNT] = {
    "iso8601", "buddhist", "chinese", "coptic", "dangi", "ethioaa", "ethiopic",
    "gregory", "hebrew", "indian", "islamic-civil", "islamic-tbla",
    "islamic-umalqura", "japanese", "persian", "roc",
};

const char *qjs_calendar_identifier(QJSCalendarId calendar)
{
    return (unsigned)calendar < QJS_CAL_COUNT ? identifiers[calendar] : NULL;
}

int qjs_calendar_is_supported(QJSCalendarId calendar)
{
    return (unsigned)calendar < QJS_CAL_COUNT &&
           (calendar == QJS_CAL_ISO8601 || qjs_calendar_backend_is_supported(calendar));
}

static int identifier_equal(const char *text, size_t length, const char *name)
{
    size_t i;

    if (strlen(name) != length)
        return 0;
    for (i = 0; i < length; i++) {
        unsigned char c = text[i];

        if (c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
        if (c != (unsigned char)name[i])
            return 0;
    }
    return 1;
}

int qjs_calendar_from_identifier(QJSCalendarId *result, const char *text,
                                 size_t length)
{
    QJSCalendarId calendar;
    int i;

    if (identifier_equal(text, length, "ethiopic-amete-alem"))
        calendar = QJS_CAL_ETHIOAA;
    else if (identifier_equal(text, length, "islamicc"))
        calendar = QJS_CAL_ISLAMIC_CIVIL;
    else {
        for (i = 0; i < QJS_CAL_COUNT; i++) {
            if (identifier_equal(text, length, identifiers[i]))
                break;
        }
        if (i == QJS_CAL_COUNT)
            return QJS_CAL_RANGE;
        calendar = (QJSCalendarId)i;
    }
    if (!qjs_calendar_is_supported(calendar))
        return QJS_CAL_UNSUPPORTED;
    *result = calendar;
    return QJS_CAL_OK;
}

static void iso_month_code(char code[5], int month)
{
    code[0] = 'M';
    code[1] = '0' + month / 10;
    code[2] = '0' + month % 10;
    code[3] = code[4] = 0;
}

int qjs_calendar_month_info(QJSCalendarId calendar, int32_t year, int month,
                            int *months, int *days, char month_code[5])
{
    if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR || month < 1)
        return QJS_CAL_RANGE;
    if (!qjs_calendar_is_supported(calendar))
        return QJS_CAL_UNSUPPORTED;
    if (calendar != QJS_CAL_ISO8601)
        return qjs_calendar_backend_month_info(calendar, year, month, months,
                                               days, month_code);
    if (month > 12)
        return QJS_CAL_RANGE;
    *months = 12;
    *days = qjs_iso_days_in_month(year, month);
    iso_month_code(month_code, month);
    return QJS_CAL_OK;
}

int qjs_calendar_to_epoch_day(QJSCalendarId calendar, int32_t year,
                              int month, int day, int64_t *result)
{
    if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR)
        return QJS_CAL_RANGE;
    if (!qjs_calendar_is_supported(calendar))
        return QJS_CAL_UNSUPPORTED;
    if (calendar != QJS_CAL_ISO8601)
        return qjs_calendar_backend_to_epoch_day(calendar, year, month, day, result);
    return qjs_iso_date_to_days(result, (QJSISODate){year, month, day}) ?
           QJS_CAL_RANGE : QJS_CAL_OK;
}

int qjs_calendar_from_epoch_day(QJSCalendarId calendar, int64_t epoch_day,
                                QJSCalendarDate *result)
{
    QJSISODate iso;
    QJSCalendarDate date = {0};
    int64_t first;

    if (!qjs_calendar_is_supported(calendar))
        return QJS_CAL_UNSUPPORTED;
    if (calendar != QJS_CAL_ISO8601)
        return qjs_calendar_backend_from_epoch_day(calendar, epoch_day, result);
    if (qjs_iso_date_from_days(&iso, epoch_day) || iso.year < QJS_CAL_MIN_YEAR ||
        iso.year > QJS_CAL_MAX_YEAR)
        return QJS_CAL_RANGE;
    if (qjs_iso_date_to_days(&first, (QJSISODate){iso.year, 1, 1}))
        return QJS_CAL_RANGE;
    date.year = iso.year;
    date.month = iso.month;
    date.day = iso.day;
    date.day_of_year = epoch_day - first + 1;
    date.days_in_month = qjs_iso_days_in_month(iso.year, iso.month);
    date.in_leap_year = qjs_iso_leap_year(iso.year);
    date.days_in_year = 365 + date.in_leap_year;
    date.months_in_year = 12;
    iso_month_code(date.month_code, iso.month);
    *result = date;
    return QJS_CAL_OK;
}

int qjs_calendar_year_from_era(QJSCalendarId calendar, const char *era,
                               int32_t era_year, int32_t *result)
{
    if (!qjs_calendar_is_supported(calendar))
        return QJS_CAL_UNSUPPORTED;
    if (calendar == QJS_CAL_ISO8601)
        return QJS_CAL_RANGE;
    return qjs_calendar_backend_year_from_era(calendar, era, era_year, result);
}

int qjs_calendar_month_ordinal(QJSCalendarId calendar, int32_t year,
                               const char *month_code, int constrain, int *result)
{
    int nominal;
    size_t length = strlen(month_code);

    if ((constrain != 0 && constrain != 1) || (length != 3 && length != 4) ||
        month_code[0] != 'M' || month_code[1] < '0' || month_code[1] > '9' ||
        month_code[2] < '0' || month_code[2] > '9' ||
        (length == 4 && month_code[3] != 'L'))
        return QJS_CAL_RANGE;
    if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR)
        return QJS_CAL_RANGE;
    nominal = (month_code[1] - '0') * 10 + month_code[2] - '0';
    if (nominal < 1 || nominal > (length == 4 ? 12 : 13))
        return QJS_CAL_RANGE;
    if (!qjs_calendar_is_supported(calendar))
        return QJS_CAL_UNSUPPORTED;
    if (calendar != QJS_CAL_ISO8601)
        return qjs_calendar_backend_month_ordinal(calendar, year, nominal,
                                                  length == 4, constrain, result);
    if (length == 4 || nominal < 1 || nominal > 12)
        return QJS_CAL_RANGE;
    *result = nominal;
    return QJS_CAL_OK;
}

int qjs_calendar_add_months(QJSCalendarId calendar, int32_t year, int month,
                            int64_t delta, int32_t *result_year, int *result_month)
{
    int64_t index, target, new_year;

    if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR || month < 1)
        return QJS_CAL_RANGE;
    if (!qjs_calendar_is_supported(calendar))
        return QJS_CAL_UNSUPPORTED;
    if (calendar != QJS_CAL_ISO8601)
        return qjs_calendar_backend_add_months(calendar, year, month, delta,
                                               result_year, result_month);
    if (month > 12)
        return QJS_CAL_RANGE;
    index = (int64_t)year * 12 + month - 1;
    if ((delta > 0 && index > INT64_MAX - delta) ||
        (delta < 0 && index < INT64_MIN - delta))
        return QJS_CAL_RANGE;
    target = index + delta;
    new_year = qjs_cal_floor_divide(target, 12);
    if (new_year < QJS_CAL_MIN_YEAR || new_year > QJS_CAL_MAX_YEAR)
        return QJS_CAL_RANGE;
    *result_year = new_year;
    *result_month = qjs_cal_floor_modulo(target, 12) + 1;
    return QJS_CAL_OK;
}
