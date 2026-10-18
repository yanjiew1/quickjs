/* Tabular Hijri rules adapted from ICU 78.3 islamcal.cpp.
 * Copyright (c) 2016 and later: Unicode, Inc. and others.
 * Copyright (C) 2003-2015, International Business Machines Corporation
 * and others. All Rights Reserved. Unicode License V3: LICENSE-ICU.
 * C adaptation copyright (c) 2026 Yan-Jie Wang. MIT, project LICENSE.
 */
#include "arithmetic.h"
#include "arithmetic-internal.h"
#include "data.h"

static int64_t civil_start(int32_t year)
{
    return 354LL * (year - 1LL) + qjs_cal_floor_divide(11LL * year + 3, 30);
}

int qjs_cal_islamic_year_start(QJSCalendarId calendar, int32_t year, int64_t *result)
{
    QJSCalendarYear published;
    int status;

    if (year < QJS_CAL_MIN_YEAR - 1 || year > QJS_CAL_MAX_YEAR + 1)
        return QJS_CAL_RANGE;
    if (calendar == QJS_CAL_ISLAMIC_UMALQURA) {
        status = qjs_calendar_data_year(&qjs_native_calendar_data, calendar, year,
                                        &published);
        if (!status) {
            *result = published.first_day;
            return 0;
        }
        if (status != QJS_CAL_RANGE)
            return status;
        /* ECMA-402 requires civil rules outside the published AH interval. */
        calendar = QJS_CAL_ISLAMIC_CIVIL;
    }
    if (calendar != QJS_CAL_ISLAMIC_CIVIL && calendar != QJS_CAL_ISLAMIC_TBLA)
        return QJS_CAL_UNSUPPORTED;
    *result = civil_start(year) + INT64_C(1948440) - 2440588 -
              (calendar == QJS_CAL_ISLAMIC_TBLA);
    return 0;
}

int qjs_cal_islamic_month_length(QJSCalendarId calendar, int32_t year, int month)
{
    QJSCalendarYear published;
    int status;

    if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR || month < 1 ||
        month > 12)
        return 0;
    if (calendar == QJS_CAL_ISLAMIC_UMALQURA) {
        status = qjs_calendar_data_year(&qjs_native_calendar_data, calendar, year,
                                        &published);
        if (!status)
            return 29 + ((published.month_lengths >> (2 * (month - 1))) & 3);
        if (status != QJS_CAL_RANGE)
            return 0;
        calendar = QJS_CAL_ISLAMIC_CIVIL;
    }
    if (calendar != QJS_CAL_ISLAMIC_CIVIL && calendar != QJS_CAL_ISLAMIC_TBLA)
        return 0;
    return 29 + (month & 1) +
           (month == 12 && civil_start(year + 1) - civil_start(year) == 355);
}
