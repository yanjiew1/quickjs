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
#include "../../src/intl/duration/duration.h"

static QJSIntlDurationOptions duration_options(QJSIntlDurationStyle style)
{
    QJSIntlDurationOptions options = {
        .style = style, .fractional_digits = -1, .maximum_output_length = 4096};
    QJSIntlDurationUnitStyle previous = QJS_INTL_DURATION_UNIT_UNSET;
    unsigned int unit;

    for (unit = 0; unit < QJS_TEMPORAL_UNIT_COUNT; unit++) {
        assert(!qjs_intl_duration_unit_options(unit, style, QJS_INTL_DURATION_UNIT_UNSET,
                                               QJS_INTL_DURATION_DISPLAY_UNSET, previous, 0,
                                               &options.units[unit]));
        if (unit >= QJS_TEMPORAL_HOUR && unit < QJS_TEMPORAL_NANOSECOND)
            previous = options.units[unit].style;
    }
    return options;
}

static void duration_text(const QJSIntlAllocator *allocator,
                          const QJSIntlDurationFormatter *formatter,
                          const QJSTemporalDuration *duration, const char *expected)
{
    QJSIntlDurationFormatted result;
    size_t i, end = 0;

    assert(!qjs_intl_duration_format(formatter, duration, &result));
    assert(result.length == strlen(expected));
    for (i = 0; i < result.length; i++)
        assert(result.text[i] == (unsigned char)expected[i]);
    for (i = 0; i < result.part_count; i++) {
        assert(result.parts[i].start == end);
        end = result.parts[i].end;
        assert(result.parts[i].type == QJS_INTL_PART_LITERAL ||
               result.parts[i].unit < QJS_TEMPORAL_UNIT_COUNT);
    }
    assert(end == result.length);
    qjs_intl_duration_result_clear(allocator, &result);
}

static void test_duration(const QJSIntlAllocator *allocator)
{
    QJSIntlDurationService *service;
    QJSIntlDurationFormatter *formatter;
    QJSIntlDurationOptions options = duration_options(QJS_INTL_DURATION_DIGITAL);
    QJSTemporalDuration duration = {{0}};
    const QJSIntlBytes *locales;
    size_t count, en, calls, i;
    QJSIntlBytes system;
    Allocation *state = allocator->opaque;
    QJSIntlDurationFormatted result;

    assert(!qjs_intl_duration_open(allocator, NULL, &service));
    assert(!qjs_intl_duration_available(service, &locales, &count) && count > 100);
    en = find(locales, count, "en");
    assert(!qjs_intl_duration_default_system(service, en, &system));
    assert(!qjs_intl_duration_formatter_open(service, en, system, &options, &formatter));
    /* Formatter handles own all Number/List state and copied separators. */
    qjs_intl_duration_close(service);
    duration_text(allocator, formatter, &duration, "0:00:00");
    duration.fields[QJS_TEMPORAL_HOUR] = 1;
    duration.fields[QJS_TEMPORAL_MINUTE] = 2;
    duration.fields[QJS_TEMPORAL_SECOND] = 3;
    duration.fields[QJS_TEMPORAL_NANOSECOND] = 1;
    duration_text(allocator, formatter, &duration, "1:02:03.000000001");
    for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++)
        duration.fields[i] = -duration.fields[i];
    duration_text(allocator, formatter, &duration, "-1:02:03.000000001");
    state->calls = 0;
    assert(!qjs_intl_duration_format(formatter, &duration, &result));
    calls = state->calls;
    qjs_intl_duration_result_clear(allocator, &result);
    for (i = 1; i <= calls; i++) {
        size_t baseline = state->live;

        state->calls = 0;
        state->fail_at = i;
        assert(qjs_intl_duration_format(formatter, &duration, &result) == QJS_INTL_NO_MEMORY);
        assert(!result.text && !result.parts && state->live == baseline);
    }
    state->fail_at = 0;
    qjs_intl_duration_formatter_close(formatter);

    assert(!qjs_intl_duration_open(allocator, NULL, &service));
    assert(!qjs_intl_duration_available(service, &locales, &count));
    options = duration_options(QJS_INTL_DURATION_DIGITAL);
    assert(!qjs_intl_duration_formatter_open(service, find(locales, count, "da"), (QJSIntlBytes){0},
                                             &options, &formatter));
    duration = (QJSTemporalDuration){{0}};
    duration.fields[QJS_TEMPORAL_HOUR] = 1;
    duration.fields[QJS_TEMPORAL_MINUTE] = 2;
    duration.fields[QJS_TEMPORAL_SECOND] = 3;
    duration_text(allocator, formatter, &duration, "1.02.03");
    qjs_intl_duration_formatter_close(formatter);
    options = duration_options(QJS_INTL_DURATION_LONG);
    assert(!qjs_intl_duration_formatter_open(service, find(locales, count, "en"), (QJSIntlBytes){0},
                                             &options, &formatter));
    duration = (QJSTemporalDuration){{0}};
    duration.fields[QJS_TEMPORAL_YEAR] = 1;
    duration.fields[QJS_TEMPORAL_DAY] = 2;
    duration_text(allocator, formatter, &duration, "1 year, 2 days");
    qjs_intl_duration_formatter_close(formatter);
    /* Admission must be executable through both dependencies. Exercise every
       declared locale/style, showing all units rather than only digital HMS. */
    for (i = 0; i < count; i++) {
        unsigned style, unit;
        for (style = QJS_INTL_DURATION_LONG; style <= QJS_INTL_DURATION_DIGITAL; style++) {
            options = duration_options(style);
            for (unit = 0; unit < QJS_TEMPORAL_UNIT_COUNT; unit++)
                duration.fields[unit] = 1;
            assert(!qjs_intl_duration_formatter_open(service, i, (QJSIntlBytes){0}, &options,
                                                     &formatter));
            assert(!qjs_intl_duration_format(formatter, &duration, &result));
            assert(result.length && result.part_count);
            qjs_intl_duration_result_clear(allocator, &result);
            qjs_intl_duration_formatter_close(formatter);
        }
    }
    qjs_intl_duration_close(service);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);

    test_duration(&allocator);
    assert(!state.live);
    return 0;
}
