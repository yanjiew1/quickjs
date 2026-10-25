/* Bounded shared month arithmetic; never iterate once per requested month.
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
#ifndef QUICKJS_CALENDAR_MONTHS_H
#define QUICKJS_CALENDAR_MONTHS_H
#include "types.h"

/* The layout and ordinal month have already been checked by the provider.
   leap_month is nominal: the repeated ordinal immediately follows it. */
static inline int64_t qjs_cal_layout_month_start(const QJSCalendarYearLayout *year, int month)
{
    int64_t day = year->first_day;
    int i;

    for (i = 1; i < month; i++)
        day += year->lengths[i - 1];
    return day;
}

static inline void qjs_cal_layout_month_code(const QJSCalendarYearLayout *year, int month,
                                             char code[5])
{
    int nominal = month, leap = 0;

    if (year->leap_month && month > year->leap_month) {
        nominal--;
        leap = month == year->leap_month + 1;
    }
    code[0] = 'M';
    code[1] = '0' + nominal / 10;
    code[2] = '0' + nominal % 10;
    code[3] = leap ? 'L' : 0;
    code[4] = 0;
}

/* Fixed, Hebrew-cycle and lunisolar month indexes use authoritative provider
   year layouts. Large deltas never cause one operation per requested month.
   Failure preserves both outputs; the callback must preserve its output too. */
int qjs_cal_add_months(QJSCalendarId calendar, int32_t year, int month, int64_t delta,
                       QJSCalendarReadYear *read_year, int32_t *result_year, int *result_month);
/* Exact 235-month/19-year cycle; accepts the complete int32_t year interval. */
int64_t qjs_cal_hebrew_months_before_year(int32_t year);
#endif
