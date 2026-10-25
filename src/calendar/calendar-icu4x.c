/* ICU4X calendar provider over checked foreign year layouts.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "calendar-backend.h"
#include "eras.h"
#include "data.h"
#include "months.h"
#include "../icu4x/calendar.h"
#include <string.h>

static int read_year(QJSCalendarId calendar, int32_t year, QJSCalendarYearLayout *out)
{
    QJSCalendarYearLayout value = {0};
    int error;

    if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR)
        return QJS_CAL_RANGE;
    QJSCalendarYear official;
    error = qjs_calendar_official_year(calendar, year, &official);
    if (!error) {
        int month;
        value.first_day = official.first_day;
        value.end_day = official.end_day;
        value.months = official.months;
        value.leap_month = official.leap_month;
        for (month = 0; month < value.months; month++)
            value.lengths[month] = 29 + ((official.month_lengths >> (2 * month)) & 3);
    } else if (error == QJS_CAL_UNSUPPORTED) {
        error = qjs_icu4x_calendar_year(calendar, year, &value);
    }
    if (!error)
        *out = value;
    return error;
}

int qjs_calendar_backend_is_supported(QJSCalendarId calendar)
{
    return calendar > QJS_CAL_ISO8601 && calendar < QJS_CAL_COUNT;
}

int qjs_calendar_backend_era_at(QJSCalendarId calendar, size_t index, const char **out)
{
    return qjs_cal_era_at(calendar, index, out);
}

int qjs_calendar_backend_from_epoch_day(QJSCalendarId calendar, int64_t day, QJSCalendarDate *out)
{
    QJSICU4XCalendarDate source = {0};
    QJSCalendarDate value = {0};
    int error = QJS_CAL_UNSUPPORTED;

    if (calendar == QJS_CAL_CHINESE || calendar == QJS_CAL_DANGI)
        error = qjs_calendar_official_date(calendar, day, &value);

    if (!error) {
        *out = value;
        return QJS_CAL_OK;
    }
    if (error != QJS_CAL_UNSUPPORTED)
        return error;
    error = qjs_icu4x_calendar_date(calendar, day, &source);
    if (error)
        return error;
    if (source.year < QJS_CAL_MIN_YEAR || source.year > QJS_CAL_MAX_YEAR)
        return QJS_CAL_RANGE;
    value.year = source.year;
    value.month = source.month;
    value.day = source.day;
    value.day_of_year = source.day_of_year;
    value.days_in_month = source.days_in_month;
    value.days_in_year = source.days_in_year;
    value.months_in_year = source.months_in_year;
    value.in_leap_year = source.in_leap_year;
    memcpy(value.month_code, source.month_code, sizeof(value.month_code));
    error = qjs_cal_set_era(calendar, day, &value);
    if (!error)
        *out = value;
    return error;
}

int qjs_calendar_backend_to_epoch_day(QJSCalendarId calendar, int32_t year, int month, int day,
                                      int64_t *out)
{
    QJSCalendarYearLayout value;
    int error = read_year(calendar, year, &value);

    if (error)
        return error;
    if (month < 1 || month > value.months || day < 1 || day > value.lengths[month - 1])
        return QJS_CAL_RANGE;
    *out = qjs_cal_layout_month_start(&value, month) + day - 1;
    return QJS_CAL_OK;
}

int qjs_calendar_backend_month_info(QJSCalendarId calendar, int32_t year, int month, int *months,
                                    int *days, char code[5])
{
    QJSCalendarYearLayout value;
    int error = read_year(calendar, year, &value);

    if (error)
        return error;
    if (month < 1 || month > value.months)
        return QJS_CAL_RANGE;
    *months = value.months;
    *days = value.lengths[month - 1];
    qjs_cal_layout_month_code(&value, month, code);
    return QJS_CAL_OK;
}

int qjs_calendar_backend_year_from_era(QJSCalendarId calendar, const char *era, int32_t era_year,
                                       int32_t *out)
{
    return qjs_cal_year_from_era(calendar, era, era_year, out);
}

int qjs_calendar_backend_month_ordinal(QJSCalendarId calendar, int32_t year, int nominal, int leap,
                                       int constrain, int *out)
{
    QJSCalendarYearLayout value;
    int lunar = calendar == QJS_CAL_CHINESE || calendar == QJS_CAL_DANGI;
    int extra =
        calendar == QJS_CAL_COPTIC || calendar == QJS_CAL_ETHIOAA || calendar == QJS_CAL_ETHIOPIC;
    int error, month = nominal;

    if (nominal < 1 || nominal > (extra ? 13 : 12) ||
        (leap && !(lunar || (calendar == QJS_CAL_HEBREW && nominal == 5))))
        return QJS_CAL_RANGE;
    error = read_year(calendar, year, &value);
    if (error)
        return error;
    if (leap) {
        if (value.leap_month == nominal)
            month++;
        else if (!constrain)
            return QJS_CAL_RANGE;
        else if (calendar == QJS_CAL_HEBREW)
            month = 6; /* Missing Adar I constrains to Adar (M06). */
        else if (value.leap_month && nominal > value.leap_month)
            month++;
    } else if (value.leap_month && nominal > value.leap_month) {
        month++;
    }
    *out = month;
    return QJS_CAL_OK;
}

int qjs_calendar_backend_add_months(QJSCalendarId calendar, int32_t year, int month, int64_t delta,
                                    int32_t *out_year, int *out_month)
{
    QJSICU4XCalendarYearMonth value;
    int error = qjs_icu4x_calendar_add_months(calendar, year, month, delta, &value);

    if (!error) {
        *out_year = value.year;
        *out_month = value.month;
    }
    return error;
}

