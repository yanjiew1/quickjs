/* ICU4C zone identity and transition contract regressions.
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
#include "../src/timezone/timezone.h"
#include "../src/timezone/windows-zone.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef struct Candidates {
    int64_t values[8];
    size_t count;
} Candidates;

static void collect(void *opaque, int64_t seconds)
{
    Candidates *values = opaque;
    assert(values->count < 8);
    values->values[values->count++] = seconds;
}

int main(void)
{
    const QJSTimeZone *zone, *previous = NULL;
    QJSTimeZoneInfo info;
    Candidates candidates = {{0}, 0};
    size_t count = 999, i;
    int32_t before, after;
    int64_t boundary = 123;
    int found, constant;
    char windows[128];

    assert(qjs_timezone_count() > 300);
    for (i = 0; i < qjs_timezone_count(); i++) {
        zone = qjs_timezone_at(i);
        assert(zone);
        if (previous)
            assert(strcmp(qjs_timezone_identifier(previous), qjs_timezone_identifier(zone)) < 0);
        previous = zone;
    }
    assert(!qjs_timezone_at(i));
    assert(qjs_timezone_lookup(&zone, "ACT", 3) == QJS_TZ_RANGE);
    assert(qjs_timezone_lookup(&zone, "SystemV/EST5", 12) == QJS_TZ_RANGE);
    assert(!qjs_timezone_lookup(&zone, "asia/calcutta", 13));
    assert(!strcmp(qjs_timezone_primary_identifier(zone), "Asia/Kolkata"));
    assert(!qjs_timezone_lookup(&zone, "Etc/UTC", 7));
    assert(!strcmp(qjs_timezone_primary_identifier(zone), "UTC"));

    assert(!qjs_timezone_lookup(&zone, "us/eastern", 10));
    assert(!strcmp(qjs_timezone_primary_identifier(zone), "America/New_York"));
    /* New York's missing 02:30 and repeated 01:30 exercise both interval
       enumeration and the offset-change boundary's inclusive convention. */
    assert(!qjs_timezone_candidates(zone, INT64_C(1710037800), collect, &candidates,
                                     &count, &before, &after, &boundary));
    assert(count == 0 && candidates.count == 0);
    assert(before == -18000 && after == -14400 && boundary == INT64_C(1710054000));
    assert(!qjs_timezone_candidates(zone, INT64_C(1730597400), collect, &candidates,
                                     &count, &before, &after, &boundary));
    assert(count == 2 && candidates.count == 2);
    assert(candidates.values[0] == INT64_C(1730611800));
    assert(candidates.values[1] == INT64_C(1730615400));
    assert(!qjs_timezone_transition(zone, INT64_C(1710054000), 1, 1, &boundary, &found));
    assert(found && boundary == INT64_C(1710054000));
    assert(!qjs_timezone_transition(zone, boundary, 1, 0, &boundary, &found));
    assert(found && boundary == INT64_C(1730613600));
    assert(!qjs_timezone_info_is_constant(zone, INT64_C(1710053999), INT64_C(1710054000), &constant));
    assert(!constant);
    assert(!qjs_timezone_info(zone, INT64_C(1710054000), &info));
    assert(info.offset_seconds == -14400 && info.is_dst);
    boundary = 123;
    assert(qjs_timezone_candidates(zone, INT64_MIN, collect, &candidates,
                                    &count, &before, &after, &boundary) == QJS_TZ_RANGE);
    assert(boundary == 123);

    assert(!qjs_timezone_lookup(&zone, "Pacific/Apia", 12));
    candidates.count = 0;
    assert(!qjs_timezone_candidates(zone, INT64_C(1325246400), collect, &candidates,
                                     &count, &before, &after, &boundary));
    assert(count == 0 && before == -36000 && after == 50400);
    assert(boundary == INT64_C(1325239200));

    /* ICU supplies CLDR's territory-specific Windows mapping, including 001
       fallback, without linking the native backend's generated tables. */
    assert(!qjs_tz_windows_lookup_identifier("Pacific Standard Time", "US", windows, sizeof(windows)));
    assert(!strcmp(windows, "America/Los_Angeles"));
    assert(!qjs_tz_windows_lookup_identifier("Eastern Standard Time", "CA", windows, sizeof(windows)));
    assert(!strcmp(windows, "America/Toronto"));
    return 0;
}
