/* Native non-ISO conversions over checked arithmetic and publication data.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include "native-internal.h"
#include "arithmetic.h"
#include "data.h"
#include "eras.h"
#include "iso.h"
#include "lunisolar.h"
#include <string.h>

int qjs_cal_native_lunar(QJSCalendarId calendar)
{
    return calendar == QJS_CAL_CHINESE || calendar == QJS_CAL_DANGI;
}

static int islamic(QJSCalendarId calendar)
{
    return calendar >= QJS_CAL_ISLAMIC_CIVIL &&
           calendar <= QJS_CAL_ISLAMIC_UMALQURA;
}

int qjs_calendar_backend_is_supported(QJSCalendarId calendar)
{
    return calendar > QJS_CAL_ISO8601 && calendar < QJS_CAL_COUNT;
}

int qjs_calendar_backend_era_at(QJSCalendarId calendar, size_t index, const char **out)
{
    return qjs_cal_era_at(calendar, index, out);
}

static int year_start(QJSCalendarId calendar, int32_t year, int64_t *result)
{
    if (calendar == QJS_CAL_HEBREW)
        return qjs_cal_hebrew_year_start(year, result);
    if (calendar == QJS_CAL_PERSIAN)
        return qjs_cal_persian_year_start(year, result);
    if (islamic(calendar))
        return qjs_cal_islamic_year_start(calendar, year, result);
    return qjs_cal_solar_year_start(calendar, year, result);
}

int qjs_cal_native_year(QJSCalendarId calendar, int32_t year,
                        QJSNativeCalendarYear *result)
{
    QJSNativeCalendarYear value = {0};
    int error, month, sum = 0;

    if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR)
        return QJS_CAL_RANGE;
    if (!qjs_calendar_backend_is_supported(calendar))
        return QJS_CAL_UNSUPPORTED;
    if (qjs_cal_native_lunar(calendar)) {
        QJSCalendarYear packed;

        error = qjs_calendar_data_year(&qjs_native_calendar_data, calendar,
                                       year, &packed);
        if (error) {
            /* Never replace a missing required publication year with a
               whole-year approximation, including partial boundary years. */
            if (error != QJS_CAL_RANGE ||
                (year >= 1899 && year <= (calendar == QJS_CAL_CHINESE ? 2100 : 2050)))
                return error == QJS_CAL_RANGE ? QJS_CAL_BACKEND : error;
            error = qjs_cal_lunisolar_approximate_year(calendar, year, &packed);
            if (error)
                return error;
        }
        value.first_day = packed.first_day;
        value.end_day = packed.end_day;
        value.months = packed.months;
        value.leap_month = packed.leap_month;
        for (month = 0; month < value.months; month++)
            value.lengths[month] = 29 + ((packed.month_lengths >> (2 * month)) & 3);
    } else {
        error = year_start(calendar, year, &value.first_day);
        if (error)
            return error;
        error = year_start(calendar, year + 1, &value.end_day);
        if (error)
            return error;
        value.months = calendar == QJS_CAL_COPTIC || calendar == QJS_CAL_ETHIOAA ||
                       calendar == QJS_CAL_ETHIOPIC ? 13 :
                       calendar == QJS_CAL_HEBREW ? 12 + qjs_cal_hebrew_leap(year) : 12;
        if (calendar == QJS_CAL_HEBREW && value.months == 13)
            value.leap_month = 5;
        for (month = 1; month <= value.months; month++) {
            int length;

            if (calendar == QJS_CAL_HEBREW)
                length = qjs_cal_hebrew_month_length(year, month);
            else if (calendar == QJS_CAL_PERSIAN)
                length = qjs_cal_persian_month_length(year, month);
            else if (islamic(calendar))
                length = qjs_cal_islamic_month_length(calendar, year, month);
            else
                length = qjs_cal_solar_month_length(calendar, year, month);
            if (length < 5 || length > 31)
                return QJS_CAL_BACKEND;
            value.lengths[month - 1] = length;
        }
    }
    for (month = 0; month < value.months; month++)
        sum += value.lengths[month];
    if (sum != value.end_day - value.first_day || sum < 353 || sum > 385)
        return QJS_CAL_BACKEND;
    *result = value;
    return QJS_CAL_OK;
}

int64_t qjs_cal_native_month_start(const QJSNativeCalendarYear *year, int month)
{
    int64_t day = year->first_day;
    int i;

    for (i = 1; i < month; i++)
        day += year->lengths[i - 1];
    return day;
}

static void month_code(const QJSNativeCalendarYear *year, int month, char code[5])
{
    int nominal = month, leap = 0;

    if (year->leap_month && month > year->leap_month) {
        nominal--;
        leap = month == year->leap_month + 1;
    }
    code[0] = 'M';
    code[1] = '0' + nominal / 10;
    code[2] = '0' + nominal % 10;
    code[3] = leap ? 'L' : 0;
    code[4] = 0;
}

int qjs_calendar_backend_month_info(QJSCalendarId calendar, int32_t year,
                                    int month, int *months, int *days,
                                    char code[5])
{
    QJSNativeCalendarYear info;
    int error = qjs_cal_native_year(calendar, year, &info);

    if (error)
        return error;
    if (month < 1 || month > info.months)
        return QJS_CAL_RANGE;
    *months = info.months;
    *days = info.lengths[month - 1];
    month_code(&info, month, code);
    return QJS_CAL_OK;
}

