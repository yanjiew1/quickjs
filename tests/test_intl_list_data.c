/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/list/data.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    int fail;
    unsigned int live;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *result;
    assert(size > 0);
    if (state->fail)
        return NULL;
    result = malloc(size);
    if (result)
        state->live++;
    return result;
}

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    assert(ptr && state->live);
    state->live--;
    free(ptr);
}

static void put_u32(uint8_t *bytes, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        bytes[i] = value >> (i * 8);
}

static void corrupt_word(const QJSIntlAllocator *allocator, size_t offset, uint32_t value)
{
    QJSIntlData base;
    QJSIntlListData data;
    uint8_t *copy = malloc(qjs_intl_native_length);
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    assert(qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &base) == QJS_INTL_OK);
    assert(qjs_intl_list_data_open(&base, allocator, &data) == QJS_INTL_DATA_ERROR);
    assert(!data.data.pack.bytes && !data.data.strings.bytes && !data.data.locales.bytes &&
           !data.lists.bytes && !data.list_contexts.bytes);
    free(copy);
}

static void test_contexts(const QJSIntlListData *data, const QJSIntlAllocator *allocator)
{
    const QJSIntlSection *section = &data->list_contexts;
    size_t at = section->bytes - data->data.pack.bytes;
    size_t vectors = at + 4 + (size_t)section->count * 4;
    unsigned int contexts = 0, kind, variant;
    uint32_t index, english = UINT32_MAX, spanish = UINT32_MAX;
    QJSIntlBytes pair, end, tag;

    assert(section->bytes && section->count == data->data.locales.count);
    corrupt_word(allocator, at, 0);
    corrupt_word(allocator, at + 4, qjs_intl_read_u32(section->bytes));
    corrupt_word(allocator, vectors, 4);
    corrupt_word(allocator, vectors + 4, data->data.strings.count);
    corrupt_word(allocator, vectors + 4, 1); /* disabled must have zero IDs */
    for (index = 0; index < data->data.locales.count; index++) {
        uint32_t parent;
        assert(qjs_intl_data_locale(&data->data, index, &tag, &parent) == QJS_INTL_OK);
        if (tag.length == 2 && !memcmp(tag.data, "en", 2))
            english = index;
        if (tag.length == 2 && !memcmp(tag.data, "es", 2))
            spanish = index;
        for (variant = 0; variant < 9; variant++) {
            assert(qjs_intl_list_data_context(data, index, variant, &kind, &pair, &end) == QJS_INTL_OK);
            assert(kind <= 3);
            if (kind) {
                assert(pair.data && end.data && pair.length >= 6 && end.length >= 6);
                contexts++;
            } else {
                assert(!pair.data && !end.data && !pair.length && !end.length);
            }
        }
    }
    assert(english != UINT32_MAX);
    assert(TEST_INTL_PROFILE ? contexts > 200 : contexts == 0);
    if (TEST_INTL_PROFILE) {
        uint32_t set;
        size_t first;
        assert(spanish != UINT32_MAX);
        set = qjs_intl_read_u32(section->bytes + 4 + (size_t)spanish * 4);
        /* Valid in-range vector references still cannot select Spanish
           contexts for English, or replace a Spanish conjunction by 'u'. */
        corrupt_word(allocator, at + 4 + (size_t)english * 4, set);
        first = vectors + (size_t)set * 27 * 4;
        corrupt_word(allocator, first, 2);
        assert(qjs_intl_list_data_context(data, spanish, 0, &kind, &pair, &end) == QJS_INTL_OK);
        assert(kind == 1 && pair.length == 9 && !memcmp(pair.data, "{0} e {1}", 9));
    }
    kind = 99;
    assert(qjs_intl_list_data_context(data, english, 9, &kind, &pair, &end) == QJS_INTL_INVALID_ARGUMENT);
    assert(!kind && !pair.data && !end.data);
    assert(qjs_intl_list_data_context(NULL, 0, 0, &kind, &pair, &end) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_list_data_context(data, english, 0, NULL, &pair, &end) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_list_data_context(data, english, 0, &kind, NULL, &end) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_list_data_context(data, english, 0, &kind, &pair, NULL) == QJS_INTL_INVALID_ARGUMENT);
}

