/* Native numbering owner: real profiles, authored corruption and OOM.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/locale/numbering-data.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t calls, fail_at, live;
} Allocation;

static void *allocate(void *opaque, size_t length)
{
    Allocation *state = opaque;
    void *ptr;
    assert(length);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(length);
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
static void equal(QJSIntlBytes value, const char *expected)
{
    assert(value.length == strlen(expected) &&
           !memcmp(value.data, expected, value.length));
}
static void put_u32(uint8_t *ptr, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        ptr[i] = value >> (i * 8);
}
static uint32_t string_id(const QJSIntlData *data, const char *value)
{
    uint32_t i;
    for (i = 0; i < data->strings.count; i++) {
        QJSIntlBytes text;
        assert(!qjs_intl_data_string(data, i, &text));
        if (text.length == strlen(value) && !memcmp(text.data, value, text.length))
            return i;
    }
    abort();
}
static void corrupt(const QJSIntlAllocator *allocator, size_t offset, uint32_t value)
{
    uint8_t *copy = malloc(qjs_intl_native_length);
    QJSIntlData data;
    QJSIntlNumberingData owner;
    assert(copy && offset <= qjs_intl_native_length - 4);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + offset, value);
    assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
    memset(&owner, 0xff, sizeof(owner));
    assert(qjs_intl_numbering_data_open(&data, allocator, &owner) ==
           QJS_INTL_DATA_ERROR);
    assert(!owner.data.pack.bytes && !owner.systems.bytes && !owner.preferences.bytes);
    free(copy);
}

static void truncate_last_table(const QJSIntlAllocator *allocator, size_t directory)
{
    size_t length = qjs_intl_native_length - 4;
    uint8_t *copy = malloc(length);
    QJSIntlData data;
    QJSIntlNumberingData owner;
    assert(copy);
    memcpy(copy, qjs_intl_native_bytes, length);
    put_u32(copy + 8, (uint32_t)length);
    put_u32(copy + directory + 8, qjs_intl_read_u32(copy + directory + 8) - 4);
    assert(!qjs_intl_data_open(copy, length, allocator, &data));
    assert(qjs_intl_numbering_data_open(&data, allocator, &owner) ==
           QJS_INTL_DATA_ERROR);
    assert(!owner.data.pack.bytes && !owner.systems.bytes && !owner.preferences.bytes);
    free(copy);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlNumberingData owner, retry;
    QJSIntlNumberingSystem system;
    QJSIntlNumberingPreferences preferences;
    size_t calls, step, systems_at, preferences_at;
    uint32_t index, latin, numeric = 0, algorithmic = 0;
    uint8_t *unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(
        !qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    state.calls = 0;
    assert(!qjs_intl_numbering_data_open(&data, &allocator, &owner));
    calls = state.calls;
    assert(!state.live);
    memset(&retry, 0xff, sizeof(retry));
    assert(qjs_intl_numbering_data_open(NULL, &allocator, &retry) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!retry.data.pack.bytes && !retry.systems.bytes && !retry.preferences.bytes);
    assert(qjs_intl_numbering_data_open(&data, NULL, &retry) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_numbering_data_open(&data, &allocator, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
    for (index = 0; index < owner.systems.count; index++) {
        uint32_t found;
        assert(!qjs_intl_numbering_system_at(&owner, index, &system));
        assert(!qjs_intl_numbering_system_find(&owner, system.name, &found) &&
               found == index);
        numeric += system.kind == QJS_INTL_NUMBERING_NUMERIC;
        algorithmic += system.kind == QJS_INTL_NUMBERING_ALGORITHMIC;
    }
    assert(numeric == (TEST_INTL_AUTHORED ? 2 : 78));
    assert(algorithmic == (TEST_INTL_AUTHORED ? 1 : 19));
    assert(!qjs_intl_numbering_system_find(&owner, bytes("latn"), &latin));
    assert(!qjs_intl_numbering_system_at(&owner, latin, &system));
    equal(system.value, "0123456789");
    assert(!qjs_intl_numbering_system_find(&owner, bytes("mathbold"), &index));
    assert(!qjs_intl_numbering_system_at(&owner, index, &system));
    assert(system.kind == QJS_INTL_NUMBERING_NUMERIC && system.value.length == 40);
    assert(!qjs_intl_numbering_preferences(&owner, bytes("root"), &preferences));
    assert(preferences.standard == latin && preferences.native == latin);
    if (TEST_INTL_AUTHORED) {
        char key[512];
        unsigned int i;
        memcpy(key, "en", 2);
        for (i = 0; i < 60; i++)
            snprintf(key + 2 + i * 6, sizeof(key) - (2 + i * 6), "-%05u", 1000 + i);
        assert(strlen(key) > 255);
        assert(!qjs_intl_numbering_preferences(&owner, bytes(key), &preferences));
        assert(preferences.standard == latin);
        assert(!qjs_intl_numbering_preferences(&owner, bytes("root"), &preferences));
        assert(calls && preferences.traditional == QJS_INTL_NUMBERING_ABSENT);
        assert(!qjs_intl_numbering_system_at(&owner, preferences.finance, &system));
        assert(system.kind == QJS_INTL_NUMBERING_ALGORITHMIC);
        equal(system.value, "roman-upper");
        assert(!qjs_intl_numbering_preferences(&owner, bytes("en-1901-1996"),
                                               &preferences));
        assert(preferences.traditional == QJS_INTL_NUMBERING_BLOCKED);
        assert(preferences.finance == QJS_INTL_NUMBERING_ABSENT);
    } else {
        assert(owner.preferences.count == 1212);
        assert(!qjs_intl_numbering_preferences(&owner, bytes("ar"), &preferences));
        assert(preferences.standard == latin);
        assert(!qjs_intl_numbering_system_at(&owner, preferences.native, &system));
        equal(system.name, "arab");
    }
    assert(qjs_intl_numbering_preferences(&owner, bytes("zz"), &preferences) ==
           QJS_INTL_UNSUPPORTED);
    assert(!preferences.standard && !preferences.native && !preferences.traditional &&
           !preferences.finance);
    assert(qjs_intl_numbering_system_find(&owner, bytes("LATN"), &index) ==
               QJS_INTL_UNSUPPORTED &&
           !index);
    assert(qjs_intl_numbering_system_at(&owner, owner.systems.count, &system) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!system.name.data && !system.value.data && !system.kind);
    assert(qjs_intl_numbering_system_find(NULL, bytes("latn"), &index) ==
               QJS_INTL_INVALID_ARGUMENT &&
           !index);
    assert(qjs_intl_numbering_preferences(&owner, (QJSIntlBytes){NULL, 0},
                                          &preferences) == QJS_INTL_INVALID_ARGUMENT);
    for (step = 1; step <= calls; step++) {
        state.calls = 0;
        state.fail_at = step;
        assert(qjs_intl_numbering_data_open(&data, &allocator, &retry) ==
               QJS_INTL_NO_MEMORY);
        assert(!state.live && !retry.data.pack.bytes && !retry.systems.bytes);
        state.fail_at = 0;
        assert(!qjs_intl_numbering_data_open(&data, &allocator, &retry));
        assert(!state.live);
    }
    systems_at = owner.systems.bytes - data.pack.bytes;
    preferences_at = owner.preferences.bytes - data.pack.bytes;
    corrupt(&allocator, systems_at, UINT32_MAX);
    corrupt(&allocator, systems_at + 4, 2);
    corrupt(&allocator, systems_at + 8, 0);
    corrupt(&allocator, systems_at + 12,
            qjs_intl_read_u32(qjs_intl_native_bytes + systems_at));
    corrupt(&allocator, preferences_at, UINT32_MAX);
    corrupt(&allocator, preferences_at + 4, QJS_INTL_NUMBERING_ABSENT);
    corrupt(&allocator, preferences_at + 8, QJS_INTL_NUMBERING_BLOCKED);
    corrupt(&allocator, preferences_at + 12, owner.systems.count);
    corrupt(&allocator, preferences_at + 16, QJS_INTL_NUMBERING_BLOCKED - 1);
    if (TEST_INTL_AUTHORED) {
        assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length,
                                   &allocator, &data));
        corrupt(&allocator, systems_at, string_id(&data, "foo"));
        corrupt(&allocator, systems_at + 8, string_id(&data, "0123456788"));
        corrupt(&allocator, systems_at + 12 + 8, string_id(&data, "0123456788"));
        corrupt(&allocator, systems_at + 2 * 12 + 8, string_id(&data, "bad rule"));
        corrupt(&allocator, preferences_at, string_id(&data, "en-1901-1901"));
        corrupt(&allocator, preferences_at, string_id(&data, "En"));
        corrupt(&allocator, preferences_at, string_id(&data, "en-us"));
        corrupt(&allocator, preferences_at, string_id(&data, "bad_tag"));
        corrupt(&allocator, preferences_at + (owner.preferences.count - 1) * 20,
                string_id(&data, "foo"));
        {
            QJSIntlBytes key;
            uint32_t name = qjs_intl_read_u32(qjs_intl_native_bytes + preferences_at);
            assert(!qjs_intl_data_string(&data, name, &key));
            assert(key.length > 255);
            /* Keep the pool order/size intact, duplicate only the last variant. */
            corrupt(&allocator,
                    (const uint8_t *)key.data - data.pack.bytes + key.length - 4,
                    qjs_intl_read_u32((const uint8_t *)key.data + 4));
        }
    }
    for (index = 0; index < data.pack.section_count; index++) {
        size_t directory = 64 + (size_t)index * 16;
        uint32_t id = qjs_intl_read_u32(qjs_intl_native_bytes + directory);
        if (id == QJS_INTL_SECTION_NUMBERING_SYSTEMS ||
            id == QJS_INTL_SECTION_NUMBERING_PREFERENCES) {
            uint32_t count = qjs_intl_read_u32(qjs_intl_native_bytes + directory + 12);
            corrupt(&allocator, directory + 12, 0);
            corrupt(&allocator, directory + 12, count + 1);
            if (TEST_INTL_AUTHORED && id == QJS_INTL_SECTION_NUMBERING_PREFERENCES) {
                corrupt(&allocator, directory, id + 1);
                truncate_last_table(&allocator, directory);
            }
        }
    }
    free(unaligned);
    assert(!state.live);
    return 0;
}
