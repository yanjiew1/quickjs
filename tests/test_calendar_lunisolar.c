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

/* Bounded approximation is separate from mandatory publication coverage. */
#include "../src/calendar/lunisolar.h"
#include "../src/calendar/astronomy.h"
#include "../src/calendar/iso.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>

static int64_t day(int year, int month, int date)
{
    int64_t result;

    assert(!qjs_iso_date_to_days(&result, (QJSISODate){year, month, date}));
    return result;
}

int main(void)
{
    const int years[] = {-1000000, -271822, -10001, -10000, -1, 0, 1899,
                         1900, 2023, 2050, 10000, 10001, 275760, 1000000};
    const QJSCalendarId calendars[] = {QJS_CAL_CHINESE, QJS_CAL_DANGI};
    QJSCalendarYear value, saved = {123, 456, 789, 12, 0};
    size_t c, i;
    int32_t offset = 123;

    for (c = 0; c < sizeof(calendars) / sizeof(calendars[0]); c++) {
        for (i = 0; i < sizeof(years) / sizeof(years[0]); i++) {
            int m, total = 0;

            assert(!qjs_cal_lunisolar_approximate_year(calendars[c], years[i], &value));
            assert(value.first_day >= day(years[i], 1, 1));
            assert(value.first_day < day(years[i] + 1, 1, 1));
            assert(value.months == (value.leap_month ? 13 : 12));
            assert(value.leap_month <= 12);
            for (m = 0; m < value.months; m++) {
                int days = 29 + ((value.month_lengths >> (2 * m)) & 3);

                assert(days == 29 || days == 30);
                total += days;
            }
            assert(total == value.end_day - value.first_day);
            if (years[i] < QJS_CAL_MAX_YEAR) {
                QJSCalendarYear next;

                assert(!qjs_cal_lunisolar_approximate_year(calendars[c], years[i] + 1, &next));
                assert(value.end_day == next.first_day);
            }
        }
        assert(!qjs_cal_lunisolar_approximate_year(calendars[c], 2023, &value));
        assert(value.first_day == day(2023, 1, 22));
        assert(value.end_day == day(2024, 2, 10));
        assert(value.months == 13 && value.leap_month == 2);
    }
    value = saved;
    assert(qjs_cal_lunisolar_approximate_year(QJS_CAL_ISO8601, 2024, &value) == QJS_CAL_UNSUPPORTED);
    assert(value.first_day == saved.first_day && value.month_lengths == saved.month_lengths);
    assert(qjs_cal_lunisolar_approximate_year(QJS_CAL_CHINESE, INT32_MAX, &value) == QJS_CAL_RANGE);
    assert(value.first_day == saved.first_day);
    assert(!qjs_calendar_lunisolar_zone_offset(QJS_CAL_CHINESE, 0, &offset));
    assert(offset == 8 * 3600000);
    assert(!qjs_calendar_lunisolar_zone_offset(QJS_CAL_DANGI, 0, &offset));
    assert(offset == 9 * 3600000);
    assert(qjs_calendar_lunisolar_zone_offset(QJS_CAL_DANGI, NAN, &offset) == QJS_CAL_RANGE);
    assert(offset == 9 * 3600000);
    {
        const int transitions[][3] = {{1897, 8, 7}, {1898, 7, 8}, {1912, 8, 9}};

        for (i = 0; i < sizeof(transitions) / sizeof(transitions[0]); i++) {
            double when = (transitions[i][0] - 1970.0) * 365 * QJS_CAL_ASTRO_DAY_MS -
                          transitions[i][1] * 3600000.0;

            assert(!qjs_calendar_lunisolar_zone_offset(QJS_CAL_DANGI, when - 1, &offset));
            assert(offset == transitions[i][1] * 3600000);
            assert(!qjs_calendar_lunisolar_zone_offset(QJS_CAL_DANGI, when, &offset));
            assert(offset == transitions[i][2] * 3600000);
        }
    }
    return 0;
}
