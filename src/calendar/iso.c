/*
 * Portable ISO Gregorian date arithmetic
 *
 * Copyright (c) 2026 Yan-Jie Wang
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
#include "iso.h"

/* Common-year month lengths. This owner alone defines the Gregorian table. */
static const uint8_t qjs_iso_month_days[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
};

int qjs_iso_leap_year(int32_t year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int64_t qjs_iso_floor_divide(int64_t value, int64_t divisor)
{
    return value / divisor - (value % divisor < 0);
}

/* Count whole common years and the leap days before January 1. Negative
   years use mathematical floor and the proleptic Gregorian year zero. */
static int64_t qjs_iso_year_start(int64_t year)
{
    int64_t previous = year - 1;

    return (year - 1970) * 365 +
        qjs_iso_floor_divide(previous, 4) - 492 -
        qjs_iso_floor_divide(previous, 100) + 19 +
        qjs_iso_floor_divide(previous, 400) - 4;
}

int qjs_iso_days_in_month(int32_t year, int32_t month)
{
    if (month < 1 || month > 12)
        return 0;
    return qjs_iso_month_days[month - 1] +
        (month == 2 && qjs_iso_leap_year(year));
}

int qjs_iso_date_is_valid(QJSISODate date)
{
    return date.month >= 1 && date.month <= 12 && date.day >= 1 &&
        date.day <= qjs_iso_days_in_month(date.year, date.month);
}

int qjs_iso_date_to_days(int64_t *result, QJSISODate date)
{
    int64_t days;
    int month;

    if (!qjs_iso_date_is_valid(date))
        return -1;
    days = qjs_iso_year_start(date.year) + date.day - 1;
    for (month = 1; month < date.month; month++)
        days += qjs_iso_days_in_month(date.year, month);
    *result = days;
    return 0;
}

int qjs_iso_date_from_days(QJSISODate *result, int64_t days)
{
    QJSISODate date;
    int64_t cycle, within_cycle, year, start, remaining;
    int month_days;

    if (days < qjs_iso_year_start(INT32_MIN) ||
        days >= qjs_iso_year_start((int64_t)INT32_MAX + 1))
        return -1;
    /* Year zero starts 719528 days before 1970. Every complete 400-year
       period has 146097 days, including years divisible by 400. */
    cycle = qjs_iso_floor_divide(days + 719528, 146097);
    within_cycle = days + 719528 - cycle * 146097;
    year = within_cycle / 365;
    if (year > 399)
        year = 399;
    year += cycle * 400;
    start = qjs_iso_year_start(year);
    /* The common-year estimate is at most one year too high: a cycle
       contributes only 97 leap days, fewer than one common year. */
    if (days < start) {
        year--;
        start = qjs_iso_year_start(year);
    }
    date.year = (int32_t)year;
    date.month = 1;
    remaining = days - start;
    for (;;) {
        month_days = qjs_iso_days_in_month(date.year, date.month);
        if (remaining < month_days)
            break;
        remaining -= month_days;
        date.month++;
    }
    date.day = (int32_t)remaining + 1;
    *result = date;
    return 0;
}
