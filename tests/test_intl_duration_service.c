/* Duration Number/List composition, exact fractions, ownership and OOM.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/duration/duration.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation { size_t calls, fail_at, live; } Allocation;
static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(size);
    if (ptr)
        state->live++;
    return ptr;
}
static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    assert(ptr && state->live);
    state->live--;
    free(ptr);
}
static void *resize(void *opaque, void *ptr, size_t size)
{
    Allocation *state = opaque;
    void *result;
    int fresh = ptr == NULL;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    result = realloc(ptr, size);
    if (result && fresh)
        state->live++;
    return result;
}
static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}
static int equal(QJSIntlBytes text, const char *expected)
{
    return text.length == strlen(expected) && (!text.length || !memcmp(text.data, expected, text.length));
}
static void text_equal(const QJSIntlDurationFormatted *result, const char *expected)
{
    size_t i;
    assert(result->length == strlen(expected));
    for (i = 0; i < result->length; i++)
        assert(result->text[i] == (unsigned char)expected[i]);
}
static void result_zero(const QJSIntlDurationFormatted *result)
{
    QJSIntlDurationFormatted zero = {0};
    assert(!memcmp(result, &zero, sizeof(zero)));
}
static QJSIntlDurationOptions defaults(QJSIntlDurationStyle style)
{
    QJSIntlDurationOptions options = {.style = style, .fractional_digits = -1, .maximum_output_length = 4096};
    QJSIntlDurationUnitStyle previous = QJS_INTL_DURATION_UNIT_UNSET;
    unsigned int unit;
    for (unit = 0; unit < QJS_TEMPORAL_UNIT_COUNT; unit++) {
        assert(!qjs_intl_duration_unit_options(unit, style, QJS_INTL_DURATION_UNIT_UNSET,
                                               QJS_INTL_DURATION_DISPLAY_UNSET, previous, 0, &options.units[unit]));
        if (unit >= QJS_TEMPORAL_HOUR && unit < QJS_TEMPORAL_NANOSECOND)
            previous = options.units[unit].style;
    }
    return options;
}
static QJSIntlDurationFormatter *open_formatter(QJSIntlDurationService *service, size_t locale,
                                               const QJSIntlDurationOptions *options)
{
    QJSIntlDurationFormatter *formatter;
    assert(!qjs_intl_duration_formatter_open(service, locale, (QJSIntlBytes){0}, options, &formatter));
    return formatter;
}
static void expect(const QJSIntlAllocator *allocator, QJSIntlDurationFormatter *formatter,
                   const QJSTemporalDuration *duration, const char *expected)
{
    QJSIntlDurationFormatted result;
    size_t i, end = 0;
    assert(!qjs_intl_duration_format(formatter, duration, &result));
    text_equal(&result, expected);
    for (i = 0; i < result.part_count; i++) {
        assert(result.parts[i].start == end && result.parts[i].end >= end);
        end = result.parts[i].end;
        assert(result.parts[i].type == QJS_INTL_PART_LITERAL || result.parts[i].unit < QJS_TEMPORAL_UNIT_COUNT);
    }
    assert(end == result.length);
    qjs_intl_duration_result_clear(allocator, &result);
}
static void sweep(const QJSIntlAllocator *allocator, Allocation *state,
                  QJSIntlDurationFormatter *formatter, const QJSTemporalDuration *duration)
{
    QJSIntlDurationFormatted result;
    size_t calls, fail, live = state->live;
    state->calls = 0;
    assert(!qjs_intl_duration_format(formatter, duration, &result));
    calls = state->calls;
    qjs_intl_duration_result_clear(allocator, &result);
    for (fail = 1; fail <= calls; fail++) {
        state->calls = 0;
        state->fail_at = fail;
        assert(qjs_intl_duration_format(formatter, duration, &result) == QJS_INTL_NO_MEMORY);
        result_zero(&result);
        assert(state->live == live);
    }
    state->fail_at = 0;
}

static void check_parts(const QJSIntlAllocator *allocator, QJSIntlDurationFormatter *formatter,
                        const QJSTemporalDuration *duration)
{
    static const QJSIntlDurationPart expected[] = {
        {0, 1, QJS_INTL_PART_MINUS_SIGN, QJS_TEMPORAL_YEAR},
        {1, 2, QJS_INTL_PART_INTEGER, QJS_TEMPORAL_YEAR},
        {2, 3, QJS_INTL_PART_LITERAL, QJS_TEMPORAL_YEAR},
        {3, 8, QJS_INTL_PART_UNIT, QJS_TEMPORAL_YEAR},
        {8, 10, QJS_INTL_PART_LITERAL, QJS_TEMPORAL_UNIT_COUNT},
        {10, 11, QJS_INTL_PART_INTEGER, QJS_TEMPORAL_MONTH},
        {11, 12, QJS_INTL_PART_LITERAL, QJS_TEMPORAL_MONTH},
        {12, 18, QJS_INTL_PART_UNIT, QJS_TEMPORAL_MONTH},
    };
    QJSIntlDurationFormatted result;
    size_t i;
    assert(!qjs_intl_duration_format(formatter, duration, &result));
    assert(result.part_count == sizeof(expected) / sizeof(*expected));
    for (i = 0; i < result.part_count; i++) {
        assert(result.parts[i].start == expected[i].start && result.parts[i].end == expected[i].end);
        assert(result.parts[i].type == expected[i].type && result.parts[i].unit == expected[i].unit);
    }
    qjs_intl_duration_result_clear(allocator, &result);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, resize, release};
    QJSIntlDurationService *service, *failed;
    QJSIntlDurationFormatter *formatter, *trial;
    QJSIntlDurationOptions options = defaults(QJS_INTL_DURATION_DIGITAL);
    QJSTemporalDuration duration = {{0}};
    QJSIntlDurationFormatted result;
    const QJSIntlBytes *available;
    size_t count, i, english = SIZE_MAX, calls, live, fail;
    int supported;
    QJSIntlAllocator incomplete = allocator;
    incomplete.realloc = NULL;
    assert(qjs_intl_duration_open(&incomplete, NULL, &failed) == QJS_INTL_INVALID_ARGUMENT && !failed && !state.calls);
    assert(!qjs_intl_duration_open(&allocator, NULL, &service));
    calls = state.calls;
    qjs_intl_duration_close(service);
    assert(!state.live);
    for (fail = 1; fail <= calls; fail++) {
        state.calls = 0;
        state.fail_at = fail;
        failed = (void *)1;
        assert(qjs_intl_duration_open(&allocator, NULL, &failed) == QJS_INTL_NO_MEMORY);
        assert(!failed && !state.live);
    }
    state.fail_at = 0;
    assert(!qjs_intl_duration_open(&allocator, NULL, &service));
    assert(!qjs_intl_duration_available(service, &available, &count));
    assert(count >= 1 && count < 10000);
    for (i = 0; i < count; i++) {
        QJSIntlBytes system;
        size_t before = state.calls;
        assert(!equal(available[i], "root"));
        if (equal(available[i], "en"))
            english = i;
        assert(!qjs_intl_duration_default_system(service, i, &system));
        assert(!qjs_intl_duration_system_supported(service, i, system, &supported) && supported);
        assert(!qjs_intl_duration_two_digit_hours(service, i, &supported) && !supported);
        assert(state.calls == before);
    }
    assert(english != SIZE_MAX);
    assert(!qjs_intl_duration_system_supported(service, english, bytes("unknown"), &supported) && !supported);
    assert(qjs_intl_duration_formatter_open(service, english, bytes("unknown"), &options, &trial) == QJS_INTL_UNSUPPORTED);
    assert(!trial);
    formatter = open_formatter(service, english, &options);
    expect(&allocator, formatter, &duration, "0:00:00");
    duration.fields[QJS_TEMPORAL_HOUR] = 1;
    duration.fields[QJS_TEMPORAL_MINUTE] = 2;
    duration.fields[QJS_TEMPORAL_SECOND] = 3;
    expect(&allocator, formatter, &duration, "1:02:03");
    duration.fields[QJS_TEMPORAL_NANOSECOND] = 1;
    expect(&allocator, formatter, &duration, "1:02:03.000000001");
    sweep(&allocator, &state, formatter, &duration);
    duration.fields[QJS_TEMPORAL_YEAR] = 1;
    duration.fields[QJS_TEMPORAL_DAY] = 2;
    expect(&allocator, formatter, &duration, "1 yr, 2 days, 1:02:03.000000001");
    sweep(&allocator, &state, formatter, &duration);
    qjs_intl_duration_formatter_close(formatter);
    options.fractional_digits = 3;
    formatter = open_formatter(service, english, &options);
    duration = (QJSTemporalDuration){{0}};
    duration.fields[QJS_TEMPORAL_SECOND] = 1;
    duration.fields[QJS_TEMPORAL_NANOSECOND] = 999999999;
    expect(&allocator, formatter, &duration, "0:00:01.999");
    qjs_intl_duration_formatter_close(formatter);
    options.fractional_digits = -1;
    formatter = open_formatter(service, english, &options);
    duration.fields[QJS_TEMPORAL_SECOND] = 9007199254740991.0;
    expect(&allocator, formatter, &duration, "0:00:9007199254740991.999999999");
    duration = (QJSTemporalDuration){{0}};
    duration.fields[QJS_TEMPORAL_MINUTE] = -2;
    expect(&allocator, formatter, &duration, "-0:02:00");
    duration.fields[QJS_TEMPORAL_SECOND] = 1;
    assert(qjs_intl_duration_format(formatter, &duration, &result) == QJS_INTL_INVALID_ARGUMENT);
    result_zero(&result);
    qjs_intl_duration_formatter_close(formatter);
    options.units[QJS_TEMPORAL_HOUR].display = QJS_INTL_DURATION_AUTO;
    options.units[QJS_TEMPORAL_MINUTE].display = QJS_INTL_DURATION_AUTO;
    options.units[QJS_TEMPORAL_SECOND].display = QJS_INTL_DURATION_AUTO;
    formatter = open_formatter(service, english, &options);
    duration = (QJSTemporalDuration){{0}};
    duration.fields[QJS_TEMPORAL_HOUR] = duration.fields[QJS_TEMPORAL_SECOND] = 1;
    expect(&allocator, formatter, &duration, "1:00:01");
    duration.fields[QJS_TEMPORAL_SECOND] = 0;
    expect(&allocator, formatter, &duration, "1");
    duration.fields[QJS_TEMPORAL_HOUR] = 0;
    expect(&allocator, formatter, &duration, "");
    qjs_intl_duration_formatter_close(formatter);
    options = defaults(QJS_INTL_DURATION_LONG);
    formatter = open_formatter(service, english, &options);
    duration.fields[QJS_TEMPORAL_YEAR] = 1;
    duration.fields[QJS_TEMPORAL_MONTH] = 2;
    expect(&allocator, formatter, &duration, "1 year, 2 months");
    sweep(&allocator, &state, formatter, &duration);
    for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++)
        duration.fields[i] = i + 1;
    assert(!qjs_intl_duration_format(formatter, &duration, &result));
    for (i = 0; i < QJS_TEMPORAL_UNIT_COUNT; i++) {
        size_t part;
        for (part = 0; part < result.part_count; part++)
            if (result.parts[part].unit == (QJSTemporalUnit)i)
                break;
        assert(part < result.part_count);
    }
    qjs_intl_duration_result_clear(&allocator, &result);
    qjs_intl_duration_formatter_close(formatter);
    options.units[QJS_TEMPORAL_YEAR].display = QJS_INTL_DURATION_ALWAYS;
    formatter = open_formatter(service, english, &options);
    duration = (QJSTemporalDuration){{0}};
    duration.fields[QJS_TEMPORAL_MONTH] = -2;
    expect(&allocator, formatter, &duration, "-0 years, 2 months");
    check_parts(&allocator, formatter, &duration);
    qjs_intl_duration_formatter_close(formatter);
    for (i = QJS_TEMPORAL_SECOND; i <= QJS_TEMPORAL_MICROSECOND; i++) {
        unsigned int following;
        static const char *const expected[] = {"1.000000001 seconds", "1.000001 milliseconds", "1.001 microseconds"};
        options = defaults(QJS_INTL_DURATION_LONG);
        for (following = i + 1; following < QJS_TEMPORAL_UNIT_COUNT; following++)
            options.units[following].style = QJS_INTL_DURATION_UNIT_FRACTIONAL;
        formatter = open_formatter(service, english, &options);
        duration = (QJSTemporalDuration){{0}};
        duration.fields[i] = duration.fields[QJS_TEMPORAL_NANOSECOND] = 1;
        expect(&allocator, formatter, &duration, expected[i - QJS_TEMPORAL_SECOND]);
        sweep(&allocator, &state, formatter, &duration);
        qjs_intl_duration_formatter_close(formatter);
    }
    options.fractional_digits = 9;
    formatter = open_formatter(service, english, &options);
    expect(&allocator, formatter, &duration, "1.001000000 microseconds");
    qjs_intl_duration_formatter_close(formatter);
    for (i = QJS_TEMPORAL_MINUTE; i <= QJS_TEMPORAL_SECOND; i++) {
        unsigned int current;
        QJSIntlDurationUnitStyle previous = QJS_INTL_DURATION_UNIT_LONG;
        options = defaults(QJS_INTL_DURATION_LONG);
        for (current = i; current < QJS_TEMPORAL_UNIT_COUNT; current++) {
            assert(!qjs_intl_duration_unit_options(current, options.style,
                       current == i ? QJS_INTL_DURATION_UNIT_NUMERIC : QJS_INTL_DURATION_UNIT_UNSET,
                       QJS_INTL_DURATION_DISPLAY_UNSET, previous, 0, &options.units[current]));
            previous = options.units[current].style;
        }
        formatter = open_formatter(service, english, &options);
        duration = (QJSTemporalDuration){{0}};
        duration.fields[QJS_TEMPORAL_SECOND] = 3;
        if (i == QJS_TEMPORAL_MINUTE)
            duration.fields[QJS_TEMPORAL_MINUTE] = 2;
        expect(&allocator, formatter, &duration, i == QJS_TEMPORAL_MINUTE ? "2:03" : "3");
        qjs_intl_duration_formatter_close(formatter);
    }
    options.units[QJS_TEMPORAL_SECOND].display = QJS_INTL_DURATION_AUTO;
    options.fractional_digits = 0;
    formatter = open_formatter(service, english, &options);
    duration = (QJSTemporalDuration){{0}};
    duration.fields[QJS_TEMPORAL_NANOSECOND] = -1;
    expect(&allocator, formatter, &duration, "-0");
    qjs_intl_duration_formatter_close(formatter);
    options = defaults(QJS_INTL_DURATION_DIGITAL);
    for (i = 0; i < count; i++) {
        if (equal(available[i], "nds") || equal(available[i], "ee") || equal(available[i], "af")) {
            formatter = open_formatter(service, i, &options);
            duration = (QJSTemporalDuration){{0}};
            duration.fields[QJS_TEMPORAL_HOUR] = 1;
            duration.fields[QJS_TEMPORAL_MINUTE] = 2;
            duration.fields[QJS_TEMPORAL_SECOND] = 3;
            expect(&allocator, formatter, &duration, equal(available[i], "nds") ? "1.02:03" : "1:02:03");
            qjs_intl_duration_formatter_close(formatter);
        }
    }
    assert(!qjs_intl_duration_formatter_open(service, english, bytes("arab"), &options, &formatter));
    duration = (QJSTemporalDuration){{0}};
    assert(!qjs_intl_duration_format(formatter, &duration, &result));
    for (i = 0; i < result.part_count; i++)
        if (result.parts[i].type == QJS_INTL_PART_INTEGER)
            assert(result.text[result.parts[i].start] == 0x660);
    qjs_intl_duration_result_clear(&allocator, &result);
    qjs_intl_duration_formatter_close(formatter);
    state.calls = 0;
    formatter = open_formatter(service, english, &options);
    calls = state.calls;
    qjs_intl_duration_formatter_close(formatter);
    live = state.live;
    for (fail = 1; fail <= calls; fail++) {
        state.calls = 0;
        state.fail_at = fail;
        assert(qjs_intl_duration_formatter_open(service, english, (QJSIntlBytes){0}, &options, &trial) == QJS_INTL_NO_MEMORY);
        assert(!trial && state.live == live);
    }
    state.fail_at = 0;
    options.maximum_output_length = 2;
    formatter = open_formatter(service, english, &options);
    assert(qjs_intl_duration_format(formatter, &duration, &result) == QJS_INTL_OVERFLOW);
    result_zero(&result);
    qjs_intl_duration_formatter_close(formatter);
    options.maximum_output_length = 4096;
    formatter = open_formatter(service, english, &options);
    qjs_intl_duration_close(service);
    assert(!qjs_intl_duration_format(formatter, &duration, &result));
    qjs_intl_duration_formatter_close(formatter);
    text_equal(&result, "0:00:00");
    qjs_intl_duration_result_clear(&allocator, &result);
    assert(!state.live);
    return 0;
}
