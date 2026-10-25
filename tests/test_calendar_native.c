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

/* Native conversion, leap-month and bounded month-addition contracts. */
#include "../src/calendar/calendar.h"
#include "../src/calendar/data.h"
#include "../src/calendar/iso.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void roundtrip(QJSCalendarId calendar, int year, int month, int day)
{
    QJSCalendarDate date;
    int64_t epoch;

    assert(!qjs_calendar_to_epoch_day(calendar, year, month, day, &epoch));
    assert(!qjs_calendar_from_epoch_day(calendar, epoch, &date));
    assert(date.year == year && date.month == month && date.day == day);
    assert(date.day_of_year >= day && date.days_in_month >= day);
}

static void check_year(QJSCalendarId calendar, int year)
{
    int months, days, month, ordinal, sum = 0;
    char code[5];
    int64_t first, next;
    QJSCalendarDate date;

    assert(!qjs_calendar_month_info(calendar, year, 1, &months, &days, code));
    assert(months == 12 || months == 13);
    assert(!qjs_calendar_to_epoch_day(calendar, year, 1, 1, &first));
    for (month = 1; month <= months; month++) {
        assert(!qjs_calendar_month_info(calendar, year, month, &months, &days, code));
        assert(!qjs_calendar_month_ordinal(calendar, year, code, 0, &ordinal));
        assert(ordinal == month);
        roundtrip(calendar, year, month, 1);
        roundtrip(calendar, year, month, days);
        sum += days;
    }
    assert(!qjs_calendar_from_epoch_day(calendar, first, &date));
    assert(sum == date.days_in_year && date.months_in_year == months);
    if (year < QJS_CAL_MAX_YEAR) {
        assert(!qjs_calendar_to_epoch_day(calendar, year + 1, 1, 1, &next));
        assert(next - first == sum);
    }
}

static void publication_months(QJSCalendarId calendar, int first, int last)
{
    int year;

    for (year = first; year <= last; year++) {
        QJSCalendarYear packed;
        QJSCalendarDate date;
        int64_t epoch;
        int month;

        assert(!qjs_calendar_data_year(&qjs_native_calendar_data, calendar, year, &packed));
        epoch = packed.first_day;
        for (month = 1; month <= packed.months; month++) {
            int32_t next_year, old_year;
            int next_month, old_month;
            int64_t actual;

            assert(!qjs_calendar_to_epoch_day(calendar, year, month, 1, &actual));
            assert(actual == epoch);
            assert(!qjs_calendar_from_epoch_day(calendar, epoch, &date));
            assert(date.year == year && date.month == month && date.day == 1);
            assert(!qjs_calendar_from_epoch_day(calendar, epoch + date.days_in_month - 1, &date));
            assert(date.year == year && date.month == month && date.day == date.days_in_month);
            if (calendar == QJS_CAL_CHINESE || calendar == QJS_CAL_DANGI) {
                assert(!qjs_calendar_add_months(calendar, year, month, 1, &next_year, &next_month));
                assert(!qjs_calendar_add_months(calendar, next_year, next_month, -1, &old_year, &old_month));
                assert(old_year == year && old_month == month);
            }
            epoch += date.days_in_month;
        }
        assert(epoch == packed.end_day);
    }
}

static void month_addition(QJSCalendarId calendar, int year, int month, int64_t delta)
{
    int32_t target_year, restored_year;
    int target_month, restored_month;
    int error = qjs_calendar_add_months(calendar, year, month, delta,
                                       &target_year, &target_month);

    if (error)
        fprintf(stderr, "calendar %d year %d month %d delta %lld: %d\n",
                calendar, year, month, (long long)delta, error);
    assert(!error);
    assert(!qjs_calendar_add_months(calendar, target_year, target_month, -delta,
                                   &restored_year, &restored_month));
    assert(restored_year == year && restored_month == month);
}

/* Enumeration exposes canonical backend identities, not localized names or
   all lenient input aliases. Its borrowed result survives subsequent queries. */
static void check_era_domain(QJSCalendarId calendar)
{
    const char *name = "unchanged", *previous;
    QJSCalendarDate date;
    size_t count, i, limit = 2;
    int error, found = 0;

    if (calendar == QJS_CAL_JAPANESE)
        limit += qjs_calendar_data_japanese_era_count(&qjs_native_calendar_data);
    assert(!qjs_calendar_from_epoch_day(calendar, 0, &date));
    for (count = 0; count <= limit; count++) {
        previous = name;
        error = qjs_calendar_era_at(calendar, count, &name);
        if (error) {
            assert(error == QJS_CAL_UNSUPPORTED && name == previous);
            break;
        }
        assert(name && *name);
        if (!strcmp(name, date.era))
            found = 1;
        for (i = 0; i < count; i++) {
            const char *other;

            assert(!qjs_calendar_era_at(calendar, i, &other));
            assert(strcmp(name, other));
        }
    }
    assert(count <= limit);
    assert(found == date.has_era);
    previous = name;
    assert(qjs_calendar_era_at(calendar, SIZE_MAX, &name) == QJS_CAL_UNSUPPORTED);
    assert(name == previous);
}

