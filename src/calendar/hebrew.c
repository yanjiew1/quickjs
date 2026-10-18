/* Hebrew arithmetic adapted from ICU 78.3 hebrwcal.cpp.
 * Copyright (c) 2016 and later: Unicode, Inc. and others.
 * Copyright (C) 2003-2016, International Business Machines Corporation
 * and others. All Rights Reserved. Unicode License V3: LICENSE-ICU.
 * C adaptation copyright (c) 2026 Yan-Jie Wang. MIT, project LICENSE.
 */
#include "arithmetic.h"
#include "arithmetic-internal.h"

int qjs_cal_hebrew_leap(int32_t year)
{
    return qjs_cal_floor_modulo(12LL * year + 17, 19) >= 12;
}

int64_t qjs_cal_hebrew_months_before_year(int32_t year)
{
    return qjs_cal_floor_divide(235LL * year - 234, 19);
}

static int64_t year_start(int32_t year)
{
    int64_t months = qjs_cal_hebrew_months_before_year(year);
    int64_t parts = months * (12 * 1080 + 793) + 11 * 1080 + 204;
    int64_t day = months * 29 + qjs_cal_floor_divide(parts, 24 * 1080);
    int64_t fraction = qjs_cal_floor_modulo(parts, 24 * 1080);
    int weekday = (int)qjs_cal_floor_modulo(day, 7); /* Zero is Monday. */

    if (weekday == 2 || weekday == 4 || weekday == 6)
        day++;
    /* Equality postpones too; strict comparisons can produce a 382-day year. */
    else if (weekday == 1 && fraction >= 15 * 1080 + 204 &&
             !qjs_cal_hebrew_leap(year))
        day += 2;
    else if (weekday == 0 && fraction >= 21 * 1080 + 589 &&
             qjs_cal_hebrew_leap(year - 1))
        day++;
    /* ICU's startOfYear denotes the day before 1 Tishrei. */
    return day + INT64_C(347998) - 2440588;
}

int qjs_cal_hebrew_year_start(int32_t year, int64_t *result)
{
    if (year < QJS_CAL_MIN_YEAR - 1 || year > QJS_CAL_MAX_YEAR + 1)
        return QJS_CAL_RANGE;
    *result = year_start(year);
    return 0;
}

int qjs_cal_hebrew_month_length(int32_t year, int month)
{
    static const unsigned char lengths[13] =
        {30, 29, 29, 29, 30, 30, 29, 30, 29, 30, 29, 30, 29};
    int length, leap, slot;

    if (year < QJS_CAL_MIN_YEAR || year > QJS_CAL_MAX_YEAR || month < 1)
        return 0;
    leap = qjs_cal_hebrew_leap(year);
    if (month > 12 + leap)
        return 0;
    slot = month - 1;
    if (!leap && slot >= 5)
        slot++;
    if (slot != 1 && slot != 2)
        return lengths[slot];
    /* Only Heshvan and Kislev depend on the length of the complete year. */
    length = (int)(year_start(year + 1) - year_start(year));
    if (slot == 1)
        return length % 10 == 5 ? 30 : 29;
    return length % 10 == 3 ? 29 : 30;
}
