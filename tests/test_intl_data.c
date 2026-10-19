/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/native/data.h"
#include "../src/intl/locale/native-data.h"
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
    QJSIntlData data;
    uint8_t *copy = malloc(qjs_intl_native_length);
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    assert(qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data) == QJS_INTL_DATA_ERROR);
    assert(!data.pack.bytes && !data.strings.bytes && !data.locales.bytes);
    free(copy);
}

static void corrupt_metadata(const QJSIntlAllocator *allocator, size_t offset, uint32_t value)
{
    QJSIntlData data;
    QJSIntlLocaleMetadata metadata;
    uint8_t *copy = malloc(qjs_intl_native_length);
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    assert(qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data) == QJS_INTL_OK);
    assert(qjs_intl_locale_metadata_open(&data, allocator, &metadata) == QJS_INTL_DATA_ERROR);
    assert(!metadata.data.pack.bytes && !metadata.aliases.bytes);
    free(copy);
}

static void test_metadata(const QJSIntlData *data, const QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    QJSIntlLocaleMetadata metadata;
    size_t aliases, likely, keys, types;

    state->fail = 1;
    assert(qjs_intl_locale_metadata_open(data, allocator, &metadata) == QJS_INTL_NO_MEMORY);
    assert(!metadata.data.pack.bytes && !state->live);
    state->fail = 0;
    assert(qjs_intl_locale_metadata_open(data, allocator, &metadata) == QJS_INTL_OK);
    assert(metadata.aliases.count >= 1200 && metadata.likely.count >= 7000);
    assert(metadata.keys.count >= 30 && metadata.types.count >= 1000 && !state->live);
    aliases = metadata.aliases.bytes - data->pack.bytes;
    likely = metadata.likely.bytes - data->pack.bytes;
    keys = metadata.keys.bytes - data->pack.bytes;
    types = metadata.types.bytes - data->pack.bytes;
    corrupt_metadata(allocator, aliases, 5);
    corrupt_metadata(allocator, aliases + 4, data->strings.count);
    corrupt_metadata(allocator, aliases + 12, 0);
    corrupt_metadata(allocator, likely, 0);
    corrupt_metadata(allocator, keys + 8, 4);
    corrupt_metadata(allocator, types + 20, 4);
    corrupt_metadata(allocator, types + 16, qjs_intl_read_u32(metadata.types.bytes + 8));
    assert(qjs_intl_locale_metadata_open(NULL, allocator, &metadata) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_locale_metadata_open(data, NULL, &metadata) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_locale_metadata_open(data, allocator, NULL) == QJS_INTL_INVALID_ARGUMENT);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlBytes text;
    uint32_t index, parent, english = UINT32_MAX;
    size_t strings_at, locales_at;
    uint8_t *copy;

    assert(qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length,
                              &allocator, &data) == QJS_INTL_OK);
    assert(!state.live);
    test_metadata(&data, &allocator);
    assert(data.pack.profile == TEST_INTL_PROFILE);
    assert(TEST_INTL_PROFILE ? data.locales.count > 1000 : data.locales.count == 3);
    for (index = 0; index < data.strings.count; index++)
        assert(qjs_intl_data_string(&data, index, &text) == QJS_INTL_OK);
    assert(qjs_intl_data_string(&data, data.strings.count, &text) == QJS_INTL_INVALID_ARGUMENT);
    assert(!text.data && !text.length);
    for (index = 0; index < data.locales.count; index++) {
        assert(qjs_intl_data_locale(&data, index, &text, &parent) == QJS_INTL_OK);
        assert(text.length && (parent == UINT32_MAX || parent < data.locales.count));
        if (text.length == 2 && !memcmp(text.data, "en", 2))
            english = index;
    }
    assert(english != UINT32_MAX);
    strings_at = data.strings.bytes - data.pack.bytes;
    locales_at = data.locales.bytes - data.pack.bytes;
    corrupt_word(&allocator, strings_at, 1);
    corrupt_word(&allocator, strings_at + 8, 0);
    corrupt_word(&allocator, locales_at, 0);
    corrupt_word(&allocator, locales_at + 4, 0);
    corrupt_word(&allocator, locales_at + 4, data.locales.count);
    assert(qjs_intl_data_locale(&data, data.locales.count, &text, &parent) == QJS_INTL_INVALID_ARGUMENT);
    copy = malloc(qjs_intl_native_length + 1);
    assert(copy);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    copy[strings_at + ((size_t)data.strings.count + 1) * 4] = 0xc0;
    assert(qjs_intl_data_open(copy, qjs_intl_native_length, &allocator, &data) == QJS_INTL_DATA_ERROR);
    assert(!data.pack.bytes && !state.live);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    /* Rename this and later identities together to preserve directory order
       without colliding with an additional prepared section. */
    for (index = 0; index < qjs_intl_read_u32(copy + 12); index++) {
        size_t entry = 64 + (size_t)index * 16;
        uint32_t identity = qjs_intl_read_u32(copy + entry);
        if (identity >= QJS_INTL_SECTION_LISTS)
            put_u32(copy + entry, identity + 100);
    }
    assert(qjs_intl_data_open(copy, qjs_intl_native_length, &allocator, &data) == QJS_INTL_OK);
    memcpy(copy + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(qjs_intl_data_open(copy + 1, qjs_intl_native_length, &allocator, &data) == QJS_INTL_OK);
    state.fail = 1;
    assert(qjs_intl_data_open(copy + 1, qjs_intl_native_length, &allocator, &data) == QJS_INTL_NO_MEMORY);
    assert(!data.pack.bytes && !state.live);
    state.fail = 0;
    assert(qjs_intl_data_open(NULL, 0, &allocator, &data) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_data_open(copy, qjs_intl_native_length, NULL, &data) == QJS_INTL_INVALID_ARGUMENT);
    free(copy);
    assert(!state.live);
    puts("Intl prepared data reader tests passed");
    return 0;
}
