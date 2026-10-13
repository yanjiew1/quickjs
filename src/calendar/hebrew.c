/* Hebrew arithmetic translated and adapted from ICU 78.3 hebrwcal.cpp.
 * © 2016 and later: Unicode, Inc. and others.
 * License & terms of use: http://www.unicode.org/copyright.html
 * Copyright (C) 2003-2016, International Business Machines Corporation
 * and others. All Rights Reserved.
 * Exact upstream notice and Unicode License V3 are in LICENSE-ICU.
 * C adaptation copyright (c) 2026 Yan-Jie Wang. MIT, project LICENSE.
 * Uses no cache/allocation. Floor division and nonnegative remainders extend
 * the postponement arithmetic proleptically to negative arithmetic years.
 */
#include "internal.h"

int qjs_cal_hebrew_leap(int32_t year)
{
    return qjs_cal_floor_mod(12LL * year + 17, 19) >= 12;
}
int64_t qjs_cal_hebrew_months_before_year(int32_t year)
{
    return qjs_cal_floor_div(235LL * year - 234, 19);
}
int64_t qjs_cal_hebrew_year_start(int32_t year)
{
    int64_t months = qjs_cal_hebrew_months_before_year(year);
    int64_t parts = months * (12 * 1080 + 793) + 11 * 1080 + 204;
    int64_t day = months * 29 + qjs_cal_floor_div(parts, 24 * 1080);
    int64_t fraction = qjs_cal_floor_mod(parts, 24 * 1080);
    int weekday = (int)qjs_cal_floor_mod(day, 7); /* 0 is Monday. */
    if (weekday == 2 || weekday == 4 || weekday == 6)
        day++;
    else if (weekday == 1 && fraction > 15 * 1080 + 204 &&
             !qjs_cal_hebrew_leap(year))
        day += 2;
    else if (weekday == 0 && fraction > 21 * 1080 + 589 &&
             qjs_cal_hebrew_leap(year - 1))
        day++;
    /* ICU's startOfYear denotes the day before 1 Tishrei. */
    return day + INT64_C(347998) - 2440588;
}
int qjs_cal_hebrew_month_length(int32_t year, int month)
{
    static const unsigned char lengths[13] =
        {30,29,29,29,30,30,29,30,29,30,29,30,29};
    int length = (int)(qjs_cal_hebrew_year_start(year + 1) -
                       qjs_cal_hebrew_year_start(year));
    int leap = qjs_cal_hebrew_leap(year);
    int slot = month - 1;
    if (!leap && slot >= 5) slot++;
    if (slot == 1) return length % 10 == 5 ? 30 : 29;
    if (slot == 2) return length % 10 == 3 ? 29 : 30;
    return lengths[slot];
}
