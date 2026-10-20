/* Canonical era mappings, independent raw IDs, corruption and scratch failure.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/era-data.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef TEST_INTL_ERA_AUTHORED
#define TEST_INTL_ERA_AUTHORED 0
#endif
#if TEST_INTL_ERA_AUTHORED
extern const uint8_t era_empty_bytes[], era_missing_bytes[];
extern const size_t era_empty_length, era_missing_length;
#endif

typedef struct Allocation { size_t calls, live, fail_at; } Allocation;

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
    return (QJSIntlBytes){text, strlen(text)};
}

static void check(const QJSIntlCalendarEraData *data, const char *calendar,
                   const char *code, const char *raw)
{
    QJSIntlBytes key;
    assert(!qjs_intl_calendar_era_find(data, bytes(calendar), bytes(code), &key));
    assert(key.length == strlen(raw) && !memcmp(key.data, raw, key.length));
}

#if TEST_INTL_ERA_AUTHORED
static void write_word(uint8_t *at, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        at[i] = (uint8_t)(value >> (8 * i));
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

static void corrupt(const QJSIntlAllocator *allocator, const QJSIntlCalendarEraData *owner)
{
    uint8_t *copy = malloc(qjs_intl_native_length);
    size_t offset = (size_t)(owner->section.bytes - qjs_intl_native_bytes), i;
    struct Change { size_t word; uint32_t value; } changes[] = {
        {0, 2}, {1, owner->data.strings.count}, {1, string_id(&owner->data, "x_y")},
        {2, string_id(&owner->data, "a")}, {2, string_id(&owner->data, "BAD")},
        {2, string_id(&owner->data, "aa--bb")}, {3, string_id(&owner->data, "01")},
        {3, string_id(&owner->data, "am")},
        {9, string_id(&owner->data, "am")},
        {3, 0}, {8, string_id(&owner->data, "bce")}, {9, string_id(&owner->data, "0")},
    };
    assert(copy);
    for (i = 0; i < sizeof(changes) / sizeof(changes[0]); i++) {
        QJSIntlData data;
        QJSIntlCalendarEraData bad, zero = {0};
        memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
        write_word(copy + offset + changes[i].word * 4, changes[i].value);
        assert(!qjs_intl_data_open(copy, qjs_intl_native_length, allocator, &data));
        memset(&bad, 0xff, sizeof(bad));
        assert(qjs_intl_calendar_era_data_open(&data, allocator, &bad) == QJS_INTL_DATA_ERROR);
        assert(!memcmp(&bad, &zero, sizeof(bad)));
    }
    free(copy);
}
#endif

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlCalendarEraData owner, failed, zero = {0};
    QJSIntlCalendarEraEntry entry;
    QJSIntlBytes key;
    uint32_t i;
    size_t calls;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_calendar_era_data_open(&data, &allocator, &owner) && !state.live);
    calls = state.calls;
    check(&owner, "coptic", "am", "1");
    check(&owner, "hebrew", "am", "0");
    check(&owner, "gregorian", "bce", "0");
    check(&owner, "gregorian", "ce", "1");
    check(&owner, "japanese", "meiji", "232");
    check(&owner, "japanese", "reiwa", "236");
#if TEST_INTL_ERA_AUTHORED
    assert(owner.section.count == 11);
    check(&owner, "japanese", "next-era", "999");
    {
        char calendar[2054], raw[2049];
        memcpy(calendar, "long-", 5);
        memset(calendar + 5, 'a', 2048);
        calendar[2053] = 0;
        memset(raw, '9', 2048);
        raw[2048] = 0;
        check(&owner, calendar, "am", "0");
        check(&owner, calendar, "be", "1");
        check(&owner, "other", "am", raw);
        check(&owner, "third", "am", raw);
    }
#else
    assert(owner.section.count == 27);
    check(&owner, "islamic-civil", "bh", "1");
    check(&owner, "ethiopic-amete-alem", "aa", "0");
#endif
    assert(qjs_intl_calendar_era_find(&owner, bytes("chinese"), bytes("ce"), &key) == QJS_INTL_UNSUPPORTED);
    assert(!key.data && !key.length);
    assert(qjs_intl_calendar_era_find(&owner, bytes("japanese"), bytes("ce"), &key) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_calendar_era_find(&owner, bytes("japanese"), bytes("Reiwa"), &key) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_calendar_era_find(&owner, (QJSIntlBytes){"x", SIZE_MAX}, bytes("ce"), &key) == QJS_INTL_INVALID_ARGUMENT);
    for (i = 0; i < owner.section.count; i++) {
        assert(!qjs_intl_calendar_era_at(&owner, i, &entry));
        assert(!qjs_intl_calendar_era_find(&owner, entry.calendar, entry.code, &key));
        assert(key.length == entry.key.length && !memcmp(key.data, entry.key.data, key.length));
    }
    assert(qjs_intl_calendar_era_at(&owner, i, &entry) == QJS_INTL_INVALID_ARGUMENT);
    assert(state.calls == calls && !state.live);
    state.fail_at = state.calls + 1;
    memset(&failed, 0xff, sizeof(failed));
    assert(qjs_intl_calendar_era_data_open(&data, &allocator, &failed) == QJS_INTL_NO_MEMORY);
    assert(!memcmp(&failed, &zero, sizeof(failed)) && !state.live);
    state.fail_at = 0;
#if TEST_INTL_ERA_AUTHORED
    corrupt(&allocator, &owner);
    assert(!qjs_intl_data_open(era_empty_bytes, era_empty_length, &allocator, &data));
    assert(!qjs_intl_calendar_era_data_open(&data, &allocator, &owner) && !owner.section.count);
    assert(qjs_intl_calendar_era_find(&owner, bytes("gregorian"), bytes("ce"), &key) == QJS_INTL_UNSUPPORTED);
    assert(!qjs_intl_data_open(era_missing_bytes, era_missing_length, &allocator, &data));
    assert(qjs_intl_calendar_era_data_open(&data, &allocator, &owner) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&owner, &zero, sizeof(owner)));
#endif
    assert(!state.live);
    return 0;
}
