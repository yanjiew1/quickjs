/* Calendar metadata equivalence does not imply renderer support.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/calendar-identity.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t calendar_ambiguous_bytes[];
extern const size_t calendar_ambiguous_length;

typedef struct Allocation {
    size_t calls, live;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    state->calls++;
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

static int equal(QJSIntlBytes text, const char *value)
{
    return text.length == strlen(value) && (!text.length || !memcmp(text.data, value, text.length));
}

static void identity(const QJSIntlLocaleMetadata *metadata, const char *spelling,
                     const char *expected, QJSIntlStatus status)
{
    QJSIntlLocaleType type;
    uint32_t index = UINT32_MAX;
    assert(qjs_intl_locale_type_identity(metadata, bytes("u"), bytes("ca"), bytes(spelling), &index) == status);
    if (status) {
        assert(!index);
    } else {
        assert(!qjs_intl_locale_type_at(metadata, index, &type));
        assert(equal(type.name, expected));
    }
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData shared;
    QJSIntlLocaleMetadata metadata;
    QJSIntlDateAvailableData available;
    QJSIntlDateStyleData styles;
    QJSIntlDateCombinationData combinations;
    QJSIntlCalendarNameData names;
    QJSIntlDateIntervalData intervals;
    QJSIntlBytes raw, invalid = {NULL, 1};
    uint32_t index;
    size_t calls;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &shared));
    assert(!qjs_intl_locale_metadata_open(&shared, &allocator, &metadata));
    assert(!qjs_intl_date_available_data_open(&shared, &allocator, &available));
    assert(!qjs_intl_date_style_data_open(&shared, &allocator, &styles));
    assert(!qjs_intl_date_combination_data_open(&shared, &allocator, &combinations));
    assert(!qjs_intl_calendar_name_data_open(&shared, &allocator, &names));
    assert(!qjs_intl_date_interval_data_open(&shared, &allocator, &intervals));
    assert(!state.live);
    calls = state.calls;
    identity(&metadata, "gregory", "gregory", QJS_INTL_OK);
    identity(&metadata, "gregorian", "gregory", QJS_INTL_OK);
    identity(&metadata, "oldgreg", "gregory", QJS_INTL_OK);
    identity(&metadata, "earliest", "gregory", QJS_INTL_OK);
    identity(&metadata, "shared", "gregory", QJS_INTL_OK);
    identity(&metadata, "ethioaa", "ethioaa", QJS_INTL_OK);
    identity(&metadata, "ethiopic-amete-alem", "ethioaa", QJS_INTL_OK);
    identity(&metadata, "islamicc", "islamic-civil", QJS_INTL_OK);
    identity(&metadata, "islamic-civil", "islamic-civil", QJS_INTL_OK);
    identity(&metadata, "iso8601", "iso8601", QJS_INTL_OK);
    identity(&metadata, "retired", "retired", QJS_INTL_OK);
    index = UINT32_MAX;
    assert(!qjs_intl_locale_type_identity(&metadata, bytes("u"), bytes("ca"), bytes("buddhist"), &index) && !index);
    identity(&metadata, "clash", NULL, QJS_INTL_DATA_ERROR);
    identity(&metadata, "generic", NULL, QJS_INTL_UNSUPPORTED);
    identity(&metadata, "GREGORIAN", NULL, QJS_INTL_UNSUPPORTED);
    identity(&metadata, "greg", NULL, QJS_INTL_UNSUPPORTED);
    index = UINT32_MAX;
    assert(!qjs_intl_locale_type_identity(&metadata, bytes("u"), bytes("ca"),
                                         (QJSIntlBytes){"gregorian-extra", 9}, &index));
    index = UINT32_MAX;
    assert(qjs_intl_locale_type_identity(&metadata, bytes("u"), bytes("zz"), bytes("CODEPOINTS"), &index) == QJS_INTL_UNSUPPORTED);
    assert(!index);
    assert(qjs_intl_locale_type_identity(&metadata, invalid, bytes("ca"), bytes("gregory"), &index) == QJS_INTL_INVALID_ARGUMENT && !index);
    assert(qjs_intl_locale_type_identity(&metadata, bytes("u"), invalid, bytes("gregory"), &index) == QJS_INTL_INVALID_ARGUMENT && !index);
    assert(qjs_intl_locale_type_identity(&metadata, bytes("u"), bytes("ca"), invalid, &index) == QJS_INTL_INVALID_ARGUMENT && !index);
    assert(qjs_intl_locale_type_identity(NULL, bytes("u"), bytes("ca"), bytes("gregory"), &index) == QJS_INTL_INVALID_ARGUMENT && !index);
    assert(qjs_intl_locale_type_identity(&metadata, bytes("u"), bytes("ca"), bytes("gregory"), NULL) == QJS_INTL_INVALID_ARGUMENT);

    assert(!qjs_intl_date_available_calendar_index(&metadata, &available, bytes("gregory"), &index) && index == 1);
    assert(!qjs_intl_date_available_calendar_at(&available, index, &raw) && equal(raw, "gregorian"));
    assert(!qjs_intl_date_style_calendar_index(&metadata, &styles, bytes("gregory"), &index) && index == 2);
    assert(!qjs_intl_date_style_calendar_at(&styles, index, &raw) && equal(raw, "gregorian"));
    assert(!qjs_intl_date_combination_calendar_index(&metadata, &combinations, bytes("gregory"), &index) && !index);
    assert(!qjs_intl_date_combination_calendar_at(&combinations, index, &raw) && equal(raw, "oldgreg"));
    assert(!qjs_intl_date_name_calendar_index(&metadata, &names, bytes("gregory"), &index) && index == 2);
    assert(!qjs_intl_calendar_name_calendar_at(&names, index, &raw) && equal(raw, "gregorian"));
    index = UINT32_MAX;
    assert(qjs_intl_date_interval_calendar_index(&metadata, &intervals, bytes("gregory"), &index) == QJS_INTL_UNSUPPORTED && !index);
    assert(!qjs_intl_date_available_calendar_index(&metadata, &available, bytes("iso8601"), &index) && index == 2);
    assert(!qjs_intl_date_available_calendar_at(&available, index, &raw) && equal(raw, "iso8601"));
    assert(!qjs_intl_date_style_calendar_index(&metadata, &styles, bytes("iso8601"), &index) && index == 3);
    assert(qjs_intl_date_combination_calendar_index(&metadata, &combinations, bytes("iso8601"), &index) == QJS_INTL_UNSUPPORTED && !index);
    assert(!qjs_intl_date_name_calendar_index(&metadata, &names, bytes("iso8601"), &index) && index == 4);
    assert(!qjs_intl_date_interval_calendar_index(&metadata, &intervals, bytes("iso8601"), &index) && index == 2);
    assert(!qjs_intl_date_style_calendar_index(&metadata, &styles, bytes("ethioaa"), &index) && index == 1);
    assert(!qjs_intl_date_name_calendar_index(&metadata, &names, bytes("ethioaa"), &index) && !index);
    assert(!qjs_intl_date_available_calendar_index(&metadata, &available, bytes("earliest"), &index) && index == 1);
    assert(qjs_intl_date_available_calendar_index(&metadata, &available, bytes("generic"), &index) == QJS_INTL_UNSUPPORTED && !index);
    assert(qjs_intl_date_available_calendar_index(&metadata, &available, bytes("clash"), &index) == QJS_INTL_DATA_ERROR && !index);
    assert(qjs_intl_date_available_calendar_index(NULL, &available, bytes("gregory"), &index) == QJS_INTL_INVALID_ARGUMENT && !index);
    assert(qjs_intl_date_available_calendar_index(&metadata, NULL, bytes("gregory"), &index) == QJS_INTL_INVALID_ARGUMENT && !index);
    assert(qjs_intl_date_available_calendar_index(&metadata, &available, invalid, &index) == QJS_INTL_INVALID_ARGUMENT && !index);
    assert(qjs_intl_date_available_calendar_index(&metadata, &available, bytes("gregory"), NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_style_calendar_index(&metadata, NULL, bytes("gregory"), &index) == QJS_INTL_INVALID_ARGUMENT && !index);
    assert(qjs_intl_date_combination_calendar_index(&metadata, NULL, bytes("gregory"), &index) == QJS_INTL_INVALID_ARGUMENT && !index);
    assert(qjs_intl_date_name_calendar_index(&metadata, NULL, bytes("gregory"), &index) == QJS_INTL_INVALID_ARGUMENT && !index);
    assert(qjs_intl_date_interval_calendar_index(&metadata, NULL, bytes("gregory"), &index) == QJS_INTL_INVALID_ARGUMENT && !index);
    assert(state.calls == calls && !state.live);

    assert(!qjs_intl_data_open(calendar_ambiguous_bytes, calendar_ambiguous_length, &allocator, &shared));
    assert(!qjs_intl_locale_metadata_open(&shared, &allocator, &metadata));
    assert(!qjs_intl_date_available_data_open(&shared, &allocator, &available));
    calls = state.calls;
    index = UINT32_MAX;
    assert(qjs_intl_date_available_calendar_index(&metadata, &available, bytes("gregory"), &index) == QJS_INTL_DATA_ERROR && !index);
    /* A different target still has one unambiguous raw source identity. */
    assert(!qjs_intl_date_available_calendar_index(&metadata, &available, bytes("iso8601"), &index));
    assert(!qjs_intl_date_available_calendar_at(&available, index, &raw) && equal(raw, "iso8601"));
    assert(state.calls == calls && !state.live);
    return 0;
}
