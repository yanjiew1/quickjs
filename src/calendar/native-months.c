/* Bounded native month arithmetic; never iterate once per requested month.
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
#include "native-internal.h"
#include "arithmetic.h"
#include "arithmetic-internal.h"
#include "astronomy.h"
#include "iso.h"
#include "lunisolar.h"
#include <limits.h>
#include <math.h>

static int month_index(QJSCalendarId calendar, int64_t day, int64_t *result)
{
    double time = (double)day * QJS_CAL_ASTRO_DAY_MS;
    int32_t offset;
    int error = qjs_calendar_lunisolar_zone_offset(calendar, time, &offset);

    if (error)
        return error;
    return qjs_calendar_astro_mean_lunation_number(time - offset, result);
}

static int qjs_cal_native_lunar_add_months(QJSCalendarId calendar, int32_t year,
                                          int month, int64_t delta,
                                          int32_t *result_year, int *result_month)
{
    QJSNativeCalendarYear source, target;
    QJSISODate iso;
    int64_t first_index, wanted, end_index, index, source_day;
    double estimate;
    int error, attempt, target_month;

    /* The complete arithmetic domain contains fewer than 26000013 months.
       Check before adding indices, including INT64_MIN and INT64_MAX. */
    if (delta < -INT64_C(26000013) || delta > INT64_C(26000013))
        return QJS_CAL_RANGE;
    error = qjs_cal_native_year(calendar, year, &source);
    if (error)
        return error;
    if (month < 1 || month > source.months)
        return QJS_CAL_RANGE;
    if (!delta) {
        *result_year = year;
        *result_month = month;
        return QJS_CAL_OK;
    }
    source_day = qjs_cal_native_month_start(&source, month);
    error = month_index(calendar, source_day, &index);
    if (error)
        return error;
    wanted = index + delta;
    /* Mean phase labels consecutive lunations even where publication dates
       differ from computed moon days. Use the mean only to seed a year;
       authoritative year records determine the resulting ordinal month. */
    estimate = (double)source_day + (double)delta *
               qjs_calendar_astro_mean_lunation_days();
    if (!isfinite(estimate) || estimate < -400000000 || estimate > 400000000 ||
        qjs_iso_date_from_days(&iso, (int64_t)floor(estimate)))
        return QJS_CAL_RANGE;
    /* The final lunar month may start in January of MAX_YEAR + 1. A mean
       seed is not an arithmetic-year decision; clamp before probing it. */
    year = iso.year < QJS_CAL_MIN_YEAR ? QJS_CAL_MIN_YEAR :
           iso.year > QJS_CAL_MAX_YEAR ? QJS_CAL_MAX_YEAR : iso.year;
    /* Across the admitted delta the original/extended mean rates differ
       by less than 53 days. Together with lunar new year in Jan/Feb this
       keeps the seed within one adjacent year; four checked probes allow
       a boundary correction without an unbounded astronomical search. */
    for (attempt = 0; attempt < 4; attempt++) {
        error = qjs_cal_native_year(calendar, year, &target);
        if (error)
            return error;
        error = month_index(calendar, target.first_day, &first_index);
        if (error)
            return error;
        error = month_index(calendar, target.end_day, &end_index);
        if (error)
            return error;
        if (end_index - first_index != target.months)
            return QJS_CAL_BACKEND;
        if (wanted < first_index) {
            year--;
        } else if (wanted >= end_index) {
            year++;
        } else {
            target_month = (int)(wanted - first_index) + 1;
            error = month_index(calendar, qjs_cal_native_month_start(&target,
                                 target_month), &index);
            if (error)
                return error;
            if (index != wanted)
                return QJS_CAL_BACKEND;
            *result_year = year;
            *result_month = target_month;
            return QJS_CAL_OK;
        }
    }
    return QJS_CAL_BACKEND;
}

int qjs_calendar_backend_add_months(QJSCalendarId calendar, int32_t year,
                                    int month, int64_t delta,
                                    int32_t *result_year, int *result_month)
{
    QJSNativeCalendarYear info;
    int64_t index, target, new_year;
    int32_t low, high;
    int new_month, error;

    if (qjs_cal_native_lunar(calendar))
        return qjs_cal_native_lunar_add_months(calendar, year, month, delta,
                                              result_year, result_month);
    error = qjs_cal_native_year(calendar, year, &info);
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
