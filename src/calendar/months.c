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
#include "months.h"
#include "arithmetic-internal.h"
#include "lunisolar.h"
#include <limits.h>

int64_t qjs_cal_hebrew_months_before_year(int32_t year)
{
    return qjs_cal_floor_divide(235LL * year - 234, 19);
}

int qjs_cal_add_months(QJSCalendarId calendar, int32_t year, int month,
                       int64_t delta, QJSCalendarReadYear *read_year,
                       int32_t *result_year, int *result_month)
{
    QJSCalendarYearLayout info;
    int64_t index, target, new_year;
    int32_t low, high;
    int new_month, error;

    if (calendar == QJS_CAL_CHINESE || calendar == QJS_CAL_DANGI)
        return qjs_cal_lunisolar_add_months(calendar, year, month, delta,
                                            read_year, result_year, result_month);
    error = read_year(calendar, year, &info);
    if (error)
        return error;
    if (month < 1 || month > info.months)
        return QJS_CAL_RANGE;
    index = calendar == QJS_CAL_HEBREW ? qjs_cal_hebrew_months_before_year(year) :
                                       (int64_t)year * info.months;
    index += month - 1;
    if ((delta > 0 && index > INT64_MAX - delta) ||
        (delta < 0 && index < INT64_MIN - delta))
        return QJS_CAL_RANGE;
    target = index + delta;
    if (calendar == QJS_CAL_HEBREW) {
        low = QJS_CAL_MIN_YEAR;
        high = QJS_CAL_MAX_YEAR + 1;
        if (target < qjs_cal_hebrew_months_before_year(low) ||
            target >= qjs_cal_hebrew_months_before_year(high))
            return QJS_CAL_RANGE;
        while (high - low > 1) {
            int32_t middle = low + (high - low) / 2;

            if (qjs_cal_hebrew_months_before_year(middle) <= target)
                low = middle;
            else
                high = middle;
        }
        new_year = low;
        new_month = (int)(target - qjs_cal_hebrew_months_before_year(low)) + 1;
    } else {
        new_year = qjs_cal_floor_divide(target, info.months);
        if (new_year < QJS_CAL_MIN_YEAR || new_year > QJS_CAL_MAX_YEAR)
            return QJS_CAL_RANGE;
        new_month = qjs_cal_floor_modulo(target, info.months) + 1;
    }
    *result_year = new_year;
    *result_month = new_month;
    return QJS_CAL_OK;
}
