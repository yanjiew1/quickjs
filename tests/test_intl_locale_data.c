/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/locale/native-data.h"
#include "../src/intl/locale/native-id.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t qjs_intl_locale_fixture[];
extern const size_t qjs_intl_locale_fixture_size;

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

static void *reallocate(void *opaque, void *old, size_t size)
{
    Allocation *state = opaque;
    int had_old = old != NULL;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = realloc(old, size);
    if (ptr && !had_old)
        state->live++;
    return ptr;
}

static void put_u32(uint8_t *bytes, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        bytes[i] = value >> (i * 8);
}

static uint32_t string_id(const QJSIntlData *data, const char *text)
{
    uint32_t index;
    for (index = 0; index < data->strings.count; index++) {
        QJSIntlBytes value;
        assert(qjs_intl_data_string(data, index, &value) == QJS_INTL_OK);
        if (value.length == strlen(text) &&
            !memcmp(value.data, text, value.length))
            return index;
    }
    abort();
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, reallocate, release};
    QJSIntlData data;
    QJSIntlLocaleMetadata metadata, changed;
    size_t calls, i;
    uint8_t *copy;
    static const struct {
        uint32_t section;
        unsigned int word;
        const char *value;
    } invalid[] = {
        {QJS_INTL_SECTION_LIKELY, 0, "EN"},
        {QJS_INTL_SECTION_LIKELY, 0, "en-u-ca-gregory"},
        {QJS_INTL_SECTION_LIKELY, 0, "en-x-foo"},
        {QJS_INTL_SECTION_LIKELY, 0, "en-latn"},
        {QJS_INTL_SECTION_LIKELY, 0, "sl-rozaj-rozaj"},
        {QJS_INTL_SECTION_LIKELY, 0, "en-"},
        {QJS_INTL_SECTION_LIKELY, 0, "en--US"},
        {QJS_INTL_SECTION_ALIASES, 2, "ru ru"},
        {QJS_INTL_SECTION_KEYS, 3, "legacy legacy"},
        {QJS_INTL_SECTION_TYPES, 3, "legacy legacy"},
        {QJS_INTL_SECTION_KEYS, 4, "ca"},
        {QJS_INTL_SECTION_TYPES, 4, "foo"},
    };

    assert(qjs_intl_data_open(qjs_intl_locale_fixture,
                              qjs_intl_locale_fixture_size, &allocator,
                              &data) == QJS_INTL_OK);
    state.calls = 0;
    assert(qjs_intl_locale_metadata_open(&data, &allocator, &metadata) ==
           QJS_INTL_OK);
    calls = state.calls;
    assert(calls >= 4 && !state.live);
    for (i = 1; i <= calls; i++) {
        state.calls = 0;
        state.fail_at = i;
        assert(qjs_intl_locale_metadata_open(&data, &allocator, &changed) ==
               QJS_INTL_NO_MEMORY);
        assert(!changed.data.pack.bytes && !state.live);
    }
    state.fail_at = 0;
    copy = malloc(qjs_intl_locale_fixture_size);
    assert(copy);
    for (i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
        QJSIntlSection section;
        QJSIntlData view;
        size_t offset;
        assert(qjs_intl_pack_section(&data.pack, invalid[i].section,
                                     &section) == QJS_INTL_OK);
        offset = section.bytes - data.pack.bytes + invalid[i].word * 4;
        memcpy(copy, qjs_intl_locale_fixture, qjs_intl_locale_fixture_size);
        put_u32(copy + offset, string_id(&data, invalid[i].value));
        assert(qjs_intl_data_open(copy, qjs_intl_locale_fixture_size,
                                  &allocator, &view) == QJS_INTL_OK);
        assert(qjs_intl_locale_metadata_open(&view, &allocator, &changed) ==
               QJS_INTL_DATA_ERROR);
        assert(!changed.data.pack.bytes && !state.live);
    }
    for (i = 0; i < 2; i++) {
        QJSIntlData view;
        QJSIntlBytes input = {i ? "en-SU" : "en-Latn", i ? 5 : 7};
        char *output = NULL;
        size_t offset = metadata.aliases.bytes - data.pack.bytes;
        memcpy(copy, qjs_intl_locale_fixture, qjs_intl_locale_fixture_size);
        put_u32(copy + offset, i ? 2 : 1);
        put_u32(copy + offset + 4, string_id(&data, i ? "su" : "latn"));
        put_u32(copy + offset + 8, string_id(&data, "abcdefghijklmnop"));
        put_u32(copy + offset + 12, 1);
        assert(qjs_intl_data_open(copy, qjs_intl_locale_fixture_size,
                                  &allocator, &view) == QJS_INTL_OK);
        assert(qjs_intl_locale_metadata_open(&view, &allocator, &changed) ==
               QJS_INTL_OK);
        assert(qjs_intl_native_locale_canonicalize(&allocator, &changed, input,
                                                   &output) ==
               QJS_INTL_DATA_ERROR);
        assert(!output && !state.live);
    }
    free(copy);
    puts("Intl locale metadata schema and allocation tests passed");
    return 0;
}
