/* Division grammar source provenance, corruption, lifetime and allocation failure.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/unit/grammar-data.h"
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

static int equal(QJSIntlBytes text, const char *value)
{
    return text.length == strlen(value) && (!text.length || !memcmp(text.data, value, text.length));
}

static uint32_t locale_id(const QJSIntlData *data, const char *value)
{
    uint32_t i, parent;
    for (i = 0; i < data->locales.count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_data_locale(data, i, &text, &parent));
        if (equal(text, value))
            return i;
    }
    abort();
}

static void expect(const QJSIntlUnitGrammarData *data, const char *locale,
                    QJSIntlUnitGrammarFeature feature, const char *origin,
                    const char *first, const char *second)
{
    QJSIntlUnitGrammarRule result;
    assert(!qjs_intl_unit_grammar_at(data, locale_id(&data->data, locale), feature, &result));
    assert(equal(result.locale, origin) && equal(result.first, first) && equal(result.second, second));
}

static void put_u32(uint8_t *ptr, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        ptr[i] = value >> (i * 8);
}

static void corrupt(const QJSIntlAllocator *allocator, size_t offset, uint32_t value)
{
    uint8_t *copy = malloc(qjs_intl_native_length);
    QJSIntlData data;
    QJSIntlUnitGrammarData result, zero = {0};
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_unit_grammar_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
    free(copy);
}

static void corruption(const QJSIntlUnitGrammarData *data, const QJSIntlAllocator *allocator)
{
    size_t header = data->section.bytes - data->data.pack.bytes;
    size_t rows = data->rows - data->data.pack.bytes;
    uint32_t i, j;
    corrupt(allocator, header, 0);
    corrupt(allocator, header, 2);
    corrupt(allocator, header + 4, 0);
    corrupt(allocator, header + 4, UINT32_MAX);
    corrupt(allocator, data->indices - data->data.pack.bytes, data->row_count);
    for (i = 0; i < data->data.pack.section_count; i++) {
        size_t directory = 64 + (size_t)i * 16;
        if (qjs_intl_read_u32(data->data.pack.bytes + directory) == QJS_INTL_SECTION_UNIT_GRAMMAR) {
            corrupt(allocator, directory + 12, data->section.count + 1);
            break;
        }
    }
    for (j = 0; j < 6; j++) {
        corrupt(allocator, rows + j * 4, data->data.strings.count);
        corrupt(allocator, rows + j * 4, 0);
    }
    for (i = 0; i < data->data.strings.count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_data_string(&data->data, i, &text));
        if (equal(text, "0") || equal(text, "En") || equal(text, "en-us") ||
            equal(text, "bad_tag") || equal(text, "en-1901-1901"))
            corrupt(allocator, rows, i);
        if (equal(text, "0") || equal(text, "nominative") || equal(text, "↑↑↑"))
            corrupt(allocator, rows + 4, i);
        if (equal(text, "0") || equal(text, "bad_tag") || equal(text, "↑↑↑"))
            corrupt(allocator, rows + 16, i);
    }
    if (data->row_count > 1) {
        uint8_t *copy = malloc(qjs_intl_native_length);
        uint8_t first[24];
        QJSIntlData base;
        QJSIntlUnitGrammarData result;
        assert(copy);
        memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
        memcpy(copy + rows + 24, copy + rows, 24);
        assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &base));
        assert(qjs_intl_unit_grammar_data_open(&base, allocator, &result) == QJS_INTL_DATA_ERROR);
        memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
        memcpy(first, copy + rows, 24);
        memcpy(copy + rows, copy + rows + 24, 24);
        memcpy(copy + rows + 24, first, 24);
        assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &base));
        assert(qjs_intl_unit_grammar_data_open(&base, allocator, &result) == QJS_INTL_DATA_ERROR);
        memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
        memset(copy + (data->indices - data->data.pack.bytes), 0, (size_t)data->section.count * 4);
        assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &base));
        assert(qjs_intl_unit_grammar_data_open(&base, allocator, &result) == QJS_INTL_DATA_ERROR);
        free(copy);
    }
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlUnitGrammarData owner, result, zero = {0};
    QJSIntlUnitGrammarRule rule, empty = {0};
    uint32_t i;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_unit_grammar_data_open(&data, &allocator, &owner));
    expect(&owner, "root", QJS_INTL_UNIT_GRAMMAR_PLURAL, "root", "compound", "one");
    expect(&owner, "en", QJS_INTL_UNIT_GRAMMAR_CASE, "root", "compound", "nominative");
#if TEST_INTL_AUTHORED || TEST_INTL_PROFILE
    expect(&owner, "de", QJS_INTL_UNIT_GRAMMAR_CASE, "de", "compound", "accusative");
    expect(&owner, "fr", QJS_INTL_UNIT_GRAMMAR_PLURAL, "fr", "compound", "one");
    expect(&owner, "fr", QJS_INTL_UNIT_GRAMMAR_CASE, "root", "compound", "nominative");
#endif
    for (i = 0; i < data.locales.count; i++) {
        unsigned int feature;
        for (feature = 0; feature < QJS_INTL_UNIT_GRAMMAR_FEATURE_COUNT; feature++)
            assert(!qjs_intl_unit_grammar_at(&owner, i, feature, &rule));
    }
    memset(&rule, 0xff, sizeof(rule));
    assert(qjs_intl_unit_grammar_at(&owner, data.locales.count, 0, &rule) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&rule, &empty, sizeof(rule)));
    assert(qjs_intl_unit_grammar_at(&owner, 0, -1, &rule) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_grammar_at(NULL, 0, 0, &rule) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_grammar_at(&owner, 0, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(!state.live);
    corruption(&owner, &allocator);
    state.fail_at = state.calls + 1;
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_unit_grammar_data_open(&data, &allocator, &result) == QJS_INTL_NO_MEMORY);
    assert(!memcmp(&result, &zero, sizeof(result)) && !state.live);
    state.fail_at = 0;
    assert(qjs_intl_unit_grammar_data_open(NULL, &allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_grammar_data_open(&data, NULL, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_unit_grammar_data_open(&data, &allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    {
        QJSIntlData missing = data;
        QJSIntlSection section;
        size_t index;
        uint8_t *copy = malloc(qjs_intl_native_length);
        assert(copy);
        memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
        assert(!qjs_intl_pack_section(&data.pack, QJS_INTL_SECTION_UNIT_GRAMMAR, &section));
        /* Keep the directory ordered when later sections are present, while
           removing this identity from an otherwise valid shared pack. */
        for (index = 0; index < data.pack.section_count; index++) {
            uint8_t *entry = copy + 64 + index * 16;
            uint32_t identity = qjs_intl_read_u32(entry);
            if (identity >= QJS_INTL_SECTION_UNIT_GRAMMAR) {
                assert(identity < UINT32_MAX);
                put_u32(entry, identity + 1);
            }
        }
        assert(!qjs_intl_data_open(copy, qjs_intl_native_length, &allocator, &missing));
        assert(qjs_intl_pack_section(&missing.pack, QJS_INTL_SECTION_UNIT_GRAMMAR, &section));
        assert(qjs_intl_unit_grammar_data_open(&missing, &allocator, &result) == QJS_INTL_DATA_ERROR);
        free(copy);
    }
