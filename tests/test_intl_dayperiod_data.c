/* Explicit origins, canonical physical lists and validation allocation failure.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/datetime/dayperiod-data.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t dayperiod_empty_bytes[];
extern const size_t dayperiod_empty_length;

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

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static int equal(QJSIntlBytes a, const char *b)
{
    return a.length == strlen(b) && (!a.length || !memcmp(a.data, b, a.length));
}

static uint32_t string_id(const QJSIntlData *data, const char *value)
{
    uint32_t i;
    QJSIntlBytes text;
    for (i = 0; i < data->strings.count; i++) {
        assert(!qjs_intl_data_string(data, i, &text));
        if (equal(text, value))
            return i;
    }
    abort();
}

static uint32_t map_for(const QJSIntlDayPeriodData *data, QJSIntlDayPeriodRuleSetKind kind, const char *locale)
{
    uint32_t map;
    assert(!qjs_intl_dayperiod_find(data, kind, bytes(locale), &map));
    return map;
}

static const uint8_t *row_for(const QJSIntlDayPeriodData *data, QJSIntlDayPeriodRuleSetKind kind, const char *locale)
{
    uint32_t i, map;
    QJSIntlDayPeriodRuleSetKind current;
    QJSIntlBytes origin;
    for (i = 0; i < data->row_count; i++) {
        assert(!qjs_intl_dayperiod_row(data, i, &current, &origin, &map));
        if (kind == current && equal(origin, locale))
            return data->rows + (size_t)i * 12;
    }
    abort();
}

static const uint8_t *rule_for(const QJSIntlDayPeriodData *data, uint32_t map, QJSIntlDayPeriodCode code)
{
    uint32_t start, count, i;
    QJSIntlDayPeriodRule rule;
    assert(!qjs_intl_dayperiod_map(data, map, &start, &count));
    for (i = 0; i < count; i++) {
        assert(!qjs_intl_dayperiod_rule(data, start + i, &rule));
        if (rule.code == code)
            return data->rules + ((size_t)start + i) * 16;
    }
    abort();
}

static void check_queries(const QJSIntlDayPeriodData *data)
{
    uint32_t i, map, found, start, count, english;
    QJSIntlDayPeriodRuleSetKind kind;
    QJSIntlDayPeriodRule rule, zero = {0};
    QJSIntlBytes origin, name;
    assert(data->row_count == 13 && data->map_count == 7 && data->rule_count == 15);
    for (i = 0; i < QJS_INTL_DAYPERIOD_CODE_COUNT; i++) {
        name = qjs_intl_dayperiod_code_name(i);
        assert(name.data && qjs_intl_dayperiod_code_find(name) == (int)i);
    }
    assert(equal(qjs_intl_dayperiod_code_name(QJS_INTL_DAYPERIOD_AFTERNOON2), "afternoon2"));
    assert(equal(qjs_intl_dayperiod_code_name(QJS_INTL_DAYPERIOD_NIGHT2), "night2"));
    assert(!qjs_intl_dayperiod_code_name(UINT32_MAX).data);
    assert(qjs_intl_dayperiod_code_find(bytes("unknown")) == -1);
    assert(qjs_intl_dayperiod_code_find((QJSIntlBytes){NULL, 1}) == -1);
    for (i = 0; i < data->row_count; i++) {
        assert(!qjs_intl_dayperiod_row(data, i, &kind, &origin, &map));
        assert(!qjs_intl_dayperiod_find(data, kind, origin, &found) && found == map);
    }
    for (i = 0; i < data->map_count; i++) {
        uint32_t j;
        assert(!qjs_intl_dayperiod_map(data, i, &start, &count));
        assert(count <= QJS_INTL_DAYPERIOD_CODE_COUNT && count <= data->rule_count - start);
        for (j = 0; j < count; j++)
            assert(!qjs_intl_dayperiod_rule(data, start + j, &rule));
    }
    english = map_for(data, QJS_INTL_DAYPERIOD_FORMAT, "en");
    assert(english == map_for(data, QJS_INTL_DAYPERIOD_FORMAT, "hi-Latn"));
    assert(english == map_for(data, QJS_INTL_DAYPERIOD_FORMAT, "en-1901-1996"));
    assert(!qjs_intl_dayperiod_map(data, english, &start, &count) && count == 5);
    for (i = 0; i < count; i++) {
        assert(!qjs_intl_dayperiod_rule(data, start + i, &rule));
        if (rule.code == QJS_INTL_DAYPERIOD_MORNING1)
            assert(rule.form == QJS_INTL_DAYPERIOD_RANGE && rule.start == 21600 && rule.end == 44100);
        if (rule.code == QJS_INTL_DAYPERIOD_NOON)
            assert(rule.form == QJS_INTL_DAYPERIOD_AT && rule.start == 43200 && rule.end == 43200);
    }
    map = map_for(data, QJS_INTL_DAYPERIOD_FORMAT, "fr");
    assert(map == map_for(data, QJS_INTL_DAYPERIOD_SELECTION, "fr"));
    assert(!qjs_intl_dayperiod_map(data, map, &start, &count) && !count);
    map = map_for(data, QJS_INTL_DAYPERIOD_SELECTION, "gsw");
    assert(map == map_for(data, QJS_INTL_DAYPERIOD_FORMAT, "gsw"));
    assert(!qjs_intl_dayperiod_map(data, map, &start, &count) && count == 1);
    assert(!qjs_intl_dayperiod_rule(data, start, &rule) && rule.code == QJS_INTL_DAYPERIOD_MIDNIGHT);
    /* An exact lookup neither truncates the source identity nor merges a
       translated-name parent chain into the clock-rule origin table. */
    assert(qjs_intl_dayperiod_find(data, QJS_INTL_DAYPERIOD_FORMAT, bytes("sr-Latn"), &map) == QJS_INTL_UNSUPPORTED && !map);
    assert(qjs_intl_dayperiod_find(data, QJS_INTL_DAYPERIOD_SELECTION, bytes("root"), &map) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_dayperiod_find(data, QJS_INTL_DAYPERIOD_KIND_COUNT, bytes("en"), &map) == QJS_INTL_INVALID_ARGUMENT && !map);
    assert(qjs_intl_dayperiod_find(data, QJS_INTL_DAYPERIOD_FORMAT, (QJSIntlBytes){NULL, 1}, &map) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_dayperiod_row(data, data->row_count, &kind, &origin, &map) == QJS_INTL_INVALID_ARGUMENT);
    assert(kind == QJS_INTL_DAYPERIOD_FORMAT && !origin.data && !map);
    assert(qjs_intl_dayperiod_map(data, data->map_count, &start, &count) == QJS_INTL_INVALID_ARGUMENT && !start && !count);
    memset(&rule, 0xff, sizeof(rule));
    assert(qjs_intl_dayperiod_rule(data, data->rule_count, &rule) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&rule, &zero, sizeof(rule)));
}

