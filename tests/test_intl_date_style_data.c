/* Shared immutable date/time styles: exact queries, corruption and allocation failure.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/style-data.h"
#include "../src/intl/native/wire.h"
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

static int equal(QJSIntlBytes text, const char *expected)
{
    return text.length == strlen(expected) &&
           (!text.length || !memcmp(text.data, expected, text.length));
}

static uint32_t locale_id(const QJSIntlData *data, const char *name)
{
    uint32_t i, parent;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_data_locale(data, i, &text, &parent));
        if (equal(text, name))
            return i;
    }
    abort();
}

#if TEST_INTL_AUTHORED
static uint32_t string_id(const QJSIntlData *data, const char *name)
{
    uint32_t i;
    for (i = 0; i < data->strings.count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_data_string(data, i, &text));
        if (equal(text, name))
            return i;
    }
    abort();
}

static void put_u32(uint8_t *at, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        at[i] = value >> (i * 8);
}

static void corrupt(const QJSIntlDateStyleData *owner, const QJSIntlAllocator *allocator,
                    const uint8_t *at, uint32_t value)
{
    QJSIntlData data;
    QJSIntlDateStyleData result, zero = {0};
    uint8_t *copy = malloc(qjs_intl_native_length);
    size_t offset = at - owner->data.pack.bytes;
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_date_style_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
    free(copy);
}

static void duplicate(const QJSIntlDateStyleData *owner, const QJSIntlAllocator *allocator,
                      const uint8_t *at, size_t length)
{
    QJSIntlData data;
    QJSIntlDateStyleData result, zero = {0};
    uint8_t *copy = malloc(qjs_intl_native_length);
    size_t offset = at - owner->data.pack.bytes;
    assert(copy && offset >= length && offset <= qjs_intl_native_length - length);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    memcpy(copy + offset, copy + offset - length, length);
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_date_style_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
    free(copy);
}

#endif

static uint32_t calendar_id(const QJSIntlDateStyleData *owner, const char *name)
{
    uint32_t i;
    for (i = 0; i < owner->calendar_count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_date_style_calendar_at(owner, i, &text));
        if (equal(text, name))
            return i;
    }
    abort();
}

#if TEST_INTL_AUTHORED
static void check_corruption(const QJSIntlDateStyleData *owner, const QJSIntlAllocator *allocator)
{
    const uint8_t *value = NULL, *missing = NULL, *blocked = NULL;
    size_t i;
    for (i = 0; i < 5; i++) {
        corrupt(owner, allocator, owner->section.bytes + i * 4, UINT32_MAX);
        corrupt(owner, allocator, owner->section.bytes + i * 4, 0);
    }
    corrupt(owner, allocator, owner->calendars, owner->data.strings.count);
    corrupt(owner, allocator, owner->calendars, string_id(&owner->data, "Generic"));
    corrupt(owner, allocator, owner->calendars + 4, qjs_intl_read_u32(owner->calendars));
    corrupt(owner, allocator, owner->calendars + 8, string_id(&owner->data, "japanese-"));
    corrupt(owner, allocator, owner->alternates, 1);
    corrupt(owner, allocator, owner->alternates + 4, owner->data.strings.count);
    corrupt(owner, allocator, owner->alternates + 4, string_id(&owner->data, "bad!"));
    corrupt(owner, allocator, owner->locale_rows, owner->row_count);
    corrupt(owner, allocator, owner->rows, owner->record_count);
    for (i = 0; i < owner->record_count; i++) {
        const uint8_t *record = owner->records + i * 36;
        uint32_t state = qjs_intl_read_u32(record);
        if (state == QJS_INTL_DATE_STYLE_VALUE)
            value = record;
        else if (state == QJS_INTL_DATE_STYLE_MISSING)
            missing = record;
        else
            blocked = record;
    }
    assert(value && missing && blocked);
    corrupt(owner, allocator, value, 3);
    for (i = 1; i <= 3; i++)
        corrupt(owner, allocator, value + i * 4, owner->data.strings.count);
    corrupt(owner, allocator, value + 16, owner->calendar_count);
    corrupt(owner, allocator, value + 20, QJS_INTL_DATE_STYLE_FAMILY_COUNT);
    corrupt(owner, allocator, value + 24, QJS_INTL_DATE_STYLE_WIDTH_COUNT);
    corrupt(owner, allocator, value + 28, QJS_INTL_DATE_STYLE_FIELD_COUNT);
    corrupt(owner, allocator, value + 32, owner->alternate_count);
    corrupt(owner, allocator, value + 4, string_id(&owner->data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91"));
    corrupt(owner, allocator, value + 4, string_id(&owner->data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85"));
    corrupt(owner, allocator, blocked + 4, string_id(&owner->data, "y-MM-dd"));
    corrupt(owner, allocator, blocked + 8, string_id(&owner->data, "latn"));
    corrupt(owner, allocator, missing + 16, 1);
    for (i = 0; i < 5; i++) {
        static const char *const bad[] = {"", "En", "en-us", "bad_tag", "en-1901-1901"};
        corrupt(owner, allocator, value + 12, string_id(&owner->data, bad[i]));
    }
    /* Strict ordering also excludes duplicate shared records and rows. */
    duplicate(owner, allocator, owner->records + 36, 36);
    duplicate(owner, allocator, owner->rows + (size_t)owner->row_width * 4,
              (size_t)owner->row_width * 4);
    assert(owner->row_count == owner->section.count);
    corrupt(owner, allocator, owner->locale_rows, qjs_intl_read_u32(owner->locale_rows + 4));
}

