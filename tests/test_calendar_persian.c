/*
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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

/* Required official leap flags and bounded proleptic approximation. */
#include "../src/calendar/arithmetic.h"
#include "../src/calendar/data.h"
#include "../src/calendar/iso.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>

static void witness(int year, QJSISODate date)
{
    int64_t actual, expected;

    assert(!qjs_cal_persian_year_start(year, &actual));
    assert(!qjs_iso_date_to_days(&expected, date));
    assert(actual == expected);
}

int main(void)
{
    const int32_t years[] = {QJS_CAL_MIN_YEAR, -271821, -34, -33, -1, 0,
                             1, 33, 1205, 1499, 1502, 1503, 1601, 2987,
                             2988, 275760, QJS_CAL_MAX_YEAR};
    size_t i;
    int year;
    int64_t first, next, saved = 123;

    witness(1206, (QJSISODate){1827, 3, 22});
    witness(1403, (QJSISODate){2024, 3, 20});
    witness(1404, (QJSISODate){2025, 3, 21});
    witness(1499, (QJSISODate){2120, 3, 21});
    assert(qjs_cal_persian_month_length(1403, 12) == 30);
    assert(qjs_cal_persian_month_length(1404, 12) == 29);
    assert(qjs_cal_persian_month_length(1502, 12) == 29);
    assert(qjs_cal_persian_month_length(1503, 12) == 30);
    for (year = 1206; year <= 1498; year++) {
        QJSCalendarYear published;

        assert(!qjs_calendar_data_year(&qjs_native_calendar_data, QJS_CAL_PERSIAN,
                                       year, &published));
        assert(!qjs_cal_persian_year_start(year, &first));
        assert(!qjs_cal_persian_year_start(year + 1, &next));
        assert(first == published.first_day && next == published.end_day);
        assert(qjs_cal_persian_month_length(year, 12) == next - first - 336);
    }
    /* Include every corrected modern approximation year and its neighbors. */
    for (year = 1499; year <= 2990; year++) {
        assert(!qjs_cal_persian_year_start(year, &first));
        assert(!qjs_cal_persian_year_start(year + 1, &next));
        assert(next - first == 365 || next - first == 366);
    }
    for (i = 0; i < sizeof(years) / sizeof(years[0]); i++) {
        int month, days = 0;

        assert(!qjs_cal_persian_year_start(years[i], &first));
        assert(!qjs_cal_persian_year_start(years[i] + 1, &next));
        for (month = 1; month <= 12; month++) {
            int length = qjs_cal_persian_month_length(years[i], month);

            assert(month <= 6 ? length == 31 : month <= 11 ? length == 30 :
                   length == 29 || length == 30);
            days += length;
        }
        assert(days == next - first);
    }
    assert(qjs_cal_persian_year_start(INT32_MIN, &saved) == QJS_CAL_RANGE);
    assert(saved == 123);
    assert(qjs_cal_persian_year_start(INT32_MAX, &saved) == QJS_CAL_RANGE);
    assert(saved == 123);
    assert(!qjs_cal_persian_month_length(INT32_MAX, 1));
    assert(!qjs_cal_persian_month_length(1403, 0));
    assert(!qjs_cal_persian_month_length(1403, 13));
    return 0;
}
