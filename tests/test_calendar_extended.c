/* Extended calendar storage-year, month-code and error-contract regressions.
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
#include "../src/calendar/calendar.h"
#include "../src/calendar/data.h"
#include "../src/calendar/iso.h"
#ifdef CONFIG_CALENDAR_ICU4X
#include "../src/icu4x/calendar.h"
#endif
#include <assert.h>
#include <limits.h>
#include <string.h>

static void official_years(void)
{
    QJSCalendarId id;

    /* Check complete month layouts, their adjacent-year seams and both ends
       of every month. This catches wrong leap labels as well as wrong dates. */
    for (id = QJS_CAL_CHINESE; id <= QJS_CAL_DANGI; id += 2) {
        int last = id == QJS_CAL_CHINESE ? 2100 : 2050;
        int year;

        for (year = 1900; year <= last; year++) {
            QJSCalendarYear packed;
            int64_t day, actual;
            int month;

            assert(!qjs_calendar_official_year(id, year, &packed));
#ifdef CONFIG_CALENDAR_ICU4X
            /* The pinned ICU4X month sequence has the same number of months
               in each official year, including its 2057 date-length conflict.
               Public month arithmetic therefore retains the corrected ordinal
               sequence across the entire publication span. */
            {
                QJSCalendarYearLayout foreign;
                assert(!qjs_icu4x_calendar_year(id, year, &foreign));
                assert(foreign.months == packed.months);
            }
#endif
            day = packed.first_day;
            for (month = 1; month <= packed.months; month++) {
                int length = 29 + ((packed.month_lengths >> (2 * (month - 1))) & 3);
                int end;

                for (end = 0; end < 2; end++) {
                    QJSCalendarDate value;
                    int date = end ? length : 1;

                    assert(!qjs_calendar_to_epoch_day(id, year, month, date, &actual));
                    assert(actual == day + date - 1);
                    assert(!qjs_calendar_from_epoch_day(id, actual, &value));
                    assert(value.year == year && value.month == month && value.day == date);
                    assert(value.days_in_year == packed.end_day - packed.first_day);
                    assert((value.month_code[3] == 'L') ==
                           (packed.leap_month && month == packed.leap_month + 1));
                }
                day += length;
            }
            assert(day == packed.end_day);
            assert(!qjs_calendar_to_epoch_day(id, year + 1, 1, 1, &actual));
            assert(actual == day);
            if (year == 1900) {
                int count, length;
                char code[5];

                assert(!qjs_calendar_month_info(id, 1899, 1, &count, &length, code));
                assert(!qjs_calendar_month_info(id, 1899, count, &count, &length, code));
                assert(!qjs_calendar_to_epoch_day(id, 1899, count, length, &actual));
                assert(actual + 1 == packed.first_day);
            }
        }
    }
}

