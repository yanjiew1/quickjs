/* Portable solar calendar rules, adapted from ICU 78.3:
 * source/i18n/{cecal,coptccal,ethpccal,indiancal,persncal}.cpp.
 * © 2016 and later: Unicode, Inc. and others.
 * License & terms of use: http://www.unicode.org/copyright.html
 * Copyright (C) 2003 - 2009, International Business Machines Corporation and
 * others. All Rights Reserved. (cecal.cpp)
 * Copyright (C) 2003 - 2013, International Business Machines Corporation and
 * others. All Rights Reserved. (coptccal.cpp, ethpccal.cpp)
 * Copyright (C) 2003-2014, International Business Machines Corporation
 * and others. All Rights Reserved. (indiancal.cpp)
 * Copyright (C) 2003-2013, International Business Machines Corporation
 * and others. All Rights Reserved. (persncal.cpp)
 * Exact upstream notices and Unicode License V3 are in LICENSE-ICU.
 * Gregorian integer conversion and C adaptation copyright (c) 2026
 * Yan-Jie Wang, MIT license, see project LICENSE.
 */
#include "internal.h"
#ifdef QJS_CAL_USE_PERSIAN_AUTHORITY_TABLE
#include "persian-years.inc"
#endif

int64_t qjs_calendar_gregorian_to_epoch_day_unchecked(int32_t year,
                                                    int month, int day)
{
    int64_t y = (int64_t)year - (month <= 2);
    int64_t era = qjs_cal_floor_div(y, 400);
    int64_t yoe = y - era * 400;
    int mp = month + (month > 2 ? -3 : 9);
    int doy = (153 * mp + 2) / 5 + day - 1;
    return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}

int32_t qjs_calendar_gregorian_year_from_epoch_day(int64_t epoch_day)
{
    int64_t z = epoch_day + 719468;
    int64_t era = qjs_cal_floor_div(z, 146097);
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = yoe + era * 400;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    return (int32_t)(y + (mp >= 10));
}

/* Root can enable the pinned mirrored-transcription table for 1206..1498 AP.
 * Its 1499 entry is only the explicit 1498 leap marker's derived end boundary.
 * ICU's corrected 33-year approximation covers all other year starts.
 * Service discovery still requires the separate authority verification gate. */
static const int16_t persian_non_leap_years[] = {
    1502,1601,1634,1667,1700,1733,1766,1799,1832,1865,1898,1931,1964,1997,
    2030,2059,2063,2096,2129,2158,2162,2191,2195,2224,2228,2257,2261,2290,
    2294,2323,2327,2356,2360,2389,2393,2422,2426,2455,2459,2488,2492,2521,
    2525,2554,2558,2587,2591,2620,2624,2653,2657,2686,2690,2719,2723,2748,
    2752,2756,2781,2785,2789,2818,2822,2847,2851,2855,2880,2884,2888,2913,
    2917,2921,2946,2950,2954,2979,2983,2987
};
static int persian_correction(int32_t year)
{
    size_t lo = 0, hi = sizeof(persian_non_leap_years) /
                             sizeof(persian_non_leap_years[0]);
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (persian_non_leap_years[mid] < year) lo = mid + 1;
        else hi = mid;
    }
    return lo < sizeof(persian_non_leap_years) /
                sizeof(persian_non_leap_years[0]) &&
           persian_non_leap_years[lo] == year;
}

int64_t qjs_cal_solar_year_start(QJSCalendarId calendar, int32_t year)
{
    int32_t gyear;
    switch (calendar) {
    case QJS_CAL_BUDDHIST:
        return qjs_calendar_gregorian_to_epoch_day_unchecked(year - 543, 1, 1);
    case QJS_CAL_ROC:
        return qjs_calendar_gregorian_to_epoch_day_unchecked(year + 1911, 1, 1);
    case QJS_CAL_COPTIC:
        return INT64_C(1824665) - 2440588 + 365LL * year +
               qjs_cal_floor_div(year, 4);
    case QJS_CAL_ETHIOPIC:
        return INT64_C(1723856) - 2440588 + 365LL * year +
               qjs_cal_floor_div(year, 4);
    case QJS_CAL_ETHIOAA:
        return -INT64_C(285019) - 2440588 + 365LL * year +
               qjs_cal_floor_div(year, 4);
    case QJS_CAL_INDIAN:
        gyear = year + 78;
        return qjs_calendar_gregorian_to_epoch_day_unchecked(gyear, 3,
                                                qjs_cal_gregorian_leap(gyear) ? 21 : 22);
    case QJS_CAL_PERSIAN:
#ifdef QJS_CAL_USE_PERSIAN_AUTHORITY_TABLE
        if (year >= QJS_PERSIAN_AUTHORITY_FIRST && year <= QJS_PERSIAN_AUTHORITY_LAST + 1)
            return qjs_persian_authority_starts[year - QJS_PERSIAN_AUTHORITY_FIRST];
#endif
        return INT64_C(1948320) - 2440588 + 365LL * (year - 1LL) +
               qjs_cal_floor_div(8LL * year + 21, 33) -
               persian_correction(year - 1);
    default:
        return qjs_calendar_gregorian_to_epoch_day_unchecked(year, 1, 1);
    }
}

int qjs_cal_solar_month_length(QJSCalendarId calendar, int32_t year, int month)
{
    static const unsigned char gregorian_days[12] =
        {31,28,31,30,31,30,31,31,30,31,30,31};
    int32_t gyear = year;
    switch (calendar) {
    case QJS_CAL_COPTIC: case QJS_CAL_ETHIOPIC: case QJS_CAL_ETHIOAA:
        return month <= 12 ? 30 : 5 + (qjs_cal_floor_mod(year, 4) == 3);
    case QJS_CAL_INDIAN:
        return month == 1 ? 30 + qjs_cal_gregorian_leap(year + 78) :
               month <= 6 ? 31 : 30;
    case QJS_CAL_PERSIAN:
#ifdef QJS_CAL_USE_PERSIAN_AUTHORITY_TABLE
        if (month == 12 && year >= QJS_PERSIAN_AUTHORITY_FIRST && year <= QJS_PERSIAN_AUTHORITY_LAST)
            return 29 + (qjs_persian_authority_stars[year - QJS_PERSIAN_AUTHORITY_FIRST] != 0);
#endif
        return month <= 6 ? 31 : month <= 11 ? 30 :
            (int)(qjs_cal_solar_year_start(calendar, year + 1) -
                  qjs_cal_solar_year_start(calendar, year) - 336);
    case QJS_CAL_BUDDHIST: gyear -= 543; break;
    case QJS_CAL_ROC: gyear += 1911; break;
    default: break;
    }
    return gregorian_days[month - 1] +
           (month == 2 && qjs_cal_gregorian_leap(gyear));
}
