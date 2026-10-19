/* Display-name facade: six kinds, prepared profiles, ownership and OOM.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/display/display.h"
#include "../src/intl/display/text.h"
#include "../src/intl/native/data.h"
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

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static int equal(QJSIntlBytes text, const char *expected)
{
    return text.length == strlen(expected) && (!text.length || !memcmp(text.data, expected, text.length));
}

static void text_equal(const QJSIntlNameText *result, const char *expected)
{
    size_t i;
    assert(result->found && result->length == strlen(expected));
    for (i = 0; i < result->length; i++)
        assert(result->text[i] == (unsigned char)expected[i]);
}

static void zero(const QJSIntlNameText *result)
{
    QJSIntlNameText empty = {0};
    assert(!memcmp(result, &empty, sizeof(empty)));
}

static QJSIntlDisplayFormatter *formatter(QJSIntlDisplayService *service, size_t locale,
                                          QJSIntlDisplayType type, QJSIntlNameStyle style, size_t limit)
{
    QJSIntlDisplayOptions options = {type, style, 1, limit};
    QJSIntlDisplayFormatter *out;
    assert(!qjs_intl_display_formatter_open(service, locale, &options, &out));
    return out;
}

static void expect(const QJSIntlAllocator *allocator, const QJSIntlDisplayFormatter *formatter,
                   const char *code, const char *expected)
{
    QJSIntlNameText result;
    assert(!qjs_intl_display_format(formatter, bytes(code), &result));
    if (expected)
        text_equal(&result, expected);
    else
        zero(&result);
    qjs_intl_name_text_clear(allocator, &result);
}

static void format_oom(const QJSIntlAllocator *allocator, Allocation *state,
                       const QJSIntlDisplayFormatter *formatter, const char *code)
{
    QJSIntlNameText result;
    size_t count, fail, live = state->live;
    state->calls = 0;
    assert(!qjs_intl_display_format(formatter, bytes(code), &result));
    count = state->calls;
    qjs_intl_name_text_clear(allocator, &result);
    for (fail = 1; fail <= count; fail++) {
        state->calls = 0;
        state->fail_at = fail;
        assert(qjs_intl_display_format(formatter, bytes(code), &result) == QJS_INTL_NO_MEMORY);
        zero(&result);
        assert(state->live == live);
    }
    state->fail_at = 0;
}

int main(void)
{
    static const char *const codes[] = {"en", "US", "Latn", "USD", "gregory", "year"};
    static const char *const names[] = {
        "English", "United States", "Latin", "US Dollar", "Gregorian Calendar", "year"
    };
    static const char *const missing[] = {"zz", "QQ", "Qaaa", "QQQ", "unknown", NULL};
    static const char *const date_codes[] = {
        "era", "year", "quarter", "month", "weekOfYear", "weekday", "day", "dayPeriod",
        "hour", "minute", "second", "timeZoneName"
    };
    /* Exact English displayName facts from the pinned CLDR source, including
       uncounted currency casing rather than the plural-name fields. */
    static const char *const date_names[][3] = {
        {"era", "era", "era"}, {"year", "yr.", "yr"},
        {"quarter", "qtr.", "qtr"}, {"month", "mo.", "mo"},
        {"week", "wk.", "wk"}, {"day of the week", "day of wk.", "day of wk."},
        {"day", "day", "day"}, {"AM/PM", "AM/PM", "AM/PM"},
        {"hour", "hr.", "hr"}, {"minute", "min.", "min"},
        {"second", "sec.", "sec"}, {"time zone", "zone", "zone"}
    };
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlDisplayService *service, *failed;
    QJSIntlDisplayFormatter *handle, *trial;
    QJSIntlNameText result;
    QJSIntlData data;
    const QJSIntlBytes *available;
    size_t count, english = SIZE_MAX, source = 0, i, width, calls, fail, baseline;
    QJSIntlDisplayOptions options = {QJS_INTL_DISPLAY_LANGUAGE, QJS_INTL_NAME_LONG, 1, 4096};
    assert(!qjs_intl_display_open(&allocator, &service));
    calls = state.calls;
    qjs_intl_display_close(service);
    assert(!state.live);
    for (fail = 1; fail <= calls; fail++) {
        state.calls = 0;
        state.fail_at = fail;
        failed = (void *)1;
        assert(qjs_intl_display_open(&allocator, &failed) == QJS_INTL_NO_MEMORY);
        assert(!failed && !state.live);
    }
    state.fail_at = 0;
    assert(!qjs_intl_display_open(&allocator, &service));
    assert(!qjs_intl_display_available(service, &available, &count));
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    assert(count == data.locales.count - 1);
    for (i = 0; i < data.locales.count; i++) {
        QJSIntlBytes tag;
        uint32_t parent;
        assert(!qjs_intl_data_locale(&data, i, &tag, &parent));
        if (parent == UINT32_MAX)
            continue;
        assert(source < count && tag.length == available[source].length &&
               !memcmp(tag.data, available[source].data, tag.length));
        assert(!equal(tag, "root"));
        if (equal(tag, "en"))
            english = source;
        source++;
    }
    assert(source == count && english != SIZE_MAX);
    baseline = state.live;
    for (i = 0; i < QJS_INTL_DISPLAY_TYPE_COUNT; i++) {
        options.type = i;
        state.calls = 0;
        assert(!qjs_intl_display_formatter_open(service, english, &options, &handle));
        calls = state.calls;
        expect(&allocator, handle, codes[i], names[i]);
        if (missing[i])
            expect(&allocator, handle, missing[i], NULL);
        format_oom(&allocator, &state, handle, codes[i]);
        qjs_intl_display_formatter_close(handle);
        assert(state.live == baseline);
        for (fail = 1; fail <= calls; fail++) {
            state.calls = 0;
            state.fail_at = fail;
            trial = (void *)1;
            assert(qjs_intl_display_formatter_open(service, english, &options, &trial) == QJS_INTL_NO_MEMORY);
            assert(!trial && state.live == baseline);
        }
        state.fail_at = 0;
    }
    handle = formatter(service, english, QJS_INTL_DISPLAY_CALENDAR, QJS_INTL_NAME_LONG, 4096);
    expect(&allocator, handle, "ethioaa", "Ethiopic Amete Alem Calendar");
    expect(&allocator, handle, "islamicc", "Hijri Calendar (tabular, civil epoch)");
    qjs_intl_display_formatter_close(handle);
    handle = formatter(service, english, QJS_INTL_DISPLAY_REGION, QJS_INTL_NAME_LONG, 4096);
    expect(&allocator, handle, "419", "Latin America");
    qjs_intl_display_formatter_close(handle);
    options = (QJSIntlDisplayOptions){QJS_INTL_DISPLAY_LANGUAGE, QJS_INTL_NAME_LONG, 0, 4096};
    assert(!qjs_intl_display_formatter_open(service, english, &options, &handle));
    expect(&allocator, handle, "en-GB", "English (United Kingdom)");
    qjs_intl_display_formatter_close(handle);
    handle = formatter(service, english, QJS_INTL_DISPLAY_LANGUAGE, QJS_INTL_NAME_LONG, 4096);
    expect(&allocator, handle, "en-GB", "British English");
    qjs_intl_display_formatter_close(handle);
    for (width = QJS_INTL_NAME_LONG; width <= QJS_INTL_NAME_NARROW; width++) {
        handle = formatter(service, english, QJS_INTL_DISPLAY_REGION, width, 4096);
        expect(&allocator, handle, "US", width == QJS_INTL_NAME_LONG ? "United States" : "US");
        qjs_intl_display_formatter_close(handle);
        handle = formatter(service, english, QJS_INTL_DISPLAY_SCRIPT, width, 4096);
        expect(&allocator, handle, "Latn", "Latin");
        qjs_intl_display_formatter_close(handle);
        handle = formatter(service, english, QJS_INTL_DISPLAY_CALENDAR, width, 4096);
        expect(&allocator, handle, "gregory", "Gregorian Calendar");
        qjs_intl_display_formatter_close(handle);
        handle = formatter(service, english, QJS_INTL_DISPLAY_DATE_TIME_FIELD, width, 4096);
        for (i = 0; i < sizeof(date_codes) / sizeof(*date_codes); i++)
            expect(&allocator, handle, date_codes[i], date_names[i][width]);
        assert(qjs_intl_display_format(handle, bytes("WeekOfYear"), &result) == QJS_INTL_INVALID_ARGUMENT);
        zero(&result);
        qjs_intl_display_formatter_close(handle);
    }
    handle = formatter(service, english, QJS_INTL_DISPLAY_REGION, QJS_INTL_NAME_LONG, 13);
    expect(&allocator, handle, "US", "United States");
    qjs_intl_display_formatter_close(handle);
    handle = formatter(service, english, QJS_INTL_DISPLAY_REGION, QJS_INTL_NAME_LONG, 12);
    assert(qjs_intl_display_format(handle, bytes("US"), &result) == QJS_INTL_OVERFLOW);
    zero(&result);
    qjs_intl_display_formatter_close(handle);
    options = (QJSIntlDisplayOptions){QJS_INTL_DISPLAY_CALENDAR, QJS_INTL_NAME_LONG, 1, 4096};
    assert(!qjs_intl_display_formatter_open(service, english, &options, &handle));
    options.type = QJS_INTL_DISPLAY_DATE_TIME_FIELD;
    qjs_intl_display_close(service);
    {
        char code[] = "gregory";
        assert(!qjs_intl_display_format(handle, bytes(code), &result));
        memset(code, 'x', sizeof(code) - 1);
    }
    qjs_intl_display_formatter_close(handle);
    text_equal(&result, "Gregorian Calendar");
    qjs_intl_name_text_clear(&allocator, &result);
    assert(!state.live);
    /* Shared text ownership keeps empty/code-identical labels present. */
    calls = state.calls;
    assert(!qjs_intl_name_text_copy(&allocator, (QJSIntlBytes){0}, 4, &result));
    assert(result.found && !result.text && !result.length && state.calls == calls);
    qjs_intl_name_text_clear(&allocator, &result);
    assert(!qjs_intl_name_text_copy(&allocator, bytes("QQQ"), 3, &result));
    text_equal(&result, "QQQ");
    qjs_intl_name_text_clear(&allocator, &result);
    assert(!qjs_intl_name_text_copy(&allocator, bytes("😀"), 2, &result));
    assert(result.found && result.length == 2 && result.text[0] == 0xd83d && result.text[1] == 0xde00);
    qjs_intl_name_text_clear(&allocator, &result);
    assert(qjs_intl_name_text_copy(&allocator, bytes("😀"), 1, &result) == QJS_INTL_OVERFLOW);
    zero(&result);
    assert(qjs_intl_display_open(NULL, &failed) == QJS_INTL_INVALID_ARGUMENT && !failed);
    assert(qjs_intl_display_open(&allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);
    available = (void *)1;
    count = 7;
    assert(qjs_intl_display_available(NULL, &available, &count) == QJS_INTL_INVALID_ARGUMENT);
    assert(!available && !count);
    trial = (void *)1;
    assert(qjs_intl_display_formatter_open(NULL, 0, &options, &trial) == QJS_INTL_INVALID_ARGUMENT && !trial);
    assert(qjs_intl_display_format(NULL, bytes("US"), &result) == QJS_INTL_INVALID_ARGUMENT);
    zero(&result);
    assert(!qjs_intl_display_open(&allocator, &service));
    options.maximum_output_length = 0;
    assert(qjs_intl_display_formatter_open(service, english, &options, &trial) == QJS_INTL_INVALID_ARGUMENT && !trial);
    options.maximum_output_length = 4096;
    assert(qjs_intl_display_formatter_open(service, SIZE_MAX, &options, &trial) == QJS_INTL_INVALID_ARGUMENT && !trial);
    options.type = QJS_INTL_DISPLAY_TYPE_COUNT;
    assert(qjs_intl_display_formatter_open(service, english, &options, &trial) == QJS_INTL_INVALID_ARGUMENT && !trial);
    options.type = QJS_INTL_DISPLAY_REGION;
    options.style = -1;
    assert(qjs_intl_display_formatter_open(service, english, &options, &trial) == QJS_INTL_INVALID_ARGUMENT && !trial);
    options.style = QJS_INTL_NAME_LONG;
    options.dialect = 2;
    assert(qjs_intl_display_formatter_open(service, english, &options, &trial) == QJS_INTL_INVALID_ARGUMENT && !trial);
    options.dialect = 1;
    options.maximum_output_length = SIZE_MAX;
    assert(qjs_intl_display_formatter_open(service, english, &options, &trial) == QJS_INTL_INVALID_ARGUMENT && !trial);
    qjs_intl_display_close(service);
    qjs_intl_display_close(NULL);
    qjs_intl_display_formatter_close(NULL);
    qjs_intl_name_text_clear(&allocator, &result);
    qjs_intl_name_text_clear(&allocator, NULL);
    assert(!state.live);
    return 0;
}