#if TEST_INTL_AUTHORED
    {
        extern const uint8_t qjs_intl_grammar_projected_bytes[];
        extern const size_t qjs_intl_grammar_projected_length;
        QJSIntlUnitGrammarRule borrowed;
        size_t calls;
        assert(!qjs_intl_data_open(qjs_intl_grammar_projected_bytes, qjs_intl_grammar_projected_length,
                                    &allocator, &data));
        calls = state.calls;
        assert(!qjs_intl_unit_grammar_data_open(&data, &allocator, &owner));
        assert(state.calls == calls + 2 && !state.live); /* Owner and source-variant scratch. */
        expect(&owner, "de", QJS_INTL_UNIT_GRAMMAR_PLURAL, "en-1901-1996", "one", "other");
        assert(!qjs_intl_unit_grammar_at(&owner, locale_id(&data, "de"),
                                         QJS_INTL_UNIT_GRAMMAR_PLURAL, &borrowed));
        memset(&owner, 0, sizeof(owner));
        memset(&data, 0, sizeof(data));
        assert(equal(borrowed.locale, "en-1901-1996") && equal(borrowed.first, "one"));
        assert(!qjs_intl_data_open(qjs_intl_grammar_projected_bytes, qjs_intl_grammar_projected_length,
                                    &allocator, &data));
        for (i = 1; i <= 2; i++) {
            state.fail_at = state.calls + i;
            memset(&result, 0xff, sizeof(result));
            assert(qjs_intl_unit_grammar_data_open(&data, &allocator, &result) == QJS_INTL_NO_MEMORY);
            assert(!memcmp(&result, &zero, sizeof(result)) && !state.live);
        }
        state.fail_at = 0;
    }
#endif
    assert(!state.live);
    return 0;
}