static void put_u32(uint8_t *at, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        at[i] = value >> (i * 8);
}

static void reject_copy(const uint8_t *copy, size_t length, const QJSIntlAllocator *allocator)
{
    QJSIntlData data;
    QJSIntlDayPeriodData result, zero = {0};
    assert(!qjs_intl_data_open(copy, length, allocator, &data));
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_dayperiod_data_open(&data, allocator, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)));
}

static void corrupt(const QJSIntlDayPeriodData *owner, const QJSIntlAllocator *allocator,
                    const uint8_t *at, uint32_t value)
{
    uint8_t *copy = malloc(qjs_intl_native_length);
    assert(copy);
    memcpy(copy, qjs_intl_native_bytes, qjs_intl_native_length);
    put_u32(copy + (at - owner->data.pack.bytes), value);
    reject_copy(copy, qjs_intl_native_length, allocator);
    free(copy);
}

static void duplicate_empty_map(const QJSIntlDayPeriodData *owner, const QJSIntlAllocator *allocator)
{
    /* Insert a second reachable empty list, keeping envelope and all semantic
       row facts valid so rejection specifically requires canonical interning. */
    size_t offset = (size_t)(owner->maps - owner->data.pack.bytes) + 8, i;
    uint8_t *copy = malloc(qjs_intl_native_length + 8);
    assert(copy && !qjs_intl_read_u32(owner->maps) && !qjs_intl_read_u32(owner->maps + 4));
    assert(owner->section.bytes + owner->section.length == owner->data.pack.bytes + owner->data.pack.length);
    memcpy(copy, owner->data.pack.bytes, offset);
    memset(copy + offset, 0, 8);
    memcpy(copy + offset + 8, owner->data.pack.bytes + offset, qjs_intl_native_length - offset);
    put_u32(copy + 8, (uint32_t)qjs_intl_native_length + 8);
    for (i = 0; i < owner->data.pack.section_count; i++) {
        uint8_t *directory = copy + 64 + i * 16;
        if (qjs_intl_read_u32(directory) == QJS_INTL_SECTION_DAYPERIOD_RULES)
            put_u32(directory + 8, (uint32_t)owner->section.length + 8);
    }
    put_u32(copy + (owner->section.bytes - owner->data.pack.bytes) + 4, owner->map_count + 1);
    for (i = 0; i < owner->row_count; i++) {
        const uint8_t *row = owner->rows + i * 12;
        uint32_t map = qjs_intl_read_u32(row + 8);
        if (map || qjs_intl_read_u32(row) == QJS_INTL_DAYPERIOD_SELECTION)
            put_u32(copy + (row - owner->data.pack.bytes) + 8, map + 1);
    }
    reject_copy(copy, qjs_intl_native_length + 8, allocator);
    free(copy);
}

