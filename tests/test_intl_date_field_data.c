/* Shared immutable date labels: exact queries, corruption and allocation failure.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/field-data.h"
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

static void corrupt(const QJSIntlDateFieldData *owner, const QJSIntlAllocator *allocator,
                    const uint8_t *at, uint32_t value)
{
    QJSIntlData data;
    QJSIntlDateFieldData result, zero = {0};
    uint8_t *copy = malloc(qjs_intl_native_length);
    size_t offset = at - owner->data.pack.bytes;
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_date_field_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
    free(copy);
}

static void check_corruption(const QJSIntlDateFieldData *owner, const QJSIntlAllocator *allocator)
{
    const uint8_t *value = NULL, *missing = NULL, *blocked = NULL;
    size_t i;
    for (i = 0; i < 4; i++) {
        corrupt(owner, allocator, owner->section.bytes + i * 4, UINT32_MAX);
        corrupt(owner, allocator, owner->section.bytes + i * 4, 0);
    }
    corrupt(owner, allocator, owner->alternates, 1);
    corrupt(owner, allocator, owner->alternates + 4, owner->data.strings.count);
    corrupt(owner, allocator, owner->alternates + 4, string_id(&owner->data, "bad!"));
    corrupt(owner, allocator, owner->locale_rows, owner->row_count);
    corrupt(owner, allocator, owner->rows, owner->record_count);
    for (i = 0; i < owner->record_count; i++) {
        const uint8_t *record = owner->records + i * 24;
        uint32_t state = qjs_intl_read_u32(record);
        if (state == QJS_INTL_DATE_FIELD_VALUE)
            value = record;
        else if (state == QJS_INTL_DATE_FIELD_MISSING)
            missing = record;
        else
            blocked = record;
    }
    assert(value && missing && blocked);
    corrupt(owner, allocator, value, 3);
    corrupt(owner, allocator, value + 4, owner->data.strings.count);
    corrupt(owner, allocator, value + 8, owner->data.strings.count);
    corrupt(owner, allocator, value + 12, QJS_INTL_DATE_FIELD_COUNT);
    corrupt(owner, allocator, value + 16, QJS_INTL_DATE_FIELD_WIDTH_COUNT);
    corrupt(owner, allocator, value + 20, owner->alternate_count);
    corrupt(owner, allocator, value + 4, string_id(&owner->data, "\xe2\x86\x91\xe2\x86\x91\xe2\x86\x91"));
    corrupt(owner, allocator, value + 4, string_id(&owner->data, "\xe2\x88\x85\xe2\x88\x85\xe2\x88\x85"));
    corrupt(owner, allocator, blocked + 4, string_id(&owner->data, "root year"));
    corrupt(owner, allocator, missing + 12, 1);
    for (i = 0; i < 5; i++) {
        static const char *const bad[] = {"", "En", "en-us", "bad_tag", "en-1901-1901"};
        corrupt(owner, allocator, value + 8, string_id(&owner->data, bad[i]));
    }
}
#endif

static void check_queries(const QJSIntlDateFieldData *owner)
{
    QJSIntlDateFieldLabel label, zero = {0};
    QJSIntlBytes text, empty = {0};
    uint32_t locale, field, width, alt;
    assert(owner->alternate_count == 2);
    assert(!qjs_intl_date_field_alternate_at(owner, 0, &text) && equal(text, ""));
    assert(!qjs_intl_date_field_alternate_at(owner, 1, &text) && equal(text, "variant"));
    for (locale = 0; locale < owner->section.count; locale++) {
        for (field = 0; field < QJS_INTL_DATE_FIELD_COUNT; field++) {
            for (width = 0; width < QJS_INTL_DATE_FIELD_WIDTH_COUNT; width++) {
                for (alt = 0; alt < owner->alternate_count; alt++) {
                    assert(!qjs_intl_date_field_label_at(owner, locale, field, width, alt, &label));
                    assert(label.state <= QJS_INTL_DATE_FIELD_BLOCKED);
                    if (label.state == QJS_INTL_DATE_FIELD_MISSING)
                        assert(!memcmp(&label, &zero, sizeof(label)));
                    else {
                        assert(label.origin_locale.length);
                        assert(label.origin_field < QJS_INTL_DATE_FIELD_COUNT);
                        assert(label.origin_width < QJS_INTL_DATE_FIELD_WIDTH_COUNT);
                        if (label.state == QJS_INTL_DATE_FIELD_BLOCKED)
                            assert(!label.text.length);
                    }
                }
            }
        }
    }
#if TEST_INTL_AUTHORED
    locale = locale_id(&owner->data, "en-GB");
    assert(!qjs_intl_date_field_label_at(owner, locale, QJS_INTL_DATE_YEAR,
                                        QJS_INTL_DATE_FIELD_NARROW, 0, &label));
    assert(equal(label.text, "British year") && equal(label.origin_locale, "en-GB"));
    assert(label.origin_field == QJS_INTL_DATE_YEAR && label.origin_width == QJS_INTL_DATE_FIELD_LONG);
    assert(!qjs_intl_date_field_label_at(owner, locale, QJS_INTL_DATE_DAY_PERIOD,
                                        QJS_INTL_DATE_FIELD_LONG, 1, &label));
    assert(equal(label.text, "am/pm") && equal(label.origin_alternate, "variant"));
    assert(!qjs_intl_date_field_label_at(owner, locale, QJS_INTL_DATE_MONTH,
                                        QJS_INTL_DATE_FIELD_LONG, 0, &label));
    assert(label.state == QJS_INTL_DATE_FIELD_BLOCKED && equal(label.origin_locale, "en"));
    assert(!qjs_intl_date_field_label_at(owner, locale, QJS_INTL_DATE_ERA,
                                        QJS_INTL_DATE_FIELD_LONG, 0, &label));
    assert(label.state == QJS_INTL_DATE_FIELD_VALUE && !label.text.length);
    assert(!qjs_intl_date_field_label_at(owner, locale_id(&owner->data, "de"), QJS_INTL_DATE_TIME_ZONE_NAME,
                                        QJS_INTL_DATE_FIELD_SHORT, 0, &label));
    assert(equal(label.text, "external time zone") && equal(label.origin_locale, "en-1901-1996"));
#else
    assert(!qjs_intl_date_field_label_at(owner, locale_id(&owner->data, "en"), QJS_INTL_DATE_YEAR,
                                        QJS_INTL_DATE_FIELD_LONG, 0, &label));
    assert(label.state == QJS_INTL_DATE_FIELD_VALUE && equal(label.text, "year"));
#endif
    memset(&label, 0xff, sizeof(label));
    assert(qjs_intl_date_field_label_at(owner, UINT32_MAX, 0, 0, 0, &label) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&label, &zero, sizeof(label)));
    assert(qjs_intl_date_field_label_at(owner, 0, -1, 0, 0, &label) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_field_label_at(owner, 0, 0, -1, 0, &label) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_field_label_at(owner, 0, 0, 0, owner->alternate_count, &label) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_field_label_at(NULL, 0, 0, 0, 0, &label) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_field_label_at(owner, 0, 0, 0, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_field_alternate_at(owner, owner->alternate_count, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&text, &empty, sizeof(text)));
    assert(qjs_intl_date_field_alternate_at(NULL, 0, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_field_alternate_at(owner, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data, unaligned_data;
    QJSIntlDateFieldData owner, result, zero = {0};
    uint8_t *unaligned = malloc(qjs_intl_native_length + 1);
    size_t calls, step;
    assert(unaligned && !qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length,
                                           &allocator, &data) && !state.live);
    state.calls = 0;
    assert(!qjs_intl_date_field_data_open(&data, &allocator, &owner) && !state.live);
    calls = state.calls;
    assert(calls);
    check_queries(&owner);
    assert(state.calls == calls); /* Every query is allocation-free. */
#if TEST_INTL_AUTHORED
    assert(calls >= 2); /* Validation scratch and multi-variant source scratch. */
    check_corruption(&owner, &allocator);
#endif
    for (step = 1; step <= calls; step++) {
        state.calls = 0;
        state.fail_at = step;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_date_field_data_open(&data, &allocator, &result) == QJS_INTL_NO_MEMORY);
        assert(!state.live && !memcmp(&result, &zero, sizeof(result)));
        state.fail_at = 0;
        assert(!qjs_intl_date_field_data_open(&data, &allocator, &result) && !state.live);
    }
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &unaligned_data));
    assert(!qjs_intl_date_field_data_open(&unaligned_data, &allocator, &result));
    check_queries(&result);
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_date_field_data_open(NULL, &allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(qjs_intl_date_field_data_open(&data, NULL, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_date_field_data_open(&data, &allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);
    free(unaligned);
    assert(!state.live);
    return 0;
}
