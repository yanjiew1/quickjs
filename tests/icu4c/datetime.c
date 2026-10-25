/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "helpers.h"
#include "../../src/intl/datetime/datetime.h"
#include "../../src/calendar/iso.h"
#include <stdio.h>

static void date_text(const QJSIntlAllocator *allocator, const QJSIntlDateTimeFormatter *formatter,
                      QJSIntlDateValueKind kind, int64_t seconds, uint32_t nanos,
                      const char *expected)
{
    QJSIntlFormatted result;
    size_t i, end = 0;

    assert(!qjs_intl_datetime_format(formatter, kind, seconds, nanos, &result));
    assert(result.length == strlen(expected));
    for (i = 0; i < result.length; i++) {
        if (result.text[i] != (unsigned char)expected[i]) {
            fprintf(stderr, "date mismatch at %zu: U+%04x != %02x in %s\n", i, result.text[i],
                    (unsigned char)expected[i], expected);
            abort();
        }
    }
    for (i = 0; i < result.part_count; i++) {
        assert(result.parts[i].start == end);
        end = result.parts[i].end;
    }
    assert(end == result.length);
    qjs_intl_formatted_clear(allocator, &result);
}

static void test_datetime(const QJSIntlAllocator *allocator)
{
    QJSIntlDateTimeService *service;
    QJSIntlDateTimeFormatter *formatter;
    QJSIntlDateTimeOptions options = {.hour_cycle = QJS_INTL_DATE_H23,
                                      .required = QJS_INTL_DATE_REQUIRE_ANY,
                                      .defaults = QJS_INTL_DATE_DEFAULT_DATE,
                                      .best_fit = 1,
                                      .maximum_output_length = 4096};
    const QJSIntlBytes *locales;
    QJSTimeZoneIdentifier zone = {
        .identifier = "+02:00", .offset_nanoseconds = INT64_C(7200000000000), .is_offset = 1};
    QJSIntlDateTimeComponents components;
    QJSIntlBytes calendar, system;
    QJSIntlDateHourPreferences hours;
    const char *identifier;
    size_t count, en, i, calls;
    Allocation *state = allocator->opaque;
    QJSIntlFormatted result;
    QJSIntlFormattedRange range;

    assert(!qjs_intl_datetime_open(allocator, NULL, &service));
    assert(!qjs_intl_datetime_available(service, &locales, &count) && count > 100);
    en = find(locales, count, "en-GB");
    assert(!qjs_intl_datetime_hour_preferences(service, en, &hours));
    assert(hours.preferred == QJS_INTL_DATE_H23);
    assert(!qjs_intl_datetime_default_calendar(service, en, &calendar));
    assert(calendar.length == 7 && !memcmp(calendar.data, "gregory", 7));
    assert(!qjs_intl_datetime_default_system(service, en, &system));
    /* Basic matching must survive empty source-skeleton rows and include
       complete date/clock candidates rather than silently losing time fields. */
    options.best_fit = 0;
    options.components.values[QJS_INTL_DATETIME_YEAR] = QJS_INTL_DATETIME_NUMERIC;
    options.components.values[QJS_INTL_DATETIME_MONTH] = QJS_INTL_DATETIME_NUMERIC;
    options.components.values[QJS_INTL_DATETIME_DAY] = QJS_INTL_DATETIME_NUMERIC;
    options.components.values[QJS_INTL_DATETIME_HOUR] = QJS_INTL_DATETIME_NUMERIC;
    options.components.values[QJS_INTL_DATETIME_MINUTE] = QJS_INTL_DATETIME_NUMERIC;
    options.components.values[QJS_INTL_DATETIME_SECOND] = QJS_INTL_DATETIME_NUMERIC;
    assert(!qjs_intl_datetime_formatter_open(service, en, calendar, system, &zone, &options,
                                             &formatter));
    assert(
        !qjs_intl_datetime_formatter_info(formatter, &calendar, &system, &identifier, &components));
    for (i = QJS_INTL_DATETIME_YEAR; i <= QJS_INTL_DATETIME_SECOND; i++)
        if (i != QJS_INTL_DATETIME_DAY_PERIOD)
            assert(components.values[i]);
    qjs_intl_datetime_formatter_close(formatter);
    assert(!qjs_intl_datetime_default_calendar(service, en, &calendar));
    assert(!qjs_intl_datetime_default_system(service, en, &system));
    options.best_fit = 1;
    options.components = (QJSIntlDateTimeComponents){{0}};
    options.components.values[QJS_INTL_DATETIME_HOUR] = QJS_INTL_DATETIME_TWO_DIGIT;
    options.components.values[QJS_INTL_DATETIME_MINUTE] = QJS_INTL_DATETIME_TWO_DIGIT;
    assert(!qjs_intl_datetime_formatter_open(service, en, calendar, system, &zone, &options,
                                             &formatter));
    assert(
        !qjs_intl_datetime_formatter_info(formatter, &calendar, &system, &identifier, &components));
    assert(!strcmp(identifier, "+02:00"));
    date_text(allocator, formatter, QJS_INTL_DATE_VALUE_DATE, 0, 0, "02:00");
    date_text(allocator, formatter, QJS_INTL_DATE_VALUE_PLAIN_TIME, 0, 0, "00:00");
    assert(qjs_intl_datetime_check_value_format(formatter, QJS_INTL_DATE_VALUE_PLAIN_DATE) ==
           QJS_INTL_UNSUPPORTED);
    assert(!qjs_intl_datetime_formatter_prepare_ranges(formatter));
    assert(!qjs_intl_datetime_format_range(formatter, QJS_INTL_DATE_VALUE_PLAIN_TIME, 0, 0, 3600, 0,
                                           &range));
    assert(range.length && range.part_count);
    qjs_intl_formatted_range_clear(allocator, &range);
    qjs_intl_datetime_formatter_close(formatter);

    /* Fractional precision controls interval comparison, including negative
       epochs. ICU itself compares hidden milliseconds unless normalized. */
    options.components = (QJSIntlDateTimeComponents){{0}};
    options.components.values[QJS_INTL_DATETIME_MINUTE] = QJS_INTL_DATETIME_TWO_DIGIT;
    options.components.values[QJS_INTL_DATETIME_SECOND] = QJS_INTL_DATETIME_TWO_DIGIT;
    options.components.values[QJS_INTL_DATETIME_FRACTION] = 1;
    assert(!qjs_intl_datetime_formatter_open(service, en, bytes("gregory"), bytes("latn"), &zone,
                                             &options, &formatter));
    qjs_intl_datetime_close(service);
    date_text(allocator, formatter, QJS_INTL_DATE_VALUE_PLAIN_TIME, 123, 100000000, "02:03.1");
    assert(!qjs_intl_datetime_formatter_prepare_ranges(formatter));
    assert(!qjs_intl_datetime_format_range(formatter, QJS_INTL_DATE_VALUE_PLAIN_TIME, 123,
                                           100000000, 123, 199000000, &range));
    for (i = 0; i < range.part_count; i++)
        assert(range.parts[i].source == QJS_INTL_RANGE_SHARED);
    qjs_intl_formatted_range_clear(allocator, &range);
    state->calls = 0;
    assert(!qjs_intl_datetime_format(formatter, QJS_INTL_DATE_VALUE_PLAIN_TIME, -1, 123456789,
                                     &result));
    calls = state->calls;
    qjs_intl_formatted_clear(allocator, &result);
    for (i = 1; i <= calls; i++) {
        size_t baseline = state->live;

        state->calls = 0;
        state->fail_at = i;
        assert(qjs_intl_datetime_format(formatter, QJS_INTL_DATE_VALUE_PLAIN_TIME, -1, 123456789,
                                        &result) == QJS_INTL_NO_MEMORY);
        assert(!result.text && !result.parts && state->live == baseline);
    }
    state->fail_at = 0;
    qjs_intl_datetime_formatter_close(formatter);

    assert(!qjs_intl_datetime_open(allocator, NULL, &service));
    assert(!qjs_intl_datetime_available(service, &locales, &count));
    options.components = (QJSIntlDateTimeComponents){{0}};
    options.components.values[QJS_INTL_DATETIME_YEAR] = QJS_INTL_DATETIME_NUMERIC;
    options.components.values[QJS_INTL_DATETIME_MONTH] = QJS_INTL_DATETIME_TWO_DIGIT;
    options.components.values[QJS_INTL_DATETIME_DAY] = QJS_INTL_DATETIME_TWO_DIGIT;
    options.components.values[QJS_INTL_DATETIME_HOUR] = QJS_INTL_DATETIME_TWO_DIGIT;
    options.components.values[QJS_INTL_DATETIME_MINUTE] = QJS_INTL_DATETIME_TWO_DIGIT;
    options.components.values[QJS_INTL_DATETIME_SECOND] = QJS_INTL_DATETIME_TWO_DIGIT;
    assert(!qjs_intl_datetime_formatter_open(service, find(locales, count, "en-GB"),
                                             bytes("gregory"), bytes("latn"), &zone, &options,
                                             &formatter));
    {
        int64_t day;

        /* Temporal plain domains include dates beyond Date's TimeClip limits. */
        assert(!qjs_iso_date_to_days(&day, (QJSISODate){-271821, 4, 19}));
        date_text(allocator, formatter, QJS_INTL_DATE_VALUE_PLAIN_DATE, day * 86400, 0,
                  "19/04/271822");
        assert(!qjs_iso_date_to_days(&day, (QJSISODate){-271821, 4, 1}));
        assert(!qjs_intl_datetime_format(formatter, QJS_INTL_DATE_VALUE_PLAIN_YEAR_MONTH,
                                         day * 86400, 0, &result));
        qjs_intl_formatted_clear(allocator, &result);
        assert(!qjs_iso_date_to_days(&day, (QJSISODate){275760, 9, 13}));
        date_text(allocator, formatter, QJS_INTL_DATE_VALUE_PLAIN_DATE_TIME, day * 86400 + 86399,
                  999999999, "13/09/275760, 23:59:59");
    }
    qjs_intl_datetime_formatter_close(formatter);

    /* AdjustDateTimeStyleFormat keeps an unchanged sufficient style instead
       of regenerating its skeleton and losing the source punctuation. */
    options.components = (QJSIntlDateTimeComponents){{0}};
    options.date_style = QJS_INTL_DATE_STYLE_MEDIUM + 1;
    assert(!qjs_intl_datetime_formatter_open(service, find(locales, count, "agq"), bytes("gregory"),
                                             bytes("latn"), &zone, &options, &formatter));
    {
        QJSIntlFormatted ordinary, plain;

        assert(!qjs_intl_datetime_format(formatter, QJS_INTL_DATE_VALUE_DATE, 1704067200, 0,
                                         &ordinary));
        assert(!qjs_intl_datetime_format(formatter, QJS_INTL_DATE_VALUE_PLAIN_DATE, 1704067200, 0,
                                         &plain));
        assert(ordinary.length == plain.length &&
               !memcmp(ordinary.text, plain.text, plain.length * sizeof(*plain.text)));
        qjs_intl_formatted_clear(allocator, &ordinary);
        qjs_intl_formatted_clear(allocator, &plain);
    }
    qjs_intl_datetime_formatter_close(formatter);
    qjs_intl_datetime_close(service);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);

    test_datetime(&allocator);
    assert(!state.live);
    return 0;
}
