/* Native calendar fields preserve API identities and source lookup spelling.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/display/calendar-names.h"
#include "../src/intl/display/select.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t calls, fail_at, live;
} Allocation;

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
    QJSIntlBytes result = {text, strlen(text)};
    return result;
}

static int equal(QJSIntlBytes text, const char *expected)
{
    return text.length == strlen(expected) &&
           (!text.length || !memcmp(text.data, expected, text.length));
}

static void check(const QJSIntlCalendarNames *names, const char *input, const char *source)
{
    QJSIntlBytes key, zero = {0};
    memset(&key, 0xff, sizeof(key));
    if (source) {
        assert(!qjs_intl_calendar_names_find(names, bytes(input), &key));
        assert(equal(key, source));
    } else {
        assert(qjs_intl_calendar_names_find(names, bytes(input), &key) == QJS_INTL_UNSUPPORTED);
        assert(!memcmp(&key, &zero, sizeof(key)));
    }
}

static void check_queries(const QJSIntlCalendarNames *projection, const QJSIntlNameData *names,
                          const QJSIntlLocaleMetadata *metadata)
{
    QJSIntlBytes key, zero = {0}, invalid = {NULL, 1};
#if TEST_INTL_CALENDAR_NAME_MODE == 4
    uint32_t i;
    for (i = 0; i < metadata->types.count; i++) {
        QJSIntlLocaleType type;
        assert(!qjs_intl_locale_type_at(metadata, i, &type));
        if (type.variable || !equal(type.extension, "u") || !equal(type.key, "ca"))
            continue;
        assert(!qjs_intl_calendar_names_find(projection, type.name, &key));
        assert(key.length);
    }
    check(projection, "gregory", "gregorian");
    check(projection, "ethioaa", "ethiopic-amete-alem");
    check(projection, "islamicc", "islamic-civil");
    check(projection, "islamic-civil", "islamic-civil");
    check(projection, "ethiopic-amete-alem", "ethiopic-amete-alem");
    check(projection, "gregorian", NULL); /* Not a Unicode type subtag. */
#elif TEST_INTL_CALENDAR_NAME_MODE == 0
    uint32_t i, parent;
    QJSIntlNameLabel label;
    assert(projection->count == 7);
    check(projection, "ancient", "legacy-calendar");
    check(projection, "modern", "legacy-calendar");
    check(projection, "oldname", "legacy-calendar");
    check(projection, "badalias", "legacy-calendar");
    check(projection, "legacy-calendar", "legacy-calendar");
    check(projection, "valid", "ninechars");
    check(projection, "extra", "extra");
    check(projection, "ninechars", NULL);
    check(projection, "unbacked", NULL);
    check(projection, "BADALIAS", NULL); /* Core supplies lowercase input. */
    assert(!qjs_intl_calendar_names_find(projection, bytes("oldname"), &key));
    for (i = 0; i < names->data.locales.count; i++) {
        QJSIntlBytes locale;
        assert(!qjs_intl_data_locale(&names->data, i, &locale, &parent));
        if (equal(locale, "en"))
            break;
    }
    assert(i < names->data.locales.count);
    assert(!qjs_intl_name_select(names, i, QJS_INTL_NAME_CALENDAR, QJS_INTL_NAME_LONG, 0, key, &label));
    assert(label.found && equal(label.entry.text, "Child calendar"));
    assert(equal(label.entry.raw_code, "LEGACY-CALENDAR"));
    assert(equal(label.origin_locale, "en"));
#elif TEST_INTL_CALENDAR_NAME_MODE == 3
    assert(!projection->count && !projection->records);
    check(projection, "modern", NULL);
#endif
    (void)names;
    (void)metadata;
    check(projection, "unknown", NULL);
    memset(&key, 0xff, sizeof(key));
    assert(qjs_intl_calendar_names_find(projection, invalid, &key) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&key, &zero, sizeof(key)));
    assert(qjs_intl_calendar_names_find(NULL, bytes("modern"), &key) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_calendar_names_find(projection, bytes("modern"), NULL) == QJS_INTL_INVALID_ARGUMENT);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlLocaleMetadata metadata;
    QJSIntlNameData names;
    QJSIntlCalendarNames projection, result, zero = {0};
    QJSIntlStatus expected = (TEST_INTL_CALENDAR_NAME_MODE == 1 || TEST_INTL_CALENDAR_NAME_MODE == 2)
                                ? QJS_INTL_DATA_ERROR : QJS_INTL_OK;
    size_t calls, step;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_locale_metadata_open(&data, &allocator, &metadata));
    assert(!qjs_intl_name_data_open(&data, &allocator, &names) && !state.live);
    state.calls = 0;
    memset(&projection, 0xff, sizeof(projection));
    assert(qjs_intl_calendar_names_open(&metadata, &names, &allocator, &projection) == expected);
    calls = state.calls;
    if (expected) {
        assert(!state.live && !memcmp(&projection, &zero, sizeof(projection)));
    } else {
        check_queries(&projection, &names, &metadata);
        assert(state.calls == calls);
        qjs_intl_calendar_names_clear(&projection);
        qjs_intl_calendar_names_clear(&projection);
        assert(!state.live && !memcmp(&projection, &zero, sizeof(projection)));
    }
    assert(calls == (TEST_INTL_CALENDAR_NAME_MODE == 3 ? 0 : 1));
    for (step = 1; step <= calls; step++) {
        state.calls = 0;
        state.fail_at = step;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_calendar_names_open(&metadata, &names, &allocator, &result) == QJS_INTL_NO_MEMORY);
        assert(!state.live && !memcmp(&result, &zero, sizeof(result)));
        state.fail_at = 0;
        assert(qjs_intl_calendar_names_open(&metadata, &names, &allocator, &result) == expected);
        qjs_intl_calendar_names_clear(&result);
        assert(!state.live);
    }
#if TEST_INTL_CALENDAR_NAME_MODE == 0
    {
        uint8_t *copy = malloc(qjs_intl_native_length);
        QJSIntlData copied_data;
        QJSIntlLocaleMetadata copied_metadata;
        assert(copy);
        memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
        assert(!qjs_intl_data_open(copy, qjs_intl_native_length, &allocator, &copied_data));
        assert(!qjs_intl_locale_metadata_open(&copied_data, &allocator, &copied_metadata));
        assert(!qjs_intl_calendar_names_open(&copied_metadata, &names, &allocator, &result));
        free(copy); /* Keys are owned; only the names blob is retained. */
        calls = state.calls;
        check_queries(&result, &names, &metadata);
        assert(state.calls == calls);
        qjs_intl_calendar_names_clear(&result);
        assert(!state.live);
    }
#endif
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_calendar_names_open(NULL, &names, &allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(qjs_intl_calendar_names_open(&metadata, NULL, &allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_calendar_names_open(&metadata, &names, NULL, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_calendar_names_open(&metadata, &names, &allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);
    qjs_intl_calendar_names_clear(NULL);
    return 0;
}
