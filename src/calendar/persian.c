/* Persian approximation adapted from ICU 78.3 persncal.cpp.
 * Copyright (c) 2016 and later: Unicode, Inc. and others.
 * Copyright (C) 2003-2013, International Business Machines Corporation
 * and others. All Rights Reserved. Unicode License V3: LICENSE-ICU.
 * C adaptation copyright (c) 2026 Yan-Jie Wang. MIT, project LICENSE.
 * Published leap markers take precedence for AP 1206..1498. The corrected
 * 33-year model below is only the documented approximation outside coverage.
 */
#include "arithmetic.h"
#include "arithmetic-internal.h"
#include "data.h"

static const int16_t corrected_common_years[] = {
    1502, 1601, 1634, 1667, 1700, 1733, 1766, 1799, 1832, 1865, 1898, 1931,
    1964, 1997, 2030, 2059, 2063, 2096, 2129, 2158, 2162, 2191, 2195, 2224,
    2228, 2257, 2261, 2290, 2294, 2323, 2327, 2356, 2360, 2389, 2393, 2422,
    2426, 2455, 2459, 2488, 2492, 2521, 2525, 2554, 2558, 2587, 2591, 2620,
    2624, 2653, 2657, 2686, 2690, 2719, 2723, 2748, 2752, 2756, 2781, 2785,
    2789, 2818, 2822, 2847, 2851, 2855, 2880, 2884, 2888, 2913, 2917, 2921,
    2946, 2950, 2954, 2979, 2983, 2987,
};

static int correction(int32_t year)
{
    size_t low = 0, high = sizeof(corrected_common_years) /
                            sizeof(corrected_common_years[0]);
    const size_t count = high;

    while (low < high) {
        size_t middle = low + (high - low) / 2;

        if (corrected_common_years[middle] < year)
            low = middle + 1;
        else
            high = middle;
    }
    return low < count && corrected_common_years[low] == year;
}

int qjs_cal_persian_year_start(int32_t year, int64_t *result)
{
    QJSCalendarYear published;
    int status;

    if (year < QJS_CAL_MIN_YEAR - 1 || year > QJS_CAL_MAX_YEAR + 1)
        return QJS_CAL_RANGE;
    status = qjs_calendar_data_year(&qjs_native_calendar_data, QJS_CAL_PERSIAN,
                                    year, &published);
    if (!status) {
        *result = published.first_day;
        return 0;
    }
    if (status != QJS_CAL_RANGE)
        return status;
    /* The final published year's successor is derived from its leap marker,
       rather than replacing that boundary with an independent approximation. */
    status = qjs_calendar_data_year(&qjs_native_calendar_data, QJS_CAL_PERSIAN,
                                    year - 1, &published);
    if (!status) {
        *result = published.end_day;
        return 0;
    }
    if (status != QJS_CAL_RANGE)
        return status;
    *result = INT64_C(1948320) - 2440588 + 365LL * (year - 1LL) +
              qjs_cal_floor_divide(8LL * year + 21, 33) - correction(year - 1);
    return 0;
}

int qjs_cal_persian_month_length(int32_t year, int month)
{
    int64_t first, next;

    if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR ||
        month < 1 || month > 12)
        return 0;
    if (month <= 11)
        return month <= 6 ? 31 : 30;
    if (qjs_cal_persian_year_start(year, &first) ||
        qjs_cal_persian_year_start(year + 1, &next))
        return 0;
    return (int)(next - first - 336);
}
