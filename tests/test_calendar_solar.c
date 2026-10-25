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

/* Solar epochs, Gregorian reuse and proleptic bounded-year invariants. */
#include "../src/calendar/arithmetic.h"
#include "../src/calendar/iso.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>

static void start(QJSCalendarId calendar, int32_t year, QJSISODate expected)
{
    int64_t actual, day;

    assert(!qjs_iso_date_to_days(&day, expected));
    assert(!qjs_cal_solar_year_start(calendar, year, &actual));
    assert(actual == day);
}

int main(void)
{
    const QJSCalendarId calendars[] = {QJS_CAL_ISO8601, QJS_CAL_BUDDHIST,
        QJS_CAL_COPTIC, QJS_CAL_ETHIOAA, QJS_CAL_ETHIOPIC, QJS_CAL_GREGORY,
        QJS_CAL_INDIAN, QJS_CAL_JAPANESE, QJS_CAL_ROC};
    const int32_t years[] = {QJS_CAL_MIN_YEAR, -271821, -400, -1, 0, 1, 3, 4,
        100, 1900, 2000, 2024, 275760, QJS_CAL_MAX_YEAR};
    size_t i, j;
    int64_t saved = 123;

    start(QJS_CAL_ISO8601, 1970, (QJSISODate){1970, 1, 1});
    start(QJS_CAL_BUDDHIST, 2567, (QJSISODate){2024, 1, 1});
    start(QJS_CAL_ROC, 1, (QJSISODate){1912, 1, 1});
    start(QJS_CAL_COPTIC, 1, (QJSISODate){284, 8, 29});
    start(QJS_CAL_ETHIOPIC, 2017, (QJSISODate){2024, 9, 11});
    start(QJS_CAL_ETHIOAA, 7517, (QJSISODate){2024, 9, 11});
    start(QJS_CAL_INDIAN, 1946, (QJSISODate){2024, 3, 21});
    start(QJS_CAL_INDIAN, 1947, (QJSISODate){2025, 3, 22});
    for (i = 0; i < sizeof(calendars) / sizeof(calendars[0]); i++) {
        for (j = 0; j < sizeof(years) / sizeof(years[0]); j++) {
            int64_t first, next;
            int month, length, total = 0;

            assert(!qjs_cal_solar_year_start(calendars[i], years[j], &first));
            assert(!qjs_cal_solar_year_start(calendars[i], years[j] + 1, &next));
            for (month = 1; (length = qjs_cal_solar_month_length(calendars[i], years[j], month)); month++) {
                assert(length >= 5 && length <= 31);
                total += length;
            }
            assert(next - first == total && (total == 365 || total == 366));
        }
        assert(qjs_cal_solar_year_start(calendars[i], INT32_MAX, &saved) == QJS_CAL_RANGE);
        assert(saved == 123);
        assert(qjs_cal_solar_year_start(calendars[i], INT32_MIN, &saved) == QJS_CAL_RANGE);
        assert(saved == 123);
        assert(!qjs_cal_solar_month_length(calendars[i], INT32_MAX, 1));
        assert(!qjs_cal_solar_month_length(calendars[i], 2024, 0));
        assert(!qjs_cal_solar_month_length(calendars[i], 2024, 14));
    }
    assert(qjs_cal_solar_year_start(QJS_CAL_HEBREW, 2024, &saved) == QJS_CAL_UNSUPPORTED);
    assert(saved == 123);
    assert(!qjs_cal_solar_month_length(QJS_CAL_HEBREW, 2024, 1));
    return 0;
}