int main(void)
{
    static const int years[] = {-271000, -10001, -1, 0, 1, 1888, 1898, 1899,
                                 1900, 2023, 2024, 2050, 2051, 2100, 2101, 275000};
    QJSCalendarDate date;
    int calendar, months, days, ordinal;
    int32_t year;
    int64_t epoch = 123;
    size_t i;
    char code[5];

    for (calendar = 0; calendar < QJS_CAL_COUNT; calendar++) {
        QJSCalendarId id;

        assert(qjs_calendar_is_supported((QJSCalendarId)calendar));
        check_era_domain((QJSCalendarId)calendar);
        assert(!qjs_calendar_from_identifier(&id,
                qjs_calendar_identifier((QJSCalendarId)calendar),
                strlen(qjs_calendar_identifier((QJSCalendarId)calendar))));
        assert(id == calendar);
        for (i = 0; i < sizeof(years) / sizeof(years[0]); i++)
            check_year(id, years[i]);
        year = 123;
        months = 456;
        assert(qjs_calendar_add_months(id, 2024, 1, INT64_MAX, &year, &months) == QJS_CAL_RANGE);
        assert(year == 123 && months == 456);
        assert(qjs_calendar_add_months(id, 2024, 1, INT64_MIN, &year, &months) == QJS_CAL_RANGE);
        assert(year == 123 && months == 456);
        assert(qjs_calendar_add_months(id, QJS_CAL_MIN_YEAR, 1, -1, &year, &months) == QJS_CAL_RANGE);
        assert(year == 123 && months == 456);
        assert(!qjs_calendar_month_info(id, QJS_CAL_MAX_YEAR, 1,
                                        &ordinal, &days, code));
        assert(qjs_calendar_add_months(id, QJS_CAL_MAX_YEAR, ordinal, 1,
                                       &year, &months) == QJS_CAL_RANGE);
        assert(year == 123 && months == 456);
        assert(qjs_calendar_to_epoch_day(id, 2024, 1, 0, &epoch) == QJS_CAL_RANGE);
        assert(epoch == 123);
    }
    publication_months(QJS_CAL_CHINESE, 1899, 2100);
    publication_months(QJS_CAL_DANGI, 1899, 2100);
    publication_months(QJS_CAL_PERSIAN, 1206, 1498);
    publication_months(QJS_CAL_ISLAMIC_UMALQURA, 1300, 1600);
    assert(!qjs_calendar_month_ordinal(QJS_CAL_HEBREW, 5784, "M05L", 0, &ordinal) && ordinal == 6);
    assert(qjs_calendar_month_ordinal(QJS_CAL_HEBREW, 5783, "M05L", 0, &ordinal) == QJS_CAL_RANGE);
    assert(!qjs_calendar_month_ordinal(QJS_CAL_HEBREW, 5783, "M05L", 1, &ordinal) && ordinal == 6);
    assert(!qjs_calendar_month_info(QJS_CAL_HEBREW, 5784, 7, &months, &days, code));
    assert(!strcmp(code, "M06"));
    assert(!qjs_calendar_month_ordinal(QJS_CAL_CHINESE, 2023, "M02L", 0, &ordinal) && ordinal == 3);
    assert(!qjs_calendar_month_ordinal(QJS_CAL_CHINESE, 2024, "M02L", 1, &ordinal) && ordinal == 2);
    assert(qjs_calendar_month_ordinal(QJS_CAL_CHINESE, 2024, "M02L", 0, &ordinal) == QJS_CAL_RANGE);
    assert(qjs_calendar_month_ordinal(QJS_CAL_HEBREW, 5784, "M13", 1, &ordinal) == QJS_CAL_RANGE);
    assert(qjs_calendar_month_ordinal(QJS_CAL_GREGORY, 2024, "M01L", 1, &ordinal) == QJS_CAL_RANGE);
    assert(!qjs_calendar_to_epoch_day(QJS_CAL_PERSIAN, 1404, 1, 1, &epoch));
    assert(!qjs_calendar_from_epoch_day(QJS_CAL_ISO8601, epoch, &date));
    assert(date.year == 2025 && date.month == 3 && date.day == 21);
    month_addition(QJS_CAL_HEBREW, -1000, 6, 40000);
    month_addition(QJS_CAL_COPTIC, 2024, 13, -100000);
    for (calendar = QJS_CAL_CHINESE; calendar <= QJS_CAL_DANGI;
         calendar += QJS_CAL_DANGI - QJS_CAL_CHINESE) {
        month_addition((QJSCalendarId)calendar, 2024, 1, 1000000);
        month_addition((QJSCalendarId)calendar, -100000, 1, -1000000);
        month_addition((QJSCalendarId)calendar, -1, 12, 1);
        month_addition((QJSCalendarId)calendar, 1898, 12, 30);
        month_addition((QJSCalendarId)calendar, 2050, 10, 50);
        month_addition((QJSCalendarId)calendar, 2100, 10, 50);
        month_addition((QJSCalendarId)calendar, QJS_CAL_MAX_YEAR, 11, 1);
        month_addition((QJSCalendarId)calendar, QJS_CAL_MIN_YEAR, 2, -1);
    }
    return 0;
}
