/*
 * Native Temporal support tests
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
#include <assert.h>
#include <stdint.h>
#include "../src/temporal/epoch.h"
#include "../src/temporal/iso.h"

static void assert_ns(QJSTemporalEpochNs actual, QJSTemporalEpochNs expected)
{
    assert(qjs_temporal_epoch_ns_compare(actual, expected) == 0);
}

static void assert_date(QJSTemporalISODate actual, QJSTemporalISODate expected)
{
    assert(actual.year == expected.year && actual.month == expected.month &&
           actual.day == expected.day);
}

static void test_gregorian_dates(void)
{
    static const struct {
        QJSTemporalISODate date;
        int64_t days;
    } cases[] = {
        { { 1970, 1, 1 }, 0 },
        { { 1969, 12, 31 }, -1 },
        { { 2000, 2, 29 }, 11016 },
        { { 1900, 3, 1 }, -25508 },
        { { 0, 1, 1 }, -719528 },
        { { -1, 12, 31 }, -719529 },
        { { -400, 1, 1 }, -865625 },
        { { -271821, 4, 20 }, -100000000 },
        { { 275760, 9, 13 }, 100000000 },
    };
    QJSTemporalISODate date, expected;
    int64_t days;
    unsigned i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        assert(qjs_iso_date_to_days(&days, cases[i].date) == 0);
        assert(days == cases[i].days);
        assert(qjs_iso_date_from_days(&date, days) == 0);
        assert_date(date, cases[i].date);
    }
    expected.year = INT32_MIN;
    expected.month = 1;
    expected.day = 1;
    assert(qjs_iso_date_to_days(&days, expected) == 0);
    assert(qjs_iso_date_from_days(&date, days) == 0);
    assert_date(date, expected);
    assert(qjs_iso_date_from_days(&date, days - 1) == -1);
    assert_date(date, expected);
    expected.year = INT32_MAX;
    expected.month = 12;
    expected.day = 31;
    assert(qjs_iso_date_to_days(&days, expected) == 0);
    assert(qjs_iso_date_from_days(&date, days) == 0);
    assert_date(date, expected);
    assert(qjs_iso_date_from_days(&date, days + 1) == -1);
    assert_date(date, expected);
    expected.year = 1900;
    expected.month = 2;
    expected.day = 29;
    days = 123;
    assert(qjs_iso_date_to_days(&days, expected) == -1);
    assert(days == 123);
    expected.year = 2000;
    assert(qjs_iso_date_is_valid(expected));
    expected.year = -100;
    assert(!qjs_iso_date_is_valid(expected));
    expected.year = -400;
    assert(qjs_iso_date_is_valid(expected));
    expected.month = 0;
    assert(!qjs_iso_date_is_valid(expected));
    expected.month = 13;
    assert(!qjs_iso_date_is_valid(expected));
    expected.month = 4;
    expected.day = 31;
    assert(!qjs_iso_date_is_valid(expected));
    date = (QJSTemporalISODate){ 1970, 1, 1 };
    assert(qjs_iso_date_from_days(&date, INT64_MIN) == -1);
    assert_date(date, (QJSTemporalISODate){ 1970, 1, 1 });
    assert(qjs_iso_date_from_days(&date, INT64_MAX) == -1);
    assert_date(date, (QJSTemporalISODate){ 1970, 1, 1 });
}

static void test_civil_epoch_conversion(void)
{
    QJSTemporalISODateTime datetime = {
        { 1969, 12, 31 }, { 23, 59, 59, 999, 999, 999 },
    };
    QJSTemporalISODateTime converted;
    QJSTemporalEpochNs value;
    int64_t days, expected_days = -865625;
    int year, month, day;

    assert(qjs_temporal_iso_datetime_to_epoch_ns(&value, datetime) == 0);
    assert_ns(value, qjs_temporal_epoch_ns_from_int64(-1));
    assert(qjs_temporal_iso_datetime_from_epoch_ns(&converted, value) == 0);
    assert_date(converted.date, datetime.date);
    assert(converted.time.hour == 23 && converted.time.minute == 59 &&
           converted.time.second == 59 && converted.time.millisecond == 999 &&
           converted.time.microsecond == 999 && converted.time.nanosecond == 999);
    /* Check every date in one complete Gregorian cycle, including year
       zero and negative centuries, independently of the estimate formula. */
    for (year = -400; year < 0; year++) {
        for (month = 1; month <= 12; month++) {
            for (day = 1; day <= 31; day++) {
                datetime.date.year = year;
                datetime.date.month = month;
                datetime.date.day = day;
                if (!qjs_iso_date_is_valid(datetime.date))
                    continue;
                assert(qjs_iso_date_to_days(&days, datetime.date) == 0);
                assert(days == expected_days++);
                assert(qjs_temporal_iso_datetime_to_epoch_ns(&value, datetime) == 0);
                assert(qjs_temporal_iso_datetime_from_epoch_ns(&converted, value) == 0);
                assert_date(converted.date, datetime.date);
                assert(converted.time.nanosecond == 999);
            }
        }
    }
    assert(expected_days == -719528);
    datetime.date.year = 1970;
    datetime.date.month = 1;
    datetime.date.day = 1;
    datetime.time.hour = 24;
    value.low = 123;
    value.high = 456;
    assert(qjs_temporal_iso_datetime_to_epoch_ns(&value, datetime) == -1);
    assert(value.low == 123 && value.high == 456);
}

int main(void)
{
    test_gregorian_dates();
    test_civil_epoch_conversion();
    return 0;
}