static void test_context_dependency(const QJSIntlListData *data,
                                    const QJSIntlAllocator *allocator)
{
    const QJSIntlSection *sections[] = {&data->data.strings, &data->data.locales,
                                       &data->list_contexts};
    const uint32_t identities[] = {QJS_INTL_SECTION_STRINGS, QJS_INTL_SECTION_LOCALES,
                                   QJS_INTL_SECTION_LIST_CONTEXTS};
    size_t offset = 64 + 3 * 16;
    uint8_t *copy = calloc(1, qjs_intl_native_length);
    QJSIntlData base;
    QJSIntlListData result;
    QJSIntlPack pack;
    unsigned int i;

    assert(copy);
    memcpy(copy, "QJIN", 4);
    put_u32(copy + 4, 1);
    put_u32(copy + 12, 3);
    for (i = 0; i < 3; i++) {
        size_t entry = 64 + i * 16;
        offset = (offset + 3) & ~(size_t)3;
        assert(sections[i]->length <= qjs_intl_native_length - offset);
        put_u32(copy + entry, identities[i]);
        put_u32(copy + entry + 4, offset);
        put_u32(copy + entry + 8, sections[i]->length);
        put_u32(copy + entry + 12, sections[i]->count);
        memcpy(copy + offset, sections[i]->bytes, sections[i]->length);
        offset += sections[i]->length;
    }
    put_u32(copy + 8, offset);
    assert(qjs_intl_pack_open(copy, offset, &pack) == QJS_INTL_OK);
    assert(qjs_intl_data_open(copy, offset, allocator, &base) == QJS_INTL_OK);
    assert(qjs_intl_list_data_open(&base, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!result.data.pack.bytes && !result.list_contexts.bytes);
    free(copy);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData base;
    QJSIntlListData data, result, zero = {0};
    QJSIntlBytes text;
    uint32_t index, parent, english = UINT32_MAX;
    size_t lists_at, vectors_at;
    uint8_t *copy;

    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &base));
    assert(!qjs_intl_list_data_open(&base, &allocator, &data));
    assert(!state.live);
    test_contexts(&data, &allocator);
    test_context_dependency(&data, &allocator);
    for (index = 0; index < base.locales.count; index++) {
        unsigned int variant, part;
        assert(!qjs_intl_data_locale(&base, index, &text, &parent));
        if (text.length == 2 && !memcmp(text.data, "en", 2))
            english = index;
        for (variant = 0; variant < 9; variant++) {
            for (part = 0; part < 4; part++) {
                assert(!qjs_intl_list_data_template(&data, index, variant, part, &text));
                assert(text.length >= 6);
            }
        }
    }
    assert(english != UINT32_MAX);
    assert(!qjs_intl_list_data_template(&data, english, 0, 0, &text));
    assert(text.length == 11 && !memcmp(text.data, "{0} and {1}", 11));
    lists_at = data.lists.bytes - base.pack.bytes;
    vectors_at = lists_at + 4 + (size_t)data.lists.count * 4;
    corrupt_word(&allocator, lists_at + 4, qjs_intl_read_u32(data.lists.bytes));
    corrupt_word(&allocator, vectors_at, base.strings.count);
    corrupt_word(&allocator, vectors_at, 0);
    assert(qjs_intl_list_data_template(&data, english, 9, 0, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(!text.data);
    assert(qjs_intl_list_data_template(&data, english, 0, 4, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_list_data_template(&data, base.locales.count, 0, 0, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_list_data_template(NULL, 0, 0, 0, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_list_data_template(&data, 0, 0, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);
    state.fail = 1;
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_list_data_open(&base, &allocator, &result) == QJS_INTL_NO_MEMORY);
    assert(!memcmp(&result, &zero, sizeof(result)) && !state.live);
    state.fail = 0;
    assert(qjs_intl_list_data_open(NULL, &allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_list_data_open(&base, NULL, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_list_data_open(&base, &allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    copy = malloc(qjs_intl_native_length + 1);
    assert(copy);
    memcpy(copy + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(copy + 1, qjs_intl_native_length, &allocator, &base));
    assert(!qjs_intl_list_data_open(&base, &allocator, &data));
    assert(!qjs_intl_list_data_template(&data, english, 0, 0, &text));
    memset(&base, 0, sizeof(base));
    memset(&data, 0, sizeof(data));
    assert(text.length == 11 && !memcmp(text.data, "{0} and {1}", 11));
    /* Retain base lists without contexts, then omit both. Preserve sorted IDs
       by renaming every following section along with the first omitted one. */
    for (index = 0; index < 2; index++) {
        uint32_t section, first = index ? QJS_INTL_SECTION_LISTS : QJS_INTL_SECTION_LIST_CONTEXTS;
        memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
        for (section = 0; section < qjs_intl_read_u32(copy + 12); section++) {
            size_t at = 64 + (size_t)section * 16;
            uint32_t id = qjs_intl_read_u32(copy + at);
            if (id >= first)
                put_u32(copy + at, id + 100);
        }
        assert(!qjs_intl_data_open(copy, qjs_intl_native_length, &allocator, &base));
        if (index) {
            assert(qjs_intl_list_data_open(&base, &allocator, &data) == QJS_INTL_UNSUPPORTED);
            assert(!memcmp(&data, &zero, sizeof(data)));
        } else {
            unsigned int kind = 99;
            QJSIntlBytes pair, end;
            assert(!qjs_intl_list_data_open(&base, &allocator, &data));
            assert(!qjs_intl_list_data_template(&data, english, 0, 0, &text));
            assert(qjs_intl_list_data_context(&data, english, 0, &kind, &pair, &end) ==
                   QJS_INTL_UNSUPPORTED);
            assert(!kind && !pair.data && !end.data);
        }
    }
    free(copy);
    assert(!state.live);
    return 0;
}
