/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "chronology.h"
#include "../../calendar/calendar.h"
#include <string.h>

static void decimal_code(char out[3], unsigned int value)
{
    if (value >= 10) {
        out[0] = '0' + value / 10;
        out[1] = '0' + value % 10;
        out[2] = 0;
    } else {
        out[0] = '0' + value;
        out[1] = 0;
    }
}

QJSIntlStatus qjs_intl_date_calendar_fields(QJSCalendarId calendar, int64_t epoch_day,
                                           QJSIntlDateCalendarFields *out)
{
    QJSIntlDateCalendarFields result = {0};
    QJSCalendarDate date;
    unsigned int nominal, key;
    int error, weekday;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (unsigned int)calendar >= QJS_CAL_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    /* Gregorian Intl formatting is mandatory even with ISO-only arithmetic. */
    error = qjs_calendar_from_epoch_day(calendar == QJS_CAL_GREGORY ? QJS_CAL_ISO8601 : calendar,
                                        epoch_day, &date);
    if (error)
        return error == QJS_CAL_UNSUPPORTED ? QJS_INTL_UNSUPPORTED :
               error == QJS_CAL_RANGE ? QJS_INTL_INVALID_ARGUMENT :
               error == QJS_CAL_MEMORY ? QJS_INTL_NO_MEMORY : QJS_INTL_DATA_ERROR;
    /* The facade supplies a checked date. Keep name-key interpretation here,
       not in arithmetic or in the raw localized-name owner. */
    nominal = (date.month_code[1] - '0') * 10 + date.month_code[2] - '0';
    result.arithmetic_year = date.year;
    result.era_year = date.era_year;
    result.numeric.year = date.year;
    result.numeric.month = date.month;
    result.numeric.day = date.day;
    result.in_leap_year = date.in_leap_year;
    result.leap_month = date.month_code[3] == 'L';
    memcpy(result.month_code, date.month_code, sizeof(result.month_code));
    key = nominal;
    if (calendar == QJS_CAL_HEBREW) {
        if (result.leap_month)
            key = 6; /* M05L / Adar I. M06 / Adar II remains fixed raw key 7. */
        else if (nominal >= 6)
            key++;
    } else if (calendar == QJS_CAL_CHINESE || calendar == QJS_CAL_DANGI) {
        int cycle = (int)(((int64_t)date.year - 4) % 60);
        if (cycle < 0)
            cycle += 60;
        result.has_cyclic_year = 1;
        result.cyclic_year = cycle + 1;
        result.numeric.related_year = date.year;
        result.numeric.month = nominal;
        decimal_code(result.cyclic_key, result.cyclic_year);
    }
    decimal_code(result.month_key, key);
    /* Reduce before adding Thursday's offset, including negative epoch days. */
    weekday = (int)(epoch_day % 7);
    if (weekday < 0)
        weekday += 7;
    result.weekday = (weekday + 4) % 7;
    result.has_era = date.has_era;
    result.era_calendar = calendar;
    memcpy(result.era_code, date.era, sizeof(result.era_code));
    if (calendar == QJS_CAL_ISO8601 || calendar == QJS_CAL_GREGORY) {
        result.has_era = 1;
        result.era_calendar = QJS_CAL_GREGORY;
        result.era_year = date.year <= 0 ? 1 - date.year : date.year;
        strcpy(result.era_code, date.year <= 0 ? "bce" : "ce");
    } else if (calendar == QJS_CAL_JAPANESE) {
        if (!strcmp(date.era, "ce") || !strcmp(date.era, "bce"))
            result.era_calendar = QJS_CAL_GREGORY;
        else
            result.numeric.year = date.era_year;
    } else if (calendar == QJS_CAL_ETHIOPIC) {
        result.numeric.year = date.era_year;
    }
    *out = result;
    return QJS_INTL_OK;
}