#endif

static void get(const QJSIntlDateStyleData *owner, const char *locale, const char *calendar,
                QJSIntlDateStyleFamily family, QJSIntlDateStyleField field, uint32_t alternate,
                QJSIntlDateStyleRecord *out)
{
    assert(!qjs_intl_date_style_record_at(owner, locale_id(&owner->data, locale),
                                         calendar_id(owner, calendar), family,
                                         QJS_INTL_DATE_STYLE_LONG, field, alternate, out));
}

static void check_queries(const QJSIntlDateStyleData *owner)
{
    QJSIntlDateStyleRecord record, zero = {0};
    QJSIntlBytes text, empty = {0};
    uint32_t locale, index;
#if TEST_INTL_AUTHORED
    assert(owner->calendar_count == 3 && owner->alternate_count == 2);
    assert(!qjs_intl_date_style_calendar_at(owner, 0, &text) && equal(text, "generic"));
    assert(!qjs_intl_date_style_alternate_at(owner, 0, &text) && equal(text, ""));
    assert(!qjs_intl_date_style_alternate_at(owner, 1, &text) && equal(text, "variant"));
#endif
    for (locale = 0; locale < owner->section.count; locale++) {
        for (index = 0; index < owner->row_width; index++) {
            uint32_t selector = index, alternate = selector % owner->alternate_count;
            uint32_t field, width, family, calendar;
            selector /= owner->alternate_count;
            field = selector % QJS_INTL_DATE_STYLE_FIELD_COUNT;
            selector /= QJS_INTL_DATE_STYLE_FIELD_COUNT;
            width = selector % QJS_INTL_DATE_STYLE_WIDTH_COUNT;
            selector /= QJS_INTL_DATE_STYLE_WIDTH_COUNT;
            family = selector % QJS_INTL_DATE_STYLE_FAMILY_COUNT;
            calendar = selector / QJS_INTL_DATE_STYLE_FAMILY_COUNT;
            assert(!qjs_intl_date_style_record_at(owner, locale, calendar, family, width,
                                                 field, alternate, &record));
            assert(record.state <= QJS_INTL_DATE_STYLE_BLOCKED);
            if (record.state == QJS_INTL_DATE_STYLE_MISSING) {
                assert(!memcmp(&record, &zero, sizeof(record)));
            } else {
                assert(record.origin_locale.length && record.origin_calendar.length);
                assert(record.origin_family < QJS_INTL_DATE_STYLE_FAMILY_COUNT);
                assert(record.origin_width < QJS_INTL_DATE_STYLE_WIDTH_COUNT);
                assert(record.origin_field < QJS_INTL_DATE_STYLE_FIELD_COUNT);
                if (record.state == QJS_INTL_DATE_STYLE_BLOCKED)
                    assert(!record.text.length && !record.numbers.length);
            }
        }
    }
#if TEST_INTL_AUTHORED
    get(owner, "en-GB", "japanese", 0, 0, 0, &record);
    assert(equal(record.text, "dd/MM/y") && equal(record.origin_locale, "en-GB"));
    assert(equal(record.origin_calendar, "gregorian") && !record.numbers.length);
    assert(record.origin_family == QJS_INTL_DATE_STYLE_DATE &&
           record.origin_width == QJS_INTL_DATE_STYLE_LONG &&
           record.origin_field == QJS_INTL_DATE_STYLE_PATTERN);
    get(owner, "en-GB", "japanese", 0, 1, 0, &record);
    assert(equal(record.text, "yMMdd") && equal(record.numbers, "latn") &&
           equal(record.origin_locale, "root") && record.origin_field == QJS_INTL_DATE_STYLE_SKELETON);
    get(owner, "root", "gregorian", 0, 0, 0, &record);
    assert(equal(record.text, "y-MM-dd") && equal(record.numbers, "y=hebr"));
    get(owner, "en-GB", "gregorian", 0, 0, 1, &record);
    assert(equal(record.text, "d MMMM y") && equal(record.numbers, "latn") &&
           equal(record.origin_alternate, "variant"));
    get(owner, "en", "gregorian", 0, 0, 0, &record);
    assert(record.state == QJS_INTL_DATE_STYLE_VALUE && !record.text.length);
    get(owner, "en-GB", "japanese", 1, 0, 0, &record);
    assert(record.state == QJS_INTL_DATE_STYLE_BLOCKED && equal(record.origin_locale, "root"));
    get(owner, "de", "gregorian", 0, 0, 0, &record);
    assert(equal(record.text, "y 'external' 😀") && equal(record.origin_locale, "en-1901-1996"));
#else
    get(owner, "en", "gregorian", QJS_INTL_DATE_STYLE_DATE, QJS_INTL_DATE_STYLE_PATTERN, 0, &record);
    assert(record.state == QJS_INTL_DATE_STYLE_VALUE && equal(record.text, "MMMM d, y"));
    assert(!record.numbers.length && equal(record.origin_locale, "en"));
    get(owner, "en", "gregorian", QJS_INTL_DATE_STYLE_DATE, QJS_INTL_DATE_STYLE_SKELETON, 0, &record);
    assert(equal(record.text, "yMMMMd"));
    get(owner, "en", "gregorian", QJS_INTL_DATE_STYLE_TIME, QJS_INTL_DATE_STYLE_PATTERN, 0, &record);
    assert(equal(record.text, "h:mm:ss a z"));
#endif
    memset(&record, 0xff, sizeof(record));
    assert(qjs_intl_date_style_record_at(owner, UINT32_MAX, 0, 0, 0, 0, 0, &record) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&record, &zero, sizeof(record)));
    assert(qjs_intl_date_style_record_at(owner, 0, owner->calendar_count, 0, 0, 0, 0, &record) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_style_record_at(owner, 0, 0, -1, 0, 0, 0, &record) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_style_record_at(owner, 0, 0, 0, -1, 0, 0, &record) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_style_record_at(owner, 0, 0, 0, 0, -1, 0, &record) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_style_record_at(owner, 0, 0, 0, 0, 0, owner->alternate_count, &record) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_style_record_at(NULL, 0, 0, 0, 0, 0, 0, &record) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_style_record_at(owner, 0, 0, 0, 0, 0, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_style_calendar_at(owner, owner->calendar_count, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&text, &empty, sizeof(text)));
    assert(qjs_intl_date_style_calendar_at(NULL, 0, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_style_calendar_at(owner, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_style_alternate_at(owner, owner->alternate_count, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&text, &empty, sizeof(text)));
    assert(qjs_intl_date_style_alternate_at(NULL, 0, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_style_alternate_at(owner, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data, unaligned_data;
    QJSIntlDateStyleData owner, result, zero = {0};
    uint8_t *unaligned = malloc(qjs_intl_native_length + 1);
    size_t calls, step;
    assert(unaligned && !qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length,
                                         &allocator, &data) && !state.live);
    state.calls = 0;
    assert(!qjs_intl_date_style_data_open(&data, &allocator, &owner) && !state.live);
    calls = state.calls;
    assert(calls);
#if TEST_INTL_AUTHORED
    assert(calls >= 2);
#endif
    check_queries(&owner);
    assert(state.calls == calls);
#if TEST_INTL_AUTHORED
    check_corruption(&owner, &allocator);
#endif
    for (step = 1; step <= calls; step++) {
        state.calls = 0;
        state.fail_at = step;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_date_style_data_open(&data, &allocator, &result) == QJS_INTL_NO_MEMORY);
        assert(!state.live && !memcmp(&result, &zero, sizeof(result)));
        state.fail_at = 0;
        assert(!qjs_intl_date_style_data_open(&data, &allocator, &result) && !state.live);
    }
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &unaligned_data));
    assert(!qjs_intl_date_style_data_open(&unaligned_data, &allocator, &result));
    check_queries(&result);
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_date_style_data_open(NULL, &allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(qjs_intl_date_style_data_open(&data, NULL, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_style_data_open(&data, &allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);
    free(unaligned);
    assert(!state.live);
    return 0;
}