static void check_corruption(const QJSIntlDayPeriodData *owner, const QJSIntlAllocator *allocator)
{
    size_t i;
    uint32_t english = map_for(owner, QJS_INTL_DAYPERIOD_FORMAT, "en");
    uint32_t long_map = map_for(owner, QJS_INTL_DAYPERIOD_SELECTION, "zz");
    const uint8_t *morning = rule_for(owner, english, QJS_INTL_DAYPERIOD_MORNING1);
    const uint8_t *midnight = rule_for(owner, english, QJS_INTL_DAYPERIOD_MIDNIGHT);
    const uint8_t *de = row_for(owner, QJS_INTL_DAYPERIOD_FORMAT, "de");
    const uint8_t *en = row_for(owner, QJS_INTL_DAYPERIOD_FORMAT, "en");
    for (i = 0; i < 3; i++)
        corrupt(owner, allocator, owner->section.bytes + i * 4, i ? UINT32_MAX : 0);
    corrupt(owner, allocator, de, QJS_INTL_DAYPERIOD_KIND_COUNT);
    corrupt(owner, allocator, de + 4, owner->data.strings.count);
    corrupt(owner, allocator, de + 4, string_id(&owner->data, "bad_tag"));
    corrupt(owner, allocator, de + 4, string_id(&owner->data, "en-us"));
    corrupt(owner, allocator, de + 4, string_id(&owner->data, "en-1901-1901"));
    corrupt(owner, allocator, de + 8, owner->map_count);
    /* Duplicate kind/locale with a different map must not be admitted. */
    corrupt(owner, allocator, en + 4, qjs_intl_read_u32(de + 4));
    corrupt(owner, allocator, de + 8, long_map);
    corrupt(owner, allocator, row_for(owner, QJS_INTL_DAYPERIOD_FORMAT, "xx") + 8,
            map_for(owner, QJS_INTL_DAYPERIOD_FORMAT, "root")); /* Unused distinct map. */
    corrupt(owner, allocator, owner->maps, 1);
    corrupt(owner, allocator, owner->maps + 4, 13);
    corrupt(owner, allocator, owner->maps + 8, 1);
    corrupt(owner, allocator, morning, QJS_INTL_DAYPERIOD_CODE_COUNT);
    corrupt(owner, allocator, morning + 4, 2);
    corrupt(owner, allocator, morning + 8, 1);
    corrupt(owner, allocator, morning + 8, 86400);
    corrupt(owner, allocator, morning + 12, 86401);
    corrupt(owner, allocator, morning + 12, 44040); /* Partition gap. */
    corrupt(owner, allocator, morning + 12, 44160); /* Partition overlap. */
    corrupt(owner, allocator, morning + 12, 65700); /* Thirteen touched hour buckets. */
    corrupt(owner, allocator, morning, QJS_INTL_DAYPERIOD_NOON); /* Repeated code. */
    corrupt(owner, allocator, midnight + 8, 60);
    corrupt(owner, allocator, midnight + 4, QJS_INTL_DAYPERIOD_RANGE);
    duplicate_empty_map(owner, allocator);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlDayPeriodData owner, result, zero = {0};
    size_t calls, successful, i;
    uint8_t *unaligned;
    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    state.calls = 0;
    assert(!qjs_intl_dayperiod_data_open(&data, &allocator, &owner) && !state.live);
    successful = state.calls;
    assert(successful >= 2); /* Map flags and multi-variant identity scratch. */
    calls = state.calls;
    check_queries(&owner);
    assert(state.calls == calls && !state.live);
    for (i = 1; i <= successful; i++) {
        state.calls = 0;
        state.fail_at = i;
        memset(&result, 0xff, sizeof(result));
        assert(qjs_intl_dayperiod_data_open(&data, &allocator, &result) == QJS_INTL_NO_MEMORY);
        assert(!memcmp(&result, &zero, sizeof(result)) && !state.live);
        state.calls = state.fail_at = 0;
        assert(!qjs_intl_dayperiod_data_open(&data, &allocator, &result) && !state.live);
    }
    check_corruption(&owner, &allocator);
    assert(!state.live);
    unaligned = malloc(qjs_intl_native_length + 1);
    assert(unaligned);
    memcpy(unaligned + 1, qjs_intl_native_bytes, qjs_intl_native_length);
    assert(!qjs_intl_data_open(unaligned + 1, qjs_intl_native_length, &allocator, &data));
    assert(!qjs_intl_dayperiod_data_open(&data, &allocator, &result));
    check_queries(&result);
    free(unaligned);
    assert(!qjs_intl_data_open(dayperiod_empty_bytes, dayperiod_empty_length, &allocator, &data));
    state.calls = 0;
    state.fail_at = 1;
    assert(!qjs_intl_dayperiod_data_open(&data, &allocator, &result));
    assert(!state.calls && !state.live && !result.row_count && !result.map_count && !result.rule_count);
    state.fail_at = 0;
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_dayperiod_data_open(NULL, &allocator, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)));
    assert(qjs_intl_dayperiod_data_open(&data, NULL, &result) == QJS_INTL_INVALID_ARGUMENT);
    return 0;
}
