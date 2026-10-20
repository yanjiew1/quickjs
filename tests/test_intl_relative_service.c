/* Relative service composition, exact internal rounding, lifetime and OOM.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/relative/relative.h"
#include <assert.h>
#include <float.h>
#include <math.h>
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
static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}
static int equal(QJSIntlBytes text, const char *expected)
{
    return text.length == strlen(expected) && (!text.length || !memcmp(text.data, expected, text.length));
}
static void text_equal(const QJSIntlRelativeFormatted *result, const char *expected)
{
    size_t i;
    assert(result->length == strlen(expected));
    for (i = 0; i < result->length; i++)
        assert(result->text[i] == (unsigned char)expected[i]);
}
static void result_zero(const QJSIntlRelativeFormatted *result)
{
    QJSIntlRelativeFormatted zero = {0};
    assert(!memcmp(result, &zero, sizeof(zero)));
}
static void expect(const QJSIntlAllocator *allocator, QJSIntlRelativeFormatter *formatter,
                   double value, const char *expected)
{
    QJSIntlRelativeFormatted result;
    assert(!qjs_intl_relative_format(formatter, value, QJS_INTL_RELATIVE_DAY, &result));
    text_equal(&result, expected);
    qjs_intl_relative_result_clear(allocator, &result);
}
static QJSIntlRelativeFormatter *open_formatter(QJSIntlRelativeService *service, size_t locale,
                                               QJSIntlRelativeWidth width, QJSIntlRelativeNumeric numeric)
{
    QJSIntlRelativeOptions options = {width, numeric, 4096};
    QJSIntlRelativeFormatter *formatter;
    assert(!qjs_intl_relative_formatter_open(service, locale, (QJSIntlBytes){0}, &options, &formatter));
    return formatter;
}
static void sweep_format(const QJSIntlAllocator *allocator, Allocation *state,
                         QJSIntlRelativeFormatter *formatter, double value)
{
    QJSIntlRelativeFormatted result;
    size_t calls, fail, live = state->live;
    state->calls = 0;
    assert(!qjs_intl_relative_format(formatter, value, QJS_INTL_RELATIVE_DAY, &result));
    calls = state->calls;
    qjs_intl_relative_result_clear(allocator, &result);
    for (fail = 1; fail <= calls; fail++) {
        state->calls = 0;
        state->fail_at = fail;
        assert(qjs_intl_relative_format(formatter, value, QJS_INTL_RELATIVE_DAY, &result) == QJS_INTL_NO_MEMORY);
        result_zero(&result);
        assert(state->live == live);
    }
    state->fail_at = 0;
}
int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlRelativeService *service, *failed;
    QJSIntlRelativeFormatter *formatter, *trial;
    QJSIntlRelativeFormatted result;
    QJSIntlRelativeOptions options = {QJS_INTL_RELATIVE_LONG, QJS_INTL_RELATIVE_ALWAYS, 4096};
    const QJSIntlBytes *available;
    size_t count, i, english = SIZE_MAX, calls, live, fail;
    int supported;
    assert(!qjs_intl_relative_open(&allocator, &service));
    calls = state.calls;
    qjs_intl_relative_close(service);
    assert(!state.live);
    for (fail = 1; fail <= calls; fail++) {
        state.calls = 0;
        state.fail_at = fail;
        failed = (void *)1;
        assert(qjs_intl_relative_open(&allocator, &failed) == QJS_INTL_NO_MEMORY);
        assert(!failed && !state.live);
    }
    state.fail_at = 0;
    assert(!qjs_intl_relative_open(&allocator, &service));
    assert(!qjs_intl_relative_available(service, &available, &count));
    assert(count >= 1 && count < 10000);
    for (i = 0; i < count; i++) {
        QJSIntlBytes system;
        assert(!equal(available[i], "root"));
        if (equal(available[i], "en"))
            english = i;
        assert(!qjs_intl_relative_default_system(service, i, &system));
        assert(!qjs_intl_relative_system_supported(service, i, system, &supported) && supported);
        formatter = open_formatter(service, i, QJS_INTL_RELATIVE_LONG, QJS_INTL_RELATIVE_ALWAYS);
        assert(!qjs_intl_relative_format(formatter, 2, QJS_INTL_RELATIVE_DAY, &result));
        assert(result.length && result.part_count);
        qjs_intl_relative_result_clear(&allocator, &result);
        qjs_intl_relative_formatter_close(formatter);
    }
    assert(english != SIZE_MAX);
    assert(!qjs_intl_relative_system_supported(service, english, bytes("unknown"), &supported) && !supported);
    assert(!qjs_intl_relative_system_supported(service, english, bytes("native"), &supported) && !supported);
    assert(!qjs_intl_relative_system_available(service, bytes("latn"), &supported) && supported);
    assert(!qjs_intl_relative_system_available(service, bytes("roman"), &supported) && !supported);
    assert(qjs_intl_relative_system_available(NULL, bytes("latn"), &supported)
           == QJS_INTL_INVALID_ARGUMENT && !supported);
    assert(qjs_intl_relative_formatter_open(service, english, bytes("unknown"), &options, &trial) == QJS_INTL_UNSUPPORTED);
    assert(!trial);
    for (i = 0; i < QJS_INTL_RELATIVE_WIDTH_COUNT; i++) {
        formatter = open_formatter(service, english, i, QJS_INTL_RELATIVE_ALWAYS);
        assert(!qjs_intl_relative_format(formatter, 2, QJS_INTL_RELATIVE_YEAR, &result));
        assert(result.length && result.part_count);
        qjs_intl_relative_result_clear(&allocator, &result);
        qjs_intl_relative_formatter_close(formatter);
    }
    assert(!qjs_intl_relative_formatter_open(service, english, bytes("arab"), &options, &formatter));
    assert(!qjs_intl_relative_format(formatter, 1, QJS_INTL_RELATIVE_DAY, &result));
    for (i = 0; i < result.part_count; i++) {
        if (result.parts[i].type == QJS_INTL_PART_INTEGER)
            break;
    }
    assert(i < result.part_count && result.text[result.parts[i].start] == 0x661);
    qjs_intl_relative_result_clear(&allocator, &result);
    qjs_intl_relative_formatter_close(formatter);
    formatter = open_formatter(service, english, QJS_INTL_RELATIVE_LONG, QJS_INTL_RELATIVE_ALWAYS);
    expect(&allocator, formatter, -1, "1 day ago");
    expect(&allocator, formatter, -0.0, "0 days ago");
    expect(&allocator, formatter, 0, "in 0 days");
    expect(&allocator, formatter, 1, "in 1 day");
    expect(&allocator, formatter, 2, "in 2 days");
    expect(&allocator, formatter, 1.0005, "in 1 day");
    expect(&allocator, formatter, 0.00001, "in 0 days");
    expect(&allocator, formatter, -0x1p-1074, "0 days ago");
    for (i = 0; i < QJS_INTL_RELATIVE_UNIT_COUNT; i++) {
        size_t part;
        assert(!qjs_intl_relative_format(formatter, 1000.5, i, &result));
        for (part = 0; part < result.part_count; part++) {
            assert(result.parts[part].unit == QJS_INTL_RELATIVE_UNIT_COUNT || result.parts[part].unit == i);
            if (result.parts[part].type != QJS_INTL_PART_LITERAL)
                assert(result.parts[part].unit == i);
        }
        qjs_intl_relative_result_clear(&allocator, &result);
    }
    assert(!qjs_intl_relative_format(formatter, DBL_MAX, QJS_INTL_RELATIVE_DAY, &result));
    assert(result.length > 300);
    qjs_intl_relative_result_clear(&allocator, &result);
    assert(qjs_intl_relative_format(formatter, NAN, 0, &result) == QJS_INTL_INVALID_ARGUMENT);
    result_zero(&result);
    assert(qjs_intl_relative_format(formatter, INFINITY, 0, &result) == QJS_INTL_INVALID_ARGUMENT);
    result_zero(&result);
    assert(qjs_intl_relative_format(formatter, 0, QJS_INTL_RELATIVE_UNIT_COUNT, &result) == QJS_INTL_INVALID_ARGUMENT);
    result_zero(&result);
    sweep_format(&allocator, &state, formatter, -1000.5);
    qjs_intl_relative_formatter_close(formatter);
    state.calls = 0;
    assert(!qjs_intl_relative_formatter_open(service, english, (QJSIntlBytes){0}, &options, &formatter));
    calls = state.calls;
    qjs_intl_relative_formatter_close(formatter);
    live = state.live;
    for (fail = 1; fail <= calls; fail++) {
        state.calls = 0;
        state.fail_at = fail;
        trial = (void *)1;
        assert(qjs_intl_relative_formatter_open(service, english, (QJSIntlBytes){0}, &options, &trial) == QJS_INTL_NO_MEMORY);
        assert(!trial && state.live == live);
    }
    state.fail_at = 0;
    formatter = open_formatter(service, english, QJS_INTL_RELATIVE_LONG, QJS_INTL_RELATIVE_AUTO);
    expect(&allocator, formatter, -0.0, "today");
    expect(&allocator, formatter, 0, "today");
    expect(&allocator, formatter, -1, "yesterday");
    expect(&allocator, formatter, 1, "tomorrow");
    expect(&allocator, formatter, 1.0001, "in 1 day");
    sweep_format(&allocator, &state, formatter, -1);
    sweep_format(&allocator, &state, formatter, 1.0001);
    assert(!qjs_intl_relative_format(formatter, 0, QJS_INTL_RELATIVE_DAY, &result));
    assert(result.part_count == 1 && result.parts[0].type == QJS_INTL_PART_LITERAL &&
           result.parts[0].unit == QJS_INTL_RELATIVE_UNIT_COUNT);
    qjs_intl_relative_result_clear(&allocator, &result);
    qjs_intl_relative_close(service);
    expect(&allocator, formatter, 2, "in 2 days");
    assert(!qjs_intl_relative_format(formatter, 1, QJS_INTL_RELATIVE_DAY, &result));
    qjs_intl_relative_formatter_close(formatter);
    text_equal(&result, "tomorrow");
    qjs_intl_relative_result_clear(&allocator, &result);
    assert(!state.live);
    qjs_intl_relative_formatter_close(NULL);
    qjs_intl_relative_close(NULL);
    return 0;
}
