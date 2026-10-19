/* Number/currency layouts: exact queries, sparse states, corruption and OOM.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/number/layout-data.h"
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

static QJSIntlBytes bytes(const char *value)
{
    return (QJSIntlBytes){value, strlen(value)};
}

static int equal(QJSIntlBytes value, const char *expected)
{
    return value.length == strlen(expected) && !memcmp(value.data, expected, value.length);
}

static uint32_t locale_id(const QJSIntlData *data, const char *expected)
{
    uint32_t i, parent;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes tag;
        assert(!qjs_intl_data_locale(data, i, &tag, &parent));
        if (equal(tag, expected))
            return i;
    }
    abort();
}

static void put_u32(uint8_t *ptr, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        ptr[i] = value >> (8 * i);
}

static void invalid_blob(const QJSIntlAllocator *allocator, const uint8_t *bytes, size_t length)
{
    QJSIntlData data;
    QJSIntlNumberingData numbering;
    QJSIntlNumberLayoutData result, zero = {0};
    assert(!qjs_intl_data_open(bytes, length, allocator, &data));
    assert(!qjs_intl_numbering_data_open(&data, allocator, &numbering));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_number_layout_data_open(&numbering, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
}

static void corrupt(const QJSIntlAllocator *allocator, size_t offset, uint32_t value)
{
    uint8_t *copy = malloc(qjs_intl_native_length);
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    invalid_blob(allocator, copy, qjs_intl_native_length);
    free(copy);
}

static void check_corruption(const QJSIntlNumberLayoutData *owner,
                             const QJSIntlAllocator *allocator)
{
    const QJSIntlData *data = &owner->numbering.data;
    size_t header = owner->section.bytes - data->pack.bytes;
    size_t maps = owner->rows - data->pack.bytes;
    size_t vectors = owner->vectors - data->pack.bytes;
    size_t locales = owner->locale_rows - data->pack.bytes;
    size_t i, last = 0;
    uint8_t *copy;
    for (i = 0; i < 4; i++) {
        corrupt(allocator, header + i * 4, 0);
        corrupt(allocator, header + i * 4, UINT32_MAX);
    }
    corrupt(allocator, owner->systems - data->pack.bytes, owner->numbering.systems.count);
    corrupt(allocator, locales, owner->row_count);
    corrupt(allocator, maps, owner->vector_count);
    corrupt(allocator, vectors, 0);
    corrupt(allocator, vectors, UINT32_MAX);
    corrupt(allocator, vectors, UINT32_C(0xfffffffe));
    copy = malloc(qjs_intl_native_length);
    assert(copy && owner->row_count > 1 && owner->vector_count > 1);
    for (i = 0; i < 2; i++) {
        size_t start = i ? vectors : maps;
        size_t width = (i ? QJS_INTL_LAYOUT_FIELD_COUNT : owner->system_count) * 4;
        memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
        memcpy(copy + start + width, copy + start, width);
        invalid_blob(allocator, copy, qjs_intl_native_length);
    }
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    for (i = 0; i < owner->section.count; i++)
        put_u32(copy + locales + i * 4, 0);
    invalid_blob(allocator, copy, qjs_intl_native_length);
    free(copy);
    for (i = 0; i < (size_t)owner->vector_count * QJS_INTL_LAYOUT_FIELD_COUNT; i++) {
        if (qjs_intl_read_u32(owner->vectors + i * 4) < data->strings.count)
            last = i;
    }
    corrupt(allocator, vectors + last * 4, data->strings.count);
    if (TEST_INTL_AUTHORED) {
        for (i = 0; i < data->strings.count; i++) {
            QJSIntlBytes value;
            assert(!qjs_intl_data_string(data, i, &value));
            if (equal(value, "↑↑↑") || equal(value, "∅∅∅"))
                corrupt(allocator, vectors + last * 4, i);
        }
    }
    for (i = 0; i < data->pack.section_count; i++) {
        size_t directory = 64 + i * 16;
        if (qjs_intl_read_u32(data->pack.bytes + directory) == QJS_INTL_SECTION_NUMBER_LAYOUTS) {
            copy = malloc(qjs_intl_native_length);
            assert(copy);
            corrupt(allocator, directory + 12, owner->section.count + 1);
            if (TEST_INTL_AUTHORED) {
                assert(i + 1 == data->pack.section_count);
                corrupt(allocator, directory, QJS_INTL_SECTION_NUMBER_LAYOUTS + 1);
            }
            /* Preserve the envelope and later sections while shortening this one. */
            {
                size_t cut = header + owner->section.length - 4;
                size_t j;
                memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
                memmove(copy + cut, copy + cut + 4, qjs_intl_native_length - cut - 4);
                put_u32(copy + 8, qjs_intl_native_length - 4);
                put_u32(copy + directory + 8, owner->section.length - 4);
                for (j = i + 1; j < data->pack.section_count; j++) {
                    size_t next = 64 + j * 16 + 4;
                    put_u32(copy + next, qjs_intl_read_u32(copy + next) - 4);
                }
                invalid_blob(allocator, copy, qjs_intl_native_length - 4);
            }
            free(copy);
        }
    }
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlNumberingData numbering;
    QJSIntlNumberLayoutData owner, retry;
    QJSIntlNumberLayouts values;
    uint32_t latin, mathbold, en, i;
    size_t calls;
    uint8_t *unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_numbering_data_open(&data, &allocator, &numbering));
    assert(!qjs_intl_number_layout_data_open(&numbering, &allocator, &owner));
    assert(!state.live && owner.system_count == (TEST_INTL_AUTHORED ? 2 : 78));
    assert(!qjs_intl_numbering_system_find(&numbering, bytes("latn"), &latin));
    assert(!qjs_intl_numbering_system_find(&numbering, bytes("mathbold"), &mathbold));
    en = locale_id(&data, "en");
    calls = state.calls;
    assert(!qjs_intl_number_layouts_at(&owner, en, latin, &values));
    assert(equal(values.fields[QJS_INTL_LAYOUT_APPROXIMATELY].text, "~{0}"));
    if (TEST_INTL_AUTHORED) {
        assert(
            equal(values.fields[QJS_INTL_LAYOUT_RANGE].text, "\xe2\x80\x8e{0} à {1}\xe2\x80\x8e"));
        assert(values.fields[QJS_INTL_LAYOUT_CURRENCY_UNIT_ONE].state == QJS_INTL_LAYOUT_BLOCKED);
        assert(!values.fields[QJS_INTL_LAYOUT_CURRENCY_UNIT_ONE].text.data);
        assert(values.fields[QJS_INTL_LAYOUT_CURRENCY_UNIT_TWO].state == QJS_INTL_LAYOUT_VALUE);
        assert(!values.fields[QJS_INTL_LAYOUT_CURRENCY_UNIT_TWO].text.length);
        assert(values.fields[QJS_INTL_LAYOUT_CURRENCY_UNIT_FEW].state == QJS_INTL_LAYOUT_ABSENT);
        assert(!values.fields[QJS_INTL_LAYOUT_CURRENCY_UNIT_FEW].text.data);
        assert(!values.fields[QJS_INTL_LAYOUT_CURRENCY_BEFORE_INSERT].text.length);
    } else {
        uint32_t arab;
        assert(equal(values.fields[QJS_INTL_LAYOUT_RANGE].text, "{0}–{1}"));
        assert(equal(values.fields[QJS_INTL_LAYOUT_CURRENCY_UNIT_OTHER].text, "{0} {1}"));
        assert(equal(values.fields[QJS_INTL_LAYOUT_CURRENCY_BEFORE_INSERT].text, " "));
        assert(!qjs_intl_numbering_system_find(&numbering, bytes("arab"), &arab));
        assert(!qjs_intl_number_layouts_at(&owner, locale_id(&data, "root"), arab, &values));
        assert(values.fields[QJS_INTL_LAYOUT_CURRENCY_UNIT_OTHER].state == QJS_INTL_LAYOUT_ABSENT);
        assert(equal(values.fields[QJS_INTL_LAYOUT_CURRENCY_AFTER_INSERT].text, " "));
        if (TEST_INTL_PROFILE) {
            assert(!qjs_intl_number_layouts_at(&owner, locale_id(&data, "fr"), latin, &values));
            assert(equal(values.fields[QJS_INTL_LAYOUT_APPROXIMATELY].text, "≈{0}"));
        }
    }
    assert(!qjs_intl_number_layouts_at(&owner, en, mathbold, &values));
    assert(equal(values.fields[QJS_INTL_LAYOUT_APPROXIMATELY].text, "~{0}"));
    for (i = 0; i < numbering.systems.count; i++) {
        QJSIntlNumberingSystem system;
        QJSIntlStatus status;
        assert(!qjs_intl_numbering_system_at(&numbering, i, &system));
        memset(&values, 0xff, sizeof(values));
        status = qjs_intl_number_layouts_at(&owner, en, i, &values);
        if (system.kind == QJS_INTL_NUMBERING_NUMERIC)
            assert(status == QJS_INTL_OK);
        else {
            assert(status == QJS_INTL_UNSUPPORTED);
            assert(!values.fields[0].text.data && !values.fields[0].state);
        }
    }
    assert(qjs_intl_number_layouts_at(&owner, data.locales.count, latin, &values) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!values.fields[0].text.data);
    assert(qjs_intl_number_layouts_at(&owner, en, numbering.systems.count, &values) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_layouts_at(NULL, en, latin, &values) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_layouts_at(&owner, en, latin, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(state.calls == calls);
    state.fail_at = state.calls + 1;
    assert(qjs_intl_number_layout_data_open(&numbering, &allocator, &retry) == QJS_INTL_NO_MEMORY);
    assert(!retry.section.bytes && !state.live);
    state.fail_at = 0;
    assert(!qjs_intl_number_layout_data_open(&numbering, &allocator, &retry));
    assert(qjs_intl_number_layout_data_open(NULL, &allocator, &retry) == QJS_INTL_INVALID_ARGUMENT);
    assert(!retry.section.bytes);
    assert(qjs_intl_number_layout_data_open(&numbering, NULL, &retry) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_number_layout_data_open(&numbering, &allocator, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_numbering_data_open(&data, &allocator, &numbering));
    assert(!qjs_intl_number_layout_data_open(&numbering, &allocator, &owner));
    check_corruption(&owner, &allocator);
    assert(!state.live);
    free(unaligned);
    return 0;
}
