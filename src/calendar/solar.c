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
 */
#include "arithmetic.h"
#include "iso.h"

static int64_t floor_divide(int64_t value, int64_t divisor)
{
    return value / divisor - (value % divisor < 0);
}

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
        day = INT64_C(1824665) - 2440588 + 365LL * year + floor_divide(year, 4);
        break;
    case QJS_CAL_ETHIOPIC:
        day = INT64_C(1723856) - 2440588 + 365LL * year + floor_divide(year, 4);
        break;
    case QJS_CAL_ETHIOAA:
        day = -INT64_C(285019) - 2440588 + 365LL * year + floor_divide(year, 4);
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
        return month <= 12 ? 30 : 5 + ((year % 4 + 4) % 4 == 3);
    default:
        if (month > 12 || gregorian_year(calendar, year, &gyear))
            return 0;
        if (calendar == QJS_CAL_INDIAN)
            return month == 1 ? 30 + qjs_iso_leap_year(gyear) :
                   month <= 6 ? 31 : 30;
        return qjs_iso_days_in_month(gyear, month);
    }
}
