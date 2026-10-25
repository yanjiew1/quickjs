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

/* ISO ownership remains in core with an empty extension backend. */
#include "../src/calendar/calendar.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

int main(void)
{
    QJSCalendarDate date;
    QJSCalendarId id = QJS_CAL_ISO8601;
    int64_t day = 123;
    int32_t year = 456;
    int months = 78, days = 90, ordinal = 123;
    char code[5] = "test";

    assert(qjs_calendar_is_supported(QJS_CAL_ISO8601));
    assert(!qjs_calendar_is_supported(QJS_CAL_GREGORY));
    assert(!qjs_calendar_is_supported((QJSCalendarId)-1));
    assert(!strcmp(qjs_calendar_identifier(QJS_CAL_GREGORY), "gregory"));
    assert(!qjs_calendar_identifier(QJS_CAL_COUNT));
    assert(!qjs_calendar_from_identifier(&id, "IsO8601", 7));
    assert(qjs_calendar_from_identifier(&id, "gregory", 7) == QJS_CAL_UNSUPPORTED);
    assert(id == QJS_CAL_ISO8601);
    assert(qjs_calendar_from_identifier(&id, "islamic", 7) == QJS_CAL_RANGE);
    assert(!qjs_calendar_from_epoch_day(id, 0, &date));
    assert(date.year == 1970 && date.month == 1 && date.day == 1);
    assert(!date.has_era && !date.in_leap_year && date.days_in_year == 365);
    assert(!strcmp(date.month_code, "M01"));
    assert(!qjs_calendar_to_epoch_day(id, 1970, 1, 1, &day) && !day);
    assert(!qjs_calendar_month_info(id, 0, 2, &months, &days, code));
    assert(months == 12 && days == 29 && !strcmp(code, "M02"));
    assert(!qjs_calendar_month_ordinal(id, 2024, "M12", 0, &ordinal) && ordinal == 12);
    assert(qjs_calendar_month_ordinal(id, 2024, "M02L", 1, &ordinal) == QJS_CAL_RANGE);
    assert(ordinal == 12);
    assert(!qjs_calendar_add_months(id, 0, 1, -1, &year, &months));
    assert(year == -1 && months == 12);
    assert(qjs_calendar_add_months(id, 0, 1, INT64_MIN, &year, &months) == QJS_CAL_RANGE);
    assert(year == -1 && months == 12);
    assert(qjs_calendar_add_months(id, 2024, 12, INT64_MAX, &year, &months) == QJS_CAL_RANGE);
    assert(year == -1 && months == 12);
    assert(qjs_calendar_year_from_era(id, "ce", 2024, &year) == QJS_CAL_RANGE);
    assert(year == -1);
    day = 123;
    assert(qjs_calendar_to_epoch_day(id, 2023, 2, 29, &day) == QJS_CAL_RANGE);
    assert(day == 123);
    assert(qjs_calendar_to_epoch_day(QJS_CAL_GREGORY, 2024, 1, 1, &day) == QJS_CAL_UNSUPPORTED);
    assert(day == 123);
    /* A consumer can obtain metadata outside Temporal's final date limits. */
    assert(!qjs_calendar_to_epoch_day(id, 500000, 1, 1, &day));
    assert(!qjs_calendar_from_epoch_day(id, day, &date) && date.year == 500000);
    assert(qjs_calendar_to_epoch_day(id, INT32_MIN, 1, 1, &day) == QJS_CAL_RANGE);
    {
        unsigned char saved[sizeof(date)];

        memset(&date, 0x5a, sizeof(date));
        memcpy(saved, &date, sizeof(date));
        assert(qjs_calendar_from_epoch_day(id, INT64_MIN, &date) == QJS_CAL_RANGE);
        assert(!memcmp(saved, &date, sizeof(date)));
        assert(qjs_calendar_from_epoch_day(id, INT64_MAX, &date) == QJS_CAL_RANGE);
        assert(!memcmp(saved, &date, sizeof(date)));
        months = 3;
        days = 4;
        memcpy(code, "keep", 5);
        assert(qjs_calendar_month_info(id, 2024, 13, &months, &days, code) == QJS_CAL_RANGE);
        assert(months == 3 && days == 4 && !strcmp(code, "keep"));
        assert(qjs_calendar_month_info(QJS_CAL_GREGORY, 2024, 1, &months, &days, code) == QJS_CAL_UNSUPPORTED);
        assert(months == 3 && days == 4 && !strcmp(code, "keep"));
    }
    for (int bound = QJS_CAL_MIN_YEAR; ; bound = QJS_CAL_MAX_YEAR) {
        assert(!qjs_calendar_to_epoch_day(id, bound, 12, 31, &day));
        assert(!qjs_calendar_from_epoch_day(id, day, &date));
        assert(date.year == bound && date.month == 12 && date.day == 31);
        if (bound == QJS_CAL_MAX_YEAR)
            break;
    }
    assert(qjs_calendar_to_epoch_day(id, QJS_CAL_MAX_YEAR + 1, 1, 1, &day) == QJS_CAL_RANGE);
    assert(qjs_calendar_to_epoch_day(id, QJS_CAL_MIN_YEAR - 1, 1, 1, &day) == QJS_CAL_RANGE);
    assert(qjs_calendar_month_ordinal(id, 2024, "M00", 0, &ordinal) == QJS_CAL_RANGE);
    assert(qjs_calendar_month_ordinal(id, 2024, "M99", 0, &ordinal) == QJS_CAL_RANGE);
    assert(qjs_calendar_month_ordinal(id, 2024, "M13L", 1, &ordinal) == QJS_CAL_RANGE);
    assert(ordinal == 12);
    {
        const char *name = "unchanged";

        assert(qjs_calendar_era_at(QJS_CAL_ISO8601, 0, &name) == QJS_CAL_UNSUPPORTED);
        assert(!strcmp(name, "unchanged"));
        assert(qjs_calendar_era_at(QJS_CAL_GREGORY, 0, &name) == QJS_CAL_UNSUPPORTED);
        assert(!strcmp(name, "unchanged"));
        assert(qjs_calendar_era_at(QJS_CAL_COUNT, 0, &name) == QJS_CAL_RANGE);
        assert(!strcmp(name, "unchanged"));
        assert(qjs_calendar_era_at(QJS_CAL_ISO8601, 0, NULL) == QJS_CAL_RANGE);
    }
    return 0;
}
