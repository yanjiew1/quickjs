/* Fixed solar calendar rules adapted from ICU 78.3:
 * source/i18n/{cecal,coptccal,ethpccal,indiancal}.cpp.
 * Copyright (c) 2016 and later: Unicode, Inc. and others.
 * Copyright (C) 2003-2009, International Business Machines Corporation and
 * others. All Rights Reserved. (cecal.cpp)
 * Copyright (C) 2003-2013, International Business Machines Corporation and
 * others. All Rights Reserved. (coptccal.cpp, ethpccal.cpp)
 * Copyright (C) 2003-2014, International Business Machines Corporation and
 * others. All Rights Reserved. (indiancal.cpp)
 * Unicode License V3 and ICU notices are in LICENSE-ICU.
 * C adaptation copyright (c) 2026 Yan-Jie Wang, MIT, project LICENSE.
 *
 * The following MIT grant applies to the C adaptation; the ICU-derived
 * portions retain Unicode License V3.
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
#include "arithmetic.h"
#include "arithmetic-internal.h"
#include "iso.h"

static int gregorian_year(QJSCalendarId calendar, int32_t year, int32_t *result)
{
    switch (calendar) {
    case QJS_CAL_BUDDHIST: year -= 543; break;
    case QJS_CAL_ROC: year += 1911; break;
    case QJS_CAL_INDIAN: year += 78; break;
    case QJS_CAL_ISO8601: case QJS_CAL_GREGORY: case QJS_CAL_JAPANESE:
        break;
    default:
        return QJS_CAL_UNSUPPORTED;
    }
    *result = year;
    return 0;
}

int qjs_cal_solar_year_start(QJSCalendarId calendar, int32_t year, int64_t *result)
{
    int64_t day;
    int32_t gyear;
    QJSISODate date;

    if (year < QJS_CAL_MIN_YEAR - 1 || year > QJS_CAL_MAX_YEAR + 1)
        return QJS_CAL_RANGE;
    switch (calendar) {
    case QJS_CAL_COPTIC:
        day = INT64_C(1824665) - 2440588 + 365LL * year + qjs_cal_floor_divide(year, 4);
        break;
    case QJS_CAL_ETHIOPIC:
        day = INT64_C(1723856) - 2440588 + 365LL * year + qjs_cal_floor_divide(year, 4);
        break;
    case QJS_CAL_ETHIOAA:
        day = -INT64_C(285019) - 2440588 + 365LL * year + qjs_cal_floor_divide(year, 4);
        break;
    default:
        if (gregorian_year(calendar, year, &gyear))
            return QJS_CAL_UNSUPPORTED;
        date = (QJSISODate){gyear, 1, 1};
        if (calendar == QJS_CAL_INDIAN) {
            date.month = 3;
            date.day = qjs_iso_leap_year(gyear) ? 21 : 22;
        }
        if (qjs_iso_date_to_days(&day, date))
            return QJS_CAL_RANGE;
        break;
    }
    *result = day;
    return 0;
}

int qjs_cal_solar_month_length(QJSCalendarId calendar, int32_t year, int month)
{
    int32_t gyear;

    if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR || month < 1)
        return 0;
    switch (calendar) {
    case QJS_CAL_COPTIC: case QJS_CAL_ETHIOPIC: case QJS_CAL_ETHIOAA:
        if (month > 13)
            return 0;
        return month <= 12 ? 30 : 5 + (qjs_cal_floor_modulo(year, 4) == 3);
    default:
        if (month > 12 || gregorian_year(calendar, year, &gyear))
            return 0;
        if (calendar == QJS_CAL_INDIAN)
            return month == 1 ? 30 + qjs_iso_leap_year(gyear) :
                   month <= 6 ? 31 : 30;
        return qjs_iso_days_in_month(gyear, month);
    }
}