int main(void)
{
    static const int32_t years[QJS_CAL_COUNT] = {
        2024, 2567, 2024, 1740, 2024, 7516, 2016, 2024,
        5784, 1945, 1445, 1445, 1445, 2024, 1402, 113,
    };
    static const char *const japanese[] = {"meiji", "taisho", "showa", "heisei", "reiwa"};
    QJSCalendarDate date, preserved;
    int64_t epoch, roundtrip;
    int32_t year;
    int month, count, days;
    char code[5];
    size_t i;
    QJSCalendarId id;

    official_years();

    assert(!qjs_iso_date_to_days(&epoch, (QJSISODate){2024, 2, 10}));
    for (id = QJS_CAL_ISO8601; id < QJS_CAL_COUNT; id++) {
        assert(qjs_calendar_is_supported(id));
        assert(!qjs_calendar_from_epoch_day(id, epoch, &date));
        assert(date.year == years[id]);
        assert(!qjs_calendar_to_epoch_day(id, date.year, date.month, date.day, &roundtrip));
        assert(roundtrip == epoch);
        assert(!qjs_calendar_month_info(id, date.year, date.month, &count, &days, code));
        assert(count == date.months_in_year && days == date.days_in_month);
        assert(!strcmp(code, date.month_code));
        assert(!qjs_calendar_add_months(id, date.year, date.month, 1, &year, &month));
        assert(!qjs_calendar_add_months(id, year, month, -1, &year, &month));
        assert(year == date.year && month == date.month);
    }
    /* Check every calendar near both Temporal endpoints, including calendars
       whose arithmetic years differ from the underlying ISO year. */
    for (id = QJS_CAL_ISO8601; id < QJS_CAL_COUNT; id++) {
        static const int64_t extremes[] = {-99999900, 99999900};

        for (i = 0; i < sizeof(extremes) / sizeof(extremes[0]); i++) {
            assert(!qjs_calendar_from_epoch_day(id, extremes[i], &date));
            assert(!qjs_calendar_to_epoch_day(id, date.year, date.month, date.day, &roundtrip));
            assert(roundtrip == extremes[i]);
        }
    }
    assert(!qjs_calendar_month_ordinal(QJS_CAL_HEBREW, 5784, "M05L", 0, &month));
    assert(month == 6);
    assert(!qjs_calendar_month_info(QJS_CAL_HEBREW, 5784, month, &count, &days, code));
    assert(count == 13 && days == 30 && !strcmp(code, "M05L"));
    assert(qjs_calendar_month_ordinal(QJS_CAL_HEBREW, 5785, "M05L", 0, &month) == QJS_CAL_RANGE);
    assert(!qjs_calendar_month_ordinal(QJS_CAL_HEBREW, 5785, "M05L", 1, &month));
    assert(!qjs_calendar_month_info(QJS_CAL_HEBREW, 5785, month, &count, &days, code));
    assert(count == 12 && !strcmp(code, "M06"));
    assert(!qjs_calendar_month_ordinal(QJS_CAL_CHINESE, 2023, "M02L", 0, &month));
    assert(month == 3);

    for (i = 0; i < sizeof(japanese) / sizeof(japanese[0]); i++) {
        const char *name;
        assert(!qjs_calendar_era_at(QJS_CAL_JAPANESE, i, &name));
        assert(!strcmp(name, japanese[i]));
    }
    assert(!qjs_calendar_year_from_era(QJS_CAL_JAPANESE, "heisei", 32, &year));
    assert(year == 2020);
    assert(!qjs_calendar_to_epoch_day(QJS_CAL_JAPANESE, year, 1, 1, &epoch));
    assert(!qjs_calendar_from_epoch_day(QJS_CAL_JAPANESE, epoch, &date));
    assert(!strcmp(date.era, "reiwa") && date.era_year == 2);

    /* These metadata dates exceed Temporal's public interval deliberately:
       backend calendar arithmetic must not inherit Date's clipping policy. */
    const int32_t metadata_years[] = {QJS_CAL_MIN_YEAR, 0, QJS_CAL_MAX_YEAR};
    for (i = 0; i < sizeof(metadata_years) / sizeof(metadata_years[0]); i++) {
        year = metadata_years[i];
        assert(!qjs_calendar_to_epoch_day(QJS_CAL_GREGORY, year, 3, 1, &epoch));
        assert(!qjs_iso_date_to_days(&roundtrip, (QJSISODate){year, 3, 1}));
        assert(epoch == roundtrip);
        assert(!qjs_calendar_from_epoch_day(QJS_CAL_GREGORY, epoch, &date));
        assert(date.year == year && date.month == 3 && date.day == 1);
    }
    assert(!qjs_calendar_to_epoch_day(QJS_CAL_GREGORY, 1500, 3, 1, &epoch));
    assert(!qjs_iso_date_to_days(&roundtrip, (QJSISODate){1500, 3, 1}));
    assert(epoch == roundtrip); /* No 1582 Julian/Gregorian cutover. */
    for (id = QJS_CAL_CHINESE; id <= QJS_CAL_DANGI; id += 2) {
        static const int32_t distant[] = {
            QJS_CAL_MIN_YEAR, -271821, -10001, -10000, -9999,
            9999, 10000, 10001, 275760, QJS_CAL_MAX_YEAR,
        };

        for (i = 0; i < sizeof(distant) / sizeof(distant[0]); i++) {
            assert(!qjs_calendar_to_epoch_day(id, distant[i], 1, 1, &epoch));
            assert(!qjs_calendar_from_epoch_day(id, epoch, &date));
            assert(date.year == distant[i] && date.month == 1 && date.day == 1);
            assert(!strcmp(date.month_code, "M01"));
            if (distant[i] < QJS_CAL_MAX_YEAR) {
                assert(!qjs_calendar_month_info(id, distant[i], 1, &count, &days, code));
                assert(!qjs_calendar_month_info(id, distant[i], count, &count, &days, code));
                assert(!qjs_calendar_to_epoch_day(id, distant[i], count, days, &epoch));
                assert(!qjs_calendar_to_epoch_day(id, distant[i] + 1, 1, 1, &roundtrip));
                assert(roundtrip == epoch + 1);
            }
        }
    }
    /* Indian arithmetic repeats over a Gregorian 400-year cycle. Exercise
       both storage-cycle seams and wide deltas, including negative JD dates. */
    {
        static const int32_t years[] = {-271899, -400, 0, 1999, 2000, 2399, 2400};
        static const int deltas[] = {-4801, -4800, -4799, 4799, 4800, 4801};
        size_t j;

        for (i = 0; i < sizeof(years) / sizeof(years[0]); i++) {
            assert(!qjs_calendar_to_epoch_day(QJS_CAL_INDIAN, years[i], 1, 29, &epoch));
            assert(!qjs_calendar_from_epoch_day(QJS_CAL_INDIAN, epoch, &date));
            assert(date.year == years[i] && date.month == 1 && date.day == 29);
            for (j = 0; j < sizeof(deltas) / sizeof(deltas[0]); j++) {
                assert(!qjs_calendar_add_months(QJS_CAL_INDIAN, years[i], 1, deltas[j], &year, &month));
                assert((int64_t)year * 12 + month - 1 == (int64_t)years[i] * 12 + deltas[j]);
                assert(!qjs_calendar_add_months(QJS_CAL_INDIAN, year, month, -deltas[j], &year, &month));
                assert(year == years[i] && month == 1);
            }
        }
    }
    memset(&preserved, 0x5a, sizeof(preserved));
    memcpy(&date, &preserved, sizeof(date));
    assert(qjs_calendar_from_epoch_day(QJS_CAL_GREGORY, INT64_MAX, &date) == QJS_CAL_RANGE);
    assert(!memcmp(&date, &preserved, sizeof(date)));
    year = 123;
    month = 4;
    assert(qjs_calendar_add_months(QJS_CAL_CHINESE, 2024, 1, INT64_MIN, &year, &month) == QJS_CAL_RANGE);
    assert(year == 123 && month == 4);
    return 0;
}