int qjs_calendar_backend_to_epoch_day(QJSCalendarId calendar, int32_t year,
                                      int month, int day, int64_t *result)
{
    QJSNativeCalendarYear info;
    int error = qjs_cal_native_year(calendar, year, &info);

    if (error)
        return error;
    if (month < 1 || month > info.months || day < 1 || day > info.lengths[month - 1])
        return QJS_CAL_RANGE;
    *result = qjs_cal_native_month_start(&info, month) + day - 1;
    return QJS_CAL_OK;
}

static int year_containing_day(QJSCalendarId calendar, int64_t day,
                                int32_t *result, QJSNativeCalendarYear *info)
{
    int32_t low = QJS_CAL_MIN_YEAR, high = QJS_CAL_MAX_YEAR + 1, year;
    int64_t first, end;
    int error;

    if (qjs_cal_native_lunar(calendar)) {
        QJSISODate iso;

        /* M01 starts in the ISO year sharing the arithmetic year number.
           Only January/February can belong to the preceding lunar year.
           Avoid binary-searching twenty expensive astronomical years. */
        if (qjs_iso_date_from_days(&iso, day) || iso.year < low || iso.year > high)
            return QJS_CAL_RANGE;
        year = iso.year == high ? high - 1 : iso.year;
        error = qjs_cal_native_year(calendar, year, info);
        if (error)
            return error;
        if (day < info->first_day) {
            if (year == low)
                return QJS_CAL_RANGE;
            error = qjs_cal_native_year(calendar, --year, info);
            if (error)
                return error;
        }
    } else {
        error = year_start(calendar, low, &first);
        if (error)
            return error;
        error = year_start(calendar, high, &end);
        if (error)
            return error;
        if (day < first || day >= end)
            return QJS_CAL_RANGE;
        while (high - low > 1) {
            int32_t middle = low + (high - low) / 2;

            error = year_start(calendar, middle, &first);
            if (error)
                return error;
            if (first <= day)
                low = middle;
            else
                high = middle;
        }
        year = low;
        error = qjs_cal_native_year(calendar, year, info);
        if (error)
            return error;
    }
    if (day < info->first_day || day >= info->end_day)
        return QJS_CAL_RANGE;
    *result = year;
    return QJS_CAL_OK;
}

int qjs_calendar_backend_from_epoch_day(QJSCalendarId calendar, int64_t day,
                                        QJSCalendarDate *result)
{
    QJSNativeCalendarYear info;
    QJSCalendarDate value = {0};
    int month = 1, remaining, error;

    if (!qjs_calendar_backend_is_supported(calendar))
        return QJS_CAL_UNSUPPORTED;
    error = year_containing_day(calendar, day, &value.year, &info);
    if (error)
        return error;
    remaining = day - info.first_day;
    value.day_of_year = remaining + 1;
    while (remaining >= info.lengths[month - 1])
        remaining -= info.lengths[month++ - 1];
    value.month = month;
    value.day = remaining + 1;
    value.days_in_month = info.lengths[month - 1];
    value.days_in_year = info.end_day - info.first_day;
    value.months_in_year = info.months;
    value.in_leap_year = calendar == QJS_CAL_HEBREW || qjs_cal_native_lunar(calendar) ?
                         info.months == 13 :
                         islamic(calendar) ? value.days_in_year > 354 :
                         value.days_in_year > 365;
    month_code(&info, month, value.month_code);
    qjs_cal_set_era(calendar, day, &value);
    *result = value;
    return QJS_CAL_OK;
}

int qjs_calendar_backend_year_from_era(QJSCalendarId calendar, const char *era,
                                       int32_t era_year, int32_t *result)
{
    return qjs_cal_year_from_era(calendar, era, era_year, result);
}

int qjs_calendar_backend_month_ordinal(QJSCalendarId calendar, int32_t year,
                                       int nominal, int leap, int constrain,
                                       int *result)
{
    QJSNativeCalendarYear info;
    int error, month = nominal;

    if (nominal < 1 || nominal > (qjs_cal_native_lunar(calendar) ||
        calendar == QJS_CAL_HEBREW ? 12 : calendar == QJS_CAL_COPTIC ||
        calendar == QJS_CAL_ETHIOAA || calendar == QJS_CAL_ETHIOPIC ? 13 : 12))
        return QJS_CAL_RANGE;
    if (leap && !(qjs_cal_native_lunar(calendar) ||
                  (calendar == QJS_CAL_HEBREW && nominal == 5)))
        return QJS_CAL_RANGE;
    error = qjs_cal_native_year(calendar, year, &info);
    if (error)
        return error;
    if (leap) {
        if (info.leap_month == nominal)
            month++;
        else if (!constrain)
            return QJS_CAL_RANGE;
        else if (calendar == QJS_CAL_HEBREW)
            month = 6; /* Missing Adar I constrains to Adar (M06). */
        else if (info.leap_month && nominal > info.leap_month)
            month++;
    } else if (info.leap_month && nominal > info.leap_month) {
        month++;
    }
    *result = month;
    return QJS_CAL_OK;
}
