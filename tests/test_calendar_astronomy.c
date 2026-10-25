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

/* Checked astronomical events and the documented remote phase extension. */
#include "../src/calendar/astronomy.h"
#include "../src/calendar/iso.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>

#define DAY QJS_CAL_ASTRO_DAY_MS
#define PI QJS_CAL_ASTRO_PI

static double midnight(int year, int month, int day)
{
    int64_t days;

    assert(!qjs_iso_date_to_days(&days, (QJSISODate){year, month, day}));
    return days * DAY;
}

static void events(double start)
{
    double previous, next, following, longitude;
    int64_t first_index, next_index;

    assert(!qjs_calendar_astro_new_moon(start, 0, &previous));
    assert(!qjs_calendar_astro_new_moon(start, 1, &next));
    assert(previous < start && next >= start);
    assert(next - previous > 28 * DAY && next - previous < 31 * DAY);
    assert(!qjs_calendar_astro_new_moon(next + DAY, 1, &following));
    assert(following - next > 28 * DAY && following - next < 31 * DAY);
    assert(!qjs_calendar_astro_mean_lunation_number(next, &first_index));
    assert(!qjs_calendar_astro_mean_lunation_number(following, &next_index));
    assert(next_index == first_index + 1);
    assert(!qjs_calendar_astro_solar_time(start, 3 * PI / 2, 1, &next));
    assert(next >= start && next - start < 367 * DAY);
    assert(!qjs_calendar_astro_sun_longitude(next, &longitude));
    assert(fabs(longitude - 3 * PI / 2) < 0.0001);
}

int main(void)
{
    const int years[] = {-1000000, -271821, -10001, -10000, -1, 0, 1900,
                         2024, 10000, 10001, 275760, 1000000};
    double event, saved = 123;
    int64_t index = 456;
    size_t i;

    assert(midnight(-10000, 1, 1) / DAY == -4371953);
    assert(midnight(10000, 1, 1) / DAY == 2932897);
    for (i = 0; i < sizeof(years) / sizeof(years[0]); i++)
        events(midnight(years[i], 7, 1));
    /* Modern sanity windows supplement structural numerical checks without
       claiming minute-level physical accuracy for the adopted approximation. */
    assert(!qjs_calendar_astro_solar_time(midnight(2024, 3, 19), 0, 1, &event));
    assert(event > midnight(2024, 3, 20) && event < midnight(2024, 3, 21));
    assert(!qjs_calendar_astro_new_moon(midnight(2024, 2, 9), 1, &event));
    assert(event > midnight(2024, 2, 9) && event < midnight(2024, 2, 10));
    assert(qjs_calendar_astro_new_moon(NAN, 1, &saved) == QJS_CAL_RANGE);
    assert(saved == 123);
    assert(qjs_calendar_astro_new_moon(0, 2, &saved) == QJS_CAL_RANGE);
    assert(saved == 123);
    assert(qjs_calendar_astro_solar_time(0, INFINITY, 1, &saved) == QJS_CAL_RANGE);
    assert(saved == 123);
    assert(qjs_calendar_astro_sun_longitude(INFINITY, &saved) == QJS_CAL_RANGE);
    assert(saved == 123);
    assert(qjs_calendar_astro_new_moon(QJS_CAL_ASTRO_MAX_ABS_MS, 1, &saved) == QJS_CAL_BACKEND);
    assert(saved == 123);
    assert(qjs_calendar_astro_mean_lunation_number(INFINITY, &index) == QJS_CAL_RANGE);
    assert(index == 456);
    assert(qjs_calendar_astro_mean_lunation_days() > 29.5);
    assert(qjs_calendar_astro_mean_lunation_days() < 29.6);
    return 0;
}
